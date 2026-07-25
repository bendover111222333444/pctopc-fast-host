#include "GraphicsEngine.h" 
#include <windows.graphics.directx.direct3d11.interop.h>

#pragma comment(lib, "d3d11.lib")

GraphicsEngine::~GraphicsEngine() { Destroy(); };

void GraphicsEngine::Init(HWND hWindow, UIManager* uiManager, Resolution startRes, UINT fps, UINT bufferCount) {

	m_res = startRes;
	m_uiManager = uiManager;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {
		.BufferDesc {
			.Width = m_res.width,						// self explainatory
			.Height = m_res.height,						// self explainatory
			.RefreshRate {
				.Numerator = fps,						// fps for fps/1 fps
				.Denominator = 1,						// 1 for fps/1 fps
			},
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,		// format for color
		},
		.SampleDesc {
			.Count = 1,									// sample amount for anti aliasing but useless in our case
			.Quality = 0,								// speed > quality
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT, // output type
		.BufferCount = bufferCount,						// two to allow swap effect to work
		.OutputWindow = hWindow,						// output
		.Windowed = true,								// windowed mode on
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,	// discard frames swap with close memory = faster or something
	};

	winrt::check_hresult(D3D11CreateDeviceAndSwapChain(
		nullptr,										// adapter
		D3D_DRIVER_TYPE_HARDWARE,						// driver type (hardware for direct gpu)
		nullptr,										// no software rastorizeor
		0,												// no flags
		nullptr,										// feature = highest direct x version available
		0,												// number of elements in freature array but null feature
		D3D11_SDK_VERSION,								// auto version
		&swapChainDesc,									// referebnce of my swap chain desc :)
		m_swapChain.put(),								// pointer so the function returns my device and swapchain
		m_device.put(),									// pointer so the function returns my device and swapchain
		nullptr,										// another feature thing idk why it keeps doing this
		m_deviceContext.put()							// pointer so the function returns my device and swapchain
	));

	winrt::check_hresult(m_swapChain->GetBuffer(
		0,											// standard buffer index
		__uuidof(ID3D11Texture2D),					// get structure of direct 3d texture
		m_backBuffer.put_void()						// double pointer to make more generic
	));

	winrt::check_hresult(m_device->CreateRenderTargetView(
		m_backBuffer.get(),							// pointer value of backBuffer
		nullptr,									// auto copy size and pixel settings
		m_renderTargetView.put()						// write to render target view
	));

	ID3D11RenderTargetView* targets[] = { m_renderTargetView.get() };		// array target
	m_deviceContext->OMSetRenderTargets(								// tells gpu to start use target
		1,																// number of targets
		targets,														// target array
		nullptr															// depth buffer (none 2d)
	);

	winrt::com_ptr<IDXGIDevice> dxgiDevice;
	dxgiDevice = m_device.as<IDXGIDevice>();							// converts normal device into dxgi device

	winrt::com_ptr<IInspectable> graphicDevice;							// default interface for winrt
	winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(			// actually makes the transfer from 
		dxgiDevice.get(),												// gets raw com pointer
		graphicDevice.put()												// puts inside the winrt object
	));

	m_finalDevice = graphicDevice.as<winrt_d3d11::IDirect3DDevice>();	// turns iinspectable into useable direct3d device

	m_uiManager->InitUI(hWindow, m_device.get(), m_deviceContext.get());

}

void GraphicsEngine::Destroy() {

	m_uiManager->ShutDownUI();

}

void GraphicsEngine::RunUILoopAndPresent() {

	m_uiManager->InitFrameUI();
	m_uiManager->RenderUI();
	m_uiManager->CloseFrameUI(m_renderTargetView.get(), m_deviceContext.get(), m_swapChain.get());

}