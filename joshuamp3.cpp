/* High Performance MPEG 1.0/2.0 Audio Player for Layer 1, 2 and 3.
Written and copyrights by Michael Hipp. http://mpg123.org
	 Windows version by Dario Greggio 1999-2023 (dario.greggio@outlook.it).
   Uses code from various people. 
	 Last version used: 0.62 from MPG123 (2006)

	http://mpg123.de/
	https://github.com/gypified/libmpg123

	OCCHIO le static entro una funzione sono CONDIVISE! come dentro la classe

	info varie: http://www.multiweb.cz/twoinches/mp3inside.htm
	 */


#include "stdafx.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <io.h>
#include <wav.h>
//#include "mpg123.h"
//#include "tables.h"

#include "vidsend.h"
#include "vidsendView.h"
#include "joshuamp3.h"
#include "mainfrm.h"
//#include "joshuaSound.h"
//#include "joshuaLog.h"
#include "player.h"

//#include        <unistd.h>

//#pragma comment(lib, "dsound.lib")
//#pragma comment(lib, "dxguid.lib")

#define debug_print(a,b)


#ifdef I386_ASSEM

extern BYTE *wordpointer;
extern int bitindex,tellcnt;

unsigned int   getBits(int);
#pragma aux getBitsFast=\
  "mov    ecx,offset wordpointer"\
  "movzx  eax,byte ptr[ecx+1]"\
  "mov    ah,[ecx]"\
  "mov    ecx,bitindex"\
  "shl    ax,cl"\
  "mov    ecx,ebx"\
  "add    bitindex,ecx"\
  "add    tellcnt,ecx"\
  "neg    ecx"\
  "add    ecx,016h"\
  "shr    eax,cl"\
  "mov    ecx,bitindex"\
  "sar    ecx,03h"\
  "add    [wordpointer],ecx"\
  "and    bitindex,07h"\
  parm [ebx] value [eax] modify [ecx];

unsigned int   getBitsFast(int);
#pragma aux getBits=\
  "cmp ebx,0"\
  "jne L1"\
  "xor eax,eax"\
  "jmp l2"\
  "L1:"\
  "mov    ecx,offset wordpointer"\
  "movzx   eax,byte ptr[ecx]"\
  "shl    eax,16"\
  "mov     ah,byte ptr[ecx]+1"\
  "mov     al,byte ptr[ecx]+2"\
  "mov     ecx,bitindex"\
  "shl    eax,8"\
  "shl    eax,cl"\
  "mov    ecx,ebx"\
  "add    bitindex,ecx"\
  "add    tellcnt,ecx"\
  "neg    ecx"\
  "add    ecx,032h"\
  "shr    eax,cl"\
  "mov    ecx,bitindex"\
  "sar    ecx,03h"\
  "add    [wordpointer],ecx"\
  "and    bitindex,07h"\
  "l2:"\
  parm [ebx] value [eax] modify [ecx];

static inline int get1Bit(void) {
	extern int bitindex;
	extern unsigned char *wordpointer;
	int ret;

	asm("\n\t"
		"movl	%1,%%ecx\n\t"
		"movzbl	(%%ecx),%%eax\n\t"
		"movl	%2,%%ecx\n\t"
		"incl	%%ecx\n\t"
		"rolb	%%cl,%%al\n\t"
		"andb	$1,%%al\n\t"
		"movl	%%ecx,%2\n\t"
		"andl	$7,%2\n\t"
		"sarl	$3,%%ecx\n\t"
		"addl	%%ecx,%1\n" 
		: "=a" (ret) 
		: "m" (wordpointer) , "m" (bitindex) 
		: "eax" , "ecx" , "memory" );
	return ret;
}

#endif




// --- open the device to read the bit stream from it 
int CJoshuaMP3::OpenStream(const char *bs_filenam) {
	int i=0;

	reader=new MP3_READER(this);
	if(reader && reader->init()) {
		if(reader->open(bs_filenam)) {
			i=1;
			}
    }
	return i;
	}

int CJoshuaMP3::ReopenStream(const char *bs_filenam) {		// tipo per cambiare stazione radio...
	int i=0;

  if(reader) {
		reader->close();
		if(reader->init()) {
			if(reader->open(bs_filenam)) {
				i=1;
				}
			}
    }
	return i;
	}

int CJoshuaMP3::OpenStream(CFile *f) {
	int i=0;
	char buf[128];

	reader=new MP3_READER(this);
	if(reader && reader->init()) {
		reader->filelen = f->GetLength();
		reader->filept=f->m_hFile;

		_tcscpy(reader->filename,f->GetFileName());
		reader->flags |= MP3_READER::READER_FD_OPENED;

		reader->filepos=0;
		reader->filelen=reader->getFileInfo(buf);
  
		if(reader->filelen > 0) {
			if(!_tcsncmp(buf,"TAG",3)) {
				reader->flags |= MP3_READER::READER_ID3TAG;
				memcpy(&reader->id3buf,buf,128);
				}
			}

		i=1;
    }
	return i;
	}

// ---  close the device containing the bit stream after a read process
int CJoshuaMP3::CloseStream() {

  if(reader) {
		reader->close();
		delete reader;
		}
	reader=NULL;
	return 1;
	}

/*******************************************************************
 * stream based operation
 */
MP3_READER::MP3_READER(CJoshuaMP3 *p) : m_parent(p), filelen(-1), filepos(0), filept(HFILE_ERROR), flags(0) {

	*filename=0;
	ZeroMemory(&id3buf,sizeof(struct ID3_TAG));
	httpauth=NULL;
	httpauth1[0]=0;
	proxyurl = NULL;
	proxyip = 0;
	proxyport=8080;
	lastread=0;
	frame_index.fill=0;
	frame_index.step=0;
	}

int MP3_READER::init() {

	frame_index.fill = 0;
	frame_index.step = 1;
	//	reset_id3();
	lastread=0;

  return 1;
	}

#include <winsock.h>

#define ACCEPT_HEAD "Accept: audio/mpeg, audio/x-mpegurl, */*\r\n"

//const char *MP3_READER::httpauth = NULL;
//char MP3_READER::httpauth1[256];

void MP3_READER::writestring(HANDLE fd, const char *string) {
	int result, bytes = strlen(string);

	while(bytes) {
		if((result = send((int)fd, string, bytes, 0)) < 0 /*&& errno != EINTR interrotto da signal...? */) {
//		if((result = write(fd, string, bytes)) < 0 /*&& errno != EINTR interrotto da signal...? */) {
//			perror ("write");
      debug_print(CLogFile::flagError2,"write: %x",WSAGetLastError());
//			exit (1);
		  return;
			}
		else if(result == 0) {
      debug_print(CLogFile::flagError2,"write: %s\n",
				"socket closed unexpectedly");
//			exit (1);
			return;
			}
		string += result;
		bytes -= result;
		}
	}

void MP3_READER::readstring(char *string, int maxlen, HANDLE fd) {
	int pos = 0;

	while(1) {
		if(recv((int)fd,string+pos,1,0) == 1) {
//		if(read(fileno(f),string+pos,1) == 1) {
			pos++;
			if(string[pos-1] == '\n'  || string[pos-1] >= 0x80) {
				string[pos] = 0;
				break;
				}
			if(pos>maxlen)
				break;
			}
		else {
			if(WSAGetLastError() != WSAEWOULDBLOCK) {
				string[pos] = 0;
				break;
				}
			}
/*		else if(errno != EINTR) {
			fprintf (stderr, "Error reading from socket or unexpected EOF.\n");
			exit(1);
		} interrotto da signal...? 
		*/
		}
	}

char *MP3_READER::url2hostport(const char *url, char **hname, unsigned long *hip, unsigned int *port) {
	const char *cptr;
	struct hostent *myhostent;
	struct in_addr myaddr;
	int isip = 1;

	if(!(strncmp(url, "http://", 7)))
		url += 7;
	cptr = url;
	while(*cptr && *cptr != ':' && *cptr != '/') {
		if((*cptr < '0' || *cptr > '9') && *cptr != '.')
			isip = 0;
		cptr++;
	}
	*hname = strdup(url); /* removed the strndup for better portability */
	if(!(*hname)) {
		*hname = NULL;
		return (NULL);
		}
	(*hname)[cptr - url] = 0;
	if(!isip) {
		if(!(myhostent = gethostbyname(*hname)))
			return (NULL);
		memcpy(&myaddr, myhostent->h_addr, sizeof(myaddr));
		*hip = myaddr.s_addr;
		}
	else
		if((*hip = inet_addr(*hname)) == INADDR_NONE)
			return (NULL);
	if(!*cptr || *cptr == '/') {
		*port = 80;
		return (char*)cptr;
		}
	*port = atoi(++cptr);
	while(*cptr && *cptr != '/')
		cptr++;
	return (char*)cptr;
	}
int MP3_READER::getauthfromURL(char *url,char *auth) {
	return 0;
	}
void MP3_READER::encode64(const char *source,char *destination) {
	// usare quella di joshuasocket
//	CSMTPAttachment::EncodeBase64(source, 255, destination,255,NULL);
	*destination=0;
	}
int MP3_READER::http_open(const char *url,char *mime,int port) {
//v. http_get.c
	char *purl=NULL, *host, *request=NULL, *sptr;
	int linelength;
	unsigned long myip;
	unsigned int myport;
	HANDLE sock=(HANDLE)INVALID_SOCKET;
	int relocate, numrelocs = 0;
	struct sockaddr_in server;
//	FILE *myfile;

	WSADATA wsaData;
	int iResult;
// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(!proxyip   /*INADDR_NONE*/) {
		if(!proxyurl)
			if(!(proxyurl = getenv("MP3_HTTP_PROXY")))
				if(!(proxyurl = getenv("http_proxy")))
					proxyurl = getenv("HTTP_PROXY");
		if(proxyurl && proxyurl[0] && strcmp(proxyurl, "none")) {
			host = NULL;
			if(!(url2hostport(proxyurl, &host, &proxyip, &proxyport))) {
		     debug_print(CLogFile::flagError2,"Unknown proxy host \"%s\".\n",
					host ? host : "");
//				exit (1);
				 return -2;
				}
			if(host)
				free(host);
			}
		else
			proxyip = INADDR_NONE;
		}	
	
	if((linelength = _tcslen(url)+200) < 1024)
		linelength = 1024;
	request = (char *)malloc(linelength);
	purl = (char *)malloc(2048);
	if(!request || !purl) {
     debug_print(CLogFile::flagError2,"malloc() failed, out of memory.\n");
//		exit(1);
		 return -1;
		}
	_tcsncpy(purl, url, 2047);
	purl[2047] = '\0';

  getauthfromURL(purl,httpauth1);

	do {
		strcpy(request, "GET ");
		if(proxyip != INADDR_NONE) {
			if(_tcsncmp(url, "http://", 7))
				_tcscat(request, "http://");
			_tcscat(request, purl);
			myport = proxyport;
			myip = proxyip;
			}
		else {
			host = NULL;
			if(!(sptr = url2hostport(purl, &host, &myip, &myport))) {
	      debug_print(CLogFile::flagError2,"Unknown host \"%s\".\n",
					host ? host : "");
//				exit (1);
				return -1;
				}
			_tcscat(request, sptr);
			}
		sprintf(request + _tcslen(request),
			" HTTP/1.0\r\nUser-Agent: %s/%s\r\n",
			"joshuamp3" /*prgName*/, "1.0" /*prgVersion*/);
		if(host) {
			sprintf(request + _tcslen(request), "Host: %s:%u\r\n", host, myport);
			free(host);
			}

		strcat(request, ACCEPT_HEAD);
		server.sin_family = AF_INET;
		server.sin_port = htons(myport);
		server.sin_addr.s_addr = myip;
		if((int)sock>=0)		// utile se relocation
			::closesocket((int)sock);
		if((sock = (HANDLE)socket(PF_INET, SOCK_STREAM,  IPPROTO_IP /*6*/)) < 0) {

//#include <fcntl.h>



//			perror ("socket");
      debug_print(CLogFile::flagError2,"socket");
//			exit (1);
			return -1;
			}

rifo:
		if(connect((int)sock, (struct sockaddr *)&server, sizeof(server))) {
			if(WSAGetLastError() != WSAEWOULDBLOCK) {

	//			perror ("connect");
				debug_print(CLogFile::flagError2,"error in connect");
	//			exit (1);
				return -1;
				}
			else
				goto rifo;
			}

   unsigned long mode = 1 /*nonblocking, 0 =blocking*/;
	 // DOPO connect() :)
   int i=ioctlsocket((unsigned int)sock, FIONBIO, &mode);
//	 debug_print(CLogFile::flagError2,"non blocking socket: %x",i);

		if(_tcslen(httpauth1) || httpauth) {
			char buf[2047];
			_tcscat(request,"Authorization: Basic ");
      if(_tcslen(httpauth1))
        encode64(httpauth1,buf);
      else
			  encode64(httpauth,buf);
			_tcscat(request,buf);
			_tcscat(request,_T("\r\n"));
			}
		_tcscat(request, _T("\r\n"));

		writestring(sock, request);
		//if(!(myfile = fdopen(sock, "rb"))) {		// solo linux...
/*    myfile = new FILE;
		if(!myfile) {
//			perror ("fdopen");
      debug_print(CLogFile::flagError2,"fdopen");
//			exit (1);
			return 0;
			}
    myfile->_file = sock;
    myfile->_cnt = 0;
    myfile->_ptr = NULL;
    myfile->_base = NULL;
    myfile->_flag = 0;
		*/
		relocate = FALSE;
		purl[0] = '\0';
		readstring(request, linelength-1, sock);
		if((sptr = strchr(request, ' '))) {
			switch(sptr[1]) {
				case '3':
					relocate = TRUE;
				case '2':
					break;
				default:
					debug_print(CLogFile::flagError2,"HTTP request failed: %s",
						sptr+1); /* '\n' is included ; a volte c'è pattume qua... */
					break;
				}
			}
		id3buf.titolo[0]=0;
		do {		// se relocation non trova gli header pare... arriva subito lo stream (v. radio2)
			readstring(request, linelength-1, sock);
			if(!strncmp(request, "Location:", 9))
				_tcsncpy(purl, trim(request+9), 2047);
			if(!strncmp(request, "Content-Type:", 13))
				_tcsncpy(mime, trim(request+13), 127);
			if(!strncmp(request, "icy-name:", 9)) {
				_tcsncpy(id3buf.titolo, trim(request+9), 30);
				flags |= MP3_READER::READER_ID3TAG;
				}
			if(!strncmp(request, "icy-description:", 16)) {
				_tcsncpy(id3buf.note, trim(request+16), 30);
				}
			if(!strncmp(request, "icy-genre:", 10)) {
				}
			if(!strncmp(request, "X-Loudness:", 11)) {
// usare!
				}
			} while(request[0] != '\r' && request[0] != '\n' && request[0] >= ' ' && request[0] < 0x80);
		} while(relocate && purl[0] && numrelocs++ < 5);
	if(relocate) {
      debug_print(CLogFile::flagError2,"Too many HTTP relocations.");
//		exit (1);
			return -1;
		}
	free(purl);
	free(request);

//	return fileno(myfile) /*sock*/; // NON funziona su windows... FINIRE 2019
	return (int)sock;
	}

int MP3_READER::open(const char *nomefile,int fd) {
	int retVal=0;
  char buf[128];
	BYTE filept_opened = 1;
	int myfilept=HFILE_ERROR; /* descriptor of opened file/stream */

	flags = 0;

//	clear_icy();
	if(!nomefile) {		// no file to open, got a descriptor (stdin)
//    myfilept = stdin;
		if(fd < 0) {		/* special: read from stdin */
			myfilept = 0;
			filept_opened = 0; /* and don't try to close it... */
			}
		else 
			myfilept = fd;
//		goto fine;
		}
	else if(!_tcsncmp(nomefile, "http://", 7)) {
		char mime[128]={0};
		myfilept = http_open(nomefile, mime);
		/* now check if we got sth. and if we got sth. good */
		if((myfilept >= 0) && mime[0] /* */ && _tcscmp(mime, "audio/mpeg") && _tcscmp(mime, "audio/x-mpeg")) {
			debug_print(CLogFile::flagError2, "Error: unknown mpeg MIME type %s - is it perhaps a playlist (use -@)?\nError: If you know the stream is mpeg1/2 audio, then please report this as 'PACKAGE_NAME' bug\n", !mime ? "<nil>" : mime);
			myfilept = -1;
			}
		if(myfilept < 0) 
			return myfilept; /* error... */
//		goto fine;
		flags |= READER_FD_OPENED_SOCKET;
		}
	#ifndef O_BINARY
	#define O_BINARY (0)
	#endif

//  else if((myfilept=_lopen(nomefile,OF_READ | OF_SHARE_DENY_WRITE)) == HFILE_ERROR) {
  else if((myfilept=(int)CreateFile(nomefile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL)) == HFILE_ERROR) {
		//OFSTRUCT ofs;
		//OpenFile(nomefile, &ofs, OF_READ | OF_SHARE_DENY_WRITE);	
//		CreateFile(nomefile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);  
		perror(nomefile);
		return myfilept; /* error... */
		}

	filelen = -1;
	filept=myfilept;

  _tcscpy(filename,nomefile);
	if(filept_opened)	
		flags |= READER_FD_OPENED;

/*	if(!init)	{
			error1("no init for reader %i!", i);
			return -1;
		}*/

	filepos=0;
	if(flags & READER_FD_OPENED && !(flags & READER_FD_OPENED_SOCKET)) {

		filelen=getFileInfo(buf);
  
		if(filelen > 0) {
			if(!_tcsncmp(buf,"TAG",3)) {
				flags |= READER_ID3TAG;
				memcpy(&id3buf,buf,128);
				}
			}
		}
/*  if(rd && rd->flags & READER_ID3TAG) {
		print_id3_tag(rd->id3buf);
		}*/


	return myfilept;
	}

void MP3_READER::close() {

  if(flags & READER_FD_OPENED) {
		if(flags & READER_FD_OPENED_SOCKET) {
			::closesocket(filept);
			WSACleanup();
			}
		else {
			CloseHandle((HANDLE)filept);
			}
		}
	filept=HFILE_ERROR;
	*filename=0;
	flags=0;
	}

char *MP3_READER::trim(char *s) {

	char *p=s;
	while(isalnum(*p) || ispunct(*p) || *p==' ' /*|| *p=='/' || *p=='-' || *p=='_'*/)
		p++;
	*p=0;
	p=s; // sarebbe tipo CStringEx::Trim()
	while(*p==' ')
		p++;
	return p;
	}


#if 0
/* stream based operation  with icy meta data*/
static ssize_t icy_fullread(struct reader *rds,unsigned char *buf, ssize_t count) {
	DWORD ret,cnt;
	int i;

	cnt = 0;

	/*
		We check against READER_ID3TAG instead of rds->filelen >= 0 because if we got the ID3 TAG we know we have the end of the file.
		If we don't have an ID3 TAG, then it is possible the file has grown since we started playing, so we want to keep reading from it if possible.
	*/
	if((rds->flags & READER_ID3TAG) && rds->filepos + count > rds->filelen) count = rds->filelen - rds->filepos;

	while(cnt < count) {
		/* all icy code is inside this if block, everything else is the plain fullread we know */
		/* debug1("read: %li left", (long) count-cnt); */
		if(icy.interval && (rds->filepos+count > icy.next))	{
			unsigned char temp_buff;
			size_t meta_size;
			ssize_t cut_pos;

			/* we are near icy-metaint boundary, read up to the boundary */
			cut_pos = icy.next-rds->filepos;
			i=ReadFile((HANDLE)rds->filept,buf,cut_pos,&ret,NULL);
			if(i <= 0) 
				return ret;

			rds->filepos += ret;
			cnt += ret;

			/* now off to read icy data */

			/* one byte icy-meta size (must be multiplied by 16 to get icy-meta length) */
			i=ReadFile((HANDLE)rds->filept,temp_buff,1,&ret,NULL);
			if(i <= 0) 
				return ret;
			if(ret == 0) 
				break;

			debug2("got meta-size byte: %u, at filepos %li", temp_buff, (long)rds->filepos);
			rds->filepos += ret; /* 1... */

			if((meta_size = ((size_t) temp_buff) * 16)) {
				/* we have got some metadata */
				char *meta_buff;
				meta_buff = (char*) malloc(meta_size+1);
				if(meta_buff != NULL) {
					i=ReadFile((HANDLE)rds->filept,meta_buff,meta_size,&ret,NULL);
					meta_buff[meta_size] = 0; /* string paranoia */
					if(i <= 0) 
						return ret;

					rds->filepos += ret;

					if(icy.data) 
						free(icy.data);
					icy.data = meta_buff;
					icy.changed = 1;
					debug2("icy-meta: %s size: %d bytes", icy.data, meta_size);
					}
				else {
					error1("cannot allocate memory for meta_buff (%lu bytes) ... trying to skip the metadata!", (unsigned long)meta_size);
					rds->skip_bytes(rds, meta_size);
					}
				}
			icy.next = rds->filepos+icy.interval;
			}

		i=ReadFile((HANDLE)rds->filept,buf+cnt,count-cnt,&ret,NULL);
		if(i <= 0) 
			return ret;
		if(ret == 0) 
			break;

		rds->filepos += ret;
		cnt += ret;
		}
	// debug1("done reading, got %li", (long)cnt);
	return cnt;
	}
#endif

/* stream based operation */
int /*size_t*/ MP3_READER::fullread(BYTE *buf, size_t count) {	// era plain_fullread... per ora me ne frego di icy... 7/1/07
	DWORD ret,cnt=0;
	int i;

	/*
		We check against READER_ID3TAG instead of rds->filelen >= 0 because if we got the ID3 TAG we know we have the end of the file.
		If we don't have an ID3 TAG, then it is possible the file has grown since we started playing, so we want to keep reading from it if possible.
	*/
	if((flags & READER_ID3TAG) && ((filepos + count) > filelen))
		count = filelen - filepos;
	while(cnt < count) {
		if(flags & READER_FD_OPENED_SOCKET) {
			i=recv(filept,(char *)buf+cnt,count-cnt,0);

			// se il socket è blocking, questo non accade mai... 2021

			if(i>0)
				lastread=timeGetTime();

			if(i == 0) {
//	      debug_print(CLogFile::flagWarning,"mpg123:SOCKET_CLOSED");
				return -1;
				break;
				}
			if(i == SOCKET_ERROR) {
				if(WSAGetLastError() != WSAEWOULDBLOCK) {

//				  debug_print(CLogFile::flagWarning,"mpg123:SOCKET_ERROR");

					return -1;
					}
				else
					i=0;
				}
			ret=i;
			}
		else {
			i=ReadFile((HANDLE)filept,buf+cnt,count-cnt,&ret,NULL);
			if(i <= 0) {
				if(GetLastError() == ERROR_IO_PENDING)
					continue;
				else
					return ret;
				}
			else {
				if(ret == 0) 
					break;
				else
					lastread=timeGetTime();
				}
			}
		filepos += ret;
		cnt += ret;
		if(m_parent->intflag) {
//      debug_print(CLogFile::flagWarning,"mpg123:STOP signaled");
			return 0;
			break;
			}
		if(lastread && (lastread+5000)<timeGetTime()) {
			return 0;
			break;
			}
		}		//while
	return cnt;
	}

off_t MP3_READER::stream_lseek(off_t pos, int whence) {
	off_t ret;

//	ret = _llseek(filept, pos, whence);	// non va?! 2023... ah forse ci andava llseek
	ret=SetFilePointer((HANDLE)filept,pos,0,whence);
	if(ret >= 0)	
		filepos = ret;

	return ret;
	}


/**************************************** 
 * HACK,HACK,HACK: step back <num> frames 
 * can only work if the 'stream' isn't a real stream but a file
 */
int MP3_READER::backBytes(int bytes) {

	if(stream_lseek(-bytes,SEEK_CUR) < 0) 
		return -1;
	/* you sure you want the buffer to resync here? */
//	if(Param.usebuffer)
//		buffer_resync();			DARIO: v. buffer.c, serve soprattutto per streams..

  return 0;
	}

/****************************************
 * HACK,HACK,HACK: step back <num> frames
 * can only work if the 'stream' isn't a real stream but a file
 */
int MP3_READER::backFrame(CJoshuaMP3 *p,int num) {
  long bytes;
  DWORD newhead;
  
	if(flags & READER_SEEKABLE) {
		unsigned long newframe, preframe;
		if(num > 0) /* back! */
		{
			if(num > p->fr.num) 
				newframe = 0;
			else 
				newframe = p->fr.num-num;
			}
		else 
			newframe = p->fr.num-num;

		/* two leading frames? hm, doesn't seem to be really needed... */
		/*if(newframe > 1) newframe -= 2;
		else newframe = 0;*/

		/* now seek to nearest leading index position and read from there until newframe is reached */
		if(stream_lseek(frame_index_find(newframe, &preframe),SEEK_SET) < 0)
			return -1;
//		debug2("going to %lu; just got %lu", newframe, preframe);
		p->fr.num = preframe;
		while(p->fr.num < newframe)	{
			// try to be non-fatal now... frameNum only gets advanced on success anyway
			if(!p->readFrame()) 
				break;
			}
		/* this is not needed at last? */
		/*read_frame(fr);
		read_frame(fr);*/

		if(p->fr.lay == 3) 
			p->setPointer(512);

//		debug1("arrived at %lu", p->fr.num);

//		if(param.usebuffer) 
//			buffer_resync();

		return 0;
		}
	else 
		return -1; /* invalid, no seek happened */

#if 0
  if(!p->firsthead)
    return 0;
  
  bytes = (p->fr.framesize+8)*(num+2);
  
  if(backBytes(bytes) < 0)
    return -1;
  if(!headRead(&newhead))
    return -1;
  
  while((newhead & HDRCMPMASK) != (p->firsthead & HDRCMPMASK)) {
    if(!headShift(&newhead))
      return -1;
		}
  
  if(backBytes(4) <0)
    return -1;

  return 0;
#endif

	}

off_t MP3_READER::frame_index_find(unsigned long want_frame, unsigned long* get_frame) {

	/* default is file start if no index position */
	off_t gopos = 0;
	*get_frame = 0;
	if(frame_index.fill) {
		/* find in index */
		size_t fi;
		/* at index fi there is frame step*fi... */
		fi = want_frame/frame_index.step;
		if(fi >= frame_index.fill) 
			fi = frame_index.fill - 1;
		*get_frame = fi*frame_index.step;
		gopos = frame_index.data[fi];
		}

	return gopos;
	}



int MP3_READER::headRead(DWORD *newhead) {
  BYTE hbuf[4];

	if(fullread(hbuf,4) != 4) 
		return FALSE;
  
  *newhead = ((DWORD) hbuf[0] << 24) |
    ((DWORD) hbuf[1] << 16) |
    ((DWORD) hbuf[2] << 8)  |
    (DWORD) hbuf[3];

//	struct MP3_FRAME_HEADER mfh;			// USARE 2023! GD
//	mfh=*(struct MP3_FRAME_HEADER *)newhead;


  
  return TRUE;
	}

int MP3_READER::headShift(DWORD *head) {
  BYTE hbuf;

	if(fullread(&hbuf,1) != 1) 
		return 0;

  *head <<= 8;
  *head |= hbuf;
//  *head &= 0xffffffff;		// a che pro?
  return 1;
	}

off_t MP3_READER::skipBytes(off_t len) {

	if(filelen >= 0) {
		off_t ret = stream_lseek(len, SEEK_CUR);
//		if(Param.usebuffer) 
//			buffer_resync();

		return ret;
		}
	else if(len >= 0)	{
		unsigned char buf[1024]; /* ThOr: Compaq cxx complained and it makes sense to me... or should one do a cast? What for? */
		off_t ret;
		while(len > 0)	{
			off_t num = len < sizeof(buf) ? len : sizeof(buf);
			ret = fullread(buf, num);
			if(ret < 0) 
				return ret;
			len -= ret;
			}
		return filepos;
		}
	else 
		return -1;
	}

int MP3_READER::readFrameBody(BYTE *buf,unsigned int size) {
  long l;

  if((l=fullread(buf,size)) != size) {
    if(l < 0)
      return -1;
		else
			return 0;
//    memset(buf+l,0,size-l);
		}
  memset(buf+l,0,size-l);

  return 1;
	}

off_t MP3_READER::tell() {

  return filepos;
	}

void MP3_READER::rewind() {

	stream_lseek(0,SEEK_SET);
//	if(param.usebuffer) 
//	buffer_resync();
	}

/*
 * returns length of a file (if filept points to a file)
 * reads the last 128 bytes information into buffer
 */
int MP3_READER::getFileInfo(char *buf) {
	int len;
	DWORD ret;

  if((len=SetFilePointer((HANDLE)filept,0,0,SEEK_END)) < 0) {
    return -1;
		}
  if(SetFilePointer((HANDLE)filept,-128,0,SEEK_END) < 0)
    return -1;
	ReadFile((HANDLE)filept,buf,128,&ret,NULL);
  if(ret != 128) {
    return -1;
		}
  if(!_tcsncmp(buf,"TAG",3)) {
    len -= 128;
		}
  if(SetFilePointer((HANDLE)filept,0,0,SEEK_SET) < 0)
    return -1;
  if(len <= 0)
    return -1;
	return len;
	}


int CJoshuaMP3::GetFileInfo(const char *nomefile,struct ID3_TAG *id3) {
	HFILE filept;
	int len;
	DWORD ret;
	BYTE buf[256];

  if((filept=(int)CreateFile(nomefile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL)) == HFILE_ERROR) {
//  if((filept=_lopen(nomefile,OF_READ | OF_SHARE_DENY_WRITE)) == HFILE_ERROR) {
	  return -1;
		}
  if((len=SetFilePointer((HANDLE)filept,0,0,SEEK_END)) < 0) {
    goto error;
		}
  if(SetFilePointer((HANDLE)filept,-128,0,SEEK_END) < 0)
    goto error;
	ReadFile((HANDLE)filept,buf,128,&ret,NULL);
	CloseHandle((HANDLE)filept);
  if(ret != 128) {
	  return -1;
		}
  if(!_tcsncmp((char *)buf,"TAG",3)) {
		memcpy(id3,buf,127);			// si potrebbero zero-terminare i campi...
		return 1;
		}
	return 0;

error:
	CloseHandle((HANDLE)filept);
  return -1;
	}

int CJoshuaMP3::GetDuration(const char *nomefile) {
	HFILE filept;
	int len;
	DWORD ret;
	BYTE buf[256];

//v. anche int CJoshuaMP3::getSongLen(struct FRAME *fr,int no) {
  if((filept=(int)CreateFile(nomefile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL)) == HFILE_ERROR) {
	  return -1;
		}
  if((len=SetFilePointer((HANDLE)filept,0,0,SEEK_END)) < 0) {
    goto error;
		}
  if(SetFilePointer((HANDLE)filept,-128,0,SEEK_END) < 0)
    goto error;
	ReadFile((HANDLE)filept,buf,128,&ret,NULL);
	CloseHandle((HANDLE)filept);
  if(ret != 128 || _tcsncmp((char *)buf,"TAG",3)) {
	  return len;			// cercare tag ID3v2 ...
		}
	return len-128;

error:
	CloseHandle((HANDLE)filept);
	return -1;
	}

int MP3_READER::parse_new_id3(unsigned long first4bytes) {

/*
	trying to parse ID3v2.3 and ID3v2.4 tags...

	returns:  0 = read-error
	         -1 = illegal ID3 header; maybe extended to mean unparseable (to new) header in future
	          1 = somehow ok...
*/

#if 0

	#define UNSYNC_FLAG 128
	#define EXTHEAD_FLAG 64
	#define EXP_FLAG 32
	#define FOOTER_FLAG 16
	#define UNKNOWN_FLAGS 15 /* 00001111*/
	unsigned char buf[6];
	unsigned long length=0;
	unsigned char flags = 0;
	int ret = 1;
	unsigned char* tagdata = NULL;
	unsigned char major = first4bytes & 0xff;
	debug1("ID3v2: major tag version: %i", major);
	if(major == 0xff) 
		return -1;
	if(!rds->read_frame_body(rds,buf,6))       /* read more header information */
		return 0;

	if(buf[0] == 0xff) /* major version, will never be 0xff */
		return -1;
	/* second new byte are some nice flags, if these are invalid skip the whole thing */
	flags = buf[1];
	debug1("ID3v2: flags 0x%08x", flags);
	/* use 4 bytes from buf to construct 28bit uint value and return 1; return 0 if bytes are not syncsafe */
	#define syncsafe_to_long(buf,res) \
	( \
		(((buf)[0]|(buf)[1]|(buf)[2]|(buf)[3]) & 0x80) ? 0 : \
		(res =  (((unsigned long) (buf)[0]) << 27) \
		     | (((unsigned long) (buf)[1]) << 14) \
		     | (((unsigned long) (buf)[2]) << 7) \
		     |  ((unsigned long) (buf)[3]) \
		,1) \
	)
	/* length-10 or length-20 (footer present); 4 synchsafe integers == 28 bit number  */
	/* we have already read 10 bytes, so left are length or length+10 bytes belonging to tag */
	if(!syncsafe_to_long(buf+2,length)) 
		return -1;
	debug1("ID3v2: tag data length %lu", length);
	if(param.verbose > 1) fprintf(stderr,"Note: ID3v2.%i rev %i tag of %lu bytes\n", major, buf[0], length);
	/* skip if unknown version/scary flags, parse otherwise */
	if((flags & UNKNOWN_FLAGS) || (major > 4)) {
		/* going to skip because there are unknown flags set */
		warning2("ID3v2: Won't parse the ID3v2 tag with major version %u and flags 0x%xu - some extra code may be needed", major, flags);
		if(!rds->skip_bytes(rds,length)) /* will not store data in backbuff! */
		ret = 0;
		}
	else {
		id3.version = major;
		/* try to interpret that beast */
		if((tagdata = (unsigned char*) malloc(length+1)))	{
			debug("ID3v2: analysing frames...");
			if(rds->read_frame_body(rds,tagdata,length)) {
				unsigned long tagpos = 0;
				debug1("ID3v2: have read at all %lu bytes for the tag now", (unsigned long)length+6);
				/* going to apply strlen for strings inside frames, make sure that it doesn't overflow! */
				tagdata[length] = 0;
				if(flags & EXTHEAD_FLAG) {
					debug("ID3v2: skipping extended header");
					if(!syncsafe_to_long(tagdata, tagpos)) 
						ret = -1;
					}
				if(ret >= 0) {
					char id[5];
					unsigned long framesize;
					unsigned long fflags; /* need 16 bits, actually */
					id[4] = 0;
					/* pos now advanced after ext head, now a frame has to follow */
					while(tagpos < length-10) /* I want to read at least a full header */
					{
						int i = 0;
						unsigned long pos = tagpos;
						/* level 1,2,3 - 0 is info from lame/info tag! */
						/* rva tags with ascending significance, then general frames */
						#define KNOWN_FRAMES 8
						const char frame_type[KNOWN_FRAMES][5] = { "COMM", "TXXX", "RVA2", "TPE1", "TALB", "TIT2", "TYER", "TCON" };
						enum { egal = -1, comment, extra, rva2, artist, album, title, year, genre } tt = egal;
						/* we may have entered the padding zone or any other strangeness: check if we have valid frame id characters */
						for(; i< 4; ++i) 
							if(!( ((tagdata[tagpos+i] > 47) && (tagdata[tagpos+i] < 58))
						                     || ((tagdata[tagpos+i] > 64) && (tagdata[tagpos+i] < 91)) ) ) {
							debug5("ID3v2: real tag data apparently ended after %lu bytes with 0x%02x%02x%02x%02x", tagpos, tagdata[tagpos], tagdata[tagpos+1], tagdata[tagpos+2], tagdata[tagpos+3]);
							ret = -1;
							break;
							}
						if(ret >= 0) {
							/* 4 bytes id */
							strncpy(id, (char*)tagdata+pos, 4);
							pos += 4;
							/* size as 32 syncsafe bits */
							if(!syncsafe_to_long(tagdata+pos, framesize))	{
								ret = -1;
								error("ID3v2: non-syncsafe frame size, aborting");
								break;
								}
							if(param.verbose > 2) 
								fprintf(stderr, "Note: ID3v2 %s frame of size %lu\n", id, framesize);
							tagpos += 10 + framesize; /* the important advancement in whole tag */
							pos += 4;
							fflags = (((unsigned long) tagdata[pos]) << 8) | ((unsigned long) tagdata[pos+1]);
							pos += 2;
							/* for sanity, after full parsing tagpos should be == pos */
							/* debug4("ID3v2: found %s frame, size %lu (as bytes: 0x%08lx), flags 0x%016lx", id, framesize, framesize, fflags); */
							/* %0abc0000 %0h00kmnp */
							#define BAD_FFLAGS (unsigned long) 36784
							#define PRES_TAG_FFLAG 16384
							#define PRES_FILE_FFLAG 8192
							#define READ_ONLY_FFLAG 4096
							#define GROUP_FFLAG 64
							#define COMPR_FFLAG 8
							#define ENCR_FFLAG 4
							#define UNSYNC_FFLAG 2
							#define DATLEN_FFLAG 1
							/* shall not or want not handle these */
							if(fflags & (BAD_FFLAGS | COMPR_FFLAG | ENCR_FFLAG)) {
								warning("ID3v2: skipping invalid/unsupported frame");
								continue;
								}
							
							for(i=0; i < KNOWN_FRAMES; ++i)
								if(!strncmp(frame_type[i], id, 4)) {
									tt = i; 
									break;
									}
							
							if(tt != egal) {
								int rva_mode = -1; /* mix / album */
								unsigned long realsize = framesize;
								unsigned char* realdata = tagdata+pos;
								if((flags & UNSYNC_FLAG) || (fflags & UNSYNC_FFLAG)) {
									unsigned long ipos = 0;
									unsigned long opos = 0;
									debug("Id3v2: going to de-unsync the frame data");
									/* de-unsync: FF00 -> FF; real FF00 is simply represented as FF0000 ... */
									/* damn, that means I have to delete bytes from withing the data block... thus need temporal storage */
									/* standard mandates that de-unsync should always be safe if flag is set */
									realdata = (unsigned char*) malloc(framesize); /* will need <= bytes */
									if(!realdata) {
										error("ID3v2: unable to allocate working buffer for de-unsync");
										continue;
										}
									/* now going byte per byte through the data... */
									realdata[0] = tagdata[pos];
									opos = 1;
									for(ipos = pos+1; ipos < pos+framesize; ++ipos) {
										if(!((tagdata[ipos] == 0) && (tagdata[ipos-1] == 0xff))) {
											realdata[opos++] = tagdata[ipos];
											}
										}
									}
									realsize = opos;
									debug2("ID3v2: de-unsync made %lu out of %lu bytes", realsize, framesize);
								}
								pos = 0; /* now at the beginning again... */
								switch(tt) {
									case comment: /* a comment that perhaps is a RVA / RVA_ALBUM/AUDIOPHILE / RVA_MIX/RADIO one */
									{
										/* Text encoding          $xx */
										/* Language               $xx xx xx */
										/* policy about encodings: do not care for now here */
										/* if(realdata[0] == 0)  */
										{
											/* don't care about language */
											pos = 4;
											if(   !strcasecmp((char*)realdata+pos, "rva")
											   || !strcasecmp((char*)realdata+pos, "rva_mix")
											   || !strcasecmp((char*)realdata+pos, "rva_radio"))
											rva_mode = 0;
											else if(   !strcasecmp((char*)realdata+pos, "rva_album")
											        || !strcasecmp((char*)realdata+pos, "rva_audiophile")
											        || !strcasecmp((char*)realdata+pos, "rva_user"))
											rva_mode = 1;
											if((rva_mode > -1) && (rva_level[rva_mode] <= tt+1)) {
												char* comstr;
												size_t comsize = realsize-4-(strlen((char*)realdata+pos)+1);
												if(param.verbose > 2) 
													fprintf(stderr, "Note: evaluating %s data for RVA\n", realdata+pos);
												if((comstr = (char*) malloc(comsize+1))) {
													memcpy(comstr,realdata+realsize-comsize, comsize);
													comstr[comsize] = 0;
													/* hm, what about utf16 here? */
													rva_gain[rva_mode] = atof(comstr);
													if(param.verbose > 2) 
														fprintf(stderr, "Note: RVA value %fdB\n", rva_gain[rva_mode]);
													rva_peak[rva_mode] = 0;
													rva_level[rva_mode] = tt+1;
													free(comstr);
													}
												else 
													error("could not allocate memory for rva comment interpretation");
												}
											else {
												if(!strcasecmp((char*)realdata+pos, "")) {
													/* only add general comments */
													realdata[pos] = realdata[pos-4]; /* the encoding field copied */
													store_id3_text(&id3.comment, (char*)realdata+pos, realsize-4);
												}
											}
										}
									}
									break;
									case extra: /* perhaps foobar2000's work */
									{
										/* Text encoding          $xx */
										/* unicode would hurt in string comparison... */
										if(realdata[0] == 0) {
											int is_peak = 0;
											pos = 1;
											
											if(!strncasecmp((char*)realdata+pos, "replaygain_track_",17))	{
												debug("ID3v2: track gain/peak");
												rva_mode = 0;
												if(!strcasecmp((char*)realdata+pos, "replaygain_track_peak")) 
													is_peak = 1;
												else if(strcasecmp((char*)realdata+pos, "replaygain_track_gain")) 
														rva_mode = -1;
												}
											else
											if(!strncasecmp((char*)realdata+pos, "replaygain_album_",17))	{
												debug("ID3v2: album gain/peak");
												rva_mode = 1;
												if(!strcasecmp((char*)realdata+pos, "replaygain_album_peak")) 
													is_peak = 1;
												else if(strcasecmp((char*)realdata+pos, "replaygain_album_gain")) 
													rva_mode = -1;
												}
											if((rva_mode > -1) && (rva_level[rva_mode] <= tt+1)) {
												char* comstr;
												size_t comsize = realsize-1-(strlen((char*)realdata+pos)+1);
												if(param.verbose > 2) 
													fprintf(stderr, "Note: evaluating %s data for RVA\n", realdata+pos);
												if((comstr = (char*) malloc(comsize+1))) {
													memcpy(comstr,realdata+realsize-comsize, comsize);
													comstr[comsize] = 0;
													if(is_peak)	{
														rva_peak[rva_mode] = atof(comstr);
														if(param.verbose > 2)
															fprintf(stderr, "Note: RVA peak %fdB\n", rva_peak[rva_mode]);
														}
													else	{
														rva_gain[rva_mode] = atof(comstr);
														if(param.verbose > 2)
															fprintf(stderr, "Note: RVA gain %fdB\n", rva_gain[rva_mode]);
														}
													rva_level[rva_mode] = tt+1;
													free(comstr);
													}
												else 
													error("could not allocate memory for rva comment interpretation");
												}
											}
										}
									break;
									case rva2: /* "the" RVA tag */
									{
										#ifdef HAVE_INTTYPES_H
										/* starts with null-terminated identification */
										if(param.verbose > 2) fprintf(stderr, "Note: RVA2 identification \"%s\"\n", realdata);
										/* default: some individual value, mix mode */
										rva_mode = 0;
										if( !strncasecmp((char*)realdata, "album", 5)
										    || !strncasecmp((char*)realdata, "audiophile", 10)
										    || !strncasecmp((char*)realdata, "user", 4))
										rva_mode = 1;
										if(rva_level[rva_mode] <= tt+1)	{
											pos += strlen((char*) realdata) + 1;
											if(realdata[pos] == 1)	{
												++pos;
												/* only handle master channel */
												debug("ID3v2: it is for the master channel");
												/* two bytes adjustment, one byte for bits representing peak - n bytes for peak */
												/* 16 bit signed integer = dB * 512 */
												/* we already assume short being 16 bit */
												rva_gain[rva_mode] = (float) ((((short) realdata[pos]) << 8) | ((short) realdata[pos+1])) / 512;
												pos += 2;
												if(param.verbose > 2) 
													fprintf(stderr, "Note: RVA value %fdB\n", rva_gain[rva_mode]);
												/* heh, the peak value is represented by a number of bits - but in what manner? Skipping that part */
												rva_peak[rva_mode] = 0;
												rva_level[rva_mode] = tt+1;
												}
											}
										#else
										warning("ID3v2: Cannot parse RVA2 value because I don't have a guaranteed 16 bit signed integer type");
										#endif
										}
									break;
									/* non-rva metainfo, simply store... */
									case artist:
										debug("ID3v2: parsing artist info");
										store_id3_text(&id3.artist, (char*) realdata, realsize);
										break;
									case album:
										debug("ID3v2: parsing album info");
										store_id3_text(&id3.album, (char*) realdata, realsize);
										break;
									case title:
										debug("ID3v2: parsing title info");
										store_id3_text(&id3.title, (char*) realdata, realsize);
										break;
									case year:
										debug("ID3v2: parsing year info");
										store_id3_text(&id3.year, (char*) realdata, realsize);
										break;
									case genre:
										debug("ID3v2: parsing genre info");
										store_id3_text(&id3.genre, (char*) realdata, realsize);
									break;
									default: error1("ID3v2: unknown frame type %i", tt);
									}
								if((flags & UNSYNC_FLAG) || (fflags & UNSYNC_FFLAG)) 
									free(realdata);
								}
							#undef BAD_FFLAGS
							#undef PRES_TAG_FFLAG
							#undef PRES_FILE_FFLAG
							#undef READ_ONLY_FFLAG
							#undef GROUP_FFLAG
							#undef COMPR_FFLAG
							#undef ENCR_FFLAG
							#undef UNSYNC_FFLAG
							#undef DATLEN_FFLAG
							}
						else 
							break;
						#undef KNOWN_FRAMES
						}
					}
				}
			else	{
				error("ID3v2: Duh, not able to read ID3v2 tag data.");
				ret = 0;
				}
			free(tagdata);
			}
		else {
			error1("ID3v2Arrg! Unable to allocate %lu bytes for interpreting ID3v2 data - trying to skip instead.", length);
			if(!rds->skip_bytes(rds,length)) /* will not store data in backbuff! */
				ret = 0;
			}
		}
	/* skip footer if present */
	if((flags & FOOTER_FLAG) && (!rds->skip_bytes(rds,length))) ret = 0;
	return ret;
	#undef UNSYNC_FLAG
	#undef EXTHEAD_FLAG
	#undef EXP_FLAG
	#undef FOOTER_FLAG
	#undef UNKOWN_FLAGS

#endif


	return -1;			// fare...
	}


inline DWORD CJoshuaMP3::getBits(int number_of_bits) {
  DWORD rval;

  if(!number_of_bits)
    return 0;

	rval = bsi.wordpointer[0];			// byte alto prima!
  rval <<= 8;
  rval |= bsi.wordpointer[1];
  rval <<= 8;
  rval |= bsi.wordpointer[2];

  rval <<= bsi.bitindex;
  rval &= 0xffffff;

  bsi.bitindex += number_of_bits;

  rval >>= (24-number_of_bits);

  bsi.wordpointer += (bsi.bitindex >> 3);
  bsi.bitindex &= 7;

  return rval;

/*_asm {
	mov ebx,number_of_bits
	cmp ebx,0
  jne L1
  xor eax,eax
  jmp l2
  L1:
  mov    ecx,offset wordpointer
  movzx   eax,byte ptr[ecx]
  shl    eax,16
  mov     ah,byte ptr[ecx]+1
  mov     al,byte ptr[ecx]+2
  mov     ecx,bitindex
  shl    eax,8
  shl    eax,cl
  mov    ecx,ebx
  add    bitindex,ecx
  add    tellcnt,ecx
  neg    ecx
  add    ecx,032h
  shr    eax,cl
  mov    ecx,bitindex
  sar    ecx,03h
  add    [wordpointer],ecx
  and    bitindex,07h
  l2:
	}*/

	}

inline DWORD CJoshuaMP3::getBitsFast(int number_of_bits) {
  WORD rval;

//  if(!number_of_bits)
//    return 0;
  rval =  (unsigned char) (bsi.wordpointer[0] << bsi.bitindex);		// byte alto prima!
  rval |= ((unsigned int) bsi.wordpointer[1]<<bsi.bitindex)>>8;
  rval <<= number_of_bits;
  rval >>= 8;

  bsi.bitindex += number_of_bits;

  bsi.wordpointer += (bsi.bitindex >> 3);
  bsi.bitindex &= 7;

  return rval;

/*_asm {
	push ecx
	mov ebx,number_of_bits
  mov    ecx,offset wordpointer
  movzx  eax,byte ptr[ecx+1]
  mov    ah,[ecx]
  mov    ecx,bitindex
  shl    ax,cl
  mov    ecx,ebx
  add    bitindex,ecx
  add    tellcnt,ecx
  neg    ecx
  add    ecx,016h
  shr    eax,cl
  mov    ecx,bitindex
  sar    ecx,03h
  add    [wordpointer],ecx
  and    bitindex,07h
	pop ecx
	}*/

	}

inline DWORD CJoshuaMP3::get1Bit() {
  BYTE rval;

  rval = *bsi.wordpointer << bsi.bitindex;

  bsi.bitindex++;
  bsi.wordpointer += (bsi.bitindex >> 3);
  bsi.bitindex &= 7;

  return rval >> 7;

/*_asm {
  mov    ecx,offset wordpointer
  movzx  eax,byte ptr[ecx]
  inc    tellcnt
  mov    ecx,bitindex
  inc    ecx
  rol    al,cl
  and    al,1
  mov    bitindex,ecx
  and    bitindex,07h
  sar    ecx,3
  add    [wordpointer],ecx
	}*/

	}

void CJoshuaMP3::backBits(int number_of_bits) {

  bsi.bitindex    -= number_of_bits;
  bsi.wordpointer += (bsi.bitindex >> 3);
  bsi.bitindex    &= 0x7;
	}

int CJoshuaMP3::getBitOffset() {
  return (-bsi.bitindex) & 0x7;
	}

int CJoshuaMP3::getByte() {

#ifdef DEBUG_GETBITS
  if(bsi.bitindex) 
    debug_print(CLogFile::flagError2,"getbyte called unsynched!");
#endif
  return *bsi.wordpointer++;
	}



/********************************/

double CJoshuaMP3::computeBpf(struct FRAME *fr) {
	double bpf;

  switch(fr->lay) {
    case 1:
      bpf = tabsel_123[fr->lsf][0][fr->bitrateIndex];
      bpf *= 12000.0 * 4.0;
      bpf /= freqs[fr->samplingFrequency] << (fr->lsf);
      break;
    case 2:
    case 3:
      bpf = tabsel_123[fr->lsf][fr->lay-1][fr->bitrateIndex];
			bpf *= 144000;
      bpf /= freqs[fr->samplingFrequency] << (fr->lsf);
      break;
    default:
      bpf = 1.0;
    }

	return bpf;
	}

double CJoshuaMP3::computeTpf(struct FRAME *fr) {
	static const WORD bs[4] = { 0,384,1152,1152 };
	double tpf;

	tpf = (double)bs[fr->lay];
	tpf /= freqs[fr->samplingFrequency] << (fr->lsf);
	return tpf;
	}

/*
 * Returns number of frames queued up in output buffer, i.e. 
 * offset between currently played and currently decoded frame.
 */
#ifndef NOXFERMEM
long CJoshuaMP3::computeBufferOffset(struct FRAME *fr) {
	long bufsize;
	
	/*
	 * buffermem->buf[0] holds output sampling rate,
	 * buffermem->buf[1] holds number of channels,
	 * buffermem->buf[2] holds audio format of output.
	 */
	
/*	if(!Param.usebuffer || !(bufsize=xfermem_get_usedspace(buffermem))
		|| !buffermem->buf[0] || !buffermem->buf[1])
		return 0;

	bufsize = (long)((double) bufsize / buffermem->buf[0] / 
			buffermem->buf[1] / computeTpf(fr));
	
	if((buffermem->buf[2] & AUDIO_FORMAT_MASK) == AUDIO_FORMAT_16)
		return bufsize/2;
	else
		return bufsize;

//	da mpg123, set 05
*/

	return 0;
	}
#endif

long CJoshuaMP3::timeToFrame(struct FRAME *fr, double seconds) {

	return (long)(seconds/computeTpf(fr));
	}

int CJoshuaMP3::getSongLen(struct FRAME *fr,int no) {
	double tpf;
	
	if(!fr)
		return 0;
	
	if(no < 0) {
		if(!reader || reader->filelen < 0)
			return 0;
		no = (double)reader->filelen / computeBpf(fr);
		}

	tpf = computeTpf(fr);
	return no*tpf;
	}



long CJoshuaMP3::setPointer(long backstep) {

  bsi.wordpointer = bsbuf + ssize - backstep;
  if(backstep)
    memcpy(bsi.wordpointer,bsbufold+fsizeold-backstep,backstep);
  bsi.bitindex = 0; 
	return bsi.tellcnt;		// tanto per gradire!
	}

void CJoshuaMP3::rewindNbits(int bits) {

  debug_print(CLogFile::flagError2,"rewind: %d",bits); 
  bsi.wordpointer -= (bits>>3);
  bsi.bitindex -= (bits & 7);
  if(bsi.bitindex < 0) {
    bsi.bitindex += 8;
    bsi.wordpointer--;
		}
	bsi.tellcnt -= bits;
	}





/*
 * Layer 2 Alloc tables .. 
 * most other tables are calculated on program start (which is (of course) not ISO-conform) .. 
 * Layer-3 huffman table is in huffman.h
 */

struct AL_TABLE CJoshuaMP3::alloc_0[] = {
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767} };

struct AL_TABLE CJoshuaMP3::alloc_1[] = {
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767} };

struct AL_TABLE CJoshuaMP3::alloc_2[] = {
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63} };

struct AL_TABLE CJoshuaMP3::alloc_3[] = {
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63} };

struct AL_TABLE CJoshuaMP3::alloc_4[] = {
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
  {2,0},{5,3},{7,5},{10,9},
  {2,0},{5,3},{7,5},{10,9},
  {2,0},{5,3},{7,5},{10,9},
  {2,0},{5,3},{7,5},{10,9},
  {2,0},{5,3},{7,5},{10,9},
  {2,0},{5,3},{7,5},{10,9},
  {2,0},{5,3},{7,5},{10,9},
  {2,0},{5,3},{7,5},{10,9}  };




/* 
 * Mpeg Layer-1 audio decoder 
 * --------------------------
 * copyright (c) 1995 by Michael Hipp, All rights reserved. See also 'README'
 * near unoptimzed ...
 *
 * may have a few bugs after last optimization ... 
 *
 */

void CJoshuaMP3::I_stepOne(DWORD balloc[], DWORD scale_index[2][SBLIMIT]) {
  DWORD *ba=balloc;
  DWORD *sca= (DWORD *)scale_index;

  if(fr.stereo == 2) {
    int i;
    int jsbound = fr.jsbound;

    for(i=0; i<jsbound; i++) { 
      *ba++ = getBits(4);
      *ba++ = getBits(4);
			}
    for(i=jsbound; i<SBLIMIT; i++)
      *ba++ = getBits(4);

    ba = balloc;

    for(i=0; i<jsbound; i++) {
      if((*ba++))
        *sca++ = getBits(6);
      if((*ba++))
        *sca++ = getBits(6);
			}
    for(i=jsbound;i<SBLIMIT;i++)
      if((*ba++)) {
        *sca++ =  getBits(6);
        *sca++ =  getBits(6);
				}
		}
  else {
    int i;
    for(i=0; i<SBLIMIT; i++)
      *ba++ = getBits(4);
    ba = balloc;
    for(i=0; i<SBLIMIT; i++)
      if((*ba++))
        *sca++ = getBits(6);
		}
	}

void CJoshuaMP3::I_stepTwo(real fraction[2][SBLIMIT],DWORD balloc[2*SBLIMIT],DWORD scale_index[2][SBLIMIT]) {
  int i,n;
  int smpb[2*SBLIMIT]; // values: 0-65535
  int *sample;
  register DWORD *ba;
  register DWORD *sca = (DWORD *) scale_index;

  if(fr.stereo) {
    int jsbound = fr.jsbound;
    register real *f0 = fraction[0];
    register real *f1 = fraction[1];

    ba = balloc;
    for(sample=smpb,i=0; i<jsbound; i++) {
      if((n = *ba++))
        *sample++ = getBits(n+1);
      if((n = *ba++))
        *sample++ = getBits(n+1);
			}
    for(i=jsbound;i<SBLIMIT;i++) 
      if((n = *ba++))
        *sample++ = getBits(n+1);

    ba = balloc;
    for(sample=smpb,i=0; i<jsbound; i++) {
      if((n=*ba++))
        *f0++ = (real) ( ((-1)<<n) + (*sample++) + 1) * muls[n+1][*sca++];
      else
        *f0++ = 0.0;
      if((n=*ba++))
        *f1++ = (real) ( ((-1)<<n) + (*sample++) + 1) * muls[n+1][*sca++];
      else
        *f1++ = 0.0;
			}
    for(i=jsbound;i<SBLIMIT;i++) {
      if((n=*ba++)) {
        real samp = ( ((-1)<<n) + (*sample++) + 1);
        *f0++ = samp * muls[n+1][*sca++];
        *f1++ = samp * muls[n+1][*sca++];
		    }
      else
        *f0++ = *f1++ = 0.0;
	    }
    for(i=fr.downSampleSBlimit; i<32; i++)
      fraction[0][i] = fraction[1][i] = 0.0;
		}
  else {
    register real *f0 = fraction[0];

    ba = balloc;
    for(sample=smpb,i=0; i<SBLIMIT; i++)
      if((n = *ba++))
        *sample++ = getBits(n+1);
    ba = balloc;
    for(sample=smpb,i=0; i<SBLIMIT; i++) {
      if((n=*ba++))
        *f0++ = (real) ( ((-1) << n) + (*sample++) + 1) * muls[n+1][*sca++];
      else
        *f0++ = 0.0;
			}
    for(i=fr.downSampleSBlimit; i<32; i++)
      fraction[0][i] = 0.0;
		}
	}

int CJoshuaMP3::DoLayer1() {
  int clip=0;
  int i;
	BYTE stereo = fr.stereo;
  DWORD balloc[2*SBLIMIT];
  DWORD scale_index[2][SBLIMIT];
  real fraction[2][SBLIMIT];
  short int single = fr.single;

  fr.jsbound = (fr.mode == MPG_MD_JOINT_STEREO) ? (fr.modeExt<<2)+4 : 32;

  if(stereo == 1 || single == 3)
    single = 0;

  I_stepOne(balloc,scale_index);

  for(i=0; i<SCALE_BLOCK; i++) {
    I_stepTwo(fraction,balloc,scale_index);

    if(single >= 0) 
      clip += (this->*fr.synthMono)((real *)fraction[single],pcmSample,&pcmPoint);
    else {
      int p1 = pcmPoint;
      clip += (this->*fr.synth)((real *)fraction[0],0,pcmSample,&p1);
      clip += (this->*fr.synth)((real *)fraction[1],1,pcmSample,&pcmPoint);
			}

    if(pcmPoint >=  /*==*/  ai->audiobufsize)
      Flush();
		}

  return clip;
	}




/* 
 * Mpeg Layer-2 audio decoder 
 * --------------------------
 * copyright (c) 1995 by Michael Hipp, All rights reserved. See also 'README'
 *
 */


void CJoshuaMP3::InitLayer2() {
  static const double mulmul[27] = {
    0.0 , -2.0/3.0 , 2.0/3.0 ,
    2.0/7.0 , 2.0/15.0 , 2.0/31.0, 2.0/63.0 , 2.0/127.0 , 2.0/255.0 ,
    2.0/511.0 , 2.0/1023.0 , 2.0/2047.0 , 2.0/4095.0 , 2.0/8191.0 ,
    2.0/16383.0 , 2.0/32767.0 , 2.0/65535.0 ,
    -4.0/5.0 , -2.0/5.0 , 2.0/5.0, 4.0/5.0 ,
    -8.0/9.0 , -4.0/9.0 , -2.0/9.0 , 2.0/9.0 , 4.0/9.0 , 8.0/9.0 };
  static const BYTE base[3][9] = {
     { 1 , 0, 2 , } ,
     { 17, 18, 0 , 19, 20 , } ,
     { 21, 1, 22, 23, 0, 24, 25, 2, 26 } };
  int i,j,k,l,len;
  real *table;
  static const BYTE tablen[3] = { 3 , 5 , 9 };
  int *itable;
	static int *tables[3] = { grp_3tab , grp_5tab , grp_9tab };

  for(i=0; i<3; i++) {
    itable = tables[i];
    len = tablen[i];
    for(j=0; j<len; j++)
      for(k=0; k<len; k++)
        for(l=0; l<len; l++) {
          *itable++ = base[i][l];
          *itable++ = base[i][k];
          *itable++ = base[i][j];
	        }
	  }

  for(k=0; k<27; k++) {
    double m=mulmul[k];

    table = muls[k];
#ifdef USE_MMX
    if(!param.down_sample) 
      for(j=3,i=0; i<63; i++,j--)
    *table++ = 16384 * m * pow(2.0,(double)j / 3.0);
    else
#endif
    for(j=3,i=0; i<63; i++,j--)
      *table++ = m * pow(2.0,(double)j / 3.0);
    *table++ = 0.0;
		}
	}


void CJoshuaMP3::II_stepOne(DWORD *bit_alloc,int *scale) {
  short int stereo = fr.stereo-1;
  short int sblimit = fr.II_sblimit;
  short int jsbound = fr.jsbound;
  short int sblimit2 = fr.II_sblimit << stereo;
  const struct AL_TABLE *alloc1 = fr.alloc;
  int i;
  /*static 2023*/DWORD scfsi_buf[64];
  DWORD *scfsi,*bita;
  int sc,step;

  bita = bit_alloc;
  if(stereo) {
    for(i=jsbound; i; i--, alloc1+=(1 << step)) {
      *bita++ = (char)getBits(step = alloc1->bits);
      *bita++ = (char)getBits(step);
			}
    for(i=sblimit-jsbound; i; i--,alloc1+=(1<<step)) {
      bita[0] = (char)getBits(step=alloc1->bits);
      bita[1] = bita[0];
      bita+=2;
      }
    bita = bit_alloc;
    scfsi=scfsi_buf;
    for(i=sblimit2; i; i--)
      if(*bita++)
        *scfsi++ = (char)getBitsFast(2);
    }
  else { // mono 
    for(i=sblimit; i; i--,alloc1+=(1<<step))
      *bita++ = (char)getBits(step=alloc1->bits);
    bita = bit_alloc;
    scfsi=scfsi_buf;
    for(i=sblimit; i; i--)
      if(*bita++)
        *scfsi++ = (char)getBitsFast(2);
		}

  bita = bit_alloc;
  scfsi=scfsi_buf;
  for(i=sblimit2; i; i--) 
    if(*bita++)
      switch(*scfsi++) {
        case 0: 
          *scale++ = getBitsFast(6);
          *scale++ = getBitsFast(6);
          *scale++ = getBitsFast(6);
          break;
        case 1 : 
          *scale++ = sc = getBitsFast(6);
          *scale++ = sc;
          *scale++ = getBitsFast(6);
          break;
        case 2: 
          *scale++ = sc = getBitsFast(6);
          *scale++ = sc;
          *scale++ = sc;
          break;
        default:              // case 3
          *scale++ = getBitsFast(6);
          *scale++ = sc = getBitsFast(6);
          *scale++ = sc;
          break;
				}

	}

void CJoshuaMP3::II_stepTwo(DWORD *bit_alloc,real fraction[2][4][SBLIMIT],int *scale,int x1) {
  int i,j,k,ba;
  short int stereo = fr.stereo;
  short int sblimit = fr.II_sblimit;
  short int jsbound = fr.jsbound;
  const struct AL_TABLE *alloc2,*alloc1=fr.alloc;
  DWORD *bita=bit_alloc;
  int d1,step;

  for(i=0; i<jsbound; i++,alloc1+=(1<<step)) {
    step = alloc1->bits;
    for(j=0; j<stereo; j++) {
      if(ba=*bita++) {
        k=(alloc2 = alloc1+ba)->bits;
        if((d1=alloc2->d) < 0) {
          real cm=muls[k][scale[x1]];

          fraction[j][0][i] = ((real)((int)getBits(k) + d1)) * cm;
          fraction[j][1][i] = ((real)((int)getBits(k) + d1)) * cm;
          fraction[j][2][i] = ((real)((int)getBits(k) + d1)) * cm;
          }        
        else {
          static const int *table[] = { 0,0,0,grp_3tab,0,grp_5tab,0,0,0,grp_9tab };
          DWORD idx,*tab,m=scale[x1];

          idx = (DWORD)getBits(k);
          tab = (DWORD *)(table[d1] + idx + idx + idx);
          fraction[j][0][i] = muls[*tab++][m];
          fraction[j][1][i] = muls[*tab++][m];
          fraction[j][2][i] = muls[*tab][m];  
          }
        scale+=3;
        }
      else
        fraction[j][0][i] = fraction[j][1][i] = fraction[j][2][i] = 0.0;
      }
    }

  for(i=jsbound; i<sblimit; i++,alloc1+=(1 << step)) {
    step = alloc1->bits;
    bita++;			// channel 1 and channel 2 bitalloc are the same
    if(ba=*bita++) {
      k=(alloc2 = alloc1+ba)->bits;
      if((d1=alloc2->d) < 0) {
        real cm;

        cm=muls[k][scale[x1+3]];
        fraction[1][0][i] = (fraction[0][0][i] = (real)((int)getBits(k) + d1) ) * cm;
        fraction[1][1][i] = (fraction[0][1][i] = (real)((int)getBits(k) + d1) ) * cm;
        fraction[1][2][i] = (fraction[0][2][i] = (real)((int)getBits(k) + d1) ) * cm;
        cm=muls[k][scale[x1]];
        fraction[0][0][i] *= cm; fraction[0][1][i] *= cm; fraction[0][2][i] *= cm;
				}
      else {
        static const int *table[] = { 0,0,0,grp_3tab,0,grp_5tab,0,0,0,grp_9tab };
        DWORD idx,*tab,m1,m2;

        m1 = scale[x1]; m2 = scale[x1+3];
        idx = (DWORD) getBits(k);
        tab = (DWORD *) (table[d1] + idx + idx + idx);
        fraction[0][0][i] = muls[*tab][m1]; fraction[1][0][i] = muls[*tab++][m2];
        fraction[0][1][i] = muls[*tab][m1]; fraction[1][1][i] = muls[*tab++][m2];
        fraction[0][2][i] = muls[*tab][m1]; fraction[1][2][i] = muls[*tab][m2];
				}
      scale+=6;
			}
    else {
      fraction[0][0][i] = fraction[0][1][i] = fraction[0][2][i] =
      fraction[1][0][i] = fraction[1][1][i] = fraction[1][2][i] = 0.0;
			}
/* 
   should we use individual scalefac for channel 2 or
   is the current way the right one , where we just copy channel 1 to
   channel 2 ?? 
   The current 'strange' thing is, that we throw away the scalefac
   values for the second channel ...!!
-> changed .. now we use the scalefac values of channel one !! 
*/
    }

  if(sblimit > (SBLIMIT >> fr.downSample) )
    sblimit = SBLIMIT >> fr.downSample;
  for(i=sblimit; i<SBLIMIT; i++)
    for(j=0; j<stereo; j++)
      fraction[j][0][i] = fraction[j][1][i] = fraction[j][2][i] = 0.0;

	}

void CJoshuaMP3::II_select_table() {
  static const BYTE translate[3][2][16] =
   { { { 0,2,2,2,2,2,2,0,0,0,1,1,1,1,1,0 } ,
       { 0,2,2,0,0,0,1,1,1,1,1,1,1,1,1,0 } } ,
     { { 0,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0 } ,
       { 0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0 } } ,
     { { 0,3,3,3,3,3,3,0,0,0,1,1,1,1,1,0 } ,
       { 0,3,3,0,0,0,1,1,1,1,1,1,1,1,1,0 } } };

  int table,sblim;
  static const struct AL_TABLE *tables[5] =
       { alloc_0,alloc_1,alloc_2,alloc_3,alloc_4 };
  static const BYTE sblims[5] = { 27 , 30 , 8, 12 , 30 };

  if(fr.samplingFrequency >= 3)	/* Or equivalent: (fr->lsf == 1) */
    table = 4;
  else
    table = translate[fr.samplingFrequency][2-fr.stereo][fr.bitrateIndex];
  sblim = sblims[table];

  fr.alloc      = tables[table];
  fr.II_sblimit = sblim;
	}

int CJoshuaMP3::DoLayer2() {
  int clip=0;
  int i,j;
  int stereo = fr.stereo;
  real fraction[2][4][SBLIMIT]; /* pick_table clears unused subbands */
  DWORD bit_alloc[64];
  int scale[192];
  short int single = fr.single;

  II_select_table();
  fr.jsbound = (fr.mode == MPG_MD_JOINT_STEREO) ?
     (fr.modeExt << 2)+4 : fr.II_sblimit;

  if(fr.jsbound > fr.II_sblimit) {
	  fprintf(stderr, "Truncating stereo boundary to sideband limit.\n");
	  fr.jsbound=fr.II_sblimit;
	  }

  if(stereo == 1 || single == 3)
    single = 0;

  II_stepOne(bit_alloc, scale);

  for(i=0; i<SCALE_BLOCK; i++) {
    II_stepTwo(bit_alloc,fraction,scale,i >> 2);
    for(j=0; j<3; j++) {
      if(single >= 0) {
        clip += (this->*fr.synthMono)(fraction[single][j],pcmSample,&pcmPoint);
				}
      else {
        int p1 = pcmPoint;
        clip += (this->*fr.synth)(fraction[0][j],0,pcmSample,&p1);
        clip += (this->*fr.synth)(fraction[1][j],1,pcmSample,&pcmPoint);
				}

      if(pcmPoint >= /* == */ ai->audiobufsize)
        Flush();
			}
		}

  return clip;
	}



void CJoshuaMP3::readFrameInit(void) {

	fr.num = 0;
	vbr = CBR;
	abr_rate = 0;

  oldhead = 0;
  firsthead = 0;
	init=1;

	track_frames = 0;
	mean_frames = 0;
	mean_framesize = 0;
	rva_level[0] = -1;
	rva_level[1] = -1;
	#ifdef GAPLESS
	/* one can at least skip the delay at beginning - though not add it at end since end is unknown */
	if(param.gapless) 
		layer3_gapless_init(DECODER_DELAY+GAP_SHIFT, 0);
	#endif

	//	reset_id3();
	}

int CJoshuaMP3::headCheck(DWORD head) {

 	if(
		/* first 11 bits are set to 1 for frame sync  NON 12??? */
		((head & 0xffe00000) != 0xffe00000)
		||
		/* layer: 01,10,11 is 1,2,3; 00 is reserved */
		(!((head>>17) & 3))
		||
		/* 1111 means bad bitrate */
		(((head>>12) & 0xf) == 0xf)
		||
		/* 0000 means free format... */
		(((head>>12) & 0xf) == 0x0)
		||
		/* sampling freq: 11 is reserved */
		(((head>>10) & 0x3) == 0x3)
		/* here used to be a mpeg 2.5 check... re-enabled 2.5 decoding due to lack of evidence that it is really not good */
		)	{
		return FALSE;
		}
	/* if no check failed, the header is valid (hopefully)*/
	else {
		return TRUE;
		}

	}

void CJoshuaMP3::do_rva() {
	float rvafact = 1;
	float peak = 0;
	long newscale;

	if(Param.rva)	{
		int rt = 0;
		// Should one assume a zero RVA as no RVA? 
		if(Param.rva == 2 && rva_level[1] != -1) 
			rt = 1;
		if(rva_level[rt] != -1)	{
			rvafact = pow(10,rva_gain[rt]/20);
			peak = rva_peak[rt];
			if(Param.verbose > 1) 
				fprintf(stderr, "Note: doing RVA with gain %f\n", rva_gain[rt]);
			}
		else {
			debug_print(CLogFile::flagWarning,"mpg123:warning: no RVA value found");
			}
		}

	newscale = outscale*rvafact;

	// if peak is unknown (== 0) this check won't hurt 
	if((peak*newscale) > MAXOUTBURST)	{
		newscale = (long) ((float) MAXOUTBURST/peak);
		debug_print(CLogFile::flagWarning,"mpg123:warning2: limiting scale value to %u to prevent clipping with indicated peak factor of %f", newscale, peak);
		}
	// first rva setting is forced with lastscale < 0 
	if(newscale != lastscale)	{
		debug_print(CLogFile::flagWarning,"mpg123:debug3: changing scale value from %u to %u (peak estimated to %u)", lastscale != -1 ? lastscale : outscale, newscale, (long) (newscale*peak));
		makeDecodeTables(newscale); // the actual work 
		lastscale = newscale;
		}

	}


int CJoshuaMP3::readFrame() {
  DWORD newhead;
	off_t framepos;
  int give_note = Param.verbose > 1 ? 1 : (do_recover ? 0 : 1 );

#define RESYNC_LIMIT 1024

  fsizeold=fr.framesize;       // for Layer3

  if(Param.halfSpeed) {
    if(halfPhase--) {
      bsi.bitindex = 0;
      bsi.wordpointer = (BYTE *)bsbuf;
      if(fr.lay == 3)
        memcpy(bsbuf, ssave, ssize);
      return 1;
			}
    else
      halfPhase = Param.halfSpeed - 1;
		}

read_again:
  if(!reader->headRead(&newhead))
    return FALSE;

  if(/*1 || */ oldhead != newhead || !oldhead) {

init_resync:
    fr.headerChange = 2;
    if(oldhead) {
      if((oldhead & 0xc00) == (newhead & 0xc00)) {
        if((oldhead & 0xc0) == 0 && (newhead & 0xc0) == 0)
    			fr.headerChange = 1; 
        else if( (oldhead & 0xc0) > 0 && (newhead & 0xc0) > 0)
					fr.headerChange = 1;
				}
			}

#ifdef SKIP_JUNK
	if(!firsthead && !headCheck(newhead) && !free_format_header(newhead)) {
		int i;

			debug_print(CLogFile::flagError2,"Junk at the beginning %08x",newhead);

		/* check for id3v2; first three bytes (of 4) are "ID3" */
		if((newhead & (unsigned long) 0xffffff00) == (unsigned long) 0x49443300) {
			int id3length = 0;
			id3length = reader->parse_new_id3(newhead);
			goto read_again;
			}
		else if(Param.verbose > 1) 
			debug_print(CLogFile::flagError2,"Note: Junk at the beginning (0x%08lx)\n",newhead);

		/* I even saw RIFF headers at the beginning of MPEG streams ;( */
		if(newhead == MAKEFOURCC('R','I','F','F')) {
			if(Param.verbose > 1) 
				debug_print(CLogFile::flagWarning, "Note: Looks like a RIFF header.\n");
			if(!reader->headRead(&newhead))
				return 0;
			while(newhead != MAKEFOURCC('d','a','t','a')) {
				if(!reader->headShift(&newhead))
					return 0;
				}
			if(!reader->headRead(&newhead))
				return 0;
			debug_print(CLogFile::flagError2,"Skipped RIFF header!");
			goto read_again;
			}

		// step in byte steps through next 64K
		for(i=0; i<65536; i++) {
			if(!reader->headShift(&newhead))
				return 0;
			if(headCheck(newhead))
				break;
#if 0
debug_print(CLogFile::flagError2,"%08lx ",newhead);
#endif
			}
		if(i == 65536) {
				debug_print(CLogFile::flagError2,"Giving up searching valid MPEG header after 64K of junk.");
			return 0;
			}

		/* 
		 * should we additionaly check, whether a new frame starts at
		 * the next expected position? (some kind of read ahead)
		 * We could implement this easily, at least for files.
		 */
	}
#endif

	/* first attempt of read ahead check to find the real first header; cannot believe what junk is out there! */
	/* for now, a spurious first free format header screws up here; need free format support for detecting false free format headers... */
	if(!firsthead && reader->flags & reader->READER_SEEKABLE && headCheck(newhead) && decodeHeader(newhead)) {
		unsigned long nexthead = 0;
		int hd = 0;

		off_t start = reader->tell();
			debug_print(CLogFile::flagWarning,"mpg123:debug1: doing ahead check with BPF %d", fr.framesize+4);
		/* step framesize bytes forward and read next possible header*/
		if(reader->backBytes(-fr.framesize)) {
				debug_print(CLogFile::flagError2,"mpg123:error: cannot seek!");
			return 0;
			}
		hd = reader->headRead(&nexthead);
		if(reader->backBytes(reader->tell()-start)) {
				debug_print(CLogFile::flagError2,"mpg123:error: cannot seek!");
			return 0;
			}
		if(!hd)	{
			debug_print(CLogFile::flagError2,"mpg123:warning: cannot read next header, a one-frame stream? Duh...");
			}
		else {
			debug_print(CLogFile::flagWarning,"mpg123:debug2: does next header 0x%08lx match first 0x%08lx?", nexthead, newhead);
			/* not allowing free format yet */
			if(!headCheck(nexthead) || (nexthead & HDRCMPMASK) != (newhead & HDRCMPMASK)) {
				debug_print(CLogFile::flagWarning,"mpg123:debug: No, the header was not valid, start from beginning...");
				/* try next byte for valid header */
				if(reader->backBytes(3)) {
					debug_print(CLogFile::flagError2,"mpg123:error: cannot seek!");
					return 0;
					}
				goto read_again;
				}
			}
		}


    // why has this head check been avoided here before? 
    if(!headCheck(newhead)) {
      if(!firsthead && free_format_header(newhead)) {
        debug_print(CLogFile::flagError2,"mpg123:error1: Header 0x%08lx seems to indicate a free format stream; I do not handle that yet", newhead);
        goto read_again;
        return 0;
				}
    // and those ugly ID3 tags 
      if((newhead & 0xffffff00) == ('T'<<24)+('A'<<16)+('G'<<8)) {
        reader->skipBytes(124);
				if(Param.verbose > 1) 
					debug_print(CLogFile::flagWarning,"mpg123:Note: Skipped ID3 Tag!\n");
        goto read_again;
				}
      /* duplicated code from above! */
      /* check for id3v2; first three bytes (of 4) are "ID3" */
      if((newhead & (unsigned long) 0xffffff00) == (unsigned long) 0x49443300) {
        int id3length = 0;
        id3length = reader->parse_new_id3(newhead);
        goto read_again;
				}
      else if(give_note) {
        debug_print(CLogFile::flagError2,"mpg123:Note: Illegal Audio-MPEG-Header 0x%08x at offset 0x%x.", newhead,reader->tell()-4);
				}

      if(give_note && (newhead & 0xffffff00) == ('b'<<24)+('m'<<16)+('p'<<8)) 
				fprintf(stderr,"Note: Could be a BMP album art.\n");
      if(Param.tryResync || do_recover) {
        int Try=0;

				if(ai)
					ai->flush();		// bah sì... 2020
          debug_print(CLogFile::flagWarning,"mpg123:audio flush");

        /* TODO: make this more robust, I'd like to cat two mp3 fragments together (in a dirty way) and still have mpg123 beign able to decode all it somehow. */
        if(give_note) 
//           debug_print(CLogFile::flagWarning,"Note: Trying to resync...");
            /* Read more bytes until we find something that looks
               reasonably like a valid header.  This is not a
               perfect strategy, but it should get us back on the
               track within a short time (and hopefully without
               too much distortion in the audio output).  */
        do {
          if(!reader->headShift(&newhead))
						return 0;
          /* debug2("resync Try %i, got newhead 0x%08lx", Try, newhead); */
          if(!oldhead) {
            debug_print(CLogFile::flagWarning,"mpg123:debug: going to init_resync...");
            goto init_resync;       /* "considered harmful", eh? */
						}
         /* we should perhaps collect a list of valid headers that occured in file... there can be more */
         /* Michael's new resync routine seems to work better with the one frame readahead (and some input buffering?) */
					} while(++Try < RESYNC_LIMIT
						&& (newhead & HDRCMPMASK) != (oldhead & HDRCMPMASK)
						&& (newhead & HDRCMPMASK) != (firsthead & HDRCMPMASK));
         /* too many false positives 
         } while (!(head_check(newhead) && decode_header(fr, newhead))); */

        if(Try == RESYNC_LIMIT) {
          debug_print(CLogFile::flagError2,"mpg123:error: giving up resync - your stream is not nice... perhaps an improved routine could catch up");
          return 0;
					}

        if(give_note)
          debug_print(CLogFile::flagError2,"mpg123:Note: Skipped %d bytes in input.",Try);
				}
      else {
        debug_print(CLogFile::flagError2,"mpg123:error: not attempting to resync...");
        return 0;
				}
			}

    if (!firsthead) {
      if(!decodeHeader(newhead)) {
         debug_print(CLogFile::flagError2,"mpg123:error: decode header failed before first valid one, going to read again");
         goto read_again;
				}
			}
    else {
      if(!decodeHeader(newhead)) {
        debug_print(CLogFile::flagError2,"mpg123:error: decode header failed - goto resync");
        /* return 0; */
        goto init_resync;
				}
			}
		}
  else
    fr.headerChange = 0;

  // flip/init buffer for Layer 3 
  bsbufold = bsbuf;
  bsbuf = bsspace[bsnum]+512;
  bsnum = (bsnum + 1) & 1;
	// if filepos is invalid, so is framepos
	framepos = reader->filepos - 4;

	{ int i;
  // read main data into memory
  i=reader->readFrameBody(bsbuf,fr.framesize);
  if(i < 0) {
		if(ai)
			ai->flush();		// 
      debug_print(CLogFile::flagWarning,"mpg123:audio flush");
    return 0;
		}
  else if(!i) {
      debug_print(CLogFile::flagWarning,"mpg123:audio resync");
			ai->flush();		// 


    return 0; //FINIRE con tot tentativi, e togliere!
		}
	}


	if(!firsthead) {
		/* following stuff is actually layer3 specific (in practice, not in theory) */
		if(fr.lay == 3) {
			/*
				going to look for Xing or Info at some position after the header
				                                    MPEG 1  MPEG 2/2.5 (LSF)
				Stereo, Joint Stereo, Dual Channel  32      17
				Mono                                17       9
				
				Also, how to avoid false positives? I guess I should interpret more of the header to rule that out(?).
				I hope that ensuring all zeros until tag start is enough.
			*/
			size_t lame_offset = (fr.stereo == 2) ? (fr.lsf ? 17 : 32 ) : (fr.lsf ? 9 : 17);
			if(fr.framesize >= 120+lame_offset) /* traditional Xing header is 120 bytes */		{
				size_t i;
				int lame_type = 0;
				/* only search for tag when all zero before it (apart from checksum) */
				for(i=2; i < lame_offset; ++i) 
					if(bsbuf[i] != 0) 
						break;
				if(i == lame_offset) {
					if(  (bsbuf[lame_offset] == 'I')
						&& (bsbuf[lame_offset+1] == 'n')
						&& (bsbuf[lame_offset+2] == 'f')
						&& (bsbuf[lame_offset+3] == 'o')
						)	{
						lame_type = 1; /* We still have to see what there is */
						}
					else if(
					     (bsbuf[lame_offset] == 'X')
						&& (bsbuf[lame_offset+1] == 'i')
						&& (bsbuf[lame_offset+2] == 'n')
						&& (bsbuf[lame_offset+3] == 'g')
						) {
						lame_type = 2;
						vbr = VBR; /* Xing header means always VBR */
						}
					if(lame_type) {
						unsigned long xing_flags;
						
						/* we have one of these headers... */
						if(Param.verbose > 1) 
							fprintf(stderr, "Note: Xing/Lame/Info header detected\n");
						/* now interpret the Xing part, I have 120 bytes total for sure */
						/* there are 4 bytes for flags, but only the last byte contains known ones */
						lame_offset += 4; /* now first byte after Xing/Name */
						/* 4 bytes dword for flags */
						#define make_long(a, o) ((((unsigned long) a[o]) << 24) | (((unsigned long) a[o+1]) << 16) | (((unsigned long) a[o+2]) << 8) | ((unsigned long) a[o+3]))
						/* 16 bit */
						#define make_short(a,o) ((((unsigned short) a[o]) << 8) | ((unsigned short) a[o+1]))
						xing_flags = make_long(bsbuf, lame_offset);
						lame_offset += 4;
						debug_print(CLogFile::flagWarning,"mpg123:debug1: Xing: flags 0x%x", xing_flags);
						if(xing_flags & 1) {		/* frames */
							/*
								In theory, one should use that value for skipping...
								When I know the exact number of samples I could simply count in audio_flush,
								but that's problematic with seeking and such.
								I still miss the real solution for detecting the end.
							*/
							track_frames = make_long(bsbuf, lame_offset);
							if(track_frames > TRACK_MAX_FRAMES) track_frames = 0; /* endless stream? */
							#ifdef GAPLESS
							/* if no further info there, remove/add at least the decoder delay */
							if(Param.gapless) {
								unsigned long length = track_frames * spf(fr);
								if(length > 1)
									layer3_gapless_init(DECODER_DELAY+GAP_SHIFT, length+DECODER_DELAY+GAP_SHIFT);
								}
							#endif
							debug_print(CLogFile::flagWarning,"mpg123:debug1: Xing: %u frames", track_frames);
							lame_offset += 4;
							}
						if(xing_flags & 0x2) {		/* bytes */
//							if(theApp.debugLevel>=2) {
//								unsigned long xing_bytes = make_long(bsbuf, lame_offset);
//								debug_print(CLogFile::flagWarning,"mpg123:Xing: %u bytes", xing_bytes);
//								}
							lame_offset += 4;
							}
						if(xing_flags & 0x4) {		/* TOC */
							lame_offset += 100; /* just skip */
							}
						if(xing_flags & 0x8) {		/* VBR quality */
//							if(theApp.debugLevel>=2) {
//								unsigned long xing_quality = make_long(bsbuf, lame_offset);
//								debug_print(CLogFile::flagWarning,"mpg123:Xing: quality = %u", xing_quality);
//								}
							lame_offset += 4;
							}
						/* I guess that either 0 or LAME extra data follows */
						/* there may this crc16 be floating around... (?) */
						if(bsbuf[lame_offset] != 0)	{
							unsigned char lame_vbr;
							float replay_gain[2] = {0,0};
							float peak = 0;
							float gain_offset = 0; /* going to be +6 for old lame that used 83dB */
							char nb[10];

							memcpy(nb, bsbuf+lame_offset, 9);
							nb[9] = 0;
							debug_print(CLogFile::flagInfo,"mpg123:debug1: Info: Encoder: %s", nb);
							if(!_tcsncmp("LAME", nb, 4))	{
								gain_offset=6;
								debug_print(CLogFile::flagWarning,"mpg123:debug: TODO: finish lame detection...");
								}
							lame_offset += 9;
							/* the 4 big bits are tag revision, the small bits vbr method */
							lame_vbr = bsbuf[lame_offset] & 15;
							debug_print(CLogFile::flagWarning,"mpg123:debug1: Info: rev %u", bsbuf[lame_offset] >> 4);
							debug_print(CLogFile::flagWarning,"mpg123:debug1: Info: vbr mode %u", lame_vbr);
							lame_offset += 1;
							switch(lame_vbr) {
								/* from rev1 proposal... not sure if all good in practice */
								case 1:
								case 8: vbr=CBR; break;
								case 2:
								case 9: vbr=ABR; break;
								default: vbr=VBR; /* 00==unknown is taken as VBR */
								}
							/* skipping: lowpass filter value */
							lame_offset += 1;
							/* replaygain */
							/* 32bit float: peak amplitude -- why did I parse it as int before??*/
							/* Ah, yes, lame seems to store it as int since some day in 2003; I've only seen zeros anyway until now, bah! */
							if(  (bsbuf[lame_offset] != 0)
								|| (bsbuf[lame_offset+1] != 0)
								|| (bsbuf[lame_offset+2] != 0)
								|| (bsbuf[lame_offset+3] != 0)
								)	{
								debug_print(CLogFile::flagWarning,"mpg123:debug: Wow! Is there _really_ a non-zero peak value? Now is it stored as float or int - how should I know?");
								peak = *(float*)(bsbuf+lame_offset);
								}
							debug_print(CLogFile::flagWarning,"mpg123:debug1: Info: peak = %f (I won't use this)", peak);
							peak = 0; /* until better times arrived */
							lame_offset += 4;
							/*
								ReplayGain values - lame only writes radio mode gain...
								16bit gain, 3 bits name, 3 bits originator, sign (1=-, 0=+), dB value*10 in 9 bits (fixed point)
								ignore the setting if name or originator == 000!
								radio 0 0 1 0 1 1 1 0 0 1 1 1 1 1 0 1
								audiophile 0 1 0 0 1 0 0 0 0 0 0 1 0 1 0 0
							*/
							
							for(i=0; i<2; ++i)	{
								unsigned char origin = (bsbuf[lame_offset] >> 2) & 0x7; /* the 3 bits after that... */
								if(origin != 0)	{
									unsigned char gt = bsbuf[lame_offset] >> 5; /* only first 3 bits */
									if(gt == 1) 
										gt = 0; // radio 
									else if(gt == 2) 
										gt = 1; // audiophile 
									else 
										continue;
									/* get the 9 bits into a number, divide by 10, multiply sign... happy bit banging */
									replay_gain[0] = ((bsbuf[lame_offset] & 0x2) ? -0.1 : 0.1) * (make_short(bsbuf, lame_offset) & 0x1f);
									}
								lame_offset += 2;
								}
							debug_print(CLogFile::flagWarning,"mpg123:debug1: Info: Radio Gain = %03.1fdB", replay_gain[0]);
							debug_print(CLogFile::flagWarning,"mpg123:debug1: Info: Audiophile Gain = %03.1fdB", replay_gain[1]);
							for(i=0; i<2; ++i) {
								if(rva_level[i] <= 0)	{
									rva_peak[i] = 0; /* at some time the parsed peak should be used */
									rva_gain[i] = replay_gain[i];
									rva_level[i] = 0;
									}
								}
							lame_offset += 1; /* skipping encoding flags byte */
							if(vbr == ABR) {
								abr_rate = bsbuf[lame_offset];
								debug_print(CLogFile::flagWarning,"mpg123:debug1: Info: ABR rate = %u", abr_rate);
								}
							lame_offset += 1;
							/* encoder delay and padding, two 12 bit values... lame does write them from int ...*/
							#ifdef GAPLESS
							if(param.gapless)	{
								/*
									Temporary hack that doesn't work with seeking and also is not waterproof but works most of the time;
									in future the lame delay/padding and frame number info should be passed to layer3.c and the junk samples avoided at the source.
								*/
								unsigned long length = track_frames * spf(fr);
								unsigned long skipbegin = DECODER_DELAY + ((((int) bsbuf[lame_offset]) << 4) | (((int) bsbuf[lame_offset+1]) >> 4));
								unsigned long skipend = -DECODER_DELAY + (((((int) bsbuf[lame_offset+1]) << 8) | (((int) bsbuf[lame_offset+2]))) & 0xfff);
								debug_print(CLogFile::flagInfo,"preparing gapless mode for layer3: length %lu, skipbegin %lu, skipend %lu", length, skipbegin, skipend);
								if(length > 1)
									layer3_gapless_init(skipbegin+GAP_SHIFT, (skipend < length) ? length-skipend+GAP_SHIFT : length+GAP_SHIFT);
								}
							#endif
							}
						/* switch buffer back ... */
						bsbuf = bsspace[bsnum]+512;
						bsnum = (bsnum + 1) & 1;
						goto read_again;
						}
					}
				}
			} /* end block for Xing/Lame/Info tag */
		firsthead = newhead; /* _now_ it's time to store it... the first real header */
		debug_print(CLogFile::flagInfo,"mpg123:debug1: firsthead: %08x", firsthead);
		// now adjust volume 
		do_rva();
		// and print id3/stream info 
		if(!Param.quiet) {
//			print_id3_tag(reader->flags & READER_ID3TAG ? reader->id3buf : NULL);
//			if(icy.name.fill) 
//				fprintf(stderr, "ICY-NAME: %s\n", icy.name.p);
//			if(icy.url.fill) 
//				fprintf(stderr, "ICY-URL: %s\n", icy.url.p);
			}
		}

  bsi.bitindex = 0;
  bsi.wordpointer= (BYTE *)bsbuf;

  if(Param.halfSpeed && fr.lay == 3)
    memcpy(ssave, bsbuf, ssize);


	if(++mean_frames != 0) {
		mean_framesize = ((mean_frames-1)*mean_framesize+computeBpf(&fr)) / mean_frames;
		}
	// index the position 
	if(INDEX_SIZE > 0) {	/* any sane compiler should make a no-brainer out of this */
		if(fr.num == reader->frame_index.fill*reader->frame_index.step) {
			if(reader->frame_index.fill == INDEX_SIZE) {
				size_t c;
				// increase step, reduce fill 
				reader->frame_index.step *= 2;
				reader->frame_index.fill /= 2; // divisable by 2! 
				for(c=0; c < reader->frame_index.fill; ++c)	{
					reader->frame_index.data[c] = reader->frame_index.data[2*c];
					}
				}
			if(fr.num == reader->frame_index.fill*reader->frame_index.step) {
				reader->frame_index.data[reader->frame_index.fill] = framepos;
				++reader->frame_index.fill;
				}
			}
		}
	++fr.num;

  return 1;
	}

void CJoshuaMP3::backFrame(int num) {

	reader->backFrame(this,num);
  readFrame();
  readFrame();
  
  if(fr.lay == 3) {
    setPointer(512);
		}
	}

/*
 * decode a header and write the information
 * into the frame structure
 */
int CJoshuaMP3::decodeHeader(DWORD newhead) {
	struct MP3_FRAME_HEADER *mph=(struct MP3_FRAME_HEADER *)&newhead;		// GD 2023

  if(!headCheck(newhead))
    return 0;

  if(newhead & (1<<20) ) {
    fr.lsf = (newhead & (1<<19)) ? 0x0 : 0x1;
    fr.mpeg25 = 0;
    }
  else {
    fr.lsf = 1;
    fr.mpeg25 = 1;
    }
    
  if(!Param.tryResync || !oldhead) {
    // If "tryresync" is true, assume that certain parameters do not change within the stream!
    fr.lay = 4-((newhead>>17) & 3);
    if(((newhead>>10) & 0x3) == 0x3) {
      debug_print(CLogFile::flagError2,"Stream error");
//      exit(1);
      }
    if(fr.mpeg25) {
      fr.samplingFrequency = 6 + ((newhead>>10) & 0x3);
      }
    else
      fr.samplingFrequency = ((newhead >> 10) & 0x3) + (fr.lsf*3);
    fr.errorProtection = ((newhead >> 16) & 0x1) ^ 0x1;
		}

  fr.bitrateIndex = ((newhead>>12) & 0xf);		mph->bitRate;
  fr.padding   = ((newhead>>9) & 0x1);		mph->padBit;
  fr.extension = ((newhead>>8) & 0x1);		mph->privBit;
  fr.mode      = (enum MPG_MODE)((newhead>>6) & 0x3);		mph->mode;
  fr.modeExt   = ((newhead>>4) & 0x3);		mph->modeExt;
  fr.copyright = ((newhead>>3) & 0x1);		mph->copy;
  fr.original  = ((newhead>>2) & 0x1);		mph->original;
  fr.emphasis  = newhead & 0x3;		mph->emphasis;

  fr.stereo    = (fr.mode == MPG_MD_MONO) ? 1 : 2;

  oldhead = newhead;

  if(!fr.bitrateIndex) {
    debug_print(CLogFile::flagError2,"Free format not supported: (head %08lx)",newhead);
    return (0);
    }

  switch(fr.lay) {
    case 1:
			fr.doLayer = DoLayer1;
#ifdef VARMODESUPPORT
        if(varmode) {
          debug_print(CLogFile::flagError2,"Sorry, layer-1 not supported in varmode."); 
          return (0);
        }
#endif
      fr.framesize  = (long)tabsel_123[fr.lsf][0][fr.bitrateIndex] * 12000;
      fr.framesize /= freqs[fr.samplingFrequency];
      fr.framesize  = ((fr.framesize+fr.padding) << 2)-4;
      break;
    case 2:
			fr.doLayer = DoLayer2;
#ifdef VARMODESUPPORT
      if(varmode) {
        debug_print(CLogFile::flagError2,"Sorry, layer-2 not supported in varmode."); 
        return (0);
        }
#endif
      fr.framesize = (long) tabsel_123[fr.lsf][1][fr.bitrateIndex] * 144000;
      fr.framesize /= freqs[fr.samplingFrequency];
      fr.framesize += fr.padding - 4;
      break;
    case 3:
      fr.doLayer = DoLayer3;
      if(fr.lsf)
        ssize = (fr.stereo == 1) ? 9 : 17;
      else
        ssize = (fr.stereo == 1) ? 17 : 32;
      if(fr.errorProtection)
        ssize += 2;
      fr.framesize  = (long) tabsel_123[fr.lsf][2][fr.bitrateIndex] * 144000;
      fr.framesize /= freqs[fr.samplingFrequency]<<(fr.lsf);
      fr.framesize = fr.framesize + fr.padding - 4;
      break; 
    default:
      debug_print(CLogFile::flagError2,"Sorry, unknown layer type."); 
      return (0);
    }
  return 1;
	}


void CJoshuaMP3::printHeader() {
	static char *modes[4] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Single-Channel" };
	static char *layers[4] = { "Unknown" , "I", "II", "III" };
	char myBuf[128],myBuf1[128],myBuf2[128];

	wsprintf(myBuf,"MPEG %s, Layer: %s, Freq: %ld, mode: %s, modext: %d, BPF : %d", 
		fr.mpeg25 ? "2.5" : (fr.lsf ? "2.0" : "1.0"),
		layers[fr.lay],freqs[fr.samplingFrequency],
		modes[fr.mode],fr.modeExt,fr.framesize+4);
	wsprintf(myBuf1,"Channels: %d, copyright: %s, original: %s, CRC: %s, emphasis: %d.",
		fr.stereo,fr.copyright?"Yes":"No",
		fr.original?"Yes":"No",fr.errorProtection ? "Yes":"No",
		fr.emphasis);
	wsprintf(myBuf2,"Bitrate: %d Kbits/s, Extension value: %d",
		tabsel_123[fr.lsf][fr.lay-1][fr.bitrateIndex],fr.extension);
	debug_print(CLogFile::flagInfo,"%s %s %s",myBuf,myBuf1,myBuf2);
	}

void CJoshuaMP3::printHeaderCompact() {
	static char *modes[4] = { "stereo", "joint-stereo", "dual-channel", "mono" };
	static char *layers[4] = { "Unknown" , "I", "II", "III" };
 
	debug_print(CLogFile::flagError2,"MPEG %s layer %s, %d kbit/s, %d Hz %s",
		fr.mpeg25 ? "2.5" : (fr.lsf ? "2.0" : "1.0"),
		layers[fr.lay],
		tabsel_123[fr.lsf][fr.lay-1][fr.bitrateIndex],
		freqs[fr.samplingFrequency], modes[fr.mode]);
	}


/* 
 * Mpeg Layer-3 audio decoder 
 * --------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp.
 * All rights reserved. See also 'README'
 *
 * - I'm currently working on that .. needs a few more optimizations,
 *   though the code is now fast enough to run in realtime on a 133Mhz 486
 * - a few personal notes are in german .. 
 *
 * used source: 
 *   mpeg1_iis package
 */ 

/*
 * huffman tables ... recalculated to work with my optimzed
 * decoder scheme (MH)
 * 
 * probably we could save a few bytes of memory, because the 
 * smaller tables are often the part of a bigger table
 */


short int CJoshuaMP3::tab0[] = { 
   0
};

short int CJoshuaMP3::tab1[] = {
  -5,  -3,  -1,  17,   1,  16,   0
};

short int CJoshuaMP3::tab2[] = {
 -15, -11,  -9,  -5,  -3,  -1,  34,   2,  18,  -1,  33,  32,  17,  -1,   1,
  16,   0
};

short int CJoshuaMP3::tab3[] = {
 -13, -11,  -9,  -5,  -3,  -1,  34,   2,  18,  -1,  33,  32,  16,  17,  -1,
   1,   0
};

short int CJoshuaMP3::tab5[] = {
 -29, -25, -23, -15,  -7,  -5,  -3,  -1,  51,  35,  50,  49,  -3,  -1,  19,
   3,  -1,  48,  34,  -3,  -1,  18,  33,  -1,   2,  32,  17,  -1,   1,  16,
   0
};

short int CJoshuaMP3::tab6[] = {
 -25, -19, -13,  -9,  -5,  -3,  -1,  51,   3,  35,  -1,  50,  48,  -1,  19,
  49,  -3,  -1,  34,   2,  18,  -3,  -1,  33,  32,   1,  -1,  17,  -1,  16,
   0
};

short int CJoshuaMP3::tab7[] = {
 -69, -65, -57, -39, -29, -17, -11,  -7,  -3,  -1,  85,  69,  -1,  84,  83,
  -1,  53,  68,  -3,  -1,  37,  82,  21,  -5,  -1,  81,  -1,   5,  52,  -1,
  80,  -1,  67,  51,  -5,  -3,  -1,  36,  66,  20,  -1,  65,  64, -11,  -7,
  -3,  -1,   4,  35,  -1,  50,   3,  -1,  19,  49,  -3,  -1,  48,  34,  18,
  -5,  -1,  33,  -1,   2,  32,  17,  -1,   1,  16,   0
};

short int CJoshuaMP3::tab8[] = {
 -65, -63, -59, -45, -31, -19, -13,  -7,  -5,  -3,  -1,  85,  84,  69,  83,
  -3,  -1,  53,  68,  37,  -3,  -1,  82,   5,  21,  -5,  -1,  81,  -1,  52,
  67,  -3,  -1,  80,  51,  36,  -5,  -3,  -1,  66,  20,  65,  -3,  -1,   4,
  64,  -1,  35,  50,  -9,  -7,  -3,  -1,  19,  49,  -1,   3,  48,  34,  -1,
   2,  32,  -1,  18,  33,  17,  -3,  -1,   1,  16,   0
};

short int CJoshuaMP3::tab9[] = {
 -63, -53, -41, -29, -19, -11,  -5,  -3,  -1,  85,  69,  53,  -1,  83,  -1,
  84,   5,  -3,  -1,  68,  37,  -1,  82,  21,  -3,  -1,  81,  52,  -1,  67,
  -1,  80,   4,  -7,  -3,  -1,  36,  66,  -1,  51,  64,  -1,  20,  65,  -5,
  -3,  -1,  35,  50,  19,  -1,  49,  -1,   3,  48,  -5,  -3,  -1,  34,   2,
  18,  -1,  33,  32,  -3,  -1,  17,   1,  -1,  16,   0
};

short int CJoshuaMP3::tab10[] = {
-125,-121,-111, -83, -55, -35, -21, -13,  -7,  -3,  -1, 119, 103,  -1, 118,
  87,  -3,  -1, 117, 102,  71,  -3,  -1, 116,  86,  -1, 101,  55,  -9,  -3,
  -1, 115,  70,  -3,  -1,  85,  84,  99,  -1,  39, 114, -11,  -5,  -3,  -1,
 100,   7, 112,  -1,  98,  -1,  69,  53,  -5,  -1,   6,  -1,  83,  68,  23,
 -17,  -5,  -1, 113,  -1,  54,  38,  -5,  -3,  -1,  37,  82,  21,  -1,  81,
  -1,  52,  67,  -3,  -1,  22,  97,  -1,  96,  -1,   5,  80, -19, -11,  -7,
  -3,  -1,  36,  66,  -1,  51,   4,  -1,  20,  65,  -3,  -1,  64,  35,  -1,
  50,   3,  -3,  -1,  19,  49,  -1,  48,  34,  -7,  -3,  -1,  18,  33,  -1,
   2,  32,  17,  -1,   1,  16,   0
};

short int CJoshuaMP3::tab11[] = {
-121,-113, -89, -59, -43, -27, -17,  -7,  -3,  -1, 119, 103,  -1, 118, 117,
  -3,  -1, 102,  71,  -1, 116,  -1,  87,  85,  -5,  -3,  -1,  86, 101,  55,
  -1, 115,  70,  -9,  -7,  -3,  -1,  69,  84,  -1,  53,  83,  39,  -1, 114,
  -1, 100,   7,  -5,  -1, 113,  -1,  23, 112,  -3,  -1,  54,  99,  -1,  96,
  -1,  68,  37, -13,  -7,  -5,  -3,  -1,  82,   5,  21,  98,  -3,  -1,  38,
   6,  22,  -5,  -1,  97,  -1,  81,  52,  -5,  -1,  80,  -1,  67,  51,  -1,
  36,  66, -15, -11,  -7,  -3,  -1,  20,  65,  -1,   4,  64,  -1,  35,  50,
  -1,  19,  49,  -5,  -3,  -1,   3,  48,  34,  33,  -5,  -1,  18,  -1,   2,
  32,  17,  -3,  -1,   1,  16,   0
};

short int CJoshuaMP3::tab12[] = {
-115, -99, -73, -45, -27, -17,  -9,  -5,  -3,  -1, 119, 103, 118,  -1,  87,
 117,  -3,  -1, 102,  71,  -1, 116, 101,  -3,  -1,  86,  55,  -3,  -1, 115,
  85,  39,  -7,  -3,  -1, 114,  70,  -1, 100,  23,  -5,  -1, 113,  -1,   7,
 112,  -1,  54,  99, -13,  -9,  -3,  -1,  69,  84,  -1,  68,  -1,   6,   5,
  -1,  38,  98,  -5,  -1,  97,  -1,  22,  96,  -3,  -1,  53,  83,  -1,  37,
  82, -17,  -7,  -3,  -1,  21,  81,  -1,  52,  67,  -5,  -3,  -1,  80,   4,
  36,  -1,  66,  20,  -3,  -1,  51,  65,  -1,  35,  50, -11,  -7,  -5,  -3,
  -1,  64,   3,  48,  19,  -1,  49,  34,  -1,  18,  33,  -7,  -5,  -3,  -1,
   2,  32,   0,  17,  -1,   1,  16
};

short int CJoshuaMP3::tab13[] = {
-509,-503,-475,-405,-333,-265,-205,-153,-115, -83, -53, -35, -21, -13,  -9,
  -7,  -5,  -3,  -1, 254, 252, 253, 237, 255,  -1, 239, 223,  -3,  -1, 238,
 207,  -1, 222, 191,  -9,  -3,  -1, 251, 206,  -1, 220,  -1, 175, 233,  -1,
 236, 221,  -9,  -5,  -3,  -1, 250, 205, 190,  -1, 235, 159,  -3,  -1, 249,
 234,  -1, 189, 219, -17,  -9,  -3,  -1, 143, 248,  -1, 204,  -1, 174, 158,
  -5,  -1, 142,  -1, 127, 126, 247,  -5,  -1, 218,  -1, 173, 188,  -3,  -1,
 203, 246, 111, -15,  -7,  -3,  -1, 232,  95,  -1, 157, 217,  -3,  -1, 245,
 231,  -1, 172, 187,  -9,  -3,  -1,  79, 244,  -3,  -1, 202, 230, 243,  -1,
  63,  -1, 141, 216, -21,  -9,  -3,  -1,  47, 242,  -3,  -1, 110, 156,  15,
  -5,  -3,  -1, 201,  94, 171,  -3,  -1, 125, 215,  78, -11,  -5,  -3,  -1,
 200, 214,  62,  -1, 185,  -1, 155, 170,  -1,  31, 241, -23, -13,  -5,  -1,
 240,  -1, 186, 229,  -3,  -1, 228, 140,  -1, 109, 227,  -5,  -1, 226,  -1,
  46,  14,  -1,  30, 225, -15,  -7,  -3,  -1, 224,  93,  -1, 213, 124,  -3,
  -1, 199,  77,  -1, 139, 184,  -7,  -3,  -1, 212, 154,  -1, 169, 108,  -1,
 198,  61, -37, -21,  -9,  -5,  -3,  -1, 211, 123,  45,  -1, 210,  29,  -5,
  -1, 183,  -1,  92, 197,  -3,  -1, 153, 122, 195,  -7,  -5,  -3,  -1, 167,
 151,  75, 209,  -3,  -1,  13, 208,  -1, 138, 168, -11,  -7,  -3,  -1,  76,
 196,  -1, 107, 182,  -1,  60,  44,  -3,  -1, 194,  91,  -3,  -1, 181, 137,
  28, -43, -23, -11,  -5,  -1, 193,  -1, 152,  12,  -1, 192,  -1, 180, 106,
  -5,  -3,  -1, 166, 121,  59,  -1, 179,  -1, 136,  90, -11,  -5,  -1,  43,
  -1, 165, 105,  -1, 164,  -1, 120, 135,  -5,  -1, 148,  -1, 119, 118, 178,
 -11,  -3,  -1,  27, 177,  -3,  -1,  11, 176,  -1, 150,  74,  -7,  -3,  -1,
  58, 163,  -1,  89, 149,  -1,  42, 162, -47, -23,  -9,  -3,  -1,  26, 161,
  -3,  -1,  10, 104, 160,  -5,  -3,  -1, 134,  73, 147,  -3,  -1,  57,  88,
  -1, 133, 103,  -9,  -3,  -1,  41, 146,  -3,  -1,  87, 117,  56,  -5,  -1,
 131,  -1, 102,  71,  -3,  -1, 116,  86,  -1, 101, 115, -11,  -3,  -1,  25,
 145,  -3,  -1,   9, 144,  -1,  72, 132,  -7,  -5,  -1, 114,  -1,  70, 100,
  40,  -1, 130,  24, -41, -27, -11,  -5,  -3,  -1,  55,  39,  23,  -1, 113,
  -1,  85,   7,  -7,  -3,  -1, 112,  54,  -1,  99,  69,  -3,  -1,  84,  38,
  -1,  98,  53,  -5,  -1, 129,  -1,   8, 128,  -3,  -1,  22,  97,  -1,   6,
  96, -13,  -9,  -5,  -3,  -1,  83,  68,  37,  -1,  82,   5,  -1,  21,  81,
  -7,  -3,  -1,  52,  67,  -1,  80,  36,  -3,  -1,  66,  51,  20, -19, -11,
  -5,  -1,  65,  -1,   4,  64,  -3,  -1,  35,  50,  19,  -3,  -1,  49,   3,
  -1,  48,  34,  -3,  -1,  18,  33,  -1,   2,  32,  -3,  -1,  17,   1,  16,
   0
};

short int CJoshuaMP3::tab15[] = {
-495,-445,-355,-263,-183,-115, -77, -43, -27, -13,  -7,  -3,  -1, 255, 239,
  -1, 254, 223,  -1, 238,  -1, 253, 207,  -7,  -3,  -1, 252, 222,  -1, 237,
 191,  -1, 251,  -1, 206, 236,  -7,  -3,  -1, 221, 175,  -1, 250, 190,  -3,
  -1, 235, 205,  -1, 220, 159, -15,  -7,  -3,  -1, 249, 234,  -1, 189, 219,
  -3,  -1, 143, 248,  -1, 204, 158,  -7,  -3,  -1, 233, 127,  -1, 247, 173,
  -3,  -1, 218, 188,  -1, 111,  -1, 174,  15, -19, -11,  -3,  -1, 203, 246,
  -3,  -1, 142, 232,  -1,  95, 157,  -3,  -1, 245, 126,  -1, 231, 172,  -9,
  -3,  -1, 202, 187,  -3,  -1, 217, 141,  79,  -3,  -1, 244,  63,  -1, 243,
 216, -33, -17,  -9,  -3,  -1, 230,  47,  -1, 242,  -1, 110, 240,  -3,  -1,
  31, 241,  -1, 156, 201,  -7,  -3,  -1,  94, 171,  -1, 186, 229,  -3,  -1,
 125, 215,  -1,  78, 228, -15,  -7,  -3,  -1, 140, 200,  -1,  62, 109,  -3,
  -1, 214, 227,  -1, 155, 185,  -7,  -3,  -1,  46, 170,  -1, 226,  30,  -5,
  -1, 225,  -1,  14, 224,  -1,  93, 213, -45, -25, -13,  -7,  -3,  -1, 124,
 199,  -1,  77, 139,  -1, 212,  -1, 184, 154,  -7,  -3,  -1, 169, 108,  -1,
 198,  61,  -1, 211, 210,  -9,  -5,  -3,  -1,  45,  13,  29,  -1, 123, 183,
  -5,  -1, 209,  -1,  92, 208,  -1, 197, 138, -17,  -7,  -3,  -1, 168,  76,
  -1, 196, 107,  -5,  -1, 182,  -1, 153,  12,  -1,  60, 195,  -9,  -3,  -1,
 122, 167,  -1, 166,  -1, 192,  11,  -1, 194,  -1,  44,  91, -55, -29, -15,
  -7,  -3,  -1, 181,  28,  -1, 137, 152,  -3,  -1, 193,  75,  -1, 180, 106,
  -5,  -3,  -1,  59, 121, 179,  -3,  -1, 151, 136,  -1,  43,  90, -11,  -5,
  -1, 178,  -1, 165,  27,  -1, 177,  -1, 176, 105,  -7,  -3,  -1, 150,  74,
  -1, 164, 120,  -3,  -1, 135,  58, 163, -17,  -7,  -3,  -1,  89, 149,  -1,
  42, 162,  -3,  -1,  26, 161,  -3,  -1,  10, 160, 104,  -7,  -3,  -1, 134,
  73,  -1, 148,  57,  -5,  -1, 147,  -1, 119,   9,  -1,  88, 133, -53, -29,
 -13,  -7,  -3,  -1,  41, 103,  -1, 118, 146,  -1, 145,  -1,  25, 144,  -7,
  -3,  -1,  72, 132,  -1,  87, 117,  -3,  -1,  56, 131,  -1, 102,  71,  -7,
  -3,  -1,  40, 130,  -1,  24, 129,  -7,  -3,  -1, 116,   8,  -1, 128,  86,
  -3,  -1, 101,  55,  -1, 115,  70, -17,  -7,  -3,  -1,  39, 114,  -1, 100,
  23,  -3,  -1,  85, 113,  -3,  -1,   7, 112,  54,  -7,  -3,  -1,  99,  69,
  -1,  84,  38,  -3,  -1,  98,  22,  -3,  -1,   6,  96,  53, -33, -19,  -9,
  -5,  -1,  97,  -1,  83,  68,  -1,  37,  82,  -3,  -1,  21,  81,  -3,  -1,
   5,  80,  52,  -7,  -3,  -1,  67,  36,  -1,  66,  51,  -1,  65,  -1,  20,
   4,  -9,  -3,  -1,  35,  50,  -3,  -1,  64,   3,  19,  -3,  -1,  49,  48,
  34,  -9,  -7,  -3,  -1,  18,  33,  -1,   2,  32,  17,  -3,  -1,   1,  16,
   0
};

short int CJoshuaMP3::tab16[] = {
-509,-503,-461,-323,-103, -37, -27, -15,  -7,  -3,  -1, 239, 254,  -1, 223,
 253,  -3,  -1, 207, 252,  -1, 191, 251,  -5,  -1, 175,  -1, 250, 159,  -3,
  -1, 249, 248, 143,  -7,  -3,  -1, 127, 247,  -1, 111, 246, 255,  -9,  -5,
  -3,  -1,  95, 245,  79,  -1, 244, 243, -53,  -1, 240,  -1,  63, -29, -19,
 -13,  -7,  -5,  -1, 206,  -1, 236, 221, 222,  -1, 233,  -1, 234, 217,  -1,
 238,  -1, 237, 235,  -3,  -1, 190, 205,  -3,  -1, 220, 219, 174, -11,  -5,
  -1, 204,  -1, 173, 218,  -3,  -1, 126, 172, 202,  -5,  -3,  -1, 201, 125,
  94, 189, 242, -93,  -5,  -3,  -1,  47,  15,  31,  -1, 241, -49, -25, -13,
  -5,  -1, 158,  -1, 188, 203,  -3,  -1, 142, 232,  -1, 157, 231,  -7,  -3,
  -1, 187, 141,  -1, 216, 110,  -1, 230, 156, -13,  -7,  -3,  -1, 171, 186,
  -1, 229, 215,  -1,  78,  -1, 228, 140,  -3,  -1, 200,  62,  -1, 109,  -1,
 214, 155, -19, -11,  -5,  -3,  -1, 185, 170, 225,  -1, 212,  -1, 184, 169,
  -5,  -1, 123,  -1, 183, 208, 227,  -7,  -3,  -1,  14, 224,  -1,  93, 213,
  -3,  -1, 124, 199,  -1,  77, 139, -75, -45, -27, -13,  -7,  -3,  -1, 154,
 108,  -1, 198,  61,  -3,  -1,  92, 197,  13,  -7,  -3,  -1, 138, 168,  -1,
 153,  76,  -3,  -1, 182, 122,  60, -11,  -5,  -3,  -1,  91, 137,  28,  -1,
 192,  -1, 152, 121,  -1, 226,  -1,  46,  30, -15,  -7,  -3,  -1, 211,  45,
  -1, 210, 209,  -5,  -1,  59,  -1, 151, 136,  29,  -7,  -3,  -1, 196, 107,
  -1, 195, 167,  -1,  44,  -1, 194, 181, -23, -13,  -7,  -3,  -1, 193,  12,
  -1,  75, 180,  -3,  -1, 106, 166, 179,  -5,  -3,  -1,  90, 165,  43,  -1,
 178,  27, -13,  -5,  -1, 177,  -1,  11, 176,  -3,  -1, 105, 150,  -1,  74,
 164,  -5,  -3,  -1, 120, 135, 163,  -3,  -1,  58,  89,  42, -97, -57, -33,
 -19, -11,  -5,  -3,  -1, 149, 104, 161,  -3,  -1, 134, 119, 148,  -5,  -3,
  -1,  73,  87, 103, 162,  -5,  -1,  26,  -1,  10, 160,  -3,  -1,  57, 147,
  -1,  88, 133,  -9,  -3,  -1,  41, 146,  -3,  -1, 118,   9,  25,  -5,  -1,
 145,  -1, 144,  72,  -3,  -1, 132, 117,  -1,  56, 131, -21, -11,  -5,  -3,
  -1, 102,  40, 130,  -3,  -1,  71, 116,  24,  -3,  -1, 129, 128,  -3,  -1,
   8,  86,  55,  -9,  -5,  -1, 115,  -1, 101,  70,  -1,  39, 114,  -5,  -3,
  -1, 100,  85,   7,  23, -23, -13,  -5,  -1, 113,  -1, 112,  54,  -3,  -1,
  99,  69,  -1,  84,  38,  -3,  -1,  98,  22,  -1,  97,  -1,   6,  96,  -9,
  -5,  -1,  83,  -1,  53,  68,  -1,  37,  82,  -1,  81,  -1,  21,   5, -33,
 -23, -13,  -7,  -3,  -1,  52,  67,  -1,  80,  36,  -3,  -1,  66,  51,  20,
  -5,  -1,  65,  -1,   4,  64,  -1,  35,  50,  -3,  -1,  19,  49,  -3,  -1,
   3,  48,  34,  -3,  -1,  18,  33,  -1,   2,  32,  -3,  -1,  17,   1,  16,
   0
};

short int CJoshuaMP3::tab24[] = {
-451,-117, -43, -25, -15,  -7,  -3,  -1, 239, 254,  -1, 223, 253,  -3,  -1,
 207, 252,  -1, 191, 251,  -5,  -1, 250,  -1, 175, 159,  -1, 249, 248,  -9,
  -5,  -3,  -1, 143, 127, 247,  -1, 111, 246,  -3,  -1,  95, 245,  -1,  79,
 244, -71,  -7,  -3,  -1,  63, 243,  -1,  47, 242,  -5,  -1, 241,  -1,  31,
 240, -25,  -9,  -1,  15,  -3,  -1, 238, 222,  -1, 237, 206,  -7,  -3,  -1,
 236, 221,  -1, 190, 235,  -3,  -1, 205, 220,  -1, 174, 234, -15,  -7,  -3,
  -1, 189, 219,  -1, 204, 158,  -3,  -1, 233, 173,  -1, 218, 188,  -7,  -3,
  -1, 203, 142,  -1, 232, 157,  -3,  -1, 217, 126,  -1, 231, 172, 255,-235,
-143, -77, -45, -25, -15,  -7,  -3,  -1, 202, 187,  -1, 141, 216,  -5,  -3,
  -1,  14, 224,  13, 230,  -5,  -3,  -1, 110, 156, 201,  -1,  94, 186,  -9,
  -5,  -1, 229,  -1, 171, 125,  -1, 215, 228,  -3,  -1, 140, 200,  -3,  -1,
  78,  46,  62, -15,  -7,  -3,  -1, 109, 214,  -1, 227, 155,  -3,  -1, 185,
 170,  -1, 226,  30,  -7,  -3,  -1, 225,  93,  -1, 213, 124,  -3,  -1, 199,
  77,  -1, 139, 184, -31, -15,  -7,  -3,  -1, 212, 154,  -1, 169, 108,  -3,
  -1, 198,  61,  -1, 211,  45,  -7,  -3,  -1, 210,  29,  -1, 123, 183,  -3,
  -1, 209,  92,  -1, 197, 138, -17,  -7,  -3,  -1, 168, 153,  -1,  76, 196,
  -3,  -1, 107, 182,  -3,  -1, 208,  12,  60,  -7,  -3,  -1, 195, 122,  -1,
 167,  44,  -3,  -1, 194,  91,  -1, 181,  28, -57, -35, -19,  -7,  -3,  -1,
 137, 152,  -1, 193,  75,  -5,  -3,  -1, 192,  11,  59,  -3,  -1, 176,  10,
  26,  -5,  -1, 180,  -1, 106, 166,  -3,  -1, 121, 151,  -3,  -1, 160,   9,
 144,  -9,  -3,  -1, 179, 136,  -3,  -1,  43,  90, 178,  -7,  -3,  -1, 165,
  27,  -1, 177, 105,  -1, 150, 164, -17,  -9,  -5,  -3,  -1,  74, 120, 135,
  -1,  58, 163,  -3,  -1,  89, 149,  -1,  42, 162,  -7,  -3,  -1, 161, 104,
  -1, 134, 119,  -3,  -1,  73, 148,  -1,  57, 147, -63, -31, -15,  -7,  -3,
  -1,  88, 133,  -1,  41, 103,  -3,  -1, 118, 146,  -1,  25, 145,  -7,  -3,
  -1,  72, 132,  -1,  87, 117,  -3,  -1,  56, 131,  -1, 102,  40, -17,  -7,
  -3,  -1, 130,  24,  -1,  71, 116,  -5,  -1, 129,  -1,   8, 128,  -1,  86,
 101,  -7,  -5,  -1,  23,  -1,   7, 112, 115,  -3,  -1,  55,  39, 114, -15,
  -7,  -3,  -1,  70, 100,  -1,  85, 113,  -3,  -1,  54,  99,  -1,  69,  84,
  -7,  -3,  -1,  38,  98,  -1,  22,  97,  -5,  -3,  -1,   6,  96,  53,  -1,
  83,  68, -51, -37, -23, -15,  -9,  -3,  -1,  37,  82,  -1,  21,  -1,   5,
  80,  -1,  81,  -1,  52,  67,  -3,  -1,  36,  66,  -1,  51,  20,  -9,  -5,
  -1,  65,  -1,   4,  64,  -1,  35,  50,  -1,  19,  49,  -7,  -5,  -3,  -1,
   3,  48,  34,  18,  -1,  33,  -1,   2,  32,  -3,  -1,  17,   1,  -1,  16,
   0
	};

short int CJoshuaMP3::tab_c0[] = {
 -29, -21, -13,  -7,  -3,  -1,  11,  15,  -1,  13,  14,  -3,  -1,   7,   5,
   9,  -3,  -1,   6,   3,  -1,  10,  12,  -3,  -1,   2,   1,  -1,   4,   8,
   0
	};

short int CJoshuaMP3::tab_c1[] = {
 -15,  -7,  -3,  -1,  15,  14,  -1,  13,  12,  -3,  -1,  11,  10,  -1,   9,
   8,  -7,  -3,  -1,   7,   6,  -1,   5,   4,  -3,  -1,   3,   2,  -1,   1,
   0
	};



struct NEW_HUFF CJoshuaMP3::ht[] = {
 { /* 0 */ 0 , tab0  } ,
 { /* 2 */ 0 , tab1  } ,
 { /* 3 */ 0 , tab2  } ,
 { /* 3 */ 0 , tab3  } ,
 { /* 0 */ 0 , tab0  } ,
 { /* 4 */ 0 , tab5  } ,
 { /* 4 */ 0 , tab6  } ,
 { /* 6 */ 0 , tab7  } ,
 { /* 6 */ 0 , tab8  } ,
 { /* 6 */ 0 , tab9  } ,
 { /* 8 */ 0 , tab10 } ,
 { /* 8 */ 0 , tab11 } ,
 { /* 8 */ 0 , tab12 } ,
 { /* 16 */ 0 , tab13 } ,
 { /* 0  */ 0 , tab0  } ,
 { /* 16 */ 0 , tab15 } ,

 { /* 16 */ 1 , tab16 } ,
 { /* 16 */ 2 , tab16 } ,
 { /* 16 */ 3 , tab16 } ,
 { /* 16 */ 4 , tab16 } ,
 { /* 16 */ 6 , tab16 } ,
 { /* 16 */ 8 , tab16 } ,
 { /* 16 */ 10, tab16 } ,
 { /* 16 */ 13, tab16 } ,
 { /* 16 */ 4 , tab24 } ,
 { /* 16 */ 5 , tab24 } ,
 { /* 16 */ 6 , tab24 } ,
 { /* 16 */ 7 , tab24 } ,
 { /* 16 */ 8 , tab24 } ,
 { /* 16 */ 9 , tab24 } ,
 { /* 16 */ 11, tab24 } ,
 { /* 16 */ 13, tab24 }
	};

struct NEW_HUFF CJoshuaMP3::htc[] = {
  { /* 1 , 1 , */ 0 , tab_c0 } ,
  { /* 1 , 1 , */ 0 , tab_c1 }
	};




struct BAND_INFO_STRUCT CJoshuaMP3::bandInfo[] = { 

// MPEG 1.0
 { {0,4,8,12,16,20,24,30,36,44,52,62,74, 90,110,134,162,196,238,288,342,418,576},
   {4,4,4,4,4,4,6,6,8, 8,10,12,16,20,24,28,34,42,50,54, 76,158},
   {0,4*3,8*3,12*3,16*3,22*3,30*3,40*3,52*3,66*3, 84*3,106*3,136*3,192*3},
   {4,4,4,4,6,8,10,12,14,18,22,30,56} } ,

 { {0,4,8,12,16,20,24,30,36,42,50,60,72, 88,106,128,156,190,230,276,330,384,576},
   {4,4,4,4,4,4,6,6,6, 8,10,12,16,18,22,28,34,40,46,54, 54,192},
   {0,4*3,8*3,12*3,16*3,22*3,28*3,38*3,50*3,64*3, 80*3,100*3,126*3,192*3},
   {4,4,4,4,6,6,10,12,14,16,20,26,66} } ,

 { {0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576} ,
   {4,4,4,4,4,4,6,6,8,10,12,16,20,24,30,38,46,56,68,84,102, 26} ,
   {0,4*3,8*3,12*3,16*3,22*3,30*3,42*3,58*3,78*3,104*3,138*3,180*3,192*3} ,
   {4,4,4,4,6,8,12,16,20,26,34,42,12} }  ,

// MPEG 2.0 
 { {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
   {6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54 } ,
   {0,4*3,8*3,12*3,18*3,24*3,32*3,42*3,56*3,74*3,100*3,132*3,174*3,192*3} ,
   {4,4,4,6,6,8,10,14,18,26,32,42,18 } } ,

/* mhipp trunk has 330 -> 332 without further explanation ... */
 { {0,6,12,18,24,30,36,44,54,66,80,96,114,136,162,194,232,278,330,394,464,540,576},
   {6,6,6,6,6,6,8,10,12,14,16,18,22,26,32,38,46,52,64,70,76,36 } ,
   {0,4*3,8*3,12*3,18*3,26*3,36*3,48*3,62*3,80*3,104*3,136*3,180*3,192*3} ,
   {4,4,4,6,8,10,12,14,18,24,32,44,12 } } ,

 { {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
   {6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54 },
   {0,4*3,8*3,12*3,18*3,26*3,36*3,48*3,62*3,80*3,104*3,134*3,174*3,192*3},
   {4,4,4,6,8,10,12,14,18,24,30,40,18 } } ,

// MPEG 2.5
 { {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576} ,
   {6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54},
   {0,12,24,36,54,78,108,144,186,240,312,402,522,576},
   {4,4,4,6,8,10,12,14,18,24,30,40,18} },
 { {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576} ,
   {6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54},
   {0,12,24,36,54,78,108,144,186,240,312,402,522,576},
   {4,4,4,6,8,10,12,14,18,24,30,40,18} },
 { {0,12,24,36,48,60,72,88,108,132,160,192,232,280,336,400,476,566,568,570,572,574,576},
   {12,12,12,12,12,12,16,20,24,28,32,40,48,56,64,76,90,2,2,2,2,2},
   {0, 24, 48, 72,108,156,216,288,372,480,486,492,498,576},
   {8,8,8,12,16,20,24,28,36,2,2,2,26} } ,
	};

real CJoshuaMP3::tan1_1[16],CJoshuaMP3::tan2_1[16],CJoshuaMP3::tan1_2[16],CJoshuaMP3::tan2_2[16];
real CJoshuaMP3::pow1_1[2][16],CJoshuaMP3::pow2_1[2][16],CJoshuaMP3::pow1_2[2][16],CJoshuaMP3::pow2_2[2][16];

/* 
 * init tables for layer-3 
 */
void CJoshuaMP3::InitLayer3(int downSampleSBlimit) {
  register int i,j,k,l;

  for(i=-256; i<118+4; i++)
#ifdef USE_MMX
    if(!param.down_sample)
      gainpow2[i+256] = 16384.0 * pow((double)2.0,-0.25 * (double)(i+210) );
    else
#endif
    gainpow2[i+256] = DOUBLE_TO_REAL(pow((double)2.0,-0.25 * (double)(i+210)));
//    gainpow2[i+256] = pow((double)2.0,-0.25 * (double) (i+210) );		// volume?
//    gainpow2[i+256] = pow((double)2.0,-0.25 * (double) (i+210) );

  for(i=0; i<8207; i++)
    ispow[i] = DOUBLE_TO_REAL(pow((double)i,(double)4.0/3.0));

  for(i=0; i<8; i++) {
    static const double Ci[8]={-0.6,-0.535,-0.33,-0.185,-0.095,-0.041,-0.0142,-0.0037};
    double sq=sqrt(1.0+Ci[i]*Ci[i]);

    aa_cs[i] = DOUBLE_TO_REAL(1.0/sq);
    aa_ca[i] = DOUBLE_TO_REAL(Ci[i]/sq);
		}

  for(i=0; i<18; i++) {
    win[0][i]    = win[1][i]    = DOUBLE_TO_REAL(0.5 * sin( M_PI / 72.0 * (double)(2*(i+0) +1) ) / cos ( M_PI * (double) (2*(i+0) +19) / 72.0 ));
    win[0][i+18] = win[3][i+18] = DOUBLE_TO_REAL(0.5 * sin( M_PI / 72.0 * (double)(2*(i+18)+1) ) / cos ( M_PI * (double) (2*(i+18)+19) / 72.0 ));
		}
  for(i=0; i<6; i++) {
    win[1][i+18] = DOUBLE_TO_REAL(0.5 / cos ( M_PI * (double)(2*(i+18)+19) / 72.0 ));
    win[3][i+12] = DOUBLE_TO_REAL(0.5 / cos ( M_PI * (double)(2*(i+12)+19) / 72.0 ));
    win[1][i+24] = DOUBLE_TO_REAL(0.5 * sin( M_PI / 24.0 * (double)(2*i+13) ) / cos ( M_PI * (double)(2*(i+24)+19) / 72.0 ));
    win[1][i+30] = win[3][i] = DOUBLE_TO_REAL(0.0);
    win[3][i+6 ] = DOUBLE_TO_REAL(0.5 * sin( M_PI / 24.0 * (double)(2*i+1) ) / cos ( M_PI * (double)(2*(i+6 )+19) / 72.0 ));
	  }

  for(i=0; i<9; i++)
    COS9[i] = DOUBLE_TO_REAL(cos( M_PI / 18.0 * (double)i));

  for(i=0; i<9; i++)
    tfcos36[i] = DOUBLE_TO_REAL(0.5 / cos ( M_PI * (double)(i*2+1) / 36.0 ));
  for(i=0; i<3; i++)
    tfcos12[i] = DOUBLE_TO_REAL(0.5 / cos ( M_PI * (double)(i*2+1) / 12.0 ));

  COS6_1 = DOUBLE_TO_REAL(cos( M_PI / 6.0 * (double)1));
  COS6_2 = DOUBLE_TO_REAL(cos( M_PI / 6.0 * (double)2));

#ifdef NEW_DCT9
  cos9[0]  = DOUBLE_TO_REAL(cos(1.0*M_PI/9.0));
  cos9[1]  = DOUBLE_TO_REAL(cos(5.0*M_PI/9.0));
  cos9[2]  = DOUBLE_TO_REAL(cos(7.0*M_PI/9.0));
  cos18[0] = DOUBLE_TO_REAL(cos(1.0*M_PI/18.0));
  cos18[1] = DOUBLE_TO_REAL(cos(11.0*M_PI/18.0));
  cos18[2] = DOUBLE_TO_REAL(cos(13.0*M_PI/18.0));
#endif

  for(i=0; i<12; i++) {
    win[2][i]  = DOUBLE_TO_REAL(0.5 * sin( M_PI / 24.0 * (double)(2*i+1) ) / cos ( M_PI * (double)(2*i+7) / 24.0 ));
    for(j=0; j<6; j++)
      COS1[i][j] = DOUBLE_TO_REAL(cos( M_PI / 24.0 * (double)((2*i+7)*(2*j+1)) ));
		}

  for(j=0; j<4; j++) {
    static const BYTE len[4] = { 36,36,12,36 };

    for(i=0; i<len[j]; i+=2)
      win1[j][i] = + win[j][i];
    for(i=1; i<len[j]; i+=2)
      win1[j][i] = - win[j][i];
		}

  for(i=0; i<16; i++) {
    double t = tan((double)i * M_PI / 12.0);

    tan1_1[i] = DOUBLE_TO_REAL(t / (1.0+t));
    tan2_1[i] = DOUBLE_TO_REAL(1.0 / (1.0 + t));
    tan1_2[i] = DOUBLE_TO_REAL(M_SQRT2 * t / (1.0+t));
    tan2_2[i] = DOUBLE_TO_REAL(M_SQRT2 / (1.0 + t));

    for(j=0; j<2; j++) {
      double base = pow(2.0,-0.25*(j+1.0));
      double p1=1.0,p2=1.0;

      if(i > 0) {
        if(i & 1)
          p1 = pow(base,(i+1.0)*0.5);
        else
          p2 = pow(base,i*0.5);
			  }
      pow1_1[j][i] = DOUBLE_TO_REAL(p1);
      pow2_1[j][i] = DOUBLE_TO_REAL(p2);
      pow1_2[j][i] = DOUBLE_TO_REAL(M_SQRT2 * p1);
      pow2_2[j][i] = DOUBLE_TO_REAL(M_SQRT2 * p2);
			}
		}

  for(j=0; j<9; j++) {
    struct BAND_INFO_STRUCT *bi = &bandInfo[j];
    int *mp;
    int cb,lwin;
    short int *bdf;

    mp = map[j][0] = mapbuf0[j];
    bdf = bi->longDiff;
    for(i=0,cb = 0; cb < 8; cb++,i+=*bdf++) {
      *mp++ = (*bdf) >> 1;
      *mp++ = i;
      *mp++ = 3;
      *mp++ = cb;
		  }
    bdf = bi->shortDiff+3;
    for(cb=3; cb<13; cb++) {
      int l = (*bdf++) >> 1;

      for(lwin=0; lwin<3; lwin++) {
        *mp++ = l;
        *mp++ = i + lwin;
        *mp++ = lwin;
        *mp++ = cb;
			  }
      i += 6*l;
		  }
    mapend[j][0] = mp;

    mp = map[j][1] = mapbuf1[j];
    bdf = bi->shortDiff+0;
    for(i=0,cb=0; cb<13; cb++) {
      int l = (*bdf++) >> 1;

      for(lwin=0; lwin<3; lwin++) {
        *mp++ = l;
        *mp++ = i + lwin;
        *mp++ = lwin;
        *mp++ = cb;
        }
      i += 6*l;
      }
    mapend[j][1] = mp;

    mp = map[j][2] = mapbuf2[j];
    bdf = bi->longDiff;
    for(cb=0; cb < 22; cb++) {
      *mp++ = (*bdf++) >> 1;
      *mp++ = cb;
	    }
    mapend[j][2] = mp;

		}

  for(j=0; j<9; j++) {
    for(i=0; i<23; i++) {
      longLimit[j][i] = (bandInfo[j].longIdx[i] - 1 + 8) / 18 + 1;
      if(longLimit[j][i] > downSampleSBlimit)
        longLimit[j][i] = downSampleSBlimit;
			}
    for(i=0; i<14; i++) {
      shortLimit[j][i] = (bandInfo[j].shortIdx[i] - 1) / 18 + 1;
      if(shortLimit[j][i] > downSampleSBlimit)
        shortLimit[j][i] = downSampleSBlimit;
			}
		}

  for(i=0; i<5; i++) {
    for(j=0; j<6; j++) {
      for(k=0; k<6; k++) {
        int n = k + j*6 + i*36;

        i_slen2[n] = i | (j<<3) | (k<<6) | (3<<12);
				}
			}
		}
  for(i=0; i<4; i++) {
    for(j=0; j<4; j++) {
      for(k=0; k<4; k++) {
        int n = k + j*4 + i*16;

        i_slen2[n+180] = i | (j<<3) | (k<<6) | (4<<12);
				}
			}
		}
  for(i=0; i<4; i++) {
    for(j=0; j<3; j++) {
      int n = j + i*3;

      i_slen2[n+244] = i | (j<<3) | (5<<12);
      n_slen2[n+500] = i | (j<<3) | (2<<12) | (1<<15);
			}
		}

  for(i=0; i<5; i++) {
    for(j=0; j<5; j++) {
      for(k=0; k<4; k++) {
        for(l=0; l<4; l++) {
          int n = l + k*4 + j*16 + i*80;

          n_slen2[n] = i | (j<<3) | (k<<6) | (l<<9) | (0<<12);
					}
				}
			}
		}
  for(i=0; i<5; i++) {
    for(j=0; j<5; j++) {
      for(k=0; k<4; k++) {
        int n = k + j*4 + i*20;

        n_slen2[n+400] = i | (j<<3) | (k<<6) | (1<<12);
				}
			}
		}

#ifdef GAPLESS
// non son sicuro che vada qua... v. src. dic. '06

/* input in bytes already */
void layer3_gapless_init(unsigned long b, unsigned long e) {

	bytified = 0;
	position = 0;
	ignore = 0;
	begin = b;
	end = e;
	debug_print(CLogFile::flagInfo,"mpg123:debug2: layer3_gapless_init: from %lu to %lu samples", begin, end);
	}

void layer3_gapless_set_position(unsigned long frames, struct frame* fr, struct audio_info_struct *ai) {

	position = samples_to_bytes(frames*spf(fr), fr, ai);
	debug_print(CLogFile::flagInfo,"set; position now %lu", position);
	}

void layer3_gapless_bytify(struct frame *fr, struct audio_info_struct *ai) {

	if(!bytified)	{
		begin = samples_to_bytes(begin, fr, ai);
		end = samples_to_bytes(end, fr, ai);
		bytified = 1;
		debug_print(CLogFile::flagInfo,"bytified: begin=%lu; end=%5lu", begin, end);
		}
	}

/* I need initialized fr here! */
void layer3_gapless_set_ignore(unsigned long frames, struct frame *fr, struct audio_info_struct *ai) {

	ignore = samples_to_bytes(frames*spf(fr), fr, ai);
	}

/*
	take the (partially or fully) filled and remove stuff for gapless mode if needed
	pcm_point may then be smaller than before...
*/
void layer3_gapless_buffercheck() {

	/* pcm_point bytes added since last position... */
	unsigned long new_pos = position + pcm_point;
	if(begin && (position < begin)) {
		debug_print(CLogFile::flagInfo,"new_pos %lu (old: %lu), begin %lu, pcm_point %i", new_pos, position, begin, pcm_point);
		if(new_pos < begin)	{
			if(ignore > pcm_point) 
				ignore -= pcm_point;
			else 
				ignore=0;
			pcm_point=0; /* full of padding/delay */
			}
		else {
			unsigned long ignored = begin-position;
			/* we need to shift the memory to the left... */
			debug_print(CLogFile::flagInfo,"old pcm_point: %i, begin %lu; good bytes: %i", pcm_point, begin, (int)(new_pos-begin));
			if(ignore > ignored) ignore -= ignored;
			else ignore = 0;
			pcm_point -= ignored;
			debug_print(CLogFile::flagInfo,"shifting %i bytes from %p to %p", pcm_point, pcm_sample+(int)(begin-position), pcm_sample);
			memmove(pcm_sample, pcm_sample+(int)(begin-position), pcm_point);
			}
		}
	/* I don't cover the case with both end and begin in chunk! */
	else if(end && (new_pos > end))	{
		ignore = 0;
		/* either end in current chunk or chunk totally out */
		debug_print(CLogFile::flagInfo,"ending at position %lu / point %i", new_pos, pcm_point);
		if(position < end)	
			pcm_point -= new_pos-end;
		else 
			pcm_point=0;
		debug_print(CLogFile::flagInfo,"set pcm_point to %i", pcm_point);
		}
	else if(ignore) {
		if(pcm_point < ignore) {
			ignore -= pcm_point;
			debug_print(CLogFile::flagInfo,"ignored %i bytes; pcm_point = 0; %lu bytes left", pcm_point, ignore);
			pcm_point=0;
			}
		else {
			/* we need to shift the memory to the left... */
			debug_print(CLogFile::flagInfo,"old pcm_point: %i, to ignore: %lu; good bytes: %i", pcm_point, ignore, pcm_point-(int)ignore);
			pcm_point -= ignore;
			debug_print(CLogFile::flagInfo,"shifting %i bytes from %p to %p", pcm_point, pcm_sample+ignore, pcm_sample);
			memmove(pcm_sample, pcm_sample+ignore, pcm_point);
			ignore=0;
			}
		}
	position = new_pos;
	}
#endif

	}


/*
 * read additional side information
 */
int CJoshuaMP3::III_getSideInfo1(struct III_SIDE_INFO *si,BYTE stereo,BYTE ms_stereo,long sfreq,short int single,BYTE lsf) {
  BYTE ch, gr;
  BYTE powdiff = (single == 3) ? 4 : 0;

  static const BYTE tabs[2][5] = { { 2,9,5,3,4 } , { 1,8,1,2,9 } };
  const BYTE *tab = tabs[lsf];

  si->mainDataBegin = getBits(tab[1]);
  if(stereo == 1)
    si->privateBits = getBitsFast(tab[2]);
  else 
    si->privateBits = getBitsFast(tab[3]);

  if(!lsf) {
    for(ch=0; ch<stereo; ch++) {
      si->ch[ch].gr[0].scfsi = -1;
      si->ch[ch].gr[1].scfsi = getBitsFast(4);
      }
    }

  for(gr=0; gr<tab[0]; gr++) {
    for(ch=0; ch<stereo; ch++) {
      register struct GR_INFO_S *gr_info = &(si->ch[ch].gr[gr]);

      gr_info->part2_3_length = getBits(12);
      gr_info->big_values = getBits(9);
      if(gr_info->big_values > 288) {
        debug_print(CLogFile::flagError2,"big_values too large!");
        gr_info->big_values = 288;
	      }
      gr_info->pow2gain = gainpow2+256 - getBitsFast(8) + powdiff;
      if(ms_stereo)
        gr_info->pow2gain += 2;
      gr_info->scalefacCompress = getBitsFast(4);
// window-switching flag == 1 for block_Type != 0 .. and block-type == 0 -> win-sw-flag = 0 
      if(get1Bit()) {
        int i;

        gr_info->blockType = getBitsFast(2);
        gr_info->mixedBlockFlag = get1Bit();
        gr_info->tableSelect[0] = getBitsFast(5);
        gr_info->tableSelect[1] = getBitsFast(5);
         /*
          * table_select[2] not needed, because there is no region2,
          * but to satisfy some verifications tools we set it either.
          */
        gr_info->tableSelect[2] = 0;
        for(i=0; i<3; i++)
          gr_info->fullGain[i] = gr_info->pow2gain + (getBitsFast(3) << 3);

        if(gr_info->blockType == 0) {
          debug_print(CLogFile::flagError2,"Blocktype == 0 and window-switching == 1 not allowed.");
//          exit(1);
          }
         // region_count/start parameters are implicit in this case. 
        if(!lsf || gr_info->blockType == 2)
          gr_info->region1start = 36>>1;
        else {
/* check this again for 2.5 and sfreq=8 */
          if(sfreq == 8)
            gr_info->region1start = 108 >> 1;
          else
            gr_info->region1start = 54 >> 1;
					}
        gr_info->region2start = 576 >> 1;
        }
      else {
        int i,r0c,r1c;

        for(i=0; i<3; i++)
          gr_info->tableSelect[i] = getBitsFast(5);
        r0c = getBitsFast(4);
        r1c = getBitsFast(3);
        gr_info->region1start = bandInfo[sfreq].longIdx[r0c+1] >> 1 ;
        gr_info->region2start = bandInfo[sfreq].longIdx[r0c+1+r1c+1] >> 1;
        gr_info->blockType = 0;
        gr_info->mixedBlockFlag = 0;
        }
      if(!lsf)
	      gr_info->preflag = get1Bit();
      gr_info->scalefacScale = get1Bit();
      gr_info->count1tableSelect = get1Bit();
      }
    }

  return 0;
	}

#if 0
/*
 * Side Info for MPEG 2.0 / LSF
 */
void CJoshuaMP3::III_getSideInfo2(struct III_SIDE_INFO *si,BYTE stereo,BYTE ms_stereo,long sfreq,short int single) {
  BYTE ch;
  BYTE powdiff = (single == 3) ? 4 : 0;

  si->mainDataBegin = getBits(8);
  if(stereo == 1)
    si->privateBits = get1Bit();
  else 
    si->privateBits = getBitsFast(2);

  for(ch=0; ch<stereo; ch++) {
    register struct GR_INFO_S *gr_info = &(si->ch[ch].gr[0]);

    gr_info->part2_3_length = getBits(12);
    gr_info->big_values = getBitsFast(9);
    gr_info->pow2gain = gainpow2+256 - getBitsFast(8) + powdiff;
    if(ms_stereo)
      gr_info->pow2gain += 2;
    gr_info->scalefacCompress = getBits(9);
// window-switching flag == 1 for block_Type != 0 .. and block-type == 0 -> win-sw-flag = 0 
    if(get1Bit()) {
      int i;

			gr_info->blockType = getBitsFast(2);
			gr_info->mixedBlockFlag = get1Bit();
			gr_info->tableSelect[0] = getBitsFast(5);
			gr_info->tableSelect[1] = getBitsFast(5);
         /*
          * table_select[2] not needed, because there is no region2,
          * but to satisfy some verifications tools we set it either.
          */
      gr_info->tableSelect[2] = 0;
      for(i=0; i<3; i++)
        gr_info->fullGain[i] = gr_info->pow2gain + (getBitsFast(3)<<3);

      if(gr_info->blockType == 0) {
        debug_print(CLogFile::flagError2,"Blocktype == 0 and window-switching == 1 not allowed.");
//        exit(1);
        }
// region_count/start parameters are implicit in this case.
// check this again! 
      if(gr_info->blockType == 2)
        gr_info->region1start = 36 >> 1;
      else {
        gr_info->region1start = 54 >> 1;
        }
      gr_info->region2start = 576 >> 1;
      }
    else {
      int i,r0c,r1c;

      for(i=0; i<3; i++)
        gr_info->tableSelect[i] = getBitsFast(5);
      r0c = getBitsFast(4);
      r1c = getBitsFast(3);
      gr_info->region1start = bandInfo[sfreq].longIdx[r0c+1] >> 1 ;
      gr_info->region2start = bandInfo[sfreq].longIdx[r0c+1+r1c+1] >> 1;
      gr_info->blockType = 0;
      gr_info->mixedBlockFlag = 0;
      }
    gr_info->scalefacScale = get1Bit();
    gr_info->count1tableSelect = get1Bit();
		}
	}
#endif

/*
 * read scalefactors
 */
int CJoshuaMP3::III_getScaleFactors1(int *scf,struct GR_INFO_S *gr_info,BYTE ch,BYTE gr) {
  static const BYTE slen[2][16] = {
    {0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4},
    {0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3}
		};
  int numbits;
  int num0 = slen[0][gr_info->scalefacCompress];
  int num1 = slen[1][gr_info->scalefacCompress];

  if(gr_info->blockType == 2) {
    int i=18;
    numbits = (num0 + num1) * 18;

    if(gr_info->mixedBlockFlag) {
      for(i=8; i; i--)
        *scf++ = getBitsFast(num0);
      i=9;
      numbits -= num0;    // num0 * 17 + num1 * 18 
			}

    for(; i; i--)
      *scf++ = getBitsFast(num0);
    for(i=18; i; i--)
      *scf++ = getBitsFast(num1);
    *scf++ = 0; *scf++ = 0; *scf++ = 0;     // short[13][0..2] = 0 
		}
  else {
    int i;
    int scfsi = gr_info->scfsi;

    if(scfsi < 0) {     // scfsi < 0 => granule == 0 
      for(i=11; i; i--)
        *scf++ = getBitsFast(num0);
      for(i=10; i; i--)
        *scf++ = getBitsFast(num1);
      numbits = (num0 + num1) * 10 + num0;
      *scf++ = 0;
			}
    else {
      numbits = 0;
      if(!(scfsi & 0x8)) {
        for(i=0; i<6; i++)
          *scf++ = getBitsFast(num0);
        numbits += num0 * 6;
				}
      else
        scf += 6; 

      if(!(scfsi & 0x4)) {
        for(i=0; i<5; i++)
          *scf++ = getBitsFast(num0);
        numbits += num0 * 5;
				}
      else
        scf += 5;

      if(!(scfsi & 0x2)) {
        for(i=0; i<5; i++)
          *scf++ = getBitsFast(num1);
        numbits += num1 * 5;
				}
      else
        scf += 5;

      if(!(scfsi & 0x1)) {
        for(i=0; i<5; i++)
          *scf++ = getBitsFast(num1);
        numbits += num1 * 5;
				}
      else
        scf += 5;

	    *scf++ = 0;  // no l[21] in original sources 
      }
		}
  return numbits;
	}

int CJoshuaMP3::III_getScaleFactors2(int *scf,struct GR_INFO_S *gr_info,BYTE i_stereo) {
  const BYTE *pnt;
  int i,j,n=0,numbits=0;
  unsigned int slen;

  static const BYTE stab[3][6][4] = {
   { { 6, 5, 5,5 } , { 6, 5, 7,3 } , { 11,10,0,0} ,
     { 7, 7, 7,0 } , { 6, 6, 6,3 } , {  8, 8,5,0} } ,
   { { 9, 9, 9,9 } , { 9, 9,12,6 } , { 18,18,0,0} ,
     {12,12,12,0 } , {12, 9, 9,6 } , { 15,12,9,0} } ,
   { { 6, 9, 9,9 } , { 6, 9,12,6 } , { 15,18,0,0} ,
     { 6,15,12,0 } , { 6,12, 9,6 } , {  6,18,9,0} } }; 

  if(i_stereo)		// i_stereo AND second channel -> do_layer3() checks this 
    slen = i_slen2[gr_info->scalefacCompress >> 1];
  else
    slen = n_slen2[gr_info->scalefacCompress];

  gr_info->preflag = (slen >> 15) & 0x1;

  n = 0;  
  if(gr_info->blockType == 2) {
    n++;
    if(gr_info->mixedBlockFlag)
      n++;
		}

  pnt = stab[n][(slen >> 12) & 0x7];

  for(i=0; i<4; i++) {
    int num = slen & 0x7;

    slen >>= 3;
    if(num) {
      for(j=0; j<(int)(pnt[i]); j++)
        *scf++ = getBitsFast(num);
      numbits += pnt[i] * num;
	    }
    else {
      for(j=0; j<(int)(pnt[i]); j++)
				*scf++ = 0;
			}
		}
  
  n = (n << 1) + 1;
  for(i=0; i<n; i++)
    *scf++ = 0;

  return numbits;
	}


BYTE CJoshuaMP3::pretab1[22] = {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,3,2,0};
BYTE CJoshuaMP3::pretab2[22] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/*
 * Dequantize samples (includes huffman decoding)
 */
/* 24 is enough because tab13 has max. a 19 bit huffvector */
#define BITSHIFT ((sizeof(long)-1)*8)
#define REFRESH_MASK \
  while(num < BITSHIFT) { \
    mask |= ((unsigned long)getByte()) << (BITSHIFT-num); \
    num += 8; \
    part2remain -= 8; }

int CJoshuaMP3::III_dequantizeSample(real xr[SBLIMIT][SSLIMIT],int *scf,struct GR_INFO_S *gr_info,int sfreq,int part2bits) {
  int shift = 1 + gr_info->scalefacScale;
  real *xrpnt = (real *)xr;
  int l[3],l3;
  int part2remain = gr_info->part2_3_length - part2bits;
  int *me;

  /* mhipp tree has this split up a bit... */
  int num=getBitOffset();
  long mask = (long) getBits(num) << (BITSHIFT+8-num);
  part2remain -= num;
  {
    int bv       = gr_info->big_values;
    int region1  = gr_info->region1start;
    int region2  = gr_info->region2start;

		if(region1 > region2)	{
			debug_print(CLogFile::flagError2,"mpg123: You got some really nasty file there... region1>region2!");
			return 1;
			}
    l3 = ((576 /* SBLIMIT * SSLIMIT */  >> 1)-bv) >> 1;   
/*
 * we may lose the 'odd' bit here !! 
 * check this later again 
 */
    if(bv <= region1) {
      l[0] = bv; l[1] = 0; l[2] = 0;
			}
    else {
      l[0] = region1;
      if(bv <= region2) {
        l[1] = bv - l[0];  l[2] = 0;
				}
      else {
        l[1] = region2 - l[0]; l[2] = bv - region2;
				}
			}
		}
 
  if(gr_info->blockType == 2) {
     // decoding with short or mixed mode BandIndex table 
    int i,max[4];
    int step=0,lwin=3,cb=0;
    register real v = 0.0;
    register int *m,mc;

    if(gr_info->mixedBlockFlag) {
      max[3] = -1;
      max[0] = max[1] = max[2] = 2;
      m = map[sfreq][0];
      me = mapend[sfreq][0];
			}
    else {
      max[0] = max[1] = max[2] = max[3] = -1;
      // max[3] not really needed in this case 
      m = map[sfreq][1];
      me = mapend[sfreq][1];
			}

    mc = 0;
    for(i=0; i<2; i++) {
      int lp = l[i];
      struct NEW_HUFF *h = ht+gr_info->tableSelect[i];

      for(; lp; lp--,mc--) {
        register int x,y;

        if(!mc) {
          mc = *m++;
          xrpnt = ((real *)xr) + (*m++);
          lwin = *m++;
          cb = *m++;
          if(lwin == 3) {
            v = gr_info->pow2gain[(*scf++) << shift];
            step = 1;
				    }
          else {
            v = gr_info->fullGain[lwin][(*scf++) << shift];
            step = 3;
					  }
					}
        {
          register short *val = h->table;
          REFRESH_MASK;

          while((y=*val++) < 0) {
            if(mask < 0)
              val -= y;
            num--;
            mask <<= 1;
						}
          x = y >> 4;
          y &= 0xf;
					}
        if(x == 15 && h->linbits) {
          max[lwin] = cb;
          REFRESH_MASK;
          x += ((DWORD) mask) >> (BITSHIFT+8-h->linbits);
          num -= h->linbits+1;
          mask <<= h->linbits;
          if(mask < 0)
            *xrpnt = REAL_MUL(-ispow[x], v);
          else
            *xrpnt = REAL_MUL(ispow[x], v);
          mask <<= 1;
			    }
        else if(x) {
          max[lwin] = cb;
          if(mask < 0)
            *xrpnt = REAL_MUL(-ispow[x], v);
          else
            *xrpnt = REAL_MUL(ispow[x], v);
          num--;
          mask <<= 1;
					}
        else
          *xrpnt = DOUBLE_TO_REAL(0.0);
        xrpnt += step;
        if(y == 15 && h->linbits) {
          max[lwin] = cb;
          REFRESH_MASK;
          y += ((DWORD) mask) >> (BITSHIFT+8-h->linbits);
          num -= h->linbits+1;
          mask <<= h->linbits;
          if(mask < 0)
            *xrpnt = REAL_MUL(-ispow[y], v);
          else
            *xrpnt = REAL_MUL(ispow[y], v);
          mask <<= 1;
		      }
        else if(y) {
          max[lwin] = cb;
          if(mask < 0)
            *xrpnt = REAL_MUL(-ispow[y], v);
          else
            *xrpnt = REAL_MUL(ispow[y], v);
          num--;
          mask <<= 1;
					}
        else
          *xrpnt = DOUBLE_TO_REAL(0.0);
        xrpnt += step;
				}
			}
    for(; l3 && (part2remain+num > 0); l3--) {
      /* not mixing code and declarations to keep C89 happy */
      struct NEW_HUFF* h;
      register short* val;
			register short a;
      /* This is only a humble hack to prevent a special segfault. */
      /* More insight into the real workings is still needed. */
      /* especially why there are (valid?) files that make xrpnt exceed the array with 4 bytes without segfaulting, more seems to be really bad, though. */
      #ifdef DEBUG
      if(!(xrpnt < &xr[SBLIMIT][0])) {
        if(Param.verbose) 
					debug_print(CLogFile::flagWarning,"mpg123:attempted soft xrpnt overflow (%p !< %p) ?", (void*) xrpnt, (void*) &xr[SBLIMIT][0]);
				}
      #endif
      if(!(xrpnt < &xr[SBLIMIT][0]+5)) {
        if(Param.verbose) 
	        debug_print(CLogFile::flagError2,"mpg123: attempted xrpnt overflow (%p !< %p)", (void*) xrpnt, (void*) &xr[SBLIMIT][0]);
        return 2;
				}
      h = htc+gr_info->count1tableSelect;
      val = h->table;

      REFRESH_MASK;
      while((a=*val++)<0) {
        if(mask < 0)
          val -= a;
        num--;
        mask <<= 1;
				}
      if(part2remain+num <= 0) {
				num -= part2remain+num;
				break;
				}

      for(i=0; i<4; i++) {
        if(!(i & 1)) {
          if(!mc) {
            mc = *m++;
            xrpnt = ((real *)xr) + (*m++);
            lwin = *m++;
            cb = *m++;
            if(lwin == 3) {
              v = gr_info->pow2gain[(*scf++) << shift];
              step = 1;
					    }
            else {
              v = gr_info->fullGain[lwin][(*scf++) << shift];
              step = 3;
						  }
						}
          mc--;
					}
        if((a & (0x8 >> i))) {
          max[lwin] = cb;
          if(part2remain+num <= 0) {
            break;
			      }
          if(mask < 0) 
            *xrpnt = -v;
          else
            *xrpnt = v;
          num--;
          mask <<= 1;
					}
        else
          *xrpnt = 0.0;
        xrpnt += step;
				}
			}
 
    if(lwin < 3) { /* short band? */
      while(1) {
        for(;mc > 0;mc--) {
          *xrpnt = DOUBLE_TO_REAL(0.0); xrpnt += 3; /* short band -> step=3 */
          *xrpnt = DOUBLE_TO_REAL(0.0); xrpnt += 3;
			    }
        if(m >= me)
          break;
        mc    = *m++;
        xrpnt = ((real *) xr) + *m++;
        if(*m++ == 0)
          break; // optimize: field will be set to zero at the end of the function 
        m++; // cb
		    }
	    }

    gr_info->maxband[0] = max[0]+1;
    gr_info->maxband[1] = max[1]+1;
    gr_info->maxband[2] = max[2]+1;
    gr_info->maxbandl = max[3]+1;

    {
      int rmax = max[0] > max[1] ? max[0] : max[1];
      rmax = (rmax > max[2] ? rmax : max[2]) + 1;
      gr_info->maxb = rmax ? shortLimit[sfreq][rmax] : longLimit[sfreq][max[3]+1];
    }

		}
  else {
     // decoding with 'long' BandIndex table (block_type != 2)
    BYTE *pretab = gr_info->preflag ? pretab1 : pretab2;
    int i,max = -1;
    int cb=0;
    int *m = map[sfreq][2];
    register real v = 0.0;
    int mc=0;

    /*
     * long hash table values
     */
    for(i=0; i<3; i++) {
      int lp = l[i];
      struct NEW_HUFF *h = ht+gr_info->tableSelect[i];

      for(; lp; lp--,mc--) {
        register int x,y;

        if(!mc) {
          mc = *m++;
          cb = *m++;
          if(cb == 21)
            v = 0.0;
          else
            v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];

	        }
				{
        register short *val = h->table;
        REFRESH_MASK;
        while((y=*val++)<0) {
          if (mask < 0)
            val -= y;
          num--;
          mask <<= 1;
					}
        x = y >> 4;
        y &= 0xf;
				}
        if(x == 15 && h->linbits) {
          max = cb;
					REFRESH_MASK;
          x += ((DWORD) mask) >> (BITSHIFT+8-h->linbits);
          num -= h->linbits+1;
          mask <<= h->linbits;
          if(mask < 0)
            *xrpnt++ = REAL_MUL(-ispow[x], v);
          else
            *xrpnt++ = REAL_MUL(ispow[x], v);
          mask <<= 1;
					}
        else if(x) {
          max = cb;
          if(mask < 0)
            *xrpnt++ = REAL_MUL(-ispow[x], v);
          else
            *xrpnt++ = REAL_MUL(ispow[x], v);
          num--;
          mask <<= 1;
					}
        else
          *xrpnt++ = DOUBLE_TO_REAL(0.0);

        if(y == 15 && h->linbits) {
          max = cb;
				  REFRESH_MASK;
          y += ((DWORD) mask) >> (BITSHIFT+8-h->linbits);
          num -= h->linbits+1;
          mask <<= h->linbits;
          if(mask < 0)
            *xrpnt++ = REAL_MUL(-ispow[y], v);
          else
            *xrpnt++ = REAL_MUL(ispow[y], v);
          mask <<= 1;
			    }
        else if(y) {
          max = cb;
          if(mask < 0)
            *xrpnt++ = REAL_MUL(-ispow[y], v);
          else
            *xrpnt++ = REAL_MUL(ispow[y], v);
          num--;
          mask <<= 1;
		      }
        else
          *xrpnt++ = DOUBLE_TO_REAL(0.0);
				}
			}

    /*
     * short (count1table) values
     */
    for(; l3 && (part2remain+num > 0); l3--) {
      struct NEW_HUFF *h = htc+gr_info->count1tableSelect;
      register short *val = h->table,a;

      REFRESH_MASK;
      while((a=*val++) < 0) {
        if(mask < 0)
          val -= a;
        num--;
        mask <<= 1;
				}
      if(part2remain+num <= 0) {
				num -= part2remain+num;
        break;
				}

      for(i=0; i<4; i++) {
        if(!(i & 1)) {
          if(!mc) {
            mc = *m++;
            cb = *m++;
            if(cb == 21)
              v = 0.0;
            else
              v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];
	          }
          mc--;
					}
        if((a & (0x8 >> i))) {
          max = cb;
          if(part2remain+num <= 0) {
            break;
	          }
          if(mask < 0)
            *xrpnt++ = -v;
          else
            *xrpnt++ = v;
          num--;
          mask <<= 1;
					}
        else
          *xrpnt++ = DOUBLE_TO_REAL(0.0);
				}
			}

    gr_info->maxbandl = max+1;
    gr_info->maxb = longLimit[sfreq][gr_info->maxbandl];
		}

	part2remain += num;
	backBits(num);
	num = 0;

  while(xrpnt < &xr[SBLIMIT][0]) 
    *xrpnt++ = DOUBLE_TO_REAL(0.0);

  while(part2remain > 16) {
    getBits(16); // Dismiss stuffing Bits 
    part2remain -= 16;
	  }
  if(part2remain > 0)
    getBits(part2remain);
  else if(part2remain < 0) {
    debug_print(CLogFile::flagError2,"mpg123: Can't rewind stream by %d bits!",-part2remain);
    return 1; //   -> error 
	  }
  return 0;
	}

#if 0
int CJoshuaMP3::III_dequantizeSampleMs(real xr[2][SBLIMIT][SSLIMIT],int *scf,struct GR_INFO_S *gr_info,int sfreq,int part2bits) {
  int shift = 1 + gr_info->scalefacScale;
  real *xrpnt = (real *)xr[1];
  real *xr0pnt = (real *)xr[0];
  int l[3],l3;
  int part2remain = gr_info->part2_3_length - part2bits;
  int *me;

  {
    int bv       = gr_info->big_values;
    int region1  = gr_info->region1start;
    int region2  = gr_info->region2start;

    l3 = ((576 >> 1)-bv) >> 1;
/*
 * we may lose the 'odd' bit here !! 
 * check this later gain 
 */
    if(bv <= region1) {
      l[0] = bv; l[1] = 0; l[2] = 0;
			}
    else {
      l[0] = region1;
      if(bv <= region2) {
        l[1] = bv - l[0];  l[2] = 0;
				}
      else {
        l[1] = region2 - l[0]; l[2] = bv - region2;
				}
			}
		}
 
  if(gr_info->blockType == 2) {
    int i,max[4];
    int step=0,lwin=0,cb=0;
    register real v = 0.0;
    register int *m,mc = 0;

    if(gr_info->mixedBlockFlag) {
      max[3] = -1;
      max[0] = max[1] = max[2] = 2;
      m = map[sfreq][0];
      me = mapend[sfreq][0];
			}
    else {
      max[0] = max[1] = max[2] = max[3] = -1;
      // max[3] not really needed in this case 
      m = map[sfreq][1];
      me = mapend[sfreq][1];
			}

    for(i=0; i<2; i++) {
      int lp = l[i];
      struct NEW_HUFF *h = ht+gr_info->tableSelect[i];

      for(; lp; lp--,mc--) {
        int x,y;

        if(!mc) {
          mc = *m++;
          xrpnt = ((real *)xr[1]) + *m;
          xr0pnt = ((real *)xr[0]) + *m++;
          lwin = *m++;
          cb = *m++;
          if(lwin == 3) {
            v = gr_info->pow2gain[(*scf++) << shift];
            step = 1;
						}
          else {
            v = gr_info->fullGain[lwin][(*scf++) << shift];
            step = 3;
						}
					}
        {
          register short *val = h->table;

          while((y=*val++) < 0) {
            if(get1Bit())
              val -= y;
            part2remain--;
						}
          x = y >> 4;
          y &= 0xf;
        }
        if(x == 15) {
          max[lwin] = cb;
          part2remain -= h->linbits+1;
          x += getBits(h->linbits);
          if(get1Bit()) {
            real a = ispow[x] * v;

            *xrpnt = *xr0pnt + a;
            *xr0pnt -= a;
						}
          else {
            real a = ispow[x] * v;

            *xrpnt = *xr0pnt - a;
            *xr0pnt += a;
						}
					}
        else if(x) {
          max[lwin] = cb;
          if(get1Bit()) {
            real a = ispow[x] * v;

            *xrpnt = *xr0pnt + a;
            *xr0pnt -= a;
						}
          else {
            real a = ispow[x] * v;

            *xrpnt = *xr0pnt - a;
            *xr0pnt += a;
						}
          part2remain--;
					}
        else
          *xrpnt = *xr0pnt;
        xrpnt += step;
        xr0pnt += step;

        if(y == 15) {
          max[lwin] = cb;
          part2remain -= h->linbits+1;
          y += getBits(h->linbits);
          if(get1Bit()) {
            real a = ispow[y] * v;

            *xrpnt = *xr0pnt + a;
            *xr0pnt -= a;
						}
          else {
            real a = ispow[y] * v;

            *xrpnt = *xr0pnt - a;
            *xr0pnt += a;
						}
					}
        else if(y) {
          max[lwin] = cb;
          if(get1Bit()) {
            real a = ispow[y] * v;

            *xrpnt = *xr0pnt + a;
            *xr0pnt -= a;
						}
          else {
            real a = ispow[y] * v;

            *xrpnt = *xr0pnt - a;
            *xr0pnt += a;
						}
          part2remain--;
					}
        else
          *xrpnt = *xr0pnt;
        xrpnt += step;
        xr0pnt += step;
				}
			}

    for(; l3 && (part2remain > 0); l3--) {
      struct NEW_HUFF *h = htc+gr_info->count1tableSelect;
      register short *val = h->table,a;

      while((a=*val++)<0) {
        part2remain--;
        if(part2remain < 0) {
          part2remain++;
          a = 0;
          break;
					}
        if(get1Bit())
          val -= a;
				}

      for(i=0; i<4; i++) {
        if(!(i & 1)) {
          if(!mc) {
            mc = *m++;
            xrpnt = ((real *)xr[1]) + *m;
            xr0pnt = ((real *)xr[0]) + *m++;
            lwin = *m++;
            cb = *m++;
            if(lwin == 3) {
              v = gr_info->pow2gain[(*scf++) << shift];
              step = 1;
							}
            else {
              v = gr_info->fullGain[lwin][(*scf++) << shift];
              step = 3;
							}
						}
          mc--;
					}
        if((a & (0x8 >> i))) {
          max[lwin] = cb;
          part2remain--;
          if(part2remain < 0) {
            part2remain++;
            break;
						}
          if(get1Bit()) {
            *xrpnt = *xr0pnt + v;
            *xr0pnt -= v;
						}
          else {
            *xrpnt = *xr0pnt - v;
            *xr0pnt += v;
						}
					}
        else
          *xrpnt = *xr0pnt;
        xrpnt += step;
        xr0pnt += step;
				}
			}
 
    while(m < me) {
      if(!mc) {
        mc = *m++;
        xrpnt = ((real *)xr) + *m;
        xr0pnt = ((real *)xr) + *m++;
        if(*m++ == 3)
          step = 1;
        else
          step = 3;
        m++;		// cb 
				}
      mc--;
      *xrpnt = *xr0pnt;
      xrpnt += step;
      xr0pnt += step;
      *xrpnt = *xr0pnt;
      xrpnt += step;
      xr0pnt += step;
			}

    gr_info->maxband[0] = max[0]+1;
    gr_info->maxband[1] = max[1]+1;
    gr_info->maxband[2] = max[2]+1;
    gr_info->maxbandl = max[3]+1;

    {
      int rmax = max[0] > max[1] ? max[0] : max[1];

      rmax = (rmax > max[2] ? rmax : max[2]) + 1;
      gr_info->maxb = rmax ? shortLimit[sfreq][rmax] : longLimit[sfreq][max[3]+1];
    }
  }
  else {
    int *pretab = gr_info->preflag ? pretab1 : pretab2;
    int i,max = -1;
    int cb = 0;
    register int mc=0,*m = map[sfreq][2];
    register real v = 0.0;

    me = mapend[sfreq][2];

    for(i=0; i<3; i++) {
      int lp = l[i];
      struct NEW_HUFF *h = ht+gr_info->tableSelect[i];

      for(; lp; lp--,mc--) {
        int x,y;
        if(!mc) {
          mc = *m++;
          cb = *m++;
          v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];
	        }
        {
          register short *val = h->table;

          while((y=*val++) < 0) {
            if(get1Bit())
              val -= y;
            part2remain--;
						}
          x = y >> 4;
          y &= 0xf;
        }
        if(x == 15) {
          max = cb;
          part2remain -= h->linbits+1;
          x += getBits(h->linbits);
          if(get1Bit()) {
            real a = ispow[x] * v;

            *xrpnt++ = *xr0pnt + a;
            *xr0pnt++ -= a;
						}
          else {
            real a = ispow[x] * v;

            *xrpnt++ = *xr0pnt - a;
            *xr0pnt++ += a;
						}
					}
        else if(x) {
          max = cb;
          if(get1Bit()) {
            real a = ispow[x] * v;

            *xrpnt++ = *xr0pnt + a;
            *xr0pnt++ -= a;
						}
          else {
            real a = ispow[x] * v;

            *xrpnt++ = *xr0pnt - a;
            *xr0pnt++ += a;
						}
          part2remain--;
					}
        else
          *xrpnt++ = *xr0pnt++;

        if(y == 15) {
          max = cb;
          part2remain -= h->linbits+1;
          y += getBits(h->linbits);
          if(get1Bit()) {
            real a = ispow[y] * v;

            *xrpnt++ = *xr0pnt + a;
            *xr0pnt++ -= a;
						}
					else {
            real a = ispow[y] * v;

            *xrpnt++ = *xr0pnt - a;
            *xr0pnt++ += a;
						}
					}
        else if(y) {
          max = cb;
          if(get1Bit()) {
            real a = ispow[y] * v;

            *xrpnt++ = *xr0pnt + a;
            *xr0pnt++ -= a;
						}
          else {
            real a = ispow[y] * v;

            *xrpnt++ = *xr0pnt - a;
            *xr0pnt++ += a;
						}
          part2remain--;
					}
        else
          *xrpnt++ = *xr0pnt++;
				}
			}

    for(; l3 && (part2remain > 0); l3--) {
      struct NEW_HUFF *h = htc+gr_info->count1tableSelect;
      register short *val = h->table,a;

      while((a=*val++)<0) {
        part2remain--;
        if(part2remain < 0) {
          part2remain++;
          a = 0;
          break;
					}
        if(get1Bit())
          val -= a;
				}

      for(i=0; i<4; i++) {
        if(!(i & 1)) {
          if(!mc) {
            mc = *m++;
            cb = *m++;
            v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];
					  }
          mc--;
					}
        if((a & (0x8 >> i))) {
          max = cb;
          part2remain--;
          if(part2remain <= 0) {
            part2remain++;
            break;
						}
          if(get1Bit()) {
            *xrpnt++ = *xr0pnt + v;
            *xr0pnt++ -= v;
						}
          else {
            *xrpnt++ = *xr0pnt - v;
            *xr0pnt++ += v;
						}
					}
        else
          *xrpnt++ = *xr0pnt++;
				}
			}
    for(i=(&xr[1][SBLIMIT][SSLIMIT]-xrpnt-SSLIMIT) >> 1; i; i--) {		// patch Dario!
      *xrpnt++ = *xr0pnt++;
      *xrpnt++ = *xr0pnt++;
			}

    gr_info->maxbandl = max+1;
    gr_info->maxb = longLimit[sfreq][gr_info->maxbandl];
		}

  while(part2remain > 16) {
    getBits(16);		// Dismiss stuffing Bits 
    part2remain -= 16;
		}
  if(part2remain > 0 )
    getBits(part2remain);
  else if(part2remain < 0) {
    if(Param.verbose)
			wsprintf(myBuf,"ms Can't rewind stream by %d bits!",-part2remain);
#ifdef _DEBUG
	    AfxMessageBox(myBuf);
#else
			debug_print(CLogFile::flagError,myBuf);
#endif
    return 1;		// -> error 
		}

  return 0;
	}
#endif	


/* 
 * III_stereo: calculate real channel values for Joint-I-Stereo-mode
 */
void CJoshuaMP3::III_IStereo(real xr_buf[2][SBLIMIT][SSLIMIT],int *scalefac,struct GR_INFO_S *gr_info,int sfreq,int ms_stereo,int lsf) {
  real (*xr)[SBLIMIT*SSLIMIT] = (real (*)[SBLIMIT*SSLIMIT]) xr_buf;
  struct BAND_INFO_STRUCT *bi = &bandInfo[sfreq];
  const real *tab1,*tab2;

#if 1
  int tab;
// TODO: optimize as static, fatto
  static const real *tabs[3][2][2] = { 
     { { tan1_1,tan2_1 }     , { tan1_2,tan2_2 } },
     { { pow1_1[0],pow2_1[0] } , { pow1_2[0],pow2_2[0] } } ,
     { { pow1_1[1],pow2_1[1] } , { pow1_2[1],pow2_2[1] } } 
	  };

  tab = lsf + (gr_info->scalefacCompress & lsf);
  tab1 = tabs[tab][ms_stereo][0];
  tab2 = tabs[tab][ms_stereo][1];
#else
  if(lsf) {
    int p = gr_info->scalefacCompress & 0x1;

    if(ms_stereo) {
      tab1 = pow1_2[p]; tab2 = pow2_2[p];
			}
    else {
      tab1 = pow1_1[p]; tab2 = pow2_1[p];
			}
		}
  else {
    if(ms_stereo) {
      tab1 = tan1_2; tab2 = tan2_2;
			}
    else {
      tab1 = tan1_1; tab2 = tan2_1;
			}
		}
#endif

  if(gr_info->blockType == 2) {
    int lwin,do_l = 0;

    if(gr_info->mixedBlockFlag)
       do_l = 1;

    for(lwin=0; lwin<3; lwin++) {   // process each window 
        // get first band with zero values 
      int is_p,sb,idx,sfb = gr_info->maxband[lwin];    // sfb is minimal 3 for mixed mode 

      if(sfb > 3)
        do_l = 0;

      for(; sfb<12; sfb++) {
        is_p = scalefac[sfb*3+lwin-gr_info->mixedBlockFlag];    // scale: 0-15 
        if(is_p != 7) {
          real t1,t2;

          sb = bi->shortDiff[sfb];
          idx = bi->shortIdx[sfb] + lwin;
          t1 = tab1[is_p]; t2 = tab2[is_p];
          for(; sb > 0; sb--,idx+=3) {
            real v = xr[0][idx];

            xr[0][idx] = REAL_MUL(v, t1);
            xr[1][idx] = REAL_MUL(v, t2);
            }
          }
        }

#if 1
/* in the original: copy 10 to 11 , here: copy 11 to 12 
maybe still wrong??? (copy 12 to 13?) */
			is_p = scalefac[11*3+lwin-gr_info->mixedBlockFlag]; // scale: 0-15 
			sb = bi->shortDiff[12];
			idx = bi->shortIdx[12] + lwin;
#else
			is_p = scalefac[10*3+lwin-gr_info->mixedBlockFlag];			// scale: 0-15 
			sb = bi->shortDiff[11];
			idx = bi->shortIdx[11] + lwin;
#endif
      if(is_p != 7) {
        real t1,t2;

        t1 = tab1[is_p]; t2 = tab2[is_p];
        for( ; sb > 0; sb--,idx+=3 ) {  
          real v = xr[0][idx];

					xr[0][idx] = REAL_MUL(v, t1);
					xr[1][idx] = REAL_MUL(v, t2);
          }
        }
      }   // end for(lwin; .. ; . ) 

/* also check l-part, if ALL bands in the three windows are 'empty'
 * and mode = mixed_mode 
 */
    if(do_l) {
			int sfb = gr_info->maxbandl;
      int idx;

      if(sfb > 21) 
				return; /* similarity fix related to CVE-2006-1655 */
      idx = bi->longIdx[sfb];

	    for( ; sfb<8; sfb++ ) {
				int sb = bi->longDiff[sfb];
				int is_p = scalefac[sfb];   // scale: 0-15 

        if(is_p != 7) {
          real t1,t2;

          t1 = tab1[is_p]; t2 = tab2[is_p];
          for( ; sb > 0; sb--,idx++) {
            real v = xr[0][idx];

						xr[0][idx] = REAL_MUL(v, t1);
						xr[1][idx] = REAL_MUL(v, t2);
            }
          }
        else 
          idx += sb;
        }
      }     
    } 
  else {		 // ((gr_info->block_type != 2)) 
    int sfb = gr_info->maxbandl;
		int is_p,idx;

		if(sfb > 21) return; /* tightened fix for CVE-2006-1655 */
		idx = bi->longIdx[sfb];

    for( ; sfb<21; sfb++) {
	    int sb = bi->longDiff[sfb];

      is_p = scalefac[sfb];    // scale: 0-15 
      if(is_p != 7) {
        real t1,t2;

        t1 = tab1[is_p]; t2 = tab2[is_p];
        for( ; sb > 0; sb--,idx++) {
          real v = xr[0][idx];

					xr[0][idx] = REAL_MUL(v, t1);
					xr[1][idx] = REAL_MUL(v, t2);
				  }
        }
      else
        idx += sb;
      }

    is_p = scalefac[20];     // copy l-band 20 to l-band 21 
    if(is_p != 7) {
      int sb;
      real t1 = tab1[is_p],t2 = tab2[is_p]; 

      for(sb = bi->longDiff[21]; sb > 0; sb--,idx++ ) {
        real v = xr[0][idx];

        xr[0][idx] = REAL_MUL(v, t1);
        xr[1][idx] = REAL_MUL(v, t2);
        }
      }
    } /* ... */
	}

void CJoshuaMP3::III_antialias(real xr[SBLIMIT][SSLIMIT],struct GR_INFO_S *gr_info) {
  int sblim;

  if(gr_info->blockType == 2) {
    if(!gr_info->mixedBlockFlag) 
      return;
    sblim = 1; 
	  }
  else {
    sblim = gr_info->maxb-1;
		if(sblim<0)
			return;			// DARIO! capita maxb==0 in certi casi, ed e' un crash!
	  }

   /* 31 alias-reduction operations between each pair of sub-bands */
   /* with 8 butterflies between each pair                         */

   {
    int sb;
    real *xr1=(real *)xr[1];

    for(sb=sblim; sb; sb--,xr1+=10) {
      int ss;
      real *cs=aa_cs,*ca=aa_ca;
      real *xr2 = xr1;

      for(ss=7; ss>=0; ss--) {       // upper and lower butterfly inputs 
        register real bu = *--xr2,bd = *xr1;

        *xr2   = REAL_MUL(bu, *cs) - REAL_MUL(bd, *ca);
        *xr1++ = REAL_MUL(bd, *cs++) + REAL_MUL(bu, *ca++);
        }
      }
		}
	}


/* 
// This is an optimized DCT from Jeff Tsay's maplay 1.2+ package.
// Saved one multiplication by doing the 'twiddle factor' stuff
// together with the window mul. (MH)
//
// This uses Byeong Gi Lee's Fast Cosine Transform algorithm, but the
// 9 point IDCT needs to be reduced further. Unfortunately, I don't
// know how to do that, because 9 is not an even number. - Jeff.
//
//////////////////////////////////////////////////////////////////
//
// 9 Point Inverse Discrete Cosine Transform
//
// This piece of code is Copyright 1997 Mikko Tommila and is freely usable
// by anybody. The algorithm itself is of course in the public domain.
//
// Again derived heuristically from the 9-point WFTA.
//
// The algorithm is optimized (?) for speed, not for small rounding errors or
// good readability.
//
// 36 additions, 11 multiplications
//
// Again this is very likely sub-optimal.
//
// The code is optimized to use a minimum number of temporary variables,
// so it should compile quite well even on 8-register Intel x86 processors.
// This makes the code quite obfuscated and very difficult to understand.
//
// References:
// [1] S. Winograd: "On Computing the Discrete Fourier Transform",
//     Mathematics of Computation, Volume 32, Number 141, January 1978,
//     Pages 175-199
*/

/*------------------------------------------------------------------*/
/*                                                                  */
/*    Function: Calculation of the inverse MDCT                     */
/*                                                                  */
/*------------------------------------------------------------------*/

void CJoshuaMP3::dct36(real *inbuf,real *o1,real *o2,const real *wintab,real *tsbuf)	{
#ifdef NEW_DCT9
  real tmp[18];
#endif

  {
    register real *in = inbuf;

    in[17]+=in[16]; in[16]+=in[15]; in[15]+=in[14];
    in[14]+=in[13]; in[13]+=in[12]; in[12]+=in[11];
    in[11]+=in[10]; in[10]+=in[9];  in[9] +=in[8];
    in[8] +=in[7];  in[7] +=in[6];  in[6] +=in[5];
    in[5] +=in[4];  in[4] +=in[3];  in[3] +=in[2];
    in[2] +=in[1];  in[1] +=in[0];

    in[17]+=in[15]; in[15]+=in[13]; in[13]+=in[11]; in[11]+=in[9];
    in[9] +=in[7];  in[7] +=in[5];  in[5] +=in[3];  in[3] +=in[1];


#ifdef NEW_DCT9
#if 1
    {
     real t3;
     { 
      real t0, t1, t2;

      t0 = REAL_MUL(COS6_2, (in[8] + in[16] - in[4]));
      t1 = REAL_MUL(COS6_2, in[12]);

      t3 = in[0];
      t2 = t3 - t1 - t1;
      tmp[1] = tmp[7] = t2 - t0;
      tmp[4]          = t2 + t0 + t0;
      t3 += t1;

      t2 = REAL_MUL(COS6_1, (in[10] + in[14] - in[2]));
      tmp[1] -= t2;
      tmp[7] += t2;
     }
     {
      real t0, t1, t2;

      t0 = REAL_MUL(cos9[0], (in[4] + in[8] ));
      t1 = REAL_MUL(cos9[1], (in[8] - in[16]));
      t2 = REAL_MUL(cos9[2], (in[4] + in[16]));

      tmp[2] = tmp[6] = t3 - t0      - t2;
      tmp[0] = tmp[8] = t3 + t0 + t1;
      tmp[3] = tmp[5] = t3      - t1 + t2;
     }
    }
    {
      real t1, t2, t3;

      t1 = REAL_MUL(cos18[0], (in[2]  + in[10]));
      t2 = REAL_MUL(cos18[1], (in[10] - in[14]));
      t3 = REAL_MUL(COS6_1,    in[6]);

      {
        real t0 = t1 + t2 + t3;
        tmp[0] += t0;
        tmp[8] -= t0;
      }

      t2 -= t3;
      t1 -= t3;

      t3 = REAL_MUL(cos18[2], (in[2] + in[14]));

      t1 += t3;
      tmp[3] += t1;
      tmp[5] -= t1;

      t2 -= t3;
      tmp[2] += t2;
      tmp[6] -= t2;
    }

#else
    {
      real t0, t1, t2, t3, t4, t5, t6, t7;

      t1 = REAL_MUL(COS6_2, in[12]);
      t2 = REAL_MUL(COS6_2, (in[8] + in[16] - in[4]));

      t3 = in[0] + t1;
      t4 = in[0] - t1 - t1;
      t5     = t4 - t2;
      tmp[4] = t4 + t2 + t2;

      t0 = REAL_MUL(cos9[0], (in[4] + in[8]));
      t1 = REAL_MUL(cos9[1], (in[8] - in[16]));

      t2 = REAL_MUL(cos9[2], (in[4] + in[16]));

      t6 = t3 - t0 - t2;
      t0 += t3 + t1;
      t3 += t2 - t1;

      t2 = REAL_MUL(cos18[0], (in[2]  + in[10]));
      t4 = REAL_MUL(cos18[1], (in[10] - in[14]));
      t7 = REAL_MUL(COS6_1, in[6]);

      t1 = t2 + t4 + t7;
      tmp[0] = t0 + t1;
      tmp[8] = t0 - t1;
      t1 = REAL_MUL(cos18[2], (in[2] + in[14]));
      t2 += t1 - t7;

      tmp[3] = t3 + t2;
      t0 = REAL_MUL(COS6_1, (in[10] + in[14] - in[2]));
      tmp[5] = t3 - t2;

      t4 -= t1 + t7;

      tmp[1] = t5 - t0;
      tmp[7] = t5 + t0;
      tmp[2] = t6 + t4;
      tmp[6] = t6 - t4;
    }
#endif

    {
      real t0, t1, t2, t3, t4, t5, t6, t7;

      t1 = REAL_MUL(COS6_2, in[13]);
      t2 = REAL_MUL(COS6_2, (in[9] + in[17] - in[5]));

      t3 = in[1] + t1;
      t4 = in[1] - t1 - t1;
      t5 = t4 - t2;

      t0 = REAL_MUL(cos9[0], (in[5] + in[9]));
      t1 = REAL_MUL(cos9[1], (in[9] - in[17]));

      tmp[13] = REAL_MUL((t4 + t2 + t2), tfcos36[17-13]);
      t2 = REAL_MUL(cos9[2], (in[5] + in[17]));

      t6 = t3 - t0 - t2;
      t0 += t3 + t1;
      t3 += t2 - t1;

      t2 = REAL_MUL(cos18[0], (in[3]  + in[11]));
      t4 = REAL_MUL(cos18[1], (in[11] - in[15]));
      t7 = REAL_MUL(COS6_1, in[7]);

      t1 = t2 + t4 + t7;
      tmp[17] = REAL_MUL((t0 + t1), tfcos36[17-17]);
      tmp[9]  = REAL_MUL((t0 - t1), tfcos36[17-9]);
      t1 = REAL_MUL(cos18[2], (in[3] + in[15]));
      t2 += t1 - t7;

      tmp[14] = REAL_MUL((t3 + t2), tfcos36[17-14]);
      t0 = REAL_MUL(COS6_1, (in[11] + in[15] - in[3]));
      tmp[12] = REAL_MUL((t3 - t2), tfcos36[17-12]);

      t4 -= t1 + t7;

      tmp[16] = REAL_MUL((t5 - t0), tfcos36[17-16]);
      tmp[10] = REAL_MUL((t5 + t0), tfcos36[17-10]);
      tmp[15] = REAL_MUL((t6 + t4), tfcos36[17-15]);
      tmp[11] = REAL_MUL((t6 - t4), tfcos36[17-11]);
   }

#define MACRO(v) { \
    real tmpval; \
    tmpval = tmp[(v)] + tmp[17-(v)]; \
    out2[9+(v)] = REAL_MUL(tmpval, w[27+(v)]); \
    out2[8-(v)] = REAL_MUL(tmpval, w[26-(v)]); \
    tmpval = tmp[(v)] - tmp[17-(v)]; \
    ts[SBLIMIT*(8-(v))] = out1[8-(v)] + REAL_MUL(tmpval, w[8-(v)]); \
    ts[SBLIMIT*(9+(v))] = out1[9+(v)] + REAL_MUL(tmpval, w[9+(v)]); }

	{
		register real *out2 = o2;
		register real *w = (real*)wintab;
		register real *out1 = o1;
		register real *ts = tsbuf;

		MACRO(0);
		MACRO(1);
		MACRO(2);
		MACRO(3);
		MACRO(4);
		MACRO(5);
		MACRO(6);
		MACRO(7);
		MACRO(8);
	}

#else

  {

#define MACRO0(v) { \
    real tmp; \
    out2[9+(v)] = REAL_MUL((tmp = sum0 + sum1), w[27+(v)]); \
    out2[8-(v)] = REAL_MUL(tmp, w[26-(v)]);   } \
    sum0 -= sum1; \
    ts[SBLIMIT*(8-(v))] = out1[8-(v)] + REAL_MUL(sum0, w[8-(v)]); \
    ts[SBLIMIT*(9+(v))] = out1[9+(v)] + REAL_MUL(sum0, w[9+(v)]);
#define MACRO1(v) { \
	real sum0,sum1; \
    sum0 = tmp1a + tmp2a; \
	sum1 = REAL_MUL((tmp1b + tmp2b), tfcos36[(v)]); \
	MACRO0(v); }
#define MACRO2(v) { \
    real sum0,sum1; \
    sum0 = tmp2a - tmp1a; \
    sum1 = REAL_MUL((tmp2b - tmp1b), tfcos36[(v)]); \
	MACRO0(v); }

    register const real *c = COS9;
    register real *out2 = o2;
		register real *w = wintab;
		register real *out1 = o1;
		register real *ts = tsbuf;

    real ta33,ta66,tb33,tb66;

    ta33 = REAL_MUL(in[2*3+0], c[3]);
    ta66 = REAL_MUL(in[2*6+0], c[6]);
    tb33 = REAL_MUL(in[2*3+1], c[3]);
    tb66 = REAL_MUL(in[2*6+1], c[6]);

    { 
      real tmp1a,tmp2a,tmp1b,tmp2b;
      tmp1a = REAL_MUL(in[2*1+0], c[1]) + ta33 + REAL_MUL(in[2*5+0], c[5]) + REAL_MUL(in[2*7+0], c[7]);
      tmp1b = REAL_MUL(in[2*1+1], c[1]) + tb33 + REAL_MUL(in[2*5+1], c[5]) + REAL_MUL(in[2*7+1], c[7]);
      tmp2a = REAL_MUL(in[2*2+0], c[2]) + REAL_MUL(in[2*4+0], c[4]) + ta66 + REAL_MUL(in[2*8+0], c[8]);
      tmp2b = REAL_MUL(in[2*2+1], c[2]) + REAL_MUL(in[2*4+1], c[4]) + tb66 + REAL_MUL(in[2*8+1], c[8]);

      MACRO1(0);
      MACRO2(8);
    }

    {
      real tmp1a,tmp2a,tmp1b,tmp2b;
      tmp1a = REAL_MUL(( in[2*1+0] - in[2*5+0] - in[2*7+0] ), c[3]);
      tmp1b = REAL_MUL(( in[2*1+1] - in[2*5+1] - in[2*7+1] ), c[3]);
      tmp2a = REAL_MUL(( in[2*2+0] - in[2*4+0] - in[2*8+0] ), c[6]) - in[2*6+0] + in[2*0+0];
      tmp2b = REAL_MUL(( in[2*2+1] - in[2*4+1] - in[2*8+1] ), c[6]) - in[2*6+1] + in[2*0+1];

      MACRO1(1);
      MACRO2(7);
    }

    {
      real tmp1a,tmp2a,tmp1b,tmp2b;
      tmp1a =   REAL_MUL(in[2*1+0], c[5]) - ta33 - REAL_MUL(in[2*5+0], c[7]) + REAL_MUL(in[2*7+0], c[1]);
      tmp1b =   REAL_MUL(in[2*1+1], c[5]) - tb33 - REAL_MUL(in[2*5+1], c[7]) + REAL_MUL(in[2*7+1], c[1]);
      tmp2a = - REAL_MUL(in[2*2+0], c[8]) - REAL_MUL(in[2*4+0], c[2]) + ta66 + REAL_MUL(in[2*8+0], c[4]);
      tmp2b = - REAL_MUL(in[2*2+1], c[8]) - REAL_MUL(in[2*4+1], c[2]) + tb66 + REAL_MUL(in[2*8+1], c[4]);

      MACRO1(2);
      MACRO2(6);
    }

    {
      real tmp1a,tmp2a,tmp1b,tmp2b;
      tmp1a =   REAL_MUL(in[2*1+0], c[7]) - ta33 + REAL_MUL(in[2*5+0], c[1]) - REAL_MUL(in[2*7+0], c[5]);
      tmp1b =   REAL_MUL(in[2*1+1], c[7]) - tb33 + REAL_MUL(in[2*5+1], c[1]) - REAL_MUL(in[2*7+1], c[5]);
      tmp2a = - REAL_MUL(in[2*2+0], c[4]) + REAL_MUL(in[2*4+0], c[8]) + ta66 - REAL_MUL(in[2*8+0], c[2]);
      tmp2b = - REAL_MUL(in[2*2+1], c[4]) + REAL_MUL(in[2*4+1], c[8]) + tb66 - REAL_MUL(in[2*8+1], c[2]);

      MACRO1(3);
      MACRO2(5);
    }

	{
		real sum0,sum1;
    	sum0 =  in[2*0+0] - in[2*2+0] + in[2*4+0] - in[2*6+0] + in[2*8+0];
    	sum1 = REAL_MUL((in[2*0+1] - in[2*2+1] + in[2*4+1] - in[2*6+1] + in[2*8+1] ), tfcos36[4]);
		MACRO0(4);
	}
		}
#endif

		}
	}

/*
 * new DCT12
 */
void CJoshuaMP3::dct12(real *in,real *rawout1,real *rawout2,register const real *wi,register real *ts)	{
#define DCT12_PART1 \
             in5 = in[5*3];  \
     in5 += (in4 = in[4*3]); \
     in4 += (in3 = in[3*3]); \
     in3 += (in2 = in[2*3]); \
     in2 += (in1 = in[1*3]); \
     in1 += (in0 = in[0*3]); \
                             \
     in5 += in3; in3 += in1; \
                             \
     in2 = REAL_MUL(in2, COS6_1); \
     in3 = REAL_MUL(in3, COS6_1); \

#define DCT12_PART2 \
     in0 += REAL_MUL(in4, COS6_2); \
                          \
     in4 = in0 + in2;     \
     in0 -= in2;          \
                          \
     in1 += REAL_MUL(in5, COS6_2); \
                          \
     in5 = REAL_MUL((in1 + in3), tfcos12[0]); \
     in1 = REAL_MUL((in1 - in3), tfcos12[2]); \
                         \
     in3 = in4 + in5;    \
     in4 -= in5;         \
                         \
     in2 = in0 + in1;    \
     in0 -= in1;


   {
     real in0,in1,in2,in3,in4,in5;
     register real *out1 = rawout1;
     ts[SBLIMIT*0] = out1[0]; ts[SBLIMIT*1] = out1[1]; ts[SBLIMIT*2] = out1[2];
     ts[SBLIMIT*3] = out1[3]; ts[SBLIMIT*4] = out1[4]; ts[SBLIMIT*5] = out1[5];
 
     DCT12_PART1

     {
       real tmp0,tmp1 = (in0 - in4);
       {
         real tmp2 = REAL_MUL((in1 - in5), tfcos12[1]);
         tmp0 = tmp1 + tmp2;
         tmp1 -= tmp2;
       }
       ts[(17-1)*SBLIMIT] = out1[17-1] + REAL_MUL(tmp0, wi[11-1]);
       ts[(12+1)*SBLIMIT] = out1[12+1] + REAL_MUL(tmp0, wi[6+1]);
       ts[(6 +1)*SBLIMIT] = out1[6 +1] + REAL_MUL(tmp1, wi[1]);
       ts[(11-1)*SBLIMIT] = out1[11-1] + REAL_MUL(tmp1, wi[5-1]);
     }

     DCT12_PART2

     ts[(17-0)*SBLIMIT] = out1[17-0] + REAL_MUL(in2, wi[11-0]);
     ts[(12+0)*SBLIMIT] = out1[12+0] + REAL_MUL(in2, wi[6+0]);
     ts[(12+2)*SBLIMIT] = out1[12+2] + REAL_MUL(in3, wi[6+2]);
     ts[(17-2)*SBLIMIT] = out1[17-2] + REAL_MUL(in3, wi[11-2]);

     ts[(6 +0)*SBLIMIT]  = out1[6+0] + REAL_MUL(in0, wi[0]);
     ts[(11-0)*SBLIMIT] = out1[11-0] + REAL_MUL(in0, wi[5-0]);
     ts[(6 +2)*SBLIMIT]  = out1[6+2] + REAL_MUL(in4, wi[2]);
     ts[(11-2)*SBLIMIT] = out1[11-2] + REAL_MUL(in4, wi[5-2]);
  }

  in++;

  {
     real in0,in1,in2,in3,in4,in5;
     register real *out2 = rawout2;
 
     DCT12_PART1

     {
       real tmp0,tmp1 = (in0 - in4);
       {
         real tmp2 = REAL_MUL((in1 - in5), tfcos12[1]);
         tmp0 = tmp1 + tmp2;
         tmp1 -= tmp2;
       }
       out2[5-1] = REAL_MUL(tmp0, wi[11-1]);
       out2[0+1] = REAL_MUL(tmp0, wi[6+1]);
       ts[(12+1)*SBLIMIT] += REAL_MUL(tmp1, wi[1]);
       ts[(17-1)*SBLIMIT] += REAL_MUL(tmp1, wi[5-1]);
     }

     DCT12_PART2

     out2[5-0] = REAL_MUL(in2, wi[11-0]);
     out2[0+0] = REAL_MUL(in2, wi[6+0]);
     out2[0+2] = REAL_MUL(in3, wi[6+2]);
     out2[5-2] = REAL_MUL(in3, wi[11-2]);

     ts[(12+0)*SBLIMIT] += REAL_MUL(in0, wi[0]);
     ts[(17-0)*SBLIMIT] += REAL_MUL(in0, wi[5-0]);
     ts[(12+2)*SBLIMIT] += REAL_MUL(in4, wi[2]);
     ts[(17-2)*SBLIMIT] += REAL_MUL(in4, wi[5-2]);
  }

  in++; 

  {
     real in0,in1,in2,in3,in4,in5;
     register real *out2 = rawout2;
     out2[12]=out2[13]=out2[14]=out2[15]=out2[16]=out2[17]=0.0;

     DCT12_PART1

     {
       real tmp0,tmp1 = (in0 - in4);
       {
         real tmp2 = REAL_MUL((in1 - in5), tfcos12[1]);
         tmp0 = tmp1 + tmp2;
         tmp1 -= tmp2;
       }
       out2[11-1] = REAL_MUL(tmp0, wi[11-1]);
       out2[6 +1] = REAL_MUL(tmp0, wi[6+1]);
       out2[0+1] += REAL_MUL(tmp1, wi[1]);
       out2[5-1] += REAL_MUL(tmp1, wi[5-1]);
     }

     DCT12_PART2

     out2[11-0] = REAL_MUL(in2, wi[11-0]);
     out2[6 +0] = REAL_MUL(in2, wi[6+0]);
     out2[6 +2] = REAL_MUL(in3, wi[6+2]);
     out2[11-2] = REAL_MUL(in3, wi[11-2]);

     out2[0+0] += REAL_MUL(in0, wi[0]);
     out2[5-0] += REAL_MUL(in0, wi[5-0]);
     out2[0+2] += REAL_MUL(in4, wi[2]);
     out2[5-2] += REAL_MUL(in4, wi[5-2]);
		}
	}

/*
 * III_hybrid
 */
void CJoshuaMP3::III_hybrid(real fsIn[SBLIMIT][SSLIMIT],real tsOut[SSLIMIT][SBLIMIT],BYTE ch,struct GR_INFO_S *gr_info) {
//  static 2023 real block[2][2][SBLIMIT*SSLIMIT] = { { { 0, } } };
//  static 2023 int blc[2]={0,0};
  real *tspnt = (real *)tsOut;
  real *rawout1,*rawout2;
  BYTE bt;
	int sb = 0;

  {
    int b = blc[ch];

    rawout1=block[b][ch];
    b=-b+1;
    rawout2=block[b][ch];
    blc[ch] = b;
  }
  
  if(gr_info->mixedBlockFlag) {
    sb = 2;
#ifdef USE_3DNOW
     (fr->dct36)(fsIn[0],rawout1,rawout2,win[0],tspnt);
     (fr->dct36)(fsIn[1],rawout1+18,rawout2+18,win1[0],tspnt+1);
#else
    dct36(fsIn[0],rawout1,rawout2,win[0],tspnt);
    dct36(fsIn[1],rawout1+18,rawout2+18,win1[0],tspnt+1);
#endif
    rawout1 += 36/*SSLIMIT*2*/; rawout2 += 36/*SSLIMIT*2*/; tspnt += 2;
		}
 
  bt = gr_info->blockType;
  if(bt == 2) {
    for(; sb<gr_info->maxb; sb+=2,tspnt+=2,rawout1+=36/*SSLIMIT*2*/,rawout2+=36/*SSLIMIT*2*/) {
      dct12(fsIn[sb],rawout1,rawout2,win[2],tspnt);
      dct12(fsIn[sb+1],rawout1+18/*SSLIMIT*/,rawout2+18/*SSLIMIT*/,win1[2],tspnt+1);
		  }
		}
  else {
    for(; sb<gr_info->maxb; sb+=2,tspnt+=2,rawout1+=36/*SSLIMIT*2*/,rawout2+=36/*SSLIMIT*2*/) {
#ifdef USE_3DNOW
       (fr->dct36)(fsIn[sb],rawout1,rawout2,win[bt],tspnt);
       (fr->dct36)(fsIn[sb+1],rawout1+18/*SSLIMIT*/,rawout2+18/*SSLIMIT*/,win1[bt],tspnt+1);
#else
      dct36(fsIn[sb],rawout1,rawout2,win[bt],tspnt);
      dct36(fsIn[sb+1],rawout1+18/*SSLIMIT*/,rawout2+18/*SSLIMIT*/,win1[bt],tspnt+1);
#endif
      }
    }

  for(; sb<SBLIMIT; sb++,tspnt++) {
    int i;

    for(i=0; i<SSLIMIT; i++) {
      tspnt[i*SBLIMIT] = *rawout1++;
      *rawout2++ = DOUBLE_TO_REAL(0.0);
      }
    }
  }


/*
 * main layer3 handler
 */
int CJoshuaMP3::DoLayer3() {
  BYTE gr,ch;
	int ss,clip=0;
  int scalefacs[2][39];	   // max 39 for short[13][3] mode, mixed: 38, long: 22 
  struct III_SIDE_INFO sideinfo;
  BYTE stereo = fr.stereo;
  short int single = fr.single;
  BYTE ms_stereo,i_stereo;
  BYTE sfreq = fr.samplingFrequency;
  BYTE stereo1,granules;

  if(stereo == 1) {
    stereo1 = 1;
    single = 0; 
		}
  else if(single >= 0)
    stereo1 = 1;
  else
    stereo1 = 2;

  if(fr.mode == MPG_MD_JOINT_STEREO) {
    ms_stereo = (fr.modeExt & 0x2) >> 1;
    i_stereo  = fr.modeExt & 0x1;
	  }
  else
    ms_stereo = i_stereo = 0;


  if(fr.lsf) {
    granules = 1;
#if 0
		III_getSideInfo2(&sideinfo,stereo,ms_stereo,sfreq,single);
#endif
	  }
  else
    granules = 2;
		
  if(III_getSideInfo1(&sideinfo,stereo,ms_stereo,sfreq,single,fr.lsf)) {
    debug_print(CLogFile::flagError2,"mpg123: bad frame - unable to get valid sideinfo");
    return clip;
		}
  

  setPointer(sideinfo.mainDataBegin);

  for(gr=0; gr<granules; gr++) {
		real hybridIn[2][SBLIMIT][SSLIMIT];
		real hybridOut[2][SSLIMIT][SBLIMIT];

    {
      struct GR_INFO_S *gr_info = &(sideinfo.ch[0].gr[gr]);
      long part2bits;

      if(fr.lsf)
        part2bits = III_getScaleFactors2(scalefacs[0],gr_info,0);
      else
        part2bits = III_getScaleFactors1(scalefacs[0],gr_info,0,gr);
      if(III_dequantizeSample(hybridIn[0],scalefacs[0],gr_info,sfreq,part2bits))
        return clip;
    }

    if(stereo == 2) {
      struct GR_INFO_S *gr_info = &(sideinfo.ch[1].gr[gr]);
      long part2bits;

      if(fr.lsf) 
        part2bits = III_getScaleFactors2(scalefacs[1],gr_info,i_stereo);
      else
        part2bits = III_getScaleFactors1(scalefacs[1],gr_info,1,gr);

      if(III_dequantizeSample(hybridIn[1],scalefacs[1],gr_info,sfreq,part2bits))
        return clip;

      if(ms_stereo) {
        int i;
        int maxb = sideinfo.ch[0].gr[gr].maxb;
        if(sideinfo.ch[1].gr[gr].maxb > maxb)
          maxb = sideinfo.ch[1].gr[gr].maxb;
        for(i=0; i<SSLIMIT*maxb; i++) {
          real tmp0 = ((real *)hybridIn[0])[i];
          real tmp1 = ((real *)hybridIn[1])[i];
          ((real *)hybridIn[0])[i] = tmp0 + tmp1;
          ((real *)hybridIn[1])[i] = tmp0 - tmp1;
					}
				}

      if(i_stereo)
        III_IStereo(hybridIn,scalefacs[1],gr_info,sfreq,ms_stereo,fr.lsf);

      if(ms_stereo || i_stereo || (single == 3)) {
        if(gr_info->maxb > sideinfo.ch[0].gr[gr].maxb) 
          sideinfo.ch[0].gr[gr].maxb = gr_info->maxb;
        else
          gr_info->maxb = sideinfo.ch[0].gr[gr].maxb;
				}

      switch(single) {
        case 3:
          {
            register int i;
            register real *in0 = (real *)hybridIn[0],*in1 = (real *)hybridIn[1];

            for(i=0; i<SSLIMIT*gr_info->maxb; i++,in0++)
              *in0 = (*in0 + *in1++);			// *0.5 done by pow-scale 
          }
          break;
        case 1:
          {
            register int i;
            register real *in0 = (real *)hybridIn[0],*in1 = (real *)hybridIn[1];

            for(i=0; i<SSLIMIT*gr_info->maxb; i++)
              *in0++ = *in1++;
          }
          break;
		    }
	    }	     // if
			

    for(ch=0; ch<stereo1; ch++) {
      struct GR_INFO_S *gr_info = &(sideinfo.ch[ch].gr[gr]);

      III_antialias(hybridIn[ch],gr_info);
#ifdef USE_3DNOW
9      III_hybrid(hybridIn[ch], hybridOut[ch], ch,gr_info,fr);
#else
//			if(bDebug) {
      III_hybrid(hybridIn[ch], hybridOut[ch], ch,gr_info);
//     }
#endif
      }

#ifdef I486_OPT
    if(fr.synth != synth_1to1 || single >= 0) {
#endif
    for(ss=0; ss<SSLIMIT; ss++) {

//			if(bDebug) {
      if(single >= 0) {
        clip += (this->*fr.synthMono)(hybridOut[0][ss],pcmSample,&pcmPoint);
// patch test mono 2021        clip += (this->*fr.synthMono)(hybridOut[0][ss],pcmSample,&pcmPoint);
				// non va ancora... salta
				}
	      
      else {
        int p1 = pcmPoint;
        clip += (this->*fr.synth)(hybridOut[0][ss],0,pcmSample,&p1);
        clip += (this->*fr.synth)(hybridOut[1][ss],1,pcmSample,&pcmPoint);
	      }
//      pcmPoint += fr.blockSize;
/*      if(fr.blockSize == 6) {		 // patch per convertire correttamente a 8000Hz...
				static short int handicap; // devo prendere 5,80 bytes ogni 8: v. synth_5to1_mono e qui tolgo ancora 1 ogni 30...

				handicap++;
				if(!(handicap % 5))
				  pcmPoint--;
				}*/
	//		}			
#ifdef VARMODESUPPORT
      if(playlimit < 128) {
        pcmPoint -= playlimit >> 1;
        playlimit = 0;
				}
      else
        playlimit -= 128;
#endif


//			if(bDebug) {
      if(pcmPoint >= ai->audiobufsize)
        Flush();
//			}
//			else
//				pcmPoint =0;


		  }		 // for
#ifdef I486_OPT
    } 
		else {
      /* Only stereo, 16 bits benefit from the 486 optimization. */
      ss=0;
      while(ss < SSLIMIT) {
        int n;
        n=(ai->audiobufsize - pcmPoint) / (2*2*32);
        if(n > (SSLIMIT-ss)) 
					n=SSLIMIT-ss;
        
        synth_1to1_486(hybridOut[0][ss],0,pcmSample+pcmPoint,n);
        synth_1to1_486(hybridOut[1][ss],1,pcmSample+pcmPoint,n);
        ss+=n;
        pcmPoint+=(2*2*32)*n;
        
        if(pcmPoint >= audiobufsize) 
	        Flush();
      }
    }
#endif
    }

  return clip;
	}


CMP3Audio::CMP3Audio(CJoshuaMP3 *p,HWAVEOUT hw,enum AUDIO_FORMATS f,BYTE ch,DWORD r,DWORD bsize,BYTE v) {

	m_parent=p;
	hWaveOut=hw;
	perif=0;
	format=f;
	channels=ch;
	rate=r;
	audiobufsize=bsize;
	verbose=v;
	init();
	}

CMP3Audio::CMP3Audio(CJoshuaMP3 *p,BYTE pf,enum AUDIO_FORMATS f,BYTE ch,DWORD r,DWORD bsize,BYTE v) {

	m_parent=p;
	hWaveOut=(HWAVEOUT)-1;
	perif=pf;
	format=f;
	channels=ch;
	rate=r;
	audiobufsize=bsize;
	verbose=v;
	init();
	}
//https://stackoverflow.com/questions/308276/can-i-call-a-constructor-from-another-constructor-do-constructor-chaining-in-c
void CMP3Audio::init() {
	register int i;
//						debug_print(CLogFile::flagInfo,"  entra ctor.");
	m_soundPlayerEventSize=0;
	m_nbSoundPlayerEvents=TOT_BUFFERS;
	m_pSoundPlayer=NULL;
	m_soundPlayerEventNbSamples=0;
	m_pSamples=NULL;
	for(i=0; i<TOT_BUFFERS; i++)
		buf[i]=NULL;
	which=0;
	reserved=0;
	critical=0;
	gain=-1;
	enabled=TRUE;
	event=NULL;
	m_exprMutex=NULL;
	nWnd=NULL;
	}


CMP3Audio::~CMP3Audio() {
	int i;

	if((hWaveOut && hWaveOut!=(HWAVEOUT)-1) || event || m_pSoundPlayer)
		close();
	for(i=0; i<TOT_BUFFERS; i++) {
		if(buf[i]) {
			HeapFree(GetProcessHeap(),0,buf[i]);
			buf[i]=NULL;
			}
		}
	if(m_exprMutex)
		delete m_exprMutex;
	m_exprMutex=NULL;


//						debug_print(CLogFile::flagInfo,"  esce dtor.");

	}

int CMP3Audio::open(BYTE q) {
	int i,retVal=-1;

//						debug_print(CLogFile::flagInfo,"  entra open.");


	if(hWaveOut && hWaveOut!=(HWAVEOUT)-1) {
		event=CreateEvent(NULL,FALSE,FALSE,NULL);
		if(!event) {
			if(verbose)
		#ifdef _DEBUG
				AfxMessageBox("Impossibile creare Event");
		#else
				debug_print(CLogFile::flagError,"Impossibile creare Event");
		#endif
			goto fine;
			}
#ifdef ONLY_22KHZ_AUDIO
		wf.wFormatTag=WAVE_FORMAT_PCM;
		wf.nChannels=channels;
		wf.nSamplesPerSec=22050;
		wf.nAvgBytesPerSec=22050*2*2;
		wf.nBlockAlign=2;
		wf.wBitsPerSample=16;
		wf.cbSize=0;
		buf1=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,audiobufsize);
		buf2=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,audiobufsize);
		buf3=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,audiobufsize);
		buf4=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,audiobufsize);
#else
		wf.wFormatTag=WAVE_FORMAT_PCM;
		wf.nChannels=channels;
		wf.nSamplesPerSec=rate /*44100*/;
		wf.nBlockAlign=format == AUDIO_FORMAT_UNSIGNED_8 ? 1 : 2;
		wf.wBitsPerSample=format == AUDIO_FORMAT_UNSIGNED_8 ? 8 : 16;
		wf.nAvgBytesPerSec=wf.nSamplesPerSec*wf.nChannels*wf.nBlockAlign;
		wf.cbSize=0;
		for(i=0; i<TOT_BUFFERS; i++) {
			buf[i]=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,audiobufsize+20);	// a volte le synth possono sforare di qualche byte...
			if(!buf[i]) {
				if(verbose)
#ifdef _DEBUG
					AfxMessageBox("Impossibile allocare buffer");
#else
					debug_print(CLogFile::flagError,"Impossibile allocare buffer");
#endif
				goto fine;
				}
			}
#endif

		if(!hWaveOut) {
			reserved=0;
			i=waveOutOpen(&hWaveOut,WAVE_MAPPER,&wf,(DWORD)event,(DWORD)0l,WAVE_ALLOWSYNC | CALLBACK_EVENT);
			if(i != MMSYSERR_NOERROR) {
				if(verbose)
#ifdef _DEBUG
					AfxMessageBox("Impossibile aprire waveOut");
#else
					debug_print(CLogFile::flagError,"Impossibile aprire waveOut");
#endif
				}
			else
				retVal=0;
			}
		else {
			reserved=1;
			retVal=0;
			}

fine:
		which=critical=0;
//	  InitializeCriticalSection(&m_cs);
		}
	else {
		Player *pPlayer = new Player(theApp.m_pMainWnd->m_hWnd /* non la usiamo qui... ma se non la metti non suona! */);
//		m_exprMutex=new Mutex;
//		nWnd=new PlayerNotifyWnd(this);
		if(pPlayer->Init(q)) {
			m_pSoundPlayer = pPlayer;

			SOUNDFORMAT myFormat;
			myFormat.NbBitsPerSample = format == AUDIO_FORMAT_UNSIGNED_8 ? 8 : 16;
			myFormat.NbChannels = channels;
			myFormat.SamplingRate = rate /*44100*/;
			m_soundPlayerEventNbSamples=rate;

	//		audiobufsize= (m_soundPlayerEventNbSamples * myFormat.NbBitsPerSample*myFormat.NbChannels)/8;
	//		DWORD m_soundPlayerEventSize = (m_soundPlayerEventNbSamples * myFormat.NbBitsPerSample*myFormat.NbChannels)/8;	
			DWORD m_soundPlayerEventSize = AUDIO_BUF_SIZE;
			int bufferSize = m_soundPlayerEventSize * m_nbSoundPlayerEvents;
						
			m_pSoundPlayer->CreateSoundBuffer(myFormat, bufferSize);

	//			m_pSoundPlayer->SetSoundEventListener(nWnd);
			std::vector<DWORD> events;

			for(int t=0; t<m_nbSoundPlayerEvents; t++)
				events.push_back((t+1)*m_soundPlayerEventSize *1 /*m_soundPlayerEventSize  *0.5*/ /* 0.95 */ );
	//			events.push_back((t+1)*m_soundPlayerEventSize *.95 /*- 1*/ /*m_soundPlayerEventSize*0.95*/ );
			events[m_nbSoundPlayerEvents-1]--;		//2023

			m_pSoundPlayer->CreateEventReadNotification(events);
						
	//	  InitializeCriticalSection(&m_cs);
			retVal=0;
			}
		else
			delete pPlayer;
		}

//						debug_print(CLogFile::flagInfo,"  esce open.");

	if(retVal>=0)
		enabled=TRUE;

  return retVal;
	}

void CMP3Audio::setRate(int r) {

	if(r>0 && r<=320000) // bah sì :)
		rate=r;
	}

void CMP3Audio::setChannels(BYTE c) {
  
	if(c>0 && c<8)
		channels=c;
	}

#if 0
int CMP3Audio::play(const BYTE *src,int size) {
  int i;
	DWORD ti;

		int size2=0;
		short int *src2=playBuffer+(playBufferBytes/2);
		short int *src3=(short int *)src;
		short int *src4=(short int *)src;
		// 16bit stereo... occorre personalizzare in base a formato out??
//    if(Param.force8bit) {
	{
		double pitchCnt=0.0;
		for(i=0; i<size/4; i++) {
			if(m_parent->Param.pitch==1.0) { 
				*src2++=*src3++;
				*src2++=*src3++;
				playBufferBytes+=4;
				}
			else if(m_parent->Param.pitch<1.0) { 
				pitchCnt+=m_parent->Param.pitch; 
				*src2++=*src3++;
				*src2++=*src3++;
				playBufferBytes+=4;
				if(playBufferBytes>=AUDIO_BUF_SIZE) {
					play();
					src2=playBuffer;
					}
				if(pitchCnt>=1.0) {
					pitchCnt-=1.0; 
					}
				else { 
					*(src2+0)=*(src2-2);
					*(src2+1)=*(src2-1);
					src2+=2;
					// si potrebbe fare una media col campione precedente o boh... [c'è un po' di artifatti]
					playBufferBytes+=4;
					}
				}
			else if(m_parent->Param.pitch>1.0) { 
				pitchCnt+=(m_parent->Param.pitch-1.0); 
				if(pitchCnt>=1.0) {
					pitchCnt-=1.0; 
					src3++;
					src3++;
					}
				else { 
					*src2++=*src3++;
					*src2++=*src3++;
					playBufferBytes+=4;
					} 
				}
			if(playBufferBytes>=AUDIO_BUF_SIZE) {
				play();
				src2=playBuffer;
				}
			}
		} 

	return playBufferBytes==0;		// tanto per..
	} 
#endif

int CMP3Audio::play(const BYTE *src,int size) {
	int i;

	if(!enabled)
		return 0;

	if(hWaveOut && hWaveOut!=(HWAVEOUT)-1) {
		critical=1;
//		EnterCriticalSection(&m_cs);
		switch(which & 3) {
			case 0:
				if(which) {
					while(!(OWaveHdr[0].dwFlags & WHDR_DONE)) {
						if(WaitForSingleObject(event,500) == WAIT_TIMEOUT)
							goto errore;
						}
					if(waveOutUnprepareHeader(hWaveOut,&OWaveHdr[0],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
						goto errore;
					}
				memcpy(buf[0],src,size);
				OWaveHdr[0].lpData=(char *)buf[0];
				OWaveHdr[0].dwBufferLength=size;
				OWaveHdr[0].dwFlags=0;
				OWaveHdr[0].dwLoops=0;
				OWaveHdr[0].dwUser=1;
				OWaveHdr[0].lpNext=0;
				OWaveHdr[0].reserved=0;
				if(waveOutPrepareHeader(hWaveOut,&OWaveHdr[0],sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
//				ti=timeGetTime()+500;
//				while(!(OWaveHdr[0].dwFlags & WHDR_PREPARED) && ti>timeGetTime());
					waveOutWrite(hWaveOut,&OWaveHdr[0],sizeof(WAVEHDR));
					}
				else
					goto errore;
				break;
			case 1:
				if(which >= 2) {
					while(!(OWaveHdr[1].dwFlags & WHDR_DONE)) {
						if(WaitForSingleObject(event,500) == WAIT_TIMEOUT)
							goto errore;
						}
					if(waveOutUnprepareHeader(hWaveOut,&OWaveHdr[1],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
						goto errore;
					}
				memcpy(buf[1],src,size);
				OWaveHdr[1].lpData=(char *)buf[1];
				OWaveHdr[1].dwBufferLength=size;
				OWaveHdr[1].dwFlags=0;
				OWaveHdr[1].dwLoops=0;
				OWaveHdr[1].dwUser=1;
				OWaveHdr[1].lpNext=0;
				OWaveHdr[1].reserved=0;
				if(waveOutPrepareHeader(hWaveOut,&OWaveHdr[1],sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
//				ti=timeGetTime()+500;
//				while(!(OWaveHdr[1].dwFlags & WHDR_PREPARED) && ti>timeGetTime());
					waveOutWrite(hWaveOut,&OWaveHdr[1],sizeof(WAVEHDR));
					}
				else
					goto errore;
				break;
			case 2:
				if(which >= 3) {
					while(!(OWaveHdr[2].dwFlags & WHDR_DONE)) {
						if(WaitForSingleObject(event,500) == WAIT_TIMEOUT)
							goto errore;
						}
					if(waveOutUnprepareHeader(hWaveOut,&OWaveHdr[2],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
						goto errore;
					}
				memcpy(buf[2],src,size);
				OWaveHdr[2].lpData=(char *)buf[2];
				OWaveHdr[2].dwBufferLength=size;
				OWaveHdr[2].dwFlags=0;
				OWaveHdr[2].dwLoops=0;
				OWaveHdr[2].dwUser=1;
				OWaveHdr[2].lpNext=0;
				OWaveHdr[2].reserved=0;
				if(waveOutPrepareHeader(hWaveOut,&OWaveHdr[2],sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
//				ti=timeGetTime()+500;
//				while(!(OWaveHdr[2].dwFlags & WHDR_PREPARED) && ti>timeGetTime());
					waveOutWrite(hWaveOut,&OWaveHdr[2],sizeof(WAVEHDR));
					}
				else
					goto errore;
				break;
			case 3:
				if(which >= 4) {
					while(!(OWaveHdr[3].dwFlags & WHDR_DONE)) {
						if(WaitForSingleObject(event,500) == WAIT_TIMEOUT)
							goto errore;
						}
					if(waveOutUnprepareHeader(hWaveOut,&OWaveHdr[3],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
						goto errore;
					}
				memcpy(buf[3],src,size);
				OWaveHdr[3].lpData=(char *)buf[3];
				OWaveHdr[3].dwBufferLength=size;
				OWaveHdr[3].dwFlags=0;
				OWaveHdr[3].dwLoops=0;
				OWaveHdr[3].dwUser=1;
				OWaveHdr[3].lpNext=0;
				OWaveHdr[3].reserved=0;
				if(waveOutPrepareHeader(hWaveOut,&OWaveHdr[3],sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
//				ti=timeGetTime()+500;
//				while(!(OWaveHdr[3].dwFlags & WHDR_PREPARED) && ti>timeGetTime());
					waveOutWrite(hWaveOut,&OWaveHdr[3],sizeof(WAVEHDR));
					}
				else
					goto errore;
				break;
			}
		which++;
errore:
//		LeaveCriticalSection(&m_cs);
		critical=0;
		}			// if hWaveOut

	else if(m_pSoundPlayer) {
		critical=1;
//		EnterCriticalSection(&m_cs);

		if(which >= m_nbSoundPlayerEvents) {
			if(WaitForSingleObject(m_pSoundPlayer->m_pReadEvent[which % m_nbSoundPlayerEvents],500 /*INFINITE qua incricca il goto...*/) == WAIT_TIMEOUT)
/*				goto errore 2021*/
				;
			}

		m_pSamples=(short int*)src;
		m_soundPlayerEventSize=size;

		m_pSoundPlayer->Write(((which) % m_nbSoundPlayerEvents)*m_soundPlayerEventSize,
			(BYTE *)m_pSamples, m_soundPlayerEventSize);

		CVidsendView22 *v=(CVidsendView22*)m_parent->tag;
		if(v) {
			i=v->measureAudio((short int *)m_pSamples,m_soundPlayerEventSize/2);
	//		v->VUMeter1->SetWindowText(i/500);
			v->mp3NotifyCallback(m_parent,1,i,0);
			}

		if(!which)
			m_pSoundPlayer->Play(1); // (loop) playing

		which++;
//		LeaveCriticalSection(&m_cs);
		critical=0;
		}
	
	return 1;
	}


#if 0
PlayerNotifyWnd::PlayerNotifyWnd(CMP3Audio *p) : m_Parent(p) {

	}

void PlayerNotifyWnd::OnSoundPlayerNotify(int eventNumber) {
	ScopeGuardMutex guard(m_Parent->m_exprMutex);		dovuto a STATIC??

//	m_Parent->m_pSoundPlayer->Write(((eventNumber+1) % m_Parent->m_nbSoundPlayerEvents)*
//		m_Parent->m_soundPlayerEventSize, (BYTE *)m_Parent->m_pSamples, m_Parent->m_soundPlayerEventSize);
//	m_Parent->inWaitSample=0;
	}
#endif


//----------------------------------------------------------------
int CJoshuaMP3::playSamples(const BYTE *src,int size) {

	switch(Param.outmode) {
		case DECODE_BOTH:
			if(bufSize+size>=44100L*2*2) {
				memcpy(bufPtr+bufSize,src,(44100L*2*2-bufSize));
				memcpy(bufPtr,src+(44100L*2*2-bufSize),size-(44100L*2*2-bufSize));
				bufSize=size-(44100L*2*2-bufSize);
				}
			else {
				memcpy(bufPtr+bufSize,src,size);
				bufSize+=size;
				}
		case DECODE_AUDIO:
			ai->play(src,size);
			if(ai2)
				ai2->play(src,size);
			break;
		case DECODE_STDOUT:		// anche un file generico!
		case DECODE_BUFFER:		// vedere se fare come sopra...
			memcpy(bufPtr+bufSize,src,size);
			bufSize+=size;
			break;
    case DECODE_WAV:
    case DECODE_CDR:
    case DECODE_AU:
//      wav_write(pcm_sample, pcm_point);
      break;
		}

	return 1;
	}

void CJoshuaMP3::Flush() {

#ifdef GAPLESS
	if(param.gapless) 
		layer3_gapless_buffercheck();
#endif
  if(pcmPoint) {
		switch(Param.outmode) {
			case DECODE_BOTH:
				if(bufSize+pcmPoint>=44100L*2*2) {
					memcpy(bufPtr+bufSize,pcmSample,(44100L*2*2-bufSize));
					memcpy(bufPtr,pcmSample+(44100L*2*2-bufSize),pcmPoint-(44100L*2*2-bufSize));
					bufSize=pcmPoint-(44100L*2*2-bufSize);
					}
				else {
					memcpy(bufPtr+bufSize,pcmSample,pcmPoint);
					bufSize+=pcmPoint;
					}
			case DECODE_AUDIO:
				ai->play(pcmSample,pcmPoint);
				if(ai2)
					ai2->play(pcmSample,pcmPoint);
				break;
			case DECODE_BUFFER:		// vedere se fare come sopra...
				memcpy(bufPtr+bufSize,pcmSample,pcmPoint);
				bufSize+=pcmPoint;
				break;
/*			case DECODE_FILE:
				write (OutputDescriptor, pcmSample, pcmPoint);
				break;
			case DECODE_WAV:
			case DECODE_CDR:
			case DECODE_AU:
				wav_write(pcmSample, pcmPoint);
				break;*/
			}
    pcmPoint = 0;
		}
	}

BYTE CJoshuaMP3::getBufferSafePos(DWORD qty,DWORD *pos1,DWORD *pos2) {

	if(bufPtr) {
		*pos1=bufSize;
		if(bufSize+qty < 44100L*2*2) {
			if(pos2)
				*pos2=0;
			}
		else {
			if(pos2)
				*pos2=bufSize+qty-44100L*2*2;
			}
		return 1;
		}
	else
		return 0;
	}
BYTE CJoshuaMP3::getBufferSafePos(DWORD qty,BYTE **ptr1,DWORD *pos1,BYTE **ptr2,DWORD *pos2) {

	if(bufPtr) {
		*ptr1=bufPtr+bufSize;
		*pos1=bufSize;
		if(bufSize+qty < 44100L*2*2) {
			if(ptr2)
				*ptr2=NULL;
			if(pos2)
				*pos2=0;
			}
		else {
			if(ptr2)
				*ptr2=bufPtr+bufSize+qty-44100L*2*2;
			if(pos2)
				*pos2=bufSize+qty-44100L*2*2;
			}
		return 1;
		}
	else
		return 0;
	}


void CMP3Audio::flush() {
	MSG msg;
	int i;

	if(hWaveOut && hWaveOut!=(HWAVEOUT)-1) {
		while(!(OWaveHdr[0].dwFlags & WHDR_DONE) ||
			!(OWaveHdr[1].dwFlags & WHDR_DONE) ||
			!(OWaveHdr[2].dwFlags & WHDR_DONE) ||
			!(OWaveHdr[3].dwFlags & WHDR_DONE) ) {
			if(PeekMessage(&msg,NULL,0,0,PM_REMOVE /*| PM_NOYIELD*/)) {
				TranslateMessage(&msg); 	 // Translates virtual key codes
				DispatchMessage(&msg);		 // Dispatches message to window
				}
			}
		}
	else if(m_pSoundPlayer) {
		for(i=0; i<m_nbSoundPlayerEvents; i++) {
			WaitForSingleObject(m_pSoundPlayer->m_pReadEvent[i],500 /*INFINITE NO !*/);

//		while(m_pSoundPlayer && m_pSoundPlayer->IsPlaying()) {
			if(PeekMessage(&msg,NULL,0,0,PM_REMOVE /*| PM_NOYIELD*/)) {
				TranslateMessage(&msg); 	 // Translates virtual key codes
				DispatchMessage(&msg);		 // Dispatches message to window
				}
			}
		}
	which=0;
	if(m_pSoundPlayer)
		m_pSoundPlayer->Stop();
	}

int CMP3Audio::close(BYTE m) {
	int i;
	HWAVEOUT hw=hWaveOut;

	if(!m) {		// 0=chiusura dolce, aspetto svuotamento buffer
		flush();
		}

//				debug_print(CLogFile::flagInfo,"  entra close.");

	if(hWaveOut && hWaveOut!=(HWAVEOUT)-1) {

		while(critical);	 // esco dal play buffer...
	//  DeleteCriticalSection(&cs);
		hWaveOut=NULL;		 // impedisco che ci rientri!
		if(hw) {
			i=waveOutReset(hw);		 // libero la periferica...
			i=waveOutUnprepareHeader(hw,&OWaveHdr[0],sizeof(WAVEHDR));	// servono davvero?
			i=waveOutUnprepareHeader(hw,&OWaveHdr[1],sizeof(WAVEHDR));
			i=waveOutUnprepareHeader(hw,&OWaveHdr[2],sizeof(WAVEHDR));
			i=waveOutUnprepareHeader(hw,&OWaveHdr[3],sizeof(WAVEHDR));
			if(!reserved)			 // settato se la periferica arriva da altrove (es. modem)
				i=waveOutClose(hw);
			}
		}
	else {
//		while(critical);	 // esco dal play buffer... NO, qui no...!
		if(m_pSoundPlayer)
			m_pSoundPlayer->Stop();
//				debug_print(CLogFile::flagInfo,"  stop/delete Player.");
		delete m_pSoundPlayer;	m_pSoundPlayer=NULL;
		if(m_exprMutex)
			delete m_exprMutex;		m_exprMutex=NULL;
		}

	rate=-1;
	channels=-1;

	if(event)
		CloseHandle(event);
	event=NULL;
	which=critical=0;
// si schianta come invalid handle... 2023 	DeleteCriticalSection(&m_cs);
	for(i=0; i<TOT_BUFFERS; i++) {
		if(buf[i])
			HeapFree(GetProcessHeap(),0,buf[i]);	 // e i buffer
		buf[i]=NULL;
		}

//					debug_print(CLogFile::flagInfo,"  esce close.");

  return 0;
	}



// -------------------------------------------------------------------------------------------------

#ifdef USE_3DNOW
//real CJoshuaMP3::decwin[2*(512+32)];
#else
//real CJoshuaMP3::decwin[512+32];
#endif
real CJoshuaMP3::cos64[16],CJoshuaMP3::cos32[8],CJoshuaMP3::cos16[4],CJoshuaMP3::cos8[2],CJoshuaMP3::cos4[1];
real *CJoshuaMP3::pnts[] = { cos64,cos32,cos16,cos8,cos4 };


const long CJoshuaMP3::intwinbase[] = {
     0,    -1,    -1,    -1,    -1,    -1,    -1,    -2,    -2,    -2,
    -2,    -3,    -3,    -4,    -4,    -5,    -5,    -6,    -7,    -7,
    -8,    -9,   -10,   -11,   -13,   -14,   -16,   -17,   -19,   -21,
   -24,   -26,   -29,   -31,   -35,   -38,   -41,   -45,   -49,   -53,
   -58,   -63,   -68,   -73,   -79,   -85,   -91,   -97,  -104,  -111,
  -117,  -125,  -132,  -139,  -147,  -154,  -161,  -169,  -176,  -183,
  -190,  -196,  -202,  -208,  -213,  -218,  -222,  -225,  -227,  -228,
  -228,  -227,  -224,  -221,  -215,  -208,  -200,  -189,  -177,  -163,
  -146,  -127,  -106,   -83,   -57,   -29,     2,    36,    72,   111,
   153,   197,   244,   294,   347,   401,   459,   519,   581,   645,
   711,   779,   848,   919,   991,  1064,  1137,  1210,  1283,  1356,
  1428,  1498,  1567,  1634,  1698,  1759,  1817,  1870,  1919,  1962,
  2001,  2032,  2057,  2075,  2085,  2087,  2080,  2063,  2037,  2000,
  1952,  1893,  1822,  1739,  1644,  1535,  1414,  1280,  1131,   970,
   794,   605,   402,   185,   -45,  -288,  -545,  -814, -1095, -1388,
 -1692, -2006, -2330, -2663, -3004, -3351, -3705, -4063, -4425, -4788,
 -5153, -5517, -5879, -6237, -6589, -6935, -7271, -7597, -7910, -8209,
 -8491, -8755, -8998, -9219, -9416, -9585, -9727, -9838, -9916, -9959,
 -9966, -9935, -9863, -9750, -9592, -9389, -9139, -8840, -8492, -8092,
 -7640, -7134, -6574, -5959, -5288, -4561, -3776, -2935, -2037, -1082,
   -70,   998,  2122,  3300,  4533,  5818,  7154,  8540,  9975, 11455,
 12980, 14548, 16155, 17799, 19478, 21189, 22929, 24694, 26482, 28289,
 30112, 31947, 33791, 35640, 37489, 39336, 41176, 43006, 44821, 46617,
 48390, 50137, 51853, 53534, 55178, 56778, 58333, 59838, 61289, 62684,
 64019, 65290, 66494, 67629, 68692, 69679, 70590, 71420, 72169, 72835,
 73415, 73908, 74313, 74630, 74856, 74992, 75038 };



/* 
 * Mpeg Layer-1,2,3 audio decoder 
 * ------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp, All rights reserved.
 * See also 'README'
 * slighlty optimized for machines without autoincrement/decrement
 *
 */

/*
 * Discrete Cosine Tansform (DCT) for subband synthesis
 * optimized for machines with no auto-increment. 
 * The performance is highly compiler dependend. Maybe
 * the dct64.c version for 'normal' processor may be faster
 * even for Intel processors.
 */

#define DCT_OPTIMIZED_386	 

#ifdef DCT_OPTIMIZED_386	 

void CJoshuaMP3::dct64(real *out0,real *out1,const real *samples) {
	real b1[0x20],b2[0x20];

	{
  register real *costab = pnts[0];

  b1[0x00] = samples[0x00] + samples[0x1F];
  b1[0x1F] = (samples[0x00] - samples[0x1F]) * costab[0x0];

  b1[0x01] = samples[0x01] + samples[0x1E];
  b1[0x1E] = (samples[0x01] - samples[0x1E]) * costab[0x1];

  b1[0x02] = samples[0x02] + samples[0x1D];
  b1[0x1D] = (samples[0x02] - samples[0x1D]) * costab[0x2];

  b1[0x03] = samples[0x03] + samples[0x1C];
  b1[0x1C] = (samples[0x03] - samples[0x1C]) * costab[0x3];

  b1[0x04] = samples[0x04] + samples[0x1B];
  b1[0x1B] = (samples[0x04] - samples[0x1B]) * costab[0x4];

  b1[0x05] = samples[0x05] + samples[0x1A];
  b1[0x1A] = (samples[0x05] - samples[0x1A]) * costab[0x5];

  b1[0x06] = samples[0x06] + samples[0x19];
  b1[0x19] = (samples[0x06] - samples[0x19]) * costab[0x6];

  b1[0x07] = samples[0x07] + samples[0x18];
  b1[0x18] = (samples[0x07] - samples[0x18]) * costab[0x7];

  b1[0x08] = samples[0x08] + samples[0x17];
  b1[0x17] = (samples[0x08] - samples[0x17]) * costab[0x8];

  b1[0x09] = samples[0x09] + samples[0x16];
  b1[0x16] = (samples[0x09] - samples[0x16]) * costab[0x9];

  b1[0x0A] = samples[0x0A] + samples[0x15];
  b1[0x15] = (samples[0x0A] - samples[0x15]) * costab[0xA];

  b1[0x0B] = samples[0x0B] + samples[0x14];
  b1[0x14] = (samples[0x0B] - samples[0x14]) * costab[0xB];

  b1[0x0C] = samples[0x0C] + samples[0x13];
  b1[0x13] = (samples[0x0C] - samples[0x13]) * costab[0xC];

  b1[0x0D] = samples[0x0D] + samples[0x12];
  b1[0x12] = (samples[0x0D] - samples[0x12]) * costab[0xD];

  b1[0x0E] = samples[0x0E] + samples[0x11];
  b1[0x11] = (samples[0x0E] - samples[0x11]) * costab[0xE];

  b1[0x0F] = samples[0x0F] + samples[0x10];
  b1[0x10] = (samples[0x0F] - samples[0x10]) * costab[0xF];
	}


	{
  register real *costab = pnts[1];

  b2[0x00] = b1[0x00] + b1[0x0F]; 
  b2[0x0F] = (b1[0x00] - b1[0x0F]) * costab[0];
  b2[0x01] = b1[0x01] + b1[0x0E]; 
  b2[0x0E] = (b1[0x01] - b1[0x0E]) * costab[1];
  b2[0x02] = b1[0x02] + b1[0x0D]; 
  b2[0x0D] = (b1[0x02] - b1[0x0D]) * costab[2];
  b2[0x03] = b1[0x03] + b1[0x0C]; 
  b2[0x0C] = (b1[0x03] - b1[0x0C]) * costab[3];
  b2[0x04] = b1[0x04] + b1[0x0B]; 
  b2[0x0B] = (b1[0x04] - b1[0x0B]) * costab[4];
  b2[0x05] = b1[0x05] + b1[0x0A]; 
  b2[0x0A] = (b1[0x05] - b1[0x0A]) * costab[5];
  b2[0x06] = b1[0x06] + b1[0x09]; 
  b2[0x09] = (b1[0x06] - b1[0x09]) * costab[6];
  b2[0x07] = b1[0x07] + b1[0x08]; 
  b2[0x08] = (b1[0x07] - b1[0x08]) * costab[7];

  b2[0x10] = b1[0x10] + b1[0x1F];
  b2[0x1F] = (b1[0x1F] - b1[0x10]) * costab[0];
  b2[0x11] = b1[0x11] + b1[0x1E];
  b2[0x1E] = (b1[0x1E] - b1[0x11]) * costab[1];
  b2[0x12] = b1[0x12] + b1[0x1D];
  b2[0x1D] = (b1[0x1D] - b1[0x12]) * costab[2];
  b2[0x13] = b1[0x13] + b1[0x1C];
  b2[0x1C] = (b1[0x1C] - b1[0x13]) * costab[3];
  b2[0x14] = b1[0x14] + b1[0x1B];
  b2[0x1B] = (b1[0x1B] - b1[0x14]) * costab[4];
  b2[0x15] = b1[0x15] + b1[0x1A];
  b2[0x1A] = (b1[0x1A] - b1[0x15]) * costab[5];
  b2[0x16] = b1[0x16] + b1[0x19];
  b2[0x19] = (b1[0x19] - b1[0x16]) * costab[6];
  b2[0x17] = b1[0x17] + b1[0x18];
  b2[0x18] = (b1[0x18] - b1[0x17]) * costab[7];
	}

	{
  register real *costab = pnts[2];

  b1[0x00] = b2[0x00] + b2[0x07];
  b1[0x07] = (b2[0x00] - b2[0x07]) * costab[0];
  b1[0x01] = b2[0x01] + b2[0x06];
  b1[0x06] = (b2[0x01] - b2[0x06]) * costab[1];
  b1[0x02] = b2[0x02] + b2[0x05];
  b1[0x05] = (b2[0x02] - b2[0x05]) * costab[2];
  b1[0x03] = b2[0x03] + b2[0x04];
  b1[0x04] = (b2[0x03] - b2[0x04]) * costab[3];

  b1[0x08] = b2[0x08] + b2[0x0F];
  b1[0x0F] = (b2[0x0F] - b2[0x08]) * costab[0];
  b1[0x09] = b2[0x09] + b2[0x0E];
  b1[0x0E] = (b2[0x0E] - b2[0x09]) * costab[1];
  b1[0x0A] = b2[0x0A] + b2[0x0D];
  b1[0x0D] = (b2[0x0D] - b2[0x0A]) * costab[2];
  b1[0x0B] = b2[0x0B] + b2[0x0C];
  b1[0x0C] = (b2[0x0C] - b2[0x0B]) * costab[3];

  b1[0x10] = b2[0x10] + b2[0x17];
  b1[0x17] = (b2[0x10] - b2[0x17]) * costab[0];
  b1[0x11] = b2[0x11] + b2[0x16];
  b1[0x16] = (b2[0x11] - b2[0x16]) * costab[1];
  b1[0x12] = b2[0x12] + b2[0x15];
  b1[0x15] = (b2[0x12] - b2[0x15]) * costab[2];
  b1[0x13] = b2[0x13] + b2[0x14];
  b1[0x14] = (b2[0x13] - b2[0x14]) * costab[3];

  b1[0x18] = b2[0x18] + b2[0x1F];
  b1[0x1F] = (b2[0x1F] - b2[0x18]) * costab[0];
  b1[0x19] = b2[0x19] + b2[0x1E];
  b1[0x1E] = (b2[0x1E] - b2[0x19]) * costab[1];
  b1[0x1A] = b2[0x1A] + b2[0x1D];
  b1[0x1D] = (b2[0x1D] - b2[0x1A]) * costab[2];
  b1[0x1B] = b2[0x1B] + b2[0x1C];
  b1[0x1C] = (b2[0x1C] - b2[0x1B]) * costab[3];
	}

	{
  register real const cos0 = pnts[3][0];
  register real const cos1 = pnts[3][1];

  b2[0x00] = b1[0x00] + b1[0x03];
  b2[0x03] = (b1[0x00] - b1[0x03]) * cos0;
  b2[0x01] = b1[0x01] + b1[0x02];
  b2[0x02] = (b1[0x01] - b1[0x02]) * cos1;

  b2[0x04] = b1[0x04] + b1[0x07];
  b2[0x07] = (b1[0x07] - b1[0x04]) * cos0;
  b2[0x05] = b1[0x05] + b1[0x06];
  b2[0x06] = (b1[0x06] - b1[0x05]) * cos1;

  b2[0x08] = b1[0x08] + b1[0x0B];
  b2[0x0B] = (b1[0x08] - b1[0x0B]) * cos0;
  b2[0x09] = b1[0x09] + b1[0x0A];
  b2[0x0A] = (b1[0x09] - b1[0x0A]) * cos1;
  
  b2[0x0C] = b1[0x0C] + b1[0x0F];
  b2[0x0F] = (b1[0x0F] - b1[0x0C]) * cos0;
  b2[0x0D] = b1[0x0D] + b1[0x0E];
  b2[0x0E] = (b1[0x0E] - b1[0x0D]) * cos1;

  b2[0x10] = b1[0x10] + b1[0x13];
  b2[0x13] = (b1[0x10] - b1[0x13]) * cos0;
  b2[0x11] = b1[0x11] + b1[0x12];
  b2[0x12] = (b1[0x11] - b1[0x12]) * cos1;

  b2[0x14] = b1[0x14] + b1[0x17];
  b2[0x17] = (b1[0x17] - b1[0x14]) * cos0;
  b2[0x15] = b1[0x15] + b1[0x16];
  b2[0x16] = (b1[0x16] - b1[0x15]) * cos1;

  b2[0x18] = b1[0x18] + b1[0x1B];
  b2[0x1B] = (b1[0x18] - b1[0x1B]) * cos0;
  b2[0x19] = b1[0x19] + b1[0x1A];
  b2[0x1A] = (b1[0x19] - b1[0x1A]) * cos1;

  b2[0x1C] = b1[0x1C] + b1[0x1F];
  b2[0x1F] = (b1[0x1F] - b1[0x1C]) * cos0;
  b2[0x1D] = b1[0x1D] + b1[0x1E];
  b2[0x1E] = (b1[0x1E] - b1[0x1D]) * cos1;
	}

	{
  register real const cos0 = pnts[4][0];

  b1[0x00] = b2[0x00] + b2[0x01];
  b1[0x01] = (b2[0x00] - b2[0x01]) * cos0;
  b1[0x02] = b2[0x02] + b2[0x03];
  b1[0x03] = (b2[0x03] - b2[0x02]) * cos0;
  b1[0x02] += b1[0x03];

  b1[0x04] = b2[0x04] + b2[0x05];
  b1[0x05] = (b2[0x04] - b2[0x05]) * cos0;
  b1[0x06] = b2[0x06] + b2[0x07];
  b1[0x07] = (b2[0x07] - b2[0x06]) * cos0;
  b1[0x06] += b1[0x07];
  b1[0x04] += b1[0x06];
  b1[0x06] += b1[0x05];
  b1[0x05] += b1[0x07];

  b1[0x08] = b2[0x08] + b2[0x09];
  b1[0x09] = (b2[0x08] - b2[0x09]) * cos0;
  b1[0x0A] = b2[0x0A] + b2[0x0B];
  b1[0x0B] = (b2[0x0B] - b2[0x0A]) * cos0;
  b1[0x0A] += b1[0x0B];

  b1[0x0C] = b2[0x0C] + b2[0x0D];
  b1[0x0D] = (b2[0x0C] - b2[0x0D]) * cos0;
  b1[0x0E] = b2[0x0E] + b2[0x0F];
  b1[0x0F] = (b2[0x0F] - b2[0x0E]) * cos0;
  b1[0x0E] += b1[0x0F];
  b1[0x0C] += b1[0x0E];
  b1[0x0E] += b1[0x0D];
  b1[0x0D] += b1[0x0F];

  b1[0x10] = b2[0x10] + b2[0x11];
  b1[0x11] = (b2[0x10] - b2[0x11]) * cos0;
  b1[0x12] = b2[0x12] + b2[0x13];
  b1[0x13] = (b2[0x13] - b2[0x12]) * cos0;
  b1[0x12] += b1[0x13];

  b1[0x14] = b2[0x14] + b2[0x15];
  b1[0x15] = (b2[0x14] - b2[0x15]) * cos0;
  b1[0x16] = b2[0x16] + b2[0x17];
  b1[0x17] = (b2[0x17] - b2[0x16]) * cos0;
  b1[0x16] += b1[0x17];
  b1[0x14] += b1[0x16];
  b1[0x16] += b1[0x15];
  b1[0x15] += b1[0x17];

  b1[0x18] = b2[0x18] + b2[0x19];
  b1[0x19] = (b2[0x18] - b2[0x19]) * cos0;
  b1[0x1A] = b2[0x1A] + b2[0x1B];
  b1[0x1B] = (b2[0x1B] - b2[0x1A]) * cos0;
  b1[0x1A] += b1[0x1B];

  b1[0x1C] = b2[0x1C] + b2[0x1D];
  b1[0x1D] = (b2[0x1C] - b2[0x1D]) * cos0;
  b1[0x1E] = b2[0x1E] + b2[0x1F];
  b1[0x1F] = (b2[0x1F] - b2[0x1E]) * cos0;
  b1[0x1E] += b1[0x1F];
  b1[0x1C] += b1[0x1E];
  b1[0x1E] += b1[0x1D];
  b1[0x1D] += b1[0x1F];
	}

	out0[0x10*16] = b1[0x00];
	out0[0x10*12] = b1[0x04];
	out0[0x10* 8] = b1[0x02];
	out0[0x10* 4] = b1[0x06];
	out0[0x10* 0] = b1[0x01];
	out1[0x10* 0] = b1[0x01];
	out1[0x10* 4] = b1[0x05];
	out1[0x10* 8] = b1[0x03];
	out1[0x10*12] = b1[0x07];

	b1[0x08] += b1[0x0C];
	out0[0x10*14] = b1[0x08];
	b1[0x0C] += b1[0x0a];
	out0[0x10*10] = b1[0x0C];
	b1[0x0A] += b1[0x0E];
	out0[0x10* 6] = b1[0x0A];
	b1[0x0E] += b1[0x09];
	out0[0x10* 2] = b1[0x0E];
	b1[0x09] += b1[0x0D];
	out1[0x10* 2] = b1[0x09];
	b1[0x0D] += b1[0x0B];
	out1[0x10* 6] = b1[0x0D];
	b1[0x0B] += b1[0x0F];
	out1[0x10*10] = b1[0x0B];
	out1[0x10*14] = b1[0x0F];

	b1[0x18] += b1[0x1C];
	out0[0x10*15] = b1[0x10] + b1[0x18];
	out0[0x10*13] = b1[0x18] + b1[0x14];
	b1[0x1C] += b1[0x1a];
	out0[0x10*11] = b1[0x14] + b1[0x1C];
	out0[0x10* 9] = b1[0x1C] + b1[0x12];
	b1[0x1A] += b1[0x1E];
	out0[0x10* 7] = b1[0x12] + b1[0x1A];
	out0[0x10* 5] = b1[0x1A] + b1[0x16];
	b1[0x1E] += b1[0x19];
	out0[0x10* 3] = b1[0x16] + b1[0x1E];
	out0[0x10* 1] = b1[0x1E] + b1[0x11];
	b1[0x19] += b1[0x1D];
	out1[0x10* 1] = b1[0x11] + b1[0x19];
	out1[0x10* 3] = b1[0x19] + b1[0x15];
	b1[0x1D] += b1[0x1B];
	out1[0x10* 5] = b1[0x15] + b1[0x1D];
	out1[0x10* 7] = b1[0x1D] + b1[0x13];
	b1[0x1B] += b1[0x1F];
	out1[0x10* 9] = b1[0x13] + b1[0x1B];
	out1[0x10*11] = b1[0x1B] + b1[0x17];
	out1[0x10*13] = b1[0x17] + b1[0x1F];
	out1[0x10*15] = b1[0x1F];
	}

#else

void CJoshuaMP3::dct64(real *out0,real *out1,const real *samples) {
  real bufs[64];

 {
  register int i,j;
  register real *b1,*b2,*bs,*costab;

  b1 = (real *)samples;
  bs = bufs;
  costab = pnts[0]+16;
  b2 = b1 + 32;

  for(i=15;i>=0;i--)
    *bs++ = (*b1++ + *--b2); 
  for(i=15;i>=0;i--)
    *bs++ = REAL_MUL((*--b2 - *b1++), *--costab);

  b1 = bufs;
  costab = pnts[1]+8;
  b2 = b1 + 16;

  {
    for(i=7;i>=0;i--)
      *bs++ = (*b1++ + *--b2); 
    for(i=7;i>=0;i--)
      *bs++ = REAL_MUL((*--b2 - *b1++), *--costab);
    b2 += 32;
    costab += 8;
    for(i=7;i>=0;i--)
      *bs++ = (*b1++ + *--b2); 
    for(i=7;i>=0;i--)
      *bs++ = REAL_MUL((*b1++ - *--b2), *--costab);
    b2 += 32;
  }

  bs = bufs;
  costab = pnts[2];
  b2 = b1 + 8;

  for(j=2;j;j--) {
    for(i=3;i>=0;i--)
      *bs++ = (*b1++ + *--b2); 
    for(i=3;i>=0;i--)
      *bs++ = REAL_MUL((*--b2 - *b1++), costab[i]);
    b2 += 16;
    for(i=3;i>=0;i--)
      *bs++ = (*b1++ + *--b2); 
    for(i=3;i>=0;i--)
      *bs++ = REAL_MUL((*b1++ - *--b2), costab[i]);
    b2 += 16;
  }

  b1 = bufs;
  costab = pnts[3];
  b2 = b1 + 4;

  for(j=4;j; j--) {
    *bs++ = (*b1++ + *--b2); 
    *bs++ = (*b1++ + *--b2);
    *bs++ = REAL_MUL((*--b2 - *b1++), costab[1]);
    *bs++ = REAL_MUL((*--b2 - *b1++), costab[0]);
    b2 += 8;
    *bs++ = (*b1++ + *--b2); 
    *bs++ = (*b1++ + *--b2);
    *bs++ = REAL_MUL((*b1++ - *--b2), costab[1]);
    *bs++ = REAL_MUL((*b1++ - *--b2), costab[0]);
    b2 += 8;
  }
  bs = bufs;
  costab = pnts[4];

  for(j=8; j; j--) {
    real v0,v1;
    v0=*b1++; v1 = *b1++;
    *bs++ = (v0 + v1);
    *bs++ = REAL_MUL((v0 - v1), (*costab));
    v0=*b1++; v1 = *b1++;
    *bs++ = (v0 + v1);
    *bs++ = REAL_MUL((v1 - v0), (*costab));
  }

 }


 {
  register real *b1;
  register int i;

  for(b1=bufs,i=8;i;i--,b1+=4)
    b1[2] += b1[3];

  for(b1=bufs,i=4;i;i--,b1+=8) {
    b1[4] += b1[6];
    b1[6] += b1[5];
    b1[5] += b1[7];
  }

  for(b1=bufs,i=2;i;i--,b1+=16) {
    b1[8]  += b1[12];
    b1[12] += b1[10];
    b1[10] += b1[14];
    b1[14] += b1[9];
    b1[9]  += b1[13];
    b1[13] += b1[11];
    b1[11] += b1[15];
  }
 }


  out0[0x10*16] = bufs[0];
  out0[0x10*15] = bufs[16+0]  + bufs[16+8];
  out0[0x10*14] = bufs[8];
  out0[0x10*13] = bufs[16+8]  + bufs[16+4];
  out0[0x10*12] = bufs[4];
  out0[0x10*11] = bufs[16+4]  + bufs[16+12];
  out0[0x10*10] = bufs[12];
  out0[0x10* 9] = bufs[16+12] + bufs[16+2];
  out0[0x10* 8] = bufs[2];
  out0[0x10* 7] = bufs[16+2]  + bufs[16+10];
  out0[0x10* 6] = bufs[10];
  out0[0x10* 5] = bufs[16+10] + bufs[16+6];
  out0[0x10* 4] = bufs[6];
  out0[0x10* 3] = bufs[16+6]  + bufs[16+14];
  out0[0x10* 2] = bufs[14];
  out0[0x10* 1] = bufs[16+14] + bufs[16+1];
  out0[0x10* 0] = bufs[1];

  out1[0x10* 0] = bufs[1];
  out1[0x10* 1] = bufs[16+1]  + bufs[16+9];
  out1[0x10* 2] = bufs[9];
  out1[0x10* 3] = bufs[16+9]  + bufs[16+5];
  out1[0x10* 4] = bufs[5];
  out1[0x10* 5] = bufs[16+5]  + bufs[16+13];
  out1[0x10* 6] = bufs[13];
  out1[0x10* 7] = bufs[16+13] + bufs[16+3];
  out1[0x10* 8] = bufs[3];
  out1[0x10* 9] = bufs[16+3]  + bufs[16+11];
  out1[0x10*10] = bufs[11];
  out1[0x10*11] = bufs[16+11] + bufs[16+7];
  out1[0x10*12] = bufs[7];
  out1[0x10*13] = bufs[16+7]  + bufs[16+15];
  out1[0x10*14] = bufs[15];
  out1[0x10*15] = bufs[16+15];
	}

#endif


void CJoshuaMP3Equalizer::equalize(real *bandPtr, BYTE channel) {
	int i;
	real equalizer_sum[2][32];		// boh?
	int equalizer_cnt;		// boh?

	for(i=0; i<32; i++)
		bandPtr[i] = REAL_MUL(bandPtr[i], cursor[channel][i]);
//		bandPtr[i] *= cursor[channel][i];

/*	if(param.equalizer & 0x2) {
		
		for(i=0;i<32;i++)
			equalizer_sum[channel][i] += bandPtr[i];
	}
*/
	}

void CJoshuaMP3Equalizer::init(int m,double v) {
	register int i,j;
	real myBand[16];

	if(m==-1)
		m=oldEqualizeMode;

//m=passaAlto;


/*	# mpg123 eq
https://sourceforge.net/p/mpg123/bugs/327/ coglione ;)
# Rock
#
1 1
1 1
0.9 0.9
0.9 0.9
0.8 0.8
0.7 0.7
0.5 0.5
0.5 0.5
0.5 0.5
0.5 0.5
0.3 0.3
0.2 0.2
0.2 0.2
0.3 0.3
0.3 0.3
0.2 0.2
0.2 0.2
0.3 0.3
0.5 0.5
0.5 0.5
0.5 0.5
0.7 0.7
0.7 0.7
0.8 0.8
0.8 0.8
0.8 0.8
1 1
1 1
1 1
1 1
1 1
1 1*/

	switch(m) {
		default:		// non deve accadere!
		case passaFlat:
		case equal_flat:
			for(i=0; i<16; i++)
				myBand[i]=1.0;

//			myBand[3]=2;

/*			for(i=0; i<16; i++)
				myBand[i]=.2;
	non corrispondono alle frequenze...
			myBand[15]=1;*/
			break;
		case passaAlto:
			for(i=0; i<4; i++)
				myBand[i]=0.3;
			for(i; i<16; i++)
				myBand[i]=((i-4)/12.0)+0.3;
			break;
		case passaBasso:
			for(i=0; i<8; i++)
				myBand[i]=((8-i)/8.0)+0.3;
			for(i; i<16; i++)
				myBand[i]=0.3;
			break;
		case passaBanda:
			for(i=0; i<4; i++)
				myBand[i]=((4-i)/4.0)+0.3;
			for(i; i<6; i++)
				myBand[i]=0.3;
			for(i; i<16; i++)
				myBand[i]=((i-4)/12.0)+0.3;
			break;
		case equal_rock:
			for(i=0; i<6; i++)
				myBand[i]=((6-i)/18.0)+0.7;
			for(i=6; i<10; i++)
				myBand[i]=0.7 /*((8-abs(i-8))/8.0)+0.3*/;
			for(i=10; i<16; i++)
				myBand[i]=((i-10)/18.0)+0.7;
			break;
		case equal_pop:
			for(i=0; i<6; i++)
				myBand[i]=((i)/12.0)+0.5;
			for(i=6; i<10; i++)
				myBand[i]=1.0 /*((8-abs(i-8))/8.0)+0.3*/;
			for(i=10; i<16; i++)
				myBand[i]=((15-i)/12.0)+0.5;
			break;
		case equal_dance:
			for(i=0; i<6; i++)
				myBand[i]=((6-i)/12.0)+0.5;
			for(i=6; i<10; i++)
				myBand[i]=0.5 /*((8-abs(i-8))/8.0)+0.3*/;
			for(i=10; i<16; i++)
				myBand[i]=((i-10)/12.0)+0.5;
			break;
		case equal_classica:
			for(i=0; i<16; i++)
				myBand[i]=(8-abs(i-7))/2.0+0.3;
			break;
		}
	for(j=0; j<2; j++) {
		for(i=0; i<16; i++)
			cursor[j][i]=v*myBand[i];
		for(i=16; i<32; i++)
			cursor[j][i]=v*myBand[15];
		}
	}

void CJoshuaMP3Equalizer::doBand(real *bandPtr,BYTE channel) {
  int i;

  for(i=0; i<576; i++) {
//    bandPtr[i] *= band[channel][i];
    bandPtr[i] = REAL_MUL(bandPtr[i], band[channel][i]);
		}
	}

CJoshuaMP3Equalizer::CJoshuaMP3Equalizer() {

	oldEqualizeMode=passaFlat;		// passa
	}

int CJoshuaMP3::volume(double vl,double vr) {

//				debug_print(CLogFile::flagInfo,"volume sw=%f",vl);
	if(theEqualizer) {
		Param.forceVolume=vl;
// finire con canale l/r
		theEqualizer->init(-1,Param.forceVolume);
		return 1;
		}
	else
		return 0;
	}

int CJoshuaMP3::volume(signed char q,double vl,double vr) {

//				debug_print(CLogFile::flagInfo,"volume sw=%f",vl);
	if(theEqualizer) {
		Param.forceVolume=vl;
// finire con canale l/r ed ev. gestire periferiche separate qua..
		theEqualizer->init(-1,Param.forceVolume);
		return 1;
		}
	else
		return 0;
	}

int CJoshuaMP3::HWvolume(double vl,double vr) {

//				debug_print(CLogFile::flagInfo,"volume hw=%f",vl);

	if(ai && ai->m_pSoundPlayer) {
		ai->m_pSoundPlayer->SetVolume(vl,vr);
		return 1;
		}
	else
		return 0;
	}

int CJoshuaMP3::HWvolume(signed char q,double vl,double vr) {

	switch(q) {
		case -1:
			break;
		case 0:
		case 1:
			if(ai)
				ai->m_pSoundPlayer->SetVolume(vl,vr);
			break;
		case 2:
			if(ai2)
				ai2->m_pSoundPlayer->SetVolume(vl,vr);
			break;
		}

	return 0;
	}

int CJoshuaMP3::enableOutput(signed char q,BOOL bEnabled) {

	switch(q) {
		case -1:
			break;
		case 0:
		case 1:
			if(ai) {
				ai->enabled=bEnabled;
				if(bEnabled) {
					if(ai->m_pSoundPlayer)
//						meglio abbassare il volume, o si perde il sincro dei tempi con il buffer dati di DECODE...
						ai->m_pSoundPlayer->Play(1); // 
					}
				else {
					if(ai->m_pSoundPlayer)
					//	ai->m_pSoundPlayer->Stop();
						ai->flush();
					}
				}
			break;
		case 2:
			if(ai2) {
				ai2->enabled=bEnabled;
				if(bEnabled) {
					if(ai2->m_pSoundPlayer)
						ai2->m_pSoundPlayer->Play(1); // 
					}
				else {
					if(ai2->m_pSoundPlayer)
					//	ai2->m_pSoundPlayer->Stop();
					  ai2->flush();
					}
				}
			break;

		}

	return 0;
	}

int CJoshuaMP3::setPitch(double p) {

	if(p>0 && p<10) {
		Param.pitch=p;
		long n = Param.pitch*freqs[fr.samplingFrequency];
    long m = ai->rate;

		fr.downSample=3;		// forza n_to_m per pitch!
		setSynthFunctions();
		synth_ntom_setStep(n,m);
		InitLayer3(fr.downSampleSBlimit);
		return 1;
		}
	else
		return 0;
	}



#define WRITE_SAMPLE(samples,sum,clip) \
  if( (sum) > 32767.0) { *(samples) = 0x7fff; (clip)++; } \
  else if( (sum) < -32768.0) { *(samples) = -0x8000; (clip)++; } \
  else { *(samples) = sum; } \
	if( (sum) < (minSample) ) { minSample=(sum); } \
	if( (sum) > (maxSample) ) { maxSample=(sum); }

 /* new WRITE_SAMPLE */
// #define WRITE_SAMPLE(samples,sum,clip) { \
//  double dtemp; int v; /* sizeof(int) == 4 */ \
// dtemp = ((((65536.0 * 65536.0 * 16)+(65536.0 * 0.5))* 65536.0)) + (sum);  \
//  v = ((*(int *)&dtemp) - 0x80000000); \
//  if( v > 32767) { *(samples) = 0x7fff; (clip)++; } \
//  else if( v < -32768) { *(samples) = -0x8000; (clip)++; } \
//  else { *(samples) = v; }  \ 
	


int CJoshuaMP3::synth_1to1_8bit(const real *bandPtr,BYTE channel,BYTE *samples,int *pnt) {
  short samples_tmp[64];
  short *tmp1 = samples_tmp + channel;
  int i,ret;
  int pnt1=0;

  ret = synth_1to1(bandPtr,channel,(BYTE *)samples_tmp,&pnt1);
  samples += channel + *pnt;

  for(i=0; i<32; i++) {
    *samples = conv16to8[*tmp1 >> AUSHIFT];
    samples += 2;
    tmp1 += 2;
		}
  *pnt += 64;

  return ret;
	}

int CJoshuaMP3::synth_1to1_8bit_mono(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[64];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_1to1(bandPtr,0,(BYTE *)samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0; i<32; i++) {
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    tmp1 += 2;
		}
  *pnt += 32;

  return ret;
	}

int CJoshuaMP3::synth_1to1_8bit_mono2stereo(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[64];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_1to1(bandPtr,0,(BYTE *)samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0;i<32;i++) {
    *samples++ = conv16to8[*tmp1>>AUSHIFT];
    *samples++ = conv16to8[*tmp1>>AUSHIFT];
    tmp1 += 2;
		}
  *pnt += 64;

  return ret;
	}

int CJoshuaMP3::synth_1to1_mono(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[64];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_1to1(bandPtr,0,(BYTE *)samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0;i<32;i++) {
    *((short int *)samples) = *tmp1;
    samples += 2;
    tmp1 += 2;
		}
  *pnt += 64;

  return ret;
	}

int CJoshuaMP3::synth_1to1_mono2stereo(const real *bandPtr,BYTE *samples,int *pnt) {
  int i,ret;

  ret = synth_1to1(bandPtr,0,samples,pnt);
  samples = samples + *pnt - 128;

  for(i=0;i<32;i++) {
    ((short int *)samples)[1] = ((short int *)samples)[0];
    samples+=4;
		}

  return ret;
	}

int CJoshuaMP3::synth_1to1(const real *bandPtr,BYTE channel,BYTE *out,int *pnt) {
#ifndef NO_PENTIUM_OPT
//  /*static 2023*/ real buffs[2][2][0x110];
  short int *samples = (short int *)(out+*pnt);

  real *b0,(*buf)[0x110];
  int clip = 0; 
  int bo1;
#endif

	if(theEqualizer)
		theEqualizer->equalize((real *)bandPtr,channel);

#ifdef NO_PENTIUM_OPT
  if(!channel) {
    bo--;
    bo &= 0xf;
    buf = buffs[0];
	  }
  else {
    samples++;
    buf = buffs[1];
		}

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    dct64(buf[1]+((bo+1)&0xf),buf[0]+bo,bandPtr);
		}
  else {
    b0 = buf[1];
    bo1 = bo+1;
    dct64(buf[0]+bo,buf[1]+bo+1,bandPtr);
		}
  
  {
    register int j;
    real *window = decwin + 16 - bo1;

    for(j=16; j; j--,b0+=0x10,window+=0x20,samples+=step) {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum -= window[0x1] * b0[0x1];
      sum += window[0x2] * b0[0x2];
      sum -= window[0x3] * b0[0x3];
      sum += window[0x4] * b0[0x4];
      sum -= window[0x5] * b0[0x5];
      sum += window[0x6] * b0[0x6];
      sum -= window[0x7] * b0[0x7];
      sum += window[0x8] * b0[0x8];
      sum -= window[0x9] * b0[0x9];
      sum += window[0xA] * b0[0xA];
      sum -= window[0xB] * b0[0xB];
      sum += window[0xC] * b0[0xC];
      sum -= window[0xD] * b0[0xD];
      sum += window[0xE] * b0[0xE];
      sum -= window[0xF] * b0[0xF];

      WRITE_SAMPLE(samples,sum,clip);
			}

    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];
      WRITE_SAMPLE(samples,sum,clip);
      b0-=0x10,window-=0x20,samples+=step;
			}
    window += bo1<<1;

    for (j=15; j; j--,b0-=0x10,window-=0x20,samples+=step) {
      real sum;
      sum = -window[-0x1] * b0[0x0];
      sum -= window[-0x2] * b0[0x1];
      sum -= window[-0x3] * b0[0x2];
      sum -= window[-0x4] * b0[0x3];
      sum -= window[-0x5] * b0[0x4];
      sum -= window[-0x6] * b0[0x5];
      sum -= window[-0x7] * b0[0x6];
      sum -= window[-0x8] * b0[0x7];
      sum -= window[-0x9] * b0[0x8];
      sum -= window[-0xA] * b0[0x9];
      sum -= window[-0xB] * b0[0xA];
      sum -= window[-0xC] * b0[0xB];
      sum -= window[-0xD] * b0[0xC];
      sum -= window[-0xE] * b0[0xD];
      sum -= window[-0xF] * b0[0xE];
      sum -= window[-0x0] * b0[0xF];

      WRITE_SAMPLE(samples,sum,clip);
			}
		}
  *pnt += 128;

  return clip;
#endif

#ifdef PENTIUM_OPT
  {
    int ret;
    ret = synth_1to1_pent((real *)bandPtr,channel,out+*pnt);
    *pnt += 128;
    return ret;
  }
#else

#ifdef USE_3DNOW
  {
    int ret;
    ret = synth_1to1_3dnow((real *)bandPtr,channel,out+*pnt);
    *pnt += 128;
    return ret;
  }
#else
// PENTIUM normale (con autoincremento)

  if(!channel) {
    bo--;
    bo &= 0xf;
    buf = buffs[0];
		}
  else {
    samples++;
    buf = buffs[1];
		}

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    dct64(buf[1]+((bo+1) & 0xf),buf[0]+bo,bandPtr);
		}
  else {
    b0 = buf[1];
    bo1 = bo+1;
    dct64(buf[0]+bo,buf[1]+bo+1,bandPtr);
		}
  
  {
    register int j;
    real *window = decwin + 16 - bo1;

    for(j=16; j; j--,window+=0x10,samples+=step) {
      real sum;
      sum  = *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;

      WRITE_SAMPLE(samples,sum,clip);
/*			// non va.... MAI
			if(Param.pitch<1.0) { 
				pitchCnt+=Param.pitch; 
				if(pitchCnt>=1.0) 
					pitchCnt-=1.0; 
				else { 
//					samples-=step; 
//				  *pnt -= 1;
					j++;
					}
				} 
			if(Param.pitch>1.0) { 
				pitchCnt+=(Param.pitch-1.0); 
				if(pitchCnt>=1.0) 
					pitchCnt-=1.0; 
				else { 
//					*(samples+step)=*samples; 
//					samples+=step; 
//				  *pnt += 1;
					j--;
					} 
				} */
			}

    {
      real sum;

      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];
      WRITE_SAMPLE(samples,sum,clip);
/*			if(Param.pitch<1.0) { 
				pitchCnt+=Param.pitch; 
				if(pitchCnt>=1.0) 
					pitchCnt-=1.0; 
				else { 
					samples-=step; 
//				  *pnt -= 1;
					}
				} 
			if(Param.pitch>1.0) { 
				pitchCnt+=(Param.pitch-1.0); 
				if(pitchCnt>=1.0) 
					pitchCnt-=1.0; 
				else { 
					*(samples+step)=*samples; 
					samples+=step; 
//				  *pnt += 1;
					} 
				} */
      b0-=0x10,window-=0x20,samples+=step;
    }
    window += bo1 << 1;

    for(j=15; j; j--,b0-=0x20,window-=0x10,samples+=step) {
      real sum;
      sum = -*(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;

      WRITE_SAMPLE(samples,sum,clip);
			/*if(Param.pitch<1.0) { 
				pitchCnt+=Param.pitch; 
				if(pitchCnt>=1.0) 
					pitchCnt-=1.0; 
				else { 
//					samples-=step; 
//				  *pnt -= 1;
					j++;
					}
				} 
			if(Param.pitch>1.0) { 
				pitchCnt+=(Param.pitch-1.0); 
				if(pitchCnt>=1.0) 
					pitchCnt-=1.0; 
				else { 
//					*(samples+step)=*samples; 
//					samples+=step; 
//				  *pnt += 1;
					j--;
					} 
				} */
	    }
		}
  *pnt += 128;
  return clip;

#endif
#endif


	}


int CJoshuaMP3::synth_2to1_8bit(const real *bandPtr,BYTE channel,BYTE *samples,int *pnt) {
  short int samples_tmp[32];
  short int *tmp1 = samples_tmp + channel;
  int i,ret;
  int pnt1 = 0;

  ret = synth_2to1(bandPtr,channel,(BYTE *)samples_tmp,&pnt1);
  samples += channel + *pnt;

  for(i=0; i<16; i++) {
    *samples = conv16to8[*tmp1 >> AUSHIFT];
    samples += 2;
    tmp1 += 2;
		}
	*pnt += 32;

  return ret;
	}

int CJoshuaMP3::synth_2to1_8bit_mono(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[32];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_2to1(bandPtr,0,(BYTE *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0; i<16; i++) {
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    tmp1 += 2;
		}
  *pnt += 16;

  return ret;
	}

int CJoshuaMP3::synth_2to1_8bit_mono2stereo(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[32];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_2to1(bandPtr,0,(BYTE *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0; i<16; i++) {
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    tmp1 += 2;
		}
  *pnt += 32;

  return ret;
	}

int CJoshuaMP3::synth_2to1_mono(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[32];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1=0;

  ret = synth_2to1(bandPtr,0,(BYTE *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0; i<16; i++) {
    *((short int *)samples) = *tmp1;
    samples += 2;
    tmp1 += 2;
		}
	*pnt += 32;

  return ret;
	}

int CJoshuaMP3::synth_2to1_mono2stereo(const real *bandPtr,BYTE *samples,int *pnt) {
  int i,ret;

  ret = synth_2to1(bandPtr,0,samples,pnt);
  samples = samples + *pnt - 64;

  for(i=0; i<16; i++) {
    ((short int *)samples)[1] = ((short *)samples)[0];
    samples+=4;
		}
  
  return ret;
	}

int CJoshuaMP3::synth_2to1(const real *bandPtr,BYTE channel,BYTE *out,int *pnt) {
  short int *samples = (short int *)(out + *pnt);
  real *b0,(*buf)[0x110];
  int clip = 0; 
  int bo1;

	if(theEqualizer)
		theEqualizer->equalize((real *)bandPtr,channel);

  if(!channel) {
    bo--;
    bo &= 0xf;
    buf = buffs[0];
		}
  else {
    samples++;
    buf = buffs[1];
		}

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    dct64(buf[1]+((bo+1)&0xf),buf[0]+bo,bandPtr);
		}
  else {
    b0 = buf[1];
    bo1 = bo+1;
    dct64(buf[0]+bo,buf[1]+bo+1,bandPtr);
		}

  {
    register int j;
    real *window = decwin + 16 - bo1;

    for(j=8; j; j--,b0+=0x10,window+=0x30) {
      real sum;
      sum  = *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;

      WRITE_SAMPLE(samples,sum,clip); samples += step;
#if 0
      WRITE_SAMPLE(samples,sum,clip); samples += step;
#endif
    }

    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];
      WRITE_SAMPLE(samples,sum,clip); samples += step;
#if 0
      WRITE_SAMPLE(samples,sum,clip); samples += step;
#endif
      b0-=0x20,window-=0x40;
			}
    window += bo1<<1;

    for(j=7; j; j--,b0-=0x30,window-=0x30) {
      real sum;
      sum = -*(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;

      WRITE_SAMPLE(samples,sum,clip); samples += step;
#if 0
      WRITE_SAMPLE(samples,sum,clip); samples += step;
#endif
    }
  }

  *pnt += 64;

  return clip;
	}

int CJoshuaMP3::synth_4to1_8bit(const real *bandPtr,BYTE channel,BYTE *samples,int *pnt) {
  short int samples_tmp[16];
  short int *tmp1=samples_tmp + channel;
  int i,ret;
  int pnt1 = 0;

  ret = synth_4to1(bandPtr,channel,(BYTE *)samples_tmp,&pnt1);
  samples += channel + *pnt;

  for(i=0; i<8; i++) {
    *samples = conv16to8[*tmp1 >> AUSHIFT];
    samples += 2;
    tmp1 += 2;
		}
  *pnt += 16;

  return ret;
	}

int CJoshuaMP3::synth_4to1_8bit_mono(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[16];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_4to1(bandPtr,0,(BYTE *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0; i<8; i++) {
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    tmp1 += 2;
		}
  *pnt += 8;

  return ret;
	}


int CJoshuaMP3::synth_4to1_8bit_mono2stereo(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[16];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_4to1(bandPtr,0,(BYTE *)samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0; i<8; i++) {
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    tmp1 += 2;
		}
  *pnt += 16;

  return ret;
	}

int CJoshuaMP3::synth_4to1_mono(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[16];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_4to1(bandPtr,0,(BYTE *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0; i<8; i++) {
    *((short int *)samples) = *tmp1;
    samples += 2;
    tmp1 += 2;
		}
  *pnt += 16;

  return ret;
	}

int CJoshuaMP3::synth_4to1_mono2stereo(const real *bandPtr,BYTE *samples,int *pnt) {
  int i,ret;

  ret = synth_4to1(bandPtr,0,samples,pnt);
  samples = samples + *pnt - 32;

  for(i=0; i<8; i++) {
    ((short int *)samples)[1] = ((short int *)samples)[0];
    samples+=4;
		}

  return ret;
	}

int CJoshuaMP3::synth_4to1(const real *bandPtr,BYTE channel,BYTE *out,int *pnt) {
  short int *samples = (short int *)(out + *pnt);

  real *b0,(*buf)[0x110];
  int clip = 0; 
  int bo1;

	if(theEqualizer)
		theEqualizer->equalize((real *)bandPtr,channel);

  if(!channel) {
    bo--;
    bo &= 0xf;
    buf = buffs[0];
		}
  else {
    samples++;
    buf = buffs[1];
		}

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    dct64(buf[1]+((bo+1)&0xf),buf[0]+bo,bandPtr);
		}
  else {
    b0 = buf[1];
    bo1 = bo+1;
    dct64(buf[0]+bo,buf[1]+bo+1,bandPtr);
		}

  {
    register int j;
    real *window = decwin + 16 - bo1;

    for(j=4;j;j--,b0+=0x30,window+=0x70) {
      real sum;
      sum  = *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;

      WRITE_SAMPLE(samples,sum,clip); samples += step;
#if 0
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
#endif
			}

    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];
      WRITE_SAMPLE(samples,sum,clip); samples += step;
#if 0
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
#endif
      b0-=0x40,window-=0x80;
			}
    window += bo1 << 1;

    for(j=3;j;j--,b0-=0x50,window-=0x70) {
      real sum;
      sum = -*(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;

      WRITE_SAMPLE(samples,sum,clip); samples += step;
#if 0
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
#endif
			}
		}
  
  *pnt += 32;

  return clip;
	}


void CJoshuaMP3::synth_ntom_setStep(long m,long n) {

	if(Param.verbose > 1)
		debug_print(CLogFile::flagError2,"Init rate converter: %d->%d",m,n);

	if(n >= 96000 || m >= 96000 || m == 0 || n == 0) {
#ifdef _DEBUG
		AfxMessageBox("NtoM converter: illegal rates");
#else
		debug_print(CLogFile::flagError,"NtoM converter: illegal rates");
#endif
		return;
//		exit(1);
		}

	n *= NTOM_MUL;
	ntom_step = n / m;

	if(ntom_step > 8*NTOM_MUL) {
#ifdef _DEBUG
		AfxMessageBox("max. 1:8 conversion allowed!");
#else
		debug_print(CLogFile::flagError,"max. 1:8 conversion allowed!");
#endif
		return;
//		exit(1);
		}

	ntom_val[0] = ntom_val[1] = NTOM_MUL >> 1;
	}

int CJoshuaMP3::synth_ntom_8bit(const real *bandPtr,BYTE channel,BYTE *samples,int *pnt) {
  short int samples_tmp[8*64];
  short int *tmp1 = samples_tmp + channel;
  int i,ret;
  int pnt1=0;

  ret = synth_ntom(bandPtr,channel,(BYTE *) samples_tmp,&pnt1);
  samples += channel + *pnt;

  for(i=0; i<(pnt1 >> 2); i++) {
    *samples = conv16to8[*tmp1 >> AUSHIFT];
    samples += 2;
    tmp1 += 2;
		}
  *pnt += pnt1 >> 1;

  return ret;
	}

int CJoshuaMP3::synth_ntom_8bit_mono(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[8*64];
  short int *tmp1=samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_ntom(bandPtr,0,(BYTE *)samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0; i<(pnt1 >> 2); i++) {
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    tmp1 += 2;
		}
  *pnt += pnt1 >> 2;
  
  return ret;
	}

int CJoshuaMP3::synth_ntom_8bit_mono2stereo(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[8*64];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_ntom(bandPtr,0,(BYTE *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0;i<(pnt1>>2);i++) {
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    *samples++ = conv16to8[*tmp1 >> AUSHIFT];
    tmp1 += 2;
		}
  *pnt += pnt1 >> 1;

  return ret;
  }

int CJoshuaMP3::synth_ntom_mono(const real *bandPtr,BYTE *samples,int *pnt) {
  short int samples_tmp[8*64];
  short int *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_ntom(bandPtr,0,(BYTE *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0; i< (pnt1 >> 2); i++) {
    *((short int *)samples) = *tmp1;
    samples += 2;
    tmp1 += 2;
		}
  *pnt += pnt1 >> 1;

  return ret;
	}


int CJoshuaMP3::synth_ntom_mono2stereo(const real *bandPtr,BYTE *samples,int *pnt) {
  int i,ret;
  int pnt1 = *pnt;

  ret = synth_ntom(bandPtr,0,samples,pnt);
  samples += pnt1;
  
  for(i=0;i<((*pnt-pnt1)>>2);i++) {
    ((short int *)samples)[1] = ((short int *)samples)[0];
    samples+=4;
		}

  return ret;
	}


int CJoshuaMP3::synth_ntom(const real *bandPtr,BYTE channel,unsigned char *out,int *pnt) {
  short int *samples = (short int *)(out + *pnt);

  real *b0,(*buf)[0x110];
  int clip = 0; 
  int bo1;
  int ntom;

	if(theEqualizer)
		theEqualizer->equalize((real *)bandPtr,channel);

  if(!channel) {
    bo--;
    bo &= 0xf;
    buf = buffs[0];
    ntom = ntom_val[1] = ntom_val[0];
		}
  else {
    samples++;
    out += 2; // to compute the right *pnt value
    buf = buffs[1];
    ntom = ntom_val[1];
		}

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    dct64(buf[1]+((bo+1) & 0xf),buf[0]+bo,bandPtr);
		}
  else {
    b0 = buf[1];
    bo1 = bo+1;
    dct64(buf[0]+bo,buf[1]+bo+1,bandPtr);
		}


  {
    register int j;
    real *window = decwin + 16 - bo1;
 
    for(j=16; j; j--,window+=0x10) {
      real sum;

      ntom += ntom_step;
      if(ntom < NTOM_MUL) {
        window += 16;
        b0 += 16;
        continue;
      }

      sum  = *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;

      while(ntom >= NTOM_MUL) {
        WRITE_SAMPLE(samples,sum,clip);
        samples += step;
        ntom -= NTOM_MUL;
				}
			}

    ntom += ntom_step;
    if(ntom >= NTOM_MUL) {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];

      while(ntom >= NTOM_MUL) {
        WRITE_SAMPLE(samples,sum,clip);
        samples += step;
        ntom -= NTOM_MUL;
				}
			}

    b0-=0x10,window-=0x20;
    window += bo1<<1;

    for(j=15;j;j--,b0-=0x20,window-=0x10) {
      real sum;

      ntom += ntom_step;
      if(ntom < NTOM_MUL) {
        window -= 16;
        b0 += 16;
        continue;
				}

      sum = -*(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;

      while(ntom >= NTOM_MUL) {
        WRITE_SAMPLE(samples,sum,clip);
        samples += step;
        ntom -= NTOM_MUL;
				}
			}
		}

  ntom_val[channel] = ntom;
  *pnt = ((BYTE *)samples - out);

  return clip;
	}




void CJoshuaMP3::setSynthFunctions() {
	typedef int (CJoshuaMP3::*Func)(const real *,BYTE,BYTE *,int *);
	typedef int (CJoshuaMP3::*FuncMono)(const real *,BYTE *,int *);
	BYTE ds = fr.downSample;
	BYTE p8;

	static Func funcs[2][4] = { 
		{ synth_1to1,
		  synth_2to1,
		  synth_4to1,
		  synth_ntom } ,
		{ synth_1to1_8bit,
		  synth_2to1_8bit,
		  synth_4to1_8bit,
		  synth_ntom_8bit } 
		};

	static FuncMono funcsMono[2][2][4] = {    
		{ { synth_1to1_mono2stereo,
		    synth_2to1_mono2stereo,
		    synth_4to1_mono2stereo,
		    synth_ntom_mono2stereo } ,
		  { synth_1to1_8bit_mono2stereo,
		    synth_2to1_8bit_mono2stereo,
		    synth_4to1_8bit_mono2stereo,
		    synth_ntom_8bit_mono2stereo } } ,
		{ { synth_1to1_mono,
		    synth_2to1_mono,
		    synth_4to1_mono,
		    synth_ntom_mono } ,
		  { synth_1to1_8bit_mono,
		    synth_2to1_8bit_mono,
		    synth_4to1_8bit_mono,
		    synth_ntom_8bit_mono } }
		};

	p8=ai->format == CMP3Audio::AUDIO_FORMAT_UNSIGNED_8 ? 1 : 0;
	fr.synth = funcs[p8][ds];
	fr.synthMono = funcsMono[Param.forceStereo ? 0 : 1][p8][ds];

	if(p8) {
		makeConv16to8Table(ai->format);
		}
	}





/*
 * -funroll-loops (for gcc) will remove the loops for better performance
 * using loops in the source-code enhances readability
 */

void CJoshuaMP3::makeDecodeTables(long scaleval) {
  int i,j,k,kr,divv;
  real *costab;
  int idx;

  for(i=0; i<5; i++) {
    kr=0x10 >> i; divv=0x40 >> i;
    costab = pnts[i];
    for(k=0; k<kr; k++)
      costab[k] = 1.0 / (2.0 * cos(M_PI * ((double)k * 2.0 + 1.0) / (double)divv));
#ifdef USE_3DNOW
    for(k=0; k<kr; k++)
      costab[k+kr] = -costab[k];
#endif
		}

  idx = 0;
  scaleval = -scaleval;
  for(i=0,j=0; i<256; i++,j++,idx+=32) {
    if(idx < 512+16)
      decwin[idx+16] = decwin[idx] = (double)intwinbase[j] / 65536.0 * (double)scaleval;

    if(i % 32 == 31)
      idx -= 1023;
    if(i % 64 == 63)
      scaleval = - scaleval;
	  }

  for( /* i=256 */ ; i<512; i++,j--,idx+=32) {
    if(idx < 512+16)
      decwin[idx+16] = decwin[idx] = (double)intwinbase[j] / 65536.0 * (double)scaleval;

    if(i % 32 == 31)
      idx -= 1023;
    if(i % 64 == 63)
      scaleval = - scaleval;
		}

#ifdef USE_3DNOW
  if(!param.down_sample) {
    for(i=0; i<512+32; i++) {
      decwin[512+31-i] *= 65536.0; /* allows faster clipping in 3dnow code */
      decwin[512+32+i] = decwin[512+31-i];
    }
  }
#endif

  }

void CJoshuaMP3::makeConv16to8Table(int mode) {
  int i;
  const double mul = 8.0;
  /*
   * ????: 8.0 is right but on SB cards '2.0' is a better value ???
   */

  if(!conv16to8_buf) {
    conv16to8_buf = new BYTE[8192];
    if(!conv16to8_buf) {
      debug_print(CLogFile::flagError2,"Can't allocate 16 to 8 converter table!");
			return;
		  }
    conv16to8 = conv16to8_buf + 4096;
	  }

  switch(mode) {
		case CMP3Audio::AUDIO_FORMAT_ULAW_8:
			{
			double m=127.0 / log(256.0);
		  int c1;

	    for(i=-4096; i<4096; i++) {
// dunno whether this is a valid transformation rule ?!?!? 
				if(i < 0)
					c1 = 127 - (int)(log( 1.0 - 255.0 * (double)i*16.0 / 32767.0 ) * m);
				else
					c1 = 255 - (int)(log( 1.0 + 255.0 * (double)i*16.0 / 32767.0 ) * m);
//				if(c1 < 0 || c1 > 255) 
//					debug_print(CLogFile::flagError2,"Converror %d %d\n",i,c1);
				if(c1 == 0)
					c1 = 2;
				conv16to8[i] = (BYTE)c1;
				}
			}
			break;
		case CMP3Audio::AUDIO_FORMAT_SIGNED_8:
			for(i=-4096; i<4096; i++)
				conv16to8[i] = i >> 5;
			break;
		case CMP3Audio::AUDIO_FORMAT_UNSIGNED_8:
			for(i=-4096; i<4096; i++)
				conv16to8[i] = (i >> 5)+128;
			break;
		default:
			for(i=-4096; i<4096; i++)
				conv16to8[i] = 0;
			break;
		}
	}


int CJoshuaMP3::InitOutput() {
//  static int initDone = FALSE;		// qua non funzionerebbe lo STATIC! e poi non serve..

  if(initOutputDone)
    return 1;
  initOutputDone = TRUE;
	// + 1024 for NtoM rate converter 
  if(!(pcmSample = new BYTE[CMP3Audio::AUDIO_BUF_SIZE*2  + 1024] )) {
    perror("malloc()");
		return 0;
    //exit (1);
		}

/*  switch(Param.outmode) {			
    case DECODE_AUDIO:							io faccio gia' audioOpen prima!
      if(audioOpen(&ai) < 0) {
        perror("audio");
        exit(1);
	      }
      break;
    case DECODE_WAV:
      wav_open(&ai,param.filename);
      break;
		}				*/
	return 1;
	}



void CJoshuaMP3::playFrame(int init) {
	int clip;
	long newrate;
	long old_rate,old_format,old_channels;

	if(fr.headerChange || init) {

		if(!Param.quiet && init) {
			if(Param.verbose)
				printHeader();
			else
				printHeaderCompact();
			}

		if(fr.headerChange > 1 || init) {
			old_rate = ai->rate;
			old_format = ai->format;
			old_channels = ai->channels;

			newrate = Param.pitch * (freqs[fr.samplingFrequency] >> (Param.downSample));
            if(Param.verbose && Param.pitch != 1.0)
               fprintf(stderr,"Pitching to %f => %ld Hz\n",Param.pitch,newrate);   

			fr.downSample = Param.downSample;
//      if(param.outmode != DECODE_CDR) {
//	      audio_fit_capabilities(&ai,fr->stereo,newrate);
//            }
//            else {
//              ai.format = AUDIO_FORMAT_SIGNED_16;
//              ai.rate = 44100;
//              ai.channels = 2;
//            }

			// check, whether the fitter setted our proposed rate
			if(ai->rate != newrate) {
				if(ai->rate == (newrate >> 1) )
					fr.downSample++;
				else if(ai->rate == (newrate >> 2) )
					fr.downSample+=2;
				else {
					fr.downSample = 3;
					debug_print(CLogFile::flagError2,"Warning, flexible rate not heavily tested!");
					}
				if(fr.downSample > 3)
					fr.downSample = 3;
				}

			switch(fr.downSample) {
				case 0:
				case 1:
				case 2:
					fr.downSampleSBlimit = SBLIMIT >> (fr.downSample);
					break;
				case 3:
					{
					long n = Param.pitch*freqs[fr.samplingFrequency];
          long m = ai->rate;

					synth_ntom_setStep(n,m);

					if(n>m) {
						fr.downSampleSBlimit = SBLIMIT * m;
						fr.downSampleSBlimit /= n;
						}
					else {
						fr.downSampleSBlimit = SBLIMIT;
						}
					}
					break;
				}

			InitOutput();
			if(ai->rate != old_rate || ai->channels != old_channels ||
			   ai->format != old_format || Param.forceReopen) {
				if(Param.forceMono < 0) {
					if(ai->channels == 1)
						fr.single = 3;
					else
						fr.single = -1;
					}
				else
					fr.single = Param.forceMono;

				Param.forceStereo &= ~0x2;
				if(fr.single >= 0 && ai->channels == 2) {
					Param.forceStereo |= 0x2;
					}

				setSynthFunctions();
				InitLayer3(fr.downSampleSBlimit);
//				reset_audio();
				if(Param.verbose) {
					if(fr.downSample == 3) {
						long n = Param.pitch * freqs[fr.samplingFrequency];
						long m = ai->rate;
						if(n > m) {
							debug_print(CLogFile::flagError2,"Audio: %2.4f:1 conversion,",(float)n/(float)m);
							}
						else {
							debug_print(CLogFile::flagError2,"Audio: 1:%2.4f conversion,",(float)m/(float)n);
							}
						}
					else {
						debug_print(CLogFile::flagError2,"Audio: %d:1 conversion,",(long)pow(2.0,fr.downSample));
						}
// 					debug_print(CLogFile::flagError2," rate: %ld, encoding: %s, channels: %d\n",ai->rate,audio_encoding_name(ai->format),ai->channels);
					}
				}
//			if(intflag)
//				return;
			}
		}

	if(fr.errorProtection)
		getBits(16); // skip crc 

	// do the decoding
	clip = (this->*fr.doLayer)();

	if(clip > 0 && Param.checkRange)
		debug_print(CLogFile::flagError2,"%d samples clipped", clip);
	}


int CJoshuaMP3::Suona(const char *fname, BYTE *bufOut, int nFrom, int nTo, int dnSample, int doMix /* e 8bit? */,
											double volume,double pitch) {
	int retVal=-1;
	int result, clip=0;
	BYTE stereo;
  int crc_error_count, total_error_count;
  DWORD l,old_crc;
  DWORD secdiff;
	char myBuf[256];
	MSG msg;

	Param.forceVolume=volume;
	// v. anche altra suona(), x volumi...
	Param.pitch=pitch;

	Param.outmode = DECODE_BUFFER;
	status=FALSE;
	Param.tryResync=1;
	Param.quiet=1;
	Param.verbose=0;
	minSample=maxSample=32768;

	Param.force8bit=dnSample & 4;

	theEqualizer=new CJoshuaMP3Equalizer;
	if(theEqualizer)
		theEqualizer->init(Param.equalizer=	/* inutilizzato ma ok*/CJoshuaMP3Equalizer::passaFlat /*passaBanda*/,
		Param.forceVolume);

  init=initOutputDone=0;

	readFrameInit();		// importante qua E dopo!
	bufPtr=bufOut;
	bufSize=0;
  if(OpenStream(fname)) {
		retVal=0;
    fr.single = -1;   // both channels 
    fr.synth = synth_1to1;
    fr.downSample = dnSample & 3;		 // 0=44100, 1=22050, 2=11025, 3=8000
    if(Param.force8bit) {
      switch(fr.downSample) {
        case 0:
					ai=new CMP3Audio(this,(HWAVEOUT)0,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8 /*AUDIO_FORMAT_ULAW_8*/,doMix ? 1 : 2,
						-1 /* viene impostato dopo un frame! v. sotto freqs[fr.samplingFrequency] ? freqs[fr.samplingFrequency] : 44100*/);
// serve solo al setFunctions, qui... MIGLIORARE!
          break;
        case 1:
					ai=new CMP3Audio(this,(HWAVEOUT)0,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,doMix ? 1 : 2,
						/*boh*/22050);
          break;
        case 2:
					ai=new CMP3Audio(this,(HWAVEOUT)0,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,doMix ? 1 : 2,
						/*boh*/11025);
          break;
        case 3:							 // per avere 8000Hz mono per il modem...	MONO vero, con UN solo canale (gli altri sono "mono" nel senso di 2 canali uguali! v. sopra)
					ai=new CMP3Audio(this,(HWAVEOUT)0,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,doMix ? 1 : 2,
						/*boh*/8000);
					synth_ntom_setStep(44100,8000);
          break;
				}
			}
    else {
			switch(fr.downSample) {
				case 0:
					ai=new CMP3Audio(this,(HWAVEOUT)0,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16 /*AUDIO_FORMAT_ULAW_8*/,doMix ? 1 : 2,
						-1 /* viene impostato dopo un frame! v. sotto freqs[fr.samplingFrequency] ? freqs[fr.samplingFrequency] : 44100*/);
					break;
				case 1:
					ai=new CMP3Audio(this,(HWAVEOUT)0,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16,doMix ? 1 : 2,
						/*boh*/22050);
					break;
				case 2:
					ai=new CMP3Audio(this,(HWAVEOUT)0,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16,doMix ? 1 : 2,
						/*boh*/11025);
					break;
        case 3:							 // qua non servirebbe...
					ai=new CMP3Audio(this,(HWAVEOUT)0,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16,doMix ? 1 : 2,
						/*boh*/8000);
					synth_ntom_setStep(44100,8000);
					break;
				}
			}

		setSynthFunctions();

		if(doMix) {
			fr.single=3;
			}
		startFrame=nFrom*40;		// circa 40 frame x sec, pare...
		numFrames=nTo ? ((nTo-nFrom)*40) : -1;
		bPaused=0;
		intflag=0;

		readFrameInit();		// importante qua E dopo!
		makeDecodeTables(outscale);
		InitLayer2();		// inits also shared tables with layer1 
		InitLayer3(SBLIMIT >> fr.downSample);	// DARIO

    if(!Param.quiet) {
      wsprintf(myBuf,"Playing MPEG stream from %s ...", fname);
#ifdef _DEBUG
			AfxMessageBox(myBuf);
#else
			debug_print(CLogFile::flagInfo,myBuf);
#endif
			}
		totFrames =reader->filelen;  // più avanti divido per dim. frame
		
		status=TRUE;
    readFrameInit();
    frameNum=0;
    while(numFrames) {
			if(readFrame() <= 0)	 // 0: fine, -1: errore
				break;
			if(frameNum > totFrames)	 // facendo cosi' tengo conto del TAG ID3 al fondo file
				break;												 // i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
      stereo = fr.stereo;
      crc_error_count   = 0;
      total_error_count = 0;

      if(!frameNum) {
//        if(!Param.quiet)
//          printHeader();
				totFrames /= (fr.framesize / 2);
//				totFrames <<= (fr.bitrateIndex/3)-1; // bah!
        if(ai->rate == -1)
          ai->setRate(freqs[fr.samplingFrequency]);
				if(ai->channels != fr.stereo)
					ai->setChannels(fr.stereo);
				totSec=totFrames/(80);	 // i secondi di durata
//					totSec=getSongLen(
				}
      if(frameNum < startFrame || (Param.doubleSpeed && (frameNum % Param.doubleSpeed))) {
        if(fr.lay == 3)
          setPointer(512);
				frameNum++;
				continue;
				}
      if(numFrames>0)
				numFrames--;

			retVal=1;
			playFrame(init);
			init = 0;

			if(Param.verbose>=3)
        debug_print(CLogFile::flagError2, "\r{%5u}",frameNum);
      if(clip > 0 && Param.checkRange)
        debug_print(CLogFile::flagError2,"%d samples clipped", clip);
			frameNum++;
			}
    CloseStream();
		status=FALSE;
    if(!Param.quiet)
#ifdef _DEBUG
      AfxMessageBox("Decoding finished.");
#else
			debug_print(CLogFile::flagInfo,"Decoding finished.");
#endif
		}
  Flush();

	delete conv16to8_buf;
	conv16to8_buf=NULL;
	delete pcmSample;
	pcmSample=NULL;
	delete ai;
	ai=NULL;
	delete ai2;
	ai2=NULL;
	if(theEqualizer) {
		delete theEqualizer;
		theEqualizer=NULL;
		}

	status=FALSE;

	return retVal;
	}

int CJoshuaMP3::Suona(CFile *mF, BYTE *bufOut, int nFrom, int nTo, int dnSample, int doMix /* e 8bit? */,
											double volume,double pitch) {
	int retVal=-1;
	int result, clip=0;
	BYTE stereo;
  int crc_error_count, total_error_count;
  DWORD l,old_crc;
  DWORD secdiff;
	char myBuf[256];
	MSG msg;

	Param.forceVolume=volume;
	// v. anche altra suona(), x volumi...
	Param.pitch=pitch;

	Param.outmode = DECODE_BUFFER;
	status=FALSE;
	Param.tryResync=1;
	Param.quiet=1;
	Param.verbose=0;
	minSample=maxSample=32768;

	Param.force8bit=dnSample & 4;

	theEqualizer=new CJoshuaMP3Equalizer;
	if(theEqualizer)
		theEqualizer->init(Param.equalizer=	/* inutilizzato ma ok*/CJoshuaMP3Equalizer::passaFlat /*passaBanda*/,Param.forceVolume);

  init=initOutputDone=0;

	readFrameInit();		// importante qua E dopo!
	bufPtr=bufOut;
	bufSize=0;
  if(OpenStream(mF)) {
		retVal=0;
    fr.single = -1;   // both channels 
    fr.synth = synth_1to1;
    fr.downSample = dnSample & 3;		 // 0=44100, 1=22050, 2=11025, 3=8000
    if(Param.force8bit) {
			ai=new CMP3Audio(this,(HWAVEOUT)NULL,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,doMix ? 1 : 2,
				-1);
// serve solo al setFunctions, qui... 
			}
    else {
			ai=new CMP3Audio(this,(HWAVEOUT)NULL,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16,doMix ? 1 : 2,
				-1);
			}

		setSynthFunctions();

		if(doMix) {
			fr.single=3;
			}
		startFrame=nFrom*40;		// circa 40 frame x sec, pare...
		numFrames=nTo ? ((nTo-nFrom)*40) : -1;
		bPaused=0;
		intflag=0;

		readFrameInit();		//
		makeDecodeTables(outscale);
		InitLayer2();		// inits also shared tables with layer1 
		InitLayer3(SBLIMIT >> fr.downSample);	// DARIO

    if(!Param.quiet) {
      wsprintf(myBuf,"Playing MPEG stream from %s ...", reader->filename);
#ifdef _DEBUG
			AfxMessageBox(myBuf);
#else
			debug_print(CLogFile::flagInfo,myBuf);
#endif
			}
		totFrames =reader->filelen;  // più avanti divido per dim. frame
		
		status=TRUE;
    readFrameInit();
    frameNum=0;
    while(numFrames) {
			if(readFrame() <= 0)	 // 0: fine, -1: errore
				break;
			if(frameNum > totFrames)	 // facendo cosi' tengo conto del TAG ID3 al fondo file
				break;												 // i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
      stereo = fr.stereo;
      crc_error_count   = 0;
      total_error_count = 0;

      if(!frameNum) {
//        if(!Param.quiet)
//          printHeader();
				totFrames /= (fr.framesize / 2);
//				totFrames <<= (fr.bitrateIndex/3)-1; // bah!
        if(ai->rate == -1)
          ai->setRate(freqs[fr.samplingFrequency]);
				if(ai->channels != fr.stereo)
					ai->setChannels(fr.stereo);
				totSec=totFrames/(80);	 // i secondi di durata
//					totSec=getSongLen(
				}
      if(frameNum < startFrame || (Param.doubleSpeed && (frameNum % Param.doubleSpeed))) {
        if(fr.lay == 3)
          setPointer(512);
				frameNum++;
				continue;
				}
      if(numFrames>0)
				numFrames--;

			retVal=1;
			playFrame(init);
//			mf->Write();		// FINIRE!! cos'era?? 2023...
			init = 0;

			if(Param.verbose>=3)
        debug_print(CLogFile::flagError2, "{%5u}",frameNum);
      if(clip > 0 && Param.checkRange)
        debug_print(CLogFile::flagError2,"%d samples clipped", clip);
			frameNum++;
			}
		status=FALSE;
    if(!Param.quiet)
#ifdef _DEBUG
      AfxMessageBox("Decoding finished.");
#else
			debug_print(CLogFile::flagInfo,"Decoding finished.");
#endif
		}
  Flush();

	delete conv16to8_buf;
	conv16to8_buf=NULL;
	delete pcmSample;
	pcmSample=NULL;

	Param.forceMono=ai->channels==1;		// PATCH per far sapere al chiamante che era Mono! (videosender playback)

	delete ai;
	ai=NULL;
	if(theEqualizer) {
		delete theEqualizer;
		theEqualizer=NULL;
		}

	return retVal;
	}

int CJoshuaMP3::Suona(const char *fname, int nFrom, int nTo, int dnSample, int doMix, HWAVEOUT hDevice, 
											CJoshuaMP3Equalizer::EQUALIZER equalizer,double volume,double pitch,BOOL bUI) {
	int retVal=-1;
  int i;
	char myBuf[256];
  int result, clip=0;
	BYTE stereo;
  int crc_error_count, total_error_count;
  DWORD l,old_crc;
  DWORD secdiff;
	MSG msg;


	if(bBusy)
		stop();

	bBusy=TRUE;
	minSample=maxSample=32768;

	Param.forceVolume= volume;
//	Param.forceVolume=theApp.Volumi[CSound::MP3_CURSOR][0]/32768.0;
	// è forse l'unico modo x usare i volumi...
	Param.pitch=pitch;

	Param.force8bit=dnSample & 4;
	Param.outmode = DECODE_AUDIO;
	status=FALSE;
	Param.quiet=1;
	Param.tryResync=1;
	Param.verbose=0;

	theEqualizer=new CJoshuaMP3Equalizer;
	if(theEqualizer)
		theEqualizer->init(equalizer,Param.forceVolume);
  init=initOutputDone=0;

	if(fname) {
    fr.single = -1;   // both channels 
    fr.synth = synth_1to1;
    fr.downSample = dnSample & 3;

    if(Param.force8bit) {
#if 0
      ai->format = CMP3Audio::AUDIO_FORMAT_UNSIGNED_8;	// spostato!
      ai->format = CMP3Audio::AUDIO_FORMAT_SIGNED_8;
#endif
      switch(fr.downSample) {
        case 0:
					ai=new CMP3Audio(this,hDevice,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8 /*AUDIO_FORMAT_ULAW_8*/,doMix ? 1 : 2,44100,CMP3Audio::AUDIO_BUF_SIZE,Param.verbose);
          break;
        case 1:
					ai=new CMP3Audio(this,hDevice,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,doMix ? 1 : 2,22050,CMP3Audio::AUDIO_BUF_SIZE/2,Param.verbose);
          break;
        case 2:
					ai=new CMP3Audio(this,hDevice,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,doMix ? 1 : 2,11025,CMP3Audio::AUDIO_BUF_SIZE/4,Param.verbose);
          break;
        case 3:							 // per avere 8000Hz mono per il modem...	MONO vero, con UN solo canale (gli altri sono "mono" nel senso di 2 canali uguali! v. sopra)
					ai=new CMP3Audio(this,hDevice,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,1,8000,CMP3Audio::AUDIO_BUF_SIZE/4,Param.verbose);
					synth_ntom_setStep(44100,8000);
          break;
				}
			}
    else {
			switch(fr.downSample) {
				case 0:
					ai=new CMP3Audio(this,hDevice,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16 /*AUDIO_FORMAT_ULAW_8*/,doMix ? 1 : 2,44100,CMP3Audio::AUDIO_BUF_SIZE,Param.verbose);
					break;
				case 1:
					ai=new CMP3Audio(this,hDevice,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16,doMix ? 1 : 2,22050,CMP3Audio::AUDIO_BUF_SIZE/2,Param.verbose);
					break;
				case 2:
					ai=new CMP3Audio(this,hDevice,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16,doMix ? 1 : 2,11025,CMP3Audio::AUDIO_BUF_SIZE/4,Param.verbose);
					break;
				}
			}
		setSynthFunctions();

		if(doMix) {
			fr.single=3;
			}
		totSec=0;
		startFrame=nFrom*40;		// ca. 40 frame x sec...
		numFrames=nTo ? ((nTo-nFrom)*40) : -1;
		bPaused=0;
		intflag=0;

		readFrameInit();		// importante qua E dopo!
		makeDecodeTables(outscale);
		InitLayer2();		// inits also shared tables with layer1 
		InitLayer3(SBLIMIT >> fr.downSample);			// DARIO

	  if(OpenStream(fname)) {
			retVal=0;
			totFrames =reader->filelen;  // più avanti divido per dim. frame
			if(readFrame() > 0) {
				totFrames /= (fr.framesize / 2);
				totSec=totFrames/(80);	 // i secondi di durata. Serve altrove! (v. sottofondo)
//					totSec=getSongLen(
				}

			if(bUI) {
//				m_Dlg = new CMP3Status(this);
		//		SetDlgItemText(hDlg,IDC_TEXT1,fname);
		//		ShowWindow(hDlg,SW_SHOW);
				}
			if(!Param.quiet) {
				wsprintf(myBuf,"Playing MPEG stream from %s ...", fname);
#ifdef _DEBUG
				AfxMessageBox(myBuf);
#else
				debug_print(CLogFile::flagInfo,myBuf);
#endif
				}
			totFrames =reader->filelen;  // più avanti divido per dim. frame
		
			if(reader->flags & MP3_READER::READER_ID3TAG) {
				memcpy(ID3.titolo,reader->id3buf.titolo,30);
				theApp.trim(ID3.titolo,30);
				memcpy(ID3.autore,reader->id3buf.autore,30);
				theApp.trim(ID3.autore,30);
				memcpy(ID3.album,reader->id3buf.album,30);
				theApp.trim(ID3.album,30);
				memcpy(ID3.anno,reader->id3buf.anno,4);
				theApp.trim(ID3.anno,4);
				memcpy(ID3.note,reader->id3buf.note,30);
				theApp.trim(ID3.note,30);
				ID3.genere=reader->id3buf.genere;
				if(m_Dlg) {
					wsprintf(myBuf,"%s - %s",ID3.titolo,ID3.autore);
					m_Dlg->SetDlgItemText(IDC_TEXT1,myBuf);
					}
				}
			else {
				*ID3.titolo=0;
				if(m_Dlg) {
					m_Dlg->SetDlgItemText(IDC_TEXT1,fname);
					}
				}

			status=TRUE;
			
			readFrameInit();
			frameNum=0;
			while(numFrames && !intflag) {
				if(bPaused)
					goto paused;
				if(readFrame() <= 0)	 // 0: fine, -1: errore
					break;
				if(frameNum > totFrames)	 // facendo cosi' tengo conto del TAG ID3 al fondo file
					break;												 // i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
				stereo = fr.stereo;
				crc_error_count   = 0;
				total_error_count = 0;

				if(!frameNum) {
//					if(!Param.quiet)
//						printHeader();
					totFrames /= (fr.framesize / 2);
	//				totFrames <<= (fr.bitrateIndex/3)-1; // bah!
					if(ai->rate == -1)
						ai->rate = freqs[fr.samplingFrequency];
					if(ai->channels != fr.stereo)
						ai->setChannels(fr.stereo);
					totSec=totFrames/(78);	 // i secondi di durata
//					totSec=getSongLen(
					CVidsendView22 *v=(CVidsendView22*)tag;
					if(v)
						v->mp3NotifyCallback(this,2,totFrames/2,totSec);
					if(m_Dlg) {
	//					totSec=totFrames/77+1;
						m_Dlg->m_Slider.SetRange(0,totFrames/10,TRUE);
						m_Dlg->m_Slider.SetPageSize(totFrames/400);
						m_Dlg->m_Slider.SetTicFreq(3*77);		// tacche ogni 30 sec.
						wsprintf(myBuf,"%d:%02d",totSec/60,totSec % 60);
						m_Dlg->SetDlgItemText(IDC_TEXT2,myBuf);
						}
					if(ai->open(ai->perif) < 0) {
#ifdef _DEBUG
						AfxMessageBox("errore open() audio");
#else
						debug_print(CLogFile::flagError,"errore open() audio");
#endif
			//      perror("audio");
						break;
						}

//			theApp.volume();

					}
				if(frameNum < startFrame || (Param.doubleSpeed && (frameNum % Param.doubleSpeed))) {
					if(fr.lay == 3)
						setPointer(512);
					continue;
					}
				if(numFrames>0)
					numFrames--;

				if(!(frameNum % 16)) {
					CVidsendView22 *v;
					v=(CVidsendView22*)tag;
					if(v)
						v->mp3NotifyCallback(this,3,frameNum,0);
					if(m_Dlg)
						m_Dlg->m_Slider.SetPos(frameNum/5);
					// i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
					// 45000 e' un liv. basso, 55000 e' normale, 58-60000 è forte
					}

				retVal=1;
				playFrame(init);
				init = 0;

				if(Param.verbose>=3)
					debug_print(CLogFile::flagError2, "\r{%5u}",frameNum);
				if(clip > 0 && Param.checkRange)
					debug_print(CLogFile::flagError2,"%d samples clipped", clip);
				frameNum++;
paused:
				;
				if(m_Dlg) {
					if(PeekMessage(&msg,NULL,0,0,PM_REMOVE /*| PM_NOYIELD*/)) {
						if(!TranslateAccelerator(((CMainFrame *)theApp.m_pMainWnd)->m_hWnd,((CMainFrame *)theApp.m_pMainWnd)->m_hAccelTable,&msg)) {
							TranslateMessage(&msg); 	 // Translates virtual key codes
							DispatchMessage(&msg);		 // Dispatches message to window
							}
						}
					}
				}
			CloseStream();
			status=FALSE;
			if(!Param.quiet)
#ifdef _DEBUG
				AfxMessageBox("Decoding finished.");
#else
				debug_print(CLogFile::flagInfo,"Decoding finished.");
#endif
			}
		Flush();

		ai->close(intflag);
		delete conv16to8_buf;
		conv16to8_buf=NULL;
		delete pcmSample;
		pcmSample=NULL;
		delete ai;
		ai=NULL;
		delete ai2;
		ai2=NULL;
		if(theEqualizer) {
			delete theEqualizer;
			theEqualizer=NULL;
			}

		from=to=0;
	//		*title=0;
		currFrame=totFrames=totSec=0;
		if(m_Dlg) {
			delete m_Dlg;
			m_Dlg=NULL;
			}
		status=FALSE;
		bBusy=FALSE;

		
		return retVal;
		}
	else
		return 0;
	}
	

int CJoshuaMP3::SuonaDX(const char *fname, int nFrom, int nTo, int dnSample, int doMix, 
												CJoshuaMP3Equalizer::EQUALIZER equalizer,double volume,double pitch,BOOL bUI) {
	int retVal=-1;
  int i;
	char myBuf[256];
  int result, clip=0;
	BYTE stereo;
  int crc_error_count, total_error_count;
  DWORD l,old_crc;
  DWORD secdiff;
	MSG msg;


	if(bBusy)
		stop();

	bBusy=TRUE;
	minSample=maxSample=32768;

	Param.forceVolume= volume;
	Param.pitch=pitch;

	Param.force8bit=dnSample & 4;
	Param.outmode = DECODE_AUDIO;
	status=FALSE;
	Param.quiet=1;
	Param.tryResync=1;
	Param.verbose=2;

	theEqualizer=new CJoshuaMP3Equalizer;
	if(theEqualizer)
		theEqualizer->init(equalizer,Param.forceVolume);
  init=initOutputDone=0;

	if(fname) {
    fr.single = -1;   // both channels 
    fr.synth = synth_1to1;
    fr.downSample = dnSample & 3;

    if(Param.force8bit) {
#if 0
      ai->format = CMP3Audio::AUDIO_FORMAT_UNSIGNED_8;	// spostato!
      ai->format = CMP3Audio::AUDIO_FORMAT_SIGNED_8;
#endif
      switch(fr.downSample) {
        case 0:
					ai=new CMP3Audio(this,1,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8 /*AUDIO_FORMAT_ULAW_8*/,doMix ? 1 : 2,
						-1 /* viene impostato dopo un frame! v. sotto freqs[fr.samplingFrequency] ? freqs[fr.samplingFrequency] : 44100*/,
						CMP3Audio::AUDIO_BUF_SIZE,Param.verbose);
          break;
        case 1:
					ai=new CMP3Audio(this,1,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,doMix ? 1 : 2,/*boh*/22050,
						CMP3Audio::AUDIO_BUF_SIZE/2,Param.verbose);
          break;
        case 2:
					ai=new CMP3Audio(this,1,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,doMix ? 1 : 2,/*boh*/11025,
						CMP3Audio::AUDIO_BUF_SIZE/4,Param.verbose);
          break;
        case 3:							 // per avere 8000Hz mono per il modem...	MONO vero, con UN solo canale (gli altri sono "mono" nel senso di 2 canali uguali! v. sopra)
					ai=new CMP3Audio(this,1,CMP3Audio::AUDIO_FORMAT_UNSIGNED_8,1,/*boh*/8000,
						CMP3Audio::AUDIO_BUF_SIZE/4,Param.verbose);
					synth_ntom_setStep(44100,8000);
          break;
				}
			}
    else {
			switch(fr.downSample) {
				case 0:
					ai=new CMP3Audio(this,1,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16 /*AUDIO_FORMAT_ULAW_8*/,doMix ? 1 : 2,
						-1 /* viene impostato dopo un frame! v. sotto freqs[fr.samplingFrequency] ? freqs[fr.samplingFrequency] : 44100*/,
						CMP3Audio::AUDIO_BUF_SIZE,Param.verbose);
					break;
				case 1:
					ai=new CMP3Audio(this,1,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16,doMix ? 1 : 2,/*boh*/22050,
						CMP3Audio::AUDIO_BUF_SIZE/2,Param.verbose);
					break;
				case 2:
					ai=new CMP3Audio(this,1,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16,doMix ? 1 : 2,/*boh*/11025,
						CMP3Audio::AUDIO_BUF_SIZE/4,Param.verbose);
					break;
				}
			}
		setSynthFunctions();

		if(doMix) {
			fr.single=3;
			}
		totSec=0;
		startFrame=nFrom*40;		// ca. 40 frame x sec...
		numFrames=nTo ? ((nTo-nFrom)*40) : -1;
		bPaused=0;
		intflag=0;

		readFrameInit();		// importante qua E dopo!
		makeDecodeTables(outscale);
		InitLayer2();		// inits also shared tables with layer1 
		InitLayer3(SBLIMIT >> fr.downSample);			// DARIO

	  if(OpenStream(fname)) {
			retVal=0;
			if(reader->filelen == 0xffffffff) {		// stream
				totFrames =0x7fffffff;			// tanto per.. un numero grande e positivo!
				totSec=86400;	 // un giorno
				if(readFrame() > 0) {
					}
				else
					goto fine;
				}
			else {
				totFrames =reader->filelen;  // più avanti divido per dim. frame
				if(readFrame() > 0) {
					totFrames /= (fr.framesize / 2);
					totSec=totFrames/(80);	 // i secondi di durata. Serve altrove! (v. sottofondo)
//					totSec=getSongLen(fr,no);
					}
				else
					goto fine;
				}

			if(bUI) {
//				m_Dlg = new CMP3Status(this);
		//		SetDlgItemText(hDlg,IDC_TEXT1,fname);
		//		ShowWindow(hDlg,SW_SHOW);
				}
			if(!Param.quiet) {
				wsprintf(myBuf,"Playing MPEG stream from %s ...", fname);
#ifdef _DEBUG
				AfxMessageBox(myBuf);
#else
				debug_print(CLogFile::flagInfo,myBuf);
#endif
				}
		
			if(reader->flags & MP3_READER::READER_ID3TAG) {
				memcpy(ID3.titolo,reader->id3buf.titolo,30);
				theApp.trim(ID3.titolo,30);
				memcpy(ID3.autore,reader->id3buf.autore,30);
				theApp.trim(ID3.autore,30);
				memcpy(ID3.album,reader->id3buf.album,30);
				theApp.trim(ID3.album,30);
				memcpy(ID3.anno,reader->id3buf.anno,4);
				theApp.trim(ID3.anno,4);
				memcpy(ID3.note,reader->id3buf.note,30);
				theApp.trim(ID3.note,30);
				ID3.genere=reader->id3buf.genere;
				if(m_Dlg) {
					wsprintf(myBuf,"%s - %s",ID3.titolo,ID3.autore);
					m_Dlg->SetDlgItemText(IDC_TEXT1,myBuf);
					}
				}
			else {
				*ID3.titolo=0;
				if(m_Dlg) {
					m_Dlg->SetDlgItemText(IDC_TEXT1,fname);
					}
				}

			status=TRUE;

			readFrameInit();
			frameNum=0;
			while(numFrames && !intflag) {
				if(bPaused)
					goto paused;
				if(readFrame() <= 0) {	 // 0: fine, -1: errore
          debug_print(CLogFile::flagError2,"mpg123:audio flush");
					ai->flush();		// in teoria solo se errore... per resyncare i buffer audio
					break;
					}
				if(frameNum > totFrames)	 // facendo cosi' tengo conto del TAG ID3 al fondo file
					break;												 // i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
				stereo = fr.stereo;
				crc_error_count   = 0;
				total_error_count = 0;

				if(!frameNum) {
//					if(!Param.quiet)
//						printHeader();
// già in Open...2021					totFrames /= (fr.framesize / 2);
	//				totFrames <<= (fr.bitrateIndex/3)-1; // bah!
					if(ai->rate == -1)
						ai->setRate(freqs[fr.samplingFrequency]);
					if(ai->channels != fr.stereo)
						ai->setChannels(fr.stereo);
//					totSec=getSongLen(
//					totSec=totFrames/(78);	 // i secondi di durata
					CVidsendView22 *v=(CVidsendView22*)tag;
					if(v)
						v->mp3NotifyCallback(this,2,totFrames/2,totSec);
					if(m_Dlg) {
	//					totSec=totFrames/77+1;
						m_Dlg->m_Slider.SetRange(0,totFrames/10,TRUE);
						m_Dlg->m_Slider.SetPageSize(totFrames/400);
						m_Dlg->m_Slider.SetTicFreq(3*77);		// tacche ogni 30 sec.
						wsprintf(myBuf,"%d:%02d",totSec/60,totSec % 60);
						m_Dlg->SetDlgItemText(IDC_TEXT2,myBuf);
						}
					if(ai->open(ai->perif) < 0) {
#ifdef _DEBUG
						AfxMessageBox("errore open() audio");
#else
						debug_print(CLogFile::flagError,"errore open() audio");
#endif
			//      perror("audio");
						break;
						}

					}
				if(frameNum < startFrame || (Param.doubleSpeed && (frameNum % Param.doubleSpeed))) {
					if(fr.lay == 3)
						setPointer(512);
					continue;
					}
				if(numFrames>0)
					numFrames--;


				if(!(frameNum % 16)) {
					CVidsendView22 *v;
					v=(CVidsendView22*)tag;
					if(v)
						v->mp3NotifyCallback(this,3,frameNum,0);
					// i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
					// 45000 e' un liv. basso, 55000 e' normale, 58-60000 è forte
					if(m_Dlg)
						m_Dlg->m_Slider.SetPos(frameNum/5);
					// i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
					}

				retVal=1;
				playFrame(init);
				init = 0;

				if(Param.verbose>=3)
					debug_print(CLogFile::flagInfo,"\r{%5u}",frameNum);
				if(clip > 0 && Param.checkRange)
					debug_print(CLogFile::flagInfo,"%d samples clipped", clip);
				frameNum++;
paused:
				;
				if(m_Dlg) {
					if(PeekMessage(&msg,m_Dlg->m_hWnd,0,0,PM_REMOVE /*| PM_NOYIELD*/)) {
						if(!TranslateAccelerator(m_Dlg->m_hWnd,((CMainFrame *)theApp.m_pMainWnd)->m_hAccelTable,&msg)) {
							TranslateMessage(&msg); 	 // Translates virtual key codes
							DispatchMessage(&msg);		 // Dispatches message to window
							}
						}
					}
				}			// while
			CloseStream();
			status=FALSE;
			if(!Param.quiet)
#ifdef _DEBUG
				AfxMessageBox("Decoding finished.");
#else
				debug_print(CLogFile::flagInfo,"Decoding finished.");
#endif
			}
		Flush();

		ai->close(intflag);

fine:
		delete conv16to8_buf;
		conv16to8_buf=NULL;
		delete pcmSample;
		pcmSample=NULL;
		delete ai;
		ai=NULL;
		delete ai2;
		ai2=NULL;
		if(theEqualizer) {
			delete theEqualizer;
			theEqualizer=NULL;
			}

		from=to=0;
	//		*title=0;
		currFrame=totFrames=totSec=0;
		if(m_Dlg) {
			delete m_Dlg;
			m_Dlg=NULL;
			}
		status=FALSE;
		bBusy=FALSE;

//						debug_print(CLogFile::flagInfo,"  esce suonaDX.");

//						Sleep(1000);
		
		return retVal;
		}
	else
		return 0;
	}
	
int CJoshuaMP3::Suona2(const char *fname,BYTE acard,CJoshuaMP3Equalizer::EQUALIZER equalizer,double volume,double pitch) {
	int retVal=-1;
  int i;
	char myBuf[256];
  int result, clip=0;
	BYTE stereo;
  int crc_error_count, total_error_count;
  DWORD l,old_crc;
  DWORD secdiff;
	MSG msg;


	if(bBusy)
		stop();

	bBusy=TRUE;
	minSample=maxSample=32768;

	Param.forceVolume= volume;
	Param.pitch=pitch;

	Param.force8bit=0;
	Param.outmode = DECODE_BOTH;
	status=FALSE;
	Param.quiet=1;
	Param.tryResync=1;
	Param.verbose=2;

	theEqualizer=new CJoshuaMP3Equalizer;
	if(theEqualizer)
		theEqualizer->init(equalizer,Param.forceVolume);
  init=initOutputDone=0;

	if(fname) {
    fr.single = -1;   // both channels 
    fr.synth = synth_ntom;
    fr.downSample = 0;

		ai=new CMP3Audio(this,acard,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16 /*AUDIO_FORMAT_ULAW_8*/,2,
			-1 /* viene impostato dopo un frame! v. sotto freqs[fr.samplingFrequency] ? freqs[fr.samplingFrequency] : 44100*/,
			CMP3Audio::AUDIO_BUF_SIZE,Param.verbose);
		bufPtr=new BYTE[44100L*2*2];
		bufSize=0;

		setSynthFunctions();

		totSec=0;
		startFrame=0;		// ca. 40 frame x sec...
		numFrames=-1;
		bPaused=0;
		intflag=0;

		readFrameInit();		// importante qua E dopo!
		makeDecodeTables(outscale);
		InitLayer2();		// inits also shared tables with layer1 
		InitLayer3(SBLIMIT >> fr.downSample);			// DARIO

	  if(OpenStream(fname)) {
			retVal=0;
			if(reader->filelen == 0xffffffff) {		// stream
				totFrames =0x7fffffff;			// tanto per.. un numero grande e positivo!
				totSec=86400;	 // un giorno
				if(readFrame() > 0) {
					}
				else
					goto fine;
				}
			else {
				totFrames =reader->filelen;  // più avanti divido per dim. frame
				if(readFrame() > 0) {
					totFrames /= (fr.framesize / 2);
					totSec=totFrames/(80);	 // i secondi di durata. Serve altrove! (v. sottofondo)
//					totSec=getSongLen(fr,no);
					}
				else
					goto fine;
				}

			if(!Param.quiet) {
				wsprintf(myBuf,"Playing MPEG stream from %s ...", fname);
#ifdef _DEBUG
				AfxMessageBox(myBuf);
#else
				debug_print(CLogFile::flagInfo,myBuf);
#endif
				}
		
			if(reader->flags & MP3_READER::READER_ID3TAG) {
				memcpy(ID3.titolo,reader->id3buf.titolo,30);
				theApp.trim(ID3.titolo,30);
				memcpy(ID3.autore,reader->id3buf.autore,30);
				theApp.trim(ID3.autore,30);
				memcpy(ID3.album,reader->id3buf.album,30);
				theApp.trim(ID3.album,30);
				memcpy(ID3.anno,reader->id3buf.anno,4);
				theApp.trim(ID3.anno,4);
				memcpy(ID3.note,reader->id3buf.note,30);
				theApp.trim(ID3.note,30);
				ID3.genere=reader->id3buf.genere;
				}
			else {
				*ID3.titolo=0;
				}

			status=TRUE;

			readFrameInit();
			frameNum=0;
			while(numFrames && !intflag) {
				if(bPaused)
					goto paused;
				if(readFrame() <= 0) {	 // 0: fine, -1: errore
          debug_print(CLogFile::flagError2,"mpg123:audio flush");
					if(ai)
						ai->flush();		// in teoria solo se errore... per resyncare i buffer audio
					break;
					}
				if(frameNum > totFrames)	 // facendo cosi' tengo conto del TAG ID3 al fondo file
					break;												 // i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
				stereo = fr.stereo;
				crc_error_count   = 0;
				total_error_count = 0;

				if(!frameNum) {
//					if(!Param.quiet)
//						printHeader();
// già in Open...2021					totFrames /= (fr.framesize / 2);
	//				totFrames <<= (fr.bitrateIndex/3)-1; // bah!
					if(ai->rate == -1)
						ai->setRate(freqs[fr.samplingFrequency]);
					if(ai->channels != fr.stereo)
						ai->setChannels(fr.stereo);
//					totSec=getSongLen(
//					totSec=totFrames/(78);	 // i secondi di durata
					CVidsendView22 *v=(CVidsendView22*)tag;
					if(v)
						v->mp3NotifyCallback(this,2,totFrames/2,totSec);
					if(ai) {
						if(ai->open(1) < 0) {
#ifdef _DEBUG
							AfxMessageBox("errore open() audio");
#else
							debug_print(CLogFile::flagError,"errore open() audio");
#endif
							break;
							}
						}
					}
				if(frameNum < startFrame || (Param.doubleSpeed && (frameNum % Param.doubleSpeed))) {
					if(fr.lay == 3)
						setPointer(512);
					continue;
					}
				if(numFrames>0)
					numFrames--;

				if(!(frameNum % 16)) {
					CVidsendView22 *v;
					v=(CVidsendView22*)tag;
					if(v)
						v->mp3NotifyCallback(this,3,frameNum,0);
					// i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
					// 45000 e' un liv. basso, 55000 e' normale, 58-60000 è forte
					// i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
					}

				retVal=1;
				playFrame(init);
				init = 0;

				if(Param.verbose>=3)
					debug_print(CLogFile::flagInfo,"\r{%5u}",frameNum);
				if(clip > 0 && Param.checkRange)
					debug_print(CLogFile::flagInfo,"%d samples clipped", clip);
				frameNum++;
paused:
				;
				}			// while
			CloseStream();
			status=FALSE;
			if(!Param.quiet)
#ifdef _DEBUG
				AfxMessageBox("Decoding finished.");
#else
				debug_print(CLogFile::flagInfo,"Decoding finished.");
#endif
			}
		Flush();

		if(ai)
			ai->close(intflag);
		delete bufPtr;
		bufPtr=NULL;

fine:
		delete conv16to8_buf;
		conv16to8_buf=NULL;
		delete pcmSample;
		pcmSample=NULL;
		delete ai;
		ai=NULL;
		if(theEqualizer) {
			delete theEqualizer;
			theEqualizer=NULL;
			}

		from=to=0;
	//		*title=0;
		currFrame=totFrames=totSec=0;
		bBusy=FALSE;

//						debug_print(CLogFile::flagInfo,"  esce suonaDX.");

//						Sleep(1000);
		
		return retVal;
		}
	else
		return 0;
	}
	
int CJoshuaMP3::Suona3(const char *fname, CJoshuaMP3Equalizer::EQUALIZER equalizer,double volume,double pitch) {
	int retVal=-1;
  int i;
	char myBuf[256];
  int result, clip=0;
	BYTE stereo;
  int crc_error_count, total_error_count;
  DWORD l,old_crc;
  DWORD secdiff;
	MSG msg;


	if(bBusy)
		stop();

	bBusy=TRUE;
	minSample=maxSample=32768;

	Param.forceVolume= volume;
	Param.pitch=pitch;

	Param.force8bit=0;
	Param.outmode = DECODE_BOTH;
	status=FALSE;
	Param.quiet=1;
	Param.tryResync=1;
	Param.verbose=2;

	theEqualizer=new CJoshuaMP3Equalizer;
	if(theEqualizer)
		theEqualizer->init(Param.equalizer=	/* inutilizzato ma ok*/equalizer,Param.forceVolume);
  init=initOutputDone=0;

	if(fname) {
    fr.single = -1;   // both channels 
    fr.synth = synth_ntom;
    fr.downSample = 0;

		ai=new CMP3Audio(this,1,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16 /*AUDIO_FORMAT_ULAW_8*/,2,
			-1 /* viene impostato dopo un frame! v. sotto freqs[fr.samplingFrequency] ? freqs[fr.samplingFrequency] : 44100*/,
			CMP3Audio::AUDIO_BUF_SIZE,Param.verbose);
		ai2=new CMP3Audio(this,2,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16,2,44100,
			CMP3Audio::AUDIO_BUF_SIZE,Param.verbose);
		bufPtr=new BYTE[44100L*2*2];
		bufSize=0;

		setSynthFunctions();

		totSec=0;
		startFrame=0;		// ca. 40 frame x sec...
		numFrames=-1;
		bPaused=0;
		intflag=0;

		readFrameInit();		// importante qua E dopo!
		makeDecodeTables(outscale);
		InitLayer2();		// inits also shared tables with layer1 
		InitLayer3(SBLIMIT >> fr.downSample);			// DARIO

	  if(OpenStream(fname)) {
			retVal=0;
			if(reader->filelen == 0xffffffff) {		// stream
				totFrames =0x7fffffff;			// tanto per.. un numero grande e positivo!
				totSec=86400;	 // un giorno
				if(readFrame() > 0) {
					}
				else
					goto fine;
				}
			else {
				totFrames =reader->filelen;  // più avanti divido per dim. frame
				if(readFrame() > 0) {
					totFrames /= (fr.framesize / 2);
					totSec=totFrames/(80);	 // i secondi di durata. Serve altrove! (v. sottofondo)
//					totSec=getSongLen(fr,no);
					}
				else
					goto fine;
				}

			if(!Param.quiet) {
				wsprintf(myBuf,"Playing MPEG stream from %s ...", fname);
#ifdef _DEBUG
				AfxMessageBox(myBuf);
#else
				debug_print(CLogFile::flagInfo,myBuf);
#endif
				}
		
			if(reader->flags & MP3_READER::READER_ID3TAG) {
				memcpy(ID3.titolo,reader->id3buf.titolo,30);
				theApp.trim(ID3.titolo,30);
				memcpy(ID3.autore,reader->id3buf.autore,30);
				theApp.trim(ID3.autore,30);
				memcpy(ID3.album,reader->id3buf.album,30);
				theApp.trim(ID3.album,30);
				memcpy(ID3.anno,reader->id3buf.anno,4);
				theApp.trim(ID3.anno,4);
				memcpy(ID3.note,reader->id3buf.note,30);
				theApp.trim(ID3.note,30);
				ID3.genere=reader->id3buf.genere;
				}
			else {
				*ID3.titolo=0;
				}

			status=TRUE;

			readFrameInit();
			frameNum=0;
			while(numFrames && !intflag) {
				if(bPaused)
					goto paused;
				if(readFrame() <= 0) {	 // 0: fine, -1: errore
          debug_print(CLogFile::flagError2,"mpg123:audio flush");
					if(ai)
						ai->flush();		// in teoria solo se errore... per resyncare i buffer audio
					if(ai2)
						ai2->flush();		// in teoria solo se errore... per resyncare i buffer audio
					break;
					}
				if(frameNum > totFrames)	 // facendo cosi' tengo conto del TAG ID3 al fondo file
					break;												 // i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
				stereo = fr.stereo;
				crc_error_count   = 0;
				total_error_count = 0;

				if(!frameNum) {
//					if(!Param.quiet)
//						printHeader();
// già in Open...2021					totFrames /= (fr.framesize / 2);
	//				totFrames <<= (fr.bitrateIndex/3)-1; // bah!
					if(ai->rate == -1)
						ai->setRate(freqs[fr.samplingFrequency]);
					if(ai->channels != fr.stereo)
						ai->setChannels(fr.stereo);
					if(ai2->rate == -1)
						ai2->setRate(freqs[fr.samplingFrequency]);
					if(ai2->channels != fr.stereo)
						ai2->setChannels(fr.stereo);
//					totSec=getSongLen(
//					totSec=totFrames/(78);	 // i secondi di durata
					CVidsendView22 *v=(CVidsendView22*)tag;
					if(v)
						v->mp3NotifyCallback(this,2,totFrames/2,totSec);
					if(ai) {
						if(ai->open(1) < 0) {
#ifdef _DEBUG
							AfxMessageBox("errore open() audio");
#else
							debug_print(CLogFile::flagError,"errore open() audio");
#endif
							break;
							}
						}
					if(ai2) {
						if(ai2->open(2) < 0) {
#ifdef _DEBUG
							AfxMessageBox("errore open() audio2");
#else
							debug_print(CLogFile::flagError,"errore open() audio2");
#endif
//							break;
							}
						}

					}
				if(frameNum < startFrame || (Param.doubleSpeed && (frameNum % Param.doubleSpeed))) {
					if(fr.lay == 3)
						setPointer(512);
					continue;
					}
				if(numFrames>0)
					numFrames--;

				if(!(frameNum % 16)) {
					CVidsendView22 *v;
					v=(CVidsendView22*)tag;
					if(v)
						v->mp3NotifyCallback(this,3,frameNum,0);
					// i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
					// 45000 e' un liv. basso, 55000 e' normale, 58-60000 è forte
					// i frame contati con ReadFrame sono la metà di quelli in totFrames (che corrispondono a WinAmp!) SISTEMARE!
					}

				retVal=1;
				playFrame(init);
				init = 0;

				if(Param.verbose>=3)
					debug_print(CLogFile::flagInfo,"\r{%5u}",frameNum);
				if(clip > 0 && Param.checkRange)
					debug_print(CLogFile::flagInfo,"%d samples clipped", clip);
				frameNum++;
paused:
				;
				}			// while
			CloseStream();
			status=FALSE;
			if(!Param.quiet)
#ifdef _DEBUG
				AfxMessageBox("Decoding finished.");
#else
				debug_print(CLogFile::flagInfo,"Decoding finished.");
#endif
			}
		Flush();

		if(ai)
			ai->close(intflag);
		if(ai2)
			ai2->close(intflag);
		delete bufPtr;
		bufPtr=NULL;

fine:
		delete conv16to8_buf;
		conv16to8_buf=NULL;
		delete pcmSample;
		pcmSample=NULL;
		delete ai;
		ai=NULL;
		delete ai2;
		ai2=NULL;
		if(theEqualizer) {
			delete theEqualizer;
			theEqualizer=NULL;
			}

		from=to=0;
	//		*title=0;
		currFrame=totFrames=totSec=0;
		bBusy=FALSE;

//						debug_print(CLogFile::flagInfo,"  esce suonaDX.");

//						Sleep(1000);
		
		return retVal;
		}
	else
		return 0;
	}
	
int CJoshuaMP3::initSuonaUDP(BYTE *outbuffer) {
  int i;
	char myBuf[256];
  int result, clip=0;
	BYTE stereo;
  int crc_error_count, total_error_count;
  DWORD l,old_crc;
  DWORD secdiff;



	minSample=maxSample=32768;

	Param.forceVolume= 1.4;

	Param.force8bit=0;
	Param.outmode = DECODE_BUFFER;
	bufPtr=outbuffer;
	bufSize=0;
	status=TRUE;
	Param.quiet=1;
	Param.tryResync=1;
	Param.verbose=2;

	theEqualizer=new CJoshuaMP3Equalizer;
	if(theEqualizer)
		theEqualizer->init(Param.equalizer=	/* inutilizzato ma ok*/CJoshuaMP3Equalizer::passaFlat,Param.forceVolume);
  init=initOutputDone=0;

  fr.single = -1;   // both channels 
  fr.synth = synth_1to1;
  fr.downSample = 0;

	setSynthFunctions();

	totSec=0;
	bPaused=0;
	intflag=0;

	readFrameInit();		// importante qua E dopo!
	makeDecodeTables(outscale);
	InitLayer2();		// inits also shared tables with layer1 
	InitLayer3(SBLIMIT >> fr.downSample);			// DARIO


	bBusy=TRUE;
	status=TRUE;
		

	readFrameInit();
	frameNum=0;
		
	return 1;
	}

int CJoshuaMP3::SuonaUDP(const BYTE *data,BYTE *outbuffer) {
	int retVal=-1;
  int i;
	char myBuf[256];
  int result, clip=0;
	BYTE stereo;
  int crc_error_count, total_error_count;
  DWORD l,old_crc;
  DWORD secdiff;


	bBusy=TRUE;
	status=TRUE;
	bufPtr=outbuffer;
	bufSize=0;
		

			readFrameInit();
			frameNum=0;

				if(readFrame() <= 0) {	 // 0: fine, -1: errore
//          debug_print(CLogFile::flagError2,"mpg123:audio flush");
					goto fine;
					}
				stereo = fr.stereo;
				crc_error_count   = 0;
				total_error_count = 0;



				retVal=1;
				playFrame(init);
				init = 0;

				if(Param.verbose>=3)
					debug_print(CLogFile::flagInfo,"\r{%5u}",frameNum);
				if(clip > 0 && Param.checkRange)
					debug_print(CLogFile::flagInfo,"%d samples clipped", clip);
				frameNum++;

		
		return retVal;

fine:
		return 0;
	}
	
int CJoshuaMP3::endSuonaUDP() {

	bBusy=FALSE;
	status=FALSE;

	return 1;
	}


#if 0
int CJoshuaMP3::SuonaWav(const char *fname, DWORD nFrom, DWORD nTo, int dnSample, int doMix, HWAVEOUT hDevice, BOOL bUI) {
	char myBuf[256];
	DWORD l;
	CFile mF;
	CFileStatus cfst;

	if(dnSample) {
		}
	if(doMix) {

		}

	from=nFrom;
	to=nTo;
	Param.quiet=1;
	Param.verbose=0;
	bBusy=TRUE;
	bPaused=0;
	intflag=0;
	ai=new CMP3Audio(this,hDevice,CMP3Audio::AUDIO_FORMAT_UNSIGNED_16 /*AUDIO_FORMAT_ULAW_8*/,2,44100,CMP3Audio::AUDIO_BUF_SIZE,Param.verbose);
	// parametrizzare!!

	bBusy=TRUE;
  if(OpenStream(fname)) {
		retVal=0;
		status=TRUE;
    if(!Param.quiet) {
      wsprintf(myBuf,"Playing WAV stream from %s ...", fname);
#ifdef _DEBUG
	      AfxMessageBox(myBuf);
#else
				debug_print(CLogFile::flagInfo,myBuf);
#endif
			}
		if(ai->open(ai->perif) < 0) {
#ifdef _DEBUG
			AfxMessageBox("errore open() audio");
#else
			debug_print(CLogFile::flagError,"errore open() audio");
#endif
		//      perror("audio");
			goto fine;
			}

		{
			CFile myF(reader->filept);
			l=CVidsendApp::getWAVinfo(&myF,NULL,&ai->wf);
		pcmPoint=0;

		}

		status=TRUE;

		if(from) {
			from *= ai->wf.nAvgBytesPerSec;
			_llseek(reader->filept,from,FILE_BEGIN);
			}
		if(to) 
			to *= ai->wf.nAvgBytesPerSec;

		do {
			ReadFile((HANDLE)reader->filept,pcmSample,CMP3Audio::AUDIO_BUF_SIZE*2,&l,NULL);
			// pcmSample è fatto di short...
			playSamples(pcmSample,CMP3Audio::AUDIO_BUF_SIZE);
			if(to) {
				if(_llseek(reader->filept,0,FILE_CURRENT) > to)
					break;
				}
			} while(l==CMP3Audio::AUDIO_BUF_SIZE*2);

    CloseStream();
		status=FALSE;
	  Flush();
		ai->close(0);

    if(!Param.quiet)
#ifdef _DEBUG
      AfxMessageBox("Decoding finished.");
#else
			debug_print(CLogFile::flagInfo,"Decoding finished.");
#endif
		;
		}
fine:	
	bBusy=FALSE;
//	SetThreadPriority(hThread,THREAD_PRIORITY_HIGHEST);
  return 0;
	}
#endif

void CJoshuaMP3::stop() {
	DWORD l,ti;
	int i;

	if(bBusy)
		intflag=1;
	}

int CJoshuaMP3::pause(int mode,BOOL bFreeWavDevice) {
	int oldPaused=bPaused;

	if(mode < 0) {
		bPaused &= mode;
		}
	else {
		bPaused |= mode;
		}
	if(bPaused) {
		if(!oldPaused) {
			if(ai->hWaveOut && ai->hWaveOut!=((HWAVEOUT)-1)) {
				if(bFreeWavDevice) {
					while(ai->critical);
					ai->close(1);
					}
				}
			else {
				ai->m_pSoundPlayer->Stop();
				}
			}
		if(m_Dlg)
			m_Dlg->SetWindowText("MP3 - in pausa");
		}
	else {
		if(oldPaused) {
			if(ai->hWaveOut && ai->hWaveOut!=((HWAVEOUT)-1)) {
				if(bFreeWavDevice) {
					ai->setRate(freqs[fr.samplingFrequency] >> (fr.downSample));	 // patch, perche' audioClose resetta ai->rate...
					ai->open(ai->perif);
					}
				}
			else {
				ai->which=0;
				ai->m_pSoundPlayer->Play(1);
				}
			}
		if(m_Dlg)
			m_Dlg->SetWindowText("MP3");
		}
	return 1;
	}

int CJoshuaMP3::goTo(DWORD n,BYTE m) {		// 1=in secondi, 0=in frame
	int i;

	ai->flush();
	if(m)
		i=reader->skipBytes(n*40*fr.framesize-reader->tell());
	else
		i=reader->skipBytes(n*fr.framesize-reader->tell());
	frameNum=(reader->tell()/fr.framesize)/2;
	return i;
	}

int CJoshuaMP3::isPaused() {
	
	return bPaused;
	}


int CJoshuaMP3::tabsel_123[2][3][16] = {
		{ {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
			{0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
			{0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },
		{ {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
			{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
			{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} }
		};

DWORD CJoshuaMP3::freqs[] = { 44100,48000,32000,22050,24000,
	16000,11025,12000,8000 } ;

// generi (da WinAmp (C))
const char *CJoshuaMP3::generiMP3[]= { 
	"Blues",
	"Classic Rock",
	"Country",
	"Dance",
	"Disco",
	"Funk",
	"Grunge",
	"Hip-Hop",
	"Jazz",
	"Metal",
	"New Age",
	"Oldies",
	"Other",
	"Pop",
	"R&B",
	"Rap",
	"Reggae",
	"Rock",
	"Techno",
	"Industrial",
	"Alternative",
	"Ska",
	"Death Metal",
	"Pranks",
	"Soundtrack",
	"Euro-Techno",
	"Ambient",
	"Trip Hop",
	"Vocal",
	"Jazz+Funk",
	"Fusion",
	"Trance",
	"Classical",
	"Instrumental",
	"Acid",
	"House",
	"Game",
	"Sound Clip",
	"Gospel",
	"Noise",
	"Alt.Rock",
	"Bass",
	"Soul",
	"Punk",
	"Space",
	"Meditative",
	"Instrumental Pop",
	"Instrumental Rock",
	"Ethnic",
	"Gothic",
	"Darkwave",
	"Techno-Industrial",
	"Electronic",
	"Pop-Folk",
	"Eurodance",
	"Dream",
	"Southern Rock",
	"Comedy",
	"Cult",
	"Gangsta Rap",
	"Top 40",
	"Christian Rap",
	"Pop/Punk",
	"Jungle",
	"Native American",
	"Cabaret",
	"New Wave",
	"Phychedelic",
	"Rave",
	"Showtunes",
	"Trailer",
	"Lo-Fi",
	"Tribal",
	"Acid Punk",
	"Acid Jazz",
	"Polka",
	"Retro",
	"Musical",
	"Rock & Roll",
	"Hard Rock",
	"Folk",
	"Folk/Rock",
	"National Folk",
	"Swing",
	"Fast-Fusion",
	"Bebop",
	"Latin",
	"Revival",
	"Celtic",
	"Blue Grass",
	"Avantegarde",
	"Gothic Rock",
	"Progressive Rock",
	"Psychedelic Rock",
	"Symphonic Rock",
	"Slow Rock",
	"Big Band",
	"Chorus",
	"Easy Listening",
	"Acoustic",
	"Humour",			// 100
	"Speech",
	"Chanson",
	"Opera",
	"Chamber Music",
	"Sonata",
	"Symphony",
	"Booty Bass",
	"Primus",
	"Porn Groove",
	"Satire",
	"Slow Jam",
	"Club",
	"Tango",
	"Samba",
	"Folklore",
	"Ballad",
	"Power Ballad",
	"Rhythmic Soul",
	"Freestyle",
	"Duet",
	"Punk Rock",
	"Drum Solo",
	"A Capella",
	"Euro-House",
	"Dance Hall",
	"Goa",
	"Drum & Bass",
	"Club-House",
	"Hardcore",
	"Terror",
	"Indie",
	"Brit Pop",
	"Negerpunk",
	"Polsk Punk",
	"Beat",
	"Christian Gangsta Rap",
	"Heavy Metal",
	"Black Metal",
	"Crossover",
	"Contemporary Christian",
	"Christian Rock",
	"Merengue",
	"Salsa",
	"Trash Metal",
	"Anime",
	"JPop",
	"Synth Pop",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Podcast"		//186 da Winamp
	};			//

CJoshuaMP3::CJoshuaMP3(const char *canzone,BOOL bUI) {

	_init((DWORD)0);
	if(canzone)
//		suona(canzone,0,0,0,0,NULL,CJoshuaMP3Equalizer::passaFlat,100,bUI);
		SuonaDX(canzone,0,0,0,NULL,CJoshuaMP3Equalizer::passaFlat,1,bUI);
	}

CJoshuaMP3::CJoshuaMP3(DWORD n) {

	_init(n);
	}

void CJoshuaMP3::_init(DWORD n) {
	register int i,j,k;

	tag=n;

	Param.equalizer=0;
	Param.aggressive=0;
	Param.shuffle=0;
	Param.remote=0;
	Param.outmode=-1;		// non valido!
	Param.quiet=1;
	Param.tryResync=0;
	Param.verbose=0;
	Param.forceMono=Param.forceStereo=Param.force8bit=Param.forceRate=0;
	Param.downSample=0;
	Param.checkRange=0;
	Param.doubleSpeed=Param.halfSpeed=0;
	Param.pitch=1.0;
	Param.forceReopen=0;
	Param.realtime=0;
	Param.forceVolume=1.0;
	Param.usebuffer=0;
	*Param.filename=0;
	Param.long_id3=0;
	Param.listentry=0;
	Param.listname=0;
	minSample=maxSample=0;
	ZeroMemory(grp_3tab,sizeof(grp_3tab));
	ZeroMemory(grp_5tab,sizeof(grp_5tab));
	ZeroMemory(grp_9tab,sizeof(grp_9tab));
	outscale=32768;
	pcmPoint=0;		// occhio, metterlo altrove!
	frameNum=0;
	startFrame=0;
	forceFrequency=-1;
	numFrames=-1;
	bufPtr=NULL;
	ZeroMemory(bsspace,sizeof(bsspace));
	bsbuf=bsspace[1];
	bsbufold=NULL;
	bsnum=0;
	bitreservoir=0;
	bufSize=0;
	oldhead=firsthead=0;
	ssize=0;
	ZeroMemory(&bsi,sizeof(bsi));
	fsizeold=0;
	totSec=0;
	ZeroMemory(&fr,sizeof(struct FRAME));
	ntom_val[0]=ntom_val[1]=NTOM_MUL >> 1;
	ntom_step=NTOM_MUL;
	COS6_1=COS6_2=0;				// non so se sarebbe il caso di azzerarle tutte ste var...
	ZeroMemory(COS9,sizeof(COS9));

	ZeroMemory(block,sizeof(block));
  ZeroMemory(blc,sizeof(blc));
	ZeroMemory(buffs,sizeof(buffs));
	ZeroMemory(ssave,sizeof(ssave));

	conv16to8=NULL;
	conv16to8_buf=NULL;
	pcmSample=NULL;

  init=initOutputDone=0;
	reader=NULL;
	ai=ai2=NULL;
	theEqualizer=NULL;
	from=to=0;
//	*title=0;
	currFrame=totFrames=0;
	frameNum=-1;
	m_Dlg=NULL;
	bBusy=0;
	status=0;
	bPaused=0;
	intflag=0;
	ZeroMemory(&ID3,sizeof(ID3));
  halfPhase=0;

	buffermem=NULL;

	lastscale = -1; // last used scale 
	rva_level[0] = -1; rva_level[1] = -1; // significance level of stored rva 
	rva_gain[0] = 0; rva_gain[1] = 0; // mix, album 
	rva_peak[0] = 0; rva_peak[1] = 0;

	vbr = CBR; // variable bitrate flag 
	abr_rate = 0;

	mean_framesize=0;
	mean_frames=0;
	do_recover=0;
	track_frames = 0;

  step = 2;
  bo = 1;

	}

CJoshuaMP3::~CJoshuaMP3() {

	delete m_Dlg;
	m_Dlg=NULL;

	delete conv16to8_buf;
	conv16to8_buf=NULL;
	delete pcmSample;
	pcmSample=NULL;
	delete ai;
	ai=NULL;
	delete ai2;
	ai2=NULL;
	if(theEqualizer) {
		delete theEqualizer;
		theEqualizer=NULL;
		}

	delete reader;
	reader=NULL;
	}


/////////////////////////////////////////////////////////////////////////////
// CMP3Status dialog
#if 0

CMP3Status::CMP3Status(CJoshuaMP3 *mp3,CWnd* pParent)
	: CDialog(CMP3Status::IDD, pParent), m_MP3(mp3) {

	hMixer=NULL;

	Create(IDD_MP3);

	//{{AFX_DATA_INIT(CMP3Status)
	m_To = _T("");
	m_Title = _T("");
	m_Mono = FALSE;
	//}}AFX_DATA_INIT
	}


void CMP3Status::DoDataExchange(CDataExchange* pDX){

	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMP3Status)
	DDX_Control(pDX, IDC_BUTTON0, m_Stop);
	DDX_Control(pDX, IDC_SLIDER2, m_Volume);
	DDX_Control(pDX, IDC_BUTTON2, m_Pause);
	DDX_Control(pDX, IDC_SLIDER1, m_Slider);
	DDX_Text(pDX, IDC_TEXT2, m_To);
	DDX_Text(pDX, IDC_TEXT1, m_Title);
	DDX_Check(pDX, IDC_CHECK1, m_Mono);
	//}}AFX_DATA_MAP
	}


BEGIN_MESSAGE_MAP(CMP3Status, CDialog)
	//{{AFX_MSG_MAP(CMP3Status)
	ON_BN_CLICKED(IDC_BUTTON0, OnButton0)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_BN_CLICKED(IDC_BUTTON2, OnButton2)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_CHECK1, OnCheck1)
	ON_BN_CLICKED(IDC_BUTTON3, OnButton3)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMP3Status message handlers


BOOL CMP3Status::OnInitDialog() {
	MIXERLINE mxl;
	MIXERCONTROL mxc;
	MIXERLINECONTROLS mxlc;
	MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;
	MIXERCONTROLDETAILS mxcd;
	int i;

	CDialog::OnInitDialog();

	// get the number of mixer devices present in the system
	i=mixerGetNumDevs();

	hMixer = NULL;
	m_volumeControlID = NULL;

	// open the first mixer
	if(i != 0) {
		if(mixerOpen(&hMixer,0,NULL,NULL,MIXER_OBJECTF_MIXER) != MMSYSERR_NOERROR)
			goto fine;

		// get dwLineID
		mxl.cbStruct = sizeof(MIXERLINE);
		mxl.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT;
		if(mixerGetLineInfo((HMIXEROBJ)hMixer,&mxl,MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE) != MMSYSERR_NOERROR)
			return FALSE;

		// get dwControlID
		mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
		mxlc.dwLineID = mxl.dwLineID;
		mxlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
		mxlc.cControls = 1;
		mxlc.cbmxctrl = sizeof(MIXERCONTROL);
		mxlc.pamxctrl = &mxc;
		if(mixerGetLineControls((HMIXEROBJ)hMixer,&mxlc,MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE) != MMSYSERR_NOERROR)
			return FALSE;

		// save record dwControlID
		m_Volume.SetRange(mxc.Bounds.dwMinimum,mxc.Bounds.dwMaximum,TRUE);
		m_volumeControlID = mxc.dwControlID;
		mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
		mxcd.dwControlID = m_volumeControlID;
		mxcd.cChannels = 1;
		mxcd.cMultipleItems = 0;
		mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
		mxcd.paDetails = &mxcdVolume;
		if(mixerGetControlDetails((HMIXEROBJ)hMixer,&mxcd,MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE) != MMSYSERR_NOERROR)
			goto fine;
	
		m_Volume.SetPos(mxcdVolume.dwValue);
		}

fine:	

	{
//https://www.codeproject.com/Questions/1257202/How-to-set-an-icon-to-a-dialog-in-VC6
		CButton *m_btn3=(CButton*)GetDlgItem(IDC_BUTTON3);
		m_btn3->SetBitmap(LoadBitmap(AfxGetInstanceHandle(),MAKEINTRESOURCE(IDB_SPEAKER)));

//https://stackoverflow.com/questions/632919/how-to-make-an-icon-button-in-c
//m_btn3->SetIcon( ::LoadIcon(NULL, IDI_SPEAKER) ); 
	}

	RECT rc={125,300,525,440};
	MoveWindow(&rc);

	return TRUE;  // return TRUE unless you set the focus to a control
	}


CMP3Status::~CMP3Status() {	 // perche' non passa di qua??? dà Warning in debugger...
	
	if(hMixer)
		mixerClose(hMixer);
//		AfxMessageBox("mixer closed");		// non e' vero... ci passa! 2/2/01
	DestroyWindow(); 
	}

void CMP3Status::OnButton0() {
	
	m_MP3->stop();
	}

void CMP3Status::OnButton1() {
	CMenu myMenu;
	POINT point;

	GetCursorPos(&point);
	myMenu.CreatePopupMenu();

	myMenu.AppendMenu(MF_STRING,0,m_MP3->ID3.titolo);
	myMenu.AppendMenu(MF_STRING,0,m_MP3->ID3.autore);
	myMenu.AppendMenu(MF_STRING,0,m_MP3->ID3.anno);
	myMenu.AppendMenu(MF_SEPARATOR);
	myMenu.AppendMenu(MF_STRING,0,m_MP3->getFilename());
	myMenu.AppendMenu(MF_SEPARATOR);
	myMenu.AppendMenu(MF_STRING,1,"Apri con Google...");
	int i=myMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,point.x,point.y,this);
	if(i==1) {
		SHELLEXECUTEINFO sei;
		CString m_strURL;

		::ZeroMemory(&sei,sizeof(SHELLEXECUTEINFO));
		sei.cbSize = sizeof( SHELLEXECUTEINFO );		// Set Size
		sei.lpVerb = TEXT( "open" );					// Set Verb
		m_strURL="http://www.google.it/search?hl=it&q=";
	//	m_strURL+="broken+strings+james+morrison";
		m_strURL+=translateString(m_MP3->ID3.titolo);
		m_strURL+='+';
		m_strURL+=translateString(m_MP3->ID3.autore);
		m_strURL+="&btnG=Cerca+con+Google&meta=";
		sei.lpFile = m_strURL;							// Set Target To Open
		sei.nShow = SW_SHOWNORMAL;						// Show Normal

		ShellExecuteEx(&sei);
		}
	}

void CMP3Status::OnButton3() {
	
	theApp.OnCasanetAudioVolume();
	}

CString CMP3Status::translateString(const char *s) {		// v. CWebCliSocket
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

void CMP3Status::OnButton2() {

	if(m_MP3->isPaused())
		m_MP3->pause(~1);
	else
		m_MP3->pause(1);
	}

void CMP3Status::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
	
	if((CSliderCtrl *)pScrollBar == &m_Volume) {
		if(m_MP3->ai->hWaveOut && m_MP3->ai->hWaveOut!=((HWAVEOUT)-1)) {
			MIXERCONTROLDETAILS mxcd;
			MIXERCONTROLDETAILS_UNSIGNED mx1;
			if(hMixer) {
				mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
				mxcd.dwControlID = m_volumeControlID;
				mxcd.cChannels = 1;
				mxcd.cMultipleItems = 0;
				mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
//				theApp.Volumi[CSound::WAVE_CURSOR][0]=theApp.Volumi[CSound::WAVE_CURSOR][1]=mx1.dwValue=m_Volume.GetPos();
				mxcd.paDetails=&mx1;
				mixerSetControlDetails((HMIXEROBJ)hMixer,&mxcd,MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE);
				}
			}
		else {
			m_MP3->HWvolume((signed char)0,m_Volume.GetPos());
			}
		}
	else {
		if(m_MP3 && m_MP3->isBusy())
			m_MP3->goTo((m_Slider.GetPos()*10)/60,1);	// da frames a secondi
//		 usare qualcosa tipo timeToFrame()!!!
		}
	
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
	}

void CMP3Status::OnCheck1() {	

	CString S;
	S.Format("single=%d",m_MP3->fr.single);
		m_MP3->fr.single=m_Mono ? 3 : 0;
	
	}

#endif

