#include "Editor/Viewport/SceneGizmos.h"
#include "Editor/Viewport/EditorCamera2D.h"
#include "Editor/Theme/EditorColors.h"

#include <Engine/Renderer/LineRenderer.h>
#include <Engine/Scene/ColliderGeometry.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>

namespace ByteForge::SceneGizmos
{
    namespace
    {
        constexpr f32 MinLineSpacingPixels = 12.0f;
        constexpr i64 MaxLinesPerAxis = 1000;

        constexpr f32 DashPixels = 6.0f;
        constexpr usize MaxDashesPerOutline = 200;
        constexpr f32 CirclePixelsPerSegment = 6.0f;
        constexpr usize MinCircleSegments = 12;
        constexpr usize MaxCircleSegments = 64;
        constexpr f32 UnselectedAlpha = 0.55f;
        constexpr f32 InactiveAlpha = 0.35f;

        constexpr u32 MaxLinesPerCollider = static_cast<u32>(MaxDashesPerOutline + MaxCircleSegments);
        constexpr u32 ReservedLines = 64;

        glm::vec4 LineColor(const i64 index, const glm::vec4& axisColor)
        {
            if (index == 0)
                return axisColor;

            return EditorColors::GetVec4(index % 10 == 0 ? EditorColor::GridMajor : EditorColor::GridMinor);
        }

        struct ColliderPass
        {
            glm::vec2 VisibleMin;
            glm::vec2 VisibleMax;
            f32 WorldUnitsPerPixel = 1.0f;
            f32 Dash = 0.0f;
            Entity Selected;
        };

        struct BodyFrame
        {
            glm::vec2 Position;
            glm::vec2 Scale{ 1.0f };
            f32 Cos = 1.0f;
            f32 Sin = 0.0f;

            [[nodiscard]] glm::vec2 ToWorld(const glm::vec2 local) const
            {
                return Position + glm::vec2(local.x * Cos - local.y * Sin, local.x * Sin + local.y * Cos);
            }
        };

        BodyFrame GetBodyFrame(const Entity entity)
        {
            const TransformComponent world = entity.GetWorldTransform();
            return {
                .Position = glm::vec2(world.Position),
                .Scale    = world.Scale,
                .Cos      = std::cos(world.Rotation),
                .Sin      = std::sin(world.Rotation)
            };
        }

        bool IsVisible(const glm::vec2 center, const f32 radius, const ColliderPass& pass)
        {
            return center.x + radius >= pass.VisibleMin.x && center.x - radius <= pass.VisibleMax.x &&
                   center.y + radius >= pass.VisibleMin.y && center.y - radius <= pass.VisibleMax.y;
        }

        glm::vec4 GetColliderColor(const Entity entity, const ColliderPass& pass)
        {
            glm::vec4 color = EditorColors::GetVec4(EditorColor::Collider);

            if (entity != pass.Selected)
                color.a *= UnselectedAlpha;
            if (!entity.HasComponent<Rigidbody2DComponent>())
                color.a *= InactiveAlpha;

            return color;
        }

        void DrawOutline(LineRenderer& lines, const std::span<const glm::vec2> points,
                         const glm::vec4& color, f32 dash)
        {
            const usize count = points.size();
            if (count < 2) return;

            const auto next = [&](const usize i) { return points[(i + 1) % count]; };

            if (dash <= 0.0f)
            {
                for (usize i = 0; i < count; ++i)
                    lines.DrawLine(glm::vec3(points[i], 0.0f), glm::vec3(next(i), 0.0f), color);
                return;
            }

            f32 perimeter = 0.0f;
            for (usize i = 0; i < count; ++i)
                perimeter += glm::distance(points[i], next(i));

            dash = std::max(dash, perimeter / static_cast<f32>(MaxDashesPerOutline * 2));

            bool drawing = true;
            f32 left = dash;

            for (usize i = 0; i < count; ++i)
            {
                const glm::vec2 from = points[i];
                const glm::vec2 to = next(i);

                const f32 length = glm::distance(from, to);
                if (length <= 0.0f) continue;

                const glm::vec2 direction = (to - from) / length;

                f32 position = 0.0f;
                while (position < length)
                {
                    const f32 step = std::min(left, length - position);

                    if (drawing)
                    {
                        lines.DrawLine(glm::vec3(from + direction * position, 0.0f),
                                       glm::vec3(from + direction * (position + step), 0.0f), color);
                    }

                    position += step;
                    left -= step;

                    if (left <= 0.0f)
                    {
                        drawing = !drawing;
                        left = dash;
                    }
                }
            }
        }

        void DrawBoxCollider(LineRenderer& lines, const Entity entity, const BoxCollider2DComponent& collider,
                             const ColliderPass& pass)
        {
            const BodyFrame body = GetBodyFrame(entity);
            const BoxColliderGeometry geometry = GetColliderGeometry(collider, body.Scale);
            const glm::vec2 center = geometry.Center;
            const glm::vec2 half = geometry.HalfExtents;

            if (!IsVisible(body.ToWorld(center), glm::length(half), pass)) return;

            const std::array<glm::vec2, 4> corners{
                body.ToWorld(collider.Offset + glm::vec2(-half.x, -half.y)),
                body.ToWorld(collider.Offset + glm::vec2( half.x, -half.y)),
                body.ToWorld(collider.Offset + glm::vec2( half.x,  half.y)),
                body.ToWorld(collider.Offset + glm::vec2(-half.x,  half.y))
            };

            DrawOutline(lines, corners, GetColliderColor(entity, pass), collider.IsSensor ? pass.Dash : 0.0f);
        }

        void DrawCircleCollider(LineRenderer& lines, const Entity entity, const CircleCollider2DComponent& collider,
                                const ColliderPass& pass)
        {
            const BodyFrame body = GetBodyFrame(entity);
            const CircleColliderGeometry geometry = GetColliderGeometry(collider, body.Scale);
            const glm::vec2 center = body.ToWorld(geometry.Center);
            const f32 radius = geometry.Radius;

            if (!IsVisible(center, radius, pass)) return;

            const f32 circumferencePixels = 2.0f * std::numbers::pi_v<f32> * radius / pass.WorldUnitsPerPixel;
            const auto segments = static_cast<usize>(std::clamp(circumferencePixels / CirclePixelsPerSegment,
                                                                static_cast<f32>(MinCircleSegments),
                                                                static_cast<f32>(MaxCircleSegments)));

            std::array<glm::vec2, MaxCircleSegments> points{};
            for (usize i = 0; i < segments; ++i)
            {
                const f32 angle = 2.0f * std::numbers::pi_v<f32> * static_cast<f32>(i) / static_cast<f32>(segments);
                points[i] = center + radius * glm::vec2(std::cos(angle), std::sin(angle));
            }

            DrawOutline(lines, std::span(points.data(), segments), GetColliderColor(entity, pass),
                        collider.IsSensor ? pass.Dash : 0.0f);
        }
    }

    void DrawGrid(LineRenderer& lines, const EditorCamera2D& camera)
    {
        const f32 spacing =
            std::pow(10.0f, std::ceil(std::log10(camera.GetWorldUnitsPerPixel() * MinLineSpacingPixels)));

        const glm::vec2 min = camera.GetVisibleMin();
        const glm::vec2 max = camera.GetVisibleMax();

        const auto firstX = static_cast<i64>(std::floor(min.x / spacing));
        const auto lastX = static_cast<i64>(std::ceil(max.x / spacing));
        const auto firstY = static_cast<i64>(std::floor(min.y / spacing));
        const auto lastY = static_cast<i64>(std::ceil(max.y / spacing));

        if (lastX - firstX > MaxLinesPerAxis || lastY - firstY > MaxLinesPerAxis) return;

        const glm::vec4 axisColorX = EditorColors::GetVec4(EditorColor::AxisX);
        const glm::vec4 axisColorY = EditorColors::GetVec4(EditorColor::AxisY);

        for (i64 i = firstX; i <= lastX; ++i)
        {
            const f32 x = static_cast<f32>(i) * spacing;
            lines.DrawLine({ x, min.y, 0.0f }, { x, max.y, 0.0f }, LineColor(i, axisColorX));
        }

        for (i64 i = firstY; i <= lastY; ++i)
        {
            const f32 y = static_cast<f32>(i) * spacing;
            lines.DrawLine({ min.x, y, 0.0f }, { max.x, y, 0.0f }, LineColor(i, axisColorY));
        }
    }

    void DrawColliders(LineRenderer& lines, Scene& scene, const EditorCamera2D& camera,
                       const Entity selected, const ColliderVisibility visibility)
    {
        if (visibility == ColliderVisibility::Off) return;

        const f32 unitsPerPixel = camera.GetWorldUnitsPerPixel();
        const ColliderPass pass{
            .VisibleMin         = camera.GetVisibleMin(),
            .VisibleMax         = camera.GetVisibleMax(),
            .WorldUnitsPerPixel = unitsPerPixel,
            .Dash               = unitsPerPixel * DashPixels,
            .Selected           = selected
        };

        const auto hasBudget = [&lines] { return lines.GetRemainingLines() > MaxLinesPerCollider + ReservedLines; };

        if (visibility == ColliderVisibility::Selected)
        {
            if (!selected.IsValid()) return;

            if (const auto* box = selected.TryGetComponent<BoxCollider2DComponent>(); box != nullptr && hasBudget())
                DrawBoxCollider(lines, selected, *box, pass);
            if (const auto* circle = selected.TryGetComponent<CircleCollider2DComponent>(); circle != nullptr && hasBudget())
                DrawCircleCollider(lines, selected, *circle, pass);
            return;
        }

        scene.Each<BoxCollider2DComponent>([&](const Entity entity, const BoxCollider2DComponent& collider)
        {
            if (hasBudget())
                DrawBoxCollider(lines, entity, collider, pass);
        });

        scene.Each<CircleCollider2DComponent>([&](const Entity entity, const CircleCollider2DComponent& collider)
        {
            if (hasBudget())
                DrawCircleCollider(lines, entity, collider, pass);
        });
    }

    void DrawSelection(LineRenderer& lines, const Entity entity)
    {
        const TransformComponent transform = entity.GetWorldTransform();
        lines.DrawRect(transform.Position, glm::abs(transform.Scale), transform.Rotation,
                       EditorColors::GetVec4(EditorColor::Selection));
    }
}
