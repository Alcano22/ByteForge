#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/AnimationClip.h"
#include "Engine/Scene/UUID.h"

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* AnimatorControllerExtension = ".bfanimator";

    struct AnimatorState
    {
        std::string Name;
        UUID Clip{ 0 };

        f32 Speed = 1.0f;
    };

    struct AnimatorControllerData
    {
        std::vector<AnimatorState> States;

        u32 DefaultState = 0;
    };

    class BYTEFORGE_API AnimatorControllerAsset
    {
    public:
        AnimatorControllerAsset(UUID handle, AnimatorControllerData data);

        [[nodiscard]] UUID GetHandle() const { return m_Handle; }
        [[nodiscard]] const AnimatorControllerData& GetData() const { return m_Data; }

        [[nodiscard]] u32 GetStateCount() const { return static_cast<u32>(m_Data.States.size()); }
        [[nodiscard]] std::optional<u32> FindState(std::string_view name) const;

        [[nodiscard]] std::optional<u32> GetDefaultState() const;

        [[nodiscard]] const AnimatorState& GetState(const u32 index) const { return m_Data.States.at(index); }

        [[nodiscard]] const Ref<AnimationClipAsset>& GetClip(u32 index) const;

    private:
        void SetData(AnimatorControllerData data);

    private:
        friend class AssetManager;

        UUID m_Handle;
        AnimatorControllerData m_Data;
        std::vector<Ref<AnimationClipAsset>> m_Clips;
    };

    [[nodiscard]] BYTEFORGE_API Result<AnimatorControllerData> ParseAnimatorController(const nlohmann::json& data);
    [[nodiscard]] BYTEFORGE_API nlohmann::json SerializeAnimatorController(const AnimatorControllerData& controller);
}
