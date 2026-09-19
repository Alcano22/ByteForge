#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/ShaderStage.h"

#include <string>

namespace ByteForge
{
    class BYTEFORGE_API Shader
    {
    public:
        virtual ~Shader() = default;

        virtual void SetUniformData(const void* data, uint32_t size) = 0;

        static Ref<Shader> Create(const std::string& vertexSrc, const std::string& fragmentSrc,
                                  const BufferLayout& vertexLayout, uint32_t uniformBufferSize,
                                  uint32_t uniformStageFlags, uint32_t pushConstantStageFlags,
                                  uint32_t pushConstantSize);
    };
}
