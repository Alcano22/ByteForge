find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h" REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(box2d CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(unofficial-nethost CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h" REQUIRED)

add_library(Engine SHARED
        src/Core/Log.cpp
        src/Core/Application.cpp
        src/Core/Window.cpp
        src/Core/LayerStack.cpp
        src/Core/JobSystem.cpp
        src/Core/Platform.cpp
        src/Core/FileSystem.cpp
        src/Input/Input.cpp
        src/Scene/UUID.cpp
        src/Scene/Scene.cpp
        src/Scene/SceneSerializer.cpp
        src/Scene/Components.cpp
        src/Scene/Physics2DWorld.cpp
        src/Project/ProjectSettings.cpp
        src/Assets/AssetManager.cpp
        src/Assets/AssetType.cpp
        src/Assets/AssetRegistry.cpp
        src/Assets/AssetSettings.cpp
        src/Assets/Prefab.cpp
        src/Assets/Pak.cpp
        src/Audio/AudioEngine.cpp
        src/Audio/AudioSystem.cpp
        src/Audio/MiniAudioImpl.cpp
        src/Renderer/RendererAPI.cpp
        src/Renderer/SwapchainSettings.cpp
        src/Renderer/Buffer.cpp
        src/Renderer/UniformBuffer.cpp
        src/Renderer/Shader.cpp
        src/Renderer/Renderer.cpp
        src/Renderer/Camera.cpp
        src/Renderer/Pipeline.cpp
        src/Renderer/Material.cpp
        src/Renderer/RenderTarget.cpp
        src/Renderer/Texture2D.cpp
        src/Renderer/SubTexture2D.cpp
        src/Renderer/Renderer2D.cpp
        src/Renderer/OrthographicCamera.cpp
        src/Renderer/ImageDecoder.cpp
        src/Renderer/RenderBackend.cpp
        src/Renderer/LineRenderer.cpp
        src/Renderer/PerspectiveCamera.cpp
        src/Renderer/MeshPrimitives.cpp
        src/Renderer/MeshRenderer.cpp
        src/Renderer/ShaderLibrary.cpp
        src/ImGui/ImGuiLayer.cpp
        src/ImGui/ImGuiRenderer.cpp
        src/ImGui/ImGuiWidgets.cpp
        src/Scripting/ScriptEngine.cpp
        src/Scripting/ScriptRuntime.cpp
        src/Scripting/NativeScripts.cpp
        src/Scripting/NativeScriptBackend.cpp
        src/Scripting/ScriptValueJson.cpp
        src/Scripting/API/ScriptAPI.cpp
        src/Scripting/API/CoreAPI.cpp
        src/Scripting/CSharp/DotNetHost.cpp
        src/Scripting/CSharp/CSharpScriptBackend.cpp
        src/Scripting/CSharp/ScriptGlue.cpp
        src/Scripting/CSharp/ManagedValue.cpp
        src/Scripting/Visual/GraphSchema.cpp
        src/Scripting/Visual/ScriptGraph.cpp
        src/Scripting/Visual/ScriptGraphSerializer.cpp
        src/Scripting/Visual/GraphCompiler.cpp
        src/Scripting/Visual/GraphScriptInstance.cpp
        src/Scripting/Visual/GraphScriptBackend.cpp
        src/Platform/GraphicsContextFactory.cpp
        src/Platform/Vulkan/VulkanContext.cpp
        src/Platform/Vulkan/VulkanInstance.cpp
        src/Platform/Vulkan/VulkanDevice.cpp
        src/Platform/Vulkan/VulkanAllocator.cpp
        src/Platform/Vulkan/VulkanBuffer.cpp
        src/Platform/Vulkan/VulkanVertexBuffer.cpp
        src/Platform/Vulkan/VulkanIndexBuffer.cpp
        src/Platform/Vulkan/VulkanUniformBuffer.cpp
        src/Platform/Vulkan/VulkanSwapchain.cpp
        src/Platform/Vulkan/VulkanHelpers.cpp
        src/Platform/Vulkan/VulkanShaderCompiler.cpp
        src/Platform/Vulkan/VulkanShader.cpp
        src/Platform/Vulkan/VulkanShaderProgram.cpp
        src/Platform/Vulkan/VulkanShaderReflection.cpp
        src/Platform/Vulkan/VulkanPipeline.cpp
        src/Platform/Vulkan/VulkanCommandPool.cpp
        src/Platform/Vulkan/VulkanSyncObjects.cpp
        src/Platform/Vulkan/VulkanRenderer.cpp
        src/Platform/Vulkan/VulkanDescriptorSetLayout.cpp
        src/Platform/Vulkan/VulkanDescriptorPool.cpp
        src/Platform/Vulkan/VulkanImGuiRenderer.cpp
        src/Platform/Vulkan/VulkanFrameData.cpp
        src/Platform/Vulkan/VulkanDescriptorAllocator.cpp
        src/Platform/Vulkan/VulkanMaterial.cpp
        src/Platform/Vulkan/VulkanDeletionQueue.cpp
        src/Platform/Vulkan/VulkanUploader.cpp
        src/Platform/Vulkan/VulkanPerFrameBuffer.cpp
        src/Platform/Vulkan/VulkanImage.cpp
        src/Platform/Vulkan/VulkanRenderTarget.cpp
        src/Platform/Vulkan/VulkanTexture2D.cpp
        src/Platform/Vulkan/VulkanSamplerCache.cpp
        src/Platform/Vulkan/VulkanBackend.cpp
)

if (WIN32)
    target_sources(Engine PRIVATE src/Platform/Windows/WindowsPlatform.cpp)
elseif (APPLE)
    target_sources(Engine PRIVATE src/Platform/MacOS/MacOSPlatform.cpp src/Platform/Posix/PosixProcess.cpp)
elseif (UNIX)
    target_sources(Engine PRIVATE src/Platform/Linux/LinuxPlatform.cpp src/Platform/Posix/PosixProcess.cpp)
else()
    message(FATAL_ERROR "ByteForge: unsupported platform")
endif()

target_compile_definitions(Engine PRIVATE BYTEFORGE_BUILD_DLL)

target_compile_definitions(Engine PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        $<$<CONFIG:Debug>:BYTEFORGE_DEBUG>
)

target_include_directories(Engine
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Engine PUBLIC
        glfw
        glm::glm
        spdlog::spdlog
        imgui::imgui
        EnTT::EnTT
        nlohmann_json::nlohmann_json
        magic_enum::magic_enum
)

target_include_directories(Engine SYSTEM PRIVATE ${STB_INCLUDE_DIRS} ${MINIAUDIO_INCLUDE_DIRS})

target_link_libraries(Engine PRIVATE
        ByteForgeWarnings
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        unofficial::spirv-reflect
        box2d::box2d
        unofficial::nethost::nethost
        $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
        ${CMAKE_DL_LIBS}
)

target_include_directories(Engine SYSTEM PRIVATE
        $<TARGET_PROPERTY:Microsoft::DirectXShaderCompiler,INTERFACE_INCLUDE_DIRECTORIES>
)

if (UNIX AND NOT APPLE)
    add_custom_command(TARGET Engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:Microsoft::DirectXShaderCompiler>/libdxildll.so"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "libdxil.so"
            "$<TARGET_FILE_DIR:Microsoft::DirectXShaderCompiler>/libdxildll.so"
        COMMENT "Working around broken libdxildll.so SONAME in directx-dxc"
    )

    add_custom_command(TARGET Engine POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=$<TARGET_FILE_DIR:Microsoft::DirectXShaderCompiler>
            -DDESTINATION=$<TARGET_FILE_DIR:Engine>
            -P ${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeLibraries.cmake
        COMMENT "Copying the shader compiler next to libEngine"
        VERBATIM
    )
    set_target_properties(Engine PROPERTIES BUILD_RPATH "$ORIGIN")
endif()

set_target_properties(Engine PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

set(BYTEFORGE_PCH_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/Engine/Core/PCH.h
        CACHE INTERNAL "Precompiled header of ByteForge")
target_precompile_headers(Engine PRIVATE ${BYTEFORGE_PCH_HEADER})

set(BYTEFORGE_ENGINE_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources
        CACHE INTERNAL "${CMAKE_CURRENT_BINARY_DIR}/${target}_${name}.stamp")

function(byteforge_add_engine_resources target)
    byteforge_copy_directory(${target}
            ${BYTEFORGE_ENGINE_RESOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}/engine
    )
endfunction()
