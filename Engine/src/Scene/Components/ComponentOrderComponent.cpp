#include "Engine/Scene/Components/ComponentOrderComponent.h"
#include "Engine/Scene/Components/ScriptComponent.h"
#include "Engine/Scene/ComponentRegistry.h"

#include <nlohmann/json.hpp>

#include <charconv>
#include <format>
#include <string>
#include <string_view>

namespace ByteForge
{
    namespace
    {
        constexpr std::string_view ScriptPrefix = "script:";
    }

    void ComponentOrderComponent::Write(const ComponentOrderComponent& component, nlohmann::json& json)
    {
        json = nlohmann::json::array();

        for (const ComponentOrderKey& key : component.Keys)
        {
            if (key.ScriptId != 0)
                json.push_back(std::format("{}{}", ScriptPrefix, key.ScriptId));
            else if (const ComponentDescriptor* descriptor = ComponentRegistry::FindByType(key.TypeId))
                json.push_back(descriptor->Key);
        }
    }

    void ComponentOrderComponent::Read(ComponentOrderComponent& component, const nlohmann::json& json)
    {
        component.Keys.clear();

        if (!json.is_array()) return;

        for (const auto& entry : json)
        {
            if (!entry.is_string()) continue;

            const std::string text = entry.get<std::string>();

            if (std::string_view(text).starts_with(ScriptPrefix))
            {
                const char* begin = text.data() + ScriptPrefix.size();
                const char* end = text.data() + text.size();

                u64 id = 0;
                if (const auto [rest, error] = std::from_chars(begin, end, id); error == std::errc{} && rest == end && id != 0)
                    component.Keys.push_back({ .TypeId = ComponentTypeId<ScriptComponent>(), .ScriptId = id });
                continue;
            }

            if (const ComponentDescriptor* descriptor = ComponentRegistry::Find(text))
                component.Keys.push_back({ .TypeId = descriptor->TypeId });
        }
    }
}
