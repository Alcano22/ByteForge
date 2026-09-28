#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Texture2D.h"

#include <imgui.h>

namespace ByteForge::UI
{
    BYTEFORGE_API void Image(const Ref<Texture2D>& texture, const ImVec2& size);
    BYTEFORGE_API bool ImageButton(const char* id, const Ref<Texture2D>& texture, const ImVec2& size);
}
