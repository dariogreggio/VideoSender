#include <afxsock.h>
#include "afxtempl.h"
#include <vfw.h>


#define AUDIO_SOCKET 7603		// in vidsend 1.0 erano 0xff2f e 0xff2e
#define VIDEO_SOCKET 7602
#define CONTROL_SOCKET 7600
#define TEXT_SOCKET 7601
#define DIRECTORY_SOCKET 7604
#define AUTHENTICATION_SOCKET 7605
// la condivisione Internet di Win98SE accetta solo porte < 1024!

class CLineText;
class CExDocument;

struct AV_PACKET_HDR {
	DWORD tag;
	WORD type;			// video=0,audio=1
//	WORD psec;			// mSec per frame
	long len;
	DWORD timestamp;
	DWORD info;			// p.es. keyFrames=AVIIF_KEYFRAME=0x10
	WORD reserved1,reserved2;		// usati come cnt per buffer Asyncroni
	void *lpData;		// a volte i byte seguono la struct, a volte sono puntati da qua!
  };

#define AV_PACKET_HDR_SIZE (sizeof(struct AV_PACKET_HDR)-sizeof(BYTE *)) 

class CSocketEx : public CSocket {
public:
	static int getIPAddress(char *,int q=0);
	static int getMyIPAddress(char *,int q=0);
	static int getMyIPAddress(SOCKADDR_IN *,int q=0);
	static char *getMyOutmostIPAddress(char *,BOOL cached=TRUE);
	static char *getMyMACAddress(char *,int q=0);
	static int IsLocalAddress(const char *);
	~CSocketEx();
	};

typedef struct _NTP_PACKET {
	struct {
		BYTE mode			: 3;
		BYTE versionNumber 	: 3;
		BYTE leapIndicator	: 2;
		} flags;

	BYTE stratum;
	CHAR poll;
	CHAR precision;
	DWORD root_delay;
	DWORD root_dispersion;
	DWORD ref_identifier;
	DWORD ref_ts_secs;
	DWORD ref_ts_fraq;
	DWORD orig_ts_secs;
	DWORD orig_ts_fraq;
	DWORD recv_ts_secs;
	DWORD recv_ts_fraq;
	DWORD tx_ts_secs;
	DWORD tx_ts_fraq;
	} NTP_PACKET;

#define NTP_EPOCH 				(86400ul * (365ul * 70ul + 17ul))	// 0:0:0 1/1/1970

class CTimeSocket : public CSocketEx {
public:
	enum {
		timeServerTCP=1,
		timeServerUDP=2
		};
protected:
	DWORD type;
public:
	void OnAccept(int);
	BOOL Create(DWORD type=timeServerTCP);
//	~CTimeSocket();
	void OnReceive(int );
	};


typedef enum { BROWSER_NETSCAPE=0,BROWSER_EXPLORER,BROWSER_OPERA,BROWSER_MOZILLA,BROWSER_FIREFOX,BROWSER_SAFARI } BROWSER_USER_AGENTS;

struct MSG_HEADER {
	int cmd;
	int status;
	DWORD protocolVersion;
	char host[128];
	char nomefile[256];
	char contentLocation[256];
	char contentType[128];
	char content[1024];
	char referer[128];
	char userAgent[64];
	DWORD userAgentVersion;
	BROWSER_USER_AGENTS userAgentType;
	char accept[1024];
	char acceptLanguage[128];
	char acceptEncoding[128];
	char acceptCharset[128];
	char userUser[64];
	char userPasw[64];
	BOOL keepAliveR,WML;
	BOOL keepAliveS;
	BOOL pragmaNoCache;
	int howCache,contentLength;
	CTime expires,ifModifiedSince;
	};

enum mimeTypes {
	mimeTypeHtml=0,
	mimeTypeText,
	mimeTypeXml,
	mimeTypeXhtml,
	mimeTypeGif,
	mimeTypeJpeg,
	mimeTypePng,
	mimeTypeBmp,
	mimeTypeTiff,
	mimeTypeWav=11,
	mimeTypeMid,
	mimeTypeMp3,
	mimeTypeAvi,
	mimeTypeZip=21,
	mimeTypePdf=22,
	mimeTypeCss=23,
	mimeTypeApk=24,
	mimeTypeJs=25,
	mimeTypeWml=91,
	mimeTypeWbmp,
	mimeTypeUnknown=99
	};


//#define USA_AUTENTICAZIONE 1

class CWebSrvSocket;
class CHtmlFile;

class CXMLValue /*: CString*/ {
public:
	static const TCHAR *defaultPrologueString,*defaultEpilogueString,*errore_string;
	CString prologue_string,epilogue_string;
public:
	CString Write(CString);
	CString Write(LPCTSTR);
	CString Write(double n);
	CString Write(DWORD);
	CString Write(CTime);
	static CString normalizeName(CString,BOOL alsoBlank=TRUE);
	void Reset() { m_Stringa.Empty(); };
	CString setHeader(CString title) { m_Title=title; };
	CString getValue() const { return m_Stringa; };
	CString getHeader() const { return m_Title; };
	int getLength() const { return m_Stringa.GetLength(); };
	void setFormatString(CString fs) { formatSpec=fs; };
	CXMLValue();
	CXMLValue(CString formatString);
	CXMLValue(CString title,CString formatString);
	void init();
private:
	CString m_Stringa,m_Title;
	CString formatSpec;
	};

class CXMLSchema {
public:
	static const TCHAR *header,*footer;
	CArray < CXMLValue *, CXMLValue *> iCampi;

public:
	int PrintHeader();
	int Print(CString S1,...);
	int Print(LPCTSTR,...);
	int Print(double d,...);
	int PrintOnecolumn(WORD ,double ,int type=0);
	int PrintOnecolumn(WORD ,CString ,int type=0);
	int Define(CStringArray &);
	CString GetBuffer();
	DWORD GetSize() const { return iCampi.GetSize(); }
	CXMLSchema();
	CXMLSchema(CStringArray &);
	~CXMLSchema();

private:
	CMemFile m_MemFile;
	};

class CXMLFile : public CStdioFile {
public:
	enum timeStampTypes {
		dontUseDate=0,
		date=1,
		dateTime=2,
		dateTimeMillisec=3,
		useIndex=0x20000000
		};
public:
	CXMLSchema m_Schema;
public:
	CXMLFile(const CString,DWORD m=dateTime);
	CXMLFile(CFile f2,DWORD m=dateTime);
	~CXMLFile();
	int print(int m,const TCHAR *s,...);
	int print(double d,...);
	int Open();
	void Close();
	void operator<<(const TCHAR *);
	int ReIndex();
	CString getNow();
//	char *getLine(int ,char *,UINT nMax=255);
//	DWORD getTotLines();
	CString getIndexFileName();
	BOOL GetStatus(CFileStatus &);
	int clearAll();
	int DefineSchema(CStringArray & SA) { return m_Schema.Define(SA); };

private:
	CString nomeFile,nomeFileNdx;
	const CWnd *textWnd;		// se c'e', indica dove visualizzare la riga di log
	DWORD mode;
	CFile *hIndexFile;
	};


class CHTTPHeader {
public:
	enum {
		TAG_HTTP=1,
		TAG_SERVER,
		TAG_HOST,
		TAG_REFERER,
		TAG_DATE,
		TAG_USERAGENT,
		TAG_CONTENT_TYPE,
		TAG_CONTENT_LENGTH,
		TAG_CONTENT_DISPOSITION,
		TAG_CONTENT_LOCATION,
		TAG_CONNECTION,
		TAG_CACHE_CONTROL,
		TAG_EXPIRES,
		TAG_KEEPALIVE,
		TAG_ETAG,
		TAG_LOCATION,
		TAG_MODIFIED,
		TAG_IFMODIFIED,
		TAG_AUTHENTICATE,
		TAG_AUTHORIZATION,
		TAG_ALLOW,
		TAG_TRANSFER_ENCODING,
		TAG_ACCEPT,
		TAG_ACCEPT_CHARSET,
		TAG_ACCEPT_ENCODING,
		TAG_ACCEPT_LANGUAGE,
		TAG_ACCEPT_RANGES,
		TAG_SET_COOKIE
		};
protected:
	CString m_Buffer;
public:
	static const TCHAR *tagHttp,*tagServer,*tagDate,*tagHost,*tagReferer,*tagUserAgent,
		*tagContentType,*tagContentLength,*tagContentDisposition,*tagContentLocation,*tagAcceptLanguage,*tagAcceptEncoding,*tagAcceptCharset,
		*tagConnection,*tagCacheControl,*tagExpires,*tagKeepAlive,*tagEtag,*tagLocation,*tagModified,*tagAuthenticate,*tagAccept,
		*tagAllow,*tagAuthorization,*tagAcceptRanges,*tagSetCookie,*tagTransferEncoding,*tagPragma,
		*tagNoCache,*tagIfModifiedSince,*tagProxyConnection,*tagFileUpload;
public:
	void Reset() { m_Buffer.Empty(); 	m_Buffer.GetBufferSetLength(1024); m_Buffer.ReleaseBuffer();
		m_Buffer="0123456789012345678901234567890123456789";
	m_Buffer+="0123456789012345678901234567890123456789";
	m_Buffer+="0123456789012345678901234567890123456789";
	m_Buffer+="0123456789012345678901234567890123456789";
	m_Buffer+="0123456789012345678901234567890123456789";
	m_Buffer+="0123456789012345678901234567890123456789";
	m_Buffer+="0123456789012345678901234567890123456789";
	m_Buffer+="0123456789012345678901234567890123456789x";

	m_Buffer.Empty();
}
	void Finalize();
	const char *AddToken(int,const char *,const char *s2=NULL,CTime t1=NULL,DWORD n1=0);
	CString GetBuffer() const { return m_Buffer; }
	operator LPCTSTR() const { return m_Buffer; }		// occhio a usarlo su Puntatore-a...
	DWORD GetLength() const { return m_Buffer.GetLength(); }
	CHTTPHeader();
//	~CHTTPHeader();
	};

class CHTTPBody : public CMemFile {
public:
	void WriteString(LPCTSTR s) { Write(s,_tcslen(s)); }
//	void WriteBodyString() { boh? }
//	void fprintf(LPCTSTR s) { Write(s,_tcslen(s)); }
	void Reset() { SetLength(0); }
//	CHTTPBody(int setDefault=1) { if(setDefault) WriteString(Html4String); }
	int dumpToFile(LPCTSTR nomefile);
	};

class CWebSrvSocket2_base : public CSocketEx {
public:
	enum {
		loggedOn=1,
		loggedAsUser=2,
		loggedAsSupervisor=4,
		loggedAsGod=8,
		};
	enum {
		HTTP_AUTHORIZATION_PLAINTEXT=1,
		HTTP_AUTHORIZATION_BASIC=2
		};

public:
	struct MSG_HEADER Msg;
	static const TCHAR *copyString;
	static const TCHAR *okString;
	static const TCHAR *Html4String,*HtmlContent,*XmlString;
	static const TCHAR *WAPcopyString,*WAPBackString,*WAP1String,*WAPHeaderString;
	static const TCHAR *passwordString,*yesNoString,*passwordL1,*passwordL2;
	static const TCHAR *CRLF,*CRLF2;
	char bodyString[256];
private:

protected:
	CLineText *m_LineText;
	CWebSrvSocket *m_Parent;
	CHTTPHeader *m_HTTPHeader;
	CHTTPBody *m_Body;
	CString m_Peer;
	DWORD proprieta,sessionID,mSeq;
	CString sessionUser;
	DWORD sessionLastAct;
	int subPageCnt;				//x WAP 1.0, spezza in sottopagine...

public:
	virtual char *getBodyString(DWORD color=0x000000,const char *extraStr=NULL);
	CWebSrvSocket *getParent() const { return m_Parent; }
//	void setAttributi(CJoshuaUtentiSet *,BOOL doSynch=FALSE);
	int IsLocalPeer() const;
	DWORD getProprieta() { return proprieta; }
	CString getSessionUser() { return sessionUser; }
	BOOL getPeer() { UINT P; return GetPeerName(m_Peer,P); }
	static int setMimeType(const char *);
	static char *getMimeTypeDescr(int );
	static BYTE BCD2Hex(const char *);
	static char *compattaHex(char *);
	static const char *stristr(const char *,const char *);
	static char *parseParm(const char *,char *, UINT, char *, UINT, int q=1,int autoSkip=1);
	static int parseNumParm(const char *s,char *parmN, UINT parmNL, char *parmV, UINT parmVL, int q=1,int autoSkip=1);
	static char *parseNamedParm(const char *,const char *, char *, UINT);
	static char *parseFormParm(const char *,const char *, char *, char *);
	static int parseQuery(CLineText *,MSG_HEADER *);
	DWORD subBuildPageDir(const char *, const char *path=NULL,int sort1=0,int sort2=0);
	//virtual int subBuildPageStatus(int *);
	int subBuildPageExt(const char *, const char *, MSG_HEADER *, CTime *);
	virtual int buildHTMLPage(int,const char *,const char *);
	virtual int buildWMLPage(int,const char *,const char *);
	virtual int buildCGIPage(const char *,const char *,int *,int *,int isWML);

	CString prepareHTTPHeader(const char *,int,CTime);
	int SendHeader();
	int SendBody();
	BOOL doLogin(const char *nome,const char *pasw,int *status);
	static BOOL ParseAuthorization(const CString& sField, CString& sUsername, CString& sPassword);
	void openSession();
	void refreshSession();
	void closeSession(BOOL bForce=FALSE);
	void OnReceive(int);
	void OnClose(int);

	CHTTPHeader *getHeader() const { return m_HTTPHeader; }
	CHTTPBody *getBody() const { return m_Body; }
	
	CWebSrvSocket2_base(CWebSrvSocket *);
	~CWebSrvSocket2_base();
	friend class CWebSrvSocket;
	friend class CProxySrvSocket2_WWW;
	};

class CWebSrvSocket : public CSocketEx {
public:
	enum {
		createLog=1,		// solo primo accesso
		createLogAll=2,	// tutti gli accessi
		useSessions=0x100
		};
	enum {
		maxClientConnections=32,			// 2004
		sessionRefresh=30000
//		sessionTimeout=900000 v. joshuaSet.h - durataSessione
		};
public:
	DWORD HTTPVer,WWWVer,WAPVer;
	char *WWWRoot,*CGIPath,*XSLRoot;
	DWORD sentBytes,recvBytes;

protected:
	DWORD opzioni;
	CDatabase *m_DB;

private:
	CList < CWebSrvSocket2_base *, CWebSrvSocket2_base * > cSockRoot;
#ifdef USA_AUTENTICAZIONE 
	CJoshuaUtentiSet *m_Set;
#endif
	int maxConn;
	UINT port;
	char ipaddr[32];
	char logFormat[32];
  /*Here are all the % sequences allowed in the configurable log format in Apache.
	%b 	bytes sent, excluding HTTP headers
	%f 	filename
	%h 	remote host
	%{Header}i 	The contents of Header: header line(s) in the request sent from the client
	%l 	remote username (from identd, if supplied)
	%{Note}n 	The contents of note "Note" from another module
	%{Header}o 	The contents of Header: header line(s) in the reply
	%p 	the port the request was served to
	%P 	the process ID of the child that serviced the request
	%r 	first line of request
	%s 	response status. For requests that got internally redirected, this is status of the original request: use %>s for the returned status
	%t 	time, in common log format time format
	%{format}t 	The time, in the form given by format, which should be in strftime format
	%T 	the time taken to serve the request, in seconds
	%u 	remote user (from auth; may be bogus if return status (%s) is 401)
	%U 	the URL path requested
	%v 	the name of the server (i.e. the virtual host)*/

public:
	BOOL Create();
	void OnAccept(int);
	void doDelete(CWebSrvSocket2_base *);
	void clearAll();
	void resetCounters();
	void sincronizzaSessioni(CWebSrvSocket2_base *);
	int getNPort() const  { return port; }
	int getNConn() const { return maxConn; }
	CDatabase *getDB() const { return m_DB; }
#ifdef USA_AUTENTICAZIONE 
	CJoshuaUtentiSet *getUtentiSet() const { return m_Set; }
#endif
	POSITION getClientRoot() const { return cSockRoot.GetHeadPosition(); }
	CWebSrvSocket2_base *getNextClient(POSITION& po) const { return cSockRoot.GetNext(po); }

	CWebSrvSocket(UINT nPort=80,const char *ipscelto=NULL,DWORD opt=createLog,CDatabase *db=NULL);
	~CWebSrvSocket();
	friend class CWebSrvSocket2_base;
	};

class CWebSrvSocket2_vidsend : public CWebSrvSocket2_base {
public:
	BYTE *getWebcam(DWORD *len,RECT *rc,int);
	char *subBuildPageJUKE(const char *, char *, DWORD *,int *,int *);
//	char *subBuildPageStatus(char *, DWORD *,int *);
	int buildHTMLPage(int,const char *,const char *);
	int buildWMLPage(int,const char *,const char *);
	int buildCGIPage(const char *,const char *,int *,int *,int isWML);
	CWebSrvSocket2_vidsend(CWebSrvSocket *p) : CWebSrvSocket2_base(p) { };
	};



class CWebCliSocket : public CSocketEx {
public:
	enum {
		NO_PROXY,
		AUTO_PROXY,
		ALWAYS_PROXY,
		PROXY_BASE,
		PROXY_SOCKS4
		};
	enum {
		MAX_HEADER_LEN=4096
		};
public:
	DWORD HTTPVer;
	CString m_Agent;
	CHTTPHeader *m_HTTPHeader;
protected:
	int m_ProxyMode,isSecure;
public:
	BOOL Connect(LPCTSTR,BOOL bSecure=FALSE,WORD port=IPPORT_HTTP /*o IPPORT_HTTPS */ );
	BOOL Disconnect();
	int Send(CString);
	CString buildQuery(CString, CString ,int m=0,int proxyMode=0);		// 0=GET, 1=PUT
	int sendQuery(CString );
	void addHeader(int,const char *,const char *s2=NULL,CTime t1=NULL,DWORD n1=0);
	int authorize(const char *,const char *,int tipo=CWebSrvSocket2_base::HTTP_AUTHORIZATION_BASIC);
	int readHeader(char *buf=NULL,DWORD len=0,WORD timeout=30);		// posso passare NULL se voglio solo saltarlo!
	int readPage(char *,DWORD,WORD timeout=45);
	int getPage(const char *,char *,DWORD,BOOL doRedirect=TRUE);
	static int getBody(const char *,char *,DWORD);
	int parseResponse(char *);
	static char *getXMLvalue(const char *inbuf,char *key,char *buf,DWORD len);
	static double getXMLvalue(const char *inbuf,char *key);
	static CString translateString(const char *);
public:
	CWebCliSocket(const char *,int bProxyMode=NO_PROXY  /* FINIRE ! AUTO_PROXY*/);
	~CWebCliSocket();
	};

class CBase64Decoder {
public:
//Methods
	int Decode(const CString& sInput, CString& sOutput);

protected:
	void WriteBits(UINT nBits, int nNumBts, LPTSTR szOutput, int& lp);

	int m_nBitsRemaining;
	ULONG m_lBitStorage;
	LPCTSTR m_szInput;
	static CString m_sBase64Alphabet;
	};


//Encapsulation of an SMTP File attachment
class CSMTPAttachment {
public:
	CSMTPAttachment();
  ~CSMTPAttachment();

//methods
  BOOL Attach(const CString& sFilename);
	BOOL Attach(const BYTE *, DWORD , const char *fileName=NULL);
	BOOL Attach(const char *, const char *filename=NULL);
  CString GetFilename() const { return m_sFilename; };
  const char* GetEncodedBuffer() const { return m_pszEncoded; };
  int GetEncodedSize() const { return m_nEncodedSize; };
  static int Base64BufferSize(int nInputSize);
  static BOOL EncodeBase64(const char* aIn, int aInLen, char* aOut, int aOutSize, int* aOutLen);

protected:
  static char m_base64tab[];

  CString  m_sFilename;    //The filename you want to send
  char*    m_pszEncoded;   //The encoded representation of the file
  int      m_nEncodedSize; //size of the encoded string
	};


class CStreamSrvSocket;

class CStreamSrvSocket2 : public CAsyncSocket {
public:
	enum {
		xOff=1,
		inSend=2,
		stalled=4,
		waitResync=8
		};
public:
	CLineText *myLineText;
	DWORD status;
	DWORD sentFrame,stops,skippedFrame;
	CTime connectTime;		// per connessione a tempo!
	int priority;					// per prioritizzare e differenziare le connessioni...
	CStreamSrvSocket *myParent;
	// usati da Manda()
	const BYTE *mandaBuf1;
	const BYTE *oldMandaBuf1;
	DWORD mandaBuf1size;
	DWORD inSendTimeout;
	struct AV_PACKET_HDR *myAvh;

public:
	int subManda();
	int Manda(const struct AV_PACKET_HDR *,int );
	inline BOOL isXOn() {	return (m_hSocket != INVALID_SOCKET && !(status & CStreamSrvSocket2::xOff)); }
	void OnReceive(int);
	void OnClose(int);
	void OnSend(int);

	CStreamSrvSocket2(CStreamSrvSocket *p=NULL);		// TOGLIERE default quando implementi la lista e togli l'array di client!!!
	~CStreamSrvSocket2();
	friend class CStreamSrvSocket;
	};

class CStreamSrvSocket : public CSocketEx {
public:
	int maxConn;
	CList < CStreamSrvSocket2 *, CStreamSrvSocket2 * > cSockRoot;
	int flag1,flag2;

public:
	void OnAccept(int);
	void doDelete(CStreamSrvSocket2 *);
	int Manda(const struct AV_PACKET_HDR *,int);

	CStreamSrvSocket(CDocument *p);
	~CStreamSrvSocket();
protected:
	CDocument *myParent;
	};


class CStreamCliSocket : public CSocketEx {
public:
	enum { frameTimeout=2000 };
public:
	DWORD receivedFrame,stops,skippedFrame;
protected:
	CLineText *myLineText;
	CWnd *theWnd;
	struct AV_PACKET_HDR *anAvh;
	struct AV_PACKET_HDR **sBuffer;
	DWORD firstIn,lastOut,ok2In,ok2Out,totBytesReceived;
	DWORD maxBuffers,lowBuffers,highBuffers;
	BOOL critical;
	DWORD lastGoodFrame;

public:
	inline int getFirstIn() { return firstIn; };
	inline int getLastOut() { return lastOut; };
	inline DWORD getBytesReceived() { DWORD n=totBytesReceived; totBytesReceived=0; return n; };
	inline struct AV_PACKET_HDR *getOutBuffer() { 
		return ok2Out ? sBuffer[lastOut] : NULL; }
	inline BOOL roomForBuffers() { 
		int n=firstIn-lastOut; if(n<=0) n+=maxBuffers;	return n<= maxBuffers; };
	inline BOOL needMoreBuffers() { 
		int n=firstIn-lastOut; if(n<=0) n+=maxBuffers;	return n<= highBuffers /*(((highBuffers-lowBuffers)/2)+1)*/; };
	inline WORD totAvailBuffers() { 
		int n=firstIn-lastOut; if(n<=0) n+=maxBuffers; return n; }
	inline WORD getMaxBuffers() { return maxBuffers;	}
	inline WORD getLowBuffers() { return lowBuffers;	}
	void bumpOutBuffer();
	void addInBuffer(struct AV_PACKET_HDR *);
	void emptyBuffers();
	void initBuffers(int,DWORD bl=0,DWORD bh=0);
	static void delay(DWORD);
	void OnClose(int);
	CStreamCliSocket(CWnd *w=NULL);
	~CStreamCliSocket();
	};

class CStreamVCliSocket : public CStreamCliSocket {
public:
public:
protected:

public:
	void OnReceive(int);
	CStreamVCliSocket(CWnd *,void *p=NULL,int numBuff=32);
	~CStreamVCliSocket();
	};

class CStreamACliSocket : public CStreamCliSocket {
public:
public:
	int totBuffers;
protected:
	HWAVEOUT *hWaveOut;

public:
	void OnReceive(int);
	CStreamACliSocket(CWnd *,HWAVEOUT *,int numBuff=32);
	~CStreamACliSocket();
	};


// Stream-control socket
class CControlSrvSocket;
class CControlSrvSocket2 : public CSocketEx {
public:
	CLineText *myLineText;
	CControlSrvSocket *myParent;
	DWORD numLogConn;
	DWORD cliTimeOut;
	CString connName;
	CTime startConn;			// il momento di inizio connessione...
	CTimeSpan timedConn;	// ...e la durata max. (se prevista)
private:
	BYTE packetType;

public:
	int checkUtenti();
	void reInit();
	void OnReceive(int);
	void OnClose(int);
	void doClose();

	CControlSrvSocket2(CControlSrvSocket *p=NULL);		// TOGLIERE default quando implementi la lista e togli l'array di client!!!
	~CControlSrvSocket2();
	friend class CControlSrvSocket;
protected:
	};

class CControlSrvSocket : public CSocketEx {
public:
	int maxConn;
	CList < CControlSrvSocket2 *, CControlSrvSocket2 * > cSockRoot;

public:
	void checkUtenti();
	void OnAccept(int);
	void OnClose(int);
	void doDelete(CControlSrvSocket2 *);

	CControlSrvSocket(CDocument *);
	~CControlSrvSocket();
protected:
	CDocument *myParent;
	};

class CControlCliSocket : public CSocketEx {
public:
public:
	CLineText *myLineText;
	BYTE packetType;
protected:
	CWnd *theWnd;

public:
	int SendUserPass(const char *,const char *,DWORD x=0,CTimeSpan t=0);
	void OnReceive(int);
	void OnClose(int);
	CControlCliSocket(CWnd *);
	~CControlCliSocket();
	};

// Authentication server/client
class CAuthSrvSocket;
class CAuthSrvSocket2 : public CSocketEx {
public:
	CLineText *myLineText;
	CAuthSrvSocket *myParent;
	BYTE packetType;
	CString nome;
	BYTE pasw[64];			// è criptata, quindi BYTE...
	CString prevIP;			// IP all'istante precedente (per controllo hacker)
	DWORD numLogConn;		// campo restituito dal database delle conn. server registrate
	DWORD cliTimeOut;
	BYTE proprieta;			// il bit0 e' 0 per guest, 1 per exhib

public:
	void updateDBUtenti();
	void OnReceive(int);
	void OnClose(int);

	CAuthSrvSocket2(CAuthSrvSocket *);
	~CAuthSrvSocket2();
	friend class CAuthSrvSocket;
	};

class CAuthSrvSocket : public CSocketEx {
public:
	int maxConn;
	CList < CAuthSrvSocket2 *, CAuthSrvSocket2 * > cSockRoot;

public:
	void updateDBUtenti();
	void OnAccept(int);
	void doDelete(CAuthSrvSocket2 *,int bForced=FALSE);

	CAuthSrvSocket();
	~CAuthSrvSocket();
	};

class CAuthCliSocket : public CSocketEx {
public:
public:
	CLineText *myLineText;
	int response,extraParm;
	DWORD IDutente;
	char tariffa[16],passwordSconto[16];
	double sconto;
	CTimeSpan timedConn;
	char FTPserver[40];
	char FTPlogin[20];
	char FTPpassword[20];
private:
	BYTE packetType;
protected:
	CWnd *theWnd;
	CExDocument *myParent; 

public:
	CExDocument *getParent() { return myParent; }
	int SendUserPass(char *,char *,int,DWORD,DWORD,DWORD,DWORD);
	void reInit();
	void OnReceive(int);
	void OnClose(int);
	CAuthCliSocket(/*CWnd * */ CExDocument *p=NULL);
	~CAuthCliSocket();
	};


// Directory server/client
class CDirectorySrvSocket;
class CDirectorySrvSocket2 : public CSocketEx {
public:
	CLineText *myLineText;
	CDirectorySrvSocket *myParent;
	CString connName;
	int cliTimeOut;			// diverso dall'altro... qua uso anche -1
	BYTE packetType;

public:
	void OnReceive(int);
	void OnClose(int);

	CDirectorySrvSocket2(CDirectorySrvSocket *);
	~CDirectorySrvSocket2();
	friend class CDirectorySrvSocket;
	};

class CDirectorySrvSocket : public CSocketEx {
public:
	int maxConn;
	CList < CDirectorySrvSocket2 *, CDirectorySrvSocket2 * > cSockRoot;

public:
	void doDelete(CDirectorySrvSocket2 *);
	void OnAccept(int);

	CDirectorySrvSocket();
	~CDirectorySrvSocket();
	};

class CDirectoryCliSocket : public CSocketEx {
public:
public:
	CLineText *myLineText;
	BYTE packetType;
protected:

public:
	void OnReceive(int);
	void OnClose(int);
	CDirectoryCliSocket();
	~CDirectoryCliSocket();
	};

// Chat server/client
class CChatSrvSocket;
class CChatSrvSocket2 : public CSocketEx /*CAsyncSocket*/ {
public:
	CLineText *myLineText;
	CChatSrvSocket *myParent;
	DWORD serNum;
	BOOL richOne2One;
	CString connName;
	COLORREF connColor;
	BYTE connType;
	DWORD cliTimeOut;
	BYTE packetType;

public:
	int Manda(char *,int );
	void OnReceive(int);
	void OnClose(int);

	CChatSrvSocket2(CChatSrvSocket *);
	~CChatSrvSocket2();
	friend class CChatSrvSocket;
	};

class CChatSrvSocket : public CSocketEx /*CAsyncSocket*/ {
public:
	int maxConn;
	CList < CChatSrvSocket2 *, CChatSrvSocket2 * > cSockRoot;

public:
	void doDelete(CChatSrvSocket2 *);
	int Manda(char *,int );
	void OnAccept(int);

	CChatSrvSocket();
	~CChatSrvSocket();
	};

class CChatCliSocket : public CSocketEx /* CAsyncSocket comporterebbe cambiamenti in OnConnect (v.anche NMChat) */ {
public:
public:
	CLineText *myLineText;
	BYTE packetType;
protected:
	CWnd *theWnd;

public:
	void OnReceive(int);
	void OnClose(int);
	CChatCliSocket(CWnd *);
	~CChatCliSocket();
	};



class CFTPclient : public CSocketEx {
	enum { bufSize=4096 };

public:
	CString m_retmsg;

public:
	BOOL GetList(CString ,BOOL pasv=FALSE);
	BOOL ChDir(CString);
	BOOL MkDir(CString);
	BOOL Delete(CString);
	BOOL SendBuff(CString,BYTE *,DWORD,BOOL pasv=FALSE);
	BOOL RecvBuff(CString, BYTE *,DWORD,BOOL pasv=FALSE);
	BOOL MoveFile(CString, CString,BOOL,BOOL pasv=FALSE);
	void LogOffServer();
	BOOL LogOnToServer(CString,int,CString,CString,LPCTSTR acct=NULL, LPCTSTR fwhost=NULL,LPCTSTR fwusername=NULL, LPCTSTR fwpassword=NULL,int fwport=0,int logontype=0);
	CFTPclient();
	~CFTPclient();
	BOOL FTPcommand(CString);
	BOOL ReadStr();
	BOOL WriteStr(CString);

private:
	CArchive* m_pCtrlRxarch;
	CArchive* m_pCtrlTxarch;
	CSocketFile* m_pCtrlsokfile;
	int m_fc;

private:
	int preMove(CString,int,int,CSocket *);
	int postMove(CSocket *);
	BOOL ReadStr2();
	BOOL OpenControlChannel(CString serverhost,int serverport);
	void CloseControlChannel();

protected:

	};

