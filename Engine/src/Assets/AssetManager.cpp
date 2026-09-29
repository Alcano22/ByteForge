#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Core/JobSystem.h"
#include "Engine/Core/Log.h"
#include "Engine/Renderer/ImageDecoder.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <future>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ByteForge
{
    namespace
    {
        struct DecodedTexture
        {
            DecodedImage Image;
            TextureSettings Settings;
            std::string Error;
        };

        std::unordered_map<UUID, Ref<TextureAsset>> s_Assets;
        std::unordered_map<UUID, std::future<DecodedTexture>> s_Pending;
        Ref<Texture2D> s_Fallback;

        const Ref<Texture2D>& Fallback()
        {
            if (!s_Fallback)
            {
                constexpr std::array<std::byte, 4> magenta{ std::byte{ 255 }, std::byte{ 0 },
                                                            std::byte{ 255 }, std::byte{ 255 } };
                s_Fallback = Texture2D::Create(1, 1, magenta,
                                               { .Filter = TextureFilter::Nearest, .GenerateMips = false });
            }
            return s_Fallback;
        }

        void RequestLoad(const UUID handle, const AssetMetadata& metadata)
        {
            const TextureSettings* stored = metadata.GetSettings<TextureSettings>();
            const TextureSettings settings = stored != nullptr ? *stored : TextureSettings{};
            const std::filesystem::path path = AssetRegistry::GetAssetRoot() / metadata.Path;

            s_Pending.insert_or_assign(handle, JobSystem::Submit([path, settings]
            {
                DecodedTexture result{ .Settings = settings };
                if (auto image = DecodeImage(path))
                    result.Image = std::move(*image);
                else
                    result.Error = std::move(image.error());
                return result;
            }));
        }
    }

    Ref<TextureAsset> AssetManager::LoadTexture2D(const UUID handle)
    {
        if (const auto it = s_Assets.find(handle); it != s_Assets.end())
            return it->second;

        auto asset = MakeRef<TextureAsset>(handle, Fallback());
        s_Assets.emplace(handle, asset);

        AssetMetadata metadata;
        if (AssetRegistry::TryGetMetadata(handle, metadata) && metadata.Type == AssetType::Texture2D)
            RequestLoad(handle, metadata);
        else
        {
            CORE_ERROR("AssetManager: {} is not a known texture asset", static_cast<uint64_t>(handle));
            asset->m_State = AssetLoadState::Failed;
        }

        return asset;
    }

    Ref<TextureAsset> AssetManager::LoadTexture2D(const std::filesystem::path& relativePath)
    {
        return LoadTexture2D(AssetRegistry::Import(relativePath));
    }

    void AssetManager::Reload(const UUID handle)
    {
        AssetMetadata metadata;
        if (s_Assets.contains(handle) && AssetRegistry::TryGetMetadata(handle, metadata))
            RequestLoad(handle, metadata);
    }

    void AssetManager::Update()
    {
        for (auto it = s_Pending.begin(); it != s_Pending.end();)
        {
            if (it->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                ++it;
                continue;
            }

            TextureAsset& asset = *s_Assets.at(it->first);
            DecodedTexture decoded = it->second.get();
            it = s_Pending.erase(it);

            try
            {
                if (!decoded.Error.empty())
                    throw std::runtime_error(decoded.Error);

                const DecodedImage& image = decoded.Image;
                asset.m_Texture = Texture2D::Create(image.Width, image.Height, image.Pixels, decoded.Settings);
                asset.m_State = AssetLoadState::Loaded;
            } catch (const std::exception& e)
            {
                CORE_ERROR("AssetManager: failed to load texture {}: {}",
                           static_cast<uint64_t>(asset.GetHandle()), e.what());

                if (asset.m_State != AssetLoadState::Loaded)
                    asset.m_State = AssetLoadState::Failed;
            }
        }
    }

    void AssetManager::Clear()
    {
        s_Pending.clear();
        s_Assets.clear();
        s_Fallback.reset();
    }
}
