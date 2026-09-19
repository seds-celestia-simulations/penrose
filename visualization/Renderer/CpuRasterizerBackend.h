#pragma once

#include "Framebuffer.h"
#include "TrajectoryRenderBackend.h"
#include "../Presentation/PresentationPipeline.h"

namespace viz {

// CPU presentation backend: CPURasterizer + optional PostProcessor into a Framebuffer.
class CpuRasterizerBackend : public TrajectoryRenderBackend {
public:
    bool initialize() override { return true; }
    void shutdown() override {}
    void upload_scene(const Scene& /*scene*/) override {}
    void resize(int width, int height) override { framebuffer_.resize(width, height); }

    void render(const Scene& scene, const Camera& camera, const RenderOptions& render_options,
                const PresentationProfile& profile) override;

    Framebuffer& framebuffer() { return framebuffer_; }
    const Framebuffer& framebuffer() const { return framebuffer_; }

private:
    Framebuffer framebuffer_;
    PresentationPipeline pipeline_;
};

} // namespace viz
