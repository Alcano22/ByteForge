#pragma once

#include "Engine/Core/Core.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>
#include <span>

namespace ByteForge
{
    enum class ShaderDataType
    {
        None = 0,
        Float, Float2, Float3, Float4,
        Mat3, Mat4,
        Int, Int2, Int3, Int4,
        Bool
    };

    inline uint32_t ShaderDataTypeSize(const ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float:  return 4 * 1;
            case ShaderDataType::Float2: return 4 * 2;
            case ShaderDataType::Float3: return 4 * 3;
            case ShaderDataType::Float4: return 4 * 4;
            case ShaderDataType::Mat3:   return 4 * 3 * 3;
            case ShaderDataType::Mat4:   return 4 * 4 * 4;
            case ShaderDataType::Int:    return 4 * 1;
            case ShaderDataType::Int2:   return 4 * 2;
            case ShaderDataType::Int3:   return 4 * 3;
            case ShaderDataType::Int4:   return 4 * 4;
            case ShaderDataType::Bool:   return 1;
            case ShaderDataType::None:   break;
        }

        return 0;
    }

    struct BYTEFORGE_API BufferElement
    {
        std::string Name;
        ShaderDataType Type;
        uint32_t Size;
        size_t Offset;
        bool Normalized;

        BufferElement() = default;

        BufferElement(const ShaderDataType type, std::string name, const bool normalized = false)
            : Name(std::move(name)), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized) {}

        [[nodiscard]] uint32_t GetComponentCount() const
        {
            switch (Type)
            {
                case ShaderDataType::Float:  return 1;
                case ShaderDataType::Float2: return 2;
                case ShaderDataType::Float3: return 3;
                case ShaderDataType::Float4: return 4;
                case ShaderDataType::Mat3:   return 3;
                case ShaderDataType::Mat4:   return 4;
                case ShaderDataType::Int:    return 1;
                case ShaderDataType::Int2:   return 2;
                case ShaderDataType::Int3:   return 3;
                case ShaderDataType::Int4:   return 4;
                case ShaderDataType::Bool:   return 1;
                case ShaderDataType::None:   break;
            }

            return 0;
        }
    };

    class BYTEFORGE_API BufferLayout
    {
    public:
        BufferLayout() = default;

        BufferLayout(const std::initializer_list<BufferElement>& elements)
            : m_Elements(elements)
        {
            CalculateOffsetsAndStride();
        }

        [[nodiscard]] uint32_t GetStride() const { return m_Stride; }
        [[nodiscard]] const std::vector<BufferElement>& GetElements() const { return m_Elements; }

        [[nodiscard]] auto begin() const { return m_Elements.begin(); }
        [[nodiscard]] auto end() const { return m_Elements.end(); }

    private:
        void CalculateOffsetsAndStride()
        {
            size_t offset = 0;
            m_Stride = 0;
            for (auto& element : m_Elements)
            {
                element.Offset = offset;
                offset += element.Size;
                m_Stride += element.Size;
            }
        }

    private:
        std::vector<BufferElement> m_Elements;
        uint32_t m_Stride = 0;
    };

    class BYTEFORGE_API VertexBuffer
    {
    public:
        virtual ~VertexBuffer() = default;

        virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;

        [[nodiscard]] virtual const BufferLayout& GetLayout() const = 0;
        virtual void SetLayout(const BufferLayout& layout) = 0;

        static Ref<VertexBuffer> Create(uint32_t size);
        static Ref<VertexBuffer> Create(const void* vertices, uint32_t size);
    };

    class BYTEFORGE_API IndexBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        [[nodiscard]] virtual uint32_t GetCount() const = 0;

        static Ref<IndexBuffer> Create(std::span<const uint32_t> indices);
    };
}
