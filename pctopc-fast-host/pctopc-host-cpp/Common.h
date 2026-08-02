#pragma once

#include <Windows.h>                                     // core Windows types and handles (HWND)
#include <string>										 // string
#include <vector>										 // vector
#include <mutex>										 // multithreading
#include <functional>									 // functions through parameters
#include <atomic>									     // adds atomically timed variables

#define WM_RENDER_UI_MESSAGE (WM_USER + 1)				 // thread macro

struct Resolution {

	UINT width = 0;
	UINT height = 0;

};