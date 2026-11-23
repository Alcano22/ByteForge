#include "Engine/Assets/AssetType.h"
#include "Engine/Assets/Prefab.h"
#include "Engine/Assets/PhysicsMaterial.h"
#include "Engine/Assets/AnimationClip.h"
#include "Engine/Assets/AnimatorController.h"
#include "Engine/Assets/InputActions.h"
#include "Engine/Assets/MaterialAsset.h"
#include "Engine/Scripting/Visual/ScriptGraph.h"
#include "Engine/Core/StringUtils.h"

#include <magic_enum/magic_enum.hpp>

namespace ByteForge
{
    const char* AssetTypeToString(const AssetType type)
    {
        return magic_enum::enum_name(type).data();
    }

    AssetType AssetTypeFromString(const std::string& value)
    {
        return magic_enum::enum_cast<AssetType>(value).value_or(AssetType::None);
    }

    AssetType AssetTypeFromExtension(const std::string& extension)
    {
        const std::string ext = ToLower(extension);

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
            return AssetType::Texture2D;
        if (ext == ".cs")
            return AssetType::Script;
        if (ext == ".bfscene")
            return AssetType::Scene;
        if (ext == PhysicsMaterialExtension)
            return AssetType::PhysicsMaterial2D;
        if (ext == ".wav" || ext == ".flac" || ext == ".mp3")
            return AssetType::AudioClip;
        if (ext == ScriptGraphExtension)
            return AssetType::ScriptGraph;
        if (ext == PrefabExtension)
            return AssetType::Prefab;
        if (ext == AnimationClipExtension)
            return AssetType::AnimationClip;
        if (ext == AnimatorControllerExtension)
            return AssetType::AnimatorController;
        if (ext == ".ttf" || ext == ".otf")
            return AssetType::Font;
        if (ext == InputActionsExtension)
            return AssetType::InputActions;
        if (ext == MaterialExtension)
            return AssetType::Material;

        return AssetType::None;
    }
}
