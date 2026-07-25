#pragma once

#include "Common.h"
#include "resource.h"
#include "Application.h"
#include "CaptureEngine.h"
#include "GraphicsEngine.h"
#include "UIManager.h"
#include "WindowManager.h"

class Application {

private:

	std::wstring m_appName = L"pctopc-fast-host";
	UINT m_fps = 144;
	UINT m_bufferCount = 2;
	UINT m_monitorDefault = MONITOR_DEFAULTTOPRIMARY;
	int m_iconId = IDI_ICON1;
	Resolution m_startingSize = {
		.width = 600,
		.height = 400,
	};

	std::unique_ptr<UIManager> m_uiManager = nullptr;
	std::unique_ptr<WindowManager> m_windowManager = nullptr;
	std::unique_ptr<CaptureEngine> m_captureEngine = nullptr;
	std::unique_ptr<GraphicsEngine> m_graphicsEngine = nullptr;

	void Init();
	void Run();
	void Loop();
	void Shutdown();

public:

	Application() = default;
	~Application() = default;

	void Start();

};