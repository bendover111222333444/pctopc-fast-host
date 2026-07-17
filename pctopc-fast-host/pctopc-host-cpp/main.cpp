#pragma once											 // standard checkoff thing

#include <iostream>                                      // standard console I/O (cout)
#include <functional>									 // gives function types
#include <Windows.h>                                     // core Windows types and handles (HWND)
#include <mutex>										 // mutex to prevent multithreading bull
#include "resource.h"									 // resources mapping for images whatever 

#include <windows.graphics.capture.interop.h>            // bridge: converts HWND to winrt capture items
#include <windows.graphics.directx.direct3d11.interop.h> // bridge: wraps raw D3D11 device into winrt

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <winrt/Windows.Foundation.h>                    // core winrt framework and smart pointers
#include <winrt/Windows.Graphics.Capture.h>              // the actual screen-capture engine
#include <winrt/Windows.Graphics.DirectX.h>              // Modern wrapper for DirectX types
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>   // Modern wrapper for Direct3D structures

#include <d3d11.h>                                       // classic direct3d 11 device and pipeline

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "d3d11.lib")

using std::wstring;
using std::function;
using std::mutex;
using std::lock_guard;
using std::vector;

namespace winrt_graphics = winrt::Windows::Graphics;
namespace winrt_capture = winrt::Windows::Graphics::Capture;
namespace winrt_directx = winrt::Windows::Graphics::DirectX;
namespace winrt_d3d11 = winrt::Windows::Graphics::DirectX::Direct3D11;

#define WM_RENDER_UI_MESSAGE (WM_USER + 1)				 // thread macro

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct Resolution {
	UINT width = 0;
	UINT height = 0;
};

class IWindowEventListener {
	public:

		virtual ~IWindowEventListener() = default;
		virtual void OnResize(Resolution res) = 0;

};

class UIManager : public IWindowEventListener {

public:

	void RenderUI()
	{

		ImGui::Begin("My Class Window");

		if (ImGui::BeginTabBar("Tabs"))
		{
			if (ImGui::BeginTabItem("Main"))
			{
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Config"))
			{
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();

	}

	void ResizeUI(Resolution res)
	{
		
		if (ImGui::GetCurrentContext() != nullptr)
		{

			float scaleX = 3840.0f / static_cast<float>(res.width);
			float scaleY = 2160.0f / static_cast<float>(res.height);

			ImGui::GetIO().DisplayFramebufferScale = ImVec2(scaleX, scaleY);
		
		} 

	}

	void InitUI(HWND hWindow, ID3D11Device* device, ID3D11DeviceContext* deviceContext) {

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		ImGui_ImplWin32_Init(hWindow);
		ImGui_ImplDX11_Init(device, deviceContext);
		ImGui::StyleColorsDark();

	}

	void ShutDownUI() {

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

	}

	void InitFrameUI() {

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

	}

	void CloseFrameUI(ID3D11RenderTargetView* renderTargetView, ID3D11DeviceContext* deviceContext, IDXGISwapChain* swapChain) {

		ImGui::Render();

		ID3D11RenderTargetView* targets[] = { renderTargetView };
		deviceContext->OMSetRenderTargets(1, targets, nullptr);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		swapChain->Present(1, 0);

	}

	void OnResize(Resolution res) override { ResizeUI(res); }

};

class WindowManager {

	private: 
		wstring m_appName;
		HMODULE m_hInstance;
		HWND m_hWindow;
		vector<IWindowEventListener*> m_listeners;

	public:

		WindowManager(const WindowManager&) = delete;
		WindowManager& operator=(const WindowManager&) = delete;

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{

			if (uMsg == WM_NCCREATE) {

				CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);					// gets lparam by reinterpreting to create struct
				WindowManager* pClass = reinterpret_cast<WindowManager*>(pCreate->lpCreateParams);	// gets "this" pointer to our thing

				::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pClass));		// save to win32 userdata

			}

			if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
				return true;
			
			WindowManager* pClass = reinterpret_cast<WindowManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));	// gets the "this" pointer back
			if (pClass) {
				
				if (uMsg == WM_DESTROY)	// checks if exists
				{													// message from windows

					::PostQuitMessage(0);								// if destroyed quit
					return 0;										// returns that was sugcessful

				}
				else if (uMsg == WM_SIZE) {

					Resolution res = {
						.width = (UINT)LOWORD(lParam),
						.height = (UINT)HIWORD(lParam),
					};

					for (auto* listener : pClass->m_listeners) {
					
						listener->OnResize(res);
					
					}

				}

			}

			return DefWindowProc(hwnd, uMsg, wParam, lParam);	// default case skip

		}

		WindowManager(wstring name, Resolution startRes, int iconId) :
			m_appName(name),
			m_hInstance(::GetModuleHandleW(nullptr)),
			m_hWindow(nullptr)
		{

			WNDCLASS windowSetup = {
				.lpfnWndProc = WindowProc,
				.hInstance = m_hInstance,													// tells windows who owns this
				.hIcon = ::LoadIconW(m_hInstance, MAKEINTRESOURCE(iconId)),					// loads pctopc icon
				.hCursor = ::LoadCursorW(nullptr, IDC_ARROW),									// make sure default cursor is still visible
				.lpszClassName = m_appName.c_str(),											// gives app top title name
			};

			::RegisterClassW(&windowSetup);	// registers class to windows

			m_hWindow = CreateWindowExW(
				0,												// extended Style (0 = no special traits)
				m_appName.c_str(),								// class Name (matches your registered blueprint)
				m_appName.c_str(),								// window title (top bar text)
				WS_OVERLAPPEDWINDOW,							// window style (standard borders and min/max/close buttons)
				CW_USEDEFAULT,									// xpos (default)
				CW_USEDEFAULT,									// ypos (default)
				startRes.width,									// width
				startRes.height,								// height
				nullptr,										// parent window
				nullptr,										// menu
				m_hInstance,									// instance handle (program identity token)
				this											// creation data (this to give private var access)
			);

		}

		~WindowManager() {
			
			if (m_hWindow) {
			
				::DestroyWindow(m_hWindow);
			
			}

			::UnregisterClassW(m_appName.c_str(), m_hInstance);	// unregisters to windows

		}

		void RegisterListener(IWindowEventListener* listener) {

			if (listener) {
			
				m_listeners.push_back(listener);
			
			}

		}

		void Show() {

			::ShowWindow(m_hWindow, SW_SHOW);
		
		}

		void MessageLoopRun(function<void()>method, int methodActivate) {

			MSG msg = {};

			while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {

				if (method && msg.message == methodActivate)
				{
					method();
				}

				::TranslateMessage(&msg);
				::DispatchMessageW(&msg);

			}
			
		}

		HMODULE GetHandle() const {

			return m_hInstance;
		
		}

		HWND GetWindowHandle() const {
		
			return m_hWindow;
		
		}

};

class GraphicsEngine {
	private:
		Resolution m_res;
		UIManager* m_uiManager;
		winrt::com_ptr<IDXGISwapChain> m_swapChain;
		winrt::com_ptr<ID3D11Texture2D> m_backBuffer;
		winrt::com_ptr<ID3D11Device> m_device;
		winrt::com_ptr<ID3D11DeviceContext> m_deviceContext;
		winrt::com_ptr<ID3D11RenderTargetView> m_renderTargetView;
		winrt_d3d11::IDirect3DDevice m_finalDevice;

	public:

		GraphicsEngine(const GraphicsEngine&) = delete;			// anti copy
		GraphicsEngine& operator=(const GraphicsEngine&) = delete;

		GraphicsEngine(HWND hWindow, UIManager* uiManager, Resolution startRes, UINT fps, UINT bufferCount)
			: m_res(startRes),
			m_uiManager(uiManager),
			m_swapChain(nullptr),
			m_backBuffer(nullptr),
			m_device(nullptr),
			m_deviceContext(nullptr),
			m_renderTargetView(nullptr),
			m_finalDevice(nullptr)
		{

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

		~GraphicsEngine() {
		
			m_uiManager->ShutDownUI();

		};

		void RunUILoopAndPresent() {

			m_uiManager->InitFrameUI();
			m_uiManager->RenderUI();
			m_uiManager->CloseFrameUI(m_renderTargetView.get(), m_deviceContext.get(), m_swapChain.get());

		}

		winrt_d3d11::IDirect3DDevice GetWinRTDevice() const {
		
			return m_finalDevice;

		}

		ID3D11DeviceContext* GetContext() const {
			
			return m_deviceContext.get();
		
		}
		
		ID3D11Texture2D* GetBackBuffer() const {

			return m_backBuffer.get();

		}

		IDXGISwapChain* GetSwapChain() const {

			return m_swapChain.get();

		}


};

class CaptureEngine {

private:

	Resolution m_res;
	HWND m_hWindow;
	wstring m_appName;
	winrt_graphics::SizeInt32 m_winrtSize;
	winrt_capture::GraphicsCaptureItem m_captureItem;
	winrt_capture::GraphicsCaptureSession m_captureSession;
	winrt_capture::Direct3D11CaptureFramePool m_framePool;
	std::mutex m_pipelineMutex;
	bool m_isStopping{ false };

public:

	CaptureEngine(const CaptureEngine&) = delete;			// anti copy
	CaptureEngine& operator=(const CaptureEngine&) = delete;

	CaptureEngine(HWND hWindow, UINT monitorDefault, wstring appName)

		: m_hWindow(hWindow),
		m_appName(appName),
		m_winrtSize({ 0, 0 }),
		m_captureItem(nullptr),
		m_captureSession(nullptr),
		m_framePool(nullptr)


	{

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

	void Start(ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, IDXGISwapChain* swapChain, winrt_d3d11::IDirect3DDevice winrtDevice, UINT bufferCount)
	{
		m_framePool = winrt_capture::Direct3D11CaptureFramePool::CreateFreeThreaded(													// new capture thread
			winrtDevice,																												// graphics device
			winrt_directx::DirectXPixelFormat::R8G8B8A8UIntNormalized,																	// color format
			bufferCount,																												// buffer frame count
			m_winrtSize																													// width height settings
		);

		m_framePool.FrameArrived([context, backBuffer, swapChain, this](auto const& sender, auto const& args) {

			lock_guard<mutex> lock(m_pipelineMutex);																					// fixes the parrelle thread issues
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

	~CaptureEngine() {
		Stop();
	}

	void Stop() {

		lock_guard<mutex> lock(m_pipelineMutex); // stop gaurd
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

	Resolution GetResolution() const {

		return m_res;

	}

};

class Application {



};

int main() {

	try {

		winrt::init_apartment();

		wstring appName = L"pctopc-fast-host";
		UINT fps = 60;
		UINT bufferCount = 2;
		UINT monitorDefault = MONITOR_DEFAULTTOPRIMARY;
		int iconId = IDI_ICON1;
		Resolution startingSize = {
			.width = 3840,
			.height = 2160,
		};

		UIManager uiManager;

		WindowManager window(
			appName,		// self explanitory
			startingSize,	// size
			iconId			// self explanitory
		);

		window.RegisterListener(&uiManager);

		CaptureEngine capture(
			window.GetWindowHandle(),	// gets the handle 
			monitorDefault,				// default monitor macro (1)
			appName
		);

		GraphicsEngine graphics(
			window.GetWindowHandle(),	// gets the handle
			&uiManager,
			capture.GetResolution(),			// gets witdth and height mopnitor
			fps,						// fps
			bufferCount					// buffer count min 2
		);

		window.Show();
		capture.Start(
			graphics.GetContext(),
			graphics.GetBackBuffer(),
			graphics.GetSwapChain(),
			graphics.GetWinRTDevice(),	// gets the final device (winrt device)
			bufferCount					// buffer count
		);

		window.MessageLoopRun([&graphics]() { graphics.RunUILoopAndPresent(); }, WM_RENDER_UI_MESSAGE);
		capture.Stop();

	}

	catch (winrt::hresult_error const& ex) {

		return static_cast<int>(ex.to_abi());	// errors 

	}

	return 0;

}