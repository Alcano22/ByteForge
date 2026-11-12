#pragma once

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
    };

    class GameExporter
    {
    public:
        [[nodiscard]] static Result<ExportResult> Export(
            const fs::path& projectRoot, const fs::path& outputRoot);
    };

    void ExportGameWithDialog(EditorContext& context);
}
