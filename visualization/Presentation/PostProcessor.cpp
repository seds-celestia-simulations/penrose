#include "PostProcessor.h"

#include "../Renderer/HorizonProjection.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace viz {

namespace {

struct FloatColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

FloatColor to_float(const Color4& c) {
    return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

Color4 to_color(const FloatColor& c) {
    return Color4::from_float(c.r, c.g, c.b, c.a);
}

FloatColor sample_bilinear(const std::vector<FloatColor>& src, int width, int height, float u,
                           float v) {
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    const float x = u * static_cast<float>(width - 1);
    const float y = v * static_cast<float>(height - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);

    auto at = [&](int px, int py) { return src[static_cast<std::size_t>(py * width + px)]; };
    const FloatColor c00 = at(x0, y0);
    const FloatColor c10 = at(x1, y0);
    const FloatColor c01 = at(x0, y1);
    const FloatColor c11 = at(x1, y1);

    const FloatColor cx0 = {c00.r + (c10.r - c00.r) * tx, c00.g + (c10.g - c00.g) * tx,
                            c00.b + (c10.b - c00.b) * tx, c00.a + (c10.a - c00.a) * tx};
    const FloatColor cx1 = {c01.r + (c11.r - c01.r) * tx, c01.g + (c11.g - c01.g) * tx,
                            c01.b + (c11.b - c01.b) * tx, c01.a + (c11.a - c01.a) * tx};
    return {cx0.r + (cx1.r - cx0.r) * ty, cx0.g + (cx1.g - cx0.g) * ty, cx0.b + (cx1.b - cx0.b) * ty,
            cx0.a + (cx1.a - cx0.a) * ty};
}

} // namespace

float PostProcessor::luminance(const Color4& c) {
    return (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) / 255.0f;
}

Color4 PostProcessor::grade_pixel(const Color4& c, float contrast, float saturation) {
    FloatColor f = to_float(c);
    f.r = (f.r - 0.5f) * contrast + 0.5f;
    f.g = (f.g - 0.5f) * contrast + 0.5f;
    f.b = (f.b - 0.5f) * contrast + 0.5f;

    const float lum = 0.2126f * f.r + 0.7152f * f.g + 0.0722f * f.b;
    f.r = lum + (f.r - lum) * saturation;
    f.g = lum + (f.g - lum) * saturation;
    f.b = lum + (f.b - lum) * saturation;
    return to_color(f);
}

void PostProcessor::apply(Framebuffer& framebuffer, const Camera& camera, float horizon_radius,
                          const PresentationProfile& profile) const {
    const int width = framebuffer.width();
    const int height = framebuffer.height();
    if (width <= 0 || height <= 0) {
        return;
    }

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const Mat4 mvp = camera.view_projection(aspect);
    const HorizonScreen horizon =
        project_central_horizon(camera, horizon_radius, width, height);
    const float horizon_px = horizon.radius_px;
    const float halo_px = horizon_px * profile.halo_radius_scale;
    const float lens_px = horizon_px * profile.lensing_radius_scale;
    const std::vector<float>& depth_buffer = framebuffer.depth_buffer();

    std::vector<FloatColor> src;
    src.reserve(static_cast<std::size_t>(width * height));
    for (const Color4& c : framebuffer.color_buffer()) {
        src.push_back(to_float(c));
    }

    std::vector<FloatColor> bloom(src.size(), FloatColor{});
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const FloatColor& px = src[static_cast<std::size_t>(y * width + x)];
            const float lum = 0.2126f * px.r + 0.7152f * px.g + 0.0722f * px.b;
            if (lum < profile.bloom_threshold) {
                continue;
            }
            const float strength =
                (lum - profile.bloom_threshold) / std::max(1e-4f, 1.0f - profile.bloom_threshold);
            const int radius = profile.bloom_radius;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    const int sx = x + dx;
                    const int sy = y + dy;
                    if (sx < 0 || sy < 0 || sx >= width || sy >= height) {
                        continue;
                    }
                    const float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (dist > static_cast<float>(radius)) {
                        continue;
                    }
                    const float w = (1.0f - dist / static_cast<float>(radius + 1)) * strength;
                    FloatColor& dst = bloom[static_cast<std::size_t>(sy * width + sx)];
                    dst.r += px.r * w * profile.bloom_intensity;
                    dst.g += px.g * w * profile.bloom_intensity;
                    dst.b += px.b * w * profile.bloom_intensity;
                }
            }
        }
    }

    std::vector<FloatColor> warped(src.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
            const float x_px = static_cast<float>(x) + 0.5f;
            const float y_px = static_cast<float>(y) + 0.5f;
            const float dist_px = horizon_pixel_distance(x_px, y_px, horizon);

            float sample_u = u;
            float sample_v = v;
            if (dist_px < lens_px && lens_px > 1e-3f) {
                const float t = 1.0f - dist_px / lens_px;
                const float warp = profile.lensing_strength * t * t * t;
                const float du = u - horizon.center_u;
                const float dv = v - horizon.center_v;
                sample_u = horizon.center_u + du * (1.0f + warp);
                sample_v = horizon.center_v + dv * (1.0f + warp);
            }

            FloatColor c = sample_bilinear(src, width, height, sample_u, sample_v);
            const FloatColor b = bloom[static_cast<std::size_t>(y * width + x)];
            c.r = std::clamp(c.r + b.r, 0.0f, 1.0f);
            c.g = std::clamp(c.g + b.g, 0.0f, 1.0f);
            c.b = std::clamp(c.b + b.b, 0.0f, 1.0f);

            if (profile.halo_strength > 0.0f && dist_px > horizon_px * 0.85f && dist_px < halo_px) {
                const float t = (dist_px - horizon_px * 0.85f) / std::max(1.0f, halo_px - horizon_px * 0.85f);
                const float falloff = (1.0f - t) * profile.halo_strength;
                c.r *= (1.0f - falloff);
                c.g *= (1.0f - falloff);
                c.b *= (1.0f - falloff);
            }

            if (dist_px <= horizon_px) {
                const float horizon_depth =
                    central_horizon_clip_depth(camera, u, v, aspect, horizon_radius, mvp);
                const std::size_t depth_idx = static_cast<std::size_t>(y * width + x);
                if (std::isfinite(horizon_depth) && depth_buffer[depth_idx] > horizon_depth) {
                    c = {0.0f, 0.0f, 0.0f, 1.0f};
                }
            } else {
                const float ring_center = horizon_px * profile.lens_ring_radius_scale;
                const float ring_half_width =
                    std::max(0.5f, horizon_px * profile.lens_ring_width_scale);
                if (std::abs(dist_px - ring_center) < ring_half_width) {
                    const float ring_t =
                        1.0f - std::abs(dist_px - ring_center) / ring_half_width;
                    const float peak = ring_t * ring_t * ring_t * profile.lens_ring_strength;
                    c.r = std::min(c.r + peak * 1.00f, 1.0f);
                    c.g = std::min(c.g + peak * 0.94f, 1.0f);
                    c.b = std::min(c.b + peak * 0.72f, 1.0f);
                }
            }

            const float vx = u - 0.5f;
            const float vy = v - 0.5f;
            const float vig = 1.0f - profile.vignette * (vx * vx + vy * vy) * 2.2f;
            c.r *= vig;
            c.g *= vig;
            c.b *= vig;

            warped[static_cast<std::size_t>(y * width + x)] = c;
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const FloatColor& f = warped[static_cast<std::size_t>(y * width + x)];
            Color4 graded = grade_pixel(to_color(f), profile.contrast, profile.saturation);
            framebuffer.color_buffer()[static_cast<std::size_t>(y * width + x)] = graded;
        }
    }
}

} // namespace viz
