#pragma once

#include "Common.h"

class WindowManager {

private:

	std::wstring m_appName = L"";
	HMODULE m_hInstance = nullptr;
	HWND m_hWindow = nullptr;
	std::vector<IWindowEventListener*> m_listeners{};

	static void HandleCreationMessage(HWND hwnd, LPARAM lParam);
	bool HandleMessage(HWND hwnd, UINT uMsg, LPARAM lParam);

public:

	WindowManager(const WindowManager&) = delete;
	WindowManager& operator=(const WindowManager&) = delete;
	
	WindowManager() = default;
	~WindowManager();

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void Init(const std::wstring& appName, Resolution startRes, int iconId);
	void Destroy();
	void Show();
	void RegisterListener(IWindowEventListener* listener);
	void MessageLoopRun(std::function<void(const MSG& msg)> method);

	HMODULE GetHandle() const { return m_hInstance; }
	HWND GetWindowHandle() const { return m_hWindow; }

};