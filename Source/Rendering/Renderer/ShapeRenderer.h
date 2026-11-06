#pragma once
#include <memory>
#include "Rendering/Core/VertexBuffer.h"
#include "Rendering/Core/ConstantBuffer.h"
#include "Rendering/Core/PixelShader.h"
#include "Rendering/Core/VertexShader.h"
#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Core/Foundation/Common.h"

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
		std::unique_ptr<Gpu::VertexBuffer> vertexBuffer;
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
	std::unique_ptr<Gpu::VertexShader> vertexShader;
	std::unique_ptr<Gpu::PixelShader> pixelShader;
	std::unique_ptr<Gpu::ConstantBuffer> constantBuffer;
};
