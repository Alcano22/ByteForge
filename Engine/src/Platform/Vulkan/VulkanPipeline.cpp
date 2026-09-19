#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <glm/glm.hpp>

#include <array>
#include <stdexcept>
#include <vector>

namespace ByteForge
{
    namespace
    {
        VkFormat ShaderDataTypeToVkFormat(const ShaderDataType type)
        {
            switch (type)
            {
                case ShaderDataType::Float:  return VK_FORMAT_R32_SFLOAT;
                case ShaderDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
                case ShaderDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
                case ShaderDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
                case ShaderDataType::Int:    return VK_FORMAT_R32_SINT;
                case ShaderDataType::Int2:   return VK_FORMAT_R32G32_SINT;
                case ShaderDataType::Int3:   return VK_FORMAT_R32G32B32_SINT;
                case ShaderDataType::Int4:   return VK_FORMAT_R32G32B32A32_SINT;
                case ShaderDataType::Bool:   return VK_FORMAT_R8_UINT;
                case ShaderDataType::Mat3:
                case ShaderDataType::Mat4:
                case ShaderDataType::None:
                    break;
            }

            throw std::runtime_error("Unsupported ShaderDataType for vertex attribute");
        }
    }

    VulkanPipeline::VulkanPipeline(const VulkanDevice& device, const VulkanRenderPass& renderPass,
                                   const VkShaderModule vertexShader, const VkShaderModule fragmentShader,
                                   const BufferLayout& vertexLayout, const VkDescriptorSetLayout descriptorSetLayout,
                                   const VkShaderStageFlags pushConstantStageFlags, const uint32_t pushConstantSize)
        : m_Device(device)
    {
        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{
            VkPipelineShaderStageCreateInfo{
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertexShader,
                .pName  = "main"
            },
            VkPipelineShaderStageCreateInfo{
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragmentShader,
                .pName  = "main"
            }
        };

        const VkVertexInputBindingDescription bindingDescription{
            .binding   = 0,
            .stride    = vertexLayout.GetStride(),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        uint32_t location = 0;
        for (const auto& element : vertexLayout)
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

        constexpr VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
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

        constexpr VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = VK_CULL_MODE_NONE,
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable         = VK_FALSE,
            .lineWidth               = 1.0f
        };

        constexpr VkPipelineMultisampleStateCreateInfo multisampling{
            .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable  = VK_FALSE
        };

        constexpr VkPipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable    = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };

        const VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable   = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments    = &colorBlendAttachment
        };

        const VkPushConstantRange pushConstantRange{
            .stageFlags = pushConstantStageFlags,
            .offset     = 0,
            .size       = pushConstantSize
        };

        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = descriptorSetLayout != nullptr ? 1u : 0u,
            .pSetLayouts            = descriptorSetLayout != nullptr ? &descriptorSetLayout : nullptr,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange
        };

        VK_CHECK(vkCreatePipelineLayout(m_Device.GetHandle(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

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
            .renderPass          = renderPass.GetHandle(),
            .subpass             = 0,
        };

        VK_CHECK(vkCreateGraphicsPipelines(m_Device.GetHandle(), nullptr, 1, &pipelineInfo, nullptr, &m_Pipeline));

        CORE_INFO("Vulkan graphics pipeline created");
    }

    VulkanPipeline::~VulkanPipeline()
    {
        if (m_Pipeline != nullptr)
            vkDestroyPipeline(m_Device.GetHandle(), m_Pipeline, nullptr);

        if (m_PipelineLayout != nullptr)
            vkDestroyPipelineLayout(m_Device.GetHandle(), m_PipelineLayout, nullptr);
    }
}
