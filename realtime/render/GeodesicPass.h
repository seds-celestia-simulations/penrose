#pragma once

#include "render/RenderPass.h"
#include "core/ShaderManager.h"

class Renderer;

class GeodesicPass : public RenderPass {
public:
    GeodesicPass(Renderer& renderer, ShaderManager& shaderManager);
    void execute(const PassContext& ctx) override;
    const std::string& name() const override { return m_name; }

private:
    Renderer& m_renderer;
    ShaderManager& m_shaderManager;
    std::string m_name = "GeodesicPass";
};
