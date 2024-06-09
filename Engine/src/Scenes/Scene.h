#pragma once
#include "Rendering/Shader.h"

class Scene
{
public:
	void Start();
	void Update(float deltaTime);
	void Render();
private:
	Ref<Shader> m_Shader;

	uint32_t m_VAO;
};
