#pragma once

#include <string>
#include <vector>
#include <cstdio>

#include <laml/laml.hpp>

void level_print(int level, const char* format, ...);

namespace utils {
    char* getCmdOption(char** begin, char** end, const std::string& option);
    bool cmdOptionExists(char** begin, char** end, const std::string& option);
    bool file_exists(const std::string& filepath);
    bool is_blend_file(const char* filename);

    bool found_blender_exe(std::string& blender_path);

    bool decompose_path(const std::string& path, std::string& root_folder, std::string& filename, std::string& extension);

    laml::Vec3 map_gltf_vec_to_vec3(const std::vector<double>& gltf_vec);
    std::string mime_type_to_ext(std::string mime_type);
}