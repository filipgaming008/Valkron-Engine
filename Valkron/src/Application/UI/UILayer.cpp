#include "Application/UI/UILayer.hpp"
#include "Event/Event.hpp"
#include "Core/Log.hpp"

#include "imgui.h"

namespace Valkron {

    void UILayer::onAttach() {
        LOG_DEBUG("UILayer attached (Dear ImGui).");
    }

    void UILayer::onDetach() {
        LOG_DEBUG("UILayer detached.");
    }

    void UILayer::onUpdate(float deltaTime) {
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 240.0f), ImGuiCond_Once);

        ImGui::Begin("Valkron Tools");
        ImGui::Text("Dear ImGui is active");
        ImGui::Separator();
        ImGui::Text("Frame dt: %.3f ms", deltaTime * 1000.0f);
        ImGui::Text("FPS: %.1f", deltaTime > 0.0f ? (1.0f / deltaTime) : 0.0f);

        if (ImGui::Button("Play")) {
            LOG_INFO("UI action: Play");
        }
        if (ImGui::Button("Settings")) {
            LOG_INFO("UI action: Settings");
        }
        if (ImGui::Button("Quit")) {
            LOG_INFO("UI action: Quit");
        }
        ImGui::End();
    }

    void UILayer::onEvent(Event& event) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse && (
            event.type == EventType::MouseButton ||
            event.type == EventType::MouseMove ||
            event.type == EventType::MouseScroll
        )) {
            event.handled = true;
            return;
        }

        if (io.WantCaptureKeyboard && event.type == EventType::Key) {
            event.handled = true;
        }
    }

}