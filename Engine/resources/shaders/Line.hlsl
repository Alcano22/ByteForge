#include "Camera.hlsl"

struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(u_ViewProjection, float4(input.Position, 1.0));
    output.Color = input.Color;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    return input.Color;
}
