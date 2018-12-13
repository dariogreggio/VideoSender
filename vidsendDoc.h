// vidsend2Doc.h : interface of the CVidsendDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_VIDSEND2DOC_H__242A4EAC_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
#define AFX_VIDSEND2DOC_H__242A4EAC_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "vidsendSet.h"

class CExDocument : public CDocument {
public:
	char *prfSection;
	DWORD Opzioni;
// Operations
public:
	CView *getView() { POSITION pos=GetFirstViewPosition(); return GetNextView(pos); }
	void getWindow(RECT *);
	void move(int, int, int x2=0, int y2=0);
	int GetPrivateProfileInt(int k) { return theApp.prStore->GetPrivateProfileInt(prfSection,k);};
	int WritePrivateProfileInt(int k, int v)  { return theApp.prStore->WritePrivateProfileInt(prfSection,k,v);};
	int GetPrivateProfileString(int k,char *s,int l,char *def=NULL) { return theApp.prStore->GetPrivateProfileString(prfSection,k,s,l,def ? def : "");};
	int WritePrivateProfileString(int k, const char *s)  { return theApp.prStore->WritePrivateProfileString(prfSection,k,s);};
	CTime GetPrivateProfileTime(int k) { return theApp.prStore->GetPrivateProfileTime(prfSection,k);};
	CTimeSpan GetPrivateProfileTimeSpan(int k) { return theApp.prStore->GetPrivateProfileTimeSpan(prfSection,k);};
	int WritePrivateProfileTime(int k, CTime t) { return theApp.prStore->WritePrivateProfileTime(prfSection,k,t);};
	int WritePrivateProfileTime(int k, CTimeSpan t) { return theApp.prStore->WritePrivateProfileTime(prfSection,k,t);};
	};

class CVidsendDoc : public CExDocument {
public:
	enum {
		autoRASconnect=0x8000000,
		authenticate=0x4000000,
		usaProxy=0x2000000,
		mayRecvVideo=0x100000,
		mayRecvAudio=0x80000,
		mayRecvText= 0x40000,
		fmt_resize= 0x8000,
		fmt_controls=0x4000,				// mostra lumin. contr. satur
		fmt4_3= 0x10,
		fmt_full= 0x20,
		fmt_double=0x40,
		fmt_bn=0x80,
		sstereo=0x8,
		} OPZIONI_PROPRIETA;
	enum {
		MRU_SIZE=10
		};
protected: // create from serialization only
	CVidsendDoc();
	DECLARE_DYNCREATE(CVidsendDoc)

// Attributes
public:
	CAuthCliSocket *authSocket;
	CString srvAddress;
	BOOL bPaused;
	WORD numBuffers;
	CString Proxy;
	CString loginName,loginPasw,authWWW;
	CString streamTitle;
	short int contrasto,luminosita,saturazione;

// Operations
public:
	CString loadCliConnRecent(CString s[MRU_SIZE]);
	BOOL save();
	void standardSize();
	void sendHrtBt();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CVidsendDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendDoc)
	afx_msg void OnFileProprieta();
	afx_msg void OnVisualizza43();
	afx_msg void OnUpdateVisualizza43(CCmdUI* pCmdUI);
	afx_msg void OnVisualizzaAtuttoschermo();
	afx_msg void OnUpdateVisualizzaAtuttoschermo(CCmdUI* pCmdUI);
	afx_msg void OnVisualizzaBn();
	afx_msg void OnUpdateVisualizzaBn(CCmdUI* pCmdUI);
	afx_msg void OnVisualizzaDimensionedoppia();
	afx_msg void OnUpdateVisualizzaDimensionedoppia(CCmdUI* pCmdUI);
	afx_msg void OnVisualizzaStereosimulato();
	afx_msg void OnUpdateVisualizzaStereosimulato(CCmdUI* pCmdUI);
	afx_msg void OnConnessionePausa();
	afx_msg void OnUpdateConnessionePausa(CCmdUI* pCmdUI);
	afx_msg void OnConnessioneRiprendi();
	afx_msg void OnUpdateConnessioneRiprendi(CCmdUI* pCmdUI);
	afx_msg void OnVisualizzaVolume();
	afx_msg void OnFilePrint();
	afx_msg void OnUpdateFilePrint(CCmdUI* pCmdUI);
	afx_msg void OnControlloImmagine();
	afx_msg void OnUpdateControlloImmagine(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc2 document
struct QUALITY_MODEL_V {
	RECT imageSize;
	WORD bpp;
	WORD fps;
	DWORD compressor;
	DWORD quality;
	};

struct QUALITY_MODEL_A {
	DWORD samplesPerSec;
	WORD bitsPerSample;
	WORD channels;
	DWORD compressor;
	DWORD quality;
	};

struct TEST_PAGE_PROP {
	int tipoVideo;
	int tipoAudio;
	int audioOpzioni;
	};

class CVidsendDoc2 : public CExDocument {
public:
	enum {
		sendVideo=1,								// I pagina propr.
		videoType=0x4,
		sendAudio=0x10,
		maySendVideo=0x8000,
		maySendAudio=0x4000,
		maySendText= 0x2000,

		forceBN=0x200000,						// II pagina propr.
		doMirror=0x100000,
		doFlip=0x80000,

		openWWW=0x80000000,					// III & IV pagina propr.
		needAuthenticate=0x60000000,	// radiobutton TipoAutorizzazione
		dontSave=0x10000000,

		timedConnection=0x8000000,
		needAuthenticateServer=0x4000000,
		registerServer=0x2000000,
		openVideoOnConnect=0x1000000,
		askOnConnect=0x800000,
		doDialUp=0x400000
		} OPZIONI_PROPRIETA;
	enum {
		quandoCancello=0x8,		// bit 0..3
		quantiFrame=0x10,
		quantiFile=0x20,
		accoda=0x40,
		autoAvvia=0x100
		} OPZIONI_SALVA_FILE_AVI;
	enum {
		useOverlay=0x1,
		aviLoop=0x10000,
		aviMode=0x20000
		} OPZIONI_SORGENTE_VIDEO;
	enum {
		PIPEBUFSIZE=4096
		};
protected:
	CVidsendDoc2();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendDoc2)

// Attributes
public:
	DWORD maxConn,videoSource,alternaSource,OpzioniSalvaVideo,KFrame,OpzioniSorgenteVideo,countFTP;
	CString forceOpenWWW,authenticationWWW,directoryWWW,directoryWWWLogin,myLogin,myPassword,splashOrIntroName,streamTitle;
	CString suonoIn,suonoOut;
	CTimeSpan timedConnLenght;
	CTime countFTP_day;
	struct QUALITY_MODEL_V myQV;
	struct QUALITY_MODEL_A myQA;
	CTV *theTV;
	CVidsendSet2 *theSet;
	DWORD myID;	// ID utente server nel database utenti
	CTimeSpan myTimedConn;		// se, come server, il mio tempo di trasmissione viene limitato
	CStreamSrvSocket *streamSocketV,*streamSocketA;
	CControlSrvSocket *controlSocket;
	CDirectoryCliSocket *dirCliSocket;
//	struct LOGGED_USER_INFO users[MAX_STREAM_CLIENTS];	// sistemare e rendere dinamico...
	BOOL bPaused,bAudio;
	RECT qualityBox;



	CAuthCliSocket *authSocket;
//spostato x test sovrascrittura...


	int trasmMode,imposeDateTime,imposeTextPos;
	char imposeText[32];
	CString nomeAVI,pathAVI,nomeAVI_PB;
	struct QUALITY_MODEL_V qsv[7];
	struct QUALITY_MODEL_A qsa[3];
	struct TEST_PAGE_PROP pagProva;
	PAVIFILE aviFile;
	PAVISTREAM psVideo,psAudio;
	AVISTREAMINFO *PBsi;
	BITMAPINFOHEADER *PBbiSrc,*PBbiDest;
	PGETFRAME gotFrame;
	HANDLE pipeV;
	TCHAR *pipeVBuffer;

// Operations
public:
	void sendHrtBt();
	void checkUtenti();
	void setTXMode(BOOL );
	struct STREAM_INFO *getConnectionInfo();
	int calcBandWidth();
	int calcBandWidth(int, int);
	int calcBandWidth(BOOL ,RECT *,int ,DWORD ,int ,int ,BOOL ,DWORD ,WORD ,WORD ,DWORD,int,CString *info=NULL);
	int acceptConnect(const char *who=NULL);
	int openVideo(CVidsendView2 *);
	DWORD getAVstep(struct QUALITY_MODEL_V *,struct QUALITY_MODEL_A *);
	int impostaVideoSource(int );
	BOOL save();
	int MandaPipeV(struct AV_PACKET_HDR *,LPARAM);
	int HandlePipeV();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc2)
	public:
	virtual void OnCloseDocument();
	//}}AFX_VIRTUAL

// Implementation
public:
	BOOL OnNewDocument();
	virtual ~CVidsendDoc2();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
#ifdef _NEWMEET_MODE
public:									// per menu condivisi
#else
protected:							// ok se menu MDI diversi
#endif
	//{{AFX_MSG(CVidsendDoc2)
	afx_msg void OnFileProprieta();
	afx_msg void OnUpdateVideoPaginadiprova(CCmdUI* pCmdUI);
	afx_msg void OnVideoLivellivolume();
	afx_msg void OnVideoTrasmissioneRiprendi();
	afx_msg void OnUpdateVideoTrasmissioneRiprendi(CCmdUI* pCmdUI);
	afx_msg void OnVideoTrasmissionePausa();
	afx_msg void OnUpdateVideoTrasmissionePausa(CCmdUI* pCmdUI);
	afx_msg void OnVideoAudio();
	afx_msg void OnUpdateVideoAudio(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveFotogramma();
	afx_msg void OnUpdateFileSaveFotogramma(CCmdUI* pCmdUI);
#ifndef _NEWMEET_MODE
	afx_msg void OnVideoTrasmissioneDalvivo();
	afx_msg void OnVideoTrasmissioneFilmato();
	afx_msg void OnVideoTrasmissionePaginadiprova();
	afx_msg void OnUpdateVideoTrasmissionePaginadiprova(CCmdUI* pCmdUI);
	afx_msg void OnUpdateVideoTrasmissioneFilmato(CCmdUI* pCmdUI);
	afx_msg void OnUpdateVideoTrasmissioneDalvivo(CCmdUI* pCmdUI);
#endif
	afx_msg void OnVideoInformazioni();
	afx_msg void OnFileSaveVideo();
	afx_msg void OnUpdateFileSaveVideo(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveFotogramma2();
	afx_msg void OnUpdateFileSaveFotogramma2(CCmdUI* pCmdUI);
	afx_msg void OnFileImpostazionivideo();
	afx_msg void OnUpdateFileImpostazionivideo(CCmdUI* pCmdUI);
	afx_msg void OnFileArchivioimmagini();
	afx_msg void OnUpdateFileArchivioimmagini(CCmdUI* pCmdUI);
	afx_msg void OnFilePrint();
//	afx_msg void OnUpdateFilePrint(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc3 document - log file

class CVidsendDoc3 : public CExDocument {
public:
	enum {
		logAttivo=1,
		} OPZIONI_PROPRIETA;
protected:
	CVidsendDoc3();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendDoc3)

// Attributes
public:
	CVidsendSet *m_vidsendSet;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc3)
	public:
	virtual void OnCloseDocument();
	protected:
	virtual BOOL OnNewDocument();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CVidsendDoc3();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendDoc3)
	afx_msg void OnFileProprieta();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc4 document

class CVidsendDoc4 : public CExDocument {
public:
	enum {
		MRU_SIZE=4
		};
public:
	enum {
		ancheAccessoWeb=0x80000000,			// 1° tab
		noOne2One=0x40000000,
		onlyOne2One=0x20000000,
		saveMessages=0x10000000,
		needAuthenticate=0x800000,			// 2° tab (opzioni server)
		openWWW=0x400000,
		dontSave=0x200000,
		timedConnection=0x100000,
		mostraEU=0x80000,
		noPrivateMsg=0x40000,
		noFreeChat=0x20000,
		slaveMode=0x10000,				// indica che e' stato aperto da qualche automatismo (newmeet)
		serverMode=0x80000,
		usaColors=0x4000,
		usaSounds=0x2000,
		usaIcons=0x1000,

		authenticate=0x80,			// 3° tab (opzioni client)
		doDialUp=0x40,
		usaProxy=0x20
		} OPZIONI_PROPRIETA;
	enum {
		avvisi_sonori=0x80000000,
		avvisi_sonori2=0x40000000,
		testo_colorato=0x20000000
		};
protected:
	CVidsendDoc4();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendDoc4)

// Attributes
public:
	CAuthCliSocket *authSocket;
	CChatSrvSocket chatSocket;
	DWORD maxConn,maxMessaggi;
	CString srvAddress;
	CString suonoIn,suonoOut;
	DWORD opzioniVisive;
	BOOL firstConnect;
	CTimeSpan timedConnLenght;
	CString Proxy;
	CString loginName,loginPasw,forceOpenWWW,authenticationWWW;

// Operations
public:
	int saveBlacklistedIP(CStringList *BlacklistedIP);
	int loadBlacklistedIP(CStringList *BlacklistedIP);
	CString loadCliConnRecent(CString s[MRU_SIZE]);
	void sendHrtBt();
	BOOL save();
	int updateTree();
	struct CHAT_INFO *getConnectionInfo();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc4)
	public:
	virtual void Serialize(CArchive& ar);   // overridden for document i/o
	virtual void OnCloseDocument();
	protected:
	virtual BOOL OnNewDocument();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CVidsendDoc4();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
#ifdef _NEWMEET_MODE
public:
#else
protected:
#endif
	//{{AFX_MSG(CVidsendDoc4)
	afx_msg void OnFileProprieta();
	afx_msg void OnVisualizzaAvvisisonori();
	afx_msg void OnVisualizzaImpostacolore();
	afx_msg void OnUpdateVisualizzaAvvisisonori(CCmdUI* pCmdUI);
	afx_msg void OnUpdateVisualizzaImpostacolore(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	};

/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc5 document

class CVidsendDoc5 : public CExDocument {
public:
	enum {
		MRU_SIZE=10
		};
protected:
	CVidsendDoc5();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendDoc5)

// Attributes
public:
	CString home,URL;
	CString Proxy;
	int mode;

// Operations
public:
	void setMode(int );
	void setURL(CString);
	CString loadCliConnRecent(CString *s);
	BOOL save();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc5)
	public:
	virtual BOOL OnNewDocument();
	//}}AFX_VIRTUAL

// Implementation
public:
	BOOL OnNewDocument(char *s=NULL);
	virtual ~CVidsendDoc5();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendDoc5)
	afx_msg void OnFileProprieta();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};



/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc6 document

class CVidsendDoc6 : public CExDocument {
public:
	enum {
		mostraAncheDirSrv=1,
		} OPZIONI_PROPRIETA;
protected:
	CVidsendDoc6();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendDoc6)

// Attributes
public:

// Operations
public:
	int update();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc6)
	public:
	virtual void Serialize(CArchive& ar);   // overridden for document i/o
	virtual void OnCloseDocument();
	protected:
	virtual BOOL OnNewDocument();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CVidsendDoc6();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendDoc6)
	afx_msg void OnFileProprieta();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CVidsendDoc7 document

class CVidsendDoc7 : public CExDocument {
public:
	enum {
		ancheAccessoWeb=1,
		} OPZIONI_PROPRIETA;
protected:
	CVidsendDoc7();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendDoc7)

// Attributes
public:

// Operations
public:
	int update();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendDoc7)
	public:
	virtual void OnCloseDocument();
	protected:
	virtual BOOL OnNewDocument();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CVidsendDoc7();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendDoc7)
	afx_msg void OnFileProprieta();
	afx_msg void OnComputerMandamessaggio();
	afx_msg void OnUpdateComputerMandamessaggio(CCmdUI* pCmdUI);
	afx_msg void OnComputerDisconnetti();
	afx_msg void OnUpdateComputerDisconnetti(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};



//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VIDSEND2DOC_H__242A4EAC_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
