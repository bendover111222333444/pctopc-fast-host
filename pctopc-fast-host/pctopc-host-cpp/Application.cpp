#include "Common.h"
#include "resource.h"
#include "Application.h"
#include "CaptureEngine.h"
#include "GraphicsEngine.h"
#include "UIManager.h"
#include "WindowManager.h"

void Application::Init() {

	winrt::init_apartment();

	m_uiManager = std::make_unique<UIManager>();

	m_windowManager = std::make_unique<WindowManager>();
	m_windowManager->Init(m_appName, m_startingSize, m_iconId);

	m_windowManager->RegisterListener(m_uiManager.get());

	m_captureEngine = std::make_unique<CaptureEngine>();
	m_captureEngine->Init(m_windowManager->GetWindowHandle(), m_monitorDefault, m_appName);

	m_graphicsEngine = std::make_unique<GraphicsEngine>();
	m_graphicsEngine->Init(
		m_windowManager->GetWindowHandle(),
		m_uiManager.get(),
		m_captureEngine->GetResolution(),
		m_fps,
		m_bufferCount
	);

}

void Application::Run() {

	m_windowManager->Show();

	m_captureEngine->Start(

		m_graphicsEngine->GetContext(),
		m_graphicsEngine->GetBackBuffer(),
		m_graphicsEngine->GetSwapChain(),
		m_graphicsEngine->GetWinRTDevice(),
		m_bufferCount

	);

}

void Application::Loop() {

	m_windowManager->MessageLoopRun([this](const MSG& msg) {

		if (msg.message == WM_RENDER_UI_MESSAGE) 
		{ 

			m_graphicsEngine->RunUILoopAndPresent(); 

		}
		
	});

}

void Application::Shutdown() {

	if (m_captureEngine) {

		m_captureEngine->Destroy();

	}

}

void Application::Start() {

	Init();
	Run();
	Loop();
	Shutdown();

}