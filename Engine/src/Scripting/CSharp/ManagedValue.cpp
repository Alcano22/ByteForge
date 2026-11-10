#include "Scripting/CSharp/ManagedValue.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

// Slot layout, 16 bytes:
//   Bool                  uint8 at 0
//   Int/Float/Double/Vec  raw value at 0
//   Entity                uint64 UUID at 0
//   Asset                 uint64 handle at 0, uint8 AssetType at 8
//   String                const char* at 0, int32 length at 8 (borrowed, never owned by the slot)

namespace ByteForge::ManagedValue
{
    namespace
    {
        constexpr usize SecondaryOffset = 8;

        static_assert(sizeof(glm::vec4) == SlotSize, "glm::vec4 must match System.Numerics.Vector4");
        static_assert(sizeof(const char*) == SecondaryOffset, "ByteForge requires a 64-bit target");

        template<typename T>
        T Load(const void* data)
        {
            T value;
            std::memcpy(&value, data, sizeof(T));
            return value;
        }

        template<typename T>
        void Store(void* data, const T& value)
        {
            std::memcpy(data, &value, sizeof(T));
        }
    }

    ScriptValue Read(const ScriptFieldType type, const void* slot)
    {
        const auto* secondary = static_cast<const std::byte*>(slot) + SecondaryOffset;

        switch (type)
        {
            case ScriptFieldType::Bool:    return Load<u8>(slot) != 0;
            case ScriptFieldType::Int:     return Load<i32>(slot);
            case ScriptFieldType::Float:   return Load<f32>(slot);
            case ScriptFieldType::Double:  return Load<f64>(slot);
            case ScriptFieldType::Vector2: return Load<glm::vec2>(slot);
            case ScriptFieldType::Vector3: return Load<glm::vec3>(slot);
            case ScriptFieldType::Vector4: return Load<glm::vec4>(slot);
            case ScriptFieldType::Entity:  return EntityRef{ UUID(Load<u64>(slot)) };
            case ScriptFieldType::Asset:
                return AssetRef{ static_cast<AssetType>(Load<u8>(secondary)), UUID(Load<u64>(slot)) };
            case ScriptFieldType::String:
            {
                const auto* text = Load<const char*>(slot);
                const auto length = Load<i32>(secondary);
                return text != nullptr && length > 0 ? std::string(text, static_cast<usize>(length)) : std::string();
            }
        }
        throw std::runtime_error("Unknown script value type");
    }

    void Write(const ScriptValue& value, void* slot)
    {
        auto* secondary = static_cast<std::byte*>(slot) + SecondaryOffset;

        std::visit([slot, secondary]<typename T>(const T& v)
        {
            if constexpr (std::is_same_v<T, bool>)
                Store(slot, static_cast<u8>(v));
            else if constexpr (std::is_same_v<T, EntityRef>)
                Store(slot, static_cast<u64>(v.Id));
            else if constexpr (std::is_same_v<T, AssetRef>)
            {
                Store(slot, static_cast<u64>(v.Handle));
                Store(secondary, static_cast<u8>(v.Type));
            } else if constexpr (std::is_same_v<T, std::string>)
            {
                if (v.size() > static_cast<usize>(std::numeric_limits<i32>::max()))
                    throw std::length_error("Script string is too long");

                Store(slot, v.data());
                Store(secondary, static_cast<i32>(v.size()));
            } else
            {
                static_assert(sizeof(T) <= SlotSize);
                Store(slot, v);
            }
        }, value);
    }
}
