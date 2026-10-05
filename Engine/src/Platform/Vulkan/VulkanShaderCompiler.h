#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/ShaderStage.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ByteForge
{
    struct ShaderSource;

    class VulkanShaderCompiler
    {
    public:
        static VulkanShaderCompiler& Get();

        VulkanShaderCompiler(const VulkanShaderCompiler&) = delete;
        VulkanShaderCompiler& operator=(const VulkanShaderCompiler&) = delete;
        VulkanShaderCompiler(VulkanShaderCompiler&&) = delete;
        VulkanShaderCompiler& operator=(VulkanShaderCompiler&&) = delete;

        [[nodiscard]] std::vector<uint32_t> Compile(const ShaderSource& source, ShaderStage stage,
                                                    const std::string& entryPoint) const;

    private:
        VulkanShaderCompiler();
        ~VulkanShaderCompiler();

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
