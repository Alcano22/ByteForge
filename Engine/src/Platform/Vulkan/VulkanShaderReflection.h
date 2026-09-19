#pragma once

#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/ShaderStage.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ByteForge
{
    struct ReflectedDescriptorBinding
    {
        std::string Name;
        uint32_t Set = 0;
        uint32_t Binding = 0;
        VkDescriptorType Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        VkShaderStageFlags Stages = 0;
        uint32_t Size = 0;
    };

    struct ReflectedVertexInput
    {
        std::string Name;
        uint32_t Location = 0;
        VkFormat Format = VK_FORMAT_UNDEFINED;
    };

    struct ReflectedPushConstants
    {
        VkShaderStageFlags Stages = 0;
        uint32_t Size = 0;
    };

    struct ShaderReflection
    {
        std::vector<ReflectedDescriptorBinding> Bindings;
        std::vector<ReflectedVertexInput> VertexInputs;
        ReflectedPushConstants PushConstants;

        [[nodiscard]] static ShaderReflection Reflect(std::span<const uint32_t> spirv, ShaderStage stage);

        [[nodiscard]] static ShaderReflection Merge(const ShaderReflection& vertex,
                                                    const ShaderReflection& fragment);
    };

    void ValidateVertexLayout(const ShaderReflection& reflection, const BufferLayout& layout);
}
