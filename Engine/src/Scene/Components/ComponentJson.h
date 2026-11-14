#pragma once

#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/PhysicsMaterial.h"

#include <nlohmann/json.hpp>

namespace ByteForge::ComponentJson
{
    inline void WriteMaterial(nlohmann::json& json, const Ref<PhysicsMaterialAsset>& material)
    {
        if (material && material->IsFileBacked())
            json["material"] = static_cast<u64>(material->GetHandle());
    }

    inline Ref<PhysicsMaterialAsset> ReadMaterial(const nlohmann::json& json)
    {
        if (!json.contains("material"))
            return nullptr;

        return AssetManager::LoadPhysicsMaterial(UUID(json.at("material").get<u64>()));
    }
}
