#include "Engine/Animation/AnimationSystem.h"
#include "Engine/Assets/AnimatorController.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"

#include <cmath>

namespace ByteForge
{
    void AnimationSystem::Update(Scene& scene, const f32 deltaTime)
    {
        scene.Each<AnimatorComponent>([deltaTime](const Entity entity, AnimatorComponent& animator)
        {
            if (static_cast<u64>(animator.m_LoadedController) != static_cast<u64>(animator.Controller))
            {
                animator.ResolveController();
                if (animator.PlayOnStart)
                    animator.EnterDefaultState();
            }

            if (!animator.m_Controller || !animator.m_State) return;

            if (*animator.m_State >= animator.m_Controller->GetStateCount())
            {
                animator.EnterDefaultState();
                if (!animator.m_State) return;
            }

            const Ref<AnimationClipAsset>& clip = animator.m_Controller->GetClip(*animator.m_State);
            if (!clip || clip->IsEmpty()) return;

            if (auto* renderer = entity.TryGetComponent<SpriteRendererComponent>())
                renderer->Sprite = clip->GetSprite(clip->SampleFrame(animator.m_Time));

            if (!animator.m_Playing) return;

            const f32 speed = animator.Speed * animator.m_Controller->GetState(*animator.m_State).Speed;
            animator.m_Time += deltaTime * speed;

            if (clip->GetData().LoopMode == AnimationLoopMode::Once)
            {
                if (clip->IsFinished(animator.m_Time))
                    animator.m_Playing = false;
            } else if (const f32 duration = clip->GetDuration(); duration > 0.0f)
                animator.m_Time = std::fmod(animator.m_Time, 2.0f * duration);
        });
    }
}
