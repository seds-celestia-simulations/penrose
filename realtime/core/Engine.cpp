#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <random>

#include "core/ShaderManager.h"
#include "core/Window.h"
#include "scene/Camera.h"
#include "core/Texture.h"
#include "render/Renderer.h"
#include "render/GeodesicPass.h"
#include "render/UpscalePass.h"
#include "scene/Particle.h"
#include "core/FrameCapture.h"
#include "core/Framebuffer.h"
#include "spacetime/LutBaker.h"
#include "spacetime/AccretionDisk.h"
#include "scene/ParticleBuffer.h"
#include "core/Engine.h"

Camera camera(glm::vec3(0.5f, 0.0f, 2.0f));
float deltaTime = 0.0f; 
float lastFrame = 0.0f; 
bool firstMouse = true;
float renderScale = 1.0f;

Engine::Engine(unsigned int width, unsigned int height, const std::string& title)
    : width(width), height(height), rs(0.25f)
{
    initWindow(title);
    initAssets();
}

Engine::~Engine() {
    glfwTerminate();
}

void Engine::initWindow(const std::string& title) {
    glfwSetErrorCallback([](int error, const char* description) {
        std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
    });

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        exit(-1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Penrose: RK4 Black Hole", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        exit(-1);

    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    setupMouseControls(window, camera);
    
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        exit(-1);

    }

    std::cout << "========================================" << std::endl;
    std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "GLSL Version:    " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "========================================" << std::endl;

}

void Engine::initAssets() {
    std::cout << "Loading baked LUT from build directory...\n";
    
    // Read the pre-baked binary asset (adjust path if your VS Code runs from a different working directory)
    std::ifstream file("build/assets/schwarzschild_lut.bin", std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        std::cerr << "FATAL: Could not find LUT asset! Check your working directory.\n";
        exit(-1);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<float> lutData(size / sizeof(float));
    file.read(reinterpret_cast<char*>(lutData.data()), size);
    file.close();

    std::cout << "Successfully loaded pre-baked LUT!\n";

    int rw = static_cast<int>(width * renderScale);
    int rh = static_cast<int>(height * renderScale);

    Physics::BakerConfig config; // Just for the lutSize dimensions
    
    glGenTextures(1, &geodesicLUT);
    glBindTexture(GL_TEXTURE_2D, geodesicLUT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, config.lutSize, config.lutSize, 0, GL_RGB, GL_FLOAT, lutData.data()); 
    skyboxTexture = loadTexture("realtime/resources/starfield_original.jpg");

    std::ifstream fileN("build/assets/noise3d.bin", std::ios::binary | std::ios::ate);
    std::streamsize sizeN = fileN.tellg();
    fileN.seekg(0, std::ios::beg);
    std::vector<float> noiseData(sizeN / sizeof(float));
    fileN.read(reinterpret_cast<char*>(noiseData.data()), sizeN);
    fileN.close();

    int noiseRes = 128;
    glGenTextures(1, &noise3DTexture);
    glBindTexture(GL_TEXTURE_3D, noise3DTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload the loaded CPU data straight to the GPU texture
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, noiseRes, noiseRes, noiseRes, 0, GL_RED, GL_FLOAT, noiseData.data());

    std::cout << "Successfully loaded pre-baked 3D Noise!\n";

// 1. Initialize Renderer WITH DIMENSIONS for the compute texture
    renderer = std::make_unique<Renderer>(rw, rh);
    
    // 2. Load the universal screen blit shader
    screenShader = std::make_unique<Shader>("realtime/shaders/common/screen.vert", "realtime/shaders/common/screen.frag");

    shaderManager = std::make_unique<ShaderManager>();
    shaderManager->setBasePath("realtime/shaders");
    shaderManager->loadMetricCompute(MetricType::SCHWARZSCHILD_REDUCED, "reduced.comp");
    shaderManager->setMetric(MetricType::SCHWARZSCHILD_REDUCED);

    auto activeShader = shaderManager->getActive();
    if (activeShader) {
        activeShader->use();
        activeShader->setInt("skybox", 0);
        activeShader->setInt("uGeodesicLUT", 1);
        
        activeShader->setFloat("uLutRMin", config.rMin);
        activeShader->setFloat("uLutRMax", config.rMax);
    }

    frameCapture = std::make_unique<FrameCapture>();
    accretionDisk = std::make_unique<Physics::AccretionDisk>(50);
    fallingSystem = std::make_unique<FallingParticleSystem>();
    fallingSystem->setRs(rs);

    particleSystems.push_back(accretionDisk.get());
    particleSystems.push_back(fallingSystem.get());

    passes.push_back(std::make_unique<GeodesicPass>(*renderer, *shaderManager));
    passes.push_back(std::make_unique<UpscalePass>(*renderer)); 
}

void Engine::update() {
    for (auto* ps : particleSystems) {
        ps->update(deltaTime);
    }

    std::vector<Particle> renderPayload;
    for (auto* ps : particleSystems) {
        const auto& particles = ps->getParticles();
        renderPayload.insert(renderPayload.end(), particles.begin(), particles.end());
    }
 //   renderer->updateParticles(renderPayload);
}

void Engine::render() {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    float currentFrame = (float)glfwGetTime();
    int rw = static_cast<int>(w * renderScale);
    int rh = static_cast<int>(h * renderScale);

    PassContext ctx {
        camera,
        currentFrame,
        w, h,
        rw, rh,
        skyboxTexture,
        geodesicLUT,
        noise3DTexture,
        screenShader.get()
    };

    for (auto& pass : passes) {
        pass->execute(ctx);
    }
}

void Engine::run() {
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        static bool escapeWasDown = false;
        processInput(window, camera, *frameCapture);
        bool escapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escapeDown && !escapeWasDown) {
            static bool mouseCaptured = true;
            mouseCaptured = !mouseCaptured;

            glfwSetInputMode(window,GLFW_CURSOR,mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
            );

            firstMouse = true;
        }
        escapeWasDown = escapeDown;
        static bool periodWasDown = false;
        bool periodDown = glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS;

        if (periodDown && !periodWasDown) {
            fallingSystem->spawnParticle(camera.Position); 
            std::cout << "Dynamic test sphere dropped into spherical coordinates pipeline!\n";
        }
        periodWasDown = periodDown;

        update();
        render();

        static double lastFPSTime = glfwGetTime();
        static int frames = 0;
        frames++;
        if (glfwGetTime() - lastFPSTime >= 1.0) {
            std::cout << "FPS: " << frames << "\n";
            frames = 0;
            lastFPSTime = glfwGetTime();
        }

        if (frameCapture->getIsCapturing()) {
            std::string filePath = frameCapture->getNextFilePath();
            renderer->captureFrame(filePath, window);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}
