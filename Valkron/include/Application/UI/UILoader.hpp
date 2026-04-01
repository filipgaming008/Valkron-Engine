#pragma once

#include "Core/Core.hpp"
#include "Application/UI/UIElement.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Valkron {

struct VALKRON_API UILoadResult {
    std::vector<std::unique_ptr<UIElement>> elements;
    std::unordered_map<std::string, UIElement*> elementsById;

    UILoadResult() = default;
    UILoadResult(const UILoadResult&) = delete;
    UILoadResult& operator=(const UILoadResult&) = delete;
    UILoadResult(UILoadResult&&) noexcept = default;
    UILoadResult& operator=(UILoadResult&&) noexcept = default;
};

class VALKRON_API UILoader {
public:
    static UILoadResult loadFromJsonFileWithIds(const std::string& path, float scale = 1.0f);
    static std::vector<std::unique_ptr<UIElement>> loadFromJsonFile(const std::string& path, float scale = 1.0f);
};

} // namespace Valkron
