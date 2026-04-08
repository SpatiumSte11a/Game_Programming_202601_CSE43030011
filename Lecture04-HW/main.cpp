#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <chrono>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

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

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer1 = nullptr;
ID3D11Buffer* g_pVertexBuffer2 = nullptr;

// ============================================================
// Triangle Data
// ============================================================
struct Vertex
{
    float x, y, z;
    float r, g, b, a;
};

// local shape for the first triangle
Vertex g_BaseTriangle1[] =
{
    {  0.0f,  0.12f, 0.5f, 1.0f, 0.2f, 0.2f, 1.0f },
    {  0.10f, -0.10f, 0.5f, 1.0f, 0.2f, 0.2f, 1.0f },
    { -0.10f, -0.10f, 0.5f, 1.0f, 0.2f, 0.2f, 1.0f }
};

// local shape for the second triangle
Vertex g_BaseTriangle2[] =
{
    {  0.0f,  0.12f, 0.5f, 0.2f, 0.8f, 1.0f, 1.0f },
    {  0.10f, -0.10f, 0.5f, 0.2f, 0.8f, 1.0f, 1.0f },
    { -0.10f, -0.10f, 0.5f, 0.2f, 0.8f, 1.0f, 1.0f }
};

// first triangle position
float g_Triangle1X = -0.5f;
float g_Triangle1Y = 0.0f;

// second triangle position
float g_Triangle2X = 0.5f;
float g_Triangle2Y = 0.0f;

// shared movement speed in normalized device coordinates
float g_MoveSpeed = 0.8f;

// simple passthrough shader
const char* g_ShaderSource = R"(
struct VS_INPUT
{
    float3 pos : POSITION;
    float4 col : COLOR;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
    return input.col;
}
)";

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
// Shader / Buffer Setup
// ============================================================
bool CreateTriangleResources()
{
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    HRESULT hr = D3DCompile(
        g_ShaderSource,
        strlen(g_ShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "VS",
        "vs_4_0",
        0,
        0,
        &vsBlob,
        &errorBlob
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            printf("VS Compile Error: %s\n", (char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    hr = D3DCompile(
        g_ShaderSource,
        strlen(g_ShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "PS",
        "ps_4_0",
        0,
        0,
        &psBlob,
        &errorBlob
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            printf("PS Compile Error: %s\n", (char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        if (vsBlob) vsBlob->Release();
        return false;
    }

    hr = g_pd3dDevice->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &g_pVertexShader
    );
    if (FAILED(hr))
    {
        printf("CreateVertexShader failed\n");
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    hr = g_pd3dDevice->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &g_pPixelShader
    );
    if (FAILED(hr))
    {
        printf("CreatePixelShader failed\n");
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = g_pd3dDevice->CreateInputLayout(
        layout,
        2,
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &g_pInputLayout
    );

    vsBlob->Release();
    psBlob->Release();

    if (FAILED(hr))
    {
        printf("CreateInputLayout failed\n");
        return false;
    }

    // vertex buffer for the first triangle
    D3D11_BUFFER_DESC bd1 = {};
    bd1.ByteWidth = sizeof(g_BaseTriangle1);
    bd1.Usage = D3D11_USAGE_DEFAULT;
    bd1.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData1 = {};
    initData1.pSysMem = g_BaseTriangle1;

    hr = g_pd3dDevice->CreateBuffer(&bd1, &initData1, &g_pVertexBuffer1);
    if (FAILED(hr))
    {
        printf("CreateBuffer for triangle 1 failed\n");
        return false;
    }

    // vertex buffer for the second triangle
    D3D11_BUFFER_DESC bd2 = {};
    bd2.ByteWidth = sizeof(g_BaseTriangle2);
    bd2.Usage = D3D11_USAGE_DEFAULT;
    bd2.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData2 = {};
    initData2.pSysMem = g_BaseTriangle2;

    hr = g_pd3dDevice->CreateBuffer(&bd2, &initData2, &g_pVertexBuffer2);
    if (FAILED(hr))
    {
        printf("CreateBuffer for triangle 2 failed\n");
        return false;
    }

    return true;
}

// ============================================================
// Triangle Update
// ============================================================
void UpdateTriangle1Position(float dt)
{
    float vx = 0.0f;
    float vy = 0.0f;

    // first triangle control
    if (GetAsyncKeyState('W') & 0x8000) vy += 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) vy -= 1.0f;
    if (GetAsyncKeyState('A') & 0x8000) vx -= 1.0f;
    if (GetAsyncKeyState('D') & 0x8000) vx += 1.0f;

    g_Triangle1X += vx * g_MoveSpeed * dt;
    g_Triangle1Y += vy * g_MoveSpeed * dt;

    if (g_Triangle1X < -0.85f) g_Triangle1X = -0.85f;
    if (g_Triangle1X > 0.85f) g_Triangle1X = 0.85f;
    if (g_Triangle1Y < -0.85f) g_Triangle1Y = -0.85f;
    if (g_Triangle1Y > 0.85f) g_Triangle1Y = 0.85f;
}

void UpdateTriangle2Position(float dt)
{
    float vx = 0.0f;
    float vy = 0.0f;

    // second triangle control
    if (GetAsyncKeyState(VK_UP) & 0x8000)    vy += 1.0f;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)  vy -= 1.0f;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)  vx -= 1.0f;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) vx += 1.0f;

    g_Triangle2X += vx * g_MoveSpeed * dt;
    g_Triangle2Y += vy * g_MoveSpeed * dt;

    if (g_Triangle2X < -0.85f) g_Triangle2X = -0.85f;
    if (g_Triangle2X > 0.85f) g_Triangle2X = 0.85f;
    if (g_Triangle2Y < -0.85f) g_Triangle2Y = -0.85f;
    if (g_Triangle2Y > 0.85f) g_Triangle2Y = 0.85f;
}

void UpdateTriangle1Buffer()
{
    Vertex currentVertices[3];

    for (int i = 0; i < 3; i++)
    {
        currentVertices[i] = g_BaseTriangle1[i];
        currentVertices[i].x += g_Triangle1X;
        currentVertices[i].y += g_Triangle1Y;
    }

    g_pImmediateContext->UpdateSubresource(
        g_pVertexBuffer1,
        0,
        nullptr,
        currentVertices,
        0,
        0
    );
}

void UpdateTriangle2Buffer()
{
    Vertex currentVertices[3];

    for (int i = 0; i < 3; i++)
    {
        currentVertices[i] = g_BaseTriangle2[i];
        currentVertices[i].x += g_Triangle2X;
        currentVertices[i].y += g_Triangle2Y;
    }

    g_pImmediateContext->UpdateSubresource(
        g_pVertexBuffer2,
        0,
        nullptr,
        currentVertices,
        0,
        0
    );
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
    // 3. Create triangle resources
    // ------------------------------------------------------------
    if (!CreateTriangleResources())
    {
        printf("Triangle resource creation failed\n");
        return 0;
    }

    // ------------------------------------------------------------
    // 4. Game Loop
    // ------------------------------------------------------------
    MSG msg = { 0 };

    // frame timing for movement
    std::chrono::high_resolution_clock::time_point prevTime =
        std::chrono::high_resolution_clock::now();

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            std::chrono::high_resolution_clock::time_point currentTime =
                std::chrono::high_resolution_clock::now();

            std::chrono::duration<float> elapsed = currentTime - prevTime;
            float deltaTime = elapsed.count();
            prevTime = currentTime;

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

            // update both triangle positions
            UpdateTriangle1Position(deltaTime);
            UpdateTriangle2Position(deltaTime);

            // update both vertex buffers
            UpdateTriangle1Buffer();
            UpdateTriangle2Buffer();

            // ----------------------------------------------------
            // rendering
            // ----------------------------------------------------
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };

            D3D11_VIEWPORT vp = {};
            vp.TopLeftX = 0.0f;
            vp.TopLeftY = 0.0f;
            vp.Width = (float)g_Config.Width;
            vp.Height = (float)g_Config.Height;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;

            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            g_pImmediateContext->IASetInputLayout(g_pInputLayout);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

            // draw first triangle
            {
                UINT stride = sizeof(Vertex);
                UINT offset = 0;
                g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer1, &stride, &offset);
                g_pImmediateContext->Draw(3, 0);
            }

            // draw second triangle
            {
                UINT stride = sizeof(Vertex);
                UINT offset = 0;
                g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer2, &stride, &offset);
                g_pImmediateContext->Draw(3, 0);
            }

            g_pSwapChain->Present(g_Config.VSync, 0);
        }
    }

    // ============================================================
    // Cleanup
    // ============================================================
    if (g_pVertexBuffer1) g_pVertexBuffer1->Release();
    if (g_pVertexBuffer2) g_pVertexBuffer2->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}