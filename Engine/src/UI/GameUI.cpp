#include "Engine/UI/GameUI.h"
#include "Engine/Renderer/UIRenderer.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>

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
                elements.push_back({ .Handle = child, .Bounds = bounds });

                LayoutChildren(child, bounds, scale, elements);
            }
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

        void DrawImage(UIRenderer& renderer, const ImageComponent& image, const Rect& bounds, const f32 scale)
        {
            if (!image.Sprite)
            {
                renderer.DrawRect(bounds.Min, bounds.Max, image.Color);
                return;
            }

            const Sprite& sprite = *image.Sprite;
            const bool sliced = image.Mode == ImageComponent::DrawMode::Sliced && image.Border != glm::vec4(0.0f);

            if (sliced)
                DrawSliced(renderer, sprite, image.Color, bounds, image.Border, scale);
            else
            {
                renderer.DrawRect(bounds.Min, bounds.Max, sprite.GetTexture(),
                                  sprite.GetUVMin(), sprite.GetUVMax(), image.Color);
            }
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

    void Render(Scene& scene, UIRenderer& renderer, const glm::vec2& screenSize)
    {
        if (screenSize.x < 1.0f || screenSize.y < 1.0f) return;

        const std::vector<CanvasLayout> canvases = ComputeLayout(scene, screenSize);
        if (canvases.empty()) return;

        renderer.Begin(screenSize);

        for (const CanvasLayout& canvas : canvases)
        {
            for (const Element& element : canvas.Elements)
            {
                if (const auto* image = element.Handle.TryGetComponent<ImageComponent>())
                    DrawImage(renderer, *image, element.Bounds, canvas.Scale);
            }
        }

        renderer.End();
    }
}
