#include "stdafx.h"
#include "vidsendSerial.h"
#include "vidsend.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CLineText::CLineText(int e,CSocket *es,DWORD wThread,DWORD bs) : sbuffSize(bs) {

	sbuff=new BYTE[sbuffSize];
	critical=0;
	clearAll();
	setEcho(e,es,wThread);
  }

CLineText::~CLineText() {
	
	delete []sbuff;
	}

void CLineText::setEcho(int e,CSocket *es,DWORD wThread) {

	writeThread=wThread;
	echoSocket=es;
	bEcho=e;				// eco normale
	}

void CLineText::clearAll() {

	*sbuff=0;
	*commLogin=0;
	*commPswd=0;
	size=0;
	sbuffReadWhere=sbuffWriteWhere=0;
	}

void CLineText::clear() {

	sbuff[size=0]=0;
	}

char *CLineText::addChar(char ch) {

	sbuff[size++]=ch;	 // inserire controllo overflow...
	sbuff[size]=0;
	return (char *)sbuff;
	}

char CLineText::getAt(int where) const {	 // where da 1 a commSize

	if(where)
		return sbuff[where-1];
	else
		return sbuff[sbuffReadWhere];
	}

char CLineText::operator[](int where) const {	 // where da 1 a commSize

	if(where)
		return sbuff[where-1];
	else
		return sbuff[sbuffReadWhere];
	}

int CLineText::indexOf(const char *s) {
	int i,k=_tcslen(s),j=ssize()-k;
	char myBuf[128];

	if(k>ssize())
		return 0;
	for(i=0; i<=j; i++) {
		if((sbuffReadWhere+i) < sbuffSize) {
			memcpy(myBuf,sbuff+sbuffReadWhere+i,k);
			}
		else {
			j=sbuffSize-sbuffReadWhere;
			memcpy(myBuf,sbuff+sbuffReadWhere,j);
			memcpy(myBuf+j,sbuff,k-j);
			}
		if(!_tcsnicmp(myBuf,s,k))
			return 1;
		}
	return 0;
	}

void CLineText::operator>>(char *s) {

	readText(s);
  }

int CLineText::readText(char *s,int echo,int maxLength) {
	register int i,j,i2,f;
	char *p,*p1,*s1;
	
	bEcho=echo;
//	if(theApp.m_Modem)
//		theApp.m_Modem->setEcho(echo);		 // non potendo usare per ora CLineText::handleRead..., l'echo viene settato per entrambe da qui...
	f=0;
	*s=0;
  j=ssize();
	p=(char *)sbuff+sbuffReadWhere;
	int foundLF=0;
	for(i=0; i<j; i++) {
		if(*p==13 || i>=maxLength) {
			*p=0;
			i2=0;
			p1=(char *)sbuff+sbuffReadWhere;
			s1=s;
			while(*p1 && i2<maxLength) {
				// mettere anche qua l'opzione bExpandCR2LF???
				switch(*p1) {
					case 10:
//						foundLF=1;
						break;
					case 8:		// backspace
						s1--;
						i2--;
						break;
					default:
					  *s1++=*p1;
						i2++;
						break;
		      }
				p1++;
				if((p1-(char *)sbuff) >= sbuffSize)
					p1=(char *)sbuff;
			  }
			*s1=0;
//			memcpy(commSLine,p+1,COMMSLINE_SIZE-i-1);
			sbuffReadWhere+=(i+1+foundLF);
			if(sbuffReadWhere >= sbuffSize)
				sbuffReadWhere-=sbuffSize;
			f=1;
			break;
		  }
		p++;
		if((p-(char *)sbuff) >= sbuffSize)
			p=(char *)sbuff;
	  }
	if(f) {
    j=_tcslen(s);
//		theApp.FileSpool->print(2,"leggo una riga di testo: %s, commSLine:-%s-",s,sbuff);
		// per echo v. readData...
		return j+1;				// restituisco 1 anche se ho trovato il solo CR, opp. lunghezza +1
		}
	else
		return 0;
  }

int CLineText::readNText(BYTE *s,int n) {
	register int i,j,k;
	char *p;
	
	*s=0;
  j=ssize();
	if(j>0) {
		j--;
		k=getAt();
		if(k>0 && k<=j) {
			if((sbuffReadWhere+k) < sbuffSize) {
				// o <= ?? v.readTextRaw!
				memcpy(s,sbuff+sbuffReadWhere+1,k);			// salto il misuratore char.
				sbuffReadWhere+=k+1;
				if(sbuffReadWhere >= sbuffSize)
					sbuffReadWhere-=sbuffSize;
				}
			else {
				j=sbuffSize-sbuffReadWhere;
				memcpy(s,sbuff+sbuffReadWhere+1,j);			// salto il misuratore char.
				memcpy(s+j,sbuff,k-j);
				sbuffReadWhere=k-j+1;
				}
			s[k]=0;
			return k;
			}
		return 0;
		}
	else
		return 0;
  }

BYTE *CLineText::readTextRaw(BYTE *s, int *n) {
	int i,j;

	if(!s)
		s=(BYTE *)GlobalAlloc(GPTR,*n);
	if(s) {
		if(!(critical & 1)) {
			i=ssize();					// sarebbe CRITICAL anche questo... ma soprattutto NON usarlo nella macro MIN( !!
			if(i=min(*n,i)) {
				critical=2;
				if((sbuffReadWhere+i) <= sbuffSize) {
					memcpy(s,sbuff+sbuffReadWhere,i);
					sbuffReadWhere+=i;
					if(sbuffReadWhere >= sbuffSize)
						sbuffReadWhere=0;
					}
				else {
					j=sbuffSize-sbuffReadWhere;
					memcpy(s,sbuff+sbuffReadWhere,j);
					memcpy(s+j,sbuff,i-j);
					sbuffReadWhere=i-j;
					}
	//	if(!commText.commSSize)						// serve davvero?
	//		*commText.commSLine=0;
				critical=0;
				}
			}
		else
			i=0;
		*n=i;
		}
	else
		*n=0;
	return s;
  }

void CLineText::skip(int n) {

	n=min(n,ssize());
	if((sbuffReadWhere+n) < sbuffSize) {
		sbuffReadWhere+=n;
		if(sbuffReadWhere >= sbuffSize)
			sbuffReadWhere-=sbuffSize;
		}
	else {
		int j=sbuffSize-sbuffReadWhere;
		sbuffReadWhere=n-j;
		}
	}

BOOL CLineText::handleReadData(const BYTE *lpszInputBuffer, DWORD dwSizeofBuffer) {
	register int i,j;
	register BYTE *p;
	const BYTE *p1;

//	theApp.FileSpool->print(2,"leggo qualcosa: %s (%d char), commSLine:-%s- (size %d)",lpszInputBuffer,dwSizeofBuffer,commSLine,commSSize);
	while(critical & 2);

	critical=1;
	p=sbuff+sbuffWriteWhere;
	p1=lpszInputBuffer;
	j=dwSizeofBuffer;
	i=sbuffWriteWhere-sbuffReadWhere;
	if(i<0)
		i+=sbuffSize;
	do {
		*p++=*p1++;
		sbuffWriteWhere++;
		if(sbuffWriteWhere >= sbuffSize) {
			sbuffWriteWhere=0;
			p=sbuff;
			}
		i++;
		if(i >= sbuffSize) {
//	    theApp.FileSpool->print(2,"buffer SLine pieno! persi %d bytes",dwSizeofBuffer);
			break;
			}
		j--;
		} while(j);
	critical=0;

/*	i=COMMSLINE_SIZE - commSSize;
	if(dwSizeofBuffer < i) {
	  memcpy(commSLine+commSSize,lpszInputBuffer,dwSizeofBuffer);
		commSSize+=dwSizeofBuffer;
	  *(commSLine+commSSize)=0;
		j=dwSizeofBuffer;
		}
	else {
	  memcpy(commSLine+commSSize,lpszInputBuffer,i);
    theApp.FileSpool->print(2,"buffer SLine pieno! persi %d bytes",dwSizeofBuffer-(COMMSLINE_SIZE - commSSize));
	  commSSize=COMMSLINE_SIZE;
		j=i;
	  }*/

  if(bEcho) {		// echo chars...
		j=dwSizeofBuffer;
		p=(BYTE *)LocalAlloc(LPTR,j);
		if(p) {
			if(bEcho>1)     // ... con password!
				memset(p,'*',j);
			else
				memcpy(p,lpszInputBuffer,j);
			if(echoSocket) {
				echoSocket->Send(p,j);
				LocalFree(p);
				}
			}
//		else
//  		theApp.FileSpool->print(2,"  Failed to Alloc to Echo");
	  }
  return i;
  }

