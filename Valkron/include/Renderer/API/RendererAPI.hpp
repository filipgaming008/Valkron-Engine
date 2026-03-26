#pragma once

#include "Core/Core.hpp"

namespace Valkron {

    enum class RendererAPIType {
        None = 0,
        OpenGL = 1
    };

    class VALKRON_API RendererAPI {
        public:
            virtual ~RendererAPI() = default;

            virtual void init() = 0;
            virtual void setViewport(int x, int y, int width, int height) = 0;
            virtual void setClearColor(float red, float green, float blue, float alpha) = 0;
            virtual void clear() = 0;

            static RendererAPIType getAPI() {
                return s_api;
            }

        protected:
            static RendererAPIType s_api;
    };

}