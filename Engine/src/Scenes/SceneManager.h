#pragma once

#include "Scene.h"
#include "Core/Base.h"

class SceneManager
{
public:
	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;
	
	static SceneManager& Get()
	{
		static SceneManager instance;
		return instance;
	}

	void Start();
	void Update(float deltaTime);
	void Render();

	Ref<Scene> GetCurrentScene() const { return m_CurrentScene; }
	void SetCurrentScene(const Ref<Scene>& scene)
	{
		scene->Start();
		m_CurrentScene = scene;
	}
private:
	SceneManager() = default;
	~SceneManager() = default;
private:
	Ref<Scene> m_CurrentScene = nullptr;
};
