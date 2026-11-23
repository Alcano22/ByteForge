#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace ByteForge
{
    inline constexpr const char* MaterialExtension = ".bfmat";

    enum class MaterialParameterType : u8
    {
        Float,
        Color,
        Texture
    };

    struct MaterialTexture
    {
        UUID Handle{ 0 };

        bool operator==(const MaterialTexture&) const = default;
    };

    using MaterialValue = std::variant<f32, glm::vec4, MaterialTexture>;

    [[nodiscard]] BYTEFORGE_API MaterialParameterType GetMaterialParameterType(const MaterialValue& value);

    struct MaterialParameterInfo
    {
        std::string_view Name;
        MaterialParameterType Type = MaterialParameterType::Float;
        MaterialValue Default;
        std::string_view Tooltip;
    };

    struct MaterialShaderInfo
    {
        std::string_view Id;
        std::string_view DisplayName;
        std::string_view Description;
        std::span<const MaterialParameterInfo> Parameters;

        [[nodiscard]] const MaterialParameterInfo* FindParameter(const std::string_view name) const
        {
            for (const MaterialParameterInfo& parameter : Parameters)
            {
                if (parameter.Name == name)
                    return &parameter;
            }
            return nullptr;
        }
    };

    inline constexpr std::string_view SpriteUnlitShader = "Sprite/Unlit";
    inline constexpr std::string_view SpriteLitShader   = "Sprite/Lit";

    [[nodiscard]] BYTEFORGE_API std::span<const MaterialShaderInfo> GetMaterialShaders();
    [[nodiscard]] BYTEFORGE_API const MaterialShaderInfo* FindMaterialShader(std::string_view id);

    struct MaterialData
    {
        std::string Shader{ SpriteUnlitShader };

        std::map<std::string, MaterialValue, std::less<>> Parameters;

        bool operator==(const MaterialData&) const = default;
    };

    [[nodiscard]] BYTEFORGE_API MaterialValue ResolveMaterialParameter(const MaterialData& data,
                                                                       const MaterialParameterInfo& parameter);

    class BYTEFORGE_API MaterialAsset
    {
    public:
        MaterialAsset(UUID handle, MaterialData data);

        [[nodiscard]] UUID GetHandle() const { return m_Handle; }
        [[nodiscard]] const MaterialData& GetData() const { return m_Data; }
        [[nodiscard]] u64 GetRevision() const { return m_Revision; }

    private:
        void SetData(MaterialData data)
        {
            m_Data = std::move(data);
            ++m_Revision;
        }

    private:
        friend class AssetManager;

        UUID m_Handle;
        MaterialData m_Data;
        u64 m_Revision = 0;
    };

    [[nodiscard]] BYTEFORGE_API Result<MaterialData> ParseMaterial(const nlohmann::json& data);
    [[nodiscard]] BYTEFORGE_API nlohmann::json SerializeMaterial(const MaterialData& data);
}
