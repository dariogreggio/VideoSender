#include "stdafx.h"
#include "vidsend.h"
#include "vidsendTime.h"
#include "vidsendLog.h"

/*
CinziaG    5.8.2017
*/

//--------------------------------------------------------------
CLogFile::CLogFile(const CString s, const CWnd *myWnd, DWORD m) :
	nomeFile(s),textWnd(myWnd),mode(m) { 
	
	hIndexFile=NULL;
	if((mode & 0xff) >= dateTimeMillisec) {
		timeBeginPeriod(1);
		}
	if(mode & keepOpen) {
	try {
		if(Open())
			SeekToEnd(); 
		}
	catch(CFileException e) {
		;
		}
	
		}

	if(mode & useIndex)
		hIndexFile=new CFile;

	}

CLogFile::CLogFile(CFile *f2,const CWnd *myWnd,DWORD m) :
	textWnd(myWnd),mode(m) { 

	m_hFile=f2->m_hFile;
	hIndexFile=NULL;
	mode &= ~keepOpen;
	if((mode & 0xff) >= dateTimeMillisec) {
		timeBeginPeriod(1);
		}
	try {
		SeekToEnd(); 
		}
	catch(CFileException e) {
		;
		}
	
	if(mode & useIndex)
		hIndexFile=new CFile;

	}

CLogFile::~CLogFile() { 

	if((mode & 0xff) >= dateTimeMillisec) {
		timeEndPeriod(1);
		}

	if(mode & keepOpen)
		Close();

	if(hIndexFile)
		delete hIndexFile;			hIndexFile=NULL;

	}


int CLogFile::Open() { 
	int i;
	
	i=CStdioFile::Open(nomeFile,CFile::modeCreate | CFile::modeNoTruncate | CFile::modeReadWrite | CFile::typeText | CFile::shareDenyWrite);

	if(mode & useIndex) {
		getIndexFileName();
		if(hIndexFile)
			hIndexFile->Open(nomeFileNdx,CFile::modeCreate | CFile::modeNoTruncate | CFile::modeReadWrite | CFile::shareDenyWrite);
		}
	
	return i;
	}

void CLogFile::Close() { 
	
	CStdioFile::Close();

	if(mode & useIndex) {
		if(hIndexFile)
			hIndexFile->Close();
		}
	}


// RICORDARSI DI CASTARE A int gli eventuali valori 32bit che sforano (-2miliardi o + 2 miliardi) o vengono mal-messi dalla chiamata...
int CLogFile::print(int m,const TCHAR *s,...) {		 // m=0 info, 1= letture skynet, 2=errore
	TCHAR myBuf[2048],myBuf1[2048],myFmt[16];
	register int i,j,ch,k;
	int n;
	double d;
	CTime myT;
	TCHAR *p,*p_myBuf;
	static BOOL inUse;
  va_list vl;
	CString S;

//	if(mode & keepOpen) {
		while(inUse)
			Sleep(20);
//		}
//	if(!inUse) {
		inUse=1;
		va_start(vl,s);
		p_myBuf=myBuf;
		if(m & 0x100) {			// se un flag almeno...
			myBuf[0]=(m & 0xff)+'$';				// flag tipo riga
			myBuf[1]=' ';			// marker di 'già letto'
			p_myBuf=myBuf+2;
			}
		if(mode & 0xff) {			// no dontUseDate...
			S=getNow();
			_tcscpy(p_myBuf,(LPTSTR)(LPCTSTR)S);
			j=_tcslen(myBuf);
			myBuf[j++]=' ';
			p_myBuf=&myBuf[j];
			}
		else
			j=p_myBuf-myBuf;
		i=0;
		while(ch=s[i++]) {
			if(ch == '%') {
				if(s[i] == '%') {
					i++;
					goto no_var;
					}
				myFmt[0]=ch;
				k=1;
				while((ch=s[i++]) && !isalpha(ch))			// salvo dettagli...
					myFmt[k++]=ch;
				myFmt[k++]=ch;			// ...copio effettivo format-type
				myFmt[k]=0;
				switch(myFmt[k-1]) {
					case 'd':
					case 'D':
					case 'u':
					case 'U':
					case 'x':
					case 'X':
						n=va_arg(vl,int);
						sprintf(myBuf1,myFmt,n);
subCopia:
						_tcscpy(myBuf+j,myBuf1);
subCopia2:
						j+=_tcslen(myBuf1);
						break;
					case 'f':
					case 'g':
						d=va_arg(vl,double);
						sprintf(myBuf1,myFmt,d);
						goto subCopia;
						break;
					case 's':
						p=va_arg(vl,TCHAR *);
						_tcsncpy(myBuf+j,p,2000-j);
						j+=min(_tcslen(p),2000-j);
						myBuf[j]=0;
						break;
					case 'c':
						n=va_arg(vl,char);
						myBuf[j++]=n;
						break;
					case 't':
						myT=va_arg(vl,CTime);
						S.Format(_T("%02u/%02u/%02u %02u:%02u:%02u"),
							myT.GetDay(),
							myT.GetMonth(),
							myT.GetYear(),
							myT.GetHour(),
							myT.GetMinute(),
							myT.GetSecond());
						_tcscpy(myBuf+j,(LPCTSTR)S);
						j+=S.GetLength();
						break;
					case 'T':
						myT=va_arg(vl,CTime);
						S=myT.Format(_T("%02u %b %04u %02u:%02u:%02u"));
						_tcscpy(myBuf+j,(LPCTSTR)S);
						j+=S.GetLength();
						break;
/*					case '%':
						myBuf[j++]='%';
						break;*/
					default:
						break;
					}
				}
			else {
				if(ch == '\n')
					myBuf[j++]=13;
no_var:
				myBuf[j++]=ch;
				}
			if(j>2000)
				break;
			}

		myBuf[j]=0;
	//	((CMainFrame *)theApp.m_pMainWnd)->m_wndStatusBar.SetWindowText(myBuf+2);
		if(textWnd && *textWnd) {
			TCHAR *p=(TCHAR *)GlobalAlloc(GPTR,2048);
			if(p) {
				_tcscpy(p,myBuf+2);
				if(::IsWindow(textWnd->m_hWnd))		// serve in chiusura...
					((CWnd *)textWnd)->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
				}
			}
	//	if(textWnd && *textWnd && ((CMainFrame *)(*textWnd))->m_wndStatusBar)
		//	((CMainFrame *)*textWnd)->m_wndStatusBar.SetPaneText(1,myBuf+2,TRUE);
  
//		myBuf[j++]=13;
		myBuf[j++]=10;
		myBuf[j]=0;

		i=0;
	try {
		if(mode & keepOpen)
			goto already_open;
		if(Open()) {
			n=SeekToEnd(); 
already_open:
			WriteString(myBuf);

			if(mode & useIndex) {
				hIndexFile->SeekToEnd();
				hIndexFile->Write(&n,sizeof(DWORD));
				}

			if(mode & flushImmediate)
				Flush();	 // rimesso... anche se rallenta!
		//FlushFileBuffers
		if(!(mode & keepOpen))
			Close();
			}
		}
	catch(CFileException e) {
		AfxMessageBox(e.m_cause);
		i=-1;
		}
	

		va_end(vl);
		inUse=0;
//		}
	return i;
  }

void CLogFile::operator<<(const TCHAR *s) {
	char myBuf[1024];
	int i,j;

	myBuf[0]=(flagInfo & 0xff) + '$';				// flag tipo riga, sempre 0!
	myBuf[1]=' ';				// marker di 'già letto'
	_tcscpy(myBuf+2,(LPTSTR)(LPCTSTR)getNow());
	j=_tcslen(myBuf);
	myBuf[j++]=' ';
	i=_tcslen(s);
	_tcsncpy(myBuf+j,s,min(i+1,1000));
	j=_tcslen(myBuf);
//	myBuf[j++]=13;
	myBuf[j++]=10;
	myBuf[j]=0;

	try {
		if(mode & keepOpen)
			goto already_open;
		if(Open()) {
			SeekToEnd(); 
already_open:
			WriteString(myBuf);

			if(mode & useIndex) {
				int n=SeekToEnd(); 
				hIndexFile->SeekToEnd();
				hIndexFile->Write(&n,sizeof(DWORD));
				}

			if(mode & flushImmediate)
				Flush();	 // rimesso... anche se rallenta!
		//FlushFileBuffers
		if(!(mode & keepOpen))
			Close();
			}
		}
	catch(CFileException e) {
		i=-1;
		}
	
	}

CString CLogFile::getNow() const {
	int m=mode & 0xffff;
	CString S;

	S.Format(_T("%02u/%02u/%02u"),
		CTime::GetCurrentTime().GetDay(),
		CTime::GetCurrentTime().GetMonth(),
		CTime::GetCurrentTime().GetYear());
	if((mode & 0xff) >= dateTime) {
		CString S2;
		S2.Format(_T("%02u:%02u:%02u"),
			CTime::GetCurrentTime().GetHour(),
			CTime::GetCurrentTime().GetMinute(),
			CTime::GetCurrentTime().GetSecond());
		S+=_T(" ");
		S+=S2;
	if((mode & 0xff) >= dateTimeMillisec) {
			CString S3;
			S3.Format(_T("%u"),/*GetTickCount*/ timeGetTime());
			S+=_T(" ");
			S+=S3;
			}
		}

	return S;
	}

CString CLogFile::getNowApache() {
	CString S;

	S.Format(_T("%02u/%s/%02u:%02u:%02u:%02u %02d00"),
		CTime::GetCurrentTime().GetDay(),
		CTimeEx::Num2Month3(CTime::GetCurrentTime().GetMonth()),
		CTime::GetCurrentTime().GetYear(),
		CTime::GetCurrentTime().GetHour(),
		CTime::GetCurrentTime().GetMinute(),
		CTime::GetCurrentTime().GetSecond(),
		-(_timezone/3600)+(_daylight ? 1 : 0)
		);

	return S;
	}

char *CLogFile::getLine(int n,char *s,UINT nMax) {
	char myBuf[1024];
	CStdioFile mF;

	if(!n)
		goto errore;

	if(mode & useIndex) {
		int myPos;

		if(mF.Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone)) {
			CFile mF2;

			if(mF2.Open(nomeFileNdx,CFile::modeRead | CFile::shareDenyNone)) {
				mF2.Seek(n*sizeof(DWORD),CFile::begin);
				mF2.Read(&myPos,sizeof(DWORD));
				mF2.Close();

				mF.Seek(myPos,CFile::begin);
				mF.ReadString(myBuf,nMax);
				mF.Close();
				_tcscpy(s,myBuf);
				}
			}
		}
	else {

		nMax=min(nMax,1000);
		if(mF.Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone)) {
			while(n) {
				if(!mF.ReadString(myBuf,nMax))
					break;
				n--;
				}
			mF.Close();
			if(!n)
				_tcscpy(s,myBuf);
			else
				goto errore;
			}
		else {
errore:
			s=NULL;
			}
		}

fine:
	return s;
	}

DWORD CLogFile::getTotLines() const {
	DWORD n=0;
	char myBuf[1024];
	CStdioFile mF;

	if(mode & useIndex) {
		if(mF.Open(nomeFileNdx,CFile::modeRead | CFile::shareDenyNone)) {
			n=mF.SeekToEnd();
			mF.Close();

			n/=sizeof(DWORD);
			}

		}

	else {
		if(mF.Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone)) {
			while(mF.ReadString(myBuf,1000))
				n++;
			mF.Close();
			}
		}

	// fare un confronto tra i due e forzare ReIndex?? oppure... usare l'uno Oppure l'altro?


	return n;
	}

int CLogFile::clearAll() {
	
	if(mode & keepOpen)
		Close();
	CStdioFile::Open(nomeFile,CFile::modeCreate);
	CStdioFile::Close();
	if(mode & useIndex)
		ReIndex();
	if(mode & keepOpen)
		Open();
	return 1;
	}

BOOL CLogFile::GetStatus(CFileStatus &fs) {

	if(mode & keepOpen)
		return CStdioFile::GetStatus(fs);
	else {
		if(Open()) {
			int i=CStdioFile::GetStatus(fs);
			Close();
			return i;
			}
		}
	}


char *CLogFile::getAsHex(const byte *s,char *d,UINT nMax) {

	while(nMax--) {
		wsprintf(d,"%02X ",*s);
		d+=3;
		s++;
		}

	return d;
	}

#ifdef _WIN32_WCE

void CStdioFileEx::WriteString(CString S) {
	
	Write((LPTSTR)(LPCTSTR)S,S.GetLength());

	}

CString CStdioFileEx::ReadString() {
	CString S;	
	char myBuf[64];
	int i;

	do {
		i=Read(myBuf,1);
		if(i<1)
			break;
		S+=*myBuf;
		if(*myBuf=='\n')
			break;
		} while(1);

	return S;
	}

#endif

int CLogFile::ReIndex() {
	int i=0,n;
	CFile hTmpIndexFile;
	CStdioFile mF;

	if(!(mode & useIndex))
		return -1;

	if(mode & keepOpen) {
		Close();
		if(hIndexFile) 
			hIndexFile->Close();
		}

	getIndexFileName();

	hTmpIndexFile.Open(nomeFileNdx,CFile::modeCreate | CFile::modeReadWrite);

	n=0;
	hTmpIndexFile.Write(&n,sizeof(DWORD));

	if(mF.Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone)) {
		char myBuf[1024];

		i=1;

		while(mF.ReadString(myBuf,1000) > 0) {
			n=mF.GetPosition();
			hTmpIndexFile.Write(&n,sizeof(DWORD));
			}
		mF.Close();
		}

	hTmpIndexFile.Close();

	if(mode & keepOpen)
		Open();

	return i;
	}

CString CLogFile::getIndexFileName() {
	int i;

	i=nomeFile.Find('.');
	if(i>=0) {
		nomeFileNdx=nomeFile.Left(i+1)+"ndx";
		}
	else {
		nomeFileNdx=nomeFile+".ndx";
		}

	return nomeFileNdx;
	}

int CLogFile::RenameAndStore(int how) {
	int i;
	CString S,S1;

	if(mode & keepOpen) {
		Close();
		if(hIndexFile) 
			hIndexFile->Close();
		}

	S1=nomeFile;
	S1=S1.Left(S1.Find('.'));
	S1=S1.Mid(S1.ReverseFind('\\'));
	S=S1+CTime::GetCurrentTime().Format("_%Y_%m_%d.txt");
//	S=nomeFile+'\\'+S+".txt";

	i=1;

	TRY
	{
		Rename(nomeFile,S);
	}
	CATCH( CFileException, e )
	{
		i=0;

    #ifdef _DEBUG
        afxDump << "Impossibile rinominare File " << nomeFile << " , cause = "
            << e->m_cause << "\n";
    #endif
	}
	END_CATCH

	clearAll();
	

	if(mode & keepOpen)
		Open();

	return i;
	}



//--------------------------------------------------------------
CReadLogFile::CReadLogFile(const CString s, DWORD m) { 

	nomeFile=s;
	mode=m;
	hIndexFile=NULL;
	curpos=0;

	if(mode & keepOpen) {
	try {
		Open();
		}
	catch(CFileException e) {
		;
		}
	
		}

	if(mode & useIndex)
		hIndexFile=new CFile;
	}

CReadLogFile::CReadLogFile(CFile *f2,DWORD m) { 

	m_hFile=f2->m_hFile;
	mode=m;
	hIndexFile=NULL;
	curpos=0;

	mode &= ~keepOpen;	
	
	if(mode & keepOpen) {
	try {
		Open();
		}
	catch(CFileException e) {
		;
		}
	
		}

	if(mode & useIndex)
		hIndexFile=new CFile;
	}

CReadLogFile::~CReadLogFile() { 

	if(hIndexFile)
		delete hIndexFile;			hIndexFile=NULL;
	}


int CReadLogFile::Open() { 
	int i;
	
	i=CStdioFile::Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone);

	if(mode & useIndex) {
		getIndexFileName();
		if(hIndexFile)
			hIndexFile->Open(nomeFileNdx,CFile::modeRead | CFile::shareDenyNone);
		}
	
	return i;
	}

void CReadLogFile::Close() { 
	
	CStdioFile::Close();

	if(mode & useIndex) {
		if(hIndexFile)
			hIndexFile->Close();
		}
	}


int CReadLogFile::read(TCHAR *s,...) {
	// testare il primo carattere: dev'essere $ !
//	char myBuf[1024];
	int i=-1;

rifo:
	if(ReadString(s,1023)) {
		if(s[0] == '$')
			i=1;
		else {
			i=0;
			goto rifo;
			}
		}
	return i;
	}

int CReadLogFile::read(CTime *ct, double *valori,int maxSize) {
	// testare il primo carattere: dev'essere $ !

	int i,n;
	char myBuf[1024],*p;

	i=read(myBuf);
	if(i>=1) {
		p=myBuf+2+20+2;
		n=0;
		do {
			while(!isdigit(*p) && *p!='\t' && *p!= 'n' && *p!='-')
				p++;
			if(isdigit(*p) || *p=='-') {		// anche + ??
				valori[n]=atof(p);
				}
			while(*p!='\t' && *p!= 'n')
				p++;
			if(*p=='\n')
				break;
			p++;
			n++;
			i=n;
			} while(n<maxSize);
		}

	return i;
	}

double CReadLogFile::operator>>(double & d) {
	char myBuf[1024];
	CString S;
	int i,j;

	try {
		if(mode & keepOpen)
			goto already_open;
		if(Open()) {
			SeekToEnd(); 
already_open:
			ReadString(S);

			curpos=Seek(0,CFile::current);

			if(!(mode & keepOpen))
				Close();
			}
		}
	catch(CFileException e) {
		i=-1;
		}

	return d;
	}

CString CReadLogFile::operator>>(CString & S) {
	char myBuf[1024];
	int i,j;

	try {
		if(mode & keepOpen)
			goto already_open;
		if(Open()) {
			SeekToEnd(); 
already_open:
			ReadString(S);

			curpos=Seek(0,CFile::current);

			if(!(mode & keepOpen))
				Close();
			}
		}
	catch(CFileException e) {
		i=-1;
		}

	return S;
	}



char *CReadLogFile::getLine(int n,char *s,UINT nMax) {
	char myBuf[1024];
	CStdioFile mF;

	if(!n)
		goto errore;

	if(mode & useIndex) {
		int myPos;

		if(mF.Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone)) {
			CFile mF2;

			if(mF2.Open(nomeFileNdx,CFile::modeRead | CFile::shareDenyNone)) {
				mF2.Seek(n*sizeof(DWORD),CFile::begin);
				mF2.Read(&myPos,sizeof(DWORD));
				mF2.Close();

				mF.Seek(myPos,CFile::begin);
				mF.ReadString(myBuf,nMax);
				mF.Close();
				_tcscpy(s,myBuf);
				curpos=Seek(0,CFile::current);
				}
			}
		}
	else {

		nMax=min(nMax,1000);
		if(mF.Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone)) {
			while(n) {
				if(!mF.ReadString(myBuf,nMax))
					break;
				n--;
				}
			mF.Close();
			if(!n) {
				_tcscpy(s,myBuf);
				curpos=Seek(0,CFile::current);
				}
			else
				goto errore;
			}
		else {
errore:
			s=NULL;
			}
		}

fine:
	return s;
	}

DWORD CReadLogFile::getTotLines() const {
	DWORD n=0;
	char myBuf[1024];
	CStdioFile mF;

	if(mode & useIndex) {
		if(mF.Open(nomeFileNdx,CFile::modeRead | CFile::shareDenyNone)) {
			n=mF.SeekToEnd();
			mF.Close();

			n/=sizeof(DWORD);
			}

		}

	else {
		if(mF.Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone)) {
			while(mF.ReadString(myBuf,1000))
				n++;
			mF.Close();
			}
		}

	// fare un confronto tra i due e forzare ReIndex?? oppure... usare l'uno Oppure l'altro?


	return n;
	}


char *CReadLogFile::getLineFromTimestamp(CTime ts,char *s,UINT nMax) {
	char myBuf[1024];
	CStdioFile mF;

	if(ts==0)
		goto errore;

	if(mF.Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone)) {
		while(1) {
			if(!mF.ReadString(myBuf,nMax))
				break;
//FINIRE!
//			_tcscpy(s,myBuf);
			curpos=Seek(0,CFile::current);
			}
		mF.Close();
		if(!*s)
			goto errore;
		}
	else {
errore:
		s=NULL;
		}

fine:
	return s;
	}


int CReadLogFile::gotoTimestamp(CTime ts,BOOL useNextIfNotFound) {
	int t,n=-1;
	char myBuf[1024];
	CStdioFile mF;
	DWORD tempPos;

	if(ts==0)
		goto fine;

	if(!(mode & keepOpen)) 
		goto fine;

//	if(Open(nomeFile,CFile::modeRead | CFile::typeText | CFile::shareDenyNone)) {
		t=0;
		n=0;
		while(1) {
			char *p;
			WORD nDay,nMonth,nYear,nHour,nMinute,nSecond;

			tempPos=Seek(0,CFile::current);
			if(!ReadString(myBuf,1023))
				break;

			p=myBuf+2;
			nDay=atoi(p);
			nMonth=atoi(p+3);
			nYear=atoi(p+6);
			nHour=atoi(p+11);
			nMinute=atoi(p+14);
			nSecond=atoi(p+17);

			if(useNextIfNotFound){			//finire se non c'è data/ora esatta!
				if(nDay==ts.GetDay() && nMonth==ts.GetMonth() && nYear==ts.GetYear() &&
					nHour==ts.GetHour() && nMinute==ts.GetMinute() && nSecond==ts.GetSecond()) {
					CTime myT(nYear,nMonth,nDay,nHour,nMinute,nSecond);
					currTimePos=myT;
					n=1;
					Seek(tempPos,CFile::begin);			// recupero ultima riga letta
					break;
					}
				}
			else {
				if(nDay==ts.GetDay() && nMonth==ts.GetMonth() && nYear==ts.GetYear() &&
					nHour==ts.GetHour() && nMinute==ts.GetMinute() && nSecond==ts.GetSecond()) {
					CTime myT(nYear,nMonth,nDay,nHour,nMinute,nSecond);
					currTimePos=myT;
					n=1;
					Seek(tempPos,CFile::begin);			// recupero ultima riga letta
					break;
					}
				}

//			_tcscpy(s,myBuf);
//			if 
//				n=t;
				curpos=Seek(0,CFile::current);

			t++;
			}
//		mF.Close();
//		}

fine:
	return n;
	}

int CReadLogFile::skipTimestamp(CTimeSpan ts,BOOL useNextIfNotFound) {
	int t,n=-1;
	CTime myT=currTimePos;

	myT+=ts;
	n=gotoTimestamp(myT,useNextIfNotFound);

	return n;
	}



CString CReadLogFile::getIndexFileName() {
	int i;

	i=nomeFile.Find('.');
	if(i>=0) {
		nomeFileNdx=nomeFile.Left(i+1)+"ndx";
		}
	else {
		nomeFileNdx=nomeFile+".ndx";
		}

	return nomeFileNdx;
	}
