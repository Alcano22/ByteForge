#pragma once

class Shader
{
public:
	Shader(const char* vertexSrc, const char* fragmentSource);
	Shader(const char* filepath);

	void Compile();

	void Bind();
	void Unbind();

	const char* GetFilepath() const { return m_Filepath; }
	
	bool IsBound() { return m_Bound; }
private:
	uint32_t CompileShader(const char* src, uint32_t type);
private:
	const char* m_Filepath;
	const char* m_VertexSrc;
	const char* m_FragmentSrc;

	uint32_t m_Program = 0;

	bool m_Bound = false;
};