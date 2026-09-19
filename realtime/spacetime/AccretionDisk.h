#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "scene/ParticleSystem.h"
#include "scene/Particle.h"

namespace Physics {

class AccretionDisk : public ParticleSystem {
public:
    AccretionDisk(int numParticles);

    void update(float deltaTime) override;
    const std::vector<Particle>& getParticles() const override { return m_renderParticles; }

private:
    int m_numParticles;
    std::vector<glm::vec3> m_sphericalCoords;
    std::vector<Particle> m_renderParticles;
};

} // namespace Physics