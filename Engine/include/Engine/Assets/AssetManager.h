#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Scene/UUID.h"

#include <filesystem>
#include <unordered_map>

namespace ByteForge
{
    class BYTEFORGE_API AssetManager
    {
    public:
        [[nodiscard]] static Ref<Texture2D> LoadTexture2D(UUID handle);
        [[nodiscard]] static Ref<Texture2D> LoadTexture2D(const std::filesystem::path& relativePath);

        static Ref<Texture2D> Reload(UUID handle);

        [[nodiscard]] static UUID GetTextureHandle(const Ref<Texture2D>& texture);

        static void Clear();

    private:
        static std::unordered_map<UUID, Ref<Texture2D>> s_Textures;
    };
}
