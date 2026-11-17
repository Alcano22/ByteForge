#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Renderer/RenderBackend.h"
#include "Renderer/ShaderSource.h"

#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace ByteForge
{
    Ref<Shader> Shader::Create(const std::string& vertexSrc, const std::string& fragmentSrc)
    {
        return RenderBackend::Get().CreateShader(vertexSrc, fragmentSrc);
    }

    Ref<Shader> Shader::Load(const fs::path& path, std::vector<std::string> defines)
    {
        if (ShaderLibrary::IsMounted())
        {
            ShaderSource source{ .Path = path, .Defines = std::move(defines) };

            Result<ShaderBinaries> binaries = ShaderLibrary::Find(ToUtf8(path.filename()), source.Defines);
            if (!binaries)
                throw Error("Shader::Load: {}", binaries.error());

            source.Precompiled = std::move(*binaries);
            return RenderBackend::Get().CreateShader(source);
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            throw Error("Shader::Load: cannot open '{}'", ToUtf8(path));

        const ShaderSource source{
            .Path    = path,
            .Code    = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()),
            .Defines = std::move(defines)
        };

        if (file.bad())
            throw Error("Shader::Load: cannot read '{}'", ToUtf8(path));

        return RenderBackend::Get().CreateShader(source);
    }
}
