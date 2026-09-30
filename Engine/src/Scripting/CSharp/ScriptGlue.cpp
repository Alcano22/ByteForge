#include "Scripting/CSharp/ScriptGlue.h"

#include "Engine/Core/Log.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"

#include <utility>

namespace ByteForge
{
    namespace
    {
        Scene* s_Scene = nullptr;
        thread_local std::string t_LastException;

        TransformComponent* FindTransform(const uint64_t entity)
        {
            if (s_Scene == nullptr)
                return nullptr;
            return s_Scene->FindEntityByUUID(UUID(entity)).TryGetComponent<TransformComponent>();
        }

        void LogMessage(const int level, const char* message)
        {
            try
            {
                switch (level)
                {
                    case 0:  APP_TRACE("{}", message); break;
                    case 1:  APP_INFO("{}", message);  break;
                    case 2:  APP_WARN("{}", message);  break;
                    default: APP_ERROR("{}", message); break;
                }
            } catch (...) {}
        }

        void ReportException(const char* message)
        {
            try
            {
                t_LastException = message != nullptr ? message : "";
            } catch (...) {}
        }

        int EntityIsValid(const uint64_t entity)
        {
            return s_Scene != nullptr && s_Scene->FindEntityByUUID(UUID(entity)).IsValid() ? 1 : 0;
        }

        int TransformGetPosition(const uint64_t entity, glm::vec3* out)
        {
            const TransformComponent* transform = FindTransform(entity);
            if (transform == nullptr || out == nullptr)
                return 0;

            *out = transform->Position;
            return 1;
        }

        int TransformSetPosition(const uint64_t entity, const glm::vec3* value)
        {
            TransformComponent* transform = FindTransform(entity);
            if (transform == nullptr || value == nullptr)
                return 0;

            transform->Position = *value;
            return 1;
        }

        int TransformGetRotation(const uint64_t entity, float* out)
        {
            const TransformComponent* transform = FindTransform(entity);
            if (transform == nullptr || out == nullptr)
                return 0;

            *out = transform->Rotation;
            return 1;
        }

        int TransformSetRotation(const uint64_t entity, const float value)
        {
            TransformComponent* transform = FindTransform(entity);
            if (transform == nullptr)
                return 0;

            transform->Rotation = value;
            return 1;
        }

        int TransformGetScale(const uint64_t entity, glm::vec2* out)
        {
            const TransformComponent* transform = FindTransform(entity);
            if (transform == nullptr || out == nullptr)
                return 0;

            *out = transform->Scale;
            return 1;
        }

        int TransformSetScale(const uint64_t entity, const glm::vec2* value)
        {
            TransformComponent* transform = FindTransform(entity);
            if (transform == nullptr || value == nullptr)
                return 0;

            transform->Scale = *value;
            return 1;
        }
    }

    namespace ScriptGlue
    {
        NativeAPI CreateNativeAPI()
        {
            return NativeAPI{
                .Size                  = sizeof(NativeAPI),
                .Log                   = &LogMessage,
                .ReportException       = &ReportException,
                .Entity_IsValid        = &EntityIsValid,
                .Transform_GetPosition = &TransformGetPosition,
                .Transform_SetPosition = &TransformSetPosition,
                .Transform_GetRotation = &TransformGetRotation,
                .Transform_SetRotation = &TransformSetRotation,
                .Transform_GetScale    = &TransformGetScale,
                .Transform_SetScale    = &TransformSetScale
            };
        }

        void SetScene(Scene* scene) { s_Scene = scene; }

        std::string TakeManagedException()
        {
            std::string message = std::move(t_LastException);
            t_LastException.clear();
            return message.empty() ? std::string("unknown C# exception") : message;
        }
    }
}
