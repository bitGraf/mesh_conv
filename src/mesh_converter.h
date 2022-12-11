#pragma once

#include "mesh.h"

namespace rh {
    const char* Version();

    class MeshConverter {
    public:
        MeshConverter();
        ~MeshConverter();

        void LoadInputFile(const char* filename_in);
        void SaveOutputFiles(const char* path);
        void ProcessMeshes();

        std::vector<MeshData> m_meshes;
        f32 m_sample_frame_rate;
        bool m_valid;
    };
}