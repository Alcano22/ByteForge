#include "Scripting/Visual/GraphScriptInstance.h"

#include "Engine/Scripting/API/ScriptAPI.h"

#include <algorithm>
#include <format>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr size_t MaxInstructionsPerRun = 100000;

        GraphEvent ToGraphEvent(const ContactEvent event)
        {
            switch (event)
            {
                case ContactEvent::SensorEnter:    return GraphEvent::OnSensorEnter;
                case ContactEvent::SensorExit:     return GraphEvent::OnSensorExit;
                case ContactEvent::CollisionEnter: return GraphEvent::OnCollisionEnter;
                case ContactEvent::CollisionExit:  return GraphEvent::OnCollisionExit;
            }
            return GraphEvent::OnCollisionExit;
        }
    }

    GraphScriptInstance::GraphScriptInstance(Ref<const GraphProgram> program, const Entity entity)
        : m_Program(std::move(program)), m_Scene(&entity.GetScene()), m_Slots(m_Program->InitialSlots),
          m_DelayRunning(m_Program->DelayCount, false)
    {
        m_Slots[GraphProgram::SelfSlot] = EntityRef{ entity.GetUUID() };
    }

    void GraphScriptInstance::OnCreate() { Fire(GraphEvent::OnCreate, std::nullopt); }

    void GraphScriptInstance::OnDestroy()
    {
        m_Delays.clear();
        Fire(GraphEvent::OnDestroy, std::nullopt);
    }

    void GraphScriptInstance::OnUpdate(const Timestep ts)
    {
        Fire(GraphEvent::OnUpdate, ts.GetSeconds());
        AdvanceDelays(ts.GetSeconds());
    }

    void GraphScriptInstance::OnContact(const ContactEvent event, const Entity other)
    {
        Fire(ToGraphEvent(event), EntityRef{ other.GetUUID() });
    }

    std::optional<ScriptValue> GraphScriptInstance::GetField(const std::string_view name) const
    {
        const auto it = m_Program->VariableSlots.find(name);
        if (it == m_Program->VariableSlots.end())
            return std::nullopt;

        return m_Slots[it->second];
    }

    bool GraphScriptInstance::SetField(const std::string_view name, const ScriptValue& value)
    {
        const auto it = m_Program->VariableSlots.find(name);
        if (it == m_Program->VariableSlots.end())
            return false;

        ScriptValue& slot = m_Slots[it->second];
        if (PinType::Of(slot) != PinType::Of(value))
            return false;

        slot = value;
        return true;
    }

    void GraphScriptInstance::Fire(const GraphEvent event, std::optional<ScriptValue> payload)
    {
        const std::optional<GraphEntry>& entry = m_Program->Events[static_cast<size_t>(event)];
        if (!entry) return;

        if (payload && entry->PayloadSlot != NoSlot)
            m_Slots[entry->PayloadSlot] = std::move(*payload);

        Run(entry->Instruction);
    }

    void GraphScriptInstance::Run(uint32_t instruction)
    {
        const GraphProgram& program = *m_Program;
        const ScriptCallContext context{ .ActiveScene = m_Scene };

        for (size_t budget = MaxInstructionsPerRun; budget > 0; --budget)
        {
            if (instruction >= program.Code.size())
                throw ScriptError("The graph jumped outside its code");

            const GraphInstruction& current = program.Code[instruction++];

            try
            {
                switch (current.Op)
                {
                    case GraphOp::Call:
                    {
                        std::vector<ScriptValue> arguments;
                        arguments.reserve(current.C);
                        for (uint32_t i = 0; i < current.C; ++i)
                            arguments.push_back(m_Slots[program.Arguments[current.B + i]]);

                        std::optional<ScriptValue> result = ScriptAPI::Get().GetFunction(current.A).Invoke(context, arguments);
                        if (result && current.D != NoSlot)
                            m_Slots[current.D] = std::move(*result);
                        break;
                    }
                    case GraphOp::Copy:
                        m_Slots[current.B] = m_Slots[current.A];
                        break;
                    case GraphOp::IntToFloat:
                        m_Slots[current.B] = static_cast<float>(std::get<int>(m_Slots[current.A]));
                        break;
                    case GraphOp::Jump:
                        instruction = current.A;
                        break;
                    case GraphOp::JumpIfFalse:
                        if (!std::get<bool>(m_Slots[current.A]))
                            instruction = current.B;
                        break;
                    case GraphOp::Delay:
                        StartDelay(current.A, std::get<float>(m_Slots[current.B]), instruction);
                        return;
                    case GraphOp::Return:
                        return;
                }
            } catch (const std::exception& e)
            {
                throw ScriptError(std::format("{} (in '{}')", e.what(), program.Labels[current.Label]));
            }
        }

        throw ScriptError("The graph ran too many steps at once; is there an exec loop without a Delay?");
    }

    void GraphScriptInstance::StartDelay(const uint32_t id, const float duration, const uint32_t resume)
    {
        if (m_DelayRunning[id]) return;

        m_DelayRunning[id] = true;
        m_Delays.push_back({ id, std::max(duration, 0.0f), resume });
    }

    void GraphScriptInstance::AdvanceDelays(const float deltaTime)
    {
        std::vector<PendingDelay> finished;
        for (PendingDelay& delay : m_Delays)
        {
            delay.Remaining -= deltaTime;
            if (delay.Remaining <= 0.0f)
                finished.push_back(delay);
        }

        std::erase_if(m_Delays, [](const PendingDelay& delay) { return delay.Remaining <= 0.0f; });

        for (const PendingDelay& delay : finished)
        {
            m_DelayRunning[delay.Id] = false;
            Run(delay.Resume);
        }
    }
}
