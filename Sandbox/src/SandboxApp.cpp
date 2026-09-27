#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Core/Log.h>
#include <Engine/Input/Input.h>
#include <Engine/Renderer/OrthographicCamera.h>
#include <Engine/Renderer/Renderer2D.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/RaycastHit2D.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/ScriptableEntity.h>

#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <cstdlib>
#include <string>

namespace
{
    ByteForge::Entity SpawnBox(ByteForge::Scene& scene, const std::string& name, const glm::vec3& position,
                              const glm::vec2& size, const glm::vec4& color,
                              const ByteForge::Rigidbody2DComponent::BodyType bodyType,
                              const float restitution = 0.1f)
    {
        const ByteForge::Entity entity = scene.CreateEntity(name);

        auto& transform = entity.GetComponent<ByteForge::TransformComponent>();
        transform.Position = position;
        transform.Scale = size;

        entity.AddComponent<ByteForge::SpriteRendererComponent>().Color = color;

        entity.AddComponent<ByteForge::Rigidbody2DComponent>().Type = bodyType;

        auto& collider = entity.AddComponent<ByteForge::BoxCollider2DComponent>();
        collider.Size = size;
        collider.Restitution = restitution;

        return entity;
    }

    ByteForge::Entity SpawnCircle(ByteForge::Scene& scene, const std::string& name, const glm::vec3& position,
                                 const float radius, const glm::vec4& color,
                                 const ByteForge::Rigidbody2DComponent::BodyType bodyType,
                                 const float restitution = 0.3f)
    {
        const ByteForge::Entity entity = scene.CreateEntity(name);

        auto& transform = entity.GetComponent<ByteForge::TransformComponent>();
        transform.Position = position;
        transform.Scale = { radius * 2.0f, radius * 2.0f };

        entity.AddComponent<ByteForge::SpriteRendererComponent>().Color = color;

        entity.AddComponent<ByteForge::Rigidbody2DComponent>().Type = bodyType;

        auto& collider = entity.AddComponent<ByteForge::CircleCollider2DComponent>();
        collider.Radius = radius;
        collider.Restitution = restitution;

        return entity;
    }

    class PlayerScript : public ByteForge::ScriptableEntity
    {
    protected:
        void OnUpdate(const ByteForge::Timestep) override
        {
            using ByteForge::Input;
            using ByteForge::KeyCode;

            const auto& body = GetComponent<ByteForge::Rigidbody2DComponent>();
            const glm::vec2 velocity = body.GetLinearVelocity();

            float moveX = 0.0f;
            if (Input::IsKeyDown(KeyCode::D) || Input::IsKeyDown(KeyCode::Right)) moveX += 1.0f;
            if (Input::IsKeyDown(KeyCode::A) || Input::IsKeyDown(KeyCode::Left))  moveX -= 1.0f;

            body.SetLinearVelocity({ moveX * m_Speed, velocity.y });

            const glm::vec3 position = GetComponent<ByteForge::TransformComponent>().Position;
            const ByteForge::RaycastHit2D ground = GetEntity().GetScene().Raycast2D(
                { position.x, position.y }, { 0.0f, -1.0f }, m_Radius + 0.05f);
            const bool isGrounded = ground.Hit && ground.HitEntity != GetEntity();

            if (isGrounded && Input::IsKeyPressed(KeyCode::Space))
                body.ApplyLinearImpulseToCenter({ 0.0f, m_JumpImpulse }, true);
        }

    private:
        float m_Speed = 4.0f;
        float m_JumpImpulse = 4.5f;
        float m_Radius = 0.35f;
    };

    class TriggerZoneScript : public ByteForge::ScriptableEntity
    {
    protected:
        void OnCreate() override { UpdateColor(); }

        void OnSensorEnter(const ByteForge::Entity other) override
        {
            ++m_Occupants;
            APP_INFO("TriggerZone: '{}' entered ({} inside)", other.GetTag(), m_Occupants);
            UpdateColor();
        }

        void OnSensorExit(const ByteForge::Entity other) override
        {
            if (m_Occupants > 0) --m_Occupants;
            APP_INFO("TriggerZone: '{}' left ({} inside)", other.GetTag(), m_Occupants);
            UpdateColor();
        }

    private:
        void UpdateColor() const
        {
            GetComponent<ByteForge::SpriteRendererComponent>().Color = m_Occupants > 0
                ? glm::vec4{ 0.3f, 0.9f, 0.4f, 0.6f }
                : glm::vec4{ 0.55f, 0.55f, 0.55f, 0.35f };
        }

        int m_Occupants = 0;
    };

    class CollisionWatcherScript : public ByteForge::ScriptableEntity
    {
    protected:
        void OnCreate() override
        {
            m_BaseColor = GetComponent<ByteForge::SpriteRendererComponent>().Color;
        }

        void OnCollisionEnter(const ByteForge::Entity other) override
        {
            ++m_Contacts;
            APP_INFO("{}: touching '{}' ({} contacts)", GetComponent<ByteForge::TagComponent>().Tag,
                    other.GetTag(), m_Contacts);
            UpdateColor();
        }

        void OnCollisionExit(const ByteForge::Entity other) override
        {
            if (m_Contacts > 0) --m_Contacts;
            APP_INFO("{}: stopped touching '{}' ({} contacts)", GetComponent<ByteForge::TagComponent>().Tag,
                    other.GetTag(), m_Contacts);
            UpdateColor();
        }

    private:
        void UpdateColor() const
        {
            const glm::vec4 highlighted = glm::lerp(m_BaseColor, glm::vec4(1.0f), 0.5f);
            GetComponent<ByteForge::SpriteRendererComponent>().Color = m_Contacts > 0 ? highlighted : m_BaseColor;
        }

        glm::vec4 m_BaseColor{ 1.0f };
        int m_Contacts = 0;
    };
}

class SandboxLayer : public ByteForge::Layer
{
public:
    SandboxLayer()
        : Layer("Sandbox"), m_Camera(6.0f, 1280.0f / 720.0f, -1.0f, 1.0f) {}

    void OnAttach() override
    {
        m_Renderer2D = ByteForge::MakeScope<ByteForge::Renderer2D>();

        SpawnBox(m_Scene, "Ground", { 0.0f, -3.0f, 0.0f }, { 16.0f, 1.0f }, { 0.35f, 0.35f, 0.4f, 1.0f },
                ByteForge::Rigidbody2DComponent::BodyType::Static);

        const auto trigger = SpawnBox(m_Scene, "TriggerZone", { 4.0f, -1.5f, 0.0f }, { 2.0f, 2.0f },
                                { 0.55f, 0.55f, 0.55f, 0.35f }, ByteForge::Rigidbody2DComponent::BodyType::Static);
        trigger.GetComponent<ByteForge::BoxCollider2DComponent>().IsSensor = true;
        trigger.AddComponent<ByteForge::NativeScriptComponent>().Bind<TriggerZoneScript>();

        m_Player = SpawnCircle(m_Scene, "Player", { -6.0f, -1.0f, 0.0f }, 0.35f, { 0.3f, 0.9f, 0.3f, 1.0f },
                               ByteForge::Rigidbody2DComponent::BodyType::Dynamic, 0.0f);
        m_Player.GetComponent<ByteForge::Rigidbody2DComponent>().FixedRotation = true;
        m_Player.AddComponent<ByteForge::NativeScriptComponent>().Bind<PlayerScript>();

        const auto box1 = SpawnBox(m_Scene, "Box1", { -2.0f, 3.0f, 0.0f }, { 0.6f, 0.6f }, { 0.9f, 0.6f, 0.2f, 1.0f },
                             ByteForge::Rigidbody2DComponent::BodyType::Dynamic, 0.1f);
        box1.AddComponent<ByteForge::NativeScriptComponent>().Bind<CollisionWatcherScript>();

        const auto ball1 = SpawnCircle(m_Scene, "Ball1", { -0.5f, 4.0f, 0.0f }, 0.4f, { 0.9f, 0.3f, 0.3f, 1.0f },
                                 ByteForge::Rigidbody2DComponent::BodyType::Dynamic, 0.7f);   // bouncy
        ball1.AddComponent<ByteForge::NativeScriptComponent>().Bind<CollisionWatcherScript>();

        SpawnBox(m_Scene, "Box2", { 1.0f, 3.5f, 0.0f }, { 0.5f, 1.0f }, { 0.3f, 0.5f, 0.9f, 1.0f },
                ByteForge::Rigidbody2DComponent::BodyType::Dynamic, 0.05f);
    }

    void OnUpdate(const ByteForge::Timestep ts) override
    {
        if (ByteForge::Input::IsKeyPressed(ByteForge::KeyCode::X))
        {
            const float x = (static_cast<float>(std::rand() % 1000) / 1000.0f - 0.5f) * 7.0f;
            const std::string name = "Spawned" + std::to_string(m_SpawnCount);

            if (m_SpawnCount % 2 == 0)
            {
                SpawnCircle(m_Scene, name, { x, 4.0f, 0.0f }, 0.3f, { 1.0f, 0.7f, 0.2f, 1.0f },
                           ByteForge::Rigidbody2DComponent::BodyType::Dynamic, 0.5f);
            }
            else
            {
                SpawnBox(m_Scene, name, { x, 4.0f, 0.0f }, { 0.5f, 0.5f }, { 0.5f, 0.3f, 0.9f, 1.0f },
                        ByteForge::Rigidbody2DComponent::BodyType::Dynamic, 0.1f);
            }

            ++m_SpawnCount;
        }

        m_Scene.OnUpdate(ts, *m_Renderer2D, m_Camera);
    }

    void OnImGuiRender() override
    {
        ImGui::Begin("Physics2D Test");
        ImGui::TextUnformatted("A / D or arrows: move, Space: jump (now grounded-only, via Raycast2D)");
        ImGui::TextUnformatted("X: spawn another falling shape");
        ImGui::TextUnformatted("Walk into the gray zone on the right - it turns green while occupied");
        ImGui::TextUnformatted("Box1 and Ball1 brighten while touching anything (OnCollisionEnter/Exit)");
        ImGui::TextUnformatted("(sensor and collision enter/exit are also logged to the console)");
        ImGui::Text("Shapes spawned with X: %d", m_SpawnCount);
        ImGui::End();
    }

private:
    ByteForge::Scope<ByteForge::Renderer2D> m_Renderer2D;
    ByteForge::Scene m_Scene;
    ByteForge::Entity m_Player;
    ByteForge::OrthographicCamera m_Camera;
    int m_SpawnCount = 0;
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Sandbox", .Width = 1280, .Height = 720 });
    app->EnableImGui();
    app->PushLayer(MakeScope<SandboxLayer>());
    return app;
}