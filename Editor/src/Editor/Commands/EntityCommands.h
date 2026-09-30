#pragma once

#include "Editor/Commands/EditorCommand.h"

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
        CreateEntityCommand(Scene& scene, std::string name, nlohmann::json snapshot);

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetName() const override { return m_Name; }

    private:
        Scene& m_Scene;
        std::string m_Name;
        nlohmann::json m_Snapshot;
    };

    class DestroyEntityCommand final : public EditorCommand
    {
    public:
        DestroyEntityCommand(Scene& scene, std::string name, nlohmann::json snapshot);

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetName() const override { return m_Name; }

    private:
        Scene& m_Scene;
        std::string m_Name;
        nlohmann::json m_Snapshot;
    };
}
