#include "UIManager.h"
#include "imgui.h" 
#include "imgui_impl_win32.h" 
#include "imgui_impl_dx11.h"

void UIManager::RenderUI()
{
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = 1.5f;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::Button("Start")) {}
		if (ImGui::Button("Stop")) {}
		if (ImGui::Button("Token")) {}

		if (ImGui::BeginMenu("Settings"))
		{
			if (ImGui::MenuItem("Engine")) {}
			if (ImGui::MenuItem("Preferences")) {}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	io.FontGlobalScale = 1.0f;
}

void UIManager::ResizeUI(Resolution res)
{

	if (ImGui::GetCurrentContext() != nullptr)
	{

		float scaleX = 3840.0f / static_cast<float>(res.width);
		float scaleY = 2160.0f / static_cast<float>(res.height);

		ImGui::GetIO().DisplayFramebufferScale = ImVec2(scaleX, scaleY);

	}

}

void UIManager::InitUI(HWND hWindow, ID3D11Device* device, ID3D11DeviceContext* deviceContext) {

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplWin32_Init(hWindow);
	ImGui_ImplDX11_Init(device, deviceContext);
	ImGui::StyleColorsDark();

}

void UIManager::ShutDownUI() {

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

}

void UIManager::InitFrameUI() {

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

}

void UIManager::CloseFrameUI(ID3D11RenderTargetView* renderTargetView, ID3D11DeviceContext* deviceContext, IDXGISwapChain* swapChain) {

	ImGui::Render();

	ID3D11RenderTargetView* targets[] = { renderTargetView };
	deviceContext->OMSetRenderTargets(1, targets, nullptr);

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	swapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);

}

void UIManager::OnResize(Resolution res) { 

	ResizeUI(res); 

}