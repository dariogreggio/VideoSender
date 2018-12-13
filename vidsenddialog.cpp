// vidsnedDialog.cpp : implementation file
//

#include "stdafx.h"
#include "vidsend.h"
//#include "vidsendView.h"
#include "vidsendDoc.h"
#include "vidsendDialog.h"

#include "ras.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD) {
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
	g_pD3D       = NULL; // Used to create the D3DDevice
	g_pd3dDevice = NULL; // Our rendering device
	g_pVB        = NULL; // Buffer to hold vertices
	g_pTexture   = NULL; // Our texture
	m_blnGeom	 = FALSE;
	numofVertices = 0;
	}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	DDX_Control(pDX, IDC_PICTURE, m_picAbout);
	DDX_Text(pDX, IDC_TEXT2, m_Text);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
	ON_BN_CLICKED(IDC_TEXT1, OnText1)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_CHAR()
	ON_BN_CLICKED(IDC_PICTURE, OnPicture)
	ON_WM_RBUTTONDOWN()
	ON_WM_LBUTTONDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CAboutDlg::OnInitDialog() {
	CDialog::OnInitDialog();
	
	DWORD n;
	
#ifdef _CAMPARTY_MODE
	CString S;
	GetWindowText(S);
	SetWindowText(S+" (Camparty)");
#endif

	DShowVideoCapture::GetDXVersion(&n,NULL);
	m_Text.Format("Versione di DirectX: %u.%02u (o superiore)",HIWORD(n),LOWORD(n));
	
	try	{
		 
		m_nTimer			= 0;
		m_nTimer2			= 0;
		m_nTimer3			= 0;
		m_fProj				= 2.0f;
		m_nSpinType			= 1;
		HRESULT			hr	= InitD3D(m_picAbout.m_hWnd);

		if(!SUCCEEDED(hr))
		{
			AfxMessageBox("Failed To Initialize Direct 3D - (Color Mode Must Be In Either 16-bit or 32-bit)\nResolution Must be 1024 x 768");
			return FALSE; //return here so it won't try any direct3d functions
		}

		hr = InitGeometry();
		if(!SUCCEEDED(hr)) {
			m_blnGeom = FALSE;
			}
		else
			m_blnGeom = TRUE;

		Render();
		}
	catch(CException* err)
	{
		pErrObject->HandleError(err," "," ");
	}
	catch(...)
	{
		AfxMessageBox("Unhandled Error ");
	}

	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CAboutDlg::OnText1() {
	CString S;
	char p;

#ifdef _NEWMEET_MODE
	if((GetKeyState(VK_SHIFT) & 0x8000) && (GetKeyState(VK_CONTROL) & 0x8000)) {
		p='A';
		S+=p;
		p='D';
		S+=p;
		p='P';
		S+=p;
		p='M';
		S+=p;
		p=' ';
		S+=p;
		p='S';
		S+=p;
		p='y';
		S+=p;
		p='n';
		S+=p;
		p='t';
		S+=p;
		p='h';
		S+=p;
		p='e';
		S+=p;
		p='s';
		S+=p;
		p='i';
		S+=p;
		p='s';
		S+=p;
		AfxMessageBox(S);	
		}

#endif
	}


//*******************************************************************
HRESULT CAboutDlg::InitD3D(HWND hWnd) {

	HRESULT hr;
	try
	{
		// Create the D3D object.
		if( NULL == ( g_pD3D = Direct3DCreate8( D3D_SDK_VERSION ) ) )
			return E_FAIL;
		if(NULL == g_pD3D) {
			throw "Direct3DCreate8 Failed - Must Have wrong version";
		}
		// Get the current desktop display mode, so we can set up a back
		// buffer of the same format
		D3DDISPLAYMODE d3ddm;
		hr=g_pD3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &d3ddm );
		if( FAILED( hr ) ) {
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &d3ddm )",DXGetErrorDescription8(  hr));
			throw emsg;
			
		}
			

		// Set up the structure used to create the D3DDevice. Since we are now
		// using more complex geometry, we will create a device with a zbuffer.
		
		ZeroMemory( &d3dpp, sizeof(D3DPRESENT_PARAMETERS) );
		d3dpp.Windowed = TRUE;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.BackBufferFormat = d3ddm.Format;
		d3dpp.EnableAutoDepthStencil = TRUE;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
		
		// Create the D3DDevice
		hr = ( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
										  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
										  &d3dpp, &g_pd3dDevice ) ) ;
		if(FAILED(hr))	{
			hr = ( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd,
										  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
										  &d3dpp, &g_pd3dDevice ) ) ;
			if(FAILED(hr))
			{
				char emsg[512];
				sprintf(emsg,"Error Description: %s \nFunction: -  CreateDevice",DXGetErrorDescription8(  hr));
				throw emsg;
			}
		}

		// Turn off culling
		hr = g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
		if(FAILED(hr)) {
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE )",DXGetErrorDescription8(  hr));
			throw emsg;
		}
		// Turn off D3D lighting
		hr = g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
		if(FAILED(hr)) {
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetRenderState( D3DRS_LIGHTING, FALSE )",DXGetErrorDescription8(  hr));
			throw emsg;
		}
		// Turn on the zbuffer
		hr = g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
		if(FAILED(hr)) {
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetRenderState( D3DRS_ZENABLE, TRUE )",DXGetErrorDescription8(  hr));
			throw emsg;
		}
	}
	catch(CException* err)
	{
		pErrObject->HandleError(err," "," ");
		return E_FAIL;
	}
	catch(char msg[])
	{
		char emsg[1000];
		sprintf(emsg,"%s \nRoutine: InitD3D",msg);
		AfxMessageBox(emsg);
		return E_FAIL;
	}
	catch(...)
	{
		AfxMessageBox("Unhandled Error ");
		return E_FAIL;
	}

    return S_OK;
}
//*******************************************************************
HRESULT CAboutDlg::InitGeometry() {
	
  try	{
//		CString strPath;
//		strPath = AfxGetApp()->GetProfileString("Install","PathToExecutable");
		char szPath[256];
		sprintf(szPath,"about.bmp");
		HRESULT hr;
		// Use D3DX to create a texture from a file based image
		hr = ( D3DXCreateTextureFromFile( g_pd3dDevice, szPath,
											   &g_pTexture ) ) ;
		if(FAILED(hr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  D3DXCreateTextureFromFile",DXGetErrorDescription8(  hr));
			throw emsg;
			
		}

		// Create the vertex buffer.
		hr = ( g_pd3dDevice->CreateVertexBuffer( 50*2*sizeof(CUSTOMVERTEX),
													  0, D3DFVF_CUSTOMVERTEX,
													  D3DPOOL_DEFAULT, &g_pVB ) );
		if(FAILED(hr)) {
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  CreateVertexBuffer",DXGetErrorDescription8(  hr));
			throw emsg;
			
		}
		
		// Fill the vertex buffer. We are setting the tu and tv texture
		// coordinates, which range from 0.0 to 1.0
    
		//*****************************************************************
		static float zp = 0.0f;
		CUSTOMVERTEX* pVertices; 
		hr = ( g_pVB->Lock( 0, 0, (BYTE**)&pVertices, 0 ) ) ;
		if(FAILED(hr)) {
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction: g_pVB->Lock",DXGetErrorDescription8(  hr));
			throw emsg;
			
		}
		pVertices[0].position = D3DXVECTOR3(-1.0	,-1.0	,0.0);
		pVertices[0].color = 0xffeeeeee;
		pVertices[0].tu = 0.0;
		pVertices[0].tv = 1.0;

		pVertices[1].position = D3DXVECTOR3(-1.0	, 1.0	,0.0);
		pVertices[1].color = 0xffffff00;
		pVertices[1].tu = 0.0;
		pVertices[1].tv = 0.0;
		
		pVertices[2].position = D3DXVECTOR3(1.0	,-1.0	,0.0);
		pVertices[2].color = 0xffeeeeee;
		pVertices[2].tu = 1.0;
		pVertices[2].tv = 1.0;

		pVertices[3].position = D3DXVECTOR3( 1.0	,-1.0	,0.0);
		pVertices[3].color = 0xffeeeeee;
		pVertices[3].tu = 1.0;
		pVertices[3].tv = 1.0;
			
		pVertices[4].position = D3DXVECTOR3( -1.0	, 1.0	,0.0);
		pVertices[4].color = 0xffffff00;
		pVertices[4].tu = 0.0;
		pVertices[4].tv = 0.0;
		
		pVertices[5].position = D3DXVECTOR3( 1.0	, 1.0	,0.0);
		pVertices[5].color = 0xff0022ff;
		pVertices[5].tu = 1.0;
		pVertices[5].tv = 0.0;

		/*pVertices[6].position = D3DXVECTOR3(  1.0	,-1.0	,0.0);
		pVertices[6].color = 0xffffff00;
		pVertices[6].tu = 0.0;
		pVertices[6].tv = 0.0;

		pVertices[7].position = D3DXVECTOR3( 1.0	, 1.0	,0.0);
		pVertices[7].color = 0xff555500;
		pVertices[7].tu = 0.0;
		pVertices[7].tv = 0.0;

		pVertices[8].position = D3DXVECTOR3( 1.0	, 1.0	,1.0);
		pVertices[8].color = 0xff5555ff;
		pVertices[8].tu = 0.0;
		pVertices[8].tv = 0.0;

		pVertices[9].position = D3DXVECTOR3( 1.0	,-1.0	,0.0);
		pVertices[9].color = 0xff000055;
		pVertices[9].tu = 0.0;
		pVertices[9].tv = 0.0;

		pVertices[10].position = D3DXVECTOR3( 1.0	, 1.0	,1.0);
		pVertices[10].color = 0xfff00f00;
		pVertices[10].tu = 0.0;
		pVertices[10].tv = 0.0;

		pVertices[11].position = D3DXVECTOR3( 1.0	,-1.0	,1.0);
		pVertices[11].color = 0xff111f00;
		pVertices[11].tu = 0.0;
		pVertices[11].tv = 0.0;

		pVertices[12].position = D3DXVECTOR3( 1.0	, 1.0	,1.0);
		pVertices[12].color = 0xff111f00;
		pVertices[12].tu = 0.0;
		pVertices[12].tv = 0.0;

		pVertices[13].position = D3DXVECTOR3( -1.0	, 1.0	,1.0);
		pVertices[13].color = 0xff555555;
		pVertices[13].tu = 0.0;
		pVertices[13].tv = 0.0;

		pVertices[14].position = D3DXVECTOR3( 1.0	,-1.0	,1.0);
		pVertices[14].color = 0xff550000;
		pVertices[14].tu = 0.0;
		pVertices[14].tv = 0.0;


		pVertices[15].position = D3DXVECTOR3(  1.0	,-1.0	,1.0);
		pVertices[15].color = 0xff550055;
		pVertices[15].tu = 0.0;
		pVertices[15].tv = 0.0;

		pVertices[16].position = D3DXVECTOR3(-1.0	, 1.0	,1.0);
		pVertices[16].color = 0xf5566500;
		pVertices[16].tu = 0.0;
		pVertices[16].tv = 0.0;

		pVertices[17].position = D3DXVECTOR3(-1.0	,-1.0	,1.0);
		pVertices[17].color = 0xff448800;
		pVertices[17].tu = 0.0;
		pVertices[17].tv = 0.0;

		pVertices[18].position = D3DXVECTOR3( 1.0	,-1.0	,0.0);
		pVertices[18].color = 0xff448800;
		pVertices[18].tu = 0.0;
		pVertices[18].tv = 0.0;

		pVertices[19].position = D3DXVECTOR3(-1.0	,-1.0	,0.0);
		pVertices[19].color = 0xffffff00;
		pVertices[19].tu = 0.0;
		pVertices[19].tv = 0.0;

		pVertices[20].position = D3DXVECTOR3(-1.0	,-1.0	,1.0);
		pVertices[20].color = 0xffffff00;
		pVertices[20].tu = 0.0;
		pVertices[20].tv = 0.0;

		pVertices[21].position = D3DXVECTOR3( 1.0	,-1.0	,0.0);
		pVertices[21].color = 0xffff0000;
		pVertices[21].tu = 0.0;
		pVertices[21].tv = 0.0;

		pVertices[22].position = D3DXVECTOR3(-1.0	,-1.0	,1.0);
		pVertices[22].color = 0xffff0000;
		pVertices[22].tu = 0.0;
		pVertices[22].tv = 0.0;

		pVertices[23].position = D3DXVECTOR3( 1.0	,-1.0	,1.0);
		pVertices[23].color = 0xffff0000;
		pVertices[23].tu = 0.0;
		pVertices[23].tv = 0.0;

		pVertices[24].position = D3DXVECTOR3(-1.0	,-1.0	,0.0);
		pVertices[24].color = 0xffff0000;
		pVertices[24].tu = 0.0;
		pVertices[24].tv = 0.0;

		pVertices[25].position = D3DXVECTOR3(-1.0	, 1.0	,1.0);
		pVertices[25].color = 0xff448800;
		pVertices[25].tu = 0.0;
		pVertices[25].tv = 0.0;

		pVertices[26].position = D3DXVECTOR3(-1.0	, 1.0	,0.0);
		pVertices[26].color = 0xff448800;
		pVertices[26].tu = 0.0;
		pVertices[26].tv = 0.0;

		pVertices[27].position = D3DXVECTOR3(-1.0	,-1.0	,0.0);
		pVertices[27].color = 0xff0055dd;
		pVertices[27].tu = 0.0;
		pVertices[27].tv = 0.0;

		pVertices[28].position = D3DXVECTOR3(-1.0	,-1.0	,1.0);
		pVertices[28].color = 0xff0055dd;
		pVertices[28].tu = 0.0;
		pVertices[28].tv = 0.0;

		pVertices[29].position = D3DXVECTOR3(-1.0	, 1.0	,1.0);
		pVertices[29].color = 0xff0055dd;
		pVertices[29].tu = 0.0;
		pVertices[29].tv = 0.0;

		pVertices[30].position = D3DXVECTOR3(-0.5f	,zp + -0.5f	,0.5f -zp);
		pVertices[30].color = 0xff000ff0;
		pVertices[30].tu = 0.0;
		pVertices[30].tv = 1.0;

		pVertices[31].position = D3DXVECTOR3( 0.0	, zp + 0.5	,0.5-zp);
		pVertices[31].color = 0xff0055dd;
		pVertices[31].tu = 0.5;
		pVertices[31].tv = 0.0;

		pVertices[32].position = D3DXVECTOR3( 0.5f	,zp + -0.5f	,0.5f - zp);
		pVertices[32].color = 0xff0055dd;
		pVertices[32].tu = 1.0;
		pVertices[32].tv = 1.0;*/


		if(zp < 1.5)
			zp += 0.01f;
			
		g_pVB->Unlock();
	}
	catch(CException* err)
	{
		pErrObject->HandleError(err," "," ");
		m_blnGeom = FALSE;
		return E_FAIL;
	}
	catch(_com_error& e)
	{
		pErrObject->HandleError(e,"","");
		m_blnGeom = FALSE;
		return E_FAIL;
	}
	catch(char msg[])
	{
		char emsg[1000];
		sprintf(emsg,"%s \nRoutine: InitGeometry",msg);
		AfxMessageBox(emsg);
		return E_FAIL;
	}
	catch(...)
	{
		AfxMessageBox("Unhandled Error ");
		m_blnGeom = FALSE;
		return E_FAIL;
	}

	m_blnGeom = TRUE;
    return S_OK;
}
//*******************************************************************
void CAboutDlg::Cleanup()
{
	try
	{
		if( g_pTexture != NULL )
			g_pTexture->Release();

		if( g_pVB != NULL )
			g_pVB->Release();

		if( g_pd3dDevice != NULL )
			g_pd3dDevice->Release();

		if( g_pD3D != NULL )
			g_pD3D->Release();

		
		KillTimer(1);
		KillTimer(2);
		KillTimer(3);
	}
	catch(CException* err)
	{
		pErrObject->HandleError(err," "," ");
	}
	catch(...)
	{
		AfxMessageBox("Unhandled Error ");
	}
}
//*******************************************************************
void CAboutDlg::SetupMatrices() {

	try
	{
		static float zz = 0.0f;
		if(!m_blnGeom)
			if(FAILED(InitGeometry()))
				m_blnGeom = FALSE;
		// For our world matrix, we will just leave it as the identity
		D3DXMATRIX matWorld;
		D3DXMatrixIdentity( &matWorld );
		
		switch(m_nSpinType)	{
		case 1:
			D3DXMatrixRotationY( &matWorld, timeGetTime()/1000.0f );
			break;
		case 2:
			D3DXMatrixRotationX( &matWorld, timeGetTime()/1000.0f );
			break;
		default:
			D3DXMatrixRotationZ( &matWorld, timeGetTime()/1000.0f );
		}
		g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

		// Set up our view matrix. A view matrix can be defined given an eye point,
		// a point to lookat, and a direction for which way is up. Here, we set the
		// eye five units back along the z-axis and up three units, look at the
		// origin, and define "up" to be in the y-direction.
		D3DXMATRIX matView;
		D3DXMatrixLookAtLH( &matView, &D3DXVECTOR3( 0.0f, 3.0f,-5.0f ),
									  &D3DXVECTOR3( 0.0f, 0.0f, 0.0f ),
									  &D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) );
		g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

		// For the projection matrix, we set up a perspective transform (which
		// transforms geometry from 3D view space to 2D viewport space, with
		// a perspective divide making objects smaller in the distance). To build
		// a perpsective transform, we need the field of view (1/4 pi is common),
		// the aspect ratio, and the near and far clipping planes (which define at
		// what distances geometry should be no longer be rendered).
		D3DXMATRIX matProj;
		if(m_fProj < 5.0f)
			m_fProj += 0.01f;
		D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/m_fProj, 1.0f, 1.0f, 100.0f );
		g_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );
	}
	catch(CException* err)
	{
		pErrObject->HandleError(err," "," ");
	}
	catch(_com_error& e)
	{
		pErrObject->HandleError(e,"","");
	}
	catch(...)
	{
		AfxMessageBox("Unhandled Error ");
	}
}
//*******************************************************************
BOOL CAboutDlg::Render() {

    try
	{
		HRESULT dhr;
		// Clear the backbuffer and the zbuffer
		g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
							 D3DCOLOR_XRGB(10,10,10), 1.0f, 0 );

		// Begin the scene
		g_pd3dDevice->BeginScene();

		// Setup the world, view, and projection matrices
		SetupMatrices();

		// Setup our texture. Using textures introduces the texture stage states,
		// which govern how textures get blended together (in the case of multiple
		// textures) and lighting information. In this case, we are modulating
		// (blending) our texture with the diffuse color of the vertices.
		dhr = g_pd3dDevice->SetTexture( 0, g_pTexture );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetTexture( 0, g_pTexture )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
		dhr = g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
		dhr = g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
		dhr = g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
		dhr = g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}

	#ifdef SHOW_HOW_TO_USE_TCI
		// Note: to use D3D texture coordinate generation, use the stage state
		// D3DTSS_TEXCOORDINDEX, as shown below. In this example, we are using
		// the position of the vertex in camera space to generate texture
		// coordinates. The tex coord index (TCI) parameters are passed into a
		// texture transform, which is a 4x4 matrix which transforms the x,y,z
		// TCI coordinates into tu, tv texture coordinates.

		// In this example, the texture matrix is setup to 
		// transform the texture from (-1,+1) position coordinates to (0,1) 
		// texture coordinate space:
		//    tu =  0.5*x + 0.5
		//    tv = -0.5*y + 0.5
		D3DXMATRIX mat;
		mat._11 = 0.25f; mat._12 = 0.00f; mat._13 = 0.00f; mat._14 = 0.00f;
		mat._21 = 0.00f; mat._22 =-0.25f; mat._23 = 0.00f; mat._24 = 0.00f;
		mat._31 = 0.00f; mat._32 = 0.00f; mat._33 = 1.00f; mat._34 = 0.00f;
		mat._41 = 0.50f; mat._42 = 0.50f; mat._43 = 0.00f; mat._44 = 1.00f;

		dhr = g_pd3dDevice->SetTransform( D3DTS_TEXTURE0, &mat );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetTransform( D3DTS_TEXTURE0, &mat )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
		dhr = g_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
		dhr = g_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
	#endif

		// Render the vertex buffer contents
		dhr = g_pd3dDevice->SetStreamSource( 0, g_pVB, sizeof(CUSTOMVERTEX) );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction:  SetStreamSource( 0, g_pVB, sizeof(CUSTOMVERTEX) )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
		dhr = g_pd3dDevice->SetVertexShader( D3DFVF_CUSTOMVERTEX );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction: SetVertexShader( D3DFVF_CUSTOMVERTEX )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
		dhr = g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 2 );
		if(FAILED(dhr))
		{
			char emsg[512];
			if(dhr == D3DERR_DEVICELOST)
				AfxMessageBox("lost device");
			sprintf(emsg,"Error Description: %s \nFunction: -  DrawPrimitive device handle is: %d ",DXGetErrorDescription8(  dhr),g_pd3dDevice);
			throw emsg;
		}
		
		// End the scene
		g_pd3dDevice->EndScene();

		// Present the backbuffer contents to the display
		dhr = g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
		if(FAILED(dhr))
		{
			char emsg[512];
			sprintf(emsg,"Error Description: %s \nFunction: Present( NULL, NULL, NULL, NULL )",DXGetErrorDescription8(  dhr));
			throw emsg;
		}
		
		if(!m_nTimer)
		{
			m_nTimer = SetTimer(1,10,0);
			m_nTimer2 = SetTimer(2,15000,0);
			m_nTimer3 = SetTimer(3,5000,0);
		}
		return TRUE;
	}
	catch(CException* err)
	{
		pErrObject->HandleError(err," "," ");
		return FALSE;
	}
	catch(_com_error& e)
	{
		pErrObject->HandleError(e,"","");
		return FALSE;
	}
	catch(char msg[])
	{
		char emsg[1000];
		sprintf(emsg,"%s \nRoutine: Render",msg);
		AfxMessageBox(emsg);
		return FALSE;
	}
	catch(const char* msg)
	{
		char em[512];
		sprintf(em,"Error Description: %s \nFunction: in Render routine",msg);
		AfxMessageBox(em);
		return FALSE;
	}
	catch(...)
	{
		AfxMessageBox("Unhandled Error in Render routine ");
		return FALSE;
	}
	
}
//*******************************************************************
void CAboutDlg::OnTimer(UINT nIDEvent) {

	try	{
		switch(nIDEvent) {
		case 1:
			if(!Render())	{
				KillTimer(1);
				KillTimer(2);
				KillTimer(3);
				AfxAbort();
			}
			break;
		case 2:
			OnOK();
			MessageBox("Now You're Just Being Nosey!","Nosey Nabber",MB_OK | MB_ICONINFORMATION);
			break;
		default:
			m_nSpinType += 1;
		}
	}
	catch(CException* err)
	{
		pErrObject->HandleError(err," "," ");
	}
	catch(_com_error& e)
	{
		pErrObject->HandleError(e,"","");
	}
	catch(...)
	{
		AfxMessageBox("Unhandled Error ");
	}
	
	CDialog::OnTimer(nIDEvent);
}
//*******************************************************************
void CAboutDlg::OnDestroy() {

	Cleanup();
	CDialog::OnDestroy();
}
//*******************************************************************
void CAboutDlg::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) {	

	CDialog::OnChar(nChar, nRepCnt, nFlags);
}
//*******************************************************************
void CAboutDlg::OnPicture() {
	
}
//*******************************************************************
BOOL CAboutDlg::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::OnNotify(wParam, lParam, pResult);
}
//*******************************************************************
void CAboutDlg::OnRButtonDown(UINT nFlags, CPoint point) {

	// TODO: Add your message handler code here and/or call default
	if(m_fProj > 2.0f)
		m_fProj -= 0.1f;
	CDialog::OnRButtonDown(nFlags, point);
}
//*******************************************************************
void CAboutDlg::OnLButtonDown(UINT nFlags, CPoint point) {

	// TODO: Add your message handler code here and/or call default
	if(m_fProj < 5.0f)
		m_fProj += 0.1f;
	CDialog::OnLButtonDown(nFlags, point);
}
//*******************************************************************
void CAboutDlg::OnOK() {
	// TODO: Add extra validation here
	
	CDialog::OnOK();
}
//*******************************************************************


/////////////////////////////////////////////////////////////////////////////
// CSplashScreenEx (prec. CSplashDlg) dialog
// da John O'Byrne's - www.codeproject.com


#ifndef AW_HIDE
	#define AW_HIDE 0x00010000
	#define AW_BLEND 0x00080000
#endif

#ifndef CS_DROPSHADOW
	#define CS_DROPSHADOW   0x00020000
#endif

// CSplashScreenEx

IMPLEMENT_DYNAMIC(CSplashScreenEx, CWnd)
CSplashScreenEx::CSplashScreenEx() {

	m_pWndParent=NULL;
	m_strText="";
	m_hRegion=0;
	m_nBitmapWidth=0;
	m_nBitmapHeight=0;
	m_nxPos=0;
	m_nyPos=0;
	m_dwTimeout=2000;
	m_dwStyle=0;
	m_rcText.SetRect(0,0,0,0);
	m_crTextColor=RGB(0,0,0);
	m_uTextFormat=DT_CENTER | DT_VCENTER | DT_WORDBREAK;

	HMODULE hUser32 = GetModuleHandle(_T("USER32.DLL"));
	if(hUser32 != NULL)
		m_fnAnimateWindow = (FN_ANIMATE_WINDOW)GetProcAddress(hUser32, _T("AnimateWindow"));
	else
		m_fnAnimateWindow = NULL;

	SetTextDefaultFont();
	}

CSplashScreenEx::~CSplashScreenEx() {

	if(m_hRegion)
		DeleteObject(m_hRegion);
	}

BOOL CSplashScreenEx::Create(CWnd *pWndParent,LPCTSTR szText,DWORD dwTimeout,DWORD dwStyle) {

	ASSERT(pWndParent != NULL);
	m_pWndParent = pWndParent;
	
	if(szText != NULL)
		m_strText = szText;
	else
		m_strText="";

	m_dwTimeout = dwTimeout;
	m_dwStyle = dwStyle;
	
	WNDCLASSEX wcx; 

	wcx.cbSize = sizeof(wcx);
	wcx.lpfnWndProc = AfxWndProc;
	wcx.style = CS_DBLCLKS | CS_SAVEBITS;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = AfxGetInstanceHandle();
	wcx.hIcon = NULL;
	wcx.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcx.hbrBackground=::GetSysColorBrush(COLOR_WINDOW);
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = "SplashScreenExClass";
	wcx.hIconSm = NULL;

	if(m_dwStyle & CSS_SHADOW)
		wcx.style |= CS_DROPSHADOW;

	ATOM classAtom = RegisterClassEx(&wcx);
      
	// didn't work? try not using dropshadow (may not be supported)

	if(classAtom==NULL) {
		if(m_dwStyle & CSS_SHADOW)	{
			wcx.style &= ~CS_DROPSHADOW;
			classAtom = RegisterClassEx(&wcx);
			}
		else
			return FALSE;
		}

	if(!CreateEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,"SplashScreenExClass",NULL,WS_POPUP,0,0,0,0,
		pWndParent ? pWndParent->m_hWnd : NULL,NULL))
		return FALSE;

	return TRUE;
	}

BOOL CSplashScreenEx::SetBitmap(UINT nBitmapID,COLORREF transparency) {
	BITMAP bm;

	m_bitmap.DeleteObject();
	if(!m_bitmap.LoadBitmap(nBitmapID))
		return FALSE;
	
	GetObject(m_bitmap.GetSafeHandle(), sizeof(bm), &bm);
	m_nBitmapWidth=bm.bmWidth;
	m_nBitmapHeight=bm.bmHeight;
	m_rcText.SetRect(0,0,bm.bmWidth,bm.bmHeight);
	
	if(m_dwStyle & CSS_CENTERSCREEN)	{
		m_nxPos=(GetSystemMetrics(SM_CXFULLSCREEN)-bm.bmWidth)/2;
		m_nyPos=(GetSystemMetrics(SM_CYFULLSCREEN)-bm.bmHeight)/2;
		}
	else if(m_dwStyle & CSS_CENTERAPP)	{
		CRect rcParentWindow;
		ASSERT(m_pWndParent != NULL);
		m_pWndParent->GetWindowRect(&rcParentWindow);
		m_nxPos=rcParentWindow.left+(rcParentWindow.right-rcParentWindow.left-bm.bmWidth)/2;
		m_nyPos=rcParentWindow.top+(rcParentWindow.bottom-rcParentWindow.top-bm.bmHeight)/2;
		}

	if(transparency != -1)	{
		m_hRegion=CreateRgnFromBitmap((HBITMAP)m_bitmap.GetSafeHandle(),transparency);
		SetWindowRgn(m_hRegion, TRUE);
//		DeleteObject(m_hRegion);
		}

	return TRUE;
	}

BOOL CSplashScreenEx::SetBitmap(LPCTSTR szFileName,COLORREF transparency) {
	BITMAP bm;
	HBITMAP hBmp;

	hBmp=(HBITMAP)::LoadImage(AfxGetInstanceHandle(),szFileName,IMAGE_BITMAP,0,0, LR_LOADFROMFILE);
	if(!hBmp)
		return FALSE;

	m_bitmap.DeleteObject();
	m_bitmap.Attach(hBmp);
	
	GetObject(m_bitmap.GetSafeHandle(), sizeof(bm), &bm);
	m_nBitmapWidth=bm.bmWidth;
	m_nBitmapHeight=bm.bmHeight;
	m_rcText.SetRect(0,0,bm.bmWidth,bm.bmHeight);
	
	if(m_dwStyle & CSS_CENTERSCREEN) {
		m_nxPos=(GetSystemMetrics(SM_CXFULLSCREEN)-bm.bmWidth)/2;
		m_nyPos=(GetSystemMetrics(SM_CYFULLSCREEN)-bm.bmHeight)/2;
		}
	else if(m_dwStyle & CSS_CENTERAPP) {
		CRect rcParentWindow;
		ASSERT(m_pWndParent != NULL);
		m_pWndParent->GetWindowRect(&rcParentWindow);
		m_nxPos=rcParentWindow.left+(rcParentWindow.right-rcParentWindow.left-bm.bmWidth)/2;
		m_nyPos=rcParentWindow.top+(rcParentWindow.bottom-rcParentWindow.top-bm.bmHeight)/2;
		}

	if(transparency != -1) {
		m_hRegion=CreateRgnFromBitmap((HBITMAP)m_bitmap.GetSafeHandle(),transparency);
		SetWindowRgn(m_hRegion,TRUE);
		}

	return TRUE;
	}

void CSplashScreenEx::SetTextFont(LPCTSTR szFont,int nSize,int nStyle) {
	LOGFONT lf;

	m_myFont.DeleteObject();
	m_myFont.CreatePointFont(nSize,szFont);
	m_myFont.GetLogFont(&lf);
	
	if(nStyle & CSS_TEXT_BOLD)
		lf.lfWeight = FW_BOLD;
	else
		lf.lfWeight = FW_NORMAL;
	
	if(nStyle & CSS_TEXT_ITALIC)
		lf.lfItalic=TRUE;
	else
		lf.lfItalic=FALSE;
	
	if(nStyle & CSS_TEXT_UNDERLINE)
		lf.lfUnderline=TRUE;
	else
		lf.lfUnderline=FALSE;

	m_myFont.DeleteObject();
	m_myFont.CreateFontIndirect(&lf);
	}

void CSplashScreenEx::SetTextDefaultFont() {
	LOGFONT lf;
	CFont *myFont=CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT));

	myFont->GetLogFont(&lf);
	m_myFont.DeleteObject();
	m_myFont.CreateFontIndirect(&lf);
	}

void CSplashScreenEx::SetText(LPCTSTR szText) {

	m_strText=szText;
	RedrawWindow();
	}

void CSplashScreenEx::SetTextColor(COLORREF crTextColor) {
	
	m_crTextColor=crTextColor;
	RedrawWindow();
	}

void CSplashScreenEx::SetTextRect(CRect& rcText) {
	RECT rc;
	
	GetClientRect(&rc);
	ASSERT(rc.right != 0);			// in questo caso (se si usa auto-centratura), va chiamata DOPO Show()!
	m_rcText=rcText;
	if(m_rcText.right == 0)			// se xSize=0, centra rispetto a finestra!
		m_rcText.right=rc.right-m_rcText.left;
	if(m_rcText.right == 0)
		m_rcText.right=rc.right-m_rcText.left;
	if(m_rcText.top < 0) {
		m_rcText.top=rc.bottom+m_rcText.top;
		m_rcText.bottom=rc.bottom;
		}
	RedrawWindow();
	}

void CSplashScreenEx::SetTextFormat(UINT uTextFormat) {
	
	m_uTextFormat=uTextFormat;
	}

void CSplashScreenEx::Show() {
	
	SetWindowPos(NULL,m_nxPos,m_nyPos,m_nBitmapWidth,m_nBitmapHeight,SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);

	if((m_dwStyle & CSS_FADEIN) && (m_fnAnimateWindow!=NULL))	{
		m_fnAnimateWindow(m_hWnd,500,AW_BLEND);
		}
	else
		ShowWindow(SW_SHOW);
	
	if(m_dwTimeout != 0)
		SetTimer(0,m_dwTimeout,NULL);
	}

void CSplashScreenEx::Hide() {

	if((m_dwStyle & CSS_FADEOUT) && (m_fnAnimateWindow!=NULL))
		m_fnAnimateWindow(m_hWnd,200,AW_HIDE | AW_BLEND);
	else
		ShowWindow(SW_HIDE);

	DestroyWindow();
	}

HRGN CSplashScreenEx::CreateRgnFromBitmap(HBITMAP hBmp, COLORREF color) {
	HRGN hRegion;
	BITMAP bm;
	int nWidth;
	int nHeight;

	hRegion = CreateRectRgn(0, 0, 0, 0);
	GetObject(hBmp, sizeof(BITMAP), &bm );
	nWidth = bm.bmWidth;
	nHeight = bm.bmHeight;

	CDC *dc=GetDC();
	HDC hDcBmp = CreateCompatibleDC(dc->m_hDC);
	SelectObject(hDcBmp, hBmp);

	for(int j=0; j<nHeight; j++ ) {
		for(int i=0; i<nWidth; i++) {
			if(GetPixel(hDcBmp,i, j) == color)
				continue;

			int start = i;

			while((i < nWidth) && (GetPixel(hDcBmp, i, j) != color))
				i++;
			
			HRGN hr=CreateRectRgn(start, j, i, j + 1);
			int z=CombineRgn(hRegion, hRegion, hr, RGN_OR);
			if(hr)
				DeleteObject(hr);
			}
		}
	
	DeleteDC(hDcBmp);
	ReleaseDC(dc);
	return hRegion;
	}

void CSplashScreenEx::DrawWindow(CDC *pDC) {
	CDC memDC;
	CBitmap *pOldBitmap;
	
	// Blit Background
	memDC.CreateCompatibleDC(pDC);
	pOldBitmap=memDC.SelectObject(&m_bitmap);
	pDC->BitBlt(0,0,m_nBitmapWidth,m_nBitmapHeight,&memDC,0,0,SRCCOPY);
	memDC.SelectObject(pOldBitmap);

	// Draw Text
	CFont *pOldFont;
	pOldFont=pDC->SelectObject(&m_myFont);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(m_crTextColor);

	pDC->DrawText(m_strText,-1,m_rcText,m_uTextFormat);

	pDC->SelectObject(pOldFont);
	}

BEGIN_MESSAGE_MAP(CSplashScreenEx, CWnd)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_MESSAGE(WM_PRINTCLIENT, OnPrintClient)
	ON_WM_TIMER()
END_MESSAGE_MAP()

// CSplashScreenEx message handlers

BOOL CSplashScreenEx::OnEraseBkgnd(CDC* pDC) {
	return TRUE;
	}

void CSplashScreenEx::OnPaint() {
	CPaintDC dc(this); // device context for painting

	DrawWindow(&dc);
	}

LRESULT CSplashScreenEx::OnPrintClient(WPARAM wParam, LPARAM lParam) {
	CDC* pDC = CDC::FromHandle((HDC)wParam);
	
	DrawWindow(pDC);
	return 1;
	}

BOOL CSplashScreenEx::PreTranslateMessage(MSG* pMsg) {

	// If a key is pressed, Hide the Splash Screen and destroy it
	if(m_dwStyle & CSS_HIDEONCLICK) {
		if(pMsg->message == WM_KEYDOWN ||
			pMsg->message == WM_SYSKEYDOWN ||
			pMsg->message == WM_LBUTTONDOWN ||
			pMsg->message == WM_RBUTTONDOWN ||
			pMsg->message == WM_MBUTTONDOWN ||
			pMsg->message == WM_NCLBUTTONDOWN ||
			pMsg->message == WM_NCRBUTTONDOWN ||
			pMsg->message == WM_NCMBUTTONDOWN) {
			Hide();
			return TRUE;
			}
		}

	return CWnd::PreTranslateMessage(pMsg);
	}

void CSplashScreenEx::OnTimer(UINT nIDEvent) {

	KillTimer(0);
	Hide();
	}

void CSplashScreenEx::PostNcDestroy() {

	CWnd::PostNcDestroy();
	delete this;
	}



/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage

IMPLEMENT_DYNAMIC(CVidsendPropPage, CPropertySheet)

CVidsendPropPage::CVidsendPropPage(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

CVidsendPropPage::CVidsendPropPage(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
}

CVidsendPropPage::~CVidsendPropPage()
{
}


BEGIN_MESSAGE_MAP(CVidsendPropPage, CPropertySheet)
	//{{AFX_MSG_MAP(CVidsendPropPage)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage message handlers

/////////////////////////////////////////////////////////////////////////////
// CVidsendDocPropPage0 property page

IMPLEMENT_DYNCREATE(CVidsendDocPropPage0, CPropertyPage)

CVidsendDocPropPage0::CVidsendDocPropPage0(CVidsendDoc *p) : CPropertyPage(CVidsendDocPropPage0::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDocPropPage0)
	m_FullScreen = FALSE;
	m_4_3 = FALSE;
	m_DoubleSize = FALSE;
	m_BN = FALSE;
	m_SStereo = FALSE;
	m_Buffers = 0;
	m_ResizeAll = FALSE;
	m_bBuffers = FALSE;
	m_SMono = FALSE;
	//}}AFX_DATA_INIT
	myParent=p;
	isInitialized=FALSE;
	}

CVidsendDocPropPage0::~CVidsendDocPropPage0()
{
}

void CVidsendDocPropPage0::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDocPropPage0)
	DDX_Check(pDX, IDC_CHECK1, m_FullScreen);
	DDX_Check(pDX, IDC_CHECK2, m_4_3);
	DDX_Check(pDX, IDC_CHECK3, m_DoubleSize);
	DDX_Check(pDX, IDC_CHECK4, m_BN);
	DDX_Check(pDX, IDC_CHECK5, m_SStereo);
	DDX_Slider(pDX, IDC_SLIDER1, m_Buffers);
	DDX_Check(pDX, IDC_CHECK10, m_ResizeAll);
	DDX_Check(pDX, IDC_CHECK7, m_bBuffers);
	DDX_Check(pDX, IDC_CHECK6, m_SMono);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDocPropPage0, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDocPropPage0)
	ON_BN_CLICKED(IDC_CHECK7, OnCheck7)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDocPropPage0 message handlers


BOOL CVidsendDocPropPage0::OnInitDialog() {

	CPropertyPage::OnInitDialog();
	
	m_BN=myParent->Opzioni & CVidsendDoc::fmt_bn ? 1 : 0;
	m_4_3=myParent->Opzioni & CVidsendDoc::fmt4_3 ? 1 : 0;
	m_FullScreen=myParent->Opzioni & CVidsendDoc::fmt_full ? 1 : 0;
	m_DoubleSize=myParent->Opzioni & CVidsendDoc::fmt_double ? 1 : 0;
	m_ResizeAll=myParent->Opzioni & CVidsendDoc::fmt_resize ? 1 : 0;
	m_SStereo=myParent->Opzioni & CVidsendDoc::sstereo ? 1 : 0;
	((CSliderCtrl *)GetDlgItem(IDC_SLIDER1))->SetRange(1,50,TRUE);
	((CSliderCtrl *)GetDlgItem(IDC_SLIDER1))->SetTicFreq(5);
	if(myParent->numBuffers) {
		m_Buffers=myParent->numBuffers;
		m_bBuffers=TRUE;
		}
	else {
		m_bBuffers=FALSE;
		}
	UpdateData(FALSE);
	updateDaCheck();
	
	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

void CVidsendDocPropPage0::OnCheck7() {
	updateDaCheck();	
	}

void CVidsendDocPropPage0::updateDaCheck() {
	
	UpdateData();
	GetDlgItem(IDC_SLIDER1)->EnableWindow(m_bBuffers != 0);
	}


/////////////////////////////////////////////////////////////////////////////
// CVidsendDocPropPage1 property page

IMPLEMENT_DYNCREATE(CVidsendDocPropPage1, CPropertyPage)

CVidsendDocPropPage1::CVidsendDocPropPage1(CVidsendDoc *p) : CPropertyPage(CVidsendDocPropPage1::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDocPropPage1)
	m_Authenticate = FALSE;
	m_bProxy = FALSE;
	m_AuthWWW = _T("");
	m_User = _T("");
	m_Pasw = _T("");
	m_bConnetti = FALSE;
	//}}AFX_DATA_INIT
	myParent=p;
	isInitialized=FALSE;
	}

CVidsendDocPropPage1::~CVidsendDocPropPage1()
{
}

void CVidsendDocPropPage1::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDocPropPage1)
	DDX_Control(pDX, IDC_COMBO1, m_ConnettiA);
	DDX_Control(pDX, IDC_IPADDRESS1, m_Proxy);
	DDX_Check(pDX, IDC_CHECK1, m_Authenticate);
	DDX_Check(pDX, IDC_CHECK2, m_bProxy);
	DDX_Text(pDX, IDC_EDIT5, m_AuthWWW);
	DDV_MaxChars(pDX, m_AuthWWW, 128);
	DDX_Text(pDX, IDC_EDIT1, m_User);
	DDV_MaxChars(pDX, m_User, 31);
	DDX_Text(pDX, IDC_EDIT3, m_Pasw);
	DDV_MaxChars(pDX, m_Pasw, 31);
	DDX_Check(pDX, IDC_CHECK3, m_bConnetti);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDocPropPage1, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDocPropPage1)
	ON_BN_CLICKED(IDC_CHECK1, OnCheck1)
	ON_BN_CLICKED(IDC_CHECK2, OnCheck2)
	ON_BN_CLICKED(IDC_CHECK3, OnCheck3)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDocPropPage1 message handlers

BOOL CVidsendDocPropPage1::OnInitDialog() {

	CPropertyPage::OnInitDialog();
	m_bConnetti = myParent->Opzioni & CVidsendDoc::autoRASconnect ? 1 : 0;
	m_Authenticate = myParent->Opzioni & CVidsendDoc::authenticate ? 1 : 0;
	m_bProxy = myParent->Opzioni & CVidsendDoc::usaProxy ? 1 : 0;
	UpdateData(FALSE);
	updateDaCheck();
		
	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	}

void CVidsendDocPropPage1::OnCheck1() {
	updateDaCheck();
	}

void CVidsendDocPropPage1::OnCheck2() {
	updateDaCheck();
	}

void CVidsendDocPropPage1::OnCheck3() {
	updateDaCheck();
	}

void CVidsendDocPropPage1::updateDaCheck() {
	
	UpdateData();
	GetDlgItem(IDC_EDIT1)->EnableWindow(m_Authenticate);
	GetDlgItem(IDC_EDIT3)->EnableWindow(m_Authenticate);
	GetDlgItem(IDC_EDIT5)->EnableWindow(m_Authenticate);
	GetDlgItem(IDC_COMBO1)->EnableWindow(m_bConnetti);
	GetDlgItem(IDC_IPADDRESS1)->EnableWindow(m_bProxy);
	}


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage0 property page

IMPLEMENT_DYNCREATE(CVidsendDoc2PropPage0, CPropertyPage)

CVidsendDoc2PropPage0::CVidsendDoc2PropPage0(CVidsendDoc2 *p,struct QUALITY_MODEL_V *qv,struct QUALITY_MODEL_A *qa) : CPropertyPage(CVidsendDoc2PropPage0::IDD) {
	//{{AFX_DATA_INIT(CVidsendDoc2PropPage0)
	m_IPAddress = 0;
	m_ServerAudio = FALSE;
	m_Bandwidth = _T("");
	m_PortaV = 0;
	m_PortaA = 0;
	m_ServerStream = FALSE;
	m_ServerVideo = FALSE;
	m_TipoVideo = -1;
	m_QualityV = 0;
	//}}AFX_DATA_INIT
	myParent=p;
	if(qv)
		memcpy(&m_QV,qv,sizeof(struct QUALITY_MODEL_V));
	else
		ZeroMemory(&m_QV,sizeof(struct QUALITY_MODEL_V));
	if(qa)
		memcpy(&m_QA,qa,sizeof(struct QUALITY_MODEL_A));
	else
		ZeroMemory(&m_QA,sizeof(struct QUALITY_MODEL_A));
	isInitialized=FALSE;
	m_CompressorV=m_QV.compressor;
	m_CompressorA=m_QA.compressor;
	}

CVidsendDoc2PropPage0::~CVidsendDoc2PropPage0() {
	}

void CVidsendDoc2PropPage0::DoDataExchange(CDataExchange* pDX) {
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc2PropPage0)
	DDX_Control(pDX, IDC_COMBO4, m_ComboCompressorV);
	DDX_Control(pDX, IDC_COMBO6, m_ComboCompressorA);
	DDX_Control(pDX, IDC_COMBO2, m_ComboImageSize);
	DDX_Control(pDX, IDC_COMBO1, m_ComboFps);
	DDX_Control(pDX, IDC_COMBO3, m_Formato);
	DDX_CBIndex(pDX, IDC_COMBO12, m_IPAddress);
	DDX_Check(pDX, IDC_CHECK4, m_ServerAudio);
	DDX_Text(pDX, IDC_EDIT4, m_Bandwidth);
	DDX_Text(pDX, IDC_EDIT1, m_PortaV);
	DDV_MinMaxUInt(pDX, m_PortaV, 1, 65535);
	DDX_Text(pDX, IDC_EDIT3, m_PortaA);
	DDV_MinMaxUInt(pDX, m_PortaA, 1, 65535);
	DDX_Check(pDX, IDC_CHECK1, m_ServerStream);
	DDX_Check(pDX, IDC_CHECK8, m_ServerVideo);
	DDX_Radio(pDX, IDC_RADIO1, m_TipoVideo);
	DDX_Slider(pDX, IDC_SLIDER3, m_QualityV);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc2PropPage0, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc2PropPage0)
	ON_BN_CLICKED(IDC_CHECK1, OnCheck1)
	ON_CBN_SELCHANGE(IDC_COMBO4, OnSelchangeCombo4)
	ON_CBN_SELCHANGE(IDC_COMBO1, OnSelchangeCombo1)
	ON_CBN_SELCHANGE(IDC_COMBO2, OnSelchangeCombo2)
	ON_CBN_SELCHANGE(IDC_COMBO6, OnSelchangeCombo6)
	ON_BN_CLICKED(IDC_CHECK4, OnCheck4)
	ON_BN_CLICKED(IDC_CHECK8, OnCheck8)
	ON_BN_CLICKED(IDC_CHECK3, OnCheck3)
	ON_BN_CLICKED(IDC_RADIO1, OnRadio1)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_RADIO2, OnRadio1)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage0 message handlers

BOOL CVidsendDoc2PropPage0::OnInitDialog() {
	char myBuf[128];
	DWORD n;
	int i;
	CString S;

	CPropertyPage::OnInitDialog();
	
	if(!CWebSrvSocket2_base::getMyIPAddress(myBuf))
		((CComboBox *)GetDlgItem(IDC_COMBO12))->AddString("<non disponibile>");
	else {
		for(i=0; i<=9; i++) {
		if(CWebSrvSocket2_base::getMyIPAddress(myBuf,i)) {
			S=myBuf;
			((CComboBox *)GetDlgItem(IDC_COMBO12))->AddString(S);
			}
		else
			break;
			}
		}
	
	m_IPAddress=0;

	if(myParent) {
		m_ServerStream=myParent->Opzioni & CVidsendDoc2::sendVideo ? 1 : 0;
		m_ServerVideo=myParent->Opzioni & CVidsendDoc2::maySendVideo ? 1 : 0;
		m_ServerAudio=myParent->Opzioni & CVidsendDoc2::maySendAudio ? 1 : 0;
		m_TipoVideo=myParent->Opzioni & CVidsendDoc2::videoType ? 1 : 0;
		}
	else {		// questo accade solo in nmvidsend, quando devo far combaciare il format video della webcam con quello del programma...
		m_ServerStream=1;
		m_ServerVideo=1;
		}
	m_PortaV=VIDEO_SOCKET;
	m_PortaA=AUDIO_SOCKET;
	n=enumCompressorV(&m_ComboCompressorV,m_CompressorV);
	m_ComboCompressorV.SetCurSel(HIWORD(n));
//	m_QV=myParent->myQV;
	itoa(m_QV.fps,myBuf,10);
	m_ComboFps.SelectString(-1,myBuf);
	((CSliderCtrl *)GetDlgItem(IDC_SLIDER3))->SetRange(0,10000,TRUE);
	((CSliderCtrl *)GetDlgItem(IDC_SLIDER3))->SetTicFreq(1000);
	m_QualityV=m_QV.quality;
	S.Format("%ux%u",m_QV.imageSize.right,m_QV.imageSize.bottom);
//	AfxMessageBox(S);		// spesso il debugger non ti mostra imageSize...!!@#!@
	m_ComboImageSize.SelectString(-1,S);
	if(myParent && myParent->theTV->biRawBitmap.biCompression != 0)
		*(DWORD *)myBuf=myParent ? myParent->theTV->biRawBitmap.biCompression : 0;
	else
		*(DWORD *)myBuf=mmioFOURCC('R','G','B',0);
	myBuf[4]=0;
	m_Formato.AddString(myBuf);			// per ora ne metto solo 1 (non so come enumerarli!)
	m_Formato.SetCurSel(0);
	
//	m_QA=myParent->myQA;
	n=enumCompressorA(&m_ComboCompressorA,m_CompressorA);
	m_ComboCompressorA.SetCurSel(HIWORD(n));

	UpdateData(FALSE);
	updateDaCheck();
	updateBWH();

	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

DWORD CVidsendDoc2PropPage0::enumCompressorV(CComboBox *c,DWORD v) {
	int i,j,n=0;
	HIC hic;
	ICINFO ii;
	CString S;

	c->AddString("<nessuna>");
	c->SetItemData(0,0);
	for(i=0; ICInfo(ICTYPE_VIDEO, i, &ii); i++) {
    hic=ICOpen(ii.fccType, ii.fccHandler, ICMODE_QUERY); 
    if(hic) { 
        // Skip this compressor if it can't handle the format. 
/*      if(fccType == ICTYPE_VIDEO && pvIn != NULL && 
        ICDecompressQuery(hic, pvIn, NULL) != ICERR_OK) { 
	      ICClose(hic); 
				continue;
				}*/
      ICGetInfo(hic, &ii, sizeof(ii)); 
      ICClose(hic); 
			}
 		S=ii./*szDescription*/ szName;
		c->AddString(S);
		c->SetItemData(i+1,ii.fccHandler);
		if(v==ii.fccHandler && !n) 
			n=i+1;
		}
	return MAKELONG(i,n);
	}

BOOL CALLBACK FormatEnumProc(HACMDRIVERID hadid, LPACMFORMATDETAILS pafd, DWORD dwInstance, DWORD fdwSupport) {
	int i;

	i=((CComboBox *)dwInstance)->GetCount();
	((CComboBox *)dwInstance)->SetItemData(i-1,pafd->dwFormatTag);
	// in realta', qui arriva una chiamata per ogni FormatIndex, ossia per ogni formato riconosciuto da questo (fotmatTag) comrpessore...

  return TRUE; // Continue enumerating.
	}


BOOL CALLBACK /*ACMDRIVERENUMCB*/ acmEnumCallback(
  HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport) {
  HACMDRIVER had = NULL;
	ACMDRIVERDETAILS acm;
	CString S;
	int i;

//	if(fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CODEC)		// mettere controllo?
	acm.cbStruct=sizeof(ACMDRIVERDETAILS);
	if(!acmDriverDetails(hadid,&acm,0)) {
		S=acm.szShortName;
		((CComboBox *)dwInstance)->AddString(S);

		if(!acmDriverOpen(&had, hadid, 0)) {
			DWORD dwSize = 0;
			i=acmMetrics((HACMOBJ)had, ACM_METRIC_MAX_SIZE_FORMAT, &dwSize);
			if(dwSize < sizeof(WAVEFORMATEX)) 
				dwSize = sizeof(WAVEFORMATEX); // for MS-PCM
			WAVEFORMATEX *pwf = (WAVEFORMATEX*)malloc(dwSize);
			ZeroMemory(pwf, dwSize);
			pwf->cbSize = LOWORD(dwSize) - sizeof(WAVEFORMATEX);
			pwf->wFormatTag = WAVE_FORMAT_UNKNOWN;
			ACMFORMATDETAILS fd;
			ZeroMemory(&fd,sizeof(fd));
			fd.cbStruct = sizeof(fd);
			fd.pwfx = pwf;
			fd.cbwfx = dwSize;
			fd.dwFormatTag = WAVE_FORMAT_UNKNOWN;
			i=acmFormatEnum(had,&fd,FormatEnumProc,dwInstance,ACM_FORMATENUMF_INPUT);  
			free(pwf);

			acmDriverClose(had, 0);
			}

		}
	return TRUE;
	}
 
DWORD CVidsendDoc2PropPage0::enumCompressorA(CComboBox *c,DWORD v) {
	int i,j,n=0;

	c->AddString("<nessuna>");
	c->SetItemData(0,0);
	acmDriverEnum((ACMDRIVERENUMCB)acmEnumCallback,(DWORD)c,0);
	j=c->GetCount();
	for(i=0; i<j; i++) {
		if(c->GetItemData(i) == v) {
			n=i;
			break;
			}
		}
	return MAKELONG(j,n);
	}


void CVidsendDoc2PropPage0::OnSelchangeCombo4() {
	int i=m_ComboCompressorV.GetCurSel();
	if(i != CB_ERR)
		m_CompressorV=m_ComboCompressorV.GetItemData(i);
	updateBWH();
	}


void CVidsendDoc2PropPage0::OnSelchangeCombo1() {
	char myBuf[32];
	int i=m_ComboFps.GetCurSel();
	if(i != CB_ERR) {
		m_ComboFps.GetLBText(i,myBuf);
		m_QV.fps=atoi(myBuf);
		}
	updateBWH();
	}

void CVidsendDoc2PropPage0::OnSelchangeCombo2() {
	int i=m_ComboImageSize.GetCurSel();
	CString S;
	
	m_ComboImageSize.GetLBText(i,S);
	m_QV.imageSize.right=atoi((LPCTSTR)S);
	m_QV.imageSize.bottom=atoi((LPCTSTR)S.Mid(S.Find('x'))+1);
	updateBWH();
	}


void CVidsendDoc2PropPage0::OnSelchangeCombo6() {
	int i=m_ComboCompressorA.GetCurSel();
	if(i != CB_ERR)
		m_CompressorA=m_ComboCompressorA.GetItemData(i);
	updateBWH();
	}

void CVidsendDoc2PropPage0::updateDaCheck() {
	
	UpdateData();
	GetDlgItem(IDC_EDIT1)->EnableWindow(m_ServerStream && m_ServerVideo);
	GetDlgItem(IDC_EDIT3)->EnableWindow(m_ServerStream && m_ServerAudio);
	GetDlgItem(IDC_CHECK4)->EnableWindow(m_ServerStream);
	GetDlgItem(IDC_CHECK8)->EnableWindow(m_ServerStream);
	GetDlgItem(IDC_COMBO1)->EnableWindow(m_ServerStream && m_ServerVideo);
	GetDlgItem(IDC_COMBO2)->EnableWindow(m_ServerStream && m_ServerVideo);
	GetDlgItem(IDC_COMBO4)->EnableWindow(m_ServerStream && m_ServerVideo && !m_TipoVideo);
	GetDlgItem(IDC_COMBO6)->EnableWindow(m_ServerStream && m_ServerAudio);
	}

void CVidsendDoc2PropPage0::OnCheck1() {
	// stream
	updateDaCheck();
	}

void CVidsendDoc2PropPage0::OnCheck4() {
	// audio
	updateDaCheck();
	updateBWH();
	}

void CVidsendDoc2PropPage0::OnCheck8() {
	// video
	updateDaCheck();
	updateBWH();
	}

void CVidsendDoc2PropPage0::OnCheck3() {
	updateDaCheck();
	}



void CVidsendDoc2PropPage0::OnRadio1() {
	// video gia' compresso/no

	updateDaCheck();
	updateBWH();
	}

void CVidsendDoc2PropPage0::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
	
	if(nSBCode == SB_THUMBPOSITION || nSBCode == SB_PAGERIGHT || nSBCode == SB_PAGELEFT || nSBCode == SB_LINERIGHT || nSBCode == SB_LINELEFT || nSBCode == SB_LEFT || nSBCode == SB_RIGHT) {
//	if(nSBCode == SB_THUMBPOSITION) {
		updateBWH();
		UpdateData();
		m_QV.quality=m_QualityV;
		}
	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
	}

void CVidsendDoc2PropPage0::updateBWH() {
	int i;

	UpdateData();
	i=theApp.theServer->calcBandWidth(m_ServerVideo,&m_QV.imageSize,m_QV.bpp,m_TipoVideo ? -1 : m_CompressorV,m_QV.quality,m_QV.fps,
																m_ServerAudio,m_QA.samplesPerSec,m_QA.bitsPerSample,m_QA.channels,m_CompressorA,m_QA.quality);
	m_Bandwidth.Format("%uKbps",i/128);			//*8 e /1K
	UpdateData(FALSE);
	}	

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage0Bis property page

IMPLEMENT_DYNCREATE(CVidsendDoc2PropPage0Bis, CPropertyPage)

CVidsendDoc2PropPage0Bis::CVidsendDoc2PropPage0Bis(CVidsendDoc2 *p,struct QUALITY_MODEL_V *qv,struct QUALITY_MODEL_A *qa) : CPropertyPage(CVidsendDoc2PropPage0Bis::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc2PropPage0Bis)
	m_IPAddress = 0;
	m_Port=0;
	m_QualitaVideo = 0;
	m_QualitaAudio = 0;
	m_Bandwidth = -1;
	m_ServerAudio = FALSE;
	m_Schede = -1;
	//}}AFX_DATA_INIT
	myParent=p;
	isInitialized=FALSE;
	memcpy(&m_QV,qv,sizeof(struct QUALITY_MODEL_V));
	memcpy(&m_QA,qa,sizeof(struct QUALITY_MODEL_A));
	}

CVidsendDoc2PropPage0Bis::~CVidsendDoc2PropPage0Bis()
{
}

void CVidsendDoc2PropPage0Bis::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc2PropPage0Bis)
	DDX_Text(pDX, IDC_EDIT7, m_Port);
	DDX_Slider(pDX, IDC_SLIDER1, m_QualitaVideo);
	DDX_Slider(pDX, IDC_SLIDER2, m_QualitaAudio);
	DDX_CBIndex(pDX, IDC_COMBO5, m_Bandwidth);
	DDX_CBIndex(pDX, IDC_COMBO1, m_Schede);
	DDX_CBIndex(pDX, IDC_COMBO6, m_IPAddress);
	DDX_Check(pDX, IDC_CHECK4, m_ServerAudio);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc2PropPage0Bis, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc2PropPage0Bis)
	ON_BN_CLICKED(IDC_CHECK8, OnCheck8)
	ON_BN_CLICKED(IDC_CHECK4, OnCheck4)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_CHECK1, OnCheck1)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_BN_CLICKED(IDC_BUTTON2, OnButton2)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage0Bis message handlers

BOOL CVidsendDoc2PropPage0Bis::OnInitDialog() {
	char myBuf[128],myBuf2[256];
	CString S,S2;
	int i;

	CPropertyPage::OnInitDialog();
	
	if(!CWebSrvSocket2_base::getMyIPAddress(myBuf))
		((CComboBox *)GetDlgItem(IDC_COMBO6))->AddString("<non disponibile>");
	else {
		for(i=0; i<=9; i++) {
		if(CWebSrvSocket2_base::getMyIPAddress(myBuf,i)) {
			S=myBuf;
			((CComboBox *)GetDlgItem(IDC_COMBO6))->AddString(S);
			}
		else
			break;
			}
		}
	
	m_IPAddress=0;

	m_Port=VIDEO_SOCKET;
	((CSliderCtrl *)GetDlgItem(IDC_SLIDER1))->SetRange(0,6,TRUE);
	GetDlgItem(IDC_SLIDER1)->EnableWindow(myParent->Opzioni & CVidsendDoc2::maySendVideo);
	((CSliderCtrl *)GetDlgItem(IDC_SLIDER2))->SetRange(0,2,TRUE);

	((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("<nessuna>");
	for(i=0; i<=9; i++) {
		if(myParent->theTV->GetDriverDescription(i,myBuf,64,myBuf2,64)) {
			S=myBuf;
			S2=myBuf2;
			((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString(S+" - "+S2);
			}
		else
			break;
		}
	maxSchede=i+1;
	m_Schede=myParent->theTV->theCapture+1;
	
	i=myParent->getAVstep(&m_QV,&m_QA);
	m_QualitaVideo=LOWORD(i);
	m_QualitaAudio=HIWORD(i);

	m_ServerAudio=myParent->Opzioni & CVidsendDoc2::maySendAudio ? 1 : 0;

	UpdateData(FALSE);
	updateBWH();		// dopo updateData!
	updateDaCheck();

	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

void CVidsendDoc2PropPage0Bis::updateVar() {
	DWORD n;

	m_ServerAudio=myParent->Opzioni & CVidsendDoc2::maySendAudio ? 1 : 0;
	n=myParent->getAVstep(&m_QV,&m_QA);
	m_QualitaVideo=LOWORD(n);
	m_QualitaAudio=HIWORD(n);
	m_QV.imageSize=myParent->qsv[m_QualitaVideo].imageSize;
	m_QV.fps=myParent->qsv[m_QualitaVideo].fps;
	m_QV.quality=myParent->qsv[m_QualitaVideo].quality;
	m_QA.quality=myParent->qsa[m_QualitaAudio].quality;
	}

void CVidsendDoc2PropPage0Bis::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
	
//	if(nSBCode == SB_THUMBPOSITION) {
	m_QV.imageSize=myParent->qsv[m_QualitaVideo].imageSize;
	m_QV.fps=myParent->qsv[m_QualitaVideo].fps;
	m_QV.quality=myParent->qsv[m_QualitaVideo].quality;
	m_QA.quality=myParent->qsa[m_QualitaAudio].quality;
		updateBWH();
//		}
	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
	}


void CVidsendDoc2PropPage0Bis::OnCheck8() {

	updateDaCheck();
	updateBWH();
	}

void CVidsendDoc2PropPage0Bis::OnCheck4() {

	updateDaCheck();
	updateBWH();
	}

void CVidsendDoc2PropPage0Bis::OnCheck1() {
	
	updateDaCheck();
	}

void CVidsendDoc2PropPage0Bis::subUpdateBWH(int n) {
	int i=0;

	if(n<1200) {		// erano bytes, ma qua si parla di bits!
		i=1;
		}
	else if(n<1800) {
		i=2;
		}
	else if(n<4200) {
		i=3;
		}
	else if(n<7000) {
		i=4;
		}
	else if(n<8000) {
		i=5;
		}
	else if(n<16000) {
		i=6;
		}
	else if(n<80000) {
		i=7;
		}
	else if(n<250000) {
		i=8;
		}
	else if(n<1250000) {
		i=9;
		}
	else  {
		i=10;
		}
	m_Bandwidth=i;
	UpdateData(FALSE);
	}

void CVidsendDoc2PropPage0Bis::updateBWH() {
	int i;

	UpdateData();
//	i=myParent->calcBandWidth(myParent->Opzioni & CVidsendDoc2::maySendVideo ? m_QualitaVideo : -1,m_ServerAudio ? m_QualitaAudio : -1);
	i=myParent->calcBandWidth(myParent->Opzioni & CVidsendDoc2::maySendVideo,
		&m_QV.imageSize,m_QV.bpp,m_QV.compressor,m_QV.quality,m_QV.fps,
		m_ServerAudio,m_QA.samplesPerSec,m_QA.bitsPerSample,m_QA.channels,m_QA.compressor,m_QA.quality);
	if(i>0)
		subUpdateBWH(i);
	else if(myParent->Opzioni & CVidsendDoc2::maySendVideo || m_ServerAudio) {
		AfxMessageBox("La configurazione richiesta non è supportata dai compressori audio/video attuali.\nProvare a cambiarli da \"impostazioni avanzate\"",MB_ICONEXCLAMATION);
		m_Bandwidth=-1;
		UpdateData(FALSE);
		}

	}

void CVidsendDoc2PropPage0Bis::updateDaCheck() {

	UpdateData();
	GetDlgItem(IDC_SLIDER2)->EnableWindow(m_ServerAudio);
	}


void CVidsendDoc2PropPage0Bis::OnButton1() {

	if(AfxMessageBox("Ripristinare le impostazioni originarie?",MB_YESNO | MB_ICONQUESTION) == IDYES) {
		m_QualitaVideo=3;
		m_QualitaAudio=0;
		UpdateData(FALSE);
		updateBWH();
		}
	}

void CVidsendDoc2PropPage0Bis::OnButton2() {

//#ifndef _NEWMEET_MODE		// alla fine lo lasciamo...
	CVidsendPropPage mySheet("Proprietà streaming avanzate",(CWnd *)myParent->getView());
	CVidsendDoc2PropPage0 myPage0(myParent,&m_QV,&m_QA);

	if(AfxMessageBox("La modifica delle impostazioni avanzate può portare al malfunzionamento del programma: continuare?",MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO) {
		return;
		}
	UpdateData();
	myParent->Opzioni &= (0xffff0000 | CVidsendDoc2::videoType);
	myParent->Opzioni |= 1 ? CVidsendDoc2::sendVideo : 0;
	myParent->Opzioni |= 1 ? CVidsendDoc2::maySendVideo : 0;
	myParent->Opzioni |= m_ServerAudio ? CVidsendDoc2::maySendAudio : 0;

//	myParent->myQV.imageSize=myParent->qsv[m_QualitaVideo].imageSize;
//	myParent->myQV.fps=myParent->qsv[m_QualitaVideo].fps;
//	myParent->myQV.quality=myParent->qsv[m_QualitaVideo].quality;
//	myParent->myQA.quality=myParent->qsa[m_QualitaVideo].quality;

	mySheet.AddPage(&myPage0);
	if(mySheet.DoModal() == IDOK) {

		if(myPage0.isInitialized) {
			myParent->Opzioni &= 0xffff0000;
			myParent->Opzioni |= myPage0.m_ServerStream ? CVidsendDoc2::sendVideo : 0;
			myParent->Opzioni |= myPage0.m_ServerVideo ? CVidsendDoc2::maySendVideo : 0;
			myParent->Opzioni |= myPage0.m_ServerAudio ? CVidsendDoc2::maySendAudio : 0;
			myParent->Opzioni |= myPage0.m_TipoVideo ? CVidsendDoc2::videoType : 0;
			m_QV.compressor=myPage0.m_CompressorV;
			m_QV.imageSize=myPage0.m_QV.imageSize;
			m_QV.fps=myPage0.m_QV.fps;
			m_QV.quality=myPage0.m_QV.quality;
			m_QA.compressor=myPage0.m_CompressorA;
			m_QA.quality=myPage0.m_QA.quality;
			}
		}
//	updateVar();
	UpdateData(FALSE);
	updateBWH();
//#else
//	AfxMessageBox("Funzione non disponibile!",MB_ICONEXCLAMATION);
//#endif
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage1 property page

IMPLEMENT_DYNCREATE(CVidsendDoc2PropPage1, CPropertyPage)

CVidsendDoc2PropPage1::CVidsendDoc2PropPage1(CVidsendDoc2 *p) : CPropertyPage(CVidsendDoc2PropPage1::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc2PropPage1)
	m_Forza_BN = FALSE;
	m_trasmAudio = -1;
	m_trasmVideo = -1;
	m_ForzaAudio = -1;
	m_ImposeDateTime = -1;
	m_ImposeTextPos = -1;
	m_ImposeText = _T("");
	m_Capovolgi = FALSE;
	m_Specchio = FALSE;
	//}}AFX_DATA_INIT
	isInitialized=FALSE;
	myParent=p;
	}

CVidsendDoc2PropPage1::~CVidsendDoc2PropPage1()
{
}

void CVidsendDoc2PropPage1::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc2PropPage1)
	DDX_Check(pDX, IDC_CHECK3, m_Forza_BN);
	DDX_Radio(pDX, IDC_RADIO1, m_trasmAudio);
	DDX_Radio(pDX, IDC_RADIO8, m_trasmVideo);
	DDX_Radio(pDX, IDC_RADIO13, m_ForzaAudio);
	DDX_CBIndex(pDX, IDC_COMBO1, m_ImposeDateTime);
	DDX_CBIndex(pDX, IDC_COMBO7, m_ImposeTextPos);
	DDX_Check(pDX, IDC_CHECK4, m_Capovolgi);
	DDX_Check(pDX, IDC_CHECK9, m_Specchio);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc2PropPage1, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc2PropPage1)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage1 message handlers

BOOL CVidsendDoc2PropPage1::OnInitDialog() {
	char myBuf[128],myBuf2[128];
	CString S,S2;
	int i;

	CPropertyPage::OnInitDialog();
	
	m_Forza_BN=myParent->Opzioni & CVidsendDoc2::forceBN ? 1 : 0;
	m_Capovolgi=myParent->Opzioni & CVidsendDoc2::doFlip ? 1 : 0;
	m_Specchio=myParent->Opzioni & CVidsendDoc2::doMirror ? 1 : 0;
	m_ImposeDateTime=myParent->imposeDateTime;
	m_ImposeTextPos=myParent->imposeTextPos;
// FINIRE	m_ImposeText=myParent->imposeText;

	UpdateData(FALSE);
	updateDaCheck();	

	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}


void CVidsendDoc2PropPage1::updateDaCheck() {

	UpdateData();
	}

void CVidsendDoc2PropPage1::OnButton1() {		// 
	RECT myrc;
	myrc.top=(myParent->qualityBox.top * 300) / myParent->myQV.imageSize.bottom;
	myrc.left=(myParent->qualityBox.left * 400) / myParent->myQV.imageSize.right;
	myrc.bottom=(myParent->qualityBox.bottom * 300) / myParent->myQV.imageSize.bottom;		// le coord dialog sono a caso...
	myrc.right=(myParent->qualityBox.right * 400) / myParent->myQV.imageSize.right;
	myrc.top+=20;
	myrc.left+=20;
	myrc.bottom+=20;
	myrc.right+=20;
	CQualityBoxDlg myDlg(&myrc);

	if(myDlg.DoModal() == IDOK) {
		myDlg.SelectedRect.top=(myDlg.SelectedRect.top * myParent->myQV.imageSize.bottom) /300;
		myDlg.SelectedRect.left=(myDlg.SelectedRect.left * myParent->myQV.imageSize.right) /400;
		myDlg.SelectedRect.bottom=(myDlg.SelectedRect.bottom * myParent->myQV.imageSize.bottom) /300;		// le coord dialog sono a caso...
		myDlg.SelectedRect.right=(myDlg.SelectedRect.right * myParent->myQV.imageSize.right) /400;
		myParent->qualityBox=myDlg.SelectedRect;

		}
	
	}



#ifdef _NEWMEET_MODE

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage1_NM property page

IMPLEMENT_DYNCREATE(CVidsendDoc2PropPage1_NM, CPropertyPage)

CVidsendDoc2PropPage1_NM::CVidsendDoc2PropPage1_NM(CVidsendDoc2 *p) : CPropertyPage(CVidsendDoc2PropPage1_NM::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc2PropPage1_NM)
	m_Forza_BN = FALSE;
	m_Capovolgi = FALSE;
	m_Specchio = FALSE;
	m_ImposeDateTime = -1;
	m_ImposeTextPos = -1;
	m_ImposeText = _T("");
	//}}AFX_DATA_INIT
	isInitialized=FALSE;
	myParent=p;
}

CVidsendDoc2PropPage1_NM::~CVidsendDoc2PropPage1_NM()
{
}

void CVidsendDoc2PropPage1_NM::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc2PropPage1_NM)
	DDX_Check(pDX, IDC_CHECK3, m_Forza_BN);
	DDX_Check(pDX, IDC_CHECK4, m_Capovolgi);
	DDX_Check(pDX, IDC_CHECK9, m_Specchio);
	DDX_CBIndex(pDX, IDC_COMBO1, m_ImposeDateTime);
	DDX_CBIndex(pDX, IDC_COMBO7, m_ImposeTextPos);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc2PropPage1_NM, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc2PropPage1_NM)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage1_NM message handlers

BOOL CVidsendDoc2PropPage1_NM::OnInitDialog() {
	CPropertyPage::OnInitDialog();
	
	m_Forza_BN=myParent->Opzioni & CVidsendDoc2::forceBN ? 1 : 0;
	m_Capovolgi=myParent->Opzioni & CVidsendDoc2::doFlip ? 1 : 0;
	m_Specchio=myParent->Opzioni & CVidsendDoc2::doMirror ? 1 : 0;
	m_ImposeDateTime=myParent->imposeDateTime;

	UpdateData(FALSE);
	isInitialized=TRUE;
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

#endif


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage2 property page

IMPLEMENT_DYNCREATE(CVidsendDoc2PropPage2, CPropertyPage)

CVidsendDoc2PropPage2::CVidsendDoc2PropPage2(CVidsendDoc2 *p) : CPropertyPage(CVidsendDoc2PropPage2::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc2PropPage2)
	m_MaxConn = 0;
	m_AuthWWW = _T("");
	m_bDirectoryServer = FALSE;
	m_bNeedAuthenticate = FALSE;
	m_DirectoryServer = _T("");
	m_NomePerServer = _T("");
	m_TipoAutorizzazione = -1;
	m_ID = _T("");
	//}}AFX_DATA_INIT
	isInitialized=FALSE;
	mySet=NULL;
	myParent=p;
	}

CVidsendDoc2PropPage2::~CVidsendDoc2PropPage2()
{
}

void CVidsendDoc2PropPage2::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc2PropPage2)
	DDX_Control(pDX, IDC_LIST1, m_Lista);
	DDX_Control(pDX, IDC_SPIN1, m_MaxConnSpin);
	DDX_Text(pDX, IDC_EDIT1, m_MaxConn);
	DDX_Text(pDX, IDC_EDIT6, m_AuthWWW);
	DDV_MaxChars(pDX, m_AuthWWW, 127);
	DDX_Check(pDX, IDC_CHECK4, m_bDirectoryServer);
	DDX_Check(pDX, IDC_CHECK9, m_bNeedAuthenticate);
	DDX_Text(pDX, IDC_EDIT2, m_DirectoryServer);
	DDV_MaxChars(pDX, m_DirectoryServer, 127);
	DDX_Text(pDX, IDC_EDIT7, m_NomePerServer);
	DDV_MaxChars(pDX, m_NomePerServer, 32);
	DDX_Radio(pDX, IDC_RADIO13, m_TipoAutorizzazione);
	DDX_Text(pDX, IDC_EDIT8, m_ID);
	DDV_MaxChars(pDX, m_ID, 10);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc2PropPage2, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc2PropPage2)
	ON_BN_CLICKED(IDC_BUTTON1, OnInsert)
	ON_BN_CLICKED(IDC_BUTTON2, OnEdit)
	ON_BN_CLICKED(IDC_BUTTON3, OnDelete)
	ON_LBN_SELCHANGE(IDC_LIST1, OnSelchangeList1)
	ON_WM_CREATE()
	ON_BN_CLICKED(IDC_CHECK4, OnCheck4)
	ON_LBN_DBLCLK(IDC_LIST1, OnDblclkList1)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_CHECK9, OnCheck9)
	ON_BN_CLICKED(IDC_RADIO13, OnRadio13)
	ON_BN_CLICKED(IDC_RADIO1, OnRadio1)
	ON_BN_CLICKED(IDC_RADIO2, OnRadio2)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage2 message handlers

BOOL CVidsendDoc2PropPage2::OnInitDialog() {
	char myBuf[128];
	int i;

	CPropertyPage::OnInitDialog();
	
	m_MaxConn=myParent->maxConn;
	m_MaxConnSpin.SetRange(0,100);
	m_AuthWWW=myParent->authenticationWWW;
	m_TipoAutorizzazione=(myParent->Opzioni & CVidsendDoc2::needAuthenticate) >> 29;
	m_bDirectoryServer = myParent->Opzioni & CVidsendDoc2::registerServer ? 1 : 0;
	m_bNeedAuthenticate= myParent->Opzioni & CVidsendDoc2::needAuthenticateServer ? 1 : 0;
	m_DirectoryServer=myParent->directoryWWW;
	m_ID.Format("%u",myParent->myID);
	m_NomePerServer=myParent->directoryWWWLogin;
	if(m_NomePerServer.IsEmpty()) {
		gethostname(myBuf,127);
		m_NomePerServer=myBuf;
		}

	UpdateData(FALSE);
	updateDaCheck();

	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

int CVidsendDoc2PropPage2::OnCreate(LPCREATESTRUCT lpCreateStruct) {

	if (CPropertyPage::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	
	return 0;
	}

void CVidsendDoc2PropPage2::OnDestroy() {

	CPropertyPage::OnDestroy();
	
	if(mySet) {
		mySet->Close();
		delete mySet;
		mySet=NULL;
		}
	}

void CVidsendDoc2PropPage2::OnCheck4() {
	updateDaCheck();
	}

void CVidsendDoc2PropPage2::OnCheck9() {
	updateDaCheck();
	}

void CVidsendDoc2PropPage2::OnRadio1() {
	updateDaCheck();
	}

void CVidsendDoc2PropPage2::OnRadio2() {
	updateDaCheck();
	}

void CVidsendDoc2PropPage2::OnRadio13() {
	updateDaCheck();
	}

void CVidsendDoc2PropPage2::updateDaCheck() {
	int i,n;
	CString S;
	char myBuf[128];
	
	UpdateData();
	if(m_TipoAutorizzazione==2) {
		CWebSrvSocket2_base::getMyOutmostIPAddress(myBuf);
		m_AuthWWW=myBuf;
		mySet=new CVidsendSet2_(NULL);
		if(mySet) {
			if(!(mySet->Open(AFX_DB_USE_DEFAULT_TYPE,NULL,CRecordset::none))) {
				goto no_set;
				}
			mySet->m_strSort="nome asc";
			mySet->Requery();
			n=0;
			while(!mySet->IsEOF()) {
				S=mySet->m_Nome;
				if((i=m_Lista.AddString(S)) != LB_ERR)
					m_Lista.SetItemData(i,mySet->m_ID);
				mySet->MoveNext();
				n++;
				}
			}
		else {
no_set:
		if(mySet) {
			delete mySet;
			mySet=NULL;
			}
			AfxMessageBox("Impossibile aprire database utenti");
			}
		}
	else {
		m_Lista.ResetContent();
		if(mySet) {
			mySet->Close();
			delete mySet;
			mySet=NULL;
			}
		}
	GetDlgItem(IDC_EDIT6)->EnableWindow(m_TipoAutorizzazione==1);
	GetDlgItem(IDC_LIST1)->EnableWindow(m_TipoAutorizzazione==2);
	GetDlgItem(IDC_BUTTON1)->EnableWindow(m_TipoAutorizzazione==2);
	GetDlgItem(IDC_BUTTON2)->EnableWindow(m_TipoAutorizzazione==2 && m_Lista.GetCaretIndex() >= 0);
	GetDlgItem(IDC_BUTTON3)->EnableWindow(m_TipoAutorizzazione==2 && m_Lista.GetCaretIndex() >= 0);
	GetDlgItem(IDC_EDIT2)->EnableWindow(m_bDirectoryServer);
	GetDlgItem(IDC_EDIT7)->EnableWindow(m_bDirectoryServer);
	GetDlgItem(IDC_CHECK9)->EnableWindow(m_bDirectoryServer);
	if(!m_bDirectoryServer)
		m_bNeedAuthenticate=0;
	UpdateData(FALSE);
	}

void CVidsendDoc2PropPage2::OnSelchangeList1() {
	char myBuf[256];
	int i;

  if((i=m_Lista.GetCaretIndex()) >= 0) {
		GetDlgItem(IDC_BUTTON2)->EnableWindow(TRUE);
		GetDlgItem(IDC_BUTTON3)->EnableWindow(TRUE);
		i=m_Lista.GetItemData(i);
		mySet->goTo(i);
		}
	else {
		GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);
		}
	UpdateData(FALSE);
  }

void CVidsendDoc2PropPage2::OnInsert() {
	CConf2Page2Utenti myDlg;
	CString S;
	int i;

	if(myDlg.DoModal() == IDOK) {
		mySet->AddNew();
		mySet->m_Nome=myDlg.m_User;
		mySet->m_Password=myDlg.m_Pasw;
		mySet->m_WelcomeMsg=myDlg.m_WelcomeMsg;
		mySet->m_TimedConn=myDlg.m_TimedConn.GetHour()*60+myDlg.m_TimedConn.GetMinute();
		mySet->m_bTimedConn=myDlg.m_bTimedConn;
		mySet->m_Abilitato=myDlg.m_Active;
//		mySet-> =m_bPayPerView;
		mySet->Update();
		S=mySet->m_Nome;
		if((i=m_Lista.AddString(S)) != LB_ERR)
			m_Lista.SetItemData(i,mySet->m_ID);
		}
	}

void CVidsendDoc2PropPage2::OnEdit() {
	CConf2Page2Utenti myDlg(this,mySet);
	int i;
	char myBuf[256];
	CString S;

	if(myDlg.DoModal() == IDOK) {
		mySet->Edit();
		mySet->m_Nome=myDlg.m_User;
		mySet->m_Password=myDlg.m_Pasw;
		mySet->m_WelcomeMsg=myDlg.m_WelcomeMsg;
		mySet->m_TimedConn=myDlg.m_TimedConn.GetHour()*60+myDlg.m_TimedConn.GetMinute();
		mySet->m_bTimedConn=myDlg.m_bTimedConn;
		mySet->m_Abilitato=myDlg.m_Active;
//		mySet-> =m_bPayPerView;
		mySet->Update();
		S=mySet->m_Nome;
		i=m_Lista.GetCaretIndex();
		m_Lista.DeleteString(i);
		if((i=m_Lista.InsertString(i,S)) != LB_ERR)
			m_Lista.SetItemData(i,mySet->m_ID);
		}
	}

void CVidsendDoc2PropPage2::OnDelete() {
	
	if(AfxMessageBox("Cancellare questo utente?",MB_YESNO | MB_DEFBUTTON2) == IDYES) {
		mySet->Delete();
		m_Lista.DeleteString(m_Lista.GetCurSel());
		}
	}

void CVidsendDoc2PropPage2::OnDblclkList1() {
	OnEdit();
	}



/////////////////////////////////////////////////////////////////////////////
// CConf2Page2Utenti dialog


CConf2Page2Utenti::CConf2Page2Utenti(CWnd* pParent /*=NULL*/,CVidsendSet2_ *mySet)
	: CDialog(CConf2Page2Utenti::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConf2Page2Utenti)
	m_TimedConn = 0;
	m_bTimedConn = FALSE;
	m_User = _T("");
	m_Pasw = _T("");
	m_WelcomeMsg = _T("");
	m_bPayPerView = FALSE;
	m_Active = FALSE;
	m_RemoteControl = FALSE;
	//}}AFX_DATA_INIT
	if(mySet) {
		m_User=mySet->m_Nome;
		m_Pasw=mySet->m_Password;
		m_WelcomeMsg=mySet->m_WelcomeMsg;
		m_TimedConn=mySet->m_TimedConn;
		m_bTimedConn=mySet->m_bTimedConn;
		m_Active=mySet->m_Abilitato;
//		m_bPayPerView=mySet-> ;
		}
}


void CConf2Page2Utenti::DoDataExchange(CDataExchange* pDX) {

	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConf2Page2Utenti)
	DDX_DateTimeCtrl(pDX, IDC_DATETIMEPICKER1, m_TimedConn);
	DDX_Check(pDX, IDC_CHECK2, m_bTimedConn);
	DDX_Text(pDX, IDC_EDIT7, m_User);
	DDV_MaxChars(pDX, m_User, 31);
	DDX_Text(pDX, IDC_EDIT8, m_Pasw);
	DDV_MaxChars(pDX, m_Pasw, 31);
	DDX_Text(pDX, IDC_EDIT9, m_WelcomeMsg);
	DDV_MaxChars(pDX, m_WelcomeMsg, 255);
	DDX_Check(pDX, IDC_CHECK9, m_bPayPerView);
	DDX_Check(pDX, IDC_CHECK5, m_Active);
	DDX_Check(pDX, IDC_CHECK12, m_RemoteControl);
	//}}AFX_DATA_MAP
	}


BEGIN_MESSAGE_MAP(CConf2Page2Utenti, CDialog)
	//{{AFX_MSG_MAP(CConf2Page2Utenti)
	ON_BN_CLICKED(IDC_CHECK2, OnCheck2)
	ON_BN_CLICKED(IDC_CHECK9, OnCheck9)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConf2Page2Utenti message handlers


BOOL CConf2Page2Utenti::OnInitDialog() {

	CDialog::OnInitDialog();
	
	((CDateTimeCtrl *)GetDlgItem(IDC_DATETIMEPICKER1))->SetFormat("hh:mm");
	UpdateData(FALSE);
	updateDaCheck();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	}


void CConf2Page2Utenti::OnCheck2() {
	
	updateDaCheck();
	}

void CConf2Page2Utenti::OnCheck9() {
	
	updateDaCheck();
	}

void CConf2Page2Utenti::updateDaCheck() {
	
	UpdateData();
	GetDlgItem(IDC_DATETIMEPICKER1)->EnableWindow(m_bTimedConn);
	}


#ifdef _NEWMEET_MODE

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage2_NM dialog

IMPLEMENT_DYNCREATE(CVidsendDoc2PropPage2_NM, CDialog)

CVidsendDoc2PropPage2_NM::CVidsendDoc2PropPage2_NM(CVidsendDoc2 *p) : CDialog(CVidsendDoc2PropPage2_NM::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc2PropPage2_NM)
	m_SuonoIn = _T("");
	m_SuonoOut = _T("");
	m_ActivateIf = FALSE;
	m_ActivateWaitConfirm=0;
	m_DialUp=FALSE;
	m_MaxConn = 0;
	//}}AFX_DATA_INIT
	myParent=p;
}

CVidsendDoc2PropPage2_NM::~CVidsendDoc2PropPage2_NM()
{
}

void CVidsendDoc2PropPage2_NM::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc2PropPage2_NM)
	DDX_CBString(pDX, IDC_COMBO3, m_SuonoIn);
	DDV_MaxChars(pDX, m_SuonoIn, 127);
	DDX_CBString(pDX, IDC_COMBO9, m_SuonoOut);
	DDV_MaxChars(pDX, m_SuonoOut, 127);
	DDX_CBString(pDX, IDC_COMBO4, m_DialUpNome);
	DDV_MaxChars(pDX, m_DialUpNome, 127);
	DDX_Check(pDX, IDC_CHECK6, m_ActivateIf);
	DDX_Check(pDX, IDC_CHECK10, m_ActivateWaitConfirm);
	DDX_Check(pDX, IDC_CHECK11, m_DialUp);
	DDX_Text(pDX, IDC_EDIT1, m_MaxConn);
	DDV_MinMaxUInt(pDX, m_MaxConn, 0, 1000);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc2PropPage2_NM, CDialog)
	//{{AFX_MSG_MAP(CVidsendDoc2PropPage2_NM)
	ON_BN_CLICKED(IDC_CHECK6, OnCheck6)
	ON_BN_CLICKED(IDC_CHECK10, OnCheck10)
	ON_BN_CLICKED(IDC_CHECK11, OnCheck11)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage2_NM message handlers

BOOL CVidsendDoc2PropPage2_NM::OnInitDialog() {
	RASENTRYNAME ren[20];
	int i;
	DWORD n,n1;

	CDialog::OnInitDialog();

	m_MaxConn=myParent->maxConn;
	m_ActivateIf=myParent->Opzioni & CVidsendDoc2::openVideoOnConnect ? 1 : 0;
	m_ActivateWaitConfirm=myParent->Opzioni & CVidsendDoc2::askOnConnect ? 1 : 0;
	m_DialUp=myParent->Opzioni & CVidsendDoc2::doDialUp ? 1 : 0;
	
	((CComboBox *)GetDlgItem(IDC_COMBO3))->Dir(0,"*.wav");
	m_SuonoIn=myParent->suonoIn;
	((CComboBox *)GetDlgItem(IDC_COMBO9))->Dir(0,"*.wav");
	m_SuonoOut=myParent->suonoOut;

	n1=sizeof(ren);
	ren[0].dwSize=sizeof(RASENTRYNAME);
	i=RasEnumEntries(NULL,NULL,ren,&n1,&n);
	if(!i) {
		for(i=0; i<n; i++) {
			((CComboBox *)GetDlgItem(IDC_COMBO4))->AddString(ren[i].szEntryName);
			}
		m_DialUpNome=theApp.DialUpNome;
#ifdef _CAMPARTY_MODE
		m_DialUpNome="camparty";
#endif
		}
	else
		AfxMessageBox("Impossibile leggere elenco delle connessioni remote!",MB_ICONSTOP);

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}



void CVidsendDoc2PropPage2_NM::OnCheck6() {

	UpdateData();
	if(m_ActivateIf)
		m_ActivateWaitConfirm=TRUE;
	UpdateData(FALSE);
	}

void CVidsendDoc2PropPage2_NM::OnCheck10() {

	UpdateData();
	if(!m_ActivateWaitConfirm)
		m_ActivateIf=FALSE;
	UpdateData(FALSE);
	}

void CVidsendDoc2PropPage2_NM::OnCheck11() {

	UpdateData();
	GetDlgItem(IDC_COMBO4)->EnableWindow(m_DialUp);
	}


#endif



/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage3 property page

IMPLEMENT_DYNCREATE(CVidsendDoc2PropPage3, CPropertyPage)

CVidsendDoc2PropPage3::CVidsendDoc2PropPage3(CVidsendDoc2 *p) : CPropertyPage(CVidsendDoc2PropPage3::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc2PropPage3)
	m_bOpenWWW = FALSE;
	m_DontSave = FALSE;
	m_bTimedConn = FALSE;
	m_SuonoIn = _T("");
	m_SuonoOut = _T("");
	m_OpenWWW = _T("");
	m_StreamTitle = _T("");
	m_TimedConn = 0;
	m_DialUp = 0;
	m_ActivateIf = 0;
	m_ActivateWaitConfirm=0;
	//}}AFX_DATA_INIT
	isInitialized=FALSE;
	myParent=p;
	}

CVidsendDoc2PropPage3::~CVidsendDoc2PropPage3()
{
}

void CVidsendDoc2PropPage3::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc2PropPage3)
	DDX_Check(pDX, IDC_CHECK3, m_bOpenWWW);
	DDX_Check(pDX, IDC_CHECK4, m_DontSave);
	DDX_Check(pDX, IDC_CHECK7, m_bTimedConn);
	DDX_CBString(pDX, IDC_COMBO3, m_SuonoIn);
	DDV_MaxChars(pDX, m_SuonoIn, 127);
	DDX_CBString(pDX, IDC_COMBO9, m_SuonoOut);
	DDV_MaxChars(pDX, m_SuonoOut, 127);
	DDX_CBString(pDX, IDC_COMBO4, m_DialUpNome);
	DDV_MaxChars(pDX, m_DialUpNome, 127);
	DDX_Text(pDX, IDC_EDIT2, m_OpenWWW);
	DDV_MaxChars(pDX, m_OpenWWW, 127);
	DDX_DateTimeCtrl(pDX, IDC_DATETIMEPICKER2, m_TimedConn);
	DDX_Text(pDX, IDC_EDIT9, m_StreamTitle);
	DDV_MaxChars(pDX, m_StreamTitle, 127);
	DDX_Check(pDX, IDC_CHECK6, m_ActivateIf);
	DDX_Check(pDX, IDC_CHECK10, m_ActivateWaitConfirm);
	DDX_Check(pDX, IDC_CHECK11, m_DialUp);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc2PropPage3, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc2PropPage3)
	ON_BN_CLICKED(IDC_CHECK6, OnCheck6)
	ON_BN_CLICKED(IDC_CHECK11, OnCheck11)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage3 message handlers

BOOL CVidsendDoc2PropPage3::OnInitDialog() {
	RASENTRYNAME ren[20];
	int i;
	DWORD n,n1;

	CPropertyPage::OnInitDialog();
	
	m_OpenWWW=myParent->forceOpenWWW;
	m_bOpenWWW=myParent->Opzioni & CVidsendDoc2::openWWW ? 1 : 0;
	m_DontSave=myParent->Opzioni & CVidsendDoc2::dontSave ? 1 : 0;
	m_ActivateIf=myParent->Opzioni & CVidsendDoc2::openVideoOnConnect ? 1 : 0;
	m_ActivateWaitConfirm=myParent->Opzioni & CVidsendDoc2::askOnConnect ? 1 : 0;
	m_bTimedConn=myParent->Opzioni & CVidsendDoc2::timedConnection ? 1 : 0;
	((CDateTimeCtrl *)GetDlgItem(IDC_DATETIMEPICKER2))->SetFormat("hh:mm");
	{ CTime myT(2000,1,1,myParent->timedConnLenght.GetTotalHours(),myParent->timedConnLenght.GetMinutes(),0);
		m_TimedConn=myT;
	}
	m_DialUp=myParent->Opzioni & CVidsendDoc2::doDialUp ? 1 : 0;
	
	((CComboBox *)GetDlgItem(IDC_COMBO3))->Dir(0,"*.wav");
	m_SuonoIn=myParent->suonoIn;
	((CComboBox *)GetDlgItem(IDC_COMBO9))->Dir(0,"*.wav");
	m_SuonoOut=myParent->suonoOut;
	m_StreamTitle=myParent->streamTitle;

	n1=sizeof(ren);
	ren[0].dwSize=sizeof(RASENTRYNAME);
	i=RasEnumEntries(NULL,NULL,ren,&n1,&n);
	if(!i) {
		for(i=0; i<n; i++) {
			((CComboBox *)GetDlgItem(IDC_COMBO4))->AddString(ren[i].szEntryName);
			}
		m_DialUpNome=theApp.DialUpNome;
#ifdef _CAMPARTY_MODE
		m_DialUpNome="camparty";
#endif
		}
	else
		AfxMessageBox("Impossibile leggere elenco delle connessioni remote!",MB_ICONSTOP);

	UpdateData(FALSE);
	updateDaCheck();

	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

void CVidsendDoc2PropPage3::updateDaCheck() {
	
	UpdateData();
	GetDlgItem(IDC_EDIT2)->EnableWindow(m_bOpenWWW);
	GetDlgItem(IDC_DATETIMEPICKER2)->EnableWindow(m_bTimedConn);
	GetDlgItem(IDC_COMBO4)->EnableWindow(m_DialUp);
	}

void CVidsendDoc2PropPage3::OnCheck6() {

	UpdateData();
	if(m_ActivateIf)
		m_ActivateWaitConfirm=TRUE;
	UpdateData(FALSE);
	}

void CVidsendDoc2PropPage3::OnCheck11() {

	updateDaCheck();
	}



/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage0 property page

IMPLEMENT_DYNCREATE(CVidsendPropPage0, CPropertyPage)

CVidsendPropPage0::CVidsendPropPage0() : CPropertyPage(CVidsendPropPage0::IDD)
{
	//{{AFX_DATA_INIT(CVidsendPropPage0)
	m_ServerWWW = FALSE;
	m_ServerOra = FALSE;
	m_IPAddress = 0;
	m_ServerAuth = FALSE;
	m_ServerDir = FALSE;
	m_RiconnettiV = FALSE;
	m_RiconnettiC = FALSE;
	m_Spool = FALSE;
	m_DDEenable = FALSE;
	m_ServerWWWPort = 0;
	m_RiapriV = FALSE;
	m_RiapriC = FALSE;
	m_TCP_UDP=0;
	m_NamedPipes=0;
	//}}AFX_DATA_INIT
	isInitialized=FALSE;
	}

CVidsendPropPage0::~CVidsendPropPage0()
{
}

void CVidsendPropPage0::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendPropPage0)
	DDX_Check(pDX, IDC_CHECK6, m_ServerWWW);
	DDX_Check(pDX, IDC_CHECK7, m_ServerOra);
	DDX_CBIndex(pDX, IDC_COMBO12, m_IPAddress);
	DDX_Check(pDX, IDC_CHECK10, m_ServerAuth);
	DDX_Check(pDX, IDC_CHECK11, m_ServerDir);
	DDX_Check(pDX, IDC_CHECK1, m_RiconnettiV);
	DDX_Check(pDX, IDC_CHECK2, m_RiconnettiC);
	DDX_Check(pDX, IDC_CHECK3, m_Spool);
	DDX_Check(pDX, IDC_CHECK13, m_DDEenable);
	DDX_Text(pDX, IDC_EDIT1, m_ServerWWWPort);
	DDV_MinMaxUInt(pDX, m_ServerWWWPort, 0, 65534);
	DDX_Check(pDX, IDC_CHECK4, m_RiapriV);
	DDX_Check(pDX, IDC_CHECK9, m_RiapriC);
	DDX_Radio(pDX, IDC_RADIO1, m_TCP_UDP);
	DDX_Check(pDX, IDC_CHECK8, m_NamedPipes);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendPropPage0, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendPropPage0)
	ON_BN_CLICKED(IDC_CHECK6, OnCheck6)
	ON_CBN_SELCHANGE(IDC_COMBO12, OnSelchangeCombo12)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage0 message handlers

BOOL CVidsendPropPage0::OnInitDialog() {
	char myBuf[128];
	DWORD n;
	int i;
	CString S;

	CPropertyPage::OnInitDialog();

	if(!CWebSrvSocket2_base::getMyIPAddress(myBuf))
		((CComboBox *)GetDlgItem(IDC_COMBO12))->AddString("<non disponibile>");
	else {
		for(i=0; i<=9; i++) {
		if(CWebSrvSocket2_base::getMyIPAddress(myBuf,i)) {
			S=myBuf;
			((CComboBox *)GetDlgItem(IDC_COMBO12))->AddString(S);
			}
		else
			break;
			}
		}
	
	m_IPAddress=0;

	m_ServerWWW=theApp.Opzioni & CVidsendApp::webServer ? 1 : 0;
	m_ServerWWWPort=theApp.serverWWWPort;
	if(m_ServerWWWPort == 0)
		m_ServerWWWPort=8080;
	m_ServerOra=theApp.Opzioni & CVidsendApp::timeServer ? 1 : 0;
	m_ServerAuth=theApp.Opzioni & CVidsendApp::authServer ? 1 : 0;
	m_ServerDir=theApp.Opzioni & CVidsendApp::dirServer ? 1 : 0;
	m_RiconnettiV=theApp.Opzioni & CVidsendApp::openClientVideo ? 1 : 0;
	m_RiconnettiC=theApp.Opzioni & CVidsendApp::openClientChat ? 1 : 0;
	m_RiapriV=theApp.Opzioni & CVidsendApp::openServerVideo ? 1 : 0;
	m_RiapriC=theApp.Opzioni & CVidsendApp::openServerChat ? 1 : 0;
	m_DDEenable=theApp.Opzioni & CVidsendApp::DDEenabled ? 1 : 0;
	m_TCP_UDP=theApp.Opzioni & CVidsendApp::TCP_UDP ? 1 : 0;
	m_NamedPipes=theApp.Opzioni & CVidsendApp::namedPipes ? 1 : 0;
	m_Spool=theApp.FileSpool ? 1 : 0;

	UpdateData(FALSE);
	updateDaCheck();

	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}


void CVidsendPropPage0::updateDaCheck() {
	
	UpdateData();
	GetDlgItem(IDC_EDIT1)->EnableWindow(m_ServerWWW);
	}

void CVidsendPropPage0::OnCheck6() {
  updateDaCheck();	
	}

void CVidsendPropPage0::OnSelchangeCombo12() {

	((CComboBox *)GetDlgItem(IDC_COMBO12))->GetLBText(m_IPAddress,m_IPAddressText);
	}


/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage1 property page

IMPLEMENT_DYNCREATE(CVidsendPropPage1, CPropertyPage)

CVidsendPropPage1::CVidsendPropPage1() : CPropertyPage(CVidsendPropPage1::IDD)
{
	//{{AFX_DATA_INIT(CVidsendPropPage1)
	m_Nome = _T("");
	m_Cognome = _T("");
	m_Citta = _T("");
	m_Note = _T("");
	m_Indirizzo = _T("");
	m_Email = _T("");
	m_SuonoFine = _T("");
	m_SuonoInizio = _T("");
	m_SaveLayout = FALSE;
	m_PasswordProtect = FALSE;
	//}}AFX_DATA_INIT
	isInitialized=FALSE;
	}

CVidsendPropPage1::~CVidsendPropPage1()
{
}

void CVidsendPropPage1::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendPropPage1)
	DDX_Text(pDX, IDC_EDIT9, m_Nome);
	DDV_MaxChars(pDX, m_Nome, 31);
	DDX_Text(pDX, IDC_EDIT8, m_Cognome);
	DDV_MaxChars(pDX, m_Cognome, 31);
	DDX_Text(pDX, IDC_EDIT6, m_Citta);
	DDV_MaxChars(pDX, m_Citta, 31);
	DDX_Text(pDX, IDC_EDIT1, m_Note);
	DDV_MaxChars(pDX, m_Note, 255);
	DDX_Text(pDX, IDC_EDIT7, m_Indirizzo);
	DDV_MaxChars(pDX, m_Indirizzo, 80);
	DDX_Text(pDX, IDC_EDIT2, m_Email);
	DDV_MaxChars(pDX, m_Email, 31);
	DDX_CBString(pDX, IDC_COMBO6, m_SuonoFine);
	DDV_MaxChars(pDX, m_SuonoFine, 127);
	DDX_CBString(pDX, IDC_COMBO1, m_SuonoInizio);
	DDV_MaxChars(pDX, m_SuonoInizio, 127);
	DDX_Check(pDX, IDC_CHECK1, m_SaveLayout);
	DDX_Check(pDX, IDC_CHECK2, m_PasswordProtect);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendPropPage1, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendPropPage1)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage1 message handlers

BOOL CVidsendPropPage1::OnInitDialog() {
	
	CPropertyPage::OnInitDialog();
	
	m_Nome=theApp.infoUtente.nome;
	m_Cognome=theApp.infoUtente.cognome;
	m_Indirizzo=theApp.infoUtente.indirizzo;
	m_Citta=theApp.infoUtente.citta;
	m_Email=theApp.infoUtente.email;
	m_Note=theApp.infoUtente.note;
	m_SaveLayout=theApp.Opzioni & CVidsendApp::saveLayout ? 1 : 0;
	m_PasswordProtect=theApp.Opzioni & CVidsendApp::passwordProtect ? 1 : 0;
	((CComboBox *)GetDlgItem(IDC_COMBO1))->Dir(0,"*.wav");
	m_SuonoInizio=theApp.suonoInizio;
	((CComboBox *)GetDlgItem(IDC_COMBO6))->Dir(0,"*.wav");
	m_SuonoFine=theApp.suonoFine;
	UpdateData(FALSE);

	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}


/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage2 dialog


IMPLEMENT_DYNCREATE(CVidsendPropPage2, CPropertyPage)
CVidsendPropPage2::CVidsendPropPage2() : CPropertyPage(CVidsendPropPage2::IDD)
{
	//{{AFX_DATA_INIT(CVidsendPropPage2)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


CVidsendPropPage2::~CVidsendPropPage2()
{
}

void CVidsendPropPage2::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendPropPage2)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendPropPage2, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendPropPage2)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage2 message handlers



/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc3PropPage0 property page

IMPLEMENT_DYNCREATE(CVidsendDoc3PropPage0, CPropertyPage)

CVidsendDoc3PropPage0::CVidsendDoc3PropPage0(CVidsendDoc3 *p) : CPropertyPage(CVidsendDoc3PropPage0::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc3PropPage0)
	m_DSN = _T("");
	m_LogAttivo = FALSE;
	//}}AFX_DATA_INIT
	isInitialized=FALSE;
	myParent=p;
	}

CVidsendDoc3PropPage0::~CVidsendDoc3PropPage0()
{
}

void CVidsendDoc3PropPage0::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc3PropPage0)
	DDX_Text(pDX, IDC_EDIT1, m_DSN);
	DDX_Check(pDX, IDC_CHECK1, m_LogAttivo);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc3PropPage0, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc3PropPage0)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc3PropPage0 message handlers

BOOL CVidsendDoc3PropPage0::OnInitDialog() {
	CPropertyPage::OnInitDialog();
	
	m_DSN=myParent->m_vidsendSet->GetDefaultConnect();
	m_LogAttivo=theApp.OpzioniLog & CVidsendDoc3::logAttivo ? 1 : 0;
	UpdateData(FALSE);
	
	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage0 property page

IMPLEMENT_DYNCREATE(CVidsendDoc4PropPage0, CPropertyPage)

CVidsendDoc4PropPage0::CVidsendDoc4PropPage0(CVidsendDoc4 *p) : CPropertyPage(CVidsendDoc4PropPage0::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc4PropPage0)
	m_MaxConn = 0;
	m_AncheWWW = FALSE;
	m_One2One=FALSE;
	m_noOne2One=FALSE;
	//}}AFX_DATA_INIT
	myParent=p;
	isInitialized=FALSE;
	}

CVidsendDoc4PropPage0::~CVidsendDoc4PropPage0()
{
}

void CVidsendDoc4PropPage0::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc4PropPage0)
	DDX_Control(pDX, IDC_SPIN1, m_MaxconnSpin);
	DDX_Text(pDX, IDC_EDIT1, m_MaxConn);
	DDV_MinMaxUInt(pDX, m_MaxConn, 0, 1000);
	DDX_Check(pDX, IDC_CHECK1, m_AncheWWW);
	DDX_Check(pDX, IDC_CHECK15, m_One2One);
	DDX_Check(pDX, IDC_CHECK16, m_noOne2One);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc4PropPage0, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc4PropPage0)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage0 message handlers

BOOL CVidsendDoc4PropPage0::OnInitDialog() {

	CPropertyPage::OnInitDialog();
	
	if(theApp.theChat) {
		m_MaxConn=myParent->maxConn;
		GetDlgItem(IDC_EDIT1)->EnableWindow(TRUE);
		GetDlgItem(IDC_SPIN1)->EnableWindow(TRUE);
		}
	m_MaxconnSpin.SetRange(0,1000);
	m_AncheWWW=myParent->Opzioni & CVidsendDoc4::ancheAccessoWeb ? 1 : 0;
	m_One2One=myParent->Opzioni & CVidsendDoc4::onlyOne2One ? 1 : 0;
	m_noOne2One=myParent->Opzioni & CVidsendDoc4::noOne2One ? 1 : 0;;
	UpdateData(FALSE);
	updateDaCheck();
	
	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

void CVidsendDoc4PropPage0::updateDaCheck() {
	
	UpdateData();
	}



/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage1 property page

IMPLEMENT_DYNCREATE(CVidsendDoc4PropPage1, CPropertyPage)

CVidsendDoc4PropPage1::CVidsendDoc4PropPage1(CStringList *bl,CVidsendDoc4 *p) : CPropertyPage(CVidsendDoc4PropPage1::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc4PropPage1)
	m_Attivo = FALSE;
	m_bLavagna = FALSE;
	m_DontSave = FALSE;
	m_NoPrivate = FALSE;
	m_OpenWWW = _T("");
	m_bOpenWWW = FALSE;
	m_bAuthWWW = FALSE;
	m_AuthWWW = _T("");
	m_bTimedConn = FALSE;
	m_TimedConn = 0;
	m_Mostra_E_U = FALSE;
	m_bUsaSuoni=0;
	m_bUsaColori=0;
	m_bUsaIcone=0;
	//}}AFX_DATA_INIT
	myParent=p;
	eData=0;
	blackList=bl;
	isInitialized=FALSE;
	}

CVidsendDoc4PropPage1::~CVidsendDoc4PropPage1()
{
}

void CVidsendDoc4PropPage1::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc4PropPage1)
	DDX_Text(pDX, IDC_IPADDRESS4, m_IP);
	DDV_MaxChars(pDX, m_IP, 16);
	DDX_Check(pDX, IDC_CHECK13, m_Attivo);
	DDX_Check(pDX, IDC_CHECK1, m_bLavagna);
	DDX_Check(pDX, IDC_CHECK4, m_DontSave);
	DDX_Check(pDX, IDC_CHECK9, m_NoPrivate);
	DDX_Text(pDX, IDC_EDIT2, m_OpenWWW);
	DDV_MaxChars(pDX, m_OpenWWW, 127);
	DDX_Check(pDX, IDC_CHECK6, m_bOpenWWW);
	DDX_Check(pDX, IDC_CHECK3, m_bAuthWWW);
	DDX_Text(pDX, IDC_EDIT6, m_AuthWWW);
	DDV_MaxChars(pDX, m_AuthWWW, 127);
	DDX_Check(pDX, IDC_CHECK7, m_bTimedConn);
	DDX_DateTimeCtrl(pDX, IDC_DATETIMEPICKER2, m_TimedConn);
	DDX_Check(pDX, IDC_CHECK10, m_Mostra_E_U);
	DDX_Check(pDX, IDC_CHECK17, m_bUsaSuoni);
	DDX_Check(pDX, IDC_CHECK18, m_bUsaColori);
	DDX_Check(pDX, IDC_CHECK19, m_bUsaIcone);
	DDX_Control(pDX, IDC_LIST1, m_Lista);
	//}}AFX_DATA_MAP
	}


BEGIN_MESSAGE_MAP(CVidsendDoc4PropPage1, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc4PropPage1)
	ON_BN_CLICKED(IDC_CHECK13, OnCheck13)
	ON_BN_CLICKED(IDC_CHECK3, OnCheck3)
	ON_BN_CLICKED(IDC_CHECK7, OnCheck7)
	ON_BN_CLICKED(IDC_CHECK6, OnCheck6)
	ON_LBN_SELCHANGE(IDC_LIST1, OnSelchangeList1)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_BN_CLICKED(IDC_BUTTON2, OnButton2)
	ON_BN_CLICKED(IDC_BUTTON3, OnButton3)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage1 message handlers

BOOL CVidsendDoc4PropPage1::OnInitDialog() {
	int i;

	CPropertyPage::OnInitDialog();

	m_Attivo=myParent->Opzioni & CVidsendDoc4::serverMode ? 1 : 0;
	m_bOpenWWW=myParent->Opzioni & CVidsendDoc4::openWWW ? 1 : 0;
	m_bAuthWWW=myParent->Opzioni & CVidsendDoc4::needAuthenticate ? 1 : 0;
	m_DontSave=myParent->Opzioni & CVidsendDoc4::dontSave ? 1 : 0;
	m_bTimedConn=myParent->Opzioni & CVidsendDoc4::timedConnection ? 1 : 0;
	m_OpenWWW=myParent->forceOpenWWW;
	m_AuthWWW=myParent->authenticationWWW;
	m_Mostra_E_U=myParent->Opzioni & CVidsendDoc4::mostraEU ? 1 : 0;
	m_bUsaSuoni=myParent->Opzioni & CVidsendDoc4::usaSounds  ? 1 : 0;
	m_bUsaColori=myParent->Opzioni & CVidsendDoc4::usaColors ? 1 : 0;
	m_bUsaIcone=myParent->Opzioni & CVidsendDoc4::usaIcons ? 1 : 0;
	m_NoPrivate=myParent->Opzioni & CVidsendDoc4::noPrivateMsg ? 1 : 0;
	((CDateTimeCtrl *)GetDlgItem(IDC_DATETIMEPICKER2))->SetFormat("hh:mm");
	{ CTime myT(2000,1,1,myParent->timedConnLenght.GetHours(),myParent->timedConnLenght.GetMinutes(),0);
		m_TimedConn=myT;
	}

	myParent->loadBlacklistedIP(blackList);
	i=0;
	POSITION po=blackList->GetHeadPosition();
	while(po) {
		m_Lista.AddString(blackList->GetAt(po));
		i++;
		blackList->GetNext(po);
		}

	UpdateData(FALSE);
	updateDaCheck();
	
	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

void CVidsendDoc4PropPage1::OnCheck3() {
	updateDaCheck();	
	}

void CVidsendDoc4PropPage1::OnCheck13() {
	updateDaCheck();
	}

void CVidsendDoc4PropPage1::OnCheck7() {
	updateDaCheck();
	}

void CVidsendDoc4PropPage1::OnCheck6() {
	updateDaCheck();
	}

void CVidsendDoc4PropPage1::OnSelchangeList1() {
	int n;
	BYTE a1=0,a2=0,a3=0,a4=0;
	CString S;

	n=m_Lista.GetCaretIndex();
	if(n != LB_ERR) {
		m_Lista.GetText(n,S);
		m_IP=S;
		GetDlgItem(IDC_BUTTON2)->EnableWindow(TRUE);
		GetDlgItem(IDC_BUTTON3)->EnableWindow(TRUE);
		}
	else {
		m_IP.Empty();
		GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);
		}
	UpdateData(FALSE);
	}


void CVidsendDoc4PropPage1::OnButton1() {		// inserisci IP/OK
	
	if(!eData) {
		entra_edata(1);
		}
	else {
		esci_edata(1);
		}
	}

void CVidsendDoc4PropPage1::OnButton2() {		// modifica IP/Cancel
	
	if(!eData) {
		entra_edata(2);
		}
	else {
		esci_edata(0);
		}
	}

void CVidsendDoc4PropPage1::OnButton3() {
	int n;
	
	if(!eData) {
		n=m_Lista.GetCaretIndex();
		if(n != LB_ERR) {
			if(AfxMessageBox("Cancellare questa riga?",MB_ICONQUESTION | MB_YESNO) == IDYES) {
				m_Lista.DeleteString(n);
				m_IP.Empty();
				GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);
				GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);
				UpdateData(FALSE);
				}
			}
		}
	}

void CVidsendDoc4PropPage1::updateDaCheck() {
	
	UpdateData();
	GetDlgItem(IDC_CHECK1)->EnableWindow(m_Attivo);
	GetDlgItem(IDC_CHECK3)->EnableWindow(m_Attivo);
	GetDlgItem(IDC_EDIT6)->EnableWindow(m_Attivo && m_bAuthWWW);
	GetDlgItem(IDC_EDIT2)->EnableWindow(m_Attivo && m_bOpenWWW);
	GetDlgItem(IDC_CHECK4)->EnableWindow(m_Attivo);
	GetDlgItem(IDC_CHECK9)->EnableWindow(m_Attivo);
	GetDlgItem(IDC_CHECK6)->EnableWindow(m_Attivo);
	GetDlgItem(IDC_CHECK7)->EnableWindow(m_Attivo);
	GetDlgItem(IDC_CHECK10)->EnableWindow(m_Attivo);
	GetDlgItem(IDC_CHECK17)->EnableWindow(m_Attivo);
	GetDlgItem(IDC_CHECK18)->EnableWindow(m_Attivo);
	GetDlgItem(IDC_CHECK19)->EnableWindow(m_Attivo);
	}

void CVidsendDoc4PropPage1::entra_edata(int m) {

	eData=m;
	GetDlgItem(IDC_LIST1)->EnableWindow(FALSE);
	GetDlgItem(IDC_IPADDRESS4)->EnableWindow(TRUE);
	GetDlgItem(IDC_IPADDRESS4)->SetFocus();
	if(m==1)
		m_IP.Empty();
	GetDlgItem(IDC_BUTTON1)->SetWindowText("OK");
	GetDlgItem(IDC_BUTTON2)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON2)->SetWindowText("Annulla");
	GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);
	UpdateData(FALSE);
	}

void CVidsendDoc4PropPage1::esci_edata(int m) {
	CString S;
	int n;
	BYTE a1,a2,a3,a4;

	UpdateData();
	if(m) {
		switch(eData) {
			case 1:
				m_Lista.AddString(m_IP);
				break;
			case 2:
				n=m_Lista.GetCaretIndex();
				if(n != LB_ERR) {
					m_Lista.DeleteString(n);
					m_Lista.AddString(m_IP);
					}
				break;
			}
		}
	GetDlgItem(IDC_LIST1)->EnableWindow(TRUE);
	GetDlgItem(IDC_LIST1)->SetFocus();
	GetDlgItem(IDC_IPADDRESS4)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON1)->SetWindowText("Inserisci");
	GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON2)->SetWindowText("Modifica");
	GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);
	eData=0;
	}

void CVidsendDoc4PropPage1::OnOK() {
	int i,j;
	char myBuf[128];

	if(eData)
		esci_edata(1);
	else {
		blackList->RemoveAll();
		i=0;
		do {
			j=m_Lista.GetText(i,myBuf);
			if(j != LB_ERR) {
				blackList->AddTail(myBuf);
				}
			i++;
			} while(j != LB_ERR);


		CPropertyPage::OnOK();
		}
	}


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage2 property page

IMPLEMENT_DYNCREATE(CVidsendDoc4PropPage2, CPropertyPage)

CVidsendDoc4PropPage2::CVidsendDoc4PropPage2(CVidsendDoc4 *p) : CPropertyPage(CVidsendDoc4PropPage2::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc4PropPage2)
	m_Authenticate = FALSE;
	m_bProxy = FALSE;
	m_bConnetti = FALSE;
	m_User = _T("");
	m_Pasw = _T("");
	m_AuthWWW = _T("");
	m_bSuoni = FALSE;
	m_bColore = FALSE;
	m_MaxMessaggi = 0;
	//}}AFX_DATA_INIT
	myParent=p;
	isInitialized=FALSE;
	}

CVidsendDoc4PropPage2::~CVidsendDoc4PropPage2()
{
}

void CVidsendDoc4PropPage2::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc4PropPage2)
	DDX_Control(pDX, IDC_SPIN1, m_MaxMessaggiSpin);
	DDX_Control(pDX, IDC_IPADDRESS1, m_Proxy);
	DDX_Control(pDX, IDC_COMBO1, m_ConnettiA);
	DDX_Check(pDX, IDC_CHECK1, m_Authenticate);
	DDX_Check(pDX, IDC_CHECK2, m_bProxy);
	DDX_Check(pDX, IDC_CHECK3, m_bConnetti);
	DDX_Text(pDX, IDC_EDIT1, m_User);
	DDV_MaxChars(pDX, m_User, 31);
	DDX_Text(pDX, IDC_EDIT3, m_Pasw);
	DDV_MaxChars(pDX, m_Pasw, 31);
	DDX_Text(pDX, IDC_EDIT5, m_AuthWWW);
	DDV_MaxChars(pDX, m_AuthWWW, 127);
	DDX_Check(pDX, IDC_CHECK4, m_bSuoni);
	DDX_Check(pDX, IDC_CHECK5, m_bColore);
	DDX_Text(pDX, IDC_EDIT11, m_MaxMessaggi);
	DDV_MinMaxUInt(pDX, m_MaxMessaggi, 1, 1000);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc4PropPage2, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc4PropPage2)
	ON_BN_CLICKED(IDC_CHECK1, OnCheck1)
	ON_BN_CLICKED(IDC_CHECK2, OnCheck2)
	ON_BN_CLICKED(IDC_CHECK3, OnCheck3)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_BN_CLICKED(IDC_CHECK5, OnCheck5)
	ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage2 message handlers

BOOL CVidsendDoc4PropPage2::OnInitDialog() {

	CPropertyPage::OnInitDialog();

	m_User=myParent->loginName;
	if(m_User.IsEmpty()) {
		m_User=theApp.infoUtente.nome;
		m_User+=*theApp.infoUtente.cognome;
		}
	m_bSuoni=myParent->opzioniVisive & CVidsendDoc4::avvisi_sonori ? 1 : 0;
	m_bColore=myParent->opzioniVisive & CVidsendDoc4::testo_colorato ? 1 : 0;
	m_colore=myParent->opzioniVisive & 0xffffff;
	m_MaxMessaggi=myParent->maxMessaggi;
	m_MaxMessaggiSpin.SetRange(1,1000);
	UpdateData(FALSE);
	updateDaCheck();

	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control
	}

HBRUSH CVidsendDoc4PropPage2::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
	HBRUSH hbr = CPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
	
	if(pWnd->GetDlgCtrlID() == IDC_STATIC1)
		pDC->SetTextColor(m_bColore ? m_colore : 0);	
	// TODO: Return a different brush if the default is not desired
	return hbr;
	}

void CVidsendDoc4PropPage2::OnCheck1() {
	updateDaCheck();
	}

void CVidsendDoc4PropPage2::OnCheck2() {
	updateDaCheck();
	}

void CVidsendDoc4PropPage2::OnCheck3() {
	updateDaCheck();
	}

void CVidsendDoc4PropPage2::OnCheck5() {
	updateDaCheck();
	}

void CVidsendDoc4PropPage2::updateDaCheck() {
	
	UpdateData();
	GetDlgItem(IDC_EDIT1)->EnableWindow(m_Authenticate);
	GetDlgItem(IDC_EDIT3)->EnableWindow(m_Authenticate);
	GetDlgItem(IDC_EDIT5)->EnableWindow(m_Authenticate);
	GetDlgItem(IDC_COMBO1)->EnableWindow(m_bConnetti);
	GetDlgItem(IDC_IPADDRESS1)->EnableWindow(m_bProxy);
	GetDlgItem(IDC_BUTTON1)->EnableWindow(m_bColore);
	GetDlgItem(IDC_STATIC1)->Invalidate();
	}

void CVidsendDoc4PropPage2::OnButton1() {
	CColorDialog cd;
	
	cd.m_cc.rgbResult=myParent->opzioniVisive & 0xffffff;
	cd.m_cc.Flags |= CC_RGBINIT;
	if(cd.DoModal() == IDOK) {
		m_colore=cd.GetColor();
		updateDaCheck();
		}
	}


#ifdef _NEWMEET_MODE

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage0_NM dialog

IMPLEMENT_DYNCREATE(CVidsendDoc4PropPage0_NM, CDialog)

CVidsendDoc4PropPage0_NM::CVidsendDoc4PropPage0_NM(CStringList *bl,CVidsendDoc4 *p) : CDialog(CVidsendDoc4PropPage0_NM::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc2PropPage4_NM)
	m_MaxConn = 0;
	m_Mostra_E_U = FALSE;
	m_NoPrivate = FALSE;
	m_bProxy = FALSE;
	m_bSuoni = FALSE;
	m_bColore = FALSE;
	m_MaxMessaggi = 0;
	m_NoFreeChat = FALSE;
	m_One2One=FALSE;
	m_noOne2One=FALSE;
	m_bUsaSuoni=0;
	m_bUsaColori=0;
	m_bUsaIcone=0;
	//}}AFX_DATA_INIT
	myParent=p;
	eData=0;
	blackList=bl;
	}

CVidsendDoc4PropPage0_NM::~CVidsendDoc4PropPage0_NM()
{
}

void CVidsendDoc4PropPage0_NM::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc4PropPage0_NM)
	DDX_Control(pDX, IDC_SPIN1, m_MaxconnSpin);
	DDX_Text(pDX, IDC_EDIT1, m_MaxConn);
	DDV_MinMaxUInt(pDX, m_MaxConn, 0, 1000);
	DDX_Check(pDX, IDC_CHECK4, m_bSuoni);
	DDX_Check(pDX, IDC_CHECK5, m_bColore);
	DDX_Text(pDX, IDC_EDIT11, m_MaxMessaggi);
	DDV_MinMaxUInt(pDX, m_MaxMessaggi, 1, 1000);
	DDX_Control(pDX, IDC_IPADDRESS1, m_Proxy);
	DDX_Check(pDX, IDC_CHECK3, m_bProxy);
	DDX_Check(pDX, IDC_CHECK9, m_NoPrivate);
	DDX_Check(pDX, IDC_CHECK10, m_Mostra_E_U);
	DDX_Check(pDX, IDC_CHECK14, m_NoFreeChat);
	DDX_Check(pDX, IDC_CHECK15, m_One2One);
	DDX_Check(pDX, IDC_CHECK16, m_noOne2One);
	DDX_Check(pDX, IDC_CHECK17, m_bUsaSuoni);
	DDX_Check(pDX, IDC_CHECK18, m_bUsaColori);
	DDX_Check(pDX, IDC_CHECK19, m_bUsaIcone);
	DDX_Control(pDX, IDC_LIST1, m_Lista);
	DDX_Text(pDX, IDC_IPADDRESS4, m_IP);
	DDV_MaxChars(pDX, m_IP, 16);
	//}}AFX_DATA_MAP
	}


BEGIN_MESSAGE_MAP(CVidsendDoc4PropPage0_NM, CDialog)
	//{{AFX_MSG_MAP(CVidsendDoc4PropPage0_NM)
	ON_BN_CLICKED(IDC_BUTTON7, OnButton7)
	ON_BN_CLICKED(IDC_CHECK3, OnCheck3)
	ON_LBN_SELCHANGE(IDC_LIST1, OnSelchangeList1)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_BN_CLICKED(IDC_BUTTON2, OnButton2)
	ON_BN_CLICKED(IDC_BUTTON3, OnButton3)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage0_NM message handlers

BOOL CVidsendDoc4PropPage0_NM::OnInitDialog() {
	int i;

	CDialog::OnInitDialog();

	m_MaxConn=myParent->maxConn;
	m_MaxconnSpin.SetRange(0,1000);
	m_Mostra_E_U=myParent->Opzioni & CVidsendDoc4::mostraEU ? 1 : 0;
	m_NoPrivate=myParent->Opzioni & CVidsendDoc4::noPrivateMsg ? 1 : 0;
	m_NoFreeChat=myParent->Opzioni & CVidsendDoc4::noFreeChat ? 1 : 0;
	m_One2One=myParent->Opzioni & CVidsendDoc4::onlyOne2One ? 1 : 0;
	m_noOne2One=myParent->Opzioni & CVidsendDoc4::noOne2One ? 1 : 0;
	m_bUsaSuoni=myParent->Opzioni & CVidsendDoc4::usaSounds  ? 1 : 0;
	m_bUsaColori=myParent->Opzioni & CVidsendDoc4::usaColors ? 1 : 0;
	m_bUsaIcone=myParent->Opzioni & CVidsendDoc4::usaIcons ? 1 : 0;
	m_bProxy=myParent->Opzioni & CVidsendDoc4::usaProxy ? 1 : 0;
	m_bSuoni=myParent->opzioniVisive & CVidsendDoc4::avvisi_sonori ? 1 : 0;
	m_bColore=myParent->opzioniVisive & CVidsendDoc4::testo_colorato ? 1 : 0;
	m_colore=myParent->opzioniVisive & 0xffffff;
	m_MaxMessaggi=myParent->maxMessaggi;
	
	myParent->loadBlacklistedIP(blackList);
	i=0;
	POSITION po=blackList->GetHeadPosition();
	while(po) {
		m_Lista.AddString(blackList->GetAt(po));
		i++;
		blackList->GetNext(po);
		}

	UpdateData(FALSE);
	updateDaCheck();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

HBRUSH CVidsendDoc4PropPage0_NM::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	
	if(pWnd->GetDlgCtrlID() == IDC_STATIC1)
		pDC->SetTextColor(m_bColore ? m_colore : 0);	
	// TODO: Return a different brush if the default is not desired
	return hbr;
	}

void CVidsendDoc4PropPage0_NM::updateDaCheck() {
	
	UpdateData();
	GetDlgItem(IDC_STATIC1)->Invalidate();
	GetDlgItem(IDC_IPADDRESS1)->EnableWindow(m_bProxy);
	}

void CVidsendDoc4PropPage0_NM::OnCheck3() {
	updateDaCheck();
	}

void CVidsendDoc4PropPage0_NM::OnButton7() {
	CColorDialog cd;
	
	cd.m_cc.rgbResult=myParent->opzioniVisive & 0xffffff;
	cd.m_cc.Flags |= CC_RGBINIT;
	if(cd.DoModal() == IDOK) {
		m_colore=cd.GetColor();
		updateDaCheck();
		}
	}


void CVidsendDoc4PropPage0_NM::OnSelchangeList1() {
	int n;
	BYTE a1=0,a2=0,a3=0,a4=0;
	CString S;

	n=m_Lista.GetCaretIndex();
	if(n != LB_ERR) {
		m_Lista.GetText(n,S);
		m_IP=S;
		GetDlgItem(IDC_BUTTON2)->EnableWindow(TRUE);
		GetDlgItem(IDC_BUTTON3)->EnableWindow(TRUE);
		}
	else {
		m_IP.Empty();
		GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);
		}
	UpdateData(FALSE);
	}


void CVidsendDoc4PropPage0_NM::OnButton1() {		// inserisci IP/OK
	
	if(!eData) {
		entra_edata(1);
		}
	else {
		esci_edata(1);
		}
	}

void CVidsendDoc4PropPage0_NM::OnButton2() {		// modifica IP/Cancel
	
	if(!eData) {
		entra_edata(2);
		}
	else {
		esci_edata(0);
		}
	}

void CVidsendDoc4PropPage0_NM::OnButton3() {
	int n;
	
	if(!eData) {
		n=m_Lista.GetCaretIndex();
		if(n != LB_ERR) {
			if(AfxMessageBox("Cancellare questa riga?",MB_ICONQUESTION | MB_YESNO) == IDYES) {
				m_Lista.DeleteString(n);
				m_IP.Empty();
				GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);
				GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);
				UpdateData(FALSE);
				}
			}
		}
	}

void CVidsendDoc4PropPage0_NM::entra_edata(int m) {

	eData=m;
	GetDlgItem(IDC_LIST1)->EnableWindow(FALSE);
	GetDlgItem(IDC_IPADDRESS4)->EnableWindow(TRUE);
	GetDlgItem(IDC_IPADDRESS4)->SetFocus();
	GetDlgItem(IDC_BUTTON1)->SetWindowText("OK");
	GetDlgItem(IDC_BUTTON2)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON2)->SetWindowText("Annulla");
	GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);

	}

void CVidsendDoc4PropPage0_NM::esci_edata(int m) {
	int n;
	BYTE a1,a2,a3,a4;

	UpdateData();
	if(m) {
		switch(eData) {
			case 1:
				m_Lista.AddString(m_IP);
				break;
			case 2:
				n=m_Lista.GetCaretIndex();
				if(n != LB_ERR) {
					m_Lista.DeleteString(n);
					m_Lista.AddString(m_IP);
					}
				break;
			}
		}
	GetDlgItem(IDC_LIST1)->EnableWindow(TRUE);
	GetDlgItem(IDC_LIST1)->SetFocus();
	GetDlgItem(IDC_IPADDRESS4)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON1)->SetWindowText("Inserisci");
	GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON2)->SetWindowText("Modifica");
	GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);
	eData=0;
	}

void CVidsendDoc4PropPage0_NM::OnOK() {
	int i,j;
	char myBuf[128];

	if(eData)
		esci_edata(1);
	else {
		blackList->RemoveAll();
		i=0;
		do {
			j=m_Lista.GetText(i,myBuf);
			if(j != LB_ERR) {
				blackList->AddTail(myBuf);
				}
			i++;
			} while(j != LB_ERR);


		CDialog::OnOK();
		}
	}



#endif




/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc5PropPage0 property page

IMPLEMENT_DYNCREATE(CVidsendDoc5PropPage0, CPropertyPage)

CVidsendDoc5PropPage0::CVidsendDoc5PropPage0() : CPropertyPage(CVidsendDoc5PropPage0::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc5PropPage0)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	isInitialized=FALSE;
	}

CVidsendDoc5PropPage0::~CVidsendDoc5PropPage0()
{
}

void CVidsendDoc5PropPage0::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc5PropPage0)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc5PropPage0, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc5PropPage0)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc5PropPage0 message handlers


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc6PropPage0 property page

IMPLEMENT_DYNCREATE(CVidsendDoc6PropPage0, CPropertyPage)

CVidsendDoc6PropPage0::CVidsendDoc6PropPage0(CVidsendDoc6 *p) : CPropertyPage(CVidsendDoc6PropPage0::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc6PropPage0)
	m_Maxconn = 0;
	m_MaxHTMLconn = 0;
	m_AncheDirSrv = FALSE;
	m_FiltraIP = FALSE;
	//}}AFX_DATA_INIT
	myParent=p;
	isInitialized=FALSE;
	}

CVidsendDoc6PropPage0::~CVidsendDoc6PropPage0()
{
}

void CVidsendDoc6PropPage0::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc6PropPage0)
	DDX_Control(pDX, IDC_SPIN2, m_MaxHTMLconnSpin);
	DDX_Control(pDX, IDC_SPIN1, m_MaxconnSpin);
	DDX_Text(pDX, IDC_EDIT1, m_Maxconn);
	DDX_Text(pDX, IDC_EDIT3, m_MaxHTMLconn);
	DDX_Check(pDX, IDC_CHECK3, m_AncheDirSrv);
	DDX_Check(pDX, IDC_CHECK1, m_FiltraIP);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendDoc6PropPage0, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc6PropPage0)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc6PropPage0 message handlers

BOOL CVidsendDoc6PropPage0::OnInitDialog() {

	CPropertyPage::OnInitDialog();
	
	if(theApp.theServer) {
		m_Maxconn=theApp.theServer->maxConn;
		GetDlgItem(IDC_EDIT1)->EnableWindow(TRUE);
		GetDlgItem(IDC_SPIN1)->EnableWindow(TRUE);
		}
	m_MaxHTMLconn=theApp.maxHTMLconn;
	m_MaxconnSpin.SetRange(0,100);
	m_MaxHTMLconnSpin.SetRange(0,100);
	m_AncheDirSrv =myParent->Opzioni & CVidsendDoc6::mostraAncheDirSrv ? 1 : 0;
	UpdateData(FALSE);
	
	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control

	}



/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc7PropPage0 property page

IMPLEMENT_DYNCREATE(CVidsendDoc7PropPage0, CPropertyPage)

CVidsendDoc7PropPage0::CVidsendDoc7PropPage0(CVidsendDoc7 *p) : CPropertyPage(CVidsendDoc7PropPage0::IDD)
{
	//{{AFX_DATA_INIT(CVidsendDoc7PropPage0)
	m_bHTMLAccess = FALSE;
	m_Maxconn = 0;
	//}}AFX_DATA_INIT
	myParent=p;
	isInitialized=FALSE;
}

CVidsendDoc7PropPage0::~CVidsendDoc7PropPage0()
{
}

void CVidsendDoc7PropPage0::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendDoc7PropPage0)
	DDX_Control(pDX, IDC_SPIN1, m_MaxconnSpin);
	DDX_Check(pDX, IDC_CHECK1, m_bHTMLAccess);
	DDX_Text(pDX, IDC_EDIT1, m_Maxconn);
	//}}AFX_DATA_MAP
	isInitialized=FALSE;
	}


BEGIN_MESSAGE_MAP(CVidsendDoc7PropPage0, CPropertyPage)
	//{{AFX_MSG_MAP(CVidsendDoc7PropPage0)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc7PropPage0 message handlers

BOOL CVidsendDoc7PropPage0::OnInitDialog() {

	CPropertyPage::OnInitDialog();
	
	m_Maxconn=theApp.maxDirSrvConn;
	GetDlgItem(IDC_EDIT1)->EnableWindow(TRUE);
	GetDlgItem(IDC_SPIN1)->EnableWindow(TRUE);
	m_MaxconnSpin.SetRange(5,100);
	m_bHTMLAccess=theApp.OpzioniDirSrv & CVidsendDoc7::ancheAccessoWeb ? 1 : 0;
	UpdateData(FALSE);
	
	isInitialized=TRUE;
	return TRUE;  // return TRUE unless you set the focus to a control

	}



/////////////////////////////////////////////////////////////////////////////
// CDlgEnterURL dialog


CDlgEnterURL::CDlgEnterURL(BOOL m,CDocument *pParent /*=NULL*/)
	: CDialog(CDlgEnterURL::IDD /*, pParent*/)
{
	//{{AFX_DATA_INIT(CDlgEnterURL)
	m_URLstring = _T("");
	m_One2One=FALSE;
	//}}AFX_DATA_INIT
	Mode=m;
	}


void CDlgEnterURL::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgEnterURL)
	DDX_Control(pDX, IDC_COMBO1, m_URL);
	DDX_CBString(pDX, IDC_COMBO1, m_URLstring);
	DDX_Check(pDX, IDC_CHECK1, m_One2One);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgEnterURL, CDialog)
	//{{AFX_MSG_MAP(CDlgEnterURL)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgEnterURL message handlers

BOOL CDlgEnterURL::OnInitDialog() {
	int i;

	CDialog::OnInitDialog();

	if(Mode)
		GetDlgItem(IDC_CHECK1)->ShowWindow(SW_SHOW);

	for(i=0; i<totStrings; i++) {
		if(!myURLs[i].IsEmpty())
			m_URL.AddString(myURLs[i]);
		}

	if(m_URL.GetCount() >= 1)
		m_URL.GetLBText(0,m_URLstring);
	else
		m_URLstring = _T("192.168.1.2");
	
	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

int CDlgEnterURL::DoModal(CString *S,int tot) {
	int i;

	for(i=0; i<min(tot,totStrings); i++)
		myURLs[i]=S[i];
	
	return CDialog::DoModal();
	}

/////////////////////////////////////////////////////////////////////////////
// CPaginaTestDlg dialog


CPaginaTestDlg::CPaginaTestDlg(CVidsendDoc2 * pParent /*=NULL*/)
	: CDialog(CPaginaTestDlg::IDD/*, pParent*/)
{
	//{{AFX_DATA_INIT(CPaginaTestDlg)
	m_VideoImmagine = -1;
	m_AudioFrequenza = -1;
	m_AudioSweep = FALSE;
	m_AudioIntervallato = FALSE;
	//}}AFX_DATA_INIT
	myParent=pParent;
}


void CPaginaTestDlg::DoDataExchange(CDataExchange* pDX) {
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPaginaTestDlg)
	DDX_CBIndex(pDX, IDC_COMBO1, m_VideoImmagine);
	DDX_CBIndex(pDX, IDC_COMBO7, m_AudioFrequenza);
	DDX_Check(pDX, IDC_CHECK2, m_AudioSweep);
	DDX_Check(pDX, IDC_CHECK1, m_AudioIntervallato);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPaginaTestDlg, CDialog)
	//{{AFX_MSG_MAP(CPaginaTestDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPaginaTestDlg message handlers

BOOL CPaginaTestDlg::OnInitDialog() {
	CDialog::OnInitDialog();
	
	m_VideoImmagine=myParent->pagProva.tipoVideo;
	m_AudioFrequenza=myParent->pagProva.tipoAudio;
	m_AudioIntervallato=myParent->pagProva.audioOpzioni & 1 ? 1 : 0;
	m_AudioSweep=myParent->pagProva.audioOpzioni & 2 ? 1 : 0;
	UpdateData(FALSE);		
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}


#ifndef _NEWMEET_MODE

/////////////////////////////////////////////////////////////////////////////
// CVideoSrcDialog property page

IMPLEMENT_DYNCREATE(CVideoSrcDialog, CDialog)

CVideoSrcDialog::CVideoSrcDialog(CVidsendDoc2 *p) : CDialog(CVideoSrcDialog::IDD)
{
	//{{AFX_DATA_INIT(CVideoSrcDialog)
	m_VideoSource = -1;
	m_AlternaSource = FALSE;
	m_Overlay = -1;
	m_Schede = -1;
	//}}AFX_DATA_INIT
	myParent=p;
	}

CVideoSrcDialog::~CVideoSrcDialog()
{
}

void CVideoSrcDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVideoSrcDialog)
	DDX_Radio(pDX, IDC_RADIO4, m_VideoSource);
	DDX_Check(pDX, IDC_CHECK2, m_AlternaSource);
	DDX_Radio(pDX, IDC_RADIO14, m_Overlay);
	DDX_CBIndex(pDX, IDC_COMBO1, m_Schede);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVideoSrcDialog, CDialog)
	//{{AFX_MSG_MAP(CVideoSrcDialog)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_BN_CLICKED(IDC_BUTTON2, OnButton2)
	ON_BN_CLICKED(IDC_BUTTON3, OnButton3)
	ON_BN_CLICKED(IDC_BUTTON4, OnButton4)
	ON_CBN_SELENDOK(IDC_COMBO1, OnSelendokCombo1)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVideoSrcDialog message handlers

BOOL CVideoSrcDialog::OnInitDialog() {
	char myBuf[128],myBuf2[128];
	CString S,S2;
	int i;

	CDialog::OnInitDialog();
	
	((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("<nessuna>");
	for(i=0; i<=9; i++) {
		if(myParent->theTV->GetDriverDescription(i,myBuf,64,myBuf2,64)) {
			S=myBuf;
			S2=myBuf2;
			((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString(S+" - "+S2);
			}
		else
			break;
		}
	maxSchede=i+1;
	m_Schede=myParent->theTV->theCapture+1;
	m_VideoSource=myParent->videoSource;
	m_AlternaSource=myParent->alternaSource;
	m_Overlay=myParent->OpzioniSorgenteVideo & CVidsendDoc2::useOverlay ? 1 : 0;

	UpdateData(FALSE);
	updateDaCheck();	

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

void CVideoSrcDialog::OnButton1() {
	int i;
	
#ifndef USA_DIRECTX
	if(i=myParent->theTV->inCapture)
		myParent->theTV->Capture(FALSE); 
	capDlgVideoSource(myParent->theTV->GetHwnd()); 
	myParent->theTV->Capture(i); 
#else
	IBaseFilter *theFilter;
  ISpecifyPropertyPages *pSpec;
  CAUUID cauuid;
	HRESULT hr;

	theFilter=myParent->theTV->m_DShow->getCaptureFilter();
  hr = myParent->theTV->m_DShow->getCaptureFilter()->QueryInterface(IID_ISpecifyPropertyPages,
      (void **)&pSpec);
  if(hr == S_OK) {
    hr = pSpec->GetPages(&cauuid);

    hr = OleCreatePropertyFrame(m_hWnd, 30, 30, NULL, 1,
          (IUnknown **)&theFilter, cauuid.cElems,
          (GUID *)cauuid.pElems, 0, 0, NULL);

    CoTaskMemFree(cauuid.pElems);
    pSpec->Release();
	  }
#endif
	}

void CVideoSrcDialog::OnButton2() {
	int i;

#ifndef USA_DIRECTX
	if(i=myParent->theTV->inCapture)
		myParent->theTV->Capture(FALSE); 
	capDlgVideoDisplay(myParent->theTV->GetHwnd()); 
	myParent->theTV->Capture(i); 
#else
                // You can change this pin's output format in these dialogs.
                // If the capture pin is already connected to somebody who's 
                // fussy about the connection type, that may prevent using 
                // this dialog(!) because the filter it's connected to might not
                // allow reconnecting to a new format. (EG: you switch from RGB
                // to some compressed type, and need to pull in a decoder)
                // I need to tear down the graph downstream of the
                // capture filter before bringing up these dialogs.
                // In any case, the graph must be STOPPED when calling them.
//              if(gcap.fWantPreview)
//                StopPreview();  // make sure graph is stopped

                // The capture pin that we are trying to set the format on is connected if
                // one of these variable is set to TRUE. The pin should be disconnected for
                // the dialog to work properly.
//              if(gcap.fCaptureGraphBuilt || gcap.fPreviewGraphBuilt) {
//                DbgLog((LOG_TRACE,1,TEXT("Tear down graph for dialog")));
//                TearDownGraph();    // graph could prevent dialog working
//                }

	IBaseFilter *theFilter;
  ISpecifyPropertyPages *pSpec;
  CAUUID cauuid;
	HRESULT hr;

	theFilter=myParent->theTV->m_DShow->getCaptureFilter();
//	ASSERT(0);
  IAMStreamConfig *pSC;
  hr = myParent->theTV->m_DShow->getCaptureGraphBuilder()->FindInterface(&PIN_CATEGORY_CAPTURE,
				&MEDIATYPE_Interleaved, theFilter,
				IID_IAMStreamConfig, (void **)&pSC);

  if(hr != NOERROR)
    hr = myParent->theTV->m_DShow->getCaptureGraphBuilder()->FindInterface(&PIN_CATEGORY_CAPTURE,
      &MEDIATYPE_Video, theFilter,
      IID_IAMStreamConfig, (void **)&pSC);

  hr = pSC->QueryInterface(IID_ISpecifyPropertyPages,
        (void **)&pSpec);

  if(hr == S_OK) {
    hr = pSpec->GetPages(&cauuid);
    hr = OleCreatePropertyFrame(m_hWnd, 30, 30, NULL, 1,
        (IUnknown **)&pSC, cauuid.cElems,
        (GUID *)cauuid.pElems, 0, 0, NULL);

        // !!! What if changing output formats couldn't reconnect
        // and the graph is broken?  Shouldn't be possible...

		/*
    if(theFilter) {
       AM_MEDIA_TYPE *pmt;
            // get format being used NOW
       hr = theFilter->GetFormat(&pmt);

            // DV capture does not use a VIDEOINFOHEADER
       if(hr == NOERROR) {
         if(pmt->formattype == FORMAT_VideoInfo) {
            // resize our window to the new capture size
            ResizeWindow(HEADER(pmt->pbFormat)->biWidth,
              abs(HEADER(pmt->pbFormat)->biHeight));
          }
        DeleteMediaType(pmt);
        }
      }
			*/

    CoTaskMemFree(cauuid.pElems);
    pSpec->Release();
		}

  pSC->Release();
//                if(gcap.fWantPreview) {
//                  BuildPreviewGraph();
//                  StartPreview();
//									}

#endif
	}

void CVideoSrcDialog::OnButton3() {
	int i;
	
#ifndef USA_DIRECTX
	if(i=myParent->theTV->inCapture)
		myParent->theTV->Capture(FALSE); 
		capDlgVideoFormat(myParent->theTV->GetHwnd());
/*	if(capDlgVideoFormat(myParent->theTV->GetHwnd())) {
		capGetVideoFormat(myParent->theTV->GetHwnd(),sizeof(myParent->theTV->biRawDef),&myParent->theTV->biRawDef);
		myParent->theTV->biCompDef=myParent->theTV->biRawDef;
		AfxMessageBox("Attenzione! Funzione non ancora completamente supportata!");

		// FARE qualcosa!!
		}*/
	myParent->theTV->Capture(i); 
#else
#endif
	}

void CVideoSrcDialog::OnButton4() {
	int i;

#ifndef USA_DIRECTX
	if(i=myParent->theTV->inCapture)
		myParent->theTV->Capture(FALSE); 
	capDlgVideoCompression(myParent->theTV->GetHwnd()); 
	myParent->theTV->Capture(i); 
#else
#endif
	}


void CVideoSrcDialog::OnSelendokCombo1() {
	
	updateDaCheck();	
	}

void CVideoSrcDialog::updateDaCheck() {

	UpdateData();
	GetDlgItem(IDC_RADIO14)->EnableWindow(m_Schede>0);
	GetDlgItem(IDC_RADIO15)->EnableWindow(m_Schede>0);
	if(myParent->theTV) {		// gestire + schede, quando lo si fara'...
#ifndef USA_DIRECTX
		if(!myParent->theTV->hasOverlay()) {
#endif
			GetDlgItem(IDC_RADIO15)->EnableWindow(FALSE);
			m_Overlay=0;
#ifndef USA_DIRECTX
			}
#endif
		}
#ifndef USA_DIRECTX
	GetDlgItem(IDC_BUTTON1)->EnableWindow(myParent->theTV->GetHwnd() != NULL && myParent->theTV->captureCaps.fHasDlgVideoSource);
	GetDlgItem(IDC_BUTTON2)->EnableWindow((myParent->Opzioni /*& CVidsendDoc2::videoType*/) && myParent->theTV->GetHwnd() != NULL && myParent->theTV->captureCaps.fHasDlgVideoDisplay);
	GetDlgItem(IDC_BUTTON3)->EnableWindow((myParent->Opzioni /*& CVidsendDoc2::videoType*/) && myParent->theTV->GetHwnd() != NULL && myParent->theTV->captureCaps.fHasDlgVideoFormat);
	GetDlgItem(IDC_BUTTON4)->EnableWindow((myParent->Opzioni /*& CVidsendDoc2::videoType*/) && myParent->theTV->GetHwnd() != NULL && 1 /* myParent->theTV->captureCaps.fHasDlgVideoCompression non c'e'!*/);
#else
	GetDlgItem(IDC_BUTTON1)->EnableWindow(myParent->theTV->GetHwnd() != NULL);
	GetDlgItem(IDC_BUTTON2)->EnableWindow(myParent->theTV->GetHwnd() != NULL);
#endif

	GetDlgItem(IDC_RADIO4)->EnableWindow(m_Schede > 0);
	GetDlgItem(IDC_RADIO9)->EnableWindow(m_Schede > 1);
	GetDlgItem(IDC_RADIO10)->EnableWindow(m_Schede > 2);
	GetDlgItem(IDC_RADIO12)->EnableWindow(m_Schede > 3);
//??	GetDlgItem(IDC_RADIO11)->EnableWindow(m_Schede > 0);
	GetDlgItem(IDC_CHECK2)->EnableWindow(m_Schede > 0);
	}

#endif

/////////////////////////////////////////////////////////////////////////////
// CTimedMessageBox dialog


CTimedMessageBox::CTimedMessageBox(CWnd* pParent /*=NULL*/)
	: CDialog(CTimedMessageBox::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTimedMessageBox)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CTimedMessageBox::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTimedMessageBox)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTimedMessageBox, CDialog)
	//{{AFX_MSG_MAP(CTimedMessageBox)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTimedMessageBox message handlers

BOOL CTimedMessageBox::OnInitDialog() {
	CDialog::OnInitDialog();
	
	SetWindowText(myTitle);
	SetDlgItemText(IDC_TEXT1,myText);
	if(myFlags & MB_OKCANCEL) {
		GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
		GetDlgItem(IDBUTTON2)->ShowWindow(SW_SHOW);
		GetDlgItem(IDBUTTON3)->ShowWindow(SW_SHOW);
		}
	else if(myFlags & MB_YESNO) {
		GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
		GetDlgItem(IDBUTTON2)->ShowWindow(SW_SHOW);
		SetDlgItemText(IDBUTTON2,"Sì");
		GetDlgItem(IDBUTTON3)->ShowWindow(SW_SHOW);
		SetDlgItemText(IDBUTTON3,"No");
		}
	else {
		}
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}

int CTimedMessageBox::DoModal(CString title,CString text,DWORD flags,DWORD showTime) {

	myTitle=title;
	myText=text;
	myFlags=flags;
	elapsedTime=timeGetTime()+showTime;
	return CDialog::DoModal();
	}

int CTimedMessageBox::ContinueModal() {
	
	return timeGetTime() < elapsedTime;
	}

int CTimedMessageBox::DoModeless(CString title,CString text,DWORD flags) {

	myTitle=title;
	myText=text;
	myFlags=flags;
	return CDialog::Create(MAKEINTRESOURCE(IDD));
	}

int CTimedMessageBox::EndModeless() {
	
	return DestroyWindow();
	}


/////////////////////////////////////////////////////////////////////////////
// CPasswordDlg dialog

CPasswordDlg::CPasswordDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPasswordDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CPasswordDlg)
	m_Nome = _T("");
	m_Pasw = _T("");
	//}}AFX_DATA_INIT
}


void CPasswordDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPasswordDlg)
	DDX_Text(pDX, IDC_EDIT1, m_Nome);
	DDV_MaxChars(pDX, m_Nome, 31);
	DDX_Text(pDX, IDC_EDIT3, m_Pasw);
	DDV_MaxChars(pDX, m_Pasw, 15);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPasswordDlg, CDialog)
	//{{AFX_MSG_MAP(CPasswordDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPasswordDlg message handlers

BOOL CPasswordDlg::OnInitDialog() {
	
	CDialog::OnInitDialog();
	
	if(!m_Nome.IsEmpty()) {
		GetDlgItem(IDC_EDIT3)->SetFocus();
		return FALSE;
		}
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}



/////////////////////////////////////////////////////////////////////////////
// CInputBoxDlg dialog


CInputBoxDlg::CInputBoxDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CInputBoxDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CInputBoxDlg)
	m_Text = _T("");
	//}}AFX_DATA_INIT
}


void CInputBoxDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CInputBoxDlg)
	DDX_Text(pDX, IDC_EDIT1, m_Text);
	DDV_MaxChars(pDX, m_Text, 255);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CInputBoxDlg, CDialog)
	//{{AFX_MSG_MAP(CInputBoxDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInputBoxDlg message handlers

BOOL CInputBoxDlg::OnInitDialog() {
	CDialog::OnInitDialog();
	
	UpdateData(FALSE);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	}




/////////////////////////////////////////////////////////////////////////////
// CSalvaVideoDlg dialog

CSalvaVideoDlg::CSalvaVideoDlg(CVidsendDoc2 *pParent /*=NULL*/)
	: CDialog(CSalvaVideoDlg::IDD)
{
	//{{AFX_DATA_INIT(CSalvaVideoDlg)
	m_salvaPath = _T("");
	m_QuantiFrame = -1;
	m_QuantiFile = -1;
	m_QuandoCancello = -1;
	m_AutoAvvia = FALSE;
	m_Accoda = FALSE;
	//}}AFX_DATA_INIT
	myParent=pParent;
}


void CSalvaVideoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSalvaVideoDlg)
	DDX_Text(pDX, IDC_EDIT1, m_salvaPath);
	DDV_MaxChars(pDX, m_salvaPath, 127);
	DDX_Radio(pDX, IDC_RADIO1, m_QuantiFrame);
	DDX_Radio(pDX, IDC_RADIO3, m_QuantiFile);
	DDX_CBIndex(pDX, IDC_COMBO1, m_QuandoCancello);
	DDX_Check(pDX, IDC_CHECK1, m_AutoAvvia);
	DDX_Check(pDX, IDC_CHECK2, m_Accoda);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSalvaVideoDlg, CDialog)
	//{{AFX_MSG_MAP(CSalvaVideoDlg)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_BN_CLICKED(IDC_RADIO3, OnRadio3)
	ON_BN_CLICKED(IDC_RADIO11, OnRadio3)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSalvaVideoDlg message handlers

void CSalvaVideoDlg::OnButton1() {

	UpdateData(TRUE);
	if(m_QuantiFile) {
		BROWSEINFO bi;
		LPITEMIDLIST lpdi;
		LPSTR lpBuffer;

		LPMALLOC g_pMalloc; 
		if(SHGetMalloc(&g_pMalloc) == E_FAIL)
			return ;
		if((lpBuffer = (LPSTR) g_pMalloc->Alloc(MAX_PATH)) == NULL) 
			return;
 
		ZeroMemory(&bi,sizeof(bi));
		bi.hwndOwner=m_hWnd;
		bi.pidlRoot=NULL; //DESKTOP
		bi.pszDisplayName=lpBuffer;
		bi.lpszTitle="Scegliere la cartella di destinazione:";
		bi.ulFlags=0;
		bi.iImage=NULL;
		if(lpdi=SHBrowseForFolder(&bi)) {
			if(SHGetPathFromIDList(lpdi, lpBuffer)) {
				m_salvaPath=lpBuffer;
 				UpdateData(FALSE);
				}
			else
				AfxMessageBox("Selezionare una posizione valida!",MB_ICONEXCLAMATION);
			}

		g_pMalloc->Free(lpdi); 
		}
	else {
		CString S=m_salvaPath.Right(1) == '\\' ? "" : m_salvaPath;
		CFileDialog myDlg(FALSE,NULL,S,OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,"File video (*.avi)|*.AVI|Tutti i file (*.*)|*.*||");
		if(myDlg.DoModal() == IDOK) {
			m_salvaPath=myDlg.GetPathName();
			UpdateData(FALSE);
			}
		}
	
}

BOOL CSalvaVideoDlg::OnInitDialog() {
	CDialog::OnInitDialog();
	
	m_QuandoCancello=myParent->OpzioniSalvaVideo & 0xf;
	m_QuantiFile=myParent->OpzioniSalvaVideo & CVidsendDoc2::quantiFile ? 1 : 0;
	m_QuantiFrame=myParent->OpzioniSalvaVideo & CVidsendDoc2::quantiFrame ? 1 : 0;
	m_AutoAvvia=myParent->OpzioniSalvaVideo & CVidsendDoc2::autoAvvia ? 1 : 0;

	UpdateData(FALSE);
	OnRadio3();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}


void CSalvaVideoDlg::OnRadio3() {

	UpdateData(TRUE);
	if(m_QuantiFile)
		m_salvaPath=myParent->pathAVI;
	else
		m_salvaPath=myParent->pathAVI+myParent->nomeAVI;

	UpdateData(FALSE);
	
	}

/////////////////////////////////////////////////////////////////////////////
// CApriVideoDlg dialog

CApriVideoDlg::CApriVideoDlg(CVidsendDoc2* pParent /*=NULL*/)
	: CDialog(CApriVideoDlg::IDD)
{
	//{{AFX_DATA_INIT(CApriVideoDlg)
	m_Loop = FALSE;
	m_NomeFile = _T("");
	m_TipoVideo = -1;
	//}}AFX_DATA_INIT
	myParent=pParent;
}


void CApriVideoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CApriVideoDlg)
	DDX_Check(pDX, IDC_CHECK2, m_Loop);
	DDX_Text(pDX, IDC_EDIT1, m_NomeFile);
	DDV_MaxChars(pDX, m_NomeFile, 127);
	DDX_Radio(pDX, IDC_RADIO1, m_TipoVideo);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CApriVideoDlg, CDialog)
	//{{AFX_MSG_MAP(CApriVideoDlg)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CApriVideoDlg message handlers

void CApriVideoDlg::OnButton1() {
	CFileDialog myDlg(TRUE,"*.avi",NULL,OFN_HIDEREADONLY,"File video (*.avi)|*.avi|File MPEG (*.mpg)|*.mpg|Tutti i file (*.*)|*.*||",this);

	if(myDlg.DoModal() == IDOK) {
		m_NomeFile=myDlg.GetPathName();
		UpdateData(FALSE);
		}
	}


BOOL CApriVideoDlg::OnInitDialog() {
	CDialog::OnInitDialog();
	
	m_Loop=myParent->OpzioniSorgenteVideo & CVidsendDoc2::aviLoop ? 1 : 0;	
	m_TipoVideo=myParent->OpzioniSorgenteVideo & CVidsendDoc2::aviMode ? 1 : 0;	
	m_NomeFile=myParent->nomeAVI_PB;
	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}


/////////////////////////////////////////////////////////////////////////////
// CSalvaFTPDlg dialog


CSalvaFTPDlg::CSalvaFTPDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSalvaFTPDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSalvaFTPDlg)
	//}}AFX_DATA_INIT
	theBitmap=NULL;
	theBits=NULL;
	returnValue=0;
}


void CSalvaFTPDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSalvaFTPDlg)
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSalvaFTPDlg, CDialog)
	//{{AFX_MSG_MAP(CSalvaFTPDlg)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDOK2, OnOk2)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


int CSalvaFTPDlg::DoModal(BITMAPINFOHEADER *b,BYTE *p) {

	theBits=p;
	theBitmap=b;
	return CDialog::DoModal();
	}

void CSalvaFTPDlg::OnPaint() {
	CPaintDC dc(this); // device context for painting
	RECT r;
	
	if(theBitmap) {
		GetClientRect(&r);
		r.top+=5;
		r.left+=5;
		r.bottom-=45;
		r.right-=10;
		CVidsendApp::renderBitmap(&dc,(LPBITMAPINFO)theBitmap,theBits,&r);
		// castare a BITMAPINFO non e' perfetto, ma non essendoci qua le palette (16M colori sempre), dovrebbe andare bene!
		}
	
	// Do not call CDialog::OnPaint() for painting messages
	}

void CSalvaFTPDlg::OnOk2() {

	returnValue=1;
	OnOK();	
	}


/////////////////////////////////////////////////////////////////////////////
// CConfirmExhibDlg dialog


CConfirmExhibDlg::CConfirmExhibDlg(CVidsendDoc2 *pParent /*=NULL*/)
	: CDialog(CConfirmExhibDlg::IDD /*, pParent*/)
{
	//{{AFX_DATA_INIT(CConfirmExhibDlg)
	m_Costo = 0.0;
	m_Password = _T("");
	m_Titolo = _T("");
	m_Descrizione = _T("");
	m_Sconto = 0;
	m_Adulti = FALSE;
	m_Categoria = -1;
	m_Sessione = -1;
	//}}AFX_DATA_INIT
	myParent=pParent;
}


void CConfirmExhibDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfirmExhibDlg)
	DDX_Text(pDX, IDC_EDIT1, m_Costo);
	DDV_MinMaxDouble(pDX, m_Costo, 0.0, 6.);
	DDX_Text(pDX, IDC_EDIT5, m_Password);
	DDV_MaxChars(pDX, m_Password, 16);
	DDX_Text(pDX, IDC_EDIT12, m_Titolo);
	DDV_MaxChars(pDX, m_Titolo, 80);
	DDX_Text(pDX, IDC_EDIT13, m_Descrizione);
	DDV_MaxChars(pDX, m_Descrizione, 255);
	DDX_Text(pDX, IDC_EDIT3, m_Sconto);
	DDV_MinMaxDWord(pDX, m_Sconto, 0, 50);
	DDX_Check(pDX, IDC_CHECK1, m_Adulti);
	DDX_CBIndex(pDX, IDC_COMBO1, m_Categoria);
	DDX_CBIndex(pDX, IDC_COMBO7, m_Sessione);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfirmExhibDlg, CDialog)
	//{{AFX_MSG_MAP(CConfirmExhibDlg)
	ON_BN_CLICKED(IDC_CHECK1, OnCheck1)
	ON_BN_CLICKED(IDC_BUTTON3, OnButton3)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfirmExhibDlg message handlers

void CConfirmExhibDlg::OnOK() {

	UpdateData();
	if(m_Titolo.IsEmpty()) {
		AfxMessageBox("Specificare il titolo della sessione");
		goto fine;
		}
	if(m_Descrizione.IsEmpty()) {
		AfxMessageBox("Descrivere la sessione");
		goto fine;
		}
	if(m_Sconto>0) {
//		if(m_Sconto > 50) {		// c'e' gia' nel Dialog!
		if(m_Password.IsEmpty()) {
			AfxMessageBox("Se si consentono sconti, è obbligatoria la password");
			goto fine;
			}
		}
	if(m_Categoria<0) {
		AfxMessageBox("Indicare la categoria");
		goto fine;
		}
	
	CDialog::OnOK();

fine:
	;
	}

BOOL CConfirmExhibDlg::OnInitDialog() {
	char myBuf[128];

	CDialog::OnInitDialog();
	
//	m_Sconto=0;
	m_Sessione=0;
	m_Adulti=0;
	if(myParent && myParent->authSocket) {
// non nell'INI...
		m_Costo=atof(myParent->authSocket->tariffa);
		m_Sconto=myParent->authSocket->sconto;
		m_Password=myParent->authSocket->passwordSconto;
//		m_Costo=((double)myParent->GetPrivateProfileInt(IDS_PREZZO_EXHIB))/100;
//		m_Sconto=myParent->GetPrivateProfileInt(IDS_SCONTO_EXHIB);
//		myParent->GetPrivateProfileString(IDS_PASSWORD_SCONTO_EXHIB,myBuf,127);
//		m_Password=myBuf;
		}
	UpdateData(FALSE);
	updateDaCheck();

	return TRUE;  // return TRUE unless you set the focus to a control
	}

void CConfirmExhibDlg::updateDaCheck() {

	UpdateData();
	((CComboBox *)GetDlgItem(IDC_COMBO1))->ResetContent();
	if(m_Adulti) {
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Girls Alone (Donne sole)");
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Lesbo (Lesbiche)");
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Couple (Coppie)");
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Gender Bender TS/TV/CD (Transessuali)");
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Guys - Alone (Uomini soli)");
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Groups (Gruppi)");
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Dungeon (Sado Maso)");
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Fetish (Fetish)");
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Interracial (Coppie miste)");
		((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Guys-Gay (Uomini-Gay)");
		}
	((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Dating (Consulenze)");
	((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("How To/Instruction (Aiuto online)");
	((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("International Chat (Chat internazionale)");
	((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Physics/Advice (Sport/Attività)");
	((CComboBox *)GetDlgItem(IDC_COMBO1))->AddString("Friendly and Family (Amicizie e famiglia)");
	}


void CConfirmExhibDlg::OnCheck1() {

	updateDaCheck();	
	}


void CConfirmExhibDlg::OnButton3() {
	CString S;

	S=myParent->authenticationWWW;
	S+="/QuestionAnswer/Help.asp?Lingua=\"in/it\"";
	ShellExecute(theApp.m_pMainWnd->m_hWnd,"open",(LPCTSTR)S,NULL,NULL,0);
	}



/////////////////////////////////////////////////////////////////////////////
// CBrowserDlg dialog

CBrowserDlg::CBrowserDlg(const char *url, CWnd* pParent /*=NULL*/)
	: CDialog(CBrowserDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CBrowserDlg)
	//}}AFX_DATA_INIT
	strcpy(myURL,url);
	updateOK=FALSE;
	}


void CBrowserDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBrowserDlg)
	DDX_Control(pDX, IDC_EXPLORER1, m_Browser);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CBrowserDlg, CDialog)
	//{{AFX_MSG_MAP(CBrowserDlg)
	ON_WM_CLOSE()
	ON_WM_CREATE()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBrowserDlg message handlers

BOOL CBrowserDlg::OnInitDialog() {
	CDialog::OnInitDialog();

	VARIANT n1,n2;

	n2.vt=VT_I4;
	n2.intVal =2 | 4 | 8;
//	AfxMessageBox(myURL);
	m_Browser.Navigate(myURL,&n2,NULL,NULL,NULL);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}




BEGIN_EVENTSINK_MAP(CBrowserDlg, CDialog)
    //{{AFX_EVENTSINK_MAP(CBrowserDlg)
	ON_EVENT(CBrowserDlg, IDC_EXPLORER1, 113 /* TitleChange */, OnTitleChangeExplorer1, VTS_BSTR)
	ON_EVENT(CBrowserDlg, IDC_EXPLORER1, 104 /* DownloadComplete */, OnDownloadCompleteExplorer1, VTS_NONE)
	ON_EVENT(CBrowserDlg, IDC_EXPLORER1, 252 /* NavigateComplete2 */, OnNavigateComplete2Explorer1, VTS_DISPATCH VTS_PVARIANT)
	ON_EVENT(CBrowserDlg, IDC_EXPLORER1, 259 /* DocumentComplete */, OnDocumentCompleteExplorer1, VTS_DISPATCH VTS_PVARIANT)
	ON_EVENT(CBrowserDlg, IDC_EXPLORER1, 108 /* ProgressChange */, OnProgressChangeExplorer1, VTS_I4 VTS_I4)
	//}}AFX_EVENTSINK_MAP
END_EVENTSINK_MAP()

void CBrowserDlg::OnTitleChangeExplorer1(LPCTSTR Text) {
	CString S;

/*	S="il titolo e' cambiato: ";
	S+=Text;
	AfxMessageBox(S);	
	AfxMessageBox(m_Browser.GetLocationName());*/


	if(!strncmp(Text,"200",3)) {
		updateOK=TRUE;
#ifndef _CAMPARTY_MODE
		OnCancel();
#endif
		}
	if(!strncmp(Text,"400",3)) {
#ifndef _CAMPARTY_MODE
		OnCancel();
#endif
		}
#ifdef _CAMPARTY_MODE
	SetWindowText(Text);
	// metterlo anche per NEWMEET?
#endif
	
	}

void CBrowserDlg::OnDownloadCompleteExplorer1() {
	//	AfxMessageBox("pagina caricata");	
	}

//BOOL CBrowserDlg::PreCreateWindow(CREATESTRUCT& cs) {
//PreCreateWindow non viene chiamata per le dialog ?!?
	

void CBrowserDlg::OnClose() {

#ifndef _CAMPARTY_MODE
	CDialog::OnClose();
#endif
	}


BOOL CBrowserDlg::Create(const char *s,const RECT *rc, DWORD stile) {
	int i;
	DWORD l;
	
	i=CDialog::Create(IDD, NULL);
	if(i && !IsRectEmpty(rc)) {
		SetWindowPos(&wndTop,rc->left,rc->top,rc->right-rc->left,rc->bottom-rc->top,SWP_NOACTIVATE | SWP_NOZORDER);
		}
	l=::GetWindowLong(m_hWnd,GWL_STYLE);
	l |= stile;
	::SetWindowLong(m_hWnd,GWL_STYLE,l);
	SetWindowText(s);
	return i;
	}

int CBrowserDlg::OnCreate(LPCREATESTRUCT lpCreateStruct) {

#ifdef _CAMPARTY_MODE				// per PROVA! metto ifndef


//	cs.style &= ~WS_CLOSE;			// NO! l'unico modo sembra essere intercettare il Close...

//	CString S;
//	S.Format("style = %x",lpCreateStruct->style);
//	AfxMessageBox(S);

//	lpCreateStruct->style |= WS_VISIBLE | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
//	lpCreateStruct->dwExStyle &= ~WS_EX_TOPMOST;


//	S.Format("new style = %x",lpCreateStruct->style);
//	AfxMessageBox(S);

	DWORD ws;

	ws=GetWindowLong(m_hWnd,GWL_EXSTYLE);
	ws &= ~WS_EX_TOPMOST;
	SetWindowLong(m_hWnd,GWL_EXSTYLE,ws);

	ws=GetWindowLong(m_hWnd,GWL_STYLE);
//	ws &= ~WS_DLGFRAME;
//	ws |= WS_VISIBLE | WS_MINIMIZEBOX | WS_MAXIMIZEBOX /*| WS_THICKFRAME*/;
	ws =WS_OVERLAPPEDWINDOW;
	SetWindowLong(m_hWnd,GWL_STYLE,ws);

//	S.Format("hwnd=%x,new style = %x",m_hWnd,ws);
//	AfxMessageBox(S);

#endif

	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
	}

void CBrowserDlg::OnSize(UINT nType, int cx, int cy) {

	CDialog::OnSize(nType, cx, cy);

	RECT rc;
	GetClientRect(&rc);
	
	if(GetDlgItem(IDC_EXPLORER1))
		GetDlgItem(IDC_EXPLORER1)->MoveWindow(rc.left+10,rc.top+10,rc.right-20,rc.bottom-20);
	}



#ifdef _CAMPARTY_MODE
/////////////////////////////////////////////////////////////////////////////
// CConfirmCamDlg dialog


CConfirmCamDlg::CConfirmCamDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConfirmCamDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfirmCamDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CConfirmCamDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfirmCamDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfirmCamDlg, CDialog)
	//{{AFX_MSG_MAP(CConfirmCamDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfirmCamDlg message handlers

#endif


/////////////////////////////////////////////////////////////////////////////
// CMyPrintDialog

IMPLEMENT_DYNAMIC(CMyPrintDialog, CPrintDialog)

CMyPrintDialog::CMyPrintDialog(DWORD dwFlags, CWnd* pParentWnd) :
	CPrintDialog(FALSE, dwFlags, pParentWnd)
{
	//{{AFX_DATA_INIT(CMyPrintDialog)
	m_Size = 1;
	m_SuperImpose=0;
	//}}AFX_DATA_INIT
	m_pd.lpPrintTemplateName =MAKEINTRESOURCE(IDD_PRINT);
	m_pd.Flags |= PD_ENABLEPRINTTEMPLATE | PD_ENABLEPRINTHOOK;
	m_pd.hInstance=theApp.m_hInstance;
	m_pd.lpfnPrintHook=PrintHookFn;
	m_pd.lCustData=(DWORD)this;
	m_pd.nFromPage=1;
	m_pd.nToPage=1;
	m_pd.nMinPage=1;
	m_pd.nMaxPage=1;
	}


void CMyPrintDialog::DoDataExchange(CDataExchange* pDX)
{
	CPrintDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMyPrintDialog)
	DDX_Radio(pDX, IDC_RADIO1, m_Size);
	DDX_Check(pDX, IDC_CHECK1, m_SuperImpose);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMyPrintDialog, CPrintDialog)
	//{{AFX_MSG_MAP(CMyPrintDialog)
//	ON_MESSAGE(WM_INITDIALOG, OnInitDialog)		// se lo metto, prende il mio ma perdo tutto il resto!!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CMyPrintDialog::OnInitDialog() {
	
	CPrintDialog::OnInitDialog();
	
	m_Size=0;				// questa non la esegue... quindi uso la HookFunction
	UpdateData(FALSE);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}


UINT CALLBACK PrintHookFn(HWND hDlg, UINT message, WPARAM wParam, LONG lParam) {
  int i;
  char myBuf[64];
  static PRINTDLG *pd;

  switch(message) {
    case WM_INITDIALOG:
      pd=(PRINTDLG *)lParam;
			((CMyPrintDialog *)pd->lCustData)->UpdateData(FALSE);
      break;
    case WM_COMMAND:
      break;
    default:
      return 0;
    }
  return 0;
  }



/////////////////////////////////////////////////////////////////////////////
// CControlsWnd

CControlsWnd::CControlsWnd()
{
}

CControlsWnd::~CControlsWnd()
{
}


BEGIN_MESSAGE_MAP(CControlsWnd, CWnd)
	//{{AFX_MSG_MAP(CControlsWnd)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CControlsWnd message handlers


void CBrowserDlg::OnNavigateComplete2Explorer1(LPDISPATCH pDisp, VARIANT FAR* URL) 
{
	// TODO: Add your control notification handler code here
	
}

void CBrowserDlg::OnDocumentCompleteExplorer1(LPDISPATCH pDisp, VARIANT FAR* URL) {
	// TODO: Add your control notification handler code here
	
}

void CBrowserDlg::OnProgressChangeExplorer1(long Progress, long ProgressMax) {
	// TODO: Add your control notification handler code here
	
}




/////////////////////////////////////////////////////////////////////////////
// CQualityBoxDlg dialog

CQualityBoxDlg::CQualityBoxDlg(RECT *rc, CWnd* pParent /*=NULL*/)
	: CDialog(CQualityBoxDlg::IDD, pParent) {

	SetRectEmpty(&SelectedRect);
	if(rc)
		SelectedRect=*rc;

	//{{AFX_DATA_INIT(CQualityBoxDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	}


void CQualityBoxDlg::DoDataExchange(CDataExchange* pDX) {

	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CQualityBoxDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CQualityBoxDlg, CDialog)
	//{{AFX_MSG_MAP(CQualityBoxDlg)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_CAPTURECHANGED()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CQualityBoxDlg message handlers


BOOL CQualityBoxDlg::OnInitDialog() {
	CDialog::OnInitDialog();
	POINT pt;

//	g_MovingMainWnd = false;
	bTrack = FALSE;
	Shape = SL_BLOCK;                  /* Shape to use for rectangle */
	RetainShape = FALSE;              /* Retain or destroy shape    */

	if(!IsRectEmpty(&SelectedRect))
		UpdateSelection(pt,&SelectedRect,SL_BLOCK);			// finire! aggiustare coord...
	UpdateData(FALSE);

//mettere un timer e sfornare frame...		theApp.renderBitmap(pDC,IDB_MONOSCOPIO,&r);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	}


/* ESEMPIo per muovere finestra
void CQualityBoxDlg::OnMouseMove(UINT nFlags, CPoint point) {
	
	if(g_MovingMainWnd) {
		POINT pt;
		if(GetCursorPos(&pt)) {
			int wnd_x = g_OrigWndPos.x + (pt.x - g_OrigCursorPos.x);
			int wnd_y = g_OrigWndPos.y + (pt.y - g_OrigCursorPos.y);
			SetWindowPos(NULL, wnd_x, wnd_y, 0, 0, 
				SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSIZE);
			}
		}	

	CDialog::OnMouseMove(nFlags, point);
	}

void CQualityBoxDlg::OnLButtonUp(UINT nFlags, CPoint point) {

	ReleaseCapture();	

	CDialog::OnLButtonUp(nFlags, point);
	}

void CQualityBoxDlg::OnLButtonDown(UINT nFlags, CPoint point) {
	
	if(GetCursorPos(&g_OrigCursorPos)) {
		RECT rt;
		GetWindowRect(&rt);
		g_OrigWndPos.x = rt.left;
		g_OrigWndPos.y = rt.top;
		g_MovingMainWnd = true;
		SetCapture();
		SetCursor(LoadCursor(NULL, IDC_SIZEALL));
		}	

	CDialog::OnLButtonDown(nFlags, point);
	}

void CQualityBoxDlg::OnCaptureChanged(CWnd *pWnd) {
	g_MovingMainWnd = pWnd == this;	

	CDialog::OnCaptureChanged(pWnd);
	}
*/

void CQualityBoxDlg::OnMouseMove(UINT nFlags, CPoint point) {
	
  if(bTrack)
    UpdateSelection(point, &SelectedRect, Shape);

	CDialog::OnMouseMove(nFlags, point);
	}

void CQualityBoxDlg::OnLButtonUp(UINT nFlags, CPoint point) {

  if(bTrack) 
    EndSelection(point, &SelectedRect);
  bTrack = FALSE;

	CDialog::OnLButtonUp(nFlags, point);
	}

void CQualityBoxDlg::OnLButtonDown(UINT nFlags, CPoint point) {
	
  bTrack = TRUE;       /* user has pressed the left button */

  /* If you don't want the shape cleared, you must clear the Rect
   * coordinates before calling StartSelection
   */

  if(RetainShape)
		SetRectEmpty(&SelectedRect);

  StartSelection(point, &SelectedRect,
	  (nFlags & MK_SHIFT) ? SL_EXTEND | Shape : Shape);

	CDialog::OnLButtonDown(nFlags, point);
	}

void CQualityBoxDlg::OnCaptureChanged(CWnd *pWnd) {

	CDialog::OnCaptureChanged(pWnd);
	}



/****************************************************************************
    FUNCTION: StartSelection(HWND, POINT, LPRECT, int)
    PURPOSE: Begin selection of region
****************************************************************************/

int CQualityBoxDlg::StartSelection(POINT ptCurrent, LPRECT lpSelectRect, int fFlags) {

  if(lpSelectRect->left != lpSelectRect->right ||
    lpSelectRect->top != lpSelectRect->bottom)
		ClearSelection(lpSelectRect, fFlags);

  lpSelectRect->right = ptCurrent.x;
  lpSelectRect->bottom = ptCurrent.y;

  /* If you are extending the box, then invert the current rectangle */

  if((fFlags & SL_SPECIAL) == SL_EXTEND)
    ClearSelection(lpSelectRect, fFlags);

    /* Otherwise, set origin to current location */

  else {
    lpSelectRect->left = ptCurrent.x;
    lpSelectRect->top = ptCurrent.y;
    }
  SetCapture();

  return 1;
	}

/****************************************************************************
    FUNCTION: UpdateSelection(HWND, POINT, LPRECT, int) - update selection area
    PURPOSE: Update selection
****************************************************************************/

int CQualityBoxDlg::UpdateSelection(POINT ptCurrent, LPRECT lpSelectRect, int fFlags) {
  CDC *hDC;
  SHORT OldROP;

  hDC = GetDC();

	switch(fFlags & SL_TYPE) {

		case SL_BOX:
			OldROP = (SHORT)hDC->SetROP2(R2_NOTXORPEN);
			MoveToEx(hDC->m_hDC,lpSelectRect->left, lpSelectRect->top, NULL);
			hDC->LineTo(lpSelectRect->right, lpSelectRect->top);
			hDC->LineTo(lpSelectRect->right, lpSelectRect->bottom);
			hDC->LineTo(lpSelectRect->left, lpSelectRect->bottom);
			hDC->LineTo(lpSelectRect->left, lpSelectRect->top);
			hDC->LineTo(ptCurrent.x, lpSelectRect->top);
			hDC->LineTo(ptCurrent.x, ptCurrent.y);
			hDC->LineTo(lpSelectRect->left, ptCurrent.y);
			hDC->LineTo(lpSelectRect->left, lpSelectRect->top);
			hDC->SetROP2(OldROP);
			break;
    
    case SL_BLOCK:
      hDC->PatBlt(lpSelectRect->left, lpSelectRect->bottom,
        lpSelectRect->right - lpSelectRect->left,
        ptCurrent.y - lpSelectRect->bottom,
        DSTINVERT);
      hDC->PatBlt(lpSelectRect->right, lpSelectRect->top,
        ptCurrent.x - lpSelectRect->right,
        ptCurrent.y - lpSelectRect->top,
        DSTINVERT);
      break;
    }

  lpSelectRect->right = ptCurrent.x;
  lpSelectRect->bottom = ptCurrent.y;
  ReleaseDC(hDC);

  return 1;
	}

/****************************************************************************
    FUNCTION: EndSelection(POINT, LPRECT)
    PURPOSE: End selection of region, release capture of mouse movement
****************************************************************************/

int CQualityBoxDlg::EndSelection(POINT ptCurrent, LPRECT lpSelectRect) {

  lpSelectRect->right = ptCurrent.x;
  lpSelectRect->bottom = ptCurrent.y;
  ReleaseCapture();

  return 1;
	}

/****************************************************************************
    FUNCTION: ClearSelection(HWND, LPRECT, int) - clear selection area
    PURPOSE: Clear the current selection
****************************************************************************/

int CQualityBoxDlg::ClearSelection(LPRECT lpSelectRect, int fFlags) {
  CDC *hDC;
  DWORD OldROP;

	hDC = GetDC();
	switch (fFlags & SL_TYPE) {

		case SL_BOX:
			OldROP = hDC->SetROP2(R2_NOTXORPEN);
			MoveToEx(hDC->m_hDC,lpSelectRect->left, lpSelectRect->top, NULL);
			hDC->LineTo(lpSelectRect->right, lpSelectRect->top);
			hDC->LineTo(lpSelectRect->right, lpSelectRect->bottom);
			hDC->LineTo(lpSelectRect->left, lpSelectRect->bottom);
			hDC->LineTo(lpSelectRect->left, lpSelectRect->top);
			hDC->SetROP2(OldROP);
			break;

		case SL_BLOCK:
			hDC->PatBlt(lpSelectRect->left,	lpSelectRect->top,
				lpSelectRect->right - lpSelectRect->left,
				lpSelectRect->bottom - lpSelectRect->top,
				DSTINVERT);
			break;
    }
  ReleaseDC(hDC);

  return 1;
	}

void CQualityBoxDlg::OnOK() {
	DWORD n;
	
	if(SelectedRect.bottom < SelectedRect.top) {
		n=SelectedRect.bottom;
		SelectedRect.bottom = SelectedRect.top;
		SelectedRect.top=n;
		}
	if(SelectedRect.right < SelectedRect.left) {
		n=SelectedRect.right;
		SelectedRect.right = SelectedRect.left;
		SelectedRect.left=n;
		}
	if(!IsRectEmpty(&SelectedRect)) {		// se le coord sono invertite, dà falso (@#£$%)
		SelectedRect.right -= 20;
		SelectedRect.left -= 20;
		SelectedRect.top -= 20;
		SelectedRect.bottom -= 20;
		if(SelectedRect.left < 0)
			SelectedRect.left=0;
		if(SelectedRect.right < 0)
			SelectedRect.right=0;
		if(SelectedRect.top < 0)
			SelectedRect.top=0;
		if(SelectedRect.bottom < 0)
			SelectedRect.bottom=0;
		}
	
	CDialog::OnOK();
	}

void CQualityBoxDlg::OnTimer(UINT nIDEvent) {

//mettere un timer e sfornare frame...		theApp.renderBitmap(pDC,IDB_MONOSCOPIO,&r);
	}



