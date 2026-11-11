#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/AssetMetadata.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <expected>

namespace ByteForge
{
    class BYTEFORGE_API AssetRegistry
    {
    public:
        static void Init(const fs::path& assetRoot);

        [[nodiscard]] static const fs::path& GetAssetRoot() { return s_AssetRoot; }
        [[nodiscard]] static u64 GetRevision() { return s_Revision; }

        static UUID Import(const fs::path& relativePath);
        static std::expected<void, std::string> Move(const fs::path& from,
                                                     const fs::path& to);

        [[nodiscard]] static bool TryGetMetadata(UUID handle, AssetMetadata& outMetadata);
        [[nodiscard]] static UUID GetHandle(const fs::path& relativePath);

        [[nodiscard]] static fs::path Resolve(UUID handle);

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

        static u64 s_Revision;
    };
}
