#pragma once

#include "Layers/Layer.h"
#include "SceneManager.h"

class SceneLayer : public Layer
{
public:
	void OnAttach() override { SceneManager::Get().Start(); }
	void Update(const float deltaTime) override { SceneManager::Get().Update(deltaTime); }
	void Render() override { SceneManager::Get().Render(); }
};
