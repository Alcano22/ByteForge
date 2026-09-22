#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Renderer/Renderer2D.h"

#include <entt/entt.hpp>

#include <string>

namespace ByteForge
{
    class Entity;

    class BYTEFORGE_API Scene : NonCopyable
    {
    public:
        Scene() = default;

        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(Entity entity);

        void OnUpdate(Renderer2D& renderer2D, const Camera& camera);

    private:
        friend class Entity;

        entt::registry m_Registry;
    };
}
