#include "Scene.hlsl"

struct DrawData
{
    float4x4 Model;
    float4 Color;
};

[[vk::push_constant]] DrawData u_Draw;

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR;
};

float3 TransformNormal(float4x4 model, float3 normal)
{
    const float3 c0 = model._m00_m10_m20;
    const float3 c1 = model._m01_m11_m21;
    const float3 c2 = model._m02_m12_m22;

    const float3 cofactor = cross(c1, c2) * normal.x + cross(c2, c0) * normal.y + cross(c0, c1) * normal.z;
    return dot(c0, cross(c1, c2)) < 0.0 ? -cofactor : cofactor;
}

VSOutput VSMain(VSInput input)
{
    const float4 world = mul(u_Draw.Model, float4(input.Position, 1.0));

    VSOutput output;
    output.Position = mul(u_ViewProjection, world);
    output.Normal = TransformNormal(u_Draw.Model, input.Normal);
    output.Color = u_Draw.Color;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    const float3 normal = normalize(input.Normal);
    const float diffuse = saturate(dot(normal, -u_LightDirection.xyz));
    const float3 lighting = u_AmbientColor.rgb + u_LightColor.rgb * diffuse;

    return float4(input.Color.rgb * lighting, input.Color.a);
}
