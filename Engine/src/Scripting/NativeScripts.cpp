#include "Engine/Scripting/NativeScripts.h"
#include "Engine/Core/Log.h"
#include "Scripting/NativeScriptRegistry.h"

#include <stdexcept>

namespace ByteForge
{
    NativeScriptFactories& GetNativeScriptFactories()
    {
        static NativeScriptFactories factories;
        return factories;
    }

    void NativeScripts::RegisterFactory(std::string className, Factory factory)
    {
        if (className.empty() || !factory)
            throw std::runtime_error("NativeScripts::Register: the class name and factory must not be empty");

        NativeScriptFactories& factories = GetNativeScriptFactories();
        if (factories.contains(className))
            CORE_WARN("NativeScripts: '{}' is registered twice, the last registration wins", className);

        factories.insert_or_assign(std::move(className), std::move(factory));
    }
}
