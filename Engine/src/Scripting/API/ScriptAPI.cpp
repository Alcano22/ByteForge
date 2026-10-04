#include "Engine/Scripting/API/ScriptAPI.h"
#include "Scripting/API/CoreAPI.h"

#include <format>
#include <stdexcept>

namespace ByteForge
{
    Entity ScriptCallContext::Resolve(const EntityRef entity) const
    {
        if (ActiveScene == nullptr)
            throw ScriptError("No scene is running");
        if (!entity.IsSet())
            throw ScriptError("Entity reference is not set");

        const Entity resolved = ActiveScene->FindEntityByUUID(entity.Id);
        if (!resolved.IsValid())
            throw ScriptError(std::format("Entity {} no longer exists", static_cast<uint64_t>(entity.Id)));

        return resolved;
    }

    std::string DescribeType(const ScriptParameter& parameter)
    {
        std::string name(ScriptFieldTypeName(parameter.Type));
        if (parameter.Type == ScriptFieldType::Asset)
            name += ':' + std::string(AssetTypeToString(parameter.Asset));
        return name;
    }

    std::string GetSignature(const ScriptFunction& function)
    {
        std::string signature = "(";
        for (size_t i = 0; i < function.Parameters.size(); ++i)
        {
            if (i > 0)
                signature += ',';
            signature += DescribeType(function.Parameters[i]);
        }

        signature += ")->";
        signature += function.Result ? DescribeType(*function.Result) : "Void";
        return signature;
    }

    void ValidateArguments(const ScriptFunction& function, const std::span<const ScriptValue> arguments)
    {
        if (arguments.size() != function.Parameters.size())
            throw ScriptError(std::format("{} expects {} argument(s), got {}",
                                          function.Id, function.Parameters.size(), arguments.size()));

        for (size_t i = 0; i < arguments.size(); ++i)
        {
            const ScriptParameter& parameter = function.Parameters[i];

            bool matches = GetFieldType(arguments[i]) == parameter.Type;
            if (const auto* asset = std::get_if<AssetRef>(&arguments[i]); matches && asset != nullptr)
                matches = !asset->IsSet() || asset->Type == parameter.Asset;

            if (!matches)
                throw ScriptError(std::format("{}: argument '{}' must be {}",
                                              function.Id, parameter.Name, DescribeType(parameter)));
        }
    }

    ScriptAPI::ScriptAPI() { RegisterCoreAPI(*this); }

    const ScriptAPI& ScriptAPI::Get()
    {
        static const ScriptAPI instance;
        return instance;
    }

    uint32_t ScriptAPI::FindIndex(const std::string_view id) const
    {
        const auto it = m_Indices.find(id);
        return it != m_Indices.end() ? it->second : InvalidIndex;
    }

    const ScriptFunction* ScriptAPI::Find(const std::string_view id) const
    {
        const uint32_t index = FindIndex(id);
        return index != InvalidIndex ? &m_Functions[index] : nullptr;
    }

    const ScriptFunction& ScriptAPI::GetFunction(const uint32_t index) const
    {
        if (index >= m_Functions.size())
            throw ScriptError(std::format("Unknown script function index {}", index));
        return m_Functions[index];
    }

    void ScriptAPI::Add(ScriptFunction function)
    {
        const size_t dot = function.Id.find('.');
        if (dot == std::string::npos || dot == 0 || dot + 1 == function.Id.size())
            throw std::logic_error(std::format("Script function id '{}' must look like 'Category.Name'", function.Id));

        if (m_Indices.contains(function.Id))
            throw std::logic_error(std::format("Script function '{}' is registered twice", function.Id));

        function.Category = function.Id.substr(0, dot);
        m_Indices.emplace(function.Id, static_cast<uint32_t>(m_Functions.size()));
        m_Functions.push_back(std::move(function));
    }
}
