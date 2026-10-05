#include "Engine/Renderer/Shader.h"
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

    Ref<Shader> Shader::Load(const std::filesystem::path& path, std::vector<std::string> defines)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error(std::format("Shader::Load: cannot open '{}'", path.string()));

        const ShaderSource source{
            .Path    = path,
            .Code    = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()),
            .Defines = std::move(defines)
        };

        if (file.bad())
            throw std::runtime_error(std::format("Shader::Load: cannot read '{}'", path.string()));

        return RenderBackend::Get().CreateShader(source);
    }
}
