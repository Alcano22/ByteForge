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
}
