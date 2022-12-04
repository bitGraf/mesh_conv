#pragma once

#include <string>

#include "mesh.h"

namespace rh::utils {
    char* getCmdOption(char** begin, char** end, const std::string& option);
    bool cmdOptionExists(char** begin, char** end, const std::string& option);
    bool file_exists(const std::string& filepath);
    bool is_blend_file(const char* filename);

    bool found_blender_exe(std::string& blender_path);
}