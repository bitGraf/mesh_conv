#pragma once

#include "mesh.h"

namespace rh {
    const char* Version();

    class MeshConverter {
    public:
        MeshConverter();
        ~MeshConverter();

        void LoadInputFile(const char* filename_in);
        void ProcessFile();
        void SaveOutputFile(const char* filename_out);
    private:
        void writeAnimFiles(const char* filename_out);

        std::vector<MeshData> m_meshes;
        MeshData m_mesh;
        std::vector<Animation> m_anims;
        bool m_valid;
    };
}