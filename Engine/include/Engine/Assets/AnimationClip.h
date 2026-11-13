#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/Sprite.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

#include <vector>

namespace ByteForge
{
    inline constexpr const char* AnimationClipExtension = ".bfanim";

    enum class AnimationLoopMode : u8
    {
        Loop,
        Once,
        PingPong
    };

    struct AnimationFrame
    {
        UUID Texture{ 0 };
        glm::vec2 UVMin{ 0.0f };
        glm::vec2 UVMax{ 1.0f };

        f32 Duration = -1.0f;
    };

    struct AnimationClipData
    {
        f32 FramesPerSecond = 12.0f;
        AnimationLoopMode LoopMode = AnimationLoopMode::Loop;
        std::vector<AnimationFrame> Frames;
    };

    class BYTEFORGE_API AnimationClipAsset
    {
    public:
        AnimationClipAsset(UUID handle, AnimationClipData data);

        [[nodiscard]] UUID GetHandle() const { return m_Handle; }
        [[nodiscard]] const AnimationClipData& GetData() const { return m_Data; }

        [[nodiscard]] u32 GetFrameCount() const { return static_cast<u32>(m_Sprites.size()); }
        [[nodiscard]] bool IsEmpty() const { return m_Sprites.empty(); }

        [[nodiscard]] f32 GetDuration() const { return m_Duration; }

        [[nodiscard]] u32 SampleFrame(f32 time) const;

        [[nodiscard]] bool IsFinished(u32 time) const;

        [[nodiscard]] const Ref<Sprite>& GetSprite(u32 frame) const;

    private:
        void SetData(AnimationClipData data);

    private:
        friend class AssetManager;

        UUID m_Handle;
        AnimationClipData m_Data;

        std::vector<Ref<Sprite>> m_Sprites;
        std::vector<f32> m_FrameEnds;
        f32 m_Duration = 0.0f;
    };

    [[nodiscard]] BYTEFORGE_API Result<AnimationClipData> ParseAnimationClip(const nlohmann::json& data);
    [[nodiscard]] BYTEFORGE_API nlohmann::json SerializeAnimationClip(const AnimationClipData& clip);
}
