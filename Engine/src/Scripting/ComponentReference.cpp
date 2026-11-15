#include "Engine/Scripting/ComponentReference.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"

#include <algorithm>
#include <array>

namespace ByteForge
{
    namespace
    {
        struct BuiltInComponentType
        {
            std::string_view Name;
            bool (*Has)(Entity);
        };

        template<typename T>
        bool Has(const Entity entity) { return entity.HasComponent<T>(); }

        constexpr std::array BuiltInTypes{
            BuiltInComponentType{ "ByteForge.Transform",      &Has<TransformComponent>      },
            BuiltInComponentType{ "ByteForge.SpriteRenderer", &Has<SpriteRendererComponent> },
            BuiltInComponentType{ "ByteForge.Rigidbody2D",    &Has<Rigidbody2DComponent>    },
            BuiltInComponentType{ "ByteForge.AudioSource",    &Has<AudioSourceComponent>    },
            BuiltInComponentType{ "ByteForge.Animator",       &Has<AnimatorComponent>       }
        };
    }

    bool HasComponentType(const Entity entity, const std::string_view typeName)
    {
        if (!entity.IsValid())
            return false;

        for (const BuiltInComponentType& type : BuiltInTypes)
        {
            if (type.Name == typeName)
                return type.Has(entity);
        }

        const auto* scripts = entity.TryGetComponent<ScriptComponent>();
        return scripts != nullptr && std::ranges::any_of(scripts->Scripts, [typeName](const ScriptEntry& script)
        {
            return script.ClassName == typeName;
        });
    }

    std::string_view ComponentTypeLabel(const std::string_view typeName)
    {
        const usize split = typeName.find_last_of(".+");
        return split == std::string_view::npos ? typeName : typeName.substr(split + 1);
    }
}
