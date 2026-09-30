#include "Scripting/NativeScriptBackend.h"
#include "Scripting/NativeScriptRegistry.h"

#include "Engine/Scene/ScriptableEntity.h"

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace ByteForge
{
    class NativeScriptInstance final : public ScriptInstance
    {
    public:
        NativeScriptInstance(Scope<ScriptableEntity> script, const Entity entity)
            : m_Script(std::move(script))
        {
            m_Script->m_Entity = entity;
        }

        void OnCreate() override { m_Script->OnCreate(); }
        void OnDestroy() override { m_Script->OnDestroy(); }

        void OnUpdate(const Timestep ts) override { m_Script->OnUpdate(ts); }

        void OnContact(const ContactEvent event, const Entity other) override
        {
            switch (event)
            {
                case ContactEvent::SensorEnter:    m_Script->OnSensorEnter(other);    break;
                case ContactEvent::SensorExit:     m_Script->OnSensorExit(other);     break;
                case ContactEvent::CollisionEnter: m_Script->OnCollisionEnter(other); break;
                case ContactEvent::CollisionExit:  m_Script->OnCollisionExit(other);  break;
            }
        }

    private:
        Scope<ScriptableEntity> m_Script;
    };

    std::vector<std::string> NativeScriptBackend::GetClassNames() const
    {
        std::vector<std::string> names;
        for (const auto& [name, factory] : GetNativeScriptFactories())
            names.push_back(name);
        return names;
    }

    bool NativeScriptBackend::HasClass(const std::string_view className) const
    {
        return GetNativeScriptFactories().contains(className);
    }

    Scope<ScriptInstance> NativeScriptBackend::CreateInstance(const std::string_view className, const Entity entity)
    {
        const NativeScriptFactories& factories = GetNativeScriptFactories();
        const auto it = factories.find(className);
        if (it == factories.end())
            throw std::runtime_error(std::format("Native script '{}' is not registered", className));

        Scope<ScriptableEntity> script = it->second();
        if (!script)
            throw std::runtime_error(std::format("The factory of native script '{}' returned null", className));

        return MakeScope<NativeScriptInstance>(std::move(script), entity);
    }
}
