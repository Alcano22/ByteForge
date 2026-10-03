#include "Editor/Panels/AssetsPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/AssetPayload.h"

#include <Engine/Core/Log.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/ImGui/ImGuiWidgets.h>

#include <algorithm>
#include <exception>
#include <system_error>
#include <string>
#include <string_view>
#include <vector>
#include <cstring>
#include <utility>
#include <expected>

namespace ByteForge
{
    namespace fs = std::filesystem;

    namespace
    {
        bool IsMetaFile(const fs::path& path) { return path.extension() == ".meta"; }

        bool IsEmptyDirectory(const fs::path& path)
        {
            std::error_code ec;
            for (const auto& child : fs::directory_iterator(path, ec))
            {
                if (!IsMetaFile(child.path()))
                    return false;
            }
            return true;
        }

        ImVec2 FitToSquare(const Texture2D& texture, const float size)
        {
            const float width = static_cast<float>(texture.GetWidth());
            const float height = static_cast<float>(texture.GetHeight());
            const float scale = size / std::max(width, height);
            return { width * scale, height * scale };
        }

        constexpr int MaxLabelLines = 2;
        constexpr const char* Ellipsis = "\xE2\x80\xA6";

        size_t NextCharEnd(const std::string_view text, size_t pos)
        {
            ++pos;
            while (pos < text.size() && (static_cast<unsigned char>(text[pos]) & 0xC0) == 0x80)
                ++pos;
            return pos;
        }

        size_t PreviousCharStart(const std::string_view text, size_t pos)
        {
            while (pos > 0)
            {
                --pos;
                if ((static_cast<unsigned char>(text[pos]) & 0xC0) != 0x80) break;
            }
            return pos;
        }

        float TextWidth(const std::string_view text)
        {
            return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
        }

        bool IsBreakAfter(const char c) { return c == ' ' || c == '_' || c == '-'; }

        std::vector<std::string> LayoutLabel(std::string_view text, const float maxWidth, const int maxLines)
        {
            std::vector<std::string> lines;

            while (!text.empty() && static_cast<int>(lines.size()) < maxLines)
            {
                size_t fit = 0;
                for (size_t end = NextCharEnd(text, 0);
                     TextWidth(text.substr(0, end)) <= maxWidth;
                     end = NextCharEnd(text, end))
                {
                    fit = end;
                    if (end >= text.size()) break;
                }

                if (fit >= text.size())
                {
                    lines.emplace_back(text);
                    break;
                }

                if (static_cast<int>(lines.size()) == maxLines - 1)
                {
                    const float ellipsisWidth = TextWidth(Ellipsis);
                    while (fit > 0 && TextWidth(text.substr(0, fit)) + ellipsisWidth > maxWidth)
                        fit = PreviousCharStart(text, fit);

                    lines.push_back(std::string(text.substr(0, fit)) + Ellipsis);
                    break;
                }

                size_t lineEnd = fit;
                for (size_t i = fit; i > 0; --i)
                {
                    if (!IsBreakAfter(text[i - 1])) continue;

                    lineEnd = i;
                    break;
                }

                if (lineEnd == 0)
                    lineEnd = NextCharEnd(text, 0);

                lines.emplace_back(text.substr(0, lineEnd));
                text.remove_prefix(lineEnd);

                while (!text.empty() && text.front() == ' ')
                    text.remove_prefix(1);
            }

            return lines;
        }

        std::string GetDisplayName(const std::filesystem::path& path, const bool isDirectory)
        {
            return isDirectory ? path.filename().string() : path.stem().string();
        }

        const char* ValidateFileName(const std::string_view name)
        {
            if (name.empty())
                return "the name is empty";
            if (name == "." || name == "..")
                return "the name is reserved";
            if (name.find_first_of("/\\:*?\"<>|") != std::string_view::npos)
                return "the name must not contain / \\ : * ? \" < > |";
            if (name.ends_with(".meta"))
                return "'.meta' is reserved for asset metadata";
            return nullptr;
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
            while (!text.empty() && text.back()  == ' ') text.remove_suffix(1);
            return text;
        }
    }

    void AssetsPanel::OnImGuiRender()
    {
        if (!m_Open) return;

        if (AssetRegistry::GetRevision() != m_SeenAssetRevision)
            m_RefreshRequested = true;

        ApplyPendingChanges();

        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        if (!m_HasRoot)
        {
            ImGui::TextDisabled("Asset root '%s' does not exist", AssetRegistry::GetAssetRoot().string().c_str());
            if (ImGui::Button("Retry"))
                m_RefreshRequested = true;

            ImGui::End();
            return;
        }

        DrawToolbar();
        ImGui::Separator();

        const float height = ImGui::GetContentRegionAvail().y;

        ImGui::BeginChild("##tree", ImVec2(m_TreeWidth, 0.0f), ImGuiChildFlags_Borders);
        DrawTree(m_Root);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::InvisibleButton("##splitter", ImVec2(4.0f, height));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
        {
            const float maxWidth = std::max(100.0f, ImGui::GetWindowWidth() - 200.0f);
            m_TreeWidth = std::clamp(m_TreeWidth + ImGui::GetIO().MouseDelta.x, 100.0f, maxWidth);
        }

        ImGui::SameLine();

        ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
        DrawContent();
        ImGui::EndChild();

        ImGui::End();
    }

    void AssetsPanel::ApplyPendingChanges()
    {
        if (m_PendingDirectory)
        {
            m_CurrentDirectory = *m_PendingDirectory;
            m_PendingDirectory.reset();
            m_LocalSelection.reset();
            m_RenamingPath.reset();
            m_RefreshRequested = true;
        }

        if (m_RefreshRequested)
        {
            Refresh();
            m_RefreshRequested = false;
        }
    }

    void AssetsPanel::Refresh()
    {
        const fs::path& assetRoot = AssetRegistry::GetAssetRoot();
        std::error_code ec;

        m_Entries.clear();

        if (assetRoot.empty() || !fs::is_directory(assetRoot, ec))
        {
            m_Root = {};
            m_HasRoot = false;
            return;
        }

        m_HasRoot = true;
        m_Root = BuildTree(assetRoot, {});
        m_Root.Name = "Assets";

        if (!m_CurrentDirectory.empty() && !fs::is_directory(assetRoot / m_CurrentDirectory, ec))
            m_CurrentDirectory.clear();

        for (const auto& item : fs::directory_iterator(assetRoot / m_CurrentDirectory, ec))
        {
            const fs::path name = item.path().filename();
            if (IsMetaFile(name)) continue;

            std::error_code itemError;
            const bool isDirectory = item.is_directory(itemError);

            m_Entries.push_back({
                .Path             = m_CurrentDirectory / name,
                .Name             = name.string(),
                .IsDirectory      = isDirectory,
                .IsDirectoryEmpty = isDirectory && IsEmptyDirectory(item.path())
            });
        }

        std::ranges::sort(m_Entries, [](const Entry& a, const Entry& b)
        {
            if (a.IsDirectory != b.IsDirectory)
                return a.IsDirectory;
            return a.Name < b.Name;
        });

        for (Entry& entry : m_Entries)
        {
            if (entry.IsDirectory) continue;

            entry.Type = AssetTypeFromExtension(entry.Path.extension().string());
            if (entry.Type != AssetType::None)
                entry.Handle = AssetRegistry::Import(entry.Path);
        }

        m_SeenAssetRevision = AssetRegistry::GetRevision();
    }

    AssetsPanel::DirectoryNode AssetsPanel::BuildTree(const fs::path& root, const fs::path& relative)
    {
        DirectoryNode node{ .Path = relative, .Name = relative.filename().string() };

        std::error_code ec;
        for (const auto& item : fs::directory_iterator(root / relative, ec))
        {
            std::error_code itemError;
            if (item.is_directory(itemError))
                node.Children.push_back(BuildTree(root, relative / item.path().filename()));
        }

        std::ranges::sort(node.Children, {}, &DirectoryNode::Name);
        return node;
    }

    void AssetsPanel::DrawToolbar()
    {
        if (ImGui::Button("Refresh"))
            m_RefreshRequested = true;

        ImGui::SameLine();
        ImGui::TextDisabled("Assets/%s", m_CurrentDirectory.generic_string().c_str());

        ImGui::SameLine(0.0f, 24.0f);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("##thumbnailSize", &m_ThumbnailSize, 48.0f, 160.0f, "%.0f");
    }

    void AssetsPanel::DrawTree(const DirectoryNode& node)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (node.Children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (node.Path.empty())
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        if (node.Path == m_CurrentDirectory)
            flags |= ImGuiTreeNodeFlags_Selected;

        const bool open = ImGui::TreeNodeEx(node.Name.c_str(), flags);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            m_PendingDirectory = node.Path;

        if (!open) return;

        for (const DirectoryNode& child : node.Children)
            DrawTree(child);

        ImGui::TreePop();
    }

    void AssetsPanel::DrawContent()
    {
        Selection& selection = GetContext().SelectionContext;

        if (!m_RenamingPath && ImGui::Shortcut(ImGuiKey_F2))
        {
            const auto it = std::ranges::find_if(m_Entries, [this](const Entry& entry) { return IsSelected(entry); });
            if (it != m_Entries.end())
                BeginRename(*it);
        }

        const ImGuiStyle& style = ImGui::GetStyle();
        const float cellWidth = m_ThumbnailSize + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
        const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cellWidth));

        if (ImGui::BeginTable("##grid", columns))
        {
            for (Entry& entry : m_Entries)
            {
                ImGui::TableNextColumn();
                DrawItem(entry);
            }
            ImGui::EndTable();
        }

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        {
            m_LocalSelection.reset();
            if (selection.GetAsset())
                selection.Clear();
        }
    }

    void AssetsPanel::DrawItem(Entry& entry)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const bool selected = IsSelected(entry);
        const bool renaming = IsRenaming(entry);

        ImGui::PushID(entry.Name.c_str());

        const float padding = style.FramePadding.x;
        const float lineHeight = ImGui::GetTextLineHeight();

        const ImVec2 tileSize{ m_ThumbnailSize + padding * 2.0f,
                               m_ThumbnailSize + padding * 2.0f +
                               style.ItemInnerSpacing.y + lineHeight * MaxLabelLines };

        const ImVec2 tileMin = ImGui::GetCursorScreenPos();
        const ImVec2 tileMax{ tileMin.x + tileSize.x, tileMin.y + tileSize.y };
        const float labelY = tileMin.y + padding + m_ThumbnailSize + style.ItemInnerSpacing.y;

        Ref<Texture2D> image;
        if (ImGui::IsRectVisible(tileSize))
            image = GetThumbnail(entry);

        const bool isIcon = image == nullptr;
        if (isIcon)
            image = GetContext().Icons.Get(GetIcon(entry));

        const ImVec4 tint = isIcon ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        const float buttonHeight = renaming ? labelY - tileMin.y : tileSize.y;
        const bool pressed = ImGui::InvisibleButton("##tile", ImVec2(tileSize.x, buttonHeight));
        const bool hovered = ImGui::IsItemHovered();

        ImGui::SetItemTooltip("%s", entry.Name.c_str());

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            Select(entry);

        if (ImGui::BeginPopupContextItem("##tileMenu"))
        {
            if (ImGui::MenuItem("Rename", "F2"))
                BeginRename(entry);
            ImGui::EndPopup();
        }

        if (!renaming && entry.Handle && ImGui::BeginDragDropSource())
        {
            const AssetPayload payload{ .Handle = *entry.Handle, .Type = entry.Type };
            ImGui::SetDragDropPayload(AssetPayloadType, &payload, sizeof(payload));

            UI::Image(image, ImVec2(32.0f, 32.0f), tint);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(entry.Name.c_str());
            ImGui::EndDragDropSource();
        }

        if (pressed)
            Select(entry);

        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            Activate(entry);

        ImDrawList& drawList = *ImGui::GetWindowDrawList();

        if (selected)
            drawList.AddRectFilled(tileMin, tileMax, ImGui::GetColorU32(ImGuiCol_Header), style.FrameRounding);

        const ImVec2 imageSize = FitToSquare(*image, m_ThumbnailSize);
        const ImVec2 imageMin{ tileMin.x + padding + (m_ThumbnailSize - imageSize.x) * 0.5f,
                               tileMin.y + padding + (m_ThumbnailSize - imageSize.y) * 0.5f };
        UI::DrawImage(drawList, image, imageMin, ImVec2(imageMin.x + imageSize.x,
                      imageMin.y + imageSize.y), ImGui::GetColorU32(tint));

        if (renaming)
            DrawRenameField(ImVec2(tileMin.x, labelY), tileSize.x, tileMax.y);
        else
        {
            const std::vector<std::string> lines = LayoutLabel(GetDisplayName(entry.Path, entry.IsDirectory),
                                                               m_ThumbnailSize, MaxLabelLines);

            float lineY = labelY;
            for (const std::string& line : lines)
            {
                const float lineX = tileMin.x + padding + (m_ThumbnailSize - TextWidth(line)) * 0.5f;
                drawList.AddText(ImVec2(lineX, lineY), ImGui::GetColorU32(ImGuiCol_Text), line.c_str());
                lineY += lineHeight;
            }
        }

        ImGui::PopID();
    }

    void AssetsPanel::BeginRename(const Entry& entry)
    {
        const std::string name = GetDisplayName(entry.Path, entry.IsDirectory);
        const size_t length = std::min(name.size(), m_RenameBuffer.size() - 1);
        std::memcpy(m_RenameBuffer.data(), name.data(), length);
        m_RenameBuffer[length] = '\0';

        m_RenamingPath = entry.Path;
        m_FocusRename = true;
    }

    void AssetsPanel::CommitRename()
    {
        const std::optional<fs::path> from = std::exchange(m_RenamingPath, std::nullopt);
        if (!from) return;

        const std::string_view name = Trim(m_RenameBuffer.data());

        std::error_code error;
        const bool isDirectory = fs::is_directory(AssetRegistry::GetAssetRoot() / *from, error);
        const fs::path newFileName = isDirectory ? fs::path(name)
                                                 : fs::path(std::string(name) + from->extension().string());
        if (newFileName == from->filename()) return;

        if (const char* invalid = ValidateFileName(name))
        {
            APP_ERROR("Cannot rename '{}': {}", from->filename().string(), invalid);
            return;
        }

        const fs::path to = from->parent_path() / newFileName;
        if (const std::expected<void, std::string> moved = AssetRegistry::Move(*from, to); !moved)
        {
            APP_ERROR("Cannot rename '{}': {}", from->filename().string(), moved.error());
            return;
        }

        if (m_LocalSelection && m_LocalSelection->Path == *from)
            m_LocalSelection->Path = to;

        m_RefreshRequested = true;
    }

    void AssetsPanel::DrawRenameField(const ImVec2& position, const float width, const float bottom)
    {
        ImGui::SetCursorScreenPos(position);
        ImGui::SetNextItemWidth(width);

        if (std::exchange(m_FocusRename, false))
            ImGui::SetKeyboardFocusHere();

        const bool submitted = ImGui::InputText("##rename", m_RenameBuffer.data(), m_RenameBuffer.size(),
                                                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (submitted)
            CommitRename();
        else if (ImGui::IsItemDeactivated())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                m_RenamingPath.reset();
            else
                CommitRename();
        }

        const float fieldBottom = ImGui::GetItemRectMax().y;
        ImGui::SetCursorScreenPos(ImVec2(position.x, fieldBottom));
        ImGui::Dummy(ImVec2(width, std::max(0.0f, bottom - fieldBottom)));
    }

    bool AssetsPanel::IsSelected(const Entry& entry) const
    {
        if (const fs::path* local = GetLocalSelection())
            return entry.Path == *local;

        return entry.Handle && GetContext().SelectionContext.IsAsset(*entry.Handle);
    }

    const std::filesystem::path* AssetsPanel::GetLocalSelection() const
    {
        if (!m_LocalSelection || m_LocalSelection->Revision != GetContext().SelectionContext.GetRevision())
            return nullptr;

        return &m_LocalSelection->Path;
    }

    void AssetsPanel::Select(const Entry& entry)
    {
        Selection& selection = GetContext().SelectionContext;

        if (entry.Handle)
        {
            m_LocalSelection.reset();
            selection.SelectAsset(*entry.Handle);
            return;
        }

        m_LocalSelection = LocalSelection{ .Path = entry.Path, .Revision = selection.GetRevision() };
    }

    void AssetsPanel::Activate(const Entry& entry)
    {
        if (entry.IsDirectory)
            m_PendingDirectory = entry.Path;
        else if (entry.Type == AssetType::Scene && entry.Handle)
            GetContext().Dialogs.RequestOpen(*entry.Handle);
    }

    Ref<Texture2D> AssetsPanel::GetThumbnail(Entry& entry)
    {
        if (!entry.Handle || entry.Type != AssetType::Texture2D)
            return nullptr;

        if (!entry.Texture)
            entry.Texture = AssetManager::LoadTexture2D(*entry.Handle);

        if (entry.Texture->GetState() == AssetLoadState::Failed)
            return nullptr;

        return entry.Texture->GetTexture();
    }

    EditorIcon AssetsPanel::GetIcon(const Entry& entry)
    {
        if (entry.IsDirectory)
            return EditorIcon::Folder;

        return EditorIcons::ForFile(entry.Path);
    }
}
