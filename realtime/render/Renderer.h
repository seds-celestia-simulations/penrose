#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

#include "core/Shader.h"
#include "scene/Camera.h"
#include "scene/ParticleBuffer.h"

class Renderer {
private:
    unsigned int dummyVAO;
    unsigned int computeOutputTexture;
    ParticleBuffer particleBuffer;

public:
    Renderer(int width, int height);
    ~Renderer();

    void updateParticles(const std::vector<Particle>& particles);
    void bindParticleBuffer(unsigned int bindingPoint);
    size_t getParticleCount() const;

    bool captureFrame(const std::string& filePath, GLFWwindow* window);
    void bindComputeImage(unsigned int unit);
    void blitToScreen();
};
