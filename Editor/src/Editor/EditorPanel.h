#pragma once

#include <Engine/Core/Timestep.h>

#include <string>
#include <utility>

namespace ByteForge
{
    class EditorContext;

    class EditorPanel
    {
    public:
        explicit EditorPanel(EditorContext& context, std::string name)
            : m_Context(context), m_Name(std::move(name)) {}

        virtual ~EditorPanel() = default;

        EditorPanel(const EditorPanel&) = delete;
        EditorPanel& operator=(const EditorPanel&) = delete;

        virtual void OnUpdate(Timestep) {}
        virtual void OnImGuiRender() = 0;

        [[nodiscard]] const std::string& GetName() const { return m_Name; }

        [[nodiscard]] bool IsOpen() const { return m_Open; }
        void SetOpen(const bool open) { m_Open = open; }

    protected:
        [[nodiscard]] EditorContext& GetContext() const { return m_Context; }

    protected:
        bool m_Open = true;

    private:
        EditorContext& m_Context;
        std::string m_Name;
    };
}
