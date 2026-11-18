#include "HubCore/EditorReleases.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>

namespace ByteForge::Hub
{
    namespace
    {
        constexpr std::string_view ManifestName = "manifest.json";

        std::string FindManifestUrl(const nlohmann::json& release)
        {
            const auto assets = release.find("assets");
            if (assets == release.end() || !assets->is_array())
                return {};

            for (const nlohmann::json& asset : *assets)
            {
                if (asset.is_object() && asset.value("name", std::string()) == ManifestName)
                    return asset.value("browser_download_url", std::string());
            }
            return {};
        }

        std::optional<EditorRelease> ReadRelease(const nlohmann::json& release)
        {
            if (!release.is_object() || release.value("draft", false))
                return std::nullopt;

            const std::optional<SemanticVersion> version =
                SemanticVersion::Parse(release.value("tag_name", std::string()));
            if (!version)
                return std::nullopt;

            std::string manifestUrl = FindManifestUrl(release);
            if (manifestUrl.empty())
                return std::nullopt;

            return EditorRelease{
                .Version     = *version,
                .PreRelease  = release.value("prerelease", false) || version->IsPreRelease(),
                .ManifestUrl = std::move(manifestUrl),
                .PageUrl     = release.value("html_url", std::string())
            };
        }
    }

    namespace EditorReleases
    {
        Result<std::vector<EditorRelease> > ParseGitHubReleases(const std::string_view json)
        {
            const nlohmann::json data = nlohmann::json::parse(json, nullptr, false);
            if (data.is_discarded())
                return Unexpected("GitHub sent no valid JSON");
            if (!data.is_array())
                return Unexpected("GitHub sent no release list");

            std::vector<EditorRelease> releases;
            for (const nlohmann::json& release : data)
            {
                try
                {
                    if (std::optional<EditorRelease> editor = ReadRelease(release))
                        releases.push_back(std::move(*editor));
                } catch (const nlohmann::json::exception&) {}
            }

            std::ranges::sort(releases, std::ranges::greater{}, &EditorRelease::Version);
            return releases;
        }
    }
}
