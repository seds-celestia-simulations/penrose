#pragma once

#include "../Camera/Camera.h"
#include "../Renderer/CPURasterizer.h"
#include "../Renderer/Framebuffer.h"
#include "../Scene/Scene.h"
#include "PostProcessor.h"
#include "PresentationProfile.h"

namespace viz {

// Presentation pipeline: deterministic rasterizer followed by optional post-processing.
class PresentationPipeline {
public:
    // Callers must pass presentation/render options from VisualizationConfig (main.cpp).
    // Defaults are inert: empty RenderOptions and disabled PresentationProfile.
    void render(Scene& scene, Camera& camera, Framebuffer& framebuffer,
                const RenderOptions& render_options = {},
                const PresentationProfile& profile = {}) const;

private:
    CPURasterizer rasterizer_;
    PostProcessor post_processor_;
};

} // namespace viz
