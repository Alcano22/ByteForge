#pragma once

#include <Engine/Scene/UUID.h>

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

        bool Save();
        bool SaveAs(const fs::path& relativePath);

        bool SetAsStartupScene() const;

        [[nodiscard]] bool HasFile() const { return m_Handle.has_value(); }
        [[nodiscard]] bool IsDirty() const;

        [[nodiscard]] fs::path GetPath() const;
        [[nodiscard]] std::string GetName() const;

        void Update() const;

        [[nodiscard]] static fs::path ToScenePath(std::string_view name);

    private:
        bool Write(const fs::path& absolutePath);

        void ResetEditorState() const;
        void RememberAsLast() const;

    public:
        static constexpr const char* Extension = ".bfscene";

    private:
        EditorContext& m_Context;
        std::optional<UUID> m_Handle;
    };
}
