#include "PresentationPipeline.h"

namespace viz {

void PresentationPipeline::render(Scene& scene, Camera& camera, Framebuffer& framebuffer,
                                  const RenderOptions& render_options,
                                  const PresentationProfile& profile) const {
    rasterizer_.render(scene, camera, framebuffer, render_options);
    if (profile.enabled) {
        post_processor_.apply(framebuffer, camera, scene.settings().horizon_radius, profile);
    }
}

} // namespace viz
