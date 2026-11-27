#pragma once

#include "Engine/Core/Core.h"

#include <bit>
#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ByteForge
{
    class ByteWriter
    {
    public:
        void U8(const u8 value) { m_Data.push_back(static_cast<std::byte>(value)); }
        void U16(const u16 value) { Unsigned(value, 2); }
        void U32(const u32 value) { Unsigned(value, 4); }
        void U64(const u64 value) { Unsigned(value, 8); }
        void F32(const f32 value) { U32(std::bit_cast<u32>(value)); }

        void Bytes(const void* data, const usize size)
        {
            const auto* bytes = static_cast<const std::byte*>(data);
            m_Data.insert(m_Data.end(), bytes, bytes + size);
        }

        [[nodiscard]] const std::vector<std::byte>& GetData() const { return m_Data; }
        [[nodiscard]] std::vector<std::byte> Take() { return std::move(m_Data); }

    private:
        void Unsigned(const u64 value, const usize size)
        {
            for (usize i = 0; i < size; ++i)
                m_Data.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFF));
        }

    private:
        std::vector<std::byte> m_Data;
    };

    class ByteReader
    {
    public:
        explicit ByteReader(const std::span<const std::byte> data)
            : m_Data(data) {}

        u8 U8() { return static_cast<u8>(Unsigned(1)); }
        u16 U16() { return static_cast<u16>(Unsigned(2)); }
        u32 U32() { return static_cast<u32>(Unsigned(4)); }
        u64 U64() { return Unsigned(8); }
        f32 F32() { return std::bit_cast<f32>(U32()); }

        std::string String(const usize size)
        {
            if (!Has(size))
            {
                m_Failed = true;
                return {};
            }

            std::string text(reinterpret_cast<const char*>(m_Data.data() + m_Position), size);
            m_Position += size;
            return text;
        }

        std::span<const std::byte> Bytes(const usize size)
        {
            if (!Has(size))
            {
                m_Failed = true;
                return {};
            }

            const std::span<const std::byte> bytes = m_Data.subspan(m_Position, size);
            m_Position += size;
            return bytes;
        }

        [[nodiscard]] bool Failed() const { return m_Failed; }

    private:
        [[nodiscard]] bool Has(const usize size) const { return !m_Failed && m_Data.size() - m_Position >= size; }

        u64 Unsigned(const usize size)
        {
            if (!Has(size))
            {
                m_Failed = true;
                return 0;
            }

            u64 value = 0;
            for (usize i = 0; i < size; ++i)
                value |= std::to_integer<u64>(m_Data[m_Position + i]) << (8 * i);

            m_Position += size;
            return value;
        }

    private:
        std::span<const std::byte> m_Data;
        usize m_Position = 0;
        bool m_Failed = false;
    };

    inline constexpr u64 HashSeed = 0xCBF29CE484222325ull;

    [[nodiscard]] inline u64 HashBytes(const std::span<const std::byte> data, u64 hash = HashSeed)
    {
        for (const std::byte b : data)
        {
            hash ^= std::to_integer<u64>(b);
            hash *= 0x100000001b3ull;
        }
        return hash;
    }
}
