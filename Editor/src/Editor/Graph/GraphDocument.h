#pragma once

#include "Editor/Commands/CommandHistory.h"

#include <Engine/Scene/UUID.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace ByteForge
{
    class GraphDocument
    {
    public:
        GraphDocument(UUID asset, std::string name, ScriptGraph graph);

        [[nodiscard]] static std::expected<Scope<GraphDocument>, std::string> Open(UUID asset);

        [[nodiscard]] UUID GetAsset() const { return m_Asset; }
        [[nodiscard]] const std::string& GetName() const { return m_Name; }
        [[nodiscard]] const ScriptGraph& GetGraph() const { return m_Graph; }
        [[nodiscard]] const std::vector<GraphDiagnostic>& GetDiagnostics() const { return m_Diagnostics; }

        [[nodiscard]] uint64_t GetRevision() const { return m_Revision; }
        [[nodiscard]] bool IsDirty() const { return m_History.IsDirty(); }

        template<typename Fn>
        bool Edit(std::string name, Fn&& change)
        {
            nlohmann::json before = SerializeScriptGraph(m_Graph);
            if (!std::forward<Fn>(change)(m_Graph))
                return false;

            Commit(std::move(name), std::move(before));
            return true;
        }

        bool Undo() { return m_History.Undo(); }
        bool Redo() { return m_History.Redo(); }
        [[nodiscard]] bool CanUndo() const { return m_History.CanUndo(); }
        [[nodiscard]] bool CanRedo() const { return m_History.CanRedo(); }

        std::expected<void, std::string> Save();

    private:
        void Commit(std::string name, nlohmann::json before);
        void Restore(const nlohmann::json& snapshot);
        void OnGraphChanged();

    private:
        friend class GraphSnapshotCommand;

        UUID m_Asset;
        std::string m_Name;
        ScriptGraph m_Graph;
        CommandHistory m_History;
        std::vector<GraphDiagnostic> m_Diagnostics;
        uint64_t m_Revision = 0;
    };
}
