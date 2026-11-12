#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/ShaderStage.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    class PakWriter;

    struct ShaderBinaries
    {
        std::vector<u32> Vertex;
        std::vector<u32> Fragment;

        [[nodiscard]] bool IsEmpty() const { return Vertex.empty() || Fragment.empty(); }
    };

    struct ShaderVariant
    {
        std::string File;
        std::vector<std::string> Defines;
    };

    class BYTEFORGE_API ShaderLibrary
    {
    public:
        static constexpr const char* ArchiveName = "engine.bfpak";

        [[nodiscard]] static std::span<const ShaderVariant> GetBuiltinVariants();

        [[nodiscard]] static Result<usize> Precompile(PakWriter& writer);

        [[nodiscard]] static Result<> Mount(const fs::path& archive);
        static void Unmount();
        [[nodiscard]] static bool IsMounted();

        [[nodiscard]] static Result<ShaderBinaries> Find(std::string_view file, std::span<const std::string> defines);
    };
}
