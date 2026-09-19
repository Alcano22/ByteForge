#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Layer.h"

#include <vector>

namespace ByteForge
{
    class BYTEFORGE_API LayerStack
    {
    public:
        ~LayerStack();

        void PushLayer(Scope<Layer> layer);
        void PushOverlay(Scope<Layer> overlay);

        [[nodiscard]] auto begin() { return m_Layers.begin(); }
        [[nodiscard]] auto end() { return m_Layers.end(); }
        [[nodiscard]] auto rbegin() { return m_Layers.rbegin(); }
        [[nodiscard]] auto rend() { return m_Layers.rend(); }

    private:
        std::vector<Scope<Layer>> m_Layers;
        size_t m_LayerInsertIndex = 0;
    };
}
