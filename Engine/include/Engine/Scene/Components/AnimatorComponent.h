#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/UUID.h"

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace ByteForge
{
    class AnimatorControllerAsset;
    class AnimationClipAsset;

    struct BYTEFORGE_API AnimatorComponent
    {
        UUID Controller{ 0 };
        f32 Speed = 1.0f;
        bool PlayOnStart = true;

        AnimatorComponent() = default;

        AnimatorComponent(const AnimatorComponent& other)
            : Controller(other.Controller), Speed(other.Speed), PlayOnStart(other.PlayOnStart) {}

        AnimatorComponent& operator=(const AnimatorComponent& other)
        {
            Controller = other.Controller;
            Speed = other.Speed;
            PlayOnStart = other.PlayOnStart;

            m_Controller.reset();
            m_LoadedController = UUID(0);
            m_State.reset();
            m_Time = 0.0f;
            m_Playing = false;
            return *this;
        }

        AnimatorComponent(AnimatorComponent&&) noexcept = default;
        AnimatorComponent& operator=(AnimatorComponent&&) noexcept = default;

        bool Play(std::string_view state, bool restart = false);
        void Stop();

        [[nodiscard]] bool IsPlaying() const { return m_Playing; }

        [[nodiscard]] std::string GetCurrentState() const;

        [[nodiscard]] f32 GetNormalizedTime() const;

        static void Write(const AnimatorComponent& component, nlohmann::json& json);
        static void Read(AnimatorComponent& component, const nlohmann::json& json);

    private:
        void ResolveController();
        void EnterDefaultState();
        [[nodiscard]] const Ref<AnimationClipAsset>& GetCurrentClip() const;

    private:
        friend class AnimationSystem;

        Ref<AnimatorControllerAsset> m_Controller;
        UUID m_LoadedController{ 0 };
        std::optional<u32> m_State;
        f32 m_Time = 0.0f;
        bool m_Playing = false;
    };
}
