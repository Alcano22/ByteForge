#pragma once

#include "Engine/Core/Core.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ByteForge
{
    class BYTEFORGE_API Shader
    {
    public:
        virtual ~Shader() = default;

        static Ref<Shader> Create(const std::string& vertexSrc, const std::string& fragmentSrc);

        [[nodiscard]] static Ref<Shader> Load(const fs::path& path,
                                              std::vector<std::string> defines = {});
    };
}
