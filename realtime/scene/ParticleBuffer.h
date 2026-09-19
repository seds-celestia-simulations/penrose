#pragma once

#include <glad/glad.h>
#include <vector>
#include "Particle.h"
#include "ParticleSystem.h"
#include <glm/glm.hpp>
#include <random>

class ParticleBuffer {
private:
    unsigned int SSBO;
    size_t particleCount;
    
public:
    ParticleBuffer();
    ~ParticleBuffer();
    
    void uploadParticles(const std::vector<Particle>& particles);
    void bind(unsigned int bindingPoint = 0);
    size_t getParticleCount() const { return particleCount; }
};

class FallingParticleSystem : public ParticleSystem {
public:
    void setRs(float rs) { m_rs = rs; }

    void update(float deltaTime) override {
        float GM = m_rs * 0.5f;

        for (auto it = m_particles.begin(); it != m_particles.end(); ) {
            glm::vec3 pos = glm::vec3(it->stateX);
            glm::vec3 vel = glm::vec3(it->stateU);
            float r = glm::length(pos);

            if (r <= m_rs * 1.001f) {
                it = m_particles.erase(it);
                continue;
            }

            glm::vec3 acceleration = -(GM / (r * r * r)) * pos;
            vel += acceleration * deltaTime;
            pos += vel * deltaTime;

            it->stateX = glm::vec4(pos, r);
            it->stateU = glm::vec4(vel, 0.0f);

            ++it;
        }
    }

    void spawnParticle(glm::vec3 cameraPosition) {
        Particle p;

        glm::vec3 spawnPos = glm::vec3(0.0f, 0.5f, 1.0f);
        
        p.stateX = glm::vec4(spawnPos, glm::length(spawnPos));
        p.stateU = glm::vec4(0.5f, 0.0f, -0.1f, 0.0f);
        p.color = glm::vec3(0.0f, 0.0f, 1.0f);
        p.mass = 1.0f;
        p.radius = 0.3f;
        p._pad0 = 0.0f;
        p._pad1 = 0.0f;
        p._pad2 = 0.0f;

        m_particles.push_back(p);
    }

    const std::vector<Particle>& getParticles() const override { return m_particles; }

private:
    std::vector<Particle> m_particles;
    float m_rs = 0.25f;
};