#include "mesh_converter.h"
#include "utils.h"

int main(int argc, char** argv) {
    char *input_filename, *output_filename;

    if (utils::cmdOptionExists(argv, argv + argc, "-v") || utils::cmdOptionExists(argv, argv + argc, "--version")) {
        printf(" Version %d\n", -1);
        return 0;
    }

    if (utils::cmdOptionExists(argv, argv + argc, "-h"))
    {
        printf("Help Message ^-^\n");
        return 0;
    }

    input_filename = utils::getCmdOption(argv, argv + argc, "-f");
    if (!input_filename) {
        input_filename = "..\\examples\\test_level.gltf";
    }
    output_filename = utils::getCmdOption(argv, argv + argc, "-o");
    if (!output_filename) {
        output_filename = "..\\examples\\test_level";
    }

    real32 sample_fps = 30.0f;
    char* fps_cmd = utils::getCmdOption(argv, argv + argc, "-fps");
    if (fps_cmd) {
        double d = std::atof(fps_cmd);
        if (d != 0.0)
            sample_fps = static_cast<real32>(d);
    }

    printf("input filename:  [%s]\n", input_filename);
    printf("output filename: [%s]\n\n", output_filename);

    std::string actual_input;
    if (false && utils::is_blend_file(input_filename)) {
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
    printf("Running mesh_converter...\n");

    mesh_conv_opts opts = {};
    opts.sample_rate = sample_fps;
    if (!convert_file(actual_input, output_filename, opts)) {
        printf("Failed to succesfully convert file!\n");
    } else {
        printf("Succesfully converted file!\n");
    }

    return 0;
}