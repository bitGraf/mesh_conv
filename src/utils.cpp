#include "utils.h"

#include <fstream>
#include <vector>
#include <cstdarg>

void level_print(int level, const char* format, ...) {
    va_list args;
    va_start(args, format);

    for (int n = 0; n < level; n++) {
        printf(" \371 ");
    }

    vprintf(format, args);

    va_end(args);
}

namespace utils {

    char* getCmdOption(char** begin, char** end, const std::string& option)
    {
        char** itr = std::find(begin, end, option);
        if (itr != end && ++itr != end)
        {
            return *itr;
        }
        return 0;
    }

    bool cmdOptionExists(char** begin, char** end, const std::string& option)
    {
        return std::find(begin, end, option) != end;
    }

    bool file_exists(const std::string& filepath) {
        std::ifstream f(filepath.c_str());
        return f.good();
    }

    bool is_blend_file(const char* filename) {
        std::string tmp(filename);

        return (tmp.substr(tmp.find_last_of(".") + 1) == "blend");
    }

    bool found_blender_exe(std::string& blender_path) {
        std::vector<std::string> common_paths = {
            "C:\\Program Files\\Blender Foundation\\Blender 2.81\\blender.exe",
            "C:\\Program Files\\Blender Foundation\\Blender 2.82\\blender.exe",
            "C:\\Program Files\\Blender Foundation\\Blender\\blender.exe"
        };

        int num_found = 0;
        std::vector<size_t> found_idx;
        for (size_t n = 0; n < common_paths.size(); n++) {
            printf("  Checking '%s'...", common_paths[n].c_str());
            if (file_exists(common_paths[n])) {
                num_found++;
                found_idx.push_back(n);
                printf("Found!\n");
            }
            else {
                printf("Not Found!\n");
            }
        }

        printf("  Found %d valid executables\n", num_found);
        if (num_found > 0) {
            if (num_found == 1) {
                // only 1 option
                blender_path = common_paths[found_idx[0]];
            }
            else {
                // just pick the first one for now...
                blender_path = common_paths[found_idx[0]];
            }

            return true;
        }

        return false;
    }

    bool decompose_path(const std::string& path, std::string& root_folder, std::string& filename, std::string& extension) {
        size_t last_slash = path.find_last_of("\\")+1;
        size_t last_dot = path.find_last_of(".");
        if (last_dot < last_slash) {
            // no extension?
            root_folder = path.substr(0, last_slash);
            filename = path.substr(last_slash, path.length() - last_slash);
            extension = std::string();
        }
        else {
            root_folder = path.substr(0, last_slash);
            filename = path.substr(last_slash, last_dot - last_slash);
            extension = path.substr(last_dot, path.length() - last_dot);
        }

        //printf("Input path: [%s]\n", path.c_str());
        //printf("  root_folder: [%s]\n", root_folder.c_str());
        //printf("  filename:    [%s]\n", filename.c_str());
        //printf("  extension:   [%s]\n", extension.c_str());

        return true;
    }

    laml::Vec3 map_gltf_vec_to_vec3(const std::vector<double>& gltf_vec) {
        laml::Vec3 vec;

        vec.x = gltf_vec[0];
        vec.y = gltf_vec[1];
        vec.z = gltf_vec[2];

        return vec;
    }

    std::string mime_type_to_ext(std::string mime_type) {
        // only doing image/subtype

        size_t slash = mime_type.find_last_of("/")+1;

        std::string type = mime_type.substr(0, slash-1);
        std::string subtype = mime_type.substr(slash, mime_type.length()-slash);

        if (type.compare("image") == 0) {
            return subtype;
        } else {
            printf("[ERROR] unexpected MIME type: '%s'\n", mime_type.c_str());
            return std::string();
        }
    }
}