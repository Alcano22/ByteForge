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

namespace ByteForge
{
    namespace
    {
        constexpr const char* BootstrapType  = "ByteForge.Interop.Bootstrap, ScriptCore";
        constexpr const char* ScriptHostType = "ByteForge.Interop.ScriptHost, ScriptCore";

        using InitializeFn = int (CORECLR_DELEGATE_CALLTYPE*)(const NativeAPI* api);

        constexpr size_t FieldValueSize = 16;
        using FieldBuffer = std::array<std::byte, FieldValueSize>;

        static_assert(sizeof(glm::vec4) == FieldValueSize, "glm::vec4 must match System.Numerics.Vector4");

        template<typename T>
        T Load(const void* data)
        {
            T value;
            std::memcpy(&value, data, sizeof(T));
            return value;
        }

        constexpr size_t AssetTypeOffset = sizeof(uint64_t);

        ScriptValue ReadValue(const ScriptFieldType type, const void* data)
        {
            const auto* bytes = static_cast<const std::byte*>(data);

            switch (type)
            {
                case ScriptFieldType::Bool:    return Load<uint8_t>(data) != 0;
                case ScriptFieldType::Int:     return Load<int32_t>(data);
                case ScriptFieldType::Float:   return Load<float>(data);
                case ScriptFieldType::Double:  return Load<double>(data);
                case ScriptFieldType::Vector2: return Load<glm::vec2>(data);
                case ScriptFieldType::Vector3: return Load<glm::vec3>(data);
                case ScriptFieldType::Vector4: return Load<glm::vec4>(data);
                case ScriptFieldType::Entity:  return EntityRef{ UUID(Load<uint64_t>(data)) };
                case ScriptFieldType::Asset:
                    return AssetRef{ static_cast<AssetType>(Load<uint8_t>(bytes + AssetTypeOffset)),
                                     UUID(Load<uint64_t>(data)) };
            }
            throw std::runtime_error("Unknown script field type");
        }

        void WriteValue(const ScriptValue& value, void* data)
        {
            auto* bytes = static_cast<std::byte*>(data);

            std::visit([data, bytes]<typename T>(const T& v)
            {
                if constexpr (std::is_same_v<T, bool>)
                {
                    const uint8_t byte = v ? 1 : 0;
                    std::memcpy(data, &byte, sizeof(byte));
                } else if constexpr (std::is_same_v<T, EntityRef>)
                {
                    const auto id = static_cast<uint64_t>(v.Id);
                    std::memcpy(data, &id, sizeof(id));
                } else if constexpr (std::is_same_v<T, AssetRef>)
                {
                    const auto handle = static_cast<uint64_t>(v.Handle);
                    const auto type = static_cast<uint8_t>(v.Type);
                    std::memcpy(data, &handle, sizeof(handle));
                    std::memcpy(bytes + AssetTypeOffset, &type, sizeof(type));
                } else
                {
                    static_assert(sizeof(T) <= FieldValueSize);
                    std::memcpy(data, &v, sizeof(T));
                }
            }, value);
        }

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
                    .Default = ReadValue(fieldType, value)
                });
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
                alignas(16) FieldBuffer buffer{};
                int type = -1;

                if (m_Functions.GetField(m_Handle, fieldName.c_str(), &type, buffer.data()) != 0 ||
                    !IsValidFieldType(type))
                    return std::nullopt;

                return ReadValue(static_cast<ScriptFieldType>(type), buffer.data());
            }

            bool SetField(const std::string_view name, const ScriptValue& value) override
            {
                const std::string fieldName(name);
                alignas(16) FieldBuffer buffer{};
                WriteValue(value, buffer.data());

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
