#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanDescriptorSetLayout.h"
#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"

#include <format>
#include <span>
#include <stdexcept>

namespace ByteForge
{
    VulkanFrameData::VulkanFrameData(const VulkanDevice& device)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device.GetPhysicalDevice(), &props);

        const uint32_t alignment = static_cast<uint32_t>(props.limits.minUniformBufferOffsetAlignment);
        m_SliceStride = (sizeof(CameraUniforms) + alignment - 1) / alignment * alignment;

        const VkDescriptorSetLayoutBinding binding{
            .binding         = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        };

        m_Layout = MakeScope<VulkanDescriptorSetLayout>(device, std::span(&binding, 1));
        m_Buffer = MakeScope<VulkanUniformBuffer>(m_SliceStride * (MaxScenesPerFrame + 1));
        m_Pool = MakeScope<VulkanDescriptorPool>(device, m_Layout->GetHandle(), *m_Buffer,
                                                 VulkanContext::GetFramesInFlight(),
                                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, sizeof(CameraUniforms));
    }

    VulkanFrameData::~VulkanFrameData() = default;

    void VulkanFrameData::BeginFrame()
    {
        ++m_FrameNumber;
        m_SceneIndex = 0;
        WriteCurrentScene(CameraUniforms{});
    }

    void VulkanFrameData::BeginScene(const CameraUniforms& uniforms)
    {
        if (m_SceneIndex == MaxScenesPerFrame)
            throw std::runtime_error(std::format("Too many scenes in one frame (maximum is {})", MaxScenesPerFrame));

        ++m_SceneIndex;
        WriteCurrentScene(uniforms);
    }

    VkDescriptorSetLayout VulkanFrameData::GetLayoutHandle() const { return m_Layout->GetHandle(); }
    VkDescriptorSet VulkanFrameData::GetSet(const uint32_t frameIndex) const { return m_Pool->GetSet(frameIndex); }

    void VulkanFrameData::WriteCurrentScene(const CameraUniforms& uniforms) const
    {
        m_Buffer->SetData(&uniforms, sizeof(uniforms), GetDynamicOffset());
    }
}
