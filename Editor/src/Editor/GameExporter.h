#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace ByteForge
{
    class EditorContext;

    struct ExportResult
    {
        std::filesystem::path Directory;
        usize FileCount = 0;
    };

    class GameExporter
    {
    public:
        [[nodiscard]] static std::expected<ExportResult, std::string> Export(
            const std::filesystem::path& projectRoot, const std::filesystem::path& outputRoot);
    };

    void ExportGameWithDialog(EditorContext& context);
}
