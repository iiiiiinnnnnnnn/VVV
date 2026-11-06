// BasicSpriteVS.hlsl

#include "BasicSprite.hlsli"

VS_OUT main(float4 position : POSITION, float2 texcoord : TEXCOORD, float alpha : COLOR0)
{
	VS_OUT vout;
	vout.position = position;
	vout.texcoord = texcoord;
	vout.alpha = alpha;
	return vout;
}