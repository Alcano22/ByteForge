#include "Editor/EditorIcons.h"

#include <Engine/Core/Log.h>

#include <lunasvg.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr size_t IconCount = static_cast<size_t>(EditorIcon::Count);

        constexpr int SvgResolution = 256;

        constexpr std::string_view SvgStyleSheet = "svg { color: #ffffff; }";

        constexpr std::array<std::string_view, IconCount> IconFiles{
            "folder.svg",
            "file_generic.svg", "file_script.svg", "file_data.svg", "file_font.svg", "file_audio.svg", "file_scene.svg",
            "player_play.svg", "player_pause.svg", "player_step.svg", "player_stop.svg",
            "tool_move.svg", "tool_rotate.svg", "tool_scale.svg"
        };

        constexpr TextureSettings IconSettings{
            .Format       = ImageFormat::RGBA8_SRGB,
            .Filter       = TextureFilter::Linear,
            .Wrap         = TextureWrap::ClampToEdge,
            .GenerateMips = true
        };

        Ref<Texture2D> LoadSvg(const std::filesystem::path& path)
        {
            const std::unique_ptr<lunasvg::Document> document = lunasvg::Document::loadFromFile(path.string());
            if (!document)
                throw std::runtime_error(std::format("Failed to parse SVG '{}'", path.string()));

            document->applyStyleSheet(std::string(SvgStyleSheet));

            lunasvg::Bitmap bitmap = document->renderToBitmap(SvgResolution, SvgResolution);
            if (bitmap.isNull())
                throw std::runtime_error(std::format("Failed to render SVG '{}'", path.string()));

            bitmap.convertToRGBA();

            const uint32_t width = static_cast<uint32_t>(bitmap.width());
            const uint32_t height = static_cast<uint32_t>(bitmap.height());
            const size_t rowSize = static_cast<size_t>(width) * 4;

            std::vector<std::byte> pixels(rowSize * height);
            for (uint32_t row = 0; row < height; ++row)
            {
                std::memcpy(pixels.data() + row * rowSize,
                            bitmap.data() + static_cast<size_t>(row) * static_cast<size_t>(bitmap.stride()),
                            rowSize);
            }

            return Texture2D::Create(width, height, pixels, IconSettings);
        }

        Ref<Texture2D> LoadIcon(const std::filesystem::path& path)
        {
            return path.extension() == ".svg" ? LoadSvg(path) : Texture2D::Load(path, IconSettings);
        }

        Ref<Texture2D> CreatePlaceholder()
        {
            constexpr std::array<std::byte, 4> magenta{ std::byte{ 255 }, std::byte{ 0 },
                                                        std::byte{ 255 }, std::byte{ 255 } };
            return Texture2D::Create(1, 1, magenta, { .Filter = TextureFilter::Nearest, .GenerateMips = false });
        }
    }

    void EditorIcons::Load(const std::filesystem::path& directory)
    {
        Ref<Texture2D> placeholder;

        for (size_t i = 0; i < IconCount; ++i)
        {
            try
            {
                m_Icons[i] = LoadIcon(directory / IconFiles[i]);
            } catch (const std::exception& e)
            {
                APP_ERROR("EditorIcons: {}", e.what());

                if (!placeholder)
                    placeholder = CreatePlaceholder();
                m_Icons[i] = placeholder;
            }
        }
    }

    const Ref<Texture2D>& EditorIcons::Get(const EditorIcon icon) const
    {
        const Ref<Texture2D>& texture = m_Icons.at(static_cast<size_t>(icon));
        if (!texture)
            throw std::runtime_error("EditorIcons::Get: icons are not loaded, call Load first");

        return texture;
    }

    EditorIcon EditorIcons::ForFile(const std::filesystem::path& path)
    {
        static const std::unordered_map<std::string_view, EditorIcon> icons{
            { ".cs",      EditorIcon::FileScript },

            { ".json",    EditorIcon::FileData   },
            { ".xml",     EditorIcon::FileData   },
            { ".yaml",    EditorIcon::FileData   },
            { ".yml",     EditorIcon::FileData   },
            { ".toml",    EditorIcon::FileData   },
            { ".csv",     EditorIcon::FileData   },

            { ".ttf",     EditorIcon::FileFont   },
            { ".otf",     EditorIcon::FileFont   },

            { ".wav",     EditorIcon::FileAudio  },
            { ".ogg",     EditorIcon::FileAudio  },
            { ".mp3",     EditorIcon::FileAudio  },
            { ".flac",    EditorIcon::FileAudio  },

            { ".bfscene", EditorIcon::FileScene  }
        };

        std::string extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(),
                               [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const auto it = icons.find(extension);
        return it != icons.end() ? it->second : EditorIcon::FileGeneric;
    }
}
