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
        [[nodiscard]] static std::expected<UUID, std::string> Create(Entity root,
                                                                     const std::filesystem::path& relativePath);

        [[nodiscard]] static std::expected<Entity, std::string> Instantiate(
            Scene& scene, UUID handle, Entity parent = {}, std::optional<glm::vec2> position = std::nullopt);

        static void Invalidate(UUID handle);
        static void Clear();
    };
}
