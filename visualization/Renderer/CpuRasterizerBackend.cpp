#include "CpuRasterizerBackend.h"

namespace viz {

void CpuRasterizerBackend::render(const Scene& scene, const Camera& camera,
                                  const RenderOptions& render_options,
                                  const PresentationProfile& profile) {
    pipeline_.render(const_cast<Scene&>(scene), const_cast<Camera&>(camera), framebuffer_,
                     render_options, profile);
}

} // namespace viz
