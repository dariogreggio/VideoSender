#include "afxcview.h"
#include "mutex.h"
#include "ScopeGuardMutex.h"
#include "player.h"

// vidsendView.h : interface of the CVidsendView classes
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_VIDSEND2VIEW_H__242A4EAE_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
#define AFX_VIDSEND2VIEW_H__242A4EAE_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// CVidsendView - client streaming video

class CVidsendView : public CView {
public:
protected: // create from serialization only
	CVidsendView();
	DECLARE_DYNCREATE(CVidsendView)

public:
	//{{AFX_DATA(CVidsendView)
	enum { IDD = IDD_VIDSEND2_FORM };
	//}}AFX_DATA

// Attributes
public:
	CStreamVCliSocket *cliSockV;
	CStreamACliSocket *cliSockA;
	CControlCliSocket *cliSockCtrl;
	STREAM_INFO streamInfo;
	HIC hICDe;
	BITMAPINFOHEADER biCompDef,biRawDef;
	HDRAWDIB hDD;
	BOOL waitForKeyFrame;
	int framesPerSec;		// in mSec
	HACMSTREAM hAcm;
	HWAVEOUT hWaveOut;
	WAVEHDR wh1,wh2,wh3,wh4;
	WAVEFORMATEX wfn;
	//GSM610WAVEFORMAT wfs;
	EXT_WAVEFORMATEX wfs;
	DWORD maxWaveoutSize;

	UINT theTimer;
	DWORD timeRef,renderedFrameTsp;
	Mutex m_exprMutex;


protected:
	BYTE *theFrame;

// Operations
public:
	CVidsendDoc *GetDocument();
	void drawFrame(const struct AV_PACKET_HDR *);
	void playSample(const struct AV_PACKET_HDR *);
	BOOL syncToVideo(const struct AV_PACKET_HDR *);
	int setTimer(int);
	void initAV(const BITMAPINFOHEADER *,DWORD,const EXT_WAVEFORMATEX *);
	void stopAV();
	int doConnect(const char *,const struct STREAM_INFO *);
	int endConnect();
	BYTE *getFrame() { return theFrame; }
	static WORD measureAudio(const BYTE *,DWORD);
	int doConnessioneRiconnetti(BOOL video=TRUE,BOOL audio=TRUE);
	static long OnSoundPlayerNotify(long);


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendView)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CVidsendView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	static void CALLBACK waveOutCallback(HWAVEOUT,UINT,DWORD,DWORD,DWORD);

// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendView)
	afx_msg void OnConnessioneConnetti();
	afx_msg void OnConnessioneDisconnetti();
	afx_msg void OnUpdateConnessioneConnetti(CCmdUI* pCmdUI);
	afx_msg void OnUpdateConnessioneDisconnetti(CCmdUI* pCmdUI);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveFotogramma();
	afx_msg void OnUpdateFileSaveFotogramma(CCmdUI* pCmdUI);
	afx_msg void OnFileSave();
	afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
	afx_msg void OnConnessioneCaratteristicheconnessione();
	afx_msg void OnUpdateConnessioneCaratteristicheconnessione(CCmdUI* pCmdUI);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnConnessioneRiconnetti();
	afx_msg void OnUpdateConnessioneRiconnetti(CCmdUI* pCmdUI);
	afx_msg void OnUpdateControlloControlloremototelecamereSorgente1(CCmdUI* pCmdUI);
	afx_msg void OnUpdateControlloControlloremototelecamereSorgente2(CCmdUI* pCmdUI);
	afx_msg void OnUpdateControlloControlloremototelecamereSorgente3(CCmdUI* pCmdUI);
	afx_msg void OnUpdateControlloControlloremototelecamereAlternasorgenti(CCmdUI* pCmdUI);
	afx_msg void OnVisualizzaVolume();
	afx_msg void OnDestroy();
	afx_msg void OnUpdateControlloControlloremototelecamereSorgente4(CCmdUI* pCmdUI);
	afx_msg void OnControlloControlloremototelecamereAlternasorgenti();
	afx_msg void OnControlloControlloremototelecamereSorgente1();
	afx_msg void OnControlloControlloremototelecamereSorgente2();
	afx_msg void OnControlloControlloremototelecamereSorgente3();
	afx_msg void OnControlloControlloremototelecamereSorgente4();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in vidsend2View.cpp
inline CVidsendDoc *CVidsendView::GetDocument()
   { return (CVidsendDoc *)m_pDocument; }
#endif


/////////////////////////////////////////////////////////////////////////////




class CBrowserDlg;

/////////////////////////////////////////////////////////////////////////////
// CVidsendView2 - Finestra main video server

class CVidsendView2 : public CView {
	friend class CVidsendDoc2;
protected:
	CVidsendView2();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendView2)

// Attributes
public:
	BOOL inClick;
	CBrowserDlg *myBrowserDlg;

// Operations
public:
	CVidsendDoc2 *GetDocument();
	int doConnect();
	int endConnect();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendView2)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CVidsendView2();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendView2)
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	//}}AFX_MSG
	afx_msg LRESULT OnVideoFrameReady(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnAudioFrameReady(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRawAudioFrameReady(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
	int gdvFrameNum,gdaFrameNum;		// UNIFORMARE con FrameNum in CTV
	};

#ifndef _DEBUG  
inline CVidsendDoc2 *CVidsendView2::GetDocument()
   { return (CVidsendDoc2 *)m_pDocument; }
#endif


/////////////////////////////////////////////////////////////////////////////
// CVidsendView22 - Finestra main audio server

class CJoshuaMP3;
class CVUMeter;
class CDSound;
class CDSoundPlay;

class CButtonDrop : public CButton {			// https://www.ucancode.net/Visual_C_MFC_Samples/CButton-Button-Control-3D-Text-Drawing-VC-MFC-Example.htm
public:
protected:
  //{{AFX_MSG(CButtonDrop)
	afx_msg void OnDropFiles(HDROP hDropInfo);
	//}}AFX_MSG
	//{{AFX_VIRTUAL(CButtonDrop)
	void PreSubclassWindow();
	//}}AFX_VIRTUAL
  DECLARE_MESSAGE_MAP()
	};


class CODBaseBtn : public CButton {		// https://www.codeproject.com/Articles/21184/Owner-Draw-Button-Helper-Class 
public:

public:
    CODBaseBtn();
    virtual ~CODBaseBtn();
		bool GetState() { return bState; }
		void SetCheck(int nCheck) { bState=nCheck; Invalidate(); }

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CODBaseBtn)
    public:
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    protected:
    virtual void PreSubclassWindow();
    //}}AFX_VIRTUAL

protected:
    //{{AFX_MSG(CODBaseBtn)
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
		afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
		afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    //}}AFX_MSG

    // Manually added functions
    afx_msg LRESULT OnMouseLeave( WPARAM, LPARAM );

    DECLARE_MESSAGE_MAP()

protected:
    // Virtual functions provided to draw the states of the button
    virtual void OnDrawNormal(CDC& dc, CRect rect) = 0;
    virtual void OnDrawHovered(CDC& dc, CRect rect) { OnDrawNormal( dc, rect ); }
    virtual void OnDrawPressed(CDC& dc, CRect rect) { OnDrawNormal( dc, rect ); }
    virtual void OnDrawDisabled(CDC& dc, CRect rect) { OnDrawNormal( dc, rect ); }

    // Virtual functions provided to handle the special events
    virtual void OnBeginHover() { }
    virtual void OnEndHover() { }
    virtual void OnPressed() { }
    virtual void OnReleased() { }

private:
  // Flags
  bool m_bMouseHover;
  bool m_bPressedState;

protected:
	BYTE bState;		// serve perché con OwnerDraw non si può avere nessun altro stile, in particolare "checkbox"...
	DWORD mStyle;
};

class CRoundButton : public CODBaseBtn {
public:
//	CBrush m_brBkgnd;
//	CPen m_Pen;
	enum {
		COLOR_VARIATION=40,
		VALUES_RANGE=100
		};

public:
	void SetValues() { m_start=m_end=-1; Invalidate(); }
	void SetValues(short int end) { m_start=0; m_end=end; Invalidate(); }
	void SetValues(short int start, short int end) { m_start=start; m_end=end; Invalidate(); }
	void SetColors(COLORREF fore, COLORREF fore2, COLORREF back) { m_crTextColor=fore; m_crOutlineColor=fore2; m_crBkColor=back; Invalidate(); }
	static COLORREF darken(COLORREF);
	static COLORREF lighten(COLORREF);
  CRoundButton(DWORD style=BS_PUSHBUTTON, COLORREF fore=RGB(192,192,192), COLORREF fore2=RGB(96,96,96), COLORREF back=RGB(64,64,64));
  virtual ~CRoundButton();

private:
	COLORREF m_crBkColor; // Holds the Background Color for the Text
	COLORREF m_crTextColor; // Holds the Color for the Text
	COLORREF m_crOutlineColor;
	short int m_start,m_end;

protected:
  void OnDrawNormal(CDC& dc, CRect rect);
  void OnDrawHovered(CDC& dc, CRect rect);
  void OnDrawPressed(CDC& dc, CRect rect);
  void OnDrawDisabled(CDC& dc, CRect rect);
	};

class CShapeButton : public CODBaseBtn {
public:
//	CBrush m_brBkgnd;
//	CPen m_Pen;

public:
  CShapeButton(COLORREF fore=RGB(64,64,64), COLORREF back=RGB(192,192,192));
  virtual ~CShapeButton();
	void SetColors(COLORREF fore, COLORREF back) { m_crTextColor=fore; m_crBkColor=back; Invalidate(); }

private:
	COLORREF m_crBkColor; // Holds the Background Color for the Text
	COLORREF m_crTextColor; // Holds the Color for the Text

protected:
  void OnDrawNormal(CDC& dc, CRect rect);
  void OnDrawHovered(CDC& dc, CRect rect);
  void OnDrawPressed(CDC& dc, CRect rect);
  void OnDrawDisabled(CDC& dc, CRect rect);
	};

class CStaticDrop : public CStatic {
public:
	CBrush m_brBkgnd; // Holds Brush Color for the Static Text
	COLORREF m_crBkColor; // Holds the Background Color for the Text
	COLORREF m_crTextColor; // Holds the Color for the Text
protected:
  //{{AFX_MSG(CStaticDrop)
	afx_msg void OnDropFiles(HDROP hDropInfo);
	//}}AFX_MSG
	//{{AFX_VIRTUAL(CStaticDrop)
	void PreSubclassWindow();
	afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
	//}}AFX_VIRTUAL
  DECLARE_MESSAGE_MAP()
	};

class CClickableProgressCtrl : public CProgressCtrl {
public:
	CClickableProgressCtrl() { ClickPosition=0; }
	int GetClickPosition();
protected:
    //{{AFX_MSG(CClickableProgressCtrl)
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
		afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    //}}AFX_MSG
  DECLARE_MESSAGE_MAP()

public:
	int ClickPosition;

	};

class CSliderCtrlSmart : public CSliderCtrl {
public:
#define NM_LDBLCLICK 101  // bah
public:
	CSliderCtrlSmart();
protected:
    //{{AFX_MSG(CSliderCtrlSmart)
		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
		afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
		afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
		afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    //}}AFX_MSG
  DECLARE_MESSAGE_MAP()

public:

	};


class CAudioMixer;
class CVidsendView22 : public CView {
	friend class CVidsendDoc22;
protected:
	CVidsendView22();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendView22)

// Attributes
public:
	BOOL inClick;
	WAVEFORMATEX wfex;
	struct EXT_WAVEFORMATEX wfd;
	HACMSTREAM m_hAcm;
	CWaveIn *m_pwi;
	DWORD maxWaveoutSize;
	DWORD aFrameNum;
	CJoshuaMP3 *m_MP3player[2];
	CWinThread *m_MP3Thread[2];
	CWinThread *m_AudioThread;
	CDSound	*m_DSound;
	CDSoundPlay *m_DS;
	Player *playerPreascolto;
	static CVidsendView22 *pThis;
	BOOL inSound;
	int gdaFrameNum;		// UNIFORMARE con FrameNum in CTV

	struct EXT_WAVEFORMATEX soundWf;
	BYTE *soundSample;
	DWORD soundLength;
	BYTE *soundSampleStart;

	int PBMP3from,PBMP3to;
	BYTE *PBMP3buffer;
	DWORD PBMP3bufferSize,PBMP3bufferPointer;

	CAudioMixer *m_Mixer;
//	class lock_guard busyAudio;			// semaforo per callback audio/wavein
	CCriticalSection /*CRITICAL_SECTION*/ m_CritSection;
//	CSingleLock *busyAudio;
//	BYTE busyAudio2;
	HANDLE busyAudio3;

	CSliderCtrlSmart *volume1,*volume2,*volume3,*volume4,*fader,*pitch1,*pitch2;
	CRoundButton *button[10];
	CRoundButton *buttonS,*buttonP1,*buttonP2;
	CShapeButton *buttonM1,*buttonM2,*buttonM3,*buttonM4,*buttonPre1,*buttonPre2,*buttonPre3,*buttonPre4;
	CButtonDrop *buttonC1,*buttonC2;
	CButton *buttonE1;
	CStaticDrop *txtMP31,*txtMP32;
	CVUMeter *VUMeter1,*VUMeter2,*VUMeter3,*VUMeter4,*VUMeter5;
	CClickableProgressCtrl *progress1,*progress2;
	CFont m_Font1,m_Font2;


// Operations
public:
	CVidsendDoc22 *GetDocument();
	int doConnect();
	int endConnect();
	int initCapture(const WAVEFORMATEX *preferredWf, BOOL bVerbose);
	void UpdateBanco();
//	int waveCallback(HWAVEIN hwi,UINT uMsg,DWORD_PTR dwInstance,DWORD_PTR dwParam1,DWORD_PTR dwParam2);
	static UINT suonaMP3(LPVOID);
	static UINT audioProva(LPVOID);
	int stopMP3(BYTE);
	static WORD measureAudio(const short int *,DWORD);
	double getVolumeFader(BYTE );
	int playSound(BYTE n,BYTE volume);
	int playSound(const char *sndFile,BYTE volume,BYTE q=0);
	int playSound(const BYTE *sndSample,BYTE volume);
	static LONG soundNotifyCallback(void *,LONG,LONG);
	static LONG mp3NotifyCallback(void *,DWORD, DWORD, DWORD);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendView22)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	BOOL OnEraseBkgnd(CDC* pDC);
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CVidsendView22();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendView22)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnToolTipNotify(UINT /*id*/, NMHDR *pNMHDR, LRESULT * /*pResult*/);
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	afx_msg void OnConnessioni11();
	afx_msg void OnUpdateConnessioni11(CCmdUI* pCmdUI);
	afx_msg void OnConnessioniElenco();
	afx_msg void OnAudioCanzone1();
	afx_msg void OnAudioCanzone2();
	afx_msg void OnAudioPlaylist();
	afx_msg void OnAudioEqualizzatoreflat();
	afx_msg void OnUpdateAudioEqualizzatoreflat(CCmdUI* pCmdUI);
	afx_msg void OnAudioEqualizzatorerock();
	afx_msg void OnUpdateAudioEqualizzatorerock(CCmdUI* pCmdUI);
	afx_msg void OnAudioEqualizzatorepop();
	afx_msg void OnUpdateAudioEqualizzatorepop(CCmdUI* pCmdUI);
	afx_msg void OnAudioEqualizzatoredance();
	afx_msg void OnUpdateAudioEqualizzatoredance(CCmdUI* pCmdUI);
	afx_msg void OnAudioEqualizzatoreclassica();
	afx_msg void OnUpdateAudioEqualizzatoreclassica(CCmdUI* pCmdUI);
	afx_msg void OnAudioEffettisonori(UINT nID);
	afx_msg void OnUpdateAudioEffettisonori(CCmdUI* pCmdUI);
	afx_msg void OnAudioEffettisonori2(UINT nID);
	afx_msg void OnAudioEffettisonoristop();
	afx_msg void OnUpdateAudioEffettisonoristop(CCmdUI* pCmdUI);
	afx_msg void OnAudioEffettisonoriBanco(UINT nID);
	afx_msg void OnUpdateAudioEffettisonoriBanco(CCmdUI* pCmdUI);
	afx_msg void OnAudioEffettisonoriImposta();
	afx_msg void OnAudioEffettisonoriImpostaBtn(UINT nID,NMHDR* p_notify_msg_ptr, LRESULT* p_result_ptr);
	//}}AFX_MSG
	afx_msg LRESULT OnAudioFrameReady(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRawAudioFrameReady(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMP3Finished(WPARAM wParam, LPARAM lParam);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnAudioEffettisonoriPlay1();
	afx_msg void OnAudioEffettisonoriPlay2();
	afx_msg void OnAudioEffettisonoriOnAir();
	afx_msg void OnAudioEffettisonoriMute1();
	afx_msg void OnAudioEffettisonoriMute2();
	afx_msg void OnAudioEffettisonoriMute3();
	afx_msg void OnAudioEffettisonoriMute4();
	afx_msg void OnAudioEffettisonoriMutePre1();
	afx_msg void OnAudioEffettisonoriMutePre2();
	afx_msg void OnAudioEffettisonoriMutePre3();
	afx_msg void OnProgressCanzone1(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnProgressCanzone2(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnSliderFader(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnSliderPitch1(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnSliderPitch2(NMHDR* pNotifyStruct, LRESULT* result);
	DECLARE_MESSAGE_MAP()
	};

#ifndef _DEBUG  
inline CVidsendDoc22 *CVidsendView22::GetDocument()
   { return (CVidsendDoc22 *)m_pDocument; }
#endif





/////////////////////////////////////////////////////////////////////////////
// CVidsendView3 view - log

class CVidsendSet;


class CVidsendView3 : public CListView {
	struct DB_DATA {
		CTime data;
		char indirizzo[32];
		char descrizione[256];
		};

protected:
	CVidsendView3();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendView3)

// Attributes
public:
	CVidsendSet *m_pSet;
	int sortCol;

// Operations
public:
	CVidsendDoc3 *GetDocument();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendView3)
	public:
	virtual void OnInitialUpdate();
	virtual CRecordset* OnGetRecordset();
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	static int CALLBACK CompareElem(DWORD, DWORD, CVidsendView3 *);
	void OnDeleteitemList1(NMHDR*, LRESULT*);
	virtual ~CVidsendView3();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendView3)
	afx_msg void OnColumnclick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLogCancellariga();
	afx_msg void OnUpdateLogCancellariga(CCmdUI* pCmdUI);
	afx_msg void OnLogCancellatutto();
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in vidsend2View.cpp
inline CVidsendDoc3 *CVidsendView3::GetDocument()
   { return (CVidsendDoc3 *)m_pDocument; }
#endif


/////////////////////////////////////////////////////////////////////////////
// CVidsendView4 form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

#define USA_TXOLE

class CTX4OLE;

class CVidsendView4 : public CFormView {
public:
	enum {
		reverseMsg=1,
		supervisorMsg=2,
		computerMsg=4,
		serverVideo=0x80
		};
protected:
	CVidsendView4();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendView4)

// Form Data
public:
	//{{AFX_DATA(CVidsendView4)
#ifdef _NEWMEET_MODE
	enum { IDD = IDD_CHATFORM_NM };
#else
	enum { IDD = IDD_CHATFORM };
#endif
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

// Attributes
public:
	CImageList il;
	CChatCliSocket *cliSock;
	CHAT_INFO chatInfo;
	CHAT_ROOMS_INFO chatRoomsInfo;
	HICON hIconUser,hIconSupervisor,hIconComputer,hIconSuona,hIconClear,hIconIcone,hIconOne2One;
	CFont theFont;
	CBitmap bmpSpkOn;
	CListBox *wl;

// Operations
public:
	CVidsendDoc4 *GetDocument();
	int doConnect(const char *,struct CHAT_INFO *);
	int endConnect();
	int updateTree();
	void addToListBox(struct CHAT_MESSAGE *);
	int subAddToListBox(CRichEditCtrl *,struct CHAT_MESSAGE *,BOOL CRLF=TRUE);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendView4)
	protected:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CVidsendView4();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CVidsendView4)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnConnessioneConnetti();
	afx_msg void OnUpdateConnessioneConnetti(CCmdUI* pCmdUI);
	afx_msg void OnConnessioneDisconnetti();
	afx_msg void OnUpdateConnessioneDisconnetti(CCmdUI* pCmdUI);
	afx_msg void OnConnessioneRiconnetti();
	afx_msg void OnUpdateConnessioneRiconnetti(CCmdUI* pCmdUI);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMis);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnDeleteItem(int nIDCtl, LPDELETEITEMSTRUCT lpDeleteItemStruct);
	afx_msg void OnUtentiConnettivideo();
	afx_msg void OnUpdateUtentiConnettivideo(CCmdUI* pCmdUI);
	afx_msg void OnUtentiMandaemail();
	afx_msg void OnUpdateUtentiMandaemail(CCmdUI* pCmdUI);
	afx_msg void OnUtenteDisconnetti();
	afx_msg void OnUpdateUtenteDisconnetti(CCmdUI* pCmdUI);
	afx_msg void OnUtenteInseriscinegliindesiderati();
	afx_msg void OnUpdateUtenteInseriscinegliindesiderati(CCmdUI* pCmdUI);
	afx_msg void OnUtenteMandamessaggio();
	afx_msg void OnUpdateUtenteMandamessaggio(CCmdUI* pCmdUI);
	afx_msg void OnUtentiMandamessaggiopertutti();
	afx_msg void OnUpdateUtentiMandamessaggiopertutti(CCmdUI* pCmdUI);
	afx_msg void OnMessaggioCancella();
	afx_msg void OnUpdateMessaggioCancella(CCmdUI* pCmdUI);
	afx_msg void OnMessaggioCancellatutto();
	afx_msg void OnUpdateMessaggioCancellatutto(CCmdUI* pCmdUI);
	afx_msg void OnMessaggioInvia();
	afx_msg void OnUpdateMessaggioInvia(CCmdUI* pCmdUI);
	afx_msg void OnFileSave();
	afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveAll();
	afx_msg void OnUpdateFileSaveAll(CCmdUI* pCmdUI);
	afx_msg void OnCopiaChat();
	afx_msg void OnUpdateCopiaChat(CCmdUI* pCmdUI);
	afx_msg void OnDestroy();
	afx_msg void OnRclick(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in vidsend2View.cpp
inline CVidsendDoc4 *CVidsendView4::GetDocument()
   { return (CVidsendDoc4 *)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////
// CVidsendView5 - client HTML per ?

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif
#include <afxhtml.h>

class CVidsendView5 : public CHtmlView {
protected:
	CVidsendView5();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendView5)

// html Data
public:
	//{{AFX_DATA(CVidsendView5)
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

// Attributes
public:

// Operations
public:
	CVidsendDoc5 *GetDocument();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendView5)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual void OnInitialUpdate();
	virtual void OnTitleChange(LPCTSTR lpszText);
	virtual void OnProgressChange(long nProgress, long nProgressMax);
	virtual void OnDownloadBegin();
	virtual void OnDownloadComplete();
	virtual void OnStatusTextChange(LPCTSTR lpszText);
	virtual void OnBeforeNavigate2(LPCTSTR lpszURL, DWORD nFlags, LPCTSTR lpszTargetFrameName, CByteArray& baPostedData, LPCTSTR lpszHeaders, BOOL* pbCancel);
	virtual void OnNewWindow2(LPDISPATCH* ppDisp, BOOL* Cancel);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CVidsendView5();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CVidsendView5)
	afx_msg void OnNavigazioneAvanti();
	afx_msg void OnNavigazioneIndietro();
	afx_msg void OnUpdateNavigazioneIndietro(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNavigazioneAvanti(CCmdUI* pCmdUI);
	afx_msg void OnNavigazionePaginainiziale();
	afx_msg void OnNavigazioneVai();
	afx_msg void OnNavigazioneAggiorna();
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnNavigazioneCerca();
	afx_msg void OnNavigazioneInterrompi();
	afx_msg void OnUpdateNavigazioneInterrompi(CCmdUI* pCmdUI);
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


#ifndef _DEBUG  // debug version in hView.cpp
inline CVidsendDoc5 *CVidsendView5::GetDocument()
   { return (CVidsendDoc5 *)m_pDocument; }
#endif




/////////////////////////////////////////////////////////////////////////////
// CVidsendView6 view

class CVidsendView6 : public CTreeView {
protected:
	CVidsendView6();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendView6)

// Attributes
public:
	CImageList il;

// Operations
public:
	CVidsendDoc6 *GetDocument();
	int update();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendView6)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CVidsendView6();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendView6)
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnVisualizzaAggiorna();
	afx_msg void OnDestroy();
	afx_msg void OnConnessioniDisconnetti();
	afx_msg void OnUpdateConnessioniDisconnetti(CCmdUI* pCmdUI);
	afx_msg void OnConnessioniInseriscinegliindesiderati();
	afx_msg void OnUpdateConnessioniInseriscinegliindesiderati(CCmdUI* pCmdUI);
	afx_msg void OnConnessioniMandamessaggio();
	afx_msg void OnUpdateConnessioniMandamessaggio(CCmdUI* pCmdUI);
	afx_msg void OnConnessioniMandamessaggioatutti();
	afx_msg void OnVisualizzaIpLookup();
	afx_msg void OnUpdateVisualizzaIpLookup(CCmdUI* pCmdUI);
	afx_msg void OnVisualizzaAncheDirectoryServer();
	afx_msg void OnUpdateVisualizzaAncheDirectoryServer(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in hView.cpp
inline CVidsendDoc6 *CVidsendView6::GetDocument()
   { return (CVidsendDoc6 *)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////
// CVidsendView7 view

class CVidsendView7 : public CTreeView {
protected:
	CVidsendView7();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CVidsendView7)

// Attributes
public:
	CImageList il;

// Operations
public:
	CVidsendDoc7 *GetDocument();
	int update();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendView7)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CVidsendView7();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CVidsendView7)
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnVisualizzaAggiorna();
	afx_msg void OnDestroy();
	afx_msg void OnComputerDisconnetti();
	afx_msg void OnUpdateComputerDisconnetti(CCmdUI* pCmdUI);
	afx_msg void OnComputerMandamessaggio();
	afx_msg void OnUpdateComputerMandamessaggio(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in hView.cpp
inline CVidsendDoc7 *CVidsendView7::GetDocument()
   { return (CVidsendDoc7 *)m_pDocument; }
#endif


/////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.


#endif // !defined(AFX_VIDSEND2VIEW_H__242A4EAE_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
