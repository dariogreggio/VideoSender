// GD/C adapted 2023-2026 da Ansersion https://www.cnblogs.com/ansersion/p/6959690.html


#include <map>
using namespace std;

#include <stdint.h>

#define PORT_RTSP 				554
#define VERSION_RTSP 			"1.0"
#define VERSION_HTTP 			"1.1"
#define SELECT_TIMEOUT_SEC 		1
#define SELECT_TIMEOUT_USEC 	0

#define CHECK_OK 				1
#define CHECK_ERROR				0

#define TRANS_OK 				1
#define TRANS_ERROR 			0

#define RECV_BUF_SIZE 			8192
#define SEARCH_PORT_RTP_FROM 	10330 // '10330' is chosen at random(must be a even number)

typedef void (*DESTROIED_CLBK) ();

#define TIMEOUT_MICROSECONDS 	1000000 // wait for a packet at most 1 second
#define GET_SPS_PPS_PERIOD 1000		// GD
#define MEDIA_BUFSIZ 65535
#define MD5_BUF_SIZE 256
#define MD5_SIZE 256

enum SessionType {
  VIDEO_SESSION = 0, 
  AUDIO_SESSION
	};

enum ErrorType {
  RTSP_NO_ERROR = 0,
  RTSP_INVALID_URI,
	RTSP_SEND_ERROR, 
	RTSP_RECV_ERROR,
	RTSP_PARSE_SDP_ERROR,
	RTSP_INVALID_MEDIA_SESSION,
	RTSP_RESPONSE_BLANK,
	RTSP_RESPONSE_200,
	RTSP_RESPONSE_400,
	RTSP_RESPONSE_401,
	RTSP_RESPONSE_404,
	RTSP_RESPONSE_40X,
	RTSP_RESPONSE_500,
	RTSP_RESPONSE_501,
	RTSP_RESPONSE_50X,
  RTSP_UNKNOWN_ERROR
	};

#define MAX_SPS_SIZE 	256
typedef struct SPS_t{
	unsigned char Sps[MAX_SPS_SIZE];
	size_t Size;
	} SPS_t;

#define MAX_PPS_SIZE 	256
typedef struct PPS_t {
	unsigned char Pps[MAX_PPS_SIZE];
	size_t Size;
	} PPS_t;

typedef struct Buffer_t {
	BYTE *Buf;
	size_t Size;
	} Buffer_t;


enum SDP_ATTR_ENUM {
  MEDIA_TYPE_NAME, // "video", "audio"
  CODEC_TYPE, // "H264", "MPA"...
  TIME_RATE, // "90000", "8000"...
  CHANNEL_NUM, // "1", "2"...
  PACK_MODE, // for h264/h265
  ATTR_SPS, // for h264/h265
  ATTR_PPS, // for h264/h265
  ATTR_VPS, // for h265
  CONTROL_URI, // "rtsp://127.0.0.1:554/ansersion/trackID=0"...
	};

// o=<username> <session id> <version> <network type> <address type> <address>
typedef struct SDPOriginStruct {
  CString userName;
  long sessionId;			// sembrano long long... boh
  long version;
  CString networkType;
  CString addressType;
  CString address;

  // SDPOriginStruct {
  //     userName = "";
  //     sessionId = 0;
  //     version = 0;
  //     networkType = "";
  //     addressType = "";
  //     address = "";
  // };
	} OriginStruct;

typedef struct SDPConnectionData {
  CString networkType;
  CString addressType;
  CString address;

  // SDPConnectionData {
  //     networkType = "";
  //     addressType = "";
  //     address = "";
  // };
	} SDPConnectionData;

typedef struct SDPSessionTime {
  long startTime;
  long stopTime;

  // SDPSessionTime {
  //     startTime = 0;
  //     stopTime = 0;
  // };
	} SDPSessionTime;

typedef struct SDPMediaInfo {
  CString mediaType;
  CString ports;
  CString transProt;
  map<int, map<SDP_ATTR_ENUM, CString> > fmtMap;
  CString controlURI;

  // SDPMediaInfo {
  //     mediaType = "";
  //     ports = "";
  //     transProt = "";
  //     fmtMap.clear();
  //     controlURI = "";
  // };
	} SDPMediaInfo;

class SDPData {
public:
  SDPData();
  ~SDPData();
  void parse(CString sdp);
  int getSdpVersion() {return sdpVersion;}
  CString getSessionName() {return sessionName;}
  SDPOriginStruct getSdpOriginStruct() {return sdpOriginStruct;}
  SDPConnectionData getSdpConnectionData() {return sdpConnectionData;}
  SDPSessionTime getSdpSessionTime() {return sdpSessionTime;}
  map<CString, SDPMediaInfo> getMediaInfoMap() {return mediaInfoMap;}

private:
  /* RFC2327.6 */
  int sdpVersion;
  CString sessionName;
  SDPOriginStruct sdpOriginStruct;
  SDPConnectionData sdpConnectionData;
  SDPSessionTime sdpSessionTime;
  map<CString, SDPMediaInfo> mediaInfoMap;

	};

class FrameTypeBase {
public:
  static FrameTypeBase *CreateNewFrameType(CString &EncodeType);
  static void DestroyFrameType(FrameTypeBase *frameTypeBase);

public:
	FrameTypeBase() {};
	virtual ~FrameTypeBase() {};

public:
  virtual void Init() {}
  virtual BYTE * PrefixParameterOnce(BYTE *buf, size_t *size) {return buf;}
  virtual bool NeedPrefixParameterOnce() { return false; }
  virtual BYTE *SuffixParameterOnce(BYTE *buf, size_t *size) {return buf;}
  virtual bool NeedSuffixParameterOnce() { return false; }
  virtual int PrefixParameterEveryFrame() {return 0;}
  virtual int PrefixParameterEveryPacket() {return 0;}
  virtual int SuffixParameterOnce() {return 0;}
  virtual int SuffixParameterEveryFrame() {return 0;}
  virtual int SuffixParameterEveryPacket() {return 0;}
	virtual int ParsePacket(const BYTE *RTPPayload, size_t size, bool * EndFlag) {
          if(EndFlag) *EndFlag = true;
            return 0;
        }
	virtual int ParseFrame(const BYTE *RTPPayload) {return 0;}

  virtual int ParseParaFromSDP(SDPMediaInfo &sdpMediaInfo) {return 0;}

	/* To play the media sessions 
	 * return: 
	 * 	0: not a complete frame, which means there are more packets; other: a complete frame
	 * */
	virtual bool IsFrameComplete(const BYTE *RTPPayload) {return true;}
	// virtual int AssemblePacket(const BYTE * RTPPayload) {return 0;}
	// virtual int GetFlagOffset(const BYTE * RTPPayload) {return 0;}
	virtual size_t CopyData(BYTE *buf, BYTE *data, size_t size) {return 0;}
	};


class StreamParameters // ###2015-01-11### //
{
public:
	/* For general media session */
	unsigned int PayloadType;
	CString EncodeType;
	unsigned int TimeRate;

	/* For H264 and H265*/
	int Packetization; 
	CString SPS;
	CString PPS;
	};

class MediaSession;

int checkerror(int rtperr);

typedef CSocket /*CAsyncSocket pare usato solo da TCP, non UDP*/ *SocketType;
typedef void (*DESTROIED_RTP_CLBK) ();
typedef void (*RECV_RTSP_CMD_CLBK)(char * rtsp_cmd);
enum RecvStateEnum {
  RECV_LEN,
  RECV_DATA,
  COMMIT_BYE,
  GOT_BYE,
	};

#define RTP_OK 		1
#define RTP_ERROR 	0

#define RTP_VERSION							2
#define RTP_MAXCSRCS							15
#define RTP_MINPACKETSIZE						600
#define RTP_DEFAULTPACKETSIZE						1400
#define RTP_PROBATIONCOUNT						2
#define RTP_MAXPRIVITEMS						256
#define RTP_SENDERTIMEOUTMULTIPLIER					2
#define RTP_BYETIMEOUTMULTIPLIER					1
#define RTP_MEMBERTIMEOUTMULTIPLIER					5
#define RTP_COLLISIONTIMEOUTMULTIPLIER					10
#define RTP_NOTETTIMEOUTMULTIPLIER					25
#define RTP_DEFAULTSESSIONBANDWIDTH					10000.0

#define RTP_RTCPTYPE_SR							200
#define RTP_RTCPTYPE_RR							201
#define RTP_RTCPTYPE_SDES						202
#define RTP_RTCPTYPE_BYE						203
#define RTP_RTCPTYPE_APP						204

#define RTCP_SDES_ID_CNAME						1
#define RTCP_SDES_ID_NAME						2
#define RTCP_SDES_ID_EMAIL						3
#define RTCP_SDES_ID_PHONE						4
#define RTCP_SDES_ID_LOCATION						5
#define RTCP_SDES_ID_TOOL						6
#define RTCP_SDES_ID_NOTE						7
#define RTCP_SDES_ID_PRIVATE						8
#define RTCP_SDES_NUMITEMS_NONPRIVATE					7
#define RTCP_SDES_MAXITEMLENGTH						255

#define RTCP_BYE_MAXREASONLENGTH					255
#define RTCP_DEFAULTMININTERVAL						5.0	
#define RTCP_DEFAULTBANDWIDTHFRACTION					0.05
#define RTCP_DEFAULTSENDERFRACTION					0.25
#define RTCP_DEFAULTHALFATSTARTUP					true
#define RTCP_DEFAULTIMMEDIATEBYE					true
#define RTCP_DEFAULTSRBYE						true

#define RTPTCPTRANS_MAXPACKSIZE							65535

#define RTPUDPV4TRANS_HASHSIZE									8317
#define RTPUDPV4TRANS_DEFAULTPORTBASE								5000

#define RTPUDPV4TRANS_RTPRECEIVEBUFFER							32768
#define RTPUDPV4TRANS_RTCPRECEIVEBUFFER							32768
#define RTPUDPV4TRANS_RTPTRANSMITBUFFER							32768
#define RTPUDPV4TRANS_RTCPTRANSMITBUFFER						32768

#define ERR_RTP_OUTOFMEM                                          -1
#define ERR_RTP_NOTHREADSUPPORT                                   -2
#define ERR_RTP_COLLISIONLIST_BADADDRESS                          -3
#define ERR_RTP_HASHTABLE_ELEMENTALREADYEXISTS                    -4
#define ERR_RTP_HASHTABLE_ELEMENTNOTFOUND                         -5
#define ERR_RTP_HASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX        -6
#define ERR_RTP_HASHTABLE_NOCURRENTELEMENT                        -7
#define ERR_RTP_KEYHASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX     -8
#define ERR_RTP_KEYHASHTABLE_KEYALREADYEXISTS                     -9
#define ERR_RTP_KEYHASHTABLE_KEYNOTFOUND                          -10
#define ERR_RTP_KEYHASHTABLE_NOCURRENTELEMENT                     -11
#define ERR_RTP_PACKBUILD_ALREADYINIT                             -12
#define ERR_RTP_PACKBUILD_CSRCALREADYINLIST                       -13
#define ERR_RTP_PACKBUILD_CSRCLISTFULL                            -14
#define ERR_RTP_PACKBUILD_CSRCNOTINLIST                           -15
#define ERR_RTP_PACKBUILD_DEFAULTMARKNOTSET                       -16
#define ERR_RTP_PACKBUILD_DEFAULTPAYLOADTYPENOTSET                -17
#define ERR_RTP_PACKBUILD_DEFAULTTSINCNOTSET                      -18
#define ERR_RTP_PACKBUILD_INVALIDMAXPACKETSIZE                    -19
#define ERR_RTP_PACKBUILD_NOTINIT                                 -20
#define ERR_RTP_PACKET_BADPAYLOADTYPE                             -21
#define ERR_RTP_PACKET_DATAEXCEEDSMAXSIZE                         -22
#define ERR_RTP_PACKET_EXTERNALBUFFERNULL                         -23
#define ERR_RTP_PACKET_ILLEGALBUFFERSIZE                          -24
#define ERR_RTP_PACKET_INVALIDPACKET                              -25
#define ERR_RTP_PACKET_TOOMANYCSRCS                               -26
#define ERR_RTP_POLLTHREAD_ALREADYRUNNING                         -27
#define ERR_RTP_POLLTHREAD_CANTINITMUTEX                          -28
#define ERR_RTP_POLLTHREAD_CANTSTARTTHREAD                        -29
#define ERR_RTP_RTCPCOMPOUND_INVALIDPACKET                        -30
#define ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILDING               -31
#define ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILT                  -32
#define ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYGOTREPORT              -33
#define ERR_RTP_RTCPCOMPPACKBUILDER_APPDATALENTOOBIG              -34
#define ERR_RTP_RTCPCOMPPACKBUILDER_BUFFERSIZETOOSMALL            -35
#define ERR_RTP_RTCPCOMPPACKBUILDER_ILLEGALAPPDATALENGTH          -36
#define ERR_RTP_RTCPCOMPPACKBUILDER_ILLEGALSUBTYPE                -37
#define ERR_RTP_RTCPCOMPPACKBUILDER_INVALIDITEMTYPE               -38
#define ERR_RTP_RTCPCOMPPACKBUILDER_MAXPACKETSIZETOOSMALL         -39
#define ERR_RTP_RTCPCOMPPACKBUILDER_NOCURRENTSOURCE               -40
#define ERR_RTP_RTCPCOMPPACKBUILDER_NOREPORTPRESENT               -41
#define ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING                   -42
#define ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT            -43
#define ERR_RTP_RTCPCOMPPACKBUILDER_REPORTNOTSTARTED              -44
#define ERR_RTP_RTCPCOMPPACKBUILDER_TOOMANYSSRCS                  -45
#define ERR_RTP_RTCPCOMPPACKBUILDER_TOTALITEMLENGTHTOOBIG         -46
#define ERR_RTP_RTCPPACKETBUILDER_ALREADYINIT                     -47
#define ERR_RTP_RTCPPACKETBUILDER_ILLEGALMAXPACKSIZE              -48
#define ERR_RTP_RTCPPACKETBUILDER_ILLEGALTIMESTAMPUNIT            -49
#define ERR_RTP_RTCPPACKETBUILDER_NOTINIT                         -50
#define ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON             -51
#define ERR_RTP_SCHEDPARAMS_BADFRACTION                           -52
#define ERR_RTP_SCHEDPARAMS_BADMINIMUMINTERVAL                    -53
#define ERR_RTP_SCHEDPARAMS_INVALIDBANDWIDTH                      -54
#define ERR_RTP_SDES_LENGTHTOOBIG                                 -55
#define ERR_RTP_SDES_MAXPRIVITEMS                                 -56
#define ERR_RTP_SDES_PREFIXNOTFOUND                               -57
#define ERR_RTP_SESSION_ALREADYCREATED                            -58
#define ERR_RTP_SESSION_CANTGETLOGINNAME                          -59
#define ERR_RTP_SESSION_CANTINITMUTEX                             -60
#define ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL                     -61
#define ERR_RTP_SESSION_NOTCREATED                                -62
#define ERR_RTP_SESSION_UNSUPPORTEDTRANSMISSIONPROTOCOL           -63
#define ERR_RTP_SESSION_USINGPOLLTHREAD                           -64
#define ERR_RTP_SOURCES_ALREADYHAVEOWNSSRC                        -65
#define ERR_RTP_SOURCES_DONTHAVEOWNSSRC                           -66
#define ERR_RTP_SOURCES_ILLEGALSDESTYPE                           -67
#define ERR_RTP_SOURCES_SSRCEXISTS                                -68
#define ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL                        -69
#define ERR_RTP_UDPV4TRANS_ALREADYCREATED                         -70
#define ERR_RTP_UDPV4TRANS_ALREADYINIT                            -71
#define ERR_RTP_UDPV4TRANS_ALREADYWAITING                         -72
#define ERR_RTP_UDPV4TRANS_CANTBINDRTCPSOCKET                     -73
#define ERR_RTP_UDPV4TRANS_CANTBINDRTPSOCKET                      -74
#define ERR_RTP_UDPV4TRANS_CANTCREATESOCKET                       -75
#define ERR_RTP_UDPV4TRANS_CANTINITMUTEX                          -76
#define ERR_RTP_UDPV4TRANS_CANTSETRTCPRECEIVEBUF                  -77
#define ERR_RTP_UDPV4TRANS_CANTSETRTCPTRANSMITBUF                 -78
#define ERR_RTP_UDPV4TRANS_CANTSETRTPRECEIVEBUF                   -79
#define ERR_RTP_UDPV4TRANS_CANTSETRTPTRANSMITBUF                  -80
#define ERR_RTP_UDPV4TRANS_COULDNTJOINMULTICASTGROUP              -81
#define ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE                   -82
#define ERR_RTP_UDPV4TRANS_ILLEGALPARAMETERS                      -83
#define ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE                     -84
#define ERR_RTP_UDPV4TRANS_NOLOCALIPS                             -85
#define ERR_RTP_UDPV4TRANS_NOMULTICASTSUPPORT                     -86
#define ERR_RTP_UDPV4TRANS_NOSUCHENTRY                            -87
#define ERR_RTP_UDPV4TRANS_NOTAMULTICASTADDRESS                   -88
#define ERR_RTP_UDPV4TRANS_NOTCREATED                             -89
#define ERR_RTP_UDPV4TRANS_NOTINIT                                -90
#define ERR_RTP_UDPV4TRANS_NOTWAITING                             -91
#define ERR_RTP_UDPV4TRANS_PORTBASENOTEVEN                        -92
#define ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG                    -93
#define ERR_RTP_UDPV6TRANS_ALREADYCREATED                         -94
#define ERR_RTP_UDPV6TRANS_ALREADYINIT                            -95
#define ERR_RTP_UDPV6TRANS_ALREADYWAITING                         -96
#define ERR_RTP_UDPV6TRANS_CANTBINDRTCPSOCKET                     -97
#define ERR_RTP_UDPV6TRANS_CANTBINDRTPSOCKET                      -98
#define ERR_RTP_UDPV6TRANS_CANTCREATESOCKET                       -99
#define ERR_RTP_UDPV6TRANS_CANTINITMUTEX                          -100
#define ERR_RTP_UDPV6TRANS_CANTSETRTCPRECEIVEBUF                  -101
#define ERR_RTP_UDPV6TRANS_CANTSETRTCPTRANSMITBUF                 -102
#define ERR_RTP_UDPV6TRANS_CANTSETRTPRECEIVEBUF                   -103
#define ERR_RTP_UDPV6TRANS_CANTSETRTPTRANSMITBUF                  -104
#define ERR_RTP_UDPV6TRANS_COULDNTJOINMULTICASTGROUP              -105
#define ERR_RTP_UDPV6TRANS_DIFFERENTRECEIVEMODE                   -106
#define ERR_RTP_UDPV6TRANS_ILLEGALPARAMETERS                      -107
#define ERR_RTP_UDPV6TRANS_INVALIDADDRESSTYPE                     -108
#define ERR_RTP_UDPV6TRANS_NOLOCALIPS                             -109
#define ERR_RTP_UDPV6TRANS_NOMULTICASTSUPPORT                     -110
#define ERR_RTP_UDPV6TRANS_NOSUCHENTRY                            -111
#define ERR_RTP_UDPV6TRANS_NOTAMULTICASTADDRESS                   -112
#define ERR_RTP_UDPV6TRANS_NOTCREATED                             -113
#define ERR_RTP_UDPV6TRANS_NOTINIT                                -114
#define ERR_RTP_UDPV6TRANS_NOTWAITING                             -115
#define ERR_RTP_UDPV6TRANS_PORTBASENOTEVEN                        -116
#define ERR_RTP_UDPV6TRANS_SPECIFIEDSIZETOOBIG                    -117
#define ERR_RTP_INTERNALSOURCEDATA_INVALIDPROBATIONTYPE           -118
#define ERR_RTP_SESSION_USERDEFINEDTRANSMITTERNULL                -119
#define ERR_RTP_FAKETRANS_ALREADYCREATED                          -120
#define ERR_RTP_FAKETRANS_ALREADYINIT                             -121
#define ERR_RTP_FAKETRANS_CANTINITMUTEX                           -122
#define ERR_RTP_FAKETRANS_COULDNTJOINMULTICASTGROUP               -123
#define ERR_RTP_FAKETRANS_DIFFERENTRECEIVEMODE                    -124
#define ERR_RTP_FAKETRANS_ILLEGALPARAMETERS                       -125
#define ERR_RTP_FAKETRANS_INVALIDADDRESSTYPE                      -126
#define ERR_RTP_FAKETRANS_NOLOCALIPS                              -127
#define ERR_RTP_FAKETRANS_NOMULTICASTSUPPORT                      -128
#define ERR_RTP_FAKETRANS_NOSUCHENTRY                             -129
#define ERR_RTP_FAKETRANS_NOTAMULTICASTADDRESS                    -130
#define ERR_RTP_FAKETRANS_NOTCREATED                              -131
#define ERR_RTP_FAKETRANS_NOTINIT                                 -132
#define ERR_RTP_FAKETRANS_PORTBASENOTEVEN                         -133
#define ERR_RTP_FAKETRANS_SPECIFIEDSIZETOOBIG                     -134
#define ERR_RTP_FAKETRANS_WAITNOTIMPLEMENTED                      -135
#define ERR_RTP_RTPRANDOMURANDOM_CANTOPEN                         -136
#define ERR_RTP_RTPRANDOMURANDOM_ALREADYOPEN                      -137
#define ERR_RTP_RTPRANDOMRANDS_NOTSUPPORTED                       -138
#define ERR_RTP_EXTERNALTRANS_ALREADYCREATED                      -139
#define ERR_RTP_EXTERNALTRANS_ALREADYINIT                         -140
#define ERR_RTP_EXTERNALTRANS_ALREADYWAITING                      -141
#define ERR_RTP_EXTERNALTRANS_BADRECEIVEMODE                      -142
#define ERR_RTP_EXTERNALTRANS_CANTINITMUTEX                       -143
#define ERR_RTP_EXTERNALTRANS_ILLEGALPARAMETERS                   -144
#define ERR_RTP_EXTERNALTRANS_NOACCEPTLIST                        -145
#define ERR_RTP_EXTERNALTRANS_NODESTINATIONSSUPPORTED             -146
#define ERR_RTP_EXTERNALTRANS_NOIGNORELIST                        -147
#define ERR_RTP_EXTERNALTRANS_NOMULTICASTSUPPORT                  -148
#define ERR_RTP_EXTERNALTRANS_NOSENDER                            -149
#define ERR_RTP_EXTERNALTRANS_NOTCREATED                          -150
#define ERR_RTP_EXTERNALTRANS_NOTINIT                             -151
#define ERR_RTP_EXTERNALTRANS_NOTWAITING                          -152
#define ERR_RTP_EXTERNALTRANS_SENDERROR                           -153
#define ERR_RTP_EXTERNALTRANS_SPECIFIEDSIZETOOBIG                 -154
#define ERR_RTP_UDPV4TRANS_CANTGETSOCKETPORT                      -155
#define ERR_RTP_UDPV4TRANS_NOTANIPV4SOCKET                        -156
#define ERR_RTP_UDPV4TRANS_SOCKETPORTNOTSET                       -157
#define ERR_RTP_UDPV4TRANS_CANTGETSOCKETTYPE                      -158
#define ERR_RTP_UDPV4TRANS_INVALIDSOCKETTYPE                      -159
#define ERR_RTP_UDPV4TRANS_CANTGETVALIDSOCKET                     -160
#define ERR_RTP_UDPV4TRANS_TOOMANYATTEMPTSCHOOSINGSOCKET          -161
#define ERR_RTP_RTPSESSION_CHANGEREQUESTEDBUTNOTIMPLEMENTED       -162
#define ERR_RTP_SECURESESSION_CONTEXTALREADYINITIALIZED           -163
#define ERR_RTP_SECURESESSION_CANTINITIALIZE_SRTPCONTEXT          -164
#define ERR_RTP_SECURESESSION_CANTINITMUTEX                       -165
#define ERR_RTP_SECURESESSION_CONTEXTNOTINITIALIZED               -166
#define ERR_RTP_SECURESESSION_NOTENOUGHDATATOENCRYPT              -167
#define ERR_RTP_SECURESESSION_CANTENCRYPTRTPDATA                  -168
#define ERR_RTP_SECURESESSION_CANTENCRYPTRTCPDATA                 -169
#define ERR_RTP_SECURESESSION_NOTENOUGHDATATODECRYPT              -170
#define ERR_RTP_SECURESESSION_CANTDECRYPTRTPDATA                  -171
#define ERR_RTP_SECURESESSION_CANTDECRYPTRTCPDATA                 -172
#define ERR_RTP_ABORTDESC_ALREADYINIT                             -173
#define ERR_RTP_ABORTDESC_NOTINIT                                 -174
#define ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS              -175
#define ERR_RTP_ABORTDESC_CANTCREATEPIPE                          -176
#define ERR_RTP_SESSION_THREADSAFETYCONFLICT                      -177
#define ERR_RTP_SELECT_ERRORINSELECT                              -178
#define ERR_RTP_SELECT_SOCKETDESCRIPTORTOOLARGE                   -179
#define ERR_RTP_SELECT_ERRORINPOLL                                -180
#define ERR_RTP_TCPTRANS_NOTINIT                                  -181
#define ERR_RTP_TCPTRANS_ALREADYINIT                              -182
#define ERR_RTP_TCPTRANS_ALREADYCREATED                           -183
#define ERR_RTP_TCPTRANS_ILLEGALPARAMETERS                        -184
#define ERR_RTP_TCPTRANS_CANTINITMUTEX                            -185
#define ERR_RTP_TCPTRANS_ALREADYWAITING                           -186
#define ERR_RTP_TCPTRANS_NOTCREATED                               -187
#define ERR_RTP_TCPTRANS_INVALIDADDRESSTYPE                       -188
#define ERR_RTP_TCPTRANS_NOSOCKETSPECIFIED                        -189
#define ERR_RTP_TCPTRANS_NOMULTICASTSUPPORT                       -190
#define ERR_RTP_TCPTRANS_RECEIVEMODENOTSUPPORTED                  -191
#define ERR_RTP_TCPTRANS_SPECIFIEDSIZETOOBIG                      -192
#define ERR_RTP_TCPTRANS_NOTWAITING                               -193
#define ERR_RTP_TCPTRANS_SOCKETALREADYINDESTINATIONS              -194
#define ERR_RTP_TCPTRANS_SOCKETNOTFOUNDINDESTINATIONS             -195
#define ERR_RTP_TCPTRANS_ERRORINSEND                              -196
#define ERR_RTP_TCPTRANS_ERRORINRECV                              -197

#ifdef RTP_SUPPORT_THREAD
	#define SOURCES_LOCK					{ if(needthreadsafety) sourcesmutex.Lock(); }
	#define SOURCES_UNLOCK					{ if(needthreadsafety) sourcesmutex.Unlock(); }
	#define BUILDER_LOCK					{ if(needthreadsafety) buildermutex.Lock(); }
	#define BUILDER_UNLOCK					{ if(needthreadsafety) buildermutex.Unlock(); }
	#define SCHED_LOCK						{ if(needthreadsafety) schedmutex.Lock(); }
	#define SCHED_UNLOCK					{ if(needthreadsafety) schedmutex.Unlock(); }
	#define PACKSENT_LOCK					{ if(needthreadsafety) packsentmutex.Lock(); }
	#define PACKSENT_UNLOCK					{ if(needthreadsafety) packsentmutex.Unlock(); } 
#else
	#define SOURCES_LOCK
	#define SOURCES_UNLOCK
	#define BUILDER_LOCK
	#define BUILDER_UNLOCK
	#define SCHED_LOCK
	#define SCHED_UNLOCK
	#define PACKSENT_LOCK
	#define PACKSENT_UNLOCK
#endif // RTP_SUPPORT_THREAD

#ifdef RTP_SUPPORT_THREAD
	#define MAINMUTEX_LOCK 		{ if(m_threadsafe) m_mainMutex.Lock(); }
	#define MAINMUTEX_UNLOCK	{ if(m_threadsafe) m_mainMutex.Unlock(); }
	#define WAITMUTEX_LOCK		{ if(m_threadsafe) m_waitMutex.Lock(); }
	#define WAITMUTEX_UNLOCK	{ if(m_threadsafe) m_waitMutex.Unlock(); }
#else
	#define MAINMUTEX_LOCK
	#define MAINMUTEX_UNLOCK
	#define WAITMUTEX_LOCK
	#define WAITMUTEX_UNLOCK
#endif // RTP_SUPPORT_THREAD

/** This class is an abstract class which is used to specify destinations, multicast groups etc. */
class RTPAddress {
public:
	/** Identifies the actual implementation being used. */
	enum AddressType  { 
		IPv4Address, /**< Used by the UDP over IPv4 transmitter. */
		IPv6Address, /**< Used by the UDP over IPv6 transmitter. */
		ByteAddress, /**< A very general type of address, consisting of a port number and a number of bytes representing the host address. */
		UserDefinedAddress,  /**< Can be useful for a user-defined transmitter. */
		TCPAddress /**< Used by the TCP transmitter. */
		}; 
	
	/** Returns the type of address the actual implementation represents. */
	AddressType GetAddressType() const	{ return addresstype; }
	/** Creates a copy of the RTPAddress instance.
	 *  Creates a copy of the RTPAddress instance. If mgr is not NULL, the
	 *  corresponding memory manager will be used to allocate the memory for the address 
	 *  copy. 
	 */
	virtual RTPAddress *CreateCopy(/* RTPMemoryManager *mgr*/ ) const = 0;
	/** Checks if the address addr is the same address as the one this instance represents. 
	 *  Checks if the address addr is the same address as the one this instance represents.
	 *  Implementations must be able to handle a NULL argument.
	 */
	virtual bool IsSameAddress(const RTPAddress *addr) const = 0;
	/** Checks if the address addr represents the same host as this instance. 
	 *  Checks if the address addr represents the same host as this instance. Implementations 
	 *  must be able to handle a NULL argument.
	 */
	virtual bool IsFromSameHost(const RTPAddress *addr) const  = 0;

#ifdef RTPDEBUG
	virtual std::string GetAddressString() const = 0;
#endif // RTPDEBUG
	
	virtual ~RTPAddress()						{ }
protected:
	// only allow subclasses to be created
	RTPAddress(const AddressType t) : addresstype(t) 		{ }
private:
	const AddressType addresstype;
	};
class RTPTCPAddress : public RTPAddress {
public:
	/** Creates an instance with which you can use a specific socket
	 *  in the TCP transmitter (must be connected). */
	RTPTCPAddress(CSocket *sock):RTPAddress(TCPAddress)	{ 
		m_socket = sock;
		}
	~RTPTCPAddress()							{ }

	/** Returns the socket that was specified in the constructor. */
	CSocket *GetSocket() const	{ return m_socket; }

	RTPAddress *CreateCopy(/* RTPMemoryManager *mgr */) const;

	// Note that these functions are only used for received packets
	bool IsSameAddress(const RTPAddress *addr) const;
	bool IsFromSameHost(const RTPAddress *addr) const;
#ifdef RTPDEBUG
	std::string GetAddressString() const;
#endif // RTPDEBUG
private:
	CSocket *m_socket;
	};

	/** This class is used to specify wallclock time, delay intervals etc.
 *  This class is used to specify wallclock time, delay intervals etc. 
 *  It stores a number of seconds and a number of microseconds.
 */
#define 	RTCPSCHED_MININTERVAL   1.0
#define CEPOCH 11644473600000000ui64
#define C1000000 1000000ui64
//typedef unsigned long uint64_t;			//SISTEMARE!!

																		/**
 * This is a simple wrapper for the most significant word (MSW) and least 
 * significant word (LSW) of an NTP timestamp.
 */
class RTPNTPTime {
public:
	RTPNTPTime()					{ msw=lsw=0; }
	/** This constructor creates and instance with MSW m and LSW l. */
	RTPNTPTime(DWORD m,DWORD l)					{ msw=m; lsw=l; }

	/** Returns the most significant word. */
	DWORD GetMSW() const								{ return msw; }

	/** Returns the least significant word. */
	DWORD GetLSW() const								{ return lsw; }
private:
	DWORD msw,lsw;
	};

#define RTP_NTPTIMEOFFSET		2208988800UL
class RTPTime {
public:
	/** Creates an instance corresponding to seconds and microseconds. */
	RTPTime(/*long */ long int seconds, DWORD microseconds);
	RTPTime(double d);
	RTPTime(RTPNTPTime ntptime);
	RTPTime() {}
	static RTPTime CurrentTime();
	static void Wait(const RTPTime &delay);
	/** Returns the time stored in this instance, expressed in units of seconds. */
	double GetDouble() const { return m_t; }
	/** Returns the NTP time corresponding to the time stored in this instance. */
	inline RTPNTPTime RTPTime::GetNTPTime() const {
		DWORD sec = (DWORD)m_t;
		DWORD microsec = (DWORD)((m_t - (double)sec)*1e6);

		DWORD msw = sec+RTP_NTPTIMEOFFSET;
		DWORD lsw;
		double x = microsec/1000000.0;
		x *= (65536.0*65536.0);
		lsw = (DWORD)x;

		return RTPNTPTime(msw,lsw);
		}

	bool IsZero() const { return m_t == 0; }
	inline RTPTime& operator+=(const RTPTime &b) { m_t+=b.m_t; return *this; }
	inline RTPTime& operator-=(const RTPTime &b) { m_t-=b.m_t; return *this; }
	inline bool operator<(const RTPTime &b) const { return m_t<b.m_t; }
	inline bool operator>(const RTPTime &b) const { return m_t>b.m_t; }
	inline bool operator<=(const RTPTime &b) const { return m_t<=b.m_t; }
	inline bool operator>=(const RTPTime &b) const { return m_t>=b.m_t; }
//	inline bool operator==(RTPTime &b) { return m_t==b.m_t; }
private:
	static inline uint64_t CalculateMicroseconds(uint64_t performancecount,uint64_t performancefrequency);

	double m_t;
	};

inline RTPTime::RTPTime(double t) {
	m_t = t;
	}

inline RTPTime::RTPTime(/*long*/ long int seconds, DWORD microseconds) {
	if(seconds >= 0)	{
		m_t = (double)seconds + 1e-6*(double)microseconds;
		}
	else {
		/*long*/ long int possec = -seconds;

		m_t = (double)possec + 1e-6*(double)microseconds;
		m_t = -m_t;
		}
	}

inline RTPTime::RTPTime(RTPNTPTime ntptime) {

	if(ntptime.GetMSW() < RTP_NTPTIMEOFFSET)	{
		m_t = 0;
		}
	else {
		DWORD sec = ntptime.GetMSW() - RTP_NTPTIMEOFFSET;
		
		double x = (double)ntptime.GetLSW();
		x /= (65536.0*65536.0);
		x *= 1000000.0;
		DWORD microsec = (DWORD)x;

		m_t = (double)sec + 1e-6*(double)microsec;
		}
	}

inline RTPTime RTPTime::CurrentTime() {
	static int inited = 0;
	static signed __int64/*uint64_t*/ microseconds, initmicroseconds;
	static LARGE_INTEGER performancefrequency;

	signed __int64/*uint64_t*/emulate_microseconds, microdiff;
	SYSTEMTIME systemtime;
	FILETIME filetime;

	LARGE_INTEGER performancecount;

	QueryPerformanceCounter(&performancecount);
    
	if(!inited){
		inited = 1;
		QueryPerformanceFrequency(&performancefrequency);
		GetSystemTime(&systemtime);
		SystemTimeToFileTime(&systemtime,&filetime);
		microseconds = ( ((uint64_t)(filetime.dwHighDateTime) << 32) + (uint64_t)(filetime.dwLowDateTime) ) / (uint64_t)10;
		microseconds-= CEPOCH; // EPOCH
		initmicroseconds = CalculateMicroseconds(performancecount.QuadPart, performancefrequency.QuadPart);
		}
    
	emulate_microseconds = CalculateMicroseconds(performancecount.QuadPart, performancefrequency.QuadPart);

	microdiff = emulate_microseconds - initmicroseconds;

	double t = 1e-6*(double)(microseconds + microdiff);
	return RTPTime(t);
	}

inline uint64_t RTPTime::CalculateMicroseconds(uint64_t performancecount,uint64_t performancefrequency) {
	uint64_t f = performancefrequency;
	uint64_t a = performancecount;
	uint64_t b = a/f;
	uint64_t c = a%f; // a = b*f+c => (a*1000000)/f = b*1000000+(c*1000000)/f

	return b*C1000000+(c*C1000000)/f;
	}

inline void RTPTime::Wait(const RTPTime &delay) {

	if(delay.m_t <= 0)
		return;

	signed __int64/*uint64_t*/sec = (uint64_t)delay.m_t;
	uint64_t microsec = (uint64_t)(1e6*(delay.m_t-(double)sec));
	DWORD t = ((DWORD)sec)*1000+(((DWORD)microsec)/1000);
	Sleep(t);
	}



class RTPTransmissionParams;
/**
 * Helper class for several RTPTransmitter instances, to be able to cancel a
 * call to 'select', 'poll' or 'WSAPoll'.
 *
 * This is a helper class for several RTPTransmitter instances. Typically a
 * call to 'select' (or 'poll' or 'WSAPoll', depending on the platform) is used
 * to wait for incoming data for a certain time. To be able to cancel this wait
 * from another thread, this class provides a socket descriptor that's compatible
 * with e.g. the 'select' call, and to which data can be sent using
 * RTPAbortDescriptors::SendAbortSignal. If the descriptor is included in the
 * 'select' call, the function will detect incoming data and the function stops
 * waiting for incoming data.
 *
 * The class can be useful in case you'd like to create an implementation which
 * uses a single poll thread for several RTPSession and RTPTransmitter instances.
 * This idea is further illustrated in `example8.cpp`.
 */
class RTPAbortDescriptors {
public:
	RTPAbortDescriptors();
	~RTPAbortDescriptors();

	/** Initializes this instance. */
	int Init();

	/** Returns the socket descriptor that can be included in a call to
	 *  'select' (for example).*/
	CSocket *GetAbortSocket() const	{ return m_descriptors[0]; }

	/** Returns a flag indicating if this instance was initialized. */
	bool IsInitialized() const			{ return m_init; }

	/** De-initializes this instance. */
	void Destroy();

	/** Send a signal to the socket that's returned by RTPAbortDescriptors::GetAbortSocket,
	 *  causing the 'select' call to detect that data is available, making the call
	 *  end. */
	int SendAbortSignal();

	/** For each RTPAbortDescriptors::SendAbortSignal function that's called, a call
	 *  to this function can be made to clear the state again. */
	int ReadSignallingByte();

	/** Similar to ReadSignallingByte::ReadSignallingByte, this function clears the signalling
	 *  state, but this also works independently from the amount of times that
	 *  RTPAbortDescriptors::SendAbortSignal was called. */
	int ClearAbortSignal();
private:
	CSocket *m_descriptors[2];
	bool m_init;
	};

class CDataSocket : public CAsyncSocket {
public:
	void OnReceive(int);
	void OnClose(int);
	void OnSend(int);

	CDataSocket();		// TOGLIERE default quando implementi la lista e togli l'array di client!!!
	~CDataSocket();
	BOOL Create(unsigned int port);
	};

class RTPRawPacket;
/** 
 *  Abstract class from which actual transmission components should be derived.
 *  The abstract class RTPTransmitter specifies the interface for
 *  actual transmission components. Currently, three implementations exist:
 *  an UDP over IPv4 transmitter, an UDP over IPv6 transmitter and a transmitter
 *  which can be used to use an external transmission mechanism.
 */
class RTPTransmitter /*: public RTPMemoryObject */ {
public:
	/** Used to identify a specific transmitter. 
	 *  If UserDefinedProto is used in the RTPSession::Create function, the RTPSession
	 *  virtual member function NewUserDefinedTransmitter will be called to create
	 *  a transmission component.
	 */
	enum TransmissionProtocol { 
		IPv4UDPProto, /**< Specifies the internal UDP over IPv4 transmitter. */
		IPv6UDPProto, /**< Specifies the internal UDP over IPv6 transmitter. */
		TCPProto, /**< Specifies the internal TCP transmitter. */
		ExternalProto, /**< Specifies the transmitter which can send packets using an external mechanism, and which can have received packets injected into it - see RTPExternalTransmitter for additional information. */
		UserDefinedProto  /**< Specifies a user defined, external transmitter. */
		};

	/** Three kind of receive modes can be specified. */
	enum ReceiveMode { 
		AcceptAll, /**< All incoming data is accepted, no matter where it originated from. */
		AcceptSome, /**< Only data coming from specific sources will be accepted. */
		IgnoreSome /**< All incoming data is accepted, except for data coming from a specific set of sources. */
		};
protected:
	/** Constructor in which you can specify a memory manager to use. */
	RTPTransmitter(/*RTPMemoryManager *mgr*/) /*: RTPMemoryObject(mgr)	*/	{ /* timeinit.Dummy(); */ }
	RTPAbortDescriptors *m_pAbortDesc; // in case an external one was specified
	RTPAbortDescriptors m_abortDesc;
	CList<RTPRawPacket *,RTPRawPacket *> rawpacketlist;
public:
	virtual ~RTPTransmitter()													{ }
	/** This function must be called before the transmission component can be used. 
	 *  This function must be called before the transmission component can be used. Depending on 
	 *  the value of threadsafe, the component will be created for thread-safe usage or not.
	 */
	virtual int Init(bool threadsafe) = 0;
	/** Prepares the component to be used.
	 *  Prepares the component to be used. The parameter maxpacksize specifies the maximum size 
	 *  a packet can have: if the packet is larger it will not be transmitted. The transparams
	 *  parameter specifies a pointer to an RTPTransmissionParams instance. This is also an abstract 
	 *  class and each actual component will define its own parameters by inheriting a class 
	 *  from RTPTransmissionParams. If transparams is NULL, the default transmission parameters 
	 *  for the component will be used.
	 */
	virtual int Create(size_t maxpacksize, const RTPTransmissionParams *transparams) = 0;
	/** By calling this function, buffers are cleared and the component cannot be used anymore. 
	 *  By calling this function, buffers are cleared and the component cannot be used anymore.
	 *  Only when the Create function is called again can the component be used again. */
	virtual void Destroy() = 0;
	/** Returns the amount of bytes that will be added to the RTP packet by the underlying layers (excluding 
	 *  the link layer). */
	virtual size_t GetHeaderOverhead() = 0;
	/** Checks for incoming data and stores it. */
	virtual int Poll() = 0;
	/** Sets the maximum packet size which the transmitter should allow to s. */
	virtual int SetMaximumPacketSize(size_t s) = 0;	
	/** Adds the address specified by addr to the list of destinations. */
	virtual int AddDestination(const RTPAddress &addr) = 0;
	/** Deletes the address specified by addr from the list of destinations. */
	virtual int DeleteDestination(const RTPAddress &addr) = 0;
	/** Clears the list of destinations. */
	virtual void ClearDestinations() = 0;
	/** Sets the receive mode.
	 *  Sets the receive mode to m, which is one of the following: RTPTransmitter::AcceptAll, 
	 *  RTPTransmitter::AcceptSome or RTPTransmitter::IgnoreSome. Note that if the receive
	 *  mode is changed, all information about the addresses to ignore to accept is lost.
	 */
	virtual int SetReceiveMode(RTPTransmitter::ReceiveMode m) = 0;
	/** Looks up the local host name.
	 *  Looks up the local host name based upon internal information about the local host's 
	 *  addresses. This function might take some time since a DNS query might be done. bufferlength 
	 *  should initially contain the number of bytes that may be stored in buffer. If the function 
	 *  succeeds, bufferlength is set to the number of bytes stored in buffer. Note that the data 
	 *  in buffer is not NULL-terminated. If the function fails because the buffer isn't large enough, 
	 *  it returns ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL and stores the number of bytes needed in
	 *  bufferlength.
	 */
	virtual int GetLocalHostName(BYTE *buffer,size_t *bufferlength) = 0;
	/** Returns true if the address specified by addr is one of the addresses of the transmitter. */
	virtual bool ComesFromThisTransmitter(const RTPAddress *addr) = 0;
	/** Send a packet with length len containing data	to all RTP addresses of the current destination list. */
	virtual int SendRTPData(const void *data,size_t len) = 0;	
	/** Send a packet with length len containing data to all RTCP addresses of the current destination list. */
	virtual int SendRTCPData(const void *data,size_t len) = 0;
	/** Returns the raw data of a received RTP packet (received during the Poll function) 
	 *  in an RTPRawPacket instance. */
	virtual RTPRawPacket *GetNextPacket() = 0;
#ifdef RTPDEBUG
	virtual void Dump() = 0;
#endif // RTPDEBUG
	};

#define RTPUDPV4TRANS_HEADERSIZE						(20+8)
class RTPTransmissionInfo;

#define RTPSOURCES_HASHSIZE	8317
class RTPSources_GetHashIndex {
public:
	static int GetIndex(const DWORD &ssrc)				{ return ssrc % RTPSOURCES_HASHSIZE; }
	};

//template<class Element,int GetIndex(const Element &k),int hashsize>
template<class Element,class GetIndex,int hashsize>
class RTPHashTable {
public:
	RTPHashTable();
	~RTPHashTable()						{ Clear(); }

	void GotoFirstElement()					{ curhashelem = firsthashelem; }
	void GotoLastElement()					{ curhashelem = lasthashelem; }
	bool HasCurrentElement()				{ return (curhashelem == 0) ? false : true; }
	int DeleteCurrentElement();
	Element &GetCurrentElement()				{ return curhashelem->GetElement(); }
	int GotoElement(const Element &e);
	bool HasElement(const Element &e);
	void GotoNextElement();
	void GotoPreviousElement();
	void Clear();

	int AddElement(const Element &elem);
	int DeleteElement(const Element &elem);

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	class HashElement {
	public:
		HashElement(const Element &e,int index):element(e) { hashprev=NULL; hashnext=NULL; listnext=NULL; listprev=NULL; hashindex = index; }
		int GetHashIndex() 						{ return hashindex; }
		Element &GetElement()						{ return element; }
#ifdef RTPDEBUG
		void Dump()							{ std::cout << "\tHash index " << hashindex << " | Element " << element << std::endl; }
#endif // RTPDEBUG
	private:
		int hashindex;
		Element element;
	public:
		HashElement *hashprev,*hashnext;
		HashElement *listprev,*listnext;
		};

	HashElement *table[hashsize];
	HashElement *firsthashelem,*lasthashelem;
	HashElement *curhashelem;
#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
	int memorytype;
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
	};

template<class Element,class GetIndex,int hashsize>
inline RTPHashTable<Element,GetIndex,hashsize>::RTPHashTable() {

	for(int i=0; i < hashsize; i++)
		table[i]=NULL;
	firsthashelem=NULL;
	lasthashelem=NULL;
#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
	memorytype = memtype;
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
	}

template<class Element,class GetIndex,int hashsize>
inline int RTPHashTable<Element,GetIndex,hashsize>::DeleteCurrentElement() {
	if(curhashelem) {
		HashElement *tmp1,*tmp2;
		int index;
		
		// First, relink elements in current hash bucket
		
		index = curhashelem->GetHashIndex();
		tmp1 = curhashelem->hashprev;
		tmp2 = curhashelem->hashnext;
		if(tmp1 == 0) // no previous element in hash bucket
		{
			table[index] = tmp2;
			if(tmp2 != 0)
				tmp2->hashprev = 0;
			}
		else // there is a previous element in the hash bucket
		{
			tmp1->hashnext = tmp2;
			if(tmp2 != 0)
				tmp2->hashprev = tmp1;
			}

		// Relink elements in list
		
		tmp1 = curhashelem->listprev;
		tmp2 = curhashelem->listnext;
		if(tmp1 == 0) // curhashelem is first in list
		{
			firsthashelem = tmp2;
			if(tmp2 != 0)
				tmp2->listprev = 0;
			else // curhashelem is also last in list
				lasthashelem = 0;	
			}
		else {
			tmp1->listnext = tmp2;
			if(tmp2 != 0)
				tmp2->listprev = tmp1;
			else // curhashelem is last in list
				lasthashelem = tmp1;
			}
		
		// finally, with everything being relinked, we can delete curhashelem
		delete curhashelem;
		curhashelem = tmp2; // Set to next element in the list
		}
	else
		return ERR_RTP_HASHTABLE_NOCURRENTELEMENT;

	return 0;
	}

template<class Element,class GetIndex,int hashsize>
inline int RTPHashTable<Element,GetIndex,hashsize>::GotoElement(const Element &e) {
	int index;
	bool found;
	
	index = GetIndex::GetIndex(e);
	if(index >= hashsize)
		return ERR_RTP_HASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX;
	
	curhashelem = table[index]; 
	found = false;
	while(!found && curhashelem)	{
		if(curhashelem->GetElement() == e)
			found = true;
		else
			curhashelem = curhashelem->hashnext;
		}
	if(!found)
		return ERR_RTP_HASHTABLE_ELEMENTNOTFOUND;
	return 0;
	}

template<class Element,class GetIndex,int hashsize>
inline bool RTPHashTable<Element,GetIndex,hashsize>::HasElement(const Element &e) {
	int index;
	bool found;
	HashElement *tmp;
	
	index = GetIndex::GetIndex(e);
	if(index >= hashsize)
		return false;
	
	tmp = table[index]; 
	found = false;
	while(!found && tmp)	{
		if(tmp->GetElement() == e)
			found = true;
		else
			tmp = tmp->hashnext;
		}
	return found;
	}

template<class Element,class GetIndex,int hashsize>
inline void RTPHashTable<Element,GetIndex,hashsize>::GotoNextElement() {
	if(curhashelem)
		curhashelem = curhashelem->listnext;
}

template<class Element,class GetIndex,int hashsize>
inline void RTPHashTable<Element,GetIndex,hashsize>::GotoPreviousElement() {
	if(curhashelem)
		curhashelem = curhashelem->listprev;
	}

template<class Element,class GetIndex,int hashsize>
inline void RTPHashTable<Element,GetIndex,hashsize>::Clear() {
	HashElement *tmp1,*tmp2;
	
	for(int i=0 ; i < hashsize ; i++)
		table[i]=NULL;
	
	tmp1 = firsthashelem;
	while(tmp1)	{
		tmp2 = tmp1->listnext;
		delete tmp1;
		tmp1 = tmp2;
		}
	firsthashelem=NULL;
	lasthashelem=NULL;
	}

template<class Element,class GetIndex,int hashsize>
inline int RTPHashTable<Element,GetIndex,hashsize>::AddElement(const Element &elem) {
	int index;
	bool found;
	HashElement *e,*newelem;
	
	index = GetIndex::GetIndex(elem);
	if(index >= hashsize)
		return ERR_RTP_HASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX;
	
	e = table[index];
	found = false;
	while(!found && e)	{
		if(e->GetElement() == elem)
			found = true;
		else
			e = e->hashnext;
		}
	if(found)
		return ERR_RTP_HASHTABLE_ELEMENTALREADYEXISTS;
	
	// Okay, the key doesn't exist, so we can add the new element in the hash table
	
	newelem = new HashElement(elem,index);
	if(!newelem)
		return ERR_RTP_OUTOFMEM;

	e = table[index];
	table[index] = newelem;
	newelem->hashnext = e;
	if(e)
		e->hashprev = newelem;
	
	// Now, we still got to add it to the linked list
	
	if(!firsthashelem)	{
		firsthashelem = newelem;
		lasthashelem = newelem;
		}
	else // there already are some elements in the list
	{
		lasthashelem->listnext = newelem;
		newelem->listprev = lasthashelem;
		lasthashelem = newelem;
		}
	return 0;
	}

template<class Element,class GetIndex,int hashsize>
inline int RTPHashTable<Element,GetIndex,hashsize>::DeleteElement(const Element &elem) {
	int status;

	status = GotoElement(elem);
	if(status < 0)
		return status;
	return DeleteCurrentElement();
	}

#ifdef RTPDEBUG
template<class Element,class GetIndex,int hashsize>
inline void RTPHashTable<Element,GetIndex,hashsize>::Dump() {
	HashElement *e;
	
	std::cout << "DUMPING TABLE CONTENTS:" << std::endl;
	for(int i=0; i < hashsize; i++)	{
		e = table[i];
		while(e)		{
			e->Dump();
			e = e->hashnext;
			}
		}
	
	std::cout << "DUMPING LIST CONTENTS:" << std::endl;
	e = firsthashelem;
	while(e)	{
		e->Dump();
		e = e->listnext;
		}
	}
#endif // RTPDEBUG

template<class Key,class Element,class GetIndex,int hashsize>
class RTPKeyHashTable {
public:
	RTPKeyHashTable();
	~RTPKeyHashTable()					{ Clear(); }

	void GotoFirstElement()					{ curhashelem = firsthashelem; }
	void GotoLastElement()					{ curhashelem = lasthashelem; }
	bool HasCurrentElement()				{ return (curhashelem == NULL) ? false : true; }
	int DeleteCurrentElement();
	Element &GetCurrentElement()				{ return curhashelem->GetElement(); }
	Key &GetCurrentKey()					{ return curhashelem->GetKey(); }
	int GotoElement(const Key &k);
	bool HasElement(const Key &k);
	void GotoNextElement();
	void GotoPreviousElement();
	void Clear();

	int AddElement(const Key &k,const Element &elem);
	int DeleteElement(const Key &k);

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	class HashElement	{
	public:
		HashElement(const Key &k,const Element &e,int index):key(k),element(e) { hashprev=NULL; hashnext=NULL; listnext=NULL; listprev=NULL; hashindex = index; }
		int GetHashIndex() 						{ return hashindex; }
		Key &GetKey()							{ return key; }
		Element &GetElement()						{ return element; }
#ifdef RTPDEBUG
		void Dump()							{ std::cout << "\tHash index " << hashindex << " | Key " << key << " | Element " << element << std::endl; }
#endif // RTPDEBUG
	private:
		int hashindex;
		Key key;
		Element element;
	public:
		HashElement *hashprev,*hashnext;
		HashElement *listprev,*listnext;
		};

	HashElement *table[hashsize];
	HashElement *firsthashelem,*lasthashelem;
	HashElement *curhashelem;
#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
	int memorytype;
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
	};

template<class Key,class Element,class GetIndex,int hashsize>
inline RTPKeyHashTable<Key,Element,GetIndex,hashsize>::RTPKeyHashTable() {

	for(int i=0; i < hashsize ; i++)
		table[i]=NULL;
	firsthashelem=NULL;
	lasthashelem=NULL;
#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
	memorytype = memtype;
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
	}

template<class Key,class Element,class GetIndex,int hashsize>
inline int RTPKeyHashTable<Key,Element,GetIndex,hashsize>::DeleteCurrentElement() {

	if(curhashelem)	{
		HashElement *tmp1,*tmp2;
		int index;
		
		// First, relink elements in current hash bucket
		
		index = curhashelem->GetHashIndex();
		tmp1 = curhashelem->hashprev;
		tmp2 = curhashelem->hashnext;
		if(tmp1) // no previous element in hash bucket
		{
			table[index] = tmp2;
			if(tmp2)
				tmp2->hashprev=NULL;
			}
		else // there is a previous element in the hash bucket
		{
			tmp1->hashnext = tmp2;
			if(tmp2)
				tmp2->hashprev = tmp1;
			}

		// Relink elements in list
		
		tmp1 = curhashelem->listprev;
		tmp2 = curhashelem->listnext;
		if(!tmp1) // curhashelem is first in list
		{
			firsthashelem = tmp2;
			if(tmp2)
				tmp2->listprev=NULL;
			else // curhashelem is also last in list
				lasthashelem=NULL;	
			}
		else {
			tmp1->listnext = tmp2;
			if(tmp2)
				tmp2->listprev = tmp1;
			else // curhashelem is last in list
				lasthashelem = tmp1;
			}
		
		// finally, with everything being relinked, we can delete curhashelem
		delete curhashelem;
		curhashelem = tmp2; // Set to next element in list
		}
	else
		return ERR_RTP_KEYHASHTABLE_NOCURRENTELEMENT;

	return 0;
	}
	
template<class Key,class Element,class GetIndex,int hashsize>
inline int RTPKeyHashTable<Key,Element,GetIndex,hashsize>::GotoElement(const Key &k) {
	int index;
	bool found;
	
	index = GetIndex::GetIndex(k);
	if(index >= hashsize)
		return ERR_RTP_KEYHASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX;
	
	curhashelem = table[index]; 
	found = false;
	while(!found && curhashelem)	{
		if(curhashelem->GetKey() == k)
			found = true;
		else
			curhashelem = curhashelem->hashnext;
		}
	if(!found)
		return ERR_RTP_KEYHASHTABLE_KEYNOTFOUND;
	return 0;
	}

template<class Key,class Element,class GetIndex,int hashsize>
inline bool RTPKeyHashTable<Key,Element,GetIndex,hashsize>::HasElement(const Key &k) {
	int index;
	bool found;
	HashElement *tmp;
	
	index = GetIndex::GetIndex(k);
	if(index >= hashsize)
		return false;
	
	tmp = table[index]; 
	found = false;
	while(!found && tmp)	{
		if(tmp->GetKey() == k)
			found = true;
		else
			tmp = tmp->hashnext;
		}
	return found;
	}

template<class Key,class Element,class GetIndex,int hashsize>
inline void RTPKeyHashTable<Key,Element,GetIndex,hashsize>::GotoNextElement() {
	if(curhashelem)
		curhashelem = curhashelem->listnext;
	}

template<class Key,class Element,class GetIndex,int hashsize>
inline void RTPKeyHashTable<Key,Element,GetIndex,hashsize>::GotoPreviousElement() {
	if(curhashelem)
		curhashelem = curhashelem->listprev;
	}

template<class Key,class Element,class GetIndex,int hashsize>
inline void RTPKeyHashTable<Key,Element,GetIndex,hashsize>::Clear() {
	HashElement *tmp1,*tmp2;
	
	for(int i=0 ; i < hashsize; i++)
		table[i]=NULL;
	
	tmp1 = firsthashelem;
	while (tmp1)	{
		tmp2 = tmp1->listnext;
		delete tmp1;
		tmp1 = tmp2;
		}
	firsthashelem=NULL;
	lasthashelem=NULL;
	}

template<class Key,class Element,class GetIndex,int hashsize>
inline int RTPKeyHashTable<Key,Element,GetIndex,hashsize>::AddElement(const Key &k,const Element &elem) {
	int index;
	bool found;
	HashElement *e,*newelem;
	
	index = GetIndex::GetIndex(k);
	if(index >= hashsize)
		return ERR_RTP_KEYHASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX;
	
	e = table[index];
	found = false;
	while(!found && e)	{
		if(e->GetKey() == k)
			found = true;
		else
			e = e->hashnext;
		}
	if(found)
		return ERR_RTP_KEYHASHTABLE_KEYALREADYEXISTS;
	
	// Okay, the key doesn't exist, so we can add the new element in the hash table
	
	newelem = new HashElement(k,elem,index);
	if(!newelem)
		return ERR_RTP_OUTOFMEM;

	e = table[index];
	table[index] = newelem;
	newelem->hashnext = e;
	if(e)
		e->hashprev = newelem;
	
	// Now, we still got to add it to the linked list
	
	if(!firsthashelem)	{
		firsthashelem = newelem;
		lasthashelem = newelem;
		}
	else // there already are some elements in the list
	{
		lasthashelem->listnext = newelem;
		newelem->listprev = lasthashelem;
		lasthashelem = newelem;
		}
	return 0;
	}

template<class Key,class Element,class GetIndex,int hashsize>
inline int RTPKeyHashTable<Key,Element,GetIndex,hashsize>::DeleteElement(const Key &k) {
	int status;

	status = GotoElement(k);
	if(status < 0)
		return status;
	return DeleteCurrentElement();
	}

#ifdef RTPDEBUG
template<class Key,class Element,class GetIndex,int hashsize>
inline void RTPKeyHashTable<Key,Element,GetIndex,hashsize>::Dump() {
	HashElement *e;
	
	std::cout << "DUMPING TABLE CONTENTS:" << std::endl;
	for(int i=0 ; i < hashsize ; i++)	{
		e = table[i];
		while (e )		{
			e->Dump();
			e = e->hashnext;
			}
		}
	
	std::cout << "DUMPING LIST CONTENTS:" << std::endl;
	e = firsthashelem;
	while(e)	{
		e->Dump();
		e = e->listnext;
		}
	}
#endif // RTPDEBUG

/** Represents an IPv4 IP address and port.
 *  This class is used by the UDP over IPv4 transmission component.
 *  When an RTPIPv4Address is used in one of the multicast functions of the transmitter, the port 
 *  number is ignored. When an instance is used in one of the accept or ignore functions of the 
 *  transmitter, a zero port number represents all ports for the specified IP address.
 */
class RTPIPv4Address : public RTPAddress {
public:
	/** Creates an instance with IP address ip and port number port (both 
	 *  are interpreted in host byte order), and possibly sets the RTCP multiplex flag
	 *  (see RTPIPv4Address::UseRTCPMultiplexingOnTransmission). */
	RTPIPv4Address(DWORD ip = 0, WORD port = 0,bool rtcpmux = false) : RTPAddress(IPv4Address)	{ 
		RTPIPv4Address::ip = ip; 
		RTPIPv4Address::port = port;
		if(rtcpmux)
			rtcpsendport = port;
		else
			rtcpsendport = port+1;
		}

	/** Creates an instance with IP address ip and port number port (both 
	 *  are interpreted in host byte order), and sets a specific port to
	 *  send RTCP packets to (see RTPIPv4Address::GetRTCPSendPort). */
	RTPIPv4Address(DWORD ip, WORD port, WORD rtcpsendport):RTPAddress(IPv4Address)	{
		RTPIPv4Address::ip = ip; 
		RTPIPv4Address::port = port; 
		RTPIPv4Address::rtcpsendport = rtcpsendport; 
		}

	/** Creates an instance with IP address ip and port number port (port is 
	 *  interpreted in host byte order) and possibly sets the RTCP multiplex flag
	 *  (see RTPIPv4Address::UseRTCPMultiplexingOnTransmission). */
	RTPIPv4Address(const BYTE ip[4],WORD port = 0,bool rtcpmux = false) : RTPAddress(IPv4Address) {
		RTPIPv4Address::ip = (DWORD)ip[3]; 
		RTPIPv4Address::ip |= (((DWORD)ip[2])<<8); 
		RTPIPv4Address::ip |= (((DWORD)ip[1])<<16); 
		RTPIPv4Address::ip |= (((DWORD)ip[0])<<24); 
		
		RTPIPv4Address::port = port; 
		if(rtcpmux)
			rtcpsendport = port;
		else
			rtcpsendport = port+1;
		}

	/** Creates an instance with IP address ip and port number port (both 
	 *  are interpreted in host byte order), and sets a specific port to
	 *  send RTCP packets to (see RTPIPv4Address::GetRTCPSendPort). */
	RTPIPv4Address(const BYTE ip[4],WORD port,WORD rtcpsendport):RTPAddress(IPv4Address) {
		RTPIPv4Address::ip = (DWORD)ip[3]; 
		RTPIPv4Address::ip |= (((DWORD)ip[2])<<8); 
		RTPIPv4Address::ip |= (((DWORD)ip[1])<<16); 
		RTPIPv4Address::ip |= (((DWORD)ip[0])<<24); 
		
		RTPIPv4Address::port = port; 
		RTPIPv4Address::rtcpsendport = rtcpsendport;
		}

	~RTPIPv4Address()																				{ }

	/** Sets the IP address for this instance to ip which is assumed to be in host byte order. */
	void SetIP(DWORD ip)																			{ RTPIPv4Address::ip = ip; }

	/** Sets the IP address of this instance to ip. */
	void SetIP(const BYTE ip[4])																	{ RTPIPv4Address::ip = (DWORD)ip[3]; RTPIPv4Address::ip |= (((DWORD)ip[2])<<8); RTPIPv4Address::ip |= (((DWORD)ip[1])<<16); RTPIPv4Address::ip |= (((DWORD)ip[0])<<24); }

	/** Sets the port number for this instance to port which is interpreted in host byte order. */
	void SetPort(WORD port)																		{ RTPIPv4Address::port = port; }

	/** Returns the IP address contained in this instance in host byte order. */
	DWORD GetIP() const																			{ return ip; }

	/** Returns the port number of this instance in host byte order. */
	WORD GetPort() const																		{ return port; }

	/** For outgoing packets, this indicates to which port RTCP packets will be sent (can, 
	 *  be the same port as the RTP packets in case RTCP multiplexing is used). */
	WORD GetRTCPSendPort() const																{ return rtcpsendport; }

	RTPAddress *CreateCopy() const;

	// Note that these functions are only used for received packets, and for those
	// the rtcpsendport variable is not important and should be ignored.
	bool IsSameAddress(const RTPAddress *addr) const;
	bool IsFromSameHost(const RTPAddress *addr) const;
#ifdef RTPDEBUG
	std::string GetAddressString() const;
#endif // RTPDEBUG
private:
	DWORD ip;
	WORD port;
	WORD rtcpsendport;
	};

class RTPIPv4Destination {
public:
	RTPIPv4Destination()	{
		ip = 0;
		memset(&rtpaddr,0,sizeof(struct sockaddr_in));
		memset(&rtcpaddr,0,sizeof(struct sockaddr_in));
		}

	RTPIPv4Destination(DWORD ip,WORD rtpport,WORD rtcpport)	{
		memset(&rtpaddr,0,sizeof(struct sockaddr_in));
		memset(&rtcpaddr,0,sizeof(struct sockaddr_in));
		
		rtpaddr.sin_family = AF_INET;
		rtpaddr.sin_port = htons(rtpport);
		rtpaddr.sin_addr.s_addr = htonl(ip);
		
		rtcpaddr.sin_family = AF_INET;
		rtcpaddr.sin_port = htons(rtcpport);
		rtcpaddr.sin_addr.s_addr = htonl(ip);

		RTPIPv4Destination::ip = ip;
		}

	bool operator==(const RTPIPv4Destination &src) const	{ 
		if(rtpaddr.sin_addr.s_addr == src.rtpaddr.sin_addr.s_addr && rtpaddr.sin_port == src.rtpaddr.sin_port) 
			return true; 
		return false; 
		}
	DWORD GetIP() const									{ return ip; }
	// nbo = network byte order
	DWORD GetIP_NBO() const								{ return rtpaddr.sin_addr.s_addr; }
	WORD GetRTPPort_NBO() const								{ return rtpaddr.sin_port; }
	WORD GetRTCPPort_NBO() const							{ return rtcpaddr.sin_port; }
	const struct sockaddr_in *GetRTPSockAddr() const					{ return &rtpaddr; }
	const struct sockaddr_in *GetRTCPSockAddr() const					{ return &rtcpaddr; }
	std::string GetDestinationString() const;

	static bool AddressToDestination(const RTPAddress &addr, RTPIPv4Destination &dest)	{
		if(addr.GetAddressType() != RTPAddress::IPv4Address)
			return false;

		const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
		WORD rtpport = address.GetPort();
		WORD rtcpport = address.GetRTCPSendPort();

		dest = RTPIPv4Destination(address.GetIP(),rtpport,rtcpport);
		return true;
		}

private:
	DWORD ip;
	struct sockaddr_in rtpaddr;
	struct sockaddr_in rtcpaddr;
	};



class RTCPCompoundPacket;

/** Base class for specific types of RTCP packets. */
class RTCPPacket {
public:
	/** Identifies the specific kind of RTCP packet. */
	enum PacketType { 
			SR,		/**< An RTCP sender report. */
			RR,		/**< An RTCP receiver report. */
			SDES,	/**< An RTCP source description packet. */
			BYE,	/**< An RTCP bye packet. */
			APP,	/**< An RTCP packet containing application specific data. */
			Unknown	/**< The type of RTCP packet was not recognized. */
	};
protected:
	RTCPPacket(PacketType t,BYTE *d,size_t dlen) : data(d),datalen(dlen),packettype(t) { knownformat = false; }
public:
	virtual ~RTCPPacket()								{ }	

	/** Returns true if the subclass was able to interpret the data and false otherwise. */
	bool IsKnownFormat() const							{ return knownformat; }
	/** Returns the actual packet type which the subclass implements. */
	PacketType GetPacketType() const					{ return packettype; }
	/** Returns a pointer to the data of this RTCP packet. */
	BYTE *GetPacketData()							{ return data; }
	/** Returns the length of this RTCP packet. */
	size_t GetPacketLength() const						{ return datalen; }

#ifdef RTPDEBUG
	virtual void Dump();
#endif // RTPDEBUG
protected:
	BYTE *data;
	size_t datalen;
	bool knownformat;
private:
	const PacketType packettype;
	};

/** Describes an RTCP source description packet. */
class RTCPSDESPacket : public RTCPPacket {
public:
	/** Identifies the type of an SDES item. */
	enum ItemType { 
		None,	/**< Used when the iteration over the items has finished. */
		CNAME,	/**< Used for a CNAME (canonical name) item. */
		NAME,	/**< Used for a NAME item. */
		EMAIL,	/**< Used for an EMAIL item. */
		PHONE,	/**< Used for a PHONE item. */
		LOC,	/**< Used for a LOC (location) item. */
		TOOL,	/**< Used for a TOOL item. */
		NOTE,	/**< Used for a NOTE item. */
		PRIV,	/**< Used for a PRIV item. */
		Unknown /**< Used when there is an item present, but the type is not recognized. */
		};
	
	/** Creates an instance based on the data in data with length datalen.
	 *  Creates an instance based on the data in data with length datalen. Since the data pointer
	 *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it 
	 *  points to is valid as long as the class instance exists.
	 */
	RTCPSDESPacket(BYTE *data,size_t datalen);
	~RTCPSDESPacket()							{ }
	/** Returns the number of SDES chunks in the SDES packet.
	 *  Returns the number of SDES chunks in the SDES packet. Each chunk has its own SSRC identifier. 
	 */
	int GetChunkCount() const;
	/** Starts the iteration over the chunks.
	 *  Starts the iteration. If no SDES chunks are present, the function returns false. Otherwise,
	 *  it returns true and sets the current chunk to be the first chunk.
	 */
	bool GotoFirstChunk();
	/** Sets the current chunk to the next available chunk.
	 *  Sets the current chunk to the next available chunk. If no next chunk is present, this function returns
	 *  false, otherwise it returns true.
	 */
	bool GotoNextChunk();
	/** Returns the SSRC identifier of the current chunk. */
	DWORD GetChunkSSRC() const;
	/** Starts the iteration over the SDES items in the current chunk.
	 *  Starts the iteration over the SDES items in the current chunk. If no SDES items are 
	 *  present, the function returns false. Otherwise, the function sets the current item
	 *  to be the first one and returns true.
	 */
	bool GotoFirstItem();
	/** Advances the iteration to the next item in the current chunk. 
	 *  If there's another item in the chunk, the current item is set to be the next one and the function
	 *  returns true. Otherwise, the function returns false.
	 */
	bool GotoNextItem();
	/** Returns the SDES item type of the current item in the current chunk. */
	ItemType GetItemType() const;
	/** Returns the item length of the current item in the current chunk. */
	size_t GetItemLength() const;
	/** Returns the item data of the current item in the current chunk. */
	BYTE *GetItemData();

#ifdef RTP_SUPPORT_SDESPRIV
	/** If the current item is an SDES PRIV item, this function returns the length of the 
	 *  prefix string of the private item. 
	 */
	size_t GetPRIVPrefixLength() const;
	/** If the current item is an SDES PRIV item, this function returns actual data of the
	 *  prefix string.
	 */
	BYTE *GetPRIVPrefixData();
	/** If the current item is an SDES PRIV item, this function returns the length of the
	 *  value string of the private item.
	 */
	size_t GetPRIVValueLength() const;
	/** If the current item is an SDES PRIV item, this function returns actual value data of the
	 *  private item.
	 */
	BYTE *GetPRIVValueData();
#endif // RTP_SUPPORT_SDESPRIV

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	BYTE *currentchunk;
	int curchunknum;
	size_t itemoffset;
};

#pragma pack( push, before_rtcph2 )
#pragma pack(1)
struct RTCPCommonHeader {
#ifdef RTP_BIG_ENDIAN
	BYTE version:2;
	BYTE padding:1;
	BYTE count:5;
#else // little endian
	BYTE count:5;
	BYTE padding:1;
	BYTE version:2;
#endif // RTP_BIG_ENDIAN

	BYTE packettype;
	WORD length;
};

struct RTCPSenderReport {
	DWORD ntptime_msw;
	DWORD ntptime_lsw;
	DWORD rtptimestamp;
	DWORD packetcount;
	DWORD octetcount;
};

struct RTCPReceiverReport {
	DWORD ssrc; // Identifies about which SSRC's data this report is...
	BYTE fractionlost;
	BYTE packetslost[3];
	DWORD exthighseqnr;
	DWORD jitter;
	DWORD lsr;
	DWORD dlsr;
};

struct RTCPSDESHeader {
	BYTE sdesid;
	BYTE length;
};
#pragma pack( pop, before_rtcph2 )

/** Describes an RTCP BYE packet. */
class RTCPBYEPacket : public RTCPPacket {
public:
	/** Creates an instance based on the data in data with length datalen. 
	 *  Creates an instance based on the data in data with length datalen. Since the data pointer
	 *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it 
	 *  points to is valid as long as the class instance exists.
	 */
	RTCPBYEPacket(BYTE *data,size_t datalen);
	~RTCPBYEPacket()							{ }
	
	/** Returns the number of SSRC identifiers present in this BYE packet. */
	int GetSSRCCount() const;

	/** Returns the SSRC described by index which may have a value from 0 to GetSSRCCount()-1 
	 *  (note that no check is performed to see if index is valid).
	 */
	DWORD GetSSRC(int index) const; // note: no check is performed to see if index is valid!

	/** Returns true if the BYE packet contains a reason for leaving. */
	bool HasReasonForLeaving() const;

	/** Returns the length of the string which describes why the source(s) left. */
	size_t GetReasonLength() const;

	/** Returns the actual reason for leaving data. */
	BYTE *GetReasonData();

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	size_t reasonoffset;
	};
inline int RTCPBYEPacket::GetSSRCCount() const {

	if(!knownformat)
		return 0;

	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return (int)(hdr->count);
	}
inline DWORD RTCPBYEPacket::GetSSRC(int index) const {

	if(!knownformat)
		return 0;
	DWORD *ssrc = (DWORD *)(data+sizeof(RTCPCommonHeader)+sizeof(DWORD)*index);
	return ntohl(*ssrc);
	}
inline bool RTCPBYEPacket::HasReasonForLeaving() const {

	if(!knownformat)
		return false;
	if(reasonoffset == 0)
		return false;
	return true;
	}
inline size_t RTCPBYEPacket::GetReasonLength() const {

	if(!knownformat)
		return 0;
	if(reasonoffset == 0)
		return 0;
	BYTE *reasonlen = (data+reasonoffset);
	return (size_t)(*reasonlen);
	}
inline BYTE *RTCPBYEPacket::GetReasonData() {

	if(!knownformat)
		return 0;
	if(reasonoffset == 0)
		return 0;
	BYTE *reasonlen = (data+reasonoffset);
	if((*reasonlen) == 0)
		return 0;
	return (data+reasonoffset+1);	
	}
inline int RTCPSDESPacket::GetChunkCount() const {

	if(!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return ((int)hdr->count);
	}
inline bool RTCPSDESPacket::GotoFirstChunk() {

	if(GetChunkCount() == 0)	{
		currentchunk = 0;
		return false;
		}
	currentchunk = data+sizeof(RTCPCommonHeader);
	curchunknum = 1;
	itemoffset = sizeof(DWORD);
	return true;
	}
inline bool RTCPSDESPacket::GotoNextChunk() {

	if(!knownformat)
		return false;
	if(currentchunk == 0)
		return false;
	if(curchunknum == GetChunkCount())
		return false;
	
	size_t offset = sizeof(DWORD);
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+sizeof(DWORD));
	
	while(sdeshdr->sdesid != 0)	{
		offset += sizeof(RTCPSDESHeader);
		offset += (size_t)(sdeshdr->length);
		sdeshdr = (RTCPSDESHeader *)(currentchunk+offset);
		}
	offset++; // for the zero byte
	if((offset & 0x03) != 0)
		offset += (4-(offset&0x03));
	currentchunk += offset;
	curchunknum++;
	itemoffset = sizeof(DWORD);
	return true;
	}
inline DWORD RTCPSDESPacket::GetChunkSSRC() const {

	if(!knownformat)
		return 0;
	if(currentchunk == 0)
		return 0;
	DWORD *ssrc = (DWORD *)currentchunk;
	return ntohl(*ssrc);
	}
inline bool RTCPSDESPacket::GotoFirstItem() {

	if(!knownformat)
		return false;
	if(currentchunk == 0)
		return false;
	itemoffset = sizeof(DWORD);
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if(sdeshdr->sdesid == 0)
		return false;
	return true;
	}
inline bool RTCPSDESPacket::GotoNextItem() {

	if(!knownformat)
		return false;
	if(currentchunk == 0)
		return false;
	
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if(sdeshdr->sdesid == 0)
		return false;
	
	size_t offset = itemoffset;
	offset += sizeof(RTCPSDESHeader);
	offset += (size_t)(sdeshdr->length);
	sdeshdr = (RTCPSDESHeader *)(currentchunk+offset);
	if(sdeshdr->sdesid == 0)
		return false;
	itemoffset = offset;
	return true;
	}
inline RTCPSDESPacket::ItemType RTCPSDESPacket::GetItemType() const {

	if(!knownformat)
		return None;
	if(currentchunk == 0)
		return None;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	switch (sdeshdr->sdesid)	{
		case 0:
			return None;
		case RTCP_SDES_ID_CNAME:
			return CNAME;
		case RTCP_SDES_ID_NAME:
			return NAME;
		case RTCP_SDES_ID_EMAIL:
			return EMAIL;
		case RTCP_SDES_ID_PHONE:
			return PHONE;
		case RTCP_SDES_ID_LOCATION:
			return LOC;
		case RTCP_SDES_ID_TOOL:
			return TOOL;
		case RTCP_SDES_ID_NOTE:
			return NOTE;
		case RTCP_SDES_ID_PRIVATE:
			return PRIV;
		default:
			return Unknown;
		}
	return Unknown;
	}
inline size_t RTCPSDESPacket::GetItemLength() const {

	if(!knownformat)
		return None;
	if(currentchunk == 0)
		return None;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if(sdeshdr->sdesid == 0)
		return 0;
	return (size_t)(sdeshdr->length);
	}
inline BYTE *RTCPSDESPacket::GetItemData() {

	if(!knownformat)
		return 0;
	if(currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if(sdeshdr->sdesid == 0)
		return 0;
	return (currentchunk+itemoffset+sizeof(RTCPSDESHeader));
	}

#ifdef RTP_SUPPORT_SDESPRIV
inline size_t RTCPSDESPacket::GetPRIVPrefixLength() const {

	if(!knownformat)
		return 0;
	if(currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if(sdeshdr->sdesid != RTCP_SDES_ID_PRIVATE)
		return 0;
	if(sdeshdr->length == 0)
		return 0;
	BYTE *preflen = currentchunk+itemoffset+sizeof(RTCPSDESHeader);
	size_t prefixlength = (size_t)(*preflen);
	if(prefixlength > (size_t)((sdeshdr->length)-1))
		return 0;
	return prefixlength;
	}
inline BYTE *RTCPSDESPacket::GetPRIVPrefixData() {

	if(!knownformat)
		return 0;
	if(currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if(sdeshdr->sdesid != RTCP_SDES_ID_PRIVATE)
		return 0;
	if(sdeshdr->length == 0)
		return 0;
	BYTE *preflen = currentchunk+itemoffset+sizeof(RTCPSDESHeader);
	size_t prefixlength = (size_t)(*preflen);
	if(prefixlength > (size_t)((sdeshdr->length)-1))
		return 0;
	if(prefixlength == 0)
		return 0;
	return (currentchunk+itemoffset+sizeof(RTCPSDESHeader)+1);
	}
inline size_t RTCPSDESPacket::GetPRIVValueLength() const {

	if(!knownformat)
		return 0;
	if(currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if(sdeshdr->sdesid != RTCP_SDES_ID_PRIVATE)
		return 0;
	if(sdeshdr->length == 0)
		return 0;
	BYTE *preflen = currentchunk+itemoffset+sizeof(RTCPSDESHeader);
	size_t prefixlength = (size_t)(*preflen);
	if(prefixlength > (size_t)((sdeshdr->length)-1))
		return 0;
	return ((size_t)(sdeshdr->length))-prefixlength-1;
	}
inline BYTE *RTCPSDESPacket::GetPRIVValueData() {

	if(!knownformat)
		return 0;
	if(currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if(sdeshdr->sdesid != RTCP_SDES_ID_PRIVATE)
		return 0;
	if(sdeshdr->length == 0)
		return 0;
	BYTE *preflen = currentchunk+itemoffset+sizeof(RTCPSDESHeader);
	size_t prefixlength = (size_t)(*preflen);
	if(prefixlength > (size_t)((sdeshdr->length)-1))
		return 0;
	size_t valuelen = ((size_t)(sdeshdr->length))-prefixlength-1;
	if(valuelen == 0)
		return 0;
	return (currentchunk+itemoffset+sizeof(RTCPSDESHeader)+1+prefixlength);
	}

#endif // RTP_SUPPORT_SDESPRIV


/** An UDP over IPv4 transmission component.
 *  This class inherits the RTPTransmitter interface and implements a transmission component 
 *  which uses UDP over IPv4 to send and receive RTP and RTCP data. The component's parameters 
 *  are described by the class RTPUDPv4TransmissionParams. The functions which have an RTPAddress 
 *  argument require an argument of RTPIPv4Address. The GetTransmissionInfo member function
 *  returns an instance of type RTPUDPv4TransmissionInfo.
 */
typedef int RTPSOCKLENTYPE;
#define RTPUDPV4TRANS_MAXPACKSIZE 65535
#define RTPUDPV4TRANS_IFREQBUFSIZE	8192
class RTPUDPv4Trans_GetHashIndex_IPv4Dest {
public:
	static int GetIndex(const RTPIPv4Destination &d)	{ return d.GetIP()%RTPUDPV4TRANS_HASHSIZE; }
	};
class RTPUDPv4Trans_GetHashIndex_DWORD {
public:
	static int GetIndex(const DWORD &k)			{ return k % RTPUDPV4TRANS_HASHSIZE; }
	};

class RTPUDPv4Transmitter : public RTPTransmitter {
public:
	RTPUDPv4Transmitter(/* RTPMemoryManager *mgr */);
	~RTPUDPv4Transmitter();

	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();
	void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

	int GetLocalHostName(BYTE *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()							{ return RTPUDPV4TRANS_HEADERSIZE; }
	
	int Poll();
	int WaitForIncomingData(const RTPTime &delay,bool *dataavailable = 0);
	int AbortWait();
	
	int SendRTPData(const void *data,size_t len);	
	int SendRTCPData(const void *data,size_t len);

	int AddDestination(const RTPAddress &addr);
	int DeleteDestination(const RTPAddress &addr);
	void ClearDestinations();

	bool SupportsMulticasting();
	int JoinMulticastGroup(const RTPAddress &addr);
	int LeaveMulticastGroup(const RTPAddress &addr);
	void LeaveAllMulticastGroups();

	int SetReceiveMode(RTPTransmitter::ReceiveMode m);
	int AddToIgnoreList(const RTPAddress &addr);
	int DeleteFromIgnoreList(const RTPAddress &addr);
	void ClearIgnoreList();
	int AddToAcceptList(const RTPAddress &addr);
	int DeleteFromAcceptList(const RTPAddress &addr);
	void ClearAcceptList();
	int SetMaximumPacketSize(size_t s);	
	
	bool NewDataAvailable();
	RTPRawPacket *GetNextPacket();
#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	int CreateLocalIPList();
	bool GetLocalIPList_Interfaces();
	void GetLocalIPList_DNS();
	void AddLoopbackAddress();
	void FlushPackets();
	int PollSocket(bool rtp);
	int ProcessAddAcceptIgnoreEntry(DWORD ip,WORD port);
	int ProcessDeleteAcceptIgnoreEntry(DWORD ip,WORD port);
#ifdef RTP_SUPPORT_IPV4MULTICAST
	bool SetMulticastTTL(BYTE ttl);
#endif // RTP_SUPPORT_IPV4MULTICAST
	bool ShouldAcceptData(DWORD srcip,WORD srcport);
	void ClearAcceptIgnoreInfo();
	
	bool init;
	bool created;
	bool waitingfordata;
	CDataSocket *rtpsock,*rtcpsock;
	DWORD mcastifaceIP;
	CList<DWORD,DWORD> localIPs;
	WORD m_rtpPort, m_rtcpPort;
	BYTE multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	BYTE *localhostname;
	size_t localhostnamelength;
	
	RTPHashTable<const RTPIPv4Destination,RTPUDPv4Trans_GetHashIndex_IPv4Dest,RTPUDPV4TRANS_HASHSIZE> destinations;
#ifdef RTP_SUPPORT_IPV4MULTICAST
	RTPHashTable<const DWORD,RTPUDPv4Trans_GetHashIndex_DWORD,RTPUDPV4TRANS_HASHSIZE> multicastgroups;
#endif // RTP_SUPPORT_IPV4MULTICAST

	bool supportsmulticasting;
	size_t maxpacksize;

	class PortInfo	{
	public:
		PortInfo() { all = false; }
		
		bool all;
		CList<WORD,WORD> portlist;
		};

	RTPKeyHashTable<const DWORD,PortInfo *,RTPUDPv4Trans_GetHashIndex_DWORD,RTPUDPV4TRANS_HASHSIZE> acceptignoreinfo;

	bool closesocketswhendone;

#ifdef RTP_SUPPORT_THREAD
	jthread::JMutex mainmutex,waitmutex;
	int threadsafe;
#endif // RTP_SUPPORT_THREAD
	};

//virtual RTPTransmitter * NewUserDefinedTransmitter();

class RTPExternalTransmitter;

/** Base class to specify a mechanism to transmit RTP packets outside of this library.
 *  Base class to specify a mechanism to transmit RTP packets outside of this library. When
 *  you want to use your own mechanism to transmit RTP packets, you need to specify that
 *  you'll be using the external transmission component, and derive a class from this base
 *  class. An instance should then be specified in the RTPExternalTransmissionParams object,
 *  so that the transmitter will call the SendRTP, SendRTCP and ComesFromThisSender
 *  methods of this instance when needed.
 */
class RTPExternalSender {
public:
	RTPExternalSender()										{ }
	virtual ~RTPExternalSender()									{ }

	/** This member function will be called when RTP data needs to be transmitted. */
	virtual bool SendRTP(const void *data, size_t len) = 0;

	/** This member function will be called when an RTCP packet needs to be transmitted. */
	virtual bool SendRTCP(const void *data, size_t len) = 0;

	/** Used to identify if an RTPAddress instance originated from this sender (to be able to detect own packets). */
	virtual bool ComesFromThisSender(const RTPAddress *a) = 0;
	};

/** Interface to inject incoming RTP and RTCP packets into the library.
 *  Interface to inject incoming RTP and RTCP packets into the library. When you have your own
 *  mechanism to receive incoming RTP/RTCP data, you'll need to pass these packets to the library.
 *  By first retrieving the RTPExternalTransmissionInfo instance for the external transmitter you'll
 *  be using, you can obtain the associated RTPExternalPacketInjecter instance. By calling it's
 *  member functions, you can then inject RTP or RTCP data into the library for further processing.
 */
class RTPExternalPacketInjecter {
public:
	RTPExternalPacketInjecter(RTPExternalTransmitter *trans)					{ transmitter = trans; }
	~RTPExternalPacketInjecter()									{ }

	/** This function can be called to insert an RTP packet into the transmission component. */
	void InjectRTP(const void *data, size_t len, const RTPAddress &a);

	/** This function can be called to insert an RTCP packet into the transmission component. */
	void InjectRTCP(const void *data, size_t len, const RTPAddress &a);

	/** Use this function to inject an RTP or RTCP packet and the transmitter will try to figure out which type of packet it is. */
	void InjectRTPorRTCP(const void *data, size_t len, const RTPAddress &a);
private:
	RTPExternalTransmitter *transmitter;
};

class RTPTransmissionParams {
protected:
	RTPTransmissionParams(RTPTransmitter::TransmissionProtocol p)				{ protocol = p; }
public:
	virtual ~RTPTransmissionParams() { }

	/** Returns the transmitter type for which these parameters are valid. */
	RTPTransmitter::TransmissionProtocol GetTransmissionProtocol() const			{ return protocol; }
private:
	RTPTransmitter::TransmissionProtocol protocol;
	};
/** Parameters for the UDP over IPv4 transmitter. */
class RTPUDPv4TransmissionParams : public RTPTransmissionParams {
public:
	RTPUDPv4TransmissionParams();

	/** Sets the IP address which is used to bind the sockets to ip. */
	void SetBindIP(DWORD ip)									{ bindIP = ip; }

	/** Sets the multicast interface IP address. */
	void SetMulticastInterfaceIP(DWORD ip)					{ mcastifaceIP = ip; }

	/** Sets the RTP portbase to pbase, which has to be an even number
	 *  unless RTPUDPv4TransmissionParams::SetAllowOddPortbase was called;
	 *  a port number of zero will cause a port to be chosen automatically. */
	void SetPortbase(WORD pbase)							{ portbase = pbase; }

	/** Sets the multicast TTL to be used to mcastTTL. */
	void SetMulticastTTL(BYTE mcastTTL)						{ multicastTTL = mcastTTL; }

	/** Passes a list of IP addresses which will be used as the local IP addresses. */
	void SetLocalIPList(CList <DWORD,DWORD> &iplist)	{ for(POSITION pos = iplist.GetHeadPosition(); pos; ) { const auto& item = iplist.GetNext(pos); localIPs.AddTail(item); } } 

	/** Clears the list of local IP addresses. 
	 *  Clears the list of local IP addresses. An empty list will make the transmission 
	 *  component itself determine the local IP addresses.
	 */
	void ClearLocalIPList()										{ localIPs.RemoveAll(); }

	/** Returns the IP address which will be used to bind the sockets. */
	DWORD GetBindIP() const									{ return bindIP; }

	/** Returns the multicast interface IP address. */
	DWORD GetMulticastInterfaceIP() const					{ return mcastifaceIP; }

	/** Returns the RTP portbase which will be used (default is 5000). */
	WORD GetPortbase() const								{ return portbase; }

	/** Returns the multicast TTL which will be used (default is 1). */
	BYTE GetMulticastTTL() const								{ return multicastTTL; }

	/** Returns the list of local IP addresses. */
	const CList <DWORD,DWORD> &GetLocalIPList() const			{ return localIPs; }

	/** Sets the RTP socket's send buffer size. */
	void SetRTPSendBuffer(int s)								{ rtpsendbuf = s; }

	/** Sets the RTP socket's receive buffer size. */
	void SetRTPReceiveBuffer(int s)								{ rtprecvbuf = s; }

	/** Sets the RTCP socket's send buffer size. */
	void SetRTCPSendBuffer(int s)								{ rtcpsendbuf = s; }

	/** Sets the RTCP socket's receive buffer size. */
	void SetRTCPReceiveBuffer(int s)							{ rtcprecvbuf = s; }

	/** Enables or disables multiplexing RTCP traffic over the RTP channel, so that only a single port is used. */
	void SetRTCPMultiplexing(bool f)							{ rtcpmux = f; }

	/** Can be used to allow the RTP port base to be any number, not just even numbers. */
	void SetAllowOddPortbase(bool f)							{ allowoddportbase = f; }

	/** Force the RTCP socket to use a specific port, not necessarily one more than
	 *  the RTP port (set this to zero to disable). */
	void SetForcedRTCPPort(WORD rtcpport)					{ forcedrtcpport = rtcpport; }

	/** Use sockets that have already been created, no checks on port numbers
	 *  will be done, and no buffer sizes will be set; you'll need to close
	 *  the sockets yourself when done, it will **not** be done automatically. */
	void SetUseExistingSockets(CDataSocket *rtpsocket, CDataSocket *rtcpsocket) { rtpsock = rtpsocket; rtcpsock = rtcpsocket; useexistingsockets = true; }

	/** If non null, the specified abort descriptors will be used to cancel
	 *  the function that's waiting for packets to arrive; set to null (the default
	 *  to let the transmitter create its own instance. */
	void SetCreatedAbortDescriptors(RTPAbortDescriptors *desc) { m_pAbortDesc = desc; }

	/** Returns the RTP socket's send buffer size. */
	int GetRTPSendBuffer() const								{ return rtpsendbuf; }

	/** Returns the RTP socket's receive buffer size. */
	int GetRTPReceiveBuffer() const								{ return rtprecvbuf; }

	/** Returns the RTCP socket's send buffer size. */
	int GetRTCPSendBuffer() const								{ return rtcpsendbuf; }

	/** Returns the RTCP socket's receive buffer size. */
	int GetRTCPReceiveBuffer() const							{ return rtcprecvbuf; }

	/** Returns a flag indicating if RTCP traffic will be multiplexed over the RTP channel. */
	bool GetRTCPMultiplexing() const							{ return rtcpmux; }

	/** If true, any RTP portbase will be allowed, not just even numbers. */
	bool GetAllowOddPortbase() const							{ return allowoddportbase; }

	/** If non-zero, the specified port will be used to receive RTCP traffic. */
	WORD GetForcedRTCPPort() const							{ return forcedrtcpport; }

	/** Returns true and fills in sockets if existing sockets were set
	 *  using RTPUDPv4TransmissionParams::SetUseExistingSockets. */
	bool GetUseExistingSockets(CDataSocket *rtpsocket, CDataSocket *rtcpsocket) const { 
		if(!useexistingsockets) return false; 
		rtpsocket = rtpsock; rtcpsocket = rtcpsock; return true;
		}

	/** If non-null, this RTPAbortDescriptors instance will be used internally,
	 *  which can be useful when creating your own poll thread for multiple
	 *  sessions. */
	RTPAbortDescriptors *GetCreatedAbortDescriptors() const		{ return m_pAbortDesc; }
private:
	WORD portbase;
	DWORD bindIP, mcastifaceIP;
	CList <DWORD,DWORD> localIPs;
	BYTE multicastTTL;
	int rtpsendbuf, rtprecvbuf;
	int rtcpsendbuf, rtcprecvbuf;
	bool rtcpmux;
	bool allowoddportbase;
	WORD forcedrtcpport;

	CDataSocket *rtpsock,*rtcpsock;
	bool useexistingsockets;

	RTPAbortDescriptors *m_pAbortDesc;
	};

inline RTPUDPv4TransmissionParams::RTPUDPv4TransmissionParams() : RTPTransmissionParams(RTPTransmitter::IPv4UDPProto)	{ 

	portbase = RTPUDPV4TRANS_DEFAULTPORTBASE; 
	bindIP = 0; 
	multicastTTL = 1; 
	mcastifaceIP = 0; 
	rtpsendbuf = RTPUDPV4TRANS_RTPTRANSMITBUFFER; 
	rtprecvbuf = RTPUDPV4TRANS_RTPRECEIVEBUFFER; 
	rtcpsendbuf = RTPUDPV4TRANS_RTCPTRANSMITBUFFER; 
	rtcprecvbuf = RTPUDPV4TRANS_RTCPRECEIVEBUFFER; 
	rtcpmux = false;
	allowoddportbase = false;
	forcedrtcpport = 0;
	useexistingsockets = false;
	rtpsock = NULL;
	rtcpsock = NULL;
	m_pAbortDesc = NULL;
	}

/** Base class for additional information about the transmitter. 
 *  This class is an abstract class which will have a specific implementation for a 
 *  specific kind of transmission component. All actual implementations inherit the
 *  GetTransmissionProtocol function which identifies the component type for which
 *  these parameters are valid.
 */
class RTPTransmissionInfo {
protected:
	RTPTransmissionInfo(RTPTransmitter::TransmissionProtocol p)		{ protocol = p; }
public:
	virtual ~RTPTransmissionInfo() { }
	/** Returns the transmitter type for which these parameters are valid. */
	RTPTransmitter::TransmissionProtocol GetTransmissionProtocol() const			{ return protocol; }
private:
	RTPTransmitter::TransmissionProtocol protocol;
	};

/** Additional information about the UDP over IPv4 transmitter. */
class RTPUDPv4TransmissionInfo : public RTPTransmissionInfo {
public:
	RTPUDPv4TransmissionInfo(CList<DWORD,DWORD> iplist,CDataSocket *rtpsock,CDataSocket *rtcpsock,
	                         WORD rtpport, WORD rtcpport) : RTPTransmissionInfo(RTPTransmitter::IPv4UDPProto) 
															{ 
		for(POSITION pos = iplist.GetHeadPosition(); pos; ) { 
			const auto& item = iplist.GetNext(pos); localIPlist.AddTail(item);
			}
		rtpsocket = rtpsock; rtcpsocket = rtcpsock; m_rtpPort = rtpport; m_rtcpPort = rtcpport; }

	~RTPUDPv4TransmissionInfo()								{ }
	
	/** Returns the list of IPv4 addresses the transmitter considers to be the local IP addresses. */
//	std::list<DWORD> GetLocalIPList() const				{ return localIPlist; }

	/** Returns the socket descriptor used for receiving and transmitting RTP packets. */
	CDataSocket *GetRTPSocket() const							{ return rtpsocket; }

	/** Returns the socket descriptor used for receiving and transmitting RTCP packets. */
	CDataSocket *GetRTCPSocket() const						{ return rtcpsocket; }

	/** Returns the port number that the RTP socket receives packets on. */
	WORD GetRTPPort() const								{ return m_rtpPort; }

	/** Returns the port number that the RTCP socket receives packets on. */
	WORD GetRTCPPort() const							{ return m_rtcpPort; }
private:
	CList<DWORD,DWORD> localIPlist;
	CDataSocket *rtpsocket,*rtcpsocket;
	WORD m_rtpPort, m_rtcpPort;
	};

/** Parameters to initialize a transmitter of type RTPExternalTransmitter. */
class RTPExternalTransmissionParams : public RTPTransmissionParams {
public:
	/** Using this constructor you can specify which RTPExternalSender object you'll be using
	 *  and how much the additional header overhead for each packet will be. */
	RTPExternalTransmissionParams(RTPExternalSender *s, int headeroverhead):RTPTransmissionParams(RTPTransmitter::ExternalProto)	{ sender = s; headersize = headeroverhead; }

	RTPExternalSender *GetSender() const								{ return sender; }
	int GetAdditionalHeaderSize() const								{ return headersize; }
private:
	RTPExternalSender *sender;
	int headersize;
};

/** Additional information about the external transmission component. */
class RTPExternalTransmissionInfo : public RTPTransmissionInfo {
public:
	RTPExternalTransmissionInfo(RTPExternalPacketInjecter *p) : RTPTransmissionInfo(RTPTransmitter::ExternalProto) { packetinjector = p; }

	/** Tells you which RTPExternalPacketInjecter you need to use to pass RTP or RTCP
	 *  data on to the transmission component. */
	RTPExternalPacketInjecter *GetPacketInjector() const						{ return packetinjector; }
private:
	RTPExternalPacketInjecter *packetinjector;
};
	
/** A transmission component which will use user specified functions to transmit the data and
 *  which will expose functions to inject received RTP or RTCP data into this component.
 *  A transmission component which will use user specified functions to transmit the data and
 *  which will expose functions to inject received RTP or RTCP data into this component. Use
 *  a class derived from RTPExternalSender to specify the functions which need to be used for
 *  sending the data. Obtain the RTPExternalTransmissionInfo object associated with this
 *  transmitter to obtain the functions needed to pass RTP/RTCP packets on to the transmitter.
 */
class RTPExternalTransmitter : public RTPTransmitter {
public:
	RTPExternalTransmitter();
	~RTPExternalTransmitter();

	int Init(bool treadsafe);
	int Create(size_t maxpacksize, const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();
	void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

	int GetLocalHostName(BYTE *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()									{ return headersize; }
	
	int Poll();
	int WaitForIncomingData(const RTPTime &delay,bool *dataavailable=NULL);
	int AbortWait();
	
	int SendRTPData(const void *data,size_t len);	
	int SendRTCPData(const void *data,size_t len);

	int AddDestination(const RTPAddress &addr);
	int DeleteDestination(const RTPAddress &addr);
	void ClearDestinations();

	bool SupportsMulticasting();
	int JoinMulticastGroup(const RTPAddress &addr);
	int LeaveMulticastGroup(const RTPAddress &addr);
	void LeaveAllMulticastGroups();

	int SetReceiveMode(RTPTransmitter::ReceiveMode m);
	int AddToIgnoreList(const RTPAddress &addr);
	int DeleteFromIgnoreList(const RTPAddress &addr);
	void ClearIgnoreList();
	int AddToAcceptList(const RTPAddress &addr);
	int DeleteFromAcceptList(const RTPAddress &addr);
	void ClearAcceptList();
	int SetMaximumPacketSize(size_t s);	
	
	bool NewDataAvailable();
	RTPRawPacket *GetNextPacket();
#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG

	void InjectRTP(const void *data, size_t len, const RTPAddress &a);
	void InjectRTCP(const void *data, size_t len, const RTPAddress &a);
	void InjectRTPorRTCP(const void *data, size_t len, const RTPAddress &a);
private:
	void FlushPackets();
	
	bool init;
	bool created;
	bool waitingfordata;
	RTPExternalSender *sender;
	RTPExternalPacketInjecter packetinjector;

	BYTE *localhostname;
	size_t localhostnamelength;

	size_t maxpacksize;
	int headersize;

	RTPAbortDescriptors m_abortDesc;
	int m_abortCount;
#ifdef RTP_SUPPORT_THREAD
	jthread::JMutex mainmutex,waitmutex;
	int threadsafe;
#endif // RTP_SUPPORT_THREAD
	};

inline void RTPExternalPacketInjecter::InjectRTP(const void *data, size_t len, const RTPAddress &a){
	transmitter->InjectRTP(data, len, a); 
	}

inline void RTPExternalPacketInjecter::InjectRTCP(const void *data, size_t len, const RTPAddress &a){ 
	transmitter->InjectRTCP(data, len, a); 
	}

inline void RTPExternalPacketInjecter::InjectRTPorRTCP(const void *data, size_t len, const RTPAddress &a){ 
	transmitter->InjectRTPorRTCP(data, len, a); 
	}


// TODO: this is for IPv4, and will only be valid if one rtp packet is in one tcp frame
#define RTPTCPTRANS_HEADERSIZE		(20+20+2) // 20 IP, 20 TCP, 2 for framing (RFC 4571)
class SocketData {
	public:
		SocketData();
		~SocketData();
		void Reset();

		BYTE m_lengthBuffer[2];
		int m_lengthBufferOffset;
		int m_dataLength;
		int m_dataBufferOffset;
		BYTE *m_pDataBuffer;

		BYTE *ExtractDataBuffer() { BYTE *pTmp = m_pDataBuffer; m_pDataBuffer=0; return pTmp; }
		int ProcessAvailableBytes(CSocket *sock, int availLen, bool &complete /*, RTPMemoryManager *pMgr*/);
		};

class RTPTCPTransmitter : public RTPTransmitter {
public:
	RTPTCPTransmitter(/*RTPMemoryManager *mgr*/);
	~RTPTCPTransmitter();
	int Init(bool treadsafe);
	int Create(size_t maxpacksize, const RTPTransmissionParams *transparams);
	void Destroy();
	int AddDestination(const RTPAddress &addr);
	int DeleteDestination(const RTPAddress &addr);
	void ClearDestinations();
	int Poll();
	void FlushPackets();
	void ClearDestSockets();
	int ValidateSocket(SocketType s);
	int SetMaximumPacketSize(size_t s);	
	int SetReceiveMode(RTPTransmitter::ReceiveMode m);
	size_t GetHeaderOverhead()							{ return RTPTCPTRANS_HEADERSIZE; }
	int GetLocalHostName(BYTE *buffer,size_t *bufferlength);
	/** By overriding this function you can be notified of an error when sending over a socket. */
	virtual void OnSendError(CSocket *sock);
	/** By overriding this function you can be notified of an error when receiving from a socket. */
	virtual void OnReceiveError(CSocket *sock);
	int SendRTPRTCPData(const void *data,size_t len);	
	int SendRTCPData(const void *data,size_t len);
	int SendRTPData(const void *data,size_t len);
	// override RTPTCPTransmitter
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	RTPRawPacket *GetNextPacket();
	int PollSocket(CSocket *sock, SocketData &sdata);
protected:
	std::map<SocketType, SocketData> m_destSockets;
	std::vector<SocketType> m_tmpSocks;
	std::vector<BYTE> m_localHostname;
	bool m_init;
	bool m_created;
	bool m_waitingForData;
	size_t m_maxPackSize;
	};
inline void RTPTCPTransmitter::OnSendError(CSocket *) { }
inline void RTPTCPTransmitter::OnReceiveError(CSocket *) { }

class MyTCPTransmitter : public RTPTCPTransmitter {
public:
	MyTCPTransmitter(const CString name) : RTPTCPTransmitter(/*0*/), m_name(name), m_recvstate(RECV_LEN), RecvRtspCmd(NULL)  { }
	MyTCPTransmitter(/*RTPMemoryManager *mgr*/) : RTPTCPTransmitter(), m_name(""), m_recvstate(RECV_LEN), RecvRtspCmd(NULL) { }

	void OnSendError(CSocket *sock)	{
		//cout << m_name << ": Error sending over socket " << sock << ", removing destination" << endl;
		DeleteDestination(RTPTCPAddress(sock));
		}
	void OnReceiveError(CSocket *sock) {
		//cout << m_name << ": Error receiving from socket " << sock << ", removing destination" << endl;
		DeleteDestination(RTPTCPAddress(sock));
		}
  virtual void SetRecvRtspCmdClbk(RECV_RTSP_CMD_CLBK clbk) { RecvRtspCmd = clbk; }

protected:
	// NOTE: functions override RTPTCPTransmitter, which should changed from 'private' to 'protected' of Jrtplib in rtptcptransmitter.h
	// override RTPTCPTransmitter
	int SendRTPRTCPData(const void *data,size_t len);	
	int SendRTCPData(const void *data,size_t len);
	int SendRTPData(const void *data,size_t len);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	RTPRawPacket *GetNextPacket();
	// override RTPTCPTransmitter
	int PollSocket(CDataSocket *sock, SocketData &sdata);
    
private:
	CString m_name;
  RecvStateEnum m_recvstate;
  int m_dataLength;
  int m_lengthBufferOffset;
  BYTE m_httpTunnelHeaderBuffer[4];
  BYTE *m_pDataBuffer;
  bool m_isrtp;
	RECV_RTSP_CMD_CLBK RecvRtspCmd;
	};

class RTPSessionParams;
class RTPSourceData;
class RTPInternalSourceData;
class RTPSession;
class RTCPSDESPacket;
class RTPPacket;
class RTCPAPPPacket;
class RTPSources /*: public RTPMemoryObject*/ {
public:
	/** Type of probation to use for new sources. */
	enum ProbationType 	{ 
		NoProbation, 		/**< Don't use the probation algorithm; accept RTP packets immediately. */
		ProbationDiscard, 	/**< Discard incoming RTP packets originating from a source that's on probation. */
		ProbationStore 		/**< Store incoming RTP packet from a source that's on probation for later retrieval. */
		};
	
	/** In the constructor you can select the probation type you'd like to use and also a memory manager. */
	RTPSources(ProbationType = ProbationStore, char *m=0/*,RTPMemoryManager *mgr=0*/);
	virtual ~RTPSources();
		/** Sets the current source to be the first source in the table which has RTPPacket instances 
	 *  that we haven't extracted yet.
	 *  Sets the current source to be the first source in the table which has RTPPacket instances 
	 *  that we haven't extracted yet. If no such member was found, the function returns false,
	 *  otherwise it returns true.
	 */

	/** This function should be called if our own session has sent an RTP packet. 
	 *  This function should be called if our own session has sent an RTP packet.
	 *  For our own SSRC entry, the sender flag is updated based upon outgoing packets instead of incoming packets.
	 */
	void SentRTPPacket();
	/** 
	 *  Processes a raw packet rawpack. The instance trans will be used to check if this
	 *  packet is one of our own packets. The flag acceptownpackets indicates whether own packets should be 
	 *  accepted or ignored.
	 */
	int ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *trans,bool acceptownpackets);
	/** 
	 *  Processes a raw packet rawpack. Every transmitter in the array trans of length numtrans
	 *  is used to check if the packet is from our own session. The flag acceptownpackets indicates
	 *  whether own packets should be accepted or ignored.
	 */
	int ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *trans[],int numtrans,bool acceptownpackets);
	/** Processes an RTPPacket instance rtppack which was received at time receivetime and 
	 *  which originated from senderaddres.
	 *  Processes an RTPPacket instance rtppack which was received at time receivetime and 
	 *  which originated from senderaddres. The senderaddress parameter must be NULL if
	 *  the packet was sent by the local participant. The flag stored indicates whether the packet 
	 *  was stored in the table or not.  If so, the rtppack instance may not be deleted.
	 */
	int ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,const RTPAddress *senderaddress,bool *stored);
	/** Processes the RTCP compound packet rtcpcomppack which was received at time receivetime from senderaddress.
	 *  Processes the RTCP compound packet rtcpcomppack which was received at time receivetime from senderaddress.
	 *  The senderaddress parameter must be NULL if the packet was sent by the local participant.
	 */
	int ProcessRTCPCompoundPacket(RTCPCompoundPacket *rtcpcomppack,const RTPTime &receivetime,
	                              const RTPAddress *senderaddress);

	/** Process the sender information of SSRC ssrc into the source table. 
	 *  Process the sender information of SSRC ssrc into the source table. The information was received
	 *  at time receivetime from address senderaddress. The senderaddress} parameter must be NULL 
	 *  if the packet was sent by the local participant.
	 */
	int ProcessRTCPSenderInfo(DWORD ssrc,const RTPNTPTime &ntptime,DWORD rtptime,
	                          DWORD packetcount,DWORD octetcount,const RTPTime &receivetime,
				  const RTPAddress *senderaddress);

    /** Processes the report block information which was sent by participant ssrc into the source table.
	 *  Processes the report block information which was sent by participant ssrc into the source table.
	 *  The information was received at time receivetime from address senderaddress The senderaddress
	 *  parameter must be NULL if the packet was sent by the local participant.
	 */
	int ProcessRTCPReportBlock(DWORD ssrc,BYTE fractionlost,int lostpackets,
	                           DWORD exthighseqnr,DWORD jitter,DWORD lsr,
	                           DWORD dlsr,const RTPTime &receivetime,const RTPAddress *senderaddress);

	/** Processes the non-private SDES item from source ssrc into the source table. 
	 *  Processes the non-private SDES item from source ssrc into the source table. The information was
	 *  received at time receivetime from address senderaddress. The senderaddress parameter must
	 *  be NULL if the packet was sent by the local participant.
	 */
	int ProcessSDESNormalItem(DWORD ssrc,RTCPSDESPacket::ItemType t,size_t itemlength,
	                          const void *itemdata,const RTPTime &receivetime,const RTPAddress *senderaddress);
#ifdef RTP_SUPPORT_SDESPRIV
	/** Processes the SDES private item from source ssrc into the source table. 
	 *  Processes the SDES private item from source ssrc into the source table. The information was 
	 *  received at time receivetime from address senderaddress. The senderaddress 
	 *  parameter must be NULL if the packet was sent by the local participant.
	 */
	int ProcessSDESPrivateItem(DWORD ssrc,size_t prefixlen,const void *prefixdata,
	                           size_t valuelen,const void *valuedata,const RTPTime &receivetime,
	                           const RTPAddress *senderaddress);
#endif //RTP_SUPPORT_SDESPRIV
	/** Processes the BYE message for SSRC ssrc. 
	 *  Processes the BYE message for SSRC ssrc. The information was received at time receivetime from
	 *  address senderaddress. The senderaddress parameter must be NULL if the packet was sent by the
	 *  local participant.
	 */
	int ProcessBYE(DWORD ssrc,size_t reasonlength,const void *reasondata,const RTPTime &receivetime,
	               const RTPAddress *senderaddress);
	/** If we heard from source ssrc, but no actual data was added to the source table (for example, if
	 *  no report block was meant for us), this function can e used to indicate that something was received from
	 *  this source. 
	 *  If we heard from source ssrc, but no actual data was added to the source table (for example, if
	 *  no report block was meant for us), this function can e used to indicate that something was received from
	 *  this source. This will prevent a premature timeout for this participant. The message was received at time 
	 *  receivetime from address senderaddress. The senderaddress parameter must be NULL if the 
	 *  packet was sent by the local participant.
	 */
	int UpdateReceiveTime(DWORD ssrc,const RTPTime &receivetime,const RTPAddress *senderaddress);
	/** Clears the source table. */
	void Clear();
	/** Sets the current source to be the first source in the table which has RTPPacket instances 
	 *  that we haven't extracted yet.
	 *  Sets the current source to be the first source in the table which has RTPPacket instances 
	 *  that we haven't extracted yet. If no such member was found, the function returns false,
	 *  otherwise it returns true.
	 */
	bool GotoFirstSourceWithData();
	/** Sets the current source to be the next source in the table.
	 *  Sets the current source to be the next source in the table. If we're already at the last source, 
	 *  the function returns false, otherwise it returns true.
	 */
	bool GotoNextSource();
	bool GotoPreviousSource();
	/** Starts the iteration over the participants by going to the first member in the table.
	 *  Starts the iteration over the participants by going to the first member in the table.
	 *  If a member was found, the function returns true, otherwise it returns false.
	 */
	bool GotoFirstSource();
	RTPSourceData *GetOwnSourceInfo()								{ return (RTPSourceData *)owndata; }

	/** Returns the RTPSourceData instance for the currently selected participant. */
	RTPSourceData *GetCurrentSourceInfo();
	/** Extracts the next packet from the received packets queue of the current participant. */
	RTPPacket *GetNextPacket();
	/** Creates an entry for our own SSRC identifier. */
	int CreateOwnSSRC(DWORD ssrc);
	/** Deletes the entry for our own SSRC identifier. */
	int DeleteOwnSSRC();
	bool GotEntry(DWORD ssrc);
	/** Is called when a new entry srcdat is added to the source table. */
	virtual void OnNewSource(RTPSourceData *srcdat);
	int ObtainSourceDataInstance(DWORD ssrc,RTPInternalSourceData **srcdat,bool *created);
	int GetRTCPSourceData(DWORD ssrc,const RTPAddress *senderaddress,RTPInternalSourceData **srcdat,bool *newsource);
	bool CheckCollision(RTPInternalSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp);
	void MultipleTimeouts(const RTPTime &curtime,const RTPTime &sendertimeout,
			      const RTPTime &byetimeout,const RTPTime &generaltimeout,
			      const RTPTime &notetimeout);
	/** Returns the number of participants which are marked as a sender. */
	int GetSenderCount() const										{ return sendercount; }
	/** Returns the total number of entries in the source table. */
	int GetTotalCount() const										{ return totalcount; }
	int GetActiveMemberCount() const								{ return activecount; } 

	/** 
	 *  Is called when an SSRC collision was detected. The instance srcdat is the one present in 
	 *  the table, the address senderaddress is the one that collided with one of the addresses 
	 *  and isrtp indicates against which address of srcdat the check failed.
	 */
	virtual void OnSSRCCollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp);
	/** Is called when the entry srcdat is about to be deleted from the source table. */
	virtual void OnRemoveSource(RTPSourceData *srcdat);
	/** Is called when participant srcdat is timed out. */
	virtual void OnTimeout(RTPSourceData *srcdat);
	/** Is called when participant srcdat is timed after having sent a BYE packet. */
	virtual void OnBYETimeout(RTPSourceData *srcdat);
	/** Is called when a BYE packet has been processed for source srcdat. */
	virtual void OnBYEPacket(RTPSourceData *srcdat);
	/** Is called when an RTCP sender report has been processed for this source. */
	virtual void OnRTCPSenderReport(RTPSourceData *srcdat);
	/** Is called when another CNAME was received than the one already present for source srcdat. */
	virtual void OnCNAMECollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,
	                              const BYTE *cname,size_t cnamelength);
	/** Is called when a specific SDES item was received for this source. */
	virtual void OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t,
	                            const void *itemdata, size_t itemlength);
	/** Is called when an RTCP receiver report has been processed for this source. */
	virtual void OnRTCPReceiverReport(RTPSourceData *srcdat);
	/** Is called when the SDES NOTE item for source srcdat has been timed out. */
	virtual void OnNoteTimeout(RTPSourceData *srcdat);
	/** Is called when an RTP packet is about to be processed. */
	virtual void OnRTPPacket(RTPPacket *pack,const RTPTime &receivetime, const RTPAddress *senderaddress);
	/** Is called when an RTCP compound packet is about to be processed. */
	virtual void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,
	                                  const RTPAddress *senderaddress);
	virtual void OnAPPPacket(RTCPAPPPacket *apppacket,const RTPTime &receivetime,
	                         const RTPAddress *senderaddress);
	/** Is called when an unknown RTCP packet type was detected. */
	virtual void OnUnknownPacketType(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                                 const RTPAddress *senderaddress);

	/** Is called when an unknown packet format for a known packet type was detected. */
	virtual void OnUnknownPacketFormat(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                                   const RTPAddress *senderaddress);

	virtual void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled);
private:
	void ClearSourceList();

	int sendercount;
	int totalcount;
	int activecount;
	RTPKeyHashTable<const DWORD,RTPInternalSourceData*,RTPSources_GetHashIndex,RTPSOURCES_HASHSIZE> sourcelist;
	RTPInternalSourceData *owndata;

	friend class RTPInternalSourceData;
	};

class RTPSessionSources : public RTPSources {
public:
	RTPSessionSources(RTPSession &sess/*,RTPMemoryManager *mgr*/) : 
			RTPSources(RTPSources::ProbationStore/*,mgr*/),rtpsession(sess)
													{ owncollision = false; }
	~RTPSessionSources()										{ }
	void ClearOwnCollisionFlag()									{ owncollision = false; }
	bool DetectedOwnCollision() const								{ return owncollision; }
private:
	RTPSession &rtpsession;
	bool owncollision;
	};

inline void RTPSources::OnAPPPacket(RTCPAPPPacket *, const RTPTime &, const RTPAddress *)                           { }
inline void RTPSources::OnUnknownPacketType(RTCPPacket *, const RTPTime &, const RTPAddress *)                      { }
inline void RTPSources::OnUnknownPacketFormat(RTCPPacket *, const RTPTime &, const RTPAddress *)                    { }
inline void RTPSources::OnRTPPacket(RTPPacket *, const RTPTime &, const RTPAddress *)                               { }
inline void RTPSources::OnRTCPCompoundPacket(RTCPCompoundPacket *, const RTPTime &, const RTPAddress *)             { }
inline void RTPSources::OnSSRCCollision(RTPSourceData *, const RTPAddress *, bool)                                  { }
inline void RTPSources::OnTimeout(RTPSourceData *)                                                                  { }
inline void RTPSources::OnRemoveSource(RTPSourceData *)                                                             { }
inline void RTPSources::OnBYETimeout(RTPSourceData *)                                                               { }
inline void RTPSources::OnNoteTimeout(RTPSourceData *)                                                              { }
inline void RTPSources::OnBYEPacket(RTPSourceData *)                                                                { }
inline void RTPSources::OnRTCPSenderReport(RTPSourceData *)                                                         { }
inline void RTPSources::OnRTCPReceiverReport(RTPSourceData *)                                                       { }
inline void RTPSources::OnRTCPSDESItem(RTPSourceData *, RTCPSDESPacket::ItemType, const void *, size_t)             { }
inline void RTPSources::OnCNAMECollision(RTPSourceData *, const RTPAddress *, const BYTE *, size_t)              { }
inline void RTPSources::OnValidatedRTPPacket(RTPSourceData *, RTPPacket *, bool, bool *)                            { }

/** This class can be used to build RTP packets and is a bit more high-level than the RTPPacket 
 *  class: it generates an SSRC identifier, keeps track of timestamp and sequence number etc.
 */
class RTPPacketBuilder /*: public RTPMemoryObject */ {
public:
	/** Constructs an instance which will use rtprand for generating random numbers
	 *  (used to initialize the SSRC value and sequence number), optionally installing a memory manager. 
	 **/
	RTPPacketBuilder(int /*RTPRandom &*/ rtprand /*, RTPMemoryManager *mgr=0*/);
	~RTPPacketBuilder();
	int Init(size_t);
	/** Cleans up the builder. */
	void Destroy();
	/** Returns the number of packets which have been created with the current SSRC identifier. */
	DWORD GetPacketCount()					{ if(!init) return 0; return numpackets; }
	/** Returns the number of payload octets which have been generated with this SSRC identifier. */
	DWORD GetPayloadOctetCount()				{ if(!init) return 0; return numpayloadbytes; }
	/** Creates a new SSRC to be used in generated packets. 
	 *  Creates a new SSRC to be used in generated packets. This will also generate new timestamp and 
	 *  sequence number offsets.
	 */
	DWORD CreateNewSSRC();
	/** Creates a new SSRC to be used in generated packets. 
	 *  Creates a new SSRC to be used in generated packets. This will also generate new timestamp and 
	 *  sequence number offsets. The source table sources is used to make sure that the chosen SSRC 
	 *  isn't used by another participant yet.
	 */
	DWORD CreateNewSSRC(RTPSources &sources);
	/** Returns the current SSRC identifier. */
	DWORD GetSSRC() const					{ if(!init) return 0; return ssrc; }
	/** Returns the current RTP timestamp. */
	DWORD GetTimestamp() const		{ if(!init) return 0; return timestamp; }
	/** Returns the time at which a packet was generated.
	 *  Returns the time at which a packet was generated. This is not necessarily the time at which 
	 *  the last RTP packet was generated: if the timestamp increment was zero, the time is not updated.
	 */
	RTPTime GetPacketTime() const					{ if(!init) return RTPTime(0,0); return lastwallclocktime; }
	/** Returns the RTP timestamp which corresponds to the time returned by the previous function. */
	DWORD GetPacketTimestamp() const				{ if(!init) return 0; return lastrtptimestamp; }
	/** Sets a specific SSRC to be used.
	 *  Sets a specific SSRC to be used. Does not create a new timestamp offset or sequence number
	 *  offset. Does not reset the packet count or byte count. Think twice before using this!
	 */
	void AdjustSSRC(DWORD s)					{ ssrc = s; }
private:
	int PrivateBuildPacket(const void *data,size_t len,
	                  BYTE pt,bool mark,DWORD timestampinc,bool gotextension,
	                  WORD hdrextID = 0,const void *hdrextdata=NULL,size_t numhdrextwords = 0);
private:
	bool init;
	DWORD numpayloadbytes;
	DWORD numpackets;
	DWORD ssrc;
	DWORD timestamp;
	WORD seqnr;
	int /*RTPRandom &*/ rtprnd;	
	size_t maxpacksize;
	BYTE *buffer;
	size_t packetlength;
	bool deftsset,defptset,defmarkset;
	int numcsrcs;
	RTPTime lastwallclocktime;
	DWORD lastrtptimestamp;
	};
inline void RTPSources::OnNewSource(RTPSourceData *)    { }

class RTCPSDESInfo {
public:
	/** Constructs an instance, optionally installing a memory manager. */
	RTCPSDESInfo() 	{  }
	virtual ~RTCPSDESInfo()			{ Clear(); }

	/** Clears all SDES information. */
	void Clear();

	/** Sets the SDES CNAME item to s with length l. */
	int SetCNAME(const BYTE *s,size_t l)					{ return SetNonPrivateItem(RTCP_SDES_ID_CNAME-1,s,l); }

	/** Sets the SDES name item to s with length l. */
	int SetName(const BYTE *s,size_t l)					{ return SetNonPrivateItem(RTCP_SDES_ID_NAME-1,s,l); }

	/** Sets the SDES e-mail item to s with length l. */
	int SetEMail(const BYTE *s,size_t l)					{ return SetNonPrivateItem(RTCP_SDES_ID_EMAIL-1,s,l); }

	/** Sets the SDES phone item to s with length l. */
	int SetPhone(const BYTE *s,size_t l)					{ return SetNonPrivateItem(RTCP_SDES_ID_PHONE-1,s,l); }

	/** Sets the SDES location item to s with length l. */
	int SetLocation(const BYTE *s,size_t l)				{ return SetNonPrivateItem(RTCP_SDES_ID_LOCATION-1,s,l); }

	/** Sets the SDES tool item to s with length l. */
	int SetTool(const BYTE *s,size_t l)					{ return SetNonPrivateItem(RTCP_SDES_ID_TOOL-1,s,l); }

	/** Sets the SDES note item to s with length l. */
	int SetNote(const BYTE *s,size_t l)					{ return SetNonPrivateItem(RTCP_SDES_ID_NOTE-1,s,l); }

#ifdef RTP_SUPPORT_SDESPRIV
	/** Sets the entry for the prefix string specified by prefix with length prefixlen to contain 
	 *  the value string specified by value with length valuelen (if the maximum allowed
	 *  number of prefixes was reached, the error code ERR_RTP_SDES_MAXPRIVITEMS is returned.
	 */
	int SetPrivateValue(const BYTE *prefix,size_t prefixlen,const BYTE *value,size_t valuelen);

	/** Deletes the entry for the prefix specified by s with length len. */
	int DeletePrivatePrefix(const BYTE *s,size_t len);
#endif // RTP_SUPPORT_SDESPRIV
	
	/** Returns the SDES CNAME item and stores its length in len. */
	BYTE *GetCNAME(size_t *len) const					{ return GetNonPrivateItem(RTCP_SDES_ID_CNAME-1,len); }
	/** Returns the SDES name item and stores its length in len. */
	BYTE *GetName(size_t *len) const						{ return GetNonPrivateItem(RTCP_SDES_ID_NAME-1,len); }
	/** Returns the SDES e-mail item and stores its length in len. */
	BYTE *GetEMail(size_t *len) const					{ return GetNonPrivateItem(RTCP_SDES_ID_EMAIL-1,len); }
	/** Returns the SDES phone item and stores its length in len. */
	BYTE *GetPhone(size_t *len) const					{ return GetNonPrivateItem(RTCP_SDES_ID_PHONE-1,len); }
	/** Returns the SDES location item and stores its length in len. */
	BYTE *GetLocation(size_t *len) const					{ return GetNonPrivateItem(RTCP_SDES_ID_LOCATION-1,len); }
	/** Returns the SDES tool item and stores its length in len. */
	BYTE *GetTool(size_t *len) const						{ return GetNonPrivateItem(RTCP_SDES_ID_TOOL-1,len); }
	/** Returns the SDES note item and stores its length in len. */
	BYTE *GetNote(size_t *len) const 					{ return GetNonPrivateItem(RTCP_SDES_ID_NOTE-1,len); }
#ifdef RTP_SUPPORT_SDESPRIV
	/** Starts the iteration over the stored SDES private item prefixes and their associated values. */
	void GotoFirstPrivateValue();

	/** Returns SDES priv item information.
	 *  If available, returns true and stores the next SDES
	 *  private item prefix in prefix and its length in
	 *  prefixlen. The associated value and its length are
	 *  then stored in value and valuelen. Otherwise,
	 *  it returns false.
     */
	bool GetNextPrivateValue(BYTE **prefix,size_t *prefixlen,BYTE **value,size_t *valuelen);

	/** Returns SDES priv item information.
	 *  Looks for the entry which corresponds to the SDES private
	 *  item prefix prefix with length prefixlen. If found,
	 *  the function returns true and stores the associated
	 *  value and its length in value and valuelen
	 *  respectively. 
	 */
	bool GetPrivateValue(const BYTE *prefix,size_t prefixlen,BYTE **value,size_t *valuelen) const;
#endif // RTP_SUPPORT_SDESPRIV
private:
	int SetNonPrivateItem(int itemno,const BYTE *s,size_t l)		{ if(l > RTCP_SDES_MAXITEMLENGTH) return ERR_RTP_SDES_LENGTHTOOBIG; return nonprivateitems[itemno].SetInfo(s,l); }
	BYTE *GetNonPrivateItem(int itemno,size_t *len) const		{ return nonprivateitems[itemno].GetInfo(len); }

	class SDESItem {
	public:
		SDESItem() {  
			str = 0; 
			length = 0; 
			}
		void SetMemoryManager() {
			}
		~SDESItem() { 
			if(str) 
				delete str;
			}
		BYTE *GetInfo(size_t *len) const				{ *len = length; return str; }
		int SetInfo(const BYTE *s,size_t len)			{ return SetString(&str,&length,s,len); }
	protected:
		int SetString(BYTE **dest,size_t *destlen,const BYTE *s,size_t len)	{
			if(len <= 0)	{
				if(*dest)
					delete (*dest);
				*dest = 0;
				*destlen = 0;
				}
			else {
				len = (len>RTCP_SDES_MAXITEMLENGTH)?RTCP_SDES_MAXITEMLENGTH:len;
				BYTE *str2 = new BYTE[len];
				if(str2 == 0)
					return ERR_RTP_OUTOFMEM;
				memcpy(str2,s,len);
				*destlen = len;
				if(*dest)
					delete (*dest);
				*dest = str2;
				}
			return 0;
			}
	private:
		BYTE *str;
		size_t length;
		};

	SDESItem nonprivateitems[RTCP_SDES_NUMITEMS_NONPRIVATE];

#ifdef RTP_SUPPORT_SDESPRIV
	class SDESPrivateItem : public SDESItem	{
	public:
		SDESPrivateItem() : SDESItem() { 
			prefixlen = 0; 
			prefix = 0; 
			}
		~SDESPrivateItem() { 
			if(prefix) 
				delete prefix;
			}
		BYTE *GetPrefix(size_t *len) const				{ *len = prefixlen; return prefix; }
		int SetPrefix(const BYTE *s,size_t len)			{ return SetString(&prefix,&prefixlen,s,len); }
	private:
		BYTE *prefix;
		size_t prefixlen;
		};

	CList<SDESPrivateItem *,SDESPrivateItem *> privitems;
	POSITION curitem;
#endif // RTP_SUPPORT_SDESPRIV
	};

class RTCPSDESInfoInternal : public RTCPSDESInfo {
public:
	RTCPSDESInfoInternal() : RTCPSDESInfo()	{ ClearFlags(); }
	void ClearFlags()			{ pname = false; pemail = false; plocation = false; pphone = false; ptool = false; pnote = false; }
	bool ProcessedName() const 		{ return pname; }
	bool ProcessedEMail() const		{ return pemail; }
	bool ProcessedLocation() const		{ return plocation; }
	bool ProcessedPhone() const		{ return pphone; }
	bool ProcessedTool() const		{ return ptool; }
	bool ProcessedNote() const		{ return pnote; }
	void SetProcessedName(bool v)		{ pname = v; }
	void SetProcessedEMail(bool v)		{ pemail = v; }
	void SetProcessedLocation(bool v)	{ plocation  = v; }
	void SetProcessedPhone(bool v)		{ pphone = v; }
	void SetProcessedTool(bool v)		{ ptool = v; }
	void SetProcessedNote(bool v)		{ pnote = v; }
private:
	bool pname,pemail,plocation,pphone,ptool,pnote;
	};

class RTCPCompoundPacketBuilder;
/** This class can be used to build RTCP compound packets, on a higher level than the RTCPCompoundPacketBuilder.
 *  The class RTCPPacketBuilder can be used to build RTCP compound packets. This class is more high-level
 *  than the RTCPCompoundPacketBuilder class: it uses the information of an RTPPacketBuilder instance and of 
 *  an RTPSources instance to automatically generate the next compound packet which should be sent. It also 
 *  provides functions to determine when SDES items other than the CNAME item should be sent.
 */
class RTCPPacketBuilder /*: public RTPMemoryObject */ {
public:
	/** Creates an RTCPPacketBuilder instance. 
	 *  Creates an instance which will use the source table sources and the RTP packet builder 
	 *  rtppackbuilder to determine the information for the next RTCP compound packet. Optionally,
	 *  the memory manager mgr can be installed.
	 */
	RTCPPacketBuilder(RTPSources &sources,RTPPacketBuilder &rtppackbuilder /*, RTPMemoryManager *mgr=0*/);
	~RTCPPacketBuilder();
	/** Initializes the builder.
	 *  Initializes the builder to use the maximum allowed packet size maxpacksize, timestamp unit 
	 *  timestampunit and the SDES CNAME item specified by cname with length cnamelen. 
	 *  The timestamp unit is defined as a time interval divided by the timestamp interval corresponding to 
	 *  that interval: for 8000 Hz audio this would be 1/8000.
	 */
	int Init(size_t maxpacksize,double timestampunit,const void *cname,size_t cnamelen);
	/** Cleans up the builder. */
	void Destroy();
	int BuildNextPacket(RTCPCompoundPacket **pack);
	int BuildBYEPacket(RTCPCompoundPacket **pack,const void *reason,size_t reasonlength,bool useSRifpossible = true);
private:
	void ClearAllSourceFlags();
	int FillInReportBlocks(RTCPCompoundPacketBuilder *pack,const RTPTime &curtime,int maxcount,bool *full,int *added,int *skipped,bool *atendoflist);
	int FillInSDES(RTCPCompoundPacketBuilder *pack,bool *full,bool *processedall,int *added);
	void ClearAllSDESFlags();

	double timestampunit;
	bool init;
	size_t maxpacketsize;
	int interval_name,interval_email,interval_location;
	int interval_phone,interval_tool,interval_note;
	RTPSources &sources;
	RTPPacketBuilder &rtppacketbuilder;
	bool firstpacket;
	RTPTime prevbuildtime,transmissiondelay;

	RTCPSDESInfoInternal ownsdesinfo;
	bool doname,doemail,doloc,dophone,dotool,donote;
	bool processingsdes;
	int sdesbuildcount;
	};

/** Represents an RTCP compound packet. */
class RTCPCompoundPacket {
public:
	/** Creates an RTCPCompoundPacket instance from the data in rawpack, installing a memory manager if specified. */
	RTCPCompoundPacket(RTPRawPacket &rawpack);

	/** Creates an RTCPCompoundPacket instance from the data in packet}, with size len.
	 *  Creates an RTCPCompoundPacket instance from the data in packet}, with size len. The deletedata
	 *  flag specifies if the data in packet should be deleted when the compound packet is destroyed. If
	 *  specified, a memory manager will be installed.
	 */
	RTCPCompoundPacket(BYTE *packet, size_t len, bool deletedata = true);
protected:
	RTCPCompoundPacket(); // this is for the compoundpacket builder
public:
	virtual ~RTCPCompoundPacket();

	/** Checks if the RTCP compound packet was created successfully.
	 *  If the raw packet data in the constructor could not be parsed, this function returns the error code of
	 *  what went wrong. If the packet had an invalid format, the return value is ERR_RTP_RTCPCOMPOUND_INVALIDPACKET.
	 */
	int GetCreationError()									{ return error; }

	/** Returns a pointer to the data of the entire RTCP compound packet. */
	BYTE *GetCompoundPacketData()						{ return compoundpacket; }

	/** Returns the size of the entire RTCP compound packet. */
	size_t GetCompoundPacketLength()						{ return compoundpacketlength; }

	/** Starts the iteration over the individual RTCP packets in the RTCP compound packet. */
	void GotoFirstPacket()				{ rtcppackit = rtcppacklist.GetHeadPosition(); }

	/** Returns a pointer to the next individual RTCP packet. 
	 *  Returns a pointer to the next individual RTCP packet. Note that no delete call may be done 
	 *  on the RTCPPacket instance which is returned.
	 */
	RTCPPacket *GetNextPacket()	{ 
		if(rtcppackit == rtcppacklist.GetTailPosition()) return NULL; RTCPPacket *p = rtcppacklist.GetAt(rtcppackit); rtcppacklist.GetNext(rtcppackit); return p; }

#ifdef RTPDEBUG
	void Dump();	
#endif // RTPDEBUG
protected:
	void ClearPacketList();
	int ParseData(BYTE *packet, size_t len);
	
	int error;

	BYTE *compoundpacket;
	size_t compoundpacketlength;
	bool deletepacket;
	
	CList<RTCPPacket *,RTCPPacket *> rtcppacklist;
	POSITION rtcppackit;
	};

/** Describes an RTCP sender report packet. */
class RTCPSRPacket : public RTCPPacket {
public:
	/** Creates an instance based on the data in data with length datalen. 
	 *  Creates an instance based on the data in data with length datalen. Since the data pointer
	 *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it 
	 *  points to is valid as long as the class instance exists.
	 */
	RTCPSRPacket(BYTE *data,size_t datalength);
	~RTCPSRPacket()								{ }

	/** Returns the SSRC of the participant who sent this packet. */
	DWORD GetSenderSSRC() const;

	/** Returns the NTP timestamp contained in the sender report. */
	RTPNTPTime GetNTPTimestamp() const;

	/** Returns the RTP timestamp contained in the sender report. */
	DWORD GetRTPTimestamp() const;

	/** Returns the sender's packet count contained in the sender report. */
	DWORD GetSenderPacketCount() const;

	/** Returns the sender's octet count contained in the sender report. */
	DWORD GetSenderOctetCount() const;

	/** Returns the number of reception report blocks present in this packet. */
	int GetReceptionReportCount() const;

	/** Returns the SSRC of the reception report block described by index which may have a value 
	 *  from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is 
	 *  valid).
	 */
	DWORD GetSSRC(int index) const;

	/** Returns the `fraction lost' field of the reception report described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	BYTE GetFractionLost(int index) const;

	/** Returns the number of lost packets in the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	int GetLostPacketCount(int index) const;

	/** Returns the extended highest sequence number of the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	DWORD GetExtendedHighestSequenceNumber(int index) const;

	/** Returns the jitter field of the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	DWORD GetJitter(int index) const;

	/** Returns the LSR field of the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	DWORD GetLSR(int index) const;

	/** Returns the DLSR field of the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	DWORD GetDLSR(int index) const;

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	RTCPReceiverReport *GotoReport(int index) const;
	};

inline DWORD RTCPSRPacket::GetSenderSSRC() const {

	if(!knownformat)
		return 0;
	
	DWORD *ssrcptr = (DWORD *)(data+sizeof(RTCPCommonHeader));
	return ntohl(*ssrcptr);
	}

inline RTPNTPTime RTCPSRPacket::GetNTPTimestamp() const {

	if(!knownformat)
		return RTPNTPTime(0,0);

	RTCPSenderReport *sr = (RTCPSenderReport *)(data+sizeof(RTCPCommonHeader)+sizeof(DWORD));
	return RTPNTPTime(ntohl(sr->ntptime_msw),ntohl(sr->ntptime_lsw));
}

inline DWORD RTCPSRPacket::GetRTPTimestamp() const {

	if(!knownformat)
		return 0;
	RTCPSenderReport *sr = (RTCPSenderReport *)(data+sizeof(RTCPCommonHeader)+sizeof(DWORD));
	return ntohl(sr->rtptimestamp);
}

inline DWORD RTCPSRPacket::GetSenderPacketCount() const {

	if(!knownformat)
		return 0;
	RTCPSenderReport *sr = (RTCPSenderReport *)(data+sizeof(RTCPCommonHeader)+sizeof(DWORD));
	return ntohl(sr->packetcount);
}
	
inline DWORD RTCPSRPacket::GetSenderOctetCount() const {

	if(!knownformat)
		return 0;
	RTCPSenderReport *sr = (RTCPSenderReport *)(data+sizeof(RTCPCommonHeader)+sizeof(DWORD));
	return ntohl(sr->octetcount);
}

inline int RTCPSRPacket::GetReceptionReportCount() const {

	if(!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return ((int)hdr->count);
}

inline RTCPReceiverReport *RTCPSRPacket::GotoReport(int index) const {

	RTCPReceiverReport *r = (RTCPReceiverReport *)(data+sizeof(RTCPCommonHeader)+sizeof(DWORD)+sizeof(RTCPSenderReport)+index*sizeof(RTCPReceiverReport));
	return r;
}

inline DWORD RTCPSRPacket::GetSSRC(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->ssrc);
}

inline BYTE RTCPSRPacket::GetFractionLost(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return r->fractionlost;
}

inline int RTCPSRPacket::GetLostPacketCount(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	DWORD count = ((DWORD)r->packetslost[2])|(((DWORD)r->packetslost[1])<<8)|(((DWORD)r->packetslost[0])<<16);
	if((count & 0x00800000) != 0) // test for negative number
		count |= 0xFF000000;
	int *count2 = (int*)(&count);
	return (*count2);
	}

inline DWORD RTCPSRPacket::GetExtendedHighestSequenceNumber(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->exthighseqnr);
}

inline DWORD RTCPSRPacket::GetJitter(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->jitter);
}

inline DWORD RTCPSRPacket::GetLSR(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->lsr);
}

inline DWORD RTCPSRPacket::GetDLSR(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->dlsr);
}

/** Describes an RTCP receiver report packet. */
class RTCPRRPacket : public RTCPPacket {
public:
	/** Creates an instance based on the data in data with length datalen. 
	 *  Creates an instance based on the data in data with length datalen. Since the data pointer
	 *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it points 
	 *  to is valid as long as the class instance exists.
	 */
	RTCPRRPacket(BYTE *data,size_t datalen);
	~RTCPRRPacket()								{ }
	
	/** Returns the SSRC of the participant who sent this packet. */
	DWORD GetSenderSSRC() const;
	/** Returns the number of reception report blocks present in this packet. */
	int GetReceptionReportCount() const;
	/** Returns the SSRC of the reception report block described by index which may have a value 
	 *  from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is 
	 *  valid).
	 */
	DWORD GetSSRC(int index) const;
	/** Returns the `fraction lost' field of the reception report described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	BYTE GetFractionLost(int index) const;
	/** Returns the number of lost packets in the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	int GetLostPacketCount(int index) const;

	/** Returns the extended highest sequence number of the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	DWORD GetExtendedHighestSequenceNumber(int index) const;

	/** Returns the jitter field of the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	DWORD GetJitter(int index) const;

	/** Returns the LSR field of the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	DWORD GetLSR(int index) const;

	/** Returns the DLSR field of the reception report block described by index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if index is
	 *  valid).
	 */
	DWORD GetDLSR(int index) const;


#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	RTCPReceiverReport *GotoReport(int index) const;
	};

inline DWORD RTCPRRPacket::GetSenderSSRC() const {

	if(!knownformat)
		return 0;
	
	DWORD *ssrcptr = (DWORD *)(data+sizeof(RTCPCommonHeader));
	return ntohl(*ssrcptr);
	}
inline int RTCPRRPacket::GetReceptionReportCount() const {

	if(!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return ((int)hdr->count);
	}

inline RTCPReceiverReport *RTCPRRPacket::GotoReport(int index) const {

	RTCPReceiverReport *r = (RTCPReceiverReport *)(data+sizeof(RTCPCommonHeader)+sizeof(DWORD)+index*sizeof(RTCPReceiverReport));
	return r;
	}

inline DWORD RTCPRRPacket::GetSSRC(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->ssrc);
	}

inline BYTE RTCPRRPacket::GetFractionLost(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return r->fractionlost;
	}

inline int RTCPRRPacket::GetLostPacketCount(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	DWORD count = ((DWORD)r->packetslost[2])|(((DWORD)r->packetslost[1])<<8)|(((DWORD)r->packetslost[0])<<16);
	if((count & 0x00800000) != 0) // test for negative number
		count |= 0xFF000000;
	int *count2 = (int *)(&count);
	return (*count2);
	}

inline DWORD RTCPRRPacket::GetExtendedHighestSequenceNumber(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->exthighseqnr);
	}

inline DWORD RTCPRRPacket::GetJitter(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->jitter);
	}

inline DWORD RTCPRRPacket::GetLSR(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->lsr);
	}

inline DWORD RTCPRRPacket::GetDLSR(int index) const {

	if(!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->dlsr);
	}

/** Describes an RTCP APP packet. */
class RTCPAPPPacket : public RTCPPacket {
public:
	/** Creates an instance based on the data in data with length datalen. 
	 *  Creates an instance based on the data in data with length datalen. Since the data pointer
	 *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it 
	 *  points to is valid as long as the class instance exists.
	 */
	RTCPAPPPacket(BYTE *data,size_t datalen);
	~RTCPAPPPacket()							{ }

	/** Returns the subtype contained in the APP packet. */
	BYTE GetSubType() const;

	/** Returns the SSRC of the source which sent this packet. */
	DWORD GetSSRC() const;

	/** Returns the name contained in the APP packet.
	 *  Returns the name contained in the APP packet. This alway consists of four bytes and is not NULL-terminated.
	 */
	BYTE *GetName(); 

	/** Returns a pointer to the actual data. */
	BYTE *GetAPPData();

	/** Returns the length of the actual data. */
	size_t GetAPPDataLength() const;
#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG	
private:
	size_t appdatalen;
	};

inline BYTE RTCPAPPPacket::GetSubType() const {
	if(!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return hdr->count;
	}

inline DWORD RTCPAPPPacket::GetSSRC() const {
	if(!knownformat)
		return 0;

	DWORD *ssrc = (DWORD *)(data+sizeof(RTCPCommonHeader));
	return ntohl(*ssrc);	
	}

inline BYTE *RTCPAPPPacket::GetName() {

	if(!knownformat)
		return 0;

	return (data+sizeof(RTCPCommonHeader)+sizeof(DWORD));	
	}

inline BYTE *RTCPAPPPacket::GetAPPData() {

	if(!knownformat)
		return 0;
	if(appdatalen == 0)
		return 0;
	return (data+sizeof(RTCPCommonHeader)+sizeof(DWORD)*2);
	}

inline size_t RTCPAPPPacket::GetAPPDataLength() const {

	if(!knownformat)
		return 0;
	return appdatalen;
	}

class RTCPCompoundPacketBuilder : public RTCPCompoundPacket {
public:
	/** Constructs an RTCPCompoundPacketBuilder instance, optionally installing a memory manager. */
	RTCPCompoundPacketBuilder();
	~RTCPCompoundPacketBuilder();

	/** 
	 *  Starts building an RTCP compound packet with maximum size maxpacketsize. New memory will be allocated 
	 *  to store the packet.
	 */
	int InitBuild(size_t maxpacketsize);

	/** 
	 *  Starts building a RTCP compound packet. Data will be stored in externalbuffer which
	 *  can contain buffersize bytes.
	 */
	int InitBuild(void *externalbuffer,size_t buffersize);
	
	/** Adds a sender report to the compound packet.
	 *  Tells the packet builder that the packet should start with a sender report which will contain
	 *  the sender information specified by this function's arguments. Once the sender report is started,
	 *  report blocks can be added using the AddReportBlock function.
	 */
	int StartSenderReport(DWORD senderssrc,const RTPNTPTime &ntptimestamp,DWORD rtptimestamp,
	                    DWORD packetcount,DWORD octetcount);

	/** Adds a receiver report to the compound packet.
	 *  Tells the packet builder that the packet should start with a receiver report which will contain
	 *  he sender SSRC senderssrc. Once the sender report is started, report blocks can be added using the
	 *  AddReportBlock function.
	 */
	int StartReceiverReport(DWORD senderssrc);

	/** Adds the report block information specified by the function's arguments.
	 *  Adds the report block information specified by the function's arguments. If more than 31 report blocks
	 *  are added, the builder will automatically use a new RTCP receiver report packet.
	 */
	int AddReportBlock(DWORD ssrc,BYTE fractionlost,int packetslost,DWORD exthighestseq,
	                   DWORD jitter,DWORD lsr,DWORD dlsr);
	
	/** Starts an SDES chunk for participant ssrc. */
	int AddSDESSource(DWORD ssrc);

	/** Adds a normal (non-private) SDES item of type t to the current SDES chunk.
	 *  Adds a normal (non-private) SDES item of type t to the current SDES chunk. The item's value
	 *  will have length itemlength and will contain the data itemdata.
	 */
	int AddSDESNormalItem(RTCPSDESPacket::ItemType t,const void *itemdata,BYTE itemlength);
#ifdef RTP_SUPPORT_SDESPRIV
	/** Adds an SDES PRIV item described by the function's arguments to the current SDES chunk. */
	int AddSDESPrivateItem(const void *prefixdata,BYTE prefixlength,const void *valuedata,
	                       BYTE valuelength);
#endif // RTP_SUPPORT_SDESPRIV

	/** Adds a BYE packet to the compound packet.
	 *  Adds a BYE packet to the compound packet. It will contain numssrcs source identifiers specified in
	 *  ssrcs and will indicate as reason for leaving the string of length reasonlength 
	 *  containing data reasondata.
	 */
	int AddBYEPacket(DWORD *ssrcs,BYTE numssrcs,const void *reasondata,BYTE reasonlength);

	/** Adds the APP packet specified by the arguments to the compound packet.
	 *  Adds the APP packet specified by the arguments to the compound packet. Note that appdatalen has to be
	 *  a multiple of four.
	 */
	int AddAPPPacket(BYTE subtype,DWORD ssrc,const BYTE name[4],const void *appdata,size_t appdatalen);

	/** Finishes building the compound packet.
	 *  Finishes building the compound packet. If successful, the RTCPCompoundPacket member functions
	 *  can be used to access the RTCP packet data.
	 */
	int EndBuild();

#ifdef RTP_SUPPORT_RTCPUNKNOWN
	/** Adds the RTCP packet specified by the arguments to the compound packet.
	 *  Adds the RTCP packet specified by the arguments to the compound packet.
	 */
	int AddUnknownPacket(BYTE payload_type, BYTE subtype, DWORD ssrc, const void *data, size_t len);
#endif // RTP_SUPPORT_RTCPUNKNOWN 
private:
	class Buffer {
	public:
		Buffer() : packetdata(0),packetlength(0) { }
		Buffer(BYTE *data,size_t len) : packetdata(data),packetlength(len) { }			
		
		BYTE *packetdata;
		size_t packetlength;
		};

	class Report {
	public:
		Report() { 
			headerdata = (BYTE *)headerdata32; 
			isSR = false; 
			headerlength = 0; 
			}
		~Report() { Clear(); }

		void Clear() {
			for(POSITION pos = reportblocks.GetHeadPosition(); pos;)
				delete reportblocks.GetNext(pos).packetdata;
			reportblocks.RemoveAll();
			isSR = false;
			headerlength = 0;
			}

		size_t NeededBytes() { 
			size_t x,n,d,r; 
			n = reportblocks.GetCount(); 
			if(n == 0)	{
				if(headerlength == 0)
					return 0;
				x = sizeof(RTCPCommonHeader)+headerlength;
				}
			else {
				x = n*sizeof(RTCPReceiverReport);
				d = n / 31; // max 31 reportblocks per report
				r = n % 31;
				if(r != 0)
					d++;
				x += d*(sizeof(RTCPCommonHeader)+sizeof(DWORD)); /* header and SSRC */
				if(isSR)
					x += sizeof(RTCPSenderReport);
				}
			return x;
			}			

		size_t NeededBytesWithExtraReportBlock() {
			size_t x,n,d,r; 
			n = reportblocks.GetCount() + 1; // +1 for the extra block
			x = n*sizeof(RTCPReceiverReport);
			d = n / 31; // max 31 reportblocks per report
			r = n % 31;
			if(r != 0)
				d++;
			x += d*(sizeof(RTCPCommonHeader)+sizeof(DWORD)); /* header and SSRC */
			if(isSR)
				x += sizeof(RTCPSenderReport);
			return x;
			}
		
		bool isSR;

		BYTE *headerdata;
		DWORD headerdata32[(sizeof(DWORD)+sizeof(RTCPSenderReport))/sizeof(DWORD)]; // either for ssrc and sender info or just ssrc
		size_t headerlength;
		CList<Buffer,Buffer> reportblocks;
		};

	class SDESSource {
	public:
//		SDESSource(DWORD s) ssrc(s),totalitemsize(0)  { }
		SDESSource(DWORD s) { ssrc=s; totalitemsize=0; }
		~SDESSource()	{
			for(POSITION pos = items.GetHeadPosition(); pos;)
				delete items.GetNext(pos).packetdata;
			items.RemoveAll();
			}

		size_t NeededBytes() {
			size_t x,r;
			x = totalitemsize + 1; // +1 for the 0 byte which terminates the item list
			r = x % sizeof(DWORD);
			if(r != 0)
				x += (sizeof(DWORD)-r); // make sure it ends on a 32 bit boundary
			x += sizeof(DWORD); // for ssrc
			return x;
			}

		size_t NeededBytesWithExtraItem(BYTE itemdatalength) {
			size_t x,r;
			x = totalitemsize + sizeof(RTCPSDESHeader) + (size_t)itemdatalength + 1;
			r = x % sizeof(DWORD);
			if(r != 0)
				x += (sizeof(DWORD)-r); // make sure it ends on a 32 bit boundary
			x += sizeof(DWORD); // for ssrc
			return x;
			}
		
		void AddItem(BYTE *buf,size_t len) {
			Buffer b(buf,len);
			totalitemsize += len;
			items.AddTail(b);	
			}
		
		DWORD ssrc;
		CList<Buffer,Buffer> items;
	private:
		size_t totalitemsize;
	};
	
	class SDES {
	public:
		SDES() { sdesit = sdessources.GetTailPosition(); }
		~SDES() { Clear(); }

		void Clear() {
			for(POSITION pos = sdessources.GetHeadPosition(); pos;)
				delete sdessources.GetNext(pos);
			sdessources.RemoveAll();
			}

		int AddSSRC(DWORD ssrc)	{
			SDESSource *s = new SDESSource(ssrc);
			if(!s)
				return ERR_RTP_OUTOFMEM;
			sdessources.AddTail(s);
			sdesit = sdessources.GetTailPosition();
//			sdessources.GetPrev(sdesit);
			return 0;
			}

		int AddItem(BYTE *buf,size_t len)	{
			if(sdessources.IsEmpty())
				return ERR_RTP_RTCPCOMPPACKBUILDER_NOCURRENTSOURCE;
			sdessources.GetAt(sdesit)->AddItem(buf,len);
			return 0;
			}

		size_t NeededBytes() {
			size_t x = 0;
			size_t n,d,r;
			
			if(sdessources.IsEmpty())
				return 0;
			
			for(POSITION pos = sdessources.GetHeadPosition(); pos;)
				x += sdessources.GetNext(pos)->NeededBytes();
			n = sdessources.GetCount();
			d = n / 31;
			r = n % 31;
			if(r != 0)
				d++;
			x += d*sizeof(RTCPCommonHeader);
			return x;
			}
		
		size_t NeededBytesWithExtraItem(BYTE itemdatalength) {
			size_t x = 0;
			size_t n,d,r;
			
			if(sdessources.IsEmpty())
				return 0;
			
			for(POSITION pos = sdessources.GetHeadPosition(); pos;)
				x += sdessources.GetNext(pos)->NeededBytes();
			x += sdessources.GetAt(sdesit)->NeededBytesWithExtraItem(itemdatalength);
			n = sdessources.GetCount();
			d = n / 31;
			r = n % 31;
			if(r != 0)
				d++;
			x += d*sizeof(RTCPCommonHeader);
			return x;
			}

		size_t NeededBytesWithExtraSource()	{
			size_t x = 0;
			size_t n,d,r;
			
			if(sdessources.IsEmpty())
				return 0;
			
			for(POSITION pos = sdessources.GetHeadPosition(); pos;)
				x += sdessources.GetNext(pos)->NeededBytes();
			
			// for the extra source we'll need at least 8 bytes (ssrc and four 0 bytes)
			x += sizeof(DWORD)*2;
			
			n = sdessources.GetCount() + 1; // also, the number of sources will increase
			d = n/31;
			r = n%31;
			if(r != 0)
				d++;
			x += d*sizeof(RTCPCommonHeader);
			return x;
		}
		
		CList<SDESSource *,SDESSource *> sdessources;
	private:
		POSITION sdesit;
	};

	size_t maximumpacketsize;
	BYTE *buffer;
	bool external;
	bool arebuilding;
	
	Report report;
	SDES sdes;

	CList<Buffer,Buffer> byepackets;
	size_t byesize;
	
	CList<Buffer,Buffer> apppackets;
	size_t appsize;

#ifdef RTP_SUPPORT_RTCPUNKNOWN
	CList<Buffer,Buffer> unknownpackets;
	size_t unknownsize;
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	
	void ClearBuildBuffers();
	};

class RTCPSchedulerParams {
public:
  RTCPSchedulerParams();
  ~RTCPSchedulerParams();
  int SetRTCPBandwidth(double bw);
  double GetRTCPBandwidth() const { return bandwidth; }
  int SetSenderBandwidthFraction(double fraction);
  double GetSenderBandwidthFraction() const { return senderfraction; }
  int SetMinimumTransmissionInterval(const RTPTime &t);
  RTPTime GetMinimumTransmissionInterval() const { return mininterval; }
  void SetUseHalfAtStartup(bool usehalf) { usehalfatstartup = usehalf; }
  bool GetUseHalfAtStartup() const { return usehalfatstartup; }
  void SetRequestImmediateBYE(bool v) { immediatebye = v; }
  bool GetRequestImmediateBYE() const { return immediatebye; }
 private:
  double bandwidth;
  double senderfraction;
  RTPTime mininterval;
  bool usehalfatstartup;
  bool immediatebye;
	};

/** This class determines when RTCP compound packets should be sent. */
class RTCPScheduler {
public:
	/** Creates an instance which will use the source table RTPSources to determine when RTCP compound 
	 *  packets should be scheduled. 
	 *  Creates an instance which will use the source table RTPSources to determine when RTCP compound 
	 *  packets should be scheduled. Note that for correct operation the sources instance should have information
	 *  about the own SSRC (added by RTPSources::CreateOwnSSRC). You must also supply a random number
	 *  generator rtprand which will be used for adding randomness to the RTCP intervals.
	 */
	RTCPScheduler(RTPSources &sources, /*RTPRandom &*/ int rtprand);
	~RTCPScheduler();
	/** Resets the scheduler. */
	void Reset();
	/** Sets the header overhead from underlying protocols (for example UDP and IP) to numbytes. */
	void SetHeaderOverhead(size_t numbytes)								{ headeroverhead = numbytes; }
	/** Returns the currently used header overhead. */
	size_t GetHeaderOverhead() const								{ return headeroverhead; }
	/** Sets the scheduler parameters to be used to params. */
	void SetParameters(const RTCPSchedulerParams &params)						{ schedparams = params; }
	/** Returns the currently used scheduler parameters. */
	RTCPSchedulerParams GetParameters() const							{ return schedparams; }
	void ScheduleBYEPacket(size_t packetsize);
	bool IsTime();
	RTPTime CalculateDeterministicInterval(bool sender = false);
	RTPTime CalculateTransmissionInterval(bool sender);
	void AnalyseOutgoing(RTCPCompoundPacket &rtcpcomppack);
private:
	void CalculateNextRTCPTime();
	RTPTime CalculateBYETransmissionInterval();

	RTPSources &sources;
	RTCPSchedulerParams schedparams;
	size_t headeroverhead;
	size_t avgrtcppacksize;
	bool hassentrtcp;
	bool firstcall;
	bool byescheduled;
	int byemembers,pbyemembers;
	size_t avgbyepacketsize;
	bool sendbyenow;
	RTPTime nextrtcptime;
	RTPTime prevrtcptime;
	int pmembers;
	/*RTPRandom &*/ int rtprand;
	};

/** This class represents a list of addresses from which SSRC collisions were detected. */
class RTPCollisionList /*: public RTPMemoryObject */ {
public:
	/** Constructs an instance, optionally installing a memory manager. */
	RTPCollisionList(/*RTPMemoryManager *mgr=0*/);
	~RTPCollisionList()		{ Clear(); }
	int UpdateAddress(const RTPAddress *,const RTPTime &,bool *);
	void Timeout(const RTPTime &,const RTPTime &);
	/** Clears the list of addresses. */
	void Clear();
private:
	class AddressAndTime {
		public:
			AddressAndTime() {}
			AddressAndTime(RTPAddress *a,const RTPTime &t) : addr(a),recvtime(t) { }
			RTPAddress *addr;
			RTPTime recvtime;
		};
	CList <class AddressAndTime,class AddressAndTime> addresslist;
	};

class RTCPCompoundPacket;	
class RTPSession /*: public RTPMemoryObject*/ {
public:
	/** Constructs an RTPSession instance, optionally using a specific instance of a random
	 *  number generator, and optionally installing a memory manager. 
	 *  Constructs an RTPSession instance, optionally using a specific instance of a random
	 *  number generator, and optionally installing a memory manager. If no random number generator
	 *  is specified, the RTPSession object will try to use either a RTPRandomURandom or 
	 *  RTPRandomRandS instance. If neither is available on the current platform, a RTPRandomRand48
	 *  instance will be used instead. By specifying a random number generator yourself, it is
	 *  possible to use the same generator in several RTPSession instances.
	 */
	RTPSession(/*RTPRandom * */ int rnd=0 /*, RTPMemoryManager *mgr=0*/);
	virtual ~RTPSession();
	/** Creates an RTP session.
	 *  This function creates an RTP session with parameters sessparams, which will use a transmitter 
	 *  corresponding to proto. Parameters for this transmitter can be specified as well. If \c
	 *  proto is of type RTPTransmitter::UserDefinedProto, the NewUserDefinedTransmitter function must
	 *  be implemented.
	 */
	int Create(const RTPSessionParams &sessparams, const RTPTransmissionParams *transparams = 0, RTPTransmitter::TransmissionProtocol proto = RTPTransmitter::IPv4UDPProto);
	/** Creates an RTP session using transmitter as transmission component.
	 *  This function creates an RTP session with parameters sessparams, which will use the
	 *  transmission component transmitter. Initialization and destruction of the transmitter
	 *  will not be done by the RTPSession class if this Create function is used. This function
	 *  can be useful if you which to reuse the transmission component in another RTPSession
	 *  instance, once the original RTPSession isn't using the transmitter anymore.
	 */
	int Create(const RTPSessionParams &sessparams, RTPTransmitter *transmitter);
	/** Leaves the session without sending a BYE packet. */
	void Destroy();
		/** Adds addr to the list of destinations. */
	int AddDestination(const RTPAddress &addr);
	/** Sends a BYE packet and leaves the session. 
	 *  Sends a BYE packet and leaves the session. At most a time maxwaittime will be waited to 
	 *  send the BYE packet. If this time expires, the session will be left without sending a BYE packet. 
	 *  The BYE packet will contain as reason for leaving reason with length reasonlength.
	 */
	void ClearDestinations();
	int DeleteDestination(const RTPAddress &addr);
	void BYEDestroy(const RTPTime &maxwaittime,const void *reason,size_t reasonlength);
	/** If you're not using the poll thread, this function must be called regularly to process incoming data
	 *  and to send RTCP data when necessary.
	 */
	int Poll();
	/** The following member functions (till EndDataAccess}) need to be accessed between a call 
	 *  to BeginDataAccess and EndDataAccess. 
	 *  The BeginDataAccess function makes sure that the poll thread won't access the source table
	 *  at the same time that you're using it. When the EndDataAccess is called, the lock on the 
	 *  source table is freed again.
	 */
	int BeginDataAccess();
	/** Frees the memory used by p. */
	void DeletePacket(RTPPacket *p);
	/** See BeginDataAccess. */
	int EndDataAccess();
	/** Sets the current source to be the first source in the table which has RTPPacket instances 
	 *  that we haven't extracted yet. 
	 *  Sets the current source to be the first source in the table which has RTPPacket instances 
	 *  that we haven't extracted yet. If no such member was found, the function returns false,
	 *  otherwise it returns true.
	 */
	bool GotoFirstSourceWithData();
	int ProcessPolledData();
	/** Extracts the next packet from the received packets queue of the current participant,
	 *  or NULL if no more packets are available.
	 *  Extracts the next packet from the received packets queue of the current participant,
	 *  or NULL if no more packets are available. When the packet is no longer needed, its
	 *  memory should be freed using the DeletePacket member function.
	 */
	RTPPacket *GetNextPacket();
	int CreateCNAME(BYTE *buffer, size_t *bufferlength, bool resolve);
	int SendRTCPData(const void *data, size_t len);

	virtual void OnNewSource(RTPSourceData *dat) {}
	virtual void OnBYEPacket(RTPSourceData *dat) {}
	virtual void OnSendRTCPCompoundPacket(RTCPCompoundPacket *pack);
	virtual void OnRemoveSource(RTPSourceData *dat) {}
	// void ProcessRTPPacket(const RTPSourceData &srcdat,const RTPPacket &rtppack);
  virtual void OnPollThreadError(int) {}
  virtual void OnPollThreadStep() {}
  virtual void OnPollThreadStart(bool &) {}
  virtual void OnPollThreadStop() {}
	void SetChangeOutgoingData(bool change)							{ m_changeOutgoingData = change; }
	void SetChangeIncomingData(bool change)							{ m_changeIncomingData = change; }
	virtual int OnChangeRTPOrRTCPData(const void *origdata, size_t origlen, bool isrtp, void **senddata, size_t *sendlen);
	virtual void OnSentRTPOrRTCPData(void *senddata, size_t sendlen, bool isrtp);
	virtual bool OnChangeIncomingData(RTPRawPacket *rawpack);
	virtual void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled);

protected:
	int InternalCreate(const RTPSessionParams &sessparams);
public:
	RTPTransmitter *rtptrans;
	bool created;
	bool deletetransmitter;
	bool usingpollthread, needthreadsafety;
	bool useSR_BYEifpossible;
	bool sentpackets;
	size_t maxpacksize;
	double sessionbandwidth;
	double controlfragment;
	double sendermultiplier;
	double byemultiplier;
	bool acceptownpackets;
	double membermultiplier;
	double collisionmultiplier;
	double notemultiplier;
	bool m_changeIncomingData, m_changeOutgoingData;
	int rndseed;		// GC 2020

	RTPSessionSources sources;
	RTPPacketBuilder packetbuilder;
	RTCPScheduler rtcpsched;
	RTCPPacketBuilder rtcpbuilder;
	RTPCollisionList collisionlist;

	CList<RTCPCompoundPacket *,RTCPCompoundPacket *> byepackets;

	};

//inline RTPTransmitter *RTPSession::NewUserDefinedTransmitter()                                          { return 0; }
//inline void RTPSession::OnRTPPacket(RTPPacket *, const RTPTime &, const RTPAddress *)                   { }
//inline void RTPSession::OnRTCPCompoundPacket(RTCPCompoundPacket *, const RTPTime &, const RTPAddress *) { }
//inline void RTPSession::OnSSRCCollision(RTPSourceData *, const RTPAddress *, bool )                     { }
//inline void RTPSession::OnCNAMECollision(RTPSourceData *, const RTPAddress *, const BYTE *, size_t ) { }
//inline void RTPSession::OnTimeout(RTPSourceData *)                                                      { }
//inline void RTPSession::OnBYETimeout(RTPSourceData *)                                                   { }
//inline void RTPSession::OnAPPPacket(RTCPAPPPacket *, const RTPTime &, const RTPAddress *)               { }
//inline void RTPSession::OnUnknownPacketType(RTCPPacket *, const RTPTime &, const RTPAddress *)          { }
//inline void RTPSession::OnUnknownPacketFormat(RTCPPacket *, const RTPTime &, const RTPAddress *)        { }
//inline void RTPSession::OnNoteTimeout(RTPSourceData *)                                                  { }
//inline void RTPSession::OnRTCPSenderReport(RTPSourceData *)                                             { }
//inline void RTPSession::OnRTCPReceiverReport(RTPSourceData *)                                           { }
//inline void RTPSession::OnRTCPSDESItem(RTPSourceData *, RTCPSDESPacket::ItemType, const void *, size_t) { }

#ifdef RTP_SUPPORT_SDESPRIV
inline void RTPSession::OnRTCPSDESPrivateItem(RTPSourceData *, const void *, size_t, const void *, size_t) { }
#endif // RTP_SUPPORT_SDESPRIV

inline void RTPSession::OnSendRTCPCompoundPacket(RTCPCompoundPacket *)                                  { }

#ifdef RTP_SUPPORT_THREAD
inline void RTPSession::OnPollThreadError(int)                                                          { }
inline void RTPSession::OnPollThreadStep()                                                              { }
inline void RTPSession::OnPollThreadStart(bool &)                                                       { }
inline void RTPSession::OnPollThreadStop()                                                              { }
#endif // RTP_SUPPORT_THREAD

inline int RTPSession::OnChangeRTPOrRTCPData(const void *, size_t, bool, void **, size_t *) {
	return ERR_RTP_RTPSESSION_CHANGEREQUESTEDBUTNOTIMPLEMENTED;
	}
inline void RTPSession::OnSentRTPOrRTCPData(void *, size_t, bool)                                       { }
inline bool RTPSession::OnChangeIncomingData(RTPRawPacket *)                                            { return true; }
inline void RTPSession::OnValidatedRTPPacket(RTPSourceData *, RTPPacket *, bool, bool *)                { }


class RTCPSenderReportInfo {
public:
	RTCPSenderReportInfo():ntptimestamp(0,0),receivetime(0,0)		{ hasinfo = false; rtptimestamp = 0; packetcount = 0; bytecount = 0; }
	void Set(const RTPNTPTime &ntptime,DWORD rtptime,DWORD pcount,
	         DWORD bcount,const RTPTime &rcvtime)			{ ntptimestamp = ntptime; rtptimestamp = rtptime; packetcount = pcount; bytecount = bcount; receivetime = rcvtime; hasinfo = true; }
	
	bool HasInfo() const							{ return hasinfo; }
	RTPNTPTime GetNTPTimestamp() const					{ return ntptimestamp; }
	DWORD GetRTPTimestamp() const					{ return rtptimestamp; }
	DWORD GetPacketCount() const						{ return packetcount; }
	DWORD GetByteCount() const						{ return bytecount; }
	RTPTime GetReceiveTime() const						{ return receivetime; }
private:
	bool hasinfo;
	RTPNTPTime ntptimestamp;
	DWORD rtptimestamp;
	DWORD packetcount;
	DWORD bytecount;
	RTPTime receivetime;
	};
class RTCPReceiverReportInfo {
public:
	RTCPReceiverReportInfo():receivetime(0,0)				{ hasinfo = false; fractionlost = 0; packetslost = 0; exthighseqnr = 0; jitter = 0; lsr = 0; dlsr = 0; } 
	void Set(BYTE fraclost,long plost,DWORD exthigh,
	         DWORD jit,DWORD l,DWORD dl,const RTPTime &rcvtime) 	{ fractionlost = ((double)fraclost)/256.0; packetslost = plost; exthighseqnr = exthigh; jitter = jit; lsr = l; dlsr = dl; receivetime = rcvtime; hasinfo = true; }
		
	bool HasInfo() const							{ return hasinfo; }
	double GetFractionLost() const						{ return fractionlost; }
	long GetPacketsLost() const						{ return packetslost; }
	DWORD GetExtendedHighestSequenceNumber() const			{ return exthighseqnr; }
	DWORD GetJitter() const						{ return jitter; }
	DWORD GetLastSRTimestamp() const					{ return lsr; }
	DWORD GetDelaySinceLastSR() const					{ return dlsr; }
	RTPTime GetReceiveTime() const						{ return receivetime; }
private:
	bool hasinfo;
	double fractionlost;
	long packetslost;
	DWORD exthighseqnr;
	DWORD jitter;
	DWORD lsr;
	DWORD dlsr;
	RTPTime receivetime;
	};

class RTPSourceStats {
public:
	RTPSourceStats();
	void ProcessPacket(RTPPacket *pack,const RTPTime &receivetime,double tsunit,bool ownpacket,bool *accept,bool applyprobation,bool *onprobation);

	bool HasSentData() const						{ return sentdata; }
	DWORD GetNumPacketsReceived() const					{ return packetsreceived; }
	DWORD GetBaseSequenceNumber() const					{ return baseseqnr; }
	DWORD GetExtendedHighestSequenceNumber() const			{ return exthighseqnr; }
	DWORD GetJitter() const						{ return jitter; }

	long GetNumPacketsReceivedInInterval() const				{ return numnewpackets; }
	DWORD GetSavedExtendedSequenceNumber() const			{ return savedextseqnr; }
	void StartNewInterval()							{ numnewpackets = 0; savedextseqnr = exthighseqnr; }
	
	void SetLastMessageTime(const RTPTime &t)				{ lastmsgtime = t; }
	RTPTime GetLastMessageTime() const					{ return lastmsgtime; }
	void SetLastRTPPacketTime(const RTPTime &t)				{ lastrtptime = t; }
	RTPTime GetLastRTPPacketTime() const					{ return lastrtptime; }

	void SetLastNoteTime(const RTPTime &t)					{ lastnotetime = t; }
	RTPTime GetLastNoteTime() const						{ return lastnotetime; }
private:
	bool sentdata;
	DWORD packetsreceived;
	DWORD numcycles; // shifted left 16 bits
	DWORD baseseqnr;
	DWORD exthighseqnr,prevexthighseqnr;
	DWORD jitter,prevtimestamp;
	double djitter;
	RTPTime prevpacktime;
	RTPTime lastmsgtime;
	RTPTime lastrtptime;
	RTPTime lastnotetime;
	DWORD numnewpackets;
	DWORD savedextseqnr;
#ifdef RTP_SUPPORT_PROBATION
	WORD prevseqnr;
	int probation;
#endif // RTP_SUPPORT_PROBATION
	};
inline RTPSourceStats::RTPSourceStats():prevpacktime(0,0),lastmsgtime(0,0),lastrtptime(0,0),lastnotetime(0,0) { 
	sentdata = false; 
	packetsreceived = 0; 
	baseseqnr = 0; 
	exthighseqnr = 0; 
	prevexthighseqnr = 0; 
	jitter = 0; 
	numcycles = 0;
	numnewpackets = 0;
	prevtimestamp = 0;
	djitter = 0;
	savedextseqnr = 0;
#ifdef RTP_SUPPORT_PROBATION
	probation = 0; 
	prevseqnr = 0; 
#endif // RTP_SUPPORT_PROBATION
	}

class RTPSourceData /*: public RTPMemoryObject*/ {
protected:
	RTPSourceData(DWORD ssrc /*, RTPMemoryManager *mgr=0*/);
	virtual ~RTPSourceData();
public:
	/** Returns the SSRC identifier for this member. */
	DWORD GetSSRC() const						{ return ssrc; }
	/** Returns true if the participant was added using the RTPSources member function CreateOwnSSRC and
	 *  returns false otherwise.
	 */
	bool IsOwnSSRC() const							{ return ownssrc; }
	/** Returns true if the source identifier is actually a CSRC from an RTP packet. */
	bool IsCSRC() const							{ return iscsrc; }
	/** Returns true if validated RTP packets have been received from this participant. */
	bool INF_HasSentData() const						{ return stats.HasSentData(); }
	/** Returns the total number of received packets from this participant. */
	int INF_GetNumPacketsReceived() const				{ return stats.GetNumPacketsReceived(); }
	/** Returns the base sequence number of this participant. */
	DWORD INF_GetBaseSequenceNumber() const				{ return stats.GetBaseSequenceNumber(); }
	/** Returns the extended highest sequence number received from this participant. */
	DWORD INF_GetExtendedHighestSequenceNumber() const			{ return stats.GetExtendedHighestSequenceNumber(); }
	/** Returns the number of packets received since a new interval was started with INF_StartNewInterval. */
	DWORD INF_GetNumPacketsReceivedInInterval() const			{ return stats.GetNumPacketsReceivedInInterval(); }
	/** Returns the extended sequence number which was stored by the INF_StartNewInterval call. */
	DWORD INF_GetSavedExtendedSequenceNumber() const			{ return stats.GetSavedExtendedSequenceNumber(); }
	/** Returns the current jitter value for this participant. */
	DWORD INF_GetJitter() const						{ return stats.GetJitter(); }
	/** Returns the time at which something was last heard from this member. */
	RTPTime INF_GetLastMessageTime() const					{ return stats.GetLastMessageTime(); }
	/** Returns the time at which the last RTP packet was received. */
	RTPTime INF_GetLastRTPPacketTime() const				{ return stats.GetLastRTPPacketTime(); }
	/** Returns the estimated timestamp unit, calculated from two consecutive sender reports. */
	double INF_GetEstimatedTimestampUnit() const;
	/** Starts a new interval to count received packets in; this also stores the current extended highest sequence 
	 *  number to be able to calculate the packet loss during the interval.
	 */
	void INF_StartNewInterval()						{ stats.StartNewInterval(); }
	/** This function is used by the RTCPPacketBuilder class to mark whether this participant's 
	 *  information has been processed in a report block or not.
	 */
	/** Returns a pointer to the SDES CNAME item of this participant and stores its length in len. */
	BYTE *SDES_GetCNAME(size_t *len) const				{ return SDESinf.GetCNAME(len); }
	/** Returns the time at which the last SDES NOTE item was received. */
	RTPTime INF_GetLastSDESNoteTime() const					{ return stats.GetLastNoteTime(); }
	bool IsSender() const							{ return issender; }
	/** Returns true if the participant is validated, which is the case if a number of 
	 *  consecutive RTP packets have been received or if a CNAME item has been received for 
	 *  this participant.
	 */
	bool IsValidated() const						{ return validated; }
	/** Returns true if the source was validated and had not yet sent a BYE packet. */
	bool IsActive() const							{ if(!validated) return false; if(receivedbye) return false; return true; }
	void SetProcessedInRTCP(bool v)						{ processedinrtcp = v; }
	/** This function is used by the RTCPPacketBuilder class and returns whether this participant 
	 *  has been processed in a report block or not.
	 */
	bool IsProcessedInRTCP() const						{ return processedinrtcp; }
	/** Returns the address from which this participant's RTP packets originate. 
	 *  Returns the address from which this participant's RTP packets originate. If the address has 
	 *  been set and the returned value is NULL, this indicates that it originated from the local 
	 *  participant.
	 */
	const RTPAddress *GetRTPDataAddress() const				{ return rtpaddr; }
	/** Returns the address from which this participant's RTCP packets originate. 
	 *  Returns the address from which this participant's RTCP packets originate. If the address has 
	 *  been set and the returned value is NULL, this indicates that it originated from the local 
	 *  participant.
	 */
	const RTPAddress *GetRTCPDataAddress() const				{ return rtcpaddr; }
	/** Returns a pointer to the SDES note item of this participant and stores its length in len. */                         
	BYTE *SDES_GetNote(size_t *len) const				{ return SDESinf.GetNote(len); }
	RTPPacket *RTPSourceData::GetNextPacket();
	/** Returns true if there are RTP packets which can be extracted. */
	bool HasData() const							{ if(!validated) return false; return packetlist.IsEmpty() ? false : true; }
	/** Returns true if we received a BYE message for this participant and false otherwise. */
	bool ReceivedBYE() const						{ return receivedbye; }
	/** Returns the time at which the BYE packet was received. */
	RTPTime GetBYETime() const						{ return byetime; }
	/** Returns true if an RTCP sender report has been received from this participant. */
	bool SR_HasInfo() const								{ return SRinf.HasInfo(); }
	/** Returns the time at which the last sender report was received. */
	RTPTime SR_GetReceiveTime() const					{ return SRinf.GetReceiveTime(); }
	/** Returns the NTP timestamp contained in the last sender report. */
	RTPNTPTime SR_GetNTPTimestamp() const				{ return SRinf.GetNTPTimestamp(); }
	/** Returns true if the address from which this participant's RTP packets originate has 
	 *  already been set.
	 */
	bool IsRTPAddressSet() const						{ return isrtpaddrset; }
	/** Returns true if the address from which this participant's RTCP packets originate has 
	 * already been set. 
	 */
	bool IsRTCPAddressSet() const						{ return isrtcpaddrset; }
	void FlushPackets();
protected:
	CList<RTPPacket *,RTPPacket *> packetlist;
	RTPAddress *rtpaddr,*rtcpaddr;
	RTCPSenderReportInfo SRinf,SRprevinf;
	RTCPReceiverReportInfo RRinf,RRprevinf;
	RTPSourceStats stats;
	RTCPSDESInfo SDESinf;
	DWORD ssrc;
	double timestampunit;
	bool iscsrc;
	bool validated;
	bool issender;
	bool processedinrtcp;
	bool receivedbye;
	bool ownssrc;
	RTPTime byetime;
	BYTE *byereason;
	size_t byereasonlen;
	bool isrtpaddrset,isrtcpaddrset;
	};

#define RTPINTERNALSOURCEDATA_MAXPROBATIONPACKETS		32
class RTPInternalSourceData : public RTPSourceData {
public:
	RTPInternalSourceData(DWORD ssrc, RTPSources::ProbationType probtype);
	~RTPInternalSourceData();

	int ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,bool *stored, RTPSources *sources);
	void ProcessSenderInfo(const RTPNTPTime &ntptime,DWORD rtptime,DWORD packetcount,
	                       DWORD octetcount,const RTPTime &receivetime)				{ SRprevinf = SRinf; SRinf.Set(ntptime,rtptime,packetcount,octetcount,receivetime); stats.SetLastMessageTime(receivetime); }
	void ProcessReportBlock(BYTE fractionlost,long lostpackets,DWORD exthighseqnr,
	                        DWORD jitter,DWORD lsr,DWORD dlsr,
				const RTPTime &receivetime)						{ RRprevinf = RRinf; RRinf.Set(fractionlost,lostpackets,exthighseqnr,jitter,lsr,dlsr,receivetime); stats.SetLastMessageTime(receivetime); }
	void UpdateMessageTime(const RTPTime &receivetime)						{ stats.SetLastMessageTime(receivetime); }
	int ProcessSDESItem(BYTE sdesid,const BYTE *data,size_t itemlen,const RTPTime &receivetime,bool *cnamecollis);
#ifdef RTP_SUPPORT_SDESPRIV
	int ProcessPrivateSDESItem(const BYTE *prefix,size_t prefixlen,const BYTE *value,size_t valuelen,const RTPTime &receivetime);
#endif // RTP_SUPPORT_SDESPRIV
	int ProcessBYEPacket(const BYTE *reason,size_t reasonlen,const RTPTime &receivetime);
		
	int SetRTPDataAddress(const RTPAddress *a);
	int SetRTCPDataAddress(const RTPAddress *a);

	void ClearSenderFlag()										{ issender = false; }
	void SentRTPPacket()										{ if(!ownssrc) return; RTPTime t = RTPTime::CurrentTime(); issender = true; stats.SetLastRTPPacketTime(t); stats.SetLastMessageTime(t); }
	void SetOwnSSRC()										{ ownssrc = true; validated = true; }
	void SetCSRC()											{ validated = true; iscsrc = true; }
	void ClearNote()										{ SDESinf.SetNote(0,0); }
	
#ifdef RTP_SUPPORT_PROBATION
private:
	RTPSources::ProbationType probationtype;
#endif // RTP_SUPPORT_PROBATION
	};
inline RTPPacket *RTPSourceData::GetNextPacket() {
	if(!validated)
		return 0;

	RTPPacket *p;

	if(packetlist.IsEmpty())
		return 0;
	p = packetlist.GetHead();
	packetlist.RemoveHead();
	return p;
	}
inline void RTPSourceData::FlushPackets() {

	for(POSITION pos = packetlist.GetHeadPosition(); pos; )
		delete packetlist.GetNext(pos);
	packetlist.RemoveAll();
	}
inline int RTPInternalSourceData::SetRTPDataAddress(const RTPAddress *a) {
	if(!a) {
		if(rtpaddr) {
			delete rtpaddr;
			rtpaddr=NULL;
			}
		}
	else {
		RTPAddress *newaddr = a->CreateCopy();
		if(!newaddr)
			return ERR_RTP_OUTOFMEM;
		
		if(rtpaddr && a != rtpaddr)
			delete rtpaddr;
		rtpaddr = newaddr;
		}
	isrtpaddrset = true;
	return 0;
	}

inline int RTPInternalSourceData::SetRTCPDataAddress(const RTPAddress *a) {
	if(!a)	{
		if(rtcpaddr)	{
			delete rtcpaddr;
			rtcpaddr=NULL;
			}
		}
	else {
		RTPAddress *newaddr = a->CreateCopy();
		if(!newaddr)
			return ERR_RTP_OUTOFMEM;
		
		if(rtcpaddr && a != rtcpaddr)
			delete rtcpaddr;
		rtcpaddr = newaddr;
		}
	isrtcpaddrset = true;
	return 0;
	}


class RTPSessionParams {
public:
	RTPSessionParams();
	/** Sets the timestamp unit for our own data.
	 *  Sets the timestamp unit for our own data. The timestamp unit is defined as a time interval in 
	 *  seconds divided by the corresponding timestamp interval. For example, for 8000 Hz audio, the 
	 *  timestamp unit would typically be 1/8000. Since this value is initially set to an illegal value, 
	 *  the user must set this to an allowed value to be able to create a session.
	 */
	void SetOwnTimestampUnit(double tsunit)						{ owntsunit = tsunit; }
	/** If probation support is enabled, this function sets the probation type to be used. */
	void SetProbationType(RTPSources::ProbationType probtype)	{ probationtype = probtype; }
	/** Returns the probation type which will be used (default is RTPSources::ProbationStore). */
	RTPSources::ProbationType GetProbationType() const			{ return probationtype; }
	/** Sets the session bandwidth in bytes per second. */
	void SetSessionBandwidth(double sessbw)						{ sessionbandwidth = sessbw; }
	/** Returns the session bandwidth in bytes per second (default is 10000 bytes per second). */
	double GetSessionBandwidth() const							{ return sessionbandwidth; }
	/** Sets the fraction of the session bandwidth to be used for control traffic. */
	void SetControlTrafficFraction(double frac)					{ controlfrac = frac; }
	/** Returns the fraction of the session bandwidth that will be used for control traffic (default is 5%). */
	double GetControlTrafficFraction() const					{ return controlfrac; }
	/** Sets the minimum fraction of the control traffic that will be used by senders. */
	void SetSenderControlBandwidthFraction(double frac)			{ senderfrac = frac; }
	/** Returns the minimum fraction of the control traffic that will be used by senders (default is 25%). */
	double GetSenderControlBandwidthFraction() const			{ return senderfrac; }
	/** Set the minimal time interval between sending RTCP packets. */
	void SetMinimumRTCPTransmissionInterval(const RTPTime &t)	{ mininterval = t; }
	/** Returns the minimal time interval between sending RTCP packets (default is 5 seconds). */
	RTPTime GetMinimumRTCPTransmissionInterval() const			{ return mininterval; }
	/** If usehalf is set to true, the session will only wait half of the calculated RTCP 
	 *  interval before sending its first RTCP packet.
	 */
	void SetUseHalfRTCPIntervalAtStartup(bool usehalf)			{ usehalfatstartup = usehalf; }
	/** Returns whether the session will only wait half of the calculated RTCP interval before sending its
	 *  first RTCP packet or not (default is true).
	 */
	bool GetUseHalfRTCPIntervalAtStartup() const				{ return usehalfatstartup; }
	/** Returns true if a BYE packet will be sent in an RTCP compound packet which starts with a 
	 *  sender report; if a receiver report will be used, the function returns false (default is true).
	 */
	/** If v is true, the session will send a BYE packet immediately if this is allowed. */
	void SetRequestImmediateBYE(bool v) 						{ immediatebye = v; }
	/** Returns whether the session should send a BYE packet immediately (if allowed) or not (default is true). */
	bool GetRequestImmediateBYE() const							{ return immediatebye; }
	bool GetSenderReportForBYE() const							{ return SR_BYE; }
	/** Returns whether the session should use a poll thread or not (default is true). */
	bool IsUsingPollThread() const								{ return usepollthread; }
	bool NeedThreadSafety() const								{ return m_needThreadSafety; }
	/** Returns the maximum allowed packet size (default is 1400 bytes). */
	size_t GetMaximumPacketSize() const							{ return maxpacksize; }
	/** Sets a flag which indicates if a predefined SSRC identifier should be used. */
	void SetUsePredefinedSSRC(bool f)							{ usepredefinedssrc = f; }
	/** Returns a flag indicating if a predefined SSRC should be used. */
	bool GetUsePredefinedSSRC() const							{ return usepredefinedssrc; }
	/** Sets the SSRC which will be used if RTPSessionParams::GetUsePredefinedSSRC returns true. */
	void SetPredefinedSSRC(DWORD ssrc)						{ predefinedssrc = ssrc; }
	/** Returns the SSRC which will be used if RTPSessionParams::GetUsePredefinedSSRC returns true. */
	DWORD GetPredefinedSSRC() const							{ return predefinedssrc; }
	/** Sets the receive mode to be used by the session. */
	void SetReceiveMode(RTPTransmitter::ReceiveMode recvmode)	{ receivemode = recvmode; }
	/** Sets the receive mode to be used by the session (default is: accept all packets). */
	RTPTransmitter::ReceiveMode GetReceiveMode() const			{ return receivemode; }
	/** Returns the currently set timestamp unit. */
	double GetOwnTimestampUnit() const							{ return owntsunit; }
	/** Forces this string to be used as the CNAME identifier. */
	void SetCNAME(const CString &s)							{ cname = s; }
	/** Returns the currently set CNAME, is blank when this will be generated automatically (the default). */
	CString GetCNAME() const								{ return cname; }
	/** Sets the multiplier to be used when timing out senders. */
	void SetSenderTimeoutMultiplier(double m)					{ sendermultiplier = m; }
	/** Returns the multiplier to be used when timing out senders (default is 2). */
	double GetSenderTimeoutMultiplier() const					{ return sendermultiplier; }
	/** Sets the multiplier to be used when timing out members. */
	void SetSourceTimeoutMultiplier(double m)					{ generaltimeoutmultiplier = m; }
	/** Returns the multiplier to be used when timing out members (default is 5). */
	double GetSourceTimeoutMultiplier() const					{ return generaltimeoutmultiplier; }
	/** Sets the multiplier to be used when timing out a member after it has sent a BYE packet. */
	void SetBYETimeoutMultiplier(double m)						{ byetimeoutmultiplier = m; }
	/** Returns the multiplier to be used when timing out a member after it has sent a BYE packet (default is 1). */
	double GetBYETimeoutMultiplier() const						{ return byetimeoutmultiplier; }
	/** Sets the multiplier to be used when timing out entries in the collision table. */
	void SetCollisionTimeoutMultiplier(double m)				{ collisionmultiplier = m; }
	/** Returns the multiplier to be used when timing out entries in the collision table (default is 10). */
	double GetCollisionTimeoutMultiplier() const				{ return collisionmultiplier; }
	/** Sets the multiplier to be used when timing out SDES NOTE information. */
	void SetNoteTimeoutMultiplier(double m)						{ notemultiplier = m; }
	/** Returns the multiplier to be used when timing out SDES NOTE information (default is 25). */
	double GetNoteTimeoutMultiplier() const						{ return notemultiplier; }
	/** If the argument is true, the session should accept its own packets and store 
	 *  them accordingly in the source table.
	 */
	void SetAcceptOwnPackets(bool accept)						{ acceptown = accept; }
	/** Returns true if the session should accept its own packets (default is false). */
	bool AcceptOwnPackets() const								{ return acceptown; }
	/** Sets a flag indicating if a DNS lookup should be done to determine our hostname (to construct a CNAME item).
	 *  If v is set to true, the session will ask the transmitter to find a host name based upon the IP
	 *  addresses in its list of local IP addresses. If set to false, a call to gethostname or something
	 *  similar will be used to find the local hostname. Note that the first method might take some time.
	 */
	void SetResolveLocalHostname(bool v)						{ resolvehostname = v; }
	/** Returns whether the local hostname should be determined from the transmitter's list of local IP addresses 
	 *  or not (default is false).
	 */
	bool GetResolveLocalHostname() const						{ return resolvehostname; }

private:
	bool acceptown;
	bool usepollthread;
	size_t maxpacksize;
	double owntsunit;
	bool resolvehostname;
	RTPSources::ProbationType probationtype;
	double sessionbandwidth;
	double controlfrac;
	double senderfrac;
	RTPTime mininterval;
	RTPTransmitter::ReceiveMode receivemode;
	bool m_needThreadSafety;
	bool usehalfatstartup;
	bool immediatebye;
	bool SR_BYE;

	double sendermultiplier;
	double generaltimeoutmultiplier;
	double byetimeoutmultiplier;
	double collisionmultiplier;
	double notemultiplier;
	bool usepredefinedssrc;
	DWORD predefinedssrc;
	CString cname;

	};
/** Parameters for the TCP transmitter. */
class RTPTCPTransmissionParams : public RTPTransmissionParams {
public:
	RTPTCPTransmissionParams();

	/** If non null, the specified abort descriptors will be used to cancel
	 *  the function that's waiting for packets to arrive; set to null (the default)
	 *  to let the transmitter create its own instance. */
	void SetCreatedAbortDescriptors(RTPAbortDescriptors *desc) { m_pAbortDesc = desc; }

	/** If non-null, this RTPAbortDescriptors instance will be used internally,
	 *  which can be useful when creating your own poll thread for multiple sessions. */
	RTPAbortDescriptors *GetCreatedAbortDescriptors() const		{ return m_pAbortDesc; }
private:
	RTPAbortDescriptors *m_pAbortDesc;
	};
inline RTPTCPTransmissionParams::RTPTCPTransmissionParams() : RTPTransmissionParams(RTPTransmitter::TCPProto)	{ 
	m_pAbortDesc = NULL;
	}

/** This class is used by the transmission component to store the incoming RTP and RTCP data in. */
class RTPRawPacket /*: public RTPMemoryObject */ {
public:	
    	/** Creates an instance which stores data from data with length datalen.
	 *  Creates an instance which stores data from data with length datalen. Only the pointer 
	 *  to the data is stored, no actual copy is made! The address from which this packet originated 
	 *  is set to address and the time at which the packet was received is set to recvtime. 
	 *  The flag which indicates whether this data is RTP or RTCP data is set to rtp. A memory
	 *  manager can be installed as well.
	 */
	RTPRawPacket(BYTE *data,size_t datalen,RTPAddress *address,RTPTime &recvtime,bool rtp/*,RTPMemoryManager *mgr = 0*/);
	~RTPRawPacket();
	
	/** Returns the pointer to the data which is contained in this packet. */
	BYTE *GetData()				{ return packetdata; }
	/** Returns the length of the packet described by this instance. */
	size_t GetDataLength() const		{ return packetdatalength; }
	/** Returns the time at which this packet was received. */
	RTPTime GetReceiveTime() const	{ return receivetime; }
	/** Returns the address stored in this packet. */
	const RTPAddress *GetSenderAddress() const	{ return senderaddress; }
	/** Returns true if this data is RTP data, false if it is RTCP data. */
	bool IsRTP() const			{ return isrtp; }

	/** Sets the pointer to the data stored in this packet to zero.
	 *  Sets the pointer to the data stored in this packet to zero. This will prevent 
	 *  a delete call for the actual data when the destructor of RTPRawPacket is called. 
	 *  This function is used by the RTPPacket and RTCPCompoundPacket classes to obtain 
	 *  the packet data (without having to copy it)	and to make sure the data isn't deleted 
	 *  when the destructor of RTPRawPacket is called.
	 */
	void ZeroData()					{ packetdata = 0; packetdatalength = 0; }
	/** Allocates a number of bytes for RTP or RTCP data using the memory manager that
	 *  was used for this raw packet instance, can be useful if the RTPRawPacket::SetData
	 *  function will be used. */
	BYTE *AllocateBytes(bool isrtp, int recvlen) const;
	/** Deallocates the previously stored data and replaces it with the data that's
	 *  specified, can be useful when e.g. decrypting data in RTPSession::OnChangeIncomingData */
	void SetData(BYTE *data, size_t datalen);
	/** Deallocates the currently stored RTPAddress instance and replaces it
	 *  with the one that's specified (you probably don't need this function). */
	void SetSenderAddress(RTPAddress *address);
private:
	void DeleteData();

	BYTE *packetdata;
	size_t packetdatalength;
	RTPTime receivetime;
	RTPAddress *senderaddress;
	bool isrtp;
	};
inline RTPRawPacket::RTPRawPacket(BYTE *data,size_t datalen,RTPAddress *address,RTPTime &recvtime,bool rtp) : receivetime(recvtime) {
	packetdata = data;
	packetdatalength = datalen;
	senderaddress = address;
	isrtp = rtp;
	}
inline RTPRawPacket::~RTPRawPacket() {
	DeleteData();
	}
inline void RTPRawPacket::DeleteData() {

	if(packetdata)
		delete packetdata;
	if(senderaddress)
		delete senderaddress;

	packetdata = NULL;
	senderaddress = NULL;
	}

#pragma pack( push, before_rtcph )
#pragma pack(1)
struct RTPHeader {
#ifdef RTP_BIG_ENDIAN
	BYTE version:2;
	BYTE padding:1;
	BYTE extension:1;
	BYTE csrccount:4;
	
	BYTE marker:1;
	BYTE payloadtype:7;
#else // little endian
	BYTE csrccount:4;
	BYTE extension:1;
	BYTE padding:1;
	BYTE version:2;
	
	BYTE payloadtype:7;
	BYTE marker:1;
#endif // RTP_BIG_ENDIAN
	
	WORD sequencenumber;
	DWORD timestamp;
	DWORD ssrc;
	};
struct RTPExtensionHeader {
	WORD extid;
	WORD length;
	};
#pragma pack( pop, before_rtcph )

/** Represents an RTP Packet.
 *  The RTPPacket class can be used to parse a RTPRawPacket instance if it represents RTP data. 
 *  The class can also be used to create a new RTP packet according to the parameters specified by
 *  the user.
 */
class RTPPacket /*: public RTPMemoryObject */ {
public:
	/** 
	 *  Creates an RTPPacket instance based upon the data in rawpack, optionally installing a memory manager. 
	 *  If successful, the data is moved from the raw packet to the RTPPacket instance.
	 */
	RTPPacket(RTPRawPacket &rawpack /*,RTPMemoryManager *mgr=0*/);

	/** 
	 *  Creates a new buffer for an RTP packet and fills in the fields according to the specified parameters.
	 *  If maxpacksize is not equal to zero, an error is generated if the total packet size would exceed 
	 *  maxpacksize. The arguments of the constructor are self-explanatory. Note that the size of a header 
	 *  extension is specified in a number of 32-bit words. A memory manager can be installed.
	 */
	RTPPacket(BYTE payloadtype,const void *payloaddata,size_t payloadlen,WORD seqnr,
		  DWORD timestamp,DWORD ssrc,bool gotmarker,BYTE numcsrcs,const DWORD *csrcs,
		  bool gotextension,WORD extensionid,WORD extensionlen_numwords,const void *extensiondata,
		  size_t maxpacksize /*, RTPMemoryManager *mgr = 0*/);
	
	/** This constructor is similar to the other constructor, but here data is stored in an external buffer
	 *   with size buffersize. */
	RTPPacket(BYTE payloadtype,const void *payloaddata,size_t payloadlen,WORD seqnr,
		  DWORD timestamp,DWORD ssrc,bool gotmarker,BYTE numcsrcs,const DWORD *csrcs,
		  bool gotextension,WORD extensionid,WORD extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t buffersize/*,RTPMemoryManager *mgr = 0*/);
	/** Returns a pointer to the data of the entire packet. */
	BYTE *GetPacketData() const					{ return packet; }
	/** Returns a pointer to the actual payload data. */
	BYTE *GetPayloadData() const				{ return payload; }
	/** Returns the length of the entire packet. */
	size_t GetPacketLength() const			{ return packetlength; }
	/** Returns the payload length. */
	size_t GetPayloadLength() const			{ return payloadlength; }
	/** Returns the extended sequence number of the packet.
	 *  Returns the extended sequence number of the packet. When the packet is just received, 
	 *  only the low $16$ bits will be set. The high 16 bits can be filled in later.
	 */
	DWORD GetExtendedSequenceNumber() const											{ return extseqnr; }
	/** Sets the extended sequence number of this packet to seq. */
	void SetExtendedSequenceNumber(DWORD seq)										{ extseqnr = seq; }
	/** Returns the timestamp of this packet. */
	DWORD GetTimestamp() const														{ return timestamp; }
	virtual ~RTPPacket()		{ if(packet && !externalbuffer) delete packet /*RTPDeleteByteArray(packet /*,GetMemoryManager())*/;  }
	/** If an error occurred in one of the constructors, this function returns the error code. */
	int GetCreationError() const														{ return error; }
	/** Returns \c true if the RTP packet has a header extension and \c false otherwise. */
	bool HasExtension() const															{ return hasextension; }
	/** Returns \c true if the marker bit was set and \c false otherwise. */
	bool HasMarker() const																{ return hasmarker; }
	/** Returns the number of CSRCs contained in this packet. */
	int GetCSRCCount() const															{ return numcsrcs; }
	/** 
	 *  Returns a specific CSRC identifier. The parameter \c num can go from 0 to GetCSRCCount()-1.
	 */
	DWORD GetCSRC(int num) const;
	/** Returns the SSRC identifier stored in this packet. */
	DWORD GetSSRC() const															{ return ssrc; }
private:
	void Clear();
	int ParseRawPacket(RTPRawPacket &rawpack);
	int BuildPacket(BYTE payloadtype,const void *payloaddata,size_t payloadlen,WORD seqnr,
	                DWORD timestamp,DWORD ssrc,bool gotmarker,BYTE numcsrcs,const DWORD *csrcs,
	                bool gotextension,WORD extensionid,WORD extensionlen_numwords,const void *extensiondata,
	                void *buffer,size_t maxsize);

	int error;
	bool hasextension,hasmarker;
	int numcsrcs;
	BYTE payloadtype;
	DWORD extseqnr,timestamp,ssrc;
	BYTE *packet,*payload;
	WORD extid;
	BYTE *extension;
	size_t extensionlength;
	size_t packetlength,payloadlength;
	bool externalbuffer;
	RTPTime receivetime;
	};


class MyRTPSession : public RTPSession {
public:
	MyRTPSession() { DestroiedClbk = NULL; }
  virtual ~MyRTPSession() {}
	MyRTP_Teardown(MediaSession *,struct timeval *);
	virtual int MyRTP_SetUp(MediaSession *media_session, CSocket *tunnelling_sock) {return 0;}
	virtual int MyRTP_SetUp(MediaSession *media_session) { return 0;}
	virtual BYTE *GetMyRTPData(BYTE *, size_t *, unsigned long);
	virtual BYTE *GetMyRTPPacket(BYTE *, size_t *, unsigned long);
	virtual void SetDestroiedClbk(DESTROIED_RTP_CLBK clbk) {DestroiedClbk = clbk;}
  // virtual DESTROIED_RTP_CLBK GetDestroiedClbk() { return DestroiedClbk; }
	virtual void SetRecvRtspCmdClbk(void (*clbk)(char * cmd)) {}
  // virtual RECV_RTSP_CMD_CLBK GetRecvRtspCmdClbk() { return RecvRtspCmd; }
  virtual void LockSocket() {}
  virtual void UnlockSocket() {}
  virtual bool TryLockSocket() {return true;}
  virtual bool TryUnlockSocket() {return true;}
  virtual CSocket *GetTunnellingSocket() const { return NULL; }

protected:
	virtual int IsError(int rtperr)	{
		if(rtperr < 0)	{   
//			std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
			return RTP_ERROR;
			}
		return RTP_OK;
		}

private:
	DESTROIED_RTP_CLBK DestroiedClbk;

// private:
//     bool isHttpTunneling;
	};

class MyRTPTCPSession : public MyRTPSession {
public:
	MyRTPTCPSession();
	virtual ~MyRTPTCPSession();
	virtual int MyRTP_SetUp(MediaSession *media_session, CSocket *tunnelling_sock);

	/* Wait 1 second for TEARDOWN at default */
	virtual void MyRTP_Teardown(MediaSession *media_session, struct timeval *tval = NULL);
	BYTE *GetMyRTPData(BYTE *data_buf, size_t * size, unsigned long timeout_ms);
	BYTE *GetMyRTPPacket(BYTE *packet_buf, size_t * size, unsigned long timeout_ms);

	void SetDestroiedClbk(void (*clbk)()) {DestroiedClbk = clbk;}

  virtual void LockSocket();
  virtual void UnlockSocket();
  virtual bool TryLockSocket();
  virtual CSocket *GetTunnellingSocket() const { return TunnellingSock; }
  virtual void SetRecvRtspCmdClbk(void (*clbk)(char *cmd));

private:
  int MyTcpCreate(const RTPSessionParams &sessparams,const RTPTransmissionParams *transparams);
  // int Poll();
  // int ProcessPolledData();
protected: 
	void OnNewSource(RTPSourceData *dat);
	void OnBYEPacket(RTPSourceData *dat);
	void OnRemoveSource(RTPSourceData *dat);

private:
  CSocket *TunnellingSock;
	void (*DestroiedClbk)();
	HANDLE SocketMutex;
  int TrylockTimes;
	};

int checkerror(int rtperr);

class MyRTPUDPSession : public MyRTPSession {
public:
	MyRTPUDPSession();
	int MyRTP_SetUp(MediaSession *media_session);

	/* Wait 1 second for TEARDOWN at default */
	void MyRTP_Teardown(MediaSession *media_session, struct timeval *tval=NULL);
	BYTE * GetMyRTPData(BYTE *data_buf, size_t *size, unsigned long timeout_ms);
	BYTE * GetMyRTPPacket(BYTE *packet_buf, size_t *size, unsigned long timeout_ms);

	void SetDestroiedClbk(void (*clbk)()) {DestroiedClbk = clbk;}

protected:
	void OnNewSource(RTPSourceData *dat);
	void OnBYEPacket(RTPSourceData *dat);
	void OnRemoveSource(RTPSourceData *dat);
	// void ProcessRTPPacket(const RTPSourceData &srcdat,const RTPPacket &rtppack);
  void OnPollThreadError(int);
  void OnPollThreadStep();
  void OnPollThreadStart(bool &);
  void OnPollThreadStop();

private:
	void (*DestroiedClbk)();
	};

class MediaSession {
public:
	MediaSession();
	~MediaSession();

	int RTP_SetUp(CSocket *tunnelling_sock=NULL);

	/* Wait 1 second for TEARDOWN at default */
	int RTP_Teardown(struct timeval *tval=NULL);

	/* Function Name: GetMediaData;
	 * Description: Get RTP payload;
	 * note:
	 * <timeout_ms> in unit of microsecond.
	 * Why we set 'timeout' here is to avoid continuously occupying CPU.
	 * */
	BYTE *GetMediaData(BYTE *buf, size_t *size, unsigned long timeout_ms=TIMEOUT_MICROSECONDS);

	/* Function Name: GetMediaPacket;
	 * Description: Get RTP Packet;
	 * note:
	 * <timeout_ms> in unit of microsecond.
	 * Why we set 'timeout' here is to avoid continuously occupying CPU.
	 * */
	BYTE *GetMediaPacket(BYTE *buf, size_t *size, unsigned long timeout_ms=TIMEOUT_MICROSECONDS);

	int MediaInfoCheck();
	void SetRtpDestroiedClbk(void (*clbk)());
  void SetRtspCmdClbk(void (*clbk)(char * cmd));

  void LockSocket();
  void UnlockSocket();
  CSocket *GetTunnellingSocket();

public:
	CString MediaType;
	std::vector<WORD> Ports; // RTP and RTCP ports, -1 indicate none. ###2015-01-11### //
	CString Protocol;

	std::vector<int> PayloadType; // ###2015-01-11### //
	CString EncodeType;
	unsigned int TimeRate;
	unsigned int ChannelNum;
	// std::map<unsigned int, StreamParameters> StreamParams;

	CString ControlURI;
	CString SessionID;
	CSocket *rtspsock;
	WORD RTPPort;
	CDataSocket *rtpsock;
	WORD RTCPPort;
	CDataSocket *rtcpsock;
	int Packetization;
	int Timeout;

public: 
  FrameTypeBase *frameTypeBase;

protected:
	MyRTPSession *RTPInterface;
	};

class CRTSPClientSocket : public CSocketEx {
public:
	CRTSPClientSocket(CString uri="");
	~CRTSPClientSocket();
	BOOL Connect(LPCTSTR,WORD port=0 /* IPPORT_RTSP */ );
	BOOL Disconnect();
	void Close();
	int Send(CString);
	int Send(WORD http_tunnel_port, CString);
	int ReadHeader(char *,DWORD,WORD timeout=5);
	int ReadHeader(char *,DWORD,WORD http_tunnel_port,WORD timeout=5);
	int ReadData(char *,DWORD,WORD timeout=5);

	int RecvSDP(CRTSPClientSocket *,char *, size_t);

	CString parseUriWithUserPwd(CString);

	ErrorType DoOPTIONS(CString uri="", bool http_tunnel_no_response=false);
	ErrorType DoDESCRIBE(CString uri="", bool http_tunnel_no_response=false);

	// To setup all of the media sessions in SDP
	ErrorType DoSETUP();

	/* To setup the media sessions 
	 * media_session: 
	 * 	the media session
	 * rtp_over_tcp: 
	 *	if set true, means using the rtsp tcp socket to transmit rtp packets. If 'http_tunnel_no_response' is also set true, it will be ignored.
	 * http_tunnel_no_response: 
	 *	when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoPLAY will not wait for the response,
       *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk), and 'rtp_over_tcp' will be ignored.
	 *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
	 * */
	ErrorType DoSETUP(MediaSession *media_session, bool rtp_over_tcp=false, bool http_tunnel_no_response=false);

	/* Example: DoSETUP("video");
	 * To setup the first video session in SDP
	 * */
	ErrorType DoSETUP(CString media_type, bool rtp_over_tcp=false);

	// To play all of the media sessions in SDP
	ErrorType DoPLAY();
	
	/* To play the media sessions 
	 * media_session: 
	 * 	the media session
	 * scale: 
	 *	playing speed, such as 1.5 means 1.5 times of the normal speed, default NULL=normal speed
	 * start_time: 
	 *	start playing point, such as 20.5 means starting to play at 20.5 seconds, default NULL=play from 0 second or the PAUSE point
	 * end_time: 
	 *	end playing point, such as 20.5 means ending play at 20.5 seconds, default NULL=play to the end
	 * http_tunnel_no_response: 
	 *	when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoPLAY will not wait for the response,
       *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk) 
	 *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
	 * */
	ErrorType DoPLAY(MediaSession *media_session, float *scale=NULL, float *start_time=NULL, float *end_time=NULL, bool http_tunnel_no_response=false);

	/* Example: DoPLAY("video");
	 * To play the first video session in SDP
	 * media_type: 
	 * "audio"/"video"
	 * scale: 
	 *	playing speed, such as 1.5 means 1.5 times of the normal speed, default NULL=normal speed
	 * start_time: 
	 *	start playing point, such as 20.5 means starting to play at 20.5 seconds, default NULL=play from 0 second or the PAUSE point
	 * end_time: 
	 *	end playing point, such as 20.5 means ending play at 20.5 seconds, default NULL=play to the end
	 * http_tunnel_no_response: 
	 *	when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoPLAY will not wait for the response,
       *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk).
	 *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
	 * 
	 * */
	ErrorType DoPLAY(CString media_type, float *scale=NULL, float *start_time=NULL, float *end_time=NULL, bool http_tunnel_no_response=false);

	// To pause all of the media sessions in SDP
	ErrorType DoPAUSE();

	// To pause the media sessions
	ErrorType DoPAUSE(MediaSession *media_session, bool http_tunnel_no_response);

	/* Example: DoPAUSE("video");
	 * To pause the first video session in SDP
	 * http_tunnel_no_response: 
	 *	when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoPAUSE will not wait for the response,
       *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk) 
	 *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
	 * */
	ErrorType DoPAUSE(CString media_type, bool http_tunnel_no_response=false);

	/* To get parameters all of the media sessions in SDP 
	* The most general use is to keep the RTSP session alive: 
	* Invoke this function periodly within TIMEOUT(see: GetSessionTimeout()) */
	ErrorType DoGET_PARAMETER();

	/* To get parameters of the media sessions */
	ErrorType DoGET_PARAMETER(MediaSession *media_session, bool http_tunnel_no_response);

	/* Example: DoGET_PARAMETER("video");
	 * To get parameters of the first video session in SDP
	 * http_tunnel_no_response: 
	 *	when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoGET_PARAMETER will not wait for the response,
       *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk) 
	 *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
	 * */
	ErrorType DoGET_PARAMETER(CString media_type, bool http_tunnel_no_response=false);

	/* To teardown all of the media sessions in SDP */
	ErrorType DoTEARDOWN();

	/* To teardown the media sessions */
	ErrorType DoTEARDOWN(MediaSession *media_session, bool http_tunnel_no_response);

	/* Example: DoTEARDOWN("video");
	 * To teardown the first video session in SDP
	 * http_tunnel_no_response: 
	 *	when using http-tunnelling, rtp and rtsp are transmitted in the same socket, if http_tunnel_no_response is set true, DoTEARDOWN will not wait for the response,
       *  because the response will be handled in callback function when getting rtp packets(refer to: SetRtspCmdClbk) 
	 *  YOU MUST SET THE CALLBACK, OTHERWITH IT WILL BLOCKED WHEN GETTING MEDIA DATA
	 * */
	ErrorType DoTEARDOWN(CString media_type, bool http_tunnel_no_response=false);

	ErrorType DoRtspOverHttpGet();
	ErrorType DoRtspOverHttpPost();

public:
	BYTE *GetMediaData(MediaSession *media_session, BYTE *buf, size_t * size, size_t max_size);
	BYTE *GetMediaData(CString media_type, BYTE *buf, size_t *size, size_t max_size);
	BYTE *GetMediaFrame(CString media_type, BYTE *buf, size_t *size, size_t max_size);

	BYTE *GetMediaPacket(MediaSession *media_session, BYTE *buf, size_t *size);
	BYTE *GetMediaPacket(CString media_type, BYTE *buf, size_t *size);

	DWORD CheckAuth(CString cmd, CString uri);
	CString MakeMd5DigestResp(CString realm, CString cmd, CString uri, CString nonce, CString username = "", CString password = "");
	CString MakeBasicResp(CString username = "", CString password = "");
	void SetUsername(CString username) {Username=username;}
	void SetPassword(CString password) {Password=password;}
	CString GetUsername() const {return Username;}
	CString GetPassword() const {return Password;}
	void UpdateXSessionCookie();
	int SetAvailableRTPPort(MediaSession *, WORD RTP_port=0);
	int ParseSDP(CString SDP="");
	CString ParseSessionID(CString);
	int ParseTimeout(CString);
	CString GetResource(CString uri="");
	WORD GetPort(CString);
	void SetDestroiedClbk(MediaSession *, DESTROIED_CLBK);
	void SetDestroiedClbk(CString, DESTROIED_CLBK);
	int GetTimeRate(CString);
	int GetChannelNum(CString);
	void SetAudioByeFromServerClbk(DESTROIED_CLBK);
	void SetVideoByeFromServerClbk(DESTROIED_CLBK);

	CString ParseError(ErrorType);


protected:
	CString RtspURI;
	unsigned int RtspCSeq;
	//int RtspSockfd;
	CString RtspIP;
	WORD RtspPort;
	CString RtspResponse;
	CString SDPStr;
	map<CString, MediaSession *> *MediaSessionMap;

	bool CmdPLAYSent;
	bool isConnected;

	Buffer_t AudioBuffer;
	Buffer_t VideoBuffer;

	// NALUTypeBase * NALUType;
	size_t GetVideoDataCount;

	// Authentication
	CString Username;
	CString Password;
	CString Realm;
	CString Nonce;
	DESTROIED_CLBK ByeFromServerAudioClbk;
	DESTROIED_CLBK ByeFromServerVideoClbk;
	int Timeout;

  /* Especially for H264/H265 */
  // bool ObtainVpsSpsPpsPeriodly;

	WORD RtspOverHttpDataPort;
	CRTSPClientSocket *RtspOverHttpDataSockfd;

protected:
  SDPData *sdpData;

public:
  static const char *HttpHeadUserAgent;
  static const char *HttpHeadXSessionCookie;
  static const char *HttpHeadAccept;
  static const char *HttpHeadPrama;
  static const char *HttpHeadCacheControl;
  static const char *HttpHeadContentType;
  static const char *HttpHeadContentLength;
  static const char *HttpHeadExpires;

protected:
  CString HttpHeadUserAgentContent;
  CString HttpHeadXSessionCookieContent;
  CString HttpHeadAcceptContent;
  CString HttpHeadPramaContent;
  CString HttpHeadCacheControlContent;
  CString HttpHeadContentTypeContent;
  CString HttpHeadContentLengthContent;
  CString HttpHeadExpiresContent;
	};





#define NAL_UNIT_TYPE_NUM_H264          32
#define PACKETIZATION_MODE_NUM_H264     3

#define NAL_UNIT_TYPE_NUM_H265          64
#define PACKETIZATION_MODE_NUM_H265     1

#define PACKET_MODE_SINGAL_NAL          0
#define PACKET_MODE_NON_INTERLEAVED     1
#define PACKET_MODE_INTERLEAVED         2

#define IS_PACKET_MODE_VALID_H264(P) 	\
	((P) >= PACKET_MODE_SINGAL_NAL && (P) <= PACKET_MODE_INTERLEAVED)

using std::string;
/* More info refer to H264 'nal_unit_type' */

class NALUTypeBase : public FrameTypeBase {
public:
	// NALU types map for h265 
	static NALUTypeBase * NalUnitType_H265[1][NAL_UNIT_TYPE_NUM_H265];
public:
	NALUTypeBase();
	virtual ~NALUTypeBase() {};
public:
	virtual uint16_t ParseNALUHeader_F(const uint8_t *RTPPayload) = 0;
	virtual uint16_t ParseNALUHeader_Type(const uint8_t *RTPPayload) = 0;

	virtual uint16_t ParseNALUHeader_NRI(const uint8_t *RTPPayload) {return 0;} // only for h264
	virtual uint16_t ParseNALUHeader_Layer_ID(const uint8_t *RTPPayload) {return 0;} // only for h265
	virtual uint16_t ParseNALUHeader_Temp_ID_Plus_1(const uint8_t *RTPPayload) {return 0;} // only for h265
	
	virtual bool IsPacketStart(const uint8_t *rtp_payload) = 0;
	virtual bool IsPacketEnd(const uint8_t *rtp_payload) = 0;
	virtual bool IsPacketReserved(const uint8_t *rtp_payload) { return false;}
	virtual bool IsPacketThisType(const uint8_t *rtp_payload) = 0;
	virtual size_t CopyData(uint8_t *buf, uint8_t *data, size_t size) = 0;
	// virtual NALUTypeBase * GetNaluRtpType(int packetization, int nalu_type_id) = 0;
	virtual std::string GetName() const { return Name; }
	virtual bool GetEndFlag() { return EndFlag; }
	virtual bool GetStartFlag() { return StartFlag; }
  virtual uint8_t *PrefixXPS(uint8_t *buf, size_t *size, CStringEx xps);
public:
  virtual bool NeedPrefixParameterOnce();
  virtual int ParseParaFromSDP(SDPMediaInfo & sdpMediaInfo);
protected:
	std::string Name;
	bool EndFlag;
	bool StartFlag;
	};

// #define NAL_UNIT_TYPE_NUM 		32
// #define PACKETIZATION_MODE_NUM 	3

#define PACKET_MODE_SINGAL_NAL 			0
#define PACKET_MODE_NON_INTERLEAVED 	1
#define PACKET_MODE_INTERLEAVED 		2

#define IS_PACKET_MODE_VALID(P) 	\
	((P) >= PACKET_MODE_SINGAL_NAL && (P) <= PACKET_MODE_INTERLEAVED)

/* More info refer to H264 'nal_unit_type' */
#define IS_NALU_TYPE_VALID_H264(N) 		\
	( \
      ((N) >= 1 && (N) <= 12) || \
      ((N) == H264TypeInterfaceSTAP_A::STAP_A_ID) || \
      ((N) == H264TypeInterfaceSTAP_B::STAP_B_ID) || \
      ((N) == H264TypeInterfaceMTAP_16::MTAP_16_ID) || \
      ((N) == H264TypeInterfaceMTAP_24::MTAP_24_ID) || \
      ((N) == H264TypeInterfaceFU_A::FU_A_ID) || \
      ((N) == H264TypeInterfaceFU_B::FU_B_ID) \
	)


/* H264TypeInterface */
class H264TypeInterface {
public:
	enum {
		_SEI = 0x6, // decimal: 6
		_SPS = 0x7, // decimal: 7
		_PPS = 0x8, // decimal: 8
		_STAP_A_ID = 0x18, // decimal: 24
		_STAP_B_ID = 0x19, // decimal: 25
		_MTAP_16_ID = 0x1A, // decimal: 26
		_MTAP_24_ID = 0x1B, // decimal: 27
		_FU_A_ID = 0x1C, // decimal: 28
		_FU_B_ID = 0x1D // decimal: 29
		};
	static H264TypeInterface * NalUnitType_H264[PACKETIZATION_MODE_NUM_H264][NAL_UNIT_TYPE_NUM_H264];

  virtual ~H264TypeInterface() {};
  virtual uint16_t ParseNALUHeader_F(const uint8_t * rtp_payload) {
    if(!rtp_payload) 
			return 0;
    uint16_t NALUHeader_F_Mask = 0x0080; // binary: 1000_0000
    return (rtp_payload[0] & NALUHeader_F_Mask);
		}
  virtual uint16_t ParseNALUHeader_NRI(const uint8_t * rtp_payload) {
    if(!rtp_payload) 
			return 0;
    uint16_t NALUHeader_NRI_Mask = 0x0060; // binary: 0110_0000
    return (rtp_payload[0] & NALUHeader_NRI_Mask);
	  }

  virtual uint16_t ParseNALUHeader_Type(const uint8_t * rtp_payload) {
    if(!rtp_payload) 
			return 0;
    uint16_t NALUHeader_Type_Mask = 0x001F; // binary: 0001_1111
    return (rtp_payload[0] & NALUHeader_Type_Mask);
		}
  virtual bool IsPacketStart(const uint8_t * rtp_payload) {return true;}
  virtual bool IsPacketEnd(const uint8_t * rtp_payload) {return true;}
  virtual bool IsPacketReserved(const uint8_t * rtp_payload) {return false;}
  virtual bool IsPacketThisType(const uint8_t * rtp_payload) {return true;}
  virtual int SkipHeaderSize(const uint8_t * rtp_payload) {return 1;}
	};

class H264TypeInterfaceSTAP_A : public H264TypeInterface {
public:
	virtual ~H264TypeInterfaceSTAP_A() {};
	virtual bool IsPacketStart(const uint8_t * rtp_payload);
	virtual bool IsPacketEnd(const uint8_t * rtp_payload);
	virtual bool IsPacketThisType(const uint8_t * rtp_payload);
public:
	static const uint8_t STAP_A_ID;
	};

class H264TypeInterfaceSTAP_B : public H264TypeInterface {
	public:
		virtual ~H264TypeInterfaceSTAP_B() {};

	public:
		static const uint8_t STAP_B_ID;
	};

class H264TypeInterfaceMTAP_16 : public H264TypeInterface {
public:
	virtual ~H264TypeInterfaceMTAP_16() {};
public:
	static const uint8_t MTAP_16_ID;
	};

class H264TypeInterfaceMTAP_24 : public H264TypeInterface {
public:
	virtual ~H264TypeInterfaceMTAP_24() {};
public:
	static const uint8_t MTAP_24_ID;
	};

#define FU_A_ERR 	0xFF

class H264TypeInterfaceFU_A : public H264TypeInterface {
public:
	virtual ~H264TypeInterfaceFU_A() {};

public:
	/* Function: "ParseNALUHeader_*":
	 * 	Return 'FU_A_ERR'(0xFF) if error occurred */
	virtual uint16_t ParseNALUHeader_F(const uint8_t * rtp_payload);
	virtual uint16_t ParseNALUHeader_NRI(const uint8_t * rtp_payload);
	virtual uint16_t ParseNALUHeader_Type(const uint8_t * rtp_payload);
	virtual int SkipHeaderSize(const uint8_t * rtp_payload) {return 2;}
public:
	static const uint8_t FU_A_ID;

public:
	/* if FU_A payload type */
	bool IsPacketThisType(const uint8_t * rtp_payload);
	/* Packet Start Flag */
	bool IsPacketStart(const uint8_t * rtp_payload);
	/* Packet End Flag */
	bool IsPacketEnd(const uint8_t * rtp_payload);
	/* Reserved */
	bool IsPacketReserved(const uint8_t * rtp_payload);
	};

class H264TypeInterfaceFU_B : public H264TypeInterface {
public:
	virtual ~H264TypeInterfaceFU_B() {};
public:
	static const uint8_t FU_B_ID;
	};

class NALUTypeBase_H264 : public NALUTypeBase {
public: 
    static const string ENCODE_TYPE;
public:
      NALUTypeBase_H264();
	virtual ~NALUTypeBase_H264() {};
public:
	virtual uint16_t ParseNALUHeader_F(const uint8_t * RTPPayload);
	virtual uint16_t ParseNALUHeader_NRI(const uint8_t * RTPPayload);
	virtual uint16_t ParseNALUHeader_Type(const uint8_t * RTPPayload);
	virtual bool IsPacketStart(const uint8_t * rtp_payload) {return true;}
	virtual bool IsPacketEnd(const uint8_t * rtp_payload) {return true;}
	virtual bool IsPacketReserved(const uint8_t * rtp_payload) {return false;}
	virtual bool IsPacketThisType(const uint8_t * rtp_payload);
	virtual H264TypeInterface * GetNaluRtpType(int packetization, int nalu_type_id);
	virtual std::string GetName() const { return Name; }
	virtual bool GetEndFlag() { return EndFlag; }
	virtual bool GetStartFlag() { return StartFlag; }

	// H265 interface with no use
	virtual uint16_t ParseNALUHeader_Layer_ID(const uint8_t * RTPPayload) {return 0;}
	virtual uint16_t ParseNALUHeader_Temp_ID_Plus_1(const uint8_t * RTPPayload) {return 0;}

public:
  virtual void Init();
  virtual uint8_t * PrefixParameterOnce(uint8_t * buf, size_t * size);
  virtual bool NeedPrefixParameterOnce();
  virtual int ParseParaFromSDP(SDPMediaInfo & sdpMediaInfo);
	virtual int ParsePacket(const uint8_t * RTPPayload, size_t size, bool * EndFlag);
	virtual size_t CopyData(uint8_t * buf, uint8_t * data, size_t size);

  virtual void SetSPS(const string &s) { SPS.assign(s);}
  virtual void SetPPS(const string &s) { PPS.assign(s);}
  virtual const string GetSPS() { return SPS;}
  virtual const string GetPPS() { return PPS;}

  void InsertXPS() { prefixParameterOnce = true; }
  void NotInsertXPSAgain() { prefixParameterOnce = false; }
private:
  bool prefixParameterOnce;
  string SPS;
  string PPS;
  int Packetization;
public:
  H264TypeInterface * NALUType;
	};


#define NAL_UNIT_TYPE_BIT_NUM           6
#define NUH_LAYER_ID_BIT_NUM            6
#define NUH_TEMPORAL_ID_PLUS1_BIT_NUM   3

#define FUs_H265_ERR 	0xFFFF

/* More info refer to H265 'nal_unit_type' */
#define IS_NALU_TYPE_VALID_H265(N) 		\
	( \
      ((N) >= 0 && (N) <= 40) || \
      ((N) == H265TypeInterfaceAPs::APs_ID_H265) || \
      ((N) == H265TypeInterfaceFUs::FUs_ID_H265) \
	)

/* H265TypeInterface */
class H265TypeInterface {
public:
	static H265TypeInterface * NalUnitType_H265[PACKETIZATION_MODE_NUM_H265][NAL_UNIT_TYPE_NUM_H265];
  virtual ~H265TypeInterface() {};
  virtual uint16_t ParseNALUHeader_F(const uint8_t * rtp_payload) {
      if(!rtp_payload) return 0;
      const uint16_t NALUHeader_F_Mask = 0x8000; // binary: 1000_0000_0000_0000
      uint16_t HeaderTmp = 0;
      HeaderTmp = ((rtp_payload[0] << 8) | rtp_payload[1]);
      HeaderTmp = HeaderTmp & NALUHeader_F_Mask;
      return HeaderTmp;
  }
  virtual uint16_t ParseNALUHeader_NRI(const uint8_t * rtp_payload) {
      if(!rtp_payload) return 0;
      uint16_t NALUHeader_NRI_Mask = 0x0060; // binary: 0110_0000
      return (rtp_payload[0] & NALUHeader_NRI_Mask);
    }

  virtual uint16_t ParseNALUHeader_Type(const uint8_t * rtp_payload) {
      if(!rtp_payload) return 0;
      const uint16_t NALUHeader_Type_Mask = 0x7E00; // binary: 0111_1110_0000_0000
      uint16_t HeaderTmp = 0;
      HeaderTmp = ((rtp_payload[0] << 8) | rtp_payload[1]);
      HeaderTmp = HeaderTmp & NALUHeader_Type_Mask;
      return HeaderTmp;
    }

  virtual uint16_t ParseNALUHeader_Layer_ID(const uint8_t * rtp_payload) {
      if(!rtp_payload) return 0;
      const uint16_t NALUHeader_Layer_ID_Mask = 0x01F8; // binary: 0000_0001_1111_1000
      uint16_t HeaderTmp = 0;
      HeaderTmp = ((rtp_payload[0] << 8) | rtp_payload[1]);
      HeaderTmp = HeaderTmp & NALUHeader_Layer_ID_Mask;
      return HeaderTmp;
    }

  virtual uint16_t ParseNALUHeader_Temp_ID_Plus_1(const uint8_t * rtp_payload) {
      if(!rtp_payload) return 0;
      const uint16_t NALUHeader_Temp_ID_Mask = 0x0007; // binary: 0000_0000_0000_0111
      uint16_t HeaderTmp = 0;
      HeaderTmp = ((rtp_payload[0] << 8) | rtp_payload[1]);
      HeaderTmp = HeaderTmp & NALUHeader_Temp_ID_Mask;
      return HeaderTmp;
    }

  virtual bool IsPacketStart(const uint8_t * rtp_payload) {return true;}
  virtual bool IsPacketEnd(const uint8_t * rtp_payload) {return true;}
  virtual bool IsPacketReserved(const uint8_t * rtp_payload) {return false;}
  virtual bool IsPacketThisType(const uint8_t * rtp_payload) {return true;}
  virtual int SkipHeaderSize(const uint8_t * rtp_payload) {return 2;}
	};


class NALUTypeBase_H265 : public NALUTypeBase {
public:
    static const string ENCODE_TYPE;
public:
	NALUTypeBase_H265();
	virtual ~NALUTypeBase_H265() {};
public:
	virtual uint16_t ParseNALUHeader_F(const uint8_t * RTPPayload);
	virtual uint16_t ParseNALUHeader_Type(const uint8_t * RTPPayload);
	virtual uint16_t ParseNALUHeader_Layer_ID(const uint8_t * RTPPayload);
	virtual uint16_t ParseNALUHeader_Temp_ID_Plus_1(const uint8_t * RTPPayload);
	virtual bool IsPacketStart(const uint8_t * rtp_payload) { return true; }
	virtual bool IsPacketEnd(const uint8_t * rtp_payload) { return true; }
	virtual bool IsPacketThisType(const uint8_t * rtp_payload);
	H265TypeInterface * GetNaluRtpType(int packetization, int nalu_type_id);
	virtual std::string GetName() const { return Name; }
	virtual bool GetEndFlag() { return EndFlag; }
	virtual bool GetStartFlag() { return StartFlag; }
public:
  virtual void SetVPS(const string &s) { VPS.assign(s);}
  virtual void SetSPS(const string &s) { SPS.assign(s);}
  virtual void SetPPS(const string &s) { PPS.assign(s);}
  virtual const string GetVPS() { return VPS;}
  virtual const string GetSPS() { return SPS;}
  virtual const string GetPPS() { return PPS;}
public:
  virtual void Init();
  virtual uint8_t * PrefixParameterOnce(uint8_t * buf, size_t * size);
  virtual bool NeedPrefixParameterOnce();
  virtual int ParseParaFromSDP(SDPMediaInfo & sdpMediaInfo);
virtual int ParsePacket(const uint8_t * RTPPayload, size_t size, bool * EndFlag);
virtual size_t CopyData(uint8_t * buf, uint8_t * data, size_t size);
  void InsertXPS() { prefixParameterOnce = true; }
  void NotInsertXPSAgain() { prefixParameterOnce = false; }

protected:
  bool prefixParameterOnce;
  string VPS;
  string SPS;
  string PPS;
    // int Packetization;
// std::string Name;
// bool EndFlag;
// bool StartFlag;

public:
  H265TypeInterface *NALUType;
	};

class H265TypeInterfaceAPs : public H265TypeInterface {
	public:
		virtual ~H265TypeInterfaceAPs() {};

	public:
		virtual bool IsPacketStart(const uint8_t * rtp_payload);
		virtual bool IsPacketEnd(const uint8_t * rtp_payload);
		virtual bool IsPacketThisType(const uint8_t * rtp_payload);
        virtual int SkipHeaderSize(const uint8_t * rtp_payload) {return 2;}

	public:
		static const uint16_t APs_ID_H265;
	};

class H265TypeInterfaceFUs : public H265TypeInterface {
public:
	virtual ~H265TypeInterfaceFUs() {};
public:
	/* Function: "ParseNALUHeader_*":
	 * 	Return 'FU_A_ERR'(0xFF) if error occurred */
	virtual uint16_t ParseNALUHeader_Type(const uint8_t * RTPPayload);
      virtual int SkipHeaderSize(const uint8_t * rtp_payload) {return 3;}
public:
	static const uint16_t FUs_ID_H265;

public:
	/* if FU_A payload type */
	bool IsPacketThisType(const uint8_t * rtp_payload);
	/* Packet Start Flag */
	bool IsPacketStart(const uint8_t * rtp_payload);
	/* Packet End Flag */
	bool IsPacketEnd(const uint8_t * rtp_payload);
	};
