#pragma once

#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/ImageFormat.h"

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <string>

namespace ByteForge
{
    class VulkanException : public std::runtime_error
    {
    public:
        VulkanException(const VkResult result, const char* expression, const char* file, const int line)
            : std::runtime_error(BuildMessage(result, expression, file, line)),
              m_Result(result) {}

        [[nodiscard]] VkResult GetResult() const { return m_Result; }

    private:
        static std::string BuildMessage(VkResult result, const char* expression, const char* file, int line);

    private:
        VkResult m_Result;
    };

    const char* VkResultToString(VkResult result);

    VkFormat ShaderDataTypeToVkFormat(ShaderDataType type);

    const char* VkFormatToString(VkFormat format);
    VkFormat ImageFormatToVk(ImageFormat format);
    VkFormat ToUnormEquivalent(VkFormat format);
    bool IsSrgbFormat(VkFormat format);
    bool IsIntegerFormat(VkFormat format);

    void CmdImageBarrier(VkCommandBuffer commandBuffer, VkImage image,
                         VkImageLayout oldLayout, VkImageLayout newLayout,
                         VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                         VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
                         VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                         uint32_t baseMipLevel = 0, uint32_t levelCount = 1);
}

#define VK_CHECK(expr) \
    do \
    { \
        const VkResult vkCheckResult_ = (expr); \
        if (vkCheckResult_ != VK_SUCCESS) \
            throw ::ByteForge::VulkanException(vkCheckResult_, #expr, __FILE__, __LINE__); \
    } while (0)
