#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/UUID.h"
#include "Engine/Scripting/Visual/ScriptGraph.h"

#include <glm/glm.hpp>

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    struct PastedGraphNodes
    {
        std::vector<UUID> Nodes;
        usize SkippedEvents = 0;
    };

    [[nodiscard]] BYTEFORGE_API std::string CopyGraphNodes(const ScriptGraph& graph, std::span<const UUID> nodes);

    [[nodiscard]] BYTEFORGE_API Result<PastedGraphNodes> PasteGraphNodes(ScriptGraph& graph, std::string_view text,
                                                                         glm::vec2 position);
}
