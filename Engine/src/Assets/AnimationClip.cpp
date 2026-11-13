#include "Engine/Assets/AnimationClip.h"
#include "Engine/Assets/AssetManager.h"

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr f32 MinFramesPerSecond = 0.01f;
        constexpr f32 MinFrameDuration   = 0.01f;

        nlohmann::json ToJson(const glm::vec2& value)
        {
            return nlohmann::json::array({ value.x, value.y });
        }

        glm::vec2 Vec2Or(const nlohmann::json& data, const char* key, const glm::vec2& fallback)
        {
            const auto it = data.find(key);
            if (it == data.end() || !it->is_array() || it->size() != 2)
                return fallback;

            return { (*it)[0].get<f32>(), (*it)[1].get<f32>() };
        }
    }

    AnimationClipAsset::AnimationClipAsset(const UUID handle, AnimationClipData data)
        : m_Handle(handle)
    {
        SetData(std::move(data));
    }

    u32 AnimationClipAsset::SampleFrame(const f32 time) const
    {
        if (m_FrameEnds.empty() || m_Duration <= 0.0f)
            return 0;

        f32 t = std::max(0.0f, time);
        switch (m_Data.LoopMode)
        {
            case AnimationLoopMode::Loop:
                t = std::fmod(t, m_Duration);
                break;
            case AnimationLoopMode::Once:
                t = std::min(t, m_Duration);
                break;
            case AnimationLoopMode::PingPong:
                t = std::fmod(t, 2.0f * m_Duration);
                if (t > m_Duration)
                    t = 2.0f * m_Duration - t;
                break;
        }

        const auto it = std::ranges::upper_bound(m_FrameEnds, t);
        const auto index = static_cast<u32>(it - m_FrameEnds.begin());
        return std::min(index, GetFrameCount() - 1);
    }

    bool AnimationClipAsset::IsFinished(const u32 time) const
    {
        return m_Data.LoopMode == AnimationLoopMode::Once && time >= m_Duration;
    }

    const Ref<Sprite>& AnimationClipAsset::GetSprite(const u32 frame) const
    {
        static const Ref<Sprite> none;
        return frame < m_Sprites.size() ? m_Sprites[frame] : none;
    }

    void AnimationClipAsset::SetData(AnimationClipData data)
    {
        m_Data = std::move(data);

        m_Sprites.clear();
        m_FrameEnds.clear();
        m_Duration = 0.0f;

        m_Sprites.reserve(m_Data.Frames.size());
        m_FrameEnds.reserve(m_Data.Frames.size());

        const f32 frameTime = 1.0f / std::max(MinFramesPerSecond, m_Data.FramesPerSecond);

        for (const AnimationFrame& frame : m_Data.Frames)
        {
            m_Sprites.push_back(MakeRef<Sprite>(AssetManager::LoadTexture2D(frame.Texture), frame.UVMin, frame.UVMax));

            m_Duration += std::max(MinFrameDuration, frame.Duration) * frameTime;
            m_FrameEnds.push_back(m_Duration);
        }
    }

    Result<AnimationClipData> ParseAnimationClip(const nlohmann::json& data)
    {
        if (!data.is_object())
            return std::unexpected("expected a JSON object");

        try
        {
            AnimationClipData clip;
            clip.FramesPerSecond = std::max(MinFramesPerSecond, data.value("fps", clip.FramesPerSecond));

            if (const auto it = data.find("loopMode"); it != data.end() && it->is_string())
                clip.LoopMode = magic_enum::enum_cast<AnimationLoopMode>(it->get<std::string>()).value_or(clip.LoopMode);

            if (const auto it = data.find("frames"); it != data.end())
            {
                if (!it->is_array())
                    return std::unexpected("'frames' must be an array");

                clip.Frames.reserve(it->size());
                for (const nlohmann::json& frameJson : *it)
                {
                    if (!frameJson.is_object())
                        return std::unexpected("every frame must be a JSON object");

                    AnimationFrame frame;
                    frame.Texture  = UUID(frameJson.value("texture", u64{ 0 }));
                    frame.UVMin    = Vec2Or(frameJson, "uvMin", frame.UVMin);
                    frame.UVMax    = Vec2Or(frameJson, "uvMax", frame.UVMax);
                    frame.Duration = std::max(MinFrameDuration, frameJson.value("duration", frame.Duration));
                    clip.Frames.push_back(frame);
                }
            }

            return clip;
        } catch (const nlohmann::json::exception& e)
        {
            return std::unexpected(e.what());
        }
    }

    nlohmann::json SerializeAnimationClip(const AnimationClipData& clip)
    {
        nlohmann::json frames = nlohmann::json::array();
        for (const AnimationFrame& frame : clip.Frames)
        {
            frames.push_back(nlohmann::json{
                { "textures", static_cast<u64>(frame.Texture) },
                { "uvMin",    ToJson(frame.UVMin)             },
                { "uvMax",    ToJson(frame.UVMax)             },
                { "duration", frame.Duration                  }
            });
        }

        return nlohmann::json{
            { "fps",      clip.FramesPerSecond                              },
            { "loopMode", std::string(magic_enum::enum_name(clip.LoopMode)) },
            { "frames",   std::move(frames)                                 }
        };
    }
}
