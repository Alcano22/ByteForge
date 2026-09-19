#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Timestep.h"

#include <string>

namespace ByteForge
{
    class Event;

    class BYTEFORGE_API Layer
    {
    public:
        explicit Layer(std::string debugName = "Layer")
            : m_DebugName(std::move(debugName)) {}
        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}

        virtual void OnUpdate(Timestep ts) {}
        virtual void OnImGuiRender() {}
        virtual void OnEvent(Event& event) {}

        [[nodiscard]] const std::string& GetName() const { return m_DebugName; }

    private:
        std::string m_DebugName;
    };
}
