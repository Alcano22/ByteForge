#include "Engine/Scene/Components/PrefabInstanceComponent.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    void PrefabInstanceComponent::Write(const PrefabInstanceComponent& component, nlohmann::json& json)
    {
        json["prefab"] = static_cast<u64>(component.Prefab);
        json["source"] = static_cast<u64>(component.Source);

        if (component.Root)
            json["root"] = true;
    }

    void PrefabInstanceComponent::Read(PrefabInstanceComponent& component, const nlohmann::json& json)
    {
        component.Prefab = UUID(json.value("prefab", u64{ 0 }));
        component.Source = UUID(json.value("source", u64{ 0 }));
        component.Root = json.value("root", false);
    }
}
