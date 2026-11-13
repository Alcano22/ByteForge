#include "Engine/Assets/AnimatorController.h"
#include "Engine/Assets/AssetManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace ByteForge
{
    AnimatorControllerAsset::AnimatorControllerAsset(const UUID handle, AnimatorControllerData data)
        : m_Handle(handle)
    {
        SetData(std::move(data));
    }

    std::optional<u32> AnimatorControllerAsset::FindState(const std::string_view name) const
    {
        for (usize i = 0; i < m_Data.States.size(); ++i)
        {
            if (m_Data.States[i].Name == name)
                return static_cast<u32>(i);
        }

        return std::nullopt;
    }

    std::optional<u32> AnimatorControllerAsset::GetDefaultState() const
    {
        if (m_Data.States.empty())
            return std::nullopt;

        return m_Data.DefaultState < GetStateCount() ? m_Data.DefaultState : 0u;
    }

    const Ref<AnimationClipAsset>& AnimatorControllerAsset::GetClip(const u32 index) const
    {
        static const Ref<AnimationClipAsset> none;
        return index < m_Clips.size() ? m_Clips[index] : none;
    }

    void AnimatorControllerAsset::SetData(AnimatorControllerData data)
    {
        m_Data = std::move(data);

        m_Clips.clear();
        m_Clips.reserve(m_Data.States.size());

        for (const AnimatorState& state : m_Data.States)
        {
            m_Clips.push_back(static_cast<u64>(state.Clip) != 0 ? AssetManager::LoadAnimationClip(state.Clip)
                                                                : Ref<AnimationClipAsset>{});
        }
    }

    Result<AnimatorControllerData> ParseAnimatorController(const nlohmann::json& data)
    {
        if (!data.is_object())
            return std::unexpected("expected a JSON object");

        try
        {
            AnimatorControllerData controller;
            controller.DefaultState = data.value("defaultState", controller.DefaultState);

            if (const auto it = data.find("states"); it != data.end())
            {
                if (!it->is_array())
                    return std::unexpected("'states' must be an array");

                controller.States.reserve(it->size());
                for (const nlohmann::json& stateJson : *it)
                {
                    if (!stateJson.is_object())
                        return std::unexpected("every state must be a JSON object");

                    AnimatorState state;
                    state.Name  = stateJson.value("name", std::string{});
                    state.Clip  = UUID(stateJson.value("clip", u64{ 0 }));
                    state.Speed = std::max(0.0f, stateJson.value("speed", state.Speed));
                    controller.States.push_back(std::move(state));
                }
            }

            return controller;
        } catch (const nlohmann::json::exception& e)
        {
            return std::unexpected(e.what());
        }
    }

    nlohmann::json SerializeAnimatorController(const AnimatorControllerData& controller)
    {
        nlohmann::json states = nlohmann::json::array();
        for (const AnimatorState& state : controller.States)
        {
            states.push_back(nlohmann::json{
                { "name",  state.Name                   },
                { "clip",  static_cast<u64>(state.Clip) },
                { "speed", state.Speed                  }
            });
        }

        return nlohmann::json{
            { "defaultState", controller.DefaultState },
            { "states",       std::move(states)       }
        };
    }
}
