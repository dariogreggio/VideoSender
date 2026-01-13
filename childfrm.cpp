// ChildFrm.cpp : implementation of the CChildFrame class
//

#include "stdafx.h"
#include "vidsend.h"
#include "vidsendview.h"
#include "vidsenddoc.h"
#include <digitaltext.h>
#include "vidsenddialog.h"
#include "vidsendlog.h"

#include "ChildFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWnd)

BEGIN_MESSAGE_MAP(CChildFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CChildFrame)
	ON_WM_SIZE()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame construction/destruction

CChildFrame::CChildFrame() {

	iconOff=theApp.LoadIcon(IDI_LEDOFF);
	iconOn=theApp.LoadIcon(IDI_LEDON);
	iconSpk=theApp.LoadIcon(IDI_SPEAKER);
	iconSpkOff=theApp.LoadIcon(IDI_SPEAKEROFF);
	m_VUMeter=new CVUMeter;
	m_ControlsWnd=new CControlsWnd;
	}

CChildFrame::~CChildFrame() {

	delete m_ControlsWnd;
	delete m_VUMeter;
	}

BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs) {

	cs.cx=340;
	cs.cy=300;
	if( !CMDIChildWnd::PreCreateWindow(cs) )
		return FALSE;

	return TRUE;
	}

static UINT indicators[] = {
	ID_SEPARATOR,
	ID_INDICATOR2,
	ID_INDICATOR3,
	ID_INDICATOR3
	};

int CChildFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	RECT myRect;

	if(CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if(!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,sizeof(indicators)/sizeof(UINT)))	{
		return -1;      // fail to create
		}

	myRect.top=3;
	myRect.left=24;
	myRect.bottom=17;
	myRect.right=myRect.left+54-1;

	//TOGLIERE se non trasmette audio!!
	if(!m_VUMeter->Create("Audio",0 /*CVUMeter::dotOrBar */ /*CVUMeter::digitalOrAnalog*/  /*| WS_VISIBLE | WS_CHILD*/,myRect,
		&m_wndStatusBar,-1,0,40))	{
		return -1;      // fail to create
		}

	return 0;
	}


/////////////////////////////////////////////////////////////////////////////
// CChildFrame diagnostics

#ifdef _DEBUG
void CChildFrame::AssertValid() const {
	CMDIChildWnd::AssertValid();
	}

void CChildFrame::Dump(CDumpContext& dc) const {
	CMDIChildWnd::Dump(dc);
	}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CChildFrame message handlers

void CChildFrame::OnSize(UINT nType, int cx, int cy) {
	CVidsendDoc *d=(CVidsendDoc *)GetActiveDocument();
	int x,cx2,cy2;
	
	cx2=cx+GetSystemMetrics(SM_CXEDGE)*4;		// perche?? con valori diversi, fa il giochino del rettangolo che rimpicciolisce pian piano...
	cy2=cy+GetSystemMetrics(SM_CYEDGE)*4+38;		// aggiungere bordi, statusbar
	if(d && d->Opzioni & CVidsendDoc::fmt4_3) {
		cy2=(cx*3)/4 +GetSystemMetrics(SM_CYEDGE)*4+38;		// aggiungere bordi, statusbar
		SetWindowPos(NULL,0,0,cx2,cy2,SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOCOPYBITS);
		}

	if(m_wndStatusBar && !IsIconic()) {
		x=cx2-GetSystemMetrics(SM_CYVSCROLL)*2-GetSystemMetrics(SM_CYEDGE)*4;	// non va bene, ma non trovo i valori giusti...
		m_wndStatusBar.SetPaneInfo(0,ID_INDICATOR2,SBPS_NORMAL,(x-(22*2))/2);
		m_wndStatusBar.SetPaneInfo(1,ID_INDICATOR2,SBPS_NORMAL,(x-(22*2))/2);
		m_wndStatusBar.SetPaneInfo(2,ID_INDICATOR3,SBPS_NORMAL /*| SBPS_STRETCH*/,22);
		m_wndStatusBar.SetPaneInfo(3,ID_INDICATOR3,SBPS_NORMAL,22);
		
		}
	
//	if(m_VUMeter)
//		m_VUMeter->SetWindowText(0);

	CMDIChildWnd::OnSize(nType, cx, cy);
	}

void CChildFrame::setStatusIcons() {
	CVidsendDoc *d=(CVidsendDoc *)GetActiveDocument();
	CVidsendView *w=(CVidsendView *)GetActiveView();
	
	if(m_wndStatusBar && d && w) {
		m_wndStatusBar.GetStatusBarCtrl().SetIcon(2,d->bPaused ? iconOff : iconOn);
		m_wndStatusBar.GetStatusBarCtrl().SetIcon(3,w->streamInfo.wf.wf.wFormatTag ? iconSpk : iconSpkOff);

		}
	}

BOOL CChildFrame::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
	CVidsendDoc *d=(CVidsendDoc *)GetActiveDocument();
	CVidsendView *w=(CVidsendView *)GetActiveView();
	TBNOTIFY *p=(TBNOTIFY *)lParam;
	
	if(d) {
		if(p->hdr.hwndFrom == m_wndStatusBar.m_hWnd) {
			switch(p->hdr.code) {
				case NM_CLICK:
					switch(p->iItem) {
						case 2:
							d->bPaused = !d->bPaused;
							break;
						case 3:
							if(w)
								w->PostMessage(WM_COMMAND,ID_VISUALIZZA_VOLUME,0);
							break;
						}
					setStatusIcons();
					break;
				}
			}
		}
	return CMDIChildWnd::OnNotify(wParam, lParam, pResult);
	}


/////////////////////////////////////////////////////////////////////////////
// CChildFrame2

IMPLEMENT_DYNCREATE(CChildFrame2, CMDIChildWnd)

CChildFrame2::CChildFrame2() {

	iconOff=theApp.LoadIcon(IDI_LEDOFF);
	iconOn=theApp.LoadIcon(IDI_LEDON);
	iconSpkOn=theApp.LoadIcon(IDI_SPEAKER);
	iconSpkOff=theApp.LoadIcon(IDI_SPEAKEROFF);
	iconPlay=theApp.LoadIcon(IDI_PLAY);
	iconPause=theApp.LoadIcon(IDI_PAUSE);
#ifdef _NEWMEET_MODE
	iconRecOn=theApp.LoadIcon(IDI_RECORD_NM);
#else
	iconRecOn=theApp.LoadIcon(IDI_RECORD);
#endif
#ifdef _NEWMEET_MODE
	iconRecOff=theApp.LoadIcon(IDI_PAUSE_NM);
#else
	iconRecOff=theApp.LoadIcon(IDI_PAUSE);
#endif
	iconCamera=theApp.LoadIcon(IDI_CAMERA);
	}

CChildFrame2::~CChildFrame2() {
	}


BEGIN_MESSAGE_MAP(CChildFrame2, CMDIChildWnd)
	//{{AFX_MSG_MAP(CChildFrame2)
	ON_WM_MOVE()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
	ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame2 message handlers
static UINT indicators2[] = {
	ID_INDICATOR3,
	ID_INDICATOR3,
#ifndef _CAMPARTY_MODE
	ID_INDICATOR3,
	ID_INDICATOR3,
	ID_INDICATOR3
#endif
	};

int CChildFrame2::OnCreate(LPCREATESTRUCT lpCreateStruct) {

	if(CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if(!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators2,
		  sizeof(indicators2)/sizeof(UINT)))	{
		return -1;      // fail to create
		}
	setStatusIcons();

	return 0;
	}

void CChildFrame2::OnSize(UINT nType, int cx, int cy) {
	int x;
	
	if(m_wndStatusBar && !IsIconic()) {
		x=cx-GetSystemMetrics(SM_CYVSCROLL)*2-GetSystemMetrics(SM_CYEDGE)*4;	// non va bene, ma non trovo i valori giusti...
		m_wndStatusBar.SetPaneInfo(0,ID_INDICATOR3,SBPS_NORMAL,18);
#ifndef _CAMPARTY_MODE
		m_wndStatusBar.SetPaneInfo(1,ID_INDICATOR3,SBPS_NORMAL,18);
		m_wndStatusBar.SetPaneInfo(2,ID_INDICATOR3,SBPS_NORMAL,18);
		m_wndStatusBar.SetPaneInfo(3,ID_INDICATOR3,SBPS_NORMAL,18);
		m_wndStatusBar.SetPaneInfo(4,ID_INDICATOR3,SBPS_NORMAL | SBPS_STRETCH,100);
#else
		m_wndStatusBar.SetPaneInfo(1,ID_INDICATOR3,SBPS_NORMAL | SBPS_STRETCH,100);
#endif
		setStatusIcons();
		}
	
	CMDIChildWnd::OnSize(nType, cx, cy);
	}

void CChildFrame2::OnMove(int x, int y) {
	RECT r;
	CVidsendDoc2 *d=(CVidsendDoc2 *)GetActiveDocument();
	
	CMDIChildWnd::OnMove(x, y);
	if(d) {
		GetClientRect(&r);
		if(d->theTV)
			d->theTV->Resize(&r);
		}

	}

void CChildFrame2::OnGetMinMaxInfo(MINMAXINFO *lpMMI) {
	CVidsendDoc2 *d=(CVidsendDoc2 *)GetActiveDocument();

	if(d) {
		if(d->theTV) {
			lpMMI->ptMaxSize.x=lpMMI->ptMinTrackSize.x=lpMMI->ptMaxTrackSize.x=d->theTV->GetXSize()+GetSystemMetrics(SM_CXEDGE)*3;
#ifdef _NEWMEET_MODE
			lpMMI->ptMaxSize.y=lpMMI->ptMinTrackSize.y=lpMMI->ptMaxTrackSize.y=d->theTV->GetYSize()+GetSystemMetrics(SM_CYEDGE)*5+GetSystemMetrics(SM_CYCAPTION) /*status bar...*/ -1;
#else
			lpMMI->ptMaxSize.y=lpMMI->ptMinTrackSize.y=lpMMI->ptMaxTrackSize.y=d->theTV->GetYSize()+GetSystemMetrics(SM_CYEDGE)*5+GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYCAPTION) /*status bar...*/;
#endif
			}
		}
	}


void CChildFrame2::setStatusIcons(CVidsendDoc2 *d) {		// passandogli il Doc2 POTREI impostare le icone subito dopo la creazione della finestra live (laddove altrimenti GetActiveDoc... restituirebbe ancora 0!)
	CString S;

	if(!d) 
		d=(CVidsendDoc2 *)GetActiveDocument();
	
	if(m_wndStatusBar && ::IsWindow(m_hWnd) && !IsIconic()) {
		if(d) {
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(0,d->bPaused ? iconPause : iconPlay);
			switch(d->trasmMode) {
				case 1:
					S="playback in corso...";
					break;
				case 2:
					S="suono di test...";
					break;
				case 0:
					S="live";
					break;
				}
			if(!(d->Opzioni & (CVidsendDoc2::maySendAudio | CVidsendDoc2::maySendVideo)))
				S+=" (disabilitato)";
#ifndef _CAMPARTY_MODE
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(1,d->bAudio ? iconSpkOn : iconSpkOff);
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(2,d->theTV && d->theTV->isRecordingVideo() ? iconRecOn : iconRecOff);
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(3,d->myID ? iconCamera : NULL);
			m_wndStatusBar.SetPaneText(4,S,TRUE);
#else
			m_wndStatusBar.SetPaneText(1,S,TRUE);
#endif
			}
		else {
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(0,iconPlay);
#ifndef _CAMPARTY_MODE
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(1,iconSpkOn);
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(2,iconRecOff);
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(3,NULL);
			m_wndStatusBar.SetPaneText(4,S,TRUE);
#else
			m_wndStatusBar.SetPaneText(1,S,TRUE);
#endif
			}
		}
	}

BOOL CChildFrame2::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
	CVidsendDoc2 *d=(CVidsendDoc2 *)GetActiveDocument();
	TBNOTIFY *p=(TBNOTIFY *)lParam;
	
	if(d) {
		if(p->hdr.hwndFrom == m_wndStatusBar.m_hWnd) {
			switch(p->hdr.code) {
				case NM_CLICK:
					switch(p->iItem) {
						case 0:
							d->bPaused = !d->bPaused;
							break;
						case 1:
#ifndef _CAMPARTY_MODE
							if(d->Opzioni & CVidsendDoc2::maySendAudio) {
								d->bAudio = !d->bAudio;
								}
#endif
							break;
						case 2:
#ifndef _CAMPARTY_MODE
							if(d->getView())
								d->getView()->PostMessage(MAKELONG(WM_COMMAND,0),ID_FILE_SAVE_VIDEO,0);
#endif
							break;
						case 3:
#ifndef _CAMPARTY_MODE
							if(d->getView())
								d->getView()->PostMessage(MAKELONG(WM_COMMAND,0),ID_FILE_SAVE_FOTOGRAMMA2,0);
#endif
							break;
						}
					setStatusIcons();
					break;
				}
			}
		}
	return CMDIChildWnd::OnNotify(wParam, lParam, pResult);
	}

BOOL CChildFrame2::PreCreateWindow(CREATESTRUCT& cs) {
	
#ifdef _NEWMEET_MODE
	cs.style &= ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | /*WS_CLOSEBOX boh? |*/ WS_SYSMENU);
#endif
	return CMDIChildWnd::PreCreateWindow(cs);
	}

void CChildFrame2::OnClose() {
#ifndef _NEWMEET_MODE
	CMDIChildWnd::OnClose();
#endif
	
	}


/////////////////////////////////////////////////////////////////////////////
// CChildFrame22

IMPLEMENT_DYNCREATE(CChildFrame22, CMDIChildWnd)

CChildFrame22::CChildFrame22() {

	iconOff=theApp.LoadIcon(IDI_LEDOFF);
	iconOn=theApp.LoadIcon(IDI_LEDON);
	iconSpkOn=theApp.LoadIcon(IDI_SPEAKER);
	iconSpkOff=theApp.LoadIcon(IDI_SPEAKEROFF);
	iconPlay=theApp.LoadIcon(IDI_PLAY);
	iconPause=theApp.LoadIcon(IDI_PAUSE);
#ifdef _NEWMEET_MODE
	iconRecOn=theApp.LoadIcon(IDI_RECORD_NM);
#else
	iconRecOn=theApp.LoadIcon(IDI_RECORD);
#endif
#ifdef _NEWMEET_MODE
	iconRecOff=theApp.LoadIcon(IDI_PAUSE_NM);
#else
	iconRecOff=theApp.LoadIcon(IDI_PAUSE);
#endif
	}

CChildFrame22::~CChildFrame22() {
	}


BEGIN_MESSAGE_MAP(CChildFrame22, CMDIChildWnd)
	//{{AFX_MSG_MAP(CChildFrame22)
	ON_WM_MOVE()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
	ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame2 message handlers
static UINT indicators22[] = {
	ID_INDICATOR3,
	ID_INDICATOR3,
	ID_INDICATOR3,
	ID_INDICATOR3
	};

int CChildFrame22::OnCreate(LPCREATESTRUCT lpCreateStruct) {

	if(CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if(!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators22,
		  sizeof(indicators22)/sizeof(UINT)))	{
		return -1;      // fail to create
		}
	setStatusIcons();

	return 0;
	}

void CChildFrame22::OnSize(UINT nType, int cx, int cy) {
	int x;
	
	if(m_wndStatusBar && !IsIconic()) {
		x=cx-GetSystemMetrics(SM_CYVSCROLL)*2-GetSystemMetrics(SM_CYEDGE)*4;	// non va bene, ma non trovo i valori giusti...
		m_wndStatusBar.SetPaneInfo(0,ID_INDICATOR3,SBPS_NORMAL,18);
		m_wndStatusBar.SetPaneInfo(1,ID_INDICATOR3,SBPS_NORMAL,18);
		m_wndStatusBar.SetPaneInfo(2,ID_INDICATOR3,SBPS_NORMAL,18);
		m_wndStatusBar.SetPaneInfo(3,ID_INDICATOR3,SBPS_NORMAL | SBPS_STRETCH,100);
		setStatusIcons();
		}
	
	CMDIChildWnd::OnSize(nType, cx, cy);
	}

void CChildFrame22::OnMove(int x, int y) {
	RECT r;
	CVidsendDoc22 *d=(CVidsendDoc22 *)GetActiveDocument();
	
	CMDIChildWnd::OnMove(x, y);
	if(d) {
		GetClientRect(&r);
		}

	}

void CChildFrame22::OnGetMinMaxInfo(MINMAXINFO *lpMMI) {
	CVidsendDoc22 *d=(CVidsendDoc22 *)GetActiveDocument();

	if(d) {
		}
	}


void CChildFrame22::setStatusIcons(CVidsendDoc22 *d) {		// passandogli il Doc2 POTREI impostare le icone subito dopo la creazione della finestra live (laddove altrimenti GetActiveDoc... restituirebbe ancora 0!)
	CString S;

	if(!d) 
		d=(CVidsendDoc22 *)GetActiveDocument();
	
	if(m_wndStatusBar && ::IsWindow(m_hWnd) && !IsIconic()) {
		if(d) {
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(0,d->bPaused ? iconPause : iconPlay);
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(1,d->bAudio ? iconSpkOn : iconSpkOff);
			switch(d->trasmMode) {
				case 1:
					S="playback in corso...";
					if(!d->psAudio)
						S+=" (file non trovato)";
					break;
				case 2:
					S="suono di test...";
					break;
				case 0:
					S="live";
					break;
				}
			if(!(d->Opzioni & CVidsendDoc22::maySendAudio))
				S+=" (disabilitato)";
			m_wndStatusBar.SetPaneText(3,S,TRUE);
			}
		else {
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(0,iconPlay);
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(1,iconSpkOn);
			m_wndStatusBar.GetStatusBarCtrl().SetIcon(2,iconRecOff);
			m_wndStatusBar.SetPaneText(3,S,TRUE);
			}
		}
	}

BOOL CChildFrame22::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
	CVidsendDoc22 *d=(CVidsendDoc22 *)GetActiveDocument();
	TBNOTIFY *p=(TBNOTIFY *)lParam;
	
	if(d) {
		if(p->hdr.hwndFrom == m_wndStatusBar.m_hWnd) {
			switch(p->hdr.code) {
				case NM_CLICK:
					switch(p->iItem) {
						case 0:
							d->bPaused = !d->bPaused;
							((CVidsendView22*)d->getView())->buttonS->SetCheck(!d->bPaused);
							break;
						case 1:
							if(d->Opzioni & CVidsendDoc22::maySendAudio) {
								d->bAudio = !d->bAudio;
								}
							break;
						case 2:
							if(d->getView())
								d->getView()->PostMessage(MAKELONG(WM_COMMAND,0),ID_FILE_SAVE_VIDEO,0);
							break;
						}
					setStatusIcons();
					break;
				}
			}
		}
	return CMDIChildWnd::OnNotify(wParam, lParam, pResult);
	}

BOOL CChildFrame22::PreCreateWindow(CREATESTRUCT& cs) {
	
	return CMDIChildWnd::PreCreateWindow(cs);
	}

void CChildFrame22::OnClose() {

	CMDIChildWnd::OnClose();
	}



/////////////////////////////////////////////////////////////////////////////
// CChildFrame3

IMPLEMENT_DYNCREATE(CChildFrame3, CMDIChildWnd)

CChildFrame3::CChildFrame3()
{
}

CChildFrame3::~CChildFrame3()
{
}


BEGIN_MESSAGE_MAP(CChildFrame3, CMDIChildWnd)
	//{{AFX_MSG_MAP(CChildFrame3)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame3 message handlers

BOOL CChildFrame3::PreCreateWindow(CREATESTRUCT& cs) {
	
#ifdef _NEWMEET_MODE
	cs.style &= ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | /*WS_CLOSEBOX boh? |*/ WS_SYSMENU);
#endif
	return CMDIChildWnd::PreCreateWindow(cs);
	}

void CChildFrame3::OnClose() {
#ifndef _NEWMEET_MODE
	CMDIChildWnd::OnClose();
#endif
	
	}


/////////////////////////////////////////////////////////////////////////////
// CChildFrame4

IMPLEMENT_DYNCREATE(CChildFrame4, CMDIChildWnd)

CChildFrame4::CChildFrame4() {
	iconOff=theApp.LoadIcon(IDI_LEDOFF);
	iconOn=theApp.LoadIcon(IDI_LEDON);
	}

CChildFrame4::~CChildFrame4()
{
}


BEGIN_MESSAGE_MAP(CChildFrame4, CMDIChildWnd)
	//{{AFX_MSG_MAP(CChildFrame4)
	ON_WM_CREATE()
	ON_WM_GETMINMAXINFO()
	ON_WM_SIZE()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame4 message handlers

static UINT indicators4[] = {
	ID_SEPARATOR,
	ID_INDICATOR3,
	ID_INDICATOR3
	};


int CChildFrame4::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	int i;

	if(CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if(!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators4,
		  sizeof(indicators4)/sizeof(UINT)))	{
		return -1;      // fail to create
		}

	m_wndStatusBar.SetPaneInfo(0,ID_SEPARATOR,SBPS_NORMAL | SBPS_STRETCH,100);
	m_wndStatusBar.SetPaneInfo(1,ID_INDICATOR3,SBPS_NORMAL,18);
	m_wndStatusBar.SetPaneInfo(2,ID_INDICATOR3,SBPS_NORMAL,18);

	return 0;
	}

void CChildFrame4::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI) {
	
	lpMMI->ptMinTrackSize.x=250;
	lpMMI->ptMinTrackSize.y=150;
	}


void CChildFrame4::OnSize(UINT nType, int cx, int cy) {
	int x;
	
	if(m_wndStatusBar && !IsIconic()) {
		x=cx-GetSystemMetrics(SM_CYVSCROLL)*2-GetSystemMetrics(SM_CYEDGE)*4;	// non va bene, ma non trovo i valori giusti...
		m_wndStatusBar.SetPaneInfo(0,ID_SEPARATOR,SBPS_NORMAL | SBPS_STRETCH,100);
		m_wndStatusBar.SetPaneInfo(1,ID_INDICATOR3,SBPS_NORMAL,18);
		m_wndStatusBar.SetPaneInfo(2,ID_INDICATOR3,SBPS_NORMAL,18);
		}
	
	CMDIChildWnd::OnSize(nType, cx, cy);
	}


BOOL CChildFrame4::PreCreateWindow(CREATESTRUCT& cs) {

#ifdef _NEWMEET_MODE
	cs.style &= ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | /*WS_CLOSEBOX boh? |*/ WS_SYSMENU);
#endif
	return CMDIChildWnd::PreCreateWindow(cs);
	}

void CChildFrame4::OnClose() {
#ifndef _NEWMEET_MODE
	CMDIChildWnd::OnClose();
#endif
	}

/////////////////////////////////////////////////////////////////////////////
// CChildFrame5

IMPLEMENT_DYNCREATE(CChildFrame5, CMDIChildWnd)

CChildFrame5::CChildFrame5()
{
}

CChildFrame5::~CChildFrame5()
{
}


BEGIN_MESSAGE_MAP(CChildFrame5, CMDIChildWnd)
	//{{AFX_MSG_MAP(CChildFrame5)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame5 message handlers

static UINT indicators3[] = {
	ID_INDICATOR3,
	ID_INDICATOR3,
	ID_SEPARATOR,
	ID_INDICATOR3
	};


int CChildFrame5::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	int i;

	if(CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if(!m_wndToolBar.CreateEx(this) ||
		!m_wndToolBar.LoadToolBar(IDR_BROWSERBAR))	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
		}
	if(!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators3,
		  sizeof(indicators3)/sizeof(UINT)))	{
		return -1;      // fail to create
		}
	if(!m_wndDlgBar.Create(this, IDD_NAVBAR, 
		CBRS_ALIGN_TOP, AFX_IDW_DIALOGBAR))	{
		TRACE0("Failed to create dialogbar\n");
		return -1;		// fail to create
		}
//	PER INSERIRE UNA DIALOG BAR FISSA IN CIMA ALLA FINESTRA (STILE WORD O EXPLORER)

	if(!m_wndReBar.Create(this) ||
		!m_wndReBar.AddBar(&m_wndToolBar) ||
		!m_wndReBar.AddBar(&m_wndDlgBar))	{
		TRACE0("Failed to create rebar\n");
		return -1;      // fail to create
		}

	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |	CBRS_TOOLTIPS | CBRS_FLYBY);

	m_wndStatusBar.SetPaneInfo(0,ID_INDICATOR3,SBPS_NORMAL,18);
	m_wndStatusBar.SetPaneInfo(1,ID_INDICATOR3,SBPS_NORMAL,18);
	m_wndStatusBar.SetPaneInfo(2,ID_SEPARATOR,SBPS_NORMAL | SBPS_STRETCH,100);
	m_wndStatusBar.SetPaneInfo(3,ID_INDICATOR3,SBPS_NORMAL,18);

	return 0;
	}




BOOL CChildFrame5::OnCommand(WPARAM wParam, LPARAM lParam) {
	int i;
	CString S;
	
	switch(LOWORD(wParam)) {
		case IDC_COMBO1:
			switch(HIWORD(wParam)) {
				case CBN_CLOSEUP:
					i=((CComboBox *)m_wndDlgBar.GetDlgItem(IDC_COMBO1))->GetCurSel();
					if(i != CB_ERR) {
						((CComboBox *)m_wndDlgBar.GetDlgItem(IDC_COMBO1))->GetLBText(i,S);
						if(!S.IsEmpty())
							((CVidsendView5 *)GetActiveView())->Navigate(S);
						}
					break;
				case CBN_EDITCHANGE:
					break;
				}
			break;
		case IDOK:
		case ID_NAVIGAZIONE_VAI:
			((CComboBox *)m_wndDlgBar.GetDlgItem(IDC_COMBO1))->GetWindowText(S);
			if(!S.IsEmpty())
				((CVidsendView5 *)GetActiveView())->Navigate(S);
			break;
		}
	return CMDIChildWnd::OnCommand(wParam, lParam);
	}



