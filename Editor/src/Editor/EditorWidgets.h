#pragma once

#include "Editor/Selection.h"

#include <Engine/Assets/TextureAsset.h>

namespace ByteForge::EditorUI
{
    bool TextureAssetField(const char* label, Ref<TextureAsset>& asset, Selection& selection);
}
