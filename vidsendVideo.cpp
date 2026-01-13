#include "stdafx.h"
#include "vidsend.h"
#include "vidsendlog.h"
#include "vidsenddoc.h"
//#include "vidsenddialog.h"
//#include "vidsendvideo.h"
#include "msacm.h"
#include "mmreg.h"

static CTV *m_TV;		// usata dalle callback...


#ifndef USA_DIRECTX


CTV::CTV(CWnd *pWnd) {

	preConstruct(pWnd);			// non so come richiamare un constructor da un altro... C11
//https://stackoverflow.com/questions/308276/can-i-call-a-constructor-from-another-constructor-do-constructor-chaining-in-c
	}

CTV::CTV(CWnd *pWnd, BOOL canOverlay,const BITMAPINFOHEADER *biRaw, DWORD fps, DWORD preferredDriver, DWORD kFrame, BOOL bAudio, WAVEFORMATEX *preferredWf, BOOL bVerbose) {

	preConstruct(pWnd);
	initCapture(canOverlay, biRaw, fps, preferredDriver, kFrame, bAudio, preferredWf,bVerbose);
	}

CTV::CTV(CWnd *pWnd, BOOL canOverlay, const SIZE *bSize, WORD bpp, DWORD fps, DWORD preferredDriver, DWORD kFrame, BOOL bAudio, WAVEFORMATEX *preferredWf, BOOL bVerbose) {
	BITMAPINFOHEADER bmi;

	preConstruct(pWnd);
	bmi.biWidth=bSize->right;
	bmi.biHeight=bSize->bottom;
	bmi.biBitCount=bpp;

	initCapture(canOverlay,&bmi, fps, preferredDriver, kFrame, bAudio, preferredWf, bVerbose);
#ifdef _CAMPARTY_MODE				// per ora solo qua, ma potrebbe avere senso anche agli altri
	bSize.bottom=bmi.biHeight;
	bSize.right=bmi.biWidth;
#endif
	}

void CTV::preConstruct(CWnd *pWnd) {

	ZeroMemory((void *)&captureCaps,sizeof(CAPDRIVERCAPS));
	ZeroMemory((void *)&captureParms,sizeof(CAPTUREPARMS));
	ZeroMemory((void *)&captureStatus,sizeof(CAPSTATUS));
	myParent=pWnd;
	theFrame=NULL;
	theCapture=theApp.prStore->GetProfileVariabileInt(IDS_WHICH_CAMERA);
	aviFile=NULL;
	psVideo = NULL, psAudio=NULL, psText = NULL;
	hICCo=hICCo2=hICDe=NULL;
	hAcm=NULL;
	m_TV=this;
	maxFrameSize=0;
	framesPerSec=0;
	KFrame=0;
	oldTimeCaptured=0;
	compressor=0;
	maxWaveoutSize=0;
	inCapture=0;
	allowOverlay=0;
	saveVideo=0;
	imposeTime=0;
	imposeTextPos=0;
	opzioniSave=0;
	m_hWnd=0;
	vFrameNum=aFrameNum=vFrameNum4Save=aFrameNum4Save=saveWait4KeyFrame=0;
	}


int CTV::initCapture(BOOL canOverlay, BITMAPINFOHEADER *biRaw, DWORD fps, DWORD preferredDriver, 
										 DWORD kFrame, BOOL bAudio, WAVEFORMATEX *preferredWf, BOOL bVerbose) {
	char myBuf[128];
	int i,bWarning=0;

	compressor=preferredDriver ? preferredDriver : mmioFOURCC('I','V','5','0');
	framesPerSec=fps ? fps : 5;
	KFrame=kFrame;

	aFrameNum=vFrameNum=0;
	vFrameNum4Save=0;
	aFrameNum4Save=0;
	saveWait4KeyFrame=0;

	allowOverlay=canOverlay;
	saveVideo=0;
	imposeTime=1;
	imposeTextPos=1; _tcscpy(imposeText,"dario");

	inCapture=-1;
	biRawBitmap.biSize=sizeof(BITMAPINFOHEADER);
	biRawBitmap.biPlanes=1;
	biRawBitmap.biCompression=0;
//	biRawBitmap.biCompression=mmioFOURCC('I','Y','U','V');			// YUV 420 Philips

	biRawBitmap.biSizeImage=0;
	biRawBitmap.biXPelsPerMeter=biRawBitmap.biYPelsPerMeter=0;
	biRawBitmap.biClrUsed=biRawBitmap.biClrImportant=0;
	if(biRaw) {
		biRawBitmap.biWidth=biRaw->biWidth;
		biRawBitmap.biHeight=biRaw->biHeight;
		biRawBitmap.biBitCount=biRaw->biBitCount;
		}
	else {
		biRawBitmap.biWidth=320;
		biRawBitmap.biHeight=240;
		biRawBitmap.biBitCount=24;
		}

	maxFrameSize=(biRawBitmap.biWidth*biRawBitmap.biHeight*biRawBitmap.biBitCount)/8;
	biRawBitmap.biSizeImage=maxFrameSize;



//		biRawBitmap.biBitCount=12;		// YUV 420	Philips
//						biRawBitmap.biBitCount=16;		//la VB RT300

	
	
	if(capGetDriverDescription(theCapture,myBuf,64,NULL,0)) {  // determino se ho scheda video
		theApp.Opzioni |= CVidsendApp::canSendVideo;
		if(preferredDriver) {
			}
		hWnd = capCreateCaptureWindow (
			(LPSTR)"", // window name if pop-up 
			WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,       // window style 
			0,0,biRawBitmap.biWidth,biRawBitmap.biHeight,
			myParent->m_hWnd, 1); 	  // parent, child ID 
		if(hWnd) {
		// Register the callback function before connecting capture driver. 
			setCallbackOnError(ErrorCallbackProc); 
	   	setCallbackOnStatus(StatusCallbackProc); 
			setCallbackOnFrame(FrameCallbackProc); 
			i=driverConnect(); 
			if(i<=0) {				// se non c'e', distruggo (v. distruttore) tutto fin qui
				setCallbackOnFrame();
				setCallbackOnError();
				setCallbackOnStatus(); 
				::DestroyWindow(hWnd);
				goto no_hwnd;
				}
			capDriverGetCaps(hWnd,&captureCaps,sizeof(CAPDRIVERCAPS));
			capCaptureGetSetup(hWnd,&captureParms, sizeof(CAPTUREPARMS));
			captureParms.dwRequestMicroSecPerFrame = (DWORD) (1.0e6 / framesPerSec);
			// La logitech QuickCAM Pro USB se ne infischia di questo e va sempre a 15fps!! (v. patch in callback video!)
			captureParms.wPercentDropForError=99;		// anche se ci sono fotogrammi persi, non mi interessa!
						// Per ELIMINARE quel msg di errore "andati persi fotogrammi" (che fa piantare tutto!) v. ErrorCallback
			captureParms.fYield = TRUE;
			captureParms.dwIndexSize=100000;		// non dovrebbe servire...
			captureParms.fCaptureAudio = bAudio;
			captureParms.vKeyAbort=0;
			captureParms.fAbortRightMouse =FALSE;
			captureParms.fAbortLeftMouse =FALSE;
			captureParms.wNumVideoRequested=8;
			captureParms.wNumAudioRequested=5;
			captureParms.wStepCaptureAverageFrames=0;
			captureParms.dwAudioBufferSize =wfex.nSamplesPerSec;
			i=capCaptureSetSetup(hWnd,&captureParms,sizeof(CAPTUREPARMS)); 
			biBaseRawBitmap=biRawBitmap;
			i=setVideoFormat(&biRawBitmap);

//			BITMAPINFOHEADER bi;
			i=getVideoFormat(&biRawBitmap);	// certe telecamere/schede si fanno i cazzi loro (SENZA dare errore), in questo modo vedo che fotogrammi mi dara'...


//							CFile mf;

//							mf.Open("c:\\frame0.bmp",CFile::modeWrite | CFile::modeCreate);
//							mf.Write(&bi,sizeof(bi));
//							mf.Close();


			overlay(FALSE); preview(FALSE);
			allowOverlay ? overlay(TRUE) : preview(TRUE);
 			}
		}
	else {
no_hwnd:
		hWnd=NULL;
		biBaseRawBitmap=biRawBitmap;		// tanto per gradire... serve (v.sotto) a inizializzare cmq un compressore, in caso si trasmetta video playback
		biBaseRawBitmap.biCompression=0;
		biBaseRawBitmap.biBitCount=24;
		}

fine_hwnd:
	biRawDef.bmiHeader=biRawBitmap;
//	biRawDef.bmiColors=NULL;

	biCompDef=biRawDef;
	biCompDef.bmiHeader=biBaseRawBitmap;
	biCompDef.bmiHeader.biCompression=compressor;
	biCompDef.bmiHeader.biBitCount=24;		// buono per IR50



//	biRawBitmap.biWidth=100;		// per prove!




	if(hWnd) {			// solo se esiste un driver video...
rifo_check_video_format:
#ifndef _CAMPARTY_MODE
		if(biBaseRawBitmap.biWidth != biRawBitmap.biWidth ||
			biBaseRawBitmap.biHeight != biRawBitmap.biHeight) {
			bWarning=TRUE;
#else
		if(biBaseRawBitmap.biWidth != biRawBitmap.biWidth || biRawBitmap.biWidth != 320 ||
			biBaseRawBitmap.biHeight != biRawBitmap.biHeight || biRawBitmap.biHeight != 240) {
			bWarning=TRUE;
#endif
#ifdef _CAMPARTY_MODE
//			biBaseRawBitmap.biWidth = biRawBitmap.biWidth;
//			biBaseRawBitmap.biHeight = biRawBitmap.biHeight;
//			*biRaw=biBaseRawBitmap;

//			if(bVerbose) {			qua??

			CConfirmCamDlg myDlg;
			if(myDlg.DoModal() == IDOK) {
				if(!capDlgVideoFormat(hWnd))
					AfxMessageBox("Impossibile aprire la finestra delle proprietà della webcam! (provare a riavviare il computer)",MB_ICONSTOP);
				i=getVideoFormat(&biRawBitmap);
				goto rifo_check_video_format;
				}
			else {			// v. distruttore
				if(theFrame && (int)theFrame != -1) 
					HeapFree(GetProcessHeap(),0,theFrame);	// usato dal JPEG dell'HTML
				if(aviFile)
					endSaveFile();
				if(hWnd) {
					Capture(0); 
					preview(FALSE);        // disables preview 
					overlay(FALSE);        // disables OVL
					setCallbackOnFrame();
					setCallbackOnError();
					setCallbackOnStatus(); 
					driverDisconnect(); 
					::DestroyWindow(hWnd);
					}
				}
#elif _NEWMEET_MODE
			
/*Qu=E0 su nmvidsend deve funzionare cos=EC:=0D
se trovi un formato video diverso un messaggio con una domanda intelligen=
te
che dice:=0D
Attenzione, il formato video della tua webcam (140x120) =E8 diversa dal
formato video di nmvidsend (320x240)!=0D
=0D
vuoi cambiare il formato video della tua webcam ? SI=0D
 (a questo punto fai apparire la finestra del driver cam)=0D
vuoi cambiare il formato video di nmvidsend ? SI=0D
 (a questo punto fai apparire la finestra di avanzate di nmvsend)=0D
=0D
questo mi s=E0 =E8 l'unico modo di evitare errori da parte dell'esibitor =
e da
parte nostra=0D*/


			if(bVerbose) {
				CString S;			
				S.Format("Attenzione, il formato video della tua webcam (%ux%u) è diverso dal formato video di NMVidsend (%ux%u)!",
					biRawBitmap.biWidth,biRawBitmap.biHeight,biBaseRawBitmap.biWidth,biBaseRawBitmap.biHeight);
				AfxMessageBox(S,MB_OK);
				
	//			biBaseRawBitmap.biWidth = biRawBitmap.biWidth;
	//			biBaseRawBitmap.biHeight = biRawBitmap.biHeight;
	//			*biRaw=biBaseRawBitmap;

				if(AfxMessageBox("Vuoi cambiare il formato video della webcam?",MB_YESNO | MB_DEFBUTTON1 | MB_ICONQUESTION) == IDYES) {
					if(!capDlgVideoFormat(hWnd))
						AfxMessageBox("Impossibile aprire la finestra delle proprietà della webcam! (provare a riavviare il computer)",MB_ICONSTOP);
					i=getVideoFormat(&biRawBitmap);
					goto rifo_check_video_format;
					}
				AfxMessageBox("E' necessario cambiare il formato video di NMVidsend!",MB_ICONSTOP);
				CVidsendPropPage mySheet("Proprietà streaming avanzate");
				struct QUALITY_MODEL_V qv;
				ZeroMemory(&qv,sizeof(struct QUALITY_MODEL_V));
				qv.imageSize.right=biRawBitmap.biWidth;
				qv.imageSize.bottom=biRawBitmap.biHeight;
				qv.bpp=24;		// tanto per...
				CVidsendDoc2PropPage0 myPage0(NULL,&qv);
				
				mySheet.AddPage(&myPage0);
				if(mySheet.DoModal() == IDOK) {
					if(myPage0.isInitialized) {
						//uso solo questo campo...
						biBaseRawBitmap.biWidth=myPage0.m_QV.imageSize.right;
						biBaseRawBitmap.biHeight=myPage0.m_QV.imageSize.bottom;
						*biRaw=biBaseRawBitmap;
						}
					goto rifo_check_video_format;
					}
				else
	//				AfxMessageBox("NMVidsend potrebbe non funzionare correttamente con le impostazioni attuali.\nE' consigliabile impostare NMVidsend in maniera corrispondente alle impostazioni della webcam.",MB_ICONEXCLAMATION);
	// no... il @*!?!@ non vuole!
					goto rifo_check_video_format;
				}

#else
			if(bVerbose) {
				if(AfxMessageBox("Il driver video non accetta le impostazioni fornitegli dal programma:\nUtilizzare le impostazioni del driver?",MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES) {
					biBaseRawBitmap.biWidth = biRawBitmap.biWidth;
					biBaseRawBitmap.biHeight = biRawBitmap.biHeight;
					AfxMessageBox("E' consigliabile impostare il programma in maniera corrispondente alle impostazioni del driver.",MB_ICONEXCLAMATION);
					}
				}
#endif
			}
		}

	hICCo=ICOpen(ICTYPE_VIDEO,compressor,ICMODE_FASTCOMPRESS);
	if(hICCo) {
/*		CString info;
		ICINFO icinfo;
		ICGetInfo(hICCo,&icinfo,sizeof(ICINFO));
		info+=icinfo.dwFlags & VIDCF_CRUNCH ? "supporta compressione a una dimensione data; " : "";
		info+=icinfo.dwFlags & VIDCF_DRAW ? "supporta drawing; " : "";
		info+=icinfo.dwFlags & VIDCF_FASTTEMPORALC ? "supporta compressione temporale e conserva copia dei dati; " : "";
		info+=icinfo.dwFlags & VIDCF_FASTTEMPORALD ? "supporta decompressione temporale e conserva copia dei dati; " : "";
		info+=icinfo.dwFlags & VIDCF_QUALITY ? "supporta impostazione qualità; " : "";
		info+=icinfo.dwFlags & VIDCF_TEMPORAL ? "supporta compressione inter-frame; " : "";
		AfxMessageBox(info);*/

		maxFrameSize=ICCompressGetSize(hICCo,&biBaseRawBitmap,&biCompDef);
		i=ICCompressBegin(hICCo,&biBaseRawBitmap,&biCompDef);


/*		i=ICGetDefaultKeyFrameRate(hICCo);
		j=ICGetDefaultQuality(hICCo);
		wsprintf(myBuf,"default key frame rate=%d, quality=%u",i,j);
		AfxMessageBox(myBuf);*/
		}


	if(biRawBitmap.biCompression != 0) {
		hICDe=ICOpen(ICTYPE_VIDEO,biRawBitmap.biCompression  /*mmioFOURCC('i','4','2','0' */),ICMODE_DECOMPRESS);
//		hICDe=ICOpen(ICTYPE_VIDEO,mmioFOURCC('i','4','2','0' /*'y','v','y','u'*/),ICMODE_DECOMPRESS);
		if(hICDe)
			i=ICDecompressBegin(hICDe,&biRawBitmap,&biBaseRawBitmap);
		if(!hICDe || i != 0)
			AfxMessageBox("Impossibile inizializzare il decompressore di supporto!");
		}



	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 1;
	wfex.nSamplesPerSec = 8000 /*FREQUENCY*/;
	wfex.nBlockAlign = 1;
	wfex.wBitsPerSample = 8;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec*wfex.nChannels*(wfex.wBitsPerSample/8);
	wfex.cbSize = 0;

	if(preferredWf) {
		memcpy(&wfd,preferredWf,sizeof(WAVEFORMATEX));
		}
	else {
		GSM610WAVEFORMAT mywfx;
		mywfx.wfx.wFormatTag = WAVE_FORMAT_GSM610;
		mywfx.wfx.nChannels = 1;
		mywfx.wfx.nSamplesPerSec = 8000 /*8000*/;
		mywfx.wfx.nAvgBytesPerSec = 1625;
		mywfx.wfx.nBlockAlign = 65;
		mywfx.wfx.wBitsPerSample = 0;
		mywfx.wfx.cbSize = 2;
		mywfx.wSamplesPerBlock = 320;
		memcpy(&wfd,&mywfx,sizeof(GSM610WAVEFORMAT));
		}


	if(setAudioFormat(&wfex))
		theApp.Opzioni |= CVidsendApp::canSendAudio;

	acmStreamOpen(&hAcm,NULL,&wfex,(WAVEFORMATEX *)&wfd,NULL,NULL,0 /*this*/,0);
	if(hAcm)
		acmStreamSize(hAcm,wfex.nAvgBytesPerSec,&maxWaveoutSize,ACM_STREAMSIZEF_SOURCE);

	capSetUserData(hWnd,this); 
fine:
	return hWnd ? (bWarning ? 2 : 1) : 0;
	}

CTV::~CTV() {

	waveInReset(m_hWaveIn);
	waveInStop(m_hWaveIn);
	waveInClose(m_hWaveIn);

	if(theFrame && (int)theFrame != -1) 
		HeapFree(GetProcessHeap(),0,theFrame);	// usato dal JPEG dell'HTML
	if(aviFile)
		endSaveFile();
	if(hWnd) {
		Capture(0); 
		preview(FALSE);        // disables preview 
		overlay(FALSE);        // disables OVL
		setCallbackOnFrame();
		setCallbackOnError();
		setCallbackOnStatus(); 
		driverDisconnect(); 
		::DestroyWindow(hWnd);
		}
	if(m_hAcm)
		acmStreamClose(m_hAcm,0);
	m_hAcm=NULL;
	delete []m_AudioBuffer1;		m_AudioBuffer1=NULL;
	delete []m_AudioBuffer2;		m_AudioBuffer2=NULL;

	if(hICCo) {
		ICCompressEnd(hICCo);
		ICClose(hICCo);
		}
	hICCo=NULL;
	if(hICDe) {
		ICDecompressEnd(hICDe);
		ICClose(hICDe);
		}
	hICDe=NULL;
  theApp.prStore->WriteProfileVariabileInt(IDS_WHICH_CAMERA,theCapture);
	}

RECT *CTV::Resize(RECT *r, BOOL force) {
	int i,oldC=-1;

	if(hWnd) {
		if(inCapture==1) {		// a volte arriva Resize MENTRE e' in Capture (da createView o altro...) obbligatorio sincronizzare le cose!
			oldC=inCapture;
			Capture(0);
			}
		allowOverlay ? overlay(FALSE) : preview(FALSE);
		if(force) {
			biRawBitmap.biWidth=min(r->right-r->left,640);
			biRawBitmap.biHeight=(biRawBitmap.biWidth*biRawBitmap.biBitcount/8)/4;
			}
		r->right=biRawBitmap.biWidth;
		r->bottom=biRawBitmap.biHeight;
		::SetWindowPos(hWnd,HWND_BOTTOM,r->left,r->top,biRawBitmap.biWidth,biRawBitmap.biHeight,SWP_NOACTIVATE);
		i=setVideoFormat(&biRawBitmap);
		allowOverlay ? overlay(TRUE) : preview(TRUE);
		if(oldC==1) {
			Capture(1); 
			}
		}
	return r;
	}

int CTV::Capture(int m) {
	DWORD l;
	MSG msg;
	char myBuf[64];


//	wsprintf(myBuf,"capture %d, hWnd %x",m,hWnd);
//	AfxMessageBox(myBuf);
	if(hWnd) {
		oldTimeCaptured=0;
		vFrameNum=0;
		aFrameNum=0;
		if(m && inCapture!=1) {
			inCapture=TRUE;
			setCallbackOnVideoStream(VideoCallbackProc);
			setCallbackOnWaveStream(WaveCallbackProc); 
			captureSequenceNoFile();

			l=timeGetTime()+5000;
			while(timeGetTime() < l) {
				if(::PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
					if(!theApp.PumpMessage()) { 
						}
					}
				getStatus();
				if(captureStatus.fCapturingNow)
					break;
				Sleep(1000);
				}

			return 1;
			}
		if(!m && inCapture==1) {
			inCapture=FALSE;
			captureStop();
			l=timeGetTime()+5000;
			while(timeGetTime() < l) {
				if(::PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
					if(!theApp.PumpMessage()) { 
						}
					}
				getStatus();
				if(!captureStatus.fCapturingNow)
					break;
				Sleep(1000);
				}
			setCallbackOnVideoStream(); 
			setCallbackOnWaveStream(); 
			return 1;
			}
		}
	else
		return 0;
	}

BOOL CTV::GetDriverDescription(WORD wDriverIndex,  
  LPSTR lpszName,int cbName,
  LPSTR lpszVer, int cbVer) {

	return capGetDriverDescription(wDriverIndex, lpszName, cbName, lpszVer, cbVer);
	}

int CTV::startSaveFile(CString nomefile,DWORD opzioni) {
	AVISTREAMINFO strhdr;
	HRESULT hr; 
	AVICOMPRESSOPTIONS opts; 
	LPAVICOMPRESSOPTIONS aopts[1] = {&opts}; 
	PAVISTREAM myps;
  DWORD dwTextFormat; 
	int i;

	psVideo=NULL, psAudio=NULL, psText = NULL;

	AVIFileInit();  // Open the movie file for writing....
	
	hr = AVIFileOpen(&aviFile,    // returned file pointer
		nomefile,            // file name
		OF_WRITE | OF_CREATE,    // mode to open file with
		NULL);    // use handler determined from file extension....
	if(hr != AVIERR_OK)
		goto error;
	
	ZeroMemory(&strhdr, sizeof(strhdr));
	strhdr.fccType                = streamtypeVIDEO;// stream type
	strhdr.fccHandler             = compressor;
	strhdr.dwScale                = 1;
	strhdr.dwRate                 = opzioni & CVidsendDoc2::quantiFrame ? 1 : framesPerSec;
	strhdr.dwSuggestedBufferSize  = maxFrameSize;
	SetRect(&strhdr.rcFrame, 0, 0,    // rectangle for stream
				(int) biCompDef.bmiHeader.biWidth,
				(int) biCompDef.bmiHeader.biHeight);  // And create the stream;
	hr = AVIFileCreateStream(aviFile,    // file pointer
												 &myps,    // returned stream pointer
												 &strhdr);    // stream header
	if(hr != AVIERR_OK)
		goto error;
	hr = AVIStreamSetFormat(myps, 0, &biCompDef.bmiHeader, sizeof(BITMAPINFOHEADER)); 
	if(hr != AVIERR_OK) 
		goto error;

//	ZeroMemory(&opts, sizeof(opts));
//	if(!AVISaveOptions(NULL, 0, 1, &myps, (LPAVICOMPRESSOPTIONS FAR *) &aopts))
//		goto error;

//if(superimposeDateTime) solo in questo caso?

	ZeroMemory(&strhdr, sizeof(strhdr)); 
	strhdr.fccType                = streamtypeTEXT; 
	strhdr.fccHandler             = mmioFOURCC('D', 'R', 'A', 'W'); 
	strhdr.dwScale                = 1; 
	strhdr.dwRate                 = 1;
	strhdr.dwSuggestedBufferSize  = 25;
	SetRect(&strhdr.rcFrame, 0, (int) biCompDef.bmiHeader.biHeight,
				(int) biCompDef.bmiHeader.biWidth,     
				(int) biCompDef.bmiHeader.biHeight+(biCompDef.bmiHeader.biHeight/8));  // And create the stream; 
	hr = AVIFileCreateStream(aviFile, &psText, &strhdr); 
	if(hr != AVIERR_OK) 
		goto error; 
  dwTextFormat = sizeof(dwTextFormat); 
	hr = AVIStreamSetFormat(psText, 0, &dwTextFormat, sizeof(dwTextFormat)); 
	if(hr != AVIERR_OK) 
		goto error;

	if(1 /* solo quando accodo...*/)	{
		long n;
		DWORD t,l;
		CBitmap b;
		BITMAP bmp;
		BYTE *p,*p2;

		b.CreateBitmap(biCompDef.bmiHeader.biWidth,biCompDef.bmiHeader.biHeight,1,biCompDef.bmiHeader.biBitCount,NULL);
		b.GetBitmap(&bmp);
		n=biCompDef.bmiHeader.biWidth*biCompDef.bmiHeader.biHeight*biCompDef.bmiHeader.biBitCount/8;
		p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,n+10 /* per DWORD! */);
		bmp.bmBits=p;
		for(i=0; i<n; i+=3) {
			*(DWORD *)p=RGB(0,0,192);
			p+=3;
			}
//		b.SetBitmapBits(n,bmp.bmBits);
		// vorrei o un monoscopio con la data e l'ora, o almeno uno sfondo colorato con data e ora
		superImposeDateTime(&biRawDef.bmiHeader,(BYTE *)bmp.bmBits);
		p2=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,maxFrameSize+100);
		t=l=0;
		i=ICCompress(hICCo,ICCOMPRESS_KEYFRAME,
			&biCompDef.bmiHeader,p2,&biRawDef.bmiHeader,bmp.bmBits,
			&l,&t,0,0/*2500*/,theApp.theServer->myQV.quality,
			NULL,NULL);
		HeapFree(GetProcessHeap(),0,bmp.bmBits);
		if(i == ICERR_OK) {
			vFrameNum4Save=AVIStreamLength(myps);
			for(i=0; i<(opzioni & CVidsendDoc2::quantiFrame ? 1 : framesPerSec); i++) {		// sempre 1 sec.
				n=AVIStreamWrite(myps,// stream pointer 
					vFrameNum4Save, // time of this frame 
					1,// number to write 
					p2,
					biCompDef.bmiHeader.biSizeImage,
					t, // flags.... 
					NULL, NULL);
				vFrameNum4Save++;
				}
			}
		HeapFree(GetProcessHeap(),0,p2);
		if(psText && imposeTime) {
			n=AVIStreamLength(psText);
			CString S;
			S=CTime::GetCurrentTime().Format("%d/%m/%Y %H:%M:%S");
			AVIStreamWrite(psText,// stream pointer 
				n+1, // time of this frame 
				1,// number to write 
				(LPSTR)(LPCTSTR)S,
				S.GetLength()+1,
				AVIIF_KEYFRAME, // flags.... 
				NULL, NULL);
			}
		}

	aFrameNum4Save=0;
	saveWait4KeyFrame=1;
	psVideo=myps;
	return 1;

error:
	return 0;
	}

int CTV::endSaveFile() {
	PAVISTREAM myps;

	if(psVideo) {
		myps=psVideo;
		psVideo=NULL;
		DWORD ti=timeGetTime()+1000;
		while(ti>timeGetTime());		// aspetta che la routine callback video finisca eventualmente di salvare... MIGLIORARE!
		AVIStreamClose(myps);
		}
	if(psAudio) {
		myps=psAudio;
		psAudio=NULL;
		DWORD ti=timeGetTime()+1000;
		while(ti>timeGetTime());		// aspetta che la routine callback finisca eventualmente di salvare... MIGLIORARE!
		AVIStreamClose(psAudio);  
		}
	if(psText) 
		AVIStreamClose(psText);  
	psText=NULL;
	if(aviFile) 
		AVIFileClose(aviFile);  
	aviFile=NULL;
	AVIFileExit(); 
	return 1;
	}

int CTV::superImposeDateTime(LPBITMAPINFOHEADER bi,BYTE *d,COLORREF dColor) {
	static BYTE display[12] = { 0x3f, 0x6, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x7, 0x7f, 0x6f,0,0 };
	// 1 bit x ogni segmento, LSB=A...b6=G
	int xSize=bi->biWidth/28,ySize=bi->biHeight/16;
	int x,y,xStart,yStart,xStep,yStep;
	int i,n,n2;
	BYTE *p,*p1;
	char myTime[16];
	CString S;
	CTime ct=CTime::GetCurrentTime();


	i=imposeTime-1;
	if(!(i & 4)) {
		xSize=(xSize*2)/3;
		ySize=(ySize*2)/3;
		}
	xStep=(xSize/3)*3;
	yStep=-bi->biWidth*3;
	switch(i & 3) {
		case 0:			// sinistra in alto
			xStart=8;
			yStart=bi->biHeight-bi->biHeight/11;
			break;
		case 1:			// destra in alto
			xStart=(bi->biWidth-xSize*12);
			yStart=bi->biHeight-bi->biHeight/11;
			break;
		case 2:			// destra in basso
			xStart=(bi->biWidth-xSize*12);
			yStart=(ySize*7)/2;
			break;
		case 3:			// sinistra in basso
			xStart=8;
			yStart=(ySize*7)/2;
			break;
		default:
			return -1;
			break;
		}

	if(bi->biBitCount != 24)		// piccola protezione...
		return -1;

//	p=d+(bi->biWidth*3*(ySize+2))+(bi->biWidth*3)*(bi->biHeight/7);		// la bitmap e' bottom-up!
	p=d+(bi->biWidth*3*yStart);		// la bitmap e' bottom-up!
	S=ct.Format("%H:%M:%S");
	_tcscpy(myTime,(LPCTSTR)S);
	n2=8;

rifo:
	p1=p+xStart*3;
	for(i=0; i<n2; i++) {
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 1) {
				p1+=3;
				for(x=2; x<xSize; x++) {
					*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
					}
				p1+=3;
				}
			else
				p1+=xSize*3;
			}
		else
			p1+=(xSize/3)*3;
		p1+=xStep;		// ...
		}

	p+=yStep;
	p1=p+xStart*3;
	for(y=0; y<ySize/2; y++) {
		BYTE *p2=p1;
		for(i=0; i<n2; i++) {
			if(isdigit(n=myTime[i])) {
				if(display[n-'0'] & 0x20) {
					*(WORD *)p2=dColor; *(p2+2)=HIWORD(dColor);
					}
				p2+=xSize*3-3;
				if(display[n-'0'] & 0x2) {
					*(WORD *)p2=dColor; p2+=2; *p2++=HIWORD(dColor);
					}
				else
					p2+=3;
				p2+=(xSize/3)*3;
				}
			else
				p2+=(xSize/3)*6;
			}
		p1+=yStep;
		}

	p+=yStep*(ySize/2);
	p1=p+xStart*3;

	for(i=0; i<n2; i++) {
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 0x40) {
				p1+=3;
				for(x=2; x<xSize; x++) {
					*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
					}
				p1+=3;
				}
			else
				p1+=xSize*3;
			}
		else
			p1+=(xSize/3)*3;
		p1+=xStep;		// ...
		}

	p+=yStep;
	p1=p+xStart*3;
	for(y=0; y<ySize/2; y++) {
		BYTE *p2=p1;
		for(i=0; i<n2; i++) {
			if(isdigit(n=myTime[i])) {
				if(display[n-'0'] & 0x10) {
					*(WORD *)p2=dColor; *(p2+2)=HIWORD(dColor);
					}
				p2+=xSize*3-3;
				if(display[n-'0'] & 0x4) {
					*(WORD *)p2=dColor; p2+=2; *p2++=HIWORD(dColor);
					}
				else
					p2+=3;
				p2+=(xSize/3)*3;
				}
			else
				p2+=(xSize/3)*6;
			}
		p1+=yStep;
		}

	p+=yStep*(ySize/2);
	p1=p+xStart*3;

	for(i=0; i<n2; i++) {
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 8) {
				p1+=3;
				for(x=2; x<xSize; x++) {
					*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
					}
				p1+=3;
				}
			else
				p1+=xSize*3;
			}
		else
			p1+=(xSize/3)*3;
		p1+=xStep;
		}

	S=ct.Format("%d/%m/%Y");
	strcpy(myTime,(LPCTSTR)S);
	if(n2==8) {
		n2=10;
		p+=yStep*(ySize/2);
		goto rifo;
		}
	return 1;
	}

int CTV::superImposeText(CBitmap *b,const char *s,int flag) {
	int xSize,ySize;
	int x,y;
	int i,n;
	BYTE *p,*p1;
	COLORREF dColor=RGB(255,255,255);
	CString S;
	CFont myFont;
	CDC *dc,dc1;
	BITMAP bmp;
	RECT rc;
	CBitmap *oldB;

	b->GetBitmap(&bmp);
	rc.top=rc.left=0;
	rc.bottom=bmp.bmHeight;
	rc.right=bmp.bmWidth;
	xSize=bmp.bmWidth/28,ySize=bmp.bmHeight/16;

  myFont.CreateFont(xSize*flag,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
	dc=theApp.m_pMainWnd->GetDC();
	dc1.CreateCompatibleDC(dc);

	oldB=(CBitmap *)dc1.SelectObject(b);
//	CVidsendApp::renderBitmap(&dc1,b,&rc);
	dc1.TextOut(5,5,s,strlen(s));
	

	dc1.SelectObject(oldB);

	/*
	dc.SetOutputDC(hDC);
	dc2.CreateCompatibleDC(&dc);		// non e' necessario Delete: lo fa il distruttore!
	pBitmap=new CBitmap;
	if(!pBitmap)
		goto salva_non_ok;
//	pBitmap->CreateBitmap(myRect.right,myRect.bottom,1,8,NULL);
	pBitmap->CreateCompatibleBitmap(&dc,myRect.right,myRect.bottom);
	oldB=(CBitmap *)dc2.SelectObject(*pBitmap);
	dc2.BitBlt(0,0,myRect.right,myRect.bottom,&dc,0,0,SRCCOPY);

	delete pBitmap;*/


	theApp.m_pMainWnd->ReleaseDC(dc);
  DeleteObject(myFont);

	return 1;
	}

int CTV::superImposeText(LPBITMAPINFOHEADER bi,BYTE *d,COLORREF dColor) {
	static BYTE display[12] = { 0x3f, 0x6, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x7, 0x7f, 0x6f,0,0 };
	// 1 bit x ogni segmento, LSB=A...b6=G
	int xSize=bi->biWidth/28,ySize=bi->biHeight/16;
	int x,y,xStart,yStart,xStep,yStep;
	int i,n,n2;
	BYTE *p,*p1;
	char myTime[16];
	CString S;
	CTime ct=CTime::GetCurrentTime();

FINIRE!
	if(imposeTime) {
		i=imposeTime-1;
		if(!(i & 4)) {
			xSize=(xSize*2)/3;
			ySize=(ySize*2)/3;
			}
		xStep=(xSize/3)*3;
		yStep=-bi->biWidth*3;
		switch(i & 3) {
			case 0:			// sinistra in alto
				xStart=8;
				yStart=bi->biHeight-bi->biHeight/11;
				break;
			case 1:			// destra in alto
				xStart=(bi->biWidth-xSize*12);
				yStart=bi->biHeight-bi->biHeight/11;
				break;
			case 2:			// destra in basso
				xStart=(bi->biWidth-xSize*12);
				yStart=(ySize*7)/2;
				break;
			case 3:			// sinistra in basso
				xStart=8;
				yStart=(ySize*7)/2;
				break;
			default:
				return -1;
				break;
			}
		}
	else
		return 0;

	if(bi->biBitCount != 24)		// piccola protezione...
		return -1;

//	p=d+(bi->biWidth*3*(ySize+2))+(bi->biWidth*3)*(bi->biHeight/7);		// la bitmap e' bottom-up!
	p=d+(bi->biWidth*3*yStart);		// la bitmap e' bottom-up!
	S=ct.Format("%H:%M:%S");
	_tcscpy(myTime,(LPCTSTR)S);
	n2=8;

rifo:
	p1=p+xStart*3;
	for(i=0; i<n2; i++) {
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 1) {
				p1+=3;
				for(x=2; x<xSize; x++) {
					*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
					}
				p1+=3;
				}
			else
				p1+=xSize*3;
			}
		else
			p1+=(xSize/3)*3;
		p1+=xStep;		// ...
		}

	p+=yStep;
	p1=p+xStart*3;
	for(y=0; y<ySize/2; y++) {
		BYTE *p2=p1;
		for(i=0; i<n2; i++) {
			if(isdigit(n=myTime[i])) {
				if(display[n-'0'] & 0x20) {
					*(WORD *)p2=dColor; *(p2+2)=HIWORD(dColor);
					}
				p2+=xSize*3-3;
				if(display[n-'0'] & 0x2) {
					*(WORD *)p2=dColor; p2+=2; *p2++=HIWORD(dColor);
					}
				else
					p2+=3;
				p2+=(xSize/3)*3;
				}
			else
				p2+=(xSize/3)*6;
			}
		p1+=yStep;
		}

	p+=yStep*(ySize/2);
	p1=p+xStart*3;

	for(i=0; i<n2; i++) {
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 0x40) {
				p1+=3;
				for(x=2; x<xSize; x++) {
					*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
					}
				p1+=3;
				}
			else
				p1+=xSize*3;
			}
		else
			p1+=(xSize/3)*3;
		p1+=xStep;		// ...
		}

	p+=yStep;
	p1=p+xStart*3;
	for(y=0; y<ySize/2; y++) {
		BYTE *p2=p1;
		for(i=0; i<n2; i++) {
			if(isdigit(n=myTime[i])) {
				if(display[n-'0'] & 0x10) {
					*(WORD *)p2=dColor; *(p2+2)=HIWORD(dColor);
					}
				p2+=xSize*3-3;
				if(display[n-'0'] & 0x4) {
					*(WORD *)p2=dColor; p2+=2; *p2++=HIWORD(dColor);
					}
				else
					p2+=3;
				p2+=(xSize/3)*3;
				}
			else
				p2+=(xSize/3)*6;
			}
		p1+=yStep;
		}

	p+=yStep*(ySize/2);
	p1=p+xStart*3;

	for(i=0; i<n2; i++) {
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 8) {
				p1+=3;
				for(x=2; x<xSize; x++) {
					*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
					}
				p1+=3;
				}
			else
				p1+=xSize*3;
			}
		else
			p1+=(xSize/3)*3;
		p1+=xStep;
		}

	S=ct.Format("%d/%m/%Y");
	_tcscpy(myTime,(LPCTSTR)S);
	if(n2==8) {
		n2=10;
		p+=yStep*(ySize/2);
		goto rifo;
		}
	return 1;
	}

int CTV::convertColorBitmapToBN(LPBITMAPINFOHEADER bi,BYTE *d) {
	BYTE *p1,*p2;
	int x,y;
	int i,j;
	DWORD n;

	switch(bi->biBitCount) {
		case 24:
			x=bi->biWidth;
			y=bi->biHeight;
			while(y--) {
				for(i=0; i<x; i++) {
					n=*(DWORD *)d;
					j=LOBYTE(LOWORD(n))+HIBYTE(LOWORD(n))+LOBYTE(HIWORD(n));
					//usare coefficienti del colore??
					j/=3;
					*d++=RGB(j,j,j);
					*d++=RGB(j,j,j);
					*d++=RGB(j,j,j);
					}
				}
			break;
		case 16:
			x=bi->biWidth;
			y=bi->biHeight;
			while(y--) {
				for(i=0; i<x; i++) {
					n=*(WORD *)d;
					j=LOBYTE(LOWORD(n))+HIBYTE(LOWORD(n));
					//usare coefficienti del colore??
					j/=2;

					//FINIRE!!

					*d++=RGB(j,j,j);
					*d++=RGB(j,j,j);
					}
				}
			break;
		}

	return 1;
	}

int CTV::mirrorBitmap(LPBITMAPINFOHEADER bi,BYTE *d) {
	BYTE *p,*p1,*p2;
	int x,y;
	int i,j;
	DWORD n;

	switch(bi->biBitCount) {
		case 24:
			x=bi->biWidth;
			y=bi->biHeight;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x*3+3);
			if(p) {
				for(j=0; j<y; j++) {
					p2=d+j*(x*3);
					memcpy(p,p2,x*3);
					p1=p;
					p2=p2+((x-1)*3);
					for(i=0; i<x; i++) {
						n=*(DWORD *)p1;
						*(WORD *)p2=n;
						*(p2+2)=LOBYTE(HIWORD(n));
						p1+=3;
						p2-=3;
						}
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		case 16:
			//testare!
			x=bi->biWidth;
			y=bi->biHeight;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x*2+2);
			if(p) {
				for(j=0; j<y; j++) {
					p2=d+j*(x*2);
					memcpy(p,p2,x*2);
					p1=p;
					p2=p2+((x-1)*2);
					for(i=0; i<x; i++) {
						n=*(WORD *)p1;
						*(WORD *)p2=n;
						p1+=2;
						p2-=2;
						}
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		}

	return 1;
	}

int CTV::flipBitmap(LPBITMAPINFOHEADER bi,BYTE *d) {
	BYTE *p,*ps,*pd;
	int x,y;
	int i;

	switch(bi->biBitCount) {
		case 24:
			x=bi->biWidth*3;
			y=bi->biHeight;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x+3);
			if(p) {
				ps=d;
				pd=d+x*(y-1);
				y/=2;
				while(y--) {
					memcpy(p,pd,x);
					memcpy(pd,ps,x);
					memcpy(ps,p,x);
					ps+=x;
					pd-=x;
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		case 16:

			//testare!
			x=bi->biWidth*2;
			y=bi->biHeight;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x+2);
			if(p) {
				ps=d;
				pd=d+x*(y-1);
				y/=2;
				while(y--) {
					memcpy(p,pd,x);
					memcpy(pd,ps,x);
					memcpy(ps,p,x);
					ps+=x;
					pd-=x;
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		}

	return 1;
	}

int CTV::resampleBitmap(LPBITMAPINFOHEADER bi,BYTE *d,RECT *rcDest) {
	BYTE *p,*p1,*p2,*p3;
	int x,y,xRatio,yRatio;
	int i,j;
	DWORD n;

// COMPLETARE v. joshua-resample

	switch(bi->biBitCount) {
		case 24:
			x=bi->biWidth;
			y=bi->biHeight;
			xRatio=x/rcDest->right;
			yRatio=y/rcDest->bottom;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x*3+3);
			p3=d;
			if(p) {
				for(j=0; j<y; j+=yRatio) {
					p2=d+j*(x*3);
					memcpy(p,p2,x*3);
					p1=p;
					for(i=0; i<x; i+=xRatio) {
						n=*(DWORD *)p1;
						*(WORD *)p3=n;
						*(p3+2)=LOBYTE(HIWORD(n));
						p1+=xRatio*3;
						p3+=3;
						}
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;

		case 16:
			//testare!
			x=bi->biWidth;
			y=bi->biHeight;
			xRatio=x/rcDest->right;
			yRatio=y/rcDest->bottom;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x*2+2);
			p3=d;
			if(p) {
				for(j=0; j<y; j+=yRatio) {
					p2=d+j*(x*2);
					memcpy(p,p2,x*2);
					p1=p;
					for(i=0; i<x; i+=xRatio) {
						n=*(WORD *)p1;
						*(WORD *)p3=n;
						p1+=xRatio*2;
						p3+=2;
						}
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		}

	return 1;
	}

LRESULT CALLBACK StatusCallbackProc(HWND hWnd,int nID,LPSTR lpStatusText) { 
	char *p;

	p=(char *)GlobalAlloc(GPTR,1024);

  if(nID == 0) {              // Zero means clear old status messages 
		strcpy(p,"<nulla>");
    } 
	else {
  // Show the status ID and status text... 
		wsprintf(p, "Status# %d: %s", nID, lpStatusText); 
		}
  theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
  return (LRESULT)TRUE; 
  } 
 
LRESULT CALLBACK ErrorCallbackProc(HWND hWnd,int nErrID,LPSTR lpErrorText) { 
	char *p;
 
  if(nErrID == 0)            // starting a new major function 
    return TRUE;            // clear out old errors 
 
    // Show the error identifier and text 
  p=(char *)GlobalAlloc(GPTR,256);
  wsprintf(p, "Errore videocattura # %d", nErrID); 
	if(theApp.debugMode)
		if(theApp.FileSpool)
			*theApp.FileSpool << p;
  theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
//  MessageBox(hWnd,lpErrorText,gachBuffer,MB_OK | MB_ICONEXCLAMATION); 
 
  return (LRESULT)TRUE; 
  } 
 
  
LRESULT CALLBACK FrameCallbackProc(HWND hWnd, LPVIDEOHDR lpVHdr) { 
	char *p;
	static long gdwFrameNum;

  p=(char *)GlobalAlloc(GPTR,1024);
	wsprintf(p,"Preview frame# %ld ",gdwFrameNum++); 
  theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
  return (LRESULT)TRUE;
  } 

LRESULT CALLBACK VideoCallbackProc(HWND hWnd, LPVIDEOHDR lpVHdr) {
	CView *v;
	BYTE *pInp,*pSBuf=NULL,*pSBuf2=NULL;
	BOOL pInpAllocated=FALSE;
  int i,tFrame;
	DWORD l,t;
  struct AV_PACKET_HDR *avh;
	static BOOL bInSend=0;
//	static int tFrameDiv=1,frameDiv=1;
//	CTV *m_TV=(CTV *)capGetUserData(hWnd) /* non e' lpVHdr->dwUser !! */;  FA CAGARE!

	if(theApp.debugMode>2) {
		if(theApp.FileSpool) {
			char myBuf[128];
			wsprintf(myBuf,"millisecondi %u",lpVHdr->dwTimeCaptured);
			*theApp.FileSpool << myBuf;
		}

	if(lpVHdr->dwTimeCaptured < (m_TV->oldTimeCaptured+((1000-80)/m_TV->framesPerSec) ))	//correzioncina xche' il timing non e' precisissimo, ed è meglio anticipare un po'...
		return 0;		// PATCH per la merda di Logitech che ignora il setCaptureParms per quanto riguarda i frame per sec. (v. sopra)
	m_TV->oldTimeCaptured=lpVHdr->dwTimeCaptured;

	pInp=lpVHdr->lpData;

/*		if(f=fopen("c:\\myframe1.bmp","wb")) {
		fwrite(pInp,lpVHdr->dwBytesUsed,1,f);
		fclose(f);
		}*/
	if(!bInSend) {
		bInSend=TRUE;
	  if(theApp.theServer) {
			v=theApp.theServer->getView();
			if(theApp.theServer->Opzioni & CVidsendDoc2::sendVideo && !theApp.theServer->bPaused) {

				if(theApp.theServer->Opzioni & CVidsendDoc2::videoType) {
					pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxFrameSize+AV_PACKET_HDR_SIZE+100);
					pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;				// indirizzo del buffer DOPO la struct header!
					avh=(struct AV_PACKET_HDR *)pSBuf;
					avh->type=0;
#ifdef _STANDALONE_MODE
					avh->tag=MAKEFOURCC('D','G','2','0');
#else
					avh->tag=MAKEFOURCC('G','D','2','0');
#endif
	//					avh.psec=1000 / m_TV->framesPerSec;
					avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());	// anche lpVHdr->dwTimeCaptured
					memcpy(pSBuf2,&m_TV->biCompDef.bmiHeader,sizeof(BITMAPINFOHEADER));
					memcpy(pSBuf2+sizeof(BITMAPINFOHEADER),pInp,lpVHdr->dwBytesUsed);
					avh->len=lpVHdr->dwBytesUsed+sizeof(BITMAPINFOHEADER);
					avh->info=(lpVHdr->dwFlags & VHDR_KEYFRAME) ? AV_PACKET_INFO_KEYFRAME : 0;
					// in b8 ora c'è qbox AV_PACKET_INFO_QBOX

					if(theApp.debugMode) {
						char *p=(char *)GlobalAlloc(GPTR,1024);
						wsprintf(p,"VFrame (precomp)# %ld: lungo %ld (%ld)",m_TV->vFrameNum,lpVHdr->dwBufferLength,lpVHdr->dwBytesUsed); 
						theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
						}

					}
				else {

					if(m_TV->biRawBitmap.biCompression != 0) {
						if(m_TV->hICDe) {
							pInp=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->biBaseRawBitmap.biHeight*m_TV->biBaseRawBitmap.biWidth*m_TV->biBaseRawBitmap.biBitCount/8);
							if(pInp) {
								pInpAllocated=TRUE;
								i=ICDecompress(m_TV->hICDe,0 /*ICDECOMPRESS_HURRYUP*/,
									&m_TV->biRawBitmap,lpVHdr->lpData,&m_TV->biBaseRawBitmap,pInp);
								if(i == 0 /* se metto HURRYUP restituisce 1, ossia DONTDRAW (vfw.h) ma NON decomprime! */) {
									}
								else
									goto not_RGB;
								}
							else
								goto not_RGB;
							}
						else
							goto not_RGB;
						}

					if(m_TV->theFrame == (BYTE *)-1) {		// se richiesto...
						l=m_TV->biBaseRawBitmap.biHeight*m_TV->biBaseRawBitmap.biWidth*m_TV->biBaseRawBitmap.biBitCount/8;
						m_TV->theFrame=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,l);	// prelevo e salvo il fotogramma per uso altrove (p.es. salva img)
						memcpy(m_TV->theFrame,pInp,l);
						}

					if(theApp.theServer->Opzioni & CVidsendDoc2::forceBN) {
						m_TV->convertColorBitmapToBN(&m_TV->biRawDef.bmiHeader,pInp);
						}
					if(theApp.theServer->Opzioni & CVidsendDoc2::doFlip) {
						m_TV->flipBitmap(&m_TV->biRawDef.bmiHeader,pInp);
						}
					if(theApp.theServer->Opzioni & CVidsendDoc2::doMirror) {
						m_TV->mirrorBitmap(&m_TV->biRawDef.bmiHeader,pInp);
						}
					if(m_TV->imposeTime) {
						m_TV->superImposeDateTime(&m_TV->biRawDef.bmiHeader,pInp);
						}
					if(m_TV->imposeTextPos) { finire :)
						m_TV->superImposeText(&m_TV->biRawDef.bmiHeader,pInp,m_TV->imposeText);
						}
					if(!IsRectEmpty(&theApp.theServer->qualityBox)) {
						m_TV->superImposeBox(&m_TV->biRawDef.bmiHeader,pInp,&theApp.theServer->qualityBox);
						}
					if(theApp.theServer->myQV.compressor) {
						l=t=0;
						pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxFrameSize+AV_PACKET_HDR_SIZE+100);
						pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;			// indirizzo del buffer DOPO la struct header!
						avh=(struct AV_PACKET_HDR *)pSBuf;
						avh->type=0;
#ifdef _STANDALONE_MODE
						avh->tag=MAKEFOURCC('D','G','2','0');
#else
						avh->tag=MAKEFOURCC('G','D','2','0');
#endif
		//	ICSeqCompressFrameStart ???	ICSeqCompressFrame
						t=l=0;
						if(m_TV->framesPerSec >1)		// usare KFrame...
							tFrame=(m_TV->vFrameNum % m_TV->framesPerSec) ? 0 : ICCOMPRESS_KEYFRAME;
						else
							tFrame=(m_TV->vFrameNum & 1) ? ICCOMPRESS_KEYFRAME : 0;
						i=ICCompress(m_TV->hICCo,tFrame,
							&m_TV->biCompDef.bmiHeader,pSBuf2+sizeof(BITMAPINFOHEADER),&m_TV->biBaseRawBitmap,pInp,
							&l,&t,m_TV->vFrameNum,0/*2500*/,theApp.theServer->myQV.quality,
							NULL,NULL);
						if(i == ICERR_OK) {
//							CFile mf;

	//						mf.Open("c:\\frame0.bmp",CFile::modeWrite | CFile::modeCreate);
	//						mf.Write(pInp,lpVHdr->dwBytesUsed);
	//						mf.Close();
	//						mf.Open("c:\\frame.bmp",CFile::modeWrite | CFile::modeCreate);
	//						mf.Write(pSBuf,l);
	//						mf.Close();
							memcpy(pSBuf2,&m_TV->biCompDef.bmiHeader,sizeof(BITMAPINFOHEADER));
							avh->len=m_TV->biCompDef.bmiHeader.biSizeImage+sizeof(BITMAPINFOHEADER);
	//							avh.psec=1000 / m_TV->framesPerSec;
							avh->info=(t & AVIIF_KEYFRAME) ? AV_PACKET_INFO_KEYFRAME : 0;
t;
						// in b8 ora c'è qbox AV_PACKET_INFO_QBOX
							avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());

							if(theApp.debugMode>1) {
								char *p=(char *)GlobalAlloc(GPTR,1024);
	//						wsprintf(p,"VFrame# %u: lungo %u (%u)",m_TV->gdvFrameNum,lpVHdr->dwBytesUsed,l); 
								wsprintf(p,"oldTime %u, time %u",m_TV->oldTimeCaptured,lpVHdr->dwTimeCaptured); 
								theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
								}
	//						memcpy(avh.lpData,pSBuf,l+40);

							}
						else {
not_RGB:
							MessageBeep(-1);
							if(pSBuf) 
								HeapFree(GetProcessHeap(),0,pSBuf);
							goto fine;
							}
						}
					else {
						pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxFrameSize+AV_PACKET_HDR_SIZE+100);
						pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;					// indirizzo del buffer DOPO la struct header!
						avh=(struct AV_PACKET_HDR *)pSBuf;
						avh->type=0;
#ifdef _STANDALONE_MODE
						avh->tag=MAKEFOURCC('D','G','2','0');
#else
						avh->tag=MAKEFOURCC('G','D','2','0');
#endif
	//						avh.psec=1000 / m_TV->framesPerSec;
						avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
						avh->len=lpVHdr->dwBytesUsed+sizeof(BITMAPINFOHEADER);
						memcpy(pSBuf2,&m_TV->biRawDef.bmiHeader,sizeof(BITMAPINFOHEADER));
						memcpy(pSBuf2+sizeof(BITMAPINFOHEADER),pInp,lpVHdr->dwBytesUsed);
						avh->info=lpVHdr->dwFlags & VHDR_KEYFRAME ? AV_PACKET_INFO_KEYFRAME : 0;
						// in b8 ora c'è qbox AV_PACKET_INFO_QBOX

						if(theApp.debugMode) {
							char *p=(char *)GlobalAlloc(GPTR,1024);
							wsprintf(p,"VFrame (raw) # %ld: lungo %ld (%ld)",m_TV->vFrameNum,lpVHdr->dwBufferLength,lpVHdr->dwBytesUsed); 
							theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
							}

						}
					}

fine_ok:
				m_TV->vFrameNum;
				if(m_TV->aviFile && m_TV->psVideo) {
					if(m_TV->opzioniSave & CVidsendDoc2::quantiFrame)
						m_TV->saveWait4KeyFrame=1;	// con questo trucco salvo solo i k-frame ossia 1 al secondo!
					if(m_TV->saveWait4KeyFrame) {
						if(avh->info & AV_PACKET_INFO_KEYFRAME)
							m_TV->saveWait4KeyFrame=0;
						else
							goto skipSave;
						}
					i= AVIStreamWrite(m_TV->psVideo,// stream pointer 
						m_TV->vFrameNum4Save, // time of this frame 
						1,// number to write 
						pSBuf2+sizeof(BITMAPINFOHEADER),
						avh->len-sizeof(BITMAPINFOHEADER),	/*m_TV->biCompDef.bmiHeader.biSizeImage,*/
						avh->info & AV_PACKET_INFO_KEYFRAME, // flags.... 
						NULL, NULL);
					if(i!=AVIERR_OK)
						if(theApp.debugMode)
							*theApp.FileSpool << "errore in stream video salva";
					if(m_TV->psText && m_TV->imposeTime && !(m_TV->vFrameNum4Save % m_TV->framesPerSec)) {
						CString S;
						S=CTime::GetCurrentTime().Format("%d/%m/%Y %H:%M:%S");
						i= AVIStreamWrite(m_TV->psText,// stream pointer 
							m_TV->vFrameNum4Save/m_TV->framesPerSec, // time of this frame 
							1,// number to write 
							(LPSTR)(LPCTSTR)S,
							S.GetLength()+1,
							avh->info & AV_PACKET_INFO_KEYFRAME, // flags.... 
							NULL, NULL);
						}
					m_TV->vFrameNum4Save++;
skipSave:	;
					}

				if(v && pSBuf) {
					avh->reserved1=avh->reserved2=0;
					v->PostMessage(WM_VIDEOFRAME_READY,(WPARAM)pSBuf,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
					}

fine: ;
				}
			else {
				if(theApp.debugMode) {
					char *p=(char *)GlobalAlloc(GPTR,1024);
					wsprintf(p,"Non mando video!"); 
					theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
					}
				}

			}
		if(pInpAllocated)
			HeapFree(GetProcessHeap(),0,pInp);
		bInSend=FALSE;
		}
	else {
		if(theApp.debugMode) {
			char *p=(char *)GlobalAlloc(GPTR,1024);
			wsprintf(p,"Rientrato!"); 
		  theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
			}
		}

//	wsprintf(p,"%ld",m_TV->gdvFrameNum++); 


  return (LRESULT)TRUE;
  }

LRESULT CALLBACK WaveCallbackProc(HWND hWnd, LPWAVEHDR lpWHdr) {
	CView *v;
	char *p;
  BYTE *pSBuf,*pSBuf2;
  struct AV_PACKET_HDR *avh=NULL;
//  struct AV_PACKET_HDR avh;
	ACMSTREAMHEADER hhacm;
	static BOOL bInSend=0;
	int i;

  if(!bInSend) {
		bInSend=TRUE;
	  if(theApp.theServer) {
			v=theApp.theServer->getView();
			if(theApp.theServer->bAudio && !theApp.theServer->bPaused) {
	//			l+=sizeof(WAVEFORMATEX);

				pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxWaveoutSize);
				pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;
				avh=(struct AV_PACKET_HDR *)pSBuf;

#ifdef _STANDALONE_MODE
				avh->tag=MAKEFOURCC('D','G','2','0');
#else
				avh->tag=MAKEFOURCC('G','D','2','0');
#endif
				avh->type=1;
	//				avh.psec=1000;
				avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
				avh->info=0;


				hhacm.cbStruct=sizeof(ACMSTREAMHEADER);
				hhacm.fdwStatus=0;
				hhacm.dwUser=(DWORD)0 /*this*/;
				hhacm.pbSrc=(BYTE *)lpWHdr->lpData;
				hhacm.cbSrcLength=lpWHdr->dwBytesRecorded;
		//			hhacm.cbSrcLengthUsed=0;
				hhacm.dwSrcUser=0;
				hhacm.pbDst=pSBuf2;
				hhacm.cbDstLength=m_TV->maxWaveoutSize;
		//			hhacm.cbDstLengthUsed=0;
				hhacm.dwDstUser=0;
				if(!acmStreamPrepareHeader(m_TV->hAcm,&hhacm,0)) {
					i=acmStreamConvert(m_TV->hAcm,&hhacm,ACM_STREAMCONVERTF_BLOCKALIGN);
				//	wsprintf(myBuf,"convertito: %d, %d bytes",i,l);
				//	AfxMessageBox(myBuf);
					acmStreamUnprepareHeader(m_TV->hAcm,&hhacm,0);

					avh->len=hhacm.cbDstLengthUsed;

					if(m_TV->aviFile && m_TV->psAudio) {
						i= AVIStreamWrite(m_TV->psAudio,// stream pointer 
							m_TV->aFrameNum, // time of this frame 
							1,// number to write 
							pSBuf2,
							hhacm.cbDstLengthUsed,
							avh->info, // flags.... 
							NULL, NULL);
						}



					if(theApp.debugMode) {
						p=(char *)GlobalAlloc(GPTR,1024);
						wsprintf(p,"AFrameT# %ld: lungo %ld (%ld) %ld",m_TV->gdaFrameNum,hhacm.cbDstLengthUsed,lpWHdr->dwBytesRecorded,lpWHdr->dwBufferLength); 
						theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
						}
					if(v) {
						avh->reserved1=avh->reserved2=0;
						v->PostMessage(WM_AUDIOFRAME_READY,(WPARAM)pSBuf,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
//					if(theApp.theServer->streamSocketA->Manda(&avh,pSBuf,hhacm.cbDstLengthUsed))
//						m_TV->aFrameNum;
						}
					}
				}
			}
		bInSend=FALSE;
		}
  return (LRESULT)TRUE;
  }




#else						// USA_DIRECTX




#include <dshow.h>
#include <qedit.h>
#include <atlbase.h>
#include <windows.h>
#include <vector>
#include <math.h>
#include "vidsenddshow.h"

#include <stdlib.h>


#define DIRECTSHOW_CAPTURE_CLASS "DSHOWCAP_CLASS"

// sample callback routine
SampleGrabberCallback::SampleGrabberCallback(HWND notifWnd) 
	: refcount(1), m_hWnd(notifWnd) {

	buffer=NULL;
	bufferSize=0;
	m_Fps=0;
	lastTimeCaptured=0;
	event = CreateEvent(0, FALSE, FALSE, "");
	if(!event) {
		throw IVideoCapture::Exception("SampleGrabberCallback::ctor - Impossibile creare un Event per la sincronizzazione");
		}

	m_hWnd=notifWnd;
	}

SampleGrabberCallback::~SampleGrabberCallback() {
	CloseHandle(event);
	}

  // referance counting.
STDMETHODIMP_(ULONG) SampleGrabberCallback::AddRef()	{ 
	return ++refcount;
	}

STDMETHODIMP_(ULONG) SampleGrabberCallback::Release() {
		
	--refcount;
	if(refcount == 0)	{
		delete this;
		return 0;
		}
	return refcount;
	}

STDMETHODIMP SampleGrabberCallback::QueryInterface(REFIID riid, void** ppvObject) {
  
  if(!ppvObject) 
		return E_POINTER;
  if(riid == __uuidof(IUnknown)) {
    *ppvObject = static_cast<IUnknown*>(this);
		AddRef();
    return S_OK;
    }
  if(riid == __uuidof(ISampleGrabberCB)) {
    *ppvObject = static_cast<ISampleGrabberCB*>(this);
		AddRef();
    return S_OK;
    }
  return E_NOTIMPL;
	}

STDMETHODIMP SampleGrabberCallback::SampleCB(double Time, IMediaSample* sample) {
	
	return E_NOTIMPL;
  }

STDMETHODIMP SampleGrabberCallback::BufferCB(double time, BYTE* buffer, long bufferSize) {
	double n;

	// forse si dovrebbe separare il Grab-Per-Preview dal Grab-Per-Cattura... 2 msg diversi? o Preview directX?
//	ASSERT(0);
	if(m_Fps) {
		n=1.0/m_Fps;
		if(time < (lastTimeCaptured+n)) 
		  return S_OK;
		}
	this->buffer = buffer;
	this->bufferSize = bufferSize;
	if(m_hWnd) {
		PostMessage(m_hWnd,WM_GRABBED_BUFFER,bufferSize,(LPARAM)buffer);
		}
	SetEvent(event);
	lastTimeCaptured=SampleGrabberCallback::round(time,1.0/m_Fps);				// serve causa non-precisione di time e arrotondamenti... su alcune schede!

  return S_OK;
  }

void SampleGrabberCallback::Reset() {
	ResetEvent(event);
	}

void SampleGrabberCallback::WaitForCallback(DWORD t) {
	WaitForSingleObject(event, t);
	}

void SampleGrabberCallback::SetWindow(HWND hWnd) { 
	m_hWnd=hWnd;
	}

void SampleGrabberCallback::SetFramePerSecond(WORD myFps) { 
	m_Fps=myFps;
	}

double SampleGrabberCallback::round(double d,double n) {
	double d1;

	d1=(double)((int)(d/n));
	d=d1*n;
	return d;
	}



//
// construction
//
DShowVideoCapture::DShowVideoCapture(int deviceIndex,HWND hWnd)
	: crossbar(0),analogVideo(0),videoAmp(0)
	, sampleGrabberCB(0)
	, isRunning(false) {
	m_hWnd=hWnd/*NULL*/;
	doPreview=TRUE;

	HRESULT hr = E_FAIL;

	hr = CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::ctor - Impossibile inizializzare COM");
		}

	CreateFilters(deviceIndex);

	// Create the filter graph.
  hr = filterGraph.CoCreateInstance(CLSID_FilterGraph);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::ctor - Impossibile creare FilterGraph");
		}
    
  // Create the capture graph builder.
  hr = graphBuilder.CoCreateInstance(CLSID_CaptureGraphBuilder2);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::ctor - Impossibile creare CaptureGraphBuilder2");
		}
    
	// set the filter graph
	hr = graphBuilder->SetFiltergraph(filterGraph);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::ctor - Impossibile assegnare il filtro graph al graph builder");
		}
    
	// get filter control interface
	hr = filterGraph->QueryInterface(&control);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::ctor - Impossibile ottenere l'interfaccia IMediaControl");
		}

	// this effectively renders the graph
	DisconnectAllFilters();
	ConnectAllFilters();
	}


//
// destruction
//
DShowVideoCapture::~DShowVideoCapture() {

	// release all COM objects before calling CoUninitialize
	if(control) 
		control->Stop();

	control.Detach()->Release();
	graphBuilder.Detach()->Release();
	filterGraph.Detach()->Release();
	
	ReleaseFilters();

	CoUninitialize();
	}


//
// start capture
//
void DShowVideoCapture::StartCapture() {
	HRESULT hr = E_FAIL;

	// clear any pending frames
	sampleGrabberCB->Reset();

	// start the filter graph running
	hr = control->Run();
	if(FAILED(hr))	{
		throw Exception("DShowVideoCapture::StartCapture - Impossibile far partire il filtro graph");
		}

	isRunning = true;

	// wait for a frame to be available
//	WaitForNextFrame();
	//NO!!!
	}


//
// stop capture
//
void DShowVideoCapture::StopCapture() {
	
	control->Stop();
	sampleGrabberCB->Reset();
	isRunning = false;
	}


//
// wait for the next frame
//
void DShowVideoCapture::WaitForNextFrame() {
	
	if(!IsCapturing()) {
		throw Exception("DShowVideoCapture::WaitForNextFrame - Non è stato fatto StartCapture");
		}
	sampleGrabberCB->WaitForCallback(4000);			// è un tempo ragionevole...!
	}

//
// return a pointer to the current frame data
//
BYTE *DShowVideoCapture::GetCurrentFrame() {

	if(!IsCapturing())	{
		throw Exception("DShowVideoCapture::GetCurrentFrame - Non è stato fatto StartCapture");
		}

	return sampleGrabberCB->buffer;
	}

//
// return the size of the current frame
//
int DShowVideoCapture::GetCurrentFrameSize() {
	
	if(!IsCapturing()) {
		throw Exception("DShowVideoCapture::GetCurrentFrameSize - Non è stato fatto StartCapture");
		}

	return sampleGrabberCB->bufferSize;
	}



//
// find the capture device
//
IBaseFilter* DShowVideoCapture::CreateCaptureDevice(int deviceIndex) {
  HRESULT hr = E_FAIL;
  CComPtr<IBaseFilter> pFilter;
  CComPtr<ICreateDevEnum> pSysDevEnum;
  CComPtr<IEnumMoniker> pEnumCat;
  CComPtr<IMoniker> pMoniker;
  ULONG cFetched;

    // Create the System Device Enumerator.
  hr = pSysDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateCaptureDevice - Impossibile creare SystemDeviceEnum");
		}

  // Obtain a class enumerator for the video compressor category.
  hr = pSysDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumCat, 0);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateCaptureDevice - Impossibile creare un enumeratore per la VideoInputDeviceCategory");
		}
  
  if(!pEnumCat)
		return NULL;

  // Enumerate the monikers.
	for(int i=0; i<=deviceIndex;  ++i) {
		// get next moniker
		hr = pEnumCat->Next(1, &pMoniker, &cFetched);
		if(FAILED(hr)) {
			throw Exception("DShowVideoCapture::CreateCaptureDevice - Impossibile trovare un dispositivo di cattura");
			}
		}

// To retrieve the filter's friendly name, do the following:
	IPropertyBag *pPropBag;
	hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);
	if(SUCCEEDED(hr)) {
		VARIANT varName;
		VariantInit(&varName);
		hr = pPropBag->Read(L"FriendlyName", &varName, 0);
		if(SUCCEEDED(hr))	{
			captureDeviceName=varName.bstrVal;
			}
		VariantClear(&varName);
		}

	// load the capture device at this moniker location
	hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pFilter);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateCaptureDevice - Impossibile trovare un dispositivo di cattura");
		}

	IBaseFilter* result = pFilter;
	result->AddRef();
	return result;
	}



//
// create the capture filter, the sample grabber, and the null renderer
//
void DShowVideoCapture::CreateFilters(int deviceIndex) {
	HRESULT hr = E_FAIL;
	VIDEOINFOHEADER vih;

	// find the video capture device
  captureFilter = CreateCaptureDevice(deviceIndex);

	// create sample grabber
	hr = sampleGrabberFilter.CoCreateInstance(CLSID_SampleGrabber);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateFilters - Impossibile creare il grabber di campionamento");
		}

	// get interface for sample grabber
	hr = sampleGrabberFilter.QueryInterface(&sampleGrabber);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateFilters - Impossibile ottenere un'interfaccia al grabber di campionamento");
		}

	// set the media type of the sample grabber
	AM_MEDIA_TYPE mt;
	ZeroMemory(&mt, sizeof (AM_MEDIA_TYPE));
	mt.majortype = /*MEDIATYPE_AnalogVideo*/ MEDIATYPE_Video;
	mt.subtype = GUID_NULL;			// we don't care about the sub type
//    mt.subtype=MEDIASUBTYPE_RGB24 /*MEDIASUBTYPE_AnalogVideo_PAL_B*/ /*AnalogVideo_PAL_B*/;
	// forse serve!!
	mt.formattype = FORMAT_VideoInfo;	
/*	mt.cbFormat=sizeof(VIDEOINFOHEADER);
	ZeroMemory(&vih,sizeof(VIDEOINFOHEADER));
	mt.pbFormat=(BYTE *)&vih;*/

	hr = sampleGrabber->SetMediaType(&mt);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateFilters - Impossibile impostare il media type desiderato per il grabber di campionamento");
		}

	// enable buffering for the sample grabber
	hr = sampleGrabber->SetBufferSamples(TRUE);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateFilters - Impossibile abilitare la bufferizzazione del campionamento sul grabber di campionamento");
		}

	// set the callback for the sample grabber
	sampleGrabberCB = new SampleGrabberCallback(m_hWnd);	// create the callback instance
	hr = sampleGrabber->SetCallback(sampleGrabberCB, 1);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateFilters - Impossibile impostare callback");
		}


	// create the null renderer
	hr = nullRendererFilter.CoCreateInstance (CLSID_NullRenderer);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateFilters - Impossibile creare il NullRenderer");
		}

/*	hr = previewRendererFilter.CoCreateInstance(CLSID_VideoRenderer);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CreateFilters - Impossibile creare il PreviewRenderer");
		}*/

	}


//
// release the filters
//
void DShowVideoCapture::ReleaseFilters() {

	if(crossbar) { 
		crossbar->Release(); 
		crossbar = NULL; 
		}
	if(sampleGrabberCB) { 
		sampleGrabberCB->Release(); 
		sampleGrabberCB = NULL; 
		}
//	videoRendererFilter.Detach()->Release();
	nullRendererFilter.Detach()->Release();
	sampleGrabber.Detach()->Release();
	sampleGrabberFilter.Detach()->Release();
	captureFilter.Detach()->Release();

	if(analogVideo) 
		analogVideo->Release();
	analogVideo=NULL;
	if(videoAmp) 
		videoAmp->Release();
	videoAmp=NULL;
	}




//
// connect the filters
// essentially render the graph
//
void DShowVideoCapture::ConnectAllFilters() {
	HRESULT hr = E_FAIL;

	// render the graph
	hr = graphBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, captureFilter, sampleGrabberFilter, nullRendererFilter);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::ConnectAllFilters - Impossibile rendere lo stream");
		}

	// get the cross bar filter if one is attached to this graph
	if(crossbar) 
		crossbar->Release();
	hr = graphBuilder->FindInterface(&LOOK_UPSTREAM_ONLY, NULL, captureFilter, IID_IAMCrossbar, (void**)&crossbar);
	if(FAILED(hr)) {
		crossbar = 0;
		}

	// GD 2005 (per PAL/NTSC): get the other filter if one is attached to this graph
	if(analogVideo) 
		analogVideo->Release();
	hr = captureFilter->QueryInterface(IID_IAMAnalogVideoDecoder, (void**)&analogVideo);
	if(FAILED(hr)) {
		analogVideo = 0;
		}
	if(videoAmp) 
		videoAmp->Release();
	hr = captureFilter->QueryInterface(IID_IAMVideoProcAmp, (void**)&videoAmp);
	if(FAILED(hr)) {
		videoAmp = 0;
		}

	// cache the video format
	CacheVideoFormat();
	}


//
// disconnect all the filters from each other
// this is done by removing all the filters from the graph
// and then adding the capture, sample grabber, and null renderer back to the graph
//
void DShowVideoCapture::DisconnectAllFilters() {
	HRESULT hr = E_FAIL;

	// make sure the graph is stopped
	control->Stop();

	// enumerate current filters
	CComPtr<IEnumFilters> filterEnum;
	hr = filterGraph->EnumFilters(&filterEnum);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::DisconnectAllFilters - Impossibile enumerare il filtro graph");
		}

	// enumerate each filter and remove it from the graph
	CComPtr<IBaseFilter> filter;
	ULONG count;
	while(filterEnum->Next(1, &filter, &count) == S_OK)	{	
		// once we remove a filter from the graph the enumeration is invalid, so we reset it
		filterGraph->RemoveFilter(filter);
		filter.Detach()->Release();	// free reference
		filterEnum->Reset();
		}


	// add the capture filter to the graph
  hr = filterGraph->AddFilter(captureFilter, L"Capture Filter");
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::DisconnectAllFilters - Impossibile aggiungere il filtro di cattura al filtro graph");
		}

	// add the sample grabber to the graph
	hr = filterGraph->AddFilter(sampleGrabberFilter, L"Sample Grabber Filter");
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::DisconnectAllFilters - Impossibile aggiungere il grabber dei campioni al filtro graph");
		}

	// add the null renderer to the graph
	hr = filterGraph->AddFilter(nullRendererFilter, L"Null Renderer Filter");
	if(FAILED(hr))	{
		throw Exception("DShowVideoCapture::DisconnectAllFilters - Impossibile aggiungere il NullRenderer al graph");
		}

	// aggiungere il preview-renderer al graph
/*	hr = filterGraph->AddFilter(previewRendererFilter, L"Null Renderer Filter");
	if(FAILED(hr))	{
		throw Exception("DShowVideoCapture::DisconnectAllFilters - Impossibile aggiungere il PreviewRenderer al graph");
		}*/

	}


//
// Enumerate the available video modes
//
std::vector<VideoFormat> DShowVideoCapture::EnumerateNativeVideoFormats() {
	std::vector<VideoFormat> result;
	HRESULT hr = E_FAIL;

	CComPtr<IAMStreamConfig> config;
	hr = graphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, captureFilter, IID_IAMStreamConfig, (void**)&config);
	if(FAILED(hr))	{
		throw Exception("DShowVideoCapture::EnumerateNativeVideoFormats - Impossibile ottenere l'interfaccia di configurazione dello stream per il pin di cattura");
		}

	int count = 0;
	int size = 0;
	hr = config->GetNumberOfCapabilities(&count, &size);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::EnumerateNativeVideoFormats - Impossibile ottenere il numero dei formati disponibili");
		}

	// Check the size to make sure we pass in the correct structure.
	if(size == sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
		// Use the video capabilities structure.

		for(int index=0;  index < count;  ++index) {
			VIDEO_STREAM_CONFIG_CAPS scc;
			AM_MEDIA_TYPE* mt;
			hr = config->GetStreamCaps(index, &mt, (BYTE*)&scc);
			if(SUCCEEDED(hr))	{
				// if format is video and supports the VIDEOINFOHEADER, then add it to the enumeration list
				if((mt->majortype == MEDIATYPE_Video) &&
					(mt->formattype == FORMAT_VideoInfo) &&
					(mt->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
//					(mt->subtype == MEDIASUBTYPE_RGB24) &&		// x Pinnacle 2005!
// NO! sbaglia formato su Pinnacle Martini @#[]!
					(mt->pbFormat != NULL)) {
					VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt->pbFormat;
					// vih contains the detailed format information.
					
					// create VideoFormat descriptor and add it to the result vector
					VideoFormat vf(vih->bmiHeader.biWidth, vih->bmiHeader.biHeight, vih->bmiHeader.biBitCount, vih->bmiHeader.biCompression);
					result.push_back(vf);
					}

				// Delete the media type
				CoTaskMemFree(mt->pbFormat);
				CoTaskMemFree(mt);
				}
			}
		}

	return result;
	}


//
// set format
// index is an index from EnumerateNativeVideoFormats
//
void DShowVideoCapture::SetNativeFormat(int enumerationIndex) {
	HRESULT hr = E_FAIL;

	DisconnectAllFilters();

	CComPtr<IAMStreamConfig> config;
	hr = graphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, captureFilter, IID_IAMStreamConfig, (void**)&config);
	if(FAILED(hr))	{
		throw Exception("DShowVideoCapture::SetNativeFormat - Impossibile ottenere l'interfaccia IAMStreamConfig per il pin di cattura");
		}

	
	VIDEO_STREAM_CONFIG_CAPS scc;
	AM_MEDIA_TYPE* mt;
	hr = config->GetStreamCaps(enumerationIndex, &mt, (BYTE*)&scc);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::SetNativeFormat - Impossibile ottenere le capacità di stream per l'indice specificato");
		}

	// set the media type to this
	hr = config->SetFormat(mt);

	// Delete the media type
	CoTaskMemFree(mt->pbFormat);
	CoTaskMemFree(mt);

	// check error
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::SetNativeFormat - Impossibile impostare il formato video");
		}

	ConnectAllFilters();
	}


//
// set custom format
//
void DShowVideoCapture::SetFormat(int width, int height, int bitsPerPixel, WORD framePerSecond, 
																	WORD framePerSecond_hard, unsigned long fourCC) {
	HRESULT hr = E_FAIL;

	DisconnectAllFilters();

	// determine the best match with a native mode
	int index = FindClosestNativeFormat(width, height, bitsPerPixel, fourCC);
//		ASSERT(0);

	if(index != -1) {

		// obtain the mode for the nearest native format, and modify its specs

		CComPtr<IAMStreamConfig> config;
		hr = graphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, captureFilter, IID_IAMStreamConfig, (void**)&config);
		if(FAILED(hr)) {
			throw Exception("DShowVideoCapture::SetFormat - Impossibile ottenere l'interfaccia IAMStreamConfig per il pin di cattura");
			}


		VIDEO_STREAM_CONFIG_CAPS scc;
		AM_MEDIA_TYPE* mt;
		hr = config->GetStreamCaps(index, &mt, (BYTE*)&scc);
		if(FAILED(hr)) {
			throw Exception("DShowVideoCapture::SetFormat - Impossibile ottenere le capacità di flusso per il modo richiesto");
			}

		// modify this media type and set the capture filter output to this media type

		mt->lSampleSize = width*height * ((bitsPerPixel+7)>>3);
		VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt->pbFormat;

rifo:
		vih->bmiHeader.biWidth = width;
		vih->bmiHeader.biHeight = height;
		vih->bmiHeader.biBitCount = bitsPerPixel;
		vih->bmiHeader.biCompression = fourCC;
		vih->bmiHeader.biSizeImage = mt->lSampleSize;
		vih->AvgTimePerFrame =(LONGLONG)(10000000 / framePerSecond);		// in 100nS units...

		if(framePerSecond_hard)		// per ora non usato... gestire??
			vih->AvgTimePerFrame =(LONGLONG)(10000000 / framePerSecond_hard);		// in 100nS units...
		// hmm, la Pinnacle nasce con 30fps... ma con questo qua sopra si incasina...ossia dopo un po' di secondi ritarda a caso...


	//	ASSERT(0);

		hr = config->SetFormat(mt);
		if(FAILED(hr) && framePerSecond_hard<2) {
			framePerSecond_hard=2;

			if(theApp.debugMode)
				if(theApp.FileSpool)
					theApp.FileSpool->print(CLogFile::flagInfo,"provo FramesPerSecond_HARD=2");

			goto rifo;			// patch x Pinnacle di mer#a, NON è perfetto a causa di discorso KEY-FRAME e altro...
			}


		// Delete the media type
		CoTaskMemFree(mt->pbFormat);
		CoTaskMemFree(mt);

		// check error
		if(FAILED(hr)) {
			throw Exception("DShowVideoCapture::SetFormat - Impossibile impostare il formato video");
			}

		else {

			ConnectAllFilters();
	//	if(FAILED(hr))			// se fallisce (tra l'altro!) l'impostazione fps hardware, userò questa:
		// o SERVE CMQ, vista la SampleGrabberCallback::BufferCB sopra...
			sampleGrabberCB->SetFramePerSecond(framePerSecond);

			}
		}

	}




//
// caches the current video format
//
void DShowVideoCapture::CacheVideoFormat() {	// get the media type that the sample grabber is processing
	HRESULT hr = E_FAIL;

	AM_MEDIA_TYPE mt;
	hr = sampleGrabber->GetConnectedMediaType(&mt);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::CacheVideoFormat - Impossibile ottenere i tipi di media connessi");
		}

	VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt.pbFormat;

	// refresh cached videoFormat
	videoFormat.width = vih->bmiHeader.biWidth;
	videoFormat.height = vih->bmiHeader.biHeight;
	videoFormat.bitsPerPixel = vih->bmiHeader.biBitCount;
	videoFormat.fourCC = vih->bmiHeader.biCompression;

	// free media block
	CoTaskMemFree(mt.pbFormat);
	}



//
// returns the index of the closest native format
// listed in the EnumerateNativeVideoFormats() function
int DShowVideoCapture::FindClosestNativeFormat(int width, int height, 
																							 int bitsPerPixel, // 0 = qualsiasi (24, 16...)
																							 unsigned int fourCC	// -1 = qualsiasi (RGB, YUV...)
																							 ) {
	std::vector<VideoFormat> list = EnumerateNativeVideoFormats();

	int size = list.size();
	int index = 0;
	int closestIndex = -1;
	while(index < size)	{
		VideoFormat vf = list[index];

		// fourCCs must match
		if(fourCC==0xffffffff || vf.fourCC == fourCC)	{
			// bitsPerPixel must match
			if(!bitsPerPixel || vf.bitsPerPixel == bitsPerPixel) {
				if(closestIndex == -1) {	// might be the best match
					closestIndex = index;
					}
				else {	// see if width and height is closer

					// calculate distance with closestIndex
					int closestWidth = list[closestIndex].width;
					int closestHeight = list[closestIndex].height;
					int dWidth = width - closestWidth;
					int dHeight = height - closestHeight;
					float closestDif = sqrtf (dWidth*dWidth + dHeight*dHeight);

					// calculate distance with this VideoFormat
					dWidth = width - vf.width;
					dHeight = height - vf.height;
					float dif = sqrtf (dWidth*dWidth + dHeight*dHeight);

					if(dif < closestDif)	{	// this difference is less
						closestIndex = index;
						}
					}
				
				}
			}

		++index;
		}

	if(closestIndex == -1)	{	// did not find any match
		throw Exception("DShowVideoCapture::FindClosestNativeFormat - bitsPerPixel e fourCC non sono supportati in alcuno dei formati nativi da questo dispositivo");
		}

	return closestIndex;
	}



//
// enumerate the crossbar input pins
//
std::vector<int> DShowVideoCapture::EnumerateCrossbarInputs() {
	HRESULT hr = E_FAIL;
	std::vector<int> result;

	if(crossbar == 0) 
		return result; // no crossbar

	DisconnectAllFilters();

	long outputPinCount = 0;
	long inputPinCount = 0;
	hr = crossbar->get_PinCounts (&outputPinCount, &inputPinCount);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::EnumerateCrossbarInputs - Impossibile ottenere il totale dei pin del crossbar");
		}

	for(int i=0;  i < inputPinCount;  ++ i)	{
		long related = 0;
		long type;
		hr = crossbar->get_CrossbarPinInfo(TRUE, i, &related, &type);
		if(SUCCEEDED(hr)) {
			result.push_back (type);
			}
		}

	ConnectAllFilters();

	return result;
	}



//
// enumerate the crossbar output pins
//
std::vector<int> DShowVideoCapture::EnumerateCrossbarOutputs() {
	HRESULT hr = E_FAIL;
	std::vector<int> result;

	if(crossbar == 0) 
		return result; // no crossbar

	DisconnectAllFilters();

	long outputPinCount = 0;
	long inputPinCount = 0;
	hr = crossbar->get_PinCounts(&outputPinCount, &inputPinCount);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::EnumerateCrossbarOutputs - Impossibile ottenere il totale dei pin del crossbar");
		}

	for(int i=0;  i<outputPinCount; ++i) {
		long related = 0;
		long type = 0;
		hr = crossbar->get_CrossbarPinInfo(FALSE, i, &related, &type);
		if(SUCCEEDED(hr)) {
			result.push_back(type);
			}
		}

	ConnectAllFilters();

	return result;
	}



//
// route one input pin to one output pin
//
void DShowVideoCapture::RouteCrossbar(int input, int output) {

	if(crossbar == 0) 
		return; // no crossbar

	HRESULT hr = E_FAIL;

	// get the index of the input value and the output value
	int inputIndex = 0;
	std::vector<int> inputs = EnumerateCrossbarInputs();
	while((inputIndex < inputs.size()) && (inputs[inputIndex] != input)) 
		inputIndex++;
	if(inputIndex == inputs.size()) {
		throw Exception("DShowVideoCapture::RouteCrossBar - Impossibile trovare l'input desiderato");
		}

	int outputIndex = 0;
	std::vector<int> outputs = EnumerateCrossbarOutputs();
	while((outputIndex < outputs.size()) && (outputs[outputIndex] != output)) 
		outputIndex++;
	if(outputIndex == outputs.size())	{
		throw Exception("DShowVideoCapture::RouteCrossBar - Impossibile trovare l'output desiderato");
		}

	DisconnectAllFilters();

	hr = crossbar->Route (outputIndex, inputIndex);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::RouteCrossBar - Impossibile collegare l'input desiderato all'output desiderato");
		}

	ConnectAllFilters();
	}



// Helper function to associate a name with the type.
const char* DShowVideoCapture::GetPhysicalConnectorName(int type) {

  switch(type) {
		case PhysConn_Video_Tuner:            return "Video Tuner";
		case PhysConn_Video_Composite:        return "Video Composite";
		case PhysConn_Video_SVideo:           return "S-Video";
		case PhysConn_Video_RGB:              return "Video RGB";
		case PhysConn_Video_YRYBY:            return "Video YRYBY";
		case PhysConn_Video_SerialDigital:    return "Video Serial Digital";
		case PhysConn_Video_ParallelDigital:  return "Video Parallel Digital"; 
		case PhysConn_Video_SCSI:             return "Video SCSI";
		case PhysConn_Video_AUX:              return "Video AUX";
		case PhysConn_Video_1394:             return "Video 1394";
		case PhysConn_Video_USB:              return "Video USB";
		case PhysConn_Video_VideoDecoder:     return "Video Decoder";
		case PhysConn_Video_VideoEncoder:     return "Video Encoder";
        
		case PhysConn_Audio_Tuner:            return "Audio Tuner";
		case PhysConn_Audio_Line:             return "Audio Line";
		case PhysConn_Audio_Mic:              return "Audio Microphone";
		case PhysConn_Audio_AESDigital:       return "Audio AES/EBU Digital";
		case PhysConn_Audio_SPDIFDigital:     return "Audio S/PDIF";
		case PhysConn_Audio_SCSI:             return "Audio SCSI";
		case PhysConn_Audio_AUX:              return "Audio AUX";
		case PhysConn_Audio_1394:             return "Audio 1394";
		case PhysConn_Audio_USB:              return "Audio USB";
		case PhysConn_Audio_AudioDecoder:     return "Audio Decoder";
        
		default:                              return "Unknown Type";
    }    
	}


HRESULT DShowVideoCapture::SetupVideoWindow() {		// per ora non la usiamo.... che fa??
  HRESULT hr;
	
  // Set the video window to be a child of the main window
  hr = window->put_Owner((OAHWND)m_TV->GetParent()->m_hWnd);
  if(FAILED(hr))
    return hr;
    
  // Set video window style
  hr = window->put_WindowStyle(WS_CHILD | WS_CLIPCHILDREN);
  if(FAILED(hr))
    return hr;
	
	hr = window->put_Width(320) ;
	hr = window->put_Height(240) ;
	
   // Resize the video preview window to match owner window size
  RECT rc;
        
  // Make the preview video fill our window
  m_TV->GetParent()->GetClientRect(&rc);
  window->SetWindowPosition(0, 0, rc.right, rc.bottom);
	
  // Make the video window visible, now that it is properly positioned
  hr = window->put_Visible(OATRUE);
  if(FAILED(hr))
    return hr;
	hr = window->put_MessageDrain((OAHWND)m_TV->GetParent()->m_hWnd);
    
	return hr;
	}

int DShowVideoCapture::SetGainParameters(double brt,double cont,double sat) {
	HRESULT hr;

//	GetProcAmpControl();

	return hr;
	}

BOOL DShowVideoCapture::GetDriverDescription(WORD wDriverIndex,  
  LPSTR lpszName, INT cbName,
  LPSTR lpszVer,  INT cbVer) {

  HRESULT hr = E_FAIL;
  CComPtr<ICreateDevEnum> pSysDevEnum;
  CComPtr<IEnumMoniker> pEnumCat;

	if(lpszName)
		*lpszName=0;

    // Create the System Device Enumerator.
  hr = pSysDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::GetDriverDescription - Impossibile creare SystemDeviceEnum");
		}

    // Obtain a class enumerator for the video compressor category.
  hr = pSysDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumCat, 0);
	if(FAILED(hr)) {
		throw Exception("DShowVideoCapture::GetDriverDescription - Impossibile creare un enumeratore per la VideoInputDeviceCategory");
		}
    
  if(!pEnumCat)
		return NULL;

  pEnumCat->Reset();

    // Enumerate the monikers.
	for(int i=0;  i <= wDriverIndex;  ++i) {
	  CComPtr<IMoniker> pMoniker;
		ULONG cFetched=0;	

		// get next moniker
		hr = pEnumCat->Next(1, &pMoniker, &cFetched);
		if(FAILED(hr)) {
//			throw Exception("DShowVideoCapture::GetDriverDescription - Failed to find capture device");
			return 0;
			}

		if(!cFetched)
			return 0;

	// To retrieve the filter's friendly name, do the following:
		IPropertyBag *pPropBag;
		hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);
		if(SUCCEEDED(hr)) {
			VARIANT varName;
			VariantInit(&varName);
			hr = pPropBag->Read(L"FriendlyName", &varName, 0);
			if(SUCCEEDED(hr))	{
        USES_CONVERSION;		// xché è UNICODE...
				if(lpszName) {
					strncpy(lpszName,W2T(varName.bstrVal),cbName-1);
					lpszName[cbName-1]=0;
					}
				}
			VariantClear(&varName);

			if(lpszVer)
				*lpszVer=0;
/*			// finire...
			hr = pPropBag->Read(L"Version", &varName, 0);
			if(SUCCEEDED(hr))	{
				CString S=varName.bstrVal;		// xché è UNICODE...
				strncpy(lpszVer,S,cbVer-1);
				lpszName[cbVer-1]=0;
				}*/
			}

		}

	return 1;
	}


#define INITGUID

//#include <ddraw.h>
//#include <initguid.h>				// facendo così, NON va messa dxguid.lib nel Linker! (d'altronde, al contrario, non trovava alcune GUID...!)
#include <dxdiag.h>
#undef INITGUID
#include <dinput.h>
#include <dmusici.h>


typedef HRESULT(WINAPI * DIRECTDRAWCREATE)( GUID*, LPDIRECTDRAW*, IUnknown* );
typedef HRESULT(WINAPI * DIRECTDRAWCREATEEX)( GUID*, VOID**, REFIID, IUnknown* );
typedef HRESULT(WINAPI * DIRECTINPUTCREATE)( HINSTANCE, DWORD, LPDIRECTINPUT*,
                                             IUnknown* );


//-----------------------------------------------------------------------------
// Name: GetDXVersion() (da dx7, elaborato da GD per dx9)
//	v. esempi in getDXver nella cartella dxmisc/misc dei samples!
// Desc: This function returns two arguments:
//          dwDXVersion:
//            0x00000000 = No DirectX installed
//            0x01000000 = DirectX version 1 installed
//            0x02000000 = DirectX 2 installed
//            0x03000000 = DirectX 3 installed
//            0x05000000 = At least DirectX 5 installed.
//            0x06000000 = At least DirectX 6 installed.
//            0x06010000 = At least DirectX 6.1 installed.
//            0x07000000 = At least DirectX 7 installed.
//            0x08000000 = DirectX 8.0 installed
//            0x08010000 = DirectX 8.1 installed
//            0x08010001 = DirectX 8.1a installed
//            0x08010002 = DirectX 8.1b installed
//            0x08020000 = DirectX 8.2 installed
//            0x09000000 = DirectX 9.0 installed
//          dwDXPlatform:
//            0                          = Unknown (This is a failure case)
//            VER_PLATFORM_WIN32_WINDOWS = Windows 9X platform
//            VER_PLATFORM_WIN32_NT      = Windows NT platform
// 
//          Please note that this code is intended as a general guideline. Your
//          app will probably be able to simply query for functionality (via
//          QueryInterface) for one or two components.
//
//          Please also note:
//            "if (dxVer != 0x5000000) return FALSE;" is BAD. 
//            "if (dxVer < 0x5000000) return FALSE;" is MUCH BETTER.
//          to ensure your app will run on future releases of DirectX.
//-----------------------------------------------------------------------------

void DShowVideoCapture::GetDXVersion(DWORD* pdwDXVersion, DWORD* pdwDXPlatform) {
  HRESULT              hr;
  HINSTANCE            DDHinst = 0;
  HINSTANCE            DIHinst = 0;
  LPDIRECTDRAW         pDDraw  = 0;
  LPDIRECTDRAW2        pDDraw2 = 0;
  DIRECTDRAWCREATE     DirectDrawCreate   = 0;
  DIRECTDRAWCREATEEX   DirectDrawCreateEx = 0;
  DIRECTINPUTCREATE    DirectInputCreate  = 0;
  OSVERSIONINFO        osVer;
  LPDIRECTDRAWSURFACE  pSurf  = 0;
  LPDIRECTDRAWSURFACE3 pSurf3 = 0;
  LPDIRECTDRAWSURFACE4 pSurf4 = 0;

  // First get the windows platform
  osVer.dwOSVersionInfoSize = sizeof(osVer);
  if(!GetVersionEx(&osVer)) {
    if(pdwDXPlatform)
	    (*pdwDXPlatform) = 0;
    (*pdwDXVersion)  = 0;
    return;
		}

  if(osVer.dwPlatformId == VER_PLATFORM_WIN32_NT) {
    if(pdwDXPlatform)
	    (*pdwDXPlatform) = VER_PLATFORM_WIN32_NT;

    // NT is easy... NT 4.0 is DX2, 4.0 SP3 is DX3, 5.0 is DX5
    // and no DX on earlier versions.
    if(osVer.dwMajorVersion < 4) {
      (*pdwDXVersion) = 0; // No DX on NT3.51 or earlier
      return;
      }

    if(osVer.dwMajorVersion == 4) {
       // NT4 up to SP2 is DX2, and SP3 onwards is DX3, so we are at least DX2
      (*pdwDXVersion) = 0x2000000;

      // We're not supposed to be able to tell which SP we're on, so check for dinput
      DIHinst = LoadLibrary( "DINPUT.DLL" );
      if(DIHinst == 0) {
        // No DInput... must be DX2 on NT 4 pre-SP3
        OutputDebugString( "Couldn't LoadLibrary DInput\r\n" );
        return;
				}

      DirectInputCreate = (DIRECTINPUTCREATE)GetProcAddress( DIHinst,
                                                           "DirectInputCreateA" );
      FreeLibrary(DIHinst);

      if(DirectInputCreate == 0) {
        // No DInput... must be pre-SP3 DX2
        OutputDebugString( "Couldn't GetProcAddress DInputCreate\r\n" );
        return;
				}

      // It must be NT4, DX2
      (*pdwDXVersion) = 0x3000000;  // DX3 on NT4 SP3 or higher
      return;
      }
    // Else it's NT5 or higher, and it's DX5a or higher: Drop through to
    // Win9x tests for a test of DDraw (DX6 or higher)
		}
  else {
    // Not NT... must be Win9x
    if(pdwDXPlatform)
			(*pdwDXPlatform) = VER_PLATFORM_WIN32_WINDOWS;
		}

  // Now we know we are in Windows 9x (or maybe 3.1), so anything's possible.
  // First see if DDRAW.DLL even exists.
  DDHinst = LoadLibrary( "DDRAW.DLL" );
  if(DDHinst == 0) {
    (*pdwDXVersion)  = 0;
    if(pdwDXPlatform)
	    (*pdwDXPlatform) = 0;
    FreeLibrary( DDHinst );
    return;
		}

  // See if we can create the DirectDraw object.
  DirectDrawCreate = (DIRECTDRAWCREATE)GetProcAddress( DDHinst, "DirectDrawCreate" );
  if(DirectDrawCreate == 0) {
    (*pdwDXVersion)  = 0;
    if(pdwDXPlatform)
	    (*pdwDXPlatform) = 0;
    FreeLibrary( DDHinst );
    OutputDebugString( "Couldn't LoadLibrary DDraw\r\n" );
    return;
		}

  hr = DirectDrawCreate( NULL, &pDDraw, NULL );
  if(FAILED(hr)) {
    (*pdwDXVersion)  = 0;
    if(pdwDXPlatform)
	    (*pdwDXPlatform) = 0;
    FreeLibrary( DDHinst );
    OutputDebugString( "Couldn't create DDraw\r\n" );
    return;
		}

  // So DirectDraw exists.  We are at least DX1.
  (*pdwDXVersion) = 0x1000000;

  // Let's see if IID_IDirectDraw2 exists.
  hr = pDDraw->QueryInterface( IID_IDirectDraw2, (VOID**)&pDDraw2 );
  if(FAILED(hr)) {
    // No IDirectDraw2 exists... must be DX1
    pDDraw->Release();
    FreeLibrary( DDHinst );
    OutputDebugString( "Couldn't QI DDraw2\r\n" );
    return;
		}

  // IDirectDraw2 exists. We must be at least DX2
  pDDraw2->Release();
  (*pdwDXVersion) = 0x2000000;


  ///////////////////////////////////////////////////////////////////////////
  // DirectX 3.0 Checks
  ///////////////////////////////////////////////////////////////////////////

  // DirectInput was added for DX3
  DIHinst = LoadLibrary( "DINPUT.DLL" );
  if(DIHinst == 0) {
    // No DInput... must not be DX3
    OutputDebugString( "Couldn't LoadLibrary DInput\r\n" );
    pDDraw->Release();
    FreeLibrary( DDHinst );
    return;
		}

  DirectInputCreate = (DIRECTINPUTCREATE)GetProcAddress( DIHinst,
                                                      "DirectInputCreateA" );
  if(DirectInputCreate == 0) {
    // No DInput... must be DX2
    FreeLibrary(DIHinst);
    FreeLibrary(DDHinst);
    pDDraw->Release();
    OutputDebugString( "Couldn't GetProcAddress DInputCreate\r\n" );
    return;
		}

  // DirectInputCreate exists. We are at least DX3
  (*pdwDXVersion) = 0x3000000;
  FreeLibrary( DIHinst );

  // Can do checks for 3a vs 3b here


  ///////////////////////////////////////////////////////////////////////////
  // DirectX 5.0 Checks
  ///////////////////////////////////////////////////////////////////////////

  // We can tell if DX5 is present by checking for the existence of
  // IDirectDrawSurface3. First, we need a surface to QI off of.
  DDSURFACEDESC ddsd;
  ZeroMemory( &ddsd, sizeof(ddsd) );
  ddsd.dwSize         = sizeof(ddsd);
  ddsd.dwFlags        = DDSD_CAPS;
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

  hr = pDDraw->SetCooperativeLevel( NULL, DDSCL_NORMAL );
  if(FAILED(hr)) {
    // Failure. This means DDraw isn't properly installed.
    pDDraw->Release();
    FreeLibrary( DDHinst );
    (*pdwDXVersion) = 0;
    OutputDebugString( "Couldn't Set coop level\r\n" );
    return;
		}

  hr = pDDraw->CreateSurface( &ddsd, &pSurf, NULL );
  if(FAILED(hr)) {
    // Failure. This means DDraw isn't properly installed.
    pDDraw->Release();
    FreeLibrary( DDHinst );
    *pdwDXVersion = 0;
    OutputDebugString( "Couldn't CreateSurface\r\n" );
    return;
		}

  // Query for the IDirectDrawSurface3 interface
  if( FAILED( pSurf->QueryInterface( IID_IDirectDrawSurface3,
                                     (VOID**)&pSurf3 ) ) ) {
    pDDraw->Release();
    FreeLibrary( DDHinst );
    return;
		}

  // QI for IDirectDrawSurface3 succeeded. We must be at least DX5
  (*pdwDXVersion) = 0x5000000;


  ///////////////////////////////////////////////////////////////////////////
  // DirectX 6.0 Checks
  ///////////////////////////////////////////////////////////////////////////

  // The IDirectDrawSurface4 interface was introduced with DX 6.0
  if( FAILED( pSurf->QueryInterface( IID_IDirectDrawSurface4,
                                     (VOID**)&pSurf4 ) ) ) {
    pDDraw->Release();
    FreeLibrary( DDHinst );
    return;
		}

  // IDirectDrawSurface4 was create successfully. We must be at least DX6
  (*pdwDXVersion) = 0x6000000;
  pSurf->Release();
  pDDraw->Release();


  ///////////////////////////////////////////////////////////////////////////
  // DirectX 6.1 Checks
  ///////////////////////////////////////////////////////////////////////////

  // Check for DMusic, which was introduced with DX6.1
  LPDIRECTMUSIC pDMusic = NULL;
  CoInitialize( NULL );
  hr = CoCreateInstance( CLSID_DirectMusic, NULL, CLSCTX_INPROC_SERVER,
                         IID_IDirectMusic, (VOID**)&pDMusic );
  if(FAILED(hr)) {
    OutputDebugString( "Couldn't create CLSID_DirectMusic\r\n" );
    FreeLibrary( DDHinst );
    return;
		}

  // DirectMusic was created successfully. We must be at least DX6.1
  (*pdwDXVersion) = 0x6010000;
  pDMusic->Release();
  CoUninitialize();
  

  ///////////////////////////////////////////////////////////////////////////
  // DirectX 7.0 Checks
  ///////////////////////////////////////////////////////////////////////////

  // Check for DirectX 7 by creating a DDraw7 object
  LPDIRECTDRAW7 pDD7;
  DirectDrawCreateEx = (DIRECTDRAWCREATEEX)GetProcAddress( DDHinst,
                                                     "DirectDrawCreateEx" );
  if( NULL == DirectDrawCreateEx ) {
    FreeLibrary( DDHinst );
    return;
		}

  if( FAILED( DirectDrawCreateEx( NULL, (VOID**)&pDD7, IID_IDirectDraw7,
                                  NULL ) ) ) {
    FreeLibrary( DDHinst );
    return;
		}

  // DDraw7 was created successfully. We must be at least DX7.0
  (*pdwDXVersion) = 0x7000000;
  pDD7->Release();


  BOOL bCleanupCOM = false;

	DWORD pdwDirectXVersionMajor=0,pdwDirectXVersionMinor=0,pcDirectXVersionLetter=0;
    
  // Init COM.  COM may fail if its already been inited with a different 
  // concurrency model.  And if it fails you shouldn't release it.
  hr = CoInitialize(NULL);
  bCleanupCOM = SUCCEEDED(hr);

  // Get an IDxDiagProvider
  IDxDiagProvider* pDxDiagProvider = NULL;
  hr = CoCreateInstance( CLSID_DxDiagProvider,
                         NULL,
                         CLSCTX_INPROC_SERVER,
                         IID_IDxDiagProvider,
                         (LPVOID*) &pDxDiagProvider );
  if(SUCCEEDED(hr)) {
    // Fill out a DXDIAG_INIT_PARAMS struct
    DXDIAG_INIT_PARAMS dxDiagInitParam;
    ZeroMemory( &dxDiagInitParam, sizeof(DXDIAG_INIT_PARAMS) );
    dxDiagInitParam.dwSize                  = sizeof(DXDIAG_INIT_PARAMS);
    dxDiagInitParam.dwDxDiagHeaderVersion   = DXDIAG_DX9_SDK_VERSION;
    dxDiagInitParam.bAllowWHQLChecks        = false;
    dxDiagInitParam.pReserved               = NULL;

    // Init the m_pDxDiagProvider
    hr = pDxDiagProvider->Initialize( &dxDiagInitParam ); 
    if(SUCCEEDED(hr)) {
      IDxDiagContainer* pDxDiagRoot = NULL;
      IDxDiagContainer* pDxDiagSystemInfo = NULL;

      // Get the DxDiag root container
      hr = pDxDiagProvider->GetRootContainer( &pDxDiagRoot );
      if( SUCCEEDED(hr) ) {
        // Get the object called DxDiag_SystemInfo
        hr = pDxDiagRoot->GetChildContainer( L"DxDiag_SystemInfo", &pDxDiagSystemInfo );
        if( SUCCEEDED(hr) ) {
          VARIANT var;
          VariantInit(&var);

          // Get the "dwDirectXVersionMajor" property
          hr = pDxDiagSystemInfo->GetProp( L"dwDirectXVersionMajor", &var );
          if( SUCCEEDED(hr) && var.vt == VT_UI4 ) {
            pdwDirectXVersionMajor = var.ulVal; 
						}
          VariantClear(&var);

          // Get the "dwDirectXVersionMinor" property
          hr = pDxDiagSystemInfo->GetProp( L"dwDirectXVersionMinor", &var );
          if( SUCCEEDED(hr) && var.vt == VT_UI4) {
            pdwDirectXVersionMinor = var.ulVal; 
						}
          VariantClear(&var);

          // Get the "szDirectXVersionLetter" property
          hr = pDxDiagSystemInfo->GetProp( L"szDirectXVersionLetter", &var );
          if(SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != NULL) {
#ifdef UNICODE
            pcDirectXVersionLetter = var.bstrVal[0]; 
#else
						char strDestination[10];
						WideCharToMultiByte( CP_ACP, 0, var.bstrVal, -1, strDestination, 10*sizeof(CHAR), NULL, NULL );
						pcDirectXVersionLetter = strDestination[0]; 
#endif
            }
          VariantClear(&var);

					(*pdwDXVersion) = MAKELONG(MAKEWORD(pcDirectXVersionLetter,pdwDirectXVersionMinor),pdwDirectXVersionMajor);

          pDxDiagSystemInfo->Release();
					}

        pDxDiagRoot->Release();
        }
      }

    pDxDiagProvider->Release();
		}

  if(bCleanupCOM)
    CoUninitialize();


  ///////////////////////////////////////////////////////////////////////////
  // End of checks
  ///////////////////////////////////////////////////////////////////////////

  // Close open libraries and return
  FreeLibrary(DDHinst);
    
	}






CTV::CTV(CWnd *pWnd) {

	preConstruct(pWnd);			// non so come richiamare un constructor da un altro...
	}

CTV::CTV(CWnd *pWnd, BOOL canOverlay, const BITMAPINFOHEADER *biRaw, DWORD fps, DWORD preferredDriver, DWORD kFrame, BOOL bAudio, const WAVEFORMATEX *preferredWf, BOOL bVerbose) {

	preConstruct(pWnd);
	initCapture(canOverlay, biRaw, fps, preferredDriver, kFrame, bAudio, preferredWf,bVerbose);
	}

CTV::CTV(CWnd *pWnd, BOOL canOverlay, const SIZE *bSize, WORD bpp, DWORD fps, DWORD preferredFormat, DWORD preferredDriver, 
				 DWORD kFrame, BOOL bAudio, const WAVEFORMATEX *preferredWf, BOOL bVerbose) {
	BITMAPINFOHEADER bmi;

	preConstruct(pWnd);
	bmi.biWidth=bSize->cx;
	bmi.biHeight=bSize->cy;
	bmi.biBitCount=bpp;
	bmi.biCompression=preferredFormat;

	initCapture(canOverlay,&bmi, fps, preferredDriver, kFrame, bAudio, preferredWf, bVerbose);
#ifdef _CAMPARTY_MODE				// per ora solo qua, ma potrebbe avere senso anche agli altri
	bSize.bottom=bmi.biHeight;
	bSize.right=bmi.biWidth;
#endif
	}

const COMPRESSION_TYPES CTV::acceptedCompressionType[]={
	{	0, 24 },

//	biRawBitmap.biCompression=mmioFOURCC('I','Y','U','V');			// YUV 420 Philips

//		biRawBitmap.biBitCount=12;		// YUV 420	Philips
//						biRawBitmap.biBitCount=16;		//la VB RT300
	{	MAKEFOURCC('Y','U','Y','2'), 16 },
	{ MAKEFOURCC('U','Y','V','Y'), 16 },
	{	MAKEFOURCC('M','J','P','G'), 24	},
	{	MAKEFOURCC('I','U','Y','V'), 12 },			// riaggiunto 2020... verificare se non dà problemi
	{	MAKEFOURCC('I','4','2','0'), 12 /*boh?*/}			// 2023... idem
	};

void CTV::preConstruct(CWnd *pWnd) {

	m_Parent=pWnd;
	theFrame=NULL;
	theCapture=theApp.prStore->GetProfileVariabileInt(IDS_WHICH_CAMERA);
	aviFile=NULL;
	psVideo = NULL, psAudio=NULL, psText = NULL;
	m_hICCo=m_hICDe=NULL;
	m_hAcm=NULL;
	m_TV=this;
	maxFrameSize=0;
	framesPerSec=0;
	KFrame=0;
	oldTimeCaptured=0;
	compressor=0;
	maxWaveoutSize=0;
	inCapture=0;
	allowOverlay=0;
	saveVideo=0;
	imposeTime=0;
	imposeTextPos=0;
	*imposeText=0;
	opzioniSave=0;
	m_hWnd=0;
	vFrameNum=aFrameNum=vFrameNum4Save=aFrameNum4Save=saveWait4KeyFrame=0;
	wInput=2 /*PhysConn_Video_Composite*/;
	m_DShow=NULL;

	m_hWaveIn=(HWAVEIN)-1;
	ZeroMemory(&IWaveHdr1,sizeof(WAVEHDR));
	ZeroMemory(&IWaveHdr2,sizeof(WAVEHDR));
	m_AudioBuffer1=m_AudioBuffer2=NULL;
	}

int CTV::initCapture(BOOL canOverlay, const BITMAPINFOHEADER *biRaw, DWORD fps, DWORD preferredDriver, DWORD kFrame, BOOL bAudio, 
										 const WAVEFORMATEX *preferredWf, BOOL bVerbose) {
	char myBuf[128];
	int i,j,bWarning=0;
	int theInput;

//	compressor=preferredDriver ? preferredDriver : mmioFOURCC('I','V','5','0');
	compressor=preferredDriver;		//2018, consentiamo tutto!
	framesPerSec=fps ? fps : 5;
	KFrame=kFrame;

	aFrameNum=vFrameNum=0;
	vFrameNum4Save=0;
	aFrameNum4Save=0;
	saveWait4KeyFrame=0;

	allowOverlay=canOverlay;
	saveVideo=0;
	imposeTime=1;
	imposeTextPos=1; _tcscpy(imposeText,"dario");

	inCapture=-1;
	biRawBitmap.biSize=sizeof(BITMAPINFOHEADER);
	biRawBitmap.biPlanes=1;
	biRawBitmap.biCompression=0;
//	biRawBitmap.biCompression=mmioFOURCC('I','Y','U','V');			// YUV 420 Philips

	biRawBitmap.biSizeImage=0;
	biRawBitmap.biXPelsPerMeter=biRawBitmap.biYPelsPerMeter=0;
	biRawBitmap.biClrUsed=biRawBitmap.biClrImportant=0;
	if(biRaw) {
		biRawBitmap.biWidth=biRaw->biWidth;
		biRawBitmap.biHeight=biRaw->biHeight;
		biRawBitmap.biBitCount=biRaw->biBitCount;
		biRawBitmap.biCompression=biRaw->biCompression;
		}
	else {
		biRawBitmap.biWidth=320;
		biRawBitmap.biHeight=240;
		biRawBitmap.biBitCount=24;
		biRawBitmap.biCompression=0;
		}

	maxFrameSize=(biRawBitmap.biWidth*biRawBitmap.biHeight*biRawBitmap.biBitCount)/8;
	biRawBitmap.biSizeImage=maxFrameSize;

	switch(wInput) {
		case 1:
			theInput=1 /*PhysConn_Video_Tuner*/;
			break;
		case 2:
			theInput=2 /*PhysConn_Video_Composite*/;
			break;
		case 3:
			theInput=3 /*PhysConn_Video_SVideo*/;
			break;
		default:
			theInput=0 /*default sotto su PhysConn_Video_Composite*/;
			break;
		}	


//		biRawBitmap.biBitCount=12;		// YUV 420	Philips
//						biRawBitmap.biBitCount=16;		//la VB RT300


	try	{
		m_DShow=new DShowVideoCapture(theCapture);

		// attempt to route the crossbar to use composite video
		// and set the format to 320x240x24bpp RGB
		try	{
			theInput=/*PhysConn_Video_USB serve per webcam? pare di no!; //*/ 
				theInput ? theInput : /*PhysConn_Video_Tuner*/ PhysConn_Video_Composite;

			m_DShow->RouteCrossbar(theInput, PhysConn_Video_VideoDecoder);
			}
		catch (IVideoCapture::Exception& e)
		{	// if routing the crossbar or setting the format fail, we don't care
			// we'll try to run the program anyways
			std::string error = e.message + "\nTento di eseguire ugualmente il programma";
			MessageBox (0, error.c_str(), "InitVideo: Warning", MB_OK | MB_ICONWARNING);
		}


		if(biRawBitmap.biCompression) {		// se != 0 allora provo preimpostato...
			if(biRawBitmap.biCompression==mmioFOURCC('R','G','B',0))		// RGB  è 0
				biRawBitmap.biCompression=0;
			try	{
				m_DShow->SetFormat(biRawBitmap.biWidth, biRawBitmap.biHeight, biRawBitmap.biBitCount, 
					framesPerSec,0,biRawBitmap.biCompression);
				i=1;
				goto format_ok;
				}
			catch (IVideoCapture::Exception& e) {	// (idem)
				CString SFmt;
				SFmt.Format("%c%c%c%c",LOBYTE(LOWORD(biRawBitmap.biCompression)),
					// RGB esce vuoto...
					HIBYTE(LOWORD(biRawBitmap.biCompression)),
					LOBYTE(HIWORD(biRawBitmap.biCompression)),
					HIBYTE(HIWORD(biRawBitmap.biCompression)));
				i=0;
				std::string error = e.message + " (";
				error += SFmt;
				error += ")\nFormato indicato non accettato: ne cerco un altro";
				MessageBox(0, error.c_str(), "VideoSetFormat: Warning", MB_OK | MB_ICONWARNING);
				}
			}

		for(j=0; j<sizeof(acceptedCompressionType)/sizeof(COMPRESSION_TYPES); j++) {
			try	{
				biRawBitmap.biCompression=acceptedCompressionType[j].fourCC;
				biRawBitmap.biBitCount=acceptedCompressionType[j].bitsPerPixel;
				m_DShow->SetFormat(biRawBitmap.biWidth, biRawBitmap.biHeight, biRawBitmap.biBitCount, 
					framesPerSec,0,biRawBitmap.biCompression);
				i=1;
				break;
				}
			catch (IVideoCapture::Exception& e) {	// (idem)
				CString SFmt;
				SFmt.Format("%c%c%c%c",LOBYTE(LOWORD(acceptedCompressionType[j].fourCC)),
					// RGB esce vuoto...
					HIBYTE(LOWORD(acceptedCompressionType[j].fourCC)),
					LOBYTE(HIWORD(acceptedCompressionType[j].fourCC)),
					HIBYTE(HIWORD(acceptedCompressionType[j].fourCC)));
				i=0;
				std::string error = e.message + " (";
				error += SFmt;
				error += ")\nTento di eseguire ugualmente il programma";
//			if(theApp.debugMode)
				// su 0 ossia RGB si potrebbe anche evitare message..
				MessageBox(0, error.c_str(), "VideoSetFormat: Warning", MB_OK | MB_ICONWARNING);
				}
			}

format_ok:
		int width = m_DShow->GetWidth();
		int height = m_DShow->GetHeight();


		WNDCLASSEX wc;
		ZeroMemory(&wc, sizeof(WNDCLASSEX));
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.hInstance = AfxGetInstanceHandle() /*theApp.m_hInstance*/;
		wc.lpfnWndProc = VideoCallback;
		wc.lpszClassName = DIRECTSHOW_CAPTURE_CLASS;
		RegisterClassEx(&wc);

		RECT r = { 0,0,width,height };
		AdjustWindowRectEx(&r, WS_CAPTION, FALSE, 0);

		// si potrebbe usare IVideoWindow , la finestra del video-live... v. AmCap!

		m_hWnd = CreateWindowEx(0, wc.lpszClassName, "DShow Capture", 
									WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 
									0, 0, 
									r.right-r.left, r.bottom-r.top,
									m_Parent->m_hWnd, 0, AfxGetInstanceHandle() /*theApp.m_hInstance*/, 0);


		ShowWindow(m_hWnd,SW_SHOW);
		UpdateWindow(m_hWnd);
//		i=SetTimer(hWnd,1,100,NULL);

		m_DShow->SetWindow(m_hWnd);


	}
	catch (IVideoCapture::Exception& e)
	{
//		if(theApp.debugMode)
		MessageBox(0, e.message.c_str(), "Error", MB_OK | MB_ICONWARNING);
		// not really proper cleanup, but who cares
	}

	

	if(m_DShow->GetDriverDescription(theCapture,myBuf,64,NULL,0)) {  // determino se ho scheda video
		theApp.Opzioni |= CVidsendApp::canSendVideo;
		if(preferredDriver) {
			}

		if(m_hWnd) {
			m_DShow->StartCapture();

/*			i=m_DShow->driverConnect(); 
			if(i<=0) {				// se non c'e', distruggo (v. distruttore) tutto fin qui
				::DestroyWindow(hWnd);
				goto no_hwnd;
				}*/
			biBaseRawBitmap=biRawBitmap;
			biBaseRawBitmap.biCompression=0;
			biBaseRawBitmap.biBitCount=24;
//			i=setVideoFormat(&biRawBitmap);

//			BITMAPINFOHEADER bi;
//			i=getVideoFormat(&biRawBitmap);	// certe telecamere/schede si fanno i cazzi loro (SENZA dare errore), in questo modo vedo che fotogrammi mi dara'...


//							CFile mf;

//							mf.Open("c:\\frame0.bmp",CFile::modeWrite | CFile::modeCreate);
//							mf.Write(&bi,sizeof(bi));
//							mf.Close();


 			}
		}
	else {
no_hwnd:
		m_hWnd=NULL;
		biBaseRawBitmap=biRawBitmap;		// tanto per gradire... serve (v.sotto) a inizializzare cmq un compressore, in caso si trasmetta video playback
		biBaseRawBitmap.biCompression=0;
		biBaseRawBitmap.biBitCount=24;
		}

fine_hwnd:
	biRawDef.bmiHeader=biRawBitmap;
//	biRawDef.bmiColors=NULL;

	biCompDef=biRawDef;
	biCompDef.bmiHeader=biBaseRawBitmap;
	biCompDef.bmiHeader.biCompression=compressor;
	biCompDef.bmiHeader.biBitCount=24;		// buono per IR50
// questa viene mandata come info al client che si connette


		// magari legare a (doc->Opzioni & videoType) ??
	if(!(theApp.theServer->Opzioni & CVidsendDoc2::videoType)) {
		biRawDef.bmiHeader.biCompression=0; biRawDef.bmiHeader.biBitCount=24;
		}
	//e questa è usata internamente, quindi direi sempre RGB pura (se serve, con decompressore di supporto
	else {
		biCompDef.bmiHeader.biCompression=biRawBitmap.biCompression;
		biCompDef.bmiHeader.biBitCount=biRawBitmap.biBitCount;
		}


//	biRawBitmap.biWidth=100;		// per prove!




	if(m_hWnd) {			// solo se esiste un driver video...
rifo_check_video_format:
#ifndef _CAMPARTY_MODE
		if(biBaseRawBitmap.biWidth != biRawBitmap.biWidth ||
			biBaseRawBitmap.biHeight != biRawBitmap.biHeight) {
			bWarning=TRUE;
#else
		if(biBaseRawBitmap.biWidth != biRawBitmap.biWidth || biRawBitmap.biWidth != 320 ||
			biBaseRawBitmap.biHeight != biRawBitmap.biHeight || biRawBitmap.biHeight != 240) {
			bWarning=TRUE;
#endif
#ifdef _CAMPARTY_MODE
//			biBaseRawBitmap.biWidth = biRawBitmap.biWidth;
//			biBaseRawBitmap.biHeight = biRawBitmap.biHeight;
//			*biRaw=biBaseRawBitmap;

//			if(bVerbose) {			qua??

			CConfirmCamDlg myDlg;
			if(myDlg.DoModal() == IDOK) {
				if(!capDlgVideoFormat(hWnd))
					AfxMessageBox("Impossibile aprire la finestra delle proprietà della webcam! (provare a riavviare il computer)",MB_ICONSTOP);
				i=getVideoFormat(&biRawBitmap);
				goto rifo_check_video_format;
				}
			else {			// v. distruttore
				if(theFrame && (int)theFrame != -1) 
					HeapFree(GetProcessHeap(),0,theFrame);	// usato dal JPEG dell'HTML
				if(aviFile)
					endSaveFile();
				if(hWnd) {
					Capture(0); 
					preview(FALSE);        // disables preview 
					overlay(FALSE);        // disables OVL
					setCallbackOnFrame();
					setCallbackOnError();
					setCallbackOnStatus(); 
					driverDisconnect(); 
					::DestroyWindow(hWnd);
					}
				}
#elif _NEWMEET_MODE
			
/*Qu=E0 su nmvidsend deve funzionare cos=EC:=0D
se trovi un formato video diverso un messaggio con una domanda intelligen=
te
che dice:=0D
Attenzione, il formato video della tua webcam (140x120) =E8 diversa dal
formato video di nmvidsend (320x240)!=0D
=0D
vuoi cambiare il formato video della tua webcam ? SI=0D
 (a questo punto fai apparire la finestra del driver cam)=0D
vuoi cambiare il formato video di nmvidsend ? SI=0D
 (a questo punto fai apparire la finestra di avanzate di nmvsend)=0D
=0D
questo mi s=E0 =E8 l'unico modo di evitare errori da parte dell'esibitor =
e da
parte nostra=0D*/


			if(bVerbose) {
				CString S;			
				S.Format("Attenzione, il formato video della tua webcam (%ux%u) è diverso dal formato video di NMVidsend (%ux%u)!",
					biRawBitmap.biWidth,biRawBitmap.biHeight,biBaseRawBitmap.biWidth,biBaseRawBitmap.biHeight);
				AfxMessageBox(S,MB_OK);
				
	//			biBaseRawBitmap.biWidth = biRawBitmap.biWidth;
	//			biBaseRawBitmap.biHeight = biRawBitmap.biHeight;
	//			*biRaw=biBaseRawBitmap;

				if(AfxMessageBox("Vuoi cambiare il formato video della webcam?",MB_YESNO | MB_DEFBUTTON1 | MB_ICONQUESTION) == IDYES) {
					if(!capDlgVideoFormat(hWnd))
						AfxMessageBox("Impossibile aprire la finestra delle proprietà della webcam! (provare a riavviare il computer)",MB_ICONSTOP);
					i=getVideoFormat(&biRawBitmap);
					goto rifo_check_video_format;
					}
				AfxMessageBox("E' necessario cambiare il formato video di NMVidsend!",MB_ICONSTOP);
				CVidsendPropPage mySheet("Proprietà streaming avanzate");
				struct QUALITY_MODEL_V qv;
				ZeroMemory(&qv,sizeof(struct QUALITY_MODEL_V));
				qv.imageSize.right=biRawBitmap.biWidth;
				qv.imageSize.bottom=biRawBitmap.biHeight;
				qv.bpp=24;		// tanto per...
				CVidsendDoc2PropPage0 myPage0(NULL,&qv);
				
				mySheet.AddPage(&myPage0);
				if(mySheet.DoModal() == IDOK) {
					if(myPage0.isInitialized) {
						//uso solo questo campo...
						biBaseRawBitmap.biWidth=myPage0.m_QV.imageSize.right;
						biBaseRawBitmap.biHeight=myPage0.m_QV.imageSize.bottom;
						*biRaw=biBaseRawBitmap;
						}
					goto rifo_check_video_format;
					}
				else
	//				AfxMessageBox("NMVidsend potrebbe non funzionare correttamente con le impostazioni attuali.\nE' consigliabile impostare NMVidsend in maniera corrispondente alle impostazioni della webcam.",MB_ICONEXCLAMATION);
	// no... il @*!?!@ non vuole!
					goto rifo_check_video_format;
				}

#else
			if(bVerbose) {
				if(AfxMessageBox("Il driver video non accetta le impostazioni fornitegli dal programma:\nUtilizzare le impostazioni del driver?",MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES) {
					biBaseRawBitmap.biWidth = biRawBitmap.biWidth;
					biBaseRawBitmap.biHeight = biRawBitmap.biHeight;
					AfxMessageBox("E' consigliabile impostare il programma in maniera corrispondente alle impostazioni del driver.",MB_ICONEXCLAMATION);
					}
				}
#endif
			}
		}

	m_hICCo=ICOpen(ICTYPE_VIDEO,compressor,ICMODE_FASTCOMPRESS);
	if(m_hICCo) {
/*		CString info;
		ICINFO icinfo;
		ICGetInfo(hICCo,&icinfo,sizeof(ICINFO));
		info+=icinfo.dwFlags & VIDCF_CRUNCH ? "supporta compressione a una dimensione data; " : "";
		info+=icinfo.dwFlags & VIDCF_DRAW ? "supporta drawing; " : "";
		info+=icinfo.dwFlags & VIDCF_FASTTEMPORALC ? "supporta compressione temporale e conserva copia dei dati; " : "";
		info+=icinfo.dwFlags & VIDCF_FASTTEMPORALD ? "supporta decompressione temporale e conserva copia dei dati; " : "";
		info+=icinfo.dwFlags & VIDCF_QUALITY ? "supporta impostazione qualità; " : "";
		info+=icinfo.dwFlags & VIDCF_TEMPORAL ? "supporta compressione inter-frame; " : "";
		AfxMessageBox(info);*/

		maxFrameSize=max(maxFrameSize /*2018, o usciva 3:4 del max... PERCHE' MANCAVA INDEO50! cmq lo lascio */,
			ICCompressGetSize(m_hICCo,&biBaseRawBitmap,&biCompDef));
		i=ICCompressBegin(m_hICCo,&biBaseRawBitmap,&biCompDef);


		/* XVID lo mette a 300.. e se ne sbatte del flag in ICCompress! 
		// forse usare ICSeq...
		i=ICGetDefaultKeyFrameRate(m_hICCo);
		int j=ICGetDefaultQuality(m_hICCo);
		wsprintf(myBuf,"default key frame rate=%d, quality=%u",i,j);
			if(theApp.debugMode)
		AfxMessageBox(myBuf);*/
		}


	if(biRawBitmap.biCompression != 0) {
		m_hICDe=ICOpen(ICTYPE_VIDEO,biRawBitmap.biCompression  /*'i','4','2','0'*/ /*'y','v','y','u'*/,ICMODE_DECOMPRESS);
//		hICDe=ICOpen(ICTYPE_VIDEO,mmioFOURCC('i','4','2','0' /*'y','v','y','u'*/),ICMODE_DECOMPRESS);
		if(m_hICDe)
			i=ICDecompressBegin(m_hICDe,&biRawBitmap,&biBaseRawBitmap);
		if(!m_hICDe || i != 0)
			AfxMessageBox("Impossibile inizializzare il decompressore di supporto!");
		}




	if(bAudio) {		// qui serve esplicitamente l'apertura del WAVE...

		if(preferredWf) {
			memcpy(&wfex,preferredWf,sizeof(WAVEFORMATEX));
			memcpy(&wfd,preferredWf,sizeof(WAVEFORMATEX));
			}
		else {
			wfex.wFormatTag = WAVE_FORMAT_PCM;
			wfex.nChannels = 1;
			wfex.nSamplesPerSec = 8000 /*FREQUENCY*/;
			wfex.nBlockAlign = 1;
			wfex.wBitsPerSample = 8;
			wfex.nAvgBytesPerSec = wfex.nSamplesPerSec*wfex.nChannels*(wfex.wBitsPerSample/8);
			wfex.cbSize = 0;

			// FINIRE con compressore scelto, 2021

			GSM610WAVEFORMAT mywfx;
			mywfx.wfx.wFormatTag = WAVE_FORMAT_GSM610;
			mywfx.wfx.nChannels = 1;
			mywfx.wfx.nSamplesPerSec = 8000 /*8000*/;
			mywfx.wfx.nAvgBytesPerSec = 1625;
			mywfx.wfx.nBlockAlign = 65;
			mywfx.wfx.wBitsPerSample = 0;
			mywfx.wfx.cbSize = 2;
			mywfx.wSamplesPerBlock = 320;
			memcpy(&wfd,&mywfx,sizeof(GSM610WAVEFORMAT));
			}



		m_AudioBuffer1=new BYTE[wfex.nAvgBytesPerSec];
		m_AudioBuffer2=new BYTE[wfex.nAvgBytesPerSec];

		i=waveInOpen(&m_hWaveIn,WAVE_MAPPER,&wfex,(DWORD)m_hWnd,(DWORD)this,CALLBACK_WINDOW);

		if(i == MMSYSERR_NOERROR) {
			theApp.theServer->Opzioni |= CVidsendDoc2::sendAudio;
			}
		else {
#ifdef _DEBUG
			AfxMessageBox("impossibile aprire audio");
#endif
			}


		acmStreamOpen(&m_hAcm,NULL,&wfex,(WAVEFORMATEX *)&wfd,NULL,NULL,0 /*this*/,0);
		if(m_hAcm)
			acmStreamSize(m_hAcm,wfex.nAvgBytesPerSec,&maxWaveoutSize,ACM_STREAMSIZEF_SOURCE);


		}


#ifdef DARIO
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 1;
	wfex.nSamplesPerSec = 8000 /*FREQUENCY*/;
	wfex.nBlockAlign = 1;
	wfex.wBitsPerSample = 8;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec*wfex.nChannels*(wfex.wBitsPerSample/8);
	wfex.cbSize = 0;

	if(preferredWf) {
		memcpy(&wfd,preferredWf,sizeof(WAVEFORMATEX));
		}
	else {
		GSM610WAVEFORMAT mywfx;
		mywfx.wfx.wFormatTag = WAVE_FORMAT_GSM610;
		mywfx.wfx.nChannels = 1;
		mywfx.wfx.nSamplesPerSec = 8000 /*8000*/;
		mywfx.wfx.nAvgBytesPerSec = 1625;
		mywfx.wfx.nBlockAlign = 65;
		mywfx.wfx.wBitsPerSample = 0;
		mywfx.wfx.cbSize = 2;
		mywfx.wSamplesPerBlock = 320;
		memcpy(&wfd,&mywfx,sizeof(GSM610WAVEFORMAT));
		}


	if(setAudioFormat(&wfex))
		theApp.Opzioni |= CVidsendApp::canSendAudio;

	acmStreamOpen(&m_hAcm,NULL,&wfex,(WAVEFORMATEX *)&wfd,NULL,NULL,0 /*this*/,0);
	if(m_hAcm)
		acmStreamSize(m_hAcm,wfex.nAvgBytesPerSec,&maxWaveoutSize,ACM_STREAMSIZEF_SOURCE);
#endif

fine:
	return m_hWnd ? (bWarning ? 2 : 1) : 0;
	}

CTV::~CTV() {

	if(m_hWaveIn != (HWAVEIN)-1) {
		waveInReset(m_hWaveIn);
		waveInStop(m_hWaveIn);
		waveInClose(m_hWaveIn);
		}
	m_hWaveIn=NULL;

	if(theFrame && (int)theFrame != -1) 
		HeapFree(GetProcessHeap(),0,theFrame);	// usato dal JPEG dell'HTML
	if(m_DShow)
		m_DShow->SetWindow(NULL);
	if(aviFile)
		endSaveFile();
	if(m_hWnd) {
		Capture(0); 
//		KillTimer(hWnd,1);
		::DestroyWindow(m_hWnd);
		}

	if(m_hAcm)
		acmStreamClose(m_hAcm,0);
	m_hAcm=NULL;
	delete []m_AudioBuffer1;		m_AudioBuffer1=NULL;
	delete []m_AudioBuffer2;		m_AudioBuffer2=NULL;

	if(m_hICCo) {
		ICCompressEnd(m_hICCo);
		ICClose(m_hICCo);
		}
	m_hICCo=NULL;
	if(m_hICDe) {
		ICDecompressEnd(m_hICDe);
		ICClose(m_hICDe);
		}
	m_hICDe=NULL;
  theApp.prStore->WriteProfileVariabileInt(IDS_WHICH_CAMERA,theCapture);

	UnregisterClass(DIRECTSHOW_CAPTURE_CLASS,AfxGetInstanceHandle());

	if(m_DShow) {
		m_DShow->SetWindow(NULL);
		delete m_DShow;
		}
	m_DShow=NULL;
	}

RECT *CTV::Resize(RECT *r, BOOL force) {
	int i,oldC=-1;

	if(m_hWnd) {
		if(inCapture==1) {		// a volte arriva Resize MENTRE e' in Capture (da createView o altro...) obbligatorio sincronizzare le cose!
			oldC=inCapture;
			Capture(0);
			}
		if(force) {
			biRawBitmap.biWidth=min(r->right-r->left,640);
			biRawBitmap.biHeight=(biRawBitmap.biWidth*biRawBitmap.biBitCount/8)/4;
			}
		r->right=biRawBitmap.biWidth;
		r->bottom=biRawBitmap.biHeight;
		::SetWindowPos(m_hWnd,HWND_BOTTOM,r->left,r->top,biRawBitmap.biWidth,biRawBitmap.biHeight,SWP_NOACTIVATE);
		m_DShow->SetFormat(biRawBitmap.biWidth, biRawBitmap.biHeight, biRawBitmap.biBitCount, framesPerSec,
			0,biRawBitmap.biCompression);
//		i=setVideoFormat(&biRawBitmap);
		if(oldC==1) {
			Capture(1); 
			}
		}
	return r;
	}

int CTV::Capture(int m) {
	DWORD l;
	MSG msg;
//	char myBuf[64];


	if(theApp.debugMode>2) {
		char myBuf[64];
		wsprintf(myBuf,"capture %d, hWnd %x",m,m_hWnd);
		AfxMessageBox(myBuf);
		}
	if(m_hWnd) {
		oldTimeCaptured=0;
		vFrameNum=0;
		aFrameNum=0;
		if(m && inCapture!=1) {
			inCapture=TRUE;
			m_DShow->StartCapture();
			return 1;
			}
		if(!m && inCapture==1) {
			inCapture=FALSE;
			m_DShow->StopCapture();
			return 1;
			}
		}
	else
		return 0;
	}


BOOL CTV::GetDriverDescription(WORD wDriverIndex,  
  LPSTR lpszName,int cbName,
  LPSTR lpszVer, int cbVer) {

	return m_DShow->GetDriverDescription(wDriverIndex, lpszName, cbName, lpszVer, cbVer);
	}

int CTV::startSaveFile(CString nomefile,DWORD opzioni) {
	AVISTREAMINFO strhdr;
	HRESULT hr; 
	AVICOMPRESSOPTIONS opts; 
	LPAVICOMPRESSOPTIONS aopts[1] = {&opts}; 
	PAVISTREAM myps;
  DWORD dwTextFormat; 
	int i;

	psVideo=NULL, psAudio=NULL, psText = NULL;

	AVIFileInit();  // Open the movie file for writing....
	
	hr = AVIFileOpen(&aviFile,    // returned file pointer
		nomefile,            // file name
		OF_WRITE | OF_CREATE,    // mode to open file with
		NULL);    // use handler determined from file extension....
	if(hr != AVIERR_OK)
		goto error;
	
	ZeroMemory(&strhdr, sizeof(strhdr));
	strhdr.fccType                = streamtypeVIDEO;// stream type
	strhdr.fccHandler             = compressor;
	strhdr.dwFlags               = 0;
	strhdr.dwCaps                = 0;
	strhdr.wPriority             = 0;
	strhdr.wLanguage             = 0;
	strhdr.dwScale                = 1;
	strhdr.dwRate                 = opzioni & CVidsendDoc2::quantiFrame ? 1 : framesPerSec;
  strhdr.dwStart               = 0;
  strhdr.dwLength              = 0;
  strhdr.dwInitialFrames       = 0;
	strhdr.dwSuggestedBufferSize  = maxFrameSize;
	strhdr.dwQuality             = 0xffffffff; //-1;         // Use default
  strhdr.dwSampleSize          = 0;
	strhdr.dwEditCount           = 0;
	strhdr.dwFormatChangeCount   = 0;
//	strhdr.szName0               = 0;
//	strhdr.szName1               = 0;
	_tcscpy(strhdr.szName,"video");
	SetRect(&strhdr.rcFrame, 0, 0,    // rectangle for stream
				(int) biCompDef.bmiHeader.biWidth,
				(int) biCompDef.bmiHeader.biHeight);  // And create the stream;
	hr = AVIFileCreateStream(aviFile,    // file pointer
												 &myps,    // returned stream pointer
												 &strhdr);    // stream header
	if(hr != AVIERR_OK)
		goto error;
	hr = AVIStreamSetFormat(myps, 0, &biCompDef.bmiHeader, sizeof(BITMAPINFOHEADER)); 
	if(hr != AVIERR_OK) 
		goto error;

//	ZeroMemory(&opts, sizeof(opts));
	opts.fccType           = 0; //fccType_;
	opts.fccHandler        = 0;//fccHandler_;
	opts.dwKeyFrameEvery   = 0;
	opts.dwQuality         = 0;  // 0 .. 10000
	opts.dwFlags           = 0;  // AVICOMRPESSF_KEYFRAMES = 4
	opts.dwBytesPerSecond  = 0;
	opts.lpFormat          = 0; //new IntPtr(0);
	opts.cbFormat          = 0;
	opts.lpParms           = 0; //new IntPtr(0);
	opts.cbParms           = 0;
	opts.dwInterleaveEvery = 0;
//	if(!AVISaveOptions(NULL, 0, 1, &myps, (LPAVICOMPRESSOPTIONS FAR *) &aopts))
//		goto error;

//if(superimposeDateTime) solo in questo caso?

	ZeroMemory(&strhdr, sizeof(strhdr)); 
	// http://forums.codeguru.com/showthread.php?428984-Add-text-to-avi-file
	// https://git.rwth-aachen.de/till.hofmann/openrave/blob/511476418615757d70d76349663c0cb48ab630c6/plugins/qtcoinrave/aviUtil.cpp
	strhdr.fccType                = streamtypeTEXT; 
	strhdr.fccHandler             = mmioFOURCC('D', 'R', 'A', 'W'); 
	strhdr.dwScale                = 1; 
	strhdr.dwRate                 = 1;
	strhdr.dwSuggestedBufferSize  = 25;
	strhdr.dwQuality             = 0xffffffff; //-1;         // Use default
	SetRect(&strhdr.rcFrame, 0, (int) biCompDef.bmiHeader.biHeight,
				(int) biCompDef.bmiHeader.biWidth,     
				(int) biCompDef.bmiHeader.biHeight+(biCompDef.bmiHeader.biHeight/8));  // And create the stream; 
	_tcscpy(strhdr.szName,"testo");
	hr = AVIFileCreateStream(aviFile, &psText, &strhdr); 
	if(hr != AVIERR_OK) 
		goto error; 
  // se no esce stream ciucco, 2018 videsendocx... dwTextFormat = 0 /*mmioFOURCC('S','U','B','T')*/ /*sizeof(dwTextFormat)*/;
  dwTextFormat = sizeof(dwTextFormat); 
	hr = AVIStreamSetFormat(psText, 0, &dwTextFormat, sizeof(dwTextFormat)); 
	if(hr != AVIERR_OK) 
		goto error;

	// http://lvcdn.net/gop/ stream analyzer

	if(opzioni & CVidsendDoc2::videoType)		// non posso farlo in formato compresso nativo...
		goto skippa_prologo;
	if(1 /* solo quando accodo...*/)	{
		long n;
		DWORD t,l;
		CBitmap b;
		BITMAP bmp;
		BYTE *p,*p2;

		b.CreateBitmap(biCompDef.bmiHeader.biWidth,biCompDef.bmiHeader.biHeight,1,biCompDef.bmiHeader.biBitCount,NULL);
		b.GetBitmap(&bmp);
		n=biCompDef.bmiHeader.biWidth*biCompDef.bmiHeader.biHeight*biCompDef.bmiHeader.biBitCount/8;
		p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,n);
		bmp.bmBits=p;
		for(i=0; i<n; i+=3) {
			*(WORD *)p=0xc0;
			*(BYTE *)(p+2)=0;
			p+=3;
			}
//		b.SetBitmapBits(n,bmp.bmBits);
		// vorrei o un monoscopio con la data e l'ora, o almeno uno sfondo colorato con data e ora
		superImposeDateTime(&biRawDef.bmiHeader,(BYTE *)bmp.bmBits,0xffffffff);
		p2=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,maxFrameSize+100);
		t=l=0;
		i=ICCompress(m_hICCo,ICCOMPRESS_KEYFRAME,
			&biCompDef.bmiHeader,p2,&biRawDef.bmiHeader,bmp.bmBits,
			&l,&t,0,0/*2500*/,theApp.theServer->myQV.quality,
			NULL,NULL);
		HeapFree(GetProcessHeap(),0,bmp.bmBits);
		if(i == ICERR_OK) {
			vFrameNum4Save=AVIStreamLength(myps);
			for(i=0; i<(opzioni & CVidsendDoc2::quantiFrame ? 1 : framesPerSec); i++) {		// sempre 1 sec.
				n=AVIStreamWrite(myps,// stream pointer 
					vFrameNum4Save, // time of this frame 
					1,// number to write 
					p2,
					biCompDef.bmiHeader.biSizeImage,
					t, // flags.... 
					NULL, NULL);
				vFrameNum4Save++;
				}
			}
		HeapFree(GetProcessHeap(),0,p2);
		if(psText && imposeTime) {
			n=AVIStreamLength(psText);
			CString S;
			S=CTime::GetCurrentTime().Format("%d/%m/%Y %H:%M:%S");
			AVIStreamWrite(psText,// stream pointer 
				n+1, // time of this frame 
				1,// number to write 
				(LPSTR)(LPCTSTR)S,
				S.GetLength()+1,
				AVIIF_KEYFRAME, // flags.... 
				NULL, NULL);
			}
		}

skippa_prologo:

	{
	CRiffList crl;
	CString S;
	S="VideoSender";
	crl.Add(MAKEFOURCC('I','S','F','T'),S);
	crl.Add(MAKEFOURCC('I','A','R','T'),S);
	S=nomefile;
	i=S.ReverseFind('\\');
	if(i)
		S=S.Mid(i+1);
	crl.Add(MAKEFOURCC('I','N','A','M'),S);
	crl.Add(MAKEFOURCC('I','C','M','T'),S);
	AVIFileWriteData(aviFile,mmioFOURCC('L', 'I', 'S', 'T'),crl.GetContent(),crl.GetContentLength());
	}

	aFrameNum4Save=0;
	saveWait4KeyFrame=1;
	psVideo=myps;
	return 1;

error:
	return 0;
	}

int CTV::endSaveFile() {
	PAVISTREAM myps;

	if(psVideo) {
		myps=psVideo;
		psVideo=NULL;
		DWORD ti=timeGetTime()+1000;
		while(ti>timeGetTime())
			Sleep(100);		// aspetta che la routine callback video finisca eventualmente di salvare... MIGLIORARE!
		AVIStreamClose(myps);
		}
	if(psAudio) {
		myps=psAudio;
		psAudio=NULL;
		DWORD ti=timeGetTime()+1000;
		while(ti>timeGetTime())
			Sleep(100);		// aspetta che la routine callback finisca eventualmente di salvare... MIGLIORARE!
		AVIStreamClose(psAudio);  
		}
	if(psText) 
		AVIStreamClose(psText);  
	psText=NULL;
	if(aviFile) 
		AVIFileClose(aviFile);  
	aviFile=NULL;
	AVIFileExit(); 
	return 1;
	}

int CTV::superImposeDateTime(const LPBITMAPINFOHEADER bi,BYTE *d,COLORREF dColor) {
	static BYTE display[12] = { 0x3f, 0x6, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x7, 0x7f, 0x6f,0,0 };
	// 1 bit x ogni segmento, LSB=A...b6=G
	int xSize=bi->biWidth/28,ySize=bi->biHeight/16;
	int x,y,xStart,yStart,xStep,yStep;
	int i,n,n2;
	BYTE *p,*p1;
	char myTime[16];
	CString S;
	CTime ct=CTime::GetCurrentTime();


	if(imposeTime) {
		i=imposeTime-1;
		if(!(i & 4)) {
			xSize=(xSize*2)/3;
			ySize=(ySize*2)/3;
			}
	//		xStep=(xSize/3)*3;
	//		yStep=-bi->biWidth*3;
		switch(i & 3) {
			case 0:			// sinistra in alto
				xStart=8;
				yStart=bi->biHeight-bi->biHeight/11;
				break;
			case 1:			// destra in alto
				xStart=(bi->biWidth-xSize*12);
				yStart=bi->biHeight-bi->biHeight/11;
				break;
			case 2:			// destra in basso
				xStart=(bi->biWidth-xSize*12);
				yStart=(ySize*7)/2;
				break;
			case 3:			// sinistra in basso
				xStart=8;
				yStart=(ySize*7)/2;
				break;
			default:
				return -1;
				break;
			}
		}
	else
		return 0;

	switch(bi->biBitCount) {
		case 24:
		
		xStep=(xSize/3)*3;
		yStep=-bi->biWidth*3;

//	p=d+(bi->biWidth*3*(ySize+2))+(bi->biWidth*3)*(bi->biHeight/7);		// la bitmap e' bottom-up!
	p=d+(bi->biWidth*3*yStart);		// la bitmap e' bottom-up!
	S=ct.Format("%H:%M:%S");
	_tcscpy(myTime,(LPCTSTR)S);
	n2=8;

//ASSERT(0);

	if(dColor == 0xffffffff) {	// calcolo colore analizzando un po' di punti della bitmap
															// forse sarebbe meglio analizzare solo il rettangolo dove andrà l'ora... ma anche questo può cmq. servire!
		BYTE *p2;		// la bitmap e' bottom-up!
		dColor = 0 ;
		for(y=0 /*yStart*/; y<(ySize*8); y+=(ySize/4)) {
			for(x=0 /*xStart*/; x<(xSize*8); x+=(xSize/4)) {
				p2=d+(bi->biWidth*3*y)+x*3;		// la bitmap e' bottom-up!
				dColor += ((*(WORD *)p2) | ((*(BYTE *)(p2+2)) << 16));
				// mi sa che questa roba è sballata... privilagia i bit/byte + alti! 2018
				dColor /= 2; 
				}
			}
		dColor = dColor > 0x800000 ? 0x000000 : 0xffffff; 
		}
	else
		dColor = RGB((dColor >> 16) & 0xff,(dColor >> 8) & 0xff,dColor & 0xff);	// praticità per dopo!

rifo24:
	p1=p+xStart*3;
	for(i=0; i<n2; i++) {			// segm. alto A
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 1) {
				p1+=3;
				for(x=2; x<xSize; x++) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p1=~*(WORD *)p1; p1+=2; *p1=~*p1; p1++;
						}
					else {*/
						*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
//						}
					}
				p1+=3;
				}
			else
				p1+=xSize*3;
			}
		else
			p1+=(xSize/3)*3;		//spazio intercarattere
		p1+=xStep;		// largh. carattere
		}

	p+=yStep;
	p1=p+xStart*3;
	for(y=0; y<ySize/2; y++) {			// segm. B & F
		BYTE *p2=p1;
		for(i=0; i<n2; i++) {
			if(isdigit(n=myTime[i])) {
				if(display[n-'0'] & 0x20) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p2=~*(WORD *)p2; *(p2+2)=~*(p2+2);
						}
					else {*/
						*(WORD *)p2=dColor; *(p2+2)=HIWORD(dColor);
//						}
					}
				p2+=xSize*3-3;
				if(display[n-'0'] & 0x2) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p2=~*(WORD *)p2; p2+=2; *p2=~*p2; p2++;
						}
					else {*/
						*(WORD *)p2=dColor; p2+=2; *p2++=HIWORD(dColor);
//						}
					}
				else
					p2+=3;
				p2+=(xSize/3)*3;
				}
			else
				p2+=(xSize/3)*6;
			}
		p1+=yStep;
		}

	p+=yStep*(ySize/2);
	p1=p+xStart*3;

	for(i=0; i<n2; i++) {			// segm. G
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 0x40) {
				p1+=3;
				for(x=2; x<xSize; x++) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p1=~*(WORD *)p1; p1+=2; *p1=~*p1; p1++;
						}
					else {*/
						*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
//						}
					}
				p1+=3;
				}
			else
				p1+=xSize*3;
			}
		else
			p1+=(xSize/3)*3;		//spazio intercarattere
		p1+=xStep;		// largh. carattere
		}

	p+=yStep;
	p1=p+xStart*3;
	for(y=0; y<ySize/2; y++) {			// segm. C & E
		BYTE *p2=p1;
		for(i=0; i<n2; i++) {
			if(isdigit(n=myTime[i])) {
				if(display[n-'0'] & 0x10) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p2=~*(WORD *)p2; *(p2+2)=~*(p2+2);
						}
					else {*/
						*(WORD *)p2=dColor; *(p2+2)=HIWORD(dColor);
//						}
					}
				p2+=xSize*3-3;
				if(display[n-'0'] & 0x4) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p2=~*(WORD *)p2; p2+=2; *p2=~*p2; p2++;
						}
					else {*/
						*(WORD *)p2=dColor; p2+=2; *p2++=HIWORD(dColor);
//						}
					}
				else
					p2+=3;
				p2+=(xSize/3)*3;
				}
			else
				p2+=(xSize/3)*6;
			}
		p1+=yStep;
		}

	p+=yStep*(ySize/2);
	p1=p+xStart*3;

	for(i=0; i<n2; i++) {
		if(isdigit(n=myTime[i])) {		// segm. basso D
			if(display[n-'0'] & 8) {
				p1+=3;
				for(x=2; x<xSize; x++) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p1=~*(WORD *)p1; p1+=2; *p1=~*p1; p1++;
						}
					else {*/
						*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
//						}
					}
				p1+=3;
				}
			else
				p1+=xSize*3;		//spazio intercarattere
			}
		else
			p1+=(xSize/3)*3;
		p1+=xStep;		// largh. carattere
		}

	S=ct.Format("%d/%m/%Y");
	_tcscpy(myTime,(LPCTSTR)S);
	if(n2==8) {
		n2=10;
		p+=yStep*(ySize/2);
		goto rifo24;
		}
		break;

		case 16:
			xStep=(xSize/3)*2;
			yStep=-bi->biWidth*2;

		
		//	p=d+(bi->biWidth*3*(ySize+2))+(bi->biWidth*3)*(bi->biHeight/7);		// la bitmap e' bottom-up!
	p=d+(bi->biWidth*2*yStart);		// la bitmap e' bottom-up!
	S=ct.Format("%H:%M:%S");
	_tcscpy(myTime,(LPCTSTR)S);
	n2=8;


	if(dColor == 0xffffffff) {	// calcolo colore analizzando un po' di punti della bitmap
															// forse sarebbe meglio analizzare solo il rettangolo dove andrà l'ora... ma anche questo può cmq. servire!
		BYTE *p2;		// la bitmap e' bottom-up!
		dColor = 0 ;
		for(y=1 /*yStart*/; y<ySize; y+=(ySize/4)) {
			for(x=1 /*xStart*/; x<xSize; x+=(xSize/4)) {
				p2=d+(bi->biWidth*2*y)+x*2;		// la bitmap e' bottom-up!
				dColor += *((WORD *)p) & 0xffff;
				// mi sa che questa roba è sballata... privilagia i bit/byte + alti! 2018
				dColor /= 2; 
				}
			}
		dColor = dColor > 0x8000 ? 0x0000 : 0xffff; 
		}

rifo16:
	p1=p+xStart*2;
	for(i=0; i<n2; i++) {			// segm. alto A
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 1) {
				p1+=2;
				for(x=2; x<xSize; x++) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p1=~*(WORD *)p1; p1+=2; *p1=~*p1; p1++;
						}
					else {*/
						*(WORD *)p1=dColor; p1+=2;
//						}
					}
				p1+=2;
				}
			else
				p1+=xSize*2;
			}
		else
			p1+=(xSize/3)*2;		//spazio intercarattere
		p1+=xStep;		// largh. carattere
		}

	p+=yStep;
	p1=p+xStart*2;
	for(y=0; y<ySize/2; y++) {			// segm. B & F
		BYTE *p2=p1;
		for(i=0; i<n2; i++) {
			if(isdigit(n=myTime[i])) {
				if(display[n-'0'] & 0x20) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p2=~*(WORD *)p2; *(p2+2)=~*(p2+2);
						}
					else {*/
						*(WORD *)p2=dColor; 
//						}
					}
				p2+=xSize*2-2;
				if(display[n-'0'] & 0x2) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p2=~*(WORD *)p2; p2+=2; *p2=~*p2; p2++;
						}
					else {*/
						*(WORD *)p2=dColor; p2+=2;
//						}
					}
				else
					p2+=2;
				p2+=(xSize/2)*2;
				}
			else
				p2+=(xSize/2)*4;
			}
		p1+=yStep;
		}

	p+=yStep*(ySize/2);
	p1=p+xStart*2;

	for(i=0; i<n2; i++) {			// segm. G
		if(isdigit(n=myTime[i])) {
			if(display[n-'0'] & 0x40) {
				p1+=2;
				for(x=2; x<xSize; x++) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p1=~*(WORD *)p1; p1+=2; *p1=~*p1; p1++;
						}
					else {*/
						*(WORD *)p1=dColor; p1+=2; 
//						}
					}
				p1+=2;
				}
			else
				p1+=xSize*2;
			}
		else
			p1+=(xSize/3)*2;		//spazio intercarattere
		p1+=xStep;		// largh. carattere
		}

	p+=yStep;
	p1=p+xStart*2;
	for(y=0; y<ySize/2; y++) {		// segm. C & E
		BYTE *p2=p1;
		for(i=0; i<n2; i++) {
			if(isdigit(n=myTime[i])) {
				if(display[n-'0'] & 0x10) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p2=~*(WORD *)p2; *(p2+2)=~*(p2+2);
						}
					else {*/
						*(WORD *)p2=dColor; 
//						}
					}
				p2+=xSize*2-2;
				if(display[n-'0'] & 0x4) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p2=~*(WORD *)p2; p2+=2; *p2=~*p2; p2++;
						}
					else {*/
						*(WORD *)p2=dColor; p2+=2; 
//						}
					}
				else
					p2+=2;
				p2+=(xSize/2)*2;
				}
			else
				p2+=(xSize/2)*4;
			}
		p1+=yStep;
		}

	p+=yStep*(ySize/2);
	p1=p+xStart*2;

	for(i=0; i<n2; i++) {
		if(isdigit(n=myTime[i])) {			// segm. basso D
			if(display[n-'0'] & 8) {
				p1+=2;
				for(x=2; x<xSize; x++) {
/*					if(dColor == 0xffffffff) {
						*(WORD *)p1=~*(WORD *)p1; p1+=2; *p1=~*p1; p1++;
						}
					else {*/
						*(WORD *)p1=dColor; p1+=2; 
//						}
					}
				p1+=2;
				}
			else
				p1+=xSize*2;
			}
		else
			p1+=(xSize/3)*2;		//spazio intercarattere
		p1+=xStep;		// largh. carattere
		}

	S=ct.Format("%d/%m/%Y");
	_tcscpy(myTime,(LPCTSTR)S);
	if(n2==8) {
		n2=10;
		p+=yStep*(ySize/2);
		goto rifo16;
		}
			break;
		}

	return 1;
	}

int CTV::superImposeText(const CBitmap *b,const char *s,COLORREF dColor,int flag) {
	int xSize,ySize;
//	int x,y;
	int i,n;
//	BYTE *p,*p1;
//	COLORREF dColor=RGB(255,255,255);
//	CString S;
	CFont myFont,*oldFont;
	CDC *dc,dc1;
	BITMAP bmp;
	RECT rc;
	CBitmap *oldB;

	((CBitmap *)b)->GetBitmap(&bmp);
	rc.top=rc.left=0;
	rc.bottom=bmp.bmHeight;
	rc.right=bmp.bmWidth;
	xSize=bmp.bmWidth/4,ySize=bmp.bmHeight/2;		// mah, tanto per autoadeguarsi ... ad altezza

  myFont.CreateFont(flag ? 28 : 14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
	dc=theApp.m_pMainWnd->GetDC();
//	dc=GetDesktopDC();
	dc1.CreateCompatibleDC(dc);
//	dc2.CreateCompatibleDC(&dc1);

//					CBitmap pBitmap2; pBitmap2.CreateCompatibleBitmap(&dc1,rc.right,rc.bottom);
//					oldB2=(CBitmap *)dc3.SelectObject((CBitmap *)&pBitmap2);
//					BITMAP bmp2;
//					pBitmap2.GetBitmap(&bmp2);

	oldB=(CBitmap *)dc1.SelectObject((CBitmap *)b);
//	oldB2=(CBitmap *)dc2.SelectObject((CBitmap *)b);
	oldFont=(CFont *)dc1.SelectObject(myFont);
	dc1.SetTextColor(RGB(255,255,255) /*dColor*/);
	dc1.SetBkColor(0 /*0xc0c0c0*/);			// colore?? no , vuole solo b/n...
	i=dc1.TextOut(1 /*xSize/2*/,1/*ySize/2*/,s,_tcslen(s));
	

//	i=dc2.BitBlt(0,0,rc.right,rc.bottom,&dc1,0,0,SRCCOPY);

//	dc2.SelectObject(oldB2);
	dc1.SelectObject(oldB);
//	dc1.SelectObject(oldFont);

	/*
	dc.SetOutputDC(hDC);
	dc2.CreateCompatibleDC(&dc);		// non e' necessario Delete: lo fa il distruttore!
	pBitmap=new CBitmap;
	if(!pBitmap)
		goto salva_non_ok;
//	pBitmap->CreateBitmap(myRect.right,myRect.bottom,1,8,NULL);
	pBitmap->CreateCompatibleBitmap(&dc,myRect.right,myRect.bottom);
	oldB=(CBitmap *)dc2.SelectObject(*pBitmap);
	dc2.BitBlt(0,0,myRect.right,myRect.bottom,&dc,0,0,SRCCOPY);

	delete pBitmap;*/


	dc1.DeleteDC();
//	dc2.DeleteDC();
	theApp.m_pMainWnd->ReleaseDC(dc);
  DeleteObject(myFont);

	return 1;
	}

int CTV::superImposeText(const LPBITMAPINFOHEADER bi,BYTE *d,const char *s,COLORREF dColor) {
	int xSize,ySize;
	int x,y,xStart,yStart,xStep,yStep;
	int i,n,n2,k;
	BYTE *p,*p1,*p2;
	CBitmap b;


	xSize=bi->biWidth/4+18;
	if(_tcslen(s) < 8)
		xSize/=2;		// bah tanto per
	ySize=bi->biHeight/16;

	k=imposeTextPos-1;
	if(!(k & 4)) {
		xSize=(xSize*2)/3;
		ySize=(ySize*2)/3;
		}
//		xStep=(xSize/3)*3;
//		yStep=-bi->biWidth*3;
	switch(k & 3) {
		case 0:			// sinistra in alto
			xStart=8;
			yStart=bi->biHeight-bi->biHeight/16;
			break;
		case 1:			// destra in alto
			xStart=(bi->biWidth-xSize*11);		// sistemare... con la dimensione grande non va bene!
			yStart=bi->biHeight-bi->biHeight/16;
			break;
		case 2:			// destra in basso
			xStart=(bi->biWidth-xSize*11);
			yStart=(ySize*2)/2;
			break;
		case 3:			// sinistra in basso
			xStart=8;
			yStart=(ySize*2)/2;
			break;
		default:
			return -1;
			break;
		}


	if(dColor == 0xffffffff) {	// calcolo colore analizzando un po' di punti della bitmap
																	// forse sarebbe meglio analizzare solo il rettangolo dove andrà l'ora... ma anche questo può cmq. servire!
		BYTE *p2;		// la bitmap e' bottom-up!
		dColor = 0 ;

		switch(bi->biBitCount) {
			case 24:
			//	p=d+(bi->biWidth*3*(ySize+2))+(bi->biWidth*3)*(bi->biHeight/7);		// la bitmap e' bottom-up!
				p=d+(bi->biWidth*3*yStart);		// la bitmap e' bottom-up!
				for(y=yStart; y<ySize; y+=(ySize/4)) {
					for(x=xStart; x<xSize; x+=(xSize/4)) {
						p2=d+(bi->biWidth*3*y)+x*3;		// la bitmap e' bottom-up!
						dColor += ((*(WORD *)p) | ((*(BYTE *)(p+2)) << 16));		// no DWORD * !!
				// mi sa che questa roba è sballata... privilegia i bit/byte + alti! 2018
						dColor /= 2; 
						}
					}
				dColor = dColor > 0x800000 ? 0x000000 : 0xffffff; 
				break;

			case 16:
				p=d+(bi->biWidth*2*yStart);		// la bitmap e' bottom-up!
				for(y=yStart; y<ySize; y+=(ySize/4)) {
					for(x=xStart; x<xSize; x+=(xSize/4)) {
						p2=d+(bi->biWidth*2*y)+x*2;		// la bitmap e' bottom-up!
						dColor += *((WORD *)p) & 0xffff;
				// mi sa che questa roba è sballata... privilagia i bit/byte + alti! 2018
						dColor /= 2; 
						}
					}
				dColor = dColor > 0x8000 ? 0x000000 : 0xffffff; 
			//controllare...

				break;
			}
		}
	else
		dColor = RGB((dColor >> 16) & 0xff,(dColor >> 8) & 0xff,dColor & 0xff);	// praticità per dopo!

//ASSERT(0);
	xSize=(xSize+15) & 0xfff0;
	b.CreateBitmap(xSize,ySize,1,1 /*24  la bitmap la vuole per forza in b/n... */,NULL);

	superImposeText(&b,s,dColor,k & 4 ? 1 : 0);

	p2=p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,xSize*ySize+100);

	n2=b.GetBitmapBits(xSize*ySize,p2);			// 24bit fissi NO !
	//o GetDIBits() ...

	if(n2) {
		switch(bi->biBitCount) {
			case 24:
				p2+=(xSize/8)*2;			// salto qualche righina
				for(y=(ySize-20+yStart); y>1-18+yStart; y--) {
					p1=d+xStart*3+((y)*bi->biWidth*3);
					for(x=xStart; x<(xStart+xSize); x+=8) {
		//				*(WORD *)p1=*(WORD *)p2; p1+=2; p2+=2; *p1++=*p2++;
						if(*p2 & 0x80) {		// la bitmap la vuole per forza in b/n... v.sopra
							*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
							}
						else {
							*(WORD *)p1=0; p1+=2; *p1++=0;
							}
						if(*p2 & 0x40) {
							*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
							}
						else {
							*(WORD *)p1=0; p1+=2; *p1++=0;
							}
						if(*p2 & 0x20) {
							*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
							}
						else {
							*(WORD *)p1=0; p1+=2; *p1++=0;
							}
						if(*p2 & 0x10) {
							*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
							}
						else {
							*(WORD *)p1=0; p1+=2; *p1++=0;
							}
						if(*p2 & 8) {
							*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
							}
						else {
							*(WORD *)p1=0; p1+=2; *p1++=0;
							}
						if(*p2 & 4) {
							*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
							}
						else {
							*(WORD *)p1=0; p1+=2; *p1++=0;
							}
						if(*p2 & 2) {
							*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
							}
						else {
							*(WORD *)p1=0; p1+=2; *p1++=0;
							}
						if(*p2 & 1) {
							*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
							}
						else {
							*(WORD *)p1=0; p1+=2; *p1++=0;
							}

						p2++;
						}
					}
				break;

			case 16:
				p2+=(xSize/8)*2;			// salto qualche righina
				for(y=(ySize-3); y>1; y--) {
					p1=d+xStart*2+((y)*bi->biWidth*2);
					for(x=0; x<xSize; x+=8) {
		//				*(WORD *)p1=*(WORD *)p2; p1+=2; p2+=2; *p1++=*p2++;
						if(*p2 & 0x80) {		// la bitmap la vuole per forza in b/n... v.sopra
							*(WORD *)p1=dColor; p1+=2; 
							}
						else {
							*(WORD *)p1=0; p1+=2; 
							}
						if(*p2 & 0x40) {
							*(WORD *)p1=dColor; p1+=2;
							}
						else {
							*(WORD *)p1=0; p1+=2; 
							}
						if(*p2 & 0x20) {
							*(WORD *)p1=dColor; p1+=2;
							}
						else {
							*(WORD *)p1=0; p1+=2; 
							}
						if(*p2 & 0x10) {
							*(WORD *)p1=dColor; p1+=2;
							}
						else {
							*(WORD *)p1=0; p1+=2; 
							}
						if(*p2 & 8) {
							*(WORD *)p1=dColor; p1+=2;
							}
						else {
							*(WORD *)p1=0; p1+=2;
							}
						if(*p2 & 4) {
							*(WORD *)p1=dColor; p1+=2;
							}
						else {
							*(WORD *)p1=0; p1+=2;
							}
						if(*p2 & 2) {
							*(WORD *)p1=dColor; p1+=2;
							}
						else {
							*(WORD *)p1=0; p1+=2;
							}
						if(*p2 & 1) {
							*(WORD *)p1=dColor; p1+=2;
							}
						else {
							*(WORD *)p1=0; p1+=2;
							}

						p2++;
						}
					}
				break;
			}
		}

	HeapFree(GetProcessHeap(),0,p);


	return 1;
	}

int CTV::superImposeBox(const LPBITMAPINFOHEADER bi,BYTE *d,const RECT *theRect,COLORREF dColor) {
	int xSize,ySize;
	int x,y,xStart,yStart;
	int i,n;
	BYTE *p,*p1;


	xStart=theRect->left;
	yStart=theRect->top;
	xSize=theRect->right-theRect->left;
	ySize=theRect->bottom-theRect->top;

	switch(bi->biBitCount) {
		case 24:
			p=d+(bi->biWidth*3*(bi->biHeight-yStart-1));		// la bitmap e' bottom-up!

			if(dColor == 0xffffffff) {	// calcolo colore analizzando un po' di punti della bitmap
																	// forse sarebbe meglio analizzare solo il rettangolo dove andrà ... ma anche questo può cmq. servire!
				BYTE *p2;		// la bitmap e' bottom-up!
				dColor = 0 ;
				for(y=yStart; y<(ySize*8); y+=(ySize/4)) {
					for(x=xStart; x<(xSize*8); x+=(xSize/4)) {
						p2=d+(bi->biWidth*3*y)+x*3;		// la bitmap e' bottom-up!
						dColor += ((*(WORD *)p2) | ((*(BYTE *)(p2+2)) << 16));		// no DWORD * !!
						dColor /= 2; 
						}
					}
				dColor = dColor > 0x800000 ? 0x000000 : 0xffffff; 
				}
			else
				dColor = RGB((dColor >> 16) & 0xff,(dColor >> 8) & 0xff,dColor & 0xff);	// praticità per dopo!

			p1=p+xStart*3-bi->biWidth*3;
			for(x=0; x<xSize; x++) {
				*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
				}
			p1=p+xStart*3;		// spessore doppio!
			for(x=0; x<xSize; x++) {
				*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
				}
			for(y=0; y<ySize; y++) {
				p1=p-(y*bi->biWidth*3)+(xStart*3);
				*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor); *(WORD *)p1=dColor; p1+=2; *p1=HIWORD(dColor);
				p1=p-(y*bi->biWidth*3)+((xStart+xSize)*3);
				*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor); *(WORD *)p1=dColor; p1+=2; *p1=HIWORD(dColor);
				}
			p1=p+xStart*3-(ySize*bi->biWidth*3);
			for(x=0; x<xSize; x++) {
				*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
				}
			p1=p+xStart*3-((ySize-1)*bi->biWidth*3);			// doppio
			for(x=0; x<xSize; x++) {
				*(WORD *)p1=dColor; p1+=2; *p1++=HIWORD(dColor);
				}

			break;

		case 16:
			p=d+(bi->biWidth*2*(bi->biHeight-yStart-1));		// la bitmap e' bottom-up!

			if(dColor == 0xffffffff) {	// calcolo colore analizzando un po' di punti della bitmap
																	// forse sarebbe meglio analizzare solo il rettangolo dove andrà ... ma anche questo può cmq. servire!
				BYTE *p2;		// la bitmap e' bottom-up!
				dColor = 0 ;
				for(y=yStart; y<ySize; y+=(ySize/4)) {
					for(x=xStart; x<xSize; x+=(xSize/4)) {
						p2=d+(bi->biWidth*2*y)+x*2;		// la bitmap e' bottom-up!
						dColor += *((WORD *)p);
						dColor /= 2; 
						}
					}
				dColor = dColor > 0x8000 ? 0x0000 : 0xffff; 
				}

			p1=p+xStart*2;
			for(x=0; x<xSize; x++) {
				*(WORD *)p1=dColor;
				}
			p1=p+xStart*2+bi->biWidth*2;
			for(x=0; x<xSize; x++) {
				*(WORD *)p1=dColor;
				}
			for(y=0; y<ySize; y++) {
				p1=p-(y*bi->biWidth*2)+(xStart*2);
				*(WORD *)p1=dColor; p1+=2; *(WORD *)p1=dColor; 
				p1=p-(y*bi->biWidth*2)+((xStart+xSize)*2);
				*(WORD *)p1=dColor; p1+=2; *(WORD *)p1=dColor; 
				}
			p1=p+xStart*2-(ySize*bi->biWidth*2);
			for(x=0; x<xSize; x++) {
				*(WORD *)p1=dColor;
				}
			p1=p+xStart*2-((ySize-1)*bi->biWidth*2);
			for(x=0; x<xSize; x++) {
				*(WORD *)p1=dColor;
				}

			break;
		}

	return 1;
	}


int CTV::checkQualityBox(const LPBITMAPINFOHEADER bi,const BYTE *d,RECT *theRect,
												 LPDWORD retLuma,DWORD retChroma[],DWORD ***myOldCells) {
	int xSize,ySize;
	int x,y,xStart,yStart,x2,y2,xStep,yStep;
	int i,n;
	int qualityThreshold=(theRect->right-theRect->left)*(theRect->bottom-theRect->top)/8;			// circa 5000 @640x480
	const BYTE *p,*p1;
//	DWORD dValue,dRGB;		//diciamo che con 800x600 pixel, griglia 8x8, fanno 100x80 8000... quindi lavoro con RGB666!
	//no, ovviamente era una fesseria ;) per ogni colore facciam la differenza, quindi max 768xpixel
	DWORD dRGBr,dRGBg,dRGBb;
	int cnt0,cnt1,cnt2;
	static DWORD newCells[QUALITYBOX_CHECK][QUALITYBOX_CHECK][3],oldCells[QUALITYBOX_CHECK][QUALITYBOX_CHECK][3];
	int diffCells[QUALITYBOX_CHECK][QUALITYBOX_CHECK][3];

	if(!qualityThreshold)
		qualityThreshold=(theRect->right-theRect->left)*(theRect->bottom-theRect->top)/12;			// circa 5000 @640x480

	xStart=theRect->left;
	yStart=theRect->top;
	xSize=theRect->right-theRect->left;
	ySize=theRect->bottom-theRect->top;
	xStep=xSize/QUALITYBOX_CHECK;
	yStep=ySize/QUALITYBOX_CHECK;

	switch(bi->biBitCount) {
		case 24:
			p=d+(bi->biWidth*3*(bi->biHeight-yStart-1));		// la bitmap e' bottom-up!

			for(y2=0; y2<QUALITYBOX_CHECK; y2++) {
				for(x2=0; x2<QUALITYBOX_CHECK; x2++) {
					dRGBr=dRGBg=dRGBb=cnt0=0;
					for(y=0; y<ySize/QUALITYBOX_CHECK; y++) {
						p1=p-((y2*yStep+y)*bi->biWidth*3)+((xStart+x2*xStep)*3);
						for(x=0; x<xSize/QUALITYBOX_CHECK; x++) {
//							dRGB = *p1 >> 2;	p1++; dRGB <<= 6;		// dell'ordine r-g-b non ci interessa!
//							dRGB |= *p1 >> 2;	p1++; dRGB <<= 6;
//							dRGB |= *p1 >> 2; p1++;
//							dValue += dRGB;
							dRGBr += *p1++;		// dell'ordine r-g-b non ci interessa!
							dRGBg += *p1++;
							dRGBb += *p1++;
							cnt0++;
							}
						}
					newCells[y2][x2][0]=dRGBr/cnt0;
					newCells[y2][x2][1]=dRGBg/cnt0;
					newCells[y2][x2][2]=dRGBb/cnt0;
					}
				}

			break;

		case 16:
			p=d+(bi->biWidth*2*(bi->biHeight-yStart-1));		// la bitmap e' bottom-up!

			for(y2=0; y2<QUALITYBOX_CHECK; y2++) {
				for(x2=0; x2<QUALITYBOX_CHECK; x2++) {
					dRGBr=dRGBg=dRGBb=cnt0=0;
					for(y=0; y<ySize; y++) {
						p1=p-(y*bi->biWidth*2)+(xStart*2);
						for(x=0; x<xSize; x++) {
							WORD c=*(WORD *)p1; p1+=2;
							dRGBr += c & 0x001f;
							dRGBg += (c & 0x07e0) >> 5;
							dRGBb += (c & 0xf800) >> 11;
							cnt0++;
							}
						}
					newCells[y2][x2][0]=dRGBr/cnt0;
					newCells[y2][x2][1]=dRGBg/cnt0;
					newCells[y2][x2][2]=dRGBb/cnt0;
					}
				}

			break;
		}

	int avgDiff[3];
	avgDiff[0]=0; avgDiff[1]=0; avgDiff[2]=0;
	for(y2=0; y2<QUALITYBOX_CHECK; y2++) {
		for(x2=0; x2<QUALITYBOX_CHECK; x2++) {
			// if(myOldCells   fare... per confrontare con una matrice prefissata...
			diffCells[y2][x2][0]=newCells[y2][x2][0]-oldCells[y2][x2][0];
			diffCells[y2][x2][1]=newCells[y2][x2][1]-oldCells[y2][x2][1];
			diffCells[y2][x2][2]=newCells[y2][x2][2]-oldCells[y2][x2][2];
			oldCells[y2][x2][0]=newCells[y2][x2][0];
			oldCells[y2][x2][1]=newCells[y2][x2][1];
			oldCells[y2][x2][2]=newCells[y2][x2][2];
			avgDiff[0]+=diffCells[y2][x2][0];
			avgDiff[1]+=diffCells[y2][x2][1];
			avgDiff[2]+=diffCells[y2][x2][2];
			}
		}
	avgDiff[0]/=(QUALITYBOX_CHECK*QUALITYBOX_CHECK);
	avgDiff[1]/=(QUALITYBOX_CHECK*QUALITYBOX_CHECK);
	avgDiff[2]/=(QUALITYBOX_CHECK*QUALITYBOX_CHECK);

	cnt0=cnt1=cnt2=0;
	for(y2=0; y2<QUALITYBOX_CHECK; y2++) {
		for(x2=0; x2<QUALITYBOX_CHECK; x2++) {
			cnt0+=abs(diffCells[y2][x2][0] - avgDiff[0]);
			cnt1+=abs(diffCells[y2][x2][1] - avgDiff[1]);
			cnt2+=abs(diffCells[y2][x2][2] - avgDiff[2]);
			}
		}

	if(theApp.debugMode) {
		char p3[64];
		wsprintf(p3,"qualityBox=%u %u %u, thrs=%u; %d %d %d",cnt0,cnt1,cnt2,qualityThreshold,
			avgDiff[0],avgDiff[1],avgDiff[2]); 
		theApp.m_pMainWnd->SetWindowText(p3);
		}
	else {
		char *p3=(char *)GlobalAlloc(GPTR,1024);
		wsprintf(p3,"qualityBox=%u, thrs=%u; %d %d %d",cnt0,qualityThreshold,
			avgDiff[0],avgDiff[1],avgDiff[2]); 
		theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p3);
		}

	if(retChroma) {
		retChroma[0]=cnt0;			// VERIFICARE pos. R & B !  
		retChroma[1]=cnt1;
		retChroma[2]=cnt1;
		}
	cnt0=cnt0+cnt1+cnt2;
	if(retLuma)
		*retLuma=cnt0;

	return cnt0>qualityThreshold;
	}



int CTV::convertColorBitmapToBN(LPBITMAPINFOHEADER bi,BYTE *d) {
	BYTE *p1,*p2;
	int x,y;
	int i,j;
	DWORD n;

	switch(bi->biBitCount) {
		case 24:
			x=bi->biWidth;
			y=bi->biHeight;
			while(y--) {
				for(i=0; i<x; i++) {
					n=(*(WORD *)d) | ((*(BYTE *)(d+2)) << 16);		// cast a DWORD poteva fallire a fine buffer.. !
		//			n=*(DWORD *)d;
					j=LOBYTE(LOWORD(n))+HIBYTE(LOWORD(n))+LOBYTE(HIWORD(n));
					//usare coefficienti del colore??
					j/=3;
					*d++=RGB(j,j,j);
					*d++=RGB(j,j,j);
					*d++=RGB(j,j,j);
					}
				}
			break;
		case 16:
			x=bi->biWidth;
			y=bi->biHeight;
			while(y--) {
				for(i=0; i<x; i++) {
					n=*(WORD *)d;
					j=LOBYTE(LOWORD(n))+HIBYTE(LOWORD(n));
					//usare coefficienti del colore??
					j/=2;

					//FINIRE!!

					*d++=RGB(j,j,j);
					*d++=RGB(j,j,j);
					}
				}
			break;
		default:
			return 0;

		}

	return 1;
	}

int CTV::mirrorBitmap(LPBITMAPINFOHEADER bi,BYTE *d) {
	BYTE *p,*p1,*p2;
	int x,y;
	int i,j;
	DWORD n;


	switch(bi->biBitCount) {
		case 24:
			x=bi->biWidth;
			y=bi->biHeight;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x*3+3);
			if(p) {
				for(j=0; j<y; j++) {
					p2=d+j*(x*3);
					memcpy(p,p2,x*3);
					p1=p;
					p2=p2+((x-1)*3);
					for(i=0; i<x; i++) {
						n=(*(WORD *)p1) | ((*(BYTE *)(p1+2)) << 16);		// cast a DWORD poteva fallire a fine buffer.. !
		//				n=*(DWORD *)p1;
						*(WORD *)p2=n;
						*(p2+2)=LOBYTE(HIWORD(n));
						p1+=3;
						p2-=3;
						}
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		case 16:
			//testare!
			x=bi->biWidth;
			y=bi->biHeight;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x*2+2);
			if(p) {
				for(j=0; j<y; j++) {
					p2=d+j*(x*2);
					memcpy(p,p2,x*2);
					p1=p;
					p2=p2+((x-1)*2);
					for(i=0; i<x; i++) {
						n=*(WORD *)p1;
						*(WORD *)p2=n;
						p1+=2;
						p2-=2;
						}
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		default:
			return 0;
		}

	return 1;
	}

int CTV::flipBitmap(LPBITMAPINFOHEADER bi,BYTE *d) {
	BYTE *p,*ps,*pd;
	int x,y;
	int i;


	switch(bi->biBitCount) {
		case 24:
			x=bi->biWidth*3;
			y=bi->biHeight;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x+3);
			if(p) {
				ps=d;
				pd=d+x*(y-1);
				y/=2;
				while(y--) {
					memcpy(p,pd,x);
					memcpy(pd,ps,x);
					memcpy(ps,p,x);
					ps+=x;
					pd-=x;
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		case 16:

			//testare!
			x=bi->biWidth*2;
			y=bi->biHeight;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x+2);
			if(p) {
				ps=d;
				pd=d+x*(y-1);
				y/=2;
				while(y--) {
					memcpy(p,pd,x);
					memcpy(pd,ps,x);
					memcpy(ps,p,x);
					ps+=x;
					pd-=x;
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		default:
			return 0;
		}

	return 1;
	}

int CTV::resampleBitmap(LPBITMAPINFOHEADER bi,BYTE *d,RECT *rcDest) {
	BYTE *p,*p1,*p2,*p3;
	int x,y,xRatio,yRatio;
	int i,j;
	DWORD n;

// COMPLETARE v. joshua-resample

	switch(bi->biBitCount) {
		case 24:
			x=bi->biWidth;
			y=bi->biHeight;
			xRatio=x/rcDest->right;
			yRatio=y/rcDest->bottom;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x*3+3);
			p3=d;
			if(p) {
				for(j=0; j<y; j+=yRatio) {
					p2=d+j*(x*3);
					memcpy(p,p2,x*3);
					p1=p;
					for(i=0; i<x; i+=xRatio) {
						n=(*(WORD *)p1) | ((*(BYTE *)(p1+2)) << 16);		// cast a DWORD poteva fallire a fine buffer.. !
		//				n=*(DWORD *)p1;
						*(WORD *)p3=n;
						*(p3+2)=LOBYTE(HIWORD(n));
						p1+=xRatio*3;
						p3+=3;
						}
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;

		case 16:
			//testare!
			x=bi->biWidth;
			y=bi->biHeight;
			xRatio=x/rcDest->right;
			yRatio=y/rcDest->bottom;
			p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,x*2+2);
			p3=d;
			if(p) {
				for(j=0; j<y; j+=yRatio) {
					p2=d+j*(x*2);
					memcpy(p,p2,x*2);
					p1=p;
					for(i=0; i<x; i+=xRatio) {
						n=*(WORD *)p1;
						*(WORD *)p3=n;
						p1+=xRatio*2;
						p3+=2;
						}
					}
				HeapFree(GetProcessHeap(),0,p);
				}
			break;
		default:
			return 0;
		}

	return 1;
	}

LRESULT CALLBACK VideoCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	CView *v;
  int i,tFrame;
  struct AV_PACKET_HDR *avh;
	static BOOL bInSend=0;
//	static int tFrameDiv=1,frameDiv=1;
//	CTV *m_TV=(CTV *)capGetUserData(hWnd) /* non e' lpVHdr->dwUser !! */;  FA CAGARE!
	static BITMAPINFOHEADER bmih;


	switch(uMsg) {
		case WM_CREATE:

			ZeroMemory(&bmih, sizeof(BITMAPINFOHEADER));
			bmih.biSize = sizeof(BITMAPINFOHEADER);
			bmih.biPlanes = 1;
			bmih.biBitCount = m_TV->m_DShow->GetBitsPerPixel();
			bmih.biCompression = m_TV->m_DShow->GetFourCC();
			bmih.biWidth = m_TV->m_DShow->GetWidth();
			bmih.biHeight = m_TV->m_DShow->GetHeight();
			bmih.biSizeImage=(bmih.biWidth*bmih.biHeight*bmih.biBitCount)/8;		// richiesto da MJPG 2019...
		
			break;
		case WM_PAINT:
		  PAINTSTRUCT ps;
		  BeginPaint(hwnd,&ps);
		  EndPaint(hwnd,&ps);
			break;
//		case WM_TIMER:
//			lpVHdr.lpData= m_TV->m_DShow->GetCurrentFrame();
//			lpVHdr.dwBytesUsed= m_TV->m_DShow->GetCurrentFrameSize();
		case WM_GRABBED_BUFFER:
			static VIDEOHDR lpVHdr;
			lpVHdr.lpData= (BYTE *)lParam;
			lpVHdr.dwBytesUsed=lpVHdr.dwBufferLength=wParam;
			lpVHdr.dwFlags=lpVHdr.dwUser=0;
//			if(lpVHdr->dwTimeCaptured < (m_TV->oldTimeCaptured+((1000-80)/m_TV->framesPerSec) ))	//correzioncina xche' il timing non e' precisissimo, ed è meglio anticipare un po'...
//				return 0;		// PATCH per la merda di Logitech che ignora il setCaptureParms per quanto riguarda i frame per sec. (v. sopra)
			m_TV->oldTimeCaptured=lpVHdr.dwTimeCaptured;
			lpVHdr.dwTimeCaptured=timeGetTime();

			if(lpVHdr.lpData) {

				if(!bInSend) {
					bInSend=TRUE;

					if(theApp.theServer) {
						v=theApp.theServer->getView();
						m_TV->compressAFrame(&lpVHdr,m_TV->m_DShow->doPreview,v,theApp.theServer->theHDD,&bmih);
						}
					bInSend=FALSE;
					}
				else {
					if(theApp.debugMode) {
						char *p=(char *)GlobalAlloc(GPTR,1024);
						wsprintf(p,"Rientrato!"); 
						theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
						}
					}


				}
			break;

		case MM_WIM_OPEN:
			{
				DWORD ti;
				m_TV->IWaveHdr1.lpData=(char *)m_TV->m_AudioBuffer1;
				m_TV->IWaveHdr1.dwBufferLength=m_TV->wfex.nAvgBytesPerSec /*176400*/ /*8000*/ / AUDIO_BUFFER_DIVIDER;
#pragma warning AUDIO dwBufferLength
				m_TV->IWaveHdr1.dwBytesRecorded=0;
				m_TV->IWaveHdr1.dwUser=(DWORD)m_TV;
				m_TV->IWaveHdr1.dwFlags=0;
				m_TV->IWaveHdr1.dwLoops=0;
				m_TV->IWaveHdr1.lpNext=NULL;
				m_TV->IWaveHdr1.reserved=0;
			waveInPrepareHeader(m_TV->m_hWaveIn,&m_TV->IWaveHdr1,sizeof(WAVEHDR));
			ti=timeGetTime()+500;
			while(!(m_TV->IWaveHdr1.dwFlags & WHDR_PREPARED) && timeGetTime()<ti);
				m_TV->IWaveHdr2.lpData=(char *)m_TV->m_AudioBuffer2;
				m_TV->IWaveHdr2.dwBufferLength=m_TV->wfex.nAvgBytesPerSec /*176400*/ /*8000*/ / AUDIO_BUFFER_DIVIDER;
				m_TV->IWaveHdr2.dwBytesRecorded=0;
				m_TV->IWaveHdr2.dwUser=(DWORD)m_TV;
				m_TV->IWaveHdr2.dwFlags=0;
				m_TV->IWaveHdr2.dwLoops=0;
				m_TV->IWaveHdr2.lpNext=NULL;
				m_TV->IWaveHdr2.reserved=0;
			waveInPrepareHeader(m_TV->m_hWaveIn,&m_TV->IWaveHdr2,sizeof(WAVEHDR));
			ti=timeGetTime()+500;
			while(!(m_TV->IWaveHdr2.dwFlags & WHDR_PREPARED) && timeGetTime()<ti);
			waveInStart(m_TV->m_hWaveIn);
			waveInAddBuffer(m_TV->m_hWaveIn,&m_TV->IWaveHdr1,sizeof(WAVEHDR));
			waveInAddBuffer(m_TV->m_hWaveIn,&m_TV->IWaveHdr2,sizeof(WAVEHDR));
//			AfxMessageBox("wave open");
			}
			break;
		case MM_WIM_DATA:
			{
	char *p;
  BYTE *pSBuf,*pSBuf2;
  struct AV_PACKET_HDR *avh=NULL;
//  struct AV_PACKET_HDR avh;
	ACMSTREAMHEADER hhacm;
	static BOOL bInSend;
	int i;
	LPWAVEHDR lpWHdr=(LPWAVEHDR)lParam;


	waveInUnprepareHeader(m_TV->m_hWaveIn,lpWHdr,sizeof(WAVEHDR));

  if(!bInSend) {
		bInSend=TRUE;
	  if(theApp.theServer) {
//			v=theApp.theServer->getView();
//			if(theApp.theServer->Opzioni & CVidsendApp::canSendAudio /*theApp.theServer->getAudio()*/ && !theApp.theServer->bPaused) {
#pragma warning FINIRE canSendAudio
			if(!theApp.theServer->bPaused) {
	//			l+=sizeof(WAVEFORMATEX);


//			goto fine_a;

				pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxWaveoutSize+AV_PACKET_HDR_SIZE  +100);
				pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;
				avh=(struct AV_PACKET_HDR *)pSBuf;

#ifdef _STANDALONE_MODE
				avh->tag=MAKEFOURCC('D','G','2','0');
#else
				avh->tag=MAKEFOURCC('G','D','2','0');
#endif
				avh->type=AV_PACKET_TYPE_AUDIO;
	//				avh.psec=1000;
				avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
				avh->info=0;




				v=theApp.theServer->getView();
				v->SendMessage(WM_RAWAUDIOFRAME_READY,(WPARAM)lpWHdr->lpData,(LPARAM)lpWHdr->dwBytesRecorded);
// OCCHIO! serve Send o il buffer viene cimito 



//					wsprintf(myBuf,"WIM_pre_compress");
//	theApp.FileSpool->print(1,myBuf);


				if(m_TV->m_hAcm) {
					hhacm.cbStruct=sizeof(ACMSTREAMHEADER);
					hhacm.fdwStatus=0;
					hhacm.dwUser=(DWORD)0 /*this*/;
					hhacm.pbSrc=(BYTE *)lpWHdr->lpData;
					hhacm.cbSrcLength=lpWHdr->dwBytesRecorded;
			//			hhacm.cbSrcLengthUsed=0;
					hhacm.dwSrcUser=0;
					hhacm.pbDst=pSBuf2;
					hhacm.cbDstLength=m_TV->maxWaveoutSize;
			//			hhacm.cbDstLengthUsed=0;
					hhacm.dwDstUser=0;
					if(!acmStreamPrepareHeader(m_TV->m_hAcm,&hhacm,0)) {
						i=acmStreamConvert(m_TV->m_hAcm,&hhacm,ACM_STREAMCONVERTF_BLOCKALIGN);
					//	wsprintf(myBuf,"convertito: %d, %d bytes",i,l);
					//	AfxMessageBox(myBuf);
						acmStreamUnprepareHeader(m_TV->m_hAcm,&hhacm,0);

						avh->len=hhacm.cbDstLengthUsed;


	//					wsprintf(myBuf,"WIM_Convert");
	//	theApp.FileSpool->print(1,myBuf);


						if(m_TV->aviFile && m_TV->psAudio) {
							i= AVIStreamWrite(m_TV->psAudio,// stream pointer 
								m_TV->aFrameNum, // time of this frame 
								1,// number to write 
								pSBuf2,
								hhacm.cbDstLengthUsed,
								avh->info, // flags.... 
								NULL, NULL);
							}



		/*				p=(char *)GlobalAlloc(GPTR,1024);
						wsprintf(p,"AFrameT# %ld: lungo %ld (%ld) %ld",m_TV->gdaFrameNum,hhacm.cbDstLengthUsed,lpWHdr->dwBytesRecorded,lpWHdr->dwBufferLength); 
						theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);*/
							avh->reserved1=avh->reserved2=0;
	//						theApp.OnAudioFrameReady((WPARAM)pSBuf,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
							v=theApp.theServer->getView();
							v->PostMessage(WM_AUDIOFRAME_READY,(WPARAM)pSBuf,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
	//					if(theApp.theServer->streamSocketA->Manda(&avh,pSBuf,hhacm.cbDstLengthUsed))
	//						m_TV->aFrameNum;
						}
					}
				else {
					HeapFree(GetProcessHeap(),0,pSBuf);
					}
				}
			}


fine_a:

		bInSend=FALSE;
		}
	lpWHdr->lpData= (char *)(lpWHdr == &m_TV->IWaveHdr1 ? m_TV->m_AudioBuffer1 : m_TV->m_AudioBuffer2);
				lpWHdr->dwBufferLength=m_TV->wfex.nAvgBytesPerSec /*176400*/ /*8000*/ / AUDIO_BUFFER_DIVIDER;
#pragma warning AUDIO dwBufferLength2
				lpWHdr->dwBytesRecorded=0;
				lpWHdr->dwUser=(DWORD)m_TV;
				lpWHdr->dwFlags=0;
				lpWHdr->dwLoops=0;
				lpWHdr->lpNext=NULL;
				lpWHdr->reserved=0;
	waveInPrepareHeader(m_TV->m_hWaveIn,lpWHdr,sizeof(WAVEHDR));
			DWORD ti=timeGetTime()+500;
			while(!(lpWHdr->dwFlags & WHDR_PREPARED) && timeGetTime()<ti);
	waveInAddBuffer(m_TV->m_hWaveIn,lpWHdr,sizeof(WAVEHDR));
			}
			break;
		case MM_WIM_CLOSE:
			waveInStop(m_TV->m_hWaveIn);
			waveInReset(m_TV->m_hWaveIn);
			waveInUnprepareHeader(m_TV->m_hWaveIn,&m_TV->IWaveHdr1,sizeof(WAVEHDR));
			waveInUnprepareHeader(m_TV->m_hWaveIn,&m_TV->IWaveHdr2,sizeof(WAVEHDR));
			break;

		case WM_CLOSE:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
			break;
		}

	return 0;


#ifdef DARIO

	if(theApp.debugMode) {
		if(theApp.FileSpool) {
			char myBuf[128];
			wsprintf(myBuf,"millisecondi %u",lpVHdr->dwTimeCaptured);
			*theApp.FileSpool << myBuf;
			}
		}

	if(lpVHdr->dwTimeCaptured < (m_TV->oldTimeCaptured+((1000-80)/m_TV->framesPerSec) ))	//correzioncina xche' il timing non e' precisissimo, ed è meglio anticipare un po'...
		return 0;		// PATCH per la merda di Logitech che ignora il setCaptureParms per quanto riguarda i frame per sec. (v. sopra)
	m_TV->oldTimeCaptured=lpVHdr->dwTimeCaptured;

	pInp=lpVHdr->lpData;

/*		if(f=fopen("c:\\myframe1.bmp","wb")) {
		fwrite(pInp,lpVHdr->dwBytesUsed,1,f);
		fclose(f);
		}*/
	if(!bInSend) {
		bInSend=TRUE;
	  if(theApp.theServer) {
			v=theApp.theServer->getView();
			if(theApp.theServer->Opzioni & CVidsendDoc2::sendVideo && !theApp.theServer->bPaused) {

				if(theApp.theServer->Opzioni & CVidsendDoc2::videoType) {
					pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxFrameSize+AV_PACKET_HDR_SIZE+100);
					pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;				// indirizzo del buffer DOPO la struct header!
					avh=(struct AV_PACKET_HDR *)pSBuf;
					avh->type=0;
#ifdef _STANDALONE_MODE
					avh->tag=MAKEFOURCC('D','G','2','0');
#else
					avh->tag=MAKEFOURCC('G','D','2','0');
#endif
	//					avh.psec=1000 / m_TV->framesPerSec;
					avh->info=0;
					avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());	// anche lpVHdr->dwTimeCaptured
					memcpy(pSBuf2,&m_TV->biCompDef.bmiHeader,sizeof(BITMAPINFOHEADER));
					memcpy(pSBuf2+sizeof(BITMAPINFOHEADER),pInp,lpVHdr->dwBytesUsed);
					avh->len=lpVHdr->dwBytesUsed+sizeof(BITMAPINFOHEADER);
					avh->info=lpVHdr->dwFlags & VHDR_KEYFRAME ? AV_PACKET_INFO_KEYFRAME : 0;
					// in b8 ora c'è qbox AV_PACKET_INFO_QBOX

					if(theApp.debugMode) {
						char *p=(char *)GlobalAlloc(GPTR,1024);
						wsprintf(p,"VFrame (precomp)# %ld: lungo %ld (%ld)",m_TV->vFrameNum,lpVHdr->dwBufferLength,lpVHdr->dwBytesUsed); 
						theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
						}

					}
				else {

					if(m_TV->biRawBitmap.biCompression != 0) {
						if(m_TV->hICDe) {
							pInp=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->biBaseRawBitmap.biHeight*m_TV->biBaseRawBitmap.biWidth*m_TV->biBaseRawBitmap.biBitcount/8);
							if(pInp) {
								pInpAllocated=TRUE;
								i=ICDecompress(m_TV->hICDe,0 /*ICDECOMPRESS_HURRYUP*/,
									&m_TV->biRawBitmap,lpVHdr->lpData,&m_TV->biBaseRawBitmap,pInp);
								if(i == 0 /* se metto HURRYUP restituisce 1, ossia DONTDRAW (vfw.h) ma NON decomprime! */) {
									}
								else
									goto not_RGB;
								}
							else
								goto not_RGB;
							}
						else
							goto not_RGB;
						}

					if(m_TV->theFrame == (BYTE *)-1) {		// se richiesto...
						m_TV->theFrame=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->biBaseRawBitmap.biHeight*m_TV->biBaseRawBitmap.biWidth*m_TV->biBaseRawBitmap.biBitcount/8);	// prelevo e salvo il fotogramma per uso altrove (p.es. salva img)
						memcpy(m_TV->theFrame,pInp,m_TV->biBaseRawBitmap.biHeight*m_TV->biBaseRawBitmap.biWidth*m_TV->biBaseRawBitmap.biBitcount/8);
						}

					if(theApp.theServer->Opzioni & CVidsendDoc2::forceBN) {
						m_TV->convertColorBitmapToBN(&m_TV->biRawDef.bmiHeader,pInp);
						}
					if(theApp.theServer->Opzioni & CVidsendDoc2::doFlip) {
						m_TV->flipBitmap(&m_TV->biRawDef.bmiHeader,pInp);
						}
					if(theApp.theServer->Opzioni & CVidsendDoc2::doMirror) {
						m_TV->mirrorBitmap(&m_TV->biRawDef.bmiHeader,pInp);
						}
					if(m_TV->imposeTime) {
						m_TV->superImposeDateTime(&m_TV->biRawDef.bmiHeader,pInp);
						}
					if(m_TV->imposeTextPos) {
						m_TV->superImposeText(&m_TV->biRawDef.bmiHeader,pInp,m_TV->imposeText,RGB(200,200,0));
						}
					if(theApp.theServer->myQV.compressor) {
						l=t=0;
						pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxFrameSize+AV_PACKET_HDR_SIZE+100);
						pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;			// indirizzo del buffer DOPO la struct header!
						avh=(struct AV_PACKET_HDR *)pSBuf;
						avh->type=0;
#ifdef _STANDALONE_MODE
						avh->tag=MAKEFOURCC('D','G','2','0');
#else
						avh->tag=MAKEFOURCC('G','D','2','0');
#endif
		//	ICSeqCompressFrameStart ???	ICSeqCompressFrame
						t=l=0;
						if(m_TV->framesPerSec >1)		// usare KFrame...
							tFrame=(m_TV->vFrameNum % m_TV->framesPerSec) ? 0 : ICCOMPRESS_KEYFRAME;
						else
							tFrame=(m_TV->vFrameNum & 1) ? ICCOMPRESS_KEYFRAME : 0;
//						pSBuf=(BYTE *)ICSeqCompressFrame(&theApp.theServer->theTV->cv,0,lpVHdr->lpData,&t,&l);
						i=ICCompress(m_TV->hICCo,tFrame,
							&m_TV->biCompDef.bmiHeader,pSBuf2+sizeof(BITMAPINFOHEADER),&m_TV->biBaseRawBitmap,pInp,
							&l,&t,m_TV->vFrameNum,0/*2500*/,theApp.theServer->myQV.quality,
							NULL,NULL);
//https://forum.videohelp.com/threads/371608-New-lossless-video-codec
						if(i == ICERR_OK) {
//							CFile mf;

	//						mf.Open("c:\\frame0.bmp",CFile::modeWrite | CFile::modeCreate);
	//						mf.Write(pInp,lpVHdr->dwBytesUsed);
	//						mf.Close();
	//						mf.Open("c:\\frame.bmp",CFile::modeWrite | CFile::modeCreate);
	//						mf.Write(pSBuf,l);
	//						mf.Close();
							memcpy(pSBuf2,&m_TV->biCompDef.bmiHeader,sizeof(BITMAPINFOHEADER));
							avh->len=m_TV->biCompDef.bmiHeader.biSizeImage+sizeof(BITMAPINFOHEADER);
	//							avh.psec=1000 / m_TV->framesPerSec;
							avh->info=t;
							// in b8 ora c'è qbox AV_PACKET_INFO_QBOX
							avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());

							if(theApp.debugMode) {
								char *p=(char *)GlobalAlloc(GPTR,1024);
		//						wsprintf(p,"VFrame# %u: lungo %u (%u)",m_TV->gdvFrameNum,lpVHdr->dwBytesUsed,l); 
								wsprintf(p,"oldTime %u, time %u",m_TV->oldTimeCaptured,lpVHdr->dwTimeCaptured); 
								theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
								}
	//						memcpy(avh.lpData,pSBuf,l+40);

							}
						else {
not_RGB:
							MessageBeep(-1);
							if(pSBuf) 
								HeapFree(GetProcessHeap(),0,pSBuf);
							goto fine;
							}
						}
					else {
						pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxFrameSize+AV_PACKET_HDR_SIZE+100);
						pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;					// indirizzo del buffer DOPO la struct header!
						avh=(struct AV_PACKET_HDR *)pSBuf;
						avh->type=0;
#ifdef _STANDALONE_MODE
						avh->tag=MAKEFOURCC('D','G','2','0');
#else
						avh->tag=MAKEFOURCC('G','D','2','0');
#endif
	//						avh.psec=1000 / m_TV->framesPerSec;
						avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
						avh->len=lpVHdr->dwBytesUsed+sizeof(BITMAPINFOHEADER);
						memcpy(pSBuf2,&m_TV->biRawDef.bmiHeader,sizeof(BITMAPINFOHEADER));
						memcpy(pSBuf2+sizeof(BITMAPINFOHEADER),pInp,lpVHdr->dwBytesUsed);
						avh->info=lpVHdr->dwFlags & VHDR_KEYFRAME ? AV_PACKET_INFO_KEYFRAME : 0;
						// in b8 ora c'è qbox AV_PACKET_INFO_QBOX

						if(theApp.debugMode) {
							char *p=(char *)GlobalAlloc(GPTR,1024);
							wsprintf(p,"VFrame (raw) # %ld: lungo %ld (%ld)",m_TV->vFrameNum,lpVHdr->dwBufferLength,lpVHdr->dwBytesUsed); 
							theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
							}

						}
					}

fine_ok:
				//m_TV->vFrameNum++;			// messo in OnVideoFrameReady()
				if(m_TV->aviFile && m_TV->psVideo) {
					if(m_TV->opzioniSave & CVidsendDoc2::quantiFrame)
						m_TV->saveWait4KeyFrame=1;	// con questo trucco salvo solo i k-frame ossia 1 al secondo!
					if(m_TV->saveWait4KeyFrame) {
						if(avh->info & AV_PACKET_INFO_KEYFRAME)
							m_TV->saveWait4KeyFrame=0;
						else
							goto skipSave;
						}
					i= AVIStreamWrite(m_TV->psVideo,// stream pointer 
						m_TV->vFrameNum4Save, // time of this frame 
						1,// number to write 
						pSBuf2+sizeof(BITMAPINFOHEADER),
						avh->len-sizeof(BITMAPINFOHEADER),	/*m_TV->biCompDef.bmiHeader.biSizeImage,*/
						avh->info & AV_PACKET_INFO_KEYFRAME, // flags.... 
						NULL, NULL);
					if(i!=AVIERR_OK)
						if(theApp.debugMode)
							if(theApp.FileSpool)
								*theApp.FileSpool << "errore in stream video salva";
					if(m_TV->psText && m_TV->imposeTime && !(m_TV->vFrameNum4Save % m_TV->framesPerSec)) {
						CString S;
						S=CTime::GetCurrentTime().Format("%d/%m/%Y %H:%M:%S");
						i= AVIStreamWrite(m_TV->psText,// stream pointer 
							m_TV->vFrameNum4Save/m_TV->framesPerSec, // time of this frame 
							1,// number to write 
							(LPSTR)(LPCTSTR)S,
							S.GetLength()+1,
							avh->info & AV_PACKET_INFO_KEYFRAME, // flags.... 
							NULL, NULL);
						}
					m_TV->vFrameNum4Save++;
skipSave:	;
					}

				if(v && pSBuf) {
					avh->reserved1=avh->reserved2=0;
					v->PostMessage(WM_VIDEOFRAME_READY,(WPARAM)pSBuf,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
					}

fine: ;
				}
			else {
				if(theApp.debugMode) {
					char *p=(char *)GlobalAlloc(GPTR,1024);
					wsprintf(p,"Non mando video!"); 
					theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
					}
				}

			}
		if(pInpAllocated)
			HeapFree(GetProcessHeap(),0,pInp);
		bInSend=FALSE;
		}
	else {
		if(theApp.debugMode) {
			char *p=(char *)GlobalAlloc(GPTR,1024);
			wsprintf(p,"Rientrato!"); 
			theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
			}
		}

//	wsprintf(p,"%ld",m_TV->gdvFrameNum++); 


#endif //DARIO

  return (LRESULT)TRUE;
  }

// OCX USA  la callback video!! v. sopra
#ifdef DARIO
LRESULT CALLBACK WaveCallbackProc(HWND hWnd, LPWAVEHDR lpWHdr) {
	CView *v;
	char *p;
  BYTE *pSBuf,*pSBuf2;
  struct AV_PACKET_HDR *avh=NULL;
//  struct AV_PACKET_HDR avh;
	ACMSTREAMHEADER hhacm;
	static BOOL bInSend=0;
	int i;

  if(!bInSend) {
		bInSend=TRUE;
	  if(theApp.theServer) {
			v=theApp.theServer->getView();
			if(theApp.theServer->bAudio && !theApp.theServer->bPaused) {
	//			l+=sizeof(WAVEFORMATEX);

				pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxWaveoutSize);
				pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;
				avh=(struct AV_PACKET_HDR *)pSBuf;

#ifdef _STANDALONE_MODE
				avh->tag=MAKEFOURCC('D','G','2','0');
#else
				avh->tag=MAKEFOURCC('G','D','2','0');
#endif
				avh->type=AV_PACKET_TYPE_AUDIO;
	//				avh.psec=1000;
				avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
				avh->info=0;
// USARE						measureAudio((BYTE *)wh->lpData,wh->dwBufferLength); per PTT mode!


				if(m_TV->m_hAcm) {
					hhacm.cbStruct=sizeof(ACMSTREAMHEADER);
					hhacm.fdwStatus=0;
					hhacm.dwUser=(DWORD)0 /*this*/;
					hhacm.pbSrc=(BYTE *)lpWHdr->lpData;
					hhacm.cbSrcLength=lpWHdr->dwBytesRecorded;
			//			hhacm.cbSrcLengthUsed=0;
					hhacm.dwSrcUser=0;
					hhacm.pbDst=pSBuf2;
					hhacm.cbDstLength=m_TV->maxWaveoutSize;
			//			hhacm.cbDstLengthUsed=0;
					hhacm.dwDstUser=0;
					if(!acmStreamPrepareHeader(m_TV->m_hAcm,&hhacm,0)) {
						i=acmStreamConvert(m_TV->m_hAcm,&hhacm,ACM_STREAMCONVERTF_BLOCKALIGN);
						if(theApp.debugMode>3) {
							char myBuf[128];
							wsprintf(myBuf,"convertito: %d, %d bytes",i,hhacm.cbDstLengthUsed);
							AfxMessageBox(myBuf);
							}
						acmStreamUnprepareHeader(m_TV->m_hAcm,&hhacm,0);

						avh->len=hhacm.cbDstLengthUsed;

						if(m_TV->aviFile && m_TV->psAudio) {
							i= AVIStreamWrite(m_TV->psAudio,// stream pointer 
								m_TV->aFrameNum, // time of this frame 
								1,// number to write 
								pSBuf2,
								hhacm.cbDstLengthUsed,
								avh->info, // flags.... 
								NULL, NULL);
							}


						if(theApp.debugMode) {
							p=(char *)GlobalAlloc(GPTR,1024);
							wsprintf(p,"AFrameT# %ld: lungo %ld (%ld) %ld",m_TV->aFrameNum,hhacm.cbDstLengthUsed,lpWHdr->dwBytesRecorded,lpWHdr->dwBufferLength); 
							theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
							}
						if(v) {
							avh->reserved1=avh->reserved2=0;
	// USARE						measureAudio((BYTE *)wh->lpData,wh->dwBufferLength); per PTT mode!
							v->PostMessage(WM_AUDIOFRAME_READY,(WPARAM)pSBuf,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
	//					if(theApp.theServer->streamSocketA->Manda(&avh,pSBuf,hhacm.cbDstLengthUsed))
	//						m_TV->aFrameNum;
							}
						}
					}
				else {
					HeapFree(GetProcessHeap(),0,pSBuf);
					}
			}
		bInSend=FALSE;
		}
  return (LRESULT)TRUE;

  }
#endif


int CTV::compressAFrame(LPVIDEOHDR lpVHdr,int doPreview,CWnd *v,HDRAWDIB hdd,LPBITMAPINFOHEADER bmih) {
	BYTE *pInp,*pSBuf=NULL,*pSBuf2=NULL;
	BOOL pInpAllocated=FALSE;
  int i,q,tFrame;
	DWORD l,t;
  struct AV_PACKET_HDR *avh;
	CDC *hdc;

	pInp=lpVHdr->lpData;
	q=0;

	if(doPreview) {
		hdc = v->GetDC();
		i=DrawDibDraw(hdd, hdc->m_hDC, 0, 0, bmih->biWidth, bmih->biHeight, 
			bmih, lpVHdr->lpData, 0, 0, bmih->biWidth , bmih->biHeight, 0 /*DDF_SAME_DRAW*/);	
		v->ReleaseDC(hdc);

#if 0
		in O7AREA non andava (nella callback) e quindi facevamo così...

					bmih.biCompression=0;	//hmmm, e quindi serve così
//#define SERVE_PATCH_PINNACLE 1
#ifdef SERVE_PATCH_PINNACLE
					bmih.biBitCount=32;
#else
					bmih.biBitCount=24;
#endif
//ASSERT(0);
					// qui, se è 16:9, non dobbiamo fare nulla perché è sufficiente stretchare la finestra madre! (2016)
					PAINTSTRUCT ps;
					BeginPaint(hwnd, &ps);
	        SetStretchBltMode(hdc, COLORONCOLOR);
					//xShowSize 2016?
					i=StretchDIBits(hdc, 0, 0, /*xShowSize */rc.right, /*yShowSize*/rc.bottom, 
						0, 0, bmih.biWidth , bmih.biHeight, pInp, (BITMAPINFO *)&bmih, DIB_RGB_COLORS, SRCCOPY );	
	        EndPaint(hwnd, &ps);
					ReleaseDC(hwnd, hdc);
					}
#endif

/*	FILE *f;
	if(f=fopen("c:\\myframe1.bmp","wb")) {
		fwrite(pInp,lpVHdr->dwBytesUsed,1,f);
		fclose(f);
		}*/

		}

	if(theApp.theServer->Opzioni & CVidsendDoc2::sendVideo && !theApp.theServer->bPaused) {

		if(theApp.theServer->Opzioni & CVidsendDoc2::videoType) {
			static DWORD lastTimeKeyframe=0;
			pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxFrameSize+AV_PACKET_HDR_SIZE+100);
			pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;				// indirizzo del buffer DOPO la struct header!
			avh=(struct AV_PACKET_HDR *)pSBuf;
			avh->type=AV_PACKET_TYPE_VIDEO;
#ifdef _STANDALONE_MODE
			avh->tag=MAKEFOURCC('D','G','2','0');
#else
			avh->tag=MAKEFOURCC('G','D','2','0');
#endif
//					avh.psec=1000 / m_TV->framesPerSec;
			memcpy(pSBuf2,&m_TV->biCompDef.bmiHeader,sizeof(BITMAPINFOHEADER));
			memcpy(pSBuf2+sizeof(BITMAPINFOHEADER),pInp,lpVHdr->dwBytesUsed);
			avh->len=lpVHdr->dwBytesUsed+sizeof(BITMAPINFOHEADER);
			avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());	// anche lpVHdr->dwTimeCaptured
//			avh->info=lpVHdr->dwFlags & VHDR_KEYFRAME ? AV_PACKET_INFO_KEYFRAME : 0; qua non c'è, quindi lo simulo
			if((timeGetTime()-lastTimeKeyframe) > 1000) {
				avh->info=AV_PACKET_INFO_KEYFRAME;
				lastTimeKeyframe=timeGetTime();
				}
			else
				avh->info=0;
			// in b8 ora c'è qbox...

			if(theApp.debugMode) {
				char *p=(char *)GlobalAlloc(GPTR,1024);
				wsprintf(p,"VFrame (precomp)# %ld: lungo %ld (%ld)",m_TV->vFrameNum,lpVHdr->dwBufferLength,lpVHdr->dwBytesUsed); 
				theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
				}

			}
		else {

			if(m_TV->biRawBitmap.biCompression != 0) {
				if(m_TV->m_hICDe) {
					pInp=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->biBaseRawBitmap.biHeight*m_TV->biBaseRawBitmap.biWidth*m_TV->biBaseRawBitmap.biBitCount/8);
					if(pInp) {
						pInpAllocated=TRUE;
						i=ICDecompress(m_TV->m_hICDe,0 /*ICDECOMPRESS_HURRYUP*/,
							&m_TV->biRawBitmap,lpVHdr->lpData,&m_TV->biBaseRawBitmap,pInp);
						if(i == 0 /* se metto HURRYUP restituisce 1, ossia DONTDRAW (vfw.h) ma NON decomprime! */) {
							}
						else
							goto not_RGB;
						}
					else
						goto not_RGB;
					}
				else
					goto not_RGB;
				}

			if(m_TV->theFrame == (BYTE *)-1) {		// se richiesto...
				l=m_TV->biBaseRawBitmap.biHeight*m_TV->biBaseRawBitmap.biWidth*m_TV->biBaseRawBitmap.biBitCount/8;
				m_TV->theFrame=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,l);	// prelevo e salvo il fotogramma per uso altrove (p.es. salva img)
				memcpy(m_TV->theFrame,pInp,l);
				}

			// quando arriviamo qua, siam SEMPRE a 24bit direi... (v. biRawDef)
			if(theApp.theServer->Opzioni & CVidsendDoc2::forceBN) {
				m_TV->convertColorBitmapToBN(&m_TV->biRawDef.bmiHeader,pInp);
				}
			if(theApp.theServer->Opzioni & CVidsendDoc2::doFlip) {
				m_TV->flipBitmap(&m_TV->biRawDef.bmiHeader,pInp);
				}
			if(theApp.theServer->Opzioni & CVidsendDoc2::doMirror) {
				m_TV->mirrorBitmap(&m_TV->biRawDef.bmiHeader,pInp);
				}
			if(m_TV->imposeTime) {
				m_TV->superImposeDateTime(&m_TV->biRawDef.bmiHeader,pInp);
				}
			if(m_TV->imposeTextPos) {
				m_TV->superImposeText(&m_TV->biRawDef.bmiHeader,pInp,m_TV->imposeText,RGB(200,200,0));
				}
			if(!IsRectEmpty(&theApp.theServer->qualityBox)) {
				q=m_TV->checkQualityBox(&m_TV->biRawDef.bmiHeader,pInp,&theApp.theServer->qualityBox);
				m_TV->superImposeBox(&m_TV->biRawDef.bmiHeader,pInp,&theApp.theServer->qualityBox,q ? RGB(255,0,0) : RGB(0,255,0));
				}
			if(theApp.theServer->myQV.compressor) {
				l=t=0;
				pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxFrameSize+AV_PACKET_HDR_SIZE+100);
				pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;			// indirizzo del buffer DOPO la struct header!
				avh=(struct AV_PACKET_HDR *)pSBuf;
				avh->type=AV_PACKET_TYPE_VIDEO;
#ifdef _STANDALONE_MODE
				avh->tag=MAKEFOURCC('D','G','2','0');
#else
				avh->tag=MAKEFOURCC('G','D','2','0');
#endif
//	ICSeqCompressFrameStart ???	ICSeqCompressFrame
				if(m_TV->framesPerSec >1)		// usare KFrame...
					tFrame=(m_TV->vFrameNum % m_TV->framesPerSec) ? 0 : ICCOMPRESS_KEYFRAME;
				else
					tFrame=(m_TV->vFrameNum & 1) ? ICCOMPRESS_KEYFRAME : 0;
				i=ICCompress(m_TV->m_hICCo,tFrame,
					&m_TV->biCompDef.bmiHeader,pSBuf2+sizeof(BITMAPINFOHEADER),&m_TV->biBaseRawBitmap,pInp,
					&l,&t,m_TV->vFrameNum,0/*2500*/,theApp.theServer->myQV.quality,
					NULL,NULL);
				if(i == ICERR_OK) {
//							CFile mf;

//						mf.Open("c:\\frame0.bmp",CFile::modeWrite | CFile::modeCreate);
//						mf.Write(pInp,lpVHdr->dwBytesUsed);
//						mf.Close();
//						mf.Open("c:\\frame.bmp",CFile::modeWrite | CFile::modeCreate);
//						mf.Write(pSBuf,l);
//						mf.Close();
					memcpy(pSBuf2,&m_TV->biCompDef.bmiHeader,sizeof(BITMAPINFOHEADER));
					avh->len=m_TV->biCompDef.bmiHeader.biSizeImage+sizeof(BITMAPINFOHEADER);
//							avh.psec=1000 / m_TV->framesPerSec;
					avh->info=((t & AVIIF_KEYFRAME) ? AV_PACKET_INFO_KEYFRAME : 0);
					if(q)
						avh->info |= AV_PACKET_INFO_QBOX;
					if(m_TV->aviFile && m_TV->psVideo)
						avh->info |= AV_PACKET_INFO_RECORDING;
					if(theApp.theServer->Opzioni & (CVidsendDoc2::forceBN | CVidsendDoc2::doFlip | CVidsendDoc2::doMirror))
						avh->info |= AV_PACKET_INFO_VIDEOEDIT;
					
					avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());

					if(theApp.debugMode) {
						char *p=(char *)GlobalAlloc(GPTR,1024);
//						wsprintf(p,"VFrame# %u: lungo %u (%u)",m_TV->gdvFrameNum,lpVHdr->dwBytesUsed,l); 
						wsprintf(p,"oldTime %u, time %u",m_TV->oldTimeCaptured,lpVHdr->dwTimeCaptured); 
						theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
						}
//						memcpy(avh.lpData,pSBuf,l+40);

					}
				else {
not_RGB:
					MessageBeep(-1);
					if(pSBuf) 
						HeapFree(GetProcessHeap(),0,pSBuf);
					goto fine;
					}
				}
			else {
				pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,m_TV->maxFrameSize+AV_PACKET_HDR_SIZE+100);
				pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;					// indirizzo del buffer DOPO la struct header!
				avh=(struct AV_PACKET_HDR *)pSBuf;
				avh->type=AV_PACKET_TYPE_VIDEO;
#ifdef _STANDALONE_MODE
				avh->tag=MAKEFOURCC('D','G','2','0');
#else
				avh->tag=MAKEFOURCC('G','D','2','0');
#endif
//						avh.psec=1000 / m_TV->framesPerSec;
				avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
				avh->len=lpVHdr->dwBytesUsed+sizeof(BITMAPINFOHEADER);
				memcpy(pSBuf2,&m_TV->biRawDef.bmiHeader,sizeof(BITMAPINFOHEADER));
				memcpy(pSBuf2+sizeof(BITMAPINFOHEADER),pInp,lpVHdr->dwBytesUsed);
				avh->info=(lpVHdr->dwFlags & VHDR_KEYFRAME) ? AV_PACKET_INFO_KEYFRAME : 0;
// qua non si applica				if(q)
//					avh->info |= AV_PACKET_INFO_QBOX;
				if(m_TV->aviFile && m_TV->psVideo)
					avh->info |= AV_PACKET_INFO_RECORDING;
// qua non si applica				if(theApp.theServer->Opzioni & (CVidsendDoc2::forceBN | CVidsendDoc2::doFlip | CVidsendDoc2::doMirror))
//					avh->info |= AV_PACKET_INFO_VIDEOEDIT;

				if(theApp.debugMode) {
					char *p=(char *)GlobalAlloc(GPTR,1024);
					wsprintf(p,"VFrame (raw) # %ld: lungo %ld (%ld)",m_TV->vFrameNum,lpVHdr->dwBufferLength,lpVHdr->dwBytesUsed); 
					theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
					}

				}
			}

fine_ok:
		m_TV->vFrameNum;
		if(m_TV->aviFile && m_TV->psVideo) {
			if(m_TV->opzioniSave & CVidsendDoc2::quantiFrame)
				m_TV->saveWait4KeyFrame=1;	// con questo trucco salvo solo i k-frame ossia 1 al secondo!
			if(m_TV->saveWait4KeyFrame) {
				if(avh->info & AV_PACKET_INFO_KEYFRAME)
					m_TV->saveWait4KeyFrame=0;
				else
					goto skipSave;
				}
			i= AVIStreamWrite(m_TV->psVideo,// stream pointer 
				m_TV->vFrameNum4Save, // time of this frame 
				1,// number to write 
				pSBuf2+sizeof(BITMAPINFOHEADER),
				avh->len-sizeof(BITMAPINFOHEADER),	/*m_TV->biCompDef.bmiHeader.biSizeImage,*/
				avh->info, // flags.... 
				NULL, NULL);
			if(i!=AVIERR_OK)
				if(theApp.debugMode)
					if(theApp.FileSpool)
						*theApp.FileSpool << "errore in stream video salva";
			if(m_TV->psText && m_TV->imposeTime && !(m_TV->vFrameNum4Save % m_TV->framesPerSec)) {
				CString S;
				S=CTime::GetCurrentTime().Format("%d/%m/%Y %H:%M:%S");
				i= AVIStreamWrite(m_TV->psText,// stream pointer 
					m_TV->vFrameNum4Save/m_TV->framesPerSec, // time of this frame 
					1,// number to write 
					(LPSTR)(LPCTSTR)S,
					S.GetLength()+1,
					avh->info, // flags.... 
					NULL, NULL);
				}
			m_TV->vFrameNum4Save++;
skipSave:	;
			}

		if(v && pSBuf) {
			avh->reserved1=avh->reserved2=0;
// USARE						measureAudio((BYTE *)wh->lpData,wh->dwBufferLength); per PTT mode!
			v->PostMessage(WM_VIDEOFRAME_READY,(WPARAM)pSBuf,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
			}

fine: ;
		}
	else {
		if(theApp.debugMode) {
			char *p=(char *)GlobalAlloc(GPTR,1024);
			wsprintf(p,"Non mando video!"); 
			theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
			}
		}

	if(pInpAllocated)
		HeapFree(GetProcessHeap(),0,pInp);
	return i;
	}



#endif
