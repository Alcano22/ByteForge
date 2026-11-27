#include "SplashScreen.h"
#include "EmbeddedFiles.h"

#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/FontAsset.h>
#include <Engine/Core/Log.h>
#include <Engine/Renderer/ImageDecoder.h>
#include <Engine/Renderer/Texture2D.h>
#include <Engine/Renderer/UIRenderer.h>
#include <Engine/Text/Font.h>
#include <Engine/Text/TextLayout.h>

#include <algorithm>
#include <string_view>

namespace ByteForge
{
    namespace
    {
        constexpr f32 FadeSeconds = 0.6f;
        constexpr f32 HoldSeconds = 1.6f;

        constexpr f32 MaxFontWaitSeconds = 10.0f;

        constexpr f32 LogoShare       = 0.28f;
        constexpr f32 LogoCenterY     = 0.56f;
        constexpr f32 CaptionShare    = 0.13f;
        constexpr f32 CaptionGapShare = 0.12f;

        constexpr glm::vec4 Background{ 0.0f, 0.0f, 0.0f, 1.0f };
        constexpr glm::vec3 CaptionColor{ 0.85f, 0.85f, 0.85f };
        constexpr std::u32string_view Caption = U"Built with ByteForge";

        const UUID BuiltInFont{ 0 };

        Ref<Font> GetBuiltInFont() { return AssetManager::LoadFont(BuiltInFont)->GetFont(); }

        bool IsBuiltInFontSettled()
        {
            const Ref<FontAsset> asset = AssetManager::LoadFont(BuiltInFont);
            return asset->GetFont() || asset->GetState() == AssetLoadState::Failed;
        }

        f32 SmoothStep(const f32 t)
        {
            const f32 x = std::clamp(t, 0.0f, 1.0f);
            return x * x * (3.0f - 2.0f * x);
        }

        void DrawCaption(UIRenderer& renderer, const glm::vec2 screenSize, const f32 top,
                         const f32 fontSize, const f32 opacity)
        {
            const Ref<Font> font = GetBuiltInFont();
            if (!font) return;

            const TextLayout layout = LayoutText(font->GetGlyphs(), Caption, {
                .FontSize   = fontSize,
                .BoxSize    = { screenSize.x, fontSize * 2.0f },
                .Horizontal = HorizontalAlignment::Center,
                .Vertical   = VerticalAlignment::Top
            });

            const glm::vec2 origin{ 0.0f, top };

            for (const PlacedGlyph& glyph : layout.Glyphs)
            {
                renderer.DrawGlyph({
                    .Min           = origin + glyph.Min,
                    .Max           = origin + glyph.Max,
                    .BottomShift   = glyph.BottomShift,
                    .TopShift      = glyph.TopShift,
                    .UVMin         = glyph.UVMin,
                    .UVMax         = glyph.UVMax,
                    .DistanceRange = font->GetDistanceRange(),
                    .Color         = glm::vec4(CaptionColor, opacity)
                }, font->GetAtlas());
            }
        }
    }

    SplashScreen::SplashScreen()
    {
        Result<DecodedImage> image = DecodeImage(Embedded::SplashLogo(), "splash logo");
        if (!image)
        {
            APP_WARN("Cannot decide the splash logo: {}", image.error());
            return;
        }

        m_Logo = Texture2D::Create(image->Width, image->Height, image->Pixels, { .Wrap = TextureWrap::ClampToEdge });

        static_cast<void>(GetBuiltInFont());
    }

    void SplashScreen::Update(const f32 seconds)
    {
        m_Time += seconds;

        switch (m_Phase)
        {
            case Phase::WaitingForFont:
                if (IsBuiltInFontSettled() || m_Time >= MaxFontWaitSeconds)
                    Enter(Phase::FadeIn);
                break;

            case Phase::FadeIn:
                if (m_Time >= FadeSeconds)
                    Enter(Phase::Hold);
                break;

            case Phase::Hold:
                if (m_GameLoaded && m_Time >= HoldSeconds)
                    Enter(Phase::FadeOut);
                break;

            case Phase::FadeOut:
                if (m_Time >= FadeSeconds)
                    Enter(Phase::Done);
                break;

            case Phase::Done:
                break;
        }
    }

    void SplashScreen::Render(UIRenderer& renderer, const glm::vec2 screenSize) const
    {
        renderer.Begin(screenSize);
        renderer.DrawRect(glm::vec2(0.0f), screenSize, Background);

        const f32 opacity = GetOpacity();
        if (opacity > 0.0f)
        {
            const f32 logoSize = std::min(screenSize.x, screenSize.y) * LogoShare;
            const glm::vec2 center{ screenSize.x * 0.5f, screenSize.y * LogoCenterY };
            const glm::vec2 half(logoSize * 0.5f);

            if (m_Logo)
                renderer.DrawRect(center - half, center + half, m_Logo, { 0.0f, 0.0f }, { 1.0f, 1.0f },
                                  glm::vec4(1.0f, 1.0f, 1.0f, opacity));

            const f32 captionTop = center.y - half.y - logoSize * CaptionGapShare;
            DrawCaption(renderer, screenSize, captionTop, logoSize * CaptionShare, opacity);
        }

        renderer.End();
    }

    void SplashScreen::Enter(const Phase phase)
    {
        m_Phase = phase;
        m_Time = 0.0f;
    }

    f32 SplashScreen::GetOpacity() const
    {
        switch (m_Phase)
        {
            case Phase::FadeIn:  return SmoothStep(m_Time / FadeSeconds);
            case Phase::Hold:    return 1.0f;
            case Phase::FadeOut: return 1.0f - SmoothStep(m_Time / FadeSeconds);
            default:             return 0.0f;
        }
    }
}
