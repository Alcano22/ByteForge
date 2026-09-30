#pragma once

#include "Editor/Commands/EditorCommand.h"

#include <Engine/Core/Core.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    class CommandHistory
    {
    public:
        explicit CommandHistory(const size_t capacity = 256)
            : m_Capacity(capacity) {}

        void Execute(Scope<EditorCommand> command);

        void Record(Scope<EditorCommand> command);

        bool Undo();
        bool Redo();
        void Clear();

        [[nodiscard]] bool CanUndo() const { return !m_UndoStack.empty(); }
        [[nodiscard]] bool CanRedo() const { return !m_RedoStack.empty(); }
        [[nodiscard]] std::string GetUndoName() const;
        [[nodiscard]] std::string GetRedoName() const;

        [[nodiscard]] bool IsDirty() const { return m_CleanDepth != m_UndoStack.size(); }
        void MarkClean() { m_CleanDepth = m_UndoStack.size(); }

        [[nodiscard]] uint64_t GetRevision() const { return m_Revision; }

    private:
        void Push(Scope<EditorCommand> command);
        void Fail(const char* action, const std::exception& error);

    private:
        std::deque<Scope<EditorCommand>> m_UndoStack;
        std::vector<Scope<EditorCommand>> m_RedoStack;
        size_t m_Capacity;

        std::optional<size_t> m_CleanDepth = 0;
        uint64_t m_Revision = 0;
    };
}
