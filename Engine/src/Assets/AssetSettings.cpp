#include "Engine/Assets/AssetSettings.h"
#include "Engine/Core/Log.h"

#include <magic_enum/magic_enum.hpp>

#include <cmath>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

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

        f32 NumberOr(const nlohmann::json& data, const char* key, const f32 fallback)
        {
            if (!data.contains(key) || !data.at(key).is_number())
                return fallback;
            return data.at(key).get<f32>();
        }

        glm::vec2 Vec2Or(const nlohmann::json& data, const char* key, const glm::vec2 fallback)
        {
            if (!data.contains(key))
                return fallback;

            const nlohmann::json& value = data.at(key);
            if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number())
                return fallback;

            return { value[0].get<f32>(), value[1].get<f32>() };
        }

        std::vector<SpriteRegion> ReadSprites(const nlohmann::json& data)
        {
            std::vector<SpriteRegion> sprites;
            if (!data.contains("sprites") || !data.at("sprites").is_array())
                return sprites;

            std::unordered_set<UUID> ids;
            for (const nlohmann::json& entry : data.at("sprites"))
            {
                try
                {
                    const nlohmann::json& rect = entry.at("rect");
                    if (!rect.is_array() || rect.size() != 4)
                    {
                        CORE_WARN("AssetSettings: sprite without a valid 'rect' is skipped");
                        continue;
                    }

                    SpriteRegion region;
                    region.Id    = UUID(entry.at("id").get<u64>());
                    region.Name  = entry.value("name", std::string());
                    region.Rect  = { rect[0].get<u32>(), rect[1].get<u32>(), rect[2].get<u32>(), rect[3].get<u32>() };
                    region.Pivot = Vec2Or(entry, "pivot", region.Pivot);

                    if (!region.Id.IsSet() || region.Rect.Width == 0 || region.Rect.Height == 0)
                    {
                        CORE_WARN("AssetSettings: sprite '{}' has no id or an empty rect and is skipped", region.Name);
                        continue;
                    }

                    if (!ids.insert(region.Id).second)
                    {
                        CORE_WARN("AssetSettings: sprite '{}' repeats id {} and is skipped",
                                  region.Name, static_cast<u64>(region.Id));
                        continue;
                    }

                    sprites.push_back(std::move(region));
                } catch (const nlohmann::json::exception& e)
                {
                    CORE_WARN("AssetSettings: broken sprite entry is skipped: {}", e.what());
                }
            }

            return sprites;
        }
    }

    AssetSettings DefaultAssetSettings(const AssetType type)
    {
        switch (type)
        {
            case AssetType::Texture2D:          return TextureAssetSettings{};
            case AssetType::AudioClip:          return AudioClipSettings{};
            case AssetType::Script:             return std::monostate{};
            case AssetType::Scene:              return std::monostate{};
            case AssetType::PhysicsMaterial2D:  return std::monostate{};
            case AssetType::ScriptGraph:        return std::monostate{};
            case AssetType::Prefab:             return std::monostate{};
            case AssetType::AnimationClip:      return std::monostate{};
            case AssetType::AnimatorController: return std::monostate{};
            case AssetType::Font:               return std::monostate{};
            case AssetType::InputActions:       return std::monostate{};
            case AssetType::Material:           return std::monostate{};
            case AssetType::Sprite:             return std::monostate{};
            case AssetType::None:               break;
        }
        return std::monostate{};
    }

    nlohmann::json SerializeAssetSettings(const AssetSettings& settings)
    {
        return std::visit([](const auto& value) -> nlohmann::json
        {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, TextureAssetSettings>)
            {
                nlohmann::json sprites = nlohmann::json::array();
                for (const SpriteRegion& region : value.Sprites)
                {
                    sprites.push_back(nlohmann::json{
                        { "id",    static_cast<u64>(region.Id) },
                        { "name",  region.Name },
                        { "rect",  nlohmann::json::array({ region.Rect.X, region.Rect.Y,
                                                           region.Rect.Width, region.Rect.Height }) },
                        { "pivot", nlohmann::json::array({ region.Pivot.x, region.Pivot.y }) }
                    });
                }

                const TextureSettings& sampling = value.Sampling;
                return nlohmann::json{
                    { "format",        std::string(magic_enum::enum_name(sampling.Format)) },
                    { "filter",        std::string(magic_enum::enum_name(sampling.Filter)) },
                    { "wrap",          std::string(magic_enum::enum_name(sampling.Wrap))   },
                    { "generateMips",  sampling.GenerateMips                               },
                    { "spriteMode",    std::string(magic_enum::enum_name(value.Mode))      },
                    { "pixelsPerUnit", value.PixelsPerUnit                                 },
                    { "pivot",         nlohmann::json::array({ value.Pivot.x, value.Pivot.y }) },
                    { "sprites",       std::move(sprites)                                  }
                };
            } else if constexpr (std::is_same_v<T, AudioClipSettings>)
                return nlohmann::json{ { "stream", value.Stream } };
            else
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
                TextureAssetSettings settings;

                TextureSettings& sampling = settings.Sampling;
                sampling.Format       = EnumOr(data, "format", sampling.Format);
                sampling.Filter       = EnumOr(data, "filter", sampling.Filter);
                sampling.Wrap         = EnumOr(data, "wrap",   sampling.Wrap);
                sampling.GenerateMips = data.value("generateMips", sampling.GenerateMips);

                if (!IsSupportedTextureFormat(sampling.Format))
                {
                    CORE_WARN("AssetSettings: '{}' is not a valid texture format, using the default",
                              magic_enum::enum_name(sampling.Format));
                    sampling.Format = TextureSettings{}.Format;
                }

                settings.Mode          = EnumOr(data, "spriteMode", settings.Mode);
                settings.PixelsPerUnit = NumberOr(data, "pixelsPerUnit", settings.PixelsPerUnit);
                settings.Pivot         = Vec2Or(data, "pivot", settings.Pivot);
                settings.Sprites       = ReadSprites(data);

                if (!std::isfinite(settings.PixelsPerUnit) || settings.PixelsPerUnit <= 0.0f)
                {
                    CORE_WARN("AssetSettings: pixels per unit must be positive, using {}", DefaultPixelsPerUnit);
                    settings.PixelsPerUnit = DefaultPixelsPerUnit;
                }

                return settings;
            }
            case AssetType::AudioClip:
                return AudioClipSettings{ .Stream = data.value("stream", false) };
            case AssetType::Script:             return std::monostate{};
            case AssetType::Scene:              return std::monostate{};
            case AssetType::PhysicsMaterial2D:  return std::monostate{};
            case AssetType::ScriptGraph:        return std::monostate{};
            case AssetType::Prefab:             return std::monostate{};
            case AssetType::AnimationClip:      return std::monostate{};
            case AssetType::AnimatorController: return std::monostate{};
            case AssetType::Font:               return std::monostate{};
            case AssetType::InputActions:       return std::monostate{};
            case AssetType::Material:           return std::monostate{};
            case AssetType::Sprite:             return std::monostate{};
            case AssetType::None:               break;
        }

        return std::monostate{};
    }
}
