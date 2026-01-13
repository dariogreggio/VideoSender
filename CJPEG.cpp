#include "stdafx.h"
#include "cjpeg.h"
#include <setjmp.h>


// Adattato dall'Independent JPEG Group's software da Dario Greggio
// 22->30/4/2000
// 13->18/8/2003
// Exif: 29-31/12/2005 (default Exif su colore YCbCr, JFIF su Grayscale); 2010
// estensioni 16:9 2016 GC
// aggiunta (compilazione) decompressed Progressive JPG, 9.17 (la morte a chirin perisic)
// gen 2021: uso di HeapAlloc ecc, messo const in m_parent


jmp_buf exitEnv;


#define CONST_BITS  8		// v. anche sotto!

/*
 * cdjpeg.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains common support routines used by the IJG application
 * programs (cjpeg, djpeg, jpegtran).
 */

#include <ctype.h>		/* to declare isupper(), tolower() */
#ifdef NEED_SIGNAL_CATCHER
#include <signal.h>		/* to declare signal() */
#endif
#ifdef USE_SETMODE
#include <fcntl.h>		/* to declare setmode()'s parameter macros */
/* If you have setmode() but not <io.h>, just delete this line: */
#include <io.h>			/* to declare setmode() */
#endif


/*
 * Signal catcher to ensure that temporary files are removed before aborting.
 * NB: for Amiga Manx C this is actually a global routine named _abort();
 * we put "#define signal_catcher _abort" in jconfig.h.  Talk about bogus...
 */

#ifdef NEED_SIGNAL_CATCHER

static J_COMMON_PTR sig_cinfo;

void signal_catcher(int signum) {/* must be global for Manx C */
  if(sig_cinfo != NULL) {
    if(sig_cinfo->err != NULL) /* turn off trace output */
      sig_cinfo->err->trace_level = 0;
    jpeg_destroy(sig_cinfo);	/* clean up memory allocation & temp files */
		}
9  exit(EXIT_FAILURE);
	}


void enable_signal_catcher(J_COMMON_PTR cinfo) {
  sig_cinfo = cinfo;
#ifdef SIGINT			/* not all systems have SIGINT */
  signal(SIGINT, signal_catcher);
#endif
#ifdef SIGTERM			/* not all systems have SIGTERM */
  signal(SIGTERM, signal_catcher);
#endif
	}

#endif


/*
 * Optional progress monitor: display a percent-done figure on stderr.
 */

#ifdef PROGRESS_REPORT

void J_PROGRESS_MGR::progressMonitor() {
  int total_passes = totalPasses + totalExtraPasses;
  int percent_done = (int)(passCounter*100L/passLimit);

  if(percent_done != percentDone) {
    percentDone = percent_done;
    if(total_passes > 1) {
      fprintf(stderr, "\rPass %d/%d: %3d%% ",
	      completedPasses + completedExtraPasses + 1,
	      total_passes, percent_done);
			}
		else {
      fprintf(stderr, "\r %3d%% ", percent_done);
			}
    fflush(stderr);
		}
	}


J_PROGRESS_MGR::J_PROGRESS_MGR(const CJpeg *p) : m_Parent(p) {

  // Enable progress display, unless trace output is on
  if(m_Parent->err->traceLevel == 0) {
    completedExtraPasses = 0;
    totalExtraPasses = 0;
    percentDone = -1;
		}
	}


void J_PROGRESS_MGR::endProgressMonitor() {

  // Clear away progress display 
  if(m_Parent->err->traceLevel == 0) {
    fprintf(stderr, "\r                \r");
    fflush(stderr);
		}
	}

#endif



/*
 * Routines to establish binary I/O mode for stdin and stdout.
 * Non-Unix systems often require some hacking to get out of text mode.
 */

FILE *readStdin() {
  FILE *inputFile = stdin;

#ifdef USE_SETMODE		// need to hack file mode?
  setmode(fileno(stdin),O_BINARY);
#endif
#ifdef USE_FDOPEN		// need to re-open in binary mode?
  if((inputFile = fdopen(fileno(stdin), READ_BINARY)) == NULL) {
    fprintf(stderr,"Cannot reopen stdin\n");
    exit(EXIT_FAILURE);
		}
#endif
  return inputFile;
	}


FILE *writeStdout() {
  FILE *outputFile = stdout;

#ifdef USE_SETMODE		// need to hack file mode? 
  setmode(fileno(stdout),O_BINARY);
#endif
#ifdef USE_FDOPEN		// need to re-open in binary mode? 
  if((outputFile = fdopen(fileno(stdout), WRITE_BINARY)) == NULL) {
    fprintf(stderr, "Cannot reopen stdout\n");
    exit(EXIT_FAILURE);
		}
#endif
  return outputFile;
	}


/*
 * jdatadst.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains compression data destination routines for the case of
 * emitting JPEG data to a file (or any stdio stream).  While these routines
 * are sufficient for most applications, some will want to use a different
 * destination manager.
 * IMPORTANT: we assume that fwrite() will correctly transcribe an array of
 * JOCTETs into 8-bit-wide elements on external storage.  If char is wider
 * than 8 bits on your machine, you may need to do some tweaking.
 */

CDestMgr::CDestMgr(const CJpeg *p,const char *nomefile) :	m_Parent(p) {

	// Allocate the output buffer --- it will be released when done with image
	buffer=(JOCTET *)m_Parent->mem->allocSmall(JPOOL_IMAGE,OUTPUT_BUF_SIZE *sizeof(JOCTET));
	totalBuffer=OUTPUT_BUF_SIZE;
	outfile=new CFile;
	outputMode=OUT_TO_NEWFILE;
	if(!(outfile->Open(nomefile, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))) {
		fprintf(stderr, "can't open %s\n",nomefile);
		m_Parent->err->errorExit(JERR_FILE_WRITE);
		}
	}

CDestMgr::CDestMgr(const CJpeg *p,CFile *f) :	m_Parent(p) {

	// Allocate the output buffer --- it will be released when done with image
	buffer=(JOCTET *)m_Parent->mem->allocSmall(JPOOL_IMAGE,OUTPUT_BUF_SIZE *sizeof(JOCTET));
	totalBuffer=OUTPUT_BUF_SIZE;
	outfile=f;
	outputMode=OUT_TO_FILE;
	}

CDestMgr::CDestMgr(const CJpeg *p,BYTE *d,DWORD buflen) :	m_Parent(p) {
	
	outfile=NULL;
  buffer=d;
	totalBuffer=buflen;
	outputMode=OUT_TO_MEM;
	}


CDestMgr::~CDestMgr() {
	
	switch(outputMode) {
		case OUT_TO_NEWFILE:
			outfile->Close();
			delete outfile;
		case OUT_TO_FILE:
			outfile=NULL;
			if(buffer)
				freeSmall(buffer,0);
		case OUT_TO_MEM:
			buffer=NULL;
			break;
		}
	}



/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

void CDestMgr::initDestination() {

  nextOutputByte=buffer;
  freeInBuffer=totalBuffer;
	}


/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

BOOL CDestMgr::emptyOutputBuffer() {

  if(outfile)
		outfile->Write(buffer, totalBuffer);
// TRY...
//    m_Parent->err->errorExit(JERR_FILE_WRITE);
	else
// con l'array non ha senso emptyOutput!!
		m_Parent->err->errorExit(JERR_FILE_WRITE);

  nextOutputByte=buffer;
  freeInBuffer=totalBuffer;

  return TRUE;
	}


/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

void CDestMgr::termDestination() {

	dataCount = totalBuffer-freeInBuffer;
  if(dataCount > 0) {
		if(outfile) {
  // Write any data remaining in the buffer
			outfile->Write(buffer,dataCount);
// TRY...      m_Parent->err->errorExit(JERR_FILE_WRITE);
			}
		}
	if(outfile) {
	  outfile->Flush();
  // Make sure we wrote the output file OK 
// usare TRY...
	/*  if(ferror(outfile))
    m_Parent->err->errorExit(JERR_FILE_WRITE);*/
		}

	// v. anche distruttore!
	}



/*
 * jdatasrc.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains decompression data source routines for the case of
 * reading JPEG data from a file (or any stdio stream).  While these routines
 * are sufficient for most applications, some will want to use a different
 * source manager.
 * IMPORTANT: we assume that fread() will correctly transcribe an array of
 * JOCTETs from 8-bit-wide elements on external storage.  If char is wider
 * than 8 bits on your machine, you may need to do some tweaking.
 */



CSourceMgr::CSourceMgr(const CJpeg *p,CFile *f) :	m_Parent(p) {

  buffer = (JOCTET *)(m_Parent->mem->allocSmall) (JPOOL_PERMANENT,
	  INPUT_BUF_SIZE *sizeof(JOCTET));

	totalBuffer=INPUT_BUF_SIZE;
	
  bytesInBuffer = 0;		// forces fill_input_buffer on first read
  nextInputByte = NULL; // until buffer loaded
	
	inputMode=IN_FROM_FILE;
	infile=f;
	startOfFile=0;
	}

CSourceMgr::CSourceMgr(const CJpeg *p,const char *nomefile) :	m_Parent(p)	{

  buffer = (JOCTET *)(m_Parent->mem->allocSmall) (JPOOL_PERMANENT,
	  INPUT_BUF_SIZE *sizeof(JOCTET));
	
  bytesInBuffer = 0;		// forces fill_input_buffer on first read
  nextInputByte = NULL; // until buffer loaded
	
	infile=new CFile;
	totalBuffer=INPUT_BUF_SIZE;
	inputMode=IN_FROM_NEWFILE;
	if(!(infile->Open(nomefile, CFile::modeRead | CFile::typeBinary))) {
		fprintf(stderr, "can't open %s\n",nomefile);
		m_Parent->err->errorExit(JERR_FILE_READ);
		}
	startOfFile=0;
	}

CSourceMgr::CSourceMgr(const CJpeg *p,BYTE *d,DWORD buflen) :	m_Parent(p)	{

  buffer = d;
	totalBuffer=buflen;

  bytesInBuffer = 0;		// forces fill_input_buffer on first read
  nextInputByte = NULL; // until buffer loaded
	
	inputMode=IN_FROM_MEM;
	infile=NULL;

	startOfFile=0;
	// FINIRE in input!!
	}

CSourceMgr::~CSourceMgr() {

	switch(inputMode) {
		case IN_FROM_NEWFILE:
			infile->Close();
			delete infile;
		case IN_FROM_FILE:
			infile=NULL;
			if(buffer)
				freeSmall(buffer,0);
		case IN_FROM_MEM:
			buffer=NULL;
			break;
		}
	}

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */

void CSourceMgr::initSource() {

  /* We reset the empty-input-file flag for each image,
   * but we don't clear the input buffer.
   * This is correct behavior for reading a series of images from one source.
   */
  startOfFile = TRUE;
	}


/*
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * In typical applications, this should read fresh data into the buffer
 * (ignoring the current state of next_input_byte & bytes_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been reloaded.  It is not necessary to
 * fill the buffer entirely, only to obtain at least one more byte.
 *
 * There is no such thing as an EOF return.  If the end of the file has been
 * reached, the routine has a choice of ERREXIT() or inserting fake data into
 * the buffer.  In most cases, generating a warning message and inserting a
 * fake EOI marker is the best course of action --- this will allow the
 * decompressor to output however much of the image is there.  However,
 * the resulting error message is misleading if the real problem is an empty
 * input file, so we handle that case specially.
 *
 * In applications that need to be able to suspend compression due to input
 * not being available yet, a FALSE return indicates that no more data can be
 * obtained right now, but more may be forthcoming later.  In this situation,
 * the decompressor will return to its caller (with an indication of the
 * number of scanlines it has read, if any).  The application should resume
 * decompression after it has loaded more data into the input buffer.  Note
 * that there are substantial restrictions on the use of suspension --- see
 * the documentation.
 *
 * When suspending, the decompressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_input_byte & bytes_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point must be rescanned after resumption, so move it to
 * the front of the buffer rather than discarding it.
 */

BOOL CSourceMgr::fillInputBuffer() {

  size_t nbytes;

  nbytes = infile->Read(buffer, INPUT_BUF_SIZE);

  if(nbytes <= 0) {
    if(startOfFile)	/* Treat empty input file as fatal error */
      m_Parent->err->errorExit(JERR_INPUT_EMPTY);
//    WARNMS(cinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
    buffer[0] = (JOCTET) 0xFF;
    buffer[1] = (JOCTET) CMarkerReader::M_EOI;
    nbytes = 2;
		}

  nextInputByte = buffer;
  bytesInBuffer = nbytes;
  startOfFile = FALSE;

  return TRUE;
	}


/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 * Writers of suspendable-input applications must note that skip_input_data
 * is not granted the right to give a suspension return.  If the skip extends
 * beyond the data currently in the buffer, the buffer can be marked empty so
 * that the next read will cause a fill_input_buffer call that can suspend.
 * Arranging for additional bytes to be discarded before reloading the input
 * buffer is the application writer's problem.
 */

void CSourceMgr::skipInputData(long numBytes) {

  /* Just a dumb implementation for now.  Could use fseek() except
   * it doesn't work on pipes.  Not clear that being smart is worth
   * any trouble anyway --- large skips are infrequent.
   */
  if(numBytes > 0) {
    while(numBytes > (long) bytesInBuffer) {
      numBytes -= (long) bytesInBuffer;
      (void)fillInputBuffer();
      /* note we assume that fill_input_buffer will never return FALSE,
       * so suspension need not be handled.
       */
    }
    nextInputByte += (size_t)numBytes;
    bytesInBuffer -= (size_t)numBytes;
		}
	}


/*
 * An additional method that can be provided by data source modules is the
 * resync_to_restart method for error recovery in the presence of RST markers.
 * For the moment, this source module just uses the default resync method
 * provided by the JPEG library.  That method assumes that no backtracking
 * is possible.
 */


/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

void CSourceMgr::termSource() {
  /* no work necessary here */
	}







/*
 * IMAGE DATA FORMATS:
 *
 * The standard input image format is a rectangular array of pixels, with
 * each pixel having the same number of "component" values (color channels).
 * Each pixel row is an array of JSAMPLEs (which typically are unsigned chars).
 * If you are working with color data, then the color values for each pixel
 * must be adjacent in the row; for example, R,G,B,R,G,B,R,G,B,... for 24-bit
 * RGB color.
 *
 * For this example, we'll assume that this data structure matches the way
 * our application has stored the image in memory, so we can just pass a
 * pointer to our image buffer.  In particular, let's say that the image is
 * RGB color and is described by:
 */







 /*
 * jmemmgr.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the JPEG system-independent memory management
 * routines.  This code is usable across a wide variety of machines; most
 * of the system dependencies have been isolated in a separate file.
 * The major functions provided here are:
 *   * pool-based allocation and freeing of memory;
 *   * policy decisions about how to divide available memory among the
 *     virtual arrays;
 *   * control logic for swapping virtual arrays between main memory and
 *     backing storage.
 * The separate system-dependent file provides the actual backing-storage
 * access code, and it contains the policy decision about how much total
 * main memory to use.
 * This file is system-dependent in the sense that some of its functions
 * are unnecessary in some systems.  For example, if there is enough virtual
 * memory so that backing storage will never be used, much of the virtual
 * array control logic could be removed.  (Of course, if you have that much
 * memory then you shouldn't care about a little bit of unused code...)
 */

#define AM_MEMORY_MANAGER	/* we define jvirt_Xarray_control structs */


/*
 * Some important notes:
 *   The allocation routines provided here must never return NULL.
 *   They should exit to error_exit if unsuccessful.
 *
 *   It's not a good idea to try to merge the sarray and barray routines,
 *   even though they are textually almost the same, because samples are
 *   usually stored as bytes while coefficients are shorts or ints.  Thus,
 *   in machines where byte pointers have a different representation from
 *   word pointers, the resulting machine code could not be the same.
 */



#ifdef MEM_STATS		// optional extra stuff for statistics 

void CJMemoryMgr::printMemStats(int pool_id) {
  SMALL_POOL_PTR shdr_ptr;
  LARGE_POOL_PTR lhdr_ptr;

  /* Since this is only a debugging stub, we can cheat a little by using
   * fprintf directly rather than going through the trace message code.
   * This is helpful because message parm array can't handle longs.
   */
  fprintf(stderr, "Freeing pool %d, total space = %ld\n",
	  pool_id, mem->total_space_allocated);

  for(lhdr_ptr = mem->large_list[pool_id]; lhdr_ptr != NULL;
    lhdr_ptr = lhdr_ptr->hdr.next) {
    fprintf(stderr,"  Large chunk used %ld\n",
	    (long)lhdr_ptr->hdr.bytes_used);
		}

  for(shdr_ptr = mem->small_list[pool_id]; shdr_ptr != NULL;
      shdr_ptr = shdr_ptr->hdr.next) {
    fprintf(stderr,"  Small chunk used %ld free %ld\n",
	    (long)shdr_ptr->hdr.bytes_used,
	    (long)shdr_ptr->hdr.bytes_left);
		}
	}

#endif // MEM_STATS


void CJMemoryMgr::outOfMemory(int which) {

// Report an out-of-memory error and stop execution
// If we compiled MEM_STATS support, report alloc requests before dying
#ifdef MEM_STATS
  m_Parent->err->traceLevel=2;	// force self_destruct to report stats
#endif
  m_Parent->err->errorExit(JERR_OUT_OF_MEMORY, which);
	}


/*
 * Allocation of "small" objects.
 *
 * For these, we use pooled storage.  When a new pool must be created,
 * we try to get enough space for the current request plus a "slop" factor,
 * where the slop will be the amount of leftover space in the new pool.
 * The speed vs. space tradeoff is largely determined by the slop values.
 * A different slop value is provided for each pool class (lifetime),
 * and we also distinguish the first pool of a class from later ones.
 * NOTE: the values given work fairly well on both 16- and 32-bit-int
 * machines, but may be too small if longs are 64 bits or more.
 */

CJMemoryMgr::CJMemoryMgr(const CJpeg *p) :	m_Parent(p) {
	int pool;

	// Make CJMemoryMgr::MAX_ALLOC_CHUNK accessible to other modules 
  maxAllocChunk = CJMemoryMgr::MAX_ALLOC_CHUNK;
  for(pool=JPOOL_NUMPOOLS-1; pool >= JPOOL_PERMANENT; pool--) {
    smallList[pool] = NULL;
    largeList[pool] = NULL;
		}

	firstPoolSlop[0]=1600;	// first PERMANENT pool
	firstPoolSlop[1]=16000;	// first IMAGE pool
	extraPoolSlop[0]=0;			// additional PERMANENT pools
	extraPoolSlop[1]=5000;	// additional IMAGE pools
	}

CJMemoryMgrExt::CJMemoryMgrExt(const CJpeg *p) : CJMemoryMgr(p) {

	totalSpaceAllocated=0; 
	maxMemoryToUse=16000000UL;

	}



void *CJMemoryMgr::allocSmall(int pool_id, size_t sizeofobject) {
// Allocate a "small" object 
  SMALL_POOL_PTR hdrPtr, prevHdrPtr;
  char *dataPtr;
  size_t oddBytes, minRequest, slop;

  // Check for unsatisfiable request (do now to ensure no overflow below)
  if(sizeofobject > (size_t) (CJMemoryMgr::MAX_ALLOC_CHUNK-sizeof(SMALL_POOL_HDR)))
    outOfMemory(1);	// request exceeds malloc's ability

  // Round up the requested size to a multiple of SIZEOF(ALIGN_TYPE)
  oddBytes = sizeofobject % sizeof(ALIGN_TYPE);
  if(oddBytes > 0)
    sizeofobject += sizeof(ALIGN_TYPE) - oddBytes;

  // See if space is available in any existing pool 
  if(pool_id < 0 || pool_id >= JPOOL_NUMPOOLS)
    m_Parent->err->errorExit(JERR_BAD_POOL_ID, pool_id);	// safety check 
  prevHdrPtr = NULL;
  hdrPtr = smallList[pool_id];
  while(hdrPtr) {
    if(hdrPtr->hdr.bytesLeft >= sizeofobject)
      break;			// found pool with enough space 
    prevHdrPtr = hdrPtr;
    hdrPtr = hdrPtr->hdr.next;
		}

  // Time to make a new pool? 
  if(!hdrPtr) {
    // min_request is what we need now, slop is what will be leftover 
    minRequest = sizeofobject + sizeof(SMALL_POOL_HDR);
    if(!prevHdrPtr)	// first pool in class? 
      slop = firstPoolSlop[pool_id];
    else
      slop = extraPoolSlop[pool_id];
    // Don't ask for more than CJMemoryMgr::MAX_ALLOC_CHUNK 
    if(slop > (size_t)(CJMemoryMgr::MAX_ALLOC_CHUNK-minRequest))
      slop = (size_t)(CJMemoryMgr::MAX_ALLOC_CHUNK-minRequest);
    // Try to get space, if fail reduce slop and try again
    for(;;) {
      hdrPtr=(SMALL_POOL_PTR)getSmall(minRequest+slop);
      if(hdrPtr)
				break;
      slop /= 2;
      if(slop < MIN_SLOP)	// give up when it gets real small
				outOfMemory(2); // jpeg_get_small failed 
			}
    totalSpaceAllocated += minRequest + slop;
    // Success, initialize the new pool header and add to end of list
    hdrPtr->hdr.next = NULL;
    hdrPtr->hdr.bytesUsed = 0;
    hdrPtr->hdr.bytesLeft = sizeofobject + slop;
    if(!prevHdrPtr)	// first pool in class?
      smallList[pool_id]=hdrPtr;
    else
      prevHdrPtr->hdr.next=hdrPtr;
	  }

  // OK, allocate the object from the current pool 
  dataPtr = (char *)(hdrPtr + 1); // point to first data byte in pool 
  dataPtr += hdrPtr->hdr.bytesUsed; // point to place for object 
  hdrPtr->hdr.bytesUsed += sizeofobject;
  hdrPtr->hdr.bytesLeft -= sizeofobject;

  return (void *)dataPtr;
	}


/*
 * Allocation of "large" objects.
 *
 * The external semantics of these are the same as "small" objects,
 * except that FAR pointers are used on 80x86.  However the pool
 * management heuristics are quite different.  We assume that each
 * request is large enough that it may as well be passed directly to
 * jpeg_get_large; the pool management just links everything together
 * so that we can free it all on demand.
 * Note: the major use of "large" objects is in JSAMPARRAY and JBLOCKARRAY
 * structures.  The routines that create these structures (see below)
 * deliberately bunch rows together to ensure a large request size.
 */

void *CJMemoryMgr::allocLarge(int pool_id, size_t sizeofobject) {
// Allocate a "large" object 
  LARGE_POOL_PTR hdrPtr;
  size_t oddBytes;

  // Check for unsatisfiable request (do now to ensure no overflow below)
  if(sizeofobject > (size_t) (CJMemoryMgr::MAX_ALLOC_CHUNK-sizeof(LARGE_POOL_HDR)))
    outOfMemory(3);	// request exceeds malloc's ability

  // Round up the requested size to a multiple of SIZEOF(ALIGN_TYPE)
  oddBytes = sizeofobject % sizeof(ALIGN_TYPE);
  if(oddBytes > 0)
    sizeofobject += sizeof(ALIGN_TYPE)-oddBytes;

  // Always make a new pool
  if(pool_id < 0 || pool_id >= JPOOL_NUMPOOLS)
    m_Parent->err->errorExit(JERR_BAD_POOL_ID, pool_id);	// safety check

  hdrPtr=(LARGE_POOL_PTR)getLarge(sizeofobject + sizeof(LARGE_POOL_HDR));
  if(hdrPtr == NULL)
    outOfMemory(4);	// jpeg_get_large failed
  totalSpaceAllocated += sizeofobject + sizeof(LARGE_POOL_HDR);

  // Success, initialize the new pool header and add to list 
  hdrPtr->hdr.next = largeList[pool_id];
  /* We maintain space counts in each pool header for statistical purposes,
   * even though they are not needed for allocation.
   */
  hdrPtr->hdr.bytesUsed = sizeofobject;
  hdrPtr->hdr.bytesLeft = 0;
  largeList[pool_id] = hdrPtr;

  return (void *)(hdrPtr+1); // point to first data byte in pool
	}


/*
 * Creation of 2-D sample arrays.
 * The pointers are in near heap, the samples themselves in FAR heap.
 *
 * To minimize allocation overhead and to allow I/O of large contiguous
 * blocks, we allocate the sample rows in groups of as many rows as possible
 * without exceeding CJMemoryMgr::MAX_ALLOC_CHUNK total bytes per allocation request.
 * NB: the virtual array control routines, later in this file, know about
 * this chunking of rows.  The rowsperchunk value is left in the mem manager
 * object so that it can be saved away if this sarray is the workspace for
 * a virtual array.
 */

JSAMPARRAY CJMemoryMgr::allocSarray(int pool_id,
	JDIMENSION samplesperrow, JDIMENSION numrows) {
// Allocate a 2-D sample array 
  JSAMPARRAY result;
  JSAMPROW workspace;
  JDIMENSION rowsperchunk, currow, i;
  long ltemp;

  // Calculate max # of rows allowed in one allocation chunk
  ltemp = (CJMemoryMgr::MAX_ALLOC_CHUNK-sizeof(LARGE_POOL_HDR)) /
	  ((long) samplesperrow * sizeof(JSAMPLE));
  if(ltemp <= 0)
    m_Parent->err->errorExit(JERR_WIDTH_OVERFLOW);
  if(ltemp < (long) numrows)
    rowsperchunk = (JDIMENSION)ltemp;
  else
    rowsperchunk = numrows;
  last_rowsPerChunk = rowsperchunk;

  // Get space for row pointers (small object)
  result=(JSAMPARRAY)allocSmall(pool_id,
				    (size_t)(numrows * sizeof(JSAMPROW)));

  // Get the rows themselves (large objects) 
  currow=0;
  while(currow < numrows) {
    rowsperchunk = min(rowsperchunk, numrows - currow);
    workspace=(JSAMPROW)allocLarge(pool_id,
			(size_t) ((size_t) rowsperchunk * (size_t) samplesperrow * sizeof(JSAMPLE)));
    for(i=rowsperchunk; i > 0; i--) {
      result[currow++] = workspace;
      workspace += samplesperrow;
			}
		}

  return result;
	}


/*
 * Creation of 2-D coefficient-block arrays.
 * This is essentially the same as the code for sample arrays, above.
 */

JBLOCKARRAY CJMemoryMgr::allocBarray(int pool_id, JDIMENSION blocksperrow, JDIMENSION numrows) {
// Allocate a 2-D coefficient-block array
  JBLOCKARRAY result;
  JBLOCKROW workspace;
  JDIMENSION rowsperchunk, currow, i;
  long ltemp;

  // Calculate max # of rows allowed in one allocation chunk
  ltemp=(CJMemoryMgr::MAX_ALLOC_CHUNK-sizeof(LARGE_POOL_HDR)) /
	  ((long) blocksperrow * sizeof(JBLOCK));
  if(ltemp <= 0)
    m_Parent->err->errorExit(JERR_WIDTH_OVERFLOW);
  if(ltemp < (long) numrows)
    rowsperchunk = (JDIMENSION)ltemp;
  else
    rowsperchunk = numrows;
  last_rowsPerChunk = rowsperchunk;

  // Get space for row pointers (small object) 
  result=(JBLOCKARRAY)allocSmall(pool_id, (size_t)(numrows*sizeof(JBLOCKROW)));

  // Get the rows themselves (large objects) 
  currow = 0;
  while(currow < numrows) {
    rowsperchunk = min(rowsperchunk, numrows - currow);
    workspace = (JBLOCKROW)allocLarge(pool_id,
		(size_t) ((size_t) rowsperchunk * (size_t) blocksperrow
		  * sizeof(JBLOCK)));
    for(i=rowsperchunk; i > 0; i--) {
      result[currow++] = workspace;
      workspace += blocksperrow;
			}
		}

  return result;
	}


/*
 * About virtual array management:
 *
 * The above "normal" array routines are only used to allocate strip buffers
 * (as wide as the image, but just a few rows high).  Full-image-sized buffers
 * are handled as "virtual" arrays.  The array is still accessed a strip at a
 * time, but the memory manager must save the whole array for repeated
 * accesses.  The intended implementation is that there is a strip buffer in
 * memory (as high as is possible given the desired memory limit), plus a
 * backing file that holds the rest of the array.
 *
 * The request_virt_array routines are told the total size of the image and
 * the maximum number of rows that will be accessed at once.  The in-memory
 * buffer must be at least as large as the maxaccess value.
 *
 * The request routines create control blocks but not the in-memory buffers.
 * That is postponed until realize_virt_arrays is called.  At that time the
 * total amount of space needed is known (approximately, anyway), so free
 * memory can be divided up fairly.
 *
 * The access_virt_array routines are responsible for making a specific strip
 * area accessible (after reading or writing the backing file, if necessary).
 * Note that the access routines are told whether the caller intends to modify
 * the accessed strip; during a read-only pass this saves having to rewrite
 * data to disk.  The access routines are also responsible for pre-zeroing
 * any newly accessed rows, if pre-zeroing was requested.
 *
 * In current usage, the access requests are usually for nonoverlapping
 * strips; that is, successive access start_row numbers differ by exactly
 * num_rows = maxaccess.  This means we can get good performance with simple
 * buffer dump/reload logic, by making the in-memory buffer be a multiple
 * of the access height; then there will never be accesses across bufferload
 * boundaries.  The code will still work with overlapping access requests,
 * but it doesn't handle bufferload overlaps very efficiently.
 */


CVirtSArray *CMainControllerExt::requestVirtSarray(int pool_id, BOOL pre_zero,
	JDIMENSION samplesperrow, JDIMENSION numrows, JDIMENSION maxaccess) {
// Request a virtual 2-D sample array 
  CVirtSArray *result;

  // Only IMAGE-lifetime virtual arrays are currently supported 
  if(pool_id != JPOOL_IMAGE)
    m_Parent->m_Parent->err->errorExit(JERR_BAD_POOL_ID, pool_id);	// safety check

  // get control block
  result=new CVirtSArray(pre_zero, samplesperrow, numrows, maxaccess);

  result->next = virtSarrayList; // add to list of virtual arrays
  virtSarrayList = result;

  return result;
	}

CVirtSArray::CVirtSArray(BOOL pre_zero, JDIMENSION samplesperrow, JDIMENSION numrows, JDIMENSION maxaccess) : CJMemoryMgr() {

  memBuffer = NULL;	// marks array not yet realized
  rowsInArray = numrows;
  samplesPerRow=samplesperrow;
  maxAccess = maxaccess;
  preZero = pre_zero;
  next=NULL; // initialized
	b_s_info=NULL;
	}

CVirtSArray::~CVirtSArray() {

	delete b_s_info;
	}

CVirtBArray *CCompCoefController::requestVirtBarray(int pool_id, BOOL pre_zero,
	JDIMENSION blocksperrow, JDIMENSION numrows, JDIMENSION maxaccess)
// Request a virtual 2-D coefficient-block array
{
  CVirtBArray *result;

  // Only IMAGE-lifetime virtual arrays are currently supported
  if(pool_id != JPOOL_IMAGE)
    m_Parent->m_Parent->err->errorExit(JERR_BAD_POOL_ID, pool_id);	// safety check

  // get control block
  result=new CVirtBArray(pre_zero, blocksperrow, numrows, maxaccess);

  result->next = virtBarrayList; // add to list of virtual arrays
  virtBarrayList = result;

  return result;
	}

CVirtBArray::CVirtBArray(BOOL pre_zero, JDIMENSION blocksperrow, JDIMENSION numrows, JDIMENSION maxaccess) {

  memBuffer = NULL;	// marks array not yet realized
  rowsInArray = numrows;
  blocksPerRow = blocksperrow;
  maxAccess = maxaccess;
  preZero = pre_zero;
  next = NULL; // init'd
	b_s_info=NULL;
	}

CVirtBArray::~CVirtBArray() {

	delete b_s_info;
	}

void CCompressJpeg::realizeVirtArrays() {				// duplicata pari-pari x semplicita'...
// Allocate the in-memory buffers for any unrealized virtual arrays
  long space_per_minheight, maximum_space, avail_mem;
  long minheights, max_minheights;
  CVirtSArray *sptr;
  CVirtBArray *bptr;

  /* Compute the minimum space needed (maxaccess rows in each buffer)
   * and the maximum space needed (full image height in each buffer).
   * These may be of use to the system-dependent jpeg_mem_available routine.
   */
  space_per_minheight = 0;
  maximum_space = 0;
  for(sptr=main->virtSarrayList; sptr != NULL; sptr = sptr->next) {
    if(!sptr->memBuffer) { // if not realized yet
      space_per_minheight += (long) sptr->maxAccess *
			  (long) sptr->samplesPerRow * sizeof(JSAMPLE);
      maximum_space += (long) sptr->rowsInArray *
		    (long) sptr->samplesPerRow * sizeof(JSAMPLE);
			}
		}
  for(bptr=coef->virtBarrayList; bptr != NULL; bptr = bptr->next) {
    if(!bptr->memBuffer) { // if not realized yet
      space_per_minheight += (long) bptr->maxAccess *
			  (long) bptr->blocksPerRow * sizeof(JBLOCK);
      maximum_space += (long) bptr->rowsInArray *
		    (long) bptr->blocksPerRow * sizeof(JBLOCK);
			}
		}

  if(space_per_minheight <= 0)
    return;			// no unrealized arrays, no work

  // Determine amount of memory to actually use; this is system-dependent.
  avail_mem = m_Parent->mem->memAvailable(space_per_minheight, maximum_space);

  /* If the maximum space needed is available, make all the buffers full
   * height; otherwise parcel it out with the same number of minheights
   * in each buffer.
   */
  if(avail_mem >= maximum_space)
    max_minheights = 1000000000L;
  else {
    max_minheights = avail_mem / space_per_minheight;
    /* If there doesn't seem to be enough space, try to get the minimum
     * anyway.  This allows a "stub" implementation of jpeg_mem_available().
     */
    if(max_minheights <= 0)
      max_minheights = 1;
		}

  // Allocate the in-memory buffers and initialize backing store as needed.

  for(sptr=main->virtSarrayList; sptr != NULL; sptr = sptr->next) {
    if(!sptr->memBuffer) { // if not realized yet
      minheights=((long) sptr->rowsInArray - 1L) / sptr->maxAccess + 1L;
      if(minheights <= max_minheights) {
	// This buffer fits in memory
				sptr->rowsInMem = sptr->rowsInArray;
				} 
			else {
	// It doesn't fit in memory, create backing store.
				sptr->rowsInMem=(JDIMENSION)(max_minheights * sptr->maxAccess);
				sptr->b_s_info=new CBackingStore(m_Parent,(long)sptr->rowsInArray * (long)sptr->samplesPerRow * (long)sizeof(JSAMPLE));
				}
		  sptr->memBuffer=sptr->allocSarray(JPOOL_IMAGE,
				sptr->samplesPerRow, sptr->rowsInMem);
      sptr->rowsPerChunk = sptr->last_rowsPerChunk;
      sptr->curStartRow = 0;
      sptr->firstUndefRow = 0;
      sptr->dirty = FALSE;
			}
	  }

  for(bptr=coef->virtBarrayList; bptr != NULL; bptr = bptr->next) {
    if(!bptr->memBuffer) { // if not realized yet
      minheights=((long)bptr->rowsInArray - 1L) / bptr->maxAccess + 1L;
      if(minheights <= max_minheights) {
		// This buffer fits in memory
				bptr->rowsInMem = bptr->rowsInArray;
				}
			else {
		// It doesn't fit in memory, create backing store.
				bptr->rowsInMem=(JDIMENSION)(max_minheights * bptr->maxAccess);
				bptr->b_s_info=new CBackingStore(m_Parent,(long)bptr->rowsInArray * (long)bptr->blocksPerRow * (long)sizeof(JBLOCK));
				}
			bptr->memBuffer=bptr->allocBarray(JPOOL_IMAGE,bptr->blocksPerRow,bptr->rowsInMem);
			bptr->rowsPerChunk = bptr->last_rowsPerChunk;
			bptr->curStartRow = 0;
			bptr->firstUndefRow = 0;
			bptr->dirty = FALSE;
			}
		}
	}

void CDecompressJpeg::realizeVirtArrays() {		// duplicata pari-pari x semplicita'...
// Allocate the in-memory buffers for any unrealized virtual arrays
  long space_per_minheight, maximum_space, avail_mem;
  long minheights, max_minheights;
  CVirtSArray *sptr;
  CVirtBArray *bptr;

  /* Compute the minimum space needed (maxaccess rows in each buffer)
   * and the maximum space needed (full image height in each buffer).
   * These may be of use to the system-dependent jpeg_mem_available routine.
   */
  space_per_minheight = 0;
  maximum_space = 0;
  for(sptr=main->virtSarrayList; sptr != NULL; sptr = sptr->next) {
    if(!sptr->memBuffer) { // if not realized yet
      space_per_minheight += (long) sptr->maxAccess *
			  (long) sptr->samplesPerRow * sizeof(JSAMPLE);
      maximum_space += (long) sptr->rowsInArray *
		    (long) sptr->samplesPerRow * sizeof(JSAMPLE);
			}
		}
  for(bptr=coef->virtBarrayList; bptr != NULL; bptr = bptr->next) {
    if(!bptr->memBuffer) { // if not realized yet
      space_per_minheight += (long) bptr->maxAccess *
			  (long) bptr->blocksPerRow * sizeof(JBLOCK);
      maximum_space += (long) bptr->rowsInArray *
		    (long) bptr->blocksPerRow * sizeof(JBLOCK);
			}
		}

  if(space_per_minheight <= 0)
    return;			// no unrealized arrays, no work

  // Determine amount of memory to actually use; this is system-dependent.
  avail_mem = m_Parent->mem->memAvailable(space_per_minheight, maximum_space);

  /* If the maximum space needed is available, make all the buffers full
   * height; otherwise parcel it out with the same number of minheights
   * in each buffer.
   */
  if(avail_mem >= maximum_space)
    max_minheights = 1000000000L;
  else {
    max_minheights = avail_mem / space_per_minheight;
    /* If there doesn't seem to be enough space, try to get the minimum
     * anyway.  This allows a "stub" implementation of jpeg_mem_available().
     */
    if(max_minheights <= 0)
      max_minheights = 1;
		}

  // Allocate the in-memory buffers and initialize backing store as needed.

  for(sptr=main->virtSarrayList; sptr != NULL; sptr = sptr->next) {
    if(!sptr->memBuffer) { // if not realized yet
      minheights=((long) sptr->rowsInArray - 1L) / sptr->maxAccess + 1L;
      if(minheights <= max_minheights) {
	// This buffer fits in memory
				sptr->rowsInMem = sptr->rowsInArray;
				} 
			else {
	// It doesn't fit in memory, create backing store.
				sptr->rowsInMem=(JDIMENSION)(max_minheights * sptr->maxAccess);
				sptr->b_s_info=new CBackingStore(m_Parent,(long)sptr->rowsInArray * (long)sptr->samplesPerRow * (long)sizeof(JSAMPLE));
				}
		  sptr->memBuffer=sptr->allocSarray(JPOOL_IMAGE,
				sptr->samplesPerRow, sptr->rowsInMem);
      sptr->rowsPerChunk = sptr->last_rowsPerChunk;
      sptr->curStartRow = 0;
      sptr->firstUndefRow = 0;
      sptr->dirty = FALSE;
			}
	  }

  for(bptr=coef->virtBarrayList; bptr != NULL; bptr = bptr->next) {
    if(!bptr->memBuffer) { // if not realized yet
      minheights=((long)bptr->rowsInArray - 1L) / bptr->maxAccess + 1L;
      if(minheights <= max_minheights) {
		// This buffer fits in memory
				bptr->rowsInMem = bptr->rowsInArray;
				}
			else {
		// It doesn't fit in memory, create backing store.
				bptr->rowsInMem=(JDIMENSION)(max_minheights * bptr->maxAccess);
				bptr->b_s_info=new CBackingStore(m_Parent,(long)bptr->rowsInArray * (long)bptr->blocksPerRow * (long)sizeof(JBLOCK));
				}
			bptr->memBuffer=bptr->allocBarray(JPOOL_IMAGE,bptr->blocksPerRow,bptr->rowsInMem);
			bptr->rowsPerChunk = bptr->last_rowsPerChunk;
			bptr->curStartRow = 0;
			bptr->firstUndefRow = 0;
			bptr->dirty = FALSE;
			}
		}
	}


void CVirtSArray::doSarrayIO(BOOL writing) {
// Do backing store read or write of a virtual sample array
  long bytesperrow, fileOffset, byteCount, rows, thisrow, i;

  bytesperrow = (long)samplesPerRow * sizeof(JSAMPLE);
  fileOffset =curStartRow * bytesperrow;
  // Loop to read or write each allocation chunk in mem_buffer
  for(i=0; i < (long)rowsInMem; i +=rowsPerChunk) {
    // One chunk, but check for short chunk at end of buffer
    rows = min((long)rowsPerChunk, (long)rowsInMem - i);
    // Transfer no more than is currently defined
    thisrow = (long)curStartRow + i;
    rows=min(rows, (long)firstUndefRow - thisrow);
    // Transfer no more than fits in file
    rows=min(rows, (long)rowsInArray - thisrow);
    if(rows <= 0)		// this chunk might be past end of file!
      break;
    byteCount = rows*bytesperrow;
    if(writing)
      b_s_info->writeStore((void *)memBuffer[i],fileOffset,byteCount);
    else
      b_s_info->readStore((void *)memBuffer[i],fileOffset,byteCount);
    fileOffset += byteCount;
		}
	}


void CVirtBArray::doBarrayIO(BOOL writing) {
// Do backing store read or write of a virtual coefficient-block array
  long bytesperrow, fileOffset, byteCount, rows, thisrow, i;

  bytesperrow = (long)blocksPerRow *sizeof(JBLOCK);
  fileOffset = curStartRow * bytesperrow;
  // Loop to read or write each allocation chunk in mem_buffer
  for(i=0; i < (long) rowsInMem; i += rowsPerChunk) {
    // One chunk, but check for short chunk at end of buffer
    rows = min((long) rowsPerChunk, (long) rowsInMem - i);
    // Transfer no more than is currently defined
    thisrow = (long) curStartRow + i;
    rows = min(rows, (long)firstUndefRow - thisrow);
    // Transfer no more than fits in file
    rows = min(rows, (long)rowsInArray - thisrow);
    if(rows <= 0)		// this chunk might be past end of file!
      break;
    byteCount = rows*bytesperrow;
    if(writing)
      b_s_info->writeStore((void *)memBuffer[i],fileOffset,byteCount);
    else
      b_s_info->readStore((void *)memBuffer[i],fileOffset,byteCount);
    fileOffset += byteCount;
		}
	}


JSAMPARRAY CVirtSArray::accessVirtSarray(JDIMENSION start_row, JDIMENSION num_rows, BOOL writable)
/* Access the part of a virtual sample array starting at start_row */
/* and extending for num_rows rows.  writable is true if  */
/* caller intends to modify the accessed area. */
{
  JDIMENSION end_row=start_row+num_rows;
  JDIMENSION undef_row;

  // debugging check
  if(end_row > rowsInArray || num_rows > maxAccess || !memBuffer)
    m_Parent->err->errorExit(JERR_BAD_VIRTUAL_ACCESS);

  // Make the desired part of the virtual array accessible 
  if(start_row < curStartRow ||
      end_row > curStartRow+rowsInMem) {
    if(!b_s_info)
      m_Parent->err->errorExit(JERR_VIRTUAL_BUG);
    // Flush old buffer contents if necessary
    if(dirty) {
      doSarrayIO(TRUE);
      dirty=FALSE;
			}
    /* Decide what part of virtual array to access.
     * Algorithm: if target address > current window, assume forward scan,
     * load starting at target address.  If target address < current window,
     * assume backward scan, load so that target area is top of window.
     * Note that when switching from forward write to forward read, will have
     * start_row = 0, so the limiting case applies and we load from 0 anyway.
     */
    if(start_row > curStartRow) {
      curStartRow = start_row;
			}
		else {
      // use long arithmetic here to avoid overflow & unsigned problems
      long ltemp;

      ltemp = (long)end_row - (long)rowsInMem;
      if(ltemp < 0)
				ltemp = 0;		// don't fall off front end of file
      curStartRow = (JDIMENSION)ltemp;
			}
    /* Read in the selected part of the array.
     * During the initial write pass, we will do no actual read
     * because the selected part is all undefined.
     */
    doSarrayIO(FALSE);
		}
  /* Ensure the accessed part of the array is defined; prezero if needed.
   * To improve locality of access, we only prezero the part of the array
   * that the caller is about to access, not the entire in-memory array.
   */
  if(firstUndefRow < end_row) {
    if(firstUndefRow < start_row) {
      if(writable)		// writer skipped over a section of array
				m_Parent->err->errorExit(JERR_BAD_VIRTUAL_ACCESS);
				undef_row = start_row;	// but reader is allowed to read ahead
				}
		else {
      undef_row=firstUndefRow;
			}
    if(writable)
      firstUndefRow = end_row;
    if(preZero) {
      size_t bytesperrow = (size_t)samplesPerRow * sizeof(JSAMPLE);
      undef_row -= curStartRow; // make indexes relative to buffer
      end_row -= curStartRow;
      while(undef_row < end_row) {
				ZeroMemory((void *)memBuffer[undef_row], bytesperrow);
				undef_row++;
				}
			}
		else {
      if(!writable)		// reader looking at undefined data
				m_Parent->err->errorExit(JERR_BAD_VIRTUAL_ACCESS);
			}
		}
  // Flag the buffer dirty if caller will write in it
  if(writable)
    dirty = TRUE;
  // Return address of proper part of the buffer
  return memBuffer + (start_row -curStartRow);
	}


JBLOCKARRAY CVirtBArray::accessVirtBarray(JDIMENSION start_row, JDIMENSION num_rows, BOOL writable)
/* Access the part of a virtual block array starting at start_row */
/* and extending for num_rows rows.  writable is true if  */
/* caller intends to modify the accessed area. */
{
  JDIMENSION end_row = start_row + num_rows;
  JDIMENSION undef_row;

  // debugging check
  if(end_row > rowsInArray || num_rows > maxAccess || !memBuffer)
    m_Parent->err->errorExit(JERR_BAD_VIRTUAL_ACCESS);

  // Make the desired part of the virtual array accessible
  if(start_row < curStartRow ||
    end_row > curStartRow+rowsInMem) {
    if(!b_s_info)
      m_Parent->err->errorExit(JERR_VIRTUAL_BUG);
    // Flush old buffer contents if necessary
    if(dirty) {
      doBarrayIO(TRUE);
      dirty = FALSE;
			}
    /* Decide what part of virtual array to access.
     * Algorithm: if target address > current window, assume forward scan,
     * load starting at target address.  If target address < current window,
     * assume backward scan, load so that target area is top of window.
     * Note that when switching from forward write to forward read, will have
     * start_row = 0, so the limiting case applies and we load from 0 anyway.
     */
    if(start_row > curStartRow) {
      curStartRow = start_row;
			}
		else {
      // use long arithmetic here to avoid overflow & unsigned problems
      long ltemp;

      ltemp = (long)end_row - (long)rowsInMem;
			if(ltemp < 0)
				ltemp = 0;		// don't fall off front end of file
				curStartRow = (JDIMENSION)ltemp;
			}
    /* Read in the selected part of the array.
     * During the initial write pass, we will do no actual read
     * because the selected part is all undefined.
     */
    doBarrayIO(FALSE);
		}
  /* Ensure the accessed part of the array is defined; prezero if needed.
   * To improve locality of access, we only prezero the part of the array
   * that the caller is about to access, not the entire in-memory array.
   */
  if(firstUndefRow < end_row) {
    if(firstUndefRow < start_row) {
      if(writable)		// writer skipped over a section of array
				m_Parent->err->errorExit(JERR_BAD_VIRTUAL_ACCESS);
			undef_row = start_row;	// but reader is allowed to read ahead
			}
		else {
      undef_row = firstUndefRow;
			}
    if(writable)
      firstUndefRow = end_row;
    if(preZero) {
      size_t bytesperrow = (size_t)blocksPerRow * sizeof(JBLOCK);
      undef_row -= curStartRow; // make indexes relative to buffer
      end_row -= curStartRow;
      while(undef_row < end_row) {
				ZeroMemory((void *)memBuffer[undef_row], bytesperrow);
				undef_row++;
				}
			} 
		else {
      if(!writable)		// reader looking at undefined data
				m_Parent->err->errorExit(JERR_BAD_VIRTUAL_ACCESS);
			}
		}
  // Flag the buffer dirty if caller will write in it
  if(writable)
    dirty = TRUE;
  // Return address of proper part of the buffer
  return memBuffer + (start_row - curStartRow);
	}


/*
 * Release all objects belonging to a specified pool.
 */

void CJMemoryMgrExt::freePool(int pool_id) {
  SMALL_POOL_PTR shdr_ptr;
  LARGE_POOL_PTR lhdr_ptr;
  size_t space_freed;

  if(pool_id < 0 || pool_id >= JPOOL_NUMPOOLS)
    m_Parent->err->errorExit(JERR_BAD_POOL_ID, pool_id);	// safety check

#ifdef MEM_STATS
  if(err->trace_level > 1)
    printMemStats(cinfo, pool_id); // print pool's memory usage statistics
#endif

  // If freeing IMAGE pool, close any virtual arrays first
  if(pool_id == JPOOL_IMAGE) {
		}

  // Release large objects
  lhdr_ptr = largeList[pool_id];
  largeList[pool_id] = NULL;

  while(lhdr_ptr != NULL) {
    LARGE_POOL_PTR next_lhdr_ptr = lhdr_ptr->hdr.next;
    space_freed = lhdr_ptr->hdr.bytesUsed +
		  lhdr_ptr->hdr.bytesLeft +
		  sizeof(LARGE_POOL_HDR);
    freeLarge((void *)lhdr_ptr, space_freed);
    totalSpaceAllocated -= space_freed;
    lhdr_ptr = next_lhdr_ptr;
		}

  // Release small objects
  shdr_ptr = smallList[pool_id];
  smallList[pool_id] = NULL;

  while(shdr_ptr != NULL) {
    SMALL_POOL_PTR next_shdr_ptr = shdr_ptr->hdr.next;
    space_freed = shdr_ptr->hdr.bytesUsed +
		  shdr_ptr->hdr.bytesLeft +
		  sizeof(SMALL_POOL_HDR);
    freeSmall((void *)shdr_ptr, space_freed);
    totalSpaceAllocated -= space_freed;
    shdr_ptr = next_shdr_ptr;
		}
	}


/*
 * Close up shop entirely.
 * Note that this cannot be called unless cinfo->mem is non-NULL.
 */

void CJpeg::selfDestruct() {
  int pool;

  /* Close all backing store, release all memory.
   * Releasing pools in reverse order might help avoid fragmentation
   * with some (brain-damaged) malloc libraries.
   */
  for(pool=JPOOL_NUMPOOLS-1; pool >= JPOOL_PERMANENT; pool--) {
    mem->freePool(pool);
		}

  // Release the memory manager control block too.
  delete mem;
	mem=NULL;		// ensures I will be called only once

  memTerm();		// system-dependent cleanup
	}


/*
 * Memory manager initialization.
 * When this is called, only the error manager pointer is valid in cinfo!
 */

CJMemoryMgrExt *CJpeg::initMemoryMgr() {
  long maxToUse;
  size_t testMac;

  mem=NULL;		// for safety if init fails

  /* Check for configuration errors.
   * SIZEOF(ALIGN_TYPE) should be a power of 2; otherwise, it probably
   * doesn't reflect any real hardware alignment requirement.
   * The test is a little tricky: for X>0, X and X-1 have no one-bits
   * in common if and only if X is a power of 2, ie has only one one-bit.
   * Some compilers may give an "unreachable code" warning here; ignore it.
   */
  if((sizeof(ALIGN_TYPE) & (sizeof(ALIGN_TYPE)-1)) != 0)
    err->errorExit(JERR_BAD_ALIGN_TYPE);
  /* CJMemoryMgr::MAX_ALLOC_CHUNK must be representable as type size_t, and must be
   * a multiple of SIZEOF(ALIGN_TYPE).
   * Again, an "unreachable code" warning may be ignored here.
   * But a "constant too large" warning means you need to fix CJMemoryMgr::MAX_ALLOC_CHUNK.
   */
  testMac=(size_t)CJMemoryMgr::MAX_ALLOC_CHUNK;
  if((long)testMac != CJMemoryMgr::MAX_ALLOC_CHUNK ||
      (CJMemoryMgr::MAX_ALLOC_CHUNK % sizeof(ALIGN_TYPE)) != 0)
    err->errorExit(JERR_BAD_ALLOC_CHUNK);

  maxToUse = memInit(); // system-dependent initialization 

  // Attempt to allocate memory manager's control block 
  mem=new CJMemoryMgrExt(this);

  if(!mem) {
    memTerm();	// system-dependent cleanup
    err->errorExit(JERR_OUT_OF_MEMORY,0);
		}

  // Initialize working state 
  mem->setMinMaxMemoryToUse(sizeof(CJMemoryMgrExt),maxToUse);

  // Declare ourselves open for business 
//  cinfo->mem = &mem->pub;
	return mem;
	}







/*
 * jcapimin.c
 *
 * Copyright (C) 1994-1998, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains application interface code for the compression half
 * of the JPEG library.  These are the "minimum" API routines that may be
 * needed in either the normal full-compression case or the transcoding-only
 * case.
 *
 * Most of the routines intended to be called directly by an application
 * are in this file or in jcapistd.c.  But also see jcparam.c for
 * parameter-setup helper routines, jcomapi.c for routines shared by
 * compression and decompression, and jctrans.c for the transcoding case.
 */



/*
 * Initialization of a JPEG compression object.
 * The error manager must already be set up (in case memory manager fails).
 */

CCompressJpeg *CJpeg::createCompress(const char *note,const char *autore,int version) {
//  int i;

  // Guard against version mismatches between library and caller.
//  mem=NULL;		// so jpeg_destroy knows mem mgr not called
	if(version != JPEG_LIB_VERSION)
    err->errorExit(JERR_BAD_LIB_VERSION, JPEG_LIB_VERSION, version);

  /* For debugging purposes, we zero the whole master structure.
   * But the application has already set the err pointer, and may have set
   * client_data, so we have to save and restore those fields.
   * Note: if application hasn't set client_data, tools like Purify may
   * complain here.
   */
	co=new CCompressJpeg(this,clientData,note,autore);
  isDecompressor = FALSE;

// Memory manager initialization
	initMemoryMgr();

#ifdef PROGRESS_REPORT
	progress=new J_PROGRESS_MGR(this);
#endif

	return co;
	}

/*
 * jpeg_natural_order[i] is the natural-order position of the i'th element
 * of zigzag order.
 *
 * When reading corrupted data, the Huffman decoders could attempt
 * to reference an entry beyond the end of this array (if the decoded
 * zero run length reaches past the end of the block).  To prevent
 * wild stores without adding an inner-loop test, we put some extra
 * "63"s after the real entries.  This will cause the extra coefficient
 * to be stored in location 63 of the block, not somewhere random.
 * The worst case would be a run-length of 15, which means we need 16
 * fake entries.
 */

const	int CCompressJpeg::nOrder[DCTSIZE2+16] = {
		0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63,
		63, 63, 63, 63, 63, 63, 63, 63, /* extra entries for safety in decoder */
		63, 63, 63, 63, 63, 63, 63, 63
		};


CCompressJpeg::CCompressJpeg(/*const*/ CJpeg *p,void *cData,const char *note,const char *autore) :	m_Parent(p) {
	int i;

	*m_note=0;
	if(note) {
		_tcsncpy(m_note,note,127);
		m_note[127]=0;
		}
	*m_autore=0;
	if(autore) {
		_tcsncpy(m_autore,autore,127);
		m_autore[127]=0;
		}

	imageWidth=imageHeight=0;
	imageRatio=0;
	inputComponents=0;
	inColorSpace=JCS_UNKNOWN;
	colorSpace=JCS_UNKNOWN;
  inputGamma = 0.0;
  dataPrecision = BITS_IN_JSAMPLE;
	numComponents=0;
	numScans=0;
	scanInfo=NULL;
	rawDataIn=0;
	optimizeCoding=0;
	CCIR601Sampling=0;
	smoothingFactor=0;
	DCT_Method=JDCT_FLOAT;
	restartInterval=restartInRows=0;
	write_JFIF_header=0;
	JFIF_majorVersion=JFIF_minorVersion=0;
	densityUnit=0;
	XDensity=YDensity=0;
	writeAdobeMarker=0;
	nextScanline=0;
	max_H_SampFactor=max_V_SampFactor=0;
	total_iMCU_rows=0;
	compsInScan=0;
	MCUs_perRow=MCU_rowsInScan=blocksInMCU=0;
	Ss=Se=Ah=Al=0;


	dest=NULL;
	coef=NULL;
	main=NULL;
	marker=NULL;
	master=NULL;
	prep=NULL;
	cconvert=NULL;
	downsample=NULL;
	fdct=NULL;
	entropy=NULL;

	progressiveMode=FALSE;
  // Use Huffman coding, not arithmetic coding, by default 
  arithCode = FALSE;

	compInfo=NULL;

  for(i=0; i<NUM_QUANT_TBLS; i++)
    quantTblPtrs[i] = NULL;

  for(i=0; i<NUM_HUFF_TBLS; i++) {
    DC_HuffTblPtrs[i] = NULL;
    AC_HuffTblPtrs[i] = NULL;
	  }

	for(i=0; i<DCTSIZE2+16; i++)
		naturalOrder[i]=CCompressJpeg::nOrder[i];

  scriptSpace = NULL;
  scriptSpaceSize = 0;

  // OK, I'm ready 
  globalState = CSTATE_START;
	}

CCompressJpeg::~CCompressJpeg() {

	delete entropy;	entropy=NULL;
	delete fdct;	fdct=NULL;
	delete downsample;	downsample=NULL;
	delete cconvert;	cconvert=NULL;
	delete prep;		prep=NULL;
	delete coef;		coef=NULL;
	delete marker;	marker=NULL;
	delete master;	master=NULL;
	delete main;	main=NULL;
	delete dest;	dest=NULL;
	}

/*
 * Destruction of a JPEG compression object
 */

void CJpeg::destroyCompress() {

  destroy(); // use common routine
	delete co;
	co=NULL;
	}


/*
 * Abort processing of a JPEG compression operation,
 * but don't destroy the object itself.
 */

void CJpeg::abortCompress() {

  abort(); // use common routine
	}


/*
 * Forcibly suppress or un-suppress all quantization and Huffman tables.
 * Marks all currently defined tables as already written (if suppress)
 * or not written (if !suppress).  This will control whether they get emitted
 * by a subsequent jpeg_start_compress call.
 *
 * This routine is exported for use by applications that want to produce
 * abbreviated JPEG datastreams.  It logically belongs in jcparam.c, but
 * since it is called by jpeg_start_compress, we put it here --- otherwise
 * jcparam.o would be linked whether the application used it or not.
 */

void CCompressJpeg::suppressTables(BOOL suppress) {
  int i;
  JQUANT_TBL *qtbl;
  JHUFF_TBL *htbl;

  for(i=0; i<NUM_QUANT_TBLS; i++) {
    if(qtbl = quantTblPtrs[i])
      qtbl->sentTable = suppress;
		}

  for(i=0; i<NUM_HUFF_TBLS; i++) {
    if(htbl = DC_HuffTblPtrs[i])
      htbl->sentTable = suppress;
    if(htbl = AC_HuffTblPtrs[i])
      htbl->sentTable = suppress;
		}
	}


/*
 * Finish JPEG compression.
 *
 * If a multipass operating mode was selected, this may do a great deal of
 * work including most of the actual output.
 */

void CCompressJpeg::finishCompress() {
  JDIMENSION iMCU_row;

//  BOOL (CCompCoefController::*d)(JSAMPIMAGE);

  if(globalState == CSTATE_SCANNING || globalState == CSTATE_RAW_OK) {
    // Terminate first pass 
    if(nextScanline < imageHeight)
      m_Parent->err->errorExit(JERR_TOO_LITTLE_DATA);
    master->finishPass();
		}
	else if(globalState != CSTATE_WRCOEFS)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
  // Perform any remaining passes 
  while(!master->isLastPass) {
    master->prepareForPass();
    for(iMCU_row=0; iMCU_row<total_iMCU_rows; iMCU_row++) {
      if(m_Parent->progress) {
				m_Parent->progress->passCounter=(long)iMCU_row;
				m_Parent->progress->passLimit=(long)total_iMCU_rows;
				m_Parent->progress->progressMonitor();
	      }
      /* We bypass the main controller and invoke coef controller directly;
       * all work is being done from the coefficient buffer.
       */
			if(! (coef->*coef->doCompressData)((JSAMPIMAGE)NULL) )
				m_Parent->err->errorExit(JERR_CANT_SUSPEND);
			}
		master->finishPass();
		}
  // Write EOI, do final cleanup 
  writeFileTrailer();
  dest->termDestination();
  // We can use jpeg_abort to release memory and reset global_state
  m_Parent->abort();
	}


/*
 * Write a special marker.
 * This is only recommended for writing COM or APPn markers.
 * Must be called after jpeg_start_compress() and before
 * first call to jpeg_write_scanlines() or jpeg_write_raw_data().
 */

void CMarkerWriter::writeMarker(int mark,const JOCTET *dataptr, DWORD datalen) {

  if(m_Parent->nextScanline != 0 ||
		(m_Parent->globalState != CCompressJpeg::CSTATE_SCANNING &&
       m_Parent->globalState != CCompressJpeg::CSTATE_RAW_OK &&
       m_Parent->globalState != CCompressJpeg::CSTATE_WRCOEFS))
    m_Parent->m_Parent->err->errorExit(JERR_BAD_STATE, m_Parent->globalState);

  writeMarkerHeader(mark, datalen);
//  writeMarkerByte = writeMarkerByte;	// copy for speed
  while(datalen--) {
    writeMarkerByte(*dataptr);
    dataptr++;
		}
	}

// Same, but piecemeal.

void CMarkerWriter::writeMHeader(int mark, DWORD datalen) {
  
	if(m_Parent->nextScanline != 0 ||
    (m_Parent->globalState != CCompressJpeg::CSTATE_SCANNING &&
     m_Parent->globalState != CCompressJpeg::CSTATE_RAW_OK &&
     m_Parent->globalState != CCompressJpeg::CSTATE_WRCOEFS))
    m_Parent->m_Parent->err->errorExit(JERR_BAD_STATE, m_Parent->globalState);

  writeMarkerHeader(mark, datalen);
	}

void CMarkerWriter::writeMByte(int val) {

  writeMarkerByte(val);
	}


/*
 * Alternate compression function: just write an abbreviated table file.
 * Before calling this, all parameters and a data destination must be set up.
 *
 * To produce a pair of files containing abbreviated tables and abbreviated
 * image data, one would proceed as follows:
 *
 *		initialize JPEG object
 *		set JPEG parameters
 *		set destination to table file
 *		jpeg_write_tables(cinfo);
 *		set destination to image file
 *		jpeg_start_compress(cinfo, FALSE);
 *		write data...
 *		jpeg_finish_compress(cinfo);
 *
 * jpeg_write_tables has the side effect of marking all tables written
 * (same as jpeg_suppress_tables(..., TRUE)).  Thus a subsequent start_compress
 * will not re-emit the tables unless it is passed write_all_tables=TRUE.
 */

void CCompressJpeg::writeTables() {

  if(globalState != CSTATE_START)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);

  // (Re)initialize error mgr and destination modules
  m_Parent->err->resetErrorMgr();
  dest->initDestination();
  // Initialize the marker writer ... bit of a crock to do it here.
	marker=new CMarkerWriter(this);
  // Write them tables!
  writeTablesOnly();
  // And clean up.
  dest->termDestination();

  /*
   * In library releases up through v6a, we called jpeg_abort() here to free
   * any working memory allocated by the destination manager and marker
   * writer.  Some applications had a problem with that: they allocated space
   * of their own from the library memory manager, and didn't want it to go
   * away during write_tables.  So now we do nothing.  This will cause a
   * memory leak if an app calls write_tables repeatedly without doing a full
   * compression cycle or otherwise resetting the JPEG object.  However, that
   * seems less bad than unexpectedly freeing memory in the normal case.
   * An app that prefers the old behavior can call jpeg_abort for itself after
   * each call to jpeg_write_tables().
   */
	}






/*
 * jcapistd.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains application interface code for the compression half
 * of the JPEG library.  These are the "standard" API routines that are
 * used in the normal full-compression case.  They are not used by a
 * transcoding-only application.  Note that if an application links in
 * jpeg_start_compress, it will end up linking in the entire compressor.
 * We thus must separate this file from jcapimin.c to avoid linking the
 * whole compression library into a transcoder.
 */



/*
 * Compression initialization.
 * Before calling this, all parameters and a data destination must be set up.
 *
 * We require a write_all_tables parameter as a failsafe check when writing
 * multiple datastreams from the same compression object.  Since prior runs
 * will have left all the tables marked sent_table=TRUE, a subsequent run
 * would emit an abbreviated stream (no tables) by default.  This may be what
 * is wanted, but for safety's sake it should not be the default behavior:
 * programmers should have to make a deliberate choice to emit abbreviated
 * images.  Therefore the documentation and examples should encourage people
 * to pass write_all_tables=TRUE; then it will take active thought to do the
 * wrong thing.
 */

void CCompressJpeg::startCompress(BOOL writeAllTables) {

  if(globalState != CSTATE_START)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);

  if(writeAllTables)
    suppressTables(FALSE);	// mark all tables to be written 

  // (Re)initialize error mgr and destination modules
  m_Parent->err->resetErrorMgr();
  dest->initDestination();
  // Perform master selection of active modules 
  initCompressMaster();
  // Set up for the first pass 
  master->prepareForPass();
  /* Ready for application to drive first pass through jpeg_write_scanlines
   * or jpeg_write_raw_data.
   */
  nextScanline=0;
  globalState=(rawDataIn ? CSTATE_RAW_OK : CSTATE_SCANNING);
	}


/*
 * Write some scanlines of data to the JPEG compressor.
 *
 * The return value will be the number of lines actually written.
 * This should be less than the supplied num_lines only in case that
 * the data destination module has requested suspension of the compressor,
 * or if more than image_height scanlines are passed in.
 *
 * Note: we warn about excess calls to jpeg_write_scanlines() since
 * this likely signals an application programmer error.  However,
 * excess scanlines passed in the last valid call are *silently* ignored,
 * so that the application need not adjust num_lines for end-of-image
 * when using a multiple-scanline buffer.
 */

JDIMENSION CCompressJpeg::writeScanlines(JSAMPARRAY scanlines,
	JDIMENSION num_lines) {
  JDIMENSION row_ctr, rows_left;

  if(globalState != CSTATE_SCANNING)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
  if(nextScanline >= imageHeight)
    m_Parent->err->warn(JWRN_TOO_MUCH_DATA);

  // Call progress monitor hook if present 
  if(m_Parent->progress) {
    m_Parent->progress->passCounter = (long)nextScanline;
    m_Parent->progress->passLimit = (long)imageHeight;
    m_Parent->progress->progressMonitor();
		}

  /* Give master control module another chance if this is first call to
   * jpeg_write_scanlines.  This lets output of the frame/scan headers be
   * delayed so that application can write COM, etc, markers between
   * jpeg_start_compress and jpeg_write_scanlines.
   */
  if(master->callPassStartup)
    master->passStartup();

  // Ignore any extra scanlines at bottom of image.
  rows_left=imageHeight-nextScanline;
  if(num_lines > rows_left)
    num_lines=rows_left;

  row_ctr=0;
  (main->*main->doProcessData)(scanlines, &row_ctr, num_lines);
  nextScanline += row_ctr;
  return row_ctr;
	}


/*
 * Alternate entry point to write raw data.
 * Processes exactly one iMCU row per call, unless suspended.
 */

JDIMENSION CCompressJpeg::writeRawData(JSAMPIMAGE data, JDIMENSION num_lines) {
  JDIMENSION lines_per_iMCU_row;

  if(globalState != CSTATE_RAW_OK)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
  if(nextScanline >= imageHeight) {
    m_Parent->err->warn(JWRN_TOO_MUCH_DATA);
    return 0;
  }

  // Call progress monitor hook if present 
  if(m_Parent->progress != NULL) {
    m_Parent->progress->passCounter = (long)nextScanline;
    m_Parent->progress->passLimit = (long)imageHeight;
    m_Parent->progress->progressMonitor();
		}

  /* Give master control module another chance if this is first call to
   * jpeg_write_raw_data.  This lets output of the frame/scan headers be
   * delayed so that application can write COM, etc, markers between
   * jpeg_start_compress and jpeg_write_raw_data.
   */
  if(master->callPassStartup)
    master->passStartup();

  // Verify that at least one iMCU row has been passed.
  lines_per_iMCU_row = max_V_SampFactor * DCTSIZE;
  if(num_lines < lines_per_iMCU_row)
    m_Parent->err->errorExit(JERR_BUFFER_SIZE);

  // Directly compress the row.
  if(!(coef->*coef->doCompressData)(data)) {
    // If compressor did not consume the whole row, suspend processing.
    return 0;
		}

  // OK, we processed one iMCU row. 
  nextScanline += lines_per_iMCU_row;
  return lines_per_iMCU_row;
	}






/*
 * jccoefct.c
 *
 * Copyright (C) 1994-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the coefficient buffer controller for compression.
 * This controller is the top level of the JPEG compressor proper.
 * The coefficient buffer lies between forward-DCT and entropy encoding steps.
 */



/* We use a full-image coefficient buffer when doing Huffman optimization,
 * and also for writing multiple-scan JPEG files.  In all cases, the DCT
 * step is run during the first pass, and subsequent passes need only read
 * the buffered coefficients.
 */
#ifdef ENTROPY_OPT_SUPPORTED
#define FULL_COEF_BUFFER_SUPPORTED
#else
#ifdef C_MULTISCAN_FILES_SUPPORTED
#define FULL_COEF_BUFFER_SUPPORTED
#endif
#endif


void CCompCoefController::start_iMCU_row() {
// Reset within-iMCU-row counters for a new row 

  /* In an interleaved scan, an MCU row is the same as an iMCU row.
   * In a noninterleaved scan, an iMCU row has v_samp_factor MCU rows.
   * But at the bottom of the image, process only what's left.
   */
  if(m_Parent->compsInScan>1) {
    MCU_rowsPer_iMCU_row = 1;
	  } 
	else {
    if(iMCU_rowNum < (m_Parent->total_iMCU_rows-1))
      MCU_rowsPer_iMCU_row = m_Parent->curCompInfo[0]->vSampFactor;
    else
      MCU_rowsPer_iMCU_row = m_Parent->curCompInfo[0]->lastRowHeight;
		}

  mcuCtr = 0;
  MCU_vertOffset = 0;
	}


/*
 * Initialize for a processing pass.
 */

void CCompCoefController::startPass(J_BUF_MODE passMode) {

  iMCU_rowNum = 0;
  start_iMCU_row();

  switch(passMode) {
		case JBUF_PASS_THRU:
			if(wholeImage[0])
				m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			doCompressData = compressData;
			break;
#ifdef FULL_COEF_BUFFER_SUPPORTED
		case JBUF_SAVE_AND_PASS:
			if(!wholeImage[0])
				m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			doCompressData = compressFirstPass;
			break;
		case JBUF_CRANK_DEST:
			if(wholeImage[0] == NULL)
				m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			doCompressData = compressOutput;
			break;
#endif
		default:
			m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			break;
		}
	}


/*
 * Process some data in the single-pass case.
 * We process the equivalent of one fully interleaved MCU row ("iMCU" row)
 * per call, ie, v_samp_factor block rows for each component in the image.
 * Returns TRUE if the iMCU row is completed, FALSE if suspended.
 *
 * NB: input_buf contains a plane for each component in image,
 * which we index according to the component's SOF position.
 */

BOOL CCompCoefController::compressData(JSAMPIMAGE input_buf) {

  JDIMENSION MCU_colNum;	// index of current MCU within row
  JDIMENSION last_MCU_col = m_Parent->MCUs_perRow - 1;
  JDIMENSION last_iMCU_row = m_Parent->total_iMCU_rows - 1;
  int blkn,bi,ci,yindex,yoffset,blockcnt;
  JDIMENSION ypos,xpos;
  J_COMPONENT_INFO *compptr;

  // Loop to write as much as one whole iMCU row
  for(yoffset=MCU_vertOffset; yoffset < MCU_rowsPer_iMCU_row; yoffset++) {
    for(MCU_colNum=mcuCtr; MCU_colNum <= last_MCU_col; MCU_colNum++) {
      /* Determine where data comes from in input_buf and do the DCT thing.
       * Each call on forward_DCT processes a horizontal row of DCT blocks
       * as wide as an MCU; we rely on having allocated the MCU_buffer[] blocks
       * sequentially.  Dummy blocks at the right or bottom edge are filled in
       * specially.  The data in them does not matter for image reconstruction,
       * so we fill them with values that will encode to the smallest amount of
       * data, viz: all zeroes in the AC entries, DC entries equal to previous
       * block's DC value.  (Thanks to Thomas Kinsman for this idea.)
       */
      blkn=0;
      for(ci=0; ci<m_Parent->compsInScan; ci++) {
				compptr = m_Parent->curCompInfo[ci];
				blockcnt=(MCU_colNum < last_MCU_col) ? compptr->MCU_width : compptr->lastColWidth;
				xpos= MCU_colNum * compptr->MCU_sampleWidth;
				ypos= yoffset*DCTSIZE; // ypos == (yoffset+yindex) * DCTSIZE 
				for(yindex=0; yindex< compptr->MCU_height; yindex++) {
					if(iMCU_rowNum < last_iMCU_row ||
						yoffset+yindex < compptr->lastRowHeight) {
							(m_Parent->fdct->*(m_Parent->fdct)->doForwardDCT)(compptr,input_buf[compptr->componentIndex],
								MCU_buffer[blkn], ypos, xpos, (JDIMENSION)blockcnt);
					if(blockcnt<compptr->MCU_width) {
	      // Create some dummy blocks at the right edge of the image.
						ZeroMemory((void *)MCU_buffer[blkn + blockcnt],
							(compptr->MCU_width - blockcnt) * sizeof(JBLOCK));
						for(bi=blockcnt; bi < compptr->MCU_width; bi++) {
							MCU_buffer[blkn+bi][0][0]=MCU_buffer[blkn+bi-1][0][0];
					}
				}
			} 
		else {
	    // Create a row of dummy blocks at the bottom of the image.
	    ZeroMemory((void *)MCU_buffer[blkn],
		    compptr->MCU_width * sizeof(JBLOCK));
	    for(bi=0; bi < compptr->MCU_width; bi++)
	      MCU_buffer[blkn+bi][0][0] = MCU_buffer[blkn-1][0][0];
		    
			}
		blkn += compptr->MCU_width;
		ypos += DCTSIZE;
		}
  }
      /* Try to write the MCU.  In event of a suspension failure, we will
       * re-DCT the MCU on restart (a bit inefficient, could be fixed...)
       */
      if(!(m_Parent->entropy->*(m_Parent->entropy)->doEncodeMCU)(MCU_buffer)) {
				// Suspension forced; update state counters and exit
				MCU_vertOffset = yoffset;
				mcuCtr = MCU_colNum;
				return FALSE;
				}
			}
    // Completed an MCU row, but perhaps not an iMCU row
    mcuCtr = 0;
		}
  // Completed the iMCU row, advance counters for next one
  iMCU_rowNum++;
  start_iMCU_row();
  return TRUE;
	}


#ifdef FULL_COEF_BUFFER_SUPPORTED

/*
 * Process some data in the first pass of a multi-pass case.
 * We process the equivalent of one fully interleaved MCU row ("iMCU" row)
 * per call, ie, v_samp_factor block rows for each component in the image.
 * This amount of data is read from the source buffer, DCT'd and quantized,
 * and saved into the virtual arrays.  We also generate suitable dummy blocks
 * as needed at the right and lower edges.  (The dummy blocks are constructed
 * in the virtual arrays, which have been padded appropriately.)  This makes
 * it possible for subsequent passes not to worry about real vs. dummy blocks.
 *
 * We must also emit the data to the entropy encoder.  This is conveniently
 * done by calling compress_output() after we've loaded the current strip
 * of the virtual arrays.
 *
 * NB: input_buf contains a plane for each component in image.  All
 * components are DCT'd and loaded into the virtual arrays in this pass.
 * However, it may be that only a subset of the components are emitted to
 * the entropy encoder during this first pass; be careful about looking
 * at the scan-dependent variables (MCU dimensions, etc).
 */

BOOL CCompCoefController::compressFirstPass(JSAMPIMAGE input_buf) {
  JDIMENSION last_iMCU_row = m_Parent->total_iMCU_rows - 1;
  JDIMENSION blocksAcross, MCUs_across, MCUindex;
  int bi,ci,hSampFactor,blockRow,blockRows,ndummy;
  JCOEF lastDC;
  J_COMPONENT_INFO *compptr;
  JBLOCKARRAY buffer;
  JBLOCKROW thisblockrow, lastblockrow;

  for(ci=0, compptr=m_Parent->compInfo; ci<m_Parent->numComponents; ci++, compptr++) {
    // Align the virtual buffer for this component.
    buffer=wholeImage[ci]->accessVirtBarray(iMCU_rowNum *compptr->vSampFactor,
       (JDIMENSION)compptr->vSampFactor, TRUE);
    // Count non-dummy DCT block rows in this iMCU row.
    if(iMCU_rowNum < last_iMCU_row)
      blockRows=compptr->vSampFactor;
    else {
      // NB: can't use last_row_height here, since may not be set!
      blockRows=(int)(compptr->heightInBlocks % compptr->vSampFactor);
      if(!blockRows)
				blockRows=compptr->vSampFactor;
	    }
    blocksAcross=compptr->widthInBlocks;
    hSampFactor=compptr->hSampFactor;
    /* Count number of dummy blocks to be added at the right margin. */
    ndummy = (int)(blocksAcross % hSampFactor);
    if(ndummy > 0)
      ndummy = hSampFactor-ndummy;
    /* Perform DCT for all non-dummy blocks in this iMCU row.  Each call
     * on forward_DCT processes a complete horizontal row of DCT blocks.
     */
    for(blockRow=0; blockRow < blockRows; blockRow++) {
      thisblockrow=buffer[blockRow];
      (m_Parent->fdct->*(m_Parent->fdct)->doForwardDCT)(compptr, input_buf[ci], thisblockrow,
				 (JDIMENSION)(blockRow * DCTSIZE),
				 (JDIMENSION)0, blocksAcross);
      if(ndummy > 0) {
	// Create dummy blocks at the right edge of the image.
				thisblockrow += blocksAcross; // => first dummy block
				ZeroMemory((void *)thisblockrow, ndummy * sizeof(JBLOCK));
				lastDC = thisblockrow[-1][0];
				for(bi=0; bi < ndummy; bi++) {
					thisblockrow[bi][0] = lastDC;
					}
	      }
		  }
    /* If at end of image, create dummy block rows as needed.
     * The tricky part here is that within each MCU, we want the DC values
     * of the dummy blocks to match the last real block's DC value.
     * This squeezes a few more bytes out of the resulting file...
     */
    if(iMCU_rowNum == last_iMCU_row) {
      blocksAcross += ndummy;	// include lower right corner
      MCUs_across = blocksAcross / hSampFactor;
      for(blockRow = blockRows; blockRow < compptr->vSampFactor; blockRow++) {
				thisblockrow = buffer[blockRow];
				lastblockrow = buffer[blockRow-1];
				ZeroMemory((void *)thisblockrow,(size_t) (blocksAcross *sizeof(JBLOCK)));
				for(MCUindex = 0; MCUindex < MCUs_across; MCUindex++) {
					lastDC = lastblockrow[hSampFactor-1][0];
					for(bi = 0; bi < hSampFactor; bi++) {
						thisblockrow[bi][0] = lastDC;
					}
					thisblockrow += hSampFactor; /* advance to next MCU in row */
					lastblockrow += hSampFactor;
				}
				}
			}
		}
  /* NB: compress_output will increment iMCU_row_num if successful.
   * A suspension return will result in redoing all the work above next time.
   */

  // Emit data to the entropy encoder, sharing code with subsequent passes
  return compressOutput(input_buf);
	}


/*
 * Process some data in subsequent passes of a multi-pass case.
 * We process the equivalent of one fully interleaved MCU row ("iMCU" row)
 * per call, ie, v_samp_factor block rows for each component in the scan.
 * The data is obtained from the virtual arrays and fed to the entropy coder.
 * Returns TRUE if the iMCU row is completed, FALSE if suspended.
 *
 * NB: input_buf is ignored; it is likely to be a NULL pointer.
 */

BOOL CCompCoefController::compressOutput(JSAMPIMAGE input_buf) {
  JDIMENSION MCU_colNum;	// index of current MCU within row
  int blkn, ci, xindex, yindex, yoffset;
  JDIMENSION start_col;
  JBLOCKARRAY buffer[MAX_COMPS_IN_SCAN];
  JBLOCKROW buffer_ptr;
  J_COMPONENT_INFO *compptr;

  /* Align the virtual buffers for the components used in this scan.
   * NB: during first pass, this is safe only because the buffers will
   * already be aligned properly, so jmemmgr.c won't need to do any I/O.
   */
  for(ci=0; ci < m_Parent->compsInScan; ci++) {
    compptr = m_Parent->curCompInfo[ci];
    buffer[ci]= wholeImage[compptr->componentIndex]->
			accessVirtBarray(iMCU_rowNum *compptr->vSampFactor,
      (JDIMENSION)compptr->vSampFactor, FALSE);
		}

  // Loop to process one whole iMCU row
  for(yoffset = MCU_vertOffset; yoffset < MCU_rowsPer_iMCU_row; yoffset++) {
    for(MCU_colNum = mcuCtr; MCU_colNum < m_Parent->MCUs_perRow; MCU_colNum++) {
      // Construct list of pointers to DCT blocks belonging to this MCU
      blkn=0;			// index of current DCT block within MCU
      for(ci=0; ci < m_Parent->compsInScan; ci++) {
				compptr=m_Parent->curCompInfo[ci];
				start_col = MCU_colNum * compptr->MCU_width;
				for(yindex=0; yindex < compptr->MCU_height; yindex++) {
					buffer_ptr = buffer[ci][yindex+yoffset] + start_col;
					for(xindex = 0; xindex < compptr->MCU_width; xindex++) {
						MCU_buffer[blkn++] = buffer_ptr++;
					}
				}
      }
      // Try to write the MCU.
		if(!(m_Parent->entropy->*(m_Parent->entropy)->doEncodeMCU)(MCU_buffer)) {
	// Suspension forced; update state counters and exit
			MCU_vertOffset = yoffset;
			mcuCtr = MCU_colNum;
			return FALSE;
			}
    }
    // Completed an MCU row, but perhaps not an iMCU row
    mcuCtr = 0;
  }
  // Completed the iMCU row, advance counters for next one
  iMCU_rowNum++;
  start_iMCU_row();
  return TRUE;
	}

#endif // FULL_COEF_BUFFER_SUPPORTED


/*
 * Initialize coefficient buffer controller.
 */
CCompCoefController::CCompCoefController(CCompressJpeg *p,BOOL needFullBuffer) : m_Parent(p) {

  virtBarrayList = NULL;
  // Create the coefficient buffer.
  if(needFullBuffer) {
#ifdef FULL_COEF_BUFFER_SUPPORTED
    /* Allocate a full-image virtual array for each component, */
    /* padded to a multiple of samp_factor DCT blocks in each direction. */
    int ci;
    J_COMPONENT_INFO *compptr;

    for(ci=0, compptr = p->compInfo; ci<p->numComponents; ci++, compptr++) {
      wholeImage[ci]=requestVirtBarray(JPOOL_IMAGE, FALSE,
				(JDIMENSION)CJpeg::roundUp((long)compptr->widthInBlocks,
				(long)compptr->hSampFactor),
				(JDIMENSION)CJpeg::roundUp((long)compptr->heightInBlocks,
				(long)compptr->vSampFactor),
				(JDIMENSION)compptr->vSampFactor);
			}
#else
			m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
#endif
		} 
	else {
    // We only need a single-MCU buffer.
    JBLOCKROW buffer;
    int i;

    buffer=(JBLOCKROW)
      m_Parent->m_Parent->mem->allocLarge(JPOOL_IMAGE,C_MAX_BLOCKS_IN_MCU * sizeof(JBLOCK));
		for(i=0; i<C_MAX_BLOCKS_IN_MCU; i++) {
			MCU_buffer[i] = buffer + i;
			}
		wholeImage[0] = NULL; // flag for no virtual arrays
		}
	}

CCompCoefController::~CCompCoefController() {
	CVirtBArray *bptr,*bptr2;

  for(bptr = virtBarrayList; bptr != NULL; bptr = bptr->next) {
		bptr2=bptr->next;
    if(bptr->b_s_info) {	// there may be no backing store
			delete bptr->b_s_info;
			}
		delete bptr;
		bptr=bptr2;
		}
  virtBarrayList = NULL;
	}


/*
 * jccolor.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains input colorspace conversion routines.
 */





/**************** RGB -> YCbCr conversion: most common case **************/

/*
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *	Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
 *	Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + CENTERJSAMPLE
 *	Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B  + CENTERJSAMPLE
 * (These numbers are derived from TIFF 6.0 section 21, dated 3-June-92.)
 * Note: older versions of the IJG code used a zero offset of MAXJSAMPLE/2,
 * rather than CENTERJSAMPLE, for Cb and Cr.  This gave equal positive and
 * negative swings for Cb/Cr, but meant that grayscale values (Cb=Cr=0)
 * were not represented exactly.  Now we sacrifice exact representation of
 * maximum red and maximum blue in order to get exact grayscales.
 *
 * To avoid floating-point arithmetic, we represent the fractional constants
 * as integers scaled up by 2^16 (about 4 digits precision); we have to divide
 * the products by 2^16, with appropriate rounding, to get the correct answer.
 *
 * For even more speed, we avoid doing any multiplications in the inner loop
 * by precalculating the constants times R,G,B for all possible values.
 * For 8-bit JSAMPLEs this is very reasonable (only 256 entries per table);
 * for 12-bit samples it is still acceptable.  It's not very reasonable for
 * 16-bit samples, but if you want lossless storage you shouldn't be changing
 * colorspace anyway.
 * The CENTERJSAMPLE offsets and the rounding fudge-factor of 0.5 are included
 * in the tables to save adding them separately in the inner loop.
 */

#define SCALEBITS	16	// speediest right-shift on some machines
#define CBCR_OFFSET	((INT32) CENTERJSAMPLE << SCALEBITS)
#define ONE_HALF	((INT32) 1 << (SCALEBITS-1))
#define FIX(x)		((INT32) ((x) * (1L<<SCALEBITS) + 0.5))

/* We allocate one big table and divide it up into eight parts, instead of
 * doing eight alloc_small requests.  This lets us use a single table base
 * address, which can be held in a register in the inner loops on many
 * machines (more than can hold all eight addresses, anyway).
 */

#define R_Y_OFF		0								// offset to R => Y section 
#define G_Y_OFF		(1*(MAXJSAMPLE+1))	// offset to G => Y section 
#define B_Y_OFF		(2*(MAXJSAMPLE+1))	// etc. 
#define R_CB_OFF	(3*(MAXJSAMPLE+1))
#define G_CB_OFF	(4*(MAXJSAMPLE+1))
#define B_CB_OFF	(5*(MAXJSAMPLE+1))
#define R_CR_OFF	B_CB_OFF				// B=>Cb, R=>Cr are the same 
#define G_CR_OFF	(6*(MAXJSAMPLE+1))
#define B_CR_OFF	(7*(MAXJSAMPLE+1))
#define TABLE_SIZE	(8*(MAXJSAMPLE+1))


/*
 * Initialize for RGB->YCC colorspace conversion.
 */

INT32 CColorConverter::rgb_ycc_start() {

  INT32 i;

  // Allocate and fill in the conversion tables. 
  rgb_ycc_tab=(INT32 *)m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,(TABLE_SIZE * sizeof(INT32)));

  for(i=0; i <= MAXJSAMPLE; i++) {
    rgb_ycc_tab[i+R_Y_OFF] = FIX(0.29900) * i;
    rgb_ycc_tab[i+G_Y_OFF] = FIX(0.58700) * i;
    rgb_ycc_tab[i+B_Y_OFF] = FIX(0.11400) * i     + ONE_HALF;
    rgb_ycc_tab[i+R_CB_OFF] = (-FIX(0.16874)) * i;
    rgb_ycc_tab[i+G_CB_OFF] = (-FIX(0.33126)) * i;
    /* We use a rounding fudge-factor of 0.5-epsilon for Cb and Cr.
     * This ensures that the maximum output will round to MAXJSAMPLE
     * not MAXJSAMPLE+1, and thus that we don't have to range-limit.
     */
    rgb_ycc_tab[i+B_CB_OFF] = FIX(0.50000) * i    + CBCR_OFFSET + ONE_HALF-1;
/*  B=>Cb and R=>Cr tables are the same
    cconvert->rgb_ycc_tab[i+R_CR_OFF] = FIX(0.50000) * i    + CBCR_OFFSET + ONE_HALF-1;
*/
    rgb_ycc_tab[i+G_CR_OFF] = (-FIX(0.41869)) * i;
    rgb_ycc_tab[i+B_CR_OFF] = (-FIX(0.08131)) * i;
		}
	return 0;
	}


/*
 * Convert some rows of samples to the JPEG colorspace.
 *
 * Note that we change from the application's interleaved-pixel format
 * to our internal noninterleaved, one-plane-per-component format.
 * The input buffer is therefore three times as wide as the output buffer.
 *
 * A starting row offset is provided only for the output buffer.  The caller
 * can easily adjust the passed input_buf value to accommodate any row
 * offset required on that side.
 */

void CColorConverter::rgb_ycc_convert(JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
		 JDIMENSION output_row, int num_rows) {
  register int r,g,b;
  register INT32 *ctab = rgb_ycc_tab;
  register JSAMPROW inptr;
  register JSAMPROW outptr0, outptr1, outptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = m_Parent->imageWidth;

  while(--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    output_row++;
    for(col=0; col<num_cols; col++) {
      r = GETJSAMPLE(inptr[RGB_RED]);
      g = GETJSAMPLE(inptr[RGB_GREEN]);
      b = GETJSAMPLE(inptr[RGB_BLUE]);
      inptr += RGB_PIXELSIZE;
      /* If the inputs are 0..MAXJSAMPLE, the outputs of these equations
       * must be too; we do not need an explicit range-limiting operation.
       * Hence the value being shifted is never negative, and we don't
       * need the general RIGHT_SHIFT macro.
       */
      // Y 
			outptr0[col]=(JSAMPLE)((ctab[r+R_Y_OFF] + ctab[g+G_Y_OFF] + ctab[b+B_Y_OFF]) >> SCALEBITS);
      // Cb 
			outptr1[col]=(JSAMPLE)((ctab[r+R_CB_OFF] + ctab[g+G_CB_OFF] + ctab[b+B_CB_OFF]) >> SCALEBITS);
      // Cr 
			outptr2[col]=(JSAMPLE)((ctab[r+R_CR_OFF] + ctab[g+G_CR_OFF] + ctab[b+B_CR_OFF])	>> SCALEBITS);
			}
		}
	}


/**************** Cases other than RGB -> YCbCr **************/


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles RGB->grayscale conversion, which is the same
 * as the RGB->Y portion of RGB->YCbCr.
 * We assume rgb_ycc_start has been called (we only use the Y tables).
 */

void CColorConverter::rgb_gray_convert(JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
	JDIMENSION output_row, int num_rows) {
  register int r,g,b;
  register INT32 *ctab = rgb_ycc_tab;
  register JSAMPROW inptr;
  register JSAMPROW outptr;
  register JDIMENSION col;
  JDIMENSION num_cols = m_Parent->imageWidth;

  while(--num_rows >= 0) {
    inptr = *input_buf++;
    outptr = output_buf[0][output_row];
    output_row++;
    for(col=0; col < num_cols; col++) {
      r= GETJSAMPLE(inptr[RGB_RED]);
      g= GETJSAMPLE(inptr[RGB_GREEN]);
      b= GETJSAMPLE(inptr[RGB_BLUE]);
      inptr += RGB_PIXELSIZE;
      // Y 
      outptr[col] = (JSAMPLE)((ctab[r+R_Y_OFF] + ctab[g+G_Y_OFF] + ctab[b+B_Y_OFF]) >> SCALEBITS);
			}
		}
	}


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles Adobe-style CMYK->YCCK conversion,
 * where we convert R=1-C, G=1-M, and B=1-Y to YCbCr using the same
 * conversion as above, while passing K (black) unchanged.
 * We assume rgb_ycc_start has been called.
 */

void CColorConverter::cmyk_ycck_convert(JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
		   JDIMENSION outputRow, int numRows) {
  register int r,g,b;
  register INT32 *ctab = rgb_ycc_tab;
  register JSAMPROW inptr;
  register JSAMPROW outptr0, outptr1, outptr2, outptr3;
  register JDIMENSION col;
  JDIMENSION num_cols=m_Parent->imageWidth;

  while(--numRows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][outputRow];
    outptr1 = output_buf[1][outputRow];
    outptr2 = output_buf[2][outputRow];
    outptr3 = output_buf[3][outputRow];
    outputRow++;
    for(col = 0; col < num_cols; col++) {
      r=MAXJSAMPLE - GETJSAMPLE(inptr[0]);
      g=MAXJSAMPLE - GETJSAMPLE(inptr[1]);
      b=MAXJSAMPLE - GETJSAMPLE(inptr[2]);
      // K passes through as-is 
      outptr3[col] = inptr[3];	// don't need GETJSAMPLE here 
      inptr += 4;
      /* If the inputs are 0..MAXJSAMPLE, the outputs of these equations
       * must be too; we do not need an explicit range-limiting operation.
       * Hence the value being shifted is never negative, and we don't
       * need the general RIGHT_SHIFT macro.
       */
      // Y 
      outptr0[col]=(JSAMPLE)((ctab[r+R_Y_OFF] + ctab[g+G_Y_OFF] + ctab[b+B_Y_OFF]) >> SCALEBITS);
      // Cb 
      outptr1[col]=(JSAMPLE)((ctab[r+R_CB_OFF] + ctab[g+G_CB_OFF] + ctab[b+B_CB_OFF]) >> SCALEBITS);
      // Cr 
      outptr2[col]=(JSAMPLE)((ctab[r+R_CR_OFF] + ctab[g+G_CR_OFF] + ctab[b+B_CR_OFF]) >> SCALEBITS);
			}
		}
	}


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles grayscale output with no conversion.
 * The source can be either plain grayscale or YCbCr (since Y == gray).
 */

void CColorConverter::grayscaleConvert(JSAMPARRAY inputBuf, JSAMPIMAGE outputBuf,
	JDIMENSION outputRow, int numRows) {
  register JSAMPROW inptr;
  register JSAMPROW outptr;
  register JDIMENSION col;
  JDIMENSION num_cols = m_Parent->imageWidth;
  int instride = m_Parent->inputComponents;

  while(--numRows >= 0) {
    inptr= *inputBuf++;
    outptr=outputBuf[0][outputRow];
    outputRow++;
    for(col=0; col<num_cols; col++) {
      outptr[col]=inptr[0];	// don't need GETJSAMPLE() here 
      inptr+=instride;
			}
		}
	}


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles multi-component colorspaces without conversion.
 * We assume input_components == num_components.
 */

void CColorConverter::nullConvert(JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
	JDIMENSION output_row, int num_rows) {
  register JSAMPROW inptr;
  register JSAMPROW outptr;
  register JDIMENSION col;
  register int ci;
  int nc = m_Parent->numComponents;
  JDIMENSION num_cols = m_Parent->imageWidth;

  while(--num_rows >= 0) {
    // It seems fastest to make a separate pass for each component. 
    for(ci=0; ci < nc; ci++) {
      inptr= *input_buf;
      outptr= output_buf[ci][output_row];
      for(col=0; col < num_cols; col++) {
				outptr[col] = inptr[ci]; // don't need GETJSAMPLE() here 
				inptr+= nc;
				}
			}
    input_buf++;
    output_row++;
		}
	}


/*
 * Empty method for start_pass.
 */

INT32 CColorConverter::nullMethod() {
  // no work needed 
	return 0;
	}


/*
 * Module initialization routine for input colorspace conversion.
 */
CColorConverter::CColorConverter(CCompressJpeg *p) : m_Parent(p) {  // set start_pass to null method until we find out differently

  doStartPass=nullMethod;
  // Make sure input_components agrees with in_color_space
  switch(p->inColorSpace) {
		case JCS_GRAYSCALE:
			if(p->inputComponents != 1)
				p->m_Parent->err->errorExit(JERR_BAD_IN_COLORSPACE);
			break;

		case JCS_RGB:
#if RGB_PIXELSIZE != 3
			if(p->inputComponents != RGB_PIXELSIZE)
				p->m_Parent->err->errorExit(JERR_BAD_IN_COLORSPACE);
			break;
#endif // else share code with YCbCr 

		case JCS_YCbCr:
			if(p->inputComponents != 3)
				p->m_Parent->err->errorExit(JERR_BAD_IN_COLORSPACE);
			break;

		case JCS_CMYK:
		case JCS_YCCK:
			if(p->inputComponents != 4)
				p->m_Parent->err->errorExit(JERR_BAD_IN_COLORSPACE);
			break;

		default:			// JCS_UNKNOWN can be anything 
			if(p->inputComponents < 1)
				p->m_Parent->err->errorExit(JERR_BAD_IN_COLORSPACE);
			break;
		}

  // Check num_components, set conversion method based on requested space 
  switch(p->colorSpace) {
		case JCS_GRAYSCALE:
			if(p->numComponents != 1)
				p->m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
			switch(p->inColorSpace) {
				case JCS_GRAYSCALE:
					doColorConvert = grayscaleConvert;
					break;
				case JCS_RGB:
					doStartPass = rgb_ycc_start;
					doColorConvert = rgb_gray_convert;
					break;
				case JCS_YCbCr:
					doColorConvert = grayscaleConvert;
					break;
				default:
					p->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
					break;
				}
			break;

		case JCS_RGB:
			if(p->numComponents != 3)
				p->m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
			if(p->inColorSpace == JCS_RGB && RGB_PIXELSIZE == 3)
				doColorConvert = nullConvert;
			else
				p->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
			break;

		case JCS_YCbCr:
			if(p->numComponents != 3)
				p->m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
			switch(p->inColorSpace) {
				case JCS_RGB:
					doStartPass = rgb_ycc_start;
					doColorConvert = rgb_ycc_convert;
					break;
				case JCS_YCbCr:
					doColorConvert = nullConvert;
					break;
				default:
					p->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
					break;
				}
			break;

		case JCS_CMYK:
			if(p->numComponents != 4)
				p->m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
			if(p->inColorSpace == JCS_CMYK)
				doColorConvert = nullConvert;
			else
				p->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
			break;

		case JCS_YCCK:
			if(p->numComponents != 4)
				p->m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
			switch(p->inColorSpace) {
				case JCS_CMYK:
					doStartPass = rgb_ycc_start;
					doColorConvert = cmyk_ycck_convert;
					break;
				case JCS_YCCK:
					doColorConvert = nullConvert;
					break;
				default:
					p->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
					break;
				}
			break;

		default:			// allow null conversion of JCS_UNKNOWN
			if(p->colorSpace != p->inColorSpace ||
				p->numComponents != p->inputComponents)
				p->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
			doColorConvert = nullConvert;
			break;
		}
	}



/*
 * jcdctmgr.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the forward-DCT management logic.
 * This code selects a particular DCT implementation to be used,
 * and it performs related housekeeping chores including coefficient
 * quantization.
 */




/*
 * Initialize for a processing pass.
 * Verify that all referenced Q-tables are present, and set up
 * the divisor table for each one.
 * In the current implementation, DCT of all components is done during
 * the first pass, even if only some components will be output in the
 * first scan.  Hence all components should be examined here.
 */

void CForwardDCT::startPass() {
  int ci, qtblno, i;
  J_COMPONENT_INFO *compptr;
  JQUANT_TBL * qtbl;
//  DCTELEM * dtbl;

  for(ci=0, compptr=m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    qtblno=compptr->quant_tbl_no;
    // Make sure specified quantization table is present 
    if(qtblno < 0 || qtblno >= NUM_QUANT_TBLS ||
			!m_Parent->quantTblPtrs[qtblno])
      m_Parent->m_Parent->err->errorExit(JERR_NO_QUANT_TABLE, qtblno);
    qtbl=m_Parent->quantTblPtrs[qtblno];
    // Compute divisors for this quant table
    /* We may do this more than once for same table, but it's not a big deal */
    switch(m_Parent->DCT_Method) {
#ifdef DCT_ISLOW_SUPPORTED
    case JDCT_ISLOW:
      /* For LL&M IDCT method, divisors are equal to raw quantization
       * coefficients multiplied by 8 (to counteract scaling).
       */
      if(divisors[qtblno] == NULL) {
				divisors[qtblno] = (DCTELEM *)
				mem->allocSmall(JPOOL_IMAGE,DCTSIZE2 * sizeof(DCTELEM));
				}
      dtbl=divisors[qtblno];
      for(i=0; i < DCTSIZE2; i++) {
				dtbl[i]=((DCTELEM)qtbl->quantval[i]) << 3;
				}
      break;
#endif
#ifdef DCT_IFAST_SUPPORTED
    case JDCT_IFAST:
      {
	/* For AA&N IDCT method, divisors are equal to quantization
	 * coefficients scaled by scalefactor[row]*scalefactor[col], where
	 *   scalefactor[0] = 1
	 *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
	 * We apply a further scale factor of 8.
	 */
#define CONST_BITS 14
			static const INT16 aanscales[DCTSIZE2] = {
				/* precomputed values scaled up by 14 bits */
				16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
				22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
				21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
				19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
				16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
				12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
				 8867, 12299, 11585, 10426,  8867,  6967,  4799,  2446,
				 4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247
			};
			SHIFT_TEMPS

		if(divisors[qtblno] == NULL) {
			divisors[qtblno] = (DCTELEM *)
				mem->allocSmall(JPOOL_IMAGE, DCTSIZE2 * sizeof(DCTELEM));
				}
			dtbl=divisors[qtblno];
			for(i=0; i < DCTSIZE2; i++) {
				dtbl[i] = (DCTELEM)
					DESCALE(MULTIPLY16V16((INT32) qtbl->quantval[i],
						(INT32)aanscales[i]),	CONST_BITS-3);
				}
      }
      break;
#endif
#ifdef DCT_FLOAT_SUPPORTED
    case JDCT_FLOAT:
      {
	/* For float AA&N IDCT method, divisors are equal to quantization
	 * coefficients scaled by scalefactor[row]*scalefactor[col], where
	 *   scalefactor[0] = 1
	 *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
	 * We apply a further scale factor of 8.
	 * What's actually stored is 1/divisor so that the inner loop can
	 * use a multiplication rather than a division.
	 */
		double *fdtbl;
		int row, col;
		static const double aanscalefactor[DCTSIZE] = {
			1.0, 1.387039845, 1.306562965, 1.175875602,
			1.0, 0.785694958, 0.541196100, 0.275899379
			};

		if(!floatDivisors[qtblno]) {
			floatDivisors[qtblno]= (double *)
				m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE, DCTSIZE2 * sizeof(double));
			}
		fdtbl=floatDivisors[qtblno];
		i=0;
		for(row=0; row < DCTSIZE; row++) {
			for(col=0; col < DCTSIZE; col++) {
				fdtbl[i]=(double)
					(1.0 / (((double) qtbl->quantVal[i] *
						 aanscalefactor[row] * aanscalefactor[col] * 8.0)));
				i++;
				}
			}
      }
      break;
#endif
			default:
				m_Parent->m_Parent->err->errorExit(JERR_NOT_COMPILED);
				break;
			}
		}
	}


/*
 * Perform forward DCT on one or more blocks of a component.
 *
 * The input samples are taken from the sample_data[] array starting at
 * position start_row/start_col, and moving to the right for any additional
 * blocks. The quantized coefficients are returned in coef_blocks[].
 */

void CForwardDCT::forward_DCT(J_COMPONENT_INFO *compptr,
	     JSAMPARRAY sample_data, JBLOCKROW coef_blocks,
	     JDIMENSION start_row, JDIMENSION start_col,
	     JDIMENSION num_blocks) {
// This version is used for integer DCT implementations. 

  // This routine is heavily used, so it's worth coding it tightly. 
  DCTELEM *divs=divisors[compptr->quant_tbl_no];
  DCTELEM workspace[DCTSIZE2];	// work area for FDCT subroutine 
  JDIMENSION bi;

  sample_data += start_row;	// fold in the vertical offset once

  for(bi=0; bi<num_blocks; bi++, start_col += DCTSIZE) {
    // Load data into workspace, applying unsigned->signed conversion
    { register DCTELEM *workspaceptr;
      register JSAMPROW elemptr;
      register int elemr;

      workspaceptr = workspace;
      for(elemr=0; elemr < DCTSIZE; elemr++) {
				elemptr=sample_data[elemr] + start_col;
#if DCTSIZE == 8		// unroll the inner loop 
				*workspaceptr++ = GETJSAMPLE(*elemptr++) - CENTERJSAMPLE;
				*workspaceptr++ = GETJSAMPLE(*elemptr++) - CENTERJSAMPLE;
				*workspaceptr++ = GETJSAMPLE(*elemptr++) - CENTERJSAMPLE;
				*workspaceptr++ = GETJSAMPLE(*elemptr++) - CENTERJSAMPLE;
				*workspaceptr++ = GETJSAMPLE(*elemptr++) - CENTERJSAMPLE;
				*workspaceptr++ = GETJSAMPLE(*elemptr++) - CENTERJSAMPLE;
				*workspaceptr++ = GETJSAMPLE(*elemptr++) - CENTERJSAMPLE;
				*workspaceptr++ = GETJSAMPLE(*elemptr++) - CENTERJSAMPLE;
#else
				{ register int elemc;
					for(elemc = DCTSIZE; elemc > 0; elemc--) {
						*workspaceptr++ = GETJSAMPLE(*elemptr++) - CENTERJSAMPLE;
					}
				}
#endif
      }
    }

    // Perform the DCT
    (this->*doDCT)(workspace);

    // Quantize/descale the coefficients, and store into coef_blocks[]
    { register DCTELEM temp, qval;
      register int i;
      register JCOEFPTR output_ptr = coef_blocks[bi];

      for(i=0; i<DCTSIZE2; i++) {
				qval=divs[i];
				temp=workspace[i];
	/* Divide the coefficient value by qval, ensuring proper rounding.
	 * Since C does not specify the direction of rounding for negative
	 * quotients, we have to force the dividend positive for portability.
	 *
	 * In most files, at least half of the output values will be zero
	 * (at default quantization settings, more like three-quarters...)
	 * so we should ensure that this case is fast.  On many machines,
	 * a comparison is enough cheaper than a divide to make a special test
	 * a win.  Since both inputs will be nonnegative, we need only test
	 * for a < b to discover whether a/b is 0.
	 * If your machine's division is fast enough, define FAST_DIVIDE.
	 */
#define DIVIDE_BY(a,b)	a /= b
				if(temp<0) {
					temp=-temp;
					temp+=qval >> 1;	// for rounding
					DIVIDE_BY(temp,qval);
					temp=-temp;
					} 
				else {
					temp += qval>>1;	// for rounding
					DIVIDE_BY(temp, qval);
					}
				output_ptr[i] = (JCOEF) temp;
				}
			}
		}
	}


#ifdef DCT_FLOAT_SUPPORTED

void CForwardDCT::forward_DCT_float(J_COMPONENT_INFO *compptr,
	JSAMPARRAY sample_data, JBLOCKROW coef_blocks,
	JDIMENSION start_row, JDIMENSION start_col,
	JDIMENSION num_blocks) {
// This version is used for floating-point DCT implementations. 
  // This routine is heavily used, so it's worth coding it tightly. 
//  float_DCT_method_ptr do_dct = fdct->do_float_dct;
  double *divs = floatDivisors[compptr->quant_tbl_no];
  double workspace[DCTSIZE2]; // work area for FDCT subroutine
  JDIMENSION bi;

  sample_data += start_row;	// fold in the vertical offset once

  for(bi=0; bi < num_blocks; bi++, start_col += DCTSIZE) {
    // Load data into workspace, applying unsigned->signed conversion
    { register double *workspaceptr;
      register JSAMPROW elemptr;
      register int elemr;

      workspaceptr=workspace;
      for(elemr=0; elemr<DCTSIZE; elemr++) {
				elemptr=sample_data[elemr] + start_col;
#if DCTSIZE == 8		// unroll the inner loop
				*workspaceptr++ =(double)(GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
				*workspaceptr++ =(double)(GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
				*workspaceptr++ =(double)(GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
				*workspaceptr++ =(double)(GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
				*workspaceptr++ =(double)(GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
				*workspaceptr++ =(double)(GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
				*workspaceptr++ =(double)(GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
				*workspaceptr++ =(double)(GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
#else
				{ register int elemc;
					for(elemc = DCTSIZE; elemc > 0; elemc--)
						*workspaceptr++ =(double)(GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
				}
#endif
      }
    }

    // Perform the DCT 
    (this->*doFloatDCT)(workspace);

    // Quantize/descale the coefficients, and store into coef_blocks[] 
    { register double temp;
      register int i;
      register JCOEFPTR output_ptr = coef_blocks[bi];

      for(i=0; i<DCTSIZE2; i++) {
	// Apply the quantization and scaling factor 
				temp=workspace[i] * divs[i];
	/* Round to nearest integer.
	 * Since C does not specify the direction of rounding for negative
	 * quotients, we have to force the dividend positive for portability.
	 * The maximum coefficient size is +-16K (for 12-bit data), so this
	 * code should work for either 16-bit or 32-bit ints.
	 */
				output_ptr[i]=(JCOEF)((int)(temp + (double) 16384.5) - 16384);
				}
			}
		}
	}

#endif // DCT_FLOAT_SUPPORTED


/*
 * Initialize FDCT manager.
 */
CForwardDCT::CForwardDCT(CCompressJpeg *p) : m_Parent(p) {
  int i;

  switch(p->DCT_Method) {
#ifdef DCT_ISLOW_SUPPORTED
		case JDCT_ISLOW:
			doForwardDCT=forward_DCT;
			doDCT=fdct_islow;
			break;
#endif
#ifdef DCT_IFAST_SUPPORTED
		case JDCT_IFAST:
			doForwardDCT=forward_DCT;
			doDCT=fdct_ifast;
			break;
#endif
#ifdef DCT_FLOAT_SUPPORTED
		case JDCT_FLOAT:
			doForwardDCT=forward_DCT_float;
			doFloatDCT=fdctFloat;
			break;
#endif
		default:
			p->m_Parent->err->errorExit(JERR_NOT_COMPILED);
			break;
	  }

  // Mark divisor tables unallocated
  for(i=0; i<NUM_QUANT_TBLS; i++) {
    divisors[i]=NULL;
#ifdef DCT_FLOAT_SUPPORTED
    floatDivisors[i]=NULL;
#endif
		}
	}




/*
 * The decompressor input side (jdinput.c) saves away the appropriate
 * quantization table for each component at the start of the first scan
 * involving that component.  (This is necessary in order to correctly
 * decode files that reuse Q-table slots.)
 * When we are ready to make an output pass, the saved Q-table is converted
 * to a multiplier table that will actually be used by the IDCT routine.
 * The multiplier table contents are IDCT-method-dependent.  To support
 * application changes in IDCT method between scans, we can remake the
 * multiplier tables if necessary.
 * In buffered-image mode, the first output pass may occur before any data
 * has been seen for some components, and thus before their Q-tables have
 * been saved away.  To handle this case, multiplier tables are preset
 * to zeroes; the result of the IDCT will be a neutral gray level.
 */



/* Allocated multiplier tables: big enough for any supported variant */

typedef union {
  ISLOW_MULT_TYPE islowArray[DCTSIZE2];
#ifdef DCT_IFAST_SUPPORTED
  IFAST_MULT_TYPE ifastArray[DCTSIZE2];
#endif
#ifdef DCT_FLOAT_SUPPORTED
  FLOAT_MULT_TYPE floatArray[DCTSIZE2];
#endif
	} MULTIPLIER_TABLE;


/* The current scaled-IDCT routines require ISLOW-style multiplier tables,
 * so be sure to compile that code if either ISLOW or SCALING is requested.
 */
#ifdef DCT_ISLOW_SUPPORTED
#define PROVIDE_ISLOW_TABLES
#else
#ifdef IDCT_SCALING_SUPPORTED
#define PROVIDE_ISLOW_TABLES
#endif
#endif


/*
 * Prepare for an output pass.
 * Here we select the proper IDCT routine for each component and build
 * a matching multiplier table.
 */

void CInverseDCT::startPass() {
  int ci, i;
  J_COMPONENT_INFO *compptr;
  void (CInverseDCT::*methodPtr) (J_COMPONENT_INFO *, JCOEFPTR,JSAMPARRAY, JDIMENSION);
  int method = 0;
  JQUANT_TBL * qtbl;

  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {

    /* Select the proper IDCT routine for this component's scaling */
    switch(compptr->DCT_scaledSize) {
#ifdef IDCT_SCALING_SUPPORTED
			case 1:
				methodPtr = jpeg_idct_1x1;
				method = JDCT_ISLOW;	/* jidctred uses islow-style table */
				break;
			case 2:
				methodPtr = jpeg_idct_2x2;
				method = JDCT_ISLOW;	/* jidctred uses islow-style table */
				break;
			case 4:
				methodPtr = jpeg_idct_4x4;
				method = JDCT_ISLOW;	/* jidctred uses islow-style table */
				break;
#endif
			case DCTSIZE:
				switch(m_Parent->dctMethod) {
#ifdef DCT_ISLOW_SUPPORTED
					case JDCT_ISLOW:
						methodPtr = inverse_DCT_islow;
						method = JDCT_ISLOW;
						break;
#endif
#ifdef DCT_IFAST_SUPPORTED
					case JDCT_IFAST:
						methodPtr = inverse_DCT_ifast;
						method = JDCT_IFAST;
						break;
#endif
#ifdef DCT_FLOAT_SUPPORTED
					case JDCT_FLOAT:
						methodPtr = inverse_DCT_float;
						method = JDCT_FLOAT;
						break;
#endif
					default:
						m_Parent->m_Parent->err->errorExit(JERR_NOT_COMPILED);
						break;
					}
				break;
			default:
				m_Parent->m_Parent->err->errorExit(JERR_BAD_DCTSIZE, compptr->DCT_scaledSize);
				break;
			}
    doInverse_DCT[ci] = methodPtr;
    /* Create multiplier table from quant table.
     * However, we can skip this if the component is uninteresting
     * or if we already built the table.  Also, if no quant table
     * has yet been saved for the component, we leave the
     * multiplier table all-zero; we'll be reading zeroes from the
     * coefficient controller's buffer anyway.
     */
    if(! compptr->componentNeeded || curMethod[ci] == method)
      continue;
    qtbl = compptr->quantTable;
    if(qtbl == NULL)		/* happens if no data yet for component */
      continue;
    curMethod[ci] = method;
    switch(method) {
#ifdef PROVIDE_ISLOW_TABLES
    case JDCT_ISLOW:
      {
	/* For LL&M IDCT method, multipliers are equal to raw quantization
	 * coefficients, but are stored as ints to ensure access efficiency.
	 */
			ISLOW_MULT_TYPE * ismtbl = (ISLOW_MULT_TYPE *) compptr->dctTable;
			for (i = 0; i < DCTSIZE2; i++) {
				ismtbl[i] = (ISLOW_MULT_TYPE) qtbl->quantval[i];
			}
      }
      break;
#endif
#ifdef DCT_IFAST_SUPPORTED
    case JDCT_IFAST:
      {
	/* For AA&N IDCT method, multipliers are equal to quantization
	 * coefficients scaled by scalefactor[row]*scalefactor[col], where
	 *   scalefactor[0] = 1
	 *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
	 * For integer operation, the multiplier table is to be scaled by
	 * IFAST_SCALE_BITS.
	 */
			IFAST_MULT_TYPE *ifmtbl = (IFAST_MULT_TYPE *) compptr->dct_table;
#define CONST_BITS 14
			static const INT16 aanscales[DCTSIZE2] = {
	  /* precomputed values scaled up by 14 bits */
				16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
				22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
				21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
				19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
				16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
				12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
				 8867, 12299, 11585, 10426,  8867,  6967,  4799,  2446,
				 4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247
				};
			SHIFT_TEMPS

			for(i=0; i < DCTSIZE2; i++) {
				ifmtbl[i] = (IFAST_MULT_TYPE)
					DESCALE(MULTIPLY16V16((INT32) qtbl->quantval[i],
							(INT32) aanscales[i]),
						CONST_BITS-IFAST_SCALE_BITS);
			}
      }
      break;
#endif
#ifdef DCT_FLOAT_SUPPORTED
    case JDCT_FLOAT:
      {
	/* For float AA&N IDCT method, multipliers are equal to quantization
	 * coefficients scaled by scalefactor[row]*scalefactor[col], where
	 *   scalefactor[0] = 1
	 *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
	 */
			FLOAT_MULT_TYPE * fmtbl = (FLOAT_MULT_TYPE *) compptr->dctTable;
			int row, col;
			static const double aanscalefactor[DCTSIZE] = {
				1.0, 1.387039845, 1.306562965, 1.175875602,
				1.0, 0.785694958, 0.541196100, 0.275899379
			};

			i = 0;
			for (row = 0; row < DCTSIZE; row++) {
				for (col = 0; col < DCTSIZE; col++) {
					fmtbl[i] = (FLOAT_MULT_TYPE)
						((double) qtbl->quantVal[i] *
						 aanscalefactor[row] * aanscalefactor[col]);
					i++;
				}
			}
      }
      break;
#endif
    default:
      m_Parent->m_Parent->err->errorExit(JERR_NOT_COMPILED);
      break;
			}
		}
	}


/*
 * Initialize IDCT manager.
 */

CInverseDCT::CInverseDCT(CDecompressJpeg *p) : m_Parent(p) {
  int ci;
  J_COMPONENT_INFO *compptr;

  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {

    /* Allocate and pre-zero a multiplier table for each component */
    compptr->dctTable =
      m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,sizeof(MULTIPLIER_TABLE));
    ZeroMemory(compptr->dctTable, sizeof(MULTIPLIER_TABLE));
    /* Mark multiplier table not yet set up for any method */
    curMethod[ci] = -1;
		}
	}




/*
 * jdcolor.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains output colorspace conversion routines.
 */


/**************** YCbCr -> RGB conversion: most common case **************/

/*
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *	R = Y                + 1.40200 * Cr
 *	G = Y - 0.34414 * Cb - 0.71414 * Cr
 *	B = Y + 1.77200 * Cb
 * where Cb and Cr represent the incoming values less CENTERJSAMPLE.
 * (These numbers are derived from TIFF 6.0 section 21, dated 3-June-92.)
 *
 * To avoid floating-point arithmetic, we represent the fractional constants
 * as integers scaled up by 2^16 (about 4 digits precision); we have to divide
 * the products by 2^16, with appropriate rounding, to get the correct answer.
 * Notice that Y, being an integral input, does not contribute any fraction
 * so it need not participate in the rounding.
 *
 * For even more speed, we avoid doing any multiplications in the inner loop
 * by precalculating the constants times Cb and Cr for all possible values.
 * For 8-bit JSAMPLEs this is very reasonable (only 256 entries per table);
 * for 12-bit samples it is still acceptable.  It's not very reasonable for
 * 16-bit samples, but if you want lossless storage you shouldn't be changing
 * colorspace anyway.
 * The Cr=>R and Cb=>B values can be rounded to integers in advance; the
 * values for the G calculation are left scaled up, since we must add them
 * together before rounding.
 */


/*
 * Initialize tables for YCC->RGB colorspace conversion.
 */

void CColorDeconverter::build_ycc_rgb_table() { 
  int i;
  INT32 x;
  SHIFT_TEMPS

  Cr_r_tab = (int *)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,
				(MAXJSAMPLE+1) * sizeof(int));
  Cb_b_tab = (int *)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,
				(MAXJSAMPLE+1) * sizeof(int));
  Cr_g_tab = (INT32 *)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,
				(MAXJSAMPLE+1) * sizeof(INT32));
  Cb_g_tab = (INT32 *)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,
				(MAXJSAMPLE+1) * sizeof(INT32));

  for(i=0, x = -CENTERJSAMPLE; i <= MAXJSAMPLE; i++, x++) {
    /* i is the actual input pixel value, in the range 0..MAXJSAMPLE */
    /* The Cb or Cr value we are thinking of is x = i - CENTERJSAMPLE */
    /* Cr=>R value is nearest int to 1.40200 * x */
    Cr_r_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.40200) * x + ONE_HALF, SCALEBITS);
    /* Cb=>B value is nearest int to 1.77200 * x */
    Cb_b_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.77200) * x + ONE_HALF, SCALEBITS);
    /* Cr=>G value is scaled-up -0.71414 * x */
    Cr_g_tab[i] = (- FIX(0.71414)) * x;
    /* Cb=>G value is scaled-up -0.34414 * x */
    /* We also add in ONE_HALF so that need not do it in inner loop */
    Cb_g_tab[i] = (- FIX(0.34414)) * x + ONE_HALF;
		}
}


/*
 * Convert some rows of samples to the output colorspace.
 *
 * Note that we change from noninterleaved, one-plane-per-component format
 * to interleaved-pixel format.  The output buffer is therefore three times
 * as wide as the input buffer.
 * A starting row offset is provided only for the input buffer.  The caller
 * can easily adjust the passed output_buf value to accommodate any row
 * offset required on that side.
 */

void CColorDeconverter::ycc_rgb_convert(JSAMPIMAGE input_buf, JDIMENSION input_row,
	JSAMPARRAY output_buf, int num_rows) {
  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = m_Parent->outputWidth;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = m_Parent->sampleRangeLimit;
  register int * Crrtab = Cr_r_tab;
  register int * Cbbtab = Cb_b_tab;
  register INT32 * Crgtab = Cr_g_tab;
  register INT32 * Cbgtab = Cb_g_tab;
  SHIFT_TEMPS

  while(--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      y  = GETJSAMPLE(inptr0[col]);
      cb = GETJSAMPLE(inptr1[col]);
      cr = GETJSAMPLE(inptr2[col]);
      /* Range-limiting is essential due to noise introduced by DCT losses. */
      outptr[RGB_RED] =   range_limit[y + Crrtab[cr]];
      outptr[RGB_GREEN] = range_limit[y +
			      ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
						 SCALEBITS))];
      outptr[RGB_BLUE] =  range_limit[y + Cbbtab[cb]];
      outptr += RGB_PIXELSIZE;
    }
  }
}


/**************** Cases other than YCbCr -> RGB **************/


/*
 * Color conversion for no colorspace change: just copy the data,
 * converting from separate-planes to interleaved representation.
 */

void CColorDeconverter::nullConvert(JSAMPIMAGE input_buf, JDIMENSION input_row,
	JSAMPARRAY output_buf, int num_rows) {
  register JSAMPROW inptr, outptr;
  register JDIMENSION count;
  register int num_components = m_Parent->numComponents;
  JDIMENSION num_cols = m_Parent->outputWidth;
  int ci;

  while(--num_rows >= 0) {
    for(ci = 0; ci < num_components; ci++) {
      inptr = input_buf[ci][input_row];
      outptr = output_buf[0] + ci;
      for (count = num_cols; count > 0; count--) {
				*outptr = *inptr++;	/* needn't bother with GETJSAMPLE() here */
				outptr += num_components;
				}
		  }
    input_row++;
    output_buf++;
		}
	}


/*
 * Color conversion for grayscale: just copy the data.
 * This also works for YCbCr -> grayscale conversion, in which
 * we just copy the Y (luminance) component and ignore chrominance.
 */

void CColorDeconverter::grayscaleConvert(JSAMPIMAGE input_buf, JDIMENSION input_row,
	JSAMPARRAY output_buf, int num_rows) {

	CCompressJpeg::copySampleRows(input_buf[0], (int) input_row, output_buf, 0,
		num_rows, m_Parent->outputWidth);
	}


/*
 * Adobe-style YCCK->CMYK conversion.
 * We convert YCbCr to R=1-C, G=1-M, and B=1-Y using the same
 * conversion as above, while passing K (black) unchanged.
 * We assume build_ycc_rgb_table has been called.
 */

void CColorDeconverter::ycck_cmyk_convert(JSAMPIMAGE input_buf, JDIMENSION input_row,
	JSAMPARRAY output_buf, int num_rows) {

  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2, inptr3;
  register JDIMENSION col;
  JDIMENSION num_cols = m_Parent->outputWidth;
  /* copy these pointers into registers if possible */
  register JSAMPLE *range_limit = m_Parent->sampleRangeLimit;
  register int *Crrtab = Cr_r_tab;
  register int *Cbbtab = Cb_b_tab;
  register INT32 *Crgtab = Cr_g_tab;
  register INT32 *Cbgtab = Cb_g_tab;
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    inptr3 = input_buf[3][input_row];
    input_row++;
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      y  = GETJSAMPLE(inptr0[col]);
      cb = GETJSAMPLE(inptr1[col]);
      cr = GETJSAMPLE(inptr2[col]);
      /* Range-limiting is essential due to noise introduced by DCT losses. */
      outptr[0] = range_limit[MAXJSAMPLE - (y + Crrtab[cr])];	/* red */
      outptr[1] = range_limit[MAXJSAMPLE - (y +			/* green */
			      ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
						 SCALEBITS)))];
      outptr[2] = range_limit[MAXJSAMPLE - (y + Cbbtab[cb])];	/* blue */
      /* K passes through unchanged */
      outptr[3] = inptr3[col];	/* don't need GETJSAMPLE here */
      outptr += 4;
    }
  }
}


/*
 * Empty method for start_pass.
 */

void CColorDeconverter::startPass() {
  /* no work needed */
	}


/*
 * Module initialization routine for output colorspace conversion.
 */

CColorDeconverter::CColorDeconverter(CDecompressJpeg *p) : m_Parent(p) {
  int ci;

  /* Make sure num_components agrees with jpeg_color_space */
  switch(m_Parent->colorSpace) {
		case JCS_GRAYSCALE:
			if(m_Parent->numComponents != 1)
				m_Parent->m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
			break;

		case JCS_RGB:
		case JCS_YCbCr:
			if(m_Parent->numComponents != 3)
				m_Parent->m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
			break;

		case JCS_CMYK:
		case JCS_YCCK:
			if(m_Parent->numComponents != 4)
				m_Parent->m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
			break;

		default:			/* JCS_UNKNOWN can be anything */
			if(m_Parent->numComponents < 1)
				m_Parent->m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
			break;
		  }

  /* Set out_color_components and conversion method based on requested space.
   * Also clear the component_needed flags for any unused components,
   * so that earlier pipeline stages can avoid useless computation.
   */

  switch(m_Parent->outColorSpace) {
		case JCS_GRAYSCALE:
			m_Parent->outColorComponents = 1;
			if(m_Parent->colorSpace == JCS_GRAYSCALE ||
				m_Parent->colorSpace == JCS_YCbCr) {
				doColorConvert = grayscaleConvert;
      /* For color->grayscale conversion, only the Y (0) component is needed */
				for(ci=1; ci < m_Parent->numComponents; ci++)
					m_Parent->compInfo[ci].componentNeeded = FALSE;
				}
			else
				m_Parent->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
			break;

		case JCS_RGB:
			m_Parent->outColorComponents = RGB_PIXELSIZE;
			if(m_Parent->colorSpace == JCS_YCbCr) {
				doColorConvert = ycc_rgb_convert;
				build_ycc_rgb_table();
				}
			else if(m_Parent->colorSpace == JCS_RGB && RGB_PIXELSIZE == 3) {
				doColorConvert = nullConvert;
				}
			else
				m_Parent->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
			break;

		case JCS_CMYK:
			m_Parent->outColorComponents = 4;
			if(m_Parent->colorSpace == JCS_YCCK) {
				doColorConvert = ycck_cmyk_convert;
				build_ycc_rgb_table();
				}
			else if(m_Parent->colorSpace == JCS_CMYK) {
				doColorConvert = nullConvert;
				}
			else
				m_Parent->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
			break;

		default:
    /* Permit null conversion to same output space */
			if(m_Parent->outColorSpace == m_Parent->colorSpace) {
				m_Parent->outColorComponents = m_Parent->numComponents;
				doColorConvert = nullConvert;
				}
			else			/* unsupported non-null conversion */
				m_Parent->m_Parent->err->errorExit(JERR_CONVERSION_NOTIMPL);
			break;
		}

  if(m_Parent->quantizeColors)
    m_Parent->outputComponents = 1; /* single colormapped output component */
  else
    m_Parent->outputComponents = m_Parent->outColorComponents;
	}



/*
 * jchuff.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains Huffman entropy encoding routines.
 *
 * Much of the complexity here has to do with supporting output suspension.
 * If the data destination module demands suspension, we want to be able to
 * back up to the start of the current MCU.  To do this, we copy state
 * variables into local working storage, and update them back to the
 * permanent JPEG objects only upon successful completion of an MCU.
 */




/* This macro is to work around compilers with missing or broken
 * structure assignment.  You'll need to fix this code if you have
 * such a compiler and you change MAX_COMPS_IN_SCAN.
 */

#ifndef NO_STRUCT_ASSIGN
#define ASSIGN_STATE(dest,src)  ((dest) = (src))
#else
#if MAX_COMPS_IN_SCAN == 4
#define ASSIGN_STATE(dest,src)  \
	((dest).put_buffer = (src).put_buffer, \
	 (dest).put_bits = (src).put_bits, \
	 (dest).last_dc_val[0] = (src).last_dc_val[0], \
	 (dest).last_dc_val[1] = (src).last_dc_val[1], \
	 (dest).last_dc_val[2] = (src).last_dc_val[2], \
	 (dest).last_dc_val[3] = (src).last_dc_val[3])
#endif
#endif


/* Working state while writing an MCU.
 * This struct contains all the fields that are needed by subroutines.
 */


/*
 * Initialize for a Huffman-compressed scan.
 * If gather_statistics is TRUE, we do not output anything during the scan,
 * just count the Huffman symbols used and generate Huffman code tables.
 */

void CHuffEntropyEncoder::startPass(BOOL gather_statistics) {
  int ci, dctbl, actbl;
  J_COMPONENT_INFO *compptr;

  if(gather_statistics) {
#ifdef ENTROPY_OPT_SUPPORTED
    doEncodeMCU=encodeMCU_Gather;
    doFinishPass=finishPassGather;
#else
    m_Parent->m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif
		} 
	else {
    doEncodeMCU = encodeMCU;
    doFinishPass = finishPass;
		}

  for(ci=0; ci<m_Parent->compsInScan; ci++) {
    compptr = m_Parent->curCompInfo[ci];
    dctbl=compptr->dc_tbl_no;
    actbl=compptr->ac_tbl_no;
    if(gather_statistics) {
#ifdef ENTROPY_OPT_SUPPORTED
      // Check for invalid table indexes
      // (make_c_derived_tbl does this in the other path)
      if(dctbl<0 || dctbl >= NUM_HUFF_TBLS)
				m_Parent->m_Parent->err->errorExit(JERR_NO_HUFF_TABLE, dctbl);
      if(actbl<0 || actbl >= NUM_HUFF_TBLS)
				m_Parent->m_Parent->err->errorExit(JERR_NO_HUFF_TABLE, actbl);
      // Allocate and zero the statistics tables
      // Note that jpeg_gen_optimal_table expects 257 entries in each table!
      if(DC_CountPtrs[dctbl] == NULL)
				DC_CountPtrs[dctbl] = (long *)
					m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE, 257 * sizeof(long));
      ZeroMemory(DC_CountPtrs[dctbl], 257*sizeof(long));
      if(AC_CountPtrs[actbl] == NULL)
				AC_CountPtrs[actbl] = (long *)
					m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,257 * sizeof(long));
				ZeroMemory(AC_CountPtrs[actbl], 257*sizeof(long));
#endif
			}
		else {
      // Compute derived values for Huffman tables
      // We may do this more than once for a table, but it's not expensive
      make_C_DerivedTbl(TRUE, dctbl, &DC_DerivedTbls[dctbl]);
      make_C_DerivedTbl(FALSE, actbl, &AC_DerivedTbls[actbl]);
		  }
    // Initialize DC predictions to 0
    saved.lastDCVal[ci]=0;
		}

  // Initialize bit buffer to empty
  saved.putBuffer=0;
  saved.putBits=0;

  // Initialize restart stuff
  restartsToGo=m_Parent->restartInterval;
  nextRestartNum=0;
	}


/*
 * Compute the derived values for a Huffman table.
 * This routine also performs some validation checks on the table.
 *
 * Note this is also used by jcphuff.c.
 */

void CEntropyEncoder::make_C_DerivedTbl(BOOL isDC, int tblno, C_DERIVED_TBL **pdtbl) {
  JHUFF_TBL *htbl;
  C_DERIVED_TBL *dtbl;
  int p,i,l,lastp,si,maxsymbol;
  char huffsize[257];
  DWORD huffcode[257];
  DWORD code;

  /* Note that huffsize[] and huffcode[] are filled in code-length order,
   * paralleling the order of the symbols themselves in htbl->huffval[].
   */

  // Find the input Huffman table
  if(tblno < 0 || tblno >= NUM_HUFF_TBLS)
    m_Parent->m_Parent->err->errorExit(JERR_NO_HUFF_TABLE, tblno);
  htbl=isDC ? m_Parent->DC_HuffTblPtrs[tblno] : m_Parent->AC_HuffTblPtrs[tblno];
  if(!htbl)
    m_Parent->m_Parent->err->errorExit(JERR_NO_HUFF_TABLE, tblno);

  // Allocate a workspace if we haven't already done so.
  if(!*pdtbl)
    *pdtbl=(C_DERIVED_TBL *)m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,sizeof(C_DERIVED_TBL));
  dtbl=*pdtbl;
  
  // Figure C.1: make table of Huffman code length for each symbol

  p=0;
  for(l=1; l<=16; l++) {
    i=(int)htbl->bits[l];
    if(i<0 || p + i > 256)	// protect against table overrun
      m_Parent->m_Parent->err->errorExit(JERR_BAD_HUFF_TABLE);
    while(i--)
      huffsize[p++] = (char)l;
		}
  huffsize[p] = 0;
  lastp = p;
  
  /* Figure C.2: generate the codes themselves */
  /* We also validate that the counts represent a legal Huffman code tree. */

  code=0;
  si=huffsize[0];
  p=0;
  while(huffsize[p]) {
    while(((int)huffsize[p]) == si) {
      huffcode[p++] = code;
      code++;
			}
    /* code is now 1 more than the last code used for codelength si; but
     * it must still fit in si bits, since no code is allowed to be all ones.
     */
    if(((INT32)code) >= (((INT32) 1) << si))
      m_Parent->m_Parent->err->errorExit(JERR_BAD_HUFF_TABLE);
    code <<= 1;
    si++;
		}
  
  /* Figure C.3: generate encoding tables */
  /* These are code and size indexed by symbol value */

  /* Set all codeless symbols to have code length 0;
   * this lets us detect duplicate VAL entries here, and later
   * allows emit_bits to detect any attempt to emit such symbols.
   */
  ZeroMemory(dtbl->ehufsi, sizeof(dtbl->ehufsi));

  /* This is also a convenient place to check for out-of-range
   * and duplicated VAL entries.  We allow 0..255 for AC symbols
   * but only 0..15 for DC.  (We could constrain them further
   * based on data depth and mode, but this seems enough.)
   */
  maxsymbol = isDC ? 15 : 255;

  for(p=0; p < lastp; p++) {
    i=htbl->huffval[p];
    if(i<0 || i > maxsymbol || dtbl->ehufsi[i])
      m_Parent->m_Parent->err->errorExit(JERR_BAD_HUFF_TABLE);
    dtbl->ehufco[i] = huffcode[p];
    dtbl->ehufsi[i] = huffsize[p];
		}
	}


BOOL CDestMgr::dumpBuffer(WORKING_STATE *state) {
// Empty the output buffer; return TRUE if successful, FALSE if must suspend 
  CDestMgr *dest = state->cinfo->dest;

  if(!emptyOutputBuffer())
    return FALSE;
  // After a successful buffer dump, must reset buffer pointers 
  state->nextOutputByte = dest->nextOutputByte;
  state->freeInBuffer = dest->freeInBuffer;
  return TRUE;
	}



// Outputting bits to the file

/* Only the right 24 bits of put_buffer are used; the valid bits are
 * left-justified in this part.  At most 16 bits can be passed to emit_bits
 * in one call, and we never retain more than 7 bits in put_buffer
 * between calls, so 24 bits are sufficient.
 */

// Emit a byte, taking 'action' if must suspend.
#define EMIT_BYTE(state,val,action)  \
	{ *(state)->nextOutputByte++ = (JOCTET) (val);  \
	  if(--(state)->freeInBuffer == 0)  \
	    if(!dumpBuffer(state))  \
	      { action; } }
	

BOOL CEntropyEncoder::dumpBuffer(WORKING_STATE *state) {
// Empty the output buffer; return TRUE if successful, FALSE if must suspend 
  CDestMgr *dest = state->cinfo->dest;

  if(!dest->emptyOutputBuffer())
    return FALSE;
  // After a successful buffer dump, must reset buffer pointers 
  state->nextOutputByte=dest->nextOutputByte;
  state->freeInBuffer=dest->freeInBuffer;
  return TRUE;
	}

inline BOOL CEntropyEncoder::emitBits(WORKING_STATE *state, DWORD code, int size) {
// Emit some bits; return TRUE if successful, FALSE if must suspend
// This routine is heavily used, so it's worth coding tightly.
  register INT32 put_buffer = (INT32)code;
  register int put_bits = state->cur.putBits;

  // if size is 0, caller used an invalid Huffman table entry
  if(size==0)
    m_Parent->m_Parent->err->errorExit(JERR_HUFF_MISSING_CODE);

  put_buffer &= (((INT32) 1)<<size) - 1; // mask off any extra bits in code
  
  put_bits += size;		// new number of bits in buffer
  
  put_buffer <<= 24-put_bits; // align incoming bits

  put_buffer |= state->cur.putBuffer; // and merge with old buffer contents
  
  while(put_bits >= 8) {
    int c=(int)((put_buffer >> 16) & 0xFF);
    
    EMIT_BYTE(state, c, return FALSE);
    if(c == 0xFF) {		// need to stuff a zero byte?
      EMIT_BYTE(state, 0, return FALSE);
			}
    put_buffer <<= 8;
    put_bits -= 8;
		}

  state->cur.putBuffer = put_buffer; // update state variables
  state->cur.putBits = put_bits;

  return TRUE;
	}


BOOL CEntropyEncoder::flushBits(WORKING_STATE *state) {

  if(!emitBits(state, 0x7F, 7)) // fill any partial byte with ones
    return FALSE;
  state->cur.putBuffer=0;	// and reset bit-buffer to empty
  state->cur.putBits=0;
  return TRUE;
	}


// Encode a single block's worth of coefficients
BOOL CHuffEntropyEncoder::encodeOneBlock(WORKING_STATE *state, JCOEFPTR block, int last_dc_val,
	C_DERIVED_TBL *dctbl, C_DERIVED_TBL *actbl)	{
  register int temp, temp2;
  register int nbits;
  register int k, r, i;
  
  // Encode the DC coefficient difference per section F.1.2.1
  
  temp=temp2=block[0]-last_dc_val;

  if(temp<0) {
    temp=-temp;		// temp is abs value of input
    /* For a negative input, want temp2 = bitwise complement of abs(input) */
    /* This code assumes we are on a two's complement machine */
    temp2--;
		}
  
  // Find the number of bits needed for the magnitude of the coefficient 
  nbits=0;
  while(temp) {
    nbits++;
    temp >>= 1;
		}
  /* Check for out-of-range coefficient values.
   * Since we're encoding a difference, the range limit is twice as much.
   */
  if(nbits > MAX_COEF_BITS+1)
    m_Parent->m_Parent->err->errorExit(JERR_BAD_DCT_COEF);
  
  // Emit the Huffman-coded symbol for the number of bits
  if(!emitBits(state, dctbl->ehufco[nbits], dctbl->ehufsi[nbits]))
    return FALSE;

  /* Emit that number of bits of the value, if positive, */
  /* or the complement of its magnitude, if negative. */
  if(nbits)			// emit_bits rejects calls with size 0
    if(!emitBits(state, (DWORD)temp2, nbits))
      return FALSE;

  // Encode the AC coefficients per section F.1.2.2 
  
  r=0;			// r = run length of zeros
  
  for(k=1; k < DCTSIZE2; k++) {
    if((temp=block[m_Parent->naturalOrder[k]]) == 0) {
      r++;
			}
		else {
      // if run length > 15, must emit special run-length-16 codes (0xF0)
      while(r > 15) {
				if(!emitBits(state, actbl->ehufco[0xF0], actbl->ehufsi[0xF0]))
					return FALSE;
				r -= 16;
				}

      temp2 = temp;
      if(temp < 0) {
				temp = -temp;		/* temp is abs value of input */
				/* This code assumes we are on a two's complement machine */
				temp2--;
				}
      
      // Find the number of bits needed for the magnitude of the coefficient
      nbits = 1;		// there must be at least one 1 bit
      while((temp >>= 1))
				nbits++;
      // Check for out-of-range coefficient values
      if(nbits > MAX_COEF_BITS)
				m_Parent->m_Parent->err->errorExit(JERR_BAD_DCT_COEF);
      
      // Emit Huffman symbol for run length / number of bits
      i= (r << 4) + nbits;
      if(!emitBits(state, actbl->ehufco[i], actbl->ehufsi[i]))
				return FALSE;

      /* Emit that number of bits of the value, if positive, */
      /* or the complement of its magnitude, if negative. */
      if(!emitBits(state, (DWORD)temp2, nbits))
				return FALSE;
      
      r = 0;
			}
		}

  // If the last coef(s) were zero, emit an end-of-block code
  if(r>0)
    if(!emitBits(state, actbl->ehufco[0], actbl->ehufsi[0]))
      return FALSE;

  return TRUE;
	}


/*
 * Emit a restart marker & resynchronize predictions.
 */

BOOL CEntropyEncoder::emitRestart(WORKING_STATE *state, int restart_num) {
  int ci;

  if(!flushBits(state))
    return FALSE;

  EMIT_BYTE(state, 0xFF, return FALSE);
  EMIT_BYTE(state, CMarkerWriter::RST0 + restart_num, return FALSE);

  // Re-initialize DC predictions to 0
  for(ci=0; ci<state->cinfo->compsInScan; ci++)
    state->cur.lastDCVal[ci]=0;

  // The restart counter is not updated until we successfully write the MCU.

  return TRUE;
	}

CEntropyEncoder::CEntropyEncoder(const CCompressJpeg *p) : m_Parent(p) {
  
	doEncodeMCU=NULL;
	doFinishPass=NULL;
	gatherStatistics=0;
	}


CEntropyDecoder::CEntropyDecoder(CDecompressJpeg *p) : m_Parent(p) {
  
	doDecodeMCU=NULL;
	insufficientData=FALSE;
	}


/*
 * jdhuff.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains Huffman entropy decoding routines.
 *
 * Much of the complexity here has to do with supporting input suspension.
 * If the data source module demands suspension, we want to be able to back
 * up to the start of the current MCU.  To do this, we copy state variables
 * into local working storage, and update them back to the permanent
 * storage only upon successful completion of an MCU.
 */

/* Macros to declare and load/save bitread local variables. */
#define BITREAD_STATE_VARS  \
	register BIT_BUF_TYPE get_buffer;  \
	register int bits_left;  \
	BITREAD_WORKING_STATE br_state

#define BITREAD_LOAD_STATE(cinfop,permstate)  \
	br_state.cinfo = cinfop; \
	br_state.nextInputByte = cinfop->src->nextInputByte; \
	br_state.bytesInBuffer = cinfop->src->bytesInBuffer; \
	br_state.unreadMarker = cinfop->unreadMarker; \
	get_buffer = permstate.getBuffer; \
	bits_left = permstate.bitsLeft; \
	br_state.printedEodPtr = & permstate.printedEod

#define BITREAD_SAVE_STATE(cinfop,permstate)  \
	cinfop->src->nextInputByte = br_state.nextInputByte; \
	cinfop->src->bytesInBuffer = br_state.bytesInBuffer; \
	cinfop->unreadMarker = br_state.unreadMarker; \
	permstate.getBuffer = get_buffer; \
	permstate.bitsLeft = bits_left

/*
 * These macros provide the in-line portion of bit fetching.
 * Use CHECK_BIT_BUFFER to ensure there are N bits in get_buffer
 * before using GET_BITS, PEEK_BITS, or DROP_BITS.
 * The variables get_buffer and bits_left are assumed to be locals,
 * but the state struct might not be (jpeg_huff_decode needs this).
 *	CHECK_BIT_BUFFER(state,n,action);
 *		Ensure there are N bits in get_buffer; if suspend, take action.
 *      val = GET_BITS(n);
 *		Fetch next N bits.
 *      val = PEEK_BITS(n);
 *		Fetch next N bits without removing them from the buffer.
 *	DROP_BITS(n);
 *		Discard next N bits.
 * The value N should be a simple variable, not an expression, because it
 * is evaluated multiple times.
 */

#define CHECK_BIT_BUFFER(state,nbits,action) \
	{ if (bits_left < (nbits)) {  \
	    if (! fillBitBuffer(&(state),get_buffer,bits_left,nbits))  \
	      { action; }  \
	    get_buffer = (state).getBuffer; bits_left = (state).bitsLeft; } }

#define GET_BITS(nbits) \
	(((int) (get_buffer >> (bits_left -= (nbits)))) & ((1<<(nbits))-1))

#define PEEK_BITS(nbits) \
	(((int) (get_buffer >> (bits_left -  (nbits)))) & ((1<<(nbits))-1))

#define DROP_BITS(nbits) \
	(bits_left -= (nbits))



/*
 * Code for extracting next Huffman-coded symbol from input bit stream.
 * Again, this is time-critical and we make the main paths be macros.
 *
 * We use a lookahead table to process codes of up to HUFF_LOOKAHEAD bits
 * without looping.  Usually, more than 95% of the Huffman codes will be 8
 * or fewer bits long.  The few overlength codes are handled with a loop,
 * which need not be inline code.
 *
 * Notes about the HUFF_DECODE macro:
 * 1. Near the end of the data segment, we may fail to get enough bits
 *    for a lookahead.  In that case, we do it the hard way.
 * 2. If the lookahead table contains no entry, the next code must be
 *    more than HUFF_LOOKAHEAD bits long.
 * 3. jpeg_huff_decode returns -1 if forced to suspend.
 */

#define HUFF_DECODE(result,state,htbl,failaction,slowlabel) \
{ register int nb, look; \
  if(bits_left < HUFF_LOOKAHEAD) { \
    if(! fillBitBuffer(&state,get_buffer,bits_left, 0)) {failaction;} \
    get_buffer = state.getBuffer; bits_left = state.bitsLeft; \
    if(bits_left < HUFF_LOOKAHEAD) { \
      nb = 1; goto slowlabel; \
    } \
  } \
  look = PEEK_BITS(HUFF_LOOKAHEAD); \
  if ((nb = htbl->lookNBits[look]) != 0) { \
    DROP_BITS(nb); \
    result = htbl->lookSym[look]; \
  } else { \
    nb = HUFF_LOOKAHEAD+1; \
slowlabel: \
    if ((result=huffDecode(&state,get_buffer,bits_left,htbl,nb)) < 0) \
	{ failaction; } \
    get_buffer = state.getBuffer; bits_left = state.bitsLeft; \
  } \
}



/*
 * Initialize for a Huffman-compressed scan.
 */

void CHuffEntropyDecoder::startPass() {
  int ci, dctbl, actbl;
  J_COMPONENT_INFO *compptr;

  /* Check that the scan parameters Ss, Se, Ah/Al are OK for sequential JPEG.
   * This ought to be an error condition, but we make it a warning because
   * there are some baseline files out there with all zeroes in these bytes.
   */
  if(m_Parent->Ss != 0 || m_Parent->Se != DCTSIZE2-1 ||
    m_Parent->Ah != 0 || m_Parent->Al != 0)
    m_Parent->m_Parent->err->warn(JWRN_NOT_SEQUENTIAL);

  for(ci=0; ci < m_Parent->compsInScan; ci++) {
    compptr = m_Parent->curCompInfo[ci];
    dctbl = compptr->dc_tbl_no;
    actbl = compptr->ac_tbl_no;
    /* Make sure requested tables are present */
    if(dctbl < 0 || dctbl >= NUM_HUFF_TBLS ||
			m_Parent->DC_HuffTblPtrs[dctbl] == NULL)
      m_Parent->m_Parent->err->errorExit(JERR_NO_HUFF_TABLE, dctbl);
    if(actbl < 0 || actbl >= NUM_HUFF_TBLS ||
			m_Parent->AC_HuffTblPtrs[actbl] == NULL)
      m_Parent->m_Parent->err->errorExit(JERR_NO_HUFF_TABLE, actbl);
    /* Compute derived values for Huffman tables */
    /* We may do this more than once for a table, but it's not expensive */
    make_D_DerivedTbl(m_Parent->DC_HuffTblPtrs[dctbl],
			    &DC_DerivedTbls[dctbl]);
    make_D_DerivedTbl(m_Parent->AC_HuffTblPtrs[actbl],
			    &AC_DerivedTbls[actbl]);
    /* Initialize DC predictions to 0 */
    saved.lastDCVal[ci] = 0;
		}

  /* Initialize bitread state variables */
  bitstate.bitsLeft = 0;
  bitstate.getBuffer = 0; /* unnecessary, but keeps Purify quiet */
  bitstate.printedEod = FALSE;

  /* Initialize restart counter */
  restartsToGo = m_Parent->restartInterval;
	}


/*
 * Compute the derived values for a Huffman table.
 * Note this is also used by jdphuff.c.
 */

void CHuffEntropyDecoder::make_D_DerivedTbl(JHUFF_TBL * htbl, D_DERIVED_TBL **pdtbl) {
  D_DERIVED_TBL *dtbl;
  int p, i, l, si;
  int lookbits, ctr;
  char huffsize[257];
  unsigned int huffcode[257];
  unsigned int code;

  /* Allocate a workspace if we haven't already done so. */
  if(*pdtbl == NULL)
    *pdtbl = (D_DERIVED_TBL *)
      m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,
				sizeof(D_DERIVED_TBL));
  dtbl = *pdtbl;
  dtbl->pub = htbl;		/* fill in back link */
  
  /* Figure C.1: make table of Huffman code length for each symbol */
  /* Note that this is in code-length order. */

  p = 0;
  for(l=1; l <= 16; l++) {
    for (i = 1; i <= (int) htbl->bits[l]; i++)
      huffsize[p++] = (char) l;
		}
  huffsize[p] = 0;
  
  /* Figure C.2: generate the codes themselves */
  /* Note that this is in code-length order. */
  
  code = 0;
  si = huffsize[0];
  p = 0;
  while(huffsize[p]) {
    while(((int) huffsize[p]) == si) {
      huffcode[p++] = code;
      code++;
	    }
		code <<= 1;
		si++;
		}

  /* Figure F.15: generate decoding tables for bit-sequential decoding */

  p = 0;
  for(l=1; l <= 16; l++) {
    if(htbl->bits[l]) {
      dtbl->valptr[l] = p; /* huffval[] index of 1st symbol of code length l */
      dtbl->mincode[l] = huffcode[p]; /* minimum code of length l */
      p += htbl->bits[l];
      dtbl->maxcode[l] = huffcode[p-1]; /* maximum code of length l */
		  } 
		else {
      dtbl->maxcode[l] = -1;	/* -1 if no codes of this length */
			}
		}
  dtbl->maxcode[17] = 0xFFFFFL; /* ensures jpeg_huff_decode terminates */

  /* Compute lookahead tables to speed up decoding.
   * First we set all the table entries to 0, indicating "too long";
   * then we iterate through the Huffman codes that are short enough and
   * fill in all the entries that correspond to bit sequences starting
   * with that code.
   */

  ZeroMemory(dtbl->lookNBits, sizeof(dtbl->lookNBits));

  p = 0;
  for (l = 1; l <= HUFF_LOOKAHEAD; l++) {
    for (i = 1; i <= (int) htbl->bits[l]; i++, p++) {
      /* l = current code's length, p = its index in huffcode[] & huffval[]. */
      /* Generate left-justified code followed by all possible bit sequences */
      lookbits = huffcode[p] << (HUFF_LOOKAHEAD-l);
      for(ctr = 1 << (HUFF_LOOKAHEAD-l); ctr > 0; ctr--) {
				dtbl->lookNBits[lookbits] = l;
				dtbl->lookSym[lookbits] = htbl->huffval[p];
				lookbits++;
				}
			}
		}
	}


/*
 * Out-of-line code for bit fetching (shared with jdphuff.c).
 * See jdhuff.h for info about usage.
 * Note: current values of get_buffer and bits_left are passed as parameters,
 * but are returned in the corresponding fields of the state struct.
 *
 * On most machines MIN_GET_BITS should be 25 to allow the full 32-bit width
 * of get_buffer to be used.  (On machines with wider words, an even larger
 * buffer could be used.)  However, on some machines 32-bit shifts are
 * quite slow and take time proportional to the number of places shifted.
 * (This is true with most PC compilers, for instance.)  In this case it may
 * be a win to set MIN_GET_BITS to the minimum value of 15.  This reduces the
 * average shift distance at the cost of more calls to jpeg_fill_bit_buffer.
 */

#ifdef SLOW_SHIFT_32
#define MIN_GET_BITS  15	/* minimum allowable value */
#else
#define MIN_GET_BITS  (BIT_BUF_SIZE-7)
#endif


BOOL CHuffEntropyDecoder::fillBitBuffer(BITREAD_WORKING_STATE *state,
		      register BIT_BUF_TYPE get_buffer, register int bits_left,
		      int nbits)
/* Load up the bit buffer to a depth of at least nbits */
{
  /* Copy heavily used state fields into locals (hopefully registers) */
  register const JOCTET *next_input_byte = state->nextInputByte;
  register size_t bytes_in_buffer = state->bytesInBuffer;
  register int c;

  /* Attempt to load at least MIN_GET_BITS bits into get_buffer. */
  /* (It is assumed that no request will be for more than that many bits.) */

  while(bits_left < MIN_GET_BITS) {
    /* Attempt to read a byte */
    if(state->unreadMarker != 0)
      goto no_more_data;	/* can't advance past a marker */

    if(bytes_in_buffer == 0) {
      if (! (state->cinfo->src->*state->cinfo->src->fillInputBuffer) ())
				return FALSE;
      next_input_byte = state->cinfo->src->nextInputByte;
      bytes_in_buffer = state->cinfo->src->bytesInBuffer;
			}
    bytes_in_buffer--;
    c = GETJOCTET(*next_input_byte++);

    /* If it's 0xFF, check and discard stuffed zero byte */
    if(c == 0xFF) {
      do {
				if(bytes_in_buffer == 0) {
					if (! (state->cinfo->src->*state->cinfo->src->fillInputBuffer) ())
						return FALSE;
					next_input_byte = state->cinfo->src->nextInputByte;
					bytes_in_buffer = state->cinfo->src->bytesInBuffer;
				}
			bytes_in_buffer--;
			c = GETJOCTET(*next_input_byte++);
      } while (c == 0xFF);

    if (c == 0) {
	/* Found FF/00, which represents an FF data byte */
			c = 0xFF;
      } else {
	/* Oops, it's actually a marker indicating end of compressed data. */
	/* Better put it back for use later */
		state->unreadMarker = c;

      no_more_data:
	/* There should be enough bits still left in the data segment; */
	/* if so, just break out of the outer while loop. */
		if(bits_left >= nbits)
			break;
	/* Uh-oh.  Report corrupted data to user and stuff zeroes into
	 * the data stream, so that we can produce some kind of image.
	 * Note that this code will be repeated for each byte demanded
	 * for the rest of the segment.  We use a nonvolatile flag to ensure
	 * that only one warning message appears.
	 */
		if(! *(state->printedEodPtr)) {
			m_Parent->m_Parent->err->warn(JWRN_HIT_MARKER);
			*(state->printedEodPtr) = TRUE;
			}
			c = 0;			/* insert a zero byte into bit buffer */
      }
    }

    /* OK, load c into get_buffer */
    get_buffer = (get_buffer << 8) | c;
    bits_left += 8;
		}

  /* Unload the local registers */
  state->nextInputByte = next_input_byte;
  state->bytesInBuffer = bytes_in_buffer;
  state->getBuffer = get_buffer;
  state->bitsLeft = bits_left;

  return TRUE;
	}


/*
 * Out-of-line code for Huffman code decoding.
 * See jdhuff.h for info about usage.
 */

int CHuffEntropyDecoder::huffDecode(BITREAD_WORKING_STATE *state,
		  register BIT_BUF_TYPE get_buffer, register int bits_left,
		  D_DERIVED_TBL *htbl, int min_bits) {
  register int l = min_bits;
  register INT32 code;

  /* HUFF_DECODE has determined that the code is at least min_bits */
  /* bits long, so fetch that many bits in one swoop. */

  CHECK_BIT_BUFFER(*state, l, return -1);
  code = GET_BITS(l);

  /* Collect the rest of the Huffman code one bit at a time. */
  /* This is per Figure F.16 in the JPEG spec. */

  while(code > htbl->maxcode[l]) {
    code <<= 1;
    CHECK_BIT_BUFFER(*state, 1, return -1);
    code |= GET_BITS(1);
    l++;
		}

  /* Unload the local registers */
  state->getBuffer = get_buffer;
  state->bitsLeft = bits_left;

  /* With garbage input we may reach the sentinel value l = 17. */

  if (l > 16) {
    m_Parent->m_Parent->err->warn(JWRN_HUFF_BAD_CODE);
    return 0;			/* fake a zero as the safest result */
		}

  return htbl->pub->huffval[ htbl->valptr[l] +
			    ((int) (code - htbl->mincode[l])) ];
	}


/*
 * Figure F.12: extend sign bit.
 * On some machines, a shift and add will be faster than a table lookup.
 */

#ifdef AVOID_TABLES

#define HUFF_EXTEND(x,s)  ((x) < (1<<((s)-1)) ? (x) + (((-1)<<(s)) + 1) : (x))

#else

#define HUFF_EXTEND(x,s)  ((x) < extend_test[s] ? (x) + extend_offset[s] : (x))

static const int extend_test[16] =   /* entry n is 2**(n-1) */
  { 0, 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000 };

static const int extend_offset[16] = /* entry n is (-1 << n) + 1 */
  { 0, ((-1)<<1) + 1, ((-1)<<2) + 1, ((-1)<<3) + 1, ((-1)<<4) + 1,
    ((-1)<<5) + 1, ((-1)<<6) + 1, ((-1)<<7) + 1, ((-1)<<8) + 1,
    ((-1)<<9) + 1, ((-1)<<10) + 1, ((-1)<<11) + 1, ((-1)<<12) + 1,
    ((-1)<<13) + 1, ((-1)<<14) + 1, ((-1)<<15) + 1 };

#endif /* AVOID_TABLES */


/*
 * Check for a restart marker & resynchronize decoder.
 * Returns FALSE if must suspend.
 */

BOOL CHuffEntropyDecoder::processRestart() {
  int ci;

  /* Throw away any unused bits remaining in bit buffer; */
  /* include any full bytes in next_marker's count of discarded bytes */
  m_Parent->marker->discardedBytes += bitstate.bitsLeft / 8;
  bitstate.bitsLeft = 0;

  /* Advance past the RSTn marker */
  if(! (m_Parent->*m_Parent->readRestartMarker) ())
    return FALSE;

  /* Re-initialize DC predictions to 0 */
  for(ci = 0; ci < m_Parent->compsInScan; ci++)
    saved.lastDCVal[ci] = 0;

  /* Reset restart counter */
  restartsToGo = m_Parent->restartInterval;

  /* Next segment can get another out-of-data warning */
  bitstate.printedEod = FALSE;

  return TRUE;
	}


/*
 * Decode and return one MCU's worth of Huffman-compressed coefficients.
 * The coefficients are reordered from zigzag order into natural array order,
 * but are not dequantized.
 *
 * The i'th block of the MCU is stored into the block pointed to by
 * MCU_data[i].  WE ASSUME THIS AREA HAS BEEN ZEROED BY THE CALLER.
 * (Wholesale zeroing is usually a little faster than retail...)
 *
 * Returns FALSE if data source requested suspension.  In that case no
 * changes have been made to permanent state.  (Exception: some output
 * coefficients may already have been assigned.  This is harmless for
 * this module, since we'll just re-assign them on the next call.)
 */

BOOL CHuffEntropyDecoder::decodeMcu(JBLOCKROW *MCU_data) {
  register int s, k, r;
  int blkn, ci;
  JBLOCKROW block;
  BITREAD_STATE_VARS;
  SAVABLE_STATE state;
  D_DERIVED_TBL *dctbl;
  D_DERIVED_TBL *actbl;
  J_COMPONENT_INFO *compptr;

  /* Process restart marker if needed; may have to suspend */
  if(m_Parent->restartInterval) {
    if(restartsToGo == 0)
      if(! processRestart())
				return FALSE;
			}

  /* Load up working state */
  BITREAD_LOAD_STATE(m_Parent,bitstate);
  ASSIGN_STATE(state, saved);

  /* Outer loop handles each block in the MCU */

  for(blkn=0; blkn < m_Parent->blocksInMCU; blkn++) {
    block = MCU_data[blkn];
    ci = m_Parent->MCU_membership[blkn];
    compptr = m_Parent->curCompInfo[ci];
    dctbl = DC_DerivedTbls[compptr->dc_tbl_no];
    actbl = AC_DerivedTbls[compptr->ac_tbl_no];

    /* Decode a single block's worth of coefficients */

    /* Section F.2.2.1: decode the DC coefficient difference */
    HUFF_DECODE(s, br_state, dctbl, return FALSE, label1);
    if(s) {
      CHECK_BIT_BUFFER(br_state, s, return FALSE);
      r = GET_BITS(s);
      s = HUFF_EXTEND(r, s);
	    }

    /* Shortcut if component's values are not interesting */
    if(!compptr->componentNeeded)
      goto skip_ACs;

    /* Convert DC difference to actual value, update last_dc_val */
    s += state.lastDCVal[ci];
    state.lastDCVal[ci] = s;
    /* Output the DC coefficient (assumes jpeg_natural_order[0] = 0) */
    (*block)[0] = (JCOEF) s;

    /* Do we need to decode the AC coefficients for this component? */
    if(compptr->DCT_scaledSize > 1) {

      /* Section F.2.2.2: decode the AC coefficients */
      /* Since zeroes are skipped, output area must be cleared beforehand */
      for (k = 1; k < DCTSIZE2; k++) {
				HUFF_DECODE(s, br_state, actbl, return FALSE, label2);
      
				r = s >> 4;
				s &= 15;
      
				if (s) {
					k += r;
					CHECK_BIT_BUFFER(br_state, s, return FALSE);
					r = GET_BITS(s);
					s = HUFF_EXTEND(r, s);
					/* Output coefficient in natural (dezigzagged) order.
					 * Note: the extra entries in jpeg_natural_order[] will save us
					 * if k >= DCTSIZE2, which could happen if the data is corrupted.
					 */
					(*block)[m_Parent->naturalOrder[k]] = (JCOEF) s;
					}
				else {
					if(r != 15)
						break;
					k += 15;
					}
				}

	    }
		else {
skip_ACs:

      /* Section F.2.2.2: decode the AC coefficients */
      /* In this path we just discard the values */
      for (k = 1; k < DCTSIZE2; k++) {
				HUFF_DECODE(s, br_state, actbl, return FALSE, label3);
      
		r = s >> 4;
		s &= 15;
      
			if (s) {
				k += r;
				CHECK_BIT_BUFFER(br_state, s, return FALSE);
				DROP_BITS(s);
				} 
			else {
				if (r != 15)
					break;
					k += 15;
					}
				}

			}
		}

  /* Completed MCU, so update state */
  BITREAD_SAVE_STATE(m_Parent,bitstate);
  ASSIGN_STATE(saved, state);

  /* Account for restart interval (no-op if not using restarts) */
  restartsToGo--;

  return TRUE;
	}


/*
 * Module initialization routine for Huffman entropy decoding.
 */
CHuffEntropyDecoder::CHuffEntropyDecoder(CDecompressJpeg *p) : CEntropyDecoder(p) {
  int i;

  doDecodeMCU = decodeMcu;

  saved.putBuffer=0;
  saved.putBits=0;

  /* Mark tables unallocated */
  for(i=0; i < NUM_HUFF_TBLS; i++) {
    DC_DerivedTbls[i] = AC_DerivedTbls[i] = NULL;
		}
	}





/*
 * Encode and output one MCU's worth of Huffman-compressed coefficients.
 */

BOOL CHuffEntropyEncoder::encodeMCU(JBLOCKROW *MCU_data) {
  WORKING_STATE state;
  int blkn, ci;
  J_COMPONENT_INFO *compptr;

  // Load up working state
  state.nextOutputByte=m_Parent->dest->nextOutputByte;
  state.freeInBuffer=m_Parent->dest->freeInBuffer;
  ASSIGN_STATE(state.cur,saved);
  state.cinfo = m_Parent;

  // Emit restart marker if needed 
  if(m_Parent->restartInterval) {
    if(!restartsToGo)
      if(!emitRestart(&state, nextRestartNum))
		return FALSE;
		}

  // Encode the MCU data blocks 
  for(blkn=0; blkn < m_Parent->blocksInMCU; blkn++) {
    ci=m_Parent->MCU_membership[blkn];
    compptr=m_Parent->curCompInfo[ci];
    if(!encodeOneBlock(&state, MCU_data[blkn][0], state.cur.lastDCVal[ci],
			DC_DerivedTbls[compptr->dc_tbl_no],	AC_DerivedTbls[compptr->ac_tbl_no]))
      return FALSE;
    // Update last_dc_val 
    state.cur.lastDCVal[ci]=MCU_data[blkn][0][0];
		}

  // Completed MCU, so update state
  m_Parent->dest->nextOutputByte=state.nextOutputByte;
  m_Parent->dest->freeInBuffer=state.freeInBuffer;
  ASSIGN_STATE(saved,state.cur);

  // Update restart-interval state too
  if(m_Parent->restartInterval) {
    if(!restartsToGo) {
      restartsToGo = m_Parent->restartInterval;
      nextRestartNum++;
      nextRestartNum &= 7;
			}
    restartsToGo--;
		}

  return TRUE;
	}


/*
 * Finish up at the end of a Huffman-compressed scan.
 */

void CHuffEntropyEncoder::finishPass() {
  WORKING_STATE state;

  // Load up working state ... flush_bits needs it
  state.nextOutputByte = m_Parent->dest->nextOutputByte;
  state.freeInBuffer = m_Parent->dest->freeInBuffer;
  ASSIGN_STATE(state.cur, saved);
  state.cinfo = m_Parent;

  // Flush out the last data
  if(!flushBits(&state))
    m_Parent->m_Parent->err->errorExit(JERR_CANT_SUSPEND);

  // Update state
  m_Parent->dest->nextOutputByte = state.nextOutputByte;
  m_Parent->dest->freeInBuffer = state.freeInBuffer;
  ASSIGN_STATE(saved, state.cur);
	}


/*
 * Huffman coding optimization.
 *
 * We first scan the supplied data and count the number of uses of each symbol
 * that is to be Huffman-coded. (This process MUST agree with the code above.)
 * Then we build a Huffman coding tree for the observed counts.
 * Symbols which are not needed at all for the particular image are not
 * assigned any code, which saves space in the DHT marker as well as in
 * the compressed data.
 */

#ifdef ENTROPY_OPT_SUPPORTED


// Process a single block's worth of coefficients

void CCompressJpeg::htestOneBlock(JCOEFPTR block, int last_dc_val,
	long dc_counts[], long ac_counts[]) {
  register int temp;
  register int nbits;
  register int k, r;
  
  // Encode the DC coefficient difference per section F.1.2.1 
  
  temp = block[0] - last_dc_val;
  if(temp < 0)
    temp = -temp;
  
  // Find the number of bits needed for the magnitude of the coefficient 
  nbits = 0;
  while(temp) {
    nbits++;
    temp >>= 1;
		}
  /* Check for out-of-range coefficient values.
   * Since we're encoding a difference, the range limit is twice as much.
   */
  if(nbits > MAX_COEF_BITS+1)
    m_Parent->err->errorExit(JERR_BAD_DCT_COEF);

  // Count the Huffman symbol for the number of bits
  dc_counts[nbits]++;
  
  // Encode the AC coefficients per section F.1.2.2 
  
  r = 0;			// r = run length of zeros 
  
  for(k=1; k < DCTSIZE2; k++) {
    if((temp = block[naturalOrder[k]]) == 0) {
      r++;
		  }
		else {
      // if run length > 15, must emit special run-length-16 codes (0xF0)
      while(r>15) {
				ac_counts[0xF0]++;
				r -= 16;
	      }
      
      // Find the number of bits needed for the magnitude of the coefficient
      if(temp < 0)
				temp=-temp;
      
      // Find the number of bits needed for the magnitude of the coefficient
      nbits=1;		// there must be at least one 1 bit
      while((temp >>= 1))
				nbits++;
      // Check for out-of-range coefficient values
      if(nbits > MAX_COEF_BITS)
				m_Parent->err->errorExit(JERR_BAD_DCT_COEF);
      
      // Count Huffman symbol for run length / number of bits
      ac_counts[(r << 4) + nbits]++;
      
      r = 0;
			}
		}

  // If the last coef(s) were zero, emit an end-of-block code
  if(r>0)
    ac_counts[0]++;
	}


/*
 * Trial-encode one MCU's worth of Huffman-compressed coefficients.
 * No data is actually output, so no suspension return is possible.
 */

BOOL CHuffEntropyEncoder::encodeMCU_Gather(JBLOCKROW *MCU_data) {
  int blkn, ci;
  J_COMPONENT_INFO *compptr;

  // Take care of restart intervals if needed 
  if(m_Parent->restartInterval) {
    if(!restartsToGo) {
      // Re-initialize DC predictions to 0 
      for(ci=0; ci<m_Parent->compsInScan; ci++)
				saved.lastDCVal[ci]=0;
      // Update restart state 
      restartsToGo = m_Parent->restartInterval;
			}
    restartsToGo--;
		}

  for(blkn=0; blkn < m_Parent->blocksInMCU; blkn++) {
    ci=m_Parent->MCU_membership[blkn];
    compptr=m_Parent->curCompInfo[ci];
    ((CCompressJpeg *)m_Parent)->htestOneBlock(MCU_data[blkn][0], saved.lastDCVal[ci],
		  DC_CountPtrs[compptr->dc_tbl_no], AC_CountPtrs[compptr->ac_tbl_no]);
    saved.lastDCVal[ci]=MCU_data[blkn][0][0];
		}

  return TRUE;
	}


/*
 * Generate the best Huffman code table for the given counts, fill htbl.
 * Note this is also used by jcphuff.c.
 *
 * The JPEG standard requires that no symbol be assigned a codeword of all
 * one bits (so that padding bits added at the end of a compressed segment
 * can't look like a valid code).  Because of the canonical ordering of
 * codewords, this just means that there must be an unused slot in the
 * longest codeword length category.  Section K.2 of the JPEG spec suggests
 * reserving such a slot by pretending that symbol 256 is a valid symbol
 * with count 1.  In theory that's not optimal; giving it count zero but
 * including it in the symbol set anyway should give a better Huffman code.
 * But the theoretically better code actually seems to come out worse in
 * practice, because it produces more all-ones bytes (which incur stuffed
 * zero bytes in the final file).  In any case the difference is tiny.
 *
 * The JPEG standard requires Huffman codes to be no more than 16 bits long.
 * If some symbols have a very small but nonzero probability, the Huffman tree
 * must be adjusted to meet the code length restriction.  We currently use
 * the adjustment method suggested in JPEG section K.2.  This method is *not*
 * optimal; it may not choose the best possible limited-length code.  But
 * typically only very-low-frequency symbols will be given less-than-optimal
 * lengths, so the code is almost optimal.  Experimental comparisons against
 * an optimal limited-length-code algorithm indicate that the difference is
 * microscopic --- usually less than a hundredth of a percent of total size.
 * So the extra complexity of an optimal algorithm doesn't seem worthwhile.
 */

void CEntropyEncoder::genOptimalTable(JHUFF_TBL * htbl, long freq[]) {
#define MAX_CLEN 32		/* assumed maximum initial code length */
  UINT8 bits[MAX_CLEN+1];	/* bits[k] = # of symbols with code length k */
  int codesize[257];		/* codesize[k] = code length of symbol k */
  int others[257];		/* next symbol in current branch of tree */
  int c1,c2;
  int p,i,j;
  long v;

  // This algorithm is explained in section K.2 of the JPEG standard 

  ZeroMemory(bits, sizeof(bits));
  ZeroMemory(codesize, sizeof(codesize));
  for(i=0; i < 257; i++)
    others[i]=-1;		// init links to empty 
  
  freq[256] = 1;		// make sure 256 has a nonzero count 
  /* Including the pseudo-symbol 256 in the Huffman procedure guarantees
   * that no real symbol is given code-value of all ones, because 256
   * will be placed last in the largest codeword category.
   */

  // Huffman's basic algorithm to assign optimal code lengths to symbols 

  for(;;) {
    /* Find the smallest nonzero frequency, set c1 = its symbol */
    /* In case of ties, take the larger symbol number */
    c1=-1;
    v=1000000000L;
    for(i=0; i <= 256; i++) {
      if(freq[i] && freq[i] <= v) {
				v = freq[i];
				c1 = i;
				}
			}

    /* Find the next smallest nonzero frequency, set c2 = its symbol */
    /* In case of ties, take the larger symbol number */
    c2 = -1;
    v = 1000000000L;
    for(i=0; i <= 256; i++) {
      if(freq[i] && freq[i] <= v && i != c1) {
				v = freq[i];
				c2 = i;
				}
			}

    // Done if we've merged everything into one frequency 
    if(c2 < 0)
      break;
    
    // Else merge the two counts/trees 
    freq[c1] += freq[c2];
    freq[c2] = 0;

    // Increment the codesize of everything in c1's tree branch
    codesize[c1]++;
    while(others[c1] >= 0) {
      c1=others[c1];
      codesize[c1]++;
	    }
    
    others[c1] = c2;		// chain c2 onto c1's tree branch
    
    // Increment the codesize of everything in c2's tree branch
    codesize[c2]++;
    while(others[c2] >= 0) {
      c2 = others[c2];
      codesize[c2]++;
			}
		}

  // Now count the number of symbols of each code length
  for(i=0; i <= 256; i++) {
    if(codesize[i]) {
      /* The JPEG standard seems to think that this can't happen, */
      /* but I'm paranoid... */
      if(codesize[i] > MAX_CLEN)
				m_Parent->m_Parent->err->errorExit(JERR_HUFF_CLEN_OVERFLOW);

      bits[codesize[i]]++;
			}
		}

  /* JPEG doesn't allow symbols with code lengths over 16 bits, so if the pure
   * Huffman procedure assigned any such lengths, we must adjust the coding.
   * Here is what the JPEG spec says about how this next bit works:
   * Since symbols are paired for the longest Huffman code, the symbols are
   * removed from this length category two at a time.  The prefix for the pair
   * (which is one bit shorter) is allocated to one of the pair; then,
   * skipping the BITS entry for that prefix length, a code word from the next
   * shortest nonzero BITS entry is converted into a prefix for two code words
   * one bit longer.
   */
  
  for(i=MAX_CLEN; i > 16; i--) {
    while(bits[i] > 0) {
      j=i-2;		// find length of new prefix to be used 
      while(bits[j] == 0)
				j--;
      
      bits[i] -= 2;		// remove two symbols
      bits[i-1]++;		// one goes in this length 
      bits[j+1] += 2;		// two new symbols in this length 
      bits[j]--;		// symbol of this length is now a prefix 
			}
		}

  // Remove the count for the pseudo-symbol 256 from the largest codelength 
  while(bits[i] == 0)		// find largest codelength still in use
    i--;
  bits[i]--;
  
  // Return final symbol counts (only for lengths 0..16)
  memcpy(htbl->bits, bits, sizeof(htbl->bits));
  
  /* Return a list of the symbols sorted by code length */
  /* It's not real clear to me why we don't need to consider the codelength
   * changes made above, but the JPEG spec seems to think this works.
   */
  p = 0;
  for(i=1; i <= MAX_CLEN; i++) {
    for(j=0; j <= 255; j++) {
      if(codesize[j] == i) {
				htbl->huffval[p] = (UINT8) j;
				p++;
				}
			}
		}

  // Set sent_table FALSE so updated table will be written to JPEG file.
  htbl->sentTable = FALSE;
	}


/*
 * Finish up a statistics-gathering pass and create the new Huffman tables.
 */

void CHuffEntropyEncoder::finishPassGather() {
  int ci, dctbl, actbl;
  J_COMPONENT_INFO *compptr;
  JHUFF_TBL **htblptr;
  BOOL did_dc[NUM_HUFF_TBLS];
  BOOL did_ac[NUM_HUFF_TBLS];

  /* It's important not to apply jpeg_gen_optimal_table more than once
   * per table, because it clobbers the input frequency counts!
   */
  ZeroMemory(did_dc,sizeof(did_dc));
  ZeroMemory(did_ac,sizeof(did_ac));
ASSERT(0);
  for(ci=0; ci < m_Parent->compsInScan; ci++) {
    compptr = m_Parent->curCompInfo[ci];
    dctbl = compptr->dc_tbl_no;
    actbl = compptr->ac_tbl_no;
    if(!did_dc[dctbl]) {
      htblptr = &((CCompressJpeg *)m_Parent)->DC_HuffTblPtrs[dctbl];
      if(!*htblptr)
				*htblptr = ((CCompressJpeg *)m_Parent)->allocHuffTable();
      genOptimalTable(*htblptr, DC_CountPtrs[dctbl]);
      did_dc[dctbl] = TRUE;
			}
    if(!did_ac[actbl]) {
      htblptr = &((CCompressJpeg *)m_Parent)->AC_HuffTblPtrs[actbl];
      if(!*htblptr)
				*htblptr = ((CCompressJpeg *)m_Parent)->allocHuffTable();
      genOptimalTable(*htblptr, AC_CountPtrs[actbl]);
      did_ac[actbl] = TRUE;
			}
		}
	}


#endif // ENTROPY_OPT_SUPPORTED


/*
 * Module initialization routine for Huffman entropy encoding.
 */
CHuffEntropyEncoder::CHuffEntropyEncoder(const CCompressJpeg *p) : CEntropyEncoder(p) {
  int i;
  
  restartsToGo=p->restartInterval;	// c'e' poi anche in startPass, ma lo metto pure qua!
  nextRestartNum=0;
  // Initialize bit buffer to empty
  saved.putBuffer=0;
  saved.putBits=0;

	// Mark tables unallocated 
  for(i=0; i < NUM_HUFF_TBLS; i++) {
    DC_DerivedTbls[i] = AC_DerivedTbls[i] = NULL;
#ifdef ENTROPY_OPT_SUPPORTED
    DC_CountPtrs[i] = AC_CountPtrs[i] = NULL;
#endif
		}
	}





/*
 * jcinit.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains initialization logic for the JPEG compressor.
 * This routine is in charge of selecting the modules to be executed and
 * making an initialization call to each one.
 *
 * Logically, this code belongs in jcmaster.c.  It's split out because
 * linking this routine implies linking the entire compression library.
 * For a transcoding-only application, we want to be able to use jcmaster.c
 * without linking in the whole library.
 */


/*
 * Master selection of compression modules.
 * This is done once at the start of processing an image.  We determine
 * which modules will be used and give them appropriate initialization calls.
 */

void CCompressJpeg::initCompressMaster() {
  
	// Initialize master control (includes parameter checking/processing)
  master=new CCompMaster(this,FALSE /* full compression */);

  // Preprocessing 
  if(!rawDataIn) {
	  cconvert=new CColorConverter(this);
	  downsample=new CDownsampler(this);
	  prep=new CCompPrepController(this,FALSE /* never need full buffer here */);
		}
  // Forward DCT 
  fdct=new CForwardDCT(this);
  // Entropy encoding: either Huffman or arithmetic coding. 
  if(arithCode) {
    m_Parent->err->errorExit(JERR_ARITH_NOTIMPL);
		} 
	else {
    if(progressiveMode) {
#ifdef C_PROGRESSIVE_SUPPORTED
		  entropy= new PCHuffEntropyEncoder(this);
#else
      m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif
			}
		else
		  entropy=new CHuffEntropyEncoder(this);
		}

  // Need a full-image coefficient buffer in any multi-pass mode.
  coef=new CCompCoefController(this,numScans > 1 || optimizeCoding);
  main=new CMainControllerExt(this,rawDataIn && FALSE /* never need full buffer here */);
	marker=new CMarkerWriter(this);

  // We can now tell the memory manager to allocate virtual arrays.
  realizeVirtArrays();

  /* Write the datastream header (SOI) immediately.
   * Frame and scan headers are postponed till later.
   * This lets application insert special markers after the SOI.
   */
  writeFileHeader();
	}





/*
 * jcmainct.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the main buffer controller for compression.
 * The main buffer lies between the pre-processor and the JPEG
 * compressor proper; it holds downsampled data in the JPEG colorspace.
 */



/*
 * Initialize for a processing pass.
 */

void CMainControllerExt::startPass(J_BUF_MODE pass_mode) {

  // Do nothing in raw-data mode. 
  if(m_Parent->rawDataIn)
    return;

  cur_iMCU_row = 0;	// initialize counters 
  rowgroupCtr = 0;
  suspended = FALSE;
  passMode = pass_mode;	// save mode for use by process_data 

  switch(pass_mode) {
	  case JBUF_PASS_THRU:
#ifdef FULL_MAIN_BUFFER_SUPPORTED
		  if(wholeImage[0])
			  m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
#endif
			doProcessData = processDataSimple;
			break;
#ifdef FULL_MAIN_BUFFER_SUPPORTED
		case JBUF_SAVE_SOURCE:
		case JBUF_CRANK_DEST:
		case JBUF_SAVE_AND_PASS:
			if(!wholeImage[0])
				m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			doProcessData = processDataBuffer;
			break;
#endif
		default:
			m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			break;
		}
	}


/*
 * Process some data.
 * This routine handles the simple pass-through mode,
 * where we have only a strip buffer.
 */

void CMainControllerExt::processDataSimple(JSAMPARRAY input_buf, JDIMENSION *in_row_ctr,
	JDIMENSION in_rows_avail) {

  while(cur_iMCU_row < m_Parent->total_iMCU_rows) {
    // Read input data if we haven't filled the main buffer yet 
    if(rowgroupCtr < DCTSIZE)
      (m_Parent->prep->*(m_Parent->prep)->doPreProcess)(input_buf, in_row_ctr, in_rows_avail,
					buffer, &rowgroupCtr,	(JDIMENSION)DCTSIZE);

    /* If we don't have a full iMCU row buffered, return to application for
     * more data.  Note that preprocessor will always pad to fill the iMCU row
     * at the bottom of the image.
     */
    if(rowgroupCtr != DCTSIZE)
      return;

    // Send the completed row to the compressor 
    if(!m_Parent->coef->compressData(buffer)) {
      /* If compressor did not consume the whole row, then we must need to
       * suspend processing and return to the application.  In this situation
       * we pretend we didn't yet consume the last input row; otherwise, if
       * it happened to be the last row of the image, the application would
       * think we were done.
       */
      if(!suspended) {
				(*in_row_ctr)--;
				suspended = TRUE;
		    }
      return;
	    }
    /* We did finish the row.  Undo our little suspension hack if a previous
     * call suspended; then mark the main buffer empty.
     */
    if(suspended) {
      (*in_row_ctr)++;
      suspended = FALSE;
			}
    rowgroupCtr = 0;
    cur_iMCU_row++;
		}
	}


#ifdef FULL_MAIN_BUFFER_SUPPORTED

/*
 * Process some data.
 * This routine handles all of the modes that use a full-size buffer.
 */

void CMainControllerExt::processDataBuffer(JSAMPARRAY input_buf, JDIMENSION *in_row_ctr,
	JDIMENSION in_rows_avail) {
  int ci;
  J_COMPONENT_INFO *compptr;
  BOOL writing = (passMode != JBUF_CRANK_DEST);

  while(cur_iMCU_row < m_Parent->total_iMCU_rows) {
    // Realign the virtual buffers if at the start of an iMCU row.
    if(!rowgroupCtr) {
      for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
				buffer[ci]=wholeImage[ci]->accessVirtSarray(cur_iMCU_row * (compptr->vSampFactor * DCTSIZE),
					(JDIMENSION)(compptr->vSampFactor * DCTSIZE), writing);
					}
      // In a read pass, pretend we just read some source data. 
				if(!writing) {
					*in_row_ctr += m_Parent->max_V_SampFactor * DCTSIZE;
					rowgroupCtr = DCTSIZE;
					}
				}

    /* If a write pass, read input data until the current iMCU row is full. */
    /* Note: preprocessor will pad if necessary to fill the last iMCU row. */
    if(writing) {
      m_Parent->prep->preProcessData(input_buf, in_row_ctr, in_rows_avail,
				buffer, &rowgroupCtr, (JDIMENSION) DCTSIZE);
      /* Return to application if we need more data to fill the iMCU row. */
      if(rowgroupCtr < DCTSIZE)
				return;
			}

    // Emit data, unless this is a sink-only pass. 
    if(passMode != JBUF_SAVE_SOURCE) {
      if(!m_Parent->coef->compressData(buffer)) {
	/* If compressor did not consume the whole row, then we must need to
	 * suspend processing and return to the application.  In this situation
	 * we pretend we didn't yet consume the last input row; otherwise, if
	 * it happened to be the last row of the image, the application would
	 * think we were done.
	 */
	if(!suspended) {
	  (*in_row_ctr)--;
	  suspended = TRUE;
		}
	return;
    }
      /* We did finish the row.  Undo our little suspension hack if a previous
       * call suspended; then mark the main buffer empty.
       */
      if(suspended) {
				(*in_row_ctr)++;
				suspended = FALSE;
      }
    }

    /* If get here, we are done with this iMCU row.  Mark buffer empty. */
    rowgroupCtr = 0;
    cur_iMCU_row++;
		}
	}

#endif // FULL_MAIN_BUFFER_SUPPORTED


/*
 * Initialize main buffer controller.
 */
CMainControllerExt::CMainControllerExt(CCompressJpeg *p,BOOL need_full_buffer) : J_C_MAIN_CONTROLLER(p) {
  int ci;
  J_COMPONENT_INFO *compptr;

  virtSarrayList=NULL;

  /* Create the buffer.  It holds downsampled data, so each component
   * may be of a different size.
   */
  if(need_full_buffer) {
#ifdef FULL_MAIN_BUFFER_SUPPORTED
    /* Allocate a full-image virtual array for each component */
    /* Note we pad the bottom to a multiple of the iMCU height */
    for(ci=0, compptr = p->compInfo; ci < p->numComponents; ci++, compptr++) {
      wholeImage[ci] = requestVirtSarray(JPOOL_IMAGE, FALSE,
				compptr->widthInBlocks * DCTSIZE,
				(JDIMENSION)CJpeg::roundUp((long) compptr->heightInBlocks,
				(long)compptr->vSampFactor) * DCTSIZE,
				(JDIMENSION)(compptr->vSampFactor * DCTSIZE));
			}
#else
    p->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
#endif
		} 
	else {
#ifdef FULL_MAIN_BUFFER_SUPPORTED
    wholeImage[0] = NULL; // flag for no virtual arrays
#endif
    // Allocate a strip buffer for each component
    for(ci=0, compptr=p->compInfo; ci < p->numComponents; ci++, compptr++) {
      buffer[ci]=m_Parent->m_Parent->mem->allocSarray(JPOOL_IMAGE,
				compptr->widthInBlocks * DCTSIZE,
				(JDIMENSION)(compptr->vSampFactor * DCTSIZE));
			}
		}
	}


CMainControllerExt::~CMainControllerExt() {
	CVirtSArray *sptr,*sptr2;

  for(sptr = virtSarrayList; sptr != NULL; sptr = sptr->next) {
		sptr2=sptr->next;
    if(sptr->b_s_info) {	// there may be no backing store
			delete sptr->b_s_info;
			}
		delete sptr;
		sptr=sptr2;
		}
  virtSarrayList = NULL;
	}



/*
 * jcmarker.c
 *
 * Copyright (C) 1991-1998, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains routines to write JPEG datastream markers.
 */



/*
 * Basic output routines.
 *
 * Note that we do not support suspension while writing a marker.
 * Therefore, an application using suspension must ensure that there is
 * enough buffer space for the initial markers (typ. 600-700 bytes) before
 * calling jpeg_start_compress, and enough space to write the trailing EOI
 * (a few bytes) before calling jpeg_finish_compress.  Multipass compression
 * modes are not supported at all with suspension, so those two are the only
 * points where markers will be written.
 */

void CMarkerWriter::emitByte(BYTE val) { // Emit a byte

  *dest->nextOutputByte++ = (JOCTET)val;
  if(!(--dest->freeInBuffer)) {
    if(!dest->emptyOutputBuffer())
      m_Parent->m_Parent->err->errorExit(JERR_CANT_SUSPEND);
		}
	}


void CMarkerWriter::emitMarker(int /*e' un enum JPEG_MARKER ! */ mark) { // Emit a marker code
  
	emitByte(0xFF);
  emitByte((int)mark);
	}


void CMarkerWriter::emit2bytes(short int value) {

// Emit a 2-byte integer; these are always MSB first in JPEG files
  emitByte((value >> 8) & 0xFF);
  emitByte(value & 0xFF);
	}

void CMarkerWriter::emitword(WORD value) {

// Emit a 2-byte integer; LSB first 
  emitByte(value & 0xFF);
  emitByte((value >> 8) & 0xFF);
	}


void CMarkerWriter::emitDateTimeExif() {
	int i;
	CString S;

	S.Format(_T("%04u:%02u:%02u %02u:%02u:%02u"),
		CTime::GetCurrentTime().GetYear(),
		CTime::GetCurrentTime().GetMonth(),
		CTime::GetCurrentTime().GetDay(),
		CTime::GetCurrentTime().GetHour(),
		CTime::GetCurrentTime().GetMinute(),
		CTime::GetCurrentTime().GetSecond());

	for(i=0; i<19; i++)
		emitByte(S.GetAt(i));
	emitByte(0);
	}

void CMarkerWriter::emitTextExif(const char *s,WORD maxlen) {
	int i,n;

	n=_tcslen(s);

	n=min(n,maxlen);
	for(i=0; i<n; i++)
		emitByte(*s++);
	for(i; i<maxlen; i++)
		emitByte(0);
	emitByte(0);
	}

void CMarkerWriter::emitTextExifUnicode(const char *s,WORD maxlen) {
	int i,n;

	n=_tcslen(s);

	n=min(n,maxlen);
	for(i=0; i<n; i++) {
		emitByte(*s++);
		emitByte(0);
		}
	for(i; i<maxlen; i++) {
		emitByte(0);
		emitByte(0);
		}
	emitByte(0);
	emitByte(0);
	}

/*
 * Routines to write specific marker types.
 */

int CCompressJpeg::emitDQT(int index) {
/* Emit a DQT marker */
/* Returns the precision used (0 = 8bits, 1 = 16bits) for baseline checking */

  JQUANT_TBL *qtbl = quantTblPtrs[index];
  int prec;
  int i;

  if(!qtbl)
    m_Parent->err->errorExit(JERR_NO_QUANT_TABLE, index);

  prec = 0;
  for(i=0; i < DCTSIZE2; i++) {
    if(qtbl->quantVal[i] > 255)
      prec = 1;
		}

  if(!qtbl->sentTable) {
    marker->emitMarker(CMarkerWriter::M_DQT);

    marker->emit2bytes(prec ? DCTSIZE2*2 + 1 + 2 : DCTSIZE2 + 1 + 2);

    marker->emitByte(index + (prec<<4));

    for(i=0; i < DCTSIZE2; i++) {
      // The table entries must be emitted in zigzag order.
      DWORD qval=qtbl->quantVal[naturalOrder[i]];
      if(prec)
				marker->emitByte((int)(qval >> 8));
      marker->emitByte((int) (qval & 0xFF));
			}

    qtbl->sentTable = TRUE;
		}

  return prec;
	}


void CCompressJpeg::emitDHT(int index, BOOL is_ac) {
// Emit a DHT marker 
  JHUFF_TBL *htbl;
  int length, i;
  
  if(is_ac) {
    htbl=AC_HuffTblPtrs[index];
    index += 0x10;		// output index has AC bit set
		}
	else {
    htbl=DC_HuffTblPtrs[index];
		}

  if(!htbl)
    m_Parent->err->errorExit(JERR_NO_HUFF_TABLE, index);
  
  if(!htbl->sentTable) {
    marker->emitMarker(CMarkerWriter::M_DHT);
    
    length = 0;
    for(i=1; i <= 16; i++)
      length += htbl->bits[i];
    
    marker->emit2bytes(length + 2 + 1 + 16);
    marker->emitByte(index);
    
    for(i=1; i <= 16; i++)
      marker->emitByte(htbl->bits[i]);
    
    for(i=0; i < length; i++)
      marker->emitByte(htbl->huffval[i]);
    
    htbl->sentTable = TRUE;
		}
	}


void CCompressJpeg::emitDac() {
/* Emit a DAC marker */
/* Since the useful info is so small, we want to emit all the tables in */
/* one DAC marker.  Therefore this routine does its own scan of the table. */
#ifdef C_ARITH_CODING_SUPPORTED
  char dc_in_use[NUM_ARITH_TBLS];
  char ac_in_use[NUM_ARITH_TBLS];
  int length, i;
  J_COMPONENT_INFO *compptr;
  
  for(i=0; i < NUM_ARITH_TBLS; i++)
    dc_in_use[i] = ac_in_use[i] = 0;
  
  for(i=0; i < compsInScan; i++) {
    compptr = cur_comp_info[i];
    dc_in_use[compptr->dc_tbl_no] = 1;
    ac_in_use[compptr->ac_tbl_no] = 1;
		}
  
  length = 0;
  for(i=0; i < NUM_ARITH_TBLS; i++)
    length += dc_in_use[i] + ac_in_use[i];
  
  emitMarker(M_DAC);
  
  emit2bytes(length*2 + 2);
  
  for(i=0; i < NUM_ARITH_TBLS; i++) {
    if(dc_in_use[i]) {
      emitByte(i);
      emitByte(arith_dc_L[i] + (arith_dc_U[i]<<4));
			}
    if(ac_in_use[i]) {
      emitByte(i + 0x10);
      emitByte(arith_ac_K[i]);
			}
		}
#endif // C_ARITH_CODING_SUPPORTED
	}


void CCompressJpeg::emitDri() {  // Emit a DRI marker

  marker->emitMarker(CMarkerWriter::M_DRI);
  
  marker->emit2bytes(4);	// fixed length 

  marker->emit2bytes((int)restartInterval);
	}


void CCompressJpeg::emitSof(int/* enum JPEG_MARKER ! */ code) { // Emit a SOF marker 
  int ci;
  J_COMPONENT_INFO *compptr;
  
  marker->emitMarker(code);
  
  marker->emit2bytes(3 * numComponents + 2 + 5 + 1); // length

  // Make sure image isn't bigger than SOF field can handle
  if((long)imageHeight > 65535L ||
      (long)imageWidth > 65535L)
    m_Parent->err->errorExit(JERR_IMAGE_TOO_BIG, (DWORD) 65535);

  marker->emitByte(dataPrecision);
  marker->emit2bytes((int)imageHeight);
  marker->emit2bytes((int)imageWidth);

  marker->emitByte(numComponents);

  for(ci=0, compptr = compInfo; ci < numComponents; ci++, compptr++) {
    marker->emitByte(compptr->componentId);
    marker->emitByte((compptr->hSampFactor << 4) + compptr->vSampFactor);
    marker->emitByte(compptr->quant_tbl_no);
		}
	}


void CCompressJpeg::emitSos() { // Emit a SOS marker 
  int i, td, ta;
  J_COMPONENT_INFO *compptr;
  
  marker->emitMarker(CMarkerWriter::M_SOS);
  
  marker->emit2bytes(2 * compsInScan + 2 + 1 + 3); // length
  
  marker->emitByte(compsInScan);
  
  for(i=0; i < compsInScan; i++) {
    compptr = curCompInfo[i];
    marker->emitByte(compptr->componentId);
    td = compptr->dc_tbl_no;
    ta = compptr->ac_tbl_no;
    if(progressiveMode) {
      /* Progressive mode: only DC or only AC tables are used in one scan;
       * furthermore, Huffman coding of DC refinement uses no table at all.
       * We emit 0 for unused field(s); this is recommended by the P&M text
       * but does not seem to be specified in the standard.
       */
      if(Ss == 0) {
				ta = 0;			// DC scan
				if(Ah != 0 && !arithCode)
					td = 0;		// no DC table either
				}
			else {
				td = 0;			// AC scan
				}
			}
    marker->emitByte((td << 4) + ta);
	  }

  marker->emitByte(Ss);
  marker->emitByte(Se);
  marker->emitByte((Ah << 4) + Al);
	}

void CCompressJpeg::emit_jfif_app0() {
/* Emit a JFIF-compliant APP0 marker */
  /*
   * Length of APP0 block	(2 bytes)
   * Block ID			(4 bytes - ASCII "JFIF")
   * Zero byte			(1 byte to terminate the ID string)
   * Version Major, Minor	(2 bytes - major first)
   * Units			(1 byte - 0x00 = none, 0x01 = inch, 0x02 = cm)
   * Xdpu			(2 bytes - dots per unit horizontal)
   * Ydpu			(2 bytes - dots per unit vertical)
   * Thumbnail X size		(1 byte)
   * Thumbnail Y size		(1 byte)
   */
  
  marker->emitMarker(CMarkerWriter::M_APP0);
  
#define JFIF_LEN (4 + 1 + 2 + 1 + 2 + 2 + 1 + 1)
  marker->emit2bytes(2 + JFIF_LEN); // length... 16?

	// seguono 14 byte
  marker->emitByte(0x4A);	// Identifier: ASCII "JFIF"
  marker->emitByte(0x46);
  marker->emitByte(0x49);
  marker->emitByte(0x46);
  marker->emitByte(0);
  marker->emitByte(JFIF_majorVersion); // Version fields
  marker->emitByte(JFIF_minorVersion);
  marker->emitByte(densityUnit); // Pixel size information
  marker->emit2bytes((int)XDensity);
  marker->emit2bytes((int)YDensity);
  marker->emitByte(0);		// No thumbnail image
  marker->emitByte(0);
	}

void CCompressJpeg::emit_exif_app1() {

/* Emit a EXIF-compliant APP1 marker 
	base... solo data/ora (dic.2005) */

  /*
   * Length of APP1 block	(2 bytes)
   * Block ID			(4 bytes - ASCII "EXIF")
   * Zero byte			(2 byte to terminate the ID string)
   * TIF Block ID			(3 bytes - ASCII "II*" - Intel format)
   * Zero byte			(1 byte to terminate the ID string)
   * offset to 1st TIFF directory (4 byte, usually 0x00000008)
   */

// https://www.awaresystems.be/imaging/tiff/tifftags/privateifd/exif.html  

  marker->emitMarker(CMarkerWriter::M_APP1);
  
#define EXIF_LEN (4 + 2 + 3 + 1 + 4  + 2 + (8*12) + 20 + 20 + 128 + 256 + 128 + 4)
#define EXIF_OFFSET ((8*12) +(4 + 2 + 3 + 1 + 4)+12 -12)
  marker->emit2bytes(2 + EXIF_LEN); // length

  marker->emitByte('E');	// Identifier: ASCII "Exif"
  marker->emitByte('x');
  marker->emitByte('i');
  marker->emitByte('f');
  marker->emitByte(0);
  marker->emitByte(0);
  marker->emitByte('I');
  marker->emitByte('I');
  marker->emitByte('*');
  marker->emitByte(0);
  marker->emitword(0x8);
  marker->emitword(0);

  marker->emitword(0x8);			// 8 directory entry

/*  marker->emitword(CMarkerWriter::TAG_EXIF_VERSION);		// version... 2.20
  marker->emitword(0x0000);			// type "short"
  marker->emitword(0x0004);			// len=4
  marker->emitword(0x0000);
  marker->emitword((DWORD)MAKEFOURCC('0','2','2','0'));
  marker->emitword(0x0000);*/

  marker->emitword(CMarkerWriter::TAG_EXIF_IMAGEWIDTH);		// width
  marker->emitword(0x0003);			// type "short"
  marker->emitword(0x0001);			// len=1
  marker->emitword(0x0000);
  marker->emitword((WORD)imageWidth);
  marker->emitword(0x0000);

  marker->emitword(CMarkerWriter::TAG_EXIF_IMAGELENGTH);		// height
  marker->emitword(0x0003);			// type "short"
  marker->emitword(0x0001);			// len=1
  marker->emitword(0x0000);
  marker->emitword((WORD)imageHeight);
  marker->emitword(0x0000);


  marker->emitword(CMarkerWriter::TAG_METERINGMODE);		// Metering mode... lo uso per il Ratio (4:3 o 16:9), 2016!
  marker->emitword(0x0003);			// type "short"
  marker->emitword(0x0001);			// len=1
  marker->emitword(0x0000);
  marker->emitword(imageRatio);
  marker->emitword(0x0000);

  marker->emitword(CMarkerWriter::TAG_DATETIME);		// DateTime
  marker->emitword(0x0002);			// type "ASCII"
  marker->emitword(20);			// len=20
  marker->emitword(0x0000);
  marker->emitword(EXIF_OFFSET);			// ofs= ? 0x00000046-12 (all'interno del TIFF!) (parametrizzare!)
  marker->emitword(0x0000);

  marker->emitword(CMarkerWriter::TAG_DATETIME_ORIGINAL);		// DateTimeOriginal
  marker->emitword(0x0002);			// type "ASCII"
  marker->emitword(20);			// len=20
  marker->emitword(0x0000);
  marker->emitword(EXIF_OFFSET+20);	// ofs= ? 0x00000046 +20 -12 (all'interno del TIFF!) (parametrizzare!)
  marker->emitword(0x0000);

  marker->emitword(CMarkerWriter::TAG_USERCOMMENT);		// Usercomment
  marker->emitword(0x0002);			// type "ASCII"
  marker->emitword(128);			// len=128
  marker->emitword(0x0000);
  marker->emitword(EXIF_OFFSET+40);			// ofs= ? 0x00000046+40-12 (all'interno del TIFF!) (parametrizzare!)
  marker->emitword(0x0000);
	//n.b. 2016: The Exif 2.3 specification explains this.  The first 8 bytes of the UserComment data specify the encoding.  For ASCII text, this is "ASCII\0\0\0".  For unknown encoding, use all zero bytes ("\0\0\0\0\0\0\0\0").
	// ovviamente è una cazzata... 2018

  marker->emitword(CMarkerWriter::TAG_XPKEYWORD);		// XP keyword, 2018 Scali
  marker->emitword(0x0001);			// type 1byte
  marker->emitword(256);			// len=128*2 UNICODE
  marker->emitword(0x0000);
  marker->emitword(EXIF_OFFSET+168);			// ofs= ? 0x00000046+168-12 (all'interno del TIFF!) (parametrizzare!)
  marker->emitword(0x0000);
	//2018: 

  marker->emitword(CMarkerWriter::TAG_AUTHOR);		// Author
  marker->emitword(0x0002);			// type "ASCII"
  marker->emitword(128);			// len=128
  marker->emitword(0x0000);
  marker->emitword(EXIF_OFFSET+168+256);			// ofs= ? 0x00000046+40+128+256-12 (all'interno del TIFF!) (parametrizzare!)
  marker->emitword(0x0000);
	//2021: 

  marker->emitword(0x0000);			// last IFD
  marker->emitword(0x0000);

	marker->emitDateTimeExif();		// 2 volte, sia "normale" che "original" :)
	marker->emitDateTimeExif();
	/*marker->emitTextExif("ASCII\0\0",7); 	*/ marker->emitTextExif(m_note,128-1);
	marker->emitTextExifUnicode(m_note,128-1);
	marker->emitTextExif(m_autore,128-1);

	}


void CCompressJpeg::emitAdobeApp14() { // Emit an Adobe APP14 marker 
  /*
   * Length of APP14 block	(2 bytes)
   * Block ID			(5 bytes - ASCII "Adobe")
   * Version Number		(2 bytes - currently 100)
   * Flags0			(2 bytes - currently 0)
   * Flags1			(2 bytes - currently 0)
   * Color transform		(1 byte)
   *
   * Although Adobe TN 5116 mentions Version = 101, all the Adobe files
   * now in circulation seem to use Version = 100, so that's what we write.
   *
   * We write the color transform byte as 1 if the JPEG color space is
   * YCbCr, 2 if it's YCCK, 0 otherwise.  Adobe's definition has to do with
   * whether the encoder performed a transformation, which is pretty useless.
   */
  
  marker->emitMarker(CMarkerWriter::M_APP14);
  
  marker->emit2bytes(2 + 5 + 2 + 2 + 2 + 1); /* length */

  marker->emitByte('A');	/* Identifier: ASCII "Adobe" */
  marker->emitByte('d');
  marker->emitByte('o');
  marker->emitByte('b');
  marker->emitByte('e');
  marker->emit2bytes(100);	/* Version */
  marker->emit2bytes(0);	/* Flags0 */
  marker->emit2bytes(0);	/* Flags1 */
  switch(colorSpace) {
		case JCS_YCbCr:
			marker->emitByte(1);	// Color transform = 1
			break;
		case JCS_YCCK:
			marker->emitByte(2);	// Color transform = 2
			break;
		default:
			marker->emitByte(0);	// Color transform = 0
			break;
		}
	}


/*
 * These routines allow writing an arbitrary marker with parameters.
 * The only intended use is to emit COM or APPn markers after calling
 * write_file_header and before calling write_frame_header.
 * Other uses are not guaranteed to produce desirable results.
 * Counting the parameter bytes properly is the caller's responsibility.
 */

void CMarkerWriter::writeMarkerHeader(int mark, DWORD datalen) { // Emit an arbitrary marker header 

  if(datalen > (DWORD)65533)		// safety check 
    m_Parent->m_Parent->err->errorExit(JERR_BAD_LENGTH);

  emitMarker( /*(JPEG_MARKER)*/ mark);

  emit2bytes((int)(datalen + 2));	// total length
	}

void CMarkerWriter::writeMarkerByte(BYTE val) {
// Emit one byte of marker parameters following write_marker_header 
  emitByte(val);
	}


/*
 * Write datastream header.
 * This consists of an SOI and optional APPn markers.
 * We recommend use of the JFIF marker, but not the Adobe marker,
 * when using YCbCr or grayscale data.  The JFIF marker should NOT
 * be used for any other JPEG colorspace.  The Adobe marker is helpful
 * to distinguish RGB, CMYK, and YCCK colorspaces.
 * Note that an application can write additional header markers after
 * jpeg_start_compress returns.
 */

void CCompressJpeg::writeFileHeader() {
	int i,j;

  marker->emitMarker(CMarkerWriter::M_SOI);	// first the SOI 

  // SOI is defined to reset restart interval to 0 
  marker->lastRestartInterval=0;

  if(write_JFIF_header)	// next an optional JFIF APP0
    emit_jfif_app0();
  if(write_EXIF_header)	// next an optional EXIF APP1
		emit_exif_app1();		// (per Data/Ora/nome/tag)
  if(writeAdobeMarker) // next an optional Adobe APP14
    emitAdobeApp14();

  j=_tcslen(m_Parent->m_appname);
	if(j>0) {
		marker->emitMarker(CMarkerWriter::M_COM);	// se serve un COMment...
		marker->emit2bytes(2 + j); /* length */

		for(i=0; i<j; i++) {
			marker->emitByte(m_Parent->m_appname[i]);	/* Identifier: ASCII */
			}
		}

  j=_tcslen(m_note);
	if(j>0) {
		marker->emitMarker(CMarkerWriter::M_APP12);	// se serve un COMment... USARE M_APP?
		marker->emit2bytes(2 + j); /* length */

		for(i=0; i<j; i++) {
			marker->emitByte(m_note[i]);	/* Identifier: ASCII */
			}
		}

	}


/*
 * Write frame header.
 * This consists of DQT and SOFn markers.
 * Note that we do not emit the SOF until we have emitted the DQT(s).
 * This avoids compatibility problems with incorrect implementations that
 * try to error-check the quant table numbers as soon as they see the SOF.
 */

void CCompressJpeg::writeFrameHeader() {
  int ci, prec;
  BOOL isBaseline;
  J_COMPONENT_INFO *compptr;
  
  /* Emit DQT for each quantization table.
   * Note that emit_dqt() suppresses any duplicate tables.
   */
  prec = 0;
  for(ci=0, compptr=compInfo; ci < numComponents; ci++, compptr++) {
    prec += emitDQT(compptr->quant_tbl_no);
		}
  /* now prec is nonzero iff there are any 16-bit quant tables. */

  /* Check for a non-baseline specification.
   * Note we assume that Huffman table numbers won't be changed later.
   */
  if(arithCode || progressiveMode || dataPrecision != 8) {
    isBaseline = FALSE;
		}
	else {
    isBaseline = TRUE;
    for(ci=0, compptr=compInfo; ci < numComponents; ci++, compptr++) {
      if(compptr->dc_tbl_no > 1 || compptr->ac_tbl_no > 1)
				isBaseline = FALSE;
			}
    if(prec && isBaseline) {
      isBaseline = FALSE;
      // If it's baseline except for quantizer size, warn the user
      m_Parent->err->trace(0,JTRC_16BIT_TABLES);
			}
		}

  // Emit the proper SOF marker 
  if(arithCode) {
    emitSof(CMarkerWriter::M_SOF9);	// SOF code for arithmetic coding 
		}
	else {
    if(progressiveMode)
      emitSof(CMarkerWriter::M_SOF2);	// SOF code for progressive Huffman 
    else if(isBaseline)
      emitSof(CMarkerWriter::M_SOF0);	// SOF code for baseline implementation 
    else
      emitSof(CMarkerWriter::M_SOF1);	// SOF code for non-baseline Huffman file 
		}
	}


/*
 * Write scan header.
 * This consists of DHT or DAC markers, optional DRI, and SOS.
 * Compressed data will be written following the SOS.
 */

void CCompressJpeg::writeScanHeader() {
  int i;
  J_COMPONENT_INFO *compptr;

  if(arithCode) {
    /* Emit arith conditioning info.  We may have some duplication
     * if the file has multiple scans, but it's so small it's hardly
     * worth worrying about.
     */
    emitDac();
		}
	else {
    /* Emit Huffman tables.
     * Note that emit_dht() suppresses any duplicate tables.
     */
    for(i=0; i<compsInScan; i++) {
      compptr=curCompInfo[i];
      if(progressiveMode) {
	// Progressive mode: only DC or only AC tables are used in one scan 
				if(!Ss) {
					if(!Ah)	// DC needs no table for refinement scan
						emitDHT(compptr->dc_tbl_no, FALSE);
					} 
				else {
					emitDHT(compptr->ac_tbl_no, TRUE);
					}
				}
			else {
	// Sequential mode: need both DC and AC tables 
				emitDHT(compptr->dc_tbl_no, FALSE);
				emitDHT(compptr->ac_tbl_no, TRUE);
				}
			}
		}

  /* Emit DRI if required --- note that DRI value could change for each scan.
   * We avoid wasting space with unnecessary DRIs, however.
   */
  if(restartInterval != marker->lastRestartInterval) {
    emitDri();
    marker->lastRestartInterval = restartInterval;
		}

  emitSos();
	}


/*
 * Write datastream trailer.
 */

void CCompressJpeg::writeFileTrailer() {

  marker->emitMarker(CMarkerWriter::M_EOI);
	}


/*
 * Write an abbreviated table-specification datastream.
 * This consists of SOI, DQT and DHT tables, and EOI.
 * Any table that is defined and not marked sent_table = TRUE will be
 * emitted.  Note that all tables will be marked sent_table = TRUE at exit.
 */

void CCompressJpeg::writeTablesOnly() {
  int i;

  marker->emitMarker(CMarkerWriter::M_SOI);

  for(i=0; i< NUM_QUANT_TBLS; i++) {
    if(quantTblPtrs[i] != NULL)
      (void)emitDQT(i);
		}

  if(!arithCode) {
    for(i=0; i < NUM_HUFF_TBLS; i++) {
      if(DC_HuffTblPtrs[i] != NULL)
				emitDHT(i,FALSE);
      if(AC_HuffTblPtrs[i] != NULL)
				emitDHT(i,TRUE);
			}
		}

  marker->emitMarker(CMarkerWriter::M_EOI);
	}


/*
 * Initialize the marker writer module.
 */
CMarkerWriter::CMarkerWriter(CCompressJpeg *p,CDestMgr *d) :
	m_Parent(p) {

  // Initialize method pointers
  // Initialize private state

	if(d)
		dest=d;
	else
		dest=p->dest;
  lastRestartInterval = 0;
	}



/*
 * jcmaster.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains master control logic for the JPEG compressor.
 * These routines are concerned with parameter validation, initial setup,
 * and inter-pass control (determining the number of passes and the work 
 * to be done in each pass).
 */



/*
 * Support routines that do various essential calculations.
 */

void CCompressJpeg::initialSetup() {
// Do computations that are needed before master selection phase 

  int ci;
  J_COMPONENT_INFO *compptr;
  long samplesperrow;
  JDIMENSION jd_samplesperrow;

  // Sanity check on image dimensions 
  if(imageHeight<=0 || imageWidth<=0 || numComponents<=0 || inputComponents<=0)
    m_Parent->err->errorExit(JERR_EMPTY_IMAGE);

  // Make sure image isn't bigger than I can handle 
  if((long)imageHeight > (long)JPEG_MAX_DIMENSION ||
     (long)imageWidth > (long)JPEG_MAX_DIMENSION)
    m_Parent->err->errorExit(JERR_IMAGE_TOO_BIG, (DWORD)JPEG_MAX_DIMENSION);

  // Width of an input scanline must be representable as JDIMENSION. 
  samplesperrow= (long)imageWidth * (long)inputComponents;
  jd_samplesperrow= (JDIMENSION)samplesperrow;
  if((long)jd_samplesperrow != samplesperrow)
    m_Parent->err->errorExit(JERR_WIDTH_OVERFLOW);

  // For now, precision must match compiled-in value... 
  if(dataPrecision != BITS_IN_JSAMPLE)
    m_Parent->err->errorExit(JERR_BAD_PRECISION, dataPrecision);

  // Check that number of components won't exceed internal array sizes 
  if(numComponents > MAX_COMPONENTS)
    m_Parent->err->errorExit(JERR_COMPONENT_COUNT, numComponents, MAX_COMPONENTS);

  // Compute maximum sampling factors; check factor validity 
  max_H_SampFactor=1;
  max_V_SampFactor=1;
  for(ci=0, compptr=compInfo; ci < numComponents; ci++, compptr++) {
    if(compptr->hSampFactor<=0 || compptr->hSampFactor>MAX_SAMP_FACTOR ||
			compptr->vSampFactor<=0 || compptr->vSampFactor>MAX_SAMP_FACTOR)
      m_Parent->err->errorExit(JERR_BAD_SAMPLING);
    max_H_SampFactor=max(max_H_SampFactor,compptr->hSampFactor);
    max_V_SampFactor=max(max_V_SampFactor,compptr->vSampFactor);
		}

  // Compute dimensions of components
  for(ci=0, compptr=compInfo; ci < numComponents; ci++, compptr++) {
    // Fill in the correct component_index value; don't rely on application 
    compptr->componentIndex=ci;
    // For compression, we never do DCT scaling. 
    compptr->DCT_scaledSize=DCTSIZE;
    // Size in DCT blocks 
    compptr->widthInBlocks=(JDIMENSION)
      CJpeg::divRoundUp((long)imageWidth * (long)compptr->hSampFactor,
		    (long)(max_H_SampFactor * DCTSIZE));
    compptr->heightInBlocks= (JDIMENSION)
      CJpeg::divRoundUp((long)imageHeight * (long)compptr->vSampFactor,
		    (long)(max_V_SampFactor * DCTSIZE));
    // Size in samples
    compptr->downsampledWidth= (JDIMENSION)
      CJpeg::divRoundUp((long)imageWidth * (long)compptr->hSampFactor,
		    (long)max_H_SampFactor);
    compptr->downsampledHeight= (JDIMENSION)
      CJpeg::divRoundUp((long)imageHeight * (long)compptr->vSampFactor,
		    (long)max_V_SampFactor);
    // Mark component needed (this flag isn't actually used for compression)
    compptr->componentNeeded = TRUE;
		}

  /* Compute number of fully interleaved MCU rows (number of times that
   * main controller will call coefficient controller).
   */
  total_iMCU_rows=(JDIMENSION)
    CJpeg::divRoundUp((long)imageHeight,(long)(max_V_SampFactor*DCTSIZE));
	}


#ifdef C_MULTISCAN_FILES_SUPPORTED

void CCompressJpeg::validateScript() {
/* Verify that the scan script in cinfo->scan_info[] is valid; also
 * determine whether it uses progressive JPEG, and set cinfo->progressive_mode.
 */
  const JPEG_SCAN_INFO *scanptr;
  int scanno,ncomps,ci,coefi,thisi;
  int Ss, Se, Ah, Al;
  BOOL component_sent[MAX_COMPONENTS];
#ifdef C_PROGRESSIVE_SUPPORTED
  int *last_bitpos_ptr;
  int last_bitpos[MAX_COMPONENTS][DCTSIZE2];
  /* -1 until that coefficient has been seen; then last Al for it */
#endif

  if(numScans <= 0)
    m_Parent->err->errorExit(JERR_BAD_SCAN_SCRIPT, 0);

  /* For sequential JPEG, all scans must have Ss=0, Se=DCTSIZE2-1;
   * for progressive JPEG, no scan can have this.
   */
  scanptr = scanInfo;
  if(scanptr->Ss || scanptr->Se != DCTSIZE2-1) {
#ifdef C_PROGRESSIVE_SUPPORTED
    progressiveMode = TRUE;
    last_bitpos_ptr = &last_bitpos[0][0];
    for(ci=0; ci<numComponents; ci++) 
      for(coefi=0; coefi < DCTSIZE2; coefi++)
				*last_bitpos_ptr++ = -1;
#else
    m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif
		}
	else {
    progressiveMode=FALSE;
    for(ci=0; ci<numComponents; ci++) 
      component_sent[ci] = FALSE;
		}

  for(scanno=1; scanno <= numScans; scanptr++, scanno++) {
    // Validate component indexes
    ncomps = scanptr->compsInScan;
    if(ncomps <= 0 || ncomps > MAX_COMPS_IN_SCAN)
      m_Parent->err->errorExit(JERR_COMPONENT_COUNT, ncomps, MAX_COMPS_IN_SCAN);
    for(ci=0; ci<ncomps; ci++) {
      thisi=scanptr->componentIndex[ci];
      if(thisi < 0 || thisi >= numComponents)
				m_Parent->err->errorExit(JERR_BAD_SCAN_SCRIPT, scanno);
      // Components must appear in SOF order within each scan
      if(ci > 0 && thisi <= scanptr->componentIndex[ci-1])
				m_Parent->err->errorExit(JERR_BAD_SCAN_SCRIPT, scanno);
			}
    // Validate progression parameters 
    Ss = scanptr->Ss;
    Se = scanptr->Se;
    Ah = scanptr->Ah;
    Al = scanptr->Al;
    if(progressiveMode) {
#ifdef C_PROGRESSIVE_SUPPORTED
      /* The JPEG spec simply gives the ranges 0..13 for Ah and Al, but that
       * seems wrong: the upper bound ought to depend on data precision.
       * Perhaps they really meant 0..N+1 for N-bit precision.
       * Here we allow 0..10 for 8-bit data; Al larger than 10 results in
       * out-of-range reconstructed DC values during the first DC scan,
       * which might cause problems for some decoders.
       */
#if BITS_IN_JSAMPLE == 8
#define MAX_AH_AL 10
#else
#define MAX_AH_AL 13
#endif
      if(Ss < 0 || Ss >= DCTSIZE2 || Se < Ss || Se >= DCTSIZE2 ||
				Ah < 0 || Ah > MAX_AH_AL || Al < 0 || Al > MAX_AH_AL)
				m_Parent->err->errorExit(JERR_BAD_PROG_SCRIPT, scanno);
      if(Ss == 0) {
				if(Se != 0)		// DC and AC together not OK
					m_Parent->err->errorExit(JERR_BAD_PROG_SCRIPT, scanno);
				}
			else {
				if(ncomps != 1)	// AC scans must be for only one component
					m_Parent->err->errorExit(JERR_BAD_PROG_SCRIPT, scanno);
				}
      for(ci=0; ci < ncomps; ci++) {
				last_bitpos_ptr = & last_bitpos[scanptr->component_index[ci]][0];
				if(Ss != 0 && last_bitpos_ptr[0] < 0) /* AC without prior DC scan */
					m_Parent->err->errorExit(JERR_BAD_PROG_SCRIPT, scanno);
				for(coefi=Ss; coefi <= Se; coefi++) {
					if(last_bitpos_ptr[coefi] < 0) {
	    // first scan of this coefficient 
	    if(Ah != 0)
	      m_Parent->err->errorExit(JERR_BAD_PROG_SCRIPT, scanno);
			}
		else {
	    // not first scan 
	    if(Ah != last_bitpos_ptr[coefi] || Al != Ah-1)
	      m_Parent->err->errorExit(JERR_BAD_PROG_SCRIPT, scanno);
				}
			last_bitpos_ptr[coefi] = Al;
			}
      }
#endif
    } 
		else {
      // For sequential JPEG, all progression parameters must be these: 
      if(Ss!=0 || Se != DCTSIZE2-1 || Ah != 0 || Al != 0)
				m_Parent->err->errorExit(JERR_BAD_PROG_SCRIPT, scanno);
      // Make sure components are not sent twice 
      for(ci=0; ci < ncomps; ci++) {
				thisi = scanptr->componentIndex[ci];
				if(component_sent[thisi])
					m_Parent->err->errorExit(JERR_BAD_SCAN_SCRIPT, scanno);
				component_sent[thisi] = TRUE;
				}
    }
  }

  // Now verify that everything got sent.
  if(progressiveMode) {
#ifdef C_PROGRESSIVE_SUPPORTED
    /* For progressive mode, we only check that at least some DC data
     * got sent for each component; the spec does not require that all bits
     * of all coefficients be transmitted.  Would it be wiser to enforce
     * transmission of all coefficient bits??
     */
    for(ci=0; ci < numComponents; ci++) {
      if(last_bitpos[ci][0] < 0)
				m_Parent->err->errorExit(JERR_MISSING_DATA);
			}
#endif
		} 
	else {
    for(ci=0; ci < numComponents; ci++) {
      if(!component_sent[ci])
				m_Parent->err->errorExit(JERR_MISSING_DATA);
			}
		}
	}

#endif // C_MULTISCAN_FILES_SUPPORTED


void CCompressJpeg::selectScanParameters() {
// Set up the scan parameters for the current scan 

  int ci;

#ifdef C_MULTISCAN_FILES_SUPPORTED
  if(scanInfo) {
    // Prepare for current scan --- the script is already validated
    const JPEG_SCAN_INFO *scanptr = scanInfo + master->scanNumber;

    compsInScan = scanptr->compsInScan;
    for(ci=0; ci < scanptr->compsInScan; ci++) {
      curCompInfo[ci]=&compInfo[scanptr->componentIndex[ci]];
			}
    Ss = scanptr->Ss;
    Se = scanptr->Se;
    Ah = scanptr->Ah;
    Al = scanptr->Al;
		}
  else
#endif
  {
    // Prepare for single sequential-JPEG scan containing all components 
    if(numComponents > MAX_COMPS_IN_SCAN)
      m_Parent->err->errorExit(JERR_COMPONENT_COUNT, numComponents,
	      MAX_COMPS_IN_SCAN);
    compsInScan = numComponents;
    for(ci=0; ci < numComponents; ci++)
      curCompInfo[ci] = &compInfo[ci];
			
    Ss = 0;
    Se = DCTSIZE2-1;
    Ah = 0;
    Al = 0;
		}
	}


void CCompressJpeg::perScanSetup() {
/* Do computations that are needed before processing a JPEG scan */
/* cinfo->comps_in_scan and cinfo->cur_comp_info[] are already set */

  int ci, mcublks, tmp;
  J_COMPONENT_INFO *compptr;
  
  if(compsInScan == 1) {
    
    // Noninterleaved (single-component) scan 
    compptr=curCompInfo[0];
    
    // Overall image size in MCUs 
    MCUs_perRow = compptr->widthInBlocks;
    MCU_rowsInScan = compptr->heightInBlocks;
    
    // For noninterleaved scan, always one block per MCU 
    compptr->MCU_width = 1;
    compptr->MCU_height = 1;
    compptr->MCU_blocks = 1;
    compptr->MCU_sampleWidth = DCTSIZE;
    compptr->lastColWidth = 1;
    /* For noninterleaved scans, it is convenient to define last_row_height
     * as the number of block rows present in the last iMCU row.
     */
    tmp=(int) (compptr->heightInBlocks % compptr->vSampFactor);
    if(!tmp)
			tmp=compptr->vSampFactor;
    compptr->lastRowHeight = tmp;
    
    // Prepare array describing MCU composition
    blocksInMCU = 1;
    MCU_membership[0] = 0;
    } 
	else {
    
    // Interleaved (multi-component) scan 
    if(compsInScan <= 0 || compsInScan > MAX_COMPS_IN_SCAN)
      m_Parent->err->errorExit(JERR_COMPONENT_COUNT, compsInScan,MAX_COMPS_IN_SCAN);
    
    // Overall image size in MCUs 
    MCUs_perRow=(JDIMENSION)
      CJpeg::divRoundUp((long)imageWidth, (long)(max_H_SampFactor*DCTSIZE));
		MCU_rowsInScan=(JDIMENSION)
      CJpeg::divRoundUp((long)imageHeight,(long)(max_V_SampFactor*DCTSIZE));
    
    blocksInMCU = 0;
    
    for(ci=0; ci < compsInScan; ci++) {
      compptr = curCompInfo[ci];
      // Sampling factors give # of blocks of component in each MCU 
      compptr->MCU_width = compptr->hSampFactor;
      compptr->MCU_height = compptr->vSampFactor;
      compptr->MCU_blocks = compptr->MCU_width * compptr->MCU_height;
      compptr->MCU_sampleWidth = compptr->MCU_width * DCTSIZE;
      // Figure number of non-dummy blocks in last MCU column & row 
      tmp=(int)(compptr->widthInBlocks % compptr->MCU_width);
      if(!tmp) 
				tmp=compptr->MCU_width;
      compptr->lastColWidth = tmp;
      tmp=(int)(compptr->heightInBlocks % compptr->MCU_height);
      if(!tmp)
				tmp=compptr->MCU_height;
      compptr->lastRowHeight=tmp;
      // Prepare array describing MCU composition 
      mcublks=compptr->MCU_blocks;
      if(blocksInMCU + mcublks > C_MAX_BLOCKS_IN_MCU)
				m_Parent->err->errorExit(JERR_BAD_MCU_SIZE);
      while(mcublks-- > 0)
				MCU_membership[blocksInMCU++]=ci;
				
			}
    
		}

  /* Convert restart specified in rows to actual MCU count. */
  /* Note that count must fit in 16 bits, so we provide limiting. */
  if(restartInRows > 0) {
    long nominal = (long)restartInRows * (long)MCUs_perRow;
    restartInterval = (DWORD)min(nominal, 65535L);
		}
	}


/*
 * Initialize the input controller module.
 * This is called only once, when the decompression object is created.
 */

void CDecompressJpeg::initInputController() {

  /* Create subobject in permanent pool */
  inputctl = new CInputController(this);

  /* Initialize method pointers */
  inputctl->doConsumeInput = inputctl->consumeMarkers;
  /* Initialize state: can't use reset_input_controller since we don't
   * want to try to reset other modules yet.
   */
  inputctl->hasMultipleScans = FALSE; /* "unknown" would be better */
  inputctl->EOI_Reached = FALSE;
  inputctl->inheaders = TRUE;
	}


void CDecompressJpeg::initialSetup() {
/* Called once, when first SOS marker is reached */
  int ci;
	J_COMPONENT_INFO *compptr;

  /* Make sure image isn't bigger than I can handle */
  if((long) imageHeight > (long) JPEG_MAX_DIMENSION ||
      (long) imageWidth > (long) JPEG_MAX_DIMENSION)
    m_Parent->err->errorExit(JERR_IMAGE_TOO_BIG, (unsigned int) JPEG_MAX_DIMENSION);

  /* For now, precision must match compiled-in value... */
  if(dataPrecision != BITS_IN_JSAMPLE)
    m_Parent->err->errorExit(JERR_BAD_PRECISION, dataPrecision);

  /* Check that number of components won't exceed internal array sizes */
  if(numComponents > MAX_COMPONENTS)
    m_Parent->err->errorExit(JERR_COMPONENT_COUNT, numComponents,
	    MAX_COMPONENTS);

  /* Compute maximum sampling factors; check factor validity */
  max_H_SampFactor = 1;
  max_V_SampFactor = 1;
  for(ci=0, compptr = compInfo; ci < numComponents; ci++, compptr++) {
    if(compptr->hSampFactor<=0 || compptr->hSampFactor>MAX_SAMP_FACTOR ||
			compptr->vSampFactor<=0 || compptr->vSampFactor>MAX_SAMP_FACTOR)
			m_Parent->err->errorExit(JERR_BAD_SAMPLING);
		max_H_SampFactor = max(max_H_SampFactor, compptr->hSampFactor);
		max_V_SampFactor = max(max_V_SampFactor, compptr->vSampFactor);
		}

  /* We initialize DCT_scaled_size and min_DCT_scaled_size to DCTSIZE.
   * In the full decompressor, this will be overridden by jdmaster.c;
   * but in the transcoder, jdmaster.c is not used, so we must do it here.
   */
  min_DCT_scaledSize = DCTSIZE;

  /* Compute dimensions of components */
  for(ci=0, compptr = compInfo; ci < numComponents; ci++, compptr++) {
    compptr->DCT_scaledSize = DCTSIZE;
    /* Size in DCT blocks */
    compptr->widthInBlocks = (JDIMENSION)
			CJpeg::roundUp((long) imageWidth * (long) compptr->hSampFactor,
		    (long) (max_H_SampFactor * DCTSIZE));
    compptr->heightInBlocks = (JDIMENSION)
      CJpeg::roundUp((long) imageHeight * (long) compptr->vSampFactor,
		    (long) (max_V_SampFactor * DCTSIZE));
    /* downsampled_width and downsampled_height will also be overridden by
     * jdmaster.c if we are doing full decompression.  The transcoder library
     * doesn't use these values, but the calling application might.
     */
    /* Size in samples */
    compptr->downsampledWidth = (JDIMENSION)
			CJpeg::roundUp((long) imageWidth * (long) compptr->hSampFactor,
		    (long) max_H_SampFactor);
    compptr->downsampledHeight = (JDIMENSION)
      CJpeg::roundUp((long) imageHeight * (long) compptr->vSampFactor,
		    (long) max_V_SampFactor);
    /* Mark component needed, until color conversion says otherwise */
    compptr->componentNeeded = TRUE;
    /* Mark no quantization table yet saved for component */
    compptr->quantTable = NULL;
		}

  /* Compute number of fully interleaved MCU rows. */
  total_iMCU_rows = (JDIMENSION)
		CJpeg::roundUp((long) imageHeight,
		  (long) (max_V_SampFactor*DCTSIZE));

  /* Decide whether file contains multiple scans */
  if(compsInScan < numComponents || progressiveMode)
    inputctl->hasMultipleScans = TRUE;
  else
    inputctl->hasMultipleScans = FALSE;
	}



/*
 * Save away a copy of the Q-table referenced by each component present
 * in the current scan, unless already saved during a prior scan.
 *
 * In a multiple-scan JPEG file, the encoder could assign different components
 * the same Q-table slot number, but change table definitions between scans
 * so that each component uses a different Q-table.  (The IJG encoder is not
 * currently capable of doing this, but other encoders might.)  Since we want
 * to be able to dequantize all the components at the end of the file, this
 * means that we have to save away the table actually used for each component.
 * We do this by copying the table at the start of the first scan containing
 * the component.
 * The JPEG spec prohibits the encoder from changing the contents of a Q-table
 * slot between scans of a component using that slot.  If the encoder does so
 * anyway, this decoder will simply use the Q-table values that were current
 * at the start of the first scan for the component.
 *
 * The decompressor output side looks only at the saved quant tables,
 * not at the current Q-table slots.
 */

void CInputController::latchQuantTables() {
  int ci, qtblno;
  J_COMPONENT_INFO *compptr;
  JQUANT_TBL * qtbl;

  for(ci=0; ci < m_Parent->compsInScan; ci++) {
    compptr = m_Parent->curCompInfo[ci];
    /* No work if we already saved Q-table for this component */
    if(compptr->quantTable != NULL)
      continue;
    /* Make sure specified quantization table is present */
    qtblno = compptr->quant_tbl_no;
    if(qtblno < 0 || qtblno >= NUM_QUANT_TBLS ||
			m_Parent->quantTblPtrs[qtblno] == NULL)
      m_Parent->m_Parent->err->errorExit(JERR_NO_QUANT_TABLE, qtblno);
    /* OK, save away the quantization table */
    qtbl = (JQUANT_TBL *)
      (m_Parent->m_Parent->mem->allocSmall) (JPOOL_IMAGE,
				 sizeof(JQUANT_TBL));
    memcpy(qtbl, m_Parent->quantTblPtrs[qtblno], sizeof(JQUANT_TBL));
    compptr->quantTable = qtbl;
		}
	}

/*
 * Initialize the input modules to read a scan of compressed data.
 * The first call to this is done by jdmaster.c after initializing
 * the entire decompressor (during jpeg_start_decompress).
 * Subsequent calls come from consume_markers, below.
 */

void CInputController::startInputPass() {

  perScanSetup();
  latchQuantTables();
  m_Parent->entropy->startPass();
  m_Parent->coef->startInputPass();
  m_Parent->inputctl->doConsumeInput = wrapConsumeData;
	}


/*
 * Finish up after inputting a compressed-data scan.
 * This is called by the coefficient controller after it's read all
 * the expected data of the scan.
 */

void CInputController::finishInputPass() {

  doConsumeInput = consumeMarkers;
	}

int CInputController::wrapConsumeData() {
	// patch xche' non posso cast-are da CCoef a CInput...
	(m_Parent->coef->*m_Parent->coef->doConsumeData)();
	return 0;
	}

/*
 * Read JPEG markers before, between, or after compressed-data scans.
 * Change state as necessary when a new scan is reached.
 * Return value is JPEG_SUSPENDED, JPEG_REACHED_SOS, or JPEG_REACHED_EOI.
 *
 * The consume_input method pointer points either here or to the
 * coefficient controller's consume_data routine, depending on whether
 * we are reading a compressed data segment or inter-segment markers.
 */
int CInputController::consumeMarkers() {
  int val;

  if(EOI_Reached)		/* After hitting EOI, read no further */
    return J_REACHED_EOI;

  val = m_Parent->marker->readMarkers();

  switch(val) {
		case J_REACHED_SOS:	/* Found SOS */
			if(inheaders) {	/* 1st SOS */
				m_Parent->initialSetup();
				inheaders = FALSE;
				/* Note: start_input_pass must be called by jdmaster.c
				 * before any more input can be consumed.  jdapi.c is
				 * responsible for enforcing this sequencing.
				 */
				}
			else {			/* 2nd or later SOS marker */
				if(!hasMultipleScans)
					m_Parent->m_Parent->err->errorExit(JERR_EOI_EXPECTED); /* Oops, I wasn't expecting this! */
				startInputPass();
				}
			break;
		case J_REACHED_EOI:	/* Found EOI */
			EOI_Reached = TRUE;
			if(inheaders) {	/* Tables-only datastream, apparently */
				if(m_Parent->marker->saw_SOF)
					m_Parent->m_Parent->err->errorExit(JERR_SOF_NO_SOS);
				}
			else {
      /* Prevent infinite loop in coef ctlr's decompress_data routine
       * if user set output_scan_number larger than number of scans.
       */
				if(m_Parent->outputScanNumber > m_Parent->inputScanNumber)
					m_Parent->outputScanNumber = m_Parent->inputScanNumber;
				}
	    break;
		case J_SUSPENDED:
			break;
		}

  return val;
	}


/*
 * Reset state to begin a fresh datastream.
 */

void CInputController::resetInputController() {

  doConsumeInput = consumeMarkers;
  hasMultipleScans = FALSE; /* "unknown" would be better */
  EOI_Reached = FALSE;
  inheaders = TRUE;
  /* Reset other modules */
  m_Parent->m_Parent->err->resetErrorMgr();
  m_Parent->marker->resetMarkerReader();
  /* Reset progression state -- would be cleaner if entropy decoder did this */
  m_Parent->coefBits = NULL;
	}


void CInputController::perScanSetup() {		// DIVERSA da quella in CCompressJpeg... (era così anche nell'originale!)
/* Do computations that are needed before processing a JPEG scan */
/* cinfo->comps_in_scan and cinfo->cur_comp_info[] are already set */

  int ci, mcublks, tmp;
  J_COMPONENT_INFO *compptr;
  
  if(m_Parent->compsInScan == 1) {
    
    // Noninterleaved (single-component) scan 
    compptr=m_Parent->curCompInfo[0];
    
    // Overall image size in MCUs 
    m_Parent->MCUs_perRow = compptr->widthInBlocks;
    m_Parent->MCU_rowsInScan = compptr->heightInBlocks;
    
    // For noninterleaved scan, always one block per MCU 
    compptr->MCU_width = 1;
    compptr->MCU_height = 1;
    compptr->MCU_blocks = 1;
    compptr->MCU_sampleWidth = DCTSIZE;
    compptr->lastColWidth = 1;
    /* For noninterleaved scans, it is convenient to define last_row_height
     * as the number of block rows present in the last iMCU row.
     */
    tmp=(int) (compptr->heightInBlocks % compptr->vSampFactor);
    if(!tmp)
			tmp=compptr->vSampFactor;
    compptr->lastRowHeight = tmp;
    
    // Prepare array describing MCU composition
    m_Parent->blocksInMCU = 1;
    m_Parent->MCU_membership[0] = 0;
    } 
	else {
    
    // Interleaved (multi-component) scan 
    if(m_Parent->compsInScan <= 0 || m_Parent->compsInScan > MAX_COMPS_IN_SCAN)
      m_Parent->m_Parent->err->errorExit(JERR_COMPONENT_COUNT, m_Parent->compsInScan,MAX_COMPS_IN_SCAN);
    
    // Overall image size in MCUs 
    m_Parent->MCUs_perRow=(JDIMENSION)
			CJpeg::divRoundUp((long)m_Parent->imageWidth, (long)(m_Parent->max_H_SampFactor*DCTSIZE));
		m_Parent->MCU_rowsInScan=(JDIMENSION)
      CJpeg::divRoundUp((long)m_Parent->imageHeight,(long)(m_Parent->max_V_SampFactor*DCTSIZE));
    
    m_Parent->blocksInMCU = 0;
    
    for(ci=0; ci < m_Parent->compsInScan; ci++) {
      compptr = m_Parent->curCompInfo[ci];
      // Sampling factors give # of blocks of component in each MCU 
      compptr->MCU_width = compptr->hSampFactor;
      compptr->MCU_height = compptr->vSampFactor;
      compptr->MCU_blocks = compptr->MCU_width * compptr->MCU_height;
      compptr->MCU_sampleWidth = compptr->MCU_width * DCTSIZE;
      // Figure number of non-dummy blocks in last MCU column & row 
      tmp=(int)(compptr->widthInBlocks % compptr->MCU_width);
      if(!tmp) 
				tmp=compptr->MCU_width;
      compptr->lastColWidth = tmp;
      tmp=(int)(compptr->heightInBlocks % compptr->MCU_height);
      if(!tmp)
				tmp=compptr->MCU_height;
      compptr->lastRowHeight=tmp;
      // Prepare array describing MCU composition 
      mcublks=compptr->MCU_blocks;
      if(m_Parent->blocksInMCU + mcublks > C_MAX_BLOCKS_IN_MCU)
				m_Parent->m_Parent->err->errorExit(JERR_BAD_MCU_SIZE);
      while(mcublks-- > 0)
				m_Parent->MCU_membership[m_Parent->blocksInMCU++]=ci;
				
			}
    
		}

  /* Convert restart specified in rows to actual MCU count. */
  /* Note that count must fit in 16 bits, so we provide limiting. */
  if(m_Parent->restartInRows > 0) {
    long nominal = (long)m_Parent->restartInRows * (long)m_Parent->MCUs_perRow;
    m_Parent->restartInterval = (DWORD)min(nominal, 65535L);
		}
	}

CInputController::CInputController(CDecompressJpeg *p) : m_Parent(p) {
	
  hasMultipleScans=FALSE;
  EOI_Reached=FALSE;
	inheaders=FALSE;
	doConsumeInput=NULL;
	doFinishInputPass=NULL;
	}



CDecompMaster::CDecompMaster(CDecompressJpeg *p) : m_Parent(p) {

	isDummyPass = FALSE;
  passNumber = 0;
  totalPasses = 0;
	}


/*
 * Determine whether merged upsample/color conversion should be used.
 * CRUCIAL: this must match the actual capabilities of jdmerge.c!
 */
BOOL CDecompMaster::useMergedUpsample() {

#ifdef UPSAMPLE_MERGING_SUPPORTED

  /* Merging is the equivalent of plain box-filter upsampling */
  if(m_Parent->doFancyUpsampling || cinfo->CCIR601_sampling)
    return FALSE;

  /* jdmerge.c only supports YCC=>RGB color conversion */
  if(jpegColorSpace != JCS_YCbCr || m_Parent->numComponents != 3 ||
    m_Parent->outColorSpace != JCS_RGB ||
    m_Parent->outColorComponents != RGB_PIXELSIZE)
    return FALSE;

  /* and it only handles 2h1v or 2h2v sampling ratios */
  if(m_Parent->compInfo[0].hSampFactor != 2 ||
    m_Parent->compInfo[1].hSampFactor != 1 ||
    m_Parent->compInfo[2].hSampFactor != 1 ||
    m_Parent->compInfo[0].vSampFactor >  2 ||
    m_Parent->compInfo[1].vSampFactor != 1 ||
    m_Parent->compInfo[2].vSampFactor != 1)
    return FALSE;

  /* furthermore, it doesn't work if we've scaled the IDCTs differently */
  if(m_Parent->compInfo[0].DCT_scaledSize != m_Parent->min_DCT_scaledSize ||
    m_Parent->compInfo[1].DCT_scaledSize != m_Parent->min_DCT_scaledSize ||
    m_Parent->compInfo[2].DCT_scaledSize != m_Parent->min_DCT_scaledSize)
    return FALSE;
  /* ??? also need to test for upsample-time rescaling, when & if supported */
  return TRUE;			/* by golly, it'll work... */

#else
  return FALSE;
#endif
	}


/*
 * Compute output image dimensions and related values.
 * NOTE: this is exported for possible use by application.
 * Hence it mustn't do anything that can't be done twice.
 * Also note that it may be called before the master module is initialized!
 */

void CDecompMaster::calcOutputDimensions() {
/* Do computations that are needed before master selection phase */
//  int ci;
//  J_COMPONENT_INFO *compptr;

  /* Prevent application from calling me at wrong times */
  if(m_Parent->globalState != CDecompressJpeg::DSTATE_READY)
    m_Parent->m_Parent->err->errorExit(JERR_BAD_STATE, m_Parent->globalState);

#ifdef IDCT_SCALING_SUPPORTED

  /* Compute actual output image dimensions and DCT scaling choices. */
  if(m_Parent->scaleNum * 8 <= m_Parent->scaleDenom) {
    /* Provide 1/8 scaling */
    m_Parent->outputWidth = (JDIMENSION)
			CJpegg::roundUp((long) m_Parent->imageWidth, 8L);
    m_Parent->outputHeight = (JDIMENSION)
			CJpegg::roundUp((long) m_Parent->imageHeight, 8L);
    m_Parent->min_DCT_scaledSsize = 1;
		} 
	else if(m_Parent->scaleNum * 4 <= m_Parent->scaleDenom) {
    /* Provide 1/4 scaling */
    m_Parent->outputWidth = (JDIMENSION)
			CJpeg::roundUp((long) m_Parent->imageWidth, 4L);
    cinfo->output_height = (JDIMENSION)
			CJpegg::roundUp((long) m_Parent->imageHeight, 4L);
    m_Parent->min_DCT_scaledSize = 2;
		} 
	else if(scaleNum * 2 <= scaleDenom) {
    /* Provide 1/2 scaling */
    outputWidth = (JDIMENSION)
			CJpeg::roundUp((long) m_Parent->imageWidth, 2L);
    m_Parent->outputHeight = (JDIMENSION)
			CJpeg::roundUp((long) m_Parent->imageHeight, 2L);
    m_Parent->min_DCT_scaledSize = 4;
		}
	else {
    /* Provide 1/1 scaling */
    m_Parent->outputWidth = m_Parent->imageWidth;
    m_Parent->outputHeight = m_Parent->imageHeight;
    m_Parent->min_DCT_scaledSize = DCTSIZE;
		}
  /* In selecting the actual DCT scaling for each component, we try to
   * scale up the chroma components via IDCT scaling rather than upsampling.
   * This saves time if the upsampler gets to use 1:1 scaling.
   * Note this code assumes that the supported DCT scalings are powers of 2.
   */
  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents;  ci++, compptr++) {
    int ssize = m_Parent->min_DCT_scaledSize;
    while(ssize < DCTSIZE &&
	    (compptr->hSampFactor * ssize * 2 <=
	    m_Parent->max_H_SampFactor * m_Parent->min_DCT_scaledSize) &&
	    (compptr->v_samp_factor * ssize * 2 <=
	    m_Parent->max_V_SampFactor * m_Parent->min_DCT_scaledSize)) {
      ssize = ssize * 2;
			}
    compptr->DCT_scaledSize = ssize;
		}

  /* Recompute downsampled dimensions of components;
   * application needs to know these if using raw downsampled data.
   */
  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {

    /* Size in samples, after IDCT scaling */
    compptr->downsampled_width = (JDIMENSION)
      CJpeg::roundUp((long) m_Parent->imageWidth *
		    (long) (compptr->hSampFactor * compptr->DCT_scaledSize),
		    (long) (cinfo->max_H_SampFactor * DCTSIZE));
    compptr->downsampled_height = (JDIMENSION)
      CJpeg::roundUp((long) m_Parent->imageHeight *
		    (long) (compptr->vSampFactor * compptr->DCT_scaledSize),
		    (long) (m_Parent->max_V_SampFactor * DCTSIZE));
		}

#else /* !IDCT_SCALING_SUPPORTED */

  /* Hardwire it to "no scaling" */
  m_Parent->outputWidth = m_Parent->imageWidth;
  m_Parent->outputHeight = m_Parent->imageHeight;
  /* jdinput.c has already initialized DCT_scaled_size to DCTSIZE,
   * and has computed unscaled downsampled_width and downsampled_height.
   */

#endif /* IDCT_SCALING_SUPPORTED */

  /* Report number of components in selected colorspace. */
  /* Probably this should be in the color conversion module... */
  switch(m_Parent->outColorSpace) {
		case JCS_GRAYSCALE:
			m_Parent->outColorComponents = 1;
			break;
		case JCS_RGB:
#if RGB_PIXELSIZE != 3
			m_Parent->outColorComponents = RGB_PIXELSIZE;
			break;
#endif /* else share code with YCbCr */
		case JCS_YCbCr:
			m_Parent->outColorComponents = 3;
	    break;
		case JCS_CMYK:
		case JCS_YCCK:
			m_Parent->outColorComponents = 4;
			break;
		default:			/* else must be same colorspace as in file */
			m_Parent->outColorComponents = m_Parent->numComponents;
			break;
		}
  m_Parent->outputComponents = (m_Parent->quantizeColors ? 1 :
		m_Parent->outColorComponents);

  /* See if upsampler will want to emit more than one row at a time */
  if(useMergedUpsample())
    m_Parent->recOutbufHeight = m_Parent->max_V_SampFactor;
  else
    m_Parent->recOutbufHeight = 1;
	}


/*
 * Several decompression processes need to range-limit values to the range
 * 0..MAXJSAMPLE; the input value may fall somewhat outside this range
 * due to noise introduced by quantization, roundoff error, etc.  These
 * processes are inner loops and need to be as fast as possible.  On most
 * machines, particularly CPUs with pipelines or instruction prefetch,
 * a (subscript-check-less) C table lookup
 *		x = sample_range_limit[x];
 * is faster than explicit tests
 *		if (x < 0)  x = 0;
 *		else if (x > MAXJSAMPLE)  x = MAXJSAMPLE;
 * These processes all use a common table prepared by the routine below.
 *
 * For most steps we can mathematically guarantee that the initial value
 * of x is within MAXJSAMPLE+1 of the legal range, so a table running from
 * -(MAXJSAMPLE+1) to 2*MAXJSAMPLE+1 is sufficient.  But for the initial
 * limiting step (just after the IDCT), a wildly out-of-range value is 
 * possible if the input data is corrupt.  To avoid any chance of indexing
 * off the end of memory and getting a bad-pointer trap, we perform the
 * post-IDCT limiting thus:
 *		x = range_limit[x & MASK];
 * where MASK is 2 bits wider than legal sample data, ie 10 bits for 8-bit
 * samples.  Under normal circumstances this is more than enough range and
 * a correct output will be generated; with bogus input data the mask will
 * cause wraparound, and we will safely generate a bogus-but-in-range output.
 * For the post-IDCT step, we want to convert the data from signed to unsigned
 * representation by adding CENTERJSAMPLE at the same time that we limit it.
 * So the post-IDCT limiting table ends up looking like this:
 *   CENTERJSAMPLE,CENTERJSAMPLE+1,...,MAXJSAMPLE,
 *   MAXJSAMPLE (repeat 2*(MAXJSAMPLE+1)-CENTERJSAMPLE times),
 *   0          (repeat 2*(MAXJSAMPLE+1)-CENTERJSAMPLE times),
 *   0,1,...,CENTERJSAMPLE-1
 * Negative inputs select values from the upper half of the table after
 * masking.
 *
 * We can save some space by overlapping the start of the post-IDCT table
 * with the simpler range limiting table.  The post-IDCT table begins at
 * sample_range_limit + CENTERJSAMPLE.
 *
 * Note that the table is allocated in near data space on PCs; it's small
 * enough and used often enough to justify this.
 */

void CDecompMaster::prepareRangeLimitTable()
/* Allocate and fill in the sample_range_limit table */
{
  JSAMPLE * table;
  int i;

  table = (JSAMPLE *)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,
		(5 * (MAXJSAMPLE+1) + CENTERJSAMPLE) * sizeof(JSAMPLE));
  table += (MAXJSAMPLE+1);	/* allow negative subscripts of simple table */
  m_Parent->sampleRangeLimit = table;
  /* First segment of "simple" table: limit[x] = 0 for x < 0 */
  ZeroMemory(table - (MAXJSAMPLE+1), (MAXJSAMPLE+1) * sizeof(JSAMPLE));
  /* Main part of "simple" table: limit[x] = x */
  for(i = 0; i <= MAXJSAMPLE; i++)
    table[i] = (JSAMPLE) i;
  table += CENTERJSAMPLE;	/* Point to where post-IDCT table starts */
  /* End of simple table, rest of first half of post-IDCT table */
  for(i = CENTERJSAMPLE; i < 2*(MAXJSAMPLE+1); i++)
    table[i] = MAXJSAMPLE;
  /* Second half of post-IDCT table */
  ZeroMemory(table + (2 * (MAXJSAMPLE+1)),
	  (2 * (MAXJSAMPLE+1) - CENTERJSAMPLE) * sizeof(JSAMPLE));
  memcpy(table + (4 * (MAXJSAMPLE+1) - CENTERJSAMPLE),
	  m_Parent->sampleRangeLimit, CENTERJSAMPLE * sizeof(JSAMPLE));
	}



/*
 * Per-pass setup.
 * This is called at the beginning of each output pass.  We determine which
 * modules will be active during this pass and give them appropriate
 * start_pass calls.  We also set is_dummy_pass to indicate whether this
 * is a "real" output pass or a dummy pass for color quantization.
 * (In the latter case, jdapi.c will crank the pass to completion.)
 */

void CDecompMaster::prepareForOutputPass() {

  if(isDummyPass) {
#ifdef QUANT_2PASS_SUPPORTED
    /* Final pass of 2-pass quantization */
    isDummyPass = FALSE;
    (m_Parent->cquantize->*m_Parent->cquantize->doStartPass) (FALSE);
    (*m_Parent->post->startPass) (JBUF_CRANK_DEST);
    (*m_Parent->main->startPass) (JBUF_CRANK_DEST);
#else
    m_Parent->m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif /* QUANT_2PASS_SUPPORTED */
		}
	else {
    if(m_Parent->quantizeColors && m_Parent->colormap == NULL) {
      /* Select new quantization method */
      if(m_Parent->twoPassQuantize && m_Parent->enable2passQuant) {
				m_Parent->cquantize = m_Parent->quantizer_2pass;
				isDummyPass = TRUE;
				}
			else if(m_Parent->enable1passQuant) {
				m_Parent->cquantize = m_Parent->quantizer_1pass;
				}
			else {
				m_Parent->m_Parent->err->errorExit(JERR_MODE_CHANGE);
		    }
	    }
    m_Parent->idct->startPass();
    m_Parent->coef->startOutputPass();
    if(!m_Parent->rawDataOut) {
      if(!usingMergedUpsample)
				m_Parent->cconvert->startPass();
			m_Parent->upsample->startPass();
      if(m_Parent->quantizeColors)
				(m_Parent->cquantize->*m_Parent->cquantize->doStartPass) (isDummyPass);
      m_Parent->post->startPass((isDummyPass ? JBUF_SAVE_AND_PASS : JBUF_PASS_THRU));
			m_Parent->main->startPass(JBUF_PASS_THRU);
			}
		}

  /* Set up progress monitor's pass info if present */
  if(m_Parent->m_Parent->progress != NULL) {
    m_Parent->m_Parent->progress->completedPasses = passNumber;
    m_Parent->m_Parent->progress->totalPasses = passNumber +
				    (isDummyPass ? 2 : 1);
    /* In buffered-image mode, we assume one more output pass if EOI not
     * yet reached, but no more passes if EOI has been reached.
     */
    if(m_Parent->bufferedImage && ! m_Parent->inputctl->EOI_Reached) {
      m_Parent->m_Parent->progress->totalPasses += (m_Parent->enable2passQuant ? 2 : 1);
			}
		}

	}

/*
 * Finish up at end of an output pass.
 */

void CDecompMaster::finishOutputPass()	{

  if(m_Parent->quantizeColors)
    (m_Parent->cquantize->*m_Parent->cquantize->doFinishPass) ();
  passNumber++;
	}


/*
 * jdmainct.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the main buffer controller for decompression.
 * The main buffer lies between the JPEG decompressor proper and the
 * post-processor; it holds downsampled data in the JPEG colorspace.
 *
 * Note that this code is bypassed in raw-data mode, since the application
 * supplies the equivalent of the main buffer in that case.
 */


/*
 * In the current system design, the main buffer need never be a full-image
 * buffer; any full-height buffers will be found inside the coefficient or
 * postprocessing controllers.  Nonetheless, the main controller is not
 * trivial.  Its responsibility is to provide context rows for upsampling/
 * rescaling, and doing this in an efficient fashion is a bit tricky.
 *
 * Postprocessor input data is counted in "row groups".  A row group
 * is defined to be (v_samp_factor * DCT_scaled_size / min_DCT_scaled_size)
 * sample rows of each component.  (We require DCT_scaled_size values to be
 * chosen such that these numbers are integers.  In practice DCT_scaled_size
 * values will likely be powers of two, so we actually have the stronger
 * condition that DCT_scaled_size / min_DCT_scaled_size is an integer.)
 * Upsampling will typically produce max_v_samp_factor pixel rows from each
 * row group (times any additional scale factor that the upsampler is
 * applying).
 *
 * The coefficient controller will deliver data to us one iMCU row at a time;
 * each iMCU row contains v_samp_factor * DCT_scaled_size sample rows, or
 * exactly min_DCT_scaled_size row groups.  (This amount of data corresponds
 * to one row of MCUs when the image is fully interleaved.)  Note that the
 * number of sample rows varies across components, but the number of row
 * groups does not.  Some garbage sample rows may be included in the last iMCU
 * row at the bottom of the image.
 *
 * Depending on the vertical scaling algorithm used, the upsampler may need
 * access to the sample row(s) above and below its current input row group.
 * The upsampler is required to set need_context_rows TRUE at global selection
 * time if so.  When need_context_rows is FALSE, this controller can simply
 * obtain one iMCU row at a time from the coefficient controller and dole it
 * out as row groups to the postprocessor.
 *
 * When need_context_rows is TRUE, this controller guarantees that the buffer
 * passed to postprocessing contains at least one row group's worth of samples
 * above and below the row group(s) being processed.  Note that the context
 * rows "above" the first passed row group appear at negative row offsets in
 * the passed buffer.  At the top and bottom of the image, the required
 * context rows are manufactured by duplicating the first or last real sample
 * row; this avoids having special cases in the upsampling inner loops.
 *
 * The amount of context is fixed at one row group just because that's a
 * convenient number for this controller to work with.  The existing
 * upsamplers really only need one sample row of context.  An upsampler
 * supporting arbitrary output rescaling might wish for more than one row
 * group of context when shrinking the image; tough, we don't handle that.
 * (This is justified by the assumption that downsizing will be handled mostly
 * by adjusting the DCT_scaled_size values, so that the actual scale factor at
 * the upsample step needn't be much less than one.)
 *
 * To provide the desired context, we have to retain the last two row groups
 * of one iMCU row while reading in the next iMCU row.  (The last row group
 * can't be processed until we have another row group for its below-context,
 * and so we have to save the next-to-last group too for its above-context.)
 * We could do this most simply by copying data around in our buffer, but
 * that'd be very slow.  We can avoid copying any data by creating a rather
 * strange pointer structure.  Here's how it works.  We allocate a workspace
 * consisting of M+2 row groups (where M = min_DCT_scaled_size is the number
 * of row groups per iMCU row).  We create two sets of redundant pointers to
 * the workspace.  Labeling the physical row groups 0 to M+1, the synthesized
 * pointer lists look like this:
 *                   M+1                          M-1
 * master pointer --> 0         master pointer --> 0
 *                    1                            1
 *                   ...                          ...
 *                   M-3                          M-3
 *                   M-2                           M
 *                   M-1                          M+1
 *                    M                           M-2
 *                   M+1                          M-1
 *                    0                            0
 * We read alternate iMCU rows using each master pointer; thus the last two
 * row groups of the previous iMCU row remain un-overwritten in the workspace.
 * The pointer lists are set up so that the required context rows appear to
 * be adjacent to the proper places when we pass the pointer lists to the
 * upsampler.
 *
 * The above pictures describe the normal state of the pointer lists.
 * At top and bottom of the image, we diddle the pointer lists to duplicate
 * the first or last sample row as necessary (this is cheaper than copying
 * sample rows around).
 *
 * This scheme breaks down if M < 2, ie, min_DCT_scaled_size is 1.  In that
 * situation each iMCU row provides only one row group so the buffering logic
 * must be different (eg, we must read two iMCU rows before we can emit the
 * first row group).  For now, we simply do not support providing context
 * rows when min_DCT_scaled_size is 1.  That combination seems unlikely to
 * be worth providing --- if someone wants a 1/8th-size preview, they probably
 * want it quick and dirty, so a context-free upsampler is sufficient.
 */




void CDMainController::allocFunnyPointers() {
/* Allocate space for the funny pointer lists.
 * This is done only once, not once per pass.
 */
	int ci, rgroup;
  int M = m_Parent->min_DCT_scaledSize;
  J_COMPONENT_INFO *compptr;
  JSAMPARRAY xbuf;

  /* Get top-level space for component array pointers.
   * We alloc both arrays with one call to save a few cycles.
   */
  xbuffer[0] = (JSAMPIMAGE)m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,
		m_Parent->numComponents * 2 * sizeof(JSAMPARRAY));
  xbuffer[1] = xbuffer[0] + m_Parent->numComponents;

  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    rgroup = (compptr->vSampFactor * compptr->DCT_scaledSize) /
      m_Parent->min_DCT_scaledSize; /* height of a row group of component */
    /* Get space for pointer lists --- M+4 row groups in each list.
     * We alloc both pointer lists with one call to save a few cycles.
     */
    xbuf = (JSAMPARRAY)(m_Parent->m_Parent->mem->allocSmall) (JPOOL_IMAGE,
		  2 * (rgroup * (M + 4)) * sizeof(JSAMPROW));
    xbuf += rgroup;		/* want one row group at negative offsets */
    xbuffer[0][ci] = xbuf;
    xbuf += rgroup * (M + 4);
    xbuffer[1][ci] = xbuf;
		}
	}


void CDMainController::makeFunnyPointers() {
/* Create the funny pointer lists discussed in the comments above.
 * The actual workspace is already allocated (in main->buffer),
 * and the space for the pointer lists is allocated too.
 * This routine just fills in the curiously ordered lists.
 * This will be repeated at the beginning of each pass.
 */
  int ci, i, rgroup;
  int M = m_Parent->min_DCT_scaledSize;
	J_COMPONENT_INFO *compptr;
  JSAMPARRAY buf, xbuf0, xbuf1;

  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    rgroup = (compptr->vSampFactor * compptr->DCT_scaledSize) /
      m_Parent->min_DCT_scaledSize; /* height of a row group of component */
    xbuf0 = xbuffer[0][ci];
    xbuf1 = xbuffer[1][ci];
    /* First copy the workspace pointers as-is */
    buf = buffer[ci];
    for(i=0; i < rgroup * (M + 2); i++) {
      xbuf0[i] = xbuf1[i] = buf[i];
			}
    /* In the second list, put the last four row groups in swapped order */
    for(i = 0; i < rgroup * 2; i++) {
      xbuf1[rgroup*(M-2) + i] = buf[rgroup*M + i];
      xbuf1[rgroup*M + i] = buf[rgroup*(M-2) + i];
    }
    /* The wraparound pointers at top and bottom will be filled later
     * (see set_wraparound_pointers, below).  Initially we want the "above"
     * pointers to duplicate the first actual data line.  This only needs
     * to happen in xbuffer[0].
     */
    for (i = 0; i < rgroup; i++) {
      xbuf0[i - rgroup] = xbuf0[0];
    }
  }
}


void CDMainController::setWraparoundPointers() {
/* Set up the "wraparound" pointers at top and bottom of the pointer lists.
 * This changes the pointer list state from top-of-image to the normal state.
 */
  int ci, i, rgroup;
  int M = m_Parent->min_DCT_scaledSize;
  J_COMPONENT_INFO *compptr;
  JSAMPARRAY xbuf0, xbuf1;

  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    rgroup = (compptr->vSampFactor * compptr->DCT_scaledSize) /
      m_Parent->min_DCT_scaledSize; /* height of a row group of component */
    xbuf0 = xbuffer[0][ci];
    xbuf1 = xbuffer[1][ci];
    for (i=0; i < rgroup; i++) {
      xbuf0[i - rgroup] = xbuf0[rgroup*(M+1) + i];
      xbuf1[i - rgroup] = xbuf1[rgroup*(M+1) + i];
      xbuf0[rgroup*(M+2) + i] = xbuf0[i];
      xbuf1[rgroup*(M+2) + i] = xbuf1[i];
			}
		}
	}


void CDMainController::setBottomPointers()
/* Change the pointer lists to duplicate the last sample row at the bottom
 * of the image.  whichptr indicates which xbuffer holds the final iMCU row.
 * Also sets rowgroups_avail to indicate number of nondummy row groups in row.
 */
{
  int ci, i, rgroup, iMCUheight, rows_left;
  J_COMPONENT_INFO *compptr;
  JSAMPARRAY xbuf;

  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    /* Count sample rows in one iMCU row and in one row group */
    iMCUheight = compptr->vSampFactor * compptr->DCT_scaledSize;
    rgroup = iMCUheight / m_Parent->min_DCT_scaledSize;
    /* Count nondummy sample rows remaining for this component */
    rows_left = (int) (compptr->downsampledHeight % (JDIMENSION) iMCUheight);
    if (rows_left == 0) rows_left = iMCUheight;
    /* Count nondummy row groups.  Should get same answer for each component,
     * so we need only do it once.
     */
    if(ci == 0) {
      rowgroupsAvail = (JDIMENSION) ((rows_left-1) / rgroup + 1);
			}
    /* Duplicate the last real sample row rgroup*2 times; this pads out the
     * last partial rowgroup and ensures at least one full rowgroup of context.
     */
    xbuf = xbuffer[whichptr][ci];
    for(i=0; i < rgroup * 2; i++) {
      xbuf[rows_left + i] = xbuf[rows_left-1];
			}
		}
	}


/*
 * Initialize for a processing pass.
 */

void CDMainController::startPass(J_BUF_MODE pass_mode) {

  switch(pass_mode) {
	  case JBUF_PASS_THRU:
		  if(m_Parent->upsample->needContextRows) {
				doProcessData = processDataContextMain;
				makeFunnyPointers(); /* Create the xbuffer[] lists */
				whichptr = 0;	/* Read first iMCU row into xbuffer[0] */
				contextState = CTX_PREPARE_FOR_IMCU;
				iMCU_rowCtr = 0;
				}
			else {
      /* Simple case with no context needed */
				doProcessData = processDataSimpleMain;
				}
			bufferFull = FALSE;	/* Mark buffer empty */
			rowgroupCtr = 0;
			break;
#ifdef QUANT_2PASS_SUPPORTED
		case JBUF_CRANK_DEST:
    /* For last pass of 2-pass quantization, just crank the postprocessor */
			doProcessData = processDataCrankPost;
			break;
#endif
		default:
			m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			break;
		}
	}


/*
 * Process some data.
 * This handles the simple case where no context is required.
 */

void CDMainController::processDataSimpleMain(JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	JDIMENSION out_rows_avail) {

//  JDIMENSION rowgroups_avail;

  /* Read input data if we haven't filled the main buffer yet */
  if(!bufferFull) {
    if(! (m_Parent->coef->*m_Parent->coef->doDecompressData) (buffer))
      return;			/* suspension forced, can do nothing more */
    bufferFull = TRUE;	/* OK, we have an iMCU row to work with */
		}

  /* There are always min_DCT_scaled_size row groups in an iMCU row. */
  rowgroupsAvail = (JDIMENSION) m_Parent->min_DCT_scaledSize;
  /* Note: at the bottom of the image, we may pass extra garbage row groups
   * to the postprocessor.  The postprocessor has to check for bottom
   * of image anyway (at row resolution), so no point in us doing it too.
   */

  /* Feed the postprocessor */
  (m_Parent->post->*m_Parent->post->doPostProcessData) (buffer,
				     &rowgroupCtr, rowgroupsAvail,
				     output_buf, out_row_ctr, out_rows_avail);

  /* Has postprocessor consumed all the data yet? If so, mark buffer empty */
  if(rowgroupCtr >= rowgroupsAvail) {
    bufferFull = FALSE;
    rowgroupCtr = 0;
		}
	}


/*
 * Process some data.
 * This handles the case where context rows must be provided.
 */

void CDMainController::processDataContextMain(JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	JDIMENSION out_rows_avail) {

  /* Read input data if we haven't filled the main buffer yet */
  if(!bufferFull) {
    if(! (m_Parent->coef->*m_Parent->coef->doDecompressData) (xbuffer[whichptr]))
      return;			/* suspension forced, can do nothing more */
    bufferFull = TRUE;	/* OK, we have an iMCU row to work with */
    iMCU_rowCtr++;	/* count rows received */
		}

  /* Postprocessor typically will not swallow all the input data it is handed
   * in one call (due to filling the output buffer first).  Must be prepared
   * to exit and restart.  This switch lets us keep track of how far we got.
   * Note that each case falls through to the next on successful completion.
   */
  switch(contextState) {
		case CTX_POSTPONED_ROW:
    /* Call postprocessor using previously set pointers for postponed row */
    (m_Parent->post->*m_Parent->post->doPostProcessData) (xbuffer[whichptr],
			&rowgroupCtr, rowgroupsAvail,
			output_buf, out_row_ctr, out_rows_avail);
    if(rowgroupCtr < rowgroupsAvail)
      return;			/* Need to suspend */
    contextState = CTX_PREPARE_FOR_IMCU;
    if(*out_row_ctr >= out_rows_avail)
      return;			/* Postprocessor exactly filled output buf */
    /*FALLTHROUGH*/
  case CTX_PREPARE_FOR_IMCU:
    /* Prepare to process first M-1 row groups of this iMCU row */
    rowgroupCtr = 0;
    rowgroupsAvail = (JDIMENSION) (m_Parent->min_DCT_scaledSize - 1);
    /* Check for bottom of image: if so, tweak pointers to "duplicate"
     * the last sample row, and adjust rowgroups_avail to ignore padding rows.
     */
    if(iMCU_rowCtr == m_Parent->total_iMCU_rows)
      setBottomPointers();
    contextState = CTX_PROCESS_IMCU;
    /*FALLTHROUGH*/
  case CTX_PROCESS_IMCU:
    /* Call postprocessor using previously set pointers */
    (m_Parent->post->*m_Parent->post->doPostProcessData) (xbuffer[whichptr],
			&rowgroupCtr, rowgroupsAvail,
			output_buf, out_row_ctr, out_rows_avail);
    if(rowgroupCtr < rowgroupsAvail)
      return;			/* Need to suspend */
    /* After the first iMCU, change wraparound pointers to normal state */
    if(iMCU_rowCtr == 1)
      setWraparoundPointers();
    /* Prepare to load new iMCU row using other xbuffer list */
    whichptr ^= 1;	/* 0=>1 or 1=>0 */
    bufferFull = FALSE;
    /* Still need to process last row group of this iMCU row, */
    /* which is saved at index M+1 of the other xbuffer */
    rowgroupCtr = (JDIMENSION) (m_Parent->min_DCT_scaledSize + 1);
    rowgroupsAvail = (JDIMENSION) (m_Parent->min_DCT_scaledSize + 2);
    contextState = CTX_POSTPONED_ROW;
		}
	}


/*
 * Process some data.
 * Final pass of two-pass quantization: just call the postprocessor.
 * Source data will be the postprocessor controller's internal buffer.
 */

#ifdef QUANT_2PASS_SUPPORTED

void CDMainController::processDataCrankPost(JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	JDIMENSION out_rows_avail) {

  (*m_Parent->post->postProcessData) ((JSAMPIMAGE) NULL,
		(JDIMENSION *) NULL, (JDIMENSION) 0,
		output_buf, out_row_ctr, out_rows_avail);
	}

#endif /* QUANT_2PASS_SUPPORTED */


/*
 * Initialize main buffer controller.
 */

CDMainController::CDMainController(CDecompressJpeg *p,BOOL need_full_buffer) : m_Parent(p) {
  int ci, rgroup, ngroups;
  J_COMPONENT_INFO *compptr;

  virtSarrayList=NULL;
	contextState=0;
	whichptr=0;
	iMCU_rowCtr=0;
	buffer[0]=NULL;
	xbuffer[0]=0;

  if(need_full_buffer)		/* shouldn't happen */
    m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);

  /* Allocate the workspace.
   * ngroups is the number of row groups we need.
   */
  if(m_Parent->upsample->needContextRows) {
    if(m_Parent->min_DCT_scaledSize < 2) /* unsupported, see comments above */
      m_Parent->m_Parent->err->errorExit(JERR_NOTIMPL);
    allocFunnyPointers(); /* Alloc space for xbuffer[] lists */
    ngroups = m_Parent->min_DCT_scaledSize + 2;
		}
	else {
    ngroups = m_Parent->min_DCT_scaledSize;
		}

  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    rgroup = (compptr->vSampFactor * compptr->DCT_scaledSize) /
      m_Parent->min_DCT_scaledSize; /* height of a row group of component */
    buffer[ci] = m_Parent->m_Parent->mem->allocSarray(JPOOL_IMAGE,
			compptr->widthInBlocks * compptr->DCT_scaledSize,
			(JDIMENSION) (rgroup * ngroups));
		}
	}








/*
 * Per-pass setup.
 * This is called at the beginning of each pass.  We determine which modules
 * will be active during this pass and give them appropriate start_pass calls.
 * We also set is_last_pass to indicate whether any more passes will be
 * required.
 */

void CCompMaster::prepareForPass() {

  switch(passType) {
		case mainPass:
    /* Initial pass: will collect input data, and do either Huffman
     * optimization or data output for the first scan.
     */
			m_Parent->selectScanParameters();
			m_Parent->perScanSetup();
			if(!m_Parent->rawDataIn) {
				(m_Parent->cconvert->*m_Parent->cconvert->doStartPass)();
				m_Parent->downsample->startPass();
				m_Parent->prep->startPass(JBUF_PASS_THRU);
				}
			m_Parent->fdct->startPass();
			m_Parent->entropy->startPass(m_Parent->optimizeCoding);
			m_Parent->coef->startPass(totalPasses > 1 ?
				JBUF_SAVE_AND_PASS : JBUF_PASS_THRU);
			m_Parent->main->startPass(JBUF_PASS_THRU);
			if(m_Parent->optimizeCoding) {
				// No immediate data output; postpone writing frame/scan headers 
				callPassStartup = FALSE;
				} 
			else {
				// Will write frame/scan headers at first jpeg_write_scanlines call
				callPassStartup = TRUE;
				}
			break;
#ifdef ENTROPY_OPT_SUPPORTED
		case huffOptPass:
			// Do Huffman optimization for a scan after the first one.
			m_Parent->selectScanParameters();
			m_Parent->perScanSetup();
			if(m_Parent->Ss != 0 || m_Parent->Ah == 0 || m_Parent->arithCode) {
				m_Parent->entropy->startPass(TRUE);
				m_Parent->coef->startPass(JBUF_CRANK_DEST);
				m_Parent->master->callPassStartup = FALSE;
				break;
				}
			/* Special case: Huffman DC refinement scans need no Huffman table
			 * and therefore we can skip the optimization pass for them.
			 */
			passType =outputPass;
			passNumber++;
			// FALLTHROUGH
#endif
		case outputPass:
			/* Do a data-output pass. */
			/* We need not repeat per-scan setup if prior optimization pass did it. */
			if(!m_Parent->optimizeCoding) {
				m_Parent->selectScanParameters();
				m_Parent->perScanSetup();
				}
			m_Parent->entropy->startPass(FALSE);
			m_Parent->coef->startPass(JBUF_CRANK_DEST);
			// We emit frame/scan headers now
			if(scanNumber == 0)
				m_Parent->writeFrameHeader();
			m_Parent->writeScanHeader();
			callPassStartup = FALSE;
			break;
		default:
			m_Parent->m_Parent->err->errorExit(JERR_NOT_COMPILED);
			}

	isLastPass = (passNumber == totalPasses-1);

  // Set up progress monitor's pass info if present
	if(m_Parent->m_Parent->progress) {
		m_Parent->m_Parent->progress->completedPasses = passNumber;
		m_Parent->m_Parent->progress->totalPasses = totalPasses;
		}

	}


/*
 * Special start-of-pass hook.
 * This is called by jpeg_write_scanlines if call_pass_startup is TRUE.
 * In single-pass processing, we need this hook because we don't want to
 * write frame/scan headers during jpeg_start_compress; we want to let the
 * application write COM markers etc. between jpeg_start_compress and the
 * jpeg_write_scanlines loop.
 * In multi-pass processing, this routine is not used.
 */

void CCompMaster::passStartup() {

	callPassStartup = FALSE; // reset flag so call only once 

	m_Parent->writeFrameHeader();
	m_Parent->writeScanHeader();
	}


/*
 * Finish up at end of pass.
 */
void CCompMaster::finishPass() {

  /* The entropy coder always needs an end-of-pass call,
   * either to analyze statistics or to flush its output buffer.
   */
  (m_Parent->entropy->*(m_Parent->entropy)->doFinishPass)();

  // Update state for next pass 
  switch(passType) {
		case mainPass:
			/* next pass is either output of scan 0 (after optimization)
			 * or output of scan 1 (if no optimization).
			 */
			passType=outputPass;
			if(!m_Parent->optimizeCoding)
				scanNumber++;
			break;
		case huffOptPass:
			// next pass is always output of current scan
			passType=outputPass;
			break;
		case outputPass:
			// next pass is either optimization or output of next scan
			if(m_Parent->optimizeCoding)
				passType =huffOptPass;
			scanNumber++;
			break;
		}

  passNumber++;
	}


/*
 * Initialize master compression control.
 */
CCompMaster::CCompMaster(CCompressJpeg *p,BOOL transcodeOnly) : m_Parent(p) {

  isLastPass = FALSE;

	// Validate parameters, determine derived values
  m_Parent->initialSetup();

  if(m_Parent->scanInfo) {
#ifdef C_MULTISCAN_FILES_SUPPORTED
    m_Parent->validateScript();
#else
    m_Parent->m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif
		} 
	else {
    m_Parent->progressiveMode=FALSE;
    m_Parent->numScans=1;
		}

  if(m_Parent->progressiveMode)	/*  TEMPORARY HACK ??? */
    m_Parent->optimizeCoding = TRUE; // assume default tables no good for progressive mode

  // Initialize my private state 
  if(transcodeOnly) {
    // no main pass in transcoding 
    if(m_Parent->optimizeCoding)
      passType = CCompMaster::huffOptPass;
    else
      passType = CCompMaster::outputPass;
		} 
	else {
    // for normal compression, first pass is always this type:
    passType = CCompMaster::mainPass;
		}
  scanNumber=0;
  passNumber=0;
  if(m_Parent->optimizeCoding)
    totalPasses = m_Parent->numScans * 2;
  else
    totalPasses = m_Parent->numScans;
	}




/*
 * jcomapi.c
 *
 * Copyright (C) 1994-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains application interface routines that are used for both
 * compression and decompression.
 */


/*
 * Abort processing of a JPEG compression or decompression operation,
 * but don't destroy the object itself.
 *
 * For this, we merely clean up all the nonpermanent memory pools.
 * Note that temp files (virtual arrays) are not allowed to belong to
 * the permanent pool, so we will be able to close all temp files here.
 * Closing a data source or destination, if necessary, is the application's
 * responsibility.
 */

void CJpeg::abort() {
  int pool;

  // Do nothing if called on a not-initialized or destroyed JPEG object.
  if(!mem)
    return;

  /* Releasing pools in reverse order might help avoid fragmentation
   * with some (brain-damaged) malloc libraries.
   */
  for(pool=JPOOL_NUMPOOLS-1; pool > JPOOL_PERMANENT; pool--) {
    mem->freePool(pool);
		}

  // Reset overall state for possible reuse of object
  if(isDecompressor) {
    de->globalState = CDecompressJpeg::DSTATE_START;
    /* Try to keep application from accessing now-deleted marker list.
     * A bit kludgy to do it here, but this is the most central place.
     */
    de->markerList=NULL;
		}
	else {
    co->globalState = CCompressJpeg::CSTATE_START;
		}
	}


/*
 * Destruction of a JPEG object.
 *
 * Everything gets deallocated except the master CCompressJpeg itself
 * and the error manager struct.  Both of these are supplied by the application
 * and must be freed, if necessary, by the application.  (Often they are on
 * the stack and so don't need to be freed anyway.)
 * Closing a data source or destination, if necessary, is the application's
 * responsibility.
 */

void CJpeg::destroy() {
  
	/* We need only tell the memory manager to release everything. */
  /* NB: mem pointer is NULL if memory mgr failed to initialize. */
  if(mem)
    selfDestruct();
  mem = NULL;		// be safe if jpeg_destroy is called twice 
  if(co)
		co->globalState=0;	// mark it destroyed 
  if(de)
		de->globalState=0;	// mark it destroyed 
	}


/*
 * Convenience routines for allocating quantization and Huffman tables.
 * (Would jutils.c be a more reasonable place to put these?)
 */

JQUANT_TBL *CCompressJpeg::allocQuantTable() {
  JQUANT_TBL *tbl;

  tbl=(JQUANT_TBL *)m_Parent->mem->allocSmall(JPOOL_PERMANENT, sizeof(JQUANT_TBL));
  tbl->sentTable = FALSE;	// make sure this is false in any new table 
  return tbl;
	}


JHUFF_TBL *CCompressJpeg::allocHuffTable() {
  JHUFF_TBL *tbl;

  tbl=(JHUFF_TBL *)m_Parent->mem->allocSmall(JPOOL_PERMANENT, sizeof(JHUFF_TBL));
  tbl->sentTable = FALSE;	// make sure this is false in any new table 
  return tbl;
	}




/*
 * jcparam.c
 *
 * Copyright (C) 1991-1998, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains optional default-setting code for the JPEG compressor.
 * Applications do not have to use this file, but those that don't use it
 * must know a lot more about the innards of the JPEG code.
 */


/*
 * Quantization table setup routines
 */

void CCompressJpeg::addQuantTable(int which_tbl, const DWORD *basic_table, int scale_factor, BOOL force_baseline)
/* Define a quantization table equal to the basic_table times
 * a scale factor (given as a percentage).
 * If force_baseline is TRUE, the computed quantization table entries
 * are limited to 1..255 for JPEG baseline compatibility.
 */
{
  JQUANT_TBL **qtblptr;
  int i;
  long temp;

  // Safety check to ensure start_compress not called yet. 
  if(globalState != CSTATE_START)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);

  if(which_tbl < 0 || which_tbl >= NUM_QUANT_TBLS)
    m_Parent->err->errorExit(JERR_DQT_INDEX, which_tbl);

  qtblptr= &quantTblPtrs[which_tbl];

  if(*qtblptr == NULL)
    *qtblptr = allocQuantTable();

  for(i=0; i < DCTSIZE2; i++) {
    temp=((long)basic_table[i] * scale_factor + 50L) / 100L;
    // limit the values to the valid range 
    if(temp <= 0L) temp = 1L;
    if(temp > 32767L) temp = 32767L; // max quantizer needed for 12 bits 
    if(force_baseline && temp > 255L)
      temp = 255L;		// limit to baseline range if requested 
    (*qtblptr)->quantVal[i] = (UINT16)temp;
		}

  // Initialize sent_table FALSE so table will be written to JPEG file. 
  (*qtblptr)->sentTable = FALSE;
	}


void CCompressJpeg::setLinearQuality(int scale_factor, BOOL force_baseline)
/* Set or change the 'quality' (quantization) setting, using default tables
 * and a straight percentage-scaling quality scale.  In most cases it's better
 * to use jpeg_set_quality (below); this entry point is provided for
 * applications that insist on a linear percentage scaling.
 */
{
  /* These are the sample quantization tables given in JPEG spec section K.1.
   * The spec says that the values given produce "good" quality, and
   * when divided by 2, "very good" quality.
   */
  static const DWORD std_luminance_quant_tbl[DCTSIZE2] = {
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99
		};
  static const DWORD std_chrominance_quant_tbl[DCTSIZE2] = {
    17,  18,  24,  47,  99,  99,  99,  99,
    18,  21,  26,  66,  99,  99,  99,  99,
    24,  26,  56,  99,  99,  99,  99,  99,
    47,  66,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
		};

  // Set up two quantization tables using the specified scaling
  addQuantTable(0, std_luminance_quant_tbl, scale_factor, force_baseline);
  addQuantTable(1, std_chrominance_quant_tbl, scale_factor, force_baseline);
	}


int CCompressJpeg::qualityScaling(int quality)
/* Convert a user-specified quality rating to a percentage scaling factor
 * for an underlying quantization table, using our recommended scaling curve.
 * The input 'quality' factor should be 0 (terrible) to 100 (very good).
 */
{
  // Safety limit on quality factor.  Convert 0 to 1 to avoid zero divide.
  if(quality <= 0) quality = 1;
  if(quality > 100) quality = 100;

  /* The basic table is used as-is (scaling 100) for a quality of 50.
   * Qualities 50..100 are converted to scaling percentage 200 - 2*Q;
   * note that at Q=100 the scaling is 0, which will cause jpeg_add_quant_table
   * to make all the table entries 1 (hence, minimum quantization loss).
   * Qualities 1..50 are converted to scaling percentage 5000/Q.
   */
  if(quality < 50)
    quality = 5000 / quality;
  else
    quality = 200 - quality*2;

  return quality;
	}


void CCompressJpeg::setQuality(int quality, BOOL force_baseline)
/* Set or change the 'quality' (quantization) setting, using default tables.
 * This is the standard quality-adjusting entry point for typical user
 * interfaces; only those who want detailed control over quantization tables
 * would use the preceding three routines directly.
 */
{
  // Convert user 0-100 rating to percentage scaling
  quality = qualityScaling(quality);

  // Set up standard quality tables 
  setLinearQuality(quality, force_baseline);
	}


/*
 * Huffman table setup routines
 */

void CCompressJpeg::addHuffTable(JHUFF_TBL **htblptr, const UINT8 *bits, const UINT8 *val) {
// Define a Huffman table 
  int nsymbols, len;

  if(*htblptr == NULL)
    *htblptr = allocHuffTable();

  // Copy the number-of-symbols-of-each-code-length counts
  memcpy((*htblptr)->bits, bits, sizeof((*htblptr)->bits));

  /* Validate the counts.  We do this here mainly so we can copy the right
   * number of symbols from the val[] array, without risking marching off
   * the end of memory.  jchuff.c will do a more thorough test later.
   */
  nsymbols = 0;
  for(len=1; len <= 16; len++)
    nsymbols += bits[len];
  if(nsymbols < 1 || nsymbols > 256)
    m_Parent->err->errorExit(JERR_BAD_HUFF_TABLE);

  memcpy((*htblptr)->huffval, val, nsymbols * sizeof(UINT8));

  // Initialize sent_table FALSE so table will be written to JPEG file.
  (*htblptr)->sentTable = FALSE;
	}


void CCompressJpeg::stdHuffTables() {
/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */
  static const UINT8 bits_dc_luminance[17] =
    { /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
  static const UINT8 val_dc_luminance[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  
  static const UINT8 bits_dc_chrominance[17] =
    { /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
  static const UINT8 val_dc_chrominance[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  
  static const UINT8 bits_ac_luminance[17] =
    { /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
  static const UINT8 val_ac_luminance[] =
    { 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
      0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
      0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
      0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
      0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
      0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
      0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
      0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
      0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
      0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
      0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
      0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
      0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
      0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
      0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
      0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
      0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
      0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
      0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa };
  
  static const UINT8 bits_ac_chrominance[17] =
    { /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
  static const UINT8 val_ac_chrominance[] =
    { 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
      0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
      0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
      0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
      0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
      0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
      0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
      0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
      0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
      0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
      0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
      0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
      0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
      0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
      0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
      0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
      0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
      0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
      0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
      0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa };
  
  addHuffTable(&DC_HuffTblPtrs[0],
		 bits_dc_luminance, val_dc_luminance);
  addHuffTable(&AC_HuffTblPtrs[0],
		 bits_ac_luminance, val_ac_luminance);
  addHuffTable(&DC_HuffTblPtrs[1],
		 bits_dc_chrominance, val_dc_chrominance);
  addHuffTable(&AC_HuffTblPtrs[1],
		 bits_ac_chrominance, val_ac_chrominance);
	}


/*
 * Default parameter setup for compression.
 *
 * Applications that don't choose to use this routine must do their
 * own setup of all these parameters.  Alternately, you can call this
 * to establish defaults and then alter parameters selectively.  This
 * is the recommended approach since, if we add any new parameters,
 * your code will still work (they'll be set to reasonable defaults).
 */

void CCompressJpeg::setDefaults() {
  int i;

  // Safety check to ensure start_compress not called yet. 
  if(globalState != CSTATE_START)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);

  /* Allocate comp_info array large enough for maximum component count.
   * Array is made permanent in case application wants to compress
   * multiple images at same param settings.
   */
  if(!compInfo)
    compInfo=(J_COMPONENT_INFO *)m_Parent->mem->allocSmall(JPOOL_PERMANENT,
			MAX_COMPONENTS * sizeof(J_COMPONENT_INFO));

  // Initialize everything not dependent on the color space 

	inputGamma=1.0;
  // Set up two quantization tables using default quality of 75 
  setQuality(75, TRUE);
  // Set up two Huffman tables 
  stdHuffTables();

  // Initialize default arithmetic coding conditioning 
  for(i=0; i < NUM_ARITH_TBLS; i++) {
    arith_dc_L[i] = 0;
    arith_dc_U[i] = 1;
    arith_ac_K[i] = 5;
		}

  // Default is no multiple-scan output 
  scanInfo = NULL;
  numScans = 0;

  // Expect normal source image, not raw downsampled data 
  rawDataIn = FALSE;

  // By default, don't do extra passes to optimize entropy coding 
  optimizeCoding = FALSE;
  /* The standard Huffman tables are only valid for 8-bit data precision.
   * If the precision is higher, force optimization on so that usable
   * tables will be computed.  This test can be removed if default tables
   * are supplied that are valid for the desired precision.
   */
  if(dataPrecision > 8)
    optimizeCoding = TRUE;

  // By default, use the simpler non-cosited sampling alignment 
  CCIR601Sampling=FALSE;

  // No input smoothing 
  smoothingFactor=0;

  // DCT algorithm preference 
  DCT_Method=JDCT_DEFAULT;

  // No restart markers 
  restartInterval=0;
  restartInRows=0;

  /* Fill in default JFIF marker parameters.  Note that whether the marker
   * will actually be written is determined by jpeg_set_colorspace.
   *
   * By default, the library emits JFIF version code 1.01.
   * An application that wants to emit JFIF 1.02 extension markers should set
   * JFIF_minor_version to 2.  We could probably get away with just defaulting
   * to 1.02, but there may still be some decoders in use that will complain
   * about that; saying 1.01 should minimize compatibility problems.
   */
  JFIF_majorVersion = 1; // Default JFIF version = 1.01
  JFIF_minorVersion = 1;
  densityUnit = 0;	// Pixel size is unknown by default
  XDensity = 1;			// Pixel aspect ratio is square by default
  YDensity = 1;

  // Choose JPEG colorspace based on input space, set defaults accordingly 
  defaultColorspace();
	}


/*
 * Select an appropriate JPEG colorspace for in_color_space.
 */

void CCompressJpeg::defaultColorspace() {
  
	switch(inColorSpace) {
		case JCS_GRAYSCALE:
			setColorspace(JCS_GRAYSCALE);
			break;
		case JCS_RGB:
			setColorspace(JCS_YCbCr);
			break;
		case JCS_YCbCr:
			setColorspace(JCS_YCbCr);
			break;
		case JCS_CMYK:
			setColorspace(JCS_CMYK); // By default, no translation
			break;
		case JCS_YCCK:
			setColorspace(JCS_YCCK);
			break;
		case JCS_UNKNOWN:
			setColorspace(JCS_UNKNOWN);
			break;
		default:
			m_Parent->err->errorExit(JERR_BAD_IN_COLORSPACE);
		}
	}


/*
 * Set the JPEG colorspace, and choose colorspace-dependent default values.
 */

void CCompressJpeg::setColorspace(J_COLOR_SPACE colorspace) {
  J_COMPONENT_INFO *compptr;
  int ci;

#define SET_COMP(index,id,hsamp,vsamp,quant,dctbl,actbl)  \
  (compptr = &compInfo[index], \
   compptr->componentId = (id), \
   compptr->hSampFactor = (hsamp), \
   compptr->vSampFactor = (vsamp), \
   compptr->quant_tbl_no = (quant), \
   compptr->dc_tbl_no = (dctbl), \
   compptr->ac_tbl_no = (actbl) )

  // Safety check to ensure start_compress not called yet.
  if(globalState != CSTATE_START)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);

  /* For all colorspaces, we use Q and Huff tables 0 for luminance components,
   * tables 1 for chrominance components.
   */

  colorSpace=colorspace;

  write_JFIF_header = FALSE; // No marker for non-JFIF colorspaces
  write_EXIF_header = FALSE; // idem...
  writeAdobeMarker = FALSE; // write no Adobe marker by default

  switch(colorspace) {
		case JCS_GRAYSCALE:
			write_JFIF_header = TRUE; // Write a JFIF marker
			write_EXIF_header = FALSE; // don't Write a EXIF marker
			numComponents=1;
			// JFIF specifies component ID 1
			SET_COMP(0, 1, 1,1, 0, 0,0);
			break;
		case JCS_RGB:
			writeAdobeMarker = TRUE; /* write Adobe marker to flag RGB */
			numComponents=3;
			SET_COMP(0, 0x52 /* 'R' */, 1,1, 0, 0,0);
			SET_COMP(1, 0x47 /* 'G' */, 1,1, 0, 0,0);
			SET_COMP(2, 0x42 /* 'B' */, 1,1, 0, 0,0);
			break;
		case JCS_YCbCr:
			write_JFIF_header=FALSE; // don' Write a JFIF marker ... 2005
			write_EXIF_header=TRUE; // Write a EXIF marker ... 2005
			numComponents=3;
			/* JFIF specifies component IDs 1,2,3 */
			/* We default to 2x2 subsamples of chrominance */
			SET_COMP(0, 1, 2,2, 0, 0,0);
			SET_COMP(1, 2, 1,1, 1, 1,1);
			SET_COMP(2, 3, 1,1, 1, 1,1);
			break;
		case JCS_CMYK:
			writeAdobeMarker = TRUE; /* write Adobe marker to flag CMYK */
			numComponents=4;
			SET_COMP(0, 0x43 /* 'C' */, 1,1, 0, 0,0);
			SET_COMP(1, 0x4D /* 'M' */, 1,1, 0, 0,0);
			SET_COMP(2, 0x59 /* 'Y' */, 1,1, 0, 0,0);
			SET_COMP(3, 0x4B /* 'K' */, 1,1, 0, 0,0);
			break;
		case JCS_YCCK:
			writeAdobeMarker = TRUE; // write Adobe marker to flag YCCK
			numComponents=4;
			SET_COMP(0, 1, 2,2, 0, 0,0);
			SET_COMP(1, 2, 1,1, 1, 1,1);
			SET_COMP(2, 3, 1,1, 1, 1,1);
			SET_COMP(3, 4, 2,2, 0, 0,0);
			break;
		case JCS_UNKNOWN:
			numComponents=inputComponents;
			if(numComponents < 1 || numComponents > MAX_COMPONENTS)
				m_Parent->err->errorExit(JERR_COMPONENT_COUNT, numComponents,
					 MAX_COMPONENTS);
			for(ci = 0; ci < numComponents; ci++) {
				SET_COMP(ci, ci, 1,1, 0, 0,0);
				}
			break;
		default:
			m_Parent->err->errorExit(JERR_BAD_J_COLORSPACE);
		}
	}


//#define C_PROGRESSIVE_SUPPORTED
#ifdef C_PROGRESSIVE_SUPPORTED

JPEG_SCAN_INFO *CCompressJpeg::fillAScan(JPEG_SCAN_INFO *scanptr, int ci,
	     int Ss, int Se, int Ah, int Al) {
// Support routine: generate one scan for specified component
  scanptr->compsInScan = 1;
  scanptr->componentIndex[0] = ci;
  scanptr->Ss = Ss;
  scanptr->Se = Se;
  scanptr->Ah = Ah;
  scanptr->Al = Al;
  scanptr++;
  return scanptr;
	}

JPEG_SCAN_INFO *CCompressJpeg::fillScans(JPEG_SCAN_INFO *scanptr, int ncomps,
	    int Ss, int Se, int Ah, int Al)
// Support routine: generate one scan for each component
{
  int ci;

  for(ci=0; ci < ncomps; ci++) {
    scanptr->compsInScan = 1;
    scanptr->componentIndex[0] = ci;
    scanptr->Ss = Ss;
    scanptr->Se = Se;
    scanptr->Ah = Ah;
    scanptr->Al = Al;
    scanptr++;
		}
  return scanptr;
	}

JPEG_SCAN_INFO *CCompressJpeg::fill_DC_scans(JPEG_SCAN_INFO *scanptr, int ncomps, int Ah, int Al)
// Support routine: generate interleaved DC scan if possible, else N scans
{
  int ci;

  if(ncomps <= MAX_COMPS_IN_SCAN) {
    // Single interleaved DC scan
    scanptr->compsInScan = ncomps;
    for(ci=0; ci < ncomps; ci++)
      scanptr->componentIndex[ci] = ci;
    scanptr->Ss = scanptr->Se = 0;
    scanptr->Ah = Ah;
    scanptr->Al = Al;
    scanptr++;
		}
	else {
    // Noninterleaved DC scan for each component
    scanptr = fillScans(scanptr, ncomps, 0, 0, Ah, Al);
		}
  return scanptr;
	}


/*
 * Create a recommended progressive-JPEG script.
 * cinfo->num_components and cinfo->jpeg_color_space must be correct.
 */

void CCompressJpeg::simpleProgression() {
  int ncomps = numComponents;
  int nscans;
  JPEG_SCAN_INFO *scanptr;

  // Safety check to ensure start_compress not called yet.
  if(globalState != CSTATE_START)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);

  // Figure space needed for script.  Calculation must match code below!
  if(ncomps == 3 && colorSpace == JCS_YCbCr) {
    // Custom script for YCbCr color images.
    nscans = 10;
		}
	else {
    // All-purpose script for other color spaces.
    if(ncomps > MAX_COMPS_IN_SCAN)
      nscans = 6 * ncomps;	/* 2 DC + 4 AC scans per component */
    else
      nscans = 2 + 4 * ncomps;	/* 2 DC scans; 4 AC scans per component */
		}

  /* Allocate space for script.
   * We need to put it in the permanent pool in case the application performs
   * multiple compressions without changing the settings.  To avoid a memory
   * leak if jpeg_simple_progression is called repeatedly for the same JPEG
   * object, we try to re-use previously allocated space, and we allocate
   * enough space to handle YCbCr even if initially asked for grayscale.
   */
  if(scriptSpace == NULL || scriptSpaceSize < nscans) {
    scriptSpaceSize = max(nscans, 10);
    scriptSpace=(JPEG_SCAN_INFO *)m_Parent->mem->allocSmall(JPOOL_PERMANENT,
			scriptSpaceSize * sizeof(JPEG_SCAN_INFO));
		}
  scanptr = scriptSpace;
  scanInfo = scanptr;
  numScans = nscans;

  if(ncomps == 3 && colorSpace == JCS_YCbCr) {
    // Custom script for YCbCr color images.
    // Initial DC scan 
    scanptr = fill_DC_scans(scanptr, ncomps, 0, 1);
    // Initial AC scan: get some luma data out in a hurry
    scanptr = fillAScan(scanptr, 0, 1, 5, 0, 2);
    // Chroma data is too small to be worth expending many scans on
    scanptr = fillAScan(scanptr, 2, 1, 63, 0, 1);
    scanptr = fillAScan(scanptr, 1, 1, 63, 0, 1);
    // Complete spectral selection for luma AC
    scanptr = fillAScan(scanptr, 0, 6, 63, 0, 2);
    // Refine next bit of luma AC
    scanptr = fillAScan(scanptr, 0, 1, 63, 2, 1);
    // Finish DC successive approximation
    scanptr = fill_DC_scans(scanptr, ncomps, 1, 0);
    // Finish AC successive approximation
    scanptr = fillAScan(scanptr, 2, 1, 63, 1, 0);
    scanptr = fillAScan(scanptr, 1, 1, 63, 1, 0);
    // Luma bottom bit comes last since it's usually largest scan 
    scanptr = fillAScan(scanptr, 0, 1, 63, 1, 0);
		}
	else {
    /* All-purpose script for other color spaces. */
    /* Successive approximation first pass */
    scanptr = fill_DC_scans(scanptr, ncomps, 0, 1);
    scanptr = fillScans(scanptr, ncomps, 1, 5, 0, 2);
    scanptr = fillScans(scanptr, ncomps, 6, 63, 0, 2);
    // Successive approximation second pass
    scanptr = fillScans(scanptr, ncomps, 1, 63, 2, 1);
    // Successive approximation final pass
    scanptr = fill_DC_scans(scanptr, ncomps, 1, 0);
    scanptr = fillScans(scanptr, ncomps, 1, 63, 1, 0);
		}
	}



/*
 * jcphuff.c
 *
 * Copyright (C) 1995-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains Huffman entropy encoding routines for progressive JPEG.
 *
 * We do not support output suspension in this module, since the library
 * currently does not allow multiple-scan files to be written with output
 * suspension.
 */



/* IRIGHT_SHIFT is like RIGHT_SHIFT, but works on int rather than INT32.
 * We assume that int right shift is unsigned if INT32 right shift is,
 * which should be safe.
 */

#ifdef RIGHT_SHIFT_IS_UNSIGNED
#define ISHIFT_TEMPS	int ishift_temp;
#define IRIGHT_SHIFT(x,shft)  \
	((ishift_temp = (x)) < 0 ? \
	 (ishift_temp >> (shft)) | ((~0) << (16-(shft))) : \
	 (ishift_temp >> (shft)))
#else
#define ISHIFT_TEMPS
#define IRIGHT_SHIFT(x,shft)	((x) >> (shft))
#endif


/*
 * Initialize for a Huffman-compressed scan using progressive JPEG.
 */

void PCHuffEntropyEncoder::startPass(BOOL gather_statistics) {  
  BOOL is_DC_band;
  int ci, tbl;
  J_COMPONENT_INFO *compptr;

ASSERT(0);
  gatherStatistics = gather_statistics;

  is_DC_band = !m_Parent->Ss;

  // We assume jcmaster.c already validated the scan parameters. 

  // Select execution routines 
  if(!m_Parent->Ah) {
    if(is_DC_band)
      doEncodeMCU = encode_mcu_DC_first;
    else
      doEncodeMCU = encode_mcu_AC_first;
		} 
	else {
    if(is_DC_band)
      doEncodeMCU = encode_mcu_DC_refine;
    else {
      doEncodeMCU = encode_mcu_AC_refine;
      // AC refinement needs a correction bit buffer
      if(bitBuffer == NULL)
				bitBuffer = (char *)
				m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE, MAX_CORR_BITS * sizeof(char));
			}
		}
  if(gatherStatistics)
    doFinishPass = finishPassGather;
  else
    doFinishPass = finishPass;

  /* Only DC coefficients may be interleaved, so cinfo->comps_in_scan = 1
   * for AC coefficients.
   */
  for(ci=0; ci < m_Parent->compsInScan; ci++) {
    compptr = m_Parent->curCompInfo[ci];
    // Initialize DC predictions to 0
    last_DC_val[ci] = 0;
    // Get table index
    if(is_DC_band) {
      if(m_Parent->Ah)	// DC refinement needs no table
				continue;
      tbl = compptr->dc_tbl_no;
			}
		else {
      ac_tbl_no = tbl = compptr->ac_tbl_no;
			}
    if(gatherStatistics) {
      /* Check for invalid table index */
      /* (make_c_derived_tbl does this in the other path) */
      if(tbl < 0 || tbl >= NUM_HUFF_TBLS)
        m_Parent->m_Parent->err->errorExit(JERR_NO_HUFF_TABLE, tbl);
      /* Allocate and zero the statistics tables */
      /* Note that jpeg_gen_optimal_table expects 257 entries in each table! */
      if(countPtrs[tbl] == NULL)
				countPtrs[tbl] = (long *)
					m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE, 257 * sizeof(long));
				ZeroMemory(countPtrs[tbl], 257 * sizeof(long));
			} 
		else {
      /* Compute derived values for Huffman table */
      /* We may do this more than once for a table, but it's not expensive */
      make_C_DerivedTbl(is_DC_band, tbl, &derivedTbls[tbl]);
			}
		}

  // Initialize AC stuff
  EOBRUN = 0;
  BE = 0;

  // Initialize bit buffer to empty
  putBuffer=0;
  putBits=0;

  // Initialize restart stuff 
  restartsToGo=m_Parent->restartInterval;
  nextRestartNum=0;
	}


/* Outputting bytes to the file.
 * NB: these must be called only when actually outputting,
 * that is, entropy->gather_statistics == FALSE.
 */

// Emit a byte (per phuff)
#define EMIT_BYTE2(val)  \
	{ *nextOutputByte++ = (JOCTET)(val);  \
	  if(--freeInBuffer == 0)  \
	    dumpBuffer(); }


void PCHuffEntropyEncoder::dumpBuffer() {
// Empty the output buffer; we do not support suspension in this module.
  J_STDIO_DESTINATION_MGR *dest = m_Parent->dest;

  if(!dest->emptyOutputBuffer())
    m_Parent->m_Parent->err->errorExit(JERR_CANT_SUSPEND);
  // After a successful buffer dump, must reset buffer pointers
  nextOutputByte=dest->nextOutputByte;
  freeInBuffer=dest->freeInBuffer;
	}


/* Outputting bits to the file */

/* Only the right 24 bits of put_buffer are used; the valid bits are
 * left-justified in this part.  At most 16 bits can be passed to emit_bits
 * in one call, and we never retain more than 7 bits in put_buffer
 * between calls, so 24 bits are sufficient.
 */

inline void PCHuffEntropyEncoder::emitBits(DWORD code, int size) { // Emit some bits, unless we are in gather mode
  // This routine is heavily used, so it's worth coding tightly.
  register INT32 put_buffer = (INT32)code;
  register int put_bits = putBits;

  // if size is 0, caller used an invalid Huffman table entry
  if(!size)
    m_Parent->m_Parent->err->errorExit(JERR_HUFF_MISSING_CODE);

  if(gatherStatistics)
    return;			// do nothing if we're only getting stats

  put_buffer &= (((INT32) 1)<<size) - 1; // mask off any extra bits in code
  
  put_bits += size;		// new number of bits in buffer
  
  put_buffer <<= 24 - putBits; // align incoming bits

  put_buffer |= putBuffer; // and merge with old buffer contents

  while(put_bits >= 8) {
    int c = (int) ((put_buffer >> 16) & 0xFF);
    
    EMIT_BYTE2(c);
    if(c == 0xFF) 		// need to stuff a zero byte? 
      EMIT_BYTE2(0);
    put_buffer <<= 8;
    put_bits -= 8;
		}

  putBuffer = put_buffer; // update variables 
  putBits = put_bits;
	}


void PCHuffEntropyEncoder::flushBits() {
  
	emitBits(0x7F, 7); // fill any partial byte with ones
  putBuffer = 0;     // and reset bit-buffer to empty
  putBits = 0;
	}


/*
 * Emit (or just count) a Huffman symbol.
 */

inline void PCHuffEntropyEncoder::emitSymbol(int tbl_no, int symbol) {
  
	if(gatherStatistics)
    countPtrs[tbl_no][symbol]++;
  else {
    C_DERIVED_TBL *tbl = derivedTbls[tbl_no];
    emitBits(tbl->ehufco[symbol], tbl->ehufsi[symbol]);
		}
	}


/*
 * Emit bits from a correction bit buffer.
 */

void PCHuffEntropyEncoder::emitBufferedBits(char *bufstart, DWORD nbits) {

  if(gatherStatistics)
    return;			// no real work

  while(nbits > 0) {
    emitBits((DWORD)(*bufstart), 1);
    bufstart++;
    nbits--;
		}
	}


/*
 * Emit any pending EOBRUN symbol.
 */

void PCHuffEntropyEncoder::emitEobrun() {
  register int temp, nbits;

  if(EOBRUN > 0) {	// if there is any pending EOBRUN
    temp=EOBRUN;
    nbits = 0;
    while((temp >>= 1))
      nbits++;
    // safety check: shouldn't happen given limited correction-bit buffer
    if(nbits > 14)
      m_Parent->m_Parent->err->errorExit(JERR_HUFF_MISSING_CODE);

    emitSymbol(ac_tbl_no, nbits << 4);
    if(nbits)
      emitBits(EOBRUN, nbits);

    EOBRUN = 0;

    // Emit any buffered correction bits
    emitBufferedBits(bitBuffer, BE);
    BE = 0;
		}
	}


/*
 * Emit a restart marker & resynchronize predictions.
 */

void PCHuffEntropyEncoder::emitRestart(int restart_num) {
  int ci;

  emitEobrun();

  if(!gatherStatistics) {
    flushBits();
    EMIT_BYTE2(0xFF);
    EMIT_BYTE2(CMarkerWriter::JPEG_RST0 + restart_num);
		}

  if(m_Parent->Ss == 0) {
    // Re-initialize DC predictions to 0
    for(ci=0; ci < m_Parent->compsInScan; ci++)
      last_DC_val[ci] = 0;
		}
	else {
    // Re-initialize all AC-related fields to 0
    EOBRUN = 0;
    BE = 0;
		}
	}


/*
 * MCU encoding for DC initial scan (either spectral selection,
 * or first pass of successive approximation).
 */

BOOL PCHuffEntropyEncoder::encode_mcu_DC_first(JBLOCKROW *MCU_data) {
  register int temp, temp2;
  register int nbits;
  int blkn, ci;
  JBLOCKROW block;
  J_COMPONENT_INFO *compptr;
  ISHIFT_TEMPS

  nextOutputByte = m_Parent->dest->nextOutputByte;
  freeInBuffer = m_Parent->dest->freeInBuffer;

  // Emit restart marker if needed 
  if(m_Parent->restartInterval)
    if(!restartsToGo)
      emitRestart(nextRestartNum);

  // Encode the MCU data blocks 
  for(blkn=0; blkn < m_Parent->blocksInMCU; blkn++) {
    block=MCU_data[blkn];
    ci=m_Parent->MCU_membership[blkn];
    compptr=m_Parent->curCompInfo[ci];

    /* Compute the DC value after the required point transform by Al.
     * This is simply an arithmetic right shift.
     */
    temp2 = IRIGHT_SHIFT((int) ((*block)[0]), m_Parent->Al);

    /* DC differences are figured on the point-transformed values. */
    temp=temp2-last_DC_val[ci];
    last_DC_val[ci] = temp2;

    /* Encode the DC coefficient difference per section G.1.2.1 */
    temp2=temp;
    if(temp < 0) {
      temp = -temp;		/* temp is abs value of input */
      /* For a negative input, want temp2 = bitwise complement of abs(input) */
      /* This code assumes we are on a two's complement machine */
      temp2--;
			}
    
    // Find the number of bits needed for the magnitude of the coefficient
    nbits = 0;
    while(temp) {
      nbits++;
      temp >>= 1;
			}
    /* Check for out-of-range coefficient values.
     * Since we're encoding a difference, the range limit is twice as much.
     */
    if(nbits > MAX_COEF_BITS+1)
      m_Parent->m_Parent->err->errorExit(JERR_BAD_DCT_COEF);
    
    /* Count/emit the Huffman-coded symbol for the number of bits */
    emitSymbol(compptr->dc_tbl_no, nbits);
    
    /* Emit that number of bits of the value, if positive,
			 or the complement of its magnitude, if negative. */
    if(nbits)			// emit_bits rejects calls with size 0 
      emitBits((DWORD)temp2, nbits);
		}

  m_Parent->dest->nextOutputByte = nextOutputByte;
  m_Parent->dest->freeInBuffer = freeInBuffer;

  // Update restart-interval state too
  if(m_Parent->restartInterval) {
    if(restartsToGo == 0) {
      restartsToGo = m_Parent->restartInterval;
      nextRestartNum++;
      nextRestartNum &= 7;
			}
    restartsToGo--;
		}

  return TRUE;
	}


/*
 * MCU encoding for AC initial scan (either spectral selection,
 * or first pass of successive approximation).
 */

BOOL PCHuffEntropyEncoder::encode_mcu_AC_first(JBLOCKROW *MCU_data) {
  register int temp, temp2;
  register int nbits;
  register int r, k;
  JBLOCKROW block;

  nextOutputByte = m_Parent->dest->nextOutputByte;
  freeInBuffer = m_Parent->dest->freeInBuffer;

  // Emit restart marker if needed
  if(m_Parent->restartInterval)
    if(!restartsToGo)
      emitRestart(nextRestartNum);

  // Encode the MCU data block
  block=MCU_data[0];

  // Encode the AC coefficients per section G.1.2.2, fig. G.3
  
  r=0;			// r = run length of zeros 
   
  for(k=m_Parent->Ss; k <= m_Parent->Se; k++) {
    if((temp = (*block)[m_Parent->naturalOrder[k]]) == 0) {
      r++;
      continue;
			}
    /* We must apply the point transform by Al.  For AC coefficients this
     * is an integer division with rounding towards 0.  To do this portably
     * in C, we shift after obtaining the absolute value; so the code is
     * interwoven with finding the abs value (temp) and output bits (temp2).
     */
    if(temp < 0) {
      temp = -temp;		// temp is abs value of input
      temp >>= m_Parent->Al;		// apply the point transform
      // For a negative coef, want temp2 = bitwise complement of abs(coef)
      temp2=~temp;
			}
		else {
      temp >>= m_Parent->Al;		// apply the point transform
      temp2=temp;
			}
    // Watch out for case that nonzero coef is zero after point transform
    if(!temp) {
      r++;
      continue;
			}

    // Emit any pending EOBRUN
    if(EOBRUN > 0)
      emitEobrun();
    // if run length > 15, must emit special run-length-16 codes (0xF0)
    while(r > 15) {
      emitSymbol(ac_tbl_no, 0xF0);
      r -= 16;
			}

    // Find the number of bits needed for the magnitude of the coefficient
    nbits = 1;			// there must be at least one 1 bit
    while((temp >>= 1))
      nbits++;
    // Check for out-of-range coefficient values
    if(nbits > MAX_COEF_BITS)
      m_Parent->m_Parent->err->errorExit(JERR_BAD_DCT_COEF);

    // Count/emit Huffman symbol for run length / number of bits
    emitSymbol(ac_tbl_no, (r << 4) + nbits);

    /* Emit that number of bits of the value, if positive, */
    /* or the complement of its magnitude, if negative. */
    emitBits((DWORD)temp2, nbits);

    r = 0;			// reset zero run length
		}

  if(r > 0) {			/* If there are trailing zeroes, */
    EOBRUN++;		/* count an EOB */
    if(EOBRUN == 0x7FFF)
      emitEobrun();	// force it out to avoid overflow
		}

  m_Parent->dest->nextOutputByte =nextOutputByte;
  m_Parent->dest->freeInBuffer = freeInBuffer;

  // Update restart-interval state too
  if(m_Parent->restartInterval) {
    if(restartsToGo == 0) {
      restartsToGo = m_Parent->restartInterval;
      nextRestartNum++;
      nextRestartNum &= 7;
			}
    restartsToGo--;
		}

  return TRUE;
	}


/*
 * MCU encoding for DC successive approximation refinement scan.
 * Note: we assume such scans can be multi-component, although the spec
 * is not very clear on the point.
 */

BOOL PCHuffEntropyEncoder::encode_mcu_DC_refine(JBLOCKROW *MCU_data) {
  register int temp;
  int blkn;
  JBLOCKROW block;

  nextOutputByte = m_Parent->dest->nextOutputByte;
  freeInBuffer = m_Parent->dest->freeInBuffer;

  // Emit restart marker if needed
  if(m_Parent->restartInterval)
    if(!restartsToGo)
      emitRestart(nextRestartNum);

  // Encode the MCU data blocks 
  for(blkn = 0; blkn < m_Parent->blocksInMCU; blkn++) {
    block = MCU_data[blkn];

    // We simply emit the Al'th bit of the DC coefficient value.
    temp = (*block)[0];
    emitBits((DWORD)(temp >> m_Parent->Al), 1);
	  }

  m_Parent->dest->nextOutputByte = nextOutputByte;
  m_Parent->dest->freeInBuffer = freeInBuffer;

  // Update restart-interval state too
  if(m_Parent->restartInterval) {
    if(!restartsToGo) {
      restartsToGo = m_Parent->restartInterval;
      nextRestartNum++;
      nextRestartNum &= 7;
			}
    restartsToGo--;
		}

  return TRUE;
	}


/*
 * MCU encoding for AC successive approximation refinement scan.
 */

BOOL PCHuffEntropyEncoder::encode_mcu_AC_refine(JBLOCKROW *MCU_data) {
  register int temp;
  register int r, k;
  int EOB;
  char *BR_buffer;
  DWORD BR;
  JBLOCKROW block;
  int absvalues[DCTSIZE2];

  nextOutputByte = m_Parent->dest->nextOutputByte;
  freeInBuffer = m_Parent->dest->freeInBuffer;

  // Emit restart marker if needed 
  if(m_Parent->restartInterval)
    if(!restartsToGo)
      emitRestart(nextRestartNum);

  // Encode the MCU data block 
  block = MCU_data[0];

  /* It is convenient to make a pre-pass to determine the transformed
   * coefficients' absolute values and the EOB position.
   */
  EOB=0;
  for(k=m_Parent->Ss; k <= m_Parent->Se; k++) {
    temp = (*block)[m_Parent->naturalOrder[k]];
    /* We must apply the point transform by Al.  For AC coefficients this
     * is an integer division with rounding towards 0.  To do this portably
     * in C, we shift after obtaining the absolute value.
     */
    if(temp < 0)
      temp = -temp;		// temp is abs value of input
    temp >>= m_Parent->Al;		// apply the point transform
    absvalues[k] = temp;	// save abs value for main pass
    if(temp == 1)
      EOB = k;			// EOB = index of last newly-nonzero coef
		}

  // Encode the AC coefficients per section G.1.2.3, fig. G.7
  
  r = 0;			/* r = run length of zeros */
  BR = 0;			/* BR = count of buffered bits added now */
  BR_buffer = bitBuffer + BE; // Append bits to buffer

  for(k=m_Parent->Ss; k <= m_Parent->Se; k++) {
    if((temp=absvalues[k]) == 0) {
      r++;
      continue;
			}

    // Emit any required ZRLs, but not if they can be folded into EOB
    while(r > 15 && k <= EOB) {
      /* emit any pending EOBRUN and the BE correction bits */
      emitEobrun();
      // Emit ZRL
      emitSymbol(ac_tbl_no, 0xF0);
      r -= 16;
      /* Emit buffered correction bits that must be associated with ZRL */
      emitBufferedBits(BR_buffer, BR);
      BR_buffer = bitBuffer; /* BE bits are gone now */
      BR = 0;
			}

    /* If the coef was previously nonzero, it only needs a correction bit.
     * NOTE: a straight translation of the spec's figure G.7 would suggest
     * that we also need to test r > 15.  But if r > 15, we can only get here
     * if k > EOB, which implies that this coefficient is not 1.
     */
    if(temp > 1) {
      // The correction bit is the next bit of the absolute value.
      BR_buffer[BR++] = (char) (temp & 1);
      continue;
			}

    // Emit any pending EOBRUN and the BE correction bits
    emitEobrun();

    // Count/emit Huffman symbol for run length / number of bits
    emitSymbol(ac_tbl_no, (r << 4) + 1);

    // Emit output bit for newly-nonzero coef
    temp=((*block)[m_Parent->naturalOrder[k]] < 0) ? 0 : 1;
    emitBits((DWORD)temp, 1);

    // Emit buffered correction bits that must be associated with this code
    emitBufferedBits(BR_buffer, BR);
    BR_buffer =bitBuffer; // BE bits are gone now
    BR = 0;
    r = 0;			// reset zero run length
	  }

  if(r > 0 || BR > 0) {	// If there are trailing zeroes,
    EOBRUN++;		// count an EOB
    BE += BR;		// concat my correction bits to older ones
    /* We force out the EOB if we risk either:
     * 1. overflow of the EOB counter;
     * 2. overflow of the correction bit buffer during the next MCU.
     */
    if(EOBRUN == 0x7FFF || BE > (MAX_CORR_BITS-DCTSIZE2+1))
      emitEobrun();
		}

  m_Parent->dest->nextOutputByte = nextOutputByte;
  m_Parent->dest->freeInBuffer = freeInBuffer;

  // Update restart-interval state too
  if(m_Parent->restartInterval) {
    if(!restartsToGo) {
      restartsToGo = m_Parent->restartInterval;
      nextRestartNum++;
      nextRestartNum &= 7;
			}
    restartsToGo--;
		}

  return TRUE;
	}


/*
 * Finish up at the end of a Huffman-compressed progressive scan.
 */

void PCHuffEntropyEncoder::finishPass() {

  nextOutputByte = m_Parent->dest->nextOutputByte;
  freeInBuffer = m_Parent->dest->freeInBuffer;

  // Flush out any buffered data
  emitEobrun();
  flushBits();

  m_Parent->dest->nextOutputByte = nextOutputByte;
  m_Parent->dest->freeInBuffer = freeInBuffer;
	}


/*
 * Finish up a statistics-gathering pass and create the new Huffman tables.
 */

void PCHuffEntropyEncoder::finishPassGather() {
  BOOL is_DC_band;
  int ci, tbl;
  J_COMPONENT_INFO *compptr;
  JHUFF_TBL **htblptr;
  BOOL did[NUM_HUFF_TBLS];

  // Flush out buffered data (all we care about is counting the EOB symbol)
  emitEobrun();

  is_DC_band = (m_Parent->Ss == 0);

  /* It's important not to apply jpeg_gen_optimal_table more than once
   * per table, because it clobbers the input frequency counts!
   */
  ZeroMemory(did,sizeof(did));

  for(ci=0; ci < m_Parent->compsInScan; ci++) {
    compptr = m_Parent->curCompInfo[ci];
    if(is_DC_band) {
      if(m_Parent->Ah != 0)	// DC refinement needs no table
				continue;
      tbl = compptr->dc_tbl_no;
			}
		else {
      tbl = compptr->ac_tbl_no;
			}
    if(!did[tbl]) {
      if(is_DC_band)
        htblptr = &m_Parent->DC_HuffTblPtrs[tbl];
      else
        htblptr = &m_Parent->AC_HuffTblPtrs[tbl];
      if(*htblptr == NULL)
        *htblptr = m_Parent->allocHuffTable();
      genOptimalTable(*htblptr, countPtrs[tbl]);
      did[tbl] = TRUE;
			}
		}
	}


/*
 * Module initialization routine for progressive Huffman entropy encoding.
 */

#endif // C_PROGRESSIVE_SUPPORTED

PCHuffEntropyEncoder::PCHuffEntropyEncoder(CCompressJpeg *p) : CEntropyEncoder(p) {
  int i;
	
  restartsToGo=p->restartInterval;	// c'e' poi anche in startPass, ma lo metto pure qua!
  nextRestartNum=0;
  // Initialize bit buffer to empty
  putBuffer=0;
  putBits=0;
	nextOutputByte=NULL;
	freeInBuffer=0;

  // Mark tables unallocated 
  for(i=0; i < NUM_HUFF_TBLS; i++) {
    derivedTbls[i] = NULL;
    countPtrs[i] = NULL;
		}
  bitBuffer = NULL;	// needed only in AC refinement scan
	}



/*
 * jcprepct.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the compression preprocessing controller.
 * This controller manages the color conversion, downsampling,
 * and edge expansion steps.
 *
 * Most of the complexity here is associated with buffering input rows
 * as required by the downsampler.  See the comments at the head of
 * jcsample.c for the downsampler's needs.
 */



/* At present, jcsample.c can request context rows only for smoothing.
 * In the future, we might also need context rows for CCIR601 sampling
 * or other more-complex downsampling procedures.  The code to support
 * context rows should be compiled only if needed.
 */


/*
 * For the simple (no-context-row) case, we just need to buffer one
 * row group's worth of pixels for the downsampling step.  At the bottom of
 * the image, we pad to a full row group by replicating the last pixel row.
 * The downsampler's last output row is then replicated if needed to pad
 * out to a full iMCU row.
 *
 * When providing context rows, we must buffer three row groups' worth of
 * pixels.  Three row groups are physically allocated, but the row pointer
 * arrays are made five row groups high, with the extra pointers above and
 * below "wrapping around" to point to the last and first real row groups.
 * This allows the downsampler to access the proper context rows.
 * At the top and bottom of the image, we create dummy context rows by
 * copying the first or last real pixel row.  This copying could be avoided
 * by pointer hacking as is done in jdmainct.c, but it doesn't seem worth the
 * trouble on the compression side.
 */



/*
 * Initialize for a processing pass.
 */

void CCompPrepController::startPass(J_BUF_MODE passMode) {

  if(passMode != JBUF_PASS_THRU)
    m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);

  // Initialize total-height counter for detecting bottom of image 
  rowsToGo = m_Parent->imageHeight;
  // Mark the conversion buffer empty
  nextBufRow = 0;
#ifdef CONTEXT_ROWS_SUPPORTED
  /* Preset additional state variables for context mode.
   * These aren't used in non-context mode, so we needn't test which mode.
   */
  thisRowGroup=0;
  /* Set next_buf_stop to stop after two row groups have been read in. */
  nextBufStop= 2 * m_Parent->max_V_SampFactor;
#endif
	}


/*
 * Expand an image vertically from height input_rows to height output_rows,
 * by duplicating the bottom row.
 */

void CCompressJpeg::expandBottomEdge(JSAMPARRAY image_data, JDIMENSION num_cols,
	int input_rows, int output_rows) {
  register int row;

  for(row=input_rows; row < output_rows; row++) {
		CCompressJpeg::copySampleRows(image_data, input_rows-1, image_data, row,
		  1, num_cols);
		}
	}


/*
 * Process some data in the simple no-context case.
 *
 * Preprocessor output data is counted in "row groups".  A row group
 * is defined to be v_samp_factor sample rows of each component.
 * Downsampling will produce this much data from each max_v_samp_factor
 * input rows.
 */

void CCompPrepController::preProcessData(JSAMPARRAY input_buf, JDIMENSION *in_row_ctr,
	JDIMENSION in_rows_avail,
	JSAMPIMAGE output_buf, JDIMENSION *out_row_group_ctr,
	JDIMENSION out_row_groups_avail) {
  int numrows, ci;
  JDIMENSION inrows;
  J_COMPONENT_INFO *compptr;

  while(*in_row_ctr < in_rows_avail &&
	 *out_row_group_ctr < out_row_groups_avail) {
    // Do color conversion to fill the conversion buffer. 
    inrows=in_rows_avail - *in_row_ctr;
    numrows= m_Parent->max_V_SampFactor - nextBufRow;
    numrows=(int)min((JDIMENSION)numrows, inrows);
    (m_Parent->cconvert->*(m_Parent->cconvert)->doColorConvert)(input_buf + *in_row_ctr,
			colorBuf,(JDIMENSION)nextBufRow, numrows);
    *in_row_ctr += numrows;
    nextBufRow += numrows;
    rowsToGo -= numrows;
    // If at bottom of image, pad to fill the conversion buffer.
    if(rowsToGo == 0 &&	nextBufRow < m_Parent->max_V_SampFactor) {
      for(ci=0; ci < m_Parent->numComponents; ci++) {
				((CCompressJpeg *)m_Parent)->expandBottomEdge(colorBuf[ci], m_Parent->imageWidth,
					nextBufRow, m_Parent->max_V_SampFactor);
				}
      nextBufRow = m_Parent->max_V_SampFactor;
			}
    // If we've filled the conversion buffer, empty it.
    if(nextBufRow == m_Parent->max_V_SampFactor) {
      (m_Parent->downsample->*(m_Parent->downsample)->doDownsample)(colorBuf, (JDIMENSION) 0,
					output_buf, *out_row_group_ctr);
      nextBufRow = 0;
      (*out_row_group_ctr)++;
			}
    /* If at bottom of image, pad the output to a full iMCU height.
     * Note we assume the caller is providing a one-iMCU-height output buffer!
     */
    if(rowsToGo == 0 &&	*out_row_group_ctr < out_row_groups_avail) {
      for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
				((CCompressJpeg *)m_Parent)->expandBottomEdge(output_buf[ci],
					compptr->widthInBlocks * DCTSIZE,
					(int)(*out_row_group_ctr * compptr->vSampFactor),
					(int)(out_row_groups_avail * compptr->vSampFactor));
				}
      *out_row_group_ctr = out_row_groups_avail;
      break;			// can exit outer loop without test 
			}
		}
	}


#ifdef CONTEXT_ROWS_SUPPORTED

/*
 * Process some data in the context case.
 */

void CCompPrepController::preProcessContext(JSAMPARRAY input_buf, JDIMENSION *in_row_ctr,
	JDIMENSION in_rows_avail,
	JSAMPIMAGE output_buf, JDIMENSION *out_row_group_ctr,
	JDIMENSION out_row_groups_avail) {
  int numrows, ci;
  int buf_height = m_Parent->max_V_SampFactor * 3;
  JDIMENSION inrows;

  while(*out_row_group_ctr < out_row_groups_avail) {
    if(*in_row_ctr < in_rows_avail) {
      // Do color conversion to fill the conversion buffer.
      inrows = in_rows_avail - *in_row_ctr;
      numrows = nextBufStop - nextBufRow;
      numrows = (int) min((JDIMENSION) numrows, inrows);
			(m_Parent->cconvert->*(m_Parent->cconvert)->doColorConvert) (input_buf + *in_row_ctr,
				colorBuf,(JDIMENSION)nextBufRow,numrows);
      // Pad at top of image, if first time through
      if(rowsToGo == m_Parent->imageHeight) {
				for(ci=0; ci < m_Parent->numComponents; ci++) {
					int row;
					for(row=1; row <= m_Parent->max_V_SampFactor; row++) {
						CCompressJpeg::copySampleRows(colorBuf[ci], 0,	colorBuf[ci], -row,
							1, m_Parent->imageWidth);
					}
				}
      }
      *in_row_ctr += numrows;
      nextBufRow += numrows;
      rowsToGo -= numrows;
			} 
		else {
      // Return for more data, unless we are at the bottom of the image. */
      if(rowsToGo != 0)
				break;
      // When at bottom of image, pad to fill the conversion buffer.
      if(nextBufRow < nextBufStop) {
				for(ci=0; ci < m_Parent->numComponents; ci++) {
					((CCompressJpeg *)m_Parent)->expandBottomEdge(colorBuf[ci], m_Parent->imageWidth,
						nextBufRow, nextBufStop);
				}
			nextBufRow = nextBufStop;
      }
    }
    // If we've gotten enough data, downsample a row group.
    if(nextBufRow == nextBufStop) {
      (m_Parent->downsample->*(m_Parent->downsample)->doDownsample)(colorBuf,
					(JDIMENSION)thisRowGroup,
					output_buf, *out_row_group_ctr);
      (*out_row_group_ctr)++;
      // Advance pointers with wraparound as necessary.
      thisRowGroup += m_Parent->max_V_SampFactor;
      if(thisRowGroup >= buf_height)
				thisRowGroup = 0;
      if(nextBufRow >= buf_height)
				nextBufRow = 0;
      nextBufStop = nextBufRow + m_Parent->max_V_SampFactor;
			}
		}
	}


/*
 * Create the wrapped-around downsampling input buffer needed for context mode.
 */

void CCompressJpeg::createContextBuffer() {
  int rgroup_height = max_V_SampFactor;
  int ci, i;
  J_COMPONENT_INFO *compptr;
  JSAMPARRAY true_buffer, fake_buffer;

  /* Grab enough space for fake row pointers for all the components;
   * we need five row groups' worth of pointers for each component.
   */
  fake_buffer=(JSAMPARRAY)m_Parent->mem->allocSmall(JPOOL_IMAGE,
		(numComponents * 5 * rgroup_height) * sizeof(JSAMPROW));

  for(ci=0, compptr = compInfo; ci < numComponents; ci++, compptr++) {
    /* Allocate the actual buffer space (3 row groups) for this component.
     * We make the buffer wide enough to allow the downsampler to edge-expand
     * horizontally within the buffer, if it so chooses.
     */
    true_buffer=m_Parent->mem->allocSarray(JPOOL_IMAGE,
			(JDIMENSION)(((long)compptr->widthInBlocks * DCTSIZE *
				max_H_SampFactor) / compptr->hSampFactor),
			(JDIMENSION)(3 * rgroup_height));
    // Copy true buffer row pointers into the middle of the fake row array 
    memcpy(fake_buffer + rgroup_height, true_buffer,
	    3 * rgroup_height * sizeof(JSAMPROW));
    // Fill in the above and below wraparound pointers 
    for(i=0; i < rgroup_height; i++) {
      fake_buffer[i] = true_buffer[2 * rgroup_height + i];
      fake_buffer[4 * rgroup_height + i] = true_buffer[i];
	    }
    prep->colorBuf[ci] = fake_buffer + rgroup_height;
    fake_buffer += 5 * rgroup_height; // point to space for next component 
		}
	}

#endif // CONTEXT_ROWS_SUPPORTED


/*
 * Initialize preprocessing controller.
 */
CCompPrepController::CCompPrepController(CCompressJpeg *p,BOOL need_full_buffer) : m_Parent(p) {
  int ci;
  J_COMPONENT_INFO *compptr;

  if(need_full_buffer)		// safety check 
    p->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);

  /* Allocate the color conversion buffer.
   * We make the buffer wide enough to allow the downsampler to edge-expand
   * horizontally within the buffer, if it so chooses.
   */
  if(p->downsample->needContextRows) {
    // Set up to provide context rows
#ifdef CONTEXT_ROWS_SUPPORTED
    doPreProcess=preProcessContext;
    p->createContextBuffer();
#else
    p->m_Parent->errorExit(JERR_NOT_COMPILED);
#endif
  } 
	else {
    // No context, just make it tall enough for one row group 
    doPreProcess=preProcessData;
    for(ci=0, compptr=p->compInfo; ci < p->numComponents;	ci++, compptr++) {
      colorBuf[ci]=m_Parent->m_Parent->mem->allocSarray(JPOOL_IMAGE,
				(JDIMENSION) (((long) compptr->widthInBlocks * DCTSIZE *
				p->max_H_SampFactor) / compptr->hSampFactor),
				(JDIMENSION)p->max_V_SampFactor);
			}
		}
	}


/*
 * jcsample.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains downsampling routines.
 *
 * Downsampling input data is counted in "row groups".  A row group
 * is defined to be max_v_samp_factor pixel rows of each component,
 * from which the downsampler produces v_samp_factor sample rows.
 * A single row group is processed in each call to the downsampler module.
 *
 * The downsampler is responsible for edge-expansion of its output data
 * to fill an integral number of DCT blocks horizontally.  The source buffer
 * may be modified if it is helpful for this purpose (the source buffer is
 * allocated wide enough to correspond to the desired output width).
 * The caller (the prep controller) is responsible for vertical padding.
 *
 * The downsampler may request "context rows" by setting need_context_rows
 * during startup.  In this case, the input arrays will contain at least
 * one row group's worth of pixels above and below the passed-in data;
 * the caller will create dummy rows at image top and bottom by replicating
 * the first or last real pixel row.
 *
 * An excellent reference for image resampling is
 *   Digital Image Warping, George Wolberg, 1990.
 *   Pub. by IEEE Computer Society Press, Los Alamitos, CA. ISBN 0-8186-8944-7.
 *
 * The downsampling algorithm used here is a simple average of the source
 * pixels covered by the output pixel.  The hi-falutin sampling literature
 * refers to this as a "box filter".  In general the characteristics of a box
 * filter are not very good, but for the specific cases we normally use (1:1
 * and 2:1 ratios) the box is equivalent to a "triangle filter" which is not
 * nearly so bad.  If you intend to use other sampling ratios, you'd be well
 * advised to improve this code.
 *
 * A simple input-smoothing capability is provided.  This is mainly intended
 * for cleaning up color-dithered GIF input files (if you find it inadequate,
 * we suggest using an external filtering program such as pnmconvol).  When
 * enabled, each input pixel P is replaced by a weighted sum of itself and its
 * eight neighbors.  P's weight is 1-8*SF and each neighbor's weight is SF,
 * where SF = (smoothing_factor / 1024).
 * Currently, smoothing is only supported for 2h2v sampling factors.
 */



/*
 * Initialize for a downsampling pass.
 */

void CDownsampler::startPass() {
  // no work for now
	}


/*
 * Expand a component horizontally from width input_cols to width output_cols,
 * by duplicating the rightmost samples.
 */

void CDownsampler::expandRightEdge(JSAMPARRAY image_data, int num_rows,
  JDIMENSION input_cols, JDIMENSION output_cols) {
  register JSAMPROW ptr;
  register JSAMPLE pixval;
  register int count;
  int row;
  int numcols = (int) (output_cols - input_cols);

  if(numcols > 0) {
    for(row=0; row < num_rows; row++) {
      ptr=image_data[row] + input_cols;
      pixval=ptr[-1];		// don't need GETJSAMPLE() here 
      for(count=numcols; count > 0; count--)
				*ptr++ = pixval;
			}
		}
	}


/*
 * Do downsampling for a whole row group (all components).
 *
 * In this version we simply downsample each component independently.
 */

void CDownsampler::sepDownsample(JSAMPIMAGE input_buf, JDIMENSION in_row_index,
		JSAMPIMAGE output_buf, JDIMENSION out_row_group_index) {
  int ci;
  J_COMPONENT_INFO *compptr;
  JSAMPARRAY in_ptr, out_ptr;

  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    in_ptr=input_buf[ci]+in_row_index;
    out_ptr=output_buf[ci]+(out_row_group_index * compptr->vSampFactor);
    myMethod=methods[ci];
//    (*(m->methods[ci]))(compptr, in_ptr, out_ptr);
    (this->*myMethod)(compptr,in_ptr,out_ptr);
		}
	}


/*
 * Downsample pixel values of a single component.
 * One row group is processed per call.
 * This version handles arbitrary integral sampling ratios, without smoothing.
 * Note that this version is not actually used for customary sampling ratios.
 */

void CDownsampler::intDownsample(J_COMPONENT_INFO *compptr,
	JSAMPARRAY input_data, JSAMPARRAY output_data) {
  int inrow, outrow, h_expand, v_expand, numpix, numpix2, h, v;
  JDIMENSION outcol, outcol_h;	// outcol_h == outcol*h_expand
  JDIMENSION output_cols = compptr->widthInBlocks * DCTSIZE;
  JSAMPROW inptr, outptr;
  INT32 outvalue;

  h_expand= m_Parent->max_H_SampFactor / compptr->hSampFactor;
  v_expand= m_Parent->max_V_SampFactor / compptr->vSampFactor;
  numpix = h_expand * v_expand;
  numpix2 = numpix/2;

  /* Expand input data enough to let all the output samples be generated
   * by the standard loop.  Special-casing padded output would be more
   * efficient.
   */
  expandRightEdge(input_data, m_Parent->max_V_SampFactor,
		m_Parent->imageWidth, output_cols * h_expand);

  inrow=0;
  for(outrow=0; outrow < compptr->vSampFactor; outrow++) {
    outptr = output_data[outrow];
    for(outcol=0, outcol_h = 0; outcol < output_cols;
			outcol++, outcol_h += h_expand) {
      outvalue = 0;
      for(v=0; v < v_expand; v++) {
				inptr = input_data[inrow+v] + outcol_h;
				for(h=0; h < h_expand; h++) {
					outvalue += (INT32) GETJSAMPLE(*inptr++);
					}
	      }
      *outptr++ = (JSAMPLE) ((outvalue + numpix2) / numpix);
		  }
    inrow += v_expand;
		}
	}


/*
 * Downsample pixel values of a single component.
 * This version handles the special case of a full-size component,
 * without smoothing.
 */

void CDownsampler::fullsizeDownsample(J_COMPONENT_INFO *compptr,
  JSAMPARRAY input_data, JSAMPARRAY output_data) {

  // Copy the data 
  CCompressJpeg::copySampleRows(input_data, 0, output_data, 0, m_Parent->max_V_SampFactor, m_Parent->imageWidth);
  // Edge-expand 
  expandRightEdge(output_data, m_Parent->max_V_SampFactor, m_Parent->imageWidth, compptr->widthInBlocks *DCTSIZE);
	}


/*
 * Downsample pixel values of a single component.
 * This version handles the common case of 2:1 horizontal and 1:1 vertical,
 * without smoothing.
 *
 * A note about the "bias" calculations: when rounding fractional values to
 * integer, we do not want to always round 0.5 up to the next integer.
 * If we did that, we'd introduce a noticeable bias towards larger values.
 * Instead, this code is arranged so that 0.5 will be rounded up or down at
 * alternate pixel locations (a simple ordered dither pattern).
 */

void CDownsampler::h2v1_downsample(J_COMPONENT_INFO * compptr,
	JSAMPARRAY input_data, JSAMPARRAY output_data) {
  int outrow;
  JDIMENSION outcol;
  JDIMENSION output_cols = compptr->widthInBlocks * DCTSIZE;
  register JSAMPROW inptr, outptr;
  register int bias;

  /* Expand input data enough to let all the output samples be generated
   * by the standard loop.  Special-casing padded output would be more
   * efficient.
   */
  expandRightEdge(input_data, m_Parent->max_V_SampFactor, m_Parent->imageWidth, output_cols * 2);

  for(outrow=0; outrow < compptr->vSampFactor; outrow++) {
    outptr = output_data[outrow];
    inptr = input_data[outrow];
    bias = 0;			/* bias = 0,1,0,1,... for successive samples */
    for(outcol=0; outcol < output_cols; outcol++) {
      *outptr++ = (JSAMPLE) ((GETJSAMPLE(*inptr) + GETJSAMPLE(inptr[1])
			+ bias) >> 1);
      bias ^= 1;		/* 0=>1, 1=>0 */
      inptr += 2;
			}
		}
	}


/*
 * Downsample pixel values of a single component.
 * This version handles the standard case of 2:1 horizontal and 2:1 vertical,
 * without smoothing.
 */

void CDownsampler::h2v2_downsample(J_COMPONENT_INFO *compptr,
	JSAMPARRAY input_data, JSAMPARRAY output_data) {
  int inrow, outrow;
  JDIMENSION outcol;
  JDIMENSION output_cols = compptr->widthInBlocks * DCTSIZE;
  register JSAMPROW inptr0, inptr1, outptr;
  register int bias;

  /* Expand input data enough to let all the output samples be generated
   * by the standard loop.  Special-casing padded output would be more
   * efficient.
   */
  expandRightEdge(input_data, m_Parent->max_V_SampFactor, m_Parent->imageWidth, output_cols * 2);

  inrow = 0;
  for(outrow=0; outrow < compptr->vSampFactor; outrow++) {
    outptr = output_data[outrow];
    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow+1];
    bias = 1;			// bias = 1,2,1,2,... for successive samples
    for(outcol=0; outcol < output_cols; outcol++) {
      *outptr++ = (JSAMPLE) ((GETJSAMPLE(*inptr0) + GETJSAMPLE(inptr0[1]) +
				GETJSAMPLE(*inptr1) + GETJSAMPLE(inptr1[1]) + bias) >> 2);
      bias ^= 3;		// 1=>2, 2=>1 
      inptr0 += 2; inptr1 += 2;
			}
		inrow += 2;
		}
	}


#ifdef INPUT_SMOOTHING_SUPPORTED

/*
 * Downsample pixel values of a single component.
 * This version handles the standard case of 2:1 horizontal and 2:1 vertical,
 * with smoothing.  One row of context is required.
 */

void CDownsampler::h2v2_smoothDownsample(J_COMPONENT_INFO *compptr,
	JSAMPARRAY input_data, JSAMPARRAY output_data) {
  int inrow, outrow;
  JDIMENSION colctr;
  JDIMENSION output_cols = compptr->widthInBlocks * DCTSIZE;
  register JSAMPROW inptr0, inptr1, above_ptr, below_ptr, outptr;
  INT32 membersum, neighsum, memberscale, neighscale;

  /* Expand input data enough to let all the output samples be generated
   * by the standard loop.  Special-casing padded output would be more
   * efficient.
   */
  expandRightEdge(input_data - 1, m_Parent->max_V_SampFactor + 2,
	  m_Parent->imageWidth, output_cols * 2);

  /* We don't bother to form the individual "smoothed" input pixel values;
   * we can directly compute the output which is the average of the four
   * smoothed values.  Each of the four member pixels contributes a fraction
   * (1-8*SF) to its own smoothed image and a fraction SF to each of the three
   * other smoothed pixels, therefore a total fraction (1-5*SF)/4 to the final
   * output.  The four corner-adjacent neighbor pixels contribute a fraction
   * SF to just one smoothed pixel, or SF/4 to the final output; while the
   * eight edge-adjacent neighbors contribute SF to each of two smoothed
   * pixels, or SF/2 overall.  In order to use integer arithmetic, these
   * factors are scaled by 2^16 = 65536.
   * Also recall that SF = smoothing_factor / 1024.
   */

  memberscale = 16384 - m_Parent->smoothingFactor * 80; // scaled (1-5*SF)/4
  neighscale = m_Parent->smoothingFactor * 16; // scaled SF/4

  inrow = 0;
  for(outrow=0; outrow < compptr->vSampFactor; outrow++) {
    outptr = output_data[outrow];
    inptr0 = input_data[inrow];
    inptr1 = input_data[inrow+1];
    above_ptr = input_data[inrow-1];
    below_ptr = input_data[inrow+2];

    // Special case for first column: pretend column -1 is same as column 0
    membersum = GETJSAMPLE(*inptr0) + GETJSAMPLE(inptr0[1]) +
		GETJSAMPLE(*inptr1) + GETJSAMPLE(inptr1[1]);
    neighsum = GETJSAMPLE(*above_ptr) + GETJSAMPLE(above_ptr[1]) +
			GETJSAMPLE(*below_ptr) + GETJSAMPLE(below_ptr[1]) +
			GETJSAMPLE(*inptr0) + GETJSAMPLE(inptr0[2]) +
			GETJSAMPLE(*inptr1) + GETJSAMPLE(inptr1[2]);
    neighsum += neighsum;
    neighsum += GETJSAMPLE(*above_ptr) + GETJSAMPLE(above_ptr[2]) +
			GETJSAMPLE(*below_ptr) + GETJSAMPLE(below_ptr[2]);
    membersum = membersum * memberscale + neighsum * neighscale;
    *outptr++ = (JSAMPLE) ((membersum + 32768) >> 16);
    inptr0 += 2; inptr1 += 2; above_ptr += 2; below_ptr += 2;

    for(colctr=output_cols-2; colctr > 0; colctr--) {
      // sum of pixels directly mapped to this output element
      membersum = GETJSAMPLE(*inptr0) + GETJSAMPLE(inptr0[1]) +
		  GETJSAMPLE(*inptr1) + GETJSAMPLE(inptr1[1]);
      // sum of edge-neighbor pixels
      neighsum = GETJSAMPLE(*above_ptr) + GETJSAMPLE(above_ptr[1]) +
				GETJSAMPLE(*below_ptr) + GETJSAMPLE(below_ptr[1]) +
				GETJSAMPLE(inptr0[-1]) + GETJSAMPLE(inptr0[2]) +
				GETJSAMPLE(inptr1[-1]) + GETJSAMPLE(inptr1[2]);
      // The edge-neighbors count twice as much as corner-neighbors
      neighsum += neighsum;
      // Add in the corner-neighbors
      neighsum += GETJSAMPLE(above_ptr[-1]) + GETJSAMPLE(above_ptr[2]) +
		  GETJSAMPLE(below_ptr[-1]) + GETJSAMPLE(below_ptr[2]);
      // form final output scaled up by 2^16
      membersum = membersum * memberscale + neighsum * neighscale;
      // round, descale and output it
      *outptr++ = (JSAMPLE) ((membersum + 32768) >> 16);
      inptr0 += 2; inptr1 += 2; above_ptr += 2; below_ptr += 2;
			}

    // Special case for last column
    membersum = GETJSAMPLE(*inptr0) + GETJSAMPLE(inptr0[1]) +
		GETJSAMPLE(*inptr1) + GETJSAMPLE(inptr1[1]);
    neighsum = GETJSAMPLE(*above_ptr) + GETJSAMPLE(above_ptr[1]) +
			GETJSAMPLE(*below_ptr) + GETJSAMPLE(below_ptr[1]) +
			GETJSAMPLE(inptr0[-1]) + GETJSAMPLE(inptr0[1]) +
			GETJSAMPLE(inptr1[-1]) + GETJSAMPLE(inptr1[1]);
    neighsum += neighsum;
    neighsum += GETJSAMPLE(above_ptr[-1]) + GETJSAMPLE(above_ptr[1]) +
			GETJSAMPLE(below_ptr[-1]) + GETJSAMPLE(below_ptr[1]);
    membersum = membersum * memberscale + neighsum * neighscale;
    *outptr=(JSAMPLE)((membersum + 32768) >> 16);

    inrow += 2;
		}
	}


/*
 * Downsample pixel values of a single component.
 * This version handles the special case of a full-size component,
 * with smoothing.  One row of context is required.
 */

void CDownsampler::fullsizeSmoothDownsample(J_COMPONENT_INFO *compptr,
	JSAMPARRAY input_data, JSAMPARRAY output_data) {
  int outrow;
  JDIMENSION colctr;
  JDIMENSION output_cols = compptr->widthInBlocks * DCTSIZE;
  register JSAMPROW inptr, above_ptr, below_ptr, outptr;
  INT32 membersum, neighsum, memberscale, neighscale;
  int colsum, lastcolsum, nextcolsum;

  /* Expand input data enough to let all the output samples be generated
   * by the standard loop.  Special-casing padded output would be more
   * efficient.
   */
  expandRightEdge(input_data - 1, m_Parent->max_V_SampFactor + 2,
		m_Parent->imageWidth, output_cols);

  /* Each of the eight neighbor pixels contributes a fraction SF to the
   * smoothed pixel, while the main pixel contributes (1-8*SF).  In order
   * to use integer arithmetic, these factors are multiplied by 2^16 = 65536.
   * Also recall that SF = smoothing_factor / 1024.
   */

  memberscale = 65536L - m_Parent->smoothingFactor * 512L; // scaled 1-8*SF
  neighscale = m_Parent->smoothingFactor * 64; // scaled SF

  for(outrow=0; outrow < compptr->vSampFactor; outrow++) {
    outptr = output_data[outrow];
    inptr = input_data[outrow];
    above_ptr = input_data[outrow-1];
    below_ptr = input_data[outrow+1];

    // Special case for first column
    colsum=GETJSAMPLE(*above_ptr++)+GETJSAMPLE(*below_ptr++)+
			GETJSAMPLE(*inptr);
    membersum = GETJSAMPLE(*inptr++);
    nextcolsum = GETJSAMPLE(*above_ptr) + GETJSAMPLE(*below_ptr) +
			GETJSAMPLE(*inptr);
    neighsum = colsum + (colsum - membersum) + nextcolsum;
    membersum = membersum * memberscale + neighsum * neighscale;
    *outptr++ = (JSAMPLE) ((membersum + 32768) >> 16);
    lastcolsum = colsum; colsum = nextcolsum;

    for(colctr = output_cols - 2; colctr > 0; colctr--) {
      membersum = GETJSAMPLE(*inptr++);
      above_ptr++; below_ptr++;
      nextcolsum = GETJSAMPLE(*above_ptr) + GETJSAMPLE(*below_ptr) +
		   GETJSAMPLE(*inptr);
      neighsum = lastcolsum + (colsum - membersum) + nextcolsum;
      membersum = membersum * memberscale + neighsum * neighscale;
      *outptr++ = (JSAMPLE) ((membersum + 32768) >> 16);
      lastcolsum = colsum; colsum = nextcolsum;
			}

    // Special case for last column
    membersum = GETJSAMPLE(*inptr);
    neighsum = lastcolsum + (colsum - membersum) + colsum;
    membersum = membersum * memberscale + neighsum * neighscale;
    *outptr = (JSAMPLE) ((membersum + 32768) >> 16);
		}
	}

#endif // INPUT_SMOOTHING_SUPPORTED 


/*
 * Module initialization routine for downsampling.
 * Note that we must select a routine for each component.
 */

CDownsampler::CDownsampler(CCompressJpeg *p) : m_Parent(p) {
  int ci;
  J_COMPONENT_INFO *compptr;
  BOOL smoothok=TRUE;

  needContextRows = FALSE;

  doDownsample=sepDownsample;
  if(p->CCIR601Sampling)
    p->m_Parent->err->errorExit(JERR_CCIR601_NOTIMPL);

  // Verify we can handle the sampling factors, and set up method pointers 
  for(ci=0, compptr=p->compInfo; ci < p->numComponents; ci++, compptr++) {
    if(compptr->hSampFactor == p->max_H_SampFactor &&
			compptr->vSampFactor == p->max_V_SampFactor) {
#ifdef INPUT_SMOOTHING_SUPPORTED
      if(p->smoothingFactor) {
				methods[ci] = fullsizeSmoothDownsample;
				needContextRows = TRUE;
				} 
			else
#endif
				methods[ci] = fullsizeDownsample;
			} 
		else if(compptr->hSampFactor * 2 == p->max_H_SampFactor &&
			compptr->vSampFactor == p->max_V_SampFactor) {
      smoothok = FALSE;
      methods[ci] = h2v1_downsample;
			} 
		else if(compptr->hSampFactor * 2 == p->max_H_SampFactor &&
	      compptr->vSampFactor * 2 == p->max_V_SampFactor) {
#ifdef INPUT_SMOOTHING_SUPPORTED
      if(m_Parent->smoothingFactor) {
				methods[ci] = h2v2_smoothDownsample;
				needContextRows = TRUE;
				} 
			else
#endif
			methods[ci] = h2v2_downsample;
			} 
		else if((p->max_H_SampFactor % compptr->hSampFactor) == 0 &&
				(p->max_V_SampFactor % compptr->vSampFactor) == 0) {
      smoothok = FALSE;
      methods[ci] = intDownsample;
			} 
		else
      p->m_Parent->err->errorExit(JERR_FRACT_SAMPLE_NOTIMPL);
		}

#ifdef INPUT_SMOOTHING_SUPPORTED
  if(m_Parent->smoothingFactor && !smoothok)
    p->m_Parent->err->trace(0, JTRC_SMOOTH_NOTIMPL);
#endif
	
	}






/*
 * Error exit handler: must not return to caller.
 *
 * Applications may override this if they want to get control back after
 * an error.  Typically one would longjmp somewhere instead of exiting.
 * The setjmp buffer can be made a private field within an expanded error
 * handler object.  Note that the info needed to generate an error message
 * is stored in the error object, so you can generate the message now or
 * later, at your convenience.
 * You should make sure that the JPEG object is cleaned up (with jpeg_abort
 * or jpeg_destroy) at some point.
 */

#define JMESSAGE(a,b) b,

const char *CErrorMgr::stdMessageTable[150] = { "messaggi non disponibili" ,
/* For maintenance convenience, list is alphabetical by message code name 
	dall'H non gli piaceva... boh*/
JMESSAGE(JERR_ARITH_NOTIMPL,
	 "Sorry, there are legal restrictions on arithmetic coding")
JMESSAGE(JERR_BAD_ALIGN_TYPE, "ALIGN_TYPE is wrong, please fix")
JMESSAGE(JERR_BAD_ALLOC_CHUNK, "MAX_ALLOC_CHUNK is wrong, please fix")
JMESSAGE(JERR_BAD_BUFFER_MODE, "Bogus buffer control mode")
JMESSAGE(JERR_BAD_COMPONENT_ID, "Invalid component ID %d in SOS")
JMESSAGE(JERR_BAD_DCT_COEF, "DCT coefficient out of range")
JMESSAGE(JERR_BAD_DCTSIZE, "IDCT output block size %d not supported")
JMESSAGE(JERR_BAD_HUFF_TABLE, "Bogus Huffman table definition")
JMESSAGE(JERR_BAD_IN_COLORSPACE, "Bogus input colorspace")
JMESSAGE(JERR_BAD_J_COLORSPACE, "Bogus JPEG colorspace")
JMESSAGE(JERR_BAD_LENGTH, "Bogus marker length")
JMESSAGE(JERR_BAD_LIB_VERSION,
	 "Wrong JPEG library version: library is %d, caller expects %d")
JMESSAGE(JERR_BAD_MCU_SIZE, "Sampling factors too large for interleaved scan")
JMESSAGE(JERR_BAD_POOL_ID, "Invalid memory pool code %d")
JMESSAGE(JERR_BAD_PRECISION, "Unsupported JPEG data precision %d")
JMESSAGE(JERR_BAD_PROGRESSION,
	 "Invalid progressive parameters Ss=%d Se=%d Ah=%d Al=%d")
JMESSAGE(JERR_BAD_PROG_SCRIPT,
	 "Invalid progressive parameters at scan script entry %d")
JMESSAGE(JERR_BAD_SAMPLING, "Bogus sampling factors")
JMESSAGE(JERR_BAD_SCAN_SCRIPT, "Invalid scan script at entry %d")
JMESSAGE(JERR_BAD_STATE, "Improper call to JPEG library in state %d")
JMESSAGE(JERR_BAD_STRUCT_SIZE,
	 "JPEG parameter struct mismatch: library thinks size is %u, caller expects %u")
JMESSAGE(JERR_BAD_VIRTUAL_ACCESS, "Bogus virtual array access")
JMESSAGE(JERR_BUFFER_SIZE, "Buffer passed to JPEG library is too small")
JMESSAGE(JERR_CANT_SUSPEND, "Suspension not allowed here")
JMESSAGE(JERR_CCIR601_NOTIMPL, "CCIR601 sampling not implemented yet")
JMESSAGE(JERR_COMPONENT_COUNT, "Too many color components: %d, max %d")
JMESSAGE(JERR_CONVERSION_NOTIMPL, "Unsupported color conversion request")
JMESSAGE(JERR_DAC_INDEX, "Bogus DAC index %d")
JMESSAGE(JERR_DAC_VALUE, "Bogus DAC value 0x%x")
JMESSAGE(JERR_DHT_COUNTS, "Bogus DHT counts")
JMESSAGE(JERR_DHT_INDEX, "Bogus DHT index %d")
JMESSAGE(JERR_DQT_INDEX, "Bogus DQT index %d")
JMESSAGE(JERR_EMPTY_IMAGE, "Empty JPEG image (DNL not supported)")
JMESSAGE(JERR_EMS_READ, "Read from EMS failed")
JMESSAGE(JERR_EMS_WRITE, "Write to EMS failed")
JMESSAGE(JERR_EOI_EXPECTED, "Didn't expect more than one scan")
JMESSAGE(JERR_FILE_READ, "Input file read error")
JMESSAGE(JERR_FILE_WRITE, "Output file write error --- out of disk space?")
JMESSAGE(JERR_FRACT_SAMPLE_NOTIMPL, "Fractional sampling not implemented yet")
JMESSAGE(JERR_HUFF_CLEN_OVERFLOW, "Huffman code size table overflow")
JMESSAGE(JERR_HUFF_MISSING_CODE, "Missing Huffman code table entry")
JMESSAGE(JERR_IMAGE_TOO_BIG, "Maximum supported image dimension is %u pixels")
JMESSAGE(JERR_INPUT_EMPTY, "Empty input file")
JMESSAGE(JERR_INPUT_EOF, "Premature end of input file")
JMESSAGE(JERR_MISMATCHED_QUANT_TABLE,
	 "Cannot transcode due to multiple use of quantization table %d")
JMESSAGE(JERR_MISSING_DATA, "Scan script does not transmit all data")
JMESSAGE(JERR_MODE_CHANGE, "Invalid color quantization mode change")
JMESSAGE(JERR_NOTIMPL, "Not implemented yet")
JMESSAGE(JERR_NOT_COMPILED, "Requested feature was omitted at compile time")
JMESSAGE(JERR_NO_BACKING_STORE, "Backing store not supported")
JMESSAGE(JERR_NO_HUFF_TABLE, "Huffman table 0x%02x was not defined")
JMESSAGE(JERR_NO_IMAGE, "JPEG datastream contains no image")
JMESSAGE(JERR_NO_QUANT_TABLE, "Quantization table 0x%02x was not defined")
JMESSAGE(JERR_NO_SOI, "Not a JPEG file: starts with 0x%02x 0x%02x")
JMESSAGE(JERR_OUT_OF_MEMORY, "Insufficient memory (case %d)")
JMESSAGE(JERR_QUANT_COMPONENTS,
	 "Cannot quantize more than %d color components")
JMESSAGE(JERR_QUANT_FEW_COLORS, "Cannot quantize to fewer than %d colors")
JMESSAGE(JERR_QUANT_MANY_COLORS, "Cannot quantize to more than %d colors")
JMESSAGE(JERR_SOF_DUPLICATE, "Invalid JPEG file structure: two SOF markers")
JMESSAGE(JERR_SOF_NO_SOS, "Invalid JPEG file structure: missing SOS marker")
JMESSAGE(JERR_SOF_UNSUPPORTED, "Unsupported JPEG process: SOF type 0x%02x")
JMESSAGE(JERR_SOI_DUPLICATE, "Invalid JPEG file structure: two SOI markers")
JMESSAGE(JERR_SOS_NO_SOF, "Invalid JPEG file structure: SOS before SOF")
JMESSAGE(JERR_TFILE_CREATE, "Failed to create temporary file %s")
JMESSAGE(JERR_TFILE_READ, "Read failed on temporary file")
JMESSAGE(JERR_TFILE_SEEK, "Seek failed on temporary file")
JMESSAGE(JERR_TFILE_WRITE,
	 "Write failed on temporary file --- out of disk space?")
JMESSAGE(JERR_TOO_LITTLE_DATA, "Application transferred too few scanlines")
JMESSAGE(JERR_UNKNOWN_MARKER, "Unsupported marker type 0x%02x")
JMESSAGE(JERR_VIRTUAL_BUG, "Virtual array controller messed up")
JMESSAGE(JERR_WIDTH_OVERFLOW, "Image too wide for this implementation")
JMESSAGE(JERR_XMS_READ, "Read from XMS failed")
JMESSAGE(JERR_XMS_WRITE, "Write to XMS failed")
JMESSAGE(JMSG_COPYRIGHT, JCOPYRIGHT)
JMESSAGE(JMSG_VERSION, JVERSION)
JMESSAGE(JTRC_16BIT_TABLES,
	 "Caution: quantization tables are too coarse for baseline JPEG")
JMESSAGE(JTRC_ADOBE,
	 "Adobe APP14 marker: version %d, flags 0x%04x 0x%04x, transform %d")
JMESSAGE(JTRC_APP0, "Unknown APP0 marker (not JFIF), length %u")
JMESSAGE(JTRC_APP14, "Unknown APP14 marker (not Adobe), length %u")
JMESSAGE(JTRC_DAC, "Define Arithmetic Table 0x%02x: 0x%02x")
JMESSAGE(JTRC_DHT, "Define Huffman Table 0x%02x")
JMESSAGE(JTRC_DQT, "Define Quantization Table %d  precision %d")
JMESSAGE(JTRC_DRI, "Define Restart Interval %u")
JMESSAGE(JTRC_EMS_CLOSE, "Freed EMS handle %u")
JMESSAGE(JTRC_EMS_OPEN, "Obtained EMS handle %u")
JMESSAGE(JTRC_EOI, "End Of Image")
JMESSAGE(JTRC_HUFFBITS, "        %3d %3d %3d %3d %3d %3d %3d %3d")
JMESSAGE(JTRC_JFIF, "JFIF APP0 marker: version %d.%02d, density %dx%d  %d")
JMESSAGE(JTRC_JFIF_BADTHUMBNAILSIZE,
	 "Warning: thumbnail image size does not match data length %u")
JMESSAGE(JTRC_JFIF_EXTENSION,
	 "JFIF extension marker: type 0x%02x, length %u")
JMESSAGE(JTRC_JFIF_THUMBNAIL, "    with %d x %d thumbnail image")
JMESSAGE(JTRC_MISC_MARKER, "Miscellaneous marker 0x%02x, length %u")
JMESSAGE(JTRC_PARMLESS_MARKER, "Unexpected marker 0x%02x")
JMESSAGE(JTRC_QUANTVALS, "        %4u %4u %4u %4u %4u %4u %4u %4u")
JMESSAGE(JTRC_QUANT_3_NCOLORS, "Quantizing to %d = %d*%d*%d colors")
JMESSAGE(JTRC_QUANT_NCOLORS, "Quantizing to %d colors")
JMESSAGE(JTRC_QUANT_SELECTED, "Selected %d colors for quantization")
JMESSAGE(JTRC_RECOVERY_ACTION, "At marker 0x%02x, recovery action %d")
JMESSAGE(JTRC_RST, "RST%d")
JMESSAGE(JTRC_SMOOTH_NOTIMPL,
	 "Smoothing not supported with nonstandard sampling ratios")
JMESSAGE(JTRC_SOF, "Start Of Frame 0x%02x: width=%u, height=%u, components=%d")
JMESSAGE(JTRC_SOF_COMPONENT, "    Component %d: %dhx%dv q=%d")
JMESSAGE(JTRC_SOI, "Start of Image")
JMESSAGE(JTRC_SOS, "Start Of Scan: %d components")
JMESSAGE(JTRC_SOS_COMPONENT, "    Component %d: dc=%d ac=%d")
JMESSAGE(JTRC_SOS_PARAMS, "  Ss=%d, Se=%d, Ah=%d, Al=%d")
JMESSAGE(JTRC_TFILE_CLOSE, "Closed temporary file %s")
JMESSAGE(JTRC_TFILE_OPEN, "Opened temporary file %s")
JMESSAGE(JTRC_THUMB_JPEG,
	 "JFIF extension marker: JPEG-compressed thumbnail image, length %u")
JMESSAGE(JTRC_THUMB_PALETTE,
	 "JFIF extension marker: palette thumbnail image, length %u")
JMESSAGE(JTRC_THUMB_RGB,
	 "JFIF extension marker: RGB thumbnail image, length %u")
JMESSAGE(JTRC_UNKNOWN_IDS, "Unrecognized component IDs %d %d %d, assuming YCbCr")
JMESSAGE(JTRC_XMS_CLOSE, "Freed XMS handle %u")
JMESSAGE(JTRC_XMS_OPEN, "Obtained XMS handle %u")
JMESSAGE(JWRN_ADOBE_XFORM, "Unknown Adobe color transform code %d")
JMESSAGE(JWRN_BOGUS_PROGRESSION,
	 "Inconsistent progression sequence for component %d coefficient %d")
JMESSAGE(JWRN_EXTRANEOUS_DATA,
	 "Corrupt JPEG data: %u extraneous bytes before marker 0x%02x")
JMESSAGE(JWRN_HIT_MARKER, "Corrupt JPEG data: premature end of data segment")
JMESSAGE(JWRN_HUFF_BAD_CODE, "Corrupt JPEG data: bad Huffman code")
JMESSAGE(JWRN_JFIF_MAJOR, "Warning: unknown JFIF revision number %d.%02d")
JMESSAGE(JWRN_JPEG_EOF, "Premature end of JPEG file")
JMESSAGE(JWRN_MUST_RESYNC, "Corrupt JPEG data: found marker 0x%02x instead of RST%d")
JMESSAGE(JWRN_NOT_SEQUENTIAL, "Invalid SOS parameters for sequential JPEG")
JMESSAGE(JWRN_TOO_MUCH_DATA, "Application transferred too many scanlines")
	};

void CErrorMgr::errorExit() {
  
	// Always display the message 
  outputMessage();

  // Let the memory manager delete any temp files before we die
  ((CJpeg *)m_Parent)->destroy();
//	jpeg_abort(); jpeg_destroy();

//  exit(EXIT_FAILURE);
	longjmp(exitEnv,EXIT_FAILURE);
	}


/*
 * Actual output of an error or trace message.
 * Applications may override this method to send JPEG messages somewhere
 * other than stderr.
 *
 * On Windows, printing to stderr is generally completely useless,
 * so we provide optional code to produce an error-dialog popup.
 * Most Windows applications will still prefer to override this routine,
 * but if they don't, it'll do something at least marginally useful.
 *
 * NOTE: to use the library in an environment that doesn't support the
 * C stdio library, you may have to delete the call to fprintf() entirely,
 * not just not use this routine.
 */

void CErrorMgr::outputMessage() {
  char buffer[JMSG_LENGTH_MAX];

  // Create the message 
  formatMessage(buffer);
//	wsprintf(buffer,"Messaggi non disponibili");

  MessageBox(GetActiveWindow(), buffer, "JPEG Library Error",
	  MB_OK | MB_ICONERROR);

//  fprintf(stderr, "%s\n", buffer);
	}


/*
 * Decide whether to emit a trace or warning message.
 * msg_level is one of:
 *   -1: recoverable corrupt-data warning, may want to abort.
 *    0: important advisory messages (always display to user).
 *    1: first level of tracing detail.
 *    2,3,...: successively more detailed tracing messages.
 * An application might override this method if it wanted to abort on warnings
 * or change the policy about which messages to display.
 */

void CErrorMgr::emitMessage(int msgLevel) {

  if(msgLevel < 0) {
    /* It's a warning message.  Since corrupt files may generate many warnings,
     * the policy implemented here is to show only the first warning,
     * unless trace_level >= 3.
     */
    if(numWarnings == 0 || traceLevel >= 3)
      outputMessage();
    // Always count warnings in num_warnings.
    numWarnings++;
		} 
	else {
    // It's a trace message.  Show it if trace_level >= msg_level.
    if(traceLevel >= msgLevel)
      outputMessage();
		}
	}


/*
 * Format a message string for the most recent JPEG error or message.
 * The message is stored into buffer, which should be at least JMSG_LENGTH_MAX
 * characters.  Note that no '\n' character is added to the string.
 * Few applications should need to override this method.
 */

void CErrorMgr::formatMessage(char *buffer) {
  const char *msgtext = NULL;
  const char *msgptr;
  char ch;
  BOOL isstring;
	char buf[16],buf2[128];

 ASSERT(0);
 
 // Look up message string in proper table 
  if(msgCode > 0 && msgCode <= lastMessage) {

		if(1) {		// i messaggi non ci sono... quindi:
			msgtext=messageTable[msgCode];
			}
		else {
			_tcscpy(buf2,messageTable[0]);
			wsprintf(buf," %u",msgCode);
			_tcscat(buf2,buf);
			msgtext=buf2;
			}
		} 
	else if(addOnMessageTable != NULL &&
		msgCode >= firstAddonMessage &&	msgCode <= lastAddonMessage) {
    msgtext=addOnMessageTable[msgCode-firstAddonMessage];
		}

  // Defend against bogus message number
  if(msgtext == NULL) {
    msgParm.i[0] = msgCode;
    msgtext = messageTable[0];
		}

  // Check for string parameter, as indicated by %s in the message text
  isstring = FALSE;
  msgptr = msgtext;
  while((ch = *msgptr++) != '\0') {
    if(ch == '%') {
      if(*msgptr == 's')
				isstring = TRUE;
      break;
			}
		}

  // Format the message into the passed buffer 
  if(isstring)
    wsprintf(buffer, msgtext, msgParm.s);
  else
    wsprintf(buffer,msgtext, msgParm.i[0], msgParm.i[1],
	    msgParm.i[2], msgParm.i[3], msgParm.i[4], msgParm.i[5],
	    msgParm.i[6], msgParm.i[7]);
	}


/*
 * Reset error state variables at start of a new image.
 * This is called during compression startup to reset trace/error
 * processing to default state, without losing any application-specific
 * method pointers.  An application might possibly want to override
 * this method if it has additional error processing state.
 */

void CErrorMgr::resetErrorMgr() {
  
	numWarnings = 0;
  // trace_level is not reset since it is an application-supplied parameter
  msgCode = 0;	// may be useful as a flag for "no error"
	}


/*
 * Fill in the standard error-handling methods in a CErrorMgr object.
 * Typical call is:
 *	struct CCompressJpeg cinfo;
 *	struct CErrorMgr err;
 *
 *	cinfo.err = jpeg_std_error(&err);
 * after which the application may override some of the methods.
 */

CErrorMgr::CErrorMgr(const CJpeg *p) : m_Parent(p) {


// Default error-management setup
  traceLevel = 0;		// default = no tracing
  numWarnings = 0;		// no warnings emitted yet
  msgCode = 0;				// may be useful as a flag for "no error"

  // Initialize message table pointers
  messageTable=stdMessageTable;
  lastMessage = (int)JMSG_LASTMSGCODE - 1;

  addOnMessageTable = NULL;
  firstAddonMessage = 0;	// for safety
  lastAddonMessage = 0;
	}





/*
 * jfdctflt.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains a floating-point implementation of the
 * forward DCT (Discrete Cosine Transform).
 *
 * This implementation should be more accurate than either of the integer
 * DCT implementations.  However, it may not give the same results on all
 * machines because of differences in roundoff behavior.  Speed will depend
 * on the hardware's floating point capacity.
 *
 * A 2-D DCT can be done by 1-D DCT on each row followed by 1-D DCT
 * on each column.  Direct algorithms are also available, but they are
 * much more complex and seem not to be any faster when reduced to code.
 *
 * This implementation is based on Arai, Agui, and Nakajima's algorithm for
 * scaled DCT.  Their original paper (Trans. IEICE E-71(11):1095) is in
 * Japanese, but the algorithm is described in the Pennebaker & Mitchell
 * JPEG textbook (see REFERENCES section in file README).  The following code
 * is based directly on figure 4-8 in P&M.
 * While an 8-point DCT cannot be done in less than 11 multiplies, it is
 * possible to arrange the computation so that many of the multiplies are
 * simple scalings of the final outputs.  These multiplies can then be
 * folded into the multiplications or divisions by the JPEG quantization
 * table entries.  The AA&N method leaves only 5 multiplies and 29 adds
 * to be done in the DCT itself.
 * The primary disadvantage of this method is that with a fixed-point
 * implementation, accuracy is lost due to imprecise representation of the
 * scaled quantization values.  However, that problem does not arise if
 * we use floating point arithmetic.
 */


#ifdef DCT_FLOAT_SUPPORTED


/*
 * This module is specialized to the case DCTSIZE = 8.
 */

#if DCTSIZE != 8
  Sorry, this code only copes with 8x8 DCTs. /* deliberate syntax err */
#endif


/*
 * Perform the forward DCT on one block of samples.
 */

void CForwardDCT::fdctFloat(double *data) {
  double tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  double tmp10, tmp11, tmp12, tmp13;
  double z1, z2, z3, z4, z5, z11, z13;
  double *dataptr;
  int ctr;

  // Pass 1: process rows. 

  dataptr = data;
  for(ctr = DCTSIZE-1; ctr >= 0; ctr--) {
    tmp0 = dataptr[0] + dataptr[7];
    tmp7 = dataptr[0] - dataptr[7];
    tmp1 = dataptr[1] + dataptr[6];
    tmp6 = dataptr[1] - dataptr[6];
    tmp2 = dataptr[2] + dataptr[5];
    tmp5 = dataptr[2] - dataptr[5];
    tmp3 = dataptr[3] + dataptr[4];
    tmp4 = dataptr[3] - dataptr[4];
    
    // Even part
    
    tmp10 = tmp0 + tmp3;	// phase 2
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;
    
    dataptr[0] = tmp10 + tmp11; // phase 3
    dataptr[4] = tmp10 - tmp11;
    
    z1 = (tmp12 + tmp13) * ((double) 0.707106781); // c4
    dataptr[2] = tmp13 + z1;	// phase 5
    dataptr[6] = tmp13 - z1;
    
    // Odd part
    tmp10 = tmp4 + tmp5;	// phase 2
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    // The rotator is modified from fig 4-8 to avoid extra negations.
    z5 = (tmp10 - tmp12) * ((double) 0.382683433); // c6
    z2 = ((double) 0.541196100) * tmp10 + z5; // c2-c6
    z4 = ((double) 1.306562965) * tmp12 + z5; // c2+c6
    z3 = tmp11 * ((double) 0.707106781);			// c4

    z11 = tmp7 + z3;		// phase 5
    z13 = tmp7 - z3;

    dataptr[5] = z13 + z2;	// phase 6
    dataptr[3] = z13 - z2;
    dataptr[1] = z11 + z4;
    dataptr[7] = z11 - z4;

    dataptr += DCTSIZE;		// advance pointer to next row
		}

  // Pass 2: process columns.

  dataptr = data;
  for(ctr = DCTSIZE-1; ctr >= 0; ctr--) {
    tmp0 = dataptr[DCTSIZE*0] + dataptr[DCTSIZE*7];
    tmp7 = dataptr[DCTSIZE*0] - dataptr[DCTSIZE*7];
    tmp1 = dataptr[DCTSIZE*1] + dataptr[DCTSIZE*6];
    tmp6 = dataptr[DCTSIZE*1] - dataptr[DCTSIZE*6];
    tmp2 = dataptr[DCTSIZE*2] + dataptr[DCTSIZE*5];
    tmp5 = dataptr[DCTSIZE*2] - dataptr[DCTSIZE*5];
    tmp3 = dataptr[DCTSIZE*3] + dataptr[DCTSIZE*4];
    tmp4 = dataptr[DCTSIZE*3] - dataptr[DCTSIZE*4];
    
    // Even part
    
    tmp10 = tmp0 + tmp3;	// phase 2
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;
    
    dataptr[DCTSIZE*0] = tmp10 + tmp11; /* phase 3 */
    dataptr[DCTSIZE*4] = tmp10 - tmp11;
    
    z1 = (tmp12 + tmp13) * ((double) 0.707106781); /* c4 */
    dataptr[DCTSIZE*2] = tmp13 + z1; /* phase 5 */
    dataptr[DCTSIZE*6] = tmp13 - z1;
    
    // Odd part
    tmp10 = tmp4 + tmp5;	// phase 2
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    // The rotator is modified from fig 4-8 to avoid extra negations.
    z5 = (tmp10 - tmp12) * ((double) 0.382683433); // c6
    z2 = ((double) 0.541196100) * tmp10 + z5; // c2-c6
    z4 = ((double) 1.306562965) * tmp12 + z5; // c2+c6
    z3 = tmp11 * ((double) 0.707106781);			// c4

    z11 = tmp7 + z3;		// phase 5
    z13 = tmp7 - z3;

    dataptr[DCTSIZE*5] = z13 + z2; // phase 6
    dataptr[DCTSIZE*3] = z13 - z2;
    dataptr[DCTSIZE*1] = z11 + z4;
    dataptr[DCTSIZE*7] = z11 - z4;

    dataptr++;			// advance pointer to next column
		}
	}

#endif // DCT_FLOAT_SUPPORTED





/*
 * jfdctfst.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains a fast, not so accurate integer implementation of the
 * forward DCT (Discrete Cosine Transform).
 *
 * A 2-D DCT can be done by 1-D DCT on each row followed by 1-D DCT
 * on each column.  Direct algorithms are also available, but they are
 * much more complex and seem not to be any faster when reduced to code.
 *
 * This implementation is based on Arai, Agui, and Nakajima's algorithm for
 * scaled DCT.  Their original paper (Trans. IEICE E-71(11):1095) is in
 * Japanese, but the algorithm is described in the Pennebaker & Mitchell
 * JPEG textbook (see REFERENCES section in file README).  The following code
 * is based directly on figure 4-8 in P&M.
 * While an 8-point DCT cannot be done in less than 11 multiplies, it is
 * possible to arrange the computation so that many of the multiplies are
 * simple scalings of the final outputs.  These multiplies can then be
 * folded into the multiplications or divisions by the JPEG quantization
 * table entries.  The AA&N method leaves only 5 multiplies and 29 adds
 * to be done in the DCT itself.
 * The primary disadvantage of this method is that with fixed-point math,
 * accuracy is lost due to imprecise representation of the scaled
 * quantization values.  The smaller the quantization table entry, the less
 * precise the scaled value, so this implementation does worse with high-
 * quality-setting files than with low-quality ones.
 */


#ifdef DCT_IFAST_SUPPORTED

/*
 * This module is specialized to the case DCTSIZE = 8.
 */

#if DCTSIZE != 8
  Sorry, this code only copes with 8x8 DCTs. /* deliberate syntax err */
#endif


/* Scaling decisions are generally the same as in the LL&M algorithm;
 * see jfdctint.c for more details.  However, we choose to descale
 * (right shift) multiplication products as soon as they are formed,
 * rather than carrying additional fractional bits into subsequent additions.
 * This compromises accuracy slightly, but it lets us save a few shifts.
 * More importantly, 16-bit arithmetic is then adequate (for 8-bit samples)
 * everywhere except in the multiplications proper; this saves a good deal
 * of work on 16-bit-int machines.
 *
 * Again to save a few shifts, the intermediate results between pass 1 and
 * pass 2 are not upscaled, but are represented only to integral precision.
 *
 * A final compromise is to represent the multiplicative constants to only
 * 8 fractional bits, rather than 13.  This saves some shifting work on some
 * machines, and may also reduce the cost of multiplication (since there
 * are fewer one-bits in the constants).
 */

#define CONST_BITS  8


/* Some C compilers fail to reduce "FIX(constant)" at compile time, thus
 * causing a lot of useless floating-point operations at run time.
 * To get around this we use the following pre-calculated constants.
 * If you change CONST_BITS you may want to add appropriate values.
 * (With a reasonable C compiler, you can just rely on the FIX() macro...)
 */

#if CONST_BITS == 8
#define FIX_0_382683433  ((INT32)   98)		/* FIX(0.382683433) */
#define FIX_0_541196100  ((INT32)  139)		/* FIX(0.541196100) */
#define FIX_0_707106781  ((INT32)  181)		/* FIX(0.707106781) */
#define FIX_1_306562965  ((INT32)  334)		/* FIX(1.306562965) */
#else
#define FIX_0_382683433  FIX(0.382683433)
#define FIX_0_541196100  FIX(0.541196100)
#define FIX_0_707106781  FIX(0.707106781)
#define FIX_1_306562965  FIX(1.306562965)
#endif


/* We can gain a little more speed, with a further compromise in accuracy,
 * by omitting the addition in a descaling shift.  This yields an incorrectly
 * rounded result half the time...
 */

#ifndef USE_ACCURATE_ROUNDING
#undef DESCALE
#define DESCALE(x,n)  RIGHT_SHIFT(x, n)
#endif


/* Multiply a DCTELEM variable by an INT32 constant, and immediately
 * descale to yield a DCTELEM result.
 */

#define MULTIPLY(var,const)  ((DCTELEM) DESCALE((var) * (const), CONST_BITS))


/*
 * Perform the forward DCT on one block of samples.
 */

void CForwardDCT::fdctIFast(DCTELEM *data) {
  DCTELEM tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  DCTELEM tmp10, tmp11, tmp12, tmp13;
  DCTELEM z1, z2, z3, z4, z5, z11, z13;
  DCTELEM *dataptr;
  int ctr;
  SHIFT_TEMPS

  // Pass 1: process rows.

  dataptr = data;
  for(ctr=DCTSIZE-1; ctr >= 0; ctr--) {
    tmp0 = dataptr[0] + dataptr[7];
    tmp7 = dataptr[0] - dataptr[7];
    tmp1 = dataptr[1] + dataptr[6];
    tmp6 = dataptr[1] - dataptr[6];
    tmp2 = dataptr[2] + dataptr[5];
    tmp5 = dataptr[2] - dataptr[5];
    tmp3 = dataptr[3] + dataptr[4];
    tmp4 = dataptr[3] - dataptr[4];
    
    // Even part 
    tmp10 = tmp0 + tmp3;	// phase 2 
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;
    
    dataptr[0] = tmp10 + tmp11; // phase 3 
    dataptr[4] = tmp10 - tmp11;
    
    z1 = MULTIPLY(tmp12 + tmp13, FIX_0_707106781); // c4 
    dataptr[2] = tmp13 + z1;	// phase 5 
    dataptr[6] = tmp13 - z1;
    
    // Odd part 
    tmp10 = tmp4 + tmp5;	// phase 2 
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    // The rotator is modified from fig 4-8 to avoid extra negations. 
    z5 = MULTIPLY(tmp10 - tmp12, FIX_0_382683433); // c6 
    z2 = MULTIPLY(tmp10, FIX_0_541196100) + z5; // c2-c6 
    z4 = MULTIPLY(tmp12, FIX_1_306562965) + z5; // c2+c6 
    z3 = MULTIPLY(tmp11, FIX_0_707106781); // c4 

    z11 = tmp7 + z3;		// phase 5 
    z13 = tmp7 - z3;

    dataptr[5] = z13 + z2;	// phase 6 
    dataptr[3] = z13 - z2;
    dataptr[1] = z11 + z4;
    dataptr[7] = z11 - z4;

    dataptr += DCTSIZE;		// advance pointer to next row 
		}

  // Pass 2: process columns. 

  dataptr = data;
  for(ctr=DCTSIZE-1; ctr >= 0; ctr--) {
    tmp0 = dataptr[DCTSIZE*0] + dataptr[DCTSIZE*7];
    tmp7 = dataptr[DCTSIZE*0] - dataptr[DCTSIZE*7];
    tmp1 = dataptr[DCTSIZE*1] + dataptr[DCTSIZE*6];
    tmp6 = dataptr[DCTSIZE*1] - dataptr[DCTSIZE*6];
    tmp2 = dataptr[DCTSIZE*2] + dataptr[DCTSIZE*5];
    tmp5 = dataptr[DCTSIZE*2] - dataptr[DCTSIZE*5];
    tmp3 = dataptr[DCTSIZE*3] + dataptr[DCTSIZE*4];
    tmp4 = dataptr[DCTSIZE*3] - dataptr[DCTSIZE*4];
    
    // Even part 
    tmp10 = tmp0 + tmp3;	// phase 2 
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;
    
    dataptr[DCTSIZE*0] = tmp10 + tmp11; // phase 3 
    dataptr[DCTSIZE*4] = tmp10 - tmp11;
    
    z1 = MULTIPLY(tmp12 + tmp13, FIX_0_707106781); // c4 
    dataptr[DCTSIZE*2] = tmp13 + z1; // phase 5 
    dataptr[DCTSIZE*6] = tmp13 - z1;
    
    // Odd part 
    tmp10 = tmp4 + tmp5;	// phase 2 
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    // The rotator is modified from fig 4-8 to avoid extra negations.
    z5 = MULTIPLY(tmp10 - tmp12, FIX_0_382683433); // c6 
    z2 = MULTIPLY(tmp10, FIX_0_541196100) + z5; // c2-c6 
    z4 = MULTIPLY(tmp12, FIX_1_306562965) + z5; // c2+c6 
    z3 = MULTIPLY(tmp11, FIX_0_707106781); // c4 

    z11 = tmp7 + z3;		// phase 5 
    z13 = tmp7 - z3;

    dataptr[DCTSIZE*5] = z13 + z2; // phase 6 
    dataptr[DCTSIZE*3] = z13 - z2;
    dataptr[DCTSIZE*1] = z11 + z4;
    dataptr[DCTSIZE*7] = z11 - z4;

    dataptr++;			// advance pointer to next column 
		}
	}

#endif // DCT_IFAST_SUPPORTED


// ci sono altre DCT disponibili nel pacchetto... ma noi useremo la FLOAT!




/*
 * jidctflt.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains a floating-point implementation of the
 * inverse DCT (Discrete Cosine Transform).  In the IJG code, this routine
 * must also perform dequantization of the input coefficients.
 *
 * This implementation should be more accurate than either of the integer
 * IDCT implementations.  However, it may not give the same results on all
 * machines because of differences in roundoff behavior.  Speed will depend
 * on the hardware's floating point capacity.
 *
 * A 2-D IDCT can be done by 1-D IDCT on each column followed by 1-D IDCT
 * on each row (or vice versa, but it's more convenient to emit a row at
 * a time).  Direct algorithms are also available, but they are much more
 * complex and seem not to be any faster when reduced to code.
 *
 * This implementation is based on Arai, Agui, and Nakajima's algorithm for
 * scaled DCT.  Their original paper (Trans. IEICE E-71(11):1095) is in
 * Japanese, but the algorithm is described in the Pennebaker & Mitchell
 * JPEG textbook (see REFERENCES section in file README).  The following code
 * is based directly on figure 4-8 in P&M.
 * While an 8-point DCT cannot be done in less than 11 multiplies, it is
 * possible to arrange the computation so that many of the multiplies are
 * simple scalings of the final outputs.  These multiplies can then be
 * folded into the multiplications or divisions by the JPEG quantization
 * table entries.  The AA&N method leaves only 5 multiplies and 29 adds
 * to be done in the DCT itself.
 * The primary disadvantage of this method is that with a fixed-point
 * implementation, accuracy is lost due to imprecise representation of the
 * scaled quantization values.  However, that problem does not arise if
 * we use floating point arithmetic.
 */

#ifdef DCT_FLOAT_SUPPORTED


/*
 * This module is specialized to the case DCTSIZE = 8.
 */

#if DCTSIZE != 8
  Sorry, this code only copes with 8x8 DCTs. /* deliberate syntax err */
#endif


/* Dequantize a coefficient by multiplying it by the multiplier-table
 * entry; produce a float result.
 */

#define DEQUANTIZE(coef,quantval)  (((double) (coef)) * (quantval))


/*
 * Perform dequantization and inverse DCT on one block of coefficients.
 */

void CInverseDCT::inverse_DCT_float(J_COMPONENT_INFO *compptr,
	JCOEFPTR coef_block,
	JSAMPARRAY output_buf, JDIMENSION output_col) {
  double tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  double tmp10, tmp11, tmp12, tmp13;
  double z5, z10, z11, z12, z13;
  JCOEFPTR inptr;
  FLOAT_MULT_TYPE * quantptr;
  double * wsptr;
  JSAMPROW outptr;
  JSAMPLE *range_limit = IDCT_rangeLimit();
  int ctr;
  double workspace[DCTSIZE2]; /* buffers data between passes */
  SHIFT_TEMPS

  /* Pass 1: process columns from input, store into work array. */

  inptr = coef_block;
  quantptr = (FLOAT_MULT_TYPE *) compptr->dctTable;
  wsptr = workspace;
  for (ctr = DCTSIZE; ctr > 0; ctr--) {
    /* Due to quantization, we will usually find that many of the input
     * coefficients are zero, especially the AC terms.  We can exploit this
     * by short-circuiting the IDCT calculation for any column in which all
     * the AC terms are zero.  In that case each output is equal to the
     * DC coefficient (with scale factor as needed).
     * With typical images and quantization tables, half or more of the
     * column DCT calculations can be simplified this way.
     */
    
    if ((inptr[DCTSIZE*1] | inptr[DCTSIZE*2] | inptr[DCTSIZE*3] |
	 inptr[DCTSIZE*4] | inptr[DCTSIZE*5] | inptr[DCTSIZE*6] |
	 inptr[DCTSIZE*7]) == 0) {
      /* AC terms all zero */
      double dcval = DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]);
      
      wsptr[DCTSIZE*0] = dcval;
      wsptr[DCTSIZE*1] = dcval;
      wsptr[DCTSIZE*2] = dcval;
      wsptr[DCTSIZE*3] = dcval;
      wsptr[DCTSIZE*4] = dcval;
      wsptr[DCTSIZE*5] = dcval;
      wsptr[DCTSIZE*6] = dcval;
      wsptr[DCTSIZE*7] = dcval;
      
      inptr++;			/* advance pointers to next column */
      quantptr++;
      wsptr++;
      continue;
    }
    
    /* Even part */

    tmp0 = DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]);
    tmp1 = DEQUANTIZE(inptr[DCTSIZE*2], quantptr[DCTSIZE*2]);
    tmp2 = DEQUANTIZE(inptr[DCTSIZE*4], quantptr[DCTSIZE*4]);
    tmp3 = DEQUANTIZE(inptr[DCTSIZE*6], quantptr[DCTSIZE*6]);

    tmp10 = tmp0 + tmp2;	/* phase 3 */
    tmp11 = tmp0 - tmp2;

    tmp13 = tmp1 + tmp3;	/* phases 5-3 */
    tmp12 = (tmp1 - tmp3) * ((double) 1.414213562) - tmp13; /* 2*c4 */

    tmp0 = tmp10 + tmp13;	/* phase 2 */
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;
    
    /* Odd part */

    tmp4 = DEQUANTIZE(inptr[DCTSIZE*1], quantptr[DCTSIZE*1]);
    tmp5 = DEQUANTIZE(inptr[DCTSIZE*3], quantptr[DCTSIZE*3]);
    tmp6 = DEQUANTIZE(inptr[DCTSIZE*5], quantptr[DCTSIZE*5]);
    tmp7 = DEQUANTIZE(inptr[DCTSIZE*7], quantptr[DCTSIZE*7]);

    z13 = tmp6 + tmp5;		/* phase 6 */
    z10 = tmp6 - tmp5;
    z11 = tmp4 + tmp7;
    z12 = tmp4 - tmp7;

    tmp7 = z11 + z13;		/* phase 5 */
    tmp11 = (z11 - z13) * ((double) 1.414213562); /* 2*c4 */

    z5 = (z10 + z12) * ((double) 1.847759065); /* 2*c2 */
    tmp10 = ((double) 1.082392200) * z12 - z5; /* 2*(c2-c6) */
    tmp12 = ((double) -2.613125930) * z10 + z5; /* -2*(c2+c6) */

    tmp6 = tmp12 - tmp7;	/* phase 2 */
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    wsptr[DCTSIZE*0] = tmp0 + tmp7;
    wsptr[DCTSIZE*7] = tmp0 - tmp7;
    wsptr[DCTSIZE*1] = tmp1 + tmp6;
    wsptr[DCTSIZE*6] = tmp1 - tmp6;
    wsptr[DCTSIZE*2] = tmp2 + tmp5;
    wsptr[DCTSIZE*5] = tmp2 - tmp5;
    wsptr[DCTSIZE*4] = tmp3 + tmp4;
    wsptr[DCTSIZE*3] = tmp3 - tmp4;

    inptr++;			/* advance pointers to next column */
    quantptr++;
    wsptr++;
  }
  
  /* Pass 2: process rows from work array, store into output array. */
  /* Note that we must descale the results by a factor of 8 == 2**3. */

  wsptr = workspace;
  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    outptr = output_buf[ctr] + output_col;
    /* Rows of zeroes can be exploited in the same way as we did with columns.
     * However, the column calculation has created many nonzero AC terms, so
     * the simplification applies less often (typically 5% to 10% of the time).
     * And testing floats for zero is relatively expensive, so we don't bother.
     */
    
    /* Even part */

    tmp10 = wsptr[0] + wsptr[4];
    tmp11 = wsptr[0] - wsptr[4];

    tmp13 = wsptr[2] + wsptr[6];
    tmp12 = (wsptr[2] - wsptr[6]) * ((double) 1.414213562) - tmp13;

    tmp0 = tmp10 + tmp13;
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;

    /* Odd part */

    z13 = wsptr[5] + wsptr[3];
    z10 = wsptr[5] - wsptr[3];
    z11 = wsptr[1] + wsptr[7];
    z12 = wsptr[1] - wsptr[7];

    tmp7 = z11 + z13;
    tmp11 = (z11 - z13) * ((double) 1.414213562);

    z5 = (z10 + z12) * ((double) 1.847759065); /* 2*c2 */
    tmp10 = ((double) 1.082392200) * z12 - z5; /* 2*(c2-c6) */
    tmp12 = ((double) -2.613125930) * z10 + z5; /* -2*(c2+c6) */

    tmp6 = tmp12 - tmp7;
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    /* Final output stage: scale down by a factor of 8 and range-limit */

    outptr[0] = range_limit[(int) DESCALE((INT32) (tmp0 + tmp7), 3)
			    & RANGE_MASK];
    outptr[7] = range_limit[(int) DESCALE((INT32) (tmp0 - tmp7), 3)
			    & RANGE_MASK];
    outptr[1] = range_limit[(int) DESCALE((INT32) (tmp1 + tmp6), 3)
			    & RANGE_MASK];
    outptr[6] = range_limit[(int) DESCALE((INT32) (tmp1 - tmp6), 3)
			    & RANGE_MASK];
    outptr[2] = range_limit[(int) DESCALE((INT32) (tmp2 + tmp5), 3)
			    & RANGE_MASK];
    outptr[5] = range_limit[(int) DESCALE((INT32) (tmp2 - tmp5), 3)
			    & RANGE_MASK];
    outptr[4] = range_limit[(int) DESCALE((INT32) (tmp3 + tmp4), 3)
			    & RANGE_MASK];
    outptr[3] = range_limit[(int) DESCALE((INT32) (tmp3 - tmp4), 3)
			    & RANGE_MASK];
    
    wsptr += DCTSIZE;		/* advance pointer to next row */
  }
}

#endif /* DCT_FLOAT_SUPPORTED */




/*
 * jmemnobs.c
 *
 * Copyright (C) 1992-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file provides a really simple implementation of the system-
 * dependent portion of the JPEG memory manager.  This implementation
 * assumes that no backing-store files are needed: all required space
 * can be obtained from malloc().
 * This is very portable in the sense that it'll compile on almost anything,
 * but you'd better have lots of main memory (or virtual memory) if you want
 * to process big images.
 * Note that the max_memory_to_use option is ignored by this implementation.
 */



/*
 * This routine computes the total memory space available for allocation.
 * Here we always say, "we got all you want bud!"
 */

long CJMemoryMgr::memAvailable(long min_bytes_needed,
	long max_bytes_needed /*, long already_allocated*/) {

  return max_bytes_needed;
	}


/*
 * Backing store (temporary file) management.
 * Since jpeg_mem_available always promised the moon,
 * this should never be called and we can just error out.
 */

CBackingStore::CBackingStore(const CJpeg *p,long total_bytes_needed) {

	isOpen=FALSE;
	p->err->errorExit(JERR_NO_BACKING_STORE);
	}

/*
 * Initial opening of a backing-store object.  This must fill in the
 * read/write/close pointers in the object.  The read/write routines
 * may take an error exit if the specified maximum file size is exceeded.
 * (If jpeg_mem_available always returns a large value, this routine can
 * just take an error exit.)
 */
BOOL CBackingStore::openStore(long l) {
	return FALSE;
	}


/*
 * These routines take care of any system-dependent initialization and
 * cleanup required.  Here, there isn't any.
 */

long CJpeg::memInit() {
  return 0;			// just set max_memory_to_use to 0 
	}

void CJpeg::memTerm() {
  // no work 
	}

// ci sono altri modi di gestione memoria, nel pacchetto, ma questo e' consigliato per Windows.



/*
 * jdapimin.c
 */

CDecompressJpeg *CJpeg::createDecompress(int version) {

  // Guard against version mismatches between library and caller.
//  mem=NULL;		// so jpeg_destroy knows mem mgr not called
	if(version != JPEG_LIB_VERSION)
    err->errorExit(JERR_BAD_LIB_VERSION, JPEG_LIB_VERSION, version);

  /* For debugging purposes, we zero the whole master structure.
   * But the application has already set the err pointer, and may have set
   * client_data, so we have to save and restore those fields.
   * Note: if application hasn't set client_data, tools like Purify may
   * complain here.
   */
	de=new CDecompressJpeg(this,clientData);
  isDecompressor = TRUE;

	// Memory manager initialization
	initMemoryMgr();

#ifdef PROGRESS_REPORT
	progress=new J_PROGRESS_MGR(this);
#endif

	return de;
	}

CDecompressJpeg::CDecompressJpeg(const CJpeg *p,void *cData) : m_Parent(p) {
	int i;

	globalState=0;

  dataPrecision = BITS_IN_JSAMPLE;
	imageWidth=imageHeight=outputWidth=outputHeight=0;
	numComponents=outColorComponents=outputComponents=0;
	colorSpace=JCS_UNKNOWN;
	outColorSpace=JCS_UNKNOWN;
	actualNumberOfColors=0;
	recOutbufHeight=0;
	inputScanNumber=outputScanNumber=0;
	scaleNum=scaleDenom=0;
	bufferedImage=0;
	rawDataOut=0;
	outputGamma=0.0;
	input_iMCU_row=output_iMCU_row=0;
	coefBits=NULL;
	doFancyUpsampling=0;
	doBlockSmoothing=0;
	quantizeColors=0;
	ditherMode=JDITHER_NONE;
	twoPassQuantize=0;
	desiredNumberOfColors=0;
	enable1passQuant=enable2passQuant=0;
	enableExternalQuant=0;
	colormap=NULL;
	markerList=NULL;
	progressiveMode=0;
	restartInterval=restartInRows=0;
	saw_JFIF_marker=sawAdobeMarker=0;
	JFIF_MajorVersion=JFIF_MinorVersion=0;
	densityUnit=0;
	AdobeTransform=0;
	CCIR601Sampling=0;
	XDensity=YDensity=0;
	max_H_SampFactor=max_V_SampFactor=0;
	min_DCT_scaledSize=0;
	total_iMCU_rows=0;
	sampleRangeLimit=NULL;
	compsInScan=0;
//	curCompInfo=0;
	MCUs_perRow=MCU_rowsInScan=blocksInMCU=0;
//	MCU_membership=NULL;
	Ss=Se=Ah=Al=0;
	outputScanline=0;
//	arith_dc_L=NULL;
//	arith_dc_U=NULL;
//	arith_dc_K=NULL;

	coef=NULL;
	main=NULL;
	marker=NULL;
	master=NULL;
	post=NULL;
	inputctl=NULL;
	entropy=NULL;
	idct=NULL;
	upsample=NULL;
	cconvert=NULL;
	cquantize=NULL;
	quantizer_1pass=quantizer_2pass=NULL;

	dctMethod=JDCT_FLOAT;


  // Use Huffman coding, not arithmetic coding, by default 
  arithCode = FALSE;

	compInfo=NULL;
  /* Zero out pointers to permanent structures. */
  src = NULL;

  for(i=0; i<NUM_QUANT_TBLS; i++)
    quantTblPtrs[i] = NULL;

  for(i=0; i<NUM_HUFF_TBLS; i++) {
    DC_HuffTblPtrs[i] = NULL;
    AC_HuffTblPtrs[i] = NULL;
	  }

	for(i=0; i<DCTSIZE2+16; i++)
		naturalOrder[i]=CCompressJpeg::nOrder[i];


  /* Initialize marker processor so application can override methods
   * for COM, APPn markers before calling jpeg_read_header.
   */
  marker=new CMarkerReader(this);
  /* And initialize the overall input controller. */
  initInputController();


  // OK, I'm ready 
  globalState = DSTATE_START;
	}

CDecompressJpeg::~CDecompressJpeg() {

	delete entropy;	entropy=NULL;
	delete cconvert;	cconvert=NULL;
	delete coef;		coef=NULL;
	delete marker;	marker=NULL;
	delete master;	master=NULL;
	delete src;	src=NULL;
	delete main;	main=NULL;
	delete post;	post=NULL;
	delete inputctl;	inputctl=NULL;
	delete idct;	idct=NULL;
	delete upsample;	upsample=NULL;
	}

JQUANT_TBL *CDecompressJpeg::allocQuantTable() {		// DUPLICATA in CCompressJpeg...
  JQUANT_TBL *tbl;

  tbl=(JQUANT_TBL *)m_Parent->mem->allocSmall(JPOOL_PERMANENT, sizeof(JQUANT_TBL));
  tbl->sentTable = FALSE;	// make sure this is false in any new table 
  return tbl;
	}

JHUFF_TBL *CDecompressJpeg::allocHuffTable() {			// DUPLICATA in CCompressJpeg...
  JHUFF_TBL *tbl;

  tbl=(JHUFF_TBL *)m_Parent->mem->allocSmall(JPOOL_PERMANENT, sizeof(JHUFF_TBL));
  tbl->sentTable = FALSE;	// make sure this is false in any new table 
  return tbl;
	}


/*
 * Decompression initialization.
 * jpeg_read_header must be completed before calling this.
 *
 * If a multipass operating mode was selected, this will do all but the
 * last pass, and thus may take a great deal of time.
 *
 * Returns FALSE if suspended.  The return value need be inspected only if
 * a suspending data source is used.
 */

BOOL CDecompressJpeg::startDecompress() {

  if(globalState == DSTATE_READY) {
    /* First call: initialize master control, select active modules */
		initDecompressMaster();

    if(bufferedImage) {
      /* No more work here; expecting jpeg_start_output next */
      globalState = DSTATE_BUFIMAGE;
      return TRUE;
			}
    globalState = DSTATE_PRELOAD;
		}
  if(globalState == DSTATE_PRELOAD) {
    /* If file has multiple scans, absorb them all into the coef buffer */
    if(inputctl->hasMultipleScans) {
#ifdef D_MULTISCAN_FILES_SUPPORTED
      for(;;) {
				int retcode;
	/* Call progress monitor hook if present */
      if(m_Parent->progress != NULL) {
				(m_Parent->progress->progressMonitor) ();
				}
			/* Absorb some more input */
				retcode = (inputctl->*inputctl->doConsumeInput) ();
				if (retcode == J_SUSPENDED)
					return FALSE;
				if (retcode == J_REACHED_EOI)
					break;
	/* Advance progress counter if appropriate */
				if(m_Parent->progress != NULL &&
						(retcode == J_ROW_COMPLETED || retcode == J_REACHED_SOS)) {
					if (++m_Parent->progress->passCounter >= m_Parent->progress->passLimit) {
						/* jdmaster underestimated number of scans; ratchet up one scan */
						m_Parent->progress->passLimit += (long) total_iMCU_rows;
						}
					}
			  }
#else
      m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif /* D_MULTISCAN_FILES_SUPPORTED */
			}
    outputScanNumber = inputScanNumber;
		}
	else if(globalState != DSTATE_PRESCAN)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);

  /* Perform any dummy output passes, and set up for the final pass */
  return outputPassSetup();
	}

/*
 * Set up for an output pass, and perform any dummy pass(es) needed.
 * Common subroutine for jpeg_start_decompress and jpeg_start_output.
 * Entry: global_state = DSTATE_PRESCAN only if previously suspended.
 * Exit: If done, returns TRUE and sets global_state for proper output mode.
 *       If suspended, returns FALSE and sets global_state = DSTATE_PRESCAN.
 */

BOOL CDecompressJpeg::outputPassSetup() {

  if(globalState != DSTATE_PRESCAN) {
    /* First call: do pass setup */
    master->prepareForOutputPass();
    outputScanline = 0;
    globalState = DSTATE_PRESCAN;
		}
  /* Loop over any required dummy passes */
  while(master->isDummyPass) {
#ifdef QUANT_2PASS_SUPPORTED
    /* Crank through the dummy pass */
    while(outputScanline < outputHeight) {
      JDIMENSION last_scanline;
      /* Call progress monitor hook if present */
      if(m_Parent->progress != NULL) {
				m_Parent->progress->passCounter = (long) outputScanline;
				m_Parent->progress->passLimit = (long) outputHeight;
				(m_Parent->progress->progressMonitor) ();
				}
      /* Process some data */
      last_scanline = outputScanline;
      (*main->processData) ((JSAMPARRAY) NULL,&outputScanline, (JDIMENSION) 0);
      if(outputScanline == last_scanline)
				return FALSE;		/* No progress made, must suspend */
			}
    /* Finish up dummy pass, and set up for another one */
    (master->finishOutputPass) ();
    (master->prepareForOutputPass) ();
    outputScanline = 0;
#else
    m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif /* QUANT_2PASS_SUPPORTED */
  }
  /* Ready for application to drive output pass through
   * jpeg_read_scanlines or jpeg_read_raw_data.
   */
  globalState = rawDataOut ? DSTATE_RAW_OK : DSTATE_SCANNING;
  return TRUE;
	}




/*
 * Read some scanlines of data from the JPEG decompressor.
 *
 * The return value will be the number of lines actually read.
 * This may be less than the number requested in several cases,
 * including bottom of image, data source suspension, and operating
 * modes that emit multiple scanlines at a time.
 *
 * Note: we warn about excess calls to jpeg_read_scanlines() since
 * this likely signals an application programmer error.  However,
 * an oversize buffer (max_lines > scanlines remaining) is not an error.
 */

JDIMENSION CDecompressJpeg::readScanlines(JSAMPARRAY scanlines, JDIMENSION max_lines) {
  JDIMENSION row_ctr;

  if(globalState != DSTATE_SCANNING)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
  if(outputScanline >= outputHeight) {
    m_Parent->err->warn(JWRN_TOO_MUCH_DATA);
    return 0;
		}

  /* Call progress monitor hook if present */
  if(m_Parent->progress != NULL) {
    m_Parent->progress->passCounter = (long) outputScanline;
    m_Parent->progress->passLimit = (long) outputHeight;
    m_Parent->progress->progressMonitor();
		}

  /* Process some data */
  row_ctr = 0;
  (main->*main->doProcessData) (scanlines, &row_ctr, max_lines);
  outputScanline += row_ctr;
  return row_ctr;
	}


/*
 * Alternate entry point to read raw data.
 * Processes exactly one iMCU row per call, unless suspended.
 */

JDIMENSION CDecompressJpeg::readRawData(JSAMPIMAGE data, JDIMENSION max_lines) {
  JDIMENSION lines_per_iMCU_row;

  if(globalState != DSTATE_RAW_OK)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
  if(outputScanline >= outputHeight) {
    m_Parent->err->warn(JWRN_TOO_MUCH_DATA);
    return 0;
		}

  /* Call progress monitor hook if present */
  if(m_Parent->progress != NULL) {
    m_Parent->progress->passCounter = (long) outputScanline;
    m_Parent->progress->passLimit = (long) outputHeight;
    m_Parent->progress->progressMonitor();
		}

  /* Verify that at least one iMCU row can be returned. */
  lines_per_iMCU_row = max_V_SampFactor * min_DCT_scaledSize;
  if(max_lines < lines_per_iMCU_row)
    m_Parent->err->errorExit(JERR_BUFFER_SIZE);

  /* Decompress directly into user's buffer. */
  if(! (coef->*coef->doDecompressData) (data))
    return 0;			/* suspension forced, can do nothing more */

  /* OK, we processed one iMCU row. */
  outputScanline += lines_per_iMCU_row;
  return lines_per_iMCU_row;
	}


/* Additional entry points for buffered-image mode. */

#ifdef D_MULTISCAN_FILES_SUPPORTED

/*
 * Initialize for an output pass in buffered-image mode.
 */

BOOL CDecompressJpeg::startOutput(int scan_number) {

  if(globalState != DSTATE_BUFIMAGE && globalState != DSTATE_PRESCAN)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
  /* Limit scan number to valid range */
  if(scan_number <= 0)
    scan_number = 1;
  if(inputctl->EOI_Reached &&
    scan_number > inputScanNumber)
    scan_number = inputScanNumber;
  outputScanNumber = scan_number;
  /* Perform any dummy output passes, and set up for the real pass */
  return outputPassSetup();
	}


/*
 * Finish up after an output pass in buffered-image mode.
 *
 * Returns FALSE if suspended.  The return value need be inspected only if
 * a suspending data source is used.
 */

BOOL CDecompressJpeg::finishOutput() {

  if((globalState == DSTATE_SCANNING ||
       globalState == DSTATE_RAW_OK) && bufferedImage) {
    /* Terminate this pass. */
    /* We do not require the whole pass to have been completed. */
    master->finishOutputPass();
    globalState = DSTATE_BUFPOST;
		}
	else if(globalState != DSTATE_BUFPOST) {
    /* BUFPOST = repeat call after a suspension, anything else is error */
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
		}
  /* Read markers looking for SOS or EOI */
  while(inputScanNumber <= outputScanNumber && !inputctl->EOI_Reached) {
    if((inputctl->*inputctl->doConsumeInput)() == J_SUSPENDED)
      return FALSE;		/* Suspend, come back later */
		}
  globalState = DSTATE_BUFIMAGE;
  return TRUE;
	}

#endif /* D_MULTISCAN_FILES_SUPPORTED */


void CJpeg::destroyDecompress() {

  destroy(); // use common routine
	delete de;
	de=NULL;
	}

void CDecompressJpeg::setMarkerProcessor(int markerCode,BOOL (CMarkerReader::*routine)()) {

  if(markerCode == CMarkerReader::COM)
    marker->process_COM = routine;
  else if(markerCode >= CMarkerReader::APP0 && markerCode <= CMarkerReader::APP0+15)
    marker->process_APPn[markerCode-CMarkerReader::APP0] = routine;
  else
    m_Parent->err->errorExit(JERR_UNKNOWN_MARKER, markerCode);
	}

void CMarkerReader::resetMarkerReader() {

  m_Parent->compInfo = NULL;		/* until allocated by get_sof */
  m_Parent->inputScanNumber = 0;		/* no SOS seen yet */
  m_Parent->unreadMarker = 0;		/* no pending marker */
  saw_SOI = FALSE;	/* set internal state too */
  saw_SOF = FALSE;
  discardedBytes = 0;
	}


/*
 * Set default decompression parameters.
 */

void CDecompressJpeg::defaultDecompressParms() {

  /* Guess the input colorspace, and set output colorspace accordingly. */
  /* (Wish JPEG committee had provided a real way to specify this...) */
  /* Note application may override our guesses. */
  switch(numComponents) {
		case 1:
			colorSpace= JCS_GRAYSCALE;
			outColorSpace = JCS_GRAYSCALE;
			break;
    
		case 3:
			if(saw_JFIF_marker) {
				colorSpace = JCS_YCbCr; /* JFIF implies YCbCr */
				}
			else if(sawAdobeMarker) {
				switch(AdobeTransform) {
					case 0:
						colorSpace= JCS_RGB;
						break;
					case 1:
						colorSpace= JCS_YCbCr;
						break;
					default:
		//				WARNMS1(cinfo, JWRN_ADOBE_XFORM, Adobe_transform);
						colorSpace= JCS_YCbCr; /* assume it's YCbCr */
						break;
					}
				}
			else {
    /* Saw no special markers, try to guess from the component IDs */
				int cid0 = compInfo[0].componentId;
				int cid1 = compInfo[1].componentId;
				int cid2 = compInfo[2].componentId;

				if(cid0 == 1 && cid1 == 2 && cid2 == 3)
					colorSpace = JCS_YCbCr; /* assume JFIF w/out marker */
				else if (cid0 == 82 && cid1 == 71 && cid2 == 66)
					colorSpace = JCS_RGB; /* ASCII 'R', 'G', 'B' */
				else {
		//			TRACEMS3(cinfo, 1, JTRC_UNKNOWN_IDS, cid0, cid1, cid2);
					colorSpace = JCS_YCbCr; /* assume it's YCbCr */
					}
				}
    /* Always guess RGB is proper output colorspace. */
			outColorSpace = JCS_RGB;
			break;
    
	  case 4:
		  if(sawAdobeMarker) {
			  switch(AdobeTransform) {
					case 0:
						colorSpace = JCS_CMYK;
						break;
					case 2:
						colorSpace = JCS_YCCK;
						break;
					default:
//				WARNMS1(cinfo, JWRN_ADOBE_XFORM, cinfo->Adobe_transform);
						colorSpace = JCS_YCCK; /* assume it's YCCK */
						break;
					}
				}
			else {
      /* No special markers, assume straight CMYK. */
				colorSpace = JCS_CMYK;
				}
			outColorSpace = JCS_CMYK;
			break;
    
		default:
			colorSpace = JCS_UNKNOWN;
			outColorSpace = JCS_UNKNOWN;
			break;
	  }

  /* Set defaults for other decompression parameters. */
  scaleNum = 1;		/* 1:1 scaling */
  scaleDenom = 1;
  outputGamma = 1.0;
  bufferedImage = FALSE;
  rawDataOut = FALSE;
  dctMethod = JDCT_DEFAULT;
  doFancyUpsampling = TRUE;
  doBlockSmoothing = TRUE;
  quantizeColors = FALSE;
  /* We set these in case application only sets quantize_colors. */
  ditherMode = JDITHER_FS;
#ifdef QUANT_2PASS_SUPPORTED
  twoPassQuantize = TRUE;
#else
  twoPassQuantize = FALSE;
#endif
  desiredNumberOfColors = 256;
  colormap = NULL;
  /* Initialize for no mode change in buffered-image mode. */
  enable1passQuant = FALSE;
  enableExternalQuant = FALSE;
  enable2passQuant = FALSE;
	}


/*
 * Decompression startup: read start of JPEG datastream to see what's there.
 * Need only initialize JPEG object and supply a data source before calling.
 *
 * This routine will read as far as the first SOS marker (ie, actual start of
 * compressed data), and will save all tables and parameters in the JPEG
 * object.  It will also initialize the decompression parameters to default
 * values, and finally return JPEG_HEADER_OK.  On return, the application may
 * adjust the decompression parameters and then call jpeg_start_decompress.
 * (Or, if the application only wanted to determine the image parameters,
 * the data need not be decompressed.  In that case, call jpeg_abort or
 * jpeg_destroy to release any temporary space.)
 * If an abbreviated (tables only) datastream is presented, the routine will
 * return JPEG_HEADER_TABLES_ONLY upon reaching EOI.  The application may then
 * re-use the JPEG object to read the abbreviated image datastream(s).
 * It is unnecessary (but OK) to call jpeg_abort in this case.
 * The JPEG_SUSPENDED return code only occurs if the data source module
 * requests suspension of the decompressor.  In this case the application
 * should load more source data and then re-call jpeg_read_header to resume
 * processing.
 * If a non-suspending data source is used and require_image is TRUE, then the
 * return code need not be inspected since only JPEG_HEADER_OK is possible.
 *
 * This routine is now just a front end to jpeg_consume_input, with some
 * extra error checking.
 */

CDecompressJpeg::readHeader(BOOL requireImage) {
  int retcode;

  if(globalState != DSTATE_START && globalState != DSTATE_INHEADER)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);

  retcode = consumeInput();

  switch(retcode) {
		case J_REACHED_SOS:
			retcode = J_HEADER_OK;
			break;
		case J_REACHED_EOI:
			if(requireImage)		/* Complain if application wanted an image */
				m_Parent->err->errorExit(JERR_NO_IMAGE);
			/* Reset to start state; it would be safer to require the application to
			 * call jpeg_abort, but we can't change it now for compatibility reasons.
			 * A side effect is to free any temporary memory (there shouldn't be any).
			 */
			abort(); /* sets state = DSTATE_START */
			retcode = J_HEADER_TABLES_ONLY;
			break;
		case J_SUSPENDED:
			/* no work */
			break;
	  }

  return retcode;
	}


/*
 * Consume data in advance of what the decompressor requires.
 * This can be called at any time once the decompressor object has
 * been created and a data source has been set up.
 *
 * This routine is essentially a state machine that handles a couple
 * of critical state-transition actions, namely initial setup and
 * transition from header scanning to ready-for-start_decompress.
 * All the actual input is done via the input controller's consume_input
 * method.
 */

int CDecompressJpeg::consumeInput() {
  int retcode = J_SUSPENDED;

  /* NB: every possible DSTATE value should be listed in this switch */
  switch(globalState) {
	  case DSTATE_START:
			/* Start-of-datastream actions: reset appropriate modules */
			inputctl->resetInputController();
			/* Initialize application's data source module */
			src->initSource();
			globalState = DSTATE_INHEADER;
			/*FALLTHROUGH*/
		case DSTATE_INHEADER:
			retcode = (inputctl->*inputctl->doConsumeInput) ();
			if(retcode == J_REACHED_SOS) {	/* Found SOS, prepare to decompress */
				/* Set up default parameters based on header data */
				defaultDecompressParms();
				/* Set global state: ready for start_decompress */
				globalState = DSTATE_READY;
				}
			break;
		case DSTATE_READY:
			/* Can't advance past first SOS until start_decompress is called */
			retcode = J_REACHED_SOS;
			break;
		case DSTATE_PRELOAD:
		case DSTATE_PRESCAN:
		case DSTATE_SCANNING:
		case DSTATE_RAW_OK:
		case DSTATE_BUFIMAGE:
		case DSTATE_BUFPOST:
		case DSTATE_STOPPING:
			retcode = (inputctl->*inputctl->doConsumeInput) ();
			break;
		default:
			m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
		}
  return retcode;
	}


/*
 * Have we finished reading the input file?
 */

CDecompressJpeg::inputComplete() {

  /* Check for valid jpeg object */
  if(globalState < DSTATE_START ||
    globalState > DSTATE_STOPPING)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
  return inputctl->EOI_Reached;
	}


/*
 * Is there more than one scan?
 */

CDecompressJpeg::hasMultipleScans() {

  /* Only valid after jpeg_read_header completes */
  if(globalState < DSTATE_READY ||
      globalState > DSTATE_STOPPING)
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
  return inputctl->hasMultipleScans;
	}

/*
 * Initialize master decompression control and select active modules.
 * This is performed at the start of jpeg_start_decompress.
 */

/*
 * Master selection of decompression modules.
 * This is done once at jpeg_start_decompress time.  We determine
 * which modules will be used and give them appropriate initialization calls.
 * We also initialize the decompressor input side to begin consuming data.
 *
 * Since jpeg_read_header has finished, we know what is in the SOF
 * and (first) SOS markers.  We also have all the application parameter
 * settings.
 */

void CDecompressJpeg::initDecompressMaster() {
	BOOL use_c_buffer;
  long samplesperrow;
  JDIMENSION jd_samplesperrow;

  master=new CDecompMaster(this);

  /* Initialize dimensions and other stuff */
  master->calcOutputDimensions();
  master->prepareRangeLimitTable();

  /* Width of an output scanline must be representable as JDIMENSION. */
  samplesperrow = (long) outputWidth * (long) outColorComponents;
  jd_samplesperrow = (JDIMENSION) samplesperrow;
  if((long) jd_samplesperrow != samplesperrow)
    m_Parent->err->errorExit(JERR_WIDTH_OVERFLOW);

  /* Initialize my private state */
  master->usingMergedUpsample = master->useMergedUpsample();

  /* Color quantizer selection */
  quantizer_1pass = NULL;
	quantizer_2pass = NULL;
  /* No mode changes if not using buffered-image mode. */
  if(!quantizeColors || ! bufferedImage) {
    enable1passQuant = FALSE;
    enableExternalQuant = FALSE;
    enable2passQuant = FALSE;
		}
  if(quantizeColors) {
    if(rawDataOut)
      m_Parent->err->errorExit(JERR_NOTIMPL);
    /* 2-pass quantizer only works in 3-component color space. */
    if(outColorComponents != 3) {
      enable1passQuant = TRUE;
      enableExternalQuant = FALSE;
      enable2passQuant = FALSE;
      colormap = NULL;
			}
		else if(colormap != NULL) {
      enableExternalQuant = TRUE;
			} 
		else if(twoPassQuantize) {
      enable2passQuant = TRUE;
			} 
		else {
      enable1passQuant = TRUE;
			}

    if(enable1passQuant) {
#ifdef QUANT_1PASS_SUPPORTED
      quantizer_1pass = new CColorQuantizer(this,1);
			cquantize=quantizer_1pass;
#else
      m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif
    }

    /* We use the 2-pass code to map to external colormaps. */
    if(enable2passQuant || enableExternalQuant) {
#ifdef QUANT_2PASS_SUPPORTED
      quantizer_2pass = new CColorQuantizer(this,2);
			cquantize=quantizer_2pass;
#else
      m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif
    }
    /* If both quantizers are initialized, the 2-pass one is left active;
     * this is necessary for starting with quantization to an external map.
     */
  }

  /* Post-processing: in particular, color conversion first */
  if(!rawDataOut) {
    if(master->usingMergedUpsample) {
#ifdef UPSAMPLE_MERGING_SUPPORTED
      upsample=new CUpsampler(); /* does color conversion too */
#else
      m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif
			}
		else {
      cconvert=new CColorDeconverter(this);
      upsample=new CUpsampler(this);
			}
    post=new CDPostController(this,enable2passQuant);
		}
  /* Inverse DCT */
  idct=new CInverseDCT(this);
  /* Entropy decoding: either Huffman or arithmetic coding. */
  if(arithCode) {
    m_Parent->err->errorExit(JERR_ARITH_NOTIMPL);
		}
	else {
    if(progressiveMode) {
#ifdef D_PROGRESSIVE_SUPPORTED
      entropy=new CHuffEntropyDecoder(this);
//      jinit_phuff_decoder();
#else
      m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif
			}
		else
      entropy=new CHuffEntropyDecoder(this);
		}

  /* Initialize principal buffer controllers. */
  use_c_buffer = inputctl->hasMultipleScans || bufferedImage;
	coef=new CDCoefController(this,use_c_buffer);

  if(!rawDataOut) 
    main=new CDMainController(this,FALSE /* never need full buffer here */);

  /* We can now tell the memory manager to allocate virtual arrays. */
  realizeVirtArrays();

  /* Initialize input side of decompressor to consume first scan. */
  inputctl->startInputPass();

#ifdef D_MULTISCAN_FILES_SUPPORTED
  /* If jpeg_start_decompress will read the whole file, initialize
   * progress monitoring appropriately.  The input step is counted
   * as one pass.
   */
  if(m_Parent->progress != NULL && ! bufferedImage &&
    inputctl->hasMultipleScans) {
    int nscans;

	ASSERT(0);

    /* Estimate number of scans to set pass_limit. */
    if(progressiveMode) {
      /* Arbitrarily estimate 2 interleaved DC scans + 3 AC scans/component. */
      nscans = 2 + 3 * numComponents;
			}
		else {
      /* For a nonprogressive multiscan file, estimate 1 scan per component. */
      nscans = numComponents;
			}
    m_Parent->progress->passCounter = 0L;
    m_Parent->progress->passLimit = (long) total_iMCU_rows * nscans;
    m_Parent->progress->completedPasses = 0;
    m_Parent->progress->totalPasses = (enable2passQuant ? 3 : 2);
    /* Count the input pass as done */
    master->passNumber++;
		}
#endif /* D_MULTISCAN_FILES_SUPPORTED */

	}


CUpsampler::CUpsampler(CDecompressJpeg *p) : doUpsample(NULL), m_Parent(p) {
	
  Cr_r_tab=NULL;
  Cb_b_tab=NULL;
  Cr_g_tab=NULL;
  Cb_g_tab=NULL;

	needContextRows=FALSE;

  outRowWidth = m_Parent->outputWidth * m_Parent->outColorComponents;

  if(m_Parent->max_V_SampFactor == 2) {
    doUpsample = merged_2v_upsample;
    upmethod = h2v2_merged_upsample;
    /* Allocate a spare row buffer */
    spareRow = (JSAMPROW)
      m_Parent->m_Parent->mem->allocLarge(JPOOL_IMAGE,
			(size_t) (outRowWidth * sizeof(JSAMPLE)));
		}
	else {
    doUpsample = merged_1v_upsample;
//    upmethod = m_Parent->numComponents != 1 ? h2v1_merged_upsample :h2v1_merged_upsample_gray; FINIRE! non gestisce grayscale jpeg...
    upmethod = h2v1_merged_upsample;

    /* No spare row needed */
    spareRow = NULL;
		}

  build_ycc_rgb_table();
	}

/*
 * Initialize tables for YCC->RGB colorspace conversion.
 * This is taken directly from jdcolor.c; see that file for more info.
 */

void CUpsampler::build_ycc_rgb_table() {
  int i;
  INT32 x;
  SHIFT_TEMPS

  Cr_r_tab = (int *)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE, (MAXJSAMPLE+1) * sizeof(int));
  Cb_b_tab = (int *)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE, (MAXJSAMPLE+1) * sizeof(int));
  Cr_g_tab = (INT32 *)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE, (MAXJSAMPLE+1) * sizeof(INT32));
  Cb_g_tab = (INT32 *)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE, (MAXJSAMPLE+1) * sizeof(INT32));

  for(i=0, x = -CENTERJSAMPLE; i <= MAXJSAMPLE; i++, x++) {
    /* i is the actual input pixel value, in the range 0..MAXJSAMPLE */
    /* The Cb or Cr value we are thinking of is x = i - CENTERJSAMPLE */
    /* Cr=>R value is nearest int to 1.40200 * x */
    Cr_r_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.40200) * x + ONE_HALF, SCALEBITS);
    /* Cb=>B value is nearest int to 1.77200 * x */
    Cb_b_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.77200) * x + ONE_HALF, SCALEBITS);
    /* Cr=>G value is scaled-up -0.71414 * x */
    Cr_g_tab[i] = (- FIX(0.71414)) * x;
    /* Cb=>G value is scaled-up -0.34414 * x */
    /* We also add in ONE_HALF so that need not do it in inner loop */
    Cb_g_tab[i] = (- FIX(0.34414)) * x + ONE_HALF;
		}
	}


/*
 * Initialize for an upsampling pass.
 */

void CUpsampler::startPass() {			// era startPassMergedUpsample...

  /* Mark the spare buffer empty */
  spareFull = FALSE;
  /* Initialize total-height counter for detecting bottom of image */
  rowsToGo = m_Parent->outputHeight;
	}


/*
 * Control routine to do upsampling (and color conversion).
 *
 * The control routine just handles the row buffering considerations.
 */

void CUpsampler::merged_2v_upsample(JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	JDIMENSION in_row_groups_avail,
	JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	JDIMENSION out_rows_avail) {

/* 2:1 vertical sampling case: may need a spare row. */
  JSAMPROW work_ptrs[2];
  JDIMENSION num_rows;		/* number of rows returned to caller */

  if(spareFull) {
    /* If we have a spare row saved from a previous cycle, just return it. */
    CCompressJpeg::copySampleRows(& spareRow, 0, output_buf + *out_row_ctr, 0,
		  1, outRowWidth);
    num_rows = 1;
    spareFull = FALSE;
		}
	else {
    /* Figure number of rows to return to caller. */
    num_rows = 2;
    /* Not more than the distance to the end of the image. */
    if(num_rows > rowsToGo)
      num_rows = rowsToGo;
    /* And not more than what the client can accept: */
    out_rows_avail -= *out_row_ctr;
    if(num_rows > out_rows_avail)
      num_rows = out_rows_avail;
    /* Create output pointer array for upsampler. */
    work_ptrs[0] = output_buf[*out_row_ctr];
    if(num_rows > 1) {
      work_ptrs[1] = output_buf[*out_row_ctr + 1];
			}
		else {
      work_ptrs[1] = spareRow;
      spareFull = TRUE;
			}
    /* Now do the upsampling. */
    (this->*upmethod) (input_buf, *in_row_group_ctr, work_ptrs);
		}

  /* Adjust counts */
  *out_row_ctr += num_rows;
  rowsToGo -= num_rows;
  /* When the buffer is emptied, declare this input row group consumed */
  if(!spareFull)
    (*in_row_group_ctr)++;
	}


void CUpsampler::merged_1v_upsample(JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	JDIMENSION in_row_groups_avail,
	JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	JDIMENSION out_rows_avail) {
/* 1:1 vertical sampling case: much easier, never need a spare row. */

  /* Just do the upsampling. */
  (this->*upmethod) (input_buf, *in_row_group_ctr, output_buf + *out_row_ctr);
  /* Adjust counts */
  (*out_row_ctr)++;
  (*in_row_group_ctr)++;
	}


/*
 * These are the routines invoked by the control routines to do
 * the actual upsampling/conversion.  One row group is processed per call.
 *
 * Note: since we may be writing directly into application-supplied buffers,
 * we have to be honest about the output width; we can't assume the buffer
 * has been rounded up to an even width.
 */


/*
 * Upsample and color convert for the case of 2:1 horizontal and 1:1 vertical.
 */

void CUpsampler::h2v1_merged_upsample(JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
	JSAMPARRAY output_buf) {

  register int y, cred, cgreen, cblue;
  int cb, cr;
  register JSAMPROW outptr;
  JSAMPROW inptr0, inptr1, inptr2;
  JDIMENSION col;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = m_Parent->sampleRangeLimit;
  int *Crrtab = Cr_r_tab;
  int *Cbbtab = Cb_b_tab;
  INT32 *Crgtab = Cr_g_tab;
  INT32 *Cbgtab = Cb_g_tab;
  SHIFT_TEMPS

  inptr0 = input_buf[0][in_row_group_ctr];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr = output_buf[0];
  /* Loop for each pair of output pixels */
  for(col = m_Parent->outputWidth >> 1; col > 0; col--) {
    /* Do the chroma part of the calculation */
    cb = GETJSAMPLE(*inptr1++);
    cr = GETJSAMPLE(*inptr2++);
    cred = Crrtab[cr];
    cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
    cblue = Cbbtab[cb];
    /* Fetch 2 Y values and emit 2 pixels */
    y  = GETJSAMPLE(*inptr0++);
    outptr[RGB_RED] =   range_limit[y + cred];
    outptr[RGB_GREEN] = range_limit[y + cgreen];
    outptr[RGB_BLUE] =  range_limit[y + cblue];
    outptr += RGB_PIXELSIZE;
    y  = GETJSAMPLE(*inptr0++);
    outptr[RGB_RED] =   range_limit[y + cred];
    outptr[RGB_GREEN] = range_limit[y + cgreen];
    outptr[RGB_BLUE] =  range_limit[y + cblue];
    outptr += RGB_PIXELSIZE;
	  }
  /* If image width is odd, do the last output column separately */
  if(m_Parent->outputWidth & 1) {
    cb = GETJSAMPLE(*inptr1);
    cr = GETJSAMPLE(*inptr2);
    cred = Crrtab[cr];
    cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
    cblue = Cbbtab[cb];
    y  = GETJSAMPLE(*inptr0);
    outptr[RGB_RED] =   range_limit[y + cred];
    outptr[RGB_GREEN] = range_limit[y + cgreen];
    outptr[RGB_BLUE] =  range_limit[y + cblue];
	  }
	}


void CUpsampler::h2v1_merged_upsample_gray(JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
	JSAMPARRAY output_buf) {

  register int y, cred, cgreen, cblue;
  int cb, cr;
  register JSAMPROW outptr;
  JSAMPROW inptr0, inptr1, inptr2;
  JDIMENSION col;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = m_Parent->sampleRangeLimit;
  int *Crrtab = Cr_r_tab;
  int *Cbbtab = Cb_b_tab;
  INT32 *Crgtab = Cr_g_tab;
  INT32 *Cbgtab = Cb_g_tab;
  SHIFT_TEMPS

  inptr0 = input_buf[0][in_row_group_ctr];
  outptr = output_buf[0];
  /* Loop for each pair of output pixels */
  for(col=m_Parent->outputWidth >> 1; col > 0; col--) {
    /* Do the chroma part of the calculation */
//    cb = GETJSAMPLE(*inptr1++);
//    cr = GETJSAMPLE(*inptr2++);
//    cred = Crrtab[cr];
//    cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
//    cblue = Cbbtab[cb];
    /* Fetch 2 Y values and emit 2 pixels */
    y  = GETJSAMPLE(*inptr0++);
    outptr[RGB_RED] =   range_limit[y];
    outptr[RGB_GREEN] = range_limit[y];
    outptr[RGB_BLUE] =  range_limit[y];
    outptr += 1;
    y  = GETJSAMPLE(*inptr0++);
//    outptr[RGB_RED] =   range_limit[y];
//    outptr[RGB_GREEN] = range_limit[y];
//    outptr[RGB_BLUE] =  range_limit[y];
//    outptr += RGB_PIXELSIZE;
	  }
  /* If image width is odd, do the last output column separately */
  if(m_Parent->outputWidth & 1) {
//    cb = GETJSAMPLE(*inptr1);
//    cr = GETJSAMPLE(*inptr2);
//    cred = Crrtab[cr];
//    cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
//    cblue = Cbbtab[cb];
    y  = GETJSAMPLE(*inptr0);
    outptr[RGB_RED] =   range_limit[y];
    outptr[RGB_GREEN] = range_limit[y];
    outptr[RGB_BLUE] =  range_limit[y];
	  }
	}

/*
 * Upsample and color convert for the case of 2:1 horizontal and 2:1 vertical.
 */

void CUpsampler::h2v2_merged_upsample(JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
	JSAMPARRAY output_buf) {
  register int y, cred, cgreen, cblue;
  int cb, cr;
  register JSAMPROW outptr0, outptr1;
  JSAMPROW inptr00, inptr01, inptr1, inptr2;
  JDIMENSION col;
  /* copy these pointers into registers if possible */
  register JSAMPLE *range_limit = m_Parent->sampleRangeLimit;
  int *Crrtab = Cr_r_tab;
  int *Cbbtab = Cb_b_tab;
  INT32 *Crgtab = Cr_g_tab;
  INT32 *Cbgtab = Cb_g_tab;
  SHIFT_TEMPS

  inptr00 = input_buf[0][in_row_group_ctr*2];
  inptr01 = input_buf[0][in_row_group_ctr*2 + 1];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr0 = output_buf[0];
  outptr1 = output_buf[1];
  /* Loop for each group of output pixels */
  for(col = m_Parent->outputWidth >> 1; col > 0; col--) {
    /* Do the chroma part of the calculation */
    cb = GETJSAMPLE(*inptr1++);
    cr = GETJSAMPLE(*inptr2++);
    cred = Crrtab[cr];
    cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
    cblue = Cbbtab[cb];
    /* Fetch 4 Y values and emit 4 pixels */
    y  = GETJSAMPLE(*inptr00++);
    outptr0[RGB_RED] =   range_limit[y + cred];
    outptr0[RGB_GREEN] = range_limit[y + cgreen];
    outptr0[RGB_BLUE] =  range_limit[y + cblue];
    outptr0 += RGB_PIXELSIZE;
    y  = GETJSAMPLE(*inptr00++);
    outptr0[RGB_RED] =   range_limit[y + cred];
    outptr0[RGB_GREEN] = range_limit[y + cgreen];
    outptr0[RGB_BLUE] =  range_limit[y + cblue];
    outptr0 += RGB_PIXELSIZE;
    y  = GETJSAMPLE(*inptr01++);
    outptr1[RGB_RED] =   range_limit[y + cred];
    outptr1[RGB_GREEN] = range_limit[y + cgreen];
    outptr1[RGB_BLUE] =  range_limit[y + cblue];
    outptr1 += RGB_PIXELSIZE;
    y  = GETJSAMPLE(*inptr01++);
    outptr1[RGB_RED] =   range_limit[y + cred];
    outptr1[RGB_GREEN] = range_limit[y + cgreen];
    outptr1[RGB_BLUE] =  range_limit[y + cblue];
    outptr1 += RGB_PIXELSIZE;
		}
  /* If image width is odd, do the last output column separately */
  if(m_Parent->outputWidth & 1) {
    cb = GETJSAMPLE(*inptr1);
    cr = GETJSAMPLE(*inptr2);
    cred = Crrtab[cr];
    cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
    cblue = Cbbtab[cb];
    y  = GETJSAMPLE(*inptr00);
    outptr0[RGB_RED] =   range_limit[y + cred];
    outptr0[RGB_GREEN] = range_limit[y + cgreen];
    outptr0[RGB_BLUE] =  range_limit[y + cblue];
    y  = GETJSAMPLE(*inptr01);
    outptr1[RGB_RED] =   range_limit[y + cred];
    outptr1[RGB_GREEN] = range_limit[y + cgreen];
    outptr1[RGB_BLUE] =  range_limit[y + cblue];
		}
	}




/*
 * Finish JPEG decompression.
 *
 * This will normally just verify the file trailer and release temp storage.
 *
 * Returns FALSE if suspended.  The return value need be inspected only if
 * a suspending data source is used.
 */

CDecompressJpeg::finishDecompress() {

  if((globalState == DSTATE_SCANNING ||
    globalState == DSTATE_RAW_OK) && ! bufferedImage) {
    /* Terminate final pass of non-buffered mode */
    if(outputScanline < outputHeight)
      m_Parent->err->errorExit(JERR_TOO_LITTLE_DATA);
    master->finishOutputPass();
    globalState = DSTATE_STOPPING;
		}
	else if(globalState == DSTATE_BUFIMAGE) {
    /* Finishing after a buffered-image operation */
    globalState = DSTATE_STOPPING;
		}
	else if(globalState != DSTATE_STOPPING) {
    /* STOPPING = repeat call after a suspension, anything else is error */
    m_Parent->err->errorExit(JERR_BAD_STATE, globalState);
	  }
  /* Read until EOI */
  while(!inputctl->EOI_Reached) {
    if((inputctl->*inputctl->doConsumeInput) () == J_SUSPENDED)
      return FALSE;		/* Suspend, come back later */
		}
  /* Do final cleanup */
  src->termSource();
  /* We can use jpeg_abort to release memory and reset global_state */
  abort();
  return TRUE;
	}


CDPostController::CDPostController(CDecompressJpeg *p,int n) : m_Parent(p) {

  wholeImage = NULL;	/* flag for no virtual arrays */
  buffer = NULL;		/* flag for no strip buffer */
	stripHeight=0;
  startingRow = nextRow = 0;
	}

void CDPostController::startPass(J_BUF_MODE pass_mode) {

  switch(pass_mode) {
	  case JBUF_PASS_THRU:
		  if(m_Parent->quantizeColors) {
      /* Single-pass processing with color quantization. */
				doPostProcessData = postProcess1Pass;
      /* We could be doing buffered-image output before starting a 2-pass
       * color quantization; in that case, jinit_d_post_controller did not
       * allocate a strip buffer.  Use the virtual-array buffer as workspace.
       */
				if(buffer == NULL) {
					buffer = wholeImage->accessVirtSarray((JDIMENSION) 0, stripHeight, TRUE);
					}
				}
			else {
				/* For single-pass processing without color quantization,
				 * I have no work to do; just call the upsampler directly.
				 */
				doPostProcessData = wrapUpsample;
				}
	    break;
#ifdef QUANT_2PASS_SUPPORTED
		case JBUF_SAVE_AND_PASS:
			/* First pass of 2-pass quantization */
			if(wholeImage == NULL)
				m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			doPostProcessData = postProcessPrepass;
			break;
		case JBUF_CRANK_DEST:
			/* Second pass of 2-pass quantization */
			if(wholeImage == NULL)
				m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			doPostProcessData = postProcess2Pass;
			break;
#endif /* QUANT_2PASS_SUPPORTED */

		default:
			m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
			break;
		}

  startingRow = nextRow = 0;
	}


/*
 * Initialize for a processing pass.
 */

void CDPostController::startPassDPost(J_BUF_MODE pass_mode) {

  switch (pass_mode) {
  case JBUF_PASS_THRU:
    if(m_Parent->quantizeColors) {
      /* Single-pass processing with color quantization. */
      doPostProcessData = postProcess1Pass;
      /* We could be doing buffered-image output before starting a 2-pass
       * color quantization; in that case, jinit_d_post_controller did not
       * allocate a strip buffer.  Use the virtual-array buffer as workspace.
       */
      if(buffer == NULL) {
				buffer = wholeImage->accessVirtSarray((JDIMENSION) 0, stripHeight, TRUE);
				}
			}
		else {
      /* For single-pass processing without color quantization,
       * I have no work to do; just call the upsampler directly.
       */
      doPostProcessData = wrapUpsample;
			}
    break;
#ifdef QUANT_2PASS_SUPPORTED
  case JBUF_SAVE_AND_PASS:
    /* First pass of 2-pass quantization */
    if(wholeImage == NULL)
      m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
    doPostProcessData = postProcessPrepass;
    break;
  case JBUF_CRANK_DEST:
    /* Second pass of 2-pass quantization */
    if(wholeImage == NULL)
      m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
		doPostProcessData = postProcess2Pass;
    break;
#endif /* QUANT_2PASS_SUPPORTED */
  default:
    m_Parent->m_Parent->err->errorExit(JERR_BAD_BUFFER_MODE);
    break;
		}
  startingRow = nextRow = 0;
	}


/*
 * Process some data in the one-pass (strip buffer) case.
 * This is used for color precision reduction as well as one-pass quantization.
 */

void CDPostController::postProcess1Pass(JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	JDIMENSION in_row_groups_avail,
	JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	JDIMENSION out_rows_avail) {

  JDIMENSION num_rows, max_rows;

  /* Fill the buffer, but not more than what we can dump out in one go. */
  /* Note we rely on the upsampler to detect bottom of image. */
  max_rows = out_rows_avail - *out_row_ctr;
  if(max_rows > stripHeight)
    max_rows = stripHeight;
  num_rows = 0;
  (m_Parent->upsample->*m_Parent->upsample->doUpsample) (input_buf, 
		in_row_group_ctr, in_row_groups_avail, buffer, &num_rows, max_rows);
  /* Quantize and emit data. */
  (m_Parent->cquantize->*m_Parent->cquantize->doColorQuantize) (buffer, 
		output_buf + *out_row_ctr, (int) num_rows);
  *out_row_ctr += num_rows;
	}


#ifdef QUANT_2PASS_SUPPORTED

/*
 * Process some data in the first pass of 2-pass quantization.
 */

void CDPostController::postProcessPrepass(JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	JDIMENSION in_row_groups_avail,
	JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	JDIMENSION out_rows_avail) {

  JDIMENSION old_next_row, num_rows;

  /* Reposition virtual buffer if at start of strip. */
  if(nextRow == 0) {
    buffer = (*cinfo->mem->access_virt_sarray)
		(wholeImage, startingRow, stripHeight, TRUE);
		}

  /* Upsample some data (up to a strip height's worth). */
  old_next_row = nextRow;
  (*m_Parent->upsample->doUpsample) (input_buf, in_row_group_ctr, in_row_groups_avail,
		buffer, &nextRow, stripHeight);

  /* Allow quantizer to scan new data.  No data is emitted, */
  /* but we advance out_row_ctr so outer loop can tell when we're done. */
  if(nextRow > old_next_row) {
    num_rows = nextRow - old_next_row;
    (*m_Parent->cquantize->doColorQuantize) (buffer + old_next_row,
			(JSAMPARRAY) NULL, (int) num_rows);
    *out_row_ctr += num_rows;
		}

  /* Advance if we filled the strip. */
  if(nextRow >= stripHeight) {
    startingRow += stripHeight;
    nextRow = 0;
		}
	}


/*
 * Process some data in the second pass of 2-pass quantization.
 */

void CDPostController::postProcess2Pass(JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	JDIMENSION in_row_groups_avail,
	JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	JDIMENSION out_rows_avail) {

  JDIMENSION num_rows, max_rows;

  /* Reposition virtual buffer if at start of strip. */
  if(nextRow == 0) {
    buffer = (*cinfo->mem->access_virt_sarray)
			(wholeImage, startingRow, stripHeight, FALSE);
		}

  /* Determine number of rows to emit. */
  num_rows = stripHeight - nextRow; /* available in strip */
  max_rows = out_rows_avail - *out_row_ctr; /* available in output area */
  if(num_rows > max_rows)
    num_rows = max_rows;
  /* We have to check bottom of image here, can't depend on upsampler. */
  max_rows = m_Parent->outputHeight - starting_row;
  if(num_rows > max_rows)
    num_rows = max_rows;

  /* Quantize and emit data. */
  (*m_Parent->cquantize->doColorQuantize) (buffer + next_row, output_buf + *out_row_ctr,
		(int) num_rows);
  *out_row_ctr += num_rows;

  /* Advance if we filled the strip. */
  nextRow += num_rows;
  if(nextRow >= stripHeight) {
    startingRow += stripHeight;
    nextRow = 0;
		}
	}

#endif /* QUANT_2PASS_SUPPORTED */


void CDPostController::wrapUpsample(JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	JDIMENSION in_row_groups_avail,	JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	JDIMENSION out_rows_avail) {

	// patch xche' non posso cast-are ...
	(m_Parent->upsample->*m_Parent->upsample->doUpsample)(input_buf, in_row_group_ctr,
		in_row_groups_avail,output_buf, out_row_ctr,out_rows_avail);
	}



/*
 * jdmarker.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains routines to decode JPEG datastream markers.
 * Most of the complexity arises from our desire to support input
 * suspension: if not all of the data for a marker is available,
 * we must exit back to the application.  On resumption, we reprocess
 * the marker.
 */
/*
 * Initialize the marker reader module.
 * This is called only once, when the decompression object is created.
 */

CMarkerReader::CMarkerReader(CDecompressJpeg *p,CSourceMgr *s) : m_Parent(p) {
  int i;

	if(s)
		src=s;
	else
		src=p->src;

  process_COM = skipVariable;
  for(i=0; i<16; i++)
    process_APPn[i] = skipVariable;
  process_APPn[0] = getApp0;
  process_APPn[1] = getApp1;
  process_APPn[14] = getApp14;
  /* Reset marker processing state */
  resetMarkerReader();

	}



/*
 * Routines to process JPEG markers.
 *
 * Entry condition: JPEG marker itself has been read and its code saved
 *   in cinfo->unread_marker; input restart point is just after the marker.
 *
 * Exit: if return TRUE, have read and processed any parameters, and have
 *   updated the restart point to point after the parameters.
 *   If return FALSE, was forced to suspend before reaching end of
 *   marker parameters; restart point has not been moved.  Same routine
 *   will be called again after application supplies more input data.
 *
 * This approach to suspension assumes that all of a marker's parameters can
 * fit into a single input bufferload.  This should hold for "normal"
 * markers.  Some COM/APPn markers might have large parameter segments,
 * but we use skip_input_data to get past those, and thereby put the problem
 * on the source manager's shoulders.
 *
 * Note that we don't bother to avoid duplicate trace messages if a
 * suspension occurs within marker parameters.  Other side effects
 * require more care.
 */



/*
 * Macros for fetching data from the data source module.
 *
 * At all times, cinfo->src->next_input_byte and ->bytes_in_buffer reflect
 * the current restart point; we update them only when we have reached a
 * suitable place to restart if a suspension occurs.
 */

/* Declare and initialize local copies of input pointer/count */
#define INPUT_VARS()  \
	const JOCTET *next_input_byte = src->nextInputByte;  \
	size_t bytes_in_buffer = src->bytesInBuffer

/* Unload the local copies --- do this only at a restart boundary */
#define INPUT_SYNC()  \
	( src->nextInputByte = next_input_byte,  \
	  src->bytesInBuffer = bytes_in_buffer )

/* Reload the local copies --- seldom used except in MAKE_BYTE_AVAIL */
#define INPUT_RELOAD()  \
	( next_input_byte = src->nextInputByte,  \
	  bytes_in_buffer = src->bytesInBuffer )

/* Internal macro for INPUT_BYTE and INPUT_2BYTES: make a byte available.
 * Note we do *not* do INPUT_SYNC before calling fill_input_buffer,
 * but we must reload the local copies after a successful fill.
 */
#define MAKE_BYTE_AVAIL(action)  \
	if(bytes_in_buffer  == 0) {  \
	  if (! (src->fillInputBuffer) ())  \
	    { action; }  \
	  INPUT_RELOAD();  \
	}  \
	bytes_in_buffer--

/* Read a byte into variable V.
 * If must suspend, take the specified action (typically "return FALSE").
 */
#define INPUT_BYTE(V,action)  \
	{ MAKE_BYTE_AVAIL(action); \
		V = GETJOCTET(*next_input_byte++); }

/* As above, but read two bytes interpreted as an unsigned 16-bit integer.
 * V should be declared unsigned int or perhaps INT32.
 */
#define INPUT_2BYTES(V,action)  \
	{ MAKE_BYTE_AVAIL(action); \
		  V = ((unsigned int) GETJOCTET(*next_input_byte++)) << 8; \
		  MAKE_BYTE_AVAIL(action); \
			V += GETJOCTET(*next_input_byte++); }



BOOL CMarkerReader::getSOI()
/* Process an SOI marker */
{
  int i;
  
//  TRACEMS(cinfo, 1, JTRC_SOI);

  if(saw_SOI)
    m_Parent->m_Parent->err->errorExit(JERR_SOI_DUPLICATE);

  /* Reset all parameters that are defined to be reset by SOI */

  for(i=0; i < NUM_ARITH_TBLS; i++) {
    m_Parent->arith_dc_L[i] = 0;
    m_Parent->arith_dc_U[i] = 1;
    m_Parent->arith_ac_K[i] = 5;
		}
  m_Parent->restartInterval = 0;

  /* Set initial assumptions for colorspace etc */

  m_Parent->colorSpace = JCS_UNKNOWN;
	m_Parent->CCIR601Sampling = FALSE; /* Assume non-CCIR sampling??? */

  m_Parent->saw_JFIF_marker = FALSE;
  m_Parent->densityUnit = 0;	/* set default JFIF APP0 values */
  m_Parent->XDensity = 1;
  m_Parent->YDensity = 1;
  m_Parent->sawAdobeMarker = FALSE;
  m_Parent->AdobeTransform = 0;

  saw_SOI = TRUE;

  return TRUE;
	}


BOOL CMarkerReader::getSOF(BOOL isProg, BOOL isArith)
/* Process a SOFn marker */
{
  INT32 length;
  int c, ci;
  J_COMPONENT_INFO *compptr;
  INPUT_VARS();

  m_Parent->progressiveMode = isProg;
  m_Parent->arithCode = isArith;

  INPUT_2BYTES(length, return FALSE);

  INPUT_BYTE(m_Parent->dataPrecision, return FALSE);
  INPUT_2BYTES(m_Parent->imageHeight, return FALSE);
  INPUT_2BYTES(m_Parent->imageWidth, return FALSE);
  INPUT_BYTE(m_Parent->numComponents, return FALSE);

  length -= 8;

/*  TRACEMS4(cinfo, 1, JTRC_SOF, cinfo->unread_marker,
	   (int) cinfo->image_width, (int) cinfo->image_height,
	   num_components);*/

  if(saw_SOF)
    m_Parent->m_Parent->err->errorExit(JERR_SOF_DUPLICATE);

  /* We don't support files in which the image height is initially specified */
  /* as 0 and is later redefined by DNL.  As long as we have to check that,  */
  /* might as well have a general sanity check. */
  if(m_Parent->imageHeight <= 0 || m_Parent->imageWidth <= 0
    || m_Parent->numComponents <= 0)
    m_Parent->m_Parent->err->errorExit(JERR_EMPTY_IMAGE);

  if(length != (m_Parent->numComponents * 3))
    m_Parent->m_Parent->err->errorExit(JERR_BAD_LENGTH);

  if(m_Parent->compInfo == NULL)	// do only once, even if suspend
    m_Parent->compInfo = (J_COMPONENT_INFO *) (m_Parent->m_Parent->mem->allocSmall)
			(JPOOL_IMAGE, m_Parent->numComponents * sizeof(J_COMPONENT_INFO));
  
  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    compptr->componentIndex = ci;
    INPUT_BYTE(compptr->componentId, return FALSE);
    INPUT_BYTE(c, return FALSE);
    compptr->hSampFactor= (c >> 4) & 15;
    compptr->vSampFactor = (c     ) & 15;
    INPUT_BYTE(compptr->quant_tbl_no, return FALSE);

/*    TRACEMS4(cinfo, 1, JTRC_SOF_COMPONENT,
	     compptr->component_id, compptr->h_samp_factor,
	     compptr->v_samp_factor, compptr->quant_tbl_no);*/
		}

  saw_SOF = TRUE;

  INPUT_SYNC();
  return TRUE;
	}


BOOL CMarkerReader::getSOS()
/* Process a SOS marker */
{
  INT32 length;
  int i, ci, n, c, cc;
  J_COMPONENT_INFO * compptr;
  INPUT_VARS();

  if(!saw_SOF)
    m_Parent->m_Parent->err->errorExit(JERR_SOS_NO_SOF);

  INPUT_2BYTES(length, return FALSE);

  INPUT_BYTE(n, return FALSE); /* Number of components */

  if(length != (n * 2 + 6) || n < 1 || n > MAX_COMPS_IN_SCAN)
    m_Parent->m_Parent->err->errorExit(JERR_BAD_LENGTH);

//  TRACEMS1(cinfo, 1, JTRC_SOS, n);

  m_Parent->compsInScan = n;

  /* Collect the component-spec parameters */

  for(i=0; i < n; i++) {
    INPUT_BYTE(cc, return FALSE);
    INPUT_BYTE(c, return FALSE);
    
    for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents;	ci++, compptr++) {
      if(cc == compptr->componentId)
				goto id_found;
			}

    m_Parent->m_Parent->err->errorExit(JERR_BAD_COMPONENT_ID, cc);

id_found:

    m_Parent->curCompInfo[i] = compptr;
    compptr->dc_tbl_no = (c >> 4) & 15;
    compptr->ac_tbl_no = (c     ) & 15;
    
//    TRACEMS3(cinfo, 1, JTRC_SOS_COMPONENT, cc,
//	     compptr->dc_tbl_no, compptr->ac_tbl_no);
  }

  /* Collect the additional scan parameters Ss, Se, Ah/Al. */
  INPUT_BYTE(c, return FALSE);
  m_Parent->Ss = c;
  INPUT_BYTE(c, return FALSE);
  m_Parent->Se = c;
  INPUT_BYTE(c, return FALSE);
  m_Parent->Ah = (c >> 4) & 15;
  m_Parent->Al = (c     ) & 15;

//  TRACEMS4(cinfo, 1, JTRC_SOS_PARAMS, cinfo->Ss, cinfo->Se,
//	   cinfo->Ah, cinfo->Al);

  /* Prepare to scan data & restart markers */
  nextRestartNum = 0;

  /* Count another SOS marker */
  m_Parent->inputScanNumber++;

  INPUT_SYNC();
  return TRUE;
	}


BOOL CMarkerReader::getApp0()
/* Process an APP0 marker */
{
  INT32 length;
  UINT8 b[JFIF_LEN];
  INPUT_VARS();

  INPUT_2BYTES(length, return FALSE);
  length -= 2;

  /* See if a JFIF APP0 marker is present */

  if(length >= JFIF_LEN) {
    for(int buffp=0; buffp < JFIF_LEN; buffp++)
      INPUT_BYTE(b[buffp], return FALSE);
    length -= JFIF_LEN;

    if(b[0]==0x4A && b[1]==0x46 && b[2]==0x49 && b[3]==0x46 && b[4]==0) {
      /* Found JFIF APP0 marker: check version */
      /* Major version must be 1, anything else signals an incompatible change.
       * We used to treat this as an error, but now it's a nonfatal warning,
       * because some bozo at Hijaak couldn't read the spec.
       * Minor version should be 0..2, but process anyway if newer.
       */
//      if(b[5] != 1)
//				WARNMS2(JWRN_JFIF_MAJOR, b[5], b[6]);
//      else if (b[6] > 2)
//				TRACEMS2(1, JTRC_JFIF_MINOR, b[5], b[6]);

      /* Save info */
      m_Parent->saw_JFIF_marker = TRUE;
      m_Parent->densityUnit = b[7];
      m_Parent->XDensity = (b[8] << 8) + b[9];
      m_Parent->YDensity = (b[10] << 8) + b[11];
//      TRACEMS3(1, JTRC_JFIF,
//	       XDensity, YDensity, densityUnit);
//      if(b[12] | b[13])
//				TRACEMS2(1, JTRC_JFIF_THUMBNAIL, b[12], b[13]);
//      if(length != ((INT32) b[12] * (INT32) b[13] * (INT32) 3))
//				TRACEMS1(1, JTRC_JFIF_BADTHUMBNAILSIZE, (int) length);
			} 
		else {
      /* Start of APP0 does not match "JFIF" */
//      TRACEMS1(1, JTRC_APP0, (int) length + JFIF_LEN);
			}
		} 
	else {
			/* Too short to be JFIF marker */
//    TRACEMS1(1, JTRC_APP0, (int)length);
	  }

  INPUT_SYNC();
  if(length > 0)		/* skip any remaining data -- could be lots */
    (src->skipInputData) ((long)length);

  return TRUE;
	}

BOOL CMarkerReader::getApp1() {
/* Process an APP1 marker (diciamo Exif) */
  INT32 length;
  INPUT_VARS();
  UINT8 b[1024];			// TUTTO un header EXIF!

  INPUT_2BYTES(length, return FALSE);
  length -= 2;
ASSERT(0);

/*{
	char buf[64];
wsprintf(buf,"len=%u",length);
AfxMessageBox(buf);
}*/

  if(length >= 4) {
    for(int buffp=0; buffp<4; buffp++)
      INPUT_BYTE(b[buffp], return FALSE);
    length -= 4;

		if(b[0]=='E' && b[1]=='x' && b[2]=='i' && b[3]=='f') {
			// gestire se si vuole
			}
		}
/*{
	char buf[64];
wsprintf(buf,"len2=%u,b=%c%c%c%c",length,b[0],b[1],b[2],b[3]);
AfxMessageBox(buf);
}*/

	while(length--) {
		//esaurisco cmq i dati, per non finire nello "skip" sotto (che darebbe warning)
    INPUT_BYTE(b[0], return FALSE);
		}

/*  if(length >= EXIF_LEN) {
    for(int buffp=0; buffp < EXIF_LEN; buffp++)
      INPUT_BYTE(b[buffp], return FALSE);
    length -= EXIF_LEN;

		if(b[0]=='E' && b[1]=='x' && b[2]=='i' && b[2]=='f') {
			}
		}*/

  INPUT_SYNC();
  if(length > 0)		/* skip any remaining data -- could be lots */
    (src->skipInputData) ((long)length);
  return TRUE;
	}

BOOL CMarkerReader::getApp14() {
/* Process an APP14 marker */
#define ADOBE_LEN 12
  INT32 length;
  UINT8 b[ADOBE_LEN];
  int buffp;
  unsigned int version, flags0, flags1, transform;
  INPUT_VARS();

  INPUT_2BYTES(length, return FALSE);
  length -= 2;

  /* See if an Adobe APP14 marker is present */

  if(length >= ADOBE_LEN) {
    for(buffp=0; buffp < ADOBE_LEN; buffp++)
      INPUT_BYTE(b[buffp], return FALSE);
    length -= ADOBE_LEN;

    if(b[0]=='A' && b[1]=='d' && b[2]=='o' && b[3]=='b' && b[4]=='e') {
      /* Found Adobe APP14 marker */
      version = (b[5] << 8) + b[6];
      flags0 = (b[7] << 8) + b[8];
      flags1 = (b[9] << 8) + b[10];
      transform = b[11];
//      TRACEMS4(cinfo, 1, JTRC_ADOBE, version, flags0, flags1, transform);
	    m_Parent->sawAdobeMarker = TRUE;
      m_Parent->AdobeTransform = (UINT8)transform;
			} 
		else {
      /* Start of APP14 does not match "Adobe" */
//      TRACEMS1(cinfo, 1, JTRC_APP14, (int) length + ADOBE_LEN);
			}
		} 
	else {
    /* Too short to be Adobe marker */
//    TRACEMS1(cinfo, 1, JTRC_APP14, (int) length);
		}

  INPUT_SYNC();
  if(length > 0)		/* skip any remaining data -- could be lots */
    (src->skipInputData) ((long) length);

  return TRUE;
	}


BOOL CMarkerReader::getDAC()
/* Process a DAC marker */
{
  INT32 length;
  int index, val;
  INPUT_VARS();

  INPUT_2BYTES(length, return FALSE);
  length -= 2;
  
  while (length > 0) {
    INPUT_BYTE(index, return FALSE);
    INPUT_BYTE(val, return FALSE);

    length -= 2;

//    TRACEMS2(cinfo, 1, JTRC_DAC, index, val);

    if(index < 0 || index >= (2*NUM_ARITH_TBLS))
      m_Parent->m_Parent->err->errorExit(JERR_DAC_INDEX, index);

    if(index >= NUM_ARITH_TBLS) { /* define AC table */
      m_Parent->arith_ac_K[index-NUM_ARITH_TBLS] = (UINT8) val;
			}
		else {			/* define DC table */
      m_Parent->arith_dc_L[index] = (UINT8) (val & 0x0F);
      m_Parent->arith_dc_U[index] = (UINT8) (val >> 4);
      if(m_Parent->arith_dc_L[index] > m_Parent->arith_dc_U[index])
				m_Parent->m_Parent->err->errorExit(JERR_DAC_VALUE, val);
			}
		}

  INPUT_SYNC();
  return TRUE;
	}


BOOL CMarkerReader::getDHT()
/* Process a DHT marker */
{
  INT32 length;
  UINT8 bits[17];
  UINT8 huffval[256];
  int i, index, count;
  JHUFF_TBL **htblptr;
  INPUT_VARS();

  INPUT_2BYTES(length, return FALSE);
  length -= 2;
  
  while (length > 0) {
    INPUT_BYTE(index, return FALSE);

//    TRACEMS1(cinfo, 1, JTRC_DHT, index);
      
    bits[0] = 0;
    count = 0;
    for (i = 1; i <= 16; i++) {
      INPUT_BYTE(bits[i], return FALSE);
      count += bits[i];
    }

    length -= 1 + 16;

//    TRACEMS8(cinfo, 2, JTRC_HUFFBITS,
//	     bits[1], bits[2], bits[3], bits[4],
//	     bits[5], bits[6], bits[7], bits[8]);
//    TRACEMS8(cinfo, 2, JTRC_HUFFBITS,
//	     bits[9], bits[10], bits[11], bits[12],
//	     bits[13], bits[14], bits[15], bits[16]);

    if(count > 256 || ((INT32) count) > length)
      m_Parent->m_Parent->err->errorExit(JERR_DHT_COUNTS);

    for(i=0; i < count; i++)
      INPUT_BYTE(huffval[i], return FALSE);

    length -= count;

    if(index & 0x10) {		/* AC table definition */
      index -= 0x10;
      htblptr = &m_Parent->AC_HuffTblPtrs[index];
			}
		else {			/* DC table definition */
      htblptr = &m_Parent->DC_HuffTblPtrs[index];
			}

    if(index < 0 || index >= NUM_HUFF_TBLS)
      m_Parent->m_Parent->err->errorExit(JERR_DHT_INDEX, index);

    if(*htblptr == NULL)
      *htblptr = m_Parent->allocHuffTable();
  
    memcpy((*htblptr)->bits, bits, sizeof((*htblptr)->bits));
    memcpy((*htblptr)->huffval, huffval, sizeof((*htblptr)->huffval));
		}

  INPUT_SYNC();
  return TRUE;
	}


BOOL CMarkerReader::getDQT()
/* Process a DQT marker */
{
  INT32 length;
  int n, i, prec;
  unsigned int tmp;
  JQUANT_TBL *quant_ptr;
  INPUT_VARS();

  INPUT_2BYTES(length, return FALSE);
  length -= 2;

  while (length > 0) {
    INPUT_BYTE(n, return FALSE);
    prec = n >> 4;
    n &= 0x0F;

//    TRACEMS2(cinfo, 1, JTRC_DQT, n, prec);

    if(n >= NUM_QUANT_TBLS)
      m_Parent->m_Parent->err->errorExit(JERR_DQT_INDEX, n);
      
    if(m_Parent->quantTblPtrs[n] == NULL)
      m_Parent->quantTblPtrs[n] = m_Parent->allocQuantTable();
    quant_ptr = m_Parent->quantTblPtrs[n];

    for(i=0; i < DCTSIZE2; i++) {
      if(prec) {
				INPUT_2BYTES(tmp, return FALSE);
				}
      else {
				INPUT_BYTE(tmp, return FALSE);
				}
      /* We convert the zigzag-order table to natural array order. */
      quant_ptr->quantVal[m_Parent->naturalOrder[i]] = (UINT16) tmp;
	    }

    if(m_Parent->m_Parent->err->traceLevel >= 2) {
      for (i = 0; i < DCTSIZE2; i += 8) {
//				TRACEMS8(cinfo, 2, JTRC_QUANTVALS,
//					quant_ptr->quantVal[i],   quant_ptr->quantVal[i+1],
//					quant_ptr->quantVal[i+2], quant_ptr->quantVal[i+3],
//					quant_ptr->quantVal[i+4], quant_ptr->quantVal[i+5],
//					quant_ptr->quantVal[i+6], quant_ptr->quantVal[i+7]);
				}
			}

    length -= DCTSIZE2+1;
    if(prec) 
			length -= DCTSIZE2;
		}

  INPUT_SYNC();
  return TRUE;
	}


BOOL CMarkerReader::getDRI()
/* Process a DRI marker */
{
  INT32 length;
  unsigned int tmp;
  INPUT_VARS();

  INPUT_2BYTES(length, return FALSE);
  
  if(length != 4)
    m_Parent->m_Parent->err->errorExit(JERR_BAD_LENGTH);

  INPUT_2BYTES(tmp, return FALSE);

//  TRACEMS1(cinfo, 1, JTRC_DRI, tmp);

  m_Parent->restartInterval = tmp;

  INPUT_SYNC();
  return TRUE;
	}


BOOL CMarkerReader::skipVariable()
/* Skip over an unknown or uninteresting variable-length marker */
{
  INT32 length;
  INPUT_VARS();

  INPUT_2BYTES(length, return FALSE);
  
//  TRACEMS2(cinfo, 1, JTRC_MISC_MARKER, cinfo->unread_marker, (int) length);

  INPUT_SYNC();		/* do before skip_input_data */
  (src->skipInputData) ((long) length - 2L);

  return TRUE;
	}


/*
 * Find the next JPEG marker, save it in cinfo->unread_marker.
 * Returns FALSE if had to suspend before reaching a marker;
 * in that case cinfo->unread_marker is unchanged.
 *
 * Note that the result might not be a valid marker code,
 * but it will never be 0 or FF.
 */

BOOL CMarkerReader::nextMarker() {
  int c;
  INPUT_VARS();

  for(;;) {
    INPUT_BYTE(c, return FALSE);
    /* Skip any non-FF bytes.
     * This may look a bit inefficient, but it will not occur in a valid file.
     * We sync after each discarded byte so that a suspending data source
     * can discard the byte from its buffer.
     */
    while(c != 0xFF) {
      discardedBytes++;
      INPUT_SYNC();
      INPUT_BYTE(c, return FALSE);
			}
    /* This loop swallows any duplicate FF bytes.  Extra FFs are legal as
     * pad bytes, so don't count them in discarded_bytes.  We assume there
     * will not be so many consecutive FF bytes as to overflow a suspending
     * data source's input buffer.
     */
    do {
      INPUT_BYTE(c, return FALSE);
		  } while(c == 0xFF);
    if(c != 0)
      break;			/* found a valid marker, exit loop */
    /* Reach here if we found a stuffed-zero data sequence (FF/00).
     * Discard it and loop back to try again.
     */
    discardedBytes += 2;
    INPUT_SYNC();
	  }

  if(discardedBytes != 0) {
    m_Parent->m_Parent->err->warn(JWRN_EXTRANEOUS_DATA, discardedBytes, c);
    discardedBytes = 0;
		}

  m_Parent->unreadMarker = c;

  INPUT_SYNC();
  return TRUE;
	}


BOOL CMarkerReader::firstMarker()
/* Like next_marker, but used to obtain the initial SOI marker. */
/* For this marker, we do not allow preceding garbage or fill; otherwise,
 * we might well scan an entire input file before realizing it ain't JPEG.
 * If an application wants to process non-JFIF files, it must seek to the
 * SOI before calling the JPEG library.
 */
{
  int c, c2;
  INPUT_VARS();

  INPUT_BYTE(c, return FALSE);
  INPUT_BYTE(c2, return FALSE);
  if(c != 0xFF || c2 != (int) CMarkerReader::M_SOI)
    m_Parent->m_Parent->err->errorExit(JERR_NO_SOI, c, c2);

  m_Parent->unreadMarker = c2;

  INPUT_SYNC();
  return TRUE;
	}


/*
 * Read markers until SOS or EOI.
 *
 * Returns same codes as are defined for jpeg_consume_input:
 * JPEG_SUSPENDED, JPEG_REACHED_SOS, or JPEG_REACHED_EOI.
 */

int CMarkerReader::readMarkers() {

  /* Outer loop repeats once for each marker. */
  for(;;) {
    /* Collect the marker proper, unless we already did. */
    /* NB: first_marker() enforces the requirement that SOI appear first. */
    if(m_Parent->unreadMarker == 0) {
      if(!saw_SOI) {
				if(!firstMarker())
					return J_SUSPENDED;
				} 
			else {
				if(!nextMarker())
					return J_SUSPENDED;
				}
			}
    /* At this point cinfo->unread_marker contains the marker code and the
     * input point is just past the marker proper, but before any parameters.
     * A suspension will cause us to return with this state still true.
     */
		switch(m_Parent->unreadMarker) {
			case CMarkerReader::M_SOI:
				if(!getSOI())
					return J_SUSPENDED;
				break;

			case CMarkerReader::M_SOF0:		/* Baseline */
			case CMarkerReader::M_SOF1:		/* Extended sequential, Huffman */
				if(!getSOF(FALSE, FALSE))
					return J_SUSPENDED;
				break;

			case CMarkerReader::M_SOF2:		/* Progressive, Huffman */
				if(! getSOF(TRUE, FALSE))
					return J_SUSPENDED;
				break;

			case CMarkerReader::M_SOF9:		/* Extended sequential, arithmetic */
				if(! getSOF(FALSE, TRUE))
					return J_SUSPENDED;
				break;

			case CMarkerReader::M_SOF10:		/* Progressive, arithmetic */
				if(! getSOF(TRUE, TRUE))
					return J_SUSPENDED;
				break;

			/* Currently unsupported SOFn types */
			case CMarkerReader::M_SOF3:		/* Lossless, Huffman */
			case CMarkerReader::M_SOF5:		/* Differential sequential, Huffman */
			case CMarkerReader::M_SOF6:		/* Differential progressive, Huffman */
			case CMarkerReader::M_SOF7:		/* Differential lossless, Huffman */
			case CMarkerReader::M_JPG:			/* Reserved for JPEG extensions */
			case CMarkerReader::M_SOF11:		/* Lossless, arithmetic */
			case CMarkerReader::M_SOF13:		/* Differential sequential, arithmetic */
			case CMarkerReader::M_SOF14:		/* Differential progressive, arithmetic */
			case CMarkerReader::M_SOF15:		/* Differential lossless, arithmetic */
				m_Parent->m_Parent->err->errorExit(JERR_SOF_UNSUPPORTED, m_Parent->unreadMarker);
				break;

			case CMarkerReader::M_SOS:
				if(! getSOS())
					return J_SUSPENDED;
				m_Parent->unreadMarker = 0;	/* processed the marker */
				return J_REACHED_SOS;
    
			case CMarkerReader::M_EOI:
	//      TRACEMS(cinfo, 1, JTRC_EOI);
				m_Parent->unreadMarker = 0;	/* processed the marker */
				return J_REACHED_EOI;
      
			case CMarkerReader::M_DAC:
				if(!getDAC())
					return J_SUSPENDED;
				break;
      
			case CMarkerReader::M_DHT:
				if(!getDHT())
					return J_SUSPENDED;
				break;
      
			case CMarkerReader::M_DQT:
				if(!getDQT())
					return J_SUSPENDED;
				break;
      
			case CMarkerReader::M_DRI:
				if(!getDRI())
					return J_SUSPENDED;
				break;
      
			case CMarkerReader::M_APP0:
			case CMarkerReader::M_APP1:
			case CMarkerReader::M_APP2:
			case CMarkerReader::M_APP3:
			case CMarkerReader::M_APP4:
			case CMarkerReader::M_APP5:
			case CMarkerReader::M_APP6:
			case CMarkerReader::M_APP7:
			case CMarkerReader::M_APP8:
			case CMarkerReader::M_APP9:
			case CMarkerReader::M_APP10:
			case CMarkerReader::M_APP11:
			case CMarkerReader::M_APP12:
			case CMarkerReader::M_APP13:
			case CMarkerReader::M_APP14:
			case CMarkerReader::M_APP15:
				if(!(this->*process_APPn[m_Parent->unreadMarker - (int) CMarkerReader::M_APP0]) ())
					return J_SUSPENDED;
				break;
      
			case CMarkerReader::M_COM:
				if(! (this->*process_COM) ())
					return J_SUSPENDED;
				break;

			case CMarkerReader::M_RST0:		/* these are all parameterless */
			case CMarkerReader::M_RST1:
			case CMarkerReader::M_RST2:
			case CMarkerReader::M_RST3:
			case CMarkerReader::M_RST4:
			case CMarkerReader::M_RST5:
			case CMarkerReader::M_RST6:
			case CMarkerReader::M_RST7:
			case CMarkerReader::M_TEM:
	//      TRACEMS1(cinfo, 1, JTRC_PARMLESS_MARKER, cinfo->unread_marker);
				break;

			case CMarkerReader::M_DNL:			/* Ignore DNL ... perhaps the wrong thing */
				if(!skipVariable())
					return J_SUSPENDED;
				break;

			default:			/* must be DHP, EXP, JPGn, or RESn */
				/* For now, we treat the reserved markers as fatal errors since they are
				 * likely to be used to signal incompatible JPEG Part 3 extensions.
				 * Once the JPEG 3 version-number marker is well defined, this code
				 * ought to change!
				 */
				m_Parent->m_Parent->err->errorExit(JERR_UNKNOWN_MARKER, m_Parent->unreadMarker);
				break;
		  }
    /* Successfully processed marker, so reset state variable */
    m_Parent->unreadMarker = 0;
	  } /* end loop */
	}


/*
 * Read a restart marker, which is expected to appear next in the datastream;
 * if the marker is not there, take appropriate recovery action.
 * Returns FALSE if suspension is required.
 *
 * This is called by the entropy decoder after it has read an appropriate
 * number of MCUs.  cinfo->unread_marker may be nonzero if the entropy decoder
 * has already read a marker from the data source.  Under normal conditions
 * cinfo->unread_marker will be reset to 0 before returning; if not reset,
 * it holds a marker which the decoder will be unable to read past.
 */

BOOL CDecompressJpeg::readRestartMarker() {

  /* Obtain a marker unless we already did. */
  /* Note that next_marker will complain if it skips any data. */
  if(unreadMarker == 0) {
    if(!marker->nextMarker())
      return FALSE;
		}

  if(unreadMarker == ((int) CMarkerReader::M_RST0 + marker->nextRestartNum)) {
    /* Normal case --- swallow the marker and let entropy decoder continue */
//    TRACEMS1(cinfo, 3, JTRC_RST, marker->nextRestartNum);
    unreadMarker = 0;
		} 
	else {
    /* Uh-oh, the restart markers have been messed up. */
    /* Let the data source manager determine how to resync. */
    if(!resyncToRestart( marker->nextRestartNum))
      return FALSE;
		}

  /* Update next-restart state */
  marker->nextRestartNum = (marker->nextRestartNum + 1) & 7;

  return TRUE;
	}


/*
 * This is the default resync_to_restart method for data source managers
 * to use if they don't have any better approach.  Some data source managers
 * may be able to back up, or may have additional knowledge about the data
 * which permits a more intelligent recovery strategy; such managers would
 * presumably supply their own resync method.
 *
 * read_restart_marker calls resync_to_restart if it finds a marker other than
 * the restart marker it was expecting.  (This code is *not* used unless
 * a nonzero restart interval has been declared.)  cinfo->unread_marker is
 * the marker code actually found (might be anything, except 0 or FF).
 * The desired restart marker number (0..7) is passed as a parameter.
 * This routine is supposed to apply whatever error recovery strategy seems
 * appropriate in order to position the input stream to the next data segment.
 * Note that cinfo->unread_marker is treated as a marker appearing before
 * the current data-source input point; usually it should be reset to zero
 * before returning.
 * Returns FALSE if suspension is required.
 *
 * This implementation is substantially constrained by wanting to treat the
 * input as a data stream; this means we can't back up.  Therefore, we have
 * only the following actions to work with:
 *   1. Simply discard the marker and let the entropy decoder resume at next
 *      byte of file.
 *   2. Read forward until we find another marker, discarding intervening
 *      data.  (In theory we could look ahead within the current bufferload,
 *      without having to discard data if we don't find the desired marker.
 *      This idea is not implemented here, in part because it makes behavior
 *      dependent on buffer size and chance buffer-boundary positions.)
 *   3. Leave the marker unread (by failing to zero cinfo->unread_marker).
 *      This will cause the entropy decoder to process an empty data segment,
 *      inserting dummy zeroes, and then we will reprocess the marker.
 *
 * #2 is appropriate if we think the desired marker lies ahead, while #3 is
 * appropriate if the found marker is a future restart marker (indicating
 * that we have missed the desired restart marker, probably because it got
 * corrupted).
 * We apply #2 or #3 if the found marker is a restart marker no more than
 * two counts behind or ahead of the expected one.  We also apply #2 if the
 * found marker is not a legal JPEG marker code (it's certainly bogus data).
 * If the found marker is a restart marker more than 2 counts away, we do #1
 * (too much risk that the marker is erroneous; with luck we will be able to
 * resync at some future point).
 * For any valid non-restart JPEG marker, we apply #3.  This keeps us from
 * overrunning the end of a scan.  An implementation limited to single-scan
 * files might find it better to apply #2 for markers other than EOI, since
 * any other marker would have to be bogus data in that case.
 */

BOOL CDecompressJpeg::resyncToRestart(int desired) {
  int myMarker = unreadMarker;
  int action = 1;
  
  /* Always put up a warning. */
  m_Parent->err->warn(JWRN_MUST_RESYNC, myMarker, desired);
  
  /* Outer loop handles repeated decision after scanning forward. */
  for(;;) {
    if(myMarker < (int) CMarkerReader::M_SOF0)
      action = 2;		/* invalid marker */
    else if(myMarker < (int) CMarkerReader::M_RST0 || myMarker > (int) CMarkerReader::M_RST7)
      action = 3;		/* valid non-restart marker */
    else {
      if(myMarker == ((int) CMarkerReader::M_RST0 + ((desired+1) & 7)) ||
			  myMarker == ((int) CMarkerReader::M_RST0 + ((desired+2) & 7)))
				action = 3;		/* one of the next two expected restarts */
      else if(myMarker == ((int) CMarkerReader::M_RST0 + ((desired-1) & 7)) ||
	      myMarker == ((int) CMarkerReader::M_RST0 + ((desired-2) & 7)))
				action = 2;		/* a prior restart, so advance */
      else
				action = 1;		/* desired restart or too far away */
			}
//    TRACEMS2(cinfo, 4, JTRC_RECOVERY_ACTION, myMarker, action);
    switch(action) {
			case 1:
				/* Discard myMarker and let entropy decoder resume processing. */
				unreadMarker = 0;
				return TRUE;
			case 2:
				/* Scan to the next marker, and repeat the decision loop. */
				if(!marker->nextMarker())
					return FALSE;
				myMarker = unreadMarker;
				break;
			case 3:
				/* Return without advancing past this marker. */
				/* Entropy decoder will be forced to process an empty segment. */
				return TRUE;
			}
		} /* end loop */
	}



/*
 * jquant1.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains 1-pass color quantization (color mapping) routines.
 * These routines provide mapping to a fixed color map using equally spaced
 * color values.  Optional Floyd-Steinberg or ordered dithering is available.
 */

#ifdef QUANT_1PASS_SUPPORTED


/*
 * The main purpose of 1-pass quantization is to provide a fast, if not very
 * high quality, colormapped output capability.  A 2-pass quantizer usually
 * gives better visual quality; however, for quantized grayscale output this
 * quantizer is perfectly adequate.  Dithering is highly recommended with this
 * quantizer, though you can turn it off if you really want to.
 *
 * In 1-pass quantization the colormap must be chosen in advance of seeing the
 * image.  We use a map consisting of all combinations of Ncolors[i] color
 * values for the i'th component.  The Ncolors[] values are chosen so that
 * their product, the total number of colors, is no more than that requested.
 * (In most cases, the product will be somewhat less.)
 *
 * Since the colormap is orthogonal, the representative value for each color
 * component can be determined without considering the other components;
 * then these indexes can be combined into a colormap index by a standard
 * N-dimensional-array-subscript calculation.  Most of the arithmetic involved
 * can be precalculated and stored in the lookup table colorindex[].
 * colorindex[i][j] maps pixel value j in component i to the nearest
 * representative value (grid plane) for that component; this index is
 * multiplied by the array stride for component i, so that the
 * index of the colormap entry closest to a given pixel value is just
 *    sum( colorindex[component-number][pixel-component-value] )
 * Aside from being fast, this scheme allows for variable spacing between
 * representative values with no additional lookup cost.
 *
 * If gamma correction has been applied in color conversion, it might be wise
 * to adjust the color grid spacing so that the representative colors are
 * equidistant in linear space.  At this writing, gamma correction is not
 * implemented by jdcolor, so nothing is done here.
 */


/* Declarations for ordered dithering.
 *
 * We use a standard 16x16 ordered dither array.  The basic concept of ordered
 * dithering is described in many references, for instance Dale Schumacher's
 * chapter II.2 of Graphics Gems II (James Arvo, ed. Academic Press, 1991).
 * In place of Schumacher's comparisons against a "threshold" value, we add a
 * "dither" value to the input pixel and then round the result to the nearest
 * output value.  The dither value is equivalent to (0.5 - threshold) times
 * the distance between output values.  For ordered dithering, we assume that
 * the output colors are equally spaced; if not, results will probably be
 * worse, since the dither may be too much or too little at a given point.
 *
 * The normal calculation would be to form pixel value + dither, range-limit
 * this to 0..MAXJSAMPLE, and then index into the colorindex table as usual.
 * We can skip the separate range-limiting step by extending the colorindex
 * table in both directions.
 */


const UINT8 CColorQuantizer::baseDitherMatrix[ODITHER_SIZE][ODITHER_SIZE] = {
  /* Bayer's order-4 dither array.  Generated by the code given in
   * Stephen Hawley's article "Ordered Dithering" in Graphics Gems I.
   * The values in this array must range from 0 to ODITHER_CELLS-1.
   */
  {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
  { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
  {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
  { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
  {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
  { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
  {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
  { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
  {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
  { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
  {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
  { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
  {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
  { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
  {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
  { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
	};




/*
 * Policy-making subroutines for create_colormap and create_colorindex.
 * These routines determine the colormap to be used.  The rest of the module
 * only assumes that the colormap is orthogonal.
 *
 *  * select_ncolors decides how to divvy up the available colors
 *    among the components.
 *  * output_value defines the set of representative values for a component.
 *  * largest_input_value defines the mapping from input values to
 *    representative values for a component.
 * Note that the latter two routines may impose different policies for
 * different components, though this is not currently done.
 */


int CColorQuantizer::selectNcolors(int Ncolors[]) {
/* Determine allocation of desired colors to components, */
/* and fill in Ncolors[] array to indicate choice. */
/* Return value is total number of colors (product of Ncolors[] values). */

  int nc = m_Parent->outColorComponents; // number of color components
  int max_colors = m_Parent->desiredNumberOfColors;
  int total_colors, iroot, i, j;
  BOOL changed;
  long temp;
  static const int RGB_order[3] = { RGB_GREEN, RGB_RED, RGB_BLUE };

  /* We can allocate at least the nc'th root of max_colors per component. */
  /* Compute floor(nc'th root of max_colors). */
  iroot = 1;
  do {
    iroot++;
    temp = iroot;		// set temp = iroot ** nc 
    for(i=1; i < nc; i++)
      temp *= iroot;
		} while(temp <= (long) max_colors); // repeat till iroot exceeds root 
  iroot--;			// now iroot = floor(root) 

  // Must have at least 2 color values per component
  if(iroot < 2)
    m_Parent->m_Parent->err->errorExit(JERR_QUANT_FEW_COLORS, (int) temp);

  // Initialize to iroot color values for each component
  total_colors = 1;
  for(i=0; i < nc; i++) {
    Ncolors[i] = iroot;
    total_colors *= iroot;
		}
  /* We may be able to increment the count for one or more components without
   * exceeding max_colors, though we know not all can be incremented.
   * Sometimes, the first component can be incremented more than once!
   * (Example: for 16 colors, we start at 2*2*2, go to 3*2*2, then 4*2*2.)
   * In RGB colorspace, try to increment G first, then R, then B.
   */
  do {
    changed = FALSE;
    for(i = 0; i < nc; i++) {
      j=(m_Parent->outColorSpace == JCS_RGB ? RGB_order[i] : i);
      // calculate new total_colors if Ncolors[j] is incremented
      temp = total_colors / Ncolors[j];
      temp *= Ncolors[j]+1;	// done in long arith to avoid oflo
      if(temp > (long) max_colors)
				break;			// won't fit, done with this pass
      Ncolors[j]++;		// OK, apply the increment
      total_colors = (int) temp;
      changed = TRUE;
			}
		} while(changed);

  return total_colors;
	}


int CColorQuantizer::outputValue(int ci, int j, int maxj)
/* Return j'th output value, where j will range from 0 to maxj */
/* The output values must fall in 0..MAXJSAMPLE in increasing order */
{
  /* We always provide values 0 and MAXJSAMPLE for each component;
   * any additional values are equally spaced between these limits.
   * (Forcing the upper and lower values to the limits ensures that
   * dithering can't produce a color outside the selected gamut.)
   */
  return (int) (((INT32) j * MAXJSAMPLE + maxj/2) / maxj);
	}


int CColorQuantizer::largestInputValue(int ci, int j, int maxj) {
/* Return largest input value that should map to j'th output value */
/* Must have largest(j=0) >= 0, and largest(j=maxj) >= MAXJSAMPLE */

  // Breakpoints are halfway between values returned by output_value 
  return (int)(((INT32) (2*j + 1) * MAXJSAMPLE + maxj) / (2*maxj));
	}


/*
 * Create the colormap.
 */

void CColorQuantizer::createColormap() {
  JSAMPARRAY colormap;		// Created colormap 
  int total_colors;		// Number of distinct output colors 
  int i,j,k, nci, blksize, blkdist, ptr, val;

  // Select number of colors for each component 
  total_colors = selectNcolors(Ncolors);

  // Report selected color counts 
/*  if(m_Parent->outColorComponents == 3)
    TRACEMS4(1, JTRC_QUANT_3_NCOLORS,
	     total_colors, cquantize->Ncolors[0],
	     cquantize->Ncolors[1], cquantize->Ncolors[2]);
  else
    TRACEMS1(1, JTRC_QUANT_NCOLORS, total_colors);*/

  /* Allocate and fill in the colormap. */
  /* The colors are ordered in the map in standard row-major order, */
  /* i.e. rightmost (highest-indexed) color changes most rapidly. */

  colormap = m_Parent->m_Parent->mem->allocSarray(JPOOL_IMAGE,
     (JDIMENSION) total_colors, (JDIMENSION) m_Parent->outColorComponents);

  /* blksize is number of adjacent repeated entries for a component */
  /* blkdist is distance between groups of identical entries for a component */
  blkdist = total_colors;

  for(i=0; i< m_Parent->outColorComponents; i++) {
    // fill in colormap entries for i'th color component 
    nci = Ncolors[i]; /* # of distinct values for this color */
    blksize = blkdist / nci;
    for(j = 0; j < nci; j++) {
      /* Compute j'th output value (out of nci) for component */
      val = outputValue(i, j, nci-1);
      // Fill in all colormap entries that have this value of this component 
      for(ptr = j * blksize; ptr < total_colors; ptr += blkdist) {
	// fill in blksize entries beginning at ptr 
			for(k=0; k < blksize; k++)
				colormap[i][ptr+k] = (JSAMPLE) val;
				}
			}
    blkdist = blksize;		// blksize of this color is blkdist of next 
		}

  /* Save the colormap in private storage,
   * where it will survive color quantization mode changes.
   */
  svColormap = colormap;
  svActual = total_colors;
	}


/*
 * Create the color index table.
 */
void CColorQuantizer::createColorindex() {
  JSAMPROW indexptr;
  int i,j,k, nci, blksize, val, pad;

  /* For ordered dither, we pad the color index tables by MAXJSAMPLE in
   * each direction (input index values can be -MAXJSAMPLE .. 2*MAXJSAMPLE).
   * This is not necessary in the other dithering modes.  However, we
   * flag whether it was done in case user changes dithering mode.
   */
  if(m_Parent->ditherMode == JDITHER_ORDERED) {
    pad = MAXJSAMPLE*2;
    isPadded = TRUE;
		} 
	else {
    pad = 0;
    isPadded = FALSE;
		}

  colorIndex = m_Parent->m_Parent->mem->allocSarray(JPOOL_IMAGE,
     (JDIMENSION)(MAXJSAMPLE+1 + pad),
     (JDIMENSION)m_Parent->outColorComponents);

  // blksize is number of adjacent repeated entries for a component
  blksize = svActual;

  for(i=0; i < m_Parent->outColorComponents; i++) {
    // fill in colorindex entries for i'th color component
    nci = Ncolors[i]; // # of distinct values for this color
    blksize = blksize / nci;

    // adjust colorindex pointers to provide padding at negative indexes.
    if(pad)
      colorIndex[i] += MAXJSAMPLE;

    // in loop, val = index of current output value,
    // and k = largest j that maps to current val
    indexptr = colorIndex[i];
    val = 0;
    k = largestInputValue(i, 0, nci-1);
    for(j=0; j<=MAXJSAMPLE; j++) {
      while(j > k)		// advance val if past boundary
				k=largestInputValue(i, ++val, nci-1);
      // premultiply so that no multiplication needed in main processing
      indexptr[j] = (JSAMPLE) (val * blksize);
			}
    // Pad at both ends if necessary
    if(pad)
      for(j=1; j <= MAXJSAMPLE; j++) {
				indexptr[-j] = indexptr[0];
				indexptr[MAXJSAMPLE+j] = indexptr[MAXJSAMPLE];
      }
		}
	}


/*
 * Create an ordered-dither array for a component having ncolors
 * distinct output values.
 */

ODITHER_MATRIX_PTR CColorQuantizer::makeOditherArray(int ncolors) {
  ODITHER_MATRIX_PTR odither;
  int j,k;
  INT32 num,den;

  odither = (ODITHER_MATRIX_PTR)
    m_Parent->m_Parent->mem->allocSmall(JPOOL_IMAGE,sizeof(ODITHER_MATRIX));
  /* The inter-value distance for this color is MAXJSAMPLE/(ncolors-1).
   * Hence the dither value for the matrix cell with fill order f
   * (f=0..N-1) should be (N-1-2*f)/(2*N) * MAXJSAMPLE/(ncolors-1).
   * On 16-bit-int machine, be careful to avoid overflow.
   */
  den = 2 * ODITHER_CELLS * ((INT32) (ncolors - 1));
  for (j = 0; j < ODITHER_SIZE; j++) {
    for (k = 0; k < ODITHER_SIZE; k++) {
      num = ((INT32) (ODITHER_CELLS-1 - 2*((int)baseDitherMatrix[j][k])))
	    * MAXJSAMPLE;
      /* Ensure round towards zero despite C's lack of consistency
       * about rounding negative values in integer division...
       */
      odither[j][k] = (int) (num<0 ? -((-num)/den) : num/den);
		  }
	  }
  return odither;
	}


/*
 * Create the ordered-dither tables.
 * Components having the same number of representative colors may 
 * share a dither table.
 */

void CColorQuantizer::createOditherTables() {
  ODITHER_MATRIX_PTR o_dither;
  int i, j, nci;

  for(i = 0; i < m_Parent->outColorComponents; i++) {
    nci = Ncolors[i]; // # of distinct values for this color
    o_dither = NULL;		// search for matching prior component
    for(j=0; j < i; j++) {
      if(nci == Ncolors[j]) {
				o_dither = odither[j];
				break;
				}
			}
    if(!o_dither)	// need a new table?
      o_dither = makeOditherArray(nci);
    odither[i] = o_dither;
		}
	}


/*
 * Map some rows of pixels to the output colormapped representation.
 */

void CColorQuantizer::colorQuantize(JSAMPARRAY input_buf,
	JSAMPARRAY output_buf, int num_rows) {  // General case, no dithering 

  JSAMPARRAY colorindex = colorIndex;
  register int pixcode, ci;
  register JSAMPROW ptrin, ptrout;
  int row;
  JDIMENSION col;
  JDIMENSION width = m_Parent->outputWidth;
  register int nc = m_Parent->outColorComponents;

  for(row=0; row < num_rows; row++) {
    ptrin= input_buf[row];
    ptrout= output_buf[row];
    for(col=width; col > 0; col--) {
      pixcode=0;
      for(ci=0; ci < nc; ci++)
				pixcode += GETJSAMPLE(colorindex[ci][GETJSAMPLE(*ptrin++)]);
      *ptrout++ = (JSAMPLE) pixcode;
			}
		}
	}


void CColorQuantizer::colorQuantize3(JSAMPARRAY input_buf,
	JSAMPARRAY output_buf, int num_rows) {
// Fast path for outColorComponents==3, no dithering 

  register int pixcode;
  register JSAMPROW ptrin, ptrout;
  JSAMPROW colorindex0 = colorIndex[0];
  JSAMPROW colorindex1 = colorIndex[1];
  JSAMPROW colorindex2 = colorIndex[2];
  int row;
  JDIMENSION col;
  JDIMENSION width=m_Parent->outputWidth;

  for(row=0; row < num_rows; row++) {
    ptrin = input_buf[row];
    ptrout = output_buf[row];
    for(col=width; col > 0; col--) {
      pixcode  = GETJSAMPLE(colorindex0[GETJSAMPLE(*ptrin++)]);
      pixcode += GETJSAMPLE(colorindex1[GETJSAMPLE(*ptrin++)]);
      pixcode += GETJSAMPLE(colorindex2[GETJSAMPLE(*ptrin++)]);
      *ptrout++ = (JSAMPLE) pixcode;
			}
		}
	}


void CColorQuantizer::quantizeOrdDither(JSAMPARRAY input_buf,
	JSAMPARRAY output_buf, int num_rows) {
// General case, with ordered dithering 

  register JSAMPROW input_ptr;
  register JSAMPROW output_ptr;
  JSAMPROW colorindex_ci;
  int *dither;			/* points to active row of dither matrix */
  int row_index, col_index;	/* current indexes into dither matrix */
  int nc=m_Parent->outColorComponents;
  int ci;
  int row;
  JDIMENSION col;
  JDIMENSION width = m_Parent->outputWidth;

  for(row=0; row<num_rows; row++) {
    /* Initialize output values to 0 so can process components separately */
    ZeroMemory((void *)output_buf[row],(size_t) (width * sizeof(JSAMPLE)));
    row_index = rowIndex;
    for(ci=0; ci < nc; ci++) {
      input_ptr = input_buf[row] + ci;
      output_ptr = output_buf[row];
      colorindex_ci = colorIndex[ci];
      dither = odither[ci][row_index];
      col_index = 0;

      for(col=width; col > 0; col--) {
	/* Form pixel value + dither, range-limit to 0..MAXJSAMPLE,
	 * select output value, accumulate into output code for this pixel.
	 * Range-limiting need not be done explicitly, as we have extended
	 * the colorindex table to produce the right answers for out-of-range
	 * inputs.  The maximum dither is +- MAXJSAMPLE; this sets the
	 * required amount of padding.
	 */
			*output_ptr += colorindex_ci[GETJSAMPLE(*input_ptr)+dither[col_index]];
			input_ptr += nc;
			output_ptr++;
			col_index = (col_index + 1) & ODITHER_MASK;
      }
    }
    // Advance row index for next row 
    row_index = (row_index + 1) & ODITHER_MASK;
    rowIndex = row_index;
		}
	}


void CColorQuantizer::quantize3OrdDither(JSAMPARRAY input_buf,
	JSAMPARRAY output_buf, int num_rows) {
// Fast path for out_color_components==3, with ordered dithering 

  register int pixcode;
  register JSAMPROW input_ptr;
  register JSAMPROW output_ptr;
  JSAMPROW colorindex0 = colorIndex[0];
  JSAMPROW colorindex1 = colorIndex[1];
  JSAMPROW colorindex2 = colorIndex[2];
  int *dither0;		/* points to active row of dither matrix */
  int *dither1;
  int *dither2;
  int row_index, col_index;	/* current indexes into dither matrix */
  int row;
  JDIMENSION col;
  JDIMENSION width =m_Parent->outputWidth;

  for(row = 0; row < num_rows; row++) {
    row_index = rowIndex;
    input_ptr = input_buf[row];
    output_ptr = output_buf[row];
    dither0 = odither[0][row_index];
    dither1 = odither[1][row_index];
    dither2 = odither[2][row_index];
    col_index = 0;

    for(col=width; col > 0; col--) {
      pixcode  = GETJSAMPLE(colorindex0[GETJSAMPLE(*input_ptr++) +
				dither0[col_index]]);
      pixcode += GETJSAMPLE(colorindex1[GETJSAMPLE(*input_ptr++) +
				dither1[col_index]]);
      pixcode += GETJSAMPLE(colorindex2[GETJSAMPLE(*input_ptr++) +
				dither2[col_index]]);
      *output_ptr++ = (JSAMPLE) pixcode;
      col_index = (col_index + 1) & ODITHER_MASK;
	    }
    row_index = (row_index + 1) & ODITHER_MASK;
    rowIndex = row_index;
		}
	}


void CColorQuantizer::quantize_FS_Dither(JSAMPARRAY input_buf,
	JSAMPARRAY output_buf, int num_rows) {
// General case, with Floyd-Steinberg dithering 

  register LOCFSERROR cur;	// current error or pixel value
  LOCFSERROR belowerr;		// error for pixel below cur
  LOCFSERROR bpreverr;		// error for below/prev col
  LOCFSERROR bnexterr;		// error for below/next col
  LOCFSERROR delta;
  register FSERRPTR errorptr;	// => fserrors[] at column before current
  register JSAMPROW input_ptr;
  register JSAMPROW output_ptr;
  JSAMPROW colorindex_ci;
  JSAMPROW colormap_ci;
  int pixcode;
  int nc =m_Parent->outColorComponents;
  int dir;			// 1 for left-to-right, -1 for right-to-left
  int dirnc;			// dir * nc
  int ci;
  int row;
  JDIMENSION col;
  JDIMENSION width = m_Parent->outputWidth;
  JSAMPLE *range_limit = m_Parent->sampleRangeLimit;
  SHIFT_TEMPS

  for(row = 0; row < num_rows; row++) {
    /* Initialize output values to 0 so can process components separately */
    ZeroMemory((void *)output_buf[row],(size_t) (width * sizeof(JSAMPLE)));
    for(ci=0; ci<nc; ci++) {
      input_ptr = input_buf[row] + ci;
      output_ptr = output_buf[row];
      if(onOddRow) {
				// work right to left in this row 
				input_ptr += (width-1) * nc; // so point to rightmost pixel 
				output_ptr += width-1;
				dir = -1;
				dirnc = -nc;
				errorptr = fserrors[ci] + (width+1); // => entry after last column
				} 
			else {
		// work left to right in this row
				dir = 1;
				dirnc = nc;
				errorptr = fserrors[ci]; /* => entry before first column */
				}
      colorindex_ci = colorIndex[ci];
      colormap_ci = svColormap[ci];
      /* Preset error values: no error propagated to first pixel from left */
      cur = 0;
      /* and no error propagated to row below yet */
      belowerr = bpreverr = 0;

      for(col=width; col > 0; col--) {
	/* cur holds the error propagated from the previous pixel on the
	 * current line.  Add the error propagated from the previous line
	 * to form the complete error correction term for this pixel, and
	 * round the error term (which is expressed * 16) to an integer.
	 * RIGHT_SHIFT rounds towards minus infinity, so adding 8 is correct
	 * for either sign of the error value.
	 * Note: errorptr points to *previous* column's array entry.
	 */
				cur = RIGHT_SHIFT(cur + errorptr[dir] + 8, 4);
	/* Form pixel value + error, and range-limit to 0..MAXJSAMPLE.
	 * The maximum error is +- MAXJSAMPLE; this sets the required size
	 * of the range_limit array.
	 */
				cur += GETJSAMPLE(*input_ptr);
				cur = GETJSAMPLE(range_limit[cur]);
	/* Select output value, accumulate into output code for this pixel */
				pixcode = GETJSAMPLE(colorindex_ci[cur]);
				*output_ptr += (JSAMPLE) pixcode;
	/* Compute actual representation error at this pixel */
	/* Note: we can do this even though we don't have the final */
	/* pixel code, because the colormap is orthogonal. */
				cur -= GETJSAMPLE(colormap_ci[pixcode]);
	/* Compute error fractions to be propagated to adjacent pixels.
	 * Add these into the running sums, and simultaneously shift the
	 * next-line error sums left by 1 column.
	 */
				bnexterr = cur;
				delta = cur * 2;
				cur += delta;		/* form error * 3 */
				errorptr[0] = (FSERROR) (bpreverr + cur);
				cur += delta;		/* form error * 5 */
				bpreverr = belowerr + cur;
				belowerr = bnexterr;
				cur += delta;		/* form error * 7 */
	/* At this point cur contains the 7/16 error value to be propagated
	 * to the next pixel on the current line, and all the errors for the
	 * next line have been shifted over. We are therefore ready to move on.
	 */
				input_ptr += dirnc;	// advance input ptr to next column
				output_ptr += dir;	// advance output ptr to next column
				errorptr += dir;	// advance errorptr to current column
	      }
      /* Post-loop cleanup: we must unload the final error value into the
       * final fserrors[] entry.  Note we need not unload belowerr because
       * it is for the dummy column before or after the actual array.
       */
      errorptr[0] = (FSERROR) bpreverr; /* unload prev err into array */
	    }
    onOddRow = (onOddRow ? FALSE : TRUE);
		}
	}


/*
 * Allocate workspace for Floyd-Steinberg errors.
 */

void CColorQuantizer::alloc_FS_Workspace() {
  size_t arraysize;
  int i;

  arraysize = (size_t) ((m_Parent->outputWidth + 2) * sizeof(FSERROR));
  for(i=0; i < m_Parent->outColorComponents; i++) {
    fserrors[i] = (FSERRPTR)
      m_Parent->m_Parent->mem->allocLarge(JPOOL_IMAGE, arraysize);
		}
	}


/*
 * Initialize for one-pass color quantization.
 */

void CColorQuantizer::startPass_1_Quant(BOOL is_pre_scan) {
  size_t arraysize;
  int i;

  /* Install my colormap. */
  m_Parent->colormap = svColormap;
  m_Parent->actualNumberOfColors = svActual;

  // Initialize for desired dithering mode. 
  switch(m_Parent->ditherMode) {
		case JDITHER_NONE:
			if(m_Parent->outColorComponents == 3)
				doColorQuantize = colorQuantize3;
			else
				doColorQuantize = colorQuantize;
			break;
		case JDITHER_ORDERED:
			if(m_Parent->outColorComponents == 3)
				doColorQuantize = quantize3OrdDither;
			else
				doColorQuantize = quantizeOrdDither;
			rowIndex = 0;	/* initialize state for ordered dither */
			/* If user changed to ordered dither from another mode,
			 * we must recreate the color index table with padding.
			 * This will cost extra space, but probably isn't very likely.
			 */
			if(!isPadded)
				createColorindex();
			// Create ordered-dither tables if we didn't already.
			if(!odither[0])
				createOditherTables();
			break;
		case JDITHER_FS:
			doColorQuantize = quantize_FS_Dither;
			onOddRow = FALSE; // initialize state for F-S dither
			// Allocate Floyd-Steinberg workspace if didn't already.
			if(!fserrors[0])
				alloc_FS_Workspace();
			// Initialize the propagated errors to zero.
			arraysize= (size_t) ((m_Parent->outputWidth + 2) * sizeof(FSERROR));
			for(i=0; i< m_Parent->outColorComponents; i++)
				ZeroMemory((void *)fserrors[i], arraysize);
			break;
		default:
			m_Parent->m_Parent->err->errorExit(JERR_NOT_COMPILED);
			break;
		}
	}


/*
 * Finish up at the end of the pass.
 */
void CColorQuantizer::finishPass_1_Quant() {
  // no work in 1-pass case 
	}


/*
 * Switch to a new external colormap between output passes.
 * Shouldn't get to this module!
 */

void CColorQuantizer::newColorMap_1_Quant() {

  m_Parent->m_Parent->err->errorExit(JERR_MODE_CHANGE);
	}


/*
 * Module initialization routine for 1-pass color quantization.
 */

void CColorQuantizer::init_1passQuantizer() {

  doStartPass = startPass_1_Quant;
  doFinishPass = finishPass_1_Quant;
  doNewColorMap = newColorMap_1_Quant;
  fserrors[0] = NULL; /* Flag FS workspace not allocated */
  odither[0] = NULL;	/* Also flag odither arrays not allocated */

  // Make sure my internal arrays won't overflow
  if(m_Parent->outColorComponents > MAX_Q_COMPS)
    m_Parent->m_Parent->err->errorExit(JERR_QUANT_COMPONENTS, MAX_Q_COMPS);
  // Make sure colormap indexes can be represented by JSAMPLEs
  if(m_Parent->desiredNumberOfColors > (MAXJSAMPLE+1))
    m_Parent->m_Parent->err->errorExit(JERR_QUANT_MANY_COLORS, MAXJSAMPLE+1);

  // Create the colormap and color index table.
  createColormap();
  createColorindex();

  /* Allocate Floyd-Steinberg workspace now if requested.
   * We do this now since it is FAR storage and may affect the memory
   * manager's space calculations.  If the user changes to FS dither
   * mode in a later pass, we will allocate the space then, and will
   * possibly overrun the max_memory_to_use setting.
   */
  if(m_Parent->ditherMode == JDITHER_FS)
    alloc_FS_Workspace();
	}

#endif // QUANT_1PASS_SUPPORTED





/*
 * jquant2.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains 2-pass color quantization (color mapping) routines.
 * These routines provide selection of a custom color map for an image,
 * followed by mapping of the image to that color map, with optional
 * Floyd-Steinberg dithering.
 * It is also possible to use just the second pass to map to an arbitrary
 * externally-given color map.
 *
 * Note: ordered dithering is not supported, since there isn't any fast
 * way to compute intercolor distances; it's unclear that ordered dither's
 * fundamental assumptions even hold with an irregularly spaced color map.
 */

#ifdef QUANT_2PASS_SUPPORTED


/*
 * This module implements the well-known Heckbert paradigm for color
 * quantization.  Most of the ideas used here can be traced back to
 * Heckbert's seminal paper
 *   Heckbert, Paul.  "Color Image Quantization for Frame Buffer Display",
 *   Proc. SIGGRAPH '82, Computer Graphics v.16 #3 (July 1982), pp 297-304.
 *
 * In the first pass over the image, we accumulate a histogram showing the
 * usage count of each possible color.  To keep the histogram to a reasonable
 * size, we reduce the precision of the input; typical practice is to retain
 * 5 or 6 bits per color, so that 8 or 4 different input values are counted
 * in the same histogram cell.
 *
 * Next, the color-selection step begins with a box representing the whole
 * color space, and repeatedly splits the "largest" remaining box until we
 * have as many boxes as desired colors.  Then the mean color in each
 * remaining box becomes one of the possible output colors.
 * 
 * The second pass over the image maps each input pixel to the closest output
 * color (optionally after applying a Floyd-Steinberg dithering correction).
 * This mapping is logically trivial, but making it go fast enough requires
 * considerable care.
 *
 * Heckbert-style quantizers vary a good deal in their policies for choosing
 * the "largest" box and deciding where to cut it.  The particular policies
 * used here have proved out well in experimental comparisons, but better ones
 * may yet be found.
 *
 * In earlier versions of the IJG code, this module quantized in YCbCr color
 * space, processing the raw upsampled data without a color conversion step.
 * This allowed the color conversion math to be done only once per colormap
 * entry, not once per pixel.  However, that optimization precluded other
 * useful optimizations (such as merging color conversion with upsampling)
 * and it also interfered with desired capabilities such as quantizing to an
 * externally-supplied colormap.  We have therefore abandoned that approach.
 * The present code works in the post-conversion color space, typically RGB.
 *
 * To improve the visual quality of the results, we actually work in scaled
 * RGB space, giving G distances more weight than R, and R in turn more than
 * B.  To do everything in integer math, we must use integer scale factors.
 * The 2/3/1 scale factors used here correspond loosely to the relative
 * weights of the colors in the NTSC grayscale equation.
 * If you want to use this code to quantize a non-RGB color space, you'll
 * probably need to change these scale factors.
 */

#define R_SCALE 2		// scale R distances by this much
#define G_SCALE 3		// scale G distances by this much
#define B_SCALE 1		// and B by this much 

/* Relabel R/G/B as components 0/1/2, respecting the RGB ordering defined
 * in jmorecfg.h.  As the code stands, it will do the right thing for R,G,B
 * and B,G,R orders.  If you define some other weird order in jmorecfg.h,
 * you'll get compile errors until you extend this logic.  In that case
 * you'll probably want to tweak the histogram sizes too.
 */

#if RGB_RED == 0
#define C0_SCALE R_SCALE
#endif
#if RGB_BLUE == 0
#define C0_SCALE B_SCALE
#endif
#if RGB_GREEN == 1
#define C1_SCALE G_SCALE
#endif
#if RGB_RED == 2
#define C2_SCALE R_SCALE
#endif
#if RGB_BLUE == 2
#define C2_SCALE B_SCALE
#endif







/*
 * Prescan some rows of pixels.
 * In this module the prescan simply updates the histogram, which has been
 * initialized to zeroes by start_pass.
 * An output_buf parameter is required by the method signature, but no data
 * is actually output (in fact the buffer controller is probably passing a
 * NULL pointer).
 */

void CColorQuantizer::prescanQuantize(JSAMPARRAY input_buf,
	JSAMPARRAY output_buf, int num_rows) {
//  MY_CQUANTIZE_PTR2 cquantize = (MY_CQUANTIZE_PTR2) cinfo->cquantize;
  register JSAMPROW ptr;
  register HISTPTR histp;
  register HIST3D myHistogram = histogram;
  int row;
  JDIMENSION col;
  JDIMENSION width = m_Parent->outputWidth;

  for(row=0; row < num_rows; row++) {
    ptr=input_buf[row];
    for(col=width; col > 0; col--) {
      /* get pixel value and index into the histogram */
      histp = & histogram[GETJSAMPLE(ptr[0]) >> C0_SHIFT]
			 [GETJSAMPLE(ptr[1]) >> C1_SHIFT]
			 [GETJSAMPLE(ptr[2]) >> C2_SHIFT];
      /* increment, check for overflow and undo increment if so. */
      if(++(*histp) <= 0)
				(*histp)--;
				ptr += 3;
				}
			}
		}


/*
 * Next we have the really interesting routines: selection of a colormap
 * given the completed histogram.
 * These routines work with a list of "boxes", each representing a rectangular
 * subset of the input color space (to histogram precision).
 */

BOXPTR CColorQuantizer::findBiggestColorPop(BOXPTR boxlist, int numboxes)
/* Find the splittable box with the largest color population */
/* Returns NULL if no splittable boxes remain */
{
  register BOXPTR boxp;
  register int i;
  register long maxc=0;
  BOXPTR which = NULL;
  
  for(i=0, boxp=boxlist; i < numboxes; i++, boxp++) {
    if(boxp->colorcount > maxc && boxp->volume > 0) {
      which = boxp;
      maxc = boxp->colorcount;
			}
		}
  return which;
	}


BOXPTR CColorQuantizer::findBiggestVolume(BOXPTR boxlist, int numboxes)
// Find the splittable box with the largest (scaled) volume
// Returns NULL if no splittable boxes remain
{
  register BOXPTR boxp;
  register int i;
  register INT32 maxv = 0;
  BOXPTR which=NULL;
  
  for(i=0, boxp = boxlist; i < numboxes; i++, boxp++) {
    if(boxp->volume > maxv) {
      which = boxp;
      maxv = boxp->volume;
			}
		}
  return which;
	}


void CColorQuantizer::updateBox(BOXPTR boxp) {
/* Shrink the min/max bounds of a box to enclose only nonzero elements, */
/* and recompute its volume and population */
  HIST3D myHistogram = histogram;
  HISTPTR histp;
  int c0,c1,c2;
  int c0min,c0max,c1min,c1max,c2min,c2max;
  INT32 dist0,dist1,dist2;
  long ccount;
  
  c0min = boxp->c0min;  c0max = boxp->c0max;
  c1min = boxp->c1min;  c1max = boxp->c1max;
  c2min = boxp->c2min;  c2max = boxp->c2max;
  
  if(c0max > c0min)
    for(c0=c0min; c0 <= c0max; c0++)
      for(c1=c1min; c1 <= c1max; c1++) {
				histp = &histogram[c0][c1][c2min];
				for(c2=c2min; c2 <= c2max; c2++)
					if(*histp++ != 0) {
						boxp->c0min = c0min = c0;
						goto have_c0min;
						}
					}
have_c0min:
  if(c0max > c0min)
    for(c0 = c0max; c0 >= c0min; c0--)
      for(c1 = c1min; c1 <= c1max; c1++) {
				histp = & histogram[c0][c1][c2min];
				for (c2 = c2min; c2 <= c2max; c2++)
					if (*histp++ != 0) {
						boxp->c0max = c0max = c0;
						goto have_c0max;
					}
      }
  have_c0max:
  if(c1max > c1min)
    for(c1=c1min; c1 <= c1max; c1++)
      for(c0=c0min; c0 <= c0max; c0++) {
				histp = &histogram[c0][c1][c2min];
				for(c2=c2min; c2 <= c2max; c2++)
					if(*histp++ != 0) {
						boxp->c1min = c1min = c1;
						goto have_c1min;
					}
	      }
have_c1min:
  if(c1max > c1min)
    for(c1=c1max; c1 >= c1min; c1--)
      for(c0=c0min; c0 <= c0max; c0++) {
				histp= &histogram[c0][c1][c2min];
				for(c2=c2min; c2 <= c2max; c2++)
					if(*histp++ != 0) {
						boxp->c1max = c1max = c1;
						goto have_c1max;
					}
				}
have_c1max:
  if(c2max > c2min)
    for(c2=c2min; c2 <= c2max; c2++)
      for(c0=c0min; c0 <= c0max; c0++) {
				histp= &histogram[c0][c1min][c2];
				for(c1=c1min; c1 <= c1max; c1++, histp += HIST_C2_ELEMS)
					if(*histp != 0) {
						boxp->c2min = c2min = c2;
						goto have_c2min;
					}
      }
have_c2min:
  if(c2max > c2min)
    for(c2=c2max; c2 >= c2min; c2--)
      for(c0=c0min; c0 <= c0max; c0++) {
				histp = & histogram[c0][c1min][c2];
				for(c1=c1min; c1 <= c1max; c1++, histp += HIST_C2_ELEMS)
					if(*histp != 0) {
						boxp->c2max = c2max = c2;
						goto have_c2max;
					}
      }
have_c2max:

  /* Update box volume.
   * We use 2-norm rather than real volume here; this biases the method
   * against making long narrow boxes, and it has the side benefit that
   * a box is splittable iff norm > 0.
   * Since the differences are expressed in histogram-cell units,
   * we have to shift back to JSAMPLE units to get consistent distances;
   * after which, we scale according to the selected distance scale factors.
   */
  dist0 = ((c0max - c0min) << C0_SHIFT) * C0_SCALE;
  dist1 = ((c1max - c1min) << C1_SHIFT) * C1_SCALE;
  dist2 = ((c2max - c2min) << C2_SHIFT) * C2_SCALE;
  boxp->volume = dist0*dist0 + dist1*dist1 + dist2*dist2;
  
  // Now scan remaining volume of box and compute population 
  ccount = 0;
  for(c0=c0min; c0 <= c0max; c0++)
    for(c1=c1min; c1 <= c1max; c1++) {
      histp=&histogram[c0][c1][c2min];
      for(c2=c2min; c2 <= c2max; c2++, histp++)
			if(*histp)
				ccount++;
	    }
	  boxp->colorcount = ccount;
	}


int CColorQuantizer::medianCut(BOXPTR boxlist, int numboxes,
	    int desired_colors) {
// Repeatedly select and split the largest box until we have enough boxes 
  int n,lb;
  int c0,c1,c2,cmax;
  register BOXPTR b1,b2;

  while(numboxes < desired_colors) {
    /* Select box to split.
     * Current algorithm: by population for first half, then by volume.
     */
    if(numboxes*2 <= desired_colors) {
      b1 = findBiggestColorPop(boxlist, numboxes);
			} 
		else {
      b1=findBiggestVolume(boxlist, numboxes);
			}
    if(b1 == NULL)		/* no splittable boxes left! */
      break;
    b2 = &boxlist[numboxes];	/* where new box will go */
    /* Copy the color bounds to the new box. */
    b2->c0max = b1->c0max; b2->c1max = b1->c1max; b2->c2max = b1->c2max;
    b2->c0min = b1->c0min; b2->c1min = b1->c1min; b2->c2min = b1->c2min;
    /* Choose which axis to split the box on.
     * Current algorithm: longest scaled axis.
     * See notes in update_box about scaling distances.
     */
    c0=((b1->c0max - b1->c0min) << C0_SHIFT) * C0_SCALE;
    c1=((b1->c1max - b1->c1min) << C1_SHIFT) * C1_SCALE;
    c2=((b1->c2max - b1->c2min) << C2_SHIFT) * C2_SCALE;
    /* We want to break any ties in favor of green, then red, blue last.
     * This code does the right thing for R,G,B or B,G,R color orders only.
     */
#if RGB_RED == 0
    cmax = c1; n = 1;
    if (c0 > cmax) { cmax = c0; n = 0; }
    if (c2 > cmax) { n = 2; }
#else
    cmax = c1; n = 1;
    if (c2 > cmax) { cmax = c2; n = 2; }
    if (c0 > cmax) { n = 0; }
#endif
    /* Choose split point along selected axis, and update box bounds.
     * Current algorithm: split at halfway point.
     * (Since the box has been shrunk to minimum volume,
     * any split will produce two nonempty subboxes.)
     * Note that lb value is max for lower box, so must be < old max.
     */
    switch(n) {
			case 0:
				lb = (b1->c0max + b1->c0min) / 2;
				b1->c0max = lb;
				b2->c0min = lb+1;
				break;
			case 1:
				lb = (b1->c1max + b1->c1min) / 2;
				b1->c1max = lb;
				b2->c1min = lb+1;
				break;
			case 2:
				lb = (b1->c2max + b1->c2min) / 2;
				b1->c2max = lb;
				b2->c2min = lb+1;
				break;
	    }
    /* Update stats for boxes */
    updateBox(b1);
    updateBox(b2);
    numboxes++;
		}
  return numboxes;
	}


void CColorQuantizer::computeColor(BOXPTR boxp, int icolor) {
// Compute representative color for a box, put it in colormap[icolor] 

  // Current algorithm: mean weighted by pixels (not colors) 
  // Note it is important to get the rounding correct! 
//  MY_CQUANTIZE_PTR2 cquantize = (MY_CQUANTIZE_PTR2) cinfo->cquantize;
  HIST3D myHistogram = histogram;
  HISTPTR histp;
  int c0,c1,c2;
  int c0min,c0max,c1min,c1max,c2min,c2max;
  long count;
  long total = 0;
  long c0total = 0;
  long c1total = 0;
  long c2total = 0;
  
  c0min = boxp->c0min;  c0max = boxp->c0max;
  c1min = boxp->c1min;  c1max = boxp->c1max;
  c2min = boxp->c2min;  c2max = boxp->c2max;
  
  for(c0=c0min; c0 <= c0max; c0++)
    for(c1=c1min; c1 <= c1max; c1++) {
      histp = & histogram[c0][c1][c2min];
      for(c2=c2min; c2 <= c2max; c2++) {
				if((count = *histp++) != 0) {
					total += count;
					c0total += ((c0 << C0_SHIFT) + ((1<<C0_SHIFT)>>1)) * count;
					c1total += ((c1 << C1_SHIFT) + ((1<<C1_SHIFT)>>1)) * count;
					c2total += ((c2 << C2_SHIFT) + ((1<<C2_SHIFT)>>1)) * count;
					}
				}
			}
  
  m_Parent->colormap[0][icolor] = (JSAMPLE) ((c0total + (total>>1)) / total);
  m_Parent->colormap[1][icolor] = (JSAMPLE) ((c1total + (total>>1)) / total);
  m_Parent->colormap[2][icolor] = (JSAMPLE) ((c2total + (total>>1)) / total);
	}


void CColorQuantizer::selectColors(int desired_colors) {
// Master routine for color selection 

  BOXPTR boxlist;
  int numboxes;
  int i;

  // Allocate workspace for box list 
  boxlist = (BOXPTR) (m_Parent->m_Parent->mem->allocSmall)
    (JPOOL_IMAGE, desired_colors * sizeof(BOX));
  // Initialize one box containing whole space 
  numboxes = 1;
  boxlist[0].c0min = 0;
  boxlist[0].c0max = MAXJSAMPLE >> C0_SHIFT;
  boxlist[0].c1min = 0;
  boxlist[0].c1max = MAXJSAMPLE >> C1_SHIFT;
  boxlist[0].c2min = 0;
  boxlist[0].c2max = MAXJSAMPLE >> C2_SHIFT;
  // Shrink it to actually-used volume and set its statistics 
  updateBox(&boxlist[0]);
  // Perform median-cut to produce final box list 
  numboxes =medianCut(boxlist, numboxes, desired_colors);
  // Compute the representative color for each box, fill colormap 
  for(i=0; i < numboxes; i++)
    computeColor(&boxlist[i], i);
  m_Parent->actualNumberOfColors = numboxes;
//  trace(1, JTRC_QUANT_SELECTED, numboxes);
	}


/*
 * These routines are concerned with the time-critical task of mapping input
 * colors to the nearest color in the selected colormap.
 *
 * We re-use the histogram space as an "inverse color map", essentially a
 * cache for the results of nearest-color searches.  All colors within a
 * histogram cell will be mapped to the same colormap entry, namely the one
 * closest to the cell's center.  This may not be quite the closest entry to
 * the actual input color, but it's almost as good.  A zero in the cache
 * indicates we haven't found the nearest color for that cell yet; the array
 * is cleared to zeroes before starting the mapping pass.  When we find the
 * nearest color for a cell, its colormap index plus one is recorded in the
 * cache for future use.  The pass2 scanning routines call fill_inverse_cmap
 * when they need to use an unfilled entry in the cache.
 *
 * Our method of efficiently finding nearest colors is based on the "locally
 * sorted search" idea described by Heckbert and on the incremental distance
 * calculation described by Spencer W. Thomas in chapter III.1 of Graphics
 * Gems II (James Arvo, ed.  Academic Press, 1991).  Thomas points out that
 * the distances from a given colormap entry to each cell of the histogram can
 * be computed quickly using an incremental method: the differences between
 * distances to adjacent cells themselves differ by a constant.  This allows a
 * fairly fast implementation of the "brute force" approach of computing the
 * distance from every colormap entry to every histogram cell.  Unfortunately,
 * it needs a work array to hold the best-distance-so-far for each histogram
 * cell (because the inner loop has to be over cells, not colormap entries).
 * The work array elements have to be INT32s, so the work array would need
 * 256Kb at our recommended precision.  This is not feasible in DOS machines.
 *
 * To get around these problems, we apply Thomas' method to compute the
 * nearest colors for only the cells within a small subbox of the histogram.
 * The work array need be only as big as the subbox, so the memory usage
 * problem is solved.  Furthermore, we need not fill subboxes that are never
 * referenced in pass2; many images use only part of the color gamut, so a
 * fair amount of work is saved.  An additional advantage of this
 * approach is that we can apply Heckbert's locality criterion to quickly
 * eliminate colormap entries that are far away from the subbox; typically
 * three-fourths of the colormap entries are rejected by Heckbert's criterion,
 * and we need not compute their distances to individual cells in the subbox.
 * The speed of this approach is heavily influenced by the subbox size: too
 * small means too much overhead, too big loses because Heckbert's criterion
 * can't eliminate as many colormap entries.  Empirically the best subbox
 * size seems to be about 1/512th of the histogram (1/8th in each direction).
 *
 * Thomas' article also describes a refined method which is asymptotically
 * faster than the brute-force method, but it is also far more complex and
 * cannot efficiently be applied to small subboxes.  It is therefore not
 * useful for programs intended to be portable to DOS machines.  On machines
 * with plenty of memory, filling the whole histogram in one shot with Thomas'
 * refined method might be faster than the present code --- but then again,
 * it might not be any faster, and it's certainly more complicated.
 */


/* log2(histogram cells in update box) for each axis; this can be adjusted */
#define BOX_C0_LOG  (HIST_C0_BITS-3)
#define BOX_C1_LOG  (HIST_C1_BITS-3)
#define BOX_C2_LOG  (HIST_C2_BITS-3)

#define BOX_C0_ELEMS  (1<<BOX_C0_LOG) /* # of hist cells in update box */
#define BOX_C1_ELEMS  (1<<BOX_C1_LOG)
#define BOX_C2_ELEMS  (1<<BOX_C2_LOG)

#define BOX_C0_SHIFT  (C0_SHIFT + BOX_C0_LOG)
#define BOX_C1_SHIFT  (C1_SHIFT + BOX_C1_LOG)
#define BOX_C2_SHIFT  (C2_SHIFT + BOX_C2_LOG)


/*
 * The next three routines implement inverse colormap filling.  They could
 * all be folded into one big routine, but splitting them up this way saves
 * some stack space (the mindist[] and bestdist[] arrays need not coexist)
 * and may allow some compilers to produce better code by registerizing more
 * inner-loop variables.
 */

int CColorQuantizer::findNearbyColors(int minc0, int minc1, int minc2,
		    JSAMPLE colorlist[])
/* Locate the colormap entries close enough to an update box to be candidates
 * for the nearest entry to some cell(s) in the update box.  The update box
 * is specified by the center coordinates of its first cell.  The number of
 * candidate colormap entries is returned, and their colormap indexes are
 * placed in colorlist[].
 * This routine uses Heckbert's "locally sorted search" criterion to select
 * the colors that need further consideration.
 */
{
  int numcolors =m_Parent->actualNumberOfColors;
  int maxc0, maxc1, maxc2;
  int centerc0, centerc1, centerc2;
  int i, x, ncolors;
  INT32 minmaxdist, min_dist, max_dist, tdist;
  INT32 mindist[MAXNUMCOLORS];	/* min distance to colormap entry i */

  /* Compute true coordinates of update box's upper corner and center.
   * Actually we compute the coordinates of the center of the upper-corner
   * histogram cell, which are the upper bounds of the volume we care about.
   * Note that since ">>" rounds down, the "center" values may be closer to
   * min than to max; hence comparisons to them must be "<=", not "<".
   */
  maxc0 = minc0 + ((1 << BOX_C0_SHIFT) - (1 << C0_SHIFT));
  centerc0 = (minc0 + maxc0) >> 1;
  maxc1 = minc1 + ((1 << BOX_C1_SHIFT) - (1 << C1_SHIFT));
  centerc1 = (minc1 + maxc1) >> 1;
  maxc2 = minc2 + ((1 << BOX_C2_SHIFT) - (1 << C2_SHIFT));
  centerc2 = (minc2 + maxc2) >> 1;

  /* For each color in colormap, find:
   *  1. its minimum squared-distance to any point in the update box
   *     (zero if color is within update box);
   *  2. its maximum squared-distance to any point in the update box.
   * Both of these can be found by considering only the corners of the box.
   * We save the minimum distance for each color in mindist[];
   * only the smallest maximum distance is of interest.
   */
  minmaxdist = 0x7FFFFFFFL;

  for(i=0; i < numcolors; i++) {
    // We compute the squared-c0-distance term, then add in the other two. 
    x = GETJSAMPLE(m_Parent->colormap[0][i]);
    if(x < minc0) {
      tdist = (x - minc0) * C0_SCALE;
      min_dist = tdist*tdist;
      tdist = (x - maxc0) * C0_SCALE;
      max_dist = tdist*tdist;
			} 
		else if (x > maxc0) {
      tdist = (x - maxc0) * C0_SCALE;
      min_dist = tdist*tdist;
      tdist = (x - minc0) * C0_SCALE;
      max_dist = tdist*tdist;
			} 
		else {
      // within cell range so no contribution to min_dist 
      min_dist = 0;
      if(x <= centerc0) {
				tdist = (x - maxc0) * C0_SCALE;
				max_dist = tdist*tdist;
	      } 
			else {
				tdist = (x - minc0) * C0_SCALE;
				max_dist = tdist*tdist;
				}
			}

    x = GETJSAMPLE(m_Parent->colormap[1][i]);
    if(x < minc1) {
      tdist = (x - minc1) * C1_SCALE;
      min_dist += tdist*tdist;
      tdist = (x - maxc1) * C1_SCALE;
      max_dist += tdist*tdist;
			} 
		else if (x > maxc1) {
      tdist = (x - maxc1) * C1_SCALE;
      min_dist += tdist*tdist;
      tdist = (x - minc1) * C1_SCALE;
      max_dist += tdist*tdist;
			} 
		else {
      /* within cell range so no contribution to min_dist */
      if (x <= centerc1) {
			tdist = (x - maxc1) * C1_SCALE;
			max_dist += tdist*tdist;
      } 
		else {
			tdist = (x - minc1) * C1_SCALE;
			max_dist += tdist*tdist;
      }
    }

    x = GETJSAMPLE(m_Parent->colormap[2][i]);
    if(x < minc2) {
      tdist = (x - minc2) * C2_SCALE;
      min_dist += tdist*tdist;
      tdist = (x - maxc2) * C2_SCALE;
      max_dist += tdist*tdist;
			}
		else if (x > maxc2) {
      tdist = (x - maxc2) * C2_SCALE;
      min_dist += tdist*tdist;
      tdist = (x - minc2) * C2_SCALE;
      max_dist += tdist*tdist;
			}
		else {
      /* within cell range so no contribution to min_dist */
      if(x <= centerc2) {
				tdist = (x - maxc2) * C2_SCALE;
				max_dist += tdist*tdist;
				} 
			else {
				tdist = (x - minc2) * C2_SCALE;
				max_dist += tdist*tdist;
      }
    }

    mindist[i] = min_dist;	// save away the results
    if (max_dist < minmaxdist)
      minmaxdist = max_dist;
		}

  /* Now we know that no cell in the update box is more than minmaxdist
   * away from some colormap entry.  Therefore, only colors that are
   * within minmaxdist of some part of the box need be considered.
   */
  ncolors = 0;
  for(i=0; i<numcolors; i++) {
    if(mindist[i] <= minmaxdist)
      colorlist[ncolors++] = (JSAMPLE) i;
		}
  return ncolors;
	}


void CColorQuantizer::findBestColors(int minc0, int minc1, int minc2,
		  int numcolors, JSAMPLE colorlist[], JSAMPLE bestcolor[]) {
/* Find the closest colormap entry for each cell in the update box,
 * given the list of candidate colors prepared by find_nearby_colors.
 * Return the indexes of the closest entries in the bestcolor[] array.
 * This routine uses Thomas' incremental distance calculation method to
 * find the distance from a colormap entry to successive cells in the box.
 */

  int ic0, ic1, ic2;
  int i, icolor;
  register INT32 *bptr;	// pointer into bestdist[] array
  JSAMPLE *cptr;		// pointer into bestcolor[] array
  INT32 dist0, dist1;		// initial distance values
  register INT32 dist2;		// current distance in inner loop
  INT32 xx0, xx1;		// distance increments
  register INT32 xx2;
  INT32 inc0, inc1, inc2;	// initial values for increments
  // This array holds the distance to the nearest-so-far color for each cell
  INT32 bestdist[BOX_C0_ELEMS * BOX_C1_ELEMS * BOX_C2_ELEMS];

  // Initialize best-distance for each cell of the update box
  bptr = bestdist;
  for(i=BOX_C0_ELEMS*BOX_C1_ELEMS*BOX_C2_ELEMS-1; i >= 0; i--)
    *bptr++ = 0x7FFFFFFFL;
  
  /* For each color selected by find_nearby_colors,
   * compute its distance to the center of each cell in the box.
   * If that's less than best-so-far, update best distance and color number.
   */
  
  // Nominal steps between cell centers ("x" in Thomas article) 
#define STEP_C0  ((1 << C0_SHIFT) * C0_SCALE)
#define STEP_C1  ((1 << C1_SHIFT) * C1_SCALE)
#define STEP_C2  ((1 << C2_SHIFT) * C2_SCALE)
  
  for(i = 0; i < numcolors; i++) {
    icolor = GETJSAMPLE(colorlist[i]);
    // Compute (square of) distance from minc0/c1/c2 to this color 
    inc0 = (minc0 - GETJSAMPLE(m_Parent->colormap[0][icolor])) * C0_SCALE;
    dist0 = inc0*inc0;
    inc1 = (minc1 - GETJSAMPLE(m_Parent->colormap[1][icolor])) * C1_SCALE;
    dist0 += inc1*inc1;
    inc2 = (minc2 - GETJSAMPLE(m_Parent->colormap[2][icolor])) * C2_SCALE;
    dist0 += inc2*inc2;
    /* Form the initial difference increments */
    inc0 = inc0 * (2 * STEP_C0) + STEP_C0 * STEP_C0;
    inc1 = inc1 * (2 * STEP_C1) + STEP_C1 * STEP_C1;
    inc2 = inc2 * (2 * STEP_C2) + STEP_C2 * STEP_C2;
    /* Now loop over all cells in box, updating distance per Thomas method */
    bptr = bestdist;
    cptr = bestcolor;
    xx0 = inc0;
    for(ic0 = BOX_C0_ELEMS-1; ic0 >= 0; ic0--) {
      dist1 = dist0;
      xx1 = inc1;
      for(ic1 = BOX_C1_ELEMS-1; ic1 >= 0; ic1--) {
				dist2 = dist1;
				xx2 = inc2;
				for(ic2 = BOX_C2_ELEMS-1; ic2 >= 0; ic2--) {
					if(dist2 < *bptr) {
						*bptr = dist2;
						*cptr = (JSAMPLE) icolor;
						}
					dist2 += xx2;
					xx2 += 2 * STEP_C2 * STEP_C2;
					bptr++;
					cptr++;
					}
				dist1 += xx1;
				xx1 += 2 * STEP_C1 * STEP_C1;
				}
      dist0 += xx0;
      xx0 += 2 * STEP_C0 * STEP_C0;
			}
		}
	}


void CColorQuantizer::fillInverseCmap(int c0, int c1, int c2)
/* Fill the inverse-colormap entries in the update box that contains */
/* histogram cell c0/c1/c2.  (Only that one cell MUST be filled, but */
/* we can fill as many others as we wish.) */
{
	HIST3D myHistogram = histogram;
	int minc0, minc1, minc2;	/* lower left corner of update box */
  int ic0, ic1, ic2;
  register JSAMPLE * cptr;	// pointer into bestcolor[] array
  register HISTPTR cachep;	// pointer into main cache array
  // This array lists the candidate colormap indexes.
  JSAMPLE colorlist[MAXNUMCOLORS];
  int numcolors;		// number of candidate colors
  // This array holds the actually closest colormap index for each cell.
  JSAMPLE bestcolor[BOX_C0_ELEMS * BOX_C1_ELEMS * BOX_C2_ELEMS];

  // Convert cell coordinates to update box ID
  c0 >>= BOX_C0_LOG;
  c1 >>= BOX_C1_LOG;
  c2 >>= BOX_C2_LOG;

  /* Compute true coordinates of update box's origin corner.
   * Actually we compute the coordinates of the center of the corner
   * histogram cell, which are the lower bounds of the volume we care about.
   */
  minc0 = (c0 << BOX_C0_SHIFT) + ((1 << C0_SHIFT) >> 1);
  minc1 = (c1 << BOX_C1_SHIFT) + ((1 << C1_SHIFT) >> 1);
  minc2 = (c2 << BOX_C2_SHIFT) + ((1 << C2_SHIFT) >> 1);
  
  /* Determine which colormap entries are close enough to be candidates
   * for the nearest entry to some cell in the update box.
   */
  numcolors = findNearbyColors(minc0, minc1, minc2, colorlist);

  /* Determine the actually nearest colors. */
  findBestColors(minc0, minc1, minc2, numcolors, colorlist, bestcolor);

  /* Save the best color numbers (plus 1) in the main cache array */
  c0 <<= BOX_C0_LOG;		// convert ID back to base cell indexes
  c1 <<= BOX_C1_LOG;
  c2 <<= BOX_C2_LOG;
  cptr = bestcolor;
  for(ic0 = 0; ic0 < BOX_C0_ELEMS; ic0++) {
    for(ic1 = 0; ic1 < BOX_C1_ELEMS; ic1++) {
      cachep = & histogram[c0+ic0][c1+ic1][c2];
      for(ic2 = 0; ic2 < BOX_C2_ELEMS; ic2++) {
				*cachep++ = (HISTCELL) (GETJSAMPLE(*cptr++) + 1);
	      }
			}
		}
	}


/*
 * Map some rows of pixels to the output colormapped representation.
 */

void CColorQuantizer::pass2_NoDither(JSAMPARRAY input_buf, JSAMPARRAY output_buf, int num_rows)
// This version performs no dithering 
{
  HIST3D myHistogram = histogram;
  register JSAMPROW inptr, outptr;
  register HISTPTR cachep;
  register int c0, c1, c2;
  int row;
  JDIMENSION col;
  JDIMENSION width = m_Parent->outputWidth;

  for(row=0; row < num_rows; row++) {
    inptr = input_buf[row];
    outptr = output_buf[row];
    for(col=width; col > 0; col--) {
      // get pixel value and index into the cache 
      c0 = GETJSAMPLE(*inptr++) >> C0_SHIFT;
      c1 = GETJSAMPLE(*inptr++) >> C1_SHIFT;
      c2 = GETJSAMPLE(*inptr++) >> C2_SHIFT;
      cachep = & histogram[c0][c1][c2];
      // If we have not seen this color before, find nearest colormap entry 
      // and update the cache 
      if(*cachep == 0)
				fillInverseCmap(c0,c1,c2);
      // Now emit the colormap index for this cell 
      *outptr++ = (JSAMPLE) (*cachep - 1);
			}
		}
	}


void CColorQuantizer::pass2_FS_Dither(JSAMPARRAY input_buf, JSAMPARRAY output_buf, int num_rows) {
// This version performs Floyd-Steinberg dithering 
  HIST3D myHistogram = histogram;
  register LOCFSERROR cur0, cur1, cur2;	/* current error or pixel value */
  LOCFSERROR belowerr0, belowerr1, belowerr2; /* error for pixel below cur */
  LOCFSERROR bpreverr0, bpreverr1, bpreverr2; /* error for below/prev col */
  register FSERRPTR errorptr;	/* => fserrors[] at column before current */
  JSAMPROW inptr;		/* => current input pixel */
  JSAMPROW outptr;		/* => current output pixel */
  HISTPTR cachep;
  int dir;			/* +1 or -1 depending on direction */
  int dir3;			/* 3*dir, for advancing inptr & errorptr */
  int row;
  JDIMENSION col;
  JDIMENSION width = m_Parent->outputWidth;
  JSAMPLE *range_limit = m_Parent->sampleRangeLimit;
  int *error_limit = errorLimiter;
  JSAMPROW colormap0 = m_Parent->colormap[0];
  JSAMPROW colormap1 = m_Parent->colormap[1];
  JSAMPROW colormap2 = m_Parent->colormap[2];
  SHIFT_TEMPS

  for(row=0; row < num_rows; row++) {
    inptr = input_buf[row];
    outptr = output_buf[row];
    if(onOddRow) {
      /* work right to left in this row */
      inptr += (width-1) * 3;	/* so point to rightmost pixel */
      outptr += width-1;
      dir = -1;
      dir3 = -3;
      errorptr = fserrors + (width+1)*3; /* => entry after last column */
      onOddRow = FALSE; /* flip for next time */
			}
		else {
      /* work left to right in this row */
      dir = 1;
      dir3 = 3;
      errorptr = fserrors; /* => entry before first real column */
      onOddRow = TRUE; /* flip for next time */
	    }
    /* Preset error values: no error propagated to first pixel from left */
    cur0 = cur1 = cur2 = 0;
    /* and no error propagated to row below yet */
    belowerr0 = belowerr1 = belowerr2 = 0;
    bpreverr0 = bpreverr1 = bpreverr2 = 0;

    for(col=width; col > 0; col--) {
      /* curN holds the error propagated from the previous pixel on the
       * current line.  Add the error propagated from the previous line
       * to form the complete error correction term for this pixel, and
       * round the error term (which is expressed * 16) to an integer.
       * RIGHT_SHIFT rounds towards minus infinity, so adding 8 is correct
       * for either sign of the error value.
       * Note: errorptr points to *previous* column's array entry.
       */
      cur0 = RIGHT_SHIFT(cur0 + errorptr[dir3+0] + 8, 4);
      cur1 = RIGHT_SHIFT(cur1 + errorptr[dir3+1] + 8, 4);
      cur2 = RIGHT_SHIFT(cur2 + errorptr[dir3+2] + 8, 4);
      /* Limit the error using transfer function set by init_error_limit.
       * See comments with init_error_limit for rationale.
       */
      cur0 = error_limit[cur0];
      cur1 = error_limit[cur1];
      cur2 = error_limit[cur2];
      /* Form pixel value + error, and range-limit to 0..MAXJSAMPLE.
       * The maximum error is +- MAXJSAMPLE (or less with error limiting);
       * this sets the required size of the range_limit array.
       */
      cur0 += GETJSAMPLE(inptr[0]);
      cur1 += GETJSAMPLE(inptr[1]);
      cur2 += GETJSAMPLE(inptr[2]);
      cur0 = GETJSAMPLE(range_limit[cur0]);
      cur1 = GETJSAMPLE(range_limit[cur1]);
      cur2 = GETJSAMPLE(range_limit[cur2]);
      /* Index into the cache with adjusted pixel value */
      cachep = & histogram[cur0>>C0_SHIFT][cur1>>C1_SHIFT][cur2>>C2_SHIFT];
      /* If we have not seen this color before, find nearest colormap */
      /* entry and update the cache */
      if(*cachep == 0)
				fillInverseCmap(cur0>>C0_SHIFT,cur1>>C1_SHIFT,cur2>>C2_SHIFT);
      /* Now emit the colormap index for this cell */
      { register int pixcode = *cachep - 1;
				*outptr = (JSAMPLE) pixcode;
				/* Compute representation error for this pixel */
				cur0 -= GETJSAMPLE(colormap0[pixcode]);
				cur1 -= GETJSAMPLE(colormap1[pixcode]);
				cur2 -= GETJSAMPLE(colormap2[pixcode]);
      }
      /* Compute error fractions to be propagated to adjacent pixels.
       * Add these into the running sums, and simultaneously shift the
       * next-line error sums left by 1 column.
       */
      { register LOCFSERROR bnexterr, delta;

			bnexterr = cur0;	/* Process component 0 */
			delta = cur0 * 2;
			cur0 += delta;		/* form error * 3 */
			errorptr[0] = (FSERROR) (bpreverr0 + cur0);
			cur0 += delta;		/* form error * 5 */
			bpreverr0 = belowerr0 + cur0;
			belowerr0 = bnexterr;
			cur0 += delta;		/* form error * 7 */
			bnexterr = cur1;	/* Process component 1 */
			delta = cur1 * 2;
			cur1 += delta;		/* form error * 3 */
			errorptr[1] = (FSERROR) (bpreverr1 + cur1);
			cur1 += delta;		/* form error * 5 */
			bpreverr1 = belowerr1 + cur1;
			belowerr1 = bnexterr;
			cur1 += delta;		/* form error * 7 */
			bnexterr = cur2;	/* Process component 2 */
			delta = cur2 * 2;
			cur2 += delta;		/* form error * 3 */
			errorptr[2] = (FSERROR) (bpreverr2 + cur2);
			cur2 += delta;		/* form error * 5 */
			bpreverr2 = belowerr2 + cur2;
			belowerr2 = bnexterr;
			cur2 += delta;		/* form error * 7 */
      }
      /* At this point curN contains the 7/16 error value to be propagated
       * to the next pixel on the current line, and all the errors for the
       * next line have been shifted over.  We are therefore ready to move on.
       */
      inptr += dir3;		/* Advance pixel pointers to next column */
      outptr += dir;
      errorptr += dir3;		/* advance errorptr to current column */
    }
    /* Post-loop cleanup: we must unload the final error values into the
     * final fserrors[] entry.  Note we need not unload belowerrN because
     * it is for the dummy column before or after the actual array.
     */
    errorptr[0] = (FSERROR) bpreverr0; /* unload prev errs into array */
    errorptr[1] = (FSERROR) bpreverr1;
    errorptr[2] = (FSERROR) bpreverr2;
		}
	}


/*
 * Initialize the error-limiting transfer function (lookup table).
 * The raw F-S error computation can potentially compute error values of up to
 * +- MAXJSAMPLE.  But we want the maximum correction applied to a pixel to be
 * much less, otherwise obviously wrong pixels will be created.  (Typical
 * effects include weird fringes at color-area boundaries, isolated bright
 * pixels in a dark area, etc.)  The standard advice for avoiding this problem
 * is to ensure that the "corners" of the color cube are allocated as output
 * colors; then repeated errors in the same direction cannot cause cascading
 * error buildup.  However, that only prevents the error from getting
 * completely out of hand; Aaron Giles reports that error limiting improves
 * the results even with corner colors allocated.
 * A simple clamping of the error values to about +- MAXJSAMPLE/8 works pretty
 * well, but the smoother transfer function used below is even better.  Thanks
 * to Aaron Giles for this idea.
 */

void CColorQuantizer::initErrorLimit() {
// Allocate and fill in the error_limiter table 
  int * table;
  int in, out;

  table = (int *) (m_Parent->m_Parent->mem->allocSmall)
    (JPOOL_IMAGE, (MAXJSAMPLE*2+1) *sizeof(int));
  table += MAXJSAMPLE;		/* so can index -MAXJSAMPLE .. +MAXJSAMPLE */
  errorLimiter = table;

#define STEPSIZE ((MAXJSAMPLE+1)/16)
  /* Map errors 1:1 up to +- MAXJSAMPLE/16 */
  out = 0;
  for(in=0; in < STEPSIZE; in++, out++) {
    table[in] = out; table[-in] = -out;
		}
  /* Map errors 1:2 up to +- 3*MAXJSAMPLE/16 */
  for(; in < STEPSIZE*3; in++, out += (in&1) ? 0 : 1) {
    table[in] = out; table[-in] = -out;
		}
  /* Clamp the rest to final out value (which is (MAXJSAMPLE+1)/8) */
  for(; in <= MAXJSAMPLE; in++) {
    table[in] = out; table[-in] = -out;
		}
#undef STEPSIZE
	}


/*
 * Finish up at the end of each pass.
 */

void CColorQuantizer::finishPass1() {

  // Select the representative colors and fill in cinfo->colormap 
  m_Parent->colormap = svColormap;
  selectColors(desired);
  // Force next pass to zero the color index table 
  needsZeroed = TRUE;
	}


void CColorQuantizer::finishPass2() {
  // no work 
	}


/*
 * Initialize for each processing pass.
 */

void CColorQuantizer::startPass_2_Quant(BOOL is_pre_scan) {
  HIST3D myHistogram = histogram;
  int i;

  // Only F-S dithering or no dithering is supported.
  // If user asks for ordered dither, give him F-S. 
  if(m_Parent->ditherMode != JDITHER_NONE)
    m_Parent->ditherMode = JDITHER_FS;

  if(is_pre_scan) {
    // Set up method pointers 
    doColorQuantize = prescanQuantize;
    doFinishPass = finishPass1;
    needsZeroed=TRUE; // Always zero histogram 
	  }
	else {
    // Set up method pointers 
    if(m_Parent->ditherMode == JDITHER_FS)
      doColorQuantize = pass2_FS_Dither;
    else
      doColorQuantize =pass2_NoDither;
    doFinishPass=finishPass2;

    /* Make sure color count is acceptable */
    i = m_Parent->actualNumberOfColors;
    if(i < 1)
      m_Parent->m_Parent->err->errorExit(JERR_QUANT_FEW_COLORS, 1);
    if(i > MAXNUMCOLORS)
      m_Parent->m_Parent->err->errorExit(JERR_QUANT_MANY_COLORS, MAXNUMCOLORS);

    if(m_Parent->ditherMode == JDITHER_FS) {
      size_t arraysize = (size_t) ((m_Parent->outputWidth + 2) *
				   (3 * sizeof(FSERROR)));
      // Allocate Floyd-Steinberg workspace if we didn't already.
      if(fserrors == NULL)
				fserrors = (FSERRPTR)(m_Parent->m_Parent->mem->allocLarge)
				  (JPOOL_IMAGE, arraysize);
      // Initialize the propagated errors to zero.
      ZeroMemory((void *)fserrors, arraysize);
      // Make the error-limit table if we didn't already.
      if(errorLimiter == NULL)
				initErrorLimit();
      onOddRow = FALSE;
			}

		}
  // Zero the histogram or inverse color map, if necessary 
  if(needsZeroed) {
    for(i=0; i < HIST_C0_ELEMS; i++) {
      ZeroMemory((void *)histogram[i],HIST_C1_ELEMS*HIST_C2_ELEMS *sizeof(HISTCELL));
	    }
    needsZeroed=FALSE;
		}
	}


/*
 * Switch to a new external colormap between output passes.
 */

void CColorQuantizer::newColorMap_2_Quant() {

  // Reset the inverse color map 
  needsZeroed = TRUE;
	}


/*
 * Module initialization routine for 2-pass color quantization.
 */

void CColorQuantizer::init_2passQuantizer() {
  int i;

  doStartPass = startPass_2_Quant;
  doNewColorMap = newColorMap_2_Quant;
  fserrors = NULL;	// flag optional arrays not allocated
  errorLimiter =NULL;

  // Make sure jdmaster didn't give me a case I can't handle
  if(m_Parent->outColorComponents != 3)
    m_Parent->m_Parent->err->errorExit(JERR_NOTIMPL);

  /* Allocate the histogram/inverse colormap storage */
  histogram = (HIST3D) (m_Parent->m_Parent->mem->allocSmall)
    (JPOOL_IMAGE, HIST_C0_ELEMS * sizeof(HIST2D));
  for(i=0; i < HIST_C0_ELEMS; i++) {
    histogram[i] = (HIST2D) (m_Parent->m_Parent->mem->allocLarge)
      (JPOOL_IMAGE,
       HIST_C1_ELEMS*HIST_C2_ELEMS * sizeof(HISTCELL));
		}
  needsZeroed = TRUE; /* histogram is garbage now */

  /* Allocate storage for the completed colormap, if required.
   * We do this now since it is FAR storage and may affect
   * the memory manager's space calculations.
   */
  if(m_Parent->enable2passQuant) {
    // Make sure color count is acceptable
    int myDesired = m_Parent->desiredNumberOfColors;
    // Lower bound on # of colors ... somewhat arbitrary as long as > 0
    if(myDesired<8)
      m_Parent->m_Parent->err->errorExit(JERR_QUANT_FEW_COLORS, 8);
    // Make sure colormap indexes can be represented by JSAMPLEs
    if(myDesired > MAXNUMCOLORS)
      m_Parent->m_Parent->err->errorExit(JERR_QUANT_MANY_COLORS, MAXNUMCOLORS);
    svColormap = m_Parent->m_Parent->mem->allocSarray(JPOOL_IMAGE, (JDIMENSION)desired, (JDIMENSION) 3);
    desired = myDesired;
		} 
	else
    svColormap = NULL;

  /* Only F-S dithering or no dithering is supported. */
  /* If user asks for ordered dither, give him F-S. */
  if(m_Parent->ditherMode != JDITHER_NONE)
    m_Parent->ditherMode = JDITHER_FS;

  /* Allocate Floyd-Steinberg workspace if necessary.
   * This isn't really needed until pass 2, but again it is FAR storage.
   * Although we will cope with a later change in dither_mode,
   * we do not promise to honor max_memory_to_use if dither_mode changes.
   */
  if(m_Parent->ditherMode == JDITHER_FS) {
    fserrors = (FSERRPTR)m_Parent->m_Parent->mem->allocLarge(JPOOL_IMAGE,(size_t)((m_Parent->outputWidth + 2) * (3 * sizeof(FSERROR))));
    // Might as well create the error-limiting table too.
    initErrorLimit();
		}
	}

#endif // QUANT_2PASS_SUPPORTED


CColorQuantizer::CColorQuantizer(CDecompressJpeg *p,int mode) : m_Parent(p) {

	}



/*
 * jdcoefct.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the coefficient buffer controller for decompression.
 * This controller is the top level of the JPEG decompressor proper.
 * The coefficient buffer lies between entropy decoding and inverse-DCT steps.
 *
 * In buffered-image mode, this controller is the interface between
 * input-oriented processing and output-oriented processing.
 * Also, the input side (only) is used when reading a file for transcoding.
 */


/* Block smoothing is only applicable for progressive JPEG, so: */
#ifndef D_PROGRESSIVE_SUPPORTED
#undef BLOCK_SMOOTHING_SUPPORTED
#endif



void CDCoefController::start_iMCU_row()
/* Reset within-iMCU-row counters for a new row (input side) */
{

  /* In an interleaved scan, an MCU row is the same as an iMCU row.
   * In a noninterleaved scan, an iMCU row has v_samp_factor MCU rows.
   * But at the bottom of the image, process only what's left.
   */
  if(m_Parent->compsInScan > 1) {
    MCU_rowsPer_iMCU_row = 1;
		} 
	else {
    if(m_Parent->input_iMCU_row < (m_Parent->total_iMCU_rows-1))
      MCU_rowsPer_iMCU_row = m_Parent->curCompInfo[0]->vSampFactor;
    else
      MCU_rowsPer_iMCU_row = m_Parent->curCompInfo[0]->lastRowHeight;
		}

  MCU_ctr = 0;
  MCU_vertOffset = 0;
	}


/*
 * Initialize for an input processing pass.
 */

void CDCoefController::startInputPass() {
  
	m_Parent->input_iMCU_row = 0;
  start_iMCU_row();
	}


/*
 * Initialize for an output processing pass.
 */

void CDCoefController::startOutputPass() {
#ifdef BLOCK_SMOOTHING_SUPPORTED

  /* If multipass, check to see whether to use block smoothing on this pass */
  if(coefArrays != NULL) {
    if(m_Parent->doBlockSmoothing && smoothing_ok())
      doDecompressData = decompressSmoothData;
    else
      doDecompressData = decompressData;
		}
#endif
  m_Parent->output_iMCU_row = 0;
	}


/*
 * Decompress and return some data in the single-pass case.
 * Always attempts to emit one fully interleaved MCU row ("iMCU" row).
 * Input and output must run in lockstep since we have only a one-MCU buffer.
 * Return value is JPEG_ROW_COMPLETED, JPEG_SCAN_COMPLETED, or JPEG_SUSPENDED.
 *
 * NB: output_buf contains a plane for each component in image.
 * For single pass, this is the same as the components in the scan.
 */

int CDCoefController::decompressOnepass(JSAMPIMAGE output_buf) {
  JDIMENSION MCU_col_num;	/* index of current MCU within row */
  JDIMENSION last_MCU_col = m_Parent->MCUs_perRow - 1;
  JDIMENSION last_iMCU_row = m_Parent->total_iMCU_rows - 1;
  int blkn, ci, xindex, yindex, yoffset, usefulWidth;
  JSAMPARRAY output_ptr;
  JDIMENSION start_col, output_col;
  J_COMPONENT_INFO *compptr;

  /* Loop to process as much as one whole iMCU row */
  for(yoffset = MCU_vertOffset; yoffset < MCU_rowsPer_iMCU_row; yoffset++) {
    for(MCU_col_num = MCU_ctr; MCU_col_num <= last_MCU_col; MCU_col_num++) {

      /* Try to fetch an MCU.  Entropy decoder expects buffer to be zeroed. */
      ZeroMemory((void *) MCU_buffer[0],
				(size_t) (m_Parent->blocksInMCU * sizeof(JBLOCK)));
      if(! (m_Parent->entropy->*m_Parent->entropy->doDecodeMCU) (MCU_buffer)) {
	/* Suspension forced; update state counters and exit */
				MCU_vertOffset = yoffset;
				MCU_ctr = MCU_col_num;
				return J_SUSPENDED;
				}
      /* Determine where data should go in output_buf and do the IDCT thing.
       * We skip dummy blocks at the right and bottom edges (but blkn gets
       * incremented past them!).  Note the inner loop relies on having
       * allocated the MCU_buffer[] blocks sequentially.
       */
      blkn = 0;			/* index of current DCT block within MCU */
      for(ci = 0; ci < m_Parent->compsInScan; ci++) {
				compptr = m_Parent->curCompInfo[ci];
	/* Don't bother to IDCT an uninteresting component. */
				if(! compptr->componentNeeded) {
					blkn += compptr->MCU_blocks;
					continue;
					}
				usefulWidth = (MCU_col_num < last_MCU_col) ? compptr->MCU_width
						    : compptr->lastColWidth;
				output_ptr = output_buf[ci] + yoffset * compptr->DCT_scaledSize;
				start_col = MCU_col_num * compptr->MCU_sampleWidth;
				for(yindex = 0; yindex < compptr->MCU_height; yindex++) {
					if(m_Parent->input_iMCU_row < last_iMCU_row ||
							yoffset+yindex < compptr->lastRowHeight) {
						output_col = start_col;
						for(xindex = 0; xindex < usefulWidth; xindex++) {
							(m_Parent->idct->*(m_Parent->idct)->doInverse_DCT[compptr->componentIndex]) (compptr,
									(JCOEFPTR) MCU_buffer[blkn+xindex],
									output_ptr, output_col);
							output_col += compptr->DCT_scaledSize;
							}
						}
					blkn += compptr->MCU_width;
					output_ptr += compptr->DCT_scaledSize;
					}
				}
			}
    /* Completed an MCU row, but perhaps not an iMCU row */
    MCU_ctr = 0;
		}
  /* Completed the iMCU row, advance counters for next one */
  m_Parent->output_iMCU_row++;
  if(++(m_Parent->input_iMCU_row) < m_Parent->total_iMCU_rows) {
    start_iMCU_row();
		return J_ROW_COMPLETED;
		}
  /* Completed the scan */
  (m_Parent->inputctl->finishInputPass) ();
  return J_SCAN_COMPLETED;
	}


/*
 * Dummy consume-input routine for single-pass operation.
 */

int CDCoefController::dummyConsumeData() {

  return J_SUSPENDED;	/* Always indicate nothing was done */
	}


#ifdef D_MULTISCAN_FILES_SUPPORTED

/*
 * Consume input data and store it in the full-image coefficient buffer.
 * We read as much as one fully interleaved MCU row ("iMCU" row) per call,
 * ie, v_samp_factor block rows for each component in the scan.
 * Return value is JPEG_ROW_COMPLETED, JPEG_SCAN_COMPLETED, or JPEG_SUSPENDED.
 */

int CDCoefController::consumeData() {
  JDIMENSION MCU_col_num;	/* index of current MCU within row */
  int blkn, ci, xindex, yindex, yoffset;
  JDIMENSION start_col;
  JBLOCKARRAY buffer[MAX_COMPS_IN_SCAN];
  JBLOCKROW buffer_ptr;
  J_COMPONENT_INFO *compptr;

  /* Align the virtual buffers for the components used in this scan. */
  for(ci=0; ci < m_Parent->compsInScan; ci++) {
    compptr = m_Parent->curCompInfo[ci];
//    buffer[ci] = (*m_Parent->m_Parent->mem->accessVirtBarray)
//      (wholeImage[compptr->componentIndex],
//       m_Parent->input_iMCU_row * compptr->vSampFactor,
//       (JDIMENSION) compptr->vSampFactor, TRUE);
    buffer[ci]= wholeImage[compptr->componentIndex]->
			accessVirtBarray(m_Parent->input_iMCU_row*compptr->vSampFactor,
      (JDIMENSION)compptr->vSampFactor, TRUE);
    /* Note: entropy decoder expects buffer to be zeroed,
     * but this is handled automatically by the memory manager
     * because we requested a pre-zeroed array.
     */
		}

  /* Loop to process one whole iMCU row */
  for(yoffset = MCU_vertOffset; yoffset < MCU_rowsPer_iMCU_row;
    yoffset++) {
    for(MCU_col_num = MCU_ctr; MCU_col_num < m_Parent->MCUs_perRow;
			MCU_col_num++) {
      /* Construct list of pointers to DCT blocks belonging to this MCU */
      blkn = 0;			/* index of current DCT block within MCU */
      for (ci = 0; ci < m_Parent->compsInScan; ci++) {
				compptr = m_Parent->curCompInfo[ci];
				start_col = MCU_col_num * compptr->MCU_width;
			for(yindex = 0; yindex < compptr->MCU_height; yindex++) {
				buffer_ptr = buffer[ci][yindex+yoffset] + start_col;
				for(xindex = 0; xindex < compptr->MCU_width; xindex++) {
					MCU_buffer[blkn++] = buffer_ptr++;
					}
				}
      }
      /* Try to fetch the MCU. */
      if(! (m_Parent->entropy->*m_Parent->entropy->doDecodeMCU) (MCU_buffer)) {
				/* Suspension forced; update state counters and exit */
				MCU_vertOffset = yoffset;
				MCU_ctr = MCU_col_num;
				return J_SUSPENDED;
				}
			}
    /* Completed an MCU row, but perhaps not an iMCU row */
    MCU_ctr = 0;
		}
  /* Completed the iMCU row, advance counters for next one */
  if(++(m_Parent->input_iMCU_row) < m_Parent->total_iMCU_rows) {
    start_iMCU_row();
    return J_ROW_COMPLETED;
		}
  /* Completed the scan */
  m_Parent->inputctl->finishInputPass();
  return J_SCAN_COMPLETED;
	}


/*
 * Decompress and return some data in the multi-pass case.
 * Always attempts to emit one fully interleaved MCU row ("iMCU" row).
 * Return value is JPEG_ROW_COMPLETED, JPEG_SCAN_COMPLETED, or JPEG_SUSPENDED.
 *
 * NB: output_buf contains a plane for each component in image.
 */

int CDCoefController::decompressData(JSAMPIMAGE output_buf) {
  JDIMENSION last_iMCU_row = m_Parent->total_iMCU_rows - 1;
  JDIMENSION block_num;
  int ci, block_row, block_rows;
  JBLOCKARRAY buffer;
  JBLOCKROW buffer_ptr;
  JSAMPARRAY output_ptr;
  JDIMENSION output_col;
  J_COMPONENT_INFO *compptr;
//  inverse_DCT_method_ptr inverse_DCT;

  /* Force some input to be done if we are getting ahead of the input. */
  while(m_Parent->inputScanNumber < m_Parent->outputScanNumber ||
	 (m_Parent->inputScanNumber == m_Parent->outputScanNumber &&
	  m_Parent->input_iMCU_row <= m_Parent->output_iMCU_row)) {
    if((m_Parent->consumeInput) () == J_SUSPENDED)
      return J_SUSPENDED;
	  }

  /* OK, output from the virtual arrays. */
  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    /* Don't bother to IDCT an uninteresting component. */
    if(!compptr->componentNeeded)
      continue;
    /* Align the virtual buffer for this component. */
//    buffer = (*m_Parent->m_Parent->mem->accessVirtBarray)
//      (wholeImage[ci],
//      m_Parent->output_iMCU_row * compptr->vSampFactor,
//      (JDIMENSION) compptr->vSampFactor, FALSE);
    buffer = wholeImage[compptr->componentIndex]->
			accessVirtBarray(m_Parent->output_iMCU_row*compptr->vSampFactor,
      (JDIMENSION)compptr->vSampFactor, FALSE);
    /* Count non-dummy DCT block rows in this iMCU row. */
    if(m_Parent->output_iMCU_row < last_iMCU_row)
      block_rows = compptr->vSampFactor;
    else {
      /* NB: can't use last_row_height here; it is input-side-dependent! */
      block_rows = (int) (compptr->heightInBlocks % compptr->vSampFactor);
      if(block_rows == 0) 
				block_rows = compptr->vSampFactor;
			}
//    inverse_DCT = m_Parent->idct->doInverse_DCT[ci];
    output_ptr = output_buf[ci];
    /* Loop over all DCT blocks to be processed. */
    for(block_row = 0; block_row < block_rows; block_row++) {
      buffer_ptr = buffer[block_row];
      output_col = 0;
      for(block_num = 0; block_num < compptr->widthInBlocks; block_num++) {
				(m_Parent->idct->*(m_Parent->idct)->doInverse_DCT[compptr->componentIndex]) (compptr,
						(JCOEFPTR)buffer_ptr,
						output_ptr, output_col);
//				(*doInverse_DCT) (compptr, (JCOEFPTR)buffer_ptr, output_ptr, output_col);
				buffer_ptr++;
				output_col += compptr->DCT_scaledSize;
				}
      output_ptr += compptr->DCT_scaledSize;
			}
		}

  if(++(m_Parent->output_iMCU_row) < m_Parent->total_iMCU_rows)
    return J_ROW_COMPLETED;

  return J_SCAN_COMPLETED;
	}

CVirtBArray *CDCoefController::requestVirtBarray(int pool_id, BOOL pre_zero,
	JDIMENSION blocksperrow, JDIMENSION numrows, JDIMENSION maxaccess)
// Request a virtual 2-D coefficient-block array
{
  CVirtBArray *result;

  // Only IMAGE-lifetime virtual arrays are currently supported
  if(pool_id != JPOOL_IMAGE)
    m_Parent->m_Parent->err->errorExit(JERR_BAD_POOL_ID, pool_id);	// safety check

  // get control block
  result=new CVirtBArray(pre_zero, blocksperrow, numrows, maxaccess);

  result->next = virtBarrayList; // add to list of virtual arrays
  virtBarrayList = result;

  return result;
	}

#endif /* D_MULTISCAN_FILES_SUPPORTED */


#ifdef BLOCK_SMOOTHING_SUPPORTED

/*
 * This code applies interblock smoothing as described by section K.8
 * of the JPEG standard: the first 5 AC coefficients are estimated from
 * the DC values of a DCT block and its 8 neighboring blocks.
 * We apply smoothing only for progressive JPEG decoding, and only if
 * the coefficients it can estimate are not yet known to full precision.
 */

/* Natural-order array positions of the first 5 zigzag-order coefficients */
#define Q01_POS  1
#define Q10_POS  8
#define Q20_POS  16
#define Q11_POS  9
#define Q02_POS  2

/*
 * Determine whether block smoothing is applicable and safe.
 * We also latch the current states of the coef_bits[] entries for the
 * AC coefficients; otherwise, if the input side of the decompressor
 * advances into a new scan, we might think the coefficients are known
 * more accurately than they really are.
 */

BOOL CDCoefController::smoothing_ok() {
  BOOL smoothing_useful = FALSE;
  int ci, coefi;
	J_COMPONENT_INFO *compptr;
  JQUANT_TBL * qtable;
  int *coef_bits;
  int *coef_bits_latch;

  if(!m_Parent->progressiveMode || m_Parent->coefBits == NULL)
    return FALSE;

  /* Allocate latch area if not already done */
  if(coefBitsLatch == NULL)
    coefBitsLatch = (int *)
      (*m_Parent->m_Parent->mem->allocSmall) (JPOOL_IMAGE,
			  m_Parent->numComponents * (SAVED_COEFS * sizeof(int)));
	coef_bits_latch = coefBitsLatch;

	for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
		/* All components' quantization values must already be latched. */
		if((qtable = compptr->quantTable) == NULL)
			return FALSE;
		/* Verify DC & first 5 AC quantizers are nonzero to avoid zero-divide. */
		if(qtable->quantval[0] == 0 || qtable->quantval[Q01_POS] == 0 ||
			qtable->quantval[Q10_POS] == 0 ||	qtable->quantval[Q20_POS] == 0 ||
			qtable->quantval[Q11_POS] == 0 || qtable->quantval[Q02_POS] == 0)
			return FALSE;
    /* DC values must be at least partly known for all components. */
    coef_bits = m_Parent->coefBits[ci];
    if(coef_bits[0] < 0)
      return FALSE;
    /* Block smoothing is helpful if some AC coefficients remain inaccurate. */
    for(coefi=1; coefi <= 5; coefi++) {
      coef_bits_latch[coefi] = coef_bits[coefi];
      if(coef_bits[coefi] != 0)
				smoothing_useful = TRUE;
			}
    coef_bits_latch += SAVED_COEFS;
		}

  return smoothing_useful;
	}


/*
 * Variant of decompress_data for use when doing block smoothing.
 */

int CDCoefController::decompressSmoothData(JSAMPIMAGE output_buf) {
  JDIMENSION last_iMCU_row = m_Parent->total_iMCU_rows - 1;
  JDIMENSION block_num, last_block_column;
  int ci, block_row, block_rows, access_rows;
  JBLOCKARRAY buffer;
  JBLOCKROW buffer_ptr, prev_block_row, next_block_row;
  JSAMPARRAY output_ptr;
  JDIMENSION output_col;
	J_COMPONENT_INFO *compptr;
  CInverseDCT *inverse_DCT;
  boolean first_row, last_row;
  JBLOCK workspace;
  int *coef_bits;
  JQUANT_TBL *quanttbl;
  INT32 Q00,Q01,Q02,Q10,Q11,Q20, num;
  int DC1,DC2,DC3,DC4,DC5,DC6,DC7,DC8,DC9;
  int Al, pred;

  /* Force some input to be done if we are getting ahead of the input. */
  while(m_Parent->inputScanNumber <= m_Parent->outputScanNumber &&
	  ! m_Parent->inputctl->EOI_Reached) {
    if(m_Parent->inputScanNumber == m_Parent->outputScanNumber) {
      /* If input is working on current scan, we ordinarily want it to
       * have completed the current row.  But if input scan is DC,
       * we want it to keep one row ahead so that next block row's DC
       * values are up to date.
       */
      JDIMENSION delta = (m_Parent->Ss == 0) ? 1 : 0;
      if(m_Parent->input_iMCU_row > m_Parent->output_iMCU_row+delta)
			break;
			}
    if((inputctl->*inputctl->doConsumeInput)() == J_SUSPENDED)
      return JPEG_SUSPENDED;
		}

  /* OK, output from the virtual arrays. */
  for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
    /* Don't bother to IDCT an uninteresting component. */
    if(! compptr->component_needed)
      continue;
    /* Count non-dummy DCT block rows in this iMCU row. */
    if(m_Parent->output_iMCU_row < last_iMCU_row) {
      blockRows = compptr->vSampFactor;
      accessRows = blockRows * 2; /* this and next iMCU row */
      lastRow = FALSE;
			}
		else {
      /* NB: can't use last_row_height here; it is input-side-dependent! */
      blockRows = (int) (compptr->heightInBlocks % compptr->v_samp_factor);
      if(blockRows == 0) 
				blockRows = compptr->vSampFactor;
      accessRows = blockRows; /* this iMCU row only */
      lastRow = TRUE;
			}
    /* Align the virtual buffer for this component. */
    if(m_Parent->output_iMCU_row > 0) {
      accessRows += compptr->vSampFactor; /* prior iMCU row too */
      buffer = (*m_Parent->mem->access_virt_barray)
				(m_Parent->coef->whole_image[ci],
				(m_Parent->output_iMCU_row - 1) * compptr->vSampFactor,
			(JDIMENSION) accessRows, FALSE);
      buffer += compptr->vSampFactor;	/* point to current iMCU row */
      first_row = FALSE;
			}
		else {
      buffer = (*m_Parent->mem->access_virt_barray)
				(m_Parent->coef->whole_image[ci],
				(JDIMENSION) 0, (JDIMENSION) access_rows, FALSE);
				first_row = TRUE;
			}
    /* Fetch component-dependent info */
    coefBits = coefBitsLatch + (ci * SAVED_COEFS);
    quanttbl = compptr->quantTable;
    Q00 = quanttbl->quantval[0];
    Q01 = quanttbl->quantval[Q01_POS];
    Q10 = quanttbl->quantval[Q10_POS];
    Q20 = quanttbl->quantval[Q20_POS];
    Q11 = quanttbl->quantval[Q11_POS];
    Q02 = quanttbl->quantval[Q02_POS];
    inverse_DCT = m_Parent->idct->inverse_DCT[ci];
    output_ptr = output_buf[ci];
    /* Loop over all DCT blocks to be processed. */
    for(block_row = 0; block_row < block_rows; block_row++) {
      buffer_ptr = buffer[block_row];
      if(firstRow && blockRow == 0)
				prevBlockRow = buffer_ptr;
      else
				prevBlockRow = buffer[blockRow-1];
      if (last_row && blockRow == blockRows-1)
				nextBlockRow = bufferPtr;
      else
				nextBlockRow = buffer[blockRow+1];
      /* We fetch the surrounding DC values using a sliding-register approach.
       * Initialize all nine here so as to do the right thing on narrow pics.
       */
      DC1 = DC2 = DC3 = (int) prevBlockRow[0][0];
      DC4 = DC5 = DC6 = (int) bufferPtr[0][0];
      DC7 = DC8 = DC9 = (int) nextBlockRow[0][0];
      output_col = 0;
      lastBlockColumn = compptr->widthInBlocks - 1;
      for(blockNum = 0; block_num <= lastBlockColumn; blockNum++) {
				/* Fetch current DCT block into workspace so we can modify it. */
				jcopyBlockRow(bufferPtr, (JBLOCKROW) workspace, (JDIMENSION) 1);
				/* Update DC values */
				if(block_num < lastBlockColumn) {
					DC3 = (int) prev_block_row[1][0];
					DC6 = (int) buffer_ptr[1][0];
					DC9 = (int) next_block_row[1][0];
				}
	/* Compute coefficient estimates per K.8.
	 * An estimate is applied only if coefficient is still zero,
	 * and is not known to be fully accurate.
	 */
	/* AC01 */
			if((Al=coef_bits[1]) != 0 && workspace[1] == 0) {
				num = 36 * Q00 * (DC4 - DC6);
				if (num >= 0) {
					pred = (int) (((Q01<<7) + num) / (Q01<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					}
				else {
					pred = (int) (((Q01<<7) - num) / (Q01<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					pred = -pred;
					}
				workspace[1] = (JCOEF) pred;
				}
	/* AC10 */
			if((Al=coef_bits[2]) != 0 && workspace[8] == 0) {
				num = 36 * Q00 * (DC2 - DC8);
				if(num >= 0) {
					pred = (int) (((Q10<<7) + num) / (Q10<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					} 
				else {
					pred = (int) (((Q10<<7) - num) / (Q10<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					pred = -pred;
					}
				workspace[8] = (JCOEF) pred;
				}
	/* AC20 */
			if((Al=coef_bits[3]) != 0 && workspace[16] == 0) {
				num = 9 * Q00 * (DC2 + DC8 - 2*DC5);
				if (num >= 0) {
					pred = (int) (((Q20<<7) + num) / (Q20<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					}
				else {
					pred = (int) (((Q20<<7) - num) / (Q20<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					pred = -pred;
					}
				workspace[16] = (JCOEF) pred;
				}
	/* AC11 */
			if ((Al=coef_bits[4]) != 0 && workspace[9] == 0) {
				num = 5 * Q00 * (DC1 - DC3 - DC7 + DC9);
				if (num >= 0) {
					pred = (int) (((Q11<<7) + num) / (Q11<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					}
				else {
					pred = (int) (((Q11<<7) - num) / (Q11<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					pred = -pred;
					}
				workspace[9] = (JCOEF) pred;
				}
	/* AC02 */
			if ((Al=coef_bits[5]) != 0 && workspace[2] == 0) {
				num = 9 * Q00 * (DC4 + DC6 - 2*DC5);
				if (num >= 0) {
					pred = (int) (((Q02<<7) + num) / (Q02<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					}
				else {
					pred = (int) (((Q02<<7) - num) / (Q02<<8));
					if (Al > 0 && pred >= (1<<Al))
						pred = (1<<Al)-1;
					pred = -pred;
					}
				workspace[2] = (JCOEF) pred;
				}
	/* OK, do the IDCT */
				(*inverse_DCT) (compptr, (JCOEFPTR) workspace, output_ptr, output_col);
				/* Advance for next column */
				DC1 = DC2; DC2 = DC3;
				DC4 = DC5; DC5 = DC6;
				DC7 = DC8; DC8 = DC9;
				buffer_ptr++, prev_block_row++, next_block_row++;
				output_col += compptr->DCT_scaledSize;
				}
      output_ptr += compptr->DCT_scaledSize;
			}
		}

  if(++(m_Parent->output_iMCU_row) < m_Parent->total_iMCU_rows)
    return J_ROW_COMPLETED;

  return J_SCAN_COMPLETED;
	}

#endif /* BLOCK_SMOOTHING_SUPPORTED */


/*
 * Initialize coefficient buffer controller.
 */

CDCoefController::CDCoefController(CDecompressJpeg *p,BOOL needFullBuffer) : m_Parent(p) {

#ifdef BLOCK_SMOOTHING_SUPPORTED
  coefBitsLatch = NULL;
#endif

	virtBarrayList=NULL;

  /* Create the coefficient buffer. */
  if(needFullBuffer) {
#ifdef D_MULTISCAN_FILES_SUPPORTED
    /* Allocate a full-image virtual array for each component, */
    /* padded to a multiple of samp_factor DCT blocks in each direction. */
    /* Note we ask for a pre-zeroed array. */
    int ci, accessRows;
    J_COMPONENT_INFO *compptr;

    for(ci=0, compptr = m_Parent->compInfo; ci < m_Parent->numComponents; ci++, compptr++) {
      accessRows = compptr->vSampFactor;
#ifdef BLOCK_SMOOTHING_SUPPORTED
      /* If block smoothing could be used, need a bigger window */
      if(m_Parent->progressiveMode)
				access_rows *= 3;
#endif
      wholeImage[ci] = requestVirtBarray(JPOOL_IMAGE, TRUE,
				(JDIMENSION)CJpeg::roundUp((long) compptr->widthInBlocks,
				(long) compptr->hSampFactor),
				(JDIMENSION)CJpeg::roundUp((long) compptr->heightInBlocks,
				(long) compptr->vSampFactor),
				(JDIMENSION)accessRows);
				}
			doConsumeData = consumeData;
			doDecompressData = decompressData;
			coefArrays = (CVirtSArray **)wholeImage; /* link to virtual arrays */
#else
	    m_Parent->m_Parent->err->errorExit(JERR_NOT_COMPILED);
#endif
			}
	else {
    /* We only need a single-MCU buffer. */
    JBLOCKROW buffer;
    int i;

    buffer = (JBLOCKROW)(m_Parent->m_Parent->mem->allocLarge) (JPOOL_IMAGE,
		  D_MAX_BLOCKS_IN_MCU * sizeof(JBLOCK));
    for(i=0; i < D_MAX_BLOCKS_IN_MCU; i++) {
      MCU_buffer[i] = buffer + i;
			}
    doConsumeData = dummyConsumeData;
    doDecompressData = decompressOnepass;
    coefArrays = NULL; /* flag for no virtual arrays */
		}
	}







/*
 * jutils.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains tables and miscellaneous utility routines needed
 * for both compression and decompression.
 * Note we prefix all global names with "j" to minimize conflicts with
 * a surrounding application.
 */


/*
 * jpeg_zigzag_order[i] is the zigzag-order position of the i'th element
 * of a DCT block read in natural order (left to right, top to bottom).
 */

#if 0				// This table is not actually needed in v6a

const int jpegZigzagOrder[DCTSIZE2] = {
   0,  1,  5,  6, 14, 15, 27, 28,
   2,  4,  7, 13, 16, 26, 29, 42,
   3,  8, 12, 17, 25, 30, 41, 43,
   9, 11, 18, 24, 31, 40, 44, 53,
  10, 19, 23, 32, 39, 45, 52, 54,
  20, 22, 33, 38, 46, 51, 55, 60,
  21, 34, 37, 47, 50, 56, 59, 61,
  35, 36, 48, 49, 57, 58, 62, 63
	};

#endif


/*
 * Arithmetic utilities
 */

long CJpeg::divRoundUp(long a, long b) {
// Compute a/b rounded up to next integer, ie, ceil(a/b) 
// Assumes a >= 0, b > 0 

  return (a + b - 1L) / b;
	}


long CJpeg::roundUp(long a, long b) {
// Compute a rounded up to next multiple of b, ie, ceil(a/b)*b 
// Assumes a >= 0, b > 0 

  a += b - 1L;
  return a - (a % b);
	}




void CCompressJpeg::copySampleRows(JSAMPARRAY input_array, int source_row,
	JSAMPARRAY output_array, int dest_row,
	int num_rows, JDIMENSION num_cols)
/* Copy some rows of samples from one place to another.
 * num_rows rows are copied from input_array[source_row++]
 * to output_array[dest_row++]; these areas may overlap for duplication.
 * The source and destination arrays must be at least as wide as num_cols.
 */
{
  register JSAMPROW inptr, outptr;
  register size_t count = (size_t)(num_cols*sizeof(JSAMPLE));
  register int row;

  input_array += source_row;
  output_array += dest_row;

  for(row=num_rows; row > 0; row--) {
    inptr=*input_array++;
    outptr=*output_array++;
    memcpy(outptr, inptr, count);
		}
	}


void CCompressJpeg::copyBlockRow(JBLOCKROW input_row, JBLOCKROW output_row,
		 JDIMENSION num_blocks) {
// Copy a row of coefficient blocks from one place to another. 

  memcpy(output_row, input_row, num_blocks * (DCTSIZE2 * sizeof(JCOEF)));
	}



CJpeg::CJpeg(const char *appName,void *cdata) : co(0), de(0), mem(0), progress(0) {

	*m_appname=0;
	if(appName) {
		_tcsncpy(m_appname,appName,127);
		m_appname[127]=0;
		}
	isDecompressor=0;
	clientData=cdata;
  err=new CErrorMgr(this);
	memInit();
	}

CJpeg::~CJpeg() {
	
	memTerm();
	delete progress;
	delete mem;
	delete err;
	delete co;
	delete de;
	}

/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

void CJpeg::setDest(const char *nomefile) {

  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
  if(!co->dest) {	// first time for this JPEG object?
    co->dest=new CDestMgr(this,nomefile);
		}
	}

void CJpeg::setDest(CFile *f) {

  if(!co->dest) {	// first time for this JPEG object?
    co->dest=new CDestMgr(this,f);
		}
	}

void CJpeg::setDest(BYTE *d,DWORD n) {

  if(!co->dest) {	// first time for this JPEG object?
    co->dest=(CDestMgr *)(new CDestMgr(this,d,n));
		}
	}

void CJpeg::setSource(const char *nomefile) {

  /* The source object and input buffer are made permanent so that a series
   * of JPEG images can be read from the same file by calling jpeg_stdio_src
   * only before the first one.  (If we discarded the buffer at the end of
   * one image, we'd likely lose the start of the next one.)
   * This makes it unsafe to use this manager and a different source
   * manager serially with the same JPEG object.  Caveat programmer.
   */
  if(!de->src) {	// first time for this JPEG object?
    de->src=new CSourceMgr(this,nomefile);
		if(de->marker)
			de->marker->setSource(de->src);
		}
	}

void CJpeg::setSource(CFile *f) {

  if(!de->src) {	// first time for this JPEG object?
    de->src=new CSourceMgr(this,f);
		if(de->marker)
			de->marker->setSource(de->src);
		}
	}

void CJpeg::setSource(BYTE *d,DWORD n) {

  if(!de->src) {	// first time for this JPEG object?
    de->src=(CSourceMgr *)(new CSourceMgr(this,d,n));
		if(de->marker)
			de->marker->setSource(de->src);
		}
	}



/*
 * Sample routine for JPEG compression.  We assume that the target file name
 * and a compression quality factor are passed in.
 */

int CJpeg::writeJPEGFile(JSAMPLE *imageBuffer,int imageWidth,int imageHeight,const char *filename, 
												 int quality, const char *note,const char *autore) {

  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  // More stuff
  JSAMPROW rowPointer[1];	/* pointer to JSAMPLE row[s] */
  int rowStride;		/* physical row width in image buffer */

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  // Now we can initialize the JPEG compression object.
  createCompress(note);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */
  setDest(filename);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  co->imageWidth = imageWidth; 	// image width and height, in pixels 
  co->imageHeight = imageHeight;
  co->inputComponents = 3;		// # of color components per pixel
  co->inColorSpace = JCS_RGB; 	// colorspace of input image 
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  co->setDefaults();
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  co->setQuality(quality,TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  co->startCompress(TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  rowStride = imageWidth * 3;	// JSAMPLEs per row in image_buffer 

  while(co->nextScanline < co->imageHeight) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    rowPointer[0]= &imageBuffer[co->nextScanline*rowStride];
    co->writeScanlines(rowPointer, 1);
		}

  /* Step 6: Finish compression */
  co->finishCompress();

  /* After finish_compress, we can close the output file. */

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  destroyCompress();

  // And we're done! 
	return 1;
	}

void CJpeg::flipBitmap(BYTE *p,const BITMAPINFOHEADER *bmi) {
	int y2=bmi->biHeight/2;
	DWORD w=(bmi->biWidth*bmi->biBitCount*bmi->biPlanes)/8;
	w=(w+3) & 0xfffffffc;
	BYTE *myBuf=new BYTE[w],*p1,*p2;
	register int y;

	for(y=0,p1=p,p2=p+w*(bmi->biHeight-1); y<y2; y++,p1+=w,p2-=w) {
		// se la larghezza e' dispari, la bitmap risultante potrebbe essere distorta... (fare PAD prima! o dopo? ossia solo se serve com BMP x windows??)
		memcpy(myBuf,p1,w);
		memcpy(p1,p2,w);
		memcpy(p2,myBuf,w);
		}
	delete myBuf;
	}

BYTE *CJpeg::padBitmapDWord(BYTE *p,const BITMAPINFOHEADER *bmi) {
	DWORD w=(bmi->biWidth*bmi->biBitCount*bmi->biPlanes)/8;

	if(!(w & 3))
		return p;

	BYTE *d;
	DWORD w2=(w+3) & 0xfffffffc;
	BYTE *p1,*p2;
	register int y;

	d=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,w2*bmi->biHeight);

	p1=p;
	p2=d;
	for(y=0; y<bmi->biHeight; y++) {		// 
		memcpy(p2,p1,w);
		memset(p2+w,0,w2-w);
		p1+=w;
		p2+=w2;
		}

	HeapFree(GetProcessHeap(),0,p);
	return d;
	}

void CJpeg::removeBitmapPad(BYTE *p,const BITMAPINFOHEADER *bmi) {
	DWORD w=(bmi->biWidth*bmi->biBitCount*bmi->biPlanes)/8;

	if(!(w & 3))
		return;

	BYTE *d;
	DWORD w2=(w+3) & 0xfffffffc;
	BYTE *p1,*p2;
	register int y;

				/*char buffer[256];
				    wsprintf(buffer,"w=%u w2=%u",w,w2);    
		        AfxMessageBox(buffer);*/

	p1=p;
	p2=p;
	for(y=0; y<bmi->biHeight; y++) {		// 
		memcpy(p1,p2,w);
		p1+=w;
		p2+=w2;
		}

	}

void CJpeg::removeAlphaChannel(BYTE *p,const BITMAPINFOHEADER *bmi) {
	int x,y,n;
	BYTE *p1,*p2;
	DWORD w=(bmi->biWidth*bmi->biBitCount*bmi->biPlanes)/8;
	w=(w+3) & 0xfffffffc;

	p1=p2=p;
	for(y=0; y<bmi->biHeight; y++) {
		n=0;
		for(x=0; x<bmi->biWidth; x++) {
			*(DWORD *)p1=*(DWORD *)p2;
			n+=3; p1+=3; p2+=4;
			}

// fa ANCHE il re-pad :)
		if(n & 3)
			p1+= 4 - (n & 3);				// dovrebbe essere DWORD-aligned..

		}
	}

int CJpeg::writeJPEGFile(const char *infile, const char *outfile, int quality, 
												 BOOL reverseV, const char *note, const char *autore) {
	int retVal=0;
	BYTE *p;
	CFile mf;
	BITMAPFILEHEADER bmf;
	BITMAPINFOHEADER bmi;

	if(mf.Open(infile,CFile::modeRead | CFile::typeBinary)) {
		mf.Read(&bmf,sizeof(BITMAPFILEHEADER));
		mf.Read(&bmi,sizeof(BITMAPINFOHEADER));
		if(bmi.biBitCount != 24)
			goto fine;
		p=new BYTE[mf.GetLength() /*bmi.biSizeImage opp bmf.bfSize */];
		mf.Read(p,bmi.biSizeImage);

		removeBitmapPad(p,&bmi);		// bisogna togliere i pad per jpeg...

		if(reverseV) {
			flipBitmap(p,&bmi);
			}

		retVal=writeJPEGFile(p,bmi.biWidth,bmi.biHeight,outfile,quality,note);
		delete p;
fine:
		mf.Close();
		}
	return retVal;
	}

BYTE *CJpeg::buildJPEG(const CBitmap *bmp, DWORD *len, BOOL reverseV, int quality, const char *note, int ratio169, const char *autore) {
	register int i;
  BYTE *d=NULL,*s;
	DWORD l;
	BITMAPINFO *bi;
	HDC dc;
	BITMAP b;
	JSAMPROW rowPointer[1];
  int rowStride;
	
	i=setjmp(exitEnv);
	if(i==EXIT_FAILURE) {
		destroyCompress();
		if(len)
			*len=0;
		HeapFree(GetProcessHeap(),0,d);
		HeapFree(GetProcessHeap(),0,b.bmBits);
		HeapFree(GetProcessHeap(),0,bi);
		return 0;
		}

  if(createCompress(note,autore)) {

		bi=(BITMAPINFO *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD));
		if(bi) {
			((CBitmap *)bmp)->GetBitmap(&b);
			if(d=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,b.bmWidth*b.bmHeight*3+5000)) {	// occupazione MAX del jpg (stimata!)

			l=b.bmWidth*b.bmHeight*3;
			b.bmBits=(char *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,l);
			if(b.bmBitsPixel == 24) {
				bmp->GetBitmapBits(b.bmWidth*b.bmHeight*3,b.bmBits);
				}
			else {
				dc=GetDC(GetDesktopWindow());
				ZeroMemory(bi,sizeof(BITMAPINFOHEADER));
				bi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
				i=GetDIBits(dc,(HBITMAP)*bmp,0,0,NULL,bi,DIB_RGB_COLORS);
				bi->bmiHeader.biBitCount=24;
				bi->bmiHeader.biCompression=0;
				bi->bmiHeader.biHeight=-bi->bmiHeader.biHeight;
			//	bi->bmiHeader.biPlanes=1;
				i=GetDIBits(dc,(HBITMAP)*bmp,0,abs(bi->bmiHeader.biHeight),b.bmBits,bi,DIB_RGB_COLORS);
				ReleaseDC(GetDesktopWindow(),dc);
				}

			s=(BYTE *)b.bmBits;

			setDest(d,b.bmWidth*b.bmHeight*3+5000);

			co->imageWidth=b.bmWidth;
			co->imageHeight=b.bmHeight;
			co->inputComponents=3;
			co->inColorSpace=JCS_RGB;
			co->imageRatio=ratio169;			// rapporto 16:9, 2016!

			co->setDefaults();

			co->setQuality(quality,TRUE);

			co->startCompress(TRUE);

			rowStride = co->imageWidth * 3;

			if(reverseV) {
				while(co->nextScanline < co->imageHeight) {
					rowPointer[0]= &s[(co->imageHeight-co->nextScanline-1)*rowStride];
					co->writeScanlines(rowPointer, 1);
					}
				}
			else {
				while(co->nextScanline < co->imageHeight) {
					rowPointer[0]= &s[co->nextScanline*rowStride];
					co->writeScanlines(rowPointer, 1);
					}
				}

			co->finishCompress();
			if(len)
				*len=co->dest->dataCount;
			destroyCompress();
			HeapFree(GetProcessHeap(),0,b.bmBits);
			}
		HeapFree(GetProcessHeap(),0,bi);
		}
		}

	return d;
	}

BYTE *CJpeg::buildJPEG(int bmpResource, DWORD *len, int quality, const char *note, const char *autore) {
	CBitmap b;

	b.LoadBitmap(bmpResource);
	return buildJPEG(&b,len,FALSE,quality,note,0,autore);
	}


BYTE *CJpeg::buildJPEG(BITMAPINFO *bi, BYTE *pBuffer, DWORD *len, BOOL reverseV, int quality, const char *note, int ratio169, const char *autore) {
	CBitmap b;

	if(!bi->bmiHeader.biSizeImage)			// per sicurezza...
		bi->bmiHeader.biSizeImage = (bi->bmiHeader.biWidth * bi->bmiHeader.biHeight * bi->bmiHeader.biBitCount * bi->bmiHeader.biPlanes) /8;
	b.CreateBitmap(bi->bmiHeader.biWidth,bi->bmiHeader.biHeight,bi->bmiHeader.biPlanes,bi->bmiHeader.biBitCount,NULL);
	b.SetBitmapBits(bi->bmiHeader.biSizeImage,pBuffer);
	return buildJPEG(&b,len,reverseV,quality,note,ratio169,autore);
	}


/*
 * SOME FINE POINTS:
 *
 * In the above loop, we ignored the return value of jpeg_write_scanlines,
 * which is the number of scanlines actually written.  We could get away
 * with this because we were only relying on the value of cinfo.next_scanline,
 * which will be incremented correctly.  If you maintain additional loop
 * variables then you should be careful to increment them properly.
 * Actually, for output to a stdio stream you needn't worry, because
 * then jpeg_write_scanlines will write all the lines passed (or else exit
 * with a fatal error).  Partial writes can only occur if you use a data
 * destination module that can demand suspension of the compressor.
 * (If you don't know what that's for, you don't need it.)
 *
 * If the compressor requires full-image buffers (for entropy-coding
 * optimization or a multi-scan JPEG file), it will create temporary
 * files for anything that doesn't fit within the maximum-memory setting.
 * (Note that temp files are NOT needed if you use the default parameters.)
 * On some systems you may need to set up a signal handler to ensure that
 * temporary files are deleted if the program is interrupted.  See libjpeg.doc.
 *
 * Scanlines MUST be supplied in top-to-bottom order if you want your JPEG
 * files to be compatible with everyone else's.  If you cannot readily read
 * your data in that order, you'll need an intermediate array to hold the
 * image.  See rdtarga.c or rdbmp.c for examples of handling bottom-to-top
 * source data using the JPEG code's internal virtual-array mechanisms.
 */





BYTE *CJpeg::readJPEGFile(const char *filename,BITMAPINFOHEADER *bmp, BYTE *theBytes,
													int colorBits,int ditherMode,BOOL reverseV) {
	CFile mF;
	BYTE *p;

	if(mF.Open(filename,CFile::modeRead | CFile::typeBinary)) {
		p=readJPEGFile(&mF,bmp,theBytes,colorBits,ditherMode,reverseV);
		mF.Close();
		return p;
		}
	else 
		return 0;

	}

BYTE *CJpeg::readJPEGFile(FILE *f, BITMAPINFOHEADER *bmp, BYTE *theBytes,
													int colorBits,int ditherMode,BOOL reverseV) {
	CFile mF(_fileno(f));			// NON VA!!!!

	int i=getc(f);
	BYTE myBuf[4];
	mF.Read(myBuf,2);

	return readJPEGFile(&mF,bmp,theBytes,colorBits,ditherMode,reverseV);
	}


BYTE *CJpeg::readJPEGFile(CFile *f, BITMAPINFOHEADER *bmp, BYTE *theBytes,
													int colorBits,int ditherMode,BOOL reverseV) {

  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  /* More stuff */
//  JSAMPARRAY buffer;		/* Output row buffer */
	JSAMPROW rowPointer[1];
  int rowStride,rowStrideTrue;		/* physical row width in output buffer, e corretta DWORD */
	BYTE *myBuf,*p;
	int i;


	i=setjmp(exitEnv);
	if(i==EXIT_FAILURE) {
		destroyDecompress();
		HeapFree(GetProcessHeap(),0,theBytes);
		return 0;
		}


  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  //cinfo.err = jpeg_std_error(&jerr.pub);
  //jerr.pub.error_exit = my_error_exit;

  /* Now we can initialize the JPEG decompression object. */
  createDecompress();

  /* Step 2: specify data source (eg, a file) */

  setSource(f);
//  stdioSrc(infile);

  /* Step 3: read file parameters with jpeg_read_header() */

  de->readHeader(TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.doc for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

	de->desiredNumberOfColors = 1 << colorBits;
	if(ditherMode)
		de->ditherMode=(J_DITHER_MODE)ditherMode;

  /* Step 5: Start decompressor */

  de->startDecompress();
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */


	if(de->numComponents == 1) {

		return 0;
		}


  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  /* JSAMPLEs per row in output buffer */
  rowStride = de->outputWidth * de->outputComponents;
	rowStrideTrue=(rowStride + 3) & 0xfffffffc;			// bitmap dovra' essere DWORD-aligned
	bmp->biSize=sizeof(BITMAPINFOHEADER);
	bmp->biBitCount=de->outputComponents*8;
	bmp->biPlanes=1			/* de-> */ ;
	bmp->biWidth=de->outputWidth;
	bmp->biHeight=de->outputHeight;
	bmp->biSizeImage=rowStrideTrue*de->outputHeight;
	bmp->biClrUsed=bmp->biClrImportant=de->desiredNumberOfColors;
	bmp->biCompression=0;
	bmp->biXPelsPerMeter=bmp->biYPelsPerMeter=0;

  if(theBytes) {
		HeapFree(GetProcessHeap(),0,theBytes);
		theBytes=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,bmp->biSizeImage);
		}
	else
		theBytes=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,bmp->biSizeImage);

	if(reverseV)
		p=theBytes+bmp->biSizeImage-rowStrideTrue;
	else
		p=theBytes;

  /* Make a one-row-high sample array that will go away when done with image */
  myBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,rowStride);
	rowPointer[0]=myBuf;

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  while(de->outputScanline < de->outputHeight) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    de->readScanlines(rowPointer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
//    put_scanline_someplace(buffer[0], row_stride);

		memcpy(p,myBuf,rowStride);
		if(reverseV)
			p-=rowStrideTrue;
		else
			p+=rowStrideTrue;
		}

	HeapFree(GetProcessHeap(),0,myBuf);

  /* Step 7: Finish decompression */

  de->finishDecompress();
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  destroyDecompress();

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */
//  fclose(infile);

  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */

  /* And we're done! */
  return theBytes;
	}


/*
 * SOME FINE POINTS:
 *
 * In the above code, we ignored the return value of jpeg_read_scanlines,
 * which is the number of scanlines actually read.  We could get away with
 * this because we asked for only one line at a time and we weren't using
 * a suspending data source.  See libjpeg.doc for more info.
 *
 * We cheated a bit by calling alloc_sarray() after jpeg_start_decompress();
 * we should have done it beforehand to ensure that the space would be
 * counted against the JPEG max_memory setting.  In some systems the above
 * code would risk an out-of-memory error.  However, in general we don't
 * know the output image dimensions before jpeg_start_decompress(), unless we
 * call jpeg_calc_output_dimensions().  See libjpeg.doc for more about this.
 *
 * Scanlines are returned in the same order as they appear in the JPEG file,
 * which is standardly top-to-bottom.  If you must emit data bottom-to-top,
 * you can use one of the virtual arrays provided by the JPEG memory manager
 * to invert the data.  See wrbmp.c for an example.
 *
 * As with compression, some operating modes may require temporary files.
 * On some systems you may need to set up a signal handler to ensure that
 * temporary files are deleted if the program is interrupted.  See libjpeg.doc.
 */
