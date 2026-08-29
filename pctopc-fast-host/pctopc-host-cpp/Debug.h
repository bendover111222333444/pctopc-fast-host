#pragma once

#pragma once

#include "Common.h"
#include "nvEncodeAPI.h"

#include <comdef.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>

namespace Debug {

	inline bool m_consoleEnabled{ false };

	void EnableDebugConsole();
	void LogFPS();
	HRESULT LogHR(const char* action, HRESULT hr);
	NVENCSTATUS LogNV(const char* action, NVENCSTATUS status);

}