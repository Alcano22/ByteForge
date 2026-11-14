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
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <expected>
#include <format>
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
        std::optional<std::string> AcceptScriptDrop()
        {
            const std::optional<AssetPayload> dragged = ReadAssetPayload(ImGui::GetDragDropPayload());
            if (!dragged || (dragged->Type != AssetType::Script && dragged->Type != AssetType::ScriptGraph))
                return std::nullopt;

            ImDrawList& drawList = *ImGui::GetWindowDrawList();
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const f32 rounding = ImGui::GetStyle().FrameRounding;

            drawList.AddRect(min, max, ImGui::GetColorU32(ImGuiCol_DragDropTarget, 0.35f), rounding);

            if (!ImGui::BeginDragDropTarget()) return std::nullopt;

            std::optional<std::string> dropped;
            constexpr ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery
                                               | ImGuiDragDropFlags_AcceptNoDrawDefaultRect
                                               | ImGuiDragDropFlags_AcceptNoPreviewTooltip;

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType, flags))
            {
                const Result<std::string> className = dragged->Type == AssetType::ScriptGraph
                    ? Result<std::string>(ScriptGraphClassName(UUID(dragged->Handle)))
                    : ScriptProject::FindClassForAsset(UUID(dragged->Handle));

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

        bool EditScriptValue(const char* label, ScriptValue& value, Scene& scene, Selection& selection)
        {
            return std::visit(Overloaded{
                [&](bool& v)        { return ImGui::Checkbox(label, &v); },
                [&](i32& v)         { return ImGui::DragInt(label, &v); },
                [&](f32& v)         { return ImGui::DragFloat(label, &v, 0.1f); },
                [&](f64& v)         { return ImGui::DragScalar(label, ImGuiDataType_Double, &v, 0.1f); },
                [&](glm::vec2& v)   { return ImGui::DragFloat2(label, glm::value_ptr(v), 0.1f); },
                [&](glm::vec3& v)   { return ImGui::DragFloat3(label, glm::value_ptr(v), 0.1f); },
                [&](glm::vec4& v)   { return ImGui::DragFloat4(label, glm::value_ptr(v), 0.1f); },
                [&](EntityRef& v)   { return EntityReferenceField(label, v, scene); },
                [&](AssetRef& v)    { return AssetReferenceField(label, v.Type, v.Handle, selection); },
                [&](std::string& v) { return ImGui::InputText(label, &v); }
            }, value);
        }

        void DrawLiveField(ScriptInstance* live, const ScriptFieldInfo& field,
                           const char* label, Scene& scene, Selection& selection)
        {
            if (std::optional<ScriptValue> value = live != nullptr ? live->GetField(field.Name) : std::nullopt)
            {
                if (EditScriptValue(label, *value, scene, selection))
                    live->SetField(field.Name, *value);
                return;
            }

            ScriptValue fallback = field.Default;
            ImGui::BeginDisabled();
            EditScriptValue(label, fallback, scene, selection);
            ImGui::EndDisabled();
        }

        void DrawStoredField(ScriptEntry& script, const ScriptFieldInfo& field,
                             const char* label, Scene& scene, Selection& selection)
        {
            const auto it = script.Fields.find(field.Name);
            const bool overridden = it != script.Fields.end() && IsCompatible(it->second, field);

            ScriptValue value = overridden ? it->second : field.Default;
            if (EditScriptValue(label, value, scene, selection))
                script.Fields.insert_or_assign(field.Name, std::move(value));

            if (ImGui::BeginPopupContextItem("##FieldMenu"))
            {
                if (ImGui::MenuItem("Reset to Default", nullptr, false, overridden))
                    script.Fields.erase(field.Name);
                ImGui::EndPopup();
            }
        }

        void DrawFields(const Entity entity, ScriptEntry& script, Selection& selection)
        {
            const std::span<const ScriptFieldInfo> fields = ScriptEngine::Get().GetFields(script.ClassName);
            if (fields.empty())
            {
                ImGui::TextDisabled("No editable fields");
                return;
            }

            Scene& scene = entity.GetScene();
            ScriptInstance* live = scene.FindScriptInstance(entity, script.Id);

            for (const ScriptFieldInfo& field : fields)
            {
                ImGui::PushID(field.Name.c_str());
                const std::string label = NicifyString(field.Name);

                if (scene.IsRunning())
                    DrawLiveField(live, field, label.c_str(), scene, selection);
                else
                    DrawStoredField(script, field, label.c_str(), scene, selection);

                ImGui::PopID();
            }
        }
    }

    std::string GetScriptTitle(const ScriptEntry& script)
    {
        if (script.ClassName.empty())
            return "Script (None)";

        if (IsScriptMissing(script))
            return std::format("{} (Missing)", script.ClassName);

        return ScriptEngine::Get().GetDisplayName(script.ClassName);
    }

    bool IsScriptMissing(const ScriptEntry& script)
    {
        return !script.ClassName.empty() && ScriptEngine::Get().FindBackend(script.ClassName) == nullptr;
    }

    void DrawScriptEntryInspector(const Entity entity, ScriptEntry& script, Selection& selection)
    {
        if (script.ClassName.empty())
        {
            ImGui::TextDisabled("This script has no class. Remove it and add a script again.");
            return;
        }

        if (IsScriptMissing(script))
        {
            ImGui::TextColored(ErrorColor, "Class '%s' not found", script.ClassName.c_str());
            ImGui::TextDisabled("It was renamed or deleted. Its saved values are kept.");
            return;
        }

        DrawFields(entity, script, selection);
    }

    ScriptEntry& AddScript(const Entity entity, std::string className)
    {
        auto& component = entity.HasComponent<ScriptComponent>()
                        ? entity.GetComponent<ScriptComponent>()
                        : entity.AddComponent<ScriptComponent>();
        return component.Add(std::move(className));
    }

    void RemoveScript(const Entity entity, const UUID script)
    {
        auto* component = entity.TryGetComponent<ScriptComponent>();
        if (component == nullptr) return;

        std::erase_if(component->Scripts, [script](const ScriptEntry& entry)
        {
            return static_cast<u64>(entry.Id) == static_cast<u64>(script);
        });

        if (component->Scripts.empty())
            entity.RemoveComponent<ScriptComponent>();
    }

    void DrawScriptDropZone(const Entity entity)
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        ImGui::Dummy(ImVec2(available.x, std::max(available.y, ImGui::GetFrameHeight() * 3.0f)));

        if (std::optional<std::string> className = AcceptScriptDrop())
            AddScript(entity, std::move(*className));
    }

    void DrawScriptAssetInspector(const UUID handle)
    {
        const Result<std::string> className = ScriptProject::FindClassForAsset(handle);
        if (className)
            ImGui::Text("Class: %s", className->c_str());
        else
            ImGui::TextColored(ErrorColor, "%s", className.error().c_str());
    }
}
