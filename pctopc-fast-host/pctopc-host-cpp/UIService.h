#pragma once

#include "Common.h"
#include <d3d11.h>

namespace UIService {

    void RenderUI();
    void ResizeUI(Resolution res);
    void InitUI(HWND hWindow, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
    void ShutDownUI();
    void InitFrameUI();
    void CloseFrameUI(ID3D11RenderTargetView* renderTargetView, ID3D11DeviceContext* deviceContext);

}