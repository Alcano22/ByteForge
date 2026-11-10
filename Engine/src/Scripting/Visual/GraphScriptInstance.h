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
        void Run(u32 instruction);
        void StartDelay(u32 id, f32 duration, u32 resume);
        void AdvanceDelays(f32 deltaTime);

    private:
        struct PendingDelay
        {
            u32 Id = 0;
            f32 Remaining = 0.0f;
            u32 Resume = 0;
        };

        Ref<const GraphProgram> m_Program;
        Scene* m_Scene;
        std::vector<ScriptValue> m_Slots;
        std::vector<PendingDelay> m_Delays;
        std::vector<bool> m_DelayRunning;
    };
}
