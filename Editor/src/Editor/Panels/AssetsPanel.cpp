#include "Editor/Panels/AssetsPanel.h"
#include "Editor/Panels/GraphEditorPanel.h"
#include "Editor/Panels/AnimationClipPanel.h"
#include "Editor/Panels/InputActionsPanel.h"
#include "Editor/Panels/SpriteEditorPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/AssetPayload.h"

#include <Engine/Core/Log.h>
#include <Engine/Core/FileSystem.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/Assets/InputActions.h>
#include <Engine/Assets/Prefab.h>
#include <Engine/Assets/SpriteSheet.h>
#include <Engine/Scene/SceneSerializer.h>
#include <Engine/ImGui/ImGuiWidgets.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <imgui_stdlib.h>

#include <algorithm>
#include <cstring>
#include <exception>
#include <expected>
#include <format>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr const char* EntryPayloadType = "ByteForge.AssetEntry";

        bool IsEmptyDirectory(const fs::path& path)
        {
            std::error_code ec;
            for (const auto& child : fs::directory_iterator(path, ec))
            {
                if (!AssetRegistry::IsMetaFile(child.path()))
                    return false;
            }
            return true;
        }

        bool IsSameOrInside(const fs::path& path, const fs::path& directory)
        {
            const auto [directoryIt, pathIt] = std::mismatch(directory.begin(), directory.end(),
                                                             path.begin(), path.end());
            return directoryIt == directory.end();
        }

        bool ContainsScripts(const fs::path& absolutePath)
        {
            std::error_code error;
            if (!fs::is_directory(absolutePath, error))
                return absolutePath.extension() == ".cs";

            for (auto it = fs::recursive_directory_iterator(absolutePath, error);
                 !error && it != fs::recursive_directory_iterator(); it.increment(error))
            {
                if (it->path().extension() == ".cs")
                    return true;
            }
            return false;
        }

        fs::path MakeUniquePath(const fs::path& directory, const std::string_view stem,
                                const std::string_view extension)
        {
            const fs::path& root = AssetRegistry::GetAssetRoot();
            std::error_code error;

            for (i32 index = 0;; ++index)
            {
                const std::string name = index == 0 ? std::format("{}{}", stem, extension)
                                                    : std::format("{} {}{}", stem, index, extension);
                fs::path candidate = directory / PathFromUtf8(name);
                if (!fs::exists(root / candidate, error))
                    return candidate;
            }
        }

        ImVec2 FitToSquare(const Texture2D& texture, const f32 size)
        {
            const f32 width = static_cast<f32>(texture.GetWidth());
            const f32 height = static_cast<f32>(texture.GetHeight());
            const f32 scale = size / std::max(width, height);
            return { width * scale, height * scale };
        }

        constexpr i32 MaxLabelLines = 2;
        constexpr const char* Ellipsis = "\xE2\x80\xA6";

        usize NextCharEnd(const std::string_view text, usize pos)
        {
            ++pos;
            while (pos < text.size() && (static_cast<u8>(text[pos]) & 0xC0) == 0x80)
                ++pos;
            return pos;
        }

        usize PreviousCharStart(const std::string_view text, usize pos)
        {
            while (pos > 0)
            {
                --pos;
                if ((static_cast<u8>(text[pos]) & 0xC0) != 0x80) break;
            }
            return pos;
        }

        f32 TextWidth(const std::string_view text)
        {
            return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
        }

        bool IsBreakAfter(const char c) { return c == ' ' || c == '_' || c == '-'; }

        std::vector<std::string> LayoutLabel(std::string_view text, const f32 maxWidth, const i32 maxLines)
        {
            std::vector<std::string> lines;

            while (!text.empty() && static_cast<i32>(lines.size()) < maxLines)
            {
                usize fit = 0;
                for (usize end = NextCharEnd(text, 0);
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

                if (static_cast<i32>(lines.size()) == maxLines - 1)
                {
                    const f32 ellipsisWidth = TextWidth(Ellipsis);
                    while (fit > 0 && TextWidth(text.substr(0, fit)) + ellipsisWidth > maxWidth)
                        fit = PreviousCharStart(text, fit);

                    lines.push_back(std::string(text.substr(0, fit)) + Ellipsis);
                    break;
                }

                usize lineEnd = fit;
                for (usize i = fit; i > 0; --i)
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

        std::string GetDisplayName(const fs::path& path, const bool isDirectory)
        {
            return isDirectory ? ToUtf8(path.filename()) : ToUtf8(path.stem());
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

        bool IsCSharpIdentifier(const std::string_view name)
        {
            if (name.empty() || (name.front() >= '0' && name.front() <= '9'))
                return false;

            return std::ranges::all_of(name, [](const char c)
            {
                return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
            });
        }

        bool IsCSharpScript(const fs::path& path)
        {
            return AssetTypeFromExtension(ToUtf8(path.extension())) == AssetType::Script;
        }

        std::string MakeScriptSource(const std::string_view className)
        {
            return std::format("using ByteForge;\n"
                               "\n"
                               "public class {} : ScriptBehaviour\n"
                               "{{\n"
                               "    protected override void OnCreate()\n"
                               "    {{\n"
                               "    }}\n"
                               "\n"
                               "    protected override void OnUpdate(float deltaTime)\n"
                               "    {{\n"
                               "    }}\n"
                               "}}\n",
                               className);
        }

        void RenameScriptClass(const fs::path& relativePath,
                               const std::string_view oldName, const std::string_view newName)
        {
            if (!IsCSharpIdentifier(oldName)) return;

            const fs::path file = AssetRegistry::GetAssetRoot() / relativePath;
            const Result<std::vector<std::byte>> bytes = ReadBinaryFile(file);
            if (!bytes)
            {
                APP_WARN("Renamed '{}', but cannot read it to rename the class: {}", ToGenericUtf8(relativePath), bytes.error());
                return;
            }

            std::string source(reinterpret_cast<const char*>(bytes->data()), bytes->size());
            const std::regex declaration(std::format(R"(\bclass\s+{}\b)", oldName));

            std::smatch match;
            if (!std::regex_search(source, match, declaration))
            {
                APP_WARN("Renamed '{}', but it declares no class '{}', rename the class to '{}' yourself",
                         ToGenericUtf8(relativePath), oldName, newName);
                return;
            }

            const usize position = static_cast<usize>(match.position(0));
            const usize length = static_cast<usize>(match.length(0));
            if (std::regex_search(source.cbegin() + static_cast<std::ptrdiff_t>(position + length), source.cend(), declaration))
            {
                APP_WARN("Renamed '{}', but it declares more than one class '{}', rename the right one to '{}' yourself",
                         ToGenericUtf8(relativePath), oldName, newName);
                return;
            }

            source.replace(position, length, std::format("class {}", newName));
            if (const Result<> written = WriteFileAtomic(file, source); !written)
                APP_WARN("Renamed '{}', but cannot rename the class: {}", ToGenericUtf8(relativePath), written.error());
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
            while (!text.empty() && text.back()  == ' ') text.remove_suffix(1);
            return text;
        }

        std::string MakeFileStem(const std::string& name)
        {
            std::string stem = name;
            for (char& c : stem)
            {
                if (std::string_view("/\\:*?\"<>|").find(c) != std::string_view::npos)
                    c = '_';
            }

            const std::string trimmed(Trim(stem));
            return trimmed.empty() || trimmed == "." || trimmed == ".." ? "New Prefab" : trimmed;
        }

        bool HasSheetSprites(const UUID texture)
        {
            AssetMetadata metadata;
            if (!AssetRegistry::TryGetMetadata(texture, metadata)) return false;

            const auto* settings = metadata.GetSettings<TextureAssetSettings>();
            return settings != nullptr && settings->Mode == SpriteMode::Multiple
                && !AssetRegistry::GetSubAssets(texture).empty();
        }

        void DrawExpandArrow(ImDrawList& drawList, const ImVec2& min,
                             const f32 size, const bool expanded, const bool hovered)
        {
            const ImVec2 center{ min.x + size * 0.5f, min.y + size * 0.5f };
            drawList.AddCircleFilled(center, size * 0.45f,
                                     ImGui::GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));

            const f32 half = size * 0.2f;
            const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
            if (expanded)
            {
                drawList.AddTriangleFilled(ImVec2(center.x + half, center.y + half), ImVec2(center.x - half, center.y),
                                           ImVec2(center.x + half, center.y - half), color);
            } else
            {
                drawList.AddTriangleFilled(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y),
                                           ImVec2(center.x - half, center.y + half), color);
            }
        }
    }

    void AssetsPanel::OnImGuiRender()
    {
        if (!m_Open) return;

        if (AssetRegistry::GetRevision() != m_SeenAssetRevision)
            m_RefreshRequested = true;

        if (ImGui::GetDragDropPayload() == nullptr)
            m_DraggedPath.reset();

        ApplyPendingChanges();

        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        if (!m_HasRoot)
        {
            ImGui::TextDisabled("Asset root '%s' does not exist", ToUtf8(AssetRegistry::GetAssetRoot()).c_str());
            if (ImGui::Button("Retry"))
                m_RefreshRequested = true;

            ImGui::End();
            return;
        }

        DrawToolbar();
        ImGui::Separator();

        const f32 height = ImGui::GetContentRegionAvail().y;

        ImGui::BeginChild("##tree", ImVec2(m_TreeWidth, 0.0f), ImGuiChildFlags_Borders);
        DrawTree(m_Root);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::InvisibleButton("##splitter", ImVec2(4.0f, height));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
        {
            const f32 maxWidth = std::max(100.0f, ImGui::GetWindowWidth() - 200.0f);
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

            if (const std::optional<fs::path> path = std::exchange(m_RenameAfterRefresh, std::nullopt))
            {
                const auto it = std::ranges::find(m_Entries, *path, &Entry::Path);
                if (it != m_Entries.end())
                {
                    Select(*it);
                    BeginRename(*it);
                }
            }
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
            if (AssetRegistry::IsMetaFile(name)) continue;

            std::error_code itemError;
            const bool isDirectory = item.is_directory(itemError);

            m_Entries.push_back({
                .Path             = m_CurrentDirectory / name,
                .Name             = ToUtf8(name),
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

            entry.Type = AssetTypeFromExtension(ToUtf8(entry.Path.extension()));
            if (entry.Type != AssetType::None)
                entry.Handle = AssetRegistry::Import(entry.Path);
        }

        std::vector<Entry> withSprites;
        withSprites.reserve(m_Entries.size());

        for (Entry& entry : m_Entries)
        {
            const std::optional<UUID> texture = entry.Type == AssetType::Texture2D ? entry.Handle : std::nullopt;
            entry.HasSprites = texture && HasSheetSprites(*texture);
            const bool expanded = entry.HasSprites && m_Expanded.contains(*texture);
            const fs::path path = entry.Path;

            withSprites.push_back(std::move(entry));
            if (!expanded) continue;

            for (const UUID sprite : AssetRegistry::GetSubAssets(*texture))
            {
                AssetMetadata metadata;
                if (!AssetRegistry::TryGetMetadata(sprite, metadata)) continue;

                withSprites.push_back(Entry{
                    .Path   = path,
                    .Name   = metadata.GetDisplayName(),
                    .Type   = AssetType::Sprite,
                    .Handle = sprite,
                    .Owner  = *texture
                });
            }
        }

        m_Entries = std::move(withSprites);

        m_SeenAssetRevision = AssetRegistry::GetRevision();
    }

    AssetsPanel::DirectoryNode AssetsPanel::BuildTree(const fs::path& root, const fs::path& relative)
    {
        DirectoryNode node{ .Path = relative, .Name = ToUtf8(relative.filename()) };

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
        ImGui::TextDisabled("Assets/%s", ToGenericUtf8(m_CurrentDirectory).c_str());

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

        DrawMoveTarget(node.Path);

        if (!open) return;

        for (const DirectoryNode& child : node.Children)
            DrawTree(child);

        ImGui::TreePop();
    }

    void AssetsPanel::DrawContent()
    {
        Selection& selection = GetContext().SelectionContext;

        const auto findSelectedFile = [this]
        {
            return std::ranges::find_if(m_Entries, [this](const Entry& entry) { return !entry.Owner && IsSelected(entry); });
        };

        if (!m_RenamingPath && ImGui::Shortcut(ImGuiKey_F2))
        {
            if (const auto it = findSelectedFile(); it != m_Entries.end())
                BeginRename(*it);
        }

        if (!m_RenamingPath && ImGui::Shortcut(ImGuiKey_Delete))
        {
            if (const auto it = findSelectedFile(); it != m_Entries.end())
                RequestDelete(*it);
        }

        const ImGuiStyle& style = ImGui::GetStyle();
        const f32 cellWidth = m_ThumbnailSize + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
        const i32 columns = std::max(1, static_cast<i32>(ImGui::GetContentRegionAvail().x / cellWidth));

        if (ImGui::BeginTable("##grid", columns))
        {
            for (Entry& entry : m_Entries)
            {
                ImGui::TableNextColumn();
                DrawItem(entry);
            }
            ImGui::EndTable();
        }

        DrawEntityDropZone();

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        {
            m_LocalSelection.reset();
            if (selection.GetAsset())
                selection.Clear();
        }

        DrawContextMenu();
    }

    void AssetsPanel::DrawItem(Entry& entry)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const bool selected = IsSelected(entry);

        const bool isSprite = entry.Owner.has_value();
        const bool renaming = !isSprite && IsRenaming(entry);

        if (isSprite)
            EditorUI::PushID(static_cast<u64>(*entry.Handle));
        else
            ImGui::PushID(entry.Name.c_str());

        const f32 padding = style.FramePadding.x;
        const f32 lineHeight = ImGui::GetTextLineHeight();

        const ImVec2 tileSize{ m_ThumbnailSize + padding * 2.0f,
                               m_ThumbnailSize + padding * 2.0f +
                               style.ItemInnerSpacing.y + lineHeight * MaxLabelLines };

        const ImVec2 tileMin = ImGui::GetCursorScreenPos();
        const ImVec2 tileMax{ tileMin.x + tileSize.x, tileMin.y + tileSize.y };
        const f32 labelY = tileMin.y + padding + m_ThumbnailSize + style.ItemInnerSpacing.y;

        Ref<Texture2D> thumbnail;
        const Sprite* sprite = nullptr;
        if (ImGui::IsRectVisible(tileSize))
        {
            if (isSprite)
                sprite = GetSpritePreview(entry);
            else
                thumbnail = GetThumbnail(entry);
        }

        const bool isIcon = thumbnail == nullptr;
        const Ref<Texture2D> image = isIcon ? GetContext().Icons.Get(GetIcon(entry)) : thumbnail;
        const ImVec4 tint = isIcon ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        const bool expandable = entry.HasSprites && !renaming;
        if (expandable)
            ImGui::SetNextItemAllowOverlap();

        const f32 buttonHeight = renaming ? labelY - tileMin.y : tileSize.y;
        const bool pressed = ImGui::InvisibleButton("##tile", ImVec2(tileSize.x, buttonHeight));
        const bool hovered = ImGui::IsItemHovered();

        if (entry.IsDirectory && !renaming)
            DrawMoveTarget(entry.Path);

        ImGui::SetItemTooltip("%s", entry.Name.c_str());

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            Select(entry);

        if (ImGui::BeginPopupContextItem("##tileMenu"))
        {
            if (isSprite)
            {
                if (ImGui::MenuItem("Open in Sprite Editor"))
                    Activate(entry);
            } else
            {
                if (entry.Type == AssetType::Texture2D && entry.Handle && ImGui::MenuItem("Open Sprite Editor"))
                    Activate(entry);

                if (ImGui::MenuItem("Rename", "F2"))
                    BeginRename(entry);

                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del"))
                    RequestDelete(entry);
            }

            ImGui::EndPopup();
        }

        if (!renaming && ImGui::BeginDragDropSource())
        {
            if (!isSprite)
                m_DraggedPath = entry.Path;

            if (entry.Handle)
            {
                const AssetPayload payload{ .Handle = *entry.Handle, .Type = entry.Type };
                ImGui::SetDragDropPayload(AssetPayloadType, &payload, sizeof(payload));
            } else
                ImGui::SetDragDropPayload(EntryPayloadType, nullptr, 0);

            if (sprite != nullptr)
            {
                const ImVec2 previewMin = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(32.0f, 32.0f));
                EditorUI::DrawSpriteImage(*ImGui::GetWindowDrawList(), *sprite, previewMin,
                                          ImVec2(previewMin.x + 32.0f, previewMin.y + 32.0f));
            } else
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

        const f32 arrowSize = ImGui::GetFrameHeight();
        const ImVec2 arrowMin{ tileMax.x - arrowSize, tileMin.y + padding + (m_ThumbnailSize - arrowSize) * 0.5f };
        const bool expanded = expandable && m_Expanded.contains(*entry.Handle);
        bool arrowHovered = false;

        if (expandable)
        {
            ImGui::SetCursorScreenPos(arrowMin);
            if (ImGui::InvisibleButton("##expand", ImVec2(arrowSize, arrowSize)))
            {
                if (expanded)
                    m_Expanded.erase(*entry.Handle);
                else
                    m_Expanded.insert(*entry.Handle);
                m_RefreshRequested = true;
            }
            arrowHovered = ImGui::IsItemHovered();
            ImGui::SetItemTooltip(expanded ? "Hide sprites" : "Show sprites");
        }

        ImDrawList& drawList = *ImGui::GetWindowDrawList();

        if (isSprite)
            drawList.AddRectFilled(tileMin, tileMax, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);

        if (selected)
            drawList.AddRectFilled(tileMin, tileMax, ImGui::GetColorU32(ImGuiCol_Header), style.FrameRounding);

        const ImVec2 thumbnailMin{ tileMin.x + padding, tileMin.y + padding };
        const ImVec2 thumbnailMax{ thumbnailMin.x + m_ThumbnailSize, thumbnailMin.y + m_ThumbnailSize };

        if (sprite != nullptr)
            EditorUI::DrawSpriteImage(drawList, *sprite, thumbnailMin, thumbnailMax);
        else
        {
            const ImVec2 imageSize = FitToSquare(*image, m_ThumbnailSize);
            const ImVec2 imageMin{ thumbnailMin.x + (m_ThumbnailSize - imageSize.x) * 0.5f,
                                   thumbnailMin.y + (m_ThumbnailSize - imageSize.y) * 0.5f };
            UI::DrawImage(drawList, image, imageMin, ImVec2(imageMin.x + imageSize.x,
                          imageMin.y + imageSize.y), ImGui::GetColorU32(tint));
        }

        if (expandable)
            DrawExpandArrow(drawList, arrowMin, arrowSize, expanded, arrowHovered);

        if (renaming)
            DrawRenameField(ImVec2(tileMin.x, labelY), tileSize.x, tileMax.y);
        else
        {
            const std::string label = isSprite ? entry.Name : GetDisplayName(entry.Path, entry.IsDirectory);
            const std::vector<std::string> lines = LayoutLabel(label, m_ThumbnailSize, MaxLabelLines);

            f32 lineY = labelY;
            for (const std::string& line : lines)
            {
                const f32 lineX = tileMin.x + padding + (m_ThumbnailSize - TextWidth(line)) * 0.5f;
                drawList.AddText(ImVec2(lineX, lineY), ImGui::GetColorU32(ImGuiCol_Text), line.c_str());
                lineY += lineHeight;
            }
        }

        ImGui::PopID();
    }

    void AssetsPanel::BeginRename(const Entry& entry)
    {
        m_RenameText = GetDisplayName(entry.Path, entry.IsDirectory);
        m_RenamingPath = entry.Path;
        m_FocusRename = true;
    }

    void AssetsPanel::CommitRename()
    {
        const std::optional<fs::path> from = std::exchange(m_RenamingPath, std::nullopt);
        if (!from) return;

        const bool isNewScript = std::exchange(m_NewScript, std::nullopt) == from;
        if (RenameEntry(*from) || isNewScript)
            GetContext().Scripts.RequestReload();
    }

    bool AssetsPanel::RenameEntry(const fs::path& from)
    {
        const std::string_view name = Trim(m_RenameText);

        std::error_code error;
        const bool isDirectory = fs::is_directory(AssetRegistry::GetAssetRoot() / from, error);
        fs::path newFileName = PathFromUtf8(name);
        if (!isDirectory)
            newFileName += from.extension();
        if (newFileName == from.filename()) return false;

        if (const char* invalid = ValidateFileName(name))
        {
            APP_ERROR("Cannot rename '{}': {}", ToUtf8(from.filename()), invalid);
            return false;
        }

        const bool isScript = !isDirectory && IsCSharpScript(from);
        if (isScript && !IsCSharpIdentifier(name))
        {
            APP_ERROR("Cannot rename '{}': a script name is its class name, so only letters, digits and _ "
                      "are allowed and it must not start with a digit", ToUtf8(from.filename()));
            return false;
        }

        const fs::path to = from.parent_path() / newFileName;
        if (const Result<> moved = AssetRegistry::Move(from, to); !moved)
        {
            APP_ERROR("Cannot rename '{}': {}", ToUtf8(from.filename()), moved.error());
            return false;
        }

        if (m_LocalSelection && m_LocalSelection->Path == from)
            m_LocalSelection->Path = to;

        m_RefreshRequested = true;

        if (!isScript) return false;

        RenameScriptClass(to, ToUtf8(from.stem()), name);
        return true;
    }

    void AssetsPanel::CancelRename()
    {
        m_RenamingPath.reset();

        if (std::exchange(m_NewScript, std::nullopt))
            GetContext().Scripts.RequestReload();
    }

    void AssetsPanel::DrawRenameField(const ImVec2& position, const f32 width, const f32 bottom)
    {
        ImGui::SetCursorScreenPos(position);
        ImGui::SetNextItemWidth(width);

        if (std::exchange(m_FocusRename, false))
            ImGui::SetKeyboardFocusHere();

        const bool submitted = ImGui::InputText("##rename", &m_RenameText, ImGuiInputTextFlags_EnterReturnsTrue
                                                                         | ImGuiInputTextFlags_AutoSelectAll);

        if (submitted)
            CommitRename();
        else if (ImGui::IsItemDeactivated())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                CancelRename();
            else
                CommitRename();
        }

        const f32 fieldBottom = ImGui::GetItemRectMax().y;
        ImGui::SetCursorScreenPos(ImVec2(position.x, fieldBottom));
        ImGui::Dummy(ImVec2(width, std::max(0.0f, bottom - fieldBottom)));
    }

    void AssetsPanel::RequestDelete(const Entry& entry)
    {
        if (const char* blocker = GetDeleteBlocker(entry.Path))
        {
            APP_WARN("Cannot delete '{}': {}", entry.Name, blocker);
            return;
        }

        const std::string question = entry.IsDirectory && !entry.IsDirectoryEmpty
                                   ? std::format("Move the folder '{}' and everything in it to the trash?", entry.Name)
                                   : std::format("Move '{}' to the trash?", entry.Name);

        GetContext().Popups.Open("Delete", [this, path = entry.Path, question](Popup& popup)
        {
            ImGui::TextUnformatted(question.c_str());
            ImGui::TextDisabled("Anything that references it will show it as missing.");
            ImGui::Spacing();

            if (ImGui::Button("Move to Trash"))
            {
                popup.Close();
                Delete(path);
            }
            ImGui::SetItemDefaultFocus();

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                popup.Close();
        });
    }

    void AssetsPanel::Delete(const fs::path& path)
    {
        EditorContext& context = GetContext();

        if (const char* blocker = GetDeleteBlocker(path))
        {
            APP_WARN("Cannot delete '{}': {}", ToUtf8(path.filename()), blocker);
            return;
        }

        const bool hadScripts = ContainsScripts(AssetRegistry::GetAssetRoot() / path);

        if (const Result<> trashed = AssetRegistry::MoveToTrash(path); !trashed)
        {
            APP_ERROR("Cannot delete '{}': {}", ToUtf8(path.filename()), trashed.error());
            return;
        }

        APP_INFO("Moved '{}' to the trash", ToGenericUtf8(path));

        Selection& selection = context.SelectionContext;
        if (const std::optional<UUID> asset = selection.GetAsset())
        {
            AssetMetadata metadata;
            if (!AssetRegistry::TryGetMetadata(*asset, metadata))
                selection.Clear();
        }

        if (m_LocalSelection && IsSameOrInside(m_LocalSelection->Path, path))
            m_LocalSelection.reset();
        if (m_RenamingPath && IsSameOrInside(*m_RenamingPath, path))
            m_RenamingPath.reset();

        if (hadScripts)
            context.Scripts.RequestReload();

        m_RefreshRequested = true;
    }

    void AssetsPanel::DrawMoveTarget(const fs::path& directory)
    {
        if (!m_DraggedPath || !ImGui::BeginDragDropTarget()) return;

        const ImGuiPayload* payload = ImGui::GetDragDropPayload();
        const bool fromThisPanel = payload != nullptr
                                && (payload->IsDataType(AssetPayloadType) || payload->IsDataType(EntryPayloadType));

        const fs::path& source = *m_DraggedPath;
        const bool valid = source.parent_path() != directory && !IsSameOrInside(directory, source);

        if (fromThisPanel && valid && ImGui::AcceptDragDropPayload(payload->DataType))
            MoveEntry(source, directory);

        ImGui::EndDragDropTarget();
    }

    void AssetsPanel::MoveEntry(const fs::path& from, const fs::path& directory)
    {
        const fs::path to = directory / from.filename();
        if (const Result<> moved = AssetRegistry::Move(from, to); !moved)
        {
            APP_ERROR("Cannot move '{}' to 'Assets/{}': {}", ToUtf8(from.filename()), ToGenericUtf8(directory), moved.error());
            return;
        }

        if (m_LocalSelection && IsSameOrInside(m_LocalSelection->Path, from))
            m_LocalSelection.reset();

        m_DraggedPath.reset();
        m_RefreshRequested = true;
    }

    const char* AssetsPanel::GetDeleteBlocker(const fs::path& path) const
    {
        const EditorContext& context = GetContext();
        if (!context.IsEditing())
            return "stop play mode first";

        const SceneDocument& document = context.Document;
        if (const fs::path scene = document.GetScenePath(); !scene.empty() && IsSameOrInside(scene, path))
            return "the open scene would be deleted, open another scene first";

        if (document.IsEditingPrefab())
        {
            if (const fs::path prefab = document.GetPath(); !prefab.empty() && IsSameOrInside(prefab, path))
                return "the prefab you are editing would be deleted, close it first";
        }

        return nullptr;
    }

    void AssetsPanel::DrawContextMenu()
    {
        constexpr ImGuiPopupFlags flags = ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems;
        if (!ImGui::BeginPopupContextWindow("##contentMenu", flags)) return;

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Folder"))
                CreateFolder();

            ImGui::Separator();

            if (ImGui::MenuItem("Scene"))
                CreateScene();

            ImGui::Separator();

            if (ImGui::MenuItem("C# Script"))
                CreateScript();
            if (ImGui::MenuItem("Script Graph"))
                CreateScriptGraph();

            ImGui::Separator();

            if (ImGui::MenuItem("Material"))
                CreateMaterial();
            if (ImGui::MenuItem("Physics Material 2D"))
                CreatePhysicsMaterial();
            if (ImGui::MenuItem("Animation Clip"))
                CreateAnimationClip();
            if (ImGui::MenuItem("Animator Controller"))
                CreateAnimatorController();
            if (ImGui::MenuItem("Input Actions"))
                CreateInputActions();

            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    void AssetsPanel::DrawEntityDropZone()
    {
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, std::max(ImGui::GetContentRegionAvail().y, 24.0f)));

        if (!ImGui::BeginDragDropTarget()) return;

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorUI::EntityPayloadType);
            payload != nullptr && payload->DataSize == sizeof(u64))
        {
            const Scene* scene = GetContext().ActiveScene;
            const Entity entity = scene != nullptr
                                ? scene->FindEntityByUUID(UUID(*static_cast<const u64*>(payload->Data)))
                                : Entity{};

            if (entity.IsValid())
                CreatePrefab(entity);
        }

        ImGui::EndDragDropTarget();
    }

    void AssetsPanel::CreateFolder()
    {
        const fs::path path = MakeUniquePath(m_CurrentDirectory, "New Folder", "");

        std::error_code error;
        if (!fs::create_directory(AssetRegistry::GetAssetRoot() / path, error))
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), error.message());
            return;
        }

        RenameAfterRefresh(path);
    }

    void AssetsPanel::CreatePhysicsMaterial()
    {
        const fs::path path = MakeUniquePath(m_CurrentDirectory, "New Physics Material", PhysicsMaterialExtension);

        if (const auto created = AssetManager::CreatePhysicsMaterial(path); !created)
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), created.error());
            return;
        }

        RenameAfterRefresh(path);
    }

    void AssetsPanel::CreateAnimationClip()
    {
        const fs::path path = MakeUniquePath(m_CurrentDirectory, "New Animation", AnimationClipExtension);

        if (const auto created = AssetManager::CreateAnimationClip(path); !created)
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), created.error());
            return;
        }

        RenameAfterRefresh(path);
    }

    void AssetsPanel::CreateAnimatorController()
    {
        const fs::path path = MakeUniquePath(m_CurrentDirectory, "New Animator", AnimatorControllerExtension);

        if (const auto created = AssetManager::CreateAnimatorController(path); !created)
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), created.error());
            return;
        }

        RenameAfterRefresh(path);
    }

    void AssetsPanel::CreateScriptGraph()
    {
        const fs::path path = MakeUniquePath(m_CurrentDirectory, "New Script Graph", ScriptGraphExtension);

        if (const auto created = AssetManager::CreateScriptGraph(path); !created)
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), created.error());
            return;
        }

        RenameAfterRefresh(path);
    }

    void AssetsPanel::CreateInputActions()
    {
        const fs::path path = MakeUniquePath(m_CurrentDirectory, "New Input Actions", InputActionsExtension);

        if (const auto created = AssetManager::CreateInputActions(path); !created)
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), created.error());
            return;
        }

        RenameAfterRefresh(path);
    }

    void AssetsPanel::CreateScript()
    {
        const fs::path& root = AssetRegistry::GetAssetRoot();
        std::error_code error;

        fs::path path;
        for (i32 index = 0;; ++index)
        {
            const std::string name = index == 0 ? "NewScript.cs" : std::format("NewScript{}.cs", index);
            path = m_CurrentDirectory / PathFromUtf8(name);
            if (!fs::exists(root / path, error))
                break;
        }

        if (const Result<> written = WriteFileAtomic(root / path, MakeScriptSource(ToUtf8(path.stem()))); !written)
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), written.error());
            return;
        }

        AssetRegistry::Import(path);

        m_NewScript = path;
        RenameAfterRefresh(path);
    }

    void AssetsPanel::CreateScene()
    {
        const fs::path path = MakeUniquePath(m_CurrentDirectory, "New Scene", SceneExtension);

        if (const auto created = AssetManager::CreateScene(path); !created)
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), created.error());
            return;
        }

        RenameAfterRefresh(path);
    }

    void AssetsPanel::CreateMaterial()
    {
        const fs::path path = MakeUniquePath(m_CurrentDirectory, "New Material", MaterialExtension);

        if (const auto created = AssetManager::CreateMaterial(path); !created)
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), created.error());
            return;
        }

        RenameAfterRefresh(path);
    }

    void AssetsPanel::CreatePrefab(const Entity entity)
    {
        const fs::path path = MakeUniquePath(m_CurrentDirectory, MakeFileStem(entity.GetTag()), PrefabExtension);

        if (const auto created = GetContext().CreatePrefab(entity, path); !created)
        {
            APP_ERROR("Cannot create '{}': {}", ToGenericUtf8(path), created.error());
            return;
        }

        RenameAfterRefresh(path);
    }

    void AssetsPanel::RenameAfterRefresh(fs::path path)
    {
        m_RenameAfterRefresh = std::move(path);
        m_RefreshRequested = true;
    }

    bool AssetsPanel::IsSelected(const Entry& entry) const
    {
        if (const fs::path* local = GetLocalSelection())
            return entry.Path == *local;

        return entry.Handle && GetContext().SelectionContext.IsAsset(*entry.Handle);
    }

    const fs::path* AssetsPanel::GetLocalSelection() const
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
        else if (entry.Type == AssetType::ScriptGraph && entry.Handle)
            GetContext().Open<GraphEditorPanel>().OpenGraph(*entry.Handle);
        else if (entry.Type == AssetType::AnimationClip && entry.Handle)
            GetContext().Open<AnimationClipPanel>().OpenClip(*entry.Handle);
        else if (entry.Type == AssetType::InputActions && entry.Handle)
            GetContext().Open<InputActionsPanel>().OpenAsset(*entry.Handle);
        else if (entry.Type == AssetType::Script)
            GetContext().CodeEditor.OpenFile(AssetRegistry::GetAssetRoot() / entry.Path);
        else if (entry.Type == AssetType::Prefab && entry.Handle)
            GetContext().Dialogs.RequestOpenPrefab(*entry.Handle);
        else if (entry.Type == AssetType::Texture2D && entry.Handle)
            GetContext().Open<SpriteEditorPanel>().OpenTexture(*entry.Handle);
        else if (entry.Type == AssetType::Sprite && entry.Owner)
            GetContext().Open<SpriteEditorPanel>().OpenTexture(*entry.Owner, entry.Handle);
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

    const Sprite* AssetsPanel::GetSpritePreview(Entry& entry)
    {
        if (!entry.Handle) return nullptr;

        if (!entry.SpritePreview)
            entry.SpritePreview = AssetManager::LoadSprite(*entry.Handle);

        return entry.SpritePreview->GetTextureAsset()->GetState() == AssetLoadState::Loaded
             ? entry.SpritePreview.get() : nullptr;
    }

    EditorIcon AssetsPanel::GetIcon(const Entry& entry)
    {
        if (entry.IsDirectory)
            return EditorIcon::Folder;

        return EditorIcons::ForFile(entry.Path);
    }
}
