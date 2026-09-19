#include "render/Renderer.h"
#include <iostream>
#include <fstream>

Renderer::Renderer(int width, int height) {
    glGenTextures(1, &computeOutputTexture);
    glBindTexture(GL_TEXTURE_2D, computeOutputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenVertexArrays(1, &dummyVAO);
}

Renderer::~Renderer() {
    glDeleteTextures(1, &computeOutputTexture);
    glDeleteVertexArrays(1, &dummyVAO);
}

void Renderer::updateParticles(const std::vector<Particle>& particles) {
    // particleBuffer.uploadParticles(particles);
}

void Renderer::bindParticleBuffer(unsigned int bindingPoint) {
    particleBuffer.bind(bindingPoint);
}

size_t Renderer::getParticleCount() const {
    return particleBuffer.getParticleCount();
}

void Renderer::bindComputeImage(unsigned int unit) {
    glBindImageTexture(unit, computeOutputTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
}

bool Renderer::captureFrame(const std::string& filePath, GLFWwindow* window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    std::vector<unsigned char> pixels(width * height * 3);

    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filePath << std::endl;
        return false;
    }

    file << "P6\n";
    file << width << " " << height << "\n";
    file << "255\n";

    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 3;
            file.write(reinterpret_cast<const char*>(pixels.data() + idx), 3);
        }
    }

    return true;
}

void Renderer::blitToScreen() {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, computeOutputTexture);

    glBindVertexArray(dummyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
