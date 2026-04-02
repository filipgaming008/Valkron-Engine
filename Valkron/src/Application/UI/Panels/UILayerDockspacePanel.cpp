#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawDockspaceHost() {
        ImGuiIO& io = ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_DockingEnable) == 0) {
            return;
        }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspaceID = ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_None);

        if (!m_dockspaceBuilt || m_resetLayoutRequested) {
            m_dockspaceBuilt = true;

            ImGui::DockBuilderRemoveNode(dockspaceID);
            ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

            ImGuiID dockMain = dockspaceID;
            const ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.20f, nullptr, &dockMain);
            const ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.24f, nullptr, &dockMain);
            const ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
            ImGui::DockBuilderDockWindow("Scene View", dockMain);
            ImGui::DockBuilderDockWindow("Inspector", dockRight);
            ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);

            ImGui::DockBuilderFinish(dockspaceID);
        }
    }


}
