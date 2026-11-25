#pragma once

#include <Engine/Scripting/API/ScriptEnum.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    class GraphEnumPicker
    {
    public:
        void Open(std::string_view enumName, i32 current, f32 width);

        [[nodiscard]] std::optional<i32> Draw();

    private:
        const ScriptEnum* m_Enum = nullptr;
        std::vector<std::string> m_Labels;
        i32 m_Current = 0;
        f32 m_Width = 0.0f;
        std::string m_Search;
        bool m_FocusSearch = false;
        bool m_ScrollToCurrent = false;
    };
}
