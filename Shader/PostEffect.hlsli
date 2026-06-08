// PostEffect.hlsli

cbuffer CbPostEffect : register(b0)
{
    float luminanceExtractionLowerEdge;
    float luminanceExtractionHigherEdge;
    float gaussianSigma;
    float bloomIntensity;
    float saturation; // ç ìx
    float exposure; // òIèo
    float2 dummy_cbposteffect;
};