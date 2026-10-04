#include "Editor/Graph/GraphDocument.h"
#include "Editor/Commands/EditorCommand.h"

#include <Engine/Assets/AssetRegistry.h>

#include <format>
#include <stdexcept>

namespace ByteForge
{
    class GraphSnapshotCommand final : public EditorCommand
    {
    public:
        GraphSnapshotCommand(GraphDocument& document, std::string name, nlohmann::json before, nlohmann::json after)
            : m_Document(document), m_Name(std::move(name)), m_Before(std::move(before)), m_After(std::move(after)) {}

        void Execute() override { m_Document.Restore(m_After); }
        void Undo() override { m_Document.Restore(m_Before); }

        [[nodiscard]] std::string GetName() const override { return m_Name; }

    private:
        GraphDocument& m_Document;
        std::string m_Name;
        nlohmann::json m_Before;
        nlohmann::json m_After;
    };

    GraphDocument::GraphDocument(const UUID asset, std::string name, ScriptGraph graph)
        : m_Asset(asset), m_Name(std::move(name)), m_Graph(std::move(graph)), m_Diagnostics(ValidateGraph(m_Graph)) {}

    std::expected<Scope<GraphDocument>, std::string> GraphDocument::Open(const UUID asset)
    {
        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(asset, metadata) || metadata.Type != AssetType::ScriptGraph)
            return std::unexpected("The asset is not a script graph");

        std::expected<ScriptGraph, std::string> graph = LoadScriptGraph(AssetRegistry::Resolve(asset));
        if (!graph)
            return std::unexpected(graph.error());

        return MakeScope<GraphDocument>(asset, metadata.Path.stem().string(), std::move(*graph));
    }

    std::expected<void, std::string> GraphDocument::Save()
    {
        if (auto saved = SaveScriptGraph(m_Graph, AssetRegistry::Resolve(m_Asset)); !saved)
            return saved;

        m_History.MarkClean();
        return {};
    }

    void GraphDocument::Commit(std::string name, nlohmann::json before)
    {
        m_History.Record(MakeScope<GraphSnapshotCommand>(*this, std::move(name), std::move(before),
                                                         SerializeScriptGraph(m_Graph)));
        OnGraphChanged();
    }

    void GraphDocument::Restore(const nlohmann::json& snapshot)
    {
        std::expected<ScriptGraph, std::string> graph = DeserializeScriptGraph(snapshot);
        if (!graph)
            throw std::runtime_error(std::format("Cannot restore the graph: {}", graph.error()));

        m_Graph = std::move(*graph);
        OnGraphChanged();
    }

    void GraphDocument::OnGraphChanged()
    {
        m_Diagnostics = ValidateGraph(m_Graph);
        ++m_Revision;
    }
}
