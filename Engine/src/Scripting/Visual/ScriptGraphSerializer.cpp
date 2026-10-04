#include "Engine/Scripting/Visual/ScriptGraphSerializer.h"
#include "Scripting/ScriptValueJson.h"

#include <magic_enum/magic_enum.hpp>

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

        nlohmann::json WritePinRef(const PinRef& pin)
        {
            return { { "node", static_cast<uint64_t>(pin.Node) }, { "pin", pin.Pin } };
        }

        PinRef ReadPinRef(const nlohmann::json& json)
        {
            return { UUID(json.at("node").get<uint64_t>()), json.at("pin").get<std::string>() };
        }

        nlohmann::json WriteNode(const GraphNode& node)
        {
            nlohmann::json json{
                { "id",       static_cast<uint64_t>(node.Id)       },
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
                    json["variable"] = static_cast<uint64_t>(data.Variable);
                else if constexpr (std::is_same_v<T, LiteralNode>)
                    json["value"] = JsonValue::TypedValueToJson(data.Value);
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
                    throw std::runtime_error(std::format("unknown event '{}'", name));
                return EventNode{ *event };
            }
            if (kind == NodeKindName<CallNode>)
                return CallNode{ json.at("function").get<std::string>() };
            if (kind == NodeKindName<BranchNode>)
                return BranchNode{};
            if (kind == NodeKindName<DelayNode>)
                return DelayNode{};
            if (kind == NodeKindName<GetVariableNode>)
                return GetVariableNode{ UUID(json.at("variable").get<uint64_t>()) };
            if (kind == NodeKindName<SetVariableNode>)
                return SetVariableNode{ UUID(json.at("variable").get<uint64_t>()) };
            if (kind == NodeKindName<LiteralNode>)
            {
                std::optional<ScriptValue> value = JsonValue::ReadScriptValue(json.at("value"));
                if (!value)
                    throw std::runtime_error("literal node has an unreadable value");
                return LiteralNode{ std::move(*value) };
            }

            throw std::runtime_error(std::format("unknown node kind '{}'", kind));
        }

        GraphNode ReadNode(const nlohmann::json& json)
        {
            GraphNode node{
                .Id       = UUID(json.at("id").get<uint64_t>()),
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
                { "id",      static_cast<uint64_t>(variable.Id)              },
                { "name",    variable.Name                                   },
                { "exposed", variable.Exposed                                },
                { "default", JsonValue::TypedValueToJson(variable.Default)   }
            };
        }

        GraphVariable ReadVariable(const nlohmann::json& json)
        {
            std::string name = json.at("name").get<std::string>();

            std::optional<ScriptValue> value = JsonValue::ReadScriptValue(json.at("default"));
            if (!value)
                throw std::runtime_error(std::format("variable '{}' has an unreadable default value", name));

            return {
                .Id      = UUID(json.at("id").get<uint64_t>()),
                .Name    = std::move(name),
                .Default = std::move(*value),
                .Exposed = json.value("exposed", true)
            };
        }
    }

    nlohmann::json SerializeScriptGraph(const ScriptGraph& graph)
    {
        nlohmann::json variables = nlohmann::json::array();
        for (const GraphVariable& variable : graph.GetVariables())
            variables.push_back(WriteVariable(variable));

        nlohmann::json nodes = nlohmann::json::array();
        for (const GraphNode& node : graph.GetNodes())
            nodes.push_back(WriteNode(node));

        nlohmann::json links = nlohmann::json::array();
        for (const GraphLink& link : graph.GetLinks())
            links.push_back({ { "from", WritePinRef(link.From) }, { "to", WritePinRef(link.To) } });

        return {
                { "version",   ScriptGraph::FormatVersion },
                { "variables", std::move(variables)       },
                { "nodes",     std::move(nodes)           },
                { "links",     std::move(links)           }
        };
    }

    std::expected<ScriptGraph, std::string> DeserializeScriptGraph(const nlohmann::json& data)
    {
        try
        {
            if (!data.is_object())
                return std::unexpected("expected a JSON object");

            const int version = data.value("version", 0);
            if (version < 1)
                return std::unexpected("the format version is missing");
            if (version > ScriptGraph::FormatVersion)
                return std::unexpected(
                    std::format("format version {} is newer than this version of ByteForge supports ({})",
                                version, ScriptGraph::FormatVersion));

            std::vector<GraphVariable> variables;
            for (const auto& json : data.value("variables", nlohmann::json::array()))
                variables.push_back(ReadVariable(json));

            std::vector<GraphNode> nodes;
            for (const auto& json : data.value("nodes", nlohmann::json::array()))
                nodes.push_back(ReadNode(json));

            std::vector<GraphLink> links;
            for (const auto& json : data.value("links", nlohmann::json::array()))
                links.push_back({ ReadPinRef(json.at("from")), ReadPinRef(json.at("to")) });

            return ScriptGraph::FromParts(std::move(nodes), std::move(links), std::move(variables));
        } catch (const std::exception& e)
        {
            return std::unexpected(e.what());
        }
    }

    std::expected<ScriptGraph, std::string> LoadScriptGraph(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
            return std::unexpected("the file cannot be opened");

        try
        {
            nlohmann::json data;
            file >> data;
            return DeserializeScriptGraph(data);
        } catch (const nlohmann::json::exception& e)
        {
            return std::unexpected(e.what());
        }
    }

    std::expected<void, std::string> SaveScriptGraph(const ScriptGraph& graph, const std::filesystem::path& path)
    {
        std::ofstream file(path);
        if (!file.is_open())
            return std::unexpected("the file cannot be written");

        file << SerializeScriptGraph(graph).dump(4);
        if (!file)
            return std::unexpected("writing the file failed");

        return {};
    }
}
