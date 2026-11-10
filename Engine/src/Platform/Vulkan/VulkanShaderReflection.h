#pragma once

#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/ShaderStage.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    struct ReflectedBlockMember
    {
        std::string Name;
        u32 Offset = 0;
        u32 Size = 0;
    };

    struct ReflectedDescriptorBinding
    {
        std::string Name;
        u32 Set = 0;
        u32 Binding = 0;
        VkDescriptorType Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        VkShaderStageFlags Stages = 0;
        u32 Size = 0;
        std::vector<ReflectedBlockMember> Members;
    };

    struct ReflectedVertexInput
    {
        std::string Name;
        u32 Location = 0;
        VkFormat Format = VK_FORMAT_UNDEFINED;
    };

    struct ReflectedPushConstants
    {
        VkShaderStageFlags Stages = 0;
        u32 Size = 0;
    };

    struct ShaderReflection
    {
        std::vector<ReflectedDescriptorBinding> Bindings;
        std::vector<ReflectedVertexInput> VertexInputs;
        ReflectedPushConstants PushConstants;

        [[nodiscard]] static ShaderReflection Reflect(std::span<const u32> spirv, ShaderStage stage);

        [[nodiscard]] static ShaderReflection Merge(const ShaderReflection& vertex,
                                                    const ShaderReflection& fragment);
    };

    inline constexpr std::string_view SamplerNameSuffix = "Sampler";

    [[nodiscard]] inline std::string_view TextureNameOfSampler(const std::string_view samplerName)
    {
        if (samplerName.size() <= SamplerNameSuffix.size() || !samplerName.ends_with(SamplerNameSuffix))
            return {};

        return samplerName.substr(0, samplerName.size() - SamplerNameSuffix.size());
    }

    void ValidateVertexLayout(const ShaderReflection& reflection, const BufferLayout& layout);
}
