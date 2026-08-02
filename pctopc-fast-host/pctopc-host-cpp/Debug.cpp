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

}

HRESULT Debug::LogHR(const char* action, HRESULT hr) {

	if (FAILED(hr)) {

		_com_error err(hr);
		wprintf(L"[ERROR] %S failed with 0x%08X: %s\n", action, hr, err.ErrorMessage());

	}

	return hr;

}