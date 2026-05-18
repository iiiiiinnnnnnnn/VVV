
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

struct CbLightManager
{
    CbDirectionalLight directionalLight;
    CbPointLight pointLight;
    CbSpotLight spotLight;
    float4 ambientColor;
    unsigned int pointLightCount;
    unsigned int spotLightCount;
    float _dummyCbLightManager[2];
};

cbuffer CbScene : register(b7)
{
    row_major float4x4  viewProjection;
    float3              viewPosition;
    float _dummyCbScene;
    CbLightManager      lightManager;
};
