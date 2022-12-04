#include "mesh_converter.h"

#include "utils.h"
#include <iostream>

using namespace rh;

int main(int argc, char** argv) {
    char *input_filename, *output_filename;

    if (utils::cmdOptionExists(argv, argv + argc, "-v") || utils::cmdOptionExists(argv, argv + argc, "--version")) {
        std::cout << " Version " << rh::Version() << std::endl;
        return 0;
    }

    if (utils::cmdOptionExists(argv, argv + argc, "-h"))
    {
        std::cout << "Help Message ^-^" << std::endl;
        return 0;
    }

    float scale_factor = 100.0f;
    if (utils::cmdOptionExists(argv, argv + argc, "-s1")) {
        scale_factor = 1.0f;
    } else if (utils::cmdOptionExists(argv, argv + argc, "-s100")) {
        scale_factor = 100.0f;
    }

    input_filename = utils::getCmdOption(argv, argv + argc, "-f");
    if (!input_filename) {
        input_filename = "../examples/test.glb";
    }
    output_filename = utils::getCmdOption(argv, argv + argc, "-o");
    if (!output_filename) {
        output_filename = "../examples/output.mesh";
    }

    std::cout << "input filename:  [" << input_filename << "]" << std::endl;
    std::cout << "output filename: [" << output_filename << "]" << std::endl << std::endl;

    std::string actual_input;
    if (utils::is_blend_file(input_filename)) {
        printf("Running the converter with a .blend file input\n");

        std::string blender_exe_filepath;
        if (utils::found_blender_exe(blender_exe_filepath)) {
            printf("  Found blender .exe: '%s'\n", blender_exe_filepath.c_str());
            std::string intermediate_output = std::string(input_filename) + ".fbx";
            printf("Converting '%s' to an intermediate FBX format '%s'\n", input_filename, intermediate_output.c_str());

            // Form: blender.exe input_file.blend --background --python blender_export_script.py -- output.fbx
            std::string sys_call = "\"" + blender_exe_filepath + "\" " + 
                std::string(input_filename) + 
                " --background --python blender_export_script.py -- " + 
                intermediate_output;

            int result = system(sys_call.c_str());
            if (result) {
                printf("Failed to call blender .exe for some reason!\n");
                return -1;
            }

            printf("Running the regular conversion on intermediate file: '%s'\n\n\n", intermediate_output.c_str());
            actual_input = std::string(intermediate_output);
        }
        else {
            printf("  Could not find blender .exe\n");
            return -1;
        }
    }
    else {
        actual_input = std::string(input_filename);
    }

    // Do the actual conversion
    std::cout << "Running mesh_converter..." << std::endl;
    rh::MeshConverter conv;
    conv.LoadInputFile(actual_input.c_str());
    conv.ProcessMeshes();
    conv.SaveOutputFiles(output_filename);

    return 0;
}