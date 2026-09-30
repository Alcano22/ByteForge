#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/RenderTarget.h"

#include <imgui.h>

namespace ByteForge::UI
{
    BYTEFORGE_API void Image(const Ref<Texture2D>& texture, const ImVec2& size,
                             const ImVec4& tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    BYTEFORGE_API void Image(const Ref<RenderTarget>& target, const ImVec2& size);

    BYTEFORGE_API bool ImageButton(const char* id, const Ref<Texture2D>& texture, const ImVec2& size,
                                   const ImVec4& tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                                   const ImVec4& background = ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    BYTEFORGE_API void DrawImage(ImDrawList& drawList, const Ref<Texture2D>& texture,
                                 const ImVec2& min, const ImVec2& max, ImU32 tint = IM_COL32_WHITE);
}
