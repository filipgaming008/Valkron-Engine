#pragma once

#include "Application/UI/UIElement.hpp"
#include "Core/Core.hpp"

#include "glm/glm.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Valkron {

    class VALKRON_API UIRenderer {
        public:
            static void init();
            static void shutdown();
            static void submitBatch(const std::vector<UIVertex>& vertices, const std::vector<std::uint32_t>& indices);
            static void render(int viewportWidth, int viewportHeight);

            static void appendTextGeometry(
                const std::string& text,
                const glm::vec2& position,
                float pixelHeight,
                const glm::vec4& color,
                std::vector<UIVertex>& outVertices,
                std::vector<std::uint32_t>& outIndices
            );
            static float getLineHeight(float pixelHeight);
            static float measureTextWidth(const std::string& text, float pixelHeight);
    };

}
