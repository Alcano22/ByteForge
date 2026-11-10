#pragma once

#include "Engine/Core/Core.h"

#include <cstdint>

namespace ByteForge
{
    class BYTEFORGE_API UniformBuffer
    {
    public:
        virtual ~UniformBuffer() = default;

        virtual void SetData(const void* data, u32 size, u32 offset = 0) = 0;

        static Ref<UniformBuffer> Create(u32 size);
    };
}
