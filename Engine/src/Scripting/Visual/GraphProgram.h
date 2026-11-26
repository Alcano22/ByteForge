#pragma once

#include "Engine/Scripting/ScriptField.h"
#include "Engine/Scripting/Visual/ScriptGraph.h"

#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ByteForge
{
    inline constexpr u32 NoSlot = std::numeric_limits<u32>::max();

    enum class GraphOp : u8
    {
        Call,             // A: function index, B: first entry in Arguments, C: argument count, D: result slot or NoSlot
        Copy,             // A: source slot, B: target slot
        IntToFloat,       // A: source slot, B: target slot
        Jump,             // A: target instruction
        JumpIfFalse,      // A: condition slot, B: target instruction
        Trace,            // A: index into TracedLinks
        Delay,            // A: delay id, B: duration slot
        Gosub,            // A: target instruction
        JumpIfGreaterInt, // A: Int slot, B: Int slot, C: target instruction if A > B
        IncrementInt,     // A: Int slot
        Return
    };

    struct GraphInstruction
    {
        GraphOp Op = GraphOp::Return;
        u32 A = 0;
        u32 B = 0;
        u32 C = 0;
        u32 D = 0;
        u32 Label = 0;
    };

    struct GraphLabel
    {
        std::string Title;
        UUID Node{ 0 };
    };

    struct GraphEntry
    {
        u32 Instruction = 0;
        u32 PayloadSlot = NoSlot;
    };

    struct GraphProgram
    {
        static constexpr u32 SelfSlot = 0;

        UUID Graph{ 0 };

        std::vector<GraphInstruction> Code;
        std::vector<u32> Arguments;
        std::vector<ScriptValue> InitialSlots;
        std::array<std::optional<GraphEntry>, GraphEventCount> Events;

        std::map<std::string, u32, std::less<>> VariableSlots;
        std::vector<ScriptFieldInfo> Fields;

        u32 DelayCount = 0;
        std::vector<GraphLabel> Labels{ GraphLabel{ "Graph" } };
        std::vector<GraphLink> TracedLinks;
        std::map<std::pair<u64, std::string>, u32> PinSlots;
    };
}
