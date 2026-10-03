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

        bool IsInside(const fs::path& path, const fs::path& directory)
        {
            const auto [directoryIt, pathIt] = std::mismatch(directory.begin(), directory.end(),
                                                             path.begin(), path.end());
            return directoryIt == directory.end() && pathIt != path.end();
        }

        fs::path GetMetaPath(const fs::path& absolutePath)
        {
            return absolutePath.string() + MetaExtension;
        }
    }

    fs::path AssetRegistry::s_AssetRoot;
    std::unordered_map<UUID, AssetMetadata> AssetRegistry::s_Assets;
    std::unordered_map<std::string, UUID> AssetRegistry::s_PathToHandle;

    uint64_t AssetRegistry::s_Revision = 0;

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
        {
            metadata.Path = relativePath;

            if (metadata.Type == AssetType::None)
            {
                const AssetType detected = AssetTypeFromExtension(absolutePath.extension().string());
                if (detected != AssetType::None)
                {
                    metadata.Type = detected;
                    metadata.Settings = DefaultAssetSettings(detected);
                    WriteMeta(absolutePath, metadata);
                }
            }
        }

        s_Assets[metadata.Handle] = metadata;
        s_PathToHandle[key] = metadata.Handle;
        ++s_Revision;
        return metadata.Handle;
    }

    std::expected<void, std::string> AssetRegistry::Move(const fs::path& from, const fs::path& to)
    {
        const fs::path fromNormal = from.lexically_normal();
        const fs::path toNormal = to.lexically_normal();

        const fs::path source = s_AssetRoot / fromNormal;
        const fs::path target = s_AssetRoot / toNormal;

        std::error_code error;
        if (!fs::exists(source, error))
            return std::unexpected("it does not exist");

        if (fs::exists(target, error) && !fs::equivalent(source, target, error))
            return std::unexpected(std::format("'{}' already exists", toNormal.generic_string()));

        const bool isDirectory = fs::is_directory(source, error);

        fs::rename(source, target, error);
        if (error)
            return std::unexpected(error.message());

        if (!isDirectory && fs::exists(GetMetaPath(source), error))
        {
            fs::rename(GetMetaPath(source), GetMetaPath(target), error);
            if (error)
            {
                std::error_code rollbackError;
                fs::rename(target, source, rollbackError);
                return std::unexpected(std::format("Could not move the metadata: {}", error.message()));
            }
        }

        for (auto& [handle, metadata] : s_Assets)
        {
            fs::path newPath;
            if (metadata.Path == fromNormal)
                newPath = toNormal;
            else if (isDirectory && IsInside(metadata.Path, fromNormal))
                newPath = toNormal / metadata.Path.lexically_relative(fromNormal);
            else
                continue;

            s_PathToHandle.erase(metadata.Path.generic_string());
            s_PathToHandle[newPath.generic_string()] = handle;
            metadata.Path = std::move(newPath);
        }

        ++s_Revision;
        return {};
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
        ++s_Revision;
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
