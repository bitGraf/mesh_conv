#include "utils.h"
//#include <algorithm>
#include <fstream>
#include <vector>

namespace rh::utils {

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

    laml::Mat4 mat4_from_aiMatrix4x4(aiMatrix4x4& in) {
        //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
        // my matrix 4 is column-major, theirs is row-major...
        laml::Mat4 m(in.a1, in.b1, in.c1, in.d1, 
            in.a2, in.b2, in.c2, in.d2,
            in.a3, in.b3, in.c3, in.d3,
            in.a4, in.b4, in.c4, in.d4
        );

        return m;
    }

    laml::Vec2 vec2_from_aiVector3D(aiVector3D& in) {
        laml::Vec2 v;

        v.x = in.x;
        v.y = in.y;

        return v;
    }
    laml::Vec3 vec3_from_aiVector3D(aiVector3D& in) {
        laml::Vec3 v;

        v.x = in.x;
        v.y = in.y;
        v.z = in.z;

        return v;
    }
    laml::Quat vec4_from_aiVector4D(aiQuaternion& in) {
        laml::Quat v;

        v.x = in.x;
        v.y = in.y;
        v.z = in.z;
        v.w = in.w;

        return v;
    }
}