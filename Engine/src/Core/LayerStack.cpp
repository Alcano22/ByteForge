#include "Engine/Core/LayerStack.h"

namespace ByteForge
{
    LayerStack::~LayerStack()
    {
        for (auto& layer : m_Layers)
            layer->OnDetach();
    }

    void LayerStack::PushLayer(Scope<Layer> layer)
    {
        layer->OnAttach();
        m_Layers.insert(m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), std::move(layer));
        ++m_LayerInsertIndex;
    }

    void LayerStack::PushOverlay(Scope<Layer> overlay)
    {
        overlay->OnAttach();
        m_Layers.emplace_back(std::move(overlay));
    }
}
