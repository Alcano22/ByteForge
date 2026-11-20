#include "Editor/Panels/InputActionsPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/StringUtils.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/InputControlSource.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr usize MaxUndoSteps = 100;
        constexpr f32 PartLabelWidth = 80.0f;

        constexpr std::array ActionTypes{
            InputActionType::Button,
            InputActionType::Axis1D,
            InputActionType::Vector2
        };

        constexpr std::array BindingKinds{
            InputBindingKind::Single,
            InputBindingKind::Composite1D,
            InputBindingKind::Composite2D
        };

        constexpr std::array<const char*, 1> SingleParts{ "Control" };
        constexpr std::array<const char*, 2> Composite1DParts{ "Negative", "Positive" };
        constexpr std::array<const char*, 4> Composite2DParts{ "Up", "Down", "Left", "Right" };

        std::span<const char* const> GetPartNames(const InputBindingKind kind)
        {
            switch (kind)
            {
                case InputBindingKind::Single:      return SingleParts;
                case InputBindingKind::Composite1D: return Composite1DParts;
                case InputBindingKind::Composite2D: return Composite2DParts;
            }
            return {};
        }

        const char* DescribeType(const InputActionType type)
        {
            switch (type)
            {
                case InputActionType::Button:  return "Pressed or released, down from half way. Read with IsDown and WasPressedThisFrame.";
                case InputActionType::Axis1D:  return "One value, e.g. a trigger or A/D. Read with ReadFloat.";
                case InputActionType::Vector2: return "Two values, e.g. a stick or WASD. Read with ReadVector2.";
            }
            return "";
        }

        std::string DescribeControl(const InputControl& control)
        {
            return control.IsSet() ? InputControlToString(control) : std::string("<unbound>");
        }

        std::string DescribeBinding(const InputBindingData& binding)
        {
            switch (binding.Kind)
            {
                case InputBindingKind::Single:
                    return binding.Controls.empty() ? std::string("<unbound>") : DescribeControl(binding.Controls[0]);
                case InputBindingKind::Composite1D:
                    return "1D Composite";
                case InputBindingKind::Composite2D:
                    return "2D Composite";
            }
            return {};
        }

        const char* DisplayName(const std::string& name) { return name.empty() ? "<unnamed>" : name.c_str(); }

        template<typename T>
        std::string MakeUniqueName(const std::vector<T>& items, const std::string& base)
        {
            const auto taken = [&items](const std::string& name)
            {
                return std::ranges::any_of(items, [&name](const T& item) { return item.Name == name; });
            };

            if (!taken(base))
                return base;

            for (u32 suffix = 1;; ++suffix)
            {
                std::string candidate = std::format("{} {}", base, suffix);
                if (!taken(candidate))
                    return candidate;
            }
        }

        bool HeaderWithAddButton(const char* title, const char* buttonId, const bool enabled)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(title);

            const f32 size = ImGui::GetFrameHeight();
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - size));

            ImGui::BeginDisabled(!enabled);
            const bool pressed = ImGui::Button(buttonId, ImVec2(size, size));
            ImGui::EndDisabled();

            ImGui::Separator();
            return pressed;
        }

        isize Offset(const usize index) { return static_cast<isize>(index); }
    }

    InputActionsPanel::InputActionsPanel(EditorContext& context)
        : EditorPanel(context, Name) {}

    void InputActionsPanel::OpenAsset(const UUID asset)
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

    void InputActionsPanel::Load(const UUID asset)
    {
        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(asset, metadata) || metadata.Type != AssetType::InputActions)
        {
            APP_ERROR("Cannot open {}: not an input actions asset", static_cast<u64>(asset));
            return;
        }

        m_Asset = asset;
        m_Name = ToUtf8(metadata.Path.stem());
        m_Working = AssetManager::LoadInputActions(asset)->GetData();

        m_UndoStack.clear();
        m_RedoStack.clear();
        m_EditStart.reset();
        m_Dirty = false;

        m_SelectedMap = m_Working.Maps.empty() ? std::nullopt : std::optional<usize>(0);
        m_SelectedAction.reset();
        m_Listening.reset();

        Validate();
    }

    bool InputActionsPanel::Save()
    {
        if (!m_Asset) return false;

        if (const Result<> valid = ValidateInputActions(m_Working); !valid)
        {
            APP_ERROR("Cannot save '{}': {}", m_Name, valid.error());
            return false;
        }

        AssetManager::SetInputActions(*m_Asset, m_Working);
        if (!AssetManager::SaveInputActions(*m_Asset))
            return false;

        m_Dirty = false;
        APP_INFO("Saved input actions '{}'", m_Name);
        return true;
    }

    void InputActionsPanel::Validate()
    {
        const Result<> valid = ValidateInputActions(m_Working);
        m_ValidationError = valid ? std::string() : valid.error();
    }

    void InputActionsPanel::OnImGuiRender()
    {
        if (!m_Open) return;

        AssetMetadata metadata;
        if (m_Asset && !AssetRegistry::TryGetMetadata(*m_Asset, metadata))
        {
            m_Asset.reset();
            m_Dirty = false;
            m_Listening.reset();
        } else if (m_Asset)
            m_Name = ToUtf8(metadata.Path.stem());

        const std::string title = m_Asset
                                ? std::format("{}{}{}", m_Name, m_Dirty ? " *" : "", WindowId)
                                : std::format("{}{}", Name, WindowId);

        if (ConsumeFocusRequest())
            ImGui::SetNextWindowFocus();

        ImGui::SetNextWindowSize(ImVec2(900.0f, 480.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title.c_str(), &m_Open))
        {
            if (!m_Asset)
                ImGui::TextDisabled("Double-click an Input Actions asset in the Assets panel to edit it");
            else
            {
                m_FrameStart = m_Working;

                UpdateListening();
                HandleShortcuts();
                DrawToolbar();
                ImGui::Separator();

                constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
                                                       ImGuiTableFlags_SizingStretchProp;

                if (ImGui::BeginTable("##columns", 3, tableFlags, ImGui::GetContentRegionAvail()))
                {
                    ImGui::TableSetupColumn("Action Maps", ImGuiTableColumnFlags_WidthStretch, 0.8f);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch, 1.2f);
                    ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch, 1.6f);
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::BeginChild("##maps");
                    DrawMaps();
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    ImGui::BeginChild("##actions");
                    DrawActions();
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    ImGui::BeginChild("##properties");
                    DrawProperties();
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
            }
        }

        ImGui::End();
    }

    void InputActionsPanel::HandleShortcuts()
    {
        if (m_Listening) return;

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
            Save();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z))
            Undo();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) ||
            ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
            Redo();
    }

    void InputActionsPanel::UpdateListening()
    {
        if (!m_Listening) return;

        const ListenTarget target = *m_Listening;
        if (target.Map >= m_Working.Maps.size() ||
            target.Action >= m_Working.Maps[target.Map].Actions.size() ||
            target.Binding >= m_Working.Maps[target.Map].Actions[target.Action].Bindings.size() ||
            target.Control >= m_Working.Maps[target.Map].Actions[target.Action].Bindings[target.Binding].Controls.size())
        {
            m_Listening.reset();
            return;
        }

        if (Input::IsKeyPressed(KeyCode::Escape))
        {
            m_Listening.reset();
            return;
        }

        const InputActionData& action = m_Working.Maps[target.Map].Actions[target.Action];
        const bool preferSticks = action.Type == InputActionType::Vector2 &&
                                  action.Bindings[target.Binding].Kind == InputBindingKind::Single;

        const std::optional<InputControl> pressed = FindPressedControl(preferSticks);
        if (!pressed) return;

        BeginChange();
        m_Working.Maps[target.Map].Actions[target.Action].Bindings[target.Binding].Controls[target.Control] = *pressed;
        OnDataChanged();

        m_Listening.reset();
    }

    void InputActionsPanel::DrawToolbar()
    {
        ImGui::BeginDisabled(!m_Dirty || !m_ValidationError.empty());
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

        if (!m_ValidationError.empty())
        {
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::AlignTextToFramePadding();
            EditorUI::ErrorText("%s", m_ValidationError.c_str());
        }
    }

    void InputActionsPanel::DrawMaps()
    {
        if (HeaderWithAddButton("Action Maps", "+##map", true))
            AddMap();

        std::optional<usize> removed;

        for (usize i = 0; i < m_Working.Maps.size(); ++i)
        {
            ImGui::PushID(static_cast<i32>(i));

            const std::string label = std::format("{}##map", DisplayName(m_Working.Maps[i].Name));
            if (ImGui::Selectable(label.c_str(), m_SelectedMap == i))
            {
                m_SelectedMap = i;
                m_SelectedAction.reset();
            }

            if (ImGui::BeginPopupContextItem("##mapMenu"))
            {
                if (ImGui::MenuItem("Delete"))
                    removed = i;
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        if (removed)
            DeleteMap(*removed);
    }

    void InputActionsPanel::DrawActions()
    {
        InputActionMapData* map = GetSelectedMap();

        if (HeaderWithAddButton("Actions", "+##action", map != nullptr))
            AddAction();

        if (map == nullptr)
        {
            ImGui::TextDisabled("Select an action map");
            return;
        }

        std::optional<usize> removedAction;
        std::optional<std::pair<usize, usize>> removedBinding;
        std::optional<std::pair<usize, InputBindingKind>> addedBinding;

        for (usize a = 0; a < map->Actions.size(); ++a)
        {
            const InputActionData& action = map->Actions[a];
            ImGui::PushID(static_cast<i32>(a));

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                                       ImGuiTreeNodeFlags_DefaultOpen;
            if (action.Bindings.empty())
                flags |= ImGuiTreeNodeFlags_Leaf;
            if (IsSelected(a, std::nullopt))
                flags |= ImGuiTreeNodeFlags_Selected;

            const bool open = ImGui::TreeNodeEx("##action", flags, "%s", DisplayName(action.Name));
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                m_SelectedAction = ActionSelection{ .Action = a };

            if (ImGui::BeginPopupContextItem("##actionMenu"))
            {
                if (ImGui::MenuItem("Add Binding"))
                    addedBinding = std::pair{ a, InputBindingKind::Single };
                if (ImGui::MenuItem("Add 1D Composite"))
                    addedBinding = std::pair{ a, InputBindingKind::Composite1D };
                if (ImGui::MenuItem("Add 2D Composite"))
                    addedBinding = std::pair{ a, InputBindingKind::Composite2D };

                ImGui::Separator();
                if (ImGui::MenuItem("Delete"))
                    removedAction = a;
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("%s", std::string(magic_enum::enum_name(action.Type)).c_str());

            if (open)
            {
                for (usize b = 0; b < action.Bindings.size(); ++b)
                {
                    const InputBindingData& binding = action.Bindings[b];
                    const bool composite = binding.Kind != InputBindingKind::Single;
                    ImGui::PushID(static_cast<i32>(b));

                    ImGuiTreeNodeFlags bindingFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
                    bindingFlags |= composite ? ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen
                                              : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (IsSelected(a, b))
                        bindingFlags |= ImGuiTreeNodeFlags_Selected;

                    const bool bindingOpen = ImGui::TreeNodeEx("##binding", bindingFlags, "%s",
                                                               DescribeBinding(binding).c_str());
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                        m_SelectedAction = ActionSelection{ .Action = a, .Binding = b };

                    if (ImGui::BeginPopupContextItem("##bindingMenu"))
                    {
                        if (ImGui::MenuItem("Delete"))
                            removedBinding = std::pair{ a, b };
                        ImGui::EndPopup();
                    }

                    if (composite && bindingOpen)
                    {
                        const std::span<const char* const> parts = GetPartNames(binding.Kind);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

                        for (usize p = 0; p < binding.Controls.size() && p < parts.size(); ++p)
                        {
                            ImGui::PushID(static_cast<i32>(p));
                            ImGui::TreeNodeEx("##part", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                        ImGuiTreeNodeFlags_SpanAvailWidth,
                                              "%s: %s", parts[p], DescribeControl(binding.Controls[p]).c_str());
                            if (ImGui::IsItemClicked())
                                m_SelectedAction = ActionSelection{ .Action = a, .Binding = b };
                            ImGui::PopID();
                        }

                        ImGui::PopStyleColor();
                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (addedBinding)
            AddBinding(addedBinding->first, addedBinding->second);
        if (removedBinding)
            DeleteBinding(removedBinding->first, removedBinding->second);
        if (removedAction)
            DeleteAction(*removedAction);
    }

    void InputActionsPanel::DrawProperties()
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Properties");
        ImGui::Separator();

        InputActionMapData* map = GetSelectedMap();
        if (map == nullptr)
        {
            ImGui::TextDisabled("Select an action map, an action or a binding");
            return;
        }

        if (!m_SelectedAction)
        {
            DrawMapProperties(*map);
            return;
        }

        InputActionData& action = map->Actions[m_SelectedAction->Action];
        if (m_SelectedAction->Binding)
            DrawBindingProperties(action, *m_SelectedAction->Binding);
        else
            DrawActionProperties(action, m_SelectedAction->Action);
    }

    void InputActionsPanel::DrawMapProperties(InputActionMapData& map)
    {
        if (ImGui::InputText("Name", &map.Name))
            OnDataChanged();
        TrackItemEdit();

        ImGui::TextDisabled("%zu actions. Scripts can switch the whole map with InputActions.EnableMap/DisableMap.",
                            map.Actions.size());
    }

    void InputActionsPanel::DrawActionProperties(InputActionData& action, const usize actionIndex)
    {
        if (ImGui::InputText("Name", &action.Name))
            OnDataChanged();
        TrackItemEdit();

        InputActionType type = action.Type;
        if (EditorUI::EnumCombo("Type", type, std::span<const InputActionType>(ActionTypes)))
        {
            BeginChange();
            action.Type = type;
            OnDataChanged();
        }

        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", DescribeType(action.Type));
        ImGui::PopTextWrapPos();

        ImGui::SeparatorText("Bindings");
        ImGui::TextDisabled("%zu bindings, the strongest one drives the action", action.Bindings.size());

        if (ImGui::Button("Add Binding"))
            AddBinding(actionIndex, InputBindingKind::Single);
        else if (ImGui::SameLine(); ImGui::Button("Add 1D Composite"))
            AddBinding(actionIndex, InputBindingKind::Composite1D);
        else if (ImGui::SameLine(); ImGui::Button("Add 2D Composite"))
            AddBinding(actionIndex, InputBindingKind::Composite2D);
    }

    void InputActionsPanel::DrawBindingProperties(InputActionData& action, const usize bindingIndex)
    {
        InputBindingData& binding = action.Bindings[bindingIndex];

        ImGui::TextDisabled("Binding of '%s'", DisplayName(action.Name));

        InputBindingKind kind = binding.Kind;
        if (EditorUI::EnumCombo("Kind", kind, std::span<const InputBindingKind>(BindingKinds)))
        {
            BeginChange();
            binding.Kind = kind;
            binding.Controls.resize(GetControlCount(kind));
            OnDataChanged();
        }

        ImGui::SeparatorText("Controls");

        const std::span<const char* const> parts = GetPartNames(binding.Kind);
        for (usize i = 0; i < binding.Controls.size() && i < parts.size(); ++i)
        {
            DrawControlRow(parts[i], binding.Controls[i], ListenTarget{
                .Map     = *m_SelectedMap,
                .Action  = m_SelectedAction->Action,
                .Binding = bindingIndex,
                .Control = i
            });
        }

        ImGui::SeparatorText("Processors");

        InputProcessors& processors = binding.Processors;

        if (ImGui::DragFloat("Deadzone", &processors.Deadzone, 0.005f, 0.0f, 0.95f, "%.3f",
                             ImGuiSliderFlags_AlwaysClamp))
            OnDataChanged();
        TrackItemEdit();
        ImGui::SetItemTooltip("Values below this magnitude read as zero, the rest is remapped to start at zero");

        if (ImGui::DragFloat("Scale", &processors.Scale, 0.01f, -100.0f, 100.0f, "%.2f"))
            OnDataChanged();
        TrackItemEdit();

        EditCheckbox("Invert X", processors.InvertX);
        ImGui::SameLine();
        EditCheckbox("Invert Y", processors.InvertY);
        ImGui::SameLine();
        EditCheckbox("Normalize", processors.Normalize);
        ImGui::SetItemTooltip("Clamps the value to length 1, so diagonal WASD isn't faster than straight");
    }

    void InputActionsPanel::DrawControlRow(const char* label, InputControl& control, const ListenTarget& target)
    {
        ImGui::PushID(static_cast<i32>(target.Control));

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(PartLabelWidth);

        if (m_Listening && *m_Listening == target)
        {
            if (ImGui::Button("Press a key or move a gamepad input, Esc cancels", ImVec2(-FLT_MIN, 0.0f)))
                m_Listening.reset();

            ImGui::PopID();
            return;
        }

        const ImGuiStyle& style = ImGui::GetStyle();
        const f32 listenWidth = EditorUI::ButtonWidth("Listen");
        const f32 clearWidth = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - listenWidth - clearWidth -
                                                style.ItemSpacing.x * 2.0f));

        if (ImGui::BeginCombo("##control", DescribeControl(control).c_str(), ImGuiComboFlags_HeightLarge))
        {
            if (ImGui::IsWindowAppearing())
                m_ControlSearch.clear();

            EditorUI::PopupSearchField("Search", m_ControlSearch);

            for (const InputControl& option : GetKnownInputControls())
            {
                const std::string name = InputControlToString(option);
                if (!m_ControlSearch.empty() && !ContainsIgnoreCase(name, m_ControlSearch)) continue;

                const bool selected = option == control;
                if (ImGui::Selectable(name.c_str(), selected) && !selected)
                {
                    BeginChange();
                    control = option;
                    OnDataChanged();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Listen"))
            m_Listening = target;
        ImGui::SetItemTooltip("Press the key or move the gamepad input to bind, mouse buttons are in the list");

        ImGui::SameLine();
        ImGui::BeginDisabled(!control.IsSet());
        if (ImGui::Button("x", ImVec2(clearWidth, 0.0f)))
        {
            BeginChange();
            control = InputControl{};
            OnDataChanged();
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Unbind");

        ImGui::PopID();
    }

    void InputActionsPanel::EditCheckbox(const char* label, bool& value)
    {
        bool edited = value;
        if (!ImGui::Checkbox(label, &edited)) return;

        BeginChange();
        value = edited;
        OnDataChanged();
    }

    void InputActionsPanel::OpenUnsavedChangesPopup(const UUID next)
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
            ImGui::BeginDisabled(!m_ValidationError.empty());
            if (ImGui::Button("Save"))
                save = true;
            ImGui::EndDisabled();
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

    void InputActionsPanel::AddMap()
    {
        BeginChange();

        m_Working.Maps.push_back(InputActionMapData{
            .Id   = UUID(),
            .Name = MakeUniqueName(m_Working.Maps, "New Map")
        });

        m_SelectedMap = m_Working.Maps.size() - 1;
        m_SelectedAction.reset();
        OnDataChanged();
    }

    void InputActionsPanel::DeleteMap(const usize index)
    {
        if (index >= m_Working.Maps.size()) return;

        BeginChange();
        m_Working.Maps.erase(m_Working.Maps.begin() + Offset(index));

        if (m_SelectedMap && *m_SelectedMap > index)
            --*m_SelectedMap;
        m_SelectedAction.reset();

        ClampSelection();
        OnDataChanged();
    }

    void InputActionsPanel::AddAction()
    {
        if (GetSelectedMap() == nullptr) return;

        BeginChange();

        InputActionMapData& map = *GetSelectedMap();
        map.Actions.push_back(InputActionData{
            .Id   = UUID(),
            .Name = MakeUniqueName(map.Actions, "New Action"),
            .Type = InputActionType::Button
        });

        m_SelectedAction = ActionSelection{ .Action = map.Actions.size() - 1 };
        OnDataChanged();
    }

    void InputActionsPanel::DeleteAction(const usize index)
    {
        InputActionMapData* map = GetSelectedMap();
        if (map == nullptr || index >= map->Actions.size()) return;

        BeginChange();
        map = GetSelectedMap();
        map->Actions.erase(map->Actions.begin() + Offset(index));

        if (m_SelectedAction && m_SelectedAction->Action == index)
            m_SelectedAction.reset();
        else if (m_SelectedAction && m_SelectedAction->Action > index)
            --m_SelectedAction->Action;

        ClampSelection();
        OnDataChanged();
    }

    void InputActionsPanel::AddBinding(const usize action, const InputBindingKind kind)
    {
        InputActionMapData* map = GetSelectedMap();
        if (map == nullptr || action >= map->Actions.size()) return;

        BeginChange();
        map = GetSelectedMap();

        std::vector<InputBindingData>& bindings = map->Actions[action].Bindings;
        bindings.push_back(InputBindingData{
            .Kind     = kind,
            .Controls = std::vector<InputControl>(GetControlCount(kind))
        });

        m_SelectedAction = ActionSelection{ .Action = action, .Binding = bindings.size() - 1 };
        OnDataChanged();
    }

    void InputActionsPanel::DeleteBinding(const usize action, const usize binding)
    {
        InputActionMapData* map = GetSelectedMap();
        if (map == nullptr || action >= map->Actions.size() || binding >= map->Actions[action].Bindings.size()) return;

        BeginChange();
        map = GetSelectedMap();

        std::vector<InputBindingData>& bindings = map->Actions[action].Bindings;
        bindings.erase(bindings.begin() + Offset(binding));

        if (m_SelectedAction && m_SelectedAction->Action == action && m_SelectedAction->Binding)
        {
            if (*m_SelectedAction->Binding == binding)
                m_SelectedAction->Binding.reset();
            else if (*m_SelectedAction->Binding > binding)
                --*m_SelectedAction->Binding;
        }

        ClampSelection();
        OnDataChanged();
    }

    InputActionMapData* InputActionsPanel::GetSelectedMap()
    {
        if (!m_SelectedMap || *m_SelectedMap >= m_Working.Maps.size())
            return nullptr;

        return &m_Working.Maps[*m_SelectedMap];
    }

    bool InputActionsPanel::IsSelected(const usize action, const std::optional<usize> binding) const
    {
        return m_SelectedAction && m_SelectedAction->Action == action && m_SelectedAction->Binding == binding;
    }

    void InputActionsPanel::ClampSelection()
    {
        if (m_SelectedMap && *m_SelectedMap >= m_Working.Maps.size())
            m_SelectedMap = m_Working.Maps.empty() ? std::nullopt : std::optional<usize>(m_Working.Maps.size() - 1);

        const InputActionMapData* map = GetSelectedMap();
        if (map == nullptr || !m_SelectedAction)
        {
            if (map == nullptr)
                m_SelectedAction.reset();
            return;
        }

        if (m_SelectedAction->Action >= map->Actions.size())
        {
            m_SelectedAction.reset();
            return;
        }

        const std::vector<InputBindingData>& bindings = map->Actions[m_SelectedAction->Action].Bindings;
        if (m_SelectedAction->Binding && *m_SelectedAction->Binding >= bindings.size())
            m_SelectedAction->Binding.reset();
    }

    void InputActionsPanel::BeginChange() { PushUndo(m_Working); }

    void InputActionsPanel::TrackItemEdit()
    {
        if (ImGui::IsItemActivated())
            m_EditStart = m_FrameStart;

        if (ImGui::IsItemDeactivatedAfterEdit() && m_EditStart)
            PushUndo(std::move(*m_EditStart));

        if (ImGui::IsItemDeactivated())
            m_EditStart.reset();
    }

    void InputActionsPanel::PushUndo(InputActionsData snapshot)
    {
        m_UndoStack.push_back(std::move(snapshot));
        if (m_UndoStack.size() > MaxUndoSteps)
            m_UndoStack.erase(m_UndoStack.begin());

        m_RedoStack.clear();
    }

    void InputActionsPanel::Undo()
    {
        if (m_UndoStack.empty()) return;

        m_RedoStack.push_back(std::move(m_Working));
        m_Working = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();

        m_Listening.reset();
        ClampSelection();
        OnDataChanged();
    }

    void InputActionsPanel::Redo()
    {
        if (m_RedoStack.empty()) return;

        m_UndoStack.push_back(std::move(m_Working));
        m_Working = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();

        m_Listening.reset();
        ClampSelection();
        OnDataChanged();
    }

    void InputActionsPanel::OnDataChanged()
    {
        m_Dirty = true;
        Validate();
    }
}
