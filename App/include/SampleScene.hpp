#pragma once

#include "Valkron.hpp"

namespace Valkron::Sample {

    class TriangleRenderableComponent : public Component {
        public:
            TriangleRenderableComponent();
            ~TriangleRenderableComponent() override = default;

            void onAttach(Entity& entity) override;
            void onDetach(Entity& entity) override;
            void onRender() override;

        private:
            std::unique_ptr<VertexArray> m_vertexArray;
            std::unique_ptr<VertexBuffer> m_vertexBuffer;
            std::unique_ptr<IndexBuffer> m_indexBuffer;
            std::unique_ptr<Shader> m_shader;
    };

    class TriangleScriptComponent : public ScriptComponent {
        public:
            void onAwake() override;
            void onStart() override;
            void onUpdate(float deltaTime) override;
            void onLateUpdate(float deltaTime) override;
            float getElapsedTime() const { return m_elapsedTime; }

        private:
            float m_elapsedTime = 0.0f;
    };

    Entity& createTriangleEntity(Scene& scene, const std::string& name = "HelloTriangle");

}