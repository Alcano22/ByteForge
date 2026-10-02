#include "Editor/Panels/AssetsPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/AssetPayload.h"

#include <Engine/Core/Log.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/ImGui/ImGuiWidgets.h>

#include <imgui.h>

#include <algorithm>
#include <exception>
#include <system_error>

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
    }

    void AssetsPanel::OnImGuiRender()
    {
        if (!m_Open) return;

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

        ImGui::PushID(entry.Name.c_str());

        const float padding = style.FramePadding.x;
        const ImVec2 nameSize = ImGui::CalcTextSize(entry.Name.c_str(), nullptr, false, m_ThumbnailSize);
        const ImVec2 tileSize{ m_ThumbnailSize + padding * 2.0f,
                               m_ThumbnailSize + padding * 2.0f + style.ItemInnerSpacing.y + nameSize.y };

        const ImVec2 tileMin = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##tile", tileSize);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 tileMax = ImGui::GetItemRectMax();

        ImDrawList& drawList = *ImGui::GetWindowDrawList();

        if (selected)
            drawList.AddRectFilled(tileMin, tileMax, ImGui::GetColorU32(ImGuiCol_Header), style.FrameRounding);

        Ref<Texture2D> image;
        if (ImGui::IsItemVisible())
            image = GetThumbnail(entry);

        const bool isIcon = image == nullptr;
        if (isIcon)
            image = GetContext().Icons.Get(GetIcon(entry));

        const ImVec4 tint = isIcon ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        const ImVec2 imageSize = FitToSquare(*image, m_ThumbnailSize);
        const ImVec2 imageMin{ tileMin.x + padding + (m_ThumbnailSize - imageSize.x) * 0.5f,
                               tileMin.y + padding + (m_ThumbnailSize - imageSize.y) * 0.5f };
        UI::DrawImage(drawList, image, imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y),
                      ImGui::GetColorU32(tint));

        const float nameOffset = nameSize.x < m_ThumbnailSize ? (m_ThumbnailSize - nameSize.x) * 0.5f : 0.0f;
        const ImVec2 namePos{ tileMin.x + padding + nameOffset,
                              tileMin.y + padding + m_ThumbnailSize + style.ItemInnerSpacing.y };
        drawList.AddText(ImGui::GetFont(), ImGui::GetFontSize(), namePos, ImGui::GetColorU32(ImGuiCol_Text),
                         entry.Name.c_str(), nullptr, m_ThumbnailSize);

        if (pressed)
            Select(entry);

        if (entry.IsDirectory && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            m_PendingDirectory = entry.Path;

        if (entry.Handle && ImGui::BeginDragDropSource())
        {
            const AssetPayload payload{ .Handle = *entry.Handle, .Type = entry.Type };
            ImGui::SetDragDropPayload(AssetPayloadType, &payload, sizeof(payload));

            UI::Image(image, ImVec2(32.0f, 32.0f), tint);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(entry.Name.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::PopID();
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
            return entry.IsDirectoryEmpty ? EditorIcon::FolderEmpty : EditorIcon::FolderFilled;

        return EditorIcons::ForFile(entry.Path);
    }
}
