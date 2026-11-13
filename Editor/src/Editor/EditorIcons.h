#pragma once

#include "Editor/SvgIconFont.h"

#include <Engine/Core/Core.h>
#include <Engine/Renderer/Texture2D.h>

#include <array>
#include <cstddef>
#include <filesystem>

namespace ByteForge
{
    class EditorFonts;

    enum class EditorIcon
    {
        Folder,
        FileGeneric,
        FileScript,
        FileData,
        FileFont,
        FileAudio,
        FileScene,
        FilePhysicsMaterial,
        FileScriptGraph,
        FileAnimationClip,
        PlayerPlay,
        PlayerPause,
        PlayerStep,
        PlayerStop,
        ToolMove,
        ToolRotate,
        ToolScale,
        ComponentTransform,
        ComponentSprite,
        ComponentRigidbody,
        ComponentBoxCollider,
        ComponentCircleCollider,
        ComponentAudioSource,
        ComponentCamera,
        Count
    };

    class EditorIcons
    {
    public:
        void Load(const fs::path& directory);

        [[nodiscard]] const Ref<Texture2D>& Get(EditorIcon icon) const;

        [[nodiscard]] static EditorIcon ForFile(const fs::path& path);

        void AddToFonts(const EditorFonts& fonts);

        [[nodiscard]] static const char* Glyph(EditorIcon icon);

        [[nodiscard]] static ImWchar GetCodepoint(EditorIcon icon);

    private:
        std::array<Ref<Texture2D>, static_cast<usize>(EditorIcon::Count)> m_Icons;
        SvgIconFont m_Font;
    };
}
