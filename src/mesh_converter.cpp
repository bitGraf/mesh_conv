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
    const char* build_version = "v0.7.0";
    const char vMajor = 0;
    const char vMinor = 7;
    const char vRevision = 0;

    const size_t MAX_BONES_PER_VERTEX = 4;
    
    const char* Version() {
        return build_version;
    }

    MeshConverter::MeshConverter() {}
    MeshConverter::~MeshConverter() {}

    void MeshConverter::LoadInputFile(const char* filename, const char* filename_out) {
        MeshSource ms(filename);

        printf("Animation count: %d\n", ms.GetAnimationCount());
        printf("Bones:\n");
        auto skele = ms.GetSkeleton();
        for (int n = 0; n < skele->GetBoneNames().size(); n++) {
            printf("  [%s]\n", skele->GetBoneName(n).c_str());
        }
        printf("submesh count: %d\n", ms.GetSubmeshes().size());
        std::cout << " transform: " << ms.GetSubmeshes()[0].Transform << std::endl;
        std::cout << " transform: " << ms.GetSubmeshes()[0].LocalTransform << std::endl;

        ProcessFile(&ms);
        SaveOutputFile(filename_out, &ms);

        printf("\n");
    }

    void MeshConverter::ProcessFile(MeshSource* ms) {
    }

    laml::Mat4 construct_inv_bind_pose(const Skeleton* skeleton, u32 bone_idx) {
        // build local transform
        auto trans_vec = skeleton->GetBoneTranslations()[bone_idx];
        auto rot_quat = skeleton->GetBoneRotations()[bone_idx];
        auto scale_vec = skeleton->GetBoneScales()[bone_idx];

        laml::Mat4 local_transform;
        laml::transform::create_transform(local_transform, rot_quat, trans_vec, scale_vec);

        u32 parent_idx = skeleton->GetParentBoneIndex(bone_idx);
        while (parent_idx != Skeleton::NullIndex) {
            laml::Mat4 parent_transform;
            trans_vec = skeleton->GetBoneTranslations()[parent_idx];
            rot_quat = skeleton->GetBoneRotations()[parent_idx];
            scale_vec = skeleton->GetBoneScales()[parent_idx];
            laml::transform::create_transform(parent_transform, rot_quat, trans_vec, scale_vec);
            local_transform = laml::mul(parent_transform, local_transform);

            parent_idx = skeleton->GetParentBoneIndex(parent_idx);
        }

        return local_transform;
    }

    void MeshConverter::SaveOutputFile(const char* filename, const MeshSource* ms) {
        if (!ms) return;

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
        
        if (ms->HasSkeleton())
            flag |= flag_has_animations;

        printf("flag = %d\n", flag);
        
        // Write to file
        u32 filesize_write = 0;
        size_t FILESIZE = 0; // will get updated later
        FILESIZE += fwrite("MESH", 1, 4, fid);
        FILESIZE += fwrite(&filesize_write, sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite(&flag, sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite("INFO", 1, 4, fid); // start mesh info section

        u32 num_verts = ms->GetVertices().size();
        u32 num_tris = ms->GetIndices().size();
        u32 num_inds = num_tris * 3;
        u16 num_submesh = ms->GetSubmeshes().size();
        FILESIZE += fwrite(&num_verts,   sizeof(num_verts), 1, fid) *  sizeof(num_verts);
        FILESIZE += fwrite(&num_inds,    sizeof(num_inds), 1, fid) *   sizeof(num_inds);
        FILESIZE += fwrite(&num_submesh, sizeof(num_submesh), 1, fid) *sizeof(num_submesh);
        
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
        
        for (int n = 0; n < num_submesh; n++) {
            FILESIZE += fwrite("SUBMESH", 1, 7, fid);
            FILESIZE += (fputc(0, fid) == 0);
        
            auto submesh = ms->GetSubmeshes()[n];
            FILESIZE += fwrite(&submesh.BaseIndex,     sizeof(u32), 1,  fid) * sizeof(u32);
            FILESIZE += fwrite(&submesh.MaterialIndex, sizeof(u32), 1,  fid) * sizeof(u32);
            FILESIZE += fwrite(&submesh.IndexCount,    sizeof(u32), 1,  fid) * sizeof(u32);
            FILESIZE += fwrite(&submesh.LocalTransform,sizeof(f32), 16, fid) * sizeof(f32);
        }
        
        // Write joint heirarchy
        if (ms->HasSkeleton()) {
            FILESIZE += fwrite("BONE", 1, 4, fid);
            auto skeleton = ms->GetSkeleton();
        
            u16 num_bones = static_cast<u16>(skeleton->GetNumBones());
            FILESIZE += fwrite(&num_bones, sizeof(u16), 1, fid) * sizeof(u16);
        
            for (int n = 0; n < num_bones; n++) {
                auto _bone_name = skeleton->GetBoneName(n);
                s32 parent_idx = skeleton->GetParentBoneIndex(n);
                laml::Mat4 inv_bind_pose = construct_inv_bind_pose(skeleton, n);
        
                u16 name_len = static_cast<u16>(_bone_name.length() + 1);
                FILESIZE += fwrite(&name_len, sizeof(u16), 1, fid) * sizeof(u16);
                FILESIZE += fwrite(_bone_name.data(), 1, name_len-1, fid);
                FILESIZE += (fputc(0, fid) == 0);
        
                FILESIZE += fwrite(&parent_idx, sizeof(s32), 1, fid) * sizeof(s32);
        
                FILESIZE += fwrite(&inv_bind_pose, sizeof(f32), 16, fid) * sizeof(f32);
                //FILESIZE += fwrite(&bone.local_pos, sizeof(f32), 3, fid) * sizeof(f32);
                //FILESIZE += fwrite(&bone.local_rot, sizeof(f32), 4, fid) * sizeof(f32);
                //FILESIZE += fwrite(&bone.local_scale, sizeof(f32), 3, fid) * sizeof(f32);
            }
        }
        
        FILESIZE += fwrite("DATA", 1, 4, fid);
        
        // Index block
        FILESIZE += fwrite("IDX", 1, 3, fid);
        FILESIZE += (fputc(0, fid) == 0);
        for (u32 n = 0; n < num_tris; n++) {
            auto face = ms->GetIndices()[n];
            u32 index = face.V1;
            FILESIZE += fwrite(&index, sizeof(u32), 1, fid) * sizeof(u32);

            index = face.V2;
            FILESIZE += fwrite(&index, sizeof(u32), 1, fid) * sizeof(u32);

            index = face.V3;
            FILESIZE += fwrite(&index, sizeof(u32), 1, fid) * sizeof(u32);
        }
        
        // Vertex block
        FILESIZE += fwrite("VERT", 1, 4, fid);
        for (u32 n = 0; n < num_verts; n++) {
            auto vert = ms->GetVertices()[n];
            FILESIZE += fwrite(&vert.Position.x, sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.Normal.x, sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.Tangent.x, sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.Bitangent.x, sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.Texcoord.x, sizeof(f32), 2, fid) * sizeof(f32);

            if (ms->HasSkeleton()) {
                auto bone_weights = ms->GetBoneInfluences()[n];
                s32 bone_idx[MAX_BONES_PER_VERTEX] = { bone_weights.BoneInfoIndices[0], bone_weights.BoneInfoIndices[1], bone_weights.BoneInfoIndices[2], bone_weights.BoneInfoIndices[3]};
                FILESIZE += fwrite(bone_idx, sizeof(s32), MAX_BONES_PER_VERTEX, fid) * sizeof(s32);
                FILESIZE += fwrite(bone_weights.Weights, sizeof(f32), MAX_BONES_PER_VERTEX, fid) * sizeof(f32);
            }
        }
        
        // Write animation names
        if (ms->GetAnimationCount() > 0) {
            printf("Writing animations: %d\n", ms->GetAnimationCount());
            FILESIZE += fwrite("ANIM", 1, 4, fid);
            u16 num_anims = static_cast<u16>(ms->GetAnimationCount());
            FILESIZE += fwrite(&num_anims, sizeof(u16), 1, fid) * sizeof(u16);

            const Skeleton* skeleton = ms->GetSkeleton();
        
            for (int n_anim = 0; n_anim < num_anims; n_anim++) {
                const Animation* anim = ms->GetAnimation(n_anim, skeleton);
                std::string anim_name = std::string(anim->GetName().c_str());

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
        writeAnimFiles(filename, ms);
    }

    void MeshConverter::writeAnimFiles(const char* filename, const MeshSource* ms) {
        if (!ms) return;

        u16 num_anims = static_cast<u16>(ms->GetAnimationCount());
        
        printf("Writing %d animations to file\n", (int)num_anims);
        std::string root_path(filename);
        root_path = root_path.substr(0, root_path.find_last_of("."));
        root_path += "_";

        const Skeleton* skeleton = ms->GetSkeleton();
        
        for (int n = 0; n < num_anims; n++) {
            auto anim = ms->GetAnimation(n, ms->GetSkeleton());
            std::string anim_name = anim->GetName();
        
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
            * u16 num_channels; // num_bones
            * f32 duration
            * u32 num_translation_keys
            * u32 num_rotation_keys
            * u32 num_scale_keys
            */
            const auto& trans_keys = anim->GetTranslationKeys();
            const auto& rotate_keys = anim->GetRotationKeys();
            const auto& scale_keys = anim->GetScaleKeys();

            u16 num_channels = skeleton->GetNumBones();
            u16 num_translate_keys = trans_keys.size();
            u16 num_rotate_keys = rotate_keys.size();
            u16 num_scale_keys = scale_keys.size();
            f32 duration = anim->GetDuration();

            FILESIZE += fwrite(&num_channels, sizeof(u16), 1, fid) * sizeof(u16);
            FILESIZE += fwrite(&num_translate_keys, sizeof(u16), 1, fid) * sizeof(u16);
            FILESIZE += fwrite(&num_rotate_keys, sizeof(u16), 1, fid) * sizeof(u16);
            FILESIZE += fwrite(&num_scale_keys, sizeof(u16), 1, fid) * sizeof(u16);
            FILESIZE += fwrite(&duration, sizeof(f32), 1, fid) * sizeof(f32);
        
            FILESIZE += fwrite("DATA", 1, 4, fid);
            for (int k = 0; k < num_translate_keys; k++) {
                const auto& key = trans_keys[k];
        
                FILESIZE += fwrite(&key.Track, sizeof(u32), 1, fid) * sizeof(u32);
                FILESIZE += fwrite(&key.FrameTime, sizeof(f32), 1, fid) * sizeof(f32);
                FILESIZE += fwrite(&key.Value, sizeof(f32), 3, fid) * sizeof(f32);
            }
            for (int k = 0; k < num_rotate_keys; k++) {
                const auto& key = rotate_keys[k];

                FILESIZE += fwrite(&key.Track, sizeof(u32), 1, fid) * sizeof(u32);
                FILESIZE += fwrite(&key.FrameTime, sizeof(f32), 1, fid) * sizeof(f32);
                FILESIZE += fwrite(&key.Value, sizeof(f32), 4, fid) * sizeof(f32);
            }
            for (int k = 0; k < num_scale_keys; k++) {
                const auto& key = scale_keys[k];

                FILESIZE += fwrite(&key.Track, sizeof(u32), 1, fid) * sizeof(u32);
                FILESIZE += fwrite(&key.FrameTime, sizeof(f32), 1, fid) * sizeof(f32);
                FILESIZE += fwrite(&key.Value, sizeof(f32), 3, fid) * sizeof(f32);
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