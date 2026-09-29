#pragma once

#include <Engine/Assets/AssetType.h>

#include <imgui.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>

namespace ByteForge
{
    inline constexpr const char* AssetPayloadType = "ByteForge.Asset";

    struct AssetPayload
    {
        uint64_t Handle = 0;
        AssetType Type = AssetType::None;
    };

    static_assert(std::is_trivially_copyable_v<AssetPayload>);

    [[nodiscard]] inline std::optional<AssetPayload> ReadAssetPayload(const ImGuiPayload* payload)
    {
        if (payload == nullptr || !payload->IsDataType(AssetPayloadType) ||
            payload->DataSize != static_cast<int>(sizeof(AssetPayload)))
            return std::nullopt;

        AssetPayload result;
        std::memcpy(&result, payload->Data, sizeof(result));
        return result;
    }
}
