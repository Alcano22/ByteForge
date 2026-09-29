#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace ByteForge
{
    namespace fs = std::filesystem;

    namespace
    {
        constexpr const char* MetaExtension = ".meta";

        bool IsMetaFile(const fs::path& path) { return path.extension() == MetaExtension; }
    }

    fs::path AssetRegistry::s_AssetRoot;
    std::unordered_map<UUID, AssetMetadata> AssetRegistry::s_Assets;
    std::unordered_map<std::string, UUID> AssetRegistry::s_PathToHandle;

    void AssetRegistry::Init(const std::filesystem::path& assetRoot)
    {
        Clear();
        s_AssetRoot = assetRoot;

        if (!fs::exists(s_AssetRoot))
        {
            CORE_WARN("AssetRegistry::Init: asset root '{}' does not exist", s_AssetRoot.string());
            return;
        }

        for (const auto& entry : fs::recursive_directory_iterator(s_AssetRoot))
        {
            if (!entry.is_regular_file() || IsMetaFile(entry.path())) continue;

            Import(fs::relative(entry.path(), s_AssetRoot));
        }

        CORE_INFO("AssetRegistry: indexed {} asset(s) under '{}'", s_Assets.size(), s_AssetRoot.string());
    }

    UUID AssetRegistry::Import(const std::filesystem::path& relativePath)
    {
        const std::string key = relativePath.generic_string();

        if (const auto it = s_PathToHandle.find(key); it != s_PathToHandle.end())
            return it->second;

        const fs::path absolutePath = s_AssetRoot / relativePath;
        const fs::path metaPath = absolutePath.string() + MetaExtension;

        AssetMetadata metadata;
        if (!ReadMeta(metaPath, metadata))
        {
            metadata.Handle = UUID();
            metadata.Type = AssetTypeFromExtension(absolutePath.extension().string());
            metadata.Path = relativePath;
            metadata.Settings = DefaultAssetSettings(metadata.Type);
            WriteMeta(absolutePath, metadata);
        } else
            metadata.Path = relativePath;

        s_Assets[metadata.Handle] = metadata;
        s_PathToHandle[key] = metadata.Handle;
        return metadata.Handle;
    }

    bool AssetRegistry::TryGetMetadata(const UUID handle, AssetMetadata& outMetadata)
    {
        const auto it = s_Assets.find(handle);
        if (it == s_Assets.end())
            return false;

        outMetadata = it->second;
        return true;
    }

    UUID AssetRegistry::GetHandle(const std::filesystem::path& relativePath)
    {
        const auto it = s_PathToHandle.find(relativePath.generic_string());
        return it != s_PathToHandle.end() ? it->second : UUID(0);
    }

    std::filesystem::path AssetRegistry::Resolve(const UUID handle)
    {
        AssetMetadata metadata;
        if (!TryGetMetadata(handle, metadata))
            return {};

        return s_AssetRoot / metadata.Path;
    }

    std::vector<AssetMetadata> AssetRegistry::GetAssetsOfType(const AssetType type)
    {
        std::vector<AssetMetadata> result;
        for (const auto& [handle, metadata] : s_Assets)
        {
            if (metadata.Type == type)
                result.push_back(metadata);
        }

        std::ranges::sort(result, [](const AssetMetadata& a, const AssetMetadata& b) { return a.Path < b.Path; });
        return result;
    }

    bool AssetRegistry::SetSettings(const UUID handle, AssetSettings settings)
    {
        const auto it = s_Assets.find(handle);
        if (it == s_Assets.end())
            return false;

        if (settings.index() != DefaultAssetSettings(it->second.Type).index())
        {
            CORE_ERROR("AssetRegistry::SetSettings: wrong settings type for '{}'", it->second.Path.string());
            return false;
        }

        it->second.Settings = std::move(settings);
        WriteMeta(s_AssetRoot / it->second.Path, it->second);
        return true;
    }

    void AssetRegistry::Clear()
    {
        s_AssetRoot.clear();
        s_Assets.clear();
        s_PathToHandle.clear();
    }

    void AssetRegistry::WriteMeta(const std::filesystem::path& absolutePath, const AssetMetadata& metadata)
    {
        nlohmann::json data;
        data["uuid"] = static_cast<uint64_t>(metadata.Handle);
        data["type"] = AssetTypeToString(metadata.Type);
        data["settings"] = SerializeAssetSettings(metadata.Settings);

        std::ofstream file(absolutePath.string() + MetaExtension);
        if (!file.is_open())
        {
            CORE_ERROR("AssetRegistry: could not write '{}.meta'", absolutePath.string());
            return;
        }

        file << data.dump(4);
    }

    bool AssetRegistry::ReadMeta(const std::filesystem::path& metaPath, AssetMetadata& outMetadata)
    {
        std::ifstream file(metaPath);
        if (!file.is_open())
            return false;

        nlohmann::json data;
        try
        {
            file >> data;
        } catch (const nlohmann::json::parse_error&)
        {
            CORE_ERROR("AssetRegistry: '{}' is not valid JSON", metaPath.string());
            return false;
        }

        if (!data.contains("uuid") || !data.contains("type"))
            return false;

        outMetadata.Handle = UUID(data.at("uuid").get<uint64_t>());
        outMetadata.Type = AssetTypeFromString(data.at("type").get<std::string>());
        outMetadata.Settings = DeserializeAssetSettings(outMetadata.Type,
                                                        data.value("settings", nlohmann::json::object()));
        return true;
    }
}
