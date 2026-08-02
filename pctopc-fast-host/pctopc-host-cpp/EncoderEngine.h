#pragma once

#include "Common.h"

#include <d3d11.h>
#include <dxgi.h>
#include <mfidl.h>					 // media foundation definition language header
#include <winrt/Windows.Graphics.Capture.h> 
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

namespace winrt_capture = winrt::Windows::Graphics::Capture;
namespace winrt_directx = winrt::Windows::Graphics::DirectX;
namespace winrt_d3d11 = winrt::Windows::Graphics::DirectX::Direct3D11;

class EncoderEngine {

private:

	winrt::com_ptr<IMFTransform> m_encoder;
	winrt::com_ptr<IMFMediaEventGenerator> m_eventGenerator;
	winrt::com_ptr<ID3D11VideoDevice> m_videoDevice;
	winrt::com_ptr<ID3D11VideoContext> m_videoContext;
	winrt::com_ptr<ID3D11VideoProcessor> m_videoProcessor;
	winrt::com_ptr<ID3D11VideoProcessorEnumerator> m_videoEnum;
	winrt::com_ptr<ID3D11Texture2D> m_nv12Texture;
	UINT64 m_frameTime{ 0 };
	UINT m_fps = 60;
	int64_t m_firstFrameTime = -1;
	int64_t m_lastTimestamp = 0;

	std::atomic<bool> s_renderPending{ false };

	void ConvertRGBtoNV12(ID3D11Texture2D* inputTexture, ID3D11Texture2D* outputTexture);
	void CleanEncodedFrame();
	void EncodeFrame(ID3D11Texture2D* texture, int64_t frameTime);

public:
	
	EncoderEngine(const EncoderEngine&) = delete;
	EncoderEngine& operator=(const EncoderEngine&) = delete;

	EncoderEngine() = default;
	~EncoderEngine() = default;

	void Init(ID3D11Device* device, ID3D11DeviceContext* deviceContext, Resolution res, UINT fps, UINT bitrate);
	void Destroy();
	void StartFrame(const winrt_capture::Direct3D11CaptureFrame& frame, ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, HWND hWindow);
	void CloseFrame(IDXGISwapChain* swapChain);
};