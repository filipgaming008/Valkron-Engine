#pragma once

#include "Core/Core.hpp"
#include "Renderer/API/RendererAPI.hpp"

#include <memory>

namespace Valkron {

class VALKRON_API RenderCommand {
public:
    static void init();
    static void setViewport(int x, int y, int width, int height);
    static void setClearColor(float red, float green, float blue, float alpha);
    static void clear();

private:
    static std::unique_ptr<RendererAPI> s_rendererAPI;
};

} // namespace Valkron