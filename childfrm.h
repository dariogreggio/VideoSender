// ChildFrm.h : interface of the CChildFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_CHILDFRM_H__242A4EAA_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
#define AFX_CHILDFRM_H__242A4EAA_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CVUMeter;
class CControlsWnd;


class CChildFrame : public CMDIChildWnd {
	DECLARE_DYNCREATE(CChildFrame)
public:
	CChildFrame();

// Attributes
public:
	CStatusBar  m_wndStatusBar;
	CVUMeter *m_VUMeter;
	CControlsWnd *m_ControlsWnd;
	HICON iconOff,iconOn,iconSpk,iconSpkOff;

// Operations
public:
	void setStatusIcons();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildFrame)
	public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	protected:
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CChildFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	//{{AFX_MSG(CChildFrame)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	};

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CChildFrame2 frame

class CChildFrame2 : public CMDIChildWnd {
	DECLARE_DYNCREATE(CChildFrame2)
protected:
	CChildFrame2();           // protected constructor used by dynamic creation

// Attributes
public:
	CStatusBar m_wndStatusBar;
	HICON iconOff,iconOn,iconSpkOff,iconSpkOn,iconPlay,iconPause,iconRecOn,iconRecOff,iconCamera;

// Operations
public:
	void setStatusIcons(CVidsendDoc2 *d=NULL);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildFrame2)
	protected:
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CChildFrame2();

	// Generated message map functions
	//{{AFX_MSG(CChildFrame2)
	afx_msg void OnMove(int x, int y);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClose();
	//}}AFX_MSG
	afx_msg void OnGetMinMaxInfo( MINMAXINFO FAR* lpMMI );
	DECLARE_MESSAGE_MAP()
	};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CChildFrame3 frame

class CChildFrame3 : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CChildFrame3)
protected:
	CChildFrame3();           // protected constructor used by dynamic creation

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildFrame3)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CChildFrame3();

	// Generated message map functions
	//{{AFX_MSG(CChildFrame3)
		// NOTE - the ClassWizard will add and remove member functions here.
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CChildFrame4 frame

class CChildFrame4 : public CMDIChildWnd {
	DECLARE_DYNCREATE(CChildFrame4)
protected:
	CChildFrame4();           // protected constructor used by dynamic creation

// Attributes
public:
	CStatusBar  m_wndStatusBar;
//	CToolBar    m_wndToolBar;
//	CReBar      m_wndReBar;
	HICON iconOff,iconOn;


// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildFrame4)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CChildFrame4();

	// Generated message map functions
	//{{AFX_MSG(CChildFrame4)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class CChildFrame5 : public CMDIChildWnd {
	DECLARE_DYNCREATE(CChildFrame5)
protected:
	CChildFrame5();           // protected constructor used by dynamic creation

// Attributes
public:
	CStatusBar  m_wndStatusBar;
	CToolBar    m_wndToolBar;
	CReBar      m_wndReBar;
	CDialogBar      m_wndDlgBar;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildFrame5)
	protected:
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CChildFrame5();

	// Generated message map functions
	//{{AFX_MSG(CChildFrame5)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHILDFRM_H__242A4EAA_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
