#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/PhysicsMaterial.h"
#include "Engine/Assets/TextureAsset.h"
#include "Engine/Scene/UUID.h"

#include <expected>
#include <filesystem>
#include <string>

namespace ByteForge
{
    class BYTEFORGE_API AssetManager
    {
    public:
        static void Update();
        static void Clear();

        [[nodiscard]] static Ref<TextureAsset> LoadTexture2D(UUID handle);
        [[nodiscard]] static Ref<TextureAsset> LoadTexture2D(const std::filesystem::path& relativePath);

        [[nodiscard]] static Ref<PhysicsMaterialAsset> LoadPhysicsMaterial(UUID handle);

        static void SetPhysicsMaterial(UUID handle, const PhysicsMaterial2D& material);
        static bool SavePhysicsMaterial(UUID handle);

        [[nodiscard]] static std::expected<UUID, std::string> CreatePhysicsMaterial(
            const std::filesystem::path& relativePath);

        [[nodiscard]] static std::expected<UUID, std::string> CreateScriptGraph(
            const std::filesystem::path& relativePath);

        static void Reload(UUID handle);
    };
}
