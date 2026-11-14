#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Components/ComponentSerialization.h"
#include "Engine/Scene/Entity.h"

#include <array>
#include <string_view>
#include <utility>

namespace ByteForge
{
    namespace
    {
        template<SerializableComponent T>
        constexpr ComponentDescriptor Describe(const char* key, const bool removable = true,
                                               const bool hidden = false)
        {
            return {
                .Key       = key,
                .Removable = removable,
                .Hidden    = hidden,
                .TypeId    = ComponentTypeId<T>(),
                .Has       = [](const Entity entity) { return entity.HasComponent<T>(); },
                .Remove    = [](const Entity entity)
                {
                    if (entity.HasComponent<T>())
                        entity.RemoveComponent<T>();
                },
                .Copy      = [](const Entity source, const Entity target)
                {
                    const T* component = source.TryGetComponent<T>();
                    if (component == nullptr) return;

                    if (target.HasComponent<T>())
                        target.GetComponent<T>() = *component;
                    else
                        target.AddComponent<T>(*component);
                },
                .Write     = [](const Entity entity, nlohmann::json& json)
                {
                    T::Write(entity.GetComponent<T>(), json);
                },
                .Read      = [](const Entity entity, const nlohmann::json& json)
                {
                    T& component = entity.HasComponent<T>() ? entity.GetComponent<T>() : entity.AddComponent<T>();
                    T::Read(component, json);
                }
            };
        }

        constexpr std::array Descriptors{
            Describe<TransformComponent>("transform", false),
            Describe<SpriteRendererComponent>("spriteRenderer"),
            Describe<CameraComponent>("camera"),
            Describe<Rigidbody2DComponent>("rigidbody2D"),
            Describe<BoxCollider2DComponent>("boxCollider2D"),
            Describe<CircleCollider2DComponent>("circleCollider2D"),
            Describe<AudioSourceComponent>("audioSource"),
            Describe<AnimatorComponent>("animator"),
            Describe<ScriptComponent>("script"),
            Describe<ComponentOrderComponent>("componentOrder", true, true)
        };

        constexpr bool HasUniqueKeys()
        {
            for (usize i = 0; i < Descriptors.size(); ++i)
            {
                for (usize j = i + 1; j < Descriptors.size(); ++j)
                {
                    if (std::string_view(Descriptors[i].Key) == std::string_view(Descriptors[j].Key))
                        return false;
                }
            }
            return true;
        }

        static_assert(HasUniqueKeys(), "ComponentRegistry: two components share the same JSON key");
    }

    std::span<const ComponentDescriptor> ComponentRegistry::GetAll() { return Descriptors; }

    const ComponentDescriptor* ComponentRegistry::Find(const std::string_view key)
    {
        for (const ComponentDescriptor& descriptor : Descriptors)
        {
            if (key == descriptor.Key)
                return &descriptor;
        }
        return nullptr;
    }

    const ComponentDescriptor* ComponentRegistry::FindByType(const u32 typeId)
    {
        for (const ComponentDescriptor& descriptor : Descriptors)
        {
            if (descriptor.TypeId == typeId)
                return &descriptor;
        }
        return nullptr;
    }

    void ComponentRegistry::CopyAll(const Entity source, const Entity target)
    {
        for (const ComponentDescriptor& descriptor : Descriptors)
            descriptor.Copy(source, target);
    }

    void ComponentRegistry::RemoveAll(const Entity entity)
    {
        for (const ComponentDescriptor& descriptor : Descriptors)
        {
            if (descriptor.Removable)
                descriptor.Remove(entity);
        }
    }

    void ComponentRegistry::WriteAll(const Entity entity, nlohmann::json& out)
    {
        for (const ComponentDescriptor& descriptor : Descriptors)
        {
            if (!descriptor.Has(entity)) continue;

            nlohmann::json componentJson = nlohmann::json::object();
            descriptor.Write(entity, componentJson);
            out[descriptor.Key] = std::move(componentJson);
        }
    }

    void ComponentRegistry::ReadAll(const Entity entity, const nlohmann::json& data)
    {
        for (const ComponentDescriptor& descriptor : Descriptors)
        {
            if (const auto it = data.find(descriptor.Key); it != data.end())
                descriptor.Read(entity, *it);
        }
    }
}
