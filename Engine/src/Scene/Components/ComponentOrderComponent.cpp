#include "Engine/Scene/Components/ComponentOrderComponent.h"
#include "Engine/Scene/ComponentRegistry.h"

#include <nlohmann/json.hpp>

#include <string>

namespace ByteForge
{
    void ComponentOrderComponent::Write(const ComponentOrderComponent& component, nlohmann::json& json)
    {
        json = nlohmann::json::array();

        for (const u32 typeId : component.TypeIds)
        {
            if (const ComponentDescriptor* descriptor = ComponentRegistry::FindByType(typeId))
                json.push_back(descriptor->Key);
        }
    }

    void ComponentOrderComponent::Read(ComponentOrderComponent& component, const nlohmann::json& json)
    {
        component.TypeIds.clear();

        if (!json.is_array()) return;

        for (const auto& entry : json)
        {
            if (!entry.is_string()) continue;

            if (const ComponentDescriptor* descriptor = ComponentRegistry::Find(entry.get<std::string>()))
                component.TypeIds.push_back(descriptor->TypeId);
        }
    }
}
