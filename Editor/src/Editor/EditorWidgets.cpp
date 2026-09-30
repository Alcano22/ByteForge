#include "Editor/EditorWidgets.h"
#include "Editor/AssetPayload.h"
#include "Editor/StringUtils.h"

#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/ImGui/ImGuiWidgets.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge::EditorUI
{
    namespace
    {
        constexpr float FieldPadding = 4.0f;
        constexpr float PreviewSize = 64.0f;
        constexpr float TooltipPreviewSize = 192.0f;
        constexpr float PickerThumbnailSize = 32.0f;
        constexpr const char* PickerPopupId = "##TexturePicker";

        constexpr ImU32 CheckerLight = IM_COL32(88, 88, 88, 255);
        constexpr ImU32 CheckerDark  = IM_COL32(62, 62, 62, 255);
        constexpr ImVec4 ErrorColor{ 0.90f, 0.35f, 0.35f, 1.0f };

        ImVec2 FitToBounds(const Texture2D& texture, const ImVec2& bounds)
        {
            const float width = static_cast<float>(texture.GetWidth());
            const float height = static_cast<float>(texture.GetHeight());
            const float scale = std::min(bounds.x / width, bounds.y / height);
            return { width * scale, height * scale };
        }

        void DrawCheckerboard(ImDrawList& drawList, const ImVec2& min, const ImVec2& max)
        {
            const float cell = std::max(4.0f, std::floor((max.x - min.x) / 8.0f));
            const int columns = static_cast<int>(std::ceil((max.x - min.x) / cell));
            const int rows = static_cast<int>(std::ceil((max.y - min.y) / cell));

            drawList.AddRectFilled(min, max, CheckerDark);
            for (int row = 0; row < rows; ++row)
            {
                for (int column = 0; column < columns; ++column)
                {
                    if ((row + column) % 2 != 0) continue;

                    const ImVec2 cellMin{ min.x + static_cast<float>(column) * cell,
                                          min.y + static_cast<float>(row) * cell };
                    const ImVec2 cellMax{ std::min(cellMin.x + cell, max.x), std::min(cellMin.y + cell, max.y) };
                    drawList.AddRectFilled(cellMin, cellMax, CheckerLight);
                }
            }
        }

        void DrawAssetPreview(ImDrawList& drawList, const TextureAsset* asset, const ImVec2& min, const ImVec2& size)
        {
            const ImVec2 max{ min.x + size.x, min.y + size.y };
            const char* overlay = nullptr;

            if (asset == nullptr)
            {
                drawList.AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBgActive));
                overlay = "None";
            } else
            {
                switch (asset->GetState())
                {
                    case AssetLoadState::Loaded:
                    {
                        DrawCheckerboard(drawList, min, max);

                        const Ref<Texture2D>& texture = asset->GetTexture();
                        const ImVec2 fitted = FitToBounds(*texture, size);
                        const ImVec2 imageMin{ min.x + (size.x - fitted.x) * 0.5f, min.y + (size.y - fitted.y) * 0.5f };
                        UI::DrawImage(drawList, texture, imageMin, ImVec2(imageMin.x + fitted.x, imageMin.y + fitted.y));
                        break;
                    }
                    case AssetLoadState::Loading:
                        DrawCheckerboard(drawList, min, max);
                        overlay = "...";
                        break;
                    case AssetLoadState::Failed:
                        drawList.AddRectFilled(min, max,
                                               ImGui::GetColorU32(ImGui::ColorConvertFloat4ToU32(ErrorColor), 0.35f));
                        overlay = "!";
                        break;
                }
            }

            if (overlay != nullptr)
            {
                const ImVec2 textSize = ImGui::CalcTextSize(overlay);
                if (textSize.x < size.x - 4.0f)
                {
                    drawList.AddText(ImVec2(min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f),
                                     ImGui::GetColorU32(ImGuiCol_TextDisabled), overlay);
                }
            }

            drawList.AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border));
        }

        void DrawAssetInfo(const TextureAsset* asset)
        {
            if (asset == nullptr)
            {
                ImGui::TextUnformatted("None");
                ImGui::TextDisabled("Drop a texture here");
                return;
            }

            AssetMetadata metadata;
            if (!AssetRegistry::TryGetMetadata(asset->GetHandle(), metadata))
            {
                ImGui::TextColored(ErrorColor, "Missing asset");
                return;
            }

            ImGui::TextUnformatted(metadata.Path.filename().string().c_str());

            switch (asset->GetState())
            {
                case AssetLoadState::Loading:
                    ImGui::TextDisabled("Loading...");
                    break;
                case AssetLoadState::Failed:
                    ImGui::TextColored(ErrorColor, "Failed to load");
                    break;
                case AssetLoadState::Loaded:
                {
                    const Texture2D& texture = *asset->GetTexture();
                    ImGui::TextDisabled("%u x %u, %s", texture.GetWidth(), texture.GetHeight(),
                                        texture.GetFilter() == TextureFilter::Nearest ? "Nearest" : "Linear");
                    break;
                }
            }
        }

        void DrawPreviewTooltip(const TextureAsset& asset)
        {
            if (asset.GetState() != AssetLoadState::Loaded || !ImGui::BeginItemTooltip()) return;

            const ImVec2 size = FitToBounds(*asset.GetTexture(), ImVec2(TooltipPreviewSize, TooltipPreviewSize));
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::Dummy(size);
            DrawAssetPreview(*ImGui::GetWindowDrawList(), &asset, min, size);

            ImGui::TextDisabled("Double-click to open import settings");
            ImGui::EndTooltip();
        }

        bool AcceptTextureDrop(ImDrawList& drawList, Ref<TextureAsset>& asset, const ImVec2& min, const ImVec2& max)
        {
            const std::optional<AssetPayload> dragged = ReadAssetPayload(ImGui::GetDragDropPayload());
            if (!dragged)
                return false;

            const float rounding = ImGui::GetStyle().FrameRounding;
            const bool compatible = dragged->Type == AssetType::Texture2D;

            if (compatible)
                drawList.AddRect(min, max, ImGui::GetColorU32(ImGuiCol_DragDropTarget, 0.35f), rounding);

            if (!ImGui::BeginDragDropTarget())
                return false;

            bool changed = false;
            constexpr ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery
                                               | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType, flags))
            {
                const ImU32 color = compatible ? ImGui::GetColorU32(ImGuiCol_DragDropTarget)
                                               : ImGui::GetColorU32(ErrorColor);
                drawList.AddRect(min, max, color, rounding, 0, 2.0f);

                const UUID handle(dragged->Handle);
                if (compatible && payload->IsDelivery() && (!asset || asset->GetHandle() != handle))
                {
                    asset = AssetManager::LoadTexture2D(handle);
                    changed = true;
                }
            }

            ImGui::EndDragDropTarget();
            return changed;
        }

        void DrawPickerRow(const AssetMetadata& metadata, const ImVec2& min, const float rowHeight)
        {
            ImDrawList& drawList = *ImGui::GetWindowDrawList();

            const Ref<TextureAsset> candidate = AssetManager::LoadTexture2D(metadata.Handle);
            const ImVec2 thumbnailMin{ min.x, min.y + (rowHeight - PickerThumbnailSize) * 0.5f };
            DrawAssetPreview(drawList, candidate.get(), thumbnailMin,
                             ImVec2(PickerThumbnailSize, PickerThumbnailSize));

            const std::string name = metadata.Path.filename().string();
            const std::string folder = metadata.Path.has_parent_path()
                                     ? "Assets/" + metadata.Path.parent_path().generic_string() : "Assets";

            const float lineHeight = ImGui::GetTextLineHeight();
            const ImVec2 textPos{ thumbnailMin.x + PickerThumbnailSize + ImGui::GetStyle().ItemSpacing.x,
                                  min.y + (rowHeight - lineHeight * 2.0f) * 0.5f };

            drawList.AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
            drawList.AddText(ImVec2(textPos.x, textPos.y + lineHeight), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                             folder.c_str());
        }

        bool DrawTexturePicker(Ref<TextureAsset>& asset)
        {
            ImGui::SetNextWindowSize(ImVec2(320.0f, 360.0f), ImGuiCond_Appearing);
            if (!ImGui::BeginPopup(PickerPopupId))
                return false;

            static std::array<char, 128> s_Filter{};
            static std::vector<AssetMetadata> s_Candidates;

            if (ImGui::IsWindowAppearing())
            {
                s_Filter.fill('\0');
                s_Candidates = AssetRegistry::GetAssetsOfType(AssetType::Texture2D);
                ImGui::SetKeyboardFocusHere();
            }

            ImGui::SetNextItemWidth(-FLT_MIN);
            const bool submitted = ImGui::InputTextWithHint("##search", "Search textures...", s_Filter.data(),
                                                            s_Filter.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Separator();

            bool changed = false;
            ImGui::BeginChild("##results");

            if (ImGui::Selectable("None", asset == nullptr) && asset)
            {
                asset.reset();
                changed = true;
            }

            const std::string_view query(s_Filter.data());
            const float rowHeight = std::max(PickerThumbnailSize, ImGui::GetTextLineHeight() * 2.0f + 2.0f);
            bool anyMatch = false;

            for (const AssetMetadata& metadata : s_Candidates)
            {
                const std::string path = metadata.Path.generic_string();
                if (!ContainsIgnoreCase(path, query)) continue;

                const bool firstMatch = !anyMatch;
                anyMatch = true;

                ImGui::PushID(path.c_str());

                const bool selected = asset && asset->GetHandle() == metadata.Handle;
                const bool clicked = ImGui::Selectable("##row", selected, ImGuiSelectableFlags_None,
                                                       ImVec2(0.0f, rowHeight));
                if (ImGui::IsItemVisible())
                    DrawPickerRow(metadata, ImGui::GetItemRectMin(), rowHeight);

                const bool pickedByEnter = submitted && firstMatch;
                if ((clicked || pickedByEnter) && !selected)
                {
                    asset = AssetManager::LoadTexture2D(metadata.Handle);
                    changed = true;
                }
                if (pickedByEnter)
                    ImGui::CloseCurrentPopup();

                ImGui::PopID();
            }

            if (!anyMatch)
                ImGui::TextDisabled("No textures match");

            ImGui::EndChild();
            ImGui::EndPopup();
            return changed;
        }
    }

    bool TextureAssetField(const char* label, Ref<TextureAsset>& asset, Selection& selection)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList& drawList = *ImGui::GetWindowDrawList();
        bool changed = false;

        ImGui::PushID(label);

        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 fieldSize{ std::max(ImGui::GetContentRegionAvail().x, 180.0f), PreviewSize + FieldPadding * 2.0f };
        const ImVec2 end{ start.x + fieldSize.x, start.y + fieldSize.y };

        drawList.AddRectFilled(start, end, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);

        const ImVec2 previewMin{ start.x + FieldPadding, start.y + FieldPadding };
        ImGui::SetCursorScreenPos(previewMin);
        ImGui::InvisibleButton("##preview", ImVec2(PreviewSize, PreviewSize));
        DrawAssetPreview(drawList, asset.get(), previewMin, ImVec2(PreviewSize, PreviewSize));

        if (asset)
        {
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                selection.SelectAsset(asset->GetHandle());
            else if (ImGui::GetDragDropPayload() == nullptr)
                DrawPreviewTooltip(*asset);
        }

        ImGui::SetCursorScreenPos(ImVec2(previewMin.x + PreviewSize + style.ItemSpacing.x, previewMin.y));
        ImGui::BeginGroup();
        ImGui::TextDisabled("%s", label);
        DrawAssetInfo(asset.get());
        ImGui::EndGroup();

        const float browseWidth = ImGui::CalcTextSize("Browse").x + style.FramePadding.x * 2.0f;
        const float clearWidth = asset
            ? ImGui::CalcTextSize("Clear").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x : 0.0f;
        ImGui::SetCursorScreenPos(ImVec2(end.x - FieldPadding - browseWidth - clearWidth, previewMin.y));

        if (asset)
        {
            if (ImGui::SmallButton("Clear"))
            {
                asset.reset();
                changed = true;
            }
            ImGui::SameLine();
        }

        if (ImGui::SmallButton("Browse"))
            ImGui::OpenPopup(PickerPopupId);

        ImGui::SetCursorScreenPos(start);
        ImGui::Dummy(fieldSize);
        changed |= AcceptTextureDrop(drawList, asset, start, end);

        changed |= DrawTexturePicker(asset);

        ImGui::PopID();
        return changed;
    }
}
