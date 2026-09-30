#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace ByteForge
{
    static_assert(sizeof(glm::vec2) == 2 * sizeof(float), "glm::vec2 must match System.Numerics.Vector2");
    static_assert(sizeof(glm::vec3) == 3 * sizeof(float), "glm::vec3 must match System.Numerics.Vector3");

    struct NativeAPI
    {
        int Size;

        void (*Log)(int level, const char* message);
        void (*ReportException)(const char* message);

        int (*Entity_IsValid)(uint64_t entity);

        int (*Transform_GetPosition)(uint64_t entity, glm::vec3* out);
        int (*Transform_SetPosition)(uint64_t entity, const glm::vec3* value);
        int (*Transform_GetRotation)(uint64_t entity, float* out);
        int (*Transform_SetRotation)(uint64_t entity, float value);
        int (*Transform_GetScale)(uint64_t entity, glm::vec2* out);
        int (*Transform_SetScale)(uint64_t entity, const glm::vec2* value);
    };
}
