#include "Application.h"
#include "Debug.h"

#include <avrt.h>
#include <d3dkmthk.h>
#include <comdef.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>

#pragma comment(lib, "Avrt.lib")

void Application::Init() {

	winrt::init_apartment();
	
	PROCESS_POWER_THROTTLING_STATE state = {};
	state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
	state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
	state.StateMask = 0;

	SetProcessInformation(
		GetCurrentProcess(), 
		ProcessPowerThrottling,
		&state, 
		sizeof(state)
	);

	D3DKMTSetProcessSchedulingPriorityClass(
		GetCurrentProcess(),
		D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH
	);

	DWORD dka_taskIndex = 0;
	HANDLE dka_mmcssHandle = AvSetMmThreadCharacteristicsW(L"Capture", &dka_taskIndex);

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	timeBeginPeriod(1);
	timeEndPeriod(1);

	Debug::EnableDebugConsole();

	m_windowManager = std::make_unique<WindowManager>();
	m_windowManager->Init(
		m_appName, 
		m_startingSize, 
		m_iconId
	);

	m_captureEngine = std::make_unique<CaptureEngine>();
	m_captureEngine->Init(
		m_windowManager->GetWindowHandle(), 
		m_monitorDefault, 
		m_appName
	);

	m_graphicsEngine = std::make_unique<GraphicsEngine>();
	m_graphicsEngine->Init(
		m_windowManager->GetWindowHandle(),
		m_captureEngine->GetResolution(),
		m_fps,
		m_bufferCount
	);

	m_encoderEngine = std::make_unique<EncoderEngine>();
	m_encoderEngine->Init(
		m_graphicsEngine->GetDevice(),
		m_graphicsEngine->GetContext(),
		m_captureEngine->GetResolution(),
		m_fps,
		m_bitrate
	);

	UIService::InitUI(m_windowManager->GetWindowHandle(), m_graphicsEngine->GetDevice(), m_graphicsEngine->GetContext());

}

void Application::Run() {

	m_windowManager->Show();

	m_captureEngine->Start(

		m_encoderEngine.get(),
		m_graphicsEngine->GetContext(),
		m_graphicsEngine->GetBackBuffer(),
		m_graphicsEngine->GetWinRTDevice(),
		m_bufferCount

	);

}

void Application::Loop() {

	m_windowManager->FreezeRunMainThread([this](const MSG& msg) {

		if (msg.message == WM_RENDER_UI_MESSAGE) 
		{ 

			UIService::InitFrameUI();
			UIService::RenderUI();
			UIService::CloseFrameUI(m_graphicsEngine->GetRenderTarget(), m_graphicsEngine->GetContext());
			m_encoderEngine->CloseFrame(m_graphicsEngine->GetSwapChain());

		}
		
	});

}

void Application::Shutdown() {

	m_encoderEngine->Destroy();

	if (m_captureEngine) {

		m_captureEngine->Destroy();
		UIService::ShutDownUI();

	}

	m_captureEngine->Destroy();
	m_windowManager->Destroy();

}

void Application::Start() {

	Init();
	Run();
	Loop();
	Shutdown();

}