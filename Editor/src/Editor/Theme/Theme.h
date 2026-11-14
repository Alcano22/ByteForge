#pragma once

#include <Engine/Core/Core.h>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace ByteForge
{
    struct Theme
    {
        std::string Name;
        bool BuiltIn = false;
        std::array<ImVec4, ImGuiCol_COUNT> Colors{};

        std::filesystem::path File;
        bool Modified = false;

        [[nodiscard]] static Theme FromStyle(std::string name, const ImGuiStyle& style, bool builtIn);

        void Apply() const;

        [[nodiscard]] nlohmann::json ToJson() const;

        [[nodiscard]] static std::optional<Theme> FromJson(const nlohmann::json& json, std::string fallbackName,
                                                           const Theme& base);
    };
}
