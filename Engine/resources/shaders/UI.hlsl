#include "Scene.hlsl"

[[vk::binding(0, 1)]] Texture2D u_Texture;
[[vk::binding(1, 1)]] SamplerState u_TextureSampler;

static const uint UI_MODE_IMAGE = 0;
static const uint UI_MODE_TEXT  = 1;

struct VSInput
{
    float2 Position      : POSITION;
    float2 UV            : TEXCOORD0;
    float4 Color         : COLOR;
    float4 ClipRect      : CLIPRECT;
    uint   Mode          : MODE;
    uint   EntityId      : ENTITYID;
    float  DistanceRange : DISTANCERANGE;
    float  Weight        : WEIGHT;
};

struct VSOutput
{
    float4 Position                      : SV_Position;
    float2 UV                            : TEXCOORD0;
    float2 PixelPosition                 : TEXCOORD1;
    float4 Color                         : COLOR;
    nointerpolation float4 ClipRect      : CLIPRECT;
    nointerpolation uint   Mode          : MODE;
    nointerpolation uint   EntityId      : ENTITYID;
    nointerpolation float  DistanceRange : DISTANCERANGE;
    nointerpolation float  Weight        : WEIGHT;
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
    output.DistanceRange = input.DistanceRange;
    output.Weight        = input.Weight;
    return output;
}

float Median(float3 value)
{
    return max(min(value.r, value.g), min(max(value.r, value.g), value.b));
}

float ScreenPixelRange(float2 uvPerPixel, float distanceRange)
{
    float2 atlasSize;
    u_Texture.GetDimensions(atlasSize.x, atlasSize.y);

    const float2 unitRange = distanceRange / atlasSize;
    const float2 screenTextureSize = 1.0 / max(uvPerPixel, 1e-6);
    return max(0.5 * dot(unitRange, screenTextureSize), 1.0);
}

float4 ShadeQuad(VSOutput input)
{
    const float2 uvDx = ddx(input.UV);
    const float2 uvDy = ddy(input.UV);
    const float2 uvPerPixel = abs(uvDx) + abs(uvDy);

    if (any(input.PixelPosition < input.ClipRect.xy) || any(input.PixelPosition >= input.ClipRect.zw)) discard;

    const float4 texel = u_Texture.SampleGrad(u_TextureSampler, input.UV, uvDx, uvDy);

    if (input.Mode == UI_MODE_TEXT)
    {
        const float field = Median(texel.rgb) - 0.5 + input.Weight;
        const float distance = ScreenPixelRange(uvPerPixel, input.DistanceRange) * field;
        const float coverage = saturate(distance + 0.5);
        return float4(input.Color.rgb, input.Color.a * coverage);
    }

    return texel * input.Color;
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
