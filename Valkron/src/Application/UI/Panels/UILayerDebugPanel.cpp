#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawDebugPanel() {
        if (!ImGui::Begin("Performance", &m_showDebugPanel)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        const float frameMs = m_lastFrameDeltaTimeSeconds * 1000.0f;
        const float fps = m_lastFrameDeltaTimeSeconds > 0.0f ? (1.0f / m_lastFrameDeltaTimeSeconds) : 0.0f;
        const auto& entities = m_activeScene.getEntityData();

        ImGui::Text("Performance");
        ImGui::Separator();
        ImGui::Text("Frame Rate (s)    : %.2f FPS", fps);
        ImGui::Text("CPU Time (ms)     : %.2f", frameMs);
        ImGui::Text("Scene State       : %s", sceneStateToString(m_activeScene.getState()));
        ImGui::Text("Viewport          : %dx%d", Renderer::getViewportWidth(), Renderer::getViewportHeight());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Memory");
        ImGui::Text("Tracked Assets    : %d", static_cast<int>(m_activeScene.getAssets().size()));
        ImGui::Text("Texture2D         : %d", static_cast<int>(AssetLoader::getTexture2DNames().size()));
        ImGui::Text("Texture3D         : %d", static_cast<int>(AssetLoader::getTexture3DNames().size()));
        ImGui::Text("Models            : %d", static_cast<int>(AssetLoader::getModelNames().size()));
        ImGui::Text("Log Lines         : %d", static_cast<int>(m_terminalLines.size()));

        ImGui::Spacing();
        ImGui::Text("Scene Stats");
        ImGui::Separator();
        ImGui::Text("Entities          : %d", static_cast<int>(entities.size()));
        ImGui::Text("Shaders           : %d", static_cast<int>(AssetLoader::getShaderNames().size()));
        ImGui::Text("Compute Shaders   : %d", static_cast<int>(AssetLoader::getComputeShaderNames().size()));
        ImGui::Text("Frame Texture ID  : %u", Renderer::getFrameTextureID());

        ImGui::End();
    }


}
