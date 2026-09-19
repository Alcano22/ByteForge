#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/ShaderStage.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ByteForge
{
    class VulkanShaderCompiler
    {
    public:
        static VulkanShaderCompiler& Get();

        VulkanShaderCompiler(const VulkanShaderCompiler&) = delete;
        VulkanShaderCompiler& operator=(const VulkanShaderCompiler&) = delete;
        VulkanShaderCompiler(VulkanShaderCompiler&&) = delete;
        VulkanShaderCompiler& operator=(VulkanShaderCompiler&&) = delete;

        [[nodiscard]] std::vector<uint32_t> Compile(const std::string& source, ShaderStage stage,
                                                    const std::string& entryPoint = "main") const;

    private:
        VulkanShaderCompiler();
        ~VulkanShaderCompiler();

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
