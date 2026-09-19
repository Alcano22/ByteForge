#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    class BYTEFORGE_API RendererAPI
    {
    public:
        enum class API
        {
            None = 0,
            Vulkan
        };

        static API GetAPI() { return s_API; }
        static void SetAPI(const API api) { s_API = api; }

    private:
        static API s_API;
    };

    template<typename TVulkan, typename TBase, typename... Args>
    Ref<TBase> CreateRHIObject(Args&&... args)
    {
        switch (RendererAPI::GetAPI())
        {
            case RendererAPI::API::None:
                CORE_CRITICAL("RendererAPI::None is not supported");
                return nullptr;
            case RendererAPI::API::Vulkan:
                return MakeRef<TVulkan>(std::forward<Args>(args)...);
        }

        CORE_CRITICAL("Unknown RendererAPI");
        return nullptr;
    }

    template<typename Fn>
    void DispatchRHICall(Fn&& vulkanCall)
    {
        switch (RendererAPI::GetAPI())
        {
            case RendererAPI::API::None:
                CORE_CRITICAL("RendererAPI::None is not supported");
                return;
            case RendererAPI::API::Vulkan:
                vulkanCall();
                return;
        }

        CORE_CRITICAL("Unknown RendererAPI");
    }
}
