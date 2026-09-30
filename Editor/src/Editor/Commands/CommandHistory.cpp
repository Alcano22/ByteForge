#include "Editor/Commands/CommandHistory.h"

#include <Engine/Core/Log.h>

#include <stdexcept>
#include <utility>

namespace ByteForge
{
    void CommandHistory::Execute(Scope<EditorCommand> command)
    {
        if (!command)
            throw std::runtime_error("CommandHistory::Execute: the command must not be null");

        command->Execute();
        Push(std::move(command));
    }

    void CommandHistory::Record(Scope<EditorCommand> command)
    {
        if (!command)
            throw std::runtime_error("CommandHistory::Record: the command must not be null");

        Push(std::move(command));
    }

    bool CommandHistory::Undo()
    {
        if (m_UndoStack.empty())
            return false;

        Scope<EditorCommand> command = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();

        try
        {
            command->Undo();
        } catch (const std::exception& e)
        {
            Fail("undo", e);
            return false;
        }

        m_RedoStack.push_back(std::move(command));
        ++m_Revision;
        return true;
    }

    bool CommandHistory::Redo()
    {
        if (m_RedoStack.empty())
            return false;

        Scope<EditorCommand> command = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();

        try
        {
            command->Execute();
        } catch (const std::exception& e)
        {
            Fail("redo", e);
            return false;
        }

        m_UndoStack.push_back(std::move(command));
        ++m_Revision;
        return true;
    }

    void CommandHistory::Clear()
    {
        const bool dirty = IsDirty();

        m_UndoStack.clear();
        m_RedoStack.clear();

        m_CleanDepth = dirty ? std::nullopt : std::optional<size_t>(0);
        ++m_Revision;
    }

    std::string CommandHistory::GetUndoName() const
    {
        return m_UndoStack.empty() ? std::string() : m_UndoStack.back()->GetName();
    }

    std::string CommandHistory::GetRedoName() const
    {
        return m_RedoStack.empty() ? std::string() : m_RedoStack.back()->GetName();
    }

    void CommandHistory::Push(Scope<EditorCommand> command)
    {
        if (m_CleanDepth && *m_CleanDepth > m_UndoStack.size())
            m_CleanDepth.reset();

        m_RedoStack.clear();
        m_UndoStack.push_back(std::move(command));

        if (m_UndoStack.size() > m_Capacity)
        {
            m_UndoStack.pop_front();

            if (m_CleanDepth)
            {
                if (*m_CleanDepth == 0)
                    m_CleanDepth.reset();
                else
                    --*m_CleanDepth;
            }
        }

        ++m_Revision;
    }

    void CommandHistory::Fail(const char* action, const std::exception& error)
    {
        APP_ERROR("Could not {} ({}); the edit history was cleared to stay consistent", action, error.what());

        m_UndoStack.clear();
        m_RedoStack.clear();
        m_CleanDepth.reset();
        ++m_Revision;
    }
}
