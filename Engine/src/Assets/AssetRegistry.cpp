#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Assets/Pak.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/FileSystem.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <span>
#include <system_error>

namespace ByteForge
{
    namespace
    {
        bool IsTemporaryFile(const fs::path& path) { return path.extension() == TemporaryFileExtension; }

        bool IsInside(const fs::path& path, const fs::path& directory)
        {
            const auto [directoryIt, pathIt] = std::mismatch(directory.begin(), directory.end(),
                                                             path.begin(), path.end());
            return directoryIt == directory.end() && pathIt != path.end();
        }

        fs::path GetMetaPath(const fs::path& absolutePath)
        {
            fs::path metaPath = absolutePath;
            metaPath += AssetRegistry::MetaExtension;
            return metaPath;
        }

        bool ParseMeta(const nlohmann::json& data, AssetMetadata& outMetadata)
        {
            if (!data.is_object() || !data.contains("uuid") || !data.contains("type"))
                return false;

            try
            {
                outMetadata.Handle = UUID(data.at("uuid").get<u64>());
                outMetadata.Type = AssetTypeFromString(data.at("type").get<std::string>());
                outMetadata.Settings = DeserializeAssetSettings(outMetadata.Type,
                                                                data.value("settings", nlohmann::json::object()));
                return true;
            } catch (const nlohmann::json::exception&)
            {
                return false;
            }
        }

        bool ParseMetaBytes(const std::span<const std::byte> bytes, const std::string_view source,
                            AssetMetadata& outMetadata)
        {
            const Result<nlohmann::json> json = ParseJson(bytes, source);
            return json && ParseMeta(*json, outMetadata);
        }

        bool IsEngineData(const fs::path& path)
        {
            return !path.empty() && ToUtf8(*path.begin()).starts_with('.');
        }
    }

    // AssetSource

    Result<std::vector<std::byte> > AssetSource::Read(const fs::path& relativePath) const
    {
        if (m_Archive)
            return m_Archive->Read(ToGenericUtf8(relativePath));

        return ReadBinaryFile(m_Root / relativePath);
    }

    // AssetRegistry

    fs::path AssetRegistry::s_AssetRoot;
    std::unordered_map<UUID, AssetMetadata> AssetRegistry::s_Assets;
    std::unordered_map<std::string, UUID> AssetRegistry::s_PathToHandle;
    Ref<PakArchive> AssetRegistry::s_Archive;

    u64 AssetRegistry::s_Revision = 0;

    void AssetRegistry::Init(const fs::path& assetRoot)
    {
        Clear();
        s_AssetRoot = assetRoot;

        if (!fs::exists(s_AssetRoot))
        {
            CORE_WARN("AssetRegistry::Init: asset root '{}' does not exist", ToUtf8(s_AssetRoot));
            return;
        }

        for (const auto& entry : fs::recursive_directory_iterator(s_AssetRoot))
        {
            if (!entry.is_regular_file() || IsMetaFile(entry.path()) || IsTemporaryFile(entry.path())) continue;

            Import(fs::relative(entry.path(), s_AssetRoot));
        }

        CORE_INFO("AssetRegistry: indexed {} asset(s) under '{}'", s_Assets.size(), ToUtf8(s_AssetRoot));
    }

    Result<> AssetRegistry::Mount(const fs::path& archive)
    {
        Clear();

        Result<Scope<PakArchive>> opened = PakArchive::Open(archive);
        if (!opened)
            return Unexpected(opened.error());

        s_AssetRoot = "assets";

        usize skipped = 0;
        for (const PakEntry& entry : (*opened)->GetEntries())
        {
            const fs::path path = PathFromUtf8(entry.Path);
            if (IsMetaFile(path) || IsEngineData(path)) continue;

            const Result<std::vector<std::byte>> meta = (*opened)->Read(entry.Path + MetaExtension);

            AssetMetadata metadata;
            if (!meta || !ParseMetaBytes(*meta, entry.Path, metadata))
            {
                CORE_WARN("AssetRegistry: '{}' has no valid metadata in the archive and is skipped", entry.Path);
                ++skipped;
                continue;
            }

            metadata.Path = path;
            s_PathToHandle[entry.Path] = metadata.Handle;
            s_Assets[metadata.Handle] = std::move(metadata);
        }

        s_Archive = std::move(*opened);
        ++s_Revision;

        CORE_INFO("AssetRegistry: mounted {} asset(s) from '{}'{}", s_Assets.size(), ToGenericUtf8(archive),
                  skipped > 0 ? std::format(" ({} skipped)", skipped) : std::string());
        return {};
    }

    UUID AssetRegistry::Import(const fs::path& relativePath)
    {
        const std::string key = ToGenericUtf8(relativePath);

        if (const auto it = s_PathToHandle.find(key); it != s_PathToHandle.end())
            return it->second;

        if (IsPacked())
        {
            CORE_ERROR("AssetRegistry: '{}' is not in the archive", key);
            return UUID(0);
        }

        const fs::path absolutePath = s_AssetRoot / relativePath;
        const fs::path metaPath = GetMetaPath(absolutePath);

        AssetMetadata metadata;
        if (!ReadMeta(metaPath, metadata))
        {
            metadata.Handle = UUID();
            metadata.Type = AssetTypeFromExtension(ToUtf8(absolutePath.extension()));
            metadata.Path = relativePath;
            metadata.Settings = DefaultAssetSettings(metadata.Type);
            WriteMeta(absolutePath, metadata);
        } else
        {
            metadata.Path = relativePath;

            if (metadata.Type == AssetType::None)
            {
                const AssetType detected = AssetTypeFromExtension(ToUtf8(absolutePath.extension()));
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

    Result<> AssetRegistry::Move(const fs::path& from, const fs::path& to)
    {
        if (IsPacked())
            return Unexpected("the assets are packed and read-only");

        const fs::path fromNormal = from.lexically_normal();
        const fs::path toNormal = to.lexically_normal();

        const fs::path source = s_AssetRoot / fromNormal;
        const fs::path target = s_AssetRoot / toNormal;

        std::error_code error;
        if (!fs::exists(source, error))
            return Unexpected("it does not exist");

        if (fs::exists(target, error) && !fs::equivalent(source, target, error))
            return Unexpected("'{}' already exists", ToGenericUtf8(toNormal));

        const bool isDirectory = fs::is_directory(source, error);

        fs::rename(source, target, error);
        if (error)
            return Unexpected(error.message());

        if (!isDirectory && fs::exists(GetMetaPath(source), error))
        {
            fs::rename(GetMetaPath(source), GetMetaPath(target), error);
            if (error)
            {
                std::error_code rollbackError;
                fs::rename(target, source, rollbackError);
                return Unexpected("Could not move the metadata: {}", error.message());
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

            s_PathToHandle.erase(ToGenericUtf8(metadata.Path));
            s_PathToHandle[ToGenericUtf8(newPath)] = handle;
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

    UUID AssetRegistry::GetHandle(const fs::path& relativePath)
    {
        const auto it = s_PathToHandle.find(ToGenericUtf8(relativePath));
        return it != s_PathToHandle.end() ? it->second : UUID(0);
    }

    fs::path AssetRegistry::Resolve(const UUID handle)
    {
        AssetMetadata metadata;
        if (!TryGetMetadata(handle, metadata))
            return {};

        return s_AssetRoot / metadata.Path;
    }

    Result<std::vector<std::byte>> AssetRegistry::Read(const UUID handle)
    {
        const auto it = s_Assets.find(handle);
        if (it == s_Assets.end())
            return Unexpected("{} is not a known asset", static_cast<u64>(handle));

        return ReadFile(it->second.Path);
    }

    Result<std::vector<std::byte>> AssetRegistry::ReadFile(const fs::path& relativePath)
    {
        return GetSource().Read(relativePath);
    }

    Result<nlohmann::json> AssetRegistry::ReadJson(const UUID handle)
    {
        const auto it = s_Assets.find(handle);
        if (it == s_Assets.end())
            return Unexpected("{} is not a known asset", static_cast<u64>(handle));

        const Result<std::vector<std::byte>> bytes = ReadFile(it->second.Path);
        if (!bytes)
            return Unexpected(bytes.error());

        return ParseJson(*bytes, ToGenericUtf8(it->second.Path));
    }

    bool AssetRegistry::ContainsFile(const fs::path& relativePath)
    {
        if (s_Archive)
            return s_Archive->Contains(ToGenericUtf8(relativePath));

        std::error_code error;
        return fs::is_regular_file(s_AssetRoot / relativePath, error);
    }

    AssetSource AssetRegistry::GetSource()
    {
        AssetSource source;
        source.m_Root = s_AssetRoot;
        source.m_Archive = s_Archive;
        return source;
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
            CORE_ERROR("AssetRegistry::SetSettings: wrong settings type for '{}'", ToUtf8(it->second.Path));
            return false;
        }

        it->second.Settings = std::move(settings);
        WriteMeta(s_AssetRoot / it->second.Path, it->second);
        return true;
    }

    void AssetRegistry::MarkModified(const UUID handle)
    {
        if (s_Assets.contains(handle))
            ++s_Revision;
    }

    void AssetRegistry::Clear()
    {
        s_AssetRoot.clear();
        s_Assets.clear();
        s_PathToHandle.clear();
        s_Archive.reset();
        ++s_Revision;
    }

    void AssetRegistry::WriteMeta(const fs::path& absolutePath, const AssetMetadata& metadata)
    {
        if (IsPacked()) return;

        nlohmann::json data;
        data["uuid"] = static_cast<u64>(metadata.Handle);
        data["type"] = AssetTypeToString(metadata.Type);
        data["settings"] = SerializeAssetSettings(metadata.Settings);

        if (const auto written = WriteJsonFile(GetMetaPath(absolutePath), data); !written)
            CORE_ERROR("AssetRegistry: {}", written.error());
    }

    bool AssetRegistry::ReadMeta(const fs::path& metaPath, AssetMetadata& outMetadata)
    {
        std::error_code error;
        if (!fs::exists(metaPath, error))
            return false;

        const Result<nlohmann::json> data = ReadJsonFile(metaPath);
        if (!data)
        {
            CORE_ERROR("AssetRegistry: {}", data.error());
            return false;
        }

        return ParseMeta(*data, outMetadata);
    }
}
