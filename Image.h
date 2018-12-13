#if !defined(AFX_IMAGE_H__F4E51541_42FD_11D4_8434_0050DA7BC9AB__INCLUDED_)
#define AFX_IMAGE_H__F4E51541_42FD_11D4_8434_0050DA7BC9AB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Image.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CImage window

class CImage : public CStatic
{
// Construction
public:
	CImage();

// Attributes
public:

// Operations
public:

    void SetImage(LPBITMAPINFOHEADER pbi);
    // Makes a copy of the specified DIB image and displays it.
    // Note that pbi == NULL is OK, it sets 'no image'.

    void SetImage(BITMAPFILEHEADER *pbmp);
    // Same as above, but with a pointer to a BMP file in memory.

    void ClearImage(void);
    // Set to no image.

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CImage)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CImage();

protected:
    LPBITMAPINFOHEADER  m_pbi;          // pointer to current (malloc'd) DIB
    BYTE*               m_lpbits;       // pointer to actual pixels

    virtual void DrawImage(CDC& dc, CRect rc);
    // Draw the current image to dc, scaled to fit in rectangle rc

    // Generated message map functions
	//{{AFX_MSG(CImage)
	afx_msg void OnPaint();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IMAGE_H__F4E51541_42FD_11D4_8434_0050DA7BC9AB__INCLUDED_)
