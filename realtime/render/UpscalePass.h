#pragma once

#include "render/RenderPass.h"

class Renderer;

class UpscalePass : public RenderPass {
public:
    UpscalePass(Renderer& renderer);
    void execute(const PassContext& ctx) override;
    const std::string& name() const override { return m_name; }

private:
    Renderer& m_renderer;
    std::string m_name = "UpscalePass";
};
