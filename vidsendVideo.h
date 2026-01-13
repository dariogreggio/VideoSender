#ifndef _VIDSENDVIDEO_INCLUDED
#define _VIDSENDVIDEO_INCLUDED



#include <vfw.h>
#include "vidsenddshow.h"

struct EXT_WAVEFORMATEX {
	WAVEFORMATEX wf;
	BYTE extra[64];
	};
//2018: v. anche WAVE_FORMAT_EXTENSIBLE, https://docs.microsoft.com/en-us/previous-versions/windows/desktop/ee419020%28v%3dvs.85%29

struct COMPRESSION_TYPES {
	DWORD fourCC;
	WORD bitsPerPixel;
	};

#define RGBConvert2416(n)    (DWORD) ((((n) & 0xF8) >> 3) | (((n) & 0xFC00) >> 5) | (((n) & 0xf80000) >> 8))

#define QUALITYBOX_CHECK 8
class CTV {
	enum {
		THE_FIRST_CAPTURE=0,			// ci saranno max 10 periferiche di cattura
		THE_LAST_CAPTURE=9,
		FREQUENCY=11025
		};
	enum {
		oneKPerSecond=0,
		oneKEveryTwo=1,
		oneKEveryFive=2,
		allK=3
		};
public:
	CTV(CWnd *);
	CTV(CWnd *, BOOL canOverlay, const BITMAPINFOHEADER *biRawDef=NULL, DWORD fps=5, DWORD kFrame=CTV::oneKPerSecond, 
		DWORD preferredDriver=0, BOOL bAudio=1, const WAVEFORMATEX *preferredWf=NULL, BOOL bVerbose=TRUE);
	CTV(CWnd *, BOOL canOverlay, const SIZE *size, WORD bpp, DWORD fps=5, DWORD preferredFormat=0, DWORD preferredDriver=0, 
		DWORD kFrame=CTV::oneKPerSecond, BOOL bAudio=1, const WAVEFORMATEX *preferredWf=NULL, BOOL bVerbose=TRUE);
	void preConstruct(CWnd *pWnd=NULL);
	~CTV();
	int initCapture(BOOL, const BITMAPINFOHEADER *, DWORD , DWORD , DWORD, BOOL, const WAVEFORMATEX *, BOOL bVerbose);
	int setSuperImposeDateTime(int m) { if(biRawBitmap.biBitCount==24) { imposeTime=m; return 1; } else return 0;};
	int setSuperImposeText(const char *s,int m) { _tcsncpy(imposeText,s,31); imposeText[31]=0; imposeTextPos=m; return 1; };
	int startSaveFile(CString,DWORD);
	int endSaveFile();
	RECT *Resize(RECT *, BOOL force=FALSE);
	DWORD GetXSize() { return biRawBitmap.biWidth; }
	DWORD GetYSize() { return biRawBitmap.biHeight; }
	HWND GetHwnd() { return m_hWnd; }
	CWnd *GetParent() { return m_Parent; }
	int Capture(int);
	BOOL GetDriverDescription(WORD , LPSTR , INT , LPSTR , INT );
#ifndef USA_DIRECTX
	BOOL hasOverlay() { return captureCaps.fHasOverlay; }
#endif
	BOOL isRecordingVideo() { return aviFile != NULL;}
	/*static */ int superImposeDateTime(const LPBITMAPINFOHEADER,BYTE *,COLORREF dColor=RGB(255,255,255));
	/*static */ int superImposeText(const LPBITMAPINFOHEADER,BYTE *,const char *s,COLORREF dColor=RGB(255,255,255));
	static int superImposeText(const CBitmap *b,const char *s,COLORREF dColor=RGB(255,255,255),int flag=0);
	static int superImposeBox(const LPBITMAPINFOHEADER,BYTE *,const RECT *,COLORREF dColor=RGB(0,255,0));
	static int convertColorBitmapToBN(const LPBITMAPINFOHEADER,BYTE *);
	static int mirrorBitmap(const LPBITMAPINFOHEADER,BYTE *);
	static int flipBitmap(const LPBITMAPINFOHEADER,BYTE *);
	static int resampleBitmap(const LPBITMAPINFOHEADER,BYTE *,RECT *);
	int checkQualityBox(const LPBITMAPINFOHEADER,const BYTE *,RECT *,LPDWORD retLuma=NULL,DWORD retChroma[]=NULL,DWORD ***myOldCells=NULL);
	int setOpzioni(DWORD n) { opzioniSave=n; return 1; };
	int compressAFrame(LPVIDEOHDR,int doPreview,CWnd *w,HDRAWDIB hdd,LPBITMAPINFOHEADER bmih);

protected:
	BOOL setAudioFormat(WAVEFORMATEX *wfex) { return m_hWnd ? capSetAudioFormat(m_hWnd,wfex,sizeof(WAVEFORMATEX)) : -1; }
	BOOL getAudioFormat(WAVEFORMATEX *wfex) { return m_hWnd ? capGetAudioFormat(m_hWnd,wfex,sizeof(WAVEFORMATEX)) : -1; }
#ifndef USA_DIRECTX
	BOOL setCallbackOnVideoStream(LRESULT (CALLBACK *fproc)(HWND, LPVIDEOHDR )=NULL) { return capSetCallbackOnVideoStream(hWnd,fproc); }
	BOOL setCallbackOnWaveStream(LRESULT (CALLBACK *fproc)(HWND, LPWAVEHDR )=NULL) { return capSetCallbackOnWaveStream(hWnd,fproc); }
	BOOL setVideoFormat(BITMAPINFOHEADER *bi) {	return hWnd ? capSetVideoFormat(hWnd,bi,sizeof(BITMAPINFOHEADER)) : -1; }
	BOOL getVideoFormat(BITMAPINFOHEADER *bi) {	return hWnd ? capGetVideoFormat(hWnd,bi,sizeof(BITMAPINFOHEADER)) : -1; }
	BOOL driverDisconnect() { return hWnd ? capDriverDisconnect(hWnd) : -1; }; 
	BOOL driverConnect() { return hWnd ? capDriverConnect(hWnd,theCapture) : -1; }; 
	BOOL preview(BOOL m) { return hWnd ? capPreview(hWnd, m) : -1; }
	BOOL overlay(BOOL m) { if(hWnd) { if(hasOverlay()) return capOverlay(hWnd,m); else { capPreviewRate(hWnd,(DWORD) (1.0e3 /*/ framesPerSec*/)); capPreviewScale(hWnd,1); return capPreview(hWnd, m);	} } else return -1; }
	BOOL setCallbackOnFrame(LRESULT (CALLBACK *fproc)(HWND, LPVIDEOHDR )=NULL) { return capSetCallbackOnFrame(hWnd,fproc); }
	BOOL setCallbackOnError(LRESULT (CALLBACK *fproc)(HWND, int, LPSTR )=NULL) { return capSetCallbackOnError(hWnd,fproc); }
	BOOL setCallbackOnStatus(LRESULT (CALLBACK *fproc)(HWND, int, LPSTR )=NULL) { return capSetCallbackOnStatus(hWnd,fproc); }
	BOOL captureStop() { return hWnd ? capCaptureStop(hWnd) : -1; }
	BOOL captureSequenceNoFile() { return hWnd ? capCaptureSequenceNoFile(hWnd) : -1; }
	BOOL getStatus() { return hWnd ? capGetStatus(hWnd,&captureStatus,sizeof(CAPSTATUS)) : -1; }
#else
	BOOL preview(BOOL m) { if(m_DShow) { m_DShow->SetPreview(m); return m; } else return -1; }
#endif

public:
	static const COMPRESSION_TYPES acceptedCompressionType[];
	HACMSTREAM m_hAcm;
	HWAVEIN m_hWaveIn;
	WAVEHDR IWaveHdr1,IWaveHdr2;
	HIC m_hICCo,m_hICDe /*per decomprimere in RGB telecamere che non lo danno*/;
	DWORD framesPerSec,KFrame;
#ifndef USA_DIRECTX
	CAPTUREPARMS captureParms;
	CAPDRIVERCAPS captureCaps;
	CAPSTATUS captureStatus;
#endif
	COMPVARS cv;
	DWORD maxFrameSize;
	DWORD compressor;
	DWORD theCapture;
	int wInput;		// whichInput scheda Video
	BITMAPINFOHEADER biRawBitmap,biBaseRawBitmap;
	BITMAPINFO biRawDef,biCompDef;
	WAVEFORMATEX wfex;
	struct EXT_WAVEFORMATEX wfd;
	DWORD maxWaveoutSize;
	DWORD oldTimeCaptured;	// usato nella callback video per patch-are i fps
	BOOL inCapture;
	BYTE *theFrame;		// ultimo frame catturato, per controllo movimento e HTML/Webcam
	BYTE *m_AudioBuffer1,*m_AudioBuffer2;

protected:
	CWnd *m_Parent;
	PAVIFILE aviFile;
	PAVISTREAM psVideo, psAudio, psText;
	BOOL allowOverlay,saveVideo;
	int imposeTime,imposeTextPos;
	char imposeText[32];
	DWORD opzioniSave;
	HWND m_hWnd;
	DWORD vFrameNum,aFrameNum,vFrameNum4Save,aFrameNum4Save,saveWait4KeyFrame;
	DShowVideoCapture *m_DShow;

	friend LRESULT CALLBACK WaveCallbackProc(HWND, LPWAVEHDR);
#ifndef USA_DIRECTX
	friend LRESULT CALLBACK StatusCallbackProc(HWND, int, LPSTR);
	friend LRESULT CALLBACK ErrorCallbackProc(HWND, int, LPSTR);
	friend LRESULT CALLBACK FrameCallbackProc(HWND, LPVIDEOHDR);
	friend LRESULT CALLBACK VideoCallbackProc(HWND, LPVIDEOHDR);
#else
	friend LRESULT CALLBACK VideoCallback(HWND , UINT , WPARAM , LPARAM );
#endif
	friend class CVidsendView2;
	friend class CVideoSrcDialog;
	};


#endif