#pragma once

#include <Engine/Core/Core.h>

#include <glm/glm.hpp>

namespace ByteForge
{
    class Texture2D;
    class UIRenderer;

    class SplashScreen
    {
    public:
        SplashScreen();

        void Update(f32 seconds);
        void Render(UIRenderer& renderer, glm::vec2 screenSize) const;

        [[nodiscard]] bool IsShowingLogo() const { return m_Phase == Phase::Hold; }

        void SetGameLoaded() { m_GameLoaded = true; }

        [[nodiscard]] bool IsDone() const { return m_Phase == Phase::Done; }

    private:
        enum class Phase { WaitingForFont, FadeIn, Hold, FadeOut, Done };

        void Enter(Phase phase);

        [[nodiscard]] f32 GetOpacity() const;

    private:
        Ref<Texture2D> m_Logo;

        Phase m_Phase = Phase::WaitingForFont;
        f32 m_Time = 0.0f;
        bool m_GameLoaded = false;
    };
}
