#pragma once

#include "Core/Core.hpp"
#include "Event/Event.hpp"
#include "Application/Layer.hpp"
#include "Application/UI/UIElement.hpp"
#include "Application/UI/UIManager.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace Valkron {

class VALKRON_API UILayer : public Layer {
public:
    struct UIActionHandlers {
        std::function<void()> onPlay;
        std::function<void()> onSettings;
        std::function<void()> onQuit;
        std::function<void(bool)> onBloomChanged;
        std::function<void(float)> onExposureChanged;
        std::function<void(std::size_t, const std::string&)> onQualityChanged;
        std::function<void(const std::string&)> onProfileNameSubmitted;
        std::function<void(float)> onGammaChanged;
    };

    struct UIState {
        bool bloomEnabled = true;
        float exposure = 0.65f;
        std::size_t qualityIndex = 2;
        std::string quality = "High";
        std::string profileName;
        float gamma = 2.2f;
    };

private:
    UIManager m_uiManager;
    std::unordered_map<std::string, UIElement*> m_elementsById;
    UIActionHandlers m_actions{};
    UIState m_state{};
    float m_uiScale = 1.0f;
    int m_viewportWidth = 1280;
    int m_viewportHeight = 720;

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

    void setUIScale(float uiScale);
    float getUIScale() const {
        return m_uiScale;
    }
    void setActionHandlers(UIActionHandlers handlers);
    const UIState& getUIState() const {
        return m_state;
    }
    bool isBloomEnabled() const {
        return m_state.bloomEnabled;
    }
    float getExposure() const {
        return m_state.exposure;
    }
    std::size_t getQualityIndex() const {
        return m_state.qualityIndex;
    }
    const std::string& getQuality() const {
        return m_state.quality;
    }
    const std::string& getProfileName() const {
        return m_state.profileName;
    }
    float getGamma() const {
        return m_state.gamma;
    }

private:
    static float clampUIScale(float value);
    void bindCallbacksById();
    void rebuildTemplateLayout();
    bool tryParseFloat(const std::string& text, float& outValue) const;
};

} // namespace Valkron
