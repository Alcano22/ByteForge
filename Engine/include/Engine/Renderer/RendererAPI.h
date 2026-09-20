#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Log.h"

#include <functional>

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

    BYTEFORGE_API void DeferRHIDestroy(std::function<void()> destroy);

    template<typename TVulkan, typename TBase, typename... Args>
    Ref<TBase> CreateRHIObject(Args&&... args)
    {
        switch (RendererAPI::GetAPI())
        {
            case RendererAPI::API::None:
                CORE_CRITICAL("RendererAPI::None is not supported");
                return nullptr;
            case RendererAPI::API::Vulkan:
                return Ref<TBase>(new TVulkan(std::forward<Args>(args)...), [](TBase* object)
                {
                    DeferRHIDestroy([object] { delete object; });
                });
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
