#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Core/SemanticVersion.h>

#include <string>
#include <string_view>
#include <vector>

namespace ByteForge::Hub
{
    struct EditorPackage
    {
        std::string Platform;
        std::string File;
        u64 Size = 0;
        std::string Sha256;
    };

    struct EditorManifest
    {
        static constexpr i32 SupportedFormat = 1;

        SemanticVersion Version;
        std::vector<EditorPackage> Packages;

        [[nodiscard]] const EditorPackage* FindPackage(std::string_view platform) const;

        [[nodiscard]] static Result<EditorManifest> Parse(std::string_view json);
    };

    [[nodiscard]] std::string_view GetCurrentPlatform();
}
