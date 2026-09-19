#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Pipeline.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

namespace ByteForge
{
    class BYTEFORGE_API Material
    {
    public:
        virtual ~Material() = default;

        [[nodiscard]] virtual const Ref<Pipeline>& GetPipeline() const = 0;

        virtual void SetRaw(std::string_view name, std::span<const std::byte> data) = 0;

        template<typename T>
        void Set(const std::string_view name, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>, "Material parameters must be trivially copyable");
            SetRaw(name, std::as_bytes(std::span(&value, 1)));
        }

        static Ref<Material> Create(const Ref<Pipeline>& pipeline);
    };
}
