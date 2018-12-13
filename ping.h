/*
Module : PING.H
Purpose: Interface for an MFC wrapper class to encapsulate PING
Created: PJN / 10-06-1998
History: None

Copyright (c) 1998 by PJ Naughter.  
All rights reserved.

*/


/////////////////////////// Macros ///////////////////////////

#ifndef __PING_H__
#define __PING_H__


#ifndef __AFXPRIV_H__
#pragma message("The class CPing requires AFXPRIV.H in your PCH")
#endif


/////////////////////////////////  Definitions ////////////////////////////////

#define MIN_ICMP_PACKET_SIZE 8    //minimum 8 byte icmp packet (just header)
#define MAX_ICMP_PACKET_SIZE 1024 //Maximum icmp packet size

#ifndef _WINSOCK2API_
#pragma message("You need to include winsock2.h in your PCH")
#endif

// IP header
typedef struct tagIP_HEADER {
	unsigned int h_len:4;          // length of the header
	unsigned int version:4;        // Version of IP
	unsigned char tos;             // Type of service
	unsigned short total_len;      // total length of the packet
	unsigned short ident;          // unique identifier
	unsigned short frag_and_flags; // flags
	unsigned char  ttl; 
	unsigned char proto;           // protocol (TCP, UDP etc)
	unsigned short checksum;       // IP checksum
	unsigned int sourceIP;
	unsigned int destIP;
	} IP_HEADER;
typedef IP_HEADER FAR* LPIP_HEADER;

// ICMP header
typedef struct tagICMP_HEADER {
  BYTE i_type;
  BYTE i_code; /* type sub code */
  USHORT i_cksum;
  USHORT i_id;
  USHORT i_seq;
  /* This is not the std header, but we reserve space for time */
  ULONG timestamp;
	} ICMP_HEADER;
typedef ICMP_HEADER FAR* LPICMP_HEADER;


/////////////////////////// Classes /////////////////////////////////

struct CPingReply {
	in_addr	 Address;  //The IP address of the replier
	unsigned long RTT; //Round Trip time in Milliseconds
	};

class CPing {
public:
	CPing();
	~CPing();

//Methods
	BOOL Ping(LPCTSTR pszHostName, CPingReply *pr=NULL, UCHAR nTTL = 10, DWORD dwTimeout = 5000, UCHAR nPacketSize = 32);

protected:
	BOOL Initialise2();
  static BOOL IsSocketReadible(SOCKET socket, DWORD dwTimeout, BOOL& bReadible);
	void FillIcmpData(LPICMP_HEADER pIcmp, int nData);
	BOOL DecodeResponse(char* pBuf, int nBytes, sockaddr_in* from);
	USHORT GenerateIPChecksum(USHORT* pBuffer, int nSize);

	BOOL sm_bAttemptedWinsock2Initialise;
  BOOL sm_bWinsock2OK;
  __int64 sm_TimerFrequency;
	};



#endif //__PING_H__

