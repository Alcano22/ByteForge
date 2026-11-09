#pragma once

#include "Engine/Renderer/OrthographicCamera.h"
#include "Engine/Scene/Components.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ByteForge
{
    [[nodiscard]] inline OrthographicCamera MakeSceneCamera(const CameraComponent& settings,
                                                            const TransformComponent& transform,
                                                            const float aspectRatio)
    {
        OrthographicCamera camera(settings.OrthographicSize, aspectRatio, settings.Near, settings.Far);
        camera.SetOrientation(glm::angleAxis(transform.Rotation, glm::vec3(0.0f, 0.0f, 1.0f)));
        camera.SetPosition(transform.Position);
        return camera;
    }

    [[nodiscard]] inline glm::vec2 ViewportPointToWorld(const Camera& camera, const glm::vec2& viewportPoint,
                                                        const glm::vec2& viewportSize)
    {
        const glm::vec2 ndc = viewportPoint / viewportSize * 2.0f - 1.0f;
        const glm::vec4 world = glm::inverse(camera.GetViewProjection()) * glm::vec4(ndc, 0.0f, 1.0f);
        return { world.x, world.y };
    }
}
