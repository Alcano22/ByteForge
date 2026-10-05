#ifndef BYTEFORGE_SCENE_HLSL
#define BYTEFORGE_SCENE_HLSL

cbuffer SceneUBO : register(b0)
{
    float4x4 u_ViewProjection;
    float4 u_CameraPosition;
    float4 u_LightDirection;
    float4 u_LightColor;
    float4 u_AmbientColor;
};

#endif
