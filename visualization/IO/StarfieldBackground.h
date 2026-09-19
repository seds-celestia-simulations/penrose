#pragma once

#include "../Geometry/Math.h"

#include <filesystem>
#include <vector>

namespace viz {

// Equirectangular starfield sourced from realtime/resources/starfield_original.jpg.
class StarfieldBackground {
public:
    static const StarfieldBackground& instance();

    bool loaded() const { return !pixels_.empty(); }
    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<std::uint8_t>& rgb_pixels() const { return pixels_; }

    Color4 sample_direction(const Vec3& direction, float brightness = 1.0f) const;

    static std::filesystem::path default_image_path();

private:
    StarfieldBackground();

    std::vector<std::uint8_t> pixels_;
    int width_ = 0;
    int height_ = 0;
    int channels_ = 0;
};

} // namespace viz
