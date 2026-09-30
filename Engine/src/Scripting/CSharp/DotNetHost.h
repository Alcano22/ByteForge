#pragma once

#include "Engine/Core/Core.h"
#include "Platform/SharedLibrary.h"

#include <coreclr_delegates.h>
#include <hostfxr.h>

#include <filesystem>
#include <string_view>

namespace ByteForge
{
    class DotNetHost
    {
    public:
        explicit DotNetHost(const std::filesystem::path& runtimeConfig);
        ~DotNetHost();

        DotNetHost(const DotNetHost&) = delete;
        DotNetHost& operator=(const DotNetHost&) = delete;

        [[nodiscard]] void* GetFunction(const std::filesystem::path& assembly, std::string_view typeName,
                                        std::string_view methodName) const;

        template<typename Fn>
        [[nodiscard]] Fn GetFunction(const std::filesystem::path& assembly, const std::string_view typeName,
                                     const std::string_view methodName) const
        {
            return reinterpret_cast<Fn>(GetFunction(assembly, typeName, methodName));
        }

    private:
        Scope<SharedLibrary> m_HostFxr;
        hostfxr_handle m_Context = nullptr;
        hostfxr_close_fn m_Close = nullptr;
        load_assembly_and_get_function_pointer_fn m_LoadAssemblyAndGetFunctionPointer = nullptr;
    };
}
