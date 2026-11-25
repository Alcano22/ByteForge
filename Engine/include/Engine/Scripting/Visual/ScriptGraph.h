#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/AssetType.h"
#include "Engine/Scene/UUID.h"
#include "Engine/Scripting/ScriptField.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <expected>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* ScriptGraphExtension = ".bfgraph";

    enum class PinDirection : u8 { Input, Output };

    struct BYTEFORGE_API PinType
    {
        bool IsExec = false;
        ScriptFieldType Value = ScriptFieldType::Float;
        AssetType Asset = AssetType::None;
        std::string_view Enum;

        [[nodiscard]] static constexpr PinType Exec() { return { .IsExec = true }; }

        [[nodiscard]] static constexpr PinType OfEnum(const std::string_view name)
        {
            return { .IsExec = false, .Value = ScriptFieldType::Int, .Asset = AssetType::None, .Enum = name };
        }

        [[nodiscard]] static constexpr PinType Of(const ScriptFieldType value,
                                                  const AssetType asset = AssetType::None)
        {
            return { .IsExec = false, .Value = value, .Asset = asset };
        }

        [[nodiscard]] static PinType Of(const ScriptValue& value);

        [[nodiscard]] bool operator==(const PinType&) const = default;
    };

    struct PinInfo
    {
        std::string Name;
        PinDirection Direction = PinDirection::Input;
        PinType Type;
    };

    enum class GraphEvent : u8
    {
        OnCreate,
        OnUpdate,
        OnDestroy,
        OnSensorEnter,
        OnSensorExit,
        OnCollisionEnter,
        OnCollisionExit,
        OnClick
    };
    inline constexpr usize GraphEventCount = static_cast<usize>(GraphEvent::OnClick) + 1;

    struct EventNode { GraphEvent Event = GraphEvent::OnUpdate; };
    struct CallNode { std::string Function; };
    struct BranchNode {};
    struct DelayNode {};
    struct GetVariableNode { UUID Variable{ 0 }; };
    struct SetVariableNode { UUID Variable{ 0 }; };
    struct LiteralNode { ScriptValue Value = 0.0f; };

    inline constexpr u32 MinSequenceOutputs = 2;
    inline constexpr u32 MaxSequenceOutputs = 16;

    struct SequenceNode { u32 Outputs = MinSequenceOutputs; };
    struct ForLoopNode {};
    struct DoOnceNode {};
    struct FlipFlopNode {};
    struct GateNode {};

    using NodeData = std::variant<EventNode, CallNode, BranchNode, DelayNode, GetVariableNode, SetVariableNode,
                                  LiteralNode, SequenceNode, ForLoopNode, DoOnceNode, FlipFlopNode, GateNode>;

    struct GraphNode
    {
        UUID Id;
        NodeData Data;
        glm::vec2 Position{ 0.0f };

        std::map<std::string, ScriptValue, std::less<>> Defaults;
    };

    struct PinRef
    {
        UUID Node{ 0 };
        std::string Pin;

        [[nodiscard]] bool operator==(const PinRef&) const = default;
    };

    struct GraphLink
    {
        PinRef From;
        PinRef To;
    };

    struct GraphVariable
    {
        UUID Id;
        std::string Name;
        ScriptValue Default = 0.0f;
        bool Exposed = true;
    };

    class BYTEFORGE_API ScriptGraph
    {
    public:
        [[nodiscard]] static ScriptGraph FromParts(std::vector<GraphNode> nodes, std::vector<GraphLink> links,
                                                   std::vector<GraphVariable> variables);

        [[nodiscard]] std::span<const GraphNode> GetNodes() const { return m_Nodes; }
        [[nodiscard]] std::span<const GraphLink> GetLinks() const { return m_Links; }
        [[nodiscard]] std::span<const GraphVariable> GetVariables() const { return m_Variables; }

        [[nodiscard]] const GraphNode* FindNode(UUID id) const;
        [[nodiscard]] GraphNode* FindNode(UUID id);
        [[nodiscard]] const GraphVariable* FindVariable(UUID id) const;
        [[nodiscard]] const GraphVariable* FindVariable(std::string_view name) const;

        UUID AddNode(NodeData data, glm::vec2 position);
        void RemoveNode(UUID id);

        Result<> Connect(const PinRef& from, const PinRef& to);
        void Disconnect(const PinRef& pin, PinDirection direction);

        void RemoveLink(const GraphLink& link);

        Result<UUID> AddVariable(std::string name, ScriptValue defaultValue);
        Result<> RenameVariable(UUID id, std::string name);
        void RemoveVariable(UUID id);

        Result<> SetVariableDefault(UUID id, ScriptValue value);
        Result<> SetVariableExposed(UUID id, bool exposed);

    private:
        [[nodiscard]] Result<> CheckVariableName(std::string_view name, UUID ignore) const;

        [[nodiscard]] GraphVariable* FindVariableForEdit(UUID id);
        void DropMismatchedVariableUses(UUID variable);

    public:
        static constexpr i32 FormatVersion = 1;

    private:
        std::vector<GraphNode> m_Nodes;
        std::vector<GraphLink> m_Links;
        std::vector<GraphVariable> m_Variables;
    };

    [[nodiscard]] BYTEFORGE_API std::string ScriptGraphClassName(UUID graphAsset);
    [[nodiscard]] BYTEFORGE_API bool UsesVariable(const GraphNode& node, UUID variable);
}
