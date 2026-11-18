#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Project/Project.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge::Tests
{
    [[nodiscard]] const fs::path& GetTemporaryRoot();
    void SetTemporaryRoot(const fs::path& root);
    [[nodiscard]] bool IsInsideTemporaryRoot(const fs::path& path);

    class TemporaryDirectory
    {
    public:
        TemporaryDirectory();
        ~TemporaryDirectory();

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        [[nodiscard]] const fs::path& GetPath() const { return m_Path; }
        [[nodiscard]] fs::path operator/(const fs::path& relative) const { return m_Path / relative; };

    private:
        fs::path m_Path;
    };

    [[nodiscard]] std::string Text(std::u8string_view text);

    [[nodiscard]] std::vector<std::byte> Bytes(std::string_view text);
    [[nodiscard]] std::string ToText(std::span<const std::byte> bytes);

    void WriteTextFile(const fs::path& file, std::string_view text);

    [[nodiscard]] fs::path CreateTemplate(const fs::path& directory);

    [[nodiscard]] Project CreateProject(const fs::path& location, const std::string& name);

    template<typename T>
    void RequireSuccess(const Result<T>& result)
    {
        if (!result)
            FAIL(result.error());
    }
}
