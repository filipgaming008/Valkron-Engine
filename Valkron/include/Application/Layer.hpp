#pragma once

#include "Core/Core.hpp"
#include "Event/Event.hpp"

#include <string>

namespace Valkron {

class VALKRON_API Layer {
protected:
    int m_layerId;

public:
    explicit Layer(int id) : m_layerId(id) {}
    virtual ~Layer() = default;

    virtual void onAttach() {}
    virtual void onDetach() {}
    virtual void onUpdate(float) {}
    virtual void onEvent(Event&) {}

    int getLayerId() const {
        return m_layerId;
    }
};

} // namespace Valkron