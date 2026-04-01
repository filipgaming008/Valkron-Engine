#pragma once

#include "Renderer/API/RendererAPI.hpp"

namespace Valkron {

class VALKRON_API OpenGLRendererAPI : public RendererAPI {
public:
    void init() override;
    void setViewport(int x, int y, int width, int height) override;
    void setClearColor(float red, float green, float blue, float alpha) override;
    void clear() override;
};

} // namespace Valkron