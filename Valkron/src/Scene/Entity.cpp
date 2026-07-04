#include "Scene/Entity.hpp"
#include "Scene/Component.hpp"
#include "Scene/ScriptComponent.hpp"

namespace Valkron {

    Entity::Entity(std::string name)
        : m_name(std::move(name)) {}

    Entity::~Entity() {
        for (auto& component : m_components) {
            if (component) {
                component->onDetach(*this);
            }
        }

        for (auto& component : m_components) {
            if (auto* script = dynamic_cast<ScriptComponent*>(component.get())) {
                script->onDestroy();
            }
        }
    }

    void Entity::setActive(bool active) {
        if (m_active == active) {
            return;
        }

        m_active = active;

        for (auto& component : m_components) {
            if (auto* script = dynamic_cast<ScriptComponent*>(component.get())) {
                if (m_active) {
                    script->onEnable();
                } else {
                    script->onDisable();
                }
            }
        }
    }

    void Entity::update(float deltaTime) {
        if (!m_active) {
            return;
        }

        if (!m_started) {
            for (auto& component : m_components) {
                if (auto* script = dynamic_cast<ScriptComponent*>(component.get())) {
                    script->onAwake();
                    script->onStart();
                }
            }
            m_started = true;
        }

        for (auto& component : m_components) {
            if (component) {
                component->onUpdate(deltaTime);

                if (auto* script = dynamic_cast<ScriptComponent*>(component.get())) {
                    script->onLateUpdate(deltaTime);
                }
            }
        }
    }

    void Entity::render() {
        if (!m_active) {
            return;
        }

        for (auto& component : m_components) {
            if (component) {
                component->onRender();
            }
        }
    }

    Entity& Scene::createEntity(const std::string& name) {
        auto entity = std::make_unique<Entity>(name);
        Entity* raw = entity.get();
        m_entities.push_back(std::move(entity));
        return *raw;
    }

    void Scene::update(float deltaTime) {
        for (auto& entity : m_entities) {
            if (entity) {
                entity->update(deltaTime);
            }
        }
    }

    void Scene::render() {
        for (auto& entity : m_entities) {
            if (entity) {
                entity->render();
            }
        }
    }

    void Scene::clear() {
        m_entities.clear();
    }

}