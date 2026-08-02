#pragma once

#include "Common.h"

#include <comdef.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>

namespace Debug {

	void EnableDebugConsole();
	HRESULT LogHR(const char* action, HRESULT hr);

}