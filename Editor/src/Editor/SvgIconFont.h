#pragma once

#include <Engine/Core/NonCopyable.h>

#include <imgui.h>

#include <memory>
#include <unordered_map>

namespace lunasvg
{
    class Document;
}

namespace ByteForge
{
    class SvgIconFont : NonCopyable
    {
    public:
        SvgIconFont();
        ~SvgIconFont();

        void Add(ImWchar codepoint, std::unique_ptr<lunasvg::Document> document);

        void AddTo(ImFont* font);

        [[nodiscard]] const lunasvg::Document* Find(ImWchar codepoint) const;

    private:
        std::unordered_map<ImWchar, std::unique_ptr<lunasvg::Document>> m_Documents;
    };
}
