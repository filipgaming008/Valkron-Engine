#pragma once

#include "Core/Core.hpp"
#include "Event/Event.hpp"

#include "glm/glm.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace Valkron {

struct UIVertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec4 color;
    float sdfMode;
};

struct UIRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct UIDrawCommand {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    bool clipEnabled = false;
    UIRect clipRect{};
};

class VALKRON_API UIElement {
protected:
    int m_layerId = 0;
    glm::vec2 m_position;
    glm::vec2 m_size;
    std::vector<UIVertex> m_vertices;
    std::vector<std::uint32_t> m_indices;
    bool m_geometryDirty = true;
    UIElement* m_parent = nullptr;
    std::vector<std::unique_ptr<UIElement>> m_children;

public:
    virtual ~UIElement() = default;
    virtual void onRender() = 0;

    virtual bool hitTest(const glm::vec2& point) = 0;

    virtual bool onMouseButtonPressed(const MouseButtonEvent&) {
        return false;
    }
    virtual bool onMouseButtonReleased(const MouseButtonEvent&) {
        return false;
    }
    virtual bool onMouseMoved(const MouseMoveEvent&) {
        return false;
    }
    virtual bool onMouseScroll(const MouseScrollEvent&) {
        return false;
    }
    virtual bool onKeyPressed(const KeyEvent&) {
        return false;
    }
    virtual bool onCharInput(const CharInputEvent&) {
        return false;
    }
    virtual void onFocusChanged(bool) {}
    virtual bool clipsChildren() const {
        return false;
    }
    virtual UIRect getChildrenClipRect() const {
        return {};
    }

    const std::vector<UIVertex>& getVertices() const {
        return m_vertices;
    }
    const std::vector<std::uint32_t>& getIndices() const {
        return m_indices;
    }

    int getLayerID() const {
        return m_layerId;
    }
    void setLayerID(int id) {
        m_layerId = id;
    }

    void markGeometryDirty(bool includeChildren = false) {
        m_geometryDirty = true;

        if (!includeChildren) {
            return;
        }

        for (auto& child : m_children) {
            child->markGeometryDirty(true);
        }
    }

    void setLocalPosition(const glm::vec2& position) {
        m_position = position;
        markGeometryDirty();
    }

    void setLocalSize(const glm::vec2& size) {
        m_size = size;
        markGeometryDirty(true);
    }

    void addChild(std::unique_ptr<UIElement> child) {
        if (child == nullptr) {
            return;
        }

        child->m_parent = this;
        m_children.push_back(std::move(child));
    }

    void clearChildren() {
        m_children.clear();
    }

    UIElement* getParent() const {
        return m_parent;
    }
    std::vector<std::unique_ptr<UIElement>>& getChildren() {
        return m_children;
    }
    const std::vector<std::unique_ptr<UIElement>>& getChildren() const {
        return m_children;
    }

    glm::vec2 getLocalPosition() const {
        return m_position;
    }
    glm::vec2 getAbsolutePosition() const {
        if (m_parent == nullptr) {
            return m_position;
        }

        return m_parent->getAbsolutePosition() + m_position;
    }
    glm::vec2 getSize() const {
        return m_size;
    }

protected:
    virtual void rebuildGeometry() = 0;
};

} // namespace Valkron