#include "StarfieldBackground.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "../../vendor/stb_image.h"

#ifndef PENROSE_SOURCE_DIR
#define PENROSE_SOURCE_DIR "."
#endif

namespace viz {

namespace {

constexpr float kPi = 3.14159265358979323846f;

} // namespace

std::filesystem::path StarfieldBackground::default_image_path() {
    return std::filesystem::path(PENROSE_SOURCE_DIR) / "visualization/resources/starfield_original.jpg";
}

StarfieldBackground::StarfieldBackground() {
    const std::filesystem::path path = default_image_path();
    int w = 0;
    int h = 0;
    int channels = 0;
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &channels, 3);
    if (data == nullptr) {
        std::cerr << "[viz] Starfield image not found: " << path << "\n";
        return;
    }

    width_ = w;
    height_ = h;
    channels_ = 3;
    pixels_.assign(data, data + static_cast<std::size_t>(w * h * 3));
    stbi_image_free(data);
    std::cout << "[viz] Loaded starfield: " << path << " (" << width_ << "x" << height_ << ")\n";
}

const StarfieldBackground& StarfieldBackground::instance() {
    static const StarfieldBackground background;
    return background;
}

Color4 StarfieldBackground::sample_direction(const Vec3& direction, float brightness) const {
    if (!loaded()) {
        return Color4::rgb(0, 0, 0);
    }

    const Vec3 dir = direction.normalized();
    const float phi = std::atan2(dir.z, dir.x);
    const float theta = std::acos(std::clamp(dir.y, -1.0f, 1.0f));

    float u = (phi + kPi) / (2.0f * kPi);
    float v = theta / kPi;
    u = u - std::floor(u);
    v = std::clamp(v, 0.0f, 1.0f);

    const float fx = u * static_cast<float>(width_ - 1);
    const float fy = v * static_cast<float>(height_ - 1);
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = std::min(x0 + 1, width_ - 1);
    const int y1 = std::min(y0 + 1, height_ - 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    auto at = [&](int x, int y) {
        const std::size_t idx = static_cast<std::size_t>((y * width_ + x) * 3);
        return Color4::rgb(pixels_[idx], pixels_[idx + 1], pixels_[idx + 2]);
    };

    const Color4 c00 = at(x0, y0);
    const Color4 c10 = at(x1, y0);
    const Color4 c01 = at(x0, y1);
    const Color4 c11 = at(x1, y1);

    auto lerp = [](const Color4& a, const Color4& b, float t) {
        return Color4::from_float(a.r / 255.0f + (b.r / 255.0f - a.r / 255.0f) * t,
                                  a.g / 255.0f + (b.g / 255.0f - a.g / 255.0f) * t,
                                  a.b / 255.0f + (b.b / 255.0f - a.b / 255.0f) * t, 1.0f);
    };

    const Color4 cx0 = lerp(c00, c10, tx);
    const Color4 cx1 = lerp(c01, c11, tx);
    const Color4 c = lerp(cx0, cx1, ty);

    const float b = std::max(0.0f, brightness);
    return Color4::from_float(std::min(c.r / 255.0f * b, 1.0f), std::min(c.g / 255.0f * b, 1.0f),
                              std::min(c.b / 255.0f * b, 1.0f), 1.0f);
}

} // namespace viz
