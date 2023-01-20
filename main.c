#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <stdio.h>
#include <assert.h>
#include <d3d11_1.h>
#include <windows.h>
#include <time.h>

typedef struct { float X, Y, Z; } v3;
typedef struct { float R, G, B, A; } color;
typedef struct { float M[4][4]; } matrix;

typedef struct { 
    v3 Position;
    v3 Direction;
    color Color;
    int Waiting;
} entity;

#define MAX_ENTITIES 1000

// Globals

entity Entities[MAX_ENTITIES];
v3 Camera = {8.0f, 9.0f, -22.0f};

int EntitiesAmount;
int TailLength;
int XTiles = 20;
int YTiles = 20;
int HeadIndex;
int FoodIndex;
int Running = 1;
int Pause;
int Delay = 30;
int DelayMin = 3;
int EnableWireframe = 0;

color ColorBGPlatform = {0.2f, 0.2f, 0.2f, 1.0f};
color ColorBGPlatformLighter = {0.21f, 0.21f, 0.21f, 1.0f};
color ColorHead = {1.0f, 0.05f, 0.95f, 1.0f};
color ColorFood = {0.30f, 0.4f, 0.9f, 1.0f};
color ColorBorder = {0.2f, 0.2f, 0.2f, 1.0f};
color ColorWireframe = {0.3f, 0.3f, 0.3f, 1.0f};

enum {UP, LEFT, DOWN, RIGHT, KEYSAMOUNT};

typedef struct {
    int Length;
    int Values[2];
} inputQueue;

inputQueue InputQueue;

void InputQueueAdd(inputQueue* Q, int Direction) {
    if(Q->Length >= 2) Q->Length = 0;
    Q->Values[Q->Length] = Direction;
    ++Q->Length;
}

int InputQueuePop(inputQueue* Q) {
    if(Q->Length <= 0) return -1;
    int Result = Q->Values[0];
    if(Q->Length > 1) {
        Q->Values[0] = Q->Values[1];
    } 
    --Q->Length;
    return Result;
}

int IsOppositeDirection(v3 A, v3 B) {
    int Result = 0;
    if(
       (A.X == 1.0f  && B.X == -1.0f) ||
       (A.X == -1.0f && B.X == 1.0f) ||
       (A.Y == 1.0f  && B.Y == -1.0f) ||
       (A.Y == -1.0f && B.Y == 1.0f)
       ) {
        Result = 1;
    }
    return Result;
}

v3 AddV3(v3 A, v3 B) {
    v3 Result = {0};
    Result.X += A.X + B.X;
    Result.Y += A.Y + B.Y;
    Result.Z += A.Z + B.Z;
    return Result;
}

int IsZeroV3(v3 Vector) {
    if(Vector.X == 0.0f &&
       Vector.Y == 0.0f &&
       Vector.Z == 0.0f) {
        return 1; 
    }
    return 0;
}

int CompareV3(v3 A, v3 B) {
    if(A.X == B.X &&
       A.Y == B.Y &&
       A.Z == B.Z) {
        return 1; 
    }
    return 0;
}

void AddEntity(entity* Entity) {
    Entities[EntitiesAmount++] = *Entity;
}

float GetRandomZeroToOne() {
    return (float)rand() / (float)RAND_MAX ;
}

color GetRandomColor() {
    return (color){
        GetRandomZeroToOne(),
        GetRandomZeroToOne(),
        GetRandomZeroToOne(),
    };
}

color GetRandomShadeOfGray() {
    float Shade = GetRandomZeroToOne();
    return (color){
        Shade,
        Shade,
        Shade,
    };
}

v3 GetRandomPosition() {
    return (v3){
        rand() % XTiles, 
        rand() % YTiles,
        0.0f,
    };
}

void Init() {
    
    // Platform
    
    for(int Y = 0; Y < YTiles; ++Y) {
        for(int X = 0; X < XTiles; ++X) {
            entity Entity = {
                .Position = {X, Y},
                .Color = ColorBGPlatform,
            };
            
            // Some lighter tiles
            
            if((rand() % 100) < 3) {
                Entity.Color = ColorBGPlatformLighter;
            }
            AddEntity(&Entity);
        }
    }
    
    // Food
    
    FoodIndex = EntitiesAmount;
    
    entity Food = {
        .Position = {rand() % XTiles, rand() % YTiles},
        .Color = ColorFood,
    };
    
    AddEntity(&Food);
    
    // Snake head
    
    HeadIndex = EntitiesAmount;
    
    entity Head = {
        .Position = {XTiles / 2, YTiles / 2},
        .Color = ColorHead,
        .Direction = {1.0f, 0.0f, 0.0f},
    };
    
    AddEntity(&Head);
}

int IsPositionOutOfBounds(v3 Position) {
    if(Position.X >= XTiles ||
       Position.Y >= YTiles ||
       Position.X < 0 ||
       Position.Y < 0) {
        return 1; 
    }
    return 0;
}

int IsPositionTailPiece(v3 Position) {
    for(int Index = HeadIndex + 1;
        Index < EntitiesAmount;
        ++Index) {
        if(CompareV3(Position, Entities[Index].Position)) {
            return 1;
        }
    }
    
    return 0;
}

void GrowSnake() {
    
    color Color = Entities[EntitiesAmount-1].Color;
    Color.R -= 0.05f; 
    Color.G += 0.05f; 
    Color.B += 0.05f;
    
    entity TailPiece = {
        .Position = Entities[EntitiesAmount-1].Position,
        .Color = Color,
        .Waiting = 1,
    };
    AddEntity(&TailPiece);
    ++TailLength;
}

void Update() {
    
    // Move tail
    
    for(int Index = EntitiesAmount-1; 
        Index >= HeadIndex + 1; 
        --Index) {
        
        if(!Entities[Index].Waiting) {
            Entities[Index].Position = Entities[Index-1].Position;
        } else {
            Entities[Index].Waiting = 0;
        }
        
    }
    
    // Head direction
    
    v3 NewDirection = {0};
    
    if(InputQueue.Length > 0) {
        int Direction = InputQueuePop(&InputQueue);
        if(Direction == UP) { 
            NewDirection.Y = 1.0f; 
        } else if(Direction == LEFT) { 
            NewDirection.X = -1.0f; 
        } else if(Direction == DOWN) { 
            NewDirection.Y = -1.0f; 
        } else if(Direction == RIGHT) { 
            NewDirection.X = 1.0f; 
        }
    }
    
    entity* Head = &Entities[HeadIndex];
    
    if(!IsZeroV3(NewDirection)) {
        if(TailLength > 0) {
            if(!IsOppositeDirection(Head->Direction, NewDirection)) {
                Head->Direction = NewDirection;
            }
        } else {
            Head->Direction = NewDirection;
        }
    }
    
    // Move head
    
    v3 NewPosition = AddV3(Head->Position, Head->Direction);
    
    if(IsPositionOutOfBounds(NewPosition) ||
       IsPositionTailPiece(NewPosition)
       ) {
        Running = 0;
    } else {
        Head->Position = NewPosition;
        
        // Food collision
        
        if(CompareV3(Head->Position, Entities[FoodIndex].Position)) {
            if(Delay > DelayMin) Delay -= 1;
            GrowSnake();
            Entities[FoodIndex].Position = GetRandomPosition();
        }
    }
}

LRESULT CALLBACK 
WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    switch(Message) {
        case WM_KEYDOWN: {
            switch(WParam) {
                case 'O': { 
                    DestroyWindow(Window); 
                } break;
                case VK_UP: { 
                    InputQueueAdd(&InputQueue, UP);
                } break;
                case VK_LEFT: { 
                    InputQueueAdd(&InputQueue, LEFT);
                } break;
                case VK_DOWN: { 
                    InputQueueAdd(&InputQueue, DOWN);
                } break;
                case VK_RIGHT: {
                    InputQueueAdd(&InputQueue, RIGHT);
                } break;
                case VK_SPACE: {
                    Pause = (Pause ? 0 : 1);
                } break;
                case 'W': { // Toggle wireframe
                    EnableWireframe = (EnableWireframe ? 0 : 1);
                } break;
            }
            
        } break;
        case WM_DESTROY: { PostQuitMessage(0); } break;
        
        default: {
            return DefWindowProc(Window, Message, WParam,  LParam);
        }
    }
    
    return 0;
}

int WINAPI 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CmdLine, int CmdShow) {
    
    srand(time(NULL));
    
    WNDCLASS WindowClass = {0};
    const char ClassName[] = "Window";
    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = ClassName;
    WindowClass.hCursor = LoadCursor(NULL, IDC_CROSS);
    
    if(!RegisterClass(&WindowClass)) {
        MessageBox(0, "RegisterClass failed", 0, 0);
        return GetLastError();
    }
    
    HWND Window = CreateWindowEx(0, ClassName, ClassName,
                                 WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                 0, 0, Instance, 0);
    
    if(!Window) {
        MessageBox(0, "CreateWindowEx failed", 0, 0);
        return GetLastError();
    }
    
    // Device & Context
    
    ID3D11Device* BaseDevice;
    ID3D11DeviceContext* BaseContext;
    
    UINT CreationFlags = 0;
#ifdef _DEBUG
    CreationFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL FeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_0
    };
    
    HRESULT Result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0,
                                       CreationFlags, FeatureLevels,
                                       ARRAYSIZE(FeatureLevels),
                                       D3D11_SDK_VERSION, &BaseDevice, 0,
                                       &BaseContext);    
    
    if(FAILED(Result)) {
        MessageBox(0, "D3D11CreateDevice failed", 0, 0);
        return GetLastError();
    }
    
    ID3D11Device1* Device;
    ID3D11DeviceContext1* Context;
    
    Result = ID3D11Device1_QueryInterface(BaseDevice, &IID_ID3D11Device1, (void**)&Device);
    assert(SUCCEEDED(Result));
    ID3D11Device1_Release(BaseDevice);
    
    Result = ID3D11DeviceContext1_QueryInterface(BaseContext, &IID_ID3D11DeviceContext1, (void**)&Context);
    assert(SUCCEEDED(Result));
    ID3D11Device1_Release(BaseContext);
    
    // Swap chain
    
    IDXGIDevice2* DxgiDevice;
    Result = ID3D11Device1_QueryInterface(Device, &IID_IDXGIDevice2, (void**)&DxgiDevice); 
    assert(SUCCEEDED(Result));
    
    IDXGIAdapter* DxgiAdapter;
    Result = IDXGIDevice2_GetAdapter(DxgiDevice, &DxgiAdapter); 
    assert(SUCCEEDED(Result));
    ID3D11Device1_Release(DxgiDevice);
    
    IDXGIFactory2* DxgiFactory;
    Result = IDXGIDevice2_GetParent(DxgiAdapter, &IID_IDXGIFactory2, (void**)&DxgiFactory); 
    assert(SUCCEEDED(Result));
    IDXGIAdapter_Release(DxgiAdapter);
    
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {0};
    SwapChainDesc.Width = 0;
    SwapChainDesc.Height = 0;
    SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 1;
    SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    SwapChainDesc.Flags = 0;
    
    IDXGISwapChain1* SwapChain;
    Result = IDXGIFactory2_CreateSwapChainForHwnd(DxgiFactory, (IUnknown*)Device, Window,
                                                  &SwapChainDesc, 0, 0, &SwapChain);
    assert(SUCCEEDED(Result));
    IDXGIFactory2_Release(DxgiFactory);
    
    // Render target view
    
    ID3D11Texture2D* FrameBuffer;
    Result = IDXGISwapChain1_GetBuffer(SwapChain, 0, &IID_ID3D11Texture2D, (void**)&FrameBuffer);
    assert(SUCCEEDED(Result));
    
    ID3D11RenderTargetView* RenderTargetView;
    Result = ID3D11Device1_CreateRenderTargetView(Device, (ID3D11Resource*)FrameBuffer, 0, &RenderTargetView);
    assert(SUCCEEDED(Result));
    ID3D11Texture2D_Release(FrameBuffer);
    
    // Shaders
    
    ID3D10Blob* VSBlob;
    D3DCompileFromFile(L"shaders.hlsl", 0, 0, "vs_main", "vs_5_0", 0, 0, &VSBlob, 0);
    ID3D11VertexShader* VertexShader;
    Result = ID3D11Device1_CreateVertexShader(Device,
                                              ID3D10Blob_GetBufferPointer(VSBlob),
                                              ID3D10Blob_GetBufferSize(VSBlob),
                                              0,
                                              &VertexShader);
    assert(SUCCEEDED(Result));
    
    ID3D10Blob* PSBlob;
    D3DCompileFromFile(L"shaders.hlsl", 0, 0, "ps_main", "ps_5_0", 0, 0, &PSBlob, 0);
    ID3D11PixelShader* PixelShader;
    Result = ID3D11Device1_CreatePixelShader(Device,
                                             ID3D10Blob_GetBufferPointer(PSBlob),
                                             ID3D10Blob_GetBufferSize(PSBlob),
                                             0,
                                             &PixelShader);
    assert(SUCCEEDED(Result));
    
    // Data layout
    
    D3D11_INPUT_ELEMENT_DESC InputElementDesc[] = {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 
            0, 0, 
            D3D11_INPUT_PER_VERTEX_DATA, 0
        }
    };
    
    ID3D11InputLayout* InputLayout;
    Result = ID3D11Device1_CreateInputLayout(Device, 
                                             InputElementDesc,
                                             ARRAYSIZE(InputElementDesc),
                                             ID3D10Blob_GetBufferPointer(VSBlob),
                                             ID3D10Blob_GetBufferSize(VSBlob),
                                             &InputLayout
                                             );
    assert(SUCCEEDED(Result));
    
    // Rectangle vertex data (TRIANGLE LIST)
    
    float VertexData[] = {
        -0.5f, -0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
    };
    
    UINT Stride = 3 * sizeof(float);
    UINT NumVertices = sizeof(VertexData) / Stride;
    UINT Offset = 0;
    
    D3D11_BUFFER_DESC BufferDesc = {
        sizeof(VertexData),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0, 0, 0
    };
    
    D3D11_SUBRESOURCE_DATA InitialData = { VertexData };
    
    ID3D11Buffer* Buffer;
    Result = ID3D11Device1_CreateBuffer(Device, &BufferDesc, &InitialData, &Buffer);
    assert(SUCCEEDED(Result));
    
    // Border vertex data (LINE LIST)
    
    float BorderVertexData[] = {
        -0.5f, -0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
    };
    
    UINT BorderStride = 3 * sizeof(float);
    UINT BorderNumVertices = sizeof(BorderVertexData) / BorderStride;
    UINT BorderOffset = 0;
    
    D3D11_BUFFER_DESC BorderBufferDesc = {
        sizeof(BorderVertexData),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0, 0, 0
    };
    
    D3D11_SUBRESOURCE_DATA BorderInitialData = { BorderVertexData };
    
    ID3D11Buffer* BorderBuffer;
    Result = ID3D11Device1_CreateBuffer(Device, &BorderBufferDesc, &BorderInitialData, &BorderBuffer);
    assert(SUCCEEDED(Result));
    
    // Constant buffer
    
    typedef struct {
        matrix Model;
        matrix View;
        matrix Projection;
        color Color;
    } constants;
    
    D3D11_BUFFER_DESC ConstantBufferDesc = {0};
    ConstantBufferDesc.ByteWidth  = sizeof(constants);
    ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    ID3D11Buffer* ConstantBuffer;
    Result = ID3D11Device1_CreateBuffer(Device, &ConstantBufferDesc, NULL, &ConstantBuffer);
    assert(SUCCEEDED(Result));
    
    // Viewport
    
    RECT WindowRect;
    GetClientRect(Window, &WindowRect);
    D3D11_VIEWPORT Viewport = { 
        0.0f, 0.0f, (FLOAT)(WindowRect.right - WindowRect.left),
        (FLOAT)(WindowRect.bottom - WindowRect.top),
        0.0f, 1.0f };
    
    // Projection matrix
    
    float AspectRatio = Viewport.Width / Viewport.Height;
    float Height = 1.0f;
    float Near = 1.0f;
    float Far = 100.0f;
    
    matrix ProjectionMatrix = {
        2.0f * Near / AspectRatio, 0.0f, 0.0f, 0.0f, 
        0.0f, 2.0f * Near / Height, 0.0f, 0.0f, 
        0.0f, 0.0f, Far / (Far - Near), 1.0f, 
        0.0f, 0.0f, Near * Far / (Near - Far), 0.0f 
    };
    
    // View matrix
    
    matrix ViewMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -Camera.X, -Camera.Y, -Camera.Z, 1.0f,
    };
    
    // Rasterizer states (only needed for wireframe toggling)
    
    ID3D11RasterizerState* RasterizerStateDefault = {0};
    D3D11_RASTERIZER_DESC RasterizerDescDefault = {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_BACK,
        .DepthClipEnable = 1,
    };
    Result = ID3D11Device1_CreateRasterizerState(Device, &RasterizerDescDefault, &RasterizerStateDefault);
    assert(SUCCEEDED(Result));
    
    ID3D11RasterizerState* RasterizerStateWireframe = {0};
    D3D11_RASTERIZER_DESC RasterizerDescWireframe = {
        .FillMode = D3D11_FILL_WIREFRAME,
        .CullMode = D3D11_CULL_BACK,
        .DepthClipEnable = 1,
    };
    
    Result = ID3D11Device1_CreateRasterizerState(Device, &RasterizerDescWireframe, &RasterizerStateWireframe);
    assert(SUCCEEDED(Result));
    
    Init();
    
    int Counter = 0;
    
    while(Running) {
        MSG Message;
        while(PeekMessage(&Message, NULL, 0, 0, PM_REMOVE)) {
            if(Message.message == WM_QUIT) Running = 0;
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
        
        if(Counter > Delay && !Pause) {
            Update();
            Counter = 0;
        }
        ++Counter;
        
        // Clear
        
        float Color[] = {0.3f, 0.3f, 0.3f, 1.0f};
        ID3D11DeviceContext1_ClearRenderTargetView(Context, RenderTargetView, Color);
        
        // Set stuff
        
        ID3D11DeviceContext1_RSSetViewports(Context, 1, &Viewport);
        ID3D11DeviceContext1_OMSetRenderTargets(Context, 1, &RenderTargetView, 0);
        ID3D11DeviceContext1_IASetInputLayout(Context, InputLayout);
        ID3D11DeviceContext1_VSSetShader(Context, VertexShader, 0, 0);
        ID3D11DeviceContext1_VSSetConstantBuffers(Context, 0, 1, &ConstantBuffer);
        ID3D11DeviceContext1_PSSetShader(Context, PixelShader, 0, 0);
        
        ID3D11DeviceContext1_RSSetState(Context, RasterizerStateDefault);
        
        // Draw entities
        
        ID3D11DeviceContext1_IASetPrimitiveTopology(Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext1_IASetVertexBuffers(Context, 0, 1, &Buffer, &Stride, &Offset);
        
        for(int Index = 0; Index < EntitiesAmount; ++Index) {
            entity* Entity = &Entities[Index];
            D3D11_MAPPED_SUBRESOURCE MappedSubresource;
            ID3D11DeviceContext1_Map(Context, (ID3D11Resource*)ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource);
            constants *Constants = (constants*)MappedSubresource.pData;
            Constants->Model = (matrix){
                1.0f, 0.0f, 0.0f, 0.0f, 
                0.0f, 1.0f, 0.0f, 0.0f, 
                0.0f, 0.0f, 1.0f, 0.0f, 
                Entity->Position.X, 
                Entity->Position.Y, 
                Entity->Position.Z, 1.0f
            };
            
            Constants->View = ViewMatrix;
            Constants->Projection = ProjectionMatrix;
            Constants->Color = Entity->Color;
            ID3D11DeviceContext1_Unmap(Context, (ID3D11Resource*)ConstantBuffer, 0);
            ID3D11DeviceContext1_Draw(Context, NumVertices, 0);
        }
        
        // Draw snake segment borders with lines
        
        ID3D11DeviceContext1_IASetPrimitiveTopology(Context, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        ID3D11DeviceContext1_IASetVertexBuffers(Context, 0, 1, &BorderBuffer, &BorderStride, &BorderOffset);
        
        for(int Index = HeadIndex; Index < EntitiesAmount; ++Index) {
            entity* Entity = &Entities[Index];
            D3D11_MAPPED_SUBRESOURCE MappedSubresource;
            ID3D11DeviceContext1_Map(Context, (ID3D11Resource*)ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource);
            constants *Constants = (constants*)MappedSubresource.pData;
            Constants->Model = (matrix){
                1.0f, 0.0f, 0.0f, 0.0f, 
                0.0f, 1.0f, 0.0f, 0.0f, 
                0.0f, 0.0f, 1.0f, 0.0f, 
                Entity->Position.X, 
                Entity->Position.Y, 
                Entity->Position.Z, 1.0f
            };
            
            Constants->View = ViewMatrix;
            Constants->Projection = ProjectionMatrix;
            Constants->Color = ColorBGPlatform;
            ID3D11DeviceContext1_Unmap(Context, (ID3D11Resource*)ConstantBuffer, 0);
            ID3D11DeviceContext1_Draw(Context, BorderNumVertices, 0);
        }
        
        // Draw wireframe
        
        if(EnableWireframe) {
            
            ID3D11DeviceContext1_IASetPrimitiveTopology(Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ID3D11DeviceContext1_IASetVertexBuffers(Context, 0, 1, 
                                                    &Buffer, 
                                                    &Stride, 
                                                    &Offset);
            
            ID3D11DeviceContext1_RSSetState(Context, RasterizerStateWireframe);
            
            for(int Index = 0; Index < EntitiesAmount; ++Index) {
                entity* Entity = &Entities[Index];
                D3D11_MAPPED_SUBRESOURCE MappedSubresource;
                ID3D11DeviceContext1_Map(Context, (ID3D11Resource*)ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource);
                constants *Constants = (constants*)MappedSubresource.pData;
                Constants->Model = (matrix){
                    1.0f, 0.0f, 0.0f, 0.0f, 
                    0.0f, 1.0f, 0.0f, 0.0f, 
                    0.0f, 0.0f, 1.0f, 0.0f, 
                    Entity->Position.X, 
                    Entity->Position.Y, 
                    Entity->Position.Z, 1.0f
                };
                
                Constants->View = ViewMatrix;
                Constants->Projection = ProjectionMatrix;
                Constants->Color = ColorWireframe;
                ID3D11DeviceContext1_Unmap(Context, (ID3D11Resource*)ConstantBuffer, 0);
                ID3D11DeviceContext1_Draw(Context, NumVertices, 0);
            }
        }
        
        // Swap
        
        IDXGISwapChain1_Present(SwapChain, 1, 0);
        
    }
    
    return 0;
}
