#pragma once

#include "Scripting/CSharp/NativeAPI.h"

#include <string>

namespace ByteForge
{
    class Scene;

    namespace ScriptGlue
    {
        [[nodiscard]] NativeAPI CreateNativeAPI();

        void SetScene(Scene* scene);

        [[nodiscard]] std::string TakeManagedException();
    }
}
