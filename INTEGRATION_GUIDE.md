# VVV への PBR + ShadowMap 移植ガイド

## 追加ファイル一覧

### Shader/
| ファイル | 役割 |
|---|---|
| `ShadowmapFunctions.hlsli` | CalcShadowTexcoord / CalcShadowColorPCFFilter |
| `ShadowmapCaster.hlsli`    | キャスター用 b9 定数バッファ定義 |
| `ShadowmapCasterVS.hlsl`   | シャドウマップキャスター頂点シェーダー |
| `PBR.hlsli`                | PBR 関数・IBL 関数・VS_OUT・定数バッファ定義 |
| `PBRVS.hlsl`               | PBR 頂点シェーダー |
| `PBRPS.hlsl`               | PBR ピクセルシェーダー |

### Source/
| ファイル | 役割 |
|---|---|
| `PBRShader.h / .cpp`              | PBR シェーダー C++ クラス |
| `ShadowmapCasterShader.h / .cpp`  | キャスターシェーダー C++ クラス |

---

## 定数バッファ・スロット対応表

| スロット | バッファ | セット側 |
|---|---|---|
| VS b6 | CbSkeleton (骨行列256本) | ModelRenderer (既存) |
| VS/PS b7 | CbScene (VP行列・ライト) | ModelRenderer (既存) |
| VS/PS b8 | CbMesh (materialColor) | PBRShader |
| VS/PS b9 | CbShadowmap / CbShadowmapCaster | PBRShader / CasterShader |

---

## テクスチャスロット対応表 (PS)

| スロット | テクスチャ |
|---|---|
| t0 | albedo (baseMap) |
| t1 | normalMap |
| t2 | metalnessRoughnessMap (G=roughness, B=metalness / glTF準拠) |
| t3 | occlusionMap |
| t8 | shadowMap |
| t17 | specular_pmrem.dds |
| t18 | diffuse_iem.dds |
| t19 | lut_ggx.dds |

---

## サンプラースロット

| スロット | サンプラー | セット側 |
|---|---|---|
| s0 | LinearWrap | ModelRenderer (既存) |
| s1 | Point / Border (FLT_MAX) | PBRShader |

---

## ModelRenderer への追加手順

### 1. ShaderId に PBR を追加 (`ModelRenderer.h`)
```cpp
enum class ShaderId
{
    Basic,
    Lambert,
    PBR,              // ← 追加
    ShadowmapCaster,  // ← 追加
    EnumCount
};
```

### 2. シェーダーを生成 (`ModelRenderer.cpp`)
```cpp
#include "PBRShader.h"
#include "ShadowmapCasterShader.h"

// コンストラクタ内
shaders[static_cast<int>(ShaderId::PBR)]
    = std::make_unique<PBRShader>(device);
shaders[static_cast<int>(ShaderId::ShadowmapCaster)]
    = std::make_unique<ShadowmapCasterShader>(device);
```

---

## シャドウマップ用テクスチャ生成例 (Scene 等で一度だけ)

```cpp
// シャドウマップ (Depth-only テクスチャ)
Microsoft::WRL::ComPtr<ID3D11Texture2D>          shadowTex;
Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   shadowDSV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadowSRV;

D3D11_TEXTURE2D_DESC texDesc = {};
texDesc.Width              = 2048;
texDesc.Height             = 2048;
texDesc.MipLevels          = 1;
texDesc.ArraySize          = 1;
texDesc.Format             = DXGI_FORMAT_R24G8_TYPELESS;
texDesc.SampleDesc.Count   = 1;
texDesc.Usage              = D3D11_USAGE_DEFAULT;
texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
device->CreateTexture2D(&texDesc, nullptr, shadowTex.GetAddressOf());

D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
dsvDesc.Format        = DXGI_FORMAT_D24_UNORM_S8_UINT;
dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
device->CreateDepthStencilView(shadowTex.Get(), &dsvDesc, shadowDSV.GetAddressOf());

D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.Format                    = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
srvDesc.Texture2D.MipLevels       = 1;
device->CreateShaderResourceView(shadowTex.Get(), &srvDesc, shadowSRV.GetAddressOf());
```

---

## IBL テクスチャ読み込み例

```cpp
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specularPMREM, diffuseIEM, lutGGX;

GpuResourceUtils::LoadTexture(device, "Data/Image/specular_pmrem.dds",
    specularPMREM.GetAddressOf());
GpuResourceUtils::LoadTexture(device, "Data/Image/diffuse_iem.dds",
    diffuseIEM.GetAddressOf());
GpuResourceUtils::LoadTexture(device, "Data/Image/lut_ggx.dds",
    lutGGX.GetAddressOf());
```

---

## 1フレームの描画フロー

```cpp
// ① シャドウマップパス
{
    // ライトビュープロジェクションを計算
    DirectX::XMFLOAT4X4 lvp = CalcLightViewProjection(...);

    auto* caster = static_cast<ShadowmapCasterShader*>(
        modelRenderer->GetShader(ShaderId::ShadowmapCaster));
    caster->SetLightViewProjection(lvp);

    // RTV なし・DSV だけバインド
    ID3D11RenderTargetView* nullRTV = nullptr;
    dc->OMSetRenderTargets(1, &nullRTV, shadowDSV.Get());
    dc->ClearDepthStencilView(shadowDSV.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    D3D11_VIEWPORT vp = { 0,0,2048,2048,0,1 };
    dc->RSSetViewports(1, &vp);

    modelRenderer->Draw(ShaderId::ShadowmapCaster, myModel);
    modelRenderer->Render(rc);
}

// ② 通常描画パス
{
    graphics.SetRenderTargets();

    auto* pbr = static_cast<PBRShader*>(
        modelRenderer->GetShader(ShaderId::PBR));

    PBRShader::ShadowmapData smData;
    smData.lightViewProjection = lvp;
    smData.shadowColor         = { 0.5f, 0.5f, 0.5f };
    smData.shadowBias          = 0.001f;
    smData.PCFKernelSize       = 5;
    smData.shadowMapSRV        = shadowSRV.Get();
    pbr->SetShadowmapData(smData);

    PBRShader::IBLData ibl;
    ibl.specularPMREM = specularPMREM.Get();
    ibl.diffuseIEM    = diffuseIEM.Get();
    ibl.lutGGX        = lutGGX.Get();
    pbr->SetIBLData(ibl);

    modelRenderer->Draw(ShaderId::PBR, myModel);
    modelRenderer->Render(rc);
}
```

---

## 注意事項

### ambient について
PBRPS.hlsl 内では `float3 ambient = float3(1,1,1)` で固定しています。
VVV の `CbScene` には `ambientLightColor` がないためです。
調整したい場合は `CbMesh (b8)` に `ambientColor` を追加し、
シェーダーと C++ 両方を修正してください。

### Model::Material のフィールド名
`PBRShader::Update` 内で以下のフィールドを参照します。
VVV の `Model::Material` と名前が異なる場合は適宜読み替えてください:

| コード内 | Model.h 実際 |
|---|---|
| `mesh.material->baseColor` | `Color baseColor` |
| `mesh.material->baseMap` | `ComPtr<...> baseMap` |
| `mesh.material->normalMap` | `ComPtr<...> normalMap` |
| `mesh.material->metalnessRoughnessMap` | `ComPtr<...> metalnessRoughnessMap` |
| `mesh.material->occlusionMap` | `ComPtr<...> occlusionMap` |

### CSO ファイルパス
シェーダーのコンパイル後の `.cso` は `Data/Shader/` に配置することを前提にしています。
プロジェクト設定でコンパイル先が異なる場合は `PBRShader.cpp` / `ShadowmapCasterShader.cpp` 内のパスを修正してください。
