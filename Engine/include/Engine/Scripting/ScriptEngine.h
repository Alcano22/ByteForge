#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Scripting/ScriptBackend.h"

#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    class Scene;

    struct ScriptClassInfo
    {
        std::string Name;
        std::string Backend;
    };

    class BYTEFORGE_API ScriptEngine : NonCopyable
    {
    public:
        ScriptEngine();
        ~ScriptEngine();

        void RegisterBackend(Scope<ScriptBackend> backend);

        [[nodiscard]] ScriptBackend* FindBackend(std::string_view className) const;
        [[nodiscard]] ScriptBackend* FindBackendByName(std::string_view name) const;

        [[nodiscard]] std::vector<ScriptClassInfo> GetClasses() const;
        [[nodiscard]] std::span<const ScriptFieldInfo> GetFields(std::string_view className) const;

        void OnRuntimeStart(Scene& scene) const;
        void OnRuntimeStop(Scene& scene) const;

        [[nodiscard]] static ScriptEngine& Get();

    private:
        void TryRegisterCSharpBackend();

    private:
        std::vector<Scope<ScriptBackend>> m_Backends;

        static ScriptEngine* s_Instance;
    };
}
