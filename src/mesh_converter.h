#pragma once

#include "mesh.h"

namespace rh {
    const char* Version();

    class MeshConverter {
    public:
        MeshConverter();
        ~MeshConverter();

        void LoadInputFile(const char* filename_in, const char* filename_out);
    private:
        void ProcessFile(MeshSource* ms);
        void SaveOutputFile(const char* filename, const MeshSource* ms);
        void writeAnimFiles(const char* filename, const MeshSource* ms);
    };
}