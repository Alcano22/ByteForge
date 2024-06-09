#include "bfpch.h"
#include "SceneManager.h"

void SceneManager::Start()
{
	if (m_CurrentScene == nullptr) return;

	m_CurrentScene->Start();
}

void SceneManager::Update(const float deltaTime)
{
	if (m_CurrentScene == nullptr) return;

	m_CurrentScene->Update(deltaTime);
}

void SceneManager::Render()
{
	if (m_CurrentScene == nullptr) return;

	m_CurrentScene->Render();
}
