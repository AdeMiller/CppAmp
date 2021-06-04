//===============================================================================
//
// Microsoft Press
// C++ AMP: Accelerated Massive Parallelism with Microsoft Visual C++
//
//===============================================================================
// Copyright (c) 2012-2013 Ade Miller & Kate Gregory.  All rights reserved.
// This code released under the terms of the 
// Microsoft Public License (Ms-PL), http://ampbook.codeplex.com/license.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//===============================================================================

#include <memory>
#include <string>
#include <deque>
#include <numeric>
#include <future>
#include <d3dx11.h>
#include <commdlg.h>
#include <atlbase.h>

#include "DXUT.h"
#include "DXUTgui.h"
#include "SDKmisc.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"

#include "NBodyAmp.h"
#include "NBodyAmpSimple.h"
#include "NBodyAmpTiled.h"
#include "NBodyAmpMultiTiled.h"
#include "resource.h"

enum ComputeType
{
    kSingleSimple = 0,
    kSingleTile64,
    kSingleTile128,
    kSingleTile256,
    kSingleTile512,

    kMultiTile = 5,
    kMultiTile64 = 5,
    kMultiTile128,
    kMultiTile256,
    kMultiTile512
};

//--------------------------------------------------------------------------------------
// Global constants.
//--------------------------------------------------------------------------------------

const float g_softeningSquared =    0.0000015625f;
const float g_dampingFactor =       0.9995f;
const float g_particleMass =        ((6.67300e-11f*10000.0f)*10000.0f*10000.0f);
const float g_deltaTime =           0.1f;

const int g_maxParticles =          (57*1024);                  // Maximum number of particles in the n-body simulation.
const int g_particleNumStepSize =   512;                        // Number of particles added for each slider tick, cannot be less than the max tile size.

const float g_Spread =              400.0f;                     // Separation between the two clusters.

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

CDXUTDialogResourceManager          g_dialogResourceManager;    // manager for shared resources of dialogs
CModelViewerCamera                  g_camera;                   // A model viewing camera
CD3DSettingsDlg                     g_d3dSettingsDlg;           // Device settings dialog
CDXUTDialog                         g_HUD;                      // dialog for standard controls
CDXUTDialog                         g_sampleUI;                 // dialog for sample specific controls
std::unique_ptr<CDXUTTextHelper>    g_pTxtHelper;
std::deque<float>                   g_FpsStatistics;

CComPtr<ID3D11VertexShader>         g_pRenderParticlesVS;
CComPtr<ID3D11GeometryShader>       g_pRenderParticlesGS;
CComPtr<ID3D11PixelShader>          g_pRenderParticlesPS;
CComPtr<ID3D11SamplerState>         g_pSampleStateLinear;
CComPtr<ID3D11BlendState>           g_pBlendingStateParticle;
CComPtr<ID3D11DepthStencilState>    g_pDepthStencilState;

CComPtr<ID3D11Buffer>               g_pParticlePosOld;
CComPtr<ID3D11Buffer>               g_pParticlePosNew;
CComPtr<ID3D11ShaderResourceView>   g_pParticlePosRvOld;
CComPtr<ID3D11ShaderResourceView>   g_pParticlePosRvNew;
CComPtr<ID3D11UnorderedAccessView>  g_pParticlePosUavOld;
CComPtr<ID3D11UnorderedAccessView>  g_pParticlePosUavNew;

CComPtr<ID3D11Buffer>               g_pParticleBuffer;
CComPtr<ID3D11InputLayout>          g_pParticleVertexLayout;

CComPtr<ID3D11Buffer>               g_pConstantBuffer;

CComPtr<ID3D11ShaderResourceView>   g_pShaderResView;

//--------------------------------------------------------------------------------------
// Nbody functionality 
//--------------------------------------------------------------------------------------

#if !(defined(DEBUG) || defined(_DEBUG))
int                                 g_numParticles = (20 * 1024);           // The current number of particles in the n-body simulation
#else
int                                 g_numParticles = g_particleNumStepSize;
#endif
ComputeType                         g_eComputeType = kSingleSimple;         // Default integrator compute type
std::shared_ptr<INBodyAmp>          g_pNBody;                               // The current integrator

//  Particle data structures.

std::vector<std::shared_ptr<TaskData>> g_deviceData;

//  Particle colours.
D3DXCOLOR                           g_particleColor;
std::vector<D3DCOLOR>               g_particleColors;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------

#define IDC_TOGGLEFULLSCREEN        1
#define IDC_TOGGLEREF               3
#define IDC_CHANGEDEVICE            4
#define IDC_RESETPARTICLES          5

#define IDC_COMPUTETYPECOMBO        6
#define IDC_NBODIES_LABEL           7
#define IDC_NBODIES_SLIDER          8
#define IDC_NBODIES_TEXT            9
#define IDC_FPS_TEXT                10

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------

bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CorrectNumberOfParticles();
inline void SetBodyText();
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext );
void InitApp();
void RenderText();

//--------------------------------------------------------------------------------------
// Helper function to compile an hlsl shader from file, 
// its binary compiled code is returned
//--------------------------------------------------------------------------------------

HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    CComPtr<ID3DBlob> pErrorBlob = nullptr;
    D3DX11CompileFromFile(str, nullptr, nullptr, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_DEBUG, 0, nullptr, ppBlobOut, &pErrorBlob, nullptr);
    if ( FAILED(hr) )
    {
        if (pErrorBlob != nullptr)
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
    }    

    return hr;
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	// NOTE: This application leaks memory on shutdown due DLL unload ordering issues.
	//
	// {619} normal block at 0x00000011ADD99E10, 152 bytes long.
	//  Data: <                > 10 9F D9 AD 11 00 00 00 E0 B6 CE AF 11 00 00 00 
	//
	// See the C++ AMP blog for further details.

    // Enable run-time memory check for debug builds.
#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();

    //DXUTInit( true, true, L"-forceref" ); // Force Create a ref device so that feature level D3D_FEATURE_LEVEL_11_0 is guaranteed
    DXUTInit( true, true );                 // Use this line instead to try to Create a hardware device

    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"C++ AMP N-Body Simulation Demo" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1280, 800 );
    DXUTMainLoop();                      // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------

void InitApp()
{
    g_d3dSettingsDlg.Init( &g_dialogResourceManager );
    g_HUD.Init( &g_dialogResourceManager );
    g_sampleUI.Init( &g_dialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); 
    int y = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, y, 170, 23 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, y += 26, 170, 23, VK_F2 );
    g_HUD.AddButton( IDC_RESETPARTICLES, L"Reset particles", 0, y += 26, 170, 22, VK_F2 );

    WCHAR szTemp[256];
    swprintf_s( szTemp, L"Bodies: %d", g_numParticles );
    g_HUD.AddStatic( IDC_NBODIES_LABEL, szTemp, -20, y += 34, 125, 22 );
    g_HUD.AddSlider( IDC_NBODIES_SLIDER, -20, y += 34, 170, 22, 1, g_maxParticles/g_particleNumStepSize );
    CDXUTComboBox* pComboBox = nullptr;
    g_HUD.AddComboBox( IDC_COMPUTETYPECOMBO, -133, y += 34, 300, 26, L'G', false, &pComboBox );

    // The ordering of these names must match the FrameProcessorType enumeration.

    std::wstring processorNames[] =
    {
        std::wstring(L"C++ AMP Simple Model "),                // kCpuSingle
        std::wstring(L"C++ AMP Tiled Model 64 "),  
        std::wstring(L"C++ AMP Tiled Model 128 "),
        std::wstring(L"C++ AMP Tiled Model 256 "),
        std::wstring(L"C++ AMP Tiled Model 512 "),             // kSingleTile512
        std::wstring(L"C++ AMP Tiled Model 64: xx GPUs"),      // kMultiTile64
        std::wstring(L"C++ AMP Tiled Model 128:xx GPUs"),
        std::wstring(L"C++ AMP Tiled Model 256:xx GPUs"),
        std::wstring(L"C++ AMP Tiled Model 512:xx GPUs")       // kMultiTile512
    };

    WCHAR buf[3];
    if (_itow_s(static_cast<int>(AmpUtils::GetGpuAccelerators().size()), buf, 3, 10) == 0)
        for (int i = kMultiTile64; i <= kMultiTile512; ++i)
            processorNames[i].replace(24, 2, buf);
    std::wstring path = accelerator(accelerator::default_accelerator).device_path;

    //  If there is a GPU accelerator then use it. 
    //  Otherwise add a REF accelerator and display warning.

    for (int i = kSingleSimple; i <= kSingleTile512; ++i)
        pComboBox->AddItem(processorNames[i].c_str(), nullptr);
    g_eComputeType = kSingleTile256;

    //  If there us more than one GPU then allow the user to use them together.

    if (AmpUtils::GetGpuAccelerators().size() >= 2)
    {
        for (int i = kMultiTile64; i <= kMultiTile512; ++i)
            pComboBox->AddItem(processorNames[i].c_str(), nullptr);
        g_eComputeType = kMultiTile256;
    }
     
    g_HUD.GetComboBox( IDC_COMPUTETYPECOMBO )->SetSelectedByData((void*)g_eComputeType);
    pComboBox->SetSelectedByIndex(g_eComputeType);

    g_HUD.GetSlider( IDC_NBODIES_SLIDER )->SetValue( (g_numParticles / g_particleNumStepSize) );
    g_particleColors.resize(kMultiTile512 + 1);
    g_particleColors[kSingleSimple] = D3DXCOLOR( 0.05f, 1.0f, 0.05f, 1.0f );
    g_particleColors[kSingleTile64] = D3DXCOLOR( 0.05f, 1.0f, 0.05f, 1.0f );
    g_particleColors[kSingleTile128] = D3DXCOLOR( 0.05f, 1.0f, 0.05f, 1.0f );
    g_particleColors[kSingleTile256] = D3DXCOLOR( 0.05f, 1.0f, 0.05f, 1.0f );
    g_particleColors[kSingleTile512] = D3DXCOLOR( 0.05f, 1.0f, 0.05f, 1.0f );
    g_particleColors[kMultiTile64] = D3DXCOLOR( 0.05f, 0.05f, 1.0f, 1.0f );
    g_particleColors[kMultiTile128] = D3DXCOLOR( 0.05f, 0.05f, 1.0f, 1.0f );
    g_particleColors[kMultiTile256] = D3DXCOLOR( 0.05f, 0.05f, 1.0f, 1.0f );
    g_particleColors[kMultiTile512] = D3DXCOLOR( 0.05f, 0.05f, 1.0f, 1.0f );
    g_particleColor = g_particleColors[g_eComputeType];

    g_sampleUI.SetCallback( OnGUIEvent );

#if (defined(DEBUG) || defined(_DEBUG))
    if (AmpUtils::GetGpuAccelerators().empty())
        MessageBox(DXUTGetHWND(), L"No C++ AMP GPU hardware accelerator detected,\nusing the REF or WARP accelerator.\n\nTo see better performance run on C++ AMP\nenabled hardware.", 
            L"No C++ AMP Hardware Accelerator Detected", 
            MB_ICONEXCLAMATION);
#endif

#ifdef FORCE_WARP
    //  Force use of the Warp accelerator, even if a better GPU exists. For testing only.
    accelerator::set_default(accelerator::direct3d_warp);
    OutputDebugStringW(L"Forcing application to use the WARP accelerator");
#endif
}

//--------------------------------------------------------------------------------------
//  Create particle buffers for use during rendering.
//--------------------------------------------------------------------------------------

HRESULT CreateParticleBuffer( ID3D11Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    D3D11_BUFFER_DESC bufferDesc =
    {
        g_maxParticles * sizeof( ParticleVertex ),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0,
        0
    };
    D3D11_SUBRESOURCE_DATA resourceData;
    ZeroMemory( &resourceData, sizeof( D3D11_SUBRESOURCE_DATA ) );

    std::vector<ParticleVertex> vertices(g_maxParticles);
    std::for_each(vertices.begin(), vertices.end(), [](ParticleVertex& v){ v.color = D3DXCOLOR( 1, 1, 0.2f, 1 ); });

    resourceData.pSysMem = &vertices[0];
    g_pParticleBuffer = nullptr;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &resourceData, &g_pParticleBuffer ) );

    return hr;
}

//--------------------------------------------------------------------------------------
//  Load particles. Two clusters set to collide.
//--------------------------------------------------------------------------------------

void LoadParticles()
{
    const float centerSpread = g_Spread * 0.50f;

    // Create particles in CPU memory.

    ParticlesCpu particles(g_maxParticles);

    for (int i = 0; i < g_maxParticles; i += g_particleNumStepSize)
    {
        LoadClusterParticles(particles, i, (g_particleNumStepSize / 2),
            float_3(centerSpread, 0.0f, 0.0f), 
            float_3( 0, 0, -20),
            g_Spread);
        LoadClusterParticles(particles, (i + g_particleNumStepSize / 2), ((g_particleNumStepSize + 1) / 2),
            float_3(-centerSpread, 0.0f, 0.0f), 
            float_3( 0, 0, 20),
            g_Spread);
    }       

    // Copy particles to GPU memory.

    index<1> begin(0); 
    extent<1> end(g_maxParticles); 
    for (size_t i = 0; i < g_deviceData.size(); ++i)
    {
        array_view<float_3, 1> posView = g_deviceData[i]->DataOld->pos.section(index<1>(begin), extent<1>(end));
        copy(particles.pos.begin(), posView);
        array_view<float_3, 1> velView = g_deviceData[i]->DataOld->vel.section(index<1>(begin), extent<1>(end));
        copy(particles.vel.begin(), velView);
    }
}

//--------------------------------------------------------------------------------------
//  Integrator class factory. 
//--------------------------------------------------------------------------------------

std::shared_ptr<INBodyAmp> NBodyFactory(ComputeType type)
{
    switch (type)
    {
    case kSingleSimple:
        return std::make_shared<NBodyAmpSimple>(g_softeningSquared, g_dampingFactor, 
            g_deltaTime, g_particleMass);
        break;
    case kSingleTile64:
        return std::make_shared<NBodyAmpTiled<64>>(g_softeningSquared, g_dampingFactor, 
            g_deltaTime, g_particleMass);
        break;
    case kSingleTile128:
        return std::make_shared<NBodyAmpTiled<128>>(g_softeningSquared, g_dampingFactor, 
            g_deltaTime, g_particleMass);
        break;
    case kSingleTile256:
        return std::make_shared<NBodyAmpTiled<256>>(g_softeningSquared, g_dampingFactor, 
            g_deltaTime, g_particleMass);
        break;
    case kSingleTile512:
        return std::make_shared<NBodyAmpTiled<512>>(g_softeningSquared, g_dampingFactor, 
            g_deltaTime, g_particleMass);
        break;
    case kMultiTile64:
        return std::make_shared<NBodyAmpMultiTiled<64>>(g_softeningSquared, g_dampingFactor, 
            g_deltaTime, g_particleMass, g_maxParticles);
        break;
    case kMultiTile128:
        return std::make_shared<NBodyAmpMultiTiled<128>>(g_softeningSquared, g_dampingFactor, 
            g_deltaTime, g_particleMass, g_maxParticles);
        break;
    case kMultiTile256:
        return std::make_shared<NBodyAmpMultiTiled<256>>(g_softeningSquared, g_dampingFactor, 
            g_deltaTime, g_particleMass, g_maxParticles);
        break;
    case kMultiTile512:
        return std::make_shared<NBodyAmpMultiTiled<512>>(g_softeningSquared, g_dampingFactor, 
            g_deltaTime, g_particleMass, g_maxParticles);
        break;
    default:
        assert(false);
        return nullptr;
        break;
    }
}

//--------------------------------------------------------------------------------------
//  Create buffers and hook them up to DirectX.
//--------------------------------------------------------------------------------------

HRESULT CreateParticlePosBuffer(ID3D11Device* pd3dDevice)
{
    HRESULT hr = S_OK;   
    accelerator_view renderView = 
        concurrency::direct3d::create_accelerator_view(reinterpret_cast<IUnknown*>(pd3dDevice));
    g_deviceData = CreateTasks(g_maxParticles, renderView);
    LoadParticles();

    g_pParticlePosOld = nullptr;
    g_pParticlePosNew = nullptr;
    //  Particles from GPU zero are the ones synced with the graphics buffers. 
    //  Attach AMP array of positions to D3D buffer.
    hr = concurrency::direct3d::get_buffer(
        g_deviceData[0]->DataOld->pos)->QueryInterface(__uuidof(ID3D11Buffer), 
        reinterpret_cast<LPVOID*>(&g_pParticlePosOld));     
    V_RETURN(hr);
    hr = concurrency::direct3d::get_buffer(
        g_deviceData[0]->DataNew->pos)->QueryInterface(__uuidof(ID3D11Buffer), 
        reinterpret_cast<LPVOID*>(&g_pParticlePosNew));
    V_RETURN(hr)
    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    ZeroMemory(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    resourceDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    resourceDesc.BufferEx.FirstElement = 0;
    resourceDesc.BufferEx.NumElements = (g_maxParticles * sizeof(float_3)) / sizeof(float);
    resourceDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    g_pParticlePosRvOld = nullptr;
    g_pParticlePosRvNew = nullptr;
    hr = pd3dDevice->CreateShaderResourceView(g_pParticlePosOld, &resourceDesc, &g_pParticlePosRvOld);
    V_RETURN(hr);
    hr = pd3dDevice->CreateShaderResourceView(g_pParticlePosNew, &resourceDesc, &g_pParticlePosRvNew);
    V_RETURN(hr);

    g_pParticlePosUavOld = nullptr;
    g_pParticlePosUavNew = nullptr;
    D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc;
    ZeroMemory(&viewDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
    viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    viewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    viewDesc.Buffer.FirstElement = 0;
    viewDesc.Buffer.NumElements = (g_maxParticles * sizeof(float_3)) / sizeof(float);
    viewDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    hr = pd3dDevice->CreateUnorderedAccessView(g_pParticlePosOld, &viewDesc, &g_pParticlePosUavOld);
    V_RETURN(hr);
    hr = pd3dDevice->CreateUnorderedAccessView(g_pParticlePosNew, &viewDesc, &g_pParticlePosUavNew);
    V_RETURN(hr);

    return hr;
}

//--------------------------------------------------------------------------------------
//  Create render buffer. 
//--------------------------------------------------------------------------------------

bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( pDeviceSettings->ver == DXUT_D3D11_DEVICE );

    // Disable vsync
    pDeviceSettings->d3d11.SyncInterval = 0;
    g_d3dSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );    

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_IsFirstTime = true;
    if( s_IsFirstTime )
    {
        s_IsFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------

void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
    g_pNBody->Integrate(g_deviceData, g_numParticles);
    std::for_each(g_deviceData.begin(), g_deviceData.end(), [](std::shared_ptr<TaskData>& t) 
    { 
        std::swap(t->DataOld, t->DataNew); 
    });
    std::swap(g_pParticlePosOld, g_pParticlePosNew);
    std::swap(g_pParticlePosRvOld, g_pParticlePosRvNew);
    std::swap(g_pParticlePosUavOld, g_pParticlePosUavNew);

    // Update the camera's position based on user input 
    g_camera.FrameMove(fElapsedTime);
}

//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------

LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext)
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_dialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_d3dSettingsDlg.IsActive() )
    {
        g_d3dSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_sampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera so it can respond to user input
    g_camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------

void SetBodyText();

void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
    case IDC_TOGGLEFULLSCREEN:
        DXUTToggleFullScreen(); 
        break;
    case IDC_CHANGEDEVICE:
        g_d3dSettingsDlg.SetActive( !g_d3dSettingsDlg.IsActive() ); 
        break;
    case IDC_RESETPARTICLES:
        LoadParticles();
        break;    
    case IDC_COMPUTETYPECOMBO:
        {
            CDXUTComboBox* pComboBox = static_cast<CDXUTComboBox*>(pControl);
            g_eComputeType = static_cast<ComputeType>(pComboBox->GetSelectedIndex());

            g_particleColor = g_particleColors[g_eComputeType];
            g_pNBody = NBodyFactory(g_eComputeType);

            CorrectNumberOfParticles();
            SetBodyText();
            g_FpsStatistics.clear();
        }
        break;  
    case IDC_NBODIES_SLIDER:
        {
            CDXUTSlider* pSlider  = static_cast<CDXUTSlider*>(pControl);
            g_numParticles = pSlider->GetValue() * g_particleNumStepSize;

            CorrectNumberOfParticles();
            SetBodyText();
            g_FpsStatistics.clear();
        }
        break;
    }
}

// For the multi-accelerator integrator there must be at least one tile of particles per GPU.

void CorrectNumberOfParticles()
{
    const int minParticles = static_cast<int>(g_deviceData.size() * g_pNBody->TileSize());

    if ((g_eComputeType >= kMultiTile) && (g_numParticles < minParticles))
    {
        g_numParticles = minParticles;
        g_HUD.GetSlider( IDC_NBODIES_SLIDER )->SetValue( g_numParticles / g_particleNumStepSize );
    }
}

void SetBodyText()
{
    WCHAR szTemp[256];
    swprintf_s( szTemp, L"Bodies: %d", g_numParticles );    
    g_HUD.GetStatic( IDC_NBODIES_LABEL )->SetText( szTemp );
}

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
{
    // reject any device which doesn't support CS4x
    return (DeviceInfo->ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x != false);
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to Create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnD3D11DestroyDevice callback. 
//--------------------------------------------------------------------------------------

HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext)
{
    HRESULT hr = S_OK;

    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS ho;
    V_RETURN( pd3dDevice->CheckFeatureSupport( D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &ho, sizeof(ho) ) );

    CComPtr<ID3D11DeviceContext> pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_dialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_d3dSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = std::unique_ptr<CDXUTTextHelper>(new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_dialogResourceManager, 15 ));

    CComPtr<ID3DBlob> pBlobRenderParticlesVS;
    CComPtr<ID3DBlob> pBlobRenderParticlesGS;
    CComPtr<ID3DBlob> pBlobRenderParticlesPS;

    // Create the shaders
    V_RETURN( CompileShaderFromFile( L"ParticleDrawGpu.hlsl", "VSParticleDraw", "vs_4_0", &pBlobRenderParticlesVS) );
    V_RETURN( CompileShaderFromFile( L"ParticleDrawGpu.hlsl", "GSParticleDraw", "gs_4_0", &pBlobRenderParticlesGS) );
    V_RETURN( CompileShaderFromFile( L"ParticleDrawGpu.hlsl", "PSParticleDraw", "ps_4_0", &pBlobRenderParticlesPS) );
    g_pRenderParticlesVS = nullptr;
    g_pRenderParticlesGS = nullptr;
    g_pRenderParticlesPS = nullptr;
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobRenderParticlesVS->GetBufferPointer(), pBlobRenderParticlesVS->GetBufferSize(), nullptr, &g_pRenderParticlesVS ) );
    V_RETURN( pd3dDevice->CreateGeometryShader( pBlobRenderParticlesGS->GetBufferPointer(), pBlobRenderParticlesGS->GetBufferSize(), nullptr, &g_pRenderParticlesGS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobRenderParticlesPS->GetBufferPointer(), pBlobRenderParticlesPS->GetBufferSize(), nullptr, &g_pRenderParticlesPS ) );

    // Create our vertex input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pParticleVertexLayout = nullptr;
    V_RETURN( pd3dDevice->CreateInputLayout( layout, sizeof( layout ) / sizeof( layout[0] ),
        pBlobRenderParticlesVS->GetBufferPointer(), pBlobRenderParticlesVS->GetBufferSize(), &g_pParticleVertexLayout ) );

    // Create NBody object
    g_pNBody = NBodyFactory(g_eComputeType);
    V_RETURN(CreateParticleBuffer(pd3dDevice));
    V_RETURN(CreateParticlePosBuffer(pd3dDevice));

    // Setup constant buffer
    D3D11_BUFFER_DESC bufferDesc;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.ByteWidth = sizeof( ResourceData );
    g_pConstantBuffer = nullptr;
    V_RETURN( pd3dDevice->CreateBuffer(&bufferDesc, nullptr, &g_pConstantBuffer));

    // Load the Particle Texture
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"UI\\Particle.dds" ) );
    g_pShaderResView = nullptr;
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, nullptr, nullptr, &g_pShaderResView, nullptr ) );

    D3D11_SAMPLER_DESC samplerDesc;
    ZeroMemory( &samplerDesc, sizeof(samplerDesc) );
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    g_pSampleStateLinear = nullptr;
    V_RETURN(pd3dDevice->CreateSamplerState(&samplerDesc, &g_pSampleStateLinear));

    D3D11_BLEND_DESC blendStateDesc;
    ZeroMemory( &blendStateDesc, sizeof(blendStateDesc) );
    blendStateDesc.RenderTarget[0].BlendEnable = TRUE;
    blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F;
    g_pBlendingStateParticle = nullptr;
    V_RETURN(pd3dDevice->CreateBlendState( &blendStateDesc, &g_pBlendingStateParticle));

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
    ZeroMemory( &depthStencilDesc, sizeof(depthStencilDesc) );
    depthStencilDesc.DepthEnable = false;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    g_pDepthStencilState = nullptr;
    pd3dDevice->CreateDepthStencilState(&depthStencilDesc, &g_pDepthStencilState);

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( -g_Spread * 2, g_Spread * 4, -g_Spread * 3 );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_camera.SetViewParams( &vecEye, &vecAt );

    return S_OK;
}

HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    HRESULT hr = S_OK;

    V_RETURN( g_dialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_d3dSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float aspect = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_camera.SetProjParams( D3DX_PI / 4, aspect, 10.0f, 500000.0f );
    g_camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_LEFT_BUTTON | MOUSE_MIDDLE_BUTTON | MOUSE_RIGHT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_sampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_sampleUI.SetSize( 170, 300 );

    return hr;
}

void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
    g_dialogResourceManager.OnD3D11ReleasingSwapChain();    
}

//--------------------------------------------------------------------------------------
//  Create particle buffers for use during rendering.
//--------------------------------------------------------------------------------------

void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( false ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->SetInsertionPos( 20, 60 );
    g_pTxtHelper->DrawFormattedTextLine( L"Bodies: %d", g_numParticles );

    g_FpsStatistics.push_front(DXUTGetFPS());
    if (g_FpsStatistics.size() > 10)
        g_FpsStatistics.pop_back();

    const float fps = accumulate(g_FpsStatistics.begin(), g_FpsStatistics.end(), 0.0f) / g_FpsStatistics.size();

    // Estimate the number of FLOPs based on 20 FLOPs per particle-particle interaction.
    g_pTxtHelper->DrawFormattedTextLine( L"FPS:    %.2f", fps );
    const float gflops = (g_numParticles / 1000.0f) * (g_numParticles / 1000.0f) * fps * 20 / 1000.0f;
    g_pTxtHelper->DrawFormattedTextLine( L"GFlops: %.2f ", gflops );

    g_pTxtHelper->End();
}

bool RenderParticles(ID3D11DeviceContext* pd3dImmediateContext, D3DXMATRIX& view, D3DXMATRIX& projection)
{
    CComPtr<ID3D11BlendState> pBlendState0;
    CComPtr<ID3D11DepthStencilState> pDepthStencilState0;
    UINT SampleMask0, StencilRef0;
    D3DXCOLOR BlendFactor0;
    pd3dImmediateContext->OMGetBlendState( &pBlendState0, &BlendFactor0.r, &SampleMask0 );
    pd3dImmediateContext->OMGetDepthStencilState( &pDepthStencilState0, &StencilRef0 );

    pd3dImmediateContext->VSSetShader( g_pRenderParticlesVS, nullptr, 0 );
    pd3dImmediateContext->GSSetShader( g_pRenderParticlesGS, nullptr, 0 );
    pd3dImmediateContext->PSSetShader( g_pRenderParticlesPS, nullptr, 0 );

    pd3dImmediateContext->IASetInputLayout( g_pParticleVertexLayout );

    // Set IA parameters, don't need to pass arrays to IASetVertexBuffers as there is only one buffer.
    const UINT stride = sizeof(ParticleVertex);
    const UINT offset =  0;
    pd3dImmediateContext->IASetVertexBuffers(0, 1, &g_pParticleBuffer.p, &stride, &offset);
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );

    // Don't need to pass array to VSSetShaderResources as there is only one buffer.
    pd3dImmediateContext->VSSetShaderResources(0, 1, &g_pParticlePosRvOld.p);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    pd3dImmediateContext->Map( g_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    ResourceData* pCBGS = static_cast<ResourceData*>(mappedResource.pData); 
    D3DXMatrixMultiply( &pCBGS->worldViewProj, &view, &projection );   
    D3DXMatrixInverse( &pCBGS->inverseView, nullptr, &view );
    pCBGS->color = g_particleColor;
    pd3dImmediateContext->Unmap( g_pConstantBuffer, 0 );
    pd3dImmediateContext->GSSetConstantBuffers( 0, 1, &g_pConstantBuffer.p);
    pd3dImmediateContext->PSSetShaderResources( 0, 1, &g_pShaderResView.p);
    pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSampleStateLinear.p);

    pd3dImmediateContext->OMSetBlendState( g_pBlendingStateParticle, D3DXCOLOR( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF  );
    pd3dImmediateContext->OMSetDepthStencilState( g_pDepthStencilState, 0 );

    pd3dImmediateContext->Draw(g_numParticles, 0);

    ID3D11ShaderResourceView* ppSRVnullptr[1] = { nullptr };
    pd3dImmediateContext->VSSetShaderResources(0, 1, ppSRVnullptr);
    pd3dImmediateContext->PSSetShaderResources( 0, 1, ppSRVnullptr );

    pd3dImmediateContext->GSSetShader( nullptr, nullptr, 0 );
    pd3dImmediateContext->OMSetBlendState( pBlendState0, &BlendFactor0.r, SampleMask0 ); 
    pd3dImmediateContext->OMSetDepthStencilState( pDepthStencilState0, StencilRef0 ); 

    return true;
}

void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext)
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if (g_d3dSettingsDlg.IsActive())
    {
        g_d3dSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    const float clearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, clearColor );
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    D3DXMATRIX view;
    D3DXMATRIX projection;

    // Get the projection & view matrix from the camera class
    projection = *g_camera.GetProjMatrix();
    view = *g_camera.GetViewMatrix();

    // Render the particles
    RenderParticles( pd3dImmediateContext, view, projection );

    g_HUD.OnRender( fElapsedTime );
    g_sampleUI.OnRender( fElapsedTime );
    RenderText();
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnD3D11CreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------

void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
    g_dialogResourceManager.OnD3D11DestroyDevice();
    g_d3dSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
}
