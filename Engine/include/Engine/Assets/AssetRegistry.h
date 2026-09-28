#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/AssetMetadata.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace ByteForge
{
    class BYTEFORGE_API AssetRegistry
    {
    public:
        static void Init(const std::filesystem::path& assetRoot);

        [[nodiscard]] static const std::filesystem::path& GetAssetRoot() { return s_AssetRoot; }

        static UUID Import(const std::filesystem::path& relativePath);

        [[nodiscard]] static bool TryGetMetadata(UUID handle, AssetMetadata& outMetadata);
        [[nodiscard]] static UUID GetHandle(const std::filesystem::path& relativePath);

        [[nodiscard]] static std::filesystem::path Resolve(UUID handle);

        static void Clear();

    private:
        static void WriteMeta(const std::filesystem::path& absolutePath, const AssetMetadata& metadata);
        [[nodiscard]] static bool ReadMeta(const std::filesystem::path& metaPath, AssetMetadata& outMetadata);

    private:
        static std::filesystem::path s_AssetRoot;
        static std::unordered_map<UUID, AssetMetadata> s_Assets;
        static std::unordered_map<std::string, UUID> s_PathToHandle;
    };
}
