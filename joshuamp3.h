#ifndef _JOSHUA_MP3_DEFINED
#define _JOSHUA_MP3_DEFINED



#undef NO_PENTIUM_OPT
#undef PENTIUM_OPT
#undef I386_ASSEM
#undef USE_3DNOW
#undef I486_OPT  //PROVARE 2021.. .mancano le synth486...
#undef GAPLESS	
#define NOXFERMEM


class CJoshuaMP3;

/* High Performance MPEG 1.0/2.0 Audio Player for Layer 1, 2 and 3.
   Written and copyrights by Michael Hipp.
	 Windows version by Dario Greggio 1999-2023 (dario.greggio@outlook.it)
   Uses code from various people. 
	 */

#pragma pack(1)
struct ID3_TAG {			// v1.0
	char tag[3];
	char titolo[30];
	char autore[30];
	char album[30];
	char anno[4];
	union {
		struct {
			char note11[28];
			BYTE reserved;
			BYTE track;	// v.1.1... ENTRO note! (fare una union?) e note[28] dev'essere =0 (marker)
			};
		char note[30];
		};
	BYTE genere;	// enum di byte... da 0x20 a ... 80?
	};
#pragma pack()

struct ID3_TAG_CONTAINER {
	char titolo[31];
	char autore[31];
	char album[31];
	char anno[5];
	char note[31];
	BYTE track;
	BYTE genere;
	};


/* max = 1728 */
#define MAX_FRAME_SIZE 1792
#define SKIP_JUNK 1
#define AUSHIFT (3)
#define NEW_DCT9


#define M_PI     3.14159265358979323846
#define M_SQRT2	 1.41421356237309504880

#define MAXOUTBURST 32768


#define  SBLIMIT                 32
#define  SCALE_BLOCK             12
#define  SSLIMIT                 18

// #define REAL_IS_FLOAT
// #define REAL_IS_FIXED
//#define VARMODESUPPORT

#ifndef off_t
	#define off_t long
#endif

#ifdef REAL_IS_FLOAT
#  define real float
#  define REAL_SCANF "%f"
#  define REAL_PRINTF "%f"
#elif defined(REAL_IS_LONG_DOUBLE)
#  define real long double
#  define REAL_SCANF "%Lf"
#  define REAL_PRINTF "%Lf"
#elif defined(REAL_IS_FIXED)
# define real long

# define REAL_RADIX            15
# define REAL_FACTOR           (32.0 * 1024.0)

# define REAL_PLUS_32767       ( 32767 << REAL_RADIX )
# define REAL_MINUS_32768      ( -32768 << REAL_RADIX )

# define DOUBLE_TO_REAL(x)     ((int)((x) * REAL_FACTOR))
# define REAL_TO_SHORT(x)      ((x) >> REAL_RADIX)
# define REAL_MUL(x, y)        (((_int64 /*long long*/)(x) * (_int64 /*long long*/)(y)) >> REAL_RADIX)
#  define REAL_SCANF "%ld"
#  define REAL_PRINTF "%ld"

#else
#  define real double
#  define REAL_SCANF "%lf"
#  define REAL_PRINTF "%f"
#endif

#ifndef DOUBLE_TO_REAL
# define DOUBLE_TO_REAL(x)     (x)
#endif
#ifndef REAL_TO_SHORT
# define REAL_TO_SHORT(x)      (x)
#endif
#ifndef REAL_PLUS_32767
# define REAL_PLUS_32767       32767.0
#endif
#ifndef REAL_MINUS_32768
# define REAL_MINUS_32768      -32768.0
#endif
#ifndef REAL_MUL
# define REAL_MUL(x, y)                ((x) * (y))
#endif

struct AL_TABLE {
  BYTE bits;
  short int d;
	};

struct NEW_HUFF {
  DWORD linbits;
  short int *table;
	};


#define HDRCMPMASK 0xfffffd00
#if 0
#define HDRCMPMASK 0xfffffdft
#endif
// #define HDRCMPMASK 0xfffffddf	 // vecchio!


#define INDEX_SIZE 1000
#define free_format_header(head) ( ((head & 0xffe00000) == 0xffe00000) && ((head>>17)&3) && (((head>>12)&0xf) == 0x0) && (((head>>10)&0x3) != 0x3 ))

class MP3_READER {
public:
	enum {
		READER_FD_OPENED=0x1,
		READER_ID3TAG=0x2,
		READER_SEEKABLE=0x4,
		READER_FD_OPENED_SOCKET=0x8,
		};
	struct {
		off_t data[INDEX_SIZE];
		size_t fill;
		unsigned long step;
		} frame_index;

public:
  int  init();
  int  open(const char *,int fd=0);
  int  http_open(const char *,char *mime,int port=80);
  void close();
  int  headRead(DWORD *);
  int  headShift(DWORD *);
  off_t skipBytes(off_t);
  int  readFrameBody(BYTE *,unsigned int);
  int  backBytes(int );
  int  backFrame(CJoshuaMP3 *,int);
  off_t tell();
  void rewind();
	int getFileInfo(char *);
	int parse_new_id3(unsigned long);
	MP3_READER(CJoshuaMP3 *);
private:
	off_t stream_lseek(off_t, int);
	int /*size_t*/ fullread(unsigned char *, size_t );
	off_t frame_index_find(unsigned long , unsigned long* );

	void writestring(HANDLE, const char *);
	void readstring(char *, int , HANDLE);
	char *url2hostport(const char *, char **, unsigned long *, unsigned int *);
	int getauthfromURL(char *,char *);
	void encode64(const char *,char *);
	char *trim(char *);

public:
  long filelen;
  long filepos;
  int  filept;
  int  flags;
	struct ID3_TAG id3buf;
	char filename[256];
	const char *httpauth;
	char httpauth1[256];
	char *proxyurl;
	unsigned long proxyip;
	unsigned int proxyport;
	CJoshuaMP3 *m_parent;

private:
	DWORD lastread;

	};


class Player;
class PlayerNotifyWnd;
class Mutex;

class CMP3Audio {
public:
	enum {
/* AUDIO_BUF_SIZE = n*64 with n=1,2,3 ...  */
		AUDIO_BUF_SIZE=16384 /*49152*/ /*88200*/,		// almeno 48000 ??? no,,, e buffer piccolo=più rapida risposta a volume e eq.
		TOT_BUFFERS=4
		};

	enum AUDIO_FORMATS {
/// cambiare con quelli WINDOWS!
		AUDIO_FORMAT_UNSIGNED_16=0x0,
		AUDIO_FORMAT_SIGNED_16=0x1,
		AUDIO_FORMAT_UNSIGNED_8=0x2,
		AUDIO_FORMAT_SIGNED_8=0x4,
		AUDIO_FORMAT_ULAW_8=0x8,
		AUDIO_FORMAT_ALAW_8=0x10
		};

public:
#ifdef AUDIO_USES_FD
  int fn; /* filenumber */
#endif
	BYTE perif;
	enum AUDIO_FORMATS format;
	int rate;
	BYTE channels;
  int gain;
	BYTE verbose;
	HWAVEOUT hWaveOut;			// usati in modalità DECODE_AUDIO
	WAVEHDR OWaveHdr[TOT_BUFFERS];
	WAVEFORMATEX wf;
	BYTE *buf[TOT_BUFFERS];
	BOOL critical;
	BOOL enabled;
	DWORD audiobufsize;
	CJoshuaMP3 *m_parent;

protected:
	Player *m_pSoundPlayer;
	CRITICAL_SECTION m_cs;
	HANDLE event;
	PlayerNotifyWnd *nWnd;
	short int reserved;
	int m_soundPlayerEventNbSamples;
	short int *m_pSamples;
	DWORD m_soundPlayerEventSize;
	int m_nbSoundPlayerEvents;
	Mutex *m_exprMutex;

private:
	int which;
//	short int playBuffer[AUDIO_BUF_SIZE/2];
//	int playBufferBytes;

public:
	int open(BYTE q=0);
	void setRate(int r);
	void setChannels(BYTE c);
	int play(const BYTE *,int);
//	int play();		// era per pitch mio..
	void flush();
	int close(BYTE m=0);
	CMP3Audio(CJoshuaMP3 *m_parent,HWAVEOUT hw,enum AUDIO_FORMATS f=AUDIO_FORMAT_UNSIGNED_16,BYTE ch=2,DWORD r=44100,DWORD bsize=AUDIO_BUF_SIZE,BYTE v=0);
		// usa wave
	CMP3Audio(CJoshuaMP3 *m_parent,BYTE perif,enum AUDIO_FORMATS f=AUDIO_FORMAT_UNSIGNED_16,BYTE ch=2,DWORD r=44100,DWORD bsize=AUDIO_BUF_SIZE,BYTE v=0);
		// usa DirectX
	void init();
	~CMP3Audio();
	Player *getPlayer() const { return m_pSoundPlayer; }
	BOOL IsReady() { return m_pSoundPlayer || hWaveOut!=(HANDLE)-1; }


	friend class PlayerNotifyWnd;
	friend class CMP3Status;
	friend class CJoshuaMP3;
	};


struct MP3_FRAME_HEADER {			//https://wiki.hydrogenaud.io/images/a/a3/MP3_file_structure.png
	unsigned int emphasis:2;
	unsigned int original:1;
	unsigned int copy:1;
	unsigned int modeExt:2;
	unsigned int mode:2;
	unsigned int privBit:1;
	unsigned int padBit:1;
	unsigned int frequency:2;
	unsigned int bitRate:4;
	unsigned int errorProtection:1;
	unsigned int layer:2;
	unsigned int version:1;
	unsigned int syncWord:12;
	};

struct GR_INFO_S {
	int scfsi;
  WORD part2_3_length;
  WORD big_values;
  WORD scalefacCompress;
  BYTE blockType;
  BYTE mixedBlockFlag;
  BYTE tableSelect[3];
  DWORD subblock_gain[3];
  DWORD maxband[3];
  DWORD maxbandl;
  unsigned int maxb;
  DWORD region1start;
  DWORD region2start;
  BYTE preflag;
  BYTE scalefacScale;
  BYTE count1tableSelect;
  real *fullGain[3];
  real *pow2gain;
	};

struct III_SIDE_INFO {
  unsigned int mainDataBegin;
  unsigned int privateBits;
  struct {
    struct GR_INFO_S gr[2];
		} ch[2];
	};

struct I_BUF {
	struct I_BUF *next;
	struct I_BUF *prev;
	BYTE *buf;
	BYTE *pnt;
	int len;
	// skip,time stamp
	};

struct BAND_INFO_STRUCT {
  short int longIdx[23];
  short int longDiff[22];
  short int shortIdx[14];
  short int shortDiff[13];
	};

struct BITSTREAM_INFO {
	int  bitindex,tellcnt;
	BYTE *wordpointer;
	};

struct PARAMETERS {
	BYTE equalizer;
	BYTE aggressive; // renice to max. priority
	BYTE shuffle;		// shuffle/random play
	BYTE remote;			// remote operation
  BYTE remote_err;	/* remote operation to stderr */
	BYTE outmode;		// where to out the decoded sampels
	BYTE quiet;	// shut up!  // 0=abilita messaggi status
  long usebuffer;	/* second level buffer size */
	BYTE tryResync;  // resync stream after error
	BYTE verbose;    // verbose level // 1=abilita messaggi d'errore
#ifdef HAVE_TERMIOS
  BYTE term_ctrl;
#endif
	BYTE forceMono;
	BYTE forceStereo;
	BYTE force8bit;
	BYTE forceRate;
	BYTE downSample;
	BYTE checkRange;
	BYTE doubleSpeed;
	BYTE halfSpeed;
	double pitch; /* <0 or >0, 0.05 for 5% speedup. */
	BYTE forceReopen;
#ifdef USE_3DNOW
  int stat_3dnow; /* automatic/force/force-off 3DNow! optimized code */
  int test_3dnow;
#endif
  long realtime;
	double forceVolume;
  char filename[256];
#ifdef GAPLESS	
  int gapless; /* (try to) remove silence padding/delay to enable gapless playback */
#endif
  long listentry; /* possibility to choose playback of one entry in playlist (0: off, > 0 : select, < 0; just show list*/
  BYTE rva; /* (which) rva to do: 0: nothing, 1: radio/mix/track 2: album/audiophile */
  char* listname; /* name of playlist */
  int long_id3;
	};

typedef struct {
	size_t freeindex;	/* [W] next free index */
	size_t readindex;	/* [R] next index to read */
	int fd[2];
	int wakeme[2];
	byte *data;
	byte *metadata;
	size_t size;
	size_t metasize;
	long rate;
	int  channels;
	int  format;
	int justwait;
} txfermem;
/*
 *   [W] -- May be written to by the writing process only!
 *   [R] -- May be written to by the reading process only!
 *   All other entries are initialized once.
 */
void xfermem_init (txfermem **xf, size_t bufsize, size_t msize, size_t skipbuf);
void xfermem_init_writer (txfermem *xf);
void xfermem_init_reader (txfermem *xf);
size_t xfermem_get_freespace (txfermem *xf);
size_t xfermem_get_usedspace (txfermem *xf);
#define XF_CMD_WAKEUP_INFO  0x04
#define XF_CMD_WAKEUP    0x02
#define XF_CMD_TERMINATE 0x03
#define XF_CMD_AUDIOCAP  0x05
#define XF_CMD_RESYNC    0x06
#define XF_CMD_ABORT     0x07
#define XF_WRITER 0
#define XF_READER 1
int xfermem_getcmd (int fd, int block);
int xfermem_putcmd (int fd, byte cmd);
int xfermem_block (int fd, txfermem *xf);
int xfermem_sigblock (int fd, txfermem *xf, int pid, int signal);
/* returns TRUE for being interrupted */
int xfermem_write(txfermem *xf, byte *buffer, size_t bytes);
void xfermem_done (txfermem *xf);
#define xfermem_done_writer xfermem_init_reader
#define xfermem_done_reader xfermem_init_writer

/////////////////////////////////////////////////////////////////////////////
// CMP3Status dialog

class CMP3Status : public CDialog {
// Construction
public:
	CMP3Status(CJoshuaMP3 *,CWnd* pParent=NULL);   // standard constructor
	~CMP3Status();
	CJoshuaMP3 *m_MP3;

// Dialog Data
	//{{AFX_DATA(CMP3Status)
	enum { IDD = IDD_MP3 };
	CButton	m_Stop;
	CSliderCtrl	m_Volume;
	CButton	m_Pause;
	CSliderCtrl	m_Slider;
	CString	m_To;
	CString	m_Title;
	BOOL	m_Mono;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMP3Status)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HMIXER hMixer;
	DWORD m_volumeControlID;

	static CString translateString(const char *); 

	// Generated message map functions
	//{{AFX_MSG(CMP3Status)
	afx_msg void OnButton0();
	afx_msg void OnButton1();
	afx_msg void OnButton2();
	virtual BOOL OnInitDialog();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnCheck1();
	afx_msg void OnButton3();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	};


class CJoshuaMP3Equalizer {
public:
	enum EQUALIZER {
		passaFlat=0,
		passaAlto=1,
		passaBasso=2,
		passaBanda=3,
		equal_flat=10,
		equal_rock=11,
		equal_pop=12,
		equal_dance=13,
		equal_classica=14,
		};

public:
	real cursor[2][32];
	real band[2][SBLIMIT*SSLIMIT];

public:
	void equalize(real *, BYTE);
	void init(int m=1, double v=1.0);
	void doBand(real *,BYTE);
	CJoshuaMP3Equalizer();
private:
	int oldEqualizeMode;
	};


class CJoshuaMP3 {
	friend class MP3_READER;
	friend class CMP3Audio;
public:
	enum MPG_MODE { 
		MPG_MD_STEREO,
		MPG_MD_JOINT_STEREO,
		MPG_MD_DUAL_CHANNEL,
		MPG_MD_MONO
		};
	enum { 
		AUDIO_OUT_HEADPHONES,
		AUDIO_OUT_INTERNAL_SPEAKER,
		AUDIO_OUT_LINE_OUT 
		};
	enum { 
		DECODE_TEST, 
		DECODE_AUDIO, 
		DECODE_STDOUT, 
		DECODE_BUFFER,
		DECODE_BOTH,
	  DECODE_WAV,
    DECODE_CDR,
    DECODE_AU
		};
	enum VBR_MODE {
		CBR=0,
		VBR=1,
		ABR=2
		};


struct FRAME {
  const struct AL_TABLE *alloc;
  int (CJoshuaMP3::*synth)(const real *,BYTE,BYTE *,int *);
  int (CJoshuaMP3::*synthMono)(const real *,BYTE *,int *);
#ifdef USE_3DNOW
    void (*dct36)(real *,real *,real *,real *,real *);
#endif
  signed char stereo;
  BYTE jsbound;		// TESTARE dopo aver messo tutti BYTE, alcuni servono short!
  short int single;			// short int!
  short int II_sblimit;
  BYTE downSampleSBlimit;
//  short int version;
  BYTE lsf;
  BYTE mpeg25;
  BYTE downSample;
  BYTE headerChange;
//  short int blockSize;
  BYTE lay;
	int (CJoshuaMP3::*doLayer)();
//	int (CJoshuaMP3::*doLayer)(struct frame *fr,int,struct audio_info_struct *);
  BYTE errorProtection;
  BYTE bitrateIndex;
  BYTE samplingFrequency;
  BYTE padding;
  BYTE extension;
  enum MPG_MODE mode;
  BYTE modeExt;
  BYTE copyright;
  BYTE original;
  BYTE emphasis;
  int framesize; // computed framesize
  BYTE vbr; /* 1 if variable bitrate was detected */
	unsigned long num; /* the nth frame in some stream... */
	};


public:
//	char title[256];
	DWORD totSec;
	DWORD tag;
	struct ID3_TAG_CONTAINER ID3;
	static const char *generiMP3[];
	static int tabsel_123[2][3][16];
	static DWORD freqs[9];
	CJoshuaMP3Equalizer *theEqualizer;

	static struct AL_TABLE alloc_0[],alloc_1[],alloc_2[],alloc_3[],alloc_4[];

  real block[2][2][SBLIMIT*SSLIMIT];
  int blc[2];
  real buffs[2][2][0x110];


protected:
	struct PARAMETERS Param;
private:
	MP3_READER *reader;
	BOOL bBusy;
	int status;
	DWORD from,to;
//	HWND hDlg;
	CMP3Status *m_Dlg;
	DWORD currFrame,totFrames;
	struct BITSTREAM_INFO bsi;
	struct FRAME fr;
	CMP3Audio *ai,*ai2;
	BYTE *bufPtr;						// usati per DECODE_BUFFER
	DWORD bufSize;		// anche bufUsed...

	static struct BAND_INFO_STRUCT bandInfo[];
	static struct NEW_HUFF htc[];
	static struct NEW_HUFF ht[];
	static short int tab0[],tab1[],tab2[],tab3[],tab5[],tab6[],tab7[],tab8[],tab9[],tab10[],
		tab11[],tab12[],tab13[],tab15[],tab16[],tab24[];
	static short int tab_c0[],tab_c1[];
	static BYTE pretab1[22],pretab2[22];


//	char fname[256];
	DWORD oldhead;
	DWORD firsthead;
	int grp_3tab[32 * 3];   // used: 27 
	int grp_5tab[128 * 3];  // used: 125
	int grp_9tab[1024 * 3]; // used: 729 

  int step;
  int bo;

	real muls[27][64];	// also used by layer 1 
#ifdef USE_3DNOW
//real CJoshuaMP3::decwin[2*(512+32)];
#else
	/*static no!*/ real decwin[512+32];
#endif
	static real cos64[16],cos32[8],cos16[4],cos8[2],cos4[1];
	static real *pnts[];
	static const long intwinbase[];

	BYTE *conv16to8_buf,*conv16to8;

	//struct I_BUF ibufs[2];
	//struct I_BUF *cibuf;
	//int ibufnum=0;

	BYTE *pcmSample;
	int pcmPoint;
	int fsizeold,ssize;
	BYTE bsspace[2][MAX_FRAME_SIZE+512];   // MAXFRAMESIZE 
	BYTE *bsbuf,*bsbufold;
	unsigned int bitreservoir;
	int bsnum;
	long outscale;
	int minSample,maxSample;
	//int changeAlways = 1;
	signed char forceFrequency;
	long numFrames;
	int frameNum;
	long startFrame;

	real ispow[8207];
	real aa_ca[8],aa_cs[8];
	real COS1[12][6];
	real win[4][36];
	real win1[4][36];
	real gainpow2[256+118+4];
	real COS9[9];
	real COS6_1,COS6_2;
	real tfcos36[9];
	real tfcos12[3];
#ifdef NEW_DCT9
	real cos9[3],cos18[3];
#endif

	int longLimit[9][23];
	int shortLimit[9][14];

  BYTE ssave[34];
  int halfPhase;

	int mapbuf0[9][152];
	int mapbuf1[9][156];
	int mapbuf2[9][44];
	int *map[9][3];
	int *mapend[9][3];

	unsigned int n_slen2[512]; // MPEG 2.0 slen for 'normal' mode
	unsigned int i_slen2[256]; // MPEG 2.0 slen for intensity stereo

	static real tan1_1[16],tan2_1[16],tan1_2[16],tan2_2[16];
	static real pow1_1[2][16],pow2_1[2][16],pow1_2[2][16],pow2_2[2][16];
	
#ifdef GAPLESS
/* still a dirty hack, places in bytes (zero-based)... */
	unsigned long position; /* position in raw decoder bytestream */
	unsigned long begin; /* first byte to play == number to skip */
	unsigned long end; /* last byte to play */
	unsigned long ignore; /* forcedly ignore stuff in between */
	int bytified;
#endif

#define NTOM_MUL (32768)
	DWORD ntom_val[2];
	DWORD ntom_step;

	int init;
  BOOL initOutputDone;

#ifdef VARMODESUPPORT
	/*
	 *   This is a dirty hack!  It might burn your PC and kill your cat!
	 *   When in "varmode", specially formatted layer-3 mpeg files are
	 *   expected as input -- it will NOT work with standard mpeg files.
	 *   The reason for this:
	 *   Varmode mpeg files enable my own GUI player to perform fast
	 *   forward and backward functions, and to jump to an arbitrary
	 *   timestamp position within the file.  This would be possible
	 *   with standard mpeg files, too, but it would be a lot harder to
	 *   implement.
	 *   A filter for converting standard mpeg to varmode mpeg is
	 *   available on request, but it's really not useful on its own.
	 *
	 *   Oliver Fromme  <oliver.fromme@heim3.tu-clausthal.de>
	 *   Mon Mar 24 00:04:24 MET 1997
	 */
	int varmode;
	int playlimit;
#endif


	enum VBR_MODE vbr; // variable bitrate flag
	int abr_rate;

	/* this could become a struct... */
	long lastscale; /* last used scale */
	int rva_level[2]; /* significance level of stored rva */
	float rva_gain[2]; /* mix, album */
	float rva_peak[2];

	double mean_framesize;
	unsigned long mean_frames;
	BOOL do_recover;
	unsigned long track_frames;
#define TRACK_MAX_FRAMES ULONG_MAX/4/1152
	txfermem *buffermem;

	BOOL bPaused;
	BOOL intflag;

public:
	int volume(double vl=1.0,double vr=-1.0);
	int HWvolume(double vl=1.0,double vr=-1.0);			// con DirectSound
	int volume(signed char q,double vl=1.0,double vr=-1.0);		// per multi-periferica
	int HWvolume(signed char q,double vl=1.0,double vr=-1.0);		// 
	int enableOutput(signed char q,BOOL bEnabled=TRUE);		// per multi-periferica
	int setPitch(double p=1.0);
	int initSuonaUDP(BYTE *outbuffer);
	int SuonaUDP(const BYTE *,BYTE *outbuffer);
	int endSuonaUDP();
	int Suona(const char *, int nFromFrame=0,int nToFrame=0, int dnSample=0, int doMix=0, HWAVEOUT hDevice=NULL, 
		CJoshuaMP3Equalizer::EQUALIZER equalizer=CJoshuaMP3Equalizer::passaFlat,double volume=1.0,double pitch=1.0,BOOL bUI=0);
	int SuonaDX(const char *, int nFromFrame=0,int nToFrame=0, int dnSample=0, int doMix=0,
		CJoshuaMP3Equalizer::EQUALIZER equalizer=CJoshuaMP3Equalizer::passaFlat,double volume=1.0,double pitch=1.0,BOOL bUI=0);
	int Suona2(const char *fname,BYTE acard,CJoshuaMP3Equalizer::EQUALIZER equalizer,double volume=1.0,double pitch=1.0);
	int Suona3(const char *fname,CJoshuaMP3Equalizer::EQUALIZER equalizer,double volume=1.0,double pitch=1.0);
	int Suona(const char *, BYTE *, int nFromFrame=0,int nToFrame=0, int dnSample=0, int doMix=0,
		double volume=1.0,double pitch=1.0);
	int Suona(CFile *, BYTE *, int nFromFrame=0,int nToFrame=0, int dnSample=0, int doMix=0,
		double volume=1.0,double pitch=1.0);
	int SuonaWav(const char *, DWORD nFromSec=0, DWORD nToSec=0, int dnSample=0, int doMix=0, HWAVEOUT hDevice=NULL, BOOL bUI=0);
	void stop();
	int pause(int mode=1,BOOL bFreeWavDevice=TRUE);
	int goTo(DWORD,BYTE);
	BOOL isPaused();
	BOOL isBusy() { return bBusy; }
	BOOL isOn() { return status != 0; }
	BOOL isInterrupted() { return intflag != 0; }
	char *getFilename() const { return reader ? reader->filename : NULL; }
	CMP3Audio *getAudioIF() const { return ai; }
	struct PARAMETERS getParameters() const { return Param; }
	HWAVEOUT getAudioHandle() const { return ai ? ai->hWaveOut : NULL; }
	static int GetFileInfo(const char *,struct ID3_TAG *);
	static int GetDuration(const char *);
	BYTE *getBuffer() { return bufPtr; }
	DWORD getBufferPos() { return bufSize; }
	BYTE getBufferSafePos(DWORD,DWORD *,DWORD *);
	BYTE getBufferSafePos(DWORD,BYTE **,DWORD *,BYTE **,DWORD *);


  CJoshuaMP3(DWORD);
  CJoshuaMP3(const char *canzone=NULL,BOOL bUI=FALSE);
	void _init(DWORD n=0);
  ~CJoshuaMP3();

private:
	void dct64(real *,real *,const real *);
	void makeDecodeTables(long );
	void makeConv16to8Table(int);
	int InitOutput();
	void InitLayer2();
	void InitLayer3(int);
	void I_stepOne(DWORD balloc[], DWORD scale_index[2][SBLIMIT]);
	void I_stepTwo(real fraction[2][SBLIMIT],DWORD balloc[2*SBLIMIT],DWORD scale_index[2][SBLIMIT]);
	void II_stepOne(DWORD *,int *);
	void II_stepTwo(DWORD *,real fraction[2][4][SBLIMIT],int *,int);
	void II_select_table();
	int III_getSideInfo1(struct III_SIDE_INFO *,BYTE,BYTE,long,short int,BYTE);
	void III_getSideInfo2(struct III_SIDE_INFO *,BYTE,BYTE,long ,short int);
	int III_getScaleFactors1(int *,struct GR_INFO_S *,BYTE,BYTE);
	int III_getScaleFactors2(int *,struct GR_INFO_S *,BYTE);
	int III_dequantizeSample(real xr[SBLIMIT][SSLIMIT],int *,struct GR_INFO_S *,int ,int );
	//int III_dequantizeSampleMs(real xr[2][SBLIMIT][SSLIMIT],int *,struct GR_INFO_S *,int ,int );
	void III_IStereo(real xr_buf[2][SBLIMIT][SSLIMIT],int *,struct GR_INFO_S *,int ,int ,int);
	void III_antialias(real xr[SBLIMIT][SSLIMIT],struct GR_INFO_S *);
	void dct36(real *,real *,real *,const real *,real *);
	void dct12(real *,real *,real *,register const real *,register real *);
	void III_hybrid(real fsIn[SBLIMIT][SSLIMIT],real tsOut[SSLIMIT][SBLIMIT],BYTE,struct GR_INFO_S *);
	int DoLayer1();
	int DoLayer2();
	int DoLayer3();
	int playSamples(const BYTE *,int);
	void Flush();

	int readFrame();
	void readFrameInit();
	void do_rva();
	int decodeHeader(DWORD);
	void printHeader();
	void printHeaderCompact();
	int OpenStream(const char *);
	int OpenStream(CFile *);
	int ReopenStream(const char *);
	int CloseStream();
	void backFrame(int );
	DWORD getBits(int);
	DWORD getBitsFast(int);
	DWORD get1Bit();
	inline int hsstell() { return bsi.tellcnt; }
	long setPointer(long);
	void rewindNbits(int);
	double computeBpf(struct FRAME *);
	double computeTpf(struct FRAME *);
	long computeBufferOffset(struct FRAME *);
	int getSongLen(struct FRAME *,int no);
	long timeToFrame(struct FRAME *, double seconds);

	void backBits(int);
	int getBitOffset();
	int getByte();
	int headCheck(DWORD);

#ifdef PENTIUM_OPT
	int synth_1to1_pent(real *,BYTE,BYTE *);
#endif
#ifdef USE_3DNOW
	int synth_1to1_3dnow(real *,BYTE,BYTE *);
#endif
	int synth_1to1_8bit(const real *,BYTE,BYTE *,int *);
	int synth_1to1_8bit_mono(const real *,BYTE *,int *);
	int synth_1to1_8bit_mono2stereo(const real *,BYTE *,int *);
	int synth_1to1_mono(const real *,BYTE *,int *);
	int synth_1to1_mono2stereo(const real *,BYTE *,int *);
	int synth_1to1(const real *,BYTE,BYTE *,int *);
	int synth_2to1_8bit(const real *,BYTE,BYTE *,int *);
	int synth_2to1_8bit_mono(const real *,BYTE *,int *);
	int synth_2to1_8bit_mono2stereo(const real *,BYTE *,int *);
	int synth_2to1_mono(const real *,BYTE *,int *);
	int synth_2to1_mono2stereo(const real *,BYTE *,int *);
	int synth_2to1(const real *,BYTE,BYTE *,int *);
	int synth_4to1_8bit(const real *,BYTE,BYTE *,int *);
	int synth_4to1_8bit_mono(const real *,BYTE *,int *);
	int synth_4to1_8bit_mono2stereo(const real *,BYTE *,int *);
	int synth_4to1_mono(const real *,BYTE *,int *);
	int synth_4to1_mono2stereo(const real *,BYTE *,int *);
	int synth_4to1(const real *,BYTE,BYTE *,int *);
	void synth_ntom_setStep(long ,long);
	int synth_ntom_8bit(const real *,BYTE,BYTE *,int *);
	int synth_ntom_8bit_mono(const real *,BYTE *,int *);
	int synth_ntom_8bit_mono2stereo(const real *,BYTE *,int *);
	int synth_ntom_mono(const real *,BYTE *,int *);
	int synth_ntom_mono2stereo(const real *,BYTE *,int *);
	int synth_ntom(const real *,BYTE,BYTE *,int *);

	void setSynthFunctions();
	void playFrame(int);

	friend class CMP3Status;
	};

#endif

