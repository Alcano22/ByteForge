#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/TextureAsset.h"
#include "Engine/Scene/UUID.h"

#include <filesystem>

namespace ByteForge
{
    class BYTEFORGE_API AssetManager
    {
    public:
        static void Update();
        static void Clear();

        [[nodiscard]] static Ref<TextureAsset> LoadTexture2D(UUID handle);
        [[nodiscard]] static Ref<TextureAsset> LoadTexture2D(const std::filesystem::path& relativePath);

        static void Reload(UUID handle);
    };
}
