#include "Editor/Panels/AssetsPanel.h"
#include "Editor/EditorContext.h"

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
            m_Entries.push_back({
                .Path        = m_CurrentDirectory / name,
                .Name        = name.string(),
                .IsDirectory = item.is_directory(itemError)
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
            if (!entry.IsDirectory && AssetTypeFromExtension(entry.Path.extension().string()) != AssetType::None)
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

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered() && GetContext().SelectionContext.GetAsset())
            GetContext().SelectionContext.Clear();
    }

    void AssetsPanel::DrawItem(Entry& entry)
    {
        const ImVec2 thumbnailSize{ m_ThumbnailSize, m_ThumbnailSize };
        const bool selected = entry.Handle && GetContext().SelectionContext.IsAsset(*entry.Handle);

        ImGui::PushID(entry.Name.c_str());
        ImGui::BeginGroup();

        Ref<Texture2D> thumbnail;
        if (ImGui::IsRectVisible(thumbnailSize))
            thumbnail = GetThumbnail(entry);

        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

        bool pressed;
        if (thumbnail)
            pressed = UI::ImageButton("##item", thumbnail, thumbnailSize);
        else
        {
            const std::string label = entry.IsDirectory ? "Folder" : entry.Path.extension().string();
            pressed = ImGui::Button(label.c_str(), ImVec2(thumbnailSize.x + ImGui::GetStyle().FramePadding.x * 2.0f,
                                                          thumbnailSize.y + ImGui::GetStyle().FramePadding.y * 2.0f));
        }

        if (selected)
            ImGui::PopStyleColor();

        if (pressed && entry.Handle)
            GetContext().SelectionContext.SelectAsset(*entry.Handle);

        if (entry.IsDirectory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            m_PendingDirectory = entry.Path;

        if (!entry.IsDirectory && ImGui::BeginDragDropSource())
        {
            const std::string path = entry.Path.generic_string();
            ImGui::SetDragDropPayload(AssetPathPayload, path.c_str(), path.size() + 1);
            ImGui::TextUnformatted(entry.Name.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize.x);
        ImGui::TextUnformatted(entry.Name.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
        ImGui::PopID();
    }

    Ref<Texture2D> AssetsPanel::GetThumbnail(Entry& entry)
    {
        if (entry.IsDirectory || entry.ThumbnailFailed)
            return nullptr;

        if (!entry.Handle || AssetTypeFromExtension(entry.Path.extension().string()) != AssetType::Texture2D)
        {
            entry.ThumbnailFailed = true;
            return nullptr;
        }

        Ref<Texture2D> texture;
        try
        {
            texture = AssetManager::LoadTexture2D(*entry.Handle);
        } catch (const std::exception& e)
        {
            CORE_ERROR("AssetsPanel: could not load thumbnail for '{}': {}", entry.Path.string(), e.what());
        }

        if (!texture)
            entry.ThumbnailFailed = true;

        return texture;
    }
}
