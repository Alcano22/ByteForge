#pragma once

#include "Engine/Core/Core.h"

#include <string>

namespace ByteForge
{
    class BYTEFORGE_API Shader
    {
    public:
        virtual ~Shader() = default;

        static Ref<Shader> Create(const std::string& vertexSrc, const std::string& fragmentSrc);
    };
}
