#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/SceneUniforms.h"

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

        void BeginScene(const SceneUniforms& uniforms);

        [[nodiscard]] VkDescriptorSetLayout GetLayoutHandle() const;
        [[nodiscard]] VkDescriptorSet GetSet(u32 frameIndex) const;
        [[nodiscard]] u32 GetDynamicOffset() const { return m_SceneIndex * m_SliceStride; }
        [[nodiscard]] u64 GetFrameNumber() const { return m_FrameNumber; }

    private:
        void WriteCurrentScene(const SceneUniforms& uniforms) const;

    public:
        static constexpr u32 MaxScenesPerFrame = 128;

    private:
        Scope<VulkanDescriptorSetLayout> m_Layout;
        Scope<VulkanUniformBuffer> m_Buffer;
        Scope<VulkanDescriptorPool> m_Pool;

        u32 m_SliceStride = 0;
        u64 m_FrameNumber = 0;
        u32 m_SceneIndex = 0;
    };
}
