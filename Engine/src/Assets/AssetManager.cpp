#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    std::unordered_map<UUID, Ref<Texture2D>> AssetManager::s_Textures;

    Ref<Texture2D> AssetManager::LoadTexture2D(const UUID handle, const TextureSettings& settings)
    {
        if (const auto it = s_Textures.find(handle); it != s_Textures.end())
            return it->second;

        const std::filesystem::path path = AssetRegistry::Resolve(handle);
        if (path.empty())
        {
            CORE_ERROR("AssetManager::LoadTexture2D: unknown asset handle {}", static_cast<uint64_t>(handle));
            return nullptr;
        }

        Ref<Texture2D> texture = Texture2D::Load(path, settings);
        s_Textures.emplace(handle, texture);
        return texture;
    }

    Ref<Texture2D> AssetManager::LoadTexture2D(const std::filesystem::path& relativePath,
                                               const TextureSettings& settings)
    {
        return LoadTexture2D(AssetRegistry::Import(relativePath), settings);
    }

    UUID AssetManager::GetTextureHandle(const Ref<Texture2D>& texture)
    {
        for (const auto& [handle, cached] : s_Textures)
        {
            if (cached == texture)
                return handle;
        }
        return UUID(0);
    }

    void AssetManager::Clear() { s_Textures.clear(); }
}
