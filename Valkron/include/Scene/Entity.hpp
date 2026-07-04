#pragma once

#include "Core/Core.hpp"
#include "Scene/Component.hpp"
#include "Scene/ScriptComponent.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Valkron {

    class Entity;

    class VALKRON_API Entity {
        public:
            explicit Entity(std::string name = "Entity");
            ~Entity();

            template<typename T, typename... Args>
            T& addComponent(Args&&... args) {
                auto component = std::make_unique<T>(std::forward<Args>(args)...);
                T* rawComponent = component.get();
                rawComponent->m_owner = this;
                rawComponent->onAttach(*this);
                m_components.push_back(std::move(component));
                return *rawComponent;
            }

            template<typename T>
            T* getComponent() {
                for (auto& component : m_components) {
                    if (auto* casted = dynamic_cast<T*>(component.get())) {
                        return casted;
                    }
                }
                return nullptr;
            }

            template<typename T>
            const T* getComponent() const {
                for (const auto& component : m_components) {
                    if (const auto* casted = dynamic_cast<const T*>(component.get())) {
                        return casted;
                    }
                }
                return nullptr;
            }

            void update(float deltaTime);
            void render();

            const std::string& getName() const { return m_name; }
            void setName(const std::string& name) { m_name = name; }

            bool isActive() const { return m_active; }
            void setActive(bool active);

        private:
            std::string m_name;
            bool m_active = true;
            bool m_started = false;
            std::vector<std::unique_ptr<Component>> m_components;
    };

    class VALKRON_API Scene {
        public:
            Entity& createEntity(const std::string& name = "Entity");
            void update(float deltaTime);
            void render();
            void clear();

        private:
            std::vector<std::unique_ptr<Entity>> m_entities;
    };

}