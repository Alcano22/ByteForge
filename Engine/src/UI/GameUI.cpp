#include "Engine/UI/GameUI.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Core/Utf8.h"
#include "Engine/Input/Input.h"
#include "Engine/Renderer/UIRenderer.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Text/TextLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <utility>

namespace ByteForge::GameUI
{
    namespace
    {
        constexpr f32 MinScale = 0.01f;

        bool IsInsideCanvas(const Entity entity)
        {
            for (Entity parent = entity.GetParent(); parent.IsValid(); parent = parent.GetParent())
            {
                if (parent.HasComponent<CanvasComponent>())
                    return true;
            }
            return false;
        }

        void LayoutChildren(const Entity parent, const Rect& parentRect, const f32 scale,
                            std::vector<Element>& elements)
        {
            for (const Entity child : parent.GetChildren())
            {
                const auto* transform = child.TryGetComponent<RectTransformComponent>();
                if (transform == nullptr) continue;

                const Rect bounds = ComputeRect(*transform, parentRect, scale);
                elements.push_back({ .Handle = child, .Bounds = bounds, .Parent = parentRect });

                LayoutChildren(child, bounds, scale, elements);
            }
        }

        bool IsPointerTarget(const Entity element)
        {
            if (element.HasComponent<ButtonComponent>())
                return true;

            const auto* image = element.TryGetComponent<ImageComponent>();
            return image != nullptr && image->RaycastTarget;
        }

        glm::vec2 ToScreenPoint(const glm::vec2& mouse, const glm::vec2& viewSize, const glm::vec2& screenSize)
        {
            const glm::vec2 scaled = mouse * (screenSize / viewSize);
            return { scaled.x, screenSize.y - scaled.y };
        }

        void DrawSliced(UIRenderer& renderer, const Sprite& sprite, const glm::vec4& color,
                        const Rect& bounds, const glm::vec4& border, const f32 scale)
        {
            const Ref<Texture2D>& texture = sprite.GetTexture();
            const glm::vec2 textureSize{ static_cast<f32>(texture->GetWidth()),
                                         static_cast<f32>(texture->GetHeight()) };
            const glm::vec2& uvMin = sprite.GetUVMin();
            const glm::vec2& uvMax = sprite.GetUVMax();
            const glm::vec2 size = bounds.GetSize();

            glm::vec2 low{ border.x * scale, border.y * scale };
            glm::vec2 high{ border.z * scale, border.w * scale };

            for (glm::length_t axis = 0; axis < 2; ++axis)
            {
                const f32 total = low[axis] + high[axis];
                if (total <= size[axis] || total <= 0.0f) continue;

                const f32 shrink = size[axis] / total;
                low[axis] *= shrink;
                high[axis] *= shrink;
            }

            const std::array<f32, 4> xs{ bounds.Min.x, bounds.Min.x + low.x, bounds.Max.x - high.x, bounds.Max.x };
            const std::array<f32, 4> ys{ bounds.Min.y, bounds.Min.y + low.y, bounds.Max.y - high.y, bounds.Max.y };

            const std::array<f32, 4> us{
                uvMin.x, uvMin.x + border.x / textureSize.x, uvMax.x - border.z / textureSize.x, uvMax.x
            };
            const std::array<f32, 4> vs{
                uvMax.y, uvMax.y - border.y / textureSize.y, uvMin.y + border.w / textureSize.y, uvMin.y
            };

            for (usize row = 0; row < 3; ++row)
            {
                for (usize column = 0; column < 3; ++column)
                {
                    renderer.DrawRect(glm::vec2(xs[column], ys[row]), glm::vec2(xs[column + 1], ys[row + 1]), texture,
                                      glm::vec2(us[column], vs[row + 1]), glm::vec2(us[column + 1], vs[row]), color);
                }
            }
        }

        void DrawImage(UIRenderer& renderer, const Entity element, const ImageComponent& image,
                       const Rect& bounds, const f32 scale)
        {
            glm::vec4 color = image.Color;
            if (const auto* button = element.TryGetComponent<ButtonComponent>())
                color *= button->GetStateColor();

            if (!image.Sprite)
            {
                renderer.DrawRect(bounds.Min, bounds.Max, color);
                return;
            }

            const Sprite& sprite = *image.Sprite;
            const bool sliced = image.Mode == ImageComponent::DrawMode::Sliced && image.Border != glm::vec4(0.0f);

            if (sliced)
                DrawSliced(renderer, sprite, color, bounds, image.Border, scale);
            else
            {
                renderer.DrawRect(bounds.Min, bounds.Max, sprite.GetTexture(),
                                  sprite.GetUVMin(), sprite.GetUVMax(), color);
            }
        }

        Ref<Font> ResolveFont(const UUID handle)
        {
            const Ref<FontAsset> asset = AssetManager::LoadFont(handle);
            if (asset->GetFont())
                return asset->GetFont();

            if (handle.IsSet() && asset->GetState() == AssetLoadState::Failed)
                return AssetManager::LoadFont(UUID(0))->GetFont();

            return nullptr;
        }

        glm::vec4 StyleColor(const TextStyle& style, const glm::vec4& textColor)
        {
            if (!style.Color)
                return textColor;
            return { style.Color->r, style.Color->g, style.Color->b, style.Color->a * textColor.a };
        }

        constexpr f32 MaxWeight = 0.45f;

        constexpr f32 MinBarThickness = 1.0f;

        f32 BoldWeight(const Font& font)
        {
            return std::min(BoldDilation * font.GetGlyphSize() / font.GetDistanceRange(), MaxWeight);
        }

        void DrawBar(UIRenderer& renderer, const PlacedBar& bar, const glm::vec2& origin, const glm::vec4& color)
        {
            glm::vec2 min = origin + bar.Min;
            glm::vec2 max = origin + bar.Max;

            const f32 missing = MinBarThickness - (max.y - min.y);
            if (missing > 0.0f)
            {
                min.y -= missing * 0.5f;
                max.y += missing * 0.5f;
            }

            renderer.DrawRect(min, max, color);
        }

        void DrawTextElement(UIRenderer& renderer, const TextComponent& text, const Rect& bounds, const f32 scale)
        {
            if (text.Text.empty()) return;

            const Ref<Font> font = ResolveFont(text.Font);
            if (!font) return;

            const TextLayoutSettings settings{
                .FontSize    = text.FontSize * scale,
                .BoxSize     = bounds.GetSize(),
                .Horizontal  = text.Horizontal,
                .Vertical    = text.Vertical,
                .WordWrap    = text.WordWrap,
                .Overflow    = text.Overflow,
                .MinFontSize = text.MinFontSize * scale
            };
            std::u32string characters = DecodeUtf8(text.Text);
            const StyledText styled = text.RichText ? ParseRichText(characters) : MakePlainText(std::move(characters));
            const TextLayout layout = LayoutText(font->GetGlyphs(), styled, settings);

            const glm::vec2 origin{ bounds.Min.x, bounds.Max.y };

            const bool clip = text.Overflow == TextOverflow::Clip;
            if (clip)
                renderer.PushClipRect(bounds.Min, bounds.Max);

            const f32 boldWeight = BoldWeight(*font);

            for (const PlacedGlyph& glyph : layout.Glyphs)
            {
                const TextStyle& style = styled.Styles[glyph.Style];

                renderer.DrawGlyph({
                    .Min           = origin + glyph.Min,
                    .Max           = origin + glyph.Max,
                    .BottomShift   = glyph.BottomShift,
                    .TopShift      = glyph.TopShift,
                    .UVMin         = glyph.UVMin,
                    .UVMax         = glyph.UVMax,
                    .DistanceRange = font->GetDistanceRange(),
                    .Weight        = style.Bold ? boldWeight : 0.0f,
                    .Color         = StyleColor(style, text.Color)
                }, font->GetAtlas());
            }

            for (const PlacedBar& bar : layout.Bars)
                DrawBar(renderer, bar, origin, StyleColor(styled.Styles[bar.Style], text.Color));

            if (clip)
                renderer.PopClipRect();
        }
    }

    f32 ComputeCanvasScale(const CanvasComponent& canvas, const glm::vec2& screenSize)
    {
        if (canvas.Scaling == CanvasComponent::ScaleMode::ConstantPixelSize)
            return std::max(canvas.ScaleFactor, MinScale);

        const glm::vec2 reference = glm::max(canvas.ReferenceResolution, glm::vec2(1.0f));
        const glm::vec2 screen = glm::max(screenSize, glm::vec2(1.0f));

        const f32 widthLog = std::log2(screen.x / reference.x);
        const f32 heightLog = std::log2(screen.y / reference.y);
        const f32 match = std::clamp(canvas.MatchWidthOrHeight, 0.0f, 1.0f);

        return std::max(std::exp2(std::lerp(widthLog, heightLog, match)), MinScale);
    }

    Rect ComputeRect(const RectTransformComponent& transform, const Rect& parent, const f32 scale)
    {
        const glm::vec2 parentSize = parent.GetSize();
        const glm::vec2 anchorMin = parent.Min + parentSize * transform.AnchorMin;
        const glm::vec2 anchorMax = parent.Min + parentSize * transform.AnchorMax;

        const glm::vec2 size = glm::max(anchorMax - anchorMin + transform.Size * scale, glm::vec2(0.0f));
        const glm::vec2 pivot = glm::mix(anchorMin, anchorMax, transform.Pivot) + transform.Position * scale;

        const glm::vec2 min = pivot - size * transform.Pivot;
        return { .Min = min, .Max = min + size };
    }

    void SetRect(RectTransformComponent& transform, const Rect& parent, const f32 scale, const Rect& rect)
    {
        const f32 safeScale = std::max(scale, MinScale);

        const glm::vec2 parentSize = parent.GetSize();
        const glm::vec2 anchorMin = parent.Min + parentSize * transform.AnchorMin;
        const glm::vec2 anchorMax = parent.Min + parentSize * transform.AnchorMax;

        const glm::vec2 size = rect.GetSize();
        const glm::vec2 pivot = rect.Min + size * transform.Pivot;

        transform.Size = (size - (anchorMax - anchorMin)) / safeScale;
        transform.Position = (pivot - glm::mix(anchorMin, anchorMax, transform.Pivot)) / safeScale;
    }

    std::vector<CanvasLayout> ComputeLayout(Scene& scene, const glm::vec2& screenSize)
    {
        std::vector<CanvasLayout> layouts;

        scene.Each<CanvasComponent>([&](const Entity entity, const CanvasComponent& canvas)
        {
            if (IsInsideCanvas(entity)) return;

            CanvasLayout layout{
                .Canvas    = entity,
                .Scale     = ComputeCanvasScale(canvas, screenSize),
                .SortOrder = canvas.SortOrder
            };

            LayoutChildren(entity, Rect{ .Min = glm::vec2(0.0f), .Max = screenSize }, layout.Scale, layout.Elements);
            layouts.push_back(std::move(layout));
        });

        std::ranges::stable_sort(layouts, {}, &CanvasLayout::SortOrder);
        return layouts;
    }

    Entity FindPointerTarget(const std::vector<CanvasLayout>& layouts, const glm::vec2& point)
    {
        for (const CanvasLayout& canvas : std::views::reverse(layouts))
        {
            for (const Element& element : std::views::reverse(canvas.Elements))
            {
                if (element.Bounds.Contains(point) && IsPointerTarget(element.Handle))
                    return element.Handle;
            }
        }
        return {};
    }

    Entity FindButton(const Entity element)
    {
        for (Entity entity = element; entity.IsValid(); entity = entity.GetParent())
        {
            if (entity.HasComponent<CanvasComponent>() && !entity.HasComponent<RectTransformComponent>()) break;

            if (entity.HasComponent<ButtonComponent>())
                return entity;
        }
        return {};
    }

    std::vector<Entity> UpdatePointer(Scene& scene)
    {
        Input::SetPointerOverUI(false);

        const glm::vec2 viewSize = Input::GetViewportSize();
        const glm::vec2 screenSize = Input::GetViewportPixelSize();

        const bool hasPointer = Input::HasViewport() && !Input::IsMouseBlocked()
                             && viewSize.x >= 1.0f && viewSize.y >= 1.0f;

        Entity target;
        if (hasPointer)
        {
            const glm::vec2 point = ToScreenPoint(Input::GetViewportMousePosition(), viewSize, screenSize);
            target = FindPointerTarget(ComputeLayout(scene, screenSize), point);
        }

        const Entity hoveredButton = FindButton(target);

        const bool pressed = Input::IsMouseButtonPressed(MouseButton::ButtonLeft);
        const bool released = Input::IsMouseButtonReleased(MouseButton::ButtonLeft);
        const bool down = Input::IsMouseButtonDown(MouseButton::ButtonLeft);

        std::vector<Entity> clicked;
        bool holding = false;

        scene.Each<ButtonComponent>([&](const Entity entity, ButtonComponent& button)
        {
            button.Hovered = button.Interactable && entity == hoveredButton;

            if (!button.Interactable)
            {
                button.Pressed = false;
                return;
            }

            if (pressed && button.Hovered)
                button.Pressed = true;

            if (button.Pressed && released)
            {
                if (button.Hovered)
                    clicked.push_back(entity);
                button.Pressed = false;
            } else if (button.Pressed && !down)
            {
                button.Pressed = false;
            }

            holding |= button.Pressed;
        });

        Input::SetPointerOverUI(target.IsValid() || holding);
        return clicked;
    }

    void Render(Scene& scene, UIRenderer& renderer, const glm::vec2& screenSize,
                const std::optional<glm::mat4>& toClip)
    {
        if (screenSize.x < 1.0f || screenSize.y < 1.0f) return;

        const std::vector<CanvasLayout> canvases = ComputeLayout(scene, screenSize);
        if (canvases.empty()) return;

        if (toClip)
            renderer.Begin(screenSize, *toClip);
        else
            renderer.Begin(screenSize);

        for (const CanvasLayout& canvas : canvases)
        {
            for (const Element& element : canvas.Elements)
            {
                if (const auto* image = element.Handle.TryGetComponent<ImageComponent>())
                    DrawImage(renderer, element.Handle, *image, element.Bounds, canvas.Scale);

                if (const auto* text = element.Handle.TryGetComponent<TextComponent>())
                    DrawTextElement(renderer, *text, element.Bounds, canvas.Scale);
            }
        }

        renderer.End();
    }
}
