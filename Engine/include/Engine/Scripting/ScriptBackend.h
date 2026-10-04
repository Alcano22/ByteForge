#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Timestep.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scripting/ScriptField.h"

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <optional>
#include <span>

namespace ByteForge
{
    class Scene;

    enum class ContactEvent { SensorEnter, SensorExit, CollisionEnter, CollisionExit };

    class BYTEFORGE_API ScriptInstance
    {
    public:
        virtual ~ScriptInstance() = default;

        virtual void OnCreate() = 0;
        virtual void OnDestroy() = 0;
        virtual void OnUpdate(Timestep ts) = 0;
        virtual void OnContact(ContactEvent event, Entity other) = 0;

        [[nodiscard]] virtual std::optional<ScriptValue> GetField(std::string_view) const { return std::nullopt; }
        virtual bool SetField(std::string_view, const ScriptValue&) { return false; }
    };

    class BYTEFORGE_API ScriptBackend
    {
    public:
        virtual ~ScriptBackend() = default;

        [[nodiscard]] virtual std::string_view GetName() const = 0;
        [[nodiscard]] virtual std::vector<std::string> GetClassNames() const = 0;
        [[nodiscard]] virtual bool HasClass(std::string_view className) const = 0;
        [[nodiscard]] virtual std::span<const ScriptFieldInfo> GetFields(std::string_view) const { return {}; }
        [[nodiscard]] virtual std::string GetDisplayName(const std::string_view className) const
        {
            return std::string(className);
        }

        [[nodiscard]] virtual Scope<ScriptInstance> CreateInstance(std::string_view className, Entity entity) = 0;

        virtual bool LoadModule(const std::filesystem::path&) { return false; }

        virtual void OnRuntimeStart(Scene&) {}
        virtual void OnRuntimeStop(Scene&) {}
    };
}
