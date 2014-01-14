#include <sstream>
#include <iomanip>
#include <random>
#include <iostream>

//DirectX includes
#include <DirectXMath.h>
using namespace DirectX;

// Effect framework includes
#include <d3dx11effect.h>

// DXUT includes
#include <DXUT.h>
#include <DXUTcamera.h>

// DirectXTK includes
#include "Effects.h"
#include "VertexTypes.h"
#include "PrimitiveBatch.h"
#include "GeometricPrimitive.h"
#include "ScreenGrab.h"

// AntTweakBar includes
#include "AntTweakBar.h"

// Internal includes
#include "util/util.h"

#include <vector>
#include "Sphere.h"

using namespace std;

vector<Sphere> spheres;
float sphereSize = 0.05f;
float lambda = 50.0f;

float timeStep = 1.0f/60;

const unsigned int hashTableSize = 5;
vector<int> hashTable[hashTableSize];
const unsigned int p1 = 83;
const unsigned int p2 = 89;
const unsigned int p3 = 97;

// DXUT camera
// NOTE: CModelViewerCamera does not only manage the standard view transformation/camera position 
//       (CModelViewerCamera::GetViewMatrix()), but also allows for model rotation
//       (CModelViewerCamera::GetWorldMatrix()). 
//       Look out for CModelViewerCamera::SetButtonMasks(...).
CModelViewerCamera g_camera;

// Effect corresponding to "effect.fx"
ID3DX11Effect* g_pEffect = nullptr;

// Main tweak bar
TwBar* g_pTweakBar;

// DirectXTK effect, input layout and primitive batch for position/color vertices
// (for drawing multicolored & unlit primitives)
BasicEffect*                          g_pEffectPositionColor          = nullptr;
ID3D11InputLayout*                    g_pInputLayoutPositionColor     = nullptr;
PrimitiveBatch<VertexPositionColor>*  g_pPrimitiveBatchPositionColor  = nullptr;

// DirectXTK effect, input layout and primitive batch for position/normal vertices
// (for drawing unicolor & oriented & lit primitives)
BasicEffect*                          g_pEffectPositionNormal         = nullptr;
ID3D11InputLayout*                    g_pInputLayoutPositionNormal    = nullptr;
PrimitiveBatch<VertexPositionNormal>* g_pPrimitiveBatchPositionNormal = nullptr;

// DirectXTK simple geometric primitives
std::unique_ptr<GeometricPrimitive> g_pSphere;
std::unique_ptr<GeometricPrimitive> g_pTeapot;

// Movable object management
XMINT2   g_viMouseDelta = XMINT2(0,0);
XMFLOAT3 g_vfMovableObjectPos = XMFLOAT3(0,0,0);

// TweakAntBar GUI variables
int   g_iNumSpheres    = 100;
float g_fSphereSize    = 0.05f;
bool  g_bDrawTeapot    = false;
bool  g_bDrawTriangle  = false;
bool  g_bDrawSpheres   = false;

void TW_CALL spawnSpheres(void *clientData)
{
	Sphere s(0, 0, 0);
	spheres.push_back(s);
}

// Create TweakBar and add required buttons and variables
void InitTweakBar(ID3D11Device* pd3dDevice)
{
    TwInit(TW_DIRECT3D11, pd3dDevice);

    g_pTweakBar = TwNewBar("TweakBar");

    // HINT: For buttons you can directly pass the callback function as a lambda expression.
    TwAddButton(g_pTweakBar, "Reset Camera", [](void *){g_camera.Reset();}, nullptr, "");

    TwAddVarRW(g_pTweakBar, "Draw Teapot",   TW_TYPE_BOOLCPP, &g_bDrawTeapot, "");
    TwAddVarRW(g_pTweakBar, "Draw Triangle", TW_TYPE_BOOLCPP, &g_bDrawTriangle, "");
    TwAddVarRW(g_pTweakBar, "Draw Spheres",  TW_TYPE_BOOLCPP, &g_bDrawSpheres, "");
    TwAddVarRW(g_pTweakBar, "Num Spheres",   TW_TYPE_INT32, &g_iNumSpheres, "min=1");
    TwAddVarRW(g_pTweakBar, "Sphere Size",   TW_TYPE_FLOAT, &g_fSphereSize, "min=0.01 step=0.01");
	TwAddButton(g_pTweakBar, "Spawn Sphere", spawnSpheres, NULL, NULL);
}

// Draw the edges of the bounding box [-0.5;0.5]³ rotated with the cameras model tranformation.
// (Drawn as line primitives using a DirectXTK primitive batch)
void DrawBoundingBox(ID3D11DeviceContext* pd3dImmediateContext)
{
    // Setup position/color effect
    g_pEffectPositionColor->SetWorld(g_camera.GetWorldMatrix());
    
    g_pEffectPositionColor->Apply(pd3dImmediateContext);
    pd3dImmediateContext->IASetInputLayout(g_pInputLayoutPositionColor);

    // Draw
    g_pPrimitiveBatchPositionColor->Begin();
    
    // Lines in x direction (red color)
    for (int i=0; i<4; i++)
    {
        g_pPrimitiveBatchPositionColor->DrawLine(
            VertexPositionColor(XMVectorSet(-0.5f, (float)(i%2)-0.5f, (float)(i/2)-0.5f, 1), Colors::Red),
            VertexPositionColor(XMVectorSet( 0.5f, (float)(i%2)-0.5f, (float)(i/2)-0.5f, 1), Colors::Red)
        );
    }

    // Lines in y direction
    for (int i=0; i<4; i++)
    {
        g_pPrimitiveBatchPositionColor->DrawLine(
            VertexPositionColor(XMVectorSet((float)(i%2)-0.5f, -0.5f, (float)(i/2)-0.5f, 1), Colors::Green),
            VertexPositionColor(XMVectorSet((float)(i%2)-0.5f,  0.5f, (float)(i/2)-0.5f, 1), Colors::Green)
        );
    }

    // Lines in z direction
    for (int i=0; i<4; i++)
    {
        g_pPrimitiveBatchPositionColor->DrawLine(
            VertexPositionColor(XMVectorSet((float)(i%2)-0.5f, (float)(i/2)-0.5f, -0.5f, 1), Colors::Blue),
            VertexPositionColor(XMVectorSet((float)(i%2)-0.5f, (float)(i/2)-0.5f,  0.5f, 1), Colors::Blue)
        );
    }

    g_pPrimitiveBatchPositionColor->End();
}

// Draw a large, square plane at y=-1.
// (Drawn as a quad, i.e. triangle strip, using a DirectXTK primitive batch)
void DrawFloor(ID3D11DeviceContext* pd3dImmediateContext)
{
    // Setup position/normal effect
    g_pEffectPositionNormal->SetWorld(XMMatrixIdentity());
    g_pEffectPositionNormal->SetEmissiveColor(Colors::Black);
    g_pEffectPositionNormal->SetDiffuseColor(0.6f * Colors::White);
    g_pEffectPositionNormal->SetSpecularColor(0.4f * Colors::White);
    g_pEffectPositionNormal->SetSpecularPower(1000);
    
    g_pEffectPositionNormal->Apply(pd3dImmediateContext);
    pd3dImmediateContext->IASetInputLayout(g_pInputLayoutPositionNormal);

    // Draw
    g_pPrimitiveBatchPositionNormal->Begin();
    g_pPrimitiveBatchPositionNormal->DrawQuad(
        VertexPositionNormal(XMFLOAT3(-100, -1,  100), XMFLOAT3(0,1,0)),
        VertexPositionNormal(XMFLOAT3( 100, -1,  100), XMFLOAT3(0,1,0)),
        VertexPositionNormal(XMFLOAT3( 100, -1, -100), XMFLOAT3(0,1,0)),
        VertexPositionNormal(XMFLOAT3(-100, -1, -100), XMFLOAT3(0,1,0))
    );
    g_pPrimitiveBatchPositionNormal->End();    
}

// Draw several objects randomly positioned in [-0.5f;0.5]³  using DirectXTK geometric primitives.
void DrawSomeRandomObjects(ID3D11DeviceContext* pd3dImmediateContext)
{
    // Setup position/normal effect (constant variables)
    g_pEffectPositionNormal->SetEmissiveColor(Colors::Black);
    g_pEffectPositionNormal->SetSpecularColor(0.4f * Colors::White);
    g_pEffectPositionNormal->SetSpecularPower(100);
      
    std::mt19937 eng;
    std::uniform_real_distribution<float> randCol( 0.0f, 1.0f);
    std::uniform_real_distribution<float> randPos(-0.5f, 0.5f);

    for (int i=0; i<g_iNumSpheres; i++)
    {
        // Setup position/normal effect (per object variables)
        g_pEffectPositionNormal->SetDiffuseColor(0.6f * XMColorHSVToRGB(XMVectorSet(randCol(eng), 1, 1, 0)));
        XMMATRIX scale    = XMMatrixScaling(g_fSphereSize, g_fSphereSize, g_fSphereSize);
        XMMATRIX trans    = XMMatrixTranslation(randPos(eng),randPos(eng),randPos(eng));
        g_pEffectPositionNormal->SetWorld(scale * trans * g_camera.GetWorldMatrix());

        // Draw
        // NOTE: The following generates one draw call per object, so performance will be bad for n>>1000 or so
        g_pSphere->Draw(g_pEffectPositionNormal, g_pInputLayoutPositionNormal);
    }
}

// Draw a teapot at the position g_vfMovableObjectPos.
void DrawMovableTeapot(ID3D11DeviceContext* pd3dImmediateContext)
{
    // Setup position/normal effect (constant variables)
    g_pEffectPositionNormal->SetEmissiveColor(Colors::Black);
    g_pEffectPositionNormal->SetDiffuseColor(0.6f * Colors::Cornsilk);
    g_pEffectPositionNormal->SetSpecularColor(0.4f * Colors::White);
    g_pEffectPositionNormal->SetSpecularPower(100);

    XMMATRIX scale    = XMMatrixScaling(0.5f, 0.5f, 0.5f);    
    XMMATRIX trans    = XMMatrixTranslation(g_vfMovableObjectPos.x, g_vfMovableObjectPos.y, g_vfMovableObjectPos.z);
    g_pEffectPositionNormal->SetWorld(scale * trans);

    // Draw
    g_pTeapot->Draw(g_pEffectPositionNormal, g_pInputLayoutPositionNormal);
}

void DrawSpheres(ID3D11DeviceContext* pd3dImmediateContext)
{
    // Setup position/normal effect (constant variables)
    g_pEffectPositionNormal->SetEmissiveColor(Colors::Black);
    g_pEffectPositionNormal->SetSpecularColor(0.4f * Colors::White);
    g_pEffectPositionNormal->SetSpecularPower(100);
      
    std::mt19937 eng;
    std::uniform_real_distribution<float> randCol( 0.0f, 1.0f);
    std::uniform_real_distribution<float> randPos(-0.5f, 0.5f);

    for (unsigned int i = 0; i < spheres.size(); i++)
    {
        // Setup position/normal effect (per object variables)
        g_pEffectPositionNormal->SetDiffuseColor(0.6f * XMColorHSVToRGB(XMVectorSet(randCol(eng), 1, 1, 0)));
		/*if (selectedPoint == i)
		{
			XMFLOAT4 white;
			white.x = white.y = white.z = white.w = 1.0f;
			g_pEffectPositionNormal->SetDiffuseColor(XMLoadFloat4(&white));
		}*/
        XMMATRIX scale    = XMMatrixScaling(sphereSize, sphereSize, sphereSize);
		Vec3 position = spheres[i].getPosition();
        XMMATRIX trans    = XMMatrixTranslation(position.x, position.y, position.z);
        g_pEffectPositionNormal->SetWorld(scale * trans * g_camera.GetWorldMatrix());

        // Draw
        // NOTE: The following generates one draw call per object, so performance will be bad for n>>1000 or so
        g_pSphere->Draw(g_pEffectPositionNormal, g_pInputLayoutPositionNormal);
    }
}


// Draw a simple triangle using custom shaders (g_pEffect)
void DrawTriangle(ID3D11DeviceContext* pd3dImmediateContext)
{
	XMMATRIX world = g_camera.GetWorldMatrix();
	XMMATRIX view  = g_camera.GetViewMatrix();
	XMMATRIX proj  = g_camera.GetProjMatrix();
	XMFLOAT4X4 mViewProj;
	XMStoreFloat4x4(&mViewProj, world * view * proj);
	g_pEffect->GetVariableByName("g_worldViewProj")->AsMatrix()->SetMatrix((float*)mViewProj.m);
	g_pEffect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0, pd3dImmediateContext);
    
	pd3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	pd3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pd3dImmediateContext->Draw(3, 0);
}


// ============================================================
// DXUT Callbacks
// ============================================================


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
	return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
	return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependent on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
	HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();;

    std::wcout << L"Device: " << DXUTGetDeviceStats() << std::endl;
    
    // Load custom effect from "effect.fxo" (compiled "effect.fx")
	std::wstring effectPath = GetExePath() + L"effect.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &g_pEffect)))
	{
        std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

    // Init AntTweakBar GUI
    InitTweakBar(pd3dDevice);

    // Create DirectXTK geometric primitives for later usage
    g_pSphere = GeometricPrimitive::CreateGeoSphere(pd3dImmediateContext, 2.0f, 2, false);
    g_pTeapot = GeometricPrimitive::CreateTeapot(pd3dImmediateContext, 1.5f, 8, false);

    // Create effect, input layout and primitive batch for position/color vertices (DirectXTK)
    {
        // Effect
        g_pEffectPositionColor = new BasicEffect(pd3dDevice);
        g_pEffectPositionColor->SetVertexColorEnabled(true); // triggers usage of position/color vertices

        // Input layout
        void const* shaderByteCode;
        size_t byteCodeLength;
        g_pEffectPositionColor->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
        
        pd3dDevice->CreateInputLayout(VertexPositionColor::InputElements,
                                      VertexPositionColor::InputElementCount,
                                      shaderByteCode, byteCodeLength,
                                      &g_pInputLayoutPositionColor);

        // Primitive batch
        g_pPrimitiveBatchPositionColor = new PrimitiveBatch<VertexPositionColor>(pd3dImmediateContext);
    }

    // Create effect, input layout and primitive batch for position/normal vertices (DirectXTK)
    {
        // Effect
        g_pEffectPositionNormal = new BasicEffect(pd3dDevice);
        g_pEffectPositionNormal->EnableDefaultLighting(); // triggers usage of position/normal vertices
        g_pEffectPositionNormal->SetPerPixelLighting(true);

        // Input layout
        void const* shaderByteCode;
        size_t byteCodeLength;
        g_pEffectPositionNormal->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        pd3dDevice->CreateInputLayout(VertexPositionNormal::InputElements,
                                      VertexPositionNormal::InputElementCount,
                                      shaderByteCode, byteCodeLength,
                                      &g_pInputLayoutPositionNormal);

        // Primitive batch
        g_pPrimitiveBatchPositionNormal = new PrimitiveBatch<VertexPositionNormal>(pd3dImmediateContext);
    }

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
	SAFE_RELEASE(g_pEffect);
	
    TwDeleteBar(g_pTweakBar);
    g_pTweakBar = nullptr;
	TwTerminate();

    g_pSphere.reset();
    g_pTeapot.reset();
    
    SAFE_DELETE (g_pPrimitiveBatchPositionColor);
    SAFE_RELEASE(g_pInputLayoutPositionColor);
    SAFE_DELETE (g_pEffectPositionColor);

    SAFE_DELETE (g_pPrimitiveBatchPositionNormal);
    SAFE_RELEASE(g_pInputLayoutPositionNormal);
    SAFE_DELETE (g_pEffectPositionNormal);
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    // Update camera parameters
	int width = pBackBufferSurfaceDesc->Width;
	int height = pBackBufferSurfaceDesc->Height;
	g_camera.SetWindow(width, height);
	g_camera.SetProjParams(XM_PI / 4.0f, float(width) / float(height), 0.1f, 100.0f);

    // Inform AntTweakBar about back buffer resolution change
  	TwWindowSize(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
	if(bKeyDown)
	{
		switch(nChar)
		{
            // ALT+RETURN: toggle fullscreen
			case VK_RETURN :
			{
				if(bAltDown) DXUTToggleFullScreen();
				break;
			}
            // F8: Take screenshot
			case VK_F8:
			{
                // Save current render target as png
                static int nr = 0;
				std::wstringstream ss;
				ss << L"Screenshot" << std::setfill(L'0') << std::setw(4) << nr++ << L".png";

                ID3D11Resource* pTex2D = nullptr;
                DXUTGetD3D11RenderTargetView()->GetResource(&pTex2D);
                SaveWICTextureToFile(DXUTGetD3D11DeviceContext(), pTex2D, GUID_ContainerFormatPng, ss.str().c_str());
                SAFE_RELEASE(pTex2D);

                std::wcout << L"Screenshot written to " << ss.str() << std::endl;
				break;
			}
			default : return;
		}
	}
}


//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
                       bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta,
                       int xPos, int yPos, void* pUserContext )
{
    // Track mouse movement if left mouse key is pressed
    {
        static int xPosSave = 0, yPosSave = 0;

        if (bLeftButtonDown)
        {
            // Accumulate deltas in g_viMouseDelta
            g_viMouseDelta.x += xPos - xPosSave;
            g_viMouseDelta.y += yPos - yPosSave;
        }    

        xPosSave = xPos;
        yPosSave = yPos;
    }
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
    // Send message to AntTweakbar first
    if (TwEventWin(hWnd, uMsg, wParam, lParam))
    {
        *pbNoFurtherProcessing = true;
        return 0;
    }

    // If message not processed yet, send to camera
	if(g_camera.HandleMessages(hWnd,uMsg,wParam,lParam))
    {
        *pbNoFurtherProcessing = true;
		return 0;
    }

	return 0;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double dTime, float fElapsedTime, void* pUserContext )
{
	UpdateWindowTitle(L"PhysicsDemo");

    // Move camera
    g_camera.FrameMove(fElapsedTime);

    // Update effects with new view + proj transformations
    g_pEffectPositionColor->SetView       (g_camera.GetViewMatrix());
    g_pEffectPositionColor->SetProjection (g_camera.GetProjMatrix());

    g_pEffectPositionNormal->SetView      (g_camera.GetViewMatrix());
    g_pEffectPositionNormal->SetProjection(g_camera.GetProjMatrix());

    // Apply accumulated mouse deltas to g_vfMovableObjectPos (move along cameras view plane)
    if (g_viMouseDelta.x != 0 || g_viMouseDelta.y != 0)
    {
        // Calcuate camera directions in world space
        XMMATRIX viewInv = XMMatrixInverse(nullptr, g_camera.GetViewMatrix());
        XMVECTOR camRightWorld = XMVector3TransformNormal(g_XMIdentityR0, viewInv);
        XMVECTOR camUpWorld    = XMVector3TransformNormal(g_XMIdentityR1, viewInv);

        // Add accumulated mouse deltas to movable object pos
        XMVECTOR vMovableObjectPos = XMLoadFloat3(&g_vfMovableObjectPos);

        float speedScale = 0.001f;
        vMovableObjectPos = XMVectorAdd(vMovableObjectPos,  speedScale * (float)g_viMouseDelta.x * camRightWorld);
        vMovableObjectPos = XMVectorAdd(vMovableObjectPos, -speedScale * (float)g_viMouseDelta.y * camUpWorld);

        XMStoreFloat3(&g_vfMovableObjectPos, vMovableObjectPos);
        
        // Reset accumulated mouse deltas
        g_viMouseDelta = XMINT2(0,0);
    }
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
	static float timeAcc = 0;

			timeAcc += fElapsedTime;
		while (timeAcc >= timeStep)
		{
			for (int i = 0; i < hashTableSize; i++)
			{
				hashTable[i].clear();
			}
			for (unsigned int i = 0; i < spheres.size(); i++)
			{
				Vec3 position = spheres[i].getPosition();
				int x, y, z;
				x = position.x / (2 * sphereSize);
				y = position.y / (2 * sphereSize);
				z = position.z / (2 * sphereSize);

				int hashIndex = (x * p1 ^ y * p2 ^ z * p3) % hashTableSize;
				hashTable[hashIndex].push_back(i);
			}

			for (unsigned int i = 0; i < spheres.size(); i++)
				spheres[i].step1();

			for (unsigned int i = 0; i < spheres.size(); i++)
			{
				spheres[i].clearForce();
				spheres[i].addGravityForce();
			}
			// naiver Ansatz
			/*for (unsigned int i = 0; i < spheres.size(); i++)
			{
				for (unsigned int j = i + 1; j < spheres.size(); j++)
				{
					Vec3 pos1 = spheres[i].getPosition();
					Vec3 pos2 = spheres[j].getPosition();
					Vec3 dir = pos2 - pos1;
					float d = sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
					if (d < 2 * sphereSize)
					{
						Vec3 dir = pos2 - pos1;
						dir.normalize();
						spheres[i].addForce(-dir * (1.0f - d / (2 * sphereSize)) * lambda);
						spheres[j].addForce(dir * (1.0f - d / (2 * sphereSize)) * lambda);
					}
				}
			}*/
			// mit Grid
			for (int hashIndex = 0; hashIndex < hashTableSize; hashIndex++)
			{
				for (unsigned int i = 0; i < hashTable[hashIndex].size(); i++)
				{
					int index1 = hashTable[hashIndex][i];

					Vec3 pos1 = spheres[index1].getPosition();

					int surroundingCells[27];
					int cIndex = 0;
					for (int s = 0; s < 3; s++)
					{
						for (int t = 0; t < 3; t++)
						{
							for (int u = 0; u < 3; u++)
							{
								int x = (pos1.x + (s - 1) * 2 * sphereSize) / (2 * sphereSize);
								int y = (pos1.y + (t - 1) * 2 * sphereSize) / (2 * sphereSize);
								int z = (pos1.z + (u - 1) * 2 * sphereSize) / (2 * sphereSize);
								surroundingCells[cIndex] = (x * p1 ^ y * p2 ^ z * p3) % hashTableSize;
								cIndex++;
							}
						}
					}

					for (int j = 0; j < 27; j++)
					{
						for (unsigned int k = 0; k < hashTable[surroundingCells[j]].size(); k++)
						{
							int index2 = hashTable[surroundingCells[j]][k];

							if (index1 != index2)
							{
								Vec3 pos2 = spheres[index2].getPosition();
								Vec3 dir = pos2 - pos1;
								float d = sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
								if (d < 2 * sphereSize)
								{
									Vec3 dir = pos2 - pos1;
									dir.normalize();
									spheres[index1].addForceFrom(-dir * (1.0f - d / (2 * sphereSize)) * lambda, index2);
									spheres[index2].addForceFrom(dir * (1.0f - d / (2 * sphereSize)) * lambda, index1);
								}
							}
						}
					}
				}
			}

			for (unsigned int i = 0; i < spheres.size(); i++)
				spheres[i].step2();

			timeAcc -= timeStep;
		}

	// Clear render target and depth stencil
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
	pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
	pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0 );

    // Draw floor
    DrawFloor(pd3dImmediateContext);

    // Draw axis box
    DrawBoundingBox(pd3dImmediateContext);

    // Draw speheres
    if (g_bDrawSpheres) DrawSomeRandomObjects(pd3dImmediateContext);

    // Draw movable teapot
    if (g_bDrawTeapot) DrawMovableTeapot(pd3dImmediateContext);

    // Draw simple triangle
    if (g_bDrawTriangle) DrawTriangle(pd3dImmediateContext);

	DrawSpheres(pd3dImmediateContext);

    // Draw GUI
    TwDraw();
}

//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	Sphere s1(0, 0, 0);
	Sphere s2(0.01f, 0.5f, 0);
	/*s1.addGravityForce();
	s2.addGravityForce();*/
	spheres.push_back(s1);
	spheres.push_back(s2);

#if defined(DEBUG) | defined(_DEBUG)
	// Enable run-time memory check for debug builds.
	// (on program exit, memory leaks are printed to Visual Studio's Output console)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

#ifdef _DEBUG
	std::wcout << L"---- DEBUG BUILD ----\n\n";
#endif

	// Set general DXUT callbacks
	DXUTSetCallbackMsgProc( MsgProc );
	DXUTSetCallbackMouse( OnMouse, true );
	DXUTSetCallbackKeyboard( OnKeyboard );

	DXUTSetCallbackFrameMove( OnFrameMove );
	DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

	// Set the D3D11 DXUT callbacks
	DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
	DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
	DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
	DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
	DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
	DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // Init camera
 	XMFLOAT3 eye(0.0f, 0.0f, -2.0f);
	XMFLOAT3 lookAt(0.0f, 0.0f, 0.0f);
	g_camera.SetViewParams(XMLoadFloat3(&eye), XMLoadFloat3(&lookAt));
    g_camera.SetButtonMasks(MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON);

    // Init DXUT and create device
	DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
	//DXUTSetIsInGammaCorrectMode( false ); // true by default (SRGB backbuffer), disable to force a RGB backbuffer
	DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
	DXUTCreateWindow( L"PhysicsDemo" );
	DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 1280, 960 );
    
	DXUTMainLoop(); // Enter into the DXUT render loop

	DXUTShutdown(); // Shuts down DXUT (includes calls to OnD3D11ReleasingSwapChain() and OnD3D11DestroyDevice())

	return DXUTGetExitCode();
}
