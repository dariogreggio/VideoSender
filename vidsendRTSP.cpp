// GD/C adapted 2023-2026 da Ansersion https://www.cnblogs.com/ansersion/p/6959690.html

#include "stdafx.h"
#include "vidsend.h"
#include "vidsendLog.h"

#include "re.h"
#include "vidsendRTSP.h"

extern NALUTypeBase_H264 NaluBaseType_H264Obj;

MyRTPSession::MyRTP_Teardown(MediaSession *,struct timeval *) {
	}

BYTE *MyRTPSession::GetMyRTPData(BYTE *, size_t *, unsigned long) {

	return 0;
	}

BYTE *MyRTPSession::GetMyRTPPacket(BYTE *, size_t *, unsigned long) {

	return 0;
	}


int MyTCPTransmitter::SendRTPData(const void *data,size_t len)	{
	return SendRTPRTCPData(data, len);
	}

int MyTCPTransmitter::SendRTCPData(const void *data,size_t len) {
	return SendRTPRTCPData(data, len);
	}

int MyTCPTransmitter::SendRTPRTCPData(const void *data, size_t len) {

	if(!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	if(!m_created)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}
	if(len > RTPTCPTRANS_MAXPACKSIZE)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_SPECIFIEDSIZETOOBIG;
	}
	
	std::map<SocketType, SocketData>::iterator it = m_destSockets.begin();
	std::map<SocketType, SocketData>::iterator end = m_destSockets.end();

	vector<SocketType> errSockets;
	int flags = 0;
#ifdef RTP_HAVE_MSG_NOSIGNAL
	flags = MSG_NOSIGNAL;
#endif // RTP_HAVE_MSG_NOSIGNAL

	while(it != end)	{
		BYTE lengthBytes[2] = { (BYTE)((len >> 8)&0xff), (BYTE)(len&0xff) };
		SocketType sock = it->first;

		if(sock->Send((const char *)lengthBytes,2,flags) < 0 ||
			sock->Send((const char *)data,len,flags) < 0)
			errSockets.push_back(sock);
		++it;
		}
	
	MAINMUTEX_UNLOCK

	if(errSockets.size() != 0)	{
		for(size_t i=0 ; i < errSockets.size() ; i++)
			OnSendError(errSockets[i]);
		}

	// Don't return an error code to avoid the poll thread exiting
	// due to one closed connection for example
	return 0;
	}

bool MyTCPTransmitter::ComesFromThisTransmitter(const RTPAddress *addr) {

	if(!m_init)
		return false;

	if(!addr)
		return false;
	
	MAINMUTEX_LOCK
	
	if(!m_created)
		return false;

	if(addr->GetAddressType() != RTPAddress::TCPAddress)
		return false;

	const RTPTCPAddress *pAddr = static_cast<const RTPTCPAddress *>(addr);
	bool v = false;

//	JRTPLIB_UNUSED(pAddr);
	// TODO: for now, we're assuming that we can't just send to the same transmitter

	MAINMUTEX_UNLOCK
	return v;
	}

RTPRawPacket *MyTCPTransmitter::GetNextPacket() {
	return 0;
	}


// ---------------------------------------------------------------------------------------------------------
#define MEDIA_SESSION_OK 		1
#define MEDIA_SESSION_ERROR 	0


/*
   Refer to RTP PayloadType list
   "0" means that the item is reserved
   */
int PT2TimeRateMap[] = {
	8000, 
	0, 
	0, 
	8000,
	8000,
	8000,
	16000,
	8000,
	8000,
	8000,
	44100, 
	44100, 
	8000,
	8000,
	90000,
	8000,
	11025, 
	22050, 
	8000, 
	0, 
	0, 
	0, 
	0, 
	0
	};

MediaSession::MediaSession():	MediaType(""), Protocol(""), EncodeType(""), TimeRate(0),
	ControlURI(""),	SessionID(""),	// RTSPSockfd(-1),
	RTPPort(0),	rtpsock(NULL),
	RTCPPort(0),	rtcpsock(NULL), rtspsock(NULL),
	Packetization(PACKET_MODE_SINGAL_NAL), frameTypeBase(NULL),
	RTPInterface(NULL), Timeout(0) {

	ChannelNum = 1;
	}

MediaSession::~MediaSession() {

	RTP_Teardown();

	if(RTPInterface) {
    delete RTPInterface;
    RTPInterface = NULL;
    }
	if(frameTypeBase) {
    delete frameTypeBase;
    frameTypeBase = NULL;
    }
	}

class MyRTPTCPSession;
class MyRTPUDPSession;

int MediaSession::RTP_Teardown(struct timeval *tval) {

	if(!RTPInterface) {
		return MEDIA_SESSION_OK;
		}
	if(0 == RTPPort) 
		return MEDIA_SESSION_ERROR;

	RTPPort = 0;
	// RTPSockfd = -1;
	rtpsock=NULL;
	RTCPPort = 0;
	// RTCPSockfd = -1;
	rtcpsock=NULL;

	RTPInterface->MyRTP_Teardown(this, tval);
	delete RTPInterface;
	RTPInterface = NULL;
	return MEDIA_SESSION_OK;
	}

int MediaSession::RTP_SetUp(CSocket *tunnelling_sock) {

	if(0 == TimeRate)
		return MEDIA_SESSION_ERROR;
	if(0 == RTPPort) 
		return MEDIA_SESSION_ERROR;
	if(RTPInterface) {
		printf("RTP SetUp already\n");
		return MEDIA_SESSION_OK;
		}

			theApp.FileSpool->print(CLogFile::flagInfo,"MyRTP_SetUp TCP: %d\n", tunnelling_sock);
  if(tunnelling_sock) {
		RTPInterface = new MyRTPTCPSession;
	  if(!RTPInterface->MyRTP_SetUp(this, tunnelling_sock)) 
			return MEDIA_SESSION_ERROR;
		} 
	else {
		RTPInterface = new MyRTPUDPSession;
		if(!RTPInterface->MyRTP_SetUp(this)) 
			return MEDIA_SESSION_ERROR;
		}

	return MEDIA_SESSION_OK;
	}

BYTE *MediaSession::GetMediaData(BYTE *buf, size_t *size, unsigned long timeout) {

	if(!RTPInterface) 
		return NULL;
	return RTPInterface->GetMyRTPData(buf,size,timeout);		// virtual, prende quella giusta se TCP o UDP
	}

BYTE *MediaSession::GetMediaPacket(BYTE *buf, size_t *size, unsigned long timeout) {

	if(!RTPInterface) 
		return NULL;
	return RTPInterface->GetMyRTPPacket(buf,size,timeout);
	}

int MediaSession::MediaInfoCheck() {

	// Check "PayloadType"
	if(PayloadType.size() == 0) {
		printf("WARNING: invalid PayloadType\n");
		return -1;
		}
/*	for(vector<int>::iterator it = PayloadType.begin(); it != PayloadType.end(); it++) {
		if(*it < 0) {
			printf("WARNING: invalid PayloadType\n");
			return -1;
			}
		}	*/
	if(TimeRate <= 0) {
		TimeRate = PT2TimeRateMap[*(PayloadType.begin())]; // FIXME: only use the first PayloadType
		printf("MediaInfoCheck: %d\n", TimeRate);
		}

	return 0;
	}

void MediaSession::SetRtpDestroiedClbk(void (*clbk)()) { 

	if(RTPInterface)
		RTPInterface->SetDestroiedClbk(clbk);
	}

void MediaSession::LockSocket() {

  if(RTPInterface)
    RTPInterface->LockSocket();
	}

void MediaSession::UnlockSocket() {

  if(RTPInterface)
    RTPInterface->UnlockSocket();
	}

CSocket *MediaSession::GetTunnellingSocket() {

  if(RTPInterface)
    return RTPInterface->GetTunnellingSocket();
  return NULL;
	}


// ---------------------------------------------------------------------------------------------------------
const const char *CRTSPClientSocket::HttpHeadUserAgent="User-Agent: ";
const const char *CRTSPClientSocket::HttpHeadXSessionCookie="x-sessioncookie: ";
const const char *CRTSPClientSocket::HttpHeadAccept="Accept: ";
const const char *CRTSPClientSocket::HttpHeadPrama="Pragma: ";
const const char *CRTSPClientSocket::HttpHeadCacheControl="Cache-Control: ";
const const char *CRTSPClientSocket::HttpHeadContentType="Content-Type: ";
const const char *CRTSPClientSocket::HttpHeadContentLength="Content-Length: ";
const const char *CRTSPClientSocket::HttpHeadExpires="Expires: ";
// const string CRTSPClientSocket::HttpTunnelMsg("User-Agent: %s\r\nx-sessioncookie: %s\r\nAccept: application/x-rtsp-tunnelled\r\nPragma: no-cache\r\nCache-Control: no-cache\r\n");

CRTSPClientSocket::CRTSPClientSocket(CString uri) :
	RtspURI(uri), RtspCSeq(0), RtspIP(""), RtspPort(PORT_RTSP), RtspResponse(""), SDPStr(""), 
	CmdPLAYSent(false), isConnected(false), GetVideoDataCount(GET_SPS_PPS_PERIOD),
	Username(""), Password(""), Realm(""), Nonce("") {

	// SDPInfo = new multimap<string, string>;
	MediaSessionMap = new map<CString, MediaSession *>;
	AudioBuffer.Size = 0;
	VideoBuffer.Size = 0;
	if((AudioBuffer.Buf = (BYTE *)malloc(MEDIA_BUFSIZ)))
		AudioBuffer.Size = MEDIA_BUFSIZ;
	if((VideoBuffer.Buf = (BYTE *)malloc(MEDIA_BUFSIZ)))
		VideoBuffer.Size = MEDIA_BUFSIZ;

	ByeFromServerAudioClbk = NULL;
	ByeFromServerVideoClbk = NULL;

	/* Temporary only FU_A supported */
	// NALUType = new FU_A;

  RtspOverHttpDataPort = 0;
  RtspOverHttpDataSockfd = NULL;

  HttpHeadUserAgentContent = "MyRTSPClient";
  HttpHeadXSessionCookieContent = "";
  HttpHeadAcceptContent = "application/x-rtsp-tunnelled";
  HttpHeadPramaContent = "no-cache";
  HttpHeadCacheControlContent = "no-cache";
  HttpHeadContentTypeContent = "application/x-rtsp-tunnelled";
  HttpHeadContentLengthContent = "32767";
  HttpHeadExpiresContent = "Sun, 6 Jan 2030 00:00:00 GMT";		//1972 9 jan sun

  sdpData = new SDPData();
	}

SDPData::SDPData() {
	sdpVersion=0;
	sessionName.Empty();
	sdpSessionTime.startTime=0;
	sdpSessionTime.stopTime=0;
	sdpConnectionData.networkType.Empty();
	sdpConnectionData.addressType.Empty();
	sdpConnectionData.address.Empty();
	sdpOriginStruct.userName.Empty();
	sdpOriginStruct.sessionId=0;
	sdpOriginStruct.version=0;
	sdpOriginStruct.networkType.Empty();
	sdpOriginStruct.addressType.Empty();
	sdpOriginStruct.address.Empty();
	}
SDPData::~SDPData() {
	}

CRTSPClientSocket::~CRTSPClientSocket() {

	delete MediaSessionMap;
	MediaSessionMap = NULL;

	if(AudioBuffer.Buf) {
		free(AudioBuffer.Buf);
		AudioBuffer.Buf = NULL;
		AudioBuffer.Size = 0;
		}

	if(VideoBuffer.Buf) {
		free(VideoBuffer.Buf);
		VideoBuffer.Buf = NULL;
		VideoBuffer.Size = 0;
		}

  delete sdpData;
  sdpData = NULL;
	isConnected=FALSE;
	}


BOOL CRTSPClientSocket::Connect(LPCTSTR s,WORD port) {

	if(!port) {
		port=RtspOverHttpDataPort ? RtspOverHttpDataPort : RtspPort;
		}
	if(CSocket::Create()) {
		CString uri(s);
    uri = parseUriWithUserPwd(uri);
		isConnected=CSocket::Connect(uri,port);
		return isConnected;
		}
	return FALSE;
	}

BOOL CRTSPClientSocket::Disconnect() {

	Close();
	isConnected=FALSE;
	return TRUE;
	}

void CRTSPClientSocket::Close() {

	if(RtspOverHttpDataSockfd)
		RtspOverHttpDataSockfd->Close();
	CSocket::Close();
	isConnected=FALSE;
	}

int CRTSPClientSocket::Send(CString S) {

	return CSocket::Send(S,S.GetLength());
	}

int CRTSPClientSocket::Send(WORD http_tunnel_port, CString S) {
	int i;

  if(http_tunnel_port != 0) {
		CStringEx S2=S;
    char encodedBytes[4096];
		S2.Encode64();
		_tcscpy(encodedBytes, (LPCTSTR)S2);
		i=CSocket::Send(encodedBytes,_tcslen(encodedBytes));
    if(i<=0) {
      Close();
      }
    return i;
    } 
	else {
		i=CSocket::Send(S,S.GetLength());
    if(i<=0) {
      Close();
		  }
    return i;
    }
	}

int CRTSPClientSocket::ReadHeader(char *buf,DWORD len,WORD timeout) {
	int i,n,n2,retVal=-1;
	DWORD ti;
	char *myBuf,myBuf2[16],*p;

	n=0;
	myBuf=(char *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,4096);
	if(!myBuf)
		return -1;

	ti=timeGetTime()+timeout*1000;
	while(ti>timeGetTime()) {
		i=CAsyncSocket::Receive(myBuf2,1);		// RTSP/1.0 200 OK<CR><LF>
		if(i<0) {
			i=GetLastError();
			if(i != WSAEWOULDBLOCK)
				break;
			}
		else {
			if((n+i) >= 4096) {
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

int CRTSPClientSocket::ReadHeader(char *buf,DWORD len,WORD http_tunnel_port,WORD timeout) {

  if(http_tunnel_port != 0) {
		map<CString, MediaSession *>::iterator it;
    for(it=MediaSessionMap->begin(); it!=MediaSessionMap->end(); it++) {
			if(it->second->GetTunnellingSocket() == RtspOverHttpDataSockfd) {
				break;
				}
      }
		if(it != MediaSessionMap->end()) {
			it->second->LockSocket();
			}
    if(RtspOverHttpDataSockfd->ReadHeader(buf,len) <= 0) {
      Close();
			if(it != MediaSessionMap->end()) {
				it->second->UnlockSocket();
				}
			return -1;
      }
		if(it != MediaSessionMap->end()) {
			it->second->UnlockSocket();
			}
    }
	else {
    if(ReadHeader(buf,len) <= 0) {
      Close();
      return 0;
      }
    }
	}

int CRTSPClientSocket::ReadData(char *buf,DWORD len,WORD timeout) {
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


CString CRTSPClientSocket::parseUriWithUserPwd(CString uri) {
	/*MyRegex Regex;
	list<string> Group;

    if(Regex.Regex(uri.c_str(), PatternRtspUriWithUserPwd.c_str(), &Group)) {
        Group.pop_front();
        uri.assign(Group.front());Group.pop_front();
        Username.assign(Group.front());Group.pop_front();
        Password.assign(Group.front());Group.pop_front();
        uri += Group.front();Group.pop_front();
    }*/

	int i;

//	uri.LowerCase();
	if(!uri.Left(5).CompareNoCase("rtsp:"))		// consento rtsp:
		uri=uri.Mid(5);
	if(!uri.Left(1).CompareNoCase("/"))			// consento quindi 2 slash
		uri=uri.Mid(1);
	if(!uri.Left(1).CompareNoCase("/"))			// o 1, per server già dato (in costruttore ecc)
		uri=uri.Mid(1);
	i=uri.Find('/');							// tolgo nomefile
	if(i>=0)
		uri=uri.Left(i);
	i=uri.Find(':');							// e ev. porta
	if(i>=0)
		uri=uri.Left(i);

  return uri;
	}

int CRTSPClientSocket::RecvSDP(CRTSPClientSocket *s,char *msg, size_t size) {

	// https://en.wikipedia.org/wiki/Session_Description_Protocol
	return s->ReadData(msg,size);
	}

ErrorType CRTSPClientSocket::DoDESCRIBE(CString uri, bool http_tunnel_no_response) {
	CString RtspUri;
//https://en.wikipedia.org/wiki/Real_Time_Streaming_Protocol

	if(!uri.IsEmpty()) {
		RtspURI=uri;
		RtspUri=uri;
		}
	else if(!RtspURI.IsEmpty()) 
		RtspUri=RtspURI;
	else 
		return RTSP_INVALID_URI;

	if(!isConnected)
		if(!Connect(RtspUri))
			return RTSP_INVALID_URI;

//	RtspUri += "/user=admin_password=cinzia_channel=1_stream=0.sdp";
//	RtspUri += "/stream=0.sdp";


	CString Cmd("DESCRIBE");
	CString Msg,S;
	Msg = Cmd + " " + "rtsp://";
	Msg += RtspURI + " " + "RTSP/" + VERSION_RTSP + CWebSrvSocket2_base::CRLF;
	Msg += "CSeq: ";
	S.Format("%u",++RtspCSeq);
	Msg += S+CWebSrvSocket2_base::CRLF2;
    // cout << "DEBUG: " << Msg.str() << endl;

  int ret=Send(Msg);
  if(ret<=0) {
		Disconnect();
    return RTSP_SEND_ERROR;
    }
  // if(RtspOverHttpDataPort != 0) {
  //     char * encodedBytes = base64Encode(Msg.str().c_str(), Msg.str().length());
  //     if(NULL == encodedBytes) {
  //         Close(Sockfd);
  //         return RTSP_SEND_ERROR;
  //     }
  //     if(RTSP_NO_ERROR != SendRTSP(Sockfd, string(encodedBytes))) {
  //         Close(Sockfd);
  //         delete[] encodedBytes;
  //         return RTSP_SEND_ERROR;
  //     }
  //     delete[] encodedBytes;
  // } else {
  //     if(RTSP_NO_ERROR != SendRTSP(Sockfd, Msg.str())) {
  //         // close(Sockfd);
  //         Close(Sockfd);
  //         return RTSP_SEND_ERROR;
  //     }
  // }

	// if(RTSP_NO_ERROR != SendRTSP(Sockfd, Msg.str())) {
	// 	// close(Sockfd);
	// 	Close(Sockfd);
	// 	return RTSP_SEND_ERROR;
	// }
	if(http_tunnel_no_response) {
		return RTSP_NO_ERROR;
		}
	char myBuf[1024];
	RtspResponse.Empty();
  ret = ReadHeader(myBuf,1024);
	// se serve auth restituisce WWW-Authenticate: Digest realm=...
  if(ret<0) {
		Disconnect();
    return RTSP_RECV_ERROR;
    }
	RtspResponse=myBuf;
			theApp.FileSpool->print(CLogFile::flagInfo,"  DODESCRIBE header: %s, content=%u",RtspResponse,ret);
  // if(RtspOverHttpDataPort != 0) {
  //     cout << "DEBUG: RecvRTSP http tunnel" << endl;
  //     if(RTSP_NO_ERROR != RecvRTSP(RtspOverHttpDataSockfd, &RtspResponse)) {
  //         cout << "DEBUG: RecvRTSP http tunnel error" << endl;
  //         Close(Sockfd);
  //         return RTSP_RECV_ERROR;
  //     }
  //     cout << "DEBUG: " << RtspResponse << endl;
  // } else {
  //     if(RTSP_NO_ERROR != RecvRTSP(Sockfd, &RtspResponse)) {
  //         Close(Sockfd);
  //         // close(Sockfd);
  //         return RTSP_RECV_ERROR;
  //     }
  // }
// check username and password, if any
	if(CheckAuth(Cmd, RtspUri) != CHECK_OK) {
//		cout << "CheckAuth: error" << endl;
		Close();
		Disconnect();
		return RTSP_RESPONSE_401;
		}
  if(RtspOverHttpDataPort != 0)
	  RecvSDP(RtspOverHttpDataSockfd,myBuf,ret);
	else
	  RecvSDP(this,myBuf,ret);
  SDPStr=myBuf;
    // cout << "DEBUG: " << SDPStr << endl;
			theApp.FileSpool->print(CLogFile::flagInfo,"  SDP: %s",SDPStr);

//	Disconnect();
	return RTSP_NO_ERROR;
	}

int CRTSPClientSocket::ParseSDP(CString SDP) {
	CString Response;
	int Result=0; // don't have meaning yet

	if(!SDP.IsEmpty()) 
		Response=SDP;
	else if(!RtspResponse.IsEmpty()) 
		Response=SDPStr;
	else 
		return Result;

	// cout << "debug: start parse sdp" << endl;
	sdpData->parse(Response);
	map<CString, SDPMediaInfo> mediaInfoMap = sdpData->getMediaInfoMap();
	map<CString, SDPMediaInfo>::iterator it1 = mediaInfoMap.begin();

	while(it1 != mediaInfoMap.end()) {
		if(MediaSessionMap->find(it1->second.mediaType) != MediaSessionMap->end()) {
			it1++;
			continue;
			}
		MediaSession *NewMediaSession = new MediaSession();
		NewMediaSession->MediaType=it1->second.mediaType;
		// cout << "debug: mediaType=" << it1->second.mediaType << endl;;
		NewMediaSession->Protocol=it1->second.transProt;
		// cout << "debug: transProt=" << it1->second.transProt << endl;;
		/* TODO: we only use the first payload type now */
		map<int, map<SDP_ATTR_ENUM, CString> >::iterator it2 = it1->second.fmtMap.begin();
		if(it2 != it1->second.fmtMap.end()) {
			NewMediaSession->PayloadType.push_back(it2->first);	
			if(it2->second.find(CODEC_TYPE) != it2->second.end()) {
				NewMediaSession->EncodeType = it2->second[CODEC_TYPE];
        FrameTypeBase *frameTypeBase = FrameTypeBase::CreateNewFrameType(NewMediaSession->EncodeType);
        if(frameTypeBase)
          frameTypeBase->ParseParaFromSDP(it1->second);
        NewMediaSession->frameTypeBase = frameTypeBase;
				// cout << "debug: EncodeType=" << NewMediaSession.EncodeType << endl;;
				}
			if(it2->second.find(TIME_RATE) != it2->second.end()) {
/*				stringstream ssTimeRate;
				ssTimeRate << it2->second[TIME_RATE];
				ssTimeRate >> NewMediaSession->TimeRate;*/
				NewMediaSession->TimeRate=atoi(it2->second[TIME_RATE]);
				// cout << "debug: TimeRate=" << NewMediaSession.TimeRate << endl;;
				}
			if(it2->second.find(CHANNEL_NUM) != it2->second.end()) {
				NewMediaSession->ChannelNum=atoi(it2->second[CHANNEL_NUM]);
				// cout << "debug: ChannelNum=" << NewMediaSession.ChannelNum << endl;;
				}
			if(it2->second.find(PACK_MODE) != it2->second.end()) {
				NewMediaSession->Packetization=atoi(it2->second[PACK_MODE]);
				// cout << "debug: Packetization=" << NewMediaSession.Packetization << endl;;
				}
			/* 'Value' could be  
			 * 1: "rtsp://127.0.0.1/ansersion/track=1"
			 * 2: "track=1"
			 * If is the '2', it should be prefixed with the URI. */
			NewMediaSession->ControlURI.Empty();
			CString S=it1->second.controlURI;
			if(!it1->second.controlURI.CompareNoCase("rtsp://")) {
				NewMediaSession->ControlURI += RtspURI;
				NewMediaSession->ControlURI += "/";
				}
			NewMediaSession->ControlURI += it1->second.controlURI;
			// cout << "debug: ControlURI=" << NewMediaSession.ControlURI << endl;;
			}
		if(MediaSessionMap->find(it1->second.mediaType) != MediaSessionMap->end()) {
			/* TODO: support multiple sessions of video(audios)*/
			delete NewMediaSession;
			NewMediaSession = NULL;
			} 
		else {
			(*MediaSessionMap)[it1->second.mediaType] = NewMediaSession;
			}
		it1++;
		}

	for(map<CString, MediaSession *>::iterator it = MediaSessionMap->begin(); it != MediaSessionMap->end(); it++)
		it->second->MediaInfoCheck();

	return Result;
	}


//----------------------------------------------------------------------------------------------------------------

void SDPData::parse(CString sdp) {
//	CString pattern = "([a-zA-Z])=(.*)";
  CString key,S;			// 
	CStringEx value;
	MyRegex regex;

#if 0
v=0
o=- 38990265062388 38990265062388 IN IP4 rtsp
s=RTSP Session
c=IN IP4 rtsp
t=0 0
a=control:*
a=range:npt=0-
m=video 0 RTP/AVP 96
a=rtpmap:96 H264/90000 
a=range:npt=0-
a=framerate:0S
a=fmtp:96 profile-level-id=64001f; packetization-mode=1; sprop-parameter-sets=Z2QAH62EAQwgCGEAQwgCGEAQwgCEK1B0CTI=,aO48sA==
a=framerate:25
a=control:trackID=3
#endif

	CStringList group;
	POSITION po;

	int nPos=0;		// https://social.msdn.microsoft.com/Forums/en-US/f67ac9e2-7c05-4c07-9e92-f4993411b6be/load-cstringlist-with-a-list-of-strings-by-a-separator?forum=vcmfcatl
	while(sdp.GetLength() > 0)	{
		nPos = sdp.Find('\n');
		if(nPos == -1) {
			group.AddTail(sdp.Left(sdp.GetLength()-1));
			sdp.Empty();
			}
		else {
			group.AddTail(sdp.Left(nPos-1));
			sdp = sdp.Mid(nPos + 1 /*sDelimiter.GetLength()*/);
			}
		}

  /* start parse session info first as default */
  bool sessionInfo=true, mediaInfo=false, timeInfo=false;


//  stringstream ssTmp;
  SDPMediaInfo *currentMediaInfo=NULL;

	// int debugCount=0;
	po=group.GetHeadPosition();

  while(po)	{
		S=group.GetAt(po);

		// cout << "debug: Count " << debugCount++ << endl;
		int i=S.Find('=');
		if(i<0)
			goto skip_next;
    key=S.Left(i);
    value=S.Mid(i+1);
    // 's': session info start flag
    // 'm': media info start flag
    // 't': time info start flag
    if(key == "s") {
      sessionInfo = true;
      mediaInfo = false;
      timeInfo = false;
	    } 
		else if(key == "m") {
      sessionInfo = false;
      mediaInfo = true;
      timeInfo = false;
		  } 
		else if(key == "t") {
      sessionInfo = false;
      mediaInfo = false;
      timeInfo = true;
			}

    if(sessionInfo) {
      if("s" == key) {
        sessionName=value;
			  } 
			else if("v" == key) {
        sdpVersion=atoi(value);
				} 
			else if("o" == key) {
static const char *SDP_ORIGIN_PATTERN = "(.*) +(.*) +(.*) +(.*) +(.*) +(.*)";
static const char *SDP_CONNECTION_DATA_PATTERN = "(.*) +(.*) +(.*)";
//FINIRE!
        if(value.Regex(SDP_ORIGIN_PATTERN)) {
					i=0;
					sdpOriginStruct.userName=value.Tokenize(_T(" "),i);		// Tokenize NON c'è in questa versione :( quindi ne faccio una io... https://social.msdn.microsoft.com/Forums/vstudio/en-US/7c69968d-d42a-4710-951a-cb85bb3f5ce4/how-to-token-cstring-correctly?forum=vclanguage
          sdpOriginStruct.sessionId=atoi(value.Tokenize(_T(" "),i));		// sembrano long long... boh
          sdpOriginStruct.version=atoi(value.Tokenize(_T(" "),i));
          sdpOriginStruct.networkType=value.Tokenize(_T(" "),i);
          sdpOriginStruct.addressType=value.Tokenize(_T(" "),i);
          sdpOriginStruct.address=value.Tokenize(_T(" "),i);
          }
        if(value.Regex(SDP_CONNECTION_DATA_PATTERN)) {
					i=0;
          sdpConnectionData.networkType=value.Tokenize(_T(" "),i);
          sdpConnectionData.addressType=value.Tokenize(_T(" "),i);
          sdpConnectionData.address=value.Tokenize(_T(" "),i);
          }
	      }
			}
    if(timeInfo) {
      if("t" == key) {
static const char *SDP_SESSION_TIME_PATTERN = "(.*) +(.*)";
        if(value.Regex(SDP_SESSION_TIME_PATTERN)) {
					i=0;
          sdpSessionTime.startTime=atol(value.Tokenize(_T(" "),i));
					sdpSessionTime.stopTime=atol(value.Tokenize(_T(" "),i));
          }
        }
	    }
    if(mediaInfo) {
      if("m" == key) {
				i=0;
        S=value.Tokenize(_T(" "),i);
        int tmp=atoi(value.Tokenize(_T(" "),i));
        currentMediaInfo = &mediaInfoMap[tmp];
        currentMediaInfo->mediaType=S;
        currentMediaInfo->transProt=value.Tokenize(_T("/"),i);
        currentMediaInfo->ports=value.Tokenize(_T(" "),i);
        S=value.Tokenize(_T(" "),i);
        while(!S.IsEmpty()) {
          int payloadId;
          payloadId=atoi(S);
          map<SDP_ATTR_ENUM, CString> *fmtMapTmp = &currentMediaInfo->fmtMap[payloadId];
          (*fmtMapTmp)[MEDIA_TYPE_NAME] = currentMediaInfo->mediaType;
	        S=value.Tokenize(_T(" "),i);
          }
              
        } 
			else if("a" == key) {
static const char *SDP_RTPMAP_PATTERN="rtpmap:(.+) +([0-9A-Za-z]+)/([0-9]+)/?([0-9])?";
static const char *SDP_FMTP_H264_PATTERN="fmtp:(.+) +packetization-mode=([0-2]);.*sprop-parameter-sets=([A-Za-z0-9+/=]+),([A-Za-z0-9+/=]+)";
static const char *SDP_FMTP_H265_PATTERN="fmtp:(.+) .*sprop-vps=([A-Za-z0-9+/=]+);.*sprop-sps=([A-Za-z0-9+/=]+);.*sprop-pps=([A-Za-z0-9+/=]+)";
static const char *SDP_CONTROL_PATTERN="control:(.+)";
//			cout << "debug: mt=" << currentMediaInfo->mediaType << ",line=" << value << endl;
        if(currentMediaInfo && 
					//value.Regex(SDP_RTPMAP_PATTERN)
					value.Left(6)=="rtpmap"
					) {
          int payloadId;
					i=0;
          payloadId=atoi(value.Tokenize(_T(":"),i));
          payloadId=atoi(value.Tokenize(_T(" "),i));
          map<int, map<SDP_ATTR_ENUM, CString> >::iterator it = currentMediaInfo->fmtMap.find(payloadId);
          if(it != currentMediaInfo->fmtMap.end()) {
            it->second[CODEC_TYPE] = value.Tokenize(_T("/"),i);
            it->second[TIME_RATE] = value.Tokenize(_T(" "),i);
            it->second[CHANNEL_NUM] = "1"; // default 1 channel
						S=value.Tokenize(_T(" "),i);
            if(!S.IsEmpty())
              it->second[CHANNEL_NUM] = S;
            }
          } 
				else if(currentMediaInfo && "video" == currentMediaInfo->mediaType && value.Regex(SDP_FMTP_H265_PATTERN)) {
//          cout << "debug: Parse h265" << endl;
          int payloadId;
					i=0;
          payloadId=atoi(value.Tokenize(_T(" "),i));
          map<int, map<SDP_ATTR_ENUM, CString> >::iterator it = currentMediaInfo->fmtMap.find(payloadId);
          if(it != currentMediaInfo->fmtMap.end()) {
            it->second[ATTR_VPS] = value.Tokenize(_T(" "),i);
            it->second[ATTR_SPS] = value.Tokenize(_T(" "),i);
            it->second[ATTR_PPS] = value.Tokenize(_T(" "),i);
						}
          } 
				else if(currentMediaInfo && "video" == currentMediaInfo->mediaType && 
					//value.Regex(SDP_FMTP_H264_PATTERN)
					value.Left(7)=="fmtp:96"
					) {
			// cout << "SDP_FMTP_H264_PATTERN" << endl;
          int payloadId;
					i=0;
          payloadId=atoi(value.Tokenize(_T(":"),i));
          payloadId=atoi(value.Tokenize(_T(" "),i));
          map<int, map<SDP_ATTR_ENUM, CString> >::iterator it = currentMediaInfo->fmtMap.find(payloadId);
          if(it != currentMediaInfo->fmtMap.end()) {
            it->second[PACK_MODE] = value.Tokenize(_T(" "),i);
            it->second[ATTR_SPS] = value.Tokenize(_T(" "),i);
            it->second[ATTR_PPS] = value.Tokenize(_T(" "),i);
						}
					}
				else if(currentMediaInfo && value.Regex(SDP_CONTROL_PATTERN)) {
					// da me non c'è... forzo sotto
          currentMediaInfo->controlURI = value;
					}

        }
      }

skip_next:
		S=group.GetNext(po);
    }

	// patch pd regex
//	rtpmap:96 H264/90000 
//	a=fmtp:96 profile-level-id=64001f; packetization-mode=1; sprop-parameter-sets=Z2QAH62EAQwgCGEAQwgCGEAQwgCEK1B0CTI=,aO48sA==
/*	{
  int payloadId;
	CStringEx value="a=fmtp:96 profile-level-id=64001f; packetization-mode=1; sprop-parameter-sets=Z2QAH62EAQwgCGEAQwgCGEAQwgCEK1B0CTI=,aO48sA==";
	int i=0;
  payloadId=atoi(value.Tokenize(_T(" "),i));
  map<int, map<SDP_ATTR_ENUM, CString> >::iterator it = currentMediaInfo->fmtMap.find(payloadId);
  if(it != currentMediaInfo->fmtMap.end()) {
    it->second[PACK_MODE] = value.Tokenize(_T(" "),i);
    it->second[ATTR_SPS] = value.Tokenize(_T(" "),i);
    it->second[ATTR_PPS] = value.Tokenize(_T(" "),i);
		}
	}*/

//	currentMediaInfo->controlURI="rtsp://192.168.1.10:554/user=admin_password=cinzia_channel=3_stream=0.sdp/trackID=3";
	currentMediaInfo->controlURI="rtsp://192.168.1.10:554/user=admin_password=cinzia_channel=3_stream=1.sdp";
	}


BYTE *CRTSPClientSocket::GetMediaPacket(MediaSession *media_session, BYTE *buf, size_t *size) {

	if(!media_session) 
		return NULL;
	return media_session->GetMediaPacket(buf, size);
	}

BYTE *CRTSPClientSocket::GetMediaPacket(CString media_type, BYTE *buf, size_t *size) {
	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;
	bool IgnoreCase = true;

	for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		if(Regex.Regex(it->first, media_type, IgnoreCase)) 
			break;
		}

	if(it == MediaSessionMap->end()) {
//		fprintf(stderr, "%s: No such media session\n", __func__);
		return NULL;
		}

	return it->second->GetMediaPacket(buf, size);
	}

DWORD CRTSPClientSocket::CheckAuth(CString cmd, CString uri) {

  /* RFC2617 */
	return 1;
	}

ErrorType CRTSPClientSocket::DoOPTIONS(CString uri, bool http_tunnel_no_response) {
	CString RtspUri;
//https://en.wikipedia.org/wiki/Real_Time_Streaming_Protocol

	if(!uri.IsEmpty()) {
		RtspURI=uri;
		RtspUri=uri;
		}
	else if(!RtspURI.IsEmpty()) 
		RtspUri=RtspURI;
	else 
		return RTSP_INVALID_URI;

	if(!isConnected)
		if(!Connect(RtspUri)) 
			return RTSP_INVALID_URI;

//	RtspUri += "/user=admin_password=cinzia_channel=1_stream=0.sdp";
//	RtspUri += "/stream=0.sdp";


	CString Cmd("OPTIONS");
	CString Msg,S;
	Msg = Cmd + " " + "rtsp://";
	Msg += RtspURI + " " + "RTSP/" + VERSION_RTSP + CWebSrvSocket2_base::CRLF;
	Msg += "CSeq: ";
	S.Format("%u",++RtspCSeq);
	Msg += S+CWebSrvSocket2_base::CRLF2;
    // cout << "DEBUG: " << Msg.str() << endl;

  int ret=Send(Msg);
  if(ret<=0) {
		Disconnect();
    return RTSP_SEND_ERROR;
    }
	// if(RTSP_NO_ERROR != SendRTSP(Sockfd, Msg.str())) {
	// 	// close(Sockfd);
	// 	Close(Sockfd);
	// 	return RTSP_SEND_ERROR;
	// }
	if(http_tunnel_no_response) {
		return RTSP_NO_ERROR;
		}
	char myBuf[1024];
	RtspResponse.Empty();
  ret = ReadHeader(myBuf,1024);
	// se serve auth restituisce WWW-Authenticate: Digest realm=...
  if(ret<0) {
		Disconnect();
    return RTSP_RECV_ERROR;
    }
	RtspResponse=myBuf;
			theApp.FileSpool->print(CLogFile::flagInfo,"  DOOPTIONS header: %s",RtspResponse);
// check username and password, if any
	if(CheckAuth(Cmd, RtspUri) != CHECK_OK) {
//		cout << "CheckAuth: error" << endl;
		Close();
		Disconnect();
		return RTSP_RESPONSE_401;
		}

	// if(RTSP_NO_ERROR != RecvRTSP(Sockfd, &RtspResponse)) {
	// 	// close(Sockfd);
	// 	Close(Sockfd);
	// 	return RTSP_RECV_ERROR;
	// }
	// close(Sockfd);
//	Disconnect();
	return RTSP_NO_ERROR;
	}

ErrorType CRTSPClientSocket::DoPAUSE(CString media_type, bool http_tunnel_no_response) {
	ErrorType Err = RTSP_NO_ERROR;
	bool IgnoreCase = true;
	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;

	for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		if(Regex.Regex(it->first, media_type, IgnoreCase)) 
//		if(!it->first.CompareNoCase(media_type)) 
			break;
		}

	if(it != MediaSessionMap->end()) {
		Err = DoPAUSE(it->second, http_tunnel_no_response);
		return Err;
		}

	Err = RTSP_INVALID_MEDIA_SESSION;
	return Err;
	}

ErrorType CRTSPClientSocket::DoPAUSE(MediaSession *media_session, bool http_tunnel_no_response) {

	if(!media_session) {
		return RTSP_INVALID_MEDIA_SESSION;
		}

	if(!isConnected)
		if(!Connect(RtspURI)) 
			return RTSP_INVALID_URI;

	CString Cmd("PAUSE");
	CString Msg,S;
	Msg = Cmd + " " + RtspURI + " " + "RTSP/" + VERSION_RTSP + CWebSrvSocket2_base::CRLF;
	Msg += "CSeq: ";
	S.Format("%u",++RtspCSeq);
	Msg += S+CWebSrvSocket2_base::CRLF;
	S.Format("%u",media_session->SessionID);
	Msg += "Session: " + S + CWebSrvSocket2_base::CRLF2;

	int ret = Send(Msg);
  if(ret<=0) {
    return RTSP_SEND_ERROR;
    }
	if(http_tunnel_no_response) {
		return RTSP_NO_ERROR;
		}
	char myBuf[1024];
	RtspResponse.Empty();
  ret = ReadHeader(myBuf,1024);
  if(ret<=0) {
    return RTSP_RECV_ERROR;
    }
	RtspResponse=myBuf;
	// check username and password, if any
	if(CheckAuth(Cmd, RtspURI) != CHECK_OK) {
//		cout << "CheckAuth: error" << endl;
		Close();
		Disconnect();
		return RTSP_RESPONSE_401;
		}

	// if(RTSP_NO_ERROR == Err && RTSP_NO_ERROR != SendRTSP(Sockfd, Msg.str())) {
	// 	Close(Sockfd);
	// 	// close(Sockfd);
	// 	Sockfd = -1;
	// 	Err = RTSP_SEND_ERROR;
	// }
	//if(RTSP_NO_ERROR == Err && RTSP_NO_ERROR != RecvRTSP(Sockfd, &RtspResponse)) {
	//	Close(Sockfd);
	//	// close(Sockfd);
	//	Sockfd = -1;
	//	Err = RTSP_RECV_ERROR;
	//}
	// close(Sockfd);
	return RTSP_NO_ERROR;
	}

ErrorType CRTSPClientSocket::DoGET_PARAMETER() {
	ErrorType Err = RTSP_NO_ERROR;
	ErrorType ErrAll = RTSP_NO_ERROR;

	for(map<CString, MediaSession *>::iterator it = MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		Err = DoGET_PARAMETER(it->second, false);
		if(RTSP_NO_ERROR == ErrAll) 
			ErrAll = Err; // Remember the first error
		printf("GET_PARAMETER Session %s: %s\n", it->first, ParseError(Err));
		}

	return ErrAll;
	}

ErrorType CRTSPClientSocket::DoGET_PARAMETER(MediaSession *media_session, bool http_tunnel_no_response) {

	if(!media_session) {
		return RTSP_INVALID_MEDIA_SESSION;
		}
	// ErrorType Err = RTSP_NO_ERROR;

	if(!isConnected)
		if(!Connect(RtspURI)) 
			return RTSP_INVALID_URI;
	
	CString Cmd("GET_PARAMETER");
	CString Msg,S;
	Msg = Cmd + " " + RtspURI + " " + "RTSP/" + VERSION_RTSP + CWebSrvSocket2_base::CRLF;
	Msg += "CSeq: ";
	S.Format("%u",++RtspCSeq);
	Msg += S+CWebSrvSocket2_base::CRLF;
	S.Format("%u",media_session->SessionID);
	Msg += "Session: " + S + CWebSrvSocket2_base::CRLF2;

	int ret = Send(Msg);
  if(ret<=0) {
		Disconnect();
		return RTSP_SEND_ERROR;
    }
	if(http_tunnel_no_response) {
		return RTSP_NO_ERROR;
		}
	char myBuf[1024];
	RtspResponse.Empty();
  if(ReadHeader(myBuf,1024)<0) {
//    ret = ReadHeader(RtspOverHttpDataPort, &RtspResponse);
//    if(RTSP_NO_ERROR != ret) {
		return RTSP_RECV_ERROR;
    }
	RtspResponse=myBuf;
			theApp.FileSpool->print(CLogFile::flagInfo,"  DOGETPARAMETER header: %s, content=%u",RtspResponse,ret);
	// check username and password, if any
	if(CheckAuth(Cmd, RtspURI) != CHECK_OK) {
//		cout << "CheckAuth: error" << endl;
		Close();
		Disconnect();
		return RTSP_RESPONSE_401;
		}

//	Disconnect();
	return RTSP_NO_ERROR;
	}

ErrorType CRTSPClientSocket::DoGET_PARAMETER(CString media_type, bool http_tunnel_no_response) {
	bool IgnoreCase = true;
	ErrorType Err = RTSP_NO_ERROR;
	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;

	for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		if(!it->first.CompareNoCase(media_type)) 
			break;
		}

	if(it != MediaSessionMap->end()) {
		Err = DoGET_PARAMETER(it->second, http_tunnel_no_response);
		return Err;
		}

	Err = RTSP_INVALID_MEDIA_SESSION;
	return Err;
	}

ErrorType CRTSPClientSocket::DoSETUP() {
	MyRegex Regex;
	ErrorType Err = RTSP_NO_ERROR;
	ErrorType ErrAll = RTSP_NO_ERROR;

	for(map<CString, MediaSession *>::iterator it = MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		Err = DoSETUP(it->second, false);
		if(RTSP_NO_ERROR == ErrAll) 
			ErrAll = Err; // Remember the first error
			theApp.FileSpool->print(CLogFile::flagInfo,("Setup Session %s: %s\n", it->first, ParseError(Err)));
		}

	return ErrAll;
	}

ErrorType CRTSPClientSocket::DoSETUP(MediaSession *media_session, bool rtp_over_tcp, bool http_tunnel_no_response) {

	if(!media_session) {
		return RTSP_INVALID_MEDIA_SESSION;
		}

	if(!isConnected)
		if(!Connect(RtspURI)) 
			return RTSP_INVALID_URI;
	
	// "CreateUdpSockfd" is only for test. 
	// We will use jrtplib instead later. 
	if(!SetAvailableRTPPort(media_session)) {
//		printf("No port available for RTP and RTCP\n");
		return RTSP_UNKNOWN_ERROR;
		}
	CString Cmd("SETUP");
	CString Msg,S;
	Msg = Cmd + " " + media_session->ControlURI + " " + "RTSP/" + VERSION_RTSP + CWebSrvSocket2_base::CRLF;
//	Msg = Cmd + " " + "rtsp://192.168.1.10:554/user=admin_password=cinzia_channel=1_stream=0.sdp/trackID=3" + " " + "RTSP/" + VERSION_RTSP + CWebSrvSocket2_base::CRLF;
//	Msg += HttpHeadUserAgent + HttpHeadUserAgentContent + CWebSrvSocket2_base::CRLF;
  if(RtspOverHttpDataPort > 0) {
	  Msg += "Transport:" + media_session->Protocol + "/TCP;";
    Msg += "interleaved=0-1\r\n";
	  } 
	else if(rtp_over_tcp) {
	  Msg += "Transport:" + media_session->Protocol + "/TCP;";
    Msg += "interleaved=0-1\r\n";
		} 
	else {
//	  Msg += "Transport:" + media_session->Protocol + "/UDP;";
	  Msg += "Transport:" + media_session->Protocol + "/UDP;";
		S.Format(" %u",media_session->RTPPort);
    Msg += "unicast;";
    Msg += "client_port=" + S;
		S.Format(" %u",media_session->RTCPPort);
		Msg += "-" + S + CWebSrvSocket2_base::CRLF;
    }
	S.Format("%u",++RtspCSeq);
	Msg += "CSeq: " + S + CWebSrvSocket2_base::CRLF;
	if(!Realm.IsEmpty() && !Nonce.IsEmpty()) {
        /* digest auth */
		CString RealmTmp = Realm;
		CString NonceTmp = Nonce;
		CString Md5Response  = MakeMd5DigestResp(RealmTmp, Cmd, media_session->ControlURI, NonceTmp);
		if(Md5Response.GetLength() != MD5_SIZE) {
//			cout << "Make MD5 digest response error" << endl;
			return RTSP_RESPONSE_401;
			}
		Msg += "Authorization: Digest username=\"" + Username + "\", realm=\""
			+ RealmTmp + "\", nonce=\"" + NonceTmp + "\", uri=\"" + media_session->ControlURI
			+ "\", response=\"" + Md5Response + "\r\n";
		} 
	else if(!Realm.IsEmpty()) {
    // basic auth 
    CString BasicResponse = MakeBasicResp();
    Msg += "Authorization: Basic " + BasicResponse;
    }
	Msg += CWebSrvSocket2_base::CRLF;
    // cout << "DEBUG: " << Msg.str() << endl;


//Msg="SETUP rtsp://192.168.1.10/stream0.sdp RTSP/1.0\nCSeq: 3\nTransport: RTP/AVP;unicast;client_port=8000-8001\n\n";


	int ret = Send(Msg);
  if(ret<=0) {
		Disconnect();
		return RTSP_SEND_ERROR;
    }
	if(http_tunnel_no_response) {
		return RTSP_NO_ERROR;
		}
	char myBuf[1024];
	RtspResponse.Empty();
  if(ReadHeader(myBuf,1024)<0) {
//  ret = ReadHeader(&RtspResponse);
//    if(RTSP_NO_ERROR != ret) {
		Disconnect();
		return RTSP_RECV_ERROR;
    }
	RtspResponse=myBuf;
			theApp.FileSpool->print(CLogFile::flagInfo,"  DOSETUP header: %s, content=%u",RtspResponse,ret);
	// check username and password, if any
	if(CheckAuth(Cmd, RtspURI) != CHECK_OK) {
//		cout << "CheckAuth: error" << endl;
		Close();
		Disconnect();
		return RTSP_RESPONSE_401;
		}

	// if(CheckAuth(Sockfd, Cmd, media_session->ControlURI) != CHECK_OK) {
	// 	cout << "CheckAuth: error" << endl;
	// 	close(Sockfd);
	// 	return RTSP_RESPONSE_401;
	// }
	media_session->SessionID = ParseSessionID(RtspResponse);
	int timeout = ParseTimeout(RtspResponse);
	if(timeout <= 0) {
		// default 60
		media_session->Timeout=60;
		}
	media_session->Timeout=timeout;

   
  if(rtp_over_tcp) {
    media_session->RTP_SetUp(this);
		} 
	else {
    media_session->RTP_SetUp(RtspOverHttpDataSockfd);
    }
	SetDestroiedClbk("audio", ByeFromServerAudioClbk);
	SetDestroiedClbk("video", ByeFromServerVideoClbk);

	media_session->rtspsock = this;
	// close(Sockfd);
//	Disconnect();
	return RTSP_NO_ERROR;
	}

ErrorType CRTSPClientSocket::DoSETUP(CString media_type, bool rtp_over_tcp) {
	ErrorType Err = RTSP_NO_ERROR;
	bool IgnoreCase = true;
	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;

	for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		if(!it->first.CompareNoCase(media_type)) 
			break;
		}

	if(it != MediaSessionMap->end()) {
		Err = DoSETUP(it->second, rtp_over_tcp, false);
		return Err;
		}

	Err = RTSP_INVALID_MEDIA_SESSION;
	return Err;
	}

ErrorType CRTSPClientSocket::DoPLAY() {
	ErrorType Err = RTSP_NO_ERROR;
	ErrorType ErrAll = RTSP_NO_ERROR;

	for(map<CString, MediaSession *>::iterator it = MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		Err = DoPLAY(it->second, NULL, NULL, NULL, false);
		if(RTSP_NO_ERROR == ErrAll) 
			ErrAll = Err; // Remember the first error
		theApp.FileSpool->print(CLogFile::flagInfo,"  PLAY Session %s: %s\n", it->first, ParseError(Err));
		}

	return ErrAll;
	}

ErrorType CRTSPClientSocket::DoPLAY(MediaSession *media_session, float *scale, float *start_time, float *end_time, bool http_tunnel_no_response) {

	if(!media_session) {
		return RTSP_INVALID_MEDIA_SESSION;
		}

	// ErrorType Err = RTSP_NO_ERROR;
	if(!isConnected)
		if(!Connect(RtspURI)) 
			return RTSP_INVALID_URI;

	CString Cmd("PLAY");
	CString Msg,S;
	Msg = Cmd + " " + RtspURI + " " + "RTSP/" + VERSION_RTSP + CWebSrvSocket2_base::CRLF;
	if(scale) {
		char floatChar[32];
		sprintf(floatChar, "%.1f", *scale);
		Msg += "Scale: ";
		Msg += floatChar;
		Msg += CWebSrvSocket2_base::CRLF;
		}
	if(start_time || end_time) {
		char floatChar[32];
		Msg += "Range: npt=";
		if(start_time) {
			sprintf(floatChar, "%.1f", *start_time);
			Msg += floatChar;
			}
		Msg += "-";
		if(end_time) {
			sprintf(floatChar, "%.1f", *end_time);
			Msg += floatChar;
			}
		Msg += CWebSrvSocket2_base::CRLF;
		}
	Msg += "CSeq: ";
	S.Format("%u",++RtspCSeq);
	Msg += S+CWebSrvSocket2_base::CRLF;
	S.Format("%u",media_session->SessionID);
	Msg += "Session: " + S + CWebSrvSocket2_base::CRLF;
//	Msg += HttpHeadUserAgent + HttpHeadUserAgentContent + CWebSrvSocket2_base::CRLF;
	if(!Realm.IsEmpty() && !Nonce.IsEmpty()) {
        /* digest auth */
		CString RealmTmp = Realm;
		CString NonceTmp = Nonce;
		CString Md5Response  = MakeMd5DigestResp(RealmTmp, Cmd, RtspURI,  NonceTmp);
		if(Md5Response.GetLength() != MD5_SIZE) {
//			cout << "Make MD5 digest response error" << endl;
			return RTSP_RESPONSE_401;
			}
		Msg += "Authorization: Digest username=\"" + Username + "\", realm=\""
			+ RealmTmp + "\", nonce=\"" + NonceTmp + "\", uri=\"" + RtspURI
			+ "\", response=\"" + Md5Response + "\r\n";
		} 
	else if(!Realm.IsEmpty()) {
        /* basic auth */
    CString BasicResponse = MakeBasicResp();
    Msg += "Authorization: Basic " + BasicResponse;
    }
	Msg += CWebSrvSocket2_base::CRLF;
    // cout << "DEBUG: " << Msg.str();

	int ret=Send(Msg);
  if(ret<=0) {
		Disconnect();
    return RTSP_SEND_ERROR;
    }
	if(http_tunnel_no_response) {
		return RTSP_NO_ERROR;
		}
	char myBuf[1024];
	RtspResponse.Empty();
  if(ReadHeader(myBuf,1024)<0) {
//    ret = ReadHeader(&RtspResponse);
//    if(RTSP_NO_ERROR != ret) {
		Disconnect();
    return RTSP_RECV_ERROR;
    }
	RtspResponse=myBuf;
			theApp.FileSpool->print(CLogFile::flagInfo,"  DOPLAY header: %s, content=%u",RtspResponse,ret);
	// check username and password, if any
	if(CheckAuth(Cmd, RtspURI) != CHECK_OK) {
//		cout << "CheckAuth: error" << endl;
		Close();
		return RTSP_RESPONSE_401;
		}

//	Disconnect();
	return RTSP_NO_ERROR;
	}

ErrorType CRTSPClientSocket::DoPLAY(CString media_type, float *scale, float *start_time, float *end_time, bool http_tunnel_no_response) {
	ErrorType Err = RTSP_NO_ERROR;
	bool IgnoreCase = true;
	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;

	for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		if(!it->first.CompareNoCase(media_type)) 
			break;
		}

	if(it != MediaSessionMap->end()) {
		Err = DoPLAY(it->second, scale, start_time, end_time, http_tunnel_no_response);
		return Err;
		}

	Err = RTSP_INVALID_MEDIA_SESSION;
	return Err;
	}

ErrorType CRTSPClientSocket::DoTEARDOWN() {
	ErrorType Err = RTSP_NO_ERROR;
	ErrorType ErrAll = RTSP_NO_ERROR;

	Err = DoTEARDOWN("audio");
	if(RTSP_NO_ERROR == ErrAll) 
		ErrAll = Err; // Remember the first error
	Err = DoTEARDOWN("video");
	if(RTSP_NO_ERROR == ErrAll) 
		ErrAll = Err; // Remember the first error

	return ErrAll;
	}

ErrorType CRTSPClientSocket::DoTEARDOWN(MediaSession *media_session, bool http_tunnel_no_response) {

	if(!media_session) {
		return RTSP_INVALID_MEDIA_SESSION;
		// return RTSP_NO_ERROR;
		}
	// ErrorType Err = RTSP_NO_ERROR;

	// cout << "TEST: TEARDOWN: ###" << media_session->MediaType << "###" << endl;

	if(!isConnected)
		if(!Connect(RtspURI)) 
			return RTSP_INVALID_URI;
	
	CString Cmd("TEARDOWN");
	CString Msg,S;
	Msg = Cmd + " " + RtspURI + " " + "RTSP/" + VERSION_RTSP + CWebSrvSocket2_base::CRLF;
	Msg += "CSeq: ";
	S.Format("%u",++RtspCSeq);
	Msg += S+CWebSrvSocket2_base::CRLF;
	S.Format("%u",media_session->SessionID);
	Msg += "Session: " + S + CWebSrvSocket2_base::CRLF;
	if(!Realm.IsEmpty() && !Nonce.IsEmpty()) {
        /* digest auth */
		CString RealmTmp = Realm;
		CString NonceTmp = Nonce;
		CString Md5Response  = MakeMd5DigestResp(RealmTmp, Cmd, RtspURI,  NonceTmp);
		if(Md5Response.GetLength() != MD5_SIZE) {
//			cout << "Make MD5 digest response error" << endl;
			return RTSP_RESPONSE_401;
		}
		Msg += "Authorization: Digest username=\"" + Username + "\", realm=\""
			+ RealmTmp + "\", nonce=\"" + NonceTmp + "\", uri=\"" + RtspURI
			+ "\", response=\"" + Md5Response + "\r\n";
		} 
	else if(!Realm.IsEmpty()) {
    /* basic auth */
    CString BasicResponse = MakeBasicResp();
    Msg += "Authorization: Basic " + BasicResponse;
    }
	Msg += CWebSrvSocket2_base::CRLF;

	int ret = Send(Msg);
  if(ret<=0) {
		Disconnect();
		return RTSP_SEND_ERROR;
    }
	if(http_tunnel_no_response) {
		return RTSP_NO_ERROR;
		}
	char myBuf[1024];
	RtspResponse.Empty();
  if(ReadHeader(myBuf,1024)<0) {
//  ret = ReadHeader(&RtspResponse);
//  if(RTSP_NO_ERROR != ret) {
		Disconnect();
		return RTSP_RECV_ERROR;
    }
	RtspResponse=myBuf;
			theApp.FileSpool->print(CLogFile::flagInfo,"  DOTEARDOWN header: %s, content=%u",RtspResponse,ret);
	// check username and password, if any
	if(CheckAuth(Cmd, RtspURI) != CHECK_OK) {
//		cout << "CheckAuth: error" << endl;
		Disconnect();
		return RTSP_RESPONSE_401;
		}

//	if(RTSP_NO_ERROR == ret) {
		map<CString, MediaSession *>::iterator it;
		for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
			if(media_session->SessionID == it->second->SessionID) 
				break;
			}
		if(it != MediaSessionMap->end()) {
			delete it->second;
			MediaSessionMap->erase(it);
			}
//		}
//	Disconnect();

	return RTSP_NO_ERROR;
	}

ErrorType CRTSPClientSocket::DoTEARDOWN(CString media_type, bool http_tunnel_no_response) {
	bool IgnoreCase = true;
	ErrorType Err = RTSP_NO_ERROR;
	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;

	for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		if(!it->first.CompareNoCase(media_type)) 
			break;
		}

	if(it != MediaSessionMap->end()) {
		Err = DoTEARDOWN(it->second, http_tunnel_no_response);
		return Err;
		}

	Err = RTSP_INVALID_MEDIA_SESSION;
	return Err;
	}

ErrorType CRTSPClientSocket::DoRtspOverHttpGet() {

	if(!isConnected)
	  if(!Connect(RtspURI)) 
			return RTSP_INVALID_URI;

  UpdateXSessionCookie();

	CString Cmd("GET");
	CString Msg;
	Msg = Cmd + " " + GetResource() + " " + "HTTP/" + VERSION_HTTP + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadUserAgent + HttpHeadUserAgentContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadXSessionCookie + HttpHeadXSessionCookieContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadAccept+ HttpHeadAcceptContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadPrama+ HttpHeadPramaContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadCacheControl + HttpHeadCacheControlContent + CWebSrvSocket2_base::CRLF;
	Msg += CWebSrvSocket2_base::CRLF;
    // cout << "DEBUG: " << Msg.str();

	if(Send(Msg)<=0) {
		Close();
		return RTSP_SEND_ERROR;
		}
	char myBuf[1024];
	RtspResponse.Empty();
  if(ReadHeader(myBuf,1024)<0) {
//	if(RTSP_NO_ERROR != ReadHeader(&RtspResponse)) {
		Disconnect();
		return RTSP_RECV_ERROR;
		}
	RtspResponse=myBuf;
			theApp.FileSpool->print(CLogFile::flagInfo,"  DORTSPGET header: %s",RtspResponse);

	return RTSP_NO_ERROR;
	}

ErrorType CRTSPClientSocket::DoRtspOverHttpPost() {

	if(!isConnected)
		if(!Connect(RtspURI)) 
			return RTSP_INVALID_URI;

	CString Cmd("POST");
	CString Msg;
	Msg = Cmd + " " + GetResource() + " " + "HTTP/" + VERSION_HTTP + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadUserAgent + HttpHeadUserAgentContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadXSessionCookie + HttpHeadXSessionCookieContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadContentType + HttpHeadContentTypeContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadPrama+ HttpHeadPramaContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadCacheControl + HttpHeadCacheControlContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadContentLength + HttpHeadContentLengthContent + CWebSrvSocket2_base::CRLF;
	Msg += HttpHeadExpires + HttpHeadExpiresContent + CWebSrvSocket2_base::CRLF;
	Msg += CWebSrvSocket2_base::CRLF;
    // cout << "DEBUG: " << Msg.str();

	if(Send(Msg)<=0) {
		Disconnect();
		return RTSP_SEND_ERROR;
		}
	// if(RTSP_NO_ERROR != RecvRTSP(Sockfd, &RtspResponse)) {
	// 	Close(Sockfd);
	// 	return RTSP_RECV_ERROR;
	// }

  return RTSP_NO_ERROR;
	}

void CRTSPClientSocket::UpdateXSessionCookie() {
  time_t timep;
	char habuf[MD5_BUF_SIZE] = {0};

  time(&timep);
	/*
	Md5sum32((void *)&timep, (unsigned char *)habuf, sizeof(timep), MD5_BUF_SIZE);
	habuf[23] = '\0';
  HttpHeadXSessionCookieContent = string(habuf);
	*/
	}

int CRTSPClientSocket::SetAvailableRTPPort(MediaSession *media_session, WORD RTP_port) {
	int RTPSockfd, RTCPSockfd;
	WORD RTPPort;
	WORD RTCPPort;
	struct sockaddr_in servaddr;
	WORD Search_RTP_Port_From;

	if(0 != RTP_port && (RTP_port % 2 == 0)) {
		Search_RTP_Port_From = RTP_port;
		}
	else {
		Search_RTP_Port_From = SEARCH_PORT_RTP_FROM;
		}
	media_session->RTPPort = 0;
	// media_session->RTPSockfd = -1;
	media_session->RTCPPort = 0;
	// media_session->RTCPSockfd = -1;

	// Create RTP and RTCP udp socket 
	for(RTPPort=Search_RTP_Port_From; RTPPort < 65535; RTPPort+=2) {
		// Bind RTP Port
		if((RTPSockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("CreateRTP_RTCPSockfd Error");
			return 0;
			}
		// if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, len) < 0) { 
		// 	close(RTPPort);
		// 	perror("CreateRTP_RTCPSockfd Error");
		// 	return 0;
		// } 
		ZeroMemory(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(RTPPort);
		if(bind(RTPSockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
			closesocket(RTPSockfd);
			continue;
			}

		// Bind RTCP Port
		RTCPPort = RTPPort+1;
		if((RTCPSockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			closesocket(RTPSockfd);
			perror("CreateRTP_RTCPSockfd Error");
			return 0; // Create failed
			}	
		// if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, len) < 0) { 
		// 	close(RTPPort);
		// 	close(RTCPPort);
		// 	perror("CreateRTP_RTCPSockfd Error");
		// 	return 0;
		// } 
		ZeroMemory(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(RTCPPort);

		if(bind(RTCPSockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
			closesocket(RTPSockfd);
			closesocket(RTCPSockfd);
			continue;
			}
		closesocket(RTPSockfd);
		closesocket(RTCPSockfd);
		media_session->RTPPort = RTPPort;
		// media_session->RTPSockfd = RTPSockfd;;
		media_session->RTCPPort = RTCPPort;
		// media_session->RTCPSockfd = RTCPSockfd;
		return 1; // Created successfully
		}

	return 0; // Created failed
	}

CString CRTSPClientSocket::ParseSessionID(CString ResponseOfSETUP) {
	CString Response;
	CString Result;
	char *p;
//	MyRegex Regex;

	if(!ResponseOfSETUP.IsEmpty()) 
		Response=ResponseOfSETUP;
	else if(!RtspResponse.IsEmpty()) 
		Response=RtspResponse;
	else 
		return Result;

	if(p=(char *)CWebSrvSocket2_base::stristr(Response,"Session:")) {
		Result.Format("%u",atoi(p+8));
		}

/*
	// Session: 970756dc30b3a638;timeout=60
	string Pattern("Session: +([0-9a-zA-Z_\\$-.\\..\\+]+)");
	list<string> Group;
	bool IgnoreCase = true;
	if(Regex.Regex(Response.c_str(), Pattern.c_str(), &Group, IgnoreCase)) {
		Group.pop_front();
		Result.assign(Group.front());
		}
	*/
	return Result;
	}

int CRTSPClientSocket::ParseTimeout(CString ResponseOfSETUP) {
	CString Response;
	int Result = -1;
	char *p;
//	MyRegex Regex;

	if(!ResponseOfSETUP.IsEmpty()) 
		Response=ResponseOfSETUP;
	else if(!RtspResponse.IsEmpty()) 
		Response=RtspResponse;
	else 
		return Result;

	if(p=(char *)CWebSrvSocket2_base::stristr(Response,"timeout=")) {
		Result=atoi(p+8);
		}

/*
	// Session: 970756dc30b3a638;timeout=60
	string Pattern("Session:.*timeout=([1-9][0-9]*)");
	list<string> Group;
	bool IgnoreCase = true;
	if(Regex.Regex(Response.c_str(), Pattern.c_str(), &Group, IgnoreCase)) {
		Group.pop_front();
		Result =  atoi(Group.front().c_str());
		}
		*/
	return Result;
	}

CString CRTSPClientSocket::GetResource(CString uri) {
/*	//### example uri: rtsp://192.168.15.100/test ###//
	MyRegex Regex;
	*/
	CString RtspUri;
	/*
	// string Pattern("rtsp://([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})");
	CString Pattern("rtsp://.+(/.+)");
	// string Pattern("rtsp://192.168.1.143(/ansersion)");
	list<string> Groups;
	*/
	char *p;

	if(!uri.IsEmpty()) {
		RtspUri=uri;
		RtspURI=uri;
		}
	else if(!RtspURI.IsEmpty()) 
		RtspUri=RtspURI;
	else 
		return "";

	if(p=(char *)CWebSrvSocket2_base::stristr(RtspUri,"rtsp://")) {
		return p+7;
		}
	/*
	if(!Regex.Regex(RtspUri.c_str(), Pattern.c_str(), &Groups, IgnoreCase)) {
		return "";
	}
	Groups.pop_front();
	return Groups.front();
	*/
	return "";
	}

WORD CRTSPClientSocket::GetPort(CString uri) {
	//### example uri: rtsp://192.168.15.100:554/test ###//
	CString RtspUri;
	char *p;

	if(!uri.IsEmpty()) {
		RtspUri=uri;
		RtspURI=uri;
		}
	else if(!RtspURI.IsEmpty()) 
		RtspUri=RtspURI;
	else 
		return RtspPort;

	if(p=(char *)strchr(RtspUri,':')) {
		if(isdigit(p[1]))		// scanso rtsp: , in caso...
			return atoi(p);
		else
			if(p=(char *)strchr(p+1,':'))
				return atoi(p);
		}
	return RtspPort;
	}

void CRTSPClientSocket::SetDestroiedClbk(MediaSession *media_session, DESTROIED_CLBK clbk) {

	if(media_session) {
		media_session->SetRtpDestroiedClbk(clbk);
		}
	}

void CRTSPClientSocket::SetDestroiedClbk(CString media_type, DESTROIED_CLBK clbk) {
	bool IgnoreCase = true;
//	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;

	for(it=MediaSessionMap->begin(); it!=MediaSessionMap->end(); it++) {
		if(!it->first.CompareNoCase(media_type)) 
			break;
		}
	if(it == MediaSessionMap->end()) {
		// fprintf(stderr, "%s: No such media session\n", __func__);
		return;
		}

	it->second->SetRtpDestroiedClbk(clbk);
	}

CString CRTSPClientSocket::MakeBasicResp(CString username, CString password) {

	if(username.IsEmpty()) {
		username=Username;
		password=Password;
		}
  CString S = username + ":" + password;
	char encUserPasw[256];

	CStringEx S2=S;

	S2.Encode64();
/*  char *encodedBytes = base64Encode(tmp.c_str(), tmp.length());
  if(NULL == encodedBytes) {
    return "";
    }
  tmp.assign(encodedBytes);
  delete[] encodedBytes;
	*/
	S=S2;

  return S;
	}

CString CRTSPClientSocket::MakeMd5DigestResp(CString realm, CString cmd, CString uri, CString nonce, CString username, CString password) {
	CString tmp;
	char ha1buf[MD5_BUF_SIZE] = {0};
	char ha2buf[MD5_BUF_SIZE] = {0};
	char habuf[MD5_BUF_SIZE] = {0};

	if(username.IsEmpty()) {
		username=Username;
		password=Password;
		}

/*	tmp = username + ":" + realm + ":" + password;
	Md5sum32((void *)tmp.c_str(), (unsigned char *)ha1buf, tmp.GetLength(), MD5_BUF_SIZE);
	ha1buf[MD5_SIZE] = '\0';

	tmp = cmd + ":" + uri;
	Md5sum32((void *)tmp.c_str(), (unsigned char *)ha2buf, tmp.GetLength(), MD5_BUF_SIZE);
	ha2buf[MD5_SIZE] = '\0';
	
	tmp=ha1buf;
	tmp += ":" + nonce + ":" + ha2buf;
	Md5sum32((void *)tmp.c_str(), (unsigned char *)habuf, tmp.GetLength(), MD5_BUF_SIZE);
	habuf[MD5_SIZE] = '\0';
*/
	tmp=habuf;

	return tmp;

	}


BYTE *CRTSPClientSocket::GetMediaData(MediaSession *media_session, BYTE *buf, size_t *size, size_t max_size) {

 	if(!media_session) 
		return NULL;
	return media_session->GetMediaData(buf, size, max_size);
	}

BYTE *CRTSPClientSocket::GetMediaData(CString media_type, BYTE *buf, size_t *size, size_t max_size) {
//	bool IgnoreCase = true;
//	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;

	if(!buf) 
		return NULL;
	if(!size) 
		return NULL;

	*size = 0;

  it=MediaSessionMap->find(media_type);
  if(it == MediaSessionMap->end()) {
    for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
			if(!it->first.CompareNoCase(media_type)) 
				break;
      }
    }

	if(it == MediaSessionMap->end()) {
//		fprintf(stderr, "%s: No such media session\n", __func__);
		return NULL;
		}

	return GetMediaData(it->second, buf, size, max_size);
	// if(it->second->MediaType == "video") return GetMediaData2(it->second, buf, size, max_size);
	// if(it->second->MediaType == "audio") return GetAudioData(it->second, buf, size, max_size);
	}

BYTE *CRTSPClientSocket::GetMediaFrame(CString media_type, BYTE *buf, size_t *size, size_t max_size) {
//	bool IgnoreCase = true;
//	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;
	struct RTPHeader *pack;
	BYTE mybuf[2048];
	size_t size2;
	BYTE *p=buf+4;
	BYTE NAL;

	for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		if(!it->first.CompareNoCase(media_type)) 
			break;
		}

	if(it == MediaSessionMap->end())
		return NULL;

	buf[0]=0x00;
	buf[1]=0x00;
	buf[2]=0x00;
	buf[3]=0x00;
	struct FU_A *fua=(struct FU_A*)&mybuf[12];
	do {
		if(!it->second->GetMediaPacket(mybuf,&size2))
			break;
		pack=(struct RTPHeader*)&mybuf;
		NAL=mybuf[12] & 0xe0 /*fua->F fua->NRI*/ | fua->h_type;
		switch(fua->i_type) {		// https://stackoverflow.com/questions/11543839/h-264-conversion-with-ffmpeg-from-a-rtp-stream
			case H264TypeInterfaceFU_A::_SPS /*7*/: //SPS
			case H264TypeInterfaceFU_A::_PPS /*8*/: //PPS
			case H264TypeInterfaceFU_A::_SEI /*6*/: //SEI
				buf[3]=0x01;
				break;
			case H264TypeInterfaceFU_A::_FU_A_ID /*28*/: //video fragment
			case H264TypeInterfaceFU_A::_FU_B_ID /*29*/: //video fragment
				buf[3]=0x01;
				break;
			case 1: //video fragment (a seguire?) o corto (1 datagram
				buf[3]=0x01;
				break;
			default: //?
				buf[3]=0x01;
				break;
			}
		switch(fua->i_type) {
			case H264TypeInterfaceFU_A::_FU_A_ID:	//  video 
				if(fua->S) {			// se il primo del gruppo...
					*p=NAL;		//0x65   mybuf[12] & 0xe0 | mybuf[13] & 0x1f;
					memcpy(p+1,mybuf  +14,size2-14);		// salto header
					p+=size2-13;
					}
				else {
					memcpy(p,mybuf  +14,size2-14);		// salto header
					p+=size2-14;
					}
				break;
			case 1:	//  video 
				*p=0x61 /*NAL ma verrebbe 0x60 e non va bene... */;		//0x61 mybuf[12] & 0xe0 | mybuf[13] & 0x1f;
				memcpy(p+1,mybuf  +13,size2-13);		// salto header
				p+=size2-12;
				break;
			default:	//non-video
				memcpy(p,mybuf  +12,size2-12);		// 
				p+=size2-12;
				break;
			}
// https://stackoverflow.com/questions/9618369/h-264-over-rtp-identify-sps-and-pps-frames
		} while(!fua->E /*pack->marker*/);
//	*p++=0; *p++=0; *p++=0; *p++=0;		// marker di fine/next NAL, per decoder H264...
	*size=p-buf;
	return buf;
	}

int CRTSPClientSocket::GetTimeRate(CString media_type) {
//	bool IgnoreCase = true;
//	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;

	for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		if(!it->first.CompareNoCase(media_type)) 
			break;
		}

	if(it == MediaSessionMap->end())
		return -1;

	return it->second->TimeRate;
	}

int CRTSPClientSocket::GetChannelNum(CString media_type) {
//	bool IgnoreCase = true;
//	MyRegex Regex;
	map<CString, MediaSession *>::iterator it;

	for(it=MediaSessionMap->begin(); it != MediaSessionMap->end(); it++) {
		if(!it->first.CompareNoCase(media_type)) 
			break;
		}

	if(it == MediaSessionMap->end())
		return -1;

	return it->second->ChannelNum;
	}

void CRTSPClientSocket::SetAudioByeFromServerClbk(DESTROIED_CLBK clbk) {

	ByeFromServerAudioClbk = clbk;
	SetDestroiedClbk("audio", clbk);
	}

void CRTSPClientSocket::SetVideoByeFromServerClbk(DESTROIED_CLBK clbk) {

	ByeFromServerVideoClbk = clbk;
	SetDestroiedClbk("video", clbk);
	}

CString CRTSPClientSocket::ParseError(ErrorType et) {
	CString ErrStr;

	switch(et) {
		case RTSP_NO_ERROR:
			ErrStr="MyRtsp: Success";
			break;
		case RTSP_INVALID_URI:
			ErrStr="MyRtsp: Invalid URI";
			break;
		case RTSP_SEND_ERROR:
			ErrStr="MyRtsp: send error";
			break;
		case RTSP_RECV_ERROR:
			ErrStr="MyRtsp: recv error";
			break;
		case RTSP_INVALID_MEDIA_SESSION:
			ErrStr="MyRtsp: invalid media session error";
			break;
		case RTSP_RESPONSE_BLANK:
			ErrStr="MyRtsp: Response BLANK";
			break;
		case RTSP_RESPONSE_200:
			ErrStr="MyRtsp: Response 200 OK";
			break;
		case RTSP_RESPONSE_400:
			ErrStr="MyRtsp: Response 400 Bad Request";
			break;
		case RTSP_RESPONSE_401:
			ErrStr="MyRtsp: Response 401 Unauthorized";
			break;
		case RTSP_RESPONSE_404:
			ErrStr="MyRtsp: Response 404 Not Found";
			break;
		case RTSP_RESPONSE_40X:
			ErrStr="MyRtsp: Response Client Error";
			break;
		case RTSP_RESPONSE_500:
			ErrStr="MyRtsp: Response 500 Internal Server Error";
			break;
		case RTSP_RESPONSE_501:
			ErrStr="MyRtsp: Response 501 Not Implemented";
			break;
		case RTSP_RESPONSE_50X:
			ErrStr="MyRtsp: Response Server Error";
			break;
		case RTSP_UNKNOWN_ERROR:
			ErrStr="MyRtsp: Unknown Error";
			break;
		default:
			ErrStr="MyRtsp: Unknown Error";
			break;
		}
	return ErrStr;
	}


FrameTypeBase *FrameTypeBase::CreateNewFrameType(CString &EncodeType) {
  FrameTypeBase *frame_type_base=NULL;

/*  if(!EncodeType.Compare(NALUTypeBase_H264::ENCODE_TYPE)) {
    frame_type_base = new NALUTypeBase_H264();
		} else if(!EncodeType.Compare(NALUTypeBase_H265::ENCODE_TYPE)) {
      frame_type_base = new NALUTypeBase_H265();
		} else if(!EncodeType.Compare(PCMU_Audio::ENCODE_TYPE)) {
      frame_type_base = new PCMU_Audio();
		} else if(!EncodeType.Compare(MPEG_Audio::ENCODE_TYPE)) {
      frame_type_base = new MPEG_Audio();
		}
*/
  return frame_type_base;
	}


void FrameTypeBase::DestroyFrameType(FrameTypeBase *frameTypeBase) {

  if(!frameTypeBase) {
    delete frameTypeBase;
    frameTypeBase=NULL;
    }
	}


// RTP TCP UDP session -----------------------------------------------------------------------------------------

#define USLEEP_UNIT 	10 /* 10000 qua sono millisecondi! */

//using namespace jrtplib;


//
// This function checks if there was a RTP error. If so, it displays an error message and exists.
//

MyRTPTCPSession::MyRTPTCPSession() : MyRTPSession() {

  TunnellingSock = 0;
	SocketMutex = CreateMutex(NULL,              // default security attributes
    FALSE,             // initially not owned
    NULL);             // unnamed mutex
//  pthread_mutex_init(&SocketMutex, NULL);
  TrylockTimes = 0;
	}

MyRTPTCPSession::~MyRTPTCPSession() {

  CloseHandle /*pthread_mutex_destroy*/(SocketMutex);
	}

int MyRTPTCPSession::MyRTP_SetUp(MediaSession *media_session, CSocket *tunnelling_sock) {

	if(!media_session) {
//		fprintf(stderr, "%s: Invalid media session\n", __func__);
		return RTP_ERROR;
		}
	if(0 == media_session->TimeRate) {
//		fprintf(stderr, "%s: Invalid MediaSession::TimeRate\n", __func__);
		return RTP_ERROR;
		}

	int status;

	// Now, we'll create a RTP session, set the destination
	// and poll for incoming data.

	RTPSessionParams sessparams;
	RTPTCPTransmissionParams transparams;

	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be just use 8000 samples per second.
	sessparams.SetOwnTimestampUnit(1.0/media_session->TimeRate);         

	sessparams.SetAcceptOwnPackets(true);

	sessparams.SetProbationType(RTPSources::NoProbation);
	// TODO: use a valueable instead of "65535"
	// sessparams.SetMaximumPacketSize(65535);

	//         bool threadsafe = false;
	// #ifdef RTP_SUPPORT_THREAD
	//         threadsafe = true;
	// #endif // RTP_SUPPORT_THREAD
	//         transparams.Init(threadsafe);
	// TODO: use a valueable instead of "65535"
	// transparams.Create(65535, 0);

	// status = Create(sessparams,&transparams, RTPTransmitter::TCPProto);  
	status = MyTcpCreate(sessparams,&transparams);  
  TunnellingSock = tunnelling_sock;
	AddDestination(RTPTCPAddress(tunnelling_sock));
	return IsError(status);
	}

void MyRTPTCPSession::MyRTP_Teardown(MediaSession *media_session, struct timeval *tval) {
	struct timeval Timeout;

	if(!tval) {
		Timeout.tv_sec = 1; 
		Timeout.tv_usec = 0; 
		} 
	else {
		Timeout.tv_sec = tval->tv_sec;
		Timeout.tv_usec = tval->tv_usec;
		}

	media_session->RTPPort = 0;
	BYEDestroy(RTPTime(Timeout.tv_sec, Timeout.tv_usec), 0, 0);
	}

BYTE *MyRTPTCPSession::GetMyRTPData(BYTE *data_buf, size_t *size, unsigned long timeout_ms) {

	if(!data_buf) {
//		fprintf(stderr, "%s: Invalid argument('data_buf==NULL')", __func__);
		return NULL;
		}

	if(!size) {
//		fprintf(stderr, "%s: Invalid argument('size==NULL')", __func__);
		return NULL;
		}
  *size = 0;

	unsigned long UsleepTimes = (timeout_ms + USLEEP_UNIT - 1) / USLEEP_UNIT; // floor the 'timeout_ms / USLEEP_UNIT'

	do {
#ifndef RTP_SUPPORT_THREAD
		if(TryLockSocket()) {
			int status = Poll();
			// printf("DEBUG: end poll: %d\n", status);
			if(!IsError(status)) 
				return NULL;
			UnlockSocket();
		}
#endif 

		BeginDataAccess();

		// check incoming packets
		if(!GotoFirstSourceWithData()) {
			EndDataAccess();
			Sleep(USLEEP_UNIT);
			UsleepTimes--;
      if(UsleepTimes <= 0) {
        break;
        }
			continue;
			// return NULL;
			}

		RTPPacket *pack;
		if(!(pack = GetNextPacket()))	{
			EndDataAccess();
			Sleep(USLEEP_UNIT);
			UsleepTimes--;
      if(UsleepTimes <= 0) {
        break;
        }
			continue;
			// return NULL;
			}

		size_t PacketSize;
		BYTE *Packet;
		Packet = pack->GetPayloadData();
		PacketSize = pack->GetPayloadLength();
		// printf("DEBUG: get packet: %d\n", PacketSize);
		//for(int i=0; i < PacketSize; i++) {
		//	printf("%x ", Packet[i]);
		//}
		//printf("\n");
		// printf("data length: %lu\t", PacketSize);
		// printf("%02x %02x %02x %02x\n", Packet[0], Packet[1], Packet[2], Packet[3]);

		*size = PacketSize;
		memcpy(data_buf, Packet, PacketSize);

		// we don't longer need the packet, so we'll delete it
		DeletePacket(pack);
		EndDataAccess();
		UsleepTimes = 0; // Got the data. So not need to sleep any more.
		} while(UsleepTimes > 0);

	return data_buf;
	}

BYTE *MyRTPTCPSession::GetMyRTPPacket(BYTE *packet_buf, size_t *size, unsigned long timeout_ms) {

	if(!packet_buf) {
//		fprintf(stderr, "%s: Invalid argument('packet_buf==NULL')", __func__);
		return NULL;
		}

	if(!size) {
//		fprintf(stderr, "%s: Invalid argument('size==NULL')", __func__);
		return NULL;
		}
  *size = 0;

	unsigned long UsleepTimes = (timeout_ms + USLEEP_UNIT - 1) / USLEEP_UNIT; // floor the 'timeout_ms / USLEEP_UNIT'

	do {
#ifndef RTP_SUPPORT_THREAD
		int status = Poll();
		if(!IsError(status)) 
			return NULL;
#endif 

		BeginDataAccess();

		// check incoming packets
		if(!GotoFirstSourceWithData()) {
			EndDataAccess();
			Sleep(USLEEP_UNIT);
			UsleepTimes--;
      if(UsleepTimes <= 0) {
        break;
        }
			continue;
			// return NULL;
			}

		RTPPacket *pack;
		if(!(pack = GetNextPacket()))	{
			EndDataAccess();
			Sleep(USLEEP_UNIT);
			UsleepTimes--;
            if(UsleepTimes <= 0) {
                break;
            }
			continue;
			// return NULL;
			}

		size_t PacketSize;
		BYTE *Packet;
		Packet = pack->GetPacketData();
		PacketSize = pack->GetPacketLength();
		// printf("packet length: %lu\n", PacketSize);

		*size = PacketSize;
		memcpy(packet_buf, Packet, PacketSize);

		// we don't longer need the packet, so
		// we'll delete it
		DeletePacket(pack);
		EndDataAccess();
		UsleepTimes = 0;
		} while(UsleepTimes > 0);

	return packet_buf;
	}

void MyRTPTCPSession::LockSocket() {

//  pthread_mutex_lock(&SocketMutex);
  /*dwWaitResult = */ WaitForSingleObject(SocketMutex,    // handle to mutex
    INFINITE);  // no time-out interval
	}

void MyRTPTCPSession::UnlockSocket() {

  ReleaseMutex /*pthread_mutex_unlock*/(SocketMutex);
	}

bool MyRTPTCPSession::TryLockSocket() {

//  if(0 != pthread_mutex_trylock(&SocketMutex)) {
	if(WAIT_OBJECT_0 != WaitForSingleObject(SocketMutex,    // handle to mutex
    INFINITE)) {  // no time-out interval
    TrylockTimes++;
    if(TrylockTimes > 10) {
      printf("WARNING: There are some RTSP packages unreceived\n");
			}
    return false;
		}
  if(TrylockTimes > 0) {
    TrylockTimes--;
		}
  return true;
	}


int MyRTPTCPSession::MyTcpCreate(const RTPSessionParams &sessparams, const RTPTransmissionParams *transparams) {
	int status;
	
	if(created)
		return ERR_RTP_SESSION_ALREADYCREATED;

	usingpollthread = sessparams.IsUsingPollThread();
	needthreadsafety = sessparams.NeedThreadSafety();
	if(usingpollthread && !needthreadsafety)
		return ERR_RTP_SESSION_THREADSAFETYCONFLICT;

	useSR_BYEifpossible = sessparams.GetSenderReportForBYE();
	sentpackets = false;
	
	// Check max packet size
	
	if((maxpacksize = sessparams.GetMaximumPacketSize()) < RTP_MINPACKETSIZE)
		return ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL;
		
	// Initialize the transmission component
	rtptrans = NULL;
//  rtptrans = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPTRANSMITTER) MyTCPTransmitter(GetMemoryManager());
  rtptrans = (RTPTransmitter*)new MyTCPTransmitter();
	if(!rtptrans)
		return ERR_RTP_OUTOFMEM;
	if((status = rtptrans->Init(needthreadsafety)) < 0) {
//		delete rtptrans;
		delete rtptrans;
		return status;
		}
	if((status = rtptrans->Create(maxpacksize,transparams)) < 0) {
//		delete rtptrans;
		delete rtptrans;
		return status;
		}

	deletetransmitter = true;
	return InternalCreate(sessparams);
	}

void MyRTPTCPSession::OnNewSource(RTPSourceData *dat) {
	// if(dat->IsOwnSSRC())
	// 	return;

	// DWORD ip;
	// WORD port;

	// if(dat->GetRTPDataAddress() != 0)
	// {
	// 	const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
	// 	ip = addr->GetIP();
	// 	port = addr->GetPort();
	// }
	// else if(dat->GetRTCPDataAddress() != 0)
	// {
	// 	const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
	// 	ip = addr->GetIP();
	// 	port = addr->GetPort()-1;
	// }
	// else
	// 	return;

	// RTPIPv4Address dest(ip,port);
	// AddDestination(dest);

	// struct in_addr inaddr;
	// inaddr.s_addr = htonl(ip);
//	std::cout << "Adding destination" << std::endl;
	}

void MyRTPTCPSession::OnBYEPacket(RTPSourceData *dat) {
	// if(dat->IsOwnSSRC())
	// 	return;

	// DWORD ip;
	// WORD port;

	// if(dat->GetRTPDataAddress() != 0)
	// {
	// 	const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
	// 	ip = addr->GetIP();
	// 	port = addr->GetPort();
	// }
	// else if(dat->GetRTCPDataAddress() != 0)
	// {
	// 	const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
	// 	ip = addr->GetIP();
	// 	port = addr->GetPort()-1;
	// }
	// else
	// 	return;

	// RTPIPv4Address dest(ip,port);
	// DeleteDestination(dest);

	// struct in_addr inaddr;
	// inaddr.s_addr = htonl(ip);
//	std::cout << "Deleting destination" << std::endl;
	if(DestroiedClbk) {
		DestroiedClbk();
		} 
	}

void MyRTPTCPSession::OnRemoveSource(RTPSourceData *dat) {
	// if(dat->IsOwnSSRC())
	// 	return;
	// if(dat->ReceivedBYE())
	// 	return;

	// DWORD ip;
	// WORD port;

	// if(dat->GetRTPDataAddress() != 0)
	// {
	// 	const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
	// 	ip = addr->GetIP();
	// 	port = addr->GetPort();
	// }
	// else if(dat->GetRTCPDataAddress() != 0)
	// {
	// 	const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
	// 	ip = addr->GetIP();
	// 	port = addr->GetPort()-1;
	// }
	// else
	// 	return;

	// RTPIPv4Address dest(ip,port);
	// DeleteDestination(dest);

	// struct in_addr inaddr;
	// inaddr.s_addr = htonl(ip);
//	std::cout << "Deleting destination" << std::endl;
	if(DestroiedClbk) {
		DestroiedClbk();
		}
	}

void MyRTPTCPSession::SetRecvRtspCmdClbk(void (*clbk)(char *cmd)) {

  MyTCPTransmitter *session = static_cast<MyTCPTransmitter *>((MyTCPTransmitter *)rtptrans);
  if(session)
    session->SetRecvRtspCmdClbk(clbk);
	}


MyRTPUDPSession::MyRTPUDPSession() : MyRTPSession(), DestroiedClbk(NULL) {

	}

int MyRTPUDPSession::MyRTP_SetUp(MediaSession *media_session) {

	if(!media_session) {
//		fprintf(stderr, "%s: Invalid media session\n", __func__);
		return RTP_ERROR;
		}
	if(0        && 0 == media_session->TimeRate) {		// idem patch 8/1/26
//		fprintf(stderr, "%s: Invalid MediaSession::TimeRate\n", __func__);
		return RTP_ERROR;
		}
	if(0 == media_session->RTPPort) {
//		fprintf(stderr, "%s: Invalid MediaSession::RTPPort\n", __func__);
		return RTP_ERROR;
		}

	int status;

	// Now, we'll create a RTP session, set the destination and poll for incoming data.
	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;

	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be just use 8000 samples per second.
	sessparams.SetOwnTimestampUnit(1.0/media_session->TimeRate);         

	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(media_session->RTPPort);
	transparams.SetRTPReceiveBuffer(100000);
	status = Create(sessparams,&transparams);  
	// printf("DEBUG: create udpsession\n");
	return IsError(status);
	}

void MyRTPUDPSession::MyRTP_Teardown(MediaSession *media_session, struct timeval *tval) {
	struct timeval Timeout;

	if(!tval) {
		Timeout.tv_sec = 1; 
		Timeout.tv_usec = 0; 
		} 
	else {
		Timeout.tv_sec = tval->tv_sec;
		Timeout.tv_usec = tval->tv_usec;
		}

	media_session->RTPPort = 0;
	BYEDestroy(RTPTime(Timeout.tv_sec, Timeout.tv_usec), 0, 0);
	}

void MyRTPUDPSession::OnNewSource(RTPSourceData *dat) {

	if(dat->IsOwnSSRC())
		return;

	DWORD ip;
	WORD port;

	if(dat->GetRTPDataAddress() != 0)	{
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort();
		}
	else if(dat->GetRTCPDataAddress() != 0) {
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort()-1;
		}
	else
		return;

	RTPIPv4Address dest(ip,port);
	AddDestination(dest);

	struct in_addr inaddr;
	inaddr.s_addr = htonl(ip);
//	std::cout << "Adding destination " << std::string(inet_ntoa(inaddr)) << ":" << port << std::endl;
	}

void MyRTPUDPSession::OnBYEPacket(RTPSourceData *dat) {

	if(dat->IsOwnSSRC())
		return;

	DWORD ip;
	WORD port;

	if(dat->GetRTPDataAddress() != 0) {
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort();
		}
	else if(dat->GetRTCPDataAddress() != 0) {
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort()-1;
		}
	else
		return;

	RTPIPv4Address dest(ip,port);
	DeleteDestination(dest);

	struct in_addr inaddr;
	inaddr.s_addr = htonl(ip);
//	std::cout << "Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port << std::endl;
	if(DestroiedClbk) {
		DestroiedClbk();
		} 
	}

void MyRTPUDPSession::OnRemoveSource(RTPSourceData *dat) {

	if(dat->IsOwnSSRC())
		return;
	if(dat->ReceivedBYE())
		return;

	DWORD ip;
	WORD port;

	if(dat->GetRTPDataAddress() != 0) {
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort();
		}
	else if(dat->GetRTCPDataAddress() != 0) {
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort()-1;
		}
	else
		return;

	RTPIPv4Address dest(ip,port);
	DeleteDestination(dest);

	struct in_addr inaddr;
	inaddr.s_addr = htonl(ip);
//	std::cout << "Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port << std::endl;
	if(DestroiedClbk)
		DestroiedClbk();
	}

BYTE *MyRTPUDPSession::GetMyRTPData(BYTE *data_buf, size_t *size, unsigned long timeout_ms) {

	if(!data_buf) {
//		fprintf(stderr, "%s: Invalide argument('data_buf==NULL')", __func__);
		return NULL;
		}

	if(!size) {
//		fprintf(stderr, "%s: Invalide argument('size==NULL')", __func__);
		return NULL;
		}

	unsigned long UsleepTimes = (timeout_ms + USLEEP_UNIT - 1) / USLEEP_UNIT; // floor the 'timeout_ms / USLEEP_UNIT'
    *size = 0;

	do {
#ifndef RTP_SUPPORT_THREAD
		int status = Poll();
		// printf("DEBUG: end poll: %d\n", status);
		if(!IsError(status)) 
			return NULL;
#endif 

		BeginDataAccess();

		// check incoming packets
		if(!GotoFirstSourceWithData()) {
			EndDataAccess();
			Sleep(USLEEP_UNIT);
			UsleepTimes--;
      if(UsleepTimes <= 0) {
        break;
        }
			continue;
			// return NULL;
			}

		RTPPacket *pack;
		if(!(pack = GetNextPacket())) 		{
			EndDataAccess();
			Sleep(USLEEP_UNIT);
			UsleepTimes--;
      if(UsleepTimes <= 0) {
        break;
        }
			continue;
			// return NULL;
			}

		size_t PacketSize;
		BYTE *Packet;
		Packet = pack->GetPayloadData();
		PacketSize = pack->GetPayloadLength();
		// printf("DEBUG: get packet: %d\n", PacketSize);
		//for(int i=0; i < PacketSize; i++) {
		//	printf("%x ", Packet[i]);
		//}
		//printf("\n");
		// printf("data length: %lu\n", PacketSize);

		*size = PacketSize;
		memcpy(data_buf, Packet, PacketSize);

		// we don't longer need the packet, so we'll delete it
		DeletePacket(pack);
		EndDataAccess();
		UsleepTimes = 0; // Got the data. So not need to sleep any more.
		} while(UsleepTimes > 0);

	return data_buf;
	}

BYTE *MyRTPUDPSession::GetMyRTPPacket(BYTE *packet_buf, size_t *size, unsigned long timeout_ms) {

	if(!packet_buf) {
//		fprintf(stderr, "%s: Invalide argument('packet_buf==NULL')", __func__);
		return NULL;
		}

	if(!size) {
//		fprintf(stderr, "%s: Invalide argument('size==NULL')", __func__);
		return NULL;
		}
  *size = 0;

	unsigned long UsleepTimes = (timeout_ms + USLEEP_UNIT - 1) / USLEEP_UNIT; // floor the 'timeout_ms / USLEEP_UNIT'

	do {
#ifndef RTP_SUPPORT_THREAD
		int status = Poll();
		if(!IsError(status)) 
			return NULL;
#endif 

		BeginDataAccess();

		// check incoming packets
		if(!GotoFirstSourceWithData()) {
			EndDataAccess();
			Sleep(USLEEP_UNIT);
			UsleepTimes--;
      if(UsleepTimes <= 0) {
        break;
				}
			continue;
			// return NULL;
			}

		RTPPacket *pack;
		if(!(pack = GetNextPacket()))	{
			EndDataAccess();
			Sleep(USLEEP_UNIT);
			UsleepTimes--;
      if(UsleepTimes <= 0) {
        break;
        }
			continue;
			// return NULL;
			}

		size_t PacketSize;
		BYTE *Packet;
		Packet = pack->GetPacketData();
		PacketSize = pack->GetPacketLength();
		// printf("packet length: %lu\n", PacketSize);

		*size = PacketSize;
		memcpy(packet_buf, Packet, PacketSize);

		// we don't longer need the packet, so we'll delete it
		DeletePacket(pack);
		EndDataAccess();
		UsleepTimes = 0;
		} while(UsleepTimes > 0);

	return packet_buf;
	}

void MyRTPUDPSession::OnPollThreadError(int) {
	}

void MyRTPUDPSession::OnPollThreadStep() {
	}

void MyRTPUDPSession::OnPollThreadStart(bool &) {
    printf("RTP Poll start\n");
	}

void MyRTPUDPSession::OnPollThreadStop() {

    printf("RTP Poll stop\n");
	}



RTPSession::RTPSession(/*RTPRandom * */ int r /*,RTPMemoryManager *mgr*/) : 
	/*RTPMemoryObject(mgr),*/rndseed( /*GetRandomNumberGenerator(*/r/*)*/),sources(*this/*,mgr*/),
	packetbuilder(::rand()/*,mgr*/),rtcpsched(sources,::rand()),
	  rtcpbuilder(sources,packetbuilder/*,mgr*/),collisionlist(/*mgr*/) {

	// We're not going to set these flags in Create, so that the constructor of a derived class
	// can already change them
	m_changeIncomingData = false;
	m_changeOutgoingData = false;

	created = false;
//	timeinit.Dummy();

	//std::cout << (void *)(rtprnd) << std::endl;
	}

int RTPSession::SendRTCPData(const void *data, size_t len) {

	if(!m_changeOutgoingData)
		return rtptrans->SendRTCPData(data, len);

	void *pSendData = 0;
	size_t sendLen = 0;
	int status = 0;

	status = OnChangeRTPOrRTCPData(data, len, false, &pSendData, &sendLen);
	if(status < 0)
		return status;

	if(pSendData)	{
		status = rtptrans->SendRTCPData(pSendData, sendLen);
		OnSentRTPOrRTCPData(pSendData, sendLen, false);
		}

	return status;
	}

RTPSession::~RTPSession() {

	Destroy();

//	if(deletertprnd)
//		delete rtprnd;
	}

int RTPSession::AddDestination(const RTPAddress &addr) {
	
	if(!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->AddDestination(addr);
	}

int RTPSession::DeleteDestination(const RTPAddress &addr) {
	
	if(!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->DeleteDestination(addr);
	}

void RTPSession::ClearDestinations() {
	
	if(!created)
		return;
	rtptrans->ClearDestinations();
	}

int RTPSession::EndDataAccess() {

	if(!created)
		return ERR_RTP_SESSION_NOTCREATED;
	SOURCES_UNLOCK
	return 0;
	}

int RTPSession::Poll() {
	int status;
	
	// printf("DEBUG: !create\n");
	if(!created)
		return ERR_RTP_SESSION_NOTCREATED;
	// printf("DEBUG: usingpollthread\n");
	if(usingpollthread)
		return ERR_RTP_SESSION_USINGPOLLTHREAD;
	// printf("DEBUG: status < 0\n");
	if((status = rtptrans->Poll()) < 0)
		return status;
	// printf("DEBUG: ProcessPolledData < 0\n");
	return ProcessPolledData();
	}

int RTPSession::BeginDataAccess() {

	if(!created)
		return ERR_RTP_SESSION_NOTCREATED;
	SOURCES_LOCK
	return 0;
	}

bool RTPSession::GotoFirstSourceWithData() {

	if(!created)
		return false;
	return sources.GotoFirstSourceWithData();
	}

int RTPSession::Create(const RTPSessionParams &sessparams,const RTPTransmissionParams *transparams /* = 0 */,
		       RTPTransmitter::TransmissionProtocol protocol) {
	int status;
	
	if(created)
		return ERR_RTP_SESSION_ALREADYCREATED;

	usingpollthread = sessparams.IsUsingPollThread();
	needthreadsafety = sessparams.NeedThreadSafety();
	if(usingpollthread && !needthreadsafety)
		return ERR_RTP_SESSION_THREADSAFETYCONFLICT;

	useSR_BYEifpossible = sessparams.GetSenderReportForBYE();
	sentpackets = false;
	
	// Check max packet size
	
	if((maxpacksize = sessparams.GetMaximumPacketSize()) < RTP_MINPACKETSIZE)
		return ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL;
		
	// Initialize the transmission component
	
	rtptrans = 0;
	switch(protocol) {
		case RTPTransmitter::IPv4UDPProto:
			rtptrans = new RTPUDPv4Transmitter();
			break;
	#ifdef RTP_SUPPORT_IPV6
		case RTPTransmitter::IPv6UDPProto:
			rtptrans = new RTPUDPv6Transmitter();
			break;
	#endif // RTP_SUPPORT_IPV6
		case RTPTransmitter::ExternalProto:
			rtptrans = new RTPExternalTransmitter();
			break;
		case RTPTransmitter::UserDefinedProto:
	/* GD		rtptrans = NewUserDefinedTransmitter();
			if(rtptrans == 0)
				return ERR_RTP_SESSION_USERDEFINEDTRANSMITTERNULL;
				*/
				return ERR_RTP_SESSION_USERDEFINEDTRANSMITTERNULL;
			break;
		case RTPTransmitter::TCPProto:
			rtptrans = new RTPTCPTransmitter();
			break;
		default:
			return ERR_RTP_SESSION_UNSUPPORTEDTRANSMISSIONPROTOCOL;
		}
	
	if(!rtptrans)
		return ERR_RTP_OUTOFMEM;
	if((status = rtptrans->Init(needthreadsafety)) < 0)	{
		delete rtptrans; rtptrans=NULL;
		return status;
		}
	if((status = rtptrans->Create(maxpacksize,transparams)) < 0)	{
		delete rtptrans; rtptrans=NULL;
		return status;
		}

	deletetransmitter = true;
	return InternalCreate(sessparams);
	}

int RTPSession::Create(const RTPSessionParams &sessparams,RTPTransmitter *transmitter) {
	int status;
	
	if(created)
		return ERR_RTP_SESSION_ALREADYCREATED;

	usingpollthread = sessparams.IsUsingPollThread();
	needthreadsafety = sessparams.NeedThreadSafety();
	if(usingpollthread && !needthreadsafety)
		return ERR_RTP_SESSION_THREADSAFETYCONFLICT;

	useSR_BYEifpossible = sessparams.GetSenderReportForBYE();
	sentpackets = false;
	
	// Check max packet size
	
	if((maxpacksize = sessparams.GetMaximumPacketSize()) < RTP_MINPACKETSIZE)
		return ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL;
		
	rtptrans = transmitter;

	if((status = rtptrans->SetMaximumPacketSize(maxpacksize)) < 0)
		return status;

	deletetransmitter = false;
	return InternalCreate(sessparams);
	}

int RTPSession::InternalCreate(const RTPSessionParams &sessparams) {
	int status;

	// Initialize packet builder
	
	if((status = packetbuilder.Init(maxpacksize)) < 0)	{
		if(deletetransmitter)
			delete rtptrans;
		return status;
		}

	if(sessparams.GetUsePredefinedSSRC())
		packetbuilder.AdjustSSRC(sessparams.GetPredefinedSSRC());

#ifdef RTP_SUPPORT_PROBATION

	// Set probation type
	sources.SetProbationType(sessparams.GetProbationType());

#endif // RTP_SUPPORT_PROBATION

	// Add our own ssrc to the source table
	if((status = sources.CreateOwnSSRC(packetbuilder.GetSSRC())) < 0)	{
		packetbuilder.Destroy();
		if(deletetransmitter)
			delete rtptrans;
		return status;
		}

	// Set the initial receive mode
	if((status = rtptrans->SetReceiveMode(sessparams.GetReceiveMode())) < 0)	{
		packetbuilder.Destroy();
		sources.Clear();
		if(deletetransmitter)
			delete rtptrans;
		return status;
		}

	// Init the RTCP packet builder
	double timestampunit = sessparams.GetOwnTimestampUnit();
	BYTE buf[1024];
	size_t buflen = 1024;
	std::string forcedcname = sessparams.GetCNAME(); 

	if(forcedcname.length() == 0)	{
		if((status = CreateCNAME(buf,&buflen,sessparams.GetResolveLocalHostname())) < 0)		{
			packetbuilder.Destroy();
			sources.Clear();
			if(deletetransmitter)
				delete rtptrans;
			return status;
			}
		}
	else	{
		_tcsncpy((char *)buf, forcedcname.c_str(), buflen);
		buf[buflen-1] = 0;
		buflen = strlen((char *)buf);
		}
	
	if((status = rtcpbuilder.Init(maxpacksize,timestampunit,buf,buflen)) < 0)	{
		packetbuilder.Destroy();
		sources.Clear();
		if(deletetransmitter)
			delete rtptrans;
		return status;
		}

	// Set scheduler parameters
	
	rtcpsched.Reset();
	rtcpsched.SetHeaderOverhead(rtptrans->GetHeaderOverhead());

	RTCPSchedulerParams schedparams;

	sessionbandwidth = sessparams.GetSessionBandwidth();
	controlfragment = sessparams.GetControlTrafficFraction();
	
	if((status = schedparams.SetRTCPBandwidth(sessionbandwidth*controlfragment)) < 0)	{
		if(deletetransmitter)
			delete rtptrans;
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
		}
	if((status = schedparams.SetSenderBandwidthFraction(sessparams.GetSenderControlBandwidthFraction())) < 0)	{
		if(deletetransmitter)
			delete rtptrans;
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
		}
	if((status = schedparams.SetMinimumTransmissionInterval(sessparams.GetMinimumRTCPTransmissionInterval())) < 0)	{
		if(deletetransmitter)
			delete rtptrans;
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
		}
	schedparams.SetUseHalfAtStartup(sessparams.GetUseHalfRTCPIntervalAtStartup());
	schedparams.SetRequestImmediateBYE(sessparams.GetRequestImmediateBYE());
	
	rtcpsched.SetParameters(schedparams);

	// copy other parameters
	acceptownpackets = sessparams.AcceptOwnPackets();
	membermultiplier = sessparams.GetSourceTimeoutMultiplier();
	sendermultiplier = sessparams.GetSenderTimeoutMultiplier();
	byemultiplier = sessparams.GetBYETimeoutMultiplier();
	collisionmultiplier = sessparams.GetCollisionTimeoutMultiplier();
	notemultiplier = sessparams.GetNoteTimeoutMultiplier();

	// Do thread stuff if necessary
	
#ifdef RTP_SUPPORT_THREAD
	pollthread = 0;
	if(usingpollthread)	{
		if(!sourcesmutex.IsInitialized())	{
			if(sourcesmutex.Init() < 0)	{
				if(deletetransmitter)
					delete rtptrans;
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
				}
			}
		if(!buildermutex.IsInitialized())	{
			if(buildermutex.Init() < 0)	{
				if(deletetransmitter)
					delete rtptrans;
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
				}
			}
		if(!schedmutex.IsInitialized())	{
			if(schedmutex.Init() < 0)	{
				if(deletetransmitter)
					delete rtptrans;
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
				}
			}
		if(!packsentmutex.IsInitialized()) {
			if(packsentmutex.Init() < 0) {
				if(deletetransmitter)
					delete rtptrans;
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
				}
			}
		
		pollthread = new RTPPollThread(*this,rtcpsched);
		if(!pollthread) {
			if(deletetransmitter)
				delete rtptrans;
			packetbuilder.Destroy();
			sources.Clear();
			rtcpbuilder.Destroy();
			return ERR_RTP_OUTOFMEM;
			}
		if((status = pollthread->Start(rtptrans)) < 0) {
			if(deletetransmitter)
				delete rtptrans;
			delete pollthread;
			packetbuilder.Destroy();
			sources.Clear();
			rtcpbuilder.Destroy();
			return status;
			}
		}
#endif // RTP_SUPPORT_THREAD	

	created = true;
	return 0;
	}

void RTPSession::Destroy() {

	if(!created)
		return;

#ifdef RTP_SUPPORT_THREAD
	if(pollthread)
		delete pollthread;
#endif // RTP_SUPPORT_THREAD
	
	if(deletetransmitter)
		delete /*RTPDelete(*/ rtptrans /*,GetMemoryManager())*/;
	packetbuilder.Destroy();
	rtcpbuilder.Destroy();
	rtcpsched.Reset();
	collisionlist.Clear();
	sources.Clear();

	for(POSITION pos = byepackets.GetHeadPosition(); pos; )
		delete byepackets.GetNext(pos);
	byepackets.RemoveAll();
	
	created = false;
	}

void RTPSession::BYEDestroy(const RTPTime &maxwaittime,const void *reason,size_t reasonlength) {

	if(!created)
		return;

	// first, stop the thread so we have full control over all components
	
#ifdef RTP_SUPPORT_THREAD
	if(pollthread)
		delete pollthread;
#endif // RTP_SUPPORT_THREAD

	RTPTime stoptime = RTPTime::CurrentTime();
	stoptime += maxwaittime;

	// add bye packet to the list if we've sent data
	RTCPCompoundPacket *pack;

	if(sentpackets)	{
		int status;
		
		reasonlength = (reasonlength > RTCP_BYE_MAXREASONLENGTH) ? RTCP_BYE_MAXREASONLENGTH : reasonlength;
	       	status = rtcpbuilder.BuildBYEPacket(&pack,reason,reasonlength,useSR_BYEifpossible);
		if(status >= 0) {
			byepackets.AddTail(pack);
			if(byepackets.GetCount() == 1)
				rtcpsched.ScheduleBYEPacket(pack->GetCompoundPacketLength());
			}
		}
	
	if(!byepackets.IsEmpty())	{
		bool done = false;
		
		while(!done)	{
			RTPTime curtime = RTPTime::CurrentTime();
			
			if(curtime >= stoptime)
				done = true;
		
			if(rtcpsched.IsTime()) {
				pack = byepackets.GetHead();
//				byepackets.pop_front();
			
				SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength());
				
				OnSendRTCPCompoundPacket(pack); // we'll place this after the actual send to avoid tampering
				
				delete pack;
				if(!byepackets.IsEmpty()) // more bye packets to send, schedule them
					rtcpsched.ScheduleBYEPacket((byepackets.GetHead())->GetCompoundPacketLength());
				else
					done = true;
				}
			if(!done)
				RTPTime::Wait(RTPTime(0,100000));
			}
		}
	
	if(deletetransmitter)
		delete rtptrans;
	packetbuilder.Destroy();
	rtcpbuilder.Destroy();
	rtcpsched.Reset();
	collisionlist.Clear();
	sources.Clear();

	// clear rest of bye packets
	for(POSITION pos = byepackets.GetHeadPosition(); pos; )
		delete byepackets.GetNext(pos);
	byepackets.RemoveAll();

	created = false;
	}

void RTPSession::DeletePacket(RTPPacket *p) {

	delete /*RTPDelete(*/ p /*,GetMemoryManager())*/;
	}

int RTPSession::ProcessPolledData() {
	RTPRawPacket *rawpack;
	int status;
	
//	printf("DEBUG: SOURCES_LOCK < 0\n");

	SOURCES_LOCK
	while(rawpack = rtptrans->GetNextPacket()) {
		if(m_changeIncomingData) {
			// Provide a way to change incoming data, for decryption for example
			if(!OnChangeIncomingData(rawpack))	{
				delete rawpack;
				continue;
				}
			}

		sources.ClearOwnCollisionFlag();

		// since our sources instance also uses the scheduler (analysis of incoming packets)
		// we'll lock it
		// printf("DEBUG: ProcessRawPacket\n");
		SCHED_LOCK
		if((status = sources.ProcessRawPacket(rawpack,rtptrans,acceptownpackets)) < 0) {
			SCHED_UNLOCK
			SOURCES_UNLOCK
			delete rawpack;
			return status;
			}
		SCHED_UNLOCK
				
		if(sources.DetectedOwnCollision()) {	// collision handling!
			bool created;
			
			if((status = collisionlist.UpdateAddress(rawpack->GetSenderAddress(),rawpack->GetReceiveTime(),&created)) < 0) {
				SOURCES_UNLOCK
				delete rawpack;
				return status;
				}

			if(created) {	// first time we've encountered this address, send bye packet and
				            // change our own SSRC
				PACKSENT_LOCK
				bool hassentpackets = sentpackets;
				PACKSENT_UNLOCK

				if(hassentpackets) {
					// Only send BYE packet if we've actually sent data using this SSRC
					
					RTCPCompoundPacket *rtcpcomppack;

					BUILDER_LOCK
					if((status = rtcpbuilder.BuildBYEPacket(&rtcpcomppack,0,0,useSR_BYEifpossible)) < 0)	{
						BUILDER_UNLOCK
						SOURCES_UNLOCK
						delete rawpack;
						return status;
						}
					BUILDER_UNLOCK

					byepackets.AddTail(rtcpcomppack);
					if(byepackets.GetCount() == 1) // was the first packet, schedule a BYE packet (otherwise there's already one scheduled)
					{
						SCHED_LOCK
						rtcpsched.ScheduleBYEPacket(rtcpcomppack->GetCompoundPacketLength());
						SCHED_UNLOCK
						}
					}
				// bye packet is built and scheduled, now change our SSRC
				// and reset the packet count in the transmitter
				
				BUILDER_LOCK
				DWORD newssrc = packetbuilder.CreateNewSSRC(sources);
				BUILDER_UNLOCK
					
				PACKSENT_LOCK
				sentpackets = false;
				PACKSENT_UNLOCK
	
				// remove old entry in source table and add new one

				if((status = sources.DeleteOwnSSRC()) < 0) {
					SOURCES_UNLOCK
					delete rawpack;
					return status;
					}
				if((status = sources.CreateOwnSSRC(newssrc)) < 0) {
					SOURCES_UNLOCK
					delete rawpack;
					return status;
					}
				}
			}
		// printf("DEBUG: RTPDelete\n");
		delete rawpack;
		}

	SCHED_LOCK
	RTPTime d = rtcpsched.CalculateDeterministicInterval(false);
	SCHED_UNLOCK
	
	RTPTime t = RTPTime::CurrentTime();
	double Td = d.GetDouble();
	RTPTime sendertimeout = RTPTime(Td*sendermultiplier);
	RTPTime generaltimeout = RTPTime(Td*membermultiplier);
	RTPTime byetimeout = RTPTime(Td*byemultiplier);
	RTPTime colltimeout = RTPTime(Td*collisionmultiplier);
	RTPTime notetimeout = RTPTime(Td*notemultiplier);
	
	sources.MultipleTimeouts(t,sendertimeout,byetimeout,generaltimeout,notetimeout);
	collisionlist.Timeout(t,colltimeout);
	
	// We'll check if it's time for RTCP stuff
	SCHED_LOCK
	bool istime = rtcpsched.IsTime();
	SCHED_UNLOCK
	
	if(istime)	{
		RTCPCompoundPacket *pack;
	
		// we'll check if there's a bye packet to send, or just a normal packet
		if(byepackets.IsEmpty())	{
			BUILDER_LOCK
			if((status = rtcpbuilder.BuildNextPacket(&pack)) < 0) {
				BUILDER_UNLOCK
				SOURCES_UNLOCK
				return status;
				}
			BUILDER_UNLOCK
			if((status = SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength())) < 0) {
				SOURCES_UNLOCK
				delete pack;
				return status;
				}
		
			PACKSENT_LOCK
			sentpackets = true;
			PACKSENT_UNLOCK

			OnSendRTCPCompoundPacket(pack); // we'll place this after the actual send to avoid tampering
			}
		else {
			pack = byepackets.GetHead();
			byepackets.RemoveHead();

			if((status = SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength())) < 0) {
				SOURCES_UNLOCK
				delete pack;
				return status;
				}
			
			PACKSENT_LOCK
			sentpackets = true;
			PACKSENT_UNLOCK

			OnSendRTCPCompoundPacket(pack); // we'll place this after the actual send to avoid tampering
			
			if(!byepackets.IsEmpty()) // more bye packets to send, schedule them
			{
				SCHED_LOCK
				rtcpsched.ScheduleBYEPacket((byepackets.GetHead())->GetCompoundPacketLength());
				SCHED_UNLOCK
				}
			}
		
		SCHED_LOCK
		rtcpsched.AnalyseOutgoing(*pack);
		SCHED_UNLOCK

		delete pack;
		}
	SOURCES_UNLOCK

	return 0;
	}

int RTPSession::CreateCNAME(BYTE *buffer,size_t *bufferlength,bool resolve) {
#ifndef WIN32
	bool gotlogin = true;

#ifdef RTP_SUPPORT_GETLOGINR
	buffer[0] = 0;
	if(getlogin_r((char *)buffer,*bufferlength) != 0)
		gotlogin = false;
	else {
		if(buffer[0] == 0)
			gotlogin = false;
		}
	
	if(!gotlogin) // try regular getlogin
	{
		char *loginname = getlogin();
		if(loginname == 0)
			gotlogin = false;
		else
			_tcsncpy((char *)buffer,loginname,*bufferlength);
		}
#else
	char *loginname = getlogin();
	if(loginname == 0)
		gotlogin = false;
	else
		strncpy((char *)buffer,loginname,*bufferlength);
#endif // RTP_SUPPORT_GETLOGINR
	if(!gotlogin) {
		char *logname = getenv("LOGNAME");
		if(logname == 0)
			return ERR_RTP_SESSION_CANTGETLOGINNAME;
		_tcsncpy((char *)buffer,logname,*bufferlength);
		}
#else // Win32 version

#ifndef _WIN32_WCE
	DWORD len = *bufferlength;
	if(!GetUserName((LPTSTR)buffer,&len))
		_tcsncpy((char *)buffer,"unknown",*bufferlength);
#else 
	_tcsncpy((char *)buffer,"unknown",*bufferlength);
#endif // _WIN32_WCE
	
#endif // WIN32
	buffer[*bufferlength-1] = 0;

	size_t offset = strlen((const char *)buffer);
	if(offset < (*bufferlength-1))
		buffer[offset] = (BYTE)'@';
	offset++;

	size_t buflen2 = *bufferlength-offset;
	int status;
	
	if(resolve)	{
		if((status = rtptrans->GetLocalHostName(buffer+offset,&buflen2)) < 0)
			return status;
		*bufferlength = buflen2+offset;
		}
	else {
		char hostname[1024];
		
		_tcsncpy(hostname,"localhost",1024); // just in case gethostname fails

		gethostname(hostname,1024);
		_tcsncpy((char *)(buffer+offset),hostname,buflen2);

		*bufferlength = offset+strlen(hostname);
		}
	if(*bufferlength > RTCP_SDES_MAXITEMLENGTH)
		*bufferlength = RTCP_SDES_MAXITEMLENGTH;
	return 0;
	}


#define ACCEPTPACKETCODE									\
		*accept = true;									\
												\
		sentdata = true;								\
		packetsreceived++;								\
		numnewpackets++;								\
												\
		if(pack->GetExtendedSequenceNumber() == 0)	{				\
			baseseqnr = 0x0000FFFF;							\
			numcycles = 0x00010000;							\
			}										\
		else										\
			baseseqnr = pack->GetExtendedSequenceNumber() - 1;			\
												\
		exthighseqnr = baseseqnr + 1;							\
		prevpacktime = receivetime;							\
		prevexthighseqnr = baseseqnr;							\
		savedextseqnr = baseseqnr;							\
												\
		pack->SetExtendedSequenceNumber(exthighseqnr);					\
												\
		prevtimestamp = pack->GetTimestamp();						\
		lastmsgtime = prevpacktime;							\
		if(!ownpacket) /* for own packet, this value is set on an outgoing packet */	\
			lastrtptime = prevpacktime;

void RTPSourceStats::ProcessPacket(RTPPacket *pack,const RTPTime &receivetime,double tsunit,
                                   bool ownpacket,bool *accept,bool applyprobation,bool *onprobation) {
//	JRTPLIB_UNUSED(applyprobation); // possibly unused

	// Note that the sequence number in the RTP packet is still just the
	// 16 bit number contained in the RTP header
	*onprobation = false;
	
	if(!sentdata) // no valid packets received yet
	{
#ifdef RTP_SUPPORT_PROBATION
		if(applyprobation)	{
			bool acceptpack = false;

			if(probation) {	
				WORD pseq;
				DWORD pseq2;
	
				pseq = prevseqnr;
				pseq++;
				pseq2 = (DWORD)pseq;
				if(pseq2 == pack->GetExtendedSequenceNumber()) // ok, its the next expected packet
				{
					prevseqnr = (WORD)pack->GetExtendedSequenceNumber();
					probation--;	
					if(probation == 0) // probation over
						acceptpack = true;
					else
						*onprobation = true;
					}
				else // not next packet
				{
					probation = RTP_PROBATIONCOUNT;
					prevseqnr = (WORD)pack->GetExtendedSequenceNumber();
					*onprobation = true;
					}
				}
			else // first packet received with this SSRC ID, start probation
			{
				probation = RTP_PROBATIONCOUNT;
				prevseqnr = (WORD)pack->GetExtendedSequenceNumber();	
				*onprobation = true;
			}
	
			if(acceptpack)	{
				ACCEPTPACKETCODE
				}
			else {
				*accept = false;
				lastmsgtime = receivetime;
				}
			}
		else // No probation
		{
			ACCEPTPACKETCODE
			}
#else // No compiled-in probation support

		ACCEPTPACKETCODE

#endif // RTP_SUPPORT_PROBATION
		}
	else // already got packets
	{
		WORD maxseq16;
		DWORD extseqnr;

		// Adjust max extended sequence number and set extende seq nr of packet

		*accept = true;
		packetsreceived++;
		numnewpackets++;

		maxseq16 = (WORD)(exthighseqnr&0x0000FFFF);
		if(pack->GetExtendedSequenceNumber() >= maxseq16)	{
			extseqnr = numcycles+pack->GetExtendedSequenceNumber();
			exthighseqnr = extseqnr;
			}
		else {
			WORD dif1,dif2;

			dif1 = ((WORD)pack->GetExtendedSequenceNumber());
			dif1 -= maxseq16;
			dif2 = maxseq16;
			dif2 -= ((WORD)pack->GetExtendedSequenceNumber());
			if(dif1 < dif2) {
				numcycles += 0x00010000;
				extseqnr = numcycles+pack->GetExtendedSequenceNumber();
				exthighseqnr = extseqnr;
				}
			else
				extseqnr = numcycles+pack->GetExtendedSequenceNumber();
			}

		pack->SetExtendedSequenceNumber(extseqnr);

		// Calculate jitter
		if(tsunit > 0)
		{
#if 0
			RTPTime curtime = receivetime;
			double diffts1,diffts2,diff;

			curtime -= prevpacktime;
			diffts1 = curtime.GetDouble()/tsunit;	
			diffts2 = (double)pack->GetTimestamp() - (double)prevtimestamp;
			diff = diffts1 - diffts2;
			if(diff < 0)
				diff = -diff;
			diff -= djitter;
			diff /= 16.0;
			djitter += diff;
			jitter = (DWORD)djitter;
#else
			RTPTime curtime = receivetime;
			double diffts1,diffts2,diff;
			DWORD curts = pack->GetTimestamp();

			curtime -= prevpacktime;
			diffts1 = curtime.GetDouble()/tsunit;	

			if(curts > prevtimestamp) {
				DWORD unsigneddiff = curts - prevtimestamp;

				if(unsigneddiff < 0x10000000) // okay, curts realy is larger than prevtimestamp
					diffts2 = (double)unsigneddiff;
				else {
		// wraparound occurred and curts is actually smaller than prevtimestamp
					unsigneddiff = -unsigneddiff; // to get the actual difference (in absolute value)
					diffts2 = -((double)unsigneddiff);
					}
				}
			else if(curts < prevtimestamp)	{
				DWORD unsigneddiff = prevtimestamp - curts;

				if(unsigneddiff < 0x10000000) // okay, curts really is smaller than prevtimestamp
					diffts2 = -((double)unsigneddiff); // negative since we actually need curts-prevtimestamp
				else {
					// wraparound occurred and curts is actually larger than prevtimestamp
					unsigneddiff = -unsigneddiff; // to get the actual difference (in absolute value)
					diffts2 = (double)unsigneddiff;
					}
				}
			else
				diffts2 = 0;

			diff = diffts1 - diffts2;
			if(diff < 0)
				diff = -diff;
			diff -= djitter;
			diff /= 16.0;
			djitter += diff;
			jitter = (DWORD)djitter;
#endif
			}
		else {
			djitter = 0;
			jitter = 0;
			}

		prevpacktime = receivetime;
		prevtimestamp = pack->GetTimestamp();
		lastmsgtime = prevpacktime;
		if(!ownpacket) // for own packet, this value is set on an outgoing packet
			lastrtptime = prevpacktime;
		}
	}

RTPSources::RTPSources(ProbationType probtype ,char *mgr/*,RTPMemoryManager *mgr*/) 
//	: sourcelist(32,owndata,RTPSources_GetHashIndex,RTPSOURCES_HASHSIZE /*?? RTPMEM_TYPE_CLASS_SOURCETABLEHASHELEMENT*/) 
//	: sourcelist(mgr,32/*RTPMEM_TYPE_CLASS_SOURCETABLEHASHELEMENT*/)
		: sourcelist()
{

	totalcount = 0;
	sendercount = 0;
	activecount = 0;

	owndata = NULL;
#ifdef RTP_SUPPORT_PROBATION
	probationtype = probtype;
#endif // RTP_SUPPORT_PROBATION
	}

RTPSources::~RTPSources() {

	Clear();
	}

void RTPSources::Clear() {

	ClearSourceList();
	}

void RTPSources::ClearSourceList() {

	sourcelist.GotoFirstElement();
	while(sourcelist.HasCurrentElement()) {
		RTPInternalSourceData *sourcedata;

		sourcedata = sourcelist.GetCurrentElement();
		delete sourcedata;
		sourcelist.GotoNextElement();
		}
	sourcelist.Clear();
	owndata =NULL;
	totalcount = 0;
	sendercount = 0;
	activecount = 0;
	}

int RTPSources::DeleteOwnSSRC() {

	if(!owndata)
		return ERR_RTP_SOURCES_DONTHAVEOWNSSRC;

	DWORD ssrc = owndata->GetSSRC();

	sourcelist.GotoElement(ssrc);
	sourcelist.DeleteCurrentElement();

	totalcount--;
	if(owndata->IsSender())
		sendercount--;
	if(owndata->IsActive())
		activecount--;

	OnRemoveSource(owndata);
	
	delete owndata;
	owndata = NULL;
	return 0;
	}

RTPSourceData::RTPSourceData(DWORD s) : SDESinf(),byetime(0,0) {

	ssrc = s;
	issender = false;
	iscsrc = false;
	timestampunit = -1;
	receivedbye = false;
	byereason = 0;
	byereasonlen = 0;
	rtpaddr = 0;
	rtcpaddr = 0;
	ownssrc = false;
	validated = false;
	processedinrtcp = false;			
	isrtpaddrset = false;
	isrtcpaddrset = false;
	}

RTPSourceData::~RTPSourceData() {

	FlushPackets();
	if(byereason)
		delete byereason;
	if(rtpaddr)
		delete rtpaddr;
	if(rtcpaddr)
		delete rtcpaddr;
	}

double RTPSourceData::INF_GetEstimatedTimestampUnit() const {

	if(!SRprevinf.HasInfo())
		return -1.0;
	
	RTPTime t1 = RTPTime(SRinf.GetNTPTimestamp());
	RTPTime t2 = RTPTime(SRprevinf.GetNTPTimestamp());
	if(t1.IsZero() || t2.IsZero()) // one of the times couldn't be calculated
		return -1.0;

	if(t1 <= t2)
		return -1.0;

	t1 -= t2; // get the time difference
	
	DWORD tsdiff = SRinf.GetRTPTimestamp()-SRprevinf.GetRTPTimestamp();
	
	return (t1.GetDouble()/((double)tsdiff));
	}

RTPSourceData *RTPSources::GetCurrentSourceInfo() {

	if(!sourcelist.HasCurrentElement())
		return 0;
	return sourcelist.GetCurrentElement();

	return 0;
	}

int RTPSources::ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,const RTPAddress *senderaddress,bool *stored) {
	DWORD ssrc;
	RTPInternalSourceData *srcdat;
	int status;
	bool created;

	OnRTPPacket(rtppack,receivetime,senderaddress);

	*stored = false;
	
	ssrc = rtppack->GetSSRC();
	if((status = ObtainSourceDataInstance(ssrc,&srcdat,&created)) < 0)
		return status;

	if(created) {
		if((status = srcdat->SetRTPDataAddress(senderaddress)) < 0)
			return status;
		}
	else {	// got a previously existing source
		if(CheckCollision(srcdat,senderaddress,true))
			return 0; // ignore packet on collision
		}
	
	bool prevsender = srcdat->IsSender();
	bool prevactive = srcdat->IsActive();
	
	DWORD CSRCs[RTP_MAXCSRCS];
	int numCSRCs = rtppack->GetCSRCCount();
	if(numCSRCs > RTP_MAXCSRCS) // shouldn't happen, but better to check than go out of bounds
		numCSRCs = RTP_MAXCSRCS;

	for(int i=0; i<numCSRCs; i++)
		CSRCs[i] = rtppack->GetCSRC(i);

	// The packet comes from a valid source, we can process it further now
	// The following function should delete rtppack itself if something goes wrong
	if((status = srcdat->ProcessRTPPacket(rtppack,receivetime,stored,this)) < 0)
		return status;

	// NOTE: we cannot use 'rtppack' anymore since it may have been deleted in
	//       OnValidatedRTPPacket

	if(!prevsender && srcdat->IsSender())
		sendercount++;
	if(!prevactive && srcdat->IsActive())
		activecount++;

	if(created)
		OnNewSource(srcdat);

	if(srcdat->IsValidated()) {		// process the CSRCs
		RTPInternalSourceData *csrcdat;
		bool createdcsrc;

		int num = numCSRCs;
		int i;

		for(i=0; i < num; i++)	{
			if((status = ObtainSourceDataInstance(CSRCs[i],&csrcdat,&createdcsrc)) < 0)
				return status;
			if(createdcsrc) {
				csrcdat->SetCSRC();
				if(csrcdat->IsActive())
					activecount++;
				OnNewSource(csrcdat);
				}
			else // already found an entry, possibly because of RTCP data
			{
				if(!CheckCollision(csrcdat,senderaddress,true))
					csrcdat->SetCSRC();
				}
			}
		}
	
	return 0;
	}

int RTPSources::ProcessRTCPCompoundPacket(RTCPCompoundPacket *rtcpcomppack,const RTPTime &receivetime,const RTPAddress *senderaddress) {
	RTCPPacket *rtcppack;
	int status;
	bool gotownssrc = ((owndata == 0) ? false : true);
	DWORD ownssrc = ((owndata != 0) ? owndata->GetSSRC():0);
	
	OnRTCPCompoundPacket(rtcpcomppack,receivetime,senderaddress);
	
	rtcpcomppack->GotoFirstPacket();	
	while((rtcppack = rtcpcomppack->GetNextPacket()))	{
		if(rtcppack->IsKnownFormat()) {
			switch(rtcppack->GetPacketType()) {
			case RTCPPacket::SR:
				{
					RTCPSRPacket *p = (RTCPSRPacket *)rtcppack;
					DWORD senderssrc = p->GetSenderSSRC();
					
					status = ProcessRTCPSenderInfo(senderssrc,p->GetNTPTimestamp(),p->GetRTPTimestamp(),
						                       p->GetSenderPacketCount(),p->GetSenderOctetCount(),
								       receivetime,senderaddress);
					if(status < 0)
						return status;
					
					bool gotinfo = false;
					if(gotownssrc)					{
						int i;
						int num = p->GetReceptionReportCount();
						for(i=0; i < num ; i++)						{
							if(p->GetSSRC(i) == ownssrc) // data is meant for us
							{
								gotinfo = true;
								status = ProcessRTCPReportBlock(senderssrc,p->GetFractionLost(i),p->GetLostPacketCount(i),
										                        p->GetExtendedHighestSequenceNumber(i),p->GetJitter(i),p->GetLSR(i),
													p->GetDLSR(i),receivetime,senderaddress);
								if(status < 0)
									return status;
								}
							}
						}
					if(!gotinfo)					{
						status = UpdateReceiveTime(senderssrc,receivetime,senderaddress);
						if(status < 0)
							return status;
					}
				}
				break;
			case RTCPPacket::RR:
				{
					RTCPRRPacket *p = (RTCPRRPacket *)rtcppack;
					DWORD senderssrc = p->GetSenderSSRC();
					
					bool gotinfo = false;

					if(gotownssrc)	{
						int i;
						int num = p->GetReceptionReportCount();
						for(i=0; i < num; i++)	{
							if(p->GetSSRC(i) == ownssrc)	{
								gotinfo = true;
								status = ProcessRTCPReportBlock(senderssrc,p->GetFractionLost(i),p->GetLostPacketCount(i),
										                        p->GetExtendedHighestSequenceNumber(i),p->GetJitter(i),p->GetLSR(i),
													p->GetDLSR(i),receivetime,senderaddress);
								if(status < 0)
									return status;
								}
							}
						}
					if(!gotinfo) {
						status = UpdateReceiveTime(senderssrc,receivetime,senderaddress);
						if(status < 0)
							return status;
					}
				}
				break;
			case RTCPPacket::SDES:
				{
					RTCPSDESPacket *p = (RTCPSDESPacket *)rtcppack;
					
					if(p->GotoFirstChunk()) {
						do {
							DWORD sdesssrc = p->GetChunkSSRC();
							bool updated = false;
							if(p->GotoFirstItem())	{
								do	{
									RTCPSDESPacket::ItemType t;
				
									if((t = p->GetItemType()) != RTCPSDESPacket::PRIV)	{
										updated = true;
										status = ProcessSDESNormalItem(sdesssrc,t,p->GetItemLength(),p->GetItemData(),receivetime,senderaddress);
										if(status < 0)
											return status;
									}
#ifdef RTP_SUPPORT_SDESPRIV
									else {
										updated = true;
										status = ProcessSDESPrivateItem(sdesssrc,p->GetPRIVPrefixLength(),p->GetPRIVPrefixData(),p->GetPRIVValueLength(),
												                        p->GetPRIVValueData(),receivetime,senderaddress);
										if(status < 0)
											return status;
									}
#endif // RTP_SUPPORT_SDESPRIV
									} while(p->GotoNextItem());
								}
							if(!updated)	{
								status = UpdateReceiveTime(sdesssrc,receivetime,senderaddress);
								if(status < 0)
									return status;
							}
						} while(p->GotoNextChunk());
					}
				}
				break;
			case RTCPPacket::BYE:
				{
					RTCPBYEPacket *p = (RTCPBYEPacket *)rtcppack;
					int i;
					int num = p->GetSSRCCount();

					for(i=0; i<num ; i++)	{
						DWORD byessrc = p->GetSSRC(i);
						status = ProcessBYE(byessrc,p->GetReasonLength(),p->GetReasonData(),receivetime,senderaddress);
						if(status < 0)
							return status;
					}
				}
				break;
			case RTCPPacket::APP:
				{
					RTCPAPPPacket *p = (RTCPAPPPacket *)rtcppack;

					OnAPPPacket(p,receivetime,senderaddress);
				}
				break; 
			case RTCPPacket::Unknown:
			default:
				{
					OnUnknownPacketType(rtcppack,receivetime,senderaddress);
				}
				break;
				}
			}
		else {
			OnUnknownPacketFormat(rtcppack,receivetime,senderaddress);
			}
		}

	return 0;
	}

bool RTPSources::GotoFirstSource() {

	sourcelist.GotoFirstElement();
	if(sourcelist.HasCurrentElement())
		return true;
	return false;
	}

bool RTPSources::GotoNextSource() {

	sourcelist.GotoNextElement();
	if(sourcelist.HasCurrentElement())
		return true;
	return false;
	}

bool RTPSources::GotoPreviousSource() {

	sourcelist.GotoPreviousElement();
	if(sourcelist.HasCurrentElement())
		return true;
	return false;
	}

bool RTPSources::GotoFirstSourceWithData() {
	bool found=false;
	
	sourcelist.GotoFirstElement();
	while(!found && sourcelist.HasCurrentElement())	{
		RTPInternalSourceData *srcdat;

		srcdat = sourcelist.GetCurrentElement();
		if(srcdat->HasData())
			found = true;
		else
			sourcelist.GotoNextElement();
		}

	return found;
	}

bool RTPSources::CheckCollision(RTPInternalSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp) {
	bool isset,otherisset;
	const RTPAddress *addr,*otheraddr;
	
	if(isrtp) {
		isset = srcdat->IsRTPAddressSet();
		addr = srcdat->GetRTPDataAddress();
		otherisset = srcdat->IsRTCPAddressSet();
		otheraddr = srcdat->GetRTCPDataAddress();
		}
	else {
		isset = srcdat->IsRTCPAddressSet();
		addr = srcdat->GetRTCPDataAddress();
		otherisset = srcdat->IsRTPAddressSet();
		otheraddr = srcdat->GetRTPDataAddress();
		}

	if(!isset) {
		if(otherisset) // got other address, can check if it comes from same host
		{
			if(!otheraddr) // other came from our own session
			{
				if(senderaddress) {
					OnSSRCCollision(srcdat,senderaddress,isrtp);
					return true;
					}

				// Ok, store it

				if(isrtp)
					srcdat->SetRTPDataAddress(senderaddress);
				else
					srcdat->SetRTCPDataAddress(senderaddress);
			}
			else {
				if(!otheraddr->IsFromSameHost(senderaddress))	{
					OnSSRCCollision(srcdat,senderaddress,isrtp);
					return true;
					}

				// Ok, comes from same host, store the address

				if(isrtp)
					srcdat->SetRTPDataAddress(senderaddress);
				else
					srcdat->SetRTCPDataAddress(senderaddress);
				}
			}
		else // no other address, store this one
		{
			if(isrtp)
				srcdat->SetRTPDataAddress(senderaddress);
			else
				srcdat->SetRTCPDataAddress(senderaddress);
			}
		}
	else // already got an address
	{
		if(!addr) {
			if(senderaddress)	{
				OnSSRCCollision(srcdat,senderaddress,isrtp);
				return true;
				}
			}
		else {
			if(!addr->IsSameAddress(senderaddress)) {
				OnSSRCCollision(srcdat,senderaddress,isrtp);
				return true;
				}
			}
		}
	
	return false;
	}


RTPPacket *RTPSources::GetNextPacket() {

	if(!sourcelist.HasCurrentElement())
		return 0;
	
	RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
	RTPPacket *pack = srcdat->GetNextPacket();
	return pack;
	}

int RTPSources::CreateOwnSSRC(DWORD ssrc) {

	if(owndata != 0)
		return ERR_RTP_SOURCES_ALREADYHAVEOWNSSRC;
	if(GotEntry(ssrc))
		return ERR_RTP_SOURCES_SSRCEXISTS;

	int status;
	bool created;
	
	status = ObtainSourceDataInstance(ssrc,&owndata,&created);
	if(status < 0) {
		owndata = 0; // just to make sure
		return status;
		}
	owndata->SetOwnSSRC();	
	owndata->SetRTPDataAddress(0);
	owndata->SetRTCPDataAddress(0);

	// we've created a validated ssrc, so we should increase activecount
	activecount++;

	OnNewSource(owndata);
	return 0;
	}

bool RTPSources::GotEntry(DWORD ssrc) {
	return sourcelist.HasElement(ssrc);
	}

int RTPSources::ProcessRTCPSenderInfo(DWORD ssrc,const RTPNTPTime &ntptime,DWORD rtptime,
                          DWORD packetcount,DWORD octetcount,const RTPTime &receivetime,
			  const RTPAddress *senderaddress) {
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if(status < 0)
		return status;
	if(!srcdat)
		return 0;
	
	srcdat->ProcessSenderInfo(ntptime,rtptime,packetcount,octetcount,receivetime);
	
	// Call the callback
	if(created)
		OnNewSource(srcdat);

	OnRTCPSenderReport(srcdat);

	return 0;
	}

int RTPSources::ProcessRTCPReportBlock(DWORD ssrc,BYTE fractionlost,int lostpackets,
                           DWORD exthighseqnr,DWORD jitter,DWORD lsr,
						   DWORD dlsr,const RTPTime &receivetime,const RTPAddress *senderaddress) {
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if(status < 0)
		return status;
	if(!srcdat)
		return 0;
	
	srcdat->ProcessReportBlock(fractionlost,lostpackets,exthighseqnr,jitter,lsr,dlsr,receivetime);

	// Call the callback
	if(created)
		OnNewSource(srcdat);

	OnRTCPReceiverReport(srcdat);
			
	return 0;
	}

int RTPSources::ProcessSDESNormalItem(DWORD ssrc,RTCPSDESPacket::ItemType t,size_t itemlength,
                          const void *itemdata,const RTPTime &receivetime,const RTPAddress *senderaddress) {
	RTPInternalSourceData *srcdat;
	bool created,cnamecollis;
	int status;
	BYTE sdesid;
	bool prevactive;

	switch(t)	{
		case RTCPSDESPacket::CNAME:
			sdesid = RTCP_SDES_ID_CNAME;
			break;
		case RTCPSDESPacket::NAME:
			sdesid = RTCP_SDES_ID_NAME;
			break;
		case RTCPSDESPacket::EMAIL:
			sdesid = RTCP_SDES_ID_EMAIL;
			break;
		case RTCPSDESPacket::PHONE:
			sdesid = RTCP_SDES_ID_PHONE;
			break;
		case RTCPSDESPacket::LOC:
			sdesid = RTCP_SDES_ID_LOCATION;
			break;
		case RTCPSDESPacket::TOOL:
			sdesid = RTCP_SDES_ID_TOOL;
			break;
		case RTCPSDESPacket::NOTE:
			sdesid = RTCP_SDES_ID_NOTE;
			break;
		default:
			return ERR_RTP_SOURCES_ILLEGALSDESTYPE;
		}	
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if(status < 0)
		return status;
	if(!srcdat)
		return 0;

	prevactive = srcdat->IsActive();
	status = srcdat->ProcessSDESItem(sdesid,(const BYTE*)itemdata,itemlength,receivetime,&cnamecollis);
	if(!prevactive && srcdat->IsActive())
		activecount++;
	
	// Call the callback
	if(created)
		OnNewSource(srcdat);
	if(cnamecollis)
		OnCNAMECollision(srcdat,senderaddress,(const BYTE*)itemdata,itemlength);

	if(status >= 0)
		OnRTCPSDESItem(srcdat, t, itemdata, itemlength);
	
	return status;
	}

int RTPSources::ProcessBYE(DWORD ssrc,size_t reasonlength,const void *reasondata,
		           const RTPTime &receivetime,const RTPAddress *senderaddress) {
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	bool prevactive;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if(status < 0)
		return status;
	if(!srcdat)
		return 0;

	// we'll ignore BYE packets for our own ssrc
	if(srcdat == owndata)
		return 0;
	
	prevactive = srcdat->IsActive();
	srcdat->ProcessBYEPacket((const BYTE*)reasondata,reasonlength,receivetime);
	if(prevactive && !srcdat->IsActive())
		activecount--;
	
	// Call the callback
	if(created)
		OnNewSource(srcdat);
	OnBYEPacket(srcdat);
	return 0;
	}

int RTPSources::ObtainSourceDataInstance(DWORD ssrc,RTPInternalSourceData **srcdat,bool *created) {
	RTPInternalSourceData *srcdat2;
	int status;
	
	if(sourcelist.GotoElement(ssrc) < 0) // No entry for this source
	{
#ifdef RTP_SUPPORT_PROBATION
		srcdat2 = new RTPInternalSourceData(ssrc,probationtype);
#else
		srcdat2 = new RTPInternalSourceData(ssrc,RTPSources::NoProbation);
#endif // RTP_SUPPORT_PROBATION
		if(!srcdat2)
			return ERR_RTP_OUTOFMEM;
		if((status = sourcelist.AddElement(ssrc,srcdat2)) < 0) {
			delete srcdat2;
			return status;
			}
		*srcdat = srcdat2;
		*created = true;
		totalcount++;
		}
	else {
		*srcdat = sourcelist.GetCurrentElement();
		*created = false;
		}
	return 0;
	}

int RTPSources::GetRTCPSourceData(DWORD ssrc,const RTPAddress *senderaddress,
		                  RTPInternalSourceData **srcdat2,bool *newsource) {
	int status;
	bool created;
	RTPInternalSourceData *srcdat;
	
	*srcdat2 = 0;
	
	if((status = ObtainSourceDataInstance(ssrc,&srcdat,&created)) < 0)
		return status;
	
	if(created) {
		if((status = srcdat->SetRTCPDataAddress(senderaddress)) < 0)
			return status;
		}
	else // got a previously existing source
	{
		if(CheckCollision(srcdat,senderaddress,false))
			return 0; // ignore packet on collision
		}
	
	*srcdat2 = srcdat;
	*newsource = created;

	return 0;
	}

int RTPSources::UpdateReceiveTime(DWORD ssrc,const RTPTime &receivetime,const RTPAddress *senderaddress) {
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if(status < 0)
		return status;
	if(!srcdat)
		return 0;
	
	// We got valid SSRC info
	srcdat->UpdateMessageTime(receivetime);
	
	// Call the callback
	if(created)
		OnNewSource(srcdat);

	return 0;
	}

void RTPSources::MultipleTimeouts(const RTPTime &curtime,const RTPTime &sendertimeout,const RTPTime &byetimeout,const RTPTime &generaltimeout,const RTPTime &notetimeout) {
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime senderchecktime = curtime;
	RTPTime byechecktime = curtime;
	RTPTime generaltchecktime = curtime;
	RTPTime notechecktime = curtime;
	senderchecktime -= sendertimeout;
	byechecktime -= byetimeout;
	generaltchecktime -= generaltimeout;
	notechecktime -= notetimeout;
	
	sourcelist.GotoFirstElement();
	while(sourcelist.HasCurrentElement()) {
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
		bool deleted,issender,isactive;
		bool byetimeout,normaltimeout,notetimeout;
		
		size_t notelen;
		
		issender = srcdat->IsSender();
		isactive = srcdat->IsActive();
		deleted = false;
		byetimeout = false;
		normaltimeout = false;
		notetimeout = false;

   	srcdat->SDES_GetNote(&notelen);
		if(notelen != 0) // Note has been set
		{
			RTPTime notetime = srcdat->INF_GetLastSDESNoteTime();
			
			if(notechecktime > notetime) {
				notetimeout = true;
				srcdat->ClearNote();
				}
			}

		if(srcdat->ReceivedBYE()) {
			RTPTime byetime = srcdat->GetBYETime();

			if((srcdat != owndata) && (byechecktime > byetime)) {
				sourcelist.DeleteCurrentElement();
				deleted = true;
				byetimeout = true;
				}
			}

		if(!deleted)	{
			RTPTime lastmsgtime = srcdat->INF_GetLastMessageTime();

			if((srcdat != owndata) && (lastmsgtime < generaltchecktime))	{
				sourcelist.DeleteCurrentElement();
				deleted = true;
				normaltimeout = true;
				}
			}
		
		if(!deleted) {
			newtotalcount++;
			
			if(issender) {
				RTPTime lastrtppacktime = srcdat->INF_GetLastRTPPacketTime();

				if(lastrtppacktime < senderchecktime) {
					srcdat->ClearSenderFlag();
					sendercount--;
					}
				else
					newsendercount++;
				}

			if(isactive)
				newactivecount++;

			if(notetimeout)
				OnNoteTimeout(srcdat);

			sourcelist.GotoNextElement();
			}
		else // deleted entry
		{
			if(issender)
				sendercount--;
			if(isactive)
				activecount--;
			totalcount--;

			if(byetimeout)
				OnBYETimeout(srcdat);
			if(normaltimeout)
				OnTimeout(srcdat);
			OnRemoveSource(srcdat);
			delete srcdat;
			}
		}	
	
#ifdef RTPDEBUG
	if(newtotalcount != totalcount) {
		SafeCountTotal();
		std::cout << "New total count " << newtotalcount << " doesnt match old total count " << totalcount << std::endl;
		}
	if(newsendercount != sendercount) {
		SafeCountSenders();
		std::cout << "New sender count " << newsendercount << " doesnt match old sender count " << sendercount << std::endl;
		}
	if(newactivecount != activecount) {
		std::cout << "New active count " << newactivecount << " doesnt match old active count " << activecount << std::endl;
		SafeCountActive();
	}
#endif // RTPDEBUG
	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
	}

int RTPSources::ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *rtptrans,bool acceptownpackets) {
	RTPTransmitter *transmitters[1];
	int num;
	
	transmitters[0] = rtptrans;
	if(!rtptrans)
		num = 0;
	else
		num = 1;
	return ProcessRawPacket(rawpack,transmitters,num,acceptownpackets);
	}

int RTPSources::ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *rtptrans[],int numtrans,bool acceptownpackets) {
	int status;
	
	if(rawpack->IsRTP()) {	// RTP packet
		RTPPacket *rtppack;
		
		// First, we'll see if the packet can be parsed
		rtppack = new RTPPacket(*rawpack);
		if(!rtppack)
			return ERR_RTP_OUTOFMEM;
		if((status = rtppack->GetCreationError()) < 0)	{
			if(status == ERR_RTP_PACKET_INVALIDPACKET)	{
				delete rtppack;
				rtppack = NULL;
				}
			else {
				delete rtppack;
				return status;
				}
			}
				
		// Check if the packet was valid
		if(rtppack)	{
			bool stored = false;
			bool ownpacket = false;
			int i;
			const RTPAddress *senderaddress = rawpack->GetSenderAddress();

			for(i=0; !ownpacket && i < numtrans; i++) {
				if(rtptrans[i]->ComesFromThisTransmitter(senderaddress))
					ownpacket = true;
				}
			
			// Check if the packet is our own.
			if(ownpacket) {
				// Now it depends on the user's preference what to do with this packet:
				if(acceptownpackets)	{
					// sender addres for own packets has to be NULL!
					if((status = ProcessRTPPacket(rtppack,rawpack->GetReceiveTime(),0,&stored)) < 0)	{
						if(!stored)
							delete rtppack;
						return status;
						}
					}
				}
			else {
				if((status = ProcessRTPPacket(rtppack,rawpack->GetReceiveTime(),senderaddress,&stored)) < 0)	{
					if(!stored)
						delete rtppack;
					return status;
					}
				}
			if(!stored)
				delete rtppack;
			}
		}
	else {		// RTCP packet
		RTCPCompoundPacket rtcpcomppack(*rawpack);
		bool valid = false;
		
		if((status = rtcpcomppack.GetCreationError()) < 0) {
			if(status != ERR_RTP_RTCPCOMPOUND_INVALIDPACKET)
				return status;
			}
		else
			valid = true;


		if(valid) {
			bool ownpacket = false;
			int i;
			const RTPAddress *senderaddress = rawpack->GetSenderAddress();

			for(i=0 ; !ownpacket && i < numtrans ; i++)	{
				if(rtptrans[i]->ComesFromThisTransmitter(senderaddress))
					ownpacket = true;
				}

			// First check if it's a packet of this session.
			if(ownpacket) {
				if(acceptownpackets)	{
					// sender address for own packets has to be NULL
					status = ProcessRTCPCompoundPacket(&rtcpcomppack,rawpack->GetReceiveTime(),0);
					if(status < 0)
						return status;
					}
				}
			else // not our own packet
			{
				status = ProcessRTCPCompoundPacket(&rtcpcomppack,rawpack->GetReceiveTime(),rawpack->GetSenderAddress());
				if(status < 0)
					return status;
				}
			}
		}
	
	return 0;
	}




bool RTPIPv4Address::IsFromSameHost(const RTPAddress *addr) const {
	
	if(!addr)
		return false;
	if(addr->GetAddressType() != IPv4Address)
		return false;
	
	const RTPIPv4Address *addr2 = (const RTPIPv4Address *)addr;
	if(addr2->GetIP() == ip)
		return true;
	return false;
	}

bool RTPIPv4Address::IsSameAddress(const RTPAddress *addr) const {
	
	if(!addr)
		return false;
	if(addr->GetAddressType() != IPv4Address)
		return false;

	const RTPIPv4Address *addr2 = (const RTPIPv4Address *)addr;
	if(addr2->GetIP() == ip && addr2->GetPort() == port)
		return true;
	return false;
	}

RTPAddress *RTPIPv4Address::CreateCopy() const {

	RTPIPv4Address *a = new RTPIPv4Address(ip,port);
	return a;
	}

bool RTPTCPAddress::IsSameAddress(const RTPAddress *addr) const {
	
	if(!addr)
		return false;
	if(addr->GetAddressType() != TCPAddress)
		return false;

	const RTPTCPAddress *a = static_cast<const RTPTCPAddress *>(addr);

	// We're using a socket to identify connections
	if(a->m_socket == m_socket)
		return true;

	return false;
	}

bool RTPTCPAddress::IsFromSameHost(const RTPAddress *addr) const {
	
	return IsSameAddress(addr);
	}

RTPAddress *RTPTCPAddress::CreateCopy(/*RTPMemoryManager *mgr*/) const {

	RTPTCPAddress *a = new /*RTPNew(mgr,RTPMEM_TYPE_CLASS_RTPADDRESS) */ RTPTCPAddress(m_socket);
	return a;
	}

void RTPCollisionList::Clear() {

	for(POSITION pos = addresslist.GetHeadPosition(); pos;)
		delete addresslist.GetNext(pos).addr;
	addresslist.RemoveAll();
	}


RTPPacket::RTPPacket(RTPRawPacket &rawpack) : receivetime(rawpack.GetReceiveTime()) {

	Clear();
	error = ParseRawPacket(rawpack);
	}

RTPPacket::RTPPacket(BYTE payloadtype,const void *payloaddata,size_t payloadlen,WORD seqnr,
		  DWORD timestamp,DWORD ssrc,bool gotmarker,BYTE numcsrcs,const DWORD *csrcs,
		  bool gotextension,WORD extensionid,WORD extensionlen_numwords,const void *extensiondata,
			size_t maxpacksize) : receivetime(0,0) {

	Clear();
	error = BuildPacket(payloadtype,payloaddata,payloadlen,seqnr,timestamp,ssrc,gotmarker,numcsrcs,
	       	            csrcs,gotextension,extensionid,extensionlen_numwords,extensiondata,0,maxpacksize);
	}

RTPPacket::RTPPacket(BYTE payloadtype,const void *payloaddata,size_t payloadlen,WORD seqnr,
		  DWORD timestamp,DWORD ssrc,bool gotmarker,BYTE numcsrcs,const DWORD *csrcs,
		  bool gotextension,WORD extensionid,WORD extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t buffersize) : receivetime(0,0) {

	Clear();
	if(!buffer)
		error = ERR_RTP_PACKET_EXTERNALBUFFERNULL;
	else if(buffersize <= 0)
		error = ERR_RTP_PACKET_ILLEGALBUFFERSIZE;
	else
		error = BuildPacket(payloadtype,payloaddata,payloadlen,seqnr,timestamp,ssrc,gotmarker,numcsrcs,
		                    csrcs,gotextension,extensionid,extensionlen_numwords,extensiondata,buffer,buffersize);
	}

int RTPPacket::ParseRawPacket(RTPRawPacket &rawpack) {
  static bool prompt_flag_for_myRtspClient = false;
	BYTE *packetbytes;
	size_t packetlen;
	BYTE payloadtype;
	RTPHeader *rtpheader;
	bool marker;
	int csrccount;
	bool hasextension;
	int payloadoffset,payloadlength;
	int numpadbytes;
	RTPExtensionHeader *rtpextheader;
	
	if(!rawpack.IsRTP()) // If we didn't receive it on the RTP port, we'll ignore it
		return ERR_RTP_PACKET_INVALIDPACKET;
	
	// The length should be at least the size of the RTP header
	packetlen = rawpack.GetDataLength();
	if(packetlen < sizeof(RTPHeader))
		return ERR_RTP_PACKET_INVALIDPACKET;
	
	packetbytes = (BYTE *)rawpack.GetData();
	rtpheader = (RTPHeader *)packetbytes;
	
	// The version number should be correct
  if(rtpheader->version != RTP_VERSION) {
    if(!prompt_flag_for_myRtspClient) {
      // printf("***Maybe you need to change 'RTP_ENDIAN' in config.xxx***\n");
      // printf("***when running genMakefiles(only for cross-compiling)***\n");
      prompt_flag_for_myRtspClient = true;
      }
    return ERR_RTP_PACKET_INVALIDPACKET;
    }
	
	// We'll check if this is possibly a RTCP packet. For this to be possible
	// the marker bit and payload type combined should be either an SR or RR identifier
	marker = (rtpheader->marker == 0) ? false : true;
	payloadtype = rtpheader->payloadtype;
	if(marker) {
		if(payloadtype == (RTP_RTCPTYPE_SR & 127)) // don't check high bit (this was the marker!!)
			return ERR_RTP_PACKET_INVALIDPACKET;
		if(payloadtype == (RTP_RTCPTYPE_RR & 127))
			return ERR_RTP_PACKET_INVALIDPACKET;
		}

	csrccount = rtpheader->csrccount;
	payloadoffset = sizeof(RTPHeader)+(int)(csrccount*sizeof(DWORD));
	
	if(rtpheader->padding) {	// adjust payload length to take padding into account
		numpadbytes = (int)packetbytes[packetlen-1]; // last byte contains number of padding bytes
		if(numpadbytes <= 0)
			return ERR_RTP_PACKET_INVALIDPACKET;
		}
	else
		numpadbytes = 0;

	hasextension = (rtpheader->extension == 0) ? false : true;
	if(hasextension) {	// got header extension
		rtpextheader = (RTPExtensionHeader *)(packetbytes+payloadoffset);
		payloadoffset += sizeof(RTPExtensionHeader);
		
		WORD exthdrlen = ntohs(rtpextheader->length);
		payloadoffset += ((int)exthdrlen)*sizeof(DWORD);
		}
	else {
		rtpextheader = 0;
		}	
	
	payloadlength = packetlen-numpadbytes-payloadoffset;
	if(payloadlength < 0)
		return ERR_RTP_PACKET_INVALIDPACKET;

	// Now, we've got a valid packet, so we can create a new instance of RTPPacket
	// and fill in the members
	RTPPacket::hasextension = hasextension;
	if(hasextension)	{
		RTPPacket::extid = ntohs(rtpextheader->extid);
		RTPPacket::extensionlength = ((int)ntohs(rtpextheader->length))*sizeof(DWORD);
		RTPPacket::extension = ((BYTE *)rtpextheader)+sizeof(RTPExtensionHeader);
		}

	RTPPacket::hasmarker = marker;
	RTPPacket::numcsrcs = csrccount;
	RTPPacket::payloadtype = payloadtype;
	
	// Note: we don't fill in the EXTENDED sequence number here, since we
	// don't have information about the source here. We just fill in the low 16 bits
	RTPPacket::extseqnr = (DWORD)ntohs(rtpheader->sequencenumber);

	RTPPacket::timestamp = ntohl(rtpheader->timestamp);
	RTPPacket::ssrc = ntohl(rtpheader->ssrc);
	RTPPacket::packet = packetbytes;
	RTPPacket::payload = packetbytes+payloadoffset;
	RTPPacket::packetlength = packetlen;
	RTPPacket::payloadlength = payloadlength;

	// We'll zero the data of the raw packet, since we're using it here now!
	rawpack.ZeroData();

	return 0;
	}

void RTPPacket::Clear() {

	hasextension = false;
	hasmarker = false;
	numcsrcs = 0;
	payloadtype = 0;
	extseqnr = 0;
	timestamp = 0;
	ssrc = 0;
	packet = 0;
	payload = 0; 
	packetlength = 0;
	payloadlength = 0;
	extid = 0;
	extension = 0;
	extensionlength = 0;
	error = 0;
	externalbuffer = false;
	}

DWORD RTPPacket::GetCSRC(int num) const {

	if(num >= numcsrcs)
		return 0;

	BYTE *csrcpos;
	DWORD *csrcval_nbo;
	DWORD csrcval_hbo;
	
	csrcpos = packet+sizeof(RTPHeader)+num*sizeof(DWORD);
	csrcval_nbo = (DWORD *)csrcpos;
	csrcval_hbo = ntohl(*csrcval_nbo);
	return csrcval_hbo;
	}

int RTPPacket::BuildPacket(BYTE payloadtype,const void *payloaddata,size_t payloadlen,WORD seqnr,
		  DWORD timestamp,DWORD ssrc,bool gotmarker,BYTE numcsrcs,const DWORD *csrcs,
		  bool gotextension,WORD extensionid,WORD extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t maxsize) {

	if(numcsrcs > RTP_MAXCSRCS)
		return ERR_RTP_PACKET_TOOMANYCSRCS;

	if(payloadtype > 127) // high bit should not be used
		return ERR_RTP_PACKET_BADPAYLOADTYPE;
	if(payloadtype == 72 || payloadtype == 73) // could cause confusion with rtcp types
		return ERR_RTP_PACKET_BADPAYLOADTYPE;
	
	packetlength = sizeof(RTPHeader);
	packetlength += sizeof(DWORD)*((size_t)numcsrcs);
	if(gotextension) {
		packetlength += sizeof(RTPExtensionHeader);
		packetlength += sizeof(DWORD)*((size_t)extensionlen_numwords);
		}
	packetlength += payloadlen;

	if(maxsize > 0 && packetlength > maxsize) {
		packetlength = 0;
		return ERR_RTP_PACKET_DATAEXCEEDSMAXSIZE;
		}

	// Ok, now we'll just fill in...
	RTPHeader *rtphdr;
	
	if(!buffer) {
		packet = new BYTE[packetlength];
		if(!packet) {
			packetlength = 0;
			return ERR_RTP_OUTOFMEM;
			}
		externalbuffer = false;
		}
	else	{
		packet = (BYTE*)buffer;
		externalbuffer = true;
		}
	
	RTPPacket::hasmarker = gotmarker;
	RTPPacket::hasextension = gotextension;
	RTPPacket::numcsrcs = numcsrcs;
	RTPPacket::payloadtype = payloadtype;
	RTPPacket::extseqnr = (DWORD)seqnr;
	RTPPacket::timestamp = timestamp;
	RTPPacket::ssrc = ssrc;
	RTPPacket::payloadlength = payloadlen;
	RTPPacket::extid = extensionid;
	RTPPacket::extensionlength = ((size_t)extensionlen_numwords)*sizeof(DWORD);
	
	rtphdr = (RTPHeader *)packet;
	rtphdr->version = RTP_VERSION;
	rtphdr->padding = 0;
	if(gotmarker)
		rtphdr->marker = 1;
	else
		rtphdr->marker = 0;
	if(gotextension)
		rtphdr->extension = 1;
	else
		rtphdr->extension = 0;
	rtphdr->csrccount = numcsrcs;
	rtphdr->payloadtype = payloadtype&127; // make sure high bit isn't set
	rtphdr->sequencenumber = htons(seqnr);
	rtphdr->timestamp = htonl(timestamp);
	rtphdr->ssrc = htonl(ssrc);
	
	DWORD *curcsrc;
	int i;

	curcsrc = (DWORD *)(packet+sizeof(RTPHeader));
	for(i=0 ; i < numcsrcs ; i++,curcsrc++)
		*curcsrc = htonl(csrcs[i]);

	payload = packet+sizeof(RTPHeader)+((size_t)numcsrcs)*sizeof(DWORD); 
	if(gotextension) {
		RTPExtensionHeader *rtpexthdr = (RTPExtensionHeader *)payload;

		rtpexthdr->extid = htons(extensionid);
		rtpexthdr->length = htons((DWORD)extensionlen_numwords);
		
		payload += sizeof(RTPExtensionHeader);
		memcpy(payload,extensiondata,RTPPacket::extensionlength);
		
		payload += RTPPacket::extensionlength;
		}
	memcpy(payload,payloaddata,payloadlen);
	return 0;
	}

#ifdef RTPDEBUG	
void RTPPacket::Dump() {
	int i;
	
	printf("Payload type:                %d\n",(int)GetPayloadType());
	printf("Extended sequence number:    0x%08x\n",GetExtendedSequenceNumber());
	printf("Timestamp:                   0x%08x\n",GetTimestamp());
	printf("SSRC:                        0x%08x\n",GetSSRC());
	printf("Marker:                      %s\n",HasMarker()?"yes":"no");
	printf("CSRC count:                  %d\n",GetCSRCCount());
	for(i=0 ; i < GetCSRCCount() ; i++)
		printf("    CSRC[%02d]:                0x%08x\n",i,GetCSRC(i));
	printf("Payload:                     %s\n",GetPayloadData());
	printf("Payload length:              %d\n",GetPayloadLength());
	printf("Packet length:               %d\n",GetPacketLength());
	printf("Extension:                   %s\n",HasExtension()?"yes":"no");
	if(HasExtension())	{
		printf("    Extension ID:            0x%04x\n",GetExtensionID());
		printf("    Extension data:          %s\n",GetExtensionData());
		printf("    Extension length:        %d\n",GetExtensionLength());
		}
	}
#endif // RTPDEBUG



RTPPacketBuilder::RTPPacketBuilder(int r /*RTPRandom &r,RTPMemoryManager *mgr*/) :
	/*RTPMemoryObject(mgr),*/rtprnd(r),lastwallclocktime(0,0) {

	init = false;
//	timeinit.Dummy();

	//std::cout << (void *)(&rtprnd) << std::endl;
	}

RTPPacketBuilder::~RTPPacketBuilder() {

	Destroy();
	}

int RTPPacketBuilder::Init(size_t max) {

	if(init)
		return ERR_RTP_PACKBUILD_ALREADYINIT;
	if(max <= 0)
		return ERR_RTP_PACKBUILD_INVALIDMAXPACKETSIZE;
	
	maxpacksize = max;
	buffer = new /*RTPNew(GetMemoryManager(),RTPMEM_TYPE_BUFFER_RTPPACKETBUILDERBUFFER) */ BYTE[max];
	if(buffer == NULL)
		return ERR_RTP_OUTOFMEM;
	packetlength=0;
	
	CreateNewSSRC();

	deftsset = false;
	defptset = false;
	defmarkset = false;
		
	numcsrcs = 0;
	
	init = true;
	return 0;
	}

void RTPPacketBuilder::Destroy() {

	if(!init)
		return;
	delete /*RTPDeleteByteArray(*/ []buffer /*,GetMemoryManager())*/;
	init = false;
	}

DWORD RTPPacketBuilder::CreateNewSSRC() {

	ssrc = rand() | ((DWORD)rand()) << 16;
	timestamp = rand() | ((DWORD)rand()) << 16;
	seqnr = rand();

	// p 38: the count SHOULD be reset if the sender changes its SSRC identifier

	numpayloadbytes = 0;
	numpackets = 0;
	return ssrc;
	}

DWORD RTPPacketBuilder::CreateNewSSRC(RTPSources &sources) {
	bool found;
	
	/*
	do {
		ssrc = rtprnd.GetRandom32();
		found = sources.GotEntry(ssrc);
		} while(found);
	
	timestamp = rtprnd.GetRandom32();
	seqnr = rtprnd.GetRandom16();
*/
	// p 38: the count SHOULD be reset if the sender changes its SSRC identifier
	numpayloadbytes = 0;
	numpackets = 0;
	return ssrc;
	}



RTPSessionParams::RTPSessionParams() : mininterval(0,0) {

#ifdef RTP_SUPPORT_THREAD
	usepollthread = true;
	m_needThreadSafety = true;
#else
	usepollthread = false;
	m_needThreadSafety = false;
#endif // RTP_SUPPORT_THREAD
	maxpacksize = RTP_DEFAULTPACKETSIZE;
	receivemode = RTPTransmitter::AcceptAll;
	acceptown = false;
	owntsunit = -1; // The user will have to set it to the correct value himself
	resolvehostname = false;
#ifdef RTP_SUPPORT_PROBATION
	probationtype = RTPSources::ProbationStore;
#endif // RTP_SUPPORT_PROBATION

	mininterval = RTPTime(RTCP_DEFAULTMININTERVAL);
	sessionbandwidth = RTP_DEFAULTSESSIONBANDWIDTH;
	controlfrac = RTCP_DEFAULTBANDWIDTHFRACTION;
	senderfrac = RTCP_DEFAULTSENDERFRACTION;
	usehalfatstartup = RTCP_DEFAULTHALFATSTARTUP;
	immediatebye = RTCP_DEFAULTIMMEDIATEBYE;
	SR_BYE = RTCP_DEFAULTSRBYE;

	sendermultiplier = RTP_SENDERTIMEOUTMULTIPLIER;
	generaltimeoutmultiplier = RTP_MEMBERTIMEOUTMULTIPLIER;
	byetimeoutmultiplier = RTP_BYETIMEOUTMULTIPLIER;
	collisionmultiplier = RTP_COLLISIONTIMEOUTMULTIPLIER;
	notemultiplier = RTP_NOTETTIMEOUTMULTIPLIER;
	
	usepredefinedssrc = false;
	predefinedssrc = 0;
	}


RTPTCPTransmitter::RTPTCPTransmitter(/*RTPMemoryManager *mgr*/) : RTPTransmitter(/*mgr*/) {

	m_created = false;
	m_init = false;
	}

RTPTCPTransmitter::~RTPTCPTransmitter() {

	Destroy();
	}

int RTPTCPTransmitter::Init(bool tsafe) {

	if(m_init)
		return ERR_RTP_TCPTRANS_ALREADYINIT;
	
#ifdef RTP_SUPPORT_THREAD
	m_threadsafe = tsafe;
	if(m_threadsafe) {
		int status;
		
		status = m_mainMutex.Init();
		if(status < 0)
			return ERR_RTP_TCPTRANS_CANTINITMUTEX;
		status = m_waitMutex.Init();
		if(status < 0)
			return ERR_RTP_TCPTRANS_CANTINITMUTEX;
		}
#else
	if(tsafe)
		return ERR_RTP_NOTHREADSUPPORT;
#endif // RTP_SUPPORT_THREAD

	m_maxPackSize = RTPTCPTRANS_MAXPACKSIZE;
	m_init = true;
	return 0;
	}

int RTPTCPTransmitter::Create(size_t maximumpacketsize, const RTPTransmissionParams *transparams) {
	const RTPTCPTransmissionParams *params,defaultparams;
	int status;

	if(!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if(m_created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_ALREADYCREATED;
		}
	
	// Obtain transmission parameters
	if(!transparams)
		params = &defaultparams;
	else {
		if(transparams->GetTransmissionProtocol() != RTPTransmitter::TCPProto) {
			MAINMUTEX_UNLOCK
			return ERR_RTP_TCPTRANS_ILLEGALPARAMETERS;
			}
		params = static_cast<const RTPTCPTransmissionParams *>(transparams);
		}

	if(!params->GetCreatedAbortDescriptors())	{
		if((status = m_abortDesc.Init()) < 0) {
			MAINMUTEX_UNLOCK
			return status;
			}
		m_pAbortDesc = &m_abortDesc;
		}
	else {
		m_pAbortDesc = params->GetCreatedAbortDescriptors();
		if(!m_pAbortDesc->IsInitialized()) {
			MAINMUTEX_UNLOCK
			return ERR_RTP_ABORTDESC_NOTINIT;
			}
		}

	m_waitingForData = false;
	m_created = true;


	MAINMUTEX_UNLOCK 
	return 0;
	}

void RTPTCPTransmitter::Destroy() {

	if(!m_init)
		return;

	MAINMUTEX_LOCK
	if(!m_created) {
		MAINMUTEX_UNLOCK;
		return;
	}

	ClearDestSockets();
	FlushPackets();
	m_created = false;
	
	if(m_waitingForData)	{
		m_pAbortDesc->SendAbortSignal();
		m_abortDesc.Destroy(); // Doesn't do anything if not initialized
		MAINMUTEX_UNLOCK
		WAITMUTEX_LOCK // to make sure that the WaitForIncomingData function ended
		WAITMUTEX_UNLOCK
		}
	else
		m_abortDesc.Destroy(); // Doesn't do anything if not initialized

	MAINMUTEX_UNLOCK
	}

bool RTPTCPTransmitter::ComesFromThisTransmitter(const RTPAddress *addr) {

	if(!m_init)
		return false;

	if(!addr)
		return false;
	
	MAINMUTEX_LOCK
	
	if(!m_created)
		return false;

	if(addr->GetAddressType() != RTPAddress::TCPAddress)
		return false;

	const RTPTCPAddress *pAddr = static_cast<const RTPTCPAddress *>(addr);
	bool v = false;

//	JRTPLIB_UNUSED(pAddr);
	// TODO: for now, we're assuming that we can't just send to the same transmitter

	MAINMUTEX_UNLOCK
	return v;
	}

int RTPTCPTransmitter::Poll() {

	if(!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;

	MAINMUTEX_LOCK
	if(!m_created)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
		}

	std::map<SocketType, SocketData>::iterator it = m_destSockets.begin();
	std::map<SocketType, SocketData>::iterator end = m_destSockets.end();
	int status = 0;

	vector<SocketType> errSockets;

	while(it != end) {
		SocketType sock = it->first;
		status = PollSocket(sock, it->second);
		if(status < 0)	{
			// Stop immediately on out of memory
			if(status == ERR_RTP_OUTOFMEM)
				break;
			else	{
				errSockets.push_back(sock);
				// Don't let this count as an error (due to a closed connection for example),
				// otherwise the poll thread (if used) will stop because of this. Since there
				// may be more than one connection, that's not desirable in general.
				status = 0; 
				}
			}
		++it;
		}
	MAINMUTEX_UNLOCK

	for(size_t i=0; i < errSockets.size(); i++)
		OnReceiveError(errSockets[i]);

//	return status; è in memorymanager??
	}

int RTPTCPTransmitter::SendRTPData(const void *data,size_t len)	{
	return SendRTPRTCPData(data, len);
	}

int RTPTCPTransmitter::SendRTCPData(const void *data,size_t len) {
	return SendRTPRTCPData(data, len);
	}

int RTPTCPTransmitter::AddDestination(const RTPAddress &addr) {
	
	if(!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;

	MAINMUTEX_LOCK

	if(!m_created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
		}

	if(addr.GetAddressType() != RTPAddress::TCPAddress)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_INVALIDADDRESSTYPE;
		}

	const RTPTCPAddress &a = static_cast<const RTPTCPAddress &>(addr);
	SocketType s = a.GetSocket();
	if(s == 0) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOSOCKETSPECIFIED;
		}

	int status = ValidateSocket(s);
	if(status != 0)	{
		MAINMUTEX_UNLOCK
		return status;
		}
	
	std::map<SocketType, SocketData>::iterator it = m_destSockets.find(s);
	if(it != m_destSockets.end())	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_SOCKETALREADYINDESTINATIONS;
		}
	m_destSockets[s] = SocketData();

	// Because the sockets are also used for incoming data, we'll abort a wait
	// that may be in progress, otherwise it could take a few seconds until the
	// new socket is monitored for incoming data
	m_pAbortDesc->SendAbortSignal();

	MAINMUTEX_UNLOCK

	return 0;
	}

int RTPTCPTransmitter::DeleteDestination(const RTPAddress &addr) {

	if(!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	if(!m_created)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
		}
	
	if(addr.GetAddressType() != RTPAddress::TCPAddress)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_INVALIDADDRESSTYPE;
		}

	const RTPTCPAddress &a = static_cast<const RTPTCPAddress &>(addr);
	SocketType s = a.GetSocket();
	if(s == 0) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOSOCKETSPECIFIED;
		}

	std::map<SocketType, SocketData>::iterator it = m_destSockets.find(s);
	if(it == m_destSockets.end())	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_SOCKETNOTFOUNDINDESTINATIONS;
		}

	// Clean up possibly allocated memory
	BYTE *pBuf = it->second.ExtractDataBuffer();
	if(pBuf)
		delete []pBuf; //RTPDeleteByteArray(pBuf, GetMemoryManager());

	m_destSockets.erase(it);

	MAINMUTEX_UNLOCK

	return 0;
	}

RTPRawPacket *RTPTCPTransmitter::GetNextPacket() {

	if(!m_init)
		return 0;
	
	MAINMUTEX_LOCK
	
	RTPRawPacket *p;
	if(!m_created) {
		MAINMUTEX_UNLOCK
		return 0;
		}
	if(rawpacketlist.IsEmpty())	{
		MAINMUTEX_UNLOCK
		return 0;
		}

	p = rawpacketlist.GetHead();
	rawpacketlist.RemoveHead();

	MAINMUTEX_UNLOCK
	return p;
	}

void RTPTCPTransmitter::ClearDestinations() {

	if(!m_init)
		return;

	MAINMUTEX_LOCK
	if(m_created)
		ClearDestSockets();
	MAINMUTEX_UNLOCK
	}

int RTPTCPTransmitter::SendRTPRTCPData(const void *data, size_t len) {

	if(!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	if(!m_created)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}
	if(len > RTPTCPTRANS_MAXPACKSIZE)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_SPECIFIEDSIZETOOBIG;
	}
	
	std::map<SocketType, SocketData>::iterator it = m_destSockets.begin();
	std::map<SocketType, SocketData>::iterator end = m_destSockets.end();

	vector<SocketType> errSockets;
	int flags = 0;
#ifdef RTP_HAVE_MSG_NOSIGNAL
	flags = MSG_NOSIGNAL;
#endif // RTP_HAVE_MSG_NOSIGNAL

	while(it != end)	{
		BYTE lengthBytes[2] = { (BYTE)((len >> 8)&0xff), (BYTE)(len&0xff) };
		SocketType sock = it->first;

		if(sock->Send((const char *)lengthBytes,2,flags) < 0 ||
			sock->Send((const char *)data,len,flags) < 0)
			errSockets.push_back(sock);
		++it;
		}
	
	MAINMUTEX_UNLOCK

	if(errSockets.size() != 0)	{
		for(size_t i=0 ; i < errSockets.size() ; i++)
			OnSendError(errSockets[i]);
	}

	// Don't return an error code to avoid the poll thread exiting
	// due to one closed connection for example

	return 0;
	}

int RTPTCPTransmitter::ValidateSocket(SocketType) {
	// TODO: should we even do a check (for a TCP socket)? 
	return 0;
	}

void RTPTCPTransmitter::ClearDestSockets() {
	std::map<SocketType, SocketData>::iterator it = m_destSockets.begin();
	std::map<SocketType, SocketData>::iterator end = m_destSockets.end();

	while(it != end) {
		BYTE *pBuf = it->second.ExtractDataBuffer();
		if(pBuf)
			delete []pBuf; //RTPDeleteByteArray(pBuf, GetMemoryManager());

		++it;
		}
	m_destSockets.clear();
	}


int RTPTCPTransmitter::GetLocalHostName(BYTE *buffer,size_t *bufferlength) {

	if(!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;

	MAINMUTEX_LOCK
	if(!m_created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
		}

	if(m_localHostname.size() == 0)	{
		//
		// TODO
		// TODO
		// TODO
		// TODO
		//
		m_localHostname.resize(9);
		memcpy(&m_localHostname[0], "localhost", m_localHostname.size());
		}
	
	if((*bufferlength) < m_localHostname.size()) {
		*bufferlength = m_localHostname.size(); // tell the application the required size of the buffer
		MAINMUTEX_UNLOCK
		return ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL;
		}

	memcpy(buffer,&m_localHostname[0], m_localHostname.size());
	*bufferlength = m_localHostname.size();
	
	MAINMUTEX_UNLOCK
	return 0;
	}

int RTPTCPTransmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m) {

	if(m != RTPTransmitter::AcceptAll)
		return ERR_RTP_TCPTRANS_RECEIVEMODENOTSUPPORTED;
	return 0;
	}

int RTPTCPTransmitter::SetMaximumPacketSize(size_t s)	{

	if(!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if(!m_created)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
		}
	if(s > RTPTCPTRANS_MAXPACKSIZE)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_SPECIFIEDSIZETOOBIG;
		}
	m_maxPackSize = s;
	MAINMUTEX_UNLOCK
	return 0;
	}

void RTPTCPTransmitter::FlushPackets() {

	for(POSITION pos = rawpacketlist.GetHeadPosition(); pos;)
		delete rawpacketlist.GetNext(pos);
	rawpacketlist.RemoveAll();
	}

int RTPTCPTransmitter::PollSocket(SocketType sock, SocketData &sdata) {
	unsigned long len;
	bool dataavailable;
	
	do {
		len = 0;
		sock->IOCtl(FIONREAD,&len);

		if(len <= 0) 
			dataavailable = false;
		else
			dataavailable = true;
		
		if(dataavailable) {
			RTPTime curtime = RTPTime::CurrentTime();
			int relevantLen = RTPTCPTRANS_MAXPACKSIZE+2;
			
			if((int)len < relevantLen)
				relevantLen = (int)len;

			bool complete = false;
			int status = sdata.ProcessAvailableBytes(sock, relevantLen, complete);
			if(status < 0)
				return status;
			
			if(complete) {
				BYTE *pBuf = sdata.ExtractDataBuffer();
				if(pBuf) {
					int dataLength = sdata.m_dataLength;
					sdata.Reset();

					RTPTCPAddress *pAddr = new RTPTCPAddress(sock);
					if(!pAddr)
						return ERR_RTP_OUTOFMEM;

					bool isrtp = true;
					if(dataLength > (int)sizeof(RTCPCommonHeader)) {
						RTCPCommonHeader *rtcpheader = (RTCPCommonHeader *)pBuf;
						BYTE packettype = rtcpheader->packettype;

						if(packettype >= 200 && packettype <= 204)
							isrtp = false;
						}
						
					RTPRawPacket *pPack = new RTPRawPacket(pBuf, dataLength, pAddr, curtime, isrtp);
					if(!pPack) {
						delete pAddr;
						delete pBuf;
						return ERR_RTP_OUTOFMEM;
						}
					rawpacketlist.AddTail(pPack);	
					}
				}
			}
		} while(dataavailable);

	return 0;
	}



RTCPScheduler::RTCPScheduler(RTPSources &s, /*RTPRandom &*/ int r) : 
	sources(s),nextrtcptime(0,0),prevrtcptime(0,0),rtprand(r) {
	Reset();

	//std::cout << (void *)(&rtprand) << std::endl;
	}

RTCPScheduler::~RTCPScheduler() {
	}

void RTCPScheduler::Reset() {

	headeroverhead = 0; // user has to set this to an appropriate value
	hassentrtcp = false;
	firstcall = true;
	avgrtcppacksize = 1000; // TODO: what is a good value for this?
	byescheduled = false;
	sendbyenow = false;
	}

void RTCPScheduler::AnalyseOutgoing(RTCPCompoundPacket &rtcpcomppack) {
	bool isbye = false;
	RTCPPacket *p;
	
	rtcpcomppack.GotoFirstPacket();
	while(!isbye && (p = rtcpcomppack.GetNextPacket()))	{
		if(p->GetPacketType() == RTCPPacket::BYE)
			isbye = true;
		}
	
	if(!isbye)	{
		size_t packsize = headeroverhead+rtcpcomppack.GetCompoundPacketLength();
		avgrtcppacksize = (size_t)((1.0/16.0)*((double)packsize)+(15.0/16.0)*((double)avgrtcppacksize));
		}

	hassentrtcp = true;
	}

bool RTCPScheduler::IsTime() {

	if(firstcall)	{
		firstcall = false;
		prevrtcptime = RTPTime::CurrentTime();
		pmembers = sources.GetActiveMemberCount();
		CalculateNextRTCPTime();
		return false;
		}

	RTPTime currenttime = RTPTime::CurrentTime();

//	// TODO: for debugging
//	double diff = nextrtcptime.GetDouble() - currenttime.GetDouble();
//
//	std::cout << "Delay till next RTCP interval: " << diff << std::endl;
	
	if(currenttime < nextrtcptime) // timer has not yet expired
		return false;

	RTPTime checktime(0,0);
	
	if(!byescheduled)	{
		bool aresender = false;
		RTPSourceData *srcdat;
		
		if((srcdat = sources.GetOwnSourceInfo()) != 0)
			aresender = srcdat->IsSender();
		
		checktime = CalculateTransmissionInterval(aresender);
		}
	else
		checktime = CalculateBYETransmissionInterval();
	
//	std::cout << "Calculated checktime: " << checktime.GetDouble() << std::endl;
	
	checktime += prevrtcptime;
	if(checktime <= currenttime) // Okay
	{
		byescheduled = false;
		prevrtcptime = currenttime;
		pmembers = sources.GetActiveMemberCount();
		CalculateNextRTCPTime();
		return true;
		}

//	std::cout << "New delay: " << nextrtcptime.GetDouble() - currenttime.GetDouble() << std::endl;
	
	nextrtcptime = checktime;
	pmembers = sources.GetActiveMemberCount();
	
	return false;
	}

void RTCPScheduler::CalculateNextRTCPTime() {
	bool aresender = false;
	RTPSourceData *srcdat;
	
	if((srcdat = sources.GetOwnSourceInfo()))
		aresender = srcdat->IsSender();
	
	nextrtcptime = RTPTime::CurrentTime();	
	nextrtcptime += CalculateTransmissionInterval(aresender);
	}

RTPTime RTCPScheduler::CalculateTransmissionInterval(bool sender) {
	RTPTime Td = CalculateDeterministicInterval(sender);
	double td,mul,T;

//	std::cout << "CalculateTransmissionInterval" << std::endl;

	td = Td.GetDouble();
//	mul = rtprand.GetRandomDouble()+0.5; // gives random value between 0.5 and 1.5
	mul = ((double)rand()/RAND_MAX)+0.5; // gives random value between 0.5 and 1.5
	T = (td*mul)/1.21828; // see RFC 3550 p 30

//	std::cout << "  Td: " << td << std::endl;
//	std::cout << "  mul: " << mul << std::endl;
//	std::cout << "  T: " << T << std::endl;

	return RTPTime(T);
	}

RTPTime RTCPScheduler::CalculateDeterministicInterval(bool sender /* = false */) {
	int numsenders = sources.GetSenderCount();
	int numtotal = sources.GetActiveMemberCount();

//	std::cout << "CalculateDeterministicInterval" << std::endl;
//	std::cout << "  numsenders: " << numsenders << std::endl;
//	std::cout << "  numtotal: " << numtotal << std::endl;

	// Try to avoid division by zero:
	if(numtotal == 0)
		numtotal++;

	double sfraction = ((double)numsenders)/((double)numtotal);
	double C,n;

	if(sfraction <= schedparams.GetSenderBandwidthFraction()) {
		if(sender) {
			C = ((double)avgrtcppacksize)/(schedparams.GetSenderBandwidthFraction()*schedparams.GetRTCPBandwidth());
			n = (double)numsenders;
			}
		else {
			C = ((double)avgrtcppacksize)/((1.0-schedparams.GetSenderBandwidthFraction())*schedparams.GetRTCPBandwidth());
			n = (double)(numtotal-numsenders);
			}
		}
	else {
		C = ((double)avgrtcppacksize)/schedparams.GetRTCPBandwidth();
		n = (double)numtotal;
	}
	
	RTPTime Tmin = schedparams.GetMinimumTransmissionInterval();
	double tmin = Tmin.GetDouble();
	
	if(!hassentrtcp && schedparams.GetUseHalfAtStartup())
		tmin /= 2.0;

	double ntimesC = n*C;
	double Td = (tmin>ntimesC)?tmin:ntimesC;

	// TODO: for debugging
//	std::cout << "  Td: " << Td << std::endl;

	return RTPTime(Td);
	}

RTPTime RTCPScheduler::CalculateBYETransmissionInterval() {

	if(!byescheduled)
		return RTPTime(0,0);
	
	if(sendbyenow)
		return RTPTime(0,0);
	
	double C,n;

	C = ((double)avgbyepacketsize)/((1.0-schedparams.GetSenderBandwidthFraction())*schedparams.GetRTCPBandwidth());
	n = (double)byemembers;
	
	RTPTime Tmin = schedparams.GetMinimumTransmissionInterval();
	double tmin = Tmin.GetDouble();
	
	if(schedparams.GetUseHalfAtStartup())
		tmin /= 2.0;

	double ntimesC = n*C;
	double Td = (tmin>ntimesC)?tmin:ntimesC;

//	double mul = rtprand.GetRandomDouble()+0.5; // gives random value between 0.5 and 1.5
	double mul = ((double)rand()/RAND_MAX)+0.5; // gives random value between 0.5 and 1.5
	double T = (Td*mul)/1.21828; // see RFC 3550 p 30
	
	return RTPTime(T);
	}

void RTCPScheduler::ScheduleBYEPacket(size_t packetsize) {

	if(byescheduled)
		return;
	
	if(firstcall)	{
		firstcall = false;
		pmembers = sources.GetActiveMemberCount();
	}

	byescheduled = true;
	avgbyepacketsize = packetsize+headeroverhead;

	// For now, we will always use the BYE backoff algorithm as described in rfc 3550 p 33
	
	byemembers = 1;
	pbyemembers = 1;

	if(schedparams.GetRequestImmediateBYE() && sources.GetActiveMemberCount() < 50) // p 34 (top)
		sendbyenow = true;
	else
		sendbyenow = false;
	
	prevrtcptime = RTPTime::CurrentTime();
	nextrtcptime = prevrtcptime;
	nextrtcptime += CalculateBYETransmissionInterval();
}



RTCPPacketBuilder::RTCPPacketBuilder(RTPSources &s,RTPPacketBuilder &pb/*,RTPMemoryManager *mgr*/)
	: /*RTPMemoryObject(mgr),*/sources(s),rtppacketbuilder(pb),prevbuildtime(0,0),transmissiondelay(0,0)
	//,ownsdesinfo(/*mgr*/) 
{

	init = false;
//	timeinit.Dummy();
	}

RTCPPacketBuilder::~RTCPPacketBuilder() {

	Destroy();
	}

int RTCPPacketBuilder::Init(size_t maxpacksize,double tsunit,const void *cname,size_t cnamelen) {

	if(init)
		return ERR_RTP_RTCPPACKETBUILDER_ALREADYINIT;
	if(maxpacksize < RTP_MINPACKETSIZE)
		return ERR_RTP_RTCPPACKETBUILDER_ILLEGALMAXPACKSIZE;
	if(tsunit < 0.0)
		return ERR_RTP_RTCPPACKETBUILDER_ILLEGALTIMESTAMPUNIT;

	if(cnamelen>255)
		cnamelen = 255;
	
	maxpacketsize = maxpacksize;
	timestampunit = tsunit;
	
	int status;
	
//	if((status = ownsdesinfo.SetCNAME((const BYTE *)cname,cnamelen)) < 0)
//		return status;
	
	ClearAllSourceFlags();
	
	interval_name = -1;
	interval_email = -1;
	interval_location = -1;
	interval_phone = -1;
	interval_tool = -1;
	interval_note = -1;

	sdesbuildcount = 0;
	transmissiondelay = RTPTime(0,0);

	firstpacket = true;
	processingsdes = false;
	init = true;
	return 0;
	}

void RTCPPacketBuilder::Destroy() {

	if(!init)
		return;
//	ownsdesinfo.Clear();
	init = false;
	}

int RTCPPacketBuilder::BuildNextPacket(RTCPCompoundPacket **pack) {

	if(!init)
		return ERR_RTP_RTCPPACKETBUILDER_NOTINIT;

	RTCPCompoundPacketBuilder *rtcpcomppack;
	int status;
	bool sender = false;
	RTPSourceData *srcdat;
	
	*pack = 0;
	
	rtcpcomppack = new RTCPCompoundPacketBuilder;
	if(!rtcpcomppack)
		return ERR_RTP_OUTOFMEM;
	
	if((status = rtcpcomppack->InitBuild(maxpacketsize)) < 0) {
		delete rtcpcomppack;
		return status;
	}
	
	if((srcdat = sources.GetOwnSourceInfo()) != 0) {
		if(srcdat->IsSender())
			sender = true;
	}
	
	DWORD ssrc = rtppacketbuilder.GetSSRC();
	RTPTime curtime = RTPTime::CurrentTime();

	if(sender)	{
		RTPTime rtppacktime = rtppacketbuilder.GetPacketTime();
		DWORD rtppacktimestamp = rtppacketbuilder.GetPacketTimestamp();
		DWORD packcount = rtppacketbuilder.GetPacketCount();
		DWORD octetcount = rtppacketbuilder.GetPayloadOctetCount();
		RTPTime diff = curtime;
		diff -= rtppacktime;
		diff += transmissiondelay; // the sample being sampled at this very instant will need a larger timestamp
		
		DWORD tsdiff = (DWORD)((diff.GetDouble()/timestampunit)+0.5);
		DWORD rtptimestamp = rtppacktimestamp+tsdiff;
		RTPNTPTime ntptimestamp = curtime.GetNTPTime();

		if((status = rtcpcomppack->StartSenderReport(ssrc,ntptimestamp,rtptimestamp,packcount,octetcount)) < 0) {
			delete rtcpcomppack;
			if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			return status;
		}
	}
	else {
		if((status = rtcpcomppack->StartReceiverReport(ssrc)) < 0)	{
			delete  rtcpcomppack;
			if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			return status;
		}
	}

	BYTE *owncname;
	size_t owncnamelen;

	owncname = ownsdesinfo.GetCNAME(&owncnamelen);

	if((status = rtcpcomppack->AddSDESSource(ssrc)) < 0)	{
		delete  rtcpcomppack;
		if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
		}
	if((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)	{
		delete  rtcpcomppack;
		if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
		}

	if(!processingsdes) {
		int added,skipped;
		bool full,atendoflist;

		if((status = FillInReportBlocks(rtcpcomppack,curtime,sources.GetTotalCount(),&full,&added,&skipped,&atendoflist)) < 0)	{
			delete  rtcpcomppack;
			return status;
			}
		
		if(full && added == 0)	{
			delete  rtcpcomppack;
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			}
	
		if(!full) {
			processingsdes = true;
			sdesbuildcount++;
			
			ClearAllSourceFlags();
	
			doname = false;
			doemail = false;
			doloc = false;
			dophone = false;
			dotool = false;
			donote = false;
			if(interval_name > 0 && ((sdesbuildcount%interval_name) == 0)) doname = true;
			if(interval_email > 0 && ((sdesbuildcount%interval_email) == 0)) doemail = true;
			if(interval_location > 0 && ((sdesbuildcount%interval_location) == 0)) doloc = true;
			if(interval_phone > 0 && ((sdesbuildcount%interval_phone) == 0)) dophone = true;
			if(interval_tool > 0 && ((sdesbuildcount%interval_tool) == 0)) dotool = true;
			if(interval_note > 0 && ((sdesbuildcount%interval_note) == 0)) donote = true;
			
			bool processedall;
			int itemcount;
			
			if((status = FillInSDES(rtcpcomppack,&full,&processedall,&itemcount)) < 0)	{
				delete  rtcpcomppack;
				return status;
			}

			if(processedall)	{
				processingsdes = false;
				ClearAllSDESFlags();
				if(!full && skipped > 0) {
					// if the packet isn't full and we skipped some sources that we already got in a previous packet,
					// we can add some of them now
					
					bool atendoflist;
					 
					if((status = FillInReportBlocks(rtcpcomppack,curtime,skipped,&full,&added,&skipped,&atendoflist)) < 0)	{
						delete  rtcpcomppack;
						return status;
						}
					}
				}
			}
		}
	else // previous sdes processing wasn't finished
	{
		bool processedall;
		int itemcount;
		bool full;
			
		if((status = FillInSDES(rtcpcomppack,&full,&processedall,&itemcount)) < 0)	{
			delete  rtcpcomppack;
			return status;
		}

		if(itemcount == 0) // Big problem: packet size is too small to let any progress happen
		{
			delete  rtcpcomppack;
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			}

		if(processedall)	{
			processingsdes = false;
			ClearAllSDESFlags();
			if(!full) {
				// if the packet isn't full and we skipped some
				// we can add some report blocks
				
				int added,skipped;
				bool atendoflist;

				if((status = FillInReportBlocks(rtcpcomppack,curtime,sources.GetTotalCount(),&full,&added,&skipped,&atendoflist)) < 0) {
					delete  rtcpcomppack;
					return status;
					}
				if(atendoflist) // filled in all possible sources
					ClearAllSourceFlags();
				}
			}
		}
		
	if((status = rtcpcomppack->EndBuild()) < 0) {
		delete  rtcpcomppack;
		return status;
		}

	*pack = rtcpcomppack;
	firstpacket = false;
	prevbuildtime = curtime;
	return 0;
	}

void RTCPPacketBuilder::ClearAllSourceFlags() {

	if(sources.GotoFirstSource()) {
		do {
			RTPSourceData *srcdat = sources.GetCurrentSourceInfo();
			srcdat->SetProcessedInRTCP(false);
			} while(sources.GotoNextSource());
		}
	}

int RTCPPacketBuilder::FillInReportBlocks(RTCPCompoundPacketBuilder *rtcpcomppack,const RTPTime &curtime,int maxcount,bool *full,int *added,int *skipped,bool *atendoflist) {
	RTPSourceData *srcdat;
	int addedcount = 0;
	int skippedcount = 0;
	bool done = false;
	bool filled = false;
	bool atend = false;
	int status;

	if(sources.GotoFirstSource()) {
		do {
			bool shouldprocess = false;
			
			srcdat = sources.GetCurrentSourceInfo();
			if(!srcdat->IsOwnSSRC()) // don't send to ourselves
			{
				if(!srcdat->IsCSRC()) // p 35: no reports should go to CSRCs
				{
					if(srcdat->INF_HasSentData()) // if this isn't true, INF_GetLastRTPPacketTime() won't make any sense
					{
						if(firstpacket)
							shouldprocess = true;
						else
						{
							// p 35: only if rtp packets were received since the last RTP packet, a report block
							// should be added
							
							RTPTime lastrtptime = srcdat->INF_GetLastRTPPacketTime();
							
							if(lastrtptime > prevbuildtime)
								shouldprocess = true;
						}
					}
				}
			}

			if(shouldprocess) {
				if(srcdat->IsProcessedInRTCP()) // already covered this one
				{
					skippedcount++;
				}
				else {
					DWORD rr_ssrc = srcdat->GetSSRC();
					DWORD num = srcdat->INF_GetNumPacketsReceivedInInterval();
					DWORD prevseq = srcdat->INF_GetSavedExtendedSequenceNumber();
					DWORD curseq = srcdat->INF_GetExtendedHighestSequenceNumber();
					DWORD expected = curseq-prevseq;
					BYTE fraclost;
					
					if(expected < num) // got duplicates
						fraclost = 0;
					else {
						double lost = (double)(expected-num);
						double frac = lost/((double)expected);
						fraclost = (BYTE)(frac*256.0);
						}

					expected = curseq-srcdat->INF_GetBaseSequenceNumber();
					num = srcdat->INF_GetNumPacketsReceived();

					DWORD diff = expected-num;
					int *packlost = (int *)&diff;
					
					DWORD jitter = srcdat->INF_GetJitter();
					DWORD lsr;
					DWORD dlsr; 	

					if(!srcdat->SR_HasInfo())	{
						lsr = 0;
						dlsr = 0;
						}
					else {
						RTPNTPTime srtime = srcdat->SR_GetNTPTimestamp();
						DWORD m = (srtime.GetMSW()&0xFFFF);
						DWORD l = ((srtime.GetLSW()>>16)&0xFFFF);
						lsr = ((m<<16)|l);

						RTPTime diff = curtime;
						diff -= srcdat->SR_GetReceiveTime();
						double diff2 = diff.GetDouble();
						diff2 *= 65536.0;
						dlsr = (DWORD)diff2;
						}

					status = rtcpcomppack->AddReportBlock(rr_ssrc,fraclost,*packlost,curseq,jitter,lsr,dlsr);
					if(status < 0) {
						if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)	{
							done = true;
							filled = true;
							}
						else
							return status;
						}
					else {
						addedcount++;
						if(addedcount >= maxcount) {
							done = true;
							if(!sources.GotoNextSource())
								atend = true;
							}
						srcdat->INF_StartNewInterval();
						srcdat->SetProcessedInRTCP(true);
						}
					}
				}

			if(!done) {
				if(!sources.GotoNextSource()) {
					atend = true;
					done = true;
					}
				}

			} while(!done);
		}
	
	*added = addedcount;
	*skipped = skippedcount;
	*full = filled;
	
	if(!atend) // search for available sources
	{
		bool shouldprocess = false;
		
		do {	
			srcdat = sources.GetCurrentSourceInfo();
			if(!srcdat->IsOwnSSRC()) // don't send to ourselves
			{
				if(!srcdat->IsCSRC()) // p 35: no reports should go to CSRCs
				{
					if(srcdat->INF_HasSentData()) // if this isn't true, INF_GetLastRTPPacketTime() won't make any sense
					{
						if(firstpacket)
							shouldprocess = true;
						else	{
							// p 35: only if rtp packets were received since the last RTP packet, a report block should be added
							RTPTime lastrtptime = srcdat->INF_GetLastRTPPacketTime();
							
							if(lastrtptime > prevbuildtime)
								shouldprocess = true;
						}
					}
				}
			}
			
			if(shouldprocess)	{
				if(srcdat->IsProcessedInRTCP())
					shouldprocess = false;
				}

			if(!shouldprocess) {
				if(!sources.GotoNextSource())
					atend = true;
				}
	
			} while(!atend && !shouldprocess);
		}	

	*atendoflist = atend;
	return 0;	
	}

int RTCPPacketBuilder::FillInSDES(RTCPCompoundPacketBuilder *rtcpcomppack,bool *full,bool *processedall,int *added) {
	int status;
	BYTE *data;
	size_t datalen;
	
	*full = false;
	*processedall = false;
	*added = 0;

	// We don't need to add a SSRC for our own data, this is still set
	// from adding the CNAME
	if(doname)	{
		if(!ownsdesinfo.ProcessedName())	{
			data = ownsdesinfo.GetName(&datalen);
			if((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::NAME,data,datalen)) < 0) {
				if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)	{
					*full = true;
					return 0;
					}
				}
			(*added)++;
			ownsdesinfo.SetProcessedName(true);
			}
		}
	if(doemail) {
		if(!ownsdesinfo.ProcessedEMail()) {
			data = ownsdesinfo.GetEMail(&datalen);
			if((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::EMAIL,data,datalen)) < 0)	{
				if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)	{
					*full = true;
					return 0;
					}
				}
			(*added)++;
			ownsdesinfo.SetProcessedEMail(true);
			}
		}
	if(doloc) {
		if(!ownsdesinfo.ProcessedLocation()) {
			data = ownsdesinfo.GetLocation(&datalen);
			if((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::LOC,data,datalen)) < 0)	{
				if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)	{
					*full = true;
					return 0;
					}
				}
			(*added)++;
			ownsdesinfo.SetProcessedLocation(true);
			}
		}
	if(dophone) {
		if(!ownsdesinfo.ProcessedPhone()) {
			data = ownsdesinfo.GetPhone(&datalen);
			if((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::PHONE,data,datalen)) < 0)	{
				if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)	{
					*full = true;
					return 0;
					}
				}
			(*added)++;
			ownsdesinfo.SetProcessedPhone(true);
			}
		}
	if(dotool)	{
		if(!ownsdesinfo.ProcessedTool())	{
			data = ownsdesinfo.GetTool(&datalen);
			if((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::TOOL,data,datalen)) < 0) {
				if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)	{
					*full = true;
					return 0;
					}
				}
			(*added)++;
			ownsdesinfo.SetProcessedTool(true);
			}
		}
	if(donote) {
		if(!ownsdesinfo.ProcessedNote())	{
			data = ownsdesinfo.GetNote(&datalen);
			if((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::NOTE,data,datalen)) < 0) {
				if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)	{
					*full = true;
					return 0;
					}
				}
			(*added)++;
			ownsdesinfo.SetProcessedNote(true);
			}
		}

	*processedall = true;
	return 0;
	}

void RTCPPacketBuilder::ClearAllSDESFlags() {

	ownsdesinfo.ClearFlags();
	}
	
int RTCPPacketBuilder::BuildBYEPacket(RTCPCompoundPacket **pack,const void *reason,size_t reasonlength,bool useSRifpossible) {
	if(!init)
		return ERR_RTP_RTCPPACKETBUILDER_NOTINIT;

	RTCPCompoundPacketBuilder *rtcpcomppack;
	int status;
	
	if(reasonlength > 255)
		reasonlength = 255;
	
	*pack = 0;
	
	rtcpcomppack = new RTCPCompoundPacketBuilder();
	if(!rtcpcomppack)
		return ERR_RTP_OUTOFMEM;
	
	if((status = rtcpcomppack->InitBuild(maxpacketsize)) < 0) {
		delete rtcpcomppack;
		return status;
		}
	
	DWORD ssrc = rtppacketbuilder.GetSSRC();
	bool useSR = false;
	
	if(useSRifpossible) {
		RTPSourceData *srcdat;
		
		if((srcdat = sources.GetOwnSourceInfo()) != 0)	{
			if(srcdat->IsSender())
				useSR = true;
			}
		}
			
	if(useSR) {
		RTPTime curtime = RTPTime::CurrentTime();
		RTPTime rtppacktime = rtppacketbuilder.GetPacketTime();
		DWORD rtppacktimestamp = rtppacketbuilder.GetPacketTimestamp();
		DWORD packcount = rtppacketbuilder.GetPacketCount();
		DWORD octetcount = rtppacketbuilder.GetPayloadOctetCount();
		RTPTime diff = curtime;
		diff -= rtppacktime;
		
		DWORD tsdiff = (DWORD)((diff.GetDouble()/timestampunit)+0.5);
		DWORD rtptimestamp = rtppacktimestamp+tsdiff;
		RTPNTPTime ntptimestamp = curtime.GetNTPTime();

		if((status = rtcpcomppack->StartSenderReport(ssrc,ntptimestamp,rtptimestamp,packcount,octetcount)) < 0) {
			delete rtcpcomppack;
			if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			return status;
		}
	}
	else {
		if((status = rtcpcomppack->StartReceiverReport(ssrc)) < 0)	{
			delete rtcpcomppack;
			if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			return status;
		}
	}

	BYTE *owncname;
	size_t owncnamelen;

	owncname = ownsdesinfo.GetCNAME(&owncnamelen);

	if((status = rtcpcomppack->AddSDESSource(ssrc)) < 0)	{
		delete rtcpcomppack;
		if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
		}
	if((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)	{
		delete rtcpcomppack;
		if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
		}

	DWORD ssrcs[1];
	ssrcs[0] = ssrc;
	
	if((status = rtcpcomppack->AddBYEPacket(ssrcs,1,(const BYTE *)reason,reasonlength)) < 0)	{
		delete rtcpcomppack;
		if(status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
		}
	
	if((status = rtcpcomppack->EndBuild()) < 0) {
		delete rtcpcomppack;
		return status;
		}

	*pack = rtcpcomppack;
	return 0;
	}

RTCPCompoundPacket::RTCPCompoundPacket(RTPRawPacket &rawpack) {

	compoundpacket = 0;
	compoundpacketlength = 0;
	error = 0;
	
	if(rawpack.IsRTP()) {
		error = ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
		return;
		}

	BYTE *data = rawpack.GetData();
	size_t datalen = rawpack.GetDataLength();

	error = ParseData(data,datalen);
	if(error < 0)
		return;
	
	compoundpacket = rawpack.GetData();
	compoundpacketlength = rawpack.GetDataLength();
	deletepacket = true;

	rawpack.ZeroData();
	
	rtcppackit = rtcppacklist.GetHeadPosition();
	}

RTCPCompoundPacket::RTCPCompoundPacket(BYTE *packet, size_t packetlen, bool deletedata) {

	compoundpacket = 0;
	compoundpacketlength = 0;
	
	error = ParseData(packet,packetlen);
	if(error < 0)
		return;
	
	compoundpacket = packet;
	compoundpacketlength = packetlen;
	deletepacket = deletedata;

	rtcppackit = rtcppacklist.GetHeadPosition();
	}

RTCPCompoundPacket::RTCPCompoundPacket() {

	compoundpacket = 0;
	compoundpacketlength = 0;
	error = 0;
	deletepacket = true;
	}

int RTCPCompoundPacket::ParseData(BYTE *data, size_t datalen) {
	bool first;
	
	if(datalen < sizeof(RTCPCommonHeader))
		return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;

	first = true;
	
	do {
		RTCPCommonHeader *rtcphdr;
		size_t length;
		
		rtcphdr = (RTCPCommonHeader *)data;
		if(rtcphdr->version != RTP_VERSION) // check version
		{
			ClearPacketList();
			return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
			}
		if(first) {
			// Check if first packet is SR or RR
			first = false;
			if(!(rtcphdr->packettype == RTP_RTCPTYPE_SR || rtcphdr->packettype == RTP_RTCPTYPE_RR)) {
				ClearPacketList();
				return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
				}
			}
		
		length = (size_t)ntohs(rtcphdr->length);
		length++;
		length *= sizeof(DWORD);

		if(length > datalen) // invalid length field
		{
			ClearPacketList();
			return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
			}
		
		if(rtcphdr->padding)	{
			// check if it's the last packet
			if(length != datalen) {
				ClearPacketList();
				return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
				}
			}

		RTCPPacket *p;
		
		switch(rtcphdr->packettype) {
			case RTP_RTCPTYPE_SR:
				p = new RTCPSRPacket(data,length);
				break;
			case RTP_RTCPTYPE_RR:
				p = new RTCPRRPacket(data,length);
				break;
			case RTP_RTCPTYPE_SDES:
				p = new RTCPSDESPacket(data,length);
				break;
			case RTP_RTCPTYPE_BYE:
				p = new RTCPBYEPacket(data,length);
				break;
			case RTP_RTCPTYPE_APP:
				p = new RTCPAPPPacket(data,length);
				break;
			default:
	//			p = new RTCPUnknownPacket(data,length);		GD
				;
			}

		if(!p) {
			ClearPacketList();
			return ERR_RTP_OUTOFMEM;
			}

		rtcppacklist.AddTail(p);
		
		datalen -= length;
		data += length;
		} while(datalen >= (size_t)sizeof(RTCPCommonHeader));

	if(datalen != 0) // some remaining bytes
	{
		ClearPacketList();
		return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
		}
	return 0;
	}

RTCPCompoundPacket::~RTCPCompoundPacket() {

	ClearPacketList();
	if(compoundpacket && deletepacket)
		delete compoundpacket;
	}

void RTCPCompoundPacket::ClearPacketList() {

	for(POSITION pos = rtcppacklist.GetHeadPosition(); pos;)
		delete rtcppacklist.GetNext(pos);
	rtcppacklist.RemoveAll();
	rtcppackit = rtcppacklist.GetHeadPosition();
	}

#ifdef RTPDEBUG
void RTCPCompoundPacket::Dump() {

	std::list<RTCPPacket *>::const_iterator it;
	for(it = rtcppacklist.begin() ; it != rtcppacklist.end() ; it++)	{
		RTCPPacket *p = *it;

		p->Dump();
		}
	}
#endif // RTPDEBUG


RTCPCompoundPacketBuilder::RTCPCompoundPacketBuilder() {

	byesize = 0;
	appsize = 0;
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownsize = 0;
#endif // RTP_SUPPORT_RTCPUNKNOWN
	maximumpacketsize = 0;
	buffer = 0;
	external = false;
	arebuilding = false;
	}

RTCPCompoundPacketBuilder::~RTCPCompoundPacketBuilder() {

	if(external)
		compoundpacket = 0; // make sure RTCPCompoundPacket doesn't delete the external buffer
	ClearBuildBuffers();
	}

void RTCPCompoundPacketBuilder::ClearBuildBuffers() {

	report.Clear();
	sdes.Clear();

	for(POSITION pos = byepackets.GetHeadPosition(); pos; )
		delete byepackets.GetNext(pos).packetdata;
	for(pos = apppackets.GetHeadPosition(); pos; )
		delete apppackets.GetNext(pos).packetdata;
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	for(pos = unknownpackets.GetHeadPosition(); pos; )
		delete unknownpackets.GetNext(pos).packetdata;
#endif // RTP_SUPPORT_RTCPUNKNOWN 

	byepackets.RemoveAll();
	apppackets.RemoveAll();
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownpackets.RemoveAll();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	byesize = 0;
	appsize = 0;
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownsize = 0;
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	}

int RTCPCompoundPacketBuilder::InitBuild(size_t maxpacketsize) {

	if(arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILDING;
	if(compoundpacket)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILT;

	if(maxpacketsize < RTP_MINPACKETSIZE)
		return ERR_RTP_RTCPCOMPPACKBUILDER_MAXPACKETSIZETOOSMALL;
	
	maximumpacketsize = maxpacketsize;
	buffer = 0;
	external = false;
	byesize = 0;
	appsize = 0;
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownsize = 0;
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	
	arebuilding = true;
	return 0;
	}

int RTCPCompoundPacketBuilder::InitBuild(void *externalbuffer,size_t buffersize) {

	if(arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILDING;
	if(compoundpacket)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILT;

	if(buffersize < RTP_MINPACKETSIZE)
		return ERR_RTP_RTCPCOMPPACKBUILDER_BUFFERSIZETOOSMALL;

	maximumpacketsize = buffersize;
	buffer = (BYTE*)externalbuffer;
	external = true;
	byesize = 0;
	appsize = 0;
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownsize = 0;
#endif // RTP_SUPPORT_RTCPUNKNOWN 

	arebuilding = true;
	return 0;
	}

int RTCPCompoundPacketBuilder::EndBuild() {

	if(!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if(report.headerlength == 0)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOREPORTPRESENT;
	
	BYTE *buf;
	size_t len;
	
#ifndef RTP_SUPPORT_RTCPUNKNOWN
	len = appsize+byesize+report.NeededBytes()+sdes.NeededBytes();
#else
	len = appsize+unknownsize+byesize+report.NeededBytes()+sdes.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	
	if(!external) {
		buf = new BYTE[len];
		if(!buf)
			return ERR_RTP_OUTOFMEM;
		}
	else
		buf = buffer;
	
	BYTE *curbuf = buf;
	RTCPPacket *p;

	// first, we'll add all report info
	{
		bool firstpacket = true;
		bool done = false;
		POSITION it = report.reportblocks.GetHeadPosition();
		do {
			RTCPCommonHeader *hdr = (RTCPCommonHeader *)curbuf;
			size_t offset;
			
			hdr->version = 2;
			hdr->padding = 0;

			if(firstpacket && report.isSR) {
				hdr->packettype = RTP_RTCPTYPE_SR;
				memcpy((curbuf+sizeof(RTCPCommonHeader)),report.headerdata,report.headerlength);
				offset = sizeof(RTCPCommonHeader)+report.headerlength;
				}
			else {
				hdr->packettype = RTP_RTCPTYPE_RR;
				memcpy((curbuf+sizeof(RTCPCommonHeader)),report.headerdata,sizeof(DWORD));
				offset = sizeof(RTCPCommonHeader)+sizeof(DWORD);
				}
			firstpacket = false;
			
			BYTE count = 0;
			while(it != report.reportblocks.GetTailPosition() && count < 31)	{
				memcpy(curbuf+offset,report.reportblocks.GetAt(it).packetdata,report.reportblocks.GetAt(it).packetlength);
				offset += report.reportblocks.GetAt(it).packetlength;
				count++;
				report.reportblocks.GetNext(it);
				}

			size_t numwords = offset/sizeof(DWORD);

			hdr->length = htons((WORD)(numwords-1));
			hdr->count = count;

			// add entry in parent's list
			if(hdr->packettype == RTP_RTCPTYPE_SR)
				p = new RTCPSRPacket(curbuf,offset);
			else
				p = new RTCPRRPacket(curbuf,offset);
			if(!p)	{
				if(!external)
					delete []buf;
				ClearPacketList();
				return ERR_RTP_OUTOFMEM;
				}
			rtcppacklist.AddTail(p);

			curbuf += offset;
			if(it == report.reportblocks.GetTailPosition())
				done = true;
			} while(!done);
		}
		
	// then, we'll add the sdes info
	if(!sdes.sdessources.IsEmpty()) {
		bool done = false;
		POSITION sourceit = sdes.sdessources.GetHeadPosition();
		
		do {
			RTCPCommonHeader *hdr = (RTCPCommonHeader *)curbuf;
			size_t offset = sizeof(RTCPCommonHeader);
			
			hdr->version = 2;
			hdr->padding = 0;
			hdr->packettype = RTP_RTCPTYPE_SDES;

			BYTE sourcecount = 0;
			while(sourceit != sdes.sdessources.GetTailPosition() && sourcecount < 31)	{
				DWORD *ssrc = (DWORD*)(curbuf+offset);
				*ssrc = htonl(sdes.sdessources.GetAt(sourceit)->ssrc);
				offset += sizeof(DWORD);
				
				POSITION itemit,itemend;
				itemit = sdes.sdessources.GetAt(sourceit)->items.GetHeadPosition();
				itemend = sdes.sdessources.GetAt(sourceit)->items.GetTailPosition();
				while(itemit != itemend)	{
					memcpy(curbuf+offset,sdes.sdessources.GetAt(sourceit)->items.GetAt(itemit).packetdata,
						sdes.sdessources.GetAt(sourceit)->items.GetAt(itemit).packetlength);
					offset += sdes.sdessources.GetAt(sourceit)->items.GetAt(itemit).packetlength;
					sdes.sdessources.GetNext(itemit);
					}

				curbuf[offset] = 0; // end of item list;
				offset++;

				size_t r = offset & 0x03;
				if(r != 0) // align to 32 bit boundary
				{
					size_t num = 4-r;
					size_t i;

					for(i=0; i < num ; i++)
						curbuf[offset+i] = 0;
					offset += num;
					}
				
				sdes.sdessources.GetNext(sourceit);
				sourcecount++;
				}

			size_t numwords = offset/4;
			
			hdr->count = sourcecount;
			hdr->length = htons((WORD)(numwords-1));

			p = new RTCPSDESPacket(curbuf,offset);
			if(!p)	{
				if(!external)
					delete []buf;
				ClearPacketList();
				return ERR_RTP_OUTOFMEM;
				}
			rtcppacklist.AddTail(p);
			
			curbuf += offset;
			if(sourceit == sdes.sdessources.GetTailPosition())
				done = true;
			} while(!done);
		}
	
	// adding the app data
	{
		for(POSITION pos = apppackets.GetHeadPosition(); pos;) {
			memcpy(curbuf,apppackets.GetAt(pos).packetdata,apppackets.GetAt(pos).packetlength);
			
			p = new RTCPAPPPacket(curbuf,apppackets.GetAt(pos).packetlength);
			if(!p)	{
				if(!external)
					delete []buf;
				ClearPacketList();
				return ERR_RTP_OUTOFMEM;
				}
			rtcppacklist.AddTail(p);
	
			curbuf += apppackets.GetAt(pos).packetlength;
				apppackets.GetNext(pos);
			}
		}

#ifdef RTP_SUPPORT_RTCPUNKNOWN
	// adding the unknown data
	{
		for(POSITION pos = unknownpackets.GetHeadPosition(); pos;) {
			memcpy(curbuf,(*it).packetdata,(*it).packetlength);
			
			p = new RTCPUnknownPacket(curbuf,(*it).packetlength);
			if(!p) {
				if(!external)
					delete []buf;
				ClearPacketList();
				return ERR_RTP_OUTOFMEM;
				}
			rtcppacklist.AddTail(p);
	
			curbuf += (*it).packetlength;
			unknownpackets.GetNext(pos);
			}
	}
#endif // RTP_SUPPORT_RTCPUNKNOWN 

	// adding bye packets
	{
		for(POSITION pos = byepackets.GetHeadPosition(); pos;) {
			memcpy(curbuf,byepackets.GetAt(pos).packetdata,byepackets.GetAt(pos).packetlength);
			
			p = new RTCPBYEPacket(curbuf,byepackets.GetAt(pos).packetlength);
			if(!p) {
				if(!external)
					delete []buf;
				ClearPacketList();
				return ERR_RTP_OUTOFMEM;
				}
			rtcppacklist.AddTail(p);
	
			curbuf += byepackets.GetAt(pos).packetlength;
			byepackets.GetNext(pos);
			}
		}
	
	compoundpacket = buf;
	compoundpacketlength = len;
	arebuilding = false;
	ClearBuildBuffers();
	return 0;
	}

int RTCPCompoundPacketBuilder::AddReportBlock(DWORD ssrc,BYTE fractionlost,int packetslost,DWORD exthighestseq,
	                                      DWORD jitter,DWORD lsr,DWORD dlsr) {

	if(!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if(report.headerlength == 0)
		return ERR_RTP_RTCPCOMPPACKBUILDER_REPORTNOTSTARTED;

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalothersize = byesize+appsize+sdes.NeededBytes();
#else
	size_t totalothersize = byesize+appsize+unknownsize+sdes.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t reportsizewithextrablock = report.NeededBytesWithExtraReportBlock();
	
	if((totalothersize+reportsizewithextrablock) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	BYTE *buf = new BYTE[sizeof(RTCPReceiverReport)];
	if(!buf)
		return ERR_RTP_OUTOFMEM;
	
	RTCPReceiverReport *rr = (RTCPReceiverReport *)buf;
	DWORD *packlost = (DWORD *)&packetslost;
	DWORD packlost2 = (*packlost);
		
	rr->ssrc = htonl(ssrc);
	rr->fractionlost = fractionlost;
	rr->packetslost[2] = (BYTE)(packlost2&0xFF);
	rr->packetslost[1] = (BYTE)((packlost2>>8)&0xFF);
	rr->packetslost[0] = (BYTE)((packlost2>>16)&0xFF);
	rr->exthighseqnr = htonl(exthighestseq);
	rr->jitter = htonl(jitter);
	rr->lsr = htonl(lsr);
	rr->dlsr = htonl(dlsr);

	report.reportblocks.AddTail(Buffer(buf,sizeof(RTCPReceiverReport)));
	return 0;
	}

int RTCPCompoundPacketBuilder::AddSDESSource(DWORD ssrc) {

	if(!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalotherbytes = byesize+appsize+report.NeededBytes();
#else
	size_t totalotherbytes = byesize+appsize+unknownsize+report.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t sdessizewithextrasource = sdes.NeededBytesWithExtraSource();

	if((totalotherbytes + sdessizewithextrasource) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	int status;

	if((status = sdes.AddSSRC(ssrc)) < 0)
		return status;
	return 0;
	}

int RTCPCompoundPacketBuilder::AddSDESNormalItem(RTCPSDESPacket::ItemType t,const void *itemdata,BYTE itemlength) {

	if(!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if(sdes.sdessources.IsEmpty())
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOCURRENTSOURCE;

	BYTE itemid;
	
	switch(t)	{
		case RTCPSDESPacket::CNAME:
			itemid = RTCP_SDES_ID_CNAME;
			break;
		case RTCPSDESPacket::NAME:
			itemid = RTCP_SDES_ID_NAME;
			break;
		case RTCPSDESPacket::EMAIL:
			itemid = RTCP_SDES_ID_EMAIL;
			break;
		case RTCPSDESPacket::PHONE:
			itemid = RTCP_SDES_ID_PHONE;
			break;
		case RTCPSDESPacket::LOC:
			itemid = RTCP_SDES_ID_LOCATION;
			break;
		case RTCPSDESPacket::TOOL:
			itemid = RTCP_SDES_ID_TOOL;
			break;
		case RTCPSDESPacket::NOTE:
			itemid = RTCP_SDES_ID_NOTE;
			break;
		default:
			return ERR_RTP_RTCPCOMPPACKBUILDER_INVALIDITEMTYPE;
		}

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalotherbytes = byesize+appsize+report.NeededBytes();
#else
	size_t totalotherbytes = byesize+appsize+unknownsize+report.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t sdessizewithextraitem = sdes.NeededBytesWithExtraItem(itemlength);

	if((sdessizewithextraitem+totalotherbytes) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	BYTE *buf;
	size_t len;

	buf = new BYTE[sizeof(RTCPSDESHeader)+(size_t)itemlength];
	if(!buf)
		return ERR_RTP_OUTOFMEM;
	len = sizeof(RTCPSDESHeader)+(size_t)itemlength;

	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(buf);

	sdeshdr->sdesid = itemid;
	sdeshdr->length = itemlength;
	if(itemlength != 0)
		memcpy((buf + sizeof(RTCPSDESHeader)),itemdata,(size_t)itemlength);

	sdes.AddItem(buf,len);
	return 0;
	}

int RTCPCompoundPacketBuilder::AddBYEPacket(DWORD *ssrcs,BYTE numssrcs,const void *reasondata,BYTE reasonlength) {

	if(!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;

	if(numssrcs > 31)
		return ERR_RTP_RTCPCOMPPACKBUILDER_TOOMANYSSRCS;
	
	size_t packsize = sizeof(RTCPCommonHeader)+sizeof(DWORD)*((size_t)numssrcs);
	size_t zerobytes = 0;
	
	if(reasonlength > 0) {
		packsize += 1; // 1 byte for the length;
		packsize += (size_t)reasonlength;

		size_t r = (packsize&0x03);
		if(r != 0) {
			zerobytes = 4-r;
			packsize += zerobytes;
			}
		}

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalotherbytes = appsize+byesize+sdes.NeededBytes()+report.NeededBytes();
#else
	size_t totalotherbytes = appsize+unknownsize+byesize+sdes.NeededBytes()+report.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 

	if((totalotherbytes + packsize) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	BYTE *buf;
	size_t numwords;
	
	buf = new BYTE[packsize];
	if(!buf)
		return ERR_RTP_OUTOFMEM;

	RTCPCommonHeader *hdr = (RTCPCommonHeader *)buf;

	hdr->version = 2;
	hdr->padding = 0;
	hdr->count = numssrcs;
	
	numwords = packsize/sizeof(DWORD);
	hdr->length = htons((WORD)(numwords-1));
	hdr->packettype = RTP_RTCPTYPE_BYE;
	
	DWORD *sources = (DWORD *)(buf+sizeof(RTCPCommonHeader));
	BYTE srcindex;
	
	for(srcindex=0; srcindex < numssrcs; srcindex++)
		sources[srcindex] = htonl(ssrcs[srcindex]);

	if(reasonlength != 0) {
		size_t offset = sizeof(RTCPCommonHeader)+((size_t)numssrcs)*sizeof(DWORD);

		buf[offset] = reasonlength;
		memcpy((buf+offset+1),reasondata,(size_t)reasonlength);
		for(size_t i=0; i < zerobytes; i++)
			buf[packsize-1-i] = 0;
		}

	byepackets.AddTail(Buffer(buf,packsize));
	byesize += packsize;
	
	return 0;
	}

int RTCPCompoundPacketBuilder::StartSenderReport(DWORD senderssrc,const RTPNTPTime &ntptimestamp,DWORD rtptimestamp,
                                                 DWORD packetcount,DWORD octetcount) {

	if(!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;

	if(report.headerlength != 0)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYGOTREPORT;

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalsize = byesize+appsize+sdes.NeededBytes();
#else
	size_t totalsize = byesize+appsize+unknownsize+sdes.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t sizeleft = maximumpacketsize-totalsize;
	size_t neededsize = sizeof(RTCPCommonHeader)+sizeof(DWORD)+sizeof(RTCPSenderReport);
	
	if(neededsize > sizeleft)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;
	
	// fill in some things

	report.headerlength = sizeof(DWORD)+sizeof(RTCPSenderReport);
	report.isSR = true;	
	
	DWORD *ssrc = (DWORD *)report.headerdata;
	*ssrc = htonl(senderssrc);

	RTCPSenderReport *sr = (RTCPSenderReport *)(report.headerdata + sizeof(DWORD));
	sr->ntptime_msw = htonl(ntptimestamp.GetMSW());
	sr->ntptime_lsw = htonl(ntptimestamp.GetLSW());
	sr->rtptimestamp = htonl(rtptimestamp);
	sr->packetcount = htonl(packetcount);
	sr->octetcount = htonl(octetcount);

	return 0;
	}

int RTCPCompoundPacketBuilder::StartReceiverReport(DWORD senderssrc) {

	if(!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if(report.headerlength != 0)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYGOTREPORT;

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalsize = byesize+appsize+sdes.NeededBytes();
#else
	size_t totalsize = byesize+appsize+unknownsize+sdes.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t sizeleft = maximumpacketsize-totalsize;
	size_t neededsize = sizeof(RTCPCommonHeader)+sizeof(DWORD);
	
	if(neededsize > sizeleft)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;
	
	// fill in some things
	report.headerlength = sizeof(DWORD);
	report.isSR = false;
	
	DWORD *ssrc = (DWORD *)report.headerdata;
	*ssrc = htonl(senderssrc);

	return 0;
	}


RTCPSDESPacket::RTCPSDESPacket(BYTE *data,size_t datalength)
	: RTCPPacket(SDES,data,datalength) {
	knownformat = false;
	currentchunk = 0;
	itemoffset = 0;
	curchunknum = 0;
		
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	size_t len = datalength;
	
	if(hdr->padding)	{
		BYTE padcount = data[datalength-1];
		if((padcount & 0x03) != 0) // not a multiple of four! (see rfc 3550 p 37)
			return;
		if(((size_t)padcount) >= len)
			return;
		len -= (size_t)padcount;
		}
	
	if(hdr->count == 0) {
		if(len != sizeof(RTCPCommonHeader))
			return;
		}
	else {
		int ssrccount = (int)(hdr->count);
		BYTE *chunk;
		int chunkoffset;
		
		if(len < sizeof(RTCPCommonHeader))
			return;
		
		len -= sizeof(RTCPCommonHeader);
		chunk = data+sizeof(RTCPCommonHeader);
		
		while((ssrccount > 0) && (len > 0)) {
			if(len < (sizeof(DWORD)*2)) // chunk must contain at least a SSRC identifier
				return;                  // and a (possibly empty) item
			
			len -= sizeof(DWORD);
			chunkoffset = sizeof(DWORD);

			bool done = false;
			while(!done)	{
				if(len < 1) // at least a zero byte (end of item list) should be there
					return;
				
				RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(chunk+chunkoffset);
				if(sdeshdr->sdesid == 0) // end of item list
				{
					len--;
					chunkoffset++;

					size_t r = (chunkoffset&0x03);
					if(r != 0)	{
						size_t addoffset = 4-r;
					
						if(addoffset > len)
							return;
						len -= addoffset;
						chunkoffset += addoffset;
						}
					done = true;
					}
				else {
					if(len < sizeof(RTCPSDESHeader))
						return;
					
					len -= sizeof(RTCPSDESHeader);
					chunkoffset += sizeof(RTCPSDESHeader);
					
					size_t itemlen = (size_t)(sdeshdr->length);
					if(itemlen > len)
						return;
					
					len -= itemlen;
					chunkoffset += itemlen;
					}		
				}
			
			ssrccount--;
			chunk += chunkoffset;
			}

		// check for remaining bytes
		if(len > 0)
			return;
		if(ssrccount > 0)
			return;
		}

	knownformat = true;
	}

#ifdef RTPDEBUG
void RTCPSDESPacket::Dump() {

	RTCPPacket::Dump();
	if(!IsKnownFormat()) {
		std::cout << "    Unknown format" << std::endl;
		return;
		}
	if(!GotoFirstChunk())	{
		std::cout << "    No chunks present" << std::endl;
		return;
		}
	
	do {
		std::cout << "    SDES Chunk for SSRC:    " << GetChunkSSRC() << std::endl;
		if(!GotoFirstItem())
			std::cout << "        No items found" << std::endl; 
		else {
			do {
				std::cout << "        ";
				switch(GetItemType()) {
					case None:
						std::cout << "None    ";
						break;
					case CNAME:
						std::cout << "CNAME   ";
						break;
					case NAME:
						std::cout << "NAME    ";
						break;
					case EMAIL:
						std::cout << "EMAIL   ";
						break;
					case PHONE:
						std::cout << "PHONE   ";
						break;
					case LOC:
						std::cout << "LOC     ";
						break;
					case TOOL:
						std::cout << "TOOL    ";
						break;
					case NOTE:
						std::cout << "NOTE    ";
						break;
					case PRIV:
						std::cout << "PRIV    ";
						break;
					case Unknown:
					default:
						std::cout << "Unknown ";
					}
				
				std::cout << "Length: " << GetItemLength() << std::endl;

				if(GetItemType() != PRIV)	{
					char str[1024];
					memcpy(str,GetItemData(),GetItemLength());
					str[GetItemLength()] = 0;
					std::cout << "                Value:  " << str << std::endl;
					}
#ifdef RTP_SUPPORT_SDESPRIV
				else // PRIV item
				{
					char str[1024];
					memcpy(str,GetPRIVPrefixData(),GetPRIVPrefixLength());
					str[GetPRIVPrefixLength()] = 0;
					std::cout << "                Prefix: " << str << std::endl;
					std::cout << "                Length: " << GetPRIVPrefixLength() << std::endl;
					memcpy(str,GetPRIVValueData(),GetPRIVValueLength());
					str[GetPRIVValueLength()] = 0;
					std::cout << "                Value:  " << str << std::endl;
					std::cout << "                Length: " << GetPRIVValueLength() << std::endl;
					}
#endif // RTP_SUPPORT_SDESPRIV
				} while(GotoNextItem());
			}
		} while(GotoNextChunk());
	}
#endif // RTPDEBUG


RTPCollisionList::RTPCollisionList(/*RTPMemoryManager *mgr*/) /*: RTPMemoryObject(mgr) */ {

	//timeinit.Dummy();
	}

void RTPCollisionList::Timeout(const RTPTime &currenttime,const RTPTime &timeoutdelay) {
	POSITION it;
	RTPTime checktime = currenttime;
	checktime -= timeoutdelay;
	
	it = addresslist.GetHeadPosition();
	while(it != addresslist.GetTailPosition())	{
		if(addresslist.GetAt(it).recvtime < checktime) // timeout
		{
			delete addresslist.GetAt(it).addr;
			addresslist.RemoveAt(it);	
			}
		else
			addresslist.GetNext(it);
			}
		}

int RTPCollisionList::UpdateAddress(const RTPAddress *addr,const RTPTime &receivetime,bool *created) {

	if(!addr)
		return ERR_RTP_COLLISIONLIST_BADADDRESS;
	
	POSITION it;
	
	for(POSITION pos = addresslist.GetHeadPosition(); pos;) {
		if((addresslist.GetAt(pos).addr)->IsSameAddress(addr)) {
			addresslist.GetAt(pos).recvtime = receivetime;
			*created = false;
			return 0;
		}
	addresslist.GetNext(pos);
	}

	RTPAddress *newaddr = addr->CreateCopy();
	if(!newaddr)
		return ERR_RTP_OUTOFMEM;
	
	addresslist.AddTail(AddressAndTime(newaddr,receivetime));
	*created = true;
	return 0;
	}


RTPPacket *RTPSession::GetNextPacket() {

	if(!created)
		return 0;
	return sources.GetNextPacket();
	}

RTPExternalTransmitter::RTPExternalTransmitter() : RTPTransmitter(), packetinjector((RTPExternalTransmitter *)this) {

	created = false;
	init = false;
	}

RTPExternalTransmitter::~RTPExternalTransmitter() {
	Destroy();
	}

int RTPExternalTransmitter::Init(bool tsafe) {

	if(init)
		return ERR_RTP_EXTERNALTRANS_ALREADYINIT;
	
#ifdef RTP_SUPPORT_THREAD
	threadsafe = tsafe;
	if(threadsafe) {
		int status;
		
		status = mainmutex.Init();
		if(status < 0)
			return ERR_RTP_EXTERNALTRANS_CANTINITMUTEX;
		status = waitmutex.Init();
		if(status < 0)
			return ERR_RTP_EXTERNALTRANS_CANTINITMUTEX;
		}
#else
	if(tsafe)
		return ERR_RTP_NOTHREADSUPPORT;
#endif // RTP_SUPPORT_THREAD

	init = true;
	return 0;
	}

int RTPExternalTransmitter::Create(size_t maximumpacketsize,const RTPTransmissionParams *transparams) {
	const RTPExternalTransmissionParams *params;
	int status;

	if(!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if(created)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_ALREADYCREATED;
		}
	
	// Obtain transmission parameters
	if(!transparams) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_ILLEGALPARAMETERS;
		}
	if(transparams->GetTransmissionProtocol() != RTPTransmitter::ExternalProto)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_ILLEGALPARAMETERS;
		}
		
	params = (const RTPExternalTransmissionParams *)transparams;

	if((status = m_abortDesc.Init()) < 0)	{
		MAINMUTEX_UNLOCK
		return status;
		}
	m_abortCount = 0;
	
	maxpacksize = maximumpacketsize;
	sender = params->GetSender();
	headersize = params->GetAdditionalHeaderSize();

	localhostname = 0;
	localhostnamelength = 0;

	waitingfordata = false;
	created = true;
	MAINMUTEX_UNLOCK
	return 0;
	}

void RTPExternalTransmitter::Destroy() {

	if(!init)
		return;

	MAINMUTEX_LOCK
	if(!created) {
		MAINMUTEX_UNLOCK;
		return;
		}

	if(localhostname) {
		delete localhostname;
		localhostname = 0;
		localhostnamelength = 0;
		}
	
	FlushPackets();
	created = false;
	
	if(waitingfordata) {
		m_abortDesc.SendAbortSignal();
		m_abortCount++;
		m_abortDesc.Destroy();
		MAINMUTEX_UNLOCK
		WAITMUTEX_LOCK // to make sure that the WaitForIncomingData function ended
		WAITMUTEX_UNLOCK
		}
	else
		m_abortDesc.Destroy();

	MAINMUTEX_UNLOCK
	}

int RTPExternalTransmitter::GetLocalHostName(BYTE *buffer,size_t *bufferlength) {

	if(!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;

	MAINMUTEX_LOCK
	if(!created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
		}

	if(localhostname == 0) {
		// We'll just use 'gethostname' for simplicity
		char name[1024];

		if(gethostname(name,1023) != 0)
			strcpy(name, "localhost"); // failsafe
		else
			name[1023] = 0; // ensure null-termination

		localhostnamelength = strlen(name);
		localhostname = new BYTE [localhostnamelength+1];

		memcpy(localhostname, name, localhostnamelength);
		localhostname[localhostnamelength] = 0;
		}
	
	if((*bufferlength) < localhostnamelength)	{
		*bufferlength = localhostnamelength; // tell the application the required size of the buffer
		MAINMUTEX_UNLOCK
		return ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL;
		}

	memcpy(buffer,localhostname,localhostnamelength);
	*bufferlength = localhostnamelength;
	
	MAINMUTEX_UNLOCK
	return 0;
	}

// Here the private functions start...
void RTPExternalTransmitter::FlushPackets() {
	
	for(POSITION pos = rawpacketlist.GetHeadPosition(); pos;)
		delete rawpacketlist.GetNext(pos);
	rawpacketlist.RemoveAll();
	}

int RTPExternalTransmitter::Poll() {
	return 0;
	}

int RTPExternalTransmitter::SendRTPData(const void *data,size_t len)	{
	return 0;
	}

int RTPExternalTransmitter::SendRTCPData(const void *data,size_t len) {
	return 0;
	}

bool RTPExternalTransmitter::ComesFromThisTransmitter(const RTPAddress *addr) {
	return 0;
	}

RTPRawPacket *RTPExternalTransmitter::GetNextPacket() {
	return 0;
	}

int RTPExternalTransmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m) {
	
	if(!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if(!created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
		}
	if(m != RTPTransmitter::AcceptAll) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_BADRECEIVEMODE;
		}
	MAINMUTEX_UNLOCK
	return 0;
	}

int RTPExternalTransmitter::SetMaximumPacketSize(size_t s) {
	
	if(!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if(!created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
		}
	maxpacksize = s;
	MAINMUTEX_UNLOCK
	return 0;
	}

int RTPExternalTransmitter::AddDestination(const RTPAddress &) {
	return ERR_RTP_EXTERNALTRANS_NODESTINATIONSSUPPORTED;
	}
int RTPExternalTransmitter::DeleteDestination(const RTPAddress &) {
	return ERR_RTP_EXTERNALTRANS_NODESTINATIONSSUPPORTED;
	}

void RTPExternalTransmitter::ClearDestinations() {
	}


RTPAbortDescriptors::RTPAbortDescriptors() {

	m_descriptors[0] = NULL;
	m_descriptors[1] = NULL;
	m_init = false;
	}

RTPAbortDescriptors::~RTPAbortDescriptors() {
	Destroy();
	}

int RTPAbortDescriptors::Init() {
	CString S;
	UINT port;

	if(m_init)
		return ERR_RTP_ABORTDESC_ALREADYINIT;

	m_descriptors[0]=new CSocket;
	m_descriptors[1]=new CSocket;

	CSocket listensock;
	int size;
	struct sockaddr_in addr;

	if(!listensock.Create(0,SOCK_STREAM,0))
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	
/*	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if(bind(listensock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)	{
		listensock.Close();
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
		}*/

/*	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);*/
	if(!listensock.GetSockName(S,port)) {
		listensock.Close();
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
		}

	unsigned short connectport = port;

	if(!m_descriptors[0]->Create(0,SOCK_STREAM,0)) {
		listensock.Close();
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
		}

/*	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if(!m_descriptors[0]->Bind((struct sockaddr *)&addr,sizeof(struct sockaddr_in)))	{
		listensock.Close();
		m_descriptors[0]->Close();
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
		}*/

	if(!listensock.Listen())	{
		listensock.Close();
		m_descriptors[0]->Close();
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
		}

/*	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(connectport);*/
	if(!m_descriptors[0]->Connect("127.0.0.1",connectport)) {
		listensock.Close();
		m_descriptors[0]->Close();
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
		}

/*	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);*/
//	if(!m_descriptors[1]->Accept(listensock)) {
	if(!listensock.Accept(*m_descriptors[1])) {
		listensock.Close();
		m_descriptors[0]->Close();		// GC dovrebbe essere ??
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
		}

	// okay, got the connection, close the listening socket
	listensock.Close();

	m_init = true;
	return 0;
	}

void RTPAbortDescriptors::Destroy() {

	if(!m_init)
		return;

	m_descriptors[0]->Close();
	m_descriptors[1]->Close();
	delete m_descriptors[0];
	delete m_descriptors[1];
	m_descriptors[0] = NULL;
	m_descriptors[1] = NULL;

	m_init = false;
	}

int RTPAbortDescriptors::SendAbortSignal() {
	if(!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	m_descriptors[1]->Send("*",1,0);
	return 0;
	}

RTCPSchedulerParams::RTCPSchedulerParams() : mininterval(RTCP_DEFAULTMININTERVAL) {

	bandwidth = 1000; // TODO What is a good value here? 
	senderfraction = RTCP_DEFAULTSENDERFRACTION;
	usehalfatstartup = RTCP_DEFAULTHALFATSTARTUP;
	immediatebye = RTCP_DEFAULTIMMEDIATEBYE;
// boh?	timeinit.Dummy();
	}
RTCPSchedulerParams::~RTCPSchedulerParams() {
	}

int RTCPSchedulerParams::SetMinimumTransmissionInterval(const RTPTime &t) {
	double t2 = t.GetDouble();

	if(t2 < RTCPSCHED_MININTERVAL)
		return ERR_RTP_SCHEDPARAMS_BADMINIMUMINTERVAL;

	mininterval = t;
	return 0;
	}

int RTCPSchedulerParams::SetSenderBandwidthFraction(double fraction) {

	if(fraction < 0.0 || fraction > 1.0)
		return ERR_RTP_SCHEDPARAMS_BADFRACTION;
	senderfraction = fraction;
	return 0;
	}

int RTCPSchedulerParams::SetRTCPBandwidth(double bw) {

	if(bw < 0.0)
		return ERR_RTP_SCHEDPARAMS_INVALIDBANDWIDTH;
	bandwidth = bw;
	return 0;
	}

RTPUDPv4Transmitter::RTPUDPv4Transmitter() : RTPTransmitter(),destinations(),
#ifdef RTP_SUPPORT_IPV4MULTICAST
								  multicastgroups(),
#endif // RTP_SUPPORT_IPV4MULTICAST
								  acceptignoreinfo() {
	rtpsock=NULL;
	rtcpsock=NULL;
	created = false;
	init = false;
	}

RTPUDPv4Transmitter::~RTPUDPv4Transmitter() {

	Destroy();
	}
int RTPUDPv4Transmitter::Init(bool tsafe) {
	
	if(init)
		return ERR_RTP_UDPV4TRANS_ALREADYINIT;
	
#ifdef RTP_SUPPORT_THREAD
	threadsafe = tsafe;
	if(threadsafe) {
		int status;
		
		status = mainmutex.Init();
		if(status < 0)
			return ERR_RTP_UDPV4TRANS_CANTINITMUTEX;
		status = waitmutex.Init();
		if(status < 0)
			return ERR_RTP_UDPV4TRANS_CANTINITMUTEX;
		}
#else
	if(tsafe)
		return ERR_RTP_NOTHREADSUPPORT;
#endif // RTP_SUPPORT_THREAD

	init = true;
	return 0;
	}

#define CLOSESOCKETS do { \
	if(closesocketswhendone) {\
		if(rtpsock != rtcpsock) \
			rtcpsock->Close(); \
		rtpsock->Close(); \
		} \
	} while(0)

static int GetIPv4SocketPort(CAsyncSocket *s, WORD *pPort) {
	ASSERT(pPort != 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	RTPSOCKLENTYPE size = sizeof(struct sockaddr_in);
	if(!s->GetSockName((SOCKADDR*)&addr,&size))
		return ERR_RTP_UDPV4TRANS_CANTGETSOCKETPORT;

	if(addr.sin_family != AF_INET)
		return ERR_RTP_UDPV4TRANS_NOTANIPV4SOCKET;

	WORD port = ntohs(addr.sin_port);
	if(port == 0)
		return ERR_RTP_UDPV4TRANS_SOCKETPORTNOTSET;
	
	int type = 0;
	RTPSOCKLENTYPE length = sizeof(type);

	if(!s->SetSockOpt(SO_TYPE, (char*)&type, length))
		return ERR_RTP_UDPV4TRANS_CANTGETSOCKETTYPE;

	if(type != SOCK_DGRAM)
		return ERR_RTP_UDPV4TRANS_INVALIDSOCKETTYPE;

	*pPort = port;
	return 0;
	}

int GetAutoSockets(DWORD bindIP, bool allowOdd, bool rtcpMux,
                   CAsyncSocket *pRtpSock, CAsyncSocket *pRtcpSock, 
                   WORD *pRtpPort, WORD *pRtcpPort) {
	const int maxAttempts = 1024;
	int attempts = 0;
	vector<CSocket *> toClose;

	while(attempts++ < maxAttempts) {
		CSocket *sock=new CSocket;
		if(!sock->Create(0, SOCK_DGRAM, 0)) {
			for(size_t i=0 ; i < toClose.size() ; i++)
				toClose[i]->Close();
			return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
			}

		// First we get an automatically chosen port
		if(!sock->Bind(0,bindIP))	{
			sock->Close();
			for(size_t i=0 ; i < toClose.size() ; i++)
				toClose[i]->Close();
			return ERR_RTP_UDPV4TRANS_CANTGETVALIDSOCKET;
			}

		WORD basePort = 0;
		int status = GetIPv4SocketPort(sock, &basePort);
		if(status < 0) {
			sock->Close();
			for(size_t i=0 ; i < toClose.size() ; i++)
				toClose[i]->Close();
			return status;
			}

		if(rtcpMux) // only need one socket
		{
			if(basePort % 2 == 0 || allowOdd)	{
				pRtpSock = sock;
				pRtcpSock = sock;
				*pRtpPort = basePort;
				*pRtcpPort = basePort;
				for(size_t i=0 ; i < toClose.size() ; i++)
					toClose[i]->Close();

				return 0;
				}
			else
				toClose.push_back(sock);
			}
		else {
			CSocket *sock2=new CSocket;
			if(!sock2->Create(0, SOCK_DGRAM, 0)) {
				sock->Close();
				for(size_t i=0 ; i < toClose.size() ; i++)
					toClose[i]->Close();
				return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
				}

			// Try the next port or the previous port
			WORD secondPort = basePort;
			bool possiblyValid = false;

			if(basePort % 2 == 0) {
				secondPort++;
				possiblyValid = true;
				}
			else if(basePort > 1) // avoid landing on port 0
			{
				secondPort--;
				possiblyValid = true;
				}

			if(possiblyValid) {
				CStringEx S;
				S.FormatIP(bindIP);
				if(!sock2->Bind(secondPort,S))	{
					// In this case, we have two consecutive port numbers, the lower of which is even
					if(basePort < secondPort)	{
						pRtpSock = sock;
						pRtcpSock = sock2;
						*pRtpPort = basePort;
						*pRtcpPort = secondPort;
						}
					else {
						pRtpSock = sock2;
						pRtcpSock = sock;
						*pRtpPort = secondPort;
						*pRtcpPort = basePort;
						}

					for(size_t i=0 ; i < toClose.size(); i++)
						toClose[i]->Close();

					return 0;
					}
				}

			toClose.push_back(sock);
			toClose.push_back(sock2);
			}
		}

	for(size_t i=0 ; i < toClose.size(); i++)
		toClose[i]->Close();

	return ERR_RTP_UDPV4TRANS_TOOMANYATTEMPTSCHOOSINGSOCKET;
	}

int RTPUDPv4Transmitter::Create(size_t maximumpacketsize,const RTPTransmissionParams *transparams) {
	const RTPUDPv4TransmissionParams *params,defaultparams;
	RTPSOCKLENTYPE size;
	int status;

	if(!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if(created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_ALREADYCREATED;
		}
	
	// Obtain transmission parameters
	
	if(!transparams)
		params = &defaultparams;
	else {
		if(transparams->GetTransmissionProtocol() != RTPTransmitter::IPv4UDPProto) {
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_ILLEGALPARAMETERS;
			}
		params = (const RTPUDPv4TransmissionParams *)transparams;
		}

	if(params->GetUseExistingSockets(rtpsock, rtcpsock))	{
		closesocketswhendone = false;

		// Determine the port numbers
		int status = GetIPv4SocketPort(rtpsock, &m_rtpPort);
		if(status < 0) {
			MAINMUTEX_UNLOCK
			return status;
			}
		status = GetIPv4SocketPort(rtcpsock, &m_rtcpPort);
		if(status < 0)	{
			MAINMUTEX_UNLOCK
			return status;
			}
		}
	else {
		closesocketswhendone = true;

		if(params->GetPortbase() == 0)	{
			int status = GetAutoSockets(params->GetBindIP(), params->GetAllowOddPortbase(), params->GetRTCPMultiplexing(),
			                            rtpsock, rtcpsock, &m_rtpPort, &m_rtcpPort);
			if(status < 0) {
				MAINMUTEX_UNLOCK
				return status;
				}
			}
		else {
			// Check if portbase is even (if necessary)
			if(!params->GetAllowOddPortbase() && params->GetPortbase() % 2 != 0)	{
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_PORTBASENOTEVEN;
				}

			// create sockets
			rtpsock=new CDataSocket();
			{
				bool b=TRUE;
//				rtpsock->SetSockOpt(SO_REUSEADDR,&b,1);
			}
			if(!rtpsock->Create(params->GetPortbase())) {
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
				}

			// If we're multiplexing, we're just going to set the RTCP socket to equal the RTP socket
			if(params->GetRTCPMultiplexing())
				rtcpsock = rtpsock;
/*			else {
				bool b=TRUE;
				rtcpsock=new CSocket();
//				rtcpsock->SetSockOpt(SO_REUSEADDR,&b,1);
				if(!rtcpsock->Create(0,SOCK_DGRAM,0)) {
//					int i=WSAGetLastError();
					rtpsock->Close();
					MAINMUTEX_UNLOCK
					return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
					}
				}*/

			// bind sockets
			DWORD bindIP = params->GetBindIP();
			
			m_rtpPort = params->GetPortbase();

			/*if(!rtpsock->Bind(params->GetPortbase())) {
				int i=WSAGetLastError();
				CLOSESOCKETS;
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_CANTBINDRTPSOCKET;
				}*/

			if(rtpsock != rtcpsock) {	// no need to bind same socket twice when multiplexing
				WORD rtpport = params->GetPortbase();
				WORD rtcpport = params->GetForcedRTCPPort();

				if(rtcpport == 0)	{
					rtcpport = rtpport;
					if(rtcpport < 0xFFFF)
						rtcpport++;
					}

				rtcpsock=new CDataSocket();
//				rtcpsock->SetSockOpt(SO_REUSEADDR,&b,1);
				if(!rtcpsock->Create(rtcpport)) {
//					int i=WSAGetLastError();
					rtcpsock->Close();
					MAINMUTEX_UNLOCK
					return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
					}
/*				if(!rtpsock->Bind(rtcpport))	{
					CLOSESOCKETS;
					MAINMUTEX_UNLOCK
					return ERR_RTP_UDPV4TRANS_CANTBINDRTCPSOCKET;
					}*/

				m_rtcpPort = rtcpport;
				}
			else
				m_rtcpPort = m_rtpPort;
			}

		// set socket buffer sizes
		
		size = params->GetRTPReceiveBuffer();
		if(!rtpsock->SetSockOpt(SO_RCVBUF,(const char *)&size,sizeof(int)))	{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_CANTSETRTPRECEIVEBUF;
			}
		size = params->GetRTPSendBuffer();
		if(!rtpsock->SetSockOpt(SO_SNDBUF,(const char *)&size,sizeof(int)))	{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_CANTSETRTPTRANSMITBUF;
			}

		if(rtpsock != rtcpsock) {	// no need to set RTCP flags when multiplexing
			size = params->GetRTCPReceiveBuffer();
			if(!rtcpsock->SetSockOpt(SO_RCVBUF,(const char *)&size,sizeof(int))) {
				CLOSESOCKETS;
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_CANTSETRTCPRECEIVEBUF;
				}
			size = params->GetRTCPSendBuffer();
			if(!rtcpsock->SetSockOpt(SO_SNDBUF,(const char *)&size,sizeof(int))) {
				CLOSESOCKETS;
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_CANTSETRTCPTRANSMITBUF;
				}
			}
		}

	// Try to obtain local IP addresses
	for(POSITION pos = params->GetLocalIPList().GetHeadPosition(); pos; ) { 
		const auto& item = params->GetLocalIPList().GetNext(pos); 
		localIPs.AddTail(item); 
		} 
	if(localIPs.IsEmpty()) {	// User did not provide list of local IP addresses, calculate them
		int status;
		
		if((status = CreateLocalIPList()) < 0) {
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return status;
			}
#ifdef RTPDEBUG
		std::cout << "Found these local IP addresses:" << std::endl;
		
		std::list<DWORD>::const_iterator it;

		for(it = localIPs.begin() ; it != localIPs.end() ; it++) {
			RTPIPv4Address a(*it);

			std::cout << a.GetAddressString() << std::endl;
			}
#endif // RTPDEBUG
		}

#ifdef RTP_SUPPORT_IPV4MULTICAST
	if(SetMulticastTTL(params->GetMulticastTTL()))
		supportsmulticasting = true;
	else
		supportsmulticasting = false;
#else // no multicast support enabled
	supportsmulticasting = false;
#endif // RTP_SUPPORT_IPV4MULTICAST

	if(maximumpacketsize > RTPUDPV4TRANS_MAXPACKSIZE) {
		CLOSESOCKETS;
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
		}
	
	if(!params->GetCreatedAbortDescriptors())	{
		if((status = m_abortDesc.Init()) < 0) {
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return status;
			}
		m_pAbortDesc = &m_abortDesc;
		}
	else {
		m_pAbortDesc = params->GetCreatedAbortDescriptors();
		if(!m_pAbortDesc->IsInitialized()) {
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return ERR_RTP_ABORTDESC_NOTINIT;
			}
		}

	maxpacksize = maximumpacketsize;
	multicastTTL = params->GetMulticastTTL();
	mcastifaceIP = params->GetMulticastInterfaceIP();
	receivemode = RTPTransmitter::AcceptAll;

	localhostname = 0;
	localhostnamelength = 0;

	waitingfordata = false;
	created = true;
	MAINMUTEX_UNLOCK 
	return 0;
	}

void RTPUDPv4Transmitter::Destroy() {
	
	if(!init)
		return;

	MAINMUTEX_LOCK
	if(!created) {
		MAINMUTEX_UNLOCK;
		return;
		}

	if(localhostname)	{
		delete localhostname;
		localhostname = 0;
		localhostnamelength = 0;
		}
	
	CLOSESOCKETS;
	destinations.Clear();
#ifdef RTP_SUPPORT_IPV4MULTICAST
	multicastgroups.Clear();
#endif // RTP_SUPPORT_IPV4MULTICAST
	FlushPackets();
	ClearAcceptIgnoreInfo();
	localIPs.RemoveAll();
	created = false;
	
	if(waitingfordata)	{
		m_pAbortDesc->SendAbortSignal();
		m_abortDesc.Destroy(); // Doesn't do anything if not initialized
		MAINMUTEX_UNLOCK
		WAITMUTEX_LOCK // to make sure that the WaitForIncomingData function ended
		WAITMUTEX_UNLOCK
		}
	else
		m_abortDesc.Destroy(); // Doesn't do anything if not initialized

	MAINMUTEX_UNLOCK
	}

void RTPUDPv4Transmitter::FlushPackets() {

	for(POSITION pos = rawpacketlist.GetHeadPosition(); pos; )
		delete rawpacketlist.GetNext(pos);
	rawpacketlist.RemoveAll();
	}

void RTPUDPv4Transmitter::ClearAcceptIgnoreInfo() {

	acceptignoreinfo.GotoFirstElement();
	while(acceptignoreinfo.HasCurrentElement())	{
		PortInfo *inf;

		inf = acceptignoreinfo.GetCurrentElement();
		delete inf;
		acceptignoreinfo.GotoNextElement();
		}
	acceptignoreinfo.Clear();
	}

int RTPUDPv4Transmitter::CreateLocalIPList() {
	 // first try to obtain the list from the network interface info

	if(!GetLocalIPList_Interfaces()) {
		// If this fails, we'll have to depend on DNS info
		GetLocalIPList_DNS();
		}
	AddLoopbackAddress();
	return 0;
	}

int RTPUDPv4Transmitter::GetLocalHostName(BYTE *buffer,size_t *bufferlength) {
	
	if(!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	if(!created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
		}

	if(localhostname == 0) {
		if(localIPs.IsEmpty()) {
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_NOLOCALIPS;
			}
		
		CList<CString,CString> hostnames;
	
		for(POSITION pos=localIPs.GetHeadPosition(); pos; ) {
			bool founddouble = false;
			bool foundentry = true;

			while(!founddouble && foundentry) {
				struct hostent *he;
				BYTE addr[4];
				DWORD ip = localIPs.GetNext(pos);
		
				addr[0] = (BYTE)((ip>>24)&0xFF);
				addr[1] = (BYTE)((ip>>16)&0xFF);
				addr[2] = (BYTE)((ip>>8)&0xFF);
				addr[3] = (BYTE)(ip&0xFF);
				he = gethostbyaddr((char *)addr,4,AF_INET);
				if(he != 0)	{
					CString hname(he->h_name);

					for(POSITION pos2 = hostnames.GetHeadPosition(); !founddouble && pos; )
						if(hostnames.GetNext(pos2) == hname)
							founddouble = true;

					if(!founddouble)
						hostnames.AddTail(hname);
					
					int i=0;
					while(!founddouble && he->h_aliases[i] != 0)	{
						CString hname(he->h_aliases[i]);
					
						for(POSITION pos2 = hostnames.GetHeadPosition(); !founddouble && pos; )
							if(hostnames.GetNext(pos2) == hname)
								founddouble = true;

						if(!founddouble) {
							hostnames.AddTail(hname);
							i++;
							}
						}
					}
				else
					foundentry = false;
				}
			}
	
		bool found  = false;
		
		if(!hostnames.IsEmpty()) {	// try to select the most appropriate hostname
		
//			hostnames.Sort();
#pragma warning {FARE SORT!
//https://groups.google.com/g/microsoft.public.vc.mfc/c/gE7B8tgitwc
//http://msgroups.net/vc.mfc/sorting-a-clist/570968

			for(POSITION pos = hostnames.GetHeadPosition(); !found && pos; ) {
				if(hostnames.GetNext(pos).Find('.') != std::string::npos)	{
					found = true;
					localhostnamelength = hostnames.GetAt(pos).GetLength();
					localhostname = new BYTE[localhostnamelength+1];
					if(!localhostname)	{
						MAINMUTEX_UNLOCK
						return ERR_RTP_OUTOFMEM;
						}
					memcpy(localhostname,hostnames.GetAt(pos).GetBuffer(0),localhostnamelength);
					hostnames.GetAt(pos).ReleaseBuffer();
					localhostname[localhostnamelength] = 0;
					}
				}
			}
	
		if(!found) {		// use an IP address
			DWORD ip;
			int len;
			char str[16];
			
			ip = localIPs.GetHead();
			
			wsprintf(str,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
			len = _tcslen(str);
	
			localhostnamelength = len;
			localhostname = new BYTE [localhostnamelength + 1];
			if(!localhostname) {
				MAINMUTEX_UNLOCK
				return ERR_RTP_OUTOFMEM;
				}
			memcpy(localhostname,str,localhostnamelength);
			localhostname[localhostnamelength] = 0;
			}
		}	
	
	if((*bufferlength) < localhostnamelength)	{
		*bufferlength = localhostnamelength; // tell the application the required size of the buffer
		MAINMUTEX_UNLOCK
		return ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL;
		}

	memcpy(buffer,localhostname,localhostnamelength);
	*bufferlength = localhostnamelength;
	
	MAINMUTEX_UNLOCK
	return 0;
	}

bool RTPUDPv4Transmitter::GetLocalIPList_Interfaces() {
	unsigned char buffer[RTPUDPV4TRANS_IFREQBUFSIZE];
	DWORD outputsize;
	DWORD numaddresses,i;
	SOCKET_ADDRESS_LIST *addrlist;

	if(WSAIoctl(rtpsock->m_hSocket,SIO_ADDRESS_LIST_QUERY,NULL,0,&buffer,RTPUDPV4TRANS_IFREQBUFSIZE,&outputsize,NULL,NULL))
		return false;
	
	addrlist = (SOCKET_ADDRESS_LIST *)buffer;
	numaddresses = addrlist->iAddressCount;
	for(i=0; i < numaddresses; i++)	{
		SOCKET_ADDRESS *sockaddr = &(addrlist->Address[i]);
		if(sockaddr->iSockaddrLength == sizeof(struct sockaddr_in)) {	// IPv4 address
			struct sockaddr_in *addr = (struct sockaddr_in *)sockaddr->lpSockaddr;

			localIPs.AddHead(ntohl(addr->sin_addr.s_addr));
			}
		}

	if(localIPs.IsEmpty())
		return false;

	return true;
	}

int RTPUDPv4Transmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m) {

	if(!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if(!created)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
		}
	if(m != receivemode) {
		receivemode = m;
		acceptignoreinfo.Clear();
		}
	MAINMUTEX_UNLOCK
	return 0;
	}

void RTPUDPv4Transmitter::GetLocalIPList_DNS() {
	struct hostent *he;
	char name[1024];
	bool done;
	int i,j;

	gethostname(name,1023);
	name[1023] = 0;
	he = gethostbyname(name);
	if(!he)
		return;
	
	i=0;
	done = false;
	while(!done)	{
		if(he->h_addr_list[i] == NULL)
			done = true;
		else {
			DWORD ip = 0;

			for(j = 0 ; j < 4 ; j++)
				ip |= ((DWORD)((unsigned char)he->h_addr_list[i][j])<<((3-j)*8));
			localIPs.AddTail(ip);
			i++;
			}
		}
	}

void RTPUDPv4Transmitter::AddLoopbackAddress() {
	DWORD loopbackaddr = (((DWORD)127)<<24)|((DWORD)1);
	bool found = false;
	
	for(POSITION pos = localIPs.GetHeadPosition(); !found && pos;) { 

		if(localIPs.GetNext(pos) == loopbackaddr)
			found = true;
		}

	if(!found)
		localIPs.AddTail(loopbackaddr);
	}

int RTPUDPv4Transmitter::SetMaximumPacketSize(size_t s)	{

	if(!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if(!created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
		}
	if(s > RTPUDPV4TRANS_MAXPACKSIZE)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
		}
	maxpacksize = s;
	MAINMUTEX_UNLOCK
	return 0;
	}

bool RTPUDPv4Transmitter::ComesFromThisTransmitter(const RTPAddress *addr) {

	if(!init)
		return false;

	if(!addr)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if(created && addr->GetAddressType() == RTPAddress::IPv4Address)	{	
		const RTPIPv4Address *addr2 = (const RTPIPv4Address *)addr;
		bool found = false;
		POSITION it=localIPs.GetHeadPosition();
	
		while(!found && it != localIPs.GetTailPosition())	{
			if(addr2->GetIP() == localIPs.GetAt(it))
				found = true;
			else
				localIPs.GetNext(it);
			}
	
		if(!found)
			v = false;
		else {
			if(addr2->GetPort() == m_rtpPort || addr2->GetPort() == m_rtcpPort) // check for RTP port and RTCP port
				v = true;
			else 
				v = false;
			}
		}
	else
		v = false;

	MAINMUTEX_UNLOCK
	return v;
	}

int RTPUDPv4Transmitter::Poll() {

	if(!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	int status;
	
	MAINMUTEX_LOCK
	if(!created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
		}
	status = PollSocket(true); // poll RTP socket
	if(rtpsock != rtcpsock) {		// no need to poll twice when multiplexing
		if(status >= 0)
			status = PollSocket(false); // poll RTCP socket
		}
	MAINMUTEX_UNLOCK
	return status;
	}

int RTPUDPv4Transmitter::PollSocket(bool rtp) {
	RTPSOCKLENTYPE fromlen;
	int recvlen;
	char packetbuffer[RTPUDPV4TRANS_MAXPACKSIZE];
	CAsyncSocket *sock;
	unsigned long len;
	struct sockaddr_in srcaddr;
	bool dataavailable;
	
	if(rtp)
		sock = rtpsock;
	else
		sock = rtcpsock;
	
	do {
		len = 0;
		int i=sock->IOCtl(FIONREAD,&len);

		if(len <= 0) // make sure a packet of length zero is not queued
		{
			// An alternative workaround would be to just use non-blocking sockets.
			// However, since the user does have access to the sockets and I do not
			// know how this would affect anyone else's code, I chose to do it using
			// an extra select call in case ioctl says the length is zero.
			
			signed char isset = 0;
			BYTE myBuf[2];
//			int status = /*WSAPoll Win Vista...*/ poll(&sock, &isset, 1, RTPTime(0));
			
			int status;
//			while(RTPTime(0).GetDouble()) {
				CString S;
				UINT port;
				status=sock->ReceiveFrom(myBuf,1,S,port,MSG_PEEK);
//				}

			if(status < 0) {
				int i=WSAGetLastError();
				if(i != 10035)		// WSAEWOULDBLOCK
					return status;
				}

			if(isset)
				dataavailable = true;
			else
				dataavailable = false;
			}
		else
			dataavailable = true;

		if(dataavailable) {
			RTPTime curtime = RTPTime::CurrentTime();
			fromlen = sizeof(struct sockaddr_in);
			recvlen = sock->ReceiveFrom(packetbuffer,RTPUDPV4TRANS_MAXPACKSIZE,(SOCKADDR*)&srcaddr,&fromlen);
			int i=WSAGetLastError();
			if(recvlen > 0)	{
				bool acceptdata;

				// got data, process it
				if(receivemode == RTPTransmitter::AcceptAll)
					acceptdata = true;
				else
					acceptdata = ShouldAcceptData(srcaddr.sin_addr.S_un.S_addr,srcaddr.sin_port);
				
				if(acceptdata)	{
					RTPRawPacket *pack;
					RTPIPv4Address *addr;
					BYTE *datacopy;

					addr = new RTPIPv4Address(srcaddr.sin_addr.S_un.S_addr,srcaddr.sin_port);
					if(!addr)
						return ERR_RTP_OUTOFMEM;
					datacopy = new BYTE[recvlen];
					if(!datacopy)	{
						delete addr;
						return ERR_RTP_OUTOFMEM;
						}
					memcpy(datacopy,packetbuffer,recvlen);
					
					bool isrtp = rtp;
					if(rtpsock == rtcpsock) {	// check payload type when multiplexing
						isrtp = true;

						if((size_t)recvlen > sizeof(RTCPCommonHeader)) {
							RTCPCommonHeader *rtcpheader = (RTCPCommonHeader *)datacopy;
							BYTE packettype = rtcpheader->packettype;

    					if(packettype >= 200 && packettype <= 204)
								isrtp = false;
							}
						}
						
					pack = new RTPRawPacket(datacopy,recvlen,addr,curtime,isrtp);
					if(!pack)	{
						delete addr;
						delete datacopy;
						return ERR_RTP_OUTOFMEM;
						}
					rawpacketlist /*packetlist*/.AddTail(pack);	
					}
				}
			}
		} while(dataavailable);

	return 0;
	}

/*inline int RTPSelect(const SocketType *sockets, int8_t *readflags, size_t numsocks, RTPTime timeout) {
	using namespace std;

	vector<struct pollfd> fds(numsocks);

	for(size_t i=0; i<numsocks; i++) {
		fds[i].fd = sockets[i];
		fds[i].events = POLLIN;
		fds[i].revents = 0;
		readflags[i] = 0;
		}

	int timeoutmsec = -1;
	if(timeout.GetDouble() >= 0)	{
		double dtimeoutmsec = timeout.GetDouble()*1000.0;
		if(dtimeoutmsec > (numeric_limits<int>::max)()) // parentheses to prevent windows 'max' macro expansion
			dtimeoutmsec = (numeric_limits<int>::max)();
		
		timeoutmsec = (int)dtimeoutmsec;
		}

#ifdef RTP_HAVE_WSAPOLL
	int status = WSAPoll(&(fds[0]), (ULONG)numsocks, timeoutmsec);
	if(status < 0)
		return ERR_RTP_SELECT_ERRORINPOLL;
#else
	int status = poll(&(fds[0]), numsocks, timeoutmsec);
	if(status < 0)	{
		// We're just going to ignore an EINTR
		if(errno == EINTR)
			return 0;
		return ERR_RTP_SELECT_ERRORINPOLL;
		}
#endif // RTP_HAVE_WSAPOLL

	if(status > 0) {
		for(size_t i=0 ; i < numsocks ; i++) {
			if(fds[i].revents)
				readflags[i] = 1;
			}
		}
	return status;
	}*/

RTPRawPacket *RTPUDPv4Transmitter::GetNextPacket() {

	if(!init)
		return NULL;
	
	MAINMUTEX_LOCK
	
	RTPRawPacket *p;
	if(!created) {
		MAINMUTEX_UNLOCK
		return 0;
		}
	if(rawpacketlist.IsEmpty())	{
		MAINMUTEX_UNLOCK
		return 0;
		}

	p = rawpacketlist.GetHead();
	rawpacketlist.RemoveHead();

	MAINMUTEX_UNLOCK
	return p;
	}

bool RTPUDPv4Transmitter::ShouldAcceptData(DWORD srcip,WORD srcport) {

	if(receivemode == RTPTransmitter::AcceptSome)	{
		PortInfo *inf;

		acceptignoreinfo.GotoElement(srcip);
		if(!acceptignoreinfo.HasCurrentElement())
			return false;
		
		inf = acceptignoreinfo.GetCurrentElement();
		if(!inf->all) {	// only accept the ones in the list
			POSITION pos,begin,end;

			begin = inf->portlist.GetHeadPosition();
			end = inf->portlist.GetTailPosition();
			for(pos=begin; pos != end; ) {
				if(inf->portlist.GetNext(pos) == srcport)
					return true;
				}
			return false;
			}
		else {	// accept all, except the ones in the list
			POSITION pos,begin,end;

			begin = inf->portlist.GetHeadPosition();
			end = inf->portlist.GetTailPosition();
			for(pos=begin; pos != end; ) {
				if(inf->portlist.GetNext(pos) == srcport)
					return false;
				}
			return true;
			}
		}
	else { // IgnoreSome
		PortInfo *inf;

		acceptignoreinfo.GotoElement(srcip);
		if(!acceptignoreinfo.HasCurrentElement())
			return true;
		
		inf = acceptignoreinfo.GetCurrentElement();
		if(!inf->all) {	// ignore the ports in the list
			POSITION pos,begin,end;

			begin = inf->portlist.GetHeadPosition();
			end = inf->portlist.GetTailPosition();
			for(pos=begin; pos != end; ) {
				if(inf->portlist.GetNext(pos) == srcport)
					return false;
				}
			return true;
			}
		else {	// ignore all, except the ones in the list
			POSITION pos,begin,end;

			begin = inf->portlist.GetHeadPosition();
			end = inf->portlist.GetTailPosition();
			for(pos=begin; pos != end; ) {
				if(inf->portlist.GetNext(pos) == srcport)
					return true;
				}
			return false;
			}
		}
	return true;
	}

int RTPUDPv4Transmitter::SendRTPData(const void *data,size_t len)	{

	if(!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	if(!created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if(len > maxpacksize) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
	}
	
	destinations.GotoFirstElement();
	while(destinations.HasCurrentElement()) {
		rtpsock->SendTo((const char *)data,len,
			(const struct sockaddr *)destinations.GetCurrentElement().GetRTPSockAddr(),sizeof(struct sockaddr_in));
		destinations.GotoNextElement();
		}
	
	MAINMUTEX_UNLOCK
	return 0;
	}

int RTPUDPv4Transmitter::SendRTCPData(const void *data,size_t len) {

	if(!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	if(!created)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
		}
	if(len > maxpacksize)	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
		}
	
	destinations.GotoFirstElement();
	while(destinations.HasCurrentElement())	{
		rtcpsock->SendTo((const char *)data,len,
			(const struct sockaddr *)destinations.GetCurrentElement().GetRTCPSockAddr(),sizeof(struct sockaddr_in));
		destinations.GotoNextElement();
		}
	
	MAINMUTEX_UNLOCK
	return 0;
	}

int RTPUDPv4Transmitter::AddDestination(const RTPAddress &addr) {
	
	if(!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if(!created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
		}

	RTPIPv4Destination dest;
	if(!RTPIPv4Destination::AddressToDestination(addr, dest))	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
		}
	
	int status = destinations.AddElement(dest);

	MAINMUTEX_UNLOCK
	return status;
	}

int RTPUDPv4Transmitter::DeleteDestination(const RTPAddress &addr) {

	if(!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	if(!created) {
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
		}
	RTPIPv4Destination dest;
	if(!RTPIPv4Destination::AddressToDestination(addr, dest))	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
		}
	
	int status = destinations.DeleteElement(dest);
	
	MAINMUTEX_UNLOCK
	return status;
	}

void RTPUDPv4Transmitter::ClearDestinations() {

	if(!init)
		return;
	
	MAINMUTEX_LOCK
	if(created)
		destinations.Clear();
	MAINMUTEX_UNLOCK
	}


RTPInternalSourceData::RTPInternalSourceData(DWORD ssrc,RTPSources::ProbationType probtype):RTPSourceData(ssrc) {

#ifdef RTP_SUPPORT_PROBATION
	probationtype = probtype;
#endif // RTP_SUPPORT_PROBATION
	}

RTPInternalSourceData::~RTPInternalSourceData() {
	}


void RTCPSDESInfo::Clear() {
#ifdef RTP_SUPPORT_SDESPRIV

	for(POSITION pos = privitems.GetHeadPosition(); pos; )
		delete privitems.GetNext(pos);
	privitems.RemoveAll();
#endif // RTP_SUPPORT_SDESPRIV
	}

// The following function should delete rtppack if necessary
int RTPInternalSourceData::ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,bool *stored,RTPSources *sources) {
	bool accept,onprobation,applyprobation;
	double tsunit;
	
	*stored = false;
	
	if(timestampunit < 0) 
		tsunit = INF_GetEstimatedTimestampUnit();
	else
		tsunit = timestampunit;

#ifdef RTP_SUPPORT_PROBATION
	if(validated) 				// If the source is our own process, we can already be validated. No 
		applyprobation = false;		// probation should be applied in that case.
	else {
		if(probationtype == RTPSources::NoProbation)
			applyprobation = false;
		else
			applyprobation = true;
		}
#else
	applyprobation = false;
#endif // RTP_SUPPORT_PROBATION

	stats.ProcessPacket(rtppack,receivetime,tsunit,ownssrc,&accept,applyprobation,&onprobation);

#ifdef RTP_SUPPORT_PROBATION
	switch(probationtype) {
		case RTPSources::ProbationStore:
			if(!(onprobation || accept))
				return 0;
			if(accept)
				validated = true;
			break;
		case RTPSources::ProbationDiscard:
		case RTPSources::NoProbation:
			if(!accept)
				return 0;
			validated = true;
			break;
		default:
			return ERR_RTP_INTERNALSOURCEDATA_INVALIDPROBATIONTYPE;
		}
#else
	if(!accept)
		return 0;
	validated = true;
#endif // RTP_SUPPORT_PROBATION;
	
	if(validated && !ownssrc) // for own ssrc these variables depend on the outgoing packets, not on the incoming
		issender = true;
	
	bool isonprobation = !validated;
	bool ispackethandled = false;

	sources->OnValidatedRTPPacket(this, rtppack, isonprobation, &ispackethandled);
	if(ispackethandled) // Packet is already handled in the callback, no need to store it in the list
	{
		// Set 'stored' to true to avoid the packet being deallocated
		*stored = true;
		return 0;
		}

	// Now, we can place the packet in the queue
	if(packetlist.IsEmpty()) {
		*stored = true;
		packetlist.AddTail(rtppack);
		return 0;
		}
	
	if(!validated) // still on probation
	{
		// Make sure that we don't buffer too much packets to avoid wasting memory
		// on a bad source. Delete the packet in the queue with the lowest sequence
		// number.
		if(packetlist.GetCount() == RTPINTERNALSOURCEDATA_MAXPROBATIONPACKETS) {
			RTPPacket *p = packetlist.GetHead();
			packetlist.RemoveHead();
			delete p;
			}
		}

	// find the right position to insert the packet
	POSITION it,start;
	bool done = false;
	DWORD newseqnr = rtppack->GetExtendedSequenceNumber();
	
	it = packetlist.GetTailPosition();
//	packetlist.GetPrev(it);
	start = packetlist.GetHeadPosition();

	while(!done)	{
		RTPPacket *p;
		DWORD seqnr;
		
		p = packetlist.GetAt(it);
		seqnr = p->GetExtendedSequenceNumber();
		if(seqnr > newseqnr)	{
			if(it != start)
				packetlist.GetPrev(it);
			else {		// we're at the start of the list
				*stored = true;
				done = true;
				packetlist.AddHead(rtppack);
				}
			}
		else if(seqnr < newseqnr) {		// insert after this packet
			packetlist.InsertAfter(it,rtppack);
			done = true;
			*stored = true;
			}
		else {	// they're equal !! Drop packet
			done = true;
			}
		}

	return 0;
	}

int RTPInternalSourceData::ProcessSDESItem(BYTE sdesid,const BYTE *data,size_t itemlen,const RTPTime &receivetime,bool *cnamecollis) {
	*cnamecollis = false;
	
	stats.SetLastMessageTime(receivetime);
	
	switch(sdesid) {
		case RTCP_SDES_ID_CNAME:
			{
				size_t curlen;
				BYTE *oldcname;
				
				// NOTE: we're going to make sure that the CNAME is only set once.
				oldcname = SDESinf.GetCNAME(&curlen);
				if(curlen == 0)	{
					// if CNAME is set, the source is validated
					SDESinf.SetCNAME(data,itemlen);
					validated = true;
				}
				else // check if this CNAME is equal to the one that is already present
				{
					if(curlen != itemlen)
						*cnamecollis = true;
					else {
						if(memcmp(data,oldcname,itemlen) != 0)
							*cnamecollis = true;
						}
					}
			}
			break;
		case RTCP_SDES_ID_NAME:
			{
				size_t oldlen;

            			SDESinf.GetName(&oldlen);
				if(oldlen == 0) // Name not set
					return SDESinf.SetName(data,itemlen);
			}
			break;
		case RTCP_SDES_ID_EMAIL:
			{
				size_t oldlen;

				SDESinf.GetEMail(&oldlen);
				if(oldlen == 0)
					return SDESinf.SetEMail(data,itemlen);
			}
			break;
		case RTCP_SDES_ID_PHONE:
			return SDESinf.SetPhone(data,itemlen);
		case RTCP_SDES_ID_LOCATION:
			return SDESinf.SetLocation(data,itemlen);
		case RTCP_SDES_ID_TOOL:
			{
				size_t oldlen;

				SDESinf.GetTool(&oldlen);
				if(oldlen == 0)
					return SDESinf.SetTool(data,itemlen);
			}
			break;
		case RTCP_SDES_ID_NOTE:
			stats.SetLastNoteTime(receivetime);
			return SDESinf.SetNote(data,itemlen);
		}
	return 0;
	}

#ifdef RTP_SUPPORT_SDESPRIV
int RTPInternalSourceData::ProcessPrivateSDESItem(const BYTE *prefix,size_t prefixlen,const BYTE *value,size_t valuelen,const RTPTime &receivetime) {
	int status;
	
	stats.SetLastMessageTime(receivetime);
	status = SDESinf.SetPrivateValue(prefix,prefixlen,value,valuelen);
	if(status == ERR_RTP_SDES_MAXPRIVITEMS)
		return 0; // don't stop processing just because the number of items is full
	return status;
	}
#endif // RTP_SUPPORT_SDESPRIV

int RTPInternalSourceData::ProcessBYEPacket(const BYTE *reason,size_t reasonlen,const RTPTime &receivetime) {

	if(byereason) {
		delete []byereason;
		byereason = NULL;
		byereasonlen = 0;
		}

	byetime = receivetime;
	byereason = new BYTE[reasonlen];
	if(!byereason)
		return ERR_RTP_OUTOFMEM;
	memcpy(byereason,reason,reasonlen);
	byereasonlen = reasonlen;
	receivedbye = true;
	stats.SetLastMessageTime(receivetime);
	return 0;
	}


RTCPRRPacket::RTCPRRPacket(BYTE *data,size_t datalength)
	: RTCPPacket(RR,data,datalength) {
	knownformat = false;
	
	RTCPCommonHeader *hdr;
	size_t len = datalength;
	size_t expectedlength;
	
	hdr = (RTCPCommonHeader *)data;
	if(hdr->padding) {
		BYTE padcount = data[datalength-1];
		if((padcount & 0x03) != 0) // not a multiple of four! (see rfc 3550 p 37)
			return;
		if(((size_t)padcount) >= len)
			return;
		len -= (size_t)padcount;
		}

	expectedlength = sizeof(RTCPCommonHeader)+sizeof(DWORD);
	expectedlength += sizeof(RTCPReceiverReport)*((int)hdr->count);

	if(expectedlength != len)
		return;
	
	knownformat = true;
	}

#ifdef RTPDEBUG
void RTCPRRPacket::Dump() {

	RTCPPacket::Dump();

	if(!IsKnownFormat())
		std::cout << "    Unknown format" << std::endl;
	else {
		int num = GetReceptionReportCount();
		int i;

		std::cout << "    SSRC of sender:     " << GetSenderSSRC() << std::endl;
		for(i=0 ; i < num ; i++) {
			std::cout << "    Report block " << i << std::endl;
			std::cout << "        SSRC:           " << GetSSRC(i) << std::endl;
			std::cout << "        Fraction lost:  " << (DWORD)GetFractionLost(i) << std::endl;
			std::cout << "        Packets lost:   " << GetLostPacketCount(i) << std::endl;
			std::cout << "        Seq. nr.:       " << GetExtendedHighestSequenceNumber(i) << std::endl;
			std::cout << "        Jitter:         " << GetJitter(i) << std::endl;
			std::cout << "        LSR:            " << GetLSR(i) << std::endl;
			std::cout << "        DLSR:           " << GetDLSR(i) << std::endl;
			}
		}	
	}
#endif // RTPDEBUG

RTCPSRPacket::RTCPSRPacket(BYTE *data,size_t datalength)
	: RTCPPacket(SR,data,datalength) {
	knownformat = false;
	
	RTCPCommonHeader *hdr;
	size_t len = datalength;
	size_t expectedlength;
	
	hdr = (RTCPCommonHeader *)data;
	if(hdr->padding)	{
		BYTE padcount = data[datalength-1];
		if((padcount & 0x03) != 0) // not a multiple of four! (see rfc 3550 p 37)
			return;
		if(((size_t)padcount) >= len)
			return;
		len -= (size_t)padcount;
		}

	expectedlength = sizeof(RTCPCommonHeader)+sizeof(DWORD)+sizeof(RTCPSenderReport);
	expectedlength += sizeof(RTCPReceiverReport)*((int)hdr->count);

	if(expectedlength != len)
		return;
	
	knownformat = true;
	}

#ifdef RTPDEBUG
void RTCPSRPacket::Dump() {

	RTCPPacket::Dump();

	if(!IsKnownFormat())
		std::cout << "    Unknown format" << std::endl;
	else {
		int num = GetReceptionReportCount();
		int i;
		RTPNTPTime t = GetNTPTimestamp();

		std::cout << "    SSRC of sender:     " << GetSenderSSRC() << std::endl;
		std::cout << "    Sender info:" << std::endl;
		std::cout << "        NTP timestamp:  " << t.GetMSW() << ":" << t.GetLSW() << std::endl;
		std::cout << "        RTP timestamp:  " << GetRTPTimestamp() << std::endl;
		std::cout << "        Packet count:   " << GetSenderPacketCount() << std::endl;
		std::cout << "        Octet count:    " << GetSenderOctetCount() << std::endl;
		for(i=0 ; i < num ; i++)	{
			std::cout << "    Report block " << i << std::endl;
			std::cout << "        SSRC:           " << GetSSRC(i) << std::endl;
			std::cout << "        Fraction lost:  " << (DWORD)GetFractionLost(i) << std::endl;
			std::cout << "        Packets lost:   " << GetLostPacketCount(i) << std::endl;
			std::cout << "        Seq. nr.:       " << GetExtendedHighestSequenceNumber(i) << std::endl;
			std::cout << "        Jitter:         " << GetJitter(i) << std::endl;
			std::cout << "        LSR:            " << GetLSR(i) << std::endl;
			std::cout << "        DLSR:           " << GetDLSR(i) << std::endl;
			}
		}	
	}
#endif // RTPDEBUG

RTCPAPPPacket::RTCPAPPPacket(BYTE *data,size_t datalength)
	: RTCPPacket(APP,data,datalength) {
	knownformat = false;
	
	RTCPCommonHeader *hdr;
	size_t len = datalength;
	
	hdr = (RTCPCommonHeader *)data;
	if(hdr->padding) {
		BYTE padcount = data[datalength-1];
		if((padcount & 0x03) != 0) // not a multiple of four! (see rfc 3550 p 37)
			return;
		if(((size_t)padcount) >= len)
			return;
		len -= (size_t)padcount;
		}
	
	if(len < (sizeof(RTCPCommonHeader)+sizeof(DWORD)*2))
		return;
	len -= (sizeof(RTCPCommonHeader)+sizeof(DWORD)*2);
	appdatalen = len;
	knownformat = true;
	}

#ifdef RTPDEBUG
void RTCPAPPPacket::Dump() {

	RTCPPacket::Dump();

	if(!IsKnownFormat())	{
		std::cout << "    Unknown format!" << std::endl;
		}
	else {
		std::cout << "    SSRC:   " << GetSSRC() << std::endl;
		
		char str[5];
		memcpy(str,GetName(),4);
		str[4] = 0;
		std::cout << "    Name:   " << std::string(str).c_str() << std::endl;
		std::cout << "    Length: " << GetAPPDataLength() << std::endl;
		}
	}
#endif // RTPDEBUG

RTCPBYEPacket::RTCPBYEPacket(BYTE *data,size_t datalength) : RTCPPacket(BYE,data,datalength) {

	knownformat = false;
	reasonoffset = 0;	
	
	RTCPCommonHeader *hdr;
	size_t len = datalength;
	
	hdr = (RTCPCommonHeader *)data;
	if(hdr->padding)	{
		BYTE padcount = data[datalength-1];
		if((padcount & 0x03) != 0) // not a multiple of four! (see rfc 3550 p 37)
			return;
		if(((size_t)padcount) >= len)
			return;
		len -= (size_t)padcount;
		}
	
	size_t ssrclen = ((size_t)(hdr->count))*sizeof(DWORD) + sizeof(RTCPCommonHeader);
	if(ssrclen > len)
		return;
	if(ssrclen < len) // there's probably a reason for leaving
	{
		BYTE *reasonlength = (data+ssrclen);
		size_t reaslen = (size_t)(*reasonlength);
		if(reaslen > (len-ssrclen-1))
			return;
		reasonoffset = ssrclen;
		}
	knownformat = true;
	}

#ifdef RTPDEBUG
void RTCPBYEPacket::Dump() {

	RTCPPacket::Dump();
	if(!IsKnownFormat())	{
		std::cout << "    Unknown format" << std::endl;
		return;	
		}

	int num = GetSSRCCount();
	int i;

	for(i=0; i < num ; i++)
		std::cout << "    SSRC: " << GetSSRC(i) << std::endl;
	if(HasReasonForLeaving())	{
		char str[1024];
		memcpy(str,GetReasonData(),GetReasonLength());
		str[GetReasonLength()] = 0;
		std::cout << "    Reason: " << str << std::endl;
		}
	}
#endif // RTPDEBUG


SocketData::SocketData() {
	Reset();
	}

void SocketData::Reset() {
	m_lengthBufferOffset = 0;
	m_dataLength = 0; 
	m_dataBufferOffset = 0;
	m_pDataBuffer = 0;
	}

SocketData::~SocketData() {
//	assert(m_pDataBuffer == 0); // Should be deleted externally to avoid storing a memory manager in the class
	}

int SocketData::ProcessAvailableBytes(SocketType sock, int availLen, bool &complete) {

	const int numLengthBuffer = 2;
	if(m_lengthBufferOffset < numLengthBuffer) // first we need to get the length
	{
//		assert(m_pDataBuffer == 0);
		int num = numLengthBuffer-m_lengthBufferOffset;
		if(num > availLen)
			num = availLen;

		int r = 0;
		if(num > 0) {
			r = (int)sock->Receive((char *)(m_lengthBuffer+m_lengthBufferOffset), num, 0);
			if(r < 0)
				return ERR_RTP_TCPTRANS_ERRORINRECV;
			}

		m_lengthBufferOffset += r;
		availLen -= r;

//		assert(m_lengthBufferOffset <= numLengthBuffer);
		if(m_lengthBufferOffset == numLengthBuffer) // we can constuct a length
		{
			int l=0;
			for(int i=numLengthBuffer-1, shift=0; i>=0; i--, shift += 8)
				l |= ((int)m_lengthBuffer[i]) << shift;

			m_dataLength = l;
			m_dataBufferOffset = 0;

			//cout << "Expecting " << m_dataLength << " bytes" << endl;

			// avoid allocation of length 0
			if(l == 0)
				l = 1;

			// We don't yet know if it's an RTP or RTCP packet, so we'll stick to RTP
			m_pDataBuffer = new BYTE[l];
			if(!m_pDataBuffer)
				return ERR_RTP_OUTOFMEM;
			}
		}

	if(m_lengthBufferOffset == numLengthBuffer && m_pDataBuffer) // the last one is to make sure we didn't run out of memory
	{
		if(m_dataBufferOffset < m_dataLength) {
			int num = m_dataLength-m_dataBufferOffset;
			if(num > availLen)
				num = availLen;

			int r = 0;
			if(num > 0) {
				r = (int)sock->Receive((char *)(m_pDataBuffer+m_dataBufferOffset), num, 0);
				if(r < 0)
					return ERR_RTP_TCPTRANS_ERRORINRECV;
				}

			m_dataBufferOffset += r;
			availLen -= r;
			}

		if(m_dataBufferOffset == m_dataLength)
			complete = true;
		}
	return 0;
	}



CDataSocket::CDataSocket() {
	}

CDataSocket::~CDataSocket() {
	}

BOOL CDataSocket::Create(unsigned int port) {
	return CAsyncSocket::Create(port,SOCK_DGRAM/*,FD_READ*/);
	}

void CDataSocket::OnClose(int nErr) {

	Close();
	}

void CDataSocket::OnReceive(int nErr) {
	BYTE myBuf[2048];
	char string[2048],*s,*s1,*parms;
	int i,n;


	}

void CDataSocket::OnSend(int nErr) {

	}





NALUTypeBase::NALUTypeBase(): FrameTypeBase() {
	}

bool NALUTypeBase::NeedPrefixParameterOnce() {
  return false;
	}

int NALUTypeBase::ParseParaFromSDP(SDPMediaInfo &sdpMediaInfo) {
  return 0;
	}

uint8_t *NALUTypeBase::PrefixXPS(uint8_t *buf, size_t *size, CStringEx xps) {

	if(!buf)
		return NULL;
	if(!size) 
		return NULL;

	*size = 0;

	buf[0] = 0; buf[1] = 0; buf[2] = 0; buf[3] = 1;
	*size += 4;

	unsigned int XpsSize = 0;
	xps.Decode64();
	unsigned char *xpsBuf = (unsigned char *)(LPCTSTR)xps;
  if(!xpsBuf) 
		return NULL;
	memcpy(buf + (*size), xpsBuf, XpsSize);
	*size += XpsSize;
	delete[] xpsBuf;
	xpsBuf = NULL;

	return buf;
	}

const string NALUTypeBase_H264::ENCODE_TYPE = "H264";

H264TypeInterface H264TypeInterface_H264Obj;
H264TypeInterfaceSTAP_A 	H264TypeInterfaceSTAP_AObj;
H264TypeInterfaceSTAP_B 	H264TypeInterfaceSTAP_BObj;
H264TypeInterfaceMTAP_16 H264TypeInterfaceMTAP_16Obj;
H264TypeInterfaceMTAP_24 H264TypeInterfaceMTAP_24Obj;
H264TypeInterfaceFU_A 	H264TypeInterfaceFU_AObj;
H264TypeInterfaceFU_B 	H264TypeInterfaceFU_BObj;

H264TypeInterface *H264TypeInterface::NalUnitType_H264[PACKETIZATION_MODE_NUM_H264][NAL_UNIT_TYPE_NUM_H264] = {
	/* Packetization Mode: Single NAL */ 
	{
		NULL,                      &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj, 
		&H264TypeInterface_H264Obj,     &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj, 
		&H264TypeInterface_H264Obj,     &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj, 
		&H264TypeInterface_H264Obj,     NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL
	},

	/* Packetization Mode: Non-interleaved */ 
	{
		NULL,                      &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj, 
		&H264TypeInterface_H264Obj,     &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj, 
		&H264TypeInterface_H264Obj,     &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj,            &H264TypeInterface_H264Obj, 
		&H264TypeInterface_H264Obj,     NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		&H264TypeInterfaceSTAP_AObj,                NULL,                             NULL,                             NULL, 
		&H264TypeInterfaceFU_AObj,                  NULL,                             NULL,                             NULL
	},

	/* Packetization Mode: Interleaved */ 
	{
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      NULL,                             NULL,                             NULL, 
		NULL,                      &H264TypeInterfaceSTAP_BObj,                       &H264TypeInterfaceMTAP_16Obj,                      &H264TypeInterfaceMTAP_24Obj, 
		&H264TypeInterfaceFU_AObj,                  &H264TypeInterfaceFU_BObj,                         NULL,                             NULL
	}
};

NALUTypeBase_H264::NALUTypeBase_H264():NALUTypeBase() {

    prefixParameterOnce = true;
    Packetization = PACKET_MODE_SINGAL_NAL;
    NALUType = NULL;
    SPS.assign("");
    PPS.assign("");
	}


void NALUTypeBase_H264::Init()  {
  InsertXPS();
	}

uint16_t NALUTypeBase_H264::ParseNALUHeader_F(const uint8_t *rtp_payload) {
	uint16_t NALUHeader_F_Mask = 0x0080; // binary: 1000_0000

	if(!rtp_payload) 
		return 0;
	return (rtp_payload[0] & NALUHeader_F_Mask);
	}

uint16_t NALUTypeBase_H264::ParseNALUHeader_NRI(const uint8_t *rtp_payload) {
	uint16_t NALUHeader_NRI_Mask = 0x0060; // binary: 0110_0000

	if(!rtp_payload) 
		return 0;
	return (rtp_payload[0] & NALUHeader_NRI_Mask);
	}

uint16_t NALUTypeBase_H264::ParseNALUHeader_Type(const uint8_t *rtp_payload) {
	uint16_t NALUHeader_Type_Mask = 0x001F; // binary: 0001_1111

	if(!rtp_payload) 
		return 0;
	return (rtp_payload[0] & NALUHeader_Type_Mask);
	}

bool NALUTypeBase_H264::IsPacketThisType(const uint8_t *rtp_payload) {
	// NAL type is valid in the range of [1,12]
	uint16_t NalType = ParseNALUHeader_Type(rtp_payload);

	return ((1 <= NalType) && (NalType <= 12));
	}

size_t NALUTypeBase_H264::CopyData(uint8_t *buf, uint8_t *data, size_t size) {
	size_t CopySize = 0;

	if(!buf || !data) 
		return 0;

  if(!NALUType) 
		return 0;

	uint8_t NALUHeader ;
	NALUHeader = (uint8_t)(NALUType->ParseNALUHeader_F(data)      | 
			NALUType->ParseNALUHeader_NRI(data)    | 
			NALUType->ParseNALUHeader_Type(data)
			);

	if(StartFlag) {
		// NALU start code size
		buf[0]=0; buf[1]=0; buf[2]=0; buf[3] = 1;
		CopySize += 4; 
        buf[CopySize++] = (NALUHeader & 0xFF);
		// memcpy(buf + CopySize, &NALUHeader, sizeof(NALUHeader));
		// CopySize += sizeof(NALUHeader);
		}
	const int SkipHeaderSize = NALUType->SkipHeaderSize(data);
	memcpy(buf + CopySize, data + SkipHeaderSize, size - SkipHeaderSize);
	CopySize += size - SkipHeaderSize;

	return CopySize;
	}

H264TypeInterface *NALUTypeBase_H264::GetNaluRtpType(int packetization, int nalu_type_id) {

	if(!IS_NALU_TYPE_VALID_H264(nalu_type_id))
		return NULL;

	return H264TypeInterface::NalUnitType_H264[packetization][nalu_type_id];
	}

uint8_t *NALUTypeBase_H264::PrefixParameterOnce(uint8_t *buf, size_t *size) {
  const size_t NALU_StartCodeSize = 4;
  size_t SizeTmp = 0;
	CStringEx S;

  if(!buf) 
		return NULL;
  if(!size) 
		return NULL;

	// non va ma chissene non serve S=SPS.c_str;
  if(!NALUTypeBase::PrefixXPS(buf + (*size), &SizeTmp, S) || SizeTmp <= NALU_StartCodeSize) {
    fprintf(stderr, "\033[31mWARNING: No SPS\033[0m\n");
    return NULL;
	  }
  *size += SizeTmp;

// idem	S=PPS.c_str;
  if(!NALUTypeBase::PrefixXPS(buf + (*size), &SizeTmp, S) || SizeTmp <= NALU_StartCodeSize) {
    fprintf(stderr, "\033[31mWARNING: No PPS\033[0m\n");
    return NULL;
		}
  *size += SizeTmp;

  NotInsertXPSAgain();

  return buf;
	}

bool NALUTypeBase_H264::NeedPrefixParameterOnce()  {
  return prefixParameterOnce;
	}

int NALUTypeBase_H264::ParseParaFromSDP(SDPMediaInfo &sdpMediaInfo) {
  map<int, map<SDP_ATTR_ENUM, string> >::iterator it ; //= sdpMediaInfo.fmtMap.begin();

  if(it->second.find(ATTR_SPS) != it->second.end()) {
    SetSPS(it->second[ATTR_SPS]);
    }
  if(it->second.find(ATTR_PPS) != it->second.end()) {
    SetPPS(it->second[ATTR_PPS]);
    }
  if(it->second.find(PACK_MODE) != it->second.end()) {
//    stringstream ssPackMode;
//    ssPackMode << it->second[PACK_MODE];
//    ssPackMode >> Packetization;
    // cout << "debug: Packetization=" << NewMediaSession.Packetization << endl;;
    }
  return 0;
	}

int NALUTypeBase_H264::ParsePacket(const uint8_t *packet, size_t size, bool *EndFlagTmp) {

  if(!packet || !EndFlagTmp)
        return -1;
	if(!IS_PACKET_MODE_VALID(Packetization)) {
//		cerr << "Error(H264): Invalid Packetization Mode" << endl;
		return -2;
		}
		 
  int PM = Packetization;
  int NT = ParseNALUHeader_Type(packet);
  NALUType = GetNaluRtpType(PM, NT);
  if(!NALUType) {
    printf("Error(H264): Unknown NALU Type(PM=%d,NT=%d)\n", PM, NT);
    return -3;
    }

  StartFlag = NALUType->IsPacketStart(packet);
  EndFlag = NALUType->IsPacketEnd(packet);
  *EndFlagTmp = EndFlag;
  return 0;
	}

const uint8_t H264TypeInterfaceSTAP_A::STAP_A_ID = _STAP_A_ID; // decimal: 24
const uint8_t H264TypeInterfaceSTAP_B::STAP_B_ID = _STAP_B_ID; // decimal: 25
const uint8_t H264TypeInterfaceMTAP_16::MTAP_16_ID = _MTAP_16_ID; // decimal: 26
const uint8_t H264TypeInterfaceMTAP_24::MTAP_24_ID = _MTAP_24_ID; // decimal: 27
const uint8_t H264TypeInterfaceFU_A::FU_A_ID = _FU_A_ID; // decimal: 28
const uint8_t H264TypeInterfaceFU_B::FU_B_ID = _FU_B_ID; // decimal: 29


bool H264TypeInterfaceSTAP_A::IsPacketStart(const uint8_t *rtp_payload)  {
	return true;
	}

bool H264TypeInterfaceSTAP_A::IsPacketEnd(const uint8_t *rtp_payload) {
	return true;
	}

bool H264TypeInterfaceSTAP_A::IsPacketThisType(const uint8_t *rtp_payload) {

	if(!rtp_payload) 
		return false;
	return (STAP_A_ID == (rtp_payload[0] & STAP_A_ID));
	}

bool H264TypeInterfaceFU_A::IsPacketThisType(const uint8_t *rtp_payload) {

	if(!rtp_payload) 
		return false;
	return (FU_A_ID == (rtp_payload[0] & FU_A_ID));
	}

uint16_t H264TypeInterfaceFU_A::ParseNALUHeader_F(const uint8_t *rtp_payload) {

	if(!rtp_payload) 
		return FU_A_ERR;
	if(FU_A_ID != (rtp_payload[0] & FU_A_ID)) 
		return FU_A_ERR;

	uint16_t NALUHeader_F_Mask = 0x0080; // binary: 1000_0000

	// "F" at the byte of rtp_payload[0]
	return (rtp_payload[0] & NALUHeader_F_Mask);
	}

uint16_t H264TypeInterfaceFU_A::ParseNALUHeader_NRI(const uint8_t *rtp_payload) {

	if(!rtp_payload) 
		return FU_A_ERR;
	if(FU_A_ID != (rtp_payload[0] & FU_A_ID)) 
		return FU_A_ERR;

	uint16_t NALUHeader_NRI_Mask = 0x0060; // binary: 0110_0000

	// "NRI" at the byte of rtp_payload[0]
	return (rtp_payload[0] & NALUHeader_NRI_Mask);
	}

uint16_t H264TypeInterfaceFU_A::ParseNALUHeader_Type(const uint8_t *rtp_payload) {

	if(!rtp_payload) 
		return FU_A_ERR;
	if(FU_A_ID != (rtp_payload[0] & FU_A_ID)) 
		return FU_A_ERR;

	uint16_t NALUHeader_Type_Mask = 0x001F; // binary: 0001_1111

	// "Type" at the byte of rtp_payload[0]
	return (rtp_payload[1] & NALUHeader_Type_Mask);
	}

bool H264TypeInterfaceFU_A::IsPacketStart(const uint8_t *rtp_payload) {

	if(!IsPacketThisType(rtp_payload)) 
		return false;

	uint8_t PacketS_Mask = 0x80; // binary:1000_0000

	return (rtp_payload[1] & PacketS_Mask);
	}

bool H264TypeInterfaceFU_A::IsPacketEnd(const uint8_t *rtp_payload) {

	if(!IsPacketThisType(rtp_payload)) 
		return false;

	uint8_t PacketE_Mask = 0x40; // binary:0100_0000

	return (rtp_payload[1] & PacketE_Mask);
	}

bool H264TypeInterfaceFU_A::IsPacketReserved(const uint8_t *rtp_payload) {

	if(!IsPacketThisType(rtp_payload)) 
		return false;

	uint8_t PacketR_Mask = 0x20; // binary:0010_0000

	return (rtp_payload[1] & PacketR_Mask);
	}

