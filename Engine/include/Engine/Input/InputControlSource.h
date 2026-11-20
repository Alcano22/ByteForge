#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Input/InputControl.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    class BYTEFORGE_API InputControlSource
    {
    public:
        virtual ~InputControlSource() = default;

        [[nodiscard]] virtual glm::vec2 Read(const InputControl& control) const = 0;
    };

    class BYTEFORGE_API RawInputSource final : public InputControlSource
    {
    public:
        [[nodiscard]] glm::vec2 Read(const InputControl& control) const override;
    };
}
