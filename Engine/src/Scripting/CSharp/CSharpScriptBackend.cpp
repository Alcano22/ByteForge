#include "Scripting/CSharp/CSharpScriptBackend.h"
#include "Scripting/CSharp/NativeAPI.h"
#include "Scripting/CSharp/ScriptGlue.h"

#include "Engine/Core/Log.h"

#include <format>
#include <stdexcept>
#include <array>
#include <cstddef>
#include <cstring>
#include <set>
#include <type_traits>
#include <variant>
#include <limits>

#include "ManagedValue.h"

namespace ByteForge
{
    namespace
    {
        constexpr const char* BootstrapType  = "ByteForge.Interop.Bootstrap, ScriptCore";
        constexpr const char* ScriptHostType = "ByteForge.Interop.ScriptHost, ScriptCore";

        using InitializeFn = int (CORECLR_DELEGATE_CALLTYPE*)(const NativeAPI* api);

        using FieldBuffer = std::array<std::byte, ManagedValue::SlotSize>;

        bool IsValidFieldType(const int type)
        {
            return type >= 0 && static_cast<size_t>(type) < ScriptFieldTypeCount;
        }

        void CollectClassName(const char* name, void* userData)
        {
            try
            {
                static_cast<std::set<std::string, std::less<>>*>(userData)->emplace(name);
            } catch (...) {}
        }

        void CollectField(const char* name, const int type, const void* value, void* userData)
        {
            if (!IsValidFieldType(type)) return;

            try
            {
                const auto fieldType = static_cast<ScriptFieldType>(type);
                static_cast<std::vector<ScriptFieldInfo>*>(userData)->push_back({
                    .Name    = name,
                    .Type    = fieldType,
                    .Default = ManagedValue::Read(fieldType, value)
                });
            } catch (...) {}
        }

        void ReceiveField(const char*, const int type, const void* value, void* userData)
        {
            if (!IsValidFieldType(type)) return;

            try
            {
                *static_cast<std::optional<ScriptValue>*>(userData) =
                    ManagedValue::Read(static_cast<ScriptFieldType>(type), value);
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

            std::optional<ScriptValue> GetField(const std::string_view name) const override
            {
                const std::string fieldName(name);
                std::optional<ScriptValue> result;

                if (m_Functions.GetField(m_Handle, fieldName.c_str(), &ReceiveField, &result) != 0)
                    return std::nullopt;

                return result;
            }

            bool SetField(const std::string_view name, const ScriptValue& value) override
            {
                const std::string fieldName(name);
                alignas(16) FieldBuffer buffer{};
                ManagedValue::Write(value, buffer.data());

                return m_Functions.SetField(m_Handle, fieldName.c_str(),
                                            static_cast<int>(GetFieldType(value)), buffer.data()) == 0;
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
        resolve(m_Functions.GetClassFields, ScriptHostType, "GetClassFields");
        resolve(m_Functions.GetField, ScriptHostType, "GetField");
        resolve(m_Functions.SetField, ScriptHostType, "SetField");
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

        RefreshClasses();
    }

    void CSharpScriptBackend::RefreshClasses()
    {
        std::set<std::string, std::less<>> names;
        m_Functions.GetClassNames(&CollectClassName, &names);

        m_Classes.clear();
        for (const std::string& name : names)
            m_Functions.GetClassFields(name.c_str(), &CollectField, &m_Classes[name]);
    }

    std::vector<std::string> CSharpScriptBackend::GetClassNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_Classes.size());
        for (const auto& [name, fields] : m_Classes)
            names.push_back(name);
        return names;
    }

    bool CSharpScriptBackend::HasClass(const std::string_view className) const
    {
        return m_Classes.contains(className);
    }

    std::span<const ScriptFieldInfo> CSharpScriptBackend::GetFields(const std::string_view className) const
    {
        const auto it = m_Classes.find(className);
        return it != m_Classes.end() ? std::span<const ScriptFieldInfo>(it->second)
                                     : std::span<const ScriptFieldInfo>{};
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

        RefreshClasses();
        return true;
    }

    void CSharpScriptBackend::OnRuntimeStart(Scene& scene) { ScriptGlue::SetScene(&scene); }
    void CSharpScriptBackend::OnRuntimeStop(Scene&) { ScriptGlue::SetScene(nullptr); }
}
