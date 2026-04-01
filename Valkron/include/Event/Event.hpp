#pragma once

namespace Valkron {

enum class EventType { Key, CharInput, MouseButton, MouseMove, MouseScroll, WindowClose, WindowResize };

struct Event {
    EventType type;
    bool handled = false;

    explicit Event(EventType eventType) : type(eventType) {}

    virtual ~Event() = default;
};

struct KeyEvent : Event {
    int key;
    int scancode;
    int action;
    int mods;

    KeyEvent(int keyCode, int scanCode, int keyAction, int modifiers)
        : Event(EventType::Key), key(keyCode), scancode(scanCode), action(keyAction), mods(modifiers) {}
};

struct MouseButtonEvent : Event {
    int button;
    int action;
    int mods;

    MouseButtonEvent(int buttonCode, int buttonAction, int modifiers)
        : Event(EventType::MouseButton), button(buttonCode), action(buttonAction), mods(modifiers) {}
};

struct CharInputEvent : Event {
    unsigned int codepoint;

    explicit CharInputEvent(unsigned int value) : Event(EventType::CharInput), codepoint(value) {}
};

struct MouseMoveEvent : Event {
    double x;
    double y;

    MouseMoveEvent(double mouseX, double mouseY) : Event(EventType::MouseMove), x(mouseX), y(mouseY) {}
};

struct MouseScrollEvent : Event {
    double xOffset;
    double yOffset;

    MouseScrollEvent(double x, double y) : Event(EventType::MouseScroll), xOffset(x), yOffset(y) {}
};

struct WindowCloseEvent : Event {
    WindowCloseEvent() : Event(EventType::WindowClose) {}
};

struct WindowResizeEvent : Event {
    int width;
    int height;

    WindowResizeEvent(int newWidth, int newHeight)
        : Event(EventType::WindowResize), width(newWidth), height(newHeight) {}
};

} // namespace Valkron