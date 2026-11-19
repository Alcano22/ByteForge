#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Assets/Prefab.h"
#include "Engine/Core/JobSystem.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/RuntimePaths.h"
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
#include <utility>

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
        std::unordered_map<UUID, Ref<PhysicsMaterialAsset>> s_PhysicsMaterials;
        std::unordered_map<UUID, Ref<AnimationClipAsset>> s_AnimationClips;
        std::unordered_map<UUID, Ref<AnimatorControllerAsset>> s_AnimatorControllers;
        std::unordered_map<UUID, Ref<FontAsset>> s_Fonts;
        std::unordered_map<UUID, std::future<Result<FontAtlasData>>> s_PendingFonts;
        Ref<Texture2D> s_Fallback;

        const UUID BuiltInFont{ 0 };

        fs::path GetBuiltInFontPath()
        {
            return RuntimePaths::GetDataDirectory() / "fonts" / "inter_regular.ttf";
        }

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
            const fs::path path = metadata.Path;
            AssetSource source = AssetRegistry::GetSource();

            s_Pending.insert_or_assign(handle, JobSystem::Submit([path, settings, source = std::move(source)]
            {
                DecodedTexture result{ .Settings = settings };

                Result<std::vector<std::byte>> encoded = source.Read(path);
                if (!encoded)
                {
                    result.Error = std::move(encoded.error());
                    return result;
                }

                if (auto image = DecodeImage(*encoded, ToGenericUtf8(path)))
                    result.Image = std::move(*image);
                else
                    result.Error = std::move(image.error());
                return result;
            }));
        }

        Result<PhysicsMaterial2D> ReadPhysicsMaterial(const UUID handle)
        {
            const Result<nlohmann::json> data = AssetRegistry::ReadJson(handle);
            if (!data)
                return Unexpected(data.error());

            if (!data->is_object())
                return Unexpected("expected a JSON object");

            try
            {
                PhysicsMaterial2D material;
                material.Friction = data->value("friction", material.Friction);
                material.Bounciness = data->value("bounciness", material.Bounciness);
                return material;
            } catch (const nlohmann::json::exception& e)
            {
                return Unexpected(e.what());
            }
        }

        Result<> WritePhysicsMaterial(const fs::path& path, const PhysicsMaterial2D material)
        {
            const nlohmann::json data{
                { "friction",   material.Friction   },
                { "bounciness", material.Bounciness }
            };

            return WriteJsonFile(path, data);
        }

        Result<AnimationClipData> ReadAnimationClip(const UUID handle)
        {
            const Result<nlohmann::json> data = AssetRegistry::ReadJson(handle);
            if (!data)
                return Unexpected(data.error());

            return ParseAnimationClip(*data);
        }

        Result<AnimatorControllerData> ReadAnimatorController(const UUID handle)
        {
            const Result<nlohmann::json> data = AssetRegistry::ReadJson(handle);
            if (!data)
                return Unexpected(data.error());

            return ParseAnimatorController(*data);
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
            CORE_ERROR("AssetManager: {} is not a known texture asset", static_cast<u64>(handle));
            asset->m_State = AssetLoadState::Failed;
        }

        return asset;
    }

    Ref<TextureAsset> AssetManager::LoadTexture2D(const fs::path& relativePath)
    {
        return LoadTexture2D(AssetRegistry::Import(relativePath));
    }

    void AssetManager::RequestFontLoad(const UUID handle)
    {
        if (handle == BuiltInFont)
        {
            s_PendingFonts.insert_or_assign(handle, JobSystem::Submit([path = GetBuiltInFontPath()]()
                -> Result<FontAtlasData>
            {
                const Result<std::vector<std::byte>> file = ReadBinaryFile(path);
                if (!file)
                    return Unexpected(file.error());
                return BuildFontAtlas(*file);
            }));
            return;
        }

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata) || metadata.Type != AssetType::Font)
        {
            CORE_ERROR("AssetManager: {} is not a known font", static_cast<u64>(handle));
            s_Fonts.at(handle)->m_State = AssetLoadState::Failed;
            return;
        }

        s_PendingFonts.insert_or_assign(handle, JobSystem::Submit(
            [path = metadata.Path, source = AssetRegistry::GetSource()]() -> Result<FontAtlasData>
        {
            const Result<std::vector<std::byte>> file = source.Read(path);
            if (!file)
                return Unexpected(file.error());
            return BuildFontAtlas(*file);
        }));
    }

    void AssetManager::FinishFontLoads()
    {
        for (auto it = s_PendingFonts.begin(); it != s_PendingFonts.end();)
        {
            if (it->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                ++it;
                continue;
            }

            FontAsset& asset = *s_Fonts.at(it->first);
            Result<FontAtlasData> atlas = it->second.get();
            it = s_PendingFonts.erase(it);

            const u64 handle = static_cast<u64>(asset.GetHandle());
            try
            {
                if (!atlas)
                    throw Error(atlas.error());

                asset.m_Font = MakeRef<Font>(std::move(*atlas));
                asset.m_State = AssetLoadState::Loaded;
            } catch (const std::exception& e)
            {
                if (handle == 0)
                    CORE_ERROR("AssetManager: cannot load the built-in font: {}", e.what());
                else
                    CORE_ERROR("AssetManager: failed to load font {}: {}", handle, e.what());

                if (asset.m_State != AssetLoadState::Loaded)
                    asset.m_State = AssetLoadState::Failed;
            }
        }
    }

    Ref<FontAsset> AssetManager::LoadFont(const UUID handle)
    {
        if (const auto it = s_Fonts.find(handle); it != s_Fonts.end())
            return it->second;

        auto asset = MakeRef<FontAsset>(handle);
        s_Fonts.emplace(handle, asset);

        RequestFontLoad(handle);
        return asset;
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
            CORE_ERROR("AssetManager: {} is not a known physics material", static_cast<u64>(handle));
            return asset;
        }

        if (auto material = ReadPhysicsMaterial(handle))
            asset->m_Material = *material;
        else
            CORE_ERROR("AssetManager: cannot load '{}': {}", ToGenericUtf8(metadata.Path), material.error());

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
            CORE_ERROR("AssetManager: cannot save '{}': {}", ToGenericUtf8(metadata.Path), written.error());
            return false;
        }

        return true;
    }

    Result<UUID> AssetManager::CreatePhysicsMaterial(const fs::path& relativePath)
    {
        const fs::path path = AssetRegistry::GetAssetRoot() / relativePath;

        std::error_code error;
        if (fs::exists(path, error))
            return Unexpected("'{}' already exists", ToGenericUtf8(relativePath));

        if (const auto written = WritePhysicsMaterial(path, PhysicsMaterial2D{}); !written)
            return Unexpected(written.error());

        return AssetRegistry::Import(relativePath);
    }

    Result<UUID> AssetManager::CreateScriptGraph(const fs::path& relativePath)
    {
        const fs::path path = AssetRegistry::GetAssetRoot() / relativePath;

        std::error_code error;
        if (fs::exists(path, error))
            return Unexpected("'{}' already exists", ToGenericUtf8(relativePath));

        ScriptGraph graph;
        graph.AddNode(EventNode{ GraphEvent::OnCreate }, { 0.0f, 0.0f });
        graph.AddNode(EventNode{ GraphEvent::OnUpdate }, { 0.0f, 200.0f });

        if (const auto saved = SaveScriptGraph(graph, path); !saved)
            return Unexpected(saved.error());

        return AssetRegistry::Import(relativePath);
    }

    Ref<AnimationClipAsset> AssetManager::LoadAnimationClip(const UUID handle)
    {
        if (const auto it = s_AnimationClips.find(handle); it != s_AnimationClips.end())
            return it->second;

        AnimationClipData data;

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata) || metadata.Type != AssetType::AnimationClip)
            CORE_ERROR("AssetManager: {} is not a known animation clip", static_cast<u64>(handle));
        else if (auto parsed = ReadAnimationClip(handle))
            data = std::move(*parsed);
        else
            CORE_ERROR("AssetManager: cannot load '{}': {}", ToGenericUtf8(metadata.Path), parsed.error());

        auto asset = MakeRef<AnimationClipAsset>(handle, std::move(data));
        s_AnimationClips.emplace(handle, asset);
        return asset;
    }

    void AssetManager::SetAnimationClip(const UUID handle, AnimationClipData clip)
    {
        LoadAnimationClip(handle)->SetData(std::move(clip));
    }

    bool AssetManager::SaveAnimationClip(const UUID handle)
    {
        const auto it = s_AnimationClips.find(handle);
        AssetMetadata metadata;
        if (it == s_AnimationClips.end() || !AssetRegistry::TryGetMetadata(handle, metadata))
            return false;

        const fs::path path = AssetRegistry::GetAssetRoot() / metadata.Path;
        if (const auto written = WriteJsonFile(path, SerializeAnimationClip(it->second->GetData())); !written)
        {
            CORE_ERROR("AssetManager: cannot save '{}': {}", ToGenericUtf8(metadata.Path), written.error());
            return false;
        }

        return true;
    }

    Result<UUID> AssetManager::CreateAnimationClip(const fs::path& relativePath)
    {
        const fs::path path = AssetRegistry::GetAssetRoot() / relativePath;

        std::error_code error;
        if (fs::exists(path, error))
            return Unexpected("'{}' already exists", ToGenericUtf8(relativePath));

        if (const auto written = WriteJsonFile(path, SerializeAnimationClip(AnimationClipData{})); !written)
            return Unexpected(written.error());

        return AssetRegistry::Import(relativePath);
    }

    Ref<AnimatorControllerAsset> AssetManager::LoadAnimatorController(const UUID handle)
    {
        if (const auto it = s_AnimatorControllers.find(handle); it != s_AnimatorControllers.end())
            return it->second;

        AnimatorControllerData data;

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata) || metadata.Type != AssetType::AnimatorController)
            CORE_ERROR("AssetManager: {} is not a known animator controller", static_cast<u64>(handle));
        else if (auto parsed = ReadAnimatorController(handle))
            data = std::move(*parsed);
        else
            CORE_ERROR("AssetManager: cannot load '{}': {}", ToGenericUtf8(metadata.Path), parsed.error());

        auto asset = MakeRef<AnimatorControllerAsset>(handle, std::move(data));
        s_AnimatorControllers.emplace(handle, asset);
        return asset;
    }

    void AssetManager::SetAnimatorController(const UUID handle, AnimatorControllerData controller)
    {
        LoadAnimatorController(handle)->SetData(std::move(controller));
    }

    bool AssetManager::SaveAnimatorController(const UUID handle)
    {
        const auto it = s_AnimatorControllers.find(handle);
        AssetMetadata metadata;
        if (it == s_AnimatorControllers.end() || !AssetRegistry::TryGetMetadata(handle, metadata))
            return false;

        const fs::path path = AssetRegistry::GetAssetRoot() / metadata.Path;
        if (const auto written = WriteJsonFile(path, SerializeAnimatorController(it->second->GetData())); !written)
        {
            CORE_ERROR("AssetManager: cannot save '{}': {}", ToGenericUtf8(metadata.Path), written.error());
            return false;
        }

        return true;
    }

    Result<UUID> AssetManager::CreateAnimatorController(const fs::path& relativePath)
    {
        const fs::path path = AssetRegistry::GetAssetRoot() / relativePath;

        std::error_code error;
        if (fs::exists(path, error))
            return Unexpected("'{}' already exists", ToGenericUtf8(relativePath));

        if (const auto written = WriteJsonFile(path, SerializeAnimatorController(AnimatorControllerData{})); !written)
            return Unexpected(written.error());

        return AssetRegistry::Import(relativePath);
    }

    void AssetManager::Reload(const UUID handle)
    {
        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata)) return;

        if (s_Assets.contains(handle))
            RequestLoad(handle, metadata);

        if (s_Fonts.contains(handle))
            RequestFontLoad(handle);

        if (const auto it = s_PhysicsMaterials.find(handle); it != s_PhysicsMaterials.end())
        {
            if (auto material = ReadPhysicsMaterial(handle))
                it->second->m_Material = *material;
            else
                CORE_ERROR("AssetManager: cannot reload '{}': {}", ToGenericUtf8(metadata.Path), material.error());
        }

        if (const auto it = s_AnimationClips.find(handle); it != s_AnimationClips.end())
        {
            if (auto clip = ReadAnimationClip(handle))
                it->second->SetData(std::move(*clip));
            else
                CORE_ERROR("AssetManager: cannot reload '{}': {}", ToGenericUtf8(metadata.Path), clip.error());
        }

        if (const auto it = s_AnimatorControllers.find(handle); it != s_AnimatorControllers.end())
        {
            if (auto controller = ReadAnimatorController(handle))
                it->second->SetData(std::move(*controller));
            else
                CORE_ERROR("AssetManager: cannot reload '{}': {}", ToGenericUtf8(metadata.Path), controller.error());
        }
    }

    void AssetManager::Update()
    {
        FinishFontLoads();

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
                    throw Error(decoded.Error);

                const DecodedImage& image = decoded.Image;
                asset.m_Texture = Texture2D::Create(image.Width, image.Height, image.Pixels, decoded.Settings);
                asset.m_State = AssetLoadState::Loaded;
            } catch (const std::exception& e)
            {
                CORE_ERROR("AssetManager: failed to load texture {}: {}",
                           static_cast<u64>(asset.GetHandle()), e.what());

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
        s_AnimationClips.clear();
        s_AnimatorControllers.clear();
        s_PendingFonts.clear();
        s_Fonts.clear();
        s_Fallback.reset();
        Prefab::Clear();
    }
}
