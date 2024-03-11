#include "mesh_converter.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tinygltf/tiny_gltf.h"

#include <unordered_set>
#include <time.h>       /* time_t, struct tm, difftime, time, mktime */

// windows specific
#include <direct.h>

/****************************************
 * 
 *   MESH
 * 
 * ************************************/
struct Mesh_Primitive {
    int32 material_index;
    std::vector<uint32> indices;

    std::vector<laml::Vec3> positions;
    std::vector<laml::Vec3> normals;
    std::vector<laml::Vec2> texcoords;
    std::vector<laml::Vec4> tangents_4;

    std::vector<laml::Vec4> bone_weights;
    std::vector<laml::Vector<int32, 4>> bone_indices;
};
struct Bone {
    int32 parent_idx; // so -1 can be the root idx
    laml::Mat4 local_matrix;
    laml::Mat4 inv_model_matrix;

    std::string name;
    uint32 node_idx;
    uint32 bone_idx;
};
struct Skeleton {
    std::vector<Bone> bones;
};
struct Mesh {
    laml::Mat4 transform;

    bool32 is_rigged;
    bool32 is_collider;

    std::string mesh_name;
    std::string name;

    std::vector<Mesh_Primitive> primitives;

    Skeleton skeleton;
};

/****************************************
* 
*   MATERIAL
* 
* ************************************/
struct Material {
    std::string name;
    bool32 double_sided;

    laml::Vec3 diffuse_factor; // vec4 instead? need different strategy for transparency entirely...
    std::string diffuse_texture;
    bool32 diffuse_has_texture;

    real32 normal_scale;
    std::string normal_texture;
    bool32 normal_has_texture;


    real32 ambient_strength;
    real32 metallic_factor;
    real32 roughness_factor;
    std::string amr_texture;
    bool32 amr_has_texture;

    laml::Vec3 emissive_factor;
    std::string emissive_texture;
    bool32 emissive_has_texture;
};

bool has_attribute(const tinygltf::Primitive& prim, std::string attr_name) {
    if (prim.attributes.find(attr_name) == prim.attributes.end())
        return false; 

    return true;
}

const unsigned char* read_buffer_view(const tinygltf::Model& gltf_model, int buffer_view_idx) {
    const tinygltf::BufferView bufferView = gltf_model.bufferViews[buffer_view_idx];

    //bufferView.buffer;
    //bufferView.byteOffset;
    //bufferView.byteLength;
    //bufferView.byteStride;

    const tinygltf::Buffer& buffer = gltf_model.buffers[bufferView.buffer];
    return (buffer.data.data() + bufferView.byteOffset);
}

template<typename Component_Type>
Component_Type read_component(int componentType, const uint8* cur_element) {
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

    //Component_Type ignore_this_warning;
    return 0;
}

template<typename Element_Type, typename Component_Type>
std::vector<Element_Type> extract_accessor(const tinygltf::Model& tinyModel, int accessor_idx, int level) {
    assert(accessor_idx >= 0);

    const tinygltf::Accessor& accessor = tinyModel.accessors[accessor_idx];
    if (accessor.sparse.isSparse) {
        level_print(level, "Sparse accessor not supported yet x.x\n");
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

    const uint8* raw_data = read_buffer_view(tinyModel, accessor.bufferView); // ptr to start of byte stream for this accessor
    const uint8* cur_element = raw_data; // our current cursor
    const uint8* cur_comp = cur_element;

    std::vector<Element_Type> elements;
    for (uint32 n = 0; n < accessor.count; n++) {
        // for each type (SCALAR, VECn, MATn, etc..)
        cur_element = raw_data + (n * num_bytes_per_Element);
        Element_Type element;

        for (uint32 i = 0; i < num_components; i++) {
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

void process_mesh(const tinygltf::Model& gltf_model, const tinygltf::Mesh& gltf_mesh, Mesh& mesh, bool has_skin, int level) {
    int num_primitives = gltf_mesh.primitives.size();
    mesh.primitives.resize(num_primitives);
    mesh.is_rigged = has_skin;
    mesh.mesh_name = gltf_mesh.name;

    tinygltf::Value extras = gltf_mesh.extras;
    bool is_collider = false;
    if (extras.IsObject()) {
        if (extras.Has("collision_obj")) {
            auto has_collision = extras.Get("collision_obj");
            if (has_collision.IsBool()) {
                is_collider = has_collision.Get<bool>();
            }
        }
    }

    if (is_collider) {
        level_print(level, "    has 'collision' tag\n");
    }
    mesh.is_collider = is_collider;

    for (int n = 0; n < num_primitives; n++) {
        tinygltf::Primitive prim = gltf_mesh.primitives[n];
        assert(prim.mode == TINYGLTF_MODE_TRIANGLES);

        //for (const auto& key : prim.attributes) {
        //    printf("->'%s'\n", key.first.c_str());
        //}

        // determine missing attributes
        if (!has_attribute(prim, "POSITION")) {
            printf("[ERROR]  primitive is missing vertex positions!!!\n");
            assert(false);
        }
        if (!has_attribute(prim, "NORMAL")) {
            printf("[WARNING]  primitive is missing normals\n");
            assert(false);
        }
        if (!has_attribute(prim, "TANGENT")) {
            printf("[WARNING]  primitive is missing tangents\n");
            assert(false);
        }
        if (!has_attribute(prim, "TEXCOORD_0")) {
            printf("[WARNING]  primitive is missing uv-coords\n");
            assert(false);
        }

        mesh.primitives[n].material_index = prim.material;

        //log_print(level, " Extracting mesh indices!\n");
        mesh.primitives[n].indices = extract_accessor<uint32, uint32>(gltf_model, prim.indices, level + 1);

        //log_print(level, " Extracting static mesh attributes!\n");
        mesh.primitives[n].positions = extract_accessor<laml::Vec3, real32>(gltf_model, prim.attributes["POSITION"],   level + 1);
            
        //mesh.primitives[n].normals.resize(mesh.primitives[n].positions.size());
        //mesh.primitives[n].tangents.resize(mesh.primitives[n].positions.size());
        //mesh.primitives[n].texcoords.resize(mesh.primitives[n].positions.size());

        mesh.primitives[n].normals = extract_accessor<laml::Vec3, real32>(gltf_model, prim.attributes["NORMAL"],     level + 1);
        mesh.primitives[n].tangents_4 = extract_accessor<laml::Vec4, real32>(gltf_model, prim.attributes["TANGENT"],    level + 1);
        mesh.primitives[n].texcoords = extract_accessor<laml::Vec2, real32>(gltf_model, prim.attributes["TEXCOORD_0"], level + 1);

        // check for skinning data if skinned
        if (has_skin) {
            // determine missing attributes
            if (!has_attribute(prim, "JOINTS_0")) {
                printf("[WARNING]  primitive is missing bone indices!!!\n");
                assert(false);
            }
            if (!has_attribute(prim, "WEIGHTS_0")) {
                printf("[ERROR]  primitive is missing bone weights!!!\n");
                assert(false);
            }

            mesh.primitives[n].bone_weights = extract_accessor<laml::Vec4, real32>(gltf_model, prim.attributes["WEIGHTS_0"], level + 1);
            mesh.primitives[n].bone_indices = extract_accessor<laml::Vector<int32,4>, int32>(gltf_model, prim.attributes["JOINTS_0"], level + 1);
        }
    }
}





std::string get_gltf_texture_name(const tinygltf::Model& gltf_model, int texture_index, bool32& has_texture) {
    if (texture_index == -1) {
        // no texture
        has_texture = false;
        return std::string();
    }

    // else, lookup texture filename from gltf_model
    has_texture = true;
    const tinygltf::Texture& gltf_texture = gltf_model.textures[texture_index];
    const tinygltf::Image& gltf_image = gltf_model.images[gltf_texture.source];
    std::string tex_name = gltf_image.name;
    std::string ext = utils::mime_type_to_ext(gltf_image.mimeType);

    return (tex_name + "." + ext);
}

std::string merge_ambient_and_metallic_roughness(const tinygltf::Model& gltf_model, 
                                                 int ambient_index, int mr_index, 
                                                 bool32& has_texture) {
    bool32 has_ambient_texture;
    std::string ambient_texture;
    ambient_texture = get_gltf_texture_name(gltf_model, ambient_index, has_ambient_texture);

    bool32 has_mr_texture;
    std::string mr_texture;
    mr_texture = get_gltf_texture_name(gltf_model, mr_index, has_mr_texture);

    // easiest situation: neither have a texture!
    if ((!has_ambient_texture) && (!has_mr_texture)) {
        has_texture = false;
        return std::string();
    }

    has_texture = true; // ONE of them at least has a texture

    // next easiest: both have the SAME texture!
    if ((has_ambient_texture && has_mr_texture) && (ambient_texture.compare(mr_texture) == 0)) {
        return ambient_texture;
    }

    // check if only 1 has a texture:
    if (has_ambient_texture && (!has_mr_texture)) {
        return ambient_texture;
    }
    if (has_mr_texture && (!has_ambient_texture)) {
        return mr_texture;
    }

    // otherwise: need to merge the two textures into 1
    printf("[WARNING] Material contains different textures for Ambient and MetallicRoughness! Need to merge into a single AMR file.\n");
    printf("[ERROR] Not supported right now...\n");
    has_texture = false;
    return std::string();
}

void process_material(const tinygltf::Model& gltf_model, const tinygltf::Material& gltf_mat, Material& material) {
    material.name = gltf_mat.name;
    material.double_sided = gltf_mat.doubleSided;

    const tinygltf::PbrMetallicRoughness pbr = gltf_mat.pbrMetallicRoughness;
    material.diffuse_factor = utils::map_gltf_vec_to_vec3(pbr.baseColorFactor);
    material.diffuse_texture = get_gltf_texture_name(gltf_model, pbr.baseColorTexture.index, material.diffuse_has_texture);

    material.normal_scale = gltf_mat.normalTexture.scale;
    material.normal_texture = get_gltf_texture_name(gltf_model, gltf_mat.normalTexture.index, material.normal_has_texture);

    material.ambient_strength = gltf_mat.occlusionTexture.strength;
    material.metallic_factor = pbr.metallicFactor;
    material.roughness_factor = pbr.roughnessFactor;
    material.amr_texture = merge_ambient_and_metallic_roughness(gltf_model, gltf_mat.occlusionTexture.index, 
                                                                pbr.metallicRoughnessTexture.index, material.amr_has_texture);

    material.emissive_factor = utils::map_gltf_vec_to_vec3(gltf_mat.emissiveFactor);
    material.emissive_texture = get_gltf_texture_name(gltf_model, gltf_mat.emissiveTexture.index, material.emissive_has_texture);

    printf("Material: '%s'\n", gltf_mat.name.c_str());
    if (material.diffuse_has_texture)  printf("  Diffuse Texture:  '%s'\n", material.diffuse_texture.c_str());
    if (material.normal_has_texture)   printf("  Normal Texture:   '%s'\n", material.normal_texture.c_str());
    if (material.amr_has_texture)      printf("  A/M/R Texture:    '%s'\n", material.amr_texture.c_str());
    if (material.emissive_has_texture) printf("  Emissive Texture: '%s'\n", material.emissive_texture.c_str());
}







laml::Mat4 get_node_local_transform(const tinygltf::Node& node) {
    /*
    * tinygltf stores these transforms as doubles, so need to cast them down component-wise
    */

    // if the node has the matrix outright
    if (node.matrix.size() == 16) {
        laml::Mat4 transform;
        for (uint32 col = 0; col < 4; col++) {
            for (uint32 row = 0; row < 4; row++) {
                uint32 n = (col*4) + row;
                transform[col][row] = static_cast<real32>(node.matrix[n]);
            }
        }
        return transform;
    }

    // otherwise constuct it from /possible/ components
    laml::Vec3 translation(0.0f);
    laml::Vec3 scale(1.0f);
    laml::Quat rotation(0.0f, 0.0f, 0.0f, 1.0f);
    if (node.translation.size() == 3) {
        for (uint32 n = 0; n < 3; n++) {
            translation[n] = static_cast<real32>(node.translation[n]);
        }
    }
    if (node.rotation.size() == 4) {
        for (uint32 n = 0; n < 4; n++) {
            rotation[n] = static_cast<real32>(node.rotation[n]);
        }
    }
    if (node.scale.size() == 3) {
        for (uint32 n = 0; n < 3; n++) {
            scale[n] = static_cast<real32>(node.scale[n]);
        }
    }

    laml::Mat4 transform(1.0f);
    laml::transform::create_transform(transform, rotation, translation, scale);
    return transform;
}

void traverse_bones(const tinygltf::Model& gltf_model,
                    const tinygltf::Node& gltf_joint,
                    std::vector<Bone>& out_bones,
                    laml::Mat4& parent_transform,
                    int32 parent_idx,
                    uint32 node_idx,
                    int level) {

    uint32 num_children = gltf_joint.children.size();
    //level_print(level, "[%s] (%d children)\n", gltf_joint.name.c_str(), num_children);
    
    Bone new_bone;
    new_bone.name = gltf_joint.name;
    new_bone.local_matrix = get_node_local_transform(gltf_joint);
    laml::Mat4 model_matrix = laml::mul(parent_transform, new_bone.local_matrix);
    new_bone.inv_model_matrix = laml::inverse(model_matrix);
    new_bone.parent_idx = parent_idx;

    int32 bone_idx = out_bones.size();
    assert((parent_idx < bone_idx) && "Bone processed before its parent!");
    new_bone.bone_idx = bone_idx;
    new_bone.node_idx = node_idx;
    out_bones.push_back(new_bone);


    for (uint32 n = 0; n < num_children; n++) {
        const tinygltf::Node& child = gltf_model.nodes[gltf_joint.children[n]];

        uint32 child_idx = gltf_joint.children[n];
        traverse_bones(gltf_model, child, out_bones, model_matrix, bone_idx, child_idx, level + 1);
    }
}

void extract_bind_pose(const tinygltf::Model& gltf_model, const tinygltf::Skin& gltf_skin, Mesh& mesh, int level) {
    std::vector<Bone> bones;
    const tinygltf::Node& root_joint = gltf_model.nodes[gltf_skin.joints[0]];
    if (root_joint.name != "root") {
        level_print(level, "[WARNING] root joint [%s] is not named 'root'\n", root_joint.name.c_str());
    }
    traverse_bones(gltf_model, root_joint, bones, laml::Mat4(1.0f), -1, gltf_skin.joints[0], level + 1);

    uint32 num_joints = gltf_skin.joints.size();
    level_print(level+1, "Found %d/%d bones!\n", bones.size(), num_joints);

    mesh.skeleton.bones = bones;
}

void traverse_nodes(const tinygltf::Model& gltf_model, 
                    const tinygltf::Node& gltf_node, 
                    std::vector<Mesh>& out_meshes, 
                    laml::Mat4& parent_transform, 
                    int level) {

    level_print(level, "Node: '%s'\n", gltf_node.name.c_str());

    bool has_camera = gltf_node.camera >= 0; // unused
    bool has_mesh = gltf_node.mesh >= 0;
    bool has_skin = gltf_node.skin >= 0;

    laml::Mat4 node_local_transform = get_node_local_transform(gltf_node);
    laml::Mat4 node_world_transform = laml::mul(parent_transform, node_local_transform);
        
    //log_print(level, "Local_Transform: ");
    //laml::print((node_local_transform), "%.2f");
    //printf("\n");
    //log_print(level, "Model_Transform: ");
    //laml::print((node_model_transform), "%.2f");
    //printf("\n");

    if (has_mesh) {
        level_print(level + 1, "%s Mesh: '%s'\n", has_skin ? "Animated" : "Static", gltf_model.meshes[gltf_node.mesh].name.c_str());
        Mesh mesh;
        //mesh.local_matrix = node_local_transform;
        mesh.transform = node_world_transform;
        mesh.name = gltf_node.name;
        process_mesh(gltf_model, gltf_model.meshes[gltf_node.mesh], mesh, has_skin, level + 1);

        if (has_skin) {
            level_print(level + 1, "NOT SUPPORTED RIGHT NOW!!\n");

            const tinygltf::Skin& gltf_skin = gltf_model.skins[gltf_node.skin];
            level_print(level + 1, "Skeleton: '%s' (%d bones)\n", gltf_skin.name.c_str(), gltf_skin.joints.size());

            extract_bind_pose(gltf_model, gltf_skin, mesh, level+1);
            //process_anim_mesh(gltf_model, gltf_model.meshes[gltf_node.mesh], mesh, level + 1);
            //Skeleton skeleton;
            //process_skin(gltf_model, gltf_model.skins[gltf_node.skin], skeleton, level + 1);
            //assign_skeleton(&mesh, &skeleton);
        }
        out_meshes.push_back(mesh);
    }

    //if (node.children.size() > 0) { level_print(level, " %d Children: \n", node.children.size()); }
    for (int n = 0; n < gltf_node.children.size(); n++) {
        // NOTE: this was passing in node_local_transform which doesnt make sense...
        //       could have been fine before since this path is separate than the path for skeletons
        //       need to confirm this is correct now.
        traverse_nodes(gltf_model, gltf_model.nodes[gltf_node.children[n]], out_meshes, node_world_transform, level + 1);
    }
}



bool32 write_mesh_file(const Mesh& mesh, 
                       const std::vector<Material>& materials, 
                       const std::string& root_folder,
                       const Options& opts);
bool32 write_level_file(const std::vector<Mesh>& meshes, 
                        const std::vector<Material>& materials, 
                        const std::string& root_folder);

bool convert_file(const Options& opts) {
    printf("----------------Loading------------------\n");
    printf("Loading file: '%s'\n", opts.input_filename.c_str());

    tinygltf::Model gltf_model;
    tinygltf::TinyGLTF gltf_loader;
    std::string err;
    std::string warn;
        
    std::string rf, fn, ext;
    utils::decompose_path(opts.input_filename, rf, fn, ext);
    printf("filename: %s\n", fn.c_str());
    bool ret = false;
    if (ext == ".glb") {
        ret = gltf_loader.LoadBinaryFromFile(&gltf_model, &err, &warn, opts.input_filename);
    } else if (ext == ".gltf") {
        ret = gltf_loader.LoadASCIIFromFile(&gltf_model, &err, &warn, opts.input_filename);
    } else {
        printf("Unknown file extension: [%s]\n", ext.c_str());
    }

    if (!warn.empty()) {
        printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
        printf("Failed to parse glTF\n");
        return false;
    }

    printf("%s file parsed.\n", ext.c_str());
    bool32 success = true;

    // Extract all meshes from the file
    std::vector<Mesh> extracted_meshes;
    for (int scene_idx = 0; scene_idx < gltf_model.scenes.size(); scene_idx++) {
        if (scene_idx > 0) {
            printf("[WARNING] Ignoring all scenes but the first!\n");
            break;
        }
        const tinygltf::Scene& scene = gltf_model.scenes[scene_idx];
        printf("Scene: %s\n", scene.name.c_str());

        // loop through all the top-level nodes
        for (int n = 0; n < scene.nodes.size(); n++) {
            int node_idx = scene.nodes[n];
            const tinygltf::Node& node = gltf_model.nodes[node_idx];
            traverse_nodes(gltf_model, node, extracted_meshes, laml::Mat4(1.0f), 1);
        }
        printf("Extracted %d meshes.\n", (int)extracted_meshes.size());
    }
    printf("-----------------------------------------\n");

    // Extract all materials from the file
    std::vector<Material> extracted_materials;
    for (int mat_idx = 0; mat_idx < gltf_model.materials.size(); mat_idx++) {
        const tinygltf::Material& gltf_mat = gltf_model.materials[mat_idx];

        Material new_mat;
        process_material(gltf_model, gltf_mat, new_mat);
        extracted_materials.push_back(new_mat);
    }
    printf("Extracted %d materials.\n", (int)extracted_materials.size());
    printf("-----------------------------------------\n");

    // Write each render mesh to its own file
    printf("Writing mesh files...\n");
    _mkdir(opts.output_folder.c_str());
    std::string mesh_folder = opts.output_folder;
    if (opts.mode == LEVEL_MODE) {
        mesh_folder = mesh_folder + '\\' + "render_meshes";
    }
    _mkdir(mesh_folder.c_str());
    std::unordered_set<std::string> written_meshes; // to catch duplicates
    for (int n = 0; n < extracted_meshes.size(); n++) {
        const Mesh& mesh = extracted_meshes[n];

        if (mesh.is_collider) continue;

        if (written_meshes.find(mesh.mesh_name) == written_meshes.end()) {
            printf("  Writing mesh %2d: '%s.mesh' [v%d]...", 1 + (int)written_meshes.size(), mesh.mesh_name.c_str(), MESH_VERSION);
            if (write_mesh_file(mesh, extracted_materials, mesh_folder, opts)) {
                printf("done!\n");
            } else {
                printf("failed!\n");
                success = false;
            }
            written_meshes.insert(mesh.mesh_name);
        }
    }
    printf("Wrote %d files.\n", (int)written_meshes.size());
    printf("-----------------------------------------\n");

    // Write collision meshes to files
    printf("Writing collider files...\n");
    std::string collision_folder = opts.output_folder;
    if (opts.mode == LEVEL_MODE) {
        collision_folder = collision_folder + '\\' + "collision_meshes";
    }
    _mkdir(collision_folder.c_str());
    written_meshes.clear(); // to catch duplicates
    for (int n = 0; n < extracted_meshes.size(); n++) {
        const Mesh& mesh = extracted_meshes[n];

        if (!mesh.is_collider) continue;

        if (written_meshes.find(mesh.mesh_name) == written_meshes.end()) {
            printf("  Writing mesh %2d: '%s.mesh' [v%d]...", 1 + (int)written_meshes.size(), mesh.mesh_name.c_str(), MESH_VERSION);
            if (write_mesh_file(mesh, extracted_materials, collision_folder, opts)) {
                printf("done!\n");
            }
            else {
                printf("failed!\n");
                success = false;
            }
            written_meshes.insert(mesh.mesh_name);
        }
    }
    printf("Wrote %d files.\n", (int)written_meshes.size());
    printf("-----------------------------------------\n");

    // Write mesh paths to level file
    if (opts.mode == LEVEL_MODE) {
        printf("Writing level file: '%s' [v%d]...", fn.c_str(), LEVEL_VERSION);
        if (write_level_file(extracted_meshes, extracted_materials, opts.output_folder + '\\' + fn)) {
            printf("done!\n");
        }
        else {
            printf("failed!\n");
            bool32 success = false;
        }
        printf("-----------------------------------------\n");
    }

    //printf("Checking transforms...\n");
    //for (int n = 0; n < extracted_meshes.size(); n++) {
    //    const Mesh& mesh = extracted_meshes[n];
    //
    //    laml::Mat3 rot_mat;
    //    laml::Vec3 trans, scale;
    //    laml::transform::decompose(mesh.transform, rot_mat, trans, scale);
    //    laml::Quat rot_quat = laml::transform::quat_from_mat(rot_mat);
    //
    //    // ypr
    //    real32 rot_pitch = asin(-rot_mat.c_23) * laml::constants::rad2deg<real32>;
    //    real32 rot_yaw = atan2(rot_mat.c_13, rot_mat.c_33) * laml::constants::rad2deg<real32>;
    //    real32 rot_roll = atan2(-rot_mat.c_21, rot_mat.c_22) * laml::constants::rad2deg<real32>;
    //
    //    printf("Object: '%s'\n", mesh.name.c_str());
    //    printf("  mesh: '%s'\n", mesh.mesh_name.c_str());
    //    printf("  position: <%.4f, %.4f, %.4f>\n", trans.x, trans.y, trans.z);
    //    printf("  scale: <%.4f, %.4f, %.4f>\n", scale.x, scale.y, scale.z);
    //    printf("  rot: <%.4f, %.4f, %.4f>\n", rot_pitch, rot_yaw, rot_roll);
    //}
    //printf("-----------------------------------------\n");

    return success;
}

size_t write_string(FILE* fid, const std::string& string) {
    uint8 len = string.length();
    size_t written = fwrite(&len, sizeof(uint8), 1, fid) * sizeof(uint8);
    written += fwrite(string.data(), sizeof(char), len, fid) * sizeof(char);
    return written;
}

bool32 write_mesh_file(const Mesh& mesh, 
    const std::vector<Material>& materials, 
    const std::string& mesh_folder, 
    const Options& opts) {

    std::string filename = mesh_folder + '\\' + mesh.mesh_name + ".mesh";

    // Open and check for valid file
    FILE* fid = nullptr;
    errno_t err = fopen_s(&fid, filename.c_str(), "wb");
    if (fid == nullptr || err) {
        //std::cout << "Failed to open output file '" << filename << "'..." << std::endl;
        return false;
    }

    uint16 num_prims = mesh.primitives.size();
    std::vector<uint32> mat_ids;
    mat_ids.resize(num_prims);
    for (int n = 0; n < num_prims; n++) {
        mat_ids[n] = mesh.primitives[n].material_index;
    }

    // construct options flag
    uint32 flag = 0;
        
    if (mesh.is_rigged)
        flag |= mesh_flag_is_rigged;

    uint64 timestamp = (uint64)time(NULL);

    // Write to file
    uint32 filesize_write = 0;
    size_t FILESIZE = 0; // will get updated later
    FILESIZE += fwrite("MESH", 1, 4, fid);
    FILESIZE += fwrite(&filesize_write, sizeof(uint32), 1, fid) * sizeof(uint32);
    FILESIZE += fwrite(&MESH_VERSION, sizeof(uint32), 1, fid) * sizeof(uint32);
    FILESIZE += fwrite(&flag, sizeof(uint32), 1, fid) * sizeof(uint32);
    FILESIZE += fwrite(&timestamp, sizeof(uint64), 1, fid) * sizeof(uint64);
    FILESIZE += fwrite(&num_prims, sizeof(uint16), 1, fid) * sizeof(uint16);
    FILESIZE += fwrite("\0\0\0\0\0\0", 1, 6, fid);

    // write materials
    for (int n = 0; n < num_prims; n++) {
        const Material& mat = materials[mat_ids[n]];
        
        uint32 mat_flag = 0;
        if (mat.double_sided)         mat_flag |= 0x01;
        if (mat.diffuse_has_texture)  mat_flag |= 0x02;
        if (mat.normal_has_texture)   mat_flag |= 0x04;
        if (mat.amr_has_texture)      mat_flag |= 0x08;
        if (mat.emissive_has_texture) mat_flag |= 0x10;

        FILESIZE += fwrite("MATL", 1, 4, fid);
        FILESIZE += fwrite(&mat_flag,             sizeof(uint32), 1, fid) * sizeof(uint32);
        FILESIZE += fwrite(&mat.diffuse_factor.x, sizeof(real32), 3, fid) * sizeof(real32);
        FILESIZE += fwrite(&mat.normal_scale,     sizeof(real32), 1, fid) * sizeof(real32);
        FILESIZE += fwrite(&mat.ambient_strength, sizeof(real32), 1, fid) * sizeof(real32);
        FILESIZE += fwrite(&mat.metallic_factor,  sizeof(real32), 1, fid) * sizeof(real32);
        FILESIZE += fwrite(&mat.roughness_factor, sizeof(real32), 1, fid) * sizeof(real32);
        FILESIZE += fwrite(&mat.emissive_factor,  sizeof(real32), 3, fid) * sizeof(real32);

        FILESIZE += write_string(fid, mat.name);

        if (mat.diffuse_has_texture)  FILESIZE += write_string(fid, mat.diffuse_texture);
        if (mat.normal_has_texture)   FILESIZE += write_string(fid, mat.normal_texture);
        if (mat.amr_has_texture)      FILESIZE += write_string(fid, mat.amr_texture);
        if (mat.emissive_has_texture) FILESIZE += write_string(fid, mat.emissive_texture);
    }

    // write vertex data
    for (int n = 0; n < num_prims; n++) {
        const Mesh_Primitive& prim = mesh.primitives[n];

        uint32 num_verts = prim.positions.size();
        uint32 num_inds = prim.indices.size();
        uint32 mat_idx = n;  //prim.material_index;

        FILESIZE += fwrite("PRIM", 1, 4, fid);
        FILESIZE += fwrite(&num_verts, sizeof(uint32), 1, fid) * sizeof(uint32);
        FILESIZE += fwrite(&num_inds,  sizeof(uint32), 1, fid) * sizeof(uint32);
        FILESIZE += fwrite(&mat_idx,   sizeof(uint32), 1, fid) * sizeof(uint32);

        // write indices
        for (int i = 0; i < num_inds; i++) {
            FILESIZE += fwrite(&prim.indices[i], sizeof(uint32), 1, fid) * sizeof(uint32);
        }

        // write vertices
        for (int i = 0; i < num_verts; i++) {
            FILESIZE += fwrite(&prim.positions[i].x, sizeof(real32), 3, fid) * sizeof(real32);
            FILESIZE += fwrite(&prim.normals[i].x,   sizeof(real32), 3, fid) * sizeof(real32);

            laml::Vec3 tangent = laml::Vec3(prim.tangents_4[i].x, prim.tangents_4[i].y, prim.tangents_4[i].z);
            laml::Vec3 bitangent = laml::cross(prim.normals[i], tangent) * prim.tangents_4[i].w;
            FILESIZE += fwrite(&tangent.x,   sizeof(real32), 3, fid) * sizeof(real32);
            FILESIZE += fwrite(&bitangent.x, sizeof(real32), 3, fid) * sizeof(real32);

            // flip y uv-coord
            real32 y;
            if (opts.flip_uvs_y) {
                y = 1.0f - prim.texcoords[i].y;
            } else {
                y = prim.texcoords[i].y;
            }
            FILESIZE += fwrite(&prim.texcoords[i].x, sizeof(real32), 1, fid) * sizeof(real32);
            FILESIZE += fwrite(&y,                   sizeof(real32), 1, fid) * sizeof(real32);
            //FILESIZE += fwrite(&vert.tex.x,       sizeof(f32), 2, fid) * sizeof(f32);

            // only write bone data if rigged
            if (mesh.is_rigged) {
                FILESIZE += fwrite(&prim.bone_indices[i].x, sizeof(int32),  4, fid) * sizeof(int32);
                FILESIZE += fwrite(&prim.bone_weights[i].x, sizeof(real32), 4, fid) * sizeof(real32);
            }
        }
    }

    // Write the skeleton if mesh is rigged
    // same skeleton for all prims?
    if (mesh.is_rigged) {
        const Skeleton& skeleton = mesh.skeleton;
        uint32 num_bones = skeleton.bones.size();

        FILESIZE += fwrite("SKEL", 1, 4, fid);
        FILESIZE += fwrite(&num_bones, sizeof(uint32), 1, fid) * sizeof(uint32);

        real32 debug_length = 1.0f;
        for (uint32 bone_idx = 0; bone_idx < num_bones; bone_idx++) {
            const Bone& bone = skeleton.bones[bone_idx];

            FILESIZE += fwrite(&bone.bone_idx, sizeof(uint32), 1, fid) * sizeof(uint32);
            FILESIZE += fwrite(&bone.parent_idx, sizeof(int32), 1, fid) * sizeof(int32);
            FILESIZE += fwrite(&debug_length, sizeof(real32), 1, fid) * sizeof(real32);
            FILESIZE += fwrite(&bone.local_matrix.c_11, sizeof(real32), 16, fid) * sizeof(real32);
            FILESIZE += fwrite(&bone.inv_model_matrix.c_11, sizeof(real32), 16, fid) * sizeof(real32);

            FILESIZE += write_string(fid, bone.name);
        }
    }

    FILESIZE += fwrite("END", 1, 3, fid);
    FILESIZE += (fputc(0, fid) == 0);

    fseek(fid, 4, SEEK_SET);
    filesize_write = static_cast<uint32>(FILESIZE);
    fwrite(&filesize_write, sizeof(uint32), 1, fid);
        
    printf(" [%i bytes] ", filesize_write);

    fclose(fid);

    return true;
}

bool32 write_level_file(const std::vector<Mesh>& meshes, const std::vector<Material>& materials, const std::string& root_folder) {
    std::string filename = root_folder + ".level";

    // Open and check for valid file
    FILE* fid = nullptr;
    errno_t err = fopen_s(&fid, filename.c_str(), "wb");
    if (fid == nullptr || err) {
        //std::cout << "Failed to open output file '" << filename << "'..." << std::endl;
        return false;
    }

    uint16 num_meshes = meshes.size();
    uint16 num_materials = materials.size();

    // construct options flag
    uint32 flag = 0;


    uint64 timestamp = (uint64)time(NULL);

    // Write to file
    uint32 filesize_write = 0;
    size_t FILESIZE = 0; // will get updated later
    FILESIZE += fwrite("LEVL", 1, 4, fid);
    FILESIZE += fwrite(&filesize_write, sizeof(uint32), 1, fid) * sizeof(uint32);
    FILESIZE += fwrite(&LEVEL_VERSION, sizeof(uint32), 1, fid) * sizeof(uint32);
    FILESIZE += fwrite(&flag, sizeof(uint32), 1, fid) * sizeof(uint32);
    FILESIZE += fwrite(&timestamp, sizeof(uint64), 1, fid) * sizeof(uint64);
    FILESIZE += fwrite(&num_meshes,    sizeof(uint16), 1, fid) * sizeof(uint16);
    FILESIZE += fwrite(&num_materials, sizeof(uint16), 1, fid) * sizeof(uint16);
    uint32 PADDING = 0;
    FILESIZE += fwrite(&PADDING, sizeof(uint32), 1, fid) * sizeof(uint32);

    for (int n = 0; n < num_meshes; n++) {
        const Mesh& mesh = meshes[n];

        FILESIZE += fwrite(&mesh.transform.c_11, sizeof(real32), 16, fid) * sizeof(real32);
        FILESIZE += fwrite(&mesh.is_collider, sizeof(bool32), 1, fid) * sizeof(bool32);
        FILESIZE += write_string(fid, mesh.name);
        FILESIZE += write_string(fid, mesh.mesh_name);

        printf(" %32s -> '%s'\n", mesh.name.c_str(), mesh.mesh_name.c_str());
    }

    FILESIZE += fwrite("END", 1, 3, fid);
    FILESIZE += (fputc(0, fid) == 0);

    fseek(fid, 4, SEEK_SET);
    filesize_write = static_cast<uint32>(FILESIZE);
    fwrite(&filesize_write, sizeof(uint32), 1, fid);
        
    printf(" [%i bytes] ", filesize_write);

    fclose(fid);

    return true;
}


// print contents of file
void display_mesh_file(const Options& opts);
void display_level_file(const Options& opts);
bool display_contents(const Options& opts) {
    printf("----------------Loading------------------\n");
    printf("Loading file: '%s'\n", opts.input_filename.c_str());

    std::string rf, fn, ext;
    utils::decompose_path(opts.input_filename, rf, fn, ext);
    printf("filename: %s\n", (fn+ext).c_str());
    bool ret = false;
    if (ext == ".mesh") {
        display_mesh_file(opts);
    } else if (ext == ".level") {
        display_level_file(opts);
    } else {
        printf("Unknown file extension: [%s]\n", ext.c_str());
    }

    return true;
}

#define read_multi(var,n) fread(var, sizeof(var[0]), n, fid);
#define read_single(var)  fread(&var, sizeof(var), 1, fid);

void print_color(float* color, char* fmt, ...);

void display_mesh_file(const Options& opts) {
    FILE* fid = fopen(opts.input_filename.c_str(), "rb");
    if (fid == nullptr) {
        printf("[ERROR] Failed to open file [%s]\n", opts.input_filename.c_str());
        return;
    }

    std::vector<std::string> mat_names;

    fseek(fid, 0L, SEEK_END);
    size_t real_filesize = ftell(fid);

    fseek(fid, 0L, SEEK_SET);

    char MAGIC[5] = { 0 };
    read_multi(MAGIC, 4);
    //printf("MAGIC = [%s]\n", MAGIC);
    if (strcmp(MAGIC, "MESH")) {
        printf("[ERROR] ill-formed .mesh file (v%d)\n", MESH_VERSION);
        goto exit;
    }

    uint32 filesize;
    read_single(filesize);
    if ((uint32)real_filesize != filesize) {
        printf("[ERROR] File is %zd bytes, file says its %d bytes...\n", real_filesize, filesize);
        goto exit;
    }

    uint32 file_version;
    read_single(file_version);

    uint32 flag;
    read_single(flag);

    uint64 timestamp;
    read_single(timestamp);
    
    struct tm* time_info;
    char timeString[32] = { 0 };
    time_info = localtime((time_t*)(&timestamp));
    strftime(timeString, sizeof(timeString), "%c", time_info);

    uint16 num_prims;
    read_single(num_prims);
    if (num_prims > 1000) { //  just in case
        printf("[ERROR] header not read properly.\n");
        goto exit;
    }
    mat_names.resize(num_prims);

    uint16 PADDING[3];
    read_multi(PADDING, 3);


    printf("Filesize: %zd bytes\n", real_filesize);
    printf("Mesh version: %d\n", file_version);
    printf("Flag = %d (", flag);
    if (flag & mesh_flag_is_rigged)   printf("is_rigged ");
    if (flag & mesh_flag_is_collider) printf("is_collider ");
    printf(")\n", flag);
    printf("File generated on: %s\n", timeString);
    printf("-----------------------------------------\n");

    // read materials
    printf("%d Materials\n", num_prims);
    for (int n = 0; n < num_prims; n++) {
        read_multi(MAGIC, 4);

        if (strcmp(MAGIC, "MATL")) {
            printf("  [ERROR] ill-formed .mesh file (v%d)\n", MESH_VERSION);
            goto exit;
        }

        uint32 mat_flag;
        read_single(mat_flag);

        real32 DiffuseFactor[3];
        read_multi(DiffuseFactor, 3);

        real32 NormalScale;
        read_single(NormalScale);

        real32 AmbientStrength;
        read_single(AmbientStrength);

        real32 MetallicFactor;
        read_single(MetallicFactor);

        real32 RoughnessFactor;
        read_single(RoughnessFactor);

        real32 EmissiveFactor[3];
        read_multi(EmissiveFactor, 3);

        char mat_name[1024] = { 0 };
        char d_name[1024] = { 0 };
        char n_name[1024] = { 0 };
        char a_name[1024] = { 0 };
        char e_name[1024] = { 0 };


        uint8 name_len;
        read_single(name_len);
        read_multi(mat_name, name_len);
        mat_names[n] = std::string(mat_name);

        // load optional textures
        if (mat_flag & 0x02) {
            read_single(name_len);
            read_multi(d_name, name_len);
        }
        if (mat_flag & 0x04) {
            read_single(name_len);
            read_multi(n_name, name_len);
        }
        if (mat_flag & 0x08) {
            read_single(name_len);
            read_multi(a_name, name_len);
        }
        if (mat_flag & 0x10) {
            read_single(name_len);
            read_multi(e_name, name_len);
        }

        // print out data
        printf("  Material %d: %s\n", n, mat_name);
        printf("    Flag = %d (", mat_flag);
        if (mat_flag & 0x02) printf("has_diffuse ");
        if (mat_flag & 0x04) printf("has_normal ");
        if (mat_flag & 0x08) printf("has_amr ");
        if (mat_flag & 0x10) printf("has_emissive ");
        printf(")\n");

        printf("    Diffuse:   ");
        print_color(DiffuseFactor, "[%.2f %.2f %.2f]", DiffuseFactor[0], DiffuseFactor[1], DiffuseFactor[2]);
        if (mat_flag & 0x02) printf(" - '%s'", d_name);
        printf("\n");

        printf("    Normal:          %.2f", NormalScale);
        if (mat_flag & 0x04) printf("       - '%s'", n_name);
        printf("\n");

        printf("    Ambient:         %.2f", AmbientStrength);
        if (mat_flag & 0x08) printf("       - '%s'", a_name);
        printf("\n");

        printf("    Metallic:        %.2f", MetallicFactor);
        if (mat_flag & 0x08) printf("       - '%s'", a_name);
        printf("\n");

        printf("    Roughness:       %.2f", RoughnessFactor);
        if (mat_flag & 0x08) printf("       - '%s'", a_name);
        printf("\n");

        printf("    Emissive:  ");
        print_color(EmissiveFactor, "[%.2f %.2f %.2f]", EmissiveFactor[0], EmissiveFactor[1], EmissiveFactor[2]);
        if (mat_flag & 0x10) printf(" - '%s'", e_name);
        printf("\n");

        if (n < (num_prims-1))
            printf("\n");
    }

    // read primitives
    printf("-----------------------------------------\n");
    printf("%d Primitives\n", num_prims);
    for (int n = 0; n < num_prims; n++) {
        read_multi(MAGIC, 4);

        if (strcmp(MAGIC, "PRIM")) {
            printf("  [ERROR] ill-formed .mesh file (v%d)\n", MESH_VERSION);
            goto exit;
        }

        uint32 num_verts;
        read_single(num_verts);
        uint32 num_indices;
        read_single(num_indices);
        uint32 mat_idx;
        read_single(mat_idx);


        fseek(fid, sizeof(uint32)*num_indices, SEEK_CUR);
        uint32 attribute_size = (flag & mesh_flag_is_rigged) ? 22 : 14;
        fseek(fid, attribute_size*sizeof(real32)*num_verts, SEEK_CUR);

        printf("  Primitive %d:\n", n);
        printf("    %d vertices\n", num_verts);
        printf("    %d indices\n", num_indices);
        printf("    Material %d (%s)\n", mat_idx, mat_names[mat_idx].c_str());
        if (n < (num_prims - 1))
            printf("\n");
    }

    // read skeleton
    if (flag & mesh_flag_is_rigged) {
        printf("-----------------------------------------\n");
        printf("Skeleton\n");

        read_multi(MAGIC, 4);
        if (strcmp(MAGIC, "SKEL")) {
            printf("  [ERROR] ill-formed .mesh file (v%d)\n", MESH_VERSION);
            goto exit;
        }

        uint32 num_bones;
        read_single(num_bones);

        printf("%d bones\n", num_bones);
        for (int n = 0; n < num_bones; n++) {

            uint32 bone_idx;
            read_single(bone_idx);

            int32 parent_idx;
            read_single(parent_idx);

            real32 debug_length;
            read_single(debug_length);

            real32 local_matrix[16];
            read_multi(local_matrix, 16);

            real32 inv_model_matrix[16];
            read_multi(local_matrix, 16);

            uint8 name_len;
            char bone_name[1024] = { 0 };
            read_single(name_len);
            read_multi(bone_name, name_len);

            printf("  %2d %2d %-20s [", bone_idx, parent_idx, bone_name);
            for (uint32 i = 0; i < 16; i++) {
                printf("%5.2f ", local_matrix[i]);
            }
            printf("]\n");
        }
    }

    read_multi(MAGIC, 4);

    if (strcmp(MAGIC, "END")) {
        printf("[ERROR] ill-formed .mesh file (v%d)\n", MESH_VERSION);
        goto exit;
    }

    printf("-----------------------------------------\n");

exit:
    fclose(fid);
    return;
}
void display_level_file(const Options& opts) {
    FILE* fid = fopen(opts.input_filename.c_str(), "rb");
    if (fid == nullptr) {
        printf("[ERROR] Failed to open file [%s]\n", opts.input_filename.c_str());
        return;
    }

    fseek(fid, 0L, SEEK_END);
    size_t real_filesize = ftell(fid);
    fseek(fid, 0L, SEEK_SET);

    char MAGIC[5] = { 0 };
    read_multi(MAGIC, 4);
    //printf("MAGIC = [%s]\n", MAGIC);
    if (strcmp(MAGIC, "LEVL")) {
        printf("[ERROR] ill-formed .level file\n");
        goto exit;
    }

    uint32 filesize;
    read_single(filesize);
    if ((uint32)real_filesize != filesize) {
        printf("[ERROR] File is %zd bytes, file says its %d bytes...\n", real_filesize, filesize);
        goto exit;
    }

    uint32 file_version;
    read_single(file_version);

    uint32 flag;
    read_single(flag);

    uint64 timestamp;
    read_single(timestamp);

    struct tm* time_info;
    char timeString[32] = { 0 };
    time_info = localtime((time_t*)(&timestamp));
    strftime(timeString, sizeof(timeString), "%c", time_info);

    uint16 num_meshes, num_materials;
    read_single(num_meshes);
    read_single(num_materials);

    uint32 PADDING;
    read_single(PADDING);

    // print data
    printf("Filesize: %zd bytes\n", real_filesize);
    printf("Level version: %d\n", file_version);
    printf("Flag = %d\n", flag);
    printf("File generated on: %s\n", timeString);
    printf("-----------------------------------------\n");

    printf("%d Meshes\n", num_meshes);
    for (int n = 0; n < num_meshes; n++) {
        real32 Transform[16];
        read_multi(Transform, 16);

        bool32 is_collider;
        read_single(is_collider);

        char name[1024] = { 0 };
        char mesh_name[1024] = { 0 };
        uint8 name_len;

        read_single(name_len);
        read_multi(name, name_len);

        read_single(name_len);
        read_multi(mesh_name, name_len);

        // print
        printf("  Mesh %d - %s\n", n, is_collider ? "collider" : "renderable");
        printf("    %s | %s\n", name, mesh_name);
        printf("    Transform [%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f]\n",
            Transform[0], Transform[1], Transform[2], Transform[3],
            Transform[4], Transform[5], Transform[6], Transform[7],
            Transform[8], Transform[9], Transform[10], Transform[11],
            Transform[12], Transform[13], Transform[14], Transform[15]);
    }
    printf("-----------------------------------------\n");

    read_multi(MAGIC, 4);
    if (strcmp(MAGIC, "END")) {
        printf("[ERROR] ill-formed .level file\n");
        goto exit;
    }

exit:
    fclose(fid);
    return;
}

void print_color(float* color, char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    const size_t bufsize = 1024;
    char buf[bufsize];
    vsnprintf(buf, bufsize, fmt, args);

    va_end(args);

    float rf = color[0];
    float gf = color[1];
    float bf = color[2];

    unsigned char r = (rf * 255.0f);
    unsigned char g = (gf * 255.0f);
    unsigned char b = (bf * 255.0f);

    printf("\033[48;2;%d;%d;%dm", 50, 50, 50);
    printf("\033[38;2;%d;%d;%dm%s\033[0m", r, g, b, buf);
}
