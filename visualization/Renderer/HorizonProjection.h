#pragma once

#include "../Camera/Camera.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace viz {

// Screen-space projection of a spherical absorbing region at the world origin.
struct HorizonScreen {
    float center_u = 0.5f;
    float center_v = 0.5f;
    float center_x_px = 0.0f;
    float center_y_px = 0.0f;
    float radius_px = 0.0f;
    float depth = 0.5f;
};

inline float horizon_pixel_distance(float x_px, float y_px, const HorizonScreen& horizon) {
    const float dx = x_px - horizon.center_x_px;
    const float dy = y_px - horizon.center_y_px;
    return std::sqrt(dx * dx + dy * dy);
}

inline HorizonScreen project_central_horizon(const Camera& camera, float horizon_radius, int width,
                                             int height) {
    HorizonScreen out;
    if (width <= 0 || height <= 0 || horizon_radius <= 0.0f) {
        return out;
    }

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const Mat4 mvp = camera.view_projection(aspect);
    const Vec3 center_world(0.0f, 0.0f, 0.0f);
    const Vec3 center_clip = mvp.transform_point(center_world);

    out.center_u = std::clamp(center_clip.x * 0.5f + 0.5f, 0.0f, 1.0f);
    out.center_v = std::clamp(1.0f - (center_clip.y * 0.5f + 0.5f), 0.0f, 1.0f);
    out.center_x_px = out.center_u * static_cast<float>(width);
    out.center_y_px = out.center_v * static_cast<float>(height);
    out.depth = center_clip.z;

    auto edge_radius_px = [&](const Vec3& edge_world) {
        const Vec3 edge_clip = mvp.transform_point(edge_world);
        const float edge_x_px = (edge_clip.x * 0.5f + 0.5f) * static_cast<float>(width);
        const float edge_y_px = (1.0f - (edge_clip.y * 0.5f + 0.5f)) * static_cast<float>(height);
        const float dx = edge_x_px - out.center_x_px;
        const float dy = edge_y_px - out.center_y_px;
        return std::sqrt(dx * dx + dy * dy);
    };

    const float r = horizon_radius;
    const float rx = edge_radius_px(Vec3(r, 0.0f, 0.0f));
    const float ry = edge_radius_px(Vec3(0.0f, r, 0.0f));
    const float rz = edge_radius_px(Vec3(0.0f, 0.0f, r));
    out.radius_px = std::max({rx, ry, rz, 1.0f});
    return out;
}

inline float central_horizon_clip_depth(const Camera& camera, float u, float v, float aspect,
                                        float horizon_radius, const Mat4& mvp) {
    if (horizon_radius <= 0.0f) {
        return std::numeric_limits<float>::infinity();
    }

    const Vec3 origin = camera.position();
    const Vec3 dir = camera.world_ray_direction(u, v, aspect);
    const float oc_dot_d = origin.dot(dir);
    const float discriminant =
        oc_dot_d * oc_dot_d - (origin.length_squared() - horizon_radius * horizon_radius);
    if (discriminant < 0.0f) {
        return std::numeric_limits<float>::infinity();
    }

    const float sqrt_d = std::sqrt(discriminant);
    float t = -oc_dot_d - sqrt_d;
    if (t < 0.0f) {
        t = -oc_dot_d + sqrt_d;
    }
    if (t < 0.0f) {
        return std::numeric_limits<float>::infinity();
    }

    const Vec3 hit = origin + dir * t;
    const float clip_z = mvp.m[2] * hit.x + mvp.m[6] * hit.y + mvp.m[10] * hit.z + mvp.m[14];
    const float clip_w = mvp.m[3] * hit.x + mvp.m[7] * hit.y + mvp.m[11] * hit.z + mvp.m[15];
    if (std::abs(clip_w) <= 1e-8f) {
        return std::numeric_limits<float>::infinity();
    }
    return clip_z / clip_w;
}

} // namespace viz
