#include "Scripting/CSharp/ScriptGlue.h"
#include "Scripting/CSharp/ManagedValue.h"

#include "Engine/Core/Log.h"
#include "Engine/Scripting/API/ScriptAPI.h"

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace
    {
        Scene* s_Scene = nullptr;
        thread_local std::string t_LastException;
        thread_local std::string t_LastError;

        thread_local std::optional<ScriptValue> t_LastResult;

        void LogMessage(const int level, const char* message)
        {
            try
            {
                switch (level)
                {
                    case 0:  APP_TRACE("{}", message); break;
                    case 1:  APP_INFO ("{}", message); break;
                    case 2:  APP_WARN ("{}", message); break;
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

        void SetLastError(const char* message) noexcept
        {
            try
            {
                t_LastError = message;
            } catch (...) {}
        }

        uint32_t FindFunction(const char* id, const char* signature)
        {
            try
            {
                const ScriptAPI& api = ScriptAPI::Get();
                const uint32_t index = api.FindIndex(id != nullptr ? id : "");
                if (index == ScriptAPI::InvalidIndex)
                    return index;

                const std::string actual = GetSignature(api.GetFunction(index));
                if (signature == nullptr || actual != signature)
                {
                    CORE_ERROR("Script API '{}': ScriptCore expects {}, the engine provides {}",
                               id, signature != nullptr ? signature : "?", actual);
                    return ScriptAPI::InvalidIndex;
                }

                return index;
            } catch (...)
            {
                return ScriptAPI::InvalidIndex;
            }
        }

        int InvokeFunction(const uint32_t index, const void* arguments, const int argumentCount, void* result)
        {
            try
            {
                const ScriptFunction& function = ScriptAPI::Get().GetFunction(index);
                if (argumentCount < 0 || static_cast<size_t>(argumentCount) != function.Parameters.size())
                    throw ScriptError(std::format("{} expects {} argument(s), got {}",
                                                  function.Id, function.Parameters.size(), argumentCount));

                const auto* slots = static_cast<const std::byte*>(arguments);
                std::vector<ScriptValue> values;
                values.reserve(function.Parameters.size());

                for (size_t i = 0; i < function.Parameters.size(); ++i)
                {
                    values.push_back(ManagedValue::Read(function.Parameters[i].Type,
                                                        slots + i * ManagedValue::SlotSize));
                }

                ValidateArguments(function, values);

                t_LastResult = function.Invoke(ScriptCallContext{ .ActiveScene = s_Scene }, values);
                if (t_LastResult && result != nullptr)
                    ManagedValue::Write(*t_LastResult, result);

                return 0;
            } catch (const std::exception& e)
            {
                SetLastError(e.what());
            } catch (...)
            {
                SetLastError("Unknown engine error");
            }
            return 1;
        }

        const char* GetLastError() { return t_LastError.c_str(); }
    }

    namespace ScriptGlue
    {
        NativeAPI CreateNativeAPI()
        {
            return NativeAPI{
                .Size             = sizeof(NativeAPI),
                .Log              = &LogMessage,
                .ReportException  = &ReportException,
                .Api_FindFunction = &FindFunction,
                .Api_Invoke       = &InvokeFunction,
                .Api_GetLastError = &GetLastError
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
