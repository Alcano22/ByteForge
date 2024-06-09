#include "bfpch.h"
#include "Scene.h"

#include "glad/glad.h"

void Scene::Start()
{
	constexpr float vertices[] = {
		-0.5f, -0.5f,
		 0.5f, -0.5f,
		 0.0f,  0.5f
	};
	
	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);
	
	uint32_t vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
	glEnableVertexAttribArray(0);
	
	const char* vertexSrc = "#version 330 core\n"
		"layout (location = 0) in vec2 a_Pos;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = vec4(a_Pos.x, a_Pos.y, 0.0f, 1.0f);\n"
		"}\0";
	
	const char* fragmentSrc = "#version 330 core\n"
		"out vec4 FragColor;\n"
		"void main()\n"
		"{\n"
		"	FragColor = vec4(0.8f, 0.2f, 0.2f, 1.0f);\n"
		"}\0";
	
	m_Shader = CreateRef<Shader>(vertexSrc, fragmentSrc);
	m_Shader->Compile();
}

void Scene::Update(float deltaTime)
{
}

void Scene::Render()
{
	m_Shader->Bind();
		
	glBindVertexArray(m_VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);
		
	m_Shader->Unbind();
}
