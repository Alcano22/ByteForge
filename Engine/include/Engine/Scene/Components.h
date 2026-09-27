#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/SubTexture2D.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>

#include <string>
#include <utility>
#include <type_traits>

namespace ByteForge
{
    class ScriptableEntity;

    struct UUIDComponent
    {
        UUID ID;
    };

    struct TagComponent
    {
        std::string Tag;
    };

    struct TransformComponent
    {
        glm::vec3 Position{ 0.0f };
        float Rotation = 0.0f;
        glm::vec2 Scale{ 1.0f };
    };

    struct SpriteRendererComponent
    {
        glm::vec4 Color{ 1.0f };

        Ref<SubTexture2D> SubTexture;
    };

    struct BYTEFORGE_API Rigidbody2DComponent
    {
        enum class BodyType { Static, Kinematic, Dynamic };

        BodyType Type = BodyType::Static;
        bool FixedRotation = false;
        float GravityScale = 1.0f;

        [[nodiscard]] bool HasRuntimeBody() const { return m_RuntimeBodyId != 0; }

        [[nodiscard]] glm::vec2 GetLinearVelocity() const;
        void SetLinearVelocity(const glm::vec2& velocity) const;

        void ApplyForceToCenter(const glm::vec2& force, bool wake = true) const;
        void ApplyLinearImpulseToCenter(const glm::vec2& impulse, bool wake = true) const;

    private:
        void RequireRuntimeBody(const char* method) const;

    private:
        friend class Physics2DWorld;

        uint64_t m_RuntimeBodyId = 0;
    };

    struct BoxCollider2DComponent
    {
        glm::vec2 Offset{ 0.0f, 0.0f };
        glm::vec2 Size{ 1.0f, 1.0f };

        float Density = 1.0f;
        float Friction = 0.6f;
        float Restitution = 0.0f;

        bool IsSensor = false;
    };

    struct CircleCollider2DComponent
    {
        glm::vec2 Offset{ 0.0f, 0.0f };
        float Radius = 0.5f;

        float Density = 1.0f;
        float Friction = 0.6f;
        float Restitution = 0.0f;

        bool IsSensor = false;
    };

    struct BYTEFORGE_API NativeScriptComponent
    {
        NativeScriptComponent() = default;
        ~NativeScriptComponent();

        NativeScriptComponent(const NativeScriptComponent&) = delete;
        NativeScriptComponent& operator=(const NativeScriptComponent&) = delete;
        NativeScriptComponent(NativeScriptComponent&& other) noexcept;
        NativeScriptComponent& operator=(NativeScriptComponent&& other) noexcept;

        template<typename T>
        T& Bind()
        {
            static_assert(std::is_base_of_v<ScriptableEntity, T>,
                          "NativeScriptComponent::Bind: T must derive from ScriptableEntity");
            static_assert(std::is_default_constructible_v<T>,
                          "NativeScriptComponent::Bind: T must be default-constructible");

            Reset();

            auto* instance = new T();
            m_Instance = instance;
            return *instance;
        }

    private:
        friend class Scene;

        void Reset();

    private:
        ScriptableEntity* m_Instance = nullptr;
        bool m_Created = false;
    };
}
