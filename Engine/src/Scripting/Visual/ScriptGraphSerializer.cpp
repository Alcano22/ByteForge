#include "Engine/Scripting/Visual/ScriptGraphSerializer.h"
#include "Engine/Scripting/API/ScriptAPI.h"
#include "Scripting/ScriptValueJson.h"
#include "Engine/Core/FileSystem.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ByteForge
{
    namespace
    {
        template<typename T> constexpr std::string_view NodeKindName = {};
        template<> constexpr std::string_view NodeKindName<EventNode>       = "Event";
        template<> constexpr std::string_view NodeKindName<CallNode>        = "Call";
        template<> constexpr std::string_view NodeKindName<BranchNode>      = "Branch";
        template<> constexpr std::string_view NodeKindName<DelayNode>       = "Delay";
        template<> constexpr std::string_view NodeKindName<GetVariableNode> = "GetVariable";
        template<> constexpr std::string_view NodeKindName<SetVariableNode> = "SetVariable";
        template<> constexpr std::string_view NodeKindName<LiteralNode>     = "Literal";
        template<> constexpr std::string_view NodeKindName<SequenceNode>    = "Sequence";
        template<> constexpr std::string_view NodeKindName<ForLoopNode>     = "ForLoop";
        template<> constexpr std::string_view NodeKindName<DoOnceNode>      = "DoOnce";
        template<> constexpr std::string_view NodeKindName<FlipFlopNode>    = "FlipFlop";
        template<> constexpr std::string_view NodeKindName<GateNode>        = "Gate";
        template<> constexpr std::string_view NodeKindName<CommentNode>     = "Comment";
        template<> constexpr std::string_view NodeKindName<RerouteNode>     = "Reroute";
        template<> constexpr std::string_view NodeKindName<SelectNode>      = "Select";
        template<> constexpr std::string_view NodeKindName<CustomEventNode> = "CustomEvent";
        template<> constexpr std::string_view NodeKindName<CallEventNode>   = "CallEvent";

        nlohmann::json WritePinRef(const PinRef& pin)
        {
            return { { "node", static_cast<u64>(pin.Node) }, { "pin", pin.Pin } };
        }

        PinRef ReadPinRef(const nlohmann::json& json)
        {
            return { UUID(json.at("node").get<u64>()), json.at("pin").get<std::string>() };
        }

        nlohmann::json WritePinType(const PinType& type)
        {
            nlohmann::json json{ { "exec", type.IsExec } };
            if (type.IsExec)
                return json;

            json["value"] = std::string(magic_enum::enum_name(type.Value));
            if (type.Value == ScriptFieldType::Asset)
                json["asset"] = std::string(magic_enum::enum_name(type.Asset));
            if (!type.Enum.empty())
                json["enum"] = std::string(type.Enum);
            return json;
        }

        std::optional<PinType> ReadPinType(const nlohmann::json& node)
        {
            const auto json = node.find("type");
            if (json == node.end() || !json->is_object())
                return std::nullopt;

            if (json->value("exec", false))
                return PinType::Exec();

            if (const auto name = json->find("enum"); name != json->end() && name->is_string())
            {
                const ScriptEnum* scriptEnum = ScriptAPI::Get().FindEnum(name->get<std::string>());
                return scriptEnum != nullptr ? std::optional(PinType::OfEnum(scriptEnum->Name)) : std::nullopt;
            }

            const auto value = magic_enum::enum_cast<ScriptFieldType>(json->value("value", std::string()));
            if (!value)
                return std::nullopt;

            const auto asset = magic_enum::enum_cast<AssetType>(json->value("asset", std::string("None")));
            return PinType::Of(*value, asset.value_or(AssetType::None));
        }

        nlohmann::json WriteNode(const GraphNode& node)
        {
            nlohmann::json json{
                { "id",       static_cast<u64>(node.Id)       },
                { "position", JsonValue::ToJson(node.Position)     }
            };

            std::visit([&json]<typename T>(const T& data)
            {
                json["kind"] = std::string(NodeKindName<T>);

                if constexpr (std::is_same_v<T, EventNode>)
                    json["event"] = std::string(magic_enum::enum_name(data.Event));
                else if constexpr (std::is_same_v<T, CallNode>)
                    json["function"] = data.Function;
                else if constexpr (std::is_same_v<T, GetVariableNode> || std::is_same_v<T, SetVariableNode>)
                    json["variable"] = static_cast<u64>(data.Variable);
                else if constexpr (std::is_same_v<T, CustomEventNode> || std::is_same_v<T, CallEventNode>)
                    json["event"] = static_cast<u64>(data.Event);
                else if constexpr (std::is_same_v<T, LiteralNode>)
                    json["value"] = JsonValue::TypedValueToJson(data.Value);
                else if constexpr (std::is_same_v<T, SequenceNode>)
                    json["outputs"] = data.Outputs;
                else if constexpr (std::is_same_v<T, CommentNode>)
                {
                    json["text"] = data.Text;
                    json["size"] = JsonValue::ToJson(data.Size);
                } else if constexpr (std::is_same_v<T, RerouteNode> || std::is_same_v<T, SelectNode>)
                {
                    if (data.Type)
                        json["type"] = WritePinType(*data.Type);
                }
            }, node.Data);

            if (!node.Defaults.empty())
            {
                nlohmann::json defaults = nlohmann::json::object();
                for (const auto& [pin, value] : node.Defaults)
                    defaults[pin] = JsonValue::TypedValueToJson(value);
                json["defaults"] = std::move(defaults);
            }

            return json;
        }

        NodeData ReadNodeData(const nlohmann::json& json)
        {
            const std::string kind = json.at("kind").get<std::string>();

            if (kind == NodeKindName<EventNode>)
            {
                const std::string name = json.at("event").get<std::string>();
                const auto event = magic_enum::enum_cast<GraphEvent>(name);
                if (!event)
                    throw Error("unknown event '{}'", name);
                return EventNode{ *event };
            }
            if (kind == NodeKindName<CallNode>)
                return CallNode{ json.at("function").get<std::string>() };
            if (kind == NodeKindName<BranchNode>)
                return BranchNode{};
            if (kind == NodeKindName<DelayNode>)
                return DelayNode{};
            if (kind == NodeKindName<GetVariableNode>)
                return GetVariableNode{ UUID(json.at("variable").get<u64>()) };
            if (kind == NodeKindName<SetVariableNode>)
                return SetVariableNode{ UUID(json.at("variable").get<u64>()) };
            if (kind == NodeKindName<LiteralNode>)
            {
                std::optional<ScriptValue> value = JsonValue::ReadScriptValue(json.at("value"));
                if (!value)
                    throw Error("literal node has an unreadable value");
                return LiteralNode{ std::move(*value) };
            }
            if (kind == NodeKindName<SequenceNode>)
            {
                const u32 outputs = json.value("outputs", MinSequenceOutputs);
                return SequenceNode{ std::clamp(outputs, MinSequenceOutputs, MaxSequenceOutputs) };
            }
            if (kind == NodeKindName<ForLoopNode>)
                return ForLoopNode{};
            if (kind == NodeKindName<DoOnceNode>)
                return DoOnceNode{};
            if (kind == NodeKindName<FlipFlopNode>)
                return FlipFlopNode{};
            if (kind == NodeKindName<GateNode>)
                return GateNode{};
            if (kind == NodeKindName<CommentNode>)
            {
                CommentNode comment{ .Text = json.value("text", std::string("Comment")) };
                if (json.contains("size"))
                    comment.Size = JsonValue::ToVec2(json.at("size"));
                return comment;
            }
            if (kind == NodeKindName<RerouteNode>)
                return RerouteNode{ ReadPinType(json) };
            if (kind == NodeKindName<SelectNode>)
                return SelectNode{ ReadPinType(json) };
            if (kind == NodeKindName<CustomEventNode>)
                return CustomEventNode{ UUID(json.at("event").get<u64>()) };
            if (kind == NodeKindName<CallEventNode>)
                return CallEventNode{ UUID(json.at("event").get<u64>()) };

            throw Error("unknown node kind '{}'", kind);
        }

        GraphNode ReadNode(const nlohmann::json& json)
        {
            GraphNode node{
                .Id       = UUID(json.at("id").get<u64>()),
                .Data     = ReadNodeData(json),
                .Position = JsonValue::ToVec2(json.at("position"))
            };

            if (json.contains("defaults"))
            {
                for (const auto& [pin, valueJson] : json.at("defaults").items())
                {
                    if (std::optional<ScriptValue> value = JsonValue::ReadScriptValue(valueJson))
                        node.Defaults.insert_or_assign(pin, std::move(*value));
                }
            }

            return node;
        }

        nlohmann::json WriteVariable(const GraphVariable& variable)
        {
            return {
                { "id",      static_cast<u64>(variable.Id)                 },
                { "name",    variable.Name                                 },
                { "exposed", variable.Exposed                              },
                { "default", JsonValue::TypedValueToJson(variable.Default) }
            };
        }

        GraphVariable ReadVariable(const nlohmann::json& json)
        {
            std::string name = json.at("name").get<std::string>();

            std::optional<ScriptValue> value = JsonValue::ReadScriptValue(json.at("default"));
            if (!value)
                throw Error("variable '{}' has an unreadable default value", name);

            return {
                .Id      = UUID(json.at("id").get<u64>()),
                .Name    = std::move(name),
                .Default = std::move(*value),
                .Exposed = json.value("exposed", true)
            };
        }

        nlohmann::json WriteEvent(const GraphCustomEvent& event)
        {
            nlohmann::json parameters = nlohmann::json::array();
            for (const GraphParameter& parameter : event.Parameters)
            {
                parameters.push_back({
                    { "id",      static_cast<u64>(parameter.Id)                 },
                    { "name",    parameter.Name                                 },
                    { "default", JsonValue::TypedValueToJson(parameter.Default) }
                });
            }

            return {
                { "id",         static_cast<u64>(event.Id) },
                { "name",       event.Name                 },
                { "parameters", std::move(parameters)      }
            };
        }

        GraphCustomEvent ReadEvent(const nlohmann::json& json)
        {
            GraphCustomEvent event{
                .Id   = UUID(json.at("id").get<u64>()),
                .Name = json.at("name").get<std::string>()
            };

            for (const auto& parameterJson : json.value("parameters", nlohmann::json::array()))
            {
                std::string name = parameterJson.at("name").get<std::string>();

                std::optional<ScriptValue> value = JsonValue::ReadScriptValue(parameterJson.at("default"));
                if (!value)
                    throw Error("parameter '{}' of event '{}' has an unreadable type", name, event.Name);

                event.Parameters.push_back({
                    .Id      = UUID(parameterJson.at("id").get<u64>()),
                    .Name    = std::move(name),
                    .Default = std::move(*value)
                });
            }

            return event;
        }
    }

    nlohmann::json SerializeScriptGraph(const ScriptGraph& graph)
    {
        nlohmann::json variables = nlohmann::json::array();
        for (const GraphVariable& variable : graph.GetVariables())
            variables.push_back(WriteVariable(variable));

        nlohmann::json events = nlohmann::json::array();
        for (const GraphCustomEvent& event : graph.GetEvents())
            events.push_back(WriteEvent(event));

        nlohmann::json nodes = nlohmann::json::array();
        for (const GraphNode& node : graph.GetNodes())
            nodes.push_back(WriteNode(node));

        nlohmann::json links = nlohmann::json::array();
        for (const GraphLink& link : graph.GetLinks())
            links.push_back({ { "from", WritePinRef(link.From) }, { "to", WritePinRef(link.To) } });

        return {
            { "version",   ScriptGraph::FormatVersion },
            { "variables", std::move(variables)       },
            { "events",    std::move(events)          },
            { "nodes",     std::move(nodes)           },
            { "links",     std::move(links)           }
        };
    }

    Result<ScriptGraph> DeserializeScriptGraph(const nlohmann::json& data)
    {
        try
        {
            if (!data.is_object())
                return Unexpected("expected a JSON object");

            const i32 version = data.value("version", 0);
            if (version < 1)
                return Unexpected("the format version is missing");
            if (version > ScriptGraph::FormatVersion)
            {
                return Unexpected("format version {} is newer than this version of ByteForge supports ({})",
                                  version, ScriptGraph::FormatVersion);
            }

            std::vector<GraphVariable> variables;
            for (const auto& json : data.value("variables", nlohmann::json::array()))
                variables.push_back(ReadVariable(json));

            std::vector<GraphCustomEvent> events;
            for (const auto& json : data.value("events", nlohmann::json::array()))
                events.push_back(ReadEvent(json));

            std::vector<GraphNode> nodes;
            for (const auto& json : data.value("nodes", nlohmann::json::array()))
                nodes.push_back(ReadNode(json));

            std::vector<GraphLink> links;
            for (const auto& json : data.value("links", nlohmann::json::array()))
                links.push_back({ ReadPinRef(json.at("from")), ReadPinRef(json.at("to")) });

            return ScriptGraph::FromParts(std::move(nodes), std::move(links), std::move(variables), std::move(events));
        } catch (const std::exception& e)
        {
            return Unexpected(e.what());
        }
    }

    Result<ScriptGraph> LoadScriptGraph(const fs::path& path)
    {
        const Result<nlohmann::json> data = ReadJsonFile(path);
        if (!data)
            return Unexpected(data.error());

        try
        {
            return DeserializeScriptGraph(*data);
        } catch (const nlohmann::json::exception& e)
        {
            return Unexpected(e.what());
        }
    }

    Result<> SaveScriptGraph(const ScriptGraph& graph, const fs::path& path)
    {
        return WriteJsonFile(path, SerializeScriptGraph(graph));
    }
}
