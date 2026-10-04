#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Core/JobSystem.h"
#include "Engine/Core/Log.h"
#include "Engine/Renderer/ImageDecoder.h"
#include "Engine/Scripting/Visual/ScriptGraphSerializer.h"

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <format>
#include <fstream>
#include <future>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>

namespace ByteForge
{
    namespace fs = std::filesystem;

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
        std::unordered_map<UUID, Ref<PhysicsMaterialAsset>> s_PhysicsMaterials;
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
            const fs::path path = AssetRegistry::GetAssetRoot() / metadata.Path;

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

        std::expected<PhysicsMaterial2D, std::string> ReadPhysicsMaterial(const fs::path& path)
        {
            std::ifstream file(path);
            if (!file.is_open())
                return std::unexpected("the file cannot be opened");

            try
            {
                nlohmann::json data;
                file >> data;

                if (!data.is_object())
                    return std::unexpected("expected a JSON object");

                PhysicsMaterial2D material;
                material.Friction = data.value("friction", material.Friction);
                material.Bounciness = data.value("bounciness", material.Bounciness);
                return material;
            } catch (const nlohmann::json::exception& e)
            {
                return std::unexpected(e.what());
            }
        }

        std::expected<void, std::string> WritePhysicsMaterial(const fs::path& path, const PhysicsMaterial2D material)
        {
            std::ofstream file(path);
            if (!file.is_open())
                return std::unexpected("the file cannot be opened for writing");

            const nlohmann::json data{
                { "friction",   material.Friction   },
                { "bounciness", material.Bounciness }
            };
            file << data.dump(4);
            return {};
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

    Ref<PhysicsMaterialAsset> AssetManager::LoadPhysicsMaterial(const UUID handle)
    {
        if (const auto it = s_PhysicsMaterials.find(handle); it != s_PhysicsMaterials.end())
            return it->second;

        auto asset = MakeRef<PhysicsMaterialAsset>(handle, PhysicsMaterial2D{});
        s_PhysicsMaterials.emplace(handle, asset);

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata) || metadata.Type != AssetType::PhysicsMaterial2D)
        {
            CORE_ERROR("AssetManager: {} is not a known physics material", static_cast<uint64_t>(handle));
            return asset;
        }

        if (auto material = ReadPhysicsMaterial(AssetRegistry::GetAssetRoot() / metadata.Path))
            asset->m_Material = *material;
        else
            CORE_ERROR("AssetManager: cannot load '{}': {}", metadata.Path.generic_string(), material.error());

        return asset;
    }

    void AssetManager::SetPhysicsMaterial(const UUID handle, const PhysicsMaterial2D& material)
    {
        LoadPhysicsMaterial(handle)->m_Material = material;
    }

    bool AssetManager::SavePhysicsMaterial(const UUID handle)
    {
        const auto it = s_PhysicsMaterials.find(handle);
        AssetMetadata metadata;
        if (it == s_PhysicsMaterials.end() || !AssetRegistry::TryGetMetadata(handle, metadata))
            return false;

        const fs::path path = AssetRegistry::GetAssetRoot() / metadata.Path;
        if (const auto written = WritePhysicsMaterial(path, it->second->m_Material); !written)
        {
            CORE_ERROR("AssetManager: cannot save '{}': {}", metadata.Path.generic_string(), written.error());
            return false;
        }

        return true;
    }

    std::expected<UUID, std::string> AssetManager::CreatePhysicsMaterial(const fs::path& relativePath)
    {
        const fs::path path = AssetRegistry::GetAssetRoot() / relativePath;

        std::error_code error;
        if (fs::exists(path, error))
            return std::unexpected(std::format("'{}' already exists", relativePath.generic_string()));

        if (const auto written = WritePhysicsMaterial(path, PhysicsMaterial2D{}); !written)
            return std::unexpected(written.error());

        return AssetRegistry::Import(relativePath);
    }

    std::expected<UUID, std::string> AssetManager::CreateScriptGraph(const std::filesystem::path& relativePath)
    {
        const fs::path path = AssetRegistry::GetAssetRoot() / relativePath;

        std::error_code error;
        if (fs::exists(path, error))
            return std::unexpected(std::format("'{}' already exists", relativePath.generic_string()));

        ScriptGraph graph;
        graph.AddNode(EventNode{ GraphEvent::OnCreate }, { 0.0f, 0.0f });
        graph.AddNode(EventNode{ GraphEvent::OnUpdate }, { 0.0f, 200.0f });

        if (const auto saved = SaveScriptGraph(graph, path); !saved)
            return std::unexpected(saved.error());

        return AssetRegistry::Import(relativePath);
    }

    void AssetManager::Reload(const UUID handle)
    {
        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata)) return;

        if (s_Assets.contains(handle))
            RequestLoad(handle, metadata);

        if (const auto it = s_PhysicsMaterials.find(handle); it != s_PhysicsMaterials.end())
        {
            if (auto material = ReadPhysicsMaterial(AssetRegistry::GetAssetRoot() / metadata.Path))
                it->second->m_Material = *material;
            else
                CORE_ERROR("AssetManager: cannot reload '{}': {}", metadata.Path.generic_string(), material.error());
        }
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
        s_PhysicsMaterials.clear();
        s_Fallback.reset();
    }
}
