// DigitalText.cpp : implementation file
// G.Dar 1997-2023

#include "stdafx.h"
//#include "Joshua.h"
#include <math.h>
#include <DigitalText.h>
#include "resource.h"       // main symbols

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// 11/4/09; 29.6.17
// using vectors, Juli 2017
// logarithmic vu-meter, 2023

/////////////////////////////////////////////////////////////////////////////
// CDigitalText

CDigitalText::CDigitalText() {

	*m_Text=0;
	nDigit=3;
	style=0;
	brightness=100;
	color=RGB(255,0,0);
	ratio=50;		// aggiustato poi in "Create"
	*m_Caption=0;
	captionToTextOffset.x=0;
	captionToTextOffset.y=0;
	m_Font=NULL;
	thickness=1;
	slant=10;
	endCap=0;
	overlap=0;

	}

CDigitalText::~CDigitalText() {

	BDisplay.DeleteObject();
	}


BEGIN_MESSAGE_MAP(CDigitalText, CStatic)
	//{{AFX_MSG_MAP(CDigitalText)
	ON_WM_PAINT()
#ifndef _WIN32_WCE
	ON_WM_WINDOWPOSCHANGING()
#endif
	//}}AFX_MSG_MAP
//	ON_MESSAGE(WM_SETTEXT,OnSetText)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDigitalText message handlers

void CDigitalText::OnPaint() {
	CPaintDC dc(this); // device context for painting

	paint(&dc);
	// Do not call CStatic::OnPaint() for painting messages
	}

void CDigitalText::paint(CDC *dc) {
	char myBuf[64];
	TCHAR *p;
	int x,y,n,th;
	int xSize,ySize,xSize0,ySize0;
	CDC srcDC;
	BITMAP myB;
	RECT myRect;
	CBitmap *hOldB;
	COLORREF bkColor;

	switch(style & LED_TIPI) {
		case CDigitalText::LCD:
			bkColor=RGB(208,255,192);
			break;
		case CDigitalText::LED_VERDI:
			bkColor=RGB(0,0,0);
			break;
		case CDigitalText::LED_GIALLI:
			bkColor=RGB(0,0,0);
			break;
		case CDigitalText::LED_ROSSI:
			bkColor=RGB(0,0,0);
			break;
		case CDigitalText::NIXIE:
			bkColor=RGB(40,50,0);
			break;
		case  CDigitalText::VERDANA:
			bkColor=RGB(255,255,255);
			break;
		default:			//non deve succedere!
			bkColor=RGB(40,0,0);			// 40,0,0 NON  in palette e questo provoca problemi alle GIF!!
			break;
		}

	CBrush brush1(bkColor),*oldBrush;

//	CStatic::GetWindowText(myBuf,64);
//	dc.TextOut(0,0,myText,strlen(myText));
	GetClientRect(&myRect);		// era true rect ma neglio no, o rimane un pezzetto fuori!
//	myRect.top++;
//	myRect.bottom--;
//	myRect.left++;
//	myRect.right--;
	if(style & CDigitalText::VERDANA) {
		th=min((myRect.bottom-myRect.top-2)/8,2);
		}
	else
		th=(myRect.bottom-myRect.top-2)/16;
	CPen pen1(PS_SOLID,th,RGB(80,32,32)),pen2(PS_SOLID,th,bkColor),pen3(PS_SOLID,th,RGB(48,0,0)),*oldPen;

//		oldBrush=dc->SelectObject(&brush1);
//	oldPen=dc->SelectObject(&pen2);
//	GetClientRect(&myRect2);		// devo cmq riempire la finestrina...
//	dc->Rectangle(&myRect2);

	oldBrush=dc->SelectObject(&brush1);
	if(style & border) {
		oldPen=dc->SelectObject(&pen2);
		dc->Rectangle(&myRect);
		x=myRect.right-th+1-1;
		y=myRect.bottom-th+1-1;
		if(style & border3D) {
			dc->SelectObject(&pen1);
			dc->MoveTo(myRect.left,y);
			dc->LineTo(myRect.left,myRect.top);
			dc->LineTo(x,myRect.top);
			dc->SelectObject(&pen3);
//			dc.MoveTo(myRect.top,x);
			dc->LineTo(x,y);
			dc->LineTo(myRect.left,y);
			ySize=myRect.bottom-myRect.top-th*2-1-2;
			y=th+th+1;
			}
		else {
			dc->SelectObject(&pen1);
			dc->MoveTo(myRect.left,y);
			dc->LineTo(myRect.left,myRect.top);
			dc->LineTo(x,myRect.top);
			dc->LineTo(x,y);
			dc->LineTo(myRect.left,y);
			ySize=myRect.bottom-myRect.top-th-1-2;
			y=th+1;
			}
		x=th;
		if(style & CDigitalText::VERDANA) {
			x+=2;
			y+=2;
			}
		}
	else {
		oldPen=dc->SelectObject(&pen2);
		dc->Rectangle(&myRect);
		ySize=myRect.bottom-myRect.top-2;
		y=1;
		x=0;
		}

	myRect.top++;
	myRect.bottom--;
	myRect.left++;
	myRect.right--;
	xSize=(ySize*ratio)/100/*+1*/;
	if(style & hasTitleTop) {
		CFont Font1,*oldFont;
		Font1.CreateFont(ySize/3,xSize,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"verdana");
			// per qualche strano motivo il "ratio" non funziona... correggo qui!

		oldFont=dc->SelectObject(&Font1);
	//		dc->Rectangle(&myRect);
		captionToTextOffset.y=(myRect.bottom-myRect.top)/3;
		y-=captionToTextOffset.y;
		dc->TextOut(style & border ? 3 : 0,ySize-ySize/3-(style & border ? 3 : 0),m_Caption);
		dc->SelectObject(oldFont);
		Font1.DeleteObject();
		}
	if(style & hasTitleLeft) {
		CFont Font1,*oldFont;
		Font1.CreateFont(ySize,xSize/2,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"verdana");
			// per qualche strano motivo il "ratio" non funziona... correggo qui!

		oldFont=dc->SelectObject(&Font1);
	//		dc->Rectangle(&myRect);
		CSize size=dc->GetTextExtent(m_Caption,_tcslen(m_Caption));
		captionToTextOffset.x=size.cx +(size.cx/10) /*(myRect.right-myRect.left)/3*/;
		x+=captionToTextOffset.x;
		dc->TextOut(style & border ? 2 : 0,style & border ? 2 : 0,m_Caption);
		dc->SelectObject(oldFont);
		Font1.DeleteObject();
		}

	srcDC.CreateCompatibleDC(dc);
	hOldB=(CBitmap *)srcDC.SelectObject(BDisplay);
	if(!(style & useVector))
		BDisplay.GetBitmap(&myB);
	if(style & CDigitalText::NIXIE) {
		xSize0=48 /*myB.bmWidth*/;
		}
	else if(style & CDigitalText::LCD) {
		xSize0=26 /*myB.bmWidth*/;
		}
	else if(style & CDigitalText::LED_VERDI) {
		xSize0=32 /*myB.bmWidth*/;
		}
	else if(style & CDigitalText::VERDANA) {
		xSize0=0;		// non usato
		}
	else {
		xSize0=32 /*myB.bmWidth*/;
		}
	ySize0=myB.bmHeight;

	if((style & LED_TIPI) == CDigitalText::VERDANA) {
		CFont Font1,*oldFont;
   	Font1.CreateFont(ySize,(xSize*85)/100,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"verdana");
		// per qualche strano motivo il "ratio" non funziona... correggo qui!

		oldFont=dc->SelectObject(&Font1);
//		dc->Rectangle(&myRect);
		dc->TextOut(x,y,m_Text,_tcslen(m_Text));
		CSize size=dc->GetTextExtent(m_Text,_tcslen(m_Text));
		dc->SelectObject(oldFont);
		Font1.DeleteObject();
		}
	else {
		p=m_Text;
		while(*p) {
			switch(*p) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					n=*p-'0';
					break;
				case '.':
					n=10;
					break;
				case ':':
					n=13;
					break;
				case '-':
					n=11;
					break;
				case '+':
					n=12;
					break;
				case ' ':					// per ora avanza come tutti gli altri!
				default:
					n=-1;
					break;
				}
			if(n>=0) {
				if(style & useVector) {
					plotDigit(dc,n,x,y,xSize,ySize);
					}
				else {
					dc->StretchBlt(x,y,xSize,ySize,&srcDC,xSize0*n,0,xSize0,ySize0,SRCCOPY);
					}
				}
			x+=(*p=='.' ? xSize/2 : xSize)+1;
			p++;
			}
		}

	dc->SelectObject(oldBrush);
	dc->SelectObject(oldPen);
	srcDC.SelectObject((HBITMAP)hOldB);
	srcDC.DeleteDC();
	}

int CDigitalText::plotDigit(CDC *dc,BYTE n,WORD xOrg,WORD yOrg,WORD xSize,WORD ySize) {
	int x1,y1,x2,y2;
	POINT coords[7];		// 6 coordinate per 7 segmenti + punto
	int noOverlap;
	COLORREF myColor;

	switch(style & LED_TIPI) {
		case CDigitalText::LED_VERDI:
			myColor=RGB(0,255,0);
			break;
		case CDigitalText::LED_GIALLI:
			myColor=RGB(255,255,0);
			break;
		case CDigitalText::LED_ROSSI:
			myColor=RGB(255,0,0);
			break;
		default:		// tanto per...
			myColor=RGB(255,255,255);
			break;
		}
	CPen pen1(PS_SOLID,thickness,myColor),*oldPen;
	CBrush brush1(RGB(0,0,0)),*oldBrush;
	oldBrush=dc->SelectObject(&brush1);
	oldPen=dc->SelectObject(&pen1);

	xSize-=thickness+2;
	ySize-=thickness+2;

	coords[0].x=xOrg+((ySize*slant)/100);			// top-left
	coords[0].y=yOrg;
	coords[1].x=xOrg+xSize+((ySize*slant)/100);		// top-right
	coords[1].y=yOrg;
	coords[2].x=xOrg+((ySize*slant)/200);			// mid-left
	coords[2].y=yOrg+ySize/2;
	coords[3].x=xOrg+xSize+((ySize*slant)/200);			// mid-right
	coords[3].y=yOrg+ySize/2;
	coords[4].x=xOrg;			// bottom-left
	coords[4].y=yOrg+ySize;
	coords[5].x=xOrg+xSize;			// bottom-right
	coords[5].y=yOrg+ySize;
	coords[6].x=xOrg+xSize;			// punto
	coords[6].y=yOrg+ySize;

	noOverlap=max(1,(coords[1].x-coords[0].x)/6);

	switch(n) {
		case 0:
			slant;
			endCap;
			overlap;
			drawSegmentA(dc,coords,noOverlap);
			drawSegmentB(dc,coords,noOverlap);
			drawSegmentC(dc,coords,noOverlap);
			drawSegmentD(dc,coords,noOverlap);
			drawSegmentE(dc,coords,noOverlap);
			drawSegmentF(dc,coords,noOverlap);
			break;
		case 1:
			drawSegmentB(dc,coords,noOverlap);
			drawSegmentC(dc,coords,noOverlap);
			break;
		case 2:
			drawSegmentA(dc,coords,noOverlap);
			drawSegmentB(dc,coords,noOverlap);
			drawSegmentG(dc,coords,noOverlap);
			drawSegmentE(dc,coords,noOverlap);
			drawSegmentD(dc,coords,noOverlap);
			break;
		case 3:
			drawSegmentA(dc,coords,noOverlap);
			drawSegmentB(dc,coords,noOverlap);
			drawSegmentC(dc,coords,noOverlap);
			drawSegmentD(dc,coords,noOverlap);
			drawSegmentG(dc,coords,noOverlap);
			break;
		case 4:
			drawSegmentB(dc,coords,noOverlap);
			drawSegmentC(dc,coords,noOverlap);
			drawSegmentG(dc,coords,noOverlap);
			drawSegmentF(dc,coords,noOverlap);
			break;
		case 5:
			drawSegmentA(dc,coords,noOverlap);
			drawSegmentC(dc,coords,noOverlap);
			drawSegmentD(dc,coords,noOverlap);
			drawSegmentF(dc,coords,noOverlap);
			drawSegmentG(dc,coords,noOverlap);
			break;
		case 6:
			drawSegmentA(dc,coords,noOverlap);
			drawSegmentC(dc,coords,noOverlap);
			drawSegmentD(dc,coords,noOverlap);
			drawSegmentE(dc,coords,noOverlap);
			drawSegmentF(dc,coords,noOverlap);
			drawSegmentG(dc,coords,noOverlap);
			break;
		case 7:
			drawSegmentA(dc,coords,noOverlap);
			drawSegmentB(dc,coords,noOverlap);
			drawSegmentC(dc,coords,noOverlap);
			break;
		case 8:
			drawSegmentA(dc,coords,noOverlap);
			drawSegmentB(dc,coords,noOverlap);
			drawSegmentC(dc,coords,noOverlap);
			drawSegmentD(dc,coords,noOverlap);
			drawSegmentE(dc,coords,noOverlap);
			drawSegmentF(dc,coords,noOverlap);
			drawSegmentG(dc,coords,noOverlap);
			break;
		case 9:
			drawSegmentA(dc,coords,noOverlap);
			drawSegmentB(dc,coords,noOverlap);
			drawSegmentC(dc,coords,noOverlap);
			drawSegmentD(dc,coords,noOverlap);
			drawSegmentF(dc,coords,noOverlap);
			drawSegmentG(dc,coords,noOverlap);
			break;
		case 10:		// punto, usare Point?
			drawSegmentDot(dc,coords,noOverlap);
			break;
		case 11:		// -
			drawSegmentG(dc,coords,noOverlap);
			break;
		case 12:		// +
			drawSegmentG(dc,coords,noOverlap);
			if(!overlap) {
				dc->MoveTo((coords[3].x+coords[2].x)/2,(coords[2].y+coords[0].y)/2+noOverlap);
				dc->LineTo((coords[3].x+coords[2].x)/2,(coords[4].y+coords[2].y)/2-noOverlap);
				}
			else {
				dc->MoveTo((coords[3].x+coords[2].x)/2,(coords[2].y+coords[0].y)/2);
				dc->LineTo((coords[3].x+coords[2].x)/2,(coords[4].y+coords[2].y)/2);
				}
			break;
		case 13:		// :
//			dc->MoveTo(coords[2]);
//			dc->LineTo(coords[3]);
			break;
		}

	dc->SelectObject(oldBrush);
	dc->SelectObject(oldPen);
	return 1;
	}

void CDigitalText::drawSegmentA(CDC *dc,POINT coords[],int noOverlap) {

	if(!overlap) {
		dc->MoveTo(coords[0].x+noOverlap,coords[0].y);
		dc->LineTo(coords[1].x-noOverlap,coords[0].y);
		}
	else {
		dc->MoveTo(coords[0]);
		dc->LineTo(coords[1]);
		}
	}

void CDigitalText::drawSegmentB(CDC *dc,POINT coords[],int noOverlap) {

	if(!overlap) {
		dc->MoveTo(coords[1].x,coords[1].y+noOverlap);
		dc->LineTo(coords[3].x,coords[3].y-noOverlap);
		}
	else {
		dc->MoveTo(coords[1]);
		dc->LineTo(coords[3]);
		}
	}

void CDigitalText::drawSegmentC(CDC *dc,POINT coords[],int noOverlap) {

	if(!overlap) {
		dc->MoveTo(coords[3].x,coords[3].y+noOverlap);
		dc->LineTo(coords[5].x,coords[5].y-noOverlap);
		}
	else {
		dc->MoveTo(coords[3]);
		dc->LineTo(coords[5]);
		}
	}

void CDigitalText::drawSegmentD(CDC *dc,POINT coords[],int noOverlap) {

	if(!overlap) {
		dc->MoveTo(coords[4].x+noOverlap,coords[4].y);
		dc->LineTo(coords[5].x-noOverlap,coords[5].y);
		}
	else {
		dc->MoveTo(coords[4]);
		dc->LineTo(coords[5]);
		}
	}

void CDigitalText::drawSegmentE(CDC *dc,POINT coords[],int noOverlap) {

	if(!overlap) {
		dc->MoveTo(coords[2].x,coords[2].y+noOverlap);
		dc->LineTo(coords[4].x,coords[4].y-noOverlap);
		}
	else {
		dc->MoveTo(coords[2]);
		dc->LineTo(coords[4]);
		}
	}

void CDigitalText::drawSegmentF(CDC *dc,POINT coords[],int noOverlap) {

	if(!overlap) {
		dc->MoveTo(coords[0].x,coords[0].y+noOverlap);
		dc->LineTo(coords[2].x,coords[2].y-noOverlap);
		}
	else {
		dc->MoveTo(coords[0]);
		dc->LineTo(coords[2]);
		}
	}

void CDigitalText::drawSegmentG(CDC *dc,POINT coords[],int noOverlap) {

	if(!overlap) {
		dc->MoveTo(coords[2].x+noOverlap,coords[2].y);
		dc->LineTo(coords[3].x-noOverlap,coords[2].y);
		}
	else {
		dc->MoveTo(coords[2]);
		dc->LineTo(coords[3]);
		}
	}

void CDigitalText::drawSegmentDot(CDC *dc,POINT coords[],int noOverlap) {
	int sz=max(1,(coords[5].x-coords[4].x)/15);

	dc->Ellipse(coords[4].x+3-sz,coords[4].y-sz,coords[4].x+3+sz,coords[4].y+sz);
	//uso la coordinata del bottom-left perch il puntino occupa un suo spazio, ridotto :)
	}

void CDigitalText::SetCaption(LPSTR lpszCaption) {

	_tcscpy(m_Caption,lpszCaption);
	Invalidate();
	}

BOOL CDigitalText::Create(LPCTSTR lpszWindowName, LPSTR lpszCaption, DWORD st, WORD digit, const RECT& rect, CWnd* pParentWnd, UINT nID) {
	int i;
	TCHAR myBuf[16];
	RECT myRect=rect;
	
	style=st;
	if(digit)
		nDigit=digit;
	if(lpszWindowName) {
		_tcsncpy(myBuf,lpszWindowName,15);
		myBuf[15]=0;
		}
	else {
		memset(myBuf,'-',nDigit);
		myBuf[nDigit]=0;
		}
//	if(nDigit)		// v. WindowPosChanging
//		myRect.right=myRect.left+(nDigit*((myRect.bottom-myRect.top-2)*ratio))/10+4;
	if(!(style & useVector))
		loadBitmap();

	i=CStatic::Create(myBuf, (style & noClipSiblings /* ma risp. agli stili privati com'?? */ ? 0 : WS_CLIPSIBLINGS) | WS_VISIBLE | WS_CHILD /*| WS_BORDER*/, 
		myRect, pParentWnd, nID);
	if(lpszCaption)
		_tcscpy(m_Caption,lpszCaption);
	if(style & (hasTitleLeft | hasTitleTop)) {		// parte dell'altezza viene sacrificata al Titolo... (v.sopra)
		}

	thickness=max(1,(myRect.bottom-myRect.top)/12);

	return i;
	}


WINAPI CDigitalText::MoveWindow(int X, int Y,
																int nWidth, int nHeight, BOOL bRepaint) {

	thickness=max(1,(nHeight)/12);

	CStatic::MoveWindow(X,Y,nWidth,nHeight,bRepaint);
	}

WINAPI CDigitalText::MoveWindow(RECT *rc, BOOL bRepaint) {


	MoveWindow(rc->left,rc->top,rc->right-rc->left,rc->bottom-rc->top,bRepaint);
	}

void CDigitalText::SetWindowText(int n) {
	TCHAR myBuf[16],myBuf1[16];

	wsprintf(myBuf1,_T("%%0%ud"),nDigit);
	if(style & zeroAsInval) {
		if(!n) {
			memset(myBuf,'-',nDigit);
			myBuf[nDigit]=0;
			}
		else
			wsprintf(myBuf,myBuf1,n);
		}	
	else
		wsprintf(myBuf,myBuf1,n);

	SetWindowText(myBuf);
	}

void CDigitalText::SetWindowText(double d,WORD nDec) {
	TCHAR myBuf1[16];
	CString S;

	if(style & zeroAsInval) {
		if(!d) {
			memset(myBuf1,'-',nDigit);
			myBuf1[nDigit]=0;
			S=myBuf1;
			}
		else {
			wsprintf(myBuf1,_T("%%0%u.%uf"),nDigit-1 /* per il segno!*/,nDec);
			S.Format(myBuf1,d);
			}
		}
	else {
		wsprintf(myBuf1,_T("%%0%u.%uf"),nDigit-1 /* per il segno!*/,nDec);
		S.Format(myBuf1,d);
		}

	SetWindowText((LPCTSTR)S);
	}

void CDigitalText::SetWindowText(TCHAR *p) {

	if(p && *p) {
		if(style & blankLZero) {
			TCHAR *p1=p;
			while(*p1=='0')
				*p1++=' ';
			if((!*p1 || *p1 == '.') && *(p1-1)==' ')		//se il primo carattere che seguiva gli zeri e' il punto decimale (opp. se non ci sono + caratteri!)
				*(p1-1)='0';											// rimetto l'ultimo '0'
			}
		if(style & showPlus && *p != '-') {
			*m_Text='+';
			_tcscpy(m_Text+1,p);
			}
		else
			_tcscpy(m_Text,p);
		if(style & rightJustify) {
			int i=nDigit-_tcslen(m_Text);
			while(i>0) {
				memmove(m_Text,m_Text+1,nDigit);
				*m_Text=' ';
				}
			}
		}
	else {
		if(style & nullAsInval) {
			memset(m_Text,'-',nDigit);
			m_Text[nDigit]=0;
			}
		else
			*m_Text=0;
		}
	Invalidate();
	}

void CDigitalText::SetWindowText(CString S) {
	TCHAR *p;

//	nDigit=S.GetLength();
//	if(style & showPlus)
//		nDigit++;
	p=S.GetBuffer(255);
	SetWindowText(p);
	S.ReleaseBuffer();
	}

void CDigitalText::SetWindowText(CTimeSpan T) {

	nDigit=5;
	SetWindowText((LPCTSTR)T.Format(_T("%M:%S")));
	}

void CDigitalText::SetWindowText(CTime T) {

	nDigit=5;
	SetWindowText((LPCTSTR)T.Format(_T("%H:%M")));
	}

void CDigitalText::SetStyle(enum STYLE_FLAGS t, BOOL m) {

	if(m)
		style |= t;
	else
		style &= ~t;
	loadBitmap();
	}

void CDigitalText::loadBitmap() {
	int i;

	if(style & CDigitalText::NIXIE) {
		i=IDB_DISPLAY_NIXIE;
		ratio=57;
		}
	else if(style & CDigitalText::LCD) {
		i=IDB_DISPLAY_LCD;
		ratio=60;
		}
	else if(style & CDigitalText::LED_VERDI) {
		i=IDB_DISPLAY_GREEN;
		ratio=51;
		}
	else if(style & CDigitalText::LED_GIALLI) {
		i=IDB_DISPLAY_GREEN;
		ratio=51;
		}
	else if(style & CDigitalText::VERDANA) {
		i=IDB_DISPLAY;	/// togliere...
		ratio=65;
		}
	else {
		i=IDB_DISPLAY;
		ratio=51;
		}
	BDisplay.DeleteObject();
	i=BDisplay.LoadBitmap(i);
	}

CBitmap *CDigitalText::GetBitmap() {
	CBitmap *pBitmap,*oldB;
	CDC *dc,dc2;
	RECT myRect;
	BITMAPINFO bmi;
	int i;
	void *v;

	dc=GetDC() /*::GetWindowDC(NULL)*/;
	dc2.CreateCompatibleDC(dc);		// non e' necessario Delete: lo fa il distruttore!
//	paint(dc);
	GetTrueRect(&myRect);
	pBitmap=new CBitmap;
//	pBitmap->CreateBitmap(myRect.right,myRect.bottom,1,8,NULL);
	pBitmap->CreateCompatibleBitmap(dc,myRect.right,myRect.bottom);
	oldB=dc2.SelectObject(pBitmap);
	paint(&dc2);
//	dc2.BitBlt(0,0,myRect.right,myRect.bottom,dc,0,0,SRCCOPY);
	dc2.SelectObject(oldB);
	ReleaseDC(dc);

	return pBitmap;
	}

/*afx_msg LRESULT CDigitalText::OnSetText(WPARAM wParam, LPARAM lParam) {
	char *p=(char *)lParam;

	if(blankLZero) {
		while(*p=='0')
			p++;
		}
	if(showPlus && *p != '-') {
		*myText='+';
		strcpy(myText+1,p);
		}
	else
		strcpy(myText,p);
	Invalidate();
	return TRUE;
	}*/

RECT *CDigitalText::GetTrueRect(RECT *r) { 
	
	if(r) {
		GetClientRect(r);
		if(nDigit) {
			r->right=((nDigit/*+1*/)*((r->bottom-2)*ratio))/100+(style & border ? (style & border3D ? 2 : 4) : 6);
			}
		if(style & hasTitleLeft) {
			r->right+=captionToTextOffset.x /*((2)*((r->bottom-2)*ratio))/100+(style & border ? (style & border3D ? 2 : 4) : 6)*/;
			}
		if(style & hasTitleTop) {
			r->bottom+=((2)*((r->bottom-2)*ratio))/100+(style & border ? (style & border3D ? 2 : 4) : 6);
			}
		r->right=(r->right/*+3*/) & 0xfffc;		 // per compatibilita' con GetBitmap!
		}
	return r;
	}

#ifndef _WIN32_WCE

void CDigitalText::OnWindowPosChanging(WINDOWPOS FAR* lpwndpos) {
	RECT r;

	if(nDigit) {
//		GetTrueRect(&r);
		lpwndpos->cx=(nDigit*((lpwndpos->cy-2)*ratio))/100+(style & border ? (style & border3D ? 2 : 4) : 6);
//		lpwndpos->flags &= ~SWP_NOSIZE;
		}
	if(style & hasTitleLeft) {
		lpwndpos->cx+=captionToTextOffset.x ? captionToTextOffset.x : // finire... al primo giro non  valido e cos... improvviso...
			((2)*((lpwndpos->cy-2)*ratio))/100+(style & border ? (style & border3D ? 2 : 4) : 6);
		}
	if(style & hasTitleTop) {
		lpwndpos->cy+=((2)*((lpwndpos->cy-2)*ratio))/100+(style & border ? (style & border3D ? 2 : 4) : 6);
		}
	CStatic::OnWindowPosChanging(lpwndpos);
	}

#endif



/////////////////////////////////////////////////////////////////////////////
// CVUMeter

CVUMeter::CVUMeter() {

	style=0;
	brightness=100;
	*m_Caption=0;
	value=0;
	minVal=0;
	maxVal=10;
	numLed=10;		// tipo LM3915
	}

CVUMeter::~CVUMeter() {
	}


BEGIN_MESSAGE_MAP(CVUMeter, CStatic)
	//{{AFX_MSG_MAP(CVUMeter)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVUMeter message handlers
//https://forums.codeguru.com/showthread.php?445227-RESOLVED-VU-Meter directX

BOOL CVUMeter::Create(LPSTR lpszCaption, short unsigned int st, const RECT& rect, CWnd* pParentWnd, 
											UINT nID, int v1,int v2) {
	int i;
	TCHAR myBuf[16];
	RECT myRect=rect;
	CBrush *oldBrush;
	CDC *dc;
	
	style=st;
//	BDisplay.LoadBitmap(style & ledOrLCD ? IDB_DISPLAY_LCD : IDB_DISPLAY);
	i=CStatic::Create(myBuf, (style & noClipSiblings ? 0 : WS_CLIPSIBLINGS) | WS_VISIBLE | WS_CHILD /*| WS_BORDER*/, myRect, pParentWnd, nID);
	if(lpszCaption)
		strcpy(m_Caption,lpszCaption);
	SetRange(v1,v2);

	bkBrush.CreateSolidBrush(style & redOrGreen ? RGB(0,64,0) : RGB(64,0,0));

	return i;
	}

void CVUMeter::OnPaint() {
	CPaintDC dc(this); // device context for painting
	
	char myBuf[64];
	char *p;
	int x,y,n;
	int xSize,ySize,xSize0,ySize0;
	RECT myRect;
	CBrush *oldBrush;
	CPen *oldPen;
	if(style & digitalOrAnalog) {
		int x2,y2;

		CBrush brush1(RGB(128,128,128));
		CPen pen1(PS_SOLID,2,style & redOrGreen ? RGB(0,192,0) : RGB(192,0,0));
		CBrush brush2(RGB(144,144,144));
		CPen pen2(PS_SOLID,1,RGB(64,64,64));

		GetClientRect(&myRect);
		oldBrush=dc.SelectObject(&brush1);
		oldPen=dc.SelectObject(&pen2);
		dc.Rectangle(&myRect);
		dc.SelectObject(&brush2);
		x=myRect.right/2;
		x2=myRect.right/10;
		y2=myRect.bottom/10;

#ifndef _WIN32_WCE
		if(style & linearOrLogarithmic) {

			dc.Chord(x-x2,myRect.bottom-(myRect.bottom/5),x+x2,myRect.bottom,x-x2,myRect.bottom,x+x2,myRect.bottom);
			dc.Arc(myRect.left,myRect.top,myRect.right,myRect.bottom,x2,y2,myRect.right-x2,myRect.bottom/4);
			}
		else {
			dc.Chord(x-x2,myRect.bottom-(myRect.bottom/5),x+x2,myRect.bottom,x-x2,myRect.bottom,x+x2,myRect.bottom);
			dc.Arc(myRect.left,myRect.top,myRect.right,myRect.bottom,x2,y2,myRect.right-x2,myRect.bottom/4);
			}
#endif

		dc.SelectObject(&pen2);
		dc.MoveTo(x,myRect.bottom);

		// finire...
		x=x2+sin(value/(maxVal-minVal)*PI)*(myRect.right-x2);
		y=y2+cos(value/(maxVal-minVal)*PI)*(myRect.bottom/4);
		dc.LineTo(x,y);

		}
	else {
		CBrush brush1(style & redOrGreen ? RGB(0,192,0) : RGB(192,0,0));
		CPen pen1(PS_SOLID,1,style & redOrGreen ? RGB(0,128,0) : RGB(128,0,0));
		CBrush brush2(style & redOrGreen ? RGB(0,64,0) : RGB(64,0,0));
		CPen pen2(PS_SOLID,1,style & redOrGreen ? RGB(0,64,0) : RGB(64,0,0));

		GetClientRect(&myRect);

		if(!(style & dotOrBar)) {
			int i,x2,y2,x3;
			int steps[256 /*numLed*/];

			x2=((myRect.right-myRect.left)/numLed)-1;
			y2=(myRect.bottom-myRect.top)/3;
			x3=(maxVal-minVal)/numLed;
			if(style & linearOrLogarithmic) {
				for(i=0; i<numLed; i++) {
					steps[i]=x3*pow(2,i/3.0);	// https://forum.arduino.cc/t/map-da-lineare-a-logaritmico/902689/27
					// log(i*i)*111.08;
					// OCCHIO a usare un range limitato... p.es. con 0.10 i primi 3 led si accendono insieme (giustamente)
					// ci va almeno 40
					}
				}
			else {
				for(i=0; i<numLed; i++) {
					steps[i]=x3*i;
					}
				}
			x=1;
			oldPen=dc.SelectObject(&pen1);
			oldBrush=dc.SelectObject(&brush1);
			for(i=0; i<numLed; i++) {
				if(value>steps[i])
					dc.SelectObject(&brush1);
				else
					dc.SelectObject(&brush2);
				dc.Rectangle(x,(myRect.bottom-myRect.top)/2-y2,x+x2,(myRect.bottom-myRect.top)/2+y2);
				x+=x2+1;
				}
			}
		else {
			oldBrush=dc.SelectObject(&brush1);
			oldPen=dc.SelectObject(&pen1);
			x=(myRect.right*value)/(maxVal-minVal);
			dc.Rectangle(myRect.left,myRect.top,x,myRect.bottom);
			dc.SelectObject(&brush2);
			dc.SelectObject(&pen2);
			dc.Rectangle(x+1,myRect.top,myRect.right,myRect.bottom);
			}

		}
	
	dc.SelectObject(oldBrush);
	dc.SelectObject(oldPen);

	// Do not call CStatic::OnPaint() for painting messages
	}

void CVUMeter::SetWindowText(int n) {

	value=n;
	if(value<minVal)
		value=minVal;
	if(value>maxVal)
		value=maxVal;
	Invalidate();
	}

void CVUMeter::SetWindowText(const BYTE *p,DWORD len) {
	int i;
	DWORD n,n1=len;
	// wow, e una FFT? :) 2019 v. Bestaudioplayer

	n=0;
	while(n1--) {
		i=*p-128;
		if(i < 0)
			i=-i;
		n+=i;
		p++;
		}
	n /= len;
	SetWindowText(n);
	}

void CVUMeter::SetRange(int m,int M) {

	minVal=min(m,M);
	maxVal=max(m,M);
	}

BOOL CVUMeter::OnEraseBkgnd(CDC* pDC) {
	CBrush *oldBrush;
	RECT myRect;

	GetClientRect(&myRect);
	oldBrush=pDC->SelectObject(&bkBrush);
	pDC->Rectangle(&myRect);
	pDC->SelectObject(oldBrush);
	return 1 /*CStatic::OnEraseBkgnd(pDC)*/;
	}




#ifndef _WIN32_WCE


//                                                                -*- C++ -*-
// ==========================================================================
//!
//! \file LedButton.cpp
//!
//! \brief Implementation of the CLedButton Class.
//!
//! \author 
//!    Ricky Marek <A HREF="mailto:ricky.marek@gmail.com">ricky.marek@gmail.com</A>
//!
//!	\par Disclaimer
//!    This code and the accompanying files are provided <b>"as is"</b> with
//!    no expressed  or  implied warranty.  No responsibilities  for possible
//!    damages, or side effects in its functionality.  The  user must assume 
//!    the entire risk of using this code.  The author accepts no  liability
//!    ifit causes any damage to your computer, causes your pet to fall ill, 
//!    increases baldness or makes your car  start  emitting  strange noises 
//!    when you start it up.  <i>This code  has no bugs,  just  undocumented 
//!    features!.</i>
//!
//! \par Terms of use
//!    This code is <b>free</b> for personal use, or freeware applications as
//!    long as this comment-header  header remains like this.  ifyou plan to 
//!    use  this  code in  a commercial  or shareware  application,   you are 
//!    politely  asked  to  contact the author for his permission via e-mail. 
//!    From: <A HREF="mailto:ricky.marek@gmail.com">ricky.marek@gmail.com</A>
//!
//! \par Attributes
//!    \li \b Created       16/Aug/2002
//!    \li \b Last-Updated  16/Dec/2004
//!    \li \b Compiler      Visual C++
//!    \li \b Requirements  Win98/Win2k or later, MFC.
//!    \li \b Tested        with Visual C++ 6.0 and 7.1(.NET 2003)
//!
//!
// ==========================================================================


// ///////////////////////////////////////////////////////////////////////////
// Header Files
// ///////////////////////////////////////////////////////////////////////////
//#include "StdAfx.h" // should be the 1st include ...



#ifndef BS_TYPEMASK
# define BS_TYPEMASK  0x0000000F
#endif// BS_TYPEMASK

// ###########################################################################
// ##                                                                       ##
// ##                   C L e d B u t t o n   C l a s s                     ##
// ##                                                                       ##
// ###########################################################################

// ///////////////////////////////////////////////////////////////////////////
// Default Constructor (Public)
// ///////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CLedButton, CButton)
CLedButton::CLedButton() {

    m_ledDataList.RemoveAll();

    m_listCount        = 0;
    m_ledState         = LED_BUTTON_STATE_OFF;
    m_isDisabled       = FALSE;
    m_buttonStyle      = BS_TYPEMASK;
    m_activityTimer    = NULL;
    m_activityDuration = LED_BUTTON_DEFAULT_ACTIVITY_DURATION;
    m_activityId       = 0;
    m_activityState    = LED_BUTTON_STATE_OFF;
    m_pCondition       = NULL;

    SetLedStatesNumber(LED_BUTTON_PREDEFINED_STATES_NUMBER, false);
	}


// ///////////////////////////////////////////////////////////////////////////
// Default Destructor (Public)
// ///////////////////////////////////////////////////////////////////////////
CLedButton::~CLedButton() {

    for(LedState ledState=0; ledState < m_listCount; ledState++) {
        RemoveIcon(ledState);
    }
    
    if(m_activityTimer) {
        KillTimer(m_activityTimer);
        m_activityTimer = NULL;
    }

    m_ledDataList.RemoveAll();
    m_listCount = 0;

	}


// ///////////////////////////////////////////////////////////////////////////
// Message Map
// ///////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CLedButton, CButton)
    ON_WM_DESTROY()
    ON_WM_SYSCOLORCHANGE()
    ON_MESSAGE(BM_SETSTYLE, OnSetStyle)
    ON_WM_CTLCOLOR_REFLECT()
    ON_WM_TIMER()
END_MESSAGE_MAP()


// ###########################################################################
// ##                                                                       ##
// ##               L E D   S T A T E   A T T R I B U T E S                 ##
// ##                                                                       ##
// ###########################################################################

// ///////////////////////////////////////////////////////////////////////////
// SetLedStatesNumber (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::SetLedStatesNumber(int maxLedStatesNumber, bool isInvalidate /*=true*/) {

    ASSERT(maxLedStatesNumber > 0);
	LedState ledState;

    for(ledState=0; ledState < m_listCount; ledState++) {
        RemoveIcon(ledState);
    }

    m_ledDataList.RemoveAll();
    m_listCount = maxLedStatesNumber;
    m_ledDataList.SetSize(maxLedStatesNumber);

    LedData *pData = m_ledDataList.GetData();
    
    for(ledState=0; ledState < m_listCount; ledState++) {
        ::ZeroMemory(&(pData[ledState]), sizeof(LedData));

    }

    RestoreDefaultColors(isInvalidate);

	}



// ///////////////////////////////////////////////////////////////////////////
// SetLedState (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::SetLedState(LedState ledState, bool isForcedChange /*=false*/) {

    if(m_pCondition) {
        ledState = m_pCondition->ChangeState(ledState, m_ledState, isForcedChange);
    }

    //
    // the following is added to remove flickering when the led is called
    // with the same led state as it is displaying.
    //
    if(ledState == m_ledState) {
        return;
    }

    if(ledState < m_listCount) {
        m_ledState = ledState;
    }

    if(m_activityId) {
        //
        // Kill Timer ifrunning.
        //
        if(m_activityTimer) {
            KillTimer(m_activityTimer);
            m_activityTimer = NULL;

        }
     
        if(m_ledState != m_activityState) {
            m_activityTimer = SetTimer(m_activityId, m_activityDuration, NULL);
        }
		  }

    Invalidate();

	}


// ///////////////////////////////////////////////////////////////////////////
// SetLedActivityTimer (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::SetLedActivityTimer(UINT timerId, 
                                     int duration /* =LED_BUTTON_DEFAULT_ACTIVITY_DURATION */, 
                                     LedState offState /*=LED_BUTTON_STATE_OFF*/) {

    if(m_activityTimer) {
        KillTimer(m_activityTimer);
        m_activityTimer = NULL;
    }

    m_activityId       = timerId;
    m_activityDuration = duration;
    m_activityState    = offState;

	}



// ///////////////////////////////////////////////////////////////////////////
// SetLedStateCondition (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::SetLedStateCondition(CLedStateCondition *pCondition) {

    m_pCondition = pCondition;
    SetLedState(m_ledState, true);
}


// ###########################################################################
// ##                                                                       ##
// ##                    I C O N   O P E R A T I O N S                      ##
// ##                                                                       ##
// ###########################################################################

// ///////////////////////////////////////////////////////////////////////////
// SetIcon(1) (Public)
// ///////////////////////////////////////////////////////////////////////////
bool CLedButton::SetIcon(LedState ledState, UINT iconId, int width/*=0*/, int height/*=0*/) {

    ASSERT(ledState < m_listCount);  // No room for this Led?.

    if(ledState < m_listCount) {
	    HINSTANCE hInstResource = AfxFindResourceHandle(MAKEINTRESOURCE(iconId), RT_GROUP_ICON);
        HICON     hIcon = (HICON)::LoadImage(hInstResource, MAKEINTRESOURCE(iconId), IMAGE_ICON, width,height,0);

        return(SetIcon(ledState, hIcon));

    }

    return(false);

}

// ///////////////////////////////////////////////////////////////////////////
// SetIcon(2) (Public)
// ///////////////////////////////////////////////////////////////////////////
bool CLedButton::SetIcon(LedState ledState, HICON hIcon) {

    ASSERT(ledState < m_listCount);  // No room for this Led?

    if(ledState < m_listCount) {
        RemoveIcon(ledState);

        if(hIcon) {
      	    ICONINFO	iconInfo;

            m_ledDataList[ledState].hIcon = hIcon;

		    ::ZeroMemory(&iconInfo, sizeof(ICONINFO));

		    if(FALSE == ::GetIconInfo(hIcon, &iconInfo))
            {
                RemoveIcon(ledState);
                return(false);
            }

		    m_ledDataList[ledState].width  = iconInfo.xHotspot * 2;
		    m_ledDataList[ledState].height = iconInfo.yHotspot * 2;

		    ::DeleteObject(iconInfo.hbmMask);
		    ::DeleteObject(iconInfo.hbmColor);

        }
    }

    return(ledState < m_listCount);


}

// ///////////////////////////////////////////////////////////////////////////
// SetIcons (Public)
// ///////////////////////////////////////////////////////////////////////////
bool CLedButton::SetIcons(UINT offIconId, UINT onIconId) {
    bool retVal = true;  // Optimistic..

    if((offIconId > 0) && (m_listCount > 0))
    {
        retVal = SetIcon(LED_BUTTON_STATE_OFF, offIconId);
    }

    if((retVal) && (onIconId > 0) && (m_listCount > 0))
    {
        retVal = SetIcon(LED_BUTTON_STATE_ON, onIconId);
    }

    return(retVal);

}

// ///////////////////////////////////////////////////////////////////////////
// RemoveIcon (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::RemoveIcon(LedState ledState) {

    ASSERT(ledState < m_listCount);  // No room for this Led?

    if(ledState < m_listCount) {
        if(m_ledDataList[ledState].hIcon) {
            ::DestroyIcon(m_ledDataList[ledState].hIcon);
            m_ledDataList[ledState].hIcon = NULL;
        }

        m_ledDataList[ledState].height = 0;
        m_ledDataList[ledState].width = 0;
    
    }

}


// ###########################################################################
// ##                                                                       ##
// ##     T E X T   A T T R I B U T E S   A N D   O P E R A T I O N S       ##
// ##                                                                       ##
// ###########################################################################


// ///////////////////////////////////////////////////////////////////////////
// RestoreDefaultColors (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::RestoreDefaultColors(bool isInvalidate /* =true */) {

    for(LedState ledState = 0; ledState < m_listCount; ledState++) {
        m_ledDataList[ledState].textForegroundColor = ::GetSysColor(COLOR_BTNTEXT);
        m_ledDataList[ledState].textBackgroundColor = ::GetSysColor(COLOR_BTNFACE);
    }

	if(isInvalidate) {
        Invalidate();
    }
}


// ///////////////////////////////////////////////////////////////////////////
// SetTextForeground (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::SetTextForeground(LedState ledState, COLORREF colorRef, bool isInvalidate /*=true*/) {

	ASSERT(ledState < m_listCount);  // No room for this Led?

    if(ledState < m_listCount) {
        m_ledDataList[ledState].textForegroundColor = colorRef;
    }

    if(isInvalidate) {
        Invalidate();
    }

}


// ///////////////////////////////////////////////////////////////////////////
// SetTextBackground (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::SetTextBackground(LedState ledState, COLORREF colorRef, bool isInvalidate /*=true*/) {

    ASSERT(ledState < m_listCount);  // No room for this Led?

    if(ledState < m_listCount) {
        m_ledDataList[ledState].textBackgroundColor = colorRef;
    }

    if(isInvalidate)
    {
        Invalidate();
    }

 
}


// ///////////////////////////////////////////////////////////////////////////
// SetTextColors (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::SetTextColors(LedState ledState, COLORREF fgColorRef, COLORREF bgColorRef, bool isInvalidate /*=true*/) {

    ASSERT(ledState < m_listCount);  // No room for this Led?

    CLedButton::SetTextBackground(ledState, bgColorRef, false);
    CLedButton::SetTextForeground(ledState, fgColorRef, isInvalidate);
}


// ///////////////////////////////////////////////////////////////////////////
// GetTextForegroundColor (Public)
// ///////////////////////////////////////////////////////////////////////////
COLORREF CLedButton::GetTextForegroundColor(LedState ledState) {

    ASSERT(ledState < m_listCount);  // No room for this Led?

    if(ledState < m_listCount) {
        return(m_ledDataList[ledState].textForegroundColor);
    }

    return(::GetSysColor(COLOR_BTNTEXT));
}


// ///////////////////////////////////////////////////////////////////////////
// GetTextBackgroundColor (Public)
// ///////////////////////////////////////////////////////////////////////////
COLORREF CLedButton::GetTextBackgroundColor(LedState ledState) {

    ASSERT(ledState < m_listCount);  // No room for this Led?

    if(ledState < m_listCount) {
        return(m_ledDataList[ledState].textBackgroundColor);
    }

    return(::GetSysColor(COLOR_BTNFACE));
}


// ###########################################################################
// ##                                                                       ##
// ##                      T O O L T I P   S T U F F                        ##
// ##                                                                       ##
// ###########################################################################


// ///////////////////////////////////////////////////////////////////////////
// ToolTipInit (Private)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::ToolTipInit(void) {

	if(!m_toolTip.m_hWnd)	{
		m_toolTip.Create(this);
		m_toolTip.Activate(FALSE);
		m_toolTip.SendMessage(TTM_SETMAXTIPWIDTH, 0, 400);
	}

}


// ///////////////////////////////////////////////////////////////////////////
// SetTooltipText(1) (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::SetTooltipText(UINT id, bool isActivate /*=true*/) {
   	CString text;
	
    if(text.LoadString(id)) {
        SetTooltipText(text, isActivate);
    }

}
// ///////////////////////////////////////////////////////////////////////////
// SetTooltipText(2) (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::SetTooltipText(LPCTSTR text, bool isActivate /*=true*/) {

	if(!text)
    {
        return;
    }

	ToolTipInit();

	if(0 == m_toolTip.GetToolCount())
	{
		CRect rect; 
		GetClientRect(rect);
		m_toolTip.AddTool(this, text, rect, 1);
	} 

	m_toolTip.UpdateTipText(text, this, 1);
	m_toolTip.Activate(isActivate);

}

// ///////////////////////////////////////////////////////////////////////////
// ActivateTooltip (Public)
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::ActivateTooltip(bool isActivate /*=true*/) {

	if(m_toolTip.GetToolCount()) {
    	m_toolTip.Activate(isActivate);
    }

    return;

}


// ###########################################################################
// ##                                                                       ##
// ##                   M E S S A G E   H A N D L E R S                     ##
// ##                                                                       ##
// ###########################################################################

// ///////////////////////////////////////////////////////////////////////////
// OnDestroy
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::OnDestroy() {

    CButton::OnDestroy();

    //
    // Kill timer
    //
    if(m_activityTimer) {
        KillTimer(m_activityTimer);
        m_activityTimer = NULL;
    }

}



// ///////////////////////////////////////////////////////////////////////////
// DrawItem
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::DrawItem(LPDRAWITEMSTRUCT lpDIS) {
   	CDC* pDC = CDC::FromHandle(lpDIS->hDC);

	m_isDisabled = ((lpDIS->itemState & ODS_DISABLED) != 0);

	CRect itemRect = lpDIS->rcItem;

	pDC->SetBkMode(TRANSPARENT);

	OnDrawBackground(pDC, &itemRect);

    CRect captionRect = lpDIS->rcItem;
    CRect ledRect;
  
    //
    // Now the cases are:
    // - BS_LEFTTEXT: The text appears on the left side of the LED icon.( true/false)
    //
    // - BS_LEFT:   Text will be left aligned.
    // - BS_RIGHT:  Text will be right aligned.
    // - BS_CENTER: Text will be center
    //
    LedData ledData;
    ::ZeroMemory(&ledData, sizeof(LedData));
    ledData.textForegroundColor = ::GetSysColor(COLOR_BTNTEXT);
    ledData.textBackgroundColor = ::GetSysColor(COLOR_BTNFACE);

    if(m_ledState < m_listCount) {
        ledData = m_ledDataList[m_ledState];
    }

    if(m_buttonStyle & BS_LEFTTEXT) {
        ledRect.left      = captionRect.right - ledData.width;
        ledRect.right     = ledRect.left + ledData.width;
        captionRect.right = ledRect.left - 3;
    }
    else {
        ledRect.left     = captionRect.left;
        ledRect.right    = ledRect.left + ledData.width;
        captionRect.left = ledRect.right + 3;
    }

    ledRect.top    = (captionRect.Height() - ledData.height)/2;
    ledRect.bottom = ledRect.top + ledData.height;

    if(ledData.hIcon)  {
        pDC->DrawState(ledRect.TopLeft(), 
                       ledRect.Size(),  
                       ledData.hIcon, 
                       (m_isDisabled ? DSS_DISABLED : DSS_NORMAL), 
                       (CBrush*)NULL);

    }

    //
    // Now deal with the text.(ifany)
    //
	CString title;
	GetWindowText(title);

    if(!title.IsEmpty())     {
        CRect centerRect = captionRect;
        UINT textFormat = DT_WORDBREAK | DT_VCENTER;

        if(BS_CENTER == (m_buttonStyle &  BS_CENTER))        {
            textFormat |= DT_CENTER;
            pDC->DrawText(title, -1, &captionRect, (textFormat | DT_CALCRECT));
            captionRect.OffsetRect((centerRect.Width() - captionRect.Width())/2, 
                                   ((centerRect.Height() - captionRect.Height())/2));

        }
        else if(BS_RIGHT == (m_buttonStyle & BS_RIGHT)) {
            textFormat |= DT_RIGHT;
            pDC->DrawText(title, -1, &captionRect, (textFormat | DT_CALCRECT));
            captionRect.OffsetRect((centerRect.Width() - captionRect.Width()), 
                                   ((centerRect.Height() - captionRect.Height())/2));
        }
        else {
            textFormat |= DT_LEFT;
            pDC->DrawText(title, -1, &captionRect, (textFormat | DT_CALCRECT));
            captionRect.OffsetRect(0, ((centerRect.Height() - captionRect.Height())/2));

        }

	    pDC->SetBkMode(TRANSPARENT);

        if(m_isDisabled) {
            captionRect.OffsetRect(1, 1);
            pDC->SetTextColor(::GetSysColor(COLOR_3DHILIGHT));
            pDC->DrawText(title, -1, &captionRect, textFormat);
            captionRect.OffsetRect(-1, -1);
            pDC->SetTextColor(::GetSysColor(COLOR_3DSHADOW));
            pDC->DrawText(title, -1, &captionRect, textFormat);
        }
        else
        {
            pDC->SetTextColor(ledData.textForegroundColor);
            pDC->SetBkColor(ledData.textForegroundColor);
            pDC->DrawText(title, -1, &captionRect, textFormat);
        }

    }

}



// ///////////////////////////////////////////////////////////////////////////
// OnTimer
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::OnTimer(UINT nIDEvent) {

    if((nIDEvent == m_activityId) && (0 != m_activityId))
    {
        SetLedState(m_activityState);
        return;
    }

    CButton::OnTimer(nIDEvent);
}

// ///////////////////////////////////////////////////////////////////////////
// OnSysColorChange
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::OnSysColorChange() {
    CWnd::OnSysColorChange();
    
    RestoreDefaultColors();
}


// ///////////////////////////////////////////////////////////////////////////
// OnDrawBackground
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::OnDrawBackground(CDC* pDC, CRect* pRect) {
    ASSERT(pDC);
    ASSERT(pRect);

    COLORREF color = GetTextBackgroundColor(m_ledState);
	CBrush brBackground(color);
	pDC->FillRect(pRect, &brBackground);
}


// ///////////////////////////////////////////////////////////////////////////
// OnSetStyle
// ///////////////////////////////////////////////////////////////////////////
LRESULT CLedButton::OnSetStyle(WPARAM wParam, LPARAM lParam) {
	return(DefWindowProc(BM_SETSTYLE, (wParam & ~BS_TYPEMASK) | BS_OWNERDRAW, lParam));
}


// ///////////////////////////////////////////////////////////////////////////
// PreTranslateMessage
// ///////////////////////////////////////////////////////////////////////////
BOOL CLedButton::PreTranslateMessage(MSG* pMsg) {

	ToolTipInit();
	m_toolTip.RelayEvent(pMsg);
	
    return(CButton::PreTranslateMessage(pMsg));
}

//
// To avoid a  warning C4189: 'style' : local variable is initialized but not referenced
// in the PreSubclassWindow method on Level4 Warnings/Release configuration.
// Reason: The ASSERT lines do not exist under Release. therefore the 'style' variable
// is never used.
#pragma warning( push )
#pragma warning( disable : 4189 )


// ///////////////////////////////////////////////////////////////////////////
// PreSubclassWindow
// ///////////////////////////////////////////////////////////////////////////
void CLedButton::PreSubclassWindow() {
	UINT style = BS_TYPEMASK  & GetButtonStyle();
    
    ASSERT(style & BS_CHECKBOX);
	ASSERT(style != BS_OWNERDRAW);

	// Switch to owner-draw
	ModifyStyle(BS_TYPEMASK, BS_OWNERDRAW, SWP_FRAMECHANGED);

    m_buttonStyle = GetWindowLong(GetSafeHwnd(), GWL_STYLE);

    CButton::PreSubclassWindow();
}
//
// Undo the above warning disabling.
//
#pragma warning( pop )


// ///////////////////////////////////////////////////////////////////////////
// CtlColor
// ///////////////////////////////////////////////////////////////////////////
HBRUSH CLedButton::CtlColor(CDC* /*pDC*/, UINT /*CtlColor*/) {

	return ((HBRUSH)::GetStockObject(NULL_BRUSH)); 
}


#endif
