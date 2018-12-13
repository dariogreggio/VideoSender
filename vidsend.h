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
#define WM_CLOSE_CHILD WM_APP+1
#define WM_VIDEOFRAME_READY WM_APP+7
#define WM_AUDIOFRAME_READY WM_APP+8

#define MAX_FTP_A_DAY 100
#define AUTHSOCK_TIMEOUT 120000
#define VIDSEND_VERSIONE MAKEWORD(0,1)			// versione dei pacchetti video...



class CVidsendDoc;
class CVidsendDoc2;
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
	char *getProfileRoot(char *);
	char *getVariabiliKey(char *);
private:
	char *getProfileKey(char *,const char *);

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


/////////////////////////////////////////////////////////////////////////////
// CVidsendApp:
// See vidsend2.cpp for the implementation of this class
//

struct STREAM_INFO {
	WORD versione;
	BITMAPINFOHEADER bm;
	DWORD fps;
	DWORD quality;
	EXT_WAVEFORMATEX wf;
	BOOL noBuffers;			// il server dice "non bufferizzare" perche' la trasmissione non e' continua.
	CTimeSpan maxTime;
	char authenticationWWW[128];
	DWORD IDServer;			// il numero ID dell'utente che fa da server, all'interno del database utenti
	BYTE dontSave;
	BYTE remoteCtrl;
	char openWWW[128];			// per aprire un WEBsite in parallelo alla connessione... tanto per gradire!
	char splashOrIntro[128];			// Schermata o ID risorsa o animazione PRIMA della partenza del video
	char streamTitle[64];			// Titolo della trasmissione
	};

/*struct CONTROL_INFO {		// da' errore in compilazione!
	BYTE tipo;
	union {
		STREAM_INFO s;
		char myBuf[256];
		};
	};*/

struct CHAT_INFO {
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


class CFileSizeString : public CString {
private:
	CString InsertSeparator(DWORD );
public:
	CString FormatSize(DWORD );
	};

class CVidsendApp : public CWinApp {
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
		openServerChat=0x80,
		TCP_UDP=0x1,
		namedPipes=0x2
		};
public:
	CVidsendApp();
	int ExitInstance();

	static char *getNow(char *);
	static char *getNowGMT(char *);
	DWORD getVersione(char *n=NULL,char *n1=NULL);
	BOOL impostaDaAtomClock(char *);
	static CTime parseGMTTime(char *);
	static int getMonthFromGMTString(char *);
	int mandaALog(const char *,const char *,int lev=1);
	int	timedMessageBox(LPSTR,LPSTR,int);
	BYTE *scaleBitmap(BITMAPINFO *,BITMAPINFO *,BYTE *d=NULL);
	static int renderBitmap(CDC *,int,RECT *);
	static int renderBitmap(CDC *,CBitmap *,RECT *);
	static int renderBitmap(CDC *,BITMAPINFO *,BYTE *,RECT *);
	static int renderBitmap(CDC *,const char *,RECT *);
	static int adjustBitmap(BYTE *,short int l=0,short int c=0,short int s=0);
	static CBitmap *createTestBitmap(RECT *,LPBITMAPINFOHEADER,int q=0);
	BOOL salvaBitmap(HBITMAP ,char *);
	static BYTE *createTestWave(WAVEFORMATEX *,DWORD *,int freq=1000,int tipo=0);
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

	int nuovoBrowser(char *s=NULL);


public:
	CChecksum *myCRC;
	CProfileStore *prStore;
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
	char RASEntry[64],RASUsername[32],RASPassword[32];
	int RASstate /* usato dalla conn. internet pilotata */,RASiState;
//	char *VarIni;

// usate per stampare...
	char *szDevice, *szDriver, *szOutput, *szTitle;
	HANDLE hInitData;  /* handle to initialization data     */
	DOCINFO *dInfo;
	DEVMODE *devMode;


	CMultiDocTemplate *pDocTemplate,*pDocTemplate2,*pDocTemplate3,*pDocTemplate4,*pDocTemplate5,*pDocTemplate6,*pDocTemplate7;
	CVidsendDoc *aClient[16];
	CVidsendDoc2 *theServer;
	CVidsendDoc3 *theLog;
	CVidsendDoc4 *theChat;
	CVidsendDoc5 *aBrowser[16];
	CVidsendDoc6 *theConnections;
	CVidsendDoc7 *theDirectoryServer;
	struct INFO_UTENTE infoUtente;
	CString suonoInizio,suonoFine;
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

