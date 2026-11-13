#include "Editor/Panels/AnimationClipPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/AssetPayload.h"

#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/Log.h>
#include <Engine/ImGui/ImGuiWidgets.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <format>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr const char* FramePayloadType = "ByteForge.AnimationFrame";

        constexpr f32 ThumbnailSize    = 72.0f;
        constexpr f32 ThumbnailPadding = 4.0f;
        constexpr usize MaxUndoSteps   = 100;

        constexpr ImU32 FrameBackground = IM_COL32(40,  40,  40,  255);
        constexpr ImU32 SelectedOutline = IM_COL32(90,  160, 255, 255);
        constexpr ImU32 PlayingOutline  = IM_COL32(255, 200, 80,  255);

        constexpr std::array LoopModes{
            AnimationLoopMode::Loop,
            AnimationLoopMode::Once,
            AnimationLoopMode::PingPong
        };

        struct SliceSettings
        {
            UUID Texture{ 0 };
            i32 Columns = 4;
            i32 Rows = 1;
            i32 FirstCell = 0;
            i32 Count = 0;
        };

        ImVec2 ToImVec2(const glm::vec2& value) { return { value.x, value.y }; }

        void DrawSprite(ImDrawList& drawList, const Sprite& sprite, const ImVec2& min, const ImVec2& max)
        {
            const Ref<Texture2D>& texture = sprite.GetTexture();
            const glm::vec2 uvSize = glm::abs(sprite.GetUVMax() - sprite.GetUVMin());
            const f32 width = static_cast<f32>(texture->GetWidth()) * uvSize.x;
            const f32 height = static_cast<f32>(texture->GetHeight()) * uvSize.y;
            if (width <= 0.0f || height <= 0.0f) return;

            const ImVec2 bounds(max.x - min.x, max.y - min.y);
            const f32 scale = std::min(bounds.x / width, bounds.y / height);
            const ImVec2 size(width * scale, height * scale);
            const ImVec2 imageMin(min.x + (bounds.x - size.x) * 0.5f, min.y + (bounds.y - size.y) * 0.5f);

            UI::DrawImage(drawList, texture, imageMin, ImVec2(imageMin.x + size.x, imageMin.y + size.y),
                          IM_COL32_WHITE, ToImVec2(sprite.GetUVMin()), ToImVec2(sprite.GetUVMax()));
        }

        AnimationFrame MakeFrame(const UUID texture) { return { .Texture = texture }; }

        std::vector<AnimationFrame> SliceGrid(const SliceSettings& settings)
        {
            const i32 columns = std::max(1, settings.Columns);
            const i32 rows = std::max(1, settings.Rows);
            const i32 cells = columns * rows;
            const i32 first = std::clamp(settings.FirstCell, 0, cells - 1);
            const i32 end = settings.Count > 0 ? std::min(cells, first + settings.Count) : cells;

            const auto columnCount = static_cast<f32>(columns);
            const auto rowCount = static_cast<f32>(rows);

            std::vector<AnimationFrame> frames;
            frames.reserve(static_cast<usize>(end - first));

            for (i32 cell = first; cell < end; ++cell)
            {
                const auto column = static_cast<f32>(cell % columns);
                const auto row = static_cast<f32>(cell / columns);

                AnimationFrame frame = MakeFrame(settings.Texture);
                frame.UVMin = { column / columnCount, row / rowCount };
                frame.UVMax = { (column + 1.0f) / columnCount, (row + 1.0f) / rowCount };
                frames.push_back(frame);
            }

            return frames;
        }

        isize Offset(const usize index) { return static_cast<isize>(index); }
    }

    AnimationClipPanel::AnimationClipPanel(EditorContext& context)
        : EditorPanel(context, Name) {}

    void AnimationClipPanel::OpenClip(const UUID asset)
    {
        m_Open = true;
        RequestFocus();

        if (m_Asset && static_cast<u64>(*m_Asset) == static_cast<u64>(asset)) return;

        if (m_Asset && m_Dirty)
        {
            OpenUnsavedChangesPopup(asset);
            return;
        }

        Load(asset);
    }

    void AnimationClipPanel::Load(const UUID asset)
    {
        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(asset, metadata) || metadata.Type != AssetType::AnimationClip)
        {
            APP_ERROR("Cannot open {}: not an animation clip", static_cast<u64>(asset));
            return;
        }

        m_Asset = asset;
        m_Name = metadata.Path.stem().string();
        m_Working = AssetManager::LoadAnimationClip(asset)->GetData();

        m_UndoStack.clear();
        m_RedoStack.clear();
        m_EditStart.reset();
        m_Dirty = false;

        m_Selected = m_Working.Frames.empty() ? std::nullopt : std::optional<usize>(0);
        m_Playing = false;
        m_PreviewTime = 0.0f;

        RebuildPreview();
    }

    bool AnimationClipPanel::Save()
    {
        if (!m_Asset)
            return false;

        AssetManager::SetAnimationClip(*m_Asset, m_Working);
        if (!AssetManager::SaveAnimationClip(*m_Asset))
            return false;

        m_Dirty = false;
        APP_INFO("Saved animation clip '{}'", m_Name);
        return true;
    }

    void AnimationClipPanel::OnUpdate(const Timestep ts)
    {
        if (!m_Playing || !m_Preview) return;

        m_PreviewTime += ts.GetSeconds();

        const f32 duration = m_Preview->GetDuration();
        if (m_Working.LoopMode == AnimationLoopMode::Once)
        {
            if (m_Preview->IsFinished(m_PreviewTime))
            {
                m_Playing = false;
                m_Selected = static_cast<usize>(m_Preview->SampleFrame(m_PreviewTime));
            }
        } else if (duration > 0.0f)
            m_PreviewTime = std::fmod(m_PreviewTime, 2.0f * duration);
    }

    void AnimationClipPanel::OnImGuiRender()
    {
        if (!m_Open) return;

        AssetMetadata metadata;
        if (m_Asset && !AssetRegistry::TryGetMetadata(*m_Asset, metadata))
        {
            m_Asset.reset();
            m_Preview.reset();
            m_Dirty = false;
        } else if (m_Asset)
            m_Name = metadata.Path.stem().string();

        const std::string title = m_Asset
                                ? std::format("{}{}{}", m_Name, m_Dirty ? " *" : "", WindowId)
                                : std::format("{}{}", Name, WindowId);

        if (ConsumeFocusRequest())
            ImGui::SetNextWindowFocus();

        ImGui::SetNextWindowSize(ImVec2(720.0f, 520.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title.c_str(), &m_Open))
        {
            if (!m_Asset || !m_Preview)
                ImGui::TextDisabled("Double-click an animation clip in the Assets panel to edit it");
            else
            {
                m_FrameStart = m_Working;

                HandleShortcuts();
                DrawToolbar();
                ImGui::Separator();

                const ImGuiStyle& style = ImGui::GetStyle();
                const f32 stripHeight = ThumbnailSize + ImGui::GetTextLineHeightWithSpacing() + style.ScrollbarSize +
                                        style.WindowPadding.y * 2.0f + style.ItemSpacing.y;
                const f32 propertiesHeight = ImGui::GetFrameHeightWithSpacing() * 3.0f +
                                             ImGui::GetTextLineHeightWithSpacing() * 2.0f;
                const f32 previewHeight = std::max(80.0f, ImGui::GetContentRegionAvail().y - stripHeight -
                                                          propertiesHeight - style.ItemSpacing.y * 2.0f);

                DrawPreview(previewHeight);
                DrawFrameStrip(stripHeight);
                DrawFrameProperties();
            }
        }

        ImGui::End();
    }

    void AnimationClipPanel::HandleShortcuts()
    {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
            Save();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z))
            Undo();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) ||
            ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
            Redo();
    }

    void AnimationClipPanel::DrawToolbar()
    {
        ImGui::BeginDisabled(!m_Dirty);
        if (ImGui::Button("Save"))
            Save();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_UndoStack.empty());
        if (ImGui::Button("Undo"))
            Undo();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_RedoStack.empty());
        if (ImGui::Button("Redo"))
            Redo();
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::BeginDisabled(m_Working.Frames.empty());
        if (ImGui::Button(m_Playing ? "Pause###playback" : "Play###playback"))
            TogglePlayback();
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::DragFloat("FPS", &m_Working.FramesPerSecond, 0.1f, 0.1f, 120.0f,
                             "%.1f", ImGuiSliderFlags_AlwaysClamp))
            OnDataChanged();
        TrackItemEdit();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        AnimationLoopMode loopMode = m_Working.LoopMode;
        if (EditorUI::EnumCombo("Loop", loopMode, std::span<const AnimationLoopMode>(LoopModes)))
        {
            BeginChange();
            m_Working.LoopMode = loopMode;
            OnDataChanged();
        }

        ImGui::SameLine(0.0f, 16.0f);
        if (ImGui::Button("Slice Sheet..."))
            OpenSliceDialog();

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%zu frames, %.2f s", m_Working.Frames.size(), m_Preview->GetDuration());
    }

    void AnimationClipPanel::DrawPreview(const f32 height)
    {
        ImGui::BeginChild("##preview", ImVec2(0.0f, height), ImGuiChildFlags_Borders);

        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 max(min.x + avail.x, min.y + avail.y);

        ImDrawList& drawList = *ImGui::GetWindowDrawList();
        drawList.AddRectFilled(min, max, FrameBackground);

        if (m_Preview->IsEmpty())
        {
            constexpr const char* hint = "Drop textures on the frame strip or use Slice Sheet...";
            const ImVec2 textSize = ImGui::CalcTextSize(hint);
            ImGui::SetCursorScreenPos(ImVec2(min.x + (avail.x - textSize.x) * 0.5f,
                                             min.y + (avail.y - textSize.y) * 0.5f));
            ImGui::TextDisabled("%s", hint);
        } else if (const Ref<Sprite>& sprite = m_Preview->GetSprite(GetDisplayedFrame()))
        {
            constexpr f32 margin = 8.0f;
            DrawSprite(drawList, *sprite,
                       ImVec2(min.x + margin, min.y + margin), ImVec2(max.x - margin, max.y - margin));
        }

        ImGui::EndChild();
    }

    void AnimationClipPanel::DrawFrameStrip(const f32 height)
    {
        ImGui::BeginChild("##frames", ImVec2(0.0f, height),
                          ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

        std::function<void()> action;

        std::optional<usize> playing;
        if (m_Playing)
            playing = m_Preview->SampleFrame(m_PreviewTime);

        for (usize i = 0; i < m_Working.Frames.size(); ++i)
        {
            ImGui::PushID(static_cast<i32>(i));
            if (i > 0)
                ImGui::SameLine();

            ImGui::BeginGroup();

            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + ThumbnailSize, min.y + ThumbnailSize);

            if (ImGui::InvisibleButton("##frame", ImVec2(ThumbnailSize, ThumbnailSize)))
            {
                m_Playing = false;
                m_Selected = i;
            }

            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload(FramePayloadType, &i, sizeof(i));
                ImGui::Text("Frame %zu", i + 1);
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FramePayloadType);
                    payload != nullptr && payload->DataSize == static_cast<i32>(sizeof(usize)))
                {
                    usize from = 0;
                    std::memcpy(&from, payload->Data, sizeof(from));
                    action = [this, from, i] { MoveFrame(from, i); };
                } else if (const auto asset = ReadAssetPayload(ImGui::AcceptDragDropPayload(AssetPayloadType));
                           asset && asset->Type == AssetType::Texture2D)
                {
                    action = [this, i, handle = asset->Handle] { InsertFrame(i, MakeFrame(UUID(handle))); };
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem("##frameMenu"))
            {
                if (ImGui::MenuItem("Duplicate"))
                    action = [this, i] { DuplicateFrame(i); };
                if (ImGui::MenuItem("Delete", "Del"))
                    action = [this, i] { DeleteFrame(i); };
                ImGui::EndPopup();
            }

            ImDrawList& drawList = *ImGui::GetWindowDrawList();
            drawList.AddRectFilled(min, max, FrameBackground, 4.0f);

            if (const Ref<Sprite>& sprite = m_Preview->GetSprite(static_cast<u32>(i)))
            {
                DrawSprite(drawList, *sprite, ImVec2(min.x + ThumbnailPadding, min.y + ThumbnailPadding),
                           ImVec2(max.x - ThumbnailPadding, max.y - ThumbnailPadding));
            }

            if (playing == i)
                drawList.AddRect(min, max, PlayingOutline, 4.0f, 0, 2.0f);
            else if (m_Selected == i)
                drawList.AddRect(min, max, SelectedOutline, 4.0f, 0, 2.0f);

            const f32 duration = m_Working.Frames[i].Duration;
            if (duration != 1.0f)
                ImGui::TextDisabled("%zu  x%.1f", i + 1, duration);
            else
                ImGui::TextDisabled("%zu", i + 1);

            ImGui::EndGroup();
            ImGui::PopID();
        }

        if (!m_Working.Frames.empty())
            ImGui::SameLine();

        ImGui::Button("+##appendFrames", ImVec2(ThumbnailSize, ThumbnailSize));
        ImGui::SetItemTooltip("Drop textures here to append frames");

        if (ImGui::BeginDragDropTarget())
        {
            if (const auto asset = ReadAssetPayload(ImGui::AcceptDragDropPayload(AssetPayloadType));
                asset && asset->Type == AssetType::Texture2D)
            {
                action = [this, handle = asset->Handle]
                {
                    InsertFrame(m_Working.Frames.size(), MakeFrame(UUID(handle)));
                };
            } else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FramePayloadType);
                       payload != nullptr && payload->DataSize == static_cast<i32>(sizeof(usize)))
            {
                usize from = 0;
                std::memcpy(&from, payload->Data, sizeof(from));
                action = [this, from] { MoveFrame(from, m_Working.Frames.size() - 1); };
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsWindowFocused() && m_Selected && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            action = [this, index = *m_Selected] { DeleteFrame(index); };

        ImGui::EndChild();

        if (action)
            action();
    }

    void AnimationClipPanel::DrawFrameProperties()
    {
        if (!m_Selected || *m_Selected >= m_Working.Frames.size())
        {
            ImGui::TextDisabled("Select a frame to edit it");
            return;
        }

        const usize index = *m_Selected;
        AnimationFrame& frame = m_Working.Frames[index];

        ImGui::SeparatorText(std::format("Frame {}", index + 1).c_str());

        AssetMetadata metadata;
        const bool known = AssetRegistry::TryGetMetadata(frame.Texture, metadata);
        ImGui::TextDisabled("Texture: %s", known ? metadata.Path.generic_string().c_str() : "missing");

        if (ImGui::DragFloat("Duration", &frame.Duration, 0.05f, 0.05f, 20.0f, "x%.2f", ImGuiSliderFlags_AlwaysClamp))
            OnDataChanged();
        TrackItemEdit();
        ImGui::SetItemTooltip("Multiplier of the clip's frame time (1 / FPS)");

        if (ImGui::DragFloat2("UV Min", &frame.UVMin.x, 0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
            OnDataChanged();
        TrackItemEdit();

        if (ImGui::DragFloat2("UV Max", &frame.UVMax.x, 0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
            OnDataChanged();
        TrackItemEdit();
    }

    void AnimationClipPanel::OpenSliceDialog()
    {
        SliceSettings settings;
        if (m_Selected && *m_Selected < m_Working.Frames.size())
            settings.Texture = m_Working.Frames[*m_Selected].Texture;

        const UUID asset = *m_Asset;

        GetContext().Popups.Open("Slice Sprite Sheet", [this, settings, asset](Popup& popup) mutable
        {
            if (!m_Asset || static_cast<u64>(*m_Asset) != static_cast<u64>(asset))
            {
                popup.Close();
                return;
            }

            ImGui::PushItemWidth(200.0f);

            EditorUI::AssetReferenceField("Texture", AssetType::Texture2D, settings.Texture,
                                          GetContext().SelectionContext);

            ImGui::DragInt("Columns", &settings.Columns, 0.1f, 1, 256, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragInt("Rows", &settings.Rows, 0.1f, 1, 256, "%d", ImGuiSliderFlags_AlwaysClamp);

            const i32 cells = settings.Columns * settings.Rows;
            ImGui::DragInt("First Cell", &settings.FirstCell, 0.1f, 0, cells - 1, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetItemTooltip("Cells are counted row by row from the top left, starting at 0");

            ImGui::DragInt("Count", &settings.Count, 0.1f, 0, cells, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetItemTooltip("0 uses all cells from the first one");

            ImGui::PopItemWidth();

            const std::vector<AnimationFrame> frames = SliceGrid(settings);
            ImGui::Spacing();
            ImGui::TextDisabled("Creates %zu frames", frames.size());
            ImGui::Spacing();

            ImGui::BeginDisabled(static_cast<u64>(settings.Texture) == 0);
            if (ImGui::Button("Append"))
            {
                AddFrames(frames, false);
                popup.Close();
            }
            ImGui::SameLine();
            if (ImGui::Button("Replace"))
            {
                AddFrames(frames, true);
                popup.Close();
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                popup.Close();
        });
    }

    void AnimationClipPanel::OpenUnsavedChangesPopup(const UUID next)
    {
        GetContext().Popups.Open("Unsaved Changes", [this, next](Popup& popup)
        {
            if (!m_Asset)
            {
                popup.Close();
                Load(next);
                return;
            }

            ImGui::Text("'%s' has unsaved changes.", m_Name.c_str());
            ImGui::Spacing();

            std::optional<bool> save;
            if (ImGui::Button("Save"))
                save = true;
            ImGui::SameLine();
            if (ImGui::Button("Discard"))
                save = false;
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                popup.Close();

            if (save)
            {
                popup.Close();
                if (!*save || Save())
                    Load(next);
            }
        });
    }

    void AnimationClipPanel::TogglePlayback()
    {
        if (m_Playing)
        {
            m_Playing = false;
            m_Selected = static_cast<usize>(m_Preview->SampleFrame(m_PreviewTime));
            return;
        }

        if (m_Preview->IsFinished(m_PreviewTime))
            m_PreviewTime = 0.0f;

        m_Playing = true;
    }

    u32 AnimationClipPanel::GetDisplayedFrame() const
    {
        if (m_Playing || !m_Selected)
            return m_Preview->SampleFrame(m_PreviewTime);

        return static_cast<u32>(*m_Selected);
    }

    void AnimationClipPanel::BeginChange() { PushUndo(m_Working); }

    void AnimationClipPanel::TrackItemEdit()
    {
        if (ImGui::IsItemActivated())
            m_EditStart = m_FrameStart;

        if (ImGui::IsItemDeactivatedAfterEdit() && m_EditStart)
            PushUndo(std::move(*m_EditStart));

        if (ImGui::IsItemDeactivated())
            m_EditStart.reset();
    }

    void AnimationClipPanel::PushUndo(AnimationClipData snapshot)
    {
        m_UndoStack.push_back(std::move(snapshot));
        if (m_UndoStack.size() > MaxUndoSteps)
            m_UndoStack.erase(m_UndoStack.begin());

        m_RedoStack.clear();
    }

    void AnimationClipPanel::Undo()
    {
        if (m_UndoStack.empty()) return;

        m_RedoStack.push_back(std::move(m_Working));
        m_Working = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        OnDataChanged();
    }

    void AnimationClipPanel::Redo()
    {
        if (m_RedoStack.empty()) return;

        m_UndoStack.push_back(std::move(m_Working));
        m_Working = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        OnDataChanged();
    }

    void AnimationClipPanel::OnDataChanged()
    {
        m_Dirty = true;
        RebuildPreview();
    }

    void AnimationClipPanel::RebuildPreview()
    {
        m_Preview = MakeScope<AnimationClipAsset>(*m_Asset, m_Working);

        if (m_Working.Frames.empty())
            m_Selected.reset();
        else if (m_Selected && *m_Selected >= m_Working.Frames.size())
            m_Selected = m_Working.Frames.size() - 1;
    }

    void AnimationClipPanel::InsertFrame(const usize index, const AnimationFrame& frame)
    {
        BeginChange();

        const usize at = std::min(index, m_Working.Frames.size());
        m_Working.Frames.insert(m_Working.Frames.begin() + Offset(at), frame);
        m_Selected = at;

        OnDataChanged();
    }

    void AnimationClipPanel::AddFrames(const std::vector<AnimationFrame>& frames, const bool replace)
    {
        if (frames.empty()) return;

        BeginChange();

        if (replace)
            m_Working.Frames.clear();

        const usize first = m_Working.Frames.size();
        m_Working.Frames.insert(m_Working.Frames.end(), frames.begin(), frames.end());
        m_Selected = first;

        OnDataChanged();
    }

    void AnimationClipPanel::MoveFrame(const usize from, const usize to)
    {
        const usize count = m_Working.Frames.size();
        if (from >= count || to >= count || from == to) return;

        BeginChange();

        const AnimationFrame frame = m_Working.Frames[from];
        m_Working.Frames.erase(m_Working.Frames.begin() + Offset(from));
        m_Working.Frames.insert(m_Working.Frames.begin() + Offset(to), frame);
        m_Selected = to;

        OnDataChanged();
    }

    void AnimationClipPanel::DeleteFrame(const usize index)
    {
        if (index >= m_Working.Frames.size()) return;

        BeginChange();
        m_Working.Frames.erase(m_Working.Frames.begin() + Offset(index));
        m_Selected = index;
        OnDataChanged();
    }

    void AnimationClipPanel::DuplicateFrame(const usize index)
    {
        if (index >= m_Working.Frames.size()) return;

        const AnimationFrame copy = m_Working.Frames[index];
        InsertFrame(index + 1, copy);
    }
}
