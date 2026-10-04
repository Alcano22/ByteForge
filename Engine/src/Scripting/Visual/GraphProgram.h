#pragma once

#include "Engine/Scripting/ScriptField.h"
#include "Engine/Scripting/Visual/ScriptGraph.h"

#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    inline constexpr uint32_t NoSlot = std::numeric_limits<uint32_t>::max();

    enum class GraphOp : uint8_t
    {
        Call,        // A: function index, B: first entry in Arguments, C: argument count, D: result slot or NoSlot
        Copy,        // A: source slot, B: target slot
        IntToFloat,  // A: source slot, B: target slot
        Jump,        // A: target instruction
        JumpIfFalse, // A: condition slot, B: target instruction
        Delay,       // A: delay id, B: duration slot; resumes at the next instruction
        Return
    };

    struct GraphInstruction
    {
        GraphOp Op = GraphOp::Return;
        uint32_t A = 0;
        uint32_t B = 0;
        uint32_t C = 0;
        uint32_t D = 0;
        uint32_t Label = 0;
    };

    struct GraphEntry
    {
        uint32_t Instruction = 0;
        uint32_t PayloadSlot = NoSlot;
    };

    struct GraphProgram
    {
        static constexpr uint32_t SelfSlot = 0;

        std::vector<GraphInstruction> Code;
        std::vector<uint32_t> Arguments;
        std::vector<ScriptValue> InitialSlots;
        std::array<std::optional<GraphEntry>, GraphEventCount> Events;

        std::map<std::string, uint32_t, std::less<>> VariableSlots;
        std::vector<ScriptFieldInfo> Fields;

        uint32_t DelayCount = 0;
        std::vector<std::string> Labels{ "Graph" };
    };
}
