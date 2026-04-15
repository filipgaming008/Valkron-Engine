#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawGameViewPanel(float deltaTime) {
        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!ImGui::Begin("Game View", &m_showGameViewPanel, windowFlags)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        const unsigned int frameTextureID = Renderer::getGameFrameTextureID();
        const int renderWidth = Renderer::getViewportWidth();
        const int renderHeight = Renderer::getViewportHeight();

        const auto& entities = m_activeScene.getEntityData();
        std::string primaryPlayCameraName = m_activeScene.getGameStateValue("PrimaryCameraEntity").value_or("");
        const bool primaryPlayCameraExists = std::any_of(entities.begin(), entities.end(), [&](const SceneEntity& entity) {
            return entity.type == SceneEntityType::Camera && entity.name == primaryPlayCameraName;
        });

        if (!primaryPlayCameraExists) {
            for (const SceneEntity& entity : entities) {
                if (entity.type == SceneEntityType::Camera) {
                    primaryPlayCameraName = entity.name;
                    break;
                }
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(9, 13, 20, 215));
        if (ImGui::BeginChild("GameViewHeader", ImVec2(0.0f, 64.0f), true, ImGuiWindowFlags_NoScrollbar)) {
            const float fps = deltaTime > 0.0f ? (1.0f / deltaTime) : 0.0f;
            ImGui::TextUnformatted("Game View");
            ImGui::Separator();
            ImGui::Text("%s | FPS %.1f", sceneStateToString(m_activeScene.getState()), fps);
            ImGui::TextDisabled("Main Camera: %s", primaryPlayCameraName.empty() ? "None" : primaryPlayCameraName.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
        if (frameTextureID == 0 || renderWidth <= 0 || renderHeight <= 0 || availableRegion.x <= 2.0f || availableRegion.y <= 2.0f) {
            ImGui::TextDisabled("Game output is not ready yet.");
            ImGui::End();
            return;
        }

        const float renderAspect = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
        float imageWidth = availableRegion.x;
        float imageHeight = imageWidth / renderAspect;

        if (imageHeight > availableRegion.y) {
            imageHeight = availableRegion.y;
            imageWidth = imageHeight * renderAspect;
        }

        const float xOffset = (availableRegion.x - imageWidth) * 0.5f;
        const float yOffset = (availableRegion.y - imageHeight) * 0.5f;
        if (xOffset > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset);
        }
        if (yOffset > 0.0f) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
        }

        const ImTextureID textureHandle = (ImTextureID)(uintptr_t)frameTextureID;
        ImGui::Image(textureHandle, ImVec2(imageWidth, imageHeight), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        ImGui::End();
    }

}
