#include "Application/UI/Panels/EditorPanel.hpp"

namespace Valkron {

    EditorPanel::EditorPanel(std::string panelId)
        : m_panelId(std::move(panelId)) {}

    const std::string& EditorPanel::getPanelId() const {
        return m_panelId;
    }

    StaticEditorPanel::StaticEditorPanel(std::string panelId, DrawCallback drawCallback)
        : EditorPanel(std::move(panelId)),
          m_drawCallback(std::move(drawCallback)) {}

    void StaticEditorPanel::render(float deltaTime) {
        (void)deltaTime;
        if (m_drawCallback) {
            m_drawCallback();
        }
    }

    DeltaEditorPanel::DeltaEditorPanel(std::string panelId, DrawCallback drawCallback)
        : EditorPanel(std::move(panelId)),
          m_drawCallback(std::move(drawCallback)) {}

    void DeltaEditorPanel::render(float deltaTime) {
        if (m_drawCallback) {
            m_drawCallback(deltaTime);
        }
    }

}
