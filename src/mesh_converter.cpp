#include "mesh_converter.h"

#include <iostream>

#include "mesh.h"
#include <iostream>
#include <ctime>
#include <unordered_map>
#include <assert.h>
#include <algorithm>
#include <cassert>
#include <cstdio>

#include "animation.h"
#include "utils.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tinygltf/tiny_gltf.h"

int printf (const char * format, ...); // for syntax highlighting...

namespace rh {
    const char* build_version = "v1.0.0";
    const char vMajor = 1;
    const char vMinor = 0;
    const char vRevision = 0;

    const size_t MAX_BONES_PER_VERTEX = 4;
    
    const char* Version() {
        return build_version;
    }

    MeshConverter::MeshConverter() : m_valid(false), m_sample_frame_rate(30.0f) {}
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
        //if (attributes.find("NORMAL") == attributes.end()) return false;
        //if (attributes.find("TANGENT") == attributes.end()) return false;
        //if (attributes.find("TEXCOORD_0") == attributes.end()) return false;

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
        switch (componentType) {
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
        //log_print(level, "num_components: %d\n", num_components);
        //log_print(level, "num_bytes_per_Component: %d\n", num_bytes_per_Component);
        //log_print(level, "num_bytes_per_Element: %d\n", num_bytes_per_Element);
        //assert(sizeof(Element_Type) == num_bytes_per_Element);
        //assert(num_components*num_bytes_per_Component == num_bytes_per_Element);

        const u8* raw_data = read_buffer_view(tinyModel, accessor.bufferView); // ptr to start of byte stream for this accessor
        const u8* cur_element = raw_data; // our current cursor
        const u8* cur_comp = cur_element;

        std::vector<Element_Type> elements;
        for (u32 n = 0; n < accessor.count; n++) {
            // for each type (SCALAR, VECn, MATn, etc..)
            cur_element = raw_data + (n * num_bytes_per_Element);
            Element_Type element;

            for (u32 i = 0; i < num_components; i++) {
                // if scalar, this just happens once per value of n.
                // otherwise,this acts on each component of the output
                // i.e. v.x, v.y, v.z for a VEC3

                cur_comp = cur_element + (i * num_bytes_per_Component);

                Component_Type comp = read_component<Component_Type>(accessor.componentType, cur_comp);
                memcpy(((Component_Type*)(&element)) + (i), &comp, sizeof(Component_Type));
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
        std::string name;
        laml::Mat4 local_matrix;
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
        mesh.name = tinyMesh.name;

        for (int n = 0; n < num_submeshes; n++) {
            tinygltf::Primitive prim = tinyMesh.primitives[n];
            assert(prim.mode == TINYGLTF_MODE_TRIANGLES);
            assert(has_correct_attributes(prim.attributes));

            //log_print(level, " Extracting mesh indices!\n");
            mesh.primitives[n].indices = extract_accessor<u32, u32>(model, prim.indices, level + 1);

            //log_print(level, " Extracting static mesh attributes!\n");
            mesh.primitives[n].positions = extract_accessor<laml::Vec3, f32>(model, prim.attributes["POSITION"],   level + 1);
            
            //mesh.primitives[n].normals.resize(mesh.primitives[n].positions.size());
            //mesh.primitives[n].tangents.resize(mesh.primitives[n].positions.size());
            //mesh.primitives[n].texcoords.resize(mesh.primitives[n].positions.size());

            mesh.primitives[n].normals = extract_accessor<laml::Vec3, f32>(model, prim.attributes["NORMAL"],     level + 1);
            mesh.primitives[n].tangents = extract_accessor<laml::Vec4, f32>(model, prim.attributes["TANGENT"],    level + 1);
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
            mesh.primitives[n].bone_idx = extract_accessor<laml::Vector<s32, 4>, s32>(model, prim.attributes["JOINTS_0"], level + 1);
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
            bone.bone_idx = n;
            bone.node_idx = glSkin.joints[n];
            bone.inv_model_matrix = (invBindMatrices[bone.bone_idx]); // TODO: why?
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
        mesh.mesh_name = glmesh->name;

        u32 index_count = 0;
        u32 vertex_count = 0;
        for (u32 n = 0; n < mesh.num_submeshes; n++) {
            SubMesh& sm = mesh.submeshes[n];
            const GLPrimitive& prim = glmesh->primitives[n];

            sm.start_index = index_count;
            sm.start_vertex = vertex_count;
            sm.mat_index = 0; // NOT USING RN
            sm.num_indices = prim.indices.size();
            sm.num_vertices = prim.positions.size();
            sm.local_matrix = glmesh->local_matrix;
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
                bone.node_idx = glbone.node_idx;
                bone.local_matrix = glbone.local_matrix;
                bone.inv_model_matrix = glbone.inv_model_matrix;
                bone.name = glbone.name;

                std::cout << "Bone " << bone.name << " " << bone.inv_model_matrix << std::endl;
            }
        }

        return mesh;
    }

    void traverse_nodes(const tinygltf::Model& model, const tinygltf::Node& node, std::vector<MeshData>& out_meshes, laml::Mat4& parent_transform, int level) {
        log_print(level, "Node: '%s'\n", node.name.c_str());

        bool has_camera = node.camera >= 0; // unused
        bool has_mesh = node.mesh >= 0;
        bool has_skin = node.skin >= 0;

        laml::Mat4 node_local_transform = get_node_local_transform(node, level);
        laml::Mat4 node_model_transform = laml::mul(parent_transform, node_local_transform);
        log_print(level, "Local_Transform: ");
        laml::print((node_local_transform), "%.2f");
        printf("\n");
        log_print(level, "Model_Transform: ");
        laml::print((node_model_transform), "%.2f");
        printf("\n");

        if (has_mesh && has_skin) {
            log_print(level + 1, "Processing Animated Mesh!\n");
            GLMesh mesh;
            process_anim_mesh(model, model.meshes[node.mesh], mesh, level + 1);
            mesh.local_matrix = node_local_transform;
            GLSkeleton skeleton;
            process_skin(model, model.skins[node.skin], skeleton, level + 1);

            out_meshes.push_back(CreateMeshData(&mesh, &skeleton));
        }
        else if (has_mesh) {
            log_print(level + 1, "Processing Static Mesh!\n");
            GLMesh mesh;
            mesh.local_matrix = node_local_transform;
            process_mesh(model, model.meshes[node.mesh], mesh, level + 1);

            out_meshes.push_back(CreateMeshData(&mesh, nullptr));
        }

        //if (node.children.size() > 0) { log_print(level, " %d Children: \n", node.children.size()); }
        for (int n = 0; n < node.children.size(); n++) {
            traverse_nodes(model, model.nodes[node.children[n]], out_meshes, node_local_transform, level + 1);
        }
    }

    template<typename T>
    std::vector<T> sample_anim_channel(const tinygltf::Model& tinymodel, 
                             const tinygltf::AnimationChannel& channel,
                             const tinygltf::AnimationSampler& sampler,
                             f32 sample_frame_rate, bool should_normalize,
                             const std::function<T(const T&, const T&, float)>& interpolater) {

        const tinygltf::Accessor& input_acc = tinymodel.accessors[sampler.input];
        const tinygltf::Accessor& output_acc = tinymodel.accessors[sampler.output];

        std::vector<f32> frame_times  = extract_accessor<f32, f32>(tinymodel, sampler.input, 0);
        std::vector<T>   frame_values = extract_accessor<T, f32>(tinymodel, sampler.output, 0);

        //printf("  time: [");
        //for (u32 n = 0; n < frame_times.size(); n++) {
        //    printf("%.2f ", frame_times[n]);
        //}
        //printf("]\n  vals: [");
        //for (u32 n = 0; n < frame_times.size(); n++) {
        //    if (frame_times.size() == frame_values.size())
        //        laml::print(frame_values[n], "%.2f");
        //    else if (3*frame_times.size() == frame_values.size())
        //        laml::print(frame_values[3*n+1], "%.2f");
        //    printf(", ");
        //}
        //printf("]\n");
        

        const f32 start_time = frame_times[0]; // offset in case the animation doesn't start at 0
        const f32 end_time = frame_times[frame_times.size()-1];
        const f32 sample_frame_time = 1.0f/sample_frame_rate;

        std::vector<T> sampled_frames;

        f32 sample_time = start_time;
        u32 frame_num = 0;
        u32 start = 1;
        while(sample_time < end_time) {
            // interpolate sample_time on the frame_times vector
            f32 left_time;
            f32 right_time;
            f32 partial = 0.0f;
            u32 left_idx = 0; 
            u32 right_idx = 0;

            for (u32 n = start; n < frame_times.size(); n++) {
                left_time = frame_times[n-1];
                right_time = frame_times[n];
                left_idx = n-1;
                right_idx = n;

                if (sample_time >= left_time && sample_time < right_time) {
                    // find the partial time
                    partial = (sample_time - left_time) / (right_time - left_time); // [0,1] range
                    start = n;
                    break;
                }
            }

            T left_value  = frame_values[left_idx];
            T right_value = frame_values[right_idx];

            T value;
            if (sampler.interpolation == "STEP") {
                value = left_value;
            } else if (sampler.interpolation == "LINEAR") {
                value = interpolater(left_value, right_value, partial);
            } else if (sampler.interpolation == "CUBICSPLINE") {
                // need to do something different here.
                // there is now 3 frame_values per frame_time (in-tangent, value, out-tangent)
                f32 delta_time = right_time - left_time;
                //https://www.desmos.com/calculator/p6edwlosbx

                left_value  = frame_values[left_idx*3 + 1];
                right_value = frame_values[right_idx*3 + 1];
                T left_out_tangent = frame_values[left_idx*3 + 2];
                T right_in_tangent = frame_values[right_idx*3 + 2];

                f32 t = partial;
                f32 t2 = partial * partial;
                f32 t3 = t2 * partial;

                value = left_value * (2*t3 - 3*t2 + 1) + 
                        left_out_tangent * ((t3 - 2*t2 + t) * delta_time) +
                        right_value * (-2*t3 + 3*t2) + 
                        right_in_tangent * ((t3 - t2) * delta_time);

                if (should_normalize)
                    value = laml::normalize(value);
            }

            sampled_frames.push_back(value);

            frame_num++;
            sample_time += sample_frame_time;
        }

        assert(frame_num == sampled_frames.size());

        return sampled_frames;
    }

    bool animation_matches_mesh(const tinygltf::Animation& tinyanim, const MeshData& mesh) {
        if (mesh.has_skeleton) {
            for (u32 b = 0; b < mesh.bind_pose.num_bones; b++) {
                if (tinyanim.channels[0].target_node == mesh.bind_pose.bones[b].node_idx) {
                    return true;
                }
            }
        }

        return false;
    }

    void extract_animations(const tinygltf::Model& tinymodel, std::vector<MeshData>& meshes, f32 sample_frame_rate) {
        int num_anims = tinymodel.animations.size();
        log_print(0, "%d animations\n", num_anims);
        log_print(0, " Sampling at %.2f fps\n", sample_frame_rate);
        for (u32 n = 0; n < num_anims; n++) {
            const tinygltf::Animation& tinyanim = tinymodel.animations[n];

            u32 mesh_id = ~0;
            for (u32 m = 0; m < meshes.size(); m++) {
                if (animation_matches_mesh(tinyanim, meshes[m])) {
                    mesh_id = m;
                    break;
                }
            }
            if (mesh_id == ~0) {
                printf("ERROR!\n");
                return;
            }

            MeshData& mesh = meshes[mesh_id];

            rh::Animation& anim = mesh.m_anims.emplace_back();
            anim.name = tinyanim.name;
            anim.num_nodes = mesh.bind_pose.num_bones;
            anim.nodes.resize(anim.num_nodes);

            // kinda hacky, hopefully channel 0 is representative of the whole animation!
            const tinygltf::AnimationChannel& chan0 = tinyanim.channels[0];
            std::vector<f32> chan0time = extract_accessor<f32, f32>(tinymodel, tinyanim.samplers[chan0.sampler].input, 0);
            anim.duration = chan0time[chan0time.size()-1] - chan0time[0];
            anim.frame_rate = sample_frame_rate;

            printf(" Animation [%s]:\n", tinyanim.name.c_str());
            printf("  %d channels\n", (int)tinyanim.channels.size());
            printf("  Duration: %.2f\n", anim.duration);
            for (u32 c = 0; c < tinyanim.channels.size(); c++) {
                const tinygltf::AnimationChannel& channel = tinyanim.channels[c];
                const tinygltf::AnimationSampler& sampler = tinyanim.samplers[channel.sampler];
                
                // find the bone that corresponds to this node
                u32 bone_idx = ~0;
                for (u32 nb = 0; nb < mesh.bind_pose.num_bones; nb++) {
                    if (mesh.bind_pose.bones[nb].node_idx == channel.target_node) {
                        bone_idx = nb;
                        break;
                    }
                }
                printf("    Node #%d|Bone #%d ", channel.target_node, bone_idx);

                if (bone_idx == ~0)
                    continue;

                AnimNode& node = anim.nodes[bone_idx];

                if (channel.target_path == "translation") {
                    printf("Translation channel:\n");
                    node.translations = sample_anim_channel<laml::Vec3>(tinymodel, channel, sampler, anim.frame_rate, false,
                        [](const laml::Vec3& v1, const laml::Vec3& v2, f32 f) { return laml::lerp(v1, v2, f); });

                    if (anim.num_samples == 0) {
                        anim.num_samples = node.translations.size();
                    } else {
                        printf("=======num_samples %d  =======size() = %d\n", anim.num_samples, node.translations.size());
                        assert(anim.num_samples == node.translations.size());
                    }
                }
                else if (channel.target_path == "rotation") {
                    printf("Rotation channel:\n");
                    node.rotations = sample_anim_channel<laml::Quat>(tinymodel, channel, sampler, anim.frame_rate, true,
                        [](const laml::Quat& q1, const laml::Quat& q2, f32 f) { return laml::slerp(q1, q2, f); });

                    if (anim.num_samples == 0) {
                        anim.num_samples = node.rotations.size();
                    } else {
                        assert(anim.num_samples == node.rotations.size());
                    }
                }
                else if (channel.target_path == "scale") {
                    printf("Scale channel:\n");
                    node.scales = sample_anim_channel<laml::Vec3>(tinymodel, channel, sampler, anim.frame_rate, false,
                        [](const laml::Vec3& v1, const laml::Vec3& v2, f32 f) { return laml::lerp(v1, v2, f); });

                    if (anim.num_samples == 0) {
                        anim.num_samples = node.scales.size();
                    } else {
                        assert(anim.num_samples == node.scales.size());
                    }
                }
            }

            anim.duration = static_cast<f32>(anim.num_samples) / anim.frame_rate;
        }
    }

    void MeshConverter::LoadInputFile(const char* filename) {
        printf("----------------Loading------------------\n");
        printf("Loading file: '%s'\n", filename);

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;
        
        std::string rf, fn, ext;
        utils::decompose_path(filename, rf, fn, ext);
        bool ret = false;
        if (ext == ".glb") {
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
        } else if (ext == ".gltf") {
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
        } else {
            printf("Unknown file extension: [%s]\n", ext.c_str());
            return;
        }

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

        for (int n = 0; n < model.scenes.size(); n++) {
            printf("Scene: %s\n", model.scenes[0].name.c_str());
            traverse_nodes(model, model.nodes[scene.nodes[0]], m_meshes, laml::Mat4(1.0f), 1);
        }
        printf("-----------------------------------------\n");
        printf("Extracted %d meshes\n", (int)m_meshes.size());
        for (u32 n = 0; n < m_meshes.size(); n++) {
            printf(" - %s [%s] [%d/%d/%d]\n",
                   m_meshes[n].mesh_name.c_str(), m_meshes[n].has_skeleton ? "Skinned" : "Static",
                   m_meshes[n].num_submeshes, m_meshes[n].num_verts, m_meshes[n].num_inds);
        }
        printf("-----------------------------------------\n");
        extract_animations(model, m_meshes, m_sample_frame_rate);
        printf("Extracted %d animations!\n", (int)m_meshes[0].m_anims.size());

        m_valid = true;
    }

    void MeshConverter::ProcessMeshes() {
        if (!m_valid) return;
        printf("---------------Processing----------------\n");
        for (u32 m = 0; m < m_meshes.size(); m++) {
            const MeshData& mesh = m_meshes[m];

            for (u32 n = 0; n < mesh.m_anims.size(); n++) {
                const Animation& anim = mesh.m_anims[n];

                printf("Animation: %s\n", anim.name.c_str());
                printf("  %d nodes\n", anim.num_nodes);
                printf("  %d frames\n", anim.num_samples);
                printf("  %.3f duration\n", anim.duration);
                printf("  %.3f frame_rate\n", anim.frame_rate);
                printf("  %.3f sec\n", anim.num_samples / anim.frame_rate);

                // fill empty animation channels
                printf("  Validating animation:\n");
                for (u32 bone_idx = 0; bone_idx < anim.num_nodes; bone_idx++) {
                    AnimNode& bone = m_meshes[m].m_anims[n].nodes[bone_idx];

                    if (bone.translations.size() == 0) {
                        printf("  No translation animation on node %i\n", bone_idx);
                        bone.translations.resize(anim.num_samples);
                        // default constructor is zero-vectors
                    }
                    else {
                        assert(bone.translations.size() == anim.num_samples);
                    }

                    if (bone.rotations.size() == 0) {
                        printf("  No rotation animation on node %i\n", bone_idx);
                        bone.rotations.resize(anim.num_samples);
                        for (u32 j = 0; j < anim.num_samples; j++) {
                            bone.rotations[j] = laml::Quat(0.0f, 0.0f, 0.0f, 1.0f);
                        }
                    } else {
                        assert(bone.rotations.size() == anim.num_samples);
                    }

                    if (bone.scales.size() == 0) {
                        printf("  No scale animation on node %i\n", bone_idx);
                        bone.scales.resize(anim.num_samples);
                        for (u32 j = 0; j < anim.num_samples; j++) {
                            bone.scales[j] = laml::Vec3(1.0f, 1.0f, 1.0f);
                        }
                    } else {
                        assert(bone.scales.size() == anim.num_samples);
                    }

                    //for (u32 i = 0; i < anim.num_samples; i++) {
                    //}
                }
            }
        }
    }


    // Output funcs
    void writeMeshFile(const std::string& filename, const MeshData& mesh);
    void writeAnimFiles(const std::string& filename, const MeshData& mesh);

    void MeshConverter::SaveOutputFiles(const char* path) {
        if (!m_valid) return;
        printf("--------------Saving Files---------------\n");

        int num_meshes = m_meshes.size();
        if (num_meshes == 1) {
            printf("1 mesh extracted from this file!\n");
            writeMeshFile(path, m_meshes[0]);
        } else {
            std::string root_folder, filename, extension;
            utils::decompose_path(path, root_folder, filename, extension);

            printf("There are %d meshes extracted from this file!\n", num_meshes);
            printf("Appending mesh_name to the filename provided!\n");
            for (u32 n = 0; n < num_meshes; n++) {
                const MeshData& mesh = m_meshes[n];
            
                printf("\n  Mesh #%d\n", n + 1);
                std::string mesh_filename = root_folder + filename + "-" + mesh.mesh_name + extension;
                writeMeshFile(mesh_filename, mesh);
            }
        }
        printf("-----------------------------------------\n");
    }

    void writeMeshFile(const std::string& filename, const MeshData& mesh) {
        // Open and check for valid file
        FILE* fid = nullptr;
        errno_t err = fopen_s(&fid, filename.c_str(), "wb");
        if (fid == nullptr || err) {
            std::cout << "Failed to open output file '" << filename << "'..." << std::endl;
            return;
        }
        
        printf("  Writing to '%s'\n", filename.c_str());
        
        // construct options flag
        u32 flag = 0;
        const u32 flag_has_animations = 0x01; // 1
        
        if (mesh.has_skeleton)
            flag |= flag_has_animations;
        
        printf("  flag = %d\n", flag);
        
        // Write to file
        u32 filesize_write = 0;
        size_t FILESIZE = 0; // will get updated later
        FILESIZE += fwrite("MESH", 1, 4, fid);
        FILESIZE += fwrite(&filesize_write, sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite(&flag, sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite("INFO", 1, 4, fid); // start mesh info section
        
        FILESIZE += fwrite(&mesh.num_verts,   sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite(&mesh.num_inds,    sizeof(u32), 1, fid) * sizeof(u32);
        FILESIZE += fwrite(&mesh.num_submeshes, sizeof(u16), 1, fid) * sizeof(u16);
        
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
        sprintf_s(comment, 100, "Mesh file generated on [%s] by mesh_conv version %s  ", date_str, Version());
        u16 comment_len = static_cast<u16>(strlen(comment) + 1);
        FILESIZE += fwrite(&comment_len, sizeof(u16), 1, fid) * sizeof(u16);
        FILESIZE += fwrite(comment, 1, comment_len-1, fid);
        FILESIZE += (fputc(0, fid) == 0);
        
        printf("  Date string: [%s]\n", date_str);
        printf("  Comment: [%s]\n", comment);
        
        for (u32 n = 0; n < mesh.num_submeshes; n++) {
            FILESIZE += fwrite("SUBMESH", 1, 7, fid);
            FILESIZE += (fputc(0, fid) == 0);
        
            const auto& submesh = mesh.submeshes[n];
            FILESIZE += fwrite(&submesh.start_index,  sizeof(u32), 1,  fid) * sizeof(u32);
            FILESIZE += fwrite(&submesh.mat_index,    sizeof(u32), 1,  fid) * sizeof(u32);
            FILESIZE += fwrite(&submesh.num_indices,  sizeof(u32), 1,  fid) * sizeof(u32);
            FILESIZE += fwrite(&submesh.local_matrix, sizeof(f32), 16, fid) * sizeof(f32);
        }
        
        // Write joint heirarchy
        if (mesh.has_skeleton) {
            FILESIZE += fwrite("BONE", 1, 4, fid);
            const auto& skeleton = mesh.bind_pose;
        
            u16 num_bones = static_cast<u16>(skeleton.num_bones);
            FILESIZE += fwrite(&num_bones, sizeof(u16), 1, fid) * sizeof(u16);
        
            for (u32 n = 0; n < num_bones; n++) {
                const auto& bone = mesh.bind_pose.bones[n];
        
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
        for (u32 n = 0; n < mesh.num_inds; n++) {
            FILESIZE += fwrite(&mesh.indices[n], sizeof(u32), 1, fid) * sizeof(u32);
        }
        
        // Vertex block
        FILESIZE += fwrite("VERT", 1, 4, fid);
        for (u32 n = 0; n < mesh.num_verts; n++) {
            const auto& vert = mesh.vertices[n];
            FILESIZE += fwrite(&vert.position.x,  sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.normal.x,    sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.tangent.x,   sizeof(f32), 3, fid) * sizeof(f32);
            FILESIZE += fwrite(&vert.bitangent.x, sizeof(f32), 3, fid) * sizeof(f32);

            f32 y = 1.0f - vert.tex.y;
            FILESIZE += fwrite(&vert.tex.x, sizeof(f32), 1, fid) * sizeof(f32);
            FILESIZE += fwrite(&y, sizeof(f32), 1, fid) * sizeof(f32);
            //FILESIZE += fwrite(&vert.tex.x,       sizeof(f32), 2, fid) * sizeof(f32);
        
            if (mesh.has_skeleton) {
                FILESIZE += fwrite(&vert.bone_indices.x, sizeof(s32), MAX_BONES_PER_VERTEX, fid) * sizeof(s32);
                FILESIZE += fwrite(&vert.bone_weights.x, sizeof(f32), MAX_BONES_PER_VERTEX, fid) * sizeof(f32);
            }
        }
        
        // Write animation names
        if (mesh.has_skeleton) {
            printf("  Contains %d animations...\n", 0);
            FILESIZE += fwrite("ANIM", 1, 4, fid);
            u16 num_anims = static_cast<u16>(mesh.m_anims.size());
            FILESIZE += fwrite(&num_anims, sizeof(u16), 1, fid) * sizeof(u16);
        
            const Skeleton& skeleton = mesh.bind_pose;
        
            for (int n_anim = 0; n_anim < num_anims; n_anim++) {
                const Animation& anim = mesh.m_anims[n_anim];
                std::string anim_name = std::string(anim.name.c_str());
                
                std::replace(anim_name.begin(), anim_name.end(), '|', '-');
                
                u16 name_len = static_cast<u16>(anim_name.size() + 1);
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
        
        // Write animation files
        if (mesh.has_skeleton) {
            writeAnimFiles(filename, mesh);
        }

        printf("\n");
    }

    void writeAnimFiles(const std::string& filename, const MeshData& mesh) {        
        u16 num_anims = static_cast<u16>(mesh.m_anims.size());
        
        if (num_anims > 0)
            printf("  Writing %d animations to file\n", (int)num_anims);

        std::string root_path(filename);
        root_path = root_path.substr(0, root_path.find_last_of("."));
        root_path += "_";
        
        const Skeleton& skeleton = mesh.bind_pose;
        
        for (int n = 0; n < num_anims; n++) {
            const Animation& anim = mesh.m_anims[n];
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
             * u32 num_samples;
            */
        
            FILESIZE += fwrite(&anim.duration,    sizeof(f32), 1, fid) * sizeof(f32);
            FILESIZE += fwrite(&anim.frame_rate,  sizeof(f32), 1, fid) * sizeof(f32);
            FILESIZE += fwrite(&anim.num_nodes,   sizeof(u32), 1, fid) * sizeof(u32);
            FILESIZE += fwrite(&anim.num_samples, sizeof(u32), 1, fid) * sizeof(u32);
        
            FILESIZE += fwrite("DATA", 1, 4, fid);
            for (u32 node_idx = 0; node_idx < anim.num_nodes; node_idx++) {
                const AnimNode& node = anim.nodes[node_idx];

                FILESIZE += fwrite(&node.flag, sizeof(u32), 1, fid) * sizeof(u32);
                
                for (u32 n = 0; n < anim.num_samples; n++) {
                    FILESIZE += fwrite(&node.translations[n].x, sizeof(f32), 3, fid) * sizeof(f32);
                }

                for (u32 n = 0; n < anim.num_samples; n++) {
                    FILESIZE += fwrite(&node.rotations[n].x, sizeof(f32), 4, fid) * sizeof(f32);
                }

                for (u32 n = 0; n < anim.num_samples; n++) {
                    FILESIZE += fwrite(&node.scales[n].x,  sizeof(f32), 3, fid) * sizeof(f32);
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