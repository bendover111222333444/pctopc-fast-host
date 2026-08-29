#include "CaptureEngine.h"
#include "Debug.h"

#include <dxgi1_2.h>
#include <wrl/client.h>
#include <winrt/Windows.Foundation.h>
#include <windows.graphics.capture.interop.h> 
#include <windows.graphics.directx.direct3d11.interop.h> 

#pragma comment(lib, "windowsapp.lib")

CaptureEngine::~CaptureEngine() { Destroy(); }

void CaptureEngine::Init(HWND hWindow, UINT monitorDefault,const std::wstring& appName)
{

	m_hWindow = hWindow;
	m_appName = appName;
	m_winrtSize = { 0, 0 };

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	HMONITOR monitor = MonitorFromWindow(m_hWindow, monitorDefault);																	// creates monitor object

	auto activationFactory = winrt::get_activation_factory<winrt_capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();		// bridges the gap between old win32 to winrt for the monitor object
	winrt::check_hresult(activationFactory->CreateForMonitor(																		// activates the monitor on winrt uses check_hresult to prevent crash
		monitor,																													// main monitor handle
		winrt::guid_of<winrt_capture::GraphicsCaptureItem>(),																		// gets the windows id of the object
		winrt::put_abi(m_captureItem)																								// gives underlying pointer
	));

	MONITORINFO monInfo = {
		.cbSize = sizeof(MONITORINFO),																								// prevent deprecation issues
	};

	GetMonitorInfoW(monitor, &monInfo);																								// gets monitor info & references monitor info

	m_res.width = monInfo.rcMonitor.right - monInfo.rcMonitor.left;																		// cal width
	m_res.height = monInfo.rcMonitor.bottom - monInfo.rcMonitor.top;																	// cal hight

	m_winrtSize = {
		.Width = static_cast<int32_t>(m_res.width),																						// makes insertable width and height
		.Height = static_cast<int32_t>(m_res.height),																					// makes insertable width and height
	};

}

void CaptureEngine::StartKeepWGCBusy() {

    std::wstring appName = L"alive overlay";
    UINT bufferCount = 2;
    Resolution startingSize = {
        .width = 1,
        .height = 1,
    };

    WNDCLASSEXW dka_wc = {};
    dka_wc.cbSize = sizeof(dka_wc);
    dka_wc.lpfnWndProc = DefWindowProcW;
    dka_wc.hInstance = GetModuleHandleW(nullptr);
    dka_wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    dka_wc.lpszClassName = appName.c_str(),
    RegisterClassExW(&dka_wc);

    HWND dka_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        appName.c_str(),
        appName.c_str(),
        WS_POPUP,
        0, 0, startingSize.width, startingSize.height,
        nullptr, nullptr, dka_wc.hInstance, nullptr
    );

    ShowWindow(dka_hwnd, SW_SHOW);
    SetWindowPos(dka_hwnd, HWND_TOPMOST, 0, 0, 1, 1, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    Microsoft::WRL::ComPtr<ID3D11Device> dka_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> dka_context;
    D3D_FEATURE_LEVEL dka_featureLevel;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, dka_device.ReleaseAndGetAddressOf(), &dka_featureLevel, dka_context.ReleaseAndGetAddressOf());

    Microsoft::WRL::ComPtr<IDXGIDevice> dka_dxgiDevice;
    dka_device.As(&dka_dxgiDevice);
    Microsoft::WRL::ComPtr<IDXGIAdapter> dka_adapter;
    dka_dxgiDevice->GetAdapter(&dka_adapter);
    Microsoft::WRL::ComPtr<IDXGIFactory2> dka_factory;
    dka_adapter->GetParent(__uuidof(IDXGIFactory2), &dka_factory);

    DXGI_SWAP_CHAIN_DESC1 dka_scDesc = {};
    dka_scDesc.Width = 1;
    dka_scDesc.Height = 1;
    dka_scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    dka_scDesc.SampleDesc.Count = 1;
    dka_scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    dka_scDesc.BufferCount = 2;
    dka_scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> dka_swapChain;
    dka_factory->CreateSwapChainForHwnd(dka_device.Get(), dka_hwnd, &dka_scDesc, nullptr, nullptr, dka_swapChain.ReleaseAndGetAddressOf());

    Microsoft::WRL::ComPtr<ID3D11Texture2D> dka_backBuffer;
    dka_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &dka_backBuffer);
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> dka_rtv;
    dka_device->CreateRenderTargetView(dka_backBuffer.Get(), nullptr, &dka_rtv);

    float dka_clearColor[4] = { 0.f, 0.f, 0.f, 0.f };

    int dka_reassertCounter = 0;
    float dka_hue = 0.f;

    while (!m_isStopping) {

        MSG msg;
        while (PeekMessage(&msg, dka_hwnd, 0, 0, PM_REMOVE)) {

            TranslateMessage(&msg);
            DispatchMessage(&msg);

        }

        if (dka_reassertCounter++ >= 30) {

            SetWindowPos(dka_hwnd, HWND_TOPMOST, 0, 0, 1, 1, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            dka_reassertCounter = 0;

        }

        dka_hue += 0.01f;
        if (dka_hue > 1.f) dka_hue = 0.f;

        dka_clearColor[0] = dka_hue;
        dka_clearColor[1] = 0.2f;
        dka_clearColor[2] = 1.f - dka_hue;
        dka_clearColor[3] = 1.f;

        dka_context->ClearRenderTargetView(dka_rtv.Get(), dka_clearColor);
        dka_swapChain->Present(1, 0);

    }

    DestroyWindow(dka_hwnd);

}

void CaptureEngine::Start(EncoderEngine* encoderEngine, ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, winrt_d3d11::IDirect3DDevice winrtDevice, UINT bufferCount)
{
	
	m_framePool = winrt_capture::Direct3D11CaptureFramePool::CreateFreeThreaded(													// new capture thread
		winrtDevice,																												// graphics device
		winrt_directx::DirectXPixelFormat::R8G8B8A8UIntNormalized,																	// color format
		bufferCount,																												// buffer frame count
		m_winrtSize																													// width height settings
	);

	m_framePool.FrameArrived([encoderEngine, context, backBuffer, this](auto const& sender, auto const& args) {

		std::lock_guard<std::mutex> lock(m_pipelineMutex);																			// fixes the parrelle thread issues
		if (m_isStopping) return;

		Debug::LogFPS();

		auto frame = sender.TryGetNextFrame();														// fetch frame																							
		encoderEngine->StartFrame(frame, context, backBuffer, m_hWindow);

	});

    std::thread dka_thread([this]() {   // another loop to update pixels so wgc captures

        StartKeepWGCBusy();

    });

    dka_thread.detach();

	m_captureSession = m_framePool.CreateCaptureSession(m_captureItem);																// creates the session now that its time
	m_captureSession.IsBorderRequired(false);																						// removes border
	m_captureSession.StartCapture();																								// starts capturing
}

void CaptureEngine::Destroy() {

	std::lock_guard<std::mutex> lock(m_pipelineMutex); // stop gaurd
	m_isStopping = true;

	if (m_captureSession) {
		m_captureSession.Close();		// cleans up
		m_captureSession = nullptr;		// cleans up here too
	}

	if (m_framePool) {
		m_framePool.Close();			// same here
		m_framePool = nullptr;			// same here
	}

}