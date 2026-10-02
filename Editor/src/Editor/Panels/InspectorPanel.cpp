#include "Editor/Panels/InspectorPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/LogFormat.h"
#include "Editor/StringUtils.h"
#include "Editor/AssetPayload.h"
#include "Editor/ScriptProject.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scripting/ScriptEngine.h>
#include <Engine/Scripting/ScriptField.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

#include <array>
#include <cstring>
#include <string>
#include <span>
#include <variant>
#include <algorithm>
#include <limits>
#include <string_view>
#include <filesystem>
#include <format>
#include <cctype>
#include <optional>
#include <expected>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr ImVec4 ScriptErrorColor{ 0.95f, 0.40f, 0.40f, 1.0f };

        void AssignScript(ScriptComponent& script, std::string className)
        {
            if (script.ClassName == className) return;

            script.ClassName = std::move(className);
            script.Fields.clear();
        }

        std::optional<std::string> AcceptScriptDrop()
        {
            const std::optional<AssetPayload> dragged = ReadAssetPayload(ImGui::GetDragDropPayload());
            if (!dragged || dragged->Type != AssetType::Script)
                return std::nullopt;

            ImDrawList& drawList = *ImGui::GetWindowDrawList();
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const float rounding = ImGui::GetStyle().FrameRounding;

            drawList.AddRect(min, max, ImGui::GetColorU32(ImGuiCol_DragDropTarget, 0.35f), rounding);

            if (!ImGui::BeginDragDropTarget())
                return std::nullopt;

            std::optional<std::string> dropped;
            constexpr ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery
                                               | ImGuiDragDropFlags_AcceptNoDrawDefaultRect
                                               | ImGuiDragDropFlags_AcceptNoPreviewTooltip;

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType, flags))
            {
                const std::expected<std::string, std::string> className =
                    ScriptProject::FindClassForAsset(UUID(dragged->Handle));

                const ImU32 color = className ? ImGui::GetColorU32(ImGuiCol_DragDropTarget)
                                              : ImGui::GetColorU32(ScriptErrorColor);
                drawList.AddRect(min, max, color, rounding, 0, 2.0f);

                if (ImGui::BeginTooltip())
                {
                    if (className)
                        ImGui::Text("Script: %s", className->c_str());
                    else
                        ImGui::TextColored(ScriptErrorColor, "%s", className.error().c_str());
                    ImGui::EndTooltip();
                }

                if (className && payload->IsDelivery())
                    dropped = *className;
            }

            ImGui::EndDragDropTarget();
            return dropped;
        }

        void DrawScriptDropZone(const Entity entity)
        {
            const ImVec2 available = ImGui::GetContentRegionAvail();
            ImGui::Dummy(ImVec2(available.x, std::max(available.y, ImGui::GetFrameHeight() * 3.0f)));

            if (std::optional<std::string> className = AcceptScriptDrop())
            {
                auto& script = entity.HasComponent<ScriptComponent>()
                             ? entity.GetComponent<ScriptComponent>()
                             : entity.AddComponent<ScriptComponent>();
                AssignScript(script, std::move(*className));
            }
        }

        template<typename... Ts>
        struct Overloaded : Ts...
        {
            using Ts::operator()...;
        };

        template<typename... Ts>
        Overloaded(Ts...) -> Overloaded<Ts...>;

        std::string NicifyName(std::string_view name)
        {
            while (name.starts_with('_'))
                name.remove_prefix(1);

            std::string result;
            result.reserve(name.size() + 4);

            for (size_t i = 0; i < name.size(); ++i)
            {
                const auto c = static_cast<unsigned char>(name[i]);
                if (i > 0 && std::isupper(c) && !std::isupper(static_cast<unsigned char>(name[i - 1])))
                    result += ' ';

                result += static_cast<char>(i == 0 ? std::toupper(c) : c);
            }
            return result;
        }

        bool DrawScriptValue(const char* label, ScriptValue& value)
        {
            return std::visit(Overloaded{
                [&](bool& v)      { return ImGui::Checkbox(label, &v); },
                [&](int32_t& v)   { return ImGui::DragInt(label, &v); },
                [&](float& v)     { return ImGui::DragFloat(label, &v, 0.1f); },
                [&](double& v)    { return ImGui::DragScalar(label, ImGuiDataType_Double, &v, 0.1f); },
                [&](glm::vec2& v) { return ImGui::DragFloat2(label, glm::value_ptr(v), 0.1f); },
                [&](glm::vec3& v) { return ImGui::DragFloat3(label, glm::value_ptr(v), 0.1f); },
                [&](glm::vec4& v) { return ImGui::DragFloat4(label, glm::value_ptr(v), 0.1f); }
            }, value);
        }

        void DrawScriptFields(const Entity entity, ScriptComponent& script)
        {
            const std::span<const ScriptFieldInfo> fields = ScriptEngine::Get().GetFields(script.ClassName);
            if (fields.empty()) return;

            ImGui::SeparatorText("Fields");

            Scene& scene = entity.GetScene();
            ScriptInstance* live = scene.FindScriptInstance(entity);

            for (const ScriptFieldInfo& field : fields)
            {
                ImGui::PushID(field.Name.c_str());
                const std::string label = NicifyName(field.Name);

                if (scene.IsRunning())
                {
                    std::optional<ScriptValue> value = live != nullptr ? live->GetField(field.Name) : std::nullopt;
                    if (value)
                    {
                        if (DrawScriptValue(label.c_str(), *value))
                            live->SetField(field.Name, *value);
                    } else
                    {
                        ScriptValue fallback = field.Default;
                        ImGui::BeginDisabled();
                        DrawScriptValue(label.c_str(), fallback);
                        ImGui::EndDisabled();
                    }
                } else
                {
                    const auto it = script.Fields.find(field.Name);
                    const bool overridden = it != script.Fields.end() && GetFieldType(it->second) == field.Type;

                    ScriptValue value = overridden ? it->second : field.Default;
                    if (DrawScriptValue(label.c_str(), value))
                        script.Fields.insert_or_assign(field.Name, std::move(value));

                    if (ImGui::BeginPopupContextItem("##FieldMenu"))
                    {
                        if (ImGui::MenuItem("Reset to Default", nullptr, false, overridden))
                            script.Fields.erase(field.Name);
                        ImGui::EndPopup();
                    }
                }

                ImGui::PopID();
            }
        }

        template<typename T>
        bool EnumCombo(const char* label, T& value, const std::span<const T> options)
        {
            bool changed = false;

            if (ImGui::BeginCombo(label, std::string(magic_enum::enum_name(value)).c_str()))
            {
                for (const T option : options)
                {
                    const bool selected = option == value;
                    if (ImGui::Selectable(std::string(magic_enum::enum_name(option)).c_str(), selected) && !selected)
                    {
                        value = option;
                        changed = true;
                    }

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            return changed;
        }

        struct ComponentDescriptor
        {
            const char* Name;
            bool (*Has)(Entity);
            void (*Add)(Entity);
        };

        template<typename T>
        constexpr ComponentDescriptor Describe(const char* name)
        {
            return {
                .Name = name,
                .Has  = [](const Entity entity) { return entity.HasComponent<T>(); },
                .Add  = [](const Entity entity) { entity.AddComponent<T>(); }
            };
        }

        constexpr std::array AddableComponents{
            Describe<SpriteRendererComponent>("Sprite Renderer"),
            Describe<Rigidbody2DComponent>("Rigidbody 2D"),
            Describe<BoxCollider2DComponent>("Box Collider 2D"),
            Describe<CircleCollider2DComponent>("Circle Collider 2D"),
            Describe<ScriptComponent>("Script")
        };

        constexpr const char* AddComponentPopupId = "##AddComponent";
    }

    void InspectorPanel::OnImGuiRender()
    {
        if (!m_Open) return;
        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        CommandHistory& history = GetContext().History;

        std::visit(Overloaded{
            [&](std::monostate)
            {
                m_EditTracker.Flush(history);
                ImGui::TextDisabled("Nothing selected");
            },
            [&](const Entity entity)
            {
                if (!entity.IsValid())
                {
                    m_EditTracker.Flush(history);
                    ImGui::TextDisabled("Nothing selected");
                    return;
                }

                if (!GetContext().IsEditing())
                {
                    m_EditTracker.Cancel();
                    DrawComponents(entity);
                    return;
                }

                m_EditTracker.Begin(entity, history);
                DrawComponents(entity);
                m_EditTracker.End(entity, history);
            },
            [&](const AssetSelection& asset)
            {
                m_EditTracker.Flush(history);
                DrawAsset(asset.Handle);
            },
            [&](const LogSelection& log)
            {
                m_EditTracker.Flush(history);
                DrawLogEntry(log.Entry);
            }
        }, GetContext().SelectionContext.Get());

        ImGui::End();
    }

    void InspectorPanel::DrawComponents(const Entity entity) const
    {
        DrawHeader(entity);

        ImGui::Separator();

        DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform)
        {
            ImGui::DragFloat3("Position", glm::value_ptr(transform.Position), 0.1f);
            float rotationDeg = glm::degrees(transform.Rotation);
            if (ImGui::DragFloat("Rotation", &rotationDeg, 0.5f))
                transform.Rotation = glm::radians(rotationDeg);
            ImGui::DragFloat2("Scale", glm::value_ptr(transform.Scale), 0.1f);
        }, false);

        DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [this](SpriteRendererComponent& spriteRenderer)
        {
            ImGui::ColorEdit4("Color", glm::value_ptr(spriteRenderer.Color));

            Ref<TextureAsset> texture = spriteRenderer.Sprite ? spriteRenderer.Sprite->GetTextureAsset() : nullptr;
            if (EditorUI::TextureAssetField("Texture", texture, GetContext().SelectionContext))
                spriteRenderer.Sprite = texture ? Sprite::Create(texture) : nullptr;
        });

        DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](Rigidbody2DComponent& rigidbody)
        {
            static constexpr std::array<const char*, 3> bodyTypeNames = { "Static", "Kinematic", "Dynamic" };
            int index = static_cast<int>(rigidbody.Type);

            if (ImGui::Combo("Body Type", &index, bodyTypeNames.data(), static_cast<int>(bodyTypeNames.size())))
                rigidbody.Type = static_cast<Rigidbody2DComponent::BodyType>(index);

            ImGui::Checkbox("Fixed Rotation", &rigidbody.FixedRotation);
            ImGui::DragFloat("Gravity Scale", &rigidbody.GravityScale, 0.05f);

            if (rigidbody.HasRuntimeBody())
                ImGui::TextDisabled("(runtime body active - changes above apply on next play)");
        });

        DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](BoxCollider2DComponent& collider)
        {
            ImGui::DragFloat2("Offset", glm::value_ptr(collider.Offset), 0.02f);
            ImGui::DragFloat2("Size", glm::value_ptr(collider.Size), 0.02f, 0.01f, std::numeric_limits<float>::max());
            ImGui::DragFloat("Density", &collider.Density, 0.05f, 0.0f, std::numeric_limits<float>::max());
            ImGui::DragFloat("Friction", &collider.Friction, 0.02f, 0.0f, 1.0f);
            ImGui::DragFloat("Restitution", &collider.Restitution, 0.02f, 0.0f, 1.0f);
            ImGui::Checkbox("Is Sensor", &collider.IsSensor);
        });

        DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](CircleCollider2DComponent& collider)
        {
            ImGui::DragFloat2("Offset", glm::value_ptr(collider.Offset), 0.02f);
            ImGui::DragFloat("Radius", &collider.Radius, 0.02f, 0.01f, std::numeric_limits<float>::max());
            ImGui::DragFloat("Density", &collider.Density, 0.05f, 0.0f, std::numeric_limits<float>::max());
            ImGui::DragFloat("Friction", &collider.Friction, 0.02f, 0.0f, 1.0f);
            ImGui::DragFloat("Restitution", &collider.Restitution, 0.02f, 0.0f, 1.0f);
            ImGui::Checkbox("Is Sensor", &collider.IsSensor);
        });

        DrawComponent<ScriptComponent>("Script", entity, [entity](ScriptComponent& script)
        {
            const std::vector<ScriptClassInfo> classes = ScriptEngine::Get().GetClasses();
            const bool known = script.ClassName.empty() ||
                               std::ranges::any_of(classes, [&](const ScriptClassInfo& info)
                               {
                                   return info.Name == script.ClassName;
                               });

            const char* preview = script.ClassName.empty() ? "None" : script.ClassName.c_str();

            if (!known)
                ImGui::PushStyleColor(ImGuiCol_Text, ScriptErrorColor);
            const bool open = ImGui::BeginCombo("Class", preview);
            if (!known)
                ImGui::PopStyleColor();

            if (open)
            {
                if (ImGui::Selectable("None", script.ClassName.empty()))
                    AssignScript(script, {});

                for (const ScriptClassInfo& info : classes)
                {
                    if (ImGui::Selectable(info.Name.c_str(), info.Name == script.ClassName))
                        AssignScript(script, info.Name);

                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", info.Backend.c_str());
                }

                ImGui::EndCombo();
            } else if (std::optional<std::string> className = AcceptScriptDrop())
                AssignScript(script, std::move(*className));

            if (!known)
                ImGui::TextColored(ScriptErrorColor, "Class '%s' not found", script.ClassName.c_str());

            DrawScriptFields(entity, script);
        });

        DrawScriptDropZone(entity);
    }

    void InspectorPanel::DrawHeader(const Entity entity)
    {
        auto& tag = entity.GetComponent<TagComponent>();

        std::array<char, 256> buffer{};
        const size_t length = std::min(tag.Tag.size(), buffer.size() - 1);
        std::memcpy(buffer.data(), tag.Tag.data(), length);
        buffer[length] = '\0';

        const ImGuiStyle& style = ImGui::GetStyle();
        const float buttonWidth = ImGui::CalcTextSize("Add Component").x + style.FramePadding.x * 2.0f;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - style.ItemSpacing.x);
        if (ImGui::InputText("##Tag", buffer.data(), buffer.size()))
            tag.Tag = buffer.data();

        ImGui::SameLine();
        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup(AddComponentPopupId);

        DrawAddComponentPopup(entity);
    }

    void InspectorPanel::DrawAddComponentPopup(const Entity entity)
    {
        if (!ImGui::BeginPopup(AddComponentPopupId)) return;

        static std::array<char, 64> s_Filter{};
        if (ImGui::IsWindowAppearing())
        {
            s_Filter.fill('\0');
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::SetNextItemWidth(220.0f);
        const bool submitted = ImGui::InputTextWithHint("##search", "Search components...", s_Filter.data(),
                                                        s_Filter.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Separator();

        const std::string_view query(s_Filter.data());
        bool anyMatch = false;

        for (const ComponentDescriptor& component : AddableComponents)
        {
            if (component.Has(entity) || !ContainsIgnoreCase(component.Name, query)) continue;

            const bool pickedByEnter = submitted && !anyMatch;
            anyMatch = true;

            if (ImGui::Selectable(component.Name) || pickedByEnter)
            {
                component.Add(entity);
                ImGui::CloseCurrentPopup();
            }
        }

        if (!anyMatch)
            ImGui::TextDisabled(query.empty() ? "All components added" : "No components match");

        ImGui::EndPopup();
    }

    void InspectorPanel::DrawAsset(const UUID handle) const
    {
        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata))
        {
            ImGui::TextDisabled("Asset no longer exists");
            return;
        }

        ImGui::TextUnformatted(metadata.Path.filename().string().c_str());
        ImGui::TextDisabled("%s", AssetTypeToString(metadata.Type));
        ImGui::Separator();

        switch (metadata.Type)
        {
            case AssetType::Texture2D: DrawTextureSettings(metadata); break;
            case AssetType::Script:
            {
                const std::expected<std::string, std::string> className = ScriptProject::FindClassForAsset(handle);
                if (className)
                    ImGui::Text("Class: %s", className->c_str());
                else
                    ImGui::TextColored(ScriptErrorColor, "%s", className.error().c_str());
                break;
            }
            case AssetType::None:      break;
        }
    }

    void InspectorPanel::DrawTextureSettings(const AssetMetadata& metadata) const
    {
        const TextureSettings* stored = metadata.GetSettings<TextureSettings>();
        TextureSettings settings = stored != nullptr ? *stored : TextureSettings{};

        constexpr std::array<ImageFormat, 2> formats{ ImageFormat::RGBA8_SRGB, ImageFormat::RGBA8_UNORM };

        bool changed = false;
        changed |= EnumCombo<ImageFormat>("Format", settings.Format, formats);
        changed |= EnumCombo<TextureFilter>("Filter", settings.Filter, magic_enum::enum_values<TextureFilter>());
        changed |= EnumCombo<TextureWrap>("Wrap", settings.Wrap, magic_enum::enum_values<TextureWrap>());
        changed |= ImGui::Checkbox("Generate Mips", &settings.GenerateMips);

        if (changed)
            GetContext().ApplyTextureSettings(metadata.Handle, settings);
    }

    void InspectorPanel::DrawLogEntry(const LogEntry& entry)
    {
        ImGui::TextColored(LogLevelColor(entry.Level), "%s", LogLevelName(entry.Level));
        ImGui::SameLine();
        ImGui::TextDisabled("%s", entry.Logger.c_str());

        ImGui::Separator();

        ImGui::TextWrapped("%s", entry.Message.c_str());
        if (ImGui::SmallButton("Copy Message"))
            ImGui::SetClipboardText(entry.Message.c_str());

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::BeginTable("##details", 2, ImGuiTableFlags_SizingFixedFit))
        {
            const auto row = [](const char* label, const std::string& value)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            };

            row("Time", FormatLogTime(entry.Time, true));

            if (entry.File.empty())
                row("Source", "unknown");
            else
            {
                const std::filesystem::path file(entry.File);
                row("Source", std::format("{}:{}", file.filename().string(), entry.Line));
                ImGui::SetItemTooltip("%s", entry.File.c_str());

                row("Function", entry.Function);
            }

            row("Thread", std::to_string(entry.ThreadId));
            row("Entry", std::format("#{}", entry.Id));

            ImGui::EndTable();
        }
    }
}
