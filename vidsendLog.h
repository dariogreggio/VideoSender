
#ifndef _JOSHUA_LOG_INCLUDED_
#define _JOSHUA_LOG_INCLUDED_


#ifdef _WIN32_WCE

class CStdioFileEx : public CStdioFile {
public:
	void WriteString(CString);
	CString ReadString();
	};

#endif



class CLogFile : public CStdioFile {
	// in VC42 non riuscivo a ereditare da CStdioFile!!! dà errore del c. con GetFileTitle
public:
	enum timeStampTypes {
		dontUseDate=0,
		date=1,
		dateTime=2,
		dateTimeMillisec=3,
		flagInfo=0x100,
		flagInfo2=0x100,
		flagValue=0x101,
		flagError=0x102,
		flagError2=0x103,
		flagWarning=0x104,
		flagAlert=0x105,
		flushImmediate=0x80000000,
		keepOpen=0x40000000,
		useIndex=0x20000000
		};
private:
	CString nomeFile,nomeFileNdx;
	const CWnd *textWnd;		// se c'e', indica dove visualizzare la riga di log
	DWORD mode;
	CFile *hIndexFile;
public:
	CLogFile(const CString,const CWnd *myWnd=NULL,DWORD m=dateTime | flushImmediate);
	CLogFile(CFile *f2,const CWnd *myWnd=NULL,DWORD m=dateTime);
	~CLogFile();
	int print(int m,const TCHAR *s,...);		 // m=0 info, 1= letture skynet, 2=errore
	int Open();
	void Close();
	void operator<<(const TCHAR *);
	int ReIndex();
	CString getNow() const;
	int RenameAndStore(int how=0 /* default: appende GG/MM/AAAA*/);
	static CString getNowApache();
	char *getLine(int ,char *,UINT nMax=255);
	DWORD getTotLines() const;
	CString getIndexFileName();
	BOOL GetStatus(CFileStatus &);
	int clearAll();
	static char *getAsHex(const byte *,char *,UINT );

private:
//	CString GetFileTitle() { CString a; return a; };
	};


class CReadLogFile : public CStdioFile {
public:
	enum timeStampTypes {
		keepOpen=0x40000000,
		useIndex=0x20000000
		};
private:
	CString nomeFile,nomeFileNdx;
	DWORD mode,curpos;
	CTime currTimePos;
	CFile *hIndexFile;
public:
	CReadLogFile(const CString,DWORD m=useIndex | keepOpen);
	CReadLogFile(CFile *f2,DWORD m=useIndex | keepOpen);
	~CReadLogFile();
	int read(TCHAR *s,...);	
	int read(CTime *, double *,int);
	CTime readTimestamp();
	int Open();
	void Close();
	double operator>>(double &);
	CString operator>>(CString &);
	char *getLine(int ,char *,UINT nMax=255);
	DWORD getTotLines() const;
	char *getLineFromTimestamp(CTime,char *,UINT nMax=255);
	int gotoTimestamp(CTime,BOOL useNextIfNotFound=FALSE);
	int skipTimestamp(CTimeSpan,BOOL useNextIfNotFound=FALSE);
	CString getIndexFileName();
	BOOL GetStatus(CFileStatus &);

private:
//	CString GetFileTitle() { CString a; return a; };
	};


#endif
