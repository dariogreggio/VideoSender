#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

class CLineText {

public:
	enum {
		maxTextLine=512,		// per richieste lunghe HTML
		defBuffSize=32768  //2048*2; 2018
		};

	char commLogin[64],commPswd[32];
	int size;
	BYTE *sbuff;
	DWORD sbuffReadWhere,sbuffWriteWhere;
	DWORD sbuffSize;
	inline DWORD ssize() const { if(sbuffWriteWhere>sbuffReadWhere) return sbuffWriteWhere-sbuffReadWhere; else if(sbuffWriteWhere<sbuffReadWhere) return sbuffWriteWhere-sbuffReadWhere+sbuffSize; else return 0;};

	BOOL handleReadData(const BYTE *, DWORD);
	int readText(char *,int echo=0,int maxLength=maxTextLine);
	int readNText(BYTE *,int );
	BYTE *readTextRaw(BYTE *, int *);
	inline char getAt(int where=0) const;
	inline char operator[](int where) const;
	int indexOf(const char *);
	inline char *addChar(char);
	void clear();
	void clearAll();
	void setEcho(int bEcho,CSocket *es=NULL,DWORD wThread=0);
	void operator>>(char *);
	void skip(int n=1);

	CLineText(int bEcho=0,CSocket *es=NULL,DWORD wThread=0,DWORD bs=defBuffSize);
	~CLineText();

protected:
	CSocket* echoSocket;
	DWORD writeThread;
	BOOL bEcho;
	int critical;
	};

