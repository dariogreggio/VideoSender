#include "stdafx.h"
#include "cgif.h"

#pragma intrinsic(memcmp)

// NON e' CHIARO COSA FARE CON LE GIF DA 1 BIT ???

/*
Michael A. Mayer / mayer@aol.com 
#include <stdio.h>

#define BLOKLEN 255
#define BUFLEN 1000

typedef struct _tn {
  int code;
  struct _tn **node;
} TreeNode,*TreePtr;

char    *AddCodeToBuffer(int code,short n,char *buf);
TreePtr InitIndices(TreePtr first,int cc);
TreePtr AddNode(int,int);
int     SearchTreeForCode(TreePtr,int *,int,int,int,int);
void    ClearTree(TreePtr,int);

main(int argc,char **argv)
{
  int     i,j,rows,cols,depth=8;
  int     raster,*pixels,**rawpxl;

  TreePtr first = NULL;
  int     len,*str,*end,*target;
  int     cc,eoi,code,next;
  short   nBits;

  char    *pos,*buffer;

  char    cmd[100],filename[100];
  FILE    *src,*dst;

  buffer = (char *)malloc((BUFLEN+1)*sizeof(char))+1;

  if(argc!=2) {
    printf("\nUsage:: %s pixelfile\n\n",argv[0]);
    exit(1);
  }

  src = fopen(argv[1],"r");
  if(!src) {
    printf("\nError:: Couldn't open %s\n",argv[1]);
    exit(1);
  }

  _tcscpy(filename,argv[1]);
  _tcscat(filename,".gif");
  dst = fopen(filename,"r");
  if(dst) {
    fclose(dst);
    printf("\nSorry:: %s will not overwrite %s\n\n",argv[0],filename);
    exit(1);
  }
  dst = fopen(filename,"w");
  
  if(fscanf(src,"%d %d",&rows,&cols)!=2) {
    fclose(dst);fclose(src);
    printf("\nError:: %s is apparently not a pixel file\n\n",argv[1]);
    sprintf(cmd,"rm %s",filename);
    system(cmd);
    exit(1);
  }

  pixels = (int *)malloc(rows*cols*sizeof(int));
  rawpxl = (int **)malloc(rows*sizeof(int *));
  raster=0;
  for(i=0;i<rows;i+=8) rawpxl[i] = pixels + cols * (raster++);
  for(i=4;i<rows;i+=8) rawpxl[i] = pixels + cols * (raster++);
  for(i=2;i<rows;i+=4) rawpxl[i] = pixels + cols * (raster++);
  for(i=1;i<rows;i+=2) rawpxl[i] = pixels + cols * (raster++);

  
  for(i=0;i<rows;i++)
    for(j=0;j<cols;j++) {
      if(fscanf(src,"%d",rawpxl[i]+j)!=1) {
        fclose(dst);fclose(src);
        printf("\nError:: %s is apparently not a pixel file\n\n",argv[1]);
        sprintf(cmd,"rm %s",filename);
        system(cmd);
        exit(1);
      }
    }
  fclose(src);
  
  pos = buffer;

  *pos++ = 'G';
  *pos++ = 'I';
  *pos++ = 'F';
  *pos++ = '8';
  *pos++ = '7';
  *pos++ = 'a';
  
  *pos++ = 0xff & cols;
  *pos++ = (0xff00 & cols)/0x100;
  *pos++ = 0xff & rows;
  *pos++ = (0xff00 & rows)/0x100;
  *pos++ = 0xf0 | (0x7&(depth-1));
  *pos++ = 0xff;
  *pos++ = 0x0;

  for(i=0;i<256;i++) {
    *pos++ = 0xff & i;
    *pos++ = 0xff & i;
    *pos++ = 0xff & i;
  }    
  *pos++ = 0x2c;
  *pos++ = 0x00;
  *pos++ = 0x00;
  *pos++ = 0x00;
  *pos++ = 0x00;
  *pos++ = 0xff & cols;
  *pos++ = (0xff00 & cols)/0x100;
  *pos++ = 0xff & rows;
  *pos++ = (0xff00 & rows)/0x100;
  *pos++ = 0x40 | 0x7&(depth-1);
  *pos++ = (depth==1)?2:depth;

  fwrite(buffer,pos-buffer,1,dst);
  pos = buffer;
  buffer[0]=0x0;
  
  cc = (depth==1)?0x4:1<<depth;
  eoi = cc+1;
  next = cc+2;

  nBits = (depth==1)?3:depth+1;

  first = InitIndices(first,cc);

  pos = AddCodeToBuffer(cc,nBits,pos);

  len=1; str=pixels; end = pixels+rows*cols;
  target = str+rows*cols/10;
  while(str+len<end) {

    if(str+len>target) {
      printf("(%3d,%3d): %3x\n",(str-pixels)/cols,(str-pixels)%cols,next);
      target += rows*cols/10;
    }

    code = SearchTreeForCode(first,str,len,next,cc,0);

    if(code!=-1) {

      pos = AddCodeToBuffer(code,nBits,pos);
      if(pos-buffer>BLOKLEN) {
        buffer[-1] = BLOKLEN;
        fwrite(buffer-1,BLOKLEN+1,1,dst);
        buffer[0]=buffer[BLOKLEN];
        buffer[1]=buffer[BLOKLEN+1];
        buffer[2]=buffer[BLOKLEN+2];
        buffer[3]=buffer[BLOKLEN+3];
        pos -= BLOKLEN;
      }
      str += len-1;
      len = 0;

      if(next==(1<<nBits)) nBits++;
      next++;

      if(next==0xfff) {
        first = InitIndices(first,cc);
        pos = AddCodeToBuffer(cc,nBits,pos);
        if(pos-buffer>BLOKLEN) {
          buffer[-1] = BLOKLEN;
          fwrite(buffer-1,BLOKLEN+1,1,dst);
          buffer[0]=buffer[BLOKLEN];
          buffer[1]=buffer[BLOKLEN+1];
          buffer[2]=buffer[BLOKLEN+2];
          buffer[3]=buffer[BLOKLEN+3];
          pos -= BLOKLEN;
        }
        next = cc+2;
        nBits = (depth==1)?3:depth+1;
      }
    }
    len++;
  }

  code = SearchTreeForCode(first,str,len,next,cc,1);
  pos = AddCodeToBuffer(code,nBits,pos);
  pos = AddCodeToBuffer(eoi,nBits,pos);
  pos = AddCodeToBuffer(0x0,-1,pos);
  buffer[-1] = pos-buffer;
  pos = AddCodeToBuffer(0x0,8,pos);
  pos = AddCodeToBuffer(0x3b,8,pos);

  fwrite(buffer-1,pos-buffer+1,1,dst);

  fclose(dst);
}


TreePtr InitIndices(TreePtr first,int cc)
{
  int i;

  ClearTree(first,cc);
  
  first = AddNode(-1,cc);
  for(i=0;i<cc;i++) first->node[i] = AddNode(i,cc);

  return first;
}

TreePtr AddNode(int code,int n)
{
  TreePtr node;
  node = (TreePtr)malloc(sizeof(TreeNode));
  node->code = code;
  node->node = (TreePtr *)malloc(n*sizeof(TreePtr));

  do {
    node->node[--n] = NULL;
  } while (n);

  return node;
}

void ClearTree(TreePtr node,int n)
{
  int i;
  
  if(node==NULL) return;

  for(i=0;i<n;i++) ClearTree(node->node[i],n);
  free(node->node);
  free(node);
}

char *AddCodeToBuffer(int code,short n,char *buf)
{
  static short need = 8;
  int    mask;

  if(n<0) {
    if(need<8) {
      buf++;
      *buf = 0x0;
    }
    need = 8;
    return buf;
  }

  while(n>=need) {
    mask = (1<<need)-1;
    *buf += (mask&code)<<(8-need);
    buf++;
    *buf = 0x0;
    code = code>>need;
    n -= need;
    need = 8;
  }
  if(n) {
    mask = (1<<n)-1;
    *buf += (mask&code)<<(8-need);
    need -= n;
  }
  return buf;
}

int SearchTreeForCode(TreePtr node,int *str,int len,int next,int n,int done)
{
  if(len==0) {
    if(done) return node->code;
    return -1;
  }

  if(node->node[str[0]])
    return SearchTreeForCode(node->node[str[0]],str+1,len-1,next,n,done);
  else {
    node->node[str[0]] = AddNode(next,n);
    return node->code;
  }
}
*/


CGif::CGif(const char *n,int nBits) {

	LZWbuffer=(char *)GlobalAlloc(GPTR,32768);
	LZWtable=(char **)GlobalAlloc(GPTR,MAX_LZW_ENTRIES*sizeof(char *));
	MyChars=(struct LZWstring *)GlobalAlloc(GPTR,2048);
	if(n)
		_tcsncpy(appName,n,63);
	else
		*appName=0;
	InitializeLZWTable(nBits);
	}

CGif::~CGif() {

	GlobalFree(MyChars);
	GlobalFree(LZWbuffer);
	GlobalFree(LZWtable);
	}

BYTE *CGif::buildGIFHdr(BITMAP *bmp,RECT *rc,BYTE *d,WORD nBits,RGBQUAD *c,DWORD bTransparency) {
	register int i,j;
	struct GIF_HEADER gfh;
	struct GIF_HEADER_PACK gfh_p;
	struct GIF_IMAGE_DESCRIPTOR gfid;
	struct GIF_IMAGE_DESCRIPTOR_PACK gfid_p;
	struct GIF_CONTROL_EXTENSION gce;
	struct GIF_CONTROL_EXTENSION_PACK gce_p;

	_tcscpy(gfh.Signature,"GIF");
	_tcscpy(gfh.Version,"89a");
	gfh.Width=(WORD)rc->right;
	gfh.Height=(WORD)rc->bottom;
	gfh_p.GlobalColorTable=1;
	gfh_p.ColorResolution=nBits-1;
	gfh_p.SortFlag=0;
	gfh_p.SizeOfGlobalColorTable=nBits-1;
	gfh.packed=*(BYTE *)&gfh_p;
	gfh.BkColor=0;
	gfh.PixelAspect=0;
	memcpy(d,&gfh,sizeof(gfh));
	d+=sizeof(gfh);
	switch((long)c) {
		case 0:					// system
			{
			LPLOGPALETTE lpal;
			HDC hDC;
			hDC=GetDC(GetDesktopWindow() /*theApp.m_pMainWnd->m_hWnd*/);
			lpal=(LPLOGPALETTE)GlobalAlloc(GPTR,2000);
			if(lpal) {
				j=GetSystemPaletteEntries(hDC,0,256,lpal->palPalEntry);
				lpal->palVersion=0x300;
				lpal->palNumEntries=j;
				for(i=0; i<j; i++) {
					*d++=lpal->palPalEntry[i].peRed;
					*d++=lpal->palPalEntry[i].peGreen;
					*d++=lpal->palPalEntry[i].peBlue;
					}
				for(; i < (1 << nBits); i++) {
					*d++=i;
					*d++=i;
					*d++=i;
					}
				GlobalFree(lpal);	
				}
			ReleaseDC(GetDesktopWindow() /*theApp.m_pMainWnd->m_hWnd*/ ,hDC);
			}
			break;
		case -1:				// grigi
			for(i=0; i<(1 << nBits); i++) {
				*d++=i;
				*d++=i;
				*d++=i;
				}
			break;
		default:
			j=1 << nBits;
			for(i=0; i<j; i++) {
				*d++=c[i].rgbRed;
				*d++=c[i].rgbGreen;
				*d++=c[i].rgbBlue;
				}
			for(; i < (1 << nBits); i++) {
				*d++=i;
				*d++=i;
				*d++=i;
				}
			break;
		}

	gce.Introducer=0x21;
	gce.ControlLabel=0xf9;
	gce.Size=4;
	gce_p.Reserved=0;
	gce_p.DisposalMethod=0;
	gce_p.UserInput=0;
	gce_p.TransparentColor=LOWORD(bTransparency);
	gce.packed=*(BYTE *)&gce_p;
	gce.DelayTime=0;
	gce.TransparencyIndex=HIWORD(bTransparency);
	gce.Terminator=0;
	memcpy(d,&gce,sizeof(gce));
	d+=sizeof(gce);

	*d++=0x21;
	*d++=GIF_BYTE_COMMENT_EXTENSION;
	*d++=_tcslen(appName)+1;
	_tcscpy((char *)d,appName);
	d+=_tcslen(appName)+1;
	*d++=0;

	gfid.ImageSeparator=0x2c;
	gfid.LeftPos=0;
	gfid.TopPos=0;
	gfid.Width=(WORD)rc->right;
	gfid.Height=(WORD)rc->bottom;
	gfid_p.LocalColorTable=0;
	gfid_p.Interlace=0;
	gfid_p.Sort=0;
	gfid_p.Reserved=0;
	gfid.packed=*(BYTE *)&gfid_p;
	memcpy(d,&gfid,sizeof(gfid));
	d+=sizeof(gfid);

	*d++=nBits;			 // dimensione primo codice LZW (sarebbe legato a bitsPerPixel...)

	return d;		// ritorna il PUNTATORE al primo byte buono!
	}

RGBQUAD *CGif::CreateTruePalette(CBitmap *b,RGBQUAD *r) {	// ritorna una palette il piu' possibile aderente all'originale (NON c'e' in Windows: GetDIBits fa un lavoro cosi' cosi'...)
	BITMAP bmp;

	b->GetBitmap(&bmp);
	switch(bmp.bmBitsPixel) {
		case 1:
			break;
		case 4:
			break;
		case 8:
			break;
		case 16:
			break;
		case 24:
			break;
		}

	return r;
	}

/*char *CGif::buildGIF(BITMAP *bmp,DWORD *len,CPalette *c) {
	char *p,*s,*pSaved;
	register int i,j,x,y;
	BYTE ch;
	register unsigned int i1;
	char *d;

	d=p=(char *)GlobalAlloc(GMEM_FIXED,bmp->bmWidthBytes*bmp->bmHeight+1000+256*3+4);
	
	p=buildGIFHdr(bmp,d,bmp->bmBitsPixel,c);

	y=0;
	s=(char *)bmp->bmBits;
//	GetDIBits();

	pSaved=p++;		// qui andra' messa la dimensione del blocco LZW
	p=PutCode(p,codeClear);
	i1=MyChars->len=0;

	while(y < bmp->bmHeight) {
		j=bmp->bmWidth;
		
		while(j--) {
//        wsprintf(buffer,"LZWlast=%x, MyCharsN=%d",LZWlast,MyCharsN);
//        MessageBox(hWnd,buffer,NULL,MB_OK);
			ch=*s++;
			MyChars->str[MyChars->len++]=ch;
			i=CodeFromString(MyChars);
			if(i < 0) {
				p=PutCode(p,i1);
				if((p-pSaved) >= 255) {		 // attenzione: in caso "sforassimo" di 2...
					*(BYTE *)pSaved=p-pSaved-1;
					p[4]=p[3];
					p[3]=p[2];
					p[2]=p[1];
					p[1]=p[0];
					pSaved=p++;		// qui andra' messa la dimensione del blocco LZW
					}
				if(AddTableEntry(MyChars)<0) {
					p=PutCode(p,codeClear);
					ReinitializeLZWTable();
					if((p-pSaved) >= 255) {
						*(BYTE *)pSaved=p-pSaved-1;
						p[4]=p[3];
						p[3]=p[2];
						p[2]=p[1];
						p[1]=p[0];
						pSaved=p++;		// qui andra' messa la dimensione del blocco LZW
						}
					}
				*MyChars->str=i1=(BYTE)ch;
				MyChars->len=1;
				}
			else
				i1=i;
			}
		
		y++;
		}
	p=PutCode(p,i1);
	p=PutCode(p,codeEOF);
	*(BYTE *)pSaved=p-pSaved-1;
	*p++=0;			 // blocco a lungh. 0, fine Data Stream
	*p++=GIF_TRAILER;

	*len=p-d;
	return d;
	}*/

BYTE *CGif::buildGIF(CBitmap *bmp,RECT *rc,DWORD *len,WORD bitDepth,int reverseV,
										 CPalette *c,DWORD bTransparency) {
	BITMAP b;
	BYTE *p,*pSaved;
	char *s;
	register int i,j,x,y;
	BYTE ch;
	register unsigned int i1;
	BYTE *d=NULL;
	DWORD l;
	BITMAPINFO *bi;
	HDC dc;
	RECT myRC;

	if(bitDepth!=1 && bitDepth!=4 && bitDepth!=8)
		return NULL;

	if(!LZWtable || !LZWbuffer)
		return NULL;

	if(bi=(BITMAPINFO *)GlobalAlloc(GMEM_FIXED,sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD))) {
		bmp->GetBitmap(&b);
		if(d=p=(BYTE *)GlobalAlloc(GMEM_FIXED,b.bmWidth*b.bmHeight+1000+256*3+4)) {	// occupazione MAX del gif (stimata!)
		
			l=b.bmWidth*b.bmHeight;
			if(rc)
				myRC=*rc;
			else {
				SetRectEmpty(&myRC);
				myRC.right=b.bmWidth;
				myRC.bottom=b.bmHeight;
				}

			b.bmBits=(char *)GlobalAlloc(GMEM_FIXED,l+1024);	 // PERCHE' PATCH 2048???
		//	bmp->GetBitmapBits(l,b.bmBits);
			dc=GetDC(GetDesktopWindow()); //theApp.m_pMainWnd->GetDC();
			ZeroMemory(bi,sizeof(BITMAPINFOHEADER));
			bi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
				// a 24 non funziona, o meglio funzionerebbe se lo schermo e' a 24 anch'esso!
			i=GetDIBits(dc,(HBITMAP)*bmp,0,0,NULL,bi,DIB_RGB_COLORS);
			bi->bmiHeader.biBitCount=bitDepth;
			bi->bmiHeader.biCompression=0;
			if(reverseV)
				bi->bmiHeader.biHeight=-bi->bmiHeader.biHeight;
		//	bi->bmiHeader.biPlanes=1;
			i=GetDIBits(dc,(HBITMAP)*bmp,0,abs(bi->bmiHeader.biHeight),b.bmBits,bi,DIB_RGB_COLORS);
			ReleaseDC(GetDesktopWindow(),dc); //theApp.m_pMainWnd->ReleaseDC(dc);
		//	CreateTruePalette(bmp);

			p=buildGIFHdr(&b,&myRC,d,bitDepth /*b.bmBitsPixel*/,bi->bmiColors,bTransparency);

			s=(char *)b.bmBits;

			y=0;
			pSaved=p++;		// qui andra' messa la dimensione del blocco LZW
			p=PutCode(p,codeClear);
			i1=MyChars->len=0;

			while(y < myRC.bottom) {
				j=myRC.right;
				
				while(j--) {
		//        wsprintf(buffer,"LZWlast=%x, MyCharsN=%d",LZWlast,MyCharsN);
		//        MessageBox(hWnd,buffer,NULL,MB_OK);
					ch=*s++;
					MyChars->str[MyChars->len++]=ch;
					i=CodeFromString(MyChars);
					if(i < 0) {
						p=PutCode(p,i1);
						if((p-pSaved) >= 255) {		 // attenzione: in caso "sforassimo" di 2...
							*(BYTE *)pSaved=p-pSaved-1;
							p[4]=p[3];
							p[3]=p[2];
							p[2]=p[1];
							p[1]=p[0];
							pSaved=p++;		// qui andra' messa la dimensione del blocco LZW
							}
						if(AddTableEntry(MyChars)<0) {
							p=PutCode(p,codeClear);
							ReinitializeLZWTable();
							if((p-pSaved) >= 255) {
								*(BYTE *)pSaved=p-pSaved-1;
								p[4]=p[3];
								p[3]=p[2];
								p[2]=p[1];
								p[1]=p[0];
								pSaved=p++;		// qui andra' messa la dimensione del blocco LZW
								}
							}
						*MyChars->str=i1=(BYTE)ch;
						MyChars->len=1;
						}
					else
						i1=i;
					}
				if(b.bmWidth > myRC.right) {
					for(j=b.bmWidth - myRC.right; j; j--)
						s++;
					}
				
				y++;
				}
			p=PutCode(p,i1);
			p=PutCode(p,codeEOF);
			*(BYTE *)pSaved=p-pSaved-1;
			*p++=0;			 // blocco a lungh. 0, fine Data Stream
			*p++=GIF_TRAILER;

			GlobalFree(b.bmBits);
			}
		GlobalFree(bi);
		}
	*len=p-d;
	return d;
	}

/*char *CGif::buildGIF(CBitmap *bmp,DWORD *len,CPalette *c) {
	BITMAP myBmp;
	char *d;
	DWORD l;

	bmp->GetBitmap(&myBmp);
	l=myBmp.bmWidthBytes*myBmp.bmHeight;
	myBmp.bmBits=(char *)GlobalAlloc(GMEM_FIXED,l);
	bmp->GetBitmapBits(l,myBmp.bmBits);
	d=buildGIF(&myBmp,len,c);
	GlobalFree(myBmp.bmBits);
	return d;
	}
*/

BYTE *CGif::buildGIF(HICON hIcon,DWORD *len) {
	BYTE *p;
	RECT myRect={0,0,32,32};

	CBitmap *pBitmap,*oldB;
	CDC dc2;
	HDC dc;
	CBrush brush(RGB(0,128,128)),*oldBrush;
	CPen pen(PS_SOLID,1,RGB(0,128,128)),*oldPen;

	dc=GetDC(GetDesktopWindow()); //theApp.m_pMainWnd->GetDC();
	dc2.CreateCompatibleDC(CDC::FromHandle(dc));		// non e' necessario Delete: lo fa il distruttore!
	pBitmap=new CBitmap;
	pBitmap->CreateCompatibleBitmap(CDC::FromHandle(dc),32,32);
	oldB=dc2.SelectObject((CBitmap *)pBitmap);

	oldBrush=dc2.SelectObject(&brush);
	oldPen=dc2.SelectObject(&pen);
	dc2.Rectangle(&myRect);
	
	dc2.SelectObject(oldBrush);
	dc2.SelectObject(oldPen);

	dc2.DrawIcon(0,0,hIcon);

//	GetIconInfo(hIcon,&ii);
	p=buildGIF(pBitmap,NULL,len,8,TRUE /*FALSE*/,NULL,MAKELONG(1,6 /* il "cyan" di sfondo nell'editor di Icone MSDEV... */));
	// non è perfetto xché se usiamo quel colore... bisognerebbe inventarsi qualcosa magari con un colore + alto nella palette di sistema... che è quella usata adesso (v.gifHdr)
//	DeleteObject(ii.hbmColor);
//	DeleteObject(ii.hbmMask);

	dc2.SelectObject(oldB);

	delete pBitmap;

	ReleaseDC(GetDesktopWindow(),dc); //theApp.m_pMainWnd->ReleaseDC(dc);

	return p;
	}

BYTE *CGif::buildGIF(int bmpResource, DWORD *len) {
	CBitmap b;

	b.LoadBitmap(bmpResource);
	return buildGIF(&b,NULL,len);
	}

BYTE *CGif::readGIF(CFile *f, BITMAPINFOHEADER *bmp, BYTE *theBytes, BOOL reverseV) {
	Gif *gif;

	gif = new_gif();
	if(gif == NULL) {
		return NULL;
		}
	read_gif(f, gif);
	if(strncmp(gif->header, "GIF", 3) != 0) {
		del_gif(gif);
		gif = NULL;
		return NULL;
		}

	bmp->biSize=sizeof(BITMAPINFOHEADER);
	bmp->biBitCount=8;
	bmp->biPlanes=1			/* de-> */ ;
	bmp->biWidth=gif->screen->width;
	bmp->biHeight=gif->screen->height;
	bmp->biSizeImage=((DWORD)gif->screen->width)*gif->screen->height;
	bmp->biClrUsed=bmp->biClrImportant=1<<8;
	bmp->biCompression=0;
	bmp->biXPelsPerMeter=bmp->biYPelsPerMeter=0;
	BITMAPINFO *bi=(BITMAPINFO *)LocalAlloc(LPTR,sizeof(BITMAPINFOHEADER)+256*4);
	
  if(theBytes) {
		GlobalFree(theBytes);
		theBytes=(BYTE *)GlobalAlloc(GPTR,bmp->biSizeImage);
		}
	else
		theBytes=(BYTE *)GlobalAlloc(GPTR,bmp->biSizeImage);

	// forse si potrebbe convertire la 8bit a 24bit... usando la palette...
	//gif->blocks[0]->pic->cmap
	memcpy(theBytes,gif->blocks[0]->pic->data,bmp->biSizeImage);

	// OVVIAMENTE altrimenti così non va! FINIRE...
	memcpy(bi->bmiColors,gif->blocks[0]->pic->cmap,768);
	LocalFree(bi);

	del_gif(gif);


  return theBytes;
	}


BYTE * __fastcall CGif::PutCode(BYTE *d,WORD c) {          // al primo giro BitPos = 0x1
	register int i,j;

//  fprintf(f,"-Scrivo: %x-",c);

	j=c;
//	i=1 << (LZWwidth-1);
  i=1;
	while(i <= LZWwidBit) {
		if(j & i) {
			CH |= BitPos;
			}
		BitPos <<= 1;
		if(!(BitPos & 0xff)) {
			*d++=CH;
			BitPos=0x1;
			CH=0;
			}
		i <<= 1;
		}
	
//  if(c== 0x101) fprintf(f,"-EOI-");

	if((c == codeEOF) && (BitPos != 0x1)) {
		*d++=CH;
		BitPos=0x1;
		CH=0;
		}

	return d;
	}

unsigned int CGif::CodeFromString(struct LZWstring *str) {
	register char **p,**p1;

	if(str->len == 1) {
		return (BYTE)(str->str[0]);
		}
	else {
		p=&LZWtable[codeClear+2];
		p1=&LZWtable[LZWlast];
		while(p <= p1) {
 			if(str->len == **p) {
				if(!memcmp(*p+1,str->str,str->len)) {
					return p-LZWtable;
					}
				}
			p++;
			}
		}
	return 0xffffffff;
	}

int CGif::AddTableEntry(struct LZWstring *str) {    // len in bytes

	LZWlast++;
	LZWtable[LZWlast]=LZWbptr;
	if(LZWbptr-LZWbuffer > 32000)
		return -1;
	*LZWbptr++=str->len;
	memcpy(LZWbptr,str->str,str->len);
	LZWbptr+=str->len;
  switch(LZWlast) {
		case 4:					// non si inizia mai da meno di questo...
			LZWwidth=3;
			LZWwidBit=0x4;
			break;
		case 8:
			LZWwidth=4;
			LZWwidBit=0x8;
			break;
		case 16:
			LZWwidth=5;
			LZWwidBit=0x10;
			break;
		case 32:
			LZWwidth=6;
			LZWwidBit=0x20;
			break;
		case 64:
			LZWwidth=7;
			LZWwidBit=0x40;
			break;
		case 128:
			LZWwidth=8;
			LZWwidBit=0x80;
			break;
		case 256:
			LZWwidth=9;
			LZWwidBit=0x100;
			break;
		case 512:
			LZWwidth=10;
			LZWwidBit=0x200;
			break;
		case 1024:
			LZWwidth=11;
			LZWwidBit=0x400;
			break;
		case 2048:
			LZWwidth=12;
			LZWwidBit=0x800;
			break;
		case (MAX_LZW_ENTRIES-1):
			return -1;
			break;
    }
  return 0;
	}

int CGif::ReinitializeLZWTable() {
	register int i;
	
	LZWbptr=LZWbuffer;
	if(LZWtable && LZWbuffer) {
		for(i=0; i < (1 << bitNum); i++) {
			LZWtable[i]=LZWbptr;
			*LZWbptr++=1;
			*LZWbptr++=i;
			}
		for(i; i<MAX_LZW_ENTRIES; i++) {
			LZWtable[i]=0;
			}
		codeClear=1 << bitNum;
		LZWwidBit=1 << bitNum;
		codeEOF=codeClear+1;
		LZWwidth=bitNum+1;
		LZWlast=codeClear+1;
		return 0;
		}
	else
		return -1;
	}

int CGif::InitializeLZWTable(int n) {
	
	LZWbptr=LZWbuffer;
	bitNum=	n>1 ? n : 2;
	ReinitializeLZWTable();
	CH=0;
	BitPos=1;
  return 0;
	}




/*
 *  Cross platform GIF source code.
 *
 *  Platform: Neutral
 *
 *  Version: 2.30  1997/07/07  Original version by Lachlan Patrick.
 *  Version: 2.35  1998/09/09  Minor upgrade to list functions.
 *  Version: 2.50  2000/01/01  Added the ability to load an animated gif.
 *  Version: 3.00  2001/03/03  Fixed a few bugs and updated the interface.
 *  Version: 3.34  2002/12/18  Debugging code is now better encapsulated.
 *  Version: 3.56  2005/08/09  Silenced a size_t conversion warning.
 *  Version: 3.60  2007/06/06  Fixed a memory leak in del_gif.
 */

/* Copyright (c) L. Patrick

   This file is part of the App cross-platform programming package.
   You may redistribute it and/or modify it under the terms of the
   App Software License. See the file LICENSE.TXT for details.
*/

/*
 *  Gif.c - Cross-platform code for loading and saving GIFs
 *
 *  The LZW encoder and decoder used in this file were
 *  written by Gershon Elber and Eric S. Raymond as part of
 *  the GifLib package.
 *
 *  The remainder of the code was written by Lachlan Patrick
 *  as part of the GraphApp cross-platform graphics library.
 *
 *  GIF(sm) is a service mark property of CompuServe Inc.
 *  For better compression and more features than GIF,
 *  use PNG: the Portable Network Graphics format.
 */

/*
 *  Copyright and patent information:
 *
 *  Because the LZW algorithm has been patented by
 *  CompuServe Inc, you probably can't use this file
 *  in a commercial application without first paying
 *  CompuServe the appropriate licensing fee.
 *  Contact CompuServe for more information about that.
 */

/*
 *  Known problems with this code:
 *
 *  There is really only one thing to watch out for:
 *  on a PC running a 16-bit operating system, such
 *  as Windows 95 or Windows 3.1, there is a 64K limit
 *  to the size of memory blocks. This may limit the
 *  size of GIF files you can load, perhaps to less
 *  than 256 pixels x 256 pixels. The new row pointer
 *  technique used in this version of this file should
 *  remove that limitation, but you should test this
 *  on your system before believing me.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gif.h"

/*
 *  GIF memory allocation helper functions.
 */
void *CGif::gif_alloc(long bytes) {
	return GlobalAlloc(GPTR,bytes);
	}

/*
 *  GIF file input/output functions.
 */
unsigned char CGif::read_byte(CFile *file) {
	int ch;
	file->Read(&ch,1);
	if(ch == EOF)
		ch=0;
	return ch;
	}

int CGif::write_byte(CFile *file, int ch) {
	
	file->Write(&ch,1);
	return 1;
	}

int CGif::read_stream(CFile *file, unsigned char buffer[], int length) {

	int count = (int)file->Read(buffer, length);
	int i = count;
	while (i < length)
		buffer[i++] = '\0';
	return count;
	}

int CGif::write_stream(CFile *file, unsigned char buffer[], int length) {
	
	file->Write(buffer, length);
	return length;
	}

int CGif::read_gif_int(CFile *file) {
	int output;
	unsigned char buf[2];

	if(file->Read(buf, 2) != 2)
		return 0;
	output = (((unsigned int) buf[1]) << 8) | buf[0];
	return output;
	}

void CGif::write_gif_int(CFile *file, int output) {

	file->Write(&output,2);
	}

/*
 *  Gif data blocks:
 */

GifData *CGif::new_gif_data(int size) {
	GifData *data = (GifData *)gif_alloc(sizeof(GifData));

	if(data) {
		data->byte_count = size;
		data->bytes = (BYTE *)GlobalAlloc(GPTR,(size * sizeof(unsigned char)));
		}
	return data;
	}

void CGif::del_gif_data(GifData *data) {

	GlobalFree(data->bytes);
	GlobalFree(data);
	}

/*
 *  Read one code block from the Gif file.
 *  This routine should be called until NULL is returned.
 *  Use app_free() to free the returned array of bytes.
 */
GifData *CGif::read_gif_data(CFile *file) {
	GifData *data;
	int size;

	size = read_byte(file);

	if(size > 0) {
		data = new_gif_data(size);
		read_stream(file, data->bytes, size);
		}
	else {
		data = NULL;
		}
	return data;
	}

/*
 *  Write a Gif data block to a file.
 *  A Gif data block is a size-byte followed by that many
 *  bytes of data (0 to 255 of them).
 */
void CGif::write_gif_data(CFile *file, GifData *data) {

	if(data) {
		write_byte(file, data->byte_count);
		write_stream(file, data->bytes, data->byte_count);
		}
	else
		write_byte(file, 0);
	}

#ifdef GIF_DEBUG
void CGif::print_gif_data(CFile *file, GifData *data) {
	int i, ch, prev;
	int ch_printable, prev_printable;

	if(data) {
		fprintf(file, "(length=%d) [", data->byte_count);
		prev_printable = 1;
		for(i=0; i < data->byte_count; i++) {
			ch = data->bytes[i];
			ch_printable = isprint(ch) ? 1 : 0;

			if(ch_printable != prev_printable)
				fprintf(file, " ");

			if(ch_printable)
				fprintf(file, "%c", (char)ch);
			else
				fprintf(file, "%02X,", ch);

			prev = ch;
			prev_printable = isprint(prev) ? 1 : 0;
			}
		fprintf(file, "]\n");
		}
	else {
		fprintf(file, "[]\n");
		}
	}
#endif

/*
 *  Read the next byte from a Gif file.
 *
 *  This function is aware of the block-nature of Gif files,
 *  and will automatically skip to the next block to find
 *  a new byte to read, or return 0 if there is no next block.
 */
unsigned char CGif::read_gif_byte(CFile *file, GifDecoder *decoder) {
	unsigned char *buf = decoder->buf;
	unsigned char next;

	if(decoder->file_state == IMAGE_COMPLETE)
		return '\0';

	if(decoder->position == decoder->bufsize) {	/* internal buffer now empty! */
		/* read the block size */
		decoder->bufsize = read_byte(file);
		if(decoder->bufsize == 0) {
			decoder->file_state = IMAGE_COMPLETE;
			return '\0';
			}
		read_stream(file, buf, decoder->bufsize);
		next = buf[0];
		decoder->position = 1;	/* where to get chars */
		}
	else {
		next = buf[decoder->position++];
		}

	return next;
	}

/*
 *  Read to end of an image, including the zero block.
 */
void CGif::finish_gif_picture(CFile *file, GifDecoder *decoder) {
	unsigned char *buf = decoder->buf;

	while (decoder->bufsize != 0) {
		decoder->bufsize = read_byte(file);
		if (decoder->bufsize == 0) {
			decoder->file_state = IMAGE_COMPLETE;
			break;
			}
		read_stream(file, buf, decoder->bufsize);
		}
	}

/*
 *  Write a byte to a Gif file.
 *
 *  This function is aware of Gif block structure and buffers
 *  chars until 255 can be written, writing the size byte first.
 *  If FLUSH_OUTPUT is the char to be written, the buffer is
 *  written and an empty block appended.
 */
void CGif::write_gif_byte(CFile *file, GifEncoder *encoder, int ch) {
	unsigned char *buf = encoder->buf;

	if(encoder->file_state == IMAGE_COMPLETE)
		return;

	if(ch == FLUSH_OUTPUT)	{
		if(encoder->bufsize) {
			write_byte(file, encoder->bufsize);
			write_stream(file, buf, encoder->bufsize);
			encoder->bufsize = 0;
			}
		/* write an empty block to mark end of data */
		write_byte(file, 0);
		encoder->file_state = IMAGE_COMPLETE;
		}
	else {
		if(encoder->bufsize == 255) {
			/* write this buffer to the file */
			write_byte(file, encoder->bufsize);
			write_stream(file, buf, encoder->bufsize);
			encoder->bufsize = 0;
			}
		buf[encoder->bufsize++] = ch;
		}
	}

/*
 *  Colour maps:
 */

GifPalette *CGif::new_gif_palette(void) {

	return (GifPalette *)gif_alloc(sizeof(GifPalette));
	}

void CGif::del_gif_palette(GifPalette *cmap) {

	GlobalFree(cmap->colours);
	GlobalFree(cmap);
	}

void CGif::read_gif_palette(CFile *file, GifPalette *cmap) {
	int i;
	unsigned char r,g,b;

	cmap->colours = (COLORREF *)GlobalAlloc(GPTR,cmap->length * sizeof(COLORREF));

	for(i=0; i<cmap->length; i++) {
		r = read_byte(file);
		g = read_byte(file);
		b = read_byte(file);
		cmap->colours[i] = RGB(r,g,b);
		}
	}

void CGif::write_gif_palette(CFile *file, GifPalette *cmap) {
	int i;
	COLORREF c;

	for(i=0; i<cmap->length; i++) {
		c = cmap->colours[i];
		write_byte(file, GetRValue(c));
		write_byte(file, GetGValue(c));
		write_byte(file, GetBValue(c));
		}
	}

#ifdef GIF_DEBUG
void CGif::print_gif_palette(CFile *file, GifPalette *cmap) {
	int i;

	fprintf(file, "  GifPalette (length=%d):\n", cmap->length);
	for(i=0; i<cmap->length; i++) {
		fprintf(file, "   %02X = 0x", i);
		fprintf(file, "%02X", cmap->colours[i].red);
		fprintf(file, "%02X", cmap->colours[i].green);
		fprintf(file, "%02X\n", cmap->colours[i].blue);
		}
	}
#endif

/*
 *  GifScreen:
 */
GifScreen *CGif::new_gif_screen(void) {
	GifScreen *screen = (GifScreen *)gif_alloc(sizeof(GifScreen));
	if(screen)
		screen->cmap = new_gif_palette();
	return screen;
	}

void CGif::del_gif_screen(GifScreen *screen) {

	del_gif_palette(screen->cmap);
	GlobalFree(screen);
	}

void CGif::read_gif_screen(CFile *file, GifScreen *screen) {
	unsigned char info;

	screen->width       = read_gif_int(file);
	screen->height      = read_gif_int(file);

	info                = read_byte(file);
	screen->has_cmap    =  (info & 0x80) >> 7;
	screen->color_res   = ((info & 0x70) >> 4) + 1;
	screen->sorted      =  (info & 0x08) >> 3;
	screen->cmap_depth  =  (info & 0x07)       + 1;

	screen->bgcolour    = read_byte(file);
	screen->aspect      = read_byte(file);

	if(screen->has_cmap) {
		screen->cmap->length = 1 << screen->cmap_depth;
		read_gif_palette(file, screen->cmap);
		}
	}

void CGif::write_gif_screen(CFile *file, GifScreen *screen) {
	unsigned char info;

	write_gif_int(file, screen->width);
	write_gif_int(file, screen->height);

	info = 0;
	info = info | (screen->has_cmap ? 0x80 : 0x00);
	info = info | ((screen->color_res - 1) << 4);
	info = info | (screen->sorted ? 0x08 : 0x00);
	if(screen->cmap_depth > 0)
		info = info | ((screen->cmap_depth) - 1);
	write_byte(file, info);

	write_byte(file, screen->bgcolour);
	write_byte(file, screen->aspect);

	if(screen->has_cmap) {
		write_gif_palette(file, screen->cmap);
		}
	}

#ifdef GIF_DEBUG
void CGif::print_gif_screen(CFile *file, GifScreen *screen) {

	fprintf(file, " GifScreen:\n");
	fprintf(file, "  width      = %d\n", screen->width);
	fprintf(file, "  height     = %d\n", screen->height);

	fprintf(file, "  has_cmap   = %d\n", screen->has_cmap ? 1:0);
	fprintf(file, "  color_res  = %d\n", screen->color_res);
	fprintf(file, "  sorted     = %d\n", screen->sorted ? 1:0);
	fprintf(file, "  cmap_depth = %d\n", screen->cmap_depth);

	fprintf(file, "  bgcolour   = %02X\n", screen->bgcolour);
	fprintf(file, "  aspect     = %d\n", screen->aspect);

	if(screen->has_cmap) {
		print_gif_palette(file, screen->cmap);
		}
	}
#endif

/*
 *  GifExtension:
 */

GifExtension *CGif::new_gif_extension(void) {

	return (GifExtension *)gif_alloc(sizeof(GifExtension));
	}

void CGif::del_gif_extension(GifExtension *ext) {
	int i;

	for(i=0; i < ext->data_count; i++)
		del_gif_data(ext->data[i]);
	GlobalFree(ext->data);
	GlobalFree(ext);
	}

void CGif::read_gif_extension(CFile *file, GifExtension *ext) {
	GifData *data;
	int i;

	ext->marker = read_byte(file);

	data = read_gif_data(file);
	while(data) {
		/* Append the data object: */
		i = ++ext->data_count;
		ext->data = (GifData **)GlobalReAlloc(ext->data, i * sizeof(GifData *),GMEM_ZEROINIT);
		ext->data[i-1] = data;
		data = read_gif_data(file);
		}
	}

void CGif::write_gif_extension(CFile *file, GifExtension *ext) {
	int i;

	write_byte(file, ext->marker);

	for(i=0; i < ext->data_count; i++)
		write_gif_data(file, ext->data[i]);
	write_gif_data(file, NULL);
	}

#ifdef GIF_DEBUG
void CGif::print_gif_extension(CFile *file, GifExtension *ext) {
	int i;

	fprintf(file, " GifExtension:\n");
	fprintf(file, "  marker = 0x%02X\n", ext->marker);
	for(i=0; i < ext->data_count; i++) {
		fprintf(file, "  data = ");
		print_gif_data(file, ext->data[i]);
		}
	}
#endif

/*
 *  GifDecoder:
 */

GifDecoder *CGif::new_gif_decoder(void) {

	return (GifDecoder *)gif_alloc(sizeof(GifDecoder));
	}

void CGif::del_gif_decoder(GifDecoder *decoder) {

	GlobalFree(decoder);
	}

void CGif::init_gif_decoder(CFile *file, GifDecoder *decoder) {
	int i, depth;
	int lzw_min;
	unsigned int *prefix;

	lzw_min = read_byte(file);
	depth = lzw_min;

	decoder->file_state   = IMAGE_LOADING;
	decoder->position     = 0;
	decoder->bufsize      = 0;
	decoder->buf[0]       = 0;
	decoder->depth        = depth;
	decoder->clear_code   = (1 << depth);
	decoder->eof_code     = decoder->clear_code + 1;
	decoder->running_code = decoder->eof_code + 1;
	decoder->running_bits = depth + 1;
	decoder->max_code_plus_one = 1 << decoder->running_bits;
	decoder->stack_ptr    = 0;
	decoder->prev_code    = NO_SUCH_CODE;
	decoder->shift_state  = 0;
	decoder->shift_data   = 0;

	prefix = decoder->prefix;
	for (i = 0; i <= LZ_MAX_CODE; i++)
		prefix[i] = NO_SUCH_CODE;
	}

/*
 *  Read the next Gif code word from the file.
 *
 *  This function looks in the decoder to find out how many
 *  bits to read, and uses a buffer in the decoder to remember
 *  bits from the last byte input.
 */
int CGif::read_gif_code(CFile *file, GifDecoder *decoder) {
	int code;
	unsigned char next_byte;
	static int code_masks[] = {
		0x0000, 0x0001, 0x0003, 0x0007,
		0x000f, 0x001f, 0x003f, 0x007f,
		0x00ff, 0x01ff, 0x03ff, 0x07ff,
		0x0fff
		};

	while (decoder->shift_state < decoder->running_bits) {
		/* Need more bytes from input file for next code: */
		next_byte = read_gif_byte(file, decoder);
		decoder->shift_data |=
		  ((unsigned long) next_byte) << decoder->shift_state;
		decoder->shift_state += 8;
		}

	code = decoder->shift_data & code_masks[decoder->running_bits];

	decoder->shift_data >>= decoder->running_bits;
	decoder->shift_state -= decoder->running_bits;

	/* If code cannot fit into running_bits bits,
	 * we must raise its size.
	 * Note: codes above 4095 are used for signalling. */
	if(++decoder->running_code > decoder->max_code_plus_one
		&& decoder->running_bits < LZ_BITS)	{
		decoder->max_code_plus_one <<= 1;
		decoder->running_bits++;
		}
	return code;
	}

/*
 *  Routine to trace the prefix-linked-list until we get
 *  a prefix which is a pixel value (less than clear_code).
 *  Returns that pixel value.
 *
 *  If the picture is defective, we might loop here forever,
 *  so we limit the loops to the maximum possible if the
 *  picture is okay, i.e. LZ_MAX_CODE times.
 */
int CGif::trace_prefix(unsigned int *prefix, int code, int clear_code) {
	int i=0;

	while(code > clear_code && i++ <= LZ_MAX_CODE)
		code = prefix[code];
	return code;
	}

/*
 *  The LZ decompression routine:
 *  Call this function once per scanline to fill in a picture.
 */
void CGif::read_gif_line(CFile *file, GifDecoder *decoder,
			unsigned char *line, int length) {
  int i=0, j;
  int current_code, eof_code, clear_code;
  int current_prefix, prev_code, stack_ptr;
  unsigned char *stack, *suffix;
  unsigned int *prefix;

  prefix	= decoder->prefix;
  suffix	= decoder->suffix;
  stack	= decoder->stack;
  stack_ptr	= decoder->stack_ptr;
  eof_code	= decoder->eof_code;
  clear_code	= decoder->clear_code;
  prev_code	= decoder->prev_code;

  if(stack_ptr != 0) {
	/* Pop the stack */
		while(stack_ptr != 0 && i < length)
			line[i++] = stack[--stack_ptr];
    }

  while(i < length) {
		current_code = read_gif_code(file, decoder);

	if(current_code == eof_code) {
	   /* unexpected EOF */
	   if (i != length - 1 || decoder->pixel_count != 0)
		return;
	   i++;
		}
	else if(current_code == clear_code) {
	    /* reset prefix table etc */
	  for(j=0; j <= LZ_MAX_CODE; j++)
			prefix[j] = NO_SUCH_CODE;
	  decoder->running_code = decoder->eof_code + 1;
	  decoder->running_bits = decoder->depth + 1;
	  decoder->max_code_plus_one = 1 << decoder->running_bits;
	  prev_code = decoder->prev_code = NO_SUCH_CODE;
		}
	else {
	  /* Regular code - if in pixel range
	   * simply add it to output pixel stream,
	   * otherwise trace code-linked-list until
	   * the prefix is in pixel range. */
	  if(current_code < clear_code) {
			/* Simple case. */
			line[i++] = current_code;
	    }
	  else {
		/* This code needs to be traced:
		 * trace the linked list until the prefix is a
		 * pixel, while pushing the suffix pixels on
		 * to the stack. If finished, pop the stack
		 * to output the pixel values. */
			if((current_code < 0) || (current_code > LZ_MAX_CODE))
				return; /* image defect */
			if(prefix[current_code] == NO_SUCH_CODE) {
		    /* Only allowed if current_code is exactly
		     * the running code:
		     * In that case current_code = XXXCode,
		     * current_code or the prefix code is the
		     * last code and the suffix char is
		     * exactly the prefix of last code! */
		    if(current_code == decoder->running_code - 2) {
					current_prefix = prev_code;
					suffix[decoder->running_code - 2]
			    = stack[stack_ptr++]
			    = trace_prefix(prefix, prev_code, clear_code);
					}
		    else {
					return; /* image defect */
				 }
			}
		else
	    current_prefix = current_code;

		/* Now (if picture is okay) we should get
		 * no NO_SUCH_CODE during the trace.
		 * As we might loop forever (if picture defect)
		 * we count the number of loops we trace and
		 * stop if we get LZ_MAX_CODE.
		 * Obviously we cannot loop more than that. */
		j = 0;
		while(j++ <= LZ_MAX_CODE
			&& current_prefix > clear_code
			&& current_prefix <= LZ_MAX_CODE) {
		    stack[stack_ptr++] = suffix[current_prefix];
		    current_prefix = prefix[current_prefix];
			}
		if(j >= LZ_MAX_CODE || current_prefix > LZ_MAX_CODE)
		    return; /* image defect */

		/* Push the last character on stack: */
		stack[stack_ptr++] = current_prefix;

		/* Now pop the entire stack into output: */
		while (stack_ptr != 0 && i < length)
		  line[i++] = stack[--stack_ptr];
	    }
	  if (prev_code != NO_SUCH_CODE) {
			if((decoder->running_code < 2) ||
				(decoder->running_code > LZ_MAX_CODE+2))
				return; /* image defect */
			prefix[decoder->running_code - 2] = prev_code;

			if(current_code == decoder->running_code - 2) {
					/* Only allowed if current_code is exactly
					 * the running code:
					 * In that case current_code = XXXCode,
					 * current_code or the prefix code is the
					 * last code and the suffix char is
					 * exactly the prefix of the last code! */
				suffix[decoder->running_code - 2] = trace_prefix(prefix, prev_code, clear_code);
				}
			else {
				suffix[decoder->running_code - 2] = trace_prefix(prefix, current_code, clear_code);
					}
				}
	    prev_code = current_code;
			}
    }

  decoder->prev_code = prev_code;
  decoder->stack_ptr = stack_ptr;
	}

/*
 *  Hash table:
 */

/*
 *  The 32 bits contain two parts: the key & code:
 *  The code is 12 bits since the algorithm is limited to 12 bits
 *  The key is a 12 bit prefix code + 8 bit new char = 20 bits.
 */
#define HT_GET_KEY(x)	((x) >> 12)
#define HT_GET_CODE(x)	((x) & 0x0FFF)
#define HT_PUT_KEY(x)	((x) << 12)
#define HT_PUT_CODE(x)	((x) & 0x0FFF)

/*
 *  Generate a hash key from the given unique key.
 *  The given key is assumed to be 20 bits as follows:
 *    lower 8 bits are the new postfix character,
 *    the upper 12 bits are the prefix code.
 */
int CGif::gif_hash_key(unsigned long key) {
	return ((key >> 12) ^ key) & HT_KEY_MASK;
	}

/*
 *  Clear the hash_table to an empty state.
 */
void CGif::clear_gif_hash_table(unsigned long *hash_table) {
	int i;

	for(i=0; i<HT_SIZE; i++)
		hash_table[i] = 0xFFFFFFFFL;
	}

/*
 *  Insert a new item into the hash_table.
 *  The data is assumed to be new.
 */
void CGif::add_gif_hash_entry(unsigned long *hash_table, unsigned long key, int code) {
	int hkey = gif_hash_key(key);

	while(HT_GET_KEY(hash_table[hkey]) != 0xFFFFFL) {
		hkey = (hkey + 1) & HT_KEY_MASK;
		}
	hash_table[hkey] = HT_PUT_KEY(key) | HT_PUT_CODE(code);
	}

/*
 *  Determine if given key exists in hash_table and if so
 *  returns its code, otherwise returns -1.
 */
int CGif::lookup_gif_hash(unsigned long *hash_table, unsigned long key) {
	int hkey = gif_hash_key(key);
	unsigned long htkey;

	while((htkey = HT_GET_KEY(hash_table[hkey])) != 0xFFFFFL) {
		if(key == htkey)
			return HT_GET_CODE(hash_table[hkey]);
		hkey = (hkey + 1) & HT_KEY_MASK;
		}
	return -1;
	}

/*
 *  GifEncoder:
 */

GifEncoder *CGif::new_gif_encoder(void) {

	return (GifEncoder *)gif_alloc(sizeof(GifEncoder));
	}

void CGif::del_gif_encoder(GifEncoder *encoder) {

	GlobalFree(encoder);
	}

/*
 *  Write a Gif code word to the output file.
 *
 *  This function packages code words up into whole bytes
 *  before writing them. It uses the encoder to store
 *  codes until enough can be packaged into a whole byte.
 */
void CGif::write_gif_code(CFile *file, GifEncoder *encoder, int code) {

	if(code == FLUSH_OUTPUT) {
		/* write all remaining data */
		while(encoder->shift_state > 0) {
			write_gif_byte(file, encoder,
				encoder->shift_data & 0xff);
			encoder->shift_data >>= 8;
			encoder->shift_state -= 8;
			}
		encoder->shift_state = 0;
		write_gif_byte(file, encoder, FLUSH_OUTPUT);
		}
	else {
		encoder->shift_data |=
			((long) code) << encoder->shift_state;
		encoder->shift_state += encoder->running_bits;

		while (encoder->shift_state >= 8)	{
			/* write full bytes */
			write_gif_byte(file, encoder,
				encoder->shift_data & 0xff);
			encoder->shift_data >>= 8;
			encoder->shift_state -= 8;
			}
		}

	/* If code can't fit into running_bits bits, raise its size.
	 * Note that codes above 4095 are for signalling. */
	if (encoder->running_code >= encoder->max_code_plus_one
		&& code <= 4095) {
    		encoder->max_code_plus_one = 1 << ++encoder->running_bits;
		}
	}

/*
 *   Initialise the encoder, given a GifPalette depth.
 */
void CGif::init_gif_encoder(CFile *file, GifEncoder *encoder, int depth) {
	int lzw_min = depth = (depth < 2 ? 2 : depth);

	encoder->file_state   = IMAGE_SAVING;
	encoder->position     = 0;
	encoder->bufsize      = 0;
	encoder->buf[0]       = 0;
	encoder->depth        = depth;
	encoder->clear_code   = (1 << depth);
	encoder->eof_code     = encoder->clear_code + 1;
	encoder->running_code = encoder->eof_code + 1;
	encoder->running_bits = depth + 1;
	encoder->max_code_plus_one = 1 << encoder->running_bits;
	encoder->current_code = FIRST_CODE;
	encoder->shift_state  = 0;
	encoder->shift_data   = 0;

	/* Write the LZW minimum code size: */
	write_byte(file, lzw_min);

	/* Clear hash table, output Clear code: */
	clear_gif_hash_table(encoder->hash_table);
	write_gif_code(file, encoder, encoder->clear_code);
	}

/*
 *  Write one scanline of pixels out to the Gif file,
 *  compressing that line using LZW into a series of codes.
 */
void CGif::write_gif_line(CFile *file, GifEncoder *encoder, unsigned char *line, int length) {
  int i=0, current_code, new_code;
  unsigned long new_key;
  unsigned char pixval;
  unsigned long *hash_table;

  hash_table = encoder->hash_table;

  if(encoder->current_code == FIRST_CODE)
		current_code = line[i++];
  else
		current_code = encoder->current_code;

  while (i < length) {
		pixval = line[i++]; /* Fetch next pixel from stream */

	/* Form a new unique key to search hash table for the code
	 * Combines current_code as prefix string with pixval as
	 * postfix char */
	new_key = (((unsigned long) current_code) << 8) + pixval;
	if((new_code = lookup_gif_hash(hash_table, new_key)) >= 0) {
	  /* This key is already there, or the string is old,
	   * so simply take new code as current_code */
	  current_code = new_code;
		}
	else {
	  /* Put it in hash table, output the prefix code,
	   * and make current_code equal to pixval */
	  write_gif_code(file, encoder, current_code);
	  current_code = pixval;

	  /* If the hash_table if full, send a clear first
	   * then clear the hash table: */
	  if(encoder->running_code >= LZ_MAX_CODE) {
			write_gif_code(file, encoder, encoder->clear_code);
			encoder->running_code = encoder->eof_code + 1;
			encoder->running_bits = encoder->depth + 1;
			encoder->max_code_plus_one = 1 << encoder->running_bits;
			clear_gif_hash_table(hash_table);
	    }
	  else {
			/* Put this unique key with its relative code in hash table */
			add_gif_hash_entry(hash_table, new_key, encoder->running_code++);
	    }
			}
    }

  /* Preserve the current state of the compression algorithm: */
  encoder->current_code = current_code;
	}

void CGif::flush_gif_encoder(CFile *file, GifEncoder *encoder) {

	write_gif_code(file, encoder, encoder->current_code);
	write_gif_code(file, encoder, encoder->eof_code);
	write_gif_code(file, encoder, FLUSH_OUTPUT);
	}

/*
 *  GifPicture:
 */

GifPicture *CGif::new_gif_picture(void) {
	GifPicture *pic = (GifPicture *)gif_alloc(sizeof(GifPicture));

	if(pic) {
		pic->cmap = new_gif_palette();
		pic->data = NULL;
		}
	return pic;
	}

void CGif::del_gif_picture(GifPicture *pic) {
	int row;

	del_gif_palette(pic->cmap);
	if(pic->data) {
		for (row=0; row < pic->height; row++)
			GlobalFree(pic->data[row]);
		GlobalFree(pic->data);
		}
	GlobalFree(pic);
	}

void CGif::read_gif_picture_data(CFile *file, GifPicture *pic) {
	GifDecoder *decoder;
	long w, h;
	int interlace_start[] = {0, 4, 2, 1};
	int interlace_step[]  = {8, 8, 4, 2};
	int scan_pass, row;

	w = pic->width;
	h = pic->height;
	pic->data = (BYTE **)GlobalAlloc(GPTR,h * sizeof(unsigned char *));
	if(pic->data == NULL)
		return;
	for(row=0; row < h; row++)
		pic->data[row] = (BYTE *)GlobalAlloc(GPTR,w * sizeof(unsigned char));

	decoder = new_gif_decoder();
	init_gif_decoder(file, decoder);

	if(pic->interlace) {
	  for (scan_pass = 0; scan_pass < 4; scan_pass++) {
	    row = interlace_start[scan_pass];
	    while(row < h) {
	      read_gif_line(file, decoder, pic->data[row], w);
	      row += interlace_step[scan_pass];
				}
			}
		}
	else {
	  row = 0;
	  while (row < h) {
	    read_gif_line(file, decoder, pic->data[row], w);
	    row += 1;
			}
		}
	finish_gif_picture(file, decoder);

	del_gif_decoder(decoder);
	}

void CGif::read_gif_picture(CFile *file, GifPicture *pic) {
	unsigned char info;

	pic->left   = read_gif_int(file);
	pic->top    = read_gif_int(file);
	pic->width  = read_gif_int(file);
	pic->height = read_gif_int(file);

	info = read_byte(file);
	pic->has_cmap    = (info & 0x80) >> 7;
	pic->interlace   = (info & 0x40) >> 6;
	pic->sorted      = (info & 0x20) >> 5;
	pic->reserved    = (info & 0x18) >> 3;

	if (pic->has_cmap) {
		pic->cmap_depth  = (info & 0x07) + 1;
		pic->cmap->length = 1 << pic->cmap_depth;
		read_gif_palette(file, pic->cmap);
		}

	read_gif_picture_data(file, pic);
	}

void CGif::write_gif_picture_data(CFile *file, GifPicture *pic) {
	GifEncoder *encoder;
	long w, h;
	int interlace_start[] = {0, 4, 2, 1};
	int interlace_step[]  = {8, 8, 4, 2};
	int scan_pass, row;

	w = pic->width;
	h = pic->height;

	encoder = new_gif_encoder();
	init_gif_encoder(file, encoder, pic->cmap_depth);

	if(pic->interlace) {
	  for (scan_pass = 0; scan_pass < 4; scan_pass++) {
	    row = interlace_start[scan_pass];
	    while (row < h) {
	      write_gif_line(file, encoder, pic->data[row], w);
	      row += interlace_step[scan_pass];
				}
			}
		}
	else {
	  row = 0;
	  while (row < h) {
	    write_gif_line(file, encoder, pic->data[row], w);
	    row += 1;
			}
		}

	flush_gif_encoder(file, encoder);
	del_gif_encoder(encoder);
	}

void CGif::write_gif_picture(CFile *file, GifPicture *pic) {
	unsigned char info;

	write_gif_int(file, pic->left);
	write_gif_int(file, pic->top);
	write_gif_int(file, pic->width);
	write_gif_int(file, pic->height);

	info = 0;
	info = info | (pic->has_cmap    ? 0x80 : 0x00);
	info = info | (pic->interlace   ? 0x40 : 0x00);
	info = info | (pic->sorted      ? 0x20 : 0x00);
	info = info | ((pic->reserved << 3) & 0x18);
	if(pic->has_cmap)
		info = info | (pic->cmap_depth - 1);

	write_byte(file, info);

	if(pic->has_cmap)
		write_gif_palette(file, pic->cmap);

	write_gif_picture_data(file, pic);
	}

#ifdef GIF_DEBUG
void CGif::print_gif_picture_data(CFile *file, GifPicture *pic) {
	int pixval, row, col;

	for(row = 0; row < pic->height; row++) {
	  fprintf(file, "   [");
	  for (col = 0; col < pic->width; col++) {
	    pixval = pic->data[row][col];
	    fprintf(file, "%02X", pixval);
			}
	  file->WriteString("]\n");
		}
	}

void CGif::print_gif_picture_header(FILE *file, GifPicture *pic) {

	fprintf(file, " GifPicture:\n");
	fprintf(file, "  left       = %d\n", pic->left);
	fprintf(file, "  top        = %d\n", pic->top);
	fprintf(file, "  width      = %d\n", pic->width);
	fprintf(file, "  height     = %d\n", pic->height);

	fprintf(file, "  has_cmap   = %d\n", pic->has_cmap);
	fprintf(file, "  interlace  = %d\n", pic->interlace);
	fprintf(file, "  sorted     = %d\n", pic->sorted);
	fprintf(file, "  reserved   = %d\n", pic->reserved);
	fprintf(file, "  cmap_depth = %d\n", pic->cmap_depth);
	}

void CGif::print_gif_picture(CFile *file, GifPicture *pic) {
	
	print_gif_picture_header(file, pic);

	if(pic->has_cmap)
		print_gif_palette(file, pic->cmap);

	print_gif_picture_data(file, pic);
	}
#endif

/*
 *  GifBlock:
 */

GifBlock *CGif::new_gif_block(void) {

	return (GifBlock *)gif_alloc(sizeof(GifBlock));
	}

void CGif::del_gif_block(GifBlock *block) {

	if(block->pic)
		del_gif_picture(block->pic);
	if(block->ext)
		del_gif_extension(block->ext);
	GlobalFree(block);
	}

void CGif::read_gif_block(CFile *file, GifBlock *block) {

	block->intro = read_byte(file);
	if(block->intro == 0x2C) {
		block->pic = new_gif_picture();
		read_gif_picture(file, block->pic);
		}
	else if(block->intro == 0x21) {
		block->ext = new_gif_extension();
		read_gif_extension(file, block->ext);
		}
	}

void CGif::write_gif_block(CFile *file, GifBlock *block) {

	write_byte(file, block->intro);
	if(block->pic)
		write_gif_picture(file, block->pic);
	if(block->ext)
		write_gif_extension(file, block->ext);
	}

#ifdef GIF_DEBUG
void CGif::print_gif_block(CFile *file, GifBlock *block) {

	fprintf(file, " GifBlock (intro=0x%02X):\n", block->intro);
	if(block->pic)
		print_gif_picture(file, block->pic);
	if(block->ext)
		print_gif_extension(file, block->ext);
	}
#endif

/*
 *  Gif:
 */

Gif *CGif::new_gif(void) {
	Gif *gif = (Gif *)gif_alloc(sizeof(Gif));

	if(gif) {
		strcpy(gif->header, "GIF87a");
		gif->screen = new_gif_screen();
		gif->blocks = NULL;
		}
	return gif;
	}

void CGif::del_gif(Gif *gif) {
	int i;

	del_gif_screen(gif->screen);
	for(i=0; i < gif->block_count; i++)
		del_gif_block(gif->blocks[i]);
	GlobalFree(gif->blocks);
	GlobalFree(gif);
	}

void CGif::read_gif(CFile *file, Gif *gif) {
	int i;
	GifBlock *block;

	for(i=0; i<6; i++)
		gif->header[i] = read_byte(file);
	if(strncmp(gif->header, "GIF", 3) != 0)
		return; /* error */

	read_gif_screen(file, gif->screen);

	while(1) {
		block = new_gif_block();
		read_gif_block(file, block);

		if(block->intro == GIF_BYTE_EOF) {	/* terminator */
			del_gif_block(block);
			break;
			}
		else if (block->intro == GIF_BYTE_IMAGE_DESCRIPTOR) {	/* image */
			/* Append the block: */
			i = ++gif->block_count;
			gif->blocks = (GifBlock **)GlobalReAlloc(gif->blocks, i * sizeof(GifBlock *),GPTR);
			gif->blocks[i-1] = block;
			}
		else if (block->intro == GIF_BYTE_EXTENSION_BLOCK) {	/* extension */
			/* Append the block: */
			i = ++gif->block_count;
			gif->blocks = (GifBlock **)GlobalReAlloc(gif->blocks, i * sizeof(GifBlock *),GPTR);
			gif->blocks[i-1] = block;
			}
		else {	/* error */
			del_gif_block(block);
			break;
			}
		}
	}

void CGif::read_one_gif_picture(CFile *file, Gif *gif) {
	int i;
	GifBlock *block;

	for(i=0; i<6; i++)
		gif->header[i] = read_byte(file);
	if(strncmp(gif->header, "GIF", 3) != 0)
		return; /* error */

	read_gif_screen(file, gif->screen);

	while(1) {
		block = new_gif_block();
		read_gif_block(file, block);

		if(block->intro == GIF_BYTE_EOF) {	/* terminator */
			del_gif_block(block);
			break;
			}
		else if (block->intro == GIF_BYTE_IMAGE_DESCRIPTOR) { /* image */
			/* Append the block: */
			i = ++gif->block_count;
			gif->blocks = (GifBlock **)GlobalReAlloc(gif->blocks, i * sizeof(GifBlock *),GPTR);
			gif->blocks[i-1] = block;
			break;
			}
		else if (block->intro == GIF_BYTE_EXTENSION_BLOCK) { /* extension */
			/* Append the block: */
			i = ++gif->block_count;
			gif->blocks = (GifBlock **)GlobalReAlloc(gif->blocks, i * sizeof(GifBlock *),GPTR);
			gif->blocks[i-1] = block;
			continue;
		}
		else {	/* error! */
			del_gif_block(block);
			break;
			}
		}
	}

void CGif::write_gif(CFile *file, Gif *gif) {
	int i;

	file->Write(gif->header,3);
	write_gif_screen(file, gif->screen);
	for(i=0; i < gif->block_count; i++)
		write_gif_block(file, gif->blocks[i]);
	write_byte(file, 0x3B);
	}

#ifdef GIF_DEBUG
void CGif::print_gif(CFile *file, Gif *gif) {
	int i;

	fprintf(file, "Gif header=%s\n", gif->header);
	print_gif_screen(file, gif->screen);
	for (i=0; i < gif->block_count; i++)
		print_gif_block(file, gif->blocks[i]);
	file.WriteString("End of gif.\n\n");
	}
#endif

/*
 *  Reading and Writing Gif files:
 */

Gif *CGif::read_gif_file(const char *filename) {
	Gif *gif;
	CFile file;

	file.Open(filename, CFile::modeRead | CFile::typeBinary);
	if(file == 0)
		return NULL;
	gif = new_gif();
	if(gif == NULL) {
		file.Close();
		return NULL;
		}
	read_gif(&file, gif);
	file.Close();
	if(strncmp(gif->header, "GIF", 3) != 0) {
		del_gif(gif);
		gif = NULL;
		}
	return gif;
	}

void CGif::write_gif_file(const char *filename, Gif *gif) {
	CFile file;

	file.Open(filename, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary);
	if(file == 0)
		return;
	if(gif == NULL) {
		file.Close();
		return;
		}
	write_gif(&file, gif);
	file.Close();
	}


// ----------------------------------------------------------------------------------------
CWBMP::CWBMP() {

	}

CWBMP::~CWBMP() {

	}

BYTE *CWBMP::buildWBMP(CBitmap *bmp,DWORD *len,int reverseV) {
	BITMAP b;
	BYTE *p;
	char *s;
	register int i,j,x,y;
	BYTE ch;
	register unsigned int i1;
	BYTE *d=NULL;
	DWORD l;
	BITMAPINFO *bi;
	HDC dc;

	if(bi=(BITMAPINFO *)GlobalAlloc(GMEM_FIXED,sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD))) {
		bmp->GetBitmap(&b);
		if(d=p=(BYTE *)GlobalAlloc(GMEM_FIXED,(b.bmWidth*b.bmHeight)/8+4)) {	// occupazione MAX del WBMP
		
		l=b.bmWidth*b.bmHeight*3;
		b.bmBits=(char *)GlobalAlloc(GMEM_FIXED,l+1024);	 // PERCHE' PATCH 2048???
	//	bmp->GetBitmapBits(l,b.bmBits);
		dc=GetDC(GetDesktopWindow()); //theApp.m_pMainWnd->GetDC();
		ZeroMemory(bi,sizeof(BITMAPINFOHEADER));
		bi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
			// a 24 non funziona, o meglio funzionerebbe se lo schermo e' a 24 anch'esso!
		i=GetDIBits(dc,(HBITMAP)*bmp,0,0,NULL,bi,DIB_RGB_COLORS);
		bi->bmiHeader.biBitCount=24;		// la voglio a 24 bit, poi ci faccio il B/N!
		bi->bmiHeader.biCompression=0;
		if(reverseV)
			bi->bmiHeader.biHeight=-bi->bmiHeader.biHeight;
	//	bi->bmiHeader.biPlanes=1;
		i=GetDIBits(dc,(HBITMAP)*bmp,0,abs(bi->bmiHeader.biHeight),b.bmBits,bi,DIB_RGB_COLORS);
		ReleaseDC(GetDesktopWindow(),dc); //theApp.m_pMainWnd->ReleaseDC(dc);

		*p=0;
		*(p+1)=0;
		*(p+2)=b.bmWidth;
		*(p+3)=b.bmHeight;
		p+=4;

		s=(char *)b.bmBits;

		for(y=0; y<b.bmHeight; y++) {
			*p=0;
			for(x=0, j=0; x<b.bmWidth; x++) {
				ch=(s[0]+s[1]+s[2])/3;
				s+=3;
				
				if(ch>=0x80)
					*p |= (1 << (7-j));
				// algoritmi per il B/N???
				j++;
				if(!(j & 7)) {
					*++p=0;
					j=0;
					}
				}
/*			if(!(j & 7)) {
				p--;
				}*/
			}

		GlobalFree(b.bmBits);
		}
		GlobalFree(bi);
		}
	*len=p-d;
	return d;
	}

BYTE *CWBMP::buildWBMP(HICON hIcon,DWORD *len) {
	ICONINFO ii;

	GetIconInfo(hIcon,&ii);
	return buildWBMP(CBitmap::FromHandle(ii.hbmColor),len,FALSE);
	}

BYTE *CWBMP::buildWBMP(int bmpResource, DWORD *len) {
	CBitmap b;

	b.LoadBitmap(bmpResource);
	return buildWBMP(&b,len);
	}



