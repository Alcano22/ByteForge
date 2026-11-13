#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/PhysicsMaterial.h"
#include "Engine/Assets/TextureAsset.h"
#include "Engine/Assets/AnimationClip.h"
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
        [[nodiscard]] static Ref<TextureAsset> LoadTexture2D(const fs::path& relativePath);

        [[nodiscard]] static Ref<PhysicsMaterialAsset> LoadPhysicsMaterial(UUID handle);

        static void SetPhysicsMaterial(UUID handle, const PhysicsMaterial2D& material);
        static bool SavePhysicsMaterial(UUID handle);

        [[nodiscard]] static Result<UUID> CreatePhysicsMaterial(const fs::path& relativePath);

        [[nodiscard]] static Result<UUID> CreateScriptGraph(const fs::path& relativePath);

        [[nodiscard]] static Ref<AnimationClipAsset> LoadAnimationClip(UUID handle);

        static void SetAnimationClip(UUID handle, AnimationClipData clip);
        static bool SaveAnimationClip(UUID handle);

        [[nodiscard]] static Result<UUID> CreateAnimationClip(const fs::path& relativePath);

        static void Reload(UUID handle);
    };
}
