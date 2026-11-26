#include "Editor/Panels/AnimationClipPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/AssetPayload.h"
#include "Editor/Panels/SpriteEditorPanel.h"

#include <Engine/Core/Log.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/SpriteSheet.h>
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

        std::vector<AnimationFrame> FramesFromAsset(const AssetPayload& asset)
        {
            std::vector<AnimationFrame> frames;

            if (asset.Type == AssetType::Sprite)
                frames.push_back(AnimationFrame{ .Sprite = UUID(asset.Handle) });
            else if (asset.Type == AssetType::Texture2D)
            {
                for (const UUID sprite : AssetRegistry::GetSubAssets(UUID(asset.Handle)))
                    frames.push_back(AnimationFrame{ .Sprite = sprite });
            }

            return frames;
        }

        bool AcceptFrameDrop(std::vector<AnimationFrame>& frames)
        {
            const std::optional<AssetPayload> dragged = ReadAssetPayload(ImGui::GetDragDropPayload());
            if (!dragged) return false;

            std::vector<AnimationFrame> candidates = FramesFromAsset(*dragged);
            if (candidates.empty() || ImGui::AcceptDragDropPayload(AssetPayloadType) == nullptr)
                return false;

            frames = std::move(candidates);
            return true;
        }

        UUID TextureOf(const AnimationFrame& frame)
        {
            if (!frame.Sprite.IsSet())
                return frame.Texture;

            AssetMetadata metadata;
            return AssetRegistry::TryGetMetadata(frame.Sprite, metadata) ? metadata.Parent : UUID(0);
        }

        isize Offset(const usize index) { return static_cast<isize>(index); }
    }

    AnimationClipPanel::AnimationClipPanel(EditorContext& context)
        : EditorPanel(context, Name) {}

    void AnimationClipPanel::OpenClip(const UUID asset)
    {
        m_Open = true;
        RequestFocus();

        if (m_Asset && *m_Asset == asset) return;

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
        m_Name = ToUtf8(metadata.Path.stem());
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
            m_Name = ToUtf8(metadata.Path.stem());

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
        if (ImGui::Button("From Sheet..."))
            OpenSheetDialog();
        ImGui::SetItemTooltip("Add frames from the sprites of a texture");

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
            constexpr const char* hint = "Drop a sprite sheet or sprites on the frame strip, or use From Sheet...";
            const ImVec2 textSize = ImGui::CalcTextSize(hint);
            ImGui::SetCursorScreenPos(ImVec2(min.x + (avail.x - textSize.x) * 0.5f,
                                             min.y + (avail.y - textSize.y) * 0.5f));
            ImGui::TextDisabled("%s", hint);
        } else if (const Ref<Sprite>& sprite = m_Preview->GetSprite(GetDisplayedFrame()))
        {
            constexpr f32 margin = 8.0f;
            EditorUI::DrawSpriteImage(drawList, *sprite,
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
                std::vector<AnimationFrame> dropped;
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FramePayloadType);
                    payload != nullptr && payload->DataSize == static_cast<i32>(sizeof(usize)))
                {
                    usize from = 0;
                    std::memcpy(&from, payload->Data, sizeof(from));
                    action = [this, from, i] { MoveFrame(from, i); };
                } else if (AcceptFrameDrop(dropped))
                    action = [this, i, frames = std::move(dropped)] { InsertFrames(i, frames); };
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
                EditorUI::DrawSpriteImage(drawList, *sprite, ImVec2(min.x + ThumbnailPadding, min.y + ThumbnailPadding),
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
        ImGui::SetItemTooltip("Drop a sprite sheet or sprites here to append frames");

        if (ImGui::BeginDragDropTarget())
        {
            std::vector<AnimationFrame> dropped;
            if (AcceptFrameDrop(dropped))
            {
                action = [this, frames = std::move(dropped)] { InsertFrames(m_Working.Frames.size(), frames); };
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

        if (frame.Sprite.IsSet())
        {
            UUID sprite = frame.Sprite;
            ImGui::SetNextItemWidth(240.0f);
            if (EditorUI::AssetReferenceField("Sprite", AssetType::Sprite, sprite, GetContext().SelectionContext)
                && sprite.IsSet())
            {
                BeginChange();
                frame.Sprite = sprite;
                OnDataChanged();
            }
        } else
        {
            AssetMetadata metadata;
            const bool known = AssetRegistry::TryGetMetadata(frame.Texture, metadata);
            ImGui::TextDisabled("Texture region of %s, from before sprite assets",
                                known ? ToGenericUtf8(metadata.Path).c_str() : "a missing texture");
        }

        if (ImGui::DragFloat("Duration", &frame.Duration, 0.05f, 0.05f, 20.0f, "x%.2f", ImGuiSliderFlags_AlwaysClamp))
            OnDataChanged();
        TrackItemEdit();
        ImGui::SetItemTooltip("Multiplier of the clip's frame time (1 / FPS)");

        if (frame.Sprite.IsSet()) return;

        if (ImGui::DragFloat2("UV Min", &frame.UVMin.x, 0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
            OnDataChanged();
        TrackItemEdit();

        if (ImGui::DragFloat2("UV Max", &frame.UVMax.x, 0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
            OnDataChanged();
        TrackItemEdit();
    }

    void AnimationClipPanel::OpenSheetDialog()
    {
        UUID texture{ 0 };
        if (m_Selected && *m_Selected < m_Working.Frames.size())
            texture = TextureOf(m_Working.Frames[*m_Selected]);

        const UUID asset = *m_Asset;

        GetContext().Popups.Open("Frames from Sprite Sheet", [this, asset, texture, first = 0, count = 0](Popup& popup) mutable
        {
            if (!m_Asset || *m_Asset != asset)
            {
                popup.Close();
                return;
            }

            ImGui::PushItemWidth(200.0f);

            EditorUI::AssetReferenceField("Texture", AssetType::Texture2D, texture, GetContext().SelectionContext);

            const std::vector<UUID> sprites = texture.IsSet() ? AssetRegistry::GetSubAssets(texture) : std::vector<UUID>{};
            const i32 total = static_cast<i32>(sprites.size());

            ImGui::DragInt("First Sprite", &first, 0.1f, 0, std::max(0, total - 1), "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetItemTooltip("Sprites are counted in sheet order, starting at 0");

            ImGui::DragInt("Count", &count, 0.1f, 0, total, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetItemTooltip("0 uses all sprites from the first one");

            ImGui::PopItemWidth();

            const usize begin = static_cast<usize>(std::clamp(first, 0, total));
            const usize end = count > 0 ? std::min(sprites.size(), begin + static_cast<usize>(count)) : sprites.size();

            std::vector<AnimationFrame> frames;
            for (usize i = begin; i < end; ++i)
                frames.push_back(AnimationFrame{ .Sprite = sprites[i] });

            AssetMetadata metadata;
            const auto* settings = texture.IsSet() && AssetRegistry::TryGetMetadata(texture, metadata)
                                 ? metadata.GetSettings<TextureAssetSettings>() : nullptr;

            ImGui::Spacing();
            ImGui::TextDisabled("Creates %zu frames", frames.size());

            if (settings != nullptr && settings->Mode == SpriteMode::Single)
            {
                ImGui::TextDisabled("This texture is not sliced, so it is a single frame.");
                if (ImGui::SmallButton("Slice it in the Sprite Editor"))
                {
                    popup.Close();
                    GetContext().Open<SpriteEditorPanel>().OpenTexture(texture);
                    return;
                }
            }

            ImGui::Spacing();

            ImGui::BeginDisabled(frames.empty());
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

    void AnimationClipPanel::InsertFrames(const usize index, const std::vector<AnimationFrame>& frames)
    {
        if (frames.empty()) return;

        BeginChange();

        const usize at = std::min(index, m_Working.Frames.size());
        m_Working.Frames.insert(m_Working.Frames.begin() + Offset(at), frames.begin(), frames.end());
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
        InsertFrames(index + 1, { copy });
    }
}
