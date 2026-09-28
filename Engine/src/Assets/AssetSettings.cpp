#include "Engine/Assets/AssetSettings.h"
#include "Engine/Core/Log.h"

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ByteForge
{
    namespace
    {
        template<typename T, size_t N>
        using EnumTable = std::array<std::pair<std::string_view, T>, N>;

        constexpr EnumTable<ImageFormat, 2> TextureFormats{{
            { "RGBA8_SRGB",  ImageFormat::RGBA8_SRGB  },
            { "RGBA8_UNORM", ImageFormat::RGBA8_UNORM }
        }};

        constexpr EnumTable<TextureFilter, 2> TextureFilters{{
            { "Nearest", TextureFilter::Nearest },
            { "Linear",  TextureFilter::Linear  }
        }};

        constexpr EnumTable<TextureWrap, 2> TextureWraps{{
            { "Repeat",      TextureWrap::Repeat      },
            { "ClampToEdge", TextureWrap::ClampToEdge }
        }};

        template<typename T, size_t N>
        std::string NameOf(const EnumTable<T, N>& table, const T value)
        {
            for (const auto& [name, candidate] : table)
            {
                if (candidate == value)
                    return std::string(name);
            }

            throw std::runtime_error("AssetSettings: enum value is not supported in .meta files");
        }

        template<typename T, size_t N>
        T ValueOf(const EnumTable<T, N>& table, const nlohmann::json& data, const char* key, const T fallback)
        {
            if (!data.contains(key) || !data.at(key).is_string())
                return fallback;

            const std::string name = data.at(key).get<std::string>();
            for (const auto& [candidate, value] : table)
            {
                if (candidate == name)
                    return value;
            }

            CORE_WARN("AssetSettings: unknown value '{}' for '{}', using the default", name, key);
            return fallback;
        }
    }

    AssetSettings DefaultAssetSettings(const AssetType type)
    {
        switch (type)
        {
            case AssetType::Texture2D: return TextureSettings{};
            case AssetType::None:      break;
        }
        return std::monostate{};
    }

    nlohmann::json SerializeAssetSettings(const AssetSettings& settings)
    {
        return std::visit([](const auto& value) -> nlohmann::json
        {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, TextureSettings>)
            {
                return nlohmann::json{
                    { "format",       NameOf(TextureFormats, value.Format) },
                    { "filter",       NameOf(TextureFilters, value.Filter) },
                    { "wrap",         NameOf(TextureWraps,   value.Wrap)   },
                    { "generateMips", value.GenerateMips                   }
                };
            } else
                return nlohmann::json::object();
        }, settings);
    }

    AssetSettings DeserializeAssetSettings(const AssetType type, const nlohmann::json& data)
    {
        if (!data.is_object())
            return DefaultAssetSettings(type);

        switch (type)
        {
            case AssetType::Texture2D:
            {
                TextureSettings settings;
                settings.Format       = ValueOf(TextureFormats, data, "format", settings.Format);
                settings.Filter       = ValueOf(TextureFilters, data, "filter", settings.Filter);
                settings.Wrap         = ValueOf(TextureWraps,   data, "wrap",   settings.Wrap);
                settings.GenerateMips = data.value("generateMips", settings.GenerateMips);
                return settings;
            }
            case AssetType::None: break;
        }

        return std::monostate{};
    }
}
