#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    std::unordered_map<UUID, Ref<Texture2D>> AssetManager::s_Textures;

    Ref<Texture2D> AssetManager::LoadTexture2D(const UUID handle)
    {
        if (const auto it = s_Textures.find(handle); it != s_Textures.end())
            return it->second;

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata))
        {
            CORE_ERROR("AssetManager::LoadTexture2D: unknown asset handle {}", static_cast<uint64_t>(handle));
            return nullptr;
        }

        if (metadata.Type != AssetType::Texture2D)
        {
            CORE_ERROR("AssetManager::LoadTexture2D: asset '{}' is a {}, not a Texture2D",
                       metadata.Path.string(), AssetTypeToString(metadata.Type));
            return nullptr;
        }

        const TextureSettings* settings = metadata.GetSettings<TextureSettings>();

        Ref<Texture2D> texture = Texture2D::Load(AssetRegistry::GetAssetRoot() / metadata.Path,
                                                 settings != nullptr ? *settings : TextureSettings{});
        s_Textures.emplace(handle, texture);
        return texture;
    }

    Ref<Texture2D> AssetManager::LoadTexture2D(const std::filesystem::path& relativePath)
    {
        return LoadTexture2D(AssetRegistry::Import(relativePath));
    }

    Ref<Texture2D> AssetManager::Reload(const UUID handle)
    {
        const auto it = s_Textures.find(handle);
        if (it == s_Textures.end())
            return LoadTexture2D(handle);

        Ref<Texture2D> previous = std::move(it->second);
        s_Textures.erase(it);

        Ref<Texture2D> reloaded;
        try
        {
            reloaded = LoadTexture2D(handle);
        } catch (...)
        {
            s_Textures.emplace(handle, std::move(previous));
            throw;
        }

        if (!reloaded)
        {
            s_Textures.emplace(handle, previous);
            return previous;
        }

        return reloaded;
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
