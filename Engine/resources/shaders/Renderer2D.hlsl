#include "Scene.hlsl"

[[vk::binding(0, 1)]] Texture2D u_Texture;
[[vk::binding(1, 1)]] SamplerState u_TextureSampler;

[[vk::binding(2, 1)]] cbuffer MaterialUBO
{
    float4 u_Tint;
};

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
    float2 WorldPosition          : TEXCOORD1;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position      = mul(u_ViewProjection, float4(input.Position, 1.0));
    output.UV            = input.UV;
    output.Color         = input.Color;
    output.EntityId      = input.EntityId;
    output.WorldPosition = input.Position.xy;
    return output;
}

#ifdef LIT

float3 LightAt(float2 position)
{
    float3 light = u_AmbientColor.rgb;

    const uint count = min(u_Light2DCount.x, MAX_LIGHTS_2D);
    for (uint i = 0; i < count; ++i)
    {
        const Light2D source = u_Lights2D[i];

        const float2 offset = position - source.PositionRadius.xy;
        const float distanceToLight = length(offset);

        float attenuation = 1.0 - smoothstep(source.PositionRadius.w, source.PositionRadius.z, distanceToLight);
        const float2 direction = distanceToLight > 1e-5 ? offset / distanceToLight : source.DirectionCone.xy;
        attenuation *= smoothstep(source.DirectionCone.z, source.DirectionCone.w, dot(direction, source.DirectionCone.xy));

        light += source.Color.rgb * attenuation;
    }

    return light;
}

#endif

float4 ShadeQuad(VSOutput input)
{
    float4 color = u_Texture.Sample(u_TextureSampler, input.UV) * input.Color * u_Tint;

#ifdef LIT
    color.rgb *= LightAt(input.WorldPosition);
#endif

    return color;
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
