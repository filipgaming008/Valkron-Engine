#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawTopNavbar() {
        if (!ImGui::BeginMainMenuBar()) {
            return;
        }

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                appendTerminalLine("TODO: New Scene flow will be implemented.");
            }
            if (ImGui::MenuItem("Open Scene")) {
                appendTerminalLine("TODO: Open Scene flow will be implemented.");
            }
            if (ImGui::MenuItem("Save Scene")) {
                appendTerminalLine("TODO: Save Scene flow will be implemented.");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene")) {
            const bool isEdit = m_activeScene.getState() == SceneState::Edit;
            const bool isPlay = m_activeScene.getState() == SceneState::Play;
            const bool isPause = m_activeScene.getState() == SceneState::Pause;

            if (ImGui::MenuItem("Edit Mode", nullptr, isEdit)) {
                m_activeScene.setState(SceneState::Edit);
                m_activeScene.setGameStateValue("Mode", "Edit");
                appendTerminalLine("Scene switched to Edit mode.");
            }
            if (ImGui::MenuItem("Play Mode", nullptr, isPlay)) {
                m_activeScene.setState(SceneState::Play);
                m_activeScene.setGameStateValue("Mode", "Play");
                appendTerminalLine("Scene switched to Play mode.");
            }
            if (ImGui::MenuItem("Pause Mode", nullptr, isPause)) {
                m_activeScene.setState(SceneState::Pause);
                m_activeScene.setGameStateValue("Mode", "Pause");
                appendTerminalLine("Scene switched to Pause mode.");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Scene Hierarchy", nullptr, &m_showSceneHierarchyPanel);
            ImGui::MenuItem("Scene View", nullptr, &m_showSceneViewPanel);
            ImGui::MenuItem("Game View", nullptr, &m_showGameViewPanel);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspectorPanel);
            ImGui::MenuItem("Project", nullptr, &m_showBottomPanel);
            ImGui::MenuItem("Performance", nullptr, &m_showDebugPanel);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                m_showSceneHierarchyPanel = true;
                m_showSceneViewPanel = true;
                m_showGameViewPanel = true;
                m_showInspectorPanel = true;
                m_showSettingsPanel = false;
                m_showBottomPanel = true;
                m_showDebugPanel = true;
                m_resetLayoutRequested = true;
                appendTerminalLine("Editor layout reset to default window arrangement.");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Open Settings")) {
                m_showSettingsPanel = true;
            }
            if (m_showSettingsPanel && ImGui::MenuItem("Close Settings")) {
                m_showSettingsPanel = false;
            }
            if (ImGui::MenuItem("Reload Engine Settings In Editor")) {
                syncEngineSettingsEditorState();
                appendTerminalLine("Settings editor reloaded from current engine config.");
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::Text("Scene: %s", m_activeScene.getName().c_str());

        ImGui::EndMainMenuBar();
    }


}
