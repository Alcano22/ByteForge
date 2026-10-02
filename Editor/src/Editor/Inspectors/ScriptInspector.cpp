#include "Editor/Inspectors/ScriptInspector.h"
#include "Editor/AssetPayload.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Overloaded.h"
#include "Editor/ScriptProject.h"
#include "Editor/StringUtils.h"

#include <Engine/Scene/Components.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scripting/ScriptEngine.h>
#include <Engine/Scripting/ScriptField.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <algorithm>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ByteForge::EditorUI
{
    namespace
    {
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

            if (!ImGui::BeginDragDropTarget()) return std::nullopt;

            std::optional<std::string> dropped;
            constexpr ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery
                                               | ImGuiDragDropFlags_AcceptNoDrawDefaultRect
                                               | ImGuiDragDropFlags_AcceptNoPreviewTooltip;

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType, flags))
            {
                const std::expected<std::string, std::string> className =
                    ScriptProject::FindClassForAsset(UUID(dragged->Handle));

                const ImU32 color = className ? ImGui::GetColorU32(ImGuiCol_DragDropTarget) : ImGui::GetColorU32(ErrorColor);
                drawList.AddRect(min, max, color, rounding, 0, 2.0f);

                if (ImGui::BeginTooltip())
                {
                    if (className)
                        ImGui::Text("Script: %s", className->c_str());
                    else
                        ImGui::TextColored(ErrorColor, "%s", className.error().c_str());
                    ImGui::EndTooltip();
                }

                if (className && payload->IsDelivery())
                    dropped = *className;
            }

            ImGui::EndDragDropTarget();
            return dropped;
        }

        bool EditScriptValue(const char* label, ScriptValue& value)
        {
            return std::visit(Overloaded{
                [&](bool& v)      { return ImGui::Checkbox(label, &v); },
                [&](int& v)       { return ImGui::DragInt(label, &v); },
                [&](float& v)     { return ImGui::DragFloat(label, &v, 0.1f); },
                [&](double& v)    { return ImGui::DragScalar(label, ImGuiDataType_Double, &v, 0.1f); },
                [&](glm::vec2& v) { return ImGui::DragFloat2(label, glm::value_ptr(v), 0.1f); },
                [&](glm::vec3& v) { return ImGui::DragFloat3(label, glm::value_ptr(v), 0.1f); },
                [&](glm::vec4& v) { return ImGui::DragFloat4(label, glm::value_ptr(v), 0.1f); }
            }, value);
        }

        void DrawClassPicker(ScriptComponent& script)
        {
            const std::vector<ScriptClassInfo> classes = ScriptEngine::Get().GetClasses();
            const bool known = script.ClassName.empty() ||
                               std::ranges::any_of(classes, [&](const ScriptClassInfo& info)
                               {
                                   return info.Name == script.ClassName;
                               });

            const char* preview = script.ClassName.empty() ? "None" : script.ClassName.c_str();

            if (!known)
                ImGui::PushStyleColor(ImGuiCol_Text, ErrorColor);
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
                ImGui::TextColored(ErrorColor, "Class '%s' not found", script.ClassName.c_str());
        }

        void DrawLiveField(ScriptInstance* live, const ScriptFieldInfo& field, const char* label)
        {
            if (std::optional<ScriptValue> value = live != nullptr ? live->GetField(field.Name) : std::nullopt)
            {
                if (EditScriptValue(label, *value))
                    live->SetField(field.Name, *value);
                return;
            }

            ScriptValue fallback = field.Default;
            ImGui::BeginDisabled();
            EditScriptValue(label, fallback);
            ImGui::EndDisabled();
        }

        void DrawStoredField(ScriptComponent& script, const ScriptFieldInfo& field, const char* label)
        {
            const auto it = script.Fields.find(field.Name);
            const bool overridden = it != script.Fields.end() && GetFieldType(it->second) == field.Type;

            ScriptValue value = overridden ? it->second : field.Default;
            if (EditScriptValue(label, value))
                script.Fields.insert_or_assign(field.Name, std::move(value));

            if (ImGui::BeginPopupContextItem("##FieldMenu"))
            {
                if (ImGui::MenuItem("Reset to Default", nullptr, false, overridden))
                    script.Fields.erase(field.Name);
                ImGui::EndPopup();
            }
        }

        void DrawFields(const Entity entity, ScriptComponent& script)
        {
            const std::span<const ScriptFieldInfo> fields = ScriptEngine::Get().GetFields(script.ClassName);
            if (fields.empty()) return;

            ImGui::Separator();

            Scene& scene = entity.GetScene();
            ScriptInstance* live = scene.FindScriptInstance(entity);

            for (const ScriptFieldInfo& field : fields)
            {
                ImGui::PushID(field.Name.c_str());
                const std::string label = NicifyString(field.Name);

                if (scene.IsRunning())
                    DrawLiveField(live, field, label.c_str());
                else
                    DrawStoredField(script, field, label.c_str());

                ImGui::PopID();
            }
        }
    }

    void DrawScriptComponentInspector(const Entity entity, ScriptComponent& script)
    {
        DrawClassPicker(script);
        DrawFields(entity, script);
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

    void DrawScriptAssetInspector(const UUID handle)
    {
        const std::expected<std::string, std::string> className = ScriptProject::FindClassForAsset(handle);
        if (className)
            ImGui::Text("Class: %s", className->c_str());
        else
            ImGui::TextColored(ErrorColor, "%s", className.error().c_str());
    }
}
