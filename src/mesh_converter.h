#pragma once

#include <cassert>
#include <laml/laml.hpp>
#include "utils.h"

enum OperationModeType {
    HELP_MODE,
    SINGLE_MESH_MODE,
    LEVEL_MODE,
};

struct Options {
    OperationModeType mode;

    std::string input_filename;
    std::string output_folder;
};

#define TOOL_VERSION "v0.1.0"
const uint32 MESH_VERSION = 2;
const uint32 LEVEL_VERSION = 1;

bool convert_file(const Options& opts);