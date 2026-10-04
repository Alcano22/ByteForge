#include "Scripting/Visual/GraphScriptBackend.h"
#include "Scripting/Visual/GraphCompiler.h"
#include "Scripting/Visual/GraphScriptInstance.h"

#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Core/Log.h"
#include "Engine/Scripting/Visual/ScriptGraphSerializer.h"

#include <filesystem>
#include <format>
#include <stdexcept>
#include <utility>

namespace ByteForge
{
    std::vector<std::string> GraphScriptBackend::GetClassNames() const
    {
        Refresh(false);

        std::vector<std::string> names;
        names.reserve(m_Classes.size());
        for (const auto& [name, graphClass] : m_Classes)
            names.push_back(name);
        return names;
    }

    bool GraphScriptBackend::HasClass(const std::string_view className) const
    {
        return Find(className) != nullptr;
    }

    std::span<const ScriptFieldInfo> GraphScriptBackend::GetFields(const std::string_view className) const
    {
        const GraphClass* graphClass = Find(className);
        return graphClass != nullptr && graphClass->Program
             ? std::span<const ScriptFieldInfo>(graphClass->Program->Fields)
             : std::span<const ScriptFieldInfo>{};
    }

    std::string GraphScriptBackend::GetDisplayName(const std::string_view className) const
    {
        const GraphClass* graphClass = Find(className);
        return graphClass != nullptr ? graphClass->DisplayName : std::string(className);
    }

    Scope<ScriptInstance> GraphScriptBackend::CreateInstance(const std::string_view className, const Entity entity)
    {
        const GraphClass* graphClass = Find(className);
        if (graphClass == nullptr)
            throw std::runtime_error(std::format("Script graph '{}' does not exist", className));

        if (!graphClass->Program)
            throw std::runtime_error(std::format("Script graph '{}' has errors: {}",
                                                 graphClass->DisplayName, graphClass->Error));

        return MakeScope<GraphScriptInstance>(graphClass->Program, entity);
    }

    void GraphScriptBackend::OnRuntimeStart(Scene&) { Refresh(true); }

    void GraphScriptBackend::Refresh(const bool force) const
    {
        const uint64_t revision = AssetRegistry::GetRevision();
        if (!force && revision == m_Revision) return;
        m_Revision = revision;

        m_Classes.clear();
        for (const AssetMetadata& metadata : AssetRegistry::GetAssetsOfType(AssetType::ScriptGraph))
        {
            GraphClass graphClass{
                .DisplayName = std::filesystem::path(metadata.Path).replace_extension().generic_string()
            };

            if (auto graph = LoadScriptGraph(AssetRegistry::GetAssetRoot() / metadata.Path); !graph)
                graphClass.Error = std::format("cannot load: {}", graph.error());
            else if (auto program = CompileScriptGraph(*graph); !program)
                graphClass.Error = program.error();
            else
                graphClass.Program = std::make_shared<const GraphProgram>(std::move(*program));

            if (!graphClass.Error.empty())
                CORE_WARN("Script graph '{}': {}", graphClass.DisplayName, graphClass.Error);

            m_Classes.insert_or_assign(ScriptGraphClassName(metadata.Handle), std::move(graphClass));
        }
    }

    const GraphScriptBackend::GraphClass* GraphScriptBackend::Find(const std::string_view className) const
    {
        Refresh(false);

        const auto it = m_Classes.find(className);
        return it != m_Classes.end() ? &it->second : nullptr;
    }
}
