/*
 *      Machine dependent defines/includes for LAME.
 *
 *      Copyright (c) 1999 A.L. Faber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include "version.h"

#include <stdio.h>
#include <assert.h>

#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <limits.h>

#include <ctype.h>

#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <stdint.h>


/*
 * 3 different types of pow() functions:
 *   - table lookup
 *   - pow()
 *   - exp()   on some machines this is claimed to be faster than pow()
 */

#define POW20(x) (assert(0 <= (x+Q_MAX2) && x < Q_MAX), pow20[x+Q_MAX2])
/*#define POW20(x)  pow(2.0,((double)(x)-210)*.25) */
/*#define POW20(x)  exp( ((double)(x)-210)*(.25*LOG2) ) */

#define IPOW20(x)  (assert(0 <= x && x < Q_MAX), ipow20[x])
/*#define IPOW20(x)  exp( -((double)(x)-210)*.1875*LOG2 ) */
/*#define IPOW20(x)  pow(2.0,-((double)(x)-210)*.1875) */


#define inline _inline

#if    defined(_MSC_VER)
# pragma warning( disable : 4244 )
/*# pragma warning( disable : 4305 ) */
#endif

/*
 * FLOAT    for variables which require at least 32 bits
 * FLOAT8   for variables which require at least 64 bits
 *
 * On some machines, 64 bit will be faster than 32 bit.  Also, some math
 * routines require 64 bit float, so setting FLOAT=float will result in a
 * lot of conversions.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <float.h>
#define FLOAT_MAX FLT_MAX

typedef double FLOAT8;
#define FLOAT8_MAX DBL_MAX

typedef long double ieee854_float80_t;
typedef double      ieee754_float64_t;
typedef float       ieee754_float32_t;

/* sample_t must be floating point, at least 32 bits */
typedef FLOAT sample_t;

#define dimension_of(array) (sizeof(array)/sizeof(array[0]))
#define beyond(array) (array+dimension_of(array))
#define compiletime_assert(expression) enum{static_assert_##FILE##_##LINE = 1/((expression)?1:0)}
#define lame_calloc(TYPE, COUNT) ((TYPE*)calloc(COUNT, sizeof(TYPE)))
#define multiple_of(CHUNK, COUNT) (\
  ( (COUNT) < 1 || (CHUNK) < 1 || (COUNT) % (CHUNK) == 0 ) \
  ? (COUNT) \
  : ((COUNT) + (CHUNK) - (COUNT) % (CHUNK)) \
  )

#if 1
#define EQ(a,b) (\
(fabs(a) > fabs(b)) \
 ? (fabs((a)-(b)) <= (fabs(a) * 1e-6f)) \
 : (fabs((a)-(b)) <= (fabs(b) * 1e-6f)))
#else
#define EQ(a,b) (fabs((a)-(b))<1E-37)
#endif

#define NEQ(a,b) (!EQ(a,b))


/* end of machine.h */



/*
 *	Interface to MP3 LAME encoding engine
 *
 *	Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id: lame.h,v 1.192 2017/08/31 14:14:46 robert Exp $ */


/* for size_t typedef */
#include <stddef.h>
/* for va_list typedef */
#include <stdarg.h>
/* for FILE typedef, TODO: remove when removing lame_mp3_tags_fid */
#include <stdio.h>

#if defined(__cplusplus)
//extern "C" {
#endif

typedef void (*lame_report_function)(const char *format, va_list ap);

#if defined(WIN32) || defined(_WIN32)
#undef CDECL
#define CDECL __cdecl
#else
#define CDECL
#endif

#define DEPRECATED_OR_OBSOLETE_CODE_REMOVED 1

typedef enum vbr_mode_e {
  vbr_off=0,
  vbr_mt,               /* obsolete, same as vbr_mtrh */
  vbr_rh,
  vbr_abr,
  vbr_mtrh,
  vbr_max_indicator,    /* Don't use this! It's used for sanity checks.       */
  vbr_default=vbr_mtrh    /* change this to change the default VBR mode of LAME */
} vbr_mode;


/* MPEG modes */
typedef enum MPEG_mode_e {
  STEREO = 0,
  JOINT_STEREO,
  DUAL_CHANNEL,   /* LAME doesn't supports this! */
  MONO,
  NOT_SET,
  MAX_INDICATOR   /* Don't use this! It's used for sanity checks. */
} MPEG_mode;

/* Padding types */
typedef enum Padding_type_e {
  PAD_NO = 0,
  PAD_ALL,
  PAD_ADJUST,
  PAD_MAX_INDICATOR   /* Don't use this! It's used for sanity checks. */
} Padding_type;



/*presets*/
typedef enum preset_mode_e {
    /*values from 8 to 320 should be reserved for abr bitrates*/
    /*for abr I'd suggest to directly use the targeted bitrate as a value*/
    ABR_8 = 8,
    ABR_320 = 320,

    V9 = 410, /*Vx to match Lame and VBR_xx to match FhG*/
    VBR_10 = 410,
    V8 = 420,
    VBR_20 = 420,
    V7 = 430,
    VBR_30 = 430,
    V6 = 440,
    VBR_40 = 440,
    V5 = 450,
    VBR_50 = 450,
    V4 = 460,
    VBR_60 = 460,
    V3 = 470,
    VBR_70 = 470,
    V2 = 480,
    VBR_80 = 480,
    V1 = 490,
    VBR_90 = 490,
    V0 = 500,
    VBR_100 = 500,



    /*still there for compatibility*/
    R3MIX = 1000,
    STANDARD = 1001,
    EXTREME = 1002,
    INSANE = 1003,
    STANDARD_FAST = 1004,
    EXTREME_FAST = 1005,
    MEDIUM = 1006,
    MEDIUM_FAST = 1007
} preset_mode;


/*asm optimizations*/
typedef enum asm_optimizations_e {
    MMX = 1,
    AMD_3DNOW = 2,
    SSE = 3
} asm_optimizations;


/* psychoacoustic model */
typedef enum Psy_model_e {
    PSY_GPSYCHO = 1,
    PSY_NSPSYTUNE = 2
} Psy_model;


/* buffer considerations */
typedef enum buffer_constraint_e {
    MDB_DEFAULT=0,
    MDB_STRICT_ISO=1,
    MDB_MAXIMUM=2
} buffer_constraint;


struct lame_global_struct;
typedef struct lame_global_struct lame_global_flags;
typedef lame_global_flags *lame_t;



# define __STR(x)  #x
# define STR(x)    __STR(x)

# define LAME_URL              "http://lame.sf.net"


# define LAME_MAJOR_VERSION      3 /* Major version number */
# define LAME_MINOR_VERSION    100 /* Minor version number */
# define LAME_TYPE_VERSION       2 /* 0:alpha 1:beta 2:release */
# define LAME_PATCH_VERSION      0 /* Patch level */
# define LAME_ALPHA_VERSION     (LAME_TYPE_VERSION==0)
# define LAME_BETA_VERSION      (LAME_TYPE_VERSION==1)
# define LAME_RELEASE_VERSION   (LAME_TYPE_VERSION==2)

# define PSY_MAJOR_VERSION       1 /* Major version number */
# define PSY_MINOR_VERSION       0 /* Minor version number */
# define PSY_ALPHA_VERSION       0 /* Set number if this is an alpha version, otherwise zero */
# define PSY_BETA_VERSION        0 /* Set number if this is a beta version, otherwise zero */

#if LAME_ALPHA_VERSION
#define LAME_PATCH_LEVEL_STRING " alpha " STR(LAME_PATCH_VERSION)
#endif
#if LAME_BETA_VERSION
#define LAME_PATCH_LEVEL_STRING " beta " STR(LAME_PATCH_VERSION)
#endif
#if LAME_RELEASE_VERSION
#if LAME_PATCH_VERSION
#define LAME_PATCH_LEVEL_STRING " release " STR(LAME_PATCH_VERSION)
#else
#define LAME_PATCH_LEVEL_STRING ""
#endif
#endif

#define LAME_VERSION_STRING STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) LAME_PATCH_LEVEL_STRING





/***********************************************************************
 *
 *  The LAME API
 *  These functions should be called, in this order, for each
 *  MP3 file to be encoded.  See the file "API" for more documentation
 *
 ***********************************************************************/


/*
 * REQUIRED:
 * initialize the encoder.  sets default for all encoder parameters,
 * returns NULL if some malloc()'s failed
 * otherwise returns pointer to structure needed for all future
 * API calls.
 */
lame_global_flags * CDECL lame_init(void);
#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
/* obsolete version */
int CDECL lame_init_old(lame_global_flags *);
#endif

/*
 * OPTIONAL:
 * set as needed to override defaults
 */

/********************************************************************
 *  input stream description
 ***********************************************************************/
/* number of samples.  default = 2^32-1   */
int CDECL lame_set_num_samples(lame_global_flags *, unsigned long);
unsigned long CDECL lame_get_num_samples(const lame_global_flags *);

/* input sample rate in Hz.  default = 44100hz */
int CDECL lame_set_in_samplerate(lame_global_flags *, int);
int CDECL lame_get_in_samplerate(const lame_global_flags *);

/* number of channels in input stream. default=2  */
int CDECL lame_set_num_channels(lame_global_flags *, int);
int CDECL lame_get_num_channels(const lame_global_flags *);

/*
  scale the input by this amount before encoding.  default=1
  (not used by decoding routines)
*/
int CDECL lame_set_scale(lame_global_flags *, float);
float CDECL lame_get_scale(const lame_global_flags *);

/*
  scale the channel 0 (left) input by this amount before encoding.  default=1
  (not used by decoding routines)
*/
int CDECL lame_set_scale_left(lame_global_flags *, float);
float CDECL lame_get_scale_left(const lame_global_flags *);

/*
  scale the channel 1 (right) input by this amount before encoding.  default=1
  (not used by decoding routines)
*/
int CDECL lame_set_scale_right(lame_global_flags *, float);
float CDECL lame_get_scale_right(const lame_global_flags *);

/*
  output sample rate in Hz.  default = 0, which means LAME picks best value
  based on the amount of compression.  MPEG only allows:
  MPEG1    32, 44.1,   48khz
  MPEG2    16, 22.05,  24
  MPEG2.5   8, 11.025, 12
  (not used by decoding routines)
*/
int CDECL lame_set_out_samplerate(lame_global_flags *, int);
int CDECL lame_get_out_samplerate(const lame_global_flags *);


/********************************************************************
 *  general control parameters
 ***********************************************************************/
/* 1=cause LAME to collect data for an MP3 frame analyzer. default=0 */
int CDECL lame_set_analysis(lame_global_flags *, int);
int CDECL lame_get_analysis(const lame_global_flags *);

/*
  1 = write a Xing VBR header frame.
  default = 1
  this variable must have been added by a Hungarian notation Windows programmer :-)
*/
int CDECL lame_set_bWriteVbrTag(lame_global_flags *, int);
int CDECL lame_get_bWriteVbrTag(const lame_global_flags *);

/* 1=decode only.  use lame/mpglib to convert mp3/ogg to wav.  default=0 */
int CDECL lame_set_decode_only(lame_global_flags *, int);
int CDECL lame_get_decode_only(const lame_global_flags *);

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
/* 1=encode a Vorbis .ogg file.  default=0 */
/* DEPRECATED */
int CDECL lame_set_ogg(lame_global_flags *, int);
int CDECL lame_get_ogg(const lame_global_flags *);
#endif

/*
  internal algorithm selection.  True quality is determined by the bitrate
  but this variable will effect quality by selecting expensive or cheap algorithms.
  quality=0..9.  0=best (very slow).  9=worst.
  recommended:  2     near-best quality, not too slow
                5     good quality, fast
                7     ok quality, really fast
*/
int CDECL lame_set_quality(lame_global_flags *, int);
int CDECL lame_get_quality(const lame_global_flags *);

/*
  mode = 0,1,2,3 = stereo, jstereo, dual channel (not supported), mono
  default: lame picks based on compression ration and input channels
*/
int CDECL lame_set_mode(lame_global_flags *, MPEG_mode);
MPEG_mode CDECL lame_get_mode(const lame_global_flags *);

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
/*
  mode_automs.  Use a M/S mode with a switching threshold based on
  compression ratio
  DEPRECATED
*/
int CDECL lame_set_mode_automs(lame_global_flags *, int);
int CDECL lame_get_mode_automs(const lame_global_flags *);
#endif

/*
  force_ms.  Force M/S for all frames.  For testing only.
  default = 0 (disabled)
*/
int CDECL lame_set_force_ms(lame_global_flags *, int);
int CDECL lame_get_force_ms(const lame_global_flags *);

/* use free_format?  default = 0 (disabled) */
int CDECL lame_set_free_format(lame_global_flags *, int);
int CDECL lame_get_free_format(const lame_global_flags *);

/* perform ReplayGain analysis?  default = 0 (disabled) */
int CDECL lame_set_findReplayGain(lame_global_flags *, BYTE);
BYTE CDECL lame_get_findReplayGain(const lame_global_flags *);

/* decode on the fly. Search for the peak sample. If the ReplayGain
 * analysis is enabled then perform the analysis on the decoded data
 * stream. default = 0 (disabled)
 * NOTE: if this option is set the build-in decoder should not be used */
int CDECL lame_set_decode_on_the_fly(lame_global_flags *, int);
int CDECL lame_get_decode_on_the_fly(const lame_global_flags *);

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
/* DEPRECATED: now does the same as lame_set_findReplayGain()
   default = 0 (disabled) */
int CDECL lame_set_ReplayGain_input(lame_global_flags *, int);
int CDECL lame_get_ReplayGain_input(const lame_global_flags *);

/* DEPRECATED: now does the same as
   lame_set_decode_on_the_fly() && lame_set_findReplayGain()
   default = 0 (disabled) */
int CDECL lame_set_ReplayGain_decode(lame_global_flags *, int);
int CDECL lame_get_ReplayGain_decode(const lame_global_flags *);

/* DEPRECATED: now does the same as lame_set_decode_on_the_fly()
   default = 0 (disabled) */
int CDECL lame_set_findPeakSample(lame_global_flags *, int);
int CDECL lame_get_findPeakSample(const lame_global_flags *);
#endif

/* counters for gapless encoding */
int CDECL lame_set_nogap_total(lame_global_flags*, int);
int CDECL lame_get_nogap_total(const lame_global_flags*);

int CDECL lame_set_nogap_currentindex(lame_global_flags* , int);
int CDECL lame_get_nogap_currentindex(const lame_global_flags*);


/*
 * OPTIONAL:
 * Set printf like error/debug/message reporting functions.
 * The second argument has to be a pointer to a function which looks like
 *   void my_debugf(const char *format, va_list ap)
 *   {
 *       (void) vfprintf(stdout, format, ap);
 *   }
 * If you use NULL as the value of the pointer in the set function, the
 * lame buildin function will be used (prints to stderr).
 * To quiet any output you have to replace the body of the example function
 * with just "return;" and use it in the set function.
 */
int CDECL lame_set_errorf(lame_global_flags *, lame_report_function);
int CDECL lame_set_debugf(lame_global_flags *, lame_report_function);
int CDECL lame_set_msgf  (lame_global_flags *, lame_report_function);



/* set one of brate compression ratio.  default is compression ratio of 11.  */
int CDECL lame_set_brate(lame_global_flags *, int);
int CDECL lame_get_brate(const lame_global_flags *);
int CDECL lame_set_compression_ratio(lame_global_flags *, float);
float CDECL lame_get_compression_ratio(const lame_global_flags *);


int CDECL lame_set_preset( lame_global_flags*  gfp, int );
int CDECL lame_set_asm_optimizations( lame_global_flags*  gfp, int, int );



/********************************************************************
 *  frame params
 ***********************************************************************/
/* mark as copyright.  default=0 */
int CDECL lame_set_copyright(lame_global_flags *, int);
int CDECL lame_get_copyright(const lame_global_flags *);

/* mark as original.  default=1 */
int CDECL lame_set_original(lame_global_flags *, int);
int CDECL lame_get_original(const lame_global_flags *);

/* error_protection.  Use 2 bytes from each frame for CRC checksum. default=0 */
int CDECL lame_set_error_protection(lame_global_flags *, int);
int CDECL lame_get_error_protection(const lame_global_flags *);

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
/* padding_type. 0=pad no frames  1=pad all frames 2=adjust padding(default) */
int CDECL lame_set_padding_type(lame_global_flags *, Padding_type);
Padding_type CDECL lame_get_padding_type(const lame_global_flags *);
#endif

/* MP3 'private extension' bit  Meaningless.  default=0 */
int CDECL lame_set_extension(lame_global_flags *, int);
int CDECL lame_get_extension(const lame_global_flags *);

/* enforce strict ISO compliance.  default=0 */
int CDECL lame_set_strict_ISO(lame_global_flags *, int);
int CDECL lame_get_strict_ISO(const lame_global_flags *);


/********************************************************************
 * quantization/noise shaping
 ***********************************************************************/

/* disable the bit reservoir. For testing only. default=0 */
int CDECL lame_set_disable_reservoir(lame_global_flags *, int);
int CDECL lame_get_disable_reservoir(const lame_global_flags *);

/* select a different "best quantization" function. default=0  */
int CDECL lame_set_quant_comp(lame_global_flags *, int);
int CDECL lame_get_quant_comp(const lame_global_flags *);
int CDECL lame_set_quant_comp_short(lame_global_flags *, int);
int CDECL lame_get_quant_comp_short(const lame_global_flags *);

int CDECL lame_set_experimentalX(lame_global_flags *, int); /* compatibility*/
int CDECL lame_get_experimentalX(const lame_global_flags *);

/* another experimental option.  for testing only */
int CDECL lame_set_experimentalY(lame_global_flags *, int);
int CDECL lame_get_experimentalY(const lame_global_flags *);

/* another experimental option.  for testing only */
int CDECL lame_set_experimentalZ(lame_global_flags *, int);
int CDECL lame_get_experimentalZ(const lame_global_flags *);

/* Naoki's psycho acoustic model.  default=0 */
int CDECL lame_set_exp_nspsytune(lame_global_flags *, int);
int CDECL lame_get_exp_nspsytune(const lame_global_flags *);

void CDECL lame_set_msfix(lame_global_flags *, double);
float CDECL lame_get_msfix(const lame_global_flags *);


/********************************************************************
 * VBR control
 ***********************************************************************/
/* Types of VBR.  default = vbr_off = CBR */
int CDECL lame_set_VBR(lame_global_flags *, vbr_mode);
vbr_mode CDECL lame_get_VBR(const lame_global_flags *);

/* VBR quality level.  0=highest  9=lowest  */
int CDECL lame_set_VBR_q(lame_global_flags *, int);
int CDECL lame_get_VBR_q(const lame_global_flags *);

/* VBR quality level.  0=highest  9=lowest, Range [0,...,10[  */
int CDECL lame_set_VBR_quality(lame_global_flags *, float);
float CDECL lame_get_VBR_quality(const lame_global_flags *);

/* Ignored except for VBR=vbr_abr (ABR mode) */
int CDECL lame_set_VBR_mean_bitrate_kbps(lame_global_flags *, int);
int CDECL lame_get_VBR_mean_bitrate_kbps(const lame_global_flags *);

int CDECL lame_set_VBR_min_bitrate_kbps(lame_global_flags *, int);
int CDECL lame_get_VBR_min_bitrate_kbps(const lame_global_flags *);

int CDECL lame_set_VBR_max_bitrate_kbps(lame_global_flags *, int);
int CDECL lame_get_VBR_max_bitrate_kbps(const lame_global_flags *);

/*
  1=strictly enforce VBR_min_bitrate.  Normally it will be violated for
  analog silence
*/
int CDECL lame_set_VBR_hard_min(lame_global_flags *, int);
int CDECL lame_get_VBR_hard_min(const lame_global_flags *);

/* for preset */
#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
int CDECL lame_set_preset_expopts(lame_global_flags *, int);
#endif

/********************************************************************
 * Filtering control
 ***********************************************************************/
/* freq in Hz to apply lowpass. Default = 0 = lame chooses.  -1 = disabled */
int CDECL lame_set_lowpassfreq(lame_global_flags *, int);
int CDECL lame_get_lowpassfreq(const lame_global_flags *);
/* width of transition band, in Hz.  Default = one polyphase filter band */
int CDECL lame_set_lowpasswidth(lame_global_flags *, int);
int CDECL lame_get_lowpasswidth(const lame_global_flags *);

/* freq in Hz to apply highpass. Default = 0 = lame chooses.  -1 = disabled */
int CDECL lame_set_highpassfreq(lame_global_flags *, int);
int CDECL lame_get_highpassfreq(const lame_global_flags *);
/* width of transition band, in Hz.  Default = one polyphase filter band */
int CDECL lame_set_highpasswidth(lame_global_flags *, int);
int CDECL lame_get_highpasswidth(const lame_global_flags *);


/********************************************************************
 * psycho acoustics and other arguments which you should not change
 * unless you know what you are doing
 ***********************************************************************/

/* only use ATH for masking */
int CDECL lame_set_ATHonly(lame_global_flags *, int);
int CDECL lame_get_ATHonly(const lame_global_flags *);

/* only use ATH for short blocks */
int CDECL lame_set_ATHshort(lame_global_flags *, int);
int CDECL lame_get_ATHshort(const lame_global_flags *);

/* disable ATH */
int CDECL lame_set_noATH(lame_global_flags *, int);
int CDECL lame_get_noATH(const lame_global_flags *);

/* select ATH formula */
int CDECL lame_set_ATHtype(lame_global_flags *, int);
int CDECL lame_get_ATHtype(const lame_global_flags *);

/* lower ATH by this many db */
int CDECL lame_set_ATHlower(lame_global_flags *, float);
float CDECL lame_get_ATHlower(const lame_global_flags *);

/* select ATH adaptive adjustment type */
int CDECL lame_set_athaa_type( lame_global_flags *, int);
int CDECL lame_get_athaa_type( const lame_global_flags *);

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
/* select the loudness approximation used by the ATH adaptive auto-leveling  */
int CDECL lame_set_athaa_loudapprox( lame_global_flags *, int);
int CDECL lame_get_athaa_loudapprox( const lame_global_flags *);
#endif

/* adjust (in dB) the point below which adaptive ATH level adjustment occurs */
int CDECL lame_set_athaa_sensitivity( lame_global_flags *, float);
float CDECL lame_get_athaa_sensitivity( const lame_global_flags* );

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
/* OBSOLETE: predictability limit (ISO tonality formula) */
int CDECL lame_set_cwlimit(lame_global_flags *, int);
int CDECL lame_get_cwlimit(const lame_global_flags *);
#endif

/*
  allow blocktypes to differ between channels?
  default: 0 for jstereo, 1 for stereo
*/
int CDECL lame_set_allow_diff_short(lame_global_flags *, int);
int CDECL lame_get_allow_diff_short(const lame_global_flags *);

/* use temporal masking effect (default = 1) */
int CDECL lame_set_useTemporal(lame_global_flags *, int);
int CDECL lame_get_useTemporal(const lame_global_flags *);

/* use temporal masking effect (default = 1) */
int CDECL lame_set_interChRatio(lame_global_flags *, float);
float CDECL lame_get_interChRatio(const lame_global_flags *);

/* disable short blocks */
int CDECL lame_set_no_short_blocks(lame_global_flags *, int);
int CDECL lame_get_no_short_blocks(const lame_global_flags *);

/* force short blocks */
int CDECL lame_set_force_short_blocks(lame_global_flags *, int);
int CDECL lame_get_force_short_blocks(const lame_global_flags *);

/* Input PCM is emphased PCM (for instance from one of the rarely
   emphased CDs), it is STRONGLY not recommended to use this, because
   psycho does not take it into account, and last but not least many decoders
   ignore these bits */
int CDECL lame_set_emphasis(lame_global_flags *, int);
int CDECL lame_get_emphasis(const lame_global_flags *);



/************************************************************************/
/* internal variables, cannot be set...                                 */
/* provided because they may be of use to calling application           */
/************************************************************************/
/* version  0=MPEG-2  1=MPEG-1  (2=MPEG-2.5)     */
int CDECL lame_get_version(const lame_global_flags *);

/* encoder delay   */
int CDECL lame_get_encoder_delay(const lame_global_flags *);

/*
  padding appended to the input to make sure decoder can fully decode
  all input.  Note that this value can only be calculated during the
  call to lame_encoder_flush().  Before lame_encoder_flush() has
  been called, the value of encoder_padding = 0.
*/
int CDECL lame_get_encoder_padding(const lame_global_flags *);

/* size of MPEG frame */
int CDECL lame_get_framesize(const lame_global_flags *);

/* number of PCM samples buffered, but not yet encoded to mp3 data. */
int CDECL lame_get_mf_samples_to_encode( const lame_global_flags*  gfp );

/*
  size (bytes) of mp3 data buffered, but not yet encoded.
  this is the number of bytes which would be output by a call to
  lame_encode_flush_nogap.  NOTE: lame_encode_flush() will return
  more bytes than this because it will encode the reamining buffered
  PCM samples before flushing the mp3 buffers.
*/
int CDECL lame_get_size_mp3buffer( const lame_global_flags*  gfp );

/* number of frames encoded so far */
int CDECL lame_get_frameNum(const lame_global_flags *);

/*
  lame's estimate of the total number of frames to be encoded
   only valid if calling program set num_samples
*/
int CDECL lame_get_totalframes(const lame_global_flags *);

/* RadioGain value. Multiplied by 10 and rounded to the nearest. */
int CDECL lame_get_RadioGain(const lame_global_flags *);

/* AudiophileGain value. Multipled by 10 and rounded to the nearest. */
int CDECL lame_get_AudiophileGain(const lame_global_flags *);

/* the peak sample */
float CDECL lame_get_PeakSample(const lame_global_flags *);

/* Gain change required for preventing clipping. The value is correct only if
   peak sample searching was enabled. If negative then the waveform
   already does not clip. The value is multiplied by 10 and rounded up. */
int CDECL lame_get_noclipGainChange(const lame_global_flags *);

/* user-specified scale factor required for preventing clipping. Value is
   correct only if peak sample searching was enabled and no user-specified
   scaling was performed. If negative then either the waveform already does
   not clip or the value cannot be determined */
float CDECL lame_get_noclipScale(const lame_global_flags *);

/* returns the limit of PCM samples, which one can pass in an encode call
   under the constrain of a provided buffer of size buffer_size */
int CDECL lame_get_maximum_number_of_samples(lame_t gfp, size_t buffer_size);




/*
 * REQUIRED:
 * sets more internal configuration based on data provided above.
 * returns -1 if something failed.
 */
int CDECL lame_init_params(lame_global_flags *);


/*
 * OPTIONAL:
 * get the version number, in a string. of the form:
 * "3.63 (beta)" or just "3.63".
 */
const char*  CDECL get_lame_version       ( void );
const char*  CDECL get_lame_short_version ( void );
const char*  CDECL get_lame_very_short_version ( void );
const char*  CDECL get_psy_version        ( void );
const char*  CDECL get_lame_url           ( void );
const char*  CDECL get_lame_os_bitness    ( void );

/*
 * OPTIONAL:
 * get the version numbers in numerical form.
 */
typedef struct {
    /* generic LAME version */
    int major;
    int minor;
    int alpha;               /* 0 if not an alpha version                  */
    int beta;                /* 0 if not a beta version                    */

    /* version of the psy model */
    int psy_major;
    int psy_minor;
    int psy_alpha;           /* 0 if not an alpha version                  */
    int psy_beta;            /* 0 if not a beta version                    */

    /* compile time features */
    const char *features;    /* Don't make assumptions about the contents! */
} lame_version_t;
void CDECL get_lame_version_numerical(lame_version_t *);


/*
 * OPTIONAL:
 * print internal lame configuration to message handler
 */
void CDECL lame_print_config(const lame_global_flags*);

void CDECL lame_print_internals( const lame_global_flags *);


/*
 * input pcm data, output (maybe) mp3 frames.
 * This routine handles all buffering, resampling and filtering for you.
 *
 * return code     number of bytes output in mp3buf. Can be 0
 *                 -1:  mp3buf was too small
 *                 -2:  malloc() problem
 *                 -3:  lame_init_params() not called
 *                 -4:  psycho acoustic problems
 *
 * The required mp3buf_size can be computed from num_samples,
 * samplerate and encoding rate, but here is a worst case estimate:
 *
 * mp3buf_size in bytes = 1.25*num_samples + 7200
 *
 * I think a tighter bound could be:  (mt, March 2000)
 * MPEG1:
 *    num_samples*(bitrate/8)/samplerate + 4*1152*(bitrate/8)/samplerate + 512
 * MPEG2:
 *    num_samples*(bitrate/8)/samplerate + 4*576*(bitrate/8)/samplerate + 256
 *
 * but test first if you use that!
 *
 * set mp3buf_size = 0 and LAME will not check if mp3buf_size is
 * large enough.
 *
 * NOTE:
 * if gfp->num_channels=2, but gfp->mode = 3 (mono), the L & R channels
 * will be averaged into the L channel before encoding only the L channel
 * This will overwrite the data in buffer_l[] and buffer_r[].
 *
*/
int CDECL lame_encode_buffer(
        lame_global_flags*  gfp,           /* global context handle         */
        const short int     buffer_l [],   /* PCM data for left channel     */
        const short int     buffer_r [],   /* PCM data for right channel    */
        const int           nsamples,      /* number of samples per channel */
        unsigned char*      mp3buf,        /* pointer to encoded MP3 stream */
        const int           mp3buf_size ); /* number of valid octets in this
                                              stream                        */

/*
 * as above, but input has L & R channel data interleaved.
 * NOTE:
 * num_samples = number of samples in the L (or R)
 * channel, not the total number of samples in pcm[]
 */
int /*CDECL*/ lame_encode_buffer_interleaved(
        lame_global_flags*  gfp,           /* global context handle         */
        const short int     pcm[],         /* PCM data for left and right
                                              channel, interleaved          */
        int                 num_samples,   /* number of samples per channel,
                                              _not_ number of samples in
                                              pcm[]                         */
        unsigned char*      mp3buf,        /* pointer to encoded MP3 stream */
        int                 mp3buf_size ); /* number of valid octets in this
                                              stream                        */


/* as lame_encode_buffer, but for 'float's.
 * !! NOTE: !! data must still be scaled to be in the same range as
 * short int, +/- 32768
 */
int CDECL lame_encode_buffer_float(
        lame_global_flags*  gfp,           /* global context handle         */
        const float         pcm_l [],      /* PCM data for left channel     */
        const float         pcm_r [],      /* PCM data for right channel    */
        const int           nsamples,      /* number of samples per channel */
        unsigned char*      mp3buf,        /* pointer to encoded MP3 stream */
        const int           mp3buf_size ); /* number of valid octets in this
                                              stream                        */

/* as lame_encode_buffer, but for 'float's.
 * !! NOTE: !! data must be scaled to +/- 1 full scale
 */
int CDECL lame_encode_buffer_ieee_float(
        lame_t          gfp,
        const float     pcm_l [],          /* PCM data for left channel     */
        const float     pcm_r [],          /* PCM data for right channel    */
        const int       nsamples,
        unsigned char * mp3buf,
        const int       mp3buf_size);
int CDECL lame_encode_buffer_interleaved_ieee_float(
        lame_t          gfp,
        const float     pcm[],             /* PCM data for left and right
                                              channel, interleaved          */
        const int       nsamples,
        unsigned char * mp3buf,
        const int       mp3buf_size);

/* as lame_encode_buffer, but for 'double's.
 * !! NOTE: !! data must be scaled to +/- 1 full scale
 */
int CDECL lame_encode_buffer_ieee_double(
        lame_t          gfp,
        const double    pcm_l [],          /* PCM data for left channel     */
        const double    pcm_r [],          /* PCM data for right channel    */
        const int       nsamples,
        unsigned char * mp3buf,
        const int       mp3buf_size);
int CDECL lame_encode_buffer_interleaved_ieee_double(
        lame_t          gfp,
        const double    pcm[],             /* PCM data for left and right
                                              channel, interleaved          */
        const int       nsamples,
        unsigned char * mp3buf,
        const int       mp3buf_size);

/* as lame_encode_buffer, but for long's
 * !! NOTE: !! data must still be scaled to be in the same range as
 * short int, +/- 32768
 *
 * This scaling was a mistake (doesn't allow one to exploit full
 * precision of type 'long'.  Use lame_encode_buffer_long2() instead.
 *
 */
int CDECL lame_encode_buffer_long(
        lame_global_flags*  gfp,           /* global context handle         */
        const long     buffer_l [],       /* PCM data for left channel     */
        const long     buffer_r [],       /* PCM data for right channel    */
        const int           nsamples,      /* number of samples per channel */
        unsigned char*      mp3buf,        /* pointer to encoded MP3 stream */
        const int           mp3buf_size ); /* number of valid octets in this
                                              stream                        */

/* Same as lame_encode_buffer_long(), but with correct scaling.
 * !! NOTE: !! data must still be scaled to be in the same range as
 * type 'long'.   Data should be in the range:  +/- 2^(8*size(long)-1)
 *
 */
int CDECL lame_encode_buffer_long2(
        lame_global_flags*  gfp,           /* global context handle         */
        const long     buffer_l [],       /* PCM data for left channel     */
        const long     buffer_r [],       /* PCM data for right channel    */
        const int           nsamples,      /* number of samples per channel */
        unsigned char*      mp3buf,        /* pointer to encoded MP3 stream */
        const int           mp3buf_size ); /* number of valid octets in this
                                              stream                        */

/* as lame_encode_buffer, but for int's
 * !! NOTE: !! input should be scaled to the maximum range of 'int'
 * If int is 4 bytes, then the values should range from
 * +/- 2147483648.
 *
 * This routine does not (and cannot, without loosing precision) use
 * the same scaling as the rest of the lame_encode_buffer() routines.
 *
 */
int CDECL lame_encode_buffer_int(
        lame_global_flags*  gfp,           /* global context handle         */
        const int      buffer_l [],       /* PCM data for left channel     */
        const int      buffer_r [],       /* PCM data for right channel    */
        const int           nsamples,      /* number of samples per channel */
        unsigned char*      mp3buf,        /* pointer to encoded MP3 stream */
        const int           mp3buf_size ); /* number of valid octets in this
                                              stream                        */

/*
 * as above, but for interleaved data.
 * !! NOTE: !! data must still be scaled to be in the same range as
 * type 'int32_t'.   Data should be in the range:  +/- 2^(8*size(int32_t)-1)
 * NOTE:
 * num_samples = number of samples in the L (or R)
 * channel, not the total number of samples in pcm[]
 */
int
lame_encode_buffer_interleaved_int(
        lame_t          gfp,
        const int       pcm [],            /* PCM data for left and right
                                              channel, interleaved          */
        const int       nsamples,          /* number of samples per channel,
                                              _not_ number of samples in
                                              pcm[]                         */
        unsigned char*  mp3buf,            /* pointer to encoded MP3 stream */
        const int       mp3buf_size );     /* number of valid octets in this
                                              stream                        */



/*
 * REQUIRED:
 * lame_encode_flush will flush the intenal PCM buffers, padding with
 * 0's to make sure the final frame is complete, and then flush
 * the internal MP3 buffers, and thus may return a
 * final few mp3 frames.  'mp3buf' should be at least 7200 bytes long
 * to hold all possible emitted data.
 *
 * will also write id3v1 tags (if any) into the bitstream
 *
 * return code = number of bytes output to mp3buf. Can be 0
 */
int CDECL lame_encode_flush(
        lame_global_flags *  gfp,    /* global context handle                 */
        unsigned char*       mp3buf, /* pointer to encoded MP3 stream         */
        int                  size);  /* number of valid octets in this stream */

/*
 * OPTIONAL:
 * lame_encode_flush_nogap will flush the internal mp3 buffers and pad
 * the last frame with ancillary data so it is a complete mp3 frame.
 *
 * 'mp3buf' should be at least 7200 bytes long
 * to hold all possible emitted data.
 *
 * After a call to this routine, the outputed mp3 data is complete, but
 * you may continue to encode new PCM samples and write future mp3 data
 * to a different file.  The two mp3 files will play back with no gaps
 * if they are concatenated together.
 *
 * This routine will NOT write id3v1 tags into the bitstream.
 *
 * return code = number of bytes output to mp3buf. Can be 0
 */
int CDECL lame_encode_flush_nogap(
        lame_global_flags *  gfp,    /* global context handle                 */
        unsigned char*       mp3buf, /* pointer to encoded MP3 stream         */
        int                  size);  /* number of valid octets in this stream */

/*
 * OPTIONAL:
 * Normally, this is called by lame_init_params().  It writes id3v2 and
 * Xing headers into the front of the bitstream, and sets frame counters
 * and bitrate histogram data to 0.  You can also call this after
 * lame_encode_flush_nogap().
 */
int CDECL lame_init_bitstream(
        lame_global_flags *gfp);    /* global context handle                 */



/*
 * OPTIONAL:    some simple statistics
 * a bitrate histogram to visualize the distribution of used frame sizes
 * a stereo mode histogram to visualize the distribution of used stereo
 *   modes, useful in joint-stereo mode only
 *   0: LR    left-right encoded
 *   1: LR-I  left-right and intensity encoded (currently not supported)
 *   2: MS    mid-side encoded
 *   3: MS-I  mid-side and intensity encoded (currently not supported)
 *
 * attention: don't call them after lame_encode_finish
 * suggested: lame_encode_flush -> lame_*_hist -> lame_close
 */

void CDECL lame_bitrate_hist(const lame_global_flags * gfp,
        int bitrate_count[14] );
void CDECL lame_bitrate_kbps(const lame_global_flags * gfp,
        int bitrate_kbps [14] );
void CDECL lame_stereo_mode_hist(
        const lame_global_flags * gfp,
        int stereo_mode_count[4] );

void CDECL lame_bitrate_stereo_mode_hist (
        const lame_global_flags * gfp,
        int bitrate_stmode_count[14][4] );

void CDECL lame_block_type_hist (
        const lame_global_flags * gfp,
        int btype_count[6] );

void CDECL lame_bitrate_block_type_hist (
        const lame_global_flags * gfp,
        int bitrate_btype_count[14][6] );

#if (DEPRECATED_OR_OBSOLETE_CODE_REMOVED && 0)
#else
/*
 * OPTIONAL:
 * lame_mp3_tags_fid will rewrite a Xing VBR tag to the mp3 file with file
 * pointer fid.  These calls perform forward and backwards seeks, so make
 * sure fid is a real file.  Make sure lame_encode_flush has been called,
 * and all mp3 data has been written to the file before calling this
 * function.
 * NOTE:
 * if VBR  tags are turned off by the user, or turned off by LAME because
 * the output is not a regular file, this call does nothing
 * NOTE:
 * LAME wants to read from the file to skip an optional ID3v2 tag, so
 * make sure you opened the file for writing and reading.
 * NOTE:
 * You can call lame_get_lametag_frame instead, if you want to insert
 * the lametag yourself.
*/
void CDECL lame_mp3_tags_fid(lame_global_flags *, FILE* fid);
#endif

/*
 * OPTIONAL:
 * lame_get_lametag_frame copies the final LAME-tag into 'buffer'.
 * The function returns the number of bytes copied into buffer, or
 * the required buffer size, if the provided buffer is too small.
 * Function failed, if the return value is larger than 'size'!
 * Make sure lame_encode flush has been called before calling this function.
 * NOTE:
 * if VBR  tags are turned off by the user, or turned off by LAME,
 * this call does nothing and returns 0.
 * NOTE:
 * LAME inserted an empty frame in the beginning of mp3 audio data,
 * which you have to replace by the final LAME-tag frame after encoding.
 * In case there is no ID3v2 tag, usually this frame will be the very first
 * data in your mp3 file. If you put some other leading data into your
 * file, you'll have to do some bookkeeping about where to write this buffer.
 */
size_t CDECL lame_get_lametag_frame(
        const lame_global_flags *, unsigned char* buffer, size_t size);

/*
 * REQUIRED:
 * final call to free all remaining buffers
 */
int  CDECL lame_close (lame_global_flags *);

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
/*
 * OBSOLETE:
 * lame_encode_finish combines lame_encode_flush() and lame_close() in
 * one call.  However, once this call is made, the statistics routines
 * will no longer work because the data will have been cleared, and
 * lame_mp3_tags_fid() cannot be called to add data to the VBR header
 */
int CDECL lame_encode_finish(
        lame_global_flags*  gfp,
        unsigned char*      mp3buf,
        int                 size );
#endif






/*********************************************************************
 *
 * decoding
 *
 * a simple interface to mpglib, part of mpg123, is also included if
 * libmp3lame is compiled with HAVE_MPGLIB
 *
 *********************************************************************/

struct hip_global_struct;
typedef struct hip_global_struct hip_global_flags;
typedef hip_global_flags *hip_t;


typedef struct {
  int header_parsed;   /* 1 if header was parsed and following data was
                          computed                                       */
  int stereo;          /* number of channels                             */
  int samplerate;      /* sample rate                                    */
  int bitrate;         /* bitrate                                        */
  int mode;            /* mp3 frame type                                 */
  int mode_ext;        /* mp3 frame type                                 */
  int framesize;       /* number of samples per mp3 frame                */

  /* this data is only computed if mpglib detects a Xing VBR header */
  unsigned long nsamp; /* number of samples in mp3 file.                 */
  int totalframes;     /* total number of frames in mp3 file             */

  /* this data is not currently computed by the mpglib routines */
  int framenum;        /* frames decoded counter                         */
} mp3data_struct;

/* required call to initialize decoder */
hip_t CDECL hip_decode_init(void);

/* cleanup call to exit decoder  */
int CDECL hip_decode_exit(hip_t gfp);

/* HIP reporting functions */
void CDECL hip_set_errorf(hip_t gfp, lame_report_function f);
void CDECL hip_set_debugf(hip_t gfp, lame_report_function f);
void CDECL hip_set_msgf  (hip_t gfp, lame_report_function f);

/*********************************************************************
 * input 1 mp3 frame, output (maybe) pcm data.
 *
 *  nout = hip_decode(hip, mp3buf,len,pcm_l,pcm_r);
 *
 * input:
 *    len          :  number of bytes of mp3 data in mp3buf
 *    mp3buf[len]  :  mp3 data to be decoded
 *
 * output:
 *    nout:  -1    : decoding error
 *            0    : need more data before we can complete the decode
 *           >0    : returned 'nout' samples worth of data in pcm_l,pcm_r
 *    pcm_l[nout]  : left channel data
 *    pcm_r[nout]  : right channel data
 *
 *********************************************************************/
int CDECL hip_decode( hip_t           gfp
                    , unsigned char * mp3buf
                    , size_t          len
                    , short           pcm_l[]
                    , short           pcm_r[]
                    );

/* same as hip_decode, and also returns mp3 header data */
int CDECL hip_decode_headers( hip_t           gfp
                            , unsigned char*  mp3buf
                            , size_t          len
                            , short           pcm_l[]
                            , short           pcm_r[]
                            , mp3data_struct* mp3data
                            );

/* same as hip_decode, but returns at most one frame */
int CDECL hip_decode1( hip_t          gfp
                     , unsigned char* mp3buf
                     , size_t         len
                     , short          pcm_l[]
                     , short          pcm_r[]
                     );

/* same as hip_decode1, but returns at most one frame and mp3 header data */
int CDECL hip_decode1_headers( hip_t           gfp
                             , unsigned char*  mp3buf
                             , size_t          len
                             , short           pcm_l[]
                             , short           pcm_r[]
                             , mp3data_struct* mp3data
                             );

/* same as hip_decode1_headers, but also returns enc_delay and enc_padding
   from VBR Info tag, (-1 if no info tag was found) */
int CDECL hip_decode1_headersB( hip_t gfp
                              , unsigned char*   mp3buf
                              , size_t           len
                              , short            pcm_l[]
                              , short            pcm_r[]
                              , mp3data_struct*  mp3data
                              , int             *enc_delay
                              , int             *enc_padding
                              );



/* OBSOLETE:
 * lame_decode... functions are there to keep old code working
 * but it is strongly recommended to replace calls by hip_decode...
 * function calls, see above.
 */
#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
int CDECL lame_decode_init(void);
int CDECL lame_decode(
        unsigned char *  mp3buf,
        int              len,
        short            pcm_l[],
        short            pcm_r[] );
int CDECL lame_decode_headers(
        unsigned char*   mp3buf,
        int              len,
        short            pcm_l[],
        short            pcm_r[],
        mp3data_struct*  mp3data );
int CDECL lame_decode1(
        unsigned char*  mp3buf,
        int             len,
        short           pcm_l[],
        short           pcm_r[] );
int CDECL lame_decode1_headers(
        unsigned char*   mp3buf,
        int              len,
        short            pcm_l[],
        short            pcm_r[],
        mp3data_struct*  mp3data );
int CDECL lame_decode1_headersB(
        unsigned char*   mp3buf,
        int              len,
        short            pcm_l[],
        short            pcm_r[],
        mp3data_struct*  mp3data,
        int              *enc_delay,
        int              *enc_padding );
int CDECL lame_decode_exit(void);

#endif /* obsolete lame_decode API calls */


/*********************************************************************
 *
 * id3tag stuff
 *
 *********************************************************************/

/*
 * id3tag.h -- Interface to write ID3 version 1 and 2 tags.
 *
 * Copyright (C) 2000 Don Melton.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

/* utility to obtain alphabetically sorted list of genre names with numbers */
void CDECL id3tag_genre_list(
        void (*handler)(int, const char *, void *),
        void*  cookie);

void CDECL id3tag_init     (lame_t gfp);

/* force addition of version 2 tag */
void CDECL id3tag_add_v2   (lame_t gfp);

/* add only a version 1 tag */
void CDECL id3tag_v1_only  (lame_t gfp);

/* add only a version 2 tag */
void CDECL id3tag_v2_only  (lame_t gfp);

/* pad version 1 tag with spaces instead of nulls */
void CDECL id3tag_space_v1 (lame_t gfp);

/* pad version 2 tag with extra 128 bytes */
void CDECL id3tag_pad_v2   (lame_t gfp);

/* pad version 2 tag with extra n bytes */
void CDECL id3tag_set_pad  (lame_t gfp, size_t n);

void CDECL id3tag_set_title(lame_t gfp, const char* title);
void CDECL id3tag_set_artist(lame_t gfp, const char* artist);
void CDECL id3tag_set_album(lame_t gfp, const char* album);
void CDECL id3tag_set_year(lame_t gfp, const char* year);
void CDECL id3tag_set_comment(lame_t gfp, const char* comment);
            
/* return -1 result if track number is out of ID3v1 range
                    and ignored for ID3v1 */
int CDECL id3tag_set_track(lame_t gfp, const char* track);

/* return non-zero result if genre name or number is invalid
  result 0: OK
  result -1: genre number out of range
  result -2: no valid ID3v1 genre name, mapped to ID3v1 'Other'
             but taken as-is for ID3v2 genre tag */
int CDECL id3tag_set_genre(lame_t gfp, const char* genre);

/* return non-zero result if field name is invalid */
int CDECL id3tag_set_fieldvalue(lame_t gfp, const char* fieldvalue);

/* return non-zero result if image type is invalid */
int CDECL id3tag_set_albumart(lame_t gfp, const char* image, size_t size);

/* lame_get_id3v1_tag copies ID3v1 tag into buffer.
 * Function returns number of bytes copied into buffer, or number
 * of bytes rquired if buffer 'size' is too small.
 * Function fails, if returned value is larger than 'size'.
 * NOTE:
 * This functions does nothing, if user/LAME disabled ID3v1 tag.
 */
size_t CDECL lame_get_id3v1_tag(lame_t gfp, unsigned char* buffer, size_t size);

/* lame_get_id3v2_tag copies ID3v2 tag into buffer.
 * Function returns number of bytes copied into buffer, or number
 * of bytes rquired if buffer 'size' is too small.
 * Function fails, if returned value is larger than 'size'.
 * NOTE:
 * This functions does nothing, if user/LAME disabled ID3v2 tag.
 */
size_t CDECL lame_get_id3v2_tag(lame_t gfp, unsigned char* buffer, size_t size);

/* normaly lame_init_param writes ID3v2 tags into the audio stream
 * Call lame_set_write_id3tag_automatic(gfp, 0) before lame_init_param
 * to turn off this behaviour and get ID3v2 tag with above function
 * write it yourself into your file.
 */
void CDECL lame_set_write_id3tag_automatic(lame_global_flags *, int);
int CDECL lame_get_write_id3tag_automatic(lame_global_flags const *);

/* experimental */
int CDECL id3tag_set_textinfo_latin1(lame_t gfp, char const *id, char const *text);

/* experimental */
int CDECL id3tag_set_comment_latin1(lame_t gfp, char const *lang, char const *desc, char const *text);

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
#else
/* experimental */
int CDECL id3tag_set_textinfo_ucs2(lame_t gfp, char const *id, unsigned short const *text);

/* experimental */
int CDECL id3tag_set_comment_ucs2(lame_t gfp, char const *lang,
                                  unsigned short const *desc, unsigned short const *text);

/* experimental */
int CDECL id3tag_set_fieldvalue_ucs2(lame_t gfp, const unsigned short *fieldvalue);
#endif

/* experimental */
int CDECL id3tag_set_fieldvalue_utf16(lame_t gfp, const unsigned short *fieldvalue);

/* experimental */
int CDECL id3tag_set_textinfo_utf16(lame_t gfp, char const *id, unsigned short const *text);

/* experimental */
int CDECL id3tag_set_comment_utf16(lame_t gfp, char const *lang, unsigned short const *desc, unsigned short const *text);


/***********************************************************************
*
*  list of valid bitrates [kbps] & sample frequencies [Hz].
*  first index: 0: MPEG-2   values  (sample frequencies 16...24 kHz)
*               1: MPEG-1   values  (sample frequencies 32...48 kHz)
*               2: MPEG-2.5 values  (sample frequencies  8...12 kHz)
***********************************************************************/

extern const int     bitrate_table    [3][16];
extern const int     samplerate_table [3][ 4];

/* access functions for use in DLL, global vars are not exported */
int CDECL lame_get_bitrate(int mpeg_version, int table_index);
int CDECL lame_get_samplerate(int mpeg_version, int table_index);


/* maximum size of albumart image (128KB), which affects LAME_MAXMP3BUFFER
   as well since lame_encode_buffer() also returns ID3v2 tag data */
#define LAME_MAXALBUMART    (128 * 1024)

/* maximum size of mp3buffer needed if you encode at most 1152 samples for
   each call to lame_encode_buffer.  see lame_encode_buffer() below  
   (LAME_MAXMP3BUFFER is now obsolete)  */
#define LAME_MAXMP3BUFFER   (16384 + LAME_MAXALBUMART)






/*
 *      encoder.h include file
 *
 *      Copyright (c) 2000 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef LAME_ENCODER_H
#define LAME_ENCODER_H

/***********************************************************************
*
*  encoder and decoder delays
*
***********************************************************************/

/* 
 * layer III enc->dec delay:  1056 (1057?)   (observed)
 * layer  II enc->dec delay:   480  (481?)   (observed)
 *
 * polyphase 256-16             (dec or enc)        = 240
 * mdct      256+32  (9*32)     (dec or enc)        = 288
 * total:    512+16
 *
 * My guess is that delay of polyphase filterbank is actualy 240.5
 * (there are technical reasons for this, see postings in mp3encoder).
 * So total Encode+Decode delay = ENCDELAY + 528 + 1
 */

/* 
 * ENCDELAY  The encoder delay.  
 *
 * Minimum allowed is MDCTDELAY (see below)
 *  
 * The first 96 samples will be attenuated, so using a value less than 96
 * will result in corrupt data for the first 96-ENCDELAY samples.
 *
 * suggested: 576
 * set to 1160 to sync with FhG.
 */

#define ENCDELAY      576



/*
 * make sure there is at least one complete frame after the
 * last frame containing real data
 *
 * Using a value of 288 would be sufficient for a 
 * a very sophisticated decoder that can decode granule-by-granule instead
 * of frame by frame.  But lets not assume this, and assume the decoder  
 * will not decode frame N unless it also has data for frame N+1
 *
 */
/*#define POSTDELAY   288*/
#define POSTDELAY   1152



/* 
 * delay of the MDCT used in mdct.c
 * original ISO routines had a delay of 528!  
 * Takehiro's routines: 
 */

#define MDCTDELAY     48
#define FFTOFFSET     (224+MDCTDELAY)

/*
 * Most decoders, including the one we use, have a delay of 528 samples.  
 */

#define DECDELAY      528


/* number of subbands */
#define SBLIMIT       32

/* parition bands bands */
#define CBANDS        64

/* number of critical bands/scale factor bands where masking is computed*/
#define SBPSY_l       21
#define SBPSY_s       12

/* total number of scalefactor bands encoded */
#define SBMAX_l       22
#define SBMAX_s       13
#define PSFB21         6
#define PSFB12         6



/* FFT sizes */
#define BLKSIZE       1024
#define HBLKSIZE      (BLKSIZE/2 + 1)
#define BLKSIZE_s     256
#define HBLKSIZE_s    (BLKSIZE_s/2 + 1)


/* #define switch_pe        1800 */
#define NORM_TYPE     0
#define START_TYPE    1
#define SHORT_TYPE    2
#define STOP_TYPE     3

/* 
 * Mode Extention:
 * When we are in stereo mode, there are 4 possible methods to store these
 * two channels. The stereo modes -m? are using a subset of them.
 *
 *  -ms: MPG_MD_LR_LR
 *  -mj: MPG_MD_LR_LR and MPG_MD_MS_LR
 *  -mf: MPG_MD_MS_LR
 *  -mi: all
 */
#if 0
#define MPG_MD_LR_LR  0
#define MPG_MD_LR_I   1
#define MPG_MD_MS_LR  2
#define MPG_MD_MS_I   3
#endif
enum MPEGChannelMode
{   MPG_MD_LR_LR = 0
,   MPG_MD_LR_I  = 1
,   MPG_MD_MS_LR = 2
,   MPG_MD_MS_I  = 3
};

struct LAME_INTERNAL_FLAGS;
typedef struct LAME_INTERNAL_FLAGS lame_internal_flags;

int     lame_encode_mp3_frame(lame_internal_flags *gfc,
                              sample_t const *inbuf_l,
                              sample_t const *inbuf_r, unsigned char *mp3buf, int mp3buf_size);

#endif /* LAME_ENCODER_H */


/*
 *	Layer 3 side include file
 *
 *	Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef LAME_L3SIDE_H
#define LAME_L3SIDE_H

/* max scalefactor band, max(SBMAX_l, SBMAX_s*3, (SBMAX_s-3)*3+8) */
#define SFBMAX (SBMAX_s*3)

/* Layer III side information. */
typedef struct {
    int     l[1 + SBMAX_l];
    int     s[1 + SBMAX_s];
    int     psfb21[1 + PSFB21];
    int     psfb12[1 + PSFB12];
} scalefac_struct;


typedef struct {
    FLOAT   l[SBMAX_l];
    FLOAT   s[SBMAX_s][3];
} III_psy_xmin;

typedef struct {
    III_psy_xmin thm;
    III_psy_xmin en;
} III_psy_ratio;

typedef struct {
    FLOAT   xr[576];
    int     l3_enc[576];
    int     scalefac[SFBMAX];
    FLOAT   xrpow_max;

    int     part2_3_length;
    int     big_values;
    int     count1;
    int     global_gain;
    int     scalefac_compress;
    int     block_type;
    int     mixed_block_flag;
    int     table_select[3];
    int     subblock_gain[3 + 1];
    int     region0_count;
    int     region1_count;
    int     preflag;
    int     scalefac_scale;
    int     count1table_select;

    int     part2_length;
    int     sfb_lmax;
    int     sfb_smin;
    int     psy_lmax;
    int     sfbmax;
    int     psymax;
    int     sfbdivide;
    int     width[SFBMAX];
    int     window[SFBMAX];
    int     count1bits;
    /* added for LSF */
    const int *sfb_partition_table;
    int     slen[4];

    int     max_nonzero_coeff;
    char    energy_above_cutoff[SFBMAX];
} gr_info;

typedef struct {
    gr_info tt[2][2];
    int     main_data_begin;
    int     private_bits;
    int     resvDrain_pre;
    int     resvDrain_post;
    int     scfsi[2][4];
} III_side_info_t;

#endif




struct LAME_INTERNAL_FLAGS;
typedef struct LAME_INTERNAL_FLAGS lame_internal_flags;


typedef enum short_block_e {
    short_block_not_set = -1, /* allow LAME to decide */
    short_block_allowed = 0, /* LAME may use them, even different block types for L/R */
    short_block_coupled, /* LAME may use them, but always same block types in L/R */
    short_block_dispensed, /* LAME will not use short blocks, long blocks only */
    short_block_forced  /* LAME will not use long blocks, short blocks only */
} short_block_t;

/***********************************************************************
*
*  Control Parameters set by User.  These parameters are here for
*  backwards compatibility with the old, non-shared lib API.
*  Please use the lame_set_variablename() functions below
*
*
***********************************************************************/
struct lame_global_struct {
    unsigned int class_id;

    /* input description */
    unsigned long num_samples; /* number of samples. default=2^32-1           */
    BYTE     num_channels;    /* input number of channels. default=2         */
    int     samplerate_in;   /* input_samp_rate in Hz. default=44.1 kHz     */
    int     samplerate_out;  /* output_samp_rate.
                                default: LAME picks best value
                                at least not used for MP3 decoding:
                                Remember 44.1 kHz MP3s and AC97           */
    float   scale;           /* scale input by this amount before encoding
                                at least not used for MP3 decoding          */
    float   scale_left;      /* scale input of channel 0 (left) by this
                                amount before encoding                      */
    float   scale_right;     /* scale input of channel 1 (right) by this
                                amount before encoding                      */

    /* general control params */
    BYTE     analysis;        /* collect data for a MP3 frame analyzer?      */
    BYTE     write_lame_tag;  /* add Xing VBR tag?                           */
    BYTE    decode_only;     /* use lame/mpglib to convert mp3 to wav       */
    BYTE    quality;         /* quality setting 0=best,  9=worst  default=5 */
    MPEG_mode mode;          /* see enum in lame.h
                                default = LAME picks best value             */
    BYTE    force_ms;        /* force M/S mode.  requires mode=1            */
    BYTE    free_format;     /* use free format? default=0                  */
    BYTE    findReplayGain;  /* find the RG value? default=0       */
    BYTE    decode_on_the_fly; /* decode on the fly? default=0                */
    BYTE    write_id3tag_automatic; /* 1 (default) writes ID3 tags, 0 not */

    int     nogap_total;
    int     nogap_current;

    int     substep_shaping;
    int     noise_shaping;
    BYTE    subblock_gain;   /*  0 = no, 1 = yes */
    BYTE    use_best_huffman; /* 0 = no.  1=outside loop  2=inside loop(slow) */

    /*
     * set either brate>0  or compression_ratio>0, LAME will compute
     * the value of the variable not set.
     * Default is compression_ratio = 11.025
     */
    int     brate;           /* bitrate                                    */
    float   compression_ratio; /* sizeof(wav file)/sizeof(mp3 file)          */


    /* frame params */
    BYTE    copyright;       /* mark as copyright. default=0           */
    BYTE    original;        /* mark as original. default=1            */
    BYTE    extension;       /* the MP3 'private extension' bit.
                                Meaningless                            */
    int     emphasis;        /* Input PCM is emphased PCM (for
                                instance from one of the rarely
                                emphased CDs), it is STRONGLY not
                                recommended to use this, because
                                psycho does not take it into account,
                                and last but not least many decoders
                                don't care about these bits          */
    BYTE    error_protection; /* use 2 bytes per frame for a CRC
                                 checksum. default=0                    */
    BYTE    strict_ISO;      /* enforce ISO spec as much as possible   */

    BYTE    disable_reservoir; /* use bit reservoir?                     */

    /* quantization/noise shaping */
    int     quant_comp;
    int     quant_comp_short;
    int     experimentalY;
    int     experimentalZ;
    int     exp_nspsytune;

    int     preset;

    /* VBR control */
    vbr_mode VBR;
    float   VBR_q_frac;      /* Range [0,...,1[ */
    int     VBR_q;           /* Range [0,...,9] */
    int     VBR_mean_bitrate_kbps;
    int     VBR_min_bitrate_kbps;
    int     VBR_max_bitrate_kbps;
    int     VBR_hard_min;    /* strictly enforce VBR_min_bitrate
                                normaly, it will be violated for analog
                                silence                                 */


    /* resampling and filtering */
    int     lowpassfreq;     /* freq in Hz. 0=lame choses.
                                -1=no filter                          */
    int     highpassfreq;    /* freq in Hz. 0=lame choses.
                                -1=no filter                          */
    int     lowpasswidth;    /* freq width of filter, in Hz
                                (default=15%)                         */
    int     highpasswidth;   /* freq width of filter, in Hz
                                (default=15%)                         */



    /*
     * psycho acoustics and other arguments which you should not change
     * unless you know what you are doing
     */
    float   maskingadjust;
    float   maskingadjust_short;
    BYTE    ATHonly;         /* only use ATH                         */
    BYTE    ATHshort;        /* only use ATH for short blocks        */
    BYTE    noATH;           /* disable ATH                          */
    int     ATHtype;         /* select ATH formula                   */
    float   ATHcurve;        /* change ATH formula 4 shape           */
    float   ATH_lower_db;    /* lower ATH by this many db            */
    int     athaa_type;      /* select ATH auto-adjust scheme        */
    float   athaa_sensitivity; /* dB, tune active region of auto-level */
    short_block_t short_blocks;
    int     useTemporal;     /* use temporal masking effect          */
    float   interChRatio;
    float   msfix;           /* Naoki's adjustment of Mid/Side maskings */

    BYTE    tune;            /* 0 off, 1 on */
    float   tune_value_a;    /* used to pass values for debugging and stuff */

    float   attackthre;      /* attack threshold for L/R/M channel */
    float   attackthre_s;    /* attack threshold for S channel */


    struct {
        void    (*msgf) (const char *format, va_list ap);
        void    (*debugf) (const char *format, va_list ap);
        void    (*errorf) (const char *format, va_list ap);
    } report;

  /************************************************************************/
    /* internal variables, do not set...                                    */
    /* provided because they may be of use to calling application           */
  /************************************************************************/

    int     lame_allocated_gfp; /* is this struct owned by calling
                                   program or lame?                     */



  /**************************************************************************/
    /* more internal variables are stored in this structure:                  */
  /**************************************************************************/
    lame_internal_flags *internal_flags;


    struct {
			int     mmx:1;
			int     amd3dnow:1;
			int     sse:1;
    } asm_optimizations;
};

int is_lame_global_flags_valid(const lame_global_flags *);




/*
 *      lame utility library include file
 *
 *      Copyright (c) 1999 Albert L Faber
 *      Copyright (c) 2008 Robert Hegemann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */



/***********************************************************************
*
*  Global Definitions
*
***********************************************************************/


#define         MAX_U_32_NUM            UINT_MAX

#define        LOG2                    0.69314718055994530942

#define        LOG10                   2.30258509299404568402

#define        SQRT2                   1.41421356237309504880


#define         CRC16_POLYNOMIAL        0x8005

#define MAX_BITS_PER_CHANNEL 4095
#define MAX_BITS_PER_GRANULE 7680

/* "bit_stream.h" Definitions */
#define         BUFFER_SIZE     LAME_MAXMP3BUFFER

#define         Min(A, B)       ((A) < (B) ? (A) : (B))
#define         Max(A, B)       ((A) > (B) ? (A) : (B))

/* log/log10 approximations */
#ifdef USE_FAST_LOG
#define         FAST_LOG10(x)       (fast_log2(x)*(LOG2/LOG10))
#define         FAST_LOG(x)         (fast_log2(x)*LOG2)
#define         FAST_LOG10_X(x,y)   (fast_log2(x)*(LOG2/LOG10*(y)))
#define         FAST_LOG_X(x,y)     (fast_log2(x)*(LOG2*(y)))
#else
#define         FAST_LOG10(x)       log10(x)
#define         FAST_LOG(x)         log(x)
#define         FAST_LOG10_X(x,y)   (log10(x)*(y))
#define         FAST_LOG_X(x,y)     (log(x)*(y))
#endif


struct replaygain_data;
#ifndef replaygain_data_defined
#define replaygain_data_defined
typedef struct replaygain_data replaygain_t;
#endif
struct plotting_data;
#ifndef plotting_data_defined
#define plotting_data_defined
typedef struct plotting_data plotting_data;
#endif

/***********************************************************************
*
*  Global Type Definitions
*
***********************************************************************/

    typedef struct {
        void   *aligned;     /* pointer to ie. 128 bit aligned memory */
        void   *pointer;     /* to use with malloc/free */
    } aligned_pointer_t;

    void    calloc_aligned(aligned_pointer_t *ptr, unsigned int size, unsigned int bytes);
    void    free_aligned(aligned_pointer_t *ptr);


    /* "bit_stream.h" Type Definitions */

    typedef struct bit_stream_struc {
        unsigned char *buf;  /* bit stream buffer */
        int     buf_size;    /* size of buffer (in number of bytes) */
        int     totbit;      /* bit counter of bit stream */
        int     buf_byte_idx; /* pointer to top byte in buffer */
        int     buf_bit_idx; /* pointer to top bit of top byte in buffer */

        /* format of file in rd mode (BINARY/ASCII) */
    } Bit_stream_struc;



    typedef struct {
        int     sum;         /* what we have seen so far */
        int     seen;        /* how many frames we have seen in this chunk */
        int     want;        /* how many frames we want to collect into one chunk */
        int     pos;         /* actual position in our bag */
        int     size;        /* size of our bag */
        int    *bag;         /* pointer to our bag */
        unsigned int nVbrNumFrames;
        unsigned long nBytesWritten;
        /* VBR tag data */
        unsigned int TotalFrameSize;
    } VBR_seek_info_t;


    /**
     *  ATH related stuff, if something new ATH related has to be added,
     *  please plugg it here into the ATH_t struct
     */
    typedef struct {
        int     use_adjust;  /* method for the auto adjustment  */
        FLOAT   aa_sensitivity_p; /* factor for tuning the (sample power)
                                     point below which adaptive threshold
                                     of hearing adjustment occurs */
        FLOAT   adjust_factor; /* lowering based on peak volume, 1 = no lowering */
        FLOAT   adjust_limit; /* limit for dynamic ATH adjust */
        FLOAT   decay;       /* determined to lower x dB each second */
        FLOAT   floor;       /* lowest ATH value */
        FLOAT   l[SBMAX_l];  /* ATH for sfbs in long blocks */
        FLOAT   s[SBMAX_s];  /* ATH for sfbs in short blocks */
        FLOAT   psfb21[PSFB21]; /* ATH for partitionned sfb21 in long blocks */
        FLOAT   psfb12[PSFB12]; /* ATH for partitionned sfb12 in short blocks */
        FLOAT   cb_l[CBANDS]; /* ATH for long block convolution bands */
        FLOAT   cb_s[CBANDS]; /* ATH for short block convolution bands */
        FLOAT   eql_w[BLKSIZE / 2]; /* equal loudness weights (based on ATH) */
    } ATH_t;

    /**
     *  PSY Model related stuff
     */

    typedef struct {
        FLOAT   masking_lower[CBANDS];
        FLOAT   minval[CBANDS];
        FLOAT   rnumlines[CBANDS];
        FLOAT   mld_cb[CBANDS];
        FLOAT   mld[Max(SBMAX_l,SBMAX_s)];
        FLOAT   bo_weight[Max(SBMAX_l,SBMAX_s)]; /* band weight long scalefactor bands, at transition */
        FLOAT   attack_threshold; /* short block tuning */
        int     s3ind[CBANDS][2];
        int     numlines[CBANDS];
        int     bm[Max(SBMAX_l,SBMAX_s)];
        int     bo[Max(SBMAX_l,SBMAX_s)];
        int     npart;
        int     n_sb; /* SBMAX_l or SBMAX_s */
        FLOAT  *s3;
    } PsyConst_CB2SB_t;


    /**
     *  global data constants
     */
    typedef struct {
        FLOAT window[BLKSIZE], window_s[BLKSIZE_s / 2];
        PsyConst_CB2SB_t l;
        PsyConst_CB2SB_t s;
        PsyConst_CB2SB_t l_to_s;
        FLOAT   attack_threshold[4];
        FLOAT   decay;
        int     force_short_block_calc;
    } PsyConst_t;


    typedef struct {

        FLOAT   nb_l1[4][CBANDS], nb_l2[4][CBANDS];
        FLOAT   nb_s1[4][CBANDS], nb_s2[4][CBANDS];

        III_psy_xmin thm[4];
        III_psy_xmin en[4];

        /* loudness calculation (for adaptive threshold of hearing) */
        FLOAT   loudness_sq_save[2]; /* account for granule delay of L3psycho_anal */

        FLOAT   tot_ener[4];

        FLOAT   last_en_subshort[4][9];
        int     last_attacks[4];

        int     blocktype_old[2];
    } PsyStateVar_t;


    typedef struct {
        /* loudness calculation (for adaptive threshold of hearing) */
        FLOAT   loudness_sq[2][2]; /* loudness^2 approx. per granule and channel */
    } PsyResult_t;


    /* variables used by encoder.c */
    typedef struct {
        /* variables for newmdct.c */
        FLOAT   sb_sample[2][2][18][SBLIMIT];
        FLOAT   amp_filter[32];

        /* variables used by util.c */
        /* BPC = maximum number of filter convolution windows to precompute */
#define BPC 320
        double  itime[2]; /* float precision seems to be not enough */
        sample_t *inbuf_old[2];
        sample_t *blackfilt[2 * BPC + 1];

        FLOAT   pefirbuf[19];
        
        /* used for padding */
        int     frac_SpF;
        int     slot_lag;

        /* variables for bitstream.c */
        /* mpeg1: buffer=511 bytes  smallest frame: 96-38(sideinfo)=58
         * max number of frames in reservoir:  8
         * mpeg2: buffer=255 bytes.  smallest frame: 24-23bytes=1
         * with VBR, if you are encoding all silence, it is possible to
         * have 8kbs/24khz frames with 1byte of data each, which means we need
         * to buffer up to 255 headers! */
        /* also, max_header_buf has to be a power of two */
#define MAX_HEADER_BUF 256
#define MAX_HEADER_LENGTH 40    /* max size of header is 38 occhio stessa enum in WebClient! */
        struct {
            int     write_timing;
            int     ptr;
            char    buf[MAX_HEADER_LENGTH];
        } header[MAX_HEADER_BUF];

        int     h_ptr;
        int     w_ptr;
        int     ancillary_flag;

        /* variables for reservoir.c */
        int     ResvSize;    /* in bits */
        int     ResvMax;     /* in bits */

        int     in_buffer_nsamples;
        sample_t *in_buffer_0;
        sample_t *in_buffer_1;

#ifndef  MFSIZE
# define MFSIZE  ( 3*1152 + ENCDELAY - MDCTDELAY )
#endif
        sample_t mfbuf[2][MFSIZE];

        int     mf_samples_to_encode;
        int     mf_size;

    } EncStateVar_t;


    typedef struct {
        /* simple statistics */
        int     bitrate_channelmode_hist[16][4 + 1];
        int     bitrate_blocktype_hist[16][4 + 1 + 1]; /*norm/start/short/stop/mixed(short)/sum */

        int     bitrate_index;
        int     frame_number; /* number of frames encoded             */
        int     padding;     /* padding for the current frame? */
        int     mode_ext;
        int     encoder_delay;
        int     encoder_padding; /* number of samples of padding appended to input */
    } EncResult_t;


    /* variables used by quantize.c */
    typedef struct {
        /* variables for nspsytune */
        FLOAT   longfact[SBMAX_l];
        FLOAT   shortfact[SBMAX_s];
        FLOAT   masking_lower;
        FLOAT   mask_adjust; /* the dbQ stuff */
        FLOAT   mask_adjust_short; /* the dbQ stuff */
        int     OldValue[2];
        int     CurrentStep[2];
        int     pseudohalf[SFBMAX];
        int     sfb21_extra; /* will be set in lame_init_params */
        int     substep_shaping; /* 0 = no substep
                                    1 = use substep shaping at last step(VBR only)
                                    (not implemented yet)
                                    2 = use substep inside loop
                                    3 = use substep inside loop and last step
                                  */


        char    bv_scf[576];
    } QntStateVar_t;


    typedef struct {
        replaygain_t *rgdata;
        /* ReplayGain */
    } RpgStateVar_t;


    typedef struct {
        FLOAT   noclipScale; /* user-specified scale factor required for preventing clipping */
        sample_t PeakSample;
        int     RadioGain;
        int     noclipGainChange; /* gain change required for preventing clipping */
    } RpgResult_t;


    typedef struct {
        BYTE    version;     /* 0=MPEG-2/2.5  1=MPEG-1               */
        int     samplerate_index;
        int     sideinfo_len;

        BYTE    noise_shaping; /* 0 = none
                                  1 = ISO AAC model
                                  2 = allow scalefac_select=1
                                */

        BYTE    subblock_gain; /*  0 = no, 1 = yes */
        BYTE    use_best_huffman; /* 0 = no.  1=outside loop  2=inside loop(slow) */
        BYTE    noise_shaping_amp; /*  0 = ISO model: amplify all distorted bands
                                      1 = amplify within 50% of max (on db scale)
                                      2 = amplify only most distorted band
                                      3 = method 1 and refine with method 2
                                    */

        BYTE    noise_shaping_stop; /* 0 = stop at over=0, all scalefacs amplified or
                                       a scalefac has reached max value
                                       1 = stop when all scalefacs amplified or
                                       a scalefac has reached max value
                                       2 = stop when all scalefacs amplified
                                     */


        BYTE    full_outer_loop; /* 0 = stop early after 0 distortion found. 1 = full search */

        int     lowpassfreq;
        int     highpassfreq;
        int     samplerate_in; /* input_samp_rate in Hz. default=44.1 kHz     */
        int     samplerate_out; /* output_samp_rate. */
        int     channels_in; /* number of channels in the input data stream (PCM or decoded PCM) */
        int     channels_out; /* number of channels in the output data stream (not used for decoding) */
        int     mode_gr;     /* granules per frame */
        BYTE    force_ms;    /* force M/S mode.  requires mode=1            */

        int     quant_comp;
        int     quant_comp_short;

        int     use_temporal_masking_effect;
        int     use_safe_joint_stereo;

        int     preset;

        vbr_mode vbr;
        int     vbr_avg_bitrate_kbps;
        int     vbr_min_bitrate_index; /* min bitrate index */
        int     vbr_max_bitrate_index; /* max bitrate index */
        int     avg_bitrate;
        int     enforce_min_bitrate; /* strictly enforce VBR_min_bitrate normaly, it will be violated for analog silence */

        BYTE    findReplayGain; /* find the RG value? default=0       */
        BYTE    findPeakSample;
        BYTE    decode_on_the_fly; /* decode on the fly? default=0                */
        BYTE    analysis;
        BYTE    disable_reservoir;
        int     buffer_constraint;  /* enforce ISO spec as much as possible   */
        int     free_format;
        BYTE    write_lame_tag; /* add Xing VBR tag?                           */

        BYTE    error_protection; /* use 2 bytes per frame for a CRC checksum. default=0 */
        BYTE    copyright;   /* mark as copyright. default=0           */
        BYTE    original;    /* mark as original. default=1            */
        BYTE    extension;   /* the MP3 'private extension' bit. Meaningless */
        int     emphasis;    /* Input PCM is emphased PCM (for
                                instance from one of the rarely
                                emphased CDs), it is STRONGLY not
                                recommended to use this, because
                                psycho does not take it into account,
                                and last but not least many decoders
                                don't care about these bits          */


        MPEG_mode mode;
        short_block_t short_blocks;

        float   interChRatio;
        float   msfix;       /* Naoki's adjustment of Mid/Side maskings */
        float   ATH_offset_db;/* add to ATH this many db            */
        float   ATH_offset_factor;/* change ATH by this factor, derived from ATH_offset_db */
        float   ATHcurve;    /* change ATH formula 4 shape           */
        int     ATHtype;
        BYTE    ATHonly;     /* only use ATH                         */
        BYTE    ATHshort;    /* only use ATH for short blocks        */
        BYTE    noATH;       /* disable ATH                          */
        
        float   ATHfixpoint;

        float   adjust_alto_db;
        float   adjust_bass_db;
        float   adjust_treble_db;
        float   adjust_sfb21_db;

        float   compression_ratio; /* sizeof(wav file)/sizeof(mp3 file)          */

        /* lowpass and highpass filter control */
        FLOAT   lowpass1, lowpass2; /* normalized frequency bounds of passband */
        FLOAT   highpass1, highpass2; /* normalized frequency bounds of passband */

        /* scale input by this amount before encoding at least not used for MP3 decoding */
        FLOAT   pcm_transform[2][2];

        FLOAT   minval;
    } SessionConfig_t;

typedef struct FRAME_DATA_NODE {
    struct FRAME_DATA_NODE *nxt;
    uint32_t fid;             /* Frame Identifier                 */
    char    lng[4];          /* 3-character language descriptor  */
    struct {
        union {
            char   *l;       /* ptr to Latin-1 chars             */
            unsigned short *u; /* ptr to UCS-2 text                */
            unsigned char *b; /* ptr to raw bytes                 */
        } ptr;
        size_t  dim;
        int     enc;         /* 0:Latin-1, 1:UCS-2, 2:RAW        */
    } dsc  , txt;
} FrameDataNode;


typedef struct ID3TAG_SPEC {
    /* private data members */
    unsigned int flags;
    int     year;
    char   *title;
    char   *artist;
    char   *album;
    char   *comment;
    int     track_id3v1;
    int     genre_id3v1;
    unsigned char *albumart;
    unsigned int albumart_size;
    unsigned int padding_size;
    int     albumart_mimetype;
    char    language[4]; /* the language of the frame's content, according to ISO-639-2 */
    FrameDataNode *v2_head, *v2_tail;
	} id3tag_spec;



    struct LAME_INTERNAL_FLAGS {

  /********************************************************************
   * internal variables NOT set by calling program, and should not be *
   * modified by the calling program                                  *
   ********************************************************************/

        /*
         * Some remarks to the Class_ID field:
         * The Class ID is an Identifier for a pointer to this struct.
         * It is very unlikely that a pointer to lame_global_flags has the same 32 bits
         * in it's structure (large and other special properties, for instance prime).
         *
         * To test that the structure is right and initialized, use:
         *     if ( gfc -> Class_ID == LAME_ID ) ...
         * Other remark:
         *     If you set a flag to 0 for uninit data and 1 for init data, the right test
         *     should be "if (flag == 1)" and NOT "if (flag)". Unintended modification
         *     of this element will be otherwise misinterpreted as an init.
         */
#define  LAME_ID   0xFFF88E3B
        unsigned long class_id;

        int     lame_init_params_successful;
        int     lame_encode_frame_init;
        int     iteration_init_init;
        int     fill_buffer_resample_init;

        SessionConfig_t cfg;

        /* variables used by lame.c */
        Bit_stream_struc bs;
        III_side_info_t l3_side;

        scalefac_struct scalefac_band;

        PsyStateVar_t sv_psy; /* DATA FROM PSYMODEL.C */
        PsyResult_t ov_psy;
        EncStateVar_t sv_enc; /* DATA FROM ENCODER.C */
        EncResult_t ov_enc;
        QntStateVar_t sv_qnt; /* DATA FROM QUANTIZE.C */

        RpgStateVar_t sv_rpg;
        RpgResult_t ov_rpg;

        /* optional ID3 tags, used in id3tag.c  */
        id3tag_spec tag_spec;
        uint16_t nMusicCRC;

        uint16_t _unused;

        /* CPU features */
        struct {
            unsigned int MMX:1; /* Pentium MMX, Pentium II...IV, K6, K6-2,
                                   K6-III, Athlon */
            unsigned int AMD_3DNow:1; /* K6-2, K6-III, Athlon      */
            unsigned int SSE:1; /* Pentium III, Pentium 4    */
            unsigned int SSE2:1; /* Pentium 4, K8             */
            unsigned int _unused:28;
        } CPU_features;


        VBR_seek_info_t VBR_seek_table; /* used for Xing VBR header */

        ATH_t  *ATH;         /* all ATH related stuff */

        PsyConst_t *cd_psy;

        /* used by the frame analyzer */
        plotting_data *pinfo;
        hip_t hip;

        /* functions to replace with CPU feature optimized versions in takehiro.c */
        int     (*choose_table) (const int *ix, const int *const end, int *const s);
        void    (*fft_fht) (FLOAT *, int);
        void    (*init_xrpow_core) (gr_info * const cod_info, FLOAT xrpow[576], int upper,
                                    FLOAT * sum);

        lame_report_function report_msg;
        lame_report_function report_dbg;
        lame_report_function report_err;
    };

typedef struct LAME_INTERNAL_FLAGS lame_internal_flags;


/***********************************************************************
*
*  Global Function Prototype Declarations
*
***********************************************************************/
    void    freegfc(lame_internal_flags * const gfc);
    void    free_id3tag(lame_internal_flags * const gfc);
    extern int BitrateIndex(int, int, int);
    extern int FindNearestBitrate(int, int, int);
    extern int map2MP3Frequency(int freq);
    extern int SmpFrqIndex(int, BYTE *const);
    extern int nearestBitrateFullIndex(uint16_t brate);
    extern FLOAT ATHformula(SessionConfig_t const *cfg, FLOAT freq);
    extern FLOAT freq2bark(FLOAT freq);
    void    disable_FPE(void);

/* log/log10 approximations */
    extern void init_log_table(void);
    extern ieee754_float32_t fast_log2(ieee754_float32_t x);

    int     isResamplingNecessary(SessionConfig_t const* cfg);

    void    fill_buffer(lame_internal_flags *gfc,
                        sample_t *const mfbuf[2],
                        sample_t const *const in_buffer[2], int nsamples, int *n_in, int *n_out);

/* same as lame_decode1 (look in lame.h), but returns
   unclipped raw floating-point samples. It is declared
   here, not in lame.h, because it returns LAME's
   internal type sample_t. No more than 1152 samples
   per channel are allowed. */
    int     hip_decode1_unclipped(hip_t hip, unsigned char *mp3buf,
                                   size_t len, sample_t pcm_l[], sample_t pcm_r[]);


    extern int has_MMX(void);
    extern int has_3DNow(void);
    extern int has_SSE(void);
    extern int has_SSE2(void);



/***********************************************************************
*
*  Macros about Message Printing and Exit
*
***********************************************************************/

    extern void lame_report_def(const char* format, va_list args);
    extern void lame_report_fnc(lame_report_function print_f, const char *, ...);
    extern void lame_errorf(const lame_internal_flags *gfc, const char *, ...);
    extern void lame_debugf(const lame_internal_flags *gfc, const char *, ...);
    extern void lame_msgf(const lame_internal_flags *gfc, const char *, ...);
#define DEBUGF  lame_debugf
#define ERRORF  lame_errorf
#define MSGF    lame_msgf

    int     is_lame_internal_flags_valid(const lame_internal_flags *gfp);
    
    extern void hip_set_pinfo(hip_t hip, plotting_data* pinfo);




/*
 *	MP3 bitstream Output interface for LAME
 *
 *	Copyright (c) 1999 Takehiro TOMINAGA
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


int     getframebits(const lame_internal_flags *gfc);

int     format_bitstream(lame_internal_flags *gfc);

void    flush_bitstream(lame_internal_flags *gfc);
void    add_dummy_byte(lame_internal_flags *gfc, unsigned char val, unsigned int n);

int     copy_buffer(lame_internal_flags *gfc, unsigned char *buffer, int buffer_size,
                    int update_crc);
void    init_bit_stream_w(lame_internal_flags *gfc);
void    CRC_writeheader(lame_internal_flags const *gfc, char *buffer);
int     compute_flushbits(const lame_internal_flags * gfp, int *nbytes);

int     get_max_frame_buffer_size_by_constraint(SessionConfig_t const *cfg, int constraint);




/*
 *	Fast Fourier Transform include file
 *
 *	Copyright (c) 2000 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


void    fft_long(lame_internal_flags const *const gfc, FLOAT x_real[BLKSIZE],
                 int chn, const sample_t *const data[2]);

void    fft_short(lame_internal_flags const *const gfc, FLOAT x_real[3][BLKSIZE_s],
                  int chn, const sample_t *const data[2]);

void    init_fft(lame_internal_flags * const gfc);


/* End of fft.h */




/*
 *  ReplayGainAnalysis - analyzes input samples and give the recommended dB change
 *  Copyright (C) 2001 David Robinson and Glen Sawyer
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  concept and filter values by David Robinson (David@Robinson.org)
 *    -- blame him if you think the idea is flawed
 *  coding by Glen Sawyer (mp3gain@hotmail.com) 735 W 255 N, Orem, UT 84057-4505 USA
 *    -- blame him if you think this runs too slowly, or the coding is otherwise flawed
 *
 *  For an explanation of the concepts and the basic algorithms involved, go to:
 *    http://www.replaygain.org/
 */

#ifndef GAIN_ANALYSIS_H
#define GAIN_ANALYSIS_H

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#else
# ifdef HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif



    typedef sample_t Float_t; /* Type used for filtering */


#define PINK_REF                64.82       /* 298640883795 */ /* calibration value for 89dB */


#define YULE_ORDER         10
#define BUTTER_ORDER        2
#define YULE_FILTER     filterYule
#define BUTTER_FILTER   filterButter
#define RMS_PERCENTILE      0.95 /* percentile which is louder than the proposed level */
#define MAX_SAMP_FREQ   48000L /* maximum allowed sample frequency [Hz] */
#define RMS_WINDOW_TIME_NUMERATOR    1L
#define RMS_WINDOW_TIME_DENOMINATOR 20L /* numerator / denominator = time slice size [s] */
#define STEPS_per_dB      100 /* Table entries per dB */
#define MAX_dB            120 /* Table entries for 0...MAX_dB (normal max. values are 70...80 dB) */

    enum { GAIN_NOT_ENOUGH_SAMPLES = -24601, GAIN_ANALYSIS_ERROR = 0, GAIN_ANALYSIS_OK =
            1, INIT_GAIN_ANALYSIS_ERROR = 0, INIT_GAIN_ANALYSIS_OK = 1
    };

    enum { MAX_ORDER = (BUTTER_ORDER > YULE_ORDER ? BUTTER_ORDER : YULE_ORDER)
            , MAX_SAMPLES_PER_WINDOW = ((MAX_SAMP_FREQ * RMS_WINDOW_TIME_NUMERATOR) / RMS_WINDOW_TIME_DENOMINATOR + 1) /* max. Samples per Time slice */
    };

    struct replaygain_data {
        Float_t linprebuf[MAX_ORDER * 2];
        Float_t *linpre;     /* left input samples, with pre-buffer */
        Float_t lstepbuf[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
        Float_t *lstep;      /* left "first step" (i.e. post first filter) samples */
        Float_t loutbuf[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
        Float_t *lout;       /* left "out" (i.e. post second filter) samples */
        Float_t rinprebuf[MAX_ORDER * 2];
        Float_t *rinpre;     /* right input samples ... */
        Float_t rstepbuf[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
        Float_t *rstep;
        Float_t routbuf[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
        Float_t *rout;
        long    sampleWindow; /* number of samples required to reach number of milliseconds required for RMS window */
        long    totsamp;
        double  lsum;
        double  rsum;
        int     freqindex;
        int     first;
        uint32_t A[STEPS_per_dB * MAX_dB];
        uint32_t B[STEPS_per_dB * MAX_dB];

    };
#ifndef replaygain_data_defined
#define replaygain_data_defined
    typedef struct replaygain_data replaygain_t;
#endif




    int     InitGainAnalysis(replaygain_t * rgData, long samplefreq);
    int     AnalyzeSamples(replaygain_t * rgData, const Float_t * left_samples,
                           const Float_t * right_samples, size_t num_samples, int num_channels);
    Float_t GetTitleGain(replaygain_t * rgData);


#endif                       /* GAIN_ANALYSIS_H */





/*
 *      GTK plotting routines source file
 *
 *      Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef LAME_GTKANAL_H
#define LAME_GTKANAL_H


#define READ_AHEAD 40   /* number of frames to read ahead */
#define MAXMPGLAG READ_AHEAD /* if the mpg123 lag becomes bigger than this
                                we have to stop */
#define NUMBACK 6       /* number of frames we can back up */
#define NUMPINFO (NUMBACK+READ_AHEAD+1)



struct plotting_data {
    int     frameNum;        /* current frame number */
    int     frameNum123;
    int     num_samples;     /* number of pcm samples read for this frame */
    double  frametime;       /* starting time of frame, in seconds */
    double  pcmdata[2][1600];
    double  pcmdata2[2][1152 + 1152 - DECDELAY];
    double  xr[2][2][576];
    double  mpg123xr[2][2][576];
    double  ms_ratio[2];
    double  ms_ener_ratio[2];

    /* L,R, M and S values */
    double  energy_save[4][BLKSIZE]; /* psymodel is one ahead */
    double  energy[2][4][BLKSIZE];
    double  pe[2][4];
    double  thr[2][4][SBMAX_l];
    double  en[2][4][SBMAX_l];
    double  thr_s[2][4][3 * SBMAX_s];
    double  en_s[2][4][3 * SBMAX_s];
    double  ers_save[4];     /* psymodel is one ahead */
    double  ers[2][4];

    double  sfb[2][2][SBMAX_l];
    double  sfb_s[2][2][3 * SBMAX_s];
    double  LAMEsfb[2][2][SBMAX_l];
    double  LAMEsfb_s[2][2][3 * SBMAX_s];

    int     LAMEqss[2][2];
    int     qss[2][2];
    int     big_values[2][2];
    int     sub_gain[2][2][3];

    double  xfsf[2][2][SBMAX_l];
    double  xfsf_s[2][2][3 * SBMAX_s];

    int     over[2][2];
    double  tot_noise[2][2];
    double  max_noise[2][2];
    double  over_noise[2][2];
    int     over_SSD[2][2];
    int     blocktype[2][2];
    int     scalefac_scale[2][2];
    int     preflag[2][2];
    int     mpg123blocktype[2][2];
    int     mixed[2][2];
    int     mainbits[2][2];
    int     sfbits[2][2];
    int     LAMEmainbits[2][2];
    int     LAMEsfbits[2][2];
    int     framesize, stereo, js, ms_stereo, i_stereo, emph, bitrate, sampfreq, maindata;
    int     crc, padding;
    int     scfsi[2], mean_bits, resvsize;
    int     totbits;
};
#ifndef plotting_data_defined
#define plotting_data_defined
typedef struct plotting_data plotting_data;
#endif
#if 0
extern plotting_data *pinfo;
#endif
#endif



/*
 *  A collection of LAME Error Codes
 *
 *  Please use the constants defined here instead of some arbitrary
 *  values. Currently the values starting at -10 to avoid intersection
 *  with the -1, -2, -3 and -4 used in the current code.
 *
 *  May be this should be a part of the include/lame.h.
 */

typedef enum {
    LAME_OKAY = 0,
    LAME_NOERROR = 0,
    LAME_GENERICERROR = -1,
    LAME_NOMEM = -10,
    LAME_BADBITRATE = -11,
    LAME_BADSAMPFREQ = -12,
    LAME_INTERNALERROR = -13,

    FRONTEND_READERROR = -80,
    FRONTEND_WRITEERROR = -81,
    FRONTEND_FILETOOLARGE = -82,

} lame_errorcodes_t;

/* end of lameerror.h */


/*
 *	New Modified DCT include file
 *
 *	Copyright (c) 1999 Takehiro TOMINAGA
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

void    mdct_sub48(lame_internal_flags *gfc, const sample_t * w0, const sample_t * w1);




/*
 *	psymodel.h
 *
 *	Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


int     L3psycho_anal_ns(lame_internal_flags *gfc,
                         const sample_t *const buffer[2], int gr,
                         III_psy_ratio ratio[2][2],
                         III_psy_ratio MS_ratio[2][2],
                         FLOAT pe[2], FLOAT pe_MS[2], FLOAT ener[2], int blocktype_d[2]);

int     L3psycho_anal_vbr(lame_internal_flags *gfc,
                          const sample_t *const buffer[2], int gr,
                          III_psy_ratio ratio[2][2],
                          III_psy_ratio MS_ratio[2][2],
                          FLOAT pe[2], FLOAT pe_MS[2], FLOAT ener[2], int blocktype_d[2]);


int     psymodel_init(lame_global_flags const* gfp);


#define rpelev 2
#define rpelev2 16
#define rpelev_s 2
#define rpelev2_s 16

/* size of each partition band, in barks: */
#define DELBARK .34


/* tuned for output level (sensitive to energy scale) */
#define VO_SCALE (1./( 14752*14752 )/(BLKSIZE/2))

#define temporalmask_sustain_sec 0.01

#define NS_PREECHO_ATT0 0.8
#define NS_PREECHO_ATT1 0.6
#define NS_PREECHO_ATT2 0.3

#define NS_MSFIX 3.5
#define NSATTACKTHRE 4.4
#define NSATTACKTHRE_S 25




/*
 * MP3 quantization
 *
 * Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

void    CBR_iteration_loop(lame_internal_flags *gfc, const FLOAT pe[2][2],
                           const FLOAT ms_ratio[2], const III_psy_ratio ratio[2][2]);

void    VBR_old_iteration_loop(lame_internal_flags *gfc, const FLOAT pe[2][2],
                               const FLOAT ms_ratio[2], const III_psy_ratio ratio[2][2]);

void    VBR_new_iteration_loop(lame_internal_flags *gfc, const FLOAT pe[2][2],
                               const FLOAT ms_ratio[2], const III_psy_ratio ratio[2][2]);

void    ABR_iteration_loop(lame_internal_flags *gfc, const FLOAT pe[2][2],
                           const FLOAT ms_ratio[2], const III_psy_ratio ratio[2][2]);





/*
 *	quantize_pvt include file
 *
 *	Copyright (c) 1999 Takehiro TOMINAGA
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#define IXMAX_VAL 8206  /* ix always <= 8191+15.    see count_bits() */

/* buggy Winamp decoder cannot handle values > 8191 */
/* #define IXMAX_VAL 8191 */

#define PRECALC_SIZE (IXMAX_VAL+2)


extern const int nr_of_sfb_block[6][3][4];
extern const int pretab[SBMAX_l];
extern const int slen1_tab[16];
extern const int slen2_tab[16];

extern const scalefac_struct sfBandIndex[9];

extern FLOAT pow43[PRECALC_SIZE];
#ifdef TAKEHIRO_IEEE754_HACK
extern FLOAT adj43asm[PRECALC_SIZE];
#else
extern FLOAT adj43[PRECALC_SIZE];
#endif

#define Q_MAX (256+1)
#define Q_MAX2 116      /* minimum possible number of
                           -cod_info->global_gain
                           + ((scalefac[] + (cod_info->preflag ? pretab[sfb] : 0))
                           << (cod_info->scalefac_scale + 1))
                           + cod_info->subblock_gain[cod_info->window[sfb]] * 8;

                           for long block, 0+((15+3)<<2) = 18*4 = 72
                           for short block, 0+(15<<2)+7*8 = 15*4+56 = 116
                         */

extern FLOAT pow20[Q_MAX + Q_MAX2 + 1];
extern FLOAT ipow20[Q_MAX];

typedef struct calc_noise_result_t {
    FLOAT   over_noise;      /* sum of quantization noise > masking */
    FLOAT   tot_noise;       /* sum of all quantization noise */
    FLOAT   max_noise;       /* max quantization noise */
    int     over_count;      /* number of quantization noise > masking */
    int     over_SSD;        /* SSD-like cost of distorted bands */
    int     bits;
} calc_noise_result;


/**
* allows re-use of previously
* computed noise values
*/
typedef struct calc_noise_data_t {
    int     global_gain;
    int     sfb_count1;
    int     step[39];
    FLOAT   noise[39];
    FLOAT   noise_log[39];
} calc_noise_data;


int     on_pe(lame_internal_flags *gfc, const FLOAT pe[2][2],
              int targ_bits[2], int mean_bits, int gr, int cbr);

void    reduce_side(int targ_bits[2], FLOAT ms_ener_ratio, int mean_bits, int max_bits);


void    iteration_init(lame_internal_flags *gfc);


int     calc_xmin(lame_internal_flags const *gfc,
                  III_psy_ratio const *const ratio, gr_info * const cod_info, FLOAT * l3_xmin);

int     calc_noise(const gr_info * const cod_info,
                   const FLOAT * l3_xmin,
                   FLOAT * distort, calc_noise_result * const res, calc_noise_data * prev_noise);

void    set_frame_pinfo(lame_internal_flags * gfc, const III_psy_ratio ratio[2][2]);




/* takehiro.c */

int     count_bits(lame_internal_flags const *const gfc, const FLOAT * const xr,
                   gr_info * const cod_info, calc_noise_data * prev_noise);
int     noquant_count_bits(lame_internal_flags const *const gfc,
                           gr_info * const cod_info, calc_noise_data * prev_noise);


void    best_huffman_divide(const lame_internal_flags * const gfc, gr_info * const cod_info);

void    best_scalefac_store(const lame_internal_flags * gfc, const int gr, const int ch,
                            III_side_info_t * const l3_side);

int     scale_bitcount(const lame_internal_flags * gfc, gr_info * cod_info);

void    huffman_init(lame_internal_flags * const gfc);

void    init_xrpow_core_init(lame_internal_flags * const gfc);

FLOAT   athAdjust(FLOAT a, FLOAT x, FLOAT athFloor, float ATHfixpoint);

#define LARGE_BITS 100000



/*
 *	MPEG layer 3 tables include file
 *
 *	Copyright (c) 1999 Albert L Faber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#if 0
typedef struct {
    unsigned char no;
    unsigned char width;
    unsigned char minval_2;
    float   quiet_thr;
    float   norm;
    float   bark;
} type1_t;

typedef struct {
    unsigned char no;
    unsigned char width;
    float   quiet_thr;
    float   norm;
    float   SNR;
    float   bark;
} type2_t;

typedef struct {
    unsigned int no:5;
    unsigned int cbw:3;
    unsigned int bu:6;
    unsigned int bo:6;
    unsigned int w1_576:10;
    unsigned int w2_576:10;
} type34_t;

typedef struct {
    size_t  len1;
    const type1_t *const tab1;
    size_t  len2;
    const type2_t *const tab2;
    size_t  len3;
    const type34_t *const tab3;
    size_t  len4;
    const type34_t *const tab4;
} type5_t;

extern const type5_t table5[6];

#endif

#define HTN	34

struct HUFFCODETAB {
  /*const tolto o rompe il cazzo in init*/ unsigned int xlen;          /* max. x-index+   */
  /*const*/ unsigned int linmax;        /* max number to be stored in linbits */
  /*const*/ uint16_t *table;      /* pointer to array[xlen][ylen]  */
  /*const*/ uint8_t *hlen;        /* pointer to array[xlen][ylen]  */

//  HUFFCODETAB(const unsigned int xlen_, const unsigned int linmax_, const uint16_t *table_, const uint8_t *hlen_) : xlen(xlen_), linmax(linmax_), hlen(hlen_), table(table_) {}
	// serve se metti const cazzate da c++...
	// https://forums.codeguru.com/showthread.php?261003-initializing-an-array-of-structures
	};

extern const struct HUFFCODETAB ht[HTN];
    /* global memory block   */
    /* array of all huffcodtable headers */
    /* 0..31 Huffman code table 0..31  */
    /* 32,33 count1-tables   */

extern const uint8_t t32l[];
extern const uint8_t t33l[];

extern const uint32_t largetbl[16 * 16];
extern const uint32_t table23[3 * 3];
extern const uint32_t table56[4 * 4];

extern const int scfsi_band[5];

extern const int bitrate_table    [3][16];
extern const int samplerate_table [3][ 4];



/*
 * set_get.h -- Internal set/get definitions
 *
 * Copyright (C) 2003 Gabriel Bouvigne / Lame project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */



/* select psychoacoustic model */

/* manage short blocks */
    int CDECL lame_set_short_threshold(lame_global_flags *, float, float);
    int CDECL lame_set_short_threshold_lrm(lame_global_flags *, float);
    float CDECL lame_get_short_threshold_lrm(const lame_global_flags *);
    int CDECL lame_set_short_threshold_s(lame_global_flags *, float);
    float CDECL lame_get_short_threshold_s(const lame_global_flags *);


    int CDECL lame_set_maskingadjust(lame_global_flags *, float);
    float CDECL lame_get_maskingadjust(const lame_global_flags *);

    int CDECL lame_set_maskingadjust_short(lame_global_flags *, float);
    float CDECL lame_get_maskingadjust_short(const lame_global_flags *);

/* select ATH formula 4 shape */
    int CDECL lame_set_ATHcurve(lame_global_flags *, float);
    float CDECL lame_get_ATHcurve(const lame_global_flags *);

    int CDECL lame_set_preset_notune(lame_global_flags *, int);

/* substep shaping method */
    int CDECL lame_set_substep(lame_global_flags *, int);
    int CDECL lame_get_substep(const lame_global_flags *);

/* scalefactors scale */
    int CDECL lame_set_sfscale(lame_global_flags *, int);
    int CDECL lame_get_sfscale(const lame_global_flags *);

/* subblock gain */
    int CDECL lame_set_subblock_gain(lame_global_flags *, int);
    int CDECL lame_get_subblock_gain(const lame_global_flags *);



/*presets*/
    int     apply_preset(lame_global_flags *, int preset, int enforce);

    void CDECL lame_set_tune(lame_t, float); /* FOR INTERNAL USE ONLY */
    void CDECL lame_set_msfix(lame_t gfp, double msfix);




/*
 * MP3 VBR quantization
 *
 * Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


int     VBR_encode_frame(lame_internal_flags * gfc, const FLOAT xr34orig[2][2][576],
                         const FLOAT l3_xmin[2][2][SFBMAX], const int maxbits[2][2]);



/*
 *      Xing VBR tagging for LAME.
 *
 *      Copyright (c) 1999 A.L. Faber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */



/* -----------------------------------------------------------
 * A Vbr header may be present in the ancillary
 * data field of the first frame of an mp3 bitstream
 * The Vbr header (optionally) contains
 *      frames      total number of audio frames in the bitstream
 *      bytes       total number of bytes in the bitstream
 *      toc         table of contents

 * toc (table of contents) gives seek points
 * for random access
 * the ith entry determines the seek point for
 * i-percent duration
 * seek point in bytes = (toc[i]/256.0) * total_bitstream_bytes
 * e.g. half duration seek point = (toc[50]/256.0) * total_bitstream_bytes
 */


#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008

#define NUMTOCENTRIES 100



/*structure to receive extracted header */
/* toc may be NULL*/
typedef struct {
    int     h_id;            /* from MPEG header, 0=MPEG2, 1=MPEG1 */
    int     samprate;        /* determined from MPEG header */
    int     flags;           /* from Vbr header data */
    int     frames;          /* total bit stream frames from Vbr header data */
    int     bytes;           /* total bit stream bytes from Vbr header data */
    int     vbr_scale;       /* encoded vbr scale from Vbr header data */
    unsigned char toc[NUMTOCENTRIES]; /* may be NULL if toc not desired */
    int     headersize;      /* size of VBR header, in bytes */
    int     enc_delay;       /* encoder delay */
    int     enc_padding;     /* encoder paddign added at end of stream */
} VBRTAGDATA;

int     GetVbrTag(VBRTAGDATA * pTagData, const unsigned char *buf);

int     InitVbrTag(lame_global_flags * gfp);
int     PutVbrTag(lame_global_flags const *gfp, FILE * fid);
void    AddVbrFrame(lame_internal_flags * gfc);
void    UpdateMusicCRC(uint16_t * crc, const unsigned char *buffer, int size);



/*
 *	bit reservoir include file
 *
 *	Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

int     ResvFrameBegin(lame_internal_flags * gfc, int *mean_bits);
void    ResvMaxBits(lame_internal_flags * gfc, int mean_bits, int *targ_bits, int *max_bits,
                    int cbr);
void    ResvAdjust(lame_internal_flags * gfc, gr_info const *gi);
void    ResvFrameEnd(lame_internal_flags * gfc, int mean_bits);






#define CHANGED_FLAG    (1U << 0)
#define ADD_V2_FLAG     (1U << 1)
#define V1_ONLY_FLAG    (1U << 2)
#define V2_ONLY_FLAG    (1U << 3)
#define SPACE_V1_FLAG   (1U << 4)
#define PAD_V2_FLAG     (1U << 5)

enum {
    MIMETYPE_NONE = 0,
    MIMETYPE_JPEG,
    MIMETYPE_PNG,
    MIMETYPE_GIF
};


/* write tag into stream at current position */
extern int id3tag_write_v2(lame_global_flags * gfp);
extern int id3tag_write_v1(lame_global_flags * gfp);
/*
 * NOTE: A version 2 tag will NOT be added unless one of the text fields won't
 * fit in a version 1 tag (e.g. the title string is longer than 30 characters),
 * or the "id3tag_add_v2" or "id3tag_v2_only" functions are used.
 */




/*
 *      lame_intrin.h include file
 *
 *      Copyright (c) 2006 Gabriel Bouvigne
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef LAME_INTRIN_H
#define LAME_INTRIN_H


void init_xrpow_core_sse(gr_info * const cod_info, FLOAT xrpow[576], int upper, FLOAT * sum);

void fht_SSE2(FLOAT* , int);

#endif


#ifdef __cplusplus
//}
#endif



// per streaming MP3/radio... v.MP3 reader
/*struct MP3_FRAME_HEADER {			//https://wiki.hydrogenaud.io/images/a/a3/MP3_file_structure.png
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
	};*/

// 209 byte/frame @64Kbps header incluso, ~39 frame/sec
// 418 byte/frame @128Kbps (pare che se c'è silenzio ne escono da 16 byte) header incluso; ~39 frame/sec
// 627 byte/frame @192Kbps header incluso, idem
