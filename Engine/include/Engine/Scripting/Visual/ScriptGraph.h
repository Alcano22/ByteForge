#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/AssetType.h"
#include "Engine/Scene/UUID.h"
#include "Engine/Scripting/ScriptField.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* ScriptGraphExtension = ".bfgraph";

    enum class PinDirection : u8 { Input, Output };

    enum class WildcardKind : u8 { None, Data, Any };

    struct BYTEFORGE_API PinType
    {
        bool IsExec = false;
        ScriptFieldType Value = ScriptFieldType::Float;
        AssetType Asset = AssetType::None;
        std::string_view Enum;
        WildcardKind Wildcard = WildcardKind::None;

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

        [[nodiscard]] static constexpr PinType Any() { return { .Wildcard = WildcardKind::Any }; }
        [[nodiscard]] static constexpr PinType AnyData() { return { .Wildcard = WildcardKind::Data }; }

        [[nodiscard]] constexpr bool IsWildcard() const { return Wildcard != WildcardKind::None; }

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

    struct CommentNode
    {
        std::string Text = "Comment";
        glm::vec2 Size{ 400.0f, 200.0f };
    };

    struct RerouteNode { std::optional<PinType> Type; };
    struct SelectNode { std::optional<PinType> Type; };
    struct CustomEventNode { UUID Event{ 0 }; };
    struct CallEventNode { UUID Event{ 0 }; };
    struct FunctionEntryNode { UUID Function{ 0 }; };
    struct FunctionReturnNode { UUID Function{ 0 }; };
    struct CallFunctionNode { UUID Function{ 0 }; };

    using NodeData = std::variant<EventNode, CallNode, BranchNode, DelayNode, GetVariableNode, SetVariableNode,
                                  LiteralNode, SequenceNode, ForLoopNode, DoOnceNode, FlipFlopNode, GateNode,
                                  CommentNode, RerouteNode, SelectNode, CustomEventNode, CallEventNode,
                                  FunctionEntryNode, FunctionReturnNode, CallFunctionNode>;

    struct GraphNode
    {
        UUID Id;
        NodeData Data;
        glm::vec2 Position{ 0.0f };

        std::map<std::string, ScriptValue, std::less<>> Defaults;
        UUID Scope{ 0 };
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
        UUID Scope{ 0 };
    };

    [[nodiscard]] inline bool IsVisibleIn(const GraphVariable& variable, const UUID scope)
    {
        return variable.Scope == UUID(0) || variable.Scope == scope;
    }

    struct GraphParameter
    {
        UUID Id;
        std::string Name;
        ScriptValue Default = 0.0f;
    };

    struct GraphCustomEvent
    {
        UUID Id;
        std::string Name;
        std::vector<GraphParameter> Parameters;
    };

    struct GraphFunction
    {
        UUID Id;
        std::string Name;
        std::vector<GraphParameter> Inputs;
        std::vector<GraphParameter> Outputs;
        bool Pure = false;
    };

    struct GraphPinChanges
    {
        std::set<std::string, std::less<>> Dropped;
        std::map<std::string, std::string, std::less<>> Renamed;
    };

    class BYTEFORGE_API ScriptGraph
    {
    public:
        [[nodiscard]] static ScriptGraph FromParts(std::vector<GraphNode> nodes, std::vector<GraphLink> links,
                                                   std::vector<GraphVariable> variables,
                                                   std::vector<GraphCustomEvent> events = {},
                                                   std::vector<GraphFunction> functions = {});

        [[nodiscard]] std::span<const GraphNode> GetNodes() const { return m_Nodes; }
        [[nodiscard]] std::span<const GraphLink> GetLinks() const { return m_Links; }
        [[nodiscard]] std::span<const GraphVariable> GetVariables() const { return m_Variables; }
        [[nodiscard]] std::span<const GraphCustomEvent> GetEvents() const { return m_Events; }
        [[nodiscard]] std::span<const GraphFunction> GetFunctions() const { return m_Functions; }

        [[nodiscard]] const GraphNode* FindNode(UUID id) const;
        [[nodiscard]] GraphNode* FindNode(UUID id);
        [[nodiscard]] const GraphVariable* FindVariable(UUID id) const;
        [[nodiscard]] const GraphVariable* FindVariable(std::string_view name) const;
        [[nodiscard]] const GraphCustomEvent* FindEvent(UUID id) const;
        [[nodiscard]] const GraphCustomEvent* FindEvent(std::string_view name) const;
        [[nodiscard]] const GraphFunction* FindFunction(UUID id) const;
        [[nodiscard]] const GraphFunction* FindFunction(std::string_view name) const;

        UUID AddNode(NodeData data, glm::vec2 position, UUID scope = UUID(0));
        void RemoveNode(UUID id);

        Result<> Connect(const PinRef& from, const PinRef& to);
        void Disconnect(const PinRef& pin, PinDirection direction);

        void RemoveLink(const GraphLink& link);

        Result<UUID> AddVariable(std::string name, ScriptValue defaultValue, UUID scope = UUID(0));
        Result<> RenameVariable(UUID id, std::string name);
        void RemoveVariable(UUID id);

        Result<> SetVariableDefault(UUID id, ScriptValue value);
        Result<> SetVariableExposed(UUID id, bool exposed);

        Result<UUID> AddEvent(std::string name);
        Result<> UpdateEvent(GraphCustomEvent event);
        void RemoveEvent(UUID id);

        Result<UUID> AddFunction(std::string name);
        Result<> UpdateFunction(GraphFunction function);
        void RemoveFunction(UUID id);

    private:
        [[nodiscard]] Result<> CheckVariableName(std::string_view name, UUID ignore, UUID scope) const;
        [[nodiscard]] Result<> CheckEventName(std::string_view name, UUID ignore) const;
        [[nodiscard]] Result<> CheckFunctionName(std::string_view name, UUID ignore) const;

        [[nodiscard]] GraphVariable* FindVariableForEdit(UUID id);
        void DropMismatchedVariableUses(UUID variable);

        std::vector<UUID> EraseLinks(const std::function<bool(const GraphLink&)>& matches);
        void AssignWildcardType(const PinRef& pin, const PinType& type);
        void ResetUnlinkedWildcards(std::span<const UUID> nodes);

        void ApplyPinChanges(const std::set<u64>& sources, const std::set<u64>& targets,
                             const GraphPinChanges& changes);

    public:
        static constexpr i32 FormatVersion = 1;

    private:
        std::vector<GraphNode> m_Nodes;
        std::vector<GraphLink> m_Links;
        std::vector<GraphVariable> m_Variables;
        std::vector<GraphCustomEvent> m_Events;
        std::vector<GraphFunction> m_Functions;
    };

    [[nodiscard]] BYTEFORGE_API std::string ScriptGraphClassName(UUID graphAsset);

    [[nodiscard]] BYTEFORGE_API bool UsesVariable(const GraphNode& node, UUID variable);
    [[nodiscard]] BYTEFORGE_API bool UsesEvent(const GraphNode& node, UUID event);

    [[nodiscard]] BYTEFORGE_API bool HasEventEntry(const ScriptGraph& graph, UUID event);
    [[nodiscard]] BYTEFORGE_API bool HasFunctionEntry(const ScriptGraph& graph, UUID function);
}
