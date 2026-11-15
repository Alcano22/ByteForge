#pragma once

namespace ByteForge
{
    class EditorContext;

    enum class SettingsGroup { Project, Editor };

    class SettingsPage
    {
    public:
        virtual ~SettingsPage() = default;

        [[nodiscard]] virtual const char* GetName() const = 0;
        [[nodiscard]] virtual SettingsGroup GetGroup() const = 0;

        [[nodiscard]] virtual bool HasUnsavedChanges() const { return false; }

        virtual void Draw(EditorContext& context) = 0;
    };
}
