#pragma once

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
}

#define VK_CHECK(expr) \
    do \
    { \
        const VkResult vkCheckResult_ = (expr); \
        if (vkCheckResult_ != VK_SUCCESS) \
            throw ::ByteForge::VulkanException(vkCheckResult_, #expr, __FILE__, __LINE__); \
    } while (0)
