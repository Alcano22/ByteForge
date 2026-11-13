#pragma once

#include "Engine/Core/Core.h"

namespace ByteForge
{
    class Scene;

    class AnimationSystem
    {
    public:
        static void Update(Scene& scene, f32 deltaTime);
    };
}
