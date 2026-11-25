#include "Editor/EditorWidgets.h"
#include "Editor/EditorIcons.h"
#include "Editor/AssetPayload.h"

#include <Engine/Core/StringUtils.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scripting/ComponentReference.h>
#include <Engine/ImGui/ImGuiWidgets.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge::EditorUI
{
    namespace
    {
        constexpr f32 FieldPadding = 4.0f;
        constexpr f32 PreviewSize = 64.0f;
        constexpr f32 TooltipPreviewSize = 192.0f;
        constexpr f32 PickerThumbnailSize = 32.0f;
        constexpr const char* PickerPopupId = "##TexturePicker";
        constexpr const char* AssetPickerPopupId = "##AssetPicker";

        constexpr ImU32 CheckerLight = IM_COL32(88, 88, 88, 255);
        constexpr ImU32 CheckerDark  = IM_COL32(62, 62, 62, 255);

        ImVec2 FitToBounds(const Texture2D& texture, const ImVec2& bounds)
        {
            const f32 width = static_cast<f32>(texture.GetWidth());
            const f32 height = static_cast<f32>(texture.GetHeight());
            const f32 scale = std::min(bounds.x / width, bounds.y / height);
            return { width * scale, height * scale };
        }

        void DrawCheckerboard(ImDrawList& drawList, const ImVec2& min, const ImVec2& max)
        {
            const f32 cell = std::max(4.0f, std::floor((max.x - min.x) / 8.0f));
            const i32 columns = static_cast<i32>(std::ceil((max.x - min.x) / cell));
            const i32 rows = static_cast<i32>(std::ceil((max.y - min.y) / cell));

            drawList.AddRectFilled(min, max, CheckerDark);
            for (i32 row = 0; row < rows; ++row)
            {
                for (i32 column = 0; column < columns; ++column)
                {
                    if ((row + column) % 2 != 0) continue;

                    const ImVec2 cellMin{ min.x + static_cast<f32>(column) * cell,
                                          min.y + static_cast<f32>(row) * cell };
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
                ErrorText("Missing asset");
                return;
            }

            ImGui::TextUnformatted(ToUtf8(metadata.Path.filename()).c_str());

            switch (asset->GetState())
            {
                case AssetLoadState::Loading:
                    ImGui::TextDisabled("Loading...");
                    break;
                case AssetLoadState::Failed:
                    ErrorText("Failed to load");
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

            const f32 rounding = ImGui::GetStyle().FrameRounding;
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

        void DrawPickerRow(const AssetMetadata& metadata, const ImVec2& min, const f32 rowHeight)
        {
            ImDrawList& drawList = *ImGui::GetWindowDrawList();

            const Ref<TextureAsset> candidate = AssetManager::LoadTexture2D(metadata.Handle);
            const ImVec2 thumbnailMin{ min.x, min.y + (rowHeight - PickerThumbnailSize) * 0.5f };
            DrawAssetPreview(drawList, candidate.get(), thumbnailMin,
                             ImVec2(PickerThumbnailSize, PickerThumbnailSize));

            const std::string name = ToUtf8(metadata.Path.filename());
            const std::string folder = metadata.Path.has_parent_path()
                                     ? "Assets/" + ToGenericUtf8(metadata.Path.parent_path()) : "Assets";

            const f32 lineHeight = ImGui::GetTextLineHeight();
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

            static std::string s_Filter;
            static std::vector<AssetMetadata> s_Candidates;

            if (ImGui::IsWindowAppearing())
                s_Candidates = AssetRegistry::GetAssetsOfType(AssetType::Texture2D);

            const bool submitted = PopupSearchField("Search textures...", s_Filter);
            ImGui::Separator();

            bool changed = false;
            ImGui::BeginChild("##results");

            if (ImGui::Selectable("None", asset == nullptr) && asset)
            {
                asset.reset();
                changed = true;
            }

            const std::string_view query = s_Filter;
            const f32 rowHeight = std::max(PickerThumbnailSize, ImGui::GetTextLineHeight() * 2.0f + 2.0f);
            bool anyMatch = false;

            for (const AssetMetadata& metadata : s_Candidates)
            {
                const std::string path = ToGenericUtf8(metadata.Path);
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

        std::string AssetLabel(const AssetMetadata& metadata)
        {
            return std::format("{}  {}", EditorIcons::Glyph(EditorIcons::ForFile(metadata.Path)),
                               ToUtf8(metadata.Path.stem()));
        }

        bool DrawAssetPicker(const AssetType type, UUID& handle)
        {
            ImGui::SetNextWindowSize(ImVec2(300.0f, 280.0f), ImGuiCond_Appearing);
            if (!ImGui::BeginPopup(AssetPickerPopupId))
                return false;

            static std::string s_Filter;
            static std::vector<AssetMetadata> s_Candidates;

            if (ImGui::IsWindowAppearing())
                s_Candidates = AssetRegistry::GetAssetsOfType(type);

            const bool submitted = PopupSearchField("Search...", s_Filter);
            ImGui::Separator();

            bool changed = false;
            const bool hasAsset = handle.IsSet();

            ImGui::BeginChild("##results");

            if (ImGui::Selectable("None", !hasAsset) && hasAsset)
            {
                handle = UUID(0);
                changed = true;
            }

            const std::string_view query = s_Filter;
            bool anyMatch = false;

            for (const AssetMetadata& metadata : s_Candidates)
            {
                const std::string path = ToGenericUtf8(metadata.Path);
                if (!ContainsIgnoreCase(path, query)) continue;

                const bool pickedByEnter = submitted && !anyMatch;
                anyMatch = true;

                ImGui::PushID(path.c_str());

                const bool selected = handle == metadata.Handle;
                if ((ImGui::Selectable(AssetLabel(metadata).c_str(), selected) || pickedByEnter) && !selected)
                {
                    handle = metadata.Handle;
                    changed = true;
                }
                ImGui::SetItemTooltip("Assets/%s", path.c_str());

                if (pickedByEnter)
                    ImGui::CloseCurrentPopup();

                ImGui::PopID();
            }

            if (!anyMatch)
                ImGui::TextDisabled("No assets match");

            ImGui::EndChild();
            ImGui::EndPopup();
            return changed;
        }
    }

    void ErrorText(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        ImGui::TextColoredV(ErrorColor, format, args);
        va_end(args);
    }

    void ErrorTextWrapped(const char* format, ...)
    {
        const ScopedStyleColor color(ImGuiCol_Text, ErrorColor);

        va_list args;
        va_start(args, format);
        ImGui::TextWrappedV(format, args);
        va_end(args);
    }

    void DrawInsertionLine(const f32 y, const f32 minX, const f32 maxX)
    {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(minX, y), ImVec2(maxX, y),
                                            ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
    }

    void DrawInsertionLine(const f32 y)
    {
        DrawInsertionLine(y, ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().x);
    }

    bool PopupSearchField(const char* hint, std::string& query, const f32 width)
    {
        if (ImGui::IsWindowAppearing())
        {
            query.clear();
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::SetNextItemWidth(width);
        return ImGui::InputTextWithHint("##search", hint, &query, ImGuiInputTextFlags_EnterReturnsTrue);
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

        const f32 browseWidth = ButtonWidth("Browse");
        const f32 clearWidth = asset ? ButtonWidth("Clear") + style.ItemSpacing.x : 0.0f;
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

    bool AssetReferenceField(const char* label, const AssetType type, UUID& handle,
                             Selection& selection, const char* noneText)
    {
        bool changed = false;
        ImGui::PushID(label);

        const bool hasAsset = handle.IsSet();
        AssetMetadata metadata;
        const bool known = hasAsset && AssetRegistry::TryGetMetadata(handle, metadata);

        const std::string text = !hasAsset ? std::string(noneText) : known ? AssetLabel(metadata) : "Missing asset";
        const std::string buttonLabel = text + "###field";

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        {
            const ScopedStyleColor missingColor(ImGuiCol_Text, ErrorColor, hasAsset && !known);

            if (ImGui::Button(buttonLabel.c_str(), ImVec2(ImGui::CalcItemWidth(), 0.0f)))
                ImGui::OpenPopup(AssetPickerPopupId);
        }
        ImGui::PopStyleVar();

        if (known)
            ImGui::SetItemTooltip("Assets/%s", ToGenericUtf8(metadata.Path).c_str());

        if (ImGui::BeginDragDropTarget())
        {
            const std::optional<AssetPayload> dragged = ReadAssetPayload(ImGui::GetDragDropPayload());
            if (dragged && dragged->Type == type && ImGui::AcceptDragDropPayload(AssetPayloadType))
            {
                handle = UUID(dragged->Handle);
                changed = true;
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("##fieldMenu"))
        {
            if (ImGui::MenuItem("Select Asset", nullptr, false, known))
                selection.SelectAsset(handle);

            if (ImGui::MenuItem("Clear", nullptr, false, hasAsset))
            {
                handle = UUID(0);
                changed = true;
            }
            ImGui::EndPopup();
        }

        changed |= DrawAssetPicker(type, handle);

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(label);

        ImGui::PopID();
        return changed;
    }

    bool EntityReferenceField(const char* label, EntityRef& reference, Scene& scene)
    {
        bool changed = false;
        ImGui::PushID(label);

        const bool wantsComponent = !reference.Component.empty();
        const std::string_view typeLabel = wantsComponent ? ComponentTypeLabel(reference.Component) : std::string_view{};

        const auto accepts = [&](const Entity entity)
        {
            return !wantsComponent || HasComponentType(entity, reference.Component);
        };

        const Entity target = reference.IsSet() ? scene.FindEntityByUUID(reference.Id) : Entity{};
        const bool missing = reference.IsSet() && !target.IsValid();
        const bool lacksComponent = target.IsValid() && !accepts(target);

        std::string preview;
        if (!reference.IsSet())
            preview = wantsComponent ? std::format("None ({})", typeLabel) : "None";
        else if (missing)
            preview = "Missing entity";
        else if (lacksComponent)
            preview = std::format("{} (no {})", target.GetTag(), typeLabel);
        else
            preview = wantsComponent ? std::format("{} ({})", target.GetTag(), typeLabel) : target.GetTag();

        const bool error = missing || lacksComponent;

        bool open = false;
        {
            const ScopedStyleColor errorColor(ImGuiCol_Text, ErrorColor, error);
            open = ImGui::BeginCombo(label, preview.c_str());
        }

        if (open)
        {
            if (ImGui::Selectable("None", !reference.IsSet()) && reference.IsSet())
            {
                reference.Id = UUID(0);
                changed = true;
            }

            scene.Each<TagComponent>([&](const Entity entity, const TagComponent& tag)
            {
                if (!accepts(entity)) return;

                EditorUI::PushID(entity.GetUUID());

                const bool selected = target == entity;
                if (ImGui::Selectable(tag.Tag.c_str(), selected) && !selected)
                {
                    reference.Id = entity.GetUUID();
                    changed = true;
                }

                ImGui::PopID();
            });

            ImGui::EndCombo();
        }

        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* peek = ImGui::GetDragDropPayload();
            if (peek != nullptr && peek->IsDataType(EntityPayloadType) && peek->DataSize == static_cast<i32>(sizeof(u64)))
            {
                u64 id = 0;
                std::memcpy(&id, peek->Data, sizeof(id));

                const Entity dragged = scene.FindEntityByUUID(UUID(id));
                if (dragged.IsValid() && accepts(dragged) && ImGui::AcceptDragDropPayload(EntityPayloadType))
                {
                    reference.Id = UUID(id);
                    changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (lacksComponent)
        {
            ImGui::SetItemTooltip("'%s' has no %.*s anymore", target.GetTag().c_str(),
                                  static_cast<int>(typeLabel.size()), typeLabel.data());
        }

        ImGui::PopID();
        return changed;
    }

    bool IconButton(const char* id, const Ref<Texture2D>& icon, const bool active)
    {
        const f32 size = ImGui::GetTextLineHeight();

        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

        const bool pressed = UI::ImageButton(id, icon, ImVec2(size, size), ImGui::GetStyleColorVec4(ImGuiCol_Text));

        if (active)
            ImGui::PopStyleColor();

        return pressed;
    }
}
