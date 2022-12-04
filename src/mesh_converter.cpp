#include "mesh_converter.h"

#include <iostream>

#include "mesh.h"
#include <iostream>
#include <ctime>
#include <unordered_map>
#include <assert.h>
#include <algorithm>
#include <cassert>

#include "animation.h"
#include "utils.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tinygltf/tiny_gltf.h"

namespace rh {
    const char* build_version = "v0.10.0";
    const char vMajor = 0;
    const char vMinor = 10;
    const char vRevision = 0;

    const size_t MAX_BONES_PER_VERTEX = 4;
    
    const char* Version() {
        return build_version;
    }

    MeshConverter::MeshConverter() : m_valid(false) {}
    MeshConverter::~MeshConverter() {}

    void log_print(int level, const char* format, ...) {
        va_list args;
        va_start(args, format);

        for (int n = 0; n < level; n++) {
            printf(". ");
        }

        vprintf(format, args);

        va_end(args);
    }

    bool has_correct_attributes(const std::map<std::string, int>& attributes) {
        if (attributes.find("POSITION") == attributes.end()) return false;
        if (attributes.find("NORMAL") == attributes.end()) return false;
        if (attributes.find("TANGENT") == attributes.end()) return false;
        if (attributes.find("TEXCOORD_0") == attributes.end()) return false;

        return true;
    }

    bool has_correct_attributes_skin(const std::map<std::string, int>& attributes) {
        if (!has_correct_attributes(attributes)) return false;
        if (attributes.find("JOINTS_0") == attributes.end()) return false;
        if (attributes.find("WEIGHTS_0") == attributes.end()) return false;

        return true;
    }

    const unsigned char* read_buffer_view(const tinygltf::Model& tinyModel, int buffer_view_idx) {
        const tinygltf::BufferView bufferView = tinyModel.bufferViews[buffer_view_idx];

        //bufferView.buffer;
        //bufferView.byteOffset;
        //bufferView.byteLength;
        //bufferView.byteStride;

        const tinygltf::Buffer& buffer = tinyModel.buffers[bufferView.buffer];
        return (buffer.data.data() + bufferView.byteOffset);
    }

    template<typename Component_Type> 
    Component_Type read_component(int componentType, const u8* cur_element) {
        switch(componentType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE: {
                char comp = 0;
                memcpy(&comp, cur_element, sizeof(char));

                return static_cast<Component_Type>(comp);
            } case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                unsigned char comp = 0;
                memcpy(&comp, cur_element, sizeof(unsigned char));

                return static_cast<Component_Type>(comp);
            } case TINYGLTF_COMPONENT_TYPE_SHORT: {
                short comp = 0;
                memcpy(&comp, cur_element, sizeof(short));

                return static_cast<Component_Type>(comp);
            } case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                unsigned short comp = 0;
                memcpy(&comp, cur_element, sizeof(unsigned short));

                return static_cast<Component_Type>(comp);
            } case TINYGLTF_COMPONENT_TYPE_INT: {
                int comp = 0;
                memcpy(&comp, cur_element, sizeof(int));

                return static_cast<Component_Type>(comp);
            } case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                unsigned int comp = 0;
                memcpy(&comp, cur_element, sizeof(unsigned int));

                return static_cast<Component_Type>(comp);
            } case TINYGLTF_COMPONENT_TYPE_FLOAT: {
                float comp = 0;
                memcpy(&comp, cur_element, sizeof(float));

                return static_cast<Component_Type>(comp);
            } case TINYGLTF_COMPONENT_TYPE_DOUBLE: {
                double comp = 0;
                memcpy(&comp, cur_element, sizeof(double));

                return static_cast<Component_Type>(comp);
            } default:
                assert(false);
        }

        Component_Type ignore_this_warning;
        return ignore_this_warning;
    }

    template<typename Element_Type, typename Component_Type>
    std::vector<Element_Type> extract_accessor(const tinygltf::Model& tinyModel, int accessor_idx, int level) {
        assert(accessor_idx >= 0);

        const tinygltf::Accessor& accessor = tinyModel.accessors[accessor_idx];
        if (accessor.sparse.isSparse) {
            log_print(level, "Sparse accessor not supported yet x.x\n");
            assert(false);
        }
        //log_print(level, "accessor.type = %d\n", accessor.type);
        //log_print(level, "accessor.componentType = %d\n", accessor.componentType);
        const tinygltf::BufferView bufferView = tinyModel.bufferViews[accessor.bufferView];
        accessor.byteOffset;
        int num_components = tinygltf::GetNumComponentsInType(accessor.type);
        int num_bytes_per_Component = tinygltf::GetComponentSizeInBytes(accessor.componentType);
        int num_bytes_per_Element = accessor.ByteStride(bufferView);
        //assert(sizeof(Element_Type) == num_bytes_per_Element);
        assert(num_components*num_bytes_per_Component == num_bytes_per_Element);

        const u8* raw_data = read_buffer_view(tinyModel, accessor.bufferView); // ptr to start of byte stream for this accessor
        const u8* cur_element = raw_data; // our current cursor
        const u8* cur_comp = cur_element;

        std::vector<Element_Type> elements;
        for (u32 n = 0; n < accessor.count; n++) {
            // for each type (SCALAR, VECn, MATn, etc..)
            cur_element = raw_data + (n * num_bytes_per_Element);
            Element_Type element;

            if (accessor.type == TINYGLTF_TYPE_SCALAR) {
                // simple case where Element_Type == Component_Type, and size is 1
                element = read_component<Element_Type>(accessor.componentType, cur_element);
            } else {
                for (u32 i = 0; i < num_components; i++) {
                    // if scalar, this just happens once per value of n.
                    // otherwise,this acts on each component of the output
                    // i.e. v.x, v.y, v.z for a VEC3

                    cur_comp = cur_element + (i * num_bytes_per_Component);

                    Component_Type comp = read_component<Component_Type>(accessor.componentType, cur_comp);
                    memcpy(((Component_Type*)(&element)) + (i), &comp, sizeof(Component_Type));
                }
            }

            elements.push_back(element);
        }

        if (accessor.normalized) {}

        return elements;
    }

    struct GLPrimitive {
        std::vector<u32> indices;

        std::vector<laml::Vec3> positions;
        std::vector<laml::Vec3> normals;
        std::vector<laml::Vec4> tangents;
        std::vector<laml::Vec2> texcoords;

        // only for animated meshes
        std::vector<laml::Vector<s32, 4>> bone_idx;
        std::vector<laml::Vec4> bone_weights;
    };

    struct GLMesh {
        bool is_rigged;
        std::vector<GLPrimitive> primitives;
    };

    struct GLBone {
        laml::Mat4 inv_model_matrix;
        laml::Mat4 local_matrix;
        int parent_idx;
        std::string name;
        
        int bone_idx;
        int node_idx;
    };

    struct GLSkeleton {
        u32 num_bones;
        std::vector<GLBone> bones;
    };

    void process_mesh(const tinygltf::Model& model, const tinygltf::Mesh& tinyMesh, GLMesh& mesh, int level) {
        int num_submeshes = tinyMesh.primitives.size();
        mesh.primitives.resize(num_submeshes);
        mesh.is_rigged = false;

        for (int n = 0; n < num_submeshes; n++) {
            tinygltf::Primitive prim = tinyMesh.primitives[n];
            assert(prim.mode == TINYGLTF_MODE_TRIANGLES);
            assert(has_correct_attributes(prim.attributes));

            //log_print(level, " Extracting mesh indices!\n");
            mesh.primitives[n].indices = extract_accessor<u32, u32>(model, prim.indices, level+1);

            //log_print(level, " Extracting static mesh attributes!\n");
            mesh.primitives[n].positions = extract_accessor<laml::Vec3, f32>(model, prim.attributes["POSITION"],   level + 1);
            mesh.primitives[n].normals   = extract_accessor<laml::Vec3, f32>(model, prim.attributes["NORMAL"],     level + 1);
            mesh.primitives[n].tangents  = extract_accessor<laml::Vec4, f32>(model, prim.attributes["TANGENT"],    level + 1);
            mesh.primitives[n].texcoords = extract_accessor<laml::Vec2, f32>(model, prim.attributes["TEXCOORD_0"], level + 1);
        }
    }

    void process_anim_mesh(const tinygltf::Model& model, const tinygltf::Mesh& tinyMesh, GLMesh& mesh, int level) {
        process_mesh(model, tinyMesh, mesh, level);
        int num_submeshes = tinyMesh.primitives.size();
        mesh.is_rigged = true;

        // check animation data is actually there
        for (int n = 0; n < num_submeshes; n++) {
            tinygltf::Primitive prim = tinyMesh.primitives[n];
            assert(prim.mode == TINYGLTF_MODE_TRIANGLES);
            assert(has_correct_attributes_skin(prim.attributes));

            //log_print(level, " Extracting skinned mesh attributes!\n");
            mesh.primitives[n].bone_idx     = extract_accessor<laml::Vector<s32, 4>, s32>(model, prim.attributes["JOINTS_0"], level + 1);
            mesh.primitives[n].bone_weights = extract_accessor<laml::Vector<f32, 4>, f32>(model, prim.attributes["WEIGHTS_0"], level + 1);
        }
    }

    laml::Mat4 get_node_local_transform(const tinygltf::Node& node, int level) {
        /*
         * tinygltf stores these transforms as doubles, so need to cast them down component-wise
         */

        // if the node has the matrix outright
        if (node.matrix.size() == 16) {
            laml::Mat4 transform;
            for (u32 col = 0; col < 4; col++) {
                for (u32 row = 0; row < 4; row++) {
                    u32 n = (col*4) + row;
                    transform[col][row] = static_cast<f32>(node.matrix[n]);
                }
            }
            return transform;
        }

        // otherwise constuct it from /possible/ components
        laml::Vec3 translation(0.0f); 
        laml::Vec3 scale(1.0f);
        laml::Quat rotation(0.0f, 0.0f, 0.0f, 1.0f);
        if (node.translation.size() == 3) {
            for (u32 n = 0; n < 3; n++) {
                translation[n] = static_cast<f32>(node.translation[n]);
            }
        }
        if (node.rotation.size() == 4) {
            for (u32 n = 0; n < 4; n++) {
                rotation[n] = static_cast<f32>(node.rotation[n]);
            }
        }
        if (node.scale.size() == 3) {
            for (u32 n = 0; n < 3; n++) {
                scale[n] = static_cast<f32>(node.scale[n]);
            }
        }

        laml::Mat4 transform(1.0f);
        laml::transform::create_transform(transform, rotation, translation, scale);
        return transform;
    }

    void crawl_skeleton_tree(const tinygltf::Model& model, rh::GLSkeleton& skeleton, int bone_idx, int parent_idx, int level) {
        GLBone& bone = skeleton.bones[bone_idx];
        bone.parent_idx = parent_idx;
        const tinygltf::Node& node = model.nodes[bone.node_idx];
        
        bone.local_matrix = get_node_local_transform(node, level);
        bone.name = node.name.c_str();

        int num_children = node.children.size();
        for (int n = 0; n < num_children; n++) {
            int child_node_idx = node.children[n];
            int child_bone_idx = -1;
            for (int i = 0; i < skeleton.num_bones; i++) {
                if (skeleton.bones[i].node_idx == child_node_idx) {
                    child_bone_idx = skeleton.bones[i].bone_idx;
                    break;
                }
            }
            assert(child_bone_idx >= 0);
            crawl_skeleton_tree(model, skeleton, child_bone_idx, bone_idx, level + 1);
        }
    }

    bool valid_skeleton_indices(const GLSkeleton& skeleton) {
        for (int bone_idx = 0; bone_idx < skeleton.num_bones; bone_idx++) {
            int parent_bone_idx = skeleton.bones[bone_idx].parent_idx;
            if (parent_bone_idx >= bone_idx) {
                log_print(0, "ERROR: bone %d has a parent of %d, which is not a valid order!\n", bone_idx, parent_bone_idx);
                return false;
            }
        }

        return true;
    }

    void process_skin(const tinygltf::Model& model, const tinygltf::Skin& glSkin, GLSkeleton& skeleton, int level) {
        log_print(level, "Extracting skeleton!\n");
        //log_print(level, " InverseBindID = %d\n", glSkin.inverseBindMatrices);
        //log_print(level, " SkeletonID = %d\n", glSkin.skeleton);
        log_print(level, " Skin Name: '%s'\n", glSkin.name.c_str());
        log_print(level, " %d Joints\n", (int)glSkin.joints.size());


        std::vector<laml::Mat4> invBindMatrices = extract_accessor<laml::Mat4, f32>(model, glSkin.inverseBindMatrices, level + 1);

        skeleton.num_bones = glSkin.joints.size();
        skeleton.bones.resize(skeleton.num_bones);
        for (u32 n = 0; n < skeleton.num_bones; n++) {
            GLBone &bone = skeleton.bones[n];
            bone.inv_model_matrix = invBindMatrices[n];
            bone.bone_idx = n;
            bone.node_idx = glSkin.joints[n];
            //log_print(level, "%d -> %d", n, glSkin.joints[n]);
        }

        // no crawl the skeleton to assign parent ids
        crawl_skeleton_tree(model, skeleton, 0, -1, level + 1);
        assert(valid_skeleton_indices(skeleton));
    }

    rh::MeshData CreateMeshData(const GLMesh* glmesh, const GLSkeleton* glskeleton) {
        assert(glmesh);
        
        MeshData mesh;
        // mesh has a skin
        mesh.has_skeleton = (glskeleton != nullptr);
        mesh.num_submeshes = glmesh->primitives.size();
        mesh.submeshes.resize(mesh.num_submeshes);

        u32 index_count = 0;
        u32 vertex_count = 0;
        for (u32 n = 0; n < mesh.num_submeshes; n++) {
            SubMesh& sm = mesh.submeshes[n];
            const GLPrimitive& prim = glmesh->primitives[n];

            sm.start_index = index_count;
            sm.start_vertex = vertex_count;
            sm.mat_index = ~0; // NOT USING RN
            sm.num_indices = prim.indices.size();
            sm.num_vertices = prim.positions.size();
            sm.local_matrix = laml::Mat4(1.0f);
            sm.model_matrix = laml::Mat4(1.0f);

            // increment for next submesh
            index_count += sm.num_indices;
            vertex_count += sm.num_vertices;

            // concatenate vertices
            for (u32 v = 0; v < sm.num_vertices; v++) {
                Vertex vert;

                vert.position = prim.positions[v];
                vert.normal = prim.normals[v];
                vert.tex = prim.texcoords[v];
                    
                // gltf stores tangents as a vec4.
                // xyz is the normalized tangent, w is a signed value for the bitangent.
                // bitangent = cross(normal, tangent.xyz) * tangent.w
                vert.tangent = laml::Vec3(prim.tangents[v].x, prim.tangents[v].y, prim.tangents[v].z);
                vert.bitangent = laml::cross(vert.normal, vert.tangent) * prim.tangents[v].w;
                    
                // skinned attributes
                if (mesh.has_skeleton) {
                    vert.bone_indices = prim.bone_idx[v];
                    vert.bone_weights = prim.bone_weights[v];
                }

                mesh.vertices.push_back(vert);
            }

            // concatenate indices
            for (u32 i = 0; i < sm.num_indices; i++) {
                mesh.indices.push_back(prim.indices[i]);
            }
        }

        mesh.num_inds = index_count;
        mesh.num_verts = vertex_count;
        assert(mesh.num_inds == mesh.indices.size());
        assert(mesh.num_verts == mesh.vertices.size());

        // now extract bind pose info
        if (glskeleton) {
            mesh.bind_pose.num_bones = glskeleton->num_bones;
            mesh.bind_pose.bones.resize(mesh.bind_pose.num_bones);

            for (u32 n = 0; n < mesh.bind_pose.num_bones; n++) {
                bone_info& bone = mesh.bind_pose.bones[n];
                const GLBone& glbone = glskeleton->bones[n];

                bone.parent_idx = glbone.parent_idx;
                bone.local_matrix = glbone.local_matrix;
                bone.inv_model_matrix = glbone.inv_model_matrix;
                bone.name = glbone.name;
            }
        } 

        return mesh;
    }

    void traverse_nodes(const tinygltf::Model& model, const tinygltf::Node& node, std::vector<MeshData>& out_meshes, int level) {
        log_print(level, "Node: '%s'\n", node.name.c_str());

        bool has_camera = node.camera >= 0; // unused
        bool has_mesh   = node.mesh   >= 0;
        bool has_skin   = node.skin   >= 0;

        if (has_mesh && has_skin) {
            log_print(level+1, "Processing Animated Mesh!\n");
            GLMesh mesh;
            process_anim_mesh(model, model.meshes[node.mesh], mesh, level + 1);
            GLSkeleton skeleton;
            process_skin(model, model.skins[node.skin], skeleton, level + 1);

            out_meshes.push_back(CreateMeshData(&mesh, &skeleton));
        }
        else if (has_mesh) {
            log_print(level+1, "Processing Static Mesh!\n");
            GLMesh mesh;
            process_mesh(model, model.meshes[node.mesh], mesh, level+1);

            out_meshes.push_back(CreateMeshData(&mesh, nullptr));
        }

        //if (node.children.size() > 0) { log_print(level, " %d Children: \n", node.children.size()); }
        for (int n = 0; n < node.children.size(); n++) {
            traverse_nodes(model, model.nodes[node.children[n]], out_meshes, level + 1);
        }
    }

    void MeshConverter::LoadInputFile(const char* filename) {
        printf("----------------Loading------------------\n");
        printf("Loading file: '%s'\n", filename);

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;
        
        bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);

        if (!warn.empty()) {
            printf("Warn: %s\n", warn.c_str());
        }

        if (!err.empty()) {
            printf("Err: %s\n", err.c_str());
        }

        if (!ret) {
            printf("Failed to parse glTF\n");
            return;
        }

        tinygltf::Scene& scene = model.scenes[model.defaultScene];
        //printf("Scene: '%s'\n", scene.name.c_str());
        //printf("  %d nodes\n", (int)scene.nodes.size());
        //for (int n = 0; n < scene.nodes.size(); n++) {
        //    auto node = model.nodes[scene.nodes[n]];
        //    printf("   [%d] node %d '%s': %d children\n", n, scene.nodes[n], node.name.c_str(), node.children.size());
        //}

        for (int n = 0; n < model.scenes.size(); n++) {
            printf("Scene: %s\n", model.scenes[0].name.c_str());
            traverse_nodes(model, model.nodes[scene.nodes[0]], m_meshes, 1);
        }

        printf("-----------------------------------------\n");
    }

    void MeshConverter::ProcessFile() {
        if (!m_valid) return;
        printf("---------------Processing----------------\n");
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
                FILESIZE += fwrite(&vert.bone_indices.x, sizeof(s32), MAX_BONES_PER_VERTEX, fid) * sizeof(s32);
                FILESIZE += fwrite(&vert.bone_indices.x, sizeof(f32), MAX_BONES_PER_VERTEX, fid) * sizeof(f32);
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