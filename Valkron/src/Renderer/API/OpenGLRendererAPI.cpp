#include "Renderer/API/OpenGLRendererAPI.hpp"
#include "glad/gl.h"

namespace Valkron {

void OpenGLRendererAPI::init() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void OpenGLRendererAPI::setViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void OpenGLRendererAPI::setClearColor(float red, float green, float blue, float alpha) {
    glClearColor(red, green, blue, alpha);
}

void OpenGLRendererAPI::clear() {
    glClear(GL_COLOR_BUFFER_BIT);
}

} // namespace Valkron