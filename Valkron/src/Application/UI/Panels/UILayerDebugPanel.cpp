#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawDebugPanel() {
        if (!ImGui::Begin("Debug", &m_showDebugPanel)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        const float frameMs = m_lastFrameDeltaTimeSeconds * 1000.0f;
        const float fps = m_lastFrameDeltaTimeSeconds > 0.0f ? (1.0f / m_lastFrameDeltaTimeSeconds) : 0.0f;
        const auto& entities = m_activeScene.getEntityData();

        ImGui::Text("Runtime");
        ImGui::Separator();
        ImGui::BulletText("Frame: %.2f ms", frameMs);
        ImGui::BulletText("FPS: %.1f", fps);
        ImGui::BulletText("Scene State: %s", sceneStateToString(m_activeScene.getState()));
        ImGui::BulletText("Entities: %d", static_cast<int>(entities.size()));
        ImGui::BulletText("Assets: %d", static_cast<int>(m_activeScene.getAssets().size()));
        ImGui::BulletText("Viewport: %dx%d", Renderer::getViewportWidth(), Renderer::getViewportHeight());
        ImGui::BulletText("Frame Texture ID: %u", Renderer::getFrameTextureID());

        ImGui::Spacing();
        ImGui::Text("Selection");
        ImGui::Separator();
        if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size())) {
            const SceneEntity& selectedEntity = entities[static_cast<std::size_t>(m_selectedEntityIndex)];
            ImGui::BulletText("Index: %d", m_selectedEntityIndex);
            ImGui::BulletText("Name: %s", selectedEntity.name.c_str());
            ImGui::BulletText("Parent Index: %d", selectedEntity.parentIndex);
            ImGui::BulletText("Position: %.2f %.2f %.2f", selectedEntity.transform.position.x, selectedEntity.transform.position.y, selectedEntity.transform.position.z);
        } else {
            ImGui::TextDisabled("No selected entity.");
        }

        ImGui::Spacing();
        ImGui::Text("AssetLoader");
        ImGui::Separator();
        ImGui::BulletText("Texture2D: %d", static_cast<int>(AssetLoader::getTexture2DNames().size()));
        ImGui::BulletText("Texture3D: %d", static_cast<int>(AssetLoader::getTexture3DNames().size()));
        ImGui::BulletText("Shaders: %d", static_cast<int>(AssetLoader::getShaderNames().size()));
        ImGui::BulletText("Compute Shaders: %d", static_cast<int>(AssetLoader::getComputeShaderNames().size()));
        ImGui::BulletText("Models: %d", static_cast<int>(AssetLoader::getModelNames().size()));
        ImGui::BulletText("Log Lines: %d", static_cast<int>(m_terminalLines.size()));

        ImGui::End();
    }


}
