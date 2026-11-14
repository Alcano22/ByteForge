#pragma once

#include "Editor/Commands/EditorCommand.h"

#include <Engine/Scene/Components.h>
#include <Engine/Scene/UUID.h>

#include <nlohmann/json.hpp>

#include <string>

namespace ByteForge
{
    class Scene;

    class ModifyEntityCommand final : public EditorCommand
    {
    public:
        ModifyEntityCommand(Scene& scene, std::string name, nlohmann::json before, nlohmann::json after);

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetName() const override { return m_Name; }

    private:
        Scene& m_Scene;
        std::string m_Name;
        nlohmann::json m_Before;
        nlohmann::json m_After;
    };

    class CreateEntityCommand final : public EditorCommand
    {
    public:
        CreateEntityCommand(Scene& scene, std::string name, nlohmann::json subtree);

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetName() const override { return m_Name; }

    private:
        Scene& m_Scene;
        std::string m_Name;
        nlohmann::json m_Subtree;
    };

    class DestroyEntityCommand final : public EditorCommand
    {
    public:
        DestroyEntityCommand(Scene& scene, std::string name, nlohmann::json subtree);

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetName() const override { return m_Name; }

    private:
        Scene& m_Scene;
        std::string m_Name;
        nlohmann::json m_Subtree;
    };

    class ModifyTransformCommand final : public EditorCommand
    {
    public:
        ModifyTransformCommand(Scene& scene, UUID entityId, std::string name,
                               const TransformComponent& before, const TransformComponent& after);

        void Execute() override { Apply(m_After); }
        void Undo() override { Apply(m_Before); }
        [[nodiscard]] std::string GetName() const override { return m_Name; }

    private:
        void Apply(const TransformComponent& transform) const;

    private:
        Scene& m_Scene;
        UUID m_EntityId;
        std::string m_Name;
        TransformComponent m_Before;
        TransformComponent m_After;
    };

    class SetParentCommand final : public EditorCommand
    {
    public:
        SetParentCommand(Scene& scene, UUID entityId, std::string name,
                         UUID oldParent, i32 oldIndex, const TransformComponent& before,
                         UUID newParent, i32 newIndex, const TransformComponent& after);

        void Execute() override { Apply(m_NewParent, m_NewIndex, m_After); }
        void Undo() override { Apply(m_OldParent, m_OldIndex, m_Before); }
        [[nodiscard]] std::string GetName() const override { return m_Name; }

    private:
        void Apply(UUID parentId, i32 index, const TransformComponent& local) const;

    private:
        Scene& m_Scene;
        UUID m_EntityId;
        std::string m_Name;
        UUID m_OldParent;
        i32 m_OldIndex;
        TransformComponent m_Before;
        UUID m_NewParent;
        i32 m_NewIndex;
        TransformComponent m_After;
    };
}
