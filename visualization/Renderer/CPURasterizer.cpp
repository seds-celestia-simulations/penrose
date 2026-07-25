#include "CPURasterizer.h"

#include "../Geometry/Mesh.h"
#include "../IO/StarfieldBackground.h"
#include "HorizonProjection.h"
#include "StarfieldGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace viz {

namespace {

void paint_absorbing_region(Framebuffer& framebuffer, const Camera& camera, float horizon_radius,
                            const HorizonScreen& horizon) {
    const float cx = horizon.center_x_px;
    const float cy = horizon.center_y_px;
    const float r = horizon.radius_px;

    const int w = framebuffer.width();
    const int h = framebuffer.height();
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    const Mat4 mvp = camera.view_projection(aspect);

    const int min_x = std::max(0, static_cast<int>(cx - r - 1.0f));
    const int max_x = std::min(w - 1, static_cast<int>(cx + r + 1.0f));
    const int min_y = std::max(0, static_cast<int>(cy - r - 1.0f));
    const int max_y = std::min(h - 1, static_cast<int>(cy + r + 1.0f));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const float x_px = static_cast<float>(x) + 0.5f;
            const float y_px = static_cast<float>(y) + 0.5f;
            if (horizon_pixel_distance(x_px, y_px, horizon) > r) {
                continue;
            }

            const float u = x_px / static_cast<float>(w);
            const float v = y_px / static_cast<float>(h);
            const float horizon_depth =
                central_horizon_clip_depth(camera, u, v, aspect, horizon_radius, mvp);
            if (!std::isfinite(horizon_depth)) {
                continue;
            }

            const std::size_t idx = static_cast<std::size_t>(y * w + x);
            if (framebuffer.depth_buffer()[idx] > horizon_depth) {
                framebuffer.set_pixel(x, y, Color4::rgb(0, 0, 0), horizon_depth);
            }
        }
    }
}

void paint_horizon_glow(Framebuffer& framebuffer, const HorizonScreen& horizon) {
    const float cx = horizon.center_x_px;
    const float cy = horizon.center_y_px;
    const float r = std::max(horizon.radius_px, 1.0f);
    const float glow_outer = r * 1.85f;

    const int w = framebuffer.width();
    const int h = framebuffer.height();
    const int min_x = std::max(0, static_cast<int>(cx - glow_outer - 1.0f));
    const int max_x = std::min(w - 1, static_cast<int>(cx + glow_outer + 1.0f));
    const int min_y = std::max(0, static_cast<int>(cy - glow_outer - 1.0f));
    const int max_y = std::min(h - 1, static_cast<int>(cy + glow_outer + 1.0f));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const float x_px = static_cast<float>(x) + 0.5f;
            const float y_px = static_cast<float>(y) + 0.5f;
            const float dist = horizon_pixel_distance(x_px, y_px, horizon);
            if (dist <= r || dist >= glow_outer) {
                continue;
            }
            const float t = (dist - r) / std::max(1.0f, glow_outer - r);
            const float alpha = 0.22f * std::pow(1.0f - t, 1.6f);
            if (alpha < 0.01f) {
                continue;
            }
            framebuffer.blend_pixel(x, y, Color4::from_float(0.70f, 0.58f, 0.42f, alpha), 0.999f);
        }
    }
}

void paint_photon_sphere_ring(Framebuffer& framebuffer, const HorizonScreen& horizon,
                              const HorizonScreen& photon) {
    const float horizon_px = std::max(horizon.radius_px, 1.0f);
    const float photon_px = photon.radius_px;
    if (photon_px <= horizon_px) {
        return;
    }

    const float cx = horizon.center_x_px;
    const float cy = horizon.center_y_px;
    const float ring_half = std::max(2.0f, horizon_px * 0.035f);
    const float outer = photon_px + ring_half;

    const int w = framebuffer.width();
    const int h = framebuffer.height();
    const int min_x = std::max(0, static_cast<int>(cx - outer - 1.0f));
    const int max_x = std::min(w - 1, static_cast<int>(cx + outer + 1.0f));
    const int min_y = std::max(0, static_cast<int>(cy - outer - 1.0f));
    const int max_y = std::min(h - 1, static_cast<int>(cy + outer + 1.0f));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const float x_px = static_cast<float>(x) + 0.5f;
            const float y_px = static_cast<float>(y) + 0.5f;
            const float dist = horizon_pixel_distance(x_px, y_px, horizon);
            const float d = std::abs(dist - photon_px);
            if (d >= ring_half) {
                continue;
            }
            const float edge = 1.0f - d / ring_half;
            const float alpha = 0.35f * edge * edge;
            if (alpha < 0.01f) {
                continue;
            }
            framebuffer.blend_pixel(x, y, Color4::from_float(0.82f, 0.78f, 0.70f, alpha), 0.999f);
        }
    }
}

bool outside_horizon_radius(const Vec3& position, float horizon_radius) {
    return position.length_squared() >= horizon_radius * horizon_radius;
}

Color4 shade_color(const Color4& base, float ndotl, float glow) {
    const float lit = std::clamp(0.25f + 0.75f * ndotl + glow, 0.0f, 1.0f);
    return Color4::from_float(base.r / 255.0f * lit, base.g / 255.0f * lit, base.b / 255.0f * lit,
                              base.a / 255.0f);
}

Color4 lerp_color(const Color4& a, const Color4& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const auto lerp_channel = [&](std::uint8_t x, std::uint8_t y) {
        return (static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t) / 255.0f;
    };
    return Color4::from_float(lerp_channel(a.r, b.r), lerp_channel(a.g, b.g), lerp_channel(a.b, b.b),
                              lerp_channel(a.a, b.a));
}

Color4 scale_color_rgb(const Color4& c, float scale, float alpha_scale = 1.0f) {
    return Color4::from_float(std::min(c.r / 255.0f * scale, 1.0f),
                              std::min(c.g / 255.0f * scale, 1.0f),
                              std::min(c.b / 255.0f * scale, 1.0f),
                              std::min(c.a / 255.0f * alpha_scale, 1.0f));
}

Color4 trajectory_segment_color(const Trajectory& traj, const TrajectoryStyle& style, std::size_t segment_index,
                                std::size_t end_idx) {
    const float along =
        end_idx > 0 ? static_cast<float>(segment_index) / static_cast<float>(end_idx) : 1.0f;
    Color4 base = style.color;
    if (style.gradient_along_path && traj.sample_count() > 1) {
        const double span = traj.max_parameter() - traj.min_parameter();
        const double mid_param =
            0.5 * (traj.samples()[segment_index - 1].parameter + traj.samples()[segment_index].parameter);
        const float gradient_t =
            span > 1e-12 ? static_cast<float>((mid_param - traj.min_parameter()) / span) : 0.0f;
        base = lerp_color(style.gradient_start, style.gradient_end, gradient_t);
    }
    const float fade = along * along;
    const float rgb_scale = style.trail_rgb_min + (1.0f - style.trail_rgb_min) * fade;
    const float alpha_scale = style.trail_alpha_min + (1.0f - style.trail_alpha_min) * fade;
    return scale_color_rgb(base, rgb_scale, alpha_scale);
}

} // namespace

void CPURasterizer::render(Scene& scene, const Camera& camera, Framebuffer& framebuffer,
                           const RenderOptions& options) const {
    const float aspect = static_cast<float>(framebuffer.width()) / static_cast<float>(framebuffer.height());
    const Mat4 mvp = camera.view_projection(aspect);
    const float rs = scene.settings().horizon_radius;

    framebuffer.clear(scene.settings().background);

    if (scene.settings().show_starfield) {
        if (scene.settings().use_image_starfield && StarfieldBackground::instance().loaded()) {
            draw_starfield_image(camera, framebuffer, scene.settings().starfield_brightness,
                                 options.starfield_half_resolution);
        } else {
            draw_starfield(camera, framebuffer, options.starfield_seed);
        }
    }

    draw_trajectories(scene, camera, mvp, framebuffer, rs, options);

    if (scene.settings().show_event_horizon) {
        const HorizonScreen horizon =
            project_central_horizon(camera, rs, framebuffer.width(), framebuffer.height());
        paint_horizon_glow(framebuffer, horizon);
        paint_absorbing_region(framebuffer, camera, rs, horizon);
        if (scene.settings().show_photon_sphere) {
            const HorizonScreen photon = project_central_horizon(
                camera, rs * 1.5f, framebuffer.width(), framebuffer.height());
            paint_photon_sphere_ring(framebuffer, horizon, photon);
        }
    }
}

void CPURasterizer::draw_starfield_image(const Camera& camera, Framebuffer& framebuffer,
                                         float brightness, bool half_resolution) const {
    const int width = framebuffer.width();
    const int height = framebuffer.height();
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const StarfieldBackground& background = StarfieldBackground::instance();
    const int step = half_resolution ? 2 : 1;

    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
            const float u = (static_cast<float>(x) + 0.5f * static_cast<float>(step)) /
                            static_cast<float>(width);
            const float v = (static_cast<float>(y) + 0.5f * static_cast<float>(step)) /
                            static_cast<float>(height);
            const Vec3 dir = camera.world_ray_direction(u, v, aspect);
            const Color4 color = background.sample_direction(dir, brightness);
            if (step == 1) {
                framebuffer.set_pixel(x, y, color, 1.0f);
            } else {
                for (int dy = 0; dy < step && y + dy < height; ++dy) {
                    for (int dx = 0; dx < step && x + dx < width; ++dx) {
                        framebuffer.set_pixel(x + dx, y + dy, color, 1.0f);
                    }
                }
            }
        }
    }
}

void CPURasterizer::draw_starfield(const Camera& camera, Framebuffer& framebuffer, std::uint32_t seed) const {
    const int star_count = 900;
    const Vec3 cam_pos = camera.position();
    const float aspect = static_cast<float>(framebuffer.width()) / static_cast<float>(framebuffer.height());
    const Mat4 mvp = camera.view_projection(aspect);
    
    std::vector<Star> stars = StarfieldGenerator::generate_stars(seed, star_count);

    for (const Star& star : stars) {
        const Vec3 pos = cam_pos + star.direction * 200.0f;

        const ProjectedVertex pv = project_vertex(pos, mvp, star.direction);
        if (pv.depth <= 0.0f || pv.depth >= 1.0f) {
            continue;
        }

        const float brightness = star.brightness;
        const Color4 color = Color4::from_float(brightness, brightness, 0.75f + 0.25f * star.w, 1.0f);
        const int cx = static_cast<int>(pv.x * framebuffer.width());
        const int cy = static_cast<int>(pv.y * framebuffer.height());
        const int radius = star.w > 0.92f ? 2 : 1;
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                framebuffer.set_pixel(cx + dx, cy + dy, color, pv.depth);
            }
        }
    }
}

CPURasterizer::ProjectedVertex CPURasterizer::project_vertex(const Vec3& v, const Mat4& mvp,
                                                               const Vec3& normal) const {
    ProjectedVertex out;
    out.world = v;
    out.normal = normal.normalized();

    const float clip_x = mvp.m[0] * v.x + mvp.m[4] * v.y + mvp.m[8] * v.z + mvp.m[12];
    const float clip_y = mvp.m[1] * v.x + mvp.m[5] * v.y + mvp.m[9] * v.z + mvp.m[13];
    const float clip_z = mvp.m[2] * v.x + mvp.m[6] * v.y + mvp.m[10] * v.z + mvp.m[14];
    const float clip_w = mvp.m[3] * v.x + mvp.m[7] * v.y + mvp.m[11] * v.z + mvp.m[15];
    if (clip_w <= 1e-6f) {
        out.depth = 1.0f;
        return out;
    }

    const float ndc_x = clip_x / clip_w;
    const float ndc_y = clip_y / clip_w;
    out.depth = clip_z / clip_w;
    out.x = (ndc_x * 0.5f + 0.5f);
    out.y = (1.0f - (ndc_y * 0.5f + 0.5f));
    return out;
}

void CPURasterizer::rasterize_triangle(const ProjectedVertex& v0, const ProjectedVertex& v1,
                                         const ProjectedVertex& v2, Framebuffer& framebuffer, const Color4& color,
                                         bool lit) const {
    const float width = static_cast<float>(framebuffer.width());
    const float height = static_cast<float>(framebuffer.height());

    const float area = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
    if (std::abs(area) < 1e-8f) {
        return;
    }

    const int min_x = std::clamp(static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x}) * width)), 0,
                                 framebuffer.width() - 1);
    const int max_x = std::clamp(static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x}) * width)), 0,
                                 framebuffer.width() - 1);
    const int min_y = std::clamp(static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y}) * height)), 0,
                                 framebuffer.height() - 1);
    const int max_y = std::clamp(static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y}) * height)), 0,
                                 framebuffer.height() - 1);

    const Vec3 light_dir = Vec3(0.15f, 0.92f, 0.12f).normalized();
    const float ndotl = std::max(0.0f, v0.normal.dot(light_dir));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const float px = (static_cast<float>(x) + 0.5f) / width;
            const float py = (static_cast<float>(y) + 0.5f) / height;

            const float w0 = ((v1.x - v2.x) * (py - v2.y) + (v2.y - v1.y) * (px - v2.x)) / area;
            const float w1 = ((v2.x - v0.x) * (py - v0.y) + (v0.y - v2.y) * (px - v0.x)) / area;
            const float w2 = 1.0f - w0 - w1;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                continue;
            }

            const float depth = w0 * v0.depth + w1 * v1.depth + w2 * v2.depth;
            const Color4 shaded = lit ? shade_color(color, ndotl, 0.05f) : color;
            if (color.a < 255) {
                framebuffer.blend_pixel(x, y, shaded, depth);
            } else {
                framebuffer.set_pixel(x, y, shaded, depth);
            }
        }
    }
}

void CPURasterizer::draw_mesh(const Mesh& mesh, const Mat4& mvp, const Camera& camera, Framebuffer& framebuffer,
                              const Color4& base_color, float alpha, bool lit, bool wireframe) const {
    (void)camera;
    for (const Triangle& tri : mesh.triangles) {
        const ProjectedVertex p0 = project_vertex(tri.v0, mvp, tri.normal);
        const ProjectedVertex p1 = project_vertex(tri.v1, mvp, tri.normal);
        const ProjectedVertex p2 = project_vertex(tri.v2, mvp, tri.normal);

        Color4 color = base_color;
        color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);

        if (wireframe) {
            draw_line(tri.v0, tri.v1, mvp, framebuffer, color, 1.0f);
            draw_line(tri.v1, tri.v2, mvp, framebuffer, color, 1.0f);
            draw_line(tri.v2, tri.v0, mvp, framebuffer, color, 1.0f);
        } else {
            rasterize_triangle(p0, p1, p2, framebuffer, color, lit);
        }
    }
}

void CPURasterizer::plot_line(int x0, int y0, int x1, int y1, float depth0, float depth1, Framebuffer& framebuffer,
                              const Color4& color, float width) const {
    const int width_px = framebuffer.width();
    const int height_px = framebuffer.height();
    const int adx = std::abs(x1 - x0);
    const int ady = std::abs(y1 - y0);
    if (adx > width_px * 4 || ady > height_px * 4) {
        return;
    }
    if (depth0 >= 1.0f && depth1 >= 1.0f) {
        return;
    }

    int dx = adx;
    int sx = x0 < x1 ? 1 : -1;
    int dy = -ady;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    const int steps = std::max(dx, -dy);
    int step = 0;
    const int half = std::max(1, static_cast<int>(width * 0.5f));

    while (true) {
        const float t = steps > 0 ? static_cast<float>(step) / static_cast<float>(steps) : 0.0f;
        const float depth = depth0 + (depth1 - depth0) * t;
        for (int oy = -half; oy <= half; ++oy) {
            for (int ox = -half; ox <= half; ++ox) {
                const float dist = std::sqrt(static_cast<float>(ox * ox + oy * oy));
                if (dist > static_cast<float>(half) + 0.5f) {
                    continue;
                }
                Color4 px = color;
                if (half > 1) {
                    const float edge = 1.0f - dist / (static_cast<float>(half) + 0.75f);
                    px.a = static_cast<std::uint8_t>(px.a * std::clamp(edge, 0.0f, 1.0f));
                }
                if (px.a < 255) {
                    framebuffer.blend_pixel(x0 + ox, y0 + oy, px, depth);
                } else {
                    framebuffer.set_pixel(x0 + ox, y0 + oy, px, depth);
                }
            }
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
        ++step;
    }
}

void CPURasterizer::draw_line(const Vec3& a, const Vec3& b, const Mat4& mvp, Framebuffer& framebuffer,
                              const Color4& color, float width) const {
    const ProjectedVertex p0 = project_vertex(a, mvp, Vec3(0.0f, 1.0f, 0.0f));
    const ProjectedVertex p1 = project_vertex(b, mvp, Vec3(0.0f, 1.0f, 0.0f));
    if (p0.depth >= 1.0f || p1.depth >= 1.0f) {
        return;
    }

    const int x0 = static_cast<int>(p0.x * framebuffer.width());
    const int y0 = static_cast<int>(p0.y * framebuffer.height());
    const int x1 = static_cast<int>(p1.x * framebuffer.width());
    const int y1 = static_cast<int>(p1.y * framebuffer.height());

    plot_line(x0, y0, x1, y1, p0.depth, p1.depth, framebuffer, color, width);
}

void CPURasterizer::draw_glow_disc(const Vec3& center, float radius, const Mat4& mvp, Framebuffer& framebuffer,
                                   const Color4& color) const {
    const ProjectedVertex c = project_vertex(center, mvp, Vec3(0.0f, 1.0f, 0.0f));
    if (c.depth >= 1.0f) {
        return;
    }

    const ProjectedVertex edge = project_vertex(center + Vec3(radius, 0.0f, 0.0f), mvp, Vec3(0.0f, 1.0f, 0.0f));
    if (edge.depth >= 1.0f) {
        return;
    }

    const float px_radius =
        std::min(std::abs(edge.x - c.x) * static_cast<float>(framebuffer.width()), 64.0f);

    const int cx = static_cast<int>(c.x * framebuffer.width());
    const int cy = static_cast<int>(c.y * framebuffer.height());
    const int r = static_cast<int>(px_radius);

    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            const float dist = std::sqrt(static_cast<float>(x * x + y * y));
            if (dist > px_radius) {
                continue;
            }
            const float falloff = 1.0f - dist / std::max(px_radius, 1.0f);
            Color4 glow = color;
            glow.a = static_cast<std::uint8_t>(color.a * falloff * falloff);
            framebuffer.blend_pixel(cx + x, cy + y, glow, c.depth);
        }
    }
}

void CPURasterizer::draw_trajectories(Scene& scene, const Camera& camera, const Mat4& mvp,
                                      Framebuffer& framebuffer, float horizon_radius,
                                      const RenderOptions& options) const {
    (void)camera;
    const double playback_time = scene.playback().time;

    for (const Trajectory& traj : scene.trajectories()) {
        if (traj.samples().empty()) {
            continue;
        }

        const std::size_t end_idx = traj.index_at_parameter(playback_time);
        const TrajectoryStyle& style = traj.style();

        if (style.show_trail && end_idx > 0) {
            const std::size_t max_dense = std::max<std::size_t>(1, options.trail_max_dense_segments);
            const std::size_t stride = end_idx > max_dense ? (end_idx / max_dense + 1) : 1;
            const std::size_t tail = options.trail_full_detail_tail;
            const std::size_t full_detail_from = end_idx > tail ? end_idx - tail : 0;
            for (std::size_t i = 1; i <= end_idx; ++i) {
                if (stride > 1 && i < full_detail_from && (i % stride) != 0) {
                    continue;
                }
                const Vec3& a = traj.samples()[i - 1].position;
                const Vec3& b = traj.samples()[i].position;
                if (!outside_horizon_radius(a, horizon_radius) ||
                    !outside_horizon_radius(b, horizon_radius)) {
                    continue;
                }
                const Color4 seg_color = trajectory_segment_color(traj, style, i, end_idx);
                draw_line(a, b, mvp, framebuffer, seg_color, style.line_width);
            }
        }

        if (style.show_marker) {
            const Vec3 marker_pos = traj.samples()[end_idx].position;
            if (!outside_horizon_radius(marker_pos, horizon_radius)) {
                continue;
            }
            Color4 head_color = style.color;
            if (style.gradient_along_path && traj.sample_count() > 1) {
                const double span = traj.max_parameter() - traj.min_parameter();
                const double param = traj.samples()[end_idx].parameter;
                const float gradient_t =
                    span > 1e-12 ? static_cast<float>((param - traj.min_parameter()) / span) : 1.0f;
                head_color = lerp_color(style.gradient_start, style.gradient_end, gradient_t);
            }
            Color4 marker_color = scale_color_rgb(head_color, style.marker_brightness, 1.0f);
            marker_color.a = style.glow_color.a;
            draw_glow_disc(marker_pos, style.marker_radius * style.marker_glow_scale, mvp, framebuffer,
                           marker_color);
        }
    }
}

} // namespace viz
