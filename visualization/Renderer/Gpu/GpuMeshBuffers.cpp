#include "GpuMeshBuffers.h"

#include <glad/glad.h>

#include <vector>

namespace viz {

GpuMeshDraw upload_mesh(const Mesh& mesh) {
    std::vector<float> vertices;
    vertices.reserve(mesh.triangles.size() * 18);
    for (const Triangle& tri : mesh.triangles) {
        const Vec3 verts[3] = {tri.v0, tri.v1, tri.v2};
        for (const Vec3& p : verts) {
            vertices.push_back(p.x);
            vertices.push_back(p.y);
            vertices.push_back(p.z);
            vertices.push_back(tri.normal.x);
            vertices.push_back(tri.normal.y);
            vertices.push_back(tri.normal.z);
        }
    }

    GpuMeshDraw draw;
    draw.vertex_count = static_cast<int>(vertices.size() / 6);
    glGenVertexArrays(1, &draw.vao);
    glGenBuffers(1, &draw.vbo);
    glBindVertexArray(draw.vao);
    glBindBuffer(GL_ARRAY_BUFFER, draw.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);
    return draw;
}

void destroy_mesh_draw(GpuMeshDraw& draw) {
    if (draw.vbo != 0) {
        glDeleteBuffers(1, &draw.vbo);
        draw.vbo = 0;
    }
    if (draw.vao != 0) {
        glDeleteVertexArrays(1, &draw.vao);
        draw.vao = 0;
    }
    draw.vertex_count = 0;
}

} // namespace viz
