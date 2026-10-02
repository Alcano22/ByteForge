#pragma once

#include <Engine/Scene/UUID.h>

#include <array>
#include <functional>

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
        void RequestOpenPicker();

        void RequestSave();
        void RequestSaveAs();

        [[nodiscard]] bool RequestQuit();

        void Draw();

    private:
        void Guard(std::function<void()> action);
        void RunContinuation();
        bool EnsureEditing(const char* what) const;

        void DrawUnsavedChanges();
        void DrawSaveAs();
        void DrawOpenScene();

    private:
        enum class Popup { None, UnsavedChanges, SaveAs, OpenScene };

        EditorContext& m_Context;

        Popup m_Requested = Popup::None;
        std::function<void()> m_Continuation;
        std::array<char, 128> m_NameBuffer{};
    };
}
