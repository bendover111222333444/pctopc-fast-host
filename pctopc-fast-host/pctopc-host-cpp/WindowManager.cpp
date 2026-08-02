#include "WindowManager.h"
#include "UIService.h"
#include "imgui_impl_win32.h" 
#include "resource.h"
#include <timeapi.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

WindowManager::~WindowManager() { Destroy(); }

void WindowManager::HandleCreationMessage(HWND hwnd, LPARAM lParam) {

	CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);					// gets lparam by reinterpreting to create struct
	WindowManager* pClass = reinterpret_cast<WindowManager*>(pCreate->lpCreateParams);	// gets "this" pointer to our thing

	::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pClass));		// save to win32 userdata

}

bool WindowManager::HandleMessage(HWND hwnd, UINT uMsg, LPARAM lParam) {

	switch (uMsg)
	{
	case WM_DESTROY:
		::PostQuitMessage(0);
		return true;

	case WM_SIZE:
	{
		Resolution res = {
			.width = (UINT)LOWORD(lParam),
			.height = (UINT)HIWORD(lParam),
		};

		UIService::ResizeUI(res);

		return false;
	}
	default:
		return false;
	}

}

LRESULT CALLBACK WindowManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	if (uMsg == WM_NCCREATE) {

		HandleCreationMessage(hwnd, lParam);

	}

	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) return true;

	WindowManager* pClass = reinterpret_cast<WindowManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));	// gets the "this" pointer back
	if (pClass) {

		if (pClass->HandleMessage(hwnd, uMsg, lParam) == true) return 0;

	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);	// default case skip

}

void WindowManager::Init(const std::wstring& appName, Resolution startRes, int iconId)
{

	m_appName = appName;
	m_hInstance = ::GetModuleHandleW(nullptr);

	timeBeginPeriod(1);

	HANDLE hProcess = GetCurrentProcess();

	PROCESS_POWER_THROTTLING_STATE powerState = {};
	powerState.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
	powerState.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
	powerState.StateMask = 0;										   // speeeeeeeeeeeed

	SetProcessInformation(
		hProcess,
		ProcessPowerThrottling,
		&powerState,
		sizeof(powerState)
	);
	
	WNDCLASSW windowSetup = {
		.lpfnWndProc = WindowProc,
		.hInstance = m_hInstance,													// tells windows who owns this
		.hIcon = ::LoadIconW(m_hInstance, MAKEINTRESOURCE(iconId)),					// loads pctopc icon
		.hCursor = ::LoadCursorW(nullptr, IDC_ARROW),								// make sure default cursor is still visible
		.lpszClassName = m_appName.c_str(),											// gives app top title name
	};

	::RegisterClassW(&windowSetup);													// registers class to windows

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

	const DWORD DWMWA_EXCLUDE_FROM_PEEK_VAL = 12;
	BOOL disablePeek = TRUE;
	DwmSetWindowAttribute(m_hWindow, DWMWA_EXCLUDE_FROM_PEEK_VAL, &disablePeek, sizeof(disablePeek));

}

void WindowManager::Destroy() {

	timeEndPeriod(1);

	if (m_hWindow) {

		::DestroyWindow(m_hWindow);
		m_hWindow = nullptr;

	}
	if (m_hInstance) {

		::UnregisterClassW(m_appName.c_str(), m_hInstance);
		m_hInstance = nullptr;

	}

}

void WindowManager::Show() {

	::ShowWindow(m_hWindow, SW_SHOW);

	::SetWindowPos(
		m_hWindow,													// hwnd
		NULL,														// inserts
		0, 0, 0, 0,													// pos
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED	// flags
	);

	::UpdateWindow(m_hWindow);

}

void WindowManager::FreezeRunMainThread(std::function<void(const MSG& msg)>method) {

	MSG msg = {};

	while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {

		method(msg);

		::TranslateMessage(&msg);
		::DispatchMessageW(&msg);

	}

}