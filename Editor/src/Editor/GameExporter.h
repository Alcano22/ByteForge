#pragma once

#include <Engine/Assets/Pak.h>

#include <expected>
#include <filesystem>
#include <string>

namespace ByteForge
{
    class EditorContext;

    struct ExportResult
    {
        fs::path Directory;
        usize FileCount = 0;
        PakStats Archive;
        usize ShaderStages = 0;
    };

    class GameExporter
    {
    public:
        [[nodiscard]] static Result<ExportResult> Export(
            const fs::path& projectRoot, const fs::path& outputRoot);
    };

    void ExportGameWithDialog(const EditorContext& context);
}
