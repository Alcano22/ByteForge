#include "Engine/Core/Version.h"

#ifndef BYTEFORGE_VERSION
    #error "BYTEFORGE_VERSION is defined by Engine/CMakeLists.txt"
#endif

namespace ByteForge
{
    std::string_view GetEngineVersion() { return BYTEFORGE_VERSION; }
}
