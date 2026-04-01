#pragma once

#include "Application/UI/UIElement.hpp"
#include "Core/Core.hpp"
#include "Renderer/Camera.hpp"

#include <cstdint>
#include <vector>

namespace Valkron {

class VALKRON_API Renderer {
public:
    static void init();
    static void shutdown();
    static void beginFrame();
    static void endFrame();
    static void submitUIBatch(const std::vector<UIVertex>& vertices, const std::vector<std::uint32_t>& indices,
                              const std::vector<UIDrawCommand>& commands);
    static void setCameraType(CameraType type);
    static void onWindowResize(int width, int height);
};

} // namespace Valkron