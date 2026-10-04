#pragma once

#include "Engine/Scripting/ScriptBackend.h"
#include "Scripting/Visual/GraphProgram.h"

#include <optional>
#include <string_view>
#include <vector>

namespace ByteForge
{
    class GraphScriptInstance final : public ScriptInstance
    {
    public:
        GraphScriptInstance(Ref<const GraphProgram> program, Entity entity);

        void OnCreate() override;
        void OnDestroy() override;
        void OnUpdate(Timestep ts) override;
        void OnContact(ContactEvent event, Entity other) override;

        [[nodiscard]] std::optional<ScriptValue> GetField(std::string_view name) const override;
        bool SetField(std::string_view name, const ScriptValue& value) override;

    private:
        void Fire(GraphEvent event, std::optional<ScriptValue> payload);
        void Run(uint32_t instruction);
        void StartDelay(uint32_t id, float duration, uint32_t resume);
        void AdvanceDelays(float deltaTime);

    private:
        struct PendingDelay
        {
            uint32_t Id = 0;
            float Remaining = 0.0f;
            uint32_t Resume = 0;
        };

        Ref<const GraphProgram> m_Program;
        Scene* m_Scene;
        std::vector<ScriptValue> m_Slots;
        std::vector<PendingDelay> m_Delays;
        std::vector<bool> m_DelayRunning;
    };
}
