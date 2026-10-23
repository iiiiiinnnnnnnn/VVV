#define main EffekseerMain
#include "Original/model_unlit_ps.fx"
#undef main
#include "EffekseerFog.hlsli"

float4 main(const PS_Input input) : SV_TARGET
{
	return ApplyEffectFog(EffekseerMain(input), input.PosVS);
}
