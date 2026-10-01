#include "Scripting/CSharp/CSharpScriptBackend.h"
#include "Scripting/CSharp/NativeAPI.h"
#include "Scripting/CSharp/ScriptGlue.h"

#include "Engine/Core/Log.h"

#include <format>
#include <stdexcept>

namespace ByteForge
{
    namespace
    {
        constexpr const char* BootstrapType  = "ByteForge.Interop.Bootstrap, ScriptCore";
        constexpr const char* ScriptHostType = "ByteForge.Interop.ScriptHost, ScriptCore";

        using InitializeFn = int (CORECLR_DELEGATE_CALLTYPE*)(const NativeAPI* api);

        void CollectClassName(const char* name, void* userData)
        {
            try
            {
                static_cast<std::set<std::string, std::less<>>*>(userData)->emplace(name);
            } catch (...) {}
        }

        class CSharpScriptInstance final : public ScriptInstance
        {
        public:
            CSharpScriptInstance(const CSharpScriptBackend::ManagedFunctions& functions, void* handle)
                : m_Functions(functions), m_Handle(handle) {}

            ~CSharpScriptInstance() override { m_Functions.DestroyInstance(m_Handle); }

            CSharpScriptInstance(const CSharpScriptInstance&) = delete;
            CSharpScriptInstance& operator=(const CSharpScriptInstance&) = delete;

            void OnCreate() override { Check(m_Functions.OnCreate(m_Handle)); }
            void OnDestroy() override { Check(m_Functions.OnDestroy(m_Handle)); }

            void OnUpdate(const Timestep ts) override { Check(m_Functions.OnUpdate(m_Handle, ts.GetSeconds())); }

            void OnContact(const ContactEvent event, const Entity other) override
            {
                Check(m_Functions.OnContact(m_Handle, static_cast<int>(event),
                                            static_cast<uint64_t>(other.GetUUID())));
            }

        private:
            static void Check(const int result)
            {
                if (result != 0)
                    throw std::runtime_error(ScriptGlue::TakeManagedException());
            }

        private:
            const CSharpScriptBackend::ManagedFunctions& m_Functions;
            void* m_Handle;
        };
    }

    CSharpScriptBackend::CSharpScriptBackend(const std::filesystem::path& scriptCoreDirectory)
        : m_Host(scriptCoreDirectory / "ScriptCore.runtimeconfig.json")
    {
        const std::filesystem::path assembly = scriptCoreDirectory / "ScriptCore.dll";

        const auto resolve = [&]<typename Fn>(Fn& target, const char* type, const char* method)
        {
            target = m_Host.GetFunction<Fn>(assembly, type, method);
        };

        InitializeFn initialize = nullptr;
        resolve(initialize, BootstrapType, "Initialize");

        resolve(m_Functions.GetClassNames, ScriptHostType, "GetClassNames");
        resolve(m_Functions.CreateInstance, ScriptHostType, "CreateInstance");
        resolve(m_Functions.DestroyInstance, ScriptHostType, "DestroyInstance");
        resolve(m_Functions.LoadGameAssembly, ScriptHostType, "LoadGameAssembly");
        resolve(m_Functions.OnCreate, ScriptHostType, "OnCreate");
        resolve(m_Functions.OnUpdate, ScriptHostType, "OnUpdate");
        resolve(m_Functions.OnDestroy, ScriptHostType, "OnDestroy");
        resolve(m_Functions.OnContact, ScriptHostType, "OnContact");

        const NativeAPI api = ScriptGlue::CreateNativeAPI();
        if (const int result = initialize(&api); result != 0)
            throw std::runtime_error(std::format("ScriptCore initialization failed (code {})", result));

        m_Functions.GetClassNames(&CollectClassName, &m_ClassNames);
    }

    std::vector<std::string> CSharpScriptBackend::GetClassNames() const
    {
        return { m_ClassNames.begin(), m_ClassNames.end() };
    }

    bool CSharpScriptBackend::HasClass(const std::string_view className) const
    {
        return m_ClassNames.contains(className);
    }

    Scope<ScriptInstance> CSharpScriptBackend::CreateInstance(const std::string_view className, const Entity entity)
    {
        const std::string name(className);

        void* handle = m_Functions.CreateInstance(name.c_str(), static_cast<uint64_t>(entity.GetUUID()));
        if (handle == nullptr)
            throw std::runtime_error(ScriptGlue::TakeManagedException());

        return MakeScope<CSharpScriptInstance>(m_Functions, handle);
    }

    bool CSharpScriptBackend::LoadModule(const std::filesystem::path& path)
    {
        const std::u8string file = path.u8string();

        if (m_Functions.LoadGameAssembly(reinterpret_cast<const char*>(file.c_str())) != 0)
        {
            CORE_ERROR("Could not load C# assembly '{}': {}", path.string(), ScriptGlue::TakeManagedException());
            return false;
        }

        m_ClassNames.clear();
        m_Functions.GetClassNames(&CollectClassName, &m_ClassNames);
        return true;
    }

    void CSharpScriptBackend::OnRuntimeStart(Scene& scene) { ScriptGlue::SetScene(&scene); }
    void CSharpScriptBackend::OnRuntimeStop(Scene& scene) { ScriptGlue::SetScene(nullptr); }
}
