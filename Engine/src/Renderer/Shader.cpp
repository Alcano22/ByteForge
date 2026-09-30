#include "Engine/Renderer/Shader.h"
#include "Renderer/RenderBackend.h"

namespace ByteForge
{
    Ref<Shader> Shader::Create(const std::string& vertexSrc, const std::string& fragmentSrc)
    {
        return RenderBackend::Get().CreateShader(vertexSrc, fragmentSrc);
    }
}
