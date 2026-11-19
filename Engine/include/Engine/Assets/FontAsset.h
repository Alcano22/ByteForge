#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/TextureAsset.h"
#include "Engine/Scene/UUID.h"
#include "Engine/Text/Font.h"

namespace ByteForge
{
    class FontAsset
    {
    public:
        explicit FontAsset(const UUID handle)
            : m_Handle(handle) {}

        [[nodiscard]] UUID GetHandle() const { return m_Handle; }
        [[nodiscard]] const Ref<Font>& GetFont() const { return m_Font; }
        [[nodiscard]] AssetLoadState GetState() const { return m_State; }

    private:
        friend class AssetManager;

        UUID m_Handle;
        Ref<Font> m_Font;
        AssetLoadState m_State = AssetLoadState::Loading;
    };
}
