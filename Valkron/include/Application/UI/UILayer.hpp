#pragma once

#include "Core/Core.hpp"
#include "Event/Event.hpp"
#include "Application/Layer.hpp"
#include "Application/UI/RuntimeAssetImportService.hpp"
#include "Application/UI/Panels/EditorPanel.hpp"
#include "Engine/EngineConfig.hpp"
#include "Engine/Scene.hpp"

#include "glm/vec3.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Valkron {

    class Window;
    class Texture;

    class VALKRON_API UILayer : public Layer {
        public:
            UILayer(int id) : Layer(id) {}
            ~UILayer() override = default;

            UILayer(const UILayer&) = delete;
            UILayer& operator=(const UILayer&) = delete;
            UILayer(UILayer&&) = delete;
            UILayer& operator=(UILayer&&) = delete;

            void onAttach() override;
            void onDetach() override;
            void onUpdate(float deltaTime) override;
            void onEvent(Event& event) override;
            void bindEngineSettings(Window* window, EngineConfig* engineConfig);

        private:
            void appendTerminalLine(const std::string& line);
            void openAssetImportBrowser(RuntimeImportMode mode, const char* title);
            void drawAssetImportFileBrowser();
            bool importRuntimeAssetFromPath(const std::string& assetPath, RuntimeImportMode mode);
            bool isAssetPathAllowedForMode(const std::filesystem::path& path, RuntimeImportMode mode) const;
            void drawTopNavbar();
            void drawDockspaceHost();
            void drawSceneHierarchyPanel();
            void drawSceneViewPanel(float deltaTime);
            void drawGameViewPanel(float deltaTime);
            void drawInspectorPanel();
            void drawSettingsPanel();
            void drawDebugPanel();
            void drawBottomPanel();
            void drawAssetsPanel();
            void drawEngineSettingsSection();
            void syncEngineSettingsEditorState();
            void drawTerminalPanel();
            void setSelectedEntity(int entityIndex);
            void clearEntitySelection();
            void initializeSceneCameraController();
            void updateSceneCameraController(bool sceneImageHovered);
            void syncRendererCameraFromController();
            bool loadRuntimeTexture2D();
            bool loadRuntimeTexture3D();
            bool loadRuntimeShader();
            bool loadRuntimeComputeShader();
            bool loadRuntimeModel();
            bool loadRuntimeAssetAuto();

            Scene m_activeScene{"Main Scene"};
            std::vector<std::string> m_terminalLines;

            Window* m_window = nullptr;
            EngineConfig* m_engineConfig = nullptr;
            EngineSettings m_editableEngineSettings;
            std::array<char, 128> m_windowTitleBuffer{};

            int m_selectedEntityIndex = -1;
            bool m_autoScrollTerminal = true;
            bool m_scrollTerminalToBottom = false;
            bool m_engineSettingsDirty = false;

            int m_pendingViewportWidth = 0;
            int m_pendingViewportHeight = 0;
            float m_viewportResizeDebounceTimer = 0.0f;
            float m_viewportResizeDebounceDelaySeconds = 0.12f;
            float m_lastFrameDeltaTimeSeconds = 0.0f;

            bool m_showSceneHierarchyPanel = true;
            bool m_showSceneViewPanel = true;
            bool m_showGameViewPanel = true;
            bool m_showInspectorPanel = true;
            bool m_showSettingsPanel = false;
            bool m_showDebugPanel = false;
            bool m_showBottomPanel = true;
            bool m_resetLayoutRequested = true;
            bool m_dockspaceBuilt = false;

            std::array<char, 128> m_selectedEntityNameBuffer{};
            int m_selectedEntityNameBufferEntityIndex = -1;
            std::array<char, 128> m_hierarchySearchBuffer{};

            bool m_sceneViewImageHovered = false;
            bool m_sceneViewOptionsPopupEnabled = true;
            bool m_showSceneGrid = true;
            bool m_sceneCameraControllerInitialized = false;
            bool m_sceneCameraInvertPan = false;
            bool m_runtimeEntityCameraActive = false;

            int m_sceneGridHalfExtent = 12;
            float m_sceneGridSpacing = 1.0f;

            int m_gizmoOperationIndex = 0;
            bool m_gizmoWorldMode = false;
            bool m_gizmoSnapEnabled = false;
            float m_gizmoTranslateSnap = 0.5f;
            float m_gizmoRotateSnapDegrees = 15.0f;
            float m_gizmoScaleSnap = 0.1f;
            float m_gizmoSizeClipSpace = 0.12f;
            float m_gizmoRotationSensitivity = 1.0f;

            std::shared_ptr<Texture> m_cameraEntityIconTexture = nullptr;
            std::shared_ptr<Texture> m_lightEntityIconTexture = nullptr;
            float m_sceneEntityIconPixels = 22.0f;

            std::unique_ptr<EditorPanel> m_topNavbarPanelController = nullptr;
            std::unique_ptr<EditorPanel> m_dockspacePanelController = nullptr;
            std::unique_ptr<EditorPanel> m_sceneHierarchyPanelController = nullptr;
            std::unique_ptr<EditorPanel> m_sceneViewPanelController = nullptr;
            std::unique_ptr<EditorPanel> m_gameViewPanelController = nullptr;
            std::unique_ptr<EditorPanel> m_inspectorPanelController = nullptr;
            std::unique_ptr<EditorPanel> m_settingsPanelController = nullptr;
            std::unique_ptr<EditorPanel> m_debugPanelController = nullptr;
            std::unique_ptr<EditorPanel> m_assetBrowserPanelController = nullptr;

            glm::vec3 m_sceneCameraPivot{0.0f, 0.0f, 0.0f};
            glm::vec3 m_sceneCameraUp{0.0f, 1.0f, 0.0f};
            float m_sceneCameraDistance = 2.0f;
            float m_sceneCameraYawRadians = 0.0f;
            float m_sceneCameraPitchRadians = 0.0f;
            float m_sceneCameraRotateSpeed = 0.010f;
            float m_sceneCameraPanSpeed = 0.004f;
            float m_sceneCameraZoomSpeed = 0.12f;

            std::string m_assetBrowserFolderFilter;
            int m_selectedAssetIndex = -1;

            bool m_assetImportBrowserOpen = false;
            bool m_assetImportBrowserRequestOpen = false;
            RuntimeImportMode m_assetImportMode = RuntimeImportMode::Auto;
            std::string m_assetImportDialogTitle;
            std::filesystem::path m_assetImportCurrentDirectory;
            std::filesystem::path m_assetImportSelectedPath;

            int m_runtimeTexture3DDepth = 8;
    };

}
