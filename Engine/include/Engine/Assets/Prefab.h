#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ByteForge
{
    inline constexpr const char* PrefabExtension = ".bfprefab";

    enum class PrefabOverrideScope : u8
    {
        Path,
        Entity,
        Instance
    };

    class BYTEFORGE_API Prefab
    {
    public:
        [[nodiscard]] static Result<UUID> Create(Entity root, const fs::path& relativePath);

        [[nodiscard]] static Result<void> Save(UUID handle, Entity root);

        [[nodiscard]] static Result<nlohmann::json> ReadData(UUID handle);
        [[nodiscard]] static Result<void> WriteData(UUID handle, const nlohmann::json& data);

        [[nodiscard]] static Result<Entity> Instantiate(Scene& scene, UUID handle, Entity parent = {},
                                                        std::optional<glm::vec2> position = std::nullopt);

        [[nodiscard]] static Result<Entity> LoadForEditing(Scene& scene, UUID handle);

        static usize SyncInstances(Scene& scene, UUID handle);
        static usize SyncAllInstances(Scene& scene);

        static void RecordOverrides(Scene& scene);

        [[nodiscard]] static nlohmann::json GetOverrides(Entity entity);

        [[nodiscard]] static std::string ComponentOverridePath(u32 componentTypeId);
        [[nodiscard]] static std::string ScriptOverridePath(u64 scriptId);

        [[nodiscard]] static bool IsOverridden(const nlohmann::json& overrides, std::string_view path);

        [[nodiscard]] static Result<void> ApplyOverrides(Entity entity, PrefabOverrideScope scope,
                                                         std::string_view path = {});

        [[nodiscard]] static Result<void> RevertOverrides(Entity entity, PrefabOverrideScope scope,
                                                          std::string_view path = {});

        static void Link(Entity root, UUID handle);
        static void Unlink(Entity entity);

        [[nodiscard]] static Entity GetInstanceRoot(Entity entity);

        [[nodiscard]] static bool Exists(UUID handle);

        static void Invalidate(UUID handle);
        static void Clear();
    };
}
