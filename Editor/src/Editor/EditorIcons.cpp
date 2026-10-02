#include "Editor/EditorIcons.h"

#include <Engine/Core/Log.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ByteForge
{
    namespace
    {
        constexpr size_t IconCount = static_cast<size_t>(EditorIcon::Count);

        constexpr std::array<std::string_view, IconCount> IconFiles{
            "folder_filled.png", "folder_empty.png",
            "file_generic.png", "file_text.png", "file_code.png", "file_data.png", "file_font.png", "file_audio.png",
            "file_scene.png",
            "player_play.png", "player_pause.png", "player_step.png", "player_stop.png"
        };

        Ref<Texture2D> CreatePlaceholder()
        {
            constexpr std::array<std::byte, 4> magenta{ std::byte{ 255 }, std::byte{ 0 },
                                                        std::byte{ 255 }, std::byte{ 255 } };
            return Texture2D::Create(1, 1, magenta, { .Filter = TextureFilter::Nearest, .GenerateMips = false });
        }
    }

    void EditorIcons::Load(const std::filesystem::path& directory)
    {
        const TextureSettings settings{
            .Format       = ImageFormat::RGBA8_SRGB,
            .Filter       = TextureFilter::Linear,
            .Wrap         = TextureWrap::ClampToEdge,
            .GenerateMips = true
        };

        Ref<Texture2D> placeholder;

        for (size_t i = 0; i < IconCount; ++i)
        {
            try
            {
                m_Icons[i] = Texture2D::Load(directory / IconFiles[i], settings);
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
            { ".txt",     EditorIcon::FileText  }, { ".md",   EditorIcon::FileText  }, { ".log",  EditorIcon::FileText  },
            { ".ini",     EditorIcon::FileText  }, { ".cfg",  EditorIcon::FileText  },

            { ".cpp",     EditorIcon::FileCode  }, { ".h",    EditorIcon::FileCode  }, { ".hpp",  EditorIcon::FileCode  },
            { ".c",       EditorIcon::FileCode  }, { ".cs",   EditorIcon::FileCode  }, { ".hlsl", EditorIcon::FileCode  },
            { ".glsl",    EditorIcon::FileCode  }, { ".lua",  EditorIcon::FileCode  }, { ".py",   EditorIcon::FileCode  },

            { ".json",    EditorIcon::FileData  }, { ".xml",  EditorIcon::FileData  }, { ".yaml", EditorIcon::FileData  },
            { ".yml",     EditorIcon::FileData  }, { ".toml", EditorIcon::FileData  }, { ".csv",  EditorIcon::FileData  },

            { ".ttf",     EditorIcon::FileFont  }, { ".otf",  EditorIcon::FileFont  },

            { ".wav",     EditorIcon::FileAudio }, { ".ogg",  EditorIcon::FileAudio }, { ".mp3",  EditorIcon::FileAudio },
            { ".flac",    EditorIcon::FileAudio },

            { ".bfscene", EditorIcon::FileScene }
        };

        std::string extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(),
                               [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const auto it = icons.find(extension);
        return it != icons.end() ? it->second : EditorIcon::FileGeneric;
    }
}
