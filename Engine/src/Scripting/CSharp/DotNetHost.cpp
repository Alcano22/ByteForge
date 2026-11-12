#include "Scripting/CSharp/DotNetHost.h"

#include "Engine/Core/Log.h"
#include "Engine/Core/Platform.h"

#include <nethost.h>

#include <array>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

namespace ByteForge
{
    namespace
    {
        std::basic_string<char_t> ToHostString(const std::string_view text)
        {
            return { text.begin(), text.end() };
        }

        std::string FromHostString(const char_t* text)
        {
            return fs::path(text).string();
        }

        template<typename Fn>
        Fn Resolve(const SharedLibrary& library, const char* name)
        {
            auto* function = reinterpret_cast<Fn>(library.GetSymbol(name));
            if (function == nullptr)
                throw std::runtime_error(std::format("DotNetHost: hostfxr has no export '{}'", name));
            return function;
        }

        void HOSTFXR_CALLTYPE WriteHostError(const char_t* message)
        {
            CORE_ERROR("[.NET Host] {}", FromHostString(message));
        }

        fs::path FindBundledRuntime()
        {
            const fs::path root = Platform::GetExecutableDirectory() / "dotnet";

            std::error_code error;
            return fs::is_directory(root / "host" / "fxr", error) ? root : fs::path();
        }
    }

    DotNetHost::DotNetHost(const fs::path& runtimeConfig)
    {
        const fs::path dotnetRoot = FindBundledRuntime();
        const bool bundled = !dotnetRoot.empty();

        const get_hostfxr_parameters pathParameters{
            .size          = sizeof(get_hostfxr_parameters),
            .assembly_path = nullptr,
            .dotnet_root   = dotnetRoot.c_str()
        };

        std::array<char_t, 4096> hostFxrPath{};
        usize size = hostFxrPath.size();
        const i32 pathResult = get_hostfxr_path(hostFxrPath.data(), &size, bundled ? &pathParameters : nullptr);
        if (pathResult != 0)
        {
            throw std::runtime_error(std::format("DotNetHost: no .NET runtime found (0{:08X})",
                                                 static_cast<u32>(pathResult)));
        }

        m_HostFxr = MakeScope<SharedLibrary>(fs::path(hostFxrPath.data()));

        const auto setErrorWriter = Resolve<hostfxr_set_error_writer_fn>(*m_HostFxr, "hostfxr_set_error_writer");
        const auto initialize = Resolve<hostfxr_initialize_for_runtime_config_fn>(
            *m_HostFxr, "hostfxr_initialize_for_runtime_config");
        const auto getDelegate = Resolve<hostfxr_get_runtime_delegate_fn>(*m_HostFxr, "hostfxr_get_runtime_delegate");
        m_Close = Resolve<hostfxr_close_fn>(*m_HostFxr, "hostfxr_close");

        setErrorWriter(&WriteHostError);

        const hostfxr_initialize_parameters initParameters{
            .size        = sizeof(hostfxr_initialize_parameters),
            .host_path   = nullptr,
            .dotnet_root = dotnetRoot.c_str()
        };

        const i32 initResult = initialize(runtimeConfig.c_str(), bundled ? &initParameters : nullptr, &m_Context);
        if (initResult < 0 || m_Context == nullptr)
        {
            if (m_Context != nullptr)
                m_Close(m_Context);

            throw std::runtime_error(std::format("DotNetHost: cannot start the runtime from '{}' (0x{:08X})",
                                                 runtimeConfig.string(), static_cast<u32>(initResult)));
        }

        void* loader = nullptr;
        const i32 delegateResult = getDelegate(m_Context, hdt_load_assembly_and_get_function_pointer, &loader);
        if (delegateResult != 0 || loader == nullptr)
        {
            m_Close(m_Context);
            throw std::runtime_error(std::format("DotNetHost: cannot get the assembly loader (0x{:08X})",
                                                 static_cast<u32>(delegateResult)));
        }

        m_LoadAssemblyAndGetFunctionPointer = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(loader);

        CORE_INFO(".NET runtime started ({}, {})",
                  runtimeConfig.filename().string(), bundled ? "bundled" : "installed");
    }

    DotNetHost::~DotNetHost()
    {
        if (m_Context != nullptr)
            m_Close(m_Context);

        static_cast<void>(m_HostFxr.release());
    }

    void* DotNetHost::GetFunction(const fs::path& assembly, const std::string_view typeName,
                                  const std::string_view methodName) const
    {
        const std::basic_string<char_t> type = ToHostString(typeName);
        const std::basic_string<char_t> method = ToHostString(methodName);

        void* function = nullptr;
        const i32 result = m_LoadAssemblyAndGetFunctionPointer(assembly.c_str(), type.c_str(), method.c_str(),
                                                               UNMANAGEDCALLERSONLY_METHOD, nullptr, &function);
        if (result != 0 || function == nullptr)
        {
            throw std::runtime_error(std::format("DotNetHost: cannot find {}.{} in '{}' (0x{:08X})",
                                                 typeName, methodName, assembly.string(),
                                                 static_cast<u32>(result)));
        }

        return function;
    }
}
