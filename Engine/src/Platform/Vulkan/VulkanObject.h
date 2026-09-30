#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Log.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDeletionQueue.h"

#include <utility>

namespace ByteForge
{
    template<typename TBase, typename TVulkan, typename... Args>
    Ref<TBase> MakeVulkanObject(Args&&... args)
    {
        return Ref<TBase>(new TVulkan(std::forward<Args>(args)...), [](TBase* object)
        {
            VulkanContext* context = VulkanContext::TryGet();
            if (context == nullptr)
            {
                CORE_CRITICAL("RHI object released after the graphics context was destroyed, leaking it");
                return;
            }

            context->GetDeletionQueue().Push([object] { delete object; });
        });
    }
}
