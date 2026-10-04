#pragma once

#include "Engine/Scripting/ScriptBackend.h"
#include "Scripting/Visual/GraphProgram.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>

namespace ByteForge
{
    class GraphScriptBackend final : public ScriptBackend
    {
    public:
        [[nodiscard]] std::string_view GetName() const override { return "Graph"; }
        [[nodiscard]] std::vector<std::string> GetClassNames() const override;
        [[nodiscard]] bool HasClass(std::string_view className) const override;
        [[nodiscard]] std::span<const ScriptFieldInfo> GetFields(std::string_view className) const override;
        [[nodiscard]] std::string GetDisplayName(std::string_view className) const override;

        [[nodiscard]] Scope<ScriptInstance> CreateInstance(std::string_view className, Entity entity) override;

        void OnRuntimeStart(Scene& scene) override;

    private:
        struct GraphClass
        {
            std::string DisplayName;
            Ref<const GraphProgram> Program;
            std::string Error;
        };

        void Refresh(bool force) const;
        [[nodiscard]] const GraphClass* Find(std::string_view className) const;

    private:
        mutable std::map<std::string, GraphClass, std::less<>> m_Classes;
        mutable uint64_t m_Revision = std::numeric_limits<uint64_t>::max();
    };
}
