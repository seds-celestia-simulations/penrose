#include "LutBaker.h"
#include <cmath>
#include <iostream>

namespace Physics {

// Internal helper for the geodesic differential equations
static void orbitDerivative(float u, float v, float rs, float& du, float& dv) {
    du = v;
    dv = -u + 1.5f * rs * u * u;
}

// Internal helper for the RK4 integration step
static void orbitRK4(float& u, float& v, float h, float rs) {
    float k1u, k1v, k2u, k2v, k3u, k3v, k4u, k4v;
    orbitDerivative(u, v, rs, k1u, k1v);
    
    orbitDerivative(u + 0.5f * h * k1u, v + 0.5f * h * k1v, rs, k2u, k2v);
    orbitDerivative(u + 0.5f * h * k2u, v + 0.5f * h * k2v, rs, k3u, k3v);
    orbitDerivative(u + h * k3u, v + h * k3v, rs, k4u, k4v);
    
    u += (h / 6.0f) * (k1u + 2.0f * k2u + 2.0f * k3u + k4u);
    v += (h / 6.0f) * (k1v + 2.0f * k2v + 2.0f * k3v + k4v);
}

std::vector<float> LutBaker::bakeSchwarzschildLUT(const BakerConfig& config) {
    std::vector<float> lutData(config.lutSize * config.lutSize * 3, 0.0f);

    const float gamma_min = 0.001f;
    const float gamma_max = 3.14159265359f - 0.001f;

    for (int y = 0; y < config.lutSize; y++) {
        float gamma = gamma_min + (gamma_max - gamma_min) * (float(y) / (config.lutSize - 1));

        for (int x = 0; x < config.lutSize; x++) {
            float r_start = config.rMin + (config.rMax - config.rMin) * (float(x) / (config.lutSize - 1));

            // Initialize ODE state
            float u = 1.0f / r_start;
            float v = -std::cos(gamma) / (r_start * std::sin(gamma));

            float psi = 0.0f;
            bool captured = false;
            float min_r = r_start;
            
            // Step forward until capture or escape
            for (int i = 0; i < config.maxSteps; ++i) {
                orbitRK4(u, v, config.dPsi, config.rs);
                psi += config.dPsi;
                
                if (u <= 0.0f) { 
                    break;
                }
                if (u > 0.0f) { // closest approach
                    min_r = std::min(min_r, 1.0f / u);
                }
                if (u < 1.0f / config.rMax && v < 0.0f) {
                    break;
                }
            }

            // Record the data
            float deltaPhi = captured ? -1.0f : psi;
            
            int index = (y * config.lutSize + x) * 3;
            lutData[index + 0] = deltaPhi;  // RED channel
            lutData[index + 1] = min_r;     // GREEN channel placeholder
            lutData[index + 2] = 0.0f;     // BLUE channel placeholder
        }
    }

    return lutData;
}

} 