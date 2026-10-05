#pragma once

cbuffer CameraUBO : register(b0)
{
    float4x4 u_ViewProjection;
};
