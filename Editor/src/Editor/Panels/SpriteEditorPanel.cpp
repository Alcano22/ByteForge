#include "Editor/Panels/SpriteEditorPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/Log.h>
#include <Engine/ImGui/ImGuiWidgets.h>
#include <Engine/Renderer/ImageDecoder.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <initializer_list>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr usize MaxUndoSteps = 100;

        constexpr f32 PropertiesWidth = 280.0f;
        constexpr f32 HandleRadius = 5.0f;
        constexpr f32 HandleSize = 3.0f;
        constexpr f32 MinLabelWidth = 32.0f;

        constexpr f32 MinZoom = 0.05f;
        constexpr f32 MaxZoom = 64.0f;
        constexpr f32 ZoomStep = 1.2f;
        constexpr f32 FitMargin = 0.9f;
        constexpr f32 PixelGridZoom = 8.0f;

        constexpr ImU32 BackgroundColor      = IM_COL32(32, 32, 32, 255);
        constexpr ImU32 ImageBackgroundColor = IM_COL32(56, 56, 56, 255);
        constexpr ImU32 BorderColor          = IM_COL32(255, 255, 255, 70);
        constexpr ImU32 GridColor            = IM_COL32(255, 255, 255, 22);
        constexpr ImU32 RegionColor          = IM_COL32(255, 255, 255, 160);
        constexpr ImU32 SelectedColor        = IM_COL32(255, 196, 64, 255);
        constexpr ImU32 CreateColor          = IM_COL32(110, 190, 255, 255);

        enum Edge : u32
        {
            EdgeNone   = 0,
            EdgeLeft   = 1 << 0,
            EdgeRight  = 1 << 1,
            EdgeTop    = 1 << 2,
            EdgeBottom = 1 << 3
        };

        struct PivotPreset
        {
            const char* Name;
            f32 X;
            f32 Y;
        };

        constexpr std::array PivotPresets{
            PivotPreset{ "Center",        0.5f, 0.5f },
            PivotPreset{ "Bottom Center", 0.5f, 0.0f },
            PivotPreset{ "Bottom Left",   0.0f, 0.0f },
            PivotPreset{ "Bottom Right",  1.0f, 0.0f },
            PivotPreset{ "Top Center",    0.5f, 1.0f },
            PivotPreset{ "Top Left",      0.0f, 1.0f },
            PivotPreset{ "Top Right",     1.0f, 1.0f },
            PivotPreset{ "Left Center",   0.0f, 0.5f },
            PivotPreset{ "Right Center",  1.0f, 0.5f }
        };

        const char* PivotName(const glm::vec2 pivot)
        {
            for (const PivotPreset& preset : PivotPresets)
            {
                if (pivot.x == preset.X && pivot.y == preset.Y)
                    return preset.Name;
            }
            return nullptr;
        }

        bool PivotField(const char* label, glm::vec2& pivot)
        {
            const char* current = PivotName(pivot);
            bool changed = false;

            if (ImGui::BeginCombo(label, current != nullptr ? current : "Custom"))
            {
                for (const PivotPreset& preset : PivotPresets)
                {
                    if (ImGui::Selectable(preset.Name, current == preset.Name))
                    {
                        pivot = { preset.X, preset.Y };
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            return changed;
        }

        bool HasName(const std::vector<SpriteRegion>& sprites, const std::string& name)
        {
            return std::ranges::any_of(sprites, [&name](const SpriteRegion& region) { return region.Name == name; });
        }

        std::string MakeSpriteName(const std::vector<SpriteRegion>& sprites, const std::string& stem)
        {
            for (usize index = sprites.size();; ++index)
            {
                std::string candidate = std::format("{}_{}", stem, index);
                if (!HasName(sprites, candidate))
                    return candidate;
            }
        }

        SpriteRect RectBetween(const glm::ivec2 a, const glm::ivec2 b, const glm::ivec2 bounds)
        {
            const glm::ivec2 last = bounds - 1;
            const glm::ivec2 low = glm::clamp(glm::min(a, b), glm::ivec2(0), last);
            const glm::ivec2 high = glm::clamp(glm::max(a, b), glm::ivec2(0), last);

            return { static_cast<u32>(low.x), static_cast<u32>(low.y),
                     static_cast<u32>(high.x - low.x + 1), static_cast<u32>(high.y - low.y + 1) };
        }

        SpriteRect ClampRect(SpriteRect rect, const glm::uvec2 bounds)
        {
            rect.Width  = std::clamp<u32>(rect.Width, 1, bounds.x);
            rect.Height = std::clamp<u32>(rect.Height, 1, bounds.y);
            rect.X = std::min(rect.X, bounds.x - rect.Width);
            rect.Y = std::min(rect.Y, bounds.y - rect.Height);
            return rect;
        }

        bool IsTransparent(const DecodedImage& image, const SpriteRect& rect)
        {
            for (u32 y = rect.Y; y < rect.Y + rect.Height; ++y)
            {
                for (u32 x = rect.X; x < rect.X + rect.Width; ++x)
                {
                    const usize alpha = (static_cast<usize>(y) * image.Width + x) * 4 + 3;
                    if (std::to_integer<u8>(image.Pixels[alpha]) != 0)
                        return false;
                }
            }
            return true;
        }

        u32 HitEdges(const ImVec2& min, const ImVec2& max, const glm::vec2 mouse)
        {
            if (mouse.x < min.x - HandleRadius || mouse.x > max.x + HandleRadius ||
                mouse.y < min.y - HandleRadius || mouse.y > max.y + HandleRadius)
                return EdgeNone;

            u32 edges = EdgeNone;
            if (std::abs(mouse.x - min.x) <= HandleRadius)
                edges |= EdgeLeft;
            else if (std::abs(mouse.x - max.x) <= HandleRadius)
                edges |= EdgeRight;

            if (std::abs(mouse.y - min.y) <= HandleRadius)
                edges |= EdgeTop;
            else if (std::abs(mouse.y - max.y) <= HandleRadius)
                edges |= EdgeBottom;

            return edges;
        }

        void SetResizeCursor(const u32 edges)
        {
            const bool horizontal = (edges & (EdgeLeft | EdgeRight)) != 0;
            const bool vertical = (edges & (EdgeTop | EdgeBottom)) != 0;

            if (horizontal && vertical)
            {
                const bool mainDiagonal = ((edges & EdgeLeft) != 0) == ((edges & EdgeTop) != 0);
                ImGui::SetMouseCursor(mainDiagonal ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_ResizeNESW);
            } else if (horizontal)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            else if (vertical)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
    }

    SpriteEditorPanel::SpriteEditorPanel(EditorContext& context)
        : EditorPanel(context, Name) {}

    void SpriteEditorPanel::OpenTexture(const UUID texture, const std::optional<UUID> sprite)
    {
        m_Open = true;
        RequestFocus();

        if (!m_Texture || *m_Texture != texture)
        {
            if (m_Texture && m_Dirty)
            {
                OpenUnsavedChangesPopup(texture);
                return;
            }

            Load(texture);
        }

        if (sprite)
            m_Selected = *sprite;
    }

    void SpriteEditorPanel::Load(const UUID texture)
    {
        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(texture, metadata) || metadata.Type != AssetType::Texture2D)
        {
            APP_ERROR("Cannot open {} in the sprite editor: not a texture", static_cast<u64>(texture));
            return;
        }

        const auto* settings = metadata.GetSettings<TextureAssetSettings>();

        m_Texture = texture;
        m_TextureAsset = AssetManager::LoadTexture2D(texture);
        m_Name = ToUtf8(metadata.Path.stem());
        m_Working = settings != nullptr ? SheetState{ settings->Mode, settings->Sprites } : SheetState{};

        m_UndoStack.clear();
        m_RedoStack.clear();
        m_EditStart.reset();
        m_Dirty = false;

        m_Selected.reset();
        m_Drag = {};
        m_DragBefore.reset();
        m_FitRequested = true;
    }

    bool SpriteEditorPanel::Apply()
    {
        if (!m_Texture) return false;

        Validate();
        if (!m_ValidationError.empty())
        {
            APP_ERROR("Cannot apply the sprites of '{}': {}", m_Name, m_ValidationError);
            return false;
        }

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(*m_Texture, metadata)) return false;

        const auto* stored = metadata.GetSettings<TextureAssetSettings>();
        TextureAssetSettings settings = stored != nullptr ? *stored : TextureAssetSettings{};
        settings.Mode = m_Working.Mode;
        settings.Sprites = m_Working.Sprites;

        EditorContext::ApplyTextureSettings(*m_Texture, settings);

        m_Dirty = false;
        APP_INFO("Applied {} sprite(s) to '{}'", settings.Mode == SpriteMode::Multiple ? settings.Sprites.size() : 1, m_Name);
        return true;
    }

    void SpriteEditorPanel::Revert()
    {
        if (m_Texture)
            Load(*m_Texture);
    }

    void SpriteEditorPanel::Validate()
    {
        m_ValidationError.clear();
        if (m_Working.Mode != SpriteMode::Multiple) return;

        const glm::uvec2 size = GetTextureSize();
        std::unordered_set<std::string> names;

        for (const SpriteRegion& region : m_Working.Sprites)
        {
            if (region.Name.empty())
            {
                m_ValidationError = "Every sprite needs a name";
                return;
            }

            if (!names.insert(region.Name).second)
            {
                m_ValidationError = std::format("Two sprites are named '{}'", region.Name);
                return;
            }

            if (size.x > 0 && (region.Rect.X + region.Rect.Width > size.x || region.Rect.Y + region.Rect.Height > size.y))
            {
                m_ValidationError = std::format("'{}' reaches outside the texture", region.Name);
                return;
            }
        }
    }

    void SpriteEditorPanel::OnImGuiRender()
    {
        if (!m_Open) return;

        AssetMetadata metadata;
        if (m_Texture && !AssetRegistry::TryGetMetadata(*m_Texture, metadata))
        {
            m_Texture.reset();
            m_TextureAsset.reset();
            m_Dirty = false;
            m_Drag = {};
        } else if (m_Texture)
            m_Name = ToUtf8(metadata.Path.stem());

        const std::string title = m_Texture
                                ? std::format("{}{}{}", m_Name, m_Dirty ? " *" : "", WindowId)
                                : std::format("{}{}", Name, WindowId);

        if (ConsumeFocusRequest())
            ImGui::SetNextWindowFocus();

        ImGui::SetNextWindowSize(ImVec2(960.0f, 620.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title.c_str(), &m_Open))
        {
            if (!m_Texture)
                ImGui::TextDisabled("Double-click a texture in the Assets panel to edit its sprites");
            else
            {
                Validate();
                m_FrameStart = m_Working;

                HandleShortcuts();
                DrawToolbar();
                ImGui::Separator();

                const f32 canvasWidth = std::max(100.0f, ImGui::GetContentRegionAvail().x - PropertiesWidth
                                                        - ImGui::GetStyle().ItemSpacing.x);

                ImGui::BeginChild("##canvasRegion", ImVec2(canvasWidth, 0.0f), ImGuiChildFlags_None,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                DrawCanvas();
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("##properties");
                DrawProperties();
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    void SpriteEditorPanel::HandleShortcuts()
    {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
            Apply();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z))
            Undo();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
            Redo();

        if (ImGui::Shortcut(ImGuiKey_Delete))
            DeleteSelected();
        if (ImGui::Shortcut(ImGuiKey_Escape))
            m_Selected.reset();
        if (ImGui::Shortcut(ImGuiKey_F))
            m_FitRequested = true;
    }

    void SpriteEditorPanel::DrawToolbar()
    {
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::BeginCombo("Mode", m_Working.Mode == SpriteMode::Single ? "Single" : "Multiple"))
        {
            for (const SpriteMode mode : { SpriteMode::Single, SpriteMode::Multiple })
            {
                if (ImGui::Selectable(mode == SpriteMode::Single ? "Single" : "Multiple", m_Working.Mode == mode)
                    && m_Working.Mode != mode)
                {
                    BeginChange();
                    m_Working.Mode = mode;
                    OnDataChanged();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Single: the whole texture is one sprite. Multiple: a sheet of named sprites.");

        ImGui::SameLine();
        if (ImGui::Button("Slice..."))
            OpenSlicePopup();

        ImGui::SameLine();
        if (ImGui::Button("Fit"))
            m_FitRequested = true;
        ImGui::SetItemTooltip("Show the whole texture (F)");

        if (const glm::uvec2 size = GetTextureSize(); size.x > 0)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%u x %u px", size.x, size.y);
        }

        if (!m_ValidationError.empty())
        {
            ImGui::SameLine();
            EditorUI::ErrorText("%s", m_ValidationError.c_str());
        }

        const ImGuiStyle& style = ImGui::GetStyle();
        const f32 buttonsWidth = ImGui::CalcTextSize("Revert").x + ImGui::CalcTextSize("Apply").x
                               + style.FramePadding.x * 4.0f + style.ItemSpacing.x;

        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - buttonsWidth));

        ImGui::BeginDisabled(!m_Dirty);
        if (ImGui::Button("Revert"))
            Revert();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_ValidationError.empty());
        if (ImGui::Button("Apply"))
            Apply();
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Writes the sprites into the texture's meta file (Ctrl+S)");
    }

    void SpriteEditorPanel::DrawCanvas()
    {
        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 canvasSize{ std::max(available.x, 50.0f), std::max(available.y, 50.0f) };
        const ImVec2 canvasMax{ canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y };

        ImGui::InvisibleButton("##canvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft
                                                     | ImGuiButtonFlags_MouseButtonMiddle
                                                     | ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();

        ImDrawList& drawList = *ImGui::GetWindowDrawList();
        drawList.AddRectFilled(canvasMin, canvasMax, BackgroundColor);

        const glm::uvec2 size = GetTextureSize();
        if (size.x == 0 || size.y == 0)
        {
            const bool failed = m_TextureAsset && m_TextureAsset->GetState() == AssetLoadState::Failed;
            drawList.AddText(ImVec2(canvasMin.x + 8.0f, canvasMin.y + 8.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                             failed ? "The texture failed to load" : "Loading...");
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        const glm::vec2 origin{ canvasMin.x, canvasMin.y };
        const glm::vec2 mouse{ io.MousePos.x, io.MousePos.y };
        const glm::vec2 textureSize(size);

        if (std::exchange(m_FitRequested, false))
        {
            const f32 fit = std::min(canvasSize.x / textureSize.x, canvasSize.y / textureSize.y) * FitMargin;
            m_Zoom = std::clamp(fit, MinZoom, MaxZoom);
            m_Offset = (glm::vec2(canvasSize.x, canvasSize.y) - textureSize * m_Zoom) * 0.5f;
        }

        if (hovered && io.MouseWheel != 0.0f)
        {
            const glm::vec2 anchor = (mouse - origin - m_Offset) / m_Zoom;
            m_Zoom = std::clamp(m_Zoom * std::pow(ZoomStep, io.MouseWheel), MinZoom, MaxZoom);
            m_Offset = mouse - origin - anchor * m_Zoom;
        }

        if (active && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right)))
            m_Offset += glm::vec2(io.MouseDelta.x, io.MouseDelta.y);

        const auto toScreen = [&](const glm::vec2 pixel)
        {
            const glm::vec2 point = origin + m_Offset + pixel * m_Zoom;
            return ImVec2(point.x, point.y);
        };
        const auto rectMin = [&](const SpriteRect& rect)
        {
            return toScreen({ static_cast<f32>(rect.X), static_cast<f32>(rect.Y) });
        };
        const auto rectMax = [&](const SpriteRect& rect)
        {
            return toScreen({ static_cast<f32>(rect.X + rect.Width), static_cast<f32>(rect.Y + rect.Height) });
        };
        const glm::vec2 mousePixel = (mouse - origin - m_Offset) / m_Zoom;

        if (m_Working.Mode == SpriteMode::Multiple)
        {
            const SpriteRegion* selected = GetSelected();
            const u32 edges = selected != nullptr ? HitEdges(rectMin(selected->Rect), rectMax(selected->Rect), mouse)
                                                  : EdgeNone;

            if (hovered && m_Drag.Kind == DragKind::None)
                SetResizeCursor(edges);

            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_Drag = CanvasDrag{ .StartPixel = mousePixel };
                m_DragBefore = m_Working;

                if (edges != EdgeNone && selected != nullptr)
                {
                    m_Drag.Kind = DragKind::Resize;
                    m_Drag.Edges = edges;
                    m_Drag.StartRect = selected->Rect;
                } else if (const SpriteRegion* hit = HitRegion(mousePixel))
                {
                    m_Selected = hit->Id;
                    m_Drag.Kind = DragKind::Move;
                    m_Drag.StartRect = hit->Rect;
                } else
                {
                    m_Selected.reset();
                    m_Drag.Kind = DragKind::Create;
                }
            }

            if (m_Drag.Kind != DragKind::None && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    m_Drag.Moved = true;
                if (m_Drag.Moved)
                    UpdateDrag(mousePixel, size);
            }

            if (m_Drag.Kind != DragKind::None && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                FinishDrag();
        } else
            m_Drag = {};

        drawList.PushClipRect(canvasMin, canvasMax, true);

        const ImVec2 imageMin = toScreen(glm::vec2(0.0f));
        const ImVec2 imageMax = toScreen(textureSize);
        drawList.AddRectFilled(imageMin, imageMax, ImageBackgroundColor);
        UI::DrawImage(drawList, m_TextureAsset->GetTexture(), imageMin, imageMax);

        if (m_Zoom >= PixelGridZoom)
        {
            const f32 firstX = std::max(0.0f, std::floor((canvasMin.x - imageMin.x) / m_Zoom));
            const f32 lastX = std::min(textureSize.x, std::ceil((canvasMax.x - imageMin.x) / m_Zoom));
            const f32 firstY = std::max(0.0f, std::floor((canvasMin.y - imageMin.y) / m_Zoom));
            const f32 lastY = std::min(textureSize.y, std::ceil((canvasMax.y - imageMin.y) / m_Zoom));

            for (f32 x = firstX; x <= lastX; x += 1.0f)
            {
                const f32 screenX = imageMin.x + x * m_Zoom;
                drawList.AddLine(ImVec2(screenX, imageMin.y), ImVec2(screenX, imageMax.y), GridColor);
            }
            for (f32 y = firstY; y <= lastY; y += 1.0f)
            {
                const f32 screenY = imageMin.y + y * m_Zoom;
                drawList.AddLine(ImVec2(imageMin.x, screenY), ImVec2(imageMax.x, screenY), GridColor);
            }
        }

        drawList.AddRect(imageMin, imageMax, BorderColor);

        if (m_Working.Mode == SpriteMode::Multiple)
        {
            for (const SpriteRegion& region : m_Working.Sprites)
            {
                const bool isSelected = m_Selected == region.Id;
                const ImU32 color = isSelected ? SelectedColor : RegionColor;
                const ImVec2 min = rectMin(region.Rect);
                const ImVec2 max = rectMax(region.Rect);

                drawList.AddRect(min, max, color, 0.0f, 0, isSelected ? 2.0f : 1.0f);

                if (max.x - min.x >= MinLabelWidth && max.y - min.y >= ImGui::GetTextLineHeight())
                {
                    drawList.PushClipRect(min, max, true);
                    drawList.AddText(ImVec2(min.x + 3.0f, min.y + 1.0f), color, region.Name.c_str());
                    drawList.PopClipRect();
                }
            }

            if (const SpriteRegion* current = GetSelected())
            {
                const ImVec2 min = rectMin(current->Rect);
                const ImVec2 max = rectMax(current->Rect);
                const ImVec2 mid{ (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };

                for (const ImVec2& point : { min, ImVec2(mid.x, min.y), ImVec2(max.x, min.y), ImVec2(max.x, mid.y),
                                             max, ImVec2(mid.x, max.y), ImVec2(min.x, max.y), ImVec2(min.x, mid.y) })
                {
                    drawList.AddRectFilled(ImVec2(point.x - HandleSize, point.y - HandleSize),
                                           ImVec2(point.x + HandleSize, point.y + HandleSize), SelectedColor);
                }

                const SpriteRect& rect = current->Rect;
                const glm::vec2 pivotPixel{
                    static_cast<f32>(rect.X) + current->Pivot.x * static_cast<f32>(rect.Width),
                    static_cast<f32>(rect.Y) + (1.0f - current->Pivot.y) * static_cast<f32>(rect.Height)
                };
                drawList.AddCircle(toScreen(pivotPixel), 5.0f, SelectedColor, 0, 2.0f);
            }

            if (m_Drag.Kind == DragKind::Create && m_Drag.Moved)
                drawList.AddRect(rectMin(m_Drag.Preview), rectMax(m_Drag.Preview), CreateColor, 0.0f, 0, 2.0f);
        }

        drawList.PopClipRect();
    }

    void SpriteEditorPanel::UpdateDrag(const glm::vec2 mousePixel, const glm::uvec2 size)
    {
        const glm::ivec2 bounds(size);

        if (m_Drag.Kind == DragKind::Create)
        {
            m_Drag.Preview = RectBetween(glm::ivec2(glm::floor(m_Drag.StartPixel)), glm::ivec2(glm::floor(mousePixel)),
                                         bounds);
            return;
        }

        SpriteRegion* region = GetSelected();
        if (region == nullptr) return;

        const SpriteRect& start = m_Drag.StartRect;
        i32 left = static_cast<i32>(start.X);
        i32 top = static_cast<i32>(start.Y);
        i32 right = left + static_cast<i32>(start.Width);
        i32 bottom = top + static_cast<i32>(start.Height);

        if (m_Drag.Kind == DragKind::Move)
        {
            const glm::ivec2 delta(glm::round(mousePixel - m_Drag.StartPixel));
            const i32 width = right - left;
            const i32 height = bottom - top;

            left = std::clamp(left + delta.x, 0, bounds.x - width);
            top = std::clamp(top + delta.y, 0, bounds.y - height);
            right = left + width;
            bottom = top + height;
        } else
        {
            const glm::ivec2 edge(glm::round(mousePixel));
            if ((m_Drag.Edges & EdgeLeft) != 0)
                left = std::clamp(edge.x, 0, right - 1);
            if ((m_Drag.Edges & EdgeRight) != 0)
                right = std::clamp(edge.x, left + 1, bounds.x);
            if ((m_Drag.Edges & EdgeTop) != 0)
                top = std::clamp(edge.y, 0, bottom - 1);
            if ((m_Drag.Edges & EdgeBottom) != 0)
                bottom = std::clamp(edge.y, top + 1, bounds.y);
        }

        const SpriteRect rect{ static_cast<u32>(left), static_cast<u32>(top),
                               static_cast<u32>(right - left), static_cast<u32>(bottom - top) };
        if (rect == region->Rect) return;

        region->Rect = rect;
        OnDataChanged();
    }

    void SpriteEditorPanel::FinishDrag()
    {
        if (m_Drag.Kind == DragKind::Create && m_Drag.Moved)
        {
            SpriteRegion region;
            region.Name = MakeSpriteName(m_Working.Sprites, m_Name);
            region.Rect = m_Drag.Preview;
            m_Selected = region.Id;

            if (m_DragBefore)
                PushUndo(std::move(*m_DragBefore));

            m_Working.Sprites.push_back(std::move(region));
            OnDataChanged();
        } else if (m_DragBefore && *m_DragBefore != m_Working)
            PushUndo(std::move(*m_DragBefore));

        m_Drag = {};
        m_DragBefore.reset();
    }

    void SpriteEditorPanel::DrawProperties()
    {
        if (m_Working.Mode == SpriteMode::Single)
        {
            ImGui::TextWrapped("The whole texture is one sprite. Its pivot and pixels per unit are in the Inspector.");
            ImGui::Spacing();
            ImGui::TextWrapped("Switch the mode to Multiple, or use Slice, to cut it into several sprites.");
            return;
        }

        ImGui::SeparatorText(std::format("Sprites ({})", m_Working.Sprites.size()).c_str());

        if (ImGui::BeginChild("##list", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8.0f),
                              ImGuiChildFlags_Borders))
        {
            for (const SpriteRegion& region : m_Working.Sprites)
            {
                EditorUI::PushID(static_cast<u64>(region.Id));
                if (ImGui::Selectable(region.Name.empty() ? "<unnamed>" : region.Name.c_str(), m_Selected == region.Id))
                    m_Selected = region.Id;
                ImGui::PopID();
            }

            if (m_Working.Sprites.empty())
                ImGui::TextDisabled("Drag on the texture or use Slice");
        }
        ImGui::EndChild();

        SpriteRegion* selected = GetSelected();
        if (selected == nullptr)
        {
            ImGui::TextDisabled("Select a sprite to edit it");
            return;
        }

        ImGui::SeparatorText("Sprite");
        ImGui::PushItemWidth(-90.0f);

        if (ImGui::InputText("Name", &selected->Name))
            OnDataChanged();
        TrackItemEdit();
        ImGui::SetItemTooltip("Re-slicing keeps the id of sprites whose name stays the same");

        const glm::uvec2 size = GetTextureSize();

        i32 position[2]{ static_cast<i32>(selected->Rect.X), static_cast<i32>(selected->Rect.Y) };
        if (ImGui::DragInt2("Position", position, 0.25f, 0, 16384))
        {
            selected->Rect.X = static_cast<u32>(std::max(0, position[0]));
            selected->Rect.Y = static_cast<u32>(std::max(0, position[1]));
            if (size.x > 0)
                selected->Rect = ClampRect(selected->Rect, size);
            OnDataChanged();
        }
        TrackItemEdit();
        ImGui::SetItemTooltip("Top left corner in texture pixels");

        i32 extent[2]{ static_cast<i32>(selected->Rect.Width), static_cast<i32>(selected->Rect.Height) };
        if (ImGui::DragInt2("Size", extent, 0.25f, 1, 16384))
        {
            selected->Rect.Width = static_cast<u32>(std::max(1, extent[0]));
            selected->Rect.Height = static_cast<u32>(std::max(1, extent[1]));
            if (size.x > 0)
                selected->Rect = ClampRect(selected->Rect, size);
            OnDataChanged();
        }
        TrackItemEdit();

        if (PivotField("Pivot", selected->Pivot))
        {
            PushUndo(m_FrameStart);
            OnDataChanged();
        }

        if (ImGui::DragFloat2("##pivotValue", &selected->Pivot.x, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
            OnDataChanged();
        TrackItemEdit();
        ImGui::SetItemTooltip("Point that sits on the entity position, (0, 0) is the bottom left");

        ImGui::PopItemWidth();

        ImGui::Spacing();
        if (ImGui::Button("Delete Sprite"))
            DeleteSelected();
    }

    void SpriteEditorPanel::OpenSlicePopup()
    {
        if (!m_Texture) return;

        GetContext().Popups.Open("Slice Sprite Sheet", [this, texture = *m_Texture](Popup& popup)
        {
            if (!m_Texture || *m_Texture != texture)
            {
                popup.Close();
                return;
            }

            SliceSettings& settings = m_Slice;
            ImGui::PushItemWidth(200.0f);

            i32 type = settings.ByCount ? 1 : 0;
            if (ImGui::Combo("Type", &type, "Grid by Cell Size\0Grid by Cell Count\0"))
                settings.ByCount = type == 1;

            if (settings.ByCount)
                ImGui::InputInt2("Columns, Rows", &settings.CellCount.x);
            else
                ImGui::InputInt2("Cell Size", &settings.CellSize.x);

            ImGui::InputInt2("Offset", &settings.Offset.x);
            ImGui::SetItemTooltip("Pixels to skip at the left and top edge");

            ImGui::InputInt2("Padding", &settings.Padding.x);
            ImGui::SetItemTooltip("Pixels between two cells");

            PivotField("Pivot", settings.Pivot);

            ImGui::Checkbox("Skip Empty Cells", &settings.SkipEmpty);
            ImGui::SetItemTooltip("Leaves out cells whose pixels are all transparent");

            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::TextDisabled("Replaces all sprites. Sprites that keep their name keep their id,\n"
                                "so scenes and clips using them stay intact.");
            ImGui::Spacing();

            if (ImGui::Button("Slice"))
            {
                popup.Close();
                Slice(settings);
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                popup.Close();
        });
    }

    void SpriteEditorPanel::Slice(const SliceSettings& settings)
    {
        const glm::uvec2 size = GetTextureSize();
        if (size.x == 0 || size.y == 0)
        {
            APP_WARN("Cannot slice '{}' before the texture has loaded", m_Name);
            return;
        }

        const glm::ivec2 bounds(size);
        const glm::ivec2 offset = glm::max(settings.Offset, glm::ivec2(0));
        const glm::ivec2 padding = glm::max(settings.Padding, glm::ivec2(0));

        glm::ivec2 cell = settings.CellSize;
        if (settings.ByCount)
        {
            const glm::ivec2 count = glm::max(settings.CellCount, glm::ivec2(1));
            cell = (bounds - offset - padding * (count - 1)) / count;
        }

        if (cell.x < 1 || cell.y < 1)
        {
            APP_WARN("Cannot slice '{}': the cells would be smaller than a pixel", m_Name);
            return;
        }

        std::optional<DecodedImage> pixels;
        if (settings.SkipEmpty)
        {
            if (const Result<std::vector<std::byte>> encoded = AssetRegistry::Read(*m_Texture))
            {
                if (Result<DecodedImage> image = DecodeImage(*encoded, m_Name);
                    image && image->Width == size.x && image->Height == size.y)
                    pixels = std::move(*image);
            }

            if (!pixels)
                APP_WARN("Cannot read the pixels of '{}', empty cells are kept", m_Name);
        }

        std::unordered_map<std::string, UUID> existing;
        for (const SpriteRegion& region : m_Working.Sprites)
            existing.emplace(region.Name, region.Id);

        std::vector<SpriteRegion> sprites;
        for (i32 y = offset.y; y + cell.y <= bounds.y; y += cell.y + padding.y)
        {
            for (i32 x = offset.x; x + cell.x <= bounds.x; x += cell.x + padding.x)
            {
                const SpriteRect rect{ static_cast<u32>(x), static_cast<u32>(y),
                                       static_cast<u32>(cell.x), static_cast<u32>(cell.y) };
                if (pixels && IsTransparent(*pixels, rect)) continue;

                SpriteRegion region;
                region.Name = std::format("{}_{}", m_Name, sprites.size());
                if (const auto it = existing.find(region.Name); it != existing.end())
                    region.Id = it->second;
                region.Rect = rect;
                region.Pivot = settings.Pivot;
                sprites.push_back(std::move(region));
            }
        }

        BeginChange();
        m_Working.Mode = SpriteMode::Multiple;
        m_Working.Sprites = std::move(sprites);
        m_Selected.reset();
        OnDataChanged();

        APP_INFO("Sliced '{}' into {} sprite(s)", m_Name, m_Working.Sprites.size());
    }

    void SpriteEditorPanel::OpenUnsavedChangesPopup(const UUID next)
    {
        GetContext().Popups.Open("Unsaved Changes", [this, next](Popup& popup)
        {
            if (!m_Texture)
            {
                popup.Close();
                Load(next);
                return;
            }

            ImGui::Text("The sprites of '%s' have unapplied changes.", m_Name.c_str());
            ImGui::Spacing();

            std::optional<bool> apply;
            ImGui::BeginDisabled(!m_ValidationError.empty());
            if (ImGui::Button("Apply"))
                apply = true;
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Discard"))
                apply = false;
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                popup.Close();

            if (apply)
            {
                popup.Close();
                if (!*apply || Apply())
                    Load(next);
            }
        });
    }

    void SpriteEditorPanel::DeleteSelected()
    {
        if (m_Working.Mode != SpriteMode::Multiple || GetSelected() == nullptr) return;

        BeginChange();
        std::erase_if(m_Working.Sprites, [this](const SpriteRegion& region) { return region.Id == *m_Selected; });
        m_Selected.reset();
        OnDataChanged();
    }

    SpriteRegion* SpriteEditorPanel::GetSelected()
    {
        if (!m_Selected) return nullptr;

        const auto it = std::ranges::find(m_Working.Sprites, *m_Selected, &SpriteRegion::Id);
        return it != m_Working.Sprites.end() ? &*it : nullptr;
    }

    const SpriteRegion* SpriteEditorPanel::HitRegion(const glm::vec2 pixel) const
    {
        for (auto it = m_Working.Sprites.rbegin(); it != m_Working.Sprites.rend(); ++it)
        {
            const SpriteRect& rect = it->Rect;
            if (pixel.x >= static_cast<f32>(rect.X) && pixel.x < static_cast<f32>(rect.X + rect.Width) &&
                pixel.y >= static_cast<f32>(rect.Y) && pixel.y < static_cast<f32>(rect.Y + rect.Height))
                return &*it;
        }
        return nullptr;
    }

    glm::uvec2 SpriteEditorPanel::GetTextureSize() const
    {
        if (!m_TextureAsset || m_TextureAsset->GetState() != AssetLoadState::Loaded)
            return glm::uvec2(0);

        const Ref<Texture2D>& texture = m_TextureAsset->GetTexture();
        return { texture->GetWidth(), texture->GetHeight() };
    }

    void SpriteEditorPanel::BeginChange() { PushUndo(m_Working); }

    void SpriteEditorPanel::TrackItemEdit()
    {
        if (ImGui::IsItemActivated())
            m_EditStart = m_FrameStart;

        if (ImGui::IsItemDeactivatedAfterEdit() && m_EditStart)
            PushUndo(std::move(*m_EditStart));

        if (ImGui::IsItemDeactivated())
            m_EditStart.reset();
    }

    void SpriteEditorPanel::PushUndo(SheetState snapshot)
    {
        m_UndoStack.push_back(std::move(snapshot));
        if (m_UndoStack.size() > MaxUndoSteps)
            m_UndoStack.erase(m_UndoStack.begin());

        m_RedoStack.clear();
    }

    void SpriteEditorPanel::Undo()
    {
        if (m_UndoStack.empty()) return;

        m_RedoStack.push_back(std::move(m_Working));
        m_Working = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();

        m_Drag = {};
        m_DragBefore.reset();
        OnDataChanged();
    }

    void SpriteEditorPanel::Redo()
    {
        if (m_RedoStack.empty()) return;

        m_UndoStack.push_back(std::move(m_Working));
        m_Working = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();

        m_Drag = {};
        m_DragBefore.reset();
        OnDataChanged();
    }

    void SpriteEditorPanel::OnDataChanged() { m_Dirty = true; }
}
