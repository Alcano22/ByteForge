#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Renderer/RenderTarget.h"
#include "Engine/Renderer/DrawRange.h"
#include "Engine/Renderer/SceneLighting.h"

#include <cstddef>
#include <span>
#include <type_traits>

namespace ByteForge
{
    class BYTEFORGE_API Renderer
    {
    public:
        static void BeginFrame();

        static void BeginScene(const Camera& camera, const SceneLighting& lighting = {});

        static void Submit(const Ref<Material>& material, const Ref<Mesh>& mesh, const DrawRange& range = {})
        {
            SubmitRaw(material, mesh, {}, range);
        }

        template<typename T>
        static void Submit(const Ref<Material>& material, const Ref<Mesh>& mesh,
                           const T& pushConstants, const DrawRange& range = {})
        {
            static_assert(std::is_trivially_copyable_v<T>, "Push constant data must be trivially copyable");
            SubmitRaw(material, mesh, std::as_bytes(std::span(&pushConstants, 1)), range);
        }

        static void SubmitRaw(const Ref<Material>& material, const Ref<Mesh>& mesh,
                              std::span<const std::byte> pushConstants, const DrawRange& range = {});

        static void EndFrame();

        static void BeginRenderTarget(const Ref<RenderTarget>& target);
        static void EndRenderTarget();

        static void OnFramebufferResized();

        static void WaitIdle();

        [[nodiscard]] static uint64_t GetFrameNumber();
        [[nodiscard]] static bool IsSrgb(ImageFormat format);
    };
}
