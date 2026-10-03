#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Renderer/Texture2D.h>

#include <array>
#include <cstddef>
#include <filesystem>

namespace ByteForge
{
    enum class EditorIcon
    {
        Folder,
        FileGeneric,
        FileScript,
        FileData,
        FileFont,
        FileAudio,
        FileScene,
        PlayerPlay,
        PlayerPause,
        PlayerStep,
        PlayerStop,
        ToolMove,
        ToolRotate,
        ToolScale,
        Count
    };

    class EditorIcons
    {
    public:
        void Load(const std::filesystem::path& directory);

        [[nodiscard]] const Ref<Texture2D>& Get(EditorIcon icon) const;

        [[nodiscard]] static EditorIcon ForFile(const std::filesystem::path& path);

    private:
        std::array<Ref<Texture2D>, static_cast<size_t>(EditorIcon::Count)> m_Icons;
    };
}
