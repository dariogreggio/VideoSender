#include "stdafx.h"
//#include "vidsendSockets.h"
#include "vidsendSerial.h"
#include "vidsend.h"
#include "vidsendlog.h"
#include "childfrm.h"
#include "vidsenddoc.h"
#include "vidsendview.h"
#include "vidsendTime.h"
#include <cgif.h>
#include <cjpeg2.h>
#include "digitalText.h"
#include "atlconv.h"
#include <iphlpapi.h>
#include "qarray.h"
#include <winsock.h>
#include "MP3Coder.h"

#ifndef _NEWMEET_MODE
#include "ping.h"
#endif


CSocketEx::~CSocketEx() {

//	closesocket(m_hSocket);		m_hSocket=INVALID_SOCKET;
	// questo trucco serve a poter chiudere un socket dal Task Principale,
	//nonostante il Socket sia stato creato in un Task secondario.
	// Se no dava un qualche errore di m...
	// NO! dioporco 2018
	}

int CSocketEx::getIPAddress(char *s,int q) {
	struct hostent *he;
	int i=0;

	if(he=gethostbyname(s)) {
		if(he->h_addr_list[q]) {
			wsprintf(s,"%u.%u.%u.%u",(BYTE)he->h_addr_list[q][0],	(BYTE)he->h_addr_list[q][1],
				(BYTE)he->h_addr_list[q][2], (BYTE)he->h_addr_list[q][3]);
			i=1;
			}
		}
	return i;
	}

int CSocketEx::getMyIPAddress(char *s,int q) {
	char myBuf[128];
	struct hostent *he;
	int i=0;

	*s=0;
	if(!gethostname(myBuf,127)) {
		if(he=gethostbyname(myBuf)) {
			if(he->h_addr_list[q]) {
				wsprintf(s,_T("%u.%u.%u.%u"),(BYTE)he->h_addr_list[q][0],	(BYTE)he->h_addr_list[q][1],
					(BYTE)he->h_addr_list[q][2], (BYTE)he->h_addr_list[q][3]);
				i=1;
				}
			}
		}
	return i;
	}

int CSocketEx::getMyIPAddress(SOCKADDR_IN *sock_in,int q) {
	char myBuf[128];
	struct hostent *he;
	int i=0;

	if(!gethostname(myBuf,127)) {
		if(he=gethostbyname(myBuf)) {
			if(he->h_addr_list[q]) {
				sock_in->sin_addr.S_un.S_un_b.s_b4=he->h_addr_list[q][3];
				sock_in->sin_addr.S_un.S_un_b.s_b3=he->h_addr_list[q][2];
				sock_in->sin_addr.S_un.S_un_b.s_b2=he->h_addr_list[q][1];
				sock_in->sin_addr.S_un.S_un_b.s_b1=he->h_addr_list[q][0];
				sock_in->sin_family=AF_INET;
				sock_in->sin_port=0;
				i=1;
				}
			}
		}
	return i;
	}

char *CSocketEx::getMyOutmostIPAddress(char *s,BOOL cached) {
	int q,i,j,nRead,totBytes;
	SOCKADDR_IN sock_in;
	int retries=3;
	CString srv,s2;
	BOOL needAuthorize=TRUE; //FALSE		/*2015*/;

	*s=0;

	if(cached && !theApp.IPaddress.IsEmpty()) {
		_tcscpy(s,(LPCTSTR)theApp.IPaddress);
		return s;
		}

	CWebCliSocket wc(_T("Joshua 2.6"));


//goto rifo;


rifo2:
// metodo che legge da whatsmyip.com...
//	srv=_T("www.whatismyip.org");			// anche http://checkip.dyndns.org/
	srv=_T("checkip.dyndns.com");			// anche http://checkip.dyndns.org/ o checkip.net, porta 80 o 8245
// anche, 2021: http://rl.ammyy.com/


	if(theApp.debugMode)
		theApp.FileSpool->print(CLogFile::flagInfo,"checkIP : provo connect:%u",timeGetTime());


	if(wc.Connect(srv,FALSE  /*,8245*/)) {
		char myBuf[1024];
		CString user,pasw;
		s2=wc.buildQuery(srv,_T("/"));		// compreso slash iniziale!


	if(theApp.debugMode)
		theApp.FileSpool->print(CLogFile::flagInfo,"checkIP : connect:%u",timeGetTime());

		s2=wc.m_HTTPHeader->GetBuffer();	// togliere, xche' addHeader va gestita...

//AfxMessageBox(s2);
//s2="GET /index.htm HTTP/1.1\x0d\x0a";

		i=wc.sendQuery((LPCTSTR)s2);

		Sleep(250);
		if(theApp.debugMode)
			theApp.FileSpool->print(CLogFile::flagInfo,"checkIP : sendquery+sleep:%u",timeGetTime());

		*myBuf=0;
//		wc.Receive(myBuf,300);
		if((totBytes=wc.readHeader())<0)
			goto skippa2;
		if(!totBytes)		// safety, ma boh :)
			totBytes=300;
		nRead=wc.readPage(myBuf,totBytes,70 /* */);		// qui è inutile aspettare così tanto... 
//		wc.getPage(myBuf,500);
		myBuf[nRead]=0;


		if(theApp.debugMode)
			theApp.FileSpool->print(CLogFile::flagInfo,"checkIP : readPage:%u; myBuf:%s, nRead=%u",timeGetTime(),myBuf,nRead);
		
		if(!*myBuf || strstr(myBuf,"401") || strstr(myBuf,"403")) {
			goto skippa2;		// a volte non legge... migliorare?
			}


		j=0;
		do {

			if(theApp.debugMode)
				theApp.FileSpool->print(CLogFile::flagError,_T("ricevuto da whatsmyIP: %s"),myBuf);

			s2=myBuf;
//			i=s2.Find(_T(CWebSrvSocket2_base::CRLF2));
	//		i=s2.Find(_T("IpAddress_Value-1"));
//			if(i>0) {
				i=s2.Find(_T("IP Address:"));		// dyndns
				if(i>0) {
//				if(isdigit(*(LPCTSTR)s2.Mid(i+4))) {
		//			s2=s2.Mid(i);
					s2=s2.Mid(i+12);
					_tcscpy(s,(LPCTSTR)s2.Left(20));
					i=s2.Find(_T("<"));
					_tcscpy(s,(LPCTSTR)s2.Left(i));
					j=1;
					break;
					}
				else {
					theApp.FileSpool->print(CLogFile::flagError,_T("IP rilevato MALE da whatsmyIP: %s"),myBuf);
					// non si capisce perché, restituisce  HTTP/1.1 500 ( The connection was reset by a peer.  )
					_tcscpy(s,(LPCTSTR)theApp.IPaddress);		// faccio così...
					j=1;
					break;
					}
//				}

			if(totBytes>500) {
				totBytes-=500;
				nRead=wc.readPage(myBuf,500,5);
				if(!nRead)
					break;
				myBuf[nRead]=0;
				}
			else
				break;

			} while(j==0);


skippa2:
		wc.Disconnect();


		if(theApp.debugMode)
			theApp.FileSpool->print(CLogFile::flagInfo2,_T("IP rilevato da router: %s"),s);


		}

	if(!*s) {
		if(--retries)
			goto rifo2;
		}


	if(theApp.debugMode)
		theApp.FileSpool->print(CLogFile::flagInfo2,_T("IP restituito: %s"),s);

	// opp. usare ipupdate e theApp.friendIPsList
	return s;




	{

rifo:
// metodo che legge dal router la pag. di Status...
 	srv=theApp.theRouter;
	if(wc.Connect(srv)) {
		char myBuf[1024];
		CString user,pasw;

		s2=wc.buildQuery(srv,_T("/ui/dboard")	/* router A1 2017*/ );		// compreso slash iniziale!
//		s2=wc.buildQuery(srv,_T("/cgi/b/is/_pppoe_/ov/?be=0&l0=2&l1=3&name=vINTERNET")	/* router A1 2015*/ );		// compreso slash iniziale!
//		s2=wc.buildQuery(srv,_T("/status/status_deviceinfo.htm")	/* router hamlet/atlantis*/ );		// compreso slash iniziale!
//		s2=wc.buildQuery(srv,_T("/cgi-bin/webcm?getpage=../html/status/status_deviceinfo.htm")	/* router wireless DLink*/ );		// compreso slash iniziale!
//		s2=wc.buildQuery(srv,_T("/index.htm")	/* router hamlet*/ );		// compreso slash iniziale!
//		s2=wc.buildQuery(srv,_T("/doc/home.htm")	/* router kraun/hasbani*/ );		// compreso slash iniziale!

		if(needAuthorize) {
//		user=_T("user"); pasw=_T("password");
		user=_T("admin"); pasw=_T("dario18");
		wc.authorize(user,pasw);
		//FINIRE!

		s2=wc.m_HTTPHeader->GetBuffer();	// togliere, xche' addHeader va gestita...

//AfxMessageBox(s2);
		}
//s2="GET /index.htm HTTP/1.1\x0d\x0a";

		i=wc.sendQuery((LPCTSTR)s2);

		Sleep(250);
		

		*myBuf=0;
//		wc.Receive(myBuf,300);
		nRead=wc.readPage(myBuf,1000,10 /* */);
//		wc.getPage(myBuf,500);
		myBuf[nRead]=0;

		if(!*myBuf || strstr(myBuf,"401")) {
			goto skippa;		// a volte non legge... migliorare?
			}

		j=0;
		int bFound1;
		bFound1=1;
		do {


			s2=myBuf;
			if(bFound1) {
				i=s2.Find(_T("IP Address:"));		// A1
//				i=s2.Find(_T("IP Address"));		// hamlet NON va Benissimo! se la stringa capita a cavallo dei chunk...
				if(i>0) {
					s2=s2.Mid(i+12);
					i=s2.Find("wan&if=2");
					if(i>0) {
						s2=s2.Mid(i);
						j=s2.Find('>');
						if(j>0) {
							s2=s2.Mid(j+1);
							j=s2.Find('<');
							_tcscpy(s,(LPCTSTR)s2.Left(j));
							}

	//				strcpy(s,"151.44.169.189");

						}
					}
				}

			nRead=wc.readPage(myBuf,1000,5);
			if(!nRead)
				break;
			myBuf[nRead]=0;

			} while(j==0);


skippa:
		wc.Disconnect();

		if(theApp.debugMode)
			theApp.FileSpool->print(CLogFile::flagInfo2,_T("IP rilevato da router: %s"),s);


		}

	if(!*s) {
		if(--retries) {
			wc.Disconnect();
			goto rifo;
			}
		}

	}

	// opp. usare ipupdate e theApp.friendIPsList
	return s;




	// metodo che legge dalle schede di rete interne...
	q=0;
	*s=0;
	do {
		if(!getMyIPAddress(&sock_in,q++))
			break;
		wsprintf(s,_T("%u.%u.%u.%u"),sock_in.sin_addr.S_un.S_un_b.s_b1, sock_in.sin_addr.S_un.S_un_b.s_b2,
			sock_in.sin_addr.S_un.S_un_b.s_b3,sock_in.sin_addr.S_un.S_un_b.s_b4);
		} while(sock_in.sin_addr.S_un.S_un_b.s_b1==10 || sock_in.sin_addr.S_un.S_un_b.s_b1==127 
			|| sock_in.sin_addr.S_un.S_un_b.s_b1==192 || sock_in.sin_addr.S_un.S_un_b.s_b1==169 /*patch Microsoft TV/Video ???*/);
	return s;
	}

// Fetches the MAC address and prints it
// da un esempio su CodeGuru... 
char *CSocketEx::getMyMACAddress(char *s,int q) {
  IP_ADAPTER_INFO AdapterInfo[16];       // Allocate information 
	CString macaddress;
                                         // for up to 16 NICs
  DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer

  DWORD dwStatus = GetAdaptersInfo(      // Call GetAdapterInfo
    AdapterInfo,                 // [out] buffer to receive data
    &dwBufLen);                  // [in] size of receive data buffer
  ASSERT(dwStatus == ERROR_SUCCESS);  // Verify return value is 
                                      // valid, no buffer overflow

  PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; // Contains pointer to
                                               // current adapter info
  do {
//    PrintMACaddress(pAdapterInfo->Address); // Print MAC address
    pAdapterInfo = pAdapterInfo->Next;    // Progress through 
                                          // linked list
		macaddress.Format( _T("%02X:%02X:%02X:%02X:%02X:%02X"),
			pAdapterInfo->Address[0],pAdapterInfo->Address[1],
			pAdapterInfo->Address[2],pAdapterInfo->Address[3],
			pAdapterInfo->Address[4],pAdapterInfo->Address[5] );
		if(!q)
			break;
		q--;
		} while(pAdapterInfo);                    // Terminate if last adapter


	_tcscpy(s,(LPCTSTR)macaddress);
	return s;
	}

WORD CSocketEx::getPort() {
	struct sockaddr_in sin;
	int addrlen = sizeof(sin);

	if(getsockname(m_hSocket, (struct sockaddr *)&sin, &addrlen) == 0 &&
		sin.sin_family == AF_INET &&
		addrlen == sizeof(sin)) {
		int local_port = ntohs(sin.sin_port);
		return local_port;
		}
	else
			; // handle error
	return 0;
	}


int CSocketEx::IsLocalAddress(const char *s) {
	int n;

	n=atoi(s);
	if(n==192) {	// migliorare...
		n=atoi(s+4);
		if(n==168)
			return 1;
		else
			return 0;
		}
	else if(n==127 || n==10)	// migliorare...
		return 1;
	else
		return 0;
	}

int CSocketEx::IsBroadcastAddress(const char *ip) {

  return !_tcsncmp(ip + _tcslen(ip) - 3, "255", 3);
	}

// ---------------------------------------------------------------------------------------
void CTimeSocket::OnAccept(int nErr) {
	int i;
	char myBuf[128];
	CSocket cSock;

	if(i=Accept(cSock)) {
		CVidsendApp::getNowGMT(myBuf);
		cSock.Send(myBuf,_tcslen(myBuf));
		cSock.Close();
		}
	}

BOOL CTimeSocket::Create(DWORD t) {
	int i,j;

	type=t;
	if(t == timeServerTCP) {
		i=CSocket::Create(IPPORT_DAYTIME);
		}
	if(t == timeServerUDP) {
		j=CSocket::Create(IPPORT_DAYTIME,SOCK_DGRAM);
		}
	return i;
	}

void CTimeSocket::OnReceive(int nErr) {			// solo UDP
	char myBuf[128];

	CVidsendApp::getNowGMT(myBuf);
	Send(myBuf,_tcslen(myBuf));
	}


//------------------------------------------------------------------------
CWebSrvSocket::CWebSrvSocket(UINT nPort,const char *sAddr,DWORD opt,CDatabase *db) :
	port(nPort), opzioni(opt), m_DB(db) {
	
	_tcscpy(ipaddr,sAddr ? sAddr : _T(""));
	maxConn=0;
	sentBytes=recvBytes=0;

	WWWRoot=_T("\\joshua\\");
	CGIPath=_T("/cgi-bin/");
	XSLRoot=_T("\\joshua\\panels\\");
	_tcscpy(logFormat,_T("%h %l %u %t \"%r\" %>s %b"));
	HTTPVer=MAKELONG(1,1);		// versione HTTP riportata nelle risposte alle pagine web
	WWWVer=MAKELONG(20,1);		// versione server WEB
	WAPVer=MAKELONG(1,1);		// versione protocollo HTTP per WAP

#ifdef USA_AUTENTICAZIONE 
	if(opzioni & useSessions) {	
		m_Set=new CJoshuaUtentiSet(m_DB);
		}
	else
		m_Set=NULL;
#endif

	}

CWebSrvSocket::~CWebSrvSocket() {

	clearAll();
#ifdef USA_AUTENTICAZIONE 
	if(m_Set)
		delete m_Set;
#endif
	}

BOOL CWebSrvSocket::Create() {

	return CSocket::Create(port);
	}

void CWebSrvSocket::resetCounters() {

	sentBytes=recvBytes=0;
	}

void CWebSrvSocket::OnAccept(int nErr) {
	int i,j;
	CWebSrvSocket2_vidsend *s;

	if(maxConn < maxClientConnections) {
		s=new CWebSrvSocket2_vidsend(this);
		if(s) {
			cSockRoot.AddTail(s);
			maxConn++;

			j=Accept(*s);
			return;
			}
		}
	{
	CWebSrvSocket2_base tempSock(this);
	char p1[512],header[512];
	DWORD l;
	Accept(tempSock);
	Accept(tempSock);
	tempSock.m_Body->WriteString(_T("<html><head><title>VideoSender - Server sovraccarico</title></head>\n"));
	// al posto di html, per xhmtml usare: <html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
	tempSock.m_Body->WriteString(_T("<body><h2>503 Il server non è in grado di accettare altre connessioni.<br><br><br></H3></body></html>"));
	tempSock.m_Body->WriteString(CWebSrvSocket2_base::copyString);
	tempSock.Msg.status=503;
	tempSock.prepareHTTPHeader(_T("Server sovraccarico"),mimeTypeHtml,0);
	tempSock.SendHeader();
	tempSock.SendBody();

	Sleep(2000);					// altrimenti il client non visualizza nulla!
	tempSock.Close();			// questo forza il client a ri-richiedere la connessione!
	
	}
	theApp.FileSpool->print(CLogFile::flagError,_T("impossibile Accept(), stroncato!"));
	}

void CWebSrvSocket::doDelete(CWebSrvSocket2_base *ss) {
	CWebSrvSocket2_base *s;
	POSITION po;

	po=cSockRoot.Find(ss);
	if(po) {
		s=cSockRoot.GetAt(po);
		delete s;
		cSockRoot.RemoveAt(po);
		}

	maxConn--;
	}

void CWebSrvSocket::clearAll() {		// perlopiu' usato come anti-virus Denial of Service

	while(!cSockRoot.IsEmpty())
		delete cSockRoot.RemoveHead();

//		if(s->sock->m_hSocket != INVALID_SOCKET)
//			s->sock->Close();
	maxConn=0;
	}

#ifdef USA_AUTENTICAZIONE 
void CWebSrvSocket::sincronizzaSessioni(CWebSrvSocket2_base *r) {
	CWebSrvSocket2_base *s;
	POSITION po;

	po=getClientRoot();
	if(po) {
		do {
			s=getNextClient(po);
			if(s!=r && s->m_Peer==r->m_Peer) {			// tutti quelli (loggati e no) con questo IP...
				s->proprieta=r->proprieta;
				s->sessionID=r->sessionID;
				s->sessionUser=r->sessionUser;
				s->mSeq=r->mSeq;
				}
			} while(po);
		}

	}
#endif


/* -------------------------------------------------------------------*/
// https://en.wikipedia.org/wiki/List_of_HTTP_header_fields
const TCHAR *CHTTPHeader::tagHttp=_T("HTTP/");
const TCHAR *CHTTPHeader::tagServer=_T("Server:");
const TCHAR *CHTTPHeader::tagDate=_T("Date:");
const TCHAR *CHTTPHeader::tagHost=_T("Host:");
const TCHAR *CHTTPHeader::tagReferer=_T("Referer:");
const TCHAR *CHTTPHeader::tagUserAgent=_T("User-Agent:");
const TCHAR *CHTTPHeader::tagContentType=_T("Content-Type:");
const TCHAR *CHTTPHeader::tagContentLength=_T("Content-Length:");
const TCHAR *CHTTPHeader::tagContentDisposition=_T("Content-Disposition:");
const TCHAR *CHTTPHeader::tagContentLocation=_T("Content-Location:");
const TCHAR *CHTTPHeader::tagConnection=_T("Connection:");
const TCHAR *CHTTPHeader::tagUpgrade=_T("Upgrade:");
const TCHAR *CHTTPHeader::tagAccept=_T("Accept:");
const TCHAR *CHTTPHeader::tagAcceptLanguage=_T("Accept-Language:");
const TCHAR *CHTTPHeader::tagAcceptCharset=_T("Accept-Charset:");
const TCHAR *CHTTPHeader::tagAcceptEncoding=_T("Accept-Encoding:");
const TCHAR *CHTTPHeader::tagTransferEncoding=_T("Transfer-Encoding:");		//gestire "chunked"
const TCHAR *CHTTPHeader::tagCacheControl=_T("Cache-Control:");
const TCHAR *CHTTPHeader::tagExpires=_T("Expires:");
const TCHAR *CHTTPHeader::tagKeepAlive=_T("Keep-Alive:");
const TCHAR *CHTTPHeader::tagClose=_T("close");
const TCHAR *CHTTPHeader::tagEtag=_T("Etag:");
const TCHAR *CHTTPHeader::tagLocation=_T("Location:");
const TCHAR *CHTTPHeader::tagModified=_T("Last-Modified:");
const TCHAR *CHTTPHeader::tagAuthenticate=_T("WWW-Authenticate: Basic Realm=");		// rifinire...?
const TCHAR *CHTTPHeader::tagAuthorization=_T("Authorization:");
const TCHAR *CHTTPHeader::tagAllow=_T("Allow:");
const TCHAR *CHTTPHeader::tagAcceptRanges=_T("Accept-Ranges:");
const TCHAR *CHTTPHeader::tagSetCookie=_T("Set-Cookie:");
const TCHAR *CHTTPHeader::tagFileUpload=_T("FileUpload:");
const TCHAR *CHTTPHeader::tagPragma=_T("Pragma:");
const TCHAR *CHTTPHeader::tagNoCache=_T("No-Cache:");
const TCHAR *CHTTPHeader::tagIfModifiedSince=_T("If-Modified-Since:");
const TCHAR *CHTTPHeader::tagProxyConnection=_T("Proxy-Connection:");
const TCHAR *CHTTPHeader::tagDeprecatedWarning=_T("Deprecated-Warning:");

CHTTPHeader::CHTTPHeader() {

	}

//CHTTPHeader::~CHTTPHeader() {
//	}

const char *CHTTPHeader::getTagString(enum HEADER_TAG t) {
	const char *p=NULL;
	
	switch(t) {
		case TAG_HTTP:
			break;
		case TAG_SERVER:
			p=tagServer;
			break;
		case TAG_HOST:
			p=tagHost;
			break;
		case TAG_USERAGENT:
			p=tagUserAgent;
			break;
		case TAG_DATE:
			p=tagDate;
			break;
		case TAG_CONTENT_TYPE:
			p=tagContentType;
			break;
		case TAG_CONTENT_LENGTH:
			p=tagContentLength;
			break;
		case TAG_CONTENT_DISPOSITION:
			p=tagContentDisposition;
			break;
		case TAG_CONTENT_LOCATION:
			p=tagContentLocation;
			break;
		case TAG_CONNECTION:
			p=tagConnection;
			break;
		case TAG_UPGRADE:
			p=tagUpgrade;
			break;
		case TAG_CACHE_CONTROL:
			p=tagCacheControl;
			break;
		case TAG_EXPIRES:
			p=tagExpires;
			break;
		case TAG_KEEPALIVE:
			p=tagKeepAlive;
			break;
		case TAG_ETAG:
			p=tagEtag;
			break;
		case TAG_LOCATION:
			p=tagLocation;
			break;
		case TAG_MODIFIED:
			p=tagModified;
			break;
		case TAG_IFMODIFIED:
			p=_T("If-Modified: ");
			break;
		case TAG_AUTHENTICATE:
			p=tagAuthenticate;
			break;
		case TAG_AUTHORIZATION:
			p=tagAuthorization;
			break;
		case TAG_ALLOW:
			p=_T("Allow: ");
			break;
		case TAG_ACCEPT:
			p=tagAccept;
			break;
		case TAG_ACCEPT_LANGUAGE:
			p=tagAcceptLanguage;
			break;
		case TAG_ACCEPT_CHARSET:
			p=tagAcceptCharset;
			break;
		case TAG_ACCEPT_ENCODING:
			p=tagAcceptEncoding;
			break;
		case TAG_ACCEPT_RANGES:
			p=tagAcceptRanges;
			break;
		case TAG_SET_COOKIE:
			p=tagSetCookie;
			break;
		case TAG_DEPRECATED_WARNING:
			p=tagDeprecatedWarning;
			break;
		case TAG_NULL:			// per passare stringhe extra...
			break;
		default:
			ASSERT(FALSE);
			break;
		}

	return p;
	}

#if 0 //2022
const char *CHTTPHeader::AddToken(enum HEADER_TAG t,const char *s1,const char *s2,CTime t1,DWORD n1) {
	const char *p;
	
	p=getTagString(t);

	if(p) {
		m_Buffer+=p;
		m_Buffer+=_T(" ");
		}
	if(s1)
		m_Buffer+=s1;
	if(s2)
		m_Buffer+=s2;
	if(n1) {
		CString S;
		S.Format(_T("%u"),n1);
		m_Buffer+=S;
		}
	if(t1 != 0) {
		m_Buffer+=asctime(t1.GetLocalTm());
		}

	m_Buffer+=CWebSrvSocket2_base::CRLF;


//	m_Buffer.GetBufferSetLength(1024);
//	int i=m_Buffer.GetLength();
//	AfxMessageBox(m_Buffer);

	return m_Buffer;
	}
#endif

const char *CHTTPHeader::AddToken(enum HEADER_TAG t,const char *s1,const char *s2) {
	const char *p;
	
	p=getTagString(t);

	if(p) {
		m_Buffer+=p;
		m_Buffer+=_T(" ");
		}
	m_Buffer+=s1;
	if(s2)
		m_Buffer+=s2;

	m_Buffer+=CWebSrvSocket2_base::CRLF;

	return m_Buffer;
	}

const char *CHTTPHeader::AddToken(const char *s) {

	m_Buffer+=s;
	m_Buffer+=CWebSrvSocket2_base::CRLF;
	return m_Buffer;
	}

const char *CHTTPHeader::AddToken(enum HEADER_TAG t,DWORD n1) {
	const char *p;
	
	p=getTagString(t);

	if(p) {
		m_Buffer+=p;
		m_Buffer+=_T(" ");
		}
	CString S;
	S.Format(_T("%u"),n1);
	m_Buffer+=S;

	m_Buffer+=CWebSrvSocket2_base::CRLF;

	return m_Buffer;
	}

const char *CHTTPHeader::AddToken(enum HEADER_TAG t,CTime t1) {
	const char *p;
	
	p=getTagString(t);

	if(p) {
		m_Buffer+=p;
		m_Buffer+=_T(" ");
		}
	m_Buffer+=asctime(t1.GetLocalTm());

	m_Buffer+=CWebSrvSocket2_base::CRLF;

	return m_Buffer;
	}

void CHTTPHeader::Finalize() {
	
	m_Buffer+=CWebSrvSocket2_base::CRLF; 
//	m_Buffer+=CWebSrvSocket2_base::CRLF;
	}


const TCHAR *CHTTPBody::CRLF=_T("\r\n");
CHTTPBody::CHTTPBody() {

	isInFile=0;
	forcedLength=0;
	fileName.Empty();
	}

int CHTTPBody::dumpToFile(LPCTSTR nomefile) {
	int lenSoFar=GetLength();
	BYTE myBuf[2048];
	CStdioFile mF;
	CFileException ex;

	try {
		if(!mF.Open(nomefile,CFile::modeCreate /*| CFile::modeNoTruncate qui scrivo da capo tutto */ | CFile::modeReadWrite | CFile::typeText | CFile::shareDenyWrite,&ex)) {
			wsprintf((char *)myBuf,"Impossibile aprire/creare file (file exception %X) in dump2File: ",ex.m_strFileName);
#ifdef _DEBUG
			AfxMessageBox((char *)myBuf);
#else
			*theApp.FileSpool << (char *)myBuf;
#endif
			return 0;
			}

		Seek(0,CFile::begin);
		while(lenSoFar > 0) {
			Read(myBuf,1024);
			mF.Write(myBuf,min(lenSoFar,1024));
			lenSoFar -= 1024;
			}

		mF.Close();
		return 1;

		}
	catch(CFileException *e) {
		wsprintf((char *)myBuf,"file exception %X in dump2File: ",e->m_strFileName);
#ifdef _DEBUG
		AfxMessageBox((char *)myBuf);
#else
		*theApp.FileSpool << (char *)myBuf;
#endif
		return 0;
		}

	}


const TCHAR *CWebSrvSocket2_base::copyString=_T("\n<center><font size=2><i>(C) <a href='http://cyberdyne.biz.ly' TARGET='_blank'>Cyberdyne</a></font></i>.</center>\n</body></html>\n\n");
const TCHAR *CWebSrvSocket2_base::Html4String=_T("<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN' 'http://www.w3.org/TR/html4/loose.dtd'>\n");
const TCHAR *CWebSrvSocket2_base::okString="<HTML><title>OK</title>\n<BODY>200 OK<BR>\n";
const TCHAR *CWebSrvSocket2_base::HtmlContent=_T("<head><meta http-equiv='Content-Type' content='text/html; charset=iso-8859-1'>");
const TCHAR *CWebSrvSocket2_base::WAP1String=_T("<?xml version='1.0'?>\n<!DOCTYPE wml PUBLIC '-//WAPFORUM//DTD WML 1.2//EN' 'http://www.wapforum.org/DTD/wml_1.2.dtd'>\n");
const TCHAR *CWebSrvSocket2_base::WAPcopyString="\n</card>\n<card id='copyright' title='copyright'><p><i>Generato da Skynet, da <a href='http://tagtag.com/adpm'>ADPM Synthesis</a></i>.</p></card></wml>\n\n";
const TCHAR *CWebSrvSocket2_base::WAPBackString="\n<template><do type='prev' label='Indietro'><prev/></do></template>\n";
const TCHAR *CWebSrvSocket2_base::WAPHeaderString="<?xml version='1.0'?>\n<!DOCTYPE wml PUBLIC '-//WAPFORUM//DTD WML 1.1//EN' 'http://www.wapforum.org/DTD/wml_1.1.xml'>\n";
const TCHAR *CWebSrvSocket2_base::passwordString="pass";
const TCHAR *CWebSrvSocket2_base::CRLF="\r\n";
const TCHAR *CWebSrvSocket2_base::CRLF2="\r\n\r\n";


CWebSrvSocket2_base::CWebSrvSocket2_base(CWebSrvSocket *p) :
	m_Parent(p) {

	m_LineText=new CLineText;
	*bodyString=0;
	proprieta=0;
	sessionID=0;
	sessionLastAct=0;
	srand(clock());
	subPageCnt=0;

	m_HTTPHeader=new CHTTPHeader;
	m_Body=new CHTTPBody;

	ZeroMemory(&Msg,sizeof(Msg));
	}

CWebSrvSocket2_base::~CWebSrvSocket2_base() {

	delete m_Body;	m_Body=NULL;
	delete m_HTTPHeader;	m_HTTPHeader=NULL;
	delete m_LineText; m_LineText=NULL;
	}

void CWebSrvSocket2_base::OnClose(int nErr) {

	Close();
	m_Parent->doDelete(this);
//	delete this;
	}

void CWebSrvSocket2_base::OnReceive(int nErr) {
	BYTE myBuf[2048];
	char string[2048],*s,*s1,*parms;
	int i,n;


/*	da W3MFC di PJNaughter: la lettura viene fatta con
BOOL CHttpSocket::ReadResponse(LPSTR pszBuffer, int nInitialBufSize, LPSTR pszTerminator, LPSTR* ppszOverFlowBuffer, DWORD dwTimeout, int nGrowBy)

	che loopa fino a un dato timeout, e aspetta di vedere il terminatore
	*/

/*	...invece, da sorgente terminale USB di Ktronic...

  // Note that since we've been called from the OnReceive notification
  // handler, then if bytesToRead is zero, it must mean the client
  // has closed the connection.
  DWORD  bytesToRead;
  IOCtl(FIONREAD, &bytesToRead);    // Anything to get?
  if(bytesToRead == 0)
    // Delete me.
    pServer->KillSocket(this);
  else
  {
    INT req;

    if(Receive(&req, sizeof(req), 0) == SOCKET_ERROR)
    {
      INT err = GetLastError();
      if(err == WSAECONNRESET)        // Remote has terminated (gracefully).
      {
        STRACE("CServerClient::ProcessPendingRead - WSAECONNERESET.\n");
        // Delete me.
        pServer->KillSocket(this);
      }
    }
    // Send request off for processing.
    ProcessReq(req);
  }
*/


	if((i=Receive(myBuf,1000)) >= 0) {
		m_Parent->recvBytes+=i;

		ZeroMemory(&Msg,sizeof(Msg));
		m_LineText->handleReadData(myBuf,i);
		myBuf[1000]=0;


	/*	CFile pppf;
		pppf.Open("\\post.txt",CFile::modeCreate | CFile::modeWrite);
		pppf.Write(myBuf,i);
		pppf.Close();*/


//		theApp.FileSpool->print(CLogFile::flagInfo,myBuf);
		if(m_LineText->indexOf(CRLF2)) {
			m_LineText->readText((char *)myBuf);
			s=(char *)myBuf;
//GET Retrieve data specified in the URL
//HEAD Return HTTP server response header information only
//POST Send information to the HTTP server for further action
//PUT Send information to the HTTP server for storage
//DELETE Delete the resource specified in the URL
//LINK  Establish one or more link relationships between specified URLs
 

//				theApp.FileSpool->print(CLogFile::flagInfo,"  COMANDO HTTP: %s",s);



			if(!_tcsnicmp((char *)myBuf,_T("get"),3)) {
				s+=4;			// inizio nome host
				Msg.cmd=200;
				}
			else if(!_tcsnicmp((char *)myBuf,_T("head"),4)) {
				s+=5;			// inizio nome host
				Msg.cmd=202;
				}
			else if(!_tcsnicmp((char *)myBuf,_T("post"),4)) {
				s+=5;			// inizio nome host
				Msg.cmd=201;
				}
			else if(!_tcsnicmp((char *)myBuf,_T("put"),3)) {
				s+=4;			// inizio nome host
				Msg.cmd=203;
				//https://www.example-code.com/cpp/http_put_json.asp
				}
			else if(!_tcsnicmp((char *)myBuf,_T("delete"),6)) {
				s+=7;			// inizio nome host
				Msg.cmd=204;
				}
			else if(!_tcsnicmp((char *)myBuf,_T("options"),7)) {
				s+=8;			// inizio nome host
				Msg.cmd=205;
				}
			else if(!_tcsnicmp((char *)myBuf,_T("propfind"),8)) {
				s+=9;			// inizio nome host
				Msg.cmd=210;
				}
			else if(!_tcsnicmp((char *)myBuf,_T("webdav"),6)) {
				s+=7;			// inizio nome host
				Msg.cmd=211;
				}
			else {			// [WEBDAV (per esempio) finisce qui]
				Msg.cmd=400;
				}

			while(*s && *s!=' ' && *s!='/')
				s++;
			s1=s;
			while(*s1!=' ' && *s1)
				s1++;
			*s1=0;

			s1++;
			if(tolower(*s1) == 'h')
				Msg.protocolVersion=MAKELONG(s1[7]-'0',s1[5]-'0');		//
			else {
				char *s2,s1u[128];
				_tcsncpy(s1u,s1,127);
				s1u[127]=0;
				_tcsupr(s1u);
				if(s2=(char *)stristr(s1u,_T("HTTP")))			// uso case-insensitive
					Msg.protocolVersion=MAKELONG(s2[7]-'0',s2[5]-'0');		// 
				else
					Msg.protocolVersion=MAKELONG(0,0);			// non trovato...
				// bisognerebbe forse mettere 0.9 e forzare l'uso del solo "GET"...
				}

			_tcsncpy(string,s,2047);
			string[2047]=0;
			*m_LineText->commLogin=0;
			*m_LineText->commPswd=0;
			parseQuery(m_LineText,&Msg);
			if(*m_LineText->commLogin) {
				//AfxMessageBox(m_LineText->commLogin);
				// con GUEST non passa... perch le sue caratteristiche sono "0"... che facciamo???
				if(!(proprieta & (loggedAsUser | loggedAsSupervisor))) {
//					doLogin(m_LineText->commLogin,m_LineText->commPswd,NULL);
					}	
				}

			/*if(Msg.cmd == 201) {			// gestisco i parametri del "post" NON + 2017, v. di l
				n=1;
				m_LineText->readTextRaw((BYTE *)myBuf,&n);
				n=Msg.contentLength;
				n=min(n,10000);			// protezione :)
				m_LineText->readTextRaw((BYTE *)myBuf,&n);

				_tcscat(string,_T("?"));
				_tcsncat(string,(char *)myBuf,n);
				}*/

// https://www.askapache.com/online-tools/request-method-scanner/

			if((sessionLastAct+CWebSrvSocket::sessionRefresh) < timeGetTime()) {
//				refreshSession();			// finch c' attivit, azzero time-out
				// MA NON FARLO TROPPO FREQUENTE!! o LEGARLO a Msg.referer?
				}
			sessionLastAct=timeGetTime();
			if(m_Parent->opzioni & CWebSrvSocket::createLog) {		// in questo metto tutto...
				// usare logFormat... "%h %l %u %t "%r" %>s %b" Apache
//				theApp.FileSpoolWebServer->print(CLogFile::flagInfo,"  %s legge %s (v.%u.%u) (con %s) (referer %s)",
//					(LPCTSTR)m_Peer,myBuf,HIWORD(Msg.protocolVersion),LOWORD(Msg.protocolVersion),
//					Msg.userAgent,Msg.referer);

				if(m_Parent->opzioni & CWebSrvSocket::createLogAll || !*Msg.referer) {		// ..e qui scrivo solo la pag. principale
					switch(Msg.cmd) {
						case 200:
						case 201:	// GET o POST

						theApp.FileSpool->print(CLogFile::flagInfo,_T("  %s legge %s (v.%u.%u) (con %s)"),
							(LPCTSTR)m_Peer,string,HIWORD(Msg.protocolVersion),LOWORD(Msg.protocolVersion),
							Msg.userAgent);
//						theApp.sendToAccessLog(CJoshuaLogSet::accessTypeInternet,(LPCTSTR)m_Peer,"apre pagina",
//							proprieta & loggedOn ? (LPCTSTR)m_Parent->m_Set->m_codice : NULL);

						if(theApp.m_pMainWnd) {
							char buf[2048];
							_tcscpy(buf,(LPSTR)(LPCTSTR)m_Peer);
							_tcscat(buf,": ");
							_tcsncat(buf,string,2000);
							char *p2=_tcsdup(buf);
//							theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE2,0,(DWORD)p2);
							}
						break;
						case 202:	//differenziare?
						theApp.FileSpool->print(CLogFile::flagInfo,_T("  %s legge2 %s (v.%u.%u) (con %s)"),
							(LPCTSTR)m_Peer,string,HIWORD(Msg.protocolVersion),LOWORD(Msg.protocolVersion),
							Msg.userAgent);
//						theApp.sendToAccessLog(CJoshuaLogSet::accessTypeInternet,(LPCTSTR)m_Peer,"apre pagina",
//							proprieta & loggedOn ? (LPCTSTR)m_Parent->m_Set->m_codice : NULL);
						break;

						case 203:	// PUT
						theApp.FileSpool->print(CLogFile::flagInfo,_T("  %s scrive %s (v.%u.%u) (con %s)"),
							(LPCTSTR)m_Peer,string,HIWORD(Msg.protocolVersion),LOWORD(Msg.protocolVersion),
							Msg.userAgent);
//						theApp.sendToAccessLog(CJoshuaLogSet::accessTypeInternet,(LPCTSTR)m_Peer,"scrive file",
//							proprieta & loggedOn ? (LPCTSTR)m_Parent->m_Set->m_codice : NULL);
						break;

						case 204:	// DELETE
						theApp.FileSpool->print(CLogFile::flagInfo,_T("  %s canc %s (v.%u.%u) (con %s)"),
							(LPCTSTR)m_Peer,string,HIWORD(Msg.protocolVersion),LOWORD(Msg.protocolVersion),
							Msg.userAgent);
//						theApp.sendToAccessLog(CJoshuaLogSet::accessTypeInternet,(LPCTSTR)m_Peer,"cancella file",
//							proprieta & loggedOn ? (LPCTSTR)m_Parent->m_Set->m_codice : NULL);
						break;

						case 205:	// OPTIONS
						theApp.FileSpool->print(CLogFile::flagInfo,_T("  %s usa %s (v.%u.%u) (con %s)"),
							(LPCTSTR)m_Peer,string,HIWORD(Msg.protocolVersion),LOWORD(Msg.protocolVersion),
							Msg.userAgent);
//						theApp.sendToAccessLog(CJoshuaLogSet::accessTypeInternet,(LPCTSTR)m_Peer,"options",
//							proprieta & loggedOn ? (LPCTSTR)m_Parent->m_Set->m_codice : NULL);
						break;

						case 210:	// PROPFIND
/* v. https://docs.microsoft.com/en-us/previous-versions/office/developer/exchange-server-2003/aa142960(v%3Dexchg.65)
	HTTP/1.1 207 Multi-Status
Content-Type: text/xml
Content-Length: 310

<?xml version="1.0"?>
<a:multistatus
  xmlns:b="urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/"
  xmlns:a="DAV:">
 <a:response>
   <a:href>http://server/public/test2/item1.txt</a:href>
   <a:propstat>
    <a:status>HTTP/1.1 200 OK</a:status>
       <a:prop>
        <a:getcontenttype>text/plain</a:getcontenttype>
        <a:getcontentlength b:dt="int">33</a:getcontentlength>
       </a:prop>
   </a:propstat>
 </a:response>
</a:multistatus>
*/
						case 211:	// WEBDAV
						theApp.FileSpool->print(CLogFile::flagInfo,_T("  %s spia %s (v.%u.%u) (con %s)"),
							(LPCTSTR)m_Peer,string,HIWORD(Msg.protocolVersion),LOWORD(Msg.protocolVersion),
							Msg.userAgent);
//						theApp.sendToAccessLog(CJoshuaLogSet::accessTypeInternet,(LPCTSTR)m_Peer,"propfind",
//							proprieta & loggedOn ? (LPCTSTR)m_Parent->m_Set->m_codice : NULL);
						break;
						}
					}
				}
			parms=_tcschr(string,'?');
			if(parms) {
				*parms++=0;			// zero-termina la prima parte, e parms punta ai parametri (o al body del form)
				}


#ifndef _DEBUG
		try {

#endif


//			char *ptr=NULL;
//			i=*(char *)ptr;
			// NON la vede... fanculo Bill 11/2005

			compattaHex(string);
			Msg.status=400;		// generico errore...
			if(!theApp.bRemoteAccessEnabled) {
				m_Body->Reset();
				m_Body->WriteString(Html4String);
				m_Body->WriteString("<title>Non disponibile</title>\n<body>410 Non disponibile: disabilitato<br>\n");
				enum mimeTypes bBinary=mimeTypeHtml;
				Msg.status=410;
				char *statMsg="Non disponibile";
				prepareHTTPHeader(statMsg,bBinary,0);
				}
			else {
				if(Msg.WML)
					i=buildWMLPage(Msg.cmd,string,parms);
				else
					i=buildHTMLPage(Msg.cmd,string,parms);
				}


#ifndef _DEBUG
			}
		catch(...) {
	// era __except(1)
#endif

#ifndef _DEBUG

//		e.GetErrorMessage((char *)myBuf, 255);

//		wsprintf((char *)myBuf,"exception %X in BuildPage: ",_exception_code());
#ifdef _DEBUG
		AfxMessageBox("exception di merda" /*(char *)myBuf*/);
#else
		*theApp.FileSpool << (char *)"exception di merda";
		*theApp.FileSpool << (char *)myBuf;
#endif
		Msg.keepAliveR=0;		// forza chiusura socket
		}
		
#endif

			if(m_Parent->opzioni & CWebSrvSocket::createLog) {		// in questo metto tutto...
				const char *cc;
				static char oldPeer[20];
				char iploc[200];

				switch(Msg.cmd) {
					case 200:
						cc=_T("GET");
						break;
					case 201:
						cc=_T("POST");
						break;
					case 202:
						cc=_T("HEAD");
						break;
					case 203:
						cc=_T("PUT");
						break;
					case 204:
						cc=_T("DELETE");
						break;
					case 205:
						cc=_T("OPTIONS");
						break;
					case 210:
						cc=_T("PROPFIND");
						break;
					case 211:
						cc=_T("WEBDAV");
						break;
					default:
						cc=_T("altro");
						break;
					}

				*iploc=0;
				if(_tcscmp((LPCTSTR)m_Peer,oldPeer)) {
					if(!IsLocalAddress((LPCTSTR)m_Peer)) {
//						if(theApp.bIPlookup)
//							theApp.subGetIPLocation(m_Peer,iploc); 
						}
//					if(*iploc)
//						theApp.FileSpool->print(CLogFile::flagInfo,(char *)iploc);
					_tcscpy(oldPeer,(LPCTSTR)m_Peer);
					}
//				theApp.FileSpoolWebServer->print(0,_T("%s - %s [%s] \"%s %s HTTP/%u.%u\" %u %u \"%s\" \"%s\"; \"%s\""),
//					(LPCTSTR)m_Peer,*m_LineText->commLogin ? m_LineText->commLogin : "-",(LPCTSTR)CLogFile::getNowApache(),
//					cc,string,
//					HIWORD(Msg.protocolVersion),LOWORD(Msg.protocolVersion),Msg.status,m_Body->isInFile ? m_Body->forcedLength : m_Body->GetLength(),
//					*Msg.referer ? Msg.referer : "-",*Msg.userAgent ? Msg.userAgent : "-", iploc);
				// OCCHIO! Quando referer e user-agent son vuoti, il "-" risultante NON deve essere tra apici...

//				theApp.FileSpoolWebServer->print(CLogFile::flagInfo,"  ...risponde %u",
//					Msg.status);
				}
//			if(m_Body->GetLength() > 0) {
				if(Msg.protocolVersion > MAKELONG(1, 1)) {//2.0 non accettato!
					return;		// finire...? (2016)
					}

			  if(Msg.protocolVersion > MAKELONG(9, 0)) //No header sent for HTTP v0.9
					SendHeader();
				if(Msg.cmd != 202 && Msg.status != 304)	{			// se status = 304 (NOT MODIFIED), niente body...
																											// se HEAD (202), non manda body
/*					DWORD lenSoFar=len,startPos=0;

					while(lenSoFar > 0) {
						theApp.TCPIP_WD=0;			// se no salta!
						if((n=Send(p1+startPos,min(lenSoFar,65536))) < 0) {			// errore (tipo: troppo lungo)
							Close();									// quindi forzo chiusura!
							break;
							}
						lenSoFar -= n;
						startPos += n;
						}
						*/
					SendBody();
					}
//				GlobalFree(p1);
//				}

			m_LineText->clearAll();
			if(!Msg.keepAliveR) {
				Close();
				m_Parent->doDelete(this);
//				delete this;
				}
			}
		else if(m_LineText->ssize() > 1000) {			// probabile virus...
				theApp.FileSpool->print(CLogFile::flagInfo,"  PROBABILE VIRUS da %s...",(LPCTSTR)m_Peer);
			m_LineText->readText((char *)myBuf);
			{
				char myBufHex[4000];
				CLogFile::getAsHex((const byte *)myBuf,myBufHex+18,1024);
				myBufHex[18+32]=0;			// rompe solo le pa##e...
				_tcsncpy(myBufHex,(char *)myBuf,16);
				myBufHex[16]=';'; myBufHex[17]=' ';
				theApp.FileSpool->print(CLogFile::flagInfo,(char *)myBufHex);

				// se no, non esce qua...
				myBuf[9]=0;
//				theApp.FileSpoolWebServer->print(0,_T("%s - %s [%s] \"%s\" %u - - -"),
//					(LPCTSTR)m_Peer,"-",(LPCTSTR)myBufHex,/*myBuf*/ CLogFile::getNowApache(),
//					400);
			}
			m_LineText->clearAll();
			}
		}
	}

int CWebSrvSocket2_base::parseQuery(CLineText *l,MSG_HEADER *h) {
	char myBuf[512],myBufL[512];		// serve abb. grosso, poiche' certe volte i tag sono lunghi!
	char *p,*p2;

	do {
		l->readText(myBuf,511);
		if(*myBuf) {
//			_tcscpy(myBufL,myBuf);
//			strlwr(myBufL);
			if(!_tcsnicmp(myBuf,CHTTPHeader::tagKeepAlive,10)) {
				h->keepAliveS=TRUE;
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagConnection,11)) {		// boh? non capisco...  giusto quello prima...!
				if(!_tcsnicmp(myBuf+12,CHTTPHeader::tagKeepAlive,10))
					h->keepAliveR=TRUE;
				else if(!_tcsnicmp(myBuf+12,CHTTPHeader::tagClose,5))
					/* Close() */;	//boh...
				else if(!_tcsnicmp(myBuf+12,_T("upgrade"),7))
					/*  */;	//boh...
				}
			// c'e' anche "connection: close (in genere in risposta dal server)... GESTIRE!!
			// e c'e' anche "connection: upgrade 2020 da virus russo... GESTIRE!!
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagHost,5)) {						// questo e' il solo IP o nome del client
				_tcsncpy(h->host,myBuf+6,127);
				h->host[127]=0;
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagReferer,8)) {			// questo e' il nome completo della pagina che sta richiedendo qualcosa
				_tcsncpy(h->referer,myBuf+9,127);
				h->referer[127]=0;
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagProxyConnection,17)) {
				if(!_tcsnicmp(myBuf+18,CHTTPHeader::tagKeepAlive,10))
					h->keepAliveS=TRUE;
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagAccept,7)) {
				_tcsncpy(h->accept,myBuf+8,127);
				h->accept[127]=0;
				if(stristr(h->accept,_T("wap"))) {
					h->WML=TRUE;		// un po' porcata, da migliorare!
					}
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagAcceptLanguage,16)) {
				_tcsncpy(h->acceptLanguage,myBuf+17,127);
				h->acceptLanguage[127]=0;
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagAcceptEncoding,16)) {
				_tcsncpy(h->acceptEncoding,myBuf+17,127);
				h->acceptEncoding[127]=0;
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagAcceptCharset,15)) {
//				_tcsncpy(h->acceptEncoding,myBuf+17,127);
//				h->acceptEncoding[127]=0;
				}
//					if(_tcsstr(myBuf,"content:"))
//						;
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagTransferEncoding,18)) {
//fare! gestire "chunked" con HTTP 1.1
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagContentLength,15))
				h->contentLength=atoi(myBuf+15);
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagPragma,7)) {
				if(!_tcsnicmp(myBuf+8,CHTTPHeader::tagNoCache,10))
					h->pragmaNoCache=1;
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagContentDisposition,21)) {
				// usare per forzare un download di file, p.es.:Content-Disposition: attachment; filename=fname.ext 
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagContentType,13)) {
				_tcsncpy(h->contentType,myBuf+14,127);
				h->contentType[127]=0;
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagCacheControl,14)) {
				h->pragmaNoCache=atoi(myBuf+15+8);		// segue "max-age=?"
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagIfModifiedSince,18))
				h->ifModifiedSince=CTimeEx::parseGMTTime(myBuf+18);
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagUserAgent,11)) {
				_tcsncpy(h->userAgent,myBuf+12,63);
				h->userAgent[63]=0;
				p=_tcschr(h->userAgent,'/');
				_tcscpy(myBufL,h->userAgent);
				_tcslwr(myBufL);
				if(p) {
					h->userAgentVersion=MAKELONG(p[3]-'0',p[1]-'0');
					if(p2=(char *)stristr(myBufL,"msie")) {		// caso particolare...
						p2=_tcschr(p2,' ');
						h->userAgentType=BROWSER_EXPLORER;
						if(p2)
							h->userAgentVersion=MAKELONG(p2[3]-'0',p2[1]-'0');
						}
					else if(p2=(char *)stristr(myBufL,"mspie")) {		// caso particolare 2 ...
						p2=_tcschr(p2,' ');
						h->userAgentType=BROWSER_EXPLORER;
						if(p2)
							h->userAgentVersion=MAKELONG(p2[3]-'0',p2[1]-'0');
						}
					else {
						h->userAgentType=BROWSER_NETSCAPE;		// e UNKNOWN??
						}
					}
				}
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagAuthorization,14)) {		// questo arriva in risposta ad un box "utente/password" (codice 401/www-authenticate)
        //Handle the Authorization header
        CString sUsername;
        CString sPassword;

				if(ParseAuthorization(myBuf+14, sUsername, sPassword)) {
//          m_Request.m_AuthorizationType = HTTP_AUTHORIZATION_PLAINTEXT;
					_tcsncpy(l->commLogin,(LPCTSTR)sUsername,63);
					l->commLogin[63]=0;
					_tcsncpy(l->commPswd,(LPCTSTR)sPassword,31);
					l->commPswd[31]=0;
						// v.sopra...
					}
		    }
			else if(!_tcsnicmp(myBuf,CHTTPHeader::tagFileUpload,11)) {		// https://stackoverflow.com/questions/3781073/upload-a-file-to-a-web-server-using-c
				_tcsncpy(h->nomefile,myBuf+12,63);		// ri-uso nomefile!

		    }
				
			}
		} while(*myBuf);

	return 1;
	}


int CWebSrvSocket2_base::subBuildPageExt(const char *path, const char *s,   
																				 MSG_HEADER *myMsg,CTime *myTime) {
	char myBuf[256];
	CFile mF;
	CFileStatus cfst;
	BYTE *p=NULL;
	int i;
	DWORD len;

	_tcscpy(myBuf,path);
	_tcscat(myBuf,s);

	if(mF.GetStatus(myBuf,cfst)) {
		len=cfst.m_size;
		m_Body->Reset();
		if(myTime)
			*myTime=cfst.m_mtime;
		if(cfst.m_mtime > myMsg->ifModifiedSince) {
			if(len < 201000000) {		// più di questo non va... boh e poi causa eccezione e rimane allocato!
				p=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,len+4);
				if(p) {
					if(mF.Open(myBuf,CFile::modeRead | CFile::shareDenyNone)) {
						i=mF.Read(p,len);
	//					m_Body->Attach(p,len,0); non va
						m_Body->Write(p,len);
						mF.Close();
						HeapFree(GetProcessHeap(),0,p);
						myMsg->status=200;
						}
					else {
						HeapFree(GetProcessHeap(),0,p);
						goto fine_errore;
						}
					}
				else {
					theApp.FileSpool->print(CLogFile::flagInfo,"  NON abbastanza memoria, %u",len);
					goto fine_errore;
					}
				}
			else {
				theApp.FileSpool->print(CLogFile::flagInfo,"  troppo grande, %u",len);
				goto fine_errore;
				}
			}
		else {
			m_Body->Reset();
			m_Body->WriteString(_T("non modificato"));
			myMsg->status=304;
			}
		return 1;
		}
	else {
fine_errore:
		myMsg->status=404;
		return 0;
		}
	}

class DirEntry {
public:
	CString Nome,Descrizione;
	DWORD Dimensione,Attributi;
	CTime Data;
public:
	static byte tSort;

public:
	void setSort(byte n) { tSort=n; }
  bool operator<(const DirEntry& elem2) const;
  bool operator>(const DirEntry& elem2) const;
	};

byte DirEntry::tSort;
bool DirEntry::operator>(const DirEntry& elem2) const { 

	switch(tSort & 0x7f) {
		case 0:
			return tSort & 0x80 ? Nome.CompareNoCase(elem2.Nome)>0 : Nome.CompareNoCase(elem2.Nome)<0;
			break;
		case 1:
			return tSort & 0x80 ? Data > elem2.Data : Data < elem2.Data;
			break;
		case 2:
			return tSort & 0x80 ? Dimensione > elem2.Dimensione : Dimensione < elem2.Dimensione;
			break;
		case 3:
			return tSort & 0x80 ? Descrizione.CompareNoCase(elem2.Descrizione)>0 : Nome.CompareNoCase(elem2.Descrizione)<0;
			break;
		default:
			return 0;
			break;
		}
	}

bool DirEntry::operator<(const DirEntry& elem2) const { 

	switch(tSort & 0x7f) {
		case 0:
			return tSort & 0x80 ? Nome.CompareNoCase(elem2.Nome)<0 : Nome.CompareNoCase(elem2.Nome)>0;
			break;
		case 1:
			return tSort & 0x80 ? Data < elem2.Data : Data > elem2.Data;
			break;
		case 2:
			return tSort & 0x80 ? Dimensione < elem2.Dimensione : Dimensione > elem2.Dimensione;
			break;
		case 3:
			return tSort & 0x80 ? Descrizione.CompareNoCase(elem2.Descrizione)<0 : Descrizione.CompareNoCase(elem2.Descrizione)>0;
			break;
		default:
			return 0;
			break;
		}
	}

DWORD CWebSrvSocket2_base::subBuildPageDir(const char *s, const char *path,int sort1,int sort2) {
	CFileFind finder;
	char myPath[128],realPath[256],myBuf1[512];
	int n;
	int tSort[4];			// b0=0..1; b7=0..1 (asc/desc)
	CArray <DirEntry,DirEntry> Files;

	tSort[0]= sort1==0 ? 1 | (sort2 ? 0x80 : 0) : 0;
	tSort[1]= sort1==1 ? 1 | (sort2 ? 0x80 : 0) : 0;
	tSort[2]= sort1==2 ? 1 | (sort2 ? 0x80 : 0) : 0;
	tSort[3]= sort1==3 ? 1 | (sort2 ? 0x80 : 0) : 0;

//#define NUM_MAX_FILES 300

	*myPath='/';
	_tcsncpy(myPath+1,s,126);
	myPath[127]=0;
	if(path) {
		_tcscpy(realPath,path);
		if(s[1]==':')
			_tcscat(realPath,s+2);
		else
			_tcscat(realPath,s);
		}
	else {
		*realPath=*s;
		realPath[1]=':';
		realPath[2]=0;
		}
	if(realPath[_tcslen(realPath)-1] != '\\')
		_tcscat(realPath,_T("\\"));

  //As a security precaution do not allow any URL's which contains any relative parts in it
  if(_tcsstr(s,_T("..")))
		return 0;

	_tcscat(realPath,"*.*");
	BOOL bWorking = finder.FindFile(realPath);
  if(!bWorking) 
		return 0;

	wsprintf(myBuf1,_T("%s<title>Directory di %s</title>\n"),Html4String,myPath);
	m_Body->WriteString(myBuf1);
	// al posto di html, per xhmtml usare: <html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
	m_Body->WriteString(getBodyString());
	wsprintf(myBuf1,_T("<h2>Directory di %s</h2><br>\n"),myPath);
	m_Body->WriteString(myBuf1);
	m_Body->WriteString("<font size=-1><table border=0>\n");
	wsprintf(myBuf1,"<tr><th width=240><img src='/images/diamond.gif' alt='Icon'> <a href='?C=0&O=%u'>Nome</a></th><th width=180><a href='?C=1&O=%u'>Ultima modifica</a></th><th width=170 align=right><a href='?C=2&O=%u'>Dimensione</a></th><th><a href='?C=3&O=%u'>Descrizione</a></th></tr>\n",
		tSort[0] & 1 ? (tSort[0] & 0x80 ? 0 : 1) : 0,
		tSort[1] & 1 ? (tSort[1] & 0x80 ? 0 : 1) : 0,
		tSort[2] & 1 ? (tSort[2] & 0x80 ? 0 : 1) : 0,
		tSort[3] & 1 ? (tSort[3] & 0x80 ? 0 : 1) : 0);
	m_Body->WriteString(myBuf1);

	n=0;  
	bWorking = finder.FindFile(realPath);
  while(bWorking) {
		bWorking = finder.FindNextFile();
		if(!finder.IsDots()) {
			class DirEntry de;
			de.Nome=finder.GetFileName();
			de.Descrizione=finder.GetFileName();
			de.Dimensione=finder.GetLength();
			de.Attributi=finder.IsHidden() ? 2 : 0 |
				finder.IsArchived() ? 8 : 0 |
				finder.IsCompressed() ? 16 : 0 |        
				finder.IsReadOnly() ? 1 : 0 |
				finder.IsSystem() ? 4 : 0 |
				finder.IsTemporary() ? 32 : 0 |
				finder.IsDots() ? 64 : 0 |
				finder.IsDirectory() ? 128 : 0;

				//Get the last modified date as a string
/*        TCHAR sDate[20];
        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);
        GetDateFormat(LOCALE_SYSTEM_DEFAULT, LOCALE_NOUSEROVERRIDE, &st, NULL, sDate, 20);

        //Get the last modified time as a string
        TCHAR sTod[20];
        GetTimeFormat(LOCALE_SYSTEM_DEFAULT, LOCALE_NOUSEROVERRIDE, &st, NULL, sTod, 20);*/

			finder.GetLastWriteTime(de.Data);

			de.setSort(sort1 | (sort2 ? 0x80 : 0));
			Files.Add(de);
			n++;
			}
		}
	finder.Close();

		
	c_arraysort<CArray<DirEntry,DirEntry>,DirEntry>(Files,sort_desc);

	for(int i=0; i<Files.GetSize(); i++) {
		char ch;					// se ci fossero apici nel nome farebbe casino...
		if(Files[i].Nome.Find('\'') >= 0)
			ch='\"';
		else if(Files[i].Nome.Find('\"') >= 0)
			ch='\'';
		else
			ch='\"';
		if(!(Files[i].Attributi & 2 /*FILE_ATTRIBUTE_HIDDEN*/)) {
			if(!Msg.WML) {
				wsprintf(myBuf1,"<tr><td width=220> <a href=%c/%s/%s%c><img src='/images/item.gif' alt='[   ]' border=0> %s</a> </td>",
					ch,s,(LPCTSTR)Files[i].Nome,ch,(LPCTSTR)Files[i].Nome);
				}
			else {		//FINIRE?? !!
				}
			}
		else {		// alcuni file non sono visibili e/o downloadabili (2013)!
			if(proprieta & (loggedAsSupervisor)) {
				wsprintf(myBuf1,"<tr><td width=220> <a href=%c/%s/%s%c><img src='/images/item.gif' alt='[   ]' border=0> %s</a> </td>",
					ch,s,(LPCTSTR)Files[i].Nome,ch,(LPCTSTR)Files[i].Nome);
				}
			else if(proprieta & (loggedAsUser)) {
				wsprintf(myBuf1,"<tr><td width=220><img src='/images/item.gif' alt='[   ]' border=0> %s </td>",Files[i].Nome);
				}
			else
				goto skippa;
			}
		m_Body->WriteString(myBuf1);

		wsprintf(myBuf1,"<td width=200 align=LEFT>%s</td>",(LPCTSTR)Files[i].Data.Format("%a %d/%m/%Y %H:%M"));
		m_Body->WriteString(myBuf1);

		m_Body->WriteString("<td width=170 align=RIGHT>");
		if(Files[i].Attributi & 128 /*FILE_ATTRIBUTE_DIRECTORY*/)
			_tcscpy(myBuf1,"DIR");
		else {
			CFileSizeString str;

			str.FormatSize(Files[i].Dimensione);
			_tcscpy(myBuf1,(LPCTSTR)str);
			}
		m_Body->WriteString(myBuf1);
		m_Body->WriteString("</td></tr>\n");

skippa: ;
		}

	if(!_tcscmp(realPath,"\\")) {
			// patch per vedere i fax... (solo in radice)
	//		_tcscat(p,"<tr><td><a href='C:/Documents and Settings/All Users.WINNT/Dati applicazioni/Microsoft/Shared Fax/Inbox/'><img src='/images/item.gif' alt='[   ]' border=0> FAX</a></td><td></td><td>DIR</td></tr>\n");
		m_Body->WriteString("<tr><td><a href='/_FAX_?C=1&O=1'><img src='/images/item.gif' alt='[   ]' border=0> FAX</a></td><td></td><td>DIR</td></tr>\n");
		}

	m_Body->WriteString("</table><font size=0><br><br>\n");
	return 1;
	}

char *CWebSrvSocket2_base::getBodyString(DWORD bkcolor,const char *extraStr) {

	if(Msg.WML) {		// finire...
		}
	else {
		wsprintf(bodyString,"<body text='#F0C0C0' link='#F0F020' vlink='#F0F020' alink='#F0F050' bgcolor='#%02X%02X%02X' %s>\n"
			"<font face='tahoma,arial'>",
			GetRValue(bkcolor),GetGValue(bkcolor),GetBValue(bkcolor),extraStr ? extraStr : "");
		}

	return bodyString;
	}

int CWebSrvSocket2_base::buildWMLPage(int cmd,const char *s,const char *parms) {
	char *p1,*statMsg,myBuf1[256];
	int i,bBinary;

	m_Body->Reset();
	bBinary=mimeTypeWml;
	Msg.howCache=1;				// abilita cache, per default
	m_Body->WriteString(WAP1String);
	m_Body->WriteString("<wml><head></head>\n<card id='card0' title='Non trovato'>\n<p><b>La pagina specificata non esiste.</b><br/></p>\n");
	bBinary=mimeTypeWml;
	Msg.status=200;
	statMsg="OK" /*"Found"*/;
	m_Body->WriteString("</wml>\n\n");
	CString S=CTimeEx::getNowGMT();	// questo ha con se' il CR/LF

	// finire!!!
	char header[1024];
	m_HTTPHeader->Reset();
	wsprintf(header,"HTTP/%u.%u %03u %s\r\nServer: Joshua/%u.%02u\r\nDate: %sAccept-Ranges: bytes\r\nContent-length: %u\r\nContent-type: %s\r\n",
		HIWORD(m_Parent->HTTPVer),LOWORD(m_Parent->HTTPVer),cmd,statMsg,HIWORD(theApp.getVersione()),LOWORD(theApp.getVersione()),(LPCTSTR)S,m_Body->GetLength(),getMimeTypeDescr(bBinary));
	m_HTTPHeader->AddToken(CHTTPHeader::TAG_NULL,header);
// non  finito... controllare!

  return 1;
	}

int CWebSrvSocket2_base::buildCGIPage(const char *s,const char *parms,
																			enum mimeTypes *bBinary,int *bAddBottom,BOOL isWML) {

	Msg.status=404;
	return 1;
	}


int CWebSrvSocket2_vidsend::buildWMLPage(int cmd,const char *s,const char *parms) {
	
	return CWebSrvSocket2_base::buildWMLPage(cmd,s,parms);
	}

int CWebSrvSocket2_vidsend::buildCGIPage(const char *s,const char *parms,
																				enum mimeTypes *bBinary,int *bAddBottom,int isWML) {
	char *p2=NULL,*statMsg,myBuf1[512],myBuf2[512],*s1;
	static char *autoRefreshedPage=isWML ?
		"<meta http-equiv='refresh' content='1; url=http://%s/casa_1.wml'></head>\n" :
//		"<head><META HTTP-EQUIV='REFRESH' CONTENT='1; URL=http://%s/casa_1.html'></head>\n";
		"<meta HTTP-EQUIV='REFRESH' CONTENT='1; URL=javascript:history.back(-1);'></head>\n";
		// o anche "5;url=javascript:history.back(-1);"  ??
	const char *okPage=isWML ?
		"<card id='card99' title='Eseguito'><p>200 OK</p></card>\n" :
		"<title>OK</title></head>\n<p>200 OK</p>\n";
	const char *okString=isWML ?
		"<p>200 OK</p>\n" :
		"<p>200 OK</p>\n";
	const char *myBackString=	isWML ? "<a href='/casa_1.wml'>Torna indietro!</a>" :
		"<a href='/casa_1.html'>Torna indietro!</A>";
	int i,j,n;
	char parmV[256],parmN[256];			// serve piu' spazio per il form utenti!	
	CString S1;
	CTime lastModTime=0;

	m_Body->Reset();

	if(!_tcsicmp(s,"/display.cgi")) {
		theApp.getVersione(myBuf2);
		CGif myGIF(myBuf2);
		CDigitalText myText;
		RECT myRect={0,0,100,52};
		CBitmap *pBitmap;


		if(parms) {
			*bBinary=mimeTypeGif;
			parseParm(parms,parmN,64,parmV,255,2);
			myText.Create(NULL,NULL,*parmV ? atoi(parmV) : CDigitalText::border | CDigitalText::showPlus | CDigitalText::nullAsInval | ((*parmV=='1') ? CDigitalText::LCD : 0),4,myRect,theApp.m_pMainWnd);
			parseParm(parms,parmN,64,parmV,255,1);
			if(*parmV) {
				if(_tcschr(parmV,'.'))
					myText.SetWindowText(atof(parmV),1);		// decimali dinamici?
				else
					myText.SetWindowText(atoi(parmV));
				}
			else
				myText.SetWindowText(NULL);
			if(pBitmap=myText.GetBitmap()) {
				DWORD len;

				BYTE *p1=myGIF.buildGIF(pBitmap,NULL,&len);
				m_Body->Write(p1,len);
				HeapFree(GetProcessHeap(),0,p1);
				delete pBitmap;
				}
			else
				goto non_trovato;
			}
		else
			goto non_trovato;
		}
	else if(!_tcsnicmp(s,"/connessioni.cgi",16)) {
		}
	else if(!_tcsnicmp(s,"/chat_msg.cgi",13)) {
		}
	else if(!_tcsnicmp(s,"/ping.cgi",9)) {
#ifndef _NEWMEET_MODE
		char *p;
		CPing myPing;
		CPingReply pr;

		i=0;
		if(*s) {
			parseParm(s,parmN,100,parmV,100,2);
			n=atoi(parmV);
			parseParm(s,parmN,100,parmV,100,1);
			if(*parmV) {
				i=myPing.Ping(parmV,n ? &pr : NULL);
				}
			else
				goto parm_errati;
			}
		m_Body->WriteString("<html><head><title>Risultati di PING</title></head>\n<BODY TEXT='#f0f0f0' LINK='#f0f020' VLINK='#80E020' BGCOLOR='#1010A0'><BR>\n");
		if(i) {
			Msg.status=200;
			wsprintf(myBuf2,"L'indirizzo IP %s risponde",parmV);
			m_Body->WriteString(myBuf2);
			wsprintf(myBuf2,n ? " in %u mSec...<BR>\n" : "." ,pr.RTT);
			m_Body->WriteString(myBuf2);
			m_Body->WriteString("<br>\n");
			}
		else {
			Msg.status=404;
			wsprintf(myBuf2,"L'indirizzo IP %s non risponde.<BR>\n",parmV);
			m_Body->WriteString(myBuf2);
			}
#else
		m_Body->WriteString("<html><head><title>Risultati di PING</title></head>\n<BODY TEXT='#f0f0f0' LINK='#f0f020' VLINK='#80E020' BGCOLOR='#1010A0'><BR>\n");
		m_Body->WriteString("Funzione non implementata.<BR>\n");
#endif
		*bAddBottom=1;
		}
	else {
parm_errati:
		Msg.status=400;
		m_Body->WriteString("<html><title>OK</title>\n<BODY>400 Parametri errati<BR>\n");
		}
	return 1;

non_trovato:
	Msg.status=404;
	return 0;
	}


int CWebSrvSocket2_base::subBuildPageStatus(int *myAddBottom) {
	MEMORYSTATUS ms;
	SYSTEM_INFO si;
	OSVERSIONINFO osvi;
//	char *CPUTypes[6]={"386","486","Pentium","Pentium II","Pentium III"};
	ULARGE_INTEGER l1,l2,l3;
	__int64 _l2,_l3;
	char myBuf[512],myBuf2[128];
	int i,n;
	CTimeSpan ct;
	CString S;
	const char *stringaSi="s",*stringaNo="NO";

//	if(Msg.WML) {			// FARE??


	m_Body->WriteString("<head><title>Situazione generale del server</title></head>\n");
	m_Body->WriteString(getBodyString());
	m_Body->WriteString("<font size=-1>");
	wsprintf(myBuf,"Il tuo browser : %s.<br>",Msg.userAgent);
	m_Body->WriteString(myBuf);
  theApp.getVersione(myBuf2);
#ifdef _DEBUG
	wsprintf(myBuf,"Versione dell'applicazione server: %s; [DEBUG]",myBuf2);
#else
	wsprintf(myBuf,"Versione dell'applicazione server: %s; ",myBuf2);
#endif
	m_Body->WriteString(myBuf);
	GetModuleFileName(NULL,myBuf2,128);
	CFileStatus cf;
	CFile::GetStatus(myBuf2,cf);
	m_Body->WriteString((LPCTSTR)cf.m_mtime.Format(" (%d/%m/%Y %H:%M)"));
	m_Body->WriteString("<br>\n");
	wsprintf(myBuf,"Versione del Web-Server: %u.%02u; ",
		HIWORD(m_Parent->WWWVer),LOWORD(m_Parent->WWWVer));
	m_Body->WriteString(myBuf);
	m_Body->WriteString("<br>\n");

	m_Body->WriteString("[");
	m_Body->WriteString((LPCTSTR)CTimeEx::getNowGMT());
	m_Body->WriteString("]<br>\n");

	osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	wsprintf(myBuf,"Versione di Windows: %u.%02u.%u<br>\n<br>\n",osvi.dwMajorVersion,osvi.dwMinorVersion,osvi.dwBuildNumber);
	m_Body->WriteString(myBuf);
	GetSystemInfo(&si);
	wsprintf(myBuf,"Tipo di microprocessore: n.%u %s %u, livello 0x%x, revisione %04x<br>",
		si.dwNumberOfProcessors,
		si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL ? "Intel" : "Altro",
		si.dwProcessorType,
		si.wProcessorLevel,
		si.wProcessorRevision);
	m_Body->WriteString(myBuf);
	ms.dwLength=sizeof(MEMORYSTATUS);
	GlobalMemoryStatus(&ms);
	wsprintf(myBuf,"Memoria RAM: totale fisica=%uKB, disponibile=%uKB, %% usata=%u; totale pagefile=%uKB, pagefile disponibile=%uKB; totale virtuale=%uKB, virtuale disponibile=%uKB.<br>\n",
		ms.dwTotalPhys/1024,ms.dwAvailPhys/1024,ms.dwMemoryLoad,ms.dwTotalPageFile/1024,ms.dwAvailPageFile/1024,ms.dwTotalVirtual/1024,ms.dwAvailVirtual/1024);
	m_Body->WriteString(myBuf);
//	GetFreeSystemResources
	i=GetDiskFreeSpaceEx("C:\\",&l1,&l2,&l3);
	_l2=l2.QuadPart;
	_l3=l3.QuadPart;
	wsprintf(myBuf,"Spazio disponibile sull'hard disk=%u KBytes (su %u KBytes totali).<br>\n",(DWORD)(_l3/1024),(DWORD)(_l2/1024));
	m_Body->WriteString(myBuf);
	i=GetDiskFreeSpaceEx("D:\\",&l1,&l2,&l3);
	_l2=l2.QuadPart;
	_l3=l3.QuadPart;
	wsprintf(myBuf," spazio disponibile sull'hard disk 2=%u KBytes (su %u KBytes totali).<br>\n",(DWORD)(_l3/1024),(DWORD)(_l2/1024));
	m_Body->WriteString(myBuf);


	m_Body->WriteString("</font><br>\n");

	*myAddBottom=3;
	return 1;
	}

int CWebSrvSocket2_base::buildHTMLPage(int cmd,const char *s,const char *parms) {
	char *statMsg;
	int i,bAddBottom;
	enum mimeTypes bBinary;

	m_Body->Reset();
	m_Body->WriteString(Html4String);
	m_Body->WriteString("pagina di base");
	bBinary=mimeTypeHtml;
	switch(cmd) {
		case 200:
			statMsg="OK";
			Msg.status=200;
			break;
		case 201:
			statMsg="OK";
			Msg.status=200;
			break;
		case 202:
			m_Body->WriteString("\nPUT non supportato!");
			statMsg="OK";
			Msg.status=200;
			break;
		default:
			Msg.status=405;			// not allowed
			break;
		}
	prepareHTTPHeader(statMsg,bBinary,0);
// non  finito... controllare!
	return 1;
	}

int CWebSrvSocket2_vidsend::buildHTMLPage(int cmd,const char *s,const char *parms) {
	char *p2=NULL,*statMsg,myBuf1[1024],myBuf2[1024],*s1;
	int i,n,bAddBottom;
	enum mimeTypes bBinary;
	DWORD len2;
	CTime lastModTime;
	const char *okPage="<html><title>OK</title></head>\n<body>200 OK<br>\n";
	const char *okString="200 OK<br>\n";
	// al posto di html, per xhtml usare: <html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
	DWORD len;

	m_Body->Reset();

	bBinary=mimeTypeHtml;
	lastModTime=CTime::GetCurrentTime();
	bAddBottom=3;
	Msg.expires=0;				// ...e quindi toglie header "expires"...

	m_Body->WriteString(Html4String);
	m_Body->WriteString("<html lang='it'>\n");
		// al posto di html, per xhmtml usare: <html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
	m_Body->WriteString(HtmlContent);

	switch(cmd) {
		case 200:					 // get
		case 202:					 // head
		wsprintf(bodyString,"<BODY TEXT='#F0C0C0' LINK='#F0F020' VLINK='#F0F020' BGCOLOR='#202060'>\n<FONT FACE='tahoma,arial'>");
		// usare font verdana?
		if(!_tcsnicmp(s,m_Parent->CGIPath,_tcslen(m_Parent->CGIPath))) {
			Msg.howCache=0;
			s+=_tcslen(m_Parent->CGIPath)-1;					// punto al nome...
			buildCGIPage(s,parms,&bBinary,&bAddBottom,0);
			}
		else {			// per default, nella root!
			Msg.howCache=1;				// abilita cache, per default...
			s++;
			if(!*s || !stricmp(s,"index.html")) {
				m_Body->WriteString("<html><head><title>Videosender Main Page</title></head>\n<body TEXT='#f0c0c0' LINK='#f0f020' VLINK='#80E020' BGCOLOR='#2020C0'><FONT FACE='tahoma,arial'>\n<center><h1>Videosender Main Page</h1><br>\n");
				m_Body->WriteString("<h3><a href='main.html'>Cliccate qui</a> per proseguire.</h3><BR><BR><BR><BR>");
				bBinary=mimeTypeHtml;
				bAddBottom=0;
				}
			else if(!stricmp(s,"main.html")) {
				m_Body->WriteString("<html><head><title>Videosender</title></head>\n");
				m_Body->WriteString("<FRAMESET rows=60%,40%>\n");
				m_Body->WriteString("<frame SRC='frame_1.html' NAME='frame1' MARGINHEIGHT=0 MARGINWIDTH=0 SCROLLING=auto>\n");
				m_Body->WriteString("<frame SRC='frame_2.html' NAME='frame2' MARGINHEIGHT=0 MARGINWIDTH=0 SCROLLING=auto>\n");
				m_Body->WriteString("</FRAMESET><BR><BR>\n");
				m_Body->WriteString("<NOFRAMES>Occorre un browser che supporti i frame!</NOFRAMES><BR>\n");
				m_Body->WriteString("<br>\n<A href='/'><IMG SRC='redball.gif' ALIGN=center BORDER=0> Torna</a> alla pagina principale.<BR><BR><BR><BR></NOFRAMES>\n");
				}
			else if(!stricmp(s,"frame_1.html")) {
				m_Body->WriteString("<HTML>\n");
				wsprintf(myBuf2,"<head><META HTTP-EQUIV='REFRESH' CONTENT='%d'></head>\n",30);
				m_Body->WriteString(myBuf2);
				m_Body->WriteString(bodyString);
				m_Body->WriteString("<CENTER><FONT COLOR='#F0F0C0'><H1><IMG SRC='pallina2.gif' VALIGN=middle> VideoSender</H1></FONT></CENTER><BR>\n");
				m_Body->WriteString("<table BORDER=0>\n");
				wsprintf(myBuf2,"<TR><TD>%s</TD></TR>\n",theApp.getNow(myBuf1));
				m_Body->WriteString(myBuf2);
				m_Body->WriteString("</table>\n<table BORDER=0>\n");
				m_Body->WriteString("<TR><TD><IMG SRC='pallina2.gif' VALIGN=middle> <IMG SRC='webcam1.jpg' ALIGN=center ALT='Telecamera 1'></TD>\n");
				m_Body->WriteString("<TD><IMG SRC='pallina2.gif' VALIGN=middle> <IMG SRC='webcam2.jpg' ALIGN=center ALT='Telecamera 2'></TD></TR>\n");
				m_Body->WriteString("</table><BR><BR>\n");
				bAddBottom=1;
				}
			else if(!stricmp(s,"frame_2.html")) {
				m_Body->WriteString("<HTML>\n");
				wsprintf(myBuf2,"<head></head>\n");
				m_Body->WriteString(myBuf2);
				m_Body->WriteString(bodyString);
				m_Body->WriteString("<table BORDER=0>\n");
				m_Body->WriteString("<TR><TD><IMG SRC='pallina2.gif' VALIGN=middle> <A href='radmin.html' TARGET='_ALL'>Gestione remota</A></TD>\n");
				m_Body->WriteString("</TR><TR><TD><IMG SRC='pallina2.gif' VALIGN=middle> <A href='cservers.html' TARGET='_ALL'>Elenco server disponibili</A></TD>\n");
				m_Body->WriteString("</TR><TR><TD><IMG SRC='pallina2.gif' VALIGN=middle> <A href='chat.html' TARGET='_ALL'>Chat/Lavagna</A></TD>\n");
				m_Body->WriteString("</TR>\n");
				m_Body->WriteString("</table><BR><BR>\n");
				wsprintf(myBuf2,"<IMG SRC='greenball.gif'>Sito in allestimento<BR>\n");
				m_Body->WriteString(myBuf2);
				bAddBottom=1;
				}
			else if(!stricmp(s,"radmin.html")) {
				m_Body->WriteString("<HTML>\n");
				wsprintf(myBuf2,"<head><title>Gestione remota VideoSender</title></head>\n");
				m_Body->WriteString(myBuf2);
				m_Body->WriteString(bodyString);
				m_Body->WriteString("<table BORDER=1>\n");
				m_Body->WriteString("<TR>\n");
				if(theApp.theServer) {
					CControlSrvSocket2 *myRoot;
					CString aIP,S;
					UINT aPort;
					POSITION po,po1;

					po=theApp.theServer->controlSocket->cSockRoot.GetHeadPosition();
					if(po) {
						do {
							po1=po;
							myRoot=theApp.theServer->controlSocket->cSockRoot.GetNext(po);
							if(myRoot->m_hSocket != INVALID_SOCKET) {
								myRoot->GetPeerName(aIP,aPort);
								S=aIP;
								wsprintf(myBuf2,"<TD><IMG SRC='pallina2.gif' VALIGN=middle> <A href='/cgi-bin/connessioni.cgi?id=%u'> %s <FONT SIZE=-1>(%s)</FONT></A></TD>\n",
									(DWORD)myRoot,(LPCTSTR)S,(LPCTSTR)aIP);
								m_Body->WriteString(myBuf2);
								}
							} while(po);
						}
					}
					
				m_Body->WriteString("</TR>\n");
				m_Body->WriteString("</table><BR><BR>\n");
				bAddBottom=1;
				}
			else if(!stricmp(s,"cservers.html")) {
				m_Body->WriteString("<HTML>\n");
				wsprintf(myBuf2,"<head><title>Elenco dei server video/audio disponibili (VideoSender)</title></head>\n");
				if(theApp.OpzioniDirSrv & CVidsendDoc7::ancheAccessoWeb) {
					CDirectorySrvSocket2 *myRoot;
					CString aIP,S;
					UINT aPort;

					m_Body->WriteString(myBuf2);
					m_Body->WriteString(bodyString);
					m_Body->WriteString("<table BORDER=1>\n");

					POSITION po,po1;

					po=theApp.dirSocket->cSockRoot.GetHeadPosition();
					if(po) {
						do {
							po1=po;
							myRoot=theApp.dirSocket->cSockRoot.GetNext(po);
							if(myRoot->m_hSocket != INVALID_SOCKET) {
								myRoot->GetPeerName(aIP,aPort);
								wsprintf(myBuf2,"<TR><TD><IMG SRC='pallina2.gif' VALIGN=middle> <A href='http://%s'><IMG SRC='http://%s/webcam1.jpg' ALIGN=center width=80 ALT='Anteprima'> %s (%s)</A></TD></TR>\n",
									(LPCTSTR)aIP,(LPCTSTR)aIP,(LPCTSTR)myRoot->connName,(LPCTSTR)aIP);
								m_Body->WriteString(myBuf2);
								}
							} while(po);
						}
					
					m_Body->WriteString("</table><BR><BR>\n");
					}
				else {
					m_Body->WriteString("<BR><H3>Accesso non consentito!</H3><BR>\n");
					}
				bAddBottom=1;
				}
			else if(!stricmp(s,"chat.html")) {
				m_Body->WriteString("<HTML>\n");
				wsprintf(myBuf2,"<head><title>Chat di VideoSender</title></head>\n");
				if(theApp.theChat && theApp.theChat->Opzioni & CVidsendDoc4::ancheAccessoWeb) {
					m_Body->WriteString(myBuf2);
					m_Body->WriteString("<head><META HTTP-EQUIV='REFRESH' CONTENT='15'></head>\n");
					m_Body->WriteString("<BODY TEXT='#202020' LINK='#F02020' VLINK='#F02020' BGCOLOR='#e0e0e0'>\n<FONT FACE='tahoma,arial'>");
					m_Body->WriteString("<table BORDER=1>\n");
					POSITION pos=theApp.theChat->GetFirstViewPosition();
					CVidsendView4 *v=(CVidsendView4 *)theApp.theChat->GetNextView(pos);
					CListBox *b=(CListBox *)v->GetDlgItem(IDC_LIST1);
					struct CHAT_MESSAGE *m;
					DWORD t;
					n=b->GetCount();
					for(i=max(n-21,0); i<n; i++) {			// # msg fisso!!
						m=(struct CHAT_MESSAGE *)b->GetItemData(i);
						if(m && (DWORD)m != LB_ERR) {
							s1=m->sender;
							while(*s1) {
								if(*s1 == '<')
									*s1='[';
								if(*s1 == '<')
									*s1=']';
								s1++;
								}
							s1=m->message;
							while(*s1) {
								if(*s1 == '<')
									*s1='[';
								if(*s1 == '>')
									*s1=']';
								s1++;
								}
							t=m->color;
							if(m->extra & CVidsendView4::reverseMsg)
								t ^= 0xffffff;
							wsprintf(myBuf2,"<TR><TD><A href='/cgi-bin/chat_msg.cgi?id=%u'><IMG SRC='%s.gif' VALIGN=center BORDER=0></A> <FONT COLOR='%02X%02X%02X'>%s] <I>%s</I></FONT></TD></TR>\n",
								m,"chatmsg",t & 255,(t >> 8) & 255,(t >> 16) & 255,m->sender,m->message);
							m_Body->WriteString(myBuf2);
							}
						}
					m_Body->WriteString("</table><BR>\n");
					m_Body->WriteString("<FORM METHOD=POST ACTION='/cgi-bin/chat_send.cgi'><INPUT TYPE=text NAME='msg' size=100> <INPUT TYPE=submit VALUE='Invia!'></FORM>\n");
					}
				else {
					m_Body->WriteString("<BR><H3>Accesso non consentito!</H3><BR>\n");
					}
				bAddBottom=1;
				}
#ifdef _DEBUG
			else if(!_tcsnicmp(s,"espos/proxynmvidsend.asp",24)) {		// per prove in locale
				m_Body->WriteString("<HTML>\n");
				wsprintf(myBuf2,"<head><title>200 update OK</title></head>\n");
				m_Body->WriteString(myBuf2);
				m_Body->WriteString(bodyString);
				m_Body->WriteString("Prova<BR>\n");
				bAddBottom=1;
				}
#endif
			else if(!_tcsicmp(s,"webcam1.jpg")) {
				CJpeg myJPEG;
				CBitmap b;
				BYTE *p1;

				bBinary=mimeTypeJpeg;
				m_Body->Reset();
				{

					if(theApp.theServer) {
						if(theApp.theServer->theTV) {
							if(theApp.theServer->theTV->theFrame && (int)theApp.theServer->theTV->theFrame != -1) {
								if(b.CreateBitmap(theApp.theServer->theTV->biRawDef.bmiHeader.biWidth,theApp.theServer->theTV->biRawDef.bmiHeader.biHeight,1,24 /* ?? */,NULL)) {
									if(b.SetBitmapBits(theApp.theServer->theTV->biRawDef.bmiHeader.biWidth*theApp.theServer->theTV->biRawDef.bmiHeader.biHeight*3 /* ?? */,theApp.theServer->theTV->theFrame)) {
										p1=myJPEG.buildJPEG(&b,&len,TRUE,theApp.theServer->myQV.quality/100);
										m_Body->Write(p1,len);
										HeapFree(GetProcessHeap(),0,p1);
#pragma warning { verificare memoria
										HeapFree(GetProcessHeap(),0,theApp.theServer->theTV->theFrame);
										theApp.theServer->theTV->theFrame=NULL;
										}
									else
										goto video_failed;
									}
								else
									goto video_failed;
								}
							else {
								theApp.theServer->theTV->theFrame=(BYTE *)-1;
								goto video_failed;
								}
							}
						else
							goto video_failed;
						}
					else {
video_failed:
						p1=myJPEG.buildJPEG(IDB_MONOSCOPIO,&len,75);
						m_Body->Write(p1,len);
						HeapFree(GetProcessHeap(),0,p1);
						}

					Msg.howCache=0;
					Msg.expires=1;
					Msg.status=200;
					}
				}
			else if(!stricmp(s,"webcam2.jpg")) {
				CJpeg myJPEG;
				CBitmap b;
				BYTE *p1;
				bBinary=mimeTypeJpeg;
				m_Body->Reset();
				{

					// verificare webcam2.jpg su client!
					if(theApp.aClient[0]) {
						if((CVidsendView *)theApp.aClient[0]->getView()) {
							if(((CVidsendView *)theApp.aClient[0]->getView())->getFrame() && (int)((CVidsendView *)theApp.aClient[0]->getView())->getFrame() != -1) {
								if(b.CreateBitmap(((CVidsendView *)theApp.aClient[0]->getView())->biRawDef.biWidth,((CVidsendView *)theApp.aClient[0]->getView())->biRawDef.biHeight,1,24 /* ?? */,NULL)) {
									if(b.SetBitmapBits(((CVidsendView *)theApp.aClient[0]->getView())->biRawDef.biWidth*((CVidsendView *)theApp.aClient[0]->getView())->biRawDef.biHeight*3 /* ?? */,((CVidsendView *)theApp.aClient[0]->getView())->getFrame())) {
										p1=myJPEG.buildJPEG(&b,&len,TRUE,theApp.theServer->myQV.quality/100);
										m_Body->Write(p1,len);
										HeapFree(GetProcessHeap(),0,p1);
//										GlobalFree(((CVidsendView *)theApp.aClient[0]->getView())->getFrame());
										}
									else
										goto video_failed2;
									}
								else
									goto video_failed2;
								}
							else {
								theApp.theServer->theTV->theFrame=(BYTE *)-1;
								goto video_failed2;
								}
							}
						else
							goto video_failed2;
						}
					else {
video_failed2:
						p1=myJPEG.buildJPEG(IDB_MONOSCOPIO,&len,75);
						m_Body->Write(p1,len);
						HeapFree(GetProcessHeap(),0,p1);
						}

					Msg.howCache=0;
					Msg.expires=1;
					Msg.status=200;
					}
				}
			else if(!stricmp(s,"chatmsg.gif")) {
				m_Body->Reset();
				theApp.getVersione(myBuf1);
				CGif myGIF(myBuf1);
				BYTE *p1;
				if(theApp.theChat) {
					POSITION pos=theApp.theChat->GetFirstViewPosition();
					CVidsendView4 *v=(CVidsendView4 *)theApp.theChat->GetNextView(pos);

					if(v) {
						bBinary=mimeTypeGif;
						p1=myGIF.buildGIF(v->hIconUser,&len);
						m_Body->Write(p1,len);
						HeapFree(GetProcessHeap(),0,p1);
						}
					}
				}
			else if(!strchr(s,'.')) {
				char parmV[128],parmN[128];
				int sort1=parseNumParm(parms,parmN,64,parmV,127,1);
				int sort2=parseNumParm(parms,parmN,64,parmV,127,2);
				i=subBuildPageDir(myBuf2,m_Parent->WWWRoot,sort1,sort2);
				if(i)
					Msg.status=200;
				else
					Msg.status=404;		// non dovrebbe capitare...
				}
			else if(*s=='/') {
				m_Body->WriteString("<HTML><BODY>\n<H2>Questo non è un proxy server!</H2>");
				}
			else {
				bAddBottom=0;
				s1=(char *)s;
				while(*s1) {
					if(*s1=='/')
						*s1='\\';
					s1++;
					}
				if(s[1] == ':') {
					myBuf1[0]=s[0];
					myBuf1[1]=s[1];
					myBuf1[2]=0;
					i=subBuildPageExt(m_Parent->WWWRoot,s+3,&Msg,&lastModTime);
//					p2=subBuildPageExt(myBuf1,s+3,len);
					}
				else
					i=subBuildPageExt(m_Parent->WWWRoot,s,&Msg,&lastModTime);
//					p2=subBuildPageExt(m_Parent->WWWRoot,s,len);
				if(i) {
					bBinary=setMimeType(s+1);
					}
				else	{					 // qui per memoria insufficiente...
non_trovato:
					Msg.status=404;
					m_Body->WriteString("<HTML><head><title>Non trovato</title></head>\n<BODY TEXT='#D0D0D0' LINK='#A0A0A0' VLINK='#80E020' BGCOLOR='#2020C0'><H1>Non trovato</H1> <H3>La pagina specificata non esiste.</H3><BR><BR><BR><BR>");
					bAddBottom=1;
					bBinary=mimeTypeHtml;
					}
				}
			}
		break;
	case 201:				 // post
		if(!_tcsnicmp(s,m_Parent->CGIPath,_tcslen(m_Parent->CGIPath))) {
			char parmV[128],parmN[128];

			s+=8;					// punto al nome...
			if(!_tcsnicmp(s,"/chat_send.cgi",14)) {
				if(theApp.theChat) {
					CString aIP,S;
					UINT aPort;
					struct CHAT_MESSAGE m;

					parseParm(s,parmN,100,parmV,100,1);
					if(*parmV) {			// no vuoto!
						GetPeerName(aIP,aPort);
						_tcscpy(m.message,parmV);
						m.id=2;
						m.color=RGB(0,0,0);
						m.extra=0;
						_tcscpy(m.sender,(LPCTSTR)aIP);
						theApp.theChat->chatSocket.Manda((char *)&m,sizeof(struct CHAT_MESSAGE));
						}
					Msg.status=200;
					}
				else {
					Msg.status=400;
					}
				}
			else {
				Msg.status=404;		// per ora niente!
				m_Body->WriteString(okString);
				}
			}
		break;
	case 203:				 // put
		break;
	case 204:				 // delete
		Msg.status=404;
		bAddBottom=1;
		bBinary=mimeTypeHtml;
		break;
	default:
		Msg.status=501;
		m_Body->WriteString(_T("<title>Metodo non accettato</title></head>\n<body TEXT='#D0D0D0' link='#F0F020' vlink='#80E020' bgcolor='#2020C0'><H1>Non trovato</h1> <h3>La pagina specificata non esiste.</h3><br><img src='/images/doh.gif'><br><br><br>\n"));
//		Msg.status=400;
//		m_Body->WriteString("<HTML><head><title>Non trovato</title></head>\n<BODY TEXT='#D0D0D0' LINK='#F0F020' VLINK='#80E020' BGCOLOR='#2020C0'><H1>Non trovato</H1> <H3>La pagina specificata non esiste.</H3><BR><BR><BR><BR>\n");
		break;
		}

	switch(Msg.status) {
//	Status Code Definitions
//- 100 Continue
//- 101 Switching Protocols
//- 200 OK
//- 201 Created			(inviare header "Location: ", dove è stato creato..)
//- 202 Accepted
//- 203 Non-Authoritative Information
//- 204 No Content		// indica che il client NON deve caricare una nuova pagina
//- 205 Reset Content
//- 206 Partial Content
//- 207 HTTP_STATUS_WEBDAV_MULTI_STATUS
//- 300 Multiple Choices
//- 301 Moved Permanently
//- 302 Moved Temporarily		// reindirizzamento...
//- 303 See Other
//- 304 Not Modified
//- 305 Use Proxy
//- 306 - No Longer Used 
//- 307 Temporary Redirect 
//- 400 Bad Request
//- 401 Unauthorized		// vuol dire che ci vuole un autorizzazione = Authentication Required
//- 402 Payment Required
//- 403 Forbidden			// vuol dire che anche con autorizzazione l'accesso e' vietato
//- 404 Not Found
//- 405 Method Not Allowed
//- 406 Not Acceptable
//- 407 Proxy Authentication Required
//- 408 Request Timeout
//- 409 Conflict
//- 410 Gone
//- 411 Length Required
//- 412 Precondition Failed
//- 413 Request Entity Too Large
//- 414 Request-URI Too Long
//- 415 Unsupported Media Type
//- 416 - Requested Range Not Satisfiable 
//- 417 - Expectation Failed (v. header Expect:)
//- 449 HTTP_STATUS_RETRY_WITH
//- 500 Internal Server Error
//- 501 Not Implemented
//- 502 Bad Gateway
//- 503 Service Unavailable
//- 504 Gateway Timeout
//- 505 HTTP Version Not Supported
		case 200:
		case 204:
		case 201:
		case 202:			// ?
			statMsg="OK";
			break;
		case 302:							 // per ora non usato
			statMsg="Trovato";
			break;
		case 304:							 // per ora non usato
			statMsg="Non modificato";
			break;
		case 400:
			statMsg="Richiesta incomprensibile";
			Msg.keepAliveR=0;
			break;
		case 401:		// questo causa la comparsa del box "utente/password" !!
			statMsg="Utente non autorizzato: inserire credenziali";
			Msg.keepAliveR=0;
			break;
		case 403:
			statMsg="Accesso vietato";
			Msg.keepAliveR=0;
			break;
		case 404:
			statMsg="Non trovato";
			Msg.keepAliveR=0;
			break;
		default:
			statMsg="[errore interno]";
			Msg.keepAliveR=0;
			break;
		}
	switch(bBinary) {
		case mimeTypeHtml:
			switch(bAddBottom) {
				case 3:
					m_Body->WriteString(copyString);
					break;
				case 2:
					wsprintf(myBuf2,"<br>\n%03u %s<br>",Msg.status,statMsg);
					m_Body->WriteString(myBuf2);
				case 1:
					m_Body->WriteString("\n</font></body></html>\n\n");
					break;
				default:
					break;
				}

			prepareHTTPHeader(statMsg,bBinary,lastModTime);

			if(proprieta & loggedAsUser) {		//PROVA!!
				CString usercookie;

				usercookie="joshuauser=";
				usercookie += m_LineText->commLogin;
				m_HTTPHeader->AddToken(CHTTPHeader::TAG_SET_COOKIE,usercookie);
				usercookie="joshuapasw=";
				usercookie+=m_LineText->commPswd;
				m_HTTPHeader->AddToken(CHTTPHeader::TAG_SET_COOKIE,usercookie);
				usercookie.Format("JSESSIONID=%u",sessionID);
				m_HTTPHeader->AddToken(CHTTPHeader::TAG_SET_COOKIE,usercookie);
				}

			break;
		default:		// se binario, p1 è stato deallocato e riallocato e ora contiene i dati binari, e *len la sua lunghezza
								// lascio cmq lo switch... anche se per ora non serve!
			if(m_Body->GetLength()) {
				prepareHTTPHeader(statMsg,bBinary,lastModTime);
				}
			if(theApp.debugMode>2) {
				theApp.FileSpool->print(CLogFile::flagInfo,p2);
				theApp.FileSpool->print(CLogFile::flagInfo,"len di p2=%d, di bin=%d",len2,len);
				}
			break;
		}

//	GlobalFree(p1);

  return Msg.keepAliveR;
	}


CString CWebSrvSocket2_base::prepareHTTPHeader(const char *statMsg,enum mimeTypes type,CTime lastModified) {
	char p1[1024];	// finire con CHTTPHEADER

	m_HTTPHeader->Reset();

	wsprintf(p1,"HTTP/%u.%u %03u %s\r\nServer: Joshua/%u.%02u",
		HIWORD(m_Parent->HTTPVer),LOWORD(m_Parent->HTTPVer),Msg.status,statMsg,HIWORD(theApp.getVersione()),LOWORD(theApp.getVersione())
		);
	m_HTTPHeader->AddToken(p1);
	m_HTTPHeader->AddToken(CHTTPHeader::TAG_DATE,CTimeEx::getNowGMT(FALSE));
	m_HTTPHeader->AddToken(CHTTPHeader::TAG_CONTENT_LENGTH,m_Body->isInFile ? m_Body->forcedLength : m_Body->GetLength());
	m_HTTPHeader->AddToken(CHTTPHeader::TAG_CONTENT_TYPE,getMimeTypeDescr(type));

	if(Msg.keepAliveR)
		m_HTTPHeader->AddToken(CHTTPHeader::TAG_CONNECTION,CHTTPHeader::tagKeepAlive);
	else
		m_HTTPHeader->AddToken(CHTTPHeader::TAG_CONNECTION,CHTTPHeader::tagClose);

	switch(Msg.howCache) {
		case 0:
			m_HTTPHeader->AddToken(CHTTPHeader::TAG_CACHE_CONTROL,_T("no-cache"));
			break;
		case 1:
//				_tcscat(p1,_T("Cache-Control: max-age=10\r\n"));
			break;
		}
	if(Msg.expires > 0) {
		m_HTTPHeader->AddToken(CHTTPHeader::TAG_EXPIRES,CTimeEx::getNowGMT(FALSE));
		}

	if(Msg.status == 401) {
		m_HTTPHeader->AddToken(CHTTPHeader::TAG_AUTHENTICATE,_T("'Joshua'"));
		// serve QUESTO per far comparire il box di login nel browser! (v. 401)
		}
	if(Msg.status == 302 /* anche 301 */) {
//		_tcscat(p1,"Uri: '/casa.html'\r\n");		// PARAMETRIZZARE!
//		if(HTTPVer > MAKELONG(0,1))
		if(stristr(Msg.referer,"indexold.html"))
			m_HTTPHeader->AddToken(CHTTPHeader::TAG_LOCATION,_T("/casa.html"));
		else
			m_HTTPHeader->AddToken(CHTTPHeader::TAG_LOCATION,_T("/index.html"));
				// PARAMETRIZZARE! (meglio questo, con HTTP 1.1 !
		//	else
//		_tcscat(p1,"Uri: '/casa.html'\r\n");		// PARAMETRIZZARE! e v. ANCHE LOGIN.CGI!!
		// redirect...
		}
	if(*Msg.contentLocation)
		m_HTTPHeader->AddToken(CHTTPHeader::TAG_CONTENT_LOCATION,Msg.contentLocation);

	if(m_Parent->HTTPVer > MAKELONG(0,1)) {
		if(lastModified != 0) {
			CString S=asctime(lastModified.GetLocalTm());
			S=S.Left(24);
			m_HTTPHeader->AddToken(CHTTPHeader::TAG_MODIFIED,S);
			// se per caso lastModified fosse successiva a CurrentTime, va mandata questa...!
			}
		}

//  CString sLine = _T("Allow: GET, HEAD, PUT, POST, DELETE\r\n");

	

//	_tcscat(p1,"\r\n");
		//		theApp.FileSpool->print(2,myBuf2);
		//		theApp.FileSpool->print(2,p1);
//		theApp.FileSpool->print(2,myBuf2);
//		theApp.FileSpool->print(2,p2);
//		theApp.FileSpool->print(2,"len di p2=%d",*len);
	return m_HTTPHeader->GetBuffer();
	}

int CWebSrvSocket2_base::SendHeader() { 
	int i;

	m_HTTPHeader->Finalize();
	if((i=Send(m_HTTPHeader->GetBuffer(),m_HTTPHeader->GetLength())) > 0) {
		m_Parent->sentBytes+=i;
		return i;
		}
	else
		return i;
	}

int CWebSrvSocket2_base::SendBody() { 
	int i,n,retVal=1;
	DWORD lenSoFar;
#define A_CHUNK 65536UL
	BYTE *pBuf=new BYTE[A_CHUNK+4];

	if(m_Body->isInFile) {
		CFile mF;
//			if(len < 1001000000UL) {		// pi di questo non (anda)va... boh e poi causa eccezione e rimane allocato!
		lenSoFar=m_Body->forcedLength;
		if(pBuf) {
			if(mF.Open(m_Body->fileName,CFile::modeRead | CFile::shareDenyNone)) {
				do {
//					theApp.TCPIP_WD=0;			// se no salta!
					n=mF.Read(pBuf,min(A_CHUNK,lenSoFar));
//					m_Body->Attach(pBuf,len,0); non va, ma alla fine qua faccio diversamente

					if((n=Send(pBuf,n)) < 0) {			// errore (tipo: troppo lungo)
						Close();									// quindi forzo chiusura!
						goto fine_errore;
						}
					lenSoFar-=n;
					m_Parent->sentBytes+=n;
					} while(lenSoFar>0);
				mF.Close();
				goto fine;
				}
			else {
				goto fine_errore;
				}
			}
		else {
			theApp.FileSpool->print(CLogFile::flagInfo,"  NON abbastanza memoria, %u,%u, inFile=%u",A_CHUNK,lenSoFar,m_Body->isInFile);
			goto fine_errore;
			}
//			else {
//				theApp.FileSpool->print(CLogFile::flagInfo,"  troppo grande, %u",len);
//				goto fine_errore;
//				}
		}
	else {
		m_Body->Seek(0,CFile::begin);
		lenSoFar=m_Body->GetLength();
		if(pBuf) {
			while(lenSoFar > 0) {
//				theApp.TCPIP_WD=0;			// se no salta!
				m_Body->Read(pBuf,A_CHUNK);
				if((n=Send(pBuf,min(lenSoFar,A_CHUNK))) < 0) {			// errore (tipo: troppo lungo)
					Close();									// quindi forzo chiusura!
					goto fine_errore;
					}
				lenSoFar -= n;
				m_Parent->sentBytes+=n;
				}
			goto fine;
			}
		else {
			theApp.FileSpool->print(CLogFile::flagInfo,"  NON abbastanza memoria, %u,%u, inFile=%u",A_CHUNK,lenSoFar,m_Body->isInFile);
			goto fine_errore;
			}
		}

fine_errore:
	retVal=0;

fine:
	delete []pBuf;
	return retVal;
	}

BOOL CWebSrvSocket2_base::ParseAuthorization(const CString& sField, CString& sUsername, CString& sPassword) {
	///*	da W3MFC di PJNaughter
	//For correct operation of the T2A macro, see MFC Tech Note 59
  USES_CONVERSION;

  int bSuccess = FALSE;

  //Make a local copy of the field we are going to parse
  char* pszField = T2A((LPTSTR) (LPCTSTR) sField);

  //Parse out the base64 encoded username and password 
  char seps[] = " ";
  char* pszToken = strtok(pszField, seps);
  if(pszToken && !_tcscmp(pszToken, "Basic")) {
    pszToken = strtok(NULL, seps);
    if(pszToken) {
      //Decode the base64 string passed to us
      CStringEx sInput(pszToken);
      if(sInput.Decode64()) {
        int nColon = sInput.Find(':');
        if(nColon != -1) {
          sUsername = sInput.Left(nColon);
          sPassword = sInput.Right(sInput.GetLength()-nColon-1);
          bSuccess = HTTP_AUTHORIZATION_BASIC; /*basic*/
					}
				}
			}
		}
	else {		// altri tipi...
		}

  return bSuccess;
	}

int CWebSrvSocket2_base::IsLocalPeer() const {

	return IsLocalAddress(m_Peer);
	}

char *CWebSrvSocket2_base::parseParm(const char *s,char *n1,UINT nl1,char *n2,UINT nl2, 
																		 int q,int m) {
	register int i;
	int j,inQuote=0;
	register char *p,*p1;
//gestire ? e & in apici? o no?

	*n1=*n2=0;
	if(!s)
		return NULL;
	if(_tcschr(s,'/'))						// mi posiziono al '?', se non ci sono gia'... (ossia se arrivo con un link completo
		if(p=_tcschr(s,'?'))
			s=p+1;												
	if(!*s)
		goto fine;
	if(!q)
		q=1;
	while(q) {
		*n2=0;
		if(p=_tcschr(s,'=')) {
			j=min(p-s,nl1-1);
			_tcsncpy(n1,s,j);
			n1[j]=0;
			if(p1=_tcschr(p,'&')) {
				j=min(p1-p-1,nl2-1);
				_tcsncpy(n2,p+1,j);
				n2[j]=0;
				s=p1+1;
				}
			else {
				_tcsncpy(n2,p+1,nl2-1);
				n2[nl2]=0;
				while(*s)
					s++;
//				s=NULL;
				}
			}
		else {
			*n1=0;
			break;
			}
		if(m) {			// se specificato...
			if(*n2)			// ...auto-evito i parm. vuoti...
				q--;
			}
		else
			q--;
		}
fine:
	p=compattaHex(n2);
	return p;
	}

int CWebSrvSocket2_base::parseNumParm(const char *s,char *parmN, UINT parmNL, char *parmV, UINT parmVL, int q,int autoSkip) {
	char *p;

	p=parseParm(s,parmN,parmNL,parmV,parmVL,q,autoSkip);
	return p ? atoi(p) : 0;
	}

char *CWebSrvSocket2_base::parseNamedParm(const char *s, const char *n, char *v, UINT vl) {
	int i;
	char parmN[256];

	for(i=1; ; i++) {
		CWebSrvSocket2_base::parseParm(s,parmN,255,v,vl,i,0);
		if(!*parmN) {
			*v=0;
			break;
			}
		if(!_tcsicmp(parmN,n)) {
			break;
			}
		}
	return v;

	}

char *CWebSrvSocket2_base::parseFormParm(const char *s, const char *s2, char *n1, char *v) {
	int i;
	char *p;

	if(!_tcsnicmp(s,CHTTPHeader::tagContentDisposition,20)) {
		if(p=(char *)stristr(s,"form-data;")) {
			p+=11;
			if(!_tcsnicmp(p,"name=",5)) {
				p+=5;
				if(*p=='\"')
					p++;
				_tcscpy(n1,p);
				if(n1[_tcslen(n1)-1]=='\"')
					n1[_tcslen(n1)-1]=0;
				}
			else
				_tcscpy(n1,p);
			if(s2 && v) {
				_tcscpy(v,s2);			// per ora così
				}
			return n1;
			}
		}
	return NULL;
	}

char *CWebSrvSocket2_base::compattaHex(char *s) {
	char *p=(char *)s,*s1=(char *)s;
	BYTE i;

	while(*s) {
		switch(*s) {
			case '+':			// rimuove i '+' usati dal CGI per indicare gli spazi...
				*p=' ';
				break;
			case '%':			// rimette a posto le conversioni dei browser...
				s++;
				if(*s) {
					if(isalnum(*s))
						*p=BCD2Hex(s);
					s++;
					}
				else
					s--;		// forza esci...
				break;
			default:
				*p=*s;
				break;
			}
		s++;
		p++;
		}
	*p=0;
	return s1;
	}

BYTE CWebSrvSocket2_base::BCD2Hex(const char *s) {
	BYTE i,n;

	n=*s-'0';
	if(n >= 10)
		n-=7;
	n <<= 4;
	s++;
	i=*s-'0';
	if(i >= 10)
		i-=7;
	n |= (i & 0xf);
	return n;
	}

const char *CWebSrvSocket2_base::stristr(const char *s1,const char *s2) {
	char s1_[1024],s2_[1024],*p;

	_tcsncpy(s1_,s1,1023);
	_tcslwr(s1_);
	_tcsncpy(s2_,s2,1023);
	_tcslwr(s2_);
	p=strstr(s1_,s2_);
	if(p)
		return p-s1_+s1;
	else
		return NULL;
	}

enum mimeTypes CWebSrvSocket2_base::setMimeType(const char *s) {
	enum mimeTypes n;

	if(_tcsstr(s,".htm") || _tcsstr(s,".html"))
		n=mimeTypeHtml;
	else if(_tcsstr(s,".txt"))
		n=mimeTypeText;
	else if(_tcsstr(s,".xml"))
		n=mimeTypeXml;
	else if(_tcsstr(s,".xhtml"))
		n=mimeTypeXhtml;
	else if(_tcsstr(s,".gif"))
		n=mimeTypeGif;
	else if(_tcsstr(s,".jpg"))
		n=mimeTypeJpeg;
	else if(_tcsstr(s,".png"))
		n=mimeTypePng;
	else if(_tcsstr(s,".wav"))
		n=mimeTypeWav;
	else if(_tcsstr(s,".mid"))
		n=mimeTypeMid;
	else if(_tcsstr(s,".tif"))
		n=mimeTypeTiff;
	else if(_tcsstr(s,".pdf"))
		n=mimeTypePdf;
	else if(_tcsstr(s,".mp3"))
		n=mimeTypeMp3;
	else if(_tcsstr(s,".avi") || _tcsstr(s,".divx"))
		n=mimeTypeAvi;
	else if(_tcsstr(s,".zip") || _tcsstr(s,".rar"))
		n=mimeTypeZip;
	else if(_tcsstr(s,".css"))
		n=mimeTypeCss;
	else if(_tcsstr(s,".js"))
		n=mimeTypeJs;
	else if(_tcsstr(s,".apk"))
		n=mimeTypeApk;
	// aggiungere per java/telefonini:
//.jad " text/vnd.sun.j2me.app-descriptor"
//.jar "application/java-archive" 	
	else
		n=mimeTypeUnknown;

	return n;
	}

char *CWebSrvSocket2_base::getMimeTypeDescr(int n) {

	switch(n) {
		case mimeTypeHtml:
			return "text/html";
			break;
		case mimeTypeXml:
			return "application/xml";
			break;
		case mimeTypeXhtml:
			return "application/xhtml+xml";
			break;
		case mimeTypeText:
			return "text/unspecified";
			break;
		case mimeTypeGif:
			return "image/gif";
			break;
		case mimeTypeJpeg:
			return "image/jpeg";
			break;
		case mimeTypePng:
			return "image/png";
			break;
		case mimeTypeTiff:
			return "image/tiff";
			break;
		case mimeTypePdf:
			return "application/pdf";
			break;
		case mimeTypeWbmp:
			return "image/vnd.wap.wbmp";
			break;
		case mimeTypeWav:
			return "audio/wav";
			break;
		case mimeTypeMid:
			return "audio/midi";
			break;
		case mimeTypeMp3:
			return "audio/x-mpeg";
			break;
		case mimeTypeAvi:
			return "video/x-msvideo";
			break;
		case mimeTypeZip:
			return "application/x-zip-compressed";
			break;
		case mimeTypeCss:
			return "text/css";
			break;
		case mimeTypeJs:
			return "application/x-javascript";
			break;
		case mimeTypeApk:
			return "application/vnd.android.package-archive";
			break;
		default:
			return "application/unspecified";
			break;
		}
	}



// *****************************************************************
CWebCliSocket::CWebCliSocket(const char *agent,int bProxy) : m_Agent(agent) {
	
	HTTPVer=MAKELONG(1,1);
	isSecure=FALSE;
	
	switch(bProxy) {
		case NO_PROXY:				// niente proxy
			m_ProxyMode=NO_PROXY;
			break;
		case ALWAYS_PROXY:		// usa proxy sempre
			m_ProxyMode=PROXY_BASE;
			break;
		case AUTO_PROXY:			// leggi da IE e comportati di conseguenza
			HKEY theRoot=HKEY_CLASSES_ROOT,pk,pksub;
			char myBuf[256];
			int n;
			DWORD v,vLen;

			if(!RegOpenKeyEx(theRoot,"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",0L,KEY_READ,&pk)) {
				n=RegQueryValueEx(pk,"AutoConfigUrl",0,NULL /*&vType*/,(BYTE *)&v,&vLen);
				n=RegQueryValueEx(pk,"ProxyEnable",0,NULL /*&vType*/,(BYTE *)&v,&vLen);
				n=RegQueryValueEx(pk,"ProxyServer",0,NULL /*&vType*/,(BYTE *)&v,&vLen);
				RegCloseKey(pk);
				}

			break;
		}

	// per capire se vanno usati dei proxy, leggere la chiave
	// HKEY_CURRENT_USER\S-1-5-21-507921405-492894223-839522115-500\Software\Microsoft\Windows\CurrentVersion\Internet Settings
	// e/o
	// HKEY_USERS\S-1-5-21-507921405-492894223-839522115-500\Software\Microsoft\Windows\CurrentVersion\Internet Settings
	// la chiave AutoConfigURL indica se è attivato il file di Configurazione automatica proxy
	// la chiave ProxyEnable e ProxyServer(:porta) indicano rispettivamente il tipo di proxy e se sì

	m_HTTPHeader=new CHTTPHeader;

	}

CWebCliSocket::~CWebCliSocket() {

	if(m_HTTPHeader)
		delete m_HTTPHeader;
	m_HTTPHeader=NULL;
	}

BOOL CWebCliSocket::Connect(LPCTSTR s,BOOL bSecure,WORD port) {

	if(CSocket::Create()) {
		isSecure=bSecure;
		if(bSecure)
			port=IPPORT_HTTPS /*443*/;
		return CSocket::Connect(s,port);
		}
	return FALSE;
	}

BOOL CWebCliSocket::Disconnect() {

	CSocket::Close();
	return TRUE;
	}

int CWebCliSocket::Send(CString S) {

	return CSocket::Send(S,S.GetLength());
	}

int CWebCliSocket::sendQuery(CString S) {

	if(Send(S))
		return Send(CWebSrvSocket2_base::CRLF);
	return 0;
	}

CString CWebCliSocket::buildQuery(CString Srv,CString S,int m,int bKeepAlive,int proxyMode) {
	char myBuf[512];
	CString S2;

	m_HTTPHeader->Reset();			// FINIRE!!
	S2=m ? "POST " : "GET ";
	switch(proxyMode) {
		case 0:
			break;
		case PROXY_BASE:
			S2+="http://";
			S2+=Srv;
			break;
		case PROXY_SOCKS4:
			break;
		}
	S2+=S;
	wsprintf(myBuf,"%s HTTP/%u.%u",(LPCTSTR)S2,HIWORD(HTTPVer),LOWORD(HTTPVer));
	m_HTTPHeader->AddToken(CHTTPHeader::TAG_NULL,myBuf);
	if(!m) {			// mah, direi
		m_HTTPHeader->AddToken(CHTTPHeader::TAG_ACCEPT,"*/*");
		m_HTTPHeader->AddToken(CHTTPHeader::TAG_ACCEPT_LANGUAGE,"it,en" /*siamo in un mondo anglo-americano :-) */);
		m_HTTPHeader->AddToken(CHTTPHeader::TAG_ACCEPT_ENCODING,"gzip, deflate");
		}


	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	char *p;
	switch(osvi.dwMajorVersion) {
		case 3:
			p="windows 3.x";
			break;
		case 4:
			switch(osvi.dwMinorVersion) {
				case 0:
					p="windows 95";
					break;
				case 10:
					p="windows 98";
					break;
				case 11:
					p="windows 98SE";
					break;
				default:
					p="windows 98";
					break;
				}
			break;
		case 5:
			switch(osvi.dwMinorVersion) {
				case 0:
					p="windows 2000";
					break;
				case 10:
					p="windows XP";
					break;
				default:
					p="windows XP+";
					break;
				}
			break;
		case 6:
			switch(osvi.dwMinorVersion) {
				case 0:		// mmmm cheffroci ;)
					p="windows Vista";
					break;
				case 1:		// 
					p="windows 7";
					break;
				}
			break;
		case 7:
//			p="windows 7";	// no... 6.1
			break;
		case 8:
			p="windows 8";
			break;
		case 9:
			p="windows 10";
			break;
		default:
			p="s.o. sconosciuto";
			break;
		}
	wsprintf(myBuf,"Mozilla/4.0 (compatible; %s; %s)",(LPCTSTR)m_Agent,p);
//	if(!m) 			// boh
		m_HTTPHeader->AddToken(CHTTPHeader::TAG_USERAGENT,myBuf);
	m_HTTPHeader->AddToken(CHTTPHeader::TAG_HOST,Srv);
	
	m_HTTPHeader->AddToken(CHTTPHeader::TAG_CONNECTION,bKeepAlive ?
		CHTTPHeader::tagKeepAlive : CHTTPHeader::tagClose);
//	S2+="Authorization: ...\r\n";		// finire/fare...


//	m_HTTPHeader->GetBuffer()+="0123456789";

//	AfxMessageBox(m_HTTPHeader->GetBuffer());

	return m_HTTPHeader->GetBuffer();
	}

void CWebCliSocket::addHeader(enum CHTTPHeader::HEADER_TAG n,const char *s,const char *s2) {
	
	if(m_HTTPHeader)
		m_HTTPHeader->AddToken(n,s,s2);
	}

int CWebCliSocket::authorize(const char *user,const char *pasw, int type) {
	char encUserPasw[256];
	CString S;

//isSecure

	switch(type) {
		case CWebSrvSocket2_base::HTTP_AUTHORIZATION_BASIC:
			S=user;
			S+=":";
			S+=pasw;
			CSMTPAttachment::EncodeBase64(S, S.GetLength(), encUserPasw, 255, NULL);
			S=encUserPasw;
			addHeader(CHTTPHeader::TAG_AUTHORIZATION,"Basic ",S);
			return CWebSrvSocket2_base::HTTP_AUTHORIZATION_BASIC;
			break;
		}
	return 0;
	}

int CWebCliSocket::readHeader(char *buf,DWORD len,WORD timeout) {
	int i,n,n2,retVal=-1;
	DWORD ti;
	char *myBuf,myBuf2[16],*p;

	n=0;
	myBuf=(char *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,MAX_HEADER_LEN);
	if(!myBuf)
		return -1;

	ti=timeGetTime()+timeout*1000;
	while(ti>timeGetTime()) {
		i=CAsyncSocket::Receive(myBuf2,1);		// HTTP/1.1 200 OK<CR><LF>
		if(i<0) {
			i=GetLastError();
			if(i != WSAEWOULDBLOCK)
				break;
			}
		else {
			if((n+i) >= MAX_HEADER_LEN) {
				break;
				}
			else {
				memcpy(myBuf+n,myBuf2,i);
				n+=i;
				myBuf[n]=0;
				if(strstr(myBuf,CWebSrvSocket2_base::CRLF2)) {
					retVal=0;
					if(p=(char *)CWebSrvSocket2_base::stristr(myBuf,CHTTPHeader::tagContentLength)) {
						retVal=atoi(p+15);
						}
					else
						retVal=1;			// bah non dovrebbe succedere ma tipo tvmovies.net...
					break;
					}
				}
			}
		}
	if(buf) {
		i=min(len,n);
		memcpy(buf,myBuf,i);
		buf[i]=0;
		}

	HeapFree(GetProcessHeap(),0,myBuf);

	return retVal;
	}

int CWebCliSocket::readPage(char *buf,DWORD len,WORD timeout) {
	int i,n,n2;
	DWORD ti;
	char *myBuf;

	n=0;
	myBuf=(char *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,len+1);
	if(!myBuf)
		return 0;

//isSecure

	ti=timeGetTime()+timeout*1000;
	while(ti>timeGetTime()) {
		n2=len-n;
		i=CAsyncSocket::Receive(myBuf,n2);		// HTTP/1.1 200 OK<CR><LF>
		if(i<0) {
			i=GetLastError();
			if(i != WSAEWOULDBLOCK)
				break;
			}
		else {
			memcpy(buf+n,myBuf,i);
			n+=i;
			if(n >= len) {
				break;
				}
			}
		}
	HeapFree(GetProcessHeap(),0,myBuf);
	buf[n]=0;

	return n;
	}

int CWebCliSocket::getBody(const char *inbuf,char *buf,DWORD len) {
	char *p,*p2;
	int myLen,i;

	if(p=(char *)CWebSrvSocket2_base::stristr(inbuf,"<body>")) {
		if(p2=(char *)CWebSrvSocket2_base::stristr(p+6,"</body>")) {
			myLen=p2-p-6;
			i=min(len,myLen);
			memcpy(buf,p+6,i);
			buf[i]=0;
			return min(len,myLen);
			}
		}

	return 0;
	}

char *CWebCliSocket::getXMLvalue(const char *inbuf,char *key,char *buf,DWORD len) {
	char *p,*p2;
	int myLen,i;
	char myBuf[256];

	wsprintf(myBuf,"<%s>",key);
	if(p=(char *)CWebSrvSocket2_base::stristr(inbuf,myBuf)) {
		wsprintf(myBuf,"</%s>",key);
		if(p2=(char *)CWebSrvSocket2_base::stristr(p+_tcslen(myBuf)-1,myBuf)) {
			i=_tcslen(myBuf)-1;
			myLen=p2-p-i;
			myLen=min(myLen,len);
			memcpy(buf,p+i,myLen);
			buf[myLen]=0;
			return buf;
			}
		}

	return NULL;
	}

double CWebCliSocket::getXMLvalue(const char *inbuf,char *key) {
	char *p,*p2;
	int myLen,i;
	char myBuf[256];

	wsprintf(myBuf,"<%s>",key);
	if(p=(char *)CWebSrvSocket2_base::stristr(inbuf,myBuf)) {
		wsprintf(myBuf,"</%s>",key);
		if(p2=(char *)CWebSrvSocket2_base::stristr(p+_tcslen(myBuf)-1,myBuf)) {
			i=_tcslen(myBuf)-1;
			myLen=p2-p-i;
			myLen=min(myLen,256);
			memcpy(myBuf,p+i,myLen);
			myBuf[myLen]=0;
			return atof(myBuf);
			}
		}

	return 0;
	}

int CWebCliSocket::getPage(const char *url,char *buf,DWORD len,BOOL doRedirect) {
	int i,n,status;
	CString s=url,srv,s2;
	char myBuf[1024];

	status=0;

	s=s.Mid(7);
	n=s.Find('/');
	srv=s.Left(n);
	if(Connect(srv)) {
		s2=buildQuery(srv,s.Mid(n));		// compreso slash iniziale!
//								AfxMessageBox(s2);
		i=Send((LPCTSTR)s2);
		if(theApp.debugMode)
			theApp.FileSpool->print(CLogFile::flagInfo,"  GET/HTTP send: %d",i);

		readHeader();

		Receive(myBuf,1000);		// HTTP/1.1 200 OK<CR><LF>
		parseResponse(myBuf);


		// FINIRE!!

		Receive(buf,len);		// contenuto
		if(theApp.debugMode)
			theApp.FileSpool->print(CLogFile::flagInfo,"  GET receive: %s",myBuf);
		Disconnect();
		}
	return status;
	}

int CWebCliSocket::parseResponse(char *buf) {
	int status;

	status=0;
	//getBody();
	// USARE, FINIRE

	return status;
	}

CString CWebCliSocket::translateString(const char *s) {
	CString S;

	while(*s) {
		switch(*s) {
			case ' ':
				S+='+';
				break;
			default:
				S+=*s;
				break;
			}
		s++;
		}
	return S;
	}




//------------------------------------------------------------------------------------
CStreamSrvSocket::CStreamSrvSocket(CDocument *p,enum STREAM_TYPE type) {

	maxConn=0;
	m_Parent=p;
	m_Type=type;
	flag1=flag2=0;
	tag=0;
	}

CStreamSrvSocket::~CStreamSrvSocket() {

	while(!cSockRoot.IsEmpty())
		delete cSockRoot.RemoveHead();

//		if(s->sock->m_hSocket != INVALID_SOCKET)
//			s->sock->Close();
	maxConn=0;
	}

void CStreamSrvSocket::OnAccept(int nErr) {
	int i,m;
	CString aIP;
	UINT aPort;
	CStreamSrvSocket2 *s;

//					AfxMessageBox("tenta connesso mp3");
	CString S;
	if(m_Parent->IsKindOf(RUNTIME_CLASS(CVidsendDoc2))) {
//		S.Format("%u %u",maxConn,((CVidsendDoc2 *)m_Parent)->maxConn);
		m=((CVidsendDoc2 *)m_Parent)->maxConn;
		}
	else {
//		S.Format("%u %u",maxConn,((CVidsendDoc22 *)m_Parent)->maxConn);
		m=((CVidsendDoc22 *)m_Parent)->maxConn;
		}
//	AfxMessageBox(S);

	if(maxConn < m) {		// questo potrebbe essere sbagliato: e' OK in ControlSrvSocket, ma qua si sommerebbero audio e video!	
																			// ...verificare!
		s=new CStreamSrvSocket2(this);
		if(s) {
			cSockRoot.AddTail(s);
			maxConn++;
	
			if(Accept(*s)) {
				s->status &= ~CStreamSrvSocket2::xOff;
				s->connectTime=CTime::GetCurrentTime();
				s->GetPeerName(aIP,aPort);
				if(theApp.theConnections)
//					theApp.theConnections->update();		//No... c'e' gia' nel ControlSocket...
					((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
				theApp.mandaALog((LPCTSTR)aIP,"Connesso client video");
//				if(theApp.debugMode) {			// mah per ora lo lascio per controllare ;)
					if(theApp.FileSpool)
						theApp.FileSpool->print(CLogFile::flagInfo2,"Connesso client video/audio da %s su porta %d, numConn=%d,maxConn=%d, s=%x",
							aIP,aPort,maxConn,m,s);
//					}

				if(getPort()==MP3_STREAM_SOCKET) {
					WORD id3v2_size;
					lame_global_flags *gf;		//
					gf=(lame_global_flags*)tag;

					s->Send("HTTP/1.0 200 OK\r\n",17);
					s->Send("Server: VideoSender 2.6\r\n",25);
					s->Send("Content-Type: audio/mpeg\r\n",26);
					S.Format("ice-audio-info: bitrate=%u;samplerate=%u;channels=%u\r\n",
						((CVidsendDoc22*)m_Parent)->myQA.mp3bitrate,44100,2);
					s->Send(S,S.GetLength());
					S.Format("icy-description: MP3 %u Kbps\r\n",((CVidsendDoc22*)m_Parent)->myQA.mp3bitrate);
					s->Send(S,S.GetLength());
//					s->Send("icy-name: RadioG\r\n",18);		// Personalizzare! 
					S.Format("icy-name: %s %s\r\n",theApp.infoUtente.cognome,theApp.infoUtente.nome);
					s->Send(S,S.GetLength());

					s->Send("Cache-Control: no-cache\r\n",24);
					s->Send("Pragma: no-cache\r\n",18);
					s->Send("\r\n",2);
					/*https://stackoverflow.com/questions/17952258/http-request-to-icecast-and-response
					icy-br: 192
ice-audio-info: bitrate=192;samplerate=44100;channels=2
icy-description: MP3 192 Kbps
icy-genre: *
icy-name: *
icy-pub: 1
icy-url: http://*
Server: Icecast 2.3.2-kh29
Cache-Control: no-cache
Expires: Mon, 26 Jul 1997 05:00:00 GMT
Pragma: no-cache*/


					if(!((CVidsendDoc22*)m_Parent)->suonoIn.IsEmpty()) {
						if(m_Parent->IsKindOf(RUNTIME_CLASS(CVidsendDoc2))) {
							sndPlaySound(((CVidsendDoc2*)m_Parent)->suonoIn,SND_ASYNC);
							}
						else {
							S.Format("%u %u",maxConn,((CVidsendDoc22 *)m_Parent)->maxConn);
							sndPlaySound(((CVidsendDoc22*)m_Parent)->suonoIn,SND_ASYNC);
							}
						}
//					AfxMessageBox("connesso mp3");

			    id3v2_size = lame_get_id3v2_tag(gf, 0, 0);
					if(id3v2_size > 0) {
						unsigned char *id3v2tag = (unsigned char *)malloc(id3v2_size);
						if(id3v2tag != 0) {
							//usare streamTitle del Doc
							size_t n_bytes = lame_get_id3v2_tag(gf, id3v2tag, id3v2_size);
							size_t written = s->Send(id3v2tag, n_bytes);
							free(id3v2tag);
							if(written != n_bytes) {
//								encoder_progress_end(gf);
//								error_printf("Error writing ID3v2 tag \n");
//								return 1;
								}
							}
						}
//			    imp3 = write_id3v1_tag(gf, outf);
//				  if(imp3) {
 //       return 1;
//						}
					}

				}
			else
				theApp.mandaALog(NULL,"Tentativo fallito di connessione client video/audio");
			return;
			}
		}
/*	{
	CStreamSrvSocket2 tempSock;
	Accept(tempSock);
	tempSock.Close();			// questo indicherebbe max conn. superate, ma c'e' gia' in ControlSocket
	}*/

	if(theApp.FileSpool)
		theApp.FileSpool->print(CLogFile::flagInfo,"impossibile Accept() stream, stroncato!");

	if(theApp.theConnections)
		((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
	// meglio Post Message perché siamo dentro Socket e specie se si fa IPlookup..
	}


void CStreamSrvSocket::doDelete(CStreamSrvSocket2 *ss) {
	CStreamSrvSocket2 *s;
	POSITION po;

	if(theApp.FileSpool)
		theApp.FileSpool->print(CLogFile::flagInfo,"doDelete stream, sockRoot=%x, ss=%x; maxConn(numConn) vale %d!",
		cSockRoot,ss,maxConn);

	po=cSockRoot.Find(ss);
	if(po) {
		s=cSockRoot.GetAt(po);
		delete s;
		cSockRoot.RemoveAt(po);
		}

	maxConn--;

	if(theApp.theConnections)
		((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
	}

int CStreamSrvSocket::Manda(const struct AV_PACKET_HDR *av,int n) {
	int i=TRUE;
	POSITION po;
//Manda del Socket Server Master si occupa di chiamare Manda per ciascun socket client connesso...

	po=cSockRoot.GetHeadPosition();
	if(po) {
		do {

			if(cSockRoot.GetNext(po)->Manda(av,n) <= 0)		// -1 se errore socket, -2 se rientrato, 0 se WOULDBLOCK
				i=FALSE;


			if(theApp.debugMode)
				theApp.FileSpool->print(CLogFile::flagInfo,"manda stream socket %x, i=%d",po,i);



			} while(po);
		}

//	else {		// se non ci sono client, libero io la memoria... (altrimenti lo fanno loro)
//		GlobalFree((void *)av->lpData);
		HeapFree(GetProcessHeap(),0,(void *)av);
//		}

	return i;
	// 0 se errore in spedizione (per tenerne conto nella preparazione dei frame)
	// 1 in caso di OK o anche se il client e' pieno (nessun problema dal punto di vista di chi prepara i frame)
	}

int CStreamSrvSocket::Manda(const void *lpData,int n) {
	int i=TRUE;
	POSITION po;
//Manda del Socket Server Master si occupa di chiamare Manda per ciascun socket client connesso...

  int iread, imp3, owrite, in_limit=0;
	BYTE mp3buffer[32768];		// diciamo 1sec @ 128Kbps DEVE STARCI TUTTO per subManda
	int err;

	lame_global_flags *gf;		// (mettere in socket?)
	gf=(lame_global_flags*)tag;
//	gf=lame_init();//idem
//	lame_init_params(gf);//idem
//	lame_init_bitstream(gf);

//    encoder_progress_begin(gf, inPath, outPath);

/*    id3v2_size = lame_get_id3v2_tag(gf, 0, 0);
    if(id3v2_size > 0) {
        unsigned char *id3v2tag = malloc(id3v2_size);
        if(id3v2tag != 0) {
            size_t  n_bytes = lame_get_id3v2_tag(gf, id3v2tag, id3v2_size);
            size_t  written = fwrite(id3v2tag, 1, n_bytes, outf);
            free(id3v2tag);
            if(written != n_bytes) {
                encoder_progress_end(gf);
                error_printf("Error writing ID3v2 tag \n");
                return 1;
            }
        }
			}
    else {
        unsigned char* id3v2tag = getOldTag(gf);
        id3v2_size = sizeOfOldTag(gf);
        if( id3v2_size > 0 ) {
            size_t owrite = fwrite(id3v2tag, 1, id3v2_size, outf);
            if(owrite != id3v2_size) {
                encoder_progress_end(gf);
                error_printf("Error writing ID3v2 tag \n");
                return 1;
            }
        }
			}
    if (global_writer.flush_write == 1) {
        fflush(outf);
			}*/

    /* do not feed more than in_limit PCM samples in one encode call
       otherwise the mp3buffer is likely too small
     */
    in_limit = lame_get_maximum_number_of_samples(gf,sizeof(mp3buffer));
    if(in_limit < 1)
      in_limit = 1;

    // encode until we hit eof 
//    do {
      // read in 'iread' samples 
//        iread = get_audio(gf, Buffer);
      iread = n /2/2;

      if(iread >= 0) {
        const short int *buffer = ((const short int*)lpData)+0;
        int rest = iread;
        do {
          int const chunk = rest < in_limit ? rest : in_limit;
//                encoder_progress(gf);

          /* encode */
          imp3 = lame_encode_buffer_interleaved(gf,buffer,chunk,mp3buffer,sizeof(mp3buffer));
          buffer += chunk;
          rest -= chunk;

          // was our output buffer big enough? 
          if(imp3 < 0) {
            if(imp3 == -1) {
          //      error_printf("mp3 buffer is not big enough... \n");
							}
            else {
          //      error_printf("mp3 internal error:  error code=%i\n", imp3);
							}
            return 0;
						}

//					if(!theApp.theServer2->bPaused) {
						po=cSockRoot.GetHeadPosition();
						if(po) {
							do {

								if(cSockRoot.GetNext(po)->Manda(mp3buffer,imp3) <= 0)		// -1 se errore socket, -2 se rientrato, 0 se WOULDBLOCK
									i=FALSE;

								if(theApp.debugMode)
									theApp.FileSpool->print(CLogFile::flagInfo,"manda mp3 socket %x, i=%d",po,i);

								} while(po);
							}
//						}

					err=GetLastError();

					try {
					if(theApp.theServer2 && theApp.theServer2->saveFile)
						theApp.theServer2->saveFile->Write(mp3buffer,imp3);
						}
					catch(...) {
						}

          } while(rest > 0 && !err);
				}
//			} while(iread > 0);

    if(1 /*nogap*/)
      imp3 = lame_encode_flush_nogap(gf, mp3buffer, sizeof(mp3buffer)); /* may return one more mp3 frame */
    else
      imp3 = lame_encode_flush(gf,mp3buffer,sizeof(mp3buffer)); /* may return one more mp3 frame */

    if(imp3 < 0) {
      if (imp3 == -1) {
//            error_printf("mp3 buffer is not big enough... \n");
				}
      else {
//            error_printf("mp3 internal error:  error code=%i\n", imp3);
				}
      return 0;
			}

//    encoder_progress_end(gf);

//			if(!theApp.theServer2->bPaused) {
				po=cSockRoot.GetHeadPosition();
				if(po) {
					do {

						if(cSockRoot.GetNext(po)->Manda(mp3buffer,imp3) <= 0)		// -1 se errore socket, -2 se rientrato, 0 se WOULDBLOCK
							i=FALSE;

						if(theApp.debugMode)
							theApp.FileSpool->print(CLogFile::flagInfo,"manda mp3 socket2 %x, i=%d",po,i);

						} while(po);
					}
//				}

			err=GetLastError();
			try {
			if(theApp.theServer2 && theApp.theServer2->saveFile)
				theApp.theServer2->saveFile->Write(mp3buffer,imp3);
				}
			catch(...) {
				}
//    imp3 = write_id3v1_tag(gf, outf);
 //   if(global_writer.flush_write == 1) {
 //       fflush(outf);
//			}
    if(imp3) {
 //       return 1;
			}
 //   write_xing_frame(gf, outf, id3v2_size);



	return i;
	// 0 se errore in spedizione (per tenerne conto nella preparazione dei frame)
	// 1 in caso di OK o anche se il client e' pieno (nessun problema dal punto di vista di chi prepara i frame)
	}

CStreamSrvSocket2::CStreamSrvSocket2(CStreamSrvSocket *p) {

	m_LineText=new CLineText;
	m_Parent=p;
	status=0;
	mandaBuf1=NULL;
	oldMandaBuf1=NULL;
	mandaBuf1size=0;
	myAvh=NULL;
	connectTime=0;
	priority=0;
	inSendTimeout=0;
	sentFrame=0;
	stops=0;
	skippedFrame=0;
	timeForPacket=0;
//	SetSockOpt(TCP_NODELAY)		/// puo' servire??
	}

CStreamSrvSocket2::~CStreamSrvSocket2() {

	delete m_LineText;
	}

void CStreamSrvSocket2::OnReceive(int nErr) {
	char myBuf[1024];
	int i;

	if(m_Parent->getPort()==MP3_STREAM_SOCKET) {
		*myBuf=0;
		if((i=Receive(myBuf,1000)) > 0) {
			myBuf[i]=0;
			}
		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"MP3 streaming: %s",myBuf);
		}
	else {
		if((i=Receive(myBuf,1)) > 0) {
			if(*myBuf) {
				stops++;
				status |= CStreamSrvSocket2::xOff;
				}
			else
				status &= ~CStreamSrvSocket2::xOff;
			}
		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"XOn: %x",*myBuf);
		}
//	p=(char *)GlobalAlloc(GPTR,1024);
//	wsprintf(p,"XOn"); 
//  theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);






	}

void CStreamSrvSocket2::OnClose(int nErr) {

	Close();
	m_Parent->doDelete(this);
//	delete this;
//	if(theApp.theConnections)		// meglio di no.. per problema di heartbeat che non va... 5/10/03
//		theApp.theConnections->update();
//		((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
	}

void CStreamSrvSocket2::OnSend(int nErr) {

	if(theApp.debugMode)
		theApp.FileSpool->print(CLogFile::flagInfo,"*** OnSend");
 	if(nErr) {
		if(theApp.FileSpool)
			theApp.FileSpool->print(CLogFile::flagError,"errore OnSend %x",nErr);
		// che fare??
		}
	subManda();
	}

int CStreamSrvSocket2::subManda() {
	int i,err;
	int retval;

//	ASSERT(0);
	if(mandaBuf1size) {			// prima deve passare l'header...
rifo1:
		DWORD ti=timeGetTime();

		i=Send(mandaBuf1,mandaBuf1size);
		ti=timeGetTime()-ti;
		timeForPacket+=ti;
		timeForPacket/=2;			// credo che si potrebbe fare un rapporto con il # byte inviati e
		err=GetLastError();
		if(theApp.debugMode)
			theApp.FileSpool->print(CLogFile::flagInfo,"**** mando header %x %u, info=%x : %x",mandaBuf1,mandaBuf1size,myAvh->info,i);
		if(i==mandaBuf1size) {
//			mandaBuf1=NULL;
			mandaBuf1size=0;
			retval=2;
			}
		else {
			if(i==SOCKET_ERROR) {
				if(theApp.debugMode)
					theApp.FileSpool->print(CLogFile::flagInfo,"*** SOCKET ERROR hdr: %u",i);
				status |= stalled | waitResync;
				if(err==WSAEWOULDBLOCK) {
					if(theApp.debugMode)
						theApp.FileSpool->print(CLogFile::flagInfo,"***   BLOCK");
//					m_Parent->flag1++;


					mandaBuf1size=0;


					retval=0;
//					goto fine;
					}
				else {
					if(theApp.debugMode)
						theApp.FileSpool->print(CLogFile::flagInfo,"***   ERRORE %u",err);
					mandaBuf1size=0;
					retval=-1;
					goto fine_clear;
					}
				}
			else {
				mandaBuf1+=i;
				mandaBuf1size-=i;
				goto rifo1;
				}
			}
		}
fine_clear:
	if(!mandaBuf1size) {			// a questo punto tutto e' passato e,...

		if(theApp.debugMode)
			theApp.FileSpool->print(CLogFile::flagInfo,"****** pulisco tutto %x %u %u %x; status=%x",
				oldMandaBuf1,m_Parent->flag1,m_Parent->flag2,mandaBuf1,status);

		if(oldMandaBuf1) {												// ...se i buffer non servono +...
			retval=0;
			if(m_Parent->flag1)
				m_Parent->flag1--;
			if(!m_Parent->flag1) {
//				GlobalFree((void *)oldMandaBuf1);
				oldMandaBuf1=mandaBuf1=NULL;
				}
			}
		status &= ~(inSend | stalled);
		}

	if(theApp.debugMode)
		theApp.FileSpool->print(CLogFile::flagInfo,"manda::server %x ",i);
fine:
//	DWORD ti=timeGetTime()+2000;
//	while(ti > timeGetTime());

	if(retval<0) {
		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"errore Send");
		}
	return retval;
	// ritorna 0 se e' rientrato in Manda (ossia Manda e' troppo lenta) o se ci sono errori di Send()
	// 1 se non manda in quanto il client e' pieno, 2> se OK
	}

int CStreamSrvSocket2::Manda(const struct AV_PACKET_HDR *av,int n) {
	int retval;
// Manda a ciascun socket collegato...

//ASSERT(0);
	if(status & inSend) {			// se sono rientrato...
		if(!inSendTimeout) {
				// watchdog su STALLED del video-client:
			inSendTimeout=timeGetTime()+10000;		//max 10 sec x un frame!
			}
		if(timeGetTime() < inSendTimeout) {
			status |= waitResync;		// ...non trasmetto questo frame e salto tutti fino al resync (KEYFRAME)
			skippedFrame++;
			retval=-2;
			goto fine;
			}
		else {

			if(theApp.debugMode)
				theApp.FileSpool->print(CLogFile::flagInfo,"...watchdog OnSend!");

			inSendTimeout=0;
			if(oldMandaBuf1) {												// pulisco tutto!
				HeapFree(GetProcessHeap(),0,(void *)oldMandaBuf1);
				oldMandaBuf1=mandaBuf1=NULL;
				}
			status &= ~inSend;
			}
		}
	if(!isXOn()) {						// se il client mi ha detto di fermarmi...
		status |= waitResync;		// ...idem
		skippedFrame++;
		retval=1;
		goto fine;
		}
	if(status & waitResync) {		// se questo è un KEYFRAME...
		if(av->info & AVIIF_KEYFRAME) {		// mi sblocco...
			status &= ~waitResync;


			// TOLGO XOFF... prova 27/11/03
//			status &= ~xOff;


			}
		else {
			skippedFrame++;
			retval=1;		// cambiare...
			goto fine;
			}
		}

	if(!mandaBuf1) {		// Qui mando per davvero!
		status |= inSend;								// imposto flag per Rientro...
		status &= ~stalled;							// non sono in stallo...
		oldMandaBuf1=mandaBuf1=(BYTE *)av;		// salvo i buffer da usare finche' la spedizione non e' completa...
		mandaBuf1size=n;			// ..e le dimensioni.
		myAvh=(struct AV_PACKET_HDR *)av;					// mi segno qual'e' il pacchetto corrente
		if(theApp.debugMode>2)
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"*** preparo pacchetti %x %u",av,n);
		sentFrame++;
		retval=subManda();
		}

	if(theApp.debugMode)
		theApp.FileSpool->print(CLogFile::flagInfo,"manda::server %x",retval);
fine:




//	if(retval)
	if(theApp.debugMode)
		if(theApp.FileSpool)
			theApp.FileSpool->print(CLogFile::flagInfo,"retval: %x, status %x, AV_INFO %x",retval,status,av->info);




//	DWORD ti=timeGetTime()+2000;
//	while(ti > timeGetTime());

	return retval;
	// ritorna 0 se e' rientrato in Manda (ossia Manda e' troppo lenta) o se ci sono errori di Send()
	// 1 se non manda in quanto il client e' pieno, 2> se OK
	}

int CStreamSrvSocket2::Manda(const void *lpData,DWORD len) {		// usata per MP3, 2023
	int err;
	DWORD ti=timeGetTime();

	Send(lpData,len);
	ti=timeGetTime()-ti;
	timeForPacket+=ti;
	timeForPacket/=2;			// credo che si potrebbe fare un rapporto con il # byte inviati e

	return 1;
	}




CStreamCliSocket::CStreamCliSocket(CWnd *hWnd) : theWnd(hWnd) {
	int i;

	m_LineText=new CLineText(0,NULL,0,264000);
	sBuffer=NULL; 
	anAvh=NULL;
	maxBuffers=lowBuffers=highBuffers=0;
	firstIn=lastOut=0;
	totBytesReceived=0;
	InitializeCriticalSection(&m_cs);
	receivedFrame=stops=skippedFrame=0;
	lastGoodFrame=0;
	}

CStreamCliSocket::~CStreamCliSocket() {

	DeleteCriticalSection(&m_cs);
	delete m_LineText;
	}

void CStreamCliSocket::initBuffers(int tb,DWORD bl,DWORD bh) {
	int i;
	
	if(sBuffer) {
		emptyBuffers();
		HeapFree(GetProcessHeap(),0,sBuffer);
		}
	maxBuffers=tb;
	lowBuffers=bl ? bl : (tb*2)/10;		// bah... con 3 VIDSEND.EXE si incricca!! in ricezione audio
//	lowBuffers=max(1,lowBuffers); //idem
	highBuffers=bh ? bh : (tb*8)/10;		// deve essere SEMPRE minore di maxBuffers, perche' ci sono [maxBuffers] buffer, numerati da 0 a maxBuffers-1... (v. addInBuffer)
//	highBuffers=max(1,highBuffers);// idem se metto buffers=0
	sBuffer=(const struct AV_PACKET_HDR **)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,sizeof(struct AV_PACKET_HDR *) 
		* (max(1 /*meglio*/,maxBuffers))); 
	for(i=0; i<maxBuffers; i++)
		sBuffer[i]=NULL;
	}


void CStreamCliSocket::addInBuffer(const struct AV_PACKET_HDR *avh) { 

	EnterCriticalSection(&m_cs);
	sBuffer[firstIn++]=avh;
	if(maxBuffers)
		firstIn %= maxBuffers;
	else
		firstIn=0;
//	if(theApp.debugMode)
//		theApp.FileSpool->print(CLogFile::flagInfo,"Buffers: FirstIn=%u,ok2out=%u",firstIn,ok2Out);
	LeaveCriticalSection(&m_cs);
	}

void CStreamCliSocket::bumpOutBuffer() { 

	EnterCriticalSection(&m_cs);
	if(sBuffer[lastOut]) {
		HeapFree(GetProcessHeap(),0,(void *)sBuffer[lastOut]->lpData);
		HeapFree(GetProcessHeap(),0,(void *)sBuffer[lastOut]); 
		sBuffer[lastOut]=NULL; 
		}
	lastOut++;
	if(maxBuffers)
		lastOut %= maxBuffers;
	else
		lastOut=0;
	LeaveCriticalSection(&m_cs);
	}

WORD CStreamCliSocket::totAvailBuffers() { 
	int n;
	
	if(maxBuffers<=1)
		return 1;
	n=firstIn-lastOut;
	if(n<0)
		n+=maxBuffers;
	return n;
	}

BOOL CStreamCliSocket::roomForBuffers() { 

	return totAvailBuffers()<maxBuffers;
	}

BOOL CStreamCliSocket::needMoreBuffers() { 
	int n=firstIn-lastOut; 

	if(n<=0) 
		n+=maxBuffers;	

	return n<=highBuffers /*(((highBuffers-lowBuffers)/2)+1)*/; 
	}



void CStreamCliSocket::emptyBuffers() {
	int i;

	for(i=0; i<maxBuffers; i++) {
		if(sBuffer[i]) {
			if(sBuffer[i]->lpData)
				HeapFree(GetProcessHeap(),0,sBuffer[i]->lpData);
			HeapFree(GetProcessHeap(),0,(void *)sBuffer[i]);
			}
		sBuffer[i]=NULL;
		}
	firstIn=lastOut=0;
	delay(1000);
	}

void CStreamCliSocket::clear() {
	int i;
	char myBuf[64];

	while((i=Receive(myBuf,64)) > 0) {
		}
	}

void CStreamCliSocket::delay(DWORD t) {
	DWORD l;
	MSG msg;

	Sleep(t);
/*	l=timeGetTime()+t;
	while(timeGetTime() < l) {
		if(::PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
			if(!theApp.PumpMessage()) { 
				}
			}
		} 2018 */
	}

void CStreamCliSocket::OnClose(int nErr) {
	
	Close();
//	theWnd->PostMessage(WM_COMMAND,ID_CONNESSIONE_DISCONNETTI,0); //non lo esegue xche' disabilitato!
	}


CStreamVCliSocket::CStreamVCliSocket(CWnd *hWnd,void *p,int numBuffers) :
	CStreamCliSocket(hWnd) {
//	int i;

	initBuffers(numBuffers);
	}

CStreamVCliSocket::~CStreamVCliSocket() {

	emptyBuffers();
	if(sBuffer) {
		HeapFree(GetProcessHeap(),0,sBuffer);
		sBuffer=NULL;
		}
	}

void CStreamVCliSocket::OnReceive(int nErr) {
	BYTE myBuf[4096];
	char *p;
//	BYTE *s;
	int i,n;
	DWORD len;
	//static struct AV_PACKET_HDR *avh;	// una variabile statica è condivisa da TUTTI gli oggetti istanziati!

  p=(char *)GlobalAlloc(GPTR,1024);

rifo:
	if(theApp.debugMode>1)
		*theApp.FileSpool << "onreceive video";
	if((i=Receive(myBuf,4000)) > 0) {			// receive consuma 1000 byte... ma SE ce ne sono altri??
																				// riavro' l'evento OnReceive... o no?
																				// v. sotto!
		totBytesReceived+=i;

		if(theApp.debugMode>2) {
			theApp.FileSpool->print(CLogFile::flagInfo,"il CliSocket %x riceve %d bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			this,i,(BYTE)myBuf[0],(BYTE)myBuf[1],(BYTE)myBuf[2],(BYTE)myBuf[3],(BYTE)myBuf[4],
			(BYTE)myBuf[5],(BYTE)myBuf[6],(BYTE)myBuf[7],(BYTE)myBuf[8],(BYTE)myBuf[9],
			(BYTE)myBuf[10],(BYTE)myBuf[11],(BYTE)myBuf[12],(BYTE)myBuf[13],(BYTE)myBuf[14],(BYTE)myBuf[15]);
			}

		m_LineText->handleReadData(myBuf,i);
		if(!anAvh) {
			if(m_LineText->ssize() >= AV_PACKET_HDR_SIZE) {
				n=AV_PACKET_HDR_SIZE;
				anAvh=(struct AV_PACKET_HDR *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,sizeof(struct AV_PACKET_HDR));
				m_LineText->readTextRaw((BYTE *)anAvh,&n);

#ifdef _STANDALONE_MODE
				if(anAvh->tag==MAKEFOURCC('D','G','2','0')) {
#else
				if(anAvh->tag==MAKEFOURCC('G','D','2','0')) {
#endif
//					((CVidsendView *)theWnd)->setTimer(1000/((CVidsendView *)theWnd)->streamInfo.fps);		// meglio qua...
					lastGoodFrame=timeGetTime()+frameTimeout;
					}
 				else {		// risincronizzarsi!
clearAll:
					if(theApp.debugMode>1) {
						char myBuf[128];
						wsprintf(myBuf,"  errore cli-frameV: anAvh->tag %x",anAvh->tag);
						*theApp.FileSpool << myBuf;
						}
					Send("\x01",1);		// fermo trasmettitore...
					m_LineText->clearAll();		// pulisco ricevitore...
					clear();
					emptyBuffers();				// distruggo buffer...
//					delay(2000);	// aspetto un po'... già in emptybuffers
					clear();
					m_LineText->clearAll();		// ri-pulisco tutto
					emptyBuffers();
					Send("\x00",1);			// il trasm. puo' ripartire.
					HeapFree(GetProcessHeap(),0,anAvh);
					anAvh=NULL;
					if(theApp.debugMode>1) {
						wsprintf(p,"attesa resync...");
						}
					((CVidsendView *)theWnd)->waitForKeyFrame=1;	// boh... un'idea per risincronizzare BENE se per caso ho perso dei pacchetti ed evitare quadrettoni...
					goto fine;
					}
				}
			if(m_LineText->ssize())		// serve quando tutto il pacchetto (header+dati) era < 1000 byte!
				goto cealtro;
			}
		else {
cealtro:
			if(m_LineText->ssize() >= anAvh->len) {
				n=anAvh->len;
				anAvh->lpData=m_LineText->readTextRaw(NULL,&n);

				if(anAvh->type == AV_PACKET_TYPE_VIDEO);			// controllare??

				if(theApp.debugMode>2)
					wsprintf(p,"ricevuto bmp, len: %d",anAvh->len);
				addInBuffer(anAvh);
				receivedFrame++;
//				if(!theApp.theCtrl->bPaused) {
					if(roomForBuffers()) {
						Send("\x00",1);
						if(theApp.debugMode)
							*theApp.FileSpool << "client: mando Xon";
						}
					else {
						stops++;
						Send("\x01",1);
						if(theApp.debugMode)
							*theApp.FileSpool << "client: mando Xoff";
						((CVidsendView *)theWnd)->waitForKeyFrame=1;	// boh... un'idea per risincronizzare BENE se per caso ho perso dei pacchetti ed evitare quadrettoni...
						}
					if(theApp.debugMode>1) {
						char myBuf[128];
						wsprintf(myBuf,"ricevuto cli-frameV: time %u, frame=%x,keyframe %u, firstIn=%u,lastOut=%u,maxBuffers=%u,Room %u ",timeGetTime(),sBuffer[firstIn ? firstIn-1 : maxBuffers-1],anAvh->info,firstIn,lastOut,highBuffers,roomForBuffers());
						*theApp.FileSpool << myBuf;
						}
				anAvh=NULL;
				}
			else {
				if(lastGoodFrame<timeGetTime()) {
					goto clearAll;
					}
				else {
					if(theApp.debugMode>1)
						wsprintf(p,"attesa dati...");
					}
				}
			}

//		if((i=CAsyncSocket::Receive(myBuf,4000,MSG_PEEK)) > 0)
//			goto rifo;
		}
//	wsprintf(p,"stato %d",state);
fine:
	if(*p) {
		if(theApp.m_pMainWnd) 
			theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
		else
			GlobalFree(p);
		}
	else
		GlobalFree(p);
	}


CStreamACliSocket::CStreamACliSocket(CWnd *hWnd,HWAVEOUT *hWO,int numBuffers) :
	CStreamCliSocket(hWnd) {

	initBuffers(numBuffers);
	hWaveOut=hWO;
	receivedSample=0;
// no					waveOutRestart(*hWaveOut);
	}

CStreamACliSocket::~CStreamACliSocket() {

	emptyBuffers();
	if(sBuffer) {
		HeapFree(GetProcessHeap(),0,sBuffer);
		sBuffer=NULL;
		}
	}

void CStreamACliSocket::OnReceive(int nErr) {
	BYTE myBuf[1024];
	char *p;
//	BYTE *s;
	int i,n;
	DWORD len;
	//static struct AV_PACKET_HDR *avh;	// una variabile statica è condivisa da TUTTI gli oggetti istanziati!

  p=(char *)GlobalAlloc(GPTR,1024);
	if((i=Receive(myBuf,1000)) > 0) {

rifo:
		totBytesReceived+=i;

		m_LineText->handleReadData(myBuf,i);
		if(!anAvh) {
			if(m_LineText->ssize() >= AV_PACKET_HDR_SIZE) {
				n=AV_PACKET_HDR_SIZE;
				anAvh=(struct AV_PACKET_HDR *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,sizeof(struct AV_PACKET_HDR));
				m_LineText->readTextRaw((BYTE *)anAvh,&n);
//				ASSERT(avh->len<32000);
#ifdef _STANDALONE_MODE
				if(anAvh->tag==MAKEFOURCC('D','G','2','0')) {
#else
				if(anAvh->tag==MAKEFOURCC('G','D','2','0')) {
#endif
//					wsprintf(p,"state0 = %d",state);
					lastGoodFrame=timeGetTime()+frameTimeout;
					}
 				else {		// risincronizzarsi!
clearAll:
					Send("\x01",1);		// fermo trasmettitore...
					m_LineText->clearAll();		// pulisco ricevitore...
					clear();
					emptyBuffers();				// distruggo buffer...
//					delay(2000);	// aspetto un po'... già in emptybuffers
					clear();
					m_LineText->clearAll();		// ri-pulisco tutto
					emptyBuffers();
					Send("\x00",1);			// il trasm. puo' ripartire.
					HeapFree(GetProcessHeap(),0,anAvh);
					anAvh=NULL;
					if(theApp.debugMode>1) {
						wsprintf(p,"attesa resync...");
						}
					goto fine;
					}
				}
			if(m_LineText->ssize())		// serve quando tutto il pacchetto (header+dati) era < 1000 byte!
				goto cealtro;
			}
		else {
cealtro:
			if(m_LineText->ssize() >= anAvh->len) {
				n=anAvh->len;
				anAvh->lpData=m_LineText->readTextRaw(NULL,&n);

				if(anAvh->type == AV_PACKET_TYPE_AUDIO) ;			// controllare??

				addInBuffer(anAvh);
				receivedSample++;
//				if(!theApp.theCtrl->bPaused) {
					if(roomForBuffers()) {
						Send("\x00",1);
						if(theApp.debugMode)
							*theApp.FileSpool << "client: mando Xon";
						}
					else {
						stops++;
						Send("\x01",1);
						if(theApp.debugMode)
							*theApp.FileSpool << "client: mando Xoff";
						((CVidsendView *)theWnd)->waitForKeyFrame=1;	// boh... un'idea per risincronizzare BENE se per caso ho perso dei pacchetti ed evitare quadrettoni...
						}

//				if(theWnd) {
//					((CVidsendView *)theWnd)->playSample(anAvh);
//					n=getBytesReceived();
//					wsprintf(p,"riproduzione in corso (@ %d.%d KB/s) (b:%d - %d)...",n/2000,(n % 2000)/200,totAvailBuffers(),needMoreBuffers());
//					if(((CChildFrame *)theWnd->GetParent())->m_wndStatusBar.m_hWnd)
//						((CChildFrame *)theWnd->GetParent())->m_wndStatusBar.SetPaneText(0,p,TRUE);
					// in realta' per ora non funziona... bisogna usare i buffer anche qua...
//					}
				if(theApp.debugMode>1)
					wsprintf(p,"ricevuti dati audio");
//				if(totBuffers>highBuffers)
// CHE SENSO HA ?? 2021					bah, ma senza non va... ah c'era un Pause in View... ma non saprei dire perché. tolto cmq!
//					waveOutRestart(*hWaveOut);
//				Send("\x01",1);
				anAvh=NULL;
				}
			else {
				if(lastGoodFrame<timeGetTime()) {
					goto clearAll;
					}
				else {
					if(theApp.debugMode>1) {
						wsprintf(p,"attesa dati...");
						}
					}
				}
			}

//		if((i=CAsyncSocket::Receive(myBuf,1000,MSG_PEEK)) > 0)
//2018			goto rifo;
		}
//	wsprintf(p,"stato %d",state);
fine:
	if(*p) {
	  if(theApp.m_pMainWnd)
			theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);
		else
			GlobalFree(p);
		}
	else
		GlobalFree(p);
	}



CControlSrvSocket2::CControlSrvSocket2(CControlSrvSocket *p) {

	m_LineText=new CLineText;
	m_Parent=p;
	packetType=0;
	reInit();
	}

CControlSrvSocket2::~CControlSrvSocket2() {

	delete m_LineText;
	}

void CControlSrvSocket2::OnReceive(int nErr) {
	BYTE myBuf[256];
	int i;

//rifo:
	if(theApp.debugMode>1)
		theApp.FileSpool->print(CLogFile::flagInfo,"ricevuto ControlSocket %u",packetType);

	switch(packetType) {
		case 0:
			Receive(&packetType,1);
//			if(CAsyncSocket::Receive(myBuf,127,MSG_PEEK) > 0)
//2018				goto rifo;
			break;
		case 1:				// dati di riconoscimento del client
			if((i=Receive(myBuf,127)) > 0) {
				m_LineText->handleReadData(myBuf,i);
				if(m_LineText->ssize() >= 127) {
					connName=myBuf;		// passo questi dati, ma solo per averli (NON per log utenti/autenticazione!)
					// password=myBuf+32... mi serve?
					appName=myBuf+64;
					numLogConn=*(DWORD *)(myBuf+96);
					timedConn=*(CTimeSpan *)(myBuf+100);
					startConn=CTime::GetCurrentTime();
					}
				packetType=0;
				}
			break;
		case 2:				// controllo remoto telecamera....
			break;
		case 11:		// heartBeat
			if((i=Receive(myBuf,1)) > 0) {		// 2 byte, ma il contenuto non mi interessa...
				cliTimeOut=timeGetTime();

				if(theApp.debugMode) {
					if(theApp.FileSpool) {
						CString aIP;
						UINT aPort;
						GetPeerName(aIP,aPort);
						theApp.FileSpool->print(CLogFile::flagInfo,"ricevuto HeartBeat da %s",(LPCTSTR)aIP);
						}
					}

				packetType=0;
				}
			break;
		default:
			Receive(myBuf,255);
			break;
		}

	}

void CControlSrvSocket2::OnClose(int nErr) {

	doClose();
	}

int CControlSrvSocket2::checkUtenti() {

	if(timedConn > 0) {
		if((startConn+timedConn) < CTime::GetCurrentTime())
			return 1;
		}
	else if(timedConn < 0) {			// messo <= 24/9/03, ri-tolto ?? 5/10/03
				//gestire meglio int in tabella utenti vs. CTime "ct" (v. vidsend.cpp, UtenteCheck)
		timedConn=0;
		return 1;
		}
	return 0;			
	}

void CControlSrvSocket2::doClose() {

	Close();
	m_Parent->doDelete(this);
//	delete this;
	}

void CControlSrvSocket2::reInit() {

	numLogConn=0;
	startConn=0;
	timedConn=0;
	cliTimeOut=timeGetTime();
	}


CControlSrvSocket::CControlSrvSocket(CDocument *p) {

	maxConn=0;
	m_Parent=p;
	}

CControlSrvSocket::~CControlSrvSocket() {

	while(!cSockRoot.IsEmpty())
		delete cSockRoot.RemoveHead();

//		if(s->sock->m_hSocket != INVALID_SOCKET)
//			s->sock->Close();
	maxConn=0;
	}

void CControlSrvSocket::OnAccept(int nErr) {
	int i;
	CString aIP;
	UINT aPort;
	CControlSrvSocket2 *s;

	if(maxConn< ((CVidsendDoc2 *)m_Parent)->maxConn) {
			s=new CControlSrvSocket2(this);
			if(s) {
				cSockRoot.AddTail(s);
				maxConn++;
		
				if(Accept(*s)) {
					s->GetPeerName(aIP,aPort);
					if(((CVidsendDoc2 *)m_Parent)->acceptConnect(aIP)) {
						struct STREAM_INFO *si=((CVidsendDoc2 *)m_Parent)->getConnectionInfo();
						if(si) {
							if(s->Send("\x1",1) != SOCKET_ERROR)
								s->Send(si,sizeof(struct STREAM_INFO));
							delete si;
		//					if() 
		//					cSock[i].Send("\x2",1);
		//					cSock[i].Send(,255);
							}
						}
					else {
						theApp.mandaALog(NULL,"Tentativo di connessione client ctrl RESPINTO");
						s->doClose();
						}
					if(theApp.theConnections)
						((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
					theApp.mandaALog((LPCTSTR)aIP,"Connesso client ctrl");
					if(theApp.debugMode) {
						if(theApp.FileSpool)
							theApp.FileSpool->print(CLogFile::flagInfo2,"Connesso client ctrl da %s",aIP);
						}
					}
				else
					theApp.mandaALog(NULL,"Tentativo fallito di connessione client ctrl");
				return;
				}
		}
	{
	CControlSrvSocket2 tempSock;
	Accept(tempSock);
	tempSock.Close();			// cosi' indica al client (WWW) che non puo' essere accettato per superamento conn. libere!
	}
	
	if(theApp.FileSpool)
		theApp.FileSpool->print(2,"impossibile Accept() control stream, stroncato!");

	if(theApp.theConnections)
		((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
	}

void CControlSrvSocket::OnClose(int nErr) {

	Close();
	}

void CControlSrvSocket::doDelete(CControlSrvSocket2 *ss) {
	CControlSrvSocket2 *s;

	if(!((CVidsendDoc2 *)m_Parent)->suonoOut.IsEmpty()) {
		sndPlaySound(((CVidsendDoc2 *)m_Parent)->suonoOut,SND_ASYNC);
		}
	POSITION po;

	po=cSockRoot.Find(ss);
	if(po) {
		s=cSockRoot.GetAt(po);
		delete s;
		cSockRoot.RemoveAt(po);
		}

	maxConn--;

	if(theApp.theConnections)
		((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);

	}

void CControlSrvSocket::checkUtenti() {
	CControlSrvSocket2 *myRoot;
	POSITION po,po1;

	po=cSockRoot.GetHeadPosition();
	if(po) {
		do {
			po1=po;
			myRoot=cSockRoot.GetNext(po);
			if(myRoot->checkUtenti()) {
				myRoot->Close();
				doDelete(myRoot);
				}
			} while(po);
		}

	}


CControlCliSocket::CControlCliSocket(CWnd *hWnd) {

	m_LineText=new CLineText;
	theWnd=hWnd;
	packetType=0;
	}

CControlCliSocket::~CControlCliSocket() {

	delete m_LineText;
	}

void CControlCliSocket::OnReceive(int nErr) {
	BYTE myBuf[1024],*p;
	int i,n;
	CString aIP;
	UINT aPort;

rifo:
	switch(packetType) {
		case 0:
			Receive(&packetType,1);
//			if(CAsyncSocket::Receive(myBuf,256,MSG_PEEK) > 0)
//2018				goto rifo;
			break;
		case 1:
			if((i=Receive(myBuf,sizeof(struct STREAM_INFO))) > 0) {
				m_LineText->handleReadData(myBuf,i);
				if(m_LineText->ssize() >= sizeof(struct STREAM_INFO)) {
					n=sizeof(struct STREAM_INFO);
					m_LineText->readTextRaw((BYTE *)&(((CVidsendView *)theWnd)->streamInfo),&n);
					if(((CVidsendView *)theWnd)->streamInfo.versione <= VIDSEND_VERSIONE) {
						GetPeerName(aIP,aPort);
						if(!((CVidsendView *)theWnd)->doConnect((char *)(LPCTSTR)aIP,&((CVidsendView *)theWnd)->streamInfo)) {
							((CVidsendView *)theWnd)->endConnect();
							Close();
							}
						}
					else {
						Close();
						}
					packetType=0;
					}
				}
			break;
		case 2:
			if((i=Receive(myBuf,1000)) > 0) {
				m_LineText->handleReadData(myBuf,i);
				if(m_LineText->ssize() >= 255) {
					n=255;
					m_LineText->readTextRaw((BYTE *)myBuf,&n);
					AfxMessageBox((char *)myBuf,MB_ICONEXCLAMATION);
					packetType=0;
					}
				}
			break;
		default:
			Receive(myBuf,1023);
			break;
		}
	}

void CControlCliSocket::OnClose(int nErr) {
	
	Close();
	((CVidsendView *)theWnd)->endConnect();		// questo forza la sconnessione dei client Stream e anche dell'Auth (eventuale)
	AfxMessageBox("Connessione terminata");
//	theWnd->PostMessage(WM_COMMAND,ID_CONNESSIONE_DISCONNETTI,0); //non lo esegue xche' disabilitato!
	}


int CControlCliSocket::SendUserPass(const char *n,const char *p,DWORD x,CTimeSpan t) {
	char myBuf[128];

	*myBuf=1;
	_tcsncpy(myBuf+1,n,31);
	myBuf[32]=0;
	theApp.Scramble(myBuf+33,p,_tcslen(p),0);			// qui non c'e' SerNum... per ora?
	_tcscpy(myBuf+65,"VideoSender");
	myBuf[96]=0;
	*(DWORD *)(myBuf+97)=x;
	*(CTimeSpan *)(myBuf+101)=t;
	return Send(myBuf,128);
	}



// VBAN 2021 --------------------------------------------------------------------------------------------------
CVBANSocket::CVBANSocket(CDocument *p,UINT port) : m_Port(port) {
	m_Parent=p;
	bytesPerFrame=256;
	
  int optVal = 10;

#define IP_MULTICAST_TTL 3        /* u_char; set/get IP multicast ttl */ /* forse in <netinet/in.h> */
	int i = SetSockOpt(IP_MULTICAST_TTL,(void *)&optVal,sizeof(int));

	config.port=VBAN_PORT;
	config.direction=(enum ::socket_direction)SOCKET_OUT;
	ZeroMemory(config.ip_address,sizeof(config.ip_address));
	}

CVBANSocket::~CVBANSocket() {
	}

const char *CVBANSocket::bit_fmt_names [VBAN_BIT_RESOLUTION_MAX] = {
  "8I", "16I", "24I", "32I", "32F", "64F", "12I", "10I"
	};

int CVBANSocket::Open(LPSTR address) {
	int i;
	char optflag = 1;

	if(address)
		_tcscpy(config.ip_address,address);
	i=Create(m_Port,SOCK_DGRAM);
	if(!address || IsBroadcastAddress(address))
		int ret = SetSockOpt(SO_BROADCAST, &optflag, sizeof(optflag));
	return i;
	}

/** Return VBanHeader pointer from buffer */
#define PACKET_HEADER_PTR(_buffer) ((struct VBanHeader *)_buffer)
/** Return payload pointer of a Vban packet from buffer pointer */
#define PACKET_PAYLOAD_PTR(_buffer) ((char *)(PACKET_HEADER_PTR(_buffer)+1))
/** Return payload size from total packet size */
#define PACKET_PAYLOAD_SIZE(_size) (_size - sizeof(struct VBanHeader))

int CVBANSocket::Ping() {
  struct VBanHeader *hdr = PACKET_HEADER_PTR(databuf);

  hdr->vban       = VBAN_HEADER_FOURC;
	hdr->format_SR  = VBAN_PROTOCOL_SERVICE;
  hdr->service_type = VBAN_SERVICE_IDENTIFICATION;
  hdr->service = VBAN_SERVICE_FNCT_PING0;
  hdr->reserved = 0;
  _tcsncpy(hdr->streamname, "PING", 4);
  hdr->nuFrame    = 0;
	return SendTo(databuf, sizeof(struct VBanHeader), config.port, *config.ip_address ? config.ip_address : NULL,0);
	}

int CVBANSocket::PacketSetNewContent(size_t payload_size) {
  struct VBanHeader *hdr = PACKET_HEADER_PTR(databuf);

  if(databuf == 0)    {
//      logger_log(LOG_FATAL, "%s: null argument", __func__);
    return -EINVAL;
	  }
  hdr->format_nbs = (((payload_size / ((hdr->format_nbc+1) * 
		VBanBitResolutionSize[(hdr->format_bit & VBAN_BIT_RESOLUTION_MASK)])) ) / 4 ) -1;
  ++hdr->nuFrame;

  return 0;
	}

void CVBANSocket::PacketSetNewSeq(DWORD seq) {
  struct VBanHeader *hdr = PACKET_HEADER_PTR(databuf);
  hdr->nuFrame=seq;
	}

/** should better be in vban.h header ?*/
size_t CVBANSocket::vban_sr_from_value(unsigned int value) {
  size_t index = 0;

  while((index < VBAN_SR_MAXNUMBER) && (value != VBanSRList[index])) {
    ++index;
    }

  return index;
	}

int CVBANSocket::PacketCheck(const char *streamname, const BYTE *buffer, size_t size) {
  struct VBanHeader const* const hdr = PACKET_HEADER_PTR(buffer);
  enum VBanProtocol protocol = VBAN_PROTOCOL_UNDEFINED_4;
  enum VBanCodec codec = VBAN_CODEC_UNDEFINED_14;

  if((streamname == 0) || (buffer == 0)) {
//        logger_log(LOG_FATAL, "%s: null pointer argument", __func__);
    return -EINVAL;
    }

  if(size <= VBAN_HEADER_SIZE) {
 //       logger_log(LOG_WARNING, "%s: packet too small", __func__);
    return -EINVAL;
    }

    if (hdr->vban != VBAN_HEADER_FOURC)    {
 //       logger_log(LOG_WARNING, "%s: invalid vban magic fourc", __func__);
    return -EINVAL;
    }

  if(_tcsncmp(streamname, hdr->streamname, VBAN_STREAM_NAME_SIZE))    {
 //       logger_log(LOG_DEBUG, "%s: different streamname", __func__);
    return -EINVAL;
    }

    /** check the reserved bit : it must be 0 */
  if(hdr->format_bit & VBAN_RESERVED_MASK)    {
 //       logger_log(LOG_WARNING, "%s: reserved format bit invalid value", __func__);
    return -EINVAL;
    }

  /** check protocol and codec */
  protocol        = (enum VBanProtocol)(hdr->format_SR & VBAN_PROTOCOL_MASK);
  codec           = (enum VBanCodec )(hdr->format_bit & VBAN_CODEC_MASK);

  switch (protocol) {
    case VBAN_PROTOCOL_AUDIO:
      return (codec == VBAN_CODEC_PCM) ? PacketPcmCheck(buffer, size) : -EINVAL;

    case VBAN_PROTOCOL_SERIAL:
    case VBAN_PROTOCOL_TXT:
    case VBAN_PROTOCOL_UNDEFINED_1:
    case VBAN_PROTOCOL_VIDEO:
    case VBAN_PROTOCOL_UNDEFINED_3:
    case VBAN_PROTOCOL_UNDEFINED_4:
      /** not supported yet */
      return -EINVAL;

    default:
  //    logger_log(LOG_ERROR, "%s: packet with unknown protocol", __func__);
      return -EINVAL;
    }
	}

int CVBANSocket::PacketPcmCheck(const BYTE *buffer, size_t size) {

    /** the packet is already a valid vban packet and buffer already checked before */

  struct VBanHeader const* const hdr = PACKET_HEADER_PTR(buffer);
  enum VBanBitResolution const bit_resolution = (enum VBanBitResolution)(hdr->format_bit & VBAN_BIT_RESOLUTION_MASK);
  int const sample_rate   = hdr->format_SR & VBAN_SR_MASK;
  int const nb_samples    = (hdr->format_nbs + 1) * 4;
  int const nb_channels   = hdr->format_nbc + 1;
  size_t sample_size      = 0;
  size_t payload_size     = 0;

//  logger_log(LOG_DEBUG, "%s: packet is vban: %u, sr: %d, nbs: %d, nbc: %d, bit: %d, name: %s, nu: %u",
//      __func__, hdr->vban, hdr->format_SR, hdr->format_nbs, hdr->format_nbc, hdr->format_bit, hdr->streamname, hdr->nuFrame);

  if(bit_resolution >= VBAN_BIT_RESOLUTION_MAX) {
  //      logger_log(LOG_WARNING, "%s: invalid bit resolution", __func__);
    return -EINVAL;
    }

  if(sample_rate >= VBAN_SR_MAXNUMBER) {
   //     logger_log(LOG_WARNING, "%s: invalid sample rate", __func__);
    return -EINVAL;
    }

  sample_size = VBanBitResolutionSize[bit_resolution];
  payload_size = nb_samples * sample_size * nb_channels;

  if(payload_size != (size - VBAN_HEADER_SIZE)) {
   //     logger_log(LOG_WARNING, "%s: invalid payload size, expected %d, got %d", __func__, payload_size, (size - VBAN_HEADER_SIZE));
    return -EINVAL;
    }
    
  return 0;
	}

int CVBANSocket::PacketInitHeader(const struct stream_config_t *stream_config, const char *streamname) {
  struct VBanHeader *hdr = PACKET_HEADER_PTR(databuf);

  if((databuf == 0) || (stream_config == 0))    {
//      logger_log(LOG_FATAL, "%s: null argument", __func__);
    return -EINVAL;
		}

  hdr->vban       = VBAN_HEADER_FOURC;
  hdr->format_nbc = stream_config->nb_channels - 1;
  hdr->format_SR  = vban_sr_from_value(stream_config->sample_rate);
  hdr->format_bit = stream_config->bit_fmt;
  _tcsncpy(hdr->streamname, streamname, VBAN_STREAM_NAME_SIZE-1);
  hdr->nuFrame    = 0;

  return 0;
	}


int CVBANSocket::Manda(const void *lpData,DWORD len) {
  int ret = 0;
  struct sockaddr_in si_other;
  int slen = sizeof(si_other);
	static DWORD frameCnt=0;

//    logger_log(LOG_DEBUG, "%s invoked", __func__);

/*
  if(m_hSocket == 0)    {
//        logger_log(LOG_ERROR, "%s: socket is not open", __func__);
    return -ENODEV;
		}

  ZeroMemory(&si_other, sizeof(si_other));		// non la uso cmq... TOGLIERE POI
  si_other.sin_family        = AF_INET;
  si_other.sin_port          = htons(config.port);
  si_other.sin_addr.s_addr   = inet_addr(config.ip_address);
*/

// messo dopo costruzione, di là!	stream_config.bit_fmt=VBAN_BITFMT_8_INT;
//	stream_config.nb_channels=1 /* theTV->wfd.wf.numChannels */;
//	stream_config.sample_rate=8000 /* theTV->wfd.wf.nSamplesPerSec */ ;
//	PacketInitHeader(buffer, &stream_config, "Liverpool's GD");

//	BYTE myBuf[32000];
//	memcpy(myBuf,lpData,len);		// perché credo che il buffer venga deallocato di là!

	int chunkSize,chunk;
	BYTE *ptr=(BYTE *)lpData /*myBuf*/;
	struct VBanHeader *hdr=(struct VBanHeader *)databuf;
	bytesPerFrame=(VBAN_SAMPLES_MAX_NB * 
		(theApp.theServer->theTV->wfd.wf.nChannels * theApp.theServer->theTV->wfd.wf.wBitsPerSample))/8;
// was: max 256 sample per packet...
	chunkSize=min(bytesPerFrame,VBAN_DATA_MAX_SIZE);
	PacketSetNewContent(chunkSize);
  chunk=((((DWORD)hdr->format_nbs) + 1) * (((hdr->format_nbc+1) * 
		VBanBitResolutionSize[(hdr->format_bit & VBAN_BIT_RESOLUTION_MASK)])) ) *4;			// per i resti della divisione/4..
	if(chunk != chunkSize) {
		chunkSize=chunk;
		PacketSetNewContent(chunkSize);
		}

	do {
		chunk=min(chunkSize,len);
		if(chunk != chunkSize)
			PacketSetNewContent(chunk);
		frameCnt++;
		PacketSetNewSeq(frameCnt);
		memcpy(databuf+sizeof(struct VBanHeader),ptr,chunk);

		ptr+=chunk;

//		struct VBanHeader *avh=(struct VBanHeader *)databuf;
		ret = SendTo(databuf, chunk+sizeof(struct VBanHeader), config.port, *config.ip_address ? config.ip_address : NULL,0);
		// SERVE ultimo parm o si incasina con ipaddress e NULL - ecc  £$%@# ; no.. è stronzo in modo Release..
//		https://social.msdn.microsoft.com/Forums/ie/en-US/3076a9cd-57a0-418d-8de1-07adc3b486bb/socket-fails-with-error-10022-when-application-is-run-from-certain-network-shares-on-vista-and?forum=wsk.
//  ret = SendTo(buffer, 1000, config.port, config.ip_address);
//						theApp.FileSpool->print(CLogFile::flagInfo,"VBAN manda %d",ret);
		if(ret < 0)    {
				ASSERT(0);
			int i = GetLastError();
						theApp.FileSpool->print(CLogFile::flagInfo,"  error %d",i);
			if(errno != EINTR)        {
	//            logger_log(LOG_ERROR, "%s: sendto error %d %s", __func__, errno, strerror(errno));
				}
			return ret;
			}

		len-=chunk;

		} while(len>0);

  return ret;
	}





#ifndef _CAMPARTY_MODE

// Authentication server Socket(s)
CAuthSrvSocket2::CAuthSrvSocket2(CAuthSrvSocket *p) {

	m_LineText=new CLineText;
	packetType=0;
	m_Parent=p;
	numLogConn=0;
	cliTimeOut=timeGetTime();
	}

CAuthSrvSocket2::~CAuthSrvSocket2() {

	delete m_LineText;
	}

void CAuthSrvSocket2::OnReceive(int nErr) {
	BYTE myBuf[256];
	char myBuf2[256],*p;
	int i,n /*,numLogConn*/;	// in realta' (v.sotto) non va perche' il socket non dura, ma viene usato e poi cancellato!!!
	DWORD user,idserver,versione,versioneW,serNum;
	CString aIP;
	UINT aPort;
	char tariffa[16],passSconto[16];
	double sconto;
	CTime ct;
	CVidsendSet6 *theServerSet=NULL;


				cliTimeOut=timeGetTime();
				// per scrupolo...


rifo:
	switch(packetType) {
		case 0:
			Receive(&packetType,1);
//			if(CAsyncSocket::Receive(myBuf,256,MSG_PEEK) > 0)
//2018				goto rifo;
			break;
		case 1:
			if((i=Receive(myBuf,127)) > 0) {
				m_LineText->handleReadData(myBuf,i);
				if(m_LineText->ssize() >= 127) {
					n=127;
					m_LineText->readTextRaw((BYTE *)myBuf2,&n);
					nome=myBuf2;
					memcpy(pasw,myBuf2+32,32);
					i=*(DWORD *)(myBuf2+64);	// se client o exhib
					proprieta=i ? 1 : 0;
					idserver=*(DWORD *)(myBuf2+68);	// se client, IDUtenti del Server(exhib)
					versione=*(DWORD *)(myBuf2+72);
					versioneW=*(DWORD *)(myBuf2+76);
					serNum=*(DWORD *)(myBuf2+80);			// potrà arrivare a 8 byte...
					GetPeerName(aIP,aPort);
					prevIP=aIP;
					wsprintf(myBuf2,"Richiesta di autenticazione da %s (nome=%s,pasw=0x%02X%02X%02X%02X%02X%02X%02X%02X...)",
						(LPCTSTR)aIP,(LPCTSTR)nome,pasw[0],pasw[1],pasw[2],pasw[3],pasw[4],pasw[5],pasw[6],pasw[7]);
					theApp.mandaALog(NULL,myBuf2);
					*theApp.FileSpool << myBuf2;
					*myBuf2=1;
					if(Send(myBuf2,1) == SOCKET_ERROR)
						goto fine;
					n=theApp.UtenteCheck(NULL,nome,(BYTE *)pasw,&user,&ct,tariffa,&sconto,passSconto,i,serNum,idserver);
					if(n>0) {
						numLogConn=theApp.UtenteOnline(NULL,nome,aIP,i,idserver,versione,versioneW,serNum);
						if(!numLogConn)
							n=-3;			// metterne poi un altro, a indicare che mettendo on-line qualcosa non e' andato bene...
						}
					else
						numLogConn=0;

					if(Send(&n,sizeof(int)) == SOCKET_ERROR)
						goto fine;
					if(Send(&numLogConn,sizeof(int)) == SOCKET_ERROR)
						goto fine;
					if(Send(&user,sizeof(DWORD)) == SOCKET_ERROR)
						goto fine;
					if(Send(tariffa,16) == SOCKET_ERROR)
						goto fine;
					if(Send(&sconto,sizeof(double)) == SOCKET_ERROR)
						goto fine;
					if(Send(passSconto,16) == SOCKET_ERROR)
						goto fine;
					if(Send(&ct,sizeof(CTimeSpan)) == SOCKET_ERROR)
						goto fine;
					if(theServerSet=new CVidsendSet6(theApp.theDB)) {
						if(theServerSet->Open(CRecordset::forwardOnly,NULL,CRecordset::readOnly)) {
							theServerSet->Requery();
							if(Send(theServerSet->m_FTPServer,40) == SOCKET_ERROR)
								goto fine;
							if(Send(theServerSet->m_FTPLogin,20) == SOCKET_ERROR)
								goto fine;
							if(Send(theServerSet->m_FTPPassword,20) == SOCKET_ERROR)
								goto fine;
							cliTimeOut=timeGetTime();
							theServerSet->Close();
							}
						delete theServerSet;
						}
fine:
					packetType=0;
					}
				}
			break;
		case 2:		// potrebbe servire per indicare altre caratteristiche del client che si autentica...
//				packetType=0;
			break;
		case 11:		// heartBeat
			if((i=Receive(myBuf,1)) > 0) {		// 2 byte, ma il contenuto non mi interessa...
				cliTimeOut=timeGetTime();

				if(theApp.debugMode) {
					if(theApp.FileSpool) {
						GetPeerName(aIP,aPort);
						theApp.FileSpool->print(CLogFile::flagInfo,"ricevuto HeartBeat da %s",(LPCTSTR)aIP);
						}
					}

				packetType=0;
				}
			break;
		default:		// per emergenza...
			i=Receive(myBuf,1);
			if(theApp.debugMode) {
				if(theApp.FileSpool) {
					theApp.FileSpool->print(CLogFile::flagInfo,"ricevuto carattere fuori sequenza da AuthSrvSocket2: %02X",*myBuf);
					}
				}
//				packetType=0;
			break;
		}
	}

void CAuthSrvSocket2::OnClose(int nErr) {

	if(theApp.debugMode) {
		if(theApp.FileSpool)
			theApp.FileSpool->print(CLogFile::flagInfo,"**close authsock srv, OnClose");
		}
	Close();
	m_Parent->doDelete(this);
	}

void CAuthSrvSocket2::updateDBUtenti() {
	CString aIP;
	UINT aPort;

	if(numLogConn) {
		GetPeerName(aIP,aPort);
		if(prevIP.IsEmpty()) 
			prevIP=aIP;
		else {
			if(prevIP != aIP) {
				Close();
				if(theApp.debugMode) {
					if(theApp.FileSpool)
						theApp.FileSpool->print(CLogFile::flagInfo,"**close authsock, IPcambiato");
					}
				}
			}
		if(!(proprieta & 1)) 		// se client...
			theApp.AggUtenteOnline((CVidsendSet4 *)NULL,numLogConn);
		else
			theApp.AggUtenteOnline((CVidsendSet5Ex *)NULL,numLogConn);
		}
	}

CAuthSrvSocket::CAuthSrvSocket() {

	maxConn=0;
	}

CAuthSrvSocket::~CAuthSrvSocket() {

	while(!cSockRoot.IsEmpty())
		delete cSockRoot.RemoveHead();

//		if(s->sock->m_hSocket != INVALID_SOCKET)
//			s->sock->Close();
	maxConn=0;
	}

void CAuthSrvSocket::OnAccept(int nErr) {
	int j;
	CAuthSrvSocket2 *s;

	if(maxConn < theApp.maxAuthConn) {
		s=new CAuthSrvSocket2(this);
		if(s) {
			cSockRoot.AddTail(s);
			maxConn++;

			j=Accept(*s);
			if(j) {

				}
			else
				theApp.mandaALog(NULL,"Tentativo fallito di connessione client AuthSrv");
			return;
			}
		}
	{
	CAuthSrvSocket2 tempSock(this);
	Accept(tempSock);
	tempSock.Close();			// questo indica troppe connessioni
	}
	if(theApp.FileSpool)
		theApp.FileSpool->print(CLogFile::flagInfo,"impossibile Accept() AuthSrv, stroncato!");
	}

void CAuthSrvSocket::doDelete(CAuthSrvSocket2 *ss,int bForced) {
	CAuthSrvSocket2 *s;

	theApp.UtenteOffline(ss,bForced);	// comunque tolgo l'utente on line, se poi c'e' anche il #log conn o chiam lo chiudo
	POSITION po;

	po=cSockRoot.Find(ss);
	if(po) {
		s=cSockRoot.GetAt(po);
		delete s;
		cSockRoot.RemoveAt(po);
		}

	maxConn--;
	}

void CAuthSrvSocket::updateDBUtenti() {
	CAuthSrvSocket2 *myRoot;
	POSITION po;

	po=cSockRoot.GetHeadPosition();
	if(po) {
		do {
			cSockRoot.GetNext(po)->updateDBUtenti();
			} while(po);
		}
	}

#endif	// CAMPARTY_MODE


CAuthCliSocket::CAuthCliSocket(/*CWnd *hWnd*/ CExDocument *p) {

	m_LineText=new CLineText;
	packetType=0;
	m_Parent=p;
//	theWnd=hWnd;
	theWnd=NULL;
	reInit();
	}

CAuthCliSocket::~CAuthCliSocket() {

	delete m_LineText;
	}

void CAuthCliSocket::OnReceive(int nErr) {
	BYTE myBuf[1024];
	int i,n;
	DWORD len;

rifo:
	switch(packetType) {
		case 0:
			Receive(&packetType,1);
//			if(CAsyncSocket::Receive(myBuf,256,MSG_PEEK) > 0)
//2018				goto rifo;
			break;
		case 1:				// dati di autenticazione
			if((i=Receive(myBuf,sizeof(int)*3+sizeof(CTimeSpan)+
					sizeof(FTPserver)+sizeof(FTPlogin)+sizeof(FTPpassword)+
					sizeof(tariffa)+sizeof(double)+sizeof(passwordSconto))) > 0) {
				m_LineText->handleReadData(myBuf,i);
				if(m_LineText->ssize() >= sizeof(int)*3+sizeof(CTimeSpan)+
					sizeof(FTPserver)+sizeof(FTPlogin)+sizeof(FTPpassword)+
					sizeof(tariffa)+sizeof(double)+sizeof(passwordSconto)) {
					n=sizeof(int);
					m_LineText->readTextRaw((BYTE *)&response,&n);
					n=sizeof(int);
					m_LineText->readTextRaw((BYTE *)&extraParm,&n);
					n=sizeof(DWORD);
					m_LineText->readTextRaw((BYTE *)&IDutente,&n);
					n=sizeof(tariffa);
					m_LineText->readTextRaw((BYTE *)tariffa,&n);
					n=sizeof(double);
					m_LineText->readTextRaw((BYTE *)&sconto,&n);
					n=sizeof(passwordSconto);
					m_LineText->readTextRaw((BYTE *)passwordSconto,&n);
					n=sizeof(CTimeSpan);
					m_LineText->readTextRaw((BYTE *)&timedConn,&n);
					n=sizeof(FTPserver);
					m_LineText->readTextRaw((BYTE *)FTPserver,&n);
					n=sizeof(FTPlogin);
					m_LineText->readTextRaw((BYTE *)FTPlogin,&n);
					n=sizeof(FTPpassword);
					m_LineText->readTextRaw((BYTE *)FTPpassword,&n);
					packetType=0;
					}
				}
			break;
		case 10:				// ping (connessione ancora attiva) da parte del vidsend.exe gestore
										// in realta' per ora non lo uso... lascio spedire l'Heartbeat dai client
			if((i=Receive(myBuf,1)) > 0) {		// 2 byte, ma il contenuto non mi interessa...
				*myBuf=11;
				myBuf[1]=0;
				i=Send(myBuf,2);
				packetType=0;
				}
			break;
		case 12:				// msg di avviso (disconnettersi) da server ev. crashato.
// disattivato sul server 2/10/03 xche' non ha senso, visto che poi chiudi socket

			if((i=Receive(myBuf,1)) > 0) {		// 2 byte, ma il contenuto non mi interessa...
				AfxMessageBox("Il server è stato riavviato!");
				getParent()->getView()->SendMessage(WM_COMMAND,ID_CONNESSIONE_DISCONNETTI,0);		// per costringere a ricollegarsi!! (v. anche OCX)
				packetType=0;
				}
			break;
		default:
			Receive(myBuf,1023);
			break;
		}


	}

void CAuthCliSocket::OnClose(int nErr) {
	
	Close();
		if(theApp.debugMode) {
			if(theApp.FileSpool)
				theApp.FileSpool->print(2,"**close authsock-cli, OnClose");
			}
		AfxMessageBox("Il server è stato riavviato!");
		getParent()->getView()->SendMessage(WM_COMMAND,ID_CONNESSIONE_DISCONNETTI,0);		// per costringere a ricollegarsi!! (v. anche OCX)

	}

void CAuthCliSocket::reInit() {

	response=0;
	extraParm=0;
	IDutente=0;
	timedConn=0;
	*FTPserver=0;
	*FTPlogin=0;
	*FTPpassword=0;
	sconto=0;
	*tariffa=0;
	*passwordSconto=0;
	}

int CAuthCliSocket::SendUserPass(char *n,char *p,int m,DWORD id2,
																 DWORD versione,DWORD versioneW,DWORD serNum) {
	char myBuf[128];

	*myBuf=1;
	strncpy(myBuf+1,n,31);
	myBuf[32]=0;
	theApp.Scramble(myBuf+33,p,_tcslen(p),serNum);
	*(DWORD *)(myBuf+65)=m;				// client-guest=0 o server-exib=1
	*(DWORD *)(myBuf+69)=id2;
	*(DWORD *)(myBuf+73)=versione;
	*(DWORD *)(myBuf+77)=versioneW;
	*(DWORD *)(myBuf+81)=serNum;			// può arrivare a 8 byte...
	return Send(myBuf,128);
	}


// Directory server Socket(s)
CDirectorySrvSocket2::CDirectorySrvSocket2(CDirectorySrvSocket *p) {

	m_LineText=new CLineText;
	packetType=0;
	cliTimeOut=-1;
	m_Parent=p;
	}

CDirectorySrvSocket2::~CDirectorySrvSocket2() {

	delete m_LineText;
	}

void CDirectorySrvSocket2::OnReceive(int nErr) {
	BYTE myBuf[256];
	char myBuf2[128],*p;
	int i,n;

	if(theApp.debugMode) {
		CString aIP;
		UINT aPort;
		GetPeerName(aIP,aPort);
		if(theApp.FileSpool)
			theApp.FileSpool->print(CLogFile::flagInfo,"ricevo (CDirectorySrvSocket2) da %s",(LPCTSTR)aIP);
		}

rifo:
	switch(packetType) {
		case 0:
			Receive(&packetType,1);
//			if(CAsyncSocket::Receive(myBuf,256,MSG_PEEK) > 0)
//2018				goto rifo;
			break;
		case 1:
			if((i=Receive(myBuf,32)) > 0) {
				m_LineText->handleReadData(myBuf,i);
				if(m_LineText->ssize() >= 32) {
					n=32;
					m_LineText->readTextRaw((BYTE *)myBuf2,&n);
					connName=myBuf2;
					if(theApp.theConnections)
						((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
					packetType=0;
					}
				}
			break;
		case 2:		// potrebbe servire per indicare le caratteristiche del server che si registra...
			break;
		default:
			Receive(myBuf,255);
			break;
		}
	}

void CDirectorySrvSocket2::OnClose(int nErr) {

	Close();
	if(theApp.theDirectoryServer)
		((CVidsendView6*)theApp.theDirectoryServer->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
	m_Parent->doDelete(this);
	}

CDirectorySrvSocket::CDirectorySrvSocket() {

	maxConn=0;
	}

CDirectorySrvSocket::~CDirectorySrvSocket() {

	while(!cSockRoot.IsEmpty())
		delete cSockRoot.RemoveHead();

//		if(s->sock->m_hSocket != INVALID_SOCKET)
//			s->sock->Close();
	maxConn=0;
	}

void CDirectorySrvSocket::OnAccept(int nErr) {
	int i,j;
	CDirectorySrvSocket2 *s;

	if(maxConn < theApp.maxDirSrvConn) {

		s=new CDirectorySrvSocket2(this);
		if(s) {
			cSockRoot.AddTail(s);
			maxConn++;

			j=Accept(*s);
			if(j) {
				if(theApp.theDirectoryServer)
					((CVidsendView6*)theApp.theDirectoryServer->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
				}
			else
				theApp.mandaALog(NULL,"Tentativo fallito di connessione client DirSrv");
			return;
			}
		}
	{
	CDirectorySrvSocket2 tempSock(this);
	Accept(tempSock);
	tempSock.Close();			// questo indica troppe connessioni
	}
	if(theApp.FileSpool)
		theApp.FileSpool->print(2,"impossibile Accept() DirSrv, stroncato!");
	}

void CDirectorySrvSocket::doDelete(CDirectorySrvSocket2 *ss) {
	CDirectorySrvSocket2 *s;

	POSITION po;

	po=cSockRoot.Find(ss);
	if(po) {
		s=cSockRoot.GetAt(po);
		delete s;
		cSockRoot.RemoveAt(po);
		}

	maxConn--;
	}



CDirectoryCliSocket::CDirectoryCliSocket() {

	m_LineText=new CLineText;
	packetType=0;
	}

CDirectoryCliSocket::~CDirectoryCliSocket() {

	delete m_LineText;
	}

void CDirectoryCliSocket::OnReceive(int nErr) {
	BYTE myBuf[1024],*p;
	int i,n;
	DWORD len;

rifo:
	switch(packetType) {
		case 0:
			Receive(&packetType,1);
//			if(CAsyncSocket::Receive(myBuf,256,MSG_PEEK) > 0)
//2018				goto rifo;
			break;
		case 2:
			if((i=Receive(myBuf,255)) > 0) {
				m_LineText->handleReadData(myBuf,i);
				if(m_LineText->ssize() >= 255) {
					n=255;
					m_LineText->readTextRaw((BYTE *)myBuf,&n);
					AfxMessageBox((char *)myBuf,MB_ICONEXCLAMATION);
					packetType=0;
					}
				}
			break;
		default:
			Receive(myBuf,1023);
			break;
		}
	}

void CDirectoryCliSocket::OnClose(int nErr) {
	
	Close();
	AfxMessageBox("Server di gestione chiuso!");
	if(theApp.theServer)
		theApp.theServer->OnCloseDocument();
	// la chiusura del client di dirSrv indica che si vuole disattivare questo server:
	// quindi chiudere i socket, la cattura video... o semplicemente distruggere la finestra video server?
	}



// CHAT Socket(s)
CChatSrvSocket2::CChatSrvSocket2(CChatSrvSocket *p) {

	m_LineText=new CLineText;
	packetType=0;
	m_Parent=p;
	cliTimeOut=timeGetTime();
	richOne2One=0;
	serNum=0;
	connColor=0;
	connType=0;
	}

CChatSrvSocket2::~CChatSrvSocket2() {

	delete m_LineText;
	}

void CChatSrvSocket2::OnReceive(int nErr) {
	BYTE myBuf[256],myBuf2[256],*p;
	int i,n,k;
	CString aIP;
	UINT aPort;
	CStringList blackList;

	if((i=Receive(myBuf,100)) > 0) {			//POTREBBE ESSERE SBAGLIATO...
																				// Bisogna fare attenzione a quanti byte si ricevono...
																				// NON sappiamo se OnReceive viene richiamata piu' volte o no...
																				// in caso io non legga tutti i dati...
		m_LineText->handleReadData(myBuf,i);
rifo:
		switch(packetType) {
			case 0:
				if(m_LineText->ssize() >= 1) {
					n=1;
					m_LineText->readTextRaw((BYTE *)&packetType,&n);
//2018					goto rifo;
					}
				break;
			case 1:			// ID di chi si connette
				if(m_LineText->ssize() >= 41) {
					n=41;
					m_LineText->readTextRaw((BYTE *)myBuf2,&n);
					connType=*(BYTE *)myBuf2;
					connColor=*(DWORD *)(myBuf2+1) & 0xffffff;
					serNum=*(DWORD *)(myBuf2+4);
					richOne2One=*(BYTE *)(myBuf2+8);
					connName=myBuf2+9;

					if(theApp.theChat) {			// SE non esiste, dovrebbe bloccare tutto! (ma non puo' succedere)

						k=theApp.theChat->loadBlacklistedIP(&blackList);
						POSITION po=blackList.GetHeadPosition();
						while(po) {
							if(serNum == atoi(blackList.GetAt(po))) {
								Close();
								if(theApp.debugMode)
									if(theApp.FileSpool)
										theApp.FileSpool->print(CLogFile::flagInfo,"Client Chat (IP=%s) indesiderato!",(LPCTSTR)aIP);
								goto fine;
								}
							blackList.GetNext(po);
							}

						if(theApp.theChat->Opzioni & CVidsendDoc4::noOne2One)
							richOne2One=0;
						theApp.theChat->updateTree();
						if(theApp.theChat->Opzioni & CVidsendDoc4::mostraEU) {
							struct CHAT_MESSAGE m;
							CString S="(Entra ";
							S+=connName;
							S+=")";
							if(richOne2One) {
								S+=" [uno a uno]";
								theApp.theChat->Opzioni |= CVidsendDoc4::onlyOne2One;
//								((CButton *)theApp.theChat->getView()->GetDlgItem(IDC_BUTTON5))->SetState(1);
								((CButton *)theApp.theChat->getView()->GetDlgItem(IDC_BUTTON5))->SetCheck(1);
								}
							_tcscpy(m.message,(LPCTSTR)S);
							m.id=2;
							m.color=RGB(255,255,255);
							m.extra=CVidsendView4::reverseMsg | CVidsendView4::computerMsg;
							_tcscpy(m.sender,(LPCTSTR)"<server");
							if(theApp.theChat->opzioniVisive & CVidsendDoc4::avvisi_sonori)
								MessageBeep(MB_OK);
							m_Parent->Manda((char *)&m,sizeof(struct CHAT_MESSAGE));
							}
						}
					if(theApp.theConnections)
						((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
					packetType=0;
					if(m_LineText->ssize() > 0)
						goto rifo;
					}
				break;
			case 2:		// messaggio pubblico (da inoltrare a tutti)
				if(m_LineText->ssize() >= 200) {
					struct CHAT_MESSAGE m;
					CString S;
					n=200;
					m_LineText->readTextRaw((BYTE *)&m.message,&n);
					m.id=2;
					m.color=connColor;
					m.extra=(connType & CVidsendView4::supervisorMsg ? CVidsendView4::supervisorMsg : 0) |
						(connType & CVidsendView4::serverVideo ? CVidsendView4::serverVideo : 0);
					_tcscpy(m.sender,(LPCTSTR)connName);
					if(!strncmp(m.message,"\\\\#",3) && !(theApp.theChat->Opzioni & CVidsendDoc4::usaSounds)) {
						goto non_inviare;
						}
					if(!strncmp(m.message,"\\\\*",3) && !(theApp.theChat->Opzioni & CVidsendDoc4::usaIcons)) {
						goto non_inviare;
						}
					m_Parent->Manda((char *)&m,sizeof(struct CHAT_MESSAGE));
non_inviare:
					packetType=0;
					if(m_LineText->ssize() > 0)
						goto rifo;
					}
				break;
			case 3:		// messaggio privato (da inoltrare a UNO!)
/*			if((i=Receive(myBuf,1000)) > 0) {
				m_LineText->handleReadData(myBuf,i);
				if(m_LineText->ssize() >= 255) {
					n=255;
					m_LineText->readTextRaw((BYTE *)myBuf,&n);
					AfxMessageBox(myBuf,MB_ICONEXCLAMATION);
					packetType=0;
					}
				}*/
				break;
			case 11:		// heartBeat
				if(m_LineText->ssize() >= 2) {
					cliTimeOut=timeGetTime();

					if(theApp.debugMode) {
						if(theApp.FileSpool) {
							GetPeerName(aIP,aPort);
							theApp.FileSpool->print(2,"ricevuto HeartBeat CHAT da %s",(LPCTSTR)aIP);
							}
						}

					packetType=0;
					}
				break;
			default:
				Receive(myBuf,255);
				break;
			}
		}
fine:	;
	}

void CChatSrvSocket2::OnClose(int nErr) {

	Close();
	if(theApp.theChat) {
		theApp.theChat->updateTree();
		if(theApp.theChat->Opzioni & CVidsendDoc4::mostraEU) {
			struct CHAT_MESSAGE m;
			CString S="(";
			S+=connName;
			S+=" esce.)";
			_tcscpy(m.message,(LPCTSTR)S);
			m.id=2;
			m.color=RGB(255,255,255);
			m.extra=CVidsendView4::reverseMsg | CVidsendView4::computerMsg;
			_tcscpy(m.sender,(LPCTSTR)"<server");
			if(theApp.theChat->opzioniVisive & CVidsendDoc4::avvisi_sonori)
				MessageBeep(MB_OK);
			if(theApp.theChat->Opzioni & CVidsendDoc4::onlyOne2One)			// all'uscita, COMUNQUE TOLGO 1:1
				theApp.theChat->Opzioni &= ~CVidsendDoc4::onlyOne2One;
			((CButton *)theApp.theChat->getView()->GetDlgItem(IDC_BUTTON5))->SetCheck(0);
			m_Parent->Manda((char *)&m,sizeof(struct CHAT_MESSAGE));
			}
		}
	m_Parent->doDelete(this);
	}

int CChatSrvSocket2::Manda(const char *p,int n) {

	return Send(p,n);
	}

CChatSrvSocket::CChatSrvSocket() {

	maxConn=0;
	}

CChatSrvSocket::~CChatSrvSocket() {

	while(!cSockRoot.IsEmpty())
		delete cSockRoot.RemoveHead();

//		if(s->sock->m_hSocket != INVALID_SOCKET)
//			s->sock->Close();
	maxConn=0;
	}

void CChatSrvSocket::OnAccept(int nErr) {
	int i,j,k;
	CChatSrvSocket2 *s;
	CString aIP;
	UINT aPort;
//	CStringList blackList;

	if((theApp.theChat->Opzioni & CVidsendDoc4::onlyOne2One && maxConn<(1+1)) || 
		(!(theApp.theChat->Opzioni & CVidsendDoc4::onlyOne2One ) && maxConn < theApp.theChat->maxConn) ) {

		s=new CChatSrvSocket2(this);
		if(s) {
			cSockRoot.AddTail(s);
			maxConn++;

			j=Accept(*s);
			if(j) {

				if(!theApp.theChat->suonoIn.IsEmpty()) {
					sndPlaySound(theApp.theChat->suonoIn,SND_ASYNC);
					}

				s->GetPeerName(aIP,aPort);
				/*
				k=theApp.theChat->loadBlacklistedIP(&blackList);
				POSITION po=blackList.GetHeadPosition();
				while(po) {
					if(aIP == blackList.GetAt(po)) {
						s->Close();
						if(theApp.debugMode)
							if(theApp.FileSpool)
								theApp.FileSpool->print(2,"Client Chat (IP=%s) indesiderato!",(LPCTSTR)aIP);
						goto fine;
						}
					blackList.GetNext(po);
					} SERVIVA x BLACK_LIST degli IP... ora v. Receive*/

				theApp.mandaALog((LPCTSTR)aIP,"Connesso client Chat");
					struct CHAT_INFO *si=theApp.theChat->getConnectionInfo();
					if(si) {
						if(s->Send("\x1",1) != SOCKET_ERROR)
							s->Send(si,sizeof(struct CHAT_INFO));
						delete si;
	//					if() {
	//					cSock[i].Send("\x2",1);
	//					cSock[i].Send(,255);
						//}
					}
				if(theApp.theChat) {
					theApp.theChat->updateTree();
					// la notifica del nuovo arrivato e' in Receive info...
					}
				if(theApp.theConnections)
				((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
				}
			else
				theApp.mandaALog(NULL,"Tentativo fallito di connessione client Chat");
fine:
			return;
			}
		}
	{
	CChatSrvSocket2 tempSock(this);
	Accept(tempSock);
	tempSock.Close();			// questo indica superamento max. connessioni!
	}
	if(theApp.FileSpool)
		theApp.FileSpool->print(2,"impossibile Accept() chat, stroncato!");
	}

int CChatSrvSocket::Manda(const char *p,int n) {
	POSITION po;

	po=cSockRoot.GetHeadPosition();
	if(po) {
		do {
			cSockRoot.GetNext(po)->Manda(p,n);
			} while(po);
		}

	return 1;
	}

void CChatSrvSocket::doDelete(CChatSrvSocket2 *ss) {
	CChatSrvSocket2 *s;
	POSITION po;

	if(!theApp.theChat->suonoOut.IsEmpty()) {
		sndPlaySound(theApp.theChat->suonoOut,SND_ASYNC);
		}

	po=cSockRoot.Find(ss);
	if(po) {
		s=cSockRoot.GetAt(po);
		delete s;
		cSockRoot.RemoveAt(po);
		}

	maxConn--;

	if(theApp.theConnections)
		((CVidsendView6*)theApp.theConnections->getView())->PostMessage(WM_COMMAND,ID_VISUALIZZA_AGGIORNA,0);
	}


CChatCliSocket::CChatCliSocket(CWnd *v) {

	m_LineText=new CLineText;
	theWnd=v;
	packetType=0;
	}

CChatCliSocket::~CChatCliSocket() {

	delete m_LineText;
	}

void CChatCliSocket::OnReceive(int nErr) {
	BYTE myBuf[256],*p;
	int i,n;
	DWORD len;
	CString aIP;
	UINT aPort;

	if((i=Receive(myBuf,100)) > 0) {			//POTREBBE ESSERE SBAGLIATO...
																				// Bisogna fare attenzione a quanti byte si ricevono...
																				// NON sappiamo se OnReceive viene richiamata piu' volte o no...
																				// in caso io non legga tutti i dati...
		m_LineText->handleReadData(myBuf,i);
rifo:
		switch(packetType) {
			case 0:
				if(m_LineText->ssize() >= 1) {
					n=1;
					m_LineText->readTextRaw((BYTE *)&packetType,&n);
					goto rifo;
					}
				break;
			case 1:
				if(m_LineText->ssize() >= sizeof(struct CHAT_INFO)) {
					n=sizeof(struct CHAT_INFO);
					m_LineText->readTextRaw((BYTE *)&(((CVidsendView4 *)theWnd)->chatInfo),&n);
					GetPeerName(aIP,aPort);
					if(!((CVidsendView4 *)theWnd)->doConnect((char *)(LPCTSTR)aIP,&((CVidsendView4 *)theWnd)->chatInfo)) {
						((CVidsendView4 *)theWnd)->endConnect();
						Close();
						}
					packetType=0;
					if(m_LineText->ssize() > 0)
						goto rifo;
					}
				break;
			case 2:
				if(m_LineText->ssize() >= (sizeof(struct CHAT_MESSAGE)-1)) {
					struct CHAT_MESSAGE *m;
					n=(sizeof(struct CHAT_MESSAGE)-1);
					if(m=new struct CHAT_MESSAGE) {
						m_LineText->readTextRaw(((BYTE *)m)+1,&n);
						((CVidsendView4 *)theWnd)->addToListBox(m);
						}
					else
						m_LineText->readTextRaw((BYTE *)myBuf,&n);
					//segnalare errore!
					packetType=0;
					if(m_LineText->ssize() > 0)
						goto rifo;
					}
				break;
			case 3:
				if(m_LineText->ssize() >= sizeof(struct CHAT_ROOMS_INFO)) {
					n=sizeof(struct CHAT_ROOMS_INFO);
					m_LineText->readTextRaw((BYTE *)&(((CVidsendView4 *)theWnd)->chatRoomsInfo),&n);
					// finire...
					packetType=0;
					if(m_LineText->ssize() > 0)
						goto rifo;
					}
				break;
			default:
				Receive(myBuf,255);
				break;
			}
		}
	}

void CChatCliSocket::OnClose(int nErr) {
	struct CHAT_MESSAGE *m;
	int i,n=(sizeof(struct CHAT_MESSAGE)-1);
	
	if(m=new struct CHAT_MESSAGE) {
		_tcscpy(m->message,"Server chiuso");
		m->color=RGB(255,255,255);
		m->extra=CVidsendView4::reverseMsg | CVidsendView4::computerMsg;
		_tcscpy(m->sender,(LPCTSTR)"<server");
		((CVidsendView4 *)theWnd)->addToListBox(m);
		}
	Close();
	((CVidsendView4 *)theWnd)->endConnect();
	AfxMessageBox("Server chat chiuso");
	}





BOOL CVidsendApp::impostaDaAtomClock(char *server) {
	SYSTEMTIME sysTime;
	CTime myTime;
	CSocket MySocket;
	int i;
	char myBuf[64];

	MySocket.Create();
	if(MySocket.Connect(server,IPPORT_DAYTIME)) {
		MySocket.Receive(myBuf,32);
		myBuf[32]=0;
		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(0,"ricevo da server atomico %s",myBuf);
		myTime=parseGMTTime(myBuf);
		sysTime.wYear=myTime.GetYear();
		sysTime.wMonth=myTime.GetMonth();
		sysTime.wDay=myTime.GetDay();
		sysTime.wHour=myTime.GetHour();
		sysTime.wMinute=myTime.GetMinute();
		sysTime.wSecond=myTime.GetSecond();
		i=SetLocalTime(&sysTime);
		if(!i) {
			i=GetLastError();
			if(theApp.FileSpool)
				theApp.FileSpool->print(2,"SetLocalTime fallita: lstErr=%x",i);
  		}
		else
//			::SendMessage(HWND_BROADCAST,WM_TIMECHANGE,0,0);
			BroadcastSystemMessage(BSF_FORCEIFHUNG,BSM_ALLCOMPONENTS,WM_TIMECHANGE,0,0);
		return 1;
		}
	else {
		if(theApp.FileSpool)
			theApp.FileSpool->print(2,"non trovo server atomico %s",server);
		}
	return 0;
	}



int CVidsendApp::getMonthFromGMTString(const char *s) {

	switch(*(int *)s) {
		case ' NAJ':
			return 1;
			break;
		case ' BEF':
			return 2;
			break;
		case ' RAM':
			return 3;
			break;
		case ' RPA':
			return 4;
			break;
		case ' YAM':
			return 5;
			break;
		case ' NUJ':
			return 6;
			break;
		case ' LUJ':
			return 7;
			break;
		case ' GUA':
			return 8;
			break;
		case ' PES':
			return 9;
			break;
		case ' TCO':
			return 10;
			break;
		case ' VON':
			return 11;
			break;
		case ' CED':
			return 12;
			break;
		}
	return -1;
	}

CTime CVidsendApp::parseGMTTime(const char *s) {
	char *p;
	int i;
	CTime myT;
	struct tm t;
	
//	_tzset();			// questo imposterebbe la timezone, che altrimenti defaulta a -8h

	while(isspace(*s))
		s++;
	if(isalpha(*s)) {
		strupr((char *)s);			// porcata! ma ok
		switch(*(short int *)s) {
			case 'US':
				i=0;
				break;
			case 'OM':
				i=1;
				break;
			case 'UT':
				i=2;
				break;
			case 'EW':
				i=3;
				break;
			case 'HT':
				i=4;
				break;
			case 'RF':
				i=5;
				break;
			case 'AS':
				i=6;
				break;
			}
		t.tm_wday=i;
		p=(char *)s+3;
		if(*p ==',')
			p++;
		p++;
		if(isdigit(*p)) {
			t.tm_mday=atoi(p);
			if(isdigit(*p))
				p++;
			if(isdigit(*p))
				p++;
			p++;
			t.tm_mon=getMonthFromGMTString(p);
			p+=4;
			t.tm_year=atoi(p)-1900;
			p+=5;
			t.tm_hour=atoi(p);
			t.tm_min=atoi(p+3);
			t.tm_sec=atoi(p+6);
			p[12]=0;
			i=atoi(p+10);
			if(p[9] == '-')
				i=-i;
			}
		else {
			t.tm_mon=getMonthFromGMTString(p);
			p+=4;
			t.tm_mday=atoi(p);
			p+=3;
			t.tm_hour=atoi(p);
			t.tm_min=atoi(p+3);
			t.tm_sec=atoi(p+6);
			if(p[9] == 'U') {		// variante con "UTC 1998
				t.tm_year=atoi(p+13)-1900;
				i=0;								// greenwich
				}
			else {							  // variante con 1998 +0100
				t.tm_year=atoi(p+9)-1900;
				p[17]=0;
				i=atoi(p+15);
				if(p[14] == '-')
					i=-i;
				}
			}
		}
	else {
		s+=2;		// salto \xd\xa
		t.tm_year=atoi(s+6);
		t.tm_mon=atoi(s+9);
		t.tm_mday=atoi(s+12);
		t.tm_hour=atoi(s+15);
		t.tm_min=atoi(s+18);
		t.tm_sec=atoi(s+21);
		i=atoi(s+24);
		}
	{
		CTime tt(t.tm_year+1900,t.tm_mon,t.tm_mday,t.tm_hour-i-(_timezone/3600),t.tm_min,t.tm_sec);

		myT=tt;
		}

	return myT;
	}


CString CBase64Decoder::m_sBase64Alphabet = 
  _T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

//////////////// Implementation //////////////////////////////////////

int CBase64Decoder::Decode(const CString& sInput, CString& sOutput) {
  m_nBitsRemaining = 0;

  sOutput.Empty();  
	if(sInput.GetLength() == 0)
		return 0;

	//Build Decode table
  int nDecode[256];
	for(int i=0; i<256; i++) 
		nDecode[i] = -2; // Illegal digit
	for(i=0; i<64; i++) {
		nDecode[m_sBase64Alphabet[i]] = i;
		nDecode[m_sBase64Alphabet[i] | 0x80] = i; // Ignore 8th bit
		nDecode['='] = -1; 
		nDecode['=' | 0x80] = -1; // Ignore MIME padding char
		}

	// Decode the Input
  i=0;
  TCHAR* szOutput = sOutput.GetBuffer(sInput.GetLength());
	for(int p=0; p<sInput.GetLength(); p++) {
		int c = sInput[p];
		int nDigit = nDecode[c & 0x7F];
		if(nDigit < -1) {
      sOutput.ReleaseBuffer();  
			return 0;
			}
		else if(nDigit >= 0) 
			// i (index into output) is incremented by write_bits()
			WriteBits(nDigit & 0x3F, 6, szOutput, i);
		}	
  szOutput[i] = _T('\0');
  sOutput.ReleaseBuffer();

	return i;
	}

void CBase64Decoder::WriteBits(UINT nBits, int nNumBits, LPTSTR szOutput, int& i) {
	UINT nScratch;

	m_lBitStorage = (m_lBitStorage << nNumBits) | nBits;
	m_nBitsRemaining += nNumBits;
	while(m_nBitsRemaining > 7) {
		nScratch = m_lBitStorage >> (m_nBitsRemaining - 8);
		szOutput[i++] = (TCHAR) (nScratch & 0xFF);
		m_nBitsRemaining -= 8;
		}
	}


char CSMTPAttachment::m_base64tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "abcdefghijklmnopqrstuvwxyz0123456789+/";
#define BASE64_MAXLINE  76
#define EOL  "\r\n"
BOOL CSMTPAttachment::EncodeBase64(const char *pszIn, int nInLen, char *pszOut, int nOutSize, int *nOutLen) {
  //Input Parameter validation
  ASSERT(pszIn);
  ASSERT(pszOut);
  ASSERT(nOutSize);
  ASSERT(nOutSize >= Base64BufferSize(nInLen));

#ifndef _DEBUG
  //justs get rid of "unreferenced formal parameter"
  //compiler warning when doing a release build
  nOutSize;
#endif

  //Set up the parameters prior to the main encoding loop
  int nInPos  = 0;
  int nOutPos = 0;
  int nLineLen = 0;

  // Get three characters at a time from the input buffer and encode them
  for(int i=0; i<nInLen/3; ++i) {

    //Get the next 2 characters
    int c1 = pszIn[nInPos++] & 0xFF;
    int c2 = pszIn[nInPos++] & 0xFF;
    int c3 = pszIn[nInPos++] & 0xFF;

    //Encode into the 4 6 bit characters
    pszOut[nOutPos++] = m_base64tab[(c1 & 0xFC) >> 2];
    pszOut[nOutPos++] = m_base64tab[((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4)];
    pszOut[nOutPos++] = m_base64tab[((c2 & 0x0F) << 2) | ((c3 & 0xC0) >> 6)];
    pszOut[nOutPos++] = m_base64tab[c3 & 0x3F];
    nLineLen += 4;

    //Handle the case where we have gone over the max line boundary
    if(nLineLen >= BASE64_MAXLINE-3) {
      char* cp = EOL;
      pszOut[nOutPos++] = *cp++;
      if (*cp) 
        pszOut[nOutPos++] = *cp;
      nLineLen = 0;
			}
		}

  // Encode the remaining one or two characters in the input buffer
  char* cp;
  switch (nInLen % 3) {
    case 0:
      cp = EOL;
      pszOut[nOutPos++] = *cp++;
      if (*cp) 
        pszOut[nOutPos++] = *cp;
      break;
    case 1:
    {
      int c1 = pszIn[nInPos] & 0xFF;
      pszOut[nOutPos++] = m_base64tab[(c1 & 0xFC) >> 2];
      pszOut[nOutPos++] = m_base64tab[((c1 & 0x03) << 4)];
      pszOut[nOutPos++] = '=';
      pszOut[nOutPos++] = '=';
      cp = EOL;
      pszOut[nOutPos++] = *cp++;
      if (*cp) 
        pszOut[nOutPos++] = *cp;
      break;
    }
    case 2:
    {
      int c1 = pszIn[nInPos++] & 0xFF;
      int c2 = pszIn[nInPos] & 0xFF;
      pszOut[nOutPos++] = m_base64tab[(c1 & 0xFC) >> 2];
      pszOut[nOutPos++] = m_base64tab[((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4)];
      pszOut[nOutPos++] = m_base64tab[((c2 & 0x0F) << 2)];
      pszOut[nOutPos++] = '=';
      cp = EOL;
      pszOut[nOutPos++] = *cp++;
      if (*cp) 
        pszOut[nOutPos++] = *cp;
      break;
    }
    default: 
      ASSERT(FALSE); 
      break;
	  }
  pszOut[nOutPos] = 0;
	if(nOutLen)
		*nOutLen = nOutPos;
  return TRUE;
	}

int CSMTPAttachment::Base64BufferSize(int nInputSize) {
  int nOutSize = (nInputSize+2)/3*4;                    // 3:4 conversion ratio

  nOutSize += _tcslen(EOL)*nOutSize/BASE64_MAXLINE + 3;  // Space for newlines and NUL
  return nOutSize;
	}




/*/////////////////////////////////////////////////////////////////////
FTPclient.cpp (c) GDI 1999
V1.0.0 (10/4/99)
Phil Anderson. philip@gd-ind.com

Simple FTP client functionality. If you have any problems with it,
please tell me about them (or better still e-mail me the fixed
code). Please feel free to use this code however you wish, although
if you make changes please put your name in the source & comment what
you did.

Nothing awesome going on here at all (all sockets are used in
synchronous blocking mode), but it does the following
things WinInet doesn't seem to:
* Supports loads of different firewalls (I think, I don't
  have access to all types so they haven't all been fully
  tested yet)
* Allows you to execute any command on the FTP server
* Adds 10K to your app install rather than 1Mb #;-)

Functions return TRUE if everything went OK, FALSE if there was an,
error. A message describing the outcome (normally the one returned
from the server) will be in m_retmsg on return from the function.
There are a few error msgs in the app's string table that you'll
need to paste into your app, along with this file & FTPclient.h

If you created your app without checking the "Use Windows Sockets"
checkbox in AppWizard, you'll need to add the following bit of code
to you app's InitInstance()

if(!AfxSocketInit())
{
	AfxMessageBox("Could not initialize Windows Sockets!");
	return FALSE;
}

To use:

1/ Create an object of CFTPclient.

2/ Use LogOnToServer() to connect to the server. Any arguments
not used (e.g. if you're not using a firewall), pass an empty
string or zero for numeric args. You must pass a server port
number, use the FTP default of 21 if you don't know what it is.

3/ Use MoveFile() to upload/download a file, 1st arg is local file
path, 2nd arg is remote file path, 3rd arg is TRUE for a PASV
connection (required by some firewalls), FALSE otherwise, 4th arg
is TRUE to upload, FALSE to download file. MoveFile only works in
synchronous mode (ie the function will not return 'till the transfer
is finished). File transfers are always of type BINARY.

4/ You can use FTPcommand() to execute FTP commands (eg
FTPcommand("CWD /home/mydir") to change directory on the server),
note that this function will return FALSE unless the server response
is a 200 series code. This should work fine for most FTP commands, 
otherwise you can use WriteStr() and ReadStr() to send commands & 
interpret the response yourself. Use LogOffServer() to disconnect
when done.

/////////////////////////////////////////////////////////////////////*/


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFTPclient::CFTPclient() {

	m_pCtrlsokfile=NULL;
	m_pCtrlTxarch=NULL;
	m_pCtrlRxarch=NULL;
	}


CFTPclient::~CFTPclient() {

	CloseControlChannel();
	}


//////////////////////////////////////////////////////////////////////
// Public Functions
//////////////////////////////////////////////////////////////////////

// function to connect & log on to FTP server
BOOL
CFTPclient::LogOnToServer(CString hostname,int hostport,CString username,CString password, 
													LPCTSTR acct, LPCTSTR fwhost,LPCTSTR fwusername, LPCTSTR fwpassword,int fwport,int logontype) {
	int port,logonpoint=0;
	const int LO=-2, ER=-1;
	CString buf,temp;
	const int NUMLOGIN=9; // currently supports 9 different login sequences
	int logonseq[NUMLOGIN][100] = {
		// this array stores all of the logon sequences for the various firewalls 
		// in blocks of 3 nums. 1st num is command to send, 2nd num is next point in logon sequence array
		// if 200 series response is rec'd from server as the result of the command, 3rd num is next
		// point in logon sequence if 300 series rec'd
		{0,LO,3, 1,LO,6, 2,LO,ER}, // no firewall
		{3,6,3, 4,6,ER, 5,ER,9, 0,LO,12, 1,LO,15, 2,LO,ER}, // SITE hostname
		{3,6,3, 4,6,ER, 6,LO,9, 1,LO,12, 2,LO,ER}, // USER after logon
		{7,3,3, 0,LO,6, 1,LO,9, 2,LO,ER}, //proxy OPEN
		{3,6,3, 4,6,ER, 0,LO,9, 1,LO,12, 2,LO,ER}, // Transparent
		{6,LO,3, 1,LO,6, 2,LO,ER}, // USER with no logon
		{8,6,3, 4,6,ER, 0,LO,9, 1,LO,12, 2,LO,ER}, //USER fireID@remotehost
		{9,ER,3, 1,LO,6, 2,LO,ER}, //USER remoteID@remotehost fireID
		{10,LO,3, 11,LO,6, 2,LO,ER} // USER remoteID@fireID@remotehost
		};

	if(logontype<0 || logontype>=NUMLOGIN) 
		return FALSE; // illegal connect code
	// are we connecting directly to the host (logon type 0) or via a firewall? (logon type>0)
	if(!logontype) {
		temp=hostname;
		port=hostport;
		}
	else {
		temp=fwhost;
		port=fwport;
		}
	if(hostport != 21)
		hostname.Format(hostname+":%d",hostport); // add port to hostname (only if port is not 21)
	if(!OpenControlChannel(temp,port)) 
		return FALSE;
	if(!FTPcommand("")) 
		return FALSE; // get initial connect msg off server

	if(theApp.debugMode)
		if(theApp.FileSpool)
			theApp.FileSpool->print(2,"FTP_welcome: %s",(LPCTSTR)m_retmsg);

	// go through appropriate logon procedure
	while(1) {
		switch(logonseq[logontype][logonpoint]) {
			case 0:
				temp="USER "+username;
				break;
			case 1:
				temp="PASS "+password;
				break;
			case 2:
				temp="ACCT ";
				temp+=acct;
				break;
			case 3:
				temp="USER ";
				temp+=fwusername;
				break;
			case 4:
				temp="PASS ";
				temp+=fwpassword;
				break;
			case 5:
				temp="SITE "+hostname;
				break;
			case 6:
				temp="USER "+username+"@"+hostname;
				break;
			case 7:
				temp="OPEN "+hostname;
				break;
			case 8:
				temp="USER ";
				temp+=fwusername;
				temp+="@"+hostname;
				break;
			case 9:
				temp="USER "+username+"@"+hostname+" "+fwusername;
				break;
			case 10:
				temp="USER "+username+"@"+fwusername+"@"+hostname;
				break;
			case 11:
				temp="PASS "+password+"@"+fwpassword;
				break;
			}
		// send command, get response

		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(2,"FTP_write: %s",temp);

		if(!WriteStr(temp)) 
			return FALSE;
		if(!ReadStr()) 
			return FALSE;

		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(2,"FTP_read: %s",(LPCTSTR)m_retmsg);

		// only these responses are valid
		if(m_fc != 2 && m_fc != 3) 
			return FALSE;
		logonpoint=logonseq[logontype][logonpoint+m_fc-1]; //get next command from array
		switch(logonpoint) {
			case ER: // ER means summat has gone wrong
				m_retmsg.LoadString(IDS_FTPMSG1);
				return FALSE;
			case LO: // LO means we're fully logged on
				return TRUE;
			}
		}
	}


// function to log off & close connection to FTP server
void CFTPclient::LogOffServer() {
	
	WriteStr("QUIT");
	CloseControlChannel();
	}


// function to execute commands on the FTP server
BOOL CFTPclient::FTPcommand(CString command) {

	if(command!="" && !WriteStr(command)) 
		return FALSE;
	if((!ReadStr()) || (m_fc!=2)) 
		return FALSE;
	return TRUE;
	}

int CFTPclient::preMove(CString remotefile,int pasv,int get,CSocket *datachannel) {
	CString lhost,temp,rhost;
	UINT localsock,serversock,i,j;
	CSocket sockSrvr;
	DWORD bufPos;
	DWORD lpArgument=0;

	if(!FTPcommand("TYPE I")) 
		return FALSE; // request BINARY mode
	if(pasv) { // set up a PASSIVE type file transfer
		if(!FTPcommand("PASV")) 
			return FALSE;
		// extract connect port number and IP from string returned by server
		if((i=m_retmsg.Find("("))==-1 || (j=m_retmsg.Find(")"))==-1) 
			return FALSE;
		temp=m_retmsg.Mid(i+1,(j-i)-1);
		i=temp.ReverseFind(',');
		serversock=atol(temp.Right(temp.GetLength()-(i+1))); //get ls byte of server socket
		temp=temp.Left(i);
		i=temp.ReverseFind(',');
		serversock+=256*atol(temp.Right(temp.GetLength()-(i+1))); // add ms byte to server socket
		rhost=temp.Left(i);
		while(1) { // convert commas to dots in IP
			if((i=rhost.Find(","))==-1) 
				break;
			rhost.SetAt(i,'.');
			}
		}
	else { // set up a ACTIVE type file transfer
		m_retmsg.LoadString(IDS_FTPMSG6);
		// get the local IP address off the control channel socket
		if(!GetSockName(lhost,localsock)) 
			return FALSE;
		while(1) { // convert returned '.' in ip address to ','
			if((i=lhost.Find("."))==-1) break;
			lhost.SetAt(i,',');
			}
		// create listen socket (let MFC choose the port) & start the socket listening
		if((!sockSrvr.Create(0,SOCK_STREAM,NULL)) || (!sockSrvr.Listen())) 
			return FALSE;
		if(!sockSrvr.GetSockName(temp,localsock))
			return FALSE;// get the port that MFC chose
		// convert the port number to 2 bytes + add to the local IP
		lhost.Format(lhost+",%d,%d",localsock/256,localsock%256);
		if(!FTPcommand("PORT "+lhost)) 
			return FALSE;// send PORT cmd to server
		}
	// send RETR/STOR command to server
	if(!WriteStr((get ? "RETR " : "STOR ")+remotefile)) 
		return FALSE;
	if(pasv) {// if PASV create the socket & initiate outbound data channel connection
		if(!datachannel->Create()) {
			m_retmsg.LoadString(IDS_FTPMSG6);
			return FALSE;
			}
		datachannel->Connect(rhost,serversock); // attempt to connect asynchronously (server will tell us if/when we're connected)
		}
	if(!ReadStr() || m_fc!=1) 
		return FALSE; // get response to RETR/STOR command
	if(!pasv && !sockSrvr.Accept(*datachannel)) 
		return FALSE; // if !PASV accept inbound data connection from server
	// we're connected & ready to do the data transfer, so set blocking mode on data channel socket
/*	if((!datachannel->AsyncSelect(0)) || (!datachannel->IOCtl(FIONBIO,&lpArgument))) {
		m_retmsg.LoadString(IDS_FTPMSG6);
		return FALSE;
		} NO, usando CSocket anziche' CAsyncSocket! */
	}	

int CFTPclient::postMove(CSocket *datachannel) {

	datachannel->Close();
	if(!FTPcommand("")) 
		return FALSE; // check transfer outcome msg from server
	return TRUE;
	}

// function to upload/download files
BOOL CFTPclient::SendBuff(CString remotefile,BYTE *buf,DWORD len,BOOL pasv) {
	CSocket datachannel;
	int numread=4096,numsent;
	DWORD bufPos;
	
	if(!preMove(remotefile,pasv,0,&datachannel))
		return FALSE;
	bufPos=0;
	while(len) { // move data to server & read local file
		if((numsent=datachannel.Send(buf+bufPos,min(len,numread)))==SOCKET_ERROR) 
			break;
		// if we sent fewer bytes than we read from file, rewind file pointer
		bufPos+=numsent;
		len-=numsent;
		}
	return postMove(&datachannel);
	}

BOOL CFTPclient::RecvBuff(CString remotefile, BYTE *buf,DWORD maxLen,BOOL pasv) {
	CSocket datachannel;
	int num,numread,numsent;
	char cbuf[bufSize];
	
	if(!preMove(remotefile,pasv,1,&datachannel))
		return FALSE;
	while(1) { // move data from server & write local file
		TRY {
			if(!(num=datachannel.Receive(cbuf,bufSize,0)) || num==SOCKET_ERROR) 
				break; // (EOF || network error)
			else {
				memcpy(buf,cbuf,num);
				buf+=num;			 // controllare maxlen!!!
				}
			}
		CATCH (CException,e) {
			m_retmsg.LoadString(IDS_FTPMSG5);
			return FALSE;
			}
		END_CATCH
		}
	return postMove(&datachannel);
	}

BOOL CFTPclient::MoveFile(CString remotefile,CString localfile,BOOL get,BOOL pasv) {
	CFile datafile;
	CSocket datachannel;
	int num,numread,numsent;
	char cbuf[bufSize];
	
	// open local file
	if(!datafile.Open(localfile,(get ? CFile::modeWrite | CFile::modeCreate : CFile::modeRead) | CFile::typeBinary)) {
		m_retmsg.LoadString(IDS_FTPMSG4);
		return FALSE;
		}
	if(!preMove(remotefile,pasv,get,&datachannel)) {
		if(get) {			// se il file remoto non esiste, NON lascio un file lungh. zero qua...
		TRY {
			CFile::Remove(localfile);
			}
		CATCH(CFileException,e) {
			;
			}
		END_CATCH
		} 
		return FALSE;
		}
	while(1) { // move data from/to server & read/write local file
		TRY {
			if(get) {
				if(!(num=datachannel.Receive(cbuf,bufSize)) || num==SOCKET_ERROR) {
					if(get && num==SOCKET_ERROR) {			// se il trasf. non termina correttamente, NON lascio un file monco qua...
						CFile::Remove(localfile);
						}
					break; // (EOF||network error)
					}
				else 
					datafile.Write(cbuf,num);
				}
			else {
				if(!(numread=datafile.Read(cbuf,bufSize))) 
					break; //EOF
				if((numsent=datachannel.Send(cbuf,numread))==SOCKET_ERROR) 
					break;
				// if we sent fewer bytes than we read from file, rewind file pointer
				if(numread != numsent) 
					datafile.Seek(numsent-numread,CFile::current);
				}
			}
		CATCH (CException,e) {
			m_retmsg.LoadString(IDS_FTPMSG5);
			return FALSE;
			}
		END_CATCH
		}
	datafile.Close();
	return postMove(&datachannel);
	}

BOOL CFTPclient::ChDir(CString S) {

	return FTPcommand("CWD "+S);
	}

BOOL CFTPclient::MkDir(CString S) {

	return FTPcommand("MKD "+S);
	}

BOOL CFTPclient::Delete(CString S) {

	return FTPcommand("DELE "+S);
	}

BOOL CFTPclient::GetList(CString localfile,BOOL pasv) {
	CFile datafile;
	CSocket datachannel;
	int num,numread;
	char cbuf[bufSize];
	CString lhost,temp;
	UINT localsock;
	int i;
	CSocket sockSrvr;
	
	// open local file
	if(!datafile.Open(localfile,(CFile::modeWrite | CFile::modeCreate) | CFile::typeBinary)) {
		m_retmsg.LoadString(IDS_FTPMSG4);
		return FALSE;
		}
	if(!FTPcommand("TYPE I")) 
		return FALSE; // request BINARY mode
	// set up a ACTIVE type file transfer
		m_retmsg.LoadString(IDS_FTPMSG6);
		// get the local IP address off the control channel socket
		if(!GetSockName(lhost,localsock)) 
			return FALSE;
		while(1) { // convert returned '.' in ip address to ','
			if((i=lhost.Find("."))==-1) break;
			lhost.SetAt(i,',');
			}
		// create listen socket (let MFC choose the port) & start the socket listening
		if((!sockSrvr.Create(0,SOCK_STREAM,NULL)) || (!sockSrvr.Listen())) 
			return FALSE;
		if(!sockSrvr.GetSockName(temp,localsock))
			return FALSE;// get the port that MFC chose
		// convert the port number to 2 bytes + add to the local IP
		lhost.Format(lhost+",%d,%d",localsock/256,localsock%256);
		if(!FTPcommand("PORT "+lhost)) 
			return FALSE;// send PORT cmd to server
	// send RETR/STOR command to server
	if(!WriteStr("LIST"))
		return FALSE;
	if(!ReadStr() || m_fc!=1) 
		return FALSE; // get response to RETR/STOR command
	if(!pasv && !sockSrvr.Accept(datachannel)) 
		return FALSE; // if !PASV accept inbound data connection from server

	while(1) { // move data from/to server & read/write local file
		TRY {
			if(!(num=datachannel.Receive(cbuf,bufSize)) || num==SOCKET_ERROR) 
				break; // (EOF||network error)
			else 
				datafile.Write(cbuf,num);
			}
		CATCH (CException,e) {
			m_retmsg.LoadString(IDS_FTPMSG5);
			return FALSE;
			}
		END_CATCH
		}
	datafile.Close();
	datachannel.Close();
	if(!FTPcommand("")) 
		return FALSE; // check transfer outcome msg from server
	return TRUE;
	}


// function to send a command string on the server control channel
BOOL CFTPclient::WriteStr(CString outputstring) {

	m_retmsg.LoadString(IDS_FTPMSG6); // pre-load "network error" msg (in case there is one) #-)
	TRY {
		m_pCtrlTxarch->WriteString(outputstring+"\r\n");
		m_pCtrlTxarch->Flush();
		}
	CATCH(CException,e) {
		return FALSE;
		}
	END_CATCH
	m_retmsg.Empty();
	return TRUE;
	}


// this function gets the server response line
BOOL CFTPclient::ReadStr() {
	int retcode;

	if(!ReadStr2()) 
		return FALSE;
	if(m_retmsg.GetLength()<4 || m_retmsg.GetAt(3) != '-') 
		return TRUE;
	retcode=atol(m_retmsg);
	while(1) { //handle multi-line server responses
		if(m_retmsg.GetLength()>3 && (m_retmsg.GetAt(3)==' ' && atol(m_retmsg)==retcode)) 
			return TRUE;
		if(!ReadStr2()) 
			return FALSE;
		}
	}


//////////////////////////////////////////////////////////////////////
// Private functions
//////////////////////////////////////////////////////////////////////

// read a single response line from the server control channel
BOOL CFTPclient::ReadStr2() {

	TRY {
		if(!m_pCtrlRxarch->ReadString(m_retmsg)) {
			m_retmsg.LoadString(IDS_FTPMSG6);
			return FALSE;
		}
	}
	CATCH(CException,e) {
		m_retmsg.LoadString(IDS_FTPMSG6);
		return FALSE;
	}
	END_CATCH
	if(m_retmsg.GetLength()>0) 
		m_fc=m_retmsg.GetAt(0)-48; // get 1st digit of the return code (indicates primary result)
	return TRUE;
	}


// open the control channel to the FTP server
BOOL CFTPclient::OpenControlChannel(CString serverhost,int serverport) {

	m_retmsg.LoadString(IDS_FTPMSG2);
	if(!Create()) 
		return FALSE;
	m_retmsg.LoadString(IDS_FTPMSG3);
	if(!(Connect(serverhost,serverport))) 
		return FALSE;
	m_retmsg.LoadString(IDS_FTPMSG2);
	if(!(m_pCtrlsokfile=new CSocketFile(this))) 
		return FALSE;
	if(!(m_pCtrlRxarch=new CArchive(m_pCtrlsokfile,CArchive::load))) 
		return FALSE;
	if(!(m_pCtrlTxarch=new CArchive(m_pCtrlsokfile,CArchive::store))) 
		return FALSE;
	return TRUE;
	}


// close the control channel to the FTP server
void CFTPclient::CloseControlChannel() {

	if(m_pCtrlTxarch) 
		delete m_pCtrlTxarch;
	m_pCtrlTxarch=NULL;
	if(m_pCtrlRxarch) 
		delete m_pCtrlRxarch;
	m_pCtrlRxarch=NULL;
	if(m_pCtrlsokfile) 
		delete m_pCtrlsokfile;
	m_pCtrlsokfile=NULL;
	Close();
	return;
	}


