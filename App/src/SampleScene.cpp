#include "SampleScene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Valkron::Sample {

    TriangleRenderableComponent::TriangleRenderableComponent() = default;

    void TriangleRenderableComponent::onAttach(Entity& /*entity*/) {
        const float vertices[] = {
            -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
             0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f
        };

        const unsigned int indices[] = {0, 1, 2};

        VertexLayout layout;
        layout.push<float>(3);
        layout.push<float>(3);

        m_vertexArray = std::make_unique<VertexArray>();
        m_vertexBuffer = std::make_unique<VertexBuffer>(vertices, sizeof(vertices));
        m_indexBuffer = std::make_unique<IndexBuffer>(indices, 3);
        m_vertexArray->addBuffer(*m_vertexBuffer, layout);
        m_shader = std::make_unique<Shader>("shaders/textured.vert", "shaders/textured.frag");
    }

    void TriangleRenderableComponent::onDetach(Entity& /*entity*/) {
        m_vertexArray.reset();
        m_vertexBuffer.reset();
        m_indexBuffer.reset();
        m_shader.reset();
    }

    void TriangleRenderableComponent::onRender() {
        if (!m_vertexArray || !m_vertexBuffer || !m_indexBuffer || !m_shader) {
            return;
        }

        Renderer::submit([this]() {
            RenderCommand::setDepthTest(false);
            m_shader->bind();

            const glm::mat4 model = glm::mat4(1.0f);
            const glm::mat4 view = glm::mat4(1.0f);
            const glm::mat4 projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

            m_shader->setMat4("u_Model", glm::value_ptr(model));
            m_shader->setMat4("u_View", glm::value_ptr(view));
            m_shader->setMat4("u_Projection", glm::value_ptr(projection));

            RenderCommand::bindVertexArray(m_vertexArray->getID());
            RenderCommand::drawIndexed(m_indexBuffer->getID(), m_indexBuffer->getCount());
        });
    }

    void TriangleScriptComponent::onAwake() {
        m_elapsedTime = 0.0f;
    }

    void TriangleScriptComponent::onStart() {
        m_elapsedTime = 0.0f;
    }

    void TriangleScriptComponent::onUpdate(float deltaTime) {
        m_elapsedTime += deltaTime;
    }

    void TriangleScriptComponent::onLateUpdate(float deltaTime) {
        (void)deltaTime;
    }

    Entity& createTriangleEntity(Scene& scene, const std::string& name) {
        auto& entity = scene.createEntity(name);
        entity.addComponent<TriangleRenderableComponent>();
        entity.addComponent<TriangleScriptComponent>();
        return entity;
    }

}