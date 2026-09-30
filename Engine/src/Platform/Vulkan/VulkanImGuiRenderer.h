#pragma once

#include "Engine/ImGui/ImGuiRenderer.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <unordered_map>

namespace ByteForge
{
    class VulkanImGuiRenderer : public ImGuiRenderer
    {
    public:
        void Init(GLFWwindow* windowHandle) override;
        void Shutdown() override;

        void NewFrame() override;
        void RenderDrawData() override;

        [[nodiscard]] ImTextureID GetTextureId(const Ref<Texture2D>& texture) override;
        [[nodiscard]] ImTextureID GetTextureId(const Ref<RenderTarget>& target) override;

    private:
        struct CachedTexture
        {
            std::weak_ptr<const void> Owner;
            VkDescriptorSet Set = nullptr;
        };

        [[nodiscard]] ImTextureID Resolve(const std::shared_ptr<const void>& owner, VkImageView view);
        void ReleaseExpired();

        static void Release(VkDescriptorSet set);

    private:
        VkFormat m_ColorFormat = VK_FORMAT_UNDEFINED;
        std::unordered_map<const void*, CachedTexture> m_Textures;
    };
}
