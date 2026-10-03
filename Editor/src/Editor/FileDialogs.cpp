#include "Editor/FileDialogs.h"

#include <Engine/Core/Log.h>

#include <imgui.h>
#include <nfd.h>

#include <string_view>
#include <vector>

namespace ByteForge::FileDialogs
{
    namespace
    {
        class Session
        {
        public:
            Session()
                : m_Ready(NFD_Init() == NFD_OKAY)
            {
                if (!m_Ready)
                    APP_ERROR("File dialogs are unavailable: {}", NFD_GetError());
            }

            ~Session()
            {
                if (m_Ready)
                    NFD_Quit();
            }

            Session(const Session&) = delete;
            Session& operator=(const Session&) = delete;

            [[nodiscard]] bool IsReady() const { return m_Ready; }

        private:
            bool m_Ready;
        };

        std::vector<nfdu8filteritem_t> ToNfdFilters(const std::span<const FileFilter> filters)
        {
            std::vector<nfdu8filteritem_t> result;
            result.reserve(filters.size());
            for (const FileFilter& filter : filters)
                result.emplace_back(filter.Name, filter.Extensions);
            return result;
        }

        std::string ToUtf8(const std::filesystem::path& path)
        {
            const std::u8string text = path.u8string();
            return { text.begin(), text.end() };
        }

        void ResetImGuiInput()
        {
            ImGuiIO& io = ImGui::GetIO();
            io.ClearInputKeys();
            io.ClearInputMouse();
        }

        std::optional<std::filesystem::path> Finish(const nfdresult_t result, nfdu8char_t* path)
        {
            ResetImGuiInput();

            if (result == NFD_OKAY)
            {
                std::filesystem::path chosen(std::u8string_view(reinterpret_cast<const char8_t*>(path)));
                NFD_FreePathU8(path);
                return chosen;
            }

            if (result == NFD_ERROR)
                APP_ERROR("File dialog failed: {}", NFD_GetError());

            return std::nullopt;
        }
    }

    std::optional<std::filesystem::path> OpenFile(const std::span<const FileFilter> filters,
                                                  const std::filesystem::path& defaultDirectory)
    {
        const Session session;
        if (!session.IsReady())
            return std::nullopt;

        const std::vector<nfdu8filteritem_t> nfdFilters = ToNfdFilters(filters);
        const std::string directory = ToUtf8(defaultDirectory);

        nfdu8char_t* path = nullptr;
        const nfdresult_t result = NFD_OpenDialogU8(&path, nfdFilters.data(),
                                                    static_cast<nfdfiltersize_t>(nfdFilters.size()),
                                                    directory.empty() ? nullptr : directory.c_str());
        return Finish(result, path);
    }

    std::optional<std::filesystem::path> SaveFile(const std::span<const FileFilter> filters,
                                                  const std::filesystem::path& defaultDirectory,
                                                  const std::string& defaultName)
    {
        const Session session;
        if (!session.IsReady())
            return std::nullopt;

        const std::vector<nfdu8filteritem_t> nfdFilters = ToNfdFilters(filters);
        const std::string directory = ToUtf8(defaultDirectory);

        nfdu8char_t* path = nullptr;
        const nfdresult_t result = NFD_SaveDialogU8(&path, nfdFilters.data(),
                                                    static_cast<nfdfiltersize_t>(nfdFilters.size()),
                                                    directory.empty() ? nullptr : directory.c_str(),
                                                    defaultName.empty() ? nullptr : defaultName.c_str());
        return Finish(result, path);
    }
}
