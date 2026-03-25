#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

/* =============================================================================
 * [1. Vertex Structure]
 * ========================================================================== */
typedef struct Vertex
{
    float x, y, z;
    float r, g, b, a;
} Vertex;

/* =============================================================================
 * [2. Game Context]
 * ========================================================================== */
typedef struct GameContext
{
    float posX;
    float posY;
    float moveSpeed;
    int isRunning;

    int keyUp;
    int keyDown;
    int keyLeft;
    int keyRight;

    Vertex hexagram[6];
} GameContext;

static GameContext g_game =
{
    0.0f,    /* posX */
    0.0f,    /* posY */
    0.002f,  /* moveSpeed */
    1,       /* isRunning */
	0, 0, 0, 0,	   /* keyUp, keyDown, keyLeft, keyRight */
	{ 0 }   /* hexagram vertices will be initialized in BuildHexagramVertices() */
};

/* =============================================================================
 * [3. DirectX Global Objects]
 * ========================================================================== */
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pImmediateContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_pRenderTargetView = NULL;
static ID3D11VertexShader* g_pVertexShader = NULL;
static ID3D11PixelShader* g_pPixelShader = NULL;
static ID3D11InputLayout* g_pVertexLayout = NULL;
static ID3D11Buffer* g_pVertexBuffer = NULL;
static ID3D11RasterizerState* g_pRasterState = NULL;

/* =============================================================================
 * [4. Shader Source]
 * ========================================================================== */
const char* g_shaderSource =
"struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };"
"struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };"

"PS_INPUT VS(VS_INPUT input) {"
"    PS_INPUT output;"
"    output.pos = float4(input.pos, 1.0f);"
"    output.col = input.col;"
"    return output;"
"}"

"float4 PS(PS_INPUT input) : SV_Target {"
"    return input.col;"
"}";

/* =============================================================================
 * [5. Hexagram Builder]
 * ========================================================================== */
void BuildHexagramVertices(GameContext* ctx)
{
    float x = ctx->posX;
    float y = ctx->posY;

    /* --- [Up Triangle] ----------------------------------------------------- */
    ctx->hexagram[0].x = 0.00f + x;  ctx->hexagram[0].y = 0.36f + y;  ctx->hexagram[0].z = 0.5f;
    ctx->hexagram[1].x = -0.31f + x;  ctx->hexagram[1].y = -0.18f + y;  ctx->hexagram[1].z = 0.5f;
    ctx->hexagram[2].x = 0.31f + x;  ctx->hexagram[2].y = -0.18f + y;  ctx->hexagram[2].z = 0.5f;

    ctx->hexagram[0].r = 1.0f; ctx->hexagram[0].g = 0.85f; ctx->hexagram[0].b = 0.20f; ctx->hexagram[0].a = 1.0f;
    ctx->hexagram[1].r = 1.0f; ctx->hexagram[1].g = 0.35f; ctx->hexagram[1].b = 0.20f; ctx->hexagram[1].a = 1.0f;
    ctx->hexagram[2].r = 1.0f; ctx->hexagram[2].g = 0.55f; ctx->hexagram[2].b = 0.20f; ctx->hexagram[2].a = 1.0f;

    /* --- [Down Triangle] --------------------------------------------------- */
    ctx->hexagram[3].x = 0.00f + x;  ctx->hexagram[3].y = -0.36f + y;  ctx->hexagram[3].z = 0.5f;
    ctx->hexagram[4].x = 0.31f + x;  ctx->hexagram[4].y = 0.18f + y;  ctx->hexagram[4].z = 0.5f;
    ctx->hexagram[5].x = -0.31f + x;  ctx->hexagram[5].y = 0.18f + y;  ctx->hexagram[5].z = 0.5f;

    ctx->hexagram[3].r = 0.20f; ctx->hexagram[3].g = 0.80f; ctx->hexagram[3].b = 1.0f; ctx->hexagram[3].a = 1.0f;
    ctx->hexagram[4].r = 0.20f; ctx->hexagram[4].g = 0.45f; ctx->hexagram[4].b = 1.0f; ctx->hexagram[4].a = 1.0f;
    ctx->hexagram[5].r = 0.45f; ctx->hexagram[5].g = 0.20f; ctx->hexagram[5].b = 1.0f; ctx->hexagram[5].a = 1.0f;
}

/* =============================================================================
 * [6. Vertex Buffer Recreation]
 * ========================================================================== */
int RecreateVertexBuffer(GameContext* ctx)
{
    HRESULT hr;
    D3D11_BUFFER_DESC bd;
    D3D11_SUBRESOURCE_DATA initData;

    if (g_pVertexBuffer != NULL)
    {
        g_pVertexBuffer->lpVtbl->Release(g_pVertexBuffer);
        g_pVertexBuffer = NULL;
    }

    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Vertex) * 6;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    ZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = ctx->hexagram;

    hr = g_pd3dDevice->lpVtbl->CreateBuffer(
        g_pd3dDevice,
        &bd,
        &initData,
        &g_pVertexBuffer
    );
    if (FAILED(hr)) return 0;

    return 1;
}

/* =============================================================================
 * [7. Game Loop Stages]
 * ========================================================================== */
void ProcessInput(GameContext* ctx)
{
    (void)ctx;
}

void UpdateGame(GameContext* ctx)
{
    int moved = 0;
    float halfWidth = 0.31f;
    float halfHeight = 0.36f;

    if (ctx->keyLeft)
    {
        ctx->posX -= ctx->moveSpeed;
        moved = 1;
    }

    if (ctx->keyRight)
    {
        ctx->posX += ctx->moveSpeed;
        moved = 1;
    }

    if (ctx->keyUp)
    {
        ctx->posY += ctx->moveSpeed;
        moved = 1;
    }

    if (ctx->keyDown)
    {
        ctx->posY -= ctx->moveSpeed;
        moved = 1;
    }

    /* --- [Boundary Check] -------------------------------------------------- */
    if (ctx->posX < -1.0f + halfWidth)  ctx->posX = -1.0f + halfWidth;
    if (ctx->posX > 1.0f - halfWidth)  ctx->posX = 1.0f - halfWidth;
    if (ctx->posY < -1.0f + halfHeight) ctx->posY = -1.0f + halfHeight;
    if (ctx->posY > 1.0f - halfHeight) ctx->posY = 1.0f - halfHeight;

    if (moved)
    {
        BuildHexagramVertices(ctx);
        RecreateVertexBuffer(ctx);
    }
}

void RenderGame(void)
{
    float clearColor[4] = { 0.05f, 0.10f, 0.18f, 1.0f };
    D3D11_VIEWPORT vp;
    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    g_pImmediateContext->lpVtbl->ClearRenderTargetView(
        g_pImmediateContext,
        g_pRenderTargetView,
        clearColor
    );

    g_pImmediateContext->lpVtbl->OMSetRenderTargets(
        g_pImmediateContext,
        1,
        &g_pRenderTargetView,
        NULL
    );

    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = 800.0f;
    vp.Height = 600.0f;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    g_pImmediateContext->lpVtbl->RSSetViewports(g_pImmediateContext, 1, &vp);
    g_pImmediateContext->lpVtbl->RSSetState(g_pImmediateContext, g_pRasterState);
    g_pImmediateContext->lpVtbl->IASetInputLayout(g_pImmediateContext, g_pVertexLayout);
    g_pImmediateContext->lpVtbl->IASetVertexBuffers(
        g_pImmediateContext,
        0,
        1,
        &g_pVertexBuffer,
        &stride,
        &offset
    );
    g_pImmediateContext->lpVtbl->IASetPrimitiveTopology(
        g_pImmediateContext,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    g_pImmediateContext->lpVtbl->VSSetShader(g_pImmediateContext, g_pVertexShader, NULL, 0);
    g_pImmediateContext->lpVtbl->PSSetShader(g_pImmediateContext, g_pPixelShader, NULL, 0);

    g_pImmediateContext->lpVtbl->Draw(g_pImmediateContext, 6, 0);
    g_pSwapChain->lpVtbl->Present(g_pSwapChain, 0, 0);
}

/* =============================================================================
 * [8. Window Procedure]
 * ========================================================================== */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd;
    (void)lParam;

    switch (message)
    {
    case WM_KEYDOWN:
        if (wParam == VK_UP || wParam == 'W') g_game.keyUp = 1;
        if (wParam == VK_DOWN || wParam == 'S') g_game.keyDown = 1;
        if (wParam == VK_LEFT || wParam == 'A') g_game.keyLeft = 1;
        if (wParam == VK_RIGHT || wParam == 'D') g_game.keyRight = 1;

        if (wParam == 'Q')
        {
            g_game.isRunning = 0;
            PostQuitMessage(0);
        }
        return 0;

    case WM_KEYUP:
        if (wParam == VK_UP || wParam == 'W') g_game.keyUp = 0;
        if (wParam == VK_DOWN || wParam == 'S') g_game.keyDown = 0;
        if (wParam == VK_LEFT || wParam == 'A') g_game.keyLeft = 0;
        if (wParam == VK_RIGHT || wParam == 'D') g_game.keyRight = 0;
        return 0;

    case WM_DESTROY:
        g_game.isRunning = 0;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

/* =============================================================================
 * [9. DirectX Initialization]
 * ========================================================================== */
int InitD3D(HWND hWnd)
{
    HRESULT hr;
    DXGI_SWAP_CHAIN_DESC sd;
    ID3D11Texture2D* pBackBuffer = NULL;
    ID3DBlob* pVSBlob = NULL;
    ID3DBlob* pPSBlob = NULL;
    D3D11_INPUT_ELEMENT_DESC layout[2];
    D3D11_RASTERIZER_DESC rsDesc;

    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        NULL,
        &g_pImmediateContext
    );
    if (FAILED(hr)) return 0;

    hr = g_pSwapChain->lpVtbl->GetBuffer(
        g_pSwapChain,
        0,
        &IID_ID3D11Texture2D,
        (void**)&pBackBuffer
    );
    if (FAILED(hr)) return 0;

    hr = g_pd3dDevice->lpVtbl->CreateRenderTargetView(
        g_pd3dDevice,
        (ID3D11Resource*)pBackBuffer,
        NULL,
        &g_pRenderTargetView
    );
    pBackBuffer->lpVtbl->Release(pBackBuffer);
    if (FAILED(hr)) return 0;

    hr = D3DCompile(
        g_shaderSource,
        strlen(g_shaderSource),
        NULL,
        NULL,
        NULL,
        "VS",
        "vs_4_0",
        0,
        0,
        &pVSBlob,
        NULL
    );
    if (FAILED(hr)) return 0;

    hr = D3DCompile(
        g_shaderSource,
        strlen(g_shaderSource),
        NULL,
        NULL,
        NULL,
        "PS",
        "ps_4_0",
        0,
        0,
        &pPSBlob,
        NULL
    );
    if (FAILED(hr)) return 0;

    hr = g_pd3dDevice->lpVtbl->CreateVertexShader(
        g_pd3dDevice,
        pVSBlob->lpVtbl->GetBufferPointer(pVSBlob),
        pVSBlob->lpVtbl->GetBufferSize(pVSBlob),
        NULL,
        &g_pVertexShader
    );
    if (FAILED(hr)) return 0;

    hr = g_pd3dDevice->lpVtbl->CreatePixelShader(
        g_pd3dDevice,
        pPSBlob->lpVtbl->GetBufferPointer(pPSBlob),
        pPSBlob->lpVtbl->GetBufferSize(pPSBlob),
        NULL,
        &g_pPixelShader
    );
    if (FAILED(hr)) return 0;

    layout[0].SemanticName = "POSITION";
    layout[0].SemanticIndex = 0;
    layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    layout[0].InputSlot = 0;
    layout[0].AlignedByteOffset = 0;
    layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    layout[0].InstanceDataStepRate = 0;

    layout[1].SemanticName = "COLOR";
    layout[1].SemanticIndex = 0;
    layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    layout[1].InputSlot = 0;
    layout[1].AlignedByteOffset = 12;
    layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    layout[1].InstanceDataStepRate = 0;

    hr = g_pd3dDevice->lpVtbl->CreateInputLayout(
        g_pd3dDevice,
        layout,
        2,
        pVSBlob->lpVtbl->GetBufferPointer(pVSBlob),
        pVSBlob->lpVtbl->GetBufferSize(pVSBlob),
        &g_pVertexLayout
    );
    if (FAILED(hr)) return 0;

    pVSBlob->lpVtbl->Release(pVSBlob);
    pPSBlob->lpVtbl->Release(pPSBlob);

    BuildHexagramVertices(&g_game);
    if (!RecreateVertexBuffer(&g_game)) return 0;

    ZeroMemory(&rsDesc, sizeof(rsDesc));
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.DepthClipEnable = TRUE;

    hr = g_pd3dDevice->lpVtbl->CreateRasterizerState(
        g_pd3dDevice,
        &rsDesc,
        &g_pRasterState
    );
    if (FAILED(hr)) return 0;

    return 1;
}

/* =============================================================================
 * [10. Cleanup]
 * ========================================================================== */
void CleanupD3D(void)
{
    if (g_pRasterState)       g_pRasterState->lpVtbl->Release(g_pRasterState);
    if (g_pVertexBuffer)      g_pVertexBuffer->lpVtbl->Release(g_pVertexBuffer);
    if (g_pVertexLayout)      g_pVertexLayout->lpVtbl->Release(g_pVertexLayout);
    if (g_pVertexShader)      g_pVertexShader->lpVtbl->Release(g_pVertexShader);
    if (g_pPixelShader)       g_pPixelShader->lpVtbl->Release(g_pPixelShader);
    if (g_pRenderTargetView)  g_pRenderTargetView->lpVtbl->Release(g_pRenderTargetView);
    if (g_pSwapChain)         g_pSwapChain->lpVtbl->Release(g_pSwapChain);
    if (g_pImmediateContext)  g_pImmediateContext->lpVtbl->Release(g_pImmediateContext);
    if (g_pd3dDevice)         g_pd3dDevice->lpVtbl->Release(g_pd3dDevice);
}

/* =============================================================================
 * [11. WinMain]
 * ========================================================================== */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXW wcex;
    HWND hWnd;
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;

    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"Lecture02HWClass";

    RegisterClassExW(&wcex);

    hWnd = CreateWindowW(
        L"Lecture02HWClass",
        L"Lecture02 HW",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hWnd) return -1;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    printf("=== Moving Hexagram Homework ===\n");
    printf("Control : Arrow Keys / WASD\n");
    printf("Quit    : Q or Window Close Button\n\n");

    if (!InitD3D(hWnd)) return -1;

    ZeroMemory(&msg, sizeof(msg));

    while (WM_QUIT != msg.message && g_game.isRunning)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            ProcessInput(&g_game);
            UpdateGame(&g_game);
            RenderGame();
        }
    }

    CleanupD3D();
    return (int)msg.wParam;
}