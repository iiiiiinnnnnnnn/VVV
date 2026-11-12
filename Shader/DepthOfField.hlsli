#include "FullScreenQuad.hlsli"

cbuffer CbDepthOfField : register(b2)
{
    row_major float4x4 inverseProjection;
    float focusDistance;
    float focusRange;
    float nearStrength;
    float farStrength;
    float2 blurStep;
    float2 padding;
};
