#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <d3d11.h>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ============================================================
// Video / Window Config
// ============================================================
struct VideoConfig
{
    int Width = 800;
    int Height = 600;
    bool IsFullscreen = false;
    bool NeedsResize = false;
    int VSync = 1;
} g_Config;

// fixed-size window for this homework
DWORD g_WindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

// ============================================================
// DirectX Global Variables
// ============================================================
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// ============================================================
// Video Resource Rebuild
// ============================================================
void RebuildVideoResources(HWND hWnd)
{
    if (!g_pSwapChain) return;

    // release old render target before ResizeBuffers
    if (g_pRenderTargetView)
    {
        g_pRenderTargetView->Release();
        g_pRenderTargetView = nullptr;
    }

    HRESULT hr = g_pSwapChain->ResizeBuffers(
        0,
        g_Config.Width,
        g_Config.Height,
        DXGI_FORMAT_UNKNOWN,
        0
    );

    if (FAILED(hr))
    {
        printf("ResizeBuffers failed\n");
        return;
    }

    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr) || pBackBuffer == nullptr)
    {
        printf("GetBuffer failed\n");
        return;
    }

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    if (FAILED(hr))
    {
        printf("CreateRenderTargetView failed\n");
        return;
    }

    // keep the window size fixed in windowed mode
    if (!g_Config.IsFullscreen)
    {
        RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
        AdjustWindowRect(&rc, g_WindowStyle, FALSE);

        SetWindowPos(
            hWnd,
            nullptr,
            0, 0,
            rc.right - rc.left,
            rc.bottom - rc.top,
            SWP_NOMOVE | SWP_NOZORDER
        );
    }

    g_Config.NeedsResize = false;
}

// ============================================================
// Window Procedure
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// ============================================================
// Entry Point
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // ------------------------------------------------------------
    // 1. Register and create window
    // ------------------------------------------------------------
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11VideoClass";
    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, g_WindowStyle, FALSE);

    HWND hWnd = CreateWindowW(
        L"DX11VideoClass",
        L"Lecture04-HW",
        g_WindowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd)
    {
        printf("CreateWindow failed\n");
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);

    // ------------------------------------------------------------
    // 2. Create DX11 Device and SwapChain
    // ------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = g_Config.Width;
    sd.BufferDesc.Height = g_Config.Height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        nullptr,
        &g_pImmediateContext
    );

    if (FAILED(hr))
    {
        printf("D3D11CreateDeviceAndSwapChain failed\n");
        return 0;
    }

    // build the first render target
    RebuildVideoResources(hWnd);

    // ------------------------------------------------------------
    // 3. Game Loop
    // ------------------------------------------------------------
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // check ESC every frame
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                PostMessage(hWnd, WM_CLOSE, 0, 0);
            }

            // toggle fullscreen
            if (GetAsyncKeyState('F') & 0x0001)
            {
                g_Config.IsFullscreen = !g_Config.IsFullscreen;
                g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
                g_Config.NeedsResize = true;
            }

            // rebuild render target after fullscreen change
            if (g_Config.NeedsResize)
            {
                RebuildVideoResources(hWnd);
            }

            // ----------------------------------------------------
            // rendering
            // ----------------------------------------------------
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            // viewport follows the current video config
            D3D11_VIEWPORT vp = {};
            vp.TopLeftX = 0.0f;
            vp.TopLeftY = 0.0f;
            vp.Width = (float)g_Config.Width;
            vp.Height = (float)g_Config.Height;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;

            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            // drawing code will be added back later
            g_pSwapChain->Present(g_Config.VSync, 0);
        }
    }

    // ============================================================
    // Cleanup
    // ============================================================
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}