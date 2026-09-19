#pragma once

#include "../Camera/Camera.h"
#include "../Scene/Scene.h"
#include "Framebuffer.h"

namespace viz {

struct RenderOptions {
    std::uint32_t starfield_seed = 0xC0FFEEu;
    bool deterministic = true;
    bool starfield_half_resolution = false;
    std::size_t trail_max_dense_segments = 1200;
    std::size_t trail_full_detail_tail = 200;
};

class CPURasterizer {
public:
    void render(Scene& scene, const Camera& camera, Framebuffer& framebuffer,
                const RenderOptions& options = {}) const;

private:
    struct ProjectedVertex {
        float x = 0.0f;
        float y = 0.0f;
        float depth = 0.0f;
        Vec3 world;
        Vec3 normal;
    };

    void draw_starfield(const Camera& camera, Framebuffer& framebuffer, std::uint32_t seed) const;
    void draw_starfield_image(const Camera& camera, Framebuffer& framebuffer, float brightness,
                              bool half_resolution) const;
    void draw_mesh(const Mesh& mesh, const Mat4& mvp, const Camera& camera, Framebuffer& framebuffer,
                   const Color4& base_color, float alpha, bool lit, bool wireframe = false) const;
    void draw_line(const Vec3& a, const Vec3& b, const Mat4& mvp, Framebuffer& framebuffer, const Color4& color,
                   float width) const;
    void draw_glow_disc(const Vec3& center, float radius, const Mat4& mvp, Framebuffer& framebuffer,
                        const Color4& color) const;
    void draw_trajectories(Scene& scene, const Camera& camera, const Mat4& mvp, Framebuffer& framebuffer,
                           float horizon_radius, const RenderOptions& options) const;

    ProjectedVertex project_vertex(const Vec3& v, const Mat4& mvp, const Vec3& normal) const;
    void rasterize_triangle(const ProjectedVertex& v0, const ProjectedVertex& v1, const ProjectedVertex& v2,
                            Framebuffer& framebuffer, const Color4& color, bool lit) const;
    void plot_line(int x0, int y0, int x1, int y1, float depth0, float depth1, Framebuffer& framebuffer,
                   const Color4& color, float width) const;
};

} // namespace viz
