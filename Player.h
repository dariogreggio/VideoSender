#ifndef _PLAYER_INCLUDED
#define _PLAYER_INCLUDED


#undef  MMNOWAVE        

#include <windows.h>
#include <mmsystem.h>

//#define INITGUID 
//#include <objbase.h>
//#include <initguid.h>

#include <dsound.h>
#include <vector>
#include "IPlayer.h"

// DirectX 7.0 compatibility
#ifndef DSBCAPS_CTRLDEFAULT
#define DSBCAPS_CTRLDEFAULT (DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME )
#endif



// Throw a first exception, without parent exception
//#define EXCEP(/*const wchar_t * */ desc, /*const wchar_t * */ from) throw( MATExceptions(__LINE__,  _T(__FILE__), 0, from, (LPCTSTR)desc) );
//cazzo di multibyte!
#define EXCEP(/*const wchar_t * */ desc, /*const wchar_t * */ from) AfxMessageBox(from);

class CVidsendView;
class CVidsendView22;
class CVidsendApp;
//#pragma comment(lib, "dsound.lib")
//#pragma comment(lib, "dxguid.lib")

class Player : public IPlayer {
public:
	HANDLE *m_pReadEvent;
private :
	SOUNDFORMAT m_format;						// Format for a sound buffer, as describe in IPlayer.h

	LPDIRECTSOUNDBUFFER	m_lpDSBuffer;		// DirectSound buffer
	LPDIRECTSOUND m_lpDS;					// DirectSound object
	HWND m_hWnd;							// Window handler

	HANDLE *m_phEvents;
	HANDLE m_hNotifyEndThread;
	LPDIRECTSOUNDNOTIFY m_lpdsNotify;
	DSBPOSITIONNOTIFY *m_pDSNotify;

	int m_nbReadEvent;

	/*IPlayer::SoundEventListener*/ CVidsendView *objectListener;
	/*IPlayer::SoundEventListener*/ CVidsendView22 *objectListener2;
		

public :
	Player(HWND p_hWnd=NULL);
	~Player();
		
	virtual void Init(LPGUID q=NULL);
	virtual BYTE Init(int q=0);
	void SetHWnd(HWND p_hWnd);

	virtual void Play(DWORD p_flag);
	virtual void Stop();	
	virtual void SetPosition(long pos);
	virtual void SetVolume(DWORD, DWORD level2=-1,WORD dst=0);
	virtual DWORD GetPosition();
	virtual bool CreateSoundBuffer(SOUNDFORMAT format, long bufferLength, DWORD flags=DSBCAPS_CTRLDEFAULT);
	virtual void Write(long start, const BYTE *data, long size);
	virtual bool CreateEventReadNotification(std::vector<DWORD>&p_event);	
	virtual void SetSoundEventListener(/*IPlayer::SoundEventListener*/ CVidsendView *p_listener);
	virtual void SetSoundEventListener(/*IPlayer::SoundEventListener*/ CVidsendView22 *p_listener);
	virtual void RemoveSoundEventListener();
	BOOL SetNotificationPositions(DWORD,SIZE_T);
	BOOL SetNotificationPositions(DWORD dwOffsets[], SIZE_T );
	BOOL IsPlaying();
	static BOOL CALLBACK DSEnumCallback(LPGUID lpGuid,LPCSTR lpcstrDescription,
         LPCSTR lpcstrModule,LPVOID lpContext);

private :	
	class Mutex *m_mutex;

	void CallEvent(int eventNumber);
	void WaitNotif();

	static void WaitForNotify(Player *p_player) ;

	};


class CAudioMixer {
public:
	HMIXER hMixer;
	DWORD m_TotMixers;
	MIXERCAPS m_mxcaps;
	IAMAudioInputMixer *m_AMMixer;
//	LPAUDIOINPUTMIXER m_AMMixer;
	LPDIRECTSOUNDCAPTURE m_lpDSC;					// DirectSound object
	LPDIRECTSOUNDCAPTUREBUFFER m_lpDSCBuffer;		// DirectSound buffer

protected:
	const CWnd *theWnd;

public:
	int SetVolume(DWORD, DWORD, DWORD level2=-1,WORD dst=0);
	DWORD GetVolume(DWORD );
	int Switch(DWORD, BOOL);
	int Choose(DWORD);

	CAudioMixer(const CWnd *);
	~CAudioMixer();
	};


class /*AFX_EXT_CLASS*/ CWave : public CObject {
	// Public Constructor(s)/Destructor
public:
	CWave();
	CWave(LPCTSTR lpszFileName);
	CWave(UINT uiResID, HMODULE hmod = AfxGetInstanceHandle());
	virtual ~CWave();

	// Public Methods
public:
	BOOL    Create(LPCTSTR lpszFileName);
	BOOL    Create(UINT uiResID, HMODULE hmod =	AfxGetInstanceHandle());
	BOOL    IsValid() const { return (m_pImageData ? TRUE :	FALSE); };
	BOOL    Play(BOOL bAsync = TRUE, BOOL bLooped = FALSE) const;
	BOOL    GetFormat(WAVEFORMATEX& wfFormat) const;
	DWORD   GetDataLen() const;
	DWORD   GetSeconds() const;
	static DWORD GetSeconds(DWORD,WAVEFORMATEX *);
	DWORD   GetData(BYTE*& pWaveData, DWORD dwMaxToCopy) const;

	// Protected Methods
protected:
	BOOL    Free();

	// Private Data
private:
	BYTE* m_pImageData;
	DWORD m_dwImageLen;
	BOOL  m_bResource;
	};

// For our convenience
//#define DSBCAPS_REGULAR (DSBCAPS_CTRLDEFAULT | DSBCAPS_STATIC | DSBCAPS_LOCSOFTWARE)
// meglio questa... se no se perdi Focus non senti pi!
#define DSBCAPS_REGULAR (DSBCAPS_CTRLDEFAULT  /* inutili e contraddittori! DSBCAPS_STATIC | DSBCAPS_LOCSOFTWARE | */ /* 2023 DSBCAPS_GLOBALFOCUS*/)
// DSBCAPS_CTRL3D 
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/directx9_c/dsbufferdesc.asp

#define DSOUND_ERR -1;

/////////////////////////// Macros //////////////////////////

#define DSVOLUME_TO_DB(volume) ((DWORD)(-30 * (100 - volume)))

class CDSound {
public:
	CComPtr<IDirectSound> m_pDirectSound;
public:
	CDSound();
	~CDSound();
	void Destroy() { m_pDirectSound = NULL; }

	BOOL Create(BYTE q=0);
	BOOL SetCooperativeLevel(HWND hWnd, DWORD dwLevel = DSSCL_NORMAL);

	operator LPDIRECTSOUND () { return m_pDirectSound; }
	LPDIRECTSOUND operator -> () { return m_pDirectSound; }
	};

class CDSBuffer {
public:
	CComPtr<IDirectSoundBuffer> m_pDSBuffer;
	CComPtr<IDirectSound> m_pDirectSound;
	CComPtr<IDirectSoundNotify> m_pDSNotify;
	UINT m_uIDRcsrWave;
	DWORD m_dwFlags; // Create flags
	DWORD m_dwImageLen;
public:
	CDSBuffer();
	~CDSBuffer();
	void Destroy() {
		m_pDirectSound = NULL;
		m_pDSBuffer = NULL;
		m_pDSNotify = NULL;
		}

	BOOL Create(IDirectSound *pDirectSound, const CWave &wave, DWORD dwFlags);
	BOOL Create(IDirectSound *pDirectSound, UINT uIDRscrWave, DWORD dwFlags, HMODULE hMod = AfxGetInstanceHandle());
	BOOL Create(IDirectSound *pDirectSound, LPCTSTR lpszFileName, DWORD dwFlags);
	BOOL Create(IDirectSound *pDirectSound, LPCTSTR lpszFileName, CWave &wave, DWORD dwFlags);
	BOOL Create(IDirectSound *pDirectSound, const BYTE *pBuf, DWORD dwDataLen, WAVEFORMATEX wfFormat, DWORD dwFlags);
	BOOL Restore();

	LPDIRECTSOUNDNOTIFY GetDSNotify() { return m_pDSNotify; }

	LPDIRECTSOUNDBUFFER operator -> () { return m_pDSBuffer; }
	LPDIRECTSOUNDBUFFER GetBuffer() { return m_pDSBuffer; }
	};

//----------------------------------------- CDSoundPlay
//This class represents an high level interface for sound playing.
class CDSoundPlay {
public:
	CDSound *m_pDirectSound; // points to an outer object
	UINT m_uIDRsrcWave;
	mutable CDSBuffer m_DSBuffer;

public:
	// - CONSTRUCTION MANAGEMENT -
	CDSoundPlay();
	~CDSoundPlay();
	void Destroy();
	BOOL Create(CDSound *pDSound, UINT uIDRsrcWave, DWORD dwFlags = DSBCAPS_REGULAR, HMODULE hMod = AfxGetInstanceHandle());
	BOOL Create(CDSound *pDSound, LPCTSTR lpszFileName, DWORD dwFlags = DSBCAPS_REGULAR);
	BOOL Create(CDSound *pDSound, LPCTSTR lpszFileName, CWave &wave, DWORD dwFlags = DSBCAPS_REGULAR);
	BOOL Create(CDSound *pDSound, const BYTE *, DWORD len=22050, WAVEFORMATEX *wf=NULL, DWORD dwFlags = DSBCAPS_REGULAR);


	// - PLAY MANAGEMENT -
	// returns current play position
	DWORD GetCurrentPosition(LPDWORD lpdwCurrentWriteCursor) const;
	BOOL Play(BOOL bLooping = FALSE); // or - !DSBPLAY_LOOPING
	BOOL SetCurrentPosition(DWORD dwNewPosition);
	BOOL Stop();


	// - SOUND MANAGEMENT -
	DWORD GetFrequency() const;
	LONG GetPan() const;
	LONG GetVolume() const;

	// New frequency, in hertz (Hz), at which to play the audio samples. 
	// The value must be in the range DSBFREQUENCY_MIN to DSBFREQUENCY_MAX
	BOOL SetFrequency(DWORD dwFrequancy);

	// The value in lPan is measured in hundredths of a decibel (dB),
	//	 in the range of DSBPAN_LEFT to DSBPAN_RIGHT
	BOOL SetPan(LONG lPan);

	// The volume is specified in hundredths of decibels (dB). 
	// Allowable values are between DSBVOLUME_MAX (no attenuation)
	// and DSBVOLUME_MIN (silence). 
	BOOL SetVolume(LONG lVolume);

	
	// - STATUS MANAGEMENT -
	// Returns a combination of the following flags.
	// DSBSTATUS_BUFFERLOST,	DSBSTATUS_LOOPING,		DSBSTATUS_PLAYING, 
	// DSBSTATUS_LOCSOFTWARE,	DSBSTATUS_LOCHARDWARE,	DSBSTATUS_TERMINATED
	DWORD GetStatus() const;
	CDSBuffer GetBuffer() { return m_DSBuffer; }
	

	// The following are a few separate variations of GetStatus.
	BOOL IsPlaying() const { return DSBSTATUS_PLAYING == (DSBSTATUS_PLAYING & GetStatus()); } // FALSE means STOPPED
	BOOL IsLooping() const { return DSBSTATUS_LOOPING == (DSBSTATUS_LOOPING & GetStatus()); }
	BOOL IsBufferLost() const { return DSBSTATUS_BUFFERLOST == (DSBSTATUS_BUFFERLOST & GetStatus()); }

	// Restores the memory allocation for a lost sound buffer.
	BOOL Restore() { return m_DSBuffer.Restore(); }


	// - NOTIFICATION MANAGEMENT -
	// Typedefs a pointer to callback function
	typedef LONG (*PNOTIFYFUNC) (void *,LONG,LONG);

	void SetNotifyFunction(PNOTIFYFUNC pfn,DWORD q) { 
		ASSERT(pfn || !pfn && !m_pThread); // No thread should be running when pfn is NULL
		m_pFunc = pfn; 
		m_pFuncParm =	q; 
		}

	// Returns FALSE if called during playback or in case of invalid 
	// parameter or lack of memory. If dwOffsets is NULL, the waiting
	// thread (and thus notifications if any) will be canceled.
	BOOL SetNotificationPositions(DWORD dwOffsets[], SIZE_T nSize);
	BOOL SetAutoNotificationPositions(SIZE_T);

private:
	// Don't bother with these stuff.
	PNOTIFYFUNC m_pFunc;
	DWORD m_pFuncParm;
	HANDLE *m_phEvents;
	HANDLE m_hEventThreadStop;
	typedef struct _INFO {
		HANDLE *hEvents;
		DWORD *dwOffsets;
		SIZE_T nSize;
		PNOTIFYFUNC pFunc;
		DWORD pFuncParm;
		} *PINFO;
	CWinThread *m_pThread;
	static UINT _ThreadProc(PVOID pParam);
	};

// } // namespace DirectSound


/////////////////////////////////////////////////////////////////////////////
// CWaveIn thread
class CWaveIn : public CWinThread {
	DECLARE_DYNCREATE(CWaveIn)
public:
	CWaveIn(CWnd *w=NULL);           // protected constructor used by dynamic creation
	CWaveIn(CWnd *w,WAVEFORMATEX *wf=NULL);
	enum {
		BUF_LEN=44100L*2*2/AUDIO_BUFFER_DIVIDER
		};

// Attributes
public:
	BOOL  m_oneExe;
	DWORD m_dwThread,tag;
	BOOL m_bRecording;
	MMRESULT m_ret;

	HWAVEIN m_hwi;

	WAVEHDR m_wh1;
	WAVEHDR m_wh2;

	WAVEFORMATEX m_wfx;

	BYTE *m_buf1;
	BYTE *m_buf2;

// Operations
public:
	BOOL StartRecord();
	virtual ~CWaveIn();
	BOOL StopRecord();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWaveIn)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CWaveIn)
		// NOTE - the ClassWizard will add and remove member functions here.
	afx_msg void On_WIM_OPEN(UINT wParam, LONG lParam);
	afx_msg void On_WIM_DATA(UINT wParam, LONG lParam);
	afx_msg void On_WIM_CLOSE(UINT wParam, LONG lParam);
	afx_msg void OnWaveInEnd(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

#endif	

