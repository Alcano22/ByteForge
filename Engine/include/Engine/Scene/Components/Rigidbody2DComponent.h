#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/PhysicsMaterial.h"
#include "Engine/Scene/PhysicsLayers.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct BYTEFORGE_API Rigidbody2DComponent
    {
        enum class BodyType { Static, Kinematic, Dynamic };

        BodyType Type = BodyType::Static;
        bool FixedRotation = false;
        f32 GravityScale = 1.0f;

        u32 Layer = PhysicsLayers::DefaultLayer;
        u32 CollisionMask = PhysicsLayers::AllLayers;

        Ref<PhysicsMaterialAsset> Material;

        Rigidbody2DComponent() = default;

        Rigidbody2DComponent(const Rigidbody2DComponent& other)
            : Type(other.Type), FixedRotation(other.FixedRotation),
              GravityScale(other.GravityScale), Layer(other.Layer), CollisionMask(other.CollisionMask),
              Material(other.Material) {}

        Rigidbody2DComponent& operator=(const Rigidbody2DComponent& other)
        {
            Type = other.Type;
            FixedRotation = other.FixedRotation;
            GravityScale = other.GravityScale;
            Layer = other.Layer;
            CollisionMask = other.CollisionMask;
            Material = other.Material;
            return *this;
        }

        Rigidbody2DComponent(Rigidbody2DComponent&&) noexcept = default;
        Rigidbody2DComponent& operator=(Rigidbody2DComponent&&) noexcept = default;

        [[nodiscard]] bool HasRuntimeBody() const { return m_RuntimeBodyId != 0; }

        [[nodiscard]] glm::vec2 GetLinearVelocity() const;
        void SetLinearVelocity(const glm::vec2& velocity) const;

        void ApplyForceToCenter(const glm::vec2& force, bool wake = true) const;
        void ApplyLinearImpulseToCenter(const glm::vec2& impulse, bool wake = true) const;

        void Teleport(const glm::vec2& position, f32 rotation) const;

        static void Write(const Rigidbody2DComponent& component, nlohmann::json& json);
        static void Read(Rigidbody2DComponent& component, const nlohmann::json& json);

    private:
        void RequireRuntimeBody(const char* method) const;

    private:
        friend class Physics2DWorld;

        u64 m_RuntimeBodyId = 0;
    };
}
