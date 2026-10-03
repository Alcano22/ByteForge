#pragma once

#include <Engine/Scene/UUID.h>

#include <functional>
#include <filesystem>

namespace ByteForge
{
    class EditorContext;

    class SceneDialogs
    {
    public:
        explicit SceneDialogs(EditorContext& context)
            : m_Context(context) {}

        void RequestNew();
        void RequestOpen(UUID scene);
        void RequestOpenFromDisk();

        bool RequestSave();
        bool RequestSaveAs();

        [[nodiscard]] bool RequestQuit();

        void Draw();

    private:
        void Guard(std::function<void()> action);
        void RunContinuation();
        bool EnsureEditing(const char* what) const;

        [[nodiscard]] std::filesystem::path GetDefaultDirectory() const;

    private:
        enum class Popup { None, UnsavedChanges, SaveAs, OpenScene };

        EditorContext& m_Context;

        bool m_PromptRequested = false;
        std::function<void()> m_Continuation;
    };
}
