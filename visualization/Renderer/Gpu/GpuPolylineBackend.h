#pragma once

#include "../TrajectoryRenderBackend.h"
#include "GpuMeshBuffers.h"
#include "GpuShaderProgram.h"

#include <vector>

namespace viz {

class GpuPolylineBackend : public TrajectoryRenderBackend {
public:
    ~GpuPolylineBackend() override;

    bool initialize() override;
    void shutdown() override;
    void upload_scene(const Scene& scene) override;
    void resize(int width, int height) override;
    void render(const Scene& scene, const Camera& camera, const RenderOptions& render_options,
                const PresentationProfile& profile) override;

private:
    struct GpuTrajectoryDraw {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        std::size_t vertex_count = 0;
        TrajectoryStyle style{};
        double min_parameter = 0.0;
        double max_parameter = 1.0;
    };

    void destroy_trajectories();
    void upload_starfield_texture();
    void draw_starfield(const Camera& camera, float aspect, float brightness,
                        const PresentationProfile& profile, const RenderOptions& options);
    void draw_trajectories(const Scene& scene, const Camera& camera, float aspect,
                           const RenderOptions& options);

    int width_ = 1;
    int height_ = 1;

    GpuShaderProgram starfield_program_;
    GpuShaderProgram line_program_;
    GpuShaderProgram marker_program_;
    GpuShaderProgram procedural_star_program_;
    GpuShaderProgram post_program_;
    GpuShaderProgram bloom_program_;
    GpuShaderProgram horizon_mask_program_;

    unsigned int fullscreen_vao_ = 0;
    unsigned int fullscreen_vbo_ = 0;
    unsigned int starfield_texture_ = 0;
    
    unsigned int scene_fbo_ = 0;
    unsigned int scene_color_tex_ = 0;
    unsigned int scene_depth_tex_ = 0;
    
    unsigned int bloom_fbo_ = 0;
    unsigned int bloom_color_tex_ = 0;

    unsigned int procedural_star_vao_ = 0;
    unsigned int procedural_star_vbo_ = 0;
    std::uint32_t last_star_seed_ = 0xFFFFFFFFu;
    
    unsigned int element_vbo_ = 0;

    unsigned int marker_vao_ = 0;
    unsigned int marker_vbo_ = 0;

    std::vector<GpuTrajectoryDraw> trajectories_;

    bool show_starfield_ = true;
    bool use_image_starfield_ = false;
    float starfield_brightness_ = 1.0f;
    bool show_event_horizon_ = true;
    bool show_photon_sphere_ = false;
    Color4 background_{};
    float horizon_radius_ = 1.0f;
    float photon_sphere_radius_ = 1.5f;
};

} // namespace viz
