#include "Editor/Panels/InspectorPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/LogFormat.h"
#include "Editor/StringUtils.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Scene/Components.h>

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

namespace
{
    template<typename... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };

    template<typename... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

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
        bool (*Has)(ByteForge::Entity);
        void (*Add)(ByteForge::Entity);
    };

    template<typename T>
    constexpr ComponentDescriptor Describe(const char* name)
    {
        return {
            .Name = name,
            .Has  = [](const ByteForge::Entity entity) { return entity.HasComponent<T>(); },
            .Add  = [](const ByteForge::Entity entity) { entity.AddComponent<T>(); }
        };
    }

    constexpr std::array AddableComponents{
        Describe<ByteForge::SpriteRendererComponent>("Sprite Renderer"),
        Describe<ByteForge::Rigidbody2DComponent>("Rigidbody 2D"),
        Describe<ByteForge::BoxCollider2DComponent>("Box Collider 2D"),
        Describe<ByteForge::CircleCollider2DComponent>("Circle Collider 2D")
    };

    constexpr const char* AddComponentPopupId = "##AddComponent";
}

namespace ByteForge
{
    void InspectorPanel::OnImGuiRender()
    {
        if (!m_Open) return;
        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        std::visit(Overloaded{
            [](std::monostate) { ImGui::TextDisabled("Nothing selected"); },
            [this](const Entity entity)
            {
                if (entity.IsValid())
                    DrawComponents(entity);
                else
                    ImGui::TextDisabled("Nothing selected");
            },
            [this](const AssetSelection& asset) { DrawAsset(asset.Handle); },
            [](const LogSelection& log) { DrawLogEntry(log.Entry); }
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
