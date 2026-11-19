#include "Editor/ScriptBuildProgress.h"
#include "Editor/EditorContext.h"

#include <optional>

namespace ByteForge
{
    namespace
    {
        constexpr f32 ShowAfterSeconds = 0.3f;
    }

    void ScriptBuildProgress::Update(EditorContext& context)
    {
        const ScriptProject& scripts = context.Scripts;
        if (!scripts.IsBuilding())
        {
            m_Shown = false;
            return;
        }

        if (m_Shown || !context.IsEditing() || scripts.GetBuildSeconds() < ShowAfterSeconds) return;

        m_Shown = true;

        context.Popups.OpenProgress("Compiling Scripts", [&context]() -> std::optional<ProgressState>
        {
            if (!context.Scripts.IsBuilding() || !context.IsEditing())
                return std::nullopt;

            return ProgressState{ .Status = "Compiling the C# scripts..." };
        });
    }
}
