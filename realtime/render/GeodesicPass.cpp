#include "render/GeodesicPass.h"
#include "render/Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

GeodesicPass::GeodesicPass(Renderer& renderer, ShaderManager& shaderManager)
    : m_renderer(renderer), m_shaderManager(shaderManager) {}

void GeodesicPass::execute(const PassContext& ctx) {
    Shader* shader = m_shaderManager.getActive();
    if (!shader) return;

    // 1. Setup Compute Shader State
    shader->use();

    shader->setBool("uHighQualityPass", false);
    shader->setFloat("uHighQualityRadius", 0.45f);
    shader->setFloat("uTime", ctx.currentFrame);
    shader->setVec2("uResolution", glm::vec2(ctx.renderWidth, ctx.renderHeight));
    shader->setVec3("uCameraPos", ctx.camera.Position);
    shader->setFloat("uLutRMin", 0.25f * 1.001f); 
    shader->setFloat("uLutRMax", 50.0f);
    
    shader->setInt("uGeodesicLUT", 1);
    shader->setInt("uNoise3D", 2);

    glm::mat4 view = ctx.camera.GetViewMatrix();
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        float(ctx.renderWidth) / float(ctx.renderHeight),
        0.1f, 100.0f
    );
    glm::mat4 invProjView = glm::inverse(projection * view);
    glUniformMatrix4fv(
        glGetUniformLocation(shader->ID, "uInvProjView"),
        1, GL_FALSE, &invProjView[0][0]
    );

    // 2. Bind Read-Only Resources
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx.skyboxTexture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, ctx.geodesicLUT);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, ctx.noise3DTexture);

    m_renderer.bindParticleBuffer(2);
    shader->setInt("uParticleCount", int(m_renderer.getParticleCount()));

    // 3. DISPATCH COMPUTE
    m_renderer.bindComputeImage(0); // Tell renderer to bind its texture for writing
    
    unsigned int numGroupsX = (ctx.renderWidth + 15) / 16;
    unsigned int numGroupsY = (ctx.renderHeight + 15) / 16;
    glDispatchCompute(numGroupsX, numGroupsY, 1);
    
    // Ensure the GPU finishes writing before we try to read it
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}