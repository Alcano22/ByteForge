#ifndef _WIN32
#define __EMULATE_UUID 1
#endif

#include "Platform/Vulkan/VulkanShaderCompiler.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Engine/Core/Log.h"
#include "Renderer/ShaderSource.h"

#include <directx-dxc/dxcapi.h>

#include <stdexcept>
#include <format>
#include <string>
#include <filesystem>
#include <vector>

namespace ByteForge
{
    namespace
    {
        template<typename T>
        struct ComDeleter
        {
            void operator()(T* ptr) const { if (ptr) ptr->Release(); }
        };

        template<typename T>
        using ComScope = Scope<T, ComDeleter<T>>;

        template<typename T>
        ComScope<T> CreateDxcInstance(REFCLSID clsid)
        {
            T* raw = nullptr;
            if (FAILED(DxcCreateInstance(clsid, __uuidof(T), reinterpret_cast<void**>(&raw))))
                throw std::runtime_error("Failed to create DXC instance");

            return ComScope<T>(raw, ComDeleter<T>{});
        }

        const wchar_t* TargetProfile(const ShaderStage stage)
        {
            return stage == ShaderStage::Vertex ? L"vs_6_0" : L"ps_6_0";
        }

        const char* StageName(const ShaderStage stage)
        {
            return stage == ShaderStage::Vertex ? "vertex" : "fragment";
        }
    }

    struct VulkanShaderCompiler::Impl
    {
        ComScope<IDxcUtils> Utils;
        ComScope<IDxcCompiler3> Compiler;
        ComScope<IDxcIncludeHandler> IncludeHandler;
    };

    VulkanShaderCompiler& VulkanShaderCompiler::Get()
    {
        static VulkanShaderCompiler instance;
        return instance;
    }

    VulkanShaderCompiler::VulkanShaderCompiler()
        : m_Impl(MakeScope<Impl>())
    {
        m_Impl->Utils = CreateDxcInstance<IDxcUtils>(CLSID_DxcUtils);
        m_Impl->Compiler = CreateDxcInstance<IDxcCompiler3>(CLSID_DxcCompiler);

        IDxcIncludeHandler* rawHandler = nullptr;
        if (FAILED(m_Impl->Utils->CreateDefaultIncludeHandler(&rawHandler)))
            throw std::runtime_error("Failed to create DXC include handler");
        m_Impl->IncludeHandler = ComScope<IDxcIncludeHandler>(rawHandler, ComDeleter<IDxcIncludeHandler>{});

        CORE_INFO("DXC shader compiler initialized");
    }

    VulkanShaderCompiler::~VulkanShaderCompiler() = default;

    std::vector<uint32_t> VulkanShaderCompiler::Compile(const ShaderSource& source, const ShaderStage stage,
                                                        const std::string& entryPoint) const
    {
        const bool fromFile = !source.Path.empty();

        const std::wstring wEntryPoint(entryPoint.begin(), entryPoint.end());

        const std::wstring path = source.Path.wstring();

        const std::filesystem::path directory = source.Path.parent_path();
        const std::wstring includeDirectory = directory.empty() ? std::wstring(L".") : directory.wstring();

        std::vector<std::wstring> defines;
        defines.reserve(source.Defines.size());
        for (const std::string& define : source.Defines)
            defines.emplace_back(define.begin(), define.end());

        std::vector<LPCWSTR> arguments;
        if (fromFile)
            arguments.push_back(path.c_str());

        arguments.insert(arguments.end(), {
            L"-E", wEntryPoint.c_str(),
            L"-T", TargetProfile(stage),
            L"-spirv",
            L"-fspv-target-env=vulkan1.3"
        });

        if (fromFile)
        {
            arguments.push_back(L"-I");
            arguments.push_back(includeDirectory.c_str());
        }

        for (const std::wstring& define : defines)
        {
            arguments.push_back(L"-D");
            arguments.push_back(define.c_str());
        }

        const DxcBuffer sourceBuffer{
            .Ptr      = source.Code.data(),
            .Size     = source.Code.size(),
            .Encoding = DXC_CP_UTF8
        };

        IDxcResult* rawResult = nullptr;
        if (FAILED(m_Impl->Compiler->Compile(&sourceBuffer,
                                             arguments.data(),
                                             static_cast<UINT32>(arguments.size()),
                                             m_Impl->IncludeHandler.get(),
                                             IID_PPV_ARGS(&rawResult))))
            throw std::runtime_error("DXC compilation call failed");

        const ComScope<IDxcResult> result(rawResult, ComDeleter<IDxcResult>{});

        IDxcBlobUtf8* rawErrors = nullptr;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&rawErrors), nullptr);
        const ComScope<IDxcBlobUtf8> errors(rawErrors, ComDeleter<IDxcBlobUtf8>{});

        std::string errorText;
        if (errors && errors->GetStringLength() > 0)
            errorText = errors->GetStringPointer();

        HRESULT compileStatus = S_OK;
        result->GetStatus(&compileStatus);
        if (FAILED(compileStatus))
        {
            const std::string name = fromFile ? source.Path.filename().string() : std::string("<inline>");
            throw std::runtime_error(std::format("Shader '{}' failed to compile its {} stage ({}):\n{}",
                                                 name, StageName(stage), entryPoint, errorText));
        }

        if (!errorText.empty())
            CORE_WARN("[DXC] {}", errorText);

        IDxcBlob* rawObject = nullptr;
        result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&rawObject), nullptr);
        const ComScope<IDxcBlob> object(rawObject, ComDeleter<IDxcBlob>{});

        const uint32_t* data = static_cast<const uint32_t*>(object->GetBufferPointer());
        const size_t wordCount = object->GetBufferSize() / sizeof(uint32_t);
        return { data, data + wordCount };
    }
}
