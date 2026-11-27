#include "Engine/Input/DualSenseTriggerEffect.h"

#include <algorithm>

namespace ByteForge
{
    namespace
    {
        constexpr u8 ModeOff       = 0x05;
        constexpr u8 ModeFeedback  = 0x21;
        constexpr u8 ModeWeapon    = 0x25;
        constexpr u8 ModeVibration = 0x26;

        constexpr u8 ZoneCount   = 10;
        constexpr u8 MaxStrength = 8;

        DualSenseTriggerPacket Off()
        {
            DualSenseTriggerPacket packet{};
            packet[0] = ModeOff;
            return packet;
        }

        u16 ActiveZones(const u8 position)
        {
            u16 zones = 0;
            for (u8 zone = position; zone < ZoneCount; ++zone)
                zones = static_cast<u16>(zones | (1u << zone));
            return zones;
        }

        u32 ZoneStrengths(const u8 position, const u8 strength)
        {
            const u32 value = static_cast<u32>(strength - 1) & 0x07u;

            u32 zones = 0;
            for (u8 zone = position; zone < ZoneCount; ++zone)
                zones |= value << (3u * zone);
            return zones;
        }

        void WriteZones(DualSenseTriggerPacket& packet, const u16 active, const u32 strengths)
        {
            packet[1] = static_cast<u8>(active & 0xFFu);
            packet[2] = static_cast<u8>(active >> 8);
            packet[3] = static_cast<u8>(strengths & 0xFFu);
            packet[4] = static_cast<u8>((strengths >> 8) & 0xFFu);
            packet[5] = static_cast<u8>((strengths >> 16) & 0xFFu);
            packet[6] = static_cast<u8>((strengths >> 24) & 0xFFu);
        }

        DualSenseTriggerPacket Encode(const TriggerOff&) { return Off(); }

        DualSenseTriggerPacket Encode(const TriggerFeedback& effect)
        {
            if (effect.Strength == 0)
                return Off();

            const u8 position = std::min<u8>(effect.Position, ZoneCount - 1);
            const u8 strength = std::min(effect.Strength, MaxStrength);

            DualSenseTriggerPacket packet{};
            packet[0] = ModeFeedback;
            WriteZones(packet, ActiveZones(position), ZoneStrengths(position, strength));
            return packet;
        }

        DualSenseTriggerPacket Encode(const TriggerWeapon& effect)
        {
            if (effect.Strength == 0)
                return Off();

            const u8 start = std::clamp<u8>(effect.Start, 2, 7);
            const u8 end = std::clamp<u8>(effect.End, static_cast<u8>(start + 1), 8);
            const u8 strength = std::min(effect.Strength, MaxStrength);
            const auto zones = static_cast<u16>((1u << start) | (1u << end));

            DualSenseTriggerPacket packet{};
            packet[0] = ModeWeapon;
            packet[1] = static_cast<u8>(zones & 0xFFu);
            packet[2] = static_cast<u8>(zones >> 8);
            packet[3] = static_cast<u8>(strength - 1);
            return packet;
        }

        DualSenseTriggerPacket Encode(const TriggerVibration& effect)
        {
            if (effect.Amplitude == 0 || effect.Frequency == 0)
                return Off();

            const u8 position = std::min<u8>(effect.Position, ZoneCount - 1);
            const u8 amplitude = std::min(effect.Amplitude, MaxStrength);

            DualSenseTriggerPacket packet{};
            packet[0] = ModeVibration;
            WriteZones(packet, ActiveZones(position), ZoneStrengths(position, amplitude));
            packet[9] = effect.Frequency;
            return packet;
        }
    }

    DualSenseTriggerPacket EncodeDualSenseTriggerEffect(const TriggerEffect& effect)
    {
        return std::visit([](const auto& value) { return Encode(value); }, effect);
    }
}
