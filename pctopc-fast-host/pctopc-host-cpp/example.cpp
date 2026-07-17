#include <iostream>
#include <vector>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h> // For Microsoft::WRL::ComPtr

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

int main() {
    HRESULT hr = S_OK;

    // 1. Create the D3D11 Device and Context
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL featureLevel;

    hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &device, &featureLevel, &context
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 Device. HR: " << hr << std::endl;
        return 1;
    }

    // 2. Get DXGI Device, Adapter, and Output (Monitor)
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = device.As(&dxgiDevice);

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter));

    ComPtr<IDXGIOutput> dxgiOutput;
    // 0 is typically the primary monitor
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    if (FAILED(hr)) {
        std::cerr << "Failed to find primary DXGI output." << std::endl;
        return 1;
    }

    // 3. Query IDXGIOutput1 to duplicate output
    ComPtr<IDXGIOutput1> dxgiOutput1;
    hr = dxgiOutput.As(&dxgiOutput1);

    ComPtr<IDXGIOutputDuplication> deskDupl;
    hr = dxgiOutput1->DuplicateOutput(device.Get(), &deskDupl);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize Desktop Duplication. (Ensure you aren't on a hybrid GPU laptop without forcing the right adapter)." << std::endl;
        return 1;
    }

    // 4. Acquire Next Frame
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    ComPtr<IDXGIResource> desktopResource;

    std::cout << "Waiting for screen update to capture frame..." << std::endl;
    // Timeout of 1000ms
    hr = deskDupl->AcquireNextFrame(1000, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        std::cerr << "Failed to acquire next frame. HR: " << hr << std::endl;
        return 1;
    }

    // 5. Query the texture from the acquired resource
    ComPtr<ID3D11Texture2D> acquiredTex;
    hr = desktopResource.As(&acquiredTex);

    D3D11_TEXTURE2D_DESC frameDesc;
    acquiredTex->GetDesc(&frameDesc);

    // 6. Create a Staging Texture to copy data from GPU to CPU
    D3D11_TEXTURE2D_DESC stagingDesc = frameDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> stagingTex;
    hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTex);
    if (FAILED(hr)) {
        std::cerr << "Failed to create staging texture." << std::endl;
        deskDupl->ReleaseFrame();
        return 1;
    }

    // Copy the screen texture into our CPU-readable staging texture
    context->CopyResource(stagingTex.Get(), acquiredTex.Get());

    // We can release the DXGI frame loop immediately after copying
    deskDupl->ReleaseFrame();

    // 7. Map the staging texture to read its data
    D3D11_MAPPED_SUBRESOURCE mappedSubresource;
    hr = context->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mappedSubresource);
    if (SUCCEEDED(hr)) {
        std::cout << "Successfully captured screen!" << std::endl;
        std::cout << "Width: " << frameDesc.Width << "px" << std::endl;
        std::cout << "Height: " << frameDesc.Height << "px" << std::endl;
        std::cout << "Row Pitch (stride): " << mappedSubresource.RowPitch << " bytes" << std::endl;

        // mappedSubresource.pData contains the raw BGRA/RGBA pixel bits
        // You can save this to a file or process it here.
        uint8_t* rawPixels = reinterpret_cast<uint8_t*>(mappedSubresource.pData);

        // Unmap when finished
        context->Unmap(stagingTex.Get(), 0);
    }
    else {
        std::cerr << "Failed to map staging texture." << std::endl;
    }

    system("pause");

    return 0;
}