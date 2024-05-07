#pragma once

#include <cassert>
#include <laml/laml.hpp>
#include "utils.h"

enum OperationModeType {
    HELP_MODE,
    SINGLE_MESH_MODE,
    ANIM_MODE,
    LEVEL_MODE,
    DISPLAY_MODE,
    UPGRADE_MODE,
};

struct Options {
    OperationModeType mode;

    std::string input_filename;
    std::string output_folder;

    bool flip_uvs_y;
    float frame_rate;
};

#define TOOL_VERSION "v0.2.0"
/* Mesh Version 3:
 *      -Added support for skinned meshes. This adds new vertex attributes, and so changes the size of the primitive data block
 * Mesh Version 4:
 *      -Added support for Line primitives. Adds a new parameter to each primitive indicating the type.
 * Mesh Version 5:
 *      -Remove material definition from mesh file. Now contains a 'default_material_name' field. This can be empty,
 *       and in use the mesh needs to be paired with a material separatly. Needs to pair with a Material Version 1.
 */
const uint32 MESH_VERSION  = 5;
const uint32 MAT_VERSION   = 1;
const uint32 ANIM_VERSION  = 1;
const uint32 LEVEL_VERSION = 1;

const uint32 mesh_flag_is_rigged   = 0x01; // 1
const uint32 mesh_flag_is_collider = 0x02; // 2

const uint32 anim_flag_is_sampled  = 0x01; // 1

const uint32 mat_flag_double_sided = 0x01; // 1
const uint32 mat_flag_has_diffuse  = 0x02; // 2
const uint32 mat_flag_has_normal   = 0x04; // 4
const uint32 mat_flag_has_amr      = 0x08; // 8
const uint32 mat_flag_has_emissive = 0x10; // 16

bool convert_file(const Options& opts);
bool extract_anims(const Options& opts);
bool display_contents(const Options& opts);
bool upgrade_file(const Options& opts);


/****************************************
 *
 *   MESH
 *
 ****************************************/
enum class prim_type : uint32 {
    NONE = 0,
    triangles = 1,
    lines = 2
};
struct Mesh_Primitive {
    int32 material_index;
    std::string default_mat_name; // default material name

    prim_type prim_type;
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
    //bool32 combined_amr;

    laml::Vec3 emissive_factor;
    std::string emissive_texture;
    bool32 emissive_has_texture;
};

/****************************************
*
*   ANIMATION
*
* ************************************/
struct BoneAnim {
    uint32 bone_idx;
    std::vector<laml::Vec3> translation;
    std::vector<laml::Quat> rotation;
    std::vector<laml::Vec3> scale;
};
struct Animation {
    Skeleton skeleton;
    std::string name;
    std::vector<BoneAnim> bones;
    real32 frame_rate;
    real32 length;
    uint32 flag;
};