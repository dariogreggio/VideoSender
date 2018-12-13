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
#include "cjpeg2.h"

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

		strcat(myBuf,myBuf1);
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
		GlobalFree(lpdm);
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
	}


void CVidsendDoc::OnVisualizzaBn() {
	
	Opzioni ^= CVidsendDoc::fmt_bn;
	}

void CVidsendDoc::OnUpdateVisualizzaBn(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::fmt_bn ? 1 : 0);
	}

void CVidsendDoc::OnVisualizzaDimensionedoppia() {
	
	Opzioni ^= CVidsendDoc::fmt_double;
	Opzioni &= ~CVidsendDoc::fmt_full;
	standardSize();
	}

void CVidsendDoc::OnUpdateVisualizzaDimensionedoppia(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::fmt_double ? 1 : 0);
	}

void CVidsendDoc::OnVisualizzaAtuttoschermo() {
	
	Opzioni ^= CVidsendDoc::fmt_full;
	Opzioni &= ~CVidsendDoc::fmt_double;
	standardSize();
	}

void CVidsendDoc::OnUpdateVisualizzaAtuttoschermo(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::fmt_full ? 1 : 0);
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

void CVidsendDoc::OnVisualizzaStereosimulato() {
	
	Opzioni ^= CVidsendDoc::sstereo;
	}

void CVidsendDoc::OnUpdateVisualizzaStereosimulato(CCmdUI* pCmdUI) {
	
	pCmdUI->SetCheck(Opzioni & CVidsendDoc::sstereo ? 1 : 0);
	}

void CVidsendDoc::OnVisualizzaVolume() {

	WinExec("sndvol32.exe",SW_SHOWNORMAL);
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

CVidsendDoc2::CVidsendDoc2() {
	char myBuf[128];
	
	prfSection="ServerStream";
	Opzioni=GetPrivateProfileInt(IDS_OPZIONI);
	OpzioniSalvaVideo=GetPrivateProfileInt(IDS_OPZIONI2);
	OpzioniSorgenteVideo=GetPrivateProfileInt(IDS_OPZIONI3);
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
	myID=0;
	myTimedConn=0;

	ZeroMemory(&myQV,sizeof(myQV));
	GetPrivateProfileString(IDS_VCOMPRESSORNAME,myBuf,5);
	myQV.compressor=*myBuf ? *((DWORD *)myBuf) : 0L;
	myQV.fps=GetPrivateProfileInt(IDS_FRAMESPERSEC);
#ifndef _NEWMEET_MODE
	GetPrivateProfileString(IDS_COMPRESSORDIMENSIONI,myBuf,32,"160x120");
#else
	GetPrivateProfileString(IDS_COMPRESSORDIMENSIONI,myBuf,32,"320x240");
#endif
	myQV.imageSize.right=atoi(myBuf);		// N.B. stringa vuota=crash... non ho parole!
	myQV.imageSize.bottom=atoi(strchr(myBuf,'x')+1);
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
	controlSocket=new CControlSrvSocket(this);
	controlSocket->Create(CONTROL_SOCKET);
	if(!(theApp.Opzioni & CVidsendApp::TCP_UDP)) {
		controlSocket->Listen();
		}
	else {
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

	pipeV = INVALID_HANDLE_VALUE;
	pipeVBuffer=NULL;


#ifndef _NEWMEET_MODE
	qsv[0].imageSize.right=160;
	qsv[0].imageSize.bottom=120;
	qsv[0].bpp=24;
	qsv[0].fps=2;
	qsv[0].quality=2500;
	qsv[1].imageSize.right=160 /*200*/;
	qsv[1].imageSize.bottom=120 /*150*/;
	qsv[1].bpp=24;
	qsv[1].fps=5;
	qsv[1].quality=2500;
	qsv[2].imageSize.right=320 /*240*/;
	qsv[2].imageSize.bottom=240 /*180*/;
	qsv[2].bpp=24;
	qsv[2].fps=2;
	qsv[2].quality=5000;
	qsv[3].imageSize.right=320 /*240*/;
	qsv[3].imageSize.bottom=240 /*180*/;
	qsv[3].bpp=24;
	qsv[3].fps=5;
	qsv[3].quality=5000;
	qsv[4].imageSize.right=320;
	qsv[4].imageSize.bottom=240;
	qsv[4].bpp=24;
	qsv[4].fps=10;
	qsv[4].quality=5000;
	qsv[5].imageSize.right=320;
	qsv[5].imageSize.bottom=240;
	qsv[5].bpp=24;
	qsv[5].fps=15;
	qsv[5].quality=7500;
	qsv[6].imageSize.right=640;
	qsv[6].imageSize.bottom=480;
	qsv[6].bpp=24;
	qsv[6].fps=25;
	qsv[6].quality=7500;

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
#else
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

	return TRUE;
	}

CVidsendDoc2::~CVidsendDoc2() {
	char myBuf[128];

	if(theTV)
		delete theTV;
	theTV=NULL;
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
	if(streamSocketV) {
		streamSocketV->Close();
		delete streamSocketV;
		}
	streamSocketV=NULL;
	save();
  WritePrivateProfileInt(IDS_MAXCONN,maxConn);
  WritePrivateProfileInt(IDS_OPZIONI,Opzioni);
  WritePrivateProfileInt(IDS_OPZIONI2,OpzioniSalvaVideo);
  WritePrivateProfileInt(IDS_OPZIONI3,OpzioniSorgenteVideo);
  WritePrivateProfileInt(IDS_VIDEOSOURCE,videoSource | (alternaSource ? 0x8000 : 0));
	}

BOOL CVidsendDoc2::save() {
	char myBuf[64];
	int i;

	*((DWORD *)myBuf)=myQV.compressor;
	myBuf[4]=0;
	WritePrivateProfileString(IDS_VCOMPRESSORNAME,myBuf);
	WritePrivateProfileInt(IDS_FRAMESPERSEC,myQV.fps);
	wsprintf(myBuf,"%ux%u",myQV.imageSize.right,myQV.imageSize.bottom);
	WritePrivateProfileString(IDS_COMPRESSORDIMENSIONI,myBuf);
	wsprintf(myBuf,"%d",myQA.compressor);
	WritePrivateProfileInt(IDS_KFRAME,KFrame);
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

	theTV=new CTV(v,OpzioniSorgenteVideo & CVidsendDoc2::useOverlay,&myQV.imageSize,
		24,myQV.fps,myQV.compressor,KFrame,Opzioni & CVidsendDoc2::maySendAudio);
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
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_PAGINADIPROVA, OnUpdateVideoTrasmissionePaginadiprova)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_FILMATO, OnUpdateVideoTrasmissioneFilmato)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_DALVIVO, OnUpdateVideoTrasmissioneDalvivo)
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

//	if(!theApp.Opzioni & CVidsendApp::advancedConf)		// per ora no...
//			theTV->theCapture=myPage0bis.m_Schede-1;

//			if(theApp.Opzioni & CVidsendApp::advancedConf) {
				myQV.compressor=myPage0.m_CompressorV;
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

	if(pipeV) {
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
	CString S;
	
	theTV->theFrame=(BYTE *)-1;
	if(myDlg.DoModal() == IDOK) {
		if(theTV->theFrame && theTV->theFrame!=(BYTE *)-1) {
			l=theTV->biRawDef.bmiHeader.biWidth*theTV->biRawDef.bmiHeader.biHeight*3;
			CFile mF;
			S=myDlg.GetFileName();
			if(S.Find("jpg") != -1) {
				CJpeg myJPEG;
				BYTE *p;
				if(b.CreateBitmap(theTV->biRawDef.bmiHeader.biWidth,theTV->biRawDef.bmiHeader.biHeight,1,24,NULL)) {
					if(b.SetBitmapBits(l,theTV->theFrame)) {
						if(p=myJPEG.buildJPEG(&b,&len,TRUE)) {
							if(mF.Open(myDlg.GetPathName(),CFile::modeCreate | CFile::modeReadWrite | CFile::typeBinary | CFile::shareDenyWrite)) {
								mF.Write(p,len);
								mF.Close();
								}
							GlobalFree(p);
							return;
							}
						}
					}
				}
			else if(S.Find("bmp") != -1) {
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
		GlobalFree(theTV->theFrame);
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
							GlobalFree(p);
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
			GlobalFree(theTV->theFrame);
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
				GlobalFree(theTV->theFrame);
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
		GlobalFree(lpdm);
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

void CVidsendDoc2::OnUpdateVideoTrasmissionePaginadiprova(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 2);
	}

void CVidsendDoc2::OnUpdateVideoTrasmissioneFilmato(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 1);
	}

void CVidsendDoc2::OnUpdateVideoTrasmissioneDalvivo(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(trasmMode == 0);
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
		S1.Format("Frame inviati: %u, STOP ricevuti:%u, frame NON inviati",n,n1,n2);
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
	pCmdUI->Enable(Opzioni & maySendAudio);
	}

void CVidsendDoc2::OnVideoLivellivolume() {
	WinExec("sndvol32.exe",SW_SHOWNORMAL);
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
	if(PBsi)
		GlobalFree(PBsi);
	PBsi=NULL;
	if(PBbiSrc)
		GlobalFree(PBbiSrc);
	PBbiSrc=NULL;
	if(PBbiDest)
		GlobalFree(PBbiDest);
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
			hr=AVIFileOpen(&aviFile,(LPCTSTR)nomeAVI_PB,OF_READ,NULL);    // use handler determined from file extension....
			if(hr != AVIERR_OK)
				goto error1;
			hr = AVIFileGetStream(aviFile, &psVideo, streamtypeVIDEO, 0); 
			if(hr != AVIERR_OK)
				goto error1;
			PBsi=(AVISTREAMINFO *)GlobalAlloc(GPTR,sizeof(AVISTREAMINFO));
			AVIStreamInfo(psVideo,PBsi,sizeof(AVISTREAMINFO));
			PBbiSrc=(BITMAPINFOHEADER *)GlobalAlloc(GPTR,sizeof(BITMAPINFOHEADER));
			PBbiSrc->biSize=sizeof(BITMAPINFOHEADER);
			PBbiSrc->biWidth=PBsi->rcFrame.right-PBsi->rcFrame.left;
			PBbiSrc->biHeight=PBsi->rcFrame.bottom-PBsi->rcFrame.top;
			PBbiSrc->biCompression=PBsi->fccHandler;
			PBbiSrc->biPlanes=1;
			PBbiSrc->biBitCount=24;		// sicuro??
			PBbiDest=(BITMAPINFOHEADER *)GlobalAlloc(GPTR,sizeof(BITMAPINFOHEADER));
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
	si->versione=VIDSEND_VERSIONE;
	if(Opzioni & maySendVideo && theTV) {
		if(trasmMode==0 || trasmMode==2)
			si->bm=theTV->biCompDef.bmiHeader;
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

	return si;
	}


int CVidsendDoc2::calcBandWidth() {		// quella in uso al momento...
	RECT rc;

	SetRectEmpty(&rc);
	rc.bottom=theTV->biRawBitmap.biHeight;
	rc.right=theTV->biRawBitmap.biWidth;
	return calcBandWidth(Opzioni & CVidsendDoc2::maySendVideo,
		&rc,theTV->biRawBitmap.biBitCount,
		myQV.compressor,myQV.quality,myQV.fps,
		Opzioni & CVidsendDoc2::maySendAudio,theTV->wfd.wf.nSamplesPerSec,
		theTV->wfd.wf.wBitsPerSample,theTV->wfd.wf.nChannels,myQA.compressor,myQA.quality);

	}

int CVidsendDoc2::calcBandWidth(int vq, int aq) {
	int i,n;

	if(vq >= 7 || aq >= 3)
		return -1;
	n=calcBandWidth(vq>=0,&qsv[vq].imageSize,qsv[vq].bpp,myQV.compressor,qsv[vq].quality,qsv[vq].fps,
		aq>=0,qsa[aq].samplesPerSec,qsa[aq].bitsPerSample,qsa[aq].channels,myQA.compressor,qsa[aq].quality);
	return n;
	}

int CVidsendDoc2::calcBandWidth(BOOL bVideo,RECT *imageSize,int imageFormat,DWORD compressorV,int qualityV,int fps,
																BOOL bAudio,DWORD samplesPerSec,WORD bitsPerSample,WORD channels,DWORD compressorA,int qualityA,
																CString *info) {
	int i;
	DWORD nv=0,na=0,t,t1;

	if(bVideo) {
		if(compressorV) {
			if(compressorV != -1) {
				HIC hICCo;
				BITMAPINFOHEADER bi,bo;
				RECT r;
				CBitmap *b;
				BITMAP bmp;
				DWORD bSize;
				BYTE *s,*d;
				DWORD l1,l2;
				r=*imageSize;
				b=theApp.createTestBitmap(&r,(LPBITMAPINFOHEADER)&theTV->biRawDef, 0 /*4*/);
				b->GetBitmap(&bmp);
				bSize=bmp.bmWidthBytes*bmp.bmHeight;
				s=(BYTE *)GlobalAlloc(GPTR,bSize);
				b->GetBitmapBits(bSize,s);
				bi.biSize=sizeof(BITMAPINFOHEADER);
				bi.biWidth=imageSize->right;
				bi.biHeight=imageSize->bottom;
				bi.biBitCount=imageFormat;
				bi.biPlanes=1;
				bi.biClrImportant=bi.biClrUsed=0;
				bi.biSizeImage=0;
				bi.biCompression=0;
				bo=bi;
				bo.biCompression=compressorV;
				hICCo=ICOpen(ICTYPE_VIDEO,compressorV,ICMODE_FASTCOMPRESS);
				nv=-1;
				if(hICCo) {
					if(!(i=ICCompressBegin(hICCo,&bi,&bo))) {
						t=ICCompressGetSize(hICCo,&bi,&bo);
						d=(BYTE *)GlobalAlloc(GPTR,t+100);
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
				GlobalFree(d);	
				GlobalFree(s);
				delete b;
				}
			else {		// finire!
				nv=0;
				}
			}
		else {
			t1=t=imageSize->right*imageSize->bottom*imageFormat/8;
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
			wf.wBitsPerSample = bitsPerSample ;
			wf.nAvgBytesPerSec = wf.nSamplesPerSec*wf.nChannels*(wf.wBitsPerSample/8);
			wf.cbSize = 0;
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
		for(i=0; i<7; i++) {
			if(qv->imageSize.right == qsv[i].imageSize.right) {
				if(qv->fps <= qsv[i].fps) {
//					if(qv->qualita <= qsv[i].qualita) {
					break;
					}
				}
			if(qv->imageSize.right < qsv[i].imageSize.right) {
				while(i>0 && (qv->imageSize.right < qsv[i].imageSize.right)) 
					i--;
				break;
				}
			}
		}
	if(qa) {
		for(j=0; j<3; j++) {
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

int CVidsendDoc2::MandaPipeV(struct AV_PACKET_HDR *avh,LPARAM lParam) {
	int i;
	DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0; 
  BOOL fSuccess = FALSE;

//https://docs.microsoft.com/en-us/windows/desktop/ipc/multithreaded-pipe-server
	if(pipeV) {
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
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		char myBuf[64];
		RECT rc;
		CVidsendView3 *w=(CVidsendView3 *)getView();

		SetRectEmpty(&rc);
		GetPrivateProfileString(IDS_COORDINATE,myBuf,32);
		sscanf(myBuf,"%d,%d,%d,%d",&rc.left,&rc.top,&rc.right,&rc.bottom);	// sono coord. client rispetto alla MDIFRAME madre della mia ChildFrame
		if(!IsRectEmpty(&rc))
			w->GetParent()->SetWindowPos(NULL,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);
		}

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
	opzioniVisive=GetPrivateProfileInt(IDS_OPZIONI2);
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
  WritePrivateProfileInt(IDS_OPZIONI2,opzioniVisive);
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
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		char myBuf[64];
		RECT rc;
		CVidsendView5 *w=(CVidsendView5 *)getView();

		SetRectEmpty(&rc);
		GetPrivateProfileString(IDS_COORDINATE,myBuf,32);
		sscanf(myBuf,"%d,%d,%d,%d",&rc.left,&rc.top,&rc.right,&rc.bottom);	// sono coord. client rispetto alla MDIFRAME madre della mia ChildFrame
		if(!IsRectEmpty(&rc))
			w->GetParent()->SetWindowPos(NULL,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);
		}

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

void CVidsendDoc6::Serialize(CArchive& ar)
{
	if(ar.IsStoring())
	{
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
			theApp.maxHTMLconn=myPage0.m_MaxHTMLconn;
			if(theApp.theServer) {
				theApp.theServer->maxConn=myPage0.m_Maxconn;
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
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		char myBuf[64];
		RECT rc;
		CVidsendView7 *w=(CVidsendView7 *)getView();

		SetRectEmpty(&rc);
		GetPrivateProfileString(IDS_COORDINATE,myBuf,32);
		sscanf(myBuf,"%d,%d,%d,%d",&rc.left,&rc.top,&rc.right,&rc.bottom);	// sono coord. client rispetto alla MDIFRAME madre della mia ChildFrame
		if(!IsRectEmpty(&rc))
			w->GetParent()->SetWindowPos(NULL,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);
		}

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


