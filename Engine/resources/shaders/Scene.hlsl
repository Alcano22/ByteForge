#ifndef BYTEFORGE_SCENE_HLSL
#define BYTEFORGE_SCENE_HLSL

#define MAX_LIGHTS_2D 64

struct Light2D
{
    float4 PositionRadius;
    float4 Color;
    float4 DirectionCone;
};

cbuffer SceneUBO : register(b0)
{
    float4x4 u_ViewProjection;
    float4 u_CameraPosition;
    float4 u_LightDirection;
    float4 u_LightColor;
    float4 u_AmbientColor;

    uint4 u_Light2DCount;
    Light2D u_Lights2D[MAX_LIGHTS_2D];
};

#endif
