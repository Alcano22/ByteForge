#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/InputActions.h"
#include "Engine/Input/InputControlSource.h"
#include "Engine/Scene/UUID.h"

#include <glm/vec2.hpp>

#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ByteForge
{
    inline constexpr f32 ButtonPressThreshold = 0.5f;

    struct InputActionState
    {
        glm::vec2 Value{ 0.0f };
        bool Down = false;
        bool Pressed = false;
        bool Released = false;
    };

    [[nodiscard]] BYTEFORGE_API glm::vec2 EvaluateBinding(const InputBindingData& binding,
                                                          const InputControlSource& source);

    [[nodiscard]] BYTEFORGE_API glm::vec2 EvaluateAction(const InputActionData& action,
                                                         const InputControlSource& source);

    class BYTEFORGE_API InputActionSystem
    {
    public:
        [[nodiscard]] const Ref<InputActionsAsset>& GetActions() const { return m_Actions; }
        void SetActions(Ref<InputActionsAsset> actions);

        bool EnableMap(std::string_view name);
        bool DisableMap(std::string_view name);
        [[nodiscard]] bool IsMapEnabled(std::string_view name) const;

        void Update(const InputControlSource& source);

        void Reset();

        [[nodiscard]] std::optional<UUID> FindAction(std::string_view path) const;

        [[nodiscard]] const InputActionState& GetState(UUID action) const;

    private:
        struct TrackedState
        {
            InputActionState State;
            u64 LastUpdate = 0;
        };

        Ref<InputActionsAsset> m_Actions;
        std::unordered_map<UUID, TrackedState> m_States;
        std::unordered_set<UUID> m_DisabledMaps;
        u64 m_UpdateCount = 0;
    };
}
