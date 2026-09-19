#pragma once

#include "../../Geometry/Mesh.h"

namespace viz {

struct GpuMeshDraw {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int vertex_count = 0;
};

GpuMeshDraw upload_mesh(const Mesh& mesh);
void destroy_mesh_draw(GpuMeshDraw& draw);

} // namespace viz
