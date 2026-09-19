#pragma once

#include "Engine/Core/Core.h"

namespace ByteForge
{
    enum class PresentMode
    {
        Immediate,
        Mailbox,
        Fifo,
        FifoRelaxed
    };

    class BYTEFORGE_API SwapchainSettings
    {
    public:
        static PresentMode GetPreferredPresentMode() { return s_PreferredPresentMode; }
        static void SetPreferredPresentMode(const PresentMode mode) { s_PreferredPresentMode = mode; }

    private:
        static PresentMode s_PreferredPresentMode;
    };
}
