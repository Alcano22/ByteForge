#include "Scene.hlsl"

[[vk::binding(0, 1)]] Texture2D u_Texture;
[[vk::binding(1, 1)]] SamplerState u_TextureSampler;

static const uint UI_MODE_IMAGE = 0;

struct VSInput
{
    float2 Position : POSITION;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR;
    float4 ClipRect : CLIPRECT;
    uint   Mode     : MODE;
    uint   EntityId : ENTITYID;
};

struct VSOutput
{
    float4 Position                 : SV_Position;
    float2 UV                       : TEXCOORD0;
    float2 PixelPosition            : TEXCOORD1;
    float4 Color                    : COLOR;
    nointerpolation float4 ClipRect : CLIPRECT;
    nointerpolation uint   Mode     : MODE;
    nointerpolation uint   EntityId : ENTITYID;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position      = mul(u_ViewProjection, float4(input.Position, 0.0, 1.0));
    output.UV            = input.UV;
    output.PixelPosition = input.Position;
    output.Color         = input.Color;
    output.ClipRect      = input.ClipRect;
    output.Mode          = input.Mode;
    output.EntityId      = input.EntityId;
    return output;
}

float4 ShadeQuad(VSOutput input)
{
    if (any(input.PixelPosition < input.ClipRect.xy) || any(input.PixelPosition >= input.ClipRect.zw)) discard;

    return u_Texture.Sample(u_TextureSampler, input.UV) * input.Color;
}

#ifdef ENTITY_ID

struct PSOutput
{
    float4 Color    : SV_Target0;
    uint   EntityId : SV_Target1;
};

PSOutput PSMain(VSOutput input)
{
    PSOutput output;
    output.Color = ShadeQuad(input);
    if (output.Color.a < 0.01) discard;

    output.EntityId = input.EntityId;
    return output;
}

#else

float4 PSMain(VSOutput input) : SV_Target
{
    return ShadeQuad(input);
}

#endif
