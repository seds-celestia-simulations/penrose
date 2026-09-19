#pragma once

#include "../Geometry/Math.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace viz {

struct Star {
    Vec3 direction;
    float brightness;
    float w;
};

class StarfieldGenerator {
public:
    static std::vector<Star> generate_stars(std::uint32_t seed, int count) {
        std::vector<Star> stars;
        stars.reserve(count);
        for (int i = 0; i < count; ++i) {
            const float u = hash_unit(seed, i * 3 + 0);
            const float v = hash_unit(seed, i * 3 + 1);
            const float w = hash_unit(seed, i * 3 + 2);

            const float theta = u * 3.14159265359f * 2.0f;
            const float phi = std::acos(2.0f * v - 1.0f);
            const Vec3 dir(std::sin(phi) * std::cos(theta), std::sin(phi) * std::sin(theta), std::cos(phi));
            
            const float brightness = 0.35f + 0.65f * w;
            stars.push_back({dir, brightness, w});
        }
        return stars;
    }

private:
    static std::uint32_t hash_u32(std::uint32_t x) {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    static float hash_unit(std::uint32_t seed, int i) {
        return static_cast<float>(hash_u32(seed + static_cast<std::uint32_t>(i)) & 0xFFFFFFu) /
               static_cast<float>(0xFFFFFFu);
    }
};

} // namespace viz
