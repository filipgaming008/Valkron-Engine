#pragma once

#include "Core/Core.hpp"

#include <functional>
#include <string>
#include <utility>

namespace Valkron {

    class VALKRON_API EditorPanel {
        public:
            explicit EditorPanel(std::string panelId);
            virtual ~EditorPanel() = default;

            EditorPanel(const EditorPanel&) = delete;
            EditorPanel& operator=(const EditorPanel&) = delete;
            EditorPanel(EditorPanel&&) = delete;
            EditorPanel& operator=(EditorPanel&&) = delete;

            const std::string& getPanelId() const;
            virtual void render(float deltaTime) = 0;

        private:
            std::string m_panelId;
    };

    class VALKRON_API StaticEditorPanel : public EditorPanel {
        public:
            using DrawCallback = std::function<void()>;

            StaticEditorPanel(std::string panelId, DrawCallback drawCallback);
            void render(float deltaTime) override;

        protected:
            DrawCallback m_drawCallback;
    };

    class VALKRON_API DeltaEditorPanel : public EditorPanel {
        public:
            using DrawCallback = std::function<void(float)>;

            DeltaEditorPanel(std::string panelId, DrawCallback drawCallback);
            void render(float deltaTime) override;

        protected:
            DrawCallback m_drawCallback;
    };

    class VALKRON_API TopNavbarPanel final : public StaticEditorPanel {
        public:
            explicit TopNavbarPanel(DrawCallback drawCallback)
                : StaticEditorPanel("TopNavbar", std::move(drawCallback)) {}
    };

    class VALKRON_API DockspacePanel final : public StaticEditorPanel {
        public:
            explicit DockspacePanel(DrawCallback drawCallback)
                : StaticEditorPanel("Dockspace", std::move(drawCallback)) {}
    };

    class VALKRON_API SceneHierarchyPanel final : public StaticEditorPanel {
        public:
            explicit SceneHierarchyPanel(DrawCallback drawCallback)
                : StaticEditorPanel("SceneHierarchy", std::move(drawCallback)) {}
    };

    class VALKRON_API SceneViewPanel final : public DeltaEditorPanel {
        public:
            explicit SceneViewPanel(DrawCallback drawCallback)
                : DeltaEditorPanel("SceneView", std::move(drawCallback)) {}
    };

    class VALKRON_API InspectorPanel final : public StaticEditorPanel {
        public:
            explicit InspectorPanel(DrawCallback drawCallback)
                : StaticEditorPanel("Inspector", std::move(drawCallback)) {}
    };

    class VALKRON_API SettingsPanel final : public StaticEditorPanel {
        public:
            explicit SettingsPanel(DrawCallback drawCallback)
                : StaticEditorPanel("Settings", std::move(drawCallback)) {}
    };

    class VALKRON_API DebugPanel final : public StaticEditorPanel {
        public:
            explicit DebugPanel(DrawCallback drawCallback)
                : StaticEditorPanel("Debug", std::move(drawCallback)) {}
    };

    class VALKRON_API AssetBrowserPanel final : public StaticEditorPanel {
        public:
            explicit AssetBrowserPanel(DrawCallback drawCallback)
                : StaticEditorPanel("AssetBrowser", std::move(drawCallback)) {}
    };

}
