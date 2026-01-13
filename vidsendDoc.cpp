// vidsend2Doc.cpp : implementation of the CVidsendDoc class
//

#include "stdafx.h"
#include "vidsend.h"

#include "childfrm.h"
#include "vidsendDoc.h"
#include "vidsendView.h"
#include "vidsendlog.h"
#include "vidsendDialog.h"
#include "vidsendSet.h"
#include "digitaltext.h"
#include "cjpeg2.h"
#include "player.h"
#include "mp3coder.h"
#include "joshuamp3.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


static int fAbort;		// mettere tutta 'sta roba in classe CVidsendDoc2, come static...
static HWND hwndAbort;
CALLBACK EXPORT AbortProc(HDC, int );
LRESULT CALLBACK EXPORT AbortDlgProc(HWND, UINT, WPARAM, LPARAM);


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc

IMPLEMENT_DYNCREATE(CVidsendDoc, CDocument)

BEGIN_MESSAGE_MAP(CVidsendDoc, CExDocument)
	//{{AFX_MSG_MAP(CVidsendDoc)
	ON_COMMAND(ID_FILE_PROPRIETA, OnFileProprieta)
	ON_COMMAND(ID_VISUALIZZA_43, OnVisualizza43)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_43, OnUpdateVisualizza43)
	ON_COMMAND(ID_VISUALIZZA_ATUTTOSCHERMO, OnVisualizzaAtuttoschermo)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_ATUTTOSCHERMO, OnUpdateVisualizzaAtuttoschermo)
	ON_COMMAND(ID_VISUALIZZA_BN, OnVisualizzaBn)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_BN, OnUpdateVisualizzaBn)
	ON_COMMAND(ID_VISUALIZZA_DIMENSIONEDOPPIA, OnVisualizzaDimensionedoppia)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_DIMENSIONEDOPPIA, OnUpdateVisualizzaDimensionedoppia)
	ON_COMMAND(ID_VISUALIZZA_STEREOSIMULATO, OnVisualizzaStereosimulato)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_STEREOSIMULATO, OnUpdateVisualizzaStereosimulato)
	ON_COMMAND(ID_VISUALIZZA_MUTE, OnVisualizzaMute)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_MUTE, OnUpdateVisualizzaMute)
	ON_COMMAND(ID_CONNESSIONE_PAUSA, OnConnessionePausa)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_PAUSA, OnUpdateConnessionePausa)
	ON_COMMAND(ID_CONNESSIONE_RIPRENDI, OnConnessioneRiprendi)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_RIPRENDI, OnUpdateConnessioneRiprendi)
	ON_COMMAND(ID_VISUALIZZA_VOLUME, OnVisualizzaVolume)
	ON_COMMAND(ID_FILE_PRINT_VIDEO, OnFilePrint)
	ON_UPDATE_COMMAND_UI(ID_FILE_PRINT_VIDEO, OnUpdateFilePrint)
	ON_COMMAND(ID_CONTROLLO_IMMAGINE, OnControlloImmagine)
	ON_UPDATE_COMMAND_UI(ID_CONTROLLO_IMMAGINE, OnUpdateControlloImmagine)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc construction/destruction

CVidsendDoc::CVidsendDoc() {
	char myBuf[128];
	
	prfSection="ClientStream";
	srvAddress="";
//	srvAddress=_T("192.168.0.2");
	bPaused=0;
	authSocket=NULL;
	Opzioni=GetPrivateProfileInt(IDS_OPZIONI);
	luminosita=GetPrivateProfileInt(IDS_LUMINOSITA);
	contrasto=GetPrivateProfileInt(IDS_CONTRASTO);
	saturazione=GetPrivateProfileInt(IDS_SATURAZIONE);
	numBuffers=GetPrivateProfileInt(IDS_BUFFERSCLIENT);
  GetPrivateProfileString(IDS_USER,myBuf,31);
  loginName=myBuf;
  GetPrivateProfileString(IDS_PASW,myBuf,31);
  loginPasw=myBuf;
  GetPrivateProfileString(IDS_AUTHSERVER,myBuf,127);
  authWWW=myBuf;
	wantsVideo=wantsAudio=TRUE;
	}

CVidsendDoc::~CVidsendDoc() {

	theApp.m_pMainWnd->PostMessage(WM_CLOSE_CHILD,1 /*video client*/,(LPARAM)this);
	save();
  WritePrivateProfileInt(IDS_OPZIONI,Opzioni);
  WritePrivateProfileInt(IDS_LUMINOSITA,luminosita);
  WritePrivateProfileInt(IDS_CONTRASTO,contrasto);
  WritePrivateProfileInt(IDS_SATURAZIONE,saturazione);
  WritePrivateProfileInt(IDS_BUFFERSCLIENT,numBuffers);
  WritePrivateProfileString(IDS_USER,(char *)(LPCTSTR)loginName);
  WritePrivateProfileString(IDS_PASW,(char *)(LPCTSTR)loginPasw);
  WritePrivateProfileString(IDS_AUTHSERVER,(char *)(LPCTSTR)authWWW);
	}

BOOL CVidsendDoc::OnNewDocument() {

	if(!CExDocument::OnNewDocument())
		return FALSE;

	return TRUE;
	}

void CVidsendDoc::OnFileProprieta() {
	int i;
	CVidsendPropPage mySheet("Proprietà client audio/video",(CVidsendView2 *)getView());
	CVidsendDocPropPage0 myPage0(this);
	CVidsendDocPropPage1 myPage1(this);
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		if(0)
			goto fine;
		}
	mySheet.AddPage(&myPage0);
	mySheet.AddPage(&myPage1);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
		if(myPage0.isInitialized) {
			Opzioni &= 0xffff0000;
			Opzioni |= myPage0.m_BN ? fmt_bn : 0;
			Opzioni |= myPage0.m_4_3 ? fmt4_3 : 0;
			Opzioni |= myPage0.m_FullScreen ? fmt_full : 0;
			Opzioni |= myPage0.m_DoubleSize ? fmt_double : 0;
			Opzioni |= myPage0.m_ResizeAll ? fmt_resize : 0;
			Opzioni |= myPage0.m_SStereo ? sstereo : 0;
			numBuffers=myPage0.m_bBuffers ? myPage0.m_Buffers : 0;
			}
		
		if(myPage1.isInitialized) {
			Opzioni &= 0xffffff;
			Opzioni |= myPage1.m_bConnetti ? autoRASconnect : 0;
			Opzioni |= myPage1.m_Authenticate ? authenticate : 0;
			Opzioni |= myPage1.m_bProxy ? usaProxy : 0;
			}
		}
fine:
		;
	}


void CVidsendDoc::OnFilePrint() {
  CMyPrintDialog myDlg;	
	int i,j,fAbort;
	char myBuf[256],myBuf1[128];
	BYTE *p,*p2;
	DWORD ti;
//	CPrintInfo pI;
	HDC hDC=NULL,hDC1;
	HBITMAP hBmp,hOldbmp;
	HWND hwndAbort;
	DEVMODE *lpdm;
	RECT rc,rc2;
	DOCINFO dInfo;
	CVidsendView *w=(CVidsendView *)getView();

	if(myDlg.DoModal() == IDOK) {
		hDC=myDlg.GetPrinterDC();
		lpdm=myDlg.GetDevMode();
		rc.top=rc.left=0;
		rc.right=lpdm->dmPaperWidth;
		rc.bottom=lpdm->dmPaperLength;
		rc.right=GetDeviceCaps(hDC,HORZRES);
		rc.bottom=GetDeviceCaps(hDC,VERTRES);
		BeginWaitCursor();
		_tcscpy(myBuf,"Immagine Video ricevuta ");
		wsprintf(myBuf1,"(VideoSender %ux%u)",w->streamInfo.bm.biWidth,w->streamInfo.bm.biHeight);

		_tcscat(myBuf,myBuf1);
		dInfo.cbSize=sizeof(DOCINFO);
		dInfo.lpszDocName=myBuf;
		dInfo.lpszOutput=NULL;
		dInfo.lpszDatatype=NULL;
		dInfo.fwType=0;

		fAbort = FALSE;
		hwndAbort = CreateDialog(theApp.m_hInstance,MAKEINTRESOURCE(IDD_ABORTPRINT),w->m_hWnd,(DLGPROC)AbortDlgProc);
		EnableWindow(w->m_hWnd,FALSE);
		i=SetAbortProc(hDC,AbortProc);
		if((i=StartDoc(hDC,&dInfo)) >0 ) {

			CBitmap b;
			BYTE *p;
			DWORD len;
			DWORD l=w->streamInfo.bm.biWidth*w->streamInfo.bm.biHeight*3;
			if(b.CreateBitmap(w->streamInfo.bm.biWidth,w->streamInfo.bm.biHeight,1,24,NULL)) {
				if(b.SetBitmapBits(l,w->getFrame())) {

					rc2.top=rc.left=0;
					rc2.right=((rc.right*19)/20)/2;		// un po' meno di meta'
					rc2.bottom=(rc2.right*3)/4  /*rc.bottom/2*/;

					wsprintf(myBuf,"Stampa in corso...");
					SetDlgItemText(hwndAbort,IDC_TEXT1,myBuf);
					if(StartPage(hDC) >0 ) {
						hDC1=CreateCompatibleDC(hDC);

// NON VA! FINIRE! theFrame va decompresso prima... (v. anche OnEditCopy)

						hBmp=CreateDIBSection(hDC1,(LPBITMAPINFO)&w->streamInfo.bm,DIB_RGB_COLORS,(void **)&p,NULL,NULL);
						memcpy(p,w->getFrame(),w->streamInfo.bm.biSizeImage);
						hOldbmp=(HBITMAP)SelectObject(hDC1,hBmp /*(HBITMAP)b*/);
						j=StretchBlt(hDC,25,25 /*rc2.bottom*/,rc2.right, /* - */ rc2.bottom,hDC1,0,0,rc2.right,rc2.bottom,SRCCOPY);
						SelectObject(hDC1,hOldbmp);
						DeleteObject(hBmp);
						DeleteDC(hDC1);
						EndPage(hDC);
						}
					}

				}
			EndDoc(hDC);
			}

		if(!fAbort) {
			EnableWindow(getView()->m_hWnd,TRUE);
			DestroyWindow(hwndAbort);
			}
		
fine:
		EndWaitCursor();
		HeapFree(GetProcessHeap(),0,lpdm);
		}

	DeleteDC(hDC);
   
	}


void CVidsendDoc::OnUpdateFilePrint(CCmdUI* pCmdUI) {
	CVidsendView *w=(CVidsendView *)getView();
	
	pCmdUI->Enable(w && w->cliSockCtrl->m_hSocket != INVALID_SOCKET && w->getFrame() != NULL);
	}

void CVidsendDoc::OnConnessionePausa() {
	
	bPaused=1;
	}

void CVidsendDoc::OnUpdateConnessionePausa(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(bPaused);
	pCmdUI->Enable(!bPaused);
	}

void CVidsendDoc::OnConnessioneRiprendi() {
	
	bPaused=0;
	// si potrebbero svuotare i buffer e Xon ecc!
	}

void CVidsendDoc::OnUpdateConnessioneRiprendi(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(!bPaused);
	pCmdUI->Enable(bPaused);
	}


void CVidsendDoc::OnVisualizza43() {
	
	Opzioni ^= CVidsendDoc::fmt4_3;
	standardSize();
	}

void CVidsendDoc::OnUpdateVisualizza43(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::fmt4_3 ? 1 : 0);
	pCmdUI->Enable(((CVidsendView *)getView())->cliSockV ? 1 : 0);
	}


void CVidsendDoc::OnVisualizzaBn() {
	
	Opzioni ^= CVidsendDoc::fmt_bn;
	}

void CVidsendDoc::OnUpdateVisualizzaBn(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::fmt_bn ? 1 : 0);
	pCmdUI->Enable(((CVidsendView *)getView())->cliSockV ? 1 : 0);
	}

void CVidsendDoc::OnVisualizzaDimensionedoppia() {
	
	Opzioni ^= CVidsendDoc::fmt_double;
	Opzioni &= ~CVidsendDoc::fmt_full;
	standardSize();
	}

void CVidsendDoc::OnUpdateVisualizzaDimensionedoppia(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::fmt_double ? 1 : 0);
	pCmdUI->Enable(((CVidsendView *)getView())->cliSockV ? 1 : 0);
	}

void CVidsendDoc::OnVisualizzaAtuttoschermo() {
	
	Opzioni ^= CVidsendDoc::fmt_full;
	Opzioni &= ~CVidsendDoc::fmt_double;
	standardSize();
	}

void CVidsendDoc::OnUpdateVisualizzaAtuttoschermo(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::fmt_full ? 1 : 0);
	pCmdUI->Enable(((CVidsendView *)getView())->cliSockV ? 1 : 0);
	}

void CVidsendDoc::standardSize() {
	RECT rc;
	CVidsendView *w=(CVidsendView *)getView();
	
	SetRectEmpty(&rc);
	rc.right=w->streamInfo.bm.biWidth;
	rc.bottom=w->streamInfo.bm.biHeight;
	if(Opzioni & CVidsendDoc::fmt_full) {
		RECT rc2;
		theApp.m_pMainWnd->GetClientRect(&rc2);
		rc=rc2;
		w->GetParent()->SetWindowPos(NULL,rc.left,rc.top,rc.right,rc.bottom,SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		goto fine;
		}
	else {
		if(Opzioni & CVidsendDoc::fmt_double) {
			rc.right *= 2;
			rc.bottom *= 2;
			}
/*		if(Opzioni & CVidsendDoc::fmt_controls) {
			rc.bottom += 100;
			} NO, dev'essere fuori dalla client area!! */
		}
//	if(Opzioni & CVidsendDoc::fmt4_3)		v. ChildFrm::OnSize!
//		rc.bottom = (rc.right*3)/4;
	rc.right+=GetSystemMetrics(SM_CXEDGE)*3;
	rc.bottom+=GetSystemMetrics(SM_CYEDGE)*5+GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYCAPTION);

	if(w)
		w->GetParent()->SetWindowPos(NULL,0,0,rc.right,rc.bottom,SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
fine: ;
	}


//-----------------------------------------------------------------------------
void CExDocument::getWindow(RECT *r) {
	CVidsendView *w=(CVidsendView *)getView();

	if(w)
		w->GetParent()->GetWindowRect(r);
	r->bottom-=r->top;
	r->right-=r->left;
	}

void CExDocument::move(int x1,int y1,int x2,int y2) {
	CVidsendView *w=(CVidsendView *)getView();

	if(w) {
		if(x2 && y2)
			w->GetParent()->SetWindowPos(NULL,x1,y1,x2,y2,SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		else
			w->GetParent()->SetWindowPos(NULL,x1,y1,0,0,SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		}
	}

BOOL CExDocument::OnNewDocument() {

	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		char myBuf[64];
		RECT rc;
		CVidsendView *w=(CVidsendView *)getView();

		SetRectEmpty(&rc);
		GetPrivateProfileString(IDS_COORDINATE,myBuf,32);
		sscanf(myBuf,"%d,%d,%d,%d",&rc.left,&rc.top,&rc.right,&rc.bottom);	// sono coord. client rispetto alla MDIFRAME madre della mia ChildFrame
		if(!IsRectEmpty(&rc))
			w->GetParent()->SetWindowPos(NULL,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);
		}

	return CDocument::OnNewDocument();
	}

void CExDocument::savelayout() {
	RECT rc;
	char myBuf[64];

	getView()->GetParent()->GetWindowRect(&rc);	// coord. schermo della mia MDIChildFrmae...
	getView()->GetParent()->GetParent()->ScreenToClient(&rc);	// ... diventano coord. client rispetto al MDI padre (che NON e' theApp.m_pMainWnd !! ce n'è una in mezzo...
//		if(!IsIconic() && !IsZoomed()) {		// NON VANNO PORCODIO
	if(rc.left>0 && rc.top>0 && (rc.right-rc.left)>100 && (rc.bottom-rc.top)>50) {		// cagata quando qualsiasi MDI child maximized...@#$%
		wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
		WritePrivateProfileString(IDS_COORDINATE,myBuf);
		}
	}

void CVidsendDoc::OnVisualizzaStereosimulato() {
	
	Opzioni ^= CVidsendDoc::sstereo;
	}

void CVidsendDoc::OnUpdateVisualizzaStereosimulato(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::sstereo ? 1 : 0);
	pCmdUI->Enable(((CVidsendView *)getView())->cliSockA ? 1 : 0);
	}

void CVidsendDoc::OnVisualizzaMute() {
	
	Opzioni ^= CVidsendDoc::muted;
	}

void CVidsendDoc::OnUpdateVisualizzaMute(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::muted ? 1 : 0);
	pCmdUI->Enable(((CVidsendView *)getView())->cliSockA ? 1 : 0);
	}

void CVidsendDoc::OnVisualizzaVolume() {

	WinExec("sndvol32.exe",SW_SHOWNORMAL);
	WinExec("sndvol.exe",SW_SHOWNORMAL);			// per Windows 7 !!!
	}


CString CVidsendDoc::loadCliConnRecent(CString s[MRU_SIZE]) {
	char myBuf[64],myBuf2[256],ks[32];
	int i;

	LoadString(theApp.m_hInstance,IDS_RECENTCONN,ks,32);
	for(i=0; i<MRU_SIZE; i++) {
		wsprintf(myBuf,"%s%u",ks,i);
		theApp.prStore->GetPrivateProfileString(prfSection,myBuf,myBuf2,159);
		s[i]=myBuf2;
		}
	return s[0];
	}

BOOL CVidsendDoc::save() {
	CString prevURL[MRU_SIZE];
	char myBuf[64],ks[32];
	int i,j,n;

	if(!srvAddress.IsEmpty()) {
		loadCliConnRecent(prevURL);
		for(i=0; i<MRU_SIZE; i++) {
			if(prevURL[i] == srvAddress) {
				prevURL[i].Empty();
				}
			}
		n=MRU_SIZE;
		for(i=0; i<n; i++) {
			if(prevURL[i].IsEmpty()) {
				for(j=i+1; j<n; j++)
					prevURL[j-1]=prevURL[j];
				prevURL[--n].Empty();
				i--;
				}
			}
		for(i=MRU_SIZE-2; i>=0; i--) {
			prevURL[i+1]=prevURL[i];
			}
		prevURL[0]=srvAddress;
		LoadString(theApp.m_hInstance,IDS_RECENTCONN,ks,32);
		for(i=0; i<MRU_SIZE; i++) {
			wsprintf(myBuf,"%s%u",ks,i);
			theApp.prStore->WritePrivateProfileString(prfSection,myBuf,(LPCTSTR)prevURL[i]);
			}
		}
	return 1;
	}



/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc serialization

void CVidsendDoc::Serialize(CArchive& ar) {
	
	if(ar.IsStoring()) {
		// TODO: add storing code here
		}
	else {
		// TODO: add loading code here
		}
	}


void CVidsendDoc::OnControlloImmagine() {

	Opzioni ^= CVidsendDoc::fmt_controls;
	standardSize();
	}

void CVidsendDoc::OnUpdateControlloImmagine(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::fmt_controls);
	pCmdUI->Enable(((CVidsendView *)getView())->cliSockV ? 1 : 0);
	}


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc diagnostics

#ifdef _DEBUG
void CVidsendDoc::AssertValid() const
{
	CExDocument::AssertValid();
}

void CVidsendDoc::Dump(CDumpContext& dc) const
{
	CExDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc commands

void CVidsendDoc::sendHrtBt() {
	char myBuf[2];
	CVidsendView *w=(CVidsendView *)getView();
	
#ifndef _NEWMEET_MODE				// era cosi'... per ora non tocchiamo!!
	if(authSocket) {
		myBuf[0]=11;
		myBuf[1]=0;
		authSocket->Send(myBuf,2);
		}
#endif
	if(w && w->cliSockCtrl) {
		myBuf[0]=11;
		myBuf[1]=0;
		w->cliSockCtrl->Send(myBuf,2);
		}
	}





/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2

IMPLEMENT_DYNCREATE(CVidsendDoc2, CDocument)

const struct WAVE_QUALITY CVidsendDoc2::waveQualities[8] = {
	{8000,8,1},
	{11025,8,1},
	{22050,8,1},
	{22050,16,1},
	{44100,8,1},
	{44100,16,1},
	{44100,16,2},
	{48000,16,2},
	};

const struct QUALITY_MODEL_V CVidsendDoc2::qsv[15]= {
	{160,120,24,1,2500},
	{160,120,24,2,2500}, /*200 150*/
	{160,120,24,5,2500}, /*240 180*/
	{320,240,24,2,5000}, /*240 180*/
	{320,240,24,2,7500},
	{320,240,24,5,5000},
	{320,240,24,10,5000},
	{320,240,24,15,7500},
	{640,480,24,5,7000},
	{640,480,24,15,9000},
	{720,576,24,5,7000},
	{720,576,24,15,9000},
	{720,576,24,25,9000},
	{800,600,24,15,9000},
	{1280,720,24,15,9000}
	};

const struct QUALITY_MODEL_A CVidsendDoc2::qsa[7]= {
	{8000,8,1,2500},
	{11025,8,1,5000},
	{22050,8,1,5000},
	{22050,8,2,5000},
	{22050,16,2,5000},
	{44100,16,2,5000},
	{44100,16,2,9000}
	};

CVidsendDoc2::CVidsendDoc2() {
	char myBuf[128];
	
	prfSection="ServerStream";
	Opzioni=GetPrivateProfileInt(IDS_OPZIONI);
	OpzioniSorgenteVideo=GetPrivateProfileInt(IDS_OPZIONI3);
	OpzioniSalvaVideo=GetPrivateProfileInt(IDS_OPZIONI4);
	imposeDateTime=GetPrivateProfileInt(IDS_IMPOSEDATETIME);
	GetPrivateProfileString(IDS_IMPOSETEXT,imposeText,31);
	imposeTextPos=GetPrivateProfileInt(IDS_IMPOSETEXTPOS);
#ifdef _NEWMEET_MODE

#ifdef _CAMPARTY_MODE
	Opzioni &= (openVideoOnConnect | askOnConnect /*| 0x20000000 /*needAuthenticate=1*/ | needAuthenticateServer );
	// forse togliere needAuthenticate (i client non pagano, qua)
	// preservo le impostazioni dell'INI...
	Opzioni |= registerServer | maySendVideo | sendVideo /* | maySendAudio */;
#else
	Opzioni &= (openVideoOnConnect | askOnConnect);
	Opzioni |= 0x20000000 /*needAuthenticate=1*/ | needAuthenticateServer | registerServer | maySendVideo | sendVideo /* | maySendAudio */;
	Opzioni &= ~openVideoOnConnect;


	// mettere AUDIO!!
#endif

	OpzioniSalvaVideo=0;
	OpzioniSorgenteVideo=0;
#endif
	maxConn=GetPrivateProfileInt(IDS_MAXCONN);
	if(!maxConn)
		maxConn=2;
#ifdef _NEWMEET_MODE
#ifdef _CAMPARTY_MODE
	maxConn=5;
#else
//	maxConn=20;		// no...
#endif
#endif
	videoSource=GetPrivateProfileInt(IDS_VIDEOSOURCE);
// è in Opzioni!	videoSourceRTSP=GetPrivateProfileInt(IDS_VIDEOSOURCE2);
	GetPrivateProfileString(IDS_RTSP_SERVER,myBuf,127);
	RTSPaddress=myBuf;
	GetPrivateProfileString(IDS_RTSP_USER,myBuf,127);
	RTSPuser=myBuf;
	GetPrivateProfileString(IDS_RTSP_PASS,myBuf,127);
	RTSPpassword=myBuf;
	alternaSource=videoSource & 0x8000 ? 1 : 0;
	videoSource &= 0x7fff;
	GetPrivateProfileString(IDS_FORCEOPENWWW,myBuf,127);
	forceOpenWWW=myBuf;
	GetPrivateProfileString(IDS_AUTHWWW,myBuf,127);
	authenticationWWW=myBuf;
#ifdef _NEWMEET_MODE
#ifdef _DEBUG

	
	
//		authenticationWWW="192.168.0.43";



//	authenticationWWW="192.168.0.100";
#else
#ifdef _CAMPARTY_MODE
	authenticationWWW="www.camparty.it";
#else
	authenticationWWW="www.newmeet.com";
#endif
#endif
#endif



//	authenticationWWW="192.168.0.100";



	timedConnLenght=GetPrivateProfileTimeSpan(IDS_TIMEDCONN);
	GetPrivateProfileString(IDS_STREAM_DIRECTORYWWW,myBuf,127);
	directoryWWW=myBuf;
#ifndef _CAMPARTY_MODE
	GetPrivateProfileString(IDS_STREAM_DIRECTORYWWWLOGIN,myBuf,31);
	directoryWWWLogin=myBuf;
#else
	directoryWWWLogin="camparty";		// qui non serve, ma lo lascio...
#endif

#ifdef _NEWMEET_MODE
#ifdef _DEBUG

	
	
	
//		directoryWWW="192.168.0.43";





//	directoryWWW="192.168.0.100";
#else
#ifdef _CAMPARTY_MODE
	directoryWWW="www.camparty.it";
#else
	directoryWWW="www.newmeet.com";
#endif
#endif
//	directoryWWWLogin=info.username;
#endif


//	directoryWWW="192.168.0.100";



	GetPrivateProfileString(IDS_AVIFILE,myBuf,255);
	nomeAVI=myBuf;
	GetPrivateProfileString(IDS_AVIPATH,myBuf,255);
	pathAVI=myBuf;
	if(pathAVI.Right(1) != '\\')
		pathAVI+='\\';
	GetPrivateProfileString(IDS_AVIFILE_PB,myBuf,255);
	nomeAVI_PB=myBuf;
	GetPrivateProfileString(IDS_SUONO1,myBuf,127);
	suonoIn=myBuf;
	GetPrivateProfileString(IDS_SUONO2,myBuf,127);
	suonoOut=myBuf;
	GetPrivateProfileString(IDS_STREAMTITLE,myBuf,127);
	streamTitle=myBuf;
	theSet=NULL;
	myID=1;
	myTimedConn=0;

	theTV=NULL;

	ZeroMemory(&myQV,sizeof(myQV));
	GetPrivateProfileString(IDS_VFORMATNAME,myBuf,5);
	myQV.format=*myBuf ? *((DWORD *)myBuf) : 0L;
	GetPrivateProfileString(IDS_VCOMPRESSORNAME,myBuf,5);
	myQV.compressor=*myBuf ? *((DWORD *)myBuf) : 0L;
	myQV.fps=GetPrivateProfileInt(IDS_FRAMESPERSEC);
#ifndef _NEWMEET_MODE
	GetPrivateProfileString(IDS_COMPRESSORDIMENSIONI,myBuf,32,"160x120");
#else
	GetPrivateProfileString(IDS_COMPRESSORDIMENSIONI,myBuf,32,"320x240");
#endif
	myQV.imageSize.cx=atoi(myBuf);		// N.B. stringa vuota=crash... non ho parole!
	myQV.imageSize.cy=atoi(strchr(myBuf,'x')+1);
	myQV.bpp=24;
	myQV.quality=GetPrivateProfileInt(IDS_COMPRESSORQUALITYV);
	KFrame=GetPrivateProfileInt(IDS_KFRAME);
	ZeroMemory(&myQA,sizeof(myQA));
	myQA.compressor=GetPrivateProfileInt(IDS_ACOMPRESSORNAME);
	myQA.samplesPerSec=8000;
	myQA.bitsPerSample=8;
	myQA.channels=1;
	myQA.quality=GetPrivateProfileInt(IDS_COMPRESSORQUALITYA);
#ifdef _NEWMEET_MODE
	myQV.compressor=MAKEFOURCC('I','V','5','0');		// NO! scelti da utente!
/*	myQV.imageSize.right=320;
	myQV.imageSize.bottom=240;
	myQV.quality=7500;*/
	// impostaDefault();
	KFrame=0;


	OpzioniSorgenteVideo |= useOverlay;			// credo sia meglio cosi'... per avere il preview dei settaggi colore/luce mentre li si modifica


#endif
	GetPrivateProfileString(IDS_QUALITYBOX,myBuf,32,"0,0,0,0");
	sscanf(myBuf,"%u,%u,%u,%u",&qualityBox.top,&qualityBox.left,&qualityBox.bottom,&qualityBox.right);		// 

	theHDD=NULL;

	pagProva.tipoVideo=GetPrivateProfileInt(IDS_COMPRESSOR_TESTPAGEV);
	pagProva.tipoAudio=GetPrivateProfileInt(IDS_COMPRESSOR_TESTPAGEA);
	pagProva.audioOpzioni=GetPrivateProfileInt(IDS_COMPRESSOR_TESTPAGEOPZ);

	countFTP=GetPrivateProfileInt(IDS_COUNTFTP);
	countFTP_day=GetPrivateProfileTime(IDS_COUNTFTP_DAY);
	if(countFTP_day.GetDay() != CTime::GetCurrentTime().GetDay())
		countFTP=0;

	streamSocketV=new CStreamSrvSocket(this);
	streamSocketV->Create(VIDEO_SOCKET,theApp.Opzioni & CVidsendApp::TCP_UDP ? SOCK_DGRAM : SOCK_STREAM);
	if(!(theApp.Opzioni & CVidsendApp::TCP_UDP)) {
		streamSocketV->Listen();
		}
	else {
		}
	streamSocketA=new CStreamSrvSocket(this);
	streamSocketA->Create(AUDIO_SOCKET,theApp.Opzioni & CVidsendApp::TCP_UDP ? SOCK_DGRAM : SOCK_STREAM);
	if(!(theApp.Opzioni & CVidsendApp::TCP_UDP)) {
		streamSocketA->Listen();
		}
	else {
		}
	streamSocketA2=new CStreamSrvSocket(this,CStreamSrvSocket::MP3);		// MP3 streaming, 2023
	streamSocketA2->Create(MP3_STREAM_SOCKET,SOCK_STREAM);
	streamSocketA2->Listen();
	streamSocketA2->tag=(DWORD)lame_init();
	lame_init_params((lame_global_flags*)streamSocketA2->tag);
	id3tag_init((lame_global_flags*)streamSocketA2->tag);
	id3tag_set_title((lame_global_flags*)streamSocketA2->tag, "RadioG");
	id3tag_set_artist((lame_global_flags*)streamSocketA2->tag, "DarioG");
	id3tag_set_year((lame_global_flags*)streamSocketA2->tag, "2023");
	id3tag_add_v2((lame_global_flags*)streamSocketA2->tag);


	controlSocket=new CControlSrvSocket(this);
	controlSocket->Create(CONTROL_SOCKET);
	if(!(theApp.Opzioni & CVidsendApp::TCP_UDP)) {
		controlSocket->Listen();
		}
	else {
		}

	streamSocketVBAN=NULL;
	if(Opzioni & usaVBAN) {
		streamSocketVBAN=new CVBANSocket(this);
		struct stream_config_t stream_config;
		if(theTV) {
			stream_config.bit_fmt=theTV->wfd.wf.wBitsPerSample==16 ? VBAN_BITFMT_16_INT : VBAN_BITFMT_8_INT;
			stream_config.nb_channels=theTV->wfd.wf.nChannels;
			stream_config.sample_rate=theTV->wfd.wf.nSamplesPerSec;
			}
		else {
			stream_config.bit_fmt=VBAN_BITFMT_8_INT;
			stream_config.nb_channels=1;
			stream_config.sample_rate=22050;
			}
		streamSocketVBAN->PacketInitHeader(&stream_config, "Liverpool's GD");		//NO! va cmq fatta a ogni pacchetto

		streamSocketVBAN->Open();

		}

	authSocket=NULL;
	theTV=NULL;
	aviFile=NULL;
	psVideo=NULL,psAudio=NULL;
	gotFrame=NULL;
	PBsi=NULL;
	PBbiSrc=PBbiDest=NULL;
	trasmMode=0;
	bPaused=0;
	bAudio=Opzioni & maySendAudio;

/* da ocx...
	if(Opzioni & usaRTSP) {
		rtspSocket=new CRTSPServer(_T("stream1"));
		if(rtspSocket) {
	#define RTSP_PORT 554
			int i=rtspSocket->Create(RTSP_PORT);
			if(!i) {
				if(theApp.FileSpool) 
					theApp.FileSpool->print(CLogFile::flagError,"Impossibile installare server RTSP");
				}
			else
				i=rtspSocket->Listen();
		//										i=WSAGetLastError();
			}
		}
		*/



	pipeV = INVALID_HANDLE_VALUE;
	pipeVBuffer=NULL;


#ifndef _NEWMEET_MODE
#else

	FINIRE ::) #stuprimannone #cazziinculoallaltropedofilo

	qsv[0].imageSize.right=160;
	qsv[0].imageSize.bottom=120;
	qsv[0].bpp=24;
	qsv[0].fps=1;
	qsv[0].quality=2500;
	qsv[1].imageSize.right=160;
	qsv[1].imageSize.bottom=120;
	qsv[1].bpp=24;
	qsv[1].fps=2;
	qsv[1].quality=2500;
	qsv[2].imageSize.right=320;
	qsv[2].imageSize.bottom=240;
	qsv[2].bpp=24;
	qsv[2].fps=2;
	qsv[2].quality=5000;
	qsv[3].imageSize.right=320;
	qsv[3].imageSize.bottom=240;
	qsv[3].bpp=24;
	qsv[3].fps=2;
	qsv[3].quality=6000;
	qsv[4].imageSize.right=320;
	qsv[4].imageSize.bottom=240;
	qsv[4].bpp=24;
	qsv[4].fps=2;
	qsv[4].quality=7500;
	qsv[5].imageSize.right=320;
	qsv[5].imageSize.bottom=240;
	qsv[5].bpp=24;
	qsv[5].fps=2;
	qsv[5].quality=9000;
	qsv[6].imageSize.right=640;
	qsv[6].imageSize.bottom=480;
	qsv[6].bpp=24;
	qsv[6].fps=5;
	qsv[6].quality=9000;

	qsa[0].samplesPerSec=8000;
	qsa[0].bitsPerSample=8;
	qsa[0].channels=1;
	qsa[0].quality=2500;
	qsa[1].samplesPerSec=11025;
	qsa[1].bitsPerSample=8;
	qsa[1].channels=1;
	qsa[1].quality=5000;
	qsa[2].samplesPerSec=22050;
	qsa[2].bitsPerSample=8;
	qsa[2].channels=1;
	qsa[2].quality=5000;
#endif

	dirCliSocket=NULL;

	}

BOOL CVidsendDoc2::OnNewDocument() {
	char *p,myBuf[128];
	int i;
	
	if(!CExDocument::OnNewDocument())
		return FALSE;

	SetTitle("Video live");

	if(Opzioni & doDialUp) {
		i=theApp.callRAS((LPCTSTR)theApp.DialUpNome);
#ifdef _CAMPARTY_MODE
		if(!i)
			return FALSE;
#endif
		}

	if(Opzioni & registerServer) {
		if(!(dirCliSocket=new CDirectoryCliSocket))
			goto no_reg;
		if(!dirCliSocket->Create())
			goto no_reg;
		if(dirCliSocket->Connect(directoryWWW,DIRECTORY_SOCKET)) {
			ZeroMemory(myBuf,33);
			*myBuf='\x1';
			_tcscpy(myBuf+1,directoryWWWLogin);
			dirCliSocket->Send(myBuf,33);
			}
		else {
no_reg:
			if(theApp.FileSpool)
				*theApp.FileSpool << "Impossibile registrare server nella directory dei Server!";
			}	
		}

#if defined(_NEWMEET_MODE) || defined(_CAMPARTY_MODE)		// lascio cmq sta cagata... 2023 morte a voi 
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		char myBuf[64];
		RECT rc;
		CVidsendView2 *w=(CVidsendView2 *)getView();

		GetPrivateProfileString(IDS_COORDINATE,myBuf,32);
#ifdef _NEWMEET_MODE
#ifndef _CAMPARTY_MODE
		_tcscpy(myBuf,"0,0,326,268");
#else
		_tcscpy(myBuf,"0,0,326,268");
#endif
#endif
		SetRectEmpty(&rc);
		sscanf(myBuf,"%d,%d,%d,%d",&rc.left,&rc.top,&rc.right,&rc.bottom);	// sono coord. client rispetto alla MDIFRAME madre della mia ChildFrame
		if(!IsRectEmpty(&rc))
#ifdef _NEWMEET_MODE
			w->GetParent()->SetWindowPos(NULL,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);	// forza le dimensioni che voglio io (ossia il layout predefinito!)
#else
			w->GetParent()->SetWindowPos(NULL,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER | SWP_NOSIZE);
#endif
		// NO RESIZE! uso le dim. della finestra video!
		}
#endif

	setTXMode(0);		// salvare TXmode??

	if(((theApp.Opzioni & CVidsendDoc2::needAuthenticate) >> 29) == 2) {
		if(theSet=new CVidsendSet2(theApp.theDB)) {
			if(!theSet->Open(AFX_DB_USE_DEFAULT_TYPE,NULL,CRecordset::readOnly))
				goto no_set;
			}
		else {
no_set:
			p="Impossibile aprire database utenti!";
			AfxMessageBox(p);
			if(theApp.FileSpool)
				*theApp.FileSpool << p;
			if(theSet) {
				delete theSet;
				return FALSE;
				}
			}
		}

/*	if(Opzioni & needAuthenticateServer) {		//se aperto con autenticazione (cioe' per newmeet)
		if(!theApp.theChat) {
			theApp.OnFileNuovoChat();
			if(theApp.theChat) {
				theApp.theChat->Opzioni |= CVidsendDoc4::slaveMode;
				}
			}
		} no! lo fanno le #define */
	if(OpzioniSalvaVideo & autoAvvia) {
		theTV->startSaveFile(pathAVI+nomeAVI,OpzioniSalvaVideo);
		}
	CVidsendView2 *v=(CVidsendView2 *)getView();
	((CChildFrame2 *)v->GetParent())->setStatusIcons(this);

	if(theApp.Opzioni & CVidsendApp::namedPipes) {
		pipeV = CreateNamedPipe( 
			TEXT("\\\\.\\pipe\\videosenderpipe"),             // pipe name 
			PIPE_ACCESS_DUPLEX |       // read/write access 
			FILE_FLAG_OVERLAPPED,    // overlapped mode 
			PIPE_TYPE_MESSAGE |       // message type pipe 
			PIPE_READMODE_MESSAGE |   // message-read mode 
			PIPE_WAIT,                // blocking mode 
			PIPE_UNLIMITED_INSTANCES, // max. instances  
			PIPEBUFSIZE,                  // output buffer size 
			PIPEBUFSIZE,                  // input buffer size 
			0,                        // client time-out 
			NULL);  
		HANDLE hHeap      = GetProcessHeap();
		pipeVBuffer=(TCHAR*)HeapAlloc(hHeap, 0, PIPEBUFSIZE*sizeof(TCHAR));
		}

	theHDD=DrawDibOpen();
	DrawDibBegin(theHDD,0,myQV.imageSize.cx,myQV.imageSize.cy,&theTV->biRawBitmap,		//2019
		myQV.imageSize.cx,myQV.imageSize.cy,0);
	return TRUE;
	}

CVidsendDoc2::~CVidsendDoc2() {
	char myBuf[128];

	if(theHDD) {
		DrawDibEnd(theHDD);
		DrawDibClose(theHDD);
		}
	setTXMode(-1);
	if(theTV)
		delete theTV;
	theTV=NULL;
	if(authSocket) {
		authSocket->Close();
		delete authSocket;
		}
	authSocket=NULL;
	if(dirCliSocket) {
		dirCliSocket->Close();
		delete dirCliSocket;
		}
	dirCliSocket=NULL;
	if(theSet) {
		theSet->Close();
		delete theSet;
		}
	theSet=NULL;
	if(controlSocket) {
		controlSocket->Close();
		delete controlSocket;
		}
	controlSocket=NULL;
	if(streamSocketA) {
		streamSocketA->Close();
		delete streamSocketA;
		}
	streamSocketA=NULL;
	if(streamSocketA2) {
		streamSocketA2->Close();
		if(streamSocketA2->tag) {
			free_id3tag(((lame_global_flags *)streamSocketA2->tag)->internal_flags);
			lame_close((lame_global_flags *)streamSocketA2->tag);
			}
		delete streamSocketA2;
		}
	streamSocketA2=NULL;

	if(streamSocketV) {
		streamSocketV->Close();
		delete streamSocketV;
		}
	streamSocketV=NULL;
	if(streamSocketVBAN) {
		streamSocketVBAN->Close();
		delete streamSocketVBAN;
		}
	streamSocketVBAN=NULL;
	save();
  WritePrivateProfileInt(IDS_MAXCONN,maxConn);
  WritePrivateProfileInt(IDS_OPZIONI,Opzioni);
  WritePrivateProfileInt(IDS_OPZIONI3,OpzioniSorgenteVideo);
  WritePrivateProfileInt(IDS_OPZIONI4,OpzioniSalvaVideo);
  WritePrivateProfileInt(IDS_VIDEOSOURCE,videoSource | (alternaSource ? 0x8000 : 0));
	}


BOOL CVidsendDoc2::save() {
	char myBuf[64];
	int i;

	*((DWORD *)myBuf)=myQV.format;
	myBuf[4]=0;
	WritePrivateProfileString(IDS_VFORMATNAME,myBuf);
	*((DWORD *)myBuf)=myQV.compressor;
	myBuf[4]=0;
	WritePrivateProfileString(IDS_VCOMPRESSORNAME,myBuf);
	WritePrivateProfileInt(IDS_FRAMESPERSEC,myQV.fps);
	wsprintf(myBuf,"%ux%u",myQV.imageSize.cx,myQV.imageSize.cy);
	WritePrivateProfileString(IDS_COMPRESSORDIMENSIONI,myBuf);
	WritePrivateProfileInt(IDS_KFRAME,KFrame);
	wsprintf(myBuf,"%d",myQA.compressor);
	WritePrivateProfileString(IDS_ACOMPRESSORNAME,myBuf);
	WritePrivateProfileInt(IDS_COMPRESSORQUALITYV,myQV.quality);
	WritePrivateProfileInt(IDS_COMPRESSORQUALITYA,myQA.quality);
	WritePrivateProfileInt(IDS_COMPRESSOR_TESTPAGEV,pagProva.tipoVideo);
	WritePrivateProfileInt(IDS_COMPRESSOR_TESTPAGEA,pagProva.tipoAudio);
	WritePrivateProfileInt(IDS_COMPRESSOR_TESTPAGEOPZ,pagProva.audioOpzioni);
	WritePrivateProfileString(IDS_FORCEOPENWWW,(LPSTR)(LPCTSTR)forceOpenWWW);
	WritePrivateProfileString(IDS_AUTHWWW,(LPSTR)(LPCTSTR)authenticationWWW);
	WritePrivateProfileTime(IDS_TIMEDCONN,timedConnLenght);
	WritePrivateProfileString(IDS_STREAM_DIRECTORYWWW,(LPSTR)(LPCTSTR)directoryWWW);
	WritePrivateProfileString(IDS_STREAM_DIRECTORYWWWLOGIN,(LPSTR)(LPCTSTR)directoryWWWLogin);
	WritePrivateProfileString(IDS_AVIPATH,(LPSTR)(LPCTSTR)pathAVI);
	WritePrivateProfileString(IDS_AVIFILE,(LPSTR)(LPCTSTR)nomeAVI);
	WritePrivateProfileString(IDS_SUONO1,(LPSTR)(LPCTSTR)suonoIn);
	WritePrivateProfileString(IDS_SUONO2,(LPSTR)(LPCTSTR)suonoOut);
	WritePrivateProfileString(IDS_STREAMTITLE,(LPSTR)(LPCTSTR)streamTitle);
	WritePrivateProfileInt(IDS_COUNTFTP,countFTP);
	WritePrivateProfileTime(IDS_COUNTFTP_DAY,countFTP_day);
  WritePrivateProfileInt(IDS_IMPOSEDATETIME,imposeDateTime);
  WritePrivateProfileString(IDS_IMPOSETEXT,imposeText);
  WritePrivateProfileInt(IDS_IMPOSETEXTPOS,imposeTextPos);
	WritePrivateProfileString(IDS_AVIFILE_PB,(LPSTR)(LPCTSTR)nomeAVI_PB);
	WritePrivateProfileString(IDS_RTSP_SERVER,(LPSTR)(LPCTSTR)RTSPaddress);
	WritePrivateProfileString(IDS_RTSP_USER,(LPSTR)(LPCTSTR)RTSPuser);
	WritePrivateProfileString(IDS_RTSP_PASS,(LPSTR)(LPCTSTR)RTSPpassword);
	
	

	wsprintf(myBuf,"%u,%u,%u,%u",qualityBox.top,qualityBox.left,qualityBox.bottom,qualityBox.right);
	WritePrivateProfileString(IDS_QUALITYBOX,myBuf);

	return 1;
	}

int CVidsendDoc2::impostaVideoSource(int m) {
	static int oldm;

	if(theTV) {
		if(m<0) {
			m=oldm;
			oldm++;
			if(oldm>2)
				oldm=0;
			}
		else if(m==oldm)
			goto fine;
		PostMessage(theTV->GetHwnd(),WM_CAP_DLG_VIDEOSOURCE,0,0);
		keybd_event(m==2 ? 'V' : (m ? 'C' : 'T'),0,0,0);
		keybd_event(VK_RETURN,0,0,0);
//		m_pMainWnd->PostMessage(/*WM_CHAR*/WM_KEYDOWN,m==2 ? 'v' : (m ? 'c' : 't'),1);
//		m_pMainWnd->PostMessage(/*WM_CHAR*/WM_KEYUP,m==2 ? 'v' : (m ? 'c' : 't'),1);
//		m_pMainWnd->PostMessage(WM_KEYDOWN,VK_RETURN,1);
//		m_pMainWnd->PostMessage(WM_KEYUP,VK_RETURN,1);
//		capDlgVideoSource(theTV);
		}
fine:
	return 0;
	}

int CVidsendDoc2::acceptConnect(const char *who) {
	CString S;
	static BOOL inUse;

	if(!suonoIn.IsEmpty()) {
		sndPlaySound(suonoIn,SND_ASYNC);
		}
	if(Opzioni & askOnConnect) {
		if(inUse)
			return 0;
		inUse=1;
		S.Format("Accettare la connessione entrante (da %s )?",who ? who : "<sconosciuto>");
		if(AfxMessageBox(S,MB_YESNO | MB_ICONQUESTION) == IDYES) {
			if(Opzioni & openVideoOnConnect && !theTV)
				openVideo((CVidsendView2 *)getView());
			inUse=0;
			}
		else {
			inUse=0;
			return 0;
			}
		}
	return 1;
	}

int CVidsendDoc2::openVideo(CVidsendView2 *v) {
	CChildFrame2 *w;
	WAVEFORMATEX myWf;

	myWf.wFormatTag = WAVE_FORMAT_PCM;
	myWf.nChannels =  1;
	myWf.nSamplesPerSec = 22050;
	myWf.wBitsPerSample = 8;
	myWf.nBlockAlign= (myWf.nChannels * myWf.wBitsPerSample)/8;
	myWf.nAvgBytesPerSec = (myWf.nSamplesPerSec * myWf.nChannels * myWf.wBitsPerSample)/8;
	myWf.cbSize = 0 /*sizeof(WAVEFORMATEX)*/;

	theApp.theServer=this;		// mi serve in CTV!! (v. anche main OpenDocument())
	theTV=new CTV(v,OpzioniSorgenteVideo & CVidsendDoc2::useOverlay,&myQV.imageSize,
		24,myQV.fps,myQV.format,myQV.compressor,KFrame,Opzioni & CVidsendDoc2::maySendAudio,&myWf);
#ifdef _CAMPARTY_MODE				// per ora solo qua, ma potrebbe avere senso anche agli altri
	// myQV viene salvato...
#endif

//		GetParent()->SendMessage(WM_MOVE,d->theTV->GetXSize(),d->theTV->GetYSize());
	if(w=(CChildFrame2 *)v->GetParent())
		w->SetWindowPos(NULL,0,0,theTV->GetXSize()+GetSystemMetrics(SM_CXEDGE)*3,theTV->GetYSize()+GetSystemMetrics(SM_CYEDGE)*5+GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYCAPTION),SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
		/* v. anche MinMaxInfo di ChildFrame2... */
	theTV->setSuperImposeDateTime(imposeDateTime);
	theTV->setSuperImposeText(imposeText,imposeTextPos);
	theTV->setOpzioni(OpzioniSalvaVideo);
	if(Opzioni & CVidsendDoc2::sendVideo) {
		/*i=*/theTV->Capture(1);
//				d->impostaVideoSource(d->videoSource);
		// fa delle cagate se un'altra finestra si apre in contemporanea... SISTEMARE!!


		if(Opzioni & usaVBAN) {
			if(streamSocketVBAN)
				streamSocketVBAN->Close();
			struct stream_config_t stream_config;
			stream_config.bit_fmt=theTV->wfd.wf.wBitsPerSample==16 ? VBAN_BITFMT_16_INT : VBAN_BITFMT_8_INT;
			stream_config.nb_channels=theTV->wfd.wf.nChannels;
			stream_config.sample_rate=theTV->wfd.wf.nSamplesPerSec;
			streamSocketVBAN->PacketInitHeader(&stream_config, "Liverpool's GD");
//			streamSocketVBAN->Open("255.255.255.255");
			streamSocketVBAN->Open("192.168.1.104");
			}

		}
	if(w)
		w->setStatusIcons(this);
	return 1;
	}



BEGIN_MESSAGE_MAP(CVidsendDoc2, CExDocument)
	//{{AFX_MSG_MAP(CVidsendDoc2)
	ON_COMMAND(ID_FILE_PROPRIETA, OnFileProprieta)
	ON_COMMAND(ID_VIDEO_LIVELLIVOLUME, OnVideoLivellivolume)
	ON_COMMAND(ID_CONNESSIONE_RIPRENDI, OnVideoTrasmissioneRiprendi)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_RIPRENDI, OnUpdateVideoTrasmissioneRiprendi)
	ON_COMMAND(ID_CONNESSIONE_PAUSA, OnVideoTrasmissionePausa)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_PAUSA, OnUpdateVideoTrasmissionePausa)
	ON_COMMAND(ID_VIDEO_AUDIO, OnVideoAudio)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_AUDIO, OnUpdateVideoAudio)
	ON_COMMAND(ID_FILE_SAVE_FOTOGRAMMA, OnFileSaveFotogramma)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_FOTOGRAMMA, OnUpdateFileSaveFotogramma)
#ifndef _NEWMEET_MODE
	ON_COMMAND(ID_VIDEO_TRASMISSIONE_DALVIVO, OnVideoTrasmissioneDalvivo)
	ON_COMMAND(ID_VIDEO_TRASMISSIONE_FILMATO, OnVideoTrasmissioneFilmato)
	ON_COMMAND(ID_VIDEO_TRASMISSIONE_PAGINADIPROVA, OnVideoTrasmissionePaginadiprova)
	ON_COMMAND(ID_VIDEO_TRASMISSIONE_CLIENTVIDEOPROXY, OnVideoTrasmissioneClientVideoProxy)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_PAGINADIPROVA, OnUpdateVideoTrasmissionePaginadiprova)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_FILMATO, OnUpdateVideoTrasmissioneFilmato)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_DALVIVO, OnUpdateVideoTrasmissioneDalvivo)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_CLIENTVIDEOPROXY, OnUpdateVideoTrasmissioneClientVideoProxy)
#endif
	ON_COMMAND(ID_VIDEO_INFORMAZIONI, OnVideoInformazioni)
	ON_COMMAND(ID_FILE_SAVE_VIDEO, OnFileSaveVideo)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_VIDEO, OnUpdateFileSaveVideo)
	ON_COMMAND(ID_FILE_SAVE_FOTOGRAMMA2, OnFileSaveFotogramma2)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_FOTOGRAMMA2, OnUpdateFileSaveFotogramma2)
	ON_COMMAND(ID_FILE_ARCHIVIOIMMAGINI, OnFileArchivioimmagini)
	ON_UPDATE_COMMAND_UI(ID_FILE_ARCHIVIOIMMAGINI, OnUpdateFileArchivioimmagini)
	ON_COMMAND(ID_FILE_PRINT, OnFilePrint)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2 diagnostics

#ifdef _DEBUG
void CVidsendDoc2::AssertValid() const
{
	CExDocument::AssertValid();
}

void CVidsendDoc2::Dump(CDumpContext& dc) const
{
	CExDocument::Dump(dc);
}
#endif //_DEBUG


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2 commands

void CVidsendDoc2::OnFileProprieta() {
	int i;

#ifndef _NEWMEET_MODE
	CVidsendPropPage mySheet("Proprietà streaming",(CVidsendView2 *)getView());
	CVidsendDoc2PropPage0 myPage0(this,&myQV,&myQA);
	CVidsendDoc2PropPage0Bis myPage0bis(this,&myQV,&myQA);
	CVidsendDoc2PropPage1 myPage1(this);
	CVidsendDoc2PropPage2 myPage2(this);
	CVidsendDoc2PropPage3 myPage3(this);
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		if(0)
			goto fine;
		}
//	if(theApp.Opzioni & CVidsendApp::advancedConf)		// per ora no...
		mySheet.AddPage(&myPage0);
//	else
//		mySheet.AddPage(&myPage0bis);
	mySheet.AddPage(&myPage1);
	mySheet.AddPage(&myPage2);
	mySheet.AddPage(&myPage3);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
		if(myPage0.isInitialized) {
			Opzioni &= 0xffff0000;
			Opzioni |= myPage0.m_ServerStream ? sendVideo : 0;
			Opzioni |= myPage0.m_ServerVideo ? maySendVideo : 0;
			Opzioni |= myPage0.m_ServerAudio ? maySendAudio : 0;
			Opzioni |= myPage0.m_TipoVideo ? videoType : 0;
// in prop video src			Opzioni |= myPage0.m_TipoVideo ? usaRTSP : 0;
			Opzioni |= myPage0.m_VBAN ? usaVBAN : 0;

//	if(!theApp.Opzioni & CVidsendApp::advancedConf)		// per ora no...
//			theTV->theCapture=myPage0bis.m_Schede-1;

//			if(theApp.Opzioni & CVidsendApp::advancedConf) {
				myQV.format=myPage0.m_FormatV;
				myQV.compressor=myPage0.m_CompressorV;
//				myQV.format=myPage0bis.m_QV.format;
//				myQV.compressor=myPage0bis.m_QV.compressor;
				myQV.imageSize=myPage0.m_QV.imageSize;
				myQV.fps=myPage0.m_QV.fps;
				myQV.quality=myPage0.m_QV.quality;
				myQA.compressor=myPage0.m_CompressorA;
				myQA.quality=myPage0.m_QA.quality;
//				}
			}

		if(myPage1.isInitialized) {
			Opzioni &= 0xff00ffff;
			Opzioni |= myPage1.m_Forza_BN ? forceBN : 0;
			Opzioni |= myPage1.m_Capovolgi ? doFlip : 0;
			Opzioni |= myPage1.m_Specchio ? doMirror : 0;
			imposeDateTime = myPage1.m_ImposeDateTime;
			imposeTextPos = myPage1.m_ImposeTextPos;
			_tcscpy(imposeText,"dario");  //  ****FINIRE
			}

		if(myPage2.isInitialized) {
			Opzioni &= 0x00ffffff;
			Opzioni |= (myPage2.m_TipoAutorizzazione) << 29;
			Opzioni |= myPage2.m_bDirectoryServer ? registerServer : 0;
			Opzioni |= myPage2.m_bNeedAuthenticate ? needAuthenticateServer : 0;
			maxConn=myPage2.m_MaxConn;
			authenticationWWW=myPage2.m_AuthWWW;
			directoryWWW=myPage2.m_DirectoryServer;
			directoryWWWLogin=myPage2.m_NomePerServer;
			}
		if(myPage3.isInitialized) {
			forceOpenWWW=myPage3.m_OpenWWW;
			Opzioni &= 0xf00fffff;
			Opzioni |= myPage3.m_bOpenWWW ? openWWW : 0;
			Opzioni |= myPage3.m_bTimedConn ? timedConnection : 0;
			Opzioni |= myPage3.m_DontSave ? dontSave : 0;
			Opzioni |= myPage3.m_ActivateIf ? openVideoOnConnect : 0;
			Opzioni |= myPage3.m_ActivateWaitConfirm ? askOnConnect : 0;
			Opzioni |= myPage3.m_DialUp ? doDialUp : 0;
			theApp.DialUpNome=myPage3.m_DialUpNome;
			{ CTimeSpan myTS(0,myPage3.m_TimedConn.GetHour(),myPage3.m_TimedConn.GetMinute(),0);
				timedConnLenght=myTS;
			}
			suonoIn=myPage3.m_SuonoIn;
			suonoOut=myPage3.m_SuonoOut;
			streamTitle=myPage3.m_StreamTitle;
			}
		}

#else
	
	CVidsendPropPage mySheet("Proprietà streaming",(CVidsendView2 *)getView());
	CVidsendDoc2PropPage0Bis myPage0bis(this,&myQV,&myQA);
	CVidsendDoc2PropPage1_NM myPage1(this);
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		if(0)
			goto fine;
		}
	mySheet.AddPage(&myPage0bis);
	mySheet.AddPage(&myPage1);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
		if(myPage0bis.isInitialized) {
			if(myPage0bis.m_ServerAudio)
				Opzioni |= maySendAudio;
			else
				Opzioni &= ~maySendAudio;
			myQV.format=myPage0bis.m_QV.format;
			myQV.compressor=myPage0bis.m_QV.compressor;
			myQV.imageSize=myPage0bis.m_QV.imageSize;
			myQV.fps=myPage0bis.m_QV.fps;
			myQV.quality=myPage0bis.m_QV.quality;
			myQA.compressor=myPage0bis.m_QA.compressor;
			myQA.quality=myPage0bis.m_QV.quality;
			theTV->theCapture=myPage0bis.m_Schede-1;
/*			Opzioni |= myPage0.m_TipoVideo ? videoType : 0;
				myQV.compressor=myPage0.m_CompressorV;
				myQV.imageSize=myPage0.m_QV.imageSize;
				myQV.fps=myPage0.m_QV.fps;
				myQV.quality=myPage0.m_QV.quality;
				myQA.compressor=myPage0.m_CompressorA;
				myQA.quality=myPage0.m_QA.quality;
				*/
			}

		if(myPage1.isInitialized) {
			Opzioni &= 0xff00ffff;
			Opzioni |= myPage1.m_Forza_BN ? forceBN : 0;
			Opzioni |= myPage1.m_Capovolgi ? doFlip : 0;
			Opzioni |= myPage1.m_Specchio ? doMirror : 0;
			imposeDateTime = myPage1.m_ImposeDateTime;
			}

		AfxMessageBox("Tenere presente che, in molti casi, è necessario impostare la webcam agli stessi valori indicati in Impostazioni Streaming\nRiavviare il programma per rendere effettive le modifiche!",MB_ICONEXCLAMATION);
		}
#endif

fine:
		;



	}



void CVidsendDoc2::OnCloseDocument() {

	if(Opzioni & needAuthenticateServer) {		//se aperto con autenticazione (cioe' per newmeet)
		if(theApp.theChat) {
			theApp.theChat->OnCloseDocument();
			}
		}

	if(pipeV != INVALID_HANDLE_VALUE) {
		FlushFileBuffers(pipeV); 
		DisconnectNamedPipe(pipeV); 
		CloseHandle(pipeV); 
		}

	CExDocument::OnCloseDocument();
	theApp.theServer=NULL;
	}

void CVidsendDoc2::OnFileSaveFotogramma() {
	DWORD l,len;
	CBitmap b;
	CVidsendView2 *v=(CVidsendView2 *)getView();
	CFileDialog myDlg(FALSE,"*.jpg",NULL,OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,"File JPEG (*.jpg)|*.jpg|File Bitmap (*.bmp)|*.bmp|Tutti i file (*.*)|*.*||",v);
	CStringEx S;
	
	theTV->theFrame=(BYTE *)-1;
	if(myDlg.DoModal() == IDOK) {
		if(theTV->theFrame && theTV->theFrame!=(BYTE *)-1) {
			l=theTV->biRawDef.bmiHeader.biWidth*theTV->biRawDef.bmiHeader.biHeight*3;
			CFile mF;
			S=myDlg.GetFileName();
			if(S.FindNoCase("jpg") != -1) {
				CJpeg myJPEG;
				BYTE *p;
				if(b.CreateBitmap(theTV->biRawDef.bmiHeader.biWidth,theTV->biRawDef.bmiHeader.biHeight,1,24,NULL)) {
					if(b.SetBitmapBits(l,theTV->theFrame)) {
						if(p=myJPEG.buildJPEG(&b,&len,TRUE)) {
							if(mF.Open(myDlg.GetPathName(),CFile::modeCreate | CFile::modeReadWrite | CFile::typeBinary | CFile::shareDenyWrite)) {
								mF.Write(p,len);
								mF.Close();
								}
							HeapFree(GetProcessHeap(),0,p);
							return;
							}
						}
					}
				}
			else if(S.FindNoCase("bmp") != -1) {
				BITMAPFILEHEADER bh;
				BITMAPINFO bi=theTV->biRawDef;
				if(mF.Open(myDlg.GetPathName(),CFile::modeCreate | CFile::modeReadWrite | CFile::typeBinary | CFile::shareDenyWrite)) {
					bh.bfType='MB';
					bh.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+l;
					bh.bfReserved1=bh.bfReserved2=0;
					bh.bfOffBits=sizeof(BITMAPINFOHEADER)+sizeof(BITMAPFILEHEADER);
					bi.bmiHeader.biSizeImage=l;
					mF.Write(&bh,sizeof(BITMAPFILEHEADER));
					mF.Write(&bi.bmiHeader,sizeof(BITMAPINFOHEADER));
					mF.Write(theTV->theFrame,l);
					mF.Close();
					return;
					}
				}
			else
				AfxMessageBox("Tipo file non valido");
			}
		AfxMessageBox("Impossibile salvare l'immagine!");
		}
	if(theTV->theFrame && theTV->theFrame != (BYTE *)-1) {
		HeapFree(GetProcessHeap(),0,theTV->theFrame);
#pragma warning {VERIFICARE come e allocata questa memoria...
		theTV->theFrame=NULL;
		}
	
	}

void CVidsendDoc2::OnUpdateFileSaveFotogramma(CCmdUI* pCmdUI) {

	pCmdUI->Enable(theTV->inCapture > 0);
	}


void CVidsendDoc2::OnFileSaveFotogramma2() {
	CSalvaFTPDlg myDlg;
	CFTPclient myFTP;
//	CStdioFile myFile;
	int m,i,j,retVal=0;
	char myBuf[256];
	CString S,serverFTP,S1;
	DWORD ti;
	CTimedMessageBox myDlg2;

//  GetPrivateProfileString(IDS_SERVERFTP,myBuf,63);
//	serverFTP=myBuf;
	
	if(countFTP >= MAX_FTP_A_DAY) {
		AfxMessageBox("Superato il massimo numero di immagini consentito!",MB_ICONEXCLAMATION);
		return;
		}

	theTV->theFrame=(BYTE *)-1;

	ti=timeGetTime()+4000;
	while(timeGetTime()<ti && theTV->theFrame==(BYTE *)-1) {
		MSG msg;
		if(::PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
			if(!theApp.PumpMessage()) { 
				}
			}
		}
	if(theTV->theFrame==(BYTE *)-1) {
		AfxMessageBox("Impossibile salvare un'immagine",MB_OK | MB_ICONSTOP);
		goto fine;
		}
	if((m=myDlg.DoModal(&theTV->biBaseRawBitmap,theTV->theFrame)) != IDCANCEL) {

		countFTP++;

		S="Salvataggio immagine in corso su ";
		S+="server";		//		S+=authSocket->FTPserver;		// no, per sicurezza server... 2/4/03
		myDlg2.DoModeless("Salvataggio immagine",S,MB_OK);

		i=myFTP.LogOnToServer(authSocket->FTPserver,21,authSocket->FTPlogin /*myLogin*/,authSocket->FTPpassword /*myPassword*/);
		if(theApp.debugMode) 
			if(theApp.FileSpool)
				theApp.FileSpool->print(2,"FTP logon: %s",(LPCTSTR)myFTP.m_retmsg);
		if(i) {
			i=myFTP.ChDir("archivio");
			if(theApp.debugMode) 
				if(theApp.FileSpool)
					theApp.FileSpool->print(2,"FTP chdir: %s",(LPCTSTR)myFTP.m_retmsg);
			if(!i)
				goto fine;
			j=0;


#ifdef DARIO				// vecchia, fino a 28/3/03
rifo:
			S.Format("%u",myID);
			i=myFTP.ChDir(S);
			if(theApp.debugMode)
				if(theApp.FileSpool)
					theApp.FileSpool->print(2,"FTP chdir: %s",(LPCTSTR)myFTP.m_retmsg);
			if(!i) {
				if(j)
					goto fine;
				else {
					i=myFTP.MkDir(S);
					if(theApp.debugMode)
						if(theApp.FileSpool)
							theApp.FileSpool->print(2,"FTP mkdir: %s",(LPCTSTR)myFTP.m_retmsg);
					if(!i)
						goto fine;
					j++;
					goto rifo;
					}
				}
			if(myDlg.returnValue == 1) {
				j=0;
rifo2:
				i=myFTP.ChDir("special");
				if(theApp.debugMode)
					if(theApp.FileSpool)
						theApp.FileSpool->print(2,"FTP chdir: %s",(LPCTSTR)myFTP.m_retmsg);
				if(!i) {
					if(j)
						goto fine;
					else {
						i=myFTP.MkDir("special");
					if(theApp.debugMode) 
						if(theApp.FileSpool)
							theApp.FileSpool->print(2,"FTP mkdir: %s",(LPCTSTR)myFTP.m_retmsg);
						if(!i)
							goto fine;
						j++;
						goto rifo2;
						}
					}
				}
#endif


			S1.Format("_%u",myID);
			S=CTime::GetCurrentTime().Format("%y%m%d_%H%M%S");
			S+=S1;
			if(myDlg.returnValue == 1)
				S+="_s";
			S+=".jpg";



/*			myFTP.GetList("c:\\listftp.txt");
			j=0;
			if(!myFile.Open("c:\\listftp.txt",CStdioFile::modeRead))
				goto fine;
			while(myFile.ReadString(myBuf,255)) {
				char a1[32],a2[32],a3[32],a4[32],a5[32],a6[32],a7[32],a8[32],a9[32];
//				sscanf(myBuf,"%s %s %s %s %s %s %s %s %s",a1,a2,a3,a4,a5,a6,a7,a8,a9);	// questo era OK con supereva (che sembra + standard...)
				sscanf(myBuf,"%s %s %s %s ",a1,a2,a3,a9);		// questo con newmeet.com...
//				theApp.FileSpool->print(2,"  -- trovo %s >%s<",myBuf,a9);
				if(*a1 != 'd') {
					i=atoi(a9+4);
					if(i>j)
						j=i;
					}
				}
			myFile.Close();
			CFile::Remove("c:\\listftp.txt");
			j++;

			S.Format("file%04u.jpg",j);*/

			{
				CJpeg *myJPEG;
				CBitmap b;
				BYTE *p;
				DWORD len;
				DWORD l=theTV->biRawDef.bmiHeader.biWidth*theTV->biRawDef.bmiHeader.biHeight*3;
				if(b.CreateBitmap(theTV->biRawDef.bmiHeader.biWidth,theTV->biRawDef.bmiHeader.biHeight,1,24,NULL)) {
					if(b.SetBitmapBits(l,theTV->theFrame)) {
//						S=CTime::GetCurrentTime().Format("%Y%m%d_%H%M%S.jpg");
						myJPEG=new CJpeg;
						if(myJPEG) {
							CTV::superImposeText(&b,"(C) NewMeet.com");
							if(p=myJPEG->buildJPEG(&b,&len,TRUE)) {
								myFTP.SendBuff(S,p,len);
								retVal=1;
								}
							HeapFree(GetProcessHeap(),0,p);
							delete myJPEG;
							}

/*						S=CTime::GetCurrentTime().Format("_%Y%m%d_%H%M%S.jpg");
						myJPEG=new CJpeg;
						if(myJPEG) {
							CTV::superImposeText(&b,"(C) NewMeet.com");
							if(p=myJPEG->buildJPEG(&b,&len,TRUE,10)) {
								myFTP.SendBuff(S,p,len);
								retVal=1;
								}
							delete myJPEG;
							}*/
						}
					}
				}
fine:
			if(theApp.debugMode) 
				if(theApp.FileSpool)
					theApp.FileSpool->print(CLogFile::flagInfo,"FTP send: %s",(LPCTSTR)myFTP.m_retmsg);
			myFTP.LogOffServer();
			}
		if(theTV->theFrame && theTV->theFrame != (BYTE *)-1) {
#pragma warning { verificare...
			HeapFree(GetProcessHeap(),0,theTV->theFrame);
			theTV->theFrame=NULL;
			}
		
		myDlg2.EndModeless();

		if(retVal>0)
			AfxMessageBox("Immagine correttamente salvata con il nome "+S,MB_OK);
		else
			AfxMessageBox("Impossibile salvare l'immagine!",MB_OK | MB_ICONSTOP);
		}

	}

void CVidsendDoc2::OnUpdateFileSaveFotogramma2(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(Opzioni & needAuthenticateServer);
	}

void CVidsendDoc2::OnFileSaveVideo() {
	CVidsendView2 *v=(CVidsendView2 *)getView();
	
	if(theTV->isRecordingVideo())
		theTV->endSaveFile();
	else {
		CSalvaVideoDlg myDlg(this);
		if(myDlg.DoModal() == IDOK) {
			OpzioniSalvaVideo = myDlg.m_QuandoCancello;
			OpzioniSalvaVideo |= myDlg.m_AutoAvvia ? autoAvvia : 0;
			OpzioniSalvaVideo |= myDlg.m_QuantiFrame ? quantiFrame : 0;
			OpzioniSalvaVideo |= myDlg.m_QuantiFile ? quantiFile : 0;
			OpzioniSalvaVideo |= myDlg.m_Accoda ? accoda : 0;
			if(OpzioniSalvaVideo & quantiFile) {
				pathAVI=myDlg.m_salvaPath;
				nomeAVI=theApp.creaStringaDaGiorno();
				}
			else {
				int i=myDlg.m_salvaPath.ReverseFind('\\');
				if(i>0) {
					nomeAVI=myDlg.m_salvaPath.Mid(i+1);
					pathAVI=myDlg.m_salvaPath.Left(i);
					}
				else {
					nomeAVI=myDlg.m_salvaPath;
					pathAVI="";
					}
				if(nomeAVI.Find('.') == -1)
					nomeAVI+=".avi";
				}
			if(pathAVI.Right(1) != '\\')
				pathAVI+='\\';
			theTV->setOpzioni(OpzioniSalvaVideo);
			theTV->startSaveFile(pathAVI+nomeAVI,OpzioniSalvaVideo);
			}
		}
	((CChildFrame2 *)v->GetParent())->setStatusIcons();
	}

void CVidsendDoc2::OnUpdateFileSaveVideo(CCmdUI* pCmdUI) {
	pCmdUI->Enable(theTV ? 1 : 0)	;
	pCmdUI->SetCheck(theTV->isRecordingVideo() ? 1 : 0)	;
	}



void CVidsendDoc2::OnFileArchivioimmagini() {
	// TODO: Add your command handler code here
	
	}

void CVidsendDoc2::OnUpdateFileArchivioimmagini(CCmdUI* pCmdUI) {
	// TODO: Add your command update UI handler code here
	
	}

void CVidsendDoc2::OnFilePrint() {
  CMyPrintDialog myDlg;	
	int i,j,fAbort;
	char myBuf[256],myBuf1[128];
	BYTE *p,*p2;
	DWORD ti;
//	CPrintInfo pI;
	HDC hDC=NULL,hDC1;
	HBITMAP hBmp,hOldbmp;
	HWND hwndAbort;
	DEVMODE *lpdm;
	RECT rc,rc2;
	DOCINFO dInfo;

	if(myDlg.DoModal() == IDOK) {
		hDC=myDlg.GetPrinterDC();
		lpdm=myDlg.GetDevMode();
		rc.top=rc.left=0;
		rc.right=lpdm->dmPaperWidth;
		rc.bottom=lpdm->dmPaperLength;
		rc.right=GetDeviceCaps(hDC,HORZRES);
		rc.bottom=GetDeviceCaps(hDC,VERTRES);
		BeginWaitCursor();
		_tcscpy(myBuf,"Immagine Video live ");
		if(theTV)
			wsprintf(myBuf1,"(VideoSender %ux%u)",theTV->biRawBitmap.biWidth,theTV->biRawBitmap.biHeight);
		else
			_tcscpy(myBuf1,"(VideoSender)");
		strcat(myBuf,myBuf1);
		dInfo.cbSize=sizeof(DOCINFO);
		dInfo.lpszDocName=myBuf;
		dInfo.lpszOutput=NULL;
		dInfo.lpszDatatype=NULL;
		dInfo.fwType=0;

		fAbort = FALSE;
		hwndAbort = CreateDialog(theApp.m_hInstance,MAKEINTRESOURCE(IDD_ABORTPRINT),getView()->m_hWnd,(DLGPROC)AbortDlgProc);
		EnableWindow(getView()->m_hWnd,FALSE);
		i=SetAbortProc(hDC,AbortProc);
		if((i=StartDoc(hDC,&dInfo)) >0 ) {

			if(theTV) {
				wsprintf(myBuf,"Caricamento in corso...");
				SetDlgItemText(hwndAbort,IDC_TEXT1,myBuf);

				theTV->theFrame=(BYTE *)-1;

				ti=timeGetTime()+4000;
				while(timeGetTime()<ti && theTV->theFrame==(BYTE *)-1) {
					MSG msg;
					if(::PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
						if(!theApp.PumpMessage()) { 
							}
						}
					}
				if(theTV->theFrame==(BYTE *)-1) {
					AfxMessageBox("Impossibile stampare un'immagine",MB_OK | MB_ICONSTOP);
					goto fine;
					}
				CBitmap b;
				BYTE *p;
				DWORD len;
				DWORD l=theTV->biRawDef.bmiHeader.biWidth*theTV->biRawDef.bmiHeader.biHeight*3;

				if(myDlg.m_SuperImpose) {
					}

				if(b.CreateBitmap(theTV->biRawDef.bmiHeader.biWidth,theTV->biRawDef.bmiHeader.biHeight,1,24,NULL)) {
					if(b.SetBitmapBits(l,theTV->theFrame)) {

						switch(myDlg.m_Size) {
							case 0:
								rc2.top=rc2.left=0;
								rc2.right=((rc.right*19)/20)/4;		// un po' meno di meta'
								rc2.bottom=(rc2.right*3)/4  /*rc.bottom/2*/;
								break;
							case 1:
								rc2.top=rc2.left=0;
								rc2.right=((rc.right*19)/20)/2;		// un po' meno 
								rc2.bottom=(rc2.right*3)/4  /*rc.bottom/2*/;
								break;
							case 2:
								rc2.top=rc2.left=0;
								rc2.right=((rc.right*19*3)/(20*4));		// un po' meno 
								rc2.bottom=(rc2.right*3)/4  /*rc.bottom/2*/;
								break;
							case 3:
								rc2.top=rc2.left=0;
								rc2.right=((rc.right*19)/20);		// un po' meno 
								rc2.bottom=(rc2.right*3)/4  /*rc.bottom/2*/;
								break;
							}

						wsprintf(myBuf,"Stampa in corso...");
						SetDlgItemText(hwndAbort,IDC_TEXT1,myBuf);
						if(StartPage(hDC) >0 ) {
							hDC1=CreateCompatibleDC(hDC);
							hBmp=CreateDIBSection(hDC1,&theTV->biRawDef,DIB_RGB_COLORS,(void **)&p,NULL,NULL);
							memcpy(p,theTV->theFrame,theTV->biRawDef.bmiHeader.biSizeImage);
							hOldbmp=(HBITMAP)SelectObject(hDC1,hBmp /*(HBITMAP)b*/);
							j=StretchBlt(hDC,25,25 /*rc2.bottom*/,rc2.right, /* - */ rc2.bottom,hDC1,0,0,theTV->biRawDef.bmiHeader.biWidth,theTV->biRawDef.bmiHeader.biHeight,SRCCOPY);
							SelectObject(hDC1,hOldbmp);
							DeleteObject(hBmp);
							DeleteDC(hDC1);
							EndPage(hDC);
							}
						}
					}

				}
			EndDoc(hDC);
			}

		if(theTV) {
			if(theTV->theFrame && theTV->theFrame != (BYTE *)-1) {
#pragma warning {verificare...
				HeapFree(GetProcessHeap(),0,theTV->theFrame);
				theTV->theFrame=NULL;
				}
			}

		if(!fAbort) {
			EnableWindow(getView()->m_hWnd,TRUE);
			DestroyWindow(hwndAbort);
			}
		
//		theFax->prnFax(m_hWnd,hDC,&rc,nomeFile,&dInfo,myDlg.m_pd.nFromPage,myDlg.m_pd.nToPage);
fine:
		EndWaitCursor();
		HeapFree(GetProcessHeap(),0,lpdm);
		}

	DeleteDC(hDC);
   
	}


CALLBACK EXPORT AbortProc(HDC hDC, int reserved) {
	MSG msg;

	while(!fAbort && PeekMessage(&msg,0,0,0,TRUE)) {
		if(!hwndAbort || !IsDialogMessage(hwndAbort,&msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			}
		}	
	return !fAbort;
	}

LRESULT CALLBACK AbortDlgProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {

  switch (msg) {
    case WM_INITDIALOG:
//      SetDlgItemText(hwnd,IDC_EDIT1,szDevice);
//      SetDlgItemText(hwnd,IDC_EDIT2,szPort);
      EnableMenuItem(GetSystemMenu(hwnd, FALSE), SC_CLOSE, MF_GRAYED);
      break;

    case WM_COMMAND:
      switch(LOWORD (wParam)) {
        case IDCANCEL:
          fAbort = TRUE;
//          AbortDoc(hdc);
//          EnableWindow(ghwndMain, TRUE);
          DestroyWindow(hwnd);
          return TRUE;
        }
      break;
    }
  return 0;
  }





#ifndef _NEWMEET_MODE

void CVidsendDoc2::OnVideoTrasmissioneDalvivo() {
	CVideoSrcDialog myDlg(this);

	if(myDlg.DoModal() == IDOK) {
		videoSource=myDlg.m_VideoSource;
		alternaSource=myDlg.m_AlternaSource;
		OpzioniSorgenteVideo &= 0xffff0000;
		OpzioniSorgenteVideo |= myDlg.m_Overlay ? useOverlay : 0;
		theTV->theCapture=myDlg.m_Schede-1;
//		videoSourceRTSP=myDlg.m_VideoSource2;
		Opzioni &= ~usaRTSP;
		Opzioni |= myDlg.m_VideoSource2 ? usaRTSP : 0;
		RTSPaddress=myDlg.m_RTSPAddress;

		setTXMode(0);
		}
	}

void CVidsendDoc2::OnVideoTrasmissioneFilmato() {
	CApriVideoDlg myDlg(this);

	if(myDlg.DoModal() == IDOK) {
		nomeAVI_PB=myDlg.m_NomeFile;
		OpzioniSorgenteVideo &= 0xff00ffff;
		OpzioniSorgenteVideo |= myDlg.m_Loop ? aviLoop : 0;
		OpzioniSorgenteVideo |= myDlg.m_TipoVideo ? aviMode : 0;

		setTXMode(1);
		}

	}

void CVidsendDoc2::OnVideoTrasmissionePaginadiprova() {
	CPaginaTestDlg myDlg(this);
	int i;
//	CVidsendView2 *c=(CVidsendView2 *)GetWindow(GW_CHILD);
//	CVidsendDoc2 *d=(CVidsendDoc2 *)c->GetDocument();
	
	if(myDlg.DoModal() == IDOK) {
		pagProva.tipoVideo=myDlg.m_VideoImmagine;
		pagProva.tipoAudio=myDlg.m_AudioFrequenza;
		pagProva.audioOpzioni=0;
		pagProva.audioOpzioni |= myDlg.m_AudioIntervallato ? 1 : 0;
		pagProva.audioOpzioni |= myDlg.m_AudioSweep ? 2 : 0;
		setTXMode(2);
		}
	}

void CVidsendDoc2::OnVideoTrasmissioneClientVideoProxy() {

	if(!theApp.aClient[0])
		theApp.OnFileNuovoVideoclient();
	//FINIRE!
	setTXMode(3);
	}

void CVidsendDoc2::OnUpdateVideoTrasmissionePaginadiprova(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 2);
	pCmdUI->Enable((Opzioni & maySendVideo | Opzioni & maySendAudio) ? TRUE : FALSE);
	}

void CVidsendDoc2::OnUpdateVideoTrasmissioneFilmato(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 1);
	pCmdUI->Enable((Opzioni & maySendVideo | Opzioni & maySendAudio) ? TRUE : FALSE);
	}

void CVidsendDoc2::OnUpdateVideoTrasmissioneDalvivo(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 0);
	pCmdUI->Enable((Opzioni & maySendVideo | Opzioni & maySendAudio) ? TRUE : FALSE);
	}

void CVidsendDoc2::OnUpdateVideoTrasmissioneClientVideoProxy(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 3);
	pCmdUI->Enable((Opzioni & maySendVideo | Opzioni & maySendAudio) ? TRUE : FALSE);
	}

#endif


void CVidsendDoc2::OnUpdateVideoPaginadiprova(CCmdUI* pCmdUI) {
	
	}

void CVidsendDoc2::OnVideoTrasmissionePausa() {
	CVidsendView2 *v=(CVidsendView2 *)getView();
	
	bPaused=1;		// azzerare i contatori dei frame?
	((CChildFrame2 *)v->GetParent())->setStatusIcons();
	}

void CVidsendDoc2::OnUpdateVideoTrasmissionePausa(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(bPaused);
//	pCmdUI->Enable(!bPaused);
	}

void CVidsendDoc2::OnVideoTrasmissioneRiprendi() {
	CVidsendView2 *v=(CVidsendView2 *)getView();
	
	bPaused=0;
	theTV->Capture(1);
	((CChildFrame2 *)v->GetParent())->setStatusIcons();
	}

void CVidsendDoc2::OnUpdateVideoTrasmissioneRiprendi(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(!bPaused);
//	pCmdUI->Enable(bPaused);
	}


void CVidsendDoc2::OnVideoInformazioni() {
	CString S,S1;
	DWORD n,n1,n2;

	if(theTV) {
		if(streamSocketV) {
			S1.Format("Trasmissione video %ux%u pixel, %ubpp, %ufps, compressione 0x%X",	// mettere nome compressore!
				theTV->biRawBitmap.biWidth,theTV->biRawBitmap.biHeight,theTV->biRawBitmap.biBitCount,theTV->framesPerSec,theTV->biCompDef.bmiHeader.biCompression);
			S=S1+"\n";
			}
		if(streamSocketA && Opzioni & CVidsendDoc2::maySendAudio) {
			S1.Format("Trasmissione audio frequenza %u, %u bit, %u canale(i), compressione 0x%X",
				theTV->wfd.wf.nSamplesPerSec,theTV->wfd.wf.wBitsPerSample,theTV->wfd.wf.nChannels,theTV->wfd.wf.wFormatTag);
			S+=S1+"\n";
			}
		n=calcBandWidth();
		S1.Format("Bitrate: %uKbps",n/128);					//*8 e /1K
		S+=S1;
		{
		POSITION po;
		n=n1=n2=0;
		po=streamSocketV->cSockRoot.GetHeadPosition();
		if(po) {
			do {
				n+=streamSocketV->cSockRoot.GetNext(po)->sentFrame;
				n1+=streamSocketV->cSockRoot.GetNext(po)->stops;
				n2+=streamSocketV->cSockRoot.GetNext(po)->skippedFrame;
				} while(po);
			}
		S1.Format("Frame inviati: %u, STOP ricevuti: %u, frame NON inviati",n,n1,n2);
		S+=S1;
		}
		}
	else
		S="Informazioni non disponibili!";
	AfxMessageBox(S,MB_ICONINFORMATION);
	}


void CVidsendDoc2::OnVideoAudio() {
	CVidsendView2 *v=(CVidsendView2 *)getView();

	bAudio=!bAudio;
	((CChildFrame2 *)v->GetParent())->setStatusIcons();
	}

void CVidsendDoc2::OnUpdateVideoAudio(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(bAudio);
	pCmdUI->Enable(Opzioni & maySendAudio ? TRUE : FALSE);
	}

void CVidsendDoc2::OnVideoLivellivolume() {
	WinExec("sndvol32.exe",SW_SHOWNORMAL);
	WinExec("sndvol.exe",SW_SHOWNORMAL);			// per Windows 7 !!!
	}



void CVidsendDoc2::setTXMode(int mode) {
	CVidsendView2 *v=(CVidsendView2 *)getView();
	HRESULT hr; 
	
	if(mode>=0) {
		v->gdvFrameNum=0;
		v->gdaFrameNum=0;
		if(theTV)
			theTV->Capture(0);
		v->KillTimer(1);
		}
	if(psVideo) 
		AVIStreamClose(psVideo);
	psVideo=NULL;
	if(psAudio) 
		AVIStreamClose(psAudio);  
	psAudio=NULL;
	if(aviFile) 
		AVIFileClose(aviFile);  
	aviFile=NULL;
	if(gotFrame)
		AVIStreamGetFrameClose(gotFrame);
	gotFrame=NULL;
	AVIFileExit();  // 
	if(PBsi)
		HeapFree(GetProcessHeap(),0,PBsi);
	PBsi=NULL;
	if(PBbiSrc)
		HeapFree(GetProcessHeap(),0,PBbiSrc);
	PBbiSrc=NULL;
	if(PBbiDest)
		HeapFree(GetProcessHeap(),0,PBbiDest);
	PBbiDest=NULL;
	switch(mode) {
		case 0:				// live
			if(theTV) {
				if(theTV->Capture(1))
					trasmMode=0;
				}
			else
				trasmMode=0;
			break;
		case 1:				// filmato
			if(theTV) 
				theTV->Capture(0);
			AVIFileInit();  // initialize vfw library.... 2025
			hr=AVIFileOpen(&aviFile,(LPCTSTR)nomeAVI_PB,OF_READ,NULL);    // use handler determined from file extension....
			if(hr != AVIERR_OK)
				goto error1;
			hr = AVIFileGetStream(aviFile, &psVideo, streamtypeVIDEO, 0); 
			if(hr != AVIERR_OK)
				goto error1;
// provare, dovrebbe fare le 2 cose insieme			AVIStreamOpenFromFile(
			PBsi=(AVISTREAMINFO *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,sizeof(AVISTREAMINFO));
			AVIStreamInfo(psVideo,PBsi,sizeof(AVISTREAMINFO));
			PBbiSrc=(BITMAPINFOHEADER *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,sizeof(BITMAPINFOHEADER));
			PBbiSrc->biSize=sizeof(BITMAPINFOHEADER);
			PBbiSrc->biWidth=PBsi->rcFrame.right-PBsi->rcFrame.left;
			PBbiSrc->biHeight=PBsi->rcFrame.bottom-PBsi->rcFrame.top;
			PBbiSrc->biCompression=PBsi->fccHandler;
			PBbiSrc->biPlanes=1;
			PBbiSrc->biBitCount=24;		// sicuro??
			PBbiDest=(BITMAPINFOHEADER *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,sizeof(BITMAPINFOHEADER));
			*PBbiDest=*PBbiSrc;
			PBbiDest->biCompression=0;
			gotFrame=AVIStreamGetFrameOpen(psVideo,PBbiDest);
			if(!gotFrame)
				goto error1;
			if(Opzioni & maySendAudio) {
				hr = AVIFileGetStream(aviFile, &psAudio, streamtypeAUDIO, 0); 
				if(hr != AVIERR_OK)
					goto error1;
				}
			v->SetTimer(1,1000/myQV.fps,NULL);
			trasmMode=1;
error1:
			break;
		case 2:				// test
			if(theTV) 
				theTV->Capture(0);
			v->SetTimer(1,1000/myQV.fps,NULL);
			trasmMode=2;
			break;
		}
	if(mode>=0)
		((CChildFrame2 *)v->GetParent())->setStatusIcons();
	}
 

struct STREAM_INFO *CVidsendDoc2::getConnectionInfo() {
	struct STREAM_INFO *si;

	si=new struct STREAM_INFO;

/*#include <mmreg.h>	
	char buf[32];
	wsprintf(buf,"%u, %u",sizeof(struct STREAM_INFO),sizeof(struct EXT_WAVEFORMATEX));
	AfxMessageBox(buf);*/


	if(si) {
		ZeroMemory(si,sizeof(si));
		si->versione=VIDSEND_VERSIONE;
		if(Opzioni & maySendVideo && theTV) {
			if(trasmMode==0 || trasmMode==2) {
				if(Opzioni & videoType)			// non son sicuro che venga scritto il formato nativo, qua, specie con video non-compresso... VERIFICARE!
					si->bm=theTV->biCompDef.bmiHeader;
				else
					si->bm=theTV->biCompDef.bmiHeader;
				}
			else {
				if(/*(PBbiSrc->biCompression==0) && */ (Opzioni & videoType)
					/*ovvero OpzioniSorgenteVideo & CVidsendDoc2::aviMode*/ )
					si->bm=*PBbiSrc;
				else
					si->bm=theTV->biCompDef.bmiHeader;
				}
			}
		else
			si->bm.biSize=0;
		si->fps=theTV ? theTV->framesPerSec : 1;			// giusto pre protezione; 1 xche' 0 potrebbe dare /0!
		si->quality=myQV.quality;
		if(Opzioni & maySendAudio && theTV)
			memcpy(&si->wf,&theTV->wfd,sizeof(theTV->wfd));
		else
			si->wf.wf.wFormatTag=0;
		if(Opzioni & timedConnection)
			si->maxTime=timedConnLenght;
		else
			si->maxTime=0;
		switch((Opzioni & needAuthenticate) >> 29) {
			case 2:
			case 1:
				_tcscpy(si->authenticationWWW,(LPCTSTR)authenticationWWW);
				break;
			case 0:
				*si->authenticationWWW=0;
				break;
			}
		if(Opzioni & openWWW)
			_tcscpy(si->openWWW,(LPCTSTR)forceOpenWWW);
		else
			*si->openWWW=0;
		si->IDServer=myID;
		si->dontSave=Opzioni & dontSave;
		si->noBuffers=FALSE;
		si->remoteCtrl=0;		// finire!
		_tcscpy(si->streamTitle,(LPCTSTR)streamTitle);
		_tcscpy(si->splashOrIntro,(LPCTSTR)splashOrIntroName);
		}

	return si;
	}


int CVidsendDoc2::calcBandWidth() {		// quella in uso al momento...
	SIZE rc;

	rc.cy=theTV->biRawBitmap.biHeight;
	rc.cx=theTV->biRawBitmap.biWidth;
	return calcBandWidth(Opzioni & CVidsendDoc2::maySendVideo,
		&rc,
		Opzioni & CVidsendDoc2::videoType ? theTV->biRawBitmap.biBitCount : theTV->biBaseRawBitmap.biBitCount,
		myQV.compressor,myQV.quality,myQV.fps,
		Opzioni & CVidsendDoc2::maySendAudio,theTV->wfd.wf.nSamplesPerSec,
		theTV->wfd.wf.wBitsPerSample,theTV->wfd.wf.nChannels,myQA.compressor,myQA.quality);

	}

int CVidsendDoc2::calcBandWidth(int vq, int aq) {
	int i,n;

	if(vq >= 14 || aq >= 7)
		return -1;
	n=calcBandWidth(vq>=0,&qsv[vq].imageSize,qsv[vq].bpp,myQV.compressor,qsv[vq].quality,qsv[vq].fps,
		aq>=0,qsa[aq].samplesPerSec,qsa[aq].bitsPerSample,qsa[aq].channels,myQA.compressor,qsa[aq].quality);
	return n;
	}

int CVidsendDoc2::calcBandWidth(BOOL bVideo,const SIZE *imageSize,int imageFormat,DWORD compressorV,int qualityV,int fps,
																BOOL bAudio,DWORD samplesPerSec,WORD bitsPerSample,WORD channels,DWORD compressorA,int qualityA,
																CString *info) {
	int i;
	DWORD nv=0,na=0,t,t1;

	if(bVideo) {
		if(compressorV) {
			if(compressorV != -1) {
				HIC hICCo;
				BITMAPINFOHEADER bi,bo;
				SIZE r;
				CBitmap *b;
				BITMAP bmp;
				DWORD bSize;
				BYTE *s,*d;
				DWORD l1,l2;
				r=*imageSize;
				bi.biSize=sizeof(BITMAPINFOHEADER);
				bi.biWidth=imageSize->cx;
				bi.biHeight=imageSize->cy;
				bi.biBitCount=imageFormat;
				bi.biPlanes=1;
				bi.biClrImportant=bi.biClrUsed=0;
				bi.biSizeImage=0;
				bi.biCompression=0;
				bo=bi;
				bo.biCompression=compressorV;
				b=theApp.createTestBitmap(&r,&bi, 0 /*4*/);
				b->GetBitmap(&bmp);
				bSize=bmp.bmWidthBytes*bmp.bmHeight;
				s=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,bSize);
				b->GetBitmapBits(bSize,s);
				hICCo=ICOpen(ICTYPE_VIDEO,compressorV,ICMODE_FASTCOMPRESS);
				nv=-1;
				if(hICCo) {
					if((i=ICCompressBegin(hICCo,&bi,&bo)) == ICERR_OK) {
						t=ICCompressGetSize(hICCo,&bi,&bo);
						d=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,t+100);
						l1=l2=0;
						i=ICCompress(hICCo,0,
							&bo,d+sizeof(BITMAPINFOHEADER),&bi,s,
							&l1,&l2,1,0,qualityV,
							NULL,NULL);
						t = i == ICERR_OK ? bo.biSizeImage : 0;
						l1=l2=0;
						i=ICCompress(hICCo,ICCOMPRESS_KEYFRAME,
							&bo,d+sizeof(BITMAPINFOHEADER),&bi,s,
							&l1,&l2,1,0,qualityV,
							NULL,NULL);
						t1 = i == ICERR_OK ? bo.biSizeImage : 0;
						ICCompressEnd(hICCo);
						nv = t*(fps-1)+t1;
						}
					if(info) {
						ICINFO icinfo;
						ICGetInfo(hICCo,&icinfo,sizeof(ICINFO));
						*info="";
						*info+=icinfo.dwFlags & VIDCF_CRUNCH ? "supporta compressione a una dimensione data; " : "";
						*info+=icinfo.dwFlags & VIDCF_DRAW ? "supporta drawing; " : "";
						*info+=icinfo.dwFlags & VIDCF_FASTTEMPORALC ? "supporta compressione temporale e conserva copia dei dati; " : "";
						*info+=icinfo.dwFlags & VIDCF_FASTTEMPORALD ? "supporta decompressione temporale e conserva copia dei dati; " : "";
						*info+=icinfo.dwFlags & VIDCF_QUALITY ? "supporta impostazione qualità; " : "";
						*info+=icinfo.dwFlags & VIDCF_TEMPORAL ? "supporta compressione inter-frame; " : "";
						AfxMessageBox(*info);
						}

					ICClose(hICCo);
					}
				HeapFree(GetProcessHeap(),0,d);	
				HeapFree(GetProcessHeap(),0,s);
				delete b;
				}
			else {		// finire!
				nv=0;
				}
			}
		else {
			t1=t=imageSize->cx*imageSize->cy*imageFormat/8;
			nv = t*(fps-1)+t1;
			}
		}
	else
		nv=0;
	if(bAudio) {
		if(compressorA) {
			WAVEFORMATEX wf;
			HACMSTREAM hAcm;
			wf.wFormatTag = WAVE_FORMAT_PCM;
			wf.nChannels = channels;
			wf.nSamplesPerSec = samplesPerSec;
			wf.nBlockAlign = 1;
			wf.wBitsPerSample = bitsPerSample;
			wf.nAvgBytesPerSec = wf.nSamplesPerSec*wf.nChannels*(wf.wBitsPerSample/8);
			wf.cbSize = 0;



			/* v. per MP3 https://social.msdn.microsoft.com/Forums/en-US/5345daf5-2aca-4956-a45c-d8ef494af5da/how-to-play-mp3?forum=vssmartdevicesnative

	WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = 1;
    waveFormat.nChannels = NCHANNEL;
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.nAvgBytesPerSec = SAMPLE_RATE * NCHANNEL * BYTE_PER_SAMPLE;
    waveFormat.nBlockAlign = NCHANNEL * BYTE_PER_SAMPLE;
    waveFormat.wBitsPerSample = BYTE_PER_SAMPLE * BITS_PER_BYTES;
    waveFormat.cbSize = 0;

	MPEGLAYER3WAVEFORMAT mp3Format;
	ZeroMemory(&mp3Format, sizeof(MPEGLAYER3WAVEFORMAT));
	mp3Format.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
	mp3Format.wfx.nChannels = NCHANNEL;
	mp3Format.wfx.nSamplesPerSec = SAMPLE_RATE;
	mp3Format.wfx.nAvgBytesPerSec = SAMPLE_RATE * NCHANNEL * BYTE_PER_SAMPLE;
	mp3Format.wfx.nBlockAlign = NCHANNEL * BYTE_PER_SAMPLE;
	mp3Format.wfx.wBitsPerSample = 0;//BYTE_PER_SAMPLE * BITS_PER_BYTES;
	mp3Format.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
	mp3Format.wID = MPEGLAYER3_ID_MPEG;
	mp3Format.fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
	mp3Format.nFramesPerBlock = 1;
	mp3Format.nBlockSize =  mp3Format.wfx.nAvgBytesPerSec * 8 / mp3Format.wfx.nSamplesPerSec;
	mp3Format.nCodecDelay = 1393;

	switch(acmStreamOpen(NULL, NULL, (LPWAVEFORMATEX)&mp3Format, &waveFormat, NULL, 0, 0, ACM_STREAMOPENF_QUERY))
	*/
			switch(compressorA) {
				case WAVE_FORMAT_GSM610:
					GSM610WAVEFORMAT mywfx;
					samplesPerSec=8000;
					mywfx.wfx.wFormatTag = compressorA;
					mywfx.wfx.nChannels = 1;
					mywfx.wfx.nSamplesPerSec = (samplesPerSec/320)*320;
					mywfx.wfx.nAvgBytesPerSec = 1625;
					mywfx.wfx.nBlockAlign = 65;
					mywfx.wfx.wBitsPerSample = 0;
					mywfx.wfx.cbSize = 2;
					mywfx.wSamplesPerBlock = 320;
					acmStreamOpen(&hAcm,NULL,&wf,(WAVEFORMATEX *)&mywfx,NULL,NULL,0,0);
					if(hAcm) {
						acmStreamSize(hAcm,wf.nAvgBytesPerSec,&na,ACM_STREAMSIZEF_SOURCE);
						acmStreamClose(hAcm,0);
						}
					else {
						na=-1;
						}
					break;
				case WAVE_FORMAT_MPEGLAYER3:
					MPEGLAYER3WAVEFORMAT mp3Format;
					ZeroMemory(&mp3Format, sizeof(MPEGLAYER3WAVEFORMAT));
					mp3Format.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
					mp3Format.wfx.nChannels = channels;
					mp3Format.wfx.nSamplesPerSec = samplesPerSec;
					mp3Format.wfx.nAvgBytesPerSec = samplesPerSec * channels * bitsPerSample;
					mp3Format.wfx.nBlockAlign = (channels*bitsPerSample)/8;;
					mp3Format.wfx.wBitsPerSample = 0;//BYTE_PER_SAMPLE * BITS_PER_BYTES;
					mp3Format.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
					mp3Format.wID = MPEGLAYER3_ID_MPEG;

					// v. CMP3Coder (lame) 2023!

					mp3Format.fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
					mp3Format.nFramesPerBlock = 1;
					mp3Format.nBlockSize =  mp3Format.wfx.nAvgBytesPerSec * 8 / mp3Format.wfx.nSamplesPerSec;
					mp3Format.nCodecDelay = 1393;
					if(acmStreamOpen(&hAcm /*NULL*/, NULL, (LPWAVEFORMATEX)&mp3Format, &wf, NULL, 0, 0, ACM_STREAMOPENF_QUERY)) {
						acmStreamSize(hAcm,wf.nAvgBytesPerSec,&na,ACM_STREAMSIZEF_SOURCE);
						acmStreamClose(hAcm,0);
						}
					else {
						na=-1;
						}
//					na=mp3Format.wfx.nAvgBytesPerSec; // FARE!! v.sopra
					na=16384 /*diciamo 128Kbps :) */;
					break;
				case WAVE_FORMAT_PCM:
					na=wf.nAvgBytesPerSec;
					break;
				default:
					na=-1;
					break;
				}
			}
		else {
			na=samplesPerSec*bitsPerSample/8;
			na*=channels;
			}
		}
	else
		na=0;
	if(nv != -1 && na != -1)
		return nv+na;
	else
		return 0;
	}

DWORD CVidsendDoc2::getAVstep(struct QUALITY_MODEL_V *qv,struct QUALITY_MODEL_A *qa) {
	register int i,j;

	if(qv) {
		for(i=0; i<15; i++) {
			if(qv->imageSize.cx == qsv[i].imageSize.cx) {
				if(qv->fps <= qsv[i].fps) {
//					if(qv->qualita <= qsv[i].qualita) {
					break;
					}
				}
			if(qv->imageSize.cx < qsv[i].imageSize.cx) {
				while(i>0 && (qv->imageSize.cx < qsv[i].imageSize.cx)) 
					i--;
				break;
				}
			}
		}
	if(qa) {
		for(j=0; j<7; j++) {
			if(qa->samplesPerSec <= qsa[j].samplesPerSec) {
				break;
				}
			}
		}
	return MAKELONG(i,j);
	}

void CVidsendDoc2::checkUtenti() {

	if(controlSocket)
		controlSocket->checkUtenti();
	}

void CVidsendDoc2::sendHrtBt() {
	char myBuf[2];
	int i;

	if(authSocket) {
		myBuf[0]=11;
		myBuf[1]=0;
		if((i=authSocket->Send(myBuf,2)) == 2)
			;
		else {
			DWORD n=GetLastError();
			if(theApp.debugMode)
				if(theApp.FileSpool) 
					theApp.FileSpool->print(CLogFile::flagError,"impossibile inviare heartbeat AUTH (errore Send %d,%u)!",i,n);
			}
		}
	else {
		if(theApp.FileSpool) 
			theApp.FileSpool->print(CLogFile::flagError,"impossibile inviare heartbeat AUTH (socket NULLO)!");
		}
	}

int CVidsendDoc2::MandaPipeV(const struct AV_PACKET_HDR *avh,LPARAM lParam) {
	int i;
	DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0; 
  BOOL fSuccess = FALSE;

//https://docs.microsoft.com/en-us/windows/desktop/ipc/multithreaded-pipe-server
	if(pipeV != INVALID_HANDLE_VALUE) {
		cbReplyBytes=lParam;
		// Write the reply to the pipe. 
    fSuccess = WriteFile( 
			pipeV,        // handle to pipe 
			avh,     // buffer to write from 
			cbReplyBytes, // number of bytes to write 
			&cbWritten,   // number of bytes written 
			NULL);        // not overlapped I/O 

    if (!fSuccess || cbReplyBytes != cbWritten) {   
     // _tprintf(TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError()); 
      //break;
      }
		}

	return fSuccess;
	}

int CVidsendDoc2::HandlePipeV() {
  BOOL   fConnected = FALSE,fSuccess = FALSE;
	DWORD cbBytesRead = 0;
  OVERLAPPED oOverlap; 

	// Wait for the client to connect; if it succeeds, 
	// the function returns a nonzero value. If the function
	// returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 

	fConnected = ConnectNamedPipe(pipeV, &oOverlap) ? 
		 TRUE : (GetLastError() == ERROR_PIPE_CONNECTED); 

	if(fConnected) { 

   // Read client requests from the pipe. This simplistic code only allows messages
   // up to BUFSIZE characters in length.
    fSuccess = ReadFile( 
			pipeV,        // handle to pipe 
			pipeVBuffer,    // buffer to receive data 
			PIPEBUFSIZE*sizeof(TCHAR), // size of buffer 
			&cbBytesRead, // number of bytes read 
			NULL);        // not overlapped I/O 

    if (!fSuccess || cbBytesRead == 0) {   
      if (GetLastError() == ERROR_BROKEN_PIPE) {
//             _tprintf(TEXT("InstanceThread: client disconnected.\n"), GetLastError()); 
        }
      else {
//             _tprintf(TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError()); 
        }
      }
		else {
			char *p=(char *)GlobalAlloc(GPTR,1024);
			wsprintf(p,"NamedPipe receive: %s",pipeVBuffer); 
			theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
			}
		} 

	return fSuccess;
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2

IMPLEMENT_DYNCREATE(CVidsendDoc22, CDocument)

const struct WAVE_QUALITY CVidsendDoc22::waveQualities[8] = {
	{8000,8,1},
	{11025,8,1},
	{22050,8,1},
	{22050,16,1},
	{44100,8,1},
	{44100,16,1},
	{44100,16,2},
	{48000,16,2},
	};

const struct QUALITY_MODEL_A CVidsendDoc22::qsa[7]= {
	{8000,8,1,2500},
	{11025,8,1,5000},
	{22050,8,1,5000},
	{22050,8,2,5000},
	{22050,16,2,5000},
	{44100,16,2,5000},
	{44100,16,2,9000}
	};

CVidsendDoc22::CVidsendDoc22() {
	char myBuf[128];
	
	prfSection="ServerStream";
	Opzioni=GetPrivateProfileInt(IDS_OPZIONI);
	Opzioni2=theApp.prStore->GetPrivateProfileInt(prfSection,IDS_OPZIONI2,7);
	OpzioniSorgenteVideo=GetPrivateProfileInt(IDS_OPZIONI3);
	maxConn=GetPrivateProfileInt(IDS_MAXCONN);
	if(!maxConn)
		maxConn=10;

/*	CString S;
	S.Format("%u",maxConn);
	AfxMessageBox(S);*/

	GetPrivateProfileString(IDS_FORCEOPENWWW,myBuf,127);
	forceOpenWWW=myBuf;
	GetPrivateProfileString(IDS_AUTHWWW,myBuf,127);
	authenticationWWW=myBuf;

//	authenticationWWW="192.168.0.100";


	timedConnLenght=GetPrivateProfileTimeSpan(IDS_TIMEDCONN);
	GetPrivateProfileString(IDS_STREAM_DIRECTORYWWW,myBuf,127);
	directoryWWW=myBuf;
	GetPrivateProfileString(IDS_STREAM_DIRECTORYWWWLOGIN,myBuf,31);
	directoryWWWLogin=myBuf;

//	directoryWWW="192.168.0.100";


	GetPrivateProfileString(IDS_MP3FILE_PB,myBuf,255);
	nomeMP3_PB=myBuf;
	GetPrivateProfileString(IDS_SUONO1,myBuf,127);
	suonoIn=myBuf;
	GetPrivateProfileString(IDS_SUONO2,myBuf,127);
	suonoOut=myBuf;
	GetPrivateProfileString(IDS_STREAMTITLE,myBuf,127);
	streamTitle=myBuf;
	theSet=NULL;
	myID=1;
	myTimedConn=0;

//	theTV->setOpzioni(OpzioniSalvaAudio);

	ZeroMemory(&myQA,sizeof(myQA));
	myQA.compressor=GetPrivateProfileInt(IDS_ACOMPRESSORNAME);
	myQA.samplesPerSec=8000;
	myQA.bitsPerSample=8;
	myQA.channels=1;
	myQA.quality=GetPrivateProfileInt(IDS_COMPRESSORQUALITYA);
	myQA.mp3bitrate=theApp.prStore->GetPrivateProfileInt(prfSection,IDS_MP3BITRATE,128);

	pagProva.tipoAudio=GetPrivateProfileInt(IDS_COMPRESSOR_TESTPAGEA);
	pagProva.audioOpzioni=GetPrivateProfileInt(IDS_COMPRESSOR_TESTPAGEOPZ);

	streamSocketA=new CStreamSrvSocket(this);
	streamSocketA->Create(AUDIO_SOCKET,theApp.Opzioni & CVidsendApp::TCP_UDP ? SOCK_DGRAM : SOCK_STREAM);
	if(!(theApp.Opzioni & CVidsendApp::TCP_UDP)) {
		streamSocketA->Listen();
		}
	else {
		}
	streamSocketA2=new CStreamSrvSocket(this,CStreamSrvSocket::MP3);		// MP3 streaming, 2023
	streamSocketA2->Create(MP3_STREAM_SOCKET,SOCK_STREAM);
	streamSocketA2->Listen();
	streamSocketA2->tag=(DWORD)lame_init();
	((lame_global_flags*)streamSocketA2->tag)->brate=myQA.mp3bitrate;
	lame_init_params((lame_global_flags*)streamSocketA2->tag);
	id3tag_init((lame_global_flags*)streamSocketA2->tag);
	id3tag_set_title((lame_global_flags*)streamSocketA2->tag, "RadioG");
	id3tag_set_artist((lame_global_flags*)streamSocketA2->tag, "Dario Greggio");
	id3tag_set_year((lame_global_flags*)streamSocketA2->tag, "2023");
	id3tag_add_v2((lame_global_flags*)streamSocketA2->tag);


	controlSocket=new CControlSrvSocket(this);
	controlSocket->Create(CONTROL_SOCKET);
	if(!(theApp.Opzioni & CVidsendApp::TCP_UDP)) {
		controlSocket->Listen();
		}
	else {
		}

	streamSocketVBAN=NULL;
	if(Opzioni & usaVBAN) {
		streamSocketVBAN=new CVBANSocket(this);
		struct stream_config_t stream_config;
		stream_config.bit_fmt=VBAN_BITFMT_8_INT;
		stream_config.nb_channels=1;
		stream_config.sample_rate=22050;
		streamSocketVBAN->PacketInitHeader(&stream_config, "Liverpool's GD");		//NO! va cmq fatta a ogni pacchetto

		streamSocketVBAN->Open();

		}

	authSocket=NULL;
	trasmMode=0;
	bPaused=0;
	bAudio=Opzioni & maySendAudio ? TRUE : FALSE;

	psAudio=NULL;

/* da ocx...
	if(Opzioni & usaRTSP) {
		rtspSocket=new CRTSPServer(_T("stream1"));
		if(rtspSocket) {
	#define RTSP_PORT 554
			int i=rtspSocket->Create(RTSP_PORT);
			if(!i) {
				if(theApp.FileSpool) 
					theApp.FileSpool->print(CLogFile::flagError,"Impossibile installare server RTSP");
				}
			else
				i=rtspSocket->Listen();
		//										i=WSAGetLastError();
			}
		}
		*/




	pipeA = INVALID_HANDLE_VALUE;
	pipeABuffer=NULL;


	dirCliSocket=NULL;

	}

BOOL CVidsendDoc22::OnNewDocument() {
	char *p,myBuf[128];
	int i,j;
	
	if(!CExDocument::OnNewDocument())
		return FALSE;

	SetTitle("Audio live");

	if(Opzioni & doDialUp) {
		i=theApp.callRAS((LPCTSTR)theApp.DialUpNome);
		}

	if(Opzioni & registerServer) {
		if(!(dirCliSocket=new CDirectoryCliSocket))
			goto no_reg;
		if(!dirCliSocket->Create())
			goto no_reg;
		if(dirCliSocket->Connect(directoryWWW,DIRECTORY_SOCKET)) {
			ZeroMemory(myBuf,33);
			*myBuf='\x1';
			_tcscpy(myBuf+1,directoryWWWLogin);
			dirCliSocket->Send(myBuf,33);
			}
		else {
no_reg:
			if(theApp.FileSpool)
				*theApp.FileSpool << "Impossibile registrare server nella directory dei Server!";
			}	
		}

	setTXMode(0);		// salvare TXmode??

	if(((theApp.Opzioni & CVidsendDoc22::needAuthenticate) >> 29) == 2) {
		if(theSet=new CVidsendSet2(theApp.theDB)) {
			if(!theSet->Open(AFX_DB_USE_DEFAULT_TYPE,NULL,CRecordset::readOnly))
				goto no_set;
			}
		else {
no_set:
			p="Impossibile aprire database utenti!";
			AfxMessageBox(p);
			if(theApp.FileSpool)
				*theApp.FileSpool << p;
			if(theSet) {
				delete theSet;
				return FALSE;
				}
			}
		}

/*	if(Opzioni & needAuthenticateServer) {		//se aperto con autenticazione (cioe' per newmeet)
		if(!theApp.theChat) {
			theApp.OnFileNuovoChat();
			if(theApp.theChat) {
				theApp.theChat->Opzioni |= CVidsendDoc4::slaveMode;
				}
			}
		} no! lo fanno le #define */
	CVidsendView22 *v=(CVidsendView22 *)getView();
	((CChildFrame22 *)v->GetParent())->setStatusIcons(this);
	v->m_DSound=new CDSound;
	if(v->m_DSound) {
		v->m_DS=new CDSoundPlay;
		if(Opzioni2 & CVidsendDoc22::preascoltoMono)
			v->m_DSound->Create(0);		// serve DOPO creazione finestra...
		else if(Opzioni2 & CVidsendDoc22::attivaSchedaAudio2)
			v->m_DSound->Create(2);		// serve DOPO 
		else
			v->m_DSound->Create(1);		// serve DOPO 
		v->m_DSound->SetCooperativeLevel(v->GetSafeHwnd(),/*DSSCL_EXCLUSIVE | */ DSSCL_PRIORITY);
		}


	if(theApp.Opzioni & CVidsendApp::namedPipes) {
		pipeA = CreateNamedPipe( 
			TEXT("\\\\.\\pipe\\audiosenderpipe"),             // pipe name 
			PIPE_ACCESS_DUPLEX |       // read/write access 
			FILE_FLAG_OVERLAPPED,    // overlapped mode 
			PIPE_TYPE_MESSAGE |       // message type pipe 
			PIPE_READMODE_MESSAGE |   // message-read mode 
			PIPE_WAIT,                // blocking mode 
			PIPE_UNLIMITED_INSTANCES, // max. instances  
			PIPEBUFSIZE,                  // output buffer size 
			PIPEBUFSIZE,                  // input buffer size 
			0,                        // client time-out 
			NULL);  
		HANDLE hHeap      = GetProcessHeap();
		pipeABuffer=(TCHAR*)HeapAlloc(hHeap, 0, PIPEBUFSIZE*sizeof(TCHAR));
		}

	for(j=0; j<3; j++) {
  	for(i=0; i<12; i++) {
		char myBuf2[128];
		  wsprintf(myBuf,"Banco%u_Suono%u",j,i);
		  theApp.prStore->GetPrivateProfileString(prfSection,myBuf,myBuf2,127,"");
			WAVFiles[j][i]=myBuf2;
			}
		}
	bBanco=0;
	v->UpdateBanco();
/*	WAVFiles[0][0]="applaus2.wav";
	WAVFiles[0][1]="chord.wav";
	WAVFiles[0][2]="chimes.wav";
	WAVFiles[0][3]="camera.wav";
	WAVFiles[0][4]="laser.wav";
	WAVFiles[0][5]="bounce.wav";
	WAVFiles[0][6]="buzz.wav";
	WAVFiles[0][7]="basemt.wav";
	WAVFiles[0][8]="phnring.wav";
	WAVFiles[0][9]="big_explosion_01.wav";
	WAVFiles[0][10]="snoring.wav";
	WAVFiles[0][11]="stbeam1.wav";
	WAVFiles[1][0]="monkey.wav";
	WAVFiles[1][1]="cow.wav";
	WAVFiles[1][2]="dogbark.wav";
	WAVFiles[1][3]="miaow02.wav";
	WAVFiles[1][4]="maiale.wav";
	WAVFiles[1][5]="owl.wav";
	WAVFiles[1][6]="sheep_02.wav";
	WAVFiles[1][7]=".wav";
	WAVFiles[1][8]=".wav";
	WAVFiles[1][9]=".wav";
	WAVFiles[1][10]="wolfhowl.wav";
	WAVFiles[1][11]="drumroll.wav";
	WAVFiles[2][0]="crash1.wav";
	WAVFiles[2][1]="carhorn2.wav";
	WAVFiles[2][2]="meepmeep.wav";
	WAVFiles[2][3]="rifle8.wav";
	WAVFiles[2][4]="stphoton.wav";
	WAVFiles[2][5]="whistle1.wav";
	WAVFiles[2][6]=".wav";
	WAVFiles[2][7]=".wav";
	WAVFiles[2][8]=".wav";
	WAVFiles[2][9]=".wav";
	WAVFiles[2][10]=".wav";
	WAVFiles[2][11]=".wav";
*/
	m_Equalizzatore=0;

	GetPrivateProfileString(IDS_CANZONE1,myBuf,127);
	m_Canzone1=myBuf;
	GetPrivateProfileString(IDS_CANZONE2,myBuf,127);
	m_Canzone2=myBuf;

	saveFile=NULL;

	return TRUE;
	}

CVidsendDoc22::~CVidsendDoc22() {
	char myBuf[128];

	if(saveFile) {
		saveFile->Close();
		delete saveFile; saveFile=NULL;
		}
	if(psAudio) {
		psAudio->Close();
		delete psAudio; psAudio=NULL;
		}
	setTXMode(-1);
	if(authSocket) {
		authSocket->Close();
		delete authSocket;
		}
	authSocket=NULL;
	if(dirCliSocket) {
		dirCliSocket->Close();
		delete dirCliSocket;
		}
	dirCliSocket=NULL;
	if(theSet) {
		theSet->Close();
		delete theSet;
		}
	theSet=NULL;
	if(controlSocket) {
		controlSocket->Close();
		delete controlSocket;
		}
	controlSocket=NULL;
	if(streamSocketA) {
		streamSocketA->Close();
		delete streamSocketA;
		}
	streamSocketA=NULL;
	if(streamSocketA2) {
		streamSocketA2->Close();
		if(streamSocketA2->tag) {
			free_id3tag(((lame_global_flags *)streamSocketA2->tag)->internal_flags);
			lame_close((lame_global_flags *)streamSocketA2->tag);
			}
		delete streamSocketA2;
		}
	streamSocketA2=NULL;

	if(streamSocketVBAN) {
		streamSocketVBAN->Close();
		delete streamSocketVBAN;
		}
	streamSocketVBAN=NULL;
	save();
  WritePrivateProfileInt(IDS_MAXCONN,maxConn);
  WritePrivateProfileInt(IDS_OPZIONI,Opzioni);
  WritePrivateProfileInt(IDS_OPZIONI3,OpzioniSorgenteVideo);
  WritePrivateProfileInt(IDS_OPZIONI2,Opzioni2);
	}

BOOL CVidsendDoc22::save() {
	char myBuf[64];
	int i,j;


	for(j=0; j<3; j++) {
  	for(i=0; i<12; i++) {
    	wsprintf(myBuf,"Banco%u_Suono%u",j,i);
		  theApp.prStore->WritePrivateProfileString(prfSection,myBuf,WAVFiles[j][i]);
			}
		}

	wsprintf(myBuf,"%d",myQA.compressor);
	WritePrivateProfileString(IDS_ACOMPRESSORNAME,myBuf);
	WritePrivateProfileInt(IDS_MP3BITRATE,myQA.mp3bitrate);
	WritePrivateProfileInt(IDS_COMPRESSORQUALITYA,myQA.quality);
	WritePrivateProfileInt(IDS_COMPRESSOR_TESTPAGEA,pagProva.tipoAudio);
	WritePrivateProfileInt(IDS_COMPRESSOR_TESTPAGEOPZ,pagProva.audioOpzioni);
	WritePrivateProfileString(IDS_FORCEOPENWWW,(LPSTR)(LPCTSTR)forceOpenWWW);
	WritePrivateProfileString(IDS_AUTHWWW,(LPSTR)(LPCTSTR)authenticationWWW);
	WritePrivateProfileTime(IDS_TIMEDCONN,timedConnLenght);
	WritePrivateProfileString(IDS_STREAM_DIRECTORYWWW,(LPSTR)(LPCTSTR)directoryWWW);
	WritePrivateProfileString(IDS_STREAM_DIRECTORYWWWLOGIN,(LPSTR)(LPCTSTR)directoryWWWLogin);
	WritePrivateProfileString(IDS_SUONO1,(LPSTR)(LPCTSTR)suonoIn);
	WritePrivateProfileString(IDS_SUONO2,(LPSTR)(LPCTSTR)suonoOut);
	WritePrivateProfileString(IDS_STREAMTITLE,(LPSTR)(LPCTSTR)streamTitle);
	WritePrivateProfileString(IDS_MP3FILE_PB,(LPSTR)(LPCTSTR)nomeMP3_PB);
	WritePrivateProfileString(IDS_CANZONE1,(LPSTR)(LPCTSTR)m_Canzone1);
	WritePrivateProfileString(IDS_CANZONE2,(LPSTR)(LPCTSTR)m_Canzone2);
	
	return 1;
	}

int CVidsendDoc22::acceptConnect(const char *who) {
	CString S;
	static BOOL inUse;

	if(!suonoIn.IsEmpty()) {
		sndPlaySound(suonoIn,SND_ASYNC);		// v. anche MP3!
		}
	if(Opzioni & askOnConnect) {
		if(inUse)
			return 0;
		inUse=1;
		S.Format("Accettare la connessione entrante (da %s )?",who ? who : "<sconosciuto>");
		if(AfxMessageBox(S,MB_YESNO | MB_ICONQUESTION) == IDYES) {
			inUse=0;
			}
		else {
			inUse=0;
			return 0;
			}
		}
	return 1;
	}

int CVidsendDoc22::openAudio(CVidsendView22 *v) {
	CChildFrame22 *w;
	WAVEFORMATEX myWf;

	myWf.wFormatTag = WAVE_FORMAT_PCM;
	myWf.nChannels =  2 /*1*/;
	myWf.nSamplesPerSec = 44100 /*22050*/;
#pragma warning WAVEFORMAT 44100
	myWf.wBitsPerSample = 16 /*8*/;
	myWf.nBlockAlign= (myWf.nChannels * myWf.wBitsPerSample)/8;
	myWf.nAvgBytesPerSec = (myWf.nSamplesPerSec * myWf.nChannels * myWf.wBitsPerSample)/8;
	myWf.cbSize = 0 /*sizeof(WAVEFORMATEX)*/;

	theApp.theServer2=this;		// mi serve in CTV!! (v. anche main OpenDocument())

//		GetParent()->SendMessage(WM_MOVE,d->theTV->GetXSize(),d->theTV->GetYSize());
	if(w=(CChildFrame22 *)v->GetParent())

		if(Opzioni & usaVBAN) {
			if(streamSocketVBAN)
				streamSocketVBAN->Close();
			struct stream_config_t stream_config;
			stream_config.bit_fmt=v->wfd.wf.wBitsPerSample==16 ? VBAN_BITFMT_16_INT : VBAN_BITFMT_8_INT;
			stream_config.nb_channels=v->wfd.wf.nChannels;
			stream_config.sample_rate=v->wfd.wf.nSamplesPerSec;
			streamSocketVBAN->PacketInitHeader(&stream_config, "Liverpool's GD");
//			streamSocketVBAN->Open("255.255.255.255");
			streamSocketVBAN->Open("192.168.1.104");

		}
	if(w)
		w->setStatusIcons(this);
	return 1;
	}



BEGIN_MESSAGE_MAP(CVidsendDoc22, CExDocument)
	//{{AFX_MSG_MAP(CVidsendDoc22)
	ON_COMMAND(ID_FILE_PROPRIETA, OnFileProprieta)
	ON_COMMAND(ID_VIDEO_LIVELLIVOLUME, OnAudioLivellivolume)
	ON_COMMAND(ID_FILE_SAVE_VIDEO, OnFileSaveAudio)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_VIDEO, OnUpdateFileSaveAudio)
	ON_COMMAND(ID_CONNESSIONE_RIPRENDI, OnAudioTrasmissioneRiprendi)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_RIPRENDI, OnUpdateAudioTrasmissioneRiprendi)
	ON_COMMAND(ID_CONNESSIONE_PAUSA, OnAudioTrasmissionePausa)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_PAUSA, OnUpdateAudioTrasmissionePausa)
	ON_COMMAND(ID_VIDEO_AUDIO, OnAudioAudio)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_AUDIO, OnUpdateAudioAudio)
	ON_COMMAND(ID_VIDEO_TRASMISSIONE_DALVIVO, OnAudioTrasmissioneDalvivo)
	ON_COMMAND(ID_VIDEO_TRASMISSIONE_FILMATO, OnAudioTrasmissioneCanzone)
	ON_COMMAND(ID_VIDEO_TRASMISSIONE_PAGINADIPROVA, OnAudioTrasmissionePaginadiprova)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_PAGINADIPROVA, OnUpdateAudioTrasmissionePaginadiprova)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_FILMATO, OnUpdateAudioTrasmissioneCanzone)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_DALVIVO, OnUpdateAudioTrasmissioneDalvivo)
	ON_COMMAND(ID_VIDEO_INFORMAZIONI, OnAudioInformazioni)
	ON_COMMAND(ID_AUDIO_MONO, OnAudioMono)
	ON_UPDATE_COMMAND_UI(ID_AUDIO_MONO, OnUpdateAudioMono)
	ON_COMMAND(ID_AUDIO_SUPERSTEREO, OnAudioSuperstereo)
	ON_UPDATE_COMMAND_UI(ID_AUDIO_SUPERSTEREO, OnUpdateAudioSuperstereo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc22 diagnostics

#ifdef _DEBUG
void CVidsendDoc22::AssertValid() const
{
	CExDocument::AssertValid();
}

void CVidsendDoc22::Dump(CDumpContext& dc) const
{
	CExDocument::Dump(dc);
}
#endif //_DEBUG


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc22 commands

void CVidsendDoc22::OnFileProprieta() {
	int i;

	CVidsendPropPage mySheet("Proprietà streaming audio",(CVidsendView22 *)getView());
	CVidsendDoc2PropPage00 myPage00(this,&myQA);
	CVidsendDoc2PropPage2 myPage2(this);
	CVidsendDoc2PropPage3 myPage3(this);
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		if(0)
			goto fine;
		}
//	if(theApp.Opzioni & CVidsendApp::advancedConf)		// per ora no...
		mySheet.AddPage(&myPage00);
//	else
//		mySheet.AddPage(&myPage0bis);
	mySheet.AddPage(&myPage2);
	mySheet.AddPage(&myPage3);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
		if(myPage00.isInitialized) {
			Opzioni &= 0xffff0000;
			Opzioni |= myPage00.m_ServerAudio ? maySendAudio : 0;
			Opzioni |= myPage00.m_VBAN ? usaVBAN : 0;
			Opzioni |= myPage00.m_MP3 ? sendAudioMP3 : 0;

//	if(!theApp.Opzioni & CVidsendApp::advancedConf)		// per ora no...
//			theTV->theCapture=myPage0bis.m_Schede-1;

//			if(theApp.Opzioni & CVidsendApp::advancedConf) {
				myQA.compressor=myPage00.m_CompressorA;
				myQA.quality=myPage00.m_QA.quality;
				myQA.mp3bitrate=myPage00.m_QA.mp3bitrate;
//				}
			Opzioni2 = 0x00000000;
			Opzioni2 |= myPage00.m_Attiva0 ? attivaSchedaAudio0 : 0;
			Opzioni2 |= myPage00.m_Attiva1 ? attivaSchedaAudio1 : 0;
			Opzioni2 |= myPage00.m_Attiva2 ? attivaSchedaAudio2 : 0;
			Opzioni2 |= myPage00.m_PreascoltoMono ? preascoltoMono : 0;
//	WORD m_SchedaAudio1,m_SchedaAudio2,m_SchedaAudio0;

			}

		if(myPage2.isInitialized) {
			Opzioni &= 0x00ffffff;
			Opzioni |= (myPage2.m_TipoAutorizzazione) << 29;
			Opzioni |= myPage2.m_bDirectoryServer ? registerServer : 0;
			Opzioni |= myPage2.m_bNeedAuthenticate ? needAuthenticateServer : 0;
			maxConn=myPage2.m_MaxConn;
			authenticationWWW=myPage2.m_AuthWWW;
			directoryWWW=myPage2.m_DirectoryServer;
			directoryWWWLogin=myPage2.m_NomePerServer;
			}
		if(myPage3.isInitialized) {
			forceOpenWWW=myPage3.m_OpenWWW;
			Opzioni &= 0xf00fffff;
			Opzioni |= myPage3.m_bOpenWWW ? openWWW : 0;
			Opzioni |= myPage3.m_bTimedConn ? timedConnection : 0;
			Opzioni |= myPage3.m_DontSave ? dontSave : 0;
			Opzioni |= myPage3.m_ActivateIf ? openAudioOnConnect : 0;
			Opzioni |= myPage3.m_ActivateWaitConfirm ? askOnConnect : 0;
			Opzioni |= myPage3.m_DialUp ? doDialUp : 0;
			theApp.DialUpNome=myPage3.m_DialUpNome;
			{ CTimeSpan myTS(0,myPage3.m_TimedConn.GetHour(),myPage3.m_TimedConn.GetMinute(),0);
				timedConnLenght=myTS;
			}
			suonoIn=myPage3.m_SuonoIn;
			suonoOut=myPage3.m_SuonoOut;
			streamTitle=myPage3.m_StreamTitle;
			}
		}


fine:
		;

	}


void CVidsendDoc22::OnAudioMono() {
	
	Opzioni ^= CVidsendDoc22::mono;
	Opzioni &= ~CVidsendDoc22::sstereo;
	}

void CVidsendDoc22::OnAudioSuperstereo() {
	
	Opzioni ^= CVidsendDoc22::sstereo;
	Opzioni &= ~CVidsendDoc22::mono;
	}

void CVidsendDoc22::OnUpdateAudioMono(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc22::mono ? 1 : 0);
//	pCmdUI->Enable(((CVidsendView22 *)getView())->cliSockA ? 1 : 0);
	}

void CVidsendDoc22::OnUpdateAudioSuperstereo(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc22::sstereo ? 1 : 0);
//	pCmdUI->Enable(((CVidsendView22 *)getView())->cliSockA ? 1 : 0);
	}

void CVidsendDoc22::OnCloseDocument() {

	if(Opzioni & needAuthenticateServer) {		//se aperto con autenticazione (cioe' per newmeet)

		}

	if(pipeA != INVALID_HANDLE_VALUE) {
		FlushFileBuffers(pipeA); 
		DisconnectNamedPipe(pipeA); 
		CloseHandle(pipeA); 
		}

	CExDocument::OnCloseDocument();
	theApp.theServer2=NULL;
	}




void CVidsendDoc22::OnAudioTrasmissioneDalvivo() {
	CVidsendView22 *v=(CVidsendView22 *)getView();

/*	if(myDlg.DoModal() == IDOK) {
		videoSource=myDlg.m_VideoSource;
		setTXMode(0);
		}*/
	setTXMode(0);
	}

void CVidsendDoc22::OnAudioTrasmissioneCanzone() {
	CApriAudioDlg myDlg(this);

	if(myDlg.DoModal() == IDOK) {
		nomeMP3_PB=myDlg.m_NomeFile;
		OpzioniSorgenteVideo &= 0xff00ffff;
		OpzioniSorgenteVideo |= myDlg.m_Loop ? mp3Loop : 0;
		setTXMode(1);
		}

	}

void CVidsendDoc22::OnAudioTrasmissionePaginadiprova() {
	CPaginaTestDlg myDlg(this);
	int i;
//	CVidsendView22 *c=(CVidsendView22 *)GetWindow(GW_CHILD);
//	CVidsendDoc22 *d=(CVidsendDoc22 *)c->GetDocument();
	
	if(myDlg.DoModal() == IDOK) {
		pagProva.tipoAudio=myDlg.m_AudioFrequenza;
		pagProva.audioOpzioni=0;
		pagProva.audioOpzioni |= myDlg.m_AudioIntervallato ? 1 : 0;
		pagProva.audioOpzioni |= myDlg.m_AudioSweep ? 2 : 0;
		setTXMode(2);
		}
	}

void CVidsendDoc22::OnUpdateAudioTrasmissionePaginadiprova(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 2);
	pCmdUI->Enable(Opzioni & (maySendAudio | sendAudioMP3) ? TRUE : FALSE);
	}

void CVidsendDoc22::OnUpdateAudioTrasmissioneCanzone(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 1);
	pCmdUI->Enable(Opzioni & (maySendAudio | sendAudioMP3) ? TRUE : FALSE);
	}

void CVidsendDoc22::OnUpdateAudioTrasmissioneDalvivo(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 0);
	pCmdUI->Enable(Opzioni & (maySendAudio | sendAudioMP3) ? TRUE : FALSE);
	}



void CVidsendDoc22::OnUpdateAudioPaginadiprova(CCmdUI* pCmdUI) {
	
	}

void CVidsendDoc22::OnAudioTrasmissionePausa() {
	CVidsendView22 *v=(CVidsendView22 *)getView();
	
	bPaused=1;		// azzerare i contatori dei frame?
	((CChildFrame22 *)v->GetParent())->setStatusIcons();
	v->buttonS->SetCheck(!bPaused);
	}

void CVidsendDoc22::OnUpdateAudioTrasmissionePausa(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(bPaused);
	pCmdUI->Enable(!bPaused);
	}

void CVidsendDoc22::OnAudioTrasmissioneRiprendi() {
	CVidsendView22 *v=(CVidsendView22 *)getView();
	
	bPaused=0;
	((CChildFrame22 *)v->GetParent())->setStatusIcons();
	v->buttonS->SetCheck(!bPaused);
	}

void CVidsendDoc22::OnUpdateAudioTrasmissioneRiprendi(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(!bPaused);
	pCmdUI->Enable(bPaused);
	}

void CVidsendDoc22::OnFileSaveAudio() {
	CVidsendView22 *v=(CVidsendView22 *)getView();
	
	if(isRecordingAudio())
		endSaveFile();
	else {
		CFileDialog myDlg(FALSE,"*.mp3",NULL,OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,"File Audio (*.mp3)|*.mp3|File Audio (*.wav)|*.wav|Tutti i file (*.*)|*.*||",v);
		if(myDlg.DoModal() == IDOK) {
			int i=myDlg.GetFileName().ReverseFind('\\');
			if(i>0) {
				nomeMP3=myDlg.GetFileName().Mid(i+1);
				pathMP3=myDlg.GetFileName().Left(i);
				}
			else {
				nomeMP3=myDlg.GetFileName();
				pathMP3="";
				}
			if(nomeMP3.Find('.') == -1)
				nomeMP3+=".mp3";
			if(pathMP3.Right(1) != '\\')
				pathMP3+='\\';
			startSaveFile(myDlg.GetFileName() /*pathMP3+nomeMP3 boh*/,0);
			}
		}
	((CChildFrame22 *)v->GetParent())->setStatusIcons();
	}

void CVidsendDoc22::OnUpdateFileSaveAudio(CCmdUI* pCmdUI) {
//	pCmdUI->Enable(  ? 1 : 0)	;
	pCmdUI->SetCheck(isRecordingAudio() ? 1 : 0)	;
	}


void CVidsendDoc22::OnAudioInformazioni() {
	CString S,S1;
	DWORD n,n1,n2;
	CVidsendView22 *v=(CVidsendView22 *)getView();

		if(streamSocketA && Opzioni & CVidsendDoc22::maySendAudio) {
			S1.Format("Trasmissione audio compresso frequenza %u, %u bit, %u canale(i), compressione 0x%X",
				v->wfd.wf.nSamplesPerSec,v->wfd.wf.wBitsPerSample,v->wfd.wf.nChannels,v->wfd.wf.wFormatTag);
			S+=S1+"\n";
			S1.Format("Trasmissione audio MP3 frequenza %u, %u bit, %u canale(i)",
				v->wfex.nSamplesPerSec,v->wfex.wBitsPerSample,v->wfex.nChannels);
			S+=S1+"\n";
			}
		n=calcBandWidth();
		S1.Format("Bitrate: %uKbps",n/128);					//*8 e /1K
		S+=S1;
		{
		POSITION po;
		n=n1=n2=0;
		po=streamSocketA2->cSockRoot.GetHeadPosition();
		if(po) {
			do {
				n+=streamSocketA2->cSockRoot.GetNext(po)->sentFrame;
				n1+=streamSocketA2->cSockRoot.GetNext(po)->stops;
				n2+=streamSocketA2->cSockRoot.GetNext(po)->skippedFrame;
				} while(po);
			}
		S1.Format("Frame inviati: %u, STOP ricevuti: %u, frame NON inviati",n,n1,n2);
		S+=S1;
		}

	AfxMessageBox(S,MB_ICONINFORMATION);
	}


void CVidsendDoc22::OnAudioAudio() {
	CVidsendView22 *v=(CVidsendView22 *)getView();

	bAudio=!bAudio;
	((CChildFrame22 *)v->GetParent())->setStatusIcons();
	}

void CVidsendDoc22::OnUpdateAudioAudio(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(bAudio);
	pCmdUI->Enable(Opzioni & maySendAudio ? TRUE : FALSE);
	}

void CVidsendDoc22::OnAudioLivellivolume() {
	WinExec("sndvol32.exe",SW_SHOWNORMAL);
	WinExec("sndvol.exe",SW_SHOWNORMAL);			// per Windows 7 !!!
	}



void CVidsendDoc22::setTXMode(int mode) {
	CVidsendView22 *v=(CVidsendView22 *)getView();
	HRESULT hr; 
	
	if(mode>=0) {
		v->gdaFrameNum=0;
//		v->KillTimer(1);
/*		if(v->m_AudioThread) {
			TerminateThread(v->m_AudioThread->m_hThread,-1);
			v->m_AudioThread=NULL;
			}*/
		if(psAudio) {
			psAudio->Close();
			delete psAudio; psAudio=NULL;
			}
		if(v->PBMP3buffer)
			delete v->PBMP3buffer;
		v->PBMP3buffer=NULL;
		}
	switch(mode) {
		case 0:				// live
			trasmMode=0;
			v->initCapture(NULL, TRUE);		// in effetti initCapture servirebbe anche negli altri casi, ora
			break;
		case 1:				// canzone
/*			if(v->m_pwi)
				v->m_pwi->StopRecord();*/

			if(Opzioni & maySendAudio) {
				psAudio=new CFile;
				if(!psAudio->Open(nomeMP3_PB,CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite)) {
					delete psAudio; 	psAudio=NULL;
//					goto case_2;
					}
				}
			v->PBMP3from=v->PBMP3to=0;
			if(!v->PBMP3buffer)
				v->PBMP3buffer=new BYTE[44100L*2*2 *2];		// diciamo 2 secondi per sicurezza! v. di là
			v->PBMP3bufferSize=v->PBMP3bufferPointer=0;

//			v->SetTimer(1,1000/AUDIO_BUFFER_DIVIDER,NULL);
//			AfxBeginThread(CVidsendView22::audioProva,v);
			trasmMode=1;
			v->VUMeter3->SetWindowText(0);

error1:
			break;
		case 2:				// test
case_2:
/*			if(v->m_pwi)
				v->m_pwi->StopRecord();*/
//			v->SetTimer(1,1000/AUDIO_BUFFER_DIVIDER,NULL);
//			AfxBeginThread(CVidsendView22::audioProva,v);
			trasmMode=2;
			v->VUMeter3->SetWindowText(0);
			break;
		}
	if(mode>=0)
		((CChildFrame22 *)v->GetParent())->setStatusIcons();
	}

BOOL CVidsendDoc22::startSaveFile(CString S,int m) {

	if(!saveFile && !S.IsEmpty()) {
		saveFile=new CFile;
		saveFile->Open(S,CFile::modeCreate | CFile::modeWrite | CFile::typeBinary | CFile::shareExclusive);
		return 1;
		}
	return 0;
	}
BOOL CVidsendDoc22::endSaveFile() {

	if(saveFile) {
		struct ID3_TAG id3;
		ZeroMemory(&id3,sizeof(struct ID3_TAG));
		id3.tag[0]='T'; id3.tag[1]='A'; id3.tag[2]='G';
		strcpy(id3.titolo, streamTitle);
		strcpy(id3.autore, theApp.infoUtente.cognome /*"DarioG"*/);
		// OCCHIO lunghezza 30!
		strcat(id3.autore, theApp.infoUtente.nome);
		strcpy(id3.anno, "2023");
		id3.genere=186;		// Podcast
		saveFile->Write(&id3,128);
		saveFile->Close();
		delete saveFile; saveFile=NULL;
		return 1;
		}
	return 0;
	}

struct STREAM_INFO *CVidsendDoc22::getConnectionInfo() {
	struct STREAM_INFO *si;
	CVidsendView22 *v=(CVidsendView22 *)getView();

	si=new struct STREAM_INFO;

/*#include <mmreg.h>	
	char buf[32];
	wsprintf(buf,"%u, %u",sizeof(struct STREAM_INFO),sizeof(struct EXT_WAVEFORMATEX));
	AfxMessageBox(buf);*/


	if(si) {
		ZeroMemory(si,sizeof(si));
		si->versione=VIDSEND_VERSIONE;
		if(Opzioni & maySendAudio)
			memcpy(&si->wf,&v->wfd,sizeof(v->wfd));
		else
			si->wf.wf.wFormatTag=0;
		if(Opzioni & timedConnection)
			si->maxTime=timedConnLenght;
		else
			si->maxTime=0;
		switch((Opzioni & needAuthenticate) >> 29) {
			case 2:
			case 1:
				_tcscpy(si->authenticationWWW,(LPCTSTR)authenticationWWW);
				break;
			case 0:
				*si->authenticationWWW=0;
				break;
			}
		if(Opzioni & openWWW)
			_tcscpy(si->openWWW,(LPCTSTR)forceOpenWWW);
		else
			*si->openWWW=0;
		si->IDServer=myID;
		si->dontSave=Opzioni & dontSave;
		si->noBuffers=FALSE;
		si->remoteCtrl=0;		// finire!
		_tcscpy(si->streamTitle,(LPCTSTR)streamTitle);
		_tcscpy(si->splashOrIntro,(LPCTSTR)splashOrIntroName);
		}

	return si;
	}


int CVidsendDoc22::calcBandWidth() {		// quella in uso al momento...
	CVidsendView22 *v=(CVidsendView22 *)getView();

	return calcBandWidth(Opzioni & CVidsendDoc22::maySendAudio,v->wfd.wf.nSamplesPerSec,
		v->wfd.wf.wBitsPerSample,v->wfd.wf.nChannels,myQA.compressor,myQA.quality);
	}

int CVidsendDoc22::calcBandWidth(int aq) {
	int i,n;

	if(aq >= 7)
		return -1;
	n=calcBandWidth(aq>=0,qsa[aq].samplesPerSec,qsa[aq].bitsPerSample,qsa[aq].channels,myQA.compressor,qsa[aq].quality);
	return n;
	}

int CVidsendDoc22::calcBandWidth(BOOL bAudio,DWORD samplesPerSec,WORD bitsPerSample,WORD channels,DWORD compressorA,int qualityA,
																CString *info) {
	int i;
	DWORD nv=0,na=0,t,t1;

	if(bAudio) {
		if(compressorA) {
			WAVEFORMATEX wf;
			HACMSTREAM hAcm;
			wf.wFormatTag = WAVE_FORMAT_PCM;
			wf.nChannels = channels;
			wf.nSamplesPerSec = samplesPerSec;
			wf.nBlockAlign = 1;
			wf.wBitsPerSample = bitsPerSample;
			wf.nAvgBytesPerSec = wf.nSamplesPerSec*wf.nChannels*(wf.wBitsPerSample/8);
			wf.cbSize = 0;



			/* v. per MP3 https://social.msdn.microsoft.com/Forums/en-US/5345daf5-2aca-4956-a45c-d8ef494af5da/how-to-play-mp3?forum=vssmartdevicesnative

	WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = 1;
    waveFormat.nChannels = NCHANNEL;
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.nAvgBytesPerSec = SAMPLE_RATE * NCHANNEL * BYTE_PER_SAMPLE;
    waveFormat.nBlockAlign = NCHANNEL * BYTE_PER_SAMPLE;
    waveFormat.wBitsPerSample = BYTE_PER_SAMPLE * BITS_PER_BYTES;
    waveFormat.cbSize = 0;

	MPEGLAYER3WAVEFORMAT mp3Format;
	ZeroMemory(&mp3Format, sizeof(MPEGLAYER3WAVEFORMAT));
	mp3Format.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
	mp3Format.wfx.nChannels = NCHANNEL;
	mp3Format.wfx.nSamplesPerSec = SAMPLE_RATE;
	mp3Format.wfx.nAvgBytesPerSec = SAMPLE_RATE * NCHANNEL * BYTE_PER_SAMPLE;
	mp3Format.wfx.nBlockAlign = NCHANNEL * BYTE_PER_SAMPLE;
	mp3Format.wfx.wBitsPerSample = 0;//BYTE_PER_SAMPLE * BITS_PER_BYTES;
	mp3Format.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
	mp3Format.wID = MPEGLAYER3_ID_MPEG;
	mp3Format.fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
	mp3Format.nFramesPerBlock = 1;
	mp3Format.nBlockSize =  mp3Format.wfx.nAvgBytesPerSec * 8 / mp3Format.wfx.nSamplesPerSec;
	mp3Format.nCodecDelay = 1393;

	switch(acmStreamOpen(NULL, NULL, (LPWAVEFORMATEX)&mp3Format, &waveFormat, NULL, 0, 0, ACM_STREAMOPENF_QUERY))
	*/
			switch(compressorA) {
				case WAVE_FORMAT_GSM610:
					GSM610WAVEFORMAT mywfx;
					samplesPerSec=8000;
					mywfx.wfx.wFormatTag = compressorA;
					mywfx.wfx.nChannels = 1;
					mywfx.wfx.nSamplesPerSec = (samplesPerSec/320)*320;
					mywfx.wfx.nAvgBytesPerSec = 1625;
					mywfx.wfx.nBlockAlign = 65;
					mywfx.wfx.wBitsPerSample = 0;
					mywfx.wfx.cbSize = 2;
					mywfx.wSamplesPerBlock = 320;
					acmStreamOpen(&hAcm,NULL,&wf,(WAVEFORMATEX *)&mywfx,NULL,NULL,0,0);
					if(hAcm) {
						acmStreamSize(hAcm,wf.nAvgBytesPerSec,&na,ACM_STREAMSIZEF_SOURCE);
						acmStreamClose(hAcm,0);
						}
					else {
						na=-1;
						}
					break;
				case WAVE_FORMAT_MPEGLAYER3:
					MPEGLAYER3WAVEFORMAT mp3Format;
					ZeroMemory(&mp3Format, sizeof(MPEGLAYER3WAVEFORMAT));
					mp3Format.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
					mp3Format.wfx.nChannels = channels;
					mp3Format.wfx.nSamplesPerSec = samplesPerSec;
					mp3Format.wfx.nAvgBytesPerSec = samplesPerSec * channels * bitsPerSample;
					mp3Format.wfx.nBlockAlign = (channels*bitsPerSample)/8;;
					mp3Format.wfx.wBitsPerSample = 0;//BYTE_PER_SAMPLE * BITS_PER_BYTES;
					mp3Format.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
					mp3Format.wID = MPEGLAYER3_ID_MPEG;

					// v. CMP3Coder (lame) 2023!

					mp3Format.fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
					mp3Format.nFramesPerBlock = 1;
					mp3Format.nBlockSize =  mp3Format.wfx.nAvgBytesPerSec * 8 / mp3Format.wfx.nSamplesPerSec;
					mp3Format.nCodecDelay = 1393;
					if(acmStreamOpen(&hAcm /*NULL*/, NULL, (LPWAVEFORMATEX)&mp3Format, &wf, NULL, 0, 0, ACM_STREAMOPENF_QUERY)) {
						acmStreamSize(hAcm,wf.nAvgBytesPerSec,&na,ACM_STREAMSIZEF_SOURCE);
						acmStreamClose(hAcm,0);
						}
					else {
						na=-1;
						}
//					na=mp3Format.wfx.nAvgBytesPerSec; // FARE!! v.sopra
					na=16384 /*diciamo 128Kbps :) */;
					break;
				case WAVE_FORMAT_PCM:
					na=wf.nAvgBytesPerSec;
					break;
				default:
					na=-1;
					break;
				}
			}
		else {
			na=samplesPerSec*bitsPerSample/8;
			na*=channels;
			}
		}
	else
		na=0;
	if(nv != -1 && na != -1)
		return nv+na;
	else
		return 0;
	}

DWORD CVidsendDoc22::getAVstep(struct QUALITY_MODEL_A *qa) {
	register int i,j;

	if(qa) {
		for(j=0; j<7; j++) {
			if(qa->samplesPerSec <= qsa[j].samplesPerSec) {
				break;
				}
			}
		}
	return MAKELONG(i,j);
	}

void CVidsendDoc22::checkUtenti() {

	if(controlSocket)
		controlSocket->checkUtenti();
	}

void CVidsendDoc22::sendHrtBt() {
	char myBuf[2];
	int i;

	if(authSocket) {
		myBuf[0]=11;
		myBuf[1]=0;
		if((i=authSocket->Send(myBuf,2)) == 2)
			;
		else {
			DWORD n=GetLastError();
			if(theApp.debugMode)
				if(theApp.FileSpool) 
					theApp.FileSpool->print(CLogFile::flagError,"impossibile inviare heartbeat AUTH (errore Send %d,%u)!",i,n);
			}
		}
	else {
		if(theApp.FileSpool) 
			theApp.FileSpool->print(CLogFile::flagError,"impossibile inviare heartbeat AUTH (socket NULLO)!");
		}
	}

int CVidsendDoc22::MandaPipeA(const struct AV_PACKET_HDR *avh,LPARAM lParam) {
	int i;
	DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0; 
  BOOL fSuccess = FALSE;

//https://docs.microsoft.com/en-us/windows/desktop/ipc/multithreaded-pipe-server
	if(pipeA != INVALID_HANDLE_VALUE) {
		cbReplyBytes=lParam;
		// Write the reply to the pipe. 
    fSuccess = WriteFile( 
			pipeA,        // handle to pipe 
			avh,     // buffer to write from 
			cbReplyBytes, // number of bytes to write 
			&cbWritten,   // number of bytes written 
			NULL);        // not overlapped I/O 

    if (!fSuccess || cbReplyBytes != cbWritten) {   
     // _tprintf(TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError()); 
      //break;
      }
		}

	return fSuccess;
	}

int CVidsendDoc22::HandlePipeA() {
  BOOL   fConnected = FALSE,fSuccess = FALSE;
	DWORD cbBytesRead = 0;
  OVERLAPPED oOverlap; 

	// Wait for the client to connect; if it succeeds, 
	// the function returns a nonzero value. If the function
	// returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 

	fConnected = ConnectNamedPipe(pipeA, &oOverlap) ? 
		 TRUE : (GetLastError() == ERROR_PIPE_CONNECTED); 

	if(fConnected) { 

   // Read client requests from the pipe. This simplistic code only allows messages
   // up to BUFSIZE characters in length.
    fSuccess = ReadFile( 
			pipeA,        // handle to pipe 
			pipeABuffer,    // buffer to receive data 
			PIPEBUFSIZE*sizeof(TCHAR), // size of buffer 
			&cbBytesRead, // number of bytes read 
			NULL);        // not overlapped I/O 

    if (!fSuccess || cbBytesRead == 0) {   
      if (GetLastError() == ERROR_BROKEN_PIPE) {
//             _tprintf(TEXT("InstanceThread: client disconnected.\n"), GetLastError()); 
        }
      else {
//             _tprintf(TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError()); 
        }
      }
		else {
			char *p=(char *)GlobalAlloc(GPTR,1024);
			wsprintf(p,"NamedPipe receive: %s",pipeABuffer); 
			theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
			}
		} 

	return fSuccess;
	}


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc3 - log file

IMPLEMENT_DYNCREATE(CVidsendDoc3, CDocument)

CVidsendDoc3::CVidsendDoc3() {

	prfSection="Log";
	m_vidsendSet=NULL;
	}

BOOL CVidsendDoc3::OnNewDocument() {

	if(!CExDocument::OnNewDocument())
		return FALSE;
	SetTitle("Log eventi");

	m_vidsendSet=new CVidsendSet(NULL);

	return TRUE;
	}

CVidsendDoc3::~CVidsendDoc3() {

	delete m_vidsendSet;
	}


BEGIN_MESSAGE_MAP(CVidsendDoc3, CExDocument)
	//{{AFX_MSG_MAP(CVidsendDoc3)
	ON_COMMAND(ID_FILE_PROPRIETA, OnFileProprieta)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc3 diagnostics

#ifdef _DEBUG
void CVidsendDoc3::AssertValid() const
{
	CExDocument::AssertValid();
}

void CVidsendDoc3::Dump(CDumpContext& dc) const
{
	CExDocument::Dump(dc);
}
#endif //_DEBUG


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc3 commands

void CVidsendDoc3::OnCloseDocument() {

	CExDocument::OnCloseDocument();
	theApp.theLog=NULL;
	}


void CVidsendDoc3::OnFileProprieta() {
	CVidsendPropPage mySheet("Proprietà logging",(CVidsendView3 *)getView());
	CVidsendDoc3PropPage0 myPage0(this);
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		if(0)
			goto fine;
		}
	mySheet.AddPage(&myPage0);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
		if(myPage0.isInitialized) {
			theApp.OpzioniLog = 0;
			theApp.OpzioniLog |= myPage0.m_LogAttivo ? logAttivo : 0;
			}
		
		}
fine:
		;
	}




/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4

IMPLEMENT_DYNCREATE(CVidsendDoc4, CDocument)

CVidsendDoc4::CVidsendDoc4() {
	int i;
	char myBuf[128],*p;

	AfxInitRichEdit();			// server per usare i Rich Edit sia 1.0 che 2.0

	prfSection="Chat";
	Opzioni=GetPrivateProfileInt(IDS_OPZIONI);
#ifdef _NEWMEET_MODE
	Opzioni |= slaveMode | serverMode | usaColors | usaSounds;		
	Opzioni &= ~(dontSave | noFreeChat | noOne2One | onlyOne2One);	
			// forse serverMode non servirebbe... arriva dal "forza server chat" in App
#endif
	opzioniVisive=GetPrivateProfileInt(IDS_OPZIONI4);
#ifdef _NEWMEET_MODE
	opzioniVisive |= avvisi_sonori | avvisi_sonori2;
#endif
	maxConn=GetPrivateProfileInt(IDS_MAXCONN);
	if(!maxConn)
		maxConn=2;
#ifdef _NEWMEET_MODE
#ifdef _CAMPARTY_MODE
	maxConn=20;
#else
	maxConn=20;		// 17/5/03
#endif
#endif
	if(Opzioni & serverMode) {
		i=chatSocket.Create(TEXT_SOCKET /*,SOCK_STREAM,(char *)&sock_in*/ , theApp.Opzioni & CVidsendApp::TCP_UDP ? SOCK_DGRAM : SOCK_STREAM);
		if(!(theApp.Opzioni & CVidsendApp::TCP_UDP)) {
			if(!(i=chatSocket.Listen())) {
				p="Impossibile installare Chat server";
				AfxMessageBox(p,MB_ICONHAND);
				if(theApp.FileSpool)
					*theApp.FileSpool << p;
				}
			}
		else {
			}
		}
	maxMessaggi=GetPrivateProfileInt(IDS_MAXMSG);
	if(!maxMessaggi)
		maxMessaggi=100;
#ifdef _NEWMEET_MODE
  loginName=theApp.infoUtente.login;
#else
  GetPrivateProfileString(IDS_USER,myBuf,31);
  loginName=myBuf;
#endif
#ifdef _NEWMEET_MODE
  loginPasw=theApp.infoUtente.pasw;
#else
  GetPrivateProfileString(IDS_PASW,myBuf,31);
  loginPasw=myBuf;
#endif
	GetPrivateProfileString(IDS_FORCEOPENWWW,myBuf,127);
  forceOpenWWW=myBuf;
  GetPrivateProfileString(IDS_AUTHSERVER,myBuf,127);
  authenticationWWW=myBuf;
	firstConnect=1;
	}

BOOL CVidsendDoc4::OnNewDocument() {
	int i;

	if(!CExDocument::OnNewDocument())
		return FALSE;
	SetTitle("Chat");

	if(Opzioni & doDialUp) {
		i=theApp.callRAS((LPCTSTR)theApp.DialUpNome);
#ifdef _CAMPARTY_MODE
		if(!i)
			return FALSE;
#endif
		}

#if defined(_NEWMEET_MODE) || defined(_CAMPARTY_MODE)  // idem 2023
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		char myBuf[64];
		RECT rc;
		CVidsendView4 *w=(CVidsendView4 *)getView();

		SetRectEmpty(&rc);
		GetPrivateProfileString(IDS_COORDINATE,myBuf,32);
#ifdef _NEWMEET_MODE
#ifndef _CAMPARTY_MODE
		_tcscpy(myBuf,"327,0,686,460" /*686 era 806 con la fin. utenti conn., che pero' non vogliamo*/);
#else
		_tcscpy(myBuf,"0,269,326,510");
#endif
#endif
		sscanf(myBuf,"%d,%d,%d,%d",&rc.left,&rc.top,&rc.right,&rc.bottom);	// sono coord. client rispetto alla MDIFRAME madre della mia ChildFrame
		if(!IsRectEmpty(&rc))
			w->GetParent()->SetWindowPos(NULL,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);
		}
#endif

	return TRUE;
	}

CVidsendDoc4::~CVidsendDoc4() {

/*	if(chatSocket) {			//
		chatSocket->Close();
		delete chatSocket;
		}
	chatSocket=NULL;*/
	chatSocket.Close();

	save();
  WritePrivateProfileInt(IDS_OPZIONI,Opzioni);
  WritePrivateProfileInt(IDS_OPZIONI4,opzioniVisive);
  WritePrivateProfileInt(IDS_MAXCONN,maxConn);
  WritePrivateProfileInt(IDS_MAXMSG,maxMessaggi);
  WritePrivateProfileString(IDS_USER,(char *)(LPCTSTR)loginName);
  WritePrivateProfileString(IDS_PASW,(char *)(LPCTSTR)loginPasw);
  WritePrivateProfileString(IDS_AUTHSERVER,(char *)(LPCTSTR)authenticationWWW);
  WritePrivateProfileString(IDS_FORCEOPENWWW,(char *)(LPCTSTR)forceOpenWWW);
	}

CString CVidsendDoc4::loadCliConnRecent(CString s[MRU_SIZE]) {
	char myBuf[64],myBuf2[256],ks[32];
	int i;

	LoadString(theApp.m_hInstance,IDS_RECENTCONN,ks,32);
	for(i=0; i<MRU_SIZE; i++) {
		wsprintf(myBuf,"%s%u",ks,i);
		theApp.prStore->GetPrivateProfileString(prfSection,myBuf,myBuf2,159);
		s[i]=myBuf2;
		}
	return s[0];
	}


int CVidsendDoc4::loadBlacklistedIP(CStringList *s) {
	char myBuf[64],myBuf2[256],ks[32];
	int i;

	LoadString(theApp.m_hInstance,IDS_BLACKLIST,ks,32);
	i=0;
	do {
		wsprintf(myBuf,"%s%u",ks,i);
		theApp.prStore->GetPrivateProfileString(prfSection,myBuf,myBuf2,159);
		if(*myBuf2) {
			s->AddTail(myBuf2);
			i++;
			}
		} while(*myBuf2);
	return i;
	}

int CVidsendDoc4::saveBlacklistedIP(CStringList *s) {
	char myBuf[64],myBuf2[256],ks[32];
	int i;
	POSITION po;

	i=0;
	LoadString(theApp.m_hInstance,IDS_BLACKLIST,ks,32);
	po=s->GetHeadPosition();
	while(po) {
		wsprintf(myBuf,"%s%u",ks,i);
		theApp.prStore->WritePrivateProfileString(prfSection,myBuf,s->GetAt(po));
		i++;
		s->GetNext(po);
		}
	wsprintf(myBuf,"%s%u",ks,i);			// pulisce il prox, per stroncare la lista!
	theApp.prStore->WritePrivateProfileString(prfSection,myBuf,"");
	return i;
	}


BOOL CVidsendDoc4::save() {
	CString prevURL[MRU_SIZE];
	char myBuf[64],ks[32];
	int i,j,n;

	if(!srvAddress.IsEmpty()) {
		loadCliConnRecent(prevURL);
		for(i=0; i<MRU_SIZE; i++) {
			if(prevURL[i] ==srvAddress) {
				prevURL[i].Empty();
				}
			}
		n=MRU_SIZE;
		for(i=0; i<n; i++) {
			if(prevURL[i].IsEmpty()) {
				for(j=i+1; j<n; j++)
					prevURL[j-1]=prevURL[j];
				prevURL[--n].Empty();
				i--;
				}
			}
		for(i=MRU_SIZE-2; i>=0; i--) {
			prevURL[i+1]=prevURL[i];
			}
		prevURL[0]=srvAddress;
		LoadString(theApp.m_hInstance,IDS_RECENTCONN,ks,32);
		for(i=0; i<MRU_SIZE; i++) {
			wsprintf(myBuf,"%s%u",ks,i);
			theApp.prStore->WritePrivateProfileString(prfSection,myBuf,(LPCTSTR)prevURL[i]);
			}
		}
	return 1;
	}

void CVidsendDoc4::OnCloseDocument() {

	Opzioni &= ~CVidsendDoc4::slaveMode;
	theApp.theChat=NULL;
	CExDocument::OnCloseDocument();
	}


BEGIN_MESSAGE_MAP(CVidsendDoc4, CExDocument)
	//{{AFX_MSG_MAP(CVidsendDoc4)
	ON_COMMAND(ID_FILE_PROPRIETA, OnFileProprieta)
	ON_COMMAND(ID_VISUALIZZA_AVVISISONORI, OnVisualizzaAvvisisonori)
	ON_COMMAND(ID_VISUALIZZA_IMPOSTACOLORE, OnVisualizzaImpostacolore)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_AVVISISONORI, OnUpdateVisualizzaAvvisisonori)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_IMPOSTACOLORE, OnUpdateVisualizzaImpostacolore)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4 diagnostics

#ifdef _DEBUG
void CVidsendDoc4::AssertValid() const
{
	CExDocument::AssertValid();
}

void CVidsendDoc4::Dump(CDumpContext& dc) const
{
	CExDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4 serialization

void CVidsendDoc4::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4 commands

void CVidsendDoc4::OnFileProprieta() {
	int i;
	CStringList bl;

#ifndef _NEWMEET_MODE
	CVidsendPropPage mySheet("Proprietà chat",(CVidsendView4 *)getView());
	CVidsendDoc4PropPage0 myPage0(this);
	CVidsendDoc4PropPage1 myPage1(&bl,this);
	CVidsendDoc4PropPage2 myPage2(this);
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		if(0)
			goto fine;
		}
	mySheet.AddPage(&myPage0);
	if(theApp.Opzioni & CVidsendApp::canSendText)
		mySheet.AddPage(&myPage1);
	mySheet.AddPage(&myPage2);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
		if(myPage0.isInitialized) {
			maxConn=myPage0.m_MaxConn;
			Opzioni &= 0xffffff ;
			Opzioni |= myPage0.m_AncheWWW ? ancheAccessoWeb : 0;
			Opzioni |= myPage0.m_One2One ? onlyOne2One : 0;
			Opzioni |= myPage0.m_noOne2One ? noOne2One : 0;
			if(Opzioni & noOne2One)
				Opzioni &= ~onlyOne2One;
			if(Opzioni & CVidsendDoc4::onlyOne2One) {
//				((CButton *)getView()->GetDlgItem(IDC_BUTTON5))->SetState(1);
				((CButton *)getView()->GetDlgItem(IDC_BUTTON5))->SetCheck(1);
				}
			else {
//				((CButton *)getView()->GetDlgItem(IDC_BUTTON5))->SetState(0);
				((CButton *)getView()->GetDlgItem(IDC_BUTTON5))->SetCheck(0);
				}
			}
		
		if(myPage1.isInitialized) {
			Opzioni &= 0xff0000ff;
			Opzioni |= myPage1.m_Attivo ? serverMode : 0;
			Opzioni |= myPage1.m_bOpenWWW ? openWWW : 0;
			Opzioni |= myPage1.m_bAuthWWW ? needAuthenticate : 0;
			Opzioni |= myPage1.m_bTimedConn ? timedConnection : 0;
			Opzioni |= myPage1.m_DontSave ? dontSave : 0;
			Opzioni |= myPage1.m_Mostra_E_U ? mostraEU : 0;
			Opzioni |= myPage1.m_bUsaSuoni ? usaSounds : 0;
			Opzioni |= myPage1.m_bUsaColori ? usaColors : 0;
			Opzioni |= myPage1.m_bUsaIcone ? usaIcons : 0;
			Opzioni |= myPage1.m_NoPrivate ? noPrivateMsg : 0;
			forceOpenWWW=myPage1.m_OpenWWW;
			authenticationWWW=myPage1.m_AuthWWW;
			{ CTime myT(2000,1,1,0,0,0);
			timedConnLenght=myPage1.m_TimedConn-myT;
			}
			saveBlacklistedIP(&bl);
			}
		if(myPage2.isInitialized) {
			Opzioni &= 0xffffff00;
			Opzioni |= myPage2.m_bProxy ? usaProxy : 0;
			Opzioni |= myPage2.m_bConnetti ? doDialUp : 0;
			loginName=myPage2.m_User;
			loginPasw=myPage2.m_Pasw;
			maxMessaggi=myPage2.m_MaxMessaggi;
			opzioniVisive = myPage2.m_bSuoni ? (avvisi_sonori | avvisi_sonori2 /* per ora insieme!*/ ) : 0;
			opzioniVisive |= myPage2.m_bColore ? testo_colorato : 0;
			opzioniVisive |= myPage2.m_colore;
			}
		}

#else
	CVidsendDoc4PropPage0_NM myDlg(&bl,this);
	
	if(myDlg.DoModal() == IDOK) {
		maxConn=myDlg.m_MaxConn;
		Opzioni &= ~(ancheAccessoWeb | saveMessages | mostraEU | noPrivateMsg | 
			usaProxy | noFreeChat |	usaSounds | usaColors | usaIcons | 
			onlyOne2One | noOne2One);
		Opzioni |= myDlg.m_Mostra_E_U ? mostraEU : 0;
		Opzioni |= myDlg.m_NoPrivate ? noPrivateMsg : 0;
		Opzioni |= myDlg.m_bProxy ? usaProxy : 0;
		Opzioni |= myDlg.m_NoFreeChat ? noFreeChat : 0;
		Opzioni |= myDlg.m_bUsaSuoni ? usaSounds : 0;
		Opzioni |= myDlg.m_bUsaColori ? usaColors : 0;
		Opzioni |= myDlg.m_bUsaIcone ? usaIcons : 0;
		Opzioni |= myDlg.m_One2One ? onlyOne2One : 0;
		Opzioni |= myDlg.m_noOne2One ? noOne2One : 0;
		if(Opzioni & noOne2One)
			Opzioni &= ~onlyOne2One;
		maxMessaggi=myDlg.m_MaxMessaggi;
		opzioniVisive = myDlg.m_bSuoni ? (avvisi_sonori | avvisi_sonori2 /* per ora insieme!*/ ) : 0;
		opzioniVisive |= myDlg.m_bColore ? testo_colorato : 0;
		opzioniVisive |= myDlg.m_colore;

		if(Opzioni & CVidsendDoc4::onlyOne2One) {
//			((CButton *)getView()->GetDlgItem(IDC_BUTTON5))->SetState(1);
			((CButton *)getView()->GetDlgItem(IDC_BUTTON5))->SetCheck(1);
			}
		else {
//			((CButton *)getView()->GetDlgItem(IDC_BUTTON5))->SetState(0);
			((CButton *)getView()->GetDlgItem(IDC_BUTTON5))->SetCheck(0);
			}
		saveBlacklistedIP(&bl);

		AfxMessageBox("Riavviare il programma per rendere effettive le modifiche!",MB_ICONEXCLAMATION);
		}


#endif

fine:
		;
	}

void CVidsendDoc4::OnVisualizzaAvvisisonori() {

	opzioniVisive ^= avvisi_sonori;
	}

void CVidsendDoc4::OnUpdateVisualizzaAvvisisonori(CCmdUI* pCmdUI) {

	pCmdUI->SetCheck(opzioniVisive & avvisi_sonori);
	}

void CVidsendDoc4::OnVisualizzaImpostacolore() {
	CColorDialog cd;
	
	cd.m_cc.rgbResult=opzioniVisive & 0xffffff;
	cd.m_cc.Flags |= CC_RGBINIT;
	if(cd.DoModal() == IDOK) {
		opzioniVisive = (opzioniVisive & (avvisi_sonori | avvisi_sonori2 | testo_colorato)) | cd.GetColor();
		}
	}

void CVidsendDoc4::OnUpdateVisualizzaImpostacolore(CCmdUI* pCmdUI) {
	pCmdUI->Enable(opzioniVisive & testo_colorato);
	}

int CVidsendDoc4::updateTree() {
	CVidsendView4 *v=(CVidsendView4 *)getView();

	if(Opzioni & serverMode && v)
		return v->updateTree();
	else
		return FALSE;
	}

struct CHAT_INFO *CVidsendDoc4::getConnectionInfo() {
	struct CHAT_INFO *si;

	si=new struct CHAT_INFO;
	if(si) {
		ZeroMemory(si,sizeof(struct CHAT_INFO));
		si->maxTime=0;
		*si->authenticationWWW=0;
		if(Opzioni & openWWW)
			_tcscpy(si->openWWW,(LPCTSTR)forceOpenWWW);
		else
			*si->openWWW=0;
		}
	return si;
	}

void CVidsendDoc4::sendHrtBt() {
	char myBuf[2];
	CVidsendView4 *w=(CVidsendView4 *)getView();
	
#ifndef _NEWMEET_MODE				// era cosi'... per ora non tocchiamo!!
#endif											// anche qua? v.sopra
	if(w && w->cliSock) {
		myBuf[0]=11;
		myBuf[1]=0;
		w->cliSock->Send(myBuf,2);
		}
	}







/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc5

IMPLEMENT_DYNCREATE(CVidsendDoc5, CDocument)

CVidsendDoc5::CVidsendDoc5() {

	prfSection="ClientWWW";
	home=_T("192.168.1.2");
	URL=home;
	mode=0;
	}

CVidsendDoc5::~CVidsendDoc5() {

	theApp.m_pMainWnd->PostMessage(WM_CLOSE_CHILD,5 /*browser*/,(LPARAM)this);
	save();
	}

BOOL CVidsendDoc5::OnNewDocument() {
	
	if(!CExDocument::OnNewDocument())
		return FALSE;

	return TRUE;
	}

void CVidsendDoc5::setURL(CString S) {
	CVidsendView5 *v=(CVidsendView5 *)getView();
	
	URL=S;
	if(v) {
		((CChildFrame5 *)v->GetParent())->m_wndDlgBar.SetDlgItemText(IDC_COMBO1,URL);
		v->Navigate(URL);

		RECT rc;

		theApp.m_pMainWnd->GetClientRect(&rc);

		if(mode) {
			v->GetParent()->SetWindowPos(&CWnd::wndTopMost,rc.left,rc.top,rc.right,rc.bottom,0);
			}
		else {
			}
		}
	}

void CVidsendDoc5::setMode(int m) {

	mode=m;
	}

CString CVidsendDoc5::loadCliConnRecent(CString s[MRU_SIZE]) {
	char myBuf[64],myBuf2[256],ks[32];
	int i;

	LoadString(theApp.m_hInstance,IDS_RECENTCONN,ks,32);
	for(i=0; i<MRU_SIZE; i++) {
		wsprintf(myBuf,"%s%u",ks,i);
		theApp.prStore->GetPrivateProfileString(prfSection,myBuf,myBuf2,159);
		s[i]=myBuf2;
		}
	return s[0];
	}

BOOL CVidsendDoc5::save() {
	CString prevURL[MRU_SIZE];
	char myBuf[64],ks[32];
	int i,j,n;

	if(!URL.IsEmpty()) {
		loadCliConnRecent(prevURL);
		for(i=0; i<MRU_SIZE; i++) {
			if(prevURL[i] ==URL) {
				prevURL[i].Empty();
				}
			}
		n=MRU_SIZE;
		for(i=0; i<n; i++) {
			if(prevURL[i].IsEmpty()) {
				for(j=i+1; j<n; j++)
					prevURL[j-1]=prevURL[j];
				prevURL[--n].Empty();
				i--;
				}
			}
		for(i=MRU_SIZE-2; i>=0; i--) {
			prevURL[i+1]=prevURL[i];
			}
		prevURL[0]=URL;
		LoadString(theApp.m_hInstance,IDS_RECENTCONN,ks,32);
		for(i=0; i<MRU_SIZE; i++) {
			wsprintf(myBuf,"%s%u",ks,i);
			theApp.prStore->WritePrivateProfileString(prfSection,myBuf,(LPCTSTR)prevURL[i]);
			}
		}
	return 1;
	}



BEGIN_MESSAGE_MAP(CVidsendDoc5, CExDocument)
	//{{AFX_MSG_MAP(CVidsendDoc5)
	ON_COMMAND(ID_FILE_PROPRIETA, OnFileProprieta)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc5 diagnostics

#ifdef _DEBUG
void CVidsendDoc5::AssertValid() const
{
	CExDocument::AssertValid();
}

void CVidsendDoc5::Dump(CDumpContext& dc) const
{
	CExDocument::Dump(dc);
}
#endif //_DEBUG


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc5 commands

void CVidsendDoc5::OnFileProprieta() {
	int i;
	CVidsendPropPage mySheet("Proprietà browser",(CVidsendView5 *)getView());
	CVidsendDoc5PropPage0 myPage0;
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		if(0)
			goto fine;
		}
	mySheet.AddPage(&myPage0);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
		if(myPage0.isInitialized) {
			Opzioni &= 0xffffff;
			}
		
		}
fine:
		;
	}





/////////////////////////////////////////////////////////////////////////////
// CVidesendDoc6

IMPLEMENT_DYNCREATE(CVidsendDoc6, CDocument)

CVidsendDoc6::CVidsendDoc6() {

	prfSection="Connessioni";
	Opzioni=GetPrivateProfileInt(IDS_OPZIONI);
	}


BOOL CVidsendDoc6::OnNewDocument() {
	CString S;

	if(!CExDocument::OnNewDocument())
		return FALSE;
	S="Connessioni";
	if(*theApp.ODBCname) {
		S+=" (";
		S+=theApp.ODBCname;
		S+=")";
		}
	SetTitle(S);

#ifdef _NEWMEET_MODE
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		char myBuf[64];
		RECT rc;
		CVidsendView6 *w=(CVidsendView6 *)getView();

		SetRectEmpty(&rc);
		GetPrivateProfileString(IDS_COORDINATE,myBuf,32);
#ifdef _NEWMEET_MODE
		_tcscpy(myBuf,"0,269,326,442");
#endif
		sscanf(myBuf,"%d,%d,%d,%d",&rc.left,&rc.top,&rc.right,&rc.bottom);	// sono coord. client rispetto alla MDIFRAME madre della mia ChildFrame
		if(!IsRectEmpty(&rc))
			w->GetParent()->SetWindowPos(NULL,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);
		}
#endif

	return TRUE;
	}

CVidsendDoc6::~CVidsendDoc6() {
  WritePrivateProfileInt(IDS_OPZIONI,Opzioni);
	}


BEGIN_MESSAGE_MAP(CVidsendDoc6, CExDocument)
	//{{AFX_MSG_MAP(CVidsendDoc6)
	ON_COMMAND(ID_FILE_PROPRIETA, OnFileProprieta)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidesendDoc6 diagnostics

#ifdef _DEBUG
void CVidsendDoc6::AssertValid() const
{
	CExDocument::AssertValid();
}

void CVidsendDoc6::Dump(CDumpContext& dc) const
{
	CExDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidesendDoc6 serialization

void CVidsendDoc6::Serialize(CArchive& ar) {

	if(ar.IsStoring()) {
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CVidesendDoc6 commands

void CVidsendDoc6::OnCloseDocument() {

	CExDocument::OnCloseDocument();
	theApp.theConnections=NULL;
	}



void CVidsendDoc6::OnFileProprieta() {
	int i;
	CVidsendPropPage mySheet("Proprietà gestione connessioni",(CVidsendView6 *)getView());
	CVidsendDoc6PropPage0 myPage0(this);
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		if(0)
			goto fine;
		}
	mySheet.AddPage(&myPage0);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
		if(myPage0.isInitialized) {
			Opzioni = 0;
			Opzioni |= myPage0.m_AncheDirSrv ? mostraAncheDirSrv : 0;
			Opzioni |= myPage0.m_IPlookup ? visualizzaIpLookup : 0;
			theApp.maxHTMLconn=myPage0.m_MaxHTMLconn;
			if(theApp.theServer) {
				theApp.theServer->maxConn=myPage0.m_Maxconn;
				}
			if(theApp.theServer2) {
				theApp.theServer2->maxConn=myPage0.m_Maxconn;
				}
			}
		
		}
fine:
		;
	}


int CVidsendDoc6::update() {
	CVidsendView6 *v=(CVidsendView6 *)getView();

	if(v)
		return v->update();
	else
		return FALSE;
	}




/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc7

IMPLEMENT_DYNCREATE(CVidsendDoc7, CDocument)

CVidsendDoc7::CVidsendDoc7() {

	prfSection="DirectoryServer";
	}

BOOL CVidsendDoc7::OnNewDocument() {
	
	if(!CExDocument::OnNewDocument())
		return FALSE;
	SetTitle("Server disponibili");

	return TRUE;
	}

CVidsendDoc7::~CVidsendDoc7() {
	}


BEGIN_MESSAGE_MAP(CVidsendDoc7, CExDocument)
	//{{AFX_MSG_MAP(CVidsendDoc7)
	ON_COMMAND(ID_FILE_PROPRIETA, OnFileProprieta)
	ON_COMMAND(ID_COMPUTER_MANDAMESSAGGIO, OnComputerMandamessaggio)
	ON_UPDATE_COMMAND_UI(ID_COMPUTER_MANDAMESSAGGIO, OnUpdateComputerMandamessaggio)
	ON_COMMAND(ID_COMPUTER_DISCONNETTI, OnComputerDisconnetti)
	ON_UPDATE_COMMAND_UI(ID_COMPUTER_DISCONNETTI, OnUpdateComputerDisconnetti)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc7 diagnostics

#ifdef _DEBUG
void CVidsendDoc7::AssertValid() const
{
	CExDocument::AssertValid();
}

void CVidsendDoc7::Dump(CDumpContext& dc) const
{
	CExDocument::Dump(dc);
}
#endif //_DEBUG


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc7 commands

void CVidsendDoc7::OnCloseDocument() {

	CExDocument::OnCloseDocument();
	theApp.theDirectoryServer=NULL;
	}



void CVidsendDoc7::OnFileProprieta() {
	int i;
	CVidsendPropPage mySheet("Proprietà gestione connessioni",(CVidsendView7 *)getView());
	CVidsendDoc7PropPage0 myPage0(this);
	
	if(theApp.Opzioni & CVidsendApp::passwordProtect) {
		if(0)
			goto fine;
		}
	mySheet.AddPage(&myPage0);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
		if(myPage0.isInitialized) {
			theApp.OpzioniDirSrv = 0;
			theApp.OpzioniDirSrv |= myPage0.m_bHTMLAccess ? ancheAccessoWeb : 0;
			theApp.maxDirSrvConn=myPage0.m_Maxconn;
			}
		
		}
fine:
		;
	}



void CVidsendDoc7::OnComputerMandamessaggio() {
	CVidsendView7 *v=(CVidsendView7 *)getView();
	HTREEITEM tp=v->GetTreeCtrl().GetSelectedItem();
	CDirectoryCliSocket *c;

	if(tp) {
		c=(CDirectoryCliSocket *)v->GetTreeCtrl().GetItemData(tp);
		if(c) {
			CInputBoxDlg myDlg;
			if(myDlg.DoModal() == IDOK) {
				c->Send("\x2",1);
				c->Send(myDlg.m_Text,255);
				}
			}
		}
	}

void CVidsendDoc7::OnUpdateComputerMandamessaggio(CCmdUI* pCmdUI) {
	pCmdUI->Enable(((CVidsendView7 *)getView())->GetTreeCtrl().GetSelectedItem() ? TRUE : FALSE);
	}

void CVidsendDoc7::OnComputerDisconnetti() {
	CVidsendView7 *v=(CVidsendView7 *)getView();
	HTREEITEM tp=v->GetTreeCtrl().GetSelectedItem();
	CDirectoryCliSocket *c;
	char myBuf[256];

	if(tp) {
		c=(CDirectoryCliSocket *)v->GetTreeCtrl().GetItemData(tp);
		if(c) {
			if(AfxMessageBox("Disconnettere?",MB_ICONQUESTION | MB_YESNO) == IDYES) {
				c->Close();
				}
			}
		}
	}

void CVidsendDoc7::OnUpdateComputerDisconnetti(CCmdUI* pCmdUI) {
	pCmdUI->Enable(((CVidsendView7 *)getView())->GetTreeCtrl().GetSelectedItem() ? TRUE : FALSE);
	// se e' evidenziata la root, forse non dovrebbe essere attivo!
	}


int CVidsendDoc7::update() {
	CVidsendView7 *v=(CVidsendView7 *)getView();

	if(v)
		return v->update();
	else
		return FALSE;
	}


