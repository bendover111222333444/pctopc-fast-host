#pragma once

#include "Common.h" 
#include <d3d11.h>
#include <d3d11_4.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

namespace winrt_d3d11 = winrt::Windows::Graphics::DirectX::Direct3D11;

class GraphicsEngine {
private:
	Resolution m_res{};
	winrt::com_ptr<IDXGISwapChain> m_swapChain = nullptr;
	winrt::com_ptr<ID3D11Texture2D> m_backBuffer = nullptr;
	winrt::com_ptr<ID3D11Device> m_device = nullptr;
	winrt::com_ptr<ID3D11DeviceContext> m_deviceContext = nullptr;
	winrt::com_ptr<ID3D11RenderTargetView> m_renderTargetView = nullptr;
	winrt_d3d11::IDirect3DDevice m_finalDevice = nullptr;

public:

	GraphicsEngine(const GraphicsEngine&) = delete;					// anti copy
	GraphicsEngine& operator=(const GraphicsEngine&) = delete;

	GraphicsEngine() = default;
	~GraphicsEngine() = default;
	
	void Init(HWND hWindow, Resolution startRes, UINT fps, UINT bufferCount);

	winrt_d3d11::IDirect3DDevice GetWinRTDevice() const { return m_finalDevice; }
	ID3D11Device* GetDevice() const { return m_device.get(); }
	ID3D11DeviceContext* GetContext() const { return m_deviceContext.get(); }
	ID3D11Texture2D* GetBackBuffer() const { return m_backBuffer.get(); }
	IDXGISwapChain* GetSwapChain() const { return m_swapChain.get(); }
	ID3D11RenderTargetView* GetRenderTarget() const { return m_renderTargetView.get(); }

};