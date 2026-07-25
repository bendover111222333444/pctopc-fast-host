#include "Application.h"

int main() {
	
	try {

		Application app;
		app.Start();

	}
	catch (winrt::hresult_error const& ex) {

		return static_cast<int>(ex.to_abi());	// errors 

	}

	return 0;

}