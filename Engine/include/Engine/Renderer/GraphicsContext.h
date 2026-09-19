#pragma once

#include "Engine/Core/Core.h"

#include <memory>

struct GLFWwindow;

namespace ByteForge
{
    class BYTEFORGE_API GraphicsContext
    {
    public:
        virtual ~GraphicsContext() = default;

        virtual void Init() = 0;

        static Scope<GraphicsContext> Create(GLFWwindow* windowHandle);
    };
}
