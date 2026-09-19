#pragma once

#include "../Camera/Camera.h"
#include "../Presentation/PresentationProfile.h"
#include "../Renderer/CPURasterizer.h"
#include "../Scene/Scene.h"

namespace viz {

// Physics-agnostic trajectory presentation backend (CPU rasterizer or GPU polylines).
class TrajectoryRenderBackend {
public:
    virtual ~TrajectoryRenderBackend() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void upload_scene(const Scene& scene) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void render(const Scene& scene, const Camera& camera, const RenderOptions& render_options,
                        const PresentationProfile& profile) = 0;
};

} // namespace viz
