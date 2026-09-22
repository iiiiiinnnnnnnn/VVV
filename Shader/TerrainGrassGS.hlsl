#include "TerrainGrass.hlsli"

[maxvertexcount(3)]
void main(point GrassGSInput input[1], inout TriangleStream<GrassPSInput> stream)
{
	// 地形変形やペイントで消された場所は草を展開しない
	const float2 terrainUV = input[0].position.xz / terrainSize + 0.5f;
	const float grassMask = terrainDataMap.SampleLevel(
		terrainPointSampler, terrainUV, 0.0f).b;
	if (grassMask <= 0.0001f)
	{
		return;
	}

    // Terrainのローカル座標から、草の根元となるワールド座標を求める
    const float3 base = mul(float4(input[0].position, 1.0f), world).xyz;

    // 地形の法線を草の上方向として斜面に沿わせる
    const float3 up = normalize(mul(float4(input[0].normal, 0.0f), world).xyz);
    float3 referenceAxis = float3(0.0f, 0.0f, 1.0f);
    if (abs(dot(referenceAxis, up)) > 0.95f)
    {
        referenceAxis = float3(1.0f, 0.0f, 0.0f);
    }
    const float3 surfaceRight = normalize(cross(referenceAxis, up));
    const float3 surfaceForward = normalize(cross(up, surfaceRight));

    // 遠くの草はポリゴンを作らず、ここで処理を終える
    const float distanceToCamera = distance(base.xz, cameraPosition.xz);
    if (distanceToCamera > drawDistance)
    {
        return;
    }

    // 0～1の乱数を使い、株ごとの大きさと向きを変える
    const float random = input[0].random;

    const float minimumSize = 1.0f - sizeVariation;
    const float maximumSize = 1.0f + sizeVariation;
    const float sizeRange = maximumSize - minimumSize;
    const float sizeScale = minimumSize + sizeRange * random;

    const float cardWidth = width * sizeScale;
    const float cardHeight = height * sizeScale;
    const float fullRotation = 6.2831853f;
    const float angle = random * fullRotation;

    // 時間と位置を位相に使うことで、全ての草が同時に揺れるのを防ぐ
    const float timePhase = time * windSpeed;
    const float positionPhaseX = base.x * 0.17f;
    const float positionPhaseZ = base.z * 0.13f;
    const float windPhase = timePhase + positionPhaseX + positionPhaseZ;
    const float windX = sin(windPhase) * windStrength;
    const float windZ = cos(windPhase * 0.83f) * windStrength;
    const float3 worldWindOffset = float3(windX, 0.0f, windZ);
    const float windAlongNormal = dot(worldWindOffset, up);
    const float3 windOffset = worldWindOffset - up * windAlongNormal;

    // 1本の草を根元2点と先端1点だけで作る
    const float angleCos = cos(angle);
    const float angleSin = sin(angle);
    const float3 bladeRight = surfaceRight * angleCos + surfaceForward * angleSin;
    const float halfBladeWidth = cardWidth * 0.5f;
    const float3 tipCenter = base + up * cardHeight + windOffset;
    const float3 positions[3] =
    {
        base - bladeRight * halfBladeWidth,
        base + bladeRight * halfBladeWidth,
        tipCenter
    };
    [unroll]
    for (int vertex = 0; vertex < 3; ++vertex)
    {
        GrassPSInput output;
        output.position = mul(float4(positions[vertex], 1.0f), viewProjection);
        output.worldPosition = positions[vertex];
        const float minimumBrightness = 0.68f;
        const float brightnessRange = 0.32f;
        output.shade = minimumBrightness + random * brightnessRange;
        stream.Append(output);
    }
    stream.RestartStrip();
}
