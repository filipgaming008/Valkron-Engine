#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawSettingsPanel() {
        if (!ImGui::Begin("Settings", &m_showSettingsPanel)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        ImGui::Text("Engine Configuration");
        ImGui::Separator();
        drawEngineSettingsSection();

        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Close Settings")) {
            m_showSettingsPanel = false;
        }

        ImGui::End();
    }


}
