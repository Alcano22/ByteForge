#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Core/DotNetLocator.h>
#include <Engine/Core/SemanticVersion.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace ByteForge::Hub
{
    struct DotNetDownload
    {
        SemanticVersion Version;
        std::string FileName;
        std::string Url;
        std::string Sha512;
    };

    namespace DotNetPackage
    {
        [[nodiscard]] std::string GetReleaseMetadataUrl();

        [[nodiscard]] std::string_view GetCurrentRid();

        [[nodiscard]] Result<DotNetDownload> ParseLatestSdk(std::string_view json, std::string_view rid);

        [[nodiscard]] fs::path GetDownloadPath(const fs::path& directory);

        [[nodiscard]] Result<DotNetInstallation> Install(const fs::path& archive, const fs::path& directory);

        [[nodiscard]] Result<> Remove(const fs::path& directory);

        void RemoveLeftovers(const fs::path& directory);
    }
}
