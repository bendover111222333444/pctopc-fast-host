#pragma once

#include "Common.h"

#include "nvEncodeAPI.h"
#include <d3d11.h>
#include <dxgi.h>
#include <winrt/Windows.Graphics.Capture.h> 
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

namespace winrt_capture = winrt::Windows::Graphics::Capture;
namespace winrt_directx = winrt::Windows::Graphics::DirectX;
namespace winrt_d3d11 = winrt::Windows::Graphics::DirectX::Direct3D11;

class EncoderEngine {

private:

	static const int m_bufferCount = 1;

	void* m_encoderSession;
	int m_currentBufferIndex;
	HMODULE m_nvencModule = nullptr;
	NV_ENCODE_API_FUNCTION_LIST m_nvenc;
	NV_ENC_OUTPUT_PTR m_bitstreamBuffers[m_bufferCount];
	winrt::com_ptr<ID3D11VideoDevice> m_videoDevice;
	winrt::com_ptr<ID3D11VideoContext> m_videoContext;
	winrt::com_ptr<ID3D11VideoProcessor> m_videoProcessor;
	winrt::com_ptr<ID3D11VideoProcessorEnumerator> m_videoEnum;
	winrt::com_ptr<ID3D11Texture2D> m_nv12Texture;
	Resolution m_res{0, 0};
	UINT64 m_frameTime{ 0 };
	UINT m_fps = 60;
	int64_t m_firstFrameTime = -1;
	int64_t m_lastTimestamp = 0;

	void ConvertRGBtoNV12(ID3D11Texture2D* inputTexture, ID3D11Texture2D* outputTexture);
	void EncodeFrame(ID3D11Texture2D* texture, int64_t frameTime);

public:
	
	EncoderEngine(const EncoderEngine&) = delete;
	EncoderEngine& operator=(const EncoderEngine&) = delete;

	EncoderEngine() = default;
	~EncoderEngine();

	void Init(ID3D11Device* device, ID3D11DeviceContext* deviceContext, Resolution res, UINT fps, UINT bitrate);
	void Destroy();
	void StartFrame(const winrt_capture::Direct3D11CaptureFrame& frame, ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, HWND hWindow);
	void CloseFrame(IDXGISwapChain* swapChain);
};