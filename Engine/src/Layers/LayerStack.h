#pragma once

#include "Layer.h"

class LayerStack
{
public:
	LayerStack() = default;
	~LayerStack();

	void PushLayer(Layer* layer);
	void PushOverlay(Layer* overlay);
	void PopLayer(Layer* layer);
	void PopOverlay(Layer* overlay);

	void UpdateLayers(float deltaTime);
	void RenderLayers();

	auto begin() { return m_Layers.begin(); }
	auto end() { return m_Layers.end(); }

	auto begin() const { return m_Layers.begin(); }
	auto end() const { return m_Layers.end(); }
private:
	std::vector<Layer*> m_Layers;
	uint32_t m_LayerInsertIndex = 0;
};
