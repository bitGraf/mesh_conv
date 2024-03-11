#pragma once

#include <cassert>
#include <laml/laml.hpp>
#include "utils.h"

enum OperationModeType {
    HELP_MODE,
    SINGLE_MESH_MODE,
    LEVEL_MODE,
    DISPLAY_MODE,
};

struct Options {
    OperationModeType mode;

    std::string input_filename;
    std::string output_folder;

    bool flip_uvs_y;
};

#define TOOL_VERSION "v0.1.2"
const uint32 MESH_VERSION = 3;
/* Mesh Version 3:
 *      -Added support for skinned meshes. This adds new vertex attributes, and so changes the size of the primitive data block
 */

const uint32 LEVEL_VERSION = 1;

const uint32 mesh_flag_is_rigged   = 0x01; // 1
const uint32 mesh_flag_is_collider = 0x02; // 2

bool convert_file(const Options& opts);
bool display_contents(const Options& opts);