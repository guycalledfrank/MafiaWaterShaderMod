#include "H:/Soft/DXSDK8/include/d3d8.h"
#include "H:/Soft/DXSDK8/include/d3dx8.h"
//#include "windows.h"
#include <string>
using namespace std;
#include "console.h"
//#include "winmm.h"
#include <fstream>

//#define NO_NIGHT
#define LOAD_COMPILED

ofstream mylog;

IDirect3DDevice8* d3d8device;
ID3DXBuffer* shaderV;
ID3DXBuffer* shaderP;
ID3DXBuffer* shdBuff;
DWORD VS;
DWORD PS;
DWORD PSx8;
DWORD PSxS;
DWORD PSx2;
IDirect3DTexture8** refrTex;
//IDirect3DTexture8** normalTex;
IDirect3DTexture8* reflTex;
IDirect3DTexture8* fresnelTex;

bool lostDevice = false;
int sWidth,sHeight;

bool saveDecalMode = false;
bool afterWater = false;
int DIPs = 0;

IDirect3DSurface8* reflDSS;
IDirect3DSurface8* reflSurf;
IDirect3DSurface8* beforeReflSurf;
IDirect3DSurface8* beforeReflDSS;

IDirect3DTexture8* lastTex;

D3DXMATRIX worldMtx;
D3DXMATRIX viewMtx;
D3DXMATRIX projMtx;

DWORD wasFog = TRUE;
DWORD fogColor;

float tiling[4];
float zeroFive[4];
float fogColorS[4];
float posCam[4];

UINT actStream;
IDirect3DVertexBuffer8* actVB;
UINT actStride;
IDirect3DIndexBuffer8* actIB;
UINT actBaseVertIndex;

UINT mrStream;
IDirect3DVertexBuffer8* mrVB;
UINT mrStride;
IDirect3DIndexBuffer8* mrIB;
UINT mrBaseVertIndex;

D3DXVECTOR3 waterColor;

UINT mrMinIndex;
UINT mrNumVerts;
UINT mrStartIndex;
UINT mrPrimCount;

float lastDecalY = -1000;

//D3DMATERIAL8* lastMat;

// test
//D3DVIEWPORT8 rView; // refl
//D3DVIEWPORT8 sView; // scene

// Create the shader declaration.
DWORD dwDecl[] =
{
    D3DVSD_STREAM(0),
    D3DVSD_REG( D3DVSDE_POSITION,  D3DVSDT_FLOAT3),

	D3DVSD_REG( D3DVSDE_DIFFUSE,  D3DVSDT_D3DCOLOR),
    D3DVSD_END()
};

__forceinline fAssert_create(HRESULT h)
{
	if (h!=0) cout << "Error creating shader\n";
}

void LoadPS(const char* fname,DWORD* ptr)
{
	cout << "1\n";
	cout << "attempting to load "<<fname<<endl;
	ifstream if3(fname,ios::binary);
	if (!if3.is_open())
	{
		MessageBox(0,"Can't load shader","Can't load",0);
		exit(0);
	}

 // get length of file:
  if3.seekg (0, ios::end);
  int length = if3.tellg();
  if3.seekg (0, ios::beg);

  cout << "2\n";
	char* shaderData = new char[length];
	if3.read(shaderData,length);
	if3.close();
	cout << "3\n";
	cout << length<<endl;
	fAssert_create(d3d8device->CreatePixelShader( (DWORD*)shaderData, ptr ));
	cout << "4\n";
	delete[] shaderData;
	cout << "5\n";

}

void LoadVS(const char* fname,DWORD* ptr)
{
	ifstream if3(fname,ios::binary);
	cout << "attempting to load "<<fname<<endl;
	if (!if3.is_open())
	{
		MessageBox(0,"Can't load shader","Can't load",0);
		exit(0);
	}

 // get length of file:
  if3.seekg (0, ios::end);
  int length = if3.tellg();
  if3.seekg (0, ios::beg);

	char* shaderData = new char[length];
	if3.read(shaderData,length);
	if3.close();
	fAssert_create(d3d8device->CreateVertexShader(dwDecl, (DWORD*)shaderData, ptr,0));
	delete[] shaderData;

}
__forceinline fAssert(HRESULT h)
{
	if (h!=0) cout << "Error setting pixel shader\n";
}

DWORD lastFVF = 0;
DWORD zEnable;

int numWaterFrames = 29;
int waterFrame = 0;

bool shaderx2 = false;
bool shaderSunset = false;

bool shipMode = false;
bool noFogMode = false;



string* stateNames;
bool reflMode = false;
bool decalMode = false;
bool firstPass = false;

bool skyboxMode = false;
bool glowMode = false;
bool freshScene = false;

struct decalCall
{
	IDirect3DVertexBuffer8* VB;
	IDirect3DIndexBuffer8* IB;

	UINT stride;
	UINT baseVertIndex;

	UINT minIndex, NumVertices, startIndex, primCount;

	D3DXMATRIX world,view,proj;
};

decalCall calls[50];
int callID = 0;

void destroyData()
{
	beforeReflSurf->Release();
	beforeReflDSS->Release();
	reflSurf->Release();
	reflTex->Release();
	reflDSS->Release();

	callID = 0;

	d3d8device->DeletePixelShader(PS);
	d3d8device->DeletePixelShader(PSx8);
	d3d8device->DeletePixelShader(PSx2);
	d3d8device->DeletePixelShader(PSxS);
	d3d8device->DeleteVertexShader(VS);
}

void recreate()
{
			d3d8device->GetDepthStencilSurface(&beforeReflDSS);
			d3d8device->GetRenderTarget(&beforeReflSurf);
			cout << beforeReflDSS<<" "<<beforeReflSurf<<endl;

		HRESULT r2 = d3d8device->CreateTexture(sWidth,sHeight,
								  1,D3DUSAGE_RENDERTARGET,D3DFMT_X8R8G8B8,
								  D3DPOOL_DEFAULT,&reflTex);

		if (r2!=D3D_OK)
		{
			MessageBox(0,"Error creating texture","Can't create",0);
			exit(0);
		}

		reflTex->GetSurfaceLevel(0,&reflSurf);

		r2 = d3d8device->CreateDepthStencilSurface(sWidth,sHeight,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&reflDSS);
		if (r2!=D3D_OK)
		{
			MessageBox(0,"Error creating DSS","Can't create",0);
			exit(0);
		}

		LoadPS("testPS.pso",&PS);
		LoadPS("testPS_night.pso",&PSx8);
		LoadPS("testPS_x2.pso",&PSx2);
		LoadPS("testPS_sunset.pso",&PSxS);
		LoadVS("testVS.vso",&VS);
}


void enterReflMode()
{
	d3d8device->SetRenderTarget(reflSurf,reflDSS);
	if (firstPass)
	{
		//d3d8device->Clear(0,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFFFFFFFF,1,0);
		d3d8device->Clear(0,NULL,D3DCLEAR_ZBUFFER,0xFFFFFFFF,1,0);
		firstPass = false;
	}
	//reflMode = true;
}

void exitReflMode()
{
	//if (reflMode)
	{
		d3d8device->SetRenderTarget(beforeReflSurf,beforeReflDSS);
		//reflMode = false;
	}
}

string itoa( int n )
{
	char buff[32];
	sprintf(buff,"%d",n);
	return string( buff );
}

class IDirect3DDevice8proxy : public IDirect3DDevice8
{
	HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj)
	{ return
		d3d8device->QueryInterface(riid, ppvObj);
	}
	ULONG __stdcall AddRef()
	{ return
		d3d8device->AddRef();
	}
	ULONG __stdcall Release()
	{ return
		d3d8device->Release();
	}
	HRESULT __stdcall TestCooperativeLevel()
	{
		cout << "testc ";
		destroyData();

		HRESULT h = d3d8device->TestCooperativeLevel();
		if (h==D3DERR_DEVICELOST)
		{
			cout << "lost\n";
		}
		else if (h==D3DERR_DEVICENOTRESET)
		{
			cout << "not reset\n";
		}
		else
		{
			cout << "OK\n";
		}
		return h;
	}
	UINT __stdcall GetAvailableTextureMem()
	{ return
		d3d8device->GetAvailableTextureMem();
	}
	HRESULT __stdcall ResourceManagerDiscardBytes(DWORD Bytes)
	{ return
		d3d8device->ResourceManagerDiscardBytes( Bytes);
	}
	HRESULT __stdcall GetDirect3D(IDirect3D8** ppD3D8)
	{ return
		d3d8device->GetDirect3D(ppD3D8);
	}
	HRESULT __stdcall GetDeviceCaps(D3DCAPS8* pCaps)
	{ return
		d3d8device->GetDeviceCaps(pCaps);
	}
	HRESULT __stdcall GetDisplayMode(D3DDISPLAYMODE* pMode)
	{ return
		d3d8device->GetDisplayMode(pMode);
	}
	HRESULT __stdcall GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
	{ return
		d3d8device->GetCreationParameters(pParameters);
	}
	HRESULT __stdcall SetCursorProperties(UINT XHotSpot,UINT YHotSpot,IDirect3DSurface8* pCursorBitmap)
	{ return
		d3d8device->SetCursorProperties( XHotSpot, YHotSpot, pCursorBitmap);
	}
	void __stdcall SetCursorPosition(UINT XScreenSpace,UINT YScreenSpace,DWORD Flags)
	{ return
		d3d8device->SetCursorPosition(XScreenSpace,YScreenSpace,Flags);
	}
	BOOL __stdcall ShowCursor(BOOL bShow)
	{ return
		d3d8device->ShowCursor(bShow);
	}
	HRESULT __stdcall CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain8** pSwapChain)
	{ return
		d3d8device->CreateAdditionalSwapChain( pPresentationParameters,pSwapChain);
	}
	HRESULT __stdcall Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
	{
		//MessageBox(0,"888","888",0);
		cout << "reset ";
		HRESULT h = d3d8device->Reset( pPresentationParameters);
		if (h==D3DERR_INVALIDCALL )
		{
			cout << "invalid call\n";
		}
		else if (h==0)
		{
			cout << "OK\n";
			recreate();
		}
		else
		{
			cout << "nomem\n";
		}
		return h;

	}
	HRESULT __stdcall Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
	{
		return d3d8device->Present(  pSourceRect,  pDestRect,hDestWindowOverride, pDirtyRegion);
	}
	HRESULT __stdcall GetBackBuffer(UINT BackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface8** ppBackBuffer)
	{ return
		d3d8device->GetBackBuffer( BackBuffer, Type, ppBackBuffer);
	}
	HRESULT __stdcall GetRasterStatus(D3DRASTER_STATUS* pRasterStatus)
	{ return
		d3d8device->GetRasterStatus( pRasterStatus);
	}
	void __stdcall SetGammaRamp(DWORD Flags,CONST D3DGAMMARAMP* pRamp)
	{ return
		d3d8device->SetGammaRamp( Flags,  pRamp);
	}
	void __stdcall GetGammaRamp(D3DGAMMARAMP* pRamp)
	{ return
		d3d8device->GetGammaRamp( pRamp);
	}
	HRESULT __stdcall CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture8** ppTexture)
	{
		return d3d8device->CreateTexture( Width, Height, Levels, Usage, Format, Pool,ppTexture);
	}
	HRESULT __stdcall CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture8** ppVolumeTexture)
	{ return
		d3d8device->CreateVolumeTexture( Width, Height, Depth, Levels, Usage, Format, Pool,ppVolumeTexture);
	}
	HRESULT __stdcall CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture8** ppCubeTexture)
	{ return
		d3d8device->CreateCubeTexture( EdgeLength, Levels, Usage, Format, Pool,ppCubeTexture);
	}
	HRESULT __stdcall CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer)
	{ return
		d3d8device->CreateVertexBuffer( Length, Usage, FVF, Pool, ppVertexBuffer);
	}
	HRESULT __stdcall CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer)
	{ return
		d3d8device->CreateIndexBuffer( Length, Usage, Format, Pool, ppIndexBuffer);
	}
	HRESULT __stdcall CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,BOOL Lockable,IDirect3DSurface8** ppSurface)
	{
		return d3d8device->CreateRenderTarget( Width, Height, Format,MultiSample,Lockable, ppSurface);
	}
	HRESULT __stdcall CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,IDirect3DSurface8** ppSurface)
	{
		return d3d8device->CreateDepthStencilSurface( Width, Height, Format,MultiSample, ppSurface);
	}
	HRESULT __stdcall CreateImageSurface(UINT Width,UINT Height,D3DFORMAT Format, IDirect3DSurface8** ppSurface)
	{
		return d3d8device->CreateImageSurface( Width, Height, Format, ppSurface);
	}
	HRESULT __stdcall CopyRects(IDirect3DSurface8* pSourceSurface,CONST RECT* pSourceRectsArray,UINT cRects,IDirect3DSurface8* pDestinationSurface,CONST POINT* pDestPointsArray)
	{ return
		d3d8device->CopyRects( pSourceSurface,  pSourceRectsArray, cRects, pDestinationSurface, pDestPointsArray);
	}
	HRESULT __stdcall UpdateTexture(IDirect3DBaseTexture8* pSourceTexture,IDirect3DBaseTexture8* pDestinationTexture)
	{ return
		d3d8device->UpdateTexture( pSourceTexture, pDestinationTexture);
	}
	HRESULT __stdcall GetFrontBuffer(IDirect3DSurface8* pDestSurface)
	{ return
		d3d8device->GetFrontBuffer( pDestSurface);
	}
	HRESULT __stdcall SetRenderTarget(IDirect3DSurface8* pRenderTarget,IDirect3DSurface8* pNewZStencil)
	{
		return d3d8device->SetRenderTarget( pRenderTarget, pNewZStencil);
	}
	HRESULT __stdcall GetRenderTarget(IDirect3DSurface8** ppRenderTarget)
	{
		return	d3d8device->GetRenderTarget( ppRenderTarget);
	}
	HRESULT __stdcall GetDepthStencilSurface(IDirect3DSurface8** ppZStencilSurface)
	{ return
		d3d8device->GetDepthStencilSurface( ppZStencilSurface);
	}
	HRESULT __stdcall BeginScene()
	{
		//MessageBox(0,"OLOLO","OLOLO",0);
		//cout << endl;
		afterWater = false;
		freshScene = true;
		firstPass = true;

		int FPS = 30;
		int delay = 1000/FPS;
		waterFrame = (timeGetTime()/delay) % numWaterFrames;
		return d3d8device->BeginScene();
	}
	HRESULT __stdcall EndScene()
	{ return
	d3d8device->EndScene();
	}
	HRESULT __stdcall Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil)
	{
		//cout << endl
		return d3d8device->Clear( Count,pRects, Flags, Color, Z, Stencil);
	}
	HRESULT __stdcall SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
	{
		//return d3d8device->SetTransform( State,  pMatrix);
		if (State==D3DTS_WORLD)
		{
			worldMtx = *pMatrix;

			if ((((float*)worldMtx)[0])<-1400)
			{
				glowMode = true;
				reflMode = true;
			}
			//if (abs(((float*)worldMtx)[0])>400)
			else if (abs(((float*)worldMtx)[0])>400)
			{
				//enterReflMode();
				reflMode=true;
			}
			else if (abs(((float*)worldMtx)[0])>100)//if (abs(((float*)worldMtx)[0])>100)
			{
				if (!skyboxMode) reflMode=false;

				/*decalMode = true;
				DWORD lastFVF2 = lastFVF;
				d3d8device->GetVertexShader(&lastFVF);
				if (lastFVF==VS) lastFVF = lastFVF2;*/
				saveDecalMode = true;
			}
			//else if (worldMtx._42<-500)
			//{
			//	cout << "detected marker: "<<worldMtx._11<<endl;
			//}
			else
			{
				//exitReflMode();
				if (!skyboxMode) reflMode=false;

				//decalMode = false;
				saveDecalMode = false;
			}
		}
		if (State==D3DTS_VIEW) viewMtx = *pMatrix;
		if (State==D3DTS_PROJECTION) projMtx = *pMatrix;

		return d3d8device->SetTransform( State,  pMatrix);
	}
	HRESULT __stdcall GetTransform(D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix)
	{ return
	d3d8device->GetTransform( State, pMatrix);
	}
	HRESULT __stdcall MultiplyTransform(D3DTRANSFORMSTATETYPE tp,CONST D3DMATRIX* m)
	{
		//MessageBox(0,"Sad but true","",0);
		return d3d8device->MultiplyTransform(tp,m);
	}
	HRESULT __stdcall SetViewport(CONST D3DVIEWPORT8* pViewport)
	{ return
	d3d8device->SetViewport(  pViewport);
	}
	HRESULT __stdcall GetViewport(D3DVIEWPORT8* pViewport)
	{ return
	d3d8device->GetViewport( pViewport);
	}
	HRESULT __stdcall SetMaterial(CONST D3DMATERIAL8* pMaterial)
	{
		//cout << "material set\n";
		//lastMat = (D3DMATERIAL8*)pMaterial;
		return d3d8device->SetMaterial( pMaterial);
	}
	HRESULT __stdcall GetMaterial(D3DMATERIAL8* pMaterial)
	{ return
	d3d8device->GetMaterial( pMaterial);
	}
	HRESULT __stdcall SetLight(DWORD Index,CONST D3DLIGHT8* lht)
	{ 
		return d3d8device->SetLight( Index, lht);
	}
	HRESULT __stdcall GetLight(DWORD Index,D3DLIGHT8* lht)
	{ return
	d3d8device->GetLight( Index, lht);
	}
	HRESULT __stdcall LightEnable(DWORD Index,BOOL Enable)
	{ return
	d3d8device->LightEnable( Index, Enable);
	}
	HRESULT __stdcall GetLightEnable(DWORD Index,BOOL* pEnable)
	{ return
	d3d8device->GetLightEnable( Index,pEnable);
	}
	HRESULT __stdcall SetClipPlane(DWORD Index,CONST float* pPlane)
	{ return
	d3d8device->SetClipPlane( Index,  pPlane);
	}
	HRESULT __stdcall GetClipPlane(DWORD Index,float* pPlane)
	{ return
	d3d8device->GetClipPlane( Index, pPlane);
	}
	HRESULT __stdcall SetRenderState(D3DRENDERSTATETYPE State,DWORD Value)
	{
			//return d3d8device->SetRenderState( State, Value);

		if (State==D3DRS_FOGENABLE)
		{
			wasFog = Value;
			//if (freshScene)
			if (Value==FALSE)
			{
				skyboxMode = true;
				reflMode = true;
			}
			else
			{
				reflMode = false;
				skyboxMode = false;
			}
		}

		if (State==D3DRS_FOGCOLOR)
		{
			fogColor = Value;
		}

		if (State==D3DRS_ZENABLE)
		{
			zEnable = Value;
		}

		if (State==D3DRS_ALPHABLENDENABLE)
		{
			if ((Value==TRUE) && (zEnable))
			{
				d3d8device->GetVertexShader(&lastFVF);
				/*d3d8device->SetRenderState(D3DRS_CLIPPING,FALSE);
				d3d8device->SetRenderState(D3DRS_ZFUNC,D3DCMP_ALWAYS);//LESSEQUAL);
				d3d8device->SetRenderState(D3DRS_ZBIAS,0);
				d3d8device->SetRenderState(D3DRS_ZENABLE,TRUE);
				d3d8device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
				d3d8device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
				exitReflMode();*/
				d3d8device->SetRenderState(D3DRS_FOGENABLE,FALSE);

				d3d8device->SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING ,FALSE);
				//cout << "drawing "<<callID<<" chunks of water\n";
				for(int i=0;i<callID;i++)
				{
					d3d8device->SetStreamSource(0,calls[i].VB,calls[i].stride);
					d3d8device->SetIndices(calls[i].IB,calls[i].baseVertIndex);

					//decalMode = true;
					SetTexture(0,reflTex); // refl
					SetTexture(1,refrTex[waterFrame]); // refr

					SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);
					SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);

					fAssert(SetVertexShader(VS));

					if (shaderSunset)
					{
						cout << "sunset\n";
						fAssert(SetPixelShader(PSxS));
						//shaderx2 = false;
					}
					else if (waterColor.x<0.001)
					{
						cout << "x8\n";
						fAssert(SetPixelShader(PSx8));
					}
					else if (shaderx2)
					{
						cout << "x2\n";
						fAssert(SetPixelShader(PSx2));
						//shaderx2 = false;
					}
					else
					{
						cout << "PS\n";
						fAssert(SetPixelShader(PS));
					}

					//shipMode = false;
					if (shipMode)//(abs(worldMtx._22) - abs(worldMtx._11) > 50)
					{
						//shipMode = true;
						float s = 0.05f;
						tiling[0] = s;
						tiling[1] = s;
						tiling[2] = s;
						tiling[3] = s;
						SetVertexShaderConstant(11,(void*)&tiling,1);//tiling for parnik mission
						cout << "tiling changed\n";
					}

					if (noFogMode)
					{
						cout << "noFog\n";
					}

					D3DXMATRIX matTrans = calls[i].world*(viewMtx*calls[i].proj);
					D3DXMatrixTranspose(&matTrans,&matTrans);
					SetVertexShaderConstant(0,(void*)&matTrans,4);
					D3DXMatrixTranspose(&matTrans,&calls[i].world);
					SetVertexShaderConstant(4,(void*)&matTrans,4);

					float s = 0.5f;
					zeroFive[0] = s;
					zeroFive[1] = s;
					zeroFive[2] = s;
					zeroFive[3] = s;
					SetVertexShaderConstant(10,(void*)&zeroFive,1);

					if (!shipMode)
					{
						s = 0.1f;
						tiling[0] = s;
						tiling[1] = s;
						tiling[2] = s;
						tiling[3] = s;
						SetVertexShaderConstant(11,(void*)&tiling,1);//tiling
					}

					D3DXMATRIX camWorld;
					D3DXMatrixInverse(&camWorld,NULL,&viewMtx);
					posCam[0] = camWorld._41;
					posCam[1] = camWorld._42;
					posCam[2] = camWorld._43;
					posCam[3] = 1.0f;
					SetVertexShaderConstant(12,(void*)&posCam,1);

				//	D3DXMatrixTranspose(&matTrans,&viewMtx);
				//	SetVertexShaderConstant(13,(void*)&matTrans,4);

					SetPixelShaderConstant(2,(void*)&waterColor,1);

					fogColorS[0] = ((char*)&fogColor)[2] / 255.0f;
					fogColorS[1] = ((char*)&fogColor)[1] / 255.0f;
					fogColorS[2] = ((char*)&fogColor)[0] / 255.0f;
					fogColorS[3] = 1;
					SetPixelShaderConstant(5,(void*)&fogColorS,1);

					/*scl[0] = 1;
					scl[1] = 0;
					scl[2] = 0;
					scl[3] = 1;
					SetPixelShaderConstant(6,(void*)&scl,1);*/

					//cout << "Y: "<<worldMtx._42<<endl;
					cout << "waterColor: "<<waterColor.x<<", "<<waterColor.y<<", "<<waterColor.z<<endl;
					HRESULT hh = d3d8device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
						calls[i].minIndex, calls[i].NumVertices, calls[i].startIndex, calls[i].primCount);

				}
				d3d8device->SetVertexShader(lastFVF);//d3d8device->SetVertexShader( lastFVF );
				d3d8device->SetPixelShader(0);
				SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);
				SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);
				d3d8device->SetRenderState(D3DRS_FOGENABLE,wasFog);
				lastDecalY = calls[i].world._42;//worldMtx._42;
				decalMode = false;
				callID = 0;
				afterWater = true;
				//d3d8device->SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING ,TRUE);
				//	return hh;

			}
			else
			{
				//decalMode = false;
			}
		}


		//if (DIPs>0) cout << "DIPs: "<<DIPs<<endl;
		//DIPs = 0;
		//cout << stateNames[State].c_str()<<" "<<Value<<endl;
		return d3d8device->SetRenderState( State, Value);
	}
	HRESULT __stdcall GetRenderState(D3DRENDERSTATETYPE State,DWORD* pValue)
	{ return
		d3d8device->GetRenderState( State, pValue);
	}
	HRESULT __stdcall BeginStateBlock()
	{ return
		d3d8device->BeginStateBlock();
	}
	HRESULT __stdcall EndStateBlock(DWORD* pToken)
	{ return
		d3d8device->EndStateBlock( pToken);
	}
	HRESULT __stdcall ApplyStateBlock(DWORD Token)
	{ return
		d3d8device->ApplyStateBlock( Token);
	}
	HRESULT __stdcall CaptureStateBlock(DWORD Token)
	{ return
		d3d8device->CaptureStateBlock( Token);
	}
	HRESULT __stdcall DeleteStateBlock(DWORD Token)
	{ return
		d3d8device->DeleteStateBlock( Token);
	}
	HRESULT __stdcall CreateStateBlock(D3DSTATEBLOCKTYPE Type,DWORD* pToken)
	{ return
		d3d8device->CreateStateBlock(Type, pToken);
	}
	HRESULT __stdcall SetClipStatus(CONST D3DCLIPSTATUS8* pClipStatus)
	{ return
		d3d8device->SetClipStatus(  pClipStatus);
	}
	HRESULT __stdcall GetClipStatus(D3DCLIPSTATUS8* pClipStatus)
	{ return
		d3d8device->GetClipStatus( pClipStatus);
	}
	HRESULT __stdcall GetTexture(DWORD Stage,IDirect3DBaseTexture8** ppTexture)
	{ return
		d3d8device->GetTexture( Stage, ppTexture);
	}
	HRESULT __stdcall SetTexture(DWORD Stage,IDirect3DBaseTexture8* pTexture)
	{
		// 0 = lmap
		// 1 = env
		//if (Stage==2) return d3d8device->SetTexture( 0, pTexture);
		lastTex = (IDirect3DTexture8*)pTexture;
		return d3d8device->SetTexture( Stage, pTexture);
	}
	HRESULT __stdcall GetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue)
	{ return
		d3d8device->GetTextureStageState( Stage, Type, pValue);
	}
	HRESULT __stdcall SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
	{ return
		d3d8device->SetTextureStageState( Stage,Type, Value);
	}
	HRESULT __stdcall ValidateDevice(DWORD* pNumPasses)
	{
		HRESULT h = d3d8device->ValidateDevice( pNumPasses);
		/*if (h==D3DERR_DEVICELOST)
		{
			MessageBox(0,"111","111",0);				
			if (reflSurf!=0)
			{
				reflSurf->Release();
				reflTex->Release();
				reflDSS->Release();
				reflSurf = 0;

				callID = 0;

				DeletePixelShader(PS);
				DeletePixelShader(PSx8);
				DeletePixelShader(PSx2);
				DeletePixelShader(PSxS);
				DeleteVertexShader(VS);

				lostDevice = true;
			}
		}*/
		return h;
	}
	HRESULT __stdcall GetInfo(DWORD DevInfoID,void* pDevInfoStruct,DWORD DevInfoStructSize)
	{ return
		d3d8device->GetInfo( DevInfoID, pDevInfoStruct, DevInfoStructSize);
	}
	HRESULT __stdcall SetPaletteEntries(UINT PaletteNumber,CONST PALETTEENTRY* pEntries)
	{ return
		d3d8device->SetPaletteEntries( PaletteNumber,  pEntries);
	}
	HRESULT __stdcall GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY* pEntries)
	{ return
		d3d8device->GetPaletteEntries( PaletteNumber, pEntries);
	}
	HRESULT __stdcall SetCurrentTexturePalette(UINT PaletteNumber)
	{ return
		d3d8device->SetCurrentTexturePalette( PaletteNumber);
	}
	HRESULT __stdcall GetCurrentTexturePalette(UINT *PaletteNumber)
	{ return
		d3d8device->GetCurrentTexturePalette( PaletteNumber);
	}
	HRESULT __stdcall DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
	{ return
		d3d8device->DrawPrimitive( PrimitiveType, StartVertex, PrimitiveCount);
	}
	HRESULT __stdcall DrawIndexedPrimitive(D3DPRIMITIVETYPE pt,UINT minIndex,UINT NumVertices,UINT startIndex,UINT primCount)
	{
		//return d3d8device->DrawIndexedPrimitive(pt, minIndex, NumVertices, startIndex, primCount);
		//reflMode = decalMode = saveDecalMode=glowMode=false;
		//cout << "dip\n";
		//if (afterWater) return 0;
		DIPs++;

		/*if ((zEnable==FALSE)||(wasFog==FALSE))
		{
			skyboxMode = true;
			reflMode = true;
		}*/

		if ((skyboxMode)&&(primCount>12))
		{
			if (!glowMode)
			{
				skyboxMode = false;
				reflMode = false;
			}
		}

		if (saveDecalMode)
		{
			calls[callID].VB = actVB;
			calls[callID].stride = actStride;
			calls[callID].IB = actIB;
			calls[callID].baseVertIndex = actBaseVertIndex;
			calls[callID].minIndex = minIndex;
			calls[callID].NumVertices = NumVertices;
			calls[callID].startIndex = startIndex;
			calls[callID].primCount = primCount;
			calls[callID].world = worldMtx;
			//calls[callID].world._42 = -5;
			//calls[callID].view = viewMtx;
			calls[callID].proj = projMtx;
			callID++;
			return 0;
		}

		if (decalMode)
		{
		}

		//shipMode = false;
		if (!reflMode)
		{
			if (decalMode)
			{

			}
			return d3d8device->DrawIndexedPrimitive(pt, minIndex, NumVertices, startIndex, primCount);
		}

	//	d3d8device->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE);
	//	d3d8device->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x00000001);
	//	d3d8device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL);

		// to RT
		//d3d8device->SetRenderState(D3DRS_ZENABLE,TRUE);
		enterReflMode();
		if (glowMode)
		{
			d3d8device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
			d3d8device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
			d3d8device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
		}

			float pln[4];
			pln[0] = 0;
			pln[1] = -1;
			pln[2] = 0;
			pln[3] = -9.5;
			if ((!skyboxMode) || (glowMode))
			{
				if (!shipMode)
				{
					d3d8device->SetClipPlane(0,pln);
					d3d8device->SetRenderState(D3DRS_CLIPPLANEENABLE,D3DCLIPPLANE0);
				}
			}
			if (waterColor.x<0.001) d3d8device->SetRenderState(D3DRS_FOGENABLE,FALSE);
			if (shaderSunset)
			{
				unsigned char fcol[4];
				/*fcol[0] = 35;//0.125*255;
				fcol[1] = 33;//0.118*255;
				fcol[2] = 27;//0.098*255;
				fcol[3] = 255;*/
				fcol[0] = 27;//0.125*255;
				fcol[1] = 33;//0.118*255;
				fcol[2] = 35;//0.098*255;
				fcol[3] = 255;
				d3d8device->SetRenderState(D3DRS_FOGCOLOR,*(DWORD*)&fcol);
			}
			if (noFogMode)
			{
				unsigned char fcol[4];
				fcol[0] = 72;//0.125*255;
				fcol[1] = 68;//0.118*255;
				fcol[2] = 52;//0.098*255;
				fcol[3] = 255;
				d3d8device->SetRenderState(D3DRS_FOGCOLOR,*(DWORD*)&fcol);
			}

		HRESULT hh = d3d8device->DrawIndexedPrimitive(pt, minIndex, NumVertices, startIndex, primCount);
		if (glowMode)
		{
			d3d8device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
			d3d8device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
			d3d8device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
		}
		if ((!skyboxMode) || (glowMode))
		{
			d3d8device->SetRenderState(D3DRS_CLIPPLANEENABLE,0);
		}
		if (waterColor.x<0.001) d3d8device->SetRenderState(D3DRS_FOGENABLE,wasFog);
		if (shaderSunset||noFogMode)
		{
			d3d8device->SetRenderState(D3DRS_FOGCOLOR,fogColor);
		}

		//d3d8device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);

		//d3d8device->SetRenderState(D3DRS_ZENABLE,zEnable);
		exitReflMode();
		// duplicate to backbuffer
		if (skyboxMode && (!glowMode))
		{
			//cout << "waterColor: "<<worldMtx._41<<", "<<worldMtx._42<<", "<<worldMtx._43<<endl;
			if (zEnable==FALSE)
			{
				IDirect3DSurface8* lsurf;
				lastTex->GetSurfaceLevel(0,&lsurf);
				D3DSURFACE_DESC desc;
				lsurf->GetDesc(&desc);
				//cout << desc.Width << endl;
				lsurf->Release();

				if (desc.Width>=256)
				{
					//cout << "probably found skybox: " << desc.Width << endl;
					waterColor = D3DXVECTOR3(
					D3DXVec3Length((D3DXVECTOR3*)&worldMtx._11)-1,
					D3DXVec3Length((D3DXVECTOR3*)&worldMtx._21)-1,
					D3DXVec3Length((D3DXVECTOR3*)&worldMtx._31)-1);
					shaderx2 = false;
					shaderSunset = false;
					shipMode = false;
					noFogMode = false;
					if (waterColor.x > 0.5)
					{
						shaderx2 = true;
						waterColor.x -= 0.5f;
					}
					//if (waterColor.z > 0.5)
					else if ((waterColor.x < 0.001)&&(waterColor.y < 0.001)&&(waterColor.z < 0.001))
					{
						shaderSunset = true;
						//waterColor.z -= 0.5f;
						waterColor = D3DXVECTOR3(0.125, 0.118, 0.098);
						/*D3DXVec3Normalize((D3DXVECTOR3*)&worldMtx._11,(D3DXVECTOR3*)&worldMtx._11);
						D3DXVec3Normalize((D3DXVECTOR3*)&worldMtx._21,(D3DXVECTOR3*)&worldMtx._21);
						D3DXVec3Normalize((D3DXVECTOR3*)&worldMtx._31,(D3DXVECTOR3*)&worldMtx._31);
						
						d3d8device->SetTransform(D3DTS_WORLD,&worldMtx);*/
					}
					else if (waterColor.z < 0.001)
					{
						shipMode = true;
					}
					else if (waterColor.y < 0.001)
					{
						noFogMode = true;
					}
					hh = d3d8device->DrawIndexedPrimitive(pt, minIndex, NumVertices, startIndex, primCount);
				}
				else
				{
					hh = d3d8device->DrawIndexedPrimitive(pt, minIndex, NumVertices, startIndex, primCount);
				}
			}
			else
			{
				hh = d3d8device->DrawIndexedPrimitive(pt, minIndex, NumVertices, startIndex, primCount);
			}
		}
		glowMode = false;

		return hh;
	}
	HRESULT __stdcall DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
	{ return
		d3d8device->DrawPrimitiveUP( PrimitiveType, PrimitiveCount,  pVertexStreamZeroData, VertexStreamZeroStride);
	}
	HRESULT __stdcall DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertexIndices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
	{ return
		d3d8device->DrawIndexedPrimitiveUP( PrimitiveType, MinVertexIndex, NumVertexIndices, PrimitiveCount,  pIndexData, IndexDataFormat,  pVertexStreamZeroData, VertexStreamZeroStride);
	}
	HRESULT __stdcall ProcessVertices(UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer8* pDestBuffer,DWORD Flags)
	{ return
		d3d8device->ProcessVertices( SrcStartIndex, DestIndex, VertexCount, pDestBuffer, Flags);
	}
	HRESULT __stdcall CreateVertexShader(CONST DWORD* pDeclaration,CONST DWORD* pFunction,DWORD* pHandle,DWORD Usage)
	{ return
		d3d8device->CreateVertexShader(  pDeclaration,  pFunction, pHandle, Usage);
	}
	HRESULT __stdcall SetVertexShader(DWORD Handle)
	{
		return d3d8device->SetVertexShader( Handle);
	}
	HRESULT __stdcall GetVertexShader(DWORD* pHandle)
	{ return
		d3d8device->GetVertexShader( pHandle);
	}
	HRESULT __stdcall DeleteVertexShader(DWORD Handle)
	{ return
		d3d8device->DeleteVertexShader( Handle);
	}
	HRESULT __stdcall SetVertexShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount)
	{ return
		d3d8device->SetVertexShaderConstant( Register,  pConstantData, ConstantCount);
	}
	HRESULT __stdcall GetVertexShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount)
	{ return
		d3d8device->GetVertexShaderConstant( Register, pConstantData, ConstantCount);
	}
	HRESULT __stdcall GetVertexShaderDeclaration(DWORD Handle,void* pData,DWORD* pSizeOfData)
	{ return
		d3d8device->GetVertexShaderDeclaration( Handle, pData, pSizeOfData);
	}
	HRESULT __stdcall GetVertexShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData)
	{ return
		d3d8device->GetVertexShaderFunction( Handle, pData, pSizeOfData);
	}
	HRESULT __stdcall SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8* pStreamData,UINT Stride)
	{
		actStream = StreamNumber;
		actVB = pStreamData;
		actStride = Stride;
		return d3d8device->SetStreamSource( StreamNumber, pStreamData, Stride);
	}
	HRESULT __stdcall GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8** ppStreamData,UINT* pStride)
	{ return
		d3d8device->GetStreamSource( StreamNumber, ppStreamData, pStride);
	}
	HRESULT __stdcall SetIndices(IDirect3DIndexBuffer8* pIndexData,UINT BaseVertexIndex)
	{
		actIB = pIndexData;
		actBaseVertIndex = BaseVertexIndex;
		return d3d8device->SetIndices( pIndexData, BaseVertexIndex);
	}
	HRESULT __stdcall GetIndices(IDirect3DIndexBuffer8** ppIndexData,UINT* pBaseVertexIndex)
	{ return
		d3d8device->GetIndices( ppIndexData, pBaseVertexIndex);
	}
	HRESULT __stdcall CreatePixelShader(CONST DWORD* pFunction,DWORD* pHandle)
	{ return
		d3d8device->CreatePixelShader(  pFunction, pHandle);
	}
	HRESULT __stdcall SetPixelShader(DWORD Handle)
	{ return
		d3d8device->SetPixelShader( Handle);
	}
	HRESULT __stdcall GetPixelShader(DWORD* pHandle)
	{ return
		d3d8device->GetPixelShader( pHandle);
	}
	HRESULT __stdcall DeletePixelShader(DWORD Handle)
	{ return
		d3d8device->DeletePixelShader( Handle);
	}
	HRESULT __stdcall SetPixelShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount)
	{ return
		d3d8device->SetPixelShaderConstant( Register,  pConstantData, ConstantCount);
	}
	HRESULT __stdcall GetPixelShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount)
	{ return
		d3d8device->GetPixelShaderConstant( Register, pConstantData, ConstantCount);
	}
	HRESULT __stdcall GetPixelShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData)
	{ return
		d3d8device->GetPixelShaderFunction( Handle, pData, pSizeOfData);
	}
	HRESULT __stdcall DrawRectPatch(UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo)
	{ return
		d3d8device->DrawRectPatch( Handle,  pNumSegs, pRectPatchInfo);
	}
	HRESULT __stdcall DrawTriPatch(UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo)
	{ return
		d3d8device->DrawTriPatch( Handle,  pNumSegs,  pTriPatchInfo);
	}
	HRESULT __stdcall DeletePatch(UINT Handle)
	{ return
		d3d8device->DeletePatch( Handle);
	}
};


IDirect3D8* d3d8;
IDirect3DDevice8proxy* d3d8deviceProxy;

class IDirect3D8proxy : public IDirect3D8
{ 
    /*** IUnknown methods ***/
	HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj)
	{ 
		return d3d8->QueryInterface(riid,ppvObj);
	}
    ULONG __stdcall AddRef()
	{ 
		return d3d8->AddRef();
	}
    ULONG __stdcall Release()
	{ 
		return d3d8->Release();
	}

    /*** IDirect3D8 methods ***/
    HRESULT __stdcall RegisterSoftwareDevice(void* pInitializeFunction)
	{ 
		return d3d8->RegisterSoftwareDevice(pInitializeFunction);
	}

	UINT __stdcall GetAdapterCount()
	{ 
		return d3d8->GetAdapterCount();
	}
	HRESULT __stdcall GetAdapterIdentifier(UINT Adapter,DWORD Flags,D3DADAPTER_IDENTIFIER8* pIdentifier)
	{
		return d3d8->GetAdapterIdentifier(Adapter,Flags,pIdentifier);
	}
	UINT __stdcall GetAdapterModeCount(UINT Adapter)
	{ 
		return d3d8->GetAdapterModeCount(Adapter);
	}
	HRESULT __stdcall EnumAdapterModes(UINT Adapter,UINT Mode,D3DDISPLAYMODE* pMode)
	{
		return d3d8->EnumAdapterModes(Adapter,Mode,pMode);
	}
	HRESULT __stdcall GetAdapterDisplayMode(UINT Adapter,D3DDISPLAYMODE* pMode)
	{
		return d3d8->GetAdapterDisplayMode(Adapter,pMode);
	}
	HRESULT __stdcall CheckDeviceType(UINT Adapter,D3DDEVTYPE CheckType,D3DFORMAT DisplayFormat,D3DFORMAT BackBufferFormat,BOOL Windowed)
	{ 
		return d3d8->CheckDeviceType(Adapter,CheckType,DisplayFormat,BackBufferFormat,Windowed);
	}
	HRESULT __stdcall CheckDeviceFormat(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat)
	{ 
		return d3d8->CheckDeviceFormat(Adapter,DeviceType,AdapterFormat,Usage,RType,CheckFormat);
	}
	HRESULT __stdcall CheckDeviceMultiSampleType(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType)
	{
		return d3d8->CheckDeviceMultiSampleType(Adapter,DeviceType,SurfaceFormat,Windowed,MultiSampleType);
	}
	HRESULT __stdcall CheckDepthStencilMatch(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat)
	{
		return d3d8->CheckDepthStencilMatch(Adapter,DeviceType,AdapterFormat,RenderTargetFormat,DepthStencilFormat);
	}
	HRESULT __stdcall GetDeviceCaps(UINT Adapter,D3DDEVTYPE DeviceType,D3DCAPS8* pCaps)
	{
		return d3d8->GetDeviceCaps(Adapter,DeviceType,pCaps);
	}
	HMONITOR __stdcall GetAdapterMonitor(UINT Adapter)
	{ 
		return d3d8->GetAdapterMonitor(Adapter);
	}
	HRESULT __stdcall CreateDevice(UINT Adapter,D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice8** ppReturnedDeviceInterface)
	{
		//int Width = 1280;//pPresentationParameters->BackBufferWidth;
		//int Height = 1024;//pPresentationParameters->BackBufferHeight;
		BehaviorFlags = D3DCREATE_MIXED_VERTEXPROCESSING;

		//mylog.open("mylog.txt");
		//cout.rdbuf(mylog.rdbuf());
//		RedirectIOToConsole();

		HRESULT r = d3d8->CreateDevice(Adapter,DeviceType,hFocusWindow,BehaviorFlags,pPresentationParameters,&d3d8device);
		d3d8deviceProxy = new IDirect3DDevice8proxy;
		*ppReturnedDeviceInterface = d3d8deviceProxy;

		d3d8device->GetDepthStencilSurface(&beforeReflDSS);
		d3d8device->GetRenderTarget(&beforeReflSurf);
		cout << beforeReflDSS<<" "<<beforeReflSurf<<endl;

		D3DSURFACE_DESC desc;
		beforeReflDSS->GetDesc(&desc);
		int Width = desc.Width;
		int Height = desc.Height;
		sWidth = Width;
		sHeight = Height;
		//cout << "!\n";
		cout << Width<<" "<<Height<<endl;


#ifdef LOAD_COMPILED
		LoadPS("testPS.pso",&PS);
#else
		HRESULT r2 = D3DXAssembleShaderFromFile( "testPS.fx", 0, NULL, &shaderP, &shdBuff );    // assemble shader code
		if (r2!=D3D_OK)
		{
			MessageBox(0,(char*)shdBuff->GetBufferPointer(),"Can't compile PS",0);
			exit(0);
		}
		d3d8device->CreatePixelShader( (DWORD*)shaderP->GetBufferPointer(), &PS );
		ofstream of3("testPS.pso",ofstream::binary);
		of3.write((const char*)shaderP->GetBufferPointer(),shaderP->GetBufferSize());
		of3.close();
#endif


#ifdef LOAD_COMPILED
		cout << "n\n";
		LoadPS("testPS_night.pso",&PSx8);
#else

		r2 = D3DXAssembleShaderFromFile( "testPS_night.fx", 0, NULL, &shaderP, &shdBuff );    // assemble shader code
		if (r2!=D3D_OK)
		{
			MessageBox(0,(char*)shdBuff->GetBufferPointer(),"Can't compile PS_night",0);
			exit(0);
		}
		d3d8device->CreatePixelShader( (DWORD*)shaderP->GetBufferPointer(), &PSx8 );
		ofstream of2("testPS_night.pso",ofstream::binary);
		of2.write((const char*)shaderP->GetBufferPointer(),shaderP->GetBufferSize());
		of2.close();
#endif


#ifdef LOAD_COMPILED
		LoadPS("testPS_x2.pso",&PSx2);
#else
		r2 = D3DXAssembleShaderFromFile( "testPS_x2.fx", 0, NULL, &shaderP, &shdBuff );    // assemble shader code
		if (r2!=D3D_OK)
		{
			MessageBox(0,(char*)shdBuff->GetBufferPointer(),"Can't compile PS_x2",0);
			exit(0);
		}
		d3d8device->CreatePixelShader( (DWORD*)shaderP->GetBufferPointer(), &PSx2 );
		ofstream of1("testPS_x2.pso",ofstream::binary);
		of1.write((const char*)shaderP->GetBufferPointer(),shaderP->GetBufferSize());
		of1.close();
#endif


#ifdef LOAD_COMPILED
		LoadPS("testPS_sunset.pso",&PSxS);
#else
		r2 = D3DXAssembleShaderFromFile( "testPS_sunset.fx", 0, NULL, &shaderP, &shdBuff );    // assemble shader code
		if (r2!=D3D_OK)
		{
			MessageBox(0,(char*)shdBuff->GetBufferPointer(),"Can't compile PS_sunset",0);
			exit(0);
		}
		d3d8device->CreatePixelShader( (DWORD*)shaderP->GetBufferPointer(), &PSxS );
		ofstream of0("testPS_sunset.pso",ofstream::binary);
		of0.write((const char*)shaderP->GetBufferPointer(),shaderP->GetBufferSize());
		of0.close();
#endif


#ifdef LOAD_COMPILED
		LoadVS("testVS.vso",&VS);
		HRESULT r2;
#else
		r2 = D3DXAssembleShaderFromFile( "testVS.fx", 0, NULL, &shaderV, &shdBuff );    // assemble shader code
		if (r2!=D3D_OK)
		{
			MessageBox(0,(char*)shdBuff->GetBufferPointer(),"Can't compile VS",0);
			exit(0);
		}
		r2 = d3d8device->CreateVertexShader(dwDecl, (DWORD*)shaderV->GetBufferPointer(), &VS,0);//D3DUSAGE_SOFTWAREPROCESSING );
		if (r2!=D3D_OK)
		{
			if (r2==D3DERR_INVALIDCALL) cout << "invalid call\n";
			MessageBox(0,"!","Can't create VS",0);
			exit(0);
		}
		ofstream of("testVS.vso",ofstream::binary);
		of.write((const char*)shaderV->GetBufferPointer(),shaderV->GetBufferSize());
		of.close();
#endif



		refrTex = new IDirect3DTexture8*[numWaterFrames];
		for(int i=0;i<numWaterFrames;i++)
		{
			r2 = D3DXCreateTextureFromFile(d3d8device,("water_normal_"+itoa(i)+".dds").c_str(),&refrTex[i]);
			if (r2!=D3D_OK)
			{
				MessageBox(0,"Can't load texture","Can't load",0);
				exit(0);
			}
		}


		r2 = d3d8device->CreateTexture(Width,Height,
								  1,D3DUSAGE_RENDERTARGET,D3DFMT_X8R8G8B8,//pPresentationParameters->AutoDepthStencilFormat,
								  D3DPOOL_DEFAULT,&reflTex);

		if (r2!=D3D_OK)
		{
			MessageBox(0,"Error creating texture","Can't create",0);
			exit(0);
		}

		reflTex->GetSurfaceLevel(0,&reflSurf);

		r2 = d3d8device->CreateDepthStencilSurface(Width,Height,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&reflDSS);
		if (r2!=D3D_OK)
		{
			MessageBox(0,"Error creating DSS","Can't create",0);
			exit(0);
		}

		d3d8device->SetRenderTarget(reflSurf,reflDSS);
		d3d8device->Clear(0,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFFFFFFFF,1,0);
		d3d8device->SetRenderTarget(beforeReflSurf,beforeReflDSS);

		stateNames = new string[174];
		stateNames[7] = "D3DRS_ZENABLE";
		stateNames[8] = "D3DRS_FILLMODE";
		stateNames[9] = "D3DRS_SHADEMODE";
		stateNames[10] = "D3DRS_LINEPATTERN";
		stateNames[14] = "D3DRS_ZWRITEENABLE";
		stateNames[15] = "D3DRS_ALPHATESTENABLE";
		stateNames[16] = "D3DRS_LASTPIXEL";
		stateNames[19] = "D3DRS_SRCBLEND";
		stateNames[20] = "D3DRS_DESTBLEND";
		stateNames[22] = "D3DRS_CULLMODE";
		stateNames[23] = "D3DRS_ZFUNC";
		stateNames[24] = "D3DRS_ALPHAREF";
		stateNames[25] = "D3DRS_ALPHAFUNC";
		stateNames[26] = "D3DRS_DITHERENABLE";
		stateNames[27] = "D3DRS_ALPHABLENDENABLE";
		stateNames[28] = "D3DRS_FOGENABLE";
		stateNames[29] = "D3DRS_SPECULARENABLE";
		stateNames[30] = "D3DRS_ZVISIBLE";
		stateNames[34] = "D3DRS_FOGCOLOR";
		stateNames[35] = "D3DRS_FOGTABLEMODE";
		stateNames[36] = "D3DRS_FOGSTART";
		stateNames[37] = "D3DRS_FOGEND";
		stateNames[38] = "D3DRS_FOGDENSITY";
		stateNames[40] = "D3DRS_EDGEANTIALIAS";
		stateNames[47] = "D3DRS_ZBIAS";
		stateNames[48] = "D3DRS_RANGEFOGENABLE";
		stateNames[52] = "D3DRS_STENCILENABLE";
		stateNames[53] = "D3DRS_STENCILFAIL";
		stateNames[54] = "D3DRS_STENCILZFAIL";
		stateNames[55] = "D3DRS_STENCILPASS";
		stateNames[56] = "D3DRS_STENCILFUNC";
		stateNames[57] = "D3DRS_STENCILREF";
		stateNames[58] = "D3DRS_STENCILMASK";
		stateNames[59] = "D3DRS_STENCILWRITEMASK";
		stateNames[60] = "D3DRS_TEXTUREFACTOR";
		stateNames[128] = "D3DRS_WRAP0";
		stateNames[129] = "D3DRS_WRAP1";
		stateNames[130] = "D3DRS_WRAP2";
		stateNames[131] = "D3DRS_WRAP3";
		stateNames[132] = "D3DRS_WRAP4";
		stateNames[133] = "D3DRS_WRAP5";
		stateNames[134] = "D3DRS_WRAP6";
		stateNames[135] = "D3DRS_WRAP7";
		stateNames[136] = "D3DRS_CLIPPING";
		stateNames[137] = "D3DRS_LIGHTING";
		stateNames[139] = "D3DRS_AMBIENT";
		stateNames[140] = "D3DRS_FOGVERTEXMODE";
		stateNames[141] = "D3DRS_COLORVERTEX";
		stateNames[142] = "D3DRS_LOCALVIEWER";
		stateNames[143] = "D3DRS_NORMALIZENORMALS";
		stateNames[145] = "D3DRS_DIFFUSEMATERIALSOURCE";
		stateNames[146] = "D3DRS_SPECULARMATERIALSOURCE";
		stateNames[147] = "D3DRS_AMBIENTMATERIALSOURCE";
		stateNames[148] = "D3DRS_EMISSIVEMATERIALSOURCE";
		stateNames[151] = "D3DRS_VERTEXBLEND";
		stateNames[152] = "D3DRS_CLIPPLANEENABLE";
		stateNames[153] = "D3DRS_SOFTWAREVERTEXPROCESSING";
		stateNames[154] = "D3DRS_POINTSIZE";
		stateNames[155] = "D3DRS_POINTSIZE_MIN";
		stateNames[156] = "D3DRS_POINTSPRITEENABLE";
		stateNames[157] = "D3DRS_POINTSCALEENABLE";
		stateNames[158] = "D3DRS_POINTSCALE_A";
		stateNames[159] = "D3DRS_POINTSCALE_B";
		stateNames[160] = "D3DRS_POINTSCALE_C";
		stateNames[161] = "D3DRS_MULTISAMPLEANTIALIAS";
		stateNames[162] = "D3DRS_MULTISAMPLEMASK";
		stateNames[163] = "D3DRS_PATCHEDGESTYLE";
		stateNames[164] = "D3DRS_PATCHSEGMENTS";
		stateNames[165] = "D3DRS_DEBUGMONITORTOKEN";
		stateNames[166] = "D3DRS_POINTSIZE_MAX";
		stateNames[167] = "D3DRS_INDEXEDVERTEXBLENDENABLE";
		stateNames[168] = "D3DRS_COLORWRITEENABLE";
		stateNames[170] = "D3DRS_TWEENFACTOR";
		stateNames[171] = "D3DRS_BLENDOP";
		stateNames[172] = "D3DRS_POSITIONORDER";
		stateNames[173] = "D3DRS_NORMALORDER";

	/*	sView.Width = 1280;
		sView.Height = 1024;
		sView.X = 0;
		sView.Y = 0;
		sView.MinZ = 0;
		sView.MaxZ = 1;

		rView.Width = 640;
		rView.Height = 480;
		rView.X = 0;
		rView.Y = 0;
		rView.MinZ = 0;
		rView.MaxZ = 1;
*/
		return r;
	}
};

IDirect3D8proxy* d3d8proxy;

void * __stdcall Direct3DCreateF(float SDKVersion)
{
	//MessageBox(0,"Good","Good",0);
	d3d8 = ::Direct3DCreate8(*(UINT*)&SDKVersion);
	d3d8proxy = new IDirect3D8proxy;
	return d3d8proxy;
}