#pragma once

#include "Scene/Component.hpp"

namespace Valkron {

    class VALKRON_API ScriptComponent : public Component {
        public:
            virtual ~ScriptComponent() = default;

            virtual void onAwake() {}
            virtual void onEnable() {}
            virtual void onStart() {}
            virtual void onUpdate(float deltaTime) override { (void)deltaTime; }
            virtual void onLateUpdate(float deltaTime) { (void)deltaTime; }
            virtual void onFixedUpdate(float deltaTime) { (void)deltaTime; }
            virtual void onDisable() {}
            virtual void onDestroy() {}
    };

}