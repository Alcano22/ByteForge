#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/ScriptableEntity.h"

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace ByteForge
{
    class BYTEFORGE_API NativeScripts
    {
    public:
        using Factory = std::function<Scope<ScriptableEntity>()>;

        template<typename T>
        static void Register(std::string className)
        {
            static_assert(std::is_base_of_v<ScriptableEntity, T>,
                          "NativeScripts::Register: T must derive from ScriptableEntity");
            static_assert(std::is_default_constructible_v<T>,
                          "NativeScripts::Register: T must be default-constructible");

            RegisterFactory(std::move(className), [] { return Scope<ScriptableEntity>(MakeScope<T>()); });
        }

        static void RegisterFactory(std::string className, Factory factory);
    };
}
