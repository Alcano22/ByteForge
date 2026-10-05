#include "Camera.hlsl"

[[vk::binding(0, 1)]] Texture2D u_Texture;
[[vk::binding(1, 1)]] SamplerState u_TextureSampler;

struct VSInput
{
    float3 Position : POSITION;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR;
    uint   EntityId : ENTITYID;
};

struct VSOutput
{
    float4 Position               : SV_Position;
    float2 UV                     : TEXCOORD0;
    float4 Color                  : COLOR;
    nointerpolation uint EntityId : ENTITYID;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(u_ViewProjection, float4(input.Position, 1.0));
    output.UV = input.UV;
    output.Color = input.Color;
    output.EntityId = input.EntityId;
    return output;
}

float4 ShadeQuad(VSOutput input)
{
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
