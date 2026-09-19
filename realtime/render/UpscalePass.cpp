#include "render/UpscalePass.h"
#include "render/Renderer.h"

UpscalePass::UpscalePass(Renderer& renderer) : m_renderer(renderer) {}

void UpscalePass::execute(const PassContext& ctx) {
    // 1. Set viewport to the full physical monitor size
    glViewport(0, 0, ctx.windowWidth, ctx.windowHeight);
    
    // 2. Activate the screen/upscale shader
    ctx.screenShader->use();
    
    // Pass the low-res dimensions so the shader knows how thick the pixels are
    ctx.screenShader->setVec2("uTexSize", glm::vec2(ctx.renderWidth, ctx.renderHeight));
    
    // 3. Blit the sharpened image to the screen
    m_renderer.blitToScreen();
}