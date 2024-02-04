#pragma once

#include <cassert>
#include <laml/laml.hpp>
#include "utils.h"

struct mesh_conv_opts {
    real32 sample_rate;
    bool pack_files;
};

bool convert_file(const std::string& input_filename, const std::string& output_filename, const mesh_conv_opts& opts);