#pragma once

#include "../Camera/Camera.h"
#include "../Renderer/Framebuffer.h"
#include "PresentationProfile.h"

namespace viz {

class PostProcessor {
public:
    void apply(Framebuffer& framebuffer, const Camera& camera, float horizon_radius,
               const PresentationProfile& profile) const;

private:
    static float luminance(const Color4& c);
    static Color4 grade_pixel(const Color4& c, float contrast, float saturation);
};

} // namespace viz
