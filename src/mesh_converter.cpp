#include "mesh_converter.h"

/* assimp include files. These three are usually needed. */
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

#include "mesh.h"
#include <iostream>
#include <ctime>
#include <unordered_map>
#include <assert.h>
#include <algorithm>

#include "animation.h"
#include "utils.h"

namespace rh {
    const char* build_version = "v0.9.0";
    const char vMajor = 0;
    const char vMinor = 9;
    const char vRevision = 0;

    const size_t MAX_BONES_PER_VERTEX = 4;
    
    const char* Version() {
        return build_version;
    }

    MeshConverter::MeshConverter() : m_valid(false) {}
    MeshConverter::~MeshConverter() {}

    void MeshConverter::LoadInputFile(const char* filename) {
        printf("Loading file: '%s'\n", filename);

        Assimp::Importer imp;
        imp.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
        imp.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);

        const aiScene* scene = imp.ReadFile(filename, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_PopulateArmatureData | aiProcess_GlobalScale);
        if (!scene /* || !scene->HasMeshes()*/)  // note: scene can legit contain no meshes (e.g. it could contain an armature, an animation, and no skin (mesh)))
        {
            printf("Failed to load file: '%s'\n", filename);
            return;
        }

        // first extract the skeleton (if there is one)
        m_mesh.has_skeleton = extract_skeleton(scene, m_mesh.bind_pose);

        printf("num_bones = %d\n", m_mesh.bind_pose.num_bones);
        printf("size()    = %d\n", m_mesh.bind_pose.bones.size());
        for (u32 n = 0; n < m_mesh.bind_pose.num_bones; n++) {
            //std::cout << n << ": " << m_mesh.bind_pose.bones[n].inv_model_matrix << std::endl;
            printf("%2d: ", n);
            laml::print(m_mesh.bind_pose.bones[n].local_matrix, "%.2f");
            printf("\n");
        }
        
        if (m_mesh.has_skeleton) {
            // load animations from assimp scene
            std::vector<std::string> animation_names;
            animation_names.reserve(scene->mNumAnimations);
            for (u32 n = 0; n < scene->mNumAnimations; n++) {
                const aiAnimation* s_anim = scene->mAnimations[n];

                animation_names.emplace_back(s_anim->mName.C_Str());
                printf("Found animation: '%s'\n", s_anim->mName.C_Str());

                Animation anim;
                extract_animation(s_anim, anim, m_mesh.bind_pose);
                m_anims.push_back(anim);
            }
        }

        // populate mesh vertices, indices (bones weights if needed) ans submeshes
        if (!extract_meshes(scene, m_mesh)) {
            printf("Failed to extract meshes!\n");
            return;
        }

        m_valid = true;
    }

    void MeshConverter::ProcessFile() {
        printf("-------------------------------\n");
        for (u32 n = 0; n < m_anims.size(); n++) {
            const auto& anim = m_anims[n];

            printf("Animation: %s\n", anim.name.c_str());
            printf("  %d nodes\n", anim.num_nodes);
            printf("  %.3f frames\n", anim.duration);
            printf("  %.3f frame_rate\n", anim.frame_rate);
            printf("  %.3f sec\n", anim.duration / anim.frame_rate);
        }
    }

    void MeshConverter::SaveOutputFile(const char* filename) {
        if (!m_valid) return;
        
        // Open and check for valid file
        FILE* fid = nullptr;
        errno_t err = fopen_s(&fid, filename, "wb");
        if (fid == nullptr || err) {
            std::cout << "Failed to open output file..." << std::endl;
            return;
        }
        
        printf("Writing to '%s'\n", filename);
        
        // construct options flag
        u32 flag = 0;
        const u32 flag_has_animations = 0x01; // 1
        
        if (m_mesh.has_skeleton)
            flag |= flag_has_animations;
        
        printf("flag = %d\n", flag);
        
        // Write to file
        u32 filesize_write = 0;
        size_t FILESIZE = 0; // will get updated later
        FILESIZE += fwrite("MESH", 1, 4, fid);
        FILESIZE += fwrite(&filesize_write, sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite(&flag, sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite("INFO", 1, 4, fid); // start mesh info section
        
        FILESIZE += fwrite(&m_mesh.num_verts,   sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite(&m_mesh.num_inds,    sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite(&m_mesh.num_submeshes, sizeof(u16), 1, fid) * sizeof(u16);
        
        FILESIZE += (fputc('v', fid) == 'v');
        FILESIZE += (fputc(vMajor, fid) == vMajor);
        FILESIZE += (fputc(vMinor, fid) == vMinor);
        FILESIZE += (fputc(vRevision, fid) == vRevision);
        
        char date_str[20];
        time_t rawtime;
        struct tm* timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(date_str, sizeof(date_str), "%d-%m-%Y %H:%M:%S", timeinfo);
        
        char comment[100];// = "sample dummy mesh";
        sprintf_s(comment, 100, "Mesh file generated on [%s] by mesh_conv version %s   ", date_str, Version());
        u16 comment_len = static_cast<u16>(strlen(comment)+1);
        FILESIZE += fwrite(&comment_len, sizeof(u16), 1, fid) * sizeof(u16);
        FILESIZE += fwrite(comment, 1, comment_len-1, fid);
        FILESIZE += (fputc(0, fid) == 0);
        
        printf("Date string: [%s]\n", date_str);
        printf("Comment: [%s]\n", comment);
        
        for (u32 n = 0; n < m_mesh.num_submeshes; n++) {
            FILESIZE += fwrite("SUBMESH", 1, 7, fid);
            FILESIZE += (fputc(0, fid) == 0);
        
            const auto& submesh = m_mesh.submeshes[n];
            FILESIZE += fwrite(&submesh.start_index,  sizeof(u32), 1,  fid) * sizeof(u32);
            FILESIZE += fwrite(&submesh.mat_index,    sizeof(u32), 1,  fid) * sizeof(u32);
            FILESIZE += fwrite(&submesh.num_indices,  sizeof(u32), 1,  fid) * sizeof(u32);
            FILESIZE += fwrite(&submesh.local_matrix, sizeof(f32), 16, fid) * sizeof(f32);
        }
        
        // Write joint heirarchy
        if (m_mesh.has_skeleton) {
            FILESIZE += fwrite("BONE", 1, 4, fid);
            const auto& skeleton = m_mesh.bind_pose;
        
            u16 num_bones = static_cast<u16>(skeleton.num_bones);
            FILESIZE += fwrite(&num_bones, sizeof(u16), 1, fid) * sizeof(u16);
        
            for (u32 n = 0; n < num_bones; n++) {
                const auto& bone = m_mesh.bind_pose.bones[n];
        
                u16 name_len = static_cast<u16>(bone.name.length() + 1);
                FILESIZE += fwrite(&name_len, sizeof(u16), 1, fid) * sizeof(u16);
                FILESIZE += fwrite(bone.name.data(), 1, name_len-1, fid);
                FILESIZE += (fputc(0, fid) == 0);
        
                // tmp for now, since we expect the parent index as an s32, but store it as a u32 here
                s32 parent_idx = bone.parent_idx == Skeleton::NullIndex ? -1 : static_cast<s32>(bone.parent_idx);

                FILESIZE += fwrite(&parent_idx, sizeof(s32), 1, fid) * sizeof(s32);
        
                FILESIZE += fwrite(&bone.local_matrix,     sizeof(f32), 16, fid) * sizeof(f32);
                FILESIZE += fwrite(&bone.inv_model_matrix, sizeof(f32), 16, fid) * sizeof(f32);
            }
        }
        
        FILESIZE += fwrite("DATA", 1, 4, fid);
        
        // Index block
        FILESIZE += fwrite("IDX", 1, 3, fid);
        FILESIZE += (fputc(0, fid) == 0);
        for (u32 n = 0; n < m_mesh.num_inds; n++) {
            FILESIZE += fwrite(&m_mesh.indices[n], sizeof(u32), 1, fid) * sizeof(u32);
        }
        
        // Vertex block
        FILESIZE += fwrite("VERT", 1, 4, fid);
        for (u32 n = 0; n < m_mesh.num_verts; n++) {
            const auto& vert = m_mesh.vertices[n];
            FILESIZE += fwrite(&vert.position.x,  sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.normal.x,    sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.tangent.x,   sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.bitangent.x, sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.tex.x,       sizeof(f32), 2, fid) * sizeof(f32);
        
            if (m_mesh.has_skeleton) {
                // tmp for now, since we expect the bone indices as an s32, but store it as a u32 here
                s32 bone_inds[MAX_BONES_PER_VERTEX] = {
                    static_cast<s32>(vert.bone_indices[0]),
                    static_cast<s32>(vert.bone_indices[1]),
                    static_cast<s32>(vert.bone_indices[2]),
                    static_cast<s32>(vert.bone_indices[3])};

                FILESIZE += fwrite(&bone_inds,               sizeof(s32), MAX_BONES_PER_VERTEX, fid) * sizeof(s32);
                FILESIZE += fwrite(vert.bone_weights.data(), sizeof(f32), MAX_BONES_PER_VERTEX, fid) * sizeof(f32);
            }
        }
        
        // Write animation names
        if (m_mesh.has_skeleton) {
            printf("Writing animations: %d\n", 0);
            FILESIZE += fwrite("ANIM", 1, 4, fid);
            u16 num_anims = static_cast<u16>(m_anims.size());
            FILESIZE += fwrite(&num_anims, sizeof(u16), 1, fid) * sizeof(u16);
        
            const Skeleton& skeleton = m_mesh.bind_pose;
        
            for (int n_anim = 0; n_anim < num_anims; n_anim++) {
                const Animation& anim = m_anims[n_anim];
                std::string anim_name = std::string(anim.name.c_str());
                
                std::replace(anim_name.begin(), anim_name.end(), '|', '-');
                
                u16 name_len = static_cast<u16>(anim_name.size()+1);
                const char* name = anim_name.c_str();
                
                printf(" Anim: '%s'\n", name);
                
                FILESIZE += fwrite(&name_len, sizeof(u16), 1, fid) * sizeof(u16);
                FILESIZE += fwrite(name, 1, name_len-1, fid);
                FILESIZE += (fputc(0, fid) == 0);
            }
        }
        
        FILESIZE += fwrite("END", 1, 3, fid);
        FILESIZE += (fputc(0, fid) == 0);
        
        fseek(fid, 4, SEEK_SET);
        filesize_write = static_cast<u32>(FILESIZE);
        fwrite(&filesize_write, sizeof(u32), 1, fid);
        
        printf("  Filesize: %i\n", filesize_write);
        
        fclose(fid);
        printf("\n");
        
        // Write animation files
        writeAnimFiles(filename);
    }

    void MeshConverter::writeAnimFiles(const char* filename) {
        if (!m_valid) return;
        
        u16 num_anims = static_cast<u16>(m_anims.size());
        
        printf("Writing %d animations to file\n", (int)num_anims);
        std::string root_path(filename);
        root_path = root_path.substr(0, root_path.find_last_of("."));
        root_path += "_";
        
        const Skeleton& skeleton = m_mesh.bind_pose;
        
        for (int n = 0; n < num_anims; n++) {
            const Animation& anim = m_anims[n];
            std::string anim_name = anim.name;
        
            std::replace(anim_name.begin(), anim_name.end(), '|', '-');
        
            std::string full_filename = root_path + anim_name + ".anim";
            printf("  Writing to '%s'\n", full_filename.c_str());
        
            // Open and check for valid file
            FILE* fid = nullptr;
            errno_t err = fopen_s(&fid, full_filename.c_str(), "wb");
            if (!fid || err) {
                std::cout << "  Failed to open output file..." << std::endl;
                continue;
            }
        
            // construct options flag
            u32 flag = 0;

            const u32 flag_non_relative_anim_transforms = 0x01; // 1
            
            flag |= flag_non_relative_anim_transforms;

            size_t FILESIZE = 0; // will get updated later
            u32 filesize_write = 0;
        
            // Write to file
            FILESIZE += fwrite("ANIM", 1, 4, fid);
            FILESIZE += fwrite(&filesize_write, sizeof(u32), 1, fid) * sizeof(u32);
        
            // Write version tag
            FILESIZE += (fputc('v', fid) == 'v');
            FILESIZE += (fputc(vMajor, fid) == vMajor);
            FILESIZE += (fputc(vMinor, fid) == vMinor);
            FILESIZE += (fputc(vRevision, fid) == vRevision);
        
            FILESIZE += fwrite(&flag, sizeof(u32), 1, fid) * sizeof(u32);
            
            /*
		    * f32 duration;
		    * f32 frame_rate;
		    * u32 num_nodes;
            */
        
            FILESIZE += fwrite(&anim.duration,   sizeof(f32), 1, fid) * sizeof(f32);
            FILESIZE += fwrite(&anim.frame_rate, sizeof(f32), 1, fid) * sizeof(f32);
            FILESIZE += fwrite(&anim.num_nodes,  sizeof(u32), 1, fid) * sizeof(u32);
        
            FILESIZE += fwrite("DATA", 1, 4, fid);
            for (u32 node_idx = 0; node_idx < anim.num_nodes; node_idx++) {
                const auto& node = anim.nodes[node_idx];

                FILESIZE += fwrite(&node.flag, sizeof(u32), 1, fid) * sizeof(u32);
                
                FILESIZE += fwrite(&node.translations.num_samples, sizeof(u32), 1, fid) * sizeof(u32);
                for (u32 n = 0; n < node.translations.num_samples; n++) {
                    FILESIZE += fwrite(&node.translations.frame_time[n],  sizeof(f32), 1, fid) * sizeof(f32);
                    FILESIZE += fwrite(node.translations.values[n]._data, sizeof(f32), 3, fid) * sizeof(f32);
                }

                FILESIZE += fwrite(&node.rotations.num_samples, sizeof(u32), 1, fid) * sizeof(u32);
                for (u32 n = 0; n < node.rotations.num_samples; n++) {
                    FILESIZE += fwrite(&node.rotations.frame_time[n],  sizeof(f32), 1, fid) * sizeof(f32);
                    FILESIZE += fwrite(node.rotations.values[n]._data, sizeof(f32), 4, fid) * sizeof(f32);
                }

                FILESIZE += fwrite(&node.scales.num_samples, sizeof(u32), 1, fid) * sizeof(u32);
                for (u32 n = 0; n < node.scales.num_samples; n++) {
                    FILESIZE += fwrite(&node.scales.frame_time[n],  sizeof(f32), 1, fid) * sizeof(f32);
                    FILESIZE += fwrite(node.scales.values[n]._data, sizeof(f32), 3, fid) * sizeof(f32);
                }
            }
        
            FILESIZE += fwrite("END", 1, 3, fid);
            FILESIZE += (fputc(0, fid) == 0);
        
            fseek(fid, 4, SEEK_SET);
            filesize_write = static_cast<u32>(FILESIZE);
            fwrite(&filesize_write, sizeof(u32), 1, fid);
        
            printf("    Filesize: %i\n", filesize_write);
        }
    }
}