// Copyright (c) 1996-2000 Logitech, Inc.  All Rights Reserved


// Image.cpp : implementation file
//

#include "stdafx.h"
#include "vfw.h"
#include "Image.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CImage

CImage::CImage()
: m_pbi(NULL)
{
}

CImage::~CImage()
{
    ClearImage();
}


static int BmiRowSize(const LPBITMAPINFOHEADER pbi)
{
    return ((pbi->biBitCount * pbi->biWidth) + 31) / 32 * 4;
}

static int BmiColors(const LPBITMAPINFOHEADER pbi)
{
    int nColors = pbi->biClrUsed;
    if (nColors == 0) {
        nColors = (1 << pbi->biBitCount) & 0x1FF;
    }
    return nColors;
}


static size_t BmiSize(const LPBITMAPINFOHEADER pbi)
{
    size_t s = pbi->biSize + BmiColors(pbi) * sizeof (RGBQUAD);
    if (pbi->biSizeImage) {
        s += pbi->biSizeImage;
    } else {
        s += pbi->biHeight * BmiRowSize(pbi);
    }
    return s;
} // BmiSize


static BYTE* BmiFindBits(LPBITMAPINFOHEADER pbi)
{
    BYTE* lp = (BYTE*)pbi;
    return lp + pbi->biSize + BmiColors(pbi) * sizeof (RGBQUAD);
} // BmiFindBits


void CImage::ClearImage(void)
{
    if (m_pbi) {
        free(m_pbi); m_pbi = NULL; m_lpbits = NULL;
    }
    if (m_hWnd) {
        Invalidate(TRUE);
    }
} // ClearImage


void CImage::SetImage(LPBITMAPINFOHEADER pbi)
{
    ClearImage();
    if (pbi) {
        size_t nb = BmiSize(pbi);
        m_pbi = (LPBITMAPINFOHEADER)malloc(nb);
        memcpy(m_pbi, pbi, nb);
        m_lpbits = BmiFindBits(m_pbi);
    }
    if (m_hWnd) {
        Invalidate(TRUE);
    }
}


void CImage::SetImage(BITMAPFILEHEADER *pbmp)
{
    // This assumes a 'packed' file, with the DIB immediately
    // following the file header.  Every BMP file I've ever
    // dumped is this way, but...
    LPBITMAPINFOHEADER pbi = (LPBITMAPINFOHEADER)++pbmp;
    SetImage(pbi);
}


void CImage::DrawImage(CDC& dc, CRect rc)
{
    if (m_pbi) {
        HDRAWDIB hdd = DrawDibOpen();
        if (hdd) {
            DrawDibDraw(hdd, dc.m_hDC,
                rc.left, rc.top,       // xDst, yDst,
                rc.Width(), rc.Height(),
                m_pbi,
                m_lpbits,
                0, 0,
                m_pbi->biWidth, m_pbi->biHeight,
                0);
            
            DrawDibClose(hdd);
        }
    } else {
        dc.FillSolidRect(rc, RGB(192, 192, 192));
    }
}


BEGIN_MESSAGE_MAP(CImage, CStatic)
	//{{AFX_MSG_MAP(CImage)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CImage message handlers

void CImage::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
    CRect rc;
    GetClientRect(rc);
    DrawImage(dc, rc);
}
