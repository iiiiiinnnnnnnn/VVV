#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Common.h"

class ShapeRenderer
{
public:
	ShapeRenderer(ID3D11Device* device);
	~ShapeRenderer() {}

	// 箱描画
	void DrawBox(
		const Vector3& position,
		const Vector3& angle,
		const Vector3& size,
		const Color& color);

	// 球描画
	void DrawSphere(
		const Vector3& position,
		float radius,
		const Color& color);

	// カプセル描画
	void DrawCapsule(
		const Matrix& transform,
		float radius,
		float height,
		const Color& color);

	// 骨描画
	void DrawBone(
		const Matrix& transform,
		float length,
		const Color& color);

	// 描画実行
	void Render(
		ID3D11DeviceContext* dc,
		const Matrix& view,
		const Matrix& projection);

private:
	struct Mesh
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
		UINT									vertexCount;
	};

	struct Instance
	{
		Mesh*		mesh;
		Matrix		worldTransform;
		Color		color;
	};

	struct CbMesh
	{
		Matrix		worldViewProjection;
		Color		color;
	};

	// メッシュ生成
	void CreateMesh(ID3D11Device* device, const std::vector<Vector3>& vertices, Mesh& mesh);

	// 箱メッシュ作成
	void CreateBoxMesh(ID3D11Device* device, float width, float height, float depth);

	// 球メッシュ作成
	void CreateSphereMesh(ID3D11Device* device, float radius, int subdivisions);

	// 半球メッシュ作成
	void CreateHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions);

	// 円柱
	void CreateCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions);

	// 骨メッシュ作成
	void CreateBoneMesh(ID3D11Device* device, float length);

private:
	Mesh										boxMesh;
	Mesh										sphereMesh;
	Mesh										halfSphereMesh;
	Mesh										cylinderMesh;
	Mesh										boneMesh;
	std::vector<Instance>						instances;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>		constantBuffer;
};
