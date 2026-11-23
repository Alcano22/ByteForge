#include "HubCore/DotNetPackage.h"
#include "HubCore/Archive.h"

#include <Engine/Core/Utf8.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <system_error>

namespace ByteForge::Hub
{
    namespace
    {
        constexpr usize Sha512Length = 128;

        constexpr std::string_view WorkSuffixes[] = { ".download", ".installing", ".removing" };

        fs::path GetWorkPath(const fs::path& directory, const std::string_view suffix)
        {
            return directory.parent_path() / PathFromUtf8(std::format(".{}{}", ToUtf8(directory.filename()), suffix));
        }

        std::string ToLower(std::string text)
        {
            std::ranges::transform(text, text.begin(), [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return text;
        }

        bool IsSha512(const std::string_view text)
        {
            return text.size() == Sha512Length && std::ranges::all_of(text, [](const char c)
            {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            });
        }

        bool IsArchive(const std::string_view name)
        {
            return name.ends_with(".zip") || name.ends_with(".tar.gz");
        }

        const nlohmann::json* FindSdk(const nlohmann::json& data, const std::string_view version)
        {
            const auto releases = data.find("releases");
            if (releases == data.end() || !releases->is_array())
                return nullptr;

            const auto matches = [version](const nlohmann::json& sdk)
            {
                return sdk.is_object() && sdk.value("version", std::string()) == version;
            };

            for (const nlohmann::json& release : *releases)
            {
                if (!release.is_object()) continue;

                if (const auto sdk = release.find("sdk"); sdk != release.end() && matches(*sdk))
                    return &*sdk;

                if (const auto sdks = release.find("sdks"); sdks != release.end() && sdks->is_array())
                {
                    for (const nlohmann::json& sdk : *sdks)
                    {
                        if (matches(sdk))
                            return &sdk;
                    }
                }
            }

            return nullptr;
        }

        Result<DotNetDownload> ReadArchiveFile(const nlohmann::json& file, const SemanticVersion& version)
        {
            DotNetDownload download{
                .Version  = version,
                .FileName = file.value("name", std::string()),
                .Url      = file.value("url", std::string()),
                .Sha512   = ToLower(file.value("hash", std::string()))
            };

            if (!download.Url.starts_with("https://"))
                return Unexpected("'{}' has no https download", download.FileName);

            if (!IsSha512(download.Sha512))
                return Unexpected("'{}' has no valid SHA-512", download.FileName);

            return download;
        }

        Result<> RemoveFolder(const fs::path& directory, const fs::path& removing)
        {
            std::error_code error;
            fs::remove_all(removing, error);

            fs::rename(directory, removing, error);
            if (error)
                return Unexpected("cannot remove the .NET SDK (is the editor still running?): {}", error.message());

            fs::remove_all(removing, error);
            if (error)
                return Unexpected("the .NET SDK was removed, but cleaning up failed: {}", error.message());

            return {};
        }
    }

    namespace DotNetPackage
    {
        std::string GetReleaseMetadataUrl()
        {
            return std::format("https://builds.dotnet.microsoft.com/dotnet/release-metadata/{}.0/releases.json",
                               DotNetMajorVersion);
        }

        std::string_view GetCurrentRid()
        {
            #if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
            return "win-x64";
#elif defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
            return "win-arm64";
#elif defined(__APPLE__) && defined(__aarch64__)
            return "osx-arm64";
#elif defined(__APPLE__) && defined(__x86_64__)
            return "osx-x64";
#elif defined(__linux__) && defined(__x86_64__)
            return "linux-x64";
#elif defined(__linux__) && defined(__aarch64__)
            return "linux-arm64";
#else
            return {};
#endif
        }

        Result<DotNetDownload> ParseLatestSdk(const std::string_view json, const std::string_view rid)
        {
            if (rid.empty())
                return Unexpected("Microsoft offers no .NET SDK for this platform");

            const nlohmann::json data = nlohmann::json::parse(json, nullptr, false);
            if (data.is_discarded() || !data.is_object())
                return Unexpected("the .NET release metadata is no valid JSON object");

            try
            {
                const std::string expected = std::format("{}.0", DotNetMajorVersion);
                if (const std::string channel = data.value("channel-version", std::string()); channel != expected)
                    return Unexpected("the release metadata is for .NET '{}', not {}", channel, expected);

                const std::string latest = data.value("latest-sdk", std::string());
                const std::optional<SemanticVersion> version = SemanticVersion::Parse(latest);
                if (!version)
                    return Unexpected("the release metadata names no valid latest SDK");

                const nlohmann::json* sdk = FindSdk(data, latest);
                if (sdk == nullptr)
                    return Unexpected("the release metadata doesn't describe SDK {}", latest);

                const auto files = sdk->find("files");
                if (files == sdk->end() || !files->is_array())
                    return Unexpected("SDK {} lists no files", latest);

                for (const nlohmann::json& file : *files)
                {
                    if (!file.is_object() || file.value("rid", std::string()) != rid) continue;

                    if (IsArchive(file.value("name", std::string())))
                        return ReadArchiveFile(file, *version);
                }

                return Unexpected("SDK {} has no archive for {}", latest, rid);
            } catch (const nlohmann::json::exception& e)
            {
                return Unexpected("the .NET release metadata is invalid: {}", e.what());
            }
        }

        fs::path GetDownloadPath(const fs::path& directory)
        {
            return GetWorkPath(directory, ".download");
        }

        Result<DotNetInstallation> Install(const fs::path& archive, const fs::path& directory)
        {
            const fs::path staging = GetWorkPath(directory, ".installing");
            const fs::path previous = GetWorkPath(directory, ".removing");

            std::error_code error;
            fs::remove_all(staging, error);

            struct StagingCleanup
            {
                const fs::path& Path;

                ~StagingCleanup()
                {
                    std::error_code ignored;
                    fs::remove_all(Path, ignored);
                }
            } cleanup{ staging };

            if (Result<> extracted = ExtractArchive(archive, staging); !extracted)
                return Unexpected(extracted.error());

            const std::optional<DotNetInstallation> unpacked =
                DotNetLocator::Inspect({ .Root = staging, .Source = DotNetSource::Hub });
            if (!unpacked || !unpacked->Has(DotNetComponent::Sdk))
                return Unexpected("the archive contains no .NET {} SDK", DotNetMajorVersion);

            const bool replacing = fs::exists(directory, error);
            if (replacing)
            {
                fs::remove_all(previous, error);
                fs::rename(directory, previous, error);
                if (error)
                {
                    return Unexpected("cannot replace the installed .NET SDK (is the editor still running?): {}",
                                      error.message());
                }
            }

            fs::create_directories(directory.parent_path(), error);
            fs::rename(staging, directory, error);
            if (error)
            {
                std::error_code ignored;
                if (replacing)
                    fs::rename(previous, directory, ignored);

                return Unexpected("cannot move the .NET SDK into place: {}", error.message());
            }

            std::error_code ignored;
            fs::remove_all(previous, ignored);

            std::optional<DotNetInstallation> installed =
                DotNetLocator::Inspect({ .Root = directory, .Source = DotNetSource::Hub });
            if (!installed)
                return Unexpected("the .NET SDK is gone right after installing it");

            return std::move(*installed);
        }

        Result<> Remove(const fs::path& directory)
        {
            std::error_code error;
            if (!fs::exists(directory, error))
                return {};

            if (!DotNetLocator::Inspect({ .Root = directory, .Source = DotNetSource::Hub }))
                return Unexpected("'{}' is no .NET installation, it is left alone", ToUtf8(directory));

            return RemoveFolder(directory, GetWorkPath(directory, ".removing"));
        }

        void RemoveLeftovers(const fs::path& directory)
        {
            for (const std::string_view suffix : WorkSuffixes)
            {
                std::error_code ignored;
                fs::remove_all(GetWorkPath(directory, suffix), ignored);
            }
        }
    }
}
