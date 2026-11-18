#include "HubCore/EditorManifest.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <optional>

namespace ByteForge::Hub
{
    namespace
    {
        constexpr usize Sha256Length = 64;

        bool IsSha256(const std::string_view text)
        {
            return text.size() == Sha256Length && std::ranges::all_of(text, [](const char c)
            {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            });
        }

        std::string ToLower(std::string text)
        {
            std::ranges::transform(text, text.begin(), [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return text;
        }

        Result<EditorPackage> ReadPackage(const nlohmann::json& data)
        {
            if (!data.is_object())
                return Unexpected("a package is no object");

            EditorPackage package{
                .Platform = data.value("platform", std::string()),
                .File     = data.value("file", std::string()),
                .Size     = data.value("size", u64{ 0 }),
                .Sha256   = ToLower(data.value("sha256", std::string()))
            };

            if (package.Platform.empty() || package.File.empty())
                return Unexpected("a package has no platform or file");

            if (package.File.find_first_of("/\\") != std::string::npos || package.File.starts_with('.'))
                return Unexpected("'{}' is no plain file name", package.File);

            if (package.Size == 0)
                return Unexpected("'{}' has no size", package.File);

            if (!IsSha256(package.Sha256))
                return Unexpected("'{}' has no valid SHA-256", package.File);

            return package;
        }
    }

    const EditorPackage* EditorManifest::FindPackage(const std::string_view platform) const
    {
        const auto package = std::ranges::find(Packages, platform, &EditorPackage::Platform);
        return package != Packages.end() ? &*package : nullptr;
    }

    Result<EditorManifest> EditorManifest::Parse(const std::string_view json)
    {
        const nlohmann::json data = nlohmann::json::parse(json, nullptr, false);
        if (data.is_discarded() || !data.is_object())
            return Unexpected("the manifest is no valid JSON object");

        try
        {
            if (const i32 format = data.value("format", 0); format != SupportedFormat)
            {
                return Unexpected("the manifest has format {}, this hub reads format {}; update the hub",
                                  format, SupportedFormat);
            }

            const std::optional<SemanticVersion> version = SemanticVersion::Parse(data.value("version", std::string()));
            if (!version)
                return Unexpected("the manifest has no valid version");

            EditorManifest manifest{ .Version = *version };

            const auto packages = data.find("editor");
            if (packages == data.end() || !packages->is_array())
                return Unexpected("the manifest lists no editor packages");

            for (const nlohmann::json& entry : *packages)
            {
                Result<EditorPackage> package = ReadPackage(entry);
                if (!package)
                    return Unexpected("the manifest is invalid: {}", package.error());

                manifest.Packages.push_back(std::move(*package));
            }

            return manifest;
        } catch (const nlohmann::json::exception& e)
        {
            return Unexpected("the manifest is invalid: {}", e.what());
        }
    }

    std::string_view GetCurrentPlatform()
    {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
        return "windows-x64";
#elif defined(__linux__) && defined(__x86_64__)
        return "linux-x64";
#else
        return {};
#endif
    }
}
