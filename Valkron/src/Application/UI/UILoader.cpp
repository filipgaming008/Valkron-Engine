#include "Application/UI/UILoader.hpp"

#include "Application/UI/UIButton.hpp"
#include "Application/UI/UIDropdown.hpp"
#include "Application/UI/UIPanel.hpp"
#include "Application/UI/UISlider.hpp"
#include "Application/UI/UITextInput.hpp"
#include "Application/UI/UIToggleButton.hpp"
#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

namespace Valkron {

namespace {

struct JsonValue;
using JsonObject = std::unordered_map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;
using JsonData = std::variant<std::nullptr_t, bool, double, std::string, JsonObject, JsonArray>;

struct JsonValue {
    JsonData data;

    bool isObject() const {
        return std::holds_alternative<JsonObject>(data);
    }
    bool isArray() const {
        return std::holds_alternative<JsonArray>(data);
    }
    bool isString() const {
        return std::holds_alternative<std::string>(data);
    }
    bool isNumber() const {
        return std::holds_alternative<double>(data);
    }
    bool isBool() const {
        return std::holds_alternative<bool>(data);
    }

    const JsonObject* asObject() const {
        return std::get_if<JsonObject>(&data);
    }
    const JsonArray* asArray() const {
        return std::get_if<JsonArray>(&data);
    }
    const std::string* asString() const {
        return std::get_if<std::string>(&data);
    }
    const double* asNumber() const {
        return std::get_if<double>(&data);
    }
    const bool* asBool() const {
        return std::get_if<bool>(&data);
    }
};

class Parser {
public:
    explicit Parser(std::string source) : m_source(std::move(source)) {}

    JsonValue parse() {
        skipWhitespace();
        JsonValue value = parseValue();
        skipWhitespace();
        return value;
    }

private:
    char peek() const {
        if (m_index >= m_source.size()) {
            return '\0';
        }
        return m_source[m_index];
    }

    char consume() {
        if (m_index >= m_source.size()) {
            return '\0';
        }
        return m_source[m_index++];
    }

    void skipWhitespace() {
        while (std::isspace(static_cast<unsigned char>(peek())) != 0) {
            consume();
        }
    }

    JsonValue parseValue() {
        skipWhitespace();
        const char ch = peek();
        if (ch == '{') {
            return JsonValue{parseObject()};
        }
        if (ch == '[') {
            return JsonValue{parseArray()};
        }
        if (ch == '"') {
            return JsonValue{parseString()};
        }
        if (ch == 't' || ch == 'f') {
            return JsonValue{parseBool()};
        }
        if (ch == 'n') {
            parseNull();
            return JsonValue{nullptr};
        }
        return JsonValue{parseNumber()};
    }

    JsonObject parseObject() {
        JsonObject object;
        consume();
        skipWhitespace();
        if (peek() == '}') {
            consume();
            return object;
        }

        while (true) {
            const std::string key = parseString();
            skipWhitespace();
            if (consume() != ':') {
                break;
            }
            object[key] = parseValue();
            skipWhitespace();
            const char delimiter = consume();
            if (delimiter == '}') {
                break;
            }
            if (delimiter != ',') {
                break;
            }
            skipWhitespace();
        }

        return object;
    }

    JsonArray parseArray() {
        JsonArray array;
        consume();
        skipWhitespace();
        if (peek() == ']') {
            consume();
            return array;
        }

        while (true) {
            array.push_back(parseValue());
            skipWhitespace();
            const char delimiter = consume();
            if (delimiter == ']') {
                break;
            }
            if (delimiter != ',') {
                break;
            }
            skipWhitespace();
        }

        return array;
    }

    std::string parseString() {
        std::string output;
        if (consume() != '"') {
            return output;
        }

        while (true) {
            const char ch = consume();
            if (ch == '\0' || ch == '"') {
                break;
            }
            if (ch == '\\') {
                const char escaped = consume();
                switch (escaped) {
                    case '"':
                        output.push_back('"');
                        break;
                    case '\\':
                        output.push_back('\\');
                        break;
                    case '/':
                        output.push_back('/');
                        break;
                    case 'b':
                        output.push_back('\b');
                        break;
                    case 'f':
                        output.push_back('\f');
                        break;
                    case 'n':
                        output.push_back('\n');
                        break;
                    case 'r':
                        output.push_back('\r');
                        break;
                    case 't':
                        output.push_back('\t');
                        break;
                    default:
                        output.push_back(escaped);
                        break;
                }
            } else {
                output.push_back(ch);
            }
        }

        return output;
    }

    bool parseBool() {
        if (m_source.compare(m_index, 4, "true") == 0) {
            m_index += 4;
            return true;
        }
        m_index += 5;
        return false;
    }

    void parseNull() {
        m_index += 4;
    }

    double parseNumber() {
        const std::size_t start = m_index;
        while (true) {
            const char ch = peek();
            if (!(std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '+' || ch == '.' ||
                  ch == 'e' || ch == 'E')) {
                break;
            }
            consume();
        }

        try {
            return std::stod(m_source.substr(start, m_index - start));
        } catch (...) {
            return 0.0;
        }
    }

    std::string m_source;
    std::size_t m_index = 0;
};

const JsonValue* getField(const JsonObject& object, const std::string& key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::vector<std::string> getStringArrayField(const JsonObject& object, const std::string& key) {
    std::vector<std::string> result;
    const JsonValue* value = getField(object, key);
    if (value == nullptr || !value->isArray()) {
        return result;
    }

    for (const JsonValue& item : *value->asArray()) {
        if (item.isString()) {
            result.push_back(*item.asString());
        }
    }

    return result;
}

float getNumberField(const JsonObject& object, const std::string& key, float fallback) {
    const JsonValue* value = getField(object, key);
    if (value == nullptr || !value->isNumber()) {
        return fallback;
    }
    return static_cast<float>(*value->asNumber());
}

int getIntField(const JsonObject& object, const std::string& key, int fallback) {
    return static_cast<int>(std::round(getNumberField(object, key, static_cast<float>(fallback))));
}

bool getBoolField(const JsonObject& object, const std::string& key, bool fallback) {
    const JsonValue* value = getField(object, key);
    if (value == nullptr || !value->isBool()) {
        return fallback;
    }
    return *value->asBool();
}

std::string getStringField(const JsonObject& object, const std::string& key, const std::string& fallback = "") {
    const JsonValue* value = getField(object, key);
    if (value == nullptr || !value->isString()) {
        return fallback;
    }
    return *value->asString();
}

glm::vec4 getColorField(const JsonObject& object, const std::string& key, const glm::vec4& fallback) {
    const JsonValue* value = getField(object, key);
    if (value == nullptr || !value->isArray()) {
        return fallback;
    }

    const JsonArray* array = value->asArray();
    if (array->size() < 3) {
        return fallback;
    }

    const float r = array->at(0).isNumber() ? static_cast<float>(*array->at(0).asNumber()) : fallback.r;
    const float g = array->at(1).isNumber() ? static_cast<float>(*array->at(1).asNumber()) : fallback.g;
    const float b = array->at(2).isNumber() ? static_cast<float>(*array->at(2).asNumber()) : fallback.b;
    const float a =
        (array->size() >= 4 && array->at(3).isNumber()) ? static_cast<float>(*array->at(3).asNumber()) : fallback.a;
    return glm::vec4(r, g, b, a);
}

std::unique_ptr<UIElement> buildElement(const JsonObject& object, float scale,
                                        std::unordered_map<std::string, UIElement*>* elementsById);

void applyCommonSettings(UIElement& element, const JsonObject& object,
                         std::unordered_map<std::string, UIElement*>* elementsById) {
    element.setLayerID(getIntField(object, "layer", element.getLayerID()));

    if (elementsById == nullptr) {
        return;
    }

    const std::string id = getStringField(object, "id", "");
    if (id.empty()) {
        return;
    }

    if (elementsById->contains(id)) {
        LOG_WARN("UILoader: duplicate element id ignored: " + id);
        return;
    }

    elementsById->insert({id, &element});
}

void buildChildren(UIElement& parent, const JsonObject& object, float scale,
                   std::unordered_map<std::string, UIElement*>* elementsById) {
    const JsonValue* childrenValue = getField(object, "children");
    if (childrenValue == nullptr || !childrenValue->isArray()) {
        return;
    }

    for (const JsonValue& childValue : *childrenValue->asArray()) {
        if (!childValue.isObject()) {
            continue;
        }
        std::unique_ptr<UIElement> child = buildElement(*childValue.asObject(), scale, elementsById);
        if (child) {
            parent.addChild(std::move(child));
        }
    }
}

std::unique_ptr<UIElement> buildPanel(const JsonObject& object, float scale,
                                      std::unordered_map<std::string, UIElement*>* elementsById) {
    auto panel = std::make_unique<UIPanel>(
        glm::vec2(getNumberField(object, "x", 0.0f) * scale, getNumberField(object, "y", 0.0f) * scale),
        glm::vec2(getNumberField(object, "width", 100.0f) * scale, getNumberField(object, "height", 100.0f) * scale),
        getColorField(object, "color", glm::vec4(0.2f, 0.2f, 0.2f, 0.9f)), getStringField(object, "title", ""));

    panel->setPadding(getNumberField(object, "padding", panel->getPadding()) * scale);
    panel->setItemSpacing(getNumberField(object, "spacing", panel->getItemSpacing()) * scale);
    panel->setClipChildren(getBoolField(object, "clipChildren", false));
    panel->setScrollingEnabled(getBoolField(object, "scrolling", false));
    panel->setScrollSpeed(getNumberField(object, "scrollSpeed", 24.0f) * scale);

    buildChildren(*panel, object, scale, elementsById);

    const std::string layout = getStringField(object, "layout", "none");
    if (layout == "vertical") {
        panel->layoutVertical();
    } else if (layout == "horizontal") {
        panel->layoutHorizontal();
    }

    applyCommonSettings(*panel, object, elementsById);
    return panel;
}

std::unique_ptr<UIElement> buildButton(const JsonObject& object, float scale,
                                       std::unordered_map<std::string, UIElement*>* elementsById) {
    auto button = std::make_unique<UIButton>(
        glm::vec2(getNumberField(object, "x", 0.0f) * scale, getNumberField(object, "y", 0.0f) * scale),
        glm::vec2(getNumberField(object, "width", 160.0f) * scale, getNumberField(object, "height", 48.0f) * scale),
        getColorField(object, "color", glm::vec4(0.3f, 0.5f, 0.9f, 0.95f)), getStringField(object, "label", "Button"));
    applyCommonSettings(*button, object, elementsById);
    return button;
}

std::unique_ptr<UIElement> buildToggle(const JsonObject& object, float scale,
                                       std::unordered_map<std::string, UIElement*>* elementsById) {
    auto toggle = std::make_unique<UIToggleButton>(
        glm::vec2(getNumberField(object, "x", 0.0f) * scale, getNumberField(object, "y", 0.0f) * scale),
        glm::vec2(getNumberField(object, "width", 180.0f) * scale, getNumberField(object, "height", 44.0f) * scale),
        getColorField(object, "offColor", glm::vec4(0.22f, 0.24f, 0.30f, 0.95f)),
        getColorField(object, "onColor", glm::vec4(0.20f, 0.72f, 0.55f, 0.95f)),
        getStringField(object, "label", "Toggle"), getBoolField(object, "value", false));
    applyCommonSettings(*toggle, object, elementsById);
    return toggle;
}

std::unique_ptr<UIElement> buildSlider(const JsonObject& object, float scale,
                                       std::unordered_map<std::string, UIElement*>* elementsById) {
    auto slider = std::make_unique<UISlider>(
        glm::vec2(getNumberField(object, "x", 0.0f) * scale, getNumberField(object, "y", 0.0f) * scale),
        glm::vec2(getNumberField(object, "width", 200.0f) * scale, getNumberField(object, "height", 42.0f) * scale),
        getColorField(object, "trackColor", glm::vec4(0.18f, 0.20f, 0.27f, 0.95f)),
        getColorField(object, "fillColor", glm::vec4(0.88f, 0.63f, 0.24f, 0.96f)),
        getStringField(object, "label", "Slider"), getNumberField(object, "value", 0.5f));
    applyCommonSettings(*slider, object, elementsById);
    return slider;
}

std::unique_ptr<UIElement> buildDropdown(const JsonObject& object, float scale,
                                         std::unordered_map<std::string, UIElement*>* elementsById) {
    std::vector<std::string> options;
    const JsonValue* optionsValue = getField(object, "options");
    if (optionsValue != nullptr && optionsValue->isArray()) {
        for (const JsonValue& value : *optionsValue->asArray()) {
            if (value.isString()) {
                options.push_back(*value.asString());
            }
        }
    }

    auto dropdown = std::make_unique<UIDropdown>(
        glm::vec2(getNumberField(object, "x", 0.0f) * scale, getNumberField(object, "y", 0.0f) * scale),
        glm::vec2(getNumberField(object, "width", 200.0f) * scale, getNumberField(object, "height", 42.0f) * scale),
        getColorField(object, "headerColor", glm::vec4(0.22f, 0.24f, 0.32f, 0.96f)),
        getColorField(object, "menuColor", glm::vec4(0.17f, 0.19f, 0.27f, 0.98f)), options,
        static_cast<std::size_t>(std::max(0, getIntField(object, "selectedIndex", 0))));
    applyCommonSettings(*dropdown, object, elementsById);
    return dropdown;
}

std::unique_ptr<UIElement> buildTextInput(const JsonObject& object, float scale,
                                          std::unordered_map<std::string, UIElement*>* elementsById) {
    const std::string modeString = getStringField(object, "mode", "any");
    UITextInputMode mode = UITextInputMode::Any;
    if (modeString == "integer") {
        mode = UITextInputMode::Integer;
    } else if (modeString == "decimal") {
        mode = UITextInputMode::Decimal;
    }

    auto input = std::make_unique<UITextInput>(
        glm::vec2(getNumberField(object, "x", 0.0f) * scale, getNumberField(object, "y", 0.0f) * scale),
        glm::vec2(getNumberField(object, "width", 220.0f) * scale, getNumberField(object, "height", 42.0f) * scale),
        getColorField(object, "color", glm::vec4(0.18f, 0.20f, 0.27f, 0.95f)),
        getStringField(object, "placeholder", ""), mode, getStringField(object, "value", ""));
    applyCommonSettings(*input, object, elementsById);
    return input;
}

std::unique_ptr<UIElement> buildElement(const JsonObject& object, float scale,
                                        std::unordered_map<std::string, UIElement*>* elementsById) {
    const std::string type = getStringField(object, "type", "");
    if (type == "panel") {
        return buildPanel(object, scale, elementsById);
    }
    if (type == "button") {
        return buildButton(object, scale, elementsById);
    }
    if (type == "toggle") {
        return buildToggle(object, scale, elementsById);
    }
    if (type == "slider") {
        return buildSlider(object, scale, elementsById);
    }
    if (type == "dropdown") {
        return buildDropdown(object, scale, elementsById);
    }
    if (type == "textInput") {
        return buildTextInput(object, scale, elementsById);
    }

    return nullptr;
}

} // namespace

UILoadResult UILoader::loadFromJsonFileWithIds(const std::string& path, float scale) {
    const std::filesystem::path resolvedPath = FileSystem::resolveAssetPath(path);
    std::ifstream input(resolvedPath);
    if (!input.is_open()) {
        LOG_WARN("UILoader: failed to open file: " + resolvedPath.string());
        return {};
    }

    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    Parser parser(source);
    JsonValue root = parser.parse();
    if (!root.isObject()) {
        LOG_WARN("UILoader: root JSON must be an object");
        return {};
    }

    const JsonObject& rootObject = *root.asObject();
    const JsonValue* elementsValue = getField(rootObject, "elements");
    if (elementsValue == nullptr || !elementsValue->isArray()) {
        LOG_WARN("UILoader: missing elements array");
        return {};
    }
    const std::vector<std::string> requiredIds = getStringArrayField(rootObject, "requiredIds");

    UILoadResult result;
    for (const JsonValue& elementValue : *elementsValue->asArray()) {
        if (!elementValue.isObject()) {
            continue;
        }

        std::unique_ptr<UIElement> element = buildElement(*elementValue.asObject(), scale, &result.elementsById);
        if (element) {
            result.elements.push_back(std::move(element));
        }
    }

    for (const std::string& requiredId : requiredIds) {
        if (requiredId.empty()) {
            continue;
        }

        if (!result.elementsById.contains(requiredId)) {
            LOG_WARN("UILoader: required id missing: " + requiredId);
            return {};
        }
    }

    return result;
}

std::vector<std::unique_ptr<UIElement>> UILoader::loadFromJsonFile(const std::string& path, float scale) {
    UILoadResult result = loadFromJsonFileWithIds(path, scale);
    return std::move(result.elements);
}

} // namespace Valkron
