// vidsend2View.cpp : implementation of the CVidsendView class
//

#include "stdafx.h"
#include "vidsend.h"

#include "vidsendSet.h"
#include "vidsendDoc.h"
#include "vidsendlog.h"
#include "vidsendView.h"
#include "vidsendDialog.h"
#include "digitaltext.h"
#include "childfrm.h"
#include "msacm.h"
#include <cjpeg2.h>
//#include "tx4ole.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CVidsendView - client streaming video

IMPLEMENT_DYNCREATE(CVidsendView, CView)

BEGIN_MESSAGE_MAP(CVidsendView, CView)
	//{{AFX_MSG_MAP(CVidsendView)
	ON_COMMAND(ID_CONNESSIONE_CONNETTI, OnConnessioneConnetti)
	ON_COMMAND(ID_CONNESSIONE_DISCONNETTI, OnConnessioneDisconnetti)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_CONNETTI, OnUpdateConnessioneConnetti)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_DISCONNETTI, OnUpdateConnessioneDisconnetti)
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_FILE_SAVE_FOTOGRAMMA, OnFileSaveFotogramma)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_FOTOGRAMMA, OnUpdateFileSaveFotogramma)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateFileSave)
	ON_COMMAND(ID_CONNESSIONE_CARATTERISTICHECONNESSIONE, OnConnessioneCaratteristicheconnessione)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_CARATTERISTICHECONNESSIONE, OnUpdateConnessioneCaratteristicheconnessione)
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_CONNESSIONE_RICONNETTI, OnConnessioneRiconnetti)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_RICONNETTI, OnUpdateConnessioneRiconnetti)
	ON_UPDATE_COMMAND_UI(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_SORGENTE1, OnUpdateControlloControlloremototelecamereSorgente1)
	ON_UPDATE_COMMAND_UI(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_SORGENTE2, OnUpdateControlloControlloremototelecamereSorgente2)
	ON_UPDATE_COMMAND_UI(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_SORGENTE3, OnUpdateControlloControlloremototelecamereSorgente3)
	ON_UPDATE_COMMAND_UI(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_ALTERNASORGENTI, OnUpdateControlloControlloremototelecamereAlternasorgenti)
	ON_COMMAND(ID_VISUALIZZA_VOLUME, OnVisualizzaVolume)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendView construction/destruction

CVidsendView::CVidsendView() {
	//{{AFX_DATA_INIT(CVidsendView)
	//}}AFX_DATA_INIT
	int i;

	cliSockCtrl=new CControlCliSocket(this);
	cliSockV=NULL;
	cliSockA=NULL;
	hICDe=NULL;
	hDD=NULL;
	hWaveOut=NULL;
	hAcm=NULL;

	theFrame=NULL;
	waitForKeyFrame=1;
	renderedFrameTsp=0;

	}

CVidsendView::~CVidsendView() {

//	stopAV();
	if(cliSockCtrl)
		delete cliSockCtrl;
	cliSockCtrl=NULL;
	framesPerSec=0;
	theTimer=0;
	}


BOOL CVidsendView::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) {
	int i;
	
	if(i=CView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext)) {

//		OnConnessioneConnetti();
		PostMessage(WM_COMMAND,ID_CONNESSIONE_CONNETTI,0);
		}
	return i;
	}

BOOL CVidsendView::PreCreateWindow(CREATESTRUCT& cs) {

	return CView::PreCreateWindow(cs);
	}

void CVidsendView::OnDestroy() {
	CVidsendDoc *d=GetDocument();
	RECT rc;
	char myBuf[64];
	
	CView::OnDestroy();
	
	stopAV();
	hWaveOut=NULL;
	if(d->authSocket) {
		d->authSocket->Close();
		delete d->authSocket;
		}
	d->authSocket=NULL;
	if(cliSockCtrl)
		cliSockCtrl->Close();
	if(cliSockA) {
//		cliSockA->stop();
//		cliSockA->ShutDown();		// a volte riceve dopo che la finestra e' stata chiusa... proviamo cosi'!
		cliSockA->Close();
		}
	if(cliSockV) {
//		cliSockV->stop();
//		cliSockV->ShutDown();
		cliSockV->Close();
		}
	delete cliSockA;
	cliSockA=NULL;
	delete cliSockV;
	cliSockV=NULL;
	if(theTimer)
		KillTimer(theTimer);
	if(hICDe)
		ICClose(hICDe);
	hICDe=NULL;
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		GetParent()->GetWindowRect(&rc);	// coord. schermo della mia MDIChildFrmae...
		GetParent()->GetParent()->ScreenToClient(&rc);	// ... diventano coord. client rispetto al MDI padre (che NON e' theApp.m_pMainWnd !! ce n'è una un mezzo...
		wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
		d->WritePrivateProfileString(IDS_COORDINATE,myBuf);
		}
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendView diagnostics

void CVidsendView::initAV(BITMAPINFOHEADER *bmi,DWORD fps,EXT_WAVEFORMATEX *wf) {
	int i;

	if(bmi) {
		waitForKeyFrame=1;
		biCompDef=*bmi;
		biRawDef=biCompDef;

		// NON DOVEVA SERVIRE! solo nel server... VERIFICARE!
		biRawDef.biCompression=0; biRawDef.biBitCount=24;

		hICDe=ICDecompressOpen(ICTYPE_VIDEO,biCompDef.biCompression,&biCompDef,NULL);
			/*CString info;
			ICINFO icinfo;
			ICGetInfo(hICDe,&icinfo,sizeof(ICINFO));
			info+=icinfo.dwFlags & VIDCF_CRUNCH ? "supporta compressione a una dimensione data; " : "";
			info+=icinfo.dwFlags & VIDCF_DRAW ? "supporta drawing; " : "";
			info+=icinfo.dwFlags & VIDCF_FASTTEMPORALC ? "supporta compressione temporale e conserva copia dei dati; " : "";
			info+=icinfo.dwFlags & VIDCF_FASTTEMPORALD ? "supporta decompressione temporale e conserva copia dei dati; " : "";
			info+=icinfo.dwFlags & VIDCF_QUALITY ? "supporta impostazione qualità; " : "";
			info+=icinfo.dwFlags & VIDCF_TEMPORAL ? "supporta compressione inter-frame; " : "";
			AfxMessageBox(info);*/
		if(!hICDe)
ICerror:
			AfxMessageBox("Impossibile inizializzare decompressore");
		else
			if(ICDecompressBegin(hICDe,&biCompDef,&biRawDef) != ICERR_OK)
				goto ICerror;
		hDD=DrawDibOpen();
		if(!hDD)
HDDerror:
			AfxMessageBox("Impossibile STARTare DrawDIB");
		else
			if(!DrawDibStart(hDD,1000000/fps))
				goto HDDerror;
		setTimer(1000/streamInfo.fps-10 /*piccola correzione... sembra durare sempre 20mSec in + !*/);
		}

	if(wf) {
		wfn.wFormatTag = WAVE_FORMAT_PCM;
		wfn.nChannels = 1;
		wfn.nSamplesPerSec = 8000;
		wfn.nBlockAlign = 1;
		wfn.wBitsPerSample = 8;
		wfn.nAvgBytesPerSec = wfn.nSamplesPerSec*wfn.nChannels*(wfn.wBitsPerSample/8);
		wfn.cbSize = 0;
		hWaveOut=NULL;
		i=waveOutOpen(&hWaveOut,WAVE_MAPPER,&wfn,(DWORD)waveOutCallback,(DWORD)this,CALLBACK_FUNCTION);
		if(i != MMSYSERR_NOERROR)
			AfxMessageBox("Impossibile aprire periferica audio");
	
		wfs=*wf;
		acmStreamOpen(&hAcm,NULL,(WAVEFORMATEX *)&wfs,&wfn,NULL,NULL,0 /*this*/,0);
		if(hAcm)
			acmStreamSize(hAcm,wfs.wf.nAvgBytesPerSec,&maxWaveoutSize,ACM_STREAMSIZEF_SOURCE);
		else
			AfxMessageBox("Impossibile aprire convertitore stream audio");
		if(hWaveOut)
			waveOutPause(hWaveOut);
		}
	((CChildFrame *)GetParent())->setStatusIcons();
//	((CChildFrame *)GetParent())->m_wndStatusBar.GetStatusBarCtrl().SetIcon(3,wf ? ((CChildFrame *)GetParent())->iconSpk : ((CChildFrame *)GetParent())->iconSpkOff);

	}

void CVidsendView::stopAV() {

	if(hWaveOut) {
		HWAVEOUT thw=hWaveOut;
		hWaveOut=NULL;
		waveOutPause(thw);
//		waveOutReset(thw);		// su nmvidsend va, qua si incricca...
		waveOutClose(thw);
		((CChildFrame *)GetParent())->m_VUMeter->SetWindowText(0);
//		hWaveOut=NULL; NO! è usato da callback (anche se non dovrebbe)
		}
	if(hAcm)
		acmStreamClose(hAcm,0);
	hAcm=NULL;
	if(hDD) {
		DrawDibStop(hDD);
		DrawDibClose(hDD);
		}
	hDD=NULL;
	if(hICDe)
		ICClose(hICDe);
	hICDe=NULL;

	}

void CVidsendView::OnDraw(CDC* pDC) {
	RECT r;

	GetClientRect(&r);
	theApp.renderBitmap(pDC,IDB_MONOSCOPIO,&r);
	}

void CVidsendView::drawFrame(struct AV_PACKET_HDR *avh) {
	CVidsendDoc *d=(CVidsendDoc *)GetDocument();
	int i;
//	BITMAPINFO *bmi;
	BITMAPINFOHEADER *bi;
	RECT r;
	BYTE *s;
	CDC *dc;

	if(d && !d->bPaused) {
		GetClientRect(&r);

		s=(BYTE *)avh->lpData;
		bi=(BITMAPINFOHEADER *)s;
//		bi=&(bmi->bmiHeader);
		if(theApp.debugMode) {
			if(theApp.FileSpool) {
				char myBuf[128];
				wsprintf(myBuf,"  drawFrame: keyframe %u ",avh->info);
				*theApp.FileSpool << myBuf;
				}
			}
		if(!waitForKeyFrame || (avh->info & AVIIF_KEYFRAME)) {
			waitForKeyFrame=0;
			if(theFrame)
				GlobalFree(theFrame);
			renderedFrameTsp=avh->timestamp;
			theFrame=(BYTE *)GlobalAlloc(GPTR,biCompDef.biWidth*biCompDef.biHeight*3 +100);
			i=ICDecompress(hICDe, avh->info & AVIIF_KEYFRAME ? 0 : ICDECOMPRESS_NOTKEYFRAME,
				bi,(s+sizeof(BITMAPINFOHEADER)),&biRawDef,theFrame);
			if(i==ICERR_OK) {
				dc=GetDC();
//				adjustBitmap(theFrame,d->contrasto,d->luminosita,d->saturazione);
				if(d->Opzioni & CVidsendDoc::fmt4_3) {
					r.bottom=(r.right*3)/4;
					}
				if(d->Opzioni & CVidsendDoc::fmt_bn) {
					CTV::convertColorBitmapToBN(bi,theFrame);
					}

				if(d->Opzioni & CVidsendDoc::fmt_double) {
//					r.right *= 2;		//gestisco in connectionInfo! meglio
//					r.bottom *= 2;
					}
				if(d->Opzioni & CVidsendDoc::fmt_full) {
					i=DrawDibDraw(hDD,::GetWindowDC(0),0,0,r.right,r.bottom,&biRawDef,theFrame,0,0,
						biRawDef.biWidth,biRawDef.biHeight, DDF_SAME_HDC /*| avh->info ? 0 : DDF_NOTKEYFRAME*/);
					}
				else {
					i=DrawDibDraw(hDD,dc->m_hDC,0,0,r.right,r.bottom,&biRawDef,theFrame,0,0,
						biRawDef.biWidth,biRawDef.biHeight, DDF_SAME_HDC /*| avh->info ? 0 : DDF_NOTKEYFRAME*/);
					}
				ReleaseDC(dc);
				}
			}
		}
//	GlobalFree(s);
	}

void CVidsendView::playSample(struct AV_PACKET_HDR *avh) {
	int i;
	BYTE *s,*smp;
	WAVEFORMATEX *wf;
	ACMSTREAMHEADER hhacm;
	DWORD l;
	WAVEHDR *wh;

	if(GetDocument() && !GetDocument()->bPaused) {

		i=syncToVideo(avh);
		if(i) {
			s=(BYTE *)avh->lpData;
	//		wf=(WAVEFORMATEX *)s;
			smp=(BYTE *)GlobalAlloc(GMEM_FIXED,maxWaveoutSize);
			hhacm.cbStruct=sizeof(ACMSTREAMHEADER);
			hhacm.fdwStatus=0;
			hhacm.dwUser=(DWORD)0 /*this*/;
			hhacm.pbSrc=s;
			hhacm.cbSrcLength=avh->len;
	//		hhacm.cbSrcLengthUsed=l;
			hhacm.dwSrcUser=0;
			hhacm.pbDst=smp;
			hhacm.cbDstLength=maxWaveoutSize;
	//		hhacm.cbDstLengthUsed=0;
			hhacm.dwDstUser=0;
			if(!acmStreamPrepareHeader(hAcm,&hhacm,0)) {
				i=acmStreamConvert(hAcm,&hhacm,ACM_STREAMCONVERTF_BLOCKALIGN);
		//	wsprintf(myBuf,"convertito: %d, %d bytes",i,l);
		//	AfxMessageBox(myBuf);
				acmStreamUnprepareHeader(hAcm,&hhacm,0);

				if(hWaveOut) {
					wh=new WAVEHDR;
					// l'ULTIMO wh al Close pare rimanere allocato... 12.18
					wh->lpData=(char *)smp;
					wh->dwBufferLength=hhacm.cbDstLengthUsed;
					wh->dwFlags=0;
					wh->dwLoops=0;
					waveOutPrepareHeader(hWaveOut,wh,sizeof(WAVEHDR));
					waveOutWrite(hWaveOut,wh,sizeof(WAVEHDR));
					((CChildFrame *)GetParent())->m_VUMeter->SetWindowText((BYTE *)wh->lpData,wh->dwBufferLength);
					}
				else
					GlobalFree(smp);
				}
			else
				GlobalFree(smp);
			}

		}
	}

BOOL CVidsendView::syncToVideo(struct AV_PACKET_HDR *avh) {
// migliorare... forse e' meglio fare sync to audio (tanti frame su uno solo...)
	if(avh->timestamp < renderedFrameTsp)		
		return 0;
	else
		return 1;
	}

int CVidsendView::setTimer(int t) {

	if(t != framesPerSec) {
		if(theTimer)
			KillTimer(theTimer);
		theTimer=SetTimer((UINT)m_hWnd,t,NULL);
		if(theTimer)
			framesPerSec=t;
		else
			framesPerSec=0;
		}
	return theTimer;
	}

void CALLBACK CVidsendView::waveOutCallback(HWAVEOUT hwo,UINT msg,DWORD dwUser,DWORD dwParam1,DWORD dwParam2) {
	CVidsendView *myPtr=(CVidsendView *)dwUser;

	switch(msg) {
		case WOM_CLOSE:
			break;
		case WOM_DONE:
			myPtr->cliSockA->totBuffers--;
			if(myPtr->cliSockA->totBuffers < myPtr->cliSockA->getLowBuffers()) {
				waveOutPause(hwo);
				}
			waveOutUnprepareHeader(hwo,(LPWAVEHDR)dwParam1,sizeof(WAVEHDR));
			GlobalFree(((WAVEHDR *)dwParam1)->lpData);
			delete ((WAVEHDR *)dwParam1);
			break;
		}
	}


#ifdef _DEBUG
void CVidsendView::AssertValid() const {
	CView::AssertValid();
	}

void CVidsendView::Dump(CDumpContext& dc) const {
	
	CView::Dump(dc);
	}

CVidsendDoc *CVidsendView::GetDocument() { // non-debug version is inline

	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVidsendDoc)));
	return (CVidsendDoc *)m_pDocument;
	}

#endif //_DEBUG



/////////////////////////////////////////////////////////////////////////////
// CVidsendView message handlers


void CVidsendView::OnConnessioneConnetti() {
	CVidsendDoc *d=GetDocument();
	CDlgEnterURL myDlg(0,d);
	CString prevURL[CVidsendDoc::MRU_SIZE];
	int i;

	d->loadCliConnRecent(prevURL);
	
	if(theApp.Opzioni & CVidsendApp::openClientVideo) {
		d->srvAddress=prevURL[0];
		if(!d->srvAddress.IsEmpty()) {
			goto connetti;
			}
		}
	if(myDlg.DoModal(prevURL,CVidsendDoc::MRU_SIZE) == IDOK) {
		GetDocument()->srvAddress=myDlg.m_URLstring;
connetti:
		OnConnessioneRiconnetti();
		}
	}

int CVidsendView::doConnect(char *myAddress,struct STREAM_INFO *si) {
	CVidsendDoc *d=GetDocument();
	int i=0;
	DWORD l;

	streamInfo=*si;
	if(*si->authenticationWWW) {
		CPasswordDlg myDlg;
rifo:
		d->authSocket=new CAuthCliSocket(d);
		if(!d->authSocket)
			goto fine;
		myDlg.m_Nome=theApp.infoUtente.login;
#ifndef _CAMPARTY_MODE
		myDlg.m_Pasw=theApp.infoUtente.pasw;
#endif
		if(myDlg.DoModal() == IDOK) {
			char myBuf[128];
			MSG msg;

			if(!d->authSocket->Create())
				goto auth_not_avail;
			if(!d->authSocket->Connect(si->authenticationWWW,AUTHENTICATION_SOCKET))
				goto auth_not_avail;

			OSVERSIONINFO osvi;
			DWORD mySerNum;
			osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
			GetVersionEx(&osvi);
			GetVolumeInformation("C:\\",NULL,0,&mySerNum,NULL,NULL,NULL,0);
			d->authSocket->SendUserPass((char *)(LPCTSTR)myDlg.m_Nome,(char *)(LPCTSTR)myDlg.m_Pasw,0,si->IDServer,
				theApp.getVersione(),MAKELONG(osvi.dwMinorVersion,osvi.dwMajorVersion),mySerNum);
			l=timeGetTime()+30000;			// 30sec timeout
			while(!d->authSocket->response && timeGetTime() < l) {
				if(::PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
					if(!theApp.PumpMessage()) { 
						}
					}
				}
			switch(d->authSocket->response) {
				case 0:
auth_not_avail:
#ifdef _LINGUA_INGLESE
					AfxMessageBox("Authentication not available");
#else
					AfxMessageBox("Autenticazione non disponibile");
#endif
					goto fine;
					break;
				case -4:
#ifdef _LINGUA_INGLESE
					AfxMessageBox("User is already connected!\nExit and try again.");
#else
					AfxMessageBox("Utente già connesso!\nUscire e ritentare l'operazione.");
#endif
					goto fine;
					break;
				case -3:
#ifdef _LINGUA_INGLESE
					AfxMessageBox("User is disabled!");
#else
					AfxMessageBox("Utente disabilitato!");
#endif
					goto fine;
					break;
				case -2:
#ifdef _LINGUA_INGLESE
					if(MessageBox("Wrong password! Retry?",NULL,MB_ICONQUESTION | MB_YESNO) == IDYES) {
#else
					if(MessageBox("Password errata! Ritentare?",NULL,MB_ICONQUESTION | MB_YESNO) == IDYES) {
#endif
						d->authSocket->Close();
						delete d->authSocket;
						goto rifo;
						}
					else
						goto fine;
					break;
				case -1:
#ifdef _LINGUA_INGLESE
					if(MessageBox("User not known! Retry?",NULL,MB_ICONQUESTION | MB_YESNO) == IDYES) {
#else
					if(MessageBox("Utente non riconosciuto! Ritentare?",NULL,MB_ICONQUESTION | MB_YESNO) == IDYES) {
#endif
						d->authSocket->Close();
						delete d->authSocket;
						goto rifo;
						}
					else
						goto fine;
					break;
				case 1:
					_tcscpy(theApp.infoUtente.login,(LPCTSTR)myDlg.m_Nome);
					_tcscpy(theApp.infoUtente.pasw,(LPCTSTR)myDlg.m_Pasw);

#ifndef _NEWMEET_MODE
					AfxMessageBox("Login OK!");
#endif
					break;
				default:		// ERRORE!
					goto fine;
					break;
				}

			}
		else {
			goto fine;
			}
		cliSockCtrl->SendUserPass((char *)(LPCTSTR)myDlg.m_Nome,(char *)(LPCTSTR)myDlg.m_Pasw,d->authSocket->extraParm,d->authSocket->timedConn);
		// ...faccio che mandare il nome del client anche se non c'e' autenticazione!
		}


	if(si->bm.biSize) {		// il server trasmette video...
		if(!(cliSockV=new CStreamVCliSocket(this,NULL,si->noBuffers ? 0 : GetDocument()->numBuffers)))
			goto fine;
		if(!cliSockV->Create(0,theApp.Opzioni & CVidsendApp::TCP_UDP ? SOCK_DGRAM : SOCK_STREAM))
			goto fine;
		if(!cliSockV->Connect(myAddress,VIDEO_SOCKET))
			goto fine;
//		cliSockV->initBuffers(GetDocument()->numBuffers);
		GetDocument()->standardSize();
		i=1;
		}

	if(si->wf.wf.wFormatTag) {		// il server trasmette audio...
		if(!(cliSockA=new CStreamACliSocket(this,&hWaveOut,si->noBuffers ? 0 : GetDocument()->numBuffers)))
			goto fine;
		if(!cliSockA->Create(0,theApp.Opzioni & CVidsendApp::TCP_UDP ? SOCK_DGRAM : SOCK_STREAM))
			goto fine;
		if(!cliSockA->Connect(myAddress,AUDIO_SOCKET))
			goto fine;
//		cliSockA->initBuffers(GetDocument()->numBuffers/4);
		i=1;		// mettere a posto meglio...
		}
	if(si->maxTime == 0 && (d->authSocket && d->authSocket->timedConn > 0)) {
		si->maxTime=d->authSocket->timedConn;
		}
	if(si->maxTime > 0) {
		CTimedMessageBox myDlg;
		CString S;
		S.Format("Durata massima della connessione: %umin. %usec.",si->maxTime.GetTotalMinutes(),si->maxTime.GetSeconds());
		myDlg.DoModal("Connessione",S,MB_OK | MB_ICONINFORMATION);
		}
	d->streamTitle=si->streamTitle;
	// finire...
	initAV(si->bm.biSize ? &si->bm : NULL,si->fps,si->wf.wf.wFormatTag ? &si->wf : NULL);
	if(*si->splashOrIntro) {
// finire...
		}
	if(*si->openWWW) {
		theApp.nuovoBrowser(si->openWWW);
		}

fine:
	return i;
	}

void CVidsendView::OnConnessioneDisconnetti() {

	endConnect();
	}

int CVidsendView::endConnect() {
	CVidsendDoc *d=GetDocument();


	((CChildFrame *)GetParent())->m_wndStatusBar.GetStatusBarCtrl().SetIcon(2,((CChildFrame *)GetParent())->iconOff);
	((CChildFrame *)GetParent())->m_wndStatusBar.GetStatusBarCtrl().SetIcon(3,NULL);
#ifdef _LINGUA_INGLESE
	((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,"Not connected",TRUE);
#else
	((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,"Non connesso",TRUE);
#endif
	((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(0,"",TRUE);
	stopAV();
	if(d->authSocket) {
		d->authSocket->Close();
		delete d->authSocket;
		}
	d->authSocket=NULL;
	if(cliSockA) {
		cliSockA->Close();
		delete cliSockA;
		}
	cliSockA=NULL;
	if(cliSockV) {
		cliSockV->Close();
		delete cliSockV;
		}
	cliSockV=NULL;
	if(cliSockCtrl)
		cliSockCtrl->Close();
	InvalidateRect(NULL);
	return 1;
	}


void CVidsendView::OnConnessioneRiconnetti() {
	char myBuf[128];
	int i=0;
  RASDIALPARAMS rdParams;
  char szBuf[256],*p;
	DWORD dwRet;

	cliSockCtrl->Create();
rifo:
#ifdef _LINGUA_INGLESE
	wsprintf(myBuf,"Connecting to %s",(LPCTSTR)GetDocument()->srvAddress);
#else
	wsprintf(myBuf,"Connessione in corso a %s",(LPCTSTR)GetDocument()->srvAddress);
#endif
	((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,myBuf,TRUE);

		
	if(cliSockCtrl->Connect(GetDocument()->srvAddress,CONTROL_SOCKET)) {

//			cliSockV->Receive(&streamInfo,sizeof(STREAM_INFO));
		((CChildFrame *)GetParent())->m_wndStatusBar.GetStatusBarCtrl().SetIcon(2,((CChildFrame *)GetParent())->iconOn);

#ifdef _LINGUA_INGLESE
		wsprintf(myBuf,"Connected to %s",(LPCTSTR)GetDocument()->srvAddress);
#else
		wsprintf(myBuf,"Connesso al server %s",(LPCTSTR)GetDocument()->srvAddress);
#endif
		((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,myBuf,TRUE);
		if(!GetDocument()->streamTitle.IsEmpty())
			((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,GetDocument()->streamTitle,TRUE);
		else		// se vuota, al limite sbianchetta quella prec.
			((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,GetDocument()->streamTitle,TRUE);
		}
	else {
		if(!i && GetDocument()->Opzioni & CVidsendDoc::autoRASconnect) {
			if(theApp.hRasConn)
				goto fine;
			rdParams.dwSize = sizeof(RASDIALPARAMS);
			_tcscpy(rdParams.szEntryName,theApp.RASEntry);
			rdParams.szPhoneNumber[0]=0;
			rdParams.szCallbackNumber[0]='\0';
			*rdParams.szUserName=0;	//_tcscpy(rdParams.szUserName, theApp.RASUsername);
			*rdParams.szPassword=0;	//_tcscpy(rdParams.szPassword, theApp.RASPassword);
			rdParams.szDomain[0] = '\0';
			dwRet=RasDial(NULL,NULL,&rdParams,0L,NULL,&theApp.hRasConn);
			if(dwRet) {
				if(RasGetErrorString((UINT)dwRet,(LPSTR)myBuf,120) != 0)
					wsprintf( (LPSTR)myBuf,"Undefined Dial Error (%ld).",dwRet);
				AfxMessageBox(myBuf);
				if(theApp.FileSpool)
					*theApp.FileSpool << myBuf;
				goto fine;
				}
			i++;
			goto rifo;
			}
fine:
		((CChildFrame *)GetParent())->m_wndStatusBar.GetStatusBarCtrl().SetIcon(2,((CChildFrame *)GetParent())->iconOff);
		((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,"Non connesso",TRUE);
		cliSockCtrl->Close();
#ifdef _LINGUA_INGLESE
		AfxMessageBox("Unable to connect to server");
#else
		AfxMessageBox("Impossibile connettersi al server");
#endif
		}

	}


void CVidsendView::OnUpdateConnessioneConnetti(CCmdUI* pCmdUI) {

	pCmdUI->Enable(cliSockCtrl->m_hSocket == INVALID_SOCKET);
	}

void CVidsendView::OnUpdateConnessioneDisconnetti(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(cliSockCtrl->m_hSocket != INVALID_SOCKET);
	}

void CVidsendView::OnUpdateConnessioneRiconnetti(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(cliSockCtrl->m_hSocket == INVALID_SOCKET && !GetDocument()->srvAddress.IsEmpty());
	}

void CVidsendView::OnConnessioneCaratteristicheconnessione() {
	CString S,S1;
	CVidsendDoc *d=GetDocument();

	if(cliSockV) {
		S1.Format("Ricezione video %ux%u pixel, %ubpp, %ufps, compressione 0x%X",	// mettere nome compressore!
			streamInfo.bm.biWidth,streamInfo.bm.biHeight,streamInfo.bm.biBitCount,streamInfo.fps,streamInfo.bm.biCompression);
		S=S1+"\n";
		}
	if(cliSockA) {
		S1.Format("Ricezione audio frequenza %u, %u bit, %u canale(i), compressione 0x%X",
			streamInfo.wf.wf.nSamplesPerSec,streamInfo.wf.wf.wBitsPerSample,streamInfo.wf.wf.nChannels,streamInfo.wf.wf.wFormatTag);
		S+=S1+"\n";
		}
	S1.Format("Bitrate: %uKbps",0);
	S+=S1;
	AfxMessageBox(S,MB_ICONINFORMATION);
	}

void CVidsendView::OnUpdateConnessioneCaratteristicheconnessione(CCmdUI* pCmdUI) {

	pCmdUI->Enable(cliSockCtrl->m_hSocket != INVALID_SOCKET);
	}


void CVidsendView::OnUpdateControlloControlloremototelecamereSorgente1(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(streamInfo.remoteCtrl & 1);
	}

void CVidsendView::OnUpdateControlloControlloremototelecamereSorgente2(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(streamInfo.remoteCtrl & 2);
	}

void CVidsendView::OnUpdateControlloControlloremototelecamereSorgente3(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(streamInfo.remoteCtrl & 3);
	}

void CVidsendView::OnUpdateControlloControlloremototelecamereAlternasorgenti(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(streamInfo.remoteCtrl & 128);
	}

void CVidsendView::OnEditCopy() {
	HANDLE h;
	BYTE *p;
	DWORD l;
	
	if(OpenClipboard()) {
		l=biRawDef.biWidth*biRawDef.biHeight*3;
 		h=GlobalAlloc(GMEM_MOVEABLE,l+sizeof(BITMAPINFOHEADER)+32);
		p=(BYTE *)GlobalLock(h);
		memcpy(p,&biRawDef,sizeof(BITMAPINFOHEADER));
		memcpy(p+sizeof(BITMAPINFOHEADER),theFrame,l);
		GlobalUnlock(h);
	  EmptyClipboard();
	  SetClipboardData(CF_DIB,h);
	  CloseClipboard();
	  }  
	
	}

void CVidsendView::OnUpdateEditCopy(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(cliSockCtrl->m_hSocket != INVALID_SOCKET && theFrame != NULL);
	}

void CVidsendView::OnFileSaveFotogramma() {
	DWORD l,len;
	CBitmap b;
	CFileDialog myDlg(FALSE,"*.jpg",NULL,OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,"File JPEG (*.jpg)|*.jpg|File Bitmap (*.bmp)|*.bmp|Tutti i file (*.*)|*.*||",this);
	CString S;
	
	if(myDlg.DoModal() == IDOK) {
		l=biRawDef.biWidth*biRawDef.biHeight*biRawDef.biBitCount/8;
		CFile mF;
		S=myDlg.GetFileName();
		if(S.Find("jpg") != -1) {
			CJpeg myJPEG;
			BYTE *p;
			if(b.CreateBitmap(biRawDef.biWidth,biRawDef.biHeight,1,biRawDef.biBitCount,NULL)) {
				if(b.SetBitmapBits(l,theFrame)) {
					if(p=myJPEG.buildJPEG(&b,&len,TRUE)) {
						if(mF.Open(myDlg.GetPathName(),CFile::modeCreate | CFile::modeReadWrite | CFile::typeBinary | CFile::shareDenyWrite)) {
							mF.Write(p,len);
							mF.Close();
							GlobalFree(p);
							return;
							}
						}
					}
				}
			}
		else if(S.Find("bmp") != -1) {
			BITMAPFILEHEADER bh;
			BITMAPINFOHEADER bi=biRawDef;
			if(mF.Open(myDlg.GetPathName(),CFile::modeCreate | CFile::modeReadWrite | CFile::typeBinary | CFile::shareDenyWrite)) {
				bh.bfType='MB';
				bh.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+l;
				bh.bfReserved1=bh.bfReserved2=0;
				bh.bfOffBits=sizeof(BITMAPINFOHEADER)+sizeof(BITMAPFILEHEADER);
				bi.biSizeImage=l;
				mF.Write(&bh,sizeof(BITMAPFILEHEADER));
				mF.Write(&bi,sizeof(BITMAPINFOHEADER));
				mF.Write(theFrame,l);
				mF.Close();
				return;
				}
			}
		else
#ifdef _LINGUA_INGLESE
			AfxMessageBox("Wrong file type");
#else
			AfxMessageBox("Tipo file non valido");
#endif

#ifdef _LINGUA_INGLESE
		AfxMessageBox("Unable to save image!");
#else
		AfxMessageBox("Impossibile salvare l'immagine!",MB_ICONSTOP);
#endif

		}

	}

void CVidsendView::OnUpdateFileSaveFotogramma(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(cliSockCtrl->m_hSocket != INVALID_SOCKET && theFrame != NULL);
	}

void CVidsendView::OnFileSave() {

	
	}

void CVidsendView::OnUpdateFileSave(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(cliSockCtrl->m_hSocket != INVALID_SOCKET && !streamInfo.dontSave);
	}


void CVidsendView::OnVisualizzaVolume() {

	WinExec("sndvol32.exe",SW_SHOWNORMAL);
	}

void CVidsendView::OnRButtonDown(UINT nFlags, CPoint point) {
	CMenu myMenu;
	
	myMenu.LoadMenu(IDR_VIDSENTYPE);
	CMenu *myMenu2=myMenu.GetSubMenu(2);
	myMenu2->AppendMenu(MF_SEPARATOR);
#ifdef _LINGUA_INGLESE
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Properties...");
#else
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Proprietà...");
#endif
	ClientToScreen(&point);
	myMenu2->TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, GetParent());
	
	CView::OnRButtonDown(nFlags, point);
	}

void CVidsendView::OnSize(UINT nType, int cx, int cy) {
	
	CView::OnSize(nType, cx, cy);
	if(hICDe)
		int i=ICDecompressBegin(hICDe,&biCompDef,&biRawDef);
	}

void CVidsendView::OnTimer(UINT nIDEvent) {
	struct AV_PACKET_HDR *av;
	char p[256];
	DWORD n;
	static int cnt;
		
	if(cliSockV) {
		av=cliSockV->getOutBuffer();
		if(theApp.debugMode) {
			if(theApp.FileSpool) {
//				theApp.FileSpool->print(CLogFile::flagInfo,"Buffers: LastOut=%u,ok2out=%u",cliSockV->lastOut,cliSockV->ok2Out);
				char myBuf[128];
				wsprintf(myBuf,"drawFrame: time %u av %x",timeGetTime(),av);
				*theApp.FileSpool << myBuf;
				}
			}
		if(av) {
			drawFrame(av);


				if(theApp.debugMode) {
					char *p1=(char *)GlobalAlloc(GPTR,256);
					wsprintf(p1,"frame %d, info %d",cnt++,av->info);
					if(theApp.m_pMainWnd)
						theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p1);
					}

	//		setTimer(av->psec);
			cliSockV->bumpOutBuffer();
			if(cliSockV->needMoreBuffers())
				cliSockV->Send("\x00",1);
			if((timeGetTime() - timeRef) > 2000) {
				timeRef=timeGetTime();
				n=cliSockV->getBytesReceived();
#ifdef _LINGUA_INGLESE
				wsprintf(p,"playback (@ %d.%d KB/s) (b:%d - %d)...",n/2000,(n % 2000)/200,/*cliSockV->getFirstIn(),cliSockV->getLastOut()*/ cliSockV->totAvailBuffers(),cliSockV->needMoreBuffers());
#else
				wsprintf(p,"riproduzione (@ %d.%d KB/s) (b:%d - %d)...",n/2000,(n % 2000)/200,/*cliSockV->getFirstIn(),cliSockV->getLastOut()*/ cliSockV->totAvailBuffers(),cliSockV->needMoreBuffers());
#endif
				if(((CChildFrame *)GetParent())->m_wndStatusBar.m_hWnd)
					((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,p,TRUE);
				}
			}
		else {
			if((timeGetTime() - timeRef) > 2000) {
				timeRef=timeGetTime();
				n=cliSockV->getBytesReceived();
#ifdef _LINGUA_INGLESE
				wsprintf(p,"buffering (%d di %d) (@ %d.%d KB/s)...",/*cliSockV->getFirstIn(),cliSockV->getLastOut()*/ cliSockV->totAvailBuffers(),cliSockV->getMaxBuffers(),n/2000, (n % 2000)/200);
#else
				wsprintf(p,"bufferizzazione (%d di %d) (@ %d.%d KB/s)...",/*cliSockV->getFirstIn(),cliSockV->getLastOut()*/ cliSockV->totAvailBuffers(),cliSockV->getMaxBuffers(),n/2000, (n % 2000)/200);
#endif
				if(((CChildFrame *)GetParent())->m_wndStatusBar.m_hWnd)
					((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,p,TRUE);
				}
			}
		}

/*	if(cliSockA) {
		av=cliSockA->getOutBuffer();
		if(av) {
			playSample(av);
	//		setTimer(av->psec);
			cliSockA->bumpOutBuffer();
			if(cliSockA->needMoreBuffers())
				cliSockA->Send("\x00",1);
			if((timeGetTime() - ti) > 2000) {
				ti=timeGetTime();
				n=cliSockA->getBytesReceived();
	//			wsprintf(p,"riproduzione (@ %d.%d KB/s) (b:%d - %d)...",n/2000,(n % 2000)/200,cliSockV->totAvailBuffers(),cliSockV->needMoreBuffers());
	//			if(((CChildFrame *)GetParent())->m_wndStatusBar.m_hWnd)
	//				((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(0,p,TRUE);
				}
			}
		else {
			if((timeGetTime() - ti) > 2000) {
				ti=timeGetTime();
				n=cliSockA->getBytesReceived();
	//			wsprintf(p,"bufferizzazione (%d di %d) (@ %d.%d KB/s)...",cliSockV->totAvailBuffers(),CStreamVCliSocket::maxStreamBuffers,n/2000, (n % 2000)/200);
	//			if(((CChildFrame *)GetParent())->m_wndStatusBar.m_hWnd)
	//				((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(0,p,TRUE);
				}
			}
		}*/
	
	CView::OnTimer(nIDEvent);
	}






/////////////////////////////////////////////////////////////////////////////
// CVidsendView2 - Finestra main video server

IMPLEMENT_DYNCREATE(CVidsendView2, CView)

CVidsendView2::CVidsendView2() {
	
	inClick=FALSE;
	myBrowserDlg=NULL;
	}

CVidsendView2::~CVidsendView2() {
	CVidsendDoc2 *d=GetDocument();

//	GetDocument()->theTV->Capture(0);		// inutile...
	if(myBrowserDlg) {
//		myBrowserDlg->DestroyWindow();		serve?? crasha!
		delete myBrowserDlg;
		myBrowserDlg=NULL;
		}
	if(d) {
		delete d->theTV;
		d->theTV=NULL;
		}
	}


BEGIN_MESSAGE_MAP(CVidsendView2, CView)
	//{{AFX_MSG_MAP(CVidsendView2)
	ON_WM_TIMER()
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_WM_RBUTTONDOWN()
	ON_WM_DESTROY()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	//}}AFX_MSG_MAP
	ON_WM_GETMINMAXINFO()
	ON_MESSAGE(WM_VIDEOFRAME_READY,OnVideoFrameReady)
	ON_MESSAGE(WM_AUDIOFRAME_READY,OnAudioFrameReady)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendView2 drawing

void CVidsendView2::OnDraw(CDC* pDC) {
	CVidsendDoc2 *pDoc=GetDocument();
	RECT r;

	if(!pDoc->theTV || !pDoc->theTV->GetHwnd()) {
		if(pDoc->Opzioni & CVidsendDoc2::openVideoOnConnect) {
			GetClientRect(&r);
			theApp.renderBitmap(pDC,IDB_VIDEOWAITING,&r);
			}
		else {
			GetClientRect(&r);
			theApp.renderBitmap(pDC,IDB_NOVIDEO,&r);
			}
		}
	}

void CVidsendView2::OnEditCopy() {
	CVidsendDoc2 *pDoc=GetDocument();
	HANDLE h;
	BYTE *p;
	DWORD l,ti;
	
	if(pDoc && OpenClipboard()) {

		pDoc->theTV->theFrame=(BYTE *)-1;

		ti=timeGetTime()+4000;
		while(timeGetTime()<ti && pDoc->theTV->theFrame==(BYTE *)-1) {
			MSG msg;
			if(::PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
				if(!theApp.PumpMessage()) { 
					}
				}
			}
		if(pDoc->theTV->theFrame==(BYTE *)-1) {
			MessageBeep(MB_ICONHAND);
			goto fine;
			}

		l=pDoc->theTV->biRawDef.bmiHeader.biWidth*pDoc->theTV->biRawDef.bmiHeader.biHeight*pDoc->theTV->biRawDef.bmiHeader.biBitCount/8;
 		h=GlobalAlloc(GMEM_MOVEABLE,l+sizeof(BITMAPINFOHEADER)+32);
		p=(BYTE *)GlobalLock(h);
		memcpy(p,&pDoc->theTV->biRawDef,sizeof(BITMAPINFOHEADER));
		memcpy(p+sizeof(BITMAPINFOHEADER),pDoc->theTV->theFrame,l);
		GlobalUnlock(h);
	  EmptyClipboard();
	  SetClipboardData(CF_DIB,h);
fine:
	  CloseClipboard();
	  }  
	
	}

void CVidsendView2::OnUpdateEditCopy(CCmdUI* pCmdUI) {
	CVidsendDoc2 *pDoc=GetDocument();
	
	pCmdUI->Enable(pDoc->theTV || pDoc->trasmMode>0);
	}


/////////////////////////////////////////////////////////////////////////////
// CVidsendView2 diagnostics

#ifdef _DEBUG
void CVidsendView2::AssertValid() const {
	
	CView::AssertValid();
	}

void CVidsendView2::Dump(CDumpContext& dc) const {
	
	CView::Dump(dc);
	}

CVidsendDoc2 *CVidsendView2::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVidsendDoc2)));
	return (CVidsendDoc2 *)m_pDocument;
	}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendView2 message handlers

BOOL CVidsendView2::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) {
	int i;
	CVidsendDoc2 *d;

	if(i=CView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext)) {
		i=doConnect();
		}
	return i;
	}

int CVidsendView2::doConnect() {
	CVidsendDoc2 *d;

	d=GetDocument();
	if(d) {			// tanto per sicurezza...

#ifdef PROVA
		CConfirmExhibDlg myDlg(d);
			if(myDlg.DoModal() == IDOK) {
				CWebCliSocket myWWW;
				CString S,S2,S3,Stemp;
				char myBuf[512],myBuf2[100];
				if(myWWW.Connect("www.newmeet.com")) {
					if(theApp.debugMode) 
						if(theApp.FileSpool)
							theApp.FileSpool->print(CLogFile::flagInfo,"  GET/HTTP connect su %s/Espos/ProxyNMvidsend.asp?","www.newmeet.com");
					S2="/Espos/ProxyNMvidsend.asp?";

					S3.Format("IDEspos=%u&",5799);
					Stemp=CWebCliSocket::translateString((LPCTSTR)myDlg.m_Titolo);
					S3+="Titolo=";
					S3+=Stemp;
					S3+="&Descrizione=";
					Stemp=CWebCliSocket::translateString((LPCTSTR)myDlg.m_Descrizione);
					S3+=Stemp;
					Stemp.Format("&IDCategoria=%u&IDTipoSessione=%u",
						myDlg.m_Categoria+(myDlg.m_Adulti ? 0 : 10),		//corregge per avere un ID fisso a certi record simulati...
						myDlg.m_Sessione);
					S3+=Stemp;
					Stemp.Format("&Tariffa=%3.2f&Sconto=%u",myDlg.m_Costo,myDlg.m_Sconto);
//					Stemp.SetAt(Stemp.Find('.'),',');		// patchina per server Win2000 in americano!
					S3+=Stemp;
					S3+="&PasswordSconto=";
					Stemp=CWebCliSocket::translateString((LPCTSTR)myDlg.m_Password);
					S3+=Stemp;

//					S3="IDEspos=4417&Titolo=provavidsend&Descrizione=01234567890123456789&IDCategoria=0&IDTipoSessione=0&Tariffa=3,75&Sconto=10&PasswordSconto=pippo";
					S=myWWW.buildQuery("www.newmeet.com",S2+S3 /*,1*/);
		if(theApp.debugMode) 
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"  GET/HTTP S vale -%s-",(LPCTSTR)S);
					i=myWWW.Send((LPCTSTR)S);
		if(theApp.debugMode) 
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"  GET/HTTP send: %d",i);
					myWWW.Receive(myBuf,500);		// HTTP/1.1 200 OK<CR><LF>
		if(theApp.debugMode) 
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"  GET receive: %s",myBuf);
					myWWW.Disconnect();
					sscanf(myBuf,"%s %u ",myBuf2,&i);
		if(theApp.debugMode) 
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"  i = %u",i);
					if(i != 200)
						MessageBox("Dati non riconosciuti.",NULL,MB_OK);
					}
				}
#endif


		if(d->Opzioni & CVidsendDoc2::registerServer) {
			MSG msg;
			DWORD l;

			if(d->Opzioni & CVidsendDoc2::needAuthenticateServer && !d->directoryWWW.IsEmpty()) {
				CPasswordDlg myDlg;
rifo:
				d->authSocket=new CAuthCliSocket(d);
				if(!d->authSocket)
					return FALSE;
				myDlg.m_Nome=d->directoryWWWLogin;
#ifndef _CAMPARTY_MODE
				if(myDlg.DoModal() == IDOK) {
					char myBuf[128];

					if(!d->authSocket->Create())
						goto auth_not_avail;
					if(!d->authSocket->Connect(d->directoryWWW,AUTHENTICATION_SOCKET))
						goto auth_not_avail;
					d->myLogin=myDlg.m_Nome;
					d->myPassword=myDlg.m_Pasw;
#else
					if(!d->authSocket->Create())
						goto auth_not_avail;
					if(!d->authSocket->Connect(d->directoryWWW,AUTHENTICATION_SOCKET))
						goto auth_not_avail;
					d->myLogin=theApp.infoUtente.login;
					d->myPassword=theApp.infoUtente.pasw;
#endif

					OSVERSIONINFO osvi;
					DWORD mySerNum;
					osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
					GetVersionEx(&osvi);
					GetVolumeInformation("C:\\",NULL,0,&mySerNum,NULL,NULL,NULL,0);
					d->authSocket->SendUserPass((char *)(LPCTSTR)d->myLogin,(char *)(LPCTSTR)d->myPassword,1,0,
						theApp.getVersione(),MAKELONG(osvi.dwMinorVersion,osvi.dwMajorVersion),mySerNum);
					l=timeGetTime()+30000;			// 30sec timeout
					while(!d->authSocket->response && timeGetTime() < l) {
						if(::PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
							if(!theApp.PumpMessage()) { 
								}
							}
						}
					d->myID=d->authSocket->IDutente;
					d->myTimedConn=d->authSocket->timedConn;
					// se si vuole limitare il tempo di trasmissione, inserire un msg qua
					switch(d->authSocket->response) {
						case 0:
auth_not_avail:
#ifdef _LINGUA_INGLESE
							AfxMessageBox("Authentication not available");
#else
							AfxMessageBox("Autenticazione non disponibile");
#endif
auth_failed:
							d->authSocket->Close();
							delete d->authSocket;
							d->authSocket=NULL;
							return FALSE;
							break;
						case -5:
#ifdef _LINGUA_INGLESE
							AfxMessageBox("User not authorized.");
#else
							AfxMessageBox("Utente non autorizzato.");
#endif
							goto auth_failed;
							break;
						case -4:
#ifdef _LINGUA_INGLESE
							AfxMessageBox("User is already connected!\nExit and try again.");
#else
							AfxMessageBox("Utente già connesso!\nUscire e ritentare l'operazione.");
#endif
							goto auth_failed;
							break;
						case -3:
#ifdef _LINGUA_INGLESE
							if(MessageBox("User unknown! Retry?",NULL,MB_ICONQUESTION | MB_YESNO) == IDYES) {
#else
							if(MessageBox("Utente non riconosciuto! Ritentare?",NULL,MB_ICONQUESTION | MB_YESNO) == IDYES) {
#endif
								d->authSocket->Close();
								delete d->authSocket;
								d->authSocket=NULL;
								goto rifo;
								}
							else
								goto auth_failed;
							break;
						case -2:
#ifdef _LINGUA_INGLESE
							if(MessageBox("Wrong password! Retry?",NULL,MB_ICONQUESTION | MB_YESNO) == IDYES) {
#else
							if(MessageBox("Password errata! Ritentare?",NULL,MB_ICONQUESTION | MB_YESNO) == IDYES) {
#endif
								d->authSocket->Close();
								delete d->authSocket;
								d->authSocket=NULL;
								goto rifo;
								}
							else
								goto auth_failed;
							break;
						case -1:
#ifdef _LINGUA_INGLESE
							AfxMessageBox("Unknown user!");
#else
							AfxMessageBox("Utente non riconosciuto!");
#endif
							goto auth_failed;
							break;
						case 1:
							_tcscpy(theApp.infoUtente.login,(LPCTSTR)myDlg.m_Nome);
							_tcscpy(theApp.infoUtente.pasw,(LPCTSTR)myDlg.m_Pasw);
							d->directoryWWWLogin=myDlg.m_Nome;
#ifndef _NEWMEET_MODE
							AfxMessageBox("Login OK!");
#endif
							break;
						default:		// ERRORE!
							goto auth_failed;
							break;
						}

#ifndef _CAMPARTY_MODE
					}			// if myDlg.DoModal...
				else {
					goto auth_failed;
					}
#endif
	//				cliSockCtrl->SendUserPass((char *)(LPCTSTR)myDlg.m_Nome,(char *)(LPCTSTR)myDlg.m_Pasw,myAuthClient.extraParm);
				}
#ifdef _NEWMEET_MODE

//			CHtmlView myHtml;		// non va... solo da serialization...
//			myHtml.Create(NULL,"Dati sessione",WS_VISIBLE | WS_CHILD,rc,this,AFX_IDW_PANE_FIRST);
/*			CVidsendDoc5 *myBrowser;
			CString S;
//			S="http://";
			S=d->authenticationWWW;
			S+="/Espos/ProxyNMvidsend.asp";
			myBrowser=(CVidsendDoc5 *)(theApp.pDocTemplate5)->OpenDocumentFile(NULL);
			myBrowser->setMode(1);
			myBrowser->setURL(S);*/

			CString S,S2;
//			S="http://";
			S=d->authenticationWWW;
			S+="/Espos/ProxyNMvidsend.asp?";
			S2.Format("ID=%u&pasw=%s&video=%ux%u&camOK=%u",
				d->myID,d->myPassword,d->myQV.imageSize.bottom,d->myQV.imageSize.right,d->theTV ? d->theTV->GetHwnd() : 0);
			// il valore di CamOK indica se c'e' un driver video funzionante (serve solo su Camparty...)
			S+=S2;

			if(myBrowserDlg)
				delete myBrowserDlg;
			myBrowserDlg=new CBrowserDlg(S);

#ifdef _CAMPARTY_MODE			// per PROVA! metto ifndef
			myBrowserDlg->Create(IDD_BROWSER);
			l=timeGetTime()+60000;			// 60sec timeout
			while(!myBrowserDlg->updateOK && timeGetTime() < l) {
				if(::PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
					if(!theApp.PumpMessage()) { 
						}
					}
				}
			if(myBrowserDlg->updateOK) {
//				AfxMessageBox("updateok");
				}
			else {
				delete myBrowserDlg;
				return FALSE;
				}

#else
			myBrowserDlg->DoModal();
			if(myBrowserDlg->updateOK) {
				}
			else {
//				AfxMessageBox("La registrazione della sessione non è stata accettata.\nFine del programma");
				//sovrascrivendo la stringa di default "AFX_IDP_FAILED_TO_CREATE_DOC" nelle String Table cambio il msg standard "impossibile creare un documento vuoto" e ci metto questo!
				return FALSE;
				}
			delete myBrowserDlg;
			myBrowserDlg=NULL;
#endif


#ifdef DARIO

			CConfirmExhibDlg myDlg(d);
			if(myDlg.DoModal() == IDOK) {

// non nell'INI!, nel database... (lo fa l'ASP)
//				d->WritePrivateProfileInt(IDS_PREZZO_EXHIB,(DWORD)(myDlg.m_Costo*100.0));
//				d->WritePrivateProfileInt(IDS_SCONTO_EXHIB,myDlg.m_Sconto);
//				d->WritePrivateProfileString(IDS_PASSWORD_SCONTO_EXHIB,(LPCTSTR)myDlg.m_Password);

				CWebCliSocket myWWW;
				CString S,S2,S3,Stemp;
				char myBuf[256],myBuf2[100];
				if(myWWW.Connect(/*"www.newmeet.dns2go.com" */ /*"www.newmeet.com"*/ d->authenticationWWW)) {
					if(theApp.debugMode) 
						if(theApp.FileSpool)
							theApp.FileSpool->print(2,"  GET/HTTP connect su %s/Espos/ProxyNMvidsend.asp?",d->authenticationWWW);
					S2="/Espos/ProxyNMvidsend.asp?";
					S3.Format("IDEspos=%u&",d->myID);
					Stemp=CWebCliSocket::translateString((LPCTSTR)myDlg.m_Titolo);
					S3+="Titolo=";
					S3+=Stemp;
					S3+="&Descrizione=";
					Stemp=CWebCliSocket::translateString((LPCTSTR)myDlg.m_Descrizione);
					S3+=Stemp;
					Stemp.Format("&IDCategoria=%u&IDTipoSessione=%u",
						myDlg.m_Categoria+(myDlg.m_Adulti ? 0 : 10),		//corregge per avere un ID fisso a certi record simulati...
						myDlg.m_Sessione);
					S3+=Stemp;
					Stemp.Format("&Tariffa=%3.2f&Sconto=%u",myDlg.m_Costo,myDlg.m_Sconto);
//					Stemp.SetAt(Stemp.Find('.'),',');		// patchina per server Win2000 in americano!
					S3+=Stemp;
					S3+="&PasswordSconto=";
					Stemp=CWebCliSocket::translateString((LPCTSTR)myDlg.m_Password);
					S3+=Stemp;

//					S3="IDEspos=4417&Titolo=provavidsend&Descrizione=01234567890123456789&IDCategoria=0&IDTipoSessione=0&Tariffa=3,75&Sconto=10&PasswordSconto=pippo";
					S=myWWW.buildQuery(d->authenticationWWW,S2+S3 /*,1*/);
		if(theApp.debugMode) 
			if(theApp.FileSpool)
				theApp.FileSpool->print(2,"  GET/HTTP S vale -%s-",(LPCTSTR)S);
					i=myWWW.Send((LPCTSTR)S);
		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(2,"  GET/HTTP send: %d",i);
					myWWW.Receive(myBuf,17);		// HTTP/1.1 200 OK<CR><LF>
		if(theApp.debugMode) 
			if(theApp.FileSpool)
				theApp.FileSpool->print(2,"  GET receive: %s",myBuf);
			// ci sarebbe il "connection: close (in risposta dal server)... da GESTIRE!!
					myWWW.Disconnect();
					sscanf(myBuf,"%s %u ",myBuf2,&i);
		if(theApp.debugMode) {
			if(theApp.FileSpool)
				theApp.FileSpool->print(2,"  i = %u",i);
					if(i != 200) {
						wsprintf(myBuf,"Dati non riconosciuti: errore del server %u",i);
						MessageBox(myBuf,NULL,MB_OK);
						}
					}	
				}
			else
				return FALSE;
#endif			// fine DARIO (non + usato 28/2/02)

#endif

			}

		if(d->Opzioni & CVidsendDoc2::openVideoOnConnect) {
			}
		else {
			d->openVideo(this);
			}
		}

	return 1;
	}

int CVidsendView2::endConnect() {
	CVidsendDoc2 *d;

	d=GetDocument();
	if(d && d->authSocket) {			// tanto per sicurezza...
		d->authSocket->Close();
		delete d->authSocket;
		d->authSocket=NULL;
		}
	return 1;
	}

void CVidsendView2::OnTimer(UINT nIDEvent) {
	CVidsendDoc2 *doc=GetDocument();
  struct AV_PACKET_HDR *avh;
	char *p;
	BYTE *d,*d2,*s,*dOut;
	int i;
	long bSize;
	DWORD l,t;
	CBitmap *b;
	BITMAP bmp;
	RECT r;
	ACMSTREAMHEADER hhacm;
	static int divider,divider2;
	static BOOL bInSend=0;

	p=(char *)GlobalAlloc(GPTR,1024);
	if(!bInSend) {
		bInSend=TRUE;
	divider++;		// questo fa avere un audio ogni n video
	if(!doc->bPaused) {
		switch(doc->trasmMode) {
			case 0:
				if(doc->alternaSource) {
			//			videoSource		// fare...
					}
				break;

			case 2:
				SetRectEmpty(&r);
				r.right=doc->theTV->biRawDef.bmiHeader.biWidth;
				r.bottom=doc->theTV->biRawDef.bmiHeader.biHeight;
				b=theApp.createTestBitmap(&r,&doc->theTV->biRawDef.bmiHeader,doc->pagProva.tipoVideo);
				b->GetBitmap(&bmp);
				bSize=bmp.bmWidthBytes*bmp.bmHeight;
				s=(BYTE *)GlobalAlloc(GMEM_FIXED,bSize);
				b->GetBitmapBits(bSize,s);

				{
				CDC *hdc = GetDC();
				HDRAWDIB hdd = DrawDibOpen();
					i=DrawDibDraw(hdd, hdc->m_hDC, 0, 0, doc->theTV->biRawDef.bmiHeader.biWidth, doc->theTV->biRawDef.bmiHeader.biHeight, 
						&doc->theTV->biRawDef.bmiHeader, s, 0, 0, doc->theTV->biRawDef.bmiHeader.biWidth , doc->theTV->biRawDef.bmiHeader.biHeight, 0);	
					ReleaseDC(hdc);
				DrawDibClose(hdd);
				}

				if(doc->Opzioni & CVidsendDoc2::forceBN)
					doc->theTV->convertColorBitmapToBN(&doc->theTV->biRawDef.bmiHeader,s);
				if(doc->Opzioni & CVidsendDoc2::doFlip)
					doc->theTV->flipBitmap(&doc->theTV->biRawDef.bmiHeader,s);
				if(doc->Opzioni & CVidsendDoc2::doMirror)
					doc->theTV->mirrorBitmap(&doc->theTV->biRawDef.bmiHeader,s);
				if(doc->imposeDateTime)
					doc->theTV->superImposeDateTime(&doc->theTV->biRawDef.bmiHeader,s);
				if(doc->theTV->imposeTextPos) {
					doc->theTV->superImposeText(&doc->theTV->biRawDef.bmiHeader,s,doc->theTV->imposeText,RGB(200,200,0));
					}
				if(!IsRectEmpty(&theApp.theServer->qualityBox)) {
					i=doc->theTV->checkQualityBox(&doc->theTV->biRawDef.bmiHeader,s,&doc->qualityBox);
					doc->theTV->superImposeBox(&doc->theTV->biRawDef.bmiHeader,s,&doc->qualityBox,i ? RGB(255,0,0) : RGB(0,255,0));
					}
				if(doc->theTV->theFrame == (BYTE *)-1) {
					doc->theTV->theFrame=(BYTE *)GlobalAlloc(GMEM_FIXED,bSize);
					memcpy(doc->theTV->theFrame,s,bSize);
					}
				l=t=0;

				d=(BYTE *)GlobalAlloc(GMEM_FIXED,doc->theTV->maxFrameSize  +100 /*AV_PACKET_HDR_SIZE*/);
				d2=d+AV_PACKET_HDR_SIZE;			// indirizzo del buffer DOPO la struct header!
				avh=(struct AV_PACKET_HDR *)d;
				i=ICCompress(doc->theTV->m_hICCo,(gdvFrameNum % doc->myQV.fps) ? 0 : ICCOMPRESS_KEYFRAME,
					&doc->theTV->biCompDef.bmiHeader,d2+sizeof(BITMAPINFOHEADER),&doc->theTV->biRawDef.bmiHeader,s,
					&l,&t,gdvFrameNum++,0 /*10000*/,doc->myQV.quality,
					NULL,NULL);
				if(i == ICERR_OK) {
					l=GetDocument()->theTV->biCompDef.bmiHeader.biSizeImage;
					memcpy(d2,&doc->theTV->biCompDef.bmiHeader,sizeof(BITMAPINFOHEADER));
					l+=sizeof(BITMAPINFOHEADER);
#ifdef _NEWMEET_MODE
					avh->tag=MAKEFOURCC('G','D','2','0');
#else
					avh->tag=MAKEFOURCC('D','G','2','0');
#endif
					avh->type=0;
					avh->len=l;
					avh->info=t;
		//			avh.psec=500;
					avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());

					if(theApp.debugMode) {
						wsprintf(p,"VFrameT# %ld: lungo %ld (%ld)",gdvFrameNum,bSize,l); 
						}
					avh->reserved1=avh->reserved2=0;
					PostMessage(WM_VIDEOFRAME_READY,(WPARAM)d,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
					}
				else {
					if(theApp.debugMode) 
						wsprintf(p,"Non mando video!"); 
					GlobalFree(d);	
					}
				GlobalFree(s);
				delete b;


				if(!(divider % doc->myQV.fps)) {
					divider2++;
					if(!(GetDocument()->pagProva.audioOpzioni & 1) || (divider2 & 1)) {
						if(GetDocument()->bAudio) {
							switch(GetDocument()->pagProva.tipoAudio) {
								case 0:
									i=100;
									break;
								case 1:
									i=440;
									break;
								case 2:
									i=1000;
									break;
								case 3:
									i=5000;
									break;
								case 4:
									i=10000;
									break;
								}
							d=theApp.createTestWave(&doc->theTV->wfex,&l,i,GetDocument()->pagProva.audioOpzioni & 2);
				//			l+=sizeof(WAVEFORMATEX);

							dOut=(BYTE *)GlobalAlloc(GMEM_FIXED,doc->theTV->maxWaveoutSize +100 /*AV_PACKET_HDR_SIZE*/);
							d2=dOut+AV_PACKET_HDR_SIZE;			// indirizzo del buffer DOPO la struct header!
							avh=(struct AV_PACKET_HDR *)dOut;
							hhacm.cbStruct=sizeof(ACMSTREAMHEADER);
							hhacm.fdwStatus=0;
							hhacm.dwUser=(DWORD)0 /*this*/;
							hhacm.pbSrc=d;
							hhacm.cbSrcLength=l;
				//			hhacm.cbSrcLengthUsed=0;
							hhacm.dwSrcUser=0;
							hhacm.pbDst=d2;
							hhacm.cbDstLength=doc->theTV->maxWaveoutSize;
				//			hhacm.cbDstLengthUsed=0;
							hhacm.dwDstUser=0;
							if(!acmStreamPrepareHeader(doc->theTV->m_hAcm,&hhacm,0)) {
#ifdef _NEWMEET_MODE
								avh->tag=MAKEFOURCC('G','D','2','0');
#else
								avh->tag=MAKEFOURCC('D','G','2','0');
#endif
								avh->type=1;
			//					avh.psec=1000;
								avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
								avh->info=theApp.theServer->theTV->wfd.wf.wFormatTag;

								i=acmStreamConvert(doc->theTV->m_hAcm,&hhacm,ACM_STREAMCONVERTF_BLOCKALIGN);
						//	wsprintf(myBuf,"convertito: %d, %d bytes",i,l);
						//	AfxMessageBox(myBuf);
								acmStreamUnprepareHeader(doc->theTV->m_hAcm,&hhacm,0);

								avh->len=hhacm.cbDstLengthUsed;
								if(theApp.debugMode) 
									wsprintf(p,"AFrameT# %ld: lungo %ld (%ld)",gdaFrameNum++,hhacm.cbDstLengthUsed,l); 
								avh->reserved1=avh->reserved2=0;
								PostMessage(WM_AUDIOFRAME_READY,(WPARAM)dOut,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
								}
							else {
								GlobalFree(dOut);
								}
							GlobalFree(d);
							}
						}
					}
				break;

			case 1:
				if(doc->psVideo) {
					d=(BYTE *)GlobalAlloc(GMEM_FIXED,doc->theTV->maxFrameSize+AV_PACKET_HDR_SIZE  +100 /*AV_PACKET_HDR_SIZE*/);
					d2=d+AV_PACKET_HDR_SIZE;			// indirizzo del buffer DOPO la struct header!
					avh=(struct AV_PACKET_HDR *)d;

rewind:
					if((i=AVIStreamRead(doc->psVideo,gdvFrameNum,1,d2+sizeof(BITMAPINFOHEADER),
						doc->theTV->maxFrameSize /* ! */,&bSize,NULL)) == AVIERR_OK) {

						// qua in MSDN era gia + chiaro, c'è SOLO IL FRAME, no header :)


/*							{
							CFile mF;
							char buf[160];
							wsprintf(buf,"bmi=%u, bmih=%u, avh=%u",sizeof(BITMAPINFOHEADER),
								sizeof(BITMAPINFOHEADER),sizeof(AV_PACKET_HDR));	// 44, 40, 28(24)
						if(mF.Open("c:\\bmp1.bin",CFile::modeCreate | CFile::modeReadWrite | CFile::typeBinary | CFile::shareDenyWrite)) {
							mF.Write(d,10000);
							mF.Close();
							}
							}*/

						// adattare la velocita' di playback a quella del server usando PBstreamInfo.dwscale e dwrate
						// e anche le dimensioni??

#ifdef _NEWMEET_MODE
						avh->tag=MAKEFOURCC('G','D','2','0');
#else
						avh->tag=MAKEFOURCC('D','G','2','0');
#endif
						avh->type=0;
						//			avh.psec=500;
						avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
						if(doc->PBsi->fccHandler == doc->myQV.compressor && 
							!(doc->OpzioniSorgenteVideo & CVidsendDoc2::aviMode) /* ovvero anche (doc->Opzioni & videoType) */ ) {
							avh->info=AVIStreamIsKeyFrame(doc->psVideo,gdvFrameNum) ? AVIIF_KEYFRAME : 0;
							avh->len=bSize+sizeof(BITMAPINFOHEADER);
							memcpy(d2,doc->PBbiSrc,sizeof(BITMAPINFOHEADER));
							wsprintf(p,"VFramePB# compresso %ld: lungo %ld",gdvFrameNum,bSize); 
							avh->reserved1=avh->reserved2=0;

/*							{
							biCompDef=*bmi;
							biRawDef=biCompDef;
							biRawDef.biCompression=0;
							hICDe=ICDecompressOpen(ICTYPE_VIDEO,biCompDef.biCompression,&biCompDef,NULL);
							if(!hICDe)
							ICerror:
								AfxMessageBox("Impossibile inizializzare decompressore");
							else
								if(ICDecompressBegin(hICDe,&biCompDef,&biRawDef) != ICERR_OK)
									goto ICerror;
							i=ICDecompress(hICDe, avh->info & AVIIF_KEYFRAME ? 0 : ICDECOMPRESS_NOTKEYFRAME,
								bi,(s+sizeof(BITMAPINFOHEADER)),&biRawDef,theFrame);
							if(i==ICERR_OK) {
								CDC *hdc = GetDC();
								HDRAWDIB hdd = DrawDibOpen();
								i=DrawDibDraw(hdd, hdc->m_hDC, 0, 0, doc->theTV->biRawDef.bmiHeader.biWidth, doc->theTV->biRawDef.bmiHeader.biHeight, 
									&doc->theTV->biRawDef.bmiHeader, b, 0, 0, doc->theTV->biRawDef.bmiHeader.biWidth , doc->theTV->biRawDef.bmiHeader.biHeight, 0);	
								ReleaseDC(hdc);
								DrawDibClose(hdd);
								}
							}*/

							PostMessage(WM_VIDEOFRAME_READY,(WPARAM)avh,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
							gdvFrameNum++;
							goto fine;
							}
						else {
							BITMAPINFOHEADER *mybi;
							BYTE *b;
							mybi=(LPBITMAPINFOHEADER)AVIStreamGetFrame(doc->gotFrame,gdvFrameNum);
							// qua c'è la bitmap completa con header (BITMAPINFOHEADER)
							if(!mybi) 
								goto error2;
							b=theApp.scaleBitmap((LPBITMAPINFO)mybi,&doc->theTV->biRawDef,NULL);

/*							{
							DWORD l=500000L;
							b=(BYTE *)GlobalAlloc(GPTR,l);
							memcpy(b,((BYTE *)mybi)+sizeof(BITMAPINFOHEADER),300000);
							CFile mF;
						if(mF.Open("c:\\bmp.bin",CFile::modeCreate | CFile::modeReadWrite | CFile::typeBinary | CFile::shareDenyWrite)) {
							mF.Write(mybi,10000);
							mF.Close();
							}
							}*/
							if(!b) 
								goto error2;

							{
							CDC *hdc = GetDC();
							HDRAWDIB hdd = DrawDibOpen();
							i=DrawDibDraw(hdd, hdc->m_hDC, 0, 0, doc->theTV->biRawDef.bmiHeader.biWidth, doc->theTV->biRawDef.bmiHeader.biHeight, 
								&doc->theTV->biRawDef.bmiHeader, b, 0, 0, doc->theTV->biRawDef.bmiHeader.biWidth , doc->theTV->biRawDef.bmiHeader.biHeight, 0);	
							ReleaseDC(hdc);
							DrawDibClose(hdd);
							}
							if(doc->theTV->theFrame == (BYTE *)-1) {		// se richiesto...
								doc->theTV->theFrame=(BYTE *)GlobalAlloc(GMEM_FIXED,doc->theTV->biBaseRawBitmap.biHeight*doc->theTV->biBaseRawBitmap.biWidth*3);	// prelevo e salvo il fotogramma per uso altrove (p.es. salva img)
								memcpy(doc->theTV->theFrame,b,doc->theTV->biBaseRawBitmap.biHeight*doc->theTV->biBaseRawBitmap.biWidth*3);
								}

							if(doc->Opzioni & CVidsendDoc2::forceBN)
								doc->theTV->convertColorBitmapToBN(&doc->theTV->biRawDef.bmiHeader,b);
							if(doc->Opzioni & CVidsendDoc2::doFlip)
								doc->theTV->flipBitmap(&doc->theTV->biRawDef.bmiHeader,b);
							if(doc->Opzioni & CVidsendDoc2::doMirror)
								doc->theTV->mirrorBitmap(&doc->theTV->biRawDef.bmiHeader,b);
							if(doc->imposeDateTime)
								doc->theTV->superImposeDateTime(&doc->theTV->biCompDef.bmiHeader,b);
							if(doc->theTV->imposeTextPos) {
								doc->theTV->superImposeText(&doc->theTV->biRawDef.bmiHeader,b,doc->theTV->imposeText,RGB(200,200,0));
								}
							if(!IsRectEmpty(&theApp.theServer->qualityBox)) {
								i=doc->theTV->checkQualityBox(&doc->theTV->biRawDef.bmiHeader,b,&doc->qualityBox);
								doc->theTV->superImposeBox(&doc->theTV->biRawDef.bmiHeader,b,&doc->qualityBox,i ? RGB(255,0,0) : RGB(0,255,0));
								}
							t=l=0;
							i=ICCompress(doc->theTV->m_hICCo,(gdvFrameNum % doc->myQV.fps) ? 0 : ICCOMPRESS_KEYFRAME,
								&doc->theTV->biCompDef.bmiHeader,d2+sizeof(BITMAPINFOHEADER),&doc->theTV->biRawDef.bmiHeader,b,
								&l,&t,gdvFrameNum,0 ,doc->myQV.quality,
								NULL,NULL);
							if(i != ICERR_OK) 
								goto error2;
							GlobalFree(b);
							l+=sizeof(BITMAPINFOHEADER);
							avh->len=l /*doc->theTV->biCompDef.bmiHeader.biSizeImage*/;
							memcpy(d2,&doc->theTV->biCompDef.bmiHeader,sizeof(BITMAPINFOHEADER));
							avh->info=t;

							if(theApp.debugMode) {
								wsprintf(p,"VFramePB ricompresso# %ld: lungo %ld (%ld)",gdvFrameNum,bSize,l); 
								}
							avh->reserved1=avh->reserved2=0;
							PostMessage(WM_VIDEOFRAME_READY,(WPARAM)d,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
							gdvFrameNum++;
							goto fine;
							}
error2:
						GlobalFree(d);
						}
					else {
						if(i==AVIERR_FILEREAD) {
							gdvFrameNum=0;
							if(doc->OpzioniSorgenteVideo & CVidsendDoc2::aviLoop) {
								i=AVIStreamFindSample(doc->psVideo,gdvFrameNum,FIND_ANY | FIND_FROM_START);
								if(i == -1)
									doc->setTXMode(2);
								else
									goto rewind;
								}
							else
								doc->setTXMode(2);
							}
						GlobalFree(d);
						}
					}
				break;
			}

		}
fine:
		bInSend=FALSE;
		}
	if(*p)
		theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
	CView::OnTimer(nIDEvent);
	}

/*void CVidsendView2::OnSize(UINT nType, int cx, int cy) {
	RECT r;

//	CView::OnSize(nType, cx, cy);
	GetClientRect(&r);
//	r.right=cx;
//	r.bottom=cy;
	if(((CVidsendDoc2 *)GetDocument())->theTV)
		(((CVidsendDoc2 *)GetDocument())->theTV)->Resize(&r);
	cx=r.right;
	cy=r.bottom+GetSystemMetrics(SM_CYEDGE)+GetSystemMetrics(SM_CYEDGE)+GetSystemMetrics(SM_CYCAPTION);
//	GetParent()->MoveWindow(r.left,r.top,r.right,r.bottom,TRUE);
	SetWindowPos(NULL,r.left,r.top,r.right,r.bottom,SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
	GetParent()->SetWindowPos(NULL,r.left,r.top,cx,cy,SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);

	}
*/

void CVidsendView2::OnRButtonDown(UINT nFlags, CPoint point) {
	CMenu myMenu;
	
#ifdef _NEWMEET_MODE

	CMenu *myMenu2=new CMenu;
#ifdef _LINGUA_INGLESE
	myMenu2->CreateMenu();
	myMenu2->AppendMenu(MF_STRING,ID_FILE_IMPOSTAZIONISTREAMING,"Streaming...");
	myMenu2->AppendMenu(MF_STRING,ID_FILE_IMPOSTAZIONISORGENTEVIDEO,"Video source...");
	myMenu2->AppendMenu(MF_STRING,ID_FILE_IMPOSTAZIONIFORMATOVIDEO,"Video format...");
#else
	myMenu2->CreateMenu();
	myMenu2->AppendMenu(MF_STRING,ID_FILE_IMPOSTAZIONISTREAMING,"Streaming...");
	myMenu2->AppendMenu(MF_STRING,ID_FILE_IMPOSTAZIONISORGENTEVIDEO,"Sorgente video...");
	myMenu2->AppendMenu(MF_STRING,ID_FILE_IMPOSTAZIONIFORMATOVIDEO,"Formato video...");
#endif

#else
	myMenu.LoadMenu(IDR_VIDSENTYPE2);
	CMenu *myMenu2=myMenu.GetSubMenu(2);
	myMenu2->AppendMenu(MF_SEPARATOR);
#ifdef _LINGUA_INGLESE
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Properties...");
#else
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Proprietà...");
#endif

#endif

	ClientToScreen(&point);
	myMenu2->TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, GetParent());
	
#ifdef _NEWMEET_MODE
	myMenu2->DestroyMenu();
#endif

	CView::OnRButtonDown(nFlags, point);
	}



void CVidsendView2::OnLButtonDown(UINT nFlags, CPoint point) {
	
	inClick=TRUE;
	CView::OnLButtonDown(nFlags, point);
	}

void CVidsendView2::OnLButtonUp(UINT nFlags, CPoint point) {
	
	inClick=FALSE;
	CView::OnLButtonUp(nFlags, point);
	}

void CVidsendView2::OnMouseMove(UINT nFlags, CPoint point) {
	
//gestire spostamento dell'intera finestra, clickandoci dentro!
	// e magari anche il posizionamento del quality box??
	CView::OnMouseMove(nFlags, point);
	}

void CVidsendView2::OnDestroy() {
	CVidsendDoc2 *d=GetDocument();
	RECT rc;
	char myBuf[64];
	
	CView::OnDestroy();
	
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		GetParent()->GetWindowRect(&rc);	// coord. schermo della mia MDIChildFrmae...
		GetParent()->GetParent()->ScreenToClient(&rc);	// ... diventano coord. client rispetto al MDI padre (che NON e' theApp.m_pMainWnd !! ce n'è una un mezzo...
		wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
		d->WritePrivateProfileString(IDS_COORDINATE,myBuf);
		}
	}


afx_msg LRESULT CVidsendView2::OnVideoFrameReady(WPARAM wParam, LPARAM lParam) {
	AV_PACKET_HDR *avh=(AV_PACKET_HDR *)wParam;
	CTV *myTV=GetDocument()->theTV;
	int retVal=0,i,j;
	MSG msg;

//	if(i=GetQueueStatus(QS_POSTMESSAGE) && !(j=GetQueueStatus(QS_TIMER))) {		// questo evita che si crei un accavallamento di messaggi del video a scapito degli altri di Windows... ma...
//		goto fine;		// ...non serve con il meccanismo dei client asincroni...
//		}

	if(!HIWORD(wParam))		// questa patch serve perche' aprendo la finestra proprieta' della sorgente video (p.es. Logitech) arriva un messaggio identico e ovviamente crasha! l'alternativa era cambiare #messaggio WM_VIDEOFRAME_READY
		goto fine;
//	if(PeekMessage(&msg,m_hWnd,WM_VIDEOFRAME_READY,WM_VIDEOFRAME_READY,PM_REMOVE))
//		goto fine;
  
	if(GetDocument()->streamSocketV && avh) {
		retVal=GetDocument()->streamSocketV->Manda(avh,lParam);
//		if(retVal=GetDocument()->streamSocketV->Manda(avh,pSBuf,avh->len))
			// in teoria questo controllo (che e' globale per TUTTI i client), non dovrebbe servire,
			// ..poiche' "stalled" del socket gestisce la trasmissione lenta e trasmettera' solo KEY_FRAME
			myTV->vFrameNum++;
//		else
//			myTV->vFrameNum -= myTV->vFrameNum % myTV->framesPerSec;		// forzo il prossimo a keyframe, se errore in spedizione...
		}																																		// ...e mettere una spietta di allarme??
		// in teoria, se un client non riceve un pacchetto perche' pieno, rischia di incasinarsi con i non-key frame!
		// pero' come fare ad avere un meccanismo flessibile per ogni client?

	if(GetDocument()->pipeV && avh) {
		retVal=GetDocument()->MandaPipeV(avh,lParam);
//			myTV->vFrameNum++;
		}



		// mettere un indicatore dei byte spediti e moltiplicato per i client??
fine:
/*	if(!((*(((WORD *)&(avh->info))+1)))) {
		if(avh)
			GlobalFree(avh);
		if(pSBuf)
			GlobalFree(pSBuf);
		}*/



/*
	static DWORD oldTimer;
	if(oldTimer < timeGetTime()) {

		oldTimer=timeGetTime()+10000;//******************
		GetDocument()->sendHrtBt();			// siccome continua a perderseli... lo metto anche qua!
		}
*/





  return (LRESULT)retVal;
	}

afx_msg LRESULT CVidsendView2::OnAudioFrameReady(WPARAM wParam, LPARAM lParam) {
	AV_PACKET_HDR *avh=(AV_PACKET_HDR *)wParam;
	CTV *myTV=GetDocument()->theTV;
	int retVal=0;
	MSG msg;

	// questo evita che si crei un accavallamento di messaggi del video a scapito degli altri di Windows...
//	if(PeekMessage(&msg,m_hWnd,WM_AUDIOFRAME_READY,WM_AUDIOFRAME_READY,PM_NOREMOVE))
//		goto fine;
	if(!HIWORD(wParam))		// questa patch serve perche' aprendo la finestra proprieta' della sorgente video (p.es. Logitech) arriva un messaggio identico e ovviamente crasha! l'alternativa era cambiare #messaggio WM_VIDEOFRAME_READY
		goto fine;					// (ma anche nell'audio??)

	if(GetDocument()->streamSocketA && avh) {
		retVal=GetDocument()->streamSocketA->Manda(avh,lParam);
		myTV->aFrameNum++;
		}

fine:
/*	if(avh)
		GlobalFree(avh);
	if(pSBuf)
		GlobalFree(pSBuf);*/

	return (LRESULT)retVal;
	}


/////////////////////////////////////////////////////////////////////////////
// CVidsendView3 - log

IMPLEMENT_DYNCREATE(CVidsendView3, CListView)

CVidsendView3::CVidsendView3() {

	m_pSet=NULL;
	sortCol=0;	
	}

CVidsendView3::~CVidsendView3() {
	
	if(m_pSet) {
		if(m_pSet->IsOpen()) 
			m_pSet->Close();
		m_pSet=NULL;
		}
	}


BEGIN_MESSAGE_MAP(CVidsendView3, CListView)
	//{{AFX_MSG_MAP(CVidsendView3)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnColumnclick)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnGetdispinfo)
	ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnDeleteitem)
	ON_COMMAND(ID_LOG_CANCELLARIGA, OnLogCancellariga)
	ON_UPDATE_COMMAND_UI(ID_LOG_CANCELLARIGA, OnUpdateLogCancellariga)
	ON_COMMAND(ID_LOG_CANCELLATUTTO, OnLogCancellatutto)
	ON_WM_RBUTTONDOWN()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendView3 drawing

void CVidsendView3::OnDraw(CDC* pDC) {
	CVidsendDoc3 *pDoc = GetDocument();
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendView3 diagnostics

#ifdef _DEBUG
void CVidsendView3::AssertValid() const {
	CListView::AssertValid();
	}

void CVidsendView3::Dump(CDumpContext& dc) const {
	CListView::Dump(dc);
	}

CVidsendDoc3 *CVidsendView3::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVidsendDoc3)));
	return (CVidsendDoc3 *)m_pDocument;
	}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendView3 message handlers

void CVidsendView3::OnInitialUpdate() {
	LV_ITEM myLV;
	int n;
//	struct DB_DATA *d;

//	CRecordView::OnInitialUpdate();

	CListView::OnInitialUpdate();

	m_pSet=GetDocument()->m_vidsendSet;
	if(m_pSet) {
		if(m_pSet->Open(AFX_DB_USE_DEFAULT_TYPE,NULL,CRecordset::none)) {
		
	//	m_pSet->m_strFilter="autore like '";
	//	m_pSet->m_strFilter+=strupr(s);
	//	m_pSet->m_strFilter+="%'";
			m_pSet->m_strSort="data desc";
			m_pSet->Requery();
			n=0;
			while(!m_pSet->IsEOF()) {
				myLV.mask=LVIF_TEXT | LVIF_PARAM;
				myLV.iItem=n;
				myLV.iSubItem=0;
				myLV.pszText=LPSTR_TEXTCALLBACK;
				myLV.lParam = (LPARAM)m_pSet->m_ID;
				myLV.iImage = 0;
				GetListCtrl().InsertItem(&myLV);
				m_pSet->MoveNext();
				n++;
				}
			}
		//	ResizeParentToFit();
		}
	}

void CVidsendView3::OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult) {
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
	char myBuf[128];

	if(pDispInfo->item.mask & LVIF_TEXT) {
//		struct DB_DATA *d = (struct DB_DATA *)(pDispInfo->item.lParam);

		wsprintf(myBuf,"id=%u",pDispInfo->item.lParam);
		m_pSet->m_strFilter=myBuf;
		m_pSet->Requery();
		switch(pDispInfo->item.iSubItem) {
			case 0:
				_tcscpy(pDispInfo->item.pszText,m_pSet->m_DATA.Format("%d/%m/%Y %H:%M:%S"));
				break;
			case 1:
				_tcscpy(pDispInfo->item.pszText,m_pSet->m_INDIRIZZO);
				break;
			case 2:
				_tcscpy(pDispInfo->item.pszText,m_pSet->m_DESCRIZIONE);
				break;
			case 3:
				wsprintf(pDispInfo->item.pszText,"%u",m_pSet->m_LIVELLO);
				break;
			default:
				*pDispInfo->item.pszText=0;
				break;
			}
		}
		
	*pResult = 0;
	}

void CVidsendView3::OnColumnclick(NMHDR* pNMHDR, LRESULT* pResult) {
	NM_LISTVIEW *pNMListView = (NM_LISTVIEW *)pNMHDR;

	// numero colonna e' iSubItem (0..3)
//	AfxMessageBox("column click");
	sortCol=pNMListView->iSubItem;
	GetListCtrl().SortItems((PFNLVCOMPARE)CompareElem, (DWORD)this);
	
	*pResult = 0;
	}

void CVidsendView3::OnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult) {
	NM_LISTVIEW *pNMListView = (NM_LISTVIEW *)pNMHDR;
	LV_ITEM lv;
	char myBuf[128];

	lv.mask = LVIF_PARAM;
	lv.iItem = pNMListView->iItem;
	lv.iSubItem = 0;
	if(GetListCtrl().GetItem(&lv) == TRUE)	{
/*		wsprintf(myBuf,"id=%u",lv.lParam);
		m_pSet->m_strFilter=myBuf;
		m_pSet->Requery();
		m_pSet->Delete();*/
//		struct DB_DATA *d = (struct DB_DATA *)(lv.lParam);
//		delete d;
		}
	*pResult = 0;

	}

int CALLBACK CVidsendView3::CompareElem(DWORD d1, DWORD d2, CVidsendView3 *p) {
	int nCmp=0;
	char myBuf[128];
	CTime data1,data2;
	CString descr1,descr2,ind1,ind2;
	DWORD lev1,lev2;

	wsprintf(myBuf,"id=%u",d1);
	p->m_pSet->m_strFilter=myBuf;
	p->m_pSet->Requery();
	data1=p->m_pSet->m_DATA;
	descr1=p->m_pSet->m_DESCRIZIONE;
	ind1=p->m_pSet->m_INDIRIZZO;
	lev1=p->m_pSet->m_LIVELLO;
	wsprintf(myBuf,"id=%u",d2);
	p->m_pSet->m_strFilter=myBuf;
	p->m_pSet->Requery();
	data2=p->m_pSet->m_DATA;
	descr2=p->m_pSet->m_DESCRIZIONE;
	ind2=p->m_pSet->m_INDIRIZZO;
	lev2=p->m_pSet->m_LIVELLO;
	switch(p->sortCol) {
		case 0:
			nCmp = data1 > data2 ? -1 : 1;
			if(!nCmp)
				nCmp = ind1 > ind2 ? 1 : -1;
			break;
		case 1:
			nCmp = ind1 > ind2 ? 1 : -1;
			if(!nCmp)
				nCmp = data1 > data2 ? -1 : 1;
			break;
		case 2:
			nCmp = descr1 > descr2 ? 1 : -1;
			if(!nCmp) {
				nCmp = data1 > data2 ? -1 : 1;
				if(!nCmp)
					nCmp = ind1 > ind2 ? 1 : -1;
				}
			break;
		case 3:
			nCmp = lev2 - lev1;
			if(!nCmp) {
				nCmp = data1 > data2 ? -1 : 1;
				if(!nCmp)
					nCmp = ind1 > ind2 ? 1 : -1;
				}
			break;
		default:
			break;
		}
	return nCmp;
	}



BOOL CVidsendView3::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) {
	int i;
	
	if(i=CView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext)) {
		GetListCtrl().SetItemCount(100);	 // verificare!
		GetListCtrl().InsertColumn(0,"Data/ora",LVCFMT_LEFT,140);
		GetListCtrl().InsertColumn(1,"Indirizzo",LVCFMT_LEFT,100);
		GetListCtrl().InsertColumn(2,"Descrizione evento",LVCFMT_LEFT,250);
		GetListCtrl().InsertColumn(3,"Severità",LVCFMT_LEFT,60);
		DWORD dws = GetWindowLong(GetListCtrl().m_hWnd,GWL_STYLE);   // Get the current window style. 
	  SetWindowLong(GetListCtrl().m_hWnd, GWL_STYLE,(dws & ~LVS_TYPEMASK) | LVS_REPORT); 
		GetListCtrl().Arrange(LVA_DEFAULT);
		}

	return i;
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendView database support
CRecordset* CVidsendView3::OnGetRecordset() {
	return m_pSet;
	}


void CVidsendView3::OnLogCancellariga() {
	LV_ITEM lv;
	POSITION p;
	int i;
	
	p=GetListCtrl().GetFirstSelectedItemPosition(); 
	i=GetListCtrl().GetNextSelectedItem(p);
	lv.mask = LVIF_PARAM;
	lv.iItem = i;
	lv.iSubItem = 0;
	if(GetListCtrl().GetItem(&lv) == TRUE)	{
		if(m_pSet->goTo(lv.lParam)) {
			m_pSet->Delete();
			GetListCtrl().DeleteItem(1);
			}
		}
	}

void CVidsendView3::OnUpdateLogCancellariga(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(1);
	}

void CVidsendView3::OnLogCancellatutto() {
	
	if(MessageBox("Cancellare l'intero log?",NULL,MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) == IDYES) {	
		m_pSet->m_strFilter="";
		m_pSet->Requery();
		while(!m_pSet->IsEOF()) {
			m_pSet->Delete();
			m_pSet->MoveNext();
			}
		GetListCtrl().DeleteAllItems();
		}
	}

void CVidsendView3::OnRButtonDown(UINT nFlags, CPoint point) {
	CMenu myMenu;
	
	myMenu.LoadMenu(IDR_VIDSENTYPE3);
	CMenu *myMenu2=myMenu.GetSubMenu(2);
	myMenu2->AppendMenu(MF_SEPARATOR);
#ifdef _LINGUA_INGLESE
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Properties...");
#else
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Proprietà...");
#endif
	ClientToScreen(&point);
	myMenu2->TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, GetParent());

	CListView::OnRButtonDown(nFlags, point);
	}


void CVidsendView3::OnDestroy() {
	CVidsendDoc3 *d=GetDocument();
	RECT rc;
	char myBuf[64];
	
	CListView::OnDestroy();
	
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		GetParent()->GetWindowRect(&rc);	// coord. schermo della mia MDIChildFrmae...
		GetParent()->GetParent()->ScreenToClient(&rc);	// ... diventano coord. client rispetto al MDI padre (che NON e' theApp.m_pMainWnd !! ce n'è una un mezzo...
		wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
		d->WritePrivateProfileString(IDS_COORDINATE,myBuf);
		}
	}





/////////////////////////////////////////////////////////////////////////////
// CVidsendView4 - chat

IMPLEMENT_DYNCREATE(CVidsendView4, CFormView)

#define A_LINE_OF_TEXT 255

CVidsendView4::CVidsendView4()
	: CFormView(CVidsendView4::IDD) {
	//{{AFX_DATA_INIT(CVidsendView4)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	hIconUser=theApp.LoadIcon(IDI_RUNNERMAN);
	hIconSupervisor=theApp.LoadIcon(IDI_SUPERVISOR);
	hIconComputer=theApp.LoadIcon(IDI_COMPUTER);
	hIconSuona=theApp.LoadIcon(IDI_SPEAKER);
	hIconClear=theApp.LoadIcon(IDI_CLEAR);
	hIconIcone=theApp.LoadIcon(IDI_ICONE);
	hIconOne2One=theApp.LoadIcon(IDI_ONE2ONE);
	bmpSpkOn.LoadBitmap(IDB_SPKON);
	il.Create(IDB_CONNECTIONS,16,0,RGB(255,255,255));
	theFont.CreateFont(8,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_DONTCARE,"MS sans serif");
	cliSock=new CChatCliSocket(this);
	wl=NULL;
	}


CVidsendView4::~CVidsendView4() {
	
	if(cliSock)
		delete cliSock;
	cliSock=NULL;
	}

void CVidsendView4::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendView4)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CVidsendView4, CFormView)
	//{{AFX_MSG_MAP(CVidsendView4)
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_COMMAND(ID_CONNESSIONE_CONNETTI, OnConnessioneConnetti)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_CONNETTI, OnUpdateConnessioneConnetti)
	ON_COMMAND(ID_CONNESSIONE_DISCONNETTI, OnConnessioneDisconnetti)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_DISCONNETTI, OnUpdateConnessioneDisconnetti)
	ON_COMMAND(ID_CONNESSIONE_RICONNETTI, OnConnessioneRiconnetti)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONE_RICONNETTI, OnUpdateConnessioneRiconnetti)
	ON_WM_DRAWITEM()
	ON_WM_MEASUREITEM()
	ON_WM_CONTEXTMENU()
	ON_WM_DELETEITEM()
	ON_COMMAND(ID_UTENTI_CONNETTIVIDEO, OnUtentiConnettivideo)
	ON_UPDATE_COMMAND_UI(ID_UTENTI_CONNETTIVIDEO, OnUpdateUtentiConnettivideo)
	ON_COMMAND(ID_UTENTI_MANDAEMAIL, OnUtentiMandaemail)
	ON_UPDATE_COMMAND_UI(ID_UTENTI_MANDAEMAIL, OnUpdateUtentiMandaemail)
	ON_COMMAND(ID_UTENTE_DISCONNETTI, OnUtenteDisconnetti)
	ON_UPDATE_COMMAND_UI(ID_UTENTE_DISCONNETTI, OnUpdateUtenteDisconnetti)
	ON_COMMAND(ID_UTENTE_INSERISCINEGLIINDESIDERATI, OnUtenteInseriscinegliindesiderati)
	ON_UPDATE_COMMAND_UI(ID_UTENTE_INSERISCINEGLIINDESIDERATI, OnUpdateUtenteInseriscinegliindesiderati)
	ON_COMMAND(ID_UTENTE_MANDAMESSAGGIO, OnUtenteMandamessaggio)
	ON_UPDATE_COMMAND_UI(ID_UTENTE_MANDAMESSAGGIO, OnUpdateUtenteMandamessaggio)
	ON_COMMAND(ID_UTENTI_MANDAMESSAGGIOPERTUTTI, OnUtentiMandamessaggiopertutti)
	ON_UPDATE_COMMAND_UI(ID_UTENTI_MANDAMESSAGGIOPERTUTTI, OnUpdateUtentiMandamessaggiopertutti)
	ON_COMMAND(ID_MESSAGGIO_CANCELLA, OnMessaggioCancella)
	ON_UPDATE_COMMAND_UI(ID_MESSAGGIO_CANCELLA, OnUpdateMessaggioCancella)
	ON_COMMAND(ID_MESSAGGIO_CANCELLATUTTO, OnMessaggioCancellatutto)
	ON_UPDATE_COMMAND_UI(ID_MESSAGGIO_CANCELLATUTTO, OnUpdateMessaggioCancellatutto)
	ON_COMMAND(ID_MESSAGGIO_INVIA, OnMessaggioInvia)
	ON_UPDATE_COMMAND_UI(ID_MESSAGGIO_INVIA, OnUpdateMessaggioInvia)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateFileSave)
	ON_COMMAND(ID_FILE_SAVE_ALL, OnFileSaveAll)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_ALL, OnUpdateFileSaveAll)
	ON_COMMAND(ID_VIDEO_COPIATESTOCHAT, OnCopiaChat)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_COPIATESTOCHAT, OnUpdateCopiaChat)
	ON_WM_DESTROY()
	ON_NOTIFY_REFLECT(NM_RCLICK, OnRclick)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendView4 diagnostics

#ifdef _DEBUG
void CVidsendView4::AssertValid() const
{
	CFormView::AssertValid();
}

void CVidsendView4::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
CVidsendDoc4* CVidsendView4::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVidsendDoc4)));
	return (CVidsendDoc4 *)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendView4 message handlers

BOOL CVidsendView4::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) {
	int i,n;
	char myBuf[64];
	CWnd *v;

	if(i=CFormView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext)) {
// era x riempire lista owner-draw delle icone... ora spostato sotto in pulsante POPUP
		}

/*   WCHAR szLic[] = L"TS-1234567890";
per #licenza TXOle...
   BSTR bstrKey = SysAllocString(szLic);
   BOOL bSuccess = m_txctrl.Create(NULL, dwStyle, rect,
      this, 1000, NULL, NULL,bstrKey);
   SysFreeString(bstrKey);
   if (!bSuccess)
      return 0;*/



	return i;
	}

int CVidsendView4::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	char myBuf[64];
	int i;

	if(CFormView::OnCreate(lpCreateStruct) == -1)
		return -1;

	CVidsendDoc4 *d=GetDocument();
	if(d->Opzioni & CVidsendDoc4::serverMode) {			// se sono server, mi riconnetto automaticamente a me stesso!
		d->srvAddress=CWebSrvSocket2_base::getMyOutmostIPAddress(myBuf);
		}
	PostMessage(WM_COMMAND,ID_CONNESSIONE_CONNETTI,0);
	return 0;
	}

void CVidsendView4::OnConnessioneConnetti() {
	CVidsendDoc4 *d=GetDocument();
	CDlgEnterURL myDlg(1,d);
	CString prevURL[CVidsendDoc4::MRU_SIZE];

	d->loadCliConnRecent(prevURL);
	
	if(theApp.Opzioni & CVidsendApp::openClientChat && d->srvAddress.IsEmpty() && d->firstConnect) {
		d->srvAddress=prevURL[0];
		}
	if(!d->srvAddress.IsEmpty()) {
		goto connetti;
		}
	if(myDlg.DoModal(prevURL,CVidsendDoc4::MRU_SIZE) == IDOK) {
		d->srvAddress=myDlg.m_URLstring;
connetti:
		d->firstConnect=0;
		OnConnessioneRiconnetti();
		}
	}

int CVidsendView4::doConnect(char *myAddress,struct CHAT_INFO *si) {
	CVidsendDoc4 *d=GetDocument();
	int i=0;
	char myBuf[128];
	CWnd *v;

	chatInfo=*si;
	if(*si->authenticationWWW) {
		CPasswordDlg myDlg;
		if(myDlg.DoModal() == IDOK) {
			}
		else {
			goto fine;
			}
		}
	
	*myBuf=1;
	*(BYTE *)(myBuf+1)=d->Opzioni & CVidsendDoc4::serverMode ? CVidsendView4::supervisorMsg : 0;
	*(BYTE *)(myBuf+1) |= theApp.theServer ? CVidsendView4::serverVideo : 0;
	*(DWORD *)(myBuf+2)=d->Opzioni & CVidsendDoc4::usaColors ? d->opzioniVisive & 0xffffff : 0x000000;
	DWORD mySerNum;
	GetVolumeInformation("C:\\",NULL,0,&mySerNum,NULL,NULL,NULL,0);
	*(DWORD *)(myBuf+5) = mySerNum;
	*(BYTE *)(myBuf+9) = 0 /*myDlg->m_One2One*/;
	_tcscpy(myBuf+10,(LPCTSTR)d->loginName);
	cliSock->Send(myBuf,42);
	v=GetDlgItem(IDC_BUTTON1);
	if(v)
		v->EnableWindow(TRUE);
	v=GetDlgItem(IDC_BUTTON2);
	if(v)
		v->EnableWindow(TRUE);
	v=GetDlgItem(IDC_BUTTON3);
	if(v)
		v->EnableWindow(TRUE);
	v=GetDlgItem(IDC_BUTTON4);
	if(v)
		v->EnableWindow(TRUE);
	v=GetDlgItem(IDC_BUTTON5);
	if(v) {
		v->EnableWindow(d->Opzioni & CVidsendDoc4::noOne2One ? FALSE : TRUE);
//		((CButton *)v)->SetState(d->Opzioni & CVidsendDoc4::onlyOne2One ? TRUE : FALSE);
		((CButton *)v)->SetCheck(d->Opzioni & CVidsendDoc4::onlyOne2One ? 1 : 0);
		}
	i=1;
	if(si->maxTime > 0) {
		CTimedMessageBox myDlg;
		CString S;
		S.Format("Durata massima della connessione: %umin. %usec.",si->maxTime.GetTotalMinutes(),si->maxTime.GetSeconds());
		myDlg.DoModal("Connessione",S,MB_OK | MB_ICONINFORMATION);
		}
	if(*si->openWWW) {
		theApp.nuovoBrowser(si->openWWW);
		}

fine:
	return i;
	}

void CVidsendView4::OnUpdateConnessioneConnetti(CCmdUI* pCmdUI) {
	pCmdUI->Enable(cliSock->m_hSocket == INVALID_SOCKET);
	}

void CVidsendView4::OnConnessioneDisconnetti() {
	endConnect();
	}

int CVidsendView4::endConnect() {
	CVidsendDoc4 *d=GetDocument();

	((CChildFrame4 *)GetParent())->m_wndStatusBar.GetStatusBarCtrl().SetIcon(1,((CChildFrame4 *)GetParent())->iconOff);
	((CChildFrame4 *)GetParent())->m_wndStatusBar.GetStatusBarCtrl().SetIcon(2,NULL);
	((CChildFrame4 *)GetParent())->m_wndStatusBar.SetPaneText(0,"Non connesso",TRUE);
	CWnd *v=GetDlgItem(IDC_BUTTON1);
	if(v)
		v->EnableWindow(FALSE);
	v=GetDlgItem(IDC_BUTTON2);
	if(v)
		v->EnableWindow(FALSE);
	v=GetDlgItem(IDC_BUTTON3);
	if(v)
		v->EnableWindow(FALSE);
	v=GetDlgItem(IDC_BUTTON4);
	if(v)
		v->EnableWindow(FALSE);
/*	v=GetDlgItem(IDC_BUTTON5);
	if(v)
		v->EnableWindow(FALSE);*/
	if(cliSock)
		cliSock->Close();
	d->srvAddress.Empty();
	InvalidateRect(NULL);
	return 1;
	}


void CVidsendView4::OnUpdateConnessioneDisconnetti(CCmdUI* pCmdUI) {
	pCmdUI->Enable(cliSock->m_hSocket != INVALID_SOCKET);
	}

void CVidsendView4::OnConnessioneRiconnetti() {
	char myBuf[128];
	int i=0;
  RASDIALPARAMS rdParams;
  char szBuf[256],*p;
	DWORD dwRet;

	cliSock->Create(0,theApp.Opzioni & CVidsendApp::TCP_UDP ? SOCK_DGRAM : SOCK_STREAM);
rifo:
#ifdef _LINGUA_INGLESE
	wsprintf(myBuf,"Connecting to %s",(LPCTSTR)GetDocument()->srvAddress);
#else
	wsprintf(myBuf,"Connessione in corso a %s",(LPCTSTR)GetDocument()->srvAddress);
#endif
	((CChildFrame4 *)GetParent())->m_wndStatusBar.SetPaneText(0,myBuf,TRUE);
	if(cliSock->Connect(GetDocument()->srvAddress,TEXT_SOCKET)) {
		((CChildFrame4 *)GetParent())->m_wndStatusBar.GetStatusBarCtrl().SetIcon(1,((CChildFrame4 *)GetParent())->iconOn);

#ifdef _LINGUA_INGLESE
		wsprintf(myBuf,"Connected to server %s",(LPCTSTR)GetDocument()->srvAddress);
#else
		wsprintf(myBuf,"Connesso al server %s",(LPCTSTR)GetDocument()->srvAddress);
#endif
		((CChildFrame4 *)GetParent())->m_wndStatusBar.SetPaneText(0,myBuf,TRUE);
		}
	else {
		if(!i && GetDocument()->Opzioni & CVidsendDoc::autoRASconnect) {
			if(theApp.hRasConn)
				goto fine;
			rdParams.dwSize = sizeof(RASDIALPARAMS);
			_tcscpy(rdParams.szEntryName,theApp.RASEntry);
			rdParams.szPhoneNumber[0]=0;
			rdParams.szCallbackNumber[0]='\0';
			*rdParams.szUserName=0;	//_tcscpy(rdParams.szUserName, theApp.RASUsername);
			*rdParams.szPassword=0;	//_tcscpy(rdParams.szPassword, theApp.RASPassword);
			rdParams.szDomain[0] = '\0';
			dwRet=RasDial(NULL,NULL,&rdParams,0L,NULL,&theApp.hRasConn);
			if(dwRet) {
				if(RasGetErrorString((UINT)dwRet,(LPSTR)myBuf,120) != 0)
					wsprintf( (LPSTR)myBuf,"Undefined Dial Error (%ld).",dwRet);
				AfxMessageBox(myBuf);
				if(theApp.FileSpool)
					*theApp.FileSpool << myBuf;
				goto fine;
				}
			i++;
			goto rifo;
			}
fine:
		((CChildFrame4 *)GetParent())->m_wndStatusBar.GetStatusBarCtrl().SetIcon(1,((CChildFrame *)GetParent())->iconOff);
		((CChildFrame4 *)GetParent())->m_wndStatusBar.SetPaneText(0,"Non connesso",TRUE);
		cliSock->Close();
		CVidsendDoc4 *d=GetDocument();
		d->srvAddress.Empty();
#ifdef _LINGUA_INGLESE
		AfxMessageBox("Unable to connect to server");
#else
		AfxMessageBox("Impossibile connettersi al server");
#endif
		}
	}

void CVidsendView4::OnUpdateConnessioneRiconnetti(CCmdUI* pCmdUI) {
	pCmdUI->Enable(cliSock->m_hSocket == INVALID_SOCKET && !GetDocument()->srvAddress.IsEmpty() );
	}

void CVidsendView4::OnSize(UINT nType, int cx, int cy) {
	CWnd *v;
	CVidsendDoc4 *pDoc = GetDocument();
	int x1,x2,x3,x4;
	int y1,y2,y3,y4;
	DWORD n;

	CFormView::OnSize(nType, cx, cy);
	cx-=GetSystemMetrics(SM_CXFRAME)*2;
	cy-=GetSystemMetrics(SM_CYFRAME)*2;
	x1=8;
	y1=8;
	y2=cy-35;
	y3=cy-20;
	y4=cy-4;
	if(pDoc->Opzioni & CVidsendDoc4::serverMode) {
//#ifndef _CAMPARTY_MODE
#ifndef _NEWMEET_MODE
		x2=(cx*3)/4-5;
		x3=(cx*3)/4+15;
		x4=cx-8+10;
#else
		x2=cx-10;
		x3=(cx*3)/4+15;
		x4=cx- 8+10;
#endif
		}
	else {
		x2=cx-8;
		}

	v=GetDlgItem(IDC_RICHEDIT1);
	if(v) {
		v->SetWindowPos(NULL,x1,y1,x2-30,y2,SWP_NOACTIVATE | SWP_NOZORDER);
//		((CListBox *)v)->SetHorizontalExtent(1024);
		n=::GetWindowLong(v->m_hWnd,GWL_STYLE);
		::SetWindowLong(v->m_hWnd,GWL_STYLE,n | WS_CLIPSIBLINGS);
		}
	v=GetDlgItem(IDC_STATIC1);
	if(v)
		v->SetWindowPos(NULL,x1+2,y3+3,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
	v=GetDlgItem(IDC_EDIT1);
	if(v) {
#ifdef _NEWMEET_MODE
		v->SetWindowPos(NULL,x1+2,y3,x2-x1-50,20,SWP_NOACTIVATE | SWP_NOZORDER);
#else
		v->SetWindowPos(NULL,x1+62,y3,x2-x1-115,20,SWP_NOACTIVATE | SWP_NOZORDER);
#endif
		((CEdit *)v)->LimitText(A_LINE_OF_TEXT);
		}
	v=GetDlgItem(IDC_BUTTON1);
	if(v) {
//#ifndef _CAMPARTY_MODE
#ifndef _NEWMEET_MODE
		v->SetWindowPos(NULL,x2-42,y3,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#else
		v->SetWindowPos(NULL,x2-44,y3,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#endif
		n=::GetWindowLong(v->m_hWnd,GWL_STYLE);
		::SetWindowLong(v->m_hWnd,GWL_STYLE,n | WS_CLIPSIBLINGS);
		}
	v=GetDlgItem(IDC_BUTTON2);
	if(v) {
//#ifndef _CAMPARTY_MODE
#ifndef _NEWMEET_MODE
		v->SetWindowPos(NULL,x2-14,y1+5,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#else
		v->SetWindowPos(NULL,x2-14,y1+5,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#endif
		v->SendMessage(BM_SETIMAGE,(WPARAM)IMAGE_ICON,(LPARAM)(HANDLE)hIconClear);
		n=::GetWindowLong(v->m_hWnd,GWL_STYLE);
		::SetWindowLong(v->m_hWnd,GWL_STYLE,n | WS_CLIPSIBLINGS);
		}
	v=GetDlgItem(IDC_BUTTON3);
	if(v) {
//#ifndef _CAMPARTY_MODE
#ifndef _NEWMEET_MODE
		v->SetWindowPos(NULL,x2-14,y1+40,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#else
		v->SetWindowPos(NULL,x2-14,y1+40,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#endif
		v->SendMessage(BM_SETIMAGE,(WPARAM)IMAGE_ICON,(LPARAM)(HANDLE)hIconSuona);
		n=::GetWindowLong(v->m_hWnd,GWL_STYLE);
		::SetWindowLong(v->m_hWnd,GWL_STYLE,n | WS_CLIPSIBLINGS);
		}
	v=GetDlgItem(IDC_BUTTON4);
	if(v) {
//#ifndef _CAMPARTY_MODE
#ifndef _NEWMEET_MODE
		v->SetWindowPos(NULL,x2-14,y1+75,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#else
		v->SetWindowPos(NULL,x2-14,y1+75,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#endif
		v->SendMessage(BM_SETIMAGE,(WPARAM)IMAGE_ICON,(LPARAM)(HANDLE)hIconIcone);
		n=::GetWindowLong(v->m_hWnd,GWL_STYLE);
		::SetWindowLong(v->m_hWnd,GWL_STYLE,n | WS_CLIPSIBLINGS);
		}
	v=GetDlgItem(IDC_BUTTON5);
	if(v) {
//#ifndef _CAMPARTY_MODE
#ifndef _NEWMEET_MODE
		v->SetWindowPos(NULL,x2-14,y1+110,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#else
		v->SetWindowPos(NULL,x2-14,y1+110,0,0,SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
#endif
		v->SendMessage(BM_SETIMAGE,(WPARAM)IMAGE_ICON,(LPARAM)(HANDLE)hIconOne2One);
		n=::GetWindowLong(v->m_hWnd,GWL_STYLE);
		::SetWindowLong(v->m_hWnd,GWL_STYLE,n | WS_CLIPSIBLINGS);
//		n=::GetWindowLong(v->m_hWnd,GWL_STYLE);
//		::SetWindowLong(v->m_hWnd,GWL_STYLE,n | BS_3STATE );
		}

	v=GetDlgItem(IDC_TREE1);
	if(v) {
#ifdef _NEWMEET_MODE
		v->ShowWindow(SW_HIDE);		// c'e' anche nelle propr. del form
#else
		v->ShowWindow(pDoc->Opzioni & CVidsendDoc4::serverMode ? SW_SHOW : SW_HIDE);
		v->SetWindowPos(NULL,x3,y1,x4-x3,y4-y1+4,SWP_NOACTIVATE | SWP_NOZORDER);
#endif
		n=::GetWindowLong(v->m_hWnd,GWL_STYLE);
		::SetWindowLong(v->m_hWnd,GWL_STYLE,n | WS_CLIPSIBLINGS);
		}

	SIZE size;
	size.cx=0;
	size.cy=0;
	SetScrollSizes(MM_TEXT,size);		// questo disattiva gli scrollbar che altrimenti compaiono quando la view si tringe rispetto alle dim. iniziali
	}


BOOL CVidsendView4::OnCommand(WPARAM wParam, LPARAM lParam) {
	CVidsendDoc4 *pDoc = GetDocument();
	int i,n,bDestroyed=0;
	static int btnState;
	CString S;
	char myBuf[260];

	switch(LOWORD(wParam)) {
		case IDC_EDIT1:
			break;
		case IDC_BUTTON1:		// non so se "INVIO" sia meglio metterlo o no...
			switch(HIWORD(wParam)) {
				case BN_CLICKED:
//					((CEdit *)FromHandle((HWND)lParam))
					OnMessaggioInvia();
					break;
				}
			break;
		case IDC_BUTTON2:		// Reset
			switch(HIWORD(wParam)) {
				case BN_CLICKED:
//					((CEdit *)FromHandle((HWND)lParam))
					OnMessaggioCancellatutto();
					((CEdit *)GetDlgItem(IDC_EDIT1))->SetFocus();
					break;
				}
			break;
		case IDC_BUTTON3:
			switch(HIWORD(wParam)) {
				case BN_CLICKED:
					if(pDoc->Opzioni & CVidsendDoc4::usaSounds && pDoc->opzioniVisive & CVidsendDoc4::avvisi_sonori2) {
						CMenu myMenu;
						CPoint point;
						myMenu.CreatePopupMenu();
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_CONSTIPATION,"Costipazione");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_CREAK_DOOR,"Porta");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_GRUNT1,"Grunt1");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_GRUNT2,"Grunt2");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_KISS,"Bacio");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_RISATA_GRASSA,"Risata");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_RODEO,"Rodeo");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_ROMANTIC_KISS,"Bacio romantico");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_SHOUT_CRY,"Urlo");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_THROAT,"Throat");
						myMenu.AppendMenu(MF_STRING | MF_ENABLED,IDR_WAV_WOW_WHISTLE,"Wow!");
						for(i=0; i<11; i++)
							myMenu.SetMenuItemBitmaps(i,MF_BYPOSITION,&bmpSpkOn,&bmpSpkOn);

						GetCursorPos(&point);
				//		ClientToScreen(&point);
						myMenu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, this);

						((CButton *)GetDescendantWindow(IDC_BUTTON1))->SetFocus();
//						((CEdit *)GetDescendantWindow(IDC_EDIT1))->SetWindowText("\\\\#suono ");
						}
					break;
				}
			break;
		case IDC_BUTTON4:		// icone
			switch(HIWORD(wParam)) {
				case BN_CLICKED:
//					((CEdit *)FromHandle((HWND)lParam))
					POINT point;
					RECT rc;
					wl=new CListBox;
					GetCursorPos(&point);
					GetWindowRect(&rc);
					rc.top=point.y-rc.top;
					rc.left=point.x-rc.left-60;
					rc.bottom=rc.top+150;
					rc.right=rc.left+70;
					wl->Create(WS_VISIBLE | WS_CHILD /*| WS_OVERLAPPED */ | WS_BORDER | WS_TABSTOP | WS_VSCROLL | LBS_OWNERDRAWVARIABLE | LBS_NOTIFY | LBS_WANTKEYBOARDINPUT /* | LBS_MULTIPLECOLUMNS*/,
						rc,this,IDC_LIST2);
					wl->SendMessage(WM_SETFONT,(WPARAM)(HFONT)theFont,MAKELPARAM(1,0));
					wl->SetOwner(this);		// questi servono (insieme con WS_CLIPSIBLINGS applicato alle altre sotto-finestr, v.sopra) per visualizzare correttamente la lista DAVANTI al resto! (WS_POPUP non è applicabile)
					wl->SetParent(this);
					n=::GetWindowLong(wl->m_hWnd,GWL_EXSTYLE);
					::SetWindowLong(wl->m_hWnd,GWL_EXSTYLE,n | WS_EX_WINDOWEDGE);
//					wl->SetWindowPos(&wndTop,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
					wl->SetFocus();
					wl->InitStorage(150,30000);
					for(i=IDB_EMOT_001; i<=IDB_EMOT_126; i++) {
//				wsprintf(myBuf,"%u",i);
						n=wl->SendMessage(LB_ADDSTRING,0,(LPARAM)i);
//				v->SendMessage(LB_SETITEMDATA,n,(LPARAM)::LoadImage(theApp.m_hInstance,MAKEINTRESOURCE(i),IMAGE_BITMAP,0,0,LR_DEFAULTCOLOR));
						}
/*					w.SetCapture();
					MSG msg;
					while(1) {
						if(PeekMessage(&msg,m_hWnd,0,0,PM_REMOVE)) {
							if(!theApp.PumpMessage()) { 
								}
							}
						}
					ReleaseCapture();*/

					break;
				}
			break;
		case IDC_BUTTON5:		// 1:1
//					S.Format("%08X",wParam);
			switch(HIWORD(wParam)) {
//				case BN_DBLCLK:
				case BN_CLICKED:
//					AfxMessageBox(((CButton *)GetDescendantWindow(IDC_BUTTON5)) == CWnd::GetFocus() ? "clcked has focus" : "clicked");
//					AfxMessageBox(S);
//					if(!(((CButton *)GetDescendantWindow(IDC_BUTTON5))->GetState() & 0x0008)) {
						if(pDoc->Opzioni & CVidsendDoc4::onlyOne2One) {
							pDoc->Opzioni &= ~CVidsendDoc4::onlyOne2One;
//							((CButton *)GetDescendantWindow(IDC_BUTTON5))->SetState(0);
							((CButton *)GetDescendantWindow(IDC_BUTTON5))->SetCheck(0);
							}
						else {
							pDoc->Opzioni &= ~CVidsendDoc4::noOne2One;
							pDoc->Opzioni |= CVidsendDoc4::onlyOne2One;
//							((CButton *)GetDescendantWindow(IDC_BUTTON5))->SetState(1);
							((CButton *)GetDescendantWindow(IDC_BUTTON5))->SetCheck(1);
							}
	//					((CEdit *)GetDescendantWindow(IDC_EDIT1))->SetFocus();
//					}
					break;
//				case BN_DBLCLK:
//					break;
				case BN_KILLFOCUS:
//					AfxMessageBox("killfocus");
/*						if(pDoc->Opzioni & CVidsendDoc4::onlyOne2One) {
							((CButton *)GetDescendantWindow(IDC_BUTTON5))->SetState(1);
							((CButton *)GetDescendantWindow(IDC_BUTTON5))->SetCheck(1);
							}
						else {
							((CButton *)GetDescendantWindow(IDC_BUTTON5))->SetState(0);
							((CButton *)GetDescendantWindow(IDC_BUTTON5))->SetCheck(0);
							}*/
					break;
				}
			break;
		case IDC_RICHEDIT1:
			switch(HIWORD(wParam)) {
//				case WM_DRAWITEM:		// no... questo arriva a parte!
//					break;
				}
			break;
		case IDC_LIST2:
			LPMEASUREITEMSTRUCT lpmis;

			switch(HIWORD(wParam)) {
//				case WM_MEASUREITEM:			// arriva a parte (v.sotto)
//					break;
//				case WM_DRAWITEM:		// no... questo arriva a parte!
//					break;
				case LBN_DBLCLK:
					i=((CListBox *)GetDescendantWindow(IDC_LIST2))->GetCaretIndex();
					wsprintf(myBuf,"\\\\*%u ",i+1);
					((CEdit *)GetDescendantWindow(IDC_EDIT1))->SetWindowText(myBuf);
					// prosegue
				case LBN_KILLFOCUS:
					HBITMAP hbmpPicture;
					for(i=0; i<=IDB_EMOT_126-IDB_EMOT_001; i++) {
						hbmpPicture=(HBITMAP)wl->SendMessage(LB_GETITEMDATA,i,0);
						DeleteObject(hbmpPicture);
						}
					wl->DestroyWindow();
					delete wl;
					wl=NULL;
					bDestroyed=TRUE;
					break;
				}
			break;
		case IDR_WAV_CONSTIPATION:
		case IDR_WAV_CREAK_DOOR:
		case IDR_WAV_GRUNT1:
		case IDR_WAV_GRUNT2:
		case IDR_WAV_KISS:
		case IDR_WAV_RISATA_GRASSA:
		case IDR_WAV_RODEO:
		case IDR_WAV_ROMANTIC_KISS:
		case IDR_WAV_SHOUT_CRY:
		case IDR_WAV_THROAT:
		case IDR_WAV_WOW_WHISTLE:
			wsprintf(myBuf,"\\\\#%u",LOWORD(wParam));
			((CEdit *)GetDescendantWindow(IDC_EDIT1))->SetWindowText(myBuf);
			break;
		}
	
		return !bDestroyed ? CFormView::OnCommand(wParam, lParam) : 1;
	}


void CVidsendView4::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDis) {
	HDC hDC=lpDis->hDC,hdcMem;
	RECT rc=lpDis->rcItem;
	RECT rcBitmap;
	char myBuf[128];
	int i,x,y,xBitmap,yBitmap;
	struct CHAT_MESSAGE *sc;
	SIZE size;
	HPEN pen1,*oldPen;
	HBRUSH brush1,*oldBrush;
	HBITMAP hbmpPicture,hbmpOld;
	LOGBRUSH lb;
	TEXTMETRIC tm;
	
	switch(nIDCtl) {
		case IDC_LIST1:
			sc=(struct CHAT_MESSAGE *)lpDis->itemData;
/*			if(sc) {
				pen1=CreatePen(PS_SOLID,1,lpDrawItemStruct->itemState & ODS_SELECTED ? RGB(96,96,128) : RGB(255,255,255));
				oldPen=(HPEN *)SelectObject(hDC,pen1);
				lb.lbStyle=BS_SOLID;
				lb.lbColor=sc->extra & 1 ? RGB(0,0,0) : RGB(255,255,255);
				brush1=CreateBrushIndirect(&lb);
				oldBrush=(HBRUSH *)SelectObject(hDC,brush1);
				x=20;
				DrawIcon(hDC,0,rc.top,sc->extra & 4 ? hIconComputer : (sc->extra & 2 ? hIconSupervisor : hIconUser));
				if(sc->extra & serverVideo) {
					DrawIcon(hDC,18,rc.top,hIconComputer);
					x=36;
					}
				SetTextColor(hDC,sc->color);
				Rectangle(hDC,x,rc.top,rc.right-1,rc.bottom-1);
				SetBkColor(hDC,sc->extra & 1 ? RGB(0,0,0) : RGB(255,255,255));
				_tcscpy(myBuf,sc->sender);
				strcat(myBuf,">");
				i=strlen(myBuf);
				TextOut(hDC,x+1,rc.top+1,myBuf,i);
				GetTextExtentPoint(hDC,myBuf,i,&size);
				TextOut(hDC,x+size.cx+5,rc.top+1,sc->message,strlen(sc->message));
				
//				wsprintf(myBuf,"iaction %X, istate %X",lpDrawItemStruct->itemAction,lpDrawItemStruct->itemState);
//				TextOut(hDC,x+150,rc.top+1,myBuf,strlen(myBuf));
				}
			SelectObject(hDC,oldPen);
			SelectObject(hDC,oldBrush);
			DeleteObject(pen1);
			DeleteObject(brush1);*/
			break;

		case IDC_LIST2:
//switch (lpDis->itemAction)
			hbmpPicture =(HBITMAP)((CListBox *)GetDescendantWindow(IDC_LIST2))->GetItemData(lpDis->itemID); 
 
      hdcMem = CreateCompatibleDC(hDC); 
      hbmpOld = (HBITMAP)SelectObject(hdcMem, hbmpPicture); 
 
			BITMAP bmp;
//			CBitmap cbmp
			(CBitmap::FromHandle(hbmpPicture))->GetBitmap(&bmp);
			xBitmap=bmp.bmWidth /*32*/;
			yBitmap=bmp.bmHeight;

      BitBlt(hDC, 
        lpDis->rcItem.left, lpDis->rcItem.top, 
        lpDis->rcItem.right - lpDis->rcItem.left, 
        lpDis->rcItem.bottom - lpDis->rcItem.top, 
        hdcMem, 0, 0, SRCCOPY); 
 
        // Display the text associated with the item. 

//      ::SendMessage(lpDis->hwndItem, LB_GETTEXT, lpDis->itemID, (LPARAM) myBuf); 
			// questo se ci fossero stringhe... HASSTRINGS
			wsprintf(myBuf,"%u",lpDis->itemID+1);

      GetTextMetrics(hDC, &tm); 

      y = (lpDis->rcItem.bottom + lpDis->rcItem.top - 
            tm.tmHeight) / 2; 

      TextOut(hDC, xBitmap + 6, y, 
        myBuf, strlen(myBuf)); 

      SelectObject(hdcMem, hbmpOld); 
      DeleteDC(hdcMem); 

        // Is the item selected? 

      if(lpDis->itemState & ODS_SELECTED) { 
         // Set RECT coordinates to surround only the bitmap. 

        rcBitmap.left = lpDis->rcItem.left; 
        rcBitmap.top = lpDis->rcItem.top; 
        rcBitmap.right = lpDis->rcItem.left + xBitmap; 
        rcBitmap.bottom = lpDis->rcItem.top + yBitmap; 

        // Draw a rectangle around bitmap to indicate the selection. 

        DrawFocusRect(hDC, &rcBitmap); 
				//FrameRect(hDC, &rcBitmap)		// + spesso?
        } 
			break;
//		case IDC_BUTTON5:
//			DrawIcon(hDC,0,rc.top,hIconOne2One);
//			break;
		}
	
	
//	CFormView::OnDrawItem(nIDCtl, lpDrawItemStruct);
	}

void CVidsendView4::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMis) {
	char myBuf[128];
	int i,x;
	SIZE size;
	HPEN pen1,*oldPen;
	HBRUSH brush1,*oldBrush;
	LOGBRUSH lb;
	
	switch(nIDCtl) {
		case IDC_LIST2:
			BITMAP bmp;
			HBITMAP hbmpPicture;

//			AfxMessageBox("loadim");

			hbmpPicture=(HBITMAP)::LoadImage(theApp.m_hInstance,MAKEINTRESOURCE(lpMis->itemData),IMAGE_BITMAP,0,0,LR_DEFAULTCOLOR);
			(CBitmap::FromHandle(hbmpPicture))->GetBitmap(&bmp);
			lpMis->itemWidth=bmp.bmWidth /*32*/;
			lpMis->itemHeight=bmp.bmHeight+2;
			GetDlgItem(IDC_LIST2)->SendMessage(LB_SETITEMDATA,lpMis->itemID,(LPARAM)hbmpPicture);
			break;
		}
	
	
//	CFormView::OnDrawItem(nIDCtl, lpDrawItemStruct);
	}

void CVidsendView4::OnContextMenu(CWnd* pWnd, CPoint point) {
	CMenu myMenu;
	
	if(pWnd==GetDlgItem(IDC_RICHEDIT1)) {
		myMenu.LoadMenu(IDR_VIDSENTYPE4);
		CMenu *myMenu2=myMenu.GetSubMenu(3);
		myMenu2->AppendMenu(MF_SEPARATOR);
#ifdef _LINGUA_INGLESE
		myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Properties...");
#else
		myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Proprietà...");
#endif
//		ClientToScreen(&point);
		myMenu2->TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, GetParent());
		}
	else if(pWnd==GetDlgItem(IDC_TREE1)) {
		myMenu.LoadMenu(IDR_VIDSENTYPE4);
		CMenu *myMenu2=myMenu.GetSubMenu(4);
		myMenu2->AppendMenu(MF_SEPARATOR);
#ifdef _LINGUA_INGLESE
		myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Properties...");
#else
		myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Proprietà...");
#endif
//		ClientToScreen(&point);
		myMenu2->TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, GetParent());
		}
	}

void CVidsendView4::OnDeleteItem(int nIDCtl, LPDELETEITEMSTRUCT lpDeleteItemStruct) {
	
	switch(nIDCtl) {
		case IDC_LIST1:
//			delete (struct CHAT_INFO *)lpDeleteItemStruct->itemData;
			break;
		}
	
	CFormView::OnDeleteItem(nIDCtl, lpDeleteItemStruct);
	}

int CVidsendView4::subAddToListBox(CRichEditCtrl *e,struct CHAT_MESSAGE *m,BOOL CRLF) {
	int i;
	DWORD n,n1;
//	CListBox *v=(CListBox *)GetDlgItem(IDC_LIST1);
	char myBuf[256];
	CHARFORMAT2 cf;


	/*	i=v->AddString(m->message);		//
		v->SetItemData(i,(DWORD)m);
		v->SetCurSel(i);
		if(v->GetCount() >= GetDocument()->maxMessaggi)
			v->DeleteString(0);*/
	ZeroMemory(&cf,sizeof(CHARFORMAT2));
	cf.cbSize=sizeof(CHARFORMAT2);
	cf.dwMask=CFM_BACKCOLOR | CFM_COLOR | CFM_UNDERLINE | CFM_BOLD;
	cf.dwEffects=/*m->extra & */ 1 ? CFE_BOLD : 0;
	cf.crTextColor=m->extra & 1 ? 0 : m->color;
	cf.crBackColor=m->extra & 1 ? RGB(0,0,0) : RGB(255,255,255);  // non va??

	n1=e->GetWindowTextLength();
	e->SetSel(n1,n1);

	i=e->SetSelectionCharFormat(cf);
	if(!i)
		AfxMessageBox("impossibile applicare stile");

	_tcscpy(myBuf,m->sender);
	strcat(myBuf,"> ");
	strcat(myBuf,m->message);
	if(CRLF)
		strcat(myBuf,"\xd\xa");
	else
		strcat(myBuf," ");
	n=strlen(m->message);
	if(n1+n >= 10000) {
		e->SetSel(0,A_LINE_OF_TEXT /*,TRUE*/);
		e->ReplaceSel("");		// Clear non funziona su Edit READ_ONLY!
//			e->SetSel(-1,-1,0);
		n1=e->GetWindowTextLength();
		e->SendMessage(EM_SETSEL /*EM_EXSETSEL*/, n1,n1);		// -1,-1 non funziona!
		i=e->SetSelectionCharFormat(cf);			// RI-APPLICO!
//		if(!i)
//			AfxMessageBox("impossibile applicare stile");
		}
	e->ReplaceSel(myBuf);
//	e->LineScroll(1);
	e->SendMessage(WM_VSCROLL, SB_LINEDOWN, 0);


	return 1;
	}

void CVidsendView4::addToListBox(struct CHAT_MESSAGE *m) {
	CVidsendDoc4 *pDoc = GetDocument();
	int i;
	DWORD n,n1;
//	CListBox *v=(CListBox *)GetDlgItem(IDC_LIST1);
	CRichEditCtrl *e=(CRichEditCtrl *)GetDlgItem(IDC_RICHEDIT1);
	char myBuf[256],filetemp[MAX_PATH],filetempPath[256];

	if(m->message[0] == '\\' && m->message[1] == '\\') {		// gestisco suoni, icone
		if(m->message[2] == '#') {
			_tcscpy(myBuf,m->message+3);
//			strcat(myBuf,".wav");
			if(pDoc->opzioniVisive & CVidsendDoc4::avvisi_sonori2)
//				i=PlaySound("C:\\creak_door.wav",NULL,SND_ASYNC | SND_FILENAME);
				i=PlaySound(MAKEINTRESOURCE(atoi(myBuf)),theApp.m_hInstance,SND_ASYNC | SND_RESOURCE);
			_tcscpy(m->message,"(suona)");
			subAddToListBox(e,m);
			}
		else if(m->message[2] == '*') {
			i=atoi(m->message+3);
			if(i>0) {
				*m->message=0;
				subAddToListBox(e,m,0);
				i--;

				if(OpenClipboard()) {
					HBITMAP hbmpPicture;
					BITMAP bmp;
					DWORD l;
					HANDLE h;
					BYTE *p;
					hbmpPicture=(HBITMAP)::LoadImage(theApp.m_hInstance,MAKEINTRESOURCE(i+IDB_EMOT_001),IMAGE_BITMAP,0,0,LR_DEFAULTCOLOR);
					(CBitmap::FromHandle(hbmpPicture))->GetBitmap(&bmp);
					l=bmp.bmWidth*bmp.bmHeight*3;		// per eccesso va bene...
 					h=GlobalAlloc(GMEM_MOVEABLE,l);
					p=(BYTE *)GlobalLock(h);
					i=GetBitmapBits(hbmpPicture,l,p);
					GlobalUnlock(h);
					EmptyClipboard();
					SetClipboardData(CF_BITMAP,hbmpPicture);
					CloseClipboard();
//					e->SetEnabled(FALSE);		// per RichEdit
//					i=e->GetCanPaste();			// per TXOle


//					e->Paste(CF_BITMAP /*CF_RTFTEXT*/);
//					e->PasteSpecial(CF_BITMAP);
					e->Paste();
//					e->SetReadOnly(TRUE);
					DeleteObject(hbmpPicture);
					} 



					

// **************************************************************************
//  EMBEDDING METAFILE and RTF PICTURE EXAMPLE
// **************************************************************************

//    AUTHOR: The Hand
//      DATE: June, 2002
//   COMPANY: EliteVB

// DESCRIPTION:
//    This example shows the user how to embed a picture (StdPicture object
//    into a metafile, and subsequently create usable RTF code so it can be
//    placed in a rich text box.
//
//    Forget those horribly cheesy Clipboard and OLEObject.Add methods and
//    use this method instead!
//
//    Feel free to use this source in your own projects. You are not allowed
//    to take credit for it, publish it, or wave it around on PSC trying
//    to win a prize without prior consent from the EliteVB team. And just
//    so you know, a few global "replace alls" does not make it 'YOUR' source
//    code. It just makes you a serious lamer, and a very sad individual
//    with little creativity. Give us credit where its due.
//
// **************************************************************************
//   Visit EliteVB.com for more high-powered API and subclassing solutions!
// **************************************************************************


					/*
Private Type Size
    cx As Long
    cy As Long
End Type

Private Type POINTAPI
    x As Long
    y As Long
End Type

Private Type BITMAP
    bmType As Long
    bmWidth As Long
    bmHeight As Long
    bmWidthBytes As Long
    bmPlanes As Integer
    bmBitsPixel As Integer
    bmBits As Long
End Type

'Private Type METAHEADER
'    mtType As Integer
'    mtHeaderSize As Integer
'    mtVersion As Integer
'    mtSize As Long
'    mtNoObjects As Integer
'    mtMaxRecord As Long
'    mtNoParameters As Integer
'End Type

' Used for creating the temporary WMF file
Private Declare Function GetTempPath Lib "kernel32" Alias "GetTempPathA" (ByVal nBufferLength As Long, ByVal lpBuffer As String) As Long

Private Const MM_ANISOTROPIC = 8 ' Map mode anisotropic

Private Function StdPicAsRTF(aStdPic As StdPicture) As String

    ' ***********************************************************************
    '  Author: The Hand
    '    Date: June, 2002
    ' Company: EliteVB
    '
    '  Function: StdPicAsRTF
    ' Arguments: aStdPic - Any standard picture object from memory, a
    '                      picturebox, or other source.
    '
    ' Description:
    '    Embeds a standard picture object in a windows metafile and returns
    '    rich text format code (RTF) so it can be placed in a RichTextBox.
    '    Useful for emoticons in chat programs, pics, etc. Currently does
    '    not support icon files, but that is easy enough to add in.
    ' ***********************************************************************
    Dim hMetaDC     As Long
    Dim hMeta       As Long
    Dim hPicDC      As Long
    Dim hOldBmp     As Long
    Dim aBMP        As BITMAP
    Dim aSize       As Size
    Dim aPt         As POINTAPI
    Dim fileName    As String
'    Dim aMetaHdr    As METAHEADER
    Dim screenDC    As Long
    Dim headerStr   As String
    Dim retStr      As String
    Dim byteStr     As String
    Dim bytes()     As Byte
    Dim filenum     As Integer
    Dim numBytes    As Long
    Dim i           As Long
    
    ' Create a metafile to a temporary file in the registered windows TEMP folder
    fileName = getTempName("WMF")
    hMetaDC = CreateMetaFile(fileName)
    
    ' Set the map mode to MM_ANISOTROPIC
    SetMapMode hMetaDC, MM_ANISOTROPIC
    ' Set the metafile origin as 0, 0
    SetWindowOrgEx hMetaDC, 0, 0, aPt
    ' Get the bitmap's dimensions
    GetObject aStdPic.Handle, Len(aBMP), aBMP
    ' Set the metafile width and height
    SetWindowExtEx hMetaDC, aBMP.bmWidth, aBMP.bmHeight, aSize
    ' save the new dimensions
    SaveDC hMetaDC
    ' OK. Now transfer the freakin image to the metafile
    screenDC = GetDC(0)
    hPicDC = CreateCompatibleDC(screenDC)
    ReleaseDC 0, screenDC
    hOldBmp = SelectObject(hPicDC, aStdPic.Handle)
    BitBlt hMetaDC, 0, 0, aBMP.bmWidth, aBMP.bmHeight, hPicDC, 0, 0, vbSrcCopy
    SelectObject hPicDC, hOldBmp
    DeleteDC hPicDC
    DeleteObject hOldBmp
    ' "redraw" the metafile DC
    RestoreDC hMetaDC, True
    ' close it and get the metafile handle
    hMeta = CloseMetaFile(hMetaDC)
    
'    GetObject hMeta, Len(aMetaHdr), aMetaHdr
    ' delete it from memory
    DeleteMetaFile hMeta
    
    ' Do the RTF header for the object. This little bit is sometimes required on
    '  earlier versions of the rich text box and in certain operating systems
    '  (WinNT springs to mind)
    headerStr = "{\rtf1\ansi"
    ' Picture specific tag stuff
    headerStr = headerStr & _
                "{\pict\picscalex100\picscaley100" & _
                "\picw" & aStdPic.Width & "\pich" & aStdPic.Height & _
                "\picwgoal" & aBMP.bmWidth * Screen.TwipsPerPixelX & _
                "\pichgoal" & aBMP.bmHeight * Screen.TwipsPerPixelY & _
                "\wmetafile8"
    
    ' Get the size of the metafile
    numBytes = FileLen(fileName)
    ' Create our byte buffer for reading
    ReDim bytes(1 To numBytes)
    ' get a free file number
    filenum = FreeFile()
    ' open the file for input
    Open fileName For Binary Access Read As #filenum
    ' read the bytes
    Get #filenum, , bytes
    ' close the file
    Close #filenum
    ' Generate our hex encoded byte string
    byteStr = String(numBytes * 2, "0")
    For i = LBound(bytes) To UBound(bytes)
        If bytes(i) > &HF Then
            Mid$(byteStr, 1 + (i - 1) * 2, 2) = Hex$(bytes(i))
        Else
            Mid$(byteStr, 2 + (i - 1) * 2, 1) = Hex$(bytes(i))
        End If
    Next i
    ' stick it all together
    retStr = headerStr & " " & byteStr & "}"
    ' Add in the closing RTF bit
    retStr = retStr & "}"
        
    StdPicAsRTF = retStr
    On Local Error Resume Next
    ' Kill the temporary file
    If Dir(fileName) <> "" Then Kill fileName
End Function
Private Function getTempName(Optional anExt As String = "tmp") As String
    ' ***********************************************************************
    '  Author: The Hand
    '    Date: June, 2002
    ' Company: EliteVB
    '
    '  Function: getTempName
    ' Arguments: anExt - an extension to be used for the temp file. If none
    '                    is provided, the function automatically uses "tmp"
    '                    as the extension. It is up to the procedure that
    '                    uses this temporary name to clean up the file (kill
    '                    it) after it is created.
    '
    ' Description:
    '    Creates a temporary filename in the registered system temp directory
    ' ***********************************************************************
    Dim tempPath    As String
    Dim fileName    As String
    Dim i           As Long
    
    Const validChars As String = "123567890qwertyuiopasdfghjklzxcvbnm"
    
    ' Create a buffer
    tempPath = String$(255, " ")
    ' get the system path
    GetTempPath 255, tempPath
    ' trim off the fat
    tempPath = Left$(tempPath, InStr(tempPath, Chr$(0)) - 1)
    ' Create a buffer
    fileName = Space(12)
    ' Put the non-random stuff into the string
    Mid$(fileName, 1, 1) = "T"
    Mid$(fileName, Len(fileName) - Len(anExt), 1) = "."
    ' Add in the specified extension, if provided ("tmp" is default)
    Mid$(fileName, Len(fileName) - Len(anExt) + 1, Len(anExt)) = anExt
    ' fill the buffer with random stuff
    Randomize
    For i = 2 To Len(fileName) - 4
        Mid$(fileName, i, 1) = Mid$(validChars, CLng(Rnd() * (Len(validChars)) + 1), 1)
    Next i
    tempPath = tempPath & fileName
    ' return the path name
    getTempName = tempPath
    
End Function
Private Sub Command1_Click()
    Dim aStr As String
    aStr = StdPicAsRTF(Picture1.Picture)
    RichTextBox1.SelRTF = aStr
End Sub

Private Sub Form_QueryUnload(Cancel As Integer, UnloadMode As Integer)
    RichTextBox1.Text = ""
End Sub

Private Sub Form_Resize()
    Command1.Move 0, Me.ScaleHeight - Command1.Height, Me.ScaleWidth
    Picture1.Move 0, 0, Me.ScaleWidth / 2, Me.ScaleHeight - Command1.Height
    RichTextBox1.Move Me.ScaleWidth / 2, 0, Me.ScaleWidth / 2, Me.ScaleHeight - Command1.Height
End Sub
*/

					
					
					
					e->ReplaceSel("\xd\xa");

				}
			}
		else {
			m->message[0] = m->message[1] = ' ';
			subAddToListBox(e,m);
			}
		}
	else
		subAddToListBox(e,m);


	delete m;

	}



int CVidsendView4::updateTree() {
	int i,j,n,oldImg;
	CTreeCtrl *v=(CTreeCtrl *)GetDlgItem(IDC_TREE1);
	HTREEITEM tp0,tp1;
	CString aIP,oldSel,S;
	UINT aPort;
	CChatSrvSocket2 *myRoot;

	if(tp0=v->GetSelectedItem()) {
		oldSel=v->GetItemText(tp0);
		v->GetItemImage(tp0,oldImg,i);
		}
	v->DeleteAllItems();
	tp0=v->InsertItem("Utenti collegati");
	v->SetImageList(&il,TVSIL_NORMAL);
	v->SetItemImage(tp0,0,0);

	POSITION po,po1;

	po=theApp.theChat->chatSocket.cSockRoot.GetHeadPosition();
	if(po) {
		do {
			po1=po;
			myRoot=theApp.theChat->chatSocket.cSockRoot.GetNext(po);
			if(myRoot->m_hSocket != INVALID_SOCKET) {
				myRoot->GetPeerName(aIP,aPort);
				S=myRoot->connName+" ("+aIP+")";
				tp1=v->InsertItem(S,tp0);
				v->SetItemImage(tp1,4,4);
				v->SetItemData(tp1,(DWORD)myRoot);
				}
			} while(po);
		}
	
	v->Expand(tp0,TVE_EXPAND);
	if(!oldSel.IsEmpty()) {
		tp1=v->GetChildItem(tp0);
		while(tp1) {
			int l1,l2;
			S=v->GetItemText(tp1);
			v->GetItemImage(tp1,l1,l2);
			if(S == oldSel && l1 == oldImg) {
				v->SelectItem(tp1/*,TVGN_CARET | TVGN_FIRSTVISIBLE*/);
				break;
				}
			tp1=v->GetNextSiblingItem(tp1);
			}
		}
	
	return 1;
	}


void CVidsendView4::OnUtentiConnettivideo() {
	// TODO: Add your command handler code here
	}

void CVidsendView4::OnUpdateUtentiConnettivideo(CCmdUI* pCmdUI) {
	HTREEITEM h=((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetSelectedItem();

	pCmdUI->Enable(GetDocument()->Opzioni & CVidsendDoc4::serverMode && h 
		&& ((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetItemData(h) != NULL);
	}

void CVidsendView4::OnUtentiMandaemail() {
	HTREEITEM tp=((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetSelectedItem();
	CChatSrvSocket2 *c;
	CString S;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	if(tp) {
		c=(CChatSrvSocket2 *)((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetItemData(tp);
		if(c) {
			S="mailto:";
//			S+=c->emailAddress;
			CreateProcess("c:\\start",(char *)(LPCTSTR)S,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);
			}
		}
	}

void CVidsendView4::OnUpdateUtentiMandaemail(CCmdUI* pCmdUI) {
	HTREEITEM h=((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetSelectedItem();

	pCmdUI->Enable(GetDocument()->Opzioni & CVidsendDoc4::serverMode && h 
		&& ((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetItemData(h) != NULL);
	}

void CVidsendView4::OnUtenteDisconnetti() {
	HTREEITEM tp=((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetSelectedItem();
	CChatSrvSocket2 *c;

	if(tp) {
		c=(CChatSrvSocket2 *)((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetItemData(tp);
		if(c) {
			if(AfxMessageBox("Disconnettere?",MB_ICONQUESTION | MB_YESNO) == IDYES) {
				// bisognerebbe mandare un msg per notificare che e' una "disconnessione" e non la chiusura del server...
				c->Close();
				}
			}
		}
	}

void CVidsendView4::OnUpdateUtenteDisconnetti(CCmdUI* pCmdUI) {
	HTREEITEM h=((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetSelectedItem();

	pCmdUI->Enable(GetDocument()->Opzioni & CVidsendDoc4::serverMode && h 
		&& ((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetItemData(h) != NULL);
	}

void CVidsendView4::OnUtenteInseriscinegliindesiderati() {
	CVidsendDoc4 *pDoc = GetDocument();
	int i;
	CStringList bl;
	CString S,S1;
	CChatSrvSocket2 *c;
	char myBuf[128];

	i=pDoc->loadBlacklistedIP(&bl);
//	if(i<CVidsendDoc4::BLACKLIST_SIZE) {
		HTREEITEM h=((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetSelectedItem();
		S=((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetItemText(h);
		i=S.ReverseFind('(');	
		if(i != -1) {					// gestivo nome + IP o solo IP
			S=S.Mid(i+1);
			if(S.Right(1) == ')')
				S=S.Left(S.GetLength()-1);
			}

		if(S == CWebSrvSocket2_base::getMyOutmostIPAddress(myBuf))
			AfxMessageBox("Attenzione: state inserendo il vostro computer tra gli indesiderati!",MB_ICONSTOP);
		c=(CChatSrvSocket2 *)((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetItemData(h);
		S1.Format("%u",c->serNum);
		if(!bl.Find(S1)) {
			bl.AddTail(S1);
			pDoc->saveBlacklistedIP(&bl);
			if(AfxMessageBox("Utente inserito!\nSi desidera disconnetterlo ora?",MB_YESNO) == IDYES) {
				c->Close();
				}
			}
		else
			AfxMessageBox("L'utente è già segnato!",MB_ICONEXCLAMATION);
//		}
//	else {
//		AfxMessageBox("La lista degli utenti indesiderati è piena!");
//		}
	}

void CVidsendView4::OnUpdateUtenteInseriscinegliindesiderati(CCmdUI* pCmdUI) {
	HTREEITEM h=((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetSelectedItem();

	pCmdUI->Enable(GetDocument()->Opzioni & CVidsendDoc4::serverMode && h 
		&& ((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetItemData(h) != NULL);
	}

void CVidsendView4::OnUtenteMandamessaggio() {
	// TODO: Add your command handler code here
	}

void CVidsendView4::OnUpdateUtenteMandamessaggio(CCmdUI* pCmdUI) {
	HTREEITEM h=((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetSelectedItem();

	pCmdUI->Enable(GetDocument()->Opzioni & CVidsendDoc4::noPrivateMsg ? 0 : 1 && h 
		&& ((CTreeCtrl *)GetDlgItem(IDC_TREE1))->GetItemData(h) != NULL);
	}

void CVidsendView4::OnUtentiMandamessaggiopertutti() {
	struct CHAT_MESSAGE m;
	CString S;

	_tcscpy(m.message,"non ancora implementato!");
	m.id=2;
	m.color=GetDocument()->opzioniVisive & 0xffffff;
	m.extra=CVidsendView4::supervisorMsg | CVidsendView4::reverseMsg;
	_tcscpy(m.sender,"<server");		// bah...
	GetDocument()->chatSocket.Manda((char *)&m,sizeof(struct CHAT_MESSAGE));
	}

void CVidsendView4::OnUpdateUtentiMandamessaggiopertutti(CCmdUI* pCmdUI) {
	pCmdUI->Enable(GetDocument()->Opzioni & CVidsendDoc4::serverMode);
	}




void CVidsendView4::OnMessaggioCancella() {
	int i=((CListBox *)GetDlgItem(IDC_RICHEDIT1))->GetCurSel();
	if(i != LB_ERR)
		((CListBox *)GetDlgItem(IDC_RICHEDIT1))->DeleteString(i);
	}

void CVidsendView4::OnUpdateMessaggioCancella(CCmdUI* pCmdUI) {
	pCmdUI->Enable(((CListBox *)GetDlgItem(IDC_RICHEDIT1))->GetCurSel() != LB_ERR);
	}

void CVidsendView4::OnMessaggioCancellatutto() {
	((CRichEditCtrl *)GetDlgItem(IDC_RICHEDIT1))->SetSel(0,-1);
	((CRichEditCtrl *)GetDlgItem(IDC_RICHEDIT1))->ReplaceSel("");		// Clear non funziona su Edit READ_ONLY!
//	((CListBox *)GetDlgItem(IDC_RICHEDIT1))->ResetContent();		era lista...
	}

void CVidsendView4::OnUpdateMessaggioCancellatutto(CCmdUI* pCmdUI) {
//	pCmdUI->Enable(((CListBox *)GetDlgItem(IDC_RICHEDIT1))->GetCount() >= 1);
	pCmdUI->Enable(TRUE);
	}

void CVidsendView4::OnMessaggioInvia() {
	char myBuf[256];

	((CEdit *)GetDlgItem(IDC_EDIT1))->GetWindowText(myBuf+1,200);
	if(*(myBuf+1)) {			// mettere un timeout minimo tra i msg, o impedire di ri-spedire lo stesso + volte
		*myBuf=2;
		cliSock->Send(myBuf,201);
		}
	else
		MessageBeep(MB_ICONASTERISK);
	((CEdit *)GetDlgItem(IDC_EDIT1))->SetSel(0,-1);
	((CEdit *)GetDlgItem(IDC_EDIT1))->Clear();
	}

void CVidsendView4::OnUpdateMessaggioInvia(CCmdUI* pCmdUI) {
	pCmdUI->Enable(cliSock && cliSock->m_hSocket != INVALID_SOCKET);
	}




DWORD CALLBACK EditStreamCallback(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb) {
	
//			ASSERT(0);
	WriteFile((HANDLE)dwCookie, pbBuff, cb, (LPDWORD)pcb, NULL);
	if(*pcb < cb)
		return 0; // file has been fully read in
	else
		return 0 /*(DWORD)*pcb cosi' non va... lo prende come errore (sembra!) */; // more to read
	} 

void CVidsendView4::OnFileSave() {
	EDITSTREAM eStream;
	HFILE hFile;
	OFSTRUCT of;
	CRichEditCtrl *e=(CRichEditCtrl *)GetDlgItem(IDC_RICHEDIT1);
	CFileDialog myDlg(FALSE,"*.txt",NULL,OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,"File di testo (*.txt)|*.txt|Tutti i file (*.*)|*.*||",this);
	CString S;
	
	if(myDlg.DoModal() == IDOK) {
		S=myDlg.GetFileName();
		if(hFile = OpenFile(S, &of, OF_WRITE | OF_CREATE)) {
			// dwCookie is an app-defined value that holds the handle to the file.
			eStream.dwCookie = hFile;
			eStream.pfnCallback = EditStreamCallback;
			eStream.dwError = 0;
			int iAttrib=SF_RTF;
			e->SendMessage(EM_STREAMOUT, (WPARAM)iAttrib, (LPARAM)&eStream);
			CloseHandle((HANDLE)hFile);
			}
		else
			AfxMessageBox("Impossibile salvare il testo!",MB_ICONSTOP);
		}
	}

void CVidsendView4::OnUpdateFileSave(CCmdUI* pCmdUI) {	

	pCmdUI->Enable(!chatInfo.dontSave && 
		((CRichEditCtrl *)GetDlgItem(IDC_RICHEDIT1))->GetLineCount() >= 1);
	}

void CVidsendView4::OnFileSaveAll() {
	
	OnFileSave();	
	}

void CVidsendView4::OnUpdateFileSaveAll(CCmdUI* pCmdUI) {	

	pCmdUI->Enable(!chatInfo.dontSave && 
		((CRichEditCtrl *)GetDlgItem(IDC_RICHEDIT1))->GetLineCount() >=1);
	}

void CVidsendView4::OnCopiaChat() {
	CString S;
	HANDLE h;
	char *p;

	((CRichEditCtrl *)GetDlgItem(IDC_RICHEDIT1))->GetWindowText(S);
	if(OpenClipboard()) {
 		h=GlobalAlloc(GMEM_MOVEABLE,S.GetLength()+1);
		p=(char *)GlobalLock(h);
		_tcscpy(p,(LPCTSTR)S);
		GlobalUnlock(h);
	  EmptyClipboard();
	  SetClipboardData(CF_TEXT,h);
	  CloseClipboard();
	  }  

	}

void CVidsendView4::OnUpdateCopiaChat(CCmdUI* pCmdUI) {
	CVidsendDoc4 *d=GetDocument();

	if(d) {
		pCmdUI->Enable(d->Opzioni & CVidsendDoc4::dontSave &&
			!chatInfo.dontSave && 
			((CRichEditCtrl *)GetDlgItem(IDC_RICHEDIT1))->GetLineCount() >=1);
		}
	}

void CVidsendView4::OnDestroy() {
	CVidsendDoc4 *d=GetDocument();
	RECT rc;
	char myBuf[64];
	
	CFormView::OnDestroy();
	
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		GetParent()->GetWindowRect(&rc);	// coord. schermo della mia MDIChildFrmae...
		GetParent()->GetParent()->ScreenToClient(&rc);	// ... diventano coord. client rispetto al MDI padre (che NON e' theApp.m_pMainWnd !! ce n'è una un mezzo...
		wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
		d->WritePrivateProfileString(IDS_COORDINATE,myBuf);
		}
	}



void CVidsendView4::OnRclick(NMHDR* pNMHDR, LRESULT* pResult) {
	CMenu myMenu;
	CPoint point;
	
//NON FUNZIONA!!! fanculo a Bill

	point.x=100;
	point.y=100;
	ASSERT(0);
	if(pNMHDR->idFrom==IDC_RICHEDIT1) {
		myMenu.LoadMenu(IDR_VIDSENTYPE4);
		CMenu *myMenu2=myMenu.GetSubMenu(3);
		myMenu2->AppendMenu(MF_SEPARATOR);
#ifdef _LINGUA_INGLESE
		myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Properties...");
#else
		myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Proprietà...");
#endif
//		ClientToScreen(&point);
		myMenu2->TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, GetParent());
		}
	
	*pResult = 0;
	}




/////////////////////////////////////////////////////////////////////////////
// CVidsendView5 - client HTML

IMPLEMENT_DYNCREATE(CVidsendView5, CHtmlView)

CVidsendView5::CVidsendView5() {
	//{{AFX_DATA_INIT(CVidsendView5)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	}

CVidsendView5::~CVidsendView5() {
	}

void CVidsendView5::DoDataExchange(CDataExchange* pDX) {
	
	CHtmlView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVidsendView5)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
	}


BEGIN_MESSAGE_MAP(CVidsendView5, CHtmlView)
	//{{AFX_MSG_MAP(CVidsendView5)
	ON_COMMAND(ID_NAVIGAZIONE_AVANTI, OnNavigazioneAvanti)
	ON_COMMAND(ID_NAVIGAZIONE_INDIETRO, OnNavigazioneIndietro)
	ON_UPDATE_COMMAND_UI(ID_NAVIGAZIONE_INDIETRO, OnUpdateNavigazioneIndietro)
	ON_UPDATE_COMMAND_UI(ID_NAVIGAZIONE_AVANTI, OnUpdateNavigazioneAvanti)
	ON_COMMAND(ID_NAVIGAZIONE_PAGINAINIZIALE, OnNavigazionePaginainiziale)
	ON_COMMAND(ID_NAVIGAZIONE_VAI, OnNavigazioneVai)
	ON_COMMAND(ID_VISUALIZZA_AGGIORNA, OnNavigazioneAggiorna)
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_NAVIGAZIONE_CERCA, OnNavigazioneCerca)
	ON_COMMAND(ID_NAVIGAZIONE_INTERROMPI, OnNavigazioneInterrompi)
	ON_UPDATE_COMMAND_UI(ID_NAVIGAZIONE_INTERROMPI, OnUpdateNavigazioneInterrompi)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendView5 diagnostics

#ifdef _DEBUG
void CVidsendView5::AssertValid() const
{
	CHtmlView::AssertValid();
}

void CVidsendView5::Dump(CDumpContext& dc) const
{
	CHtmlView::Dump(dc);
}
CVidsendDoc5* CVidsendView5::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVidsendDoc5)));
	return (CVidsendDoc5 *)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendView5 message handlers

BOOL CVidsendView5::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) {
	int i;
	
	if(i=CHtmlView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext)) {
		return i;
		}
	return 0;
	}

void CVidsendView5::OnInitialUpdate() {
	CString prevURL[CVidsendDoc::MRU_SIZE];
	int i;

	CHtmlView::OnInitialUpdate();

	GetDocument()->loadCliConnRecent(prevURL);
		// nella create della View non va bene, perche' la DialogBar non esiste ancora (6/9/00)
	for(i=0; i<CVidsendDoc5::MRU_SIZE; i++) {
		if(*prevURL[i])
			((CChildFrame5 *)GetParent())->m_wndDlgBar.SendDlgItemMessage(IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)(LPCTSTR)prevURL[i]);
		}

	((CChildFrame5 *)GetParent())->m_wndDlgBar.SetDlgItemText(IDC_COMBO1,GetDocument()->URL);
	OnNavigazioneVai();
	}

void CVidsendView5::OnNavigazioneVai() {
	
//	dBar.UpdateData();
	((CChildFrame5 *)GetParent())->m_wndDlgBar.GetDlgItemText(IDC_COMBO1,GetDocument()->URL);
	GetDocument()->save();
	Navigate(GetDocument()->URL);
	}

void CVidsendView5::OnTitleChange(LPCTSTR lpszText) {
	
	CHtmlView::OnTitleChange(lpszText);
	GetDocument()->SetTitle(lpszText);
	}

void CVidsendView5::OnStatusTextChange(LPCTSTR lpszText) {
	
	//CHtmlView::OnStatusTextChange(lpszText);		// evito che aggiorni la status bar globale
	}

void CVidsendView5::OnProgressChange(long nProgress, long nProgressMax) {
	CString S;
	
	if(nProgressMax) {
#ifdef _LINGUA_INGLESE
		S.Format("Loading... (%u%%)",(nProgress*100)/nProgressMax);
#else
		S.Format("Caricamento in corso... (%u%%)",(nProgress*100)/nProgressMax);
#endif
		((CChildFrame5 *)GetParent())->m_wndStatusBar.SetPaneText(2,S);
		}
	else
		((CChildFrame5 *)GetParent())->m_wndStatusBar.SetPaneText(2,"Operazione completata");
	CHtmlView::OnProgressChange(nProgress, nProgressMax);
	}

void CVidsendView5::OnNewWindow2(LPDISPATCH* ppDisp, BOOL* Cancel) {
	
	
	CHtmlView::OnNewWindow2(ppDisp, Cancel);
	}
void CVidsendView5::OnBeforeNavigate2(LPCTSTR lpszURL, DWORD nFlags, LPCTSTR lpszTargetFrameName, CByteArray& baPostedData, LPCTSTR lpszHeaders, BOOL* pbCancel) {
	
	if(lpszTargetFrameName)
		lpszTargetFrameName=NULL;			// voglio impedire che per aprire in un'altra window, apra un Explorer al di fuori dell'applicazione...
	CHtmlView::OnBeforeNavigate2(lpszURL, nFlags,	lpszTargetFrameName, baPostedData, lpszHeaders, pbCancel);
	}

void CVidsendView5::OnDownloadBegin() {
	CString S;
	
#ifdef _LINGUA_INGLESE
	S.Format("Opening %s...",GetDocument()->URL);
#else
	S.Format("Apertura %s in corso...",GetDocument()->URL);
#endif
	((CChildFrame5 *)GetParent())->m_wndStatusBar.SetPaneText(2,S);
	CHtmlView::OnDownloadBegin();
	}

void CVidsendView5::OnDownloadComplete() {
	
	((CChildFrame5 *)GetParent())->m_wndStatusBar.SetPaneText(2,"Operazione completata");
	CHtmlView::OnDownloadComplete();
	}

void CVidsendView5::OnNavigazioneAggiorna() {
	
	Refresh();
	}


void CVidsendView5::OnNavigazioneAvanti() {
	
	GoForward();
	((CChildFrame5 *)GetParent())->m_wndDlgBar.SetDlgItemText(IDC_COMBO1,GetLocationURL());
	}

void CVidsendView5::OnNavigazioneIndietro() {
	
	GoBack();
	((CChildFrame5 *)GetParent())->m_wndDlgBar.SetDlgItemText(IDC_COMBO1,GetLocationURL());
	}

void CVidsendView5::OnUpdateNavigazioneIndietro(CCmdUI* pCmdUI) {
		
	}

void CVidsendView5::OnUpdateNavigazioneAvanti(CCmdUI* pCmdUI) {
	
	}

void CVidsendView5::OnNavigazionePaginainiziale() {
	
	Navigate(GetDocument()->home);
	}


void CVidsendView5::OnNavigazioneCerca() {

	Navigate("http://www.altavista.com");		// fare di meglio!
	// anche GoSearch();
	}


void CVidsendView5::OnNavigazioneInterrompi() {
	
	Stop();
	}

void CVidsendView5::OnUpdateNavigazioneInterrompi(CCmdUI* pCmdUI) {
//	int i=GetReadyState();
//	pCmdUI->Enable(i==READYSTATE_LOADING || i==READYSTATE_LOADED || i==READYSTATE_INTERACTIVE);
	pCmdUI->Enable(GetBusy());
	}

void CVidsendView5::OnRButtonDown(UINT nFlags, CPoint point) {
	// TODO: Add your message handler code here and/or call default
	
	CHtmlView::OnRButtonDown(nFlags, point);
}

void CVidsendView5::OnDestroy() {
	CVidsendDoc5 *d=GetDocument();
	RECT rc;
	char myBuf[64];
	
	CHtmlView::OnDestroy();
	
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		GetParent()->GetWindowRect(&rc);	// coord. schermo della mia MDIChildFrmae...
		GetParent()->GetParent()->ScreenToClient(&rc);	// ... diventano coord. client rispetto al MDI padre (che NON e' theApp.m_pMainWnd !! ce n'è una un mezzo...
		wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
		d->WritePrivateProfileString(IDS_COORDINATE,myBuf);
		}
	}





/////////////////////////////////////////////////////////////////////////////
// CVidsendView6 - visualizzazione accessi

IMPLEMENT_DYNCREATE(CVidsendView6, CTreeView)

CVidsendView6::CVidsendView6() {

	il.Create(IDB_CONNECTIONS,16,0,RGB(255,255,255));
	}

CVidsendView6::~CVidsendView6() {
	}


BEGIN_MESSAGE_MAP(CVidsendView6, CTreeView)
	//{{AFX_MSG_MAP(CVidsendView6)
	ON_WM_TIMER()
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_VISUALIZZA_AGGIORNA, OnVisualizzaAggiorna)
	ON_WM_DESTROY()
	ON_COMMAND(ID_CONNESSIONI_DISCONNETTI, OnConnessioniDisconnetti)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONI_DISCONNETTI, OnUpdateConnessioniDisconnetti)
	ON_COMMAND(ID_CONNESSIONI_INSERISCINEGLIINDESIDERATI, OnConnessioniInseriscinegliindesiderati)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONI_INSERISCINEGLIINDESIDERATI, OnUpdateConnessioniInseriscinegliindesiderati)
	ON_COMMAND(ID_CONNESSIONI_MANDAMESSAGGIO, OnConnessioniMandamessaggio)
	ON_UPDATE_COMMAND_UI(ID_CONNESSIONI_MANDAMESSAGGIO, OnUpdateConnessioniMandamessaggio)
	ON_COMMAND(ID_CONNESSIONI_MANDAMESSAGGIOATUTTI, OnConnessioniMandamessaggioatutti)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendView6 drawing

void CVidsendView6::OnDraw(CDC* pDC){
	
	CVidsendDoc6 *pDoc = GetDocument();
	// TODO: add draw code here
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendView6 diagnostics

#ifdef _DEBUG
void CVidsendView6::AssertValid() const
{
	CTreeView::AssertValid();
}

void CVidsendView6::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}
CVidsendDoc6 *CVidsendView6::GetDocument() { // non-debug version is inline

	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVidsendDoc6)));
	return (CVidsendDoc6 *)m_pDocument;
	}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendView6 message handlers

void CVidsendView6::OnDestroy() {
	CVidsendDoc6 *d=GetDocument();
	RECT rc;
	char myBuf[64];
	
	CTreeView::OnDestroy();
	
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		GetParent()->GetWindowRect(&rc);	// coord. schermo della mia MDIChildFrmae...
		GetParent()->GetParent()->ScreenToClient(&rc);	// ... diventano coord. client rispetto al MDI padre (che NON e' theApp.m_pMainWnd !! ce n'è una un mezzo...
		wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
		d->WritePrivateProfileString(IDS_COORDINATE,myBuf);
		}
	}




int CVidsendView6::update() {
	int i,j,n;
	HTREEITEM tp0,tp1;
	CString aIP,S;
	UINT aPort;
	DWORD oldSel=0;
	CWebSrvSocket2_base *myRoot;

	if(tp0=GetTreeCtrl().GetSelectedItem()) {
		oldSel=GetTreeCtrl().GetItemData(tp0);
		}
	GetTreeCtrl().DeleteAllItems();
#ifdef _LINGUA_INGLESE
	tp0=GetTreeCtrl().InsertItem("Connections");
#else
	tp0=GetTreeCtrl().InsertItem("Connessioni");
#endif
	GetTreeCtrl().SetImageList(&il,TVSIL_NORMAL);
	GetTreeCtrl().SetItemImage(tp0,0,0);
	if(theApp.wwwSocket) {

		POSITION po,po1;

		po=theApp.wwwSocket->getClientRoot();
		if(po) {
			do {
				po1=po;
				myRoot=theApp.wwwSocket->getNextClient(po);
				if(myRoot->m_hSocket != INVALID_SOCKET) {
					myRoot->GetPeerName(aIP,aPort);
					tp1=GetTreeCtrl().InsertItem(aIP,tp0);
					GetTreeCtrl().SetItemImage(tp1,2,2);
					GetTreeCtrl().SetItemData(tp1,(DWORD)myRoot);		// completare... in futuro?
					}
				} while(po);
			}
		}
	if(theApp.theServer) {
		CControlSrvSocket2 *myRoot;
		POSITION po,po1;

		po=theApp.theServer->controlSocket->cSockRoot.GetHeadPosition();
		if(po) {
			do {
				po1=po;
				myRoot=theApp.theServer->controlSocket->cSockRoot.GetNext(po);
				if(myRoot->m_hSocket != INVALID_SOCKET) {
					myRoot->GetPeerName(aIP,aPort);
					S=aIP;
					if((myRoot->cliTimeOut+30000) < timeGetTime())
						S+=" (time-out)";




					if(theApp.debugMode) 
						if(theApp.FileSpool)
							theApp.FileSpool->print(CLogFile::flagInfo,"  control: IP: %s, time: %u",(LPCTSTR)S,myRoot->cliTimeOut);




					tp1=GetTreeCtrl().InsertItem(S,tp0);
					GetTreeCtrl().SetItemImage(tp1,1,1);
					GetTreeCtrl().SetItemData(tp1,(DWORD)myRoot);
					}
				} while(po);
			}
		
		}
	if(theApp.theChat) {
		CChatSrvSocket2 *myRoot;

		POSITION po,po1;

		po=theApp.theChat->chatSocket.cSockRoot.GetHeadPosition();
		if(po) {
			do {
				po1=po;
				myRoot=theApp.theChat->chatSocket.cSockRoot.GetNext(po);
				if(myRoot->m_hSocket != INVALID_SOCKET) {
					myRoot->GetPeerName(aIP,aPort);
					S=myRoot->connName+" ("+aIP+")";
					if((myRoot->cliTimeOut+30000) < timeGetTime())
						S+=" (time-out)";
					tp1=GetTreeCtrl().InsertItem(S,tp0);
					GetTreeCtrl().SetItemImage(tp1,4,4);
					GetTreeCtrl().SetItemData(tp1,(DWORD)myRoot);
					}
				} while(po);
			}
		}
	if(((CVidsendDoc6 *)GetDocument())->Opzioni & CVidsendDoc6::mostraAncheDirSrv) {
		// v. anche vidsendView7...
		CDirectorySrvSocket2 *myRoot;
		CAuthSrvSocket2 *myRoot2;
		CString aIP2;
		UINT aPort2;
		if(theApp.dirSocket) {
			POSITION po,po2;

			po=theApp.dirSocket->cSockRoot.GetHeadPosition();
			if(po) {
				do {
					myRoot=theApp.dirSocket->cSockRoot.GetNext(po);
					if(myRoot->m_hSocket != INVALID_SOCKET) {
						myRoot->GetPeerName(aIP,aPort);
						S=myRoot->connName+" ("+aIP+")";

#ifndef _NEWMEET_MODE
					// forse questa parte non dovrebbe esserci nella modalita' NEWMEET...

						po2=theApp.authSocket->cSockRoot.GetHeadPosition();
						if(po2) {
							do {
								myRoot2=theApp.authSocket->cSockRoot.GetNext(po2);
								myRoot2->GetPeerName(aIP2,aPort2);
								if(aIP2==aIP) {
									if((myRoot2->cliTimeOut+(AUTHSOCK_TIMEOUT/2)) < timeGetTime()) {			// v. anche OnTimer di MainFrm
										S+=" (time-out)";
										char myBuf[16];
										myBuf[0]=10;
										myBuf[1]=0;
										if((i=myRoot2->Send(myBuf,2)) == 2) {
											theApp.FileSpool->print(2,"** inviata richiesta heartbeat d'emergenza!");

											// non so come sia possibile, ma pur essendo i sock connessi ed entrambi in trasmissione
											// HeartBeat non viene ricevuto... QUINDI FACCIO COSI'!!
											myRoot2->cliTimeOut=timeGetTime();

											}
										else {
											DWORD n=GetLastError();
											if(theApp.FileSpool) 
												theApp.FileSpool->print(2,"impossibile inviare richiesta heartbeat d'emergenza (errore Send %d,%u)!",i,n);
											}
										}







		if(theApp.debugMode) 
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"  auth per dir: IP: %s, time: %u",(LPCTSTR)S,myRoot2->cliTimeOut);






									myRoot->cliTimeOut=-1;		// reimposto...
									goto fineDirSrv;
									}
								} while(po2);
							}
						
						S+=" (authSock non trovato)";
						// dopo un po di questo, CHIUDO i socket "monchi"...
						if(myRoot->cliTimeOut==-1)
							myRoot->cliTimeOut=timeGetTime()+AUTHSOCK_TIMEOUT;		// 2min...
						else {
							if(myRoot->cliTimeOut < timeGetTime()) {
								myRoot->myParent->doDelete(myRoot);
								if(theApp.FileSpool) {
									theApp.FileSpool->print(CLogFile::flagInfo2,"  cancellazione socket Dir che non risponde: %s",(LPCTSTR)aIP);
									}
								goto hasBeenDeleted;
								}
							}
#endif

fineDirSrv:
						;
						}
					else
						S=myRoot->connName+" (socket chiuso!)";

					tp1=GetTreeCtrl().InsertItem(S,tp0);
					GetTreeCtrl().SetItemImage(tp1,3,3);
					GetTreeCtrl().SetItemData(tp1,(DWORD)myRoot);
hasBeenDeleted:
						;
					} while(po);
				}
			}
			
		}
	if(theApp.timeSocket && theApp.timeSocket->m_hSocket != INVALID_SOCKET) {
		tp1=GetTreeCtrl().InsertItem(aIP,tp0);
		GetTreeCtrl().SetItemImage(tp1,3,3);		// finire...
		GetTreeCtrl().SetItemData(tp1,0);		// completare... in futuro?
		}
	

	GetTreeCtrl().Expand(tp0,TVE_EXPAND);
	if(oldSel) {
		tp1=GetTreeCtrl().GetChildItem(tp0);
		while(tp1) {
			if(oldSel == GetTreeCtrl().GetItemData(tp1)) {
				GetTreeCtrl().SelectItem(tp1/*,TVGN_CARET | TVGN_FIRSTVISIBLE*/);
				break;
				}
			tp1=GetTreeCtrl().GetNextSiblingItem(tp1);
			}
		}
	
	return 1;
	}

void CVidsendView6::OnTimer(UINT nIDEvent) {
	
	update();
	CTreeView::OnTimer(nIDEvent);
	}


BOOL CVidsendView6::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) {
	int i;

	if(i=CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext)) {
#ifdef _LINGUA_INGLESE
		GetParent()->SetWindowText("Connections");
#else
		GetParent()->SetWindowText("Connessioni");
#endif
		update();
		SetTimer(1,5000,NULL);	
		}
	return i;
	}


BOOL CVidsendView6::PreCreateWindow(CREATESTRUCT& cs) {
	
	cs.style |= TVS_HASLINES /*| TVS_LINESATROOT*/;
	return CTreeView::PreCreateWindow(cs);
	}


void CVidsendView6::OnRButtonDown(UINT nFlags, CPoint point) {
	CMenu myMenu;
	
	myMenu.LoadMenu(IDR_VIDSENTYPE6);
	CMenu *myMenu2=myMenu.GetSubMenu(2);
	myMenu2->AppendMenu(MF_SEPARATOR);
#ifdef _LINGUA_INGLESE
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Properties...");
#else
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Proprietà...");
#endif
	ClientToScreen(&point);
	myMenu2->TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, this /*GetParent()*/ );
	
//	CTreeView::OnRButtonDown(nFlags, point);
	}

void CVidsendView6::OnVisualizzaAggiorna() {

	update();
	}


void CVidsendView6::OnConnessioniDisconnetti() {
	HTREEITEM tp=GetTreeCtrl().GetSelectedItem();
	CControlCliSocket *c;
	char myBuf[256];

	if(tp) {
		c=(CControlCliSocket *)GetTreeCtrl().GetItemData(tp);
		if(c) {
			if(AfxMessageBox("Disconnettere?",MB_ICONQUESTION | MB_YESNO) == IDYES) {
				CString aIP;
				UINT aPort;
				c->GetPeerName(aIP,aPort);

#ifndef _NEWMEET_MODE
				CString aIP2;
				UINT aPort2;
				CAuthSrvSocket2 *myRoot2;
				POSITION po;
				int n1,n2;

				GetTreeCtrl().GetItemImage(tp,n1,n2);
				if(n1==3) {	// DirSrvSocket, quindi cerco e chiudo l'AuthSock relativo...
					po=theApp.authSocket->cSockRoot.GetHeadPosition();
					if(po) {
						do {
							myRoot2=theApp.authSocket->cSockRoot.GetNext(po);
							myRoot2->GetPeerName(aIP2,aPort2);
							if(aIP2==aIP) {
								myRoot2->Close();
								goto trovatoSocket;
								}
							} while(po);
						}

				}
trovatoSocket:				
#endif

				c->Close();
				}
			}
		}
	}

void CVidsendView6::OnUpdateConnessioniDisconnetti(CCmdUI* pCmdUI) {
	HTREEITEM h=NULL;

	h=GetTreeCtrl().GetSelectedItem();
	pCmdUI->Enable(h && GetTreeCtrl().GetItemData(h) != NULL);
	}

void CVidsendView6::OnConnessioniMandamessaggio() {
	HTREEITEM tp=GetTreeCtrl().GetSelectedItem();
	int i,i1;

	if(tp) {
		GetTreeCtrl().GetItemImage(tp,i,i1);
		switch(i) {
			case 1:				// video
				{
				CControlCliSocket *c;
				c=(CControlCliSocket *)GetTreeCtrl().GetItemData(tp);
				if(c) {
					CInputBoxDlg myDlg;
					if(myDlg.DoModal() == IDOK) {
						c->Send("\x2",1);
						c->Send(myDlg.m_Text,255);
						}
					}
				}
				break;
			case 3:				// DirSrv
				{
				CDirectoryCliSocket *c;
				c=(CDirectoryCliSocket *)GetTreeCtrl().GetItemData(tp);
				if(c) {
					CInputBoxDlg myDlg;
					if(myDlg.DoModal() == IDOK) {
						c->Send("\x2",1);
						c->Send(myDlg.m_Text,200);
						}
					}
				}
				break;
			case 4:				// chat
				{
				CChatCliSocket *c;
				c=(CChatCliSocket *)GetTreeCtrl().GetItemData(tp);
				if(c) {
					CInputBoxDlg myDlg;
					if(myDlg.DoModal() == IDOK) {
						c->Send("\x3",1);
						c->Send(myDlg.m_Text,200);
						}
					}
				}
				break;
			}
		}
	}

void CVidsendView6::OnUpdateConnessioniMandamessaggio(CCmdUI* pCmdUI) {
	HTREEITEM h=NULL;

	h=GetTreeCtrl().GetSelectedItem();
	pCmdUI->Enable(h && GetTreeCtrl().GetItemData(h) != NULL);
	}


void CVidsendView6::OnConnessioniInseriscinegliindesiderati() {
	HTREEITEM tp=GetTreeCtrl().GetSelectedItem();
	int i,i1;
	CStringList bl;
	CString S,S1;
	CChatSrvSocket2 *c;
	char myBuf[128];

	if(theApp.theChat && tp) {
		GetTreeCtrl().GetItemImage(tp,i,i1);
		if(i==4) {				// chat
			i=theApp.theChat->loadBlacklistedIP(&bl);
	//	if(i<CVidsendDoc4::BLACKLIST_SIZE) {
			S=GetTreeCtrl().GetItemText(tp);
			i=S.ReverseFind('(');	
			if(i != -1) {					// gestisco nome + IP o solo IP
				S=S.Mid(i+1);
				if(S.Right(1) == ')')
					S=S.Left(S.GetLength()-1);
				}
			if(S == CWebSrvSocket2_base::getMyOutmostIPAddress(myBuf))
				AfxMessageBox("Attenzione: state inserendo il vostro computer tra gli indesiderati!",MB_ICONSTOP);
			c=(CChatSrvSocket2 *)(GetTreeCtrl().GetItemData(tp));
			S1.Format("%u",c->serNum);
			if(!bl.Find(S1)) {
				bl.AddTail(S1);
				theApp.theChat->saveBlacklistedIP(&bl);
				if(AfxMessageBox("Utente inserito!\nSi desidera disconnetterlo ora?",MB_YESNO) == IDYES) {
					c->Close();
					}
				}
			else
				AfxMessageBox("L'utente è già segnato!",MB_ICONEXCLAMATION);
	//		}
	//	else {
	//		AfxMessageBox("La lista degli utenti indesiderati è piena!");
	//		}
			}
		else if(i==1) {				// x il video x ora nulla...
			}
		}

	}

void CVidsendView6::OnUpdateConnessioniInseriscinegliindesiderati(CCmdUI* pCmdUI) {
	HTREEITEM h=NULL;

	h=GetTreeCtrl().GetSelectedItem();
	pCmdUI->Enable(h && GetTreeCtrl().GetItemData(h) != NULL);
	}


void CVidsendView6::OnConnessioniMandamessaggioatutti() {
	// TODO: Add your command handler code here
	
}


/////////////////////////////////////////////////////////////////////////////
// CVidsendView7 - directory dei server disponibili

IMPLEMENT_DYNCREATE(CVidsendView7, CTreeView)

CVidsendView7::CVidsendView7() {

	il.Create(IDB_CONNECTIONS,16,0,RGB(255,255,255));
	}

CVidsendView7::~CVidsendView7() {
	}


BEGIN_MESSAGE_MAP(CVidsendView7, CTreeView)
	//{{AFX_MSG_MAP(CVidsendView7)
	ON_WM_TIMER()
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_VISUALIZZA_AGGIORNA, OnVisualizzaAggiorna)
	ON_WM_DESTROY()
	ON_COMMAND(ID_COMPUTER_DISCONNETTI, OnComputerDisconnetti)
	ON_UPDATE_COMMAND_UI(ID_COMPUTER_DISCONNETTI, OnUpdateComputerDisconnetti)
	ON_COMMAND(ID_COMPUTER_MANDAMESSAGGIO, OnComputerMandamessaggio)
	ON_UPDATE_COMMAND_UI(ID_COMPUTER_MANDAMESSAGGIO, OnUpdateComputerMandamessaggio)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendView7 drawing

void CVidsendView7::OnDraw(CDC* pDC) {
	
	CVidsendDoc7 *pDoc = GetDocument();
	// TODO: add draw code here
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendView7 diagnostics

#ifdef _DEBUG
void CVidsendView7::AssertValid() const
{
	CTreeView::AssertValid();
}

void CVidsendView7::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}
CVidsendDoc7 *CVidsendView7::GetDocument() { // non-debug version is inline

	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVidsendDoc7)));
	return (CVidsendDoc7 *)m_pDocument;
	}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendView7 message handlers






int CVidsendView7::update() {
	int i,j,n,oldImg;
	char myBuf[128];
	HTREEITEM tp0,tp1;
	CString aIP,oldSel,S;
	UINT aPort;
	CDirectorySrvSocket2 *myRoot;

	if(tp0=GetTreeCtrl().GetSelectedItem()) {
		oldSel=GetTreeCtrl().GetItemText(tp0);
		GetTreeCtrl().GetItemImage(tp0,oldImg,i);
		}
	GetTreeCtrl().DeleteAllItems();
	CWebSrvSocket2_base::getMyIPAddress(myBuf);
	S="Server registrati su ";
	S+=myBuf;
	tp0=GetTreeCtrl().InsertItem(S);
	GetTreeCtrl().SetImageList(&il,TVSIL_NORMAL);
	GetTreeCtrl().SetItemImage(tp0,0,0);
	if(theApp.dirSocket) {
		CAuthSrvSocket2 *myRoot2;
		CString aIP2;
		UINT aPort2;
		POSITION po,po2;

		po=theApp.dirSocket->cSockRoot.GetHeadPosition();
		if(po) {
			do {
				myRoot=theApp.dirSocket->cSockRoot.GetNext(po);
				if(myRoot->m_hSocket != INVALID_SOCKET) {
					myRoot->GetPeerName(aIP,aPort);
					S=myRoot->connName+" ("+aIP+")";

					po2=theApp.authSocket->cSockRoot.GetHeadPosition();
					if(po2) {
						do {
							myRoot2=theApp.authSocket->cSockRoot.GetNext(po2);
							myRoot2->GetPeerName(aIP2,aPort2);
							if(aIP2==aIP) {
								if((myRoot2->cliTimeOut+(AUTHSOCK_TIMEOUT/2)) < timeGetTime())		// v. anche OnTimer di MainFrm
									S+=" (time-out)";
								myRoot->cliTimeOut=-1;		// reimposto...
								goto fineDirSrv;
								}
							} while(po2);
						}

					// dopo un po di questo, CHIUDO i socket "monchi"...
					if(myRoot->cliTimeOut==-1)
						myRoot->cliTimeOut=timeGetTime()+AUTHSOCK_TIMEOUT;		// 1.5min...
					else {
						if(myRoot->cliTimeOut < timeGetTime()) {
							myRoot->myParent->doDelete(myRoot);
							if(theApp.FileSpool) {
								theApp.FileSpool->print(2,"  cancellazione socket Dir che non risponde: %s",(LPCTSTR)aIP);
								}
							goto hasBeenDeleted;
							}
						}
					S+=" (authSock non trovato)";
fineDirSrv:
					;
					}
				else
					S=myRoot->connName+" (socket chiuso!)";

				tp1=GetTreeCtrl().InsertItem(S,tp0);
				GetTreeCtrl().SetItemImage(tp1,2,2);
				GetTreeCtrl().SetItemData(tp1,(DWORD)myRoot);
hasBeenDeleted:
					;
				} while(po);
			}
		}
	


	GetTreeCtrl().Expand(tp0,TVE_EXPAND);
	if(!oldSel.IsEmpty()) {
		tp1=GetTreeCtrl().GetChildItem(tp0);
		while(tp1) {
			int l1,l2;
			S=GetTreeCtrl().GetItemText(tp1);
			GetTreeCtrl().GetItemImage(tp1,l1,l2);
			if(S == oldSel && l1 == oldImg) {
				GetTreeCtrl().SelectItem(tp1/*,TVGN_CARET | TVGN_FIRSTVISIBLE*/);
				break;
				}
			tp1=GetTreeCtrl().GetNextSiblingItem(tp1);
			}
		}
	
	return 1;
	}

void CVidsendView7::OnTimer(UINT nIDEvent) {
	
	update();
	CTreeView::OnTimer(nIDEvent);
	}


BOOL CVidsendView7::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) {
	int i;

	if(i=CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext)) {
#ifdef _LINGUA_INGLESE
		GetParent()->SetWindowText("Available servers");
#else
		GetParent()->SetWindowText("Server disponibili");
#endif
		update();
		SetTimer(1,5000,NULL);	
		}
	return i;
	}


BOOL CVidsendView7::PreCreateWindow(CREATESTRUCT& cs) {
	
	cs.style |= TVS_HASLINES /*| TVS_LINESATROOT*/;
	return CTreeView::PreCreateWindow(cs);
	}


void CVidsendView7::OnRButtonDown(UINT nFlags, CPoint point) {
	CMenu myMenu;
	
	myMenu.LoadMenu(IDR_VIDSENTYPE7);
	CMenu *myMenu2=myMenu.GetSubMenu(2);
	myMenu2->AppendMenu(MF_SEPARATOR);
#ifdef _LINGUA_INGLESE
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Properties...");
#else
	myMenu2->AppendMenu(MF_STRING,ID_FILE_PROPRIETA,"Proprietà...");
#endif
	ClientToScreen(&point);
	myMenu2->TrackPopupMenu(TPM_LEFTBUTTON | TPM_LEFTALIGN, point.x, point.y, GetParent());
	
	CTreeView::OnRButtonDown(nFlags, point);
	}

void CVidsendView7::OnVisualizzaAggiorna() {

	update();
	}

void CVidsendView7::OnDestroy() {
	CVidsendDoc7 *d=GetDocument();
	RECT rc;
	char myBuf[64];
	
	CTreeView::OnDestroy();
	
	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		GetParent()->GetWindowRect(&rc);	// coord. schermo della mia MDIChildFrmae...
		GetParent()->GetParent()->ScreenToClient(&rc);	// ... diventano coord. client rispetto al MDI padre (che NON e' theApp.m_pMainWnd !! ce n'è una un mezzo...
		wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
		d->WritePrivateProfileString(IDS_COORDINATE,myBuf);
		}
	}





void CVidsendView7::OnComputerDisconnetti() 
{
	// TODO: Add your command handler code here
	
}

void CVidsendView7::OnUpdateComputerDisconnetti(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CVidsendView7::OnComputerMandamessaggio() 
{
	// TODO: Add your command handler code here
	
}

void CVidsendView7::OnUpdateComputerMandamessaggio(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

