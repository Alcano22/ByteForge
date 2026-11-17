#include "RenderBackend.h"

#include <stdexcept>

namespace ByteForge
{
    RenderBackend* RenderBackend::s_Active = nullptr;

    RenderBackend& RenderBackend::Get()
    {
        if (s_Active == nullptr)
            throw Error("RenderBackend: no graphics context is initialized");

        return *s_Active;
    }
}
