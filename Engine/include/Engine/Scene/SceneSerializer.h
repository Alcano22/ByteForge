#pragma once

#include "Engine/Core/Core.h"

#include <nlohmann/json.hpp>

#include <string>

namespace ByteForge
{
    class Entity;
    class Scene;

    class BYTEFORGE_API SceneSerializer
    {
    public:
        [[nodiscard]] static nlohmann::json Serialize(Scene& scene);
        static void Deserialize(Scene& scene, const nlohmann::json& data);

        static void SerializeToFile(Scene& scene, const std::string& filepath);
        [[nodiscard]] static bool DeserializeFromFile(Scene& scene, const std::string& filepath);

        [[nodiscard]] static nlohmann::json SerializeEntity(Entity entity);
        static Entity DeserializeEntity(Scene& scene, const nlohmann::json& data);
        static void ApplyEntity(Entity entity, const nlohmann::json& data);
    };
}
