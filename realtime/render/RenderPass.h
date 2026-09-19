#pragma once
#include <string>
#include "scene/Camera.h"
#include "core/Shader.h"

struct PassContext {
    Camera& camera;
    float currentFrame;
    int windowWidth, windowHeight; 
    int renderWidth, renderHeight;
    unsigned int skyboxTexture;
    unsigned int geodesicLUT;
    unsigned int noise3DTexture;
    Shader* screenShader;
};

class RenderPass {
public:
    virtual ~RenderPass() = default;

    virtual void execute(const PassContext& ctx) = 0;
    virtual const std::string& name() const = 0;
};
