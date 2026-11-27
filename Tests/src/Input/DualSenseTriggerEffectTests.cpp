#include <Engine/Input/DualSenseTriggerEffect.h>

#include <catch2/catch_test_macros.hpp>

using namespace ByteForge;

TEST_CASE("Off and zero strength encode as the off mode", "[Input][DualSense]")
{
    const DualSenseTriggerPacket off{ 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    CHECK(EncodeDualSenseTriggerEffect(TriggerOff{}) == off);
    CHECK(EncodeDualSenseTriggerEffect(TriggerFeedback{ .Position = 3, .Strength = 0 }) == off);
    CHECK(EncodeDualSenseTriggerEffect(TriggerWeapon{ .Strength = 0 }) == off);
    CHECK(EncodeDualSenseTriggerEffect(TriggerVibration{ .Frequency = 0 }) == off);
}

TEST_CASE("Feedback covers every zone from its position", "[Input][DualSense]")
{
    CHECK(EncodeDualSenseTriggerEffect(TriggerFeedback{ .Position = 0, .Strength = 8 }) ==
          DualSenseTriggerPacket{ 0x21, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0x3F, 0, 0, 0, 0 });

    CHECK(EncodeDualSenseTriggerEffect(TriggerFeedback{ .Position = 5, .Strength = 1 }) ==
          DualSenseTriggerPacket{ 0x21, 0xE0, 0x03, 0, 0, 0, 0, 0, 0, 0, 0 });
}

TEST_CASE("Weapon marks its start and end zone", "[Input][DualSense]")
{
    CHECK(EncodeDualSenseTriggerEffect(TriggerWeapon{ .Start = 2, .End = 6, .Strength = 8 }) ==
          DualSenseTriggerPacket{ 0x25, 0x44, 0x00, 0x07, 0, 0, 0, 0, 0, 0, 0 });

    CHECK(EncodeDualSenseTriggerEffect(TriggerWeapon{ .Start = 9, .End = 3, .Strength = 1 }) ==
          DualSenseTriggerPacket{ 0x25, 0x80, 0x01, 0x00, 0, 0, 0, 0, 0, 0, 0 });
}

TEST_CASE("Vibration stores its frequency after the zones", "[Input][DualSense]")
{
    CHECK(EncodeDualSenseTriggerEffect(TriggerVibration{ .Position = 0, .Amplitude = 8, .Frequency = 30 }) ==
          DualSenseTriggerPacket{ 0x26, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0x3F, 0, 0, 30, 0 });
}
