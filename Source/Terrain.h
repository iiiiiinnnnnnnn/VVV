// Terrain.h

#pragma once

#include "Component.h"
#include "Model.h"
#include "Transform.h"

class Terrain : public Component
{
public:
	Terrain(Object* owner);
	~Terrain();

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;

private:
	//	テクスチャ書き込み情報
	bool	use_brush{true};
	int		brush_size{50};	//	ブラシサイズ
	Color	brush_color{1, 1, 1, 0.005f};
	float	brush_intensity{10};
	Microsoft::WRL::ComPtr<ID3D11BlendState> brush_blend_states;

	//	地形関係
	bool		is_terrain_texture_clear_color{true};
	Quaternion	terrain_texture_clear_color{0, 0, 0, 1};
	Microsoft::WRL::ComPtr<ID3D11Texture2D> terrain_texture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> terrain_texture_render_target_view;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrain_texture_shader_resource_view;

	Model* model;
	Transform modelTransform;

	// テッセレーションの定数バッファ
	struct tesselation_constants
	{
		float	edge_factor{64};		//	エッジ分割数
		float	inner_factor{64};		//	内部分割数
		float	height_scaler{+0.5f};	//	高さ係数
		float	tilling_scale{1.0f};	//	タイリング係数
	};
	bool	use_wire{false};
	tesselation_constants tesselation_constant;
	Microsoft::WRL::ComPtr<ID3D11Buffer> tesselation_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> terrain_primitive_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> terrain_primitive_input_layout;
	Microsoft::WRL::ComPtr<ID3D11HullShader> terrain_primitive_hull_shader;
	Microsoft::WRL::ComPtr<ID3D11DomainShader> terrain_primitive_domain_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> terrain_primitive_pixel_shader;

	//	地形用テクスチャ
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrain_base_color_shader_resource_view[3];
};