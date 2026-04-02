#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawTerminalPanel() {
        if (ImGui::Button("Clear")) {
            m_terminalLines.clear();
        }

        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            std::string fullLog;
            for (const std::string& line : m_terminalLines) {
                fullLog += line;
                fullLog += '\n';
            }
            ImGui::SetClipboardText(fullLog.c_str());
            appendTerminalLine("Terminal log copied to clipboard.");
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScrollTerminal);
        ImGui::Separator();

        ImGui::BeginChild("TerminalLogRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const std::string& line : m_terminalLines) {
            ImGui::TextUnformatted(line.c_str());
        }

        if (m_autoScrollTerminal && m_scrollTerminalToBottom) {
            ImGui::SetScrollHereY(1.0f);
        }

        m_scrollTerminalToBottom = false;
        ImGui::EndChild();
    }

}
