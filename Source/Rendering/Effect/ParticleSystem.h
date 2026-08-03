// ParticleSystem.h

#pragma once

#include "Core/Foundation/Common.h"

#include <vector>

#include "Resource/Texture.h"
#include "Rendering/Shader/Shader.h"

#include "Core/Object/Component.h"
#include "Rendering/Core/RenderContext.h"

class ParticleSystem
{
private:
	struct Vertex
	{
		Vector3 position;	//	位置
		Vector2 texcoord;	//	UV
		Color color;		//	頂点色		
		Vector4 param;		//	汎用パラメータ
	};

	struct ParticleData
	{
		float x, y, z;
		float w, h;
		float aw, ah;
		float vx, vy, vz;
		float ax, ay, az;
		float alpha;
		float initialTimer;
		float fadeInDuration;
		float fadeOutDuration;
		Color color;
		float timer;
		float animeTimer;
		float type;

		bool anime;
		float animeSpeed;
	};

	//	定数バッファのデータ定義
	struct Constants
	{
		Matrix viewProjection;
		Vector3 cameraRight;
		float dummy0;
		Vector3 cameraUp;
		float dummy1;
	};

public:
	ParticleSystem(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView, int komax = 1, int komay = 1, int num = 1000);

	~ParticleSystem();
	void Update();

	void Render(const RenderContext& rc);

	void Set(
		int type,
		float timer,
		Vector3 p,
		Vector3 v = Vector3(0.0f, 0.0f, 0.0f),
		Vector3 f = Vector3(0.0f, 0.0f, 0.0f),
		Vector2 size = Vector2(1.0f, 1.0f),
		bool anime = false,
		float animeSpeed = 24.0f,
		Color color = Color(0.35f, 0.9f, 1.0f, 1.0f),
		float fadeInDuration = 0.0f,
		float fadeOutDuration = 0.0f
	);

private:
	ParticleData* data;	//	パーティクル情報
	Vertex* vertices;		//	頂点バッファ書き込み情報
	int numParticles = 0;	//	パーティクル数
	int komax, komay;		//	Textureの縦横分割数

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometryShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
};
