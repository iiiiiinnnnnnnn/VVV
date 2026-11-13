#ifndef __SCENE_HLSLI__
#define __SCENE_HLSLI__

struct CbDirectionalLight
{
    float3 direction;
    float _dummyCbDirectionalLight;
    
    float4 color;
};

struct CbPointLight
{
    float3 position;
    float range;
    
    float4 color;
};

struct CbSpotLight
{
    float3 position;
    float _dummyCbSpotLight1;
    
    float3 direction;
    float _dummyCbSpotLight2;
    
    float4 color;
    
    float range;
    float innerConeAngle;
    float outerConeAngle;
    float _dummyCbSpotLight3;
};

struct CbAreaLight
{
    float3 position;
    float width;
    
    float3 direction; // 法線
    float height;
    
    float3 right; // 矩形X軸
    float range;
    
    float4 color;
};

#define MaxLights 150
#define MaxDirectionalLights MaxLights
#define MaxPointLights MaxLights
#define MaxSpotLights MaxLights
#define MaxAreaLights MaxLights
struct CbLightData
{
    CbDirectionalLight directionalLights[MaxDirectionalLights];
    CbPointLight pointLights[MaxPointLights];
    CbSpotLight spotLights[MaxSpotLights];
    CbAreaLight areaLights[MaxAreaLights];
    float4 ambientColor;
    int pointLightCount;
    int spotLightCount;
    int areaLightCount;
    int directionalLightCount;
};

struct CbTimeData
{
    float time; // 起動してからの経過時間
    float scale; // 時間のスケール
    float deltaTime; // 前フレームからの経過時間
    float unscaledDeltaTime; // 時間スケールの影響を受けない経過時間
};

struct CbFogData
{
    float4 distanceFogColor;
    float4 distanceFogParams;
};

cbuffer CbScene : register(b7)
{
    row_major float4x4 viewProjection;
    float3 viewPosition;
    float previewShadingMode;
    CbLightData lightData;
    CbFogData cbFogData;
    CbTimeData timeData;
};

#endif