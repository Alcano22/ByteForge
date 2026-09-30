#pragma once

#include <string>

namespace ByteForge
{
    class EditorCommand
    {
    public:
        virtual ~EditorCommand() = default;

        virtual void Execute() = 0;
        virtual void Undo() = 0;

        [[nodiscard]] virtual std::string GetName() const = 0;
    };
}
