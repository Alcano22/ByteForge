#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanShaderProgram.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanDescriptorSetLayout.h"
#include "Platform/Vulkan/VulkanDescriptorAllocator.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"
#include "Engine/Renderer/SceneUniforms.h"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace ByteForge
{
    namespace
    {
        VkPrimitiveTopology ToVk(const PrimitiveTopology topology)
        {
            switch (topology)
            {
                case PrimitiveTopology::TriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                case PrimitiveTopology::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            }

            throw std::runtime_error("Unknown PrimitiveTopology");
        }

        VkCullModeFlags ToVk(const CullMode cullMode)
        {
            switch (cullMode)
            {
                case CullMode::None:  return VK_CULL_MODE_NONE;
                case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
                case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
            }

            throw std::runtime_error("Unknown CullMode");
        }

        VkFrontFace ToVk(const FrontFace frontFace)
        {
            switch (frontFace)
            {
                case FrontFace::Clockwise:        return VK_FRONT_FACE_CLOCKWISE;
                case FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
            }

            throw std::runtime_error("Unknown FrontFace");
        }

        VkCompareOp ToVk(const CompareOp compareOp)
        {
            switch (compareOp)
            {
                case CompareOp::Less:        return VK_COMPARE_OP_LESS;
                case CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
                case CompareOp::Always:      return VK_COMPARE_OP_ALWAYS;
            }

            throw std::runtime_error("Unknown CompareOp");
        }

        VkPipelineColorBlendAttachmentState ToVkBlendAttachment(const BlendMode blendMode, const bool writeEnabled)
        {
            const VkColorComponentFlags writeMask = writeEnabled
                                                  ? VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                                  | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
                                                  : 0;

            switch (blendMode)
            {
                case BlendMode::None:
                    return { .blendEnable = VK_FALSE, .colorWriteMask = writeMask };

                case BlendMode::Alpha:
                    return {
                        .blendEnable         = VK_TRUE,
                        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                        .colorBlendOp        = VK_BLEND_OP_ADD,
                        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                        .alphaBlendOp        = VK_BLEND_OP_ADD,
                        .colorWriteMask      = writeMask
                    };
            }

            throw std::runtime_error("Unknown BlendMode");
        }

        const char* DescriptorTypeName(const VkDescriptorType type)
        {
            switch (type)
            {
                case VK_DESCRIPTOR_TYPE_SAMPLER:                return "sampler";
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "combined image sampler";
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          return "sampled image";
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:          return "storage image";
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return "uniform buffer";
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return "storage buffer";
                default:                                        return "descriptor";
            }
        }

        void ValidateShaderResources(const ShaderReflection& reflection)
        {
            uint32_t uniformBuffers = 0;
            uint32_t sampledImages = 0;
            uint32_t samplers = 0;

            for (const ReflectedDescriptorBinding& binding : reflection.Bindings)
            {
                if (binding.Set == 0)
                {
                    if (binding.Binding == 0 && binding.Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    {
                        if (binding.Size > sizeof(SceneUniforms))
                        {
                            throw std::runtime_error(std::format(
                                "Uniform buffer '{}' (set 0, binding 0) is {} bytes, but the camera data has only {} "
                                "bytes (see CameraUniforms.h)", binding.Name, binding.Size, sizeof(SceneUniforms)));
                        }
                        continue;
                    }

                    throw std::runtime_error(std::format(
                        "Unsupported shader resource '{}' ({}, set 0, binding {}): set 0 only holds the scene "
                        "uniform buffer at binding 0", binding.Name, DescriptorTypeName(binding.Type), binding.Binding));
                }

                if (binding.Set == 1)
                {
                    switch (binding.Type)
                    {
                        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: ++uniformBuffers; break;
                        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:  ++sampledImages;  break;
                        case VK_DESCRIPTOR_TYPE_SAMPLER:        ++samplers;       break;
                        default:
                            throw std::runtime_error(std::format(
                                "Unsupported shader resource '{}' ({}, set 1, binding {}): materials support one "
                                "uniform buffer, Texture2D and SamplerState resources",
                                binding.Name, DescriptorTypeName(binding.Type), binding.Binding));
                    }
                    continue;
                }

                throw std::runtime_error(std::format(
                    "Unsupported shader resource '{}' (set {}, binding {}): only set 0 (camera) and set 1 (material) "
                    "are supported", binding.Name, binding.Set, binding.Binding));
            }

            if (uniformBuffers > 1)
                throw std::runtime_error("A material can have at most one uniform buffer (its parameters) in set 1");

            if (sampledImages > VulkanDescriptorAllocator::MaxSampledImagesPerSet ||
                samplers > VulkanDescriptorAllocator::MaxSamplersPerSet)
            {
                throw std::runtime_error(std::format(
                    "A material supports at most {} textures and {} samplers, but the shader declares {} and {}",
                    VulkanDescriptorAllocator::MaxSampledImagesPerSet, VulkanDescriptorAllocator::MaxSamplersPerSet,
                    sampledImages, samplers));
            }

            for (const ReflectedDescriptorBinding& sampler : reflection.Bindings)
            {
                if (sampler.Set != 1 || sampler.Type != VK_DESCRIPTOR_TYPE_SAMPLER) continue;

                const std::string_view textureName = TextureNameOfSampler(sampler.Name);
                const bool hasTexture = !textureName.empty() &&
                    std::ranges::any_of(reflection.Bindings, [&](const ReflectedDescriptorBinding& other)
                    {
                        return other.Set == 1 && other.Type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                            && other.Name == textureName;
                    });

                if (!hasTexture)
                {
                    throw std::runtime_error(std::format(
                        "Sampler '{}' (set 1, binding {}) does not belong to a texture: name a sampler "
                        "after its texture plus the suffix '{}', e.g. 'u_AlbedoSampler' for 'u_Albedo'",
                        sampler.Name, sampler.Binding, SamplerNameSuffix));
                }
            }
        }
    }

    VulkanPipeline::VulkanPipeline(const PipelineSpec& spec)
        : m_Device(VulkanContext::Get().GetDevice())
    {
        if (!spec.Shader)
            throw std::runtime_error("PipelineSpec::Shader must not be null");

        const auto& shader = static_cast<const VulkanShaderProgram&>(*spec.Shader);
        const ShaderReflection& reflection = shader.GetReflection();
        ValidateVertexLayout(reflection, spec.VertexLayout);

        ValidateShaderResources(reflection);
        CreateMaterialSetLayout(reflection);
        CreatePipelineLayout(reflection.PushConstants);
        CreatePipeline(spec, shader);

        CORE_INFO("Vulkan graphics pipeline created");
    }

    VulkanPipeline::~VulkanPipeline()
    {
        if (m_Pipeline != nullptr)
            vkDestroyPipeline(m_Device.GetHandle(), m_Pipeline, nullptr);

        if (m_PipelineLayout != nullptr)
            vkDestroyPipelineLayout(m_Device.GetHandle(), m_PipelineLayout, nullptr);
    }

    VkDescriptorSetLayout VulkanPipeline::GetMaterialSetLayout() const
    {
        return m_MaterialSetLayout ? m_MaterialSetLayout->GetHandle() : nullptr;
    }

    void VulkanPipeline::CreateMaterialSetLayout(const ShaderReflection& reflection)
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings;

        for (const ReflectedDescriptorBinding& binding : reflection.Bindings)
        {
            if (binding.Set != 1) continue;

            layoutBindings.push_back({
                .binding         = binding.Binding,
                .descriptorType  = binding.Type,
                .descriptorCount = 1,
                .stageFlags      = binding.Stages
            });
            m_MaterialBindings.push_back(binding);
        }

        if (layoutBindings.empty()) return;

        m_MaterialSetLayout = MakeScope<VulkanDescriptorSetLayout>(m_Device, layoutBindings);
    }

    void VulkanPipeline::CreatePipelineLayout(const ReflectedPushConstants& pushConstants)
    {
        if ((pushConstants.Size > 0) != (pushConstants.Stages != 0))
            throw std::runtime_error("Push constant size and stage flags must be specified together");

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(m_Device.GetPhysicalDevice(), &deviceProperties);
        if (pushConstants.Size > deviceProperties.limits.maxPushConstantsSize)
        {
            throw std::runtime_error(std::format("Push constant block is {} bytes, but the device supports only {}",
                                                 pushConstants.Size, deviceProperties.limits.maxPushConstantsSize));
        }

        const bool hasPushConstants = pushConstants.Size > 0;
        m_PushConstantStages = pushConstants.Stages;
        m_PushConstantSize = pushConstants.Size;

        const VkPushConstantRange pushConstantRange{
            .stageFlags = pushConstants.Stages,
            .offset     = 0,
            .size       = pushConstants.Size
        };

        const std::array<VkDescriptorSetLayout, 2> setLayouts{
            VulkanContext::Get().GetFrameData().GetLayoutHandle(),
            GetMaterialSetLayout()
        };

        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = m_MaterialSetLayout ? 2u : 1u,
            .pSetLayouts            = setLayouts.data(),
            .pushConstantRangeCount = hasPushConstants ? 1u : 0u,
            .pPushConstantRanges    = hasPushConstants ? &pushConstantRange : nullptr
        };

        VK_CHECK(vkCreatePipelineLayout(m_Device.GetHandle(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout));
    }

    void VulkanPipeline::CreatePipeline(const PipelineSpec& spec, const VulkanShaderProgram& shader)
    {
        if (spec.ColorAttachments.empty())
        {
            throw std::runtime_error("PipelineSpec::ColorAttachments must not be empty "
                                     "(depth-only pipelines are not supported yet)");
        }

        m_ColorFormats.clear();
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
        blendAttachments.reserve(spec.ColorAttachments.size());

        for (const ColorAttachment& attachment : spec.ColorAttachments)
        {
            const VkFormat format = attachment.Format == ImageFormat::Swapchain
                                  ? VulkanContext::Get().GetSwapchain().GetImageFormat()
                                  : ImageFormatToVk(attachment.Format);

            if (format == VK_FORMAT_UNDEFINED)
                throw std::runtime_error("PipelineSpec: color attachment formats must not be None");

            if (IsIntegerFormat(format) && attachment.Blend != BlendMode::None)
                throw std::runtime_error("PipelineSpec: integer color attachments cannot be blended");

            m_ColorFormats.push_back(format);
            blendAttachments.push_back(ToVkBlendAttachment(attachment.Blend, attachment.WriteEnabled));
        }

        m_DepthFormat = ImageFormatToVk(spec.DepthFormat);

        if ((spec.DepthTest || spec.DepthWrite) && m_DepthFormat == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("PipelineSpec: depth test or depth write requires a DepthFormat");

        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{
            VkPipelineShaderStageCreateInfo{
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                .module = shader.GetVertexModule(),
                .pName  = shader.GetVertexEntryPoint().c_str()
            },
            VkPipelineShaderStageCreateInfo{
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = shader.GetFragmentModule(),
                .pName  = shader.GetFragmentEntryPoint().c_str()
            }
        };

        const VkVertexInputBindingDescription bindingDescription{
            .binding   = 0,
            .stride    = spec.VertexLayout.GetStride(),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        uint32_t location = 0;
        for (const auto& element : spec.VertexLayout)
        {
            attributeDescriptions.push_back({
                .location = location++,
                .binding  = 0,
                .format   = ShaderDataTypeToVkFormat(element.Type),
                .offset   = static_cast<uint32_t>(element.Offset)
            });
        }

        const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions    = attributeDescriptions.data()
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology               = ToVk(spec.Topology),
            .primitiveRestartEnable = VK_FALSE
        };

        constexpr std::array<VkDynamicState, 2> dynamicStates{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        const VkPipelineDynamicStateCreateInfo dynamicState{
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates    = dynamicStates.data()
        };

        constexpr VkPipelineViewportStateCreateInfo viewportState{
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount  = 1
        };

        const VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = ToVk(spec.Cull),
            .frontFace               = ToVk(spec.Front),
            .depthBiasEnable         = VK_FALSE,
            .lineWidth               = 1.0f
        };

        constexpr VkPipelineMultisampleStateCreateInfo multisampling{
            .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable  = VK_FALSE
        };

        const VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable   = VK_FALSE,
            .attachmentCount = static_cast<uint32_t>(blendAttachments.size()),
            .pAttachments    = blendAttachments.data()
        };

        const VkPipelineDepthStencilStateCreateInfo depthStencil{
            .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable  = spec.DepthTest ? VK_TRUE : VK_FALSE,
            .depthWriteEnable = spec.DepthWrite ? VK_TRUE : VK_FALSE,
            .depthCompareOp   = ToVk(spec.DepthCompare)
        };

        const VkPipelineRenderingCreateInfo renderingInfo{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount    = static_cast<uint32_t>(m_ColorFormats.size()),
            .pColorAttachmentFormats = m_ColorFormats.data(),
            .depthAttachmentFormat   = m_DepthFormat
        };

        const VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = &renderingInfo,
            .stageCount          = static_cast<uint32_t>(shaderStages.size()),
            .pStages             = shaderStages.data(),
            .pVertexInputState   = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState   = &multisampling,
            .pDepthStencilState  = m_DepthFormat != VK_FORMAT_UNDEFINED ? &depthStencil : nullptr,
            .pColorBlendState    = &colorBlending,
            .pDynamicState       = &dynamicState,
            .layout              = m_PipelineLayout
        };

        VK_CHECK(vkCreateGraphicsPipelines(m_Device.GetHandle(), nullptr, 1, &pipelineInfo, nullptr, &m_Pipeline));
    }
}
