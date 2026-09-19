#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/CameraUniforms.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace ByteForge
{
    class VulkanDevice;
    class VulkanDescriptorSetLayout;
    class VulkanDescriptorPool;
    class VulkanUniformBuffer;

    class VulkanFrameData : NonCopyable
    {
    public:
        explicit VulkanFrameData(const VulkanDevice& device);
        ~VulkanFrameData();

        void BeginFrame();

        void BeginScene(const CameraUniforms& uniforms);

        [[nodiscard]] VkDescriptorSetLayout GetLayoutHandle() const;
        [[nodiscard]] VkDescriptorSet GetSet(uint32_t frameIndex) const;
        [[nodiscard]] uint32_t GetDynamicOffset() const { return m_SceneIndex * m_SliceStride; }
        [[nodiscard]] uint64_t GetFrameNumber() const { return m_FrameNumber; }

    private:
        void WriteCurrentScene(const CameraUniforms& uniforms) const;

    public:
        static constexpr uint32_t MaxScenesPerFrame = 16;

    private:
        Scope<VulkanDescriptorSetLayout> m_Layout;
        Scope<VulkanUniformBuffer> m_Buffer;
        Scope<VulkanDescriptorPool> m_Pool;

        uint32_t m_SliceStride = 0;
        uint64_t m_FrameNumber = 0;
        uint32_t m_SceneIndex = 0;
    };
}
