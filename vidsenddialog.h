//{{AFX_INCLUDES()
#include "webbrowser2.h"
#include "richtext.h"
//}}AFX_INCLUDES
#if !defined(AFX_VIDSENDDIALOG_H__F5FDF1C2_2277_11D4_94A8_00A0C9AFFE49__INCLUDED_)
#define AFX_VIDSENDDIALOG_H__F5FDF1C2_2277_11D4_94A8_00A0C9AFFE49__INCLUDED_


//#include <d3d8.h>
#include <d3dx8.h>
//#include <ddraw.h>
#include "Dxerr8.h"
#include <D3dx8tex.h>
#include "ErrorObject.h"


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// vidsendDialog.h : header file
//




//*******************************************************************
// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

class CAboutDlg : public CDialog {
public:
	//global variables for DirectX stuff
	LPDIRECT3D8             g_pD3D       ; // Used to create the D3DDevice
	LPDIRECT3DDEVICE8       g_pd3dDevice ; // Our rendering device
	LPDIRECT3DVERTEXBUFFER8 g_pVB        ; // Buffer to hold vertices
	LPDIRECT3DTEXTURE8      g_pTexture   ; // Our texture
	D3DPRESENT_PARAMETERS	d3dpp;
	BOOL					m_blnGeom	 ;
	float					m_fProj;
	int						m_nSpinType ;
	long numofVertices ;
	// A structure for our custom vertex type. We added texture coordinates
	struct CUSTOMVERTEX
	{
			D3DXVECTOR3 position; // The position
			D3DCOLOR    color;    // The color
			FLOAT       tu, tv;   // The texture coordinates
	};
	CErrorObject* pErrObject;
	UINT m_nTimer;
	UINT m_nTimer2;
	UINT m_nTimer3;
	BOOL Render();
	void SetupMatrices();
	void Cleanup();
	HRESULT InitGeometry();
	HRESULT InitD3D( HWND hWnd );
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { 
#ifdef _NEWMEET_MODE
#ifdef _LINGUA_INGLESE
		IDD = IDD_ABOUTBOX_NM_ENG
#else
		IDD = IDD_ABOUTBOX_NM
#endif
#else
		IDD = IDD_ABOUTBOX 
#endif
		};
	CStatic	m_picAbout;
	CString m_Text;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnPicture();
	afx_msg void OnText1();
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	};


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CSplashScreenEx (prec. CSplashDlg) dialog
// da John O'Byrne's - www.codeproject.com

typedef BOOL (WINAPI* FN_ANIMATE_WINDOW)(HWND,DWORD,DWORD);

// CSplashScreenEx

class CSplashScreenEx : public CWnd {
	DECLARE_DYNAMIC(CSplashScreenEx)

public:
	enum {
		CSS_FADEIN=0x0001,
		CSS_FADEOUT=0x0002,
		CSS_FADE=CSS_FADEIN | CSS_FADEOUT,
		CSS_SHADOW=0x0004,
		CSS_CENTERSCREEN=0x0008,
		CSS_CENTERAPP=0x0010,
		CSS_HIDEONCLICK=0x0020,

		CSS_TEXT_NORMAL=0x0000,
		CSS_TEXT_BOLD=0x0001,
		CSS_TEXT_ITALIC=0x0002,
		CSS_TEXT_UNDERLINE=0x0004

		};
public:
	CSplashScreenEx();
	virtual ~CSplashScreenEx();

	BOOL Create(CWnd *pWndParent,LPCTSTR szText=NULL,DWORD dwTimeout=2000,DWORD dwStyle=CSS_FADE | CSS_CENTERSCREEN | CSS_SHADOW);
	BOOL SetBitmap(UINT nBitmapID,COLORREF transparency=-1);
	BOOL SetBitmap(LPCTSTR szFileName,COLORREF transparency=-1);

	void Show();
	void Hide();

	void SetText(LPCTSTR szText);
	void SetTextFont(LPCTSTR szFont,int nSize,int nStyle);
	void SetTextDefaultFont();
	void SetTextColor(COLORREF crTextColor);
	void SetTextRect(CRect& rcText);
	void SetTextFormat(UINT uTextFormat);
	
protected:	
	FN_ANIMATE_WINDOW m_fnAnimateWindow;
	CWnd *m_pWndParent;
	CBitmap m_bitmap;
	CFont m_myFont;
	HRGN m_hRegion;
	
	DWORD m_dwStyle;
	DWORD m_dwTimeout;
	CString m_strText;
	CRect m_rcText;
	UINT m_uTextFormat;
	COLORREF m_crTextColor;

	int m_nBitmapWidth;
	int m_nBitmapHeight;
	int m_nxPos;
	int m_nyPos;
		
	HRGN CreateRgnFromBitmap(HBITMAP, COLORREF );
	void DrawWindow(CDC *pDC);

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT nIDEvent);
	LRESULT OnPrintClient(WPARAM wParam, LPARAM lParam);
protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();
	};


/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage

class CVidsendPropPage : public CPropertySheet {
	DECLARE_DYNAMIC(CVidsendPropPage)

// Construction
public:
	CVidsendPropPage(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CVidsendPropPage(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendPropPage)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CVidsendPropPage();

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendPropPage)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CVidsendDocPropPage0 dialog

class CVidsendDocPropPage0 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDocPropPage0)

// Construction
public:
	CVidsendDocPropPage0(CVidsendDoc *myParent=NULL);
	~CVidsendDocPropPage0();

public:
	int isInitialized;
// Dialog Data
	//{{AFX_DATA(CVidsendDocPropPage0)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF1_PAGE0_ENG };
#else
	enum { IDD = IDD_CONF1_PAGE0 };
#endif
	BOOL	m_FullScreen;
	BOOL	m_4_3;
	BOOL	m_DoubleSize;
	BOOL	m_BN;
	BOOL	m_SStereo;
	int		m_Buffers;
	BOOL	m_ResizeAll;
	BOOL	m_bBuffers;
	BOOL	m_SMono;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDocPropPage0)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDocPropPage0)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck7();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc *myParent;
	void updateDaCheck();
	};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CVidsendDocPropPage1 dialog

class CVidsendDocPropPage1 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDocPropPage1)

// Construction
public:
	CVidsendDocPropPage1(CVidsendDoc *myParent=NULL);
	~CVidsendDocPropPage1();

public:
	int isInitialized;
// Dialog Data
	//{{AFX_DATA(CVidsendDocPropPage1)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF1_PAGE1_ENG };
#else
	enum { IDD = IDD_CONF1_PAGE1 };
#endif
	CComboBox	m_ConnettiA;
	CIPAddressCtrl	m_Proxy;
	BOOL	m_Authenticate;
	BOOL	m_bProxy;
	CString	m_AuthWWW;
	CString	m_User;
	CString	m_Pasw;
	BOOL	m_bConnetti;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDocPropPage1)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDocPropPage1)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck1();
	afx_msg void OnCheck2();
	afx_msg void OnCheck3();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc *myParent;
	void updateDaCheck();
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage0 dialog

class CVidsendDoc2PropPage0 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc2PropPage0)

// Construction
public:
	CVidsendDoc2PropPage0(CVidsendDoc2 *myParent=NULL,struct QUALITY_MODEL_V *v=NULL,struct QUALITY_MODEL_A *a=NULL);	// NON deve essere NULL, ma un costruttore di default ci vuole!
	~CVidsendDoc2PropPage0();
	DWORD enumCompressorV(CComboBox *,DWORD);
	DWORD enumCompressorA(CComboBox *,DWORD);
//	BOOL ACMFORMATENUMCB acmFormatEnumCallback(HACMDRIVERID, LPACMFORMATDETAILS, DWORD, DWORD);
	static BOOL CALLBACK /*ACMDRIVERENUMCB*/ acmEnumCallback(HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport);

public:
	int isInitialized;

// Dialog Data
	DWORD m_FormatV;
	DWORD m_CompressorV;
	DWORD m_CompressorA;
	struct QUALITY_MODEL_V m_QV;
	struct QUALITY_MODEL_A m_QA;

	//{{AFX_DATA(CVidsendDoc2PropPage0)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF2_PAGE0_ENG };
#else
	enum { IDD = IDD_CONF2_PAGE0 };
#endif
	CComboBox	m_ComboCompressorV;
	CComboBox	m_ComboCompressorA;
	CComboBox	m_ComboImageSize;
	CComboBox	m_ComboFps;
	CComboBox m_ComboFormatoV;
	CComboBox m_ComboTipoAudio;
	int m_IPAddress;
	BOOL	m_ServerAudio;
	CString	m_Bandwidth;
	UINT	m_PortaV;
	UINT	m_PortaA;
	UINT	m_PortaVBAN;
	UINT	m_PortaMP3;
	BOOL	m_ServerStream;
	BOOL	m_ServerVideo;
	int		m_TipoVideo;
	int		m_QualityV;
	int		m_VBAN;
	int		m_MP3;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc2PropPage0)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc2PropPage0)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck1();
	afx_msg void OnSelchangeCombo4();
	afx_msg void OnSelchangeCombo3();
	afx_msg void OnSelchangeCombo1();
	afx_msg void OnSelchangeCombo2();
	afx_msg void OnSelchangeCombo6();
	afx_msg void OnCheck4();
	afx_msg void OnCheck8();
	afx_msg void OnCheck3();
	afx_msg void OnRadio1();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	void updateDaCheck();
	void updateBWH();
	CVidsendDoc2 *myParent;

	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage0Bis dialog

class CVidsendDoc2PropPage0Bis : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc2PropPage0Bis)

// Construction
public:
	CVidsendDoc2PropPage0Bis(CVidsendDoc2 *myParent=NULL,struct QUALITY_MODEL_V *v=NULL,struct QUALITY_MODEL_A *a=NULL);	// NON deve essere NULL, ma un costruttore di default ci vuole!
	~CVidsendDoc2PropPage0Bis();

public:
	int isInitialized;
	WORD maxSchede;
	struct QUALITY_MODEL_V m_QV;
	struct QUALITY_MODEL_A m_QA;

// Dialog Data
	//{{AFX_DATA(CVidsendDoc2PropPage0Bis)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF2_PAGE0BIS_ENG };
#else
	enum { IDD = IDD_CONF2_PAGE0BIS };
#endif
	int m_IPAddress;
	UINT m_Port;
	int		m_QualitaVideo;
	int		m_QualitaAudio;
	int		m_Bandwidth;
	BOOL	m_ServerAudio;
	int		m_Schede;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc2PropPage0Bis)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc2PropPage0Bis)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck8();
	afx_msg void OnCheck4();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnCheck1();
	afx_msg void OnButton1();
	afx_msg void OnButton2();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	void updateDaCheck();
	void updateBWH();
	void subUpdateBWH(int );
	void updateVar();
	CVidsendDoc2 *myParent;

	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage0 dialog

class CVidsendDoc2PropPage00 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc2PropPage00)

// Construction
public:
	CVidsendDoc2PropPage00(CVidsendDoc22 *myParent=NULL,struct QUALITY_MODEL_A *a=NULL);	// NON deve essere NULL, ma un costruttore di default ci vuole!
	~CVidsendDoc2PropPage00();
	DWORD enumCompressorA(CComboBox *,DWORD);
//	BOOL ACMFORMATENUMCB acmFormatEnumCallback(HACMDRIVERID, LPACMFORMATDETAILS, DWORD, DWORD);
	static BOOL CALLBACK DSEnumCallback(LPGUID lpGuid,LPCSTR lpcstrDescription,
         LPCSTR lpcstrModule,LPVOID lpContext);
	DWORD enumSchedeAudio(CComboBox *,DWORD);

public:
	int isInitialized;

// Dialog Data
	DWORD m_CompressorA;
	struct QUALITY_MODEL_A m_QA;
	WORD m_SchedaAudio1,m_SchedaAudio2,m_SchedaAudio0;

	//{{AFX_DATA(CVidsendDoc2PropPage0)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF2_PAGE00_ENG };
#else
	enum { IDD = IDD_CONF2_PAGE00 };
#endif
	CComboBox	m_ComboCompressorA;
	CComboBox m_ComboTipoAudio;
	CComboBox m_ComboTipoMP3;
	int m_IPAddress;
	BOOL	m_ServerAudio;
	CString	m_Bandwidth;
	UINT	m_PortaA;
	UINT	m_PortaVBAN;
	UINT	m_PortaMP3;
	int m_VBAN;
	int m_MP3;
	BOOL	m_ServerStream;
	BOOL	m_OutputStream;
	BOOL  m_Attiva1;
	BOOL  m_Attiva2;
	BOOL  m_Attiva0;
	BOOL  m_PreascoltoMono;
	CComboBox	m_ComboSchedaAudio1;
	CComboBox	m_ComboSchedaAudio2;
	CComboBox	m_ComboSchedaAudio0;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc2PropPage0)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc2PropPage0)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck1();
	afx_msg void OnSelchangeCombo4();
	afx_msg void OnSelchangeCombo1();
	afx_msg void OnSelchangeCombo2();
	afx_msg void OnSelchangeCombo6();
	afx_msg void OnSelchangeCombo5();
	afx_msg void OnSelchangeCombo14();
	afx_msg void OnCheck4();
	afx_msg void OnCheck14();
	afx_msg void OnCheck3();
	afx_msg void OnRadio1();
	afx_msg void OnButton7();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	void updateDaCheck();
	void updateBWH();
	CVidsendDoc22 *myParent;

	};



/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage1 dialog

class CVidsendDoc2PropPage1 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc2PropPage1)

// Construction
public:
	CVidsendDoc2PropPage1(CVidsendDoc2 *myParent=NULL);
	~CVidsendDoc2PropPage1();
	void updateDaCheck();

public:
	int isInitialized;

// Dialog Data
	//{{AFX_DATA(CVidsendDoc2PropPage1)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF2_PAGE1_ENG };
#else
	enum { IDD = IDD_CONF2_PAGE1 };
#endif
	BOOL	m_Forza_BN;
	int		m_trasmAudio;
	int		m_trasmVideo;
	int		m_ForzaAudio;
	int		m_ImposeDateTime;
	int		m_ImposeTextPos;
	CString	m_ImposeText;
	BOOL	m_Capovolgi;
	BOOL	m_Specchio;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc2PropPage1)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc2PropPage1)
	virtual BOOL OnInitDialog();
	afx_msg void OnButton1();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;

	};

#ifdef _NEWMEET_MODE
/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage1_NM dialog

class CVidsendDoc2PropPage1_NM : public CPropertyPage
{
	DECLARE_DYNCREATE(CVidsendDoc2PropPage1_NM)

// Construction
public:
	CVidsendDoc2PropPage1_NM(CVidsendDoc2 *myParent=NULL);
	~CVidsendDoc2PropPage1_NM();

	void updateDaCheck();

public:
	int isInitialized;

// Dialog Data
	//{{AFX_DATA(CVidsendDoc2PropPage1_NM)
	enum { IDD = IDD_CONF2_PAGE1_NM };
	BOOL	m_Forza_BN;
	BOOL	m_Capovolgi;
	BOOL	m_Specchio;
	int		m_ImposeDateTime;
	int		m_ImposeTextPos;
	CString	m_ImposeText;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc2PropPage1_NM)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc2PropPage1_NM)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;
};

#endif

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage2 dialog

class CVidsendDoc2PropPage2 : public CPropertyPage {

// Construction
public:
	DECLARE_DYNCREATE(CVidsendDoc2PropPage2)
	CVidsendDoc2PropPage2(CVidsendDoc2 *myParent);
	CVidsendDoc2PropPage2(CVidsendDoc22 *myParent);
	~CVidsendDoc2PropPage2();
	void updateDaCheck();

private:
//https://learn.microsoft.com/en-us/cpp/mfc/reference/run-time-object-model-services?view=msvc-170#implement_dyncreate
	CVidsendDoc2PropPage2() {}

public:
	int isInitialized;

// Dialog Data
	//{{AFX_DATA(CVidsendDoc2PropPage2)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF2_PAGE2_ENG };
#else
	enum { IDD = IDD_CONF2_PAGE2 };
#endif
	CListBox	m_Lista;
	CSpinButtonCtrl	m_MaxConnSpin;
	UINT	m_MaxConn;
	CString	m_AuthWWW;
	BOOL	m_bDirectoryServer;
	BOOL	m_bNeedAuthenticate;
	CString	m_DirectoryServer;
	CString	m_NomePerServer;
	int		m_TipoAutorizzazione;
	CString	m_ID;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc2PropPage2)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc2PropPage2)
	virtual BOOL OnInitDialog();
	afx_msg void OnInsert();
	afx_msg void OnEdit();
	afx_msg void OnDelete();
	afx_msg void OnSelchangeList1();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnCheck4();
	afx_msg void OnDblclkList1();
	afx_msg void OnDestroy();
	afx_msg void OnCheck9();
	afx_msg void OnRadio13();
	afx_msg void OnRadio1();
	afx_msg void OnRadio2();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;
	CVidsendDoc22 *myParent2;
	CVidsendSet2_ *mySet;

	};

#ifdef _NEWMEET_MODE

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage2_NM dialog

class CVidsendDoc2PropPage2_NM : public CDialog
{
	DECLARE_DYNCREATE(CVidsendDoc2PropPage2_NM)

// Construction
public:
	CVidsendDoc2PropPage2_NM(CVidsendDoc2 *myParent=NULL);
	~CVidsendDoc2PropPage2_NM();

	void updateDaCheck();

public:

// Dialog Data
	//{{AFX_DATA(CVidsendDoc2PropPage2_NM)
	enum { IDD = IDD_CONF2_PAGE2_NM };
	CString	m_SuonoIn;
	CString	m_SuonoOut;
	CString	m_DialUpNome;
	BOOL	m_ActivateIf;
	BOOL m_ActivateWaitConfirm;
	BOOL m_DialUp;
	UINT	m_MaxConn;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc2PropPage2_NM)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc2PropPage2_NM)
		virtual BOOL OnInitDialog();
	afx_msg void OnCheck6();
	afx_msg void OnCheck10();
	afx_msg void OnCheck11();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;

};

#endif

/////////////////////////////////////////////////////////////////////////////
// CConf2Page2Utenti dialog

class CConf2Page2Utenti : public CDialog
{
// Construction
public:
	CConf2Page2Utenti(CWnd* pParent = NULL,CVidsendSet2_ *mySet=NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CConf2Page2Utenti)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF2_PAGE2_UTENTI_ENG };
#else
	enum { IDD = IDD_CONF2_PAGE2_UTENTI };
#endif
	CTime	m_TimedConn;
	BOOL	m_bTimedConn;
	CString	m_User;
	CString	m_Pasw;
	CString	m_WelcomeMsg;
	BOOL	m_bPayPerView;
	BOOL	m_Active;
	BOOL	m_RemoteControl;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConf2Page2Utenti)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConf2Page2Utenti)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck2();
	afx_msg void OnCheck9();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	void updateDaCheck();
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2PropPage3 dialog

class CVidsendDoc2PropPage3 : public CPropertyPage
{

// Construction
public:
	DECLARE_DYNCREATE(CVidsendDoc2PropPage3)
	CVidsendDoc2PropPage3(CVidsendDoc2 *myParent);
	CVidsendDoc2PropPage3(CVidsendDoc22 *myParent);
	~CVidsendDoc2PropPage3();
	void updateDaCheck();

private:
	CVidsendDoc2PropPage3() {}

public:
	int isInitialized;

// Dialog Data
	//{{AFX_DATA(CVidsendDoc2PropPage3)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF2_PAGE3_ENG };
#else
	enum { IDD = IDD_CONF2_PAGE3 };
#endif
	BOOL	m_bOpenWWW;
	BOOL	m_DontSave;
	BOOL	m_bTimedConn;
	CString	m_SuonoIn;
	CString	m_SuonoOut;
	CString m_DialUpNome;
	CString	m_OpenWWW;
	CTime	m_TimedConn;
	CString	m_StreamTitle;
	CString	m_Messaggio;
	BOOL  m_ActivateIf;
	BOOL m_ActivateWaitConfirm;
	BOOL m_DialUp;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc2PropPage3)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc2PropPage3)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck6();
	afx_msg void OnCheck11();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;
	CVidsendDoc22 *myParent2;
};

/////////////////////////////////////////////////////////////////////////////
// CSalvaVideoDlg dialog

class CSalvaVideoDlg : public CDialog
{
// Construction
public:
	CSalvaVideoDlg(CVidsendDoc2 *myParent=NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CSalvaVideoDlg)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_SAVE_VIDEO_ENG };
#else
	enum { IDD = IDD_SAVE_VIDEO };
#endif
	CString	m_salvaPath;
	int		m_QuantiFrame;
	int		m_QuantiFile;
	int		m_QuandoCancello;
	BOOL	m_AutoAvvia;
	BOOL	m_Accoda;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSalvaVideoDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSalvaVideoDlg)
	afx_msg void OnButton1();
	virtual BOOL OnInitDialog();
	afx_msg void OnRadio3();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;
};

/////////////////////////////////////////////////////////////////////////////
// CApriVideoDlg dialog

class CApriVideoDlg : public CDialog
{
// Construction
public:
	CApriVideoDlg(CVidsendDoc2 * pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CApriVideoDlg)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_OPEN_VIDEO_ENG };
#else
	enum { IDD = IDD_OPEN_VIDEO };
#endif
	BOOL	m_Loop;
	CString	m_NomeFile;
	int		m_TipoVideo;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CApriVideoDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CApriVideoDlg)
	afx_msg void OnButton1();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;
};

/////////////////////////////////////////////////////////////////////////////
// CApriAudioDlg dialog

class CApriAudioDlg : public CDialog
{
// Construction
public:
	CApriAudioDlg(CVidsendDoc22 *pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CApriAudioDlg)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_OPEN_VIDEO_ENG };
#else
	enum { IDD = IDD_OPEN_VIDEO };
#endif
	BOOL	m_Loop;
	CString	m_NomeFile;
	int		m_TipoVideo;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CApriAudioDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CApriAudioDlg)
	afx_msg void OnButton1();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc22 *myParent;
};

/////////////////////////////////////////////////////////////////////////////
// CConfirmExhibDlg dialog

class CConfirmExhibDlg : public CDialog
{
// Construction
public:
	CConfirmExhibDlg(CVidsendDoc2 *p=NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CConfirmExhibDlg)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONFIRM_EXHIB_ENG };
#else
	enum { IDD = IDD_CONFIRM_EXHIB };
#endif
	double	m_Costo;
	CString	m_Password;
	CString	m_Titolo;
	CString	m_Descrizione;
	DWORD	m_Sconto;
	BOOL	m_Adulti;
	int		m_Categoria;
	int		m_Sessione;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfirmExhibDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfirmExhibDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck1();
	afx_msg void OnButton3();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;
	void updateDaCheck();
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage0 dialog

class CVidsendPropPage0 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendPropPage0)

// Construction
public:
	CVidsendPropPage0();
	~CVidsendPropPage0();

public:
	int isInitialized;
	CString m_IPAddressText;

// Dialog Data
	//{{AFX_DATA(CVidsendPropPage0)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF_PAGE0_ENG };
#else
	enum { IDD = IDD_CONF_PAGE0 };
#endif
	BOOL	m_ServerWWW;
	BOOL	m_ServerOra;
	int  m_IPAddress;
	BOOL	m_ServerAuth;
	BOOL	m_ServerDir;
	BOOL	m_RiconnettiV;
	BOOL	m_RiconnettiC;
	BOOL	m_Spool;
	BOOL	m_DDEenable;
	UINT	m_ServerWWWPort;
	BOOL	m_RiapriV;
	BOOL	m_RiapriA;
	BOOL	m_RiapriC;
	BOOL  m_TCP_UDP;
	BOOL  m_NamedPipes;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendPropPage0)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendPropPage0)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck6();
	afx_msg void OnSelchangeCombo12();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	void updateDaCheck();

	};
/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage1 dialog

class CVidsendPropPage1 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendPropPage1)

// Construction
public:
	CVidsendPropPage1();
	~CVidsendPropPage1();

public:
	int isInitialized;

// Dialog Data
	//{{AFX_DATA(CVidsendPropPage1)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF_PAGE1_ENG };
#else
	enum { IDD = IDD_CONF_PAGE1 };
#endif
	CString	m_Nome;
	CString	m_Cognome;
	CString	m_Citta;
	CString	m_Note;
	CString	m_Indirizzo;
	CString	m_Email;
	CString	m_SuonoFine;
	CString	m_SuonoInizio;
	CString	m_Sfondo;
	BOOL	m_SaveLayout;
	BOOL	m_PasswordProtect;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendPropPage1)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendPropPage1)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendPropPage2 dialog

class CVidsendPropPage2 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendPropPage2)
// Construction
public:
	CVidsendPropPage2();
	~CVidsendPropPage2();

public:
	int isInitialized;
// Dialog Data
	//{{AFX_DATA(CVidsendPropPage2)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF_PAGE2_ENG };
#else
	enum { IDD = IDD_CONF_PAGE2 };
#endif
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendPropPage2)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CVidsendPropPage2)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc3PropPage0 dialog

class CVidsendDoc3PropPage0 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc3PropPage0)

// Construction
public:
	CVidsendDoc3PropPage0(CVidsendDoc3 *myParent=NULL);
	~CVidsendDoc3PropPage0();

public:
	int isInitialized;

// Dialog Data
	//{{AFX_DATA(CVidsendDoc3PropPage0)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF3_PAGE0_ENG };
#else
	enum { IDD = IDD_CONF3_PAGE0 };
#endif
	CString	m_DSN;
	BOOL	m_LogAttivo;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc3PropPage0)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc3PropPage0)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc3 *myParent;
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage0 dialog

class CVidsendDoc4PropPage0 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc4PropPage0)

public:
	int isInitialized;

// Construction
public:
	CVidsendDoc4PropPage0(CVidsendDoc4 *p=NULL);
	~CVidsendDoc4PropPage0();

// Dialog Data
	//{{AFX_DATA(CVidsendDoc4PropPage0)
	enum { IDD = IDD_CONF4_PAGE0 };
	CSpinButtonCtrl	m_MaxconnSpin;
	UINT	m_MaxConn;
	BOOL	m_AncheWWW;
	BOOL	m_One2One;
	BOOL	m_noOne2One;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc4PropPage0)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc4PropPage0)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc4 *myParent;
	void updateDaCheck();
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage1 dialog

class CVidsendDoc4PropPage1 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc4PropPage1)

public:
	int isInitialized;
	int eData;
//	CIPAddressCtrl	m_IP;

// Construction
public:
	CVidsendDoc4PropPage1(CStringList *bl=NULL,CVidsendDoc4 *p=NULL);
	~CVidsendDoc4PropPage1();

// Dialog Data
	//{{AFX_DATA(CVidsendDoc4PropPage1)
	enum { IDD = IDD_CONF4_PAGE1 };
	CString	m_IP;
	BOOL	m_Attivo;
	BOOL	m_bLavagna;
	BOOL	m_DontSave;
	BOOL	m_NoPrivate;
	CString	m_OpenWWW;
	BOOL	m_bOpenWWW;
	BOOL	m_bAuthWWW;
	CString	m_AuthWWW;
	BOOL	m_bTimedConn;
	CTime	m_TimedConn;
	BOOL	m_Mostra_E_U;
	BOOL	m_bUsaSuoni;
	BOOL	m_bUsaColori;
	BOOL	m_bUsaIcone;
	CListBox	m_Lista;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc4PropPage1)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc4PropPage1)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck13();
	afx_msg void OnCheck3();
	afx_msg void OnCheck7();
	afx_msg void OnCheck6();
	afx_msg void OnSelchangeList1();
	afx_msg void OnButton1();
	afx_msg void OnButton2();
	afx_msg void OnButton3();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	void updateDaCheck();
	void entra_edata(int );
	void esci_edata(int );
	CStringList *blackList;
	CVidsendDoc4 *myParent;
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage2 dialog

class CVidsendDoc4PropPage2 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc4PropPage2)

public:
	int isInitialized;

// Construction
public:
	CVidsendDoc4PropPage2(CVidsendDoc4 *p=NULL);
	~CVidsendDoc4PropPage2();

// Dialog Data
	//{{AFX_DATA(CVidsendDoc4PropPage2)
	enum { IDD = IDD_CONF4_PAGE2 };
	CSpinButtonCtrl	m_MaxMessaggiSpin;
	CIPAddressCtrl	m_Proxy;
	CComboBox	m_ConnettiA;
	BOOL	m_Authenticate;
	BOOL	m_bProxy;
	BOOL	m_bConnetti;
	CString	m_User;
	CString	m_Pasw;
	CString	m_AuthWWW;
	BOOL	m_bSuoni;
	BOOL	m_bColore;
	UINT	m_MaxMessaggi;
	//}}AFX_DATA
	COLORREF m_colore;

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc4PropPage2)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc4PropPage2)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck1();
	afx_msg void OnCheck2();
	afx_msg void OnCheck3();
	afx_msg void OnButton1();
	afx_msg void OnCheck5();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	void updateDaCheck();
	CVidsendDoc4 *myParent;
	};


#ifdef _NEWMEET_MODE

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4PropPage0_NM dialog

class CVidsendDoc4PropPage0_NM : public CDialog
{
	DECLARE_DYNCREATE(CVidsendDoc4PropPage0_NM)

// Construction
public:
	CVidsendDoc4PropPage0_NM(CStringList *bl=NULL,CVidsendDoc4 *myParent=NULL);
	~CVidsendDoc4PropPage0_NM();

	void updateDaCheck();

public:

// Dialog Data
	//{{AFX_DATA(CVidsendDoc4PropPage0_NM)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF4_PAGE0_NM_ENG };
#else
	enum { IDD = IDD_CONF4_PAGE0_NM };
#endif
	CSpinButtonCtrl	m_MaxconnSpin;
	BOOL	m_NoPrivate;
	BOOL	m_Mostra_E_U;
	UINT	m_MaxConn;
	CIPAddressCtrl	m_Proxy;
	CComboBox	m_ConnettiA;
	BOOL	m_Authenticate;
	BOOL	m_bProxy;
	BOOL	m_bSuoni;
	BOOL	m_bColore;
	BOOL  m_NoFreeChat;
	UINT	m_MaxMessaggi;
	BOOL	m_One2One;
	BOOL	m_noOne2One;
	BOOL	m_bUsaSuoni;
	BOOL	m_bUsaColori;
	BOOL	m_bUsaIcone;
	CListBox	m_Lista;
	CString	m_IP;
	//}}AFX_DATA
	COLORREF m_colore;
	int eData;
	CStringList *blackList;
//	CIPAddressCtrl	m_IP;


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc4PropPage0_NM)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc4PropPage0_NM)
		afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
		virtual BOOL OnInitDialog();
		afx_msg void OnButton7();
		afx_msg void OnCheck3();
	afx_msg void OnSelchangeList1();
	afx_msg void OnButton1();
	afx_msg void OnButton2();
	afx_msg void OnButton3();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc4 *myParent;
	void entra_edata(int );
	void esci_edata(int );

	};

#endif


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc5PropPage0 dialog

class CVidsendDoc5PropPage0 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc5PropPage0)

// Construction
public:
	CVidsendDoc5PropPage0();
	~CVidsendDoc5PropPage0();

public:
	int isInitialized;

// Dialog Data
	//{{AFX_DATA(CVidsendDoc5PropPage0)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF5_PAGE0_ENG };
#else
	enum { IDD = IDD_CONF5_PAGE0 };
#endif
		// NOTE - ClassWizard will add data members here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc5PropPage0)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc5PropPage0)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc6PropPage0 dialog

class CVidsendDoc6PropPage0 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc6PropPage0)

// Construction
public:
	CVidsendDoc6PropPage0(CVidsendDoc6 *p=NULL);
	~CVidsendDoc6PropPage0();

public:
	int isInitialized;

// Dialog Data
	//{{AFX_DATA(CVidsendDoc6PropPage0)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF6_PAGE0_ENG };
#else
	enum { IDD = IDD_CONF6_PAGE0 };
#endif
	CSpinButtonCtrl	m_MaxHTMLconnSpin;
	CSpinButtonCtrl	m_MaxconnSpin;
	UINT	m_Maxconn;
	UINT	m_MaxHTMLconn;
	BOOL	m_AncheDirSrv;
	BOOL	m_FiltraIP;
	BOOL m_IPlookup;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc6PropPage0)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc6PropPage0)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc6 *myParent;

};


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc7PropPage0 dialog

class CVidsendDoc7PropPage0 : public CPropertyPage {
	DECLARE_DYNCREATE(CVidsendDoc7PropPage0)

// Construction
public:
	CVidsendDoc7PropPage0(CVidsendDoc7 *p=NULL);
	~CVidsendDoc7PropPage0();

public:
	int isInitialized;

// Dialog Data
	//{{AFX_DATA(CVidsendDoc7PropPage0)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_CONF7_PAGE0_ENG };
#else
	enum { IDD = IDD_CONF7_PAGE0 };
#endif
	CSpinButtonCtrl	m_MaxconnSpin;
	BOOL	m_bHTMLAccess;
	UINT	m_Maxconn;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc7PropPage0)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVidsendDoc7PropPage0)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc7 *myParent;

};


/////////////////////////////////////////////////////////////////////////////
// CDlgEnterURL dialog

class CDlgEnterURL : public CDialog {
public:
	enum {
		totStrings=20
		};
// Construction
public:
	CDlgEnterURL(BOOL mode=0,CDocument *pParent=NULL);   // standard constructor
public:
	CVidsendDoc *myParent;
	BOOL Mode;

// Dialog Data
	//{{AFX_DATA(CDlgEnterURL)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_ENTERURL_ENG };
#else
	enum { IDD = IDD_ENTERURL };
#endif
	CComboBox	m_URL;
	CString	m_URLstring;
	BOOL m_One2One;
	BOOL m_Video;
	BOOL m_Audio;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDlgEnterURL)
	public:
	virtual int DoModal(CString *,int );
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CString myURLs[20];

	// Generated message map functions
	//{{AFX_MSG(CDlgEnterURL)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CPaginaTestDlg dialog

class CPaginaTestDlg : public CDialog {
// Construction
public:
	CPaginaTestDlg(CVidsendDoc2 *pParent = NULL);   // standard constructor
	CPaginaTestDlg(CVidsendDoc22 *pParent = NULL);   // alternate constructor

// Dialog Data
	//{{AFX_DATA(CPaginaTestDlg)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_PAGINA_PROVA_ENG };
#else
	enum { IDD = IDD_PAGINA_PROVA };
#endif
	int		m_VideoImmagine;
	int		m_AudioFrequenza;
	BOOL	m_AudioSweep;
	BOOL	m_AudioIntervallato;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPaginaTestDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPaginaTestDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;
	CVidsendDoc22 *myParent2;
};


/////////////////////////////////////////////////////////////////////////////
// CVideoSrcDialog dialog

class CVideoSrcDialog : public CDialog {
	DECLARE_DYNCREATE(CVideoSrcDialog)

// Construction
public:
	CVideoSrcDialog(CVidsendDoc2 *myParent=NULL);
	~CVideoSrcDialog();
	void updateDaCheck();

public:
	WORD maxSchede;
// Dialog Data
	//{{AFX_DATA(CVideoSrcDialog)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_VIDEO_SRC_ENG };
#else
	enum { IDD = IDD_VIDEO_SRC };
#endif
	int		m_VideoSource;
	int		m_VideoSource2;
	BOOL	m_AlternaSource;
	int		m_Overlay;
	int		m_Schede;
	CString m_RTSPAddress;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CVideoSrcDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CVideoSrcDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnButton1();
	afx_msg void OnButton2();
	afx_msg void OnButton3();
	afx_msg void OnButton4();
	afx_msg void OnSelendokCombo1();
	afx_msg void OnRadio1();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CVidsendDoc2 *myParent;

	};

/////////////////////////////////////////////////////////////////////////////
// CTimedMessageBox dialog

class CTimedMessageBox : public CDialog
{
// Construction
public:
	CTimedMessageBox(CWnd* pParent = NULL);   // standard constructor
	int DoModal(CString ,CString ,DWORD ,DWORD showTime=3000);
	int DoModeless(CString ,CString ,DWORD);
	int EndModeless();

// Dialog Data
	//{{AFX_DATA(CTimedMessageBox)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_MESSAGEBOX_ENG };
#else
	enum { IDD = IDD_MESSAGEBOX };
#endif
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTimedMessageBox)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CString myText,myTitle;
	DWORD myFlags;
	BOOL exitMessageBox;
	DWORD elapsedTime;

	// Generated message map functions
	//{{AFX_MSG(CTimedMessageBox)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	int ContinueModal();
	};

/////////////////////////////////////////////////////////////////////////////
// CPasswordDlg dialog

class CPasswordDlg : public CDialog
{
// Construction
public:
	CPasswordDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CPasswordDlg)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_PASSWORD_ENG };
#else
	enum { IDD = IDD_PASSWORD };
#endif
	CString	m_Nome;
	CString	m_Pasw;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPasswordDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPasswordDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CInputBoxDlg dialog

class CInputBoxDlg : public CDialog
{
// Construction
public:
	CInputBoxDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CInputBoxDlg)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_INPUTBOX_ENG };
#else
	enum { IDD = IDD_INPUTBOX };
#endif
	CString	m_Text;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CInputBoxDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CInputBoxDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CSalvaFTPDlg dialog

class CSalvaFTPDlg : public CDialog {

// Construction
public:
	CSalvaFTPDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CSalvaFTPDlg)
#ifdef _LINGUA_INGLESE
	enum { IDD = IDD_SALVAFTP_ENG };
#else
	enum { IDD = IDD_SALVAFTP };
#endif
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSalvaFTPDlg)
	public:
	virtual int DoModal(BITMAPINFOHEADER *bi=NULL,BYTE *p=NULL);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	int returnValue;
protected:

	// Generated message map functions
	//{{AFX_MSG(CSalvaFTPDlg)
	afx_msg void OnPaint();
	afx_msg void OnOk2();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	BYTE *theBits;
	BITMAPINFOHEADER *theBitmap;
	};


#endif // !defined(AFX_VIDSENDDIALOG_H__F5FDF1C2_2277_11D4_94A8_00A0C9AFFE49__INCLUDED_)

/////////////////////////////////////////////////////////////////////////////
// CBrowserDlg dialog

class CBrowserDlg : public CDialog
{
// Construction
public:
	CBrowserDlg(const char *url=NULL,CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CBrowserDlg)
	enum { IDD = IDD_BROWSER };
	CWebBrowser2	m_Browser;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBrowserDlg)
	public:
	virtual BOOL Create(const char *,const RECT *rect,DWORD stile=0);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	BOOL updateOK;
protected:

	// Generated message map functions
	//{{AFX_MSG(CBrowserDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnTitleChangeExplorer1(LPCTSTR Text);
	afx_msg void OnDownloadCompleteExplorer1();
	afx_msg void OnClose();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnNavigateComplete2Explorer1(LPDISPATCH pDisp, VARIANT FAR* URL);
	afx_msg void OnDocumentCompleteExplorer1(LPDISPATCH pDisp, VARIANT FAR* URL);
	afx_msg void OnProgressChangeExplorer1(long Progress, long ProgressMax);
	DECLARE_EVENTSINK_MAP()
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	char myURL[256];
	};


/////////////////////////////////////////////////////////////////////////////
// CConfirmCamDlg dialog

#ifdef _CAMPARTY_MODE

class CConfirmCamDlg : public CDialog
{
// Construction
public:
	CConfirmCamDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CConfirmCamDlg)
	enum { IDD = IDD_CONFIRM_CAM };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfirmCamDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfirmCamDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif


/////////////////////////////////////////////////////////////////////////////
// CMyPrintDialog

UINT CALLBACK PrintHookFn(HWND , UINT , WPARAM , LONG );

class CMyPrintDialog : public CPrintDialog {
	DECLARE_DYNAMIC(CMyPrintDialog)

public:
	CMyPrintDialog(DWORD dwFlags = PD_ALLPAGES | PD_USEDEVMODECOPIES | PD_NOPAGENUMS
		| PD_HIDEPRINTTOFILE | PD_NOSELECTION,CWnd* pParentWnd = NULL);

// Dialog Data
	//{{AFX_DATA(CMyPrintDialog)
	enum { IDD = IDD_PRINT };
	int		m_Size;
	BOOL  m_SuperImpose;
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CMyPrintDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	public:
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CMyPrintDialog)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	};


/////////////////////////////////////////////////////////////////////////////
// CControlsWnd window

class CControlsWnd : public CWnd {
// Construction
public:
	CControlsWnd();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CControlsWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CControlsWnd();

	// Generated message map functions
protected:
	//{{AFX_MSG(CControlsWnd)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
/////////////////////////////////////////////////////////////////////////////
// CQualityBoxDlg dialog

class CQualityBoxDlg : public CDialog {
	
	enum { 
		SL_BOX   =1,             /* Draw a solid border around the rectangle  */
		SL_BLOCK  =2             /* Draw a solid rectangle                    */
		};

	enum { SL_EXTEND=256 };           /* Extend the current pattern                */

	enum { 
		SL_TYPE    =0x00FF,       /* Mask out everything but the type flags    */
		SL_SPECIAL =0xFF00       /* Mask out everything but the special flags */
		};

// Construction
public:
	CQualityBoxDlg(RECT *rc=NULL, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CQualityBoxDlg)
	enum { IDD = IDD_QUALITYBOX };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CQualityBoxDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	RECT SelectedRect;
protected:
//	bool g_MovingMainWnd;
//	POINT g_OrigCursorPos;
//	POINT g_OrigWndPos;
	bool bTrack;
	int Shape;                  /* Shape to use for rectangle */
	BOOL RetainShape;              /* Retain or destroy shape    */


	int StartSelection(POINT, LPRECT, int);
	int UpdateSelection(POINT, LPRECT, int);
	int EndSelection(POINT ptCurrent,LPRECT lpSelectRect);
	int ClearSelection(LPRECT,int);

	// Generated message map functions
	//{{AFX_MSG(CQualityBoxDlg)
	afx_msg void OnTimer(UINT nIDEvent);
	virtual BOOL OnInitDialog();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd *pWnd);
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};



/////////////////////////////////////////////////////////////////////////////
// CImpostaEffettiSonoriDlg2 dialog

class CImpostaEffettiSonoriDlg;

class CImpostaEffettiSonoriDlg2 : public CDialog {
public:
	CImpostaEffettiSonoriDlg *m_Parent;
// Construction
public:
	CImpostaEffettiSonoriDlg2(CWnd* pParent);   // standard constructor
	void updateFields();
	void readFields();

// Dialog Data
	//{{AFX_DATA(CImpostaEffettiSonoriDlg2)
	enum { IDD = IDD_IMPOSTA_EFFETTISONORI2 };
	CString	m_Wav1;
	CString	m_Wav2;
	CString	m_Wav3;
	CString	m_Wav4;
	CString	m_Wav5;
	CString	m_Wav6;
	CString	m_Wav7;
	CString	m_Wav8;
	CString	m_Wav9;
	CString	m_Wav10;
	CString	m_Wav11;
	CString	m_Wav12;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CImpostaEffettiSonoriDlg2)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CImpostaEffettiSonoriDlg2)
	afx_msg void OnButtonSfoglia(UINT);
	afx_msg void OnButtonPlay(UINT);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CImpostaEffettiSonoriDlg dialog

class CImpostaEffettiSonoriDlg : public CDialog {
// Construction
public:
	CImpostaEffettiSonoriDlg2 *myDlg;
	CString m_Wavs[3][12];
	CVidsendView22 *m_Parent;

public:
	CImpostaEffettiSonoriDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CImpostaEffettiSonoriDlg)
	enum { IDD = IDD_IMPOSTA_EFFETTISONORI };
	CTabCtrl	m_Tab;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CImpostaEffettiSonoriDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

public:
	virtual void OnOK();
	virtual void OnCancel();

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CImpostaEffettiSonoriDlg)
	afx_msg void OnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSelchangingTab1(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.


