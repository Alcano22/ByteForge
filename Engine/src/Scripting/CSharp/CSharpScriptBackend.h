#pragma once

#include "Engine/Scripting/ScriptBackend.h"
#include "Scripting/CSharp/DotNetHost.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <set>
#include <string>

namespace ByteForge
{
    class CSharpScriptBackend : public ScriptBackend
    {
    public:
        struct ManagedFunctions
        {
            void (CORECLR_DELEGATE_CALLTYPE* GetClassNames)(void (*sink)(const char*, void*), void* userData) = nullptr;
            void* (CORECLR_DELEGATE_CALLTYPE* CreateInstance)(const char* className, uint64_t entity) = nullptr;
            void (CORECLR_DELEGATE_CALLTYPE* DestroyInstance)(void* instance) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* OnCreate)(void* instance) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* OnUpdate)(void* instance, float deltaTime) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* OnDestroy)(void* instance) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* OnContact)(void* instance, int event, uint64_t other) = nullptr;
        };

        explicit CSharpScriptBackend(const std::filesystem::path& scriptCoreDirectory);

        [[nodiscard]] std::string_view GetName() const override { return "C#"; }
        [[nodiscard]] std::vector<std::string> GetClassNames() const override;
        [[nodiscard]] bool HasClass(std::string_view className) const override;

        [[nodiscard]] Scope<ScriptInstance> CreateInstance(std::string_view className, Entity entity) override;

        void OnRuntimeStart(Scene& scene) override;
        void OnRuntimeStop(Scene& scene) override;

    private:
        DotNetHost m_Host;
        ManagedFunctions m_Functions;
        std::set<std::string, std::less<>> m_ClassNames;
    };
}
