#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Scene/UUID.h"

#include <utility>

namespace ByteForge
{
    enum class AssetLoadState { Loading, Loaded, Failed };

    class TextureAsset
    {
    public:
        TextureAsset(const UUID handle, Ref<Texture2D> placeholder)
            : m_Handle(handle), m_Texture(std::move(placeholder)) {}

        [[nodiscard]] UUID GetHandle() const { return m_Handle; }
        [[nodiscard]] const Ref<Texture2D>& GetTexture() const { return m_Texture; }
        [[nodiscard]] AssetLoadState GetState() const { return m_State; }

    private:
        friend class AssetManager;

        UUID m_Handle;
        Ref<Texture2D> m_Texture;
        AssetLoadState m_State = AssetLoadState::Loading;
    };
}
