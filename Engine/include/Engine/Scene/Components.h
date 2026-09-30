#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/Sprite.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>

#include <string>
#include <utility>

namespace ByteForge
{
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
        Ref<ByteForge::Sprite> Sprite;
    };

    struct BYTEFORGE_API Rigidbody2DComponent
    {
        enum class BodyType { Static, Kinematic, Dynamic };

        BodyType Type = BodyType::Static;
        bool FixedRotation = false;
        float GravityScale = 1.0f;

        Rigidbody2DComponent() = default;

        Rigidbody2DComponent(const Rigidbody2DComponent& other)
            : Type(other.Type), FixedRotation(other.FixedRotation), GravityScale(other.GravityScale) {}

        Rigidbody2DComponent& operator=(const Rigidbody2DComponent& other)
        {
            Type = other.Type;
            FixedRotation = other.FixedRotation;
            GravityScale = other.GravityScale;
            return *this;
        }

        Rigidbody2DComponent(Rigidbody2DComponent&&) noexcept = default;
        Rigidbody2DComponent& operator=(Rigidbody2DComponent&&) noexcept = default;

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

    struct ScriptComponent
    {
        std::string ClassName;
    };
}
