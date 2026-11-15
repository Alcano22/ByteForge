#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace ByteForge
{
    inline constexpr const char* PrefabExtension = ".bfprefab";

    class BYTEFORGE_API Prefab
    {
    public:
        [[nodiscard]] static Result<UUID> Create(Entity root, const fs::path& relativePath);

        [[nodiscard]] static Result<void> Save(UUID handle, Entity root);

        [[nodiscard]] static Result<Entity> Instantiate(Scene& scene, UUID handle, Entity parent = {},
                                                        std::optional<glm::vec2> position = std::nullopt);

        [[nodiscard]] static Result<Entity> LoadForEditing(Scene& scene, UUID handle);

        static usize SyncInstances(Scene& scene, UUID handle);

        static void Link(Entity root, UUID handle);
        static void Unlink(Entity entity);

        [[nodiscard]] static Entity GetInstanceRoot(Entity entity);

        [[nodiscard]] static bool Exists(UUID handle);

        static void Invalidate(UUID handle);
        static void Clear();
    };
}
