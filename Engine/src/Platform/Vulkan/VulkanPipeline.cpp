#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanShaderProgram.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"
#include "Engine/Renderer/CameraUniforms.h"

#include <array>
#include <format>
#include <stdexcept>
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

        VkPipelineColorBlendAttachmentState ToVkBlendAttachment(const BlendMode blendMode)
        {
            constexpr VkColorComponentFlags writeMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                                      | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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

        void ValidateShaderResources(const ShaderReflection& reflection)
        {
            for (const ReflectedDescriptorBinding& binding : reflection.Bindings)
            {
                const bool isCameraBinding = binding.Set == 0 && binding.Binding == 0
                                          && binding.Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                if (!isCameraBinding)
                {
                    throw std::runtime_error(std::format(
                        "Unsupported shader resource '{}' (set {}, binding {}): set 0, binding 0 is reserved for "
                        "the camera uniform buffer, other resources are not supported yet",
                        binding.Name, binding.Set, binding.Binding));
                }

                if (binding.Size > sizeof(CameraUniforms))
                {
                    throw std::runtime_error(std::format(
                        "Uniform buffer '{}' (set 0, binding 0) is {} bytes, but the camera data has only {} bytes "
                        "(see CameraUniforms.h)", binding.Name, binding.Size, sizeof(CameraUniforms)));
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

        const VkDescriptorSetLayout frameSetLayout = VulkanContext::Get().GetFrameData().GetLayoutHandle();

        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = 1,
            .pSetLayouts            = &frameSetLayout,
            .pushConstantRangeCount = hasPushConstants ? 1u : 0u,
            .pPushConstantRanges    = hasPushConstants ? &pushConstantRange : nullptr
        };

        VK_CHECK(vkCreatePipelineLayout(m_Device.GetHandle(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout));
    }

    void VulkanPipeline::CreatePipeline(const PipelineSpec& spec, const VulkanShaderProgram& shader)
    {
        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{
            VkPipelineShaderStageCreateInfo{
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                .module = shader.GetVertexModule(),
                .pName  = "main"
            },
            VkPipelineShaderStageCreateInfo{
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = shader.GetFragmentModule(),
                .pName  = "main"
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

        const VkPipelineColorBlendAttachmentState colorBlendAttachment = ToVkBlendAttachment(spec.Blend);

        const VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable   = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments    = &colorBlendAttachment
        };

        const VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount          = static_cast<uint32_t>(shaderStages.size()),
            .pStages             = shaderStages.data(),
            .pVertexInputState   = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState   = &multisampling,
            .pColorBlendState    = &colorBlending,
            .pDynamicState       = &dynamicState,
            .layout              = m_PipelineLayout,
            .renderPass          = VulkanContext::Get().GetRenderPass().GetHandle(),
            .subpass             = 0,
        };

        VK_CHECK(vkCreateGraphicsPipelines(m_Device.GetHandle(), nullptr, 1, &pipelineInfo, nullptr, &m_Pipeline));
    }
}
