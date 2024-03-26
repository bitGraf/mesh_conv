#include "mesh_converter.h"
#include "utils.h"

#include <filesystem>

//printf("usage: mesh_conv [-v | --version] [-h] [-f input.gltf] [-o output/folder] [-fps 30]\n");
const char* usage_string =
"usage: meshconv [-v | --version] [-h | --help] <mode> input_filename [-o path/to/output] [-flip-uv] [-fps 30]\n"
"\n"
"modes:\n"
"    mesh:  simply extracts meshes from the input and writes them to the output directory\n"
"\n"
"    anim:  extracts all animations from the input and writes them to the output directory\n"
"\n"
"    level: extracts meshes and writes a .level file containing a catalog of\n"
"           of meshes (both renderable and colliders), while writing those\n"
"           .mesh files to separate folders.\n"
"\n"
"    disp: loads a .mesh or .level file and print the contents to the console\n"
"\n"
"    upgrade: loads a .mesh or .level file and upgrades it to the newest version (if possible)\n"
"\n";

int main(int argc, char** argv) {
    //printf("-------------------------------------------------\n");
    //printf("%d args\n", argc);
    //for (int n = 0; n < argc; n++) {
    //    printf("arg[%d] = '%s'\n", n, argv[n]);
    //}
    //printf("-------------------------------------------------\n");

    // mesh_conv help (or no mode set)
    if (argc < 2 || (strcmp(argv[1],"help")==0) || utils::cmdOptionExists(argv, argv + argc, "-h") || utils::cmdOptionExists(argv, argv + argc, "--help")) {
        printf("%s\n", usage_string);

        return 0;
    }

    // print tool version
    if (utils::cmdOptionExists(argv, argv + argc, "-v") || utils::cmdOptionExists(argv, argv + argc, "--version")) {
        printf("mesh_conv Version %s  (mesh v%d) (anim v%d) (level v%d)\n", TOOL_VERSION, MESH_VERSION, ANIM_VERSION, LEVEL_VERSION);
        return 0;
    }

    Options opt = {};

    char* mode_str = argv[1];
    if (strcmp(mode_str, "mesh") == 0) {
        // single mesh conversion mode
        opt.mode = SINGLE_MESH_MODE;
        printf("Creating a single .mesh file\n");
    } else if (strcmp(mode_str, "anim") == 0) {
        // full level conversion mode
        opt.mode = ANIM_MODE;
        printf("Outputting scene as .level file\n");
    } else if (strcmp(mode_str, "level") == 0) {
        // full level conversion mode
        opt.mode = LEVEL_MODE;
        printf("Outputting scene as .level file\n");
    } else if (strcmp(mode_str, "disp") == 0) {
        // print out the contents of a file
        opt.mode = DISPLAY_MODE;
    } else if (strcmp(mode_str, "upgrade") == 0) {
        // upgrade a files version
        opt.mode = UPGRADE_MODE;
    } else {
        printf("Incorrect mode!\n");
        printf("%s\n", usage_string);
        return 0;
    }

    if (argc < 3) {
        printf("Error - No input filename given!\n");
        return -1;
    }
    opt.input_filename = std::string(argv[2]);
    std::replace(opt.input_filename.begin(), opt.input_filename.end(), '/', '\\');

    // check if an output name is given
    if (utils::cmdOptionExists(argv + 3, argv + argc, "-o")) {
        char* out = utils::getCmdOption(argv + 3, argv + argc, "-o");
        if (out) {
            opt.output_folder = std::string(out);

            // check if its a filename or a path
            std::string folder, filename, ext;
            utils::decompose_path(std::string(opt.output_folder), folder, filename, ext);
            if (ext.length() == 0) {
                // its a dir
                printf("output directory\n");
            }
            else {
                // its a file
                printf("output file\n");
            }
        }
        else {
            printf("No output given after -o option!\n");
            return -1;
        }
    }
    else {
        // no output filename given - use the input filename but change the extension
        std::string folder, filename, ext;
        utils::decompose_path(std::string(opt.input_filename), folder, filename, ext);
        //printf("Input filename: [%s|%s|%s]\n", folder.c_str(), filename.c_str(), ext.c_str());

        if (folder.length() == 0 || folder[0] == '.') {
            // either just a filename, or a relative path.
            //printf("none/relative path!\n");
            //std::string pwd = std::filesystem::current_path().string();
            //folder = pwd + "/" + folder;
            //std::replace(folder.begin(), folder.end(), '\\', '/');
        }
        //printf("Input filename: [%s|%s|%s]\n", folder.c_str(), filename.c_str(), ext.c_str());

        opt.output_folder = folder + filename;
    }
    std::replace(opt.output_folder.begin(), opt.output_folder.end(), '/', '\\');

    if (utils::cmdOptionExists(argv, argv + argc, "-flip-uv")) {
        printf("  flipping UV y-coordinates\n");
        opt.flip_uvs_y = true;
    } else {
        opt.flip_uvs_y = false;
    }

    opt.frame_rate = 30.0;
    char* fps_str = utils::getCmdOption(argv, argv + argc, "-fps");
    if (fps_str) {
        double fps = std::atof(fps_str);
        if (fps != 0.0)
            opt.frame_rate = fps;
    }

    // print options
    printf("  input_filename: %s\n", opt.input_filename.c_str());

    if (opt.mode == DISPLAY_MODE) {
        // display contents of file
        display_contents(opt);
    } else if (opt.mode == UPGRADE_MODE) {
        // upgrade file
        upgrade_file(opt);
    } else if (opt.mode == SINGLE_MESH_MODE) {
        printf("  output_folder:  %s\n", opt.output_folder.c_str());

        // Run the actual conversion
        if (!convert_file(opt)) {
            printf("Failed to succesfully convert file!\n");
        } else {
            printf("Succesfully converted file!\n");
        }
    } else if (opt.mode == ANIM_MODE) {
        printf("  output_folder:  %s\n", opt.output_folder.c_str());

        // Run the actual conversion
        if (!extract_anims(opt)) {
            printf("Failed to succesfully convert file!\n");
        }
        else {
            printf("Succesfully converted file!\n");
        }
    }

    return 0;

#if 0
    if (utils::cmdOptionExists(argv, argv + argc, "-v") || utils::cmdOptionExists(argv, argv + argc, "--version")) {
        printf(" Version %d\n", -1);
        return 0;
    }

    if (utils::cmdOptionExists(argv, argv + argc, "-h"))
    {
        //printf("Help Message ^-^\n");
        printf("usage: mesh_conv [-v | --version] [-h] [-f input.gltf] [-o output/folder] [-fps 30]\n");
        return 0;
    }

    input_filename = utils::getCmdOption(argv, argv + argc, "-f");
    if (!input_filename) {
        input_filename = "..\\examples\\garden.gltf";
    }
    output_filename = utils::getCmdOption(argv, argv + argc, "-o");
    if (!output_filename) {
        output_filename = "..\\examples\\garden";
    }

    bool pack_files = false;
    if (utils::cmdOptionExists(argv, argv + argc, "-pack")) {
        printf("Packing model files into one file.\n");
        pack_files = true;
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
    opts.pack_files = pack_files;
    if (!convert_file(actual_input, output_filename, opts)) {
        printf("Failed to succesfully convert file!\n");
    } else {
        printf("Succesfully converted file!\n");
    }
#endif

    return 0;
}