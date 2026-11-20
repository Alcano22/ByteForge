#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Input/InputControl.h"
#include "Engine/Scene/UUID.h"

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* InputActionsExtension = ".bfinput";

    enum class InputActionType : u8
    {
        Button,
        Axis1D,
        Vector2
    };

    enum class InputBindingKind : u8
    {
        Single,
        Composite1D,
        Composite2D
    };

    struct InputProcessors
    {
        f32 Deadzone = 0.0f;
        f32 Scale = 1.0f;
        bool InvertX = false;
        bool InvertY = false;
        bool Normalize = false;

        bool operator==(const InputProcessors&) const = default;
    };

    struct InputBindingData
    {
        InputBindingKind Kind = InputBindingKind::Single;
        std::vector<InputControl> Controls;
        InputProcessors Processors;

        bool operator==(const InputBindingData&) const = default;
    };

    struct InputActionData
    {
        UUID Id{ 0 };
        std::string Name;
        InputActionType Type = InputActionType::Button;
        std::vector<InputBindingData> Bindings;

        bool operator==(const InputActionData&) const = default;
    };

    struct InputActionMapData
    {
        UUID Id{ 0 };
        std::string Name;
        std::vector<InputActionData> Actions;

        bool operator==(const InputActionMapData&) const = default;
    };

    struct InputActionsData
    {
        std::vector<InputActionMapData> Maps;

        bool operator==(const InputActionsData&) const = default;
    };

    [[nodiscard]] BYTEFORGE_API usize GetControlCount(InputBindingKind kind);

    class BYTEFORGE_API InputActionsAsset
    {
    public:
        InputActionsAsset(UUID handle, InputActionsData data);

        [[nodiscard]] UUID GetHandle() const { return m_Handle; }
        [[nodiscard]] const InputActionsData& GetData() const { return m_Data; }

        [[nodiscard]] const InputActionMapData* FindMap(std::string_view name) const;
        [[nodiscard]] const InputActionData* FindAction(std::string_view path) const;
        [[nodiscard]] const InputActionData* FindAction(UUID id) const;

    private:
        friend class AssetManager;

        UUID m_Handle;
        InputActionsData m_Data;
    };

    [[nodiscard]] BYTEFORGE_API Result<> ValidateInputActions(const InputActionsData& data);

    [[nodiscard]] BYTEFORGE_API Result<InputActionsData> ParseInputActions(const nlohmann::json& data);
    [[nodiscard]] BYTEFORGE_API nlohmann::json SerializeInputActions(const InputActionsData& data);

    [[nodiscard]] BYTEFORGE_API InputActionsData MakeDefaultInputActions();
}
