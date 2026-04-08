#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <string>
#include <chrono>
#include <cstdio>
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

HWND g_hWnd = nullptr;

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

// shared shader for all triangle renderers
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
// Vertex Type
// ============================================================
struct Vertex
{
    float x, y, z;
    float r, g, b, a;
};

// ============================================================
// Forward Declarations
// ============================================================
void RebuildVideoResources(HWND hWnd);
bool InitD3D(HWND hWnd);
bool InitShadersAndInputLayout();
void CleanupD3D();
void PrintGameInstruction();

// ============================================================
// Component System
// ============================================================
class GameObject;

class Component
{
public:
    GameObject* pOwner = nullptr;
    bool isStarted = false;

    virtual void Start() = 0;
    virtual void Input() {}
    virtual void Update(float dt) = 0;
    virtual void Render() {}
    virtual ~Component() {}
};

class GameObject
{
public:
    std::string name;
    float x, y;
    std::vector<Component*> components;

    GameObject(std::string n, float startX = 0.0f, float startY = 0.0f)
    {
        name = n;
        x = startX;
        y = startY;
    }

    ~GameObject()
    {
        for (int i = 0; i < (int)components.size(); i++)
        {
            delete components[i];
        }
    }

    void AddComponent(Component* pComp)
    {
        pComp->pOwner = this;
        pComp->isStarted = false;
        components.push_back(pComp);
    }

    // start each component only once
    void StartComponents()
    {
        for (int i = 0; i < (int)components.size(); i++)
        {
            if (components[i]->isStarted == false)
            {
                components[i]->Start();
                components[i]->isStarted = true;
            }
        }
    }

    void InputComponents()
    {
        for (int i = 0; i < (int)components.size(); i++)
        {
            components[i]->Input();
        }
    }

    void UpdateComponents(float dt)
    {
        for (int i = 0; i < (int)components.size(); i++)
        {
            components[i]->Update(dt);
        }
    }

    void RenderComponents()
    {
        for (int i = 0; i < (int)components.size(); i++)
        {
            components[i]->Render();
        }
    }
};

// ============================================================
// Components
// ============================================================
class PlayerControl : public Component
{
public:
    float speed;
    bool moveUp, moveDown, moveLeft, moveRight;
    int playerType = 0; // 0 = WASD, 1 = Arrow Keys

    PlayerControl(int type)
    {
        playerType = type;
    }

    void Start() override
    {
        speed = 0.8f;
        moveUp = moveDown = moveLeft = moveRight = false;
    }

    void Input() override
    {
        if (playerType == 0)
        {
            moveUp = (GetAsyncKeyState('W') & 0x8000);
            moveDown = (GetAsyncKeyState('S') & 0x8000);
            moveLeft = (GetAsyncKeyState('A') & 0x8000);
            moveRight = (GetAsyncKeyState('D') & 0x8000);
        }
        else if (playerType == 1)
        {
            moveUp = (GetAsyncKeyState(VK_UP) & 0x8000);
            moveDown = (GetAsyncKeyState(VK_DOWN) & 0x8000);
            moveLeft = (GetAsyncKeyState(VK_LEFT) & 0x8000);
            moveRight = (GetAsyncKeyState(VK_RIGHT) & 0x8000);
        }
    }

    void Update(float dt) override
    {
        float vx = 0.0f;
        float vy = 0.0f;

        if (moveUp)    vy += 1.0f;
        if (moveDown)  vy -= 1.0f;
        if (moveLeft)  vx -= 1.0f;
        if (moveRight) vx += 1.0f;

        // position = position + velocity * deltaTime
        pOwner->x += vx * speed * dt;
        pOwner->y += vy * speed * dt;

        // keep the triangle inside the visible area
        if (pOwner->x < -0.85f) pOwner->x = -0.85f;
        if (pOwner->x > 0.85f) pOwner->x = 0.85f;
        if (pOwner->y < -0.85f) pOwner->y = -0.85f;
        if (pOwner->y > 0.85f) pOwner->y = 0.85f;
    }
};

class TriangleRenderer : public Component
{
public:
    float r, g, b, a;
    ID3D11Buffer* pVertexBuffer = nullptr;

    TriangleRenderer(float red, float green, float blue, float alpha = 1.0f)
    {
        r = red;
        g = green;
        b = blue;
        a = alpha;
    }

    void Start() override
    {
        // local triangle shape around the owner position
        Vertex vertices[3] =
        {
            {  0.0f,  0.12f, 0.5f, r, g, b, a },
            {  0.10f, -0.10f, 0.5f, r, g, b, a },
            { -0.10f, -0.10f, 0.5f, r, g, b, a }
        };

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = sizeof(vertices);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = vertices;

        HRESULT hr = g_pd3dDevice->CreateBuffer(&bd, &initData, &pVertexBuffer);
        if (FAILED(hr))
        {
            printf("Triangle vertex buffer create failed\n");
        }
    }

    void Update(float dt) override
    {
        (void)dt;
    }

    void Render() override
    {
        if (!pVertexBuffer) return;

        // rebuild the current vertices from owner position
        Vertex vertices[3] =
        {
            {  pOwner->x + 0.0f,  pOwner->y + 0.12f, 0.5f, r, g, b, a },
            {  pOwner->x + 0.10f, pOwner->y - 0.10f, 0.5f, r, g, b, a },
            {  pOwner->x - 0.10f, pOwner->y - 0.10f, 0.5f, r, g, b, a }
        };

        g_pImmediateContext->UpdateSubresource(pVertexBuffer, 0, nullptr, vertices, 0, 0);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;

        g_pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
        g_pImmediateContext->IASetInputLayout(g_pInputLayout);
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

        g_pImmediateContext->Draw(3, 0);
    }

    ~TriangleRenderer() override
    {
        if (pVertexBuffer)
        {
            pVertexBuffer->Release();
            pVertexBuffer = nullptr;
        }
    }
};

// ============================================================
// Game Loop
// ============================================================
class GameLoop
{
public:
    bool isRunning;
    std::vector<GameObject*> gameWorld;
    std::chrono::high_resolution_clock::time_point prevTime;
    float deltaTime;

    void Initialize()
    {
        isRunning = true;
        gameWorld.clear();
        prevTime = std::chrono::high_resolution_clock::now();
        deltaTime = 0.0f;
    }

    void Input()
    {
        // check ESC every frame
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            isRunning = false;
            PostMessage(g_hWnd, WM_CLOSE, 0, 0);
            return;
        }

        // toggle fullscreen
        if (GetAsyncKeyState('F') & 0x0001)
        {
            g_Config.IsFullscreen = !g_Config.IsFullscreen;
            if (g_pSwapChain)
            {
                g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
            }
            g_Config.NeedsResize = true;
        }

        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            gameWorld[i]->InputComponents();
        }
    }

    void Update()
    {
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            gameWorld[i]->StartComponents();
        }

        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            gameWorld[i]->UpdateComponents(deltaTime);
        }
    }

    void Render()
    {
        if (!g_pImmediateContext || !g_pRenderTargetView) return;

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

        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            gameWorld[i]->RenderComponents();
        }

        g_pSwapChain->Present(g_Config.VSync, 0);
    }

    void Run()
    {
        MSG msg = { 0 };

        while (isRunning && msg.message != WM_QUIT)
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
                deltaTime = elapsed.count();
                prevTime = currentTime;

                // rebuild render target after fullscreen change
                if (g_Config.NeedsResize)
                {
                    RebuildVideoResources(g_hWnd);
                }

                Input();
                Update();
                Render();
            }
        }
    }

    GameLoop()
    {
        Initialize();
    }

    ~GameLoop()
    {
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            delete gameWorld[i];
        }
    }
};

// ============================================================
// Console Help
// ============================================================
void PrintGameInstruction()
{
    printf("===============================================\n");
    printf("Lecture04-HW\n");
    printf("===============================================\n");
    printf("Player 1 : Red Triangle\n");
    printf("Control  : W, A, S, D\n\n");

    printf("Player 2 : Blue Triangle\n");
    printf("Control  : Arrow Keys\n\n");

    printf("ESC      : Exit the program\n");
    printf("F        : Toggle between Windowed Mode (800 x 600)\n");
    printf("           and Fullscreen Mode\n");
    printf("===============================================\n\n");
}

// ============================================================
// DirectX Setup
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

bool InitShadersAndInputLayout()
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

    return true;
}

bool InitD3D(HWND hWnd)
{
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
        return false;
    }

    RebuildVideoResources(hWnd);

    if (!InitShadersAndInputLayout())
    {
        printf("InitShadersAndInputLayout failed\n");
        return false;
    }

    return true;
}

void CleanupD3D()
{
    if (g_pSwapChain && g_Config.IsFullscreen)
    {
        g_pSwapChain->SetFullscreenState(FALSE, nullptr);
    }

    if (g_pInputLayout) { g_pInputLayout->Release();      g_pInputLayout = nullptr; }
    if (g_pVertexShader) { g_pVertexShader->Release();     g_pVertexShader = nullptr; }
    if (g_pPixelShader) { g_pPixelShader->Release();      g_pPixelShader = nullptr; }
    if (g_pRenderTargetView) { g_pRenderTargetView->Release(); g_pRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pImmediateContext) { g_pImmediateContext->Release(); g_pImmediateContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
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
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // ------------------------------------------------------------
    // 1. Register and create window
    // ------------------------------------------------------------
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11GameWorldClass";

    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, g_WindowStyle, FALSE);

    g_hWnd = CreateWindowW(
        L"DX11GameWorldClass",
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

    if (!g_hWnd)
    {
        printf("CreateWindow failed\n");
        return 0;
    }

    ShowWindow(g_hWnd, nCmdShow);

    if (!InitD3D(g_hWnd))
    {
        CleanupD3D();
        return 0;
    }

    PrintGameInstruction();

    // ------------------------------------------------------------
    // 2. Create game loop and game objects
    // ------------------------------------------------------------
    GameLoop gLoop;
    gLoop.Initialize();

    // first player
    GameObject* player1 = new GameObject("Player1", -0.5f, 0.0f);
    player1->AddComponent(new PlayerControl(0)); // WASD
    player1->AddComponent(new TriangleRenderer(1.0f, 0.2f, 0.2f));
    gLoop.gameWorld.push_back(player1);

    // second player
    GameObject* player2 = new GameObject("Player2", 0.5f, 0.0f);
    player2->AddComponent(new PlayerControl(1)); // Arrow Keys
    player2->AddComponent(new TriangleRenderer(0.2f, 0.8f, 1.0f));
    gLoop.gameWorld.push_back(player2);

    // ------------------------------------------------------------
    // 3. Run
    // ------------------------------------------------------------
    gLoop.Run();

    CleanupD3D();
    return 0;
}