#pragma once

#include "Engine/Core/Core.h"

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
}
