#pragma once

#include "Engine/Scripting/NativeScripts.h"

#include <functional>
#include <map>
#include <string>

namespace ByteForge
{
    using NativeScriptFactories = std::map<std::string, NativeScripts::Factory, std::less<>>;

    [[nodiscard]] NativeScriptFactories& GetNativeScriptFactories();
}
