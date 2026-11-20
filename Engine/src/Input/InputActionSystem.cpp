#include "Engine/Input/InputActionSystem.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <utility>

namespace ByteForge
{
    namespace
    {
        glm::vec2 Read(const InputControl& control, const InputControlSource& source)
        {
            return control.IsSet() ? source.Read(control) : glm::vec2(0.0f);
        }

        f32 ReadPart(const InputControl& control, const InputControlSource& source)
        {
            return std::clamp(glm::length(Read(control, source)), 0.0f, 1.0f);
        }

        glm::vec2 ApplyDeadzone(const glm::vec2 value, const f32 deadzone)
        {
            if (deadzone <= 0.0f)
                return value;

            const f32 length = glm::length(value);
            if (length <= deadzone)
                return glm::vec2(0.0f);

            const f32 remapped = (length - deadzone) / (1.0f - deadzone);
            return value * (remapped / length);
        }

        glm::vec2 ApplyProcessors(glm::vec2 value, const InputProcessors& processors)
        {
            value = ApplyDeadzone(value, processors.Deadzone);

            if (processors.InvertX)
                value.x = -value.x;
            if (processors.InvertY)
                value.y = -value.y;

            if (processors.Normalize)
            {
                const f32 length = glm::length(value);
                if (length > 1.0f)
                    value /= length;
            }

            return value * processors.Scale;
        }

        glm::vec2 ShapeToType(const glm::vec2 value, const InputActionType type)
        {
            switch (type)
            {
                case InputActionType::Button:  return { std::clamp(glm::length(value), 0.0f, 1.0f), 0.0f };
                case InputActionType::Axis1D:  return { value.x, 0.0f };
                case InputActionType::Vector2: return value;
            }
            return glm::vec2(0.0f);
        }

        bool IsDown(const InputActionType type, const glm::vec2 value)
        {
            if (type == InputActionType::Button)
                return value.x >= ButtonPressThreshold;

            return glm::dot(value, value) > 0.0f;
        }
    }

    glm::vec2 EvaluateBinding(const InputBindingData& binding, const InputControlSource& source)
    {
        const std::vector<InputControl>& controls = binding.Controls;
        if (controls.size() != GetControlCount(binding.Kind))
            return glm::vec2(0.0f);

        glm::vec2 value(0.0f);
        switch (binding.Kind)
        {
            case InputBindingKind::Single:
                value = Read(controls[0], source);
                break;

            case InputBindingKind::Composite1D:
                value.x = ReadPart(controls[1], source) - ReadPart(controls[0], source);
                break;

            case InputBindingKind::Composite2D:
                value.x = ReadPart(controls[3], source) - ReadPart(controls[2], source);
                value.y = ReadPart(controls[0], source) - ReadPart(controls[1], source);
                break;
        }

        return ApplyProcessors(value, binding.Processors);
    }

    glm::vec2 EvaluateAction(const InputActionData& action, const InputControlSource& source)
    {
        glm::vec2 strongest(0.0f);
        f32 strongestSquared = 0.0f;

        for (const InputBindingData& binding : action.Bindings)
        {
            const glm::vec2 value = ShapeToType(EvaluateBinding(binding, source), action.Type);
            const f32 squared = glm::dot(value, value);

            if (squared > strongestSquared)
            {
                strongest = value;
                strongestSquared = squared;
            }
        }

        return strongest;
    }

    void InputActionSystem::SetActions(Ref<InputActionsAsset> actions)
    {
        m_Actions = std::move(actions);
        m_DisabledMaps.clear();
        Reset();
    }

    bool InputActionSystem::EnableMap(const std::string_view name)
    {
        const InputActionMapData* map = m_Actions ? m_Actions->FindMap(name) : nullptr;
        if (!map)
            return false;

        m_DisabledMaps.erase(map->Id);
        return true;
    }

    bool InputActionSystem::DisableMap(const std::string_view name)
    {
        const InputActionMapData* map = m_Actions ? m_Actions->FindMap(name) : nullptr;
        if (!map)
            return false;

        m_DisabledMaps.insert(map->Id);
        return true;
    }

    bool InputActionSystem::IsMapEnabled(const std::string_view name) const
    {
        const InputActionMapData* map = m_Actions ? m_Actions->FindMap(name) : nullptr;
        return map && !m_DisabledMaps.contains(map->Id);
    }

    void InputActionSystem::Update(const InputControlSource& source)
    {
        ++m_UpdateCount;

        if (!m_Actions)
        {
            m_States.clear();
            return;
        }

        for (const InputActionMapData& map : m_Actions->GetData().Maps)
        {
            const bool enabled = !m_DisabledMaps.contains(map.Id);

            for (const InputActionData& action : map.Actions)
            {
                TrackedState& tracked = m_States[action.Id];
                const bool wasDown = tracked.State.Down;

                const glm::vec2 value = enabled ? EvaluateAction(action, source) : glm::vec2(0.0f);
                const bool down = IsDown(action.Type, value);

                tracked.State = InputActionState{
                    .Value    = value,
                    .Down     = down,
                    .Pressed  = down && !wasDown,
                    .Released = !down && wasDown
                };
                tracked.LastUpdate = m_UpdateCount;
            }
        }

        std::erase_if(m_States, [this](const auto& entry) { return entry.second.LastUpdate != m_UpdateCount; });
    }

    void InputActionSystem::Reset() { m_States.clear(); }

    std::optional<UUID> InputActionSystem::FindAction(const std::string_view path) const
    {
        const InputActionData* action = m_Actions ? m_Actions->FindAction(path) : nullptr;
        return action ? std::optional<UUID>(action->Id) : std::nullopt;
    }

    const InputActionState& InputActionSystem::GetState(const UUID action) const
    {
        static constexpr InputActionState idle;

        const auto it = m_States.find(action);
        return it != m_States.end() ? it->second.State : idle;
    }
}
