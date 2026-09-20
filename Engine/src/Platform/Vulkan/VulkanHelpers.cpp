#include "Platform/Vulkan/VulkanHelpers.h"

#include <format>

namespace ByteForge
{
    std::string VulkanException::BuildMessage(const VkResult result, const char* expression,
                                              const char* file, const int line)
    {
        return std::format("Vulkan error {} ({}) in '{}' at {}: {}",
                           VkResultToString(result),
                           std::to_string(static_cast<int>(result)),
                           expression, file, line);
    }

    const char* VkResultToString(const VkResult result)
    {
        switch (result)
        {
            case VK_SUCCESS:                     return "VK_SUCCESS";
            case VK_NOT_READY:                   return "VK_NOT_READY";
            case VK_TIMEOUT:                     return "VK_TIMEOUT";
            case VK_ERROR_OUT_OF_HOST_MEMORY:    return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:  return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST:           return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_LAYER_NOT_PRESENT:     return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT:   return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER:   return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS:      return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED:  return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_SURFACE_LOST_KHR:      return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR:       return "VK_ERROR_OUT_OF_DATE_KHR";
            default:                             return "UNKNOWN_VK_RESULT";
        }
    }

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
            case ShaderDataType::None:   break;
        }

        throw std::runtime_error("Unsupported ShaderDataType for vertex attribute");
    }

    const char* VkFormatToString(const VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_UNDEFINED:      return "none";
            case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
            case VK_FORMAT_B8G8R8A8_SRGB:  return "B8G8R8A8_SRGB";
            case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
            case VK_FORMAT_R8G8B8A8_SRGB:  return "R8G8B8A8_SRGB";
            case VK_FORMAT_D32_SFLOAT:     return "D32_SFLOAT";
            default:                       return "another format";
        }
    }

    VkFormat ImageFormatToVk(const ImageFormat format)
    {
        switch (format)
        {
            case ImageFormat::None:        return VK_FORMAT_UNDEFINED;
            case ImageFormat::RGBA8_SRGB:  return VK_FORMAT_R8G8B8A8_SRGB;
            case ImageFormat::RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
            case ImageFormat::Depth32F:    return VK_FORMAT_D32_SFLOAT;
            case ImageFormat::Swapchain:   break;
        }

        throw std::runtime_error("ImageFormat::Swapchain is only valid for PipelineSpec::ColorFormat");
    }

    VkFormat ToUnormEquivalent(const VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UNORM;
            case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UNORM;
            default:                      return format;
        }
    }

    void CmdImageBarrier(const VkCommandBuffer commandBuffer, const VkImage image,
                         const VkImageLayout oldLayout, const VkImageLayout newLayout,
                         const VkPipelineStageFlags2 srcStage, const VkAccessFlags2 srcAccess,
                         const VkPipelineStageFlags2 dstStage, const VkAccessFlags2 dstAccess,
                         const VkImageAspectFlags aspect, const uint32_t baseMipLevel,
                         const uint32_t levelCount)
    {
        const VkImageMemoryBarrier2 barrier{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask        = srcStage,
            .srcAccessMask       = srcAccess,
            .dstStageMask        = dstStage,
            .dstAccessMask       = dstAccess,
            .oldLayout           = oldLayout,
            .newLayout           = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = image,
            .subresourceRange    = {
                .aspectMask   = aspect,
                .baseMipLevel = baseMipLevel,
                .levelCount   = levelCount,
                .layerCount   = 1
            }
        };

        const VkDependencyInfo dependencyInfo{
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &barrier
        };

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }
}
