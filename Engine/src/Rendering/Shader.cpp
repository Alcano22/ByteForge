#include "bfpch.h"
#include "Shader.h"

#include "glad/glad.h"

static constexpr int LOG_LEN = 512;

Shader::Shader(const char* vertexSrc, const char* fragmentSrc)
	: m_Filepath("undefined"), m_VertexSrc(vertexSrc), m_FragmentSrc(fragmentSrc)
{
}

Shader::Shader(const char* filepath)
	: m_Filepath(filepath), m_VertexSrc(""), m_FragmentSrc("")
{
}

void Shader::Compile()
{
	const uint32_t vertexShader = CompileShader(m_VertexSrc, GL_VERTEX_SHADER);
	const uint32_t fragmentShader = CompileShader(m_FragmentSrc, GL_FRAGMENT_SHADER);

	m_Program = glCreateProgram();
	glAttachShader(m_Program, vertexShader);
	glAttachShader(m_Program, fragmentShader);
	glLinkProgram(m_Program);

	int success;
	char infoLog[LOG_LEN];
	glGetProgramiv(m_Program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(m_Program, LOG_LEN, nullptr, infoLog);
		BF_LOG_CORE_ERROR(infoLog);
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

uint32_t Shader::CompileShader(const char* src, const uint32_t type)
{
	const uint32_t shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);

	int success;
	char infoLog[LOG_LEN];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(shader, LOG_LEN, nullptr, infoLog);
		BF_LOG_CORE_ERROR(infoLog);
	}

	return shader;
}

void Shader::Bind()
{
	glUseProgram(m_Program);
}

void Shader::Unbind()
{
	glUseProgram(0);
}
