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
            const ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.18f, nullptr, &dockMain);
            const ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.24f, nullptr, &dockMain);
            const ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.34f, nullptr, &dockMain);

            ImGuiID dockTopCenter = dockMain;
            const ImGuiID dockGame = ImGui::DockBuilderSplitNode(dockTopCenter, ImGuiDir_Right, 0.58f, nullptr, &dockTopCenter);
            ImGuiID dockBottomMain = dockBottom;
            const ImGuiID dockBottomRight = ImGui::DockBuilderSplitNode(dockBottomMain, ImGuiDir_Right, 0.38f, nullptr, &dockBottomMain);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
            ImGui::DockBuilderDockWindow("Scene View", dockTopCenter);
            ImGui::DockBuilderDockWindow("Game View", dockGame);
            ImGui::DockBuilderDockWindow("Inspector", dockRight);
            ImGui::DockBuilderDockWindow("Project", dockBottomMain);
            ImGui::DockBuilderDockWindow("Performance", dockBottomRight);

            ImGui::DockBuilderFinish(dockspaceID);
        }
    }


}
