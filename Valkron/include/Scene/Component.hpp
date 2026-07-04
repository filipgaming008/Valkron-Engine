#pragma once

#include "Core/Core.hpp"

namespace Valkron {

    class Entity;

    class VALKRON_API Component {
        public:
            virtual ~Component() = default;

            virtual void onAttach(Entity&) {}
            virtual void onDetach(Entity&) {}
            virtual void onUpdate(float) {}
            virtual void onRender() {}

            Entity* getOwner() const { return m_owner; }

        protected:
            Entity* m_owner = nullptr;
            friend class Entity;
    };

}