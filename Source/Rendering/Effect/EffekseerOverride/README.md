Effekseer DX11 private API headers, matching the bundled SDK (MIT).
Source: https://github.com/effekseer/Effekseer/tree/effd528a5ae5dbc60531c03e290db33e187f815b/Dev/Cpp/EffekseerRendererDX11/EffekseerRendererDX11

Shader.h only changes its includes to GraphicsDevice.h; the Shader class is unchanged.
These declarations let EffectManager construct replacement shader objects using the
existing library, rather than temporary overrides that GetShader(Unlit) clears.
