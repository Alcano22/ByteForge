#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/UUID.h"

#include <cstdint>

namespace ByteForge
{
    inline constexpr const char* PhysicsMaterialExtension = ".bfphysmat";

    struct PhysicsMaterial2D
    {
        float Friction = 0.6f;
        float Bounciness = 0.0f;

        bool operator==(const PhysicsMaterial2D&) const = default;
    };

    class PhysicsMaterialAsset
    {
    public:
        PhysicsMaterialAsset(const UUID handle, const PhysicsMaterial2D& material)
            : m_Handle(handle), m_Material(material) {}

        [[nodiscard]] UUID GetHandle() const { return m_Handle; }
        [[nodiscard]] bool IsFileBacked() const { return static_cast<uint64_t>(m_Handle) != 0; }
        [[nodiscard]] const PhysicsMaterial2D& GetMaterial() const { return m_Material; }

    private:
        friend class AssetManager;

        UUID m_Handle;
        PhysicsMaterial2D m_Material;
    };
}
