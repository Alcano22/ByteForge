#include "Editor/Panels/InspectorPanel.h"
#include "Editor/EditorContext.h"

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
            [](const Entity entity)
            {
                if (entity.IsValid())
                    DrawComponents(entity);
                else
                    ImGui::TextDisabled("Nothing selected");
            },
            [this](const AssetSelection& asset) { DrawAsset(asset.Handle); }
        }, GetContext().SelectionContext.Get());

        ImGui::End();
    }

    void InspectorPanel::DrawComponents(const Entity entity)
    {
        {
            auto& tag = entity.GetComponent<TagComponent>();

            std::array<char, 256> buffer{};
            const size_t length = std::min(tag.Tag.size(), buffer.size() - 1);
            std::memcpy(buffer.data(), tag.Tag.data(), length);
            buffer[length] = '\0';

            if (ImGui::InputText("##Tag", buffer.data(), buffer.size()))
                tag.Tag = buffer.data();
        }

        ImGui::Separator();

        DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform)
        {
            ImGui::DragFloat3("Position", glm::value_ptr(transform.Position), 0.1f);
            float rotationDeg = glm::degrees(transform.Rotation);
            if (ImGui::DragFloat("Rotation", &rotationDeg, 0.5f))
                transform.Rotation = glm::radians(rotationDeg);
            ImGui::DragFloat2("Scale", glm::value_ptr(transform.Scale), 0.1f);
        });

        DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](SpriteRendererComponent& spriteRenderer)
        {
            ImGui::ColorEdit4("Color", glm::value_ptr(spriteRenderer.Color));
            ImGui::Text("SubTexture: %s", spriteRenderer.SubTexture ? "bound" : "none");
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
}
