#pragma once

#include <string>
#include <assimp/matrix4x4.h>
#include <assimp/vector2.h>
#include <assimp/vector3.h>
#include <assimp/quaternion.h>

#include "mesh.h"

namespace rh::utils {
    char* getCmdOption(char** begin, char** end, const std::string& option);
    bool cmdOptionExists(char** begin, char** end, const std::string& option);
    bool file_exists(const std::string& filepath);
    bool is_blend_file(const char* filename);

    bool found_blender_exe(std::string& blender_path);

    laml::Mat4 mat4_from_aiMatrix4x4(aiMatrix4x4& in);
    laml::Vec2 vec2_from_aiVector3D(aiVector3D& in);
    laml::Vec3 vec3_from_aiVector3D(aiVector3D& in);
    laml::Quat quat_from_aiQuaternion(aiQuaternion& in);
}