#include "AccretionDisk.h"
#include <random>
#include <cmath>

namespace Physics {

AccretionDisk::AccretionDisk(int numParticles)
    : m_numParticles(numParticles), m_renderParticles(numParticles), m_sphericalCoords(numParticles)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    const float PI = 3.14159265359f;

    std::uniform_real_distribution<float> radDist(3.0f, 15.0f);
    std::uniform_real_distribution<float> phiDist(0.0f, 2.0f * PI);
    std::normal_distribution<float> thetaDist(PI / 2.0f, 0.05f);

    for (int i = 0; i < m_numParticles; i++) {
        float r = radDist(gen);
        float theta = thetaDist(gen);
        float phi = phiDist(gen);

        m_sphericalCoords[i] = glm::vec3(r, theta, phi);

        float x = r * std::sin(theta) * std::cos(phi);
        float y = r * std::sin(theta) * std::sin(phi);
        float z = r * std::cos(theta);

        m_renderParticles[i].stateX = glm::vec4(x, y, z, r);

        float heat = 1.0f - ((r - 3.0f) / 12.0f);
        m_renderParticles[i].color = glm::vec3(1.0f, heat * 0.8f, heat * 0.2f);
        m_renderParticles[i].mass = 1.0f;
        m_renderParticles[i].radius = 0.03f;
        m_renderParticles[i]._pad0 = 0.0f;
        m_renderParticles[i]._pad1 = 0.0f;
        m_renderParticles[i]._pad2 = 0.0f;
    }
}

void AccretionDisk::update(float deltaTime) {
    for (int i = 0; i < m_numParticles; i++) {
        float r = m_sphericalCoords[i].x;
        float theta = m_sphericalCoords[i].y;
        float phi = m_sphericalCoords[i].z;

        float orbitSpeed = 2.0f / std::pow(r, 1.5f);
        phi += orbitSpeed * deltaTime;

        m_sphericalCoords[i].z = phi;

        m_renderParticles[i].stateX.x = r * std::sin(theta) * std::cos(phi);
        m_renderParticles[i].stateX.y = r * std::sin(theta) * std::sin(phi);
        m_renderParticles[i].stateX.z = r * std::cos(theta);
    }
}

} // namespace Physics
