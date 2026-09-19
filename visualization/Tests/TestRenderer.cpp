#include "TestDeclarations.h"
#include "../Camera/Camera.h"
#include "../IO/PpmWriter.h"
#include "../Renderer/CPURasterizer.h"
#include "../Scene/SceneBuilder.h"

#include <cstdint>
#include <fstream>

namespace viz::test {

namespace {

std::uint64_t checksum_framebuffer(const Framebuffer& fb) {
    std::uint64_t sum = 0;
    for (const Color4& c : fb.color_buffer()) {
        sum = sum * 1315423911u + c.r + (static_cast<std::uint64_t>(c.g) << 8) +
              (static_cast<std::uint64_t>(c.b) << 16);
    }
    return sum;
}

} // namespace

bool test_deterministic_render_checksum() {
    Scene scene = SceneBuilder::default_scene();
    Camera camera;
    Framebuffer fb(160, 120);
    CPURasterizer rasterizer;

    rasterizer.render(scene, camera, fb);
    const std::uint64_t a = checksum_framebuffer(fb);
    rasterizer.render(scene, camera, fb);
    const std::uint64_t b = checksum_framebuffer(fb);
    return a == b && a != 0;
}

bool test_golden_ppm_roundtrip() {
    Scene scene = SceneBuilder::default_scene();
    Camera camera;
    Framebuffer fb(64, 64);
    CPURasterizer rasterizer;
    rasterizer.render(scene, camera, fb);

    const std::string path = "/tmp/penrose_viz_golden.ppm";
    if (!write_ppm(fb, path)) {
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    std::string magic;
    in >> magic;
    return magic == "P6";
}

} // namespace viz::test
