/*

Copyright (c) 1998 by PJ Naughter.  
All rights reserved.

*/

/////////////////////////////////  Includes  //////////////////////////////////


#include "stdafx.h"

#ifndef _NEWMEET_MODE

#include "atlconv.h"
#include "afxpriv.h"
#include "winsock2.h"
#include "ping.h"

#endif


/////////////////////////////////  Macros & Statics ///////////////////////////

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifndef _NEWMEET_MODE


CPing::CPing() {
	sm_bAttemptedWinsock2Initialise=0;
  sm_bWinsock2OK=0;
	sm_TimerFrequency = 0;
	}

CPing::~CPing() {
//	WSACleanup();
	}



///////////////////////////////// Implementation //////////////////////////////

BOOL CPing::Initialise2() {


	if(!sm_bAttemptedWinsock2Initialise) {
		sm_bAttemptedWinsock2Initialise = TRUE;

		//Initialise the winsock 2 stack
		WSADATA wsa;
		sm_bWinsock2OK = 1 /*(WSAStartup(MAKEWORD(2, 1), &wsa) == 0)*/;

    //Use the High performace counter to get an accurate RTT
    LARGE_INTEGER Frequency;
		Frequency.QuadPart = 0;
    sm_bWinsock2OK = sm_bWinsock2OK && QueryPerformanceFrequency(&Frequency);
    if(sm_bWinsock2OK)
      sm_TimerFrequency = Frequency.QuadPart;
		}

	return sm_bWinsock2OK;
	}


BOOL CPing::Ping(LPCTSTR pszHostName, CPingReply *pr, UCHAR /*nTTL*/, DWORD dwTimeout, UCHAR nPacketSize) {

  //Parameter validation
  if(nPacketSize > MAX_ICMP_PACKET_SIZE || nPacketSize < MIN_ICMP_PACKET_SIZE) {
    ASSERT(FALSE);
    SetLastError(WSAENOBUFS);
    return FALSE;
		}

	//For correct operation of the T2A macro, see TN059
	USES_CONVERSION;

	//Make sure everything is initialised
	if (!Initialise2())
	  return FALSE;

  //Resolve the address of the host to connect to
  sockaddr_in dest;
  memset(&dest,0,sizeof(dest));
	LPSTR lpszAscii = T2A((LPTSTR) pszHostName);
  unsigned long addr = inet_addr(lpszAscii);
	if(addr == INADDR_NONE)	{
		//Not a dotted address, then do a lookup of the name
		hostent* hp = gethostbyname(lpszAscii);
		if(hp) {
      memcpy(&(dest.sin_addr),hp->h_addr,hp->h_length);
  	  dest.sin_family = hp->h_addrtype;
			}
    else {
		  TRACE(_T("CPing::Ping2, Could not resolve the host name %s\n"), pszHostName);
		  return FALSE;
			}
		}
	else {
			dest.sin_addr.s_addr = addr;
			dest.sin_family = AF_INET;
		}

  //Create the raw socket
  SOCKET sockRaw = WSASocket(AF_INET, SOCK_RAW, IPPROTO_ICMP, NULL, 0, 0);
  if(sockRaw == INVALID_SOCKET) {
	  TRACE(_T("CPing::Ping, Failed to create a raw socket\n"));
	  return FALSE;
		}

  //Allocate the ICMP packet
  int nBufSize = nPacketSize + sizeof(ICMP_HEADER);
  char* pICMP = new char[nBufSize+256];		// patch: l'originale crashava su NT!
  FillIcmpData((LPICMP_HEADER) pICMP, nBufSize);

  //Get the tick count prior to sending the packet
  LARGE_INTEGER TimerTick;
  VERIFY(QueryPerformanceCounter(&TimerTick));
  __int64 nStartTick = TimerTick.QuadPart;

  //Send of the packet
	int nWrote = sendto(sockRaw, pICMP, nBufSize, 0, (sockaddr*)&dest, sizeof(dest));
	if(nWrote == SOCKET_ERROR) {
		TRACE(_T("CPing::Ping2, sendto failed\n"));

    delete [] pICMP;

    DWORD dwError = GetLastError();
    closesocket(sockRaw);
    SetLastError(dwError);

    return FALSE;
		}

  //allocate the recv buffer
  char* pRecvBuf = new char[MAX_ICMP_PACKET_SIZE];
  BOOL bReadable;
  sockaddr_in from;
  int nFromlen = sizeof(from);
  int nRead = 0;

  //Allow the specified timeout
  if(IsSocketReadible(sockRaw, dwTimeout, bReadable)) {
    if(bReadable) {
      //Receive the response
	    nRead = recvfrom(sockRaw, pRecvBuf, MAX_ICMP_PACKET_SIZE, 0, (sockaddr*)&from, &nFromlen);
			}
    else {
		  TRACE(_T("CPing::Ping2, timeout occured while awaiting recvfrom\n"));
      closesocket(sockRaw);

      delete [] pICMP;
      delete [] pRecvBuf;

      //set the error to timed out
      SetLastError(WSAETIMEDOUT);

		  return FALSE;
			}
		}
  else {
		TRACE(_T("CPing::Ping2, IsReadible call failed\n"));

    delete [] pICMP;
    delete [] pRecvBuf;

    DWORD dwError = GetLastError();
    closesocket(sockRaw);
    SetLastError(dwError);

		return FALSE;
		}

  //Get the current tick count
  VERIFY(QueryPerformanceCounter(&TimerTick));

  //Now check the return response from recvfrom
	if(nRead == SOCKET_ERROR) {
		TRACE(_T("CPing::Ping2, recvfrom call failed\n"));

    delete [] pICMP;
    delete [] pRecvBuf;

    DWORD dwError = GetLastError();
    closesocket(sockRaw);
    SetLastError(dwError);

		return FALSE;
		}

  //Decode the response we got back
 	BOOL bSuccess = DecodeResponse(pRecvBuf, nRead, &from);

  //If we successfully decoded the response, then return the
  //values in the CPingReply instance
  if(bSuccess) {
		if(pr) {
			pr->Address = from.sin_addr;
			pr->RTT = (ULONG) ((TimerTick.QuadPart - nStartTick) * 1000 / sm_TimerFrequency);
			}
		}

  //Don't forget to release out socket
  closesocket(sockRaw);

  //Free up the memory we allocated
  delete [] pICMP;
  delete [] pRecvBuf;

  //return the status
	return bSuccess;
	}

BOOL CPing::IsSocketReadible(SOCKET socket, DWORD dwTimeout, BOOL& bReadible) {

  timeval timeout = {dwTimeout/1000, dwTimeout % 1000};
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(socket, &fds);
  int nStatus = select(0, &fds, NULL, NULL, &timeout);
  if(nStatus == SOCKET_ERROR) {
    return FALSE;
		}
  else {
    bReadible = !(nStatus == 0);
    return TRUE;
		}
	}


//Decode the raw Ip packet we get back
BOOL CPing::DecodeResponse(char* pBuf, int nBytes, sockaddr_in* from) {

  //Get the current tick count
  LARGE_INTEGER TimerTick;
  VERIFY(QueryPerformanceCounter(&TimerTick));


	LPIP_HEADER pIpHdr = (LPIP_HEADER) pBuf;
	int nIpHdrlen = pIpHdr->h_len * 4; //Number of 32-bit words*4 = bytes

  //Not enough data recieved
	if(nBytes < nIpHdrlen + MIN_ICMP_PACKET_SIZE) {
		TRACE(_T("Received too few bytes from %s\n"), inet_ntoa(from->sin_addr));
    SetLastError(ERROR_UNEXP_NET_ERR);
    return FALSE;
		}

  //Check it is an ICMP_ECHOREPLY packet
	LPICMP_HEADER pIcmpHdr = (LPICMP_HEADER) (pBuf + nIpHdrlen);
	if(pIcmpHdr->i_type != 0) { //type ICMP_ECHOREPLY is 0
		TRACE(_T("non-echo type %d recvd\n"), pIcmpHdr->i_type);
    SetLastError(ERROR_UNEXP_NET_ERR);
		return FALSE;
		}

  //Check it is the same id as we sent
	if(pIcmpHdr->i_id != (USHORT)GetCurrentProcessId()) {
		TRACE(_T("Received someone else's packet!\n"));
    SetLastError(ERROR_UNEXP_NET_ERR);
		return FALSE;
		}

  return TRUE;
	}

//generate an IP checksum based on a given data buffer
USHORT CPing::GenerateIPChecksum(USHORT* pBuffer, int nSize) {
  unsigned long cksum = 0;

  while(nSize > 1) {
	  cksum += *pBuffer++;
	  nSize -= sizeof(USHORT);
		}
  
  if (nSize) 
	  cksum += *(UCHAR*)pBuffer;

  cksum = (cksum >> 16) + (cksum & 0xffff);
  cksum += (cksum >>16);
  return (USHORT)(~cksum);
	}

//Fill up the ICMP packet with defined values
void CPing::FillIcmpData(LPICMP_HEADER pIcmp, int nData) {

  pIcmp->i_type    = 8; //ICMP_ECHO type
  pIcmp->i_code    = 0;
  pIcmp->i_id      = (USHORT) GetCurrentProcessId();
  pIcmp->i_seq     = 0;
  pIcmp->i_cksum   = 0;
  pIcmp->timestamp = GetTickCount();
 
  //Set up the data which will be sent
  int nHdrSize = sizeof(ICMP_HEADER);
  char* pData = (char*) (pIcmp + nHdrSize);
  memset(pData, 'E', nData - nHdrSize);

  //Generate the checksum
  pIcmp->i_cksum = GenerateIPChecksum((USHORT*)pIcmp, nData);
	}


#endif				// NEWMEET_MODE
