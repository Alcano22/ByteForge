#pragma once

#include "Editor/Commands/CommandHistory.h"

#include <Engine/Scene/UUID.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ByteForge
{
    class EditorContext;

    class SceneDocument
    {
    public:
        explicit SceneDocument(EditorContext& context)
            : m_Context(context) {}

        void New();

        bool Open(UUID scene);
        bool OpenLast();
        bool OpenStartScene();

        bool Save();
        bool SaveAs(const fs::path& relativePath);

        bool SetAsStartupScene() const;

        bool OpenPrefab(UUID prefab);
        void ClosePrefab();
        [[nodiscard]] bool IsEditingPrefab() const { return m_PrefabStage.has_value(); }
        [[nodiscard]] std::optional<UUID> GetPrefab() const;
        [[nodiscard]] std::string GetSceneName() const;

        [[nodiscard]] bool HasFile() const { return IsEditingPrefab() || m_Handle.has_value(); }
        [[nodiscard]] bool IsDirty() const;

        [[nodiscard]] fs::path GetPath() const;
        [[nodiscard]] fs::path GetScenePath() const;
        [[nodiscard]] std::string GetName() const;

        void Update() const;

        [[nodiscard]] static fs::path ToScenePath(std::string_view name);

    private:
        bool Write(const fs::path& absolutePath);

        bool SavePrefab();

        void ResetEditorState() const;
        void RememberAsLast() const;

    public:
        static constexpr const char* Extension = ".bfscene";

    private:
        struct PrefabStage
        {
            UUID Prefab;
            nlohmann::json SceneSnapshot;
            CommandHistory SceneHistory;
            bool Saved = false;
        };

        EditorContext& m_Context;
        std::optional<UUID> m_Handle;
        std::optional<PrefabStage> m_PrefabStage;
    };
}
