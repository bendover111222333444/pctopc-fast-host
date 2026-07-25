#include "CaptureEngine.h" 
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

void CaptureEngine::Start(ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, IDXGISwapChain* swapChain, winrt_d3d11::IDirect3DDevice winrtDevice, UINT bufferCount)
{
	m_framePool = winrt_capture::Direct3D11CaptureFramePool::CreateFreeThreaded(													// new capture thread
		winrtDevice,																												// graphics device
		winrt_directx::DirectXPixelFormat::R8G8B8A8UIntNormalized,																	// color format
		bufferCount,																												// buffer frame count
		m_winrtSize																													// width height settings
	);

	m_framePool.FrameArrived([context, backBuffer, swapChain, this](auto const& sender, auto const& args) {

		std::lock_guard<std::mutex> lock(m_pipelineMutex);																					// fixes the parrelle thread issues
		if (m_isStopping) return;

		winrt_capture::Direct3D11CaptureFrame frame = sender.TryGetNextFrame();														// fetch frame
		if (frame != nullptr) {																										// prevent null

			winrt_d3d11::IDirect3DSurface surface = frame.Surface();

			auto interopAccess = surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();				// new windows texture
			winrt::com_ptr<ID3D11Texture2D> rawTexture;

			interopAccess->GetInterface(																							// transfers interopacess
				__uuidof(ID3D11Texture2D),																							// interface id
				rawTexture.put_void()																								// output pointer
			);

			if (rawTexture && backBuffer) {

				context->CopyResource(backBuffer, rawTexture.get());
				::PostMessageW(
					m_hWindow,
					WM_RENDER_UI_MESSAGE,
					0,
					0
				);

			}

			surface.Close();

		}

		frame.Close();

		});

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