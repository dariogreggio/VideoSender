// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "vidsend.h"

#include "MainFrm.h"
#include "vidsendlog.h"
#include "vidsenddoc.h"

#include <cjpeg2.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_DROPFILES()
	ON_WM_MOVE()
	ON_WM_ENDSESSION()
	ON_WM_CLOSE()
//	ON_WM_PAINT()
//	ON_WM_ERASEBKGND()
	ON_WM_NCPAINT()
	ON_WM_CONTEXTMENU()
	ON_WM_QUERYENDSESSION()
	ON_COMMAND(ID_WINDOW_LAYOUT1, OnWindowLayout1)
	ON_COMMAND(ID_WINDOW_LAYOUT2, OnWindowLayout2)
	ON_COMMAND(ID_WINDOW_LAYOUT3, OnWindowLayout3)
	ON_COMMAND(ID_WINDOW_LAYOUT_ADATTA, OnWindowLayoutAdatta)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_LAYOUT_ADATTA, OnUpdateWindowLayoutAdatta)
	ON_WM_TIMER()
	ON_COMMAND(ID_VISUALIZZA_TOOLBAR, OnVisualizzaToolbar)
	ON_COMMAND(ID_VISUALIZZA_STATUS_BAR, OnVisualizzaStatusBar)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_TOOLBAR, OnUpdateVisualizzaToolbar)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_STATUS_BAR, OnUpdateVisualizzaStatusBar)
	ON_COMMAND(ID_WINDOW_LAYOUT4, OnWindowLayout4)
	//}}AFX_MSG_MAP
	// Global help commands
	ON_COMMAND(ID_HELP_FINDER, CMDIFrameWnd::OnHelpFinder)
	ON_COMMAND(ID_HELP, CMDIFrameWnd::OnHelp)
	ON_COMMAND(ID_CONTEXT_HELP, CMDIFrameWnd::OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP, CMDIFrameWnd::OnHelpFinder)
	ON_MESSAGE(WM_UPDATE_PANE,OnUpdatePane)
	ON_MESSAGE(WM_CLOSE_CHILD,OnCloseChild)
END_MESSAGE_MAP()

static UINT indicators[] = {
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,           // 2nd status line indicator
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
	};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame() {
	}

CMainFrame::~CMainFrame() {
	}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {

	if(CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if(!m_wndToolBar.CreateEx(this) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
		}

	if(!m_wndReBar.Create(this) ||
		!m_wndReBar.AddBar(&m_wndToolBar) )	{
		TRACE0("Failed to create rebar\n");
		return -1;      // fail to create
		}

	if(!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,sizeof(indicators)/sizeof(UINT)))	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
		}
	m_wndStatusBar.SetPaneInfo(0,ID_SEPARATOR,SBPS_NORMAL,440);
	m_wndStatusBar.SetPaneInfo(1,ID_SEPARATOR,SBPS_NORMAL | SBPS_STRETCH,330);
	m_wndStatusBar.SetPaneInfo(2,ID_INDICATOR_NUM,SBPS_NORMAL,26);
	m_wndStatusBar.SetPaneInfo(3,ID_INDICATOR_SCRL,SBPS_NORMAL,26);

	// TODO: Remove this if you don't want tool tips
	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |	CBRS_TOOLTIPS | CBRS_FLYBY);

#ifdef _CAMPARTY_MODE
	SetWindowText("Camparty Server");
	theApp.m_hIcon = AfxGetApp()->LoadIcon(IDI_CAMPARTY);
	SetIcon(theApp.m_hIcon,FALSE);		// tutto questo serve x cambiare l'icona del programma...
	SetIcon(theApp.m_hIcon,TRUE);			// ...e soprattutto l'icona piccola! (altrimenti, in teoria, lo fa il framework)...
#endif

	SetTimer(1,5000,NULL);
	return 0;
	}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
	char myBuf[64];

	cs.style &= ~FWS_ADDTOTITLE;
	CRect rc;
	int n,n2;

	if(theApp.Opzioni & CVidsendApp::saveLayout) {			// credo inutile dato il nuovo flag in winappex
		theApp.LoadWindowPlacement(rc,n,n2);
		if(IsRectEmpty(&rc)) {
			rc.left=rc.top=10;
			rc.right=600; rc.bottom=400;
			}
		cs.x=rc.left;
		cs.y=rc.top;
		cs.cx=rc.right;
		cs.cy=rc.bottom;
		cs.cx-=cs.x;
		cs.cy-=cs.y;
		}
	if( !CMDIFrameWnd::PreCreateWindow(cs) )
		return FALSE;

	return TRUE;
	}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const {
	CMDIFrameWnd::AssertValid();
	}

void CMainFrame::Dump(CDumpContext& dc) const {
	CMDIFrameWnd::Dump(dc);
	}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

afx_msg LRESULT CMainFrame::OnUpdatePane(WPARAM wParam, LPARAM lParam) {
	int i;
	char *s=(char *)lParam;
	// serve per poter scrivere nella status pane da altri task...

	if(*s)
		i=m_wndStatusBar.SetPaneText(1,s,TRUE);
	GlobalFree(s);
	return i;
	}

afx_msg LRESULT CMainFrame::OnCloseChild(WPARAM wParam, LPARAM lParam) {
	int i;
	// chiude un documento/finestra figlia

	switch(wParam) {
		case 1:
			for(i=0; i<32; i++) {
				if(theApp.aClient[i] == (CVidsendDoc *)lParam) {
					theApp.aClient[i]=NULL;
					break;
					}
				}
			break;
		case 5:
			for(i=0; i<16; i++) {
				if(theApp.aBrowser[i] == (CVidsendDoc5 *)lParam) {
					theApp.aBrowser[i]=NULL;
					break;
					}
				}
			break;
		}
	return i;
	}

void CMainFrame::OnDropFiles(HDROP hDropInfo) {
	char myBuf[256];
	
	DragQueryFile(hDropInfo,0,myBuf,256);

	DragFinish(hDropInfo);
//	CMDIFrameWnd::OnDropFiles(hDropInfo);
	}


void CMainFrame::OnMove(int x, int y) {
	RECT r;

	CMDIFrameWnd::OnMove(x, y);
	if(theApp.theServer) {
		GetClientRect(&r);
		if(theApp.theServer->theTV)
			theApp.theServer->theTV->Resize(&r);
		}
	
	}

BOOL CMainFrame::DestroyWindow() {
	RECT rc;
	char myBuf[64];
	
/*	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		GetWindowRect(&rc);
		wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
		theApp.prStore->WriteProfileVariabileString(IDS_COORDINATE,myBuf);
		}*/
	
	/* v. BestaudioPlayer, là va (SDI vs. MDI)
	CRect r;

	if(theApp.Opzioni & CVidsendApp::saveLayout) {
//	if(theApp.m_bLoadWindowPlacement) {
		GetWindowRect(&r);
		theApp.StoreWindowPlacement(r,0,IsIconic());
		}
	*/

#ifdef _CAMPARTY_MODE
	DestroyIcon(theApp.m_hIcon);
#endif

	theApp.OnClosingMainFrame();

	return CMDIFrameWnd::DestroyWindow();
	}


void CMainFrame::OnEndSession(BOOL bEnding) {
	CMDIFrameWnd::OnEndSession(bEnding);
	
// puo' servire perche' EndSession non provoca ExitInstance... sembra!
	}

void CMainFrame::OnClose() {
	int i=TRUE;

	if(theApp.Opzioni & CVidsendApp::saveLayout) {	// in destroyWindow sarebbe troppo tardi!
		theApp.OpzioniOpenWin &= 0xff80;
		if(theApp.aClient[0])
			theApp.OpzioniOpenWin |= 1;
		if(theApp.theServer)
			theApp.OpzioniOpenWin |= 2;
		if(theApp.theLog)
			theApp.OpzioniOpenWin |= 4;
		if(theApp.theChat)
			theApp.OpzioniOpenWin |= 8;
		if(theApp.aBrowser[0])
			theApp.OpzioniOpenWin |= 16;
		if(theApp.theConnections)
			theApp.OpzioniOpenWin |= 32;
		if(theApp.theDirectoryServer)
			theApp.OpzioniOpenWin |= 64;
		}
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		}
	else {
#ifndef _NEWMEET_MODE
		if(theApp.theServer || theApp.theServer2) {
#ifndef _DEBUG
#ifdef _LINGUA_INGLESE
			if(MessageBox("Closing VideoSender will stop video/audio streaming.\nContinue anyway?","Warning",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
#else
			if(MessageBox("La chiusura di VideoSender interromperà la trasmissione video/audio.\nContinuare?","Attenzione",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
#endif
				// metterci QUANTI utenti sono collegati!!
				i=FALSE;
#else
				i=TRUE;
#endif
			}
#endif
#ifndef _NEWMEET_MODE
		if(theApp.theChat && theApp.theChat->Opzioni & CVidsendDoc4::serverMode) {
#ifndef _DEBUG
#ifdef _LINGUA_INGLESE
			if(MessageBox("Closing VideoSender will stop Chat.\nContinue anyway?","Warning",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
#else
			if(MessageBox("La chiusura di VideoSender interromperà la Chat.\nContinuare?","Attenzione",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
#endif
				i=FALSE;
#else
				i=TRUE;
#endif
			}
#endif
#ifdef _NEWMEET_MODE
		if(theApp.theServer && theApp.theChat) {
#ifndef _DEBUG
#ifdef _LINGUA_INGLESE
			if(MessageBox("Closing VideoSender will stop Video and Chat.\nContinue anyway?","Warning",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
#else
			if(MessageBox("La chiusura di VideoSender interromperà la trasmissione Video e la Chat.\nContinuare?","Attenzione",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
#endif
				i=FALSE;
#else
				i=TRUE;
#endif
			}
#endif
		if(theApp.Opzioni & CVidsendApp::authServer || theApp.Opzioni & CVidsendApp::dirServer) {
#ifndef _DEBUG
			if(MessageBox("La chiusura di VideoSender interromperà la gestione Utenti Autenticati.\nContinuare?","Attenzione",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
				i=FALSE;
#else
				i=TRUE;
#endif
			}
		}
	if(i)
		CMDIFrameWnd::OnClose();
	
	}

BOOL CMainFrame::OnQueryEndSession() {
	int i=TRUE;

	if(!CMDIFrameWnd::OnQueryEndSession())
		return FALSE;
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		}
	else {
		if(theApp.theServer) {
#ifndef _DEBUG
#ifdef _LINGUA_INGLESE
			if(MessageBox("Closing Windows and VideoSender will stop streaming.\nContinue anyway?","Warning",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
#else
			if(MessageBox("La chiusura di Windows e di VideoSender interromperà la trasmissione video.\nContinuare?","Attenzione",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
#endif
				i=FALSE;
#else
				i=TRUE;
#endif
			}
		if(theApp.theChat && theApp.theChat->Opzioni & CVidsendDoc4::serverMode) {
			if(MessageBox("La chiusura di Windows e di VideoSender interromperà la Chat.\nContinuare?","Attenzione",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
				i=FALSE;
			}
		if(theApp.Opzioni & CVidsendApp::authServer || theApp.Opzioni & CVidsendApp::dirServer) {
			if(MessageBox("La chiusura di Windows e di VideoSender interromperà la gestione Utenti Autenticati.\nContinuare?","Attenzione",MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP) != IDOK)
				i=FALSE;
			}
		}
	return i;
 	}

void CMainFrame::OnContextMenu(CWnd* pWnd, CPoint point) {
	CMenu myMenu;
	
	myMenu.CreateMenu();
#ifdef _LINGUA_INGLESE
	myMenu.AppendMenu(MF_STRING,ID_FILE_CONFIG,"Setup...");
#else
	myMenu.AppendMenu(MF_STRING,ID_FILE_CONFIG,"Configurazione...");
#endif
	myMenu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, this);
	}

void CMainFrame::OnWindowLayout1() {
	RECT rc,rc3;
	int yy=GetSystemMetrics(SM_CYEDGE)*5+GetSystemMetrics(SM_CYCAPTION)*3+GetSystemMetrics(SM_CYMENUSIZE)*3;
	int xx=GetSystemMetrics(SM_CXEDGE)*6;

	GetWindowRect(&rc);
	rc.right-=rc.left;
	rc.bottom-=rc.top;
	rc.left=0;
	rc.top=0;
	if(theApp.theServer) {
		RECT rc2;
		theApp.theServer->getWindow(&rc2);
		if(theApp.aClient[0]) {		// server e client
			RECT rc4;
			theApp.aClient[0]->getWindow(&rc4);
			if(theApp.OpzioniOpenWin & 0x8000) {
				theApp.theServer->move(0,0);
				theApp.aClient[0]->move(rc2.right,0);
				rc.bottom=max(rc2.bottom,rc4.bottom)+yy;
				rc.right=rc2.right+rc4.right+2+xx;
				}
			else {
				if(rc.right < (rc2.right+rc4.right)) {
					theApp.theServer->move(0,0);
					theApp.aClient[0]->move(rc2.right,0);
					}
				else {
					RECT rc5;
					rc3.left=(rc.right/2-rc2.right)/2;
					rc5.left=rc.right/2+(rc.right/2-rc4.right)/2;
					theApp.theServer->move(rc3.left,0);
					theApp.aClient[0]->move(rc5.left,0);
					}
				}
			}
		else {		// solo server
			if(theApp.OpzioniOpenWin & 0x8000) {
				theApp.theServer->move(0,0);
				rc.bottom=rc2.bottom+yy;
				rc.right=rc2.right+xx;
				}
			else {
				rc3.left=max((rc.right-rc2.right)/2-1,0);
				theApp.theServer->move(rc3.left,0);
				}
			}
		}
	else {			// solo client
		if(theApp.aClient[0]) {
			RECT rc4;
			theApp.aClient[0]->getWindow(&rc4);
			if(theApp.OpzioniOpenWin & 0x8000) {
				theApp.aClient[0]->move(0,0);
				}
			else {
				rc3.left=max((rc.right-rc4.right)/2-1,0);
				theApp.aClient[0]->move(rc3.left,0);
				}
			if(theApp.OpzioniOpenWin & 0x8000) {
				rc.bottom=rc4.bottom+yy;
				rc.right=rc4.right+xx;
				}
			}
		}
	if(theApp.OpzioniOpenWin & 0x8000)
		SetWindowPos(NULL,0,0,rc.right,rc.bottom,SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE);
	}

void CMainFrame::OnWindowLayout2() {
	RECT rc,rc3;
	int yy=GetSystemMetrics(SM_CYEDGE)*5+GetSystemMetrics(SM_CYCAPTION)*3+GetSystemMetrics(SM_CYMENUSIZE)*4;
	int xx=GetSystemMetrics(SM_CXEDGE)*6;

	GetWindowRect(&rc);
	rc.right-=rc.left;
	rc.bottom-=rc.top;
	rc.left=0;
	rc.top=0;
	if(theApp.theChat) {
		RECT rc2;
		theApp.theChat->getWindow(&rc2);
		if(theApp.aClient[0]) {		// chat e video
			RECT rc4;
			theApp.aClient[0]->getWindow(&rc4);
			if(theApp.OpzioniOpenWin & 0x8000) {
				rc.right=(GetSystemMetrics(SM_CXSCREEN)*4)/5;
				rc.bottom=(rc.right*25)/40;
				theApp.aClient[0]->move((rc.right-rc4.right)/2-1,0);
				theApp.theChat->move(0,rc4.bottom,rc.right-1,rc.bottom-rc4.bottom);
				rc.bottom+=yy;
				rc.right+=xx;
				}
			else {
				rc3.left=max((rc.right-rc4.right)/2-1,0);
				theApp.aClient[0]->move(rc3.left,0);
				theApp.theChat->move(0,rc4.bottom,rc.right-xx,rc.bottom-rc4.bottom-yy);
				}
			}
		else {		// solo chat
			if(theApp.OpzioniOpenWin & 0x8000) {
				rc.right=(GetSystemMetrics(SM_CXSCREEN)*4)/5;
				rc.bottom=(rc.right*25)/40;
				theApp.theChat->move(0,0,rc.right,rc.bottom);
				rc.bottom+=yy;
				rc.right+=xx;
				}
			else {
				theApp.theChat->move(0,0,rc.right-xx,rc.bottom-yy);
				}
			}
		}
	else {			// solo client
		if(theApp.aClient[0]) {
			RECT rc4;
			theApp.aClient[0]->getWindow(&rc4);
			if(theApp.OpzioniOpenWin & 0x8000) {
				theApp.aClient[0]->move(0,0);
				}
			else {
				rc3.left=max((rc.right-rc4.right)/2-1,0);
				theApp.aClient[0]->move(rc3.left,0);
				}
			if(theApp.OpzioniOpenWin & 0x8000) {
				rc.bottom=rc4.bottom+yy;
				rc.right=rc4.right+xx;
				}
			}
		}
	if(theApp.OpzioniOpenWin & 0x8000)
		SetWindowPos(NULL,0,0,rc.right,rc.bottom,SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE);
	}

void CMainFrame::OnWindowLayout3() {
	RECT rc,rc3;
	int yy=GetSystemMetrics(SM_CYEDGE)*5+GetSystemMetrics(SM_CYCAPTION)*3+GetSystemMetrics(SM_CYMENUSIZE)*3;
	int xx=GetSystemMetrics(SM_CXEDGE)*6;

	GetWindowRect(&rc);
	rc.right-=rc.left;
	rc.bottom-=rc.top;
	rc.left=0;
	rc.top=0;
	if(theApp.theServer) {
		RECT rc2;
		theApp.theServer->getWindow(&rc2);
		rc3.top=0;
		rc3.left=rc2.right+1;
		if(theApp.OpzioniOpenWin & 0x8000) {
			rc3.right=(GetSystemMetrics(SM_CXSCREEN)*9)/10;
			rc3.bottom=(rc.right*8)/40;
			rc.right=rc3.right;
			rc.bottom=rc3.bottom*3;
			}
		else {
			rc3.right=rc.right-xx;
			rc3.bottom=(rc.bottom-yy)/3;
			rc.right-=xx;
			rc.bottom-=yy;
			}
		rc3.right-=rc3.left;
		if(theApp.theConnections || theApp.theLog || theApp.theDirectoryServer) {		// server ed elenchi
			RECT rc4;
			rc4.left=max((rc.right-rc3.right-rc2.right)/2-1,0);
			if(rc.bottom-rc2.bottom > rc3.bottom)		// se sotto ci starebbe il log tutto largo...
				rc4.top=max((rc.bottom-rc2.bottom-rc3.bottom)/2-1,0);
			else
				rc4.top=max((rc.bottom-rc2.bottom)/2-1,0);
			theApp.theServer->move(rc4.left,rc4.top);
			if(theApp.theConnections) {
				theApp.theConnections->move(rc3.left,rc3.top,rc3.right,rc3.bottom);
				}
			rc3.top=rc3.bottom+1;
			if(theApp.theDirectoryServer) {
				theApp.theDirectoryServer->move(rc3.left,rc3.top,rc3.right,rc3.bottom);
				}
			rc3.top+=rc3.bottom+1;
			if(theApp.theLog) {
				if(rc2.bottom+rc3.bottom+1 < rc.bottom)	// ...se ci stava il log tutto largo...
					theApp.theLog->move(0,rc3.top,rc.right,rc3.bottom);
				else
					theApp.theLog->move(rc3.left,rc3.top,rc3.right,rc3.bottom);
				rc3.top+=rc3.bottom+1;
				}
			if(theApp.OpzioniOpenWin & 0x8000) {
				rc.right+=xx;
				rc.bottom+=yy;
				}
			}
		else {		// solo server
			if(theApp.OpzioniOpenWin & 0x8000) {
				theApp.theServer->move(0,0);
				rc.bottom=rc2.bottom+yy;
				rc.right=rc2.right+xx;
				}
			else {
				rc3.left=max((rc.right-rc2.right)/2-1,0);
				theApp.theServer->move(rc3.left,0);
				}
			}
		}
	else {			// solo connessioni
		rc3.top=0;
		if(theApp.OpzioniOpenWin & 0x8000) {
			rc3.right=(GetSystemMetrics(SM_CXSCREEN)*4)/5;
			rc3.bottom=(rc.right*9)/40;
			}
		else {
			rc3.right=rc.right-xx;
			rc3.bottom=(rc.bottom-yy)/3;
			}
		if(theApp.theConnections) {
			theApp.theConnections->move(0,rc3.top,rc3.right,rc3.bottom);
			rc3.top=rc3.bottom+1;
			}
		if(theApp.theDirectoryServer) {
			theApp.theDirectoryServer->move(0,rc3.top,rc3.right,rc3.bottom);
			rc3.top+=rc3.bottom+1;
			}
		if(theApp.theLog) {
			theApp.theLog->move(0,rc3.top,rc3.right,rc3.bottom);
			rc3.top+=rc3.bottom+1;
			}
		if(theApp.OpzioniOpenWin & 0x8000) {
			rc.right=rc3.right+xx;
			rc.bottom=rc3.top+yy;
			}
		}
	if(theApp.OpzioniOpenWin & 0x8000)
		SetWindowPos(NULL,0,0,rc.right,rc.bottom,SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE);
	
	}

void CMainFrame::OnWindowLayout4() {
	
	}



void CMainFrame::OnWindowLayoutAdatta() {
/*	RECT rc;
	CMDIChildWnd *w=MDIGetActive();
	if(w) {
		w->GetClientRect(&rc);
		rc.bottom+=GetSystemMetrics(SM_CYEDGE)*5+GetSystemMetrics(SM_CYCAPTION)*3+GetSystemMetrics(SM_CYMENUSIZE)*3;
		rc.bottom+=GetSystemMetrics(SM_CYMENUSIZE);		// perche' spesso la menu bar passa su due righe...
		rc.right+=GetSystemMetrics(SM_CXEDGE)*8;
		w->SetWindowPos(NULL,0,0,0,0,SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSIZE);
		SetWindowPos(NULL,0,0,rc.right,rc.bottom,SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE);
		}*/
	theApp.OpzioniOpenWin ^= 0x8000;
	}

void CMainFrame::OnUpdateWindowLayoutAdatta(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(theApp.OpzioniOpenWin & 0x8000 ? 1 : 0);
	}

/*void CMainFrame::OnDraw(CDC* pDC) {
	RECT rc;
	
  if(IsIconic())    {
		}
	else {
		GetClientRect(&rc);
		if(!theApp.sfondo.IsEmpty()) {
			theApp.renderBitmap(pDC,IDB_DISCONNESSO,&rc);
		}
				}
	}*/
/*void CMainFrame::OnPaint() {
//	https://forums.codeguru.com/showthread.php?111082-Bitmaps-in-the-MainFrame-Background
	RECT rc;
	
  if(IsIconic())    {
    CPaintDC dc(this); // device context for painting
		}
	else {
    CPaintDC dc(this); // device context for painting
		GetClientRect(&rc);
		if(!theApp.sfondo.IsEmpty()) {
			theApp.renderBitmap(&dc,theApp.sfondo,&rc,0);
		}
				}
	CMDIFrameWnd::OnPaint();
	}*/


BEGIN_MESSAGE_MAP(CBackWnd, CWnd)
	//{{AFX_MSG_MAP(CBackWnd)
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CBackWnd::OnEraseBkgnd(CDC *pDC) {
//	https://forums.codeguru.com/showthread.php?111082-Bitmaps-in-the-MainFrame-Background
	RECT rc;
	
  if(IsIconic())    {
		}
	else {
		GetClientRect(&rc);
		if(!theApp.sfondo.IsEmpty()) {
			theApp.renderBitmap(pDC,theApp.sfondo,&rc,0);
//			theApp.renderBitmap(pDC,IDB_MONOSCOPIO,&rc);
			return 1 /*CWnd::OnEraseBkgnd(pDC)*/;
			}
		else
			return CWnd::OnEraseBkgnd(pDC);
		}
	return CWnd::OnEraseBkgnd(pDC);
	}



BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) {
	BOOL bRet = CMDIFrameWnd::OnCreateClient(lpcs, pContext);

#undef SubclassWindow		// https://jeffpar.github.io/kbarchive/kb/150/Q150076/ fanculo
	if(bRet)
		m_BackWnd.SubclassWindow(m_hWndMDIClient);

	return bRet;
	}

/*BOOL CMainFrame::OnEraseBkgnd(CDC *pDC) {
//	https://forums.codeguru.com/showthread.php?111082-Bitmaps-in-the-MainFrame-Background
	RECT rc;
	
  if(IsIconic())    {
		}
	else {
		GetClientRect(&rc);
		if(!theApp.sfondo.IsEmpty()) {
//			theApp.renderBitmap(pDC,theApp.sfondo,&rc,0);
			theApp.renderBitmap(pDC,IDB_MONOSCOPIO,&rc);
		}
				}
	return  CMDIFrameWnd::OnEraseBkgnd(pDC);
	}*/

void CMainFrame::OnNcPaint() {
	RECT rc;
#ifdef _NEWMEET_MODE
	CWindowDC pDC(this);
	
	GetWindowRect(&rc);
	rc.top+=44;
	rc.left+=160;
	rc.bottom=rc.top+16;
	rc.right=rc.left+96;
	if(!theApp.theServer || !theApp.theServer->authSocket) {
		theApp.renderBitmap(&pDC,IDB_DISCONNESSO,&rc);
		}
#endif	
	CMDIFrameWnd::OnNcPaint();
	}


void CMainFrame::OnTimer(UINT nIDEvent) {
	static int divider;
	int i;
	char myBuf[128];


#ifndef _NEWMEET_MODE
	DWORD tiTimer=timeGetTime();
	static DWORD correctionTimer;
#endif
	

#ifndef _CAMPARTY_MODE
	if(!(divider % 6)) {		// ogni 30 sec.
		if(theApp.authSocket)
			theApp.authSocket->updateDBUtenti();
		}
#endif

	if(theApp.theServer) {
		theApp.theServer->checkUtenti();
		if(!(divider % 3)) {		// ogni 15 sec.
			theApp.theServer->sendHrtBt();
			if(theApp.debugMode) 
				if(theApp.FileSpool)
					theApp.FileSpool->print(CLogFile::flagInfo2,"  invio HeartBeat");
			}

		theApp.theServer->HandlePipeV();
		}

//#ifndef _NEWMEET_MODE
	if(!(divider % 3)) {		// ogni 15 sec.
		for(i=0; i<16; i++) {
			if(theApp.aClient[i])
				theApp.aClient[i]->sendHrtBt();
			}
		if(theApp.theChat)
			theApp.theChat->sendHrtBt();
		}
//#endif

#ifndef _NEWMEET_MODE
	if(theApp.authSocket) {
		CString aIP;
		UINT aPort;

		if(!(divider % 2)) {		// ogni 10 sec.
			CAuthSrvSocket2 *myRoot2;
			POSITION po;

			po=theApp.authSocket->cSockRoot.GetHeadPosition();
			if(po) {
				do {
					myRoot2=theApp.authSocket->cSockRoot.GetNext(po);




					myRoot2->GetPeerName(aIP,aPort);
					if(theApp.debugMode) 
						if(theApp.FileSpool)
							theApp.FileSpool->print(CLogFile::flagInfo2,"  auth: IP: %s, time: %u",(LPCTSTR)aIP,myRoot2->cliTimeOut);

					if(correctionTimer) {
						myRoot2->cliTimeOut+=correctionTimer;
						if(theApp.debugMode) 
							if(theApp.FileSpool)
								theApp.FileSpool->print(CLogFile::flagInfo,"  correggo Timer di %u sec.",correctionTimer/1000);

						}


					if((myRoot2->cliTimeOut+AUTHSOCK_TIMEOUT) < timeGetTime()) {







						char myBuf[16];
						myBuf[0]=10;
						myBuf[1]=0;
						if((i=myRoot2->Send(myBuf,2)) == 2) {
							theApp.FileSpool->print(CLogFile::flagInfo2,"** inviata richiesta heartbeat d'emergenza a guest!");

							// non so come sia possibile, ma pur essendo i sock connessi ed entrambi in trasmissione
							// HeartBeat non viene ricevuto... QUINDI FACCIO COSI'!!
							myRoot2->cliTimeOut=timeGetTime();

							}
						else {
							DWORD n=GetLastError();
							if(theApp.FileSpool) 
								theApp.FileSpool->print(2,"impossibile inviare richiesta heartbeat d'emergenza (errore Send %d,%u)!",i,n);
							}






						if(theApp.FileSpool) {
							myRoot2->GetPeerName(aIP,aPort);
							theApp.FileSpool->print(2,"  cancellazione utente che non risponde: %s (tipo=%x)",(LPCTSTR)aIP,myRoot2->proprieta);

							}


#ifdef DARIO_TIMEOUTSULSERVER
						// forse qua si fermava a cercare di avvisare chi "chiude", e ne perde altri...
						*myBuf=12;			// avviso che è stato cancellato dal server...
						myBuf[1]=0;
						myRoot2->Send(myBuf,2);
#endif

	//					myRoot2->Close();	//non dovrebbe servire...
						myRoot2->m_Parent->doDelete(myRoot2,TRUE);
						// se è un guest, bisognerebbe che l'exhibitor a cui è collegato lo buttasse fuori... è un po' un casino!
						}
					} while(po);
			
				}

			
			if(correctionTimer) 
				correctionTimer=0;


			}
		}




#endif

//#ifndef _NEWMEET_MODE			// forse potrei NON volerlo nella modalita' normale... boh?
	if(theApp.theServer) {
		CString aIP;
		UINT aPort;

		if(!(divider % 2)) {		// ogni 10 sec.
			CControlSrvSocket2 *myRoot2;
			POSITION po,po1;

			po=theApp.theServer->controlSocket->cSockRoot.GetHeadPosition();
			if(po) {
				do {
					po1=po;
					myRoot2=theApp.theServer->controlSocket->cSockRoot.GetNext(po);
					if((myRoot2->cliTimeOut+90000) < timeGetTime()) {

//					if(theApp.FileSpool) {
//						myRoot2->GetPeerName(aIP,aPort);
//						theApp.FileSpool->print(2,"  cancellazione utente che non risponde: %s",(LPCTSTR)aIP);
//						}

//					myRoot2->Close();	//non dovrebbe servire...
						myRoot2->m_Parent->doDelete(myRoot2);
						}
					} while(po);
				}
			}
		}
/*	if(theApp.theChat) {		// FINIRE!!
		CString aIP;
		UINT aPort;

		if(!(divider % 2)) {		// ogni 10 sec.
			CChatSrvSocket2 *myRoot2,*myRoot3;
			myRoot2=theApp.theChat->chatSocket.cSockRoot;
			while(myRoot2) {
				if((myRoot2->cliTimeOut+120000) < timeGetTime()) {
					myRoot3=myRoot2->next;

//					if(theApp.FileSpool) {
//						myRoot2->GetPeerName(aIP,aPort);
//						theApp.FileSpool->print(2,"  cancellazione utente che non risponde: %s",(LPCTSTR)aIP);
//						}

//					myRoot2->Close();	//non dovrebbe servire...
					myRoot2->m_Parent->doDelete(myRoot2);
					myRoot2=myRoot3;
					}
				else
					myRoot2=myRoot2->next;
				}
			}
		}*/
//#endif


#ifndef _NEWMEET_MODE
	if((timeGetTime() - tiTimer) > 5000) {
		if(theApp.debugMode) 
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagWarning,"  OnTimer è durata + di %u sec",
					(timeGetTime() - tiTimer)/1000);
			correctionTimer=timeGetTime() - tiTimer;
		}
#endif


	if(!(divider % 24)) {		// ogni 2 min
		if(CTime::GetCurrentTime().GetDay() != theApp.ultimaOra.GetDay() ||
			CTime::GetCurrentTime().GetMonth() != theApp.ultimaOra.GetMonth() ||
			CTime::GetCurrentTime().GetYear() != theApp.ultimaOra.GetYear()) {
			if(theApp.theServer && theApp.theServer->theTV && theApp.theServer->OpzioniSalvaVideo & CVidsendDoc2::quantiFile) {
				if(theApp.theServer->theTV->isRecordingVideo()) {
					CString S;
					theApp.theServer->theTV->endSaveFile();
					theApp.theServer->nomeAVI=theApp.creaStringaDaGiorno();
					theApp.theServer->theTV->startSaveFile(theApp.theServer->pathAVI+theApp.theServer->nomeAVI,theApp.theServer->OpzioniSalvaVideo);
					}
				}
			//cancellare file vecchi!!
			theApp.ultimaOra=CTime::GetCurrentTime();
			}
		}

	divider++;
	CMDIFrameWnd::OnTimer(nIDEvent);
	}



void CMainFrame::OnVisualizzaToolbar() {

	if(m_wndToolBar.IsWindowVisible() ) {
		theApp.Opzioni &= ~CVidsendApp::hasToolbar;
//    SendMessage(WM_COMMAND, ID_VIEW_TOOLBAR, 0L);
		}
	else {
		theApp.Opzioni |= CVidsendApp::hasToolbar;
//    SendMessage(WM_COMMAND, ID_VIEW_TOOLBAR, 1L);
		}
	showToolbar(theApp.Opzioni & CVidsendApp::hasToolbar);
	}

void CMainFrame::OnVisualizzaStatusBar() {

	if(m_wndStatusBar.IsWindowVisible() ) {
		theApp.Opzioni &= ~CVidsendApp::hasStatusbar;
//    SendMessage(WM_COMMAND, ID_VIEW_STATUS_BAR, 0L);
		}
	else {
		theApp.Opzioni |= CVidsendApp::hasStatusbar;
//    SendMessage(WM_COMMAND, ID_VIEW_STATUS_BAR, 1L);
		}
	showStatusbar(theApp.Opzioni & CVidsendApp::hasStatusbar);
	}

void CMainFrame::OnUpdateVisualizzaToolbar(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(theApp.Opzioni & CVidsendApp::hasToolbar);
	}

void CMainFrame::OnUpdateVisualizzaStatusBar(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(theApp.Opzioni & CVidsendApp::hasStatusbar);
	}


