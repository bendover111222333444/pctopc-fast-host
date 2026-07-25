#pragma once

#include "Common.h"

#include <d3d11.h>
#include <dxgi.h>
#include <winrt/Windows.Graphics.Capture.h> 
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

namespace winrt_graphics = winrt::Windows::Graphics;
namespace winrt_capture = winrt::Windows::Graphics::Capture;
namespace winrt_directx = winrt::Windows::Graphics::DirectX;
namespace winrt_d3d11 = winrt::Windows::Graphics::DirectX::Direct3D11;

class CaptureEngine {

private:

	Resolution m_res{};
	HWND m_hWindow = nullptr;
	std::wstring m_appName = L"";
	winrt_graphics::SizeInt32 m_winrtSize{};
	winrt_capture::GraphicsCaptureItem m_captureItem = nullptr;
	winrt_capture::GraphicsCaptureSession m_captureSession = nullptr;
	winrt_capture::Direct3D11CaptureFramePool m_framePool = nullptr;

	std::mutex m_pipelineMutex;
	bool m_isStopping = false;

public:

	CaptureEngine() = default;
	~CaptureEngine();

	CaptureEngine(const CaptureEngine&) = delete;
	CaptureEngine& operator=(const CaptureEngine&) = delete;

	void Init(HWND hWindow, UINT monitorDefault, const std::wstring& appName);
	void Start(ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, IDXGISwapChain* swapChain, winrt_d3d11::IDirect3DDevice winrtDevice, UINT bufferCount);
	void Destroy();
	Resolution GetResolution() const { return m_res; }

};