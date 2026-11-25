#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/UUID.h"
#include "Engine/Scripting/Visual/ScriptGraph.h"

#include <string>
#include <vector>

namespace ByteForge
{
    struct GraphRuntimeError
    {
        UUID Node{ 0 };
        UUID Entity{ 0 };
        std::string Message;
    };

    class BYTEFORGE_API GraphDebugger
    {
    public:
        static void Watch(UUID graph);
        [[nodiscard]] static bool IsWatched(UUID graph);

        static void OnLinkTaken(UUID graph, const GraphLink& link);
        static void OnError(UUID graph, GraphRuntimeError error);

        [[nodiscard]] static std::vector<GraphLink> TakeLinks();

        [[nodiscard]] static std::vector<GraphRuntimeError> GetErrors(UUID graph);
        static void ClearErrors();
    };
}
