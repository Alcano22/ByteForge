#include "Engine/Scripting/ScriptEngine.h"
#include "Engine/Core/Platform.h"
#include "Engine/Core/Log.h"
#include "Scripting/NativeScriptBackend.h"
#include "Scripting/CSharp/CSharpScriptBackend.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>
#include <filesystem>

namespace ByteForge
{
    ScriptEngine* ScriptEngine::s_Instance = nullptr;

    ScriptEngine::ScriptEngine()
    {
        if (s_Instance != nullptr)
            throw std::runtime_error("ScriptEngine: only one instance may exist");

        s_Instance = this;
        RegisterBackend(MakeScope<NativeScriptBackend>());
        TryRegisterCSharpBackend();
    }

    ScriptEngine::~ScriptEngine()
    {
        m_Backends.clear();
        s_Instance = nullptr;
    }

    ScriptEngine& ScriptEngine::Get()
    {
        if (s_Instance == nullptr)
            throw std::runtime_error("ScriptEngine: no instance exists, it is created by the Application");

        return *s_Instance;
    }

    void ScriptEngine::RegisterBackend(Scope<ScriptBackend> backend)
    {
        if (!backend)
            throw std::runtime_error("ScriptEngine::RegisterBackend: the backend must not be null");

        CORE_INFO("Script backend '{}' registered", backend->GetName());
        m_Backends.push_back(std::move(backend));
    }

    ScriptBackend* ScriptEngine::FindBackend(const std::string_view className) const
    {
        for (const auto& backend : m_Backends)
        {
            if (backend->HasClass(className))
                return backend.get();
        }
        return nullptr;
    }

    ScriptBackend* ScriptEngine::FindBackendByName(const std::string_view name) const
    {
        for (const auto& backend : m_Backends)
        {
            if (backend->GetName() == name)
                return backend.get();
        }
        return nullptr;
    }

    std::vector<ScriptClassInfo> ScriptEngine::GetClasses() const
    {
        std::vector<ScriptClassInfo> classes;
        for (const auto& backend : m_Backends)
        {
            for (std::string& name : backend->GetClassNames())
                classes.push_back({ .Name = std::move(name), .Backend = std::string(backend->GetName()) });
        }

        std::ranges::sort(classes, {}, &ScriptClassInfo::Name);
        return classes;
    }

    std::span<const ScriptFieldInfo> ScriptEngine::GetFields(const std::string_view className) const
    {
        const ScriptBackend* backend = FindBackend(className);
        return backend != nullptr ? backend->GetFields(className) : std::span<const ScriptFieldInfo>{};
    }

    void ScriptEngine::OnRuntimeStart(Scene& scene) const
    {
        for (const auto& backend : m_Backends)
        {
            try
            {
                backend->OnRuntimeStart(scene);
            } catch (const std::exception& e)
            {
                CORE_ERROR("Script backend '{}' failed to start: {}", backend->GetName(), e.what());
            }
        }
    }

    void ScriptEngine::OnRuntimeStop(Scene& scene) const
    {
        for (const auto& backend : m_Backends)
        {
            try
            {
                backend->OnRuntimeStop(scene);
            } catch (const std::exception& e)
            {
                CORE_ERROR("Script backend '{}' failed to stop: {}", backend->GetName(), e.what());
            }
        }
    }

    void ScriptEngine::TryRegisterCSharpBackend()
    {
        const std::filesystem::path directory = Platform::GetExecutableDirectory() / "ScriptCore";

        std::error_code error;
        if (!std::filesystem::is_regular_file(directory / "ScriptCore.dll", error))
        {
            CORE_INFO("C# scripting unavailable: no ScriptCore next to the executable");
            return;
        }

        try
        {
            RegisterBackend(MakeScope<CSharpScriptBackend>(directory));
        } catch (const std::exception& e)
        {
            CORE_ERROR("C# scripting unavailable: {}", e.what());
        }
    }
}
