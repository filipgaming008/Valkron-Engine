#include "Application/UI/UILayer.hpp"
#include "Application/UI/UIButton.hpp"
#include "Application/UI/UIDropdown.hpp"
#include "Application/UI/UILoader.hpp"
#include "Application/UI/UIPanel.hpp"
#include "Application/UI/UISlider.hpp"
#include "Application/UI/UITextInput.hpp"
#include "Application/UI/UIToggleButton.hpp"
#include "Event/Event.hpp"
#include "Core/Log.hpp"
#include "Renderer/Renderer.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace Valkron {

namespace {
void bindCallbacksRecursive(UIElement& element) {
    if (auto* button = dynamic_cast<UIButton*>(&element)) {
        const std::string label = button->getLabel().empty() ? "Button" : button->getLabel();
        button->setOnClick([label]() { LOG_INFO("UI action: " + label + " clicked"); });
    }

    if (auto* toggle = dynamic_cast<UIToggleButton*>(&element)) {
        toggle->setOnToggleChanged(
            [](bool enabled) { LOG_INFO("UI action: Toggle -> " + std::string(enabled ? "ON" : "OFF")); });
    }

    if (auto* slider = dynamic_cast<UISlider*>(&element)) {
        slider->setOnValueChanged([](float value) { LOG_INFO("UI action: Slider -> " + std::to_string(value)); });
    }

    if (auto* dropdown = dynamic_cast<UIDropdown*>(&element)) {
        dropdown->setOnSelectionChanged([](std::size_t index, const std::string& value) {
            LOG_INFO("UI action: Dropdown selected [" + std::to_string(index) + "] " + value);
        });
    }

    if (auto* input = dynamic_cast<UITextInput*>(&element)) {
        input->setOnTextChanged([](const std::string& value) { LOG_INFO("UI action: Text changed -> " + value); });
        input->setOnSubmit([](const std::string& value) { LOG_INFO("UI action: Text submitted -> " + value); });
    }

    for (auto& child : element.getChildren()) {
        bindCallbacksRecursive(*child);
    }
}

template <typename T> T* getElementAs(const std::unordered_map<std::string, UIElement*>& elementsById, const char* id) {
    const auto it = elementsById.find(id);
    if (it == elementsById.end() || it->second == nullptr) {
        return nullptr;
    }
    return dynamic_cast<T*>(it->second);
}
} // namespace

float UILayer::clampUIScale(float value) {
    return std::clamp(value, 0.5f, 2.25f);
}

void UILayer::setUIScale(float uiScale) {
    const float clamped = clampUIScale(uiScale);
    if (std::abs(clamped - m_uiScale) < 0.001f) {
        return;
    }

    m_uiScale = clamped;
    rebuildTemplateLayout();
    LOG_INFO("UI scale changed to " + std::to_string(m_uiScale));
}

void UILayer::setActionHandlers(UIActionHandlers handlers) {
    m_actions = std::move(handlers);
    bindCallbacksById();
}

bool UILayer::tryParseFloat(const std::string& text, float& outValue) const {
    if (text.empty()) {
        return false;
    }

    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }

    outValue = parsed;
    return true;
}

void UILayer::bindCallbacksById() {
    if (auto* button = getElementAs<UIButton>(m_elementsById, "playButton")) {
        button->setOnClick([this]() {
            if (m_actions.onPlay) {
                m_actions.onPlay();
                return;
            }
            LOG_INFO("UI action: Play clicked");
        });
    }

    if (auto* button = getElementAs<UIButton>(m_elementsById, "settingsButton")) {
        button->setOnClick([this]() {
            if (m_actions.onSettings) {
                m_actions.onSettings();
                return;
            }
            LOG_INFO("UI action: Settings clicked");
        });
    }

    if (auto* button = getElementAs<UIButton>(m_elementsById, "quitButton")) {
        button->setOnClick([this]() {
            if (m_actions.onQuit) {
                m_actions.onQuit();
                return;
            }

            if (GLFWwindow* window = glfwGetCurrentContext()) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            LOG_INFO("UI action: Quit clicked (request close)");
        });
    }

    if (auto* toggle = getElementAs<UIToggleButton>(m_elementsById, "bloomToggle")) {
        toggle->setOnToggleChanged([this](bool enabled) {
            m_state.bloomEnabled = enabled;
            if (m_actions.onBloomChanged) {
                m_actions.onBloomChanged(enabled);
            }
            LOG_INFO("UI action: Bloom -> " + std::string(enabled ? "ON" : "OFF"));
        });
    }

    if (auto* slider = getElementAs<UISlider>(m_elementsById, "exposureSlider")) {
        slider->setOnValueChanged([this](float value) {
            m_state.exposure = value;
            if (m_actions.onExposureChanged) {
                m_actions.onExposureChanged(value);
            }
            LOG_INFO("UI action: Exposure -> " + std::to_string(value));
        });
    }

    if (auto* dropdown = getElementAs<UIDropdown>(m_elementsById, "qualityDropdown")) {
        dropdown->setOnSelectionChanged([this](std::size_t index, const std::string& value) {
            m_state.qualityIndex = index;
            m_state.quality = value;
            if (m_actions.onQualityChanged) {
                m_actions.onQualityChanged(index, value);
            }
            LOG_INFO("UI action: Quality selected [" + std::to_string(index) + "] " + value);
        });
    }

    if (auto* input = getElementAs<UITextInput>(m_elementsById, "profileNameInput")) {
        input->setOnSubmit([this](const std::string& value) {
            m_state.profileName = value;
            if (m_actions.onProfileNameSubmitted) {
                m_actions.onProfileNameSubmitted(value);
            }
            LOG_INFO("UI action: Name submitted -> " + value);
        });
    }

    if (auto* input = getElementAs<UITextInput>(m_elementsById, "gammaInput")) {
        input->setOnTextChanged([this](const std::string& value) {
            float parsed = 0.0f;
            if (!tryParseFloat(value, parsed)) {
                return;
            }

            m_state.gamma = parsed;
            if (m_actions.onGammaChanged) {
                m_actions.onGammaChanged(parsed);
            }
        });

        input->setOnSubmit([this](const std::string& value) {
            float parsed = 0.0f;
            if (tryParseFloat(value, parsed)) {
                m_state.gamma = parsed;
                if (m_actions.onGammaChanged) {
                    m_actions.onGammaChanged(parsed);
                }
                LOG_INFO("UI action: Gamma submitted -> " + std::to_string(parsed));
                return;
            }

            LOG_WARN("UI action: Invalid gamma input: " + value);
        });
    }
}

void UILayer::rebuildTemplateLayout() {
    m_uiManager.removeAll();
    m_elementsById.clear();

    constexpr float baseWidth = 1365.0f;
    constexpr float baseHeight = 768.0f;
    const float fitScale =
        std::min(static_cast<float>(m_viewportWidth) / baseWidth, static_cast<float>(m_viewportHeight) / baseHeight);
    const float effectiveScale = std::max(0.1f, fitScale * m_uiScale);
    auto s = [effectiveScale](float value) { return value * effectiveScale; };

    UILoadResult loaded = UILoader::loadFromJsonFileWithIds("ui/layout.json", effectiveScale);
    if (!loaded.elements.empty()) {
        m_elementsById = std::move(loaded.elementsById);
        for (auto& element : loaded.elements) {
            bindCallbacksRecursive(*element);
            m_uiManager.addElement(std::move(element));
        }
        bindCallbacksById();
        LOG_INFO("UILayer loaded UI from JSON layout.");
        return;
    }

    const float canvasW = s(1260.0f);
    const float canvasH = s(690.0f);
    const float canvasX = std::max(0.0f, (static_cast<float>(m_viewportWidth) - canvasW) * 0.5f);
    const float canvasY = std::max(0.0f, (static_cast<float>(m_viewportHeight) - canvasH) * 0.5f);

    auto canvasPanel = std::make_unique<UIPanel>(glm::vec2(canvasX, canvasY), glm::vec2(canvasW, canvasH),
                                                 glm::vec4(0.08f, 0.10f, 0.13f, 0.86f), "UI Canvas");
    canvasPanel->setPadding(s(14.0f));
    canvasPanel->setItemSpacing(s(18.0f));
    canvasPanel->setLayerID(10);

    auto leftDockPanel = std::make_unique<UIPanel>(glm::vec2(0.0f, 0.0f), glm::vec2(s(320.0f), s(620.0f)),
                                                   glm::vec4(0.11f, 0.14f, 0.18f, 0.95f), "Left Dock");
    leftDockPanel->setPadding(s(14.0f));
    leftDockPanel->setItemSpacing(s(14.0f));
    leftDockPanel->setLayerID(5);

    auto controlsPanel = std::make_unique<UIPanel>(glm::vec2(0.0f, 0.0f), glm::vec2(s(292.0f), s(360.0f)),
                                                   glm::vec4(0.14f, 0.18f, 0.24f, 0.95f), "Control Group");
    controlsPanel->setPadding(s(16.0f));
    controlsPanel->setItemSpacing(s(12.0f));
    controlsPanel->setLayerID(3);

    auto playButton = std::make_unique<UIButton>(glm::vec2(0.0f, 0.0f), glm::vec2(s(260.0f), s(56.0f)),
                                                 glm::vec4(0.15f, 0.65f, 0.95f, 0.95f), "Play");
    playButton->setOnClick([]() { LOG_INFO("UI action: Play clicked"); });
    playButton->setLayerID(2);
    controlsPanel->addChild(std::move(playButton));

    auto settingsButton = std::make_unique<UIButton>(glm::vec2(0.0f, 0.0f), glm::vec2(s(260.0f), s(56.0f)),
                                                     glm::vec4(0.20f, 0.80f, 0.40f, 0.95f), "Settings");
    settingsButton->setOnClick([]() { LOG_INFO("UI action: Settings clicked"); });
    settingsButton->setLayerID(2);
    controlsPanel->addChild(std::move(settingsButton));

    auto quitButton = std::make_unique<UIButton>(glm::vec2(0.0f, 0.0f), glm::vec2(s(260.0f), s(56.0f)),
                                                 glm::vec4(0.90f, 0.30f, 0.30f, 0.95f), "Quit");
    quitButton->setOnClick([]() { LOG_INFO("UI action: Quit clicked"); });
    quitButton->setLayerID(2);
    controlsPanel->addChild(std::move(quitButton));

    auto bloomToggle = std::make_unique<UIToggleButton>(glm::vec2(0.0f, 0.0f), glm::vec2(s(260.0f), s(48.0f)),
                                                        glm::vec4(0.25f, 0.28f, 0.35f, 0.95f),
                                                        glm::vec4(0.20f, 0.72f, 0.55f, 0.95f), "Bloom", true);
    bloomToggle->setOnToggleChanged(
        [](bool enabled) { LOG_INFO("UI action: Bloom toggled -> " + std::string(enabled ? "ON" : "OFF")); });
    bloomToggle->setLayerID(2);
    controlsPanel->addChild(std::move(bloomToggle));

    auto exposureSlider = std::make_unique<UISlider>(glm::vec2(0.0f, 0.0f), glm::vec2(s(260.0f), s(44.0f)),
                                                     glm::vec4(0.18f, 0.20f, 0.27f, 0.95f),
                                                     glm::vec4(0.88f, 0.63f, 0.24f, 0.96f), "Exposure", 0.65f);
    exposureSlider->setOnValueChanged([](float value) { LOG_INFO("UI action: Exposure -> " + std::to_string(value)); });
    exposureSlider->setLayerID(2);
    controlsPanel->addChild(std::move(exposureSlider));

    auto qualityDropdown = std::make_unique<UIDropdown>(
        glm::vec2(0.0f, 0.0f), glm::vec2(s(260.0f), s(44.0f)), glm::vec4(0.22f, 0.24f, 0.32f, 0.96f),
        glm::vec4(0.17f, 0.19f, 0.27f, 0.98f), std::vector<std::string>{"Low", "Medium", "High", "Ultra"}, 2);
    qualityDropdown->setOnSelectionChanged([](std::size_t index, const std::string& value) {
        LOG_INFO("UI action: Quality selected [" + std::to_string(index) + "] " + value);
    });
    qualityDropdown->setLayerID(2);
    controlsPanel->addChild(std::move(qualityDropdown));

    auto nameInput =
        std::make_unique<UITextInput>(glm::vec2(0.0f, 0.0f), glm::vec2(s(260.0f), s(44.0f)),
                                      glm::vec4(0.17f, 0.20f, 0.26f, 0.95f), "Profile Name", UITextInputMode::Any, "");
    nameInput->setOnSubmit([](const std::string& value) { LOG_INFO("UI action: Name submitted -> " + value); });
    nameInput->setLayerID(2);
    controlsPanel->addChild(std::move(nameInput));

    auto numericInput = std::make_unique<UITextInput>(glm::vec2(0.0f, 0.0f), glm::vec2(s(260.0f), s(44.0f)),
                                                      glm::vec4(0.17f, 0.20f, 0.26f, 0.95f), "Gamma (decimal)",
                                                      UITextInputMode::Decimal, "2.2");
    numericInput->setOnSubmit([](const std::string& value) { LOG_INFO("UI action: Numeric submitted -> " + value); });
    numericInput->setLayerID(2);
    controlsPanel->addChild(std::move(numericInput));

    controlsPanel->layoutVertical();
    leftDockPanel->addChild(std::move(controlsPanel));
    leftDockPanel->layoutVertical();

    auto centerPanel = std::make_unique<UIPanel>(glm::vec2(0.0f, 0.0f), glm::vec2(s(880.0f), s(620.0f)),
                                                 glm::vec4(0.10f, 0.12f, 0.17f, 0.90f), "Workspace");
    centerPanel->setPadding(s(18.0f));
    centerPanel->setItemSpacing(s(20.0f));
    centerPanel->setLayerID(6);

    auto toolbarPanel = std::make_unique<UIPanel>(glm::vec2(0.0f, 0.0f), glm::vec2(s(844.0f), s(98.0f)),
                                                  glm::vec4(0.13f, 0.17f, 0.24f, 0.96f), "Toolbar");
    toolbarPanel->setPadding(s(14.0f));
    toolbarPanel->setItemSpacing(s(14.0f));
    toolbarPanel->setLayerID(4);

    auto newButton = std::make_unique<UIButton>(glm::vec2(0.0f, 0.0f), glm::vec2(s(170.0f), s(48.0f)),
                                                glm::vec4(0.27f, 0.50f, 0.95f, 0.95f), "New");
    newButton->setLayerID(3);
    toolbarPanel->addChild(std::move(newButton));

    auto saveButton = std::make_unique<UIButton>(glm::vec2(0.0f, 0.0f), glm::vec2(s(170.0f), s(48.0f)),
                                                 glm::vec4(0.26f, 0.72f, 0.55f, 0.95f), "Save");
    saveButton->setLayerID(3);
    toolbarPanel->addChild(std::move(saveButton));

    auto exportButton = std::make_unique<UIButton>(glm::vec2(0.0f, 0.0f), glm::vec2(s(170.0f), s(48.0f)),
                                                   glm::vec4(0.94f, 0.63f, 0.22f, 0.95f), "Export");
    exportButton->setLayerID(3);
    toolbarPanel->addChild(std::move(exportButton));
    toolbarPanel->layoutHorizontal();

    auto previewPanel = std::make_unique<UIPanel>(glm::vec2(0.0f, 0.0f), glm::vec2(s(844.0f), s(438.0f)),
                                                  glm::vec4(0.12f, 0.15f, 0.22f, 0.94f), "Preview Panel");
    previewPanel->setPadding(s(22.0f));
    previewPanel->setItemSpacing(s(10.0f));
    previewPanel->setLayerID(2);
    previewPanel->setScrollingEnabled(true);
    previewPanel->setScrollSpeed(s(34.0f));

    for (int i = 0; i < 12; ++i) {
        auto widgetButton =
            std::make_unique<UIButton>(glm::vec2(0.0f, 0.0f), glm::vec2(s(220.0f), s(52.0f)),
                                       glm::vec4(0.46f, 0.38f, 0.88f, 0.95f), "Widget " + std::to_string(i + 1));
        widgetButton->setLayerID(3);
        previewPanel->addChild(std::move(widgetButton));
    }
    previewPanel->layoutVertical();

    centerPanel->addChild(std::move(toolbarPanel));
    centerPanel->addChild(std::move(previewPanel));
    centerPanel->layoutVertical();

    canvasPanel->addChild(std::move(leftDockPanel));
    canvasPanel->addChild(std::move(centerPanel));
    canvasPanel->layoutHorizontal();

    m_uiManager.addElement(std::move(canvasPanel));
}

void UILayer::onAttach() {
    GLFWwindow* currentContext = glfwGetCurrentContext();
    if (currentContext != nullptr) {
        glfwGetWindowSize(currentContext, &m_viewportWidth, &m_viewportHeight);
    }

    rebuildTemplateLayout();
    LOG_DEBUG("UILayer attached with responsive panel hierarchy template.");
}

void UILayer::onDetach() {
    LOG_DEBUG("UILayer detached.");
}

void UILayer::onUpdate(float deltaTime) {
    (void)deltaTime;

    std::vector<UIVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<UIDrawCommand> commands;
    m_uiManager.buildDrawData(vertices, indices, commands);
    static bool loggedBatch = false;
    if (!loggedBatch) {
        LOG_INFO("UILayer batch stats: vertices=" + std::to_string(vertices.size()) +
                 ", indices=" + std::to_string(indices.size()) + ", commands=" + std::to_string(commands.size()));
        loggedBatch = true;
    }
    Renderer::submitUIBatch(vertices, indices, commands);
}

void UILayer::onEvent(Event& event) {
    if (m_uiManager.handleEvent(event)) {
        event.handled = true;
        return;
    }

    if (event.type == EventType::Key) {
        auto& keyEvent = static_cast<KeyEvent&>(event);
        if (keyEvent.action == GLFW_PRESS) {
            const bool textInputFocused = dynamic_cast<UITextInput*>(m_uiManager.getFocusedElement()) != nullptr;
            if (textInputFocused) {
                return;
            }

            if (keyEvent.key == GLFW_KEY_EQUAL || keyEvent.key == GLFW_KEY_KP_ADD) {
                setUIScale(m_uiScale + 0.1f);
                event.handled = true;
                return;
            }

            if (keyEvent.key == GLFW_KEY_MINUS || keyEvent.key == GLFW_KEY_KP_SUBTRACT) {
                setUIScale(m_uiScale - 0.1f);
                event.handled = true;
                return;
            }

            if (keyEvent.key == GLFW_KEY_0 || keyEvent.key == GLFW_KEY_KP_0) {
                setUIScale(1.0f);
                event.handled = true;
                return;
            }

            LOG_DEBUG("Key " + std::to_string(keyEvent.key) + " pressed.");
            event.handled = true;
        }
        return;
    }

    if (event.type == EventType::WindowResize) {
        auto& resizeEvent = static_cast<WindowResizeEvent&>(event);
        m_viewportWidth = std::max(1, resizeEvent.width);
        m_viewportHeight = std::max(1, resizeEvent.height);
        rebuildTemplateLayout();
        return;
    }

    if (event.type == EventType::MouseButton) {
        auto& mouseButtonEvent = static_cast<MouseButtonEvent&>(event);
        if (mouseButtonEvent.action == GLFW_PRESS) {
            LOG_DEBUG("Mouse button " + std::to_string(mouseButtonEvent.button) + " pressed.");
        }
        return;
    }

    if (event.type == EventType::MouseMove) {
        auto& mouseMoveEvent = static_cast<MouseMoveEvent&>(event);
        LOG_DEBUG("Mouse moved to (" + std::to_string(mouseMoveEvent.x) + ", " + std::to_string(mouseMoveEvent.y) +
                  ").");
        return;
    }

    if (event.type == EventType::MouseScroll) {
        auto& scrollEvent = static_cast<MouseScrollEvent&>(event);
        LOG_DEBUG("Mouse scrolled (" + std::to_string(scrollEvent.xOffset) + ", " +
                  std::to_string(scrollEvent.yOffset) + ").");
    }
}

} // namespace Valkron