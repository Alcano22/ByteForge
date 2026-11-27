#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/PhysicsMaterial.h"
#include "Engine/Assets/TextureAsset.h"
#include "Engine/Assets/AnimationClip.h"
#include "Engine/Assets/AnimatorController.h"
#include "Engine/Assets/FontAsset.h"
#include "Engine/Assets/InputActions.h"
#include "Engine/Assets/MaterialAsset.h"
#include "Engine/Assets/Sprite.h"
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

        static void SetFontCacheDirectory(const fs::path& directory);
        [[nodiscard]] static fs::path GetFontCacheFile(UUID handle);

        [[nodiscard]] static Ref<TextureAsset> LoadTexture2D(UUID handle);
        [[nodiscard]] static Ref<TextureAsset> LoadTexture2D(const fs::path& relativePath);

        [[nodiscard]] static Ref<Sprite> LoadSprite(UUID handle);
        static void ReloadSprites(UUID texture);

        [[nodiscard]] static Ref<PhysicsMaterialAsset> LoadPhysicsMaterial(UUID handle);

        static void SetPhysicsMaterial(UUID handle, const PhysicsMaterial2D& material);
        static bool SavePhysicsMaterial(UUID handle);

        [[nodiscard]] static Result<UUID> CreatePhysicsMaterial(const fs::path& relativePath);

        [[nodiscard]] static Result<UUID> CreateScriptGraph(const fs::path& relativePath);

        [[nodiscard]] static Ref<AnimationClipAsset> LoadAnimationClip(UUID handle);

        static void SetAnimationClip(UUID handle, AnimationClipData clip);
        static bool SaveAnimationClip(UUID handle);

        [[nodiscard]] static Result<UUID> CreateAnimationClip(const fs::path& relativePath);

        [[nodiscard]] static Ref<AnimatorControllerAsset> LoadAnimatorController(UUID handle);

        static void SetAnimatorController(UUID handle, AnimatorControllerData controller);
        static bool SaveAnimatorController(UUID handle);

        [[nodiscard]] static Result<UUID> CreateAnimatorController(const fs::path& relativePath);

        [[nodiscard]] static Ref<InputActionsAsset> LoadInputActions(UUID handle);

        static void SetInputActions(UUID handle, InputActionsData data);
        static bool SaveInputActions(UUID handle);

        [[nodiscard]] static Result<UUID> CreateInputActions(const fs::path& relativePath);

        [[nodiscard]] static Ref<MaterialAsset> LoadMaterial(UUID handle);

        static void SetMaterial(UUID handle, MaterialData data);
        static bool SaveMaterial(UUID handle);

        [[nodiscard]] static Result<UUID> CreateMaterial(const fs::path& relativePath);

        [[nodiscard]] static Result<UUID> CreateScene(const fs::path& relativePath);

        [[nodiscard]] static Ref<FontAsset> LoadFont(UUID handle);

        static void Reload(UUID handle);

    private:
        static void RequestFontLoad(UUID handle);
        static void FinishFontLoads();

        static void RefreshSprite(Sprite& sprite);
    };
}
