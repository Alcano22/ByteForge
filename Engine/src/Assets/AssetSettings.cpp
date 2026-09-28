#include "Engine/Assets/AssetSettings.h"
#include "Engine/Core/Log.h"

#include <magic_enum/magic_enum.hpp>

#include <string>
#include <type_traits>

namespace ByteForge
{
    namespace
    {
        template<typename T>
        T EnumOr(const nlohmann::json& data, const char* key, const T fallback)
        {
            if (!data.contains(key) || !data.at(key).is_string())
                return fallback;

            const auto name = data.at(key).get<std::string>();
            if (const auto value = magic_enum::enum_cast<T>(name))
                return *value;

            CORE_WARN("AssetSettings: unknown value '{}' for '{}', using the default", name, key);
            return fallback;
        }

        bool IsSupportedTextureFormat(const ImageFormat format)
        {
            return format == ImageFormat::RGBA8_SRGB || format == ImageFormat::RGBA8_UNORM;
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
                    { "format",       std::string(magic_enum::enum_name(value.Format)) },
                    { "filter",       std::string(magic_enum::enum_name(value.Filter)) },
                    { "wrap",         std::string(magic_enum::enum_name(value.Wrap))   },
                    { "generateMips", value.GenerateMips                               }
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
                settings.Format       = EnumOr(data, "format", settings.Format);
                settings.Filter       = EnumOr(data, "filter", settings.Filter);
                settings.Wrap         = EnumOr(data, "wrap",   settings.Wrap);
                settings.GenerateMips = data.value("generateMips", settings.GenerateMips);

                if (!IsSupportedTextureFormat(settings.Format))
                {
                    CORE_WARN("AssetSettings: '{}' is not a valid texture format, using the default",
                              magic_enum::enum_name(settings.Format));
                    settings.Format = TextureSettings{}.Format;
                }

                return settings;
            }
            case AssetType::None: break;
        }

        return std::monostate{};
    }
}
