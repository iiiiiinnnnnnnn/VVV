#pragma once

#include "Widget.h"
#include "GaussianFilterShader.h"

class TestWidget : public Widget
{
public:
    TestWidget(Vector2 pos);
    
    void OnUpdate(float elapsedTime) override;
    void OnRender(float elapsedTime) override;
	void OnDrawGUI(float elapsedTime) override;

    void SetGaussianKernelSize(int size) { gaussianParams.kernel_size = size; }
    void SetGaussianSigma(float sigma) { gaussianParams.sigma = sigma; }

	std::shared_ptr<Texture> texture = nullptr;

private:
    GaussianFilterShader::GaussianFilterData gaussianParams;
};