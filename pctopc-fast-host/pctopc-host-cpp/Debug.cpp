#include "Debug.h"

void Debug::EnableDebugConsole() {

	AllocConsole();

	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	freopen_s(&fp, "CONIN$", "r", stdin);

	std::ios::sync_with_stdio();
	std::cout.clear();
	std::cerr.clear();

	setvbuf(stdout, NULL, _IONBF, 0);

	m_consoleEnabled = true;

}

void Debug::LogFPS() {
	
	if (m_consoleEnabled == false) {

		std::cerr << "Custom console is not enabled enable with EnableDebugConsole\n";

	}

	static auto lastLog = std::chrono::steady_clock::now();

	static int frameCount = 0;
	frameCount++;

	auto now = std::chrono::steady_clock::now();

	if (std::chrono::duration_cast<std::chrono::seconds>(now - lastLog).count() >= 1) {

		printf("[CAPTURE] FrameArrived count in last second: %d\n", frameCount);
		frameCount = 0;
		lastLog = now;

	}

}

HRESULT Debug::LogHR(const char* action, HRESULT hr) {

	if (m_consoleEnabled == false) {

		std::cerr << "Custom console is not enabled enable with EnableDebugConsole\n";

	}

	if (FAILED(hr)) {

		_com_error err(hr);
		wprintf(L"[HRESULT ERROR] %S failed with 0x%08X: %s\n", action, hr, err.ErrorMessage());

	}

	return hr;

}

NVENCSTATUS Debug::LogNV(const char* action, NVENCSTATUS status) {

	if (m_consoleEnabled == false) {

		std::cerr << "Custom console is not enabled enable with EnableDebugConsole\n";

	}

	if (status != NV_ENC_SUCCESS) {

		printf("[NVENCSTATUS ERROR] %s failed with status code %d\n", action, status);
	
	}

	return status;
}