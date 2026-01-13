#include "vidsendSockets.h"

#define USA_DIRECTX

#define DIRECT3D_VERSION         0x0800
#define DIRECTINPUT_VERSION      0x0800
// necessario 8.0 per AboutDialog con bitmap girevole!

#include "vidsendVideo.h"
#include <ras.h>

// #define _NEWMEET_MODE messo nei SETTINGS dei progetti... per le risorse!

#if !defined(AFX_VIDSEND2_H__242A4EA4_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
#define AFX_VIDSEND2_H__242A4EA4_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

#define WM_UPDATE_PANE WM_APP+5
#define WM_MP3_FINISHED WM_APP+6
#define WM_CLOSE_CHILD WM_APP+1
#define WM_VIDEOFRAME_READY WM_APP+7
#define WM_AUDIOFRAME_READY WM_APP+8
#define WM_RAWAUDIOFRAME_READY WM_APP+9			// per VBAN 2021
#define WM_WAVEINEND WM_APP+11			// per WAVEIN thread

#define MAX_FTP_A_DAY 100
#define AUTHSOCK_TIMEOUT 120000
#define VIDSEND_VERSIONE MAKEWORD(1,1)			// versione dei pacchetti video...

#define AUDIO_BUFFER_DIVIDER 10


class CVidsendDoc;
class CVidsendDoc2;
class CVidsendDoc22;
class CVidsendDoc3;
class CVidsendDoc4;
class CVidsendDoc5;
class CVidsendDoc6;
class CVidsendDoc7;

class CVidsendSet2;
class CVidsendSet2_;
class CVidsendSet3;
class CVidsendSet4;
class CVidsendSet5;
class CVidsendSet5Ex;
class CVidsendSet8;

class CCommandLineInfoEx : public CCommandLineInfo {

public:
	BOOL m_restoreOptions;
	BOOL m_openVideoServer,m_openChatServer;
	int m_debugMode;
	CString m_ODBCname;
public:
	void ParseParam(LPCTSTR , BOOL , BOOL );
  CCommandLineInfoEx();
  
	};


class CLogFile;


#define _CPRIVATEPROFILE_USEINI 1

class CProfileStore {

public:
	static char *emptyString;
private:
	HINSTANCE m_hInstance;
	CString regRoot;
	char *variabiliKey;
public:
#ifdef _CPRIVATEPROFILE_USEINI
	int WritePrivateProfileInt(char *, char *, int, const char *);
#endif
	int WritePrivateProfileString(const char *,const char *,const char *);
	int WritePrivateProfileString(const char *s, int k, const char *v=NULL) { char ks[32]; LoadString(m_hInstance,k,ks,32); return WritePrivateProfileString(s,ks,v);};
	int WritePrivateProfileInt(const char *, const char *, int);
	int WritePrivateProfileInt(const char *s, int k, int v)  { char ks[32]; LoadString(m_hInstance,k,ks,32); return WritePrivateProfileInt(s,ks,v);};
	int WritePrivateProfileDouble(const char *, const char *, double);
	int WritePrivateProfileDouble(const char *s, int k, double v)  { char ks[32]; LoadString(m_hInstance,k,ks,32); return WritePrivateProfileInt(s,ks,v);};
	int GetPrivateProfileInt(const char *s, const char *k, int def=0);
	int GetPrivateProfileInt(const char *s, int k, int def=0) { char ks[32]; LoadString(m_hInstance,k,ks,32); return GetPrivateProfileInt(s,ks,def);};
	int GetPrivateProfileString(const char *s, const char *k, char *v, int len, const char *def=emptyString);
	int GetPrivateProfileString(const char *s, int k, char *v, int len, const char *def=emptyString) { char ks[32]; LoadString(m_hInstance,k,ks,32); return GetPrivateProfileString(s,ks,v,len,def);};
	double GetPrivateProfileDouble(const char *s, const char *k, int def=0);
	double GetPrivateProfileDouble(const char *s, int k, int def=0) { char ks[32]; LoadString(m_hInstance,k,ks,32); return GetPrivateProfileInt(s,ks,def);};

	CTime GetPrivateProfileTime(const char *,const char *);
	CTime GetPrivateProfileTime(const char *s,int k) { char ks[32]; LoadString(m_hInstance,k,ks,32); return GetPrivateProfileTime(s,ks); }
	CTimeSpan GetPrivateProfileTimeSpan(const char *,char *);
	CTimeSpan GetPrivateProfileTimeSpan(const char *s,int k) { char ks[32]; LoadString(m_hInstance,k,ks,32); return GetPrivateProfileTimeSpan(s,ks); }
	int WritePrivateProfileTime(const char *, const char *, CTime );
	int WritePrivateProfileTime(const char *s, int k, CTime t) { char ks[32]; LoadString(m_hInstance,k,ks,32); return WritePrivateProfileTime(s,ks,t); }
	int WritePrivateProfileTime(const char *, const char *, CTimeSpan );
	int WritePrivateProfileTime(const char *s, int k, CTimeSpan t) { char ks[32]; LoadString(m_hInstance,k,ks,32); return WritePrivateProfileTime(s,ks,t); }

	int WriteProfileVariabileInt(int k, int v)  { return WritePrivateProfileInt(variabiliKey,k,v);};
	int GetProfileVariabileInt(int k) { return GetPrivateProfileInt(variabiliKey,k);};
	int WriteProfileVariabileString(int k, const char *v)  { return WritePrivateProfileString(variabiliKey,k,v);};
	int GetProfileVariabileString(int k,char *v,int len,const char *def=emptyString) { return GetPrivateProfileString(variabiliKey,k,v,len,def);};
	int FlushProfile();
  CProfileStore(HINSTANCE,const char *,const char *);

	CString getProfileRoot() const { return regRoot; };
	CString getVariabiliKey() const;
//private:
	CString getProfileKey(CString s=emptyString) const;

	};


class CChecksum {
public:
	CChecksum(WORD width,DWORD polynomial,DWORD iv=0,BOOL rIn=FALSE,BOOL rOut=FALSE);
	~CChecksum();
	DWORD __fastcall GetChecksum(BYTE const message[], DWORD );
	DWORD getCrcTable(int n) { return crcTable[n]; }
	static DWORD __fastcall reflect(DWORD,int);
protected:
	DWORD *crcTable;
	WORD width;
	DWORD topbit,widmask;
	DWORD polynomial;		//0x85 @8bits (PicFruit); 0x8005 @16bits (CRC-16); 0x04C11DB7 @32bits(Ethernet, PKZIP)
	DWORD initValue;
	BOOL reflectInput,reflectOutput;
	DWORD xorOutput;

	};

class CRiffList {
private:
	enum {
		bufsize=2048
		};
	BYTE *buf;
	int ptr;
public:
	CRiffList(DWORD m4cc=0);
	~CRiffList();
	BYTE *GetContent();
	int GetContentLength();
	int Add(DWORD,const BYTE *,WORD);
	int Add(DWORD,const char *);
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendApp:
// See vidsend2.cpp for the implementation of this class
//

#pragma pack( push, before_streaminfo )
#pragma pack(1)

// NOTARE CTime/Span diventa 64 bit nelle nuove versioni!!!
// https://docs.microsoft.com/en-us/cpp/atl-mfc-shared/reference/ctime-class?view=vs-2017
struct STREAM_INFO {
	WORD versione;
	BITMAPINFOHEADER bm;		//40
	DWORD fps;
	WORD quality;
	EXT_WAVEFORMATEX wf;		//82
	BOOL noBuffers;			// il server dice "non bufferizzare" perche' la trasmissione non e' continua.
	CTimeSpan /*DWORD*/ maxTime;
	char authenticationWWW[128];
	DWORD IDServer;			// il numero ID dell'utente che fa da server, all'interno del database utenti
	BYTE dontSave;
	BYTE remoteCtrl;
	char openWWW[128];			// per aprire un WEBsite in parallelo alla connessione... tanto per gradire!
	char splashOrIntro[128];			// Schermata o ID risorsa o animazione PRIMA della partenza del video
	char streamTitle[64];			// Titolo della trasmissione
	};		//591->594 bytes, 30.12.18

/*struct CONTROL_INFO {		// da' errore in compilazione! togliere CTimespan sopra se serve questa...
	BYTE tipo;
	union {
		struct STREAM_INFO s;
		char myBuf[256];
		};
	};*/

struct CHAT_INFO {
	WORD versione;
	char authenticationWWW[128];
	BYTE dontSave;
	CTimeSpan maxTime;
	char openWWW[128];			// per aprire un WEBsite in parallelo alla connessione... tanto per gradire!
	};

struct CHAT_MESSAGE {
	BYTE id;
	BYTE extra;
	BYTE room;
	BYTE extra2;
	COLORREF color;
	char sender[32];
	char message[200];
	};

struct CHAT_ROOMS_INFO {
	DWORD numTot;
	char names[252];
	};

struct LOGGED_USER_INFO {
	int dontSave:1;
	int remoteCtrl:2;
	int timedConn:1;
	char nome[32];
	CTime loginTime;
	};

struct INFO_UTENTE {
	char nome[32];
	char cognome[32];
	char indirizzo[80];
	char citta[32];
	char email[80];
	char note[256];
	char login[20];
	char pasw[20];
	};

#pragma pack( pop, before_streaminfo )

class CFileSizeString : public CString {
private:
	CString InsertSeparator(DWORD );
public:
	CString FormatSize(DWORD );
	};

class CRTSPClientSocket;

class CWinAppEx : public CWinApp {

	DECLARE_DYNAMIC(CWinAppEx)
public:
	friend class CMainFrame;

	CWinAppEx(BOOL bResourceSmartUpdate = TRUE);
	virtual ~CWinAppEx();

	virtual int ExitInstance();

	LPCTSTR SetRegistryBase(LPCTSTR lpszSectionName = NULL);
	LPCTSTR	GetRegistryBase() { return m_strRegSection; }

	// Saved data version:
	int GetDataVersionMajor() const { return m_iSavedVersionMajor; }
	int GetDataVersionMinor() const { return m_iSavedVersionMinor; }
	int GetDataVersion() const;

	BOOL InitMouseManager();
	BOOL InitContextMenuManager();
	BOOL InitKeyboardManager();
	BOOL InitShellManager();
	BOOL InitTooltipManager();


	BOOL IsResourceSmartUpdate() const { return m_bResourceSmartUpdate; }
	void EnableLoadWindowPlacement(BOOL bEnable = TRUE) { m_bLoadWindowPlacement = bEnable; }


	// Call one of these in CMyApp::InitInstance just after ProcessShellCommand() and before pMainFrame->ShowWindow().
	BOOL LoadState(CMDIFrameWnd* pFrame, LPCTSTR lpszSectionName = NULL);
	BOOL LoadState(CFrameWnd* pFrame, LPCTSTR lpszSectionName = NULL);

	virtual BOOL CleanState(LPCTSTR lpszSectionName = NULL);

	BOOL SaveState(CMDIFrameWnd* pFrame, LPCTSTR lpszSectionName = NULL);
	BOOL SaveState(CFrameWnd* pFrame, LPCTSTR lpszSectionName = NULL);

	BOOL IsStateExists(LPCTSTR lpszSectionName /*=NULL*/);

	virtual BOOL OnViewDoubleClick(CWnd* pWnd, int iViewId);
	virtual BOOL ShowPopupMenu(UINT uiMenuResId, const CPoint& point, CWnd* pWnd);

	CString GetRegSectionPath(LPCTSTR szSectionAdd = _T(""));

	// These functions load and store values from the "Custom" subkey
	// To use subkeys of the "Custom" subkey use GetSectionInt() etc. instead
	int GetInt(LPCTSTR lpszEntry, int nDefault = 0);
	CString GetString(LPCTSTR lpszEntry, LPCTSTR lpszDefault = _T(""));
	BOOL GetBinary(LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes);
	BOOL GetObject(LPCTSTR lpszEntry, CObject& obj);
	BOOL WriteInt(LPCTSTR lpszEntry, int nValue );
	BOOL WriteString(LPCTSTR lpszEntry, LPCTSTR lpszValue );
	BOOL WriteBinary(LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes);
	BOOL WriteObject(LPCTSTR lpszEntry, CObject& obj);

	// These functions load and store values from a given subkey
	// of the "Custom" subkey. For simpler access you may use GetInt() etc.
	int GetSectionInt( LPCTSTR lpszSubSection, LPCTSTR lpszEntry, int nDefault = 0);
	CString GetSectionString( LPCTSTR lpszSubSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault = _T(""));
	BOOL GetSectionBinary(LPCTSTR lpszSubSection, LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes);
	BOOL GetSectionObject(LPCTSTR lpszSubSection, LPCTSTR lpszEntry, CObject& obj);
	BOOL WriteSectionInt( LPCTSTR lpszSubSection, LPCTSTR lpszEntry, int nValue );
	BOOL WriteSectionString( LPCTSTR lpszSubSection, LPCTSTR lpszEntry, LPCTSTR lpszValue );
	BOOL WriteSectionBinary(LPCTSTR lpszSubSection, LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes);
	BOOL WriteSectionObject(LPCTSTR lpszSubSection, LPCTSTR lpszEntry, CObject& obj);

	void SetTitle();
  BOOL FirstInstance(int nCmdShow=SW_SHOW);

	// WinHelp override:
	virtual void OnAppContextHelp(CWnd* pWndControl, const DWORD dwHelpIDArray []);

	// Idle processing override:
	virtual BOOL OnWorkspaceIdle(CWnd* /*pWnd*/) { return FALSE; }

public:
	BOOL m_bLoadUserToolbars;
	CProfileStore *prStore;

protected:

	// Overidables for customization
	virtual void OnClosingMainFrame();
	
	virtual void PreLoadState() {}    // called before anything is loaded
	virtual void LoadCustomState() {} // called after everything is loaded
	virtual void PreSaveState() {}    // called before anything is saved
	virtual void SaveCustomState() {} // called after everything is saved

	virtual BOOL LoadWindowPlacement(CRect& rectNormalPosition, int& nFflags, int& nShowCmd);
	virtual BOOL StoreWindowPlacement(const CRect& rectNormalPosition, int nFflags, int nShowCmd);
	virtual BOOL ReloadWindowPlacement(CFrameWnd* pFrame);

protected:
	CString m_strRegSection;
  BOOL bClassRegistered;
	BOOL AppInited;

	BOOL m_bKeyboardManagerAutocreated;
	BOOL m_bContextMenuManagerAutocreated;
	BOOL m_bMouseManagerAutocreated;
	BOOL m_bUserToolsManagerAutoCreated;
	BOOL m_bTearOffManagerAutoCreated;
	BOOL m_bShellManagerAutocreated;
	BOOL m_bTooltipManagerAutocreated;
	BOOL m_bForceDockStateLoad; // Load dock bars state even it's not valid
	BOOL m_bLoadSaveFrameBarsOnly;
	BOOL m_bSaveState;          // Automatically save state when the main frame is closed.
	BOOL m_bForceImageReset;    // Force image reset every time when the frame is loaded
	BOOL m_bLoadWindowPlacement;

	const BOOL m_bResourceSmartUpdate; // Automatic toolbars/menu resource update

	int m_iSavedVersionMajor;
	int m_iSavedVersionMinor;
	};


class CVidsendApp : public CWinAppEx {
public:
	enum {
		canSendVideo=0x80000000,
		canSendAudio=0x40000000,
		canSendText= 0x20000000,
		canRecvVideo=0x10000000,
		canRecvAudio=0x8000000,
		canRecvText= 0x4000000,
		advancedConf=0x800000,			// II pagina
		saveLayout=0x80000,
		passwordProtect=0x40000,
		hasToolbar=0x20000,
		hasStatusbar=0x10000,
		timeServer=0x8000,			// I pagina
		webServer=0x4000,
		authServer=0x2000,
		dirServer=0x1000,
		DDEenabled=0x800,
		openClientVideo=0x400,
		openClientChat=0x200,
		openServerVideo=0x100,
		openServerAudio=0x80,
		openServerChat=0x40,
		TCP_UDP=0x1,
		namedPipes=0x2
		};
public:
	CVidsendApp();
	int ExitInstance();
	void LoadCustomState();
	void SaveCustomState();

	static char *getNow(char *);
	static char *getNowGMT(char *);
	static char *trim(char *,int);
	DWORD getVersione(char *n=NULL,char *n1=NULL,const char *language=NULL);
	BOOL impostaDaAtomClock(char *);
	static CTime parseGMTTime(const char *);
	static int getMonthFromGMTString(const char *);
	int mandaALog(const char *,const char *,int lev=1);
	int	timedMessageBox(LPSTR,LPSTR,int);
	static BYTE *scaleBitmap(const BITMAPINFO *,BITMAPINFO *,BYTE *d=NULL);
	static int renderBitmap(CDC *,int,RECT *);
	static int renderBitmap(CDC *,const CBitmap *,RECT *);
	static int renderBitmap(CDC *,const BITMAPINFO *,const BYTE *,const RECT *);
	static int renderBitmap(CDC *,const char *,const RECT *,int);
	static int adjustBitmap(BYTE *,short int l=0,short int c=0,short int s=0);
	static CBitmap *createTestBitmap(const SIZE *,LPBITMAPINFOHEADER,int q=0);
	static BOOL salvaBitmap(HBITMAP ,char *);
	static BYTE *createTestWave(BYTE *,const WAVEFORMATEX *,DWORD *,int freq=1000,BYTE tipo=0,BYTE volume=50,int mode=0);
	static BYTE *convertTo22K8(const BYTE *pIn,DWORD dIn,const WAVEFORMATEX *wIn,const WAVEFORMATEX *wOut,BYTE *pOut,DWORD *dOut);
	static BYTE *convertWAV2Modem(const BYTE *pIn,DWORD dIn,const WAVEFORMATEX *wIn,const WAVEFORMATEX *wOut,BYTE *pOut,DWORD *dOut);
	static BYTE *convertWAV16_2Modem8(const BYTE *pIn,DWORD dIn,const WAVEFORMATEX *wIn,BYTE *pOut,DWORD *dOut);
	static BYTE *mixWaves(const BYTE *s,DWORD da,DWORD a,BYTE *d,DWORD l,DWORD in=0,signed char vol=50);
	static DWORD mixWaves(const short int *s,DWORD da,DWORD a,short int *d,DWORD l,DWORD in=0,signed char vol=100);
	static DWORD mixWaves1Mono(const short int *s,DWORD da,DWORD a,short int *d,DWORD l,DWORD in=0,signed char vol=100);
	static DWORD mixWaves(const BYTE *s,DWORD da,DWORD a,short int *d,DWORD l,DWORD in=0,signed char vol=50);
	static DWORD mixWaves(signed char const * *s,const EXT_WAVEFORMATEX *wf,DWORD da,DWORD a,short int *d,DWORD l,DWORD in=0,signed char vol=100);
	static BYTE *mixWaves(const char *n,const WAVEFORMATEX *wOut,DWORD da,DWORD a,BYTE *d,DWORD l,DWORD in=0,signed char vol=50);
	static BYTE *volWaves(const BYTE *s,BYTE *d,DWORD l,int level);
	static DWORD volWaves(const short int*s,short int *d,DWORD l,int level);
	static BYTE *mixWavesStereo(const BYTE *s,DWORD da,DWORD a,BYTE *d,DWORD l,DWORD in=0,signed char volL=100,signed char volR=100);
	static DWORD make2MonoFrom2Stereo(short int *,const short int *,DWORD);
	static DWORD makeMono(short int *,DWORD);   // 
	static DWORD makeSStereo(short int *,DWORD,double tot=0.5);   // 0.5 è ok
	static long getWAVinfo(CFile *f,const char *n, WAVEFORMATEX *wf);
	static long getWAVinfo(const char *nomefile,WAVEFORMATEX *wf);
	static DWORD getWAVduration(CFile *f,const char *n);
	static DWORD getWAVduration(const char *nomefile);
	char *Scramble(char *, const char *, int len, DWORD key);
	static CString creaStringaDaGiorno();
	int callRAS(const char *,const char *user=NULL,const char *pasw=NULL,BOOL sync=TRUE);
	BOOL hangUpRAS();
	char *subGetIPLocation(const char *IP, char *);
//	char *subGetIPLocation(IP_ADDR IP);

	int UtenteCheck(CVidsendSet2_ *,const CString ,const BYTE * ,DWORD *,CTime *,char *,double *,char *,DWORD bExib,DWORD serNum,DWORD idServer,BOOL allowMultiAccess=0);
	int UtenteOnline(CVidsendSet2_ *,CString,CString,int,DWORD,DWORD,DWORD,DWORD);
	int UtenteOffline(CVidsendSet2_ *,CString,DWORD,int,int bForced=FALSE);
	int UtenteOffline(CAuthSrvSocket2 *,int bForced=FALSE);
	DWORD AggUtenteOnline(CVidsendSet5Ex *,DWORD, DWORD idUtente=0,LPCTSTR IP=NULL,DWORD versione=0,DWORD versioneW=0,DWORD serNum=0);
	DWORD AggUtenteOnline(CVidsendSet5 *,DWORD, DWORD idUtente=0,LPCTSTR IP=NULL,DWORD versione=0,DWORD versioneW=0,DWORD serNum=0);
	DWORD AggUtenteOnline(CVidsendSet4 *,DWORD,DWORD idUtenteOsserv=0,DWORD idUtenteExib=0,LPCTSTR tariffa=NULL,BYTE comepago=0,LPCTSTR ipUtenteOsserv=NULL,LPCTSTR ipUtenteExib=NULL,DWORD versione=0);

	int nuovoBrowser(const char *s=NULL);

	static DWORD RTSPvideo_Thread(LPVOID);
	static DWORD WINAPI RTSPvideo(CRTSPClientSocket *);
	static DWORD RTSPaudio_Thread(LPVOID);
	static DWORD WINAPI RTSPaudio(CRTSPClientSocket *);


public:
	CChecksum *myCRC;
	TCHAR szDocPath[MAX_PATH];
  CLogFile *FileSpool;
	char *infoUtenteKey;
	CDatabase *theDB;
	CString theODBCConnectString;
	CVidsendSet2_ *mySetUtenti;
	CVidsendSet8 *mySetSernum;
	CVidsendSet3 *mySetUtentiOnline;
	CVidsendSet5 *mySetLogConnessioni;
	CVidsendSet5Ex *mySetLogConnessioniEx;
	CVidsendSet4 *mySetLogChiamate;
	CVidsendSet2 *mySetUt;
	CString IPaddress,theRouter;
	BOOL bRemoteAccessEnabled;
	int debugMode;

	DWORD Opzioni,OpzioniOpenWin,OpzioniLog,OpzioniDirSrv /* qua, xche' non sempre la finestra Log esiste, ma le sue opz. si'!*/,maxConnWWW;
	char ODBCname[64];
	DWORD maxHTMLconn,maxDirSrvConn,maxAuthConn;
	CWebSrvSocket *wwwSocket;
	UINT serverWWWPort;
	CTimeSocket *timeSocket;
	CDirectorySrvSocket *dirSocket;
	CAuthSrvSocket *authSocket;
	HRASCONN hRasConn;
	CString RASEntry,RASUsername,RASPassword;
	CString RTSPServer,RTSPUsername,RTSPPassword,RTSPStream;
	int RASstate /* usato dalla conn. internet pilotata */,RASiState;
//	char *VarIni;

	HANDLE ThreadHandleRTSPV,ThreadHandleRTSPA;		// handle ai task RTSP, 2020
	HANDLE hCloseEventRTSPV,hCloseEventRTSPA;
	DWORD ThreadRTSPVID,ThreadRTSPAID;

// usate per stampare...
	char *szDevice, *szDriver, *szOutput, *szTitle;
	HANDLE hInitData;  /* handle to initialization data     */
	DOCINFO *dInfo;
	DEVMODE *devMode;


	CMultiDocTemplate *pDocTemplate,*pDocTemplate2,*pDocTemplate22,*pDocTemplate3,*pDocTemplate4,*pDocTemplate5,*pDocTemplate6,*pDocTemplate7;
	CVidsendDoc *aClient[16];
	CVidsendDoc2 *theServer;
	CVidsendDoc22 *theServer2;
	CVidsendDoc3 *theLog;
	CVidsendDoc4 *theChat;
	CVidsendDoc5 *aBrowser[16];
	CVidsendDoc6 *theConnections;
	CVidsendDoc7 *theDirectoryServer;
	struct INFO_UTENTE infoUtente;
	CString suonoInizio,suonoFine;
	CString sfondo;
	CString confPassword,DialUpNome,IPScelto;
	CTime ultimaOra;
#ifdef _CAMPARTY_MODE
	HICON m_hIcon;
#endif

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation
	//{{AFX_MSG(CVidsendApp)
	afx_msg void OnAppAbout();
	afx_msg void OnFileNew();
	afx_msg void OnFileNuovoBrowser();
	afx_msg void OnUpdateFileNuovoBrowser(CCmdUI* pCmdUI);
	afx_msg void OnFileNuovoConnessioni();
	afx_msg void OnUpdateFileNuovoConnessioni(CCmdUI* pCmdUI);
	afx_msg void OnFileNuovoLog();
	afx_msg void OnUpdateFileNuovoLog(CCmdUI* pCmdUI);
	afx_msg void OnFileNuovoVideoserver();
	afx_msg void OnUpdateFileNuovoVideoserver(CCmdUI* pCmdUI);
	afx_msg void OnFileNuovoAudioserver();
	afx_msg void OnUpdateFileNuovoAudioserver(CCmdUI* pCmdUI);
	afx_msg void OnFileNuovoVideoclient();
	afx_msg void OnUpdateFileNuovoVideoclient(CCmdUI* pCmdUI);
	afx_msg void OnFileConfig();
	afx_msg void OnFileNuovoServerdisponibili();
	afx_msg void OnUpdateFileNuovoServerdisponibili(CCmdUI* pCmdUI);
	afx_msg void OnFileNuovoChat();
	afx_msg void OnUpdateFileNuovoChat(CCmdUI* pCmdUI);
	afx_msg void OnFileImpostazioniformatovideo();
	afx_msg void OnUpdateFileImpostazioniformatovideo(CCmdUI* pCmdUI);
	afx_msg void OnFileImpostazionisorgentevideo();
	afx_msg void OnUpdateFileImpostazionisorgentevideo(CCmdUI* pCmdUI);
	afx_msg void OnFileImpostazionistreaming();
	afx_msg void OnUpdateFileImpostazionistreaming(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveVideo();
	afx_msg void OnUpdateFileSaveVideo(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveFotogramma();
	afx_msg void OnUpdateFileSaveFotogramma(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveFotogramma2();
	afx_msg void OnUpdateFileSaveFotogramma2(CCmdUI* pCmdUI);
	afx_msg void OnFileArchivioimmagini();
	afx_msg void OnUpdateFileArchivioimmagini(CCmdUI* pCmdUI);
	afx_msg void OnOpzioniChat();
	afx_msg void OnUpdateOpzioniChat(CCmdUI* pCmdUI);
	afx_msg void OnOpzioniVarie();
	afx_msg void OnUpdateOpzioniVarie(CCmdUI* pCmdUI);
	afx_msg void OnOpzioniVideo();
	afx_msg void OnUpdateOpzioniVideo(CCmdUI* pCmdUI);
	afx_msg void OnVideoTrasmissioneDalvivo();
	afx_msg void OnUpdateVideoTrasmissioneDalvivo(CCmdUI* pCmdUI);
	afx_msg void OnVideoTrasmissioneFilmato();
	afx_msg void OnUpdateVideoTrasmissioneFilmato(CCmdUI* pCmdUI);
	afx_msg void OnInfo();
	afx_msg void OnHelpDesk();
	afx_msg void OnDisconnetti();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VIDSEND2_H__242A4EA4_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)

extern CVidsendApp theApp;

class CStringEx : public CString {
	public:
		enum Options {
			NO_OPTIONS=0,
			COMMA_DELIMIT=1,
			};
		static const int BASE64_MAXLINE;
		static const char *EOL;
		static const char decimalChar,thousandChar;
	public:
		static const char m_base64tab[];
	public:
		CString Tokenize(CString delimiter, int& first);
		static CStringEx CommaDelimitNumber(const char *);
		static CStringEx CommaDelimitNumber(CString);
		static CStringEx CommaDelimitNumber(DWORD);
		CStringEx SubStr(int begin, int len) const;					// substring from s[begin] to s[begin+len]
		BYTE Asc(int);
		int Val(int base=10);
		double Val();
		struct in_addr CStringEx::IPVal();
		void Repeat(int);
		void Repeat(const char *,int);
		void Repeat(char,int);
		void AddCR() { CStringEx::operator+=('\n'); }
		void RemoveLeft(int n) { CStringEx::operator=(Mid(n)); }
		void RemoveRight(int n) { CStringEx::operator=(Mid(1,GetLength()-n)); }		// era un'idea per fare LEFT$ di tot char, ma Trimright c' gi anche se diversa...
		void Trim() { CString::TrimLeft(); CString::TrimRight(); }	
		static BOOL IsAlpha(char);
		BOOL IsAlpha(int);
		static BOOL IsAlnum(char);
		BOOL IsAlnum(int);
		static BOOL IsDigit(char);
		BOOL IsDigit(int);
		static BOOL IsPrint(char);
		BOOL IsPrint(int);
		int FindNoCase(CString substr,int start=0);
		int ReverseFindNoCase(CString substr);
		int Regex(const char *pattern,bool ignoreCase=FALSE);
		WORD GetAsciiLength();
		CStringEx Encode64();
		int Decode64();
		CStringEx FormatTime(int m=0,CTime mT=0);
		CStringEx FormatSize(DWORD);
		CStringEx FormatIP(DWORD);
		void Print();
		void Debug();
		CStringEx() : CString() {};		// servono tutti i costruttori "perch non ne ha di virtual, la CString" !
		// https://www.codeguru.com/cpp/cpp/string/ext/article.php/c2793/CString-Extension.htm
		// https://www.codeproject.com/Articles/2396/Simple-CString-Extension
		CStringEx(const CString& stringSrc) : CString(stringSrc) {};
		// bah, eppure non sembra... 2021...
//		CStringEx(const CStringEx& stringSrc) : CString(stringSrc) {};
		CStringEx(TCHAR ch, int nRepeat = 1) : CString(ch, nRepeat) {};
//		CStringEx(LPCTSTR lpch, int nLength) : CString(lpch, nLength) {};
//		CStringEx(const unsigned char *psz) : CString(psz) {};
		CStringEx(LPCWSTR lpsz) : CString(lpsz) {};
		CStringEx(LPCSTR lpsz) : CString(lpsz) {};
//		CStringEx(const char c) {char s[2]={'\0', '\0'}; s[0]=c; CString::operator=(s);}
		CStringEx(int i, const char* format="%d", DWORD options=NO_OPTIONS);
		CStringEx(double d, const char* format="%02lf", DWORD options=NO_OPTIONS);
		virtual ~CStringEx() {};
private:
	CString InsertSeparator(DWORD);
	};


