#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/AssetMetadata.h"

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <expected>

namespace ByteForge
{
    class PakArchive;

    class BYTEFORGE_API AssetRegistry
    {
    public:
        static constexpr const char* GameScriptsEntry = ".scripts/GameScripts.dll";

        static void Init(const fs::path& assetRoot);

        [[nodiscard]] static Result<> Mount(const fs::path& archive);

        [[nodiscard]] static const fs::path& GetAssetRoot() { return s_AssetRoot; }
        [[nodiscard]] static u64 GetRevision() { return s_Revision; }
        [[nodiscard]] static bool IsPacked() { return s_Archive != nullptr; }

        static UUID Import(const fs::path& relativePath);
        static Result<> Move(const fs::path& from, const fs::path& to);

        [[nodiscard]] static bool TryGetMetadata(UUID handle, AssetMetadata& outMetadata);
        [[nodiscard]] static UUID GetHandle(const fs::path& relativePath);

        [[nodiscard]] static fs::path Resolve(UUID handle);

        [[nodiscard]] static Result<std::vector<std::byte>> Read(UUID handle);
        [[nodiscard]] static Result<std::vector<std::byte>> ReadFile(const fs::path& relativePath);
        [[nodiscard]] static Result<nlohmann::json> ReadJson(UUID handle);

        [[nodiscard]] static bool ContainsFile(const fs::path& relativePath);

        [[nodiscard]] static std::vector<AssetMetadata> GetAssetsOfType(AssetType type);

        static bool SetSettings(UUID handle, AssetSettings settings);

        static void Clear();

    private:
        static void WriteMeta(const fs::path& absolutePath, const AssetMetadata& metadata);
        [[nodiscard]] static bool ReadMeta(const fs::path& metaPath, AssetMetadata& outMetadata);

    private:
        static fs::path s_AssetRoot;
        static std::unordered_map<UUID, AssetMetadata> s_Assets;
        static std::unordered_map<std::string, UUID> s_PathToHandle;
        static Scope<PakArchive> s_Archive;

        static u64 s_Revision;
    };
}
