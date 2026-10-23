cbuffer EffectFogConstants : register(b13)
{
	row_major float4x4 effectInverseViewProjection;
	float4 effectCameraPosition;
	float4 effectViewport; // xy: origin, zw: inverse size
	float4 effectFogParams; // start, end, strength, enabled
};

float4 ApplyEffectFog(float4 color, float4 position)
{
	if (effectFogParams.w < 0.5f) return color;
	float2 uv = (position.xy - effectViewport.xy) * effectViewport.zw;
	float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 world = mul(float4(ndc, position.z, 1.0f), effectInverseViewProjection);
	float distanceFromCamera = distance(world.xyz / world.w, effectCameraPosition.xyz);
	float end = max(effectFogParams.y, effectFogParams.x + 0.001f);
	float fog = smoothstep(effectFogParams.x, end, distanceFromCamera);
	fog *= fog * saturate(effectFogParams.z);
	// 発光を含む元のブレンドを保ち、距離に応じて背景へなじませる。
	color.a *= 1.0f - fog;
	return color;
}
