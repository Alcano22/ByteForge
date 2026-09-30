#pragma once

#include "Engine/Scripting/ScriptBackend.h"

namespace ByteForge
{
    class NativeScriptBackend final : public ScriptBackend
    {
    public:
        [[nodiscard]] std::string_view GetName() const override { return "C++"; }
        [[nodiscard]] std::vector<std::string> GetClassNames() const override;
        [[nodiscard]] bool HasClass(std::string_view className) const override;

        [[nodiscard]] Scope<ScriptInstance> CreateInstance(std::string_view className, Entity entity) override;
    };
}
