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
#include "wav.h"
#include <cjpeg2.h>
#include "player.h"
#include "joshuamp3.h"
//#include "tx4ole.h"
#include "scopeguardmutex.h"

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
	ON_UPDATE_COMMAND_UI(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_SORGENTE4, OnUpdateControlloControlloremototelecamereSorgente4)
	ON_COMMAND(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_ALTERNASORGENTI, OnControlloControlloremototelecamereAlternasorgenti)
	ON_COMMAND(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_SORGENTE1, OnControlloControlloremototelecamereSorgente1)
	ON_COMMAND(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_SORGENTE2, OnControlloControlloremototelecamereSorgente2)
	ON_COMMAND(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_SORGENTE3, OnControlloControlloremototelecamereSorgente3)
	ON_COMMAND(ID_CONTROLLO_CONTROLLOREMOTOTELECAMERE_SORGENTE4, OnControlloControlloremototelecamereSorgente4)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendView construction/destruction

CVidsendView::CVidsendView() {
	//{{AFX_DATA_INIT(CVidsendView)
	//}}AFX_DATA_INIT

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
	framesPerSec=0;
	theTimer=0;

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

	if(theApp.Opzioni & CVidsendApp::saveLayout)
		d->savelayout();
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendView diagnostics

void CVidsendView::initAV(const BITMAPINFOHEADER *bmi,DWORD fps,const EXT_WAVEFORMATEX *wf) {
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
		setTimer(1000/fps /*-10*/ /*piccola correzione... sembra durare sempre 20mSec in + !*/);
		// forse c'entrava la Timer Resolution?  	timeBeginPeriod(1);

		}

	if(wf) {
		wfn.wFormatTag = WAVE_FORMAT_PCM;
		wfn.nChannels = wf->wf.nChannels /*1*/;
		wfn.nSamplesPerSec = wf->wf.nSamplesPerSec /* 8000*/;
		wfn.wBitsPerSample = wf->wf.wBitsPerSample;
		wfn.nBlockAlign = wf->wf.nBlockAlign;
		wfn.nAvgBytesPerSec = (wfn.nSamplesPerSec*wfn.nChannels*wfn.wBitsPerSample)/8;
		wfn.cbSize = 0;
		hWaveOut=NULL;
		i=waveOutOpen(&hWaveOut,WAVE_MAPPER,&wfn,(DWORD)waveOutCallback,(DWORD)this,CALLBACK_FUNCTION);
		if(i != MMSYSERR_NOERROR)
			AfxMessageBox("Impossibile aprire periferica audio");
	
		wfs=*wf;
//		if(wfs.wf.wFormatTag != WAVE_FORMAT_PCM) {			// sarebbe da fare, ma bisogna analgoamente togliere ACM in tutti gli altri posti (2021)
			acmStreamOpen(&hAcm,NULL,(WAVEFORMATEX *)&wfs,&wfn,NULL,NULL,0 /*this*/,0);
			if(hAcm)
				acmStreamSize(hAcm,wfs.wf.nAvgBytesPerSec,&maxWaveoutSize,ACM_STREAMSIZEF_SOURCE);
			else
				AfxMessageBox("Impossibile aprire convertitore stream audio");
//			}
//		if(hWaveOut)
//			waveOutPause(hWaveOut);			// boh?? tolto 2021, v. anche restart in Sockets (con bug totbuffer)
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

void CVidsendView::drawFrame(const struct AV_PACKET_HDR *avh) {
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
		//in avh->info b8 ora c'è qbox!
		bi=(BITMAPINFOHEADER *)s;
//		bi=&(bmi->bmiHeader);
		if(theApp.debugMode) {
			if(theApp.FileSpool) {
				char myBuf[128];
				wsprintf(myBuf,"  drawFrame: keyframe+alert %x ",avh->info);
				*theApp.FileSpool << myBuf;
				}
			}
		if(!waitForKeyFrame || (avh->info & AVIIF_KEYFRAME)) {
			waitForKeyFrame=0;
			if(theFrame)
				HeapFree(GetProcessHeap(),0,theFrame);
			renderedFrameTsp=avh->timestamp;
			theFrame=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,biCompDef.biWidth*biCompDef.biHeight*biCompDef.biBitCount/8 +100);
			i=ICDecompress(hICDe, avh->info & AVIIF_KEYFRAME ? 0 : ICDECOMPRESS_NOTKEYFRAME,
				bi,(s+sizeof(BITMAPINFOHEADER)),&biRawDef,theFrame);
			if(i==ICERR_OK) {

				if(theApp.theServer && theApp.theServer->trasmMode==3) {			// per proxy
/*			VIDEOHDR lpVHdr;
			lpVHdr.lpData= (BYTE *)lParam;
			lpVHdr.dwBytesUsed=lpVHdr.dwBufferLength=wParam;
			lpVHdr.dwFlags=lpVHdr.dwUser=0;
			lpVHdr.dwTimeCaptured=timeGetTime();*/
//		m_TV->compressAFrame(&lpVHdr,m_TV->m_DShow->doPreview,v,hdd,&bmih);

					}

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

void CVidsendView::playSample(const struct AV_PACKET_HDR *avh) {
	int i;
	WORD n;
	BYTE *s,*smp;
	WAVEFORMATEX *wf;
	ACMSTREAMHEADER hhacm;
	DWORD l;
	WAVEHDR *wh;

	if(GetDocument() && !GetDocument()->bPaused) {

		i=syncToVideo(avh);
		if(i) {
			s=(BYTE *)avh->lpData;
	//			avh->info |= AV_PACKET_USED; non usata

	//		wf=(WAVEFORMATEX *)s;
			smp=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,maxWaveoutSize);
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

				if(!(GetDocument()->Opzioni & CVidsendDoc::muted)) {
					if(hWaveOut) {
						wh=new WAVEHDR;
						// l'ULTIMO wh al Close pare rimanere allocato... 12.18
						if(GetDocument()->Opzioni & CVidsendDoc::sstereo) {
							BYTE k1,k2;
							// bisogna forzare la WAVEOUT a stereo, innanzitutto...
							// ... a pensarci bene, cos'è "stereo simulato"?? forse si intendeva SuperStereo come da N.E.!
							for(i=0; i<wh->dwBufferLength; i++) {			// ovviamente solo se PCM...
/*								k1=wh->lpData[i]/8;
								k2=lpData2[i]/8;
								wh->lpData[i]-=k1;
								lpData2[i]-=k2;*/
								}
							}
						if(GetDocument()->Opzioni & CVidsendDoc::mono) {
							BYTE k1,k2;
							for(i=0; i<wh->dwBufferLength; i++) {			// ovviamente solo se PCM...
/*								k1=wh->lpData[i];
								k2=lpData2[i];
								k2=(k1+k2)/2
								wh->lpData[i]=k1;
								lpData2[i]=k1;*/
								}
							}

						wh->lpData=(char *)smp;
						wh->dwBufferLength=hhacm.cbDstLengthUsed;
						wh->dwFlags=0;
						wh->dwLoops=0;
						waveOutPrepareHeader(hWaveOut,wh,sizeof(WAVEHDR));
						waveOutWrite(hWaveOut,wh,sizeof(WAVEHDR));
						n=measureAudio((BYTE *)wh->lpData,wh->dwBufferLength);
						((CChildFrame *)GetParent())->m_VUMeter->SetWindowText(n);
						}
					else
						HeapFree(GetProcessHeap(),0,smp);
					}
				else {
					HeapFree(GetProcessHeap(),0,smp);
					((CChildFrame *)GetParent())->m_VUMeter->SetWindowText(0);
					}
				}
			else
				HeapFree(GetProcessHeap(),0,smp);
			}

		}
	}

WORD CVidsendView::measureAudio(const BYTE *p,DWORD len) {
	int i;
	DWORD n,n1=len;
	// wow, e una FFT? :) 2019
	// v. BESTAUDIOPLAYER 2021!

	n=0;
	while(n1--) {
		i=*p-128;
		if(i < 0)
			i=-i;
		n+=i;
		p++;
		}
	n /= len;
	return sqrt(n);
	}


BOOL CVidsendView::syncToVideo(const struct AV_PACKET_HDR *avh) {
// migliorare... forse e' meglio fare sync to audio (tanti frame su uno solo...)
	if(avh->timestamp < renderedFrameTsp)		
		return 0;
	else
		return 1;
	}

int CVidsendView::setTimer(int t) {

	if(!theTimer || t != framesPerSec) {
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
//			myPtr->cliSockA->totBuffers--;		// ma bumpOutBuffers ?? 2021
//			myPtr->cliSockA->bumpOutBuffer();
//			if(myPtr->cliSockA->totBuffers < myPtr->cliSockA->getLowBuffers()) {
//				waveOutPause(hwo);
//				}
			waveOutUnprepareHeader(hwo,(LPWAVEHDR)dwParam1,sizeof(WAVEHDR));
			HeapFree(GetProcessHeap(),0,((WAVEHDR *)dwParam1)->lpData);
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
		GetDocument()->wantsVideo=myDlg.m_Video;
		GetDocument()->wantsAudio=myDlg.m_Audio;
connetti:
		doConnessioneRiconnetti();
		}
	}

int CVidsendView::doConnect(const char *myAddress,const struct STREAM_INFO *si) {
	CVidsendDoc *d=GetDocument();
	int i=0;
	DWORD l;

	streamInfo=*si;

	if(si->versione > VIDSEND_VERSIONE) {
		i=-1;
		goto fine;
		}

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
		}
		else
			cliSockCtrl->SendUserPass((char *)(LPCTSTR)theApp.infoUtente.cognome,(char *)(LPCTSTR)"",0,0);
		// ...faccio che mandare il nome del client anche se non c'e' autenticazione! 2023 modifica


	if(d->wantsVideo && si->bm.biSize) {		// il server trasmette video...
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

	if(d->wantsAudio && si->wf.wf.wFormatTag) {		// il server trasmette audio...
		if(!(cliSockA=new CStreamACliSocket(this,&hWaveOut,si->noBuffers ? 0 : GetDocument()->numBuffers)))
			goto fine;
		if(!cliSockA->Create(0,theApp.Opzioni & CVidsendApp::TCP_UDP ? SOCK_DGRAM : SOCK_STREAM))
			goto fine;
		if(!cliSockA->Connect(myAddress,AUDIO_SOCKET))
			goto fine;
//		cliSockA->initBuffers(GetDocument()->numBuffers/4);
		i=1;		// mettere a posto meglio...
		}
	else {
		((CChildFrame *)GetParent())->m_VUMeter->ShowWindow(0);
		}

	if(si->maxTime == 0 && (d->authSocket && d->authSocket->timedConn > 0)) {
		((struct STREAM_INFO *)si)->maxTime=d->authSocket->timedConn;
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

	if(theTimer)
		KillTimer(theTimer);
	theTimer=0;

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

	doConnessioneRiconnetti(GetDocument()->wantsVideo,GetDocument()->wantsAudio);
	}

int CVidsendView::doConnessioneRiconnetti(BOOL wantVideo,BOOL wantAudio) {
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
		return 1;
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

		return 0;
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
	SIZE r;

	if(cliSockV) {
		S1.Format("Ricezione video %ux%u pixel, %ubpp, %ufps, compressione 0x%X",	// mettere nome compressore!
			streamInfo.bm.biWidth,streamInfo.bm.biHeight,streamInfo.bm.biBitCount,streamInfo.fps,streamInfo.bm.biCompression);
		S=S1+"\n";
		}
	if(cliSockA) {
		S1.Format("Ricezione audio %usample/sec, %ubit, %ucanale(i), formato 0x%X",
			streamInfo.wf.wf.nSamplesPerSec,streamInfo.wf.wf.wBitsPerSample,streamInfo.wf.wf.nChannels,streamInfo.wf.wf.wFormatTag);
		S+=S1+"\n";
		}
	r.cx=streamInfo.bm.biWidth;
	r.cy=streamInfo.bm.biHeight;
	S1.Format("Bitrate: %uKbps",
		CVidsendDoc2::calcBandWidth(TRUE,&r,streamInfo.bm.biBitCount,streamInfo.bm.biCompression,streamInfo.quality,streamInfo.fps,
		TRUE,streamInfo.wf.wf.nSamplesPerSec,streamInfo.wf.wf.wBitsPerSample,streamInfo.wf.wf.nChannels,streamInfo.wf.wf.wFormatTag,streamInfo.quality /*è del VIDEO ma ok!*/)
		/128 /* x8 e /1024 */);
	S+=S1;
	AfxMessageBox(S,MB_ICONINFORMATION);
	}

void CVidsendView::OnUpdateConnessioneCaratteristicheconnessione(CCmdUI* pCmdUI) {

	pCmdUI->Enable(cliSockCtrl->m_hSocket != INVALID_SOCKET);
	}


void CVidsendView::OnUpdateControlloControlloremototelecamereSorgente1(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(/* cliSockV && bah potrei controllarne l'audio.. */ streamInfo.remoteCtrl & 1);
	}

void CVidsendView::OnUpdateControlloControlloremototelecamereSorgente2(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(/* cliSockV && bah potrei controllarne l'audio.. */ streamInfo.remoteCtrl & 2);
	}

void CVidsendView::OnUpdateControlloControlloremototelecamereSorgente3(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(/* cliSockV && bah potrei controllarne l'audio.. */ streamInfo.remoteCtrl & 4);
	}

void CVidsendView::OnUpdateControlloControlloremototelecamereSorgente4(CCmdUI* pCmdUI) {

	pCmdUI->Enable(/* cliSockV && bah potrei controllarne l'audio.. */ streamInfo.remoteCtrl & 8);
	}

void CVidsendView::OnControlloControlloremototelecamereAlternasorgenti() {
	// TODO: Add your command handler code here
	
	}

void CVidsendView::OnControlloControlloremototelecamereSorgente1() {
	// TODO: Add your command handler code here
	
	}

void CVidsendView::OnControlloControlloremototelecamereSorgente2() {
	// TODO: Add your command handler code here
	
	}

void CVidsendView::OnControlloControlloremototelecamereSorgente3() {
	// TODO: Add your command handler code here
	
	}

void CVidsendView::OnControlloControlloremototelecamereSorgente4() {
	// TODO: Add your command handler code here
	
	}

void CVidsendView::OnUpdateControlloControlloremototelecamereAlternasorgenti(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(/* cliSockV && bah potrei controllarne l'audio.. */ streamInfo.remoteCtrl & 128);
	}

void CVidsendView::OnEditCopy() {
	HANDLE h;
	BYTE *p;
	DWORD l;
	CVidsendDoc *d=GetDocument();
	
	if(OpenClipboard()) {
		if(cliSockV) {		// questo ha la priorità :)
			l=biRawDef.biWidth*biRawDef.biHeight*3;
 			h=GlobalAlloc(GMEM_MOVEABLE,l+sizeof(BITMAPINFOHEADER)+32);
			p=(BYTE *)GlobalLock(h);
			memcpy(p,&biRawDef,sizeof(BITMAPINFOHEADER));
			memcpy(p+sizeof(BITMAPINFOHEADER),theFrame,l);
			GlobalUnlock(h);
			EmptyClipboard();
			SetClipboardData(CF_DIB,h);
			}
		else if(cliSockA) {
			struct WAV_HEADER_0 wh;
			struct WAV_HEADER_FMT wfmt;
			struct WAV_HEADER_DATA whd;

			wh.tag[0]='R'; wh.tag[1]='I'; wh.tag[2]='F'; wh.tag[3]='F';
			wh.NumBytes=streamInfo.wf.wf.nAvgBytesPerSec -8 +
				sizeof(struct WAV_HEADER_0)+sizeof(struct WAV_HEADER_FMT) -2 +sizeof(struct WAV_HEADER_DATA);
			wh.tag1[0]='W'; wh.tag1[1]='A'; wh.tag1[2]='V'; wh.tag1[3]='E'; 
			wfmt.tag[0]='f'; wfmt.tag[1]='m'; wfmt.tag[2]='t'; wfmt.tag[3]=' '; 
			wfmt.size=sizeof(struct WAV_HEADER_FMT)-sizeof(wfmt.size)-sizeof(wfmt.tag)-2;
			wfmt.AudioFormat=1;
			wfmt.NumChannels=streamInfo.wf.wf.nChannels;
			wfmt.SamplesPerSec=streamInfo.wf.wf.nSamplesPerSec;
			wfmt.BytesPerSec=streamInfo.wf.wf.nAvgBytesPerSec;
			wfmt.BlockAlign=streamInfo.wf.wf.nBlockAlign;
			wfmt.BitsPerSample=streamInfo.wf.wf.wBitsPerSample;
			wfmt.ExtraParamSize=0;			//la "scavalco" sotto :) e sottraggo size
			whd.tag[0]='d'; whd.tag[1]='a'; whd.tag[2]='t'; whd.tag[3]='a'; 
			whd.NumSamples=wh.NumBytes-sizeof(struct WAV_HEADER_0)-sizeof(struct WAV_HEADER_FMT) -2 -sizeof(struct WAV_HEADER_DATA);
 			h=GlobalAlloc(GMEM_MOVEABLE,wh.NumBytes+8);
			p=(BYTE *)GlobalLock(h);
			memcpy(p,&wh,sizeof(struct WAV_HEADER_0));
			memcpy(p+sizeof(struct WAV_HEADER_0),&wfmt,sizeof(struct WAV_HEADER_FMT));
			memcpy(p+sizeof(struct WAV_HEADER_0)+sizeof(struct WAV_HEADER_FMT) -2 ,&whd,sizeof(struct WAV_HEADER_DATA));

			/* prendere i samples ... in qualche modo
			memcpy(p+sizeof(struct WAV_HEADER_0)+sizeof(struct WAV_HEADER_FMT) -2 +sizeof(struct WAV_HEADER_DATA),
				theApp.m_pSamples,streamInfo.wf.wf.nAvgBytesPerSec); */

			GlobalUnlock(h);
			EmptyClipboard();
			SetClipboardData(CF_WAVE,h);
			}
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
	CStringEx S;
	
	if(myDlg.DoModal() == IDOK) {
		l=biRawDef.biWidth*biRawDef.biHeight*biRawDef.biBitCount/8;
		CFile mF;
		S=myDlg.GetFileName();
		if(S.FindNoCase(".jpg") != -1) {
			CJpeg myJPEG;
			BYTE *p;
			if(b.CreateBitmap(biRawDef.biWidth,biRawDef.biHeight,1,biRawDef.biBitCount,NULL)) {
				if(b.SetBitmapBits(l,theFrame)) {
					if(p=myJPEG.buildJPEG(&b,&len,TRUE)) {
						if(mF.Open(myDlg.GetPathName(),CFile::modeCreate | CFile::modeReadWrite | CFile::typeBinary | CFile::shareDenyWrite)) {
							mF.Write(p,len);
							mF.Close();
							HeapFree(GetProcessHeap(),0,p);
							return;
							}
						}
					}
				}
			}
		else if(S.FindNoCase(".bmp") != -1) {
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
	
	pCmdUI->Enable(cliSockCtrl->m_hSocket != INVALID_SOCKET && cliSockV && theFrame != NULL);
	}


void CVidsendView::OnFileSave() {

	
	}

void CVidsendView::OnUpdateFileSave(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(cliSockCtrl->m_hSocket != INVALID_SOCKET && !streamInfo.dontSave);
	}


void CVidsendView::OnVisualizzaVolume() {

	WinExec("sndvol32.exe",SW_SHOWNORMAL);
	WinExec("sndvol.exe",SW_SHOWNORMAL);			// per Windows 7 !!!
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
	const struct AV_PACKET_HDR *av;
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
		//in av->info b8 ora c'è qbox!
//			av->info |= AV_PACKET_USED; non usata


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
				if(cliSockA) 
					n+=cliSockA->getBytesReceived();
#ifdef _LINGUA_INGLESE
				wsprintf(p,"playback (@ %d.%d KB/s) (b:%d - %d)...",n/2000,(n % 2000)/200,/*cliSockV->getFirstIn(),cliSockV->getLastOut()*/ cliSockV->totAvailBuffers(),cliSockV->needMoreBuffers());
#else
				wsprintf(p,"riproduzione (@ %d.%d KB/s) (b:%d - %d)...",n/2000,(n % 2000)/200,
					/*cliSockV->getFirstIn(),cliSockV->getLastOut()*/ cliSockV->totAvailBuffers(),cliSockV->needMoreBuffers());
#endif
				if(((CChildFrame *)GetParent())->m_wndStatusBar.m_hWnd)
					((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,p,TRUE);
				}
			}
		else {
			if((timeGetTime() - timeRef) > 2000) {
				timeRef=timeGetTime();
				n=cliSockV->getBytesReceived();
				if(cliSockA) 
					n+=cliSockA->getBytesReceived();
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
	else {
		CDC *hdc = GetDC();
		RECT rc;
		GetClientRect(&rc);
		theApp.renderBitmap(hdc,IDB_MUSIC,&rc);
		}

	if(cliSockA) {			// (era stato gestito direttamente da socket... boh, credo 2018)
		av=cliSockA->getOutBuffer();
		if(av) {
			playSample(av);
	//		setTimer(av->psec);
			cliSockA->bumpOutBuffer();
			if(cliSockA->needMoreBuffers())
				cliSockA->Send("\x00",1);
			if((timeGetTime() - timeRef) > 2000) {
				timeRef=timeGetTime();
				n=cliSockA->getBytesReceived();
	//			wsprintf(p,"riproduzione (@ %d.%d KB/s) (b:%d - %d)...",n/2000,(n % 2000)/200,cliSockV->totAvailBuffers(),cliSockV->needMoreBuffers());
	//			if(((CChildFrame *)GetParent())->m_wndStatusBar.m_hWnd)
	//				((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(0,p,TRUE);
				}
			}
		else {
			if((timeGetTime() - timeRef) > 2000) {
				timeRef=timeGetTime();
				n=cliSockA->getBytesReceived();
	//			wsprintf(p,"bufferizzazione (%d di %d) (@ %d.%d KB/s)...",cliSockV->totAvailBuffers(),CStreamVCliSocket::maxStreamBuffers,n/2000, (n % 2000)/200);
	//			if(((CChildFrame *)GetParent())->m_wndStatusBar.m_hWnd)
	//				((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(0,p,TRUE);
				}
			}
		}
	if(cliSockA && !cliSockV) {			// ...quindi faccio così per vedere il bitrate!
		if((timeGetTime() - timeRef) > 2000) {
			timeRef=timeGetTime();
			n=cliSockA->getBytesReceived();
#ifdef _LINGUA_INGLESE
			wsprintf(p,"playback (@ %d.%d KB/s) (b:%d - %d)...",n/2000,(n % 2000)/200,/*cliSockV->getFirstIn(),cliSockV->getLastOut()*/ cliSockA->totAvailBuffers(),cliSockA->needMoreBuffers());
#else
			wsprintf(p,"riproduzione (@ %d.%d KB/s) (b:%d - %d)...",n/2000,(n % 2000)/200,
				/*cliSockV->getFirstIn(),cliSockV->getLastOut()*/ cliSockA->totAvailBuffers(),cliSockA->needMoreBuffers());
#endif
			if(((CChildFrame *)GetParent())->m_wndStatusBar.m_hWnd)
				((CChildFrame *)GetParent())->m_wndStatusBar.SetPaneText(1,p,TRUE);
			}
		}

	
	CView::OnTimer(nIDEvent);
	}

long CVidsendView::OnSoundPlayerNotify(long eventNumber) {
//	ScopeGuardMutex guard(&m_exprMutex);  dovuto a STATIC??
	const struct AUDIO_BUFFER *p;

/* USARE DirectSound 2021!!
	if(!theApp.bPaused) {
		if(theApp.m_Socket.isOk2Out()) {
//			((CMainFrame *)m_pMainWnd)->m_wndStatusBar.SetPaneText(1,"playback",TRUE);
			p=theApp.m_Socket.getOutBuffer();
			if(p) {
				theApp.m_pSoundPlayer->Write(0, p->ptr, theApp.wf.nAvgBytesPerSec);
				theApp.m_pSoundPlayer->Play(0); // 
				}
			theApp.m_Socket.bumpOutBuffer();
			}
		}
//				theApp.m_pSoundPlayer->Write(0, (BYTE*)buff, 100);			// TRIGGERA 1°evento!
//				theApp.m_pSoundPlayer->Play(0); // 

return;	
	try	{
		m_pSoundPlayer->Write(((eventNumber+1) % m_nbSoundPlayerEvents)*m_soundPlayerEventSize, (unsigned char *)m_pSamples, 
			m_soundPlayerEventSize);
		}
	catch( CSimpleException &e )	{
//		OutputDebugString(e.getAllExceptionStr().c_str());		// it would be better to stop the program...
		}			
*/
	return 0;
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
	ON_MESSAGE(WM_RAWAUDIOFRAME_READY,OnRawAudioFrameReady)
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
	SIZE r;
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
				r.cx=doc->theTV->biRawDef.bmiHeader.biWidth;
				r.cy=doc->theTV->biRawDef.bmiHeader.biHeight;
				b=theApp.createTestBitmap(&r,&doc->theTV->biRawDef.bmiHeader,doc->pagProva.tipoVideo);
				b->GetBitmap(&bmp);
				bSize=bmp.bmWidthBytes*bmp.bmHeight;
				s=(BYTE *)GlobalAlloc(GMEM_FIXED,bSize);
				b->GetBitmapBits(bSize,s);

				l=t=0;

				VIDEOHDR lpVHdr;
				lpVHdr.lpData= (BYTE *)s;
				lpVHdr.dwBytesUsed=lpVHdr.dwBufferLength=bSize /*0*/;
				lpVHdr.dwFlags=lpVHdr.dwUser=0;
				lpVHdr.dwTimeCaptured=timeGetTime();
				doc->theTV->compressAFrame(&lpVHdr,TRUE/*doc->theTV->m_DShow->doPreview*/,this,doc->theHDD,&doc->theTV->biRawDef.bmiHeader);
				HeapFree(GetProcessHeap(),0,s);
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
							d=theApp.createTestWave(NULL,&doc->theTV->wfex,&l,i,GetDocument()->pagProva.audioOpzioni & 2);
				//			l+=sizeof(WAVEFORMATEX);

							// FINIRE la gestione di  / AUDIO_BUFFER_DIVIDER, servono 2 timer o boh

				SendMessage(WM_RAWAUDIOFRAME_READY,(WPARAM)d,(LPARAM)l);
	// fare mixWaves(const WORD *s,DWORD da,DWORD a,BYTE *d,DWORD l,DWORD in,short int vol) {   // se<>0 da dove a dove (nel primo) o solo la lunghezza se da=0
// OCCHIO! serve Send o il buffer viene cimito 


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
							if(doc->theTV->m_hAcm && !acmStreamPrepareHeader(doc->theTV->m_hAcm,&hhacm,0)) {
#ifdef _NEWMEET_MODE
								avh->tag=MAKEFOURCC('G','D','2','0');
#else
								avh->tag=MAKEFOURCC('D','G','2','0');
#endif
								avh->type=AV_PACKET_TYPE_AUDIO;
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
								HeapFree(GetProcessHeap(),0,dOut);
								}
							HeapFree(GetProcessHeap(),0,d);
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

							t=l=0;
							VIDEOHDR lpVHdr;
							lpVHdr.lpData= (BYTE *)b;
							lpVHdr.dwBytesUsed=lpVHdr.dwBufferLength=0;
							lpVHdr.dwFlags=lpVHdr.dwUser=0;
							lpVHdr.dwTimeCaptured=timeGetTime();
							doc->theTV->compressAFrame(&lpVHdr,TRUE/*doc->theTV->m_DShow->doPreview*/,this,doc->theHDD,&doc->theTV->biRawDef.bmiHeader);
							gdvFrameNum++;
							goto fine;
							}
error2:
						HeapFree(GetProcessHeap(),0,d);
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
						HeapFree(GetProcessHeap(),0,d);
						}
					}
				break;

			case 3:
				
				break;

			}

		}
fine:
		bInSend=FALSE;
		}
	if(*p) {
		if(theApp.m_pMainWnd)
			theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
		else
			GlobalFree(p);
		}
	else
		GlobalFree(p);
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
	if(theApp.Opzioni & CVidsendApp::saveLayout)
		d->savelayout();
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

// GESTITO come RAW, per ora, 2021 prove 
		/*if(GetDocument()->Opzioni & CVidsendDoc2::usaVBAN) {
			if(GetDocument()->streamSocketVBAN)
				GetDocument()->streamSocketVBAN->Manda(avh,lParam);
			}*/

		myTV->aFrameNum++;
		}

fine:
/*	if(avh)
		GlobalFree(avh);
	if(pSBuf)
		GlobalFree(pSBuf);*/

	return (LRESULT)retVal;
	}

afx_msg LRESULT CVidsendView2::OnRawAudioFrameReady(WPARAM wParam, LPARAM lParam) {
	CTV *myTV=GetDocument()->theTV;
	int retVal=0;
	MSG msg;

	// questo evita che si crei un accavallamento di messaggi del video a scapito degli altri di Windows...
//	if(PeekMessage(&msg,m_hWnd,WM_RAWAUDIOFRAME_READY,WM_AUDIOFRAME_READY,PM_NOREMOVE))
//		goto fine;

	if(GetDocument()->Opzioni & CVidsendDoc2::usaVBAN) {
		if(GetDocument()->streamSocketVBAN)
			retVal=GetDocument()->streamSocketVBAN->Manda((const void *)wParam,lParam);
		}

	// prova MP3 streaming, 2023
	if(GetDocument()->streamSocketA2)
		retVal=GetDocument()->streamSocketA2->Manda((const void *)wParam,lParam);


fine:
/*	if(avh)
		GlobalFree(avh);
	if(pSBuf)
		GlobalFree(pSBuf);*/

	return (LRESULT)retVal;
	}


BEGIN_MESSAGE_MAP(CButtonDrop, CButton)
	//{{AFX_MSG_MAP(CButtonDrop)
	ON_WM_DROPFILES()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CButtonDrop::PreSubclassWindow() {

	DragAcceptFiles(TRUE);
	}

void CButtonDrop::OnDropFiles(HDROP dropInfo) {
  CString    sFile;
  DWORD      nBuffer    = 0;
  // Get the number of files dropped
  UINT nFilesDropped    = DragQueryFile (dropInfo, 0xFFFFFFFF, NULL, 0);
	CVidsendDoc22 *pDoc=((CVidsendView22*)GetParent())->GetDocument();

  // If more than one, only use the first
  if(nFilesDropped > 0)    {
    // Get the buffer size for the first filename
    nBuffer = DragQueryFile(dropInfo, 0, NULL, 0);
    // Get path and name of the first file
    DragQueryFile (dropInfo, 0, sFile.GetBuffer (nBuffer + 1), nBuffer + 1);
    sFile.ReleaseBuffer();
    // Do something with the path
		//AfxMessageBox(sFile);
		if(sFile.Find(".mp3")>=0) {
			if(GetDlgCtrlID()==51)
				pDoc->m_Canzone1=sFile;
			else
				pDoc->m_Canzone2=sFile;
			GetParent()->Invalidate();
			}
		else
			MessageBeep(MB_ICONERROR);
		}
  // Free the memory block containing the dropped-file information
  DragFinish(dropInfo);
	}



CODBaseBtn::CODBaseBtn() : m_bMouseHover(false),m_bPressedState(false) {
	bState=0;
	mStyle=0;
	}

CODBaseBtn::~CODBaseBtn() {
	}

BEGIN_MESSAGE_MAP(CODBaseBtn, CButton)
    //{{AFX_MSG_MAP(CODBaseBtn)
    ON_WM_MOUSEMOVE()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONUP()
    //}}AFX_MSG_MAP
    ON_MESSAGE( WM_MOUSELEAVE, OnMouseLeave )
END_MESSAGE_MAP()

void CODBaseBtn::PreSubclassWindow() {

  // Initialize the flags
  m_bMouseHover = false;
  m_bPressedState = false;

  // Update the style to owner draw
	mStyle=GetStyle() & 0x00ff;			// i bit bassi passati a Create...
  ModifyStyle(0, BS_OWNERDRAW);
  CButton::PreSubclassWindow();
	}

void CODBaseBtn::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) {

  // Get the button rectangle and dimension
  CRect rect(lpDrawItemStruct->rcItem);
  int nWidth = rect.Width();
  int nHeight = rect.Height();

  // Prepare the main DC
  CDC dcOrig;
  dcOrig.Attach(lpDrawItemStruct->hDC);

  // Create a memory DC for flicker free drawing
  CDC dcMem;
  dcMem.CreateCompatibleDC(&dcOrig);
  CBitmap bitmap;
  bitmap.CreateCompatibleBitmap(&dcOrig, nWidth, nHeight);
  dcMem.SelectObject(&bitmap);

  // Check the pressed state
  bool bPressed = (0 != (lpDrawItemStruct->itemState & ODS_SELECTED));
// no    bPressed |= ( 0 != (lpDrawItemStruct->itemState & ODS_FOCUS));

	int i=lpDrawItemStruct->itemState & ODS_CHECKED;
/*ODS_SELECTED                         equ 1h
ODS_GRAYED                           equ 2h
ODS_DISABLED                         equ 4h
ODS_CHECKED                          equ 8h
ODS_FOCUS                            equ 10h
ODS_DEFAULT                       equ 20h
ODS_COMBOBOXEDIT                  equ 1000h
ODS_HOTLIGHT                      equ 40h
ODS_INACTIVE                      equ 80h*/

    // Call the appropriate functions as per the state
    if(lpDrawItemStruct->itemState & ODS_DISABLED) // Disabled state
    {
        OnDrawDisabled(dcMem, rect);
			}
    else    {
        if(bPressed) // Pressed state
        {
            OnDrawPressed(dcMem, rect);
				  }
        else        {
            if(m_bMouseHover) // Mouse cursor is above the button
            {
                OnDrawHovered(dcMem, rect);
					    }
            else // Normal state
            {
                OnDrawNormal(dcMem, rect);
						  }
					}
			}

    // Copy from memory DC to original DC
    dcOrig.BitBlt(0, 0, nWidth, nHeight, &dcMem, 0, 0, SRCCOPY);

    // Detach the original DC from the CDC object
    dcOrig.Detach();

    // Pressed/Released action should be performed
    if(m_bPressedState != bPressed)    {
        if(bPressed) {
            OnPressed();
			    }
        else {
            OnReleased();
		      }
    m_bPressedState = bPressed;
    }
	}

void CODBaseBtn::OnMouseMove(UINT nFlags, CPoint point) {

    // If the flag is false then this mouse message was sending for the first time.
    // So it can be taken as mouse entering the client area of the button
    if(!m_bMouseHover)     {
        // Set the flag so that next mouse move message will be neglected
        m_bMouseHover = true;

        // Invoke the call so that derived class can know this mouse hover
        OnBeginHover();

        // Track the mouse leave message
        TRACKMOUSEEVENT stTME = { 0 };
        stTME.cbSize    = sizeof( stTME );
        stTME.hwndTrack = m_hWnd;
        stTME.dwFlags   = TME_LEAVE;
        _TrackMouseEvent( &stTME );

        // Update the button
        Invalidate( FALSE );
    }

  CButton::OnMouseMove(nFlags, point);
	}

LRESULT CODBaseBtn::OnMouseLeave(WPARAM, LPARAM) {

  // Invoke the call so that derived class can know this mouse leave
  OnEndHover();
  // Reset the flag
  m_bMouseHover = false;
  // Update the button
  Invalidate(FALSE);
  return 0;
	}

BOOL CODBaseBtn::OnEraseBkgnd(CDC* pDC) {

  // Skip it for flicker free drawing
  return TRUE;
	}

void CODBaseBtn::OnLButtonDblClk(UINT nFlags, CPoint point) {
  // Convert the double click to single click
  const MSG* pstMSG = GetCurrentMessage();
  DefWindowProc(WM_LBUTTONDOWN, pstMSG->wParam, pstMSG->lParam);
	}

void CODBaseBtn::OnLButtonUp(UINT nFlags, CPoint point) {
	
	if(mStyle==BS_AUTOCHECKBOX /*BS_AUTORADIOBUTTON*/)
		bState ^= BST_CHECKED;
	CButton::OnLButtonUp(nFlags,point);
	}

void CODBaseBtn::OnRButtonUp(UINT nFlags, CPoint point) {
	// https://www.go4expert.com/articles/mouse-button-event-handler-t381/
//	GetParent()->PostMessage(WM_COMMAND,GetDlgCtrlID(),0);
//	CButton::OnRButtonUp(nFlags,point);

	NMHDR hdr;
  hdr.code = NM_RCLICK;
  hdr.hwndFrom = GetSafeHwnd();
  hdr.idFrom = GetDlgCtrlID();
  TRACE("OnRButtonUp");
  GetParent()->SendMessage(WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
	}


CRoundButton::CRoundButton(DWORD style, COLORREF fore, COLORREF fore2, COLORREF back) { 
	
	m_start=m_end=-1;

	m_crTextColor=fore;
	m_crOutlineColor=fore2;
	m_crBkColor=back;
	mStyle=style;
	}

CRoundButton::~CRoundButton() {
	}

COLORREF CRoundButton::darken(COLORREF c) {
	COLORREF c2;
	short int r=GetRValue(c);
	short int g=GetGValue(c);
	short int b=GetBValue(c);

	r=max(0,r-COLOR_VARIATION);
	g=max(0,g-COLOR_VARIATION);
	b=max(0,b-COLOR_VARIATION);

	c2=RGB(r,g,b);
	return c2;
	}
COLORREF CRoundButton::lighten(COLORREF c) {
	COLORREF c2;
	short int r=GetRValue(c);
	short int g=GetGValue(c);
	short int b=GetBValue(c);

	r=min(255,r+COLOR_VARIATION);
	g=min(255,g+COLOR_VARIATION);
	b=min(255,b+COLOR_VARIATION);

	c2=RGB(r,g,b);
	return c2;
	}

void CRoundButton::OnDrawNormal(CDC& dc, CRect rect) {
  CString csCaption;
	CBrush m_brBkgnd;
	CPen m_Pen;
	BYTE pensize;

  GetWindowText(csCaption);
	pensize=rect.Width()/24;
	if(!pensize)
		pensize=1;

	if(bState & 1) {
		m_brBkgnd.CreateSolidBrush(darken(m_crBkColor)); // Create the Brush Color for the Background.
		m_Pen.CreatePen(PS_SOLID,pensize,m_crOutlineColor);
		dc.SelectObject(m_brBkgnd);
		}
	else {
		m_brBkgnd.CreateSolidBrush(m_crBkColor);
		m_Pen.CreatePen(PS_SOLID,pensize,m_crOutlineColor);
		dc.SelectObject(m_brBkgnd);
		}
//    dc.FillSolidRect( &rect, RGB( 192, 192, 192 ));
	if(m_start<0 || m_end<0) {
		dc.SelectObject(m_Pen);
		dc.Ellipse(rect);
		}
	else {
		POINT pt1,pt2;
		CPen pen2(PS_SOLID,pensize,m_crBkColor);
		dc.SelectObject(pen2);
		dc.Ellipse(rect);
		pt1.x=rect.right/2+sin((double)m_start/VALUES_RANGE*2*PI)*rect.right/2;
		pt1.y=rect.bottom/2-cos((double)m_start/VALUES_RANGE*2*PI)*rect.bottom/2;
		pt2.x=rect.right/2+sin((double)m_end/VALUES_RANGE*2*PI)*rect.right/2;
		pt2.y=rect.bottom/2-cos((double)m_end/VALUES_RANGE*2*PI)*rect.bottom/2;
		dc.SelectObject(m_Pen);
		dc.Arc(rect,pt1,pt2);
		}
//    dc.Draw3dRect( &rect, RGB( 222, 222, 222 ), RGB( 160, 160, 160 ));
  dc.SetBkMode(TRANSPARENT);
  dc.SetTextColor(m_crTextColor);
	dc.SelectObject(GetFont());
  dc.DrawText(csCaption,&rect,/*DT_WORDBREAK*/ DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}

void CRoundButton::OnDrawHovered(CDC& dc, CRect rect) {
  CString csCaption;
	CBrush m_brBkgnd;
	CPen m_Pen;
	BYTE pensize;

	pensize=rect.Width()/24;
	if(!pensize)
		pensize=1;

	m_brBkgnd.CreateSolidBrush(lighten(m_crBkColor));
	m_Pen.CreatePen(PS_SOLID,pensize,m_crOutlineColor);
  GetWindowText(csCaption);

	dc.SelectObject(m_brBkgnd);
	dc.SelectObject(m_Pen);
//  dc.FillSolidRect( &rect, RGB( 210, 210, 210 ));
	if(m_start<0 || m_end<0) {
		dc.SelectObject(m_Pen);
		dc.Ellipse(rect);
		}
	else {
		POINT pt1,pt2;
		CPen pen2(PS_SOLID,pensize,m_crBkColor);
		dc.SelectObject(pen2);
		dc.Ellipse(rect);
		pt1.x=rect.right/2+sin((double)m_start/VALUES_RANGE*2*PI)*rect.right/2;
		pt1.y=rect.bottom/2-cos((double)m_start/VALUES_RANGE*2*PI)*rect.bottom/2;
		pt2.x=rect.right/2+sin((double)m_end/VALUES_RANGE*2*PI)*rect.right/2;
		pt2.y=rect.bottom/2-cos((double)m_end/VALUES_RANGE*2*PI)*rect.bottom/2;
		dc.SelectObject(m_Pen);
		dc.Arc(rect,pt1,pt2);
		}
//  dc.Draw3dRect( &rect, RGB( 232, 232, 232 ), RGB( 140, 140, 140 ));
  dc.SetBkMode(TRANSPARENT);
  dc.SetTextColor(RGB(0,0,0));
	dc.SelectObject(GetFont());
  dc.DrawText(csCaption, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}

void CRoundButton::OnDrawPressed(CDC& dc, CRect rect) {
  CString csCaption;
	CBrush m_brBkgnd;
	CPen m_Pen;
	BYTE pensize;

	pensize=rect.Width()/24;
	if(!pensize)
		pensize=1;

	m_Pen.CreatePen(PS_SOLID,pensize,m_crOutlineColor);
	m_brBkgnd.CreateSolidBrush(darken(m_crBkColor));
  GetWindowText(csCaption);

	dc.SelectObject(m_brBkgnd);
	dc.SelectObject(m_Pen);
//  dc.FillSolidRect( &rect, RGB( 222, 222, 222 ));
	if(m_start<0 || m_end<0) {
		dc.SelectObject(m_Pen);
		dc.Ellipse(rect);
		}
	else {
		POINT pt1,pt2;
		CPen pen2(PS_SOLID,pensize,m_crBkColor);
		dc.SelectObject(pen2);
		dc.Ellipse(rect);
		pt1.x=rect.right/2+sin((double)m_start/VALUES_RANGE*2*PI)*rect.right/2;
		pt1.y=rect.bottom/2-cos((double)m_start/VALUES_RANGE*2*PI)*rect.bottom/2;
		pt2.x=rect.right/2+sin((double)m_end/VALUES_RANGE*2*PI)*rect.right/2;
		pt2.y=rect.bottom/2-cos((double)m_end/VALUES_RANGE*2*PI)*rect.bottom/2;
		dc.SelectObject(m_Pen);
		dc.Arc(rect,pt1,pt2);
		}
//  dc.Draw3dRect( &rect, RGB( 128, 128, 128 ), RGB( 255, 255, 255 ));
  dc.SetBkMode(TRANSPARENT);
  dc.SetTextColor(RGB( 0, 0, 0));
	dc.SelectObject(GetFont());
  dc.DrawText(csCaption, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}

void CRoundButton::OnDrawDisabled(CDC& dc, CRect rect) {
  CString csCaption;
	CBrush m_brBkgnd;
	CPen m_Pen;
	BYTE pensize;

	pensize=rect.Width()/24;
	if(!pensize)
		pensize=1;

	m_Pen.CreatePen(PS_SOLID,pensize,m_crOutlineColor);
	m_brBkgnd.CreateSolidBrush(m_crBkColor);
  GetWindowText(csCaption);

	dc.SelectObject(m_brBkgnd);
	dc.SelectObject(m_Pen);
//  dc.FillSolidRect( &rect, RGB( 160, 160, 160 ));
	if(m_start<0 || m_end<0) {
		dc.SelectObject(m_Pen);
		dc.Ellipse(rect);
		}
	else {
		POINT pt1,pt2;
		CPen pen2(PS_SOLID,pensize,m_crBkColor);
		dc.SelectObject(pen2);
		dc.Ellipse(rect);
		pt1.x=rect.right/2+sin((double)m_start/VALUES_RANGE*2*PI)*rect.right/2;
		pt1.y=rect.bottom/2-cos((double)m_start/VALUES_RANGE*2*PI)*rect.bottom/2;
		pt2.x=rect.right/2+sin((double)m_end/VALUES_RANGE*2*PI)*rect.right/2;
		pt2.y=rect.bottom/2-cos((double)m_end/VALUES_RANGE*2*PI)*rect.bottom/2;
		dc.SelectObject(m_Pen);
		dc.Arc(rect,pt1,pt2);
		}
//  dc.Draw3dRect( &rect, RGB( 128, 128, 128 ), RGB( 128, 128, 128 ));
  dc.SetBkMode(TRANSPARENT);
  dc.SetTextColor(RGB( 128, 128, 128));
	dc.SelectObject(GetFont());
  dc.DrawText(csCaption, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}


CShapeButton::CShapeButton(COLORREF fore, COLORREF back) { 

	m_crTextColor=fore;
	m_crBkColor=back;
	}

CShapeButton::~CShapeButton() {
	}

void CShapeButton::OnDrawNormal( CDC& dc, CRect rect ){
  CString csCaption;
  GetWindowText(csCaption);

//	if(bState & 1)
//	  dc.FillSolidRect(&rect,RGB(32,32,32));
//	else
	  dc.FillSolidRect(&rect,m_crBkColor);
  dc.Draw3dRect( &rect, RGB( 222, 222, 222 ), RGB( 160, 160, 160 ));
  dc.SetBkMode(TRANSPARENT);
  dc.SetTextColor(m_crTextColor);
	dc.SelectObject(GetFont());
	if(bState & 1)
	  dc.DrawText(csCaption,&rect,DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}

void CShapeButton::OnDrawHovered(CDC& dc, CRect rect) {
  CString csCaption;

  GetWindowText(csCaption);

  dc.FillSolidRect( &rect, RGB( 210, 210, 210 ));
  dc.Draw3dRect( &rect, RGB( 232, 232, 232 ), RGB( 140, 140, 140 ));
  dc.SetBkMode(TRANSPARENT);
  dc.SetTextColor(RGB(0,0,0));
	dc.SelectObject(GetFont());
//	if(bState & 1)
  dc.DrawText(csCaption, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}

void CShapeButton::OnDrawPressed(CDC& dc, CRect rect) {
  CString csCaption;

  GetWindowText(csCaption);

  dc.FillSolidRect( &rect, RGB( 222, 222, 222 ));
  dc.Draw3dRect( &rect, RGB( 128, 128, 128 ), RGB( 255, 255, 255 ));
  dc.SetBkMode(TRANSPARENT);
  dc.SetTextColor(RGB( 0, 0, 0));
	dc.SelectObject(GetFont());
//	if(bState & 1)
  dc.DrawText(csCaption, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}

void CShapeButton::OnDrawDisabled(CDC& dc, CRect rect) {
  CString csCaption;

  GetWindowText(csCaption);

  dc.FillSolidRect( &rect, RGB( 160, 160, 160 ));
  dc.Draw3dRect( &rect, RGB( 128, 128, 128 ), RGB( 128, 128, 128 ));
  dc.SetBkMode(TRANSPARENT);
  dc.SetTextColor(RGB( 128, 128, 128));
	dc.SelectObject(GetFont());
	if(bState & 1)
	  dc.DrawText(csCaption, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}


BEGIN_MESSAGE_MAP(CStaticDrop, CStatic)
	//{{AFX_MSG_MAP(CStaticDrop)
	ON_WM_DROPFILES()
	ON_WM_CTLCOLOR_REFLECT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CStaticDrop::PreSubclassWindow() {

	m_crBkColor = RGB(24,16,16); // Initializing the Background Color to the system face color.
	m_crTextColor = RGB(224,224,224); // Initializing the text to Black
	m_brBkgnd.CreateSolidBrush(m_crBkColor); // Create the Brush Color for the Background.
	DragAcceptFiles(TRUE);
	}

HBRUSH CStaticDrop::CtlColor(CDC* pDC, UINT nCtlColor) {
	HBRUSH hbr;

	hbr = (HBRUSH)m_brBkgnd; // Passing a Handle to the Brush
	pDC->SetBkColor(m_crBkColor); // Setting the Color of the Text Background to the one passed by the Dialog
	pDC->SetTextColor(m_crTextColor); // Setting the Text Color to the one Passed by the Dialog
	return hbr;
	}

#if 0
void CStaticDrop::OnPaint() {
	CPaintDC dc(this);
	CString s="piciu";
	CRect rect;

	CStatic::OnPaint();
/*	GetClientRect(&rect);

	CRgn ClipRgn;
if (ClipRgn.CreateRectRgnIndirect(&rClient))
{
    pDC->SelectClipRgn(&ClipRgn);
}

	GetWindowText(s);

	dc.SetTextColor(RGB(224,224,224));
	dc.SetBkColor(RGB(16,8,8));

	dc.SetTextAlign(TA_BASELINE | TA_CENTER);
	dc.TextOut(rect.right / 2, rect.bottom / 2, s, s.GetLength());*/
	}
#endif

void CStaticDrop::OnDropFiles(HDROP dropInfo) {
  CString    sFile;
  DWORD      nBuffer    = 0;
  // Get the number of files dropped
  UINT nFilesDropped    = DragQueryFile (dropInfo, 0xFFFFFFFF, NULL, 0);
	CVidsendDoc22 *pDoc=((CVidsendView22*)GetParent())->GetDocument();

  // If more than one, only use the first
  if(nFilesDropped > 0)    {
    // Get the buffer size for the first filename
    nBuffer = DragQueryFile(dropInfo, 0, NULL, 0);
    // Get path and name of the first file
    DragQueryFile (dropInfo, 0, sFile.GetBuffer (nBuffer + 1), nBuffer + 1);
    sFile.ReleaseBuffer();
    // Do something with the path
		//AfxMessageBox(sFile);
		if(sFile.Find(".mp3")>=0 || sFile.Find(".m3u")>=0) {
			if(GetDlgCtrlID()==30 && ((CVidsendView22*)GetParent())->buttonC1->IsWindowEnabled())
				pDoc->m_Canzone1=sFile;
			if(GetDlgCtrlID()==31 && ((CVidsendView22*)GetParent())->buttonC2->IsWindowEnabled())
				pDoc->m_Canzone2=sFile;
			GetParent()->Invalidate();
			}
		else
			MessageBeep(MB_ICONERROR);
		}
  // Free the memory block containing the dropped-file information
  DragFinish(dropInfo);
	}


BEGIN_MESSAGE_MAP(CClickableProgressCtrl, CProgressCtrl)
    //{{AFX_MSG_MAP(CClickableProgressCtrl)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDOWN()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CClickableProgressCtrl::GetClickPosition() { 
	RECT rc;
	int m,M;

	GetClientRect(&rc);
	GetRange(m,M);
	return (ClickPosition*(M-m))/rc.right;
	}

void CClickableProgressCtrl::OnMouseMove(UINT nFlags, CPoint point) {

	CProgressCtrl::OnMouseMove(nFlags,point);
	}

void CClickableProgressCtrl::OnLButtonDown(UINT nFlags, CPoint point) {
	
	NMHDR hdr;
  hdr.code = NM_CLICK;
  hdr.hwndFrom = GetSafeHwnd();
  hdr.idFrom = GetDlgCtrlID();
	ClickPosition=point.x;
  TRACE("OnLButtonDown");
  GetParent()->SendMessage(WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
	CProgressCtrl::OnLButtonDown(nFlags,point);
	}

void CClickableProgressCtrl::OnLButtonUp(UINT nFlags, CPoint point) {
	
/*	NMHDR hdr;
  hdr.code = NM_LCLICK;
  hdr.hwndFrom = GetSafeHwnd();
  hdr.idFrom = GetDlgCtrlID();
  TRACE("OnLButtonUp");
  GetParent()->SendMessage(WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);*/
	CProgressCtrl::OnLButtonUp(nFlags,point);
	}


BEGIN_MESSAGE_MAP(CSliderCtrlSmart, CSliderCtrl)
    //{{AFX_MSG_MAP(CSliderCtrlSmart)
		ON_WM_CREATE()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONUP()
		ON_WM_MOUSEWHEEL()
		ON_WM_MOUSEMOVE()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CSliderCtrlSmart::OnCreate(LPCREATESTRUCT lpCreateStruct) {

	if(CSliderCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;

  DWORD dwClssStyle = GetClassLong(m_hWnd, GCL_STYLE);
  dwClssStyle |= CS_DBLCLKS;
  dwClssStyle = SetClassLong(m_hWnd, GCL_STYLE, (LONG)dwClssStyle);
	return 0;
	}

CSliderCtrlSmart::CSliderCtrlSmart() {
	}

void CSliderCtrlSmart::OnLButtonDblClk(UINT nFlags, CPoint point) {
	
	NMHDR hdr;
  hdr.code = NM_LDBLCLICK;
  hdr.hwndFrom = GetSafeHwnd();
  hdr.idFrom = GetDlgCtrlID();
  TRACE("OnLButtonDblClk");
  GetParent()->SendMessage(WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);

	CSliderCtrl::OnLButtonDblClk(nFlags,point);
	}
// bah alla fine non so quale dei 2 è meglio... dblclk rischia di prenderlo mentre ti sposti!
void CSliderCtrlSmart::OnRButtonUp(UINT nFlags, CPoint point) {
	
	NMHDR hdr;
  hdr.code = NM_RCLICK;
  hdr.hwndFrom = GetSafeHwnd();
  hdr.idFrom = GetDlgCtrlID();
  TRACE("OnRButtonUp");
  GetParent()->SendMessage(WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
	}

BOOL CSliderCtrlSmart::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {

	if(!(nFlags & MK_MBUTTON))
		SetPos(GetPos()+zDelta/40);
	else
		SetPos(GetPos()+zDelta/15);
	return CSliderCtrl::OnMouseWheel(nFlags, zDelta, pt);
	}

void CSliderCtrlSmart::OnMouseMove(UINT nFlags, CPoint point) {

	SetFocus();
	CSliderCtrl::OnMouseMove(nFlags, point);
	}



/////////////////////////////////////////////////////////////////////////////
// CVidsendView22 - Finestra main audio server

IMPLEMENT_DYNCREATE(CVidsendView22, CView)

CVidsendView22 *CVidsendView22::pThis;
CVidsendView22::CVidsendView22() {
	int i;
	
	inClick=FALSE;
	inSound=0;
/*	busyAudio=CreateSemaphore(NULL,           // default security attributes
        0,  // initial count
        1,  // maximum count
        NULL);          // unnamed semaphore
				*/
//	busyAudio=new CSingleLock(&m_CritSection);
//	busyAudio=0;

/*	PSECURITY_DESCRIPTOR psd = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH); 
InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION);
SetSecurityDescriptorDacl(psd, TRUE, NULL, FALSE);

SECURITY_ATTRIBUTES sa = {0};
sa.nLength = sizeof(sa);
sa.lpSecurityDescriptor = psd;
sa.bInheritHandle = FALSE;
	busyAudio3=  CreateEvent(&sa, TRUE, TRUE, NULL);
	*/
	busyAudio3=CreateEvent(NULL, TRUE, TRUE, "porcodio");
//	busyAudio3=  OpenEvent(EVENT_ALL_ACCESS, TRUE, "porcodio");


	pThis=this;

	maxWaveoutSize=0;

	m_Mixer=NULL;

	m_DSound=NULL;
	m_DS=NULL;
	soundSample=NULL;
	soundLength=0;
	soundSampleStart=NULL;

	PBMP3from=PBMP3to=0;
	PBMP3buffer=NULL;
	PBMP3bufferSize=PBMP3bufferPointer=0;

	m_MP3player[0]=m_MP3player[1]=NULL;
	m_AudioThread=NULL;

	for(i=0; i<10; i++)
		button[i]=NULL;
	buttonP1=buttonP2=buttonS=NULL;
	buttonM1=buttonM2=buttonM3=buttonM4=NULL;
	buttonPre1=buttonPre2=buttonPre3=buttonPre4=NULL;
	buttonC1=buttonC2=NULL;
	buttonE1=NULL;
	volume1=volume2=volume3=volume4=fader=NULL;
	pitch1=pitch2=NULL;
	txtMP31=txtMP32=NULL;
	VUMeter1=VUMeter2=VUMeter3=VUMeter4=VUMeter5=NULL;
	progress1=progress2=NULL;

	EnableToolTips(TRUE);
	}

CVidsendView22::~CVidsendView22() {
	CVidsendDoc22 *d=GetDocument();
	int i;

/*	if(m_hWaveIn != (HWAVEIN)-1) {
		waveInReset(m_hWaveIn);
		waveInStop(m_hWaveIn);
		waveInClose(m_hWaveIn);
		}
	m_hWaveIn=NULL;*/

	delete playerPreascolto;

	stopMP3(0);
	stopMP3(1);
	delete m_MP3player[0]; m_MP3player[0]=NULL;
	delete m_MP3player[1]; m_MP3player[1]=NULL;
	if(d) {
		}
	if(m_hAcm)
		acmStreamClose(m_hAcm,0);
	m_hAcm=NULL;
	delete m_pwi;

	delete soundSample;

	delete m_DS;
	delete m_DSound;

	delete PBMP3buffer;

	for(i=0; i<10; i++)
		delete button[i];
	delete buttonP1;
	delete buttonP2;
	delete buttonS;
	delete buttonE1;
	delete buttonC1;
	delete buttonC2;
	delete buttonM1;
	delete buttonM2;
	delete buttonM3;
	delete buttonM4;
	delete buttonPre1;
	delete buttonPre2;
	delete buttonPre3;
	delete buttonPre4;
	delete volume1;
	delete volume2;
	delete volume3;
	delete volume4;
	delete fader;
	delete pitch1;
	delete pitch2;
	delete txtMP31;
	delete txtMP32;
	delete VUMeter1;
	delete VUMeter2;
	delete VUMeter3;
	delete VUMeter4;
	delete VUMeter5;
	delete progress1;
	delete progress2;

	}


BEGIN_MESSAGE_MAP(CVidsendView22, CView)
	//{{AFX_MSG_MAP(CVidsendView22)
	ON_WM_TIMER()
	ON_WM_KEYDOWN()
	ON_WM_MOUSEWHEEL()
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_VIDEO_CONNESSIONE11, OnConnessioni11)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_CONNESSIONE11, OnUpdateConnessioni11)
	ON_COMMAND(ID_CONNESSIONI_ELENCO, OnConnessioniElenco)
	ON_COMMAND(ID_AUDIO_CANZONE1, OnAudioCanzone1)
	ON_COMMAND(ID_AUDIO_CANZONE2, OnAudioCanzone2)
	ON_COMMAND(ID_AUDIO_PLAYLIST, OnAudioPlaylist)
	ON_COMMAND(ID_AUDIO_EQUALIZZATORE_FLAT, OnAudioEqualizzatoreflat)
	ON_UPDATE_COMMAND_UI(ID_AUDIO_EQUALIZZATORE_FLAT, OnUpdateAudioEqualizzatoreflat)
	ON_COMMAND(ID_AUDIO_EQUALIZZATORE_ROCK, OnAudioEqualizzatorerock)
	ON_UPDATE_COMMAND_UI(ID_AUDIO_EQUALIZZATORE_ROCK, OnUpdateAudioEqualizzatorerock)
	ON_COMMAND(ID_AUDIO_EQUALIZZATORE_POP, OnAudioEqualizzatorepop)
	ON_UPDATE_COMMAND_UI(ID_AUDIO_EQUALIZZATORE_POP, OnUpdateAudioEqualizzatorepop)
	ON_COMMAND(ID_AUDIO_EQUALIZZATORE_DANCE, OnAudioEqualizzatoredance)
	ON_UPDATE_COMMAND_UI(ID_AUDIO_EQUALIZZATORE_DANCE, OnUpdateAudioEqualizzatoredance)
	ON_COMMAND(ID_AUDIO_EQUALIZZATORE_CLASSICA, OnAudioEqualizzatoreclassica)
	ON_UPDATE_COMMAND_UI(ID_AUDIO_EQUALIZZATORE_CLASSICA, OnUpdateAudioEqualizzatoreclassica)
	ON_COMMAND_RANGE(ID_AUDIO_EFFETTISONORI_1, ID_AUDIO_EFFETTISONORI_12, OnAudioEffettisonori)
	ON_UPDATE_COMMAND_UI_RANGE(ID_AUDIO_EFFETTISONORI_1,ID_AUDIO_EFFETTISONORI_12, OnUpdateAudioEffettisonori)
	ON_COMMAND(ID_AUDIO_EFFETTISONORI_STOP, OnAudioEffettisonoristop)
	ON_UPDATE_COMMAND_UI(ID_AUDIO_EFFETTISONORI_STOP, OnUpdateAudioEffettisonoristop)
	ON_COMMAND_RANGE(ID_AUDIO_EFFETTISONORI_BANCO1,ID_AUDIO_EFFETTISONORI_BANCO3, OnAudioEffettisonoriBanco)
	ON_UPDATE_COMMAND_UI_RANGE(ID_AUDIO_EFFETTISONORI_BANCO1,ID_AUDIO_EFFETTISONORI_BANCO3, OnUpdateAudioEffettisonoriBanco)
	ON_COMMAND(ID_AUDIO_EFFETTISONORI_IMPOSTA, OnAudioEffettisonoriImposta)
	ON_WM_RBUTTONDOWN()
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_ERASEBKGND()
	ON_WM_HSCROLL()
	//}}AFX_MSG_MAP
	ON_CONTROL_RANGE(BN_CLICKED, 2, 10, OnAudioEffettisonori2)
	ON_NOTIFY_RANGE(NM_RCLICK, 2, 10, OnAudioEffettisonoriImpostaBtn)
	ON_BN_CLICKED(11, OnAudioEffettisonoriPlay1)
	ON_BN_CLICKED(12, OnAudioEffettisonoriPlay2)
	ON_BN_CLICKED(13, OnAudioEffettisonoriOnAir)
	ON_BN_CLICKED(15, OnAudioEffettisonoriMute1)
	ON_BN_CLICKED(16, OnAudioEffettisonoriMute2)
	ON_BN_CLICKED(17, OnAudioEffettisonoriMute3)
	ON_BN_CLICKED(18, OnAudioEffettisonoriMute4)
	ON_BN_CLICKED(70, OnAudioEffettisonoriMutePre1)
	ON_BN_CLICKED(71, OnAudioEffettisonoriMutePre2)
	ON_BN_CLICKED(72, OnAudioEffettisonoriMutePre3)
	ON_BN_CLICKED(51, OnAudioCanzone1)
	ON_BN_CLICKED(52, OnAudioCanzone2)
	ON_BN_CLICKED(55, OnAudioEffettisonoriImposta)
	ON_NOTIFY(NM_LDBLCLICK, 25, OnSliderPitch1)		// 
	ON_NOTIFY(NM_LDBLCLICK, 26, OnSliderPitch2)		// 
	ON_NOTIFY(NM_RCLICK, 25, OnSliderPitch1)		// 
	ON_NOTIFY(NM_RCLICK, 26, OnSliderPitch2)		// 
	ON_NOTIFY(NM_RCLICK, 24, OnSliderFader)		// 
	ON_NOTIFY(NM_CLICK, 60, OnProgressCanzone1)		// 
	ON_NOTIFY(NM_CLICK, 61, OnProgressCanzone2)
//	ON_WM_LBUTTONDOWN(61, OnProgressCanzone2)
	ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
	ON_WM_GETMINMAXINFO()
	ON_MESSAGE(WM_AUDIOFRAME_READY,OnAudioFrameReady)
	ON_MESSAGE(WM_RAWAUDIOFRAME_READY,OnRawAudioFrameReady)
	ON_MESSAGE(WM_MP3_FINISHED,OnMP3Finished)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendView22 drawing

BOOL CVidsendView22::OnEraseBkgnd(CDC* pDC) {
	CVidsendDoc22 *pDoc=GetDocument();
	RECT r;

	CWnd::OnEraseBkgnd(pDC);

	GetClientRect(&r);
	theApp.renderBitmap(pDC,IDB_MIXER,&r);
	return 1;
	}

void CVidsendView22::OnDraw(CDC* pDC) {
	CVidsendDoc22 *pDoc=GetDocument();
	RECT r;
	char myBuf[128];
	struct ID3_TAG id3;
	CStringEx S;

	GetClientRect(&r);
	if(CJoshuaMP3::GetFileInfo(pDoc->m_Canzone1,&id3)>0) {
		id3.titolo[sizeof(id3.titolo)-1]=0;
		id3.autore[sizeof(id3.autore)-1]=0;
		S=id3.titolo;
		S.TrimRight(' ');
		S+=" - ";
		S+=id3.autore;
		S.TrimRight(' ');
		txtMP31->SetWindowText(S);
		}
	else {
		_tcscpy(myBuf,(LPSTR)(LPCTSTR)pDoc->m_Canzone1);
		PathStripPath(myBuf);		//
		txtMP31->SetWindowText(myBuf);
		}
	if(CJoshuaMP3::GetFileInfo(pDoc->m_Canzone2,&id3)>0) {
		id3.titolo[sizeof(id3.titolo)-1]=0;
		id3.autore[sizeof(id3.autore)-1]=0;
		S=id3.titolo;
		S.TrimRight(' ');
		S+=" - ";
		S+=id3.autore;
		S.TrimRight(' ');
		txtMP32->SetWindowText(S);
		}
	else {
		_tcscpy(myBuf,(LPSTR)(LPCTSTR)pDoc->m_Canzone2);
		PathStripPath(myBuf);		//
		txtMP32->SetWindowText(myBuf);
		}

	CFont myFont,*oldFont;

	pDC->SetTextColor(RGB(222,222,222));
	pDC->SetBkColor(RGB(0,0,0));

	myFont.CreateFont(12,5,0,0,400,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_SWISS,"tahoma");

	oldFont=(CFont *)pDC->SelectObject(&myFont);

	S.Format("Banco %u",pDoc->bBanco+1);
	pDC->TextOut(r.right/1.29,r.bottom/1.16,S);

	pDC->SelectObject(oldFont);
	}

void CVidsendView22::OnEditCopy() {
	CVidsendDoc22 *pDoc=GetDocument();
	HANDLE h;
	BYTE *p;
	DWORD l,ti;
	
	if(pDoc && OpenClipboard()) {

	  }  
	
	}

void CVidsendView22::OnUpdateEditCopy(CCmdUI* pCmdUI) {
	CVidsendDoc22 *pDoc=GetDocument();
	
	pCmdUI->Enable(pDoc->trasmMode>0);
	}

void CVidsendView22::OnConnessioni11() {
	CVidsendDoc22 *pDoc=GetDocument();
	}

void CVidsendView22::OnUpdateConnessioni11(CCmdUI* pCmdUI) {
	CVidsendDoc22 *pDoc=GetDocument();
	
	}

void CVidsendView22::OnConnessioniElenco() {
	CVidsendDoc22 *pDoc=GetDocument();
	CString strResult;
	CString aIP,S,S1;
	UINT aPort;
	CStreamSrvSocket2 *myRoot;

	POSITION po,po1;

	po=theApp.theServer2 ? theApp.theServer2->streamSocketA2->cSockRoot.GetHeadPosition() : NULL;
	if(po) {
		do {
			po1=po;
			myRoot=theApp.theServer2->streamSocketA2->cSockRoot.GetNext(po);
			if(myRoot->m_hSocket != INVALID_SOCKET) {
				myRoot->GetPeerName(aIP,aPort);
//				S1=myRoot->connName;
				S1.Empty();

				S=aIP+"\t"+S1+"\t";
				S1.Format("%u",myRoot);
				S=S+S1+"\n";
				strResult += S;
				}
			} while(po);
		}

	AfxMessageBox(strResult);

	#if 0
	// fare con lista..
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

	po=theApp.theChat->chatSocket ? theApp.theChat->chatSocket->cSockRoot.GetHeadPosition() : NULL;
	if(po) {
		do {
			po1=po;
			myRoot=theApp.theChat->chatSocket->cSockRoot.GetNext(po);
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
#endif

	}

void CVidsendView22::OnAudioCanzone1() {
	CVidsendDoc22 *pDoc=GetDocument();
	CFileDialog myDlg(TRUE,"*.mp3",pDoc->m_Canzone1,OFN_HIDEREADONLY,"File audio (*.mp3)|*.mp3|Playlist (*.m3u)|*.m3u|Tutti i file (*.*)|*.*||",this);

	if(myDlg.DoModal() == IDOK) {
		pDoc->m_Canzone1=myDlg.GetPathName();
		Invalidate();
		}
	}

void CVidsendView22::OnAudioCanzone2() {
	CVidsendDoc22 *pDoc=GetDocument();
	CFileDialog myDlg(TRUE,"*.mp3",pDoc->m_Canzone2,OFN_HIDEREADONLY,"File audio (*.mp3)|*.mp3|Playlist (*.m3u)|*.m3u|Tutti i file (*.*)|*.*||",this);

	if(myDlg.DoModal() == IDOK) {
		pDoc->m_Canzone2=myDlg.GetPathName();
		Invalidate();
		}
	}

void CVidsendView22::OnAudioPlaylist() {
	SHELLEXECUTEINFO sei;
	int i,j;
	CString m_strURL;
	char szDocPath[256];

	m_strURL ="c:\\user\\public\\my music";
//	FOLDERID_Music

//https://docs.microsoft.com/it-it/windows/desktop/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
	{
	typedef HRESULT (WINAPI * FN_SHGETFOLDER)(HWND,LPTSTR,int,BOOL);
	const WORD CSIDL_MYMUSIC = 0x000D;
	HINSTANCE hinstLib;
	HMODULE hshell32;
	hinstLib = LoadLibrary("C:\\WINDOWS\\system32\\shell32.dll");
	if(hinstLib)
		hshell32 = GetModuleHandle("SHELL32.DLL");

	// SHGetFolderPath questa non la compila, qua... v. MSDN & O7FUNCGD
	if(hshell32) {
		FN_SHGETFOLDER fGetFolder = (FN_SHGETFOLDER)GetProcAddress(hshell32, "SHGetSpecialFolderPathA");
		if(fGetFolder) {
			if(fGetFolder(NULL,szDocPath,CSIDL_MYMUSIC,
				0))
				;
			else
				goto no_get_folder;
			}
		else {
no_get_folder:
			_tcscpy(szDocPath,"c:\\");
				}
			}
		else {
			_tcscpy(szDocPath,"c:\\");
			}

	}

	m_strURL=szDocPath;

	::ZeroMemory(&sei,sizeof(SHELLEXECUTEINFO));
	sei.cbSize = sizeof( SHELLEXECUTEINFO );		// Set Size
	sei.lpVerb = TEXT( "open" );					// Set Verb
	sei.lpFile = m_strURL;							// Set Target To Open
	sei.nShow = SW_SHOWNORMAL;						// Show Normal
	ShellExecuteEx(&sei);
	}

void CVidsendView22::OnAudioEqualizzatoreflat() {
	CVidsendDoc22 *pDoc=GetDocument();

	pDoc->m_Equalizzatore=0;
	if(m_MP3player[0])
		;
	if(m_MP3player[1])
		;
	}

void CVidsendView22::OnUpdateAudioEqualizzatoreflat(CCmdUI* pCmdUI) {
	CVidsendDoc22 *pDoc=GetDocument();
	
	pCmdUI->SetCheck(	pDoc->m_Equalizzatore==0);
	}

void CVidsendView22::OnAudioEqualizzatorerock() {
	CVidsendDoc22 *pDoc=GetDocument();

	pDoc->m_Equalizzatore=1;
	if(m_MP3player[0])
		;
	if(m_MP3player[1])
		;
	}

void CVidsendView22::OnUpdateAudioEqualizzatorerock(CCmdUI* pCmdUI) {
	CVidsendDoc22 *pDoc=GetDocument();
	
	pCmdUI->SetCheck(	pDoc->m_Equalizzatore==1);
	}

void CVidsendView22::OnAudioEqualizzatorepop() {
	CVidsendDoc22 *pDoc=GetDocument();

	pDoc->m_Equalizzatore=2;
	if(m_MP3player[0])
		;
	if(m_MP3player[1])
		;
	}

void CVidsendView22::OnUpdateAudioEqualizzatorepop(CCmdUI* pCmdUI) {
	CVidsendDoc22 *pDoc=GetDocument();
	
	pCmdUI->SetCheck(	pDoc->m_Equalizzatore==2);
	}

void CVidsendView22::OnAudioEqualizzatoredance() {
	CVidsendDoc22 *pDoc=GetDocument();

	pDoc->m_Equalizzatore=3;
	if(m_MP3player[0])
		;
	if(m_MP3player[1])
		;
	}

void CVidsendView22::OnUpdateAudioEqualizzatoredance(CCmdUI* pCmdUI) {
	CVidsendDoc22 *pDoc=GetDocument();
	
	pCmdUI->SetCheck(	pDoc->m_Equalizzatore==3);
	}

void CVidsendView22::OnAudioEqualizzatoreclassica() {
	CVidsendDoc22 *pDoc=GetDocument();

	pDoc->m_Equalizzatore=4;
	if(m_MP3player[0])
		;
	if(m_MP3player[1])
		;
	}

void CVidsendView22::OnUpdateAudioEqualizzatoreclassica(CCmdUI* pCmdUI) {
	CVidsendDoc22 *pDoc=GetDocument();
	
	pCmdUI->SetCheck(	pDoc->m_Equalizzatore==4);
	}

void CVidsendView22::OnAudioEffettisonori(UINT nID) {
	CVidsendDoc22 *pDoc=GetDocument();

//  bRtn = sndPlaySound(lpRes, SND_MEMORY | SND_ASYNC | SND_NODEFAULT | (bStop ? 0 : SND_NOSTOP));
  playSound(pDoc->WAVFiles[pDoc->bBanco][nID-ID_AUDIO_EFFETTISONORI_1],100/*volume4->GetPos()*/,nID-ID_AUDIO_EFFETTISONORI_1);
//  playSound(nID-ID_AUDIO_EFFETTISONORI_1,volume4->GetPos());

	// USARE convertTo22K8(const BYTE *pIn,DWORD dIn,const WAVEFORMATEX *wIn,const WAVEFORMATEX *wOut,BYTE *pOut,DWORD *dOut) {	 // converte da wIn (8 bit 8KHz) a 8 bit 22050, PCM
	// fare mixWaves(const BYTE *s,DWORD da,DWORD a,BYTE *d,DWORD l,DWORD in,short int vol) {   // se<>0 da dove a dove (nel primo) o solo la lunghezza se da=0
	}

void CVidsendView22::OnUpdateAudioEffettisonori(CCmdUI* pCmdUI) {
	CVidsendDoc22 *pDoc=GetDocument();

	pCmdUI->Enable(!pDoc->WAVFiles[pDoc->bBanco][pCmdUI->m_nID-ID_AUDIO_EFFETTISONORI_1].IsEmpty());
	}

void CVidsendView22::OnAudioEffettisonori2(UINT nID) {
	CVidsendDoc22 *pDoc=GetDocument();

  playSound(pDoc->WAVFiles[pDoc->bBanco][nID-2],100 /*volume4->GetPos()*/,nID-2);
	}


void CVidsendView22::OnAudioEffettisonoristop() {
	CVidsendDoc22 *pDoc=GetDocument();

  playSound((const BYTE *)NULL,0);
//  PlaySound(NULL,NULL,SND_ASYNC | SND_NODEFAULT);
	}

void CVidsendView22::OnUpdateAudioEffettisonoristop(CCmdUI* pCmdUI) {
		// attivare solo se sta suonando...
//	pCmdUI->SetCheck( );
	}

void CVidsendView22::OnAudioEffettisonoriBanco(UINT nID) {
	CVidsendDoc22 *pDoc=GetDocument();
	RECT rc;

	GetClientRect(&rc);
	rc.left=rc.right/1.5;		// vabbe' ;)
	rc.top=rc.bottom/1.5;

  pDoc->bBanco=nID-ID_AUDIO_EFFETTISONORI_BANCO1;

	UpdateBanco();

	InvalidateRect(&rc);
	}

void CVidsendView22::OnUpdateAudioEffettisonoriBanco(CCmdUI* pCmdUI) {
	CVidsendDoc22 *pDoc=GetDocument();
		
	pCmdUI->SetCheck(pDoc->bBanco==pCmdUI->m_nID-ID_AUDIO_EFFETTISONORI_BANCO1);
	}

void CVidsendView22::UpdateBanco() {
	CVidsendDoc22 *pDoc=GetDocument();
	int i,n;
	
	TCHAR path[MAX_PATH];
	// così si sposta nella cartella di lavoro! se non è specificato path nei suoni
	// v. anche playSound
	GetModuleFileName(NULL, path, MAX_PATH);
	PathRemoveFileSpec(path);
	SetCurrentDirectory(path);

 	for(i=0; i<12; i++) {
		CWave w(pDoc->WAVFiles[pDoc->bBanco][i]);
		n=w.GetSeconds();

		// aggiornare testo/label
		}
	}


void CVidsendView22::OnAudioEffettisonoriImposta() {
	CVidsendDoc22 *pDoc=GetDocument();
	CImpostaEffettiSonoriDlg myDlg(this);
	CString S;
	int i,j;
	
	if(myDlg.DoModal() == IDOK) {		// 
//		i=myDlg.m_Tab.GetCurSel();
  	for(j=0; j<3; j++) {
  		for(i=0; i<12; i++) {
				pDoc->WAVFiles[j][i]=myDlg.m_Wavs[j][i];
				}
			}
		}
	}

void CVidsendView22::OnAudioEffettisonoriImpostaBtn(UINT nID,NMHDR* p_notify_msg_ptr,LRESULT* p_result_ptr) {
	CVidsendDoc22 *pDoc=GetDocument();
	CFileDialog myDlg(TRUE,"*.wav",pDoc->WAVFiles[pDoc->bBanco][nID-2],OFN_HIDEREADONLY,"File audio (*.wav)|*.wav|Tutti i file (*.*)|*.*||",this);
	
	if(myDlg.DoModal() == IDOK) {
		pDoc->WAVFiles[pDoc->bBanco][nID-2]=myDlg.GetFileName();
		}
	}

double CVidsendView22::getVolumeFader(BYTE w) {
	int n;

	if(!w) {
		n=volume1->GetPos();
		if(fader->GetPos() > 0)
			n=(n*(50-fader->GetPos()))/50;
		}
	else {
		n=volume2->GetPos();
		if(fader->GetPos() < 0)
			n=(n*(50+fader->GetPos()))/50;
		}
	return n/100.0;
	}

void CVidsendView22::OnAudioEffettisonoriPlay1() {
	CVidsendDoc22 *pDoc=GetDocument();
	char *p;

	if(!m_MP3player[0]) {
		if(p=(char *)malloc(256)) {
			stopMP3(0);
			*(DWORD *)p=(DWORD)this;
			*(p+4)=0;
			_tcscpy(p+6,(LPSTR)(LPCTSTR)pDoc->m_Canzone1);
			*(p+5)=/*getVolumeFader(0)* */100;
			AfxBeginThread(CVidsendView22::suonaMP3,p);
			buttonP1->SetWindowText("II");
			buttonC1->EnableWindow(FALSE);
			pitch1->SetPos(0);
			pitch1->EnableWindow(TRUE);
			// m_MP3player[0]->totSec
			}
		}
	else {
		stopMP3(0);
		buttonP1->SetWindowText(">");
		VUMeter1->SetWindowText(0);
		progress1->SetPos(0);
		buttonC1->EnableWindow(TRUE);
		pitch1->SetPos(0);
		pitch1->EnableWindow(FALSE);
		}
	}

void CVidsendView22::OnAudioEffettisonoriPlay2() {
	CVidsendDoc22 *pDoc=GetDocument();
	char *p;

	if(!m_MP3player[1]) {
		if(p=(char *)malloc(256)) {
			stopMP3(1);
			*(DWORD *)p=(DWORD)this;
			*(p+4)=1;
			*(p+5)=/*getVolumeFader(1)**/100;
			_tcscpy(p+6,(LPSTR)(LPCTSTR)pDoc->m_Canzone2);
			AfxBeginThread(CVidsendView22::suonaMP3,p);
			buttonP2->SetWindowText("II");
			buttonC2->EnableWindow(FALSE);
			pitch2->SetPos(0);
			pitch2->EnableWindow(TRUE);
			// m_MP3player[1]->totSec
			}
		}
	else {
		stopMP3(1);
		buttonP2->SetWindowText(">");
		VUMeter2->SetWindowText(0);
		progress2->SetPos(0);
		buttonC2->EnableWindow(TRUE);
		pitch2->SetPos(0);
		pitch2->EnableWindow(FALSE);
		}
	}

void CVidsendView22::OnAudioEffettisonoriMute1() {

	if(GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
		}
	else {
		}
/*	if(m_MP3player[0]) {
		m_MP3player[0]->volume(buttonM1->GetState() & BST_CHECKED ? 0.0 : 
			getVolumeFader(0));
		} gestito in mixwaves/WIM_DATA*/
	}

void CVidsendView22::OnAudioEffettisonoriMute2() {

	if(GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
		}
	else {
		}
/*	if(m_MP3player[1]) {
		m_MP3player[1]->volume(buttonM2->GetState() & BST_CHECKED ? 0.0 : 
			getVolumeFader(1));
		} gestito in mixwaves/WIM_DATA*/
	}

void CVidsendView22::OnAudioEffettisonoriMute3() {

	if(GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
		}
	else {
		}
	}

void CVidsendView22::OnAudioEffettisonoriMute4() {

	if(GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
		}
	else {
		}
/*	if(m_DS) {
		m_DS->SetVolume(buttonM4->GetState() & BST_CHECKED ? DSBVOLUME_MIN : 
			volume4->GetPos()*40 +DSBVOLUME_MIN+6000);
		}*/
	}

void CVidsendView22::OnAudioEffettisonoriMutePre1() {

	if(GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
		if(m_MP3player[0]) 
			m_MP3player[0]->HWvolume((signed char)0,DSBVOLUME_MIN);
		}
	else {
		if(m_MP3player[0]) 
			m_MP3player[0]->HWvolume((signed char)0,!(buttonPre1->GetState() & BST_CHECKED) ? DSBVOLUME_MIN : 
				DSBVOLUME_MAX /*getVolumeFader(0)*/);
// no per non perdere il sync		m_MP3player[0]->enableOutput(1 /**/,buttonPre1->GetState() & BST_CHECKED ? FALSE : TRUE);
		}
	}

void CVidsendView22::OnAudioEffettisonoriMutePre2() {

	if(GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
		if(m_MP3player[1]) 
			m_MP3player[1]->HWvolume((signed char)0,DSBVOLUME_MIN);
		}
	else {
		if(m_MP3player[1]) 
			m_MP3player[1]->HWvolume((signed char)0,!(buttonPre2->GetState() & BST_CHECKED) ? DSBVOLUME_MIN : 
				DSBVOLUME_MAX /*getVolumeFader(1)*/);
		}
	//idem
	}

void CVidsendView22::OnAudioEffettisonoriMutePre3() {

	if(GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
			m_DS->SetVolume(DSBVOLUME_MIN);
		}
	else {
		if(m_DS)
			m_DS->SetVolume(!(buttonPre3->GetState() & BST_CHECKED) ? DSBVOLUME_MIN : 
				DSBVOLUME_MAX /*volume4->GetPos()*40 +DSBVOLUME_MIN  +6000*/);
		}
	}

void CVidsendView22::OnProgressCanzone1(NMHDR* pNotifyStruct, LRESULT* result) {

	if(m_MP3player[0]) 
		m_MP3player[0]->goTo(((CClickableProgressCtrl*)GetDlgItem(pNotifyStruct->idFrom))->GetClickPosition(),0);
	}

void CVidsendView22::OnProgressCanzone2(NMHDR* pNotifyStruct, LRESULT* result) {

	if(m_MP3player[1]) 
		m_MP3player[1]->goTo(((CClickableProgressCtrl*)GetDlgItem(pNotifyStruct->idFrom))->GetClickPosition(),0);
	}

void CVidsendView22::OnSliderFader(NMHDR* pNotifyStruct, LRESULT* result) {
	((CSliderCtrlSmart*)GetDlgItem(pNotifyStruct->idFrom))->SetPos(0);
/*	if(m_MP3player[0])
		m_MP3player[0]->volume(getVolumeFader(0));
	if(m_MP3player[1])
		m_MP3player[1]->volume(getVolumeFader(1));*/
	}

void CVidsendView22::OnSliderPitch1(NMHDR* pNotifyStruct, LRESULT* result) {
	((CSliderCtrlSmart*)GetDlgItem(pNotifyStruct->idFrom))->SetPos(0);
	if(m_MP3player[0]) {
		m_MP3player[0]->setPitch(1.0);
		}
	}

void CVidsendView22::OnSliderPitch2(NMHDR* pNotifyStruct, LRESULT* result) {
	((CSliderCtrlSmart*)GetDlgItem(pNotifyStruct->idFrom))->SetPos(0);
	if(m_MP3player[1]) {
		m_MP3player[1]->setPitch(1.0);
		}
	}

void CVidsendView22::OnAudioEffettisonoriOnAir() {
	CVidsendDoc22 *pDoc=GetDocument();
	pDoc->bPaused = !pDoc->bPaused;
			//	buttonS->GetState & BST_CHECKED
	((CChildFrame22 *)GetParent())->setStatusIcons();
	}

void CVidsendView22::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
	
//	if(nSBCode == SB_THUMBPOSITION) {
//		}
	if(pScrollBar == (CScrollBar*)volume1) {
		if(m_MP3player[0]) {
//			m_MP3player[0]->volume(getVolumeFader(0));
			}
		}
	else if(pScrollBar == (CScrollBar*)volume2) {
		if(m_MP3player[1]) {
//			m_MP3player[1]->volume(getVolumeFader(1));
			}
		}
	else if(pScrollBar == (CScrollBar*)volume3) {
// non va		m_Mixer->SetVolume(
//			MIXERLINE_COMPONENTTYPE_SRC_ANALOG /*MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED*/ /*MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY*/,
//			volume3->GetPos()*650,volume3->GetPos()*650,MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE /*MIXERLINE_COMPONENTTYPE_SRC_LINE*/);
// gestito in WIM_DATA volWaves
		}
	else if(pScrollBar == (CScrollBar*)volume4) {
		if(m_DS) {
//			m_DS->SetVolume(buttonPre3->GetState() & BST_CHECKED ? 0 : 
//				volume4->GetPos()*40 +DSBVOLUME_MIN  +6000);
			}
		}
	else if(pScrollBar == (CScrollBar*)fader) {
/*		if(m_MP3player[0])
			m_MP3player[0]->volume(getVolumeFader(0));
		if(m_MP3player[1])
			m_MP3player[1]->volume(getVolumeFader(1));*/
		}
	else if(pScrollBar == (CScrollBar*)pitch1) {
		if(m_MP3player[0]) {
			m_MP3player[0]->setPitch(pitch1->GetPos()>0 ? log10(pitch1->GetPos())/8+1 : 
				(pitch1->GetPos()<0 ? 1-log10(-pitch1->GetPos())/8 : 1.0));
			}
		}
	else if(pScrollBar == (CScrollBar*)pitch2) {
		if(m_MP3player[1]) {
			m_MP3player[1]->setPitch(pitch2->GetPos()>0 ? log10(pitch2->GetPos())/8+1 : 
				(pitch2->GetPos()<0 ? 1-log10(-pitch2->GetPos())/8 : 1.0));
			}
		}

	CView::OnHScroll(nSBCode, nPos, pScrollBar);
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendView22 diagnostics

#ifdef _DEBUG
void CVidsendView22::AssertValid() const {
	
	CView::AssertValid();
	}

void CVidsendView22::Dump(CDumpContext& dc) const {
	
	CView::Dump(dc);
	}

CVidsendDoc22 *CVidsendView22::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVidsendDoc22)));
	return (CVidsendDoc22 *)m_pDocument;
	}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendView22 message handlers

BOOL CVidsendView22::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) {
	int i;
	int x,y,xs,ys;
	CVidsendDoc22 *d;
	RECT rc,myRect;

	if(i=CView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext)) {

//		m_Mixer=new CAudioMixer(this /*m_pMainWnd*/);
		// METTERE in theApp !!
//		m_Mixer->SetVolume(MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE,58000,-1,MIXERLINE_COMPONENTTYPE_DST_WAVEIN);
		// scheda audio HD (XP) 2009!!!
//		m_Mixer->Choose(MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE);
//		non va su windows 7 ecc... 

		GetClientRect(&rc);		// qua non è ancora valido... v. OnSize
		x=0;y=0; xs=1; ys=1;
		for(i=0; i<10; i++) {
			char myBuf[2];
			button[i]=new CRoundButton;
			myBuf[0]=i+'0'; myBuf[1]=0;
			button[i]->Create(myBuf,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
				CRect(100+(i%3)*x,100+(i/3)*y,100+(i%3)*x+xs,100+(i/3)*y+ys),this,i+1);
			}
		buttonP1=new CRoundButton;
		buttonP1->Create(">",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,CRect(10,200,10+20,220),this,11);
		buttonP2=new CRoundButton;
		buttonP2->Create(">",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,CRect(50,200,50+20,220),this,12);
		buttonS=new CRoundButton(RGB(220,220,220),RGB(255,0,0),RGB(200,0,0));
		buttonS->Create("ON AIR",WS_CHILD|WS_VISIBLE|BS_MULTILINE|BS_AUTOCHECKBOX|BS_PUSHLIKE,
			CRect(100,10,100+40,30),this,13);
		buttonS->SetCheck(BST_CHECKED);
		buttonM1=new CShapeButton;
		buttonM1->Create("x",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX/*|BS_PUSHLIKE*/,CRect(10,200,10+20,220),this,15);
		buttonM2=new CShapeButton;
		buttonM2->Create("x",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX/*|BS_PUSHLIKE*/,CRect(10,200,10+20,220),this,16);
		buttonM3=new CShapeButton;
		buttonM3->Create("x",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX/*|BS_PUSHLIKE*/,CRect(10,200,10+20,220),this,17);
		buttonM4=new CShapeButton;
		buttonM4->Create("x",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX/*|BS_PUSHLIKE*/,CRect(10,200,10+20,220),this,18);
		buttonPre1=new CShapeButton(RGB(240,240,0));
		buttonPre1->Create("o",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX/*|BS_PUSHLIKE*/,CRect(10,200,10+20,220),this,70);
		buttonPre2=new CShapeButton(RGB(240,240,0));
		buttonPre2->Create("o",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX/*|BS_PUSHLIKE*/,CRect(10,200,10+20,220),this,71);
		buttonPre3=new CShapeButton(RGB(240,240,0));
		buttonPre3->Create("o",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX/*|BS_PUSHLIKE*/,CRect(10,200,10+20,220),this,72);
		buttonPre4=new CShapeButton(RGB(240,240,0));
		buttonPre4->Create("o",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX/*|BS_PUSHLIKE*/,CRect(10,200,10+20,220),this,73);
		buttonC1=new CButtonDrop;
		buttonC1->Create("+",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,CRect(10,200,10+20,220),this,51);
		buttonC2=new CButtonDrop;
		buttonC2->Create("+",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,CRect(10,200,10+20,220),this,52);
		buttonE1=new CButton;
		buttonE1->Create("Edit",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,CRect(10,200,10+20,220),this,55);
		volume1=new CSliderCtrlSmart;
		volume1->Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_BOTTOM|TBS_AUTOTICKS,CRect(100,310,100+40,330),this,20);
		volume1->SetRange(0,100);
		volume1->SetPos(70);
		volume1->SetTicFreq(10);
		volume2=new CSliderCtrlSmart;
		volume2->Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_BOTTOM|TBS_AUTOTICKS,CRect(100,310,100+40,330),this,21);
		volume2->SetRange(0,100);
		volume2->SetPos(70);
		volume2->SetTicFreq(10);
		volume3=new CSliderCtrlSmart;
		volume3->Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_BOTTOM|TBS_AUTOTICKS,CRect(100,310,100+40,330),this,22);
		volume3->SetRange(0,100);
		volume3->SetPos(100
//			m_Mixer->GetVolume(MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE /*MIXERLINE_COMPONENTTYPE_SRC_LINE*/)*655 
			);
		volume3->SetTicFreq(10);
		volume4=new CSliderCtrlSmart;
		volume4->Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_BOTTOM|TBS_AUTOTICKS,CRect(100,310,100+40,330),this,23);
		volume4->SetRange(0,100);
		volume4->SetPos(75);
		volume4->SetTicFreq(10);
		fader=new CSliderCtrlSmart;
		fader->Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_BOTH|TBS_AUTOTICKS,CRect(100,310,100+40,330),this,24);
		fader->SetRange(-50,50);
		fader->SetPos(0);
		fader->SetTicFreq(5);
		pitch1=new CSliderCtrlSmart;
		pitch1->Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_TOP|TBS_AUTOTICKS,CRect(100,310,100+40,330),this,25);
		pitch1->SetRange(-10,10);
		pitch1->SetPos(0);
		pitch1->SetTicFreq(2);
		pitch1->EnableWindow(FALSE);
		pitch2=new CSliderCtrlSmart;
		pitch2->Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_TOP|TBS_AUTOTICKS,CRect(100,310,100+40,330),this,26);
		pitch2->SetRange(-10,10);
		pitch2->SetPos(0);
		pitch2->SetTicFreq(2);
		pitch2->EnableWindow(FALSE);
#define SS_EDITCONTROL 0x00002000
		txtMP31=new CStaticDrop;
		txtMP31->Create("Brano 1",WS_CHILD|WS_VISIBLE|SS_EDITCONTROL,CRect(100,310,100+40,330),this,30);
		txtMP32=new CStaticDrop;
		txtMP32->Create("Brano 2",WS_CHILD|WS_VISIBLE|SS_EDITCONTROL,CRect(100,310,100+40,330),this,31);
		VUMeter1=new CVUMeter;
		VUMeter1->Create("A1",WS_VISIBLE | WS_CHILD /*|CVUMeter::dotOrBar */ /*CVUMeter::digitalOrAnalog*/ | CVUMeter::linearOrLogarithmic ,
			CRect(100,110,100+80,130),
			this,40,0,40);
		VUMeter2=new CVUMeter;
		VUMeter2->Create("A2",WS_VISIBLE | WS_CHILD | CVUMeter::linearOrLogarithmic,
			CRect(100,110,100+80,130),this,41,0,40);
		VUMeter3=new CVUMeter;
		VUMeter3->Create("A3",WS_VISIBLE | WS_CHILD | CVUMeter::linearOrLogarithmic,
			CRect(100,110,100+80,130),this,42,0,40);
		VUMeter4=new CVUMeter;
		VUMeter4->Create("A4",WS_VISIBLE | WS_CHILD | CVUMeter::linearOrLogarithmic,
			CRect(100,110,100+80,130),this,43,0,40);
		VUMeter5=new CVUMeter;
		VUMeter5->Create("A5",WS_VISIBLE | WS_CHILD | CVUMeter::linearOrLogarithmic,
			CRect(100,110,100+80,130),this,44,0,40);
		progress1=new CClickableProgressCtrl;
		progress1->Create(WS_VISIBLE | WS_CHILD,CRect(100,140,100+80,160),this,60);
		progress2=new CClickableProgressCtrl;
		progress2->Create(WS_VISIBLE | WS_CHILD,CRect(100,140,100+80,160),this,61);

		i=doConnect();
		}
	return i;
	}

int CVidsendView22::doConnect() {
	CVidsendDoc22 *d;

	d=GetDocument();
	if(d) {			// tanto per sicurezza...

		if(d->Opzioni & CVidsendDoc22::registerServer) {
			MSG msg;
			DWORD l;

			if(d->Opzioni & CVidsendDoc2::needAuthenticateServer && !d->directoryWWW.IsEmpty()) {
				CPasswordDlg myDlg;
rifo:
				d->authSocket=new CAuthCliSocket(d);
				if(!d->authSocket)
					return FALSE;
				myDlg.m_Nome=d->directoryWWWLogin;
				if(myDlg.DoModal() == IDOK) {
					char myBuf[128];

					if(!d->authSocket->Create())
						goto auth_not_avail;
					if(!d->authSocket->Connect(d->directoryWWW,AUTHENTICATION_SOCKET))
						goto auth_not_avail;
					d->myLogin=myDlg.m_Nome;
					d->myPassword=myDlg.m_Pasw;

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
							AfxMessageBox("Login OK!");
							break;
						default:		// ERRORE!
							goto auth_failed;
							break;
						}

					}			// if myDlg.DoModal...
				else {
					goto auth_failed;
					}
	//				cliSockCtrl->SendUserPass((char *)(LPCTSTR)myDlg.m_Nome,(char *)(LPCTSTR)myDlg.m_Pasw,myAuthClient.extraParm);
				}

			}

		if(d->Opzioni & CVidsendDoc22::openAudioOnConnect) {
			}
		else {
			d->openAudio(this);
			}
		}

	return 1;
	}

int CVidsendView22::endConnect() {
	CVidsendDoc22 *d;

	d=GetDocument();
	if(d && d->authSocket) {			// tanto per sicurezza...
		d->authSocket->Close();
		delete d->authSocket;
		d->authSocket=NULL;
		}
	return 1;
	}

void CVidsendView22::OnTimer(UINT nIDEvent) {
#if 0			// messo in WAVE callback!
	CVidsendDoc22 *doc=GetDocument();
  struct AV_PACKET_HDR *avh;
	char *p;
	BYTE *d,*d2,*s,*dOut;
	int i;
	long bSize;
	DWORD l,t;
	ACMSTREAMHEADER hhacm;
	static int divider2;
	static BOOL bInSend=0;

	p=(char *)GlobalAlloc(GPTR,1024);
	if(!bInSend) {
		bInSend=TRUE;
	if(!doc->bPaused) {
		switch(doc->trasmMode) {
			case 0:
				if(doc->alternaSource) {

					}
				break;

			case 2:

				if(1 /*!(divider % 2   )*/) {
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
							if(GetDocument()->Opzioni & CVidsendDoc22::sstereo) {
								}
							if(GetDocument()->Opzioni & CVidsendDoc22::mono) {
								}

							d=theApp.createTestWave(NULL,&wfex,&l,i,GetDocument()->pagProva.audioOpzioni & 2,
								!(GetDocument()->Opzioni & CVidsendDoc22::mono));
				//			l+=sizeof(WAVEFORMATEX);

							// FINIRE la gestione di  / AUDIO_BUFFER_DIVIDER, servono 2 timer o boh

				SendMessage(WM_RAWAUDIOFRAME_READY,(WPARAM)d,(LPARAM)l);
// OCCHIO! serve Send o il buffer viene cimito 


							dOut=(BYTE *)GlobalAlloc(GMEM_FIXED,doc->maxWaveoutSize +100 /*AV_PACKET_HDR_SIZE*/);
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
							hhacm.cbDstLength=doc->maxWaveoutSize;
				//			hhacm.cbDstLengthUsed=0;
							hhacm.dwDstUser=0;
							if(m_hAcm && !acmStreamPrepareHeader(m_hAcm,&hhacm,0)) {
								avh->tag=MAKEFOURCC('D','G','2','0');
								avh->type=AV_PACKET_TYPE_AUDIO;
			//					avh.psec=1000;
								avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
								avh->info=wfd.wf.wFormatTag;

								i=acmStreamConvert(m_hAcm,&hhacm,ACM_STREAMCONVERTF_BLOCKALIGN);
						//	wsprintf(myBuf,"convertito: %d, %d bytes",i,l);
						//	AfxMessageBox(myBuf);
								acmStreamUnprepareHeader(m_hAcm,&hhacm,0);

								avh->len=hhacm.cbDstLengthUsed;
								if(theApp.debugMode) 
									wsprintf(p,"AFrameT# %ld: lungo %ld (%ld)",gdaFrameNum++,hhacm.cbDstLengthUsed,l); 
								avh->reserved1=avh->reserved2=0;
								PostMessage(WM_AUDIOFRAME_READY,(WPARAM)dOut,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
								}
							else {
								HeapFree(GetProcessHeap(),0,dOut);
								}
							HeapFree(GetProcessHeap(),0,d);
							}
						}
					}
				break;

			case 1:
				if(doc->psAudio) {
					d=(BYTE *)GlobalAlloc(GMEM_FIXED,2048+AV_PACKET_HDR_SIZE  +100 /*AV_PACKET_HDR_SIZE*/);
					d2=d+AV_PACKET_HDR_SIZE;			// indirizzo del buffer DOPO la struct header!
					if(doc->psAudio->Read(d,2048)) {

						gdaFrameNum++;
						HeapFree(GetProcessHeap(),0,d);
						}
					else {
						gdaFrameNum=0;
						if(doc->OpzioniSorgenteVideo & CVidsendDoc22::mp3Loop) {
							doc->psAudio->Seek(0,CFile::begin);
							}
						else
							doc->setTXMode(2);
						HeapFree(GetProcessHeap(),0,d);
						}
					}
				break;

			}

		}
fine:
		bInSend=FALSE;
		}
	if(*p) {
		if(theApp.m_pMainWnd)
			theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
		else
			GlobalFree(p);
		}
	else
		GlobalFree(p);
#endif
	CView::OnTimer(nIDEvent);
	}

#if 0			// messo in WAVE callback!
UINT CVidsendView22::audioProva(LPVOID ptr) {
	CVidsendView22 *v=(CVidsendView22*)ptr;
	CVidsendDoc22 *doc=v->GetDocument();

	// strattona lo stesso... strano (al posto di Timer)

	char myBuf[256];

  struct AV_PACKET_HDR *avh;
	char *p;
	BYTE *d,*d2,*s,*dOut;
	BYTE myWAVbuf[44100L*2*2/AUDIO_BUFFER_DIVIDER];
	int i;
	long bSize;
	DWORD l,t;
	ACMSTREAMHEADER hhacm;
	static int divider2;

	v->m_AudioThread=AfxGetThread();
	while(1) {
	if(!doc->bPaused) {
		switch(doc->trasmMode) {
			case 0:
				if(doc->alternaSource) {

					}
				break;

			case 2:

				if(1 /*!(divider % 2   )*/) {
					divider2++;
					if(!(doc->pagProva.audioOpzioni & 1) || (divider2 & 1)) {
						if(doc->bAudio) {
							switch(doc->pagProva.tipoAudio) {
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
							if(doc->Opzioni & CVidsendDoc22::sstereo) {
								}
							if(doc->Opzioni & CVidsendDoc22::mono) {
								}

							d=theApp.createTestWave(NULL,&v->wfex,&l,i,doc->pagProva.audioOpzioni & 2,
								40,!(doc->Opzioni & CVidsendDoc22::mono));
				//			l+=sizeof(WAVEFORMATEX);

							// [FINIRE la gestione di  / AUDIO_BUFFER_DIVIDER, servono 2 timer o boh]
							i=v->measureAudio((short int *)d,l/2);
							v->VUMeter3->SetWindowText(i/2);

				v->SendMessage(WM_RAWAUDIOFRAME_READY,(WPARAM)d,(LPARAM)l);
// OCCHIO! serve Send o il buffer viene cimito 


							dOut=(BYTE *)GlobalAlloc(GMEM_FIXED,doc->maxWaveoutSize +100 /*AV_PACKET_HDR_SIZE*/);
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
							hhacm.cbDstLength=doc->maxWaveoutSize;
				//			hhacm.cbDstLengthUsed=0;
							hhacm.dwDstUser=0;
							if(v->m_hAcm && !acmStreamPrepareHeader(v->m_hAcm,&hhacm,0)) {
								avh->tag=MAKEFOURCC('D','G','2','0');
								avh->type=AV_PACKET_TYPE_AUDIO;
			//					avh.psec=1000;
								avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
								avh->info=v->wfd.wf.wFormatTag;

								i=acmStreamConvert(v->m_hAcm,&hhacm,ACM_STREAMCONVERTF_BLOCKALIGN);
						//	wsprintf(myBuf,"convertito: %d, %d bytes",i,l);
						//	AfxMessageBox(myBuf);
								acmStreamUnprepareHeader(v->m_hAcm,&hhacm,0);

								avh->len=hhacm.cbDstLengthUsed;
								avh->reserved1=avh->reserved2=0;
								v->PostMessage(WM_AUDIOFRAME_READY,(WPARAM)dOut,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
								}
							else {
								HeapFree(GetProcessHeap(),0,dOut);
								}
//							HeapFree(GetProcessHeap(),0,d);
							}
						}
					}
				break;

			case 1:
				if(doc->psAudio) {
					if(doc->nomeMP3_PB.FindNoCase(".mp3") != -1) {
						d=(BYTE *)GlobalAlloc(GMEM_FIXED,44100L*2*2/AUDIO_BUFFER_DIVIDER+AV_PACKET_HDR_SIZE  +100 /*AV_PACKET_HDR_SIZE*/);
						d2=d+AV_PACKET_HDR_SIZE;			// indirizzo del buffer DOPO la struct header!

						l=44100L*2*2/AUDIO_BUFFER_DIVIDER;
						if(!v->PBMP3bufferSize) {
							CJoshuaMP3 myMP3((DWORD)0);
//no							v->PBMP3to+=576*2  *10;		// (mi dà 82944 byte in output @44100/2/16)
							if(myMP3.suona(doc->psAudio, v->PBMP3buffer, v->PBMP3from, ++v->PBMP3to,0,0,0.7)) {
								v->PBMP3from=v->PBMP3to;
								v->PBMP3bufferPointer=0 /*myMP3.getBuffer()*/;
								v->PBMP3bufferSize=myMP3.getBufferPos();
								memcpy(myWAVbuf,v->PBMP3buffer+v->PBMP3bufferPointer,l);
								v->PBMP3bufferPointer+=l;
								}
							else
								l=0;			// fine brano..
							}
						else if(v->PBMP3bufferPointer+l >= v->PBMP3bufferSize) {
							CJoshuaMP3 myMP3((DWORD)0);
							memcpy(myWAVbuf,v->PBMP3buffer+v->PBMP3bufferPointer,
								v->PBMP3bufferSize-v->PBMP3bufferPointer);
							v->PBMP3bufferPointer += l;
							v->PBMP3bufferPointer %= v->PBMP3bufferSize;

//							v->PBMP3to+=576*2 *10;
							if(myMP3.suona(doc->psAudio, v->PBMP3buffer, v->PBMP3from, ++v->PBMP3to,0,0,0.7)) {
								v->PBMP3from=v->PBMP3to;
//								v->PBMP3bufferPointer=0 /*myMP3.getBuffer()*/;
								memcpy(myWAVbuf+l-v->PBMP3bufferPointer,v->PBMP3buffer,v->PBMP3bufferPointer);
								v->PBMP3bufferSize=myMP3.getBufferPos();
//								v->PBMP3bufferPointer+=l;
								}
							else
								l=0;			// fine brano..

//							v->PBMP3bufferSize=0;
							}
						else {
							memcpy(myWAVbuf,v->PBMP3buffer+v->PBMP3bufferPointer,l);
							v->PBMP3bufferPointer+=l;
							}
						}
					else {
						}

					if(l) {
						memcpy(d2,myWAVbuf,l);
						i=v->measureAudio((short int *)d2,l/2);
						v->VUMeter3->SetWindowText(i/2);

							// NO bisogna decomprimere e ricomprimere...
						v->SendMessage(WM_RAWAUDIOFRAME_READY,(WPARAM)myWAVbuf,(LPARAM)l);

#if 0 // fare volendo!
							avh=(struct AV_PACKET_HDR *)dOut;
							hhacm.cbStruct=sizeof(ACMSTREAMHEADER);
							hhacm.fdwStatus=0;
							hhacm.dwUser=(DWORD)0 /*this*/;
							hhacm.pbSrc=d;
							hhacm.cbSrcLength=l;
				//			hhacm.cbSrcLengthUsed=0;
							hhacm.dwSrcUser=0;
							hhacm.pbDst=d2;
							hhacm.cbDstLength=doc->maxWaveoutSize;
				//			hhacm.cbDstLengthUsed=0;
							hhacm.dwDstUser=0;
							if(v->m_hAcm && !acmStreamPrepareHeader(v->m_hAcm,&hhacm,0)) {
								avh->tag=MAKEFOURCC('D','G','2','0');
								avh->type=AV_PACKET_TYPE_AUDIO;
			//					avh.psec=1000;
								avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
								avh->info=v->wfd.wf.wFormatTag;

								i=acmStreamConvert(v->m_hAcm,&hhacm,ACM_STREAMCONVERTF_BLOCKALIGN);
						//	wsprintf(myBuf,"convertito: %d, %d bytes",i,l);
						//	AfxMessageBox(myBuf);
								acmStreamUnprepareHeader(v->m_hAcm,&hhacm,0);

								avh->len=hhacm.cbDstLengthUsed;
								avh->reserved1=avh->reserved2=0;
								v->PostMessage(WM_AUDIOFRAME_READY,(WPARAM)dOut,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
								}
							else {
								HeapFree(GetProcessHeap(),0,dOut);
								}
//							HeapFree(GetProcessHeap(),0,d);
							}
#endif


						v->gdaFrameNum++;
						HeapFree(GetProcessHeap(),0,d);
						}
					else {
						v->gdaFrameNum=0;
						if(doc->OpzioniSorgenteVideo & CVidsendDoc22::mp3Loop) {
							doc->psAudio->Seek(0,CFile::begin);
							v->PBMP3from=v->PBMP3to=0;
							v->PBMP3bufferSize=v->PBMP3bufferPointer=0;
							}
						else
							doc->setTXMode(2);
						HeapFree(GetProcessHeap(),0,d);
						}
					}
				break;

			}
		}


		Sleep(1000/AUDIO_BUFFER_DIVIDER);
		}

	return 1;
	}
#endif

void CVidsendView22::OnSize(UINT nType, int cx, int cy) {
	int i,x,y,xs,ys;
	CVidsendDoc22 *d=GetDocument();
	RECT rc;

	CView::OnSize(nType, cx, cy);

	GetClientRect(&rc);

	xs=rc.right/36; ys=rc.bottom/20;
	m_Font1.DeleteObject();
	m_Font1.CreateFont(ys,xs,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_SWISS,"Arial");
	xs=rc.right/60; ys=rc.bottom/30;
	m_Font2.DeleteObject();
	m_Font2.CreateFont(ys,xs,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_SWISS,"Arial");

	xs=rc.right/13.3; ys=rc.bottom/9.7;
	x=xs*1.56; y=ys*1.66;
	for(i=1; i<10; i++) {
		if(button[i]) {
			button[i]->MoveWindow(rc.right/1.51+((i-1)%3)*x,rc.bottom/2.63+((i-1)/3)*y,xs,ys,TRUE);
			button[i]->SetFont(&m_Font1);
			}
		}

	if(buttonS) {
		buttonS->MoveWindow(rc.right/2.25,rc.bottom/38,xs*1.6,ys*1.2,TRUE);
		buttonS->SetFont(&m_Font2);
		}
	xs=rc.right/17; ys=rc.bottom/12.5;
	if(buttonP1) {
		buttonP1->MoveWindow(rc.right/43,rc.bottom/2.3,xs,ys,TRUE);
		buttonP1->SetFont(&m_Font2);
		}
	if(buttonP2) {
		buttonP2->MoveWindow(rc.right/1.83,rc.bottom/2.3,xs,ys,TRUE);
		buttonP2->SetFont(&m_Font2);
		}
	if(buttonC1) {
		buttonC1->MoveWindow(rc.right/45,rc.bottom/3.91,xs,ys,TRUE);
		buttonC1->SetFont(&m_Font2);
		}
	if(buttonC2) {
		buttonC2->MoveWindow(rc.right/3.1,rc.bottom/3.91,xs,ys,TRUE);
		buttonC2->SetFont(&m_Font2);
		}
	if(buttonE1) {
		buttonE1->MoveWindow(rc.right/1.09,rc.bottom/3.7,xs,ys/1.7,TRUE);
		buttonE1->SetFont(&m_Font2);
		}
	xs=rc.right/32; ys=rc.bottom/28;
	if(buttonM1) {
		buttonM1->MoveWindow(rc.right/3.7,rc.bottom/2.31,xs,ys,TRUE);
		buttonM1->SetFont(&m_Font2);
		}
	if(buttonM2) {
		buttonM2->MoveWindow(rc.right/3.1,rc.bottom/2.31,xs,ys,TRUE);
		buttonM2->SetFont(&m_Font2);
		}
	if(buttonM3) {
		buttonM3->MoveWindow(rc.right/3.15,rc.bottom/1.29,xs,ys,TRUE);
		buttonM3->SetFont(&m_Font2);
		}
	if(buttonM4) {
		buttonM4->MoveWindow(rc.right/1.26,rc.bottom/3.6,xs,ys,TRUE);
		buttonM4->SetFont(&m_Font2);
		}
	//spostare SOPRA CUFFIE in grafica e sistemare con iconette!
	if(buttonPre1) {
		buttonPre1->MoveWindow(rc.right/12.5,rc.bottom/1.59,xs,ys,TRUE);
		buttonPre1->SetFont(&m_Font2);
		buttonPre1->SetColors(RGB(240,240,0),RGB(100,100,100));
		buttonPre1->SetCheck(TRUE);
		}
	if(buttonPre2) {
		buttonPre2->MoveWindow(rc.right/1.81,rc.bottom/1.59,xs,ys,TRUE);
		buttonPre2->SetFont(&m_Font2);
		buttonPre2->SetColors(RGB(240,240,0),RGB(100,100,100));
		buttonPre2->SetCheck(TRUE);
		}
	if(buttonPre3) {
		buttonPre3->MoveWindow(rc.right/1.14,rc.bottom/3.6,xs,ys,TRUE);
		buttonPre3->SetFont(&m_Font2);
		buttonPre3->SetColors(RGB(240,240,0),RGB(100,100,100));
		buttonPre3->SetCheck(TRUE);
		}
	if(buttonPre4) {
		buttonPre4->MoveWindow(rc.right/1.7,rc.bottom/1.29,xs,ys,TRUE);
		buttonPre4->SetFont(&m_Font2);
		buttonPre4->SetColors(RGB(240,240,0),RGB(100,100,100));
// questo no... Mic		buttonPre4->SetCheck(TRUE);
		}
	xs=rc.right/7; ys=rc.bottom/15;
	if(volume1)
		volume1->MoveWindow(rc.right/9.7,rc.bottom/2.4,xs,ys,TRUE);
	if(volume2)
		volume2->MoveWindow(rc.right/2.7,rc.bottom/2.4,xs,ys,TRUE);
	if(volume3)
		volume3->MoveWindow(rc.right/2.5,rc.bottom/1.31,xs,ys,TRUE);
	if(volume4)
		volume4->MoveWindow(rc.right/1.55,rc.bottom/4,xs,ys,TRUE);
	xs=rc.right/9; ys=rc.bottom/18;
	if(pitch1)
		pitch1->MoveWindow(rc.right/20,rc.bottom/1.85,xs,ys,TRUE);
	if(pitch2)
		pitch2->MoveWindow(rc.right/2.1,rc.bottom/1.85,xs,ys,TRUE);
	xs=rc.right/4.5; ys=rc.bottom/13;
	if(fader)
		fader->MoveWindow(rc.right/4.8,rc.bottom/1.75,xs,ys*1.40,TRUE);
	xs=rc.right/5; ys=rc.bottom/14;
	if(txtMP31) {
		txtMP31->MoveWindow(rc.right/12.2,rc.bottom/3.9,xs,ys,TRUE);
		txtMP31->SetFont(&m_Font2);
		txtMP31->SetWindowText(d->m_Canzone1);
		}
	if(txtMP32) {
		txtMP32->MoveWindow(rc.right/2.6,rc.bottom/3.9,xs,ys,TRUE);
		txtMP32->SetFont(&m_Font2);
		txtMP32->SetWindowText(d->m_Canzone2);
		}
	xs=rc.right/7; ys=rc.bottom/19;
	if(VUMeter1)
		VUMeter1->MoveWindow(rc.right/9.7,rc.bottom/2.07,xs,ys,TRUE);
	if(VUMeter2)
		VUMeter2->MoveWindow(rc.right/2.7,rc.bottom/2.07,xs,ys,TRUE);
	if(VUMeter3)
		VUMeter3->MoveWindow(rc.right/2.5,rc.bottom/1.2,xs,ys,TRUE);
	if(VUMeter4)
		VUMeter4->MoveWindow(rc.right/1.55,rc.bottom/3.25,xs,ys,TRUE);
	if(VUMeter5)
		VUMeter5->MoveWindow(rc.right/3,rc.bottom/1.055,xs,ys,TRUE);
	xs=rc.right/3.8; ys=rc.bottom/24;
	if(progress1)
		progress1->MoveWindow(rc.right/40,rc.bottom/2.9,xs,ys,TRUE);
	if(progress2)
		progress2->MoveWindow(rc.right/3.05,rc.bottom/2.9,xs,ys,TRUE);
	}

void CVidsendView22::OnRButtonDown(UINT nFlags, CPoint point) {
	CMenu myMenu;
	
	myMenu.LoadMenu(IDR_VIDSENTYPE22);
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



void CVidsendView22::OnLButtonDown(UINT nFlags, CPoint point) {
	
	inClick=TRUE;
	CView::OnLButtonDown(nFlags, point);
	}

void CVidsendView22::OnLButtonUp(UINT nFlags, CPoint point) {
	
	inClick=FALSE;
	CView::OnLButtonUp(nFlags, point);
	}

void CVidsendView22::OnMouseMove(UINT nFlags, CPoint point) {
	
//gestire spostamento dell'intera finestra, clickandoci dentro!
	// e magari anche il posizionamento del quality box??
	CView::OnMouseMove(nFlags, point);
	}

void CVidsendView22::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
	CVidsendDoc22 *d=GetDocument();

	switch(nChar) {
		case VK_SPACE:
			if(d->bPaused)
//				d->OnAudioTrasmissioneRiprendi();
				PostMessage(WM_COMMAND,ID_CONNESSIONE_RIPRENDI,0);
			else
				PostMessage(WM_COMMAND,ID_CONNESSIONE_PAUSA,0);
			break;
		}

	CView::OnKeyDown(nChar, nRepCnt, nFlags);
	}

BOOL CVidsendView22::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
	CVidsendDoc22 *d=GetDocument();

#if 0
	// si potrebbe cambiare a seconda di "dove" si trova il mouse...
	if(!(nFlags & MK_CONTROL)) {
		volume3->SetPos(volume3->GetPos()+zDelta/10);
		}
	else {
		fader->SetPos(fader->GetPos()+zDelta/10);
		}
#endif

	return CView::OnMouseWheel(nFlags, zDelta, pt);
	}
						
BOOL CVidsendView22::OnToolTipNotify(UINT /*id*/, NMHDR *pNMHDR, LRESULT * /*pResult*/) {
	TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
	TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
	CString strTipText = "";
	CString rString = "";
	UINT nID = pNMHDR->idFrom;
	CVidsendDoc22 *d=GetDocument();

	ASSERT(pNMHDR->code == TTN_NEEDTEXTA || pNMHDR->code == TTN_NEEDTEXTW);

// if there is a top level routing frame then let it handle the message
	if(GetRoutingFrame() != NULL) 
		return FALSE;


// to be through we will need to handle UNICODE versions of the message also !!
	if(pNMHDR->code == TTN_NEEDTEXTA && (pTTTA->uFlags & TTF_IDISHWND)) {
		// idFrom is actually the HWND of the tool
		UINT nIdentifier = ::GetDlgCtrlID((HWND)pNMHDR->idFrom);

		if(nIdentifier>=2  && nIdentifier<=10) {
//			pTTT->lpszText = " hallo - this is a tool tip text "; //m_oStrToolTip.GetBuffer(m_oStrToolTip.GetLength()) ;
			pTTT->lpszText = (LPSTR)(LPCTSTR)d->WAVFiles[d->bBanco][nIdentifier-2];
				// or directly set : pTTT->lpszText = " hallo - this is a tool tip text ";
			}
		else
			pTTT->lpszText = NULL;

		CWnd *p = CWnd::FromHandle(pNMHDR->hwndFrom);
		p->SendMessage(TTM_SETDELAYTIME, TTDT_AUTOPOP, 30000);
		//forced multiline
		p->SendMessage(TTM_SETDELAYTIME, TTDT_INITIAL, 0);
		p->SendMessage(TTM_SETMAXTIPWIDTH, 0, 100);
		//forced various time to show
		if(pTTT->lpszText)
			return TRUE ; // there is text to display
			}

		//set tooltips for toolbar
		if(nID != 0) {		// will be zero on a separator
			strTipText.LoadString(nID);

		if (pNMHDR->code == TTN_NEEDTEXTA) {
			lstrcpyn(pTTTA->szText, strTipText, sizeof(pTTTA->szText));
			}

		// bring the tooltip window above other popup windows
		::SetWindowPos(pNMHDR->hwndFrom, HWND_TOP, 0, 0, 0, 0,SWP_NOACTIVATE|
		SWP_NOSIZE|SWP_NOMOVE|SWP_NOOWNERZORDER); return TRUE;
		}

	return FALSE;
	} 

void CVidsendView22::OnClose() {		// non lo esegue mai...??
		int i=GetWindowLong(m_hWnd,GWL_STYLE);
		int j=IsZoomed();
		CView::OnClose();
	}
void CVidsendView22::OnDestroy() {
	CVidsendDoc22 *d=GetDocument();
	RECT rc;
	char myBuf[64];
	
	if(m_pwi)
		m_pwi->StopRecord();

	CView::OnDestroy();

	if(theApp.Opzioni & CVidsendApp::saveLayout)
		d->savelayout();
/*	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		GetParent()->GetWindowRect(&rc);	// coord. schermo della mia MDIChildFrmae...
		GetParent()->GetParent()->ScreenToClient(&rc);	// ... diventano coord. client rispetto al MDI padre (che NON e' theApp.m_pMainWnd !! ce n'è una in mezzo...
//		if(!IsIconic() && !IsZoomed()) {		// NON VANNO PORCODIO
		if(rc.left>0 && rc.top>0 && (rc.right-rc.left)>100 && (rc.bottom-rc.top)>50) {		// cagata quando qualsiasi MDI child maximized...@#$%
			wsprintf(myBuf,"%d,%d,%d,%d",rc.left,rc.top,rc.right,rc.bottom);
			d->WritePrivateProfileString(IDS_COORDINATE,myBuf);
			}
		}*/
	}

// viene INTERROTTA da altri eventi, mouse ecc... e questo incula i semafori. Provo thread
//void CALLBACK waveCallback(HWAVEIN hwi,UINT uMsg,DWORD_PTR dwInstance,DWORD_PTR dwParam1,DWORD_PTR dwParam2) {

int CVidsendView22::initCapture(const WAVEFORMATEX *preferredWf, BOOL bVerbose) {
	char myBuf[128];
	int i,bWarning=0;
	CVidsendDoc22 *d=GetDocument();

	aFrameNum=0;


	if(preferredWf) {
		memcpy(&wfex,preferredWf,sizeof(WAVEFORMATEX));
		memcpy(&wfd,preferredWf,sizeof(WAVEFORMATEX));
		}
	else {
		wfex.wFormatTag = WAVE_FORMAT_PCM;
		wfex.nChannels = 2 /*1*/;
		wfex.nSamplesPerSec = 44100 /*8000*/ /*FREQUENCY*/;
		wfex.nBlockAlign = 4 /*1*/;
		wfex.wBitsPerSample = 16 /*8*/;
		wfex.nAvgBytesPerSec = wfex.nSamplesPerSec*wfex.nChannels*(wfex.wBitsPerSample/8);
		wfex.cbSize = 0;

		// FINIRE con compressore scelto, 2021-23

		GSM610WAVEFORMAT mywfx;
		mywfx.wfx.wFormatTag = WAVE_FORMAT_GSM610;
		mywfx.wfx.nChannels = wfex.nChannels /*1*/;
		mywfx.wfx.nSamplesPerSec = wfex.nSamplesPerSec /*8000*/;
		mywfx.wfx.nAvgBytesPerSec = 1625;
		mywfx.wfx.nBlockAlign = 65;
		mywfx.wfx.wBitsPerSample = 0;
		mywfx.wfx.cbSize = 2;
		mywfx.wSamplesPerBlock = 320;
		memcpy(&wfd,&mywfx,sizeof(GSM610WAVEFORMAT));
		// e occhio, se il formato non è valido l'acm è nullo!
		}

// mettere anche dopo dialog config...	if(GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
	playerPreascolto = new Player(this->m_hWnd);
	if(playerPreascolto->Init(0)) {

		SOUNDFORMAT myFormat;
		myFormat.NbBitsPerSample = 16;
		myFormat.NbChannels = 2;
		myFormat.SamplingRate = 44100;

		playerPreascolto->CreateSoundBuffer(myFormat, 44100L*2*2);

		playerPreascolto->SetNotificationPositions(44100L*2*2,AUDIO_BUFFER_DIVIDER);

		}


	m_pwi = (CWaveIn*)AfxBeginThread(
		RUNTIME_CLASS(CWaveIn),
		THREAD_PRIORITY_HIGHEST, 0, CREATE_SUSPENDED);
	m_pwi->tag=(DWORD)this;		// non so come fare a usare un costruttore con parm in RUNTIME_CLASS...
	m_pwi->m_wfx=wfex;
	m_pwi->ResumeThread();
#if 0
	i=waveInOpen(&m_hWaveIn,WAVE_MAPPER,&wfex,(DWORD)waveCallback,(DWORD)this,CALLBACK_FUNCTION);

	if(i == MMSYSERR_NOERROR) {
		DWORD ti;
		IWaveHdr1.lpData=(char *)m_AudioBuffer1;
		IWaveHdr1.dwBufferLength=wfex.nAvgBytesPerSec /*176400*/ /*8000*/ / AUDIO_BUFFER_DIVIDER;
#pragma warning AUDIO dwBufferLength
		IWaveHdr1.dwBytesRecorded=0;
		IWaveHdr1.dwUser=(DWORD)this;
		IWaveHdr1.dwFlags=0;
		IWaveHdr1.dwLoops=0;
		IWaveHdr1.lpNext=NULL;
		IWaveHdr1.reserved=0;
		IWaveHdr2.lpData=(char *)m_AudioBuffer2;
		IWaveHdr2.dwBufferLength=wfex.nAvgBytesPerSec /*176400*/ /*8000*/ / AUDIO_BUFFER_DIVIDER;
		IWaveHdr2.dwBytesRecorded=0;
		IWaveHdr2.dwUser=(DWORD)this;
		IWaveHdr2.dwFlags=0;
		IWaveHdr2.dwLoops=0;
		IWaveHdr2.lpNext=NULL;
		IWaveHdr2.reserved=0;
		i=waveInPrepareHeader(m_hWaveIn,&IWaveHdr1,sizeof(WAVEHDR));
		ti=timeGetTime()+500;
		while(!(IWaveHdr1.dwFlags & WHDR_PREPARED) && timeGetTime()<ti);
		i=waveInPrepareHeader(m_hWaveIn,&IWaveHdr2,sizeof(WAVEHDR));
		ti=timeGetTime()+500;
		while(!(IWaveHdr2.dwFlags & WHDR_PREPARED) && timeGetTime()<ti);
		i=waveInStart(m_hWaveIn);
		waveInAddBuffer(m_hWaveIn,&IWaveHdr1,sizeof(WAVEHDR));
		waveInAddBuffer(m_hWaveIn,&IWaveHdr2,sizeof(WAVEHDR));
		theApp.theServer2->Opzioni |= CVidsendDoc2::sendAudio;
		}
	else {
#ifdef _DEBUG
		AfxMessageBox("impossibile aprire audio");
#endif
		}
#endif

	acmStreamOpen(&m_hAcm,NULL,&wfex,(WAVEFORMATEX *)&wfd,NULL,NULL,(DWORD)this,0);
	if(m_hAcm)
		acmStreamSize(m_hAcm,wfex.nAvgBytesPerSec,&maxWaveoutSize,ACM_STREAMSIZEF_SOURCE);

fine:
	return i;
	}

LRESULT CVidsendView22::OnAudioFrameReady(WPARAM wParam, LPARAM lParam) {
	AV_PACKET_HDR *avh=(AV_PACKET_HDR *)wParam;
	int retVal=0;
	MSG msg;

	// questo evita che si crei un accavallamento di messaggi del video a scapito degli altri di Windows...
//	if(PeekMessage(&msg,m_hWnd,WM_AUDIOFRAME_READY,WM_AUDIOFRAME_READY,PM_NOREMOVE))
//		goto fine;
	if(!HIWORD(wParam))		// questa patch serve perche' aprendo la finestra proprieta' della sorgente video (p.es. Logitech) arriva un messaggio identico e ovviamente crasha! l'alternativa era cambiare #messaggio WM_VIDEOFRAME_READY
		goto fine;					// (ma anche nell'audio??)

	if(GetDocument()->streamSocketA && avh) {
		retVal=GetDocument()->streamSocketA->Manda(avh,lParam);

// GESTITO come RAW, per ora, 2021 prove 
		/*if(GetDocument()->Opzioni & CVidsendDoc2::usaVBAN) {
			if(GetDocument()->streamSocketVBAN)
				GetDocument()->streamSocketVBAN->Manda(avh,lParam);
			}*/

//		myTV->aFrameNum++;
		}

fine:
/*	if(avh)
		GlobalFree(avh);
	if(pSBuf)
		GlobalFree(pSBuf);*/

	return (LRESULT)retVal;
	}

LRESULT CVidsendView22::OnRawAudioFrameReady(WPARAM wParam, LPARAM lParam) {
	int retVal=0;
	MSG msg;

	// questo evita che si crei un accavallamento di messaggi del video a scapito degli altri di Windows...
//	if(PeekMessage(&msg,m_hWnd,WM_RAWAUDIOFRAME_READY,WM_AUDIOFRAME_READY,PM_NOREMOVE))
//		goto fine;

	if(GetDocument()->Opzioni & CVidsendDoc2::usaVBAN) {
		if(GetDocument()->streamSocketVBAN)
			retVal=GetDocument()->streamSocketVBAN->Manda((const void *)wParam,lParam);
		}

	// prova MP3 streaming, 2023
	if(GetDocument()->streamSocketA2)
		retVal=GetDocument()->streamSocketA2->Manda((const void *)wParam,lParam);


fine:
/*	if(avh)
		GlobalFree(avh);
	if(pSBuf)
		GlobalFree(pSBuf);*/

	return (LRESULT)retVal;
	}

LRESULT CVidsendView22::OnMP3Finished(WPARAM wParam, LPARAM lParam) {

	if(wParam) {
		buttonP2->SetWindowText(">");
		buttonC2->EnableWindow(lParam);
		VUMeter2->SetWindowText(0);
		progress2->SetPos(0);
		}
	else {
		buttonP1->SetWindowText(">");
		buttonC1->EnableWindow(lParam);
		VUMeter1->SetWindowText(0);
		progress1->SetPos(0);
		}
	return 1;
	}


WORD CVidsendView22::measureAudio(const short int *p,DWORD len) {
	int i;
	DWORD n,n1=len;
	// wow, e una FFT? :) 2019
	// v. BESTAUDIOPLAYER 2021!

	n=0;
	while(n1--) {
		if(*p < 0)
			n-=*p;
		else
			n+=*p;
		p++;
		}
	n /= len;
	return sqrt(n);
	}


UINT CVidsendView22::suonaMP3(LPVOID ptr) {
	CVidsendView22 *v=(CVidsendView22*)*(DWORD*)ptr;
	BYTE w=*((char *)ptr+4);
	BYTE vol=*((BYTE *)ptr+5);
	char *n=((char *)ptr+6);
	CString S;
	CJoshuaMP3Equalizer::EQUALIZER equalizer; 
	CVidsendDoc22 *d=v->GetDocument();

	char myBuf[256];

//	EnterCriticalSection(&m_cs);
	try {

	if(!v->m_MP3player[w]) {

		if(v->m_MP3player[w]=new CJoshuaMP3((DWORD)v)) {
			v->m_MP3Thread[w]=AfxGetThread();
//      v->m_MP3Thread[w]->m_bAutoDelete = FALSE;		// boh
//	SetThreadPriority(v->m_MP3Thread[w],/*THREAD_PRIORITY_ABOVE_NORMAL*/ THREAD_PRIORITY_HIGHEST);

			switch(d->m_Equalizzatore) {
				default:		// non deve accadere!
				case 0:
					equalizer=CJoshuaMP3Equalizer::passaFlat;
					break;
				case 1:
					equalizer=CJoshuaMP3Equalizer::equal_rock;
					break;
				case 2:
					equalizer=CJoshuaMP3Equalizer::equal_pop;
					break;
				case 3:
					equalizer=CJoshuaMP3Equalizer::equal_dance;
					break;
				case 4:
					equalizer=CJoshuaMP3Equalizer::equal_classica;
					break;
				}

//			v->m_MP3player[w]->suona(n,0,0,0,0, ,equalizer,vol/100.0,0);
//			v->m_MP3player[w]->suonaDX(n,0,0,0,0,equalizer,vol/100.0 /*,1.0,0*/);
			v->m_MP3player[w]->Suona2(n,d->Opzioni2 & CVidsendDoc22::attivaSchedaAudio2 ? 2 : 1,
				equalizer,vol/100.0 /*,1.0*/);

			v->PostMessage(WM_MP3_FINISHED,w,TRUE);
/*			if(w) {// puttana la madonna non posso chiamare funzioni windows da qua o fa cagare
				v->buttonP2->SetWindowText(">");
				v->buttonC2->EnableWindow(TRUE);
				v->VUMeter2->SetWindowText(0);
				v->progress2->SetPos(0);
				}
			else {
				v->buttonP1->SetWindowText(">");
				v->buttonC1->EnableWindow(TRUE);
				v->VUMeter1->SetWindowText(0);
				v->progress1->SetPos(0);
				}*/

			/*v->busyAudio->Lock();
			delete v->m_MP3player[w];
			v->m_MP3player[w]=NULL;
			v->busyAudio->Unlock();*/
//porcamadonna			while(v->busyAudio2) ;
		WaitForSingleObject(v->busyAudio3, 1000 /*INFINITE*/);
			delete v->m_MP3player[w];
			v->m_MP3player[w]=NULL;
			}

		}
	if(ptr)		// memoria allocata con malloc o tcsdup
		free(ptr);
//	myMP3Thread=NULL;		// rischia di far saltare la stop() che lo controlla!



		}
	catch(...) {
//			AfxMessageBox("exception");
			CSimpleException e;
		e.GetErrorMessage(myBuf, 127);
		theApp.FileSpool->print(CLogFile::flagError,"Exception in MP3");
		theApp.FileSpool->print(CLogFile::flagError,myBuf);

/*		v->busyAudio->Lock();
		delete v->m_MP3player[w];
		v->m_MP3player[w]=NULL;
		v->busyAudio->Unlock();*/
//		while(v->busyAudio2) ;
		WaitForSingleObject(v->busyAudio3, 1000 /*INFINITE*/);
		delete v->m_MP3player[w];
		v->m_MP3player[w]=NULL;
		}
		
	//AfxEndThread(1);

	return 1;
	}

int CVidsendView22::stopMP3(BYTE w) {
	int i=-1;
	MSG msg;
	BOOL bWait;

//	EnterCriticalSection(&m_cs);

	if(m_MP3player[w]) {
		m_MP3player[w]->stop();

		if(m_MP3Thread[w]) {
			bWait=TRUE;
			while(bWait) {
				switch(MsgWaitForMultipleObjects(1, &m_MP3Thread[w]->m_hThread,		// serve perché ci vengono postati messaggi dal thread WAVEIN...
														 FALSE, INFINITE, QS_ALLINPUT)) {
					case WAIT_OBJECT_0:
						bWait=FALSE;
						break;
					case WAIT_OBJECT_0+1:
						if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
							TranslateMessage(&msg);
							DispatchMessage(&msg);
							}
						break;
					default:
			//      return FALSE; // unexpected failure ovvero thread terminato di suo
						bWait=FALSE;
						}
					}
//			i=MsgWaitForMultipleObjects(1,&m_MP3Thread[w]->m_hThread,FALSE,2000,QS_ALLINPUT);		// usare questa se il thread usa window ecc..
// [non viene mai segnalato... 2023]
/*			DWORD ti=timeGetTime()+3000,i;
			do {
				i=WaitForSingleObject(m_MP3Thread[w]->m_hThread,3000);
//				PumpMessage(); no! cazzo!! rientra
				} while(i != WAIT_OBJECT_0 && i != 0xffffffff && ti>timeGetTime());*/
			if(i != WAIT_OBJECT_0 && i != 0xffffffff /* handle invalidato se il thread si è chiuso!*/) {
				TerminateThread(m_MP3Thread[w]->m_hThread,-1);
//				delete m_MP3Canzoni;
//				m_MP3Canzoni=NULL;
				}
			}
		m_MP3Thread[w]=NULL;

/*		busyAudio->Lock();
		delete m_MP3player[w];
		m_MP3player[w]=NULL;
		busyAudio->Unlock();*/
//PORCODIO		while(busyAudio2) Sleep(50);

//		WaitForSingleObject(busyAudio3, 1000 /*INFINITE*/);		// porcospiritos

		bWait=TRUE;
		while(bWait) {
			switch(MsgWaitForMultipleObjects(1, &busyAudio3,		// serve perché ci vengono postati messaggi dal thread WAVEIN...
													 FALSE, INFINITE, QS_ALLINPUT)) {
				case WAIT_OBJECT_0:
					bWait=FALSE;
					break;
				case WAIT_OBJECT_0+1:
					if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
						TranslateMessage(&msg);
						DispatchMessage(&msg);
						}
					break;
				default:
		//      return FALSE; // unexpected failure
					bWait=FALSE;
					}
				}

		delete m_MP3player[w];
		m_MP3player[w]=NULL;

		i=1;
		}
	else
		i=0;


	return i;
	}


int CVidsendView22::playSound(BYTE q,BYTE volume) {
	CVidsendDoc22 *pDoc=GetDocument();
  return playSound(pDoc->WAVFiles[pDoc->bBanco][q],volume,q);
	}
int CVidsendView22::playSound(const char *sndFile,BYTE volume,BYTE q) {
	BOOL fOk;
	CWave wave;
	TCHAR path[MAX_PATH];
	// così si sposta nella cartella di lavoro! se non è specificato path nei suoni
	// ..perché le dialog file lo cambiano!
	GetModuleFileName(NULL, path, MAX_PATH);
	PathRemoveFileSpec(path);
	SetCurrentDirectory(path);

//	busyAudio->Lock();
//	while(busyAudio2) ;
	WaitForSingleObject(busyAudio3, 1000 /*INFINITE*/);
	m_DS->Destroy();
//	busyAudio->Unlock();

	if(GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
		wave.Create(sndFile);
		soundLength=wave.GetDataLen();
		wave.GetFormat(soundWf.wf);
		if(soundSample)
			delete soundSample;
		soundSample=new BYTE[soundLength];
		wave.GetData(soundSample,soundLength);
		}
	else {
		fOk = m_DS->Create(m_DSound, sndFile, wave, DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_REGULAR);
//		int i=GetLastError();

		if(fOk) {
			if(q>=0 && q<9) {
				m_DS->SetNotifyFunction(soundNotifyCallback,(DWORD)button[q+1]);
		//		DWORD dwOffset[] = { 0, DSBPN_OFFSETSTOP };
		//		m_DS->SetNotificationPositions(dwOffset, 2);
				m_DS->SetAutoNotificationPositions(20);
				}

			m_DS->SetCurrentPosition(0);
			m_DS->SetVolume(!(buttonPre3->GetState() & BST_CHECKED) ? DSBVOLUME_MIN : volume*40 +DSBVOLUME_MIN  +6000 /*DSVOLUME_TO_DB(volume)*/);
			m_DS->Play(0);

//			soundLength=m_DS->m_DSBuffer.m_dwImageLen;
			soundLength=wave.GetDataLen();
//			m_DS->m_DSBuffer.m_pDSBuffer->GetFormat(&soundWf.wf,sizeof(soundWf.wf),NULL);
			wave.GetFormat(soundWf.wf);
// forse serve? o abbiam tempo da sopra?	WaitForSingleObject(busyAudio3, 1000 /*INFINITE*/);
			if(soundSample)
				delete soundSample;
			soundSampleStart=NULL;
			soundSample=new BYTE[soundLength];
			wave.GetData(soundSample,soundLength);
			}
		else {
	//		theApp.FileSpool->print(CLogFile::flagInfo,"errore: IMPOSSIBILE SUONARE -%s-",sndFile);
			}
		}

	return fOk;
	}

int CVidsendView22::playSound(const BYTE *sndSample,BYTE volume) {
	BOOL fOk =TRUE;

	m_DS->Destroy();
	if(sndSample) {
		fOk = m_DS->Create(m_DSound, sndSample, 22050, NULL, DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_REGULAR);
		if(fOk) {
			m_DS->SetNotifyFunction(soundNotifyCallback,0);
			DWORD dwOffset[] = { 0, DSBPN_OFFSETSTOP };
			m_DS->SetNotificationPositions(dwOffset, 2);

			m_DS->SetCurrentPosition(0);
			m_DS->SetVolume(volume*40 +DSBVOLUME_MIN  +6000 /*DSVOLUME_TO_DB(volume)*/);
			m_DS->Play(0);
			}
		else {
//		theApp.FileSpool->print(CLogFile::flagInfo,"errore: IMPOSSIBILE SUONARE sample/-11000");
			}
		}
	else {
//		fOk=m_DSound->Stop();
		}

	return fOk;
	}

LONG CVidsendView22::soundNotifyCallback(void *v,LONG f, LONG f2) { // static function
	CRoundButton *btn=(CRoundButton *)v;

//	TRACE("- CSound::NotifyCallback: %d\n", f);

	//	ScopeGuardMutex guard(&m_exprMutex);		dovuto a STATIC??
	const BYTE *p;
	BYTE p1;

	theApp.FileSpool->print(CLogFile::flagInfo,"- CSound::NotifyCallback: %d\n", f);

	switch(f) {
		case -3:			// totale secondi .. fare
			f2;
			break;
		case DSBPN_OFFSETSTOP:
//		case 20:
			CVidsendView22::pThis->inSound=0;
//		CSound::pThis->SetDlgItemText(IDC_STATIC_EVENT, TEXT("stopped"));
//		CSound::pThis->GetDlgItem(IDC_PLAYSTOP)->SetWindowText("Play");
//		CSound::pThis->GetDlgItem(IDC_CHECK_REPEAT)->EnableWindow();
//		CSound::pThis->m_bRunning = FALSE;
			btn->SetColors(RGB(192,192,192),RGB(96,96,96),RGB(64,64,64));
			btn->SetValues();
//			((CVidsendView22*)btn->GetParent())->VUMeter4->SetWindowText(0);		
//strano... restituisce pattume..
			break;
		case -2:
//			v->m_DS->Destroy();
			btn->SetColors(RGB(192,192,192),RGB(96,96,96),RGB(64,64,64));
			btn->SetValues();		// serve se si fa partire un suono mentre ne gira un altro!
			break;
		case 1:
			btn->SetColors(RGB(192,192,192),RGB(210,190,80),RGB(64,64,64)); 
		default:	
//		CSound::pThis->SetDlgItemText(IDC_STATIC_EVENT, TEXT("started"));
			btn->SetValues(f*(CRoundButton::VALUES_RANGE/20)); 
			CVidsendView22* v2=(CVidsendView22*)(btn->GetParent());
//strano... restituisce pattume..

//			((CVidsendView22*)btn->GetParent())->VUMeter4->SetWindowText(15   );		// fare!
			break;
		}

	return 0;
	}

LONG CVidsendView22::mp3NotifyCallback(void *p, DWORD p1, DWORD p2, DWORD p3) { // static function
	CJoshuaMP3 *m=(CJoshuaMP3 *)p;
	CVidsendView22 *v=(CVidsendView22*)m->tag;

	switch(p1) {
		case 1:			// vumeter-value
			if(p==v->m_MP3player[0])
				v->VUMeter1->SetWindowText(p2/5);
			else
				v->VUMeter2->SetWindowText(p2/5);
			break;
		case 2:			// totale frame/secondi
			if(p==v->m_MP3player[0])
				v->progress1->SetRange(0,p2);
			else
				v->progress2->SetRange(0,p2);
			break;
		case 3:			// frame attualmente suonato
			if(p==v->m_MP3player[0])
				v->progress1->SetPos(p2);
			else
				v->progress2->SetPos(p2);
			break;
		}
	return 0;
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

	if(theApp.Opzioni & CVidsendApp::saveLayout)
		d->savelayout();
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

int CVidsendView4::doConnect(const char *myAddress,struct CHAT_INFO *si) {
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
	if(theApp.Opzioni & CVidsendApp::saveLayout)
		d->savelayout();
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

	Navigate("http://www.google.com");		// fare di meglio! :) 2023
	// anche GoSearch();
	}


void CVidsendView5::OnNavigazioneInterrompi() {
	
	Stop();
	}

void CVidsendView5::OnUpdateNavigazioneInterrompi(CCmdUI* pCmdUI) {
//	int i=GetReadyState();
//	pCmdUI->Enable(i==READYSTATE_LOADING || i==READYSTATE_LOADED || i==READYSTATE_INTERACTIVE);
	pCmdUI->Enable(GetBusy() ? TRUE : FALSE);
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
	if(theApp.Opzioni & CVidsendApp::saveLayout)
		d->savelayout();
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
	ON_COMMAND(ID_VISUALIZZA_ANCHEDIRECTORYSERVER, OnVisualizzaAncheDirectoryServer)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_ANCHEDIRECTORYSERVER, OnUpdateVisualizzaAncheDirectoryServer)
	ON_COMMAND(ID_VISUALIZZA_IPLOOKUP, OnVisualizzaIpLookup)
	ON_UPDATE_COMMAND_UI(ID_VISUALIZZA_IPLOOKUP, OnUpdateVisualizzaIpLookup)
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
	if(theApp.Opzioni & CVidsendApp::saveLayout)
		d->savelayout();
	}


int CVidsendView6::update() {
	int i,j,n;
	HTREEITEM tp0,tp1;
	CString aIP,S;
	UINT aPort;
	DWORD oldSel=0;
	CVidsendDoc6 *d=GetDocument();
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
		CStreamSrvSocket2 *myRoot2;
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
		

		po=theApp.theServer->streamSocketA2->cSockRoot.GetHeadPosition();		// mp3 stream, 2023
		if(po) {
			do {
				po1=po;
				myRoot2=theApp.theServer->streamSocketA2->cSockRoot.GetNext(po);
				if(myRoot2->m_hSocket != INVALID_SOCKET) {
					myRoot2->GetPeerName(aIP,aPort);
					S=aIP;

					tp1=GetTreeCtrl().InsertItem(S,tp0);
					GetTreeCtrl().SetItemImage(tp1,1,1);
					GetTreeCtrl().SetItemData(tp1,(DWORD)myRoot);
					}
				} while(po);
			}
		}
	if(theApp.theServer2) {
		POSITION po;
		CStreamSrvSocket2 *myRoot;

		po=theApp.theServer2->streamSocketA2->cSockRoot.GetHeadPosition(); // mp3 stream, 2023
		if(po) {
			do {
				myRoot=theApp.theServer2->streamSocketA2->cSockRoot.GetNext(po);
				if(myRoot->m_hSocket != INVALID_SOCKET) {
					myRoot->GetPeerName(aIP,aPort);
					S=aIP;

					char iploc[200];
					*iploc=0;
/*					aIP="5.5.5.5";
							theApp.subGetIPLocation(aIP,iploc); 
							if(*iploc) {
								char *s;
								i=5;
								s=strtok(iploc,",");
								if(s) {
									while(i--)
										s=strtok(NULL,",");
									}
								S+=" (";
								S+=s;
								S+=')';
								}*/
					if(!CSocketEx::IsLocalAddress((LPCTSTR)aIP)) {
						if(d->Opzioni & CVidsendDoc6::visualizzaIpLookup) {
							if(myRoot->IPlocation.IsEmpty()) {
								theApp.subGetIPLocation(aIP,iploc); 
								if(*iploc) {
									char *s;
									i=5;
									s=strtok(iploc,",");
//									if(s) {
										while(i-- && s)
											s=strtok(NULL,",");
//										}
									myRoot->IPlocation=s;
									}
								}
							if(!myRoot->IPlocation.IsEmpty()) {
								S+=" (";
								S+=myRoot->IPlocation;
								S+=')';
								}
							}
						}

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
								myRoot->m_Parent->doDelete(myRoot);
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

void CVidsendView6::OnVisualizzaIpLookup() {
	CVidsendDoc6 *d=GetDocument();

	d->Opzioni ^= CVidsendDoc6::visualizzaIpLookup;
	}

void CVidsendView6::OnUpdateVisualizzaIpLookup(CCmdUI* pCmdUI) {
	CVidsendDoc6 *d=GetDocument();

	pCmdUI->SetCheck(d->Opzioni & CVidsendDoc6::visualizzaIpLookup ? 1 : 0);
	}

void CVidsendView6::OnVisualizzaAncheDirectoryServer() {
	CVidsendDoc6 *d=GetDocument();

	d->Opzioni ^= CVidsendDoc6::mostraAncheDirSrv;
	}

void CVidsendView6::OnUpdateVisualizzaAncheDirectoryServer(CCmdUI* pCmdUI) {
	CVidsendDoc6 *d=GetDocument();

	pCmdUI->SetCheck(d->Opzioni & CVidsendDoc6::mostraAncheDirSrv ? 1 : 0);
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
							myRoot->m_Parent->doDelete(myRoot);
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
	if(theApp.Opzioni & CVidsendApp::saveLayout)
		d->savelayout();
	}



void CVidsendView7::OnComputerDisconnetti() {
	
	
	}

void CVidsendView7::OnUpdateComputerDisconnetti(CCmdUI* pCmdUI) {
	
	
	}

void CVidsendView7::OnComputerMandamessaggio() {
	
	
	}

void CVidsendView7::OnUpdateComputerMandamessaggio(CCmdUI* pCmdUI) {
	
	
	}


