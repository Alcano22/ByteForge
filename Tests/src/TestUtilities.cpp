#include "TestUtilities.h"

#include <Engine/Core/FileSystem.h>
#include <Engine/Project/ProjectTemplate.h>
#include <Engine/Scene/UUID.h>

#include <format>
#include <system_error>
#include <utility>

namespace ByteForge::Tests
{
    namespace
    {
        fs::path s_TemporaryRoot;
    }

    const fs::path& GetTemporaryRoot() { return s_TemporaryRoot; }
    void SetTemporaryRoot(const fs::path& root) { s_TemporaryRoot = root; }

    bool IsInsideTemporaryRoot(const fs::path& path)
    {
        if (s_TemporaryRoot.empty())
            return false;

        const fs::path relative = path.lexically_normal().lexically_relative(s_TemporaryRoot.lexically_normal());
        return !relative.empty() && *relative.begin() != "..";
    }

    TemporaryDirectory::TemporaryDirectory()
        : m_Path(GetTemporaryRoot() / std::format("{:016x}", static_cast<u64>(UUID())))
    {
        REQUIRE_FALSE(GetTemporaryRoot().empty());
        fs::create_directories(m_Path);
    }

    TemporaryDirectory::~TemporaryDirectory()
    {
        std::error_code ignored;
        fs::remove_all(m_Path, ignored);
    }

    std::string Text(const std::u8string_view text) { return { text.begin(), text.end() }; }

    std::vector<std::byte> Bytes(const std::string_view text)
    {
        const auto* data = reinterpret_cast<const std::byte*>(text.data());
        return { data, data + text.size() };
    }

    std::string ToText(const std::span<const std::byte> bytes)
    {
        return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
    }

    void WriteTextFile(const fs::path& file, const std::string_view text)
    {
        fs::create_directories(file.parent_path());
        RequireSuccess(WriteFileAtomic(file, text));
    }

    fs::path CreateTemplate(const fs::path& directory)
    {
        WriteTextFile(directory / ProjectTemplate::FileName, R"({ "name": "Test", "description": "For the unit tests" })");
        WriteTextFile(directory / Project::AssetDirectoryName / "Hello.txt", "hello");
        WriteTextFile(directory / "GameScripts.csproj", "<Project />");
        WriteTextFile(directory / Project::StateDirectoryName / "state.json", "{}");
        return directory;
    }

    Project CreateProject(const fs::path& location, const std::string& name)
    {
        const fs::path templateDirectory = CreateTemplate(location / ("Template-" + name));

        Result<Project> project = Project::Create({
            .Name              = name,
            .Location          = location,
            .TemplateDirectory = templateDirectory
        });
        RequireSuccess(project);
        return std::move(*project);
    }
}
