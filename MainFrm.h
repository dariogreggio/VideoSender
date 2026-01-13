// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__242A4EA8_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
#define AFX_MAINFRM_H__242A4EA8_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CBackWnd : public CWnd {
// Construction
public:
	CBackWnd() {};

// Implementation
public:
	virtual ~CBackWnd() {};

// Generated message map functions
protected:
	//{{AFX_MSG(CBackWnd)
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class CMainFrame : public CMDIFrameWnd {

	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// Attributes
public:
	CBackWnd m_BackWnd;

// Operations
public:
	void showToolbar(int n) { m_wndToolBar.ShowWindow(n ? SW_SHOW : SW_HIDE); RecalcLayout(); }
	void showStatusbar(int n) { m_wndStatusBar.ShowWindow(n ? SW_SHOW : SW_HIDE); RecalcLayout(); }

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL DestroyWindow();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members
	CStatusBar  m_wndStatusBar;
	CToolBar    m_wndToolBar;
	CReBar      m_wndReBar;
	CDialogBar  m_wndDlgBar;

// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnEndSession(BOOL bEnding);
	afx_msg void OnClose();
//	afx_msg void OnDraw(CDC*);
//	afx_msg void OnPaint();
//	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg void OnNcPaint();
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg BOOL OnQueryEndSession();
	afx_msg void OnWindowLayout1();
	afx_msg void OnWindowLayout2();
	afx_msg void OnWindowLayout3();
	afx_msg void OnWindowLayoutAdatta();
	afx_msg void OnUpdateWindowLayoutAdatta(CCmdUI* pCmdUI);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnVisualizzaToolbar();
	afx_msg void OnVisualizzaStatusBar();
	afx_msg void OnUpdateVisualizzaToolbar(CCmdUI* pCmdUI);
	afx_msg void OnUpdateVisualizzaStatusBar(CCmdUI* pCmdUI);
	afx_msg void OnWindowLayout4();
	//}}AFX_MSG
	afx_msg LRESULT OnUpdatePane(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnCloseChild(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
	};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__242A4EA8_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
