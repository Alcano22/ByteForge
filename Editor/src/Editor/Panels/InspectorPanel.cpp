#include "Editor/Panels/InspectorPanel.h"
#include "Editor/EditorContext.h"

#include <Engine/Scene/Components.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <array>
#include <cstring>
#include <string>

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

        const Entity selection = GetContext().SelectionContext;
        if (selection.IsValid())
            DrawComponents(selection);
        else
            ImGui::TextDisabled("No entity selected");

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
}
