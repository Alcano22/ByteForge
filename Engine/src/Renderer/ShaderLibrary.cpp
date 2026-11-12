#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Assets/Pak.h"
#include "Engine/Core/Log.h"
#include "Renderer/BuiltinShaders.h"
#include "Renderer/RenderBackend.h"
#include "Renderer/ShaderSource.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <format>

namespace ByteForge
{
    namespace
    {
        constexpr u32 SpirvMagic = 0x07230203;

        Scope<PakArchive> s_Archive;

        std::string MakeEntryPath(const std::string_view file, const std::span<const std::string> defines,
                                  const ShaderStage stage)
        {
            std::vector<std::string> sorted(defines.begin(), defines.end());
            std::ranges::sort(sorted);

            std::string variant;
            for (const std::string& define : sorted)
            {
                if (!variant.empty())
                    variant += '+';
                variant += define;
            }

            return std::format("shaders/{}/{}.{}.spv",
                               file, variant.empty() ? "default" : variant, stage == ShaderStage::Vertex ? "vs" : "ps");
        }

        Result<std::vector<u32>> ToWords(const std::span<const std::byte> bytes, const std::string& path)
        {
            if (bytes.size() < sizeof(u32) || bytes.size() % sizeof(u32) != 0)
                return std::unexpected(std::format("'{}' is not valid SPIR-V", path));

            std::vector<u32> words(bytes.size() / sizeof(u32));
            std::memcpy(words.data(), bytes.data(), bytes.size());

            if (words.front() != SpirvMagic)
                return std::unexpected(std::format("'{}' is not valid SPIR-V (wrong magic number)", path));

            return words;
        }

        Result<> AddStage(PakWriter& writer, const ShaderVariant& variant,
                          const ShaderStage stage, const std::vector<u32>& words)
        {
            return writer.Add(MakeEntryPath(variant.File, variant.Defines, stage), std::as_bytes(std::span(words)));
        }
    }

    std::span<const ShaderVariant> ShaderLibrary::GetBuiltinVariants()
    {
        static const std::array<ShaderVariant, 4> variants{{
            { .File = "Renderer2D.hlsl", .Defines = {}              },
            { .File = "Renderer2D.hlsl", .Defines = { "ENTITY_ID" } },
            { .File = "Line.hlsl",       .Defines = {}              },
            { .File = "Mesh.hlsl",       .Defines = {}              }
        }};
        return variants;
    }

    Result<usize> ShaderLibrary::Precompile(PakWriter& writer)
    {
        usize stages = 0;
        for (const ShaderVariant& variant : GetBuiltinVariants())
        {
            const fs::path path = GetBuiltinShaderPath(variant.File);

            const Result<std::vector<std::byte>> code = ReadBinaryFile(path);
            if (!code)
                return std::unexpected(code.error());

            const ShaderSource source{
                .Path    = path,
                .Code    = std::string(reinterpret_cast<const char*>(code->data()), code->size()),
                .Defines = variant.Defines
            };

            ShaderBinaries binaries;
            try
            {
                binaries = RenderBackend::Get().CompileShader(source);
            } catch (const std::exception& e)
            {
                return std::unexpected(std::string(e.what()));
            }

            if (const auto added = AddStage(writer, variant, ShaderStage::Vertex, binaries.Vertex); !added)
                return std::unexpected(added.error());
            if (const auto added = AddStage(writer, variant, ShaderStage::Fragment, binaries.Fragment); !added)
                return std::unexpected(added.error());

            stages += 2;
        }
        return stages;
    }

    Result<> ShaderLibrary::Mount(const fs::path& archive)
    {
        Result<Scope<PakArchive>> opened = PakArchive::Open(archive);
        if (!opened)
            return std::unexpected(opened.error());

        s_Archive = std::move(*opened);
        CORE_INFO("Using {} precompiled shader stage(s) from '{}'",
                  s_Archive->GetEntries().size(), archive.generic_string());
        return {};
    }

    void ShaderLibrary::Unmount() { s_Archive.reset(); }

    bool ShaderLibrary::IsMounted() { return s_Archive != nullptr; }

    Result<ShaderBinaries> ShaderLibrary::Find(const std::string_view file, const std::span<const std::string> defines)
    {
        if (!s_Archive)
            return std::unexpected("no precompiled shaders are mounted");

        ShaderBinaries binaries;
        for (const ShaderStage stage : { ShaderStage::Vertex, ShaderStage::Fragment })
        {
            const std::string path = MakeEntryPath(file, defines, stage);

            const Result<std::vector<std::byte>> bytes = s_Archive->Read(path);
            if (!bytes)
                return std::unexpected(std::format("shader variant '{}' was not precompiled, add it to "
                                                   "ShaderLibrary::GetBuiltinVariants ({})", path, bytes.error()));

            Result<std::vector<u32>> words = ToWords(*bytes, path);
            if (!words)
                return std::unexpected(words.error());

            (stage == ShaderStage::Vertex ? binaries.Vertex : binaries.Fragment) = std::move(*words);
        }
        return binaries;
    }
}
