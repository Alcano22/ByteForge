#pragma once

#include <Engine/Core/Utf8.h>

#include <QString>

#include <filesystem>
#include <string_view>

namespace ByteForge::Hub
{
    [[nodiscard]] inline QString ToQString(const std::string_view text)
    {
        return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    }

    [[nodiscard]] inline QString PathToQString(const std::filesystem::path& path)
    {
        return ToQString(ToUtf8(path));
    }

    [[nodiscard]] inline std::filesystem::path ToPath(const QString& text)
    {
        return PathFromUtf8(text.toStdString());
    }
}
