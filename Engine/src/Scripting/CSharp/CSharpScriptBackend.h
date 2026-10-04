#pragma once

#include "Engine/Scripting/ScriptBackend.h"
#include "Scripting/CSharp/DotNetHost.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <map>
#include <span>
#include <vector>

namespace ByteForge
{
    class CSharpScriptBackend : public ScriptBackend
    {
    public:
        struct ManagedFunctions
        {
            using FieldSink = void (*)(const char* name, int type, const void* value, void* userData);

            void (CORECLR_DELEGATE_CALLTYPE* GetClassNames)(void (*sink)(const char*, void*), void* userData) = nullptr;
            void (CORECLR_DELEGATE_CALLTYPE* GetClassFields)(const char* className, FieldSink sink, void* userData) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* GetField)(void* instance, const char* name, FieldSink sink, void* userData) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* SetField)(void* instance, const char* name, int type, const void* value) = nullptr;
            void* (CORECLR_DELEGATE_CALLTYPE* CreateInstance)(const char* className, uint64_t entity) = nullptr;
            void (CORECLR_DELEGATE_CALLTYPE* DestroyInstance)(void* instance) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* LoadGameAssembly)(const char* path) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* OnCreate)(void* instance) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* OnUpdate)(void* instance, float deltaTime) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* OnDestroy)(void* instance) = nullptr;
            int (CORECLR_DELEGATE_CALLTYPE* OnContact)(void* instance, int event, uint64_t other) = nullptr;
        };

        explicit CSharpScriptBackend(const std::filesystem::path& scriptCoreDirectory);

        [[nodiscard]] std::string_view GetName() const override { return "C#"; }
        [[nodiscard]] std::vector<std::string> GetClassNames() const override;
        [[nodiscard]] bool HasClass(std::string_view className) const override;
        [[nodiscard]] std::span<const ScriptFieldInfo> GetFields(std::string_view) const override;

        [[nodiscard]] Scope<ScriptInstance> CreateInstance(std::string_view className, Entity entity) override;

        bool LoadModule(const std::filesystem::path& path) override;

        void OnRuntimeStart(Scene& scene) override;
        void OnRuntimeStop(Scene& scene) override;

    private:
        void RefreshClasses();

    private:
        DotNetHost m_Host;
        ManagedFunctions m_Functions;
        std::map<std::string, std::vector<ScriptFieldInfo>, std::less<>> m_Classes;
    };
}
