#pragma once

namespace ByteForge
{
    class EditorContext;

    class ScriptBuildProgress
    {
    public:
        void Update(EditorContext& context);

    private:
        bool m_Shown = false;
    };
}
