#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include "LutBaker.h"

// --- Fast C++ 3D Noise ---
float hash(float x, float y, float z) {
    float dot = x * 123.34f + y * 456.21f + z * 789.12f;
    dot += (dot * dot + 45.32f);
    float h = std::sin(dot) * 43758.5453f;
    return h - std::floor(h);
}

float mix(float x, float y, float a) { return x * (1.0f - a) + y * a; }

float valueNoise(float x, float y, float z) {
    float ix = std::floor(x), iy = std::floor(y), iz = std::floor(z);
    float fx = x - ix, fy = y - iy, fz = z - iz;
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    float uz = fz * fz * (3.0f - 2.0f * fz);

    float n000 = hash(ix,iy,iz); float n100 = hash(ix+1,iy,iz);
    float n010 = hash(ix,iy+1,iz); float n110 = hash(ix+1,iy+1,iz);
    float n001 = hash(ix,iy,iz+1); float n101 = hash(ix+1,iy,iz+1);
    float n011 = hash(ix,iy+1,iz+1); float n111 = hash(ix+1,iy+1,iz+1);

    return mix(mix(mix(n000, n100, ux), mix(n010, n110, ux), uy),
               mix(mix(n001, n101, ux), mix(n011, n111, ux), uy), uz);
}

std::vector<float> bakeNoise3D(int res) {
    std::vector<float> data(res * res * res);
    for(int z = 0; z < res; ++z) {
        for(int y = 0; y < res; ++y) {
            for(int x = 0; x < res; ++x) {
                float px = x * 0.15f, py = y * 0.15f, pz = z * 0.15f;
                float val = 0.0f, amp = 0.5f, freq = 1.0f;
                for(int i = 0; i < 4; ++i) {
                    val += amp * valueNoise(px*freq, py*freq, pz*freq);
                    freq *= 2.0f; amp *= 0.5f;
                }
                data[x + y*res + z*res*res] = val;
            }
        }
    }
    return data;
}
// -------------------------

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: PenroseBaker <lut_path> <noise_path>\n";
        return 1;
    }
    
    std::string lutPath = argv[1];
    std::string noisePath = argv[2];

    // 1. Bake LUT
    Physics::BakerConfig config; config.rs = 0.25f; config.rMin = config.rs * 1.001f;
    std::vector<float> lutData = Physics::LutBaker::bakeSchwarzschildLUT(config);
    std::ofstream outLut(lutPath, std::ios::binary);
    outLut.write(reinterpret_cast<const char*>(lutData.data()), lutData.size() * sizeof(float));
    outLut.close();

    // 2. Bake 3D Noise
    int noiseRes = 128;
    std::vector<float> noiseData = bakeNoise3D(noiseRes);
    std::ofstream outNoise(noisePath, std::ios::binary);
    outNoise.write(reinterpret_cast<const char*>(noiseData.data()), noiseData.size() * sizeof(float));
    outNoise.close();

    std::cout << "Bake complete!\n";
    return 0;
}