#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <memory>
#include <vector>

#include "render/Renderer.h"
#include "render/RenderPass.h"
#include "core/ShaderManager.h"
#include "core/FrameCapture.h"
#include "scene/ParticleBuffer.h"
#include "scene/ParticleSystem.h"
#include "spacetime/AccretionDisk.h"

class Engine {
public:
    Engine(unsigned int width, unsigned int height, const std::string& title);
    ~Engine();

    void run();

private:
    void initWindow(const std::string& title);
    void initAssets();
    void update();
    void render();

    std::unique_ptr<Shader> screenShader;

    GLFWwindow* window;
    unsigned int width;
    unsigned int height;
    float rs;

    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<ShaderManager> shaderManager;
    std::unique_ptr<FrameCapture> frameCapture;

    std::unique_ptr<Physics::AccretionDisk> accretionDisk;
    std::unique_ptr<FallingParticleSystem> fallingSystem;
    std::vector<ParticleSystem*> particleSystems;

    std::vector<std::unique_ptr<RenderPass>> passes;

    unsigned int skyboxTexture;
    unsigned int geodesicLUT;
    unsigned int noise3DTexture;
};
