#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/SemanticVersion.h"

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace ByteForge
{
    inline constexpr u32 DotNetMajorVersion = 10;

    enum class DotNetSource : u8
    {
        Bundled,
        Hub,
        System
    };

    enum class DotNetComponent : u8
    {
        Runtime,
        Sdk
    };

    struct DotNetCandidate
    {
        fs::path Root;
        DotNetSource Source = DotNetSource::System;
    };

    struct DotNetInstallation
    {
        fs::path Root;
        DotNetSource Source = DotNetSource::System;

        std::optional<SemanticVersion> Runtime;
        std::optional<SemanticVersion> Sdk;

        [[nodiscard]] bool Has(const DotNetComponent component) const
        {
            return component == DotNetComponent::Runtime ? Runtime.has_value() : Sdk.has_value();
        }

        [[nodiscard]] fs::path GetHost() const
        {
#ifdef _WIN32
            return Root / "dotnet.exe";
#else
            return Root / "dotnet";
#endif
        }
    };

    namespace DotNetLocator
    {
        [[nodiscard]] BYTEFORGE_CORE_API fs::path GetHubDirectory();

        [[nodiscard]] BYTEFORGE_CORE_API std::vector<DotNetCandidate> GetCandidates();

        [[nodiscard]] BYTEFORGE_CORE_API std::optional<DotNetInstallation> Inspect(const DotNetCandidate& candidate);

        [[nodiscard]] BYTEFORGE_CORE_API std::optional<DotNetInstallation> FindIn(
            std::span<const DotNetCandidate> candidates, DotNetComponent component);

        [[nodiscard]] BYTEFORGE_CORE_API std::optional<DotNetInstallation> Find(DotNetComponent component);
    }
}
