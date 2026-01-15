// GD/C adapted 12/1/2026 


#include "stdafx.h"
#include "vidsend.h"
#include "vidsendLog.h"

#include <stdint.h>

#include "vidsendH264.h"


#define BYTE_STREAM_ERROR  0xFFFFFFFF

/*------------------------------------------------------------------------------

    Function name: ExtractNalUnit

        Functional description:
            Extracts one NAL unit from the byte stream buffer. Removes
            emulation prevention bytes if present. The original stream buffer
            is used directly and is therefore modified if emulation prevention
            bytes are present in the stream.

            Stream buffer is assumed to contain either exactly one NAL unit
            and nothing else, or one or more NAL units embedded in byte
            stream format described in the Annex B of the standard. Function
            detects which one is used based on the first bytes in the buffer.

        Inputs:
            pByteStream     pointer to byte stream buffer
            len             length of the stream buffer (in bytes)

        Outputs:
            pStrmData       stream information is stored here
            readBytes       number of bytes "consumed" from the stream buffer

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      error in byte stream

------------------------------------------------------------------------------*/
uint32_t h264bsdExtractNalUnit(uint8_t *pByteStream, uint32_t len, strmData_t *pStrmData, uint32_t *readBytes) {
  uint32_t i, tmp;
  uint32_t byteCount,initByteCount;
  uint32_t zeroCount;
  uint8_t  byte;
  uint32_t hasEmulation = HANTRO_FALSE;
  uint32_t invalidStream = HANTRO_FALSE;
  uint8_t *readPtr, *writePtr;

  ASSERT(pByteStream);
  ASSERT(len);
  ASSERT(len < BYTE_STREAM_ERROR);
  ASSERT(pStrmData);

  /* byte stream format if starts with 0x000001 or 0x000000 */
  if(len > 3 && pByteStream[0] == 0x00 && pByteStream[1] == 0x00 && (pByteStream[2] & 0xFE) == 0x00) {
    /* search for NAL unit start point, i.e. point after first start code prefix in the stream */
    zeroCount = byteCount = 2;
    readPtr = pByteStream + 2;
    while(1) {
      byte = *readPtr++;
      byteCount++;

      if(byteCount == len) {
        /* no start code prefix found -> error */
        *readBytes = len;
        return(HANTRO_NOK);
				}

      if(!byte)
        zeroCount++;
      else if((byte == 0x01) && (zeroCount >= 2))
        break;
      else
        zeroCount=0;
			}

    initByteCount = byteCount;

    /* determine size of the NAL unit. Search for next start code prefix
     * or end of stream and ignore possible trailing zero bytes */
    zeroCount=0;
    while(1) {
      byte = *readPtr++;
      byteCount++;
      if(!byte)
        zeroCount++;

      if((byte == 0x03) && (zeroCount == 2))
         hasEmulation = HANTRO_TRUE;

      if((byte == 0x01) && (zeroCount >= 2)) {
        pStrmData->strmBuffSize = byteCount - initByteCount - zeroCount - 1;
        zeroCount -= MIN(zeroCount, 3);
        break;
	      }
      else if(byte) {
        if(zeroCount >= 3)
          invalidStream = HANTRO_TRUE;
        zeroCount=0;
		    }

      if(byteCount == len) {
        pStrmData->strmBuffSize = byteCount - initByteCount - zeroCount;
        break;
			  }
      }
    }
    /* separate NAL units as input -> just set stream params */
  else {
    initByteCount=0;
    zeroCount=0;
    pStrmData->strmBuffSize = len;
    hasEmulation = HANTRO_TRUE;
    }

  pStrmData->pStrmBuffStart    = pByteStream + initByteCount;
  pStrmData->pStrmCurrPos      = pStrmData->pStrmBuffStart;
  pStrmData->bitPosInWord     =0;
  pStrmData->strmBuffReadBits =0;

  /* return number of bytes "consumed" */
  *readBytes = pStrmData->strmBuffSize + initByteCount + zeroCount;

  if(invalidStream) 
    return(HANTRO_NOK);

  /* remove emulation prevention bytes before rbsp processing */
  if(hasEmulation) {
    tmp = pStrmData->strmBuffSize;
    readPtr = writePtr = pStrmData->pStrmBuffStart;
    zeroCount=0;
    for(i=tmp; i--;) {
      if((zeroCount == 2) && (*readPtr == 0x03)) {
        /* emulation prevention byte shall be followed by one of the
         * following bytes: 0x00, 0x01, 0x02, 0x03. This implies that
         * emulation prevention 0x03 byte shall not be the last byte
         * of the stream. */
        if((i == 0) || (*(readPtr+1) > 0x03))
            return(HANTRO_NOK);

        /* do not write emulation prevention byte */
        readPtr++;
        zeroCount=0;
				}
      else {
        /* NAL unit shall not contain byte sequences 0x000000,
         * 0x000001 or 0x000002 */
        if((zeroCount == 2) && (*readPtr <= 0x02))
            return(HANTRO_NOK);

        if(*readPtr == 0)
            zeroCount++;
        else
            zeroCount=0;

        *writePtr++ = *readPtr++;
	      }
			}

    /* (readPtr - writePtr) indicates number of "removed" emulation
     * prevention bytes -> subtract from stream buffer size */
    pStrmData->strmBuffSize -= (uint32_t)(readPtr - writePtr);
    }

  return(HANTRO_OK);
	}



/* Following descriptions use term "information field" to represent combination
 * of certain decoded symbol value and the length of the corresponding variable
 * length code word. For example, total_zeros information field consists of
 * 4 bits symbol value (bits [4,7]) along with four bits to represent length
 * of the VLC code word (bits [0,3]) */

/* macro to obtain length of the coeff token information field, bits [0,4]  */
#define LENGTH_TC(vlc) ((vlc) & 0x1F)
/* macro to obtain length of the other information fields, bits [0,3] */
#define LENGTH(vlc) ((vlc) & 0xF)
/* macro to obtain code word from the information fields, bits [4,7] */
#define INFO(vlc) (((vlc) >> 4) & 0xF)  /* 4 MSB bits contain information */
/* macro to obtain trailing ones from the coeff token information word,
 * bits [5,10] */
#define TRAILING_ONES(coeffToken) ((coeffToken>>5) & 0x3F)
/* macro to obtain total coeff from the coeff token information word,
 * bits [11,15] */
#define TOTAL_COEFF(coeffToken) (((coeffToken) >> 11) & 0x1F)

#define VLC_NOT_FOUND 0xFFFFFFFEU

/* VLC tables for coeff_token. Because of long codes (max. 16 bits) some of the
 * tables have been splitted into multiple separate tables. Each array/table
 * element has the following structure:
 * [5 bits for tot.coeff.] [6 bits for tr.ones] [5 bits for VLC length]
 * If there is a 0x0000 value, it means that there is not corresponding VLC
 * codeword for that index. */

/* VLC lengths up to 6 bits, 0 <= nC < 2 */
static const uint16_t coeffToken0_0[32] = {
    0x0000,0x0000,0x0000,0x2066,0x1026,0x0806,0x1865,0x1865,
    0x1043,0x1043,0x1043,0x1043,0x1043,0x1043,0x1043,0x1043,
    0x0822,0x0822,0x0822,0x0822,0x0822,0x0822,0x0822,0x0822,
    0x0822,0x0822,0x0822,0x0822,0x0822,0x0822,0x0822,0x0822};

/* VLC lengths up to 10 bits, 0 <= nC < 2 */
static const uint16_t coeffToken0_1[48] = {
    0x0000,0x0000,0x0000,0x0000,0x406a,0x304a,0x282a,0x200a,
    0x3869,0x3869,0x2849,0x2849,0x2029,0x2029,0x1809,0x1809,
    0x3068,0x3068,0x3068,0x3068,0x2048,0x2048,0x2048,0x2048,
    0x1828,0x1828,0x1828,0x1828,0x1008,0x1008,0x1008,0x1008,
    0x2867,0x2867,0x2867,0x2867,0x2867,0x2867,0x2867,0x2867,
    0x1847,0x1847,0x1847,0x1847,0x1847,0x1847,0x1847,0x1847};

/* VLC lengths up to 14 bits, 0 <= nC < 2 */
static const uint16_t coeffToken0_2[56] = {
    0x606e,0x584e,0x502e,0x500e,0x586e,0x504e,0x482e,0x480e,
    0x400d,0x400d,0x484d,0x484d,0x402d,0x402d,0x380d,0x380d,
    0x506d,0x506d,0x404d,0x404d,0x382d,0x382d,0x300d,0x300d,
    0x486b,0x486b,0x486b,0x486b,0x486b,0x486b,0x486b,0x486b,
    0x384b,0x384b,0x384b,0x384b,0x384b,0x384b,0x384b,0x384b,
    0x302b,0x302b,0x302b,0x302b,0x302b,0x302b,0x302b,0x302b,
    0x280b,0x280b,0x280b,0x280b,0x280b,0x280b,0x280b,0x280b};

/* VLC lengths up to 16 bits, 0 <= nC < 2 */
static const uint16_t coeffToken0_3[32] = {
    0x0000,0x0000,0x682f,0x682f,0x8010,0x8050,0x8030,0x7810,
    0x8070,0x7850,0x7830,0x7010,0x7870,0x7050,0x7030,0x6810,
    0x706f,0x706f,0x684f,0x684f,0x602f,0x602f,0x600f,0x600f,
    0x686f,0x686f,0x604f,0x604f,0x582f,0x582f,0x580f,0x580f};

/* VLC lengths up to 6 bits, 2 <= nC < 4 */
static const uint16_t coeffToken2_0[32] = {
    0x0000,0x0000,0x0000,0x0000,0x3866,0x2046,0x2026,0x1006,
    0x3066,0x1846,0x1826,0x0806,0x2865,0x2865,0x1025,0x1025,
    0x2064,0x2064,0x2064,0x2064,0x1864,0x1864,0x1864,0x1864,
    0x1043,0x1043,0x1043,0x1043,0x1043,0x1043,0x1043,0x1043};

/* VLC lengths up to 9 bits, 2 <= nC < 4 */
static const uint16_t coeffToken2_1[32] = {
    0x0000,0x0000,0x0000,0x0000,0x4869,0x3849,0x3829,0x3009,
    0x2808,0x2808,0x3048,0x3048,0x3028,0x3028,0x2008,0x2008,
    0x4067,0x4067,0x4067,0x4067,0x2847,0x2847,0x2847,0x2847,
    0x2827,0x2827,0x2827,0x2827,0x1807,0x1807,0x1807,0x1807};

/* VLC lengths up to 14 bits, 2 <= nC < 4 */
static const uint16_t coeffToken2_2[128] = {
    0x0000,0x0000,0x786d,0x786d,0x806e,0x804e,0x802e,0x800e,
    0x782e,0x780e,0x784e,0x702e,0x704d,0x704d,0x700d,0x700d,
    0x706d,0x706d,0x684d,0x684d,0x682d,0x682d,0x680d,0x680d,
    0x686d,0x686d,0x604d,0x604d,0x602d,0x602d,0x600d,0x600d,
    0x580c,0x580c,0x580c,0x580c,0x584c,0x584c,0x584c,0x584c,
    0x582c,0x582c,0x582c,0x582c,0x500c,0x500c,0x500c,0x500c,
    0x606c,0x606c,0x606c,0x606c,0x504c,0x504c,0x504c,0x504c,
    0x502c,0x502c,0x502c,0x502c,0x480c,0x480c,0x480c,0x480c,
    0x586b,0x586b,0x586b,0x586b,0x586b,0x586b,0x586b,0x586b,
    0x484b,0x484b,0x484b,0x484b,0x484b,0x484b,0x484b,0x484b,
    0x482b,0x482b,0x482b,0x482b,0x482b,0x482b,0x482b,0x482b,
    0x400b,0x400b,0x400b,0x400b,0x400b,0x400b,0x400b,0x400b,
    0x506b,0x506b,0x506b,0x506b,0x506b,0x506b,0x506b,0x506b,
    0x404b,0x404b,0x404b,0x404b,0x404b,0x404b,0x404b,0x404b,
    0x402b,0x402b,0x402b,0x402b,0x402b,0x402b,0x402b,0x402b,
    0x380b,0x380b,0x380b,0x380b,0x380b,0x380b,0x380b,0x380b};

/* VLC lengths up to 6 bits, 4 <= nC < 8 */
static const uint16_t coeffToken4_0[64] = {
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x1806,0x3846,0x3826,0x1006,0x4866,0x3046,0x3026,0x0806,
    0x2825,0x2825,0x2845,0x2845,0x2025,0x2025,0x2045,0x2045,
    0x1825,0x1825,0x4065,0x4065,0x1845,0x1845,0x1025,0x1025,
    0x3864,0x3864,0x3864,0x3864,0x3064,0x3064,0x3064,0x3064,
    0x2864,0x2864,0x2864,0x2864,0x2064,0x2064,0x2064,0x2064,
    0x1864,0x1864,0x1864,0x1864,0x1044,0x1044,0x1044,0x1044,
    0x0824,0x0824,0x0824,0x0824,0x0004,0x0004,0x0004,0x0004};

/* VLC lengths up to 10 bits, 4 <= nC < 8 */
static const uint16_t coeffToken4_1[128] = {
    0x0000,0x800a,0x806a,0x804a,0x802a,0x780a,0x786a,0x784a,
    0x782a,0x700a,0x706a,0x704a,0x702a,0x680a,0x6829,0x6829,
    0x6009,0x6009,0x6849,0x6849,0x6029,0x6029,0x5809,0x5809,
    0x6869,0x6869,0x6049,0x6049,0x5829,0x5829,0x5009,0x5009,
    0x6068,0x6068,0x6068,0x6068,0x5848,0x5848,0x5848,0x5848,
    0x5028,0x5028,0x5028,0x5028,0x4808,0x4808,0x4808,0x4808,
    0x5868,0x5868,0x5868,0x5868,0x5048,0x5048,0x5048,0x5048,
    0x4828,0x4828,0x4828,0x4828,0x4008,0x4008,0x4008,0x4008,
    0x3807,0x3807,0x3807,0x3807,0x3807,0x3807,0x3807,0x3807,
    0x3007,0x3007,0x3007,0x3007,0x3007,0x3007,0x3007,0x3007,
    0x4847,0x4847,0x4847,0x4847,0x4847,0x4847,0x4847,0x4847,
    0x2807,0x2807,0x2807,0x2807,0x2807,0x2807,0x2807,0x2807,
    0x5067,0x5067,0x5067,0x5067,0x5067,0x5067,0x5067,0x5067,
    0x4047,0x4047,0x4047,0x4047,0x4047,0x4047,0x4047,0x4047,
    0x4027,0x4027,0x4027,0x4027,0x4027,0x4027,0x4027,0x4027,
    0x2007,0x2007,0x2007,0x2007,0x2007,0x2007,0x2007,0x2007};

/* fixed 6 bit length VLC, nC <= 8 */
static const uint16_t coeffToken8[64] = {
    0x0806,0x0826,0x0000,0x0006,0x1006,0x1026,0x1046,0x0000,
    0x1806,0x1826,0x1846,0x1866,0x2006,0x2026,0x2046,0x2066,
    0x2806,0x2826,0x2846,0x2866,0x3006,0x3026,0x3046,0x3066,
    0x3806,0x3826,0x3846,0x3866,0x4006,0x4026,0x4046,0x4066,
    0x4806,0x4826,0x4846,0x4866,0x5006,0x5026,0x5046,0x5066,
    0x5806,0x5826,0x5846,0x5866,0x6006,0x6026,0x6046,0x6066,
    0x6806,0x6826,0x6846,0x6866,0x7006,0x7026,0x7046,0x7066,
    0x7806,0x7826,0x7846,0x7866,0x8006,0x8026,0x8046,0x8066};

/* VLC lengths up to 3 bits, nC == -1 */
static const uint16_t coeffTokenMinus1_0[8] = {
    0x0000,0x1043,0x0002,0x0002,0x0821,0x0821,0x0821,0x0821};

/* VLC lengths up to 8 bits, nC == -1 */
static const uint16_t coeffTokenMinus1_1[32] = {
    0x2067,0x2067,0x2048,0x2028,0x1847,0x1847,0x1827,0x1827,
    0x2006,0x2006,0x2006,0x2006,0x1806,0x1806,0x1806,0x1806,
    0x1006,0x1006,0x1006,0x1006,0x1866,0x1866,0x1866,0x1866,
    0x1026,0x1026,0x1026,0x1026,0x0806,0x0806,0x0806,0x0806};

/* VLC tables for total_zeros. One table containing longer code, totalZeros_1,
 * has been broken into two separate tables. Table elements have the
 * following structure:
 * [4 bits for info] [4 bits for VLC length] */

/* VLC lengths up to 5 bits */
static const uint8_t totalZeros_1_0[32] = {
    0x00,0x00,0x65,0x55,0x44,0x44,0x34,0x34,
    0x23,0x23,0x23,0x23,0x13,0x13,0x13,0x13,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01};

/* VLC lengths up to 9 bits */
static const uint8_t totalZeros_1_1[32] = {
    0x00,0xf9,0xe9,0xd9,0xc8,0xc8,0xb8,0xb8,
    0xa7,0xa7,0xa7,0xa7,0x97,0x97,0x97,0x97,
    0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,
    0x76,0x76,0x76,0x76,0x76,0x76,0x76,0x76};

static const uint8_t totalZeros_2[64] = {
    0xe6,0xd6,0xc6,0xb6,0xa5,0xa5,0x95,0x95,
    0x84,0x84,0x84,0x84,0x74,0x74,0x74,0x74,
    0x64,0x64,0x64,0x64,0x54,0x54,0x54,0x54,
    0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,
    0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33,
    0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
    0x13,0x13,0x13,0x13,0x13,0x13,0x13,0x13,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};

static const uint8_t totalZeros_3[64] = {
    0xd6,0xb6,0xc5,0xc5,0xa5,0xa5,0x95,0x95,
    0x84,0x84,0x84,0x84,0x54,0x54,0x54,0x54,
    0x44,0x44,0x44,0x44,0x04,0x04,0x04,0x04,
    0x73,0x73,0x73,0x73,0x73,0x73,0x73,0x73,
    0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
    0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33,
    0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
    0x13,0x13,0x13,0x13,0x13,0x13,0x13,0x13};

static const uint8_t totalZeros_4[32] = {
    0xc5,0xb5,0xa5,0x05,0x94,0x94,0x74,0x74,
    0x34,0x34,0x24,0x24,0x83,0x83,0x83,0x83,
    0x63,0x63,0x63,0x63,0x53,0x53,0x53,0x53,
    0x43,0x43,0x43,0x43,0x13,0x13,0x13,0x13};

static const uint8_t totalZeros_5[32] = {
    0xb5,0x95,0xa4,0xa4,0x84,0x84,0x24,0x24,
    0x14,0x14,0x04,0x04,0x73,0x73,0x73,0x73,
    0x63,0x63,0x63,0x63,0x53,0x53,0x53,0x53,
    0x43,0x43,0x43,0x43,0x33,0x33,0x33,0x33};

static const uint8_t totalZeros_6[64] = {
    0xa6,0x06,0x15,0x15,0x84,0x84,0x84,0x84,
    0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
    0x73,0x73,0x73,0x73,0x73,0x73,0x73,0x73,
    0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
    0x53,0x53,0x53,0x53,0x53,0x53,0x53,0x53,
    0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,
    0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33,
    0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23};

static const uint8_t totalZeros_7[64] = {
    0x96,0x06,0x15,0x15,0x74,0x74,0x74,0x74,
    0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,
    0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
    0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,
    0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33,
    0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
    0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,
    0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52};

static const uint8_t totalZeros_8[64] = {
    0x86,0x06,0x25,0x25,0x14,0x14,0x14,0x14,
    0x73,0x73,0x73,0x73,0x73,0x73,0x73,0x73,
    0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
    0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33,
    0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,
    0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,
    0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,
    0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42};

static const uint8_t totalZeros_9[64] = {
    0x16,0x06,0x75,0x75,0x24,0x24,0x24,0x24,
    0x53,0x53,0x53,0x53,0x53,0x53,0x53,0x53,
    0x62,0x62,0x62,0x62,0x62,0x62,0x62,0x62,
    0x62,0x62,0x62,0x62,0x62,0x62,0x62,0x62,
    0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,
    0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,
    0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,
    0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32};

static const uint8_t totalZeros_10[32] = {
    0x15,0x05,0x64,0x64,0x23,0x23,0x23,0x23,
    0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,
    0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,
    0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32};

static const uint8_t totalZeros_11[16] = {
    0x04,0x14,0x23,0x23,0x33,0x33,0x53,0x53,
    0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41};

static const uint8_t totalZeros_12[16] = {
    0x04,0x14,0x43,0x43,0x22,0x22,0x22,0x22,
    0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31};

static const uint8_t totalZeros_13[8] = {0x03,0x13,0x32,0x32,0x21,0x21,0x21,0x21};

static const uint8_t totalZeros_14[4] = {0x02,0x12,0x21,0x21};

/* VLC tables for run_before. Table elements have the following structure:
 * [4 bits for info] [4bits for VLC length]
 */

static const uint8_t runBefore_6[8] = {0x13,0x23,0x43,0x33,0x63,0x53,0x02,0x02};
static const uint8_t runBefore_5[8] = {0x53,0x43,0x33,0x23,0x12,0x12,0x02,0x02};
static const uint8_t runBefore_4[8] = {0x43,0x33,0x22,0x22,0x12,0x12,0x02,0x02};
static const uint8_t runBefore_3[4] = {0x32,0x22,0x12,0x02};
static const uint8_t runBefore_2[4] = {0x22,0x12,0x01,0x01};
static const uint8_t runBefore_1[2] = {0x11,0x01};

/* following four macros are used to handle stream buffer "cache" in the CAVLC
 * decoding function */

/* macro to initialize stream buffer cache, fills the buffer (32 bits) */
#define BUFFER_INIT(value, bits) \
{ \
    bits = 32; \
    value = h264bsdShowBits32(pStrmData); \
}

/* macro to read numBits bits from the buffer, bits will be written to
 * outVal. Refills the buffer if not enough bits left */
#define BUFFER_SHOW(value, bits, outVal, numBits) \
{ \
    if(bits < (numBits)) { \
        if(h264bsdFlushBits(pStrmData,32-bits) == END_OF_STREAM) \
            return(HANTRO_NOK); \
        value = h264bsdShowBits32(pStrmData); \
        bits = 32; \
    } \
    (outVal) = value >> (32 - (numBits)); \
}

/* macro to flush numBits bits from the buffer */
#define BUFFER_FLUSH(value, bits, numBits) \
{ \
    value <<= (numBits); \
    bits -= (numBits); \
}

/* macro to read and flush  numBits bits from the buffer, bits will be written
 * to outVal. Refills the buffer if not enough bits left */
#define BUFFER_GET(value, bits, outVal, numBits) \
{ \
    if(bits < (numBits)) { \
        if(h264bsdFlushBits(pStrmData,32-bits) == END_OF_STREAM) \
            return(HANTRO_NOK); \
        value = h264bsdShowBits32(pStrmData); \
        bits = 32; \
    } \
    (outVal) = value >> (32 - (numBits)); \
    value <<= (numBits); \
    bits -= (numBits); \
}


static uint32_t DecodeCoeffToken(uint32_t bits, uint32_t nc);
static uint32_t DecodeLevelPrefix(uint32_t bits);
static uint32_t DecodeTotalZeros(uint32_t bits, uint32_t totalCoeff, uint32_t isChromaDC);
static uint32_t DecodeRunBefore(uint32_t bits,uint32_t zerosLeft);

/*------------------------------------------------------------------------------

    Function: DecodeCoeffToken

        Functional description:
          Function to decode coeff_token information field from the stream.

        Inputs:
          uint32_t bits                  next 16 stream bits
          uint32_t nc                    nC, see standard for details

        Outputs:
          uint32_t  information field (11 bits for value, 5 bits for length)

------------------------------------------------------------------------------*/
uint32_t DecodeCoeffToken(uint32_t bits, uint32_t nc) {
    uint32_t value;


    /* standard defines that nc for decoding of chroma dc coefficients is -1,
     * represented by uint32_t here -> -1 maps to 2^32 - 1 */
    ASSERT(nc <= 16 || nc == (uint32_t)(-1));

    if(nc < 2) {
        if(bits >= 0x8000)
            value=0x0001;
        else if(bits >= 0x0C00)
            value = coeffToken0_0[bits >> 10];
        else if(bits >= 0x0100)
            value = coeffToken0_1[bits >> 6];
        else if(bits >= 0x0020)
            value = coeffToken0_2[(bits>>2)-8];
        else
            value = coeffToken0_3[bits];
	    }
    else if(nc < 4) {
        if(bits >= 0x8000)
            value = bits & 0x4000 ? 0x0002 : 0x0822;
        else if(bits >= 0x1000)
            value = coeffToken2_0[bits >> 10];
        else if(bits >= 0x0200)
            value = coeffToken2_1[bits >> 7];
        else
            value = coeffToken2_2[bits>>2];
		  }
    else if(nc < 8) {
        value = coeffToken4_0[bits >> 10];
        if(!value)
            value = coeffToken4_1[bits>>6];
			}
    else if(nc <= 16) {
        value = coeffToken8[bits>>10];
			}
    else {
        value = coeffTokenMinus1_0[bits >> 13];
        if(!value)
            value = coeffTokenMinus1_1[bits>>8];
			}

    return(value);

}

/*------------------------------------------------------------------------------

    Function: DecodeLevelPrefix

        Functional description:
          Function to decode level_prefix information field from the stream

        Inputs:
          uint32_t bits      next 16 stream bits

        Outputs:
          uint32_t  level_prefix information field or VLC_NOT_FOUND

------------------------------------------------------------------------------*/
uint32_t DecodeLevelPrefix(uint32_t bits) {
  uint32_t numZeros;

  if(bits >= 0x8000)
      numZeros=0;
  else if(bits >= 0x4000)
      numZeros = 1;
  else if(bits >= 0x2000)
      numZeros = 2;
  else if(bits >= 0x1000)
      numZeros = 3;
  else if(bits >= 0x0800)
      numZeros = 4;
  else if(bits >= 0x0400)
      numZeros = 5;
  else if(bits >= 0x0200)
      numZeros = 6;
  else if(bits >= 0x0100)
      numZeros = 7;
  else if(bits >= 0x0080)
      numZeros = 8;
  else if(bits >= 0x0040)
      numZeros = 9;
  else if(bits >= 0x0020)
      numZeros = 10;
  else if(bits >= 0x0010)
      numZeros = 11;
  else if(bits >= 0x0008)
      numZeros = 12;
  else if(bits >= 0x0004)
      numZeros = 13;
  else if(bits >= 0x0002)
      numZeros = 14;
  else if(bits >= 0x0001)
      numZeros = 15;
  else /* more than 15 zeros encountered which is an error */
      return(VLC_NOT_FOUND);

  return(numZeros);

}

/*------------------------------------------------------------------------------

    Function: DecodeTotalZeros

        Functional description:
          Function to decode total_zeros information field from the stream

        Inputs:
          uint32_t bits                  next 9 stream bits
          uint32_t totalCoeff            total number of coefficients for the block
                                    being decoded
          uint32_t isChromaDC           flag to indicate chroma DC block

        Outputs:
          uint32_t  information field (4 bits value, 4 bits length)

------------------------------------------------------------------------------*/
uint32_t DecodeTotalZeros(uint32_t bits, uint32_t totalCoeff, uint32_t isChromaDC) {
   uint32_t value=0x0;


  ASSERT(totalCoeff);

  if(!isChromaDC) {
      ASSERT(totalCoeff < 16);
      switch(totalCoeff) {
          case 1:
              value = totalZeros_1_0[bits >> 4];
              if(!value)
                  value = totalZeros_1_1[bits];
              break;

          case 2:
              value = totalZeros_2[bits >> 3];
              break;

          case 3:
              value = totalZeros_3[bits >> 3];
              break;

          case 4:
              value = totalZeros_4[bits >> 4];
              break;

          case 5:
              value = totalZeros_5[bits >> 4];
              break;

          case 6:
              value = totalZeros_6[bits >> 3];
              break;

          case 7:
              value = totalZeros_7[bits >> 3];
              break;

          case 8:
              value = totalZeros_8[bits >> 3];
              break;

          case 9:
              value = totalZeros_9[bits >> 3];
              break;
          case 10:
              value = totalZeros_10[bits >> 4];
              break;
          case 11:
              value = totalZeros_11[bits >> 5];
              break;
          case 12:
              value = totalZeros_12[bits >> 5];
              break;
          case 13:
              value = totalZeros_13[bits >> 6];
              break;
          case 14:
              value = totalZeros_14[bits >> 7];
              break;
          default: /* case 15 */
              value = (bits >> 8) ? 0x11 : 0x01;
              break;
		    }
	    }
    else {
        ASSERT(totalCoeff < 4);
        bits >>= 6;
        if(bits > 3)
            value=0x01;
        else {
            if(totalCoeff == 3)
                value=0x11;
            else if(bits > 1)
                value=0x12;
            else if(totalCoeff == 2)
                value=0x22;
            else if(bits)
                value=0x23;
            else
                value=0x33;
        }
    }

  return value;
	}

/*------------------------------------------------------------------------------

    Function: DecodeRunBefore

        Functional description:
          Function to decode run_before information field from the stream

        Inputs:
          uint32_t bits                  next 11 stream bits
          uint32_t zerosLeft             number of zeros left for the current block

        Outputs:
          uint32_t  information field (4 bits value, 4 bits length)

------------------------------------------------------------------------------*/
uint32_t DecodeRunBefore(uint32_t bits, uint32_t zerosLeft) {
  uint32_t value=0x0;

  switch(zerosLeft) {
      case 1:
          value = runBefore_1[bits>>10];
          break;
      case 2:
          value = runBefore_2[bits>>9];
          break;
      case 3:
          value = runBefore_3[bits>>9];
          break;
      case 4:
          value = runBefore_4[bits>>8];
          break;
      case 5:
          value = runBefore_5[bits>>8];
          break;
      case 6:
          value = runBefore_6[bits>>8];
          break;
      default:
          if(bits >= 0x100)
              value = ((7-(bits>>8))<<4)+0x3;
          else if(bits >= 0x80)
              value=0x74;
          else if(bits >= 0x40)
              value=0x85;
          else if(bits >= 0x20)
              value=0x96;
          else if(bits >= 0x10)
              value=0xa7;
          else if(bits >= 0x8)
              value=0xb8;
          else if(bits >= 0x4)
              value=0xc9;
          else if(bits >= 0x2)
              value=0xdA;
          else if(bits)
              value=0xeB;
          if(INFO(value) > zerosLeft)
              value=0;
          break;
    }

  return value;
	}

/*------------------------------------------------------------------------------

    Function: DecodeResidualBlockCavlc

        Functional description:
          Function to decode one CAVLC coded block. This corresponds to
          syntax elements residual_block_cavlc() in the standard.

        Inputs:
          pStrmData             pointer to stream data structure
          nc                    nC value
          maxNumCoeff           maximum number of residual coefficients

        Outputs:
          coeffLevel            stores decoded coefficient levels

        Returns:
          numCoeffs             on bits [4,11] if successful
          coeffMap              on bits [16,31] if successful, this is bit map
                                where each bit indicates if the corresponding
                                coefficient was zero (0) or non-zero (1)
          HANTRO_NOK            end of stream or error in stream

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeResidualBlockCavlc(strmData_t *pStrmData,
  int32_t *coeffLevel, int32_t nc, uint32_t maxNumCoeff) {

    uint32_t i, tmp, totalCoeff, trailingOnes, suffixLength, levelPrefix;
    uint32_t levelSuffix, zerosLeft, bit;
    int32_t level[16];
    uint32_t run[16];
    /* stream "cache" */
    uint32_t bufferValue;
    uint32_t bufferBits;


    ASSERT(pStrmData);
    ASSERT(coeffLevel);
    ASSERT(nc > -2);
    ASSERT(maxNumCoeff == 4 || maxNumCoeff == 15 || maxNumCoeff == 16);
    ASSERT(VLC_NOT_FOUND != END_OF_STREAM);

    /* assume that coeffLevel array has been "cleaned" by caller */

    BUFFER_INIT(bufferValue, bufferBits);

    /*lint -e774 disable lint warning on always false comparison */
    BUFFER_SHOW(bufferValue, bufferBits, bit, 16);
    /*lint +e774 */
    tmp = DecodeCoeffToken(bit, (uint32_t)nc);
    if(!tmp)
        return(HANTRO_NOK);
    BUFFER_FLUSH(bufferValue, bufferBits, LENGTH_TC(tmp));

    totalCoeff = TOTAL_COEFF(tmp);
    if(totalCoeff > maxNumCoeff)
        return(HANTRO_NOK);
    trailingOnes = TRAILING_ONES(tmp);

    if(totalCoeff != 0) {
        i=0;
        /* nonzero coefficients: +/- 1 */
        if(trailingOnes) {
            BUFFER_GET(bufferValue, bufferBits, bit, trailingOnes);
            tmp = 1 << (trailingOnes - 1);
            for(; tmp; i++) {
                level[i] = bit & tmp ? -1 : 1;
                tmp >>= 1;
		          }
	        }

        /* other levels */
        if(totalCoeff > 10 && trailingOnes < 3)
            suffixLength = 1;
        else
            suffixLength=0;

        for(; i < totalCoeff; i++) {
            BUFFER_SHOW(bufferValue, bufferBits, bit, 16);
            levelPrefix = DecodeLevelPrefix(bit);
            if(levelPrefix == VLC_NOT_FOUND)
                return(HANTRO_NOK);
            BUFFER_FLUSH(bufferValue, bufferBits, levelPrefix+1);

            if(levelPrefix < 14)
                tmp = suffixLength;
            else if(levelPrefix == 14)
                tmp = suffixLength ? suffixLength : 4;
            else {
                /* setting suffixLength to 1 here corresponds to adding 15
                 * to levelCode value if levelPrefix == 15 and
                 * suffixLength == 0 */
                if(!suffixLength)
                    suffixLength = 1;
                tmp = 12;
	            }

            if(suffixLength)
                levelPrefix <<= suffixLength;

            if(tmp) {
                BUFFER_GET(bufferValue, bufferBits, levelSuffix, tmp);
                levelPrefix += levelSuffix;
            }

            tmp = levelPrefix;

            if(i == trailingOnes && trailingOnes < 3)
                tmp += 2;

            level[i] = (tmp+2)>>1;

            if(suffixLength == 0)
                suffixLength = 1;

            if((level[i] > (3 << (suffixLength - 1))) && suffixLength < 6)
                suffixLength++;

            if(tmp & 0x1)
                level[i] = -level[i];
			    }

        /* zero runs */
        if(totalCoeff < maxNumCoeff) {
            BUFFER_SHOW(bufferValue, bufferBits, bit,9);
            zerosLeft = DecodeTotalZeros(bit, totalCoeff,
                                        (uint32_t)(maxNumCoeff == 4));
            if(!zerosLeft)
                return(HANTRO_NOK);
            BUFFER_FLUSH(bufferValue, bufferBits, LENGTH(zerosLeft));
            zerosLeft = INFO(zerosLeft);
        }
        else
            zerosLeft=0;

        for(i=0; i < totalCoeff - 1; i++) {
            if(zerosLeft > 0) {
                BUFFER_SHOW(bufferValue, bufferBits, bit,11);
                tmp = DecodeRunBefore(bit, zerosLeft);
                if(!tmp)
                    return(HANTRO_NOK);
                BUFFER_FLUSH(bufferValue, bufferBits, LENGTH(tmp));
                run[i] = INFO(tmp);
                zerosLeft -= run[i]++;
				      }
            else
              run[i] = 1;
        }

        /* combining level and run, levelSuffix variable used to hold coeffMap,
         * i.e. bit map indicating which coefficients had non-zero value. */

        tmp = zerosLeft;
        coeffLevel[tmp] = level[totalCoeff-1];
        levelSuffix = 1 << tmp;
        for(i = totalCoeff-1; i--;) {
            tmp += run[i];
            levelSuffix |= 1 << tmp;
            coeffLevel[tmp] = level[i];
        }

			}
    else
        levelSuffix=0;

    if(h264bsdFlushBits(pStrmData, 32-bufferBits) != HANTRO_OK)
        return(HANTRO_NOK);

  return((totalCoeff << 4) | (levelSuffix << 16));
	}



static uint32_t ConcealMb(mbStorage_t *pMb, image_t *currImage, uint32_t row, uint32_t col,
    uint32_t sliceType, uint8_t *data);
static void Transform(int32_t *data);

/*------------------------------------------------------------------------------

    Function name: h264bsdConceal

        Functional description:
            Perform error concealment for a picture. Two types of concealment
            is performed based on sliceType:
                1) copy from previous picture for P-slices.
                2) concealment from neighbour pixels for I-slices

            I-type concealment is based on ideas presented by Jarno Tulkki.
            The concealment algorithm determines frequency domain coefficients
            from the neighbour pixels, applies integer transform (the same
            transform used in the residual processing) and uses the results as
            pixel values for concealed macroblocks. Transform produces 4x4
            array and one pixel value has to be used for 4x4 luma blocks and
            2x2 chroma blocks.

            Similar concealment is performed for whole picture (the choise
            of the type is based on last successfully decoded slice header of
            the picture but it is handled by the calling function). It is
            acknowledged that this may result in wrong type of concealment
            when a picture contains both types of slices. However,
            determination of slice type macroblock-by-macroblock cannot
            be done due to the fact that it is impossible to know to which
            slice each corrupted (not successfully decoded) macroblock
            belongs.

            The error concealment is started by searching the first propoerly
            decoded macroblock and concealing the row containing the macroblock
            in question. After that all macroblocks above the row in question
            are concealed. Finally concealment of rows below is performed.
            The order of concealment for 4x4 picture where macroblock 9 is the
            first properly decoded one is as follows (properly decoded
            macroblocks marked with 'x', numbers indicating the order of
            concealment):

               4  6  8 10
               3  5  7  9
               1  x  x  2
              11 12 13 14

            If all macroblocks of the picture are lost, the concealment is
            copy of previous picture for P-type and setting the image to
            constant gray (pixel value 128) for I-type.

            Concealment sets quantization parameter of the concealed
            macroblocks to value 40 and macroblock type to intra to enable
            deblocking filter to smooth the edges of the concealed areas.

        Inputs:
            pStorage        pointer to storage structure
            currImage       pointer to current image structure
            sliceType       type of the slice

        Outputs:
            currImage       concealed macroblocks will be written here

        Returns:
            HANTRO_OK

------------------------------------------------------------------------------*/
uint32_t h264bsdConceal(storage_t *pStorage, image_t *currImage, uint32_t sliceType) {
  uint32_t i, j;
  uint32_t row, col;
  uint32_t width, height;
  uint8_t *refData;
  mbStorage_t *mb;

  ASSERT(pStorage);
  ASSERT(currImage);

	if(IS_I_SLICE(sliceType))
		DEBUGP("Concealing intra slice");
	else
		DEBUGP("Concealing inter slice");

  width = currImage->width;
  height = currImage->height;
  refData = NULL;
  /* use reference picture with smallest available index */
  if(IS_P_SLICE(sliceType) || (pStorage->intraConcealmentFlag != 0)) {
    i=0;
    do {
      refData = h264bsdGetRefPicData(pStorage->dpb, i);
      i++;
      if(i >= 16)
          break;
			} while(!refData);
		}

  i = row = col=0;
  /* find first properly decoded macroblock -> start point for concealment */
  while(i < pStorage->picSizeInMbs && !pStorage->mb[i].decoded) {
    i++;
    col++;
    if(col == width) {
        row++;
        col=0;
      }
		}

  /* whole picture lost -> copy previous or set grey */
  if(i == pStorage->picSizeInMbs) {
    if((IS_I_SLICE(sliceType) && (pStorage->intraConcealmentFlag == 0)) || !refData)
      memset(currImage->data, 128, width*height*384);
		else {
#ifndef FLASCC
        memcpy(currImage->data, refData, width*height*384);
#else
        int ii=0;
        int size = width*height*384;
        uint8_t* curr_data = currImage->data;
        for(ii=0; ii < size;ii++)
            curr_data[i] = refData[i];
#endif
				}

      pStorage->numConcealedMbs = pStorage->picSizeInMbs;

      /* no filtering if whole picture concealed */
      for(i=0; i < pStorage->picSizeInMbs; i++)
          pStorage->mb[i].disableDeblockingFilterIdc = 1;

      return(HANTRO_OK);
		}

  /* start from the row containing the first correct macroblock, conceal the
   * row in question, all rows above that row and then continue downwards */
  mb = pStorage->mb + row * width;
  for(j = col; j--;) {
      ConcealMb(mb+j, currImage, row, j, sliceType, refData);
      mb[j].decoded = 1;
      pStorage->numConcealedMbs++;
		}
  for(j = col + 1; j < width; j++) {
      if(!mb[j].decoded) {
          ConcealMb(mb+j, currImage, row, j, sliceType, refData);
          mb[j].decoded = 1;
          pStorage->numConcealedMbs++;
      }
		}
  /* if previous row(s) could not be concealed -> conceal them now */
  if(row) {
      for(j=0; j < width; j++) {
          i = row - 1;
          mb = pStorage->mb + i*width + j;
          do{
              ConcealMb(mb, currImage, i, j, sliceType, refData);
              mb->decoded = 1;
              pStorage->numConcealedMbs++;
              mb -= width;
						} while(i--);
				}
		}

  /* process rows below the one containing the first correct macroblock */
  for(i = row + 1; i < height; i++) {
      mb = pStorage->mb + i * width;

      for(j=0; j < width; j++) {
          if(!mb[j].decoded) {
              ConcealMb(mb+j, currImage, i, j, sliceType, refData);
              mb[j].decoded = 1;
              pStorage->numConcealedMbs++;
          }
      }
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function name: ConcealMb

        Functional description:
            Perform error concealment for one macroblock, location of the
            macroblock in the picture indicated by row and col

------------------------------------------------------------------------------*/
uint32_t ConcealMb(mbStorage_t *pMb, image_t *currImage, uint32_t row, uint32_t col,
    uint32_t sliceType, uint8_t *refData) {
    uint32_t i, j, comp;
    uint32_t hor, ver;
    uint32_t mbNum;
    uint32_t width, height;
    uint8_t *mbPos;
    uint8_t data[384];
    uint8_t *pData;
    int32_t tmp;
    int32_t firstPhase[16];
    int32_t *pTmp;
    /* neighbours above, below, left and right */
    int32_t a[4] = { 0,0,0,0 }, b[4], l[4] = { 0,0,0,0 }, r[4];
    uint32_t A, B, L, R;
#ifdef H264DEC_OMXDL
    uint8_t fillBuff[32*21 + 15 + 32];
    uint8_t *pFill;
#endif

    ASSERT(pMb);
    ASSERT(!pMb->decoded);
    ASSERT(currImage);
    ASSERT(col < currImage->width);
    ASSERT(row < currImage->height);

#ifdef H264DEC_OMXDL
    pFill = ALIGN(fillBuff, 16);
#endif
    width = currImage->width;
    height = currImage->height;
    mbNum = row * width + col;

    h264bsdSetCurrImageMbPointers(currImage, mbNum);

    mbPos = currImage->data + row * MACROBLOCK_SIZE * width * MACROBLOCK_SIZE + col * MACROBLOCK_SIZE;
    A = B = L = R = HANTRO_FALSE;

    /* set qpY to 40 to enable some filtering in deblocking (stetson value) */
    pMb->qpY = 40;
    pMb->disableDeblockingFilterIdc=0;
    /* mbType set to intra to perform filtering despite the values of other
     * boundary strength determination fields */
    pMb->mbType = I_4x4;
    pMb->filterOffsetA=0;
    pMb->filterOffsetB=0;
    pMb->chromaQpIndexOffset=0;

    if(IS_I_SLICE(sliceType))
        memset(data, 0, sizeof(data));
    else {
        mv_t mv = {0,0};
        image_t refImage;
        refImage.width = width;
        refImage.height = height;
        refImage.data = refData;
        if(refImage.data) {
#ifndef H264DEC_OMXDL
            h264bsdPredictSamples(data, &mv, &refImage, col*16, row*16,
                0, 0, 16, 16);
#else
            h264bsdPredictSamples(data, &mv, &refImage,
                    ((row*16) + ((col*16)<<16)),
                    0x00001010, pFill);
#endif
            h264bsdWriteMacroblock(currImage, data);

            return(HANTRO_OK);
		      }
        else
            memset(data, 0, sizeof(data));
	    }

    memset(firstPhase, 0, sizeof(firstPhase));

    /* counter for number of neighbours used */
    j=0;
    hor = ver=0;
    if(row && (pMb-width)->decoded) {
        A = HANTRO_TRUE;
        pData = mbPos - width*16;
        a[0] = *pData++; a[0] += *pData++; a[0] += *pData++; a[0] += *pData++;
        a[1] = *pData++; a[1] += *pData++; a[1] += *pData++; a[1] += *pData++;
        a[2] = *pData++; a[2] += *pData++; a[2] += *pData++; a[2] += *pData++;
        a[3] = *pData++; a[3] += *pData++; a[3] += *pData++; a[3] += *pData++;
        j++;
        hor++;
        firstPhase[0] += a[0] + a[1] + a[2] + a[3];
        firstPhase[1] += a[0] + a[1] - a[2] - a[3];
			}
    if((row != height - 1) && (pMb+width)->decoded) {
        B = HANTRO_TRUE;
        pData = mbPos + 16*width*16;
        b[0] = *pData++; b[0] += *pData++; b[0] += *pData++; b[0] += *pData++;
        b[1] = *pData++; b[1] += *pData++; b[1] += *pData++; b[1] += *pData++;
        b[2] = *pData++; b[2] += *pData++; b[2] += *pData++; b[2] += *pData++;
        b[3] = *pData++; b[3] += *pData++; b[3] += *pData++; b[3] += *pData++;
        j++;
        hor++;
        firstPhase[0] += b[0] + b[1] + b[2] + b[3];
        firstPhase[1] += b[0] + b[1] - b[2] - b[3];
			}
    if(col && (pMb-1)->decoded) {
        L = HANTRO_TRUE;
        pData = mbPos - 1;
        l[0] = pData[0]; l[0] += pData[16*width];
        l[0] += pData[32*width]; l[0] += pData[48*width];
        pData += 64*width;
        l[1] = pData[0]; l[1] += pData[16*width];
        l[1] += pData[32*width]; l[1] += pData[48*width];
        pData += 64*width;
        l[2] = pData[0]; l[2] += pData[16*width];
        l[2] += pData[32*width]; l[2] += pData[48*width];
        pData += 64*width;
        l[3] = pData[0]; l[3] += pData[16*width];
        l[3] += pData[32*width]; l[3] += pData[48*width];
        j++;
        ver++;
        firstPhase[0] += l[0] + l[1] + l[2] + l[3];
        firstPhase[4] += l[0] + l[1] - l[2] - l[3];
			}
    if((col != width - 1) && (pMb+1)->decoded) {
        R = HANTRO_TRUE;
        pData = mbPos + 16;
        r[0] = pData[0]; r[0] += pData[16*width];
        r[0] += pData[32*width]; r[0] += pData[48*width];
        pData += 64*width;
        r[1] = pData[0]; r[1] += pData[16*width];
        r[1] += pData[32*width]; r[1] += pData[48*width];
        pData += 64*width;
        r[2] = pData[0]; r[2] += pData[16*width];
        r[2] += pData[32*width]; r[2] += pData[48*width];
        pData += 64*width;
        r[3] = pData[0]; r[3] += pData[16*width];
        r[3] += pData[32*width]; r[3] += pData[48*width];
        j++;
        ver++;
        firstPhase[0] += r[0] + r[1] + r[2] + r[3];
        firstPhase[4] += r[0] + r[1] - r[2] - r[3];
    }

    /* at least one properly decoded neighbour available */
    ASSERT(j);

    /*lint -esym(644,l,r,a,b) variable initialized above */
    if(!hor && L && R)
        firstPhase[1] = (l[0]+l[1]+l[2]+l[3]-r[0]-r[1]-r[2]-r[3]) >> 5;
    else if(hor)
        firstPhase[1] >>= (3+hor);

    if(!ver && A && B)
        firstPhase[4] = (a[0]+a[1]+a[2]+a[3]-b[0]-b[1]-b[2]-b[3]) >> 5;
    else if(ver)
        firstPhase[4] >>= (3+ver);

    switch(j) {
        case 1:
            firstPhase[0] >>= 4;
            break;
        case 2:
            firstPhase[0] >>= 5;
            break;
        case 3:
            /* approximate (firstPhase[0]*4/3)>>6 */
            firstPhase[0] = (21 * firstPhase[0]) >> 10;
            break;
        default: /* 4 */
            firstPhase[0] >>= 6;
            break;
			}


    Transform(firstPhase);

    for(i=0, pData = data, pTmp = firstPhase; i < 256;) {
        tmp = pTmp[(i & 0xF)>>2];
        /*lint -e734 CLIP1 macro results in value that fits into 8 bits */
        *pData++ = CLIP1(tmp);
        /*lint +e734 */

        i++;
        if(!(i & 0x3F))
            pTmp += 4;
			}

    /* chroma components */
    mbPos = currImage->data + width * height * 256 +
       row * 8 * width * 8 + col * 8;
    for(comp=0; comp < 2; comp++) {

        memset(firstPhase, 0, sizeof(firstPhase));

        /* counter for number of neighbours used */
        j=0;
        hor = ver=0;
        if(A) {
            pData = mbPos - width*8;
            a[0] = *pData++; a[0] += *pData++;
            a[1] = *pData++; a[1] += *pData++;
            a[2] = *pData++; a[2] += *pData++;
            a[3] = *pData++; a[3] += *pData++;
            j++;
            hor++;
            firstPhase[0] += a[0] + a[1] + a[2] + a[3];
            firstPhase[1] += a[0] + a[1] - a[2] - a[3];
					}
        if(B) {
            pData = mbPos + 8*width*8;
            b[0] = *pData++; b[0] += *pData++;
            b[1] = *pData++; b[1] += *pData++;
            b[2] = *pData++; b[2] += *pData++;
            b[3] = *pData++; b[3] += *pData++;
            j++;
            hor++;
            firstPhase[0] += b[0] + b[1] + b[2] + b[3];
            firstPhase[1] += b[0] + b[1] - b[2] - b[3];
					}
        if(L) {
            pData = mbPos - 1;
            l[0] = pData[0]; l[0] += pData[8*width];
            pData += 16*width;
            l[1] = pData[0]; l[1] += pData[8*width];
            pData += 16*width;
            l[2] = pData[0]; l[2] += pData[8*width];
            pData += 16*width;
            l[3] = pData[0]; l[3] += pData[8*width];
            j++;
            ver++;
            firstPhase[0] += l[0] + l[1] + l[2] + l[3];
            firstPhase[4] += l[0] + l[1] - l[2] - l[3];
					}
        if(R) {
            pData = mbPos + 8;
            r[0] = pData[0]; r[0] += pData[8*width];
            pData += 16*width;
            r[1] = pData[0]; r[1] += pData[8*width];
            pData += 16*width;
            r[2] = pData[0]; r[2] += pData[8*width];
            pData += 16*width;
            r[3] = pData[0]; r[3] += pData[8*width];
            j++;
            ver++;
            firstPhase[0] += r[0] + r[1] + r[2] + r[3];
            firstPhase[4] += r[0] + r[1] - r[2] - r[3];
				  }
        if(!hor && L && R)
            firstPhase[1] = (l[0]+l[1]+l[2]+l[3]-r[0]-r[1]-r[2]-r[3]) >> 4;
        else if(hor)
            firstPhase[1] >>= (2+hor);

        if(!ver && A && B)
            firstPhase[4] = (a[0]+a[1]+a[2]+a[3]-b[0]-b[1]-b[2]-b[3]) >> 4;
        else if(ver)
            firstPhase[4] >>= (2+ver);

        switch(j) {
          case 1:
            firstPhase[0] >>= 3;
            break;
          case 2:
            firstPhase[0] >>= 4;
            break;
          case 3:
            /* approximate (firstPhase[0]*4/3)>>5 */
            firstPhase[0] = (21 * firstPhase[0]) >> 9;
            break;
          default: /* 4 */
            firstPhase[0] >>= 5;
            break;
					}

        Transform(firstPhase);

        pData = data + 256 + comp*64;
        for(i=0, pTmp = firstPhase; i < 64;) {
          tmp = pTmp[(i & 0x7)>>1];
          /*lint -e734 CLIP1 macro results in value that fits into 8 bits */
          *pData++ = CLIP1(tmp);
          /*lint +e734 */

          i++;
          if(!(i & 0xF))
              pTmp += 4;
	        }

      /* increment pointers for cr */
      mbPos += width * height * 64;
			}

    h264bsdWriteMacroblock(currImage, data);

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function name: Transform

        Functional description:
            Simplified transform, assuming that only dc component and lowest
            horizontal and lowest vertical component may be non-zero

------------------------------------------------------------------------------*/
void Transform(int32_t *data) {
  uint32_t col;
  int32_t tmp0, tmp1;

  if(!data[1] && !data[4]) {
    data[1]  = data[2]  = data[3]  = data[4]  = data[5]  =
	    data[6]  = data[7]  = data[8]  = data[9]  = data[10] =
		  data[11] = data[12] = data[13] = data[14] = data[15] = data[0];
    return;
	  }

  /* first horizontal transform for rows 0 and 1 */
  tmp0 = data[0];
  tmp1 = data[1];
  data[0] = tmp0 + tmp1;
  data[1] = tmp0 + (tmp1>>1);
  data[2] = tmp0 - (tmp1>>1);
  data[3] = tmp0 - tmp1;

  tmp0 = data[4];
  data[5] = tmp0;
  data[6] = tmp0;
  data[7] = tmp0;

  /* then vertical transform */
  for(col = 4; col--; data++) {
    tmp0 = data[0];
    tmp1 = data[4];
    data[0] = tmp0 + tmp1;
    data[4] = tmp0 + (tmp1>>1);
    data[8] = tmp0 - (tmp1>>1);
    data[12] = tmp0 - tmp1;
    }

	}



#ifdef H264DEC_OMXDL
#include "omxtypes.h"
#include "omxVC.h"
#include "armVC.h"
#endif /* H264DEC_OMXDL */


/* array of alpha values, from the standard */
static const uint8_t alphas[52] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,5,6,7,8,9,10,
    12,13,15,17,20,22,25,28,32,36,40,45,50,56,63,71,80,90,101,113,127,144,162,
    182,203,226,255,255};

/* array of beta values, from the standard */
static const uint8_t betas[52] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,3,3,3,3,4,4,
    4,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,17,17,18,18};



#ifndef H264DEC_OMXDL
/* array of tc0 values, from the standard, each triplet corresponds to a
 * column in the table. Indexing goes as tc0[indexA][bS-1] */
static const uint8_t tc0[52][3] = {
	{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
	{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
	{0,0,0},{0,0,1},{0,0,1},{0,0,1},{0,0,1},{0,1,1},{0,1,1},{1,1,1},
	{1,1,1},{1,1,1},{1,1,1},{1,1,2},{1,1,2},{1,1,2},{1,1,2},{1,2,3},
	{1,2,3},{2,2,3},{2,2,4},{2,3,4},{2,3,4},{3,3,5},{3,4,6},{3,4,6},
	{4,5,7},{4,5,8},{4,6,9},{5,7,10},{6,8,11},{6,8,13},{7,10,14},{8,11,16},
	{9,12,18},{10,13,20},{11,15,23},{13,17,25}
	};
#else
/* array of tc0 values, from the standard, each triplet corresponds to a
 * column in the table. Indexing goes as tc0[indexA][bS] */
static const uint8_t tc0[52][5] = {
	{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 1, 0},
	{0, 0, 0, 1, 0}, {0, 0, 1, 1, 0}, {0, 0, 1, 1, 0}, {0, 1, 1, 1, 0},
	{0, 1, 1, 1, 0}, {0, 1, 1, 1, 0}, {0, 1, 1, 1, 0}, {0, 1, 1, 2, 0},
	{0, 1, 1, 2, 0}, {0, 1, 1, 2, 0}, {0, 1, 1, 2, 0}, {0, 1, 2, 3, 0},
	{0, 1, 2, 3, 0}, {0, 2, 2, 3, 0}, {0, 2, 2, 4, 0}, {0, 2, 3, 4, 0},
	{0, 2, 3, 4, 0}, {0, 3, 3, 5, 0}, {0, 3, 4, 6, 0}, {0, 3, 4, 6, 0},
	{0, 4, 5, 7, 0}, {0, 4, 5, 8, 0}, {0, 4, 6, 9, 0}, {0, 5, 7, 10, 0},
	{0, 6, 8, 11, 0}, {0, 6, 8, 13, 0}, {0, 7, 10, 14, 0},
	{0, 8, 11, 16, 0}, {0, 9, 12, 18, 0}, {0, 10, 13, 20, 0},
	{0, 11, 15, 23, 0}, {0, 13, 17, 25, 0}
	};
#endif


#ifndef H264DEC_OMXDL
/* mapping of raster scan block index to 4x4 block index */
static const uint32_t mb4x4Index[16] =
{0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15};

typedef struct {
    const uint8_t *tc0;
    uint32_t alpha;
    uint32_t beta;
} edgeThreshold_t;

typedef struct {
    uint32_t top;
    uint32_t left;
} bS_t;

enum { TOP=0, LEFT = 1, INNER = 2 };
#endif /* H264DEC_OMXDL */

#define FILTER_LEFT_EDGE    0x04
#define FILTER_TOP_EDGE     0x02
#define FILTER_INNER_EDGE   0x01


/* clipping table defined in intra_prediction.c */
extern const uint8_t h264bsdClip[];


static uint32_t InnerBoundaryStrength(mbStorage_t *mb1, uint32_t i1, uint32_t i2);

#ifndef H264DEC_OMXDL
static uint32_t EdgeBoundaryStrength(mbStorage_t *mb1, mbStorage_t *mb2,
    uint32_t i1, uint32_t i2);
#else
static uint32_t InnerBoundaryStrength2(mbStorage_t *mb1, uint32_t i1, uint32_t i2);
static uint32_t EdgeBoundaryStrengthLeft(mbStorage_t *mb1, mbStorage_t *mb2);
static uint32_t EdgeBoundaryStrengthTop(mbStorage_t *mb1, mbStorage_t *mb2);
#endif

static uint32_t IsSliceBoundaryOnLeft(mbStorage_t *mb);
static uint32_t IsSliceBoundaryOnTop(mbStorage_t *mb);
static uint32_t GetMbFilteringFlags(mbStorage_t *mb);

#ifndef H264DEC_OMXDL
static uint32_t GetBoundaryStrengths(mbStorage_t *mb, bS_t *bs, uint32_t flags);
static void FilterLuma(uint8_t *data, bS_t *bS, edgeThreshold_t *thresholds,
        uint32_t imageWidth);
static void FilterChroma(uint8_t *cb, uint8_t *cr, bS_t *bS, edgeThreshold_t *thresholds,
        uint32_t imageWidth);
static void FilterVerLumaEdge( uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds,
        uint32_t imageWidth);
static void FilterHorLumaEdge( uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds,
        int32_t imageWidth);
static void FilterHorLuma( uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds,
        int32_t imageWidth);
static void FilterVerChromaEdge( uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds,
  uint32_t imageWidth);
static void FilterHorChromaEdge( uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds,
  int32_t imageWidth);
static void FilterHorChroma( uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds,
  int32_t imageWidth);

static void GetLumaEdgeThresholds(edgeThreshold_t *thresholds,
  mbStorage_t *mb, uint32_t filteringFlags);
static void GetChromaEdgeThresholds(edgeThreshold_t *thresholds,
  mbStorage_t *mb,  uint32_t filteringFlags,
  int32_t chromaQpIndexOffset);

#else /* H264DEC_OMXDL */
static uint32_t GetBoundaryStrengths(mbStorage_t *mb, uint8_t (*bs)[16], uint32_t flags);

static void GetLumaEdgeThresholds(mbStorage_t *mb,
    uint8_t (*alpha)[2],    uint8_t (*beta)[2],
    uint8_t (*threshold)[16],
    uint8_t (*bs)[16],
    uint32_t filteringFlags );

static void GetChromaEdgeThresholds(mbStorage_t *mb,
    uint8_t (*alpha)[2],    uint8_t (*beta)[2],
    uint8_t (*threshold)[8],
    uint8_t (*bs)[16],
    uint32_t filteringFlags,
    int32_t chromaQpIndexOffset);
#endif /* H264DEC_OMXDL */

/*------------------------------------------------------------------------------

    Function: IsSliceBoundaryOnLeft

        Functional description:
            Function to determine if there is a slice boundary on the left side
            of a macroblock.

------------------------------------------------------------------------------*/
uint32_t IsSliceBoundaryOnLeft(mbStorage_t *mb) {

    ASSERT(mb && mb->mbA);

    if(mb->sliceId != mb->mbA->sliceId)
        return(HANTRO_TRUE);
    else
        return(HANTRO_FALSE);

	}

/*------------------------------------------------------------------------------

    Function: IsSliceBoundaryOnTop

        Functional description:
            Function to determine if there is a slice boundary above the
            current macroblock.

------------------------------------------------------------------------------*/
uint32_t IsSliceBoundaryOnTop(mbStorage_t *mb) {

    ASSERT(mb && mb->mbB);

    if(mb->sliceId != mb->mbB->sliceId)
        return(HANTRO_TRUE);
    else
        return(HANTRO_FALSE);

}

/*------------------------------------------------------------------------------

    Function: GetMbFilteringFlags

        Functional description:
          Function to determine which edges of a macroblock has to be
          filtered. Output is a bit-wise OR of FILTER_LEFT_EDGE,
          FILTER_TOP_EDGE and FILTER_INNER_EDGE, depending on which edges
          shall be filtered.

------------------------------------------------------------------------------*/
uint32_t GetMbFilteringFlags(mbStorage_t *mb) {
    uint32_t flags=0;


    ASSERT(mb);

    /* nothing will be filtered if disableDeblockingFilterIdc == 1 */
    if(mb->disableDeblockingFilterIdc != 1) {
        flags |= FILTER_INNER_EDGE;

        /* filterLeftMbEdgeFlag, left mb is MB_A */
        if(mb->mbA &&
            ((mb->disableDeblockingFilterIdc != 2) ||
             !IsSliceBoundaryOnLeft(mb)))
            flags |= FILTER_LEFT_EDGE;

        /* filterTopMbEdgeFlag */
        if(mb->mbB &&
            ((mb->disableDeblockingFilterIdc != 2) ||
             !IsSliceBoundaryOnTop(mb)))
            flags |= FILTER_TOP_EDGE;
    }

    return(flags);
	}

/*------------------------------------------------------------------------------

    Function: InnerBoundaryStrength

        Functional description:
            Function to calculate boundary strength value bs for an inner
            edge of a macroblock. Macroblock type is checked before this is
            called -> no intra mb condition here.

------------------------------------------------------------------------------*/
uint32_t InnerBoundaryStrength(mbStorage_t *mb1, uint32_t ind1, uint32_t ind2) {
    int32_t tmp1, tmp2;
    int32_t mv1, mv2, mv3, mv4;

    tmp1 = mb1->totalCoeff[ind1];
    tmp2 = mb1->totalCoeff[ind2];
    mv1 = mb1->mv[ind1].hor;
    mv2 = mb1->mv[ind2].hor;
    mv3 = mb1->mv[ind1].ver;
    mv4 = mb1->mv[ind2].ver;

    if(tmp1 || tmp2) {
        return 2;
    }
    else if(((uint32_t)ABS(mv1 - mv2) >= 4) || ((uint32_t)ABS(mv3 - mv4) >= 4) ||
              (mb1->refAddr[ind1 >> 2] != mb1->refAddr[ind2 >> 2])) {
        return 1;
    }
    else
        return 0;
}

/*------------------------------------------------------------------------------

    Function: InnerBoundaryStrength2

        Functional description:
            Function to calculate boundary strength value bs for an inner
            edge of a macroblock. The function is the same as
            InnerBoundaryStrength but without checking totalCoeff.

------------------------------------------------------------------------------*/
uint32_t InnerBoundaryStrength2(mbStorage_t *mb1, uint32_t ind1, uint32_t ind2) {
    int32_t tmp1, tmp2, tmp3, tmp4;

    tmp1 = mb1->mv[ind1].hor;
    tmp2 = mb1->mv[ind2].hor;
    tmp3 = mb1->mv[ind1].ver;
    tmp4 = mb1->mv[ind2].ver;

    if(((uint32_t)ABS(tmp1 - tmp2) >= 4) || ((uint32_t)ABS(tmp3 - tmp4) >= 4) ||
         (mb1->refAddr[ind1 >> 2] != mb1->refAddr[ind2 >> 2]))
        return 1;
    else
        return 0;
	}

#ifndef H264DEC_OMXDL
/*------------------------------------------------------------------------------

    Function: EdgeBoundaryStrength

        Functional description:
            Function to calculate boundary strength value bs for left- or
            top-most edge of a macroblock. Macroblock types are checked
            before this is called -> no intra mb conditions here.

------------------------------------------------------------------------------*/
uint32_t EdgeBoundaryStrength(mbStorage_t *mb1, mbStorage_t *mb2, uint32_t ind1, uint32_t ind2) {

    if(mb1->totalCoeff[ind1] || mb2->totalCoeff[ind2]) {
        return 2;
    }
    else if((mb1->refAddr[ind1 >> 2] != mb2->refAddr[ind2 >> 2]) ||
             ((uint32_t)ABS(mb1->mv[ind1].hor - mb2->mv[ind2].hor) >= 4) ||
             ((uint32_t)ABS(mb1->mv[ind1].ver - mb2->mv[ind2].ver) >= 4)) {
        return 1;
    }
    else
        return 0;
	}

#else /* H264DEC_OMXDL */

/*------------------------------------------------------------------------------

    Function: EdgeBoundaryStrengthTop

        Functional description:
            Function to calculate boundary strength value bs for
            top-most edge of a macroblock. Macroblock types are checked
            before this is called -> no intra mb conditions here.

------------------------------------------------------------------------------*/
uint32_t EdgeBoundaryStrengthTop(mbStorage_t *mb1, mbStorage_t *mb2) {
    uint32_t topBs=0;
    uint32_t tmp1, tmp2, tmp3, tmp4;

    tmp1 = mb1->totalCoeff[0];
    tmp2 = mb2->totalCoeff[10];
    tmp3 = mb1->totalCoeff[1];
    tmp4 = mb2->totalCoeff[11];
    if(tmp1 || tmp2) {
        topBs = 2 << 0;
			}
    else if(((uint32_t)ABS(mb1->mv[0].hor - mb2->mv[10].hor) >= 4) ||
             ((uint32_t)ABS(mb1->mv[0].ver - mb2->mv[10].ver) >= 4) ||
             (mb1->refAddr[0] != mb2->refAddr[10 >> 2])) {
        topBs = 1 << 0;
			}
    tmp1 = mb1->totalCoeff[4];
    tmp2 = mb2->totalCoeff[14];
    if(tmp3 || tmp4) {
        topBs += 2 << 8;
			}
    else if(((uint32_t)ABS(mb1->mv[1].hor - mb2->mv[11].hor) >= 4) ||
             ((uint32_t)ABS(mb1->mv[1].ver - mb2->mv[11].ver) >= 4) ||
             (mb1->refAddr[0] != mb2->refAddr[11 >> 2])) {
        topBs += 1<<8;
			}
    tmp3 = mb1->totalCoeff[5];
    tmp4 = mb2->totalCoeff[15];
    if(tmp1 || tmp2) {
        topBs += 2<<16;
			}
    else if(((uint32_t)ABS(mb1->mv[4].hor - mb2->mv[14].hor) >= 4) ||
             ((uint32_t)ABS(mb1->mv[4].ver - mb2->mv[14].ver) >= 4) ||
             (mb1->refAddr[4 >> 2] != mb2->refAddr[14 >> 2])) {
        topBs += 1<<16;
			}
    if(tmp3 || tmp4) {
        topBs += 2<<24;
			}
    else if(((uint32_t)ABS(mb1->mv[5].hor - mb2->mv[15].hor) >= 4) ||
             ((uint32_t)ABS(mb1->mv[5].ver - mb2->mv[15].ver) >= 4) ||
             (mb1->refAddr[5 >> 2] != mb2->refAddr[15 >> 2])) {
        topBs += 1<<24;
			}

    return topBs;
	}

/*------------------------------------------------------------------------------

    Function: EdgeBoundaryStrengthLeft

        Functional description:
            Function to calculate boundary strength value bs for left-
            edge of a macroblock. Macroblock types are checked
            before this is called -> no intra mb conditions here.

------------------------------------------------------------------------------*/
uint32_t EdgeBoundaryStrengthLeft(mbStorage_t *mb1, mbStorage_t *mb2) {
    uint32_t leftBs=0;
    uint32_t tmp1, tmp2, tmp3, tmp4;

    tmp1 = mb1->totalCoeff[0];
    tmp2 = mb2->totalCoeff[5];
    tmp3 = mb1->totalCoeff[2];
    tmp4 = mb2->totalCoeff[7];

    if(tmp1 || tmp2) {
        leftBs = 2<<0;
			}
    else if(((uint32_t)ABS(mb1->mv[0].hor - mb2->mv[5].hor) >= 4) ||
             ((uint32_t)ABS(mb1->mv[0].ver - mb2->mv[5].ver) >= 4) ||
             (mb1->refAddr[0] != mb2->refAddr[5 >> 2])) {
        leftBs = 1<<0;
			}
    tmp1 = mb1->totalCoeff[8];
    tmp2 = mb2->totalCoeff[13];
    if(tmp3 || tmp4) {
        leftBs += 2<<8;
			}
    else if(((uint32_t)ABS(mb1->mv[2].hor - mb2->mv[7].hor) >= 4) ||
             ((uint32_t)ABS(mb1->mv[2].ver - mb2->mv[7].ver) >= 4) ||
             (mb1->refAddr[0] != mb2->refAddr[7 >> 2])) {
        leftBs += 1<<8;
			}
    tmp3 = mb1->totalCoeff[10];
    tmp4 = mb2->totalCoeff[15];
    if(tmp1 || tmp2) {
        leftBs += 2<<16;
			}
    else if(((uint32_t)ABS(mb1->mv[8].hor - mb2->mv[13].hor) >= 4) ||
             ((uint32_t)ABS(mb1->mv[8].ver - mb2->mv[13].ver) >= 4) ||
             (mb1->refAddr[8 >> 2] != mb2->refAddr[13 >> 2])) {
        leftBs += 1<<16;
			}
    if(tmp3 || tmp4) {
        leftBs += 2<<24;
		  }
    else if(((uint32_t)ABS(mb1->mv[10].hor - mb2->mv[15].hor) >= 4) ||
             ((uint32_t)ABS(mb1->mv[10].ver - mb2->mv[15].ver) >= 4) ||
             (mb1->refAddr[10 >> 2] != mb2->refAddr[15 >> 2])) {
        leftBs += 1<<24;
	    }

  return leftBs;
	}
#endif /* H264DEC_OMXDL */

/*------------------------------------------------------------------------------

    Function: h264bsdFilterPicture

        Functional description:
          Perform deblocking filtering for a picture. Filter does not copy
          the original picture anywhere but filtering is performed directly
          on the original image. Parameters controlling the filtering process
          are computed based on information in macroblock structures of the
          filtered macroblock, macroblock above and macroblock on the left of
          the filtered one.

        Inputs:
          image         pointer to image to be filtered
          mb            pointer to macroblock data structure of the top-left
                        macroblock of the picture

        Outputs:
          image         filtered image stored here

        Returns:
          none

------------------------------------------------------------------------------*/
#ifndef H264DEC_OMXDL
void h264bsdFilterPicture(image_t *image, mbStorage_t *mb) {
    uint32_t flags;
    uint32_t picSizeInMbs, mbRow, mbCol;
    uint32_t picWidthInMbs;
    uint8_t *data;
    mbStorage_t *pMb;
    bS_t bS[16];
    edgeThreshold_t thresholds[3];


    ASSERT(image);
    ASSERT(mb);
    ASSERT(image->data);
    ASSERT(image->width);
    ASSERT(image->height);

    picWidthInMbs = image->width;
    data = image->data;
    picSizeInMbs = picWidthInMbs * image->height;

    pMb = mb;

    for(mbRow=0, mbCol=0; mbRow < image->height; pMb++) {
        flags = GetMbFilteringFlags(pMb);

        if(flags) {
            /* GetBoundaryStrengths function returns non-zero value if any of
             * the bS values for the macroblock being processed was non-zero */
            if(GetBoundaryStrengths(pMb, bS, flags)) {
                /* luma */
                GetLumaEdgeThresholds(thresholds, pMb, flags);
                data = image->data + mbRow * picWidthInMbs * 256 + mbCol * MACROBLOCK_SIZE;

                FilterLuma((uint8_t*)data, bS, thresholds, picWidthInMbs*16);

                /* chroma */
                GetChromaEdgeThresholds(thresholds, pMb, flags,
                    pMb->chromaQpIndexOffset);
                data = image->data + picSizeInMbs * 256 +
                    mbRow * picWidthInMbs * 64 + mbCol * 8;

                FilterChroma((uint8_t*)data, data + 64*picSizeInMbs, bS,
                        thresholds, picWidthInMbs*8);

            }
        }

        mbCol++;
        if(mbCol == picWidthInMbs) {
            mbCol=0;
            mbRow++;
        }
    }

	}

int sample=0;
unsigned int hashA=0;
unsigned int hashB=0;
unsigned int hashC=0;
unsigned int hashD=0;

/*------------------------------------------------------------------------------

    Function: FilterVerLumaEdge

        Functional description:
            Filter one vertical 4-pixel luma edge.

------------------------------------------------------------------------------*/
void FilterVerLumaEdge(uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds, uint32_t imageWidth) {
    int32_t delta, tc, tmp;
    uint32_t i;
    int32_t p0, q0, p1, q1, p2, q2;
    uint32_t tmpFlag;
    const uint8_t *clp = h264bsdClip + 512;

    uint32_t alpha = thresholds->alpha;
    uint32_t beta = thresholds->beta;
    int32_t val;


    ASSERT(data);
    ASSERT(bS && bS <= 4);
    ASSERT(thresholds);

    if(bS < 4) {
        tc = thresholds->tc0[bS-1];
        tmp = tc;
        for(i = 4; i; i--, data += imageWidth) {
            p1 = data[-2]; p0 = data[-1];
            q0 = data[0]; q1 = data[1];

            if(((uint32_t)ABS(p0 - q0) < alpha) &&
                 ((uint32_t)ABS(p1 - p0) < beta)  &&
                 ((uint32_t)ABS(q1 - q0) < beta)) {
                p2 = data[-3];
                q2 = data[2];

                if((uint32_t)ABS(p2 - p0) < beta) {
                    val = (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1;
                    data[-2] = (p1 + CLIP3(-tc, tc, val));
                    tmp++;
                }

                if((uint32_t)ABS(q2 - q0) < beta) {
                    val = (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1;
                    data[1] = (q1 + CLIP3(-tc, tc, val));
                    tmp++;
                }

                val = (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3;
                delta = CLIP3(-tmp, tmp, val);

                p0 = clp[p0 + delta];
                q0 = clp[q0 - delta];
                tmp = tc;
                data[-1] = p0;
                data[ 0] = q0;
            }
            // hashA += data[-2] + data[-1] + data[0] + data[1];
        }
	    }
    else {
        for(i = 4; i; i--, data += imageWidth) {
            p1 = data[-2]; p0 = data[-1];
            q0 = data[0]; q1 = data[1];
            if(((uint32_t)ABS(p0-q0) < alpha) &&
                 ((uint32_t)ABS(p1-p0) < beta)  &&
                 ((uint32_t)ABS(q1-q0) < beta)) {
                tmpFlag = ((uint32_t)ABS(p0 - q0) < ((alpha >> 2) +2)) ? HANTRO_TRUE : HANTRO_FALSE;

                p2 = data[-3];
                q2 = data[2];

                if(tmpFlag && (uint32_t)ABS(p2-p0) < beta) {
                    tmp = p1 + p0 + q0;
                    data[-1] = ((p2 + 2 * tmp + q1 + 4) >> 3);
                    data[-2] = ((p2 + tmp + 2) >> 2);
                    data[-3] = ((2 * data[-4] + 3 * p2 + tmp + 4) >> 3);
                }
                else
                    data[-1] = (2 * p1 + p0 + q1 + 2) >> 2;

                if(tmpFlag && (uint32_t)ABS(q2-q0) < beta) {
                    tmp = p0 + q0 + q1;
                    data[0] = ((p1 + 2 * tmp + q2 + 4) >> 3);
                    data[1] = ((tmp + q2 + 2) >> 2);
                    data[2] = ((2 * data[3] + 3 * q2 + tmp + 4) >> 3);
                }
                else
                    data[0] = ((2 * q1 + q0 + p1 + 2) >> 2);
            }
        }
    }

}

/*------------------------------------------------------------------------------

    Function: FilterHorLumaEdge

        Functional description:
            Filter one horizontal 4-pixel luma edge

------------------------------------------------------------------------------*/
void FilterHorLumaEdge(uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds, int32_t imageWidth) {
    int32_t delta, tc, tmp;
    uint32_t i;
    uint8_t p0, q0, p1, q1, p2, q2;
    const uint8_t *clp = h264bsdClip + 512;
    int32_t val;


    ASSERT(data);
    ASSERT(bS < 4);
    ASSERT(thresholds);

//    if(sample ++ % (1024 * 128) == 0) {
//        printf("Hash A: %d, Hash B: %d\n", hashA, hashB);
//    }

    tc = thresholds->tc0[bS-1];
    tmp = tc;
    for(i = 4; i; i--, data++) {
        p1 = data[-imageWidth*2]; p0 = data[-imageWidth];
        q0 = data[0]; q1 = data[imageWidth];
        if(((uint32_t)ABS(p0-q0) < thresholds->alpha) &&
             ((uint32_t)ABS(p1-p0) < thresholds->beta)  &&
             ((uint32_t)ABS(q1-q0) < thresholds->beta)) {
            p2 = data[-imageWidth*3];

            if((uint32_t)ABS(p2-p0) < thresholds->beta) {
                val = (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1;
                data[-imageWidth*2] = (p1 + CLIP3(-tc, tc, val));
                tmp++;
            }

            q2 = data[imageWidth*2];

            if((uint32_t)ABS(q2-q0) < thresholds->beta) {
                val = (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1;
                data[imageWidth] = (q1 + CLIP3(-tc, tc, val));
                tmp++;
            }

            val = ((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3);
            delta = CLIP3(-tmp, tmp, val);

            p0 = clp[p0 + delta];
            q0 = clp[q0 - delta];
            tmp = tc;
            data[-imageWidth] = p0;
            data[  0] = q0;
        }

        // hashB += data[-imageWidth*2] + data[-imageWidth] + data[0] + data[imageWidth];
    }
	}

/*------------------------------------------------------------------------------

    Function: FilterHorLuma

        Functional description:
            Filter all four successive horizontal 4-pixel luma edges. This can
            be done when bS is equal to all four edges.

------------------------------------------------------------------------------*/
void FilterHorLuma(uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds, int32_t imageWidth) {
    int32_t delta, tc, tmp;
    uint32_t i;
    int32_t p0, q0, p1, q1, p2, q2;
    uint32_t tmpFlag;
    const uint8_t *clp = h264bsdClip + 512;
    uint32_t alpha = thresholds->alpha;
    uint32_t beta = thresholds->beta;
    int32_t val;


    ASSERT(data);
    ASSERT(bS <= 4);
    ASSERT(thresholds);

//    if(sample ++ % (1024 * 64) == 0) {
//        printf("Hash A: %d, Hash B: %d\n", hashA, hashB);
//    }

    if(bS < 4) {
        tc = thresholds->tc0[bS-1];
        tmp = tc;
        for(i = 16; i; i--, data++) {
            p1 = data[-imageWidth*2]; p0 = data[-imageWidth];
            q0 = data[0]; q1 = data[imageWidth];
            if(((uint32_t)ABS(p0 - q0) < alpha) &&
                 ((uint32_t)ABS(p1 - p0) < beta)  &&
                 ((uint32_t)ABS(q1 - q0) < beta)) {
                p2 = data[-imageWidth*3];

                if((uint32_t)ABS(p2 - p0) < beta) {
                    val = (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1;
                    data[-imageWidth*2] = (uint8_t)(p1 + CLIP3(-tc, tc, val));
                    tmp++;
                }

                q2 = data[imageWidth*2];

                if((uint32_t)ABS(q2-q0) < beta) {
                    val = (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1;
                    data[imageWidth] = (uint8_t)(q1 + CLIP3(-tc, tc, val));
                    tmp++;
                }

                val = ((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3);
                delta = CLIP3(-tmp, tmp, val);

                p0 = clp[p0 + delta];
                q0 = clp[q0 - delta];
                tmp = tc;
                data[-imageWidth] = p0;
                data[  0] = q0;
            }
        }
    }
    else {
        for(i = 16; i; i--, data++) {
            p1 = data[-imageWidth*2]; p0 = data[-imageWidth];
            q0 = data[0]; q1 = data[imageWidth];
            if(((uint32_t)ABS(p0 - q0) < alpha) &&
                 ((uint32_t)ABS(p1 - p0) < beta)  &&
                 ((uint32_t)ABS(q1 - q0) < beta)) {
                tmpFlag = ((uint32_t)ABS(p0 - q0) < ((alpha >> 2) +2))
                            ? HANTRO_TRUE : HANTRO_FALSE;

                p2 = data[-imageWidth*3];
                q2 = data[imageWidth*2];

                if(tmpFlag && (uint32_t)ABS(p2 - p0) < beta) {
                    tmp = p1 + p0 + q0;
                    data[-imageWidth] = (uint8_t)((p2 + 2 * tmp + q1 + 4) >> 3);
                    data[-imageWidth*2] = (uint8_t)((p2 + tmp + 2) >> 2);
                    data[-imageWidth*3] = (uint8_t)((2 * data[-imageWidth*4] +
                                           3 * p2 + tmp + 4) >> 3);
                }
                else
                    data[-imageWidth] = (uint8_t)((2 * p1 + p0 + q1 + 2) >> 2);

                if(tmpFlag && (uint32_t)ABS(q2 - q0) < beta) {
                    tmp = p0 + q0 + q1;
                    data[ 0] = (uint8_t)((p1 + 2 * tmp + q2 + 4) >> 3);
                    data[imageWidth] = (uint8_t)((tmp + q2 + 2) >> 2);
                    data[imageWidth*2] = (uint8_t)((2 * data[imageWidth*3] +
                                          3 * q2 + tmp + 4) >> 3);
                }
                else
                    data[0] = (2 * q1 + q0 + p1 + 2) >> 2;
            }
        }
    }

    // hashA += data[-imageWidth*2] + data[-imageWidth] + data[0] + data[imageWidth];

	}

/*------------------------------------------------------------------------------

    Function: FilterVerChromaEdge

        Functional description:
            Filter one vertical 2-pixel chroma edge

------------------------------------------------------------------------------*/
void FilterVerChromaEdge(uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds, uint32_t width) {
    int32_t delta, tc;
    uint8_t p0, q0, p1, q1;
    const uint8_t *clp = h264bsdClip + 512;


    ASSERT(data);
    ASSERT(bS <= 4);
    ASSERT(thresholds);

    p1 = data[-2]; p0 = data[-1];
    q0 = data[0]; q1 = data[1];
    if(((uint32_t)ABS(p0-q0) < thresholds->alpha) &&
         ((uint32_t)ABS(p1-p0) < thresholds->beta)  &&
         ((uint32_t)ABS(q1-q0) < thresholds->beta)) {
        if(bS < 4) {
            tc = thresholds->tc0[bS-1] + 1;
            delta = CLIP3(-tc, tc, ((((q0 - p0) << 2) +
                      (p1 - q1) + 4) >> 3));
            p0 = clp[p0 + delta];
            q0 = clp[q0 - delta];
            data[-1] = p0;
            data[ 0] = q0;
        }
        else {
            data[-1] = (2 * p1 + p0 + q1 + 2) >> 2;
            data[ 0] = (2 * q1 + q0 + p1 + 2) >> 2;
        }
    }
    data += width;
    p1 = data[-2]; p0 = data[-1];
    q0 = data[0]; q1 = data[1];
    if(((uint32_t)ABS(p0-q0) < thresholds->alpha) &&
         ((uint32_t)ABS(p1-p0) < thresholds->beta)  &&
         ((uint32_t)ABS(q1-q0) < thresholds->beta)) {
        if(bS < 4) {
            tc = thresholds->tc0[bS-1] + 1;
            delta = CLIP3(-tc, tc, ((((q0 - p0) << 2) +
                      (p1 - q1) + 4) >> 3));
            p0 = clp[p0 + delta];
            q0 = clp[q0 - delta];
            data[-1] = p0;
            data[ 0] = q0;
        }
        else {
            data[-1] = (2 * p1 + p0 + q1 + 2) >> 2;
            data[ 0] = (2 * q1 + q0 + p1 + 2) >> 2;
        }
    }

	}

/*------------------------------------------------------------------------------

    Function: FilterHorChromaEdge

        Functional description:
            Filter one horizontal 2-pixel chroma edge

------------------------------------------------------------------------------*/
void FilterHorChromaEdge(uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds, int32_t width) {
    int32_t delta, tc;
    uint32_t i;
    uint8_t p0, q0, p1, q1;
    const uint8_t *clp = h264bsdClip + 512;


    ASSERT(data);
    ASSERT(bS < 4);
    ASSERT(thresholds);

    tc = thresholds->tc0[bS-1] + 1;
    for(i = 2; i; i--, data++) {
        p1 = data[-width*2]; p0 = data[-width];
        q0 = data[0]; q1 = data[width];
        if(((uint32_t)ABS(p0-q0) < thresholds->alpha) &&
             ((uint32_t)ABS(p1-p0) < thresholds->beta)  &&
             ((uint32_t)ABS(q1-q0) < thresholds->beta)) {
            delta = CLIP3(-tc, tc, ((((q0 - p0) << 2) +
                      (p1 - q1) + 4) >> 3));
            p0 = clp[p0 + delta];
            q0 = clp[q0 - delta];
            data[-width] = p0;
            data[  0] = q0;
        }
    }
	}

/*------------------------------------------------------------------------------

    Function: FilterHorChroma

        Functional description:
            Filter all four successive horizontal 2-pixel chroma edges. This
            can be done if bS is equal for all four edges.

------------------------------------------------------------------------------*/
void FilterHorChroma(uint8_t *data, uint32_t bS, edgeThreshold_t *thresholds, int32_t width) {
    int32_t delta, tc;
    uint32_t i;
    uint8_t p0, q0, p1, q1;
    const uint8_t *clp = h264bsdClip + 512;


    ASSERT(data);
    ASSERT(bS <= 4);
    ASSERT(thresholds);

    if(bS < 4) {
        tc = thresholds->tc0[bS-1] + 1;
        for(i = 8; i; i--, data++) {
            p1 = data[-width*2]; p0 = data[-width];
            q0 = data[0]; q1 = data[width];
            if(((uint32_t)ABS(p0-q0) < thresholds->alpha) &&
                 ((uint32_t)ABS(p1-p0) < thresholds->beta)  &&
                 ((uint32_t)ABS(q1-q0) < thresholds->beta)) {
                delta = CLIP3(-tc, tc, ((((q0 - p0) << 2) +
                          (p1 - q1) + 4) >> 3));
                p0 = clp[p0 + delta];
                q0 = clp[q0 - delta];
                data[-width] = p0;
                data[  0] = q0;
            }
        }
    }
    else {
        for(i = 8; i; i--, data++) {
            p1 = data[-width*2]; p0 = data[-width];
            q0 = data[0]; q1 = data[width];
            if(((uint32_t)ABS(p0-q0) < thresholds->alpha) &&
                 ((uint32_t)ABS(p1-p0) < thresholds->beta)  &&
                 ((uint32_t)ABS(q1-q0) < thresholds->beta)) {
                    data[-width] = (2 * p1 + p0 + q1 + 2) >> 2;
                    data[  0] = (2 * q1 + q0 + p1 + 2) >> 2;
            }
        }
    }

	}


void GetBoundaryStrengthsA(mbStorage_t *mb, bS_t *bS) {
    bS[4].top = mb->totalCoeff[2] || mb->totalCoeff[0] ? 2 : 0;
    bS[5].top = mb->totalCoeff[3] || mb->totalCoeff[1] ? 2 : 0;
    bS[6].top = mb->totalCoeff[6] || mb->totalCoeff[4] ? 2 : 0;
    bS[7].top = mb->totalCoeff[7] || mb->totalCoeff[5] ? 2 : 0;
    bS[8].top = mb->totalCoeff[8] || mb->totalCoeff[2] ? 2 : 0;
    bS[9].top = mb->totalCoeff[9] || mb->totalCoeff[3] ? 2 : 0;
    bS[10].top = mb->totalCoeff[12] || mb->totalCoeff[6] ? 2 : 0;
    bS[11].top = mb->totalCoeff[13] || mb->totalCoeff[7] ? 2 : 0;
    bS[12].top = mb->totalCoeff[10] || mb->totalCoeff[8] ? 2 : 0;
    bS[13].top = mb->totalCoeff[11] || mb->totalCoeff[9] ? 2 : 0;
    bS[14].top = mb->totalCoeff[14] || mb->totalCoeff[12] ? 2 : 0;
    bS[15].top = mb->totalCoeff[15] || mb->totalCoeff[13] ? 2 : 0;

    bS[1].left = mb->totalCoeff[1] || mb->totalCoeff[0] ? 2 : 0;
    bS[2].left = mb->totalCoeff[4] || mb->totalCoeff[1] ? 2 : 0;
    bS[3].left = mb->totalCoeff[5] || mb->totalCoeff[4] ? 2 : 0;
    bS[5].left = mb->totalCoeff[3] || mb->totalCoeff[2] ? 2 : 0;
    bS[6].left = mb->totalCoeff[6] || mb->totalCoeff[3] ? 2 : 0;
    bS[7].left = mb->totalCoeff[7] || mb->totalCoeff[6] ? 2 : 0;
    bS[9].left = mb->totalCoeff[9] || mb->totalCoeff[8] ? 2 : 0;
    bS[10].left = mb->totalCoeff[12] || mb->totalCoeff[9] ? 2 : 0;
    bS[11].left = mb->totalCoeff[13] || mb->totalCoeff[12] ? 2 : 0;
    bS[13].left = mb->totalCoeff[11] || mb->totalCoeff[10] ? 2 : 0;
    bS[14].left = mb->totalCoeff[14] || mb->totalCoeff[11] ? 2 : 0;
    bS[15].left = mb->totalCoeff[15] || mb->totalCoeff[14] ? 2 : 0;
	}

/*------------------------------------------------------------------------------

    Function: GetBoundaryStrengths

        Functional description:
            Function to calculate boundary strengths for all edges of a
            macroblock. Function returns HANTRO_TRUE if any of the bS values for
            the macroblock had non-zero value, HANTRO_FALSE otherwise.

------------------------------------------------------------------------------*/
uint32_t GetBoundaryStrengths(mbStorage_t *mb, bS_t *bS, uint32_t flags) {
    /* this flag is set HANTRO_TRUE as soon as any boundary strength value is
     * non-zero */
    uint32_t nonZeroBs = HANTRO_FALSE;


    ASSERT(mb);
    ASSERT(bS);
    ASSERT(flags);

//    if(sample ++ % (1024 * 128) == 0) {
//        printf("Hash A: %d, Hash B: %d, Hash C: %d, Hash D: %d\n", hashA, hashB, hashC, hashD);
//    }

    /* top edges */
    if(flags & FILTER_TOP_EDGE) {
        if(IS_INTRA_MB(*mb) || IS_INTRA_MB(*mb->mbB)) {
            bS[0].top = bS[1].top = bS[2].top = bS[3].top = 4;
            nonZeroBs = HANTRO_TRUE;
        }
        else {
            bS[0].top = EdgeBoundaryStrength(mb, mb->mbB, 0, 10);
            bS[1].top = EdgeBoundaryStrength(mb, mb->mbB, 1, 11);
            bS[2].top = EdgeBoundaryStrength(mb, mb->mbB, 4, 14);
            bS[3].top = EdgeBoundaryStrength(mb, mb->mbB, 5, 15);
            if(bS[0].top || bS[1].top || bS[2].top || bS[3].top)
                nonZeroBs = HANTRO_TRUE;
        }
		  }
    else {
        bS[0].top = bS[1].top = bS[2].top = bS[3].top=0;
			}

    /* left edges */
    if(flags & FILTER_LEFT_EDGE) {
        if(IS_INTRA_MB(*mb) || IS_INTRA_MB(*mb->mbA)) {
            bS[0].left = bS[4].left = bS[8].left = bS[12].left = 4;
            nonZeroBs = HANTRO_TRUE;
        }
        else {
            bS[0].left = EdgeBoundaryStrength(mb, mb->mbA, 0, 5);
            bS[4].left = EdgeBoundaryStrength(mb, mb->mbA, 2, 7);
            bS[8].left = EdgeBoundaryStrength(mb, mb->mbA, 8, 13);
            bS[12].left = EdgeBoundaryStrength(mb, mb->mbA, 10, 15);
            if(!nonZeroBs &&
                (bS[0].left || bS[4].left || bS[8].left || bS[12].left))
                nonZeroBs = HANTRO_TRUE;
        }
			}
    else {
        bS[0].left = bS[4].left = bS[8].left = bS[12].left=0;
			}

    /* inner edges */
    if(IS_INTRA_MB(*mb)) {
        bS[4].top  = bS[5].top  = bS[6].top  = bS[7].top  =
        bS[8].top  = bS[9].top  = bS[10].top = bS[11].top =
        bS[12].top = bS[13].top = bS[14].top = bS[15].top = 3;

        bS[1].left  = bS[2].left  = bS[3].left  =
        bS[5].left  = bS[6].left  = bS[7].left  =
        bS[9].left  = bS[10].left = bS[11].left =
        bS[13].left = bS[14].left = bS[15].left = 3;
        nonZeroBs = HANTRO_TRUE;
			}
    else {
        /* 16x16 inter mb -> ref addresses or motion vectors cannot differ,
         * only check if either of the blocks contain coefficients */
        if(h264bsdNumMbPart(mb->mbType) == 1) {
            GetBoundaryStrengthsA(mb, bS);
        }
        /* 16x8 inter mb -> ref addresses and motion vectors can be different
         * only for the middle horizontal edge, for the other top edges it is
         * enough to check whether the blocks contain coefficients or not. The
         * same applies to all internal left edges. */
        else if(mb->mbType == P_L0_L0_16x8) {
            bS[4].top = mb->totalCoeff[2] || mb->totalCoeff[0] ? 2 : 0;
            bS[5].top = mb->totalCoeff[3] || mb->totalCoeff[1] ? 2 : 0;
            bS[6].top = mb->totalCoeff[6] || mb->totalCoeff[4] ? 2 : 0;
            bS[7].top = mb->totalCoeff[7] || mb->totalCoeff[5] ? 2 : 0;
            bS[12].top = mb->totalCoeff[10] || mb->totalCoeff[8] ? 2 : 0;
            bS[13].top = mb->totalCoeff[11] || mb->totalCoeff[9] ? 2 : 0;
            bS[14].top = mb->totalCoeff[14] || mb->totalCoeff[12] ? 2 : 0;
            bS[15].top = mb->totalCoeff[15] || mb->totalCoeff[13] ? 2 : 0;
            bS[8].top = InnerBoundaryStrength(mb, 8, 2);
            bS[9].top = InnerBoundaryStrength(mb, 9, 3);
            bS[10].top = InnerBoundaryStrength(mb, 12, 6);
            bS[11].top = InnerBoundaryStrength(mb, 13, 7);

            bS[1].left = mb->totalCoeff[1] || mb->totalCoeff[0] ? 2 : 0;
            bS[2].left = mb->totalCoeff[4] || mb->totalCoeff[1] ? 2 : 0;
            bS[3].left = mb->totalCoeff[5] || mb->totalCoeff[4] ? 2 : 0;
            bS[5].left = mb->totalCoeff[3] || mb->totalCoeff[2] ? 2 : 0;
            bS[6].left = mb->totalCoeff[6] || mb->totalCoeff[3] ? 2 : 0;
            bS[7].left = mb->totalCoeff[7] || mb->totalCoeff[6] ? 2 : 0;
            bS[9].left = mb->totalCoeff[9] || mb->totalCoeff[8] ? 2 : 0;
            bS[10].left = mb->totalCoeff[12] || mb->totalCoeff[9] ? 2 : 0;
            bS[11].left = mb->totalCoeff[13] || mb->totalCoeff[12] ? 2 : 0;
            bS[13].left = mb->totalCoeff[11] || mb->totalCoeff[10] ? 2 : 0;
            bS[14].left = mb->totalCoeff[14] || mb->totalCoeff[11] ? 2 : 0;
            bS[15].left = mb->totalCoeff[15] || mb->totalCoeff[14] ? 2 : 0;
        }
        /* 8x16 inter mb -> ref addresses and motion vectors can be different
         * only for the middle vertical edge, for the other left edges it is
         * enough to check whether the blocks contain coefficients or not. The
         * same applies to all internal top edges. */
        else if(mb->mbType == P_L0_L0_8x16) {
            bS[4].top = mb->totalCoeff[2] || mb->totalCoeff[0] ? 2 : 0;
            bS[5].top = mb->totalCoeff[3] || mb->totalCoeff[1] ? 2 : 0;
            bS[6].top = mb->totalCoeff[6] || mb->totalCoeff[4] ? 2 : 0;
            bS[7].top = mb->totalCoeff[7] || mb->totalCoeff[5] ? 2 : 0;
            bS[8].top = mb->totalCoeff[8] || mb->totalCoeff[2] ? 2 : 0;
            bS[9].top = mb->totalCoeff[9] || mb->totalCoeff[3] ? 2 : 0;
            bS[10].top = mb->totalCoeff[12] || mb->totalCoeff[6] ? 2 : 0;
            bS[11].top = mb->totalCoeff[13] || mb->totalCoeff[7] ? 2 : 0;
            bS[12].top = mb->totalCoeff[10] || mb->totalCoeff[8] ? 2 : 0;
            bS[13].top = mb->totalCoeff[11] || mb->totalCoeff[9] ? 2 : 0;
            bS[14].top = mb->totalCoeff[14] || mb->totalCoeff[12] ? 2 : 0;
            bS[15].top = mb->totalCoeff[15] || mb->totalCoeff[13] ? 2 : 0;

            bS[1].left = mb->totalCoeff[1] || mb->totalCoeff[0] ? 2 : 0;
            bS[3].left = mb->totalCoeff[5] || mb->totalCoeff[4] ? 2 : 0;
            bS[5].left = mb->totalCoeff[3] || mb->totalCoeff[2] ? 2 : 0;
            bS[7].left = mb->totalCoeff[7] || mb->totalCoeff[6] ? 2 : 0;
            bS[9].left = mb->totalCoeff[9] || mb->totalCoeff[8] ? 2 : 0;
            bS[11].left = mb->totalCoeff[13] || mb->totalCoeff[12] ? 2 : 0;
            bS[13].left = mb->totalCoeff[11] || mb->totalCoeff[10] ? 2 : 0;
            bS[15].left = mb->totalCoeff[15] || mb->totalCoeff[14] ? 2 : 0;
            bS[2].left = InnerBoundaryStrength(mb, 4, 1);
            bS[6].left = InnerBoundaryStrength(mb, 6, 3);
            bS[10].left = InnerBoundaryStrength(mb, 12, 9);
            bS[14].left = InnerBoundaryStrength(mb, 14, 11);
        }
        else {
            bS[4].top = InnerBoundaryStrength(mb, mb4x4Index[4], mb4x4Index[0]);
            bS[5].top = InnerBoundaryStrength(mb, mb4x4Index[5], mb4x4Index[1]);
            bS[6].top = InnerBoundaryStrength(mb, mb4x4Index[6], mb4x4Index[2]);
            bS[7].top = InnerBoundaryStrength(mb, mb4x4Index[7], mb4x4Index[3]);
            bS[8].top = InnerBoundaryStrength(mb, mb4x4Index[8], mb4x4Index[4]);
            bS[9].top = InnerBoundaryStrength(mb, mb4x4Index[9], mb4x4Index[5]);
            bS[10].top = InnerBoundaryStrength(mb, mb4x4Index[10], mb4x4Index[6]);
            bS[11].top = InnerBoundaryStrength(mb, mb4x4Index[11], mb4x4Index[7]);
            bS[12].top = InnerBoundaryStrength(mb, mb4x4Index[12], mb4x4Index[8]);
            bS[13].top = InnerBoundaryStrength(mb, mb4x4Index[13], mb4x4Index[9]);
            bS[14].top = InnerBoundaryStrength(mb, mb4x4Index[14], mb4x4Index[10]);
            bS[15].top = InnerBoundaryStrength(mb, mb4x4Index[15], mb4x4Index[11]);

            bS[1].left = InnerBoundaryStrength(mb, mb4x4Index[1], mb4x4Index[0]);
            bS[2].left = InnerBoundaryStrength(mb, mb4x4Index[2], mb4x4Index[1]);
            bS[3].left = InnerBoundaryStrength(mb, mb4x4Index[3], mb4x4Index[2]);
            bS[5].left = InnerBoundaryStrength(mb, mb4x4Index[5], mb4x4Index[4]);
            bS[6].left = InnerBoundaryStrength(mb, mb4x4Index[6], mb4x4Index[5]);
            bS[7].left = InnerBoundaryStrength(mb, mb4x4Index[7], mb4x4Index[6]);
            bS[9].left = InnerBoundaryStrength(mb, mb4x4Index[9], mb4x4Index[8]);
            bS[10].left = InnerBoundaryStrength(mb, mb4x4Index[10], mb4x4Index[9]);
            bS[11].left = InnerBoundaryStrength(mb, mb4x4Index[11], mb4x4Index[10]);
            bS[13].left = InnerBoundaryStrength(mb, mb4x4Index[13], mb4x4Index[12]);
            bS[14].left = InnerBoundaryStrength(mb, mb4x4Index[14], mb4x4Index[13]);
            bS[15].left = InnerBoundaryStrength(mb, mb4x4Index[15], mb4x4Index[14]);
        }
        if(!nonZeroBs &&
            (bS[4].top || bS[5].top || bS[6].top || bS[7].top ||
             bS[8].top || bS[9].top || bS[10].top || bS[11].top ||
             bS[12].top || bS[13].top || bS[14].top || bS[15].top ||
             bS[1].left || bS[2].left || bS[3].left ||
             bS[5].left || bS[6].left || bS[7].left ||
             bS[9].left || bS[10].left || bS[11].left ||
             bS[13].left || bS[14].left || bS[15].left))
            nonZeroBs = HANTRO_TRUE;
    }

  return(nonZeroBs);
	}

/*------------------------------------------------------------------------------

    Function: GetLumaEdgeThresholds

        Functional description:
            Compute alpha, beta and tc0 thresholds for inner, left and top
            luma edges of a macroblock.

------------------------------------------------------------------------------*/
void GetLumaEdgeThresholds(edgeThreshold_t *thresholds,mbStorage_t *mb, uint32_t filteringFlags) {
    uint32_t indexA, indexB;
    uint32_t qpAv, qp, qpTmp;


    ASSERT(thresholds);
    ASSERT(mb);

    qp = mb->qpY;

    indexA = (uint32_t)CLIP3(0, 51, (int32_t)qp + mb->filterOffsetA);
    indexB = (uint32_t)CLIP3(0, 51, (int32_t)qp + mb->filterOffsetB);

    thresholds[INNER].alpha = alphas[indexA];
    thresholds[INNER].beta = betas[indexB];
    thresholds[INNER].tc0 = tc0[indexA];

    if(filteringFlags & FILTER_TOP_EDGE) {
        qpTmp = mb->mbB->qpY;
        if(qpTmp != qp) {
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetA);
            indexB = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetB);

            thresholds[TOP].alpha = alphas[indexA];
            thresholds[TOP].beta = betas[indexB];
            thresholds[TOP].tc0 = tc0[indexA];
        }
        else {
            thresholds[TOP].alpha = thresholds[INNER].alpha;
            thresholds[TOP].beta = thresholds[INNER].beta;
            thresholds[TOP].tc0 = thresholds[INNER].tc0;
        }
			}
    if(filteringFlags & FILTER_LEFT_EDGE) {
        qpTmp = mb->mbA->qpY;
        if(qpTmp != qp) {
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetA);
            indexB = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetB);

            thresholds[LEFT].alpha = alphas[indexA];
            thresholds[LEFT].beta = betas[indexB];
            thresholds[LEFT].tc0 = tc0[indexA];
        }
        else {
            thresholds[LEFT].alpha = thresholds[INNER].alpha;
            thresholds[LEFT].beta = thresholds[INNER].beta;
            thresholds[LEFT].tc0 = thresholds[INNER].tc0;
        }
    }

	}

/*------------------------------------------------------------------------------

    Function: GetChromaEdgeThresholds

        Functional description:
            Compute alpha, beta and tc0 thresholds for inner, left and top
            chroma edges of a macroblock.

------------------------------------------------------------------------------*/
void GetChromaEdgeThresholds(edgeThreshold_t *thresholds, mbStorage_t *mb,
  uint32_t filteringFlags, int32_t chromaQpIndexOffset) {
    uint32_t indexA, indexB;
    uint32_t qpAv, qp, qpTmp;


    ASSERT(thresholds);
    ASSERT(mb);

    qp = mb->qpY;
    qp = h264bsdQpC[CLIP3(0, 51, (int32_t)qp + chromaQpIndexOffset)];

    indexA = (uint32_t)CLIP3(0, 51, (int32_t)qp + mb->filterOffsetA);
    indexB = (uint32_t)CLIP3(0, 51, (int32_t)qp + mb->filterOffsetB);

    thresholds[INNER].alpha = alphas[indexA];
    thresholds[INNER].beta = betas[indexB];
    thresholds[INNER].tc0 = tc0[indexA];

    if(filteringFlags & FILTER_TOP_EDGE) {
        qpTmp = mb->mbB->qpY;
        if(qpTmp != mb->qpY) {
            qpTmp = h264bsdQpC[CLIP3(0, 51, (int32_t)qpTmp + chromaQpIndexOffset)];
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetA);
            indexB = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetB);

            thresholds[TOP].alpha = alphas[indexA];
            thresholds[TOP].beta = betas[indexB];
            thresholds[TOP].tc0 = tc0[indexA];
		      }
        else {
            thresholds[TOP].alpha = thresholds[INNER].alpha;
            thresholds[TOP].beta = thresholds[INNER].beta;
            thresholds[TOP].tc0 = thresholds[INNER].tc0;
        }
			}
    if(filteringFlags & FILTER_LEFT_EDGE) {
        qpTmp = mb->mbA->qpY;
        if(qpTmp != mb->qpY) {
            qpTmp = h264bsdQpC[CLIP3(0, 51, (int32_t)qpTmp + chromaQpIndexOffset)];
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetA);
            indexB = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetB);

            thresholds[LEFT].alpha = alphas[indexA];
            thresholds[LEFT].beta = betas[indexB];
            thresholds[LEFT].tc0 = tc0[indexA];
        }
        else {
            thresholds[LEFT].alpha = thresholds[INNER].alpha;
            thresholds[LEFT].beta = thresholds[INNER].beta;
            thresholds[LEFT].tc0 = thresholds[INNER].tc0;
        }
    }

	}

/*------------------------------------------------------------------------------

    Function: FilterLuma

        Functional description:
            Function to filter all luma edges of a macroblock

------------------------------------------------------------------------------*/
void FilterLuma(uint8_t *data, bS_t *bS, edgeThreshold_t *thresholds, uint32_t width) {
    uint32_t vblock;
    bS_t *tmp;
    uint8_t *ptr;
    uint32_t offset;


    ASSERT(data);
    ASSERT(bS);
    ASSERT(thresholds);

    ptr = data;
    tmp = bS;

    offset  = TOP;

    /* loop block rows, perform filtering for all vertical edges of the block
     * row first, then filter each horizontal edge of the row */
    for(vblock = 4; vblock--;) {
        /* only perform filtering if bS is non-zero, first of the four
         * FilterVerLumaEdge handles the left edge of the macroblock, others
         * filter inner edges */
        if(tmp[0].left)
            FilterVerLumaEdge(ptr, tmp[0].left, thresholds + LEFT, width);
        if(tmp[1].left)
            FilterVerLumaEdge(ptr+4, tmp[1].left, thresholds + INNER, width);
        if(tmp[2].left)
            FilterVerLumaEdge(ptr+8, tmp[2].left, thresholds + INNER, width);
        if(tmp[3].left)
            FilterVerLumaEdge(ptr+12, tmp[3].left, thresholds + INNER, width);

        /* if bS is equal for all horizontal edges of the row -> perform
         * filtering with FilterHorLuma, otherwise use FilterHorLumaEdge for
         * each edge separately. offset variable indicates top macroblock edge
         * on the first loop round, inner edge for the other rounds */
        if(tmp[0].top == tmp[1].top && tmp[1].top == tmp[2].top &&
            tmp[2].top == tmp[3].top) {
            if(tmp[0].top)
                FilterHorLuma(ptr, tmp[0].top, thresholds + offset, (int32_t)width);
        }
        else {
            if(tmp[0].top)
                FilterHorLumaEdge(ptr, tmp[0].top, thresholds+offset,
                    (int32_t)width);
            if(tmp[1].top)
                FilterHorLumaEdge(ptr+4, tmp[1].top, thresholds+offset,
                    (int32_t)width);
            if(tmp[2].top)
                FilterHorLumaEdge(ptr+8, tmp[2].top, thresholds+offset,
                    (int32_t)width);
            if(tmp[3].top)
                FilterHorLumaEdge(ptr+12, tmp[3].top, thresholds+offset,
                    (int32_t)width);
        }

        /* four pixel rows ahead, i.e. next row of 4x4-blocks */
        ptr += width*4;
        tmp += 4;
        offset = INNER;
    }
	}

/*------------------------------------------------------------------------------

    Function: FilterChroma

        Functional description:
            Function to filter all chroma edges of a macroblock

------------------------------------------------------------------------------*/
void FilterChroma(uint8_t *dataCb, uint8_t *dataCr, bS_t *bS, edgeThreshold_t *thresholds, uint32_t width) {
    uint32_t vblock;
    bS_t *tmp;
    uint32_t offset;

    ASSERT(dataCb);
    ASSERT(dataCr);
    ASSERT(bS);
    ASSERT(thresholds);

    tmp = bS;
    offset = TOP;

    /* loop block rows, perform filtering for all vertical edges of the block
     * row first, then filter each horizontal edge of the row */
    for(vblock=0; vblock < 2; vblock++) {
        /* only perform filtering if bS is non-zero, first two of the four
         * FilterVerChromaEdge calls handle the left edge of the macroblock,
         * others filter the inner edge. Note that as chroma uses bS values
         * determined for luma edges, each bS is used only for 2 pixels of
         * a 4-pixel edge */
        if(tmp[0].left) {
            FilterVerChromaEdge(dataCb, tmp[0].left, thresholds + LEFT, width);
            FilterVerChromaEdge(dataCr, tmp[0].left, thresholds + LEFT, width);
					}
        if(tmp[4].left) {
            FilterVerChromaEdge(dataCb+2*width, tmp[4].left, thresholds + LEFT,
                width);
            FilterVerChromaEdge(dataCr+2*width, tmp[4].left, thresholds + LEFT,
                width);
					}
        if(tmp[2].left) {
            FilterVerChromaEdge(dataCb+4, tmp[2].left, thresholds + INNER,
                width);
            FilterVerChromaEdge(dataCr+4, tmp[2].left, thresholds + INNER,
                width);
				  }
        if(tmp[6].left) {
            FilterVerChromaEdge(dataCb+2*width+4, tmp[6].left,
                thresholds + INNER, width);
            FilterVerChromaEdge(dataCr+2*width+4, tmp[6].left,
                thresholds + INNER, width);
			    }

        /* if bS is equal for all horizontal edges of the row -> perform
         * filtering with FilterHorChroma, otherwise use FilterHorChromaEdge
         * for each edge separately. offset variable indicates top macroblock
         * edge on the first loop round, inner edge for the second */
        if(tmp[0].top == tmp[1].top && tmp[1].top == tmp[2].top &&
            tmp[2].top == tmp[3].top) {
            if(tmp[0].top) {
                FilterHorChroma(dataCb, tmp[0].top, thresholds+offset,
                    (int32_t)width);
                FilterHorChroma(dataCr, tmp[0].top, thresholds+offset,
                    (int32_t)width);
            }
        }
        else {
            if(tmp[0].top) {
                FilterHorChromaEdge(dataCb, tmp[0].top, thresholds+offset,
                    (int32_t)width);
                FilterHorChromaEdge(dataCr, tmp[0].top, thresholds+offset,
                    (int32_t)width);
            }
            if(tmp[1].top) {
                FilterHorChromaEdge(dataCb+2, tmp[1].top, thresholds+offset,
                    (int32_t)width);
                FilterHorChromaEdge(dataCr+2, tmp[1].top, thresholds+offset,
                    (int32_t)width);
            }
            if(tmp[2].top) {
                FilterHorChromaEdge(dataCb+4, tmp[2].top, thresholds+offset,
                    (int32_t)width);
                FilterHorChromaEdge(dataCr+4, tmp[2].top, thresholds+offset,
                    (int32_t)width);
            }
            if(tmp[3].top) {
                FilterHorChromaEdge(dataCb+6, tmp[3].top, thresholds+offset,
                    (int32_t)width);
                FilterHorChromaEdge(dataCr+6, tmp[3].top, thresholds+offset,
                    (int32_t)width);
            }
        }

        tmp += 8;
        dataCb += width*4;
        dataCr += width*4;
        offset = INNER;
    }
	}

#else /* H264DEC_OMXDL */

/*------------------------------------------------------------------------------

    Function: h264bsdFilterPicture

        Functional description:
          Perform deblocking filtering for a picture. Filter does not copy
          the original picture anywhere but filtering is performed directly
          on the original image. Parameters controlling the filtering process
          are computed based on information in macroblock structures of the
          filtered macroblock, macroblock above and macroblock on the left of
          the filtered one.

        Inputs:
          image         pointer to image to be filtered
          mb            pointer to macroblock data structure of the top-left
                        macroblock of the picture

        Outputs:
          image         filtered image stored here

        Returns:
          none

------------------------------------------------------------------------------*/

void h264bsdFilterPicture(image_t *image, mbStorage_t *mb) {
  uint32_t flags;
  uint32_t picSizeInMbs, mbRow, mbCol;
  uint32_t picWidthInMbs;
  uint8_t *data;
  mbStorage_t *pMb;
  uint8_t bS[2][16];
  uint8_t thresholdLuma[2][16];
  uint8_t thresholdChroma[2][8];
  uint8_t alpha[2][2];
  uint8_t beta[2][2];
  OMXResult res;


  ASSERT(image);
  ASSERT(mb);
  ASSERT(image->data);
  ASSERT(image->width);
  ASSERT(image->height);

  picWidthInMbs = image->width;
  data = image->data;
  picSizeInMbs = picWidthInMbs * image->height;

  pMb = mb;

  for(mbRow=0, mbCol=0; mbRow < image->height; pMb++) {
    flags = GetMbFilteringFlags(pMb);

    if(flags) {
      /* GetBoundaryStrengths function returns non-zero value if any of
       * the bS values for the macroblock being processed was non-zero */
      if(GetBoundaryStrengths(pMb, bS, flags)) {

        /* Luma */
        GetLumaEdgeThresholds(pMb,alpha,beta,thresholdLuma,bS,flags);
        data = image->data + mbRow * picWidthInMbs * 256 + mbCol * MACROBLOCK_SIZE;

        res = omxVCM4P10_FilterDeblockingLuma_VerEdge_I( data,(OMX_S32)(picWidthInMbs*16),
                                        (const OMX_U8*)alpha,(const OMX_U8*)beta,
                                        (const OMX_U8*)thresholdLuma,
                                        (const OMX_U8*)bS );

        res = omxVCM4P10_FilterDeblockingLuma_HorEdge_I( data,(OMX_S32)(picWidthInMbs*16),
                                        (const OMX_U8*)alpha+2,(const OMX_U8*)beta+2,
                                        (const OMX_U8*)thresholdLuma+16,
                                        (const OMX_U8*)bS+16 );
        /* Cb */
        GetChromaEdgeThresholds(pMb, alpha, beta, thresholdChroma,
                                bS, flags, pMb->chromaQpIndexOffset);
        data = image->data + picSizeInMbs * 256 +
            mbRow * picWidthInMbs * 64 + mbCol * 8;

        res = omxVCM4P10_FilterDeblockingChroma_VerEdge_I( data,(OMX_S32)(picWidthInMbs*8),
                                      (const OMX_U8*)alpha,(const OMX_U8*)beta,
                                      (const OMX_U8*)thresholdChroma,
                                      (const OMX_U8*)bS );
        res = omxVCM4P10_FilterDeblockingChroma_HorEdge_I( data,(OMX_S32)(picWidthInMbs*8),
                                      (const OMX_U8*)alpha+2,(const OMX_U8*)beta+2,
                                      (const OMX_U8*)thresholdChroma+8,
                                      (const OMX_U8*)bS+16 );
        /* Cr */
        data += (picSizeInMbs * 64);
        res = omxVCM4P10_FilterDeblockingChroma_VerEdge_I( data,
                                      (OMX_S32)(picWidthInMbs*8),
                                      (const OMX_U8*)alpha,(const OMX_U8*)beta,
                                      (const OMX_U8*)thresholdChroma,
                                      (const OMX_U8*)bS );
        res = omxVCM4P10_FilterDeblockingChroma_HorEdge_I( data,(OMX_S32)(picWidthInMbs*8),
                                      (const OMX_U8*)alpha+2,(const OMX_U8*)beta+2,
                                      (const OMX_U8*)thresholdChroma+8,
                                      (const OMX_U8*)bS+16 );
        }
      }

    mbCol++;
    if(mbCol == picWidthInMbs) {
      mbCol=0;
      mbRow++;
      }
		}

	}

/*------------------------------------------------------------------------------

    Function: GetBoundaryStrengths

        Functional description:
            Function to calculate boundary strengths for all edges of a
            macroblock. Function returns HANTRO_TRUE if any of the bS values for
            the macroblock had non-zero value, HANTRO_FALSE otherwise.

------------------------------------------------------------------------------*/
uint32_t GetBoundaryStrengths(mbStorage_t *mb, uint8_t (*bS)[16], uint32_t flags) {
    /* this flag is set HANTRO_TRUE as soon as any boundary strength value is
     * non-zero */
    uint32_t nonZeroBs = HANTRO_FALSE;
    uint32_t *pTmp;
    uint32_t tmp1, tmp2, isIntraMb;

    ASSERT(mb);
    ASSERT(bS);
    ASSERT(flags);

    isIntraMb = IS_INTRA_MB(*mb);

    /* top edges */
    pTmp = (uint32_t*)&bS[1][0];
    if(flags & FILTER_TOP_EDGE) {
        if(isIntraMb || IS_INTRA_MB(*mb->mbB)) {
            *pTmp=0x04040404;
            nonZeroBs = HANTRO_TRUE;
        }
        else {
            *pTmp = EdgeBoundaryStrengthTop(mb, mb->mbB);
            if(*pTmp)
                nonZeroBs = HANTRO_TRUE;
        }
	    }
    else
        *pTmp=0;

    /* left edges */
    pTmp = (uint32_t*)&bS[0][0];
    if(flags & FILTER_LEFT_EDGE) {
        if(isIntraMb || IS_INTRA_MB(*mb->mbA)) {
            /*bS[0][0] = bS[0][1] = bS[0][2] = bS[0][3] = 4;*/
            *pTmp=0x04040404;
            nonZeroBs = HANTRO_TRUE;
        }
        else {
            *pTmp = EdgeBoundaryStrengthLeft(mb, mb->mbA);
            if(!nonZeroBs && *pTmp)
                nonZeroBs = HANTRO_TRUE;
        }
    }
    else
      *pTmp=0;

    /* inner edges */
    if(isIntraMb) {
        pTmp++;
        *pTmp++=0x03030303;
        *pTmp++=0x03030303;
        *pTmp++=0x03030303;
        pTmp++;
        *pTmp++=0x03030303;
        *pTmp++=0x03030303;
        *pTmp=0x03030303;

        nonZeroBs = HANTRO_TRUE;
			}
    else {
        pTmp = (uint32_t*)mb->totalCoeff;

        /* 16x16 inter mb -> ref addresses or motion vectors cannot differ,
         * only check if either of the blocks contain coefficients */
        if(h264bsdNumMbPart(mb->mbType) == 1) {
            tmp1 = *pTmp++;
            tmp2 = *pTmp++;
            bS[1][4]  = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [2]  || [0] */
            bS[1][5]  = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [3]  || [1] */
            bS[0][4]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [1]  || [0] */
            bS[0][5]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [3]  || [2] */

            tmp1 = *pTmp++;
            bS[1][6]  = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [6]  || [4] */
            bS[1][7]  = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [7]  || [5] */
            bS[0][12] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [5]  || [4] */
            bS[0][13] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [7]  || [6] */
            tmp2 = *pTmp;
            bS[1][12] = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [10] || [8] */
            bS[1][13] = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [11] || [9] */
            bS[0][6]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [9]  || [8] */
            bS[0][7]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [11] || [10] */

            bS[1][14] = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [14] || [12] */
            bS[1][15] = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [15] || [13] */
            bS[0][14] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [13] || [12] */
            bS[0][15] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [15] || [14] */

						{
            uint32_t tmp3, tmp4;

            tmp1 = mb->totalCoeff[8];
            tmp2 = mb->totalCoeff[2];
            tmp3 = mb->totalCoeff[9];
            tmp4 = mb->totalCoeff[3];

            bS[1][8] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[12];
            tmp2 = mb->totalCoeff[6];
            bS[1][9] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[13];
            tmp4 = mb->totalCoeff[7];
            bS[1][10] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[4];
            tmp2 = mb->totalCoeff[1];
            bS[1][11] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[6];
            tmp4 = mb->totalCoeff[3];
            bS[0][8] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[12];
            tmp2 = mb->totalCoeff[9];
            bS[0][9] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[14];
            tmp4 = mb->totalCoeff[11];
            bS[0][10] = tmp1 || tmp2 ? 2 : 0;
            bS[0][11] = tmp3 || tmp4 ? 2 : 0;
            }
        }

        /* 16x8 inter mb -> ref addresses and motion vectors can be different
         * only for the middle horizontal edge, for the other top edges it is
         * enough to check whether the blocks contain coefficients or not. The
         * same applies to all internal left edges. */
        else if(mb->mbType == P_L0_L0_16x8) {
            tmp1 = *pTmp++;
            tmp2 = *pTmp++;
            bS[1][4]  = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [2]  || [0] */
            bS[1][5]  = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [3]  || [1] */
            bS[0][4]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [1]  || [0] */
            bS[0][5]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [3]  || [2] */
            tmp1 = *pTmp++;
            bS[1][6]  = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [6]  || [4] */
            bS[1][7]  = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [7]  || [5] */
            bS[0][12] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [5]  || [4] */
            bS[0][13] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [7]  || [6] */
            tmp2 = *pTmp;
            bS[1][12] = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [10] || [8] */
            bS[1][13] = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [11] || [9] */
            bS[0][6]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [9]  || [8] */
            bS[0][7]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [11] || [10] */

            bS[1][14] = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [14] || [12] */
            bS[1][15] = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [15] || [13] */
            bS[0][14] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [13] || [12] */
            bS[0][15] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [15] || [14] */

            bS[1][8] = (uint8_t)InnerBoundaryStrength(mb, 8, 2);
            bS[1][9] = (uint8_t)InnerBoundaryStrength(mb, 9, 3);
            bS[1][10] = (uint8_t)InnerBoundaryStrength(mb, 12, 6);
            bS[1][11] = (uint8_t)InnerBoundaryStrength(mb, 13, 7);

						{
            uint32_t tmp3, tmp4;

            tmp1 = mb->totalCoeff[4];
            tmp2 = mb->totalCoeff[1];
            tmp3 = mb->totalCoeff[6];
            tmp4 = mb->totalCoeff[3];
            bS[0][8] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[12];
            tmp2 = mb->totalCoeff[9];
            bS[0][9] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[14];
            tmp4 = mb->totalCoeff[11];
            bS[0][10] = tmp1 || tmp2 ? 2 : 0;
            bS[0][11] = tmp3 || tmp4 ? 2 : 0;
            }
        }
        /* 8x16 inter mb -> ref addresses and motion vectors can be different
         * only for the middle vertical edge, for the other left edges it is
         * enough to check whether the blocks contain coefficients or not. The
         * same applies to all internal top edges. */
        else if(mb->mbType == P_L0_L0_8x16) {
            tmp1 = *pTmp++;
            tmp2 = *pTmp++;
            bS[1][4]  = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [2]  || [0] */
            bS[1][5]  = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [3]  || [1] */
            bS[0][4]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [1]  || [0] */
            bS[0][5]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [3]  || [2] */
            tmp1 = *pTmp++;
            bS[1][6]  = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [6]  || [4] */
            bS[1][7]  = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [7]  || [5] */
            bS[0][12] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [5]  || [4] */
            bS[0][13] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [7]  || [6] */
            tmp2 = *pTmp;
            bS[1][12] = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [10] || [8] */
            bS[1][13] = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [11] || [9] */
            bS[0][6]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [9]  || [8] */
            bS[0][7]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [11] || [10] */

            bS[1][14] = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [14] || [12] */
            bS[1][15] = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [15] || [13] */
            bS[0][14] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [13] || [12] */
            bS[0][15] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [15] || [14] */

            bS[0][8] = (uint8_t)InnerBoundaryStrength(mb, 4, 1);
            bS[0][9] = (uint8_t)InnerBoundaryStrength(mb, 6, 3);
            bS[0][10] = (uint8_t)InnerBoundaryStrength(mb, 12, 9);
            bS[0][11] = (uint8_t)InnerBoundaryStrength(mb, 14, 11);

						{
            uint32_t tmp3, tmp4;

            tmp1 = mb->totalCoeff[8];
            tmp2 = mb->totalCoeff[2];
            tmp3 = mb->totalCoeff[9];
            tmp4 = mb->totalCoeff[3];
            bS[1][8] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[12];
            tmp2 = mb->totalCoeff[6];
            bS[1][9] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[13];
            tmp4 = mb->totalCoeff[7];
            bS[1][10] = tmp1 || tmp2 ? 2 : 0;
            bS[1][11] = tmp3 || tmp4 ? 2 : 0;
            }
        }
        else {
            tmp1 = *pTmp++;
            bS[1][4] = (tmp1 & 0x00FF00FF) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 2, 0);
            bS[1][5] = (tmp1 & 0xFF00FF00) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 3, 1);
            bS[0][4] = (tmp1 & 0x0000FFFF) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 1, 0);
            bS[0][5] = (tmp1 & 0xFFFF0000) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 3, 2);
            tmp1 = *pTmp++;
            bS[1][6]  = (tmp1 & 0x00FF00FF) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 6, 4);
            bS[1][7]  = (tmp1 & 0xFF00FF00) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 7, 5);
            bS[0][12] = (tmp1 & 0x0000FFFF) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 5, 4);
            bS[0][13] = (tmp1 & 0xFFFF0000) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 7, 6);
            tmp1 = *pTmp++;
            bS[1][12] = (tmp1 & 0x00FF00FF) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 10, 8);
            bS[1][13] = (tmp1 & 0xFF00FF00) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 11, 9);
            bS[0][6]  = (tmp1 & 0x0000FFFF) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 9, 8);
            bS[0][7]  = (tmp1 & 0xFFFF0000) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 11, 10);
            tmp1 = *pTmp;
            bS[1][14] = (tmp1 & 0x00FF00FF) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 14, 12);
            bS[1][15] = (tmp1 & 0xFF00FF00) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 15, 13);
            bS[0][14] = (tmp1 & 0x0000FFFF) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 13, 12);
            bS[0][15] = (tmp1 & 0xFFFF0000) ? 2 : (uint8_t)InnerBoundaryStrength2(mb, 15, 14);

            bS[1][8] = (uint8_t)InnerBoundaryStrength(mb, 8, 2);
            bS[1][9] = (uint8_t)InnerBoundaryStrength(mb, 9, 3);
            bS[1][10] = (uint8_t)InnerBoundaryStrength(mb, 12, 6);
            bS[1][11] = (uint8_t)InnerBoundaryStrength(mb, 13, 7);

            bS[0][8] = (uint8_t)InnerBoundaryStrength(mb, 4, 1);
            bS[0][9] = (uint8_t)InnerBoundaryStrength(mb, 6, 3);
            bS[0][10] = (uint8_t)InnerBoundaryStrength(mb, 12, 9);
            bS[0][11] = (uint8_t)InnerBoundaryStrength(mb, 14, 11);
        }
        pTmp = (uint32_t*)&bS[0][0];
        if(!nonZeroBs && (pTmp[1] || pTmp[2] || pTmp[3] ||
                           pTmp[5] || pTmp[6] || pTmp[7])) {
            nonZeroBs = HANTRO_TRUE;
        }
    }

  return nonZeroBs;
	}

/*------------------------------------------------------------------------------

    Function: GetLumaEdgeThresholds

        Functional description:
            Compute alpha, beta and tc0 thresholds for inner, left and top
            luma edges of a macroblock.

------------------------------------------------------------------------------*/
void GetLumaEdgeThresholds(mbStorage_t *mb,
    uint8_t (*alpha)[2],    uint8_t (*beta)[2],
    uint8_t (*threshold)[16],
    uint8_t (*bs)[16],
    uint32_t filteringFlags ) {
    uint32_t indexA, indexB;
    uint32_t qpAv, qp, qpTmp;
    uint32_t i;

    ASSERT(threshold);
    ASSERT(bs);
    ASSERT(beta);
    ASSERT(alpha);
    ASSERT(mb);

    qp = mb->qpY;

    indexA = (uint32_t)CLIP3(0, 51, (int32_t)qp + mb->filterOffsetA);
    indexB = (uint32_t)CLIP3(0, 51, (int32_t)qp + mb->filterOffsetB);

    /* Internal edge values */
    alpha[0][1] = alphas[indexA];
    alpha[1][1] = alphas[indexA];
    alpha[1][0] = alphas[indexA];
    alpha[0][0] = alphas[indexA];
    beta[0][1] = betas[indexB];
    beta[1][1] = betas[indexB];
    beta[1][0] = betas[indexB];
    beta[0][0] = betas[indexB];

    /* vertical scan order */
    for(i=0; i < 2; i++) {
        uint32_t t1, t2;

        t1 = bs[i][0];
        t2 = bs[i][1];
        threshold[i][0]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][2];
        threshold[i][1]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][3];
        threshold[i][2]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][4];
        threshold[i][3]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][5];
        threshold[i][4]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][6];
        threshold[i][5]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][7];
        threshold[i][6]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][8];
        threshold[i][7]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][9];
        threshold[i][8]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][10];
        threshold[i][9]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][11];
        threshold[i][10] = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][12];
        threshold[i][11] = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][13];
        threshold[i][12] = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][14];
        threshold[i][13] = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][15];
        threshold[i][14] = (t1) ? tc0[indexA][t1] : 0;
        threshold[i][15] = (t2) ? tc0[indexA][t2] : 0;
    }

    if(filteringFlags & FILTER_TOP_EDGE) {
        qpTmp = mb->mbB->qpY;
        if(qpTmp != qp) {
            uint32_t t1, t2, t3, t4;
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetA);
            indexB = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetB);

            alpha[1][0] = alphas[indexA];
            beta[1][0] = betas[indexB];
            t1 = bs[1][0];
            t2 = bs[1][1];
            t3 = bs[1][2];
            t4 = bs[1][3];
            threshold[1][0] = (t1 && (t1 < 4)) ? tc0[indexA][t1] : 0;
            threshold[1][1] = (t2 && (t2 < 4)) ? tc0[indexA][t2] : 0;
            threshold[1][2] = (t3 && (t3 < 4)) ? tc0[indexA][t3] : 0;
            threshold[1][3] = (t4 && (t4 < 4)) ? tc0[indexA][t4] : 0;
        }
			}
    if(filteringFlags & FILTER_LEFT_EDGE) {
        qpTmp = mb->mbA->qpY;
        if(qpTmp != qp) {
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetA);
            indexB = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetB);

            alpha[0][0] = alphas[indexA];
            beta[0][0] = betas[indexB];
            threshold[0][0] = (bs[0][0] && (bs[0][0] < 4)) ? tc0[indexA][bs[0][0]] : 0;
            threshold[0][1] = (bs[0][1] && (bs[0][1] < 4)) ? tc0[indexA][bs[0][1]] : 0;
            threshold[0][2] = (bs[0][2] && (bs[0][2] < 4)) ? tc0[indexA][bs[0][2]] : 0;
            threshold[0][3] = (bs[0][3] && (bs[0][3] < 4)) ? tc0[indexA][bs[0][3]] : 0;
        }
			}

	}

/*------------------------------------------------------------------------------

    Function: GetChromaEdgeThresholds

        Functional description:
            Compute alpha, beta and tc0 thresholds for inner, left and top
            chroma edges of a macroblock.

------------------------------------------------------------------------------*/
void GetChromaEdgeThresholds(mbStorage_t *mb,
    uint8_t (*alpha)[2],    uint8_t (*beta)[2],
    uint8_t (*threshold)[8],
    uint8_t (*bs)[16],
    uint32_t filteringFlags,   int32_t chromaQpIndexOffset) {
    uint32_t indexA, indexB;
    uint32_t qpAv, qp, qpTmp;
    uint32_t i;


    ASSERT(threshold);
    ASSERT(bs);
    ASSERT(beta);
    ASSERT(alpha);
    ASSERT(mb);
    ASSERT(mb);

    qp = mb->qpY;
    qp = h264bsdQpC[CLIP3(0, 51, (int32_t)qp + chromaQpIndexOffset)];

    indexA = (uint32_t)CLIP3(0, 51, (int32_t)qp + mb->filterOffsetA);
    indexB = (uint32_t)CLIP3(0, 51, (int32_t)qp + mb->filterOffsetB);

    alpha[0][1] = alphas[indexA];
    alpha[1][1] = alphas[indexA];
    alpha[1][0] = alphas[indexA];
    alpha[0][0] = alphas[indexA];
    beta[0][1] = betas[indexB];
    beta[1][1] = betas[indexB];
    beta[1][0] = betas[indexB];
    beta[0][0] = betas[indexB];

    for(i=0; i < 2; i++) {
        uint32_t t1, t2;

        t1 = bs[i][0];
        t2 = bs[i][1];
        threshold[i][0]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][2];
        threshold[i][1]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][3];
        threshold[i][2]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][8];
        threshold[i][3]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][9];
        threshold[i][4]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][10];
        threshold[i][5]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][11];
        threshold[i][6]  = (t1) ? tc0[indexA][t1] : 0;
        threshold[i][7]  = (t2) ? tc0[indexA][t2] : 0;
    }

    if(filteringFlags & FILTER_TOP_EDGE) {
        qpTmp = mb->mbB->qpY;
        if(qpTmp != mb->qpY) {
            uint32_t t1, t2, t3, t4;
            qpTmp = h264bsdQpC[CLIP3(0, 51, (int32_t)qpTmp + chromaQpIndexOffset)];
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetA);
            indexB = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetB);

            alpha[1][0] = alphas[indexA];
            beta[1][0] = betas[indexB];

            t1 = bs[1][0];
            t2 = bs[1][1];
            t3 = bs[1][2];
            t4 = bs[1][3];
            threshold[1][0] = (t1) ? tc0[indexA][t1] : 0;
            threshold[1][1] = (t2) ? tc0[indexA][t2] : 0;
            threshold[1][2] = (t3) ? tc0[indexA][t3] : 0;
            threshold[1][3] = (t4) ? tc0[indexA][t4] : 0;
				  }
			}
    if(filteringFlags & FILTER_LEFT_EDGE) {
        qpTmp = mb->mbA->qpY;
        if(qpTmp != mb->qpY) {

            qpTmp = h264bsdQpC[CLIP3(0, 51, (int32_t)qpTmp + chromaQpIndexOffset)];
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetA);
            indexB = (uint32_t)CLIP3(0, 51, (int32_t)qpAv + mb->filterOffsetB);

            alpha[0][0] = alphas[indexA];
            beta[0][0] = betas[indexB];
            threshold[0][0] = (bs[0][0]) ? tc0[indexA][bs[0][0]] : 0;
            threshold[0][1] = (bs[0][1]) ? tc0[indexA][bs[0][1]] : 0;
            threshold[0][2] = (bs[0][2]) ? tc0[indexA][bs[0][2]] : 0;
            threshold[0][3] = (bs[0][3]) ? tc0[indexA][bs[0][3]] : 0;
        }
    }

	}

#endif /* H264DEC_OMXDL */




uint32_t h264bsdInit(storage_t *pStorage, uint32_t noOutputReordering) {
  uint32_t size;

  ASSERT(pStorage);

  h264bsdInitStorage(pStorage);

  /* allocate mbLayer to be next multiple of 64 to enable use of
   * specific NEON optimized "memset" for clearing the structure */
  size = (sizeof(macroblockLayer_t) + 63) & ~0x3F;

  pStorage->mbLayer = (macroblockLayer_t*)malloc(size);
  if(!pStorage->mbLayer)
      return HANTRO_NOK;

  if(noOutputReordering)
      pStorage->noReordering = HANTRO_TRUE;

  return HANTRO_OK;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdDecode

        Functional description:
            Decode a NAL unit. This function calls other modules to perform
            tasks like
                * extract and decode NAL unit from the byte stream
                * decode parameter sets
                * decode slice header and slice data
                * conceal errors in the picture
                * perform deblocking filtering

            This function contains top level control logic of the decoder.

        Inputs:
            pStorage        pointer to storage data structure
            byteStrm        pointer to stream buffer given by application
            len             length of the buffer in bytes
            picId           identifier for a picture, assigned by the
                            application

        Outputs:
            readBytes       number of bytes read from the stream is stored
                            here

        Returns:
            H264BSD_RDY             decoding finished, nothing special
            H264BSD_PIC_RDY         decoding of a picture finished
            H264BSD_HDRS_RDY        param sets activated, information like picture dimensions etc can be read
            H264BSD_ERROR           error in decoding
            H264BSD_PARAM_SET_ERROR serius error in decoding, failed to activate param sets

------------------------------------------------------------------------------*/
uint32_t h264bsdDecode(storage_t *pStorage, uint8_t *byteStrm, uint32_t len, uint32_t picId,
  uint32_t *readBytes) {
  uint32_t tmp, ppsId, spsId;
  int32_t picOrderCnt;
  nalUnit_t nalUnit;
  seqParamSet_t seqParamSet;
  picParamSet_t picParamSet;
  strmData_t strm;
  uint32_t accessUnitBoundaryFlag = HANTRO_FALSE;
  uint32_t picReady = HANTRO_FALSE;

  DEBUGP2("h264bsdDecode len=%d", len);

  ASSERT(pStorage);
  ASSERT(byteStrm);
  ASSERT(len);
  ASSERT(readBytes);

  /* if previous buffer was not finished and same pointer given -> skip NAL unit extraction */
  if(pStorage->prevBufNotFinished && byteStrm == pStorage->prevBufPointer) {
    strm = pStorage->strm[0];
    strm.pStrmCurrPos = strm.pStrmBuffStart;
    strm.strmBuffReadBits = strm.bitPosInWord=0;
    *readBytes = pStorage->prevBytesConsumed;
    }
  else {
    tmp = h264bsdExtractNalUnit(byteStrm, len, &strm, readBytes);
    if(tmp != HANTRO_OK) {
      EPRINT("BYTE_STREAM");
      return(H264BSD_ERROR);
			}
    /* store stream */
    pStorage->strm[0] = strm;
    pStorage->prevBytesConsumed = *readBytes;
    pStorage->prevBufPointer = byteStrm;
		}
  pStorage->prevBufNotFinished = HANTRO_FALSE;

  tmp = h264bsdDecodeNalUnit(&strm, &nalUnit);
  if(tmp != HANTRO_OK) {
    EPRINT("NAL_UNIT");
    return(H264BSD_ERROR);
		}

  /* Discard unspecified, reserved, SPS extension and auxiliary picture slices */
  if(nalUnit.nalUnitType == 0 || nalUnit.nalUnitType >= 13) {
    DEBUGP("DISCARDED NAL (UNSPECIFIED, REGISTERED, SPS ext or AUX slice)");
    return(H264BSD_RDY);
    }

  tmp = h264bsdCheckAccessUnitBoundary(&strm, &nalUnit, pStorage, &accessUnitBoundaryFlag);
  if(tmp != HANTRO_OK) {
    EPRINT("ACCESS UNIT BOUNDARY CHECK");
    if(tmp == PARAM_SET_ERROR)
      return(H264BSD_PARAM_SET_ERROR);
    else
      return(H264BSD_ERROR);
		}

  if(accessUnitBoundaryFlag) {
    DEBUGP("Access unit boundary");
    /* conceal if picture started and param sets activated */
    if(pStorage->picStarted && pStorage->activeSps) {
      DEBUGP("CONCEALING...");

      /* return error if second phase of initialization is not completed */
      if(pStorage->pendingActivation) {
        EPRINT("Pending activation not completed");
        return (H264BSD_ERROR);
	      }

      if(!pStorage->validSliceInAccessUnit) {
        pStorage->currImage->data = h264bsdAllocateDpbImage(pStorage->dpb);
        h264bsdInitRefPicList(pStorage->dpb);
        tmp = h264bsdConceal(pStorage, pStorage->currImage, P_SLICE);
				}
      else
        tmp = h264bsdConceal(pStorage, pStorage->currImage, pStorage->sliceHeader->sliceType);

      picReady = HANTRO_TRUE;

      /* current NAL unit should be decoded on next activation -> set readBytes to 0 */
      *readBytes=0;
      pStorage->prevBufNotFinished = HANTRO_TRUE;
      DEBUGP("...DONE");
	    }
    else
      pStorage->validSliceInAccessUnit = HANTRO_FALSE;
    pStorage->skipRedundantSlices = HANTRO_FALSE;
    }

  if(!picReady) {
    switch(nalUnit.nalUnitType) {
      case NAL_SEQ_PARAM_SET:
        DEBUGP("SEQ PARAM SET");
        tmp = h264bsdDecodeSeqParamSet(&strm, &seqParamSet);
        if(tmp != HANTRO_OK) {
          EPRINT("SEQ_PARAM_SET");
          FREE(seqParamSet.offsetForRefFrame);
          FREE(seqParamSet.vuiParameters);
          return(H264BSD_ERROR);
          }
        tmp = h264bsdStoreSeqParamSet(pStorage, &seqParamSet);
        break;

      case NAL_PIC_PARAM_SET:
        DEBUGP("PIC PARAM SET");
        tmp = h264bsdDecodePicParamSet(&strm, &picParamSet, pStorage->sps[0]->profileIdc);			// FINIRE altri SPS
        if(tmp != HANTRO_OK) {
          EPRINT("PIC_PARAM_SET");
          FREE(picParamSet.runLength);
          FREE(picParamSet.topLeft);
          FREE(picParamSet.bottomRight);
          FREE(picParamSet.sliceGroupId);
          return(H264BSD_ERROR);
		      }
        tmp = h264bsdStorePicParamSet(pStorage, &picParamSet);
        break;

      case NAL_CODED_SLICE_IDR:
        DEBUGP("IDR ");
        /* fall through */
      case NAL_CODED_SLICE:
	      DEBUGP("SLICE HEADER");

        /* picture successfully finished and still decoding same old
         * access unit -> no need to decode redundant slices */
        if(pStorage->skipRedundantSlices)
          return(H264BSD_RDY);

        pStorage->picStarted = HANTRO_TRUE;

        if(h264bsdIsStartOfPicture(pStorage)) {
          pStorage->numConcealedMbs=0;
          pStorage->currentPicId    = picId;

          tmp = h264bsdCheckPpsId(&strm, &ppsId);
          ASSERT(tmp == HANTRO_OK);
          /* store old activeSpsId and return headers ready indication if activeSps changes */
          spsId = pStorage->activeSpsId;
          tmp = h264bsdActivateParamSets(pStorage, ppsId, IS_IDR_NAL_UNIT(&nalUnit) ? HANTRO_TRUE : HANTRO_FALSE);
          if(tmp != HANTRO_OK)  {
            EPRINT("Param set activation");
            pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS;
            pStorage->activePps = NULL;
            pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS;
            pStorage->activeSps = NULL;
            pStorage->pendingActivation = HANTRO_FALSE;

            if(tmp == MEMORY_ALLOCATION_ERROR)
              return H264BSD_MEMALLOC_ERROR;
            else
              return(H264BSD_PARAM_SET_ERROR);
	          }

          if(spsId != pStorage->activeSpsId)  {
            seqParamSet_t *oldSPS = NULL;
            seqParamSet_t *newSPS = pStorage->activeSps;
            uint32_t noOutputOfPriorPicsFlag = 1;

            if(pStorage->oldSpsId < MAX_NUM_SEQ_PARAM_SETS)
              oldSPS = pStorage->sps[pStorage->oldSpsId];

            *readBytes=0;
            pStorage->prevBufNotFinished = HANTRO_TRUE;

            if(nalUnit.nalUnitType == NAL_CODED_SLICE_IDR)
              tmp = h264bsdCheckPriorPicsFlag(&noOutputOfPriorPicsFlag, &strm, newSPS,
                                            pStorage->activePps, nalUnit.nalUnitType);
            else 
              tmp = HANTRO_NOK;

            if((tmp != HANTRO_OK) ||
							(noOutputOfPriorPicsFlag != 0) || (pStorage->dpb->noReordering) ||
							(!oldSPS) ||
							(oldSPS->picWidthInMbs != newSPS->picWidthInMbs) ||
							(oldSPS->picHeightInMbs != newSPS->picHeightInMbs) ||
							(oldSPS->maxDpbSize != newSPS->maxDpbSize))
              pStorage->dpb->flushed=0;
            else
              h264bsdFlushDpb(pStorage->dpb);

            pStorage->oldSpsId = pStorage->activeSpsId;
            return(H264BSD_HDRS_RDY);
            }
					}

        /* return error if second phase of initialization is not completed */
        if(pStorage->pendingActivation) {
          EPRINT("Pending activation not completed");
          return (H264BSD_ERROR);
					}
        tmp = h264bsdDecodeSliceHeader(&strm, pStorage->sliceHeader + 1,
          pStorage->activeSps, pStorage->activePps, &nalUnit);
        if(tmp != HANTRO_OK) {
          EPRINT("SLICE_HEADER");
          return(H264BSD_ERROR);
					}
        if(h264bsdIsStartOfPicture(pStorage)) {
          if(!IS_IDR_NAL_UNIT(&nalUnit))  {
            tmp = h264bsdCheckGapsInFrameNum(pStorage->dpb,
              pStorage->sliceHeader[1].frameNum,
              nalUnit.nalRefIdc != 0 ? HANTRO_TRUE : HANTRO_FALSE,
              pStorage->activeSps->gapsInFrameNumValueAllowedFlag);
            if(tmp != HANTRO_OK)      {
              EPRINT("Gaps in frame num");
              return(H264BSD_ERROR);
              }
						}
          pStorage->currImage->data = h264bsdAllocateDpbImage(pStorage->dpb);
					}

        /* store slice header to storage if successfully decoded */
        pStorage->sliceHeader[0] = pStorage->sliceHeader[1];
        pStorage->validSliceInAccessUnit = HANTRO_TRUE;
        pStorage->prevNalUnit[0] = nalUnit;

        h264bsdComputeSliceGroupMap(pStorage,
            pStorage->sliceHeader->sliceGroupChangeCycle);

        h264bsdInitRefPicList(pStorage->dpb);
        tmp = h264bsdReorderRefPicList(pStorage->dpb,
            &pStorage->sliceHeader->refPicListReordering,
            pStorage->sliceHeader->frameNum,
            pStorage->sliceHeader->numRefIdxL0Active);
        if(tmp != HANTRO_OK) {
          EPRINT("Reordering");
          return(H264BSD_ERROR);
					}

        DEBUGP2("SLICE DATA, FIRST %d", pStorage->sliceHeader->firstMbInSlice);
        tmp = h264bsdDecodeSliceData(&strm, pStorage, pStorage->currImage, pStorage->sliceHeader);
        if(tmp != HANTRO_OK) {
          EPRINT("SLICE_DATA");
          h264bsdMarkSliceCorrupted(pStorage, pStorage->sliceHeader->firstMbInSlice);
          return(H264BSD_ERROR);
	        }

        if(h264bsdIsEndOfPicture(pStorage)) {
          picReady = HANTRO_TRUE;
          pStorage->skipRedundantSlices = HANTRO_TRUE;
		      }
        break;

	    case NAL_SEI:
				DEBUGP("SEI MESSAGE, NOT DECODED");
				break;

	    default:
		    DEBUGP2("NOT IMPLEMENTED YET %d",nalUnit.nalUnitType);
      }
	  }

//  DEBUGP2("h264bsdDecode usati %d", *readBytes);

  if(picReady) {
    h264bsdFilterPicture(pStorage->currImage, pStorage->mb);

    h264bsdResetStorage(pStorage);

     picOrderCnt = h264bsdDecodePicOrderCnt(pStorage->poc,
        pStorage->activeSps, pStorage->sliceHeader, pStorage->prevNalUnit);

    if(pStorage->validSliceInAccessUnit) {
      if(pStorage->prevNalUnit->nalRefIdc)
        tmp = h264bsdMarkDecRefPic(pStorage->dpb,
            &pStorage->sliceHeader->decRefPicMarking,
            pStorage->currImage, pStorage->sliceHeader->frameNum,
            picOrderCnt,
            IS_IDR_NAL_UNIT(pStorage->prevNalUnit) ? HANTRO_TRUE : HANTRO_FALSE,
            pStorage->currentPicId, pStorage->numConcealedMbs);
      /* non-reference picture, just store for possible display reordering */
      else
        tmp = h264bsdMarkDecRefPic(pStorage->dpb, NULL,
          pStorage->currImage, pStorage->sliceHeader->frameNum,
          picOrderCnt,
          IS_IDR_NAL_UNIT(pStorage->prevNalUnit) ? HANTRO_TRUE : HANTRO_FALSE,
          pStorage->currentPicId, pStorage->numConcealedMbs);
			}

    pStorage->picStarted = HANTRO_FALSE;
    pStorage->validSliceInAccessUnit = HANTRO_FALSE;

    return(H264BSD_PIC_RDY);
		}
  else
    return(H264BSD_RDY);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdShutdown

        Functional description:
            Shutdown a decoder instance. Function frees all the memories
            allocated for the decoder instance.

        Inputs:
            pStorage    pointer to storage data structure

        Returns:
            none


------------------------------------------------------------------------------*/
void h264bsdShutdown(storage_t *pStorage) {
  uint32_t i;

  ASSERT(pStorage);

  for(i=0; i < MAX_NUM_SEQ_PARAM_SETS; i++) {
    if(pStorage->sps[i]) {
      FREE(pStorage->sps[i]->offsetForRefFrame);
      FREE(pStorage->sps[i]->vuiParameters);
      FREE(pStorage->sps[i]);
      }
		}

  for(i=0; i < MAX_NUM_PIC_PARAM_SETS; i++) {
    if(pStorage->pps[i]) {
      FREE(pStorage->pps[i]->runLength);
      FREE(pStorage->pps[i]->topLeft);
      FREE(pStorage->pps[i]->bottomRight);
      FREE(pStorage->pps[i]->sliceGroupId);
      FREE(pStorage->pps[i]);
      }
		}

  FREE(pStorage->mbLayer);
  FREE(pStorage->mb);
  FREE(pStorage->sliceGroupMap);

  if(pStorage->conversionBuffer)
		FREE(pStorage->conversionBuffer);

  h264bsdFreeDpb(pStorage->dpb);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdNextOutputPicture

        Functional description:
            Get next output picture in display order.

        Inputs:
            pStorage    pointer to storage data structure

        Outputs:
            picId       identifier of the picture will be stored here
            isIdrPic    IDR flag of the picture will be stored here
            numErrMbs   number of concealed macroblocks in the picture
                        will be stored here

        Returns:
            pointer to the picture data
            NULL if no pictures available for display

------------------------------------------------------------------------------*/
uint8_t* h264bsdNextOutputPicture(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic,
    uint32_t *numErrMbs) {
	dpbOutPicture_t *pOut;

	ASSERT(pStorage);

	pOut = h264bsdDpbOutputPicture(pStorage->dpb);
	if(pOut) {
		*picId = pOut->picId;
		*isIdrPic = pOut->isIdr;
		*numErrMbs = pOut->numErrMbs;
		return pOut->data;
		}
	else
		return NULL;
	}


/*------------------------------------------------------------------------------

    Function: h264bsdNextOutputPictureRGBA

        Functional description:
            Get next output picture in display order, converted to RGBA.
            RGBA is the color format most commonly used by OpenGL.

        Inputs:
            pStorage    pointer to storage data structure

        Outputs:
            picId       identifier of the picture will be stored here
            isIdrPic    IDR flag of the picture will be stored here
            numErrMbs   number of concealed macroblocks in the picture will be stored here

        Returns:
            pointer to the picture data
            NULL if no pictures available for display

------------------------------------------------------------------------------*/
uint32_t* h264bsdNextOutputPictureRGBA(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic, uint32_t *numErrMbs) {
  uint32_t width = h264bsdPicWidth(pStorage) * MACROBLOCK_SIZE;
  uint32_t height = h264bsdPicHeight(pStorage) * MACROBLOCK_SIZE;
  uint8_t* data = h264bsdNextOutputPicture(pStorage, picId, isIdrPic, numErrMbs);
  size_t rgbSize = sizeof(uint32_t) * width * height;

  if(!data) 
		return NULL;

  if(pStorage->conversionBufferSize < rgbSize) {
    if(pStorage->conversionBuffer)
			free(pStorage->conversionBuffer);
    pStorage->conversionBufferSize = rgbSize;
    pStorage->conversionBuffer = (uint32_t*)malloc(rgbSize);
		}

  h264bsdConvertToRGBA(width, height, data, pStorage->conversionBuffer);
  return pStorage->conversionBuffer;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdNextOutputPictureRGBA

        Functional description:
            Get next output picture in display order, converted to BGRA.
            BGRA is the color format most commonly used by Windows.

        Inputs:
            pStorage    pointer to storage data structure

        Outputs:
            picId       identifier of the picture will be stored here
            isIdrPic    IDR flag of the picture will be stored here
            numErrMbs   number of concealed macroblocks in the picture will be stored here

        Returns:
            pointer to the picture data
            NULL if no pictures available for display

------------------------------------------------------------------------------*/
uint32_t* h264bsdNextOutputPictureBGRA(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic, uint32_t *numErrMbs) {
  uint32_t width = h264bsdPicWidth(pStorage) * MACROBLOCK_SIZE;
  uint32_t height = h264bsdPicHeight(pStorage) * MACROBLOCK_SIZE;
  uint8_t* data = h264bsdNextOutputPicture(pStorage, picId, isIdrPic, numErrMbs);
  size_t rgbSize = sizeof(uint32_t) * width * height;

  if(!data) 
		return NULL;

  if(pStorage->conversionBufferSize < rgbSize) {
    if(pStorage->conversionBuffer) 
			free(pStorage->conversionBuffer);
    pStorage->conversionBufferSize = rgbSize;
    pStorage->conversionBuffer = (uint32_t*)malloc(rgbSize);
		}

  h264bsdConvertToBGRA(width, height, data, pStorage->conversionBuffer);
  return pStorage->conversionBuffer;
	}
uint32_t* h264bsdNextOutputPictureBGR(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic, uint32_t *numErrMbs) {
  uint32_t width = h264bsdPicWidth(pStorage) * MACROBLOCK_SIZE;
  uint32_t height = h264bsdPicHeight(pStorage) * MACROBLOCK_SIZE;
  uint8_t* data = h264bsdNextOutputPicture(pStorage, picId, isIdrPic, numErrMbs);
  size_t rgbSize = 3 * width * height;

  if(!data) 
		return NULL;

  if(pStorage->conversionBufferSize < rgbSize) {
    if(pStorage->conversionBuffer) 
			free(pStorage->conversionBuffer);
    pStorage->conversionBufferSize = rgbSize;
    pStorage->conversionBuffer = (uint32_t*)malloc(rgbSize);
		}

  h264bsdConvertToBGR(width,height,data,(uint8_t*)pStorage->conversionBuffer);
  return pStorage->conversionBuffer;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdNextOutputPictureYCbCrA

        Functional description:
            Get next output picture in display order, converted to YCbCrA.
            YCbCrA is a 4:4:4 format that uses uint32_t pixels where the MSB is alpha.

        Inputs:
            pStorage    pointer to storage data structure

        Outputs:
            picId       identifier of the picture will be stored here
            isIdrPic    IDR flag of the picture will be stored here
            numErrMbs   number of concealed macroblocks in the picture
                        will be stored here

        Returns:
            pointer to the picture data
            NULL if no pictures available for display

------------------------------------------------------------------------------*/
uint32_t* h264bsdNextOutputPictureYCbCrA(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic, uint32_t *numErrMbs) {
  uint32_t width = h264bsdPicWidth(pStorage) * MACROBLOCK_SIZE;
  uint32_t height = h264bsdPicHeight(pStorage) * MACROBLOCK_SIZE;
  uint8_t* data = h264bsdNextOutputPicture(pStorage, picId, isIdrPic, numErrMbs);
  size_t rgbSize = sizeof(uint32_t) * width * height;

  if(!data)
		return NULL;

  if(pStorage->conversionBufferSize < rgbSize) {
    if(pStorage->conversionBuffer)
			free(pStorage->conversionBuffer);
    pStorage->conversionBufferSize = rgbSize;
    pStorage->conversionBuffer = (uint32_t*)malloc(rgbSize);
	  }

  h264bsdConvertToYCbCrA(width, height, data, pStorage->conversionBuffer);
  return pStorage->conversionBuffer;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdPicWidth

        Functional description:
            Get width of the picture in macroblocks

        Inputs:
            pStorage    pointer to storage data structure

        Outputs:
            none

        Returns:
            picture width
            0 if parameters sets not yet activated

------------------------------------------------------------------------------*/
uint32_t h264bsdPicWidth(storage_t *pStorage) {

  ASSERT(pStorage);

  if(pStorage->activeSps)
    return(pStorage->activeSps->picWidthInMbs);
  else
    return 0;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdPicHeight

        Functional description:
            Get height of the picture in macroblocks

        Inputs:
            pStorage    pointer to storage data structure

        Outputs:
            none

        Returns:
            picture width
            0 if parameters sets not yet activated

------------------------------------------------------------------------------*/
uint32_t h264bsdPicHeight(storage_t *pStorage) {

  ASSERT(pStorage);

  if(pStorage->activeSps)
    return pStorage->activeSps->picHeightInMbs;
  else
    return 0;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdFlushBuffer

        Functional description:
            Flush the decoded picture buffer, see dpb.c for details

        Inputs:
            pStorage    pointer to storage data structure

------------------------------------------------------------------------------*/
void h264bsdFlushBuffer(storage_t *pStorage) {

  ASSERT(pStorage);

  h264bsdFlushDpb(pStorage->dpb);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckValidParamSets

        Functional description:
            Check if any valid parameter set combinations (SPS/PPS) exists.

        Inputs:
            pStorage    pointer to storage structure

        Returns:
            1       at least one valid SPS/PPS combination found
            0       no valid param set combinations found


------------------------------------------------------------------------------*/
uint32_t h264bsdCheckValidParamSets(storage_t *pStorage) {

  ASSERT(pStorage);

  return(h264bsdValidParamSets(pStorage) == HANTRO_OK ? 1 : 0);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdVideoRange

        Functional description:
            Get value of video_full_range_flag received in the VUI data.

        Inputs:
            pStorage    pointer to storage structure

        Returns:
            1   video_full_range_flag received and value is 1
            0   otherwise

------------------------------------------------------------------------------*/
uint32_t h264bsdVideoRange(storage_t *pStorage) {

  ASSERT(pStorage);

  if(pStorage->activeSps && pStorage->activeSps->vuiParametersPresentFlag &&
    pStorage->activeSps->vuiParameters &&
    pStorage->activeSps->vuiParameters->videoSignalTypePresentFlag &&
    pStorage->activeSps->vuiParameters->videoFullRangeFlag)
    return 1;
  else /* default value of video_full_range_flag is 0 */
    return 0;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdMatrixCoefficients

        Functional description:
            Get value of matrix_coefficients received in the VUI data

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            value of matrix_coefficients if received
            2   otherwise (this is the default value)

------------------------------------------------------------------------------*/
uint32_t h264bsdMatrixCoefficients(storage_t *pStorage) {

  ASSERT(pStorage);

  if(pStorage->activeSps && pStorage->activeSps->vuiParametersPresentFlag &&
    pStorage->activeSps->vuiParameters &&
    pStorage->activeSps->vuiParameters->videoSignalTypePresentFlag &&
    pStorage->activeSps->vuiParameters->colourDescriptionPresentFlag)
    return(pStorage->activeSps->vuiParameters->matrixCoefficients);
  else /* default unspecified */
    return 2;
	}

/*------------------------------------------------------------------------------

    Function: hh264bsdCroppingParams

        Functional description:
            Get cropping parameters of the active SPS

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            croppingFlag    flag indicating if cropping params present is
                            stored here
            leftOffset      cropping left offset in pixels is stored here
            width           width of the image after cropping is stored here
            topOffset       cropping top offset in pixels is stored here
            height          height of the image after cropping is stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdCroppingParams(storage_t *pStorage, uint32_t *croppingFlag,
  uint32_t *leftOffset, uint32_t *width, uint32_t *topOffset, uint32_t *height) {

  ASSERT(pStorage);

  if(pStorage->activeSps && pStorage->activeSps->frameCroppingFlag) {
    *croppingFlag = 1;
    *leftOffset = 2 * pStorage->activeSps->frameCropLeftOffset;
    *width = 16 * pStorage->activeSps->picWidthInMbs -
             2 * (pStorage->activeSps->frameCropLeftOffset +
                  pStorage->activeSps->frameCropRightOffset);
    *topOffset = 2 * pStorage->activeSps->frameCropTopOffset;
    *height = 16 * pStorage->activeSps->picHeightInMbs -
              2 * (pStorage->activeSps->frameCropTopOffset +
                   pStorage->activeSps->frameCropBottomOffset);
	  }
  else {
    *croppingFlag=0;
    *leftOffset=0;
    *width=0;
    *topOffset=0;
    *height=0;
	  }
	}

/*------------------------------------------------------------------------------

    Function: h264bsdSampleAspectRatio

        Functional description:
            Get aspect ratio received in the VUI data

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            sarWidth    sample aspect ratio height
            sarHeight   sample aspect ratio width

------------------------------------------------------------------------------*/
void h264bsdSampleAspectRatio(storage_t *pStorage, uint32_t *sarWidth, uint32_t *sarHeight) {
  uint32_t w = 1;
  uint32_t h = 1;

  ASSERT(pStorage);


  if(pStorage->activeSps &&
      pStorage->activeSps->vuiParametersPresentFlag &&
      pStorage->activeSps->vuiParameters &&
      pStorage->activeSps->vuiParameters->aspectRatioPresentFlag ) {
      switch(pStorage->activeSps->vuiParameters->aspectRatioIdc) {
          case ASPECT_RATIO_UNSPECIFIED:  w =   0; h =  0; break;
          case ASPECT_RATIO_1_1:          w =   1; h =  1; break;
          case ASPECT_RATIO_12_11:        w =  12; h = 11; break;
          case ASPECT_RATIO_10_11:        w =  10; h = 11; break;
          case ASPECT_RATIO_16_11:        w =  16; h = 11; break;
          case ASPECT_RATIO_40_33:        w =  40; h = 33; break;
          case ASPECT_RATIO_24_11:        w =  24; h = 11; break;
          case ASPECT_RATIO_20_11:        w =  20; h = 11; break;
          case ASPECT_RATIO_32_11:        w =  32; h = 11; break;
          case ASPECT_RATIO_80_33:        w =  80; h = 33; break;
          case ASPECT_RATIO_18_11:        w =  18; h = 11; break;
          case ASPECT_RATIO_15_11:        w =  15; h = 11; break;
          case ASPECT_RATIO_64_33:        w =  64; h = 33; break;
          case ASPECT_RATIO_160_99:       w = 160; h = 99; break;
          case ASPECT_RATIO_EXTENDED_SAR:
              w = pStorage->activeSps->vuiParameters->sarWidth;
              h = pStorage->activeSps->vuiParameters->sarHeight;
              if((w == 0) || (h == 0))
                  w = h=0;
              break;
          default:
              w=0;
              h=0;
              break;
      }
    }

  /* set aspect ratio*/
  *sarWidth = w;
  *sarHeight = h;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdProfile

        Functional description:
            Get profile information from active SPS

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            profile   current profile

------------------------------------------------------------------------------*/
uint32_t h264bsdProfile(storage_t *pStorage) {

  if(pStorage->activeSps)
    return pStorage->activeSps->profileIdc;
  else
    return 0;
	}

/*------------------------------------------------------------------------------

    Function name: h264bsdAlloc

        Functional description:
            Allocate storage for a decoder

        Inputs:
            none

        Outputs:
            none

        Returns:
            pStorage            pointer to uninitialized storage structure

------------------------------------------------------------------------------*/
storage_t* h264bsdAlloc() {
  return (storage_t*)malloc(sizeof(storage_t));
	}

/*------------------------------------------------------------------------------

    Function name: h264bsdFree

        Functional description:
            Free storage for a decoder

        Inputs:
            pStorage            pointer to storage structure

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdFree(storage_t *pStorage) {
  free(pStorage);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdConvertToRGBA

        Functional description:
            Convert decoded image data RGBA format.
            RGBA is the color format most commonly used by OpenGL.
            RGBA format uses uint32_t pixels where the MSB is alpha.
            *Note* While this function is available, it is not heavily optimized.
            If possible, you should use decoded image data directly. 
            This function should only be used when there is no other way to get RGBA data.

        Inputs:
            width       width of the image in pixels
            height      height of the image in pixels
            data        pointer to decoded image data

        Outputs:
            pOutput     pointer to the buffer where the RGBA data will be written

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdConvertToRGBA(uint32_t width, uint32_t height, uint8_t* data, uint32_t *pOutput) {
    const int w = (int)width;
    const int h = (int)height;

    int x=0;
    int y=0;

    size_t ySize = w * h;
    size_t cbSize = w/2 * h/2;

    uint8_t* luma = data;
    uint8_t* cb = data + ySize;
    uint8_t* cr = data + ySize + cbSize;
    uint32_t* rgba = pOutput;

    while(y < h) {
        int c = *luma - 16;
        int d = *cb - 128;
        int e = *cr - 128;

        uint32_t r = (uint32_t)CLIP1((298*c         + 409*e + 128) >> 8);
        uint32_t g = (uint32_t)CLIP1((298*c - 100*d - 208*e + 128) >> 8);
        uint32_t b = (uint32_t)CLIP1((298*c + 516*d         + 128) >> 8);

        uint32_t pixel=0xff;
        pixel = (pixel << 8) + b;
        pixel = (pixel << 8) + g;
        pixel = (pixel << 8) + r;

        *rgba = pixel;

        ++x;
        ++rgba;
        ++luma;

        if(!(x & 1)) {
            ++cb;
            ++cr;
					}

        if(x < w) 
					continue;

        x=0;
        ++y;

        if(y & 1) {
            cb -= w/2;
            cr -= w/2;
        }
    }
	}

/*------------------------------------------------------------------------------

    Function: h264bsdConvertToBGRA

        Functional description:
            Convert decoded image data BGRA format.
            BGRA is the color format most commonly used by Windows.
            BGRA format uses uint32_t pixels where the MSB is alpha.
            *Note* While this function is available, it is not heavily optimized.
            If possible, you should use decoded image data directly. 
            This function should only be used when there is no other way to get BGRA data.

        Inputs:
            width       width of the image in pixels
            height      height of the image in pixels
            data        pointer to decoded image data

        Outputs:
            pOutput     pointer to the buffer where the BGRA data will be written

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdConvertToBGRA(uint32_t width, uint32_t height, uint8_t* data, uint32_t *pOutput) {
  const int w = (int)width;
  const int h = (int)height;

  int x=0;
  int y=0;

  size_t ySize = w * h;
  size_t cbSize = w/2 * h/2;

  uint8_t* luma = data;
  uint8_t* cb = data + ySize;
  uint8_t* cr = data + ySize + cbSize;
  uint32_t* bgra = pOutput;

  while(y < h) {
    int c = *luma - 16;
    int d = *cb - 128;
    int e = *cr - 128;

    uint32_t r = (uint32_t)CLIP1((298*c         + 409*e + 128) >> 8);
    uint32_t g = (uint32_t)CLIP1((298*c - 100*d - 208*e + 128) >> 8);
    uint32_t b = (uint32_t)CLIP1((298*c + 516*d         + 128) >> 8);

    uint32_t pixel=0xff;
    pixel = (pixel << 8) + r;
    pixel = (pixel << 8) + g;
    pixel = (pixel << 8) + b;

    *bgra = pixel;

    ++x;
    ++bgra;
    ++luma;

    if(!(x & 1)) {
      ++cb;
      ++cr;
			}

    if(x < w) 
			continue;

    x=0;
    ++y;

    if(y & 1) {
      cb -= w/2;
      cr -= w/2;
			}
		}
	}

// GD RGB only
void h264bsdConvertToBGR(uint32_t width, uint32_t height, uint8_t* data, uint8_t *pOutput) {
  const int w = (int)width;
  const int h = (int)height;

  int x=0;
  int y=0;

  size_t ySize = w * h;
  size_t cbSize = w/2 * h/2;

  uint8_t* luma = data;
  uint8_t* cb = data + ySize;
  uint8_t* cr = data + ySize + cbSize;
  uint8_t* bgra = pOutput;

  while(y < h) {
    int c = *luma - 16;
    int d = *cb - 128;
    int e = *cr - 128;

    uint32_t r = (uint32_t)CLIP1((298*c         + 409*e + 128) >> 8);
    uint32_t g = (uint32_t)CLIP1((298*c - 100*d - 208*e + 128) >> 8);
    uint32_t b = (uint32_t)CLIP1((298*c + 516*d         + 128) >> 8);

    *bgra++ = b;
    *bgra++ = g;
    *bgra++ = r;

    x++;
    luma++;

    if(!(x & 1)) {
      cb++;
      cr++;
			}

    if(x < w) 
			continue;

    x=0;
    y++;

    if(y & 1) {
      cb -= w/2;
      cr -= w/2;
			}
		}
	}

/*------------------------------------------------------------------------------

    Function: h264bsdConvertToYCbCrA

        Functional description:
            Convert decoded image data YCbCrA format.
            YCbCrA is a 4:4:4 format that uses uint32_t pixels where the MSB is alpha.
            *Note* While this function is available, it is not heavily optimized.
            If possible, you should use decoded image data directly. 
            This function should only be used when there is no other way to get YCbCrA data.

        Inputs:
            width       width of the image in pixels
            height      height of the image in pixels
            data        pointer to decoded image data

        Outputs:
            pOutput     pointer to the buffer where the YCbCrA data will be written

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdConvertToYCbCrA(uint32_t width, uint32_t height, uint8_t* data, uint32_t *pOutput) {
  const int w = (int)width;
  const int h = (int)height;

  int x=0;
  int y=0;

  size_t ySize = w * h;
  size_t cbSize = w/2 * h/2;

  uint8_t* luma = data;
  uint8_t* cb = data + ySize;
  uint8_t* cr = data + ySize + cbSize;
  uint32_t* yCbCr = pOutput;

  while(y < h) {
      uint32_t pixel=0xff;
      pixel = (pixel << 8) + *cr;
      pixel = (pixel << 8) + *cb;
      pixel = (pixel << 8) + *luma;

      *yCbCr = pixel;

      ++x;
      ++yCbCr;
      ++luma;

      if(!(x & 1)) {
          ++cb;
          ++cr;
      }

      if(x < w) continue;

      x=0;
      ++y;

      if(y & 1) {
          cb -= w/2;
          cr -= w/2;
        }
    }
	}



/* macros to determine picture status. Note that IS_SHORT_TERM macro returns
 * true also for non-existing pictures because non-existing pictures are
 * regarded short term pictures according to H.264 standard */
#define IS_REFERENCE(a) ((a).status)
#define IS_EXISTING(a) ((a).status > NON_EXISTING)
#define IS_SHORT_TERM(a) \
    ((a).status == NON_EXISTING || (a).status == SHORT_TERM)
#define IS_LONG_TERM(a) ((a).status == LONG_TERM)

/* macro to set a picture unused for reference */
#define SET_UNUSED(a) (a).status = UNUSED;

#define MAX_NUM_REF_IDX_L0_ACTIVE 16

static int32_t ComparePictures(const void *ptr1, const void *ptr2);
static uint32_t Mmcop1(dpbStorage_t *dpb, uint32_t currPicNum, uint32_t differenceOfPicNums);
static uint32_t Mmcop2(dpbStorage_t *dpb, uint32_t longTermPicNum);
static uint32_t Mmcop3(dpbStorage_t *dpb, uint32_t currPicNum, uint32_t differenceOfPicNums,
    uint32_t longTermFrameIdx);
static uint32_t Mmcop4(dpbStorage_t *dpb, uint32_t maxLongTermFrameIdx);
static uint32_t Mmcop5(dpbStorage_t *dpb);
static uint32_t Mmcop6(dpbStorage_t *dpb, uint32_t frameNum, int32_t picOrderCnt,
    uint32_t longTermFrameIdx);
static uint32_t SlidingWindowRefPicMarking(dpbStorage_t *dpb);
static int32_t FindDpbPic(dpbStorage_t *dpb, int32_t picNum, uint32_t isShortTerm);
static void SetPicNums(dpbStorage_t *dpb, uint32_t currFrameNum);
static dpbPicture_t* FindSmallestPicOrderCnt(dpbStorage_t *dpb);
static uint32_t OutputPicture(dpbStorage_t *dpb);
static void ShellSort(dpbPicture_t *pPic, uint32_t num);

/*------------------------------------------------------------------------------

    Function: ComparePictures

        Functional description:
            Function to compare dpb pictures, used by the ShellSort() function.
            Order of the pictures after sorting shall be as follows:
                1) short term reference pictures starting with the largest
                   picNum
                2) long term reference pictures starting with the smallest
                   longTermPicNum
                3) pictures unused for reference but needed for display
                4) other pictures

        Returns:
            -1      pic 1 is greater than pic 2
             0      equal from comparison point of view
             1      pic 2 is greater then pic 1

------------------------------------------------------------------------------*/
static int32_t ComparePictures(const void *ptr1, const void *ptr2) {
    dpbPicture_t *pic1, *pic2;

    ASSERT(ptr1);
    ASSERT(ptr2);

    pic1 = (dpbPicture_t*)ptr1;
    pic2 = (dpbPicture_t*)ptr2;

    /* both are non-reference pictures, check if needed for display */
    if(!IS_REFERENCE(*pic1) && !IS_REFERENCE(*pic2)) {
        if(pic1->toBeDisplayed && !pic2->toBeDisplayed)
            return(-1);
        else if(!pic1->toBeDisplayed && pic2->toBeDisplayed)
            return 1;
        else
            return 0;
			}
    /* only pic 1 needed for reference -> greater */
    else if(!IS_REFERENCE(*pic2))
        return(-1);
    /* only pic 2 needed for reference -> greater */
    else if(!IS_REFERENCE(*pic1))
        return 1;
    /* both are short term reference pictures -> check picNum */
    else if(IS_SHORT_TERM(*pic1) && IS_SHORT_TERM(*pic2)) {
        if(pic1->picNum > pic2->picNum)
            return(-1);
        else if(pic1->picNum < pic2->picNum)
            return 1;
        else
            return 0;
		  }
    /* only pic 1 is short term -> greater */
    else if(IS_SHORT_TERM(*pic1))
        return(-1);
    /* only pic 2 is short term -> greater */
    else if(IS_SHORT_TERM(*pic2))
        return 1;
    /* both are long term reference pictures -> check picNum (contains the
     * longTermPicNum */
    else {
        if(pic1->picNum > pic2->picNum)
            return 1;
        else if(pic1->picNum < pic2->picNum)
            return(-1);
        else
            return 0;
    }
	}

/*------------------------------------------------------------------------------

    Function: h264bsdReorderRefPicList

        Functional description:
            Function to perform reference picture list reordering based on
            reordering commands received in the slice header. See details
            of the process in the H.264 standard.

        Inputs:
            dpb             pointer to dpb storage structure
            order           pointer to reordering commands
            currFrameNum    current frame number
            numRefIdxActive number of active reference indices for current
                            picture

        Outputs:
            dpb             'list' field of the structure reordered

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     if non-existing pictures referred to in the
                           reordering commands

------------------------------------------------------------------------------*/
uint32_t h264bsdReorderRefPicList(dpbStorage_t *dpb,
  refPicListReordering_t *order,
  uint32_t currFrameNum, uint32_t numRefIdxActive) {
    uint32_t i, j, k, picNumPred, refIdx;
    int32_t picNum, picNumNoWrap, index;
    uint32_t isShortTerm;

    ASSERT(order);
    ASSERT(currFrameNum <= dpb->maxFrameNum);
    ASSERT(numRefIdxActive <= MAX_NUM_REF_IDX_L0_ACTIVE);

    /* set dpb picture numbers for sorting */
    SetPicNums(dpb, currFrameNum);

    if(!order->refPicListReorderingFlagL0)
        return(HANTRO_OK);

    refIdx    =0;
    picNumPred = currFrameNum;

    i=0;
    while(order->command[i].reorderingOfPicNumsIdc < 3) {
        /* short term */
        if(order->command[i].reorderingOfPicNumsIdc < 2) {
            if(order->command[i].reorderingOfPicNumsIdc == 0) {
                picNumNoWrap =
                    (int32_t)picNumPred - (int32_t)order->command[i].absDiffPicNum;
                if(picNumNoWrap < 0)
                    picNumNoWrap += (int32_t)dpb->maxFrameNum;
            }
            else {
                picNumNoWrap =
                    (int32_t)(picNumPred + order->command[i].absDiffPicNum);
                if(picNumNoWrap >= (int32_t)dpb->maxFrameNum)
                    picNumNoWrap -= (int32_t)dpb->maxFrameNum;
            }
            picNumPred = (uint32_t)picNumNoWrap;
            picNum = picNumNoWrap;
            if((uint32_t)picNumNoWrap > currFrameNum)
                picNum -= (int32_t)dpb->maxFrameNum;
            isShortTerm = HANTRO_TRUE;
        }
        /* long term */
        else {
            picNum = (int32_t)order->command[i].longTermPicNum;
            isShortTerm = HANTRO_FALSE;

        }
        /* find corresponding picture from dpb */
        index = FindDpbPic(dpb, picNum, isShortTerm);
        if(index < 0 || !IS_EXISTING(dpb->buffer[index]))
            return(HANTRO_NOK);

        /* shift pictures */
        for(j = numRefIdxActive; j > refIdx; j--)
            dpb->list[j] = dpb->list[j-1];
        /* put picture into the list */
        dpb->list[refIdx++] = &dpb->buffer[index];
        /* remove later references to the same picture */
        for(j = k = refIdx; j <= numRefIdxActive; j++)
            if(dpb->list[j] != &dpb->buffer[index])
                dpb->list[k++] = dpb->list[j];

        i++;
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: Mmcop1

        Functional description:
            Function to mark a short-term reference picture unused for
            reference, memory_management_control_operation equal to 1

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     failure, picture does not exist in the buffer

------------------------------------------------------------------------------*/
static uint32_t Mmcop1(dpbStorage_t *dpb, uint32_t currPicNum, uint32_t differenceOfPicNums) {
    int32_t index, picNum;

    ASSERT(currPicNum < dpb->maxFrameNum);

    picNum = (int32_t)currPicNum - (int32_t)differenceOfPicNums;

    index = FindDpbPic(dpb, picNum, HANTRO_TRUE);
    if(index < 0)
        return(HANTRO_NOK);

    SET_UNUSED(dpb->buffer[index]);
    dpb->numRefFrames--;
    if(!dpb->buffer[index].toBeDisplayed)
        dpb->fullness--;

    return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: Mmcop2

        Functional description:
            Function to mark a long-term reference picture unused for
            reference, memory_management_control_operation equal to 2

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     failure, picture does not exist in the buffer

------------------------------------------------------------------------------*/
static uint32_t Mmcop2(dpbStorage_t *dpb, uint32_t longTermPicNum) {
    int32_t index;

    index = FindDpbPic(dpb, (int32_t)longTermPicNum, HANTRO_FALSE);
    if(index < 0)
        return(HANTRO_NOK);

    SET_UNUSED(dpb->buffer[index]);
    dpb->numRefFrames--;
    if(!dpb->buffer[index].toBeDisplayed)
        dpb->fullness--;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: Mmcop3

        Functional description:
            Function to assing a longTermFrameIdx to a short-term reference
            frame (i.e. to change it to a long-term reference picture),
            memory_management_control_operation equal to 3

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     failure, short-term picture does not exist in the
                           buffer or is a non-existing picture, or invalid
                           longTermFrameIdx given

------------------------------------------------------------------------------*/

static uint32_t Mmcop3(dpbStorage_t *dpb, uint32_t currPicNum, uint32_t differenceOfPicNums,
    uint32_t longTermFrameIdx) {
    int32_t index, picNum;
    uint32_t i;


    ASSERT(dpb);
    ASSERT(currPicNum < dpb->maxFrameNum);

    if((dpb->maxLongTermFrameIdx == NO_LONG_TERM_FRAME_INDICES) ||
         (longTermFrameIdx > dpb->maxLongTermFrameIdx))
        return(HANTRO_NOK);

    /* check if a long term picture with the same longTermFrameIdx already
     * exist and remove it if necessary */
    for(i=0; i < dpb->maxRefFrames; i++)
        if(IS_LONG_TERM(dpb->buffer[i]) &&
          (uint32_t)dpb->buffer[i].picNum == longTermFrameIdx) {
            SET_UNUSED(dpb->buffer[i]);
            dpb->numRefFrames--;
            if(!dpb->buffer[i].toBeDisplayed)
                dpb->fullness--;
            break;
        }

    picNum = (int32_t)currPicNum - (int32_t)differenceOfPicNums;

    index = FindDpbPic(dpb, picNum, HANTRO_TRUE);
    if(index < 0)
        return(HANTRO_NOK);
    if(!IS_EXISTING(dpb->buffer[index]))
        return(HANTRO_NOK);

    dpb->buffer[index].status = LONG_TERM;
    dpb->buffer[index].picNum = (int32_t)longTermFrameIdx;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: Mmcop4

        Functional description:
            Function to set maxLongTermFrameIdx,
            memory_management_control_operation equal to 4

        Returns:
            HANTRO_OK      success

------------------------------------------------------------------------------*/
static uint32_t Mmcop4(dpbStorage_t *dpb, uint32_t maxLongTermFrameIdx) {
  uint32_t i;


  dpb->maxLongTermFrameIdx = maxLongTermFrameIdx;

  for(i=0; i < dpb->maxRefFrames; i++)
      if(IS_LONG_TERM(dpb->buffer[i]) &&
        (((uint32_t)dpb->buffer[i].picNum > maxLongTermFrameIdx) ||
          (dpb->maxLongTermFrameIdx == NO_LONG_TERM_FRAME_INDICES))) {
          SET_UNUSED(dpb->buffer[i]);
          dpb->numRefFrames--;
          if(!dpb->buffer[i].toBeDisplayed)
              dpb->fullness--;
      }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: Mmcop5

        Functional description:
            Function to mark all reference pictures unused for reference and
            set maxLongTermFrameIdx to NO_LONG_TERM_FRAME_INDICES,
            memory_management_control_operation equal to 5. Function flushes
            the buffer and places all pictures that are needed for display into
            the output buffer.

        Returns:
            HANTRO_OK      success

------------------------------------------------------------------------------*/
static uint32_t Mmcop5(dpbStorage_t *dpb) {
  uint32_t i;


  for(i=0; i < 16; i++) {
      if(IS_REFERENCE(dpb->buffer[i])) {
          SET_UNUSED(dpb->buffer[i]);
          if(!dpb->buffer[i].toBeDisplayed)
              dpb->fullness--;
	    }
		}

  /* output all pictures */
  while(OutputPicture(dpb) == HANTRO_OK)
      ;
  dpb->numRefFrames=0;
  dpb->maxLongTermFrameIdx = NO_LONG_TERM_FRAME_INDICES;
  dpb->prevRefFrameNum=0;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: Mmcop6

        Functional description:
            Function to assign longTermFrameIdx to the current picture,
            memory_management_control_operation equal to 6

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     invalid longTermFrameIdx or no room for current
                           picture in the buffer

------------------------------------------------------------------------------*/
static uint32_t Mmcop6(dpbStorage_t *dpb, uint32_t frameNum, int32_t picOrderCnt,
  uint32_t longTermFrameIdx) {
  uint32_t i;


  ASSERT(frameNum < dpb->maxFrameNum);

  if((dpb->maxLongTermFrameIdx == NO_LONG_TERM_FRAME_INDICES) ||
       (longTermFrameIdx > dpb->maxLongTermFrameIdx))
      return(HANTRO_NOK);

  /* check if a long term picture with the same longTermFrameIdx already
   * exist and remove it if necessary */
  for(i=0; i < dpb->maxRefFrames; i++)
      if(IS_LONG_TERM(dpb->buffer[i]) &&
        (uint32_t)dpb->buffer[i].picNum == longTermFrameIdx) {
          SET_UNUSED(dpb->buffer[i]);
          dpb->numRefFrames--;
          if(!dpb->buffer[i].toBeDisplayed)
              dpb->fullness--;
          break;
      }

  if(dpb->numRefFrames < dpb->maxRefFrames) {
      dpb->currentOut->frameNum = frameNum;
      dpb->currentOut->picNum   = (int32_t)longTermFrameIdx;
      dpb->currentOut->picOrderCnt = picOrderCnt;
      dpb->currentOut->status   = LONG_TERM;
      if(dpb->noReordering)
          dpb->currentOut->toBeDisplayed = HANTRO_FALSE;
      else
          dpb->currentOut->toBeDisplayed = HANTRO_TRUE;
      dpb->numRefFrames++;
      dpb->fullness++;
      return(HANTRO_OK);
  }
  /* if there is no room, return an error */
  else
      return(HANTRO_NOK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdMarkDecRefPic

        Functional description:
            Function to perform reference picture marking process. This
            function should be called both for reference and non-reference
            pictures.  Non-reference pictures shall have mark pointer set to
            NULL.

        Inputs:
            dpb         pointer to the DPB data structure
            mark        pointer to reference picture marking commands
            image       pointer to current picture to be placed in the buffer
            frameNum    frame number of the current picture
            picOrderCnt picture order count for the current picture
            isIdr       flag to indicate if the current picture is an
                        IDR picture
            currentPicId    identifier for the current picture, from the
                            application, stored along with the picture
            numErrMbs       number of concealed macroblocks in the current
                            picture, stored along with the picture

        Outputs:
            dpb         'buffer' modified, possible output frames placed into
                        'outBuf'

        Returns:
            HANTRO_OK   success
            HANTRO_NOK  failure

------------------------------------------------------------------------------*/
uint32_t h264bsdMarkDecRefPic(dpbStorage_t *dpb, decRefPicMarking_t *mark,
  image_t *image,
  uint32_t frameNum, int32_t picOrderCnt, uint32_t isIdr,
  uint32_t currentPicId, uint32_t numErrMbs) {


  uint32_t i, status;
  uint32_t markedAsLongTerm;
  uint32_t toBeDisplayed;


  ASSERT(dpb);
  ASSERT(mark || !isIdr);
  ASSERT(!isIdr || (frameNum == 0 && picOrderCnt == 0));
  ASSERT(frameNum < dpb->maxFrameNum);

  if(image->data != dpb->currentOut->data) {
      EPRINT("TRYING TO MARK NON-ALLOCATED IMAGE");
      return(HANTRO_NOK);
    }

    dpb->lastContainsMmco5 = HANTRO_FALSE;
    status = HANTRO_OK;

    toBeDisplayed = dpb->noReordering ? HANTRO_FALSE : HANTRO_TRUE;

    /* non-reference picture, stored for display reordering purposes */
    if(!mark) {
        dpb->currentOut->status = UNUSED;
        dpb->currentOut->frameNum = frameNum;
        dpb->currentOut->picNum = (int32_t)frameNum;
        dpb->currentOut->picOrderCnt = picOrderCnt;
        dpb->currentOut->toBeDisplayed = toBeDisplayed;
        if(!dpb->noReordering)
            dpb->fullness++;
    }
    /* IDR picture */
    else if(isIdr) {

        /* h264bsdCheckGapsInFrameNum not called for IDR pictures -> have to
         * reset numOut and outIndex here */
        dpb->numOut = dpb->outIndex=0;

        /* flush the buffer */
        Mmcop5(dpb);
        /* if noOutputOfPriorPicsFlag was set -> the pictures preceding the
         * IDR picture shall not be output -> set output buffer empty */
        if(mark->noOutputOfPriorPicsFlag || dpb->noReordering) {
            dpb->numOut=0;
            dpb->outIndex=0;
        }

        if(mark->longTermReferenceFlag) {
            dpb->currentOut->status = LONG_TERM;
            dpb->maxLongTermFrameIdx=0;
        }
        else {
            dpb->currentOut->status = SHORT_TERM;
            dpb->maxLongTermFrameIdx = NO_LONG_TERM_FRAME_INDICES;
        }
        dpb->currentOut->frameNum =0;
        dpb->currentOut->picNum   =0;
        dpb->currentOut->picOrderCnt=0;
        dpb->currentOut->toBeDisplayed = toBeDisplayed;
        dpb->fullness = 1;
        dpb->numRefFrames = 1;
    }
    /* reference picture */
    else {
        markedAsLongTerm = HANTRO_FALSE;
        if(mark->adaptiveRefPicMarkingModeFlag) {
            i=0;
            while(mark->operation[i].memoryManagementControlOperation) {
                switch(mark->operation[i].memoryManagementControlOperation) {
                case 1:
                    status = Mmcop1(
                      dpb,
                      frameNum,
                      mark->operation[i].differenceOfPicNums);
                    break;
                case 2:
                    status = Mmcop2(dpb, mark->operation[i].longTermPicNum);
                    break;
                case 3:
                    status =  Mmcop3(
                      dpb,
                      frameNum,
                      mark->operation[i].differenceOfPicNums,
                      mark->operation[i].longTermFrameIdx);
                    break;
                case 4:
                    status = Mmcop4(
                      dpb,
                      mark->operation[i].maxLongTermFrameIdx);
                    break;

                case 5:
                    status = Mmcop5(dpb);
                    dpb->lastContainsMmco5 = HANTRO_TRUE;
                    frameNum=0;
                    break;

                case 6:
                    status = Mmcop6(
                      dpb,
                      frameNum,
                      picOrderCnt,
                      mark->operation[i].longTermFrameIdx);
                    if(status == HANTRO_OK)
                        markedAsLongTerm = HANTRO_TRUE;
                    break;

                default: /* invalid memory management control operation */
                    status = HANTRO_NOK;
                    break;
	                }
                if(status != HANTRO_OK)
                    break;
                i++;
            }
		      }
        else
          status = SlidingWindowRefPicMarking(dpb);
        /* if current picture was not marked as long-term reference by
         * memory management control operation 6 -> mark current as short
         * term and insert it into dpb (if there is room) */
        if(!markedAsLongTerm) {
            if(dpb->numRefFrames < dpb->maxRefFrames) {
                dpb->currentOut->frameNum = frameNum;
                dpb->currentOut->picNum   = (int32_t)frameNum;
                dpb->currentOut->picOrderCnt = picOrderCnt;
                dpb->currentOut->status   = SHORT_TERM;
                dpb->currentOut->toBeDisplayed = toBeDisplayed;
                dpb->fullness++;
                dpb->numRefFrames++;
            }
            /* no room */
            else
              status = HANTRO_NOK;
        }
		  }

    dpb->currentOut->isIdr = isIdr;
    dpb->currentOut->picId = currentPicId;
    dpb->currentOut->numErrMbs = numErrMbs;

    /* dpb was initialized to not to reorder the pictures -> output current
     * picture immediately */
    if(dpb->noReordering) {
        ASSERT(dpb->numOut == 0);
        ASSERT(dpb->outIndex == 0);
        dpb->outBuf[dpb->numOut].data  = dpb->currentOut->data;
        dpb->outBuf[dpb->numOut].isIdr = dpb->currentOut->isIdr;
        dpb->outBuf[dpb->numOut].picId = dpb->currentOut->picId;
        dpb->outBuf[dpb->numOut].numErrMbs = dpb->currentOut->numErrMbs;
        dpb->numOut++;
	    }
    else {
      /* output pictures if buffer full */
      while(dpb->fullness > dpb->dpbSize) {
        i = OutputPicture(dpb);
        ASSERT(i == HANTRO_OK);
        }
			}

    /* sort dpb */
    ShellSort(dpb->buffer, dpb->dpbSize+1);

    return(status);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdGetRefPicData

        Functional description:
            Function to get reference picture data from the reference picture
            list

        Returns:
            pointer to desired reference picture data
            NULL if invalid index or non-existing picture referred

------------------------------------------------------------------------------*/
uint8_t* h264bsdGetRefPicData(dpbStorage_t *dpb, uint32_t index) {

	if(index > 16 || !dpb->list[index])
        return NULL;
    else if(!IS_EXISTING(*dpb->list[index]))
        return NULL;
    else
        return(dpb->list[index]->data);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdAllocateDpbImage

        Functional description:
            function to allocate memory for a image. This function does not
            really allocate any memory but reserves one of the buffer
            positions for decoding of current picture

        Returns:
            pointer to memory area for the image


------------------------------------------------------------------------------*/
uint8_t* h264bsdAllocateDpbImage(dpbStorage_t *dpb) {

  ASSERT( !dpb->buffer[dpb->dpbSize].toBeDisplayed &&
          !IS_REFERENCE(dpb->buffer[dpb->dpbSize]));
  ASSERT(dpb->fullness <=  dpb->dpbSize);

  dpb->currentOut = dpb->buffer + dpb->dpbSize;

  return(dpb->currentOut->data);
	}

/*------------------------------------------------------------------------------

    Function: SlidingWindowRefPicMarking

        Functional description:
            Function to perform sliding window refence picture marking process.

        Outputs:
            HANTRO_OK      success
            HANTRO_NOK     failure, no short-term reference frame found that
                           could be marked unused


------------------------------------------------------------------------------*/
static uint32_t SlidingWindowRefPicMarking(dpbStorage_t *dpb) {
  int32_t index, picNum;
  uint32_t i;



  if(dpb->numRefFrames < dpb->maxRefFrames) {
      return(HANTRO_OK);
	  }
  else {
      index = -1;
      picNum=0;
      /* find the oldest short term picture */
      for(i=0; i < dpb->numRefFrames; i++)
          if(IS_SHORT_TERM(dpb->buffer[i]))
              if(dpb->buffer[i].picNum < picNum || index == -1) {
                  index = (int32_t)i;
                  picNum = dpb->buffer[i].picNum;
              }
      if(index >= 0) {
          SET_UNUSED(dpb->buffer[index]);
          dpb->numRefFrames--;
          if(!dpb->buffer[index].toBeDisplayed)
              dpb->fullness--;

          return(HANTRO_OK);
      }
    }

  return(HANTRO_NOK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdInitDpb

        Functional description:
            Function to initialize DPB. Reserves memories for the buffer,
            reference picture list and output buffer. dpbSize indicates
            the maximum DPB size indicated by the levelIdc in the stream.
            If noReordering flag is FALSE the DPB stores dpbSize pictures
            for display reordering purposes. On the other hand, if the
            flag is TRUE the DPB only stores maxRefFrames reference pictures
            and outputs all the pictures immediately.

        Inputs:
            picSizeInMbs    picture size in macroblocks
            dpbSize         size of the DPB (number of pictures)
            maxRefFrames    max number of reference frames
            maxFrameNum     max frame number
            noReordering    flag to indicate that DPB does not have to
                            prepare to reorder frames for display

        Outputs:
            dpb             pointer to dpb data storage

        Returns:
            HANTRO_OK       success
            MEMORY_ALLOCATION_ERROR if memory allocation failed

------------------------------------------------------------------------------*/
uint32_t h264bsdInitDpb(dpbStorage_t *dpb, uint32_t picSizeInMbs,
  uint32_t dpbSize, uint32_t maxRefFrames,
  uint32_t maxFrameNum, uint32_t noReordering) {
    uint32_t i;


    ASSERT(picSizeInMbs);
    ASSERT(maxRefFrames <= MAX_NUM_REF_PICS);
    ASSERT(maxRefFrames <= dpbSize);
    ASSERT(maxFrameNum);
    ASSERT(dpbSize);

    dpb->maxLongTermFrameIdx = NO_LONG_TERM_FRAME_INDICES;
    dpb->maxRefFrames        = MAX(maxRefFrames, 1);
    if(noReordering)
        dpb->dpbSize         = dpb->maxRefFrames;
    else
        dpb->dpbSize         = dpbSize;
    dpb->maxFrameNum         = maxFrameNum;
    dpb->noReordering        = noReordering;
    dpb->fullness           =0;
    dpb->numRefFrames       =0;
    dpb->prevRefFrameNum    =0;

    ALLOCATE(dpb->buffer, MAX_NUM_REF_IDX_L0_ACTIVE + 1, dpbPicture_t);
    if(dpb->buffer == NULL)
        return(MEMORY_ALLOCATION_ERROR);
    memset(dpb->buffer, 0,
            (MAX_NUM_REF_IDX_L0_ACTIVE + 1)*sizeof(dpbPicture_t));
    for(i=0; i < dpb->dpbSize + 1; i++) {
        /* Allocate needed amount of memory, which is:
         * image size + 32 + 15, where 32 cames from the fact that in ARM OpenMax
         * DL implementation Functions may read beyond the end of an array,
         * by a maximum of 32 bytes. And +15 cames for the need to align memory
         * to 16-byte boundary */
        ALLOCATE(dpb->buffer[i].pAllocatedData, (picSizeInMbs*384 + 32+15), uint8_t);
        if(dpb->buffer[i].pAllocatedData == NULL)
            return(MEMORY_ALLOCATION_ERROR);

        dpb->buffer[i].data = ALIGN(dpb->buffer[i].pAllocatedData, 16);
	    }

    ALLOCATE(dpb->list, MAX_NUM_REF_IDX_L0_ACTIVE + 1, dpbPicture_t*);
    ALLOCATE(dpb->outBuf, dpb->dpbSize+1, dpbOutPicture_t);

    if(dpb->list == NULL || dpb->outBuf == NULL)
        return(MEMORY_ALLOCATION_ERROR);

    memset(dpb->list, 0,
            ((MAX_NUM_REF_IDX_L0_ACTIVE + 1) * sizeof(dpbPicture_t*)));

    dpb->numOut = dpb->outIndex=0;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdResetDpb

        Functional description:
            Function to reset DPB. This function should be called when an IDR
            slice (other than the first) activates new sequence parameter set.
            Function calls h264bsdFreeDpb to free old allocated memories and
            h264bsdInitDpb to re-initialize the DPB. Same inputs, outputs and
            returns as for h264bsdInitDpb.

------------------------------------------------------------------------------*/
uint32_t h264bsdResetDpb(dpbStorage_t *dpb, uint32_t picSizeInMbs,
  uint32_t dpbSize, uint32_t maxRefFrames, uint32_t maxFrameNum,
  uint32_t noReordering) {


  ASSERT(picSizeInMbs);
  ASSERT(maxRefFrames <= MAX_NUM_REF_PICS);
  ASSERT(maxRefFrames <= dpbSize);
  ASSERT(maxFrameNum);
  ASSERT(dpbSize);

  h264bsdFreeDpb(dpb);

  return h264bsdInitDpb(dpb, picSizeInMbs, dpbSize, maxRefFrames,
                        maxFrameNum, noReordering);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdInitRefPicList

        Functional description:
            Function to initialize reference picture list. Function just
            sets pointers in the list according to pictures in the buffer.
            The buffer is assumed to contain pictures sorted according to
            what the H.264 standard says about initial reference picture list.

        Inputs:
            dpb     pointer to dpb data structure

        Outputs:
            dpb     'list' field initialized

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdInitRefPicList(dpbStorage_t *dpb) {
  uint32_t i;


  for(i=0; i < dpb->numRefFrames; i++)
      dpb->list[i] = &dpb->buffer[i];
	}

/*------------------------------------------------------------------------------

    Function: FindDpbPic

        Functional description:
            Function to find a reference picture from the buffer. The picture
            to be found is identified by picNum and isShortTerm flag.

        Returns:
            index of the picture in the buffer
            -1 if the specified picture was not found in the buffer

------------------------------------------------------------------------------*/
static int32_t FindDpbPic(dpbStorage_t *dpb, int32_t picNum, uint32_t isShortTerm) {
  uint32_t i=0;
  uint32_t found = HANTRO_FALSE;


  if(isShortTerm) {
      while(i < dpb->maxRefFrames && !found) {
          if(IS_SHORT_TERM(dpb->buffer[i]) &&
            dpb->buffer[i].picNum == picNum)
              found = HANTRO_TRUE;
          else
              i++;
      }
		}
  else {
      ASSERT(picNum >= 0);
      while(i < dpb->maxRefFrames && !found) {
          if(IS_LONG_TERM(dpb->buffer[i]) &&
            dpb->buffer[i].picNum == picNum)
              found = HANTRO_TRUE;
          else
              i++;
      }
	 }

  if(found)
      return((int32_t)i);
  else
      return(-1);
	}

/*------------------------------------------------------------------------------

    Function: SetPicNums

        Functional description:
            Function to set picNum values for short-term pictures in the
            buffer. Numbering of pictures is based on frame numbers and as
            frame numbers are modulo maxFrameNum -> frame numbers of older
            pictures in the buffer may be bigger than the currFrameNum.
            picNums will be set so that current frame has the largest picNum
            and all the short-term frames in the buffer will get smaller picNum
            representing their "distance" from the current frame. This
            function kind of maps the modulo arithmetic back to normal.

------------------------------------------------------------------------------*/
static void SetPicNums(dpbStorage_t *dpb, uint32_t currFrameNum) {
  uint32_t i;
  int32_t frameNumWrap;


  ASSERT(dpb);
  ASSERT(currFrameNum < dpb->maxFrameNum);

  for(i=0; i < dpb->numRefFrames; i++)
      if(IS_SHORT_TERM(dpb->buffer[i])) {
          if(dpb->buffer[i].frameNum > currFrameNum)
              frameNumWrap =
                  (int32_t)dpb->buffer[i].frameNum - (int32_t)dpb->maxFrameNum;
          else
              frameNumWrap = (int32_t)dpb->buffer[i].frameNum;
          dpb->buffer[i].picNum = frameNumWrap;
      }

	}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckGapsInFrameNum

        Functional description:
            Function to check gaps in frame_num and generate non-existing
            (short term) reference pictures if necessary. This function should
            be called only for non-IDR pictures.

        Inputs:
            dpb         pointer to dpb data structure
            frameNum    frame number of the current picture
            isRefPic    flag to indicate if current picture is a reference or
                        non-reference picture
            gapsAllowed Flag which indicates active SPS stance on whether
                        to allow gaps

        Outputs:
            dpb         'buffer' possibly modified by inserting non-existing
                        pictures with sliding window marking process

        Returns:
            HANTRO_OK   success
            HANTRO_NOK  error in sliding window reference picture marking or
                        frameNum equal to previous reference frame used for
                        a reference picture

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckGapsInFrameNum(dpbStorage_t *dpb, uint32_t frameNum, uint32_t isRefPic,
                               uint32_t gapsAllowed) {
    uint32_t unUsedShortTermFrameNum;
    uint8_t *tmp;


    ASSERT(dpb);
    ASSERT(dpb->fullness <= dpb->dpbSize);
    ASSERT(frameNum < dpb->maxFrameNum);

    dpb->numOut=0;
    dpb->outIndex=0;

    if(!gapsAllowed)
        return(HANTRO_OK);

    if((frameNum != dpb->prevRefFrameNum) &&
         (frameNum != ((dpb->prevRefFrameNum + 1) % dpb->maxFrameNum))) {

        unUsedShortTermFrameNum = (dpb->prevRefFrameNum + 1) % dpb->maxFrameNum;

        /* store data pointer of last buffer position to be used as next
         * "allocated" data pointer if last buffer position after this process
         * contains data pointer located in outBuf (buffer placed in the output
         * shall not be overwritten by the current picture) */
        tmp = dpb->buffer[dpb->dpbSize].data;
        do{
            SetPicNums(dpb, unUsedShortTermFrameNum);

            if(SlidingWindowRefPicMarking(dpb) != HANTRO_OK)
                return(HANTRO_NOK);

            /* output pictures if buffer full */
            while(dpb->fullness >= dpb->dpbSize) {
#ifdef _ASSERT_USED
                ASSERT(!dpb->noReordering);
                ASSERT(OutputPicture(dpb) == HANTRO_OK);
#else
                OutputPicture(dpb);
#endif
            }

            /* add to end of list */
            ASSERT( !dpb->buffer[dpb->dpbSize].toBeDisplayed &&
                    !IS_REFERENCE(dpb->buffer[dpb->dpbSize]));
            dpb->buffer[dpb->dpbSize].status = NON_EXISTING;
            dpb->buffer[dpb->dpbSize].frameNum = unUsedShortTermFrameNum;
            dpb->buffer[dpb->dpbSize].picNum   = (int32_t)unUsedShortTermFrameNum;
            dpb->buffer[dpb->dpbSize].picOrderCnt=0;
            dpb->buffer[dpb->dpbSize].toBeDisplayed = HANTRO_FALSE;
            dpb->fullness++;
            dpb->numRefFrames++;

            /* sort the buffer */
            ShellSort(dpb->buffer, dpb->dpbSize+1);

            unUsedShortTermFrameNum = (unUsedShortTermFrameNum + 1) %
                dpb->maxFrameNum;

					} while(unUsedShortTermFrameNum != frameNum);

        /* pictures placed in output buffer -> check that 'data' in
         * buffer position dpbSize is not in the output buffer (this will be
         * "allocated" by h264bsdAllocateDpbImage). If it is -> exchange data
         * pointer with the one stored in the beginning */
        if(dpb->numOut) {
            uint32_t i;

            for(i=0; i < dpb->numOut; i++) {
                if(dpb->outBuf[i].data == dpb->buffer[dpb->dpbSize].data) {
                    /* find buffer position containing data pointer stored in
                     * tmp */
                    for(i=0; i < dpb->dpbSize; i++)  {
                        if(dpb->buffer[i].data == tmp)      {
                            dpb->buffer[i].data =
                                dpb->buffer[dpb->dpbSize].data;
                            dpb->buffer[dpb->dpbSize].data = tmp;
                            break;
                        }
	                    }
                    ASSERT(i < dpb->dpbSize);
                    break;
                }
            }
        }
    }
  /* frameNum for reference pictures shall not be the same as for previous
   * reference picture, otherwise accesses to pictures in the buffer cannot
   * be solved unambiguously */
  else if(isRefPic && frameNum == dpb->prevRefFrameNum)
      return(HANTRO_NOK);

  /* save current frame_num in prevRefFrameNum. For non-reference frame
   * prevFrameNum is set to frame number of last non-existing frame above */
  if(isRefPic)
    dpb->prevRefFrameNum = frameNum;
  else if(frameNum != dpb->prevRefFrameNum)
    dpb->prevRefFrameNum = (frameNum + dpb->maxFrameNum - 1) % dpb->maxFrameNum;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: FindSmallestPicOrderCnt

        Functional description:
            Function to find picture with smallest picture order count. This
            will be the next picture in display order.

        Returns:
            pointer to the picture, NULL if no pictures to be displayed

------------------------------------------------------------------------------*/
dpbPicture_t* FindSmallestPicOrderCnt(dpbStorage_t *dpb) {
    uint32_t i;
    int32_t picOrderCnt;
    dpbPicture_t *tmp;


    ASSERT(dpb);

    picOrderCnt=0x7FFFFFFF;
    tmp = NULL;

    for(i=0; i <= dpb->dpbSize; i++) {
        if(dpb->buffer[i].toBeDisplayed &&
            (dpb->buffer[i].picOrderCnt < picOrderCnt)) {
            tmp = dpb->buffer + i;
            picOrderCnt = dpb->buffer[i].picOrderCnt;
        }
    }

  return tmp;
	}

/*------------------------------------------------------------------------------

    Function: OutputPicture

        Functional description:
            Function to put next display order picture into the output buffer.

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     no pictures to display

------------------------------------------------------------------------------*/
uint32_t OutputPicture(dpbStorage_t *dpb) {
  dpbPicture_t *tmp;

  ASSERT(dpb);

  if(dpb->noReordering)
      return(HANTRO_NOK);

  tmp = FindSmallestPicOrderCnt(dpb);

  /* no pictures to be displayed */
  if(tmp == NULL)
      return(HANTRO_NOK);

  dpb->outBuf[dpb->numOut].data  = tmp->data;
  dpb->outBuf[dpb->numOut].isIdr = tmp->isIdr;
  dpb->outBuf[dpb->numOut].picId = tmp->picId;
  dpb->outBuf[dpb->numOut].numErrMbs = tmp->numErrMbs;
  dpb->numOut++;

  tmp->toBeDisplayed = HANTRO_FALSE;
  if(!IS_REFERENCE(*tmp)) 
      dpb->fullness--;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdDpbOutputPicture

        Functional description:
            Function to get next display order picture from the output buffer.

        Return:
            pointer to output picture structure, NULL if no pictures to display

------------------------------------------------------------------------------*/
dpbOutPicture_t* h264bsdDpbOutputPicture(dpbStorage_t *dpb) {

  ASSERT(dpb);

  if(dpb->outIndex < dpb->numOut)
    return(dpb->outBuf + dpb->outIndex++);
  else
    return NULL;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdFlushDpb

        Functional description:
            Function to flush the DPB. Function puts all pictures needed for
            display into the output buffer. This function shall be called in
            the end of the stream to obtain pictures buffered for display
            re-ordering purposes.

------------------------------------------------------------------------------*/
void h264bsdFlushDpb(dpbStorage_t *dpb) {

  /* don't do anything if buffer not reserved */
  if(dpb->buffer) {
    dpb->flushed = 1;
    /* output all pictures */
    while(OutputPicture(dpb) == HANTRO_OK)
        ;
    }

	}

/*------------------------------------------------------------------------------

    Function: h264bsdFreeDpb

        Functional description:
            Function to free memories reserved for the DPB.

------------------------------------------------------------------------------*/
void h264bsdFreeDpb(dpbStorage_t *dpb) {
  uint32_t i;


  ASSERT(dpb);

  if(dpb->buffer) {
    for(i=0; i < dpb->dpbSize+1; i++)
      FREE(dpb->buffer[i].pAllocatedData);
		}
  FREE(dpb->buffer);
  FREE(dpb->list);
  FREE(dpb->outBuf);
	}

/*------------------------------------------------------------------------------

    Function: ShellSort

        Functional description:
            Sort pictures in the buffer. Function implements Shell's method,
            i.e. diminishing increment sort. See e.g. "Numerical Recipes in C"
            for more information.

------------------------------------------------------------------------------*/
static void ShellSort(dpbPicture_t *pPic, uint32_t num) {
    uint32_t i, j;
    uint32_t step;
    dpbPicture_t tmpPic;

    step = 7;

    while(step) {
        for(i = step; i < num; i++) {
            tmpPic = pPic[i];
            j = i;
            while(j >= step && ComparePictures(pPic + j - step, &tmpPic) > 0) {
                pPic[j] = pPic[j-step];
                j -= step;
            }
            pPic[j] = tmpPic;
        }
        step >>= 1;
    }

	}



/* x- and y-coordinates for each block, defined in h264bsd_intra_prediction.c */
extern const uint32_t h264bsdBlockX[];
extern const uint32_t h264bsdBlockY[];

/* clipping table, defined in h264bsd_intra_prediction.c */
extern const uint8_t h264bsdClip[];


/*------------------------------------------------------------------------------

    Function: h264bsdWriteMacroblock

        Functional description:
            Write one macroblock into the image. Both luma and chroma
            components will be written at the same time.

        Inputs:
            data    pointer to macroblock data to be written, 256 values for
                    luma followed by 64 values for both chroma components

        Outputs:
            image   pointer to the image where the macroblock will be written

        Returns:
            none

------------------------------------------------------------------------------*/
#ifndef H264DEC_NEON
void h264bsdWriteMacroblock(image_t *image, uint8_t *data) {
    uint32_t i;
    uint32_t width;
    uint32_t *lum, *cb, *cr;
    uint32_t *ptr;
    uint32_t tmp1, tmp2;



    ASSERT(image);
    ASSERT(data);
    ASSERT(!((uint32_t)data&0x3));

    width = image->width;

    /*lint -save -e826 lum, cb and cr used to copy 4 bytes at the time, disable
     * "area too small" info message */
    lum = (uint32_t*)image->luma;
    cb = (uint32_t*)image->cb;
    cr = (uint32_t*)image->cr;
    ASSERT(!((uint32_t)lum&0x3));
    ASSERT(!((uint32_t)cb&0x3));
    ASSERT(!((uint32_t)cr&0x3));

    ptr = (uint32_t*)data;

    width *= 4;
    for(i = 16; i ; i--) {
      tmp1 = *ptr++;
      tmp2 = *ptr++;
      *lum++ = tmp1;
      *lum++ = tmp2;
      tmp1 = *ptr++;
      tmp2 = *ptr++;
      *lum++ = tmp1;
      *lum++ = tmp2;
      lum += width-4;
    }

  width >>= 1;
  for(i = 8; i ; i--) {
    tmp1 = *ptr++;
    tmp2 = *ptr++;
    *cb++ = tmp1;
    *cb++ = tmp2;
    cb += width-2;
    }

  for(i=8; i ; i--) {
    tmp1 = *ptr++;
    tmp2 = *ptr++;
    *cr++ = tmp1;
    *cr++ = tmp2;
    cr += width-2;
    }

	}
#endif

#ifndef H264DEC_OMXDL
/*------------------------------------------------------------------------------

    Function: h264bsdWriteOutputBlocks

        Functional description:
            Write one macroblock into the image. Prediction for the macroblock
            and the residual are given separately and will be combined while
            writing the data to the image

        Inputs:
            data        pointer to macroblock prediction data, 256 values for
                        luma followed by 64 values for both chroma components
            mbNum       number of the macroblock
            residual    pointer to residual data, 16 16-element arrays for luma
                        followed by 4 16-element arrays for both chroma
                        components

        Outputs:
            image       pointer to the image where the data will be written

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdWriteOutputBlocks(image_t *image, uint32_t mbNum, uint8_t *data,
        int32_t residual[][16]) {
    uint32_t i;
    uint32_t picWidth, picSize;
    uint8_t *lum, *cb, *cr;
    uint8_t *imageBlock;
    uint8_t *tmp;
    uint32_t row, col;
    uint32_t block;
    uint32_t x, y;
    int32_t *pRes;
    int32_t tmp1, tmp2, tmp3, tmp4;
    const uint8_t *clp = h264bsdClip + 512;


    ASSERT(image);
    ASSERT(data);
    ASSERT(mbNum < image->width * image->height);
    ASSERT(!((uint32_t)data&0x3));

    /* Image size in macroblocks */
    picWidth = image->width;
    picSize = picWidth * image->height;
    row = mbNum / picWidth;
    col = mbNum % picWidth;

    /* Output macroblock position in output picture */
    lum = (image->data + row * picWidth * 256 + col * MACROBLOCK_SIZE);
    cb = (image->data + picSize * 256 + row * picWidth * 64 + col * 8);
    cr = (cb + picSize * 64);

    picWidth *= 16;

    for(block=0; block < 16; block++) {
        x = h264bsdBlockX[block];
        y = h264bsdBlockY[block];

        pRes = residual[block];

        ASSERT(pRes);

        tmp = data + y*16 + x;
        imageBlock = lum + y*picWidth + x;

        ASSERT(!((uint32_t)tmp&0x3));
        ASSERT(!((uint32_t)imageBlock&0x3));

        if(IS_RESIDUAL_EMPTY(pRes)) {
            /*lint -e826 */
            int32_t *in32 = (int32_t*)tmp;
            int32_t *out32 = (int32_t*)imageBlock;

            /* Residual is zero => copy prediction block to output */
            tmp1 = *in32;  in32 += 4;
            tmp2 = *in32;  in32 += 4;
            *out32 = tmp1; out32 += picWidth/4;
            *out32 = tmp2; out32 += picWidth/4;
            tmp1 = *in32;  in32 += 4;
            tmp2 = *in32;
            *out32 = tmp1; out32 += picWidth/4;
            *out32 = tmp2;
        }
        else {

            RANGE_CHECK_ARRAY(pRes, -512, 511, 16);

            /* Calculate image = prediction + residual
             * Process four pixels in a loop */
            for(i = 4; i; i--) {
              tmp1 = tmp[0];
              tmp2 = *pRes++;
              tmp3 = tmp[1];
              tmp1 = clp[tmp1 + tmp2];
              tmp4 = *pRes++;
              imageBlock[0] = (uint8_t)tmp1;
              tmp3 = clp[tmp3 + tmp4];
              tmp1 = tmp[2];
              tmp2 = *pRes++;
              imageBlock[1] = (uint8_t)tmp3;
              tmp1 = clp[tmp1 + tmp2];
              tmp3 = tmp[3];
              tmp4 = *pRes++;
              imageBlock[2] = (uint8_t)tmp1;
              tmp3 = clp[tmp3 + tmp4];
              tmp += 16;
              imageBlock[3] = (uint8_t)tmp3;
              imageBlock += picWidth;
	            }
        }

    }

    picWidth /= 2;

    for(block = 16; block <= 23; block++) {
        x = h264bsdBlockX[block & 0x3];
        y = h264bsdBlockY[block & 0x3];

        pRes = residual[block];

        ASSERT(pRes);

        tmp = data + 256;
        imageBlock = cb;

        if(block >= 20) {
            imageBlock = cr;
            tmp += 64;
        }

        tmp += y*8 + x;
        imageBlock += y*picWidth + x;

        ASSERT(!((uint32_t)tmp&0x3));
        ASSERT(!((uint32_t)imageBlock&0x3));

        if(IS_RESIDUAL_EMPTY(pRes)) {
            /*lint -e826 */
            int32_t *in32 = (int32_t*)tmp;
            int32_t *out32 = (int32_t*)imageBlock;

            /* Residual is zero => copy prediction block to output */
            tmp1 = *in32;  in32 += 2;
            tmp2 = *in32;  in32 += 2;
            *out32 = tmp1; out32 += picWidth/4;
            *out32 = tmp2; out32 += picWidth/4;
            tmp1 = *in32;  in32 += 2;
            tmp2 = *in32;
            *out32 = tmp1; out32 += picWidth/4;
            *out32 = tmp2;
        }
        else {

            RANGE_CHECK_ARRAY(pRes, -512, 511, 16);

            for(i = 4; i; i--) {
                tmp1 = tmp[0];
                tmp2 = *pRes++;
                tmp3 = tmp[1];
                tmp1 = clp[tmp1 + tmp2];
                tmp4 = *pRes++;
                imageBlock[0] = (uint8_t)tmp1;
                tmp3 = clp[tmp3 + tmp4];
                tmp1 = tmp[2];
                tmp2 = *pRes++;
                imageBlock[1] = (uint8_t)tmp3;
                tmp1 = clp[tmp1 + tmp2];
                tmp3 = tmp[3];
                tmp4 = *pRes++;
                imageBlock[2] = (uint8_t)tmp1;
                tmp3 = clp[tmp3 + tmp4];
                tmp += 8;
                imageBlock[3] = (uint8_t)tmp3;
                imageBlock += picWidth;
            }
        }
    }

	}
#endif /* H264DEC_OMXDL */



typedef struct {
  uint32_t available;
  uint32_t refIndex;
  mv_t mv;
	} interNeighbour_t;


static uint32_t MvPrediction16x16(mbStorage_t *pMb, mbPred_t *mbPred,
    dpbStorage_t *dpb);
static uint32_t MvPrediction16x8(mbStorage_t *pMb, mbPred_t *mbPred,
    dpbStorage_t *dpb);
static uint32_t MvPrediction8x16(mbStorage_t *pMb, mbPred_t *mbPred,
    dpbStorage_t *dpb);
static uint32_t MvPrediction8x8(mbStorage_t *pMb, subMbPred_t *subMbPred,
    dpbStorage_t *dpb);
static uint32_t MvPrediction(mbStorage_t *pMb, subMbPred_t *subMbPred,
    uint32_t mbPartIdx, uint32_t subMbPartIdx);
static int32_t MedianFilter(int32_t a, int32_t b, int32_t c);

static void GetInterNeighbour(uint32_t sliceId, mbStorage_t *nMb,
    interNeighbour_t *n, uint32_t index);
static void GetPredictionMv(mv_t *mv, interNeighbour_t *a, uint32_t refIndex);

static const neighbour_t N_A_SUB_PART[4][4][4] = {
	{ { {MB_A,5}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_A,5}, {MB_A,7}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_A,5}, {MB_CURR,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_A,5}, {MB_CURR,0}, {MB_A,7}, {MB_CURR,2} } },

	{ { {MB_CURR,1}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,1}, {MB_CURR,3}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,1}, {MB_CURR,4}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,1}, {MB_CURR,4}, {MB_CURR,3}, {MB_CURR,6} } },

	{ { {MB_A,13}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_A,13}, {MB_A,15}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_A,13}, {MB_CURR,8}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_A,13}, {MB_CURR,8}, {MB_A,15}, {MB_CURR,10} } },

	{ { {MB_CURR,9}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,9}, {MB_CURR,11}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,9}, {MB_CURR,12}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,9}, {MB_CURR,12}, {MB_CURR,11}, {MB_CURR,14} } } };

static const neighbour_t N_B_SUB_PART[4][4][4] = {
	{ { {MB_B,10}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,10}, {MB_CURR,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,10}, {MB_B,11}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,10}, {MB_B,11}, {MB_CURR,0}, {MB_CURR,1} } },

	{ { {MB_B,14}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,14}, {MB_CURR,4}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,14}, {MB_B,15}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,14}, {MB_B,15}, {MB_CURR,4}, {MB_CURR,5} } },

	{ { {MB_CURR,2}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,2}, {MB_CURR,8}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,2}, {MB_CURR,3}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,2}, {MB_CURR,3}, {MB_CURR,8}, {MB_CURR,9} } },

	{ { {MB_CURR,6}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,6}, {MB_CURR,12}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,6}, {MB_CURR,7}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,6}, {MB_CURR,7}, {MB_CURR,12}, {MB_CURR,13} } } };

static const neighbour_t N_C_SUB_PART[4][4][4] = {
	{ { {MB_B,14}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,14}, {MB_NA,4}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,11}, {MB_B,14}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,11}, {MB_B,14}, {MB_CURR,1}, {MB_NA,4} } },

	{ { {MB_C,10}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_C,10}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,15}, {MB_C,10}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,15}, {MB_C,10}, {MB_CURR,5}, {MB_NA,0} } },

	{ { {MB_CURR,6}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,6}, {MB_NA,12}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,3}, {MB_CURR,6}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,3}, {MB_CURR,6}, {MB_CURR,9}, {MB_NA,12} } },

	{ { {MB_NA,2}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_NA,2}, {MB_NA,8}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,7}, {MB_NA,2}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,7}, {MB_NA,2}, {MB_CURR,13}, {MB_NA,8} } } };

static const neighbour_t N_D_SUB_PART[4][4][4] = {
	{ { {MB_D,15}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_D,15}, {MB_A,5}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_D,15}, {MB_B,10}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_D,15}, {MB_B,10}, {MB_A,5}, {MB_CURR,0} } },

	{ { {MB_B,11}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,11}, {MB_CURR,1}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,11}, {MB_B,14}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_B,11}, {MB_B,14}, {MB_CURR,1}, {MB_CURR,4} } },

	{ { {MB_A,7}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_A,7}, {MB_A,13}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_A,7}, {MB_CURR,2}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_A,7}, {MB_CURR,2}, {MB_A,13}, {MB_CURR,8} } },

	{ { {MB_CURR,3}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,3}, {MB_CURR,9}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,3}, {MB_CURR,6}, {MB_NA,0}, {MB_NA,0} },
		{ {MB_CURR,3}, {MB_CURR,6}, {MB_CURR,9}, {MB_CURR,12} } } };


#ifdef H264DEC_OMXDL
/*------------------------------------------------------------------------------

    Function: h264bsdInterPrediction

        Functional description:
          Processes one inter macroblock. Performs motion vector prediction
          and reconstructs prediction macroblock. Writes the final macroblock
          (prediction + residual) into the output image (currImage)

        Inputs:
          pMb           pointer to macroblock specific information
          pMbLayer      pointer to current macroblock data from stream
          dpb           pointer to decoded picture buffer
          mbNum         current macroblock number
          currImage     pointer to output image
          data          pointer where predicted macroblock will be stored

        Outputs:
          pMb           structure is updated with current macroblock
          currImage     current macroblock is written into image
          data          prediction is stored here

        Returns:
          HANTRO_OK     success
          HANTRO_NOK    error in motion vector prediction

------------------------------------------------------------------------------*/
uint32_t h264bsdInterPrediction(mbStorage_t *pMb, macroblockLayer_t *pMbLayer,
    dpbStorage_t *dpb, uint32_t mbNum, image_t *currImage, uint8_t *data) {
    uint32_t i;
    uint32_t x, y;
    uint32_t colAndRow;
    subMbPartMode_e subPartMode;
    image_t refImage;
    uint8_t fillBuff[32*21 + 15 + 32];
    uint8_t *pFill;
    uint32_t tmp;


    ASSERT(pMb);
    ASSERT(h264bsdMbPartPredMode(pMb->mbType) == PRED_MODE_INTER);
    ASSERT(pMbLayer);

    /* 16-byte alignment */
    pFill = ALIGN(fillBuff, 16);

    /* set row bits 15:0 */
    colAndRow = mbNum / currImage->width;
    /*set col to bits 31:16 */
    colAndRow += (mbNum - colAndRow * currImage->width) << 16;
    colAndRow <<= 4;

    refImage.width = currImage->width;
    refImage.height = currImage->height;

    switch(pMb->mbType) {
        case P_Skip:
        case P_L0_16x16:
            if(MvPrediction16x16(pMb, &pMbLayer->mbPred, dpb) != HANTRO_OK)
                return(HANTRO_NOK);
            refImage.data = pMb->refAddr[0];
            tmp = (0<<24) + (0<<16) + (16<<8) + 16;
            h264bsdPredictSamples(data, pMb->mv, &refImage,
                                    colAndRow, tmp, pFill);
            break;

        case P_L0_L0_16x8:
            if( MvPrediction16x8(pMb, &pMbLayer->mbPred, dpb) != HANTRO_OK)
                return(HANTRO_NOK);
            refImage.data = pMb->refAddr[0];
            tmp = (0<<24) + (0<<16) + (16<<8) + 8;
            h264bsdPredictSamples(data, pMb->mv, &refImage,
                                    colAndRow, tmp, pFill);

            refImage.data = pMb->refAddr[2];
            tmp = (0<<24) + (8<<16) + (16<<8) + 8;
            h264bsdPredictSamples(data, pMb->mv+8, &refImage,
                                    colAndRow, tmp, pFill);
            break;

        case P_L0_L0_8x16:
            if( MvPrediction8x16(pMb, &pMbLayer->mbPred, dpb) != HANTRO_OK)
                return(HANTRO_NOK);
            refImage.data = pMb->refAddr[0];
            tmp = (0<<24) + (0<<16) + (8<<8) + 16;
            h264bsdPredictSamples(data, pMb->mv, &refImage,
                                    colAndRow, tmp, pFill);
            refImage.data = pMb->refAddr[1];
            tmp = (8<<24) + (0<<16) + (8<<8) + 16;
            h264bsdPredictSamples(data, pMb->mv+4, &refImage,
                                    colAndRow, tmp, pFill);
            break;

        default: /* P_8x8 and P_8x8ref0 */
            if( MvPrediction8x8(pMb, &pMbLayer->subMbPred, dpb) != HANTRO_OK)
                return(HANTRO_NOK);
            for(i=0; i < 4; i++) {
              refImage.data = pMb->refAddr[i];
              subPartMode =
                  h264bsdSubMbPartMode(pMbLayer->subMbPred.subMbType[i]);
              x = i & 0x1 ? 8 : 0;
              y = i < 2 ? 0 : 8;
              switch(subPartMode) {
                case MB_SP_8x8:
                  tmp = (x<<24) + (y<<16) + (8<<8) + 8;
                  h264bsdPredictSamples(data, pMb->mv+4*i, &refImage,
                                              colAndRow, tmp, pFill);
                    break;

                case MB_SP_8x4:
                  tmp = (x<<24) + (y<<16) + (8<<8) + 4;
                  h264bsdPredictSamples(data, pMb->mv+4*i, &refImage,
                                              colAndRow, tmp, pFill);
                  tmp = (x<<24) + ((y+4)<<16) + (8<<8) + 4;
                  h264bsdPredictSamples(data, pMb->mv+4*i+2, &refImage,
                                              colAndRow, tmp, pFill);
                    break;

                case MB_SP_4x8:
                  tmp = (x<<24) + (y<<16) + (4<<8) + 8;
                  h264bsdPredictSamples(data, pMb->mv+4*i, &refImage,
                                              colAndRow, tmp, pFill);
                  tmp = ((x+4)<<24) + (y<<16) + (4<<8) + 8;
                  h264bsdPredictSamples(data, pMb->mv+4*i+1, &refImage,
                                              colAndRow, tmp, pFill);
                    break;
                default:
                  tmp = (x<<24) + (y<<16) + (4<<8) + 4;
                  h264bsdPredictSamples(data, pMb->mv+4*i, &refImage,
                                              colAndRow, tmp, pFill);
                  tmp = ((x+4)<<24) + (y<<16) + (4<<8) + 4;
                  h264bsdPredictSamples(data, pMb->mv+4*i+1, &refImage,
                                              colAndRow, tmp, pFill);
                  tmp = (x<<24) + ((y+4)<<16) + (4<<8) + 4;
                  h264bsdPredictSamples(data, pMb->mv+4*i+2, &refImage,
                                              colAndRow, tmp, pFill);
                  tmp = ((x+4)<<24) + ((y+4)<<16) + (4<<8) + 4;
                  h264bsdPredictSamples(data, pMb->mv+4*i+3, &refImage,
                                              colAndRow, tmp, pFill);
                  break;
                }
            }
          break;
	    }

    /* if decoded flag > 1 -> mb has already been successfully decoded and
     * written to output -> do not write again */
    if(pMb->decoded > 1)
        return HANTRO_OK;

  return(HANTRO_OK);
	}

#else /* H264DEC_OMXDL */

/*------------------------------------------------------------------------------

    Function: h264bsdInterPrediction

        Functional description:
          Processes one inter macroblock. Performs motion vector prediction
          and reconstructs prediction macroblock. Writes the final macroblock
          (prediction + residual) into the output image (currImage)

        Inputs:
          pMb           pointer to macroblock specific information
          pMbLayer      pointer to current macroblock data from stream
          dpb           pointer to decoded picture buffer
          mbNum         current macroblock number
          currImage     pointer to output image
          data          pointer where predicted macroblock will be stored

        Outputs:
          pMb           structure is updated with current macroblock
          currImage     current macroblock is written into image
          data          prediction is stored here

        Returns:
          HANTRO_OK     success
          HANTRO_NOK    error in motion vector prediction

------------------------------------------------------------------------------*/
uint32_t h264bsdInterPrediction(mbStorage_t *pMb, macroblockLayer_t *pMbLayer,
    dpbStorage_t *dpb, uint32_t mbNum, image_t *currImage, uint8_t *data) {
    uint32_t i;
    uint32_t x, y;
    uint32_t row, col;
    subMbPartMode_e subPartMode;
    image_t refImage;


    ASSERT(pMb);
    ASSERT(h264bsdMbPartPredMode(pMb->mbType) == PRED_MODE_INTER);
    ASSERT(pMbLayer);

    row = mbNum / currImage->width;
    col = mbNum - row * currImage->width;
    row *= 16;
    col *= 16;

    refImage.width = currImage->width;
    refImage.height = currImage->height;

    switch(pMb->mbType) {
        case P_Skip:
        case P_L0_16x16:
            if(MvPrediction16x16(pMb, &pMbLayer->mbPred, dpb) != HANTRO_OK)
                return(HANTRO_NOK);
            refImage.data = pMb->refAddr[0];
            h264bsdPredictSamples(data, pMb->mv, &refImage, col, row, 0, 0,
                16, 16);
            break;

        case P_L0_L0_16x8:
            if( MvPrediction16x8(pMb, &pMbLayer->mbPred, dpb) != HANTRO_OK)
                return(HANTRO_NOK);
            refImage.data = pMb->refAddr[0];
            h264bsdPredictSamples(data, pMb->mv, &refImage, col, row, 0, 0,
                16, 8);
            refImage.data = pMb->refAddr[2];
            h264bsdPredictSamples(data, pMb->mv+8, &refImage, col, row, 0, 8,
                16, 8);
            break;

        case P_L0_L0_8x16:
            if( MvPrediction8x16(pMb, &pMbLayer->mbPred, dpb) != HANTRO_OK)
                return(HANTRO_NOK);
            refImage.data = pMb->refAddr[0];
            h264bsdPredictSamples(data, pMb->mv, &refImage, col, row, 0, 0,
                8, 16);
            refImage.data = pMb->refAddr[1];
            h264bsdPredictSamples(data, pMb->mv+4, &refImage, col, row, 8, 0,
                8, 16);
            break;

        default: /* P_8x8 and P_8x8ref0 */
            if( MvPrediction8x8(pMb, &pMbLayer->subMbPred, dpb) != HANTRO_OK)
                return(HANTRO_NOK);
            for(i=0; i < 4; i++) {
                refImage.data = pMb->refAddr[i];
                subPartMode =
                    h264bsdSubMbPartMode(pMbLayer->subMbPred.subMbType[i]);
                x = i & 0x1 ? 8 : 0;
                y = i < 2 ? 0 : 8;
                switch(subPartMode) {
                  case MB_SP_8x8:
                    h264bsdPredictSamples(data, pMb->mv+4*i, &refImage,
                        col, row, x, y, 8, 8);
                    break;

                  case MB_SP_8x4:
                    h264bsdPredictSamples(data, pMb->mv+4*i, &refImage,
                        col, row, x, y, 8, 4);
                    h264bsdPredictSamples(data, pMb->mv+4*i+2, &refImage,
                        col, row, x, y+4, 8, 4);
                    break;

                  case MB_SP_4x8:
                    h264bsdPredictSamples(data, pMb->mv+4*i, &refImage,
                        col, row, x, y, 4, 8);
                    h264bsdPredictSamples(data, pMb->mv+4*i+1, &refImage,
                        col, row, x+4, y, 4, 8);
                    break;

                  default:
                    h264bsdPredictSamples(data, pMb->mv+4*i, &refImage,
                        col, row, x, y, 4, 4);
                    h264bsdPredictSamples(data, pMb->mv+4*i+1, &refImage,
                        col, row, x+4, y, 4, 4);
                    h264bsdPredictSamples(data, pMb->mv+4*i+2, &refImage,
                        col, row, x, y+4, 4, 4);
                    h264bsdPredictSamples(data, pMb->mv+4*i+3, &refImage,
                        col, row, x+4, y+4, 4, 4);
                    break;
                }
            }
            break;
			}

    /* if decoded flag > 1 -> mb has already been successfully decoded and
     * written to output -> do not write again */
    if(pMb->decoded > 1)
        return HANTRO_OK;

    if(pMb->mbType != P_Skip)
        h264bsdWriteOutputBlocks(currImage, mbNum, data,
            pMbLayer->residual.level);
    else
        h264bsdWriteMacroblock(currImage, data);

  return(HANTRO_OK);
	}
#endif /* H264DEC_OMXDL */

/*------------------------------------------------------------------------------

    Function: MvPrediction16x16

        Functional description:
            Motion vector prediction for 16x16 partition mode

------------------------------------------------------------------------------*/
uint32_t MvPrediction16x16(mbStorage_t *pMb, mbPred_t *mbPred, dpbStorage_t *dpb) {
  mv_t mv;
  mv_t mvPred;
  interNeighbour_t a[3]; /* A, B, C */
  uint32_t refIndex;
  uint8_t *tmp;
  uint32_t *tmpMv1, *tmpMv2;


  refIndex = mbPred->refIdxL0[0];

  GetInterNeighbour(pMb->sliceId, pMb->mbA, a, 5);
  GetInterNeighbour(pMb->sliceId, pMb->mbB, a+1, 10);
  /*lint --e(740)  Unusual pointer cast (incompatible indirect types) */
  tmpMv1 = (uint32_t*)(&a[0].mv); /* we test just that both MVs are zero */
  /*lint --e(740) */
  tmpMv2 = (uint32_t*)(&a[1].mv); /* i.e. a[0].mv.hor == 0 && a[0].mv.ver == 0 */
  if(pMb->mbType == P_Skip &&
      (!a[0].available || !a[1].available ||
       ( a[0].refIndex == 0 && ((uint32_t)(*tmpMv1) == 0)) ||
       ( a[1].refIndex == 0 && ((uint32_t)(*tmpMv2) == 0)))) {
          mv.hor = mv.ver=0;
    }
  else {
        mv = mbPred->mvdL0[0];
        GetInterNeighbour(pMb->sliceId, pMb->mbC, a+2, 10);
        if(!a[2].available) {
            GetInterNeighbour(pMb->sliceId, pMb->mbD, a+2, 15);
        }

        GetPredictionMv(&mvPred, a, refIndex);

        mv.hor += mvPred.hor;
        mv.ver += mvPred.ver;

        /* horizontal motion vector range [-2048, 2047.75] */
        if((uint32_t)(int32_t)(mv.hor+8192) >= (16384))
            return(HANTRO_NOK);

        /* vertical motion vector range [-512, 511.75]
         * (smaller for low levels) */
        if((uint32_t)(int32_t)(mv.ver+2048) >= (4096))
            return(HANTRO_NOK);
	    }

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if(!tmp)
        return(HANTRO_NOK);

    pMb->mv[0] = pMb->mv[1] = pMb->mv[2] = pMb->mv[3] =
    pMb->mv[4] = pMb->mv[5] = pMb->mv[6] = pMb->mv[7] =
    pMb->mv[8] = pMb->mv[9] = pMb->mv[10] = pMb->mv[11] =
    pMb->mv[12] = pMb->mv[13] = pMb->mv[14] = pMb->mv[15] = mv;

    pMb->refPic[0] = refIndex;
    pMb->refPic[1] = refIndex;
    pMb->refPic[2] = refIndex;
    pMb->refPic[3] = refIndex;
    pMb->refAddr[0] = tmp;
    pMb->refAddr[1] = tmp;
    pMb->refAddr[2] = tmp;
    pMb->refAddr[3] = tmp;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: MvPrediction16x8

        Functional description:
            Motion vector prediction for 16x8 partition mode

------------------------------------------------------------------------------*/
uint32_t MvPrediction16x8(mbStorage_t *pMb, mbPred_t *mbPred, dpbStorage_t *dpb) {
    mv_t mv;
    mv_t mvPred;
    interNeighbour_t a[3]; /* A, B, C */
    uint32_t refIndex;
    uint8_t *tmp;

    mv = mbPred->mvdL0[0];
    refIndex = mbPred->refIdxL0[0];

    GetInterNeighbour(pMb->sliceId, pMb->mbB, a+1, 10);

    if(a[1].refIndex == refIndex)
        mvPred = a[1].mv;
    else {
        GetInterNeighbour(pMb->sliceId, pMb->mbA, a, 5);
        GetInterNeighbour(pMb->sliceId, pMb->mbC, a+2, 10);
        if(!a[2].available)
            GetInterNeighbour(pMb->sliceId, pMb->mbD, a+2, 15);

        GetPredictionMv(&mvPred, a, refIndex);

    }
    mv.hor += mvPred.hor;
    mv.ver += mvPred.ver;

    /* horizontal motion vector range [-2048, 2047.75] */
    if((uint32_t)(int32_t)(mv.hor+8192) >= (16384))
        return(HANTRO_NOK);

    /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
    if((uint32_t)(int32_t)(mv.ver+2048) >= (4096))
        return(HANTRO_NOK);

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if(!tmp)
        return(HANTRO_NOK);

    pMb->mv[0] = pMb->mv[1] = pMb->mv[2] = pMb->mv[3] =
    pMb->mv[4] = pMb->mv[5] = pMb->mv[6] = pMb->mv[7] = mv;
    pMb->refPic[0] = refIndex;
    pMb->refPic[1] = refIndex;
    pMb->refAddr[0] = tmp;
    pMb->refAddr[1] = tmp;

    mv = mbPred->mvdL0[1];
    refIndex = mbPred->refIdxL0[1];

    GetInterNeighbour(pMb->sliceId, pMb->mbA, a, 13);
    if(a[0].refIndex == refIndex)
        mvPred = a[0].mv;
    else {
        a[1].available = HANTRO_TRUE;
        a[1].refIndex = pMb->refPic[0];
        a[1].mv = pMb->mv[0];

        /* c is not available */
        GetInterNeighbour(pMb->sliceId, pMb->mbA, a+2, 7);

        GetPredictionMv(&mvPred, a, refIndex);

	    }
    mv.hor += mvPred.hor;
    mv.ver += mvPred.ver;

    /* horizontal motion vector range [-2048, 2047.75] */
    if((uint32_t)(int32_t)(mv.hor+8192) >= (16384))
        return(HANTRO_NOK);

    /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
    if((uint32_t)(int32_t)(mv.ver+2048) >= (4096))
        return(HANTRO_NOK);

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if(tmp == NULL)
        return(HANTRO_NOK);

    pMb->mv[8] = pMb->mv[9] = pMb->mv[10] = pMb->mv[11] =
    pMb->mv[12] = pMb->mv[13] = pMb->mv[14] = pMb->mv[15] = mv;
    pMb->refPic[2] = refIndex;
    pMb->refPic[3] = refIndex;
    pMb->refAddr[2] = tmp;
    pMb->refAddr[3] = tmp;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: MvPrediction8x16

        Functional description:
            Motion vector prediction for 8x16 partition mode

------------------------------------------------------------------------------*/
uint32_t MvPrediction8x16(mbStorage_t *pMb, mbPred_t *mbPred, dpbStorage_t *dpb) {
    mv_t mv;
    mv_t mvPred;
    interNeighbour_t a[3]; /* A, B, C */
    uint32_t refIndex;
    uint8_t *tmp;


    mv = mbPred->mvdL0[0];
    refIndex = mbPred->refIdxL0[0];

    GetInterNeighbour(pMb->sliceId, pMb->mbA, a, 5);

    if(a[0].refIndex == refIndex)
        mvPred = a[0].mv;
    else {
        GetInterNeighbour(pMb->sliceId, pMb->mbB, a+1, 10);
        GetInterNeighbour(pMb->sliceId, pMb->mbB, a+2, 14);
        if(!a[2].available)
          GetInterNeighbour(pMb->sliceId, pMb->mbD, a+2, 15);

        GetPredictionMv(&mvPred, a, refIndex);

	    }
    mv.hor += mvPred.hor;
    mv.ver += mvPred.ver;

    /* horizontal motion vector range [-2048, 2047.75] */
    if((uint32_t)(int32_t)(mv.hor+8192) >= (16384))
        return(HANTRO_NOK);

    /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
    if((uint32_t)(int32_t)(mv.ver+2048) >= (4096))
        return(HANTRO_NOK);

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if(tmp == NULL)
        return(HANTRO_NOK);

    pMb->mv[0] = pMb->mv[1] = pMb->mv[2] = pMb->mv[3] =
    pMb->mv[8] = pMb->mv[9] = pMb->mv[10] = pMb->mv[11] = mv;
    pMb->refPic[0] = refIndex;
    pMb->refPic[2] = refIndex;
    pMb->refAddr[0] = tmp;
    pMb->refAddr[2] = tmp;

    mv = mbPred->mvdL0[1];
    refIndex = mbPred->refIdxL0[1];

    GetInterNeighbour(pMb->sliceId, pMb->mbC, a+2, 10);
    if(!a[2].available)
        GetInterNeighbour(pMb->sliceId, pMb->mbB, a+2, 11);
    if(a[2].refIndex == refIndex)
        mvPred = a[2].mv;
    else {
        a[0].available = HANTRO_TRUE;
        a[0].refIndex = pMb->refPic[0];
        a[0].mv = pMb->mv[0];

        GetInterNeighbour(pMb->sliceId, pMb->mbB, a+1, 14);

        GetPredictionMv(&mvPred, a, refIndex);

	    }
    mv.hor += mvPred.hor;
    mv.ver += mvPred.ver;

    /* horizontal motion vector range [-2048, 2047.75] */
    if((uint32_t)(int32_t)(mv.hor+8192) >= (16384))
        return(HANTRO_NOK);

    /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
    if((uint32_t)(int32_t)(mv.ver+2048) >= (4096))
        return(HANTRO_NOK);

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if(!tmp)
        return(HANTRO_NOK);

    pMb->mv[4] = pMb->mv[5] = pMb->mv[6] = pMb->mv[7] =
    pMb->mv[12] = pMb->mv[13] = pMb->mv[14] = pMb->mv[15] = mv;
    pMb->refPic[1] = refIndex;
    pMb->refPic[3] = refIndex;
    pMb->refAddr[1] = tmp;
    pMb->refAddr[3] = tmp;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: MvPrediction8x8

        Functional description:
            Motion vector prediction for 8x8 partition mode

------------------------------------------------------------------------------*/
uint32_t MvPrediction8x8(mbStorage_t *pMb, subMbPred_t *subMbPred, dpbStorage_t *dpb) {
    uint32_t i, j;
    uint32_t numSubMbPart;


    for(i=0; i < 4; i++) {
      numSubMbPart = h264bsdNumSubMbPart(subMbPred->subMbType[i]);
      pMb->refPic[i] = subMbPred->refIdxL0[i];
      pMb->refAddr[i] = h264bsdGetRefPicData(dpb, subMbPred->refIdxL0[i]);
      if(pMb->refAddr[i] == NULL)
          return(HANTRO_NOK);
      for(j=0; j < numSubMbPart; j++) {
          if(MvPrediction(pMb, subMbPred, i, j) != HANTRO_OK)
              return(HANTRO_NOK);
        }
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: MvPrediction

        Functional description:
            Perform motion vector prediction for sub-partition

------------------------------------------------------------------------------*/
uint32_t MvPrediction(mbStorage_t *pMb, subMbPred_t *subMbPred, uint32_t mbPartIdx, uint32_t subMbPartIdx) {
    mv_t mv, mvPred;
    uint32_t refIndex;
    subMbPartMode_e subMbPartMode;
    const neighbour_t *n;
    mbStorage_t *nMb;
    interNeighbour_t a[3]; /* A, B, C */

    mv = subMbPred->mvdL0[mbPartIdx][subMbPartIdx];
    subMbPartMode = h264bsdSubMbPartMode(subMbPred->subMbType[mbPartIdx]);
    refIndex = subMbPred->refIdxL0[mbPartIdx];

    n = N_A_SUB_PART[mbPartIdx][subMbPartMode]+subMbPartIdx;
    nMb = h264bsdGetNeighbourMb(pMb, n->mb);
    GetInterNeighbour(pMb->sliceId, nMb, a, n->index);

    n = N_B_SUB_PART[mbPartIdx][subMbPartMode]+subMbPartIdx;
    nMb = h264bsdGetNeighbourMb(pMb, n->mb);
    GetInterNeighbour(pMb->sliceId, nMb, a+1, n->index);

    n = N_C_SUB_PART[mbPartIdx][subMbPartMode]+subMbPartIdx;
    nMb = h264bsdGetNeighbourMb(pMb, n->mb);
    GetInterNeighbour(pMb->sliceId, nMb, a+2, n->index);

  if(!a[2].available) {
    n = N_D_SUB_PART[mbPartIdx][subMbPartMode]+subMbPartIdx;
    nMb = h264bsdGetNeighbourMb(pMb, n->mb);
    GetInterNeighbour(pMb->sliceId, nMb, a+2, n->index);
    }

  GetPredictionMv(&mvPred, a, refIndex);

  mv.hor += mvPred.hor;
  mv.ver += mvPred.ver;

  /* horizontal motion vector range [-2048, 2047.75] */
  if(((uint32_t)(int32_t)(mv.hor+8192) >= (16384)))
      return(HANTRO_NOK);

  /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
  if(((uint32_t)(int32_t)(mv.ver+2048) >= (4096)))
      return(HANTRO_NOK);

  switch(subMbPartMode) {
    case MB_SP_8x8:
      pMb->mv[4*mbPartIdx] = mv;
      pMb->mv[4*mbPartIdx + 1] = mv;
      pMb->mv[4*mbPartIdx + 2] = mv;
      pMb->mv[4*mbPartIdx + 3] = mv;
      break;
    case MB_SP_8x4:
      pMb->mv[4*mbPartIdx + 2*subMbPartIdx] = mv;
      pMb->mv[4*mbPartIdx + 2*subMbPartIdx + 1] = mv;
      break;
    case MB_SP_4x8:
      pMb->mv[4*mbPartIdx + subMbPartIdx] = mv;
      pMb->mv[4*mbPartIdx + subMbPartIdx + 2] = mv;
      break;
    case MB_SP_4x4:
      pMb->mv[4*mbPartIdx + subMbPartIdx] = mv;
      break;
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: MedianFilter

        Functional description:
            Median filtering for motion vector prediction

------------------------------------------------------------------------------*/
int32_t MedianFilter(int32_t a, int32_t b, int32_t c) {
  int32_t max,min,med;

  max = min = med = a;
  if(b > max)
      max = b;
  else if(b < min)
      min = b;
  if(c > max)
      med = max;
  else if(c < min)
      med = min;
  else 
      med = c;

  return med;
	}

/*------------------------------------------------------------------------------

    Function: GetInterNeighbour

        Functional description:
            Get availability, reference index and motion vector of a neighbour

------------------------------------------------------------------------------*/
void GetInterNeighbour(uint32_t sliceId, mbStorage_t *nMb, interNeighbour_t *n, uint32_t index) {

    n->available = HANTRO_FALSE;
    n->refIndex=0xFFFFFFFF;
    n->mv.hor = n->mv.ver=0;

    if(nMb && (sliceId == nMb->sliceId)) {
        uint32_t tmp;
        mv_t tmpMv;

        tmp = nMb->mbType;
        n->available = HANTRO_TRUE;
        /* MbPartPredMode "inlined" */
        if(tmp <= P_8x8ref0) {
            tmpMv = nMb->mv[index];
            tmp = nMb->refPic[index>>2];
            n->refIndex = tmp;
            n->mv = tmpMv;
        }
    }

	}

/*------------------------------------------------------------------------------

    Function: GetPredictionMv

        Functional description:
            Compute motion vector predictor based on neighbours A, B and C

------------------------------------------------------------------------------*/
void GetPredictionMv(mv_t *mv, interNeighbour_t *a, uint32_t refIndex) {

  if( a[1].available || a[2].available || !a[0].available) {
      uint32_t isA, isB, isC;
      isA = (a[0].refIndex == refIndex) ? HANTRO_TRUE : HANTRO_FALSE;
      isB = (a[1].refIndex == refIndex) ? HANTRO_TRUE : HANTRO_FALSE;
      isC = (a[2].refIndex == refIndex) ? HANTRO_TRUE : HANTRO_FALSE;

      if(((uint32_t)isA+(uint32_t)isB+(uint32_t)isC) != 1) {
        mv->hor = (int16_t)MedianFilter(a[0].mv.hor, a[1].mv.hor, a[2].mv.hor);
        mv->ver = (int16_t)MedianFilter(a[0].mv.ver, a[1].mv.ver, a[2].mv.ver);
      }
    else if(isA)
        *mv = a[0].mv;
    else if(isB)
        *mv = a[1].mv;
    else
        *mv = a[2].mv;
    }
  else   
      *mv = a[0].mv;

	}




/* x- and y-coordinates for each block */
const uint32_t h264bsdBlockX[16] =
{ 0, 4, 0, 4, 8, 12, 8, 12, 0, 4, 0, 4, 8, 12, 8, 12 };
const uint32_t h264bsdBlockY[16] =
{ 0, 0, 4, 4, 0, 0, 4, 4, 8, 8, 12, 12, 8, 8, 12, 12 };

const uint8_t h264bsdClip[1280] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
    48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
    64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
    80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
    96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
    112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
    208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
    240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
	};

uint8_t *get_h264bsdClip() {
  return (uint8_t*)h264bsdClip;
	}

#ifndef H264DEC_OMXDL
static void Get4x4NeighbourPels(uint8_t *a, uint8_t *l, uint8_t *data, uint8_t *above, uint8_t *left,
    uint32_t blockNum);
static void Intra16x16VerticalPrediction(uint8_t *data, uint8_t *above);
static void Intra16x16HorizontalPrediction(uint8_t *data, uint8_t *left);
static void Intra16x16DcPrediction(uint8_t *data, uint8_t *above, uint8_t *left,
    uint32_t A, uint32_t B);
static void Intra16x16PlanePrediction(uint8_t *data, uint8_t *above, uint8_t *left);
static void IntraChromaDcPrediction(uint8_t *data, uint8_t *above, uint8_t *left,
    uint32_t A, uint32_t B);
static void IntraChromaHorizontalPrediction(uint8_t *data, uint8_t *left);
static void IntraChromaVerticalPrediction(uint8_t *data, uint8_t *above);
static void IntraChromaPlanePrediction(uint8_t *data, uint8_t *above, uint8_t *left);
static void Intra4x4VerticalPrediction(uint8_t *data, uint8_t *above);
static void Intra4x4HorizontalPrediction(uint8_t *data, uint8_t *left);
static void Intra4x4DcPrediction(uint8_t *data, uint8_t *above, uint8_t *left, uint32_t A, uint32_t B);
static void Intra4x4DiagonalDownLeftPrediction(uint8_t *data, uint8_t *above);
static void Intra4x4DiagonalDownRightPrediction(uint8_t *data, uint8_t *above, uint8_t *left);
static void Intra4x4VerticalRightPrediction(uint8_t *data, uint8_t *above, uint8_t *left);
static void Intra4x4HorizontalDownPrediction(uint8_t *data, uint8_t *above, uint8_t *left);
static void Intra4x4VerticalLeftPrediction(uint8_t *data, uint8_t *above);
static void Intra4x4HorizontalUpPrediction(uint8_t *data, uint8_t *left);
void h264bsdAddResidual(uint8_t *data, int32_t *residual, uint32_t blockNum);
static void Write4x4To16x16(uint8_t *data, uint8_t *data4x4, uint32_t blockNum);
#endif /* H264DEC_OMXDL */

static uint32_t DetermineIntra4x4PredMode(macroblockLayer_t *pMbLayer,
    uint32_t available, neighbour_t *nA, neighbour_t *nB, uint32_t index,
    mbStorage_t *nMbA, mbStorage_t *nMbB);


#ifdef H264DEC_OMXDL

/*------------------------------------------------------------------------------

    Function: h264bsdIntra16x16Prediction

        Functional description:
          Perform intra 16x16 prediction mode for luma pixels and add
          residual into prediction. The resulting luma pixels are
          stored in macroblock array 'data'.

------------------------------------------------------------------------------*/
uint32_t h264bsdIntra16x16Prediction(mbStorage_t *pMb, uint8_t *data, uint8_t *ptr,
                                uint32_t width, uint32_t constrainedIntraPred) {
    uint32_t availableA, availableB, availableD;
    OMXResult omxRes;

    ASSERT(pMb);
    ASSERT(data);
    ASSERT(ptr);
    ASSERT(h264bsdPredModeIntra16x16(pMb->mbType) < 4);

    availableA = h264bsdIsNeighbourAvailable(pMb, pMb->mbA);
    if(availableA && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbA->mbType) == PRED_MODE_INTER))
        availableA = HANTRO_FALSE;
    availableB = h264bsdIsNeighbourAvailable(pMb, pMb->mbB);
    if(availableB && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbB->mbType) == PRED_MODE_INTER))
        availableB = HANTRO_FALSE;
    availableD = h264bsdIsNeighbourAvailable(pMb, pMb->mbD);
    if(availableD && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbD->mbType) == PRED_MODE_INTER))
        availableD = HANTRO_FALSE;

    omxRes = omxVCM4P10_PredictIntra_16x16((ptr-1),
                                    (ptr - width),
                                    (ptr - width-1),
                                    data,
                                    (int32_t)width,
                                    16,
                                    (OMXVCM4P10Intra16x16PredMode)
                                    h264bsdPredModeIntra16x16(pMb->mbType),
                                    (int32_t)(availableB + (availableA<<1) +
                                     (availableD<<5)));
    if(omxRes != OMX_Sts_NoErr)
        return HANTRO_NOK;
    else
        return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdIntra4x4Prediction

        Functional description:
          Perform intra 4x4 prediction for luma pixels and add residual
          into prediction. The resulting luma pixels are stored in
          macroblock array 'data'. The intra 4x4 prediction mode for each
          block is stored in 'pMb' structure.

------------------------------------------------------------------------------*/
uint32_t h264bsdIntra4x4Prediction(mbStorage_t *pMb, uint8_t *data,
                              macroblockLayer_t *mbLayer,
                              uint8_t *ptr, uint32_t width,
                              uint32_t constrainedIntraPred, uint32_t block) {
    uint32_t mode;
    neighbour_t neighbour, neighbourB;
    mbStorage_t *nMb, *nMb2;
    uint32_t availableA, availableB, availableC, availableD;
    OMXResult omxRes;
    uint32_t x, y;
    uint8_t *l, *a, *al;

    ASSERT(pMb);
    ASSERT(data);
    ASSERT(mbLayer);
    ASSERT(ptr);
    ASSERT(pMb->intra4x4PredMode[block] < 9);

    neighbour = *h264bsdNeighbour4x4BlockA(block);
    nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
    availableA = h264bsdIsNeighbourAvailable(pMb, nMb);
    if(availableA && constrainedIntraPred &&
       ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER))
        availableA = HANTRO_FALSE;

    neighbourB = *h264bsdNeighbour4x4BlockB(block);
    nMb2 = h264bsdGetNeighbourMb(pMb, neighbourB.mb);
    availableB = h264bsdIsNeighbourAvailable(pMb, nMb2);
    if(availableB && constrainedIntraPred &&
       ( h264bsdMbPartPredMode(nMb2->mbType) == PRED_MODE_INTER))
        availableB = HANTRO_FALSE;

    mode = DetermineIntra4x4PredMode(mbLayer,
        (uint32_t)(availableA && availableB),
        &neighbour, &neighbourB, block, nMb, nMb2);
    pMb->intra4x4PredMode[block] = (uint8_t)mode;

    neighbour = *h264bsdNeighbour4x4BlockC(block);
    nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
    availableC = h264bsdIsNeighbourAvailable(pMb, nMb);
    if(availableC && constrainedIntraPred &&
       ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER))
        availableC = HANTRO_FALSE;

    neighbour = *h264bsdNeighbour4x4BlockD(block);
    nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
    availableD = h264bsdIsNeighbourAvailable(pMb, nMb);
    if(availableD && constrainedIntraPred &&
       ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER))
        availableD = HANTRO_FALSE;

    x = h264bsdBlockX[block];
    y = h264bsdBlockY[block];

    if(y == 0)
        a = ptr - width + x;
    else
        a = data-16;

    if(x == 0)
        l = ptr + y * width -1;
    else {
        l = data-1;
        width = 16;
	    }

    if(x == 0)
        al = l-width;
    else
        al = a-1;

    omxRes = omxVCM4P10_PredictIntra_4x4( l, a, al,
                                          data, (int32_t)width, 16,
                                          (OMXVCM4P10Intra4x4PredMode)mode,
                                          (int32_t)(availableB + (availableA<<1) + (availableD<<5) + (availableC<<6)));
    if(omxRes != OMX_Sts_NoErr)
        return HANTRO_NOK;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdIntraChromaPrediction

        Functional description:
          Perform intra prediction for chroma pixels and add residual
          into prediction. The resulting chroma pixels are stored in 'data'.

------------------------------------------------------------------------------*/
uint32_t h264bsdIntraChromaPrediction(mbStorage_t *pMb, uint8_t *data, image_t *image,
                                        uint32_t predMode, uint32_t constrainedIntraPred) {
    uint32_t availableA, availableB, availableD;
    OMXResult omxRes;
    uint8_t *ptr;
    uint32_t width;

    ASSERT(pMb);
    ASSERT(data);
    ASSERT(image);
    ASSERT(predMode < 4);

    availableA = h264bsdIsNeighbourAvailable(pMb, pMb->mbA);
    if(availableA && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbA->mbType) == PRED_MODE_INTER))
        availableA = HANTRO_FALSE;
    availableB = h264bsdIsNeighbourAvailable(pMb, pMb->mbB);
    if(availableB && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbB->mbType) == PRED_MODE_INTER))
        availableB = HANTRO_FALSE;
    availableD = h264bsdIsNeighbourAvailable(pMb, pMb->mbD);
    if(availableD && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbD->mbType) == PRED_MODE_INTER))
        availableD = HANTRO_FALSE;

    ptr = image->cb;
    width = image->width*8;

    omxRes = omxVCM4P10_PredictIntraChroma_8x8((ptr-1), (ptr - width), (ptr - width -1),
                                                data, (int32_t)width, 8,
                                                (OMXVCM4P10IntraChromaPredMode)
                                                predMode,
                                                (int32_t)(availableB + (availableA<<1) + (availableD<<5)));
    if(omxRes != OMX_Sts_NoErr)
        return HANTRO_NOK;

    /* advance pointers */
    data += 64;
    ptr = image->cr;

    omxRes = omxVCM4P10_PredictIntraChroma_8x8((ptr-1), (ptr - width), (ptr - width -1),
                                                data, (int32_t)width, 8,
                                                (OMXVCM4P10IntraChromaPredMode)
                                                predMode,
                                                (int32_t)(availableB + (availableA<<1) + (availableD<<5)));
    if(omxRes != OMX_Sts_NoErr)
        return HANTRO_NOK;

    return(HANTRO_OK);
	}

#else /* H264DEC_OMXDL */


/*------------------------------------------------------------------------------

    Function: h264bsdIntraPrediction

        Functional description:
          Processes one intra macroblock. Performs intra prediction using
          specified prediction mode. Writes the final macroblock
          (prediction + residual) into the output image (image)

        Inputs:
          pMb           pointer to macroblock specific information
          mbLayer       pointer to current macroblock data from stream
          image         pointer to output image
          mbNum         current macroblock number
          constrainedIntraPred  flag specifying if neighbouring inter
                                macroblocks are used in intra prediction
          data          pointer where output macroblock will be stored

        Outputs:
          pMb           structure is updated with current macroblock
          image         current macroblock is written into image
          data          current macroblock is stored here

        Returns:
          HANTRO_OK     success
          HANTRO_NOK    error in intra prediction

------------------------------------------------------------------------------*/
uint32_t h264bsdIntraPrediction(mbStorage_t *pMb, macroblockLayer_t *mbLayer,
    image_t *image, uint32_t mbNum, uint32_t constrainedIntraPred, uint8_t *data) {
    /* pelAbove and pelLeft contain samples above and left to the current
     * macroblock. Above array contains also sample above-left to the current
     * mb as well as 4 samples above-right to the current mb (latter only for
     * luma) */
    /* lumD + lumB + lumC + cbD + cbB + crD + crB */
    uint8_t pelAbove[1 + 16 + 4 + 1 + 8 + 1 + 8];
    /* lumA + cbA + crA */
    uint8_t pelLeft[16 + 8 + 8];
    uint32_t tmp;


    ASSERT(pMb);
    ASSERT(image);
    ASSERT(mbNum < image->width * image->height);
    ASSERT(h264bsdMbPartPredMode(pMb->mbType) != PRED_MODE_INTER);

    h264bsdGetNeighbourPels(image, pelAbove, pelLeft, mbNum);

    if(h264bsdMbPartPredMode(pMb->mbType) == PRED_MODE_INTRA16x16) {
        tmp = h264bsdIntra16x16Prediction(pMb, data, mbLayer->residual.level,
            pelAbove, pelLeft, constrainedIntraPred);
        if(tmp != HANTRO_OK)
            return tmp;
		  }
    else {
        tmp = h264bsdIntra4x4Prediction(pMb, data, mbLayer,
            pelAbove, pelLeft, constrainedIntraPred);
        if(tmp != HANTRO_OK)
            return tmp;
	    }

    tmp = h264bsdIntraChromaPrediction(pMb, data + 256,
            mbLayer->residual.level+16, pelAbove + 21, pelLeft + 16,
            mbLayer->mbPred.intraChromaPredMode, constrainedIntraPred);
    if(tmp != HANTRO_OK)
        return tmp;

    /* if decoded flag > 1 -> mb has already been successfully decoded and
     * written to output -> do not write again */
    if(pMb->decoded > 1)
        return HANTRO_OK;

    h264bsdWriteMacroblock(image, data);

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdGetNeighbourPels

        Functional description:
          Get pixel values from neighbouring macroblocks into 'above'
          and 'left' arrays.

------------------------------------------------------------------------------*/
void h264bsdGetNeighbourPels(image_t *image, uint8_t *above, uint8_t *left, uint32_t mbNum) {
    uint32_t i;
    uint32_t width, picSize;
    uint8_t *ptr, *tmp;
    uint32_t row, col;


    ASSERT(image);
    ASSERT(above);
    ASSERT(left);
    ASSERT(mbNum < image->width * image->height);

    if(!mbNum)
        return;

    width = image->width;
    picSize = width * image->height;
    row = mbNum / width;
    col = mbNum - row * width;

    width *= 16;
    ptr = image->data + row * MACROBLOCK_SIZE * width  + col * MACROBLOCK_SIZE;

    /* note that luma samples above-right to current macroblock do not make
     * sense when current mb is the right-most mb in a row. Same applies to
     * sample above-left if col is zero. However, usage of pels in prediction
     * is controlled by neighbour availability information in actual prediction
     * process */
    if(row) {
        tmp = ptr - (width + 1);
        for(i = 21; i--;)
            *above++ = *tmp++;
    }

    if(col) {
        ptr--;
        for(i = 16; i--; ptr+=width)
            *left++ = *ptr;
	    }

    width >>= 1;
    ptr = image->data + picSize * 256 + row * 8 * width  + col * 8;

    if(row) {
        tmp = ptr - (width + 1);
        for(i = 9; i--;)
            *above++ = *tmp++;
        tmp += (picSize * 64) - 9;
        for(i = 9; i--;)
            *above++ = *tmp++;
		  }

    if(col) {
        ptr--;
        for(i = 8; i--; ptr+=width)
            *left++ = *ptr;
        ptr += (picSize * 64) - 8 * width;
        for(i = 8; i--; ptr+=width)
            *left++ = *ptr;
    }
	}

/*------------------------------------------------------------------------------

    Function: Intra16x16Prediction

        Functional description:
          Perform intra 16x16 prediction mode for luma pixels and add
          residual into prediction. The resulting luma pixels are
          stored in macroblock array 'data'.

------------------------------------------------------------------------------*/
uint32_t h264bsdIntra16x16Prediction(mbStorage_t *pMb, uint8_t *data, int32_t residual[][16],
                                uint8_t *above, uint8_t *left, uint32_t constrainedIntraPred) {
    uint32_t i;
    uint32_t availableA, availableB, availableD;


    ASSERT(data);
    ASSERT(residual);
    ASSERT(above);
    ASSERT(left);
    ASSERT(h264bsdPredModeIntra16x16(pMb->mbType) < 4);

    availableA = h264bsdIsNeighbourAvailable(pMb, pMb->mbA);
    if(availableA && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbA->mbType) == PRED_MODE_INTER))
        availableA = HANTRO_FALSE;
    availableB = h264bsdIsNeighbourAvailable(pMb, pMb->mbB);
    if(availableB && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbB->mbType) == PRED_MODE_INTER))
        availableB = HANTRO_FALSE;
    availableD = h264bsdIsNeighbourAvailable(pMb, pMb->mbD);
    if(availableD && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbD->mbType) == PRED_MODE_INTER))
        availableD = HANTRO_FALSE;

    switch(h264bsdPredModeIntra16x16(pMb->mbType)) {
        case 0: /* Intra_16x16_Vertical */
          if(!availableB)
              return(HANTRO_NOK);
          Intra16x16VerticalPrediction(data, above+1);
          break;

        case 1: /* Intra_16x16_Horizontal */
          if(!availableA)
              return(HANTRO_NOK);
          Intra16x16HorizontalPrediction(data, left);
          break;

        case 2: /* Intra_16x16_DC */
          Intra16x16DcPrediction(data, above+1, left, availableA, availableB);
          break;

        default: /* case 3: Intra_16x16_Plane */
          if(!availableA || !availableB || !availableD)
              return(HANTRO_NOK);
          Intra16x16PlanePrediction(data, above+1, left);
          break;
	    }
    /* add residual */
    for(i=0; i < 16; i++)
        h264bsdAddResidual(data, residual[i], i);

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: Intra4x4Prediction

        Functional description:
          Perform intra 4x4 prediction for luma pixels and add residual
          into prediction. The resulting luma pixels are stored in
          macroblock array 'data'. The intra 4x4 prediction mode for each
          block is stored in 'pMb' structure.

------------------------------------------------------------------------------*/
uint32_t h264bsdIntra4x4Prediction(mbStorage_t *pMb, uint8_t *data,
                              macroblockLayer_t *mbLayer, uint8_t *above,
                              uint8_t *left, uint32_t constrainedIntraPred) {
    uint32_t block;
    uint32_t mode;
    neighbour_t neighbour, neighbourB;
    mbStorage_t *nMb, *nMb2;
    uint8_t a[1 + 4 + 4], l[1 + 4];
    uint32_t data4x4[4];
    uint32_t availableA, availableB, availableC, availableD;


    ASSERT(data);
    ASSERT(mbLayer);
    ASSERT(above);
    ASSERT(left);

    for(block=0; block < 16; block++) {

        ASSERT(pMb->intra4x4PredMode[block] < 9);

        neighbour = *h264bsdNeighbour4x4BlockA(block);
        nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
        availableA = h264bsdIsNeighbourAvailable(pMb, nMb);
        if(availableA && constrainedIntraPred &&
           ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER))
            availableA = HANTRO_FALSE;

        neighbourB = *h264bsdNeighbour4x4BlockB(block);
        nMb2 = h264bsdGetNeighbourMb(pMb, neighbourB.mb);
        availableB = h264bsdIsNeighbourAvailable(pMb, nMb2);
        if(availableB && constrainedIntraPred &&
           ( h264bsdMbPartPredMode(nMb2->mbType) == PRED_MODE_INTER))
            availableB = HANTRO_FALSE;

        mode = DetermineIntra4x4PredMode(mbLayer,
            (uint32_t)(availableA && availableB),
            &neighbour, &neighbourB, block, nMb, nMb2);
        pMb->intra4x4PredMode[block] = (uint8_t)mode;

        neighbour = *h264bsdNeighbour4x4BlockC(block);
        nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
        availableC = h264bsdIsNeighbourAvailable(pMb, nMb);
        if(availableC && constrainedIntraPred &&
           ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER))
            availableC = HANTRO_FALSE;

        neighbour = *h264bsdNeighbour4x4BlockD(block);
        nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
        availableD = h264bsdIsNeighbourAvailable(pMb, nMb);
        if(availableD && constrainedIntraPred &&
           ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER))
            availableD = HANTRO_FALSE;

        Get4x4NeighbourPels(a, l, data, above, left, block);

        switch(mode) {
            case 0: /* Intra_4x4_Vertical */
                if(!availableB)
                    return(HANTRO_NOK);
                Intra4x4VerticalPrediction((uint8_t*)data4x4, a + 1);
                break;
            case 1: /* Intra_4x4_Horizontal */
                if(!availableA)
                    return(HANTRO_NOK);
                Intra4x4HorizontalPrediction((uint8_t*)data4x4, l + 1);
                break;
            case 2: /* Intra_4x4_DC */
                Intra4x4DcPrediction((uint8_t*)data4x4, a + 1, l + 1,
                    availableA, availableB);
                break;
            case 3: /* Intra_4x4_Diagonal_Down_Left */
                if(!availableB)
                    return(HANTRO_NOK);
                if(!availableC) {
                    a[5] = a[6] = a[7] = a[8] = a[4];
                }
                Intra4x4DiagonalDownLeftPrediction((uint8_t*)data4x4, a + 1);
                break;
            case 4: /* Intra_4x4_Diagonal_Down_Right */
                if(!availableA || !availableB || !availableD)
                    return(HANTRO_NOK);
                Intra4x4DiagonalDownRightPrediction((uint8_t*)data4x4, a + 1, l + 1);
                break;
            case 5: /* Intra_4x4_Vertical_Right */
                if(!availableA || !availableB || !availableD)
                    return(HANTRO_NOK);
                Intra4x4VerticalRightPrediction((uint8_t*)data4x4, a + 1, l + 1);
                break;
            case 6: /* Intra_4x4_Horizontal_Down */
                if(!availableA || !availableB || !availableD)
                    return(HANTRO_NOK);
                Intra4x4HorizontalDownPrediction((uint8_t*)data4x4, a + 1, l + 1);
                break;
            case 7: /* Intra_4x4_Vertical_Left */
                if(!availableB)
                    return(HANTRO_NOK);
                if(!availableC)
                    a[5] = a[6] = a[7] = a[8] = a[4];
                Intra4x4VerticalLeftPrediction((uint8_t*)data4x4, a + 1);
                break;
            default: /* case 8 Intra_4x4_Horizontal_Up */
                if(!availableA)
                    return(HANTRO_NOK);
                Intra4x4HorizontalUpPrediction((uint8_t*)data4x4, l + 1);
                break;
        }

    Write4x4To16x16(data, (uint8_t*)data4x4, block);
    h264bsdAddResidual(data, mbLayer->residual.level[block], block);
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: IntraChromaPrediction

        Functional description:
          Perform intra prediction for chroma pixels and add residual
          into prediction. The resulting chroma pixels are stored in 'data'.

------------------------------------------------------------------------------*/
uint32_t h264bsdIntraChromaPrediction(mbStorage_t *pMb, uint8_t *data, int32_t residual[][16],
                    uint8_t *above, uint8_t *left, uint32_t predMode, uint32_t constrainedIntraPred) {
    uint32_t i, comp, block;
    uint32_t availableA, availableB, availableD;


    ASSERT(data);
    ASSERT(residual);
    ASSERT(above);
    ASSERT(left);
    ASSERT(predMode < 4);

    availableA = h264bsdIsNeighbourAvailable(pMb, pMb->mbA);
    if(availableA && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbA->mbType) == PRED_MODE_INTER))
        availableA = HANTRO_FALSE;
    availableB = h264bsdIsNeighbourAvailable(pMb, pMb->mbB);
    if(availableB && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbB->mbType) == PRED_MODE_INTER))
        availableB = HANTRO_FALSE;
    availableD = h264bsdIsNeighbourAvailable(pMb, pMb->mbD);
    if(availableD && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbD->mbType) == PRED_MODE_INTER))
        availableD = HANTRO_FALSE;

    for(comp=0, block = 16; comp < 2; comp++) {
        switch(predMode) {
          case 0: /* Intra_Chroma_DC */
            IntraChromaDcPrediction(data, above+1, left, availableA,
                availableB);
            break;
          case 1: /* Intra_Chroma_Horizontal */
            if(!availableA)
                return(HANTRO_NOK);
            IntraChromaHorizontalPrediction(data, left);
            break;
          case 2: /* Intra_Chroma_Vertical */
            if(!availableB)
                return(HANTRO_NOK);
            IntraChromaVerticalPrediction(data, above+1);
            break;
          default: /* case 3: Intra_Chroma_Plane */
            if(!availableA || !availableB || !availableD)
                return(HANTRO_NOK);
            IntraChromaPlanePrediction(data, above+1, left);
            break;
	        }
        for(i=0; i < 4; i++, block++)
            h264bsdAddResidual(data, residual[i], block);

        /* advance pointers */
        data += 64;
        above += 9;
        left += 8;
        residual += 4;
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdAddResidual

        Functional description:
          Add residual of a block into prediction in macroblock array 'data'.
          The result (residual + prediction) is stored in 'data'.

------------------------------------------------------------------------------*/
#ifndef H264DEC_OMXDL
void h264bsdAddResidual(uint8_t *data, int32_t *residual, uint32_t blockNum) {
    uint32_t i;
    uint32_t x, y;
    uint32_t width;
    int32_t tmp1, tmp2, tmp3, tmp4;
    uint8_t *tmp;
    const uint8_t *clp = h264bsdClip + 512;


    ASSERT(data);
    ASSERT(residual);
    ASSERT(blockNum < 16 + 4 + 4);

    if(IS_RESIDUAL_EMPTY(residual))
        return;

    RANGE_CHECK_ARRAY(residual, -512, 511, 16);

    if(blockNum < 16) {
        width = 16;
        x = h264bsdBlockX[blockNum];
        y = h264bsdBlockY[blockNum];
	    }
    else {
        width = 8;
        x = h264bsdBlockX[blockNum & 0x3];
        y = h264bsdBlockY[blockNum & 0x3];
		  }

    tmp = data + y*width + x;
    for(i = 4; i; i--) {
      tmp1 = *residual++;
      tmp2 = tmp[0];
      tmp3 = *residual++;
      tmp4 = tmp[1];

      tmp[0] = clp[tmp1 + tmp2];

      tmp1 = *residual++;
      tmp2 = tmp[2];

      tmp[1] = clp[tmp3 + tmp4];

      tmp3 = *residual++;
      tmp4 = tmp[3];

      tmp1 = clp[tmp1 + tmp2];
      tmp3 = clp[tmp3 + tmp4];
      tmp[2] = (uint8_t)tmp1;
      tmp[3] = (uint8_t)tmp3;

      tmp += width;
			}

	}
#endif

/*------------------------------------------------------------------------------

    Function: Intra16x16VerticalPrediction

        Functional description:
          Perform intra 16x16 vertical prediction mode.

------------------------------------------------------------------------------*/
void Intra16x16VerticalPrediction(uint8_t *data, uint8_t *above) {
  uint32_t i, j;


  ASSERT(data);
  ASSERT(above);

  for(i=0; i < 16; i++) {
      for(j=0; j < 16; j++)
          *data++ = above[j];
    }

	}

/*------------------------------------------------------------------------------

    Function: Intra16x16HorizontalPrediction

        Functional description:
          Perform intra 16x16 horizontal prediction mode.

------------------------------------------------------------------------------*/
void Intra16x16HorizontalPrediction(uint8_t *data, uint8_t *left) {
    uint32_t i, j;


    ASSERT(data);
    ASSERT(left);

    for(i=0; i < 16; i++) {
        for(j=0; j < 16; j++)
            *data++ = left[i];
    }

	}

/*------------------------------------------------------------------------------

    Function: Intra16x16DcPrediction

        Functional description:
          Perform intra 16x16 DC prediction mode.

------------------------------------------------------------------------------*/
void Intra16x16DcPrediction(uint8_t *data, uint8_t *above, uint8_t *left, uint32_t availableA,
    uint32_t availableB) {
  uint32_t i, tmp;


  ASSERT(data);
  ASSERT(above);
  ASSERT(left);

  if(availableA && availableB) {
      for(i=0, tmp=0; i < 16; i++)
          tmp += above[i] + left[i];
      tmp = (tmp + 16) >> 5;
		}
  else if(availableA) {
      for(i=0, tmp=0; i < 16; i++)
          tmp += left[i];
      tmp = (tmp + 8) >> 4;
		}
  else if(availableB) {
      for(i=0, tmp=0; i < 16; i++)
          tmp += above[i];
      tmp = (tmp + 8) >> 4;
		}
  /* neither A nor B available */
  else {
      tmp = 128;
	  }
  for(i=0; i < 256; i++)
      data[i] = (uint8_t)tmp;

	}

/*------------------------------------------------------------------------------

    Function: Intra16x16PlanePrediction

        Functional description:
          Perform intra 16x16 plane prediction mode.

------------------------------------------------------------------------------*/
void Intra16x16PlanePrediction(uint8_t *data, uint8_t *above, uint8_t *left) {
    int32_t i, j;
    int32_t a, b, c;
    int32_t tmp;


    ASSERT(data);
    ASSERT(above);
    ASSERT(left);

    a = 16 * (above[15] + left[15]);

    for(i=0, b=0; i < 8; i++)
        b += (i + 1) * (above[8+i] - above[6-i]);
    b = (5 * b + 32) >> 6;

    for(i=0, c=0; i < 7; i++)
        c += (i + 1) * (left[8+i] - left[6-i]);
    /* p[-1,-1] has to be accessed through above pointer */
    c += (i + 1) * (left[8+i] - above[-1]);
    c = (5 * c + 32) >> 6;

    for(i=0; i < 16; i++) {
        for(j=0; j < 16; j++) {
            tmp = (a + b * (j - 7) + c * (i - 7) + 16) >> 5;
            data[i*16+j] = (uint8_t)CLIP1(tmp);
        }
    }

	}

/*------------------------------------------------------------------------------

    Function: IntraChromaDcPrediction

        Functional description:
          Perform intra chroma DC prediction mode.

------------------------------------------------------------------------------*/
void IntraChromaDcPrediction(uint8_t *data, uint8_t *above, uint8_t *left, uint32_t availableA, uint32_t availableB) {
    uint32_t i;
    uint32_t tmp1, tmp2;


    ASSERT(data);
    ASSERT(above);
    ASSERT(left);

    /* y=0..3 */
    if(availableA && availableB) {
        tmp1 = above[0] + above[1] + above[2] + above[3] +
              left[0] + left[1] + left[2] + left[3];
        tmp1 = (tmp1 + 4) >> 3;
        tmp2 = (above[4] + above[5] + above[6] + above[7] + 2) >> 2;
			}
    else if(availableB) {
        tmp1 = (above[0] + above[1] + above[2] + above[3] + 2) >> 2;
        tmp2 = (above[4] + above[5] + above[6] + above[7] + 2) >> 2;
			}
    else if(availableA) {
        tmp1 = (left[0] + left[1] + left[2] + left[3] + 2) >> 2;
        tmp2 = tmp1;
		  }
    /* neither A nor B available */
    else {
        tmp1 = tmp2 = 128;
			}

    ASSERT(tmp1 < 256 && tmp2 < 256);
    for(i = 4; i--;) {
        *data++ = (uint8_t)tmp1;
        *data++ = (uint8_t)tmp1;
        *data++ = (uint8_t)tmp1;
        *data++ = (uint8_t)tmp1;
        *data++ = (uint8_t)tmp2;
        *data++ = (uint8_t)tmp2;
        *data++ = (uint8_t)tmp2;
        *data++ = (uint8_t)tmp2;
    }

    /* y = 4...7 */
    if(availableA) {
        tmp1 = (left[4] + left[5] + left[6] + left[7] + 2) >> 2;
        if(availableB) {
            tmp2 = above[4] + above[5] + above[6] + above[7] +
                   left[4] + left[5] + left[6] + left[7];
            tmp2 = (tmp2 + 4) >> 3;
				  }
        else
            tmp2 = tmp1;
			}
    else if(availableB) {
        tmp1 = (above[0] + above[1] + above[2] + above[3] + 2) >> 2;
        tmp2 = (above[4] + above[5] + above[6] + above[7] + 2) >> 2;
			}
    else {
        tmp1 = tmp2 = 128;
			}

    ASSERT(tmp1 < 256 && tmp2 < 256);
    for(i = 4; i--;) {
        *data++ = (uint8_t)tmp1;
        *data++ = (uint8_t)tmp1;
        *data++ = (uint8_t)tmp1;
        *data++ = (uint8_t)tmp1;
        *data++ = (uint8_t)tmp2;
        *data++ = (uint8_t)tmp2;
        *data++ = (uint8_t)tmp2;
        *data++ = (uint8_t)tmp2;
    }
	}

/*------------------------------------------------------------------------------

    Function: IntraChromaHorizontalPrediction

        Functional description:
          Perform intra chroma horizontal prediction mode.

------------------------------------------------------------------------------*/
void IntraChromaHorizontalPrediction(uint8_t *data, uint8_t *left) {
    uint32_t i;

    ASSERT(data);
    ASSERT(left);

    for(i = 8; i--;) {
        *data++ = *left;
        *data++ = *left;
        *data++ = *left;
        *data++ = *left;
        *data++ = *left;
        *data++ = *left;
        *data++ = *left;
        *data++ = *left++;
    }

	}

/*------------------------------------------------------------------------------

    Function: IntraChromaVerticalPrediction

        Functional description:
          Perform intra chroma vertical prediction mode.

------------------------------------------------------------------------------*/
void IntraChromaVerticalPrediction(uint8_t *data, uint8_t *above) {
    uint32_t i;

    ASSERT(data);
    ASSERT(above);

    for(i = 8; i--;data++/*above-=8*/) {
        data[0] = *above;
        data[8] = *above;
        data[16] = *above;
        data[24] = *above;
        data[32] = *above;
        data[40] = *above;
        data[48] = *above;
        data[56] = *above++;
    }

	}

/*------------------------------------------------------------------------------

    Function: IntraChromaPlanePrediction

        Functional description:
          Perform intra chroma plane prediction mode.

------------------------------------------------------------------------------*/
void IntraChromaPlanePrediction(uint8_t *data, uint8_t *above, uint8_t *left) {
    uint32_t i;
    int32_t a, b, c;
    int32_t tmp;
    const uint8_t *clp = h264bsdClip + 512;


    ASSERT(data);
    ASSERT(above);
    ASSERT(left);

    a = 16 * (above[7] + left[7]);

    b = (above[4] - above[2]) + 2 * (above[5] - above[1])
        + 3 * (above[6] - above[0]) + 4 * (above[7] - above[-1]);
    b = (17 * b + 16) >> 5;

    /* p[-1,-1] has to be accessed through above pointer */
    c = (left[4] - left[2]) + 2 * (left[5] - left[1])
        + 3 * (left[6] - left[0]) + 4 * (left[7] - above[-1]);
    c = (17 * c + 16) >> 5;

  /*a += 16;*/
  a = a - 3 * c + 16;
  for(i = 8; i--; a += c) {
      tmp = (a - 3 * b);
      *data++ = clp[tmp>>5];
      tmp += b;
      *data++ = clp[tmp>>5];
      tmp += b;
      *data++ = clp[tmp>>5];
      tmp += b;
      *data++ = clp[tmp>>5];
      tmp += b;
      *data++ = clp[tmp>>5];
      tmp += b;
      *data++ = clp[tmp>>5];
      tmp += b;
      *data++ = clp[tmp>>5];
      tmp += b;
      *data++ = clp[tmp>>5];
    }

	}

/*------------------------------------------------------------------------------

    Function: Get4x4NeighbourPels

        Functional description:
          Get neighbouring pixels of a 4x4 block into 'a' and 'l'.

------------------------------------------------------------------------------*/
void Get4x4NeighbourPels(uint8_t *a, uint8_t *l, uint8_t *data, uint8_t *above, uint8_t *left, uint32_t blockNum) {
    uint32_t x, y;
    uint8_t t1, t2;


    ASSERT(a);
    ASSERT(l);
    ASSERT(data);
    ASSERT(above);
    ASSERT(left);
    ASSERT(blockNum < 16);

    x = h264bsdBlockX[blockNum];
    y = h264bsdBlockY[blockNum];

    /* A and D */
    if(x == 0) {
        t1 = left[y    ];
        t2 = left[y + 1];
        l[1] = t1;
        l[2] = t2;
        t1 = left[y + 2];
        t2 = left[y + 3];
        l[3] = t1;
        l[4] = t2;
		  }
    else {
        t1 = data[y * MACROBLOCK_SIZE + x - 1     ];
        t2 = data[y * MACROBLOCK_SIZE + x - 1 + 16];
        l[1] = t1;
        l[2] = t2;
        t1 = data[y * MACROBLOCK_SIZE + x - 1 + 32];
        t2 = data[y * MACROBLOCK_SIZE + x - 1 + 48];
        l[3] = t1;
        l[4] = t2;
			}

    /* B, C and D */
    if(y == 0) {
        t1 = above[x    ];
        t2 = above[x    ];
        l[0] = t1;
        a[0] = t2;
        t1 = above[x + 1];
        t2 = above[x + 2];
        a[1] = t1;
        a[2] = t2;
        t1 = above[x + 3];
        t2 = above[x + 4];
        a[3] = t1;
        a[4] = t2;
        t1 = above[x + 5];
        t2 = above[x + 6];
        a[5] = t1;
        a[6] = t2;
        t1 = above[x + 7];
        t2 = above[x + 8];
        a[7] = t1;
        a[8] = t2;
	    }
    else {
        t1 = data[(y - 1) * MACROBLOCK_SIZE + x    ];
        t2 = data[(y - 1) * MACROBLOCK_SIZE + x + 1];
        a[1] = t1;
        a[2] = t2;
        t1 = data[(y - 1) * MACROBLOCK_SIZE + x + 2];
        t2 = data[(y - 1) * MACROBLOCK_SIZE + x + 3];
        a[3] = t1;
        a[4] = t2;
        t1 = data[(y - 1) * MACROBLOCK_SIZE + x + 4];
        t2 = data[(y - 1) * MACROBLOCK_SIZE + x + 5];
        a[5] = t1;
        a[6] = t2;
        t1 = data[(y - 1) * MACROBLOCK_SIZE + x + 6];
        t2 = data[(y - 1) * MACROBLOCK_SIZE + x + 7];
        a[7] = t1;
        a[8] = t2;

        if(x == 0)
            l[0] = a[0] = left[y-1];
        else
            l[0] = a[0] = data[(y - 1) * MACROBLOCK_SIZE + x - 1];
    }
	}


/*------------------------------------------------------------------------------

    Function: Intra4x4VerticalPrediction

        Functional description:
          Perform intra 4x4 vertical prediction mode.

------------------------------------------------------------------------------*/
void Intra4x4VerticalPrediction(uint8_t *data, uint8_t *above) {
  uint8_t t1, t2;


  ASSERT(data);
  ASSERT(above);

  t1 = above[0];
  t2 = above[1];
  data[0] = data[4] = data[8] = data[12] = t1;
  data[1] = data[5] = data[9] = data[13] = t2;
  t1 = above[2];
  t2 = above[3];
  data[2] = data[6] = data[10] = data[14] = t1;
  data[3] = data[7] = data[11] = data[15] = t2;
	}

/*------------------------------------------------------------------------------

    Function: Intra4x4HorizontalPrediction

        Functional description:
          Perform intra 4x4 horizontal prediction mode.

------------------------------------------------------------------------------*/
void Intra4x4HorizontalPrediction(uint8_t *data, uint8_t *left) {
  uint8_t t1, t2;

  ASSERT(data);
  ASSERT(left);

  t1 = left[0];
  t2 = left[1];
  data[0] = data[1] = data[2] = data[3] = t1;
  data[4] = data[5] = data[6] = data[7] = t2;
  t1 = left[2];
  t2 = left[3];
  data[8] = data[9] = data[10] = data[11] = t1;
  data[12] = data[13] = data[14] = data[15] = t2;
	}

/*------------------------------------------------------------------------------

    Function: Intra4x4DcPrediction

        Functional description:
          Perform intra 4x4 DC prediction mode.

------------------------------------------------------------------------------*/
void Intra4x4DcPrediction(uint8_t *data, uint8_t *above, uint8_t *left, uint32_t availableA, uint32_t availableB) {
  uint32_t tmp;
  uint8_t t1, t2, t3, t4;

  ASSERT(data);
  ASSERT(above);
  ASSERT(left);

  if(availableA && availableB) {
      t1 = above[0]; t2 = above[1]; t3 = above[2]; t4 = above[3];
      tmp = t1 + t2 + t3 + t4;
      t1 = left[0]; t2 = left[1]; t3 = left[2]; t4 = left[3];
      tmp += t1 + t2 + t3 + t4;
      tmp = (tmp + 4) >> 3;
		}
  else if(availableA) {
      t1 = left[0]; t2 = left[1]; t3 = left[2]; t4 = left[3];
      tmp = (t1 + t2 + t3 + t4 + 2) >> 2;
		}
  else if(availableB) {
      t1 = above[0]; t2 = above[1]; t3 = above[2]; t4 = above[3];
      tmp = (t1 + t2 + t3 + t4 + 2) >> 2;
		}
  else {
      tmp = 128;
	  }

  ASSERT(tmp < 256);
  data[0] = data[1] = data[2] = data[3] =
  data[4] = data[5] = data[6] = data[7] =
  data[8] = data[9] = data[10] = data[11] =
  data[12] = data[13] = data[14] = data[15] = (uint8_t)tmp;
	}

/*------------------------------------------------------------------------------

    Function: Intra4x4DiagonalDownLeftPrediction

        Functional description:
          Perform intra 4x4 diagonal down-left prediction mode.

------------------------------------------------------------------------------*/
void Intra4x4DiagonalDownLeftPrediction(uint8_t *data, uint8_t *above) {

    ASSERT(data);
    ASSERT(above);

    data[ 0] = (above[0] + 2 * above[1] + above[2] + 2) >> 2;
    data[ 1] = (above[1] + 2 * above[2] + above[3] + 2) >> 2;
    data[ 4] = (above[1] + 2 * above[2] + above[3] + 2) >> 2;
    data[ 2] = (above[2] + 2 * above[3] + above[4] + 2) >> 2;
    data[ 5] = (above[2] + 2 * above[3] + above[4] + 2) >> 2;
    data[ 8] = (above[2] + 2 * above[3] + above[4] + 2) >> 2;
    data[ 3] = (above[3] + 2 * above[4] + above[5] + 2) >> 2;
    data[ 6] = (above[3] + 2 * above[4] + above[5] + 2) >> 2;
    data[ 9] = (above[3] + 2 * above[4] + above[5] + 2) >> 2;
    data[12] = (above[3] + 2 * above[4] + above[5] + 2) >> 2;
    data[ 7] = (above[4] + 2 * above[5] + above[6] + 2) >> 2;
    data[10] = (above[4] + 2 * above[5] + above[6] + 2) >> 2;
    data[13] = (above[4] + 2 * above[5] + above[6] + 2) >> 2;
    data[11] = (above[5] + 2 * above[6] + above[7] + 2) >> 2;
    data[14] = (above[5] + 2 * above[6] + above[7] + 2) >> 2;
    data[15] = (above[6] + 3 * above[7] + 2) >> 2;

	}

/*------------------------------------------------------------------------------

    Function: Intra4x4DiagonalDownRightPrediction

        Functional description:
          Perform intra 4x4 diagonal down-right prediction mode.

------------------------------------------------------------------------------*/
void Intra4x4DiagonalDownRightPrediction(uint8_t *data, uint8_t *above, uint8_t *left) {

    ASSERT(data);
    ASSERT(above);
    ASSERT(left);

    data[ 0] = (above[0] + 2 * above[-1] + left[0] + 2) >> 2;
    data[ 5] = (above[0] + 2 * above[-1] + left[0] + 2) >> 2;
    data[10] = (above[0] + 2 * above[-1] + left[0] + 2) >> 2;
    data[15] = (above[0] + 2 * above[-1] + left[0] + 2) >> 2;
    data[ 1] = (above[-1] + 2 * above[0] + above[1] + 2) >> 2;
    data[ 6] = (above[-1] + 2 * above[0] + above[1] + 2) >> 2;
    data[11] = (above[-1] + 2 * above[0] + above[1] + 2) >> 2;
    data[ 2] = (above[0] + 2 * above[1] + above[2] + 2) >> 2;
    data[ 7] = (above[0] + 2 * above[1] + above[2] + 2) >> 2;
    data[ 3] = (above[1] + 2 * above[2] + above[3] + 2) >> 2;
    data[ 4] = (left[-1] + 2 * left[0] + left[1] + 2) >> 2;
    data[ 9] = (left[-1] + 2 * left[0] + left[1] + 2) >> 2;
    data[14] = (left[-1] + 2 * left[0] + left[1] + 2) >> 2;
    data[ 8] = (left[0] + 2 * left[1] + left[2] + 2) >> 2;
    data[13] = (left[0] + 2 * left[1] + left[2] + 2) >> 2;
    data[12] = (left[1] + 2 * left[2] + left[3] + 2) >> 2;
	}

/*------------------------------------------------------------------------------

    Function: Intra4x4VerticalRightPrediction

        Functional description:
          Perform intra 4x4 vertical right prediction mode.

------------------------------------------------------------------------------*/
void Intra4x4VerticalRightPrediction(uint8_t *data, uint8_t *above, uint8_t *left) {

    ASSERT(data);
    ASSERT(above);
    ASSERT(left);

    data[ 0] = (above[-1] + above[0] + 1) >> 1;
    data[ 9] = (above[-1] + above[0] + 1) >> 1;
    data[ 5] = (above[-1] + 2 * above[0] + above[1] + 2) >> 2;
    data[14] = (above[-1] + 2 * above[0] + above[1] + 2) >> 2;
    data[ 4] = (above[0] + 2 * above[-1] + left[0] + 2) >> 2;
    data[13] = (above[0] + 2 * above[-1] + left[0] + 2) >> 2;
    data[ 1] = (above[0] + above[1] + 1) >> 1;
    data[10] = (above[0] + above[1] + 1) >> 1;
    data[ 6] = (above[0] + 2 * above[1] + above[2] + 2) >> 2;
    data[15] = (above[0] + 2 * above[1] + above[2] + 2) >> 2;
    data[ 2] = (above[1] + above[2] + 1) >> 1;
    data[11] = (above[1] + above[2] + 1) >> 1;
    data[ 7] = (above[1] + 2 * above[2] + above[3] + 2) >> 2;
    data[ 3] = (above[2] + above[3] + 1) >> 1;
    data[ 8] = (left[1] + 2 * left[0] + left[-1] + 2) >> 2;
    data[12] = (left[2] + 2 * left[1] + left[0] + 2) >> 2;

	}

/*------------------------------------------------------------------------------

    Function: Intra4x4HorizontalDownPrediction

        Functional description:
          Perform intra 4x4 horizontal down prediction mode.

------------------------------------------------------------------------------*/
void Intra4x4HorizontalDownPrediction(uint8_t *data, uint8_t *above, uint8_t *left) {

    ASSERT(data);
    ASSERT(above);
    ASSERT(left);

    data[ 0] = (left[-1] + left[0] + 1) >> 1;
    data[ 6] = (left[-1] + left[0] + 1) >> 1;
    data[ 5] = (left[-1] + 2 * left[0] + left[1] + 2) >> 2;
    data[11] = (left[-1] + 2 * left[0] + left[1] + 2) >> 2;
    data[ 4] = (left[0] + left[1] + 1) >> 1;
    data[10] = (left[0] + left[1] + 1) >> 1;
    data[ 9] = (left[0] + 2 * left[1] + left[2] + 2) >> 2;
    data[15] = (left[0] + 2 * left[1] + left[2] + 2) >> 2;
    data[ 8] = (left[1] + left[2] + 1) >> 1;
    data[14] = (left[1] + left[2] + 1) >> 1;
    data[13] = (left[1] + 2 * left[2] + left[3] + 2) >> 2;
    data[12] = (left[2] + left[3] + 1) >> 1;
    data[ 1] = (above[0] + 2 * above[-1] + left[0] + 2) >> 2;
    data[ 7] = (above[0] + 2 * above[-1] + left[0] + 2) >> 2;
    data[ 2] = (above[1] + 2 * above[0] + above[-1] + 2) >> 2;
    data[ 3] = (above[2] + 2 * above[1] + above[0] + 2) >> 2;
	}

/*------------------------------------------------------------------------------

    Function: Intra4x4VerticalLeftPrediction

        Functional description:
          Perform intra 4x4 vertical left prediction mode.

------------------------------------------------------------------------------*/
void Intra4x4VerticalLeftPrediction(uint8_t *data, uint8_t *above) {


    ASSERT(data);
    ASSERT(above);

    data[ 0] = (above[0] + above[1] + 1) >> 1;
    data[ 1] = (above[1] + above[2] + 1) >> 1;
    data[ 2] = (above[2] + above[3] + 1) >> 1;
    data[ 3] = (above[3] + above[4] + 1) >> 1;
    data[ 4] = (above[0] + 2 * above[1] + above[2] + 2) >> 2;
    data[ 5] = (above[1] + 2 * above[2] + above[3] + 2) >> 2;
    data[ 6] = (above[2] + 2 * above[3] + above[4] + 2) >> 2;
    data[ 7] = (above[3] + 2 * above[4] + above[5] + 2) >> 2;
    data[ 8] = (above[1] + above[2] + 1) >> 1;
    data[ 9] = (above[2] + above[3] + 1) >> 1;
    data[10] = (above[3] + above[4] + 1) >> 1;
    data[11] = (above[4] + above[5] + 1) >> 1;
    data[12] = (above[1] + 2 * above[2] + above[3] + 2) >> 2;
    data[13] = (above[2] + 2 * above[3] + above[4] + 2) >> 2;
    data[14] = (above[3] + 2 * above[4] + above[5] + 2) >> 2;
    data[15] = (above[4] + 2 * above[5] + above[6] + 2) >> 2;

	}

/*------------------------------------------------------------------------------

    Function: Intra4x4HorizontalUpPrediction

        Functional description:
          Perform intra 4x4 horizontal up prediction mode.

------------------------------------------------------------------------------*/
void Intra4x4HorizontalUpPrediction(uint8_t *data, uint8_t *left) {

    ASSERT(data);
    ASSERT(left);

    data[ 0] = (left[0] + left[1] + 1) >> 1;
    data[ 1] = (left[0] + 2 * left[1] + left[2] + 2) >> 2;
    data[ 2] = (left[1] + left[2] + 1) >> 1;
    data[ 3] = (left[1] + 2 * left[2] + left[3] + 2) >> 2;
    data[ 4] = (left[1] + left[2] + 1) >> 1;
    data[ 5] = (left[1] + 2 * left[2] + left[3] + 2) >> 2;
    data[ 6] = (left[2] + left[3] + 1) >> 1;
    data[ 7] = (left[2] + 3 * left[3] + 2) >> 2;
    data[ 8] = (left[2] + left[3] + 1) >> 1;
    data[ 9] = (left[2] + 3 * left[3] + 2) >> 2;
    data[10] = left[3];
    data[11] = left[3];
    data[12] = left[3];
    data[13] = left[3];
    data[14] = left[3];
    data[15] = left[3];

	}

#endif /* H264DEC_OMXDL */

/*------------------------------------------------------------------------------

    Function: Write4x4To16x16

        Functional description:
          Write a 4x4 block (data4x4) into correct position
          in 16x16 macroblock (data).

------------------------------------------------------------------------------*/
void Write4x4To16x16(uint8_t *data, uint8_t *data4x4, uint32_t blockNum) {
    uint32_t x, y;
    uint32_t *in32, *out32;


    ASSERT(data);
    ASSERT(data4x4);
    ASSERT(blockNum < 16);

    x = h264bsdBlockX[blockNum];
    y = h264bsdBlockY[blockNum];

    data += y*16+x;

    ASSERT(((uint32_t)data&0x3) == 0);

    out32 = (uint32_t *)data;
    in32 = (uint32_t *)data4x4;

    out32[0] = *in32++;
    out32[4] = *in32++;
    out32[8] = *in32++;
    out32[12] = *in32++;
	}

/*------------------------------------------------------------------------------

    Function: DetermineIntra4x4PredMode

        Functional description:
          Returns the intra 4x4 prediction mode of a block based on the
          neighbouring macroblocks and information parsed from stream.

------------------------------------------------------------------------------*/
uint32_t DetermineIntra4x4PredMode(macroblockLayer_t *pMbLayer,
    uint32_t available, neighbour_t *nA, neighbour_t *nB, uint32_t index,
    mbStorage_t *nMbA, mbStorage_t *nMbB) {
    uint32_t mode1, mode2;
    mbStorage_t *pMb;


    ASSERT(pMbLayer);

    /* dc only prediction? */
    if(!available)
        mode1 = 2;
    else {
        pMb = nMbA;
        if(h264bsdMbPartPredMode(pMb->mbType) == PRED_MODE_INTRA4x4)
            mode1 = pMb->intra4x4PredMode[nA->index];
        else
            mode1 = 2;

        pMb = nMbB;
        if(h264bsdMbPartPredMode(pMb->mbType) == PRED_MODE_INTRA4x4)
            mode2 = pMb->intra4x4PredMode[nB->index];
        else
            mode2 = 2;

        mode1 = MIN(mode1, mode2);
	    }

    if(!pMbLayer->mbPred.prevIntra4x4PredModeFlag[index]) {
        if(pMbLayer->mbPred.remIntra4x4PredMode[index] < mode1)
            mode1 = pMbLayer->mbPred.remIntra4x4PredMode[index];
        else
            mode1 = pMbLayer->mbPred.remIntra4x4PredMode[index] + 1;
	    }

  return(mode1);
	}




#ifdef H264DEC_OMXDL
static const uint32_t chromaIndex[8] = { 256, 260, 288, 292, 320, 324, 352, 356 };
static const uint32_t lumaIndex[16] = {   0,   4,  64,  68,
                                     8,  12,  72,  76,
                                   128, 132, 192, 196,
                                   136, 140, 200, 204 };
#endif
/* mapping of dc coefficients array to luma blocks */
static const uint32_t dcCoeffIndex[16] =
{0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15};

static uint32_t DecodeMbPred(strmData_t *pStrmData, mbPred_t *pMbPred,
    mbType_e mbType, uint32_t numRefIdxActive);
static uint32_t DecodeSubMbPred(strmData_t *pStrmData, subMbPred_t *pSubMbPred,
    mbType_e mbType, uint32_t numRefIdxActive);
static uint32_t DecodeResidual(strmData_t *pStrmData, residual_t *pResidual,
    mbStorage_t *pMb, mbType_e mbType, uint32_t codedBlockPattern);

#ifdef H264DEC_OMXDL
static uint32_t DetermineNc(mbStorage_t *pMb, uint32_t blockIndex, uint8_t *pTotalCoeff);
#else
static uint32_t DetermineNc(mbStorage_t *pMb, uint32_t blockIndex, int16_t *pTotalCoeff);
#endif

static uint32_t CbpIntra16x16(mbType_e mbType);
#ifdef H264DEC_OMXDL
static uint32_t ProcessIntra4x4Residual(mbStorage_t *pMb, uint8_t *data, uint32_t constrainedIntraPred,
                    macroblockLayer_t *mbLayer, const uint8_t **pSrc, image_t *image);
static uint32_t ProcessChromaResidual(mbStorage_t *pMb, uint8_t *data, const uint8_t **pSrc );
static uint32_t ProcessIntra16x16Residual(mbStorage_t *pMb, uint8_t *data, uint32_t constrainedIntraPred,
                    uint32_t intraChromaPredMode, const uint8_t **pSrc, image_t *image);


#else
static uint32_t ProcessResidual(mbStorage_t *pMb, int32_t residualLevel[][16], uint32_t *);
#endif

/*------------------------------------------------------------------------------

    Function name: h264bsdDecodeMacroblockLayer

        Functional description:
          Parse macroblock specific information from bit stream.

        Inputs:
          pStrmData         pointer to stream data structure
          pMb               pointer to macroblock storage structure
          sliceType         type of the current slice
          numRefIdxActive   maximum reference index

        Outputs:
          pMbLayer          stores the macroblock data parsed from stream

        Returns:
          HANTRO_OK         success
          HANTRO_NOK        end of stream or error in stream

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeMacroblockLayer(strmData_t *pStrmData,
    macroblockLayer_t *pMbLayer, mbStorage_t *pMb, uint32_t sliceType,
    uint32_t numRefIdxActive) {
    uint32_t tmp, i, value;
    int32_t itmp;
    mbPartPredMode_e partMode;


    ASSERT(pStrmData);
    ASSERT(pMbLayer);

#ifdef H264DEC_NEON
    h264bsdClearMbLayer(pMbLayer, ((sizeof(macroblockLayer_t) + 63) & ~0x3F));
#else
    memset(pMbLayer, 0, sizeof(macroblockLayer_t));
#endif

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);

    if(IS_I_SLICE(sliceType)) {
      if((value + 6) > 31 || tmp != HANTRO_OK)
        return(HANTRO_NOK);
      pMbLayer->mbType = (mbType_e)(value + 6);
			}
    else {
      if((value + 1) > 31 || tmp != HANTRO_OK)
          return(HANTRO_NOK);
      pMbLayer->mbType = (mbType_e)(value + 1);
			}

    if(pMbLayer->mbType == I_PCM) {
      int32_t *level;
      while( !h264bsdIsByteAligned(pStrmData)) {
          /* pcm_alignment_zero_bit */
        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp)
          return(HANTRO_NOK);
				}

      level = pMbLayer->residual.level[0];
      for(i=0; i < 384; i++) {
        value = h264bsdGetBits(pStrmData, 8);
        if(value == END_OF_STREAM)
            return(HANTRO_NOK);
        *level++ = (int32_t)value;
        }
			}
    else {
      partMode = h264bsdMbPartPredMode(pMbLayer->mbType);
      if((partMode == PRED_MODE_INTER) && (h264bsdNumMbPart(pMbLayer->mbType) == 4)) {
          tmp = DecodeSubMbPred(pStrmData, &pMbLayer->subMbPred,
              pMbLayer->mbType, numRefIdxActive);
				}
      else {
          tmp = DecodeMbPred(pStrmData, &pMbLayer->mbPred, pMbLayer->mbType, numRefIdxActive);
	      }
      if(tmp != HANTRO_OK)
          return tmp;

      if(partMode != PRED_MODE_INTRA16x16) {
          tmp = h264bsdDecodeExpGolombMapped(pStrmData, &value, (uint32_t)(partMode == PRED_MODE_INTRA4x4));
          if(tmp != HANTRO_OK)
              return tmp;
          pMbLayer->codedBlockPattern = value;
				}
      else
        pMbLayer->codedBlockPattern = CbpIntra16x16(pMbLayer->mbType);

      if( pMbLayer->codedBlockPattern || (partMode == PRED_MODE_INTRA16x16)) {
        tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
        if(tmp != HANTRO_OK || (itmp < -26) || (itmp > 25))
            return(HANTRO_NOK);
        pMbLayer->mbQpDelta = itmp;

        tmp = DecodeResidual(pStrmData, &pMbLayer->residual, pMb,
            pMbLayer->mbType, pMbLayer->codedBlockPattern);

        pStrmData->strmBuffReadBits =
            (uint32_t)(pStrmData->pStrmCurrPos - pStrmData->pStrmBuffStart) * 8 +
            pStrmData->bitPosInWord;

        if(tmp != HANTRO_OK)
            return tmp;
        }
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdMbPartPredMode

        Functional description:
          Returns the prediction mode of a macroblock type

------------------------------------------------------------------------------*/
mbPartPredMode_e h264bsdMbPartPredMode(mbType_e mbType) {

  ASSERT(mbType <= 31);

  if((mbType <= P_8x8ref0))
      return(PRED_MODE_INTER);
  else if(mbType == I_4x4)
      return(PRED_MODE_INTRA4x4);
  else
      return(PRED_MODE_INTRA16x16);

	}

/*------------------------------------------------------------------------------

    Function: h264bsdNumMbPart

        Functional description:
          Returns the amount of macroblock partitions in a macroblock type

------------------------------------------------------------------------------*/
uint32_t h264bsdNumMbPart(mbType_e mbType) {

  ASSERT(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTER);

  switch(mbType) {
    case P_L0_16x16:
    case P_Skip:
        return 1;

    case P_L0_L0_16x8:
    case P_L0_L0_8x16:
        return(2);

    /* P_8x8 or P_8x8ref0 */
    default:
        return(4);
    }

	}

/*------------------------------------------------------------------------------

    Function: h264bsdNumSubMbPart

        Functional description:
          Returns the amount of sub-partitions in a sub-macroblock type

------------------------------------------------------------------------------*/
uint32_t h264bsdNumSubMbPart(subMbType_e subMbType) {

  ASSERT(subMbType <= P_L0_4x4);

  switch(subMbType) {
      case P_L0_8x8:
          return 1;

      case P_L0_8x4:
      case P_L0_4x8:
          return 2;

      /* P_L0_4x4 */
      default:
          return 4;
    }

	}

/*------------------------------------------------------------------------------

    Function: DecodeMbPred

        Functional description:
          Parse macroblock prediction information from bit stream and store
          in 'pMbPred'.

------------------------------------------------------------------------------*/

uint32_t DecodeMbPred(strmData_t *pStrmData, mbPred_t *pMbPred, mbType_e mbType,
    uint32_t numRefIdxActive) {
    uint32_t tmp, i, j, value;
    int32_t itmp;


    ASSERT(pStrmData);
    ASSERT(pMbPred);

    switch(h264bsdMbPartPredMode(mbType)) {
        case PRED_MODE_INTER: /* PRED_MODE_INTER */
            if(numRefIdxActive > 1) {
                for(i = h264bsdNumMbPart(mbType), j=0; i--; j++) {
                    tmp = h264bsdDecodeExpGolombTruncated(pStrmData, &value,
                        (uint32_t)(numRefIdxActive > 2));
                    if(tmp != HANTRO_OK || value >= numRefIdxActive)
                        return(HANTRO_NOK);

                    pMbPred->refIdxL0[j] = value;
                }
	            }

            for(i = h264bsdNumMbPart(mbType), j=0; i--; j++) {
                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                if(tmp != HANTRO_OK)
                    return tmp;
                pMbPred->mvdL0[j].hor = (int16_t)itmp;

                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                if(tmp != HANTRO_OK)
                    return tmp;
                pMbPred->mvdL0[j].ver = (int16_t)itmp;
		          }
            break;

        case PRED_MODE_INTRA4x4:
            for(itmp=0, i=0; itmp < 2; itmp++) {
                value = h264bsdShowBits32(pStrmData);
                tmp=0;
                for(j = 8; j--; i++) {
                    pMbPred->prevIntra4x4PredModeFlag[i] =
                        value & 0x80000000 ? HANTRO_TRUE : HANTRO_FALSE;
                    value <<= 1;
                    if(!pMbPred->prevIntra4x4PredModeFlag[i])  {
                        pMbPred->remIntra4x4PredMode[i] = value>>29;
                        value <<= 3;
                        tmp++;
                    }
                }
                if(h264bsdFlushBits(pStrmData, 8 + 3*tmp) == END_OF_STREAM)
                    return(HANTRO_NOK);
	            }
            /* fall-through */

        case PRED_MODE_INTRA16x16:
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if(tmp != HANTRO_OK || value > 3)
                return(HANTRO_NOK);
            pMbPred->intraChromaPredMode = value;
            break;
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeSubMbPred

        Functional description:
          Parse sub-macroblock prediction information from bit stream and
          store in 'pMbPred'.

------------------------------------------------------------------------------*/
uint32_t DecodeSubMbPred(strmData_t *pStrmData, subMbPred_t *pSubMbPred,
    mbType_e mbType, uint32_t numRefIdxActive) {
    uint32_t tmp, i, j, value;
    int32_t itmp;


    ASSERT(pStrmData);
    ASSERT(pSubMbPred);
    ASSERT(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTER);

    for(i=0; i < 4; i++) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK || value > 3)
            return(HANTRO_NOK);
        pSubMbPred->subMbType[i] = (subMbType_e)value;
		  }

    if((numRefIdxActive > 1) && (mbType != P_8x8ref0)) {
        for(i=0; i < 4; i++) {
            tmp = h264bsdDecodeExpGolombTruncated(pStrmData, &value,
                (uint32_t)(numRefIdxActive > 2));
            if(tmp != HANTRO_OK || value >= numRefIdxActive)
                return(HANTRO_NOK);
            pSubMbPred->refIdxL0[i] = value;
				  }
			}

    for(i=0; i < 4; i++) {
        j=0;
        for(value = h264bsdNumSubMbPart(pSubMbPred->subMbType[i]);
             value--; j++) {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if(tmp != HANTRO_OK)
                return tmp;
            pSubMbPred->mvdL0[i][j].hor = (int16_t)itmp;

            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if(tmp != HANTRO_OK)
                return tmp;
            pSubMbPred->mvdL0[i][j].ver = (int16_t)itmp;
        }
    }

  return(HANTRO_OK);
	}

#ifdef H264DEC_OMXDL
/*------------------------------------------------------------------------------

    Function: DecodeResidual

        Functional description:
          Parse residual information from bit stream and store in 'pResidual'.

------------------------------------------------------------------------------*/

uint32_t DecodeResidual(strmData_t *pStrmData, residual_t *pResidual,
    mbStorage_t *pMb, mbType_e mbType, uint32_t codedBlockPattern) {
    uint32_t i, j;
    uint32_t blockCoded;
    uint32_t blockIndex;
    uint32_t is16x16;
    OMX_INT nc;
    OMXResult omxRes;
    OMX_U8 *pPosCoefBuf;



    ASSERT(pStrmData);
    ASSERT(pResidual);

    pPosCoefBuf = pResidual->posCoefBuf;

    /* luma DC is at index 24 */
    if(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTRA16x16) {
        nc = (OMX_INT)DetermineNc(pMb, 0, pResidual->totalCoeff);
#ifndef H264DEC_NEON
        omxRes =  omxVCM4P10_DecodeCoeffsToPairCAVLC(
                (const OMX_U8 **) (&pStrmData->pStrmCurrPos),
                (OMX_S32*) (&pStrmData->bitPosInWord),
                &pResidual->totalCoeff[24],
                &pPosCoefBuf,
                nc,
                16);
#else
        omxRes = armVCM4P10_DecodeCoeffsToPair(
                (const OMX_U8 **) (&pStrmData->pStrmCurrPos),
                (OMX_S32*) (&pStrmData->bitPosInWord),
                &pResidual->totalCoeff[24],
                &pPosCoefBuf,
                nc,
                16);
#endif
        if(omxRes != OMX_Sts_NoErr)
            return(HANTRO_NOK);
        is16x16 = HANTRO_TRUE;
			}
    else
        is16x16 = HANTRO_FALSE;

    for(i = 4, blockIndex=0; i--;) {
        /* luma cbp in bits 0-3 */
        blockCoded = codedBlockPattern & 0x1;
        codedBlockPattern >>= 1;
        if(blockCoded) {
            for(j = 4; j--; blockIndex++) {
                nc = (OMX_INT)DetermineNc(pMb,blockIndex,pResidual->totalCoeff);
                if(is16x16) {
#ifndef H264DEC_NEON
                    omxRes =  omxVCM4P10_DecodeCoeffsToPairCAVLC(
                            (const OMX_U8 **) (&pStrmData->pStrmCurrPos),
                            (OMX_S32*) (&pStrmData->bitPosInWord),
                            &pResidual->totalCoeff[blockIndex],
                            &pPosCoefBuf,
                            nc,
                            15);
#else
                    omxRes =  armVCM4P10_DecodeCoeffsToPair(
                            (const OMX_U8 **) (&pStrmData->pStrmCurrPos),
                            (OMX_S32*) (&pStrmData->bitPosInWord),
                            &pResidual->totalCoeff[blockIndex],
                            &pPosCoefBuf,
                            nc,
                            15);
#endif
                }
                else {
#ifndef H264DEC_NEON
                    omxRes =  omxVCM4P10_DecodeCoeffsToPairCAVLC(
                            (const OMX_U8 **) (&pStrmData->pStrmCurrPos),
                            (OMX_S32*) (&pStrmData->bitPosInWord),
                            &pResidual->totalCoeff[blockIndex],
                            &pPosCoefBuf,
                            nc,
                            16);
#else
                    omxRes = armVCM4P10_DecodeCoeffsToPair(
                            (const OMX_U8 **) (&pStrmData->pStrmCurrPos),
                            (OMX_S32*) (&pStrmData->bitPosInWord),
                            &pResidual->totalCoeff[blockIndex],
                            &pPosCoefBuf,
                            nc,
                            16);
#endif
                }
                if(omxRes != OMX_Sts_NoErr)
                    return(HANTRO_NOK);
            }
        }
        else
            blockIndex += 4;
    }

    /* chroma DC block are at indices 25 and 26 */
    blockCoded = codedBlockPattern & 0x3;
    if(blockCoded) {
#ifndef H264DEC_NEON
        omxRes =  omxVCM4P10_DecodeChromaDcCoeffsToPairCAVLC(
                (const OMX_U8**) (&pStrmData->pStrmCurrPos),
                (OMX_S32*) (&pStrmData->bitPosInWord),
                &pResidual->totalCoeff[25],
                &pPosCoefBuf);
#else
        omxRes = armVCM4P10_DecodeCoeffsToPair(
                (const OMX_U8**) (&pStrmData->pStrmCurrPos),
                (OMX_S32*) (&pStrmData->bitPosInWord),
                &pResidual->totalCoeff[25],
                &pPosCoefBuf,
                17,
                4);
#endif
        if(omxRes != OMX_Sts_NoErr)
            return(HANTRO_NOK);
#ifndef H264DEC_NEON
        omxRes =  omxVCM4P10_DecodeChromaDcCoeffsToPairCAVLC(
                (const OMX_U8**) (&pStrmData->pStrmCurrPos),
                (OMX_S32*) (&pStrmData->bitPosInWord),
                &pResidual->totalCoeff[26],
                &pPosCoefBuf);
#else
        omxRes = armVCM4P10_DecodeCoeffsToPair(
                (const OMX_U8**) (&pStrmData->pStrmCurrPos),
                (OMX_S32*) (&pStrmData->bitPosInWord),
                &pResidual->totalCoeff[26],
                &pPosCoefBuf,
                17,
                4);
#endif
        if(omxRes != OMX_Sts_NoErr)
            return(HANTRO_NOK);
    }

    /* chroma AC */
    blockCoded = codedBlockPattern & 0x2;
    if(blockCoded) {
        for(i = 8; i--; blockIndex++) {
            nc = (OMX_INT)DetermineNc(pMb, blockIndex, pResidual->totalCoeff);
#ifndef H264DEC_NEON
            omxRes =  omxVCM4P10_DecodeCoeffsToPairCAVLC(
                    (const OMX_U8 **) (&pStrmData->pStrmCurrPos),
                    (OMX_S32*) (&pStrmData->bitPosInWord),
                    &pResidual->totalCoeff[blockIndex],
                    &pPosCoefBuf,
                    nc,
                    15);
#else
            omxRes =  armVCM4P10_DecodeCoeffsToPair(
                    (const OMX_U8 **) (&pStrmData->pStrmCurrPos),
                    (OMX_S32*) (&pStrmData->bitPosInWord),
                    &pResidual->totalCoeff[blockIndex],
                    &pPosCoefBuf,
                    nc,
                    15);
#endif
            if(omxRes != OMX_Sts_NoErr)
                return(HANTRO_NOK);
        }
    }

  return(HANTRO_OK);
	}

#else
/*------------------------------------------------------------------------------

    Function: DecodeResidual

        Functional description:
          Parse residual information from bit stream and store in 'pResidual'.

------------------------------------------------------------------------------*/
uint32_t DecodeResidual(strmData_t *pStrmData, residual_t *pResidual,
    mbStorage_t *pMb, mbType_e mbType, uint32_t codedBlockPattern) {
    uint32_t i, j, tmp;
    int32_t nc;
    uint32_t blockCoded;
    uint32_t blockIndex;
    uint32_t is16x16;
    int32_t (*level)[16];


    ASSERT(pStrmData);
    ASSERT(pResidual);

    level = pResidual->level;

    /* luma DC is at index 24 */
    if(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTRA16x16) {
        nc = (int32_t)DetermineNc(pMb, 0, pResidual->totalCoeff);
        tmp = h264bsdDecodeResidualBlockCavlc(pStrmData, level[24], nc, 16);
        if((tmp & 0xF) != HANTRO_OK)
            return tmp;
        pResidual->totalCoeff[24] = (tmp >> 4) & 0xFF;
        is16x16 = HANTRO_TRUE;
    }
    else
        is16x16 = HANTRO_FALSE;

    for(i = 4, blockIndex=0; i--;) {
        /* luma cbp in bits 0-3 */
        blockCoded = codedBlockPattern & 0x1;
        codedBlockPattern >>= 1;
        if(blockCoded) {
            for(j = 4; j--; blockIndex++) {
                nc = (int32_t)DetermineNc(pMb, blockIndex, pResidual->totalCoeff);
                if(is16x16) {
                    tmp = h264bsdDecodeResidualBlockCavlc(pStrmData,
                        level[blockIndex] + 1, nc, 15);
                    pResidual->coeffMap[blockIndex] = tmp >> 15;
				          }
                else {
                    tmp = h264bsdDecodeResidualBlockCavlc(pStrmData,
                        level[blockIndex], nc, 16);
                    pResidual->coeffMap[blockIndex] = tmp >> 16;
			            }
                if((tmp & 0xF) != HANTRO_OK)
                    return tmp;
                pResidual->totalCoeff[blockIndex] = (tmp >> 4) & 0xFF;
            }
	        }
        else
            blockIndex += 4;
		  }

    /* chroma DC block are at indices 25 and 26 */
    blockCoded = codedBlockPattern & 0x3;
    if(blockCoded) {
        tmp = h264bsdDecodeResidualBlockCavlc(pStrmData, level[25], -1, 4);
        if((tmp & 0xF) != HANTRO_OK)
            return tmp;
        pResidual->totalCoeff[25] = (tmp >> 4) & 0xFF;
        tmp = h264bsdDecodeResidualBlockCavlc(pStrmData, level[25]+4, -1, 4);
        if((tmp & 0xF) != HANTRO_OK)
            return tmp;
        pResidual->totalCoeff[26] = (tmp >> 4) & 0xFF;
    }

    /* chroma AC */
    blockCoded = codedBlockPattern & 0x2;
    if(blockCoded) {
        for(i = 8; i--;blockIndex++) {
          nc = (int32_t)DetermineNc(pMb, blockIndex, pResidual->totalCoeff);
          tmp = h264bsdDecodeResidualBlockCavlc(pStrmData,
              level[blockIndex] + 1, nc, 15);
          if((tmp & 0xF) != HANTRO_OK)
              return tmp;
          pResidual->totalCoeff[blockIndex] = (tmp >> 4) & 0xFF;
          pResidual->coeffMap[blockIndex] = (tmp >> 15);
        }
    }

  return(HANTRO_OK);
	}
#endif

/*------------------------------------------------------------------------------

    Function: DetermineNc

        Functional description:
          Returns the nC of a block.

------------------------------------------------------------------------------*/
#ifdef H264DEC_OMXDL
uint32_t DetermineNc(mbStorage_t *pMb, uint32_t blockIndex, uint8_t *pTotalCoeff)
#else
uint32_t DetermineNc(mbStorage_t *pMb, uint32_t blockIndex, int16_t *pTotalCoeff)
#endif
{
    uint32_t tmp;
    int32_t n;
    const neighbour_t *neighbourA, *neighbourB;
    uint8_t neighbourAindex, neighbourBindex;


    ASSERT(blockIndex < 24);

    /* if neighbour block belongs to current macroblock totalCoeff array
     * mbStorage has not been set/updated yet -> use pTotalCoeff */
    neighbourA = h264bsdNeighbour4x4BlockA(blockIndex);
    neighbourB = h264bsdNeighbour4x4BlockB(blockIndex);
    neighbourAindex = neighbourA->index;
    neighbourBindex = neighbourB->index;
    if(neighbourA->mb == MB_CURR && neighbourB->mb == MB_CURR)
        n = (pTotalCoeff[neighbourAindex] + pTotalCoeff[neighbourBindex] + 1)>>1;
    else if(neighbourA->mb == MB_CURR) {
        n = pTotalCoeff[neighbourAindex];
        if(h264bsdIsNeighbourAvailable(pMb, pMb->mbB))
            n = (n + pMb->mbB->totalCoeff[neighbourBindex] + 1) >> 1;
	    }
    else if(neighbourB->mb == MB_CURR) {
        n = pTotalCoeff[neighbourBindex];
        if(h264bsdIsNeighbourAvailable(pMb, pMb->mbA))
            n = (n + pMb->mbA->totalCoeff[neighbourAindex] + 1) >> 1;
		  }
    else {
        n = tmp=0;
        if(h264bsdIsNeighbourAvailable(pMb, pMb->mbA)) {
            n = pMb->mbA->totalCoeff[neighbourAindex];
            tmp = 1;
        }
        if(h264bsdIsNeighbourAvailable(pMb, pMb->mbB)) {
            if(tmp)
                n = (n + pMb->mbB->totalCoeff[neighbourBindex] + 1) >> 1;
            else
                n = pMb->mbB->totalCoeff[neighbourBindex];
	        }
    }
  return((uint32_t)n);
	}

/*------------------------------------------------------------------------------

    Function: CbpIntra16x16

        Functional description:
          Returns the coded block pattern for intra 16x16 macroblock.

------------------------------------------------------------------------------*/
uint32_t CbpIntra16x16(mbType_e mbType) {
  uint32_t cbp;
  uint32_t tmp;


  ASSERT(mbType >= I_16x16_0_0_0 && mbType <= I_16x16_3_2_1);

  if(mbType >= I_16x16_0_0_1)
      cbp = 15;
  else
      cbp=0;

  /* tmp is 0 for I_16x16_0_0_0 mb type */
  /* ignore lint warning on arithmetic on enum's */
  tmp = /*lint -e(656)*/(mbType - I_16x16_0_0_0) >> 2;
  if(tmp > 2)
      tmp -= 3;

  cbp += tmp << 4;

  return cbp;

	}

/*------------------------------------------------------------------------------

    Function: h264bsdPredModeIntra16x16

        Functional description:
          Returns the prediction mode for intra 16x16 macroblock.

------------------------------------------------------------------------------*/
uint32_t h264bsdPredModeIntra16x16(mbType_e mbType) {
    uint32_t tmp;


    ASSERT(mbType >= I_16x16_0_0_0 && mbType <= I_16x16_3_2_1);

    /* tmp is 0 for I_16x16_0_0_0 mb type */
    /* ignore lint warning on arithmetic on enum's */
    tmp = /*lint -e(656)*/(mbType - I_16x16_0_0_0);

  return(tmp & 0x3);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdDecodeMacroblock

        Functional description:
          Decode one macroblock and write into output image.

        Inputs:
          pMb           pointer to macroblock specific information
          mbLayer       pointer to current macroblock data from stream
          currImage     pointer to output image
          dpb           pointer to decoded picture buffer
          qpY           pointer to slice QP
          mbNum         current macroblock number
          constrainedIntraPred  flag specifying if neighbouring inter
                                macroblocks are used in intra prediction

        Outputs:
          pMb           structure is updated with current macroblock
          currImage     decoded macroblock is written into output image

        Returns:
          HANTRO_OK     success
          HANTRO_NOK    error in macroblock decoding

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeMacroblock(mbStorage_t *pMb, macroblockLayer_t *pMbLayer,
    image_t *currImage, dpbStorage_t *dpb, int32_t *qpY, uint32_t mbNum,
    uint32_t constrainedIntraPredFlag, uint8_t* data) {
    uint32_t i, tmp;
    mbType_e mbType;
#ifdef H264DEC_OMXDL
    const uint8_t *pSrc;
#endif


    ASSERT(pMb);
    ASSERT(pMbLayer);
    ASSERT(currImage);
    ASSERT(qpY && *qpY < 52);
    ASSERT(mbNum < currImage->width*currImage->height);

    mbType = pMbLayer->mbType;
    pMb->mbType = mbType;

    pMb->decoded++;

    h264bsdSetCurrImageMbPointers(currImage, mbNum);

    if(mbType == I_PCM) {
        uint8_t *pData = (uint8_t*)data;
#ifdef H264DEC_OMXDL
        uint8_t *tot = pMb->totalCoeff;
#else
        int16_t *tot = pMb->totalCoeff;
#endif
        int32_t *lev = pMbLayer->residual.level[0];

        pMb->qpY=0;

        /* if decoded flag > 1 -> mb has already been successfully decoded and
         * written to output -> do not write again */
        if(pMb->decoded > 1) {
            for(i = 24; i--;)
                *tot++ = 16;
            return HANTRO_OK;
				  }

        for(i = 24; i--;) {
            *tot++ = 16;
            for(tmp = 16; tmp--;)
                *pData++ = (uint8_t)(*lev++);
			    }
        h264bsdWriteMacroblock(currImage, (uint8_t*)data);

        return(HANTRO_OK);
		  }
    else {
#ifdef H264DEC_OMXDL
        if(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTER) {
            tmp = h264bsdInterPrediction(pMb, pMbLayer, dpb, mbNum,
                currImage, (uint8_t*)data);
            if(tmp != HANTRO_OK) return tmp;
	        }
#endif
        if(mbType != P_Skip) {
            memcpy(pMb->totalCoeff,
                            pMbLayer->residual.totalCoeff,
                            27*sizeof(*pMb->totalCoeff));

            /* update qpY */
            if(pMbLayer->mbQpDelta) {
                *qpY = *qpY + pMbLayer->mbQpDelta;
                if(*qpY < 0) *qpY += 52;
                else if(*qpY >= 52) *qpY -= 52;
	            }
            pMb->qpY = (uint32_t)*qpY;

#ifdef H264DEC_OMXDL
            pSrc = pMbLayer->residual.posCoefBuf;

            if(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTER) {
              OMXResult res;
              uint8_t *p;
              uint8_t *totalCoeff = pMb->totalCoeff;

              for(i=0; i < 16; i++, totalCoeff++) {
                p = data + lumaIndex[i];
                if(*totalCoeff)  {
                  res = omxVCM4P10_DequantTransformResidualFromPairAndAdd(
                          &pSrc, p, 0, p, 16, 16, *qpY, *totalCoeff);
                  if(res != OMX_Sts_NoErr)
                      return (HANTRO_NOK);
                  }
                }

					    }
            else if(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTRA4x4) {
                tmp = ProcessIntra4x4Residual(pMb, data,
                                              constrainedIntraPredFlag, pMbLayer,
                                              &pSrc, currImage);
                if(tmp != HANTRO_OK)
                    return tmp;
	            }
            else if(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTRA16x16) {
                tmp = ProcessIntra16x16Residual(pMb, data,
                                        constrainedIntraPredFlag, pMbLayer->mbPred.intraChromaPredMode,
                                        &pSrc, currImage);
                if(tmp != HANTRO_OK)
                    return tmp;
		          }

            tmp = ProcessChromaResidual(pMb, data, &pSrc);

#else
            tmp = ProcessResidual(pMb, pMbLayer->residual.level, pMbLayer->residual.coeffMap);
#endif
            if(tmp != HANTRO_OK)
                return tmp;
			    }
        else {
            memset(pMb->totalCoeff, 0, 27*sizeof(*pMb->totalCoeff));
            pMb->qpY = (uint32_t)*qpY;
        }
#ifdef H264DEC_OMXDL
        /* if decoded flag > 1 -> mb has already been successfully decoded and
         * written to output -> do not write again */
        if(pMb->decoded > 1)
            return HANTRO_OK;

        h264bsdWriteMacroblock(currImage, data);
#else
        if(h264bsdMbPartPredMode(mbType) != PRED_MODE_INTER) {
            tmp = h264bsdIntraPrediction(pMb, pMbLayer, currImage, mbNum,
                constrainedIntraPredFlag, (uint8_t*)data);
            if(tmp != HANTRO_OK) return tmp;
				  }
        else {
            tmp = h264bsdInterPrediction(pMb, pMbLayer, dpb, mbNum,
                currImage, (uint8_t*)data);
            if(tmp != HANTRO_OK) 
							return tmp;
					}
#endif
    }

  return HANTRO_OK;
	}


#ifdef H264DEC_OMXDL
/*------------------------------------------------------------------------------

    Function: ProcessChromaResidual

        Functional description:
          Process the residual data of chroma with
          inverse quantization and inverse transform.

------------------------------------------------------------------------------*/
uint32_t ProcessChromaResidual(mbStorage_t *pMb, uint8_t *data, const uint8_t **pSrc ) {
    uint32_t i;
    uint32_t chromaQp;
    int16_t *pDc;
    int16_t dc[4 + 4] = {0,0,0,0,0,0,0,0};
    uint8_t *totalCoeff;
    OMXResult result;
    uint8_t *p;

    /* chroma DC processing. First chroma dc block is block with index 25 */
    chromaQp = h264bsdQpC[CLIP3(0, 51, (int32_t)pMb->qpY + pMb->chromaQpIndexOffset)];

    if(pMb->totalCoeff[25]) {
        pDc = dc;
        result = omxVCM4P10_TransformDequantChromaDCFromPair(
                pSrc,
                pDc,
                (int32_t)chromaQp);
        if(result != OMX_Sts_NoErr)
            return (HANTRO_NOK);
			}
    if(pMb->totalCoeff[26]) {
        pDc = dc+4;
        result = omxVCM4P10_TransformDequantChromaDCFromPair(
                pSrc, pDc,
                (int32_t)chromaQp);
        if(result != OMX_Sts_NoErr)
            return (HANTRO_NOK);
			}

    pDc = dc;
    totalCoeff = pMb->totalCoeff + 16;
    for(i=0; i < 8; i++, pDc++, totalCoeff++) {
      /* chroma prediction */
      if(*totalCoeff || *pDc) {
          p = data + chromaIndex[i];
          result = omxVCM4P10_DequantTransformResidualFromPairAndAdd(
                  pSrc,p, pDc,p, 8,8,
                  (int32_t)chromaQp,*totalCoeff);
          if(result != OMX_Sts_NoErr)
              return (HANTRO_NOK);
				}
			}

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: ProcessIntra16x16Residual

        Functional description:
          Process the residual data of luma with
          inverse quantization and inverse transform.

------------------------------------------------------------------------------*/
uint32_t ProcessIntra16x16Residual(mbStorage_t *pMb,uint8_t *data,
                              uint32_t constrainedIntraPred,
                              uint32_t intraChromaPredMode,
                              const uint8_t** pSrc,image_t *image) {
    uint32_t i;
    int16_t *pDc;
    int16_t dc[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    uint8_t *totalCoeff;
    OMXResult result;
    uint8_t *p;

    totalCoeff = pMb->totalCoeff;

    if(totalCoeff[24]) {
        pDc = dc;
        result = omxVCM4P10_TransformDequantLumaDCFromPair(
                    pSrc, pDc,
                    (int32_t)pMb->qpY);
        if(result != OMX_Sts_NoErr)
            return (HANTRO_NOK);
			}
    /* Intra 16x16 pred */
    if(h264bsdIntra16x16Prediction(pMb, data, image->luma,
                            image->width*16, constrainedIntraPred) != HANTRO_OK)
        return(HANTRO_NOK);
    for(i=0; i < 16; i++, totalCoeff++) {
        p = data + lumaIndex[i];
        pDc = &dc[dcCoeffIndex[i]];
        if(*totalCoeff || *pDc) {
            result = omxVCM4P10_DequantTransformResidualFromPairAndAdd(
                    pSrc,p,
                    pDc,p,
                    16,16,
                    (int32_t)pMb->qpY,*totalCoeff);
            if(result != OMX_Sts_NoErr)
                return (HANTRO_NOK);
        }
		  }

    if(h264bsdIntraChromaPrediction(pMb, data + 256,
                image,
                intraChromaPredMode,
                constrainedIntraPred) != HANTRO_OK)
        return(HANTRO_NOK);

    return HANTRO_OK;
	}

/*------------------------------------------------------------------------------

    Function: ProcessIntra4x4Residual

        Functional description:
          Process the residual data of luma with
          inverse quantization and inverse transform.

------------------------------------------------------------------------------*/
uint32_t ProcessIntra4x4Residual(mbStorage_t *pMb, uint8_t *data,
                            uint32_t constrainedIntraPred,
                            macroblockLayer_t *mbLayer,
                            const uint8_t **pSrc, image_t *image) {
    uint32_t i;
    uint8_t *totalCoeff;
    OMXResult result;
    uint8_t *p;

    totalCoeff = pMb->totalCoeff;

    for(i=0; i < 16; i++, totalCoeff++) {
        p = data + lumaIndex[i];
        if(h264bsdIntra4x4Prediction(pMb, p, mbLayer, image->luma,
                    image->width*16, constrainedIntraPred, i) != HANTRO_OK)
            return(HANTRO_NOK);

        if(*totalCoeff) {
            result = omxVCM4P10_DequantTransformResidualFromPairAndAdd(
                    pSrc, p, NULL, p,
                    16,                    16,
                    (int32_t)pMb->qpY,
                    *totalCoeff);
            if(result != OMX_Sts_NoErr)
                return (HANTRO_NOK);
        }
	    }

    if(h264bsdIntraChromaPrediction(pMb, data + 256,
                image,
                mbLayer->mbPred.intraChromaPredMode,
                constrainedIntraPred) != HANTRO_OK)
        return(HANTRO_NOK);

    return HANTRO_OK;
	}

#else /* H264DEC_OMXDL */

/*------------------------------------------------------------------------------

    Function: ProcessResidual

        Functional description:
          Process the residual data of one macroblock with
          inverse quantization and inverse transform.

------------------------------------------------------------------------------*/
uint32_t ProcessResidual(mbStorage_t *pMb, int32_t residualLevel[][16], uint32_t *coeffMap) {
    uint32_t i;
    uint32_t chromaQp;
    int32_t (*blockData)[16];
    int32_t (*blockDc)[16];
    int16_t *totalCoeff;
    int32_t *chromaDc;
    const uint32_t *dcCoeffIdx;


    ASSERT(pMb);
    ASSERT(residualLevel);

    /* set pointers to DC coefficient blocks */
    blockDc = residualLevel + 24;

    blockData = residualLevel;
    totalCoeff = pMb->totalCoeff;
    if(h264bsdMbPartPredMode(pMb->mbType) == PRED_MODE_INTRA16x16) {
        if(totalCoeff[24])
            h264bsdProcessLumaDc(*blockDc, pMb->qpY);
        dcCoeffIdx = dcCoeffIndex;

        for(i = 16; i--; blockData++, totalCoeff++, coeffMap++) {
            /* set dc coefficient of luma block */
            (*blockData)[0] = (*blockDc)[*dcCoeffIdx++];
            if((*blockData)[0] || *totalCoeff) {
                if(h264bsdProcessBlock(*blockData, pMb->qpY, 1, *coeffMap) !=
                    HANTRO_OK)
                    return(HANTRO_NOK);
            }
            else
                MARK_RESIDUAL_EMPTY(*blockData);
        }
		  }
    else {
        for(i = 16; i--; blockData++, totalCoeff++, coeffMap++) {
            if(*totalCoeff) {
                if(h264bsdProcessBlock(*blockData, pMb->qpY, 0, *coeffMap) != HANTRO_OK)
                    return(HANTRO_NOK);
				      }
            else
                MARK_RESIDUAL_EMPTY(*blockData);
        }
			}

    /* chroma DC processing. First chroma dc block is block with index 25 */
    chromaQp =
        h264bsdQpC[CLIP3(0, 51, (int32_t)pMb->qpY + pMb->chromaQpIndexOffset)];
    if(pMb->totalCoeff[25] || pMb->totalCoeff[26])
        h264bsdProcessChromaDc(residualLevel[25], chromaQp);
    chromaDc = residualLevel[25];
    for(i = 8; i--; blockData++, totalCoeff++, coeffMap++) {
        /* set dc coefficient of chroma block */
        (*blockData)[0] = *chromaDc++;
        if((*blockData)[0] || *totalCoeff) {
            if(h264bsdProcessBlock(*blockData, chromaQp, 1,*coeffMap) !=
                HANTRO_OK)
                return(HANTRO_NOK);
					}
        else
            MARK_RESIDUAL_EMPTY(*blockData);
    }

    return(HANTRO_OK);
	}
#endif /* H264DEC_OMXDL */

/*------------------------------------------------------------------------------

    Function: h264bsdSubMbPartMode

        Functional description:
          Returns the macroblock's sub-partition mode.

------------------------------------------------------------------------------*/
subMbPartMode_e h264bsdSubMbPartMode(subMbType_e subMbType) {

  ASSERT(subMbType < 4);

  return((subMbPartMode_e)subMbType);
	}

/*------------------------------------------------------------------------------

    Function name: h264bsdDecodeNalUnit

        Functional description:
            Decode NAL unit header information

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            pNalUnit        NAL unit header information is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid NAL unit header information

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeNalUnit(strmData_t *pStrmData, nalUnit_t *pNalUnit) {
  uint32_t tmp;


  ASSERT(pStrmData);
  ASSERT(pNalUnit);
  ASSERT(pStrmData->bitPosInWord == 0);

  /* forbidden_zero_bit (not checked to be zero, errors ignored) */
  tmp = h264bsdGetBits(pStrmData, 1);
  /* Assuming that NAL unit starts from byte boundary ­> don't have to check
   * following 7 bits for END_OF_STREAM */
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);

  tmp = h264bsdGetBits(pStrmData, 2);
  pNalUnit->nalRefIdc = tmp;

  tmp = h264bsdGetBits(pStrmData, 5);
  pNalUnit->nalUnitType = (nalUnitType_e)tmp;

  /* data partitioning NAL units not supported */
  if((tmp == 2) || (tmp == 3) || (tmp == 4))
    return(HANTRO_NOK);
		
  /* nal_ref_idc shall not be zero for these nal_unit_types */
  if(((tmp == NAL_SEQ_PARAM_SET) || (tmp == NAL_PIC_PARAM_SET) ||
    (tmp == NAL_CODED_SLICE_IDR)) && (pNalUnit->nalRefIdc == 0))
    return(HANTRO_NOK);

  /* nal_ref_idc shall be zero for these nal_unit_types */
  else if(((tmp == NAL_SEI) || (tmp == NAL_ACCESS_UNIT_DELIMITER) ||
           (tmp == NAL_END_OF_SEQUENCE) || (tmp == NAL_END_OF_STREAM) ||
           (tmp == NAL_FILLER_DATA)) && (pNalUnit->nalRefIdc != 0))
    return(HANTRO_NOK);


  return(HANTRO_OK);
	}



/* Following four tables indicate neighbours of each block of a macroblock.
 * First 16 values are for luma blocks, next 4 values for Cb and last 4
 * values for Cr. Elements of the table indicate to which macroblock the
 * neighbour block belongs and the index of the neighbour block in question.
 * Indexing of the blocks goes as follows
 *
 *          Y             Cb       Cr
 *      0  1  4  5      16 17    20 21
 *      2  3  6  7      18 19    22 23
 *      8  9 12 13
 *     10 11 14 15
 */

/* left neighbour for each block */
static const neighbour_t N_A_4x4B[24] = {
	{MB_A,5},{MB_CURR,0}, {MB_A,7},{MB_CURR,2},
	{MB_CURR,1}, {MB_CURR,4}, {MB_CURR,3}, {MB_CURR,6},
	{MB_A,13},{MB_CURR,8}, {MB_A,15},{MB_CURR,10},
	{MB_CURR,9}, {MB_CURR,12},{MB_CURR,11},{MB_CURR,14},
	{MB_A,17},{MB_CURR,16},{MB_A,19},{MB_CURR,18},
	{MB_A,21},{MB_CURR,20},{MB_A,23},{MB_CURR,22} };
/* above neighbour for each block */
static const neighbour_t N_B_4x4B[24] = {
	{MB_B,10},{MB_B,11},{MB_CURR,0}, {MB_CURR,1},
	{MB_B,14},{MB_B,15},{MB_CURR,4}, {MB_CURR,5},
	{MB_CURR,2}, {MB_CURR,3}, {MB_CURR,8}, {MB_CURR,9},
	{MB_CURR,6}, {MB_CURR,7}, {MB_CURR,12},{MB_CURR,13},
	{MB_B,18},{MB_B,19},{MB_CURR,16},{MB_CURR,17},
	{MB_B,22},{MB_B,23},{MB_CURR,20},{MB_CURR,21} };
/* above-right neighbour for each block */
static const neighbour_t N_C_4x4B[24] = {
	{MB_B,11},{MB_B,14},{MB_CURR,1}, {MB_NA,4},
	{MB_B,15},{MB_C,10},{MB_CURR,5}, {MB_NA,0},
	{MB_CURR,3}, {MB_CURR,6}, {MB_CURR,9}, {MB_NA,12},
	{MB_CURR,7}, {MB_NA,2},{MB_CURR,13},{MB_NA,8},
	{MB_B,19},{MB_C,18},{MB_CURR,17},{MB_NA,16},
	{MB_B,23},{MB_C,22},{MB_CURR,21},{MB_NA,20} };
/* above-left neighbour for each block */
static const neighbour_t N_D_4x4B[24] = {
	{MB_D,15},{MB_B,10},{MB_A,5},{MB_CURR,0},
	{MB_B,11},{MB_B,14},{MB_CURR,1}, {MB_CURR,4},
	{MB_A,7},{MB_CURR,2}, {MB_A,13},{MB_CURR,8},
	{MB_CURR,3}, {MB_CURR,6}, {MB_CURR,9}, {MB_CURR,12},
	{MB_D,19},{MB_B,18},{MB_A,17},{MB_CURR,16},
	{MB_D,23},{MB_B,22},{MB_A,21},{MB_CURR,20} };


/*------------------------------------------------------------------------------

    Function: h264bsdInitMbNeighbours

        Functional description:
            Initialize macroblock neighbours. Function sets neighbour
            macroblock pointers in macroblock structures to point to
            macroblocks on the left, above, above-right and above-left.
            Pointers are set NULL if the neighbour does not fit into the
            picture.

        Inputs:
            picWidth        width of the picture in macroblocks
            picSizeInMbs    no need to clarify

        Outputs:
            pMbStorage      neighbour pointers of each mbStorage structure
                            stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdInitMbNeighbours(mbStorage_t *pMbStorage, uint32_t picWidth, uint32_t picSizeInMbs) {
  uint32_t i, row, col;


  ASSERT(pMbStorage);
  ASSERT(picWidth);
  ASSERT(picWidth <= picSizeInMbs);
  ASSERT(((picSizeInMbs / picWidth) * picWidth) == picSizeInMbs);

  row = col=0;

  for(i=0; i < picSizeInMbs; i++) {

      if(col)
          pMbStorage[i].mbA = pMbStorage + i - 1;
      else
          pMbStorage[i].mbA = NULL;

      if(row)
          pMbStorage[i].mbB = pMbStorage + i - picWidth;
      else
          pMbStorage[i].mbB = NULL;

      if(row && (col < picWidth - 1))
          pMbStorage[i].mbC = pMbStorage + i - (picWidth - 1);
      else
          pMbStorage[i].mbC = NULL;

      if(row && col)
          pMbStorage[i].mbD = pMbStorage + i - (picWidth + 1);
      else
          pMbStorage[i].mbD = NULL;

      col++;
      if(col == picWidth) {
          col=0;
          row++;
      }
    }

	}

/*------------------------------------------------------------------------------

    Function: h264bsdGetNeighbourMb

        Functional description:
            Get pointer to neighbour macroblock.

        Inputs:
            pMb         pointer to macroblock structure of the macroblock
                        whose neighbour is wanted
            neighbour   indicates which neighbour is wanted

        Outputs:
            none

        Returns:
            pointer to neighbour macroblock
            NULL if not available

------------------------------------------------------------------------------*/
mbStorage_t* h264bsdGetNeighbourMb(mbStorage_t *pMb, neighbourMb_e neighbour) {


  ASSERT((neighbour <= MB_CURR) || (neighbour == MB_NA));

  if(neighbour == MB_A)
      return pMb->mbA;
  else if(neighbour == MB_B)
      return pMb->mbB;
  else if(neighbour == MB_C)
      return pMb->mbC;
  else if(neighbour == MB_D)
      return pMb->mbD;
  else if(neighbour == MB_CURR)
      return pMb;
  else
      return NULL;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdNeighbour4x4BlockA

        Functional description:
            Get left neighbour of the block. Function returns pointer to
            the table defined in the beginning of the file.

        Inputs:
            blockIndex  indicates the block whose neighbours are wanted

        Outputs:

        Returns:
            pointer to neighbour structure

------------------------------------------------------------------------------*/
const neighbour_t* h264bsdNeighbour4x4BlockA(uint32_t blockIndex) {

  ASSERT(blockIndex < 24);

  return(N_A_4x4B+blockIndex);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdNeighbour4x4BlockB

        Functional description:
            Get above neighbour of the block. Function returns pointer to
            the table defined in the beginning of the file.

        Inputs:
            blockIndex  indicates the block whose neighbours are wanted

        Outputs:

        Returns:
            pointer to neighbour structure

------------------------------------------------------------------------------*/
const neighbour_t* h264bsdNeighbour4x4BlockB(uint32_t blockIndex) {

  ASSERT(blockIndex < 24);

  return(N_B_4x4B+blockIndex);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdNeighbour4x4BlockC

        Functional description:
            Get above-right  neighbour of the block. Function returns pointer
            to the table defined in the beginning of the file.

        Inputs:
            blockIndex  indicates the block whose neighbours are wanted

        Outputs:

        Returns:
            pointer to neighbour structure

------------------------------------------------------------------------------*/
const neighbour_t* h264bsdNeighbour4x4BlockC(uint32_t blockIndex) {


  ASSERT(blockIndex < 24);

  return(N_C_4x4B+blockIndex);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdNeighbour4x4BlockD

        Functional description:
            Get above-left neighbour of the block. Function returns pointer to
            the table defined in the beginning of the file.

        Inputs:
            blockIndex  indicates the block whose neighbours are wanted

        Outputs:

        Returns:
            pointer to neighbour structure

------------------------------------------------------------------------------*/
const neighbour_t* h264bsdNeighbour4x4BlockD(uint32_t blockIndex) {

  ASSERT(blockIndex < 24);

  return(N_D_4x4B+blockIndex);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdIsNeighbourAvailable

        Functional description:
            Check if neighbour macroblock is available. Neighbour macroblock
            is considered available if it is within the picture and belongs
            to the same slice as the current macroblock.

        Inputs:
            pMb         pointer to the current macroblock
            pNeighbour  pointer to the neighbour macroblock

        Outputs:
            none

        Returns:
            TRUE    neighbour is available
            FALSE   neighbour is not available

------------------------------------------------------------------------------*/
uint32_t h264bsdIsNeighbourAvailable(mbStorage_t *pMb, mbStorage_t *pNeighbour) {

  if((!pNeighbour) || (pMb->sliceId != pNeighbour->sliceId))
      return(HANTRO_FALSE);
  else
      return(HANTRO_TRUE);
	}


/*------------------------------------------------------------------------------

    Function: h264bsdDecodePicOrderCnt

        Functional description:
            Compute picture order count for a picture. Function implements
            computation of all POC types (0, 1 and 2), type is obtained from
            sps. See standard for description of the POC types and how POC is
            computed for each type.

            Function returns the minimum of top field and bottom field pic
            order counts.

        Inputs:
            poc         pointer to previous results
            sps         pointer to sequence parameter set
            slicHeader  pointer to current slice header, frame number and
                        other params needed for POC computation
            pNalUnit    pointer to current NAL unit structrue, function needs
                        to know if this is an IDR picture and also if this is
                        a reference picture

        Outputs:
            poc         results stored here for computation of next POC

        Returns:
            picture order count

------------------------------------------------------------------------------*/
int32_t h264bsdDecodePicOrderCnt(pocStorage_t *poc, seqParamSet_t *sps, sliceHeader_t *pSliceHeader, nalUnit_t *pNalUnit) {
    uint32_t i;
    int32_t picOrderCnt;
    uint32_t frameNumOffset, absFrameNum, picOrderCntCycleCnt;
    uint32_t frameNumInPicOrderCntCycle;
    int32_t expectedDeltaPicOrderCntCycle;
    uint32_t containsMmco5;


    ASSERT(poc);
    ASSERT(sps);
    ASSERT(pSliceHeader);
    ASSERT(pNalUnit);
    ASSERT(sps->picOrderCntType <= 2);

#if 0
    /* JanSa: I don't think this is necessary, don't see any reason to
     * increment prevFrameNum one by one instead of one big increment.
     * However, standard specifies that this should be done -> if someone
     * figures out any case when the outcome would be different for step by
     * step increment, this part of the code should be enabled */

    /* if there was a gap in frame numbering and picOrderCntType is 1 or 2 ->
     * "compute" pic order counts for non-existing frames. These are not
     * actually computed, but process needs to be done to update the
     * prevFrameNum and prevFrameNumOffset */
    if( sps->picOrderCntType > 0 &&
         pSliceHeader->frameNum != poc->prevFrameNum &&
         pSliceHeader->frameNum != ((poc->prevFrameNum + 1) % sps->maxFrameNum)) {

        /* use variable i for unUsedShortTermFrameNum */
        i = (poc->prevFrameNum + 1) % sps->maxFrameNum;

        do {
            if(poc->prevFrameNum > i)
                frameNumOffset = poc->prevFrameNumOffset + sps->maxFrameNum;
            else
                frameNumOffset = poc->prevFrameNumOffset;

            poc->prevFrameNumOffset = frameNumOffset;
            poc->prevFrameNum = i;

            i = (i + 1) % sps->maxFrameNum;

					} while(i != pSliceHeader->frameNum);
			}
#endif

    /* check if current slice includes mmco equal to 5 */
    containsMmco5 = HANTRO_FALSE;
    if(pSliceHeader->decRefPicMarking.adaptiveRefPicMarkingModeFlag) {
        i=0;
        while(pSliceHeader->decRefPicMarking.operation[i].
            memoryManagementControlOperation) {
            if(pSliceHeader->decRefPicMarking.operation[i].
                memoryManagementControlOperation == 5) {
                containsMmco5 = HANTRO_TRUE;
                break;
            }
            i++;
        }
	    }
    switch(sps->picOrderCntType) {
		  case 0:
        /* set prevPicOrderCnt values for IDR frame */
        if(IS_IDR_NAL_UNIT(pNalUnit)) {
            poc->prevPicOrderCntMsb=0;
            poc->prevPicOrderCntLsb=0;
			    }

        /* compute picOrderCntMsb (stored in picOrderCnt variable) */
        if((pSliceHeader->picOrderCntLsb < poc->prevPicOrderCntLsb) &&
            ((poc->prevPicOrderCntLsb - pSliceHeader->picOrderCntLsb) >=
             sps->maxPicOrderCntLsb/2)) {
            picOrderCnt = poc->prevPicOrderCntMsb +
                (int32_t)sps->maxPicOrderCntLsb;
				  }
        else if((pSliceHeader->picOrderCntLsb > poc->prevPicOrderCntLsb) &&
            ((pSliceHeader->picOrderCntLsb - poc->prevPicOrderCntLsb) >
             sps->maxPicOrderCntLsb/2)) {
            picOrderCnt = poc->prevPicOrderCntMsb -
                (int32_t)sps->maxPicOrderCntLsb;
					}
        else
            picOrderCnt = poc->prevPicOrderCntMsb;

        /* standard specifies that prevPicOrderCntMsb is from previous
         * rererence frame -> replace old value only if current frame is
         * rererence frame */
        if(pNalUnit->nalRefIdc)
            poc->prevPicOrderCntMsb = picOrderCnt;

        /* compute top field order cnt (stored in picOrderCnt) */
        picOrderCnt += (int32_t)pSliceHeader->picOrderCntLsb;

        /* if delta for bottom field is negative -> bottom will be the
         * minimum pic order count */
        if(pSliceHeader->deltaPicOrderCntBottom < 0)
            picOrderCnt += pSliceHeader->deltaPicOrderCntBottom;

        /* standard specifies that prevPicOrderCntLsb is from previous
         * rererence frame -> replace old value only if current frame is
         * rererence frame */
        if(pNalUnit->nalRefIdc) {
            /* if current frame contains mmco5 -> modify values to be
             * stored */
            if(containsMmco5) {
                poc->prevPicOrderCntMsb=0;
                /* prevPicOrderCntLsb should be the top field picOrderCnt
                 * if previous frame included mmco5. Top field picOrderCnt
                 * for frames containing mmco5 is obtained by subtracting
                 * the picOrderCnt from original top field order count ->
                 * value is zero if top field was the minimum, i.e. delta
                 * for bottom was positive, otherwise value is
                 * -deltaPicOrderCntBottom */
                if(pSliceHeader->deltaPicOrderCntBottom < 0)
                    poc->prevPicOrderCntLsb =
                        (uint32_t)(-pSliceHeader->deltaPicOrderCntBottom);
                else
                    poc->prevPicOrderCntLsb=0;
                picOrderCnt=0;
            }
            else {
                poc->prevPicOrderCntLsb = pSliceHeader->picOrderCntLsb;
            }
        }

        break;

			case 1:
        /* step 1 (in the description in the standard) */
        if(IS_IDR_NAL_UNIT(pNalUnit))
            frameNumOffset=0;
        else if(poc->prevFrameNum > pSliceHeader->frameNum)
            frameNumOffset = poc->prevFrameNumOffset + sps->maxFrameNum;
        else
            frameNumOffset = poc->prevFrameNumOffset;

        /* step 2 */
        if(sps->numRefFramesInPicOrderCntCycle)
            absFrameNum = frameNumOffset + pSliceHeader->frameNum;
        else
            absFrameNum=0;

        if(pNalUnit->nalRefIdc == 0 && absFrameNum > 0)
            absFrameNum -= 1;

        /* step 3 */
        if(absFrameNum > 0) {
            picOrderCntCycleCnt =
                (absFrameNum - 1)/sps->numRefFramesInPicOrderCntCycle;
            frameNumInPicOrderCntCycle =
                (absFrameNum - 1)%sps->numRefFramesInPicOrderCntCycle;
					}

        /* step 4 */
        expectedDeltaPicOrderCntCycle=0;
        for(i=0; i < sps->numRefFramesInPicOrderCntCycle; i++)
            expectedDeltaPicOrderCntCycle += sps->offsetForRefFrame[i];

        /* step 5 (picOrderCnt used to store expectedPicOrderCnt) */
        /*lint -esym(644,picOrderCntCycleCnt) always initialized */
        /*lint -esym(644,frameNumInPicOrderCntCycle) always initialized */
        if(absFrameNum > 0) {
            picOrderCnt =
                (int32_t)picOrderCntCycleCnt * expectedDeltaPicOrderCntCycle;
            for(i=0; i <= frameNumInPicOrderCntCycle; i++)
                picOrderCnt += sps->offsetForRefFrame[i];
					}
        else
            picOrderCnt=0;

        if(pNalUnit->nalRefIdc == 0)
            picOrderCnt += sps->offsetForNonRefPic;

        /* step 6 (picOrderCnt is top field order cnt if delta for bottom
         * is positive, otherwise it is bottom field order cnt) */
        picOrderCnt += pSliceHeader->deltaPicOrderCnt[0];

        if((sps->offsetForTopToBottomField +
                pSliceHeader->deltaPicOrderCnt[1]) < 0 ) {
            picOrderCnt += sps->offsetForTopToBottomField +
                pSliceHeader->deltaPicOrderCnt[1];
					}

        /* if current picture contains mmco5 -> set prevFrameNumOffset and
         * prevFrameNum to 0 for computation of picOrderCnt of next
         * frame, otherwise store frameNum and frameNumOffset to poc
         * structure */
        if(!containsMmco5) {
            poc->prevFrameNumOffset = frameNumOffset;
            poc->prevFrameNum = pSliceHeader->frameNum;
        }
        else {
            poc->prevFrameNumOffset=0;
            poc->prevFrameNum=0;
            picOrderCnt=0;
        }
        break;

			default: /* case 2 */
        /* derive frameNumOffset */
        if(IS_IDR_NAL_UNIT(pNalUnit))
            frameNumOffset=0;
        else if(poc->prevFrameNum > pSliceHeader->frameNum)
            frameNumOffset = poc->prevFrameNumOffset + sps->maxFrameNum;
        else
            frameNumOffset = poc->prevFrameNumOffset;

        /* derive picOrderCnt (type 2 has same value for top and bottom
         * field order cnts) */
        if(IS_IDR_NAL_UNIT(pNalUnit))
            picOrderCnt=0;
        else if(pNalUnit->nalRefIdc == 0)
            picOrderCnt =
                2 * (int32_t)(frameNumOffset + pSliceHeader->frameNum) - 1;
        else
            picOrderCnt =
                2 * (int32_t)(frameNumOffset + pSliceHeader->frameNum);

        /* if current picture contains mmco5 -> set prevFrameNumOffset and
         * prevFrameNum to 0 for computation of picOrderCnt of next
         * frame, otherwise store frameNum and frameNumOffset to poc
         * structure */
        if(!containsMmco5) {
            poc->prevFrameNumOffset = frameNumOffset;
            poc->prevFrameNum = pSliceHeader->frameNum;
					}
        else {
          poc->prevFrameNumOffset=0;
          poc->prevFrameNum=0;
          picOrderCnt=0;
	        }
        break;

    }

  return picOrderCnt;
	}



/* lookup table for ceil(log2(numSliceGroups)), i.e. number of bits needed to
 * represent range [0, numSliceGroups)
 *
 * NOTE: if MAX_NUM_SLICE_GROUPS is higher than 8 this table has to be resized
 * accordingly */
static const uint32_t CeilLog2NumSliceGroups[8] = {1, 1, 2, 2, 3, 3, 3, 3};


/*------------------------------------------------------------------------------

    Function name: h264bsdDecodePicParamSet

        Functional description:
            Decode picture parameter set information from the stream.

            Function allocates memory for
                - run lengths if slice group map type is 0
                - top-left and bottom-right arrays if map type is 2
                - for slice group ids if map type is 6

            Validity of some of the slice group mapping information depends
            on the image dimensions which are not known here. Therefore the
            validity has to be checked afterwards, currently in the parameter
            set activation phase.

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            pPicParamSet    decoded information is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      failure, invalid information or end of stream
            MEMORY_ALLOCATION_ERROR for memory allocation failure


	https://crosvm.dev/doc/cros_codecs/decoders/h264/parser/struct.Pps.html
------------------------------------------------------------------------------*/
uint32_t h264bsdDecodePicParamSet(strmData_t *pStrmData, picParamSet_t *pPicParamSet, uint32_t profile) {
  uint32_t tmp, i, value;
  int32_t itmp;


  ASSERT(pStrmData);
  ASSERT(pPicParamSet);


  memset(pPicParamSet, 0, sizeof(picParamSet_t));

  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pPicParamSet->picParameterSetId);
  if(tmp != HANTRO_OK)
    return tmp;
  if(pPicParamSet->picParameterSetId >= MAX_NUM_PIC_PARAM_SETS) {
    EPRINT("pic_parameter_set_id");
    return(HANTRO_NOK);
    }

  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pPicParamSet->seqParameterSetId);
  if(tmp != HANTRO_OK)
    return tmp;
  if(pPicParamSet->seqParameterSetId >= MAX_NUM_SEQ_PARAM_SETS) {
    EPRINT("seq_param_set_id");
    return(HANTRO_NOK);
    }

  /* entropy_coding_mode_flag, shall be 0 for baseline profile */
	tmp = h264bsdGetBits(pStrmData, 1);
	if(1 /*seqParamSet.profileIdc==100*/) {
		}
	else {
		if(tmp) {
			EPRINT("entropy_coding_mode_flag");
			return(HANTRO_NOK);
			}
		}

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pPicParamSet->picOrderPresentFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  /* num_slice_groups_minus1 */
  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
  if(tmp != HANTRO_OK)
    return tmp;
  pPicParamSet->numSliceGroups = value + 1;
  if(pPicParamSet->numSliceGroups > MAX_NUM_SLICE_GROUPS) {
    EPRINT("num_slice_groups_minus1");
    return(HANTRO_NOK);
		}

  /* decode slice group mapping information if more than one slice groups */
  if(pPicParamSet->numSliceGroups > 1) {
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pPicParamSet->sliceGroupMapType);
    if(tmp != HANTRO_OK)
      return tmp;
    if(pPicParamSet->sliceGroupMapType > 6) {
      EPRINT("slice_group_map_type");
      return(HANTRO_NOK);
      }

    if(pPicParamSet->sliceGroupMapType == 0) {
      ALLOCATE(pPicParamSet->runLength,pPicParamSet->numSliceGroups, uint32_t);
      if(!pPicParamSet->runLength)
        return(MEMORY_ALLOCATION_ERROR);
      for(i=0; i < pPicParamSet->numSliceGroups; i++) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK)
            return tmp;
        pPicParamSet->runLength[i] = value+1;
        /* param values checked in CheckPps() */
        }
	    }
    else if(pPicParamSet->sliceGroupMapType == 2) {
      ALLOCATE(pPicParamSet->topLeft,pPicParamSet->numSliceGroups - 1, uint32_t);
      ALLOCATE(pPicParamSet->bottomRight,pPicParamSet->numSliceGroups - 1, uint32_t);
      if(!pPicParamSet->topLeft || !pPicParamSet->bottomRight)
        return(MEMORY_ALLOCATION_ERROR);
      for(i=0; i < pPicParamSet->numSliceGroups - 1; i++) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK)
          return tmp;
        pPicParamSet->topLeft[i] = value;
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK)
          return tmp;
        pPicParamSet->bottomRight[i] = value;
        /* param values checked in CheckPps() */
        }
			}
    else if((pPicParamSet->sliceGroupMapType == 3) ||
            (pPicParamSet->sliceGroupMapType == 4) ||
            (pPicParamSet->sliceGroupMapType == 5)) {
      tmp = h264bsdGetBits(pStrmData, 1);
      if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
      pPicParamSet->sliceGroupChangeDirectionFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;
      tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
      if(tmp != HANTRO_OK)
        return tmp;
      pPicParamSet->sliceGroupChangeRate = value + 1;
      /* param value checked in CheckPps() */
			}
    else if(pPicParamSet->sliceGroupMapType == 6) {
      tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
      if(tmp != HANTRO_OK)
        return tmp;
      pPicParamSet->picSizeInMapUnits = value + 1;

      ALLOCATE(pPicParamSet->sliceGroupId,pPicParamSet->picSizeInMapUnits, uint32_t);
      if(!pPicParamSet->sliceGroupId)
        return(MEMORY_ALLOCATION_ERROR);

      /* determine number of bits needed to represent range [0, numSliceGroups) */
      tmp = CeilLog2NumSliceGroups[pPicParamSet->numSliceGroups-1];

      for(i=0; i < pPicParamSet->picSizeInMapUnits; i++) {
        pPicParamSet->sliceGroupId[i] = h264bsdGetBits(pStrmData, tmp);
        if( pPicParamSet->sliceGroupId[i] >= pPicParamSet->numSliceGroups ) {
          EPRINT("slice_group_id");
          return(HANTRO_NOK);
          }
        }
      }
		}

  /* num_ref_idx_l0_active_minus1 */
  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
  if(tmp != HANTRO_OK)
    return tmp;
  if(value > 31) {
    EPRINT("num_ref_idx_l0_active_minus1");
    return(HANTRO_NOK);
    }
  pPicParamSet->numRefIdxL0Active = value + 1;

  /* num_ref_idx_l1_active_minus1 */
  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
  if(tmp != HANTRO_OK)
    return tmp;
  if(value > 31) {
    EPRINT("num_ref_idx_l1_active_minus1");
    return(HANTRO_NOK);
    }

  /* weighted_pred_flag, this shall be 0 for baseline profile */
  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp) {
    EPRINT("weighted_pred_flag");
    return(HANTRO_NOK);
		}

  /* weighted_bipred_idc */
  tmp = h264bsdGetBits(pStrmData, 2);
  if(tmp > 2) {
    EPRINT("weighted_bipred_idc");
    return(HANTRO_NOK);
		}

  /* pic_init_qp_minus26 */
  tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
  if(tmp != HANTRO_OK)
    return tmp;
  if((itmp < -26) || (itmp > 25)) {
    EPRINT("pic_init_qp_minus26");
    return(HANTRO_NOK);
    }
  pPicParamSet->picInitQp = (uint32_t)(itmp + 26);

  /* pic_init_qs_minus26 */
  tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
  if(tmp != HANTRO_OK)
    return tmp;
  if((itmp < -26) || (itmp > 25)) {
    EPRINT("pic_init_qs_minus26");
    return(HANTRO_NOK);
		}

  tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
  if(tmp != HANTRO_OK)
    return tmp;
  if((itmp < -12) || (itmp > 12)) {
    EPRINT("chroma_qp_index_offset");
    return(HANTRO_NOK);
		}
  pPicParamSet->chromaQpIndexOffset = itmp;

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  pPicParamSet->deblockingFilterControlPresentFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  pPicParamSet->constrainedIntraPredFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  pPicParamSet->redundantPicCntPresentFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

	// transform_8x8_mode_flag
  tmp = h264bsdGetBits(pStrmData, 1);
  DEBUGP2("Transform 8x8 %d", tmp);

	// pic_scaling_matrix_present_flag
  tmp = h264bsdGetBits(pStrmData, 1);
	if(tmp) {
		if(profile==100) {
		// FARE matrice come di là!! scaling 4x4 ecc
			}
		}
	else {
		}


	if(profile==66)
		goto skippa; 

	// second_chroma_qp_index_offset
  tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
  if(tmp != HANTRO_OK)
    return tmp;

skippa:
  tmp = h264bsdRbspTrailingBits(pStrmData);

  /* ignore possible errors in trailing bits of parameters sets */
  return(HANTRO_OK);
	}




/* Luma fractional-sample positions
 *
 *  G a b c H
 *  d e f g
 *  h i j k m
 *  n p q r
 *  M   s   N
 *
 *  G, H, M and N are integer sample positions
 *  a-s are fractional samples that need to be interpolated.
 */
#ifndef H264DEC_OMXDL
static const uint32_t lumaFracPos[4][4] = {
  /* G  d  h  n    a  e  i  p    b  f  j   q     c   g   k   r */
		{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14, 15}};
#endif /* H264DEC_OMXDL */

/* clipping table, defined in h264bsd_intra_prediction.c */
extern const uint8_t h264bsdClip[];


#ifndef H264DEC_OMXDL
/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateChromaHor

        Functional description:
          This function performs chroma interpolation in horizontal direction.
          Overfilling is done only if needed. Reference image (pRef) is
          read at correct position and the predicted part is written to
          macroblock's chrominance (predPartChroma)
        Inputs:
          pRef              pointer to reference frame Cb top-left corner
          x0                integer x-coordinate for prediction
          y0                integer y-coordinate for prediction
          width             width of the reference frame chrominance in pixels
          height            height of the reference frame chrominance in pixels
          xFrac             horizontal fraction for prediction in 1/8 pixels
          chromaPartWidth   width of the predicted part in pixels
          chromaPartHeight  height of the predicted part in pixels
        Outputs:
          predPartChroma    pointer where predicted part is written

------------------------------------------------------------------------------*/
#ifndef H264DEC_ARM11
void h264bsdInterpolateChromaHor(uint8_t *pRef, uint8_t *predPartChroma,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t xFrac,
  uint32_t chromaPartWidth,uint32_t chromaPartHeight) {
    uint32_t x, y, tmp1, tmp2, tmp3, tmp4, c, val;
    uint8_t *ptrA, *cbr;
    uint32_t comp;
    uint8_t block[9*8*2];


    ASSERT(predPartChroma);
    ASSERT(chromaPartWidth);
    ASSERT(chromaPartHeight);
    ASSERT(xFrac < 8);
    ASSERT(pRef);

    if((x0 < 0) || ((uint32_t)x0+chromaPartWidth+1 > width) ||
        (y0 < 0) || ((uint32_t)y0+chromaPartHeight > height)) {
        h264bsdFillBlock(pRef, block, x0, y0, width, height,
            chromaPartWidth + 1, chromaPartHeight, chromaPartWidth + 1);
        pRef += width * height;
        h264bsdFillBlock(pRef, block + (chromaPartWidth+1)*chromaPartHeight,
            x0, y0, width, height, chromaPartWidth + 1,
            chromaPartHeight, chromaPartWidth + 1);

        pRef = block;
        x0=0;
        y0=0;
        width = chromaPartWidth+1;
        height = chromaPartHeight;
    }

    val = 8 - xFrac;

    for(comp=0; comp <= 1; comp++) {

        ptrA = pRef + (comp * height + (uint32_t)y0) * width + x0;
        cbr = predPartChroma + comp * 8 * 8;

        /* 2x2 pels per iteration bilinear horizontal interpolation */
        for(y = (chromaPartHeight >> 1); y; y--) {
            for(x = (chromaPartWidth >> 1); x; x--) {
                tmp1 = ptrA[width];
                tmp2 = *ptrA++;
                tmp3 = ptrA[width];
                tmp4 = *ptrA++;
                c = ((val * tmp1 + xFrac * tmp3) << 3) + 32;
                c >>= 6;
                cbr[8] = (uint8_t)c;
                c = ((val * tmp2 + xFrac * tmp4) << 3) + 32;
                c >>= 6;
                *cbr++ = (uint8_t)c;
                tmp1 = ptrA[width];
                tmp2 = *ptrA;
                c = ((val * tmp3 + xFrac * tmp1) << 3) + 32;
                c >>= 6;
                cbr[8] = (uint8_t)c;
                c = ((val * tmp4 + xFrac * tmp2) << 3) + 32;
                c >>= 6;
                *cbr++ = (uint8_t)c;
            }
            cbr += 2*8 - chromaPartWidth;
            ptrA += 2*width - chromaPartWidth;
        }
    }

	}

/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateChromaVer

        Functional description:
          This function performs chroma interpolation in vertical direction.
          Overfilling is done only if needed. Reference image (pRef) is
          read at correct position and the predicted part is written to
          macroblock's chrominance (predPartChroma)

------------------------------------------------------------------------------*/
void h264bsdInterpolateChromaVer(uint8_t *pRef, uint8_t *predPartChroma,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t yFrac,
  uint32_t chromaPartWidth,uint32_t chromaPartHeight) {
    uint32_t x, y, tmp1, tmp2, tmp3, c, val;
    uint8_t *ptrA, *cbr;
    uint32_t comp;
    uint8_t block[9*8*2];


    ASSERT(predPartChroma);
    ASSERT(chromaPartWidth);
    ASSERT(chromaPartHeight);
    ASSERT(yFrac < 8);
    ASSERT(pRef);

    if((x0 < 0) || ((uint32_t)x0+chromaPartWidth > width) ||
        (y0 < 0) || ((uint32_t)y0+chromaPartHeight+1 > height)) {
        h264bsdFillBlock(pRef, block, x0, y0, width, height, chromaPartWidth,
            chromaPartHeight + 1, chromaPartWidth);
        pRef += width * height;
        h264bsdFillBlock(pRef, block + chromaPartWidth*(chromaPartHeight+1),
            x0, y0, width, height, chromaPartWidth,
            chromaPartHeight + 1, chromaPartWidth);

        pRef = block;
        x0=0;
        y0=0;
        width = chromaPartWidth;
        height = chromaPartHeight+1;
    }

    val = 8 - yFrac;

    for(comp=0; comp <= 1; comp++) {

        ptrA = pRef + (comp * height + (uint32_t)y0) * width + x0;
        cbr = predPartChroma + comp * 8 * 8;

        /* 2x2 pels per iteration bilinear vertical interpolation */
        for(y = (chromaPartHeight >> 1); y; y--) {
            for(x = (chromaPartWidth >> 1); x; x--) {
                tmp3 = ptrA[width*2];
                tmp2 = ptrA[width];
                tmp1 = *ptrA++;
                c = ((val * tmp2 + yFrac * tmp3) << 3) + 32;
                c >>= 6;
                cbr[8] = (uint8_t)c;
                c = ((val * tmp1 + yFrac * tmp2) << 3) + 32;
                c >>= 6;
                *cbr++ = (uint8_t)c;
                tmp3 = ptrA[width*2];
                tmp2 = ptrA[width];
                tmp1 = *ptrA++;
                c = ((val * tmp2 + yFrac * tmp3) << 3) + 32;
                c >>= 6;
                cbr[8] = (uint8_t)c;
                c = ((val * tmp1 + yFrac * tmp2) << 3) + 32;
                c >>= 6;
                *cbr++ = (uint8_t)c;
            }
            cbr += 2*8 - chromaPartWidth;
            ptrA += 2*width - chromaPartWidth;
        }
    }

	}
#endif

/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateChromaHorVer

        Functional description:
          This function performs chroma interpolation in horizontal and
          vertical direction. Overfilling is done only if needed. Reference
          image (ref) is read at correct position and the predicted part
          is written to macroblock's chrominance (predPartChroma)

------------------------------------------------------------------------------*/
void h264bsdInterpolateChromaHorVer(uint8_t *ref, uint8_t *predPartChroma,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t xFrac,uint32_t yFrac,
  uint32_t chromaPartWidth,uint32_t chromaPartHeight) {
    uint8_t block[9*9*2];
    uint32_t x, y, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, valX, valY, plus32 = 32;
    uint32_t comp;
    uint8_t *ptrA, *cbr;



    ASSERT(predPartChroma);
    ASSERT(chromaPartWidth);
    ASSERT(chromaPartHeight);
    ASSERT(xFrac < 8);
    ASSERT(yFrac < 8);
    ASSERT(ref);

    if((x0 < 0) || ((uint32_t)x0+chromaPartWidth+1 > width) ||
        (y0 < 0) || ((uint32_t)y0+chromaPartHeight+1 > height)) {
        h264bsdFillBlock(ref, block, x0, y0, width, height,
            chromaPartWidth + 1, chromaPartHeight + 1, chromaPartWidth + 1);
        ref += width * height;
        h264bsdFillBlock(ref, block + (chromaPartWidth+1)*(chromaPartHeight+1),
            x0, y0, width, height, chromaPartWidth + 1,
            chromaPartHeight + 1, chromaPartWidth + 1);

        ref = block;
        x0=0;
        y0=0;
        width = chromaPartWidth+1;
        height = chromaPartHeight+1;
		  }

    valX = 8 - xFrac;
    valY = 8 - yFrac;

    for(comp=0; comp <= 1; comp++) {

        ptrA = ref + (comp * height + (uint32_t)y0) * width + x0;
        cbr = predPartChroma + comp * 8 * 8;

        /* 2x2 pels per iteration
         * bilinear vertical and horizontal interpolation */
        for(y = (chromaPartHeight >> 1); y; y--) {
            tmp1 = *ptrA;
            tmp3 = ptrA[width];
            tmp5 = ptrA[width*2];
            tmp1 *= valY;
            tmp1 += tmp3 * yFrac;
            tmp3 *= valY;
            tmp3 += tmp5 * yFrac;
            for(x = (chromaPartWidth >> 1); x; x--) {
                tmp2 = *++ptrA;
                tmp4 = ptrA[width];
                tmp6 = ptrA[width*2];
                tmp2 *= valY;
                tmp2 += tmp4 * yFrac;
                tmp4 *= valY;
                tmp4 += tmp6 * yFrac;
                tmp1 = tmp1 * valX + plus32;
                tmp3 = tmp3 * valX + plus32;
                tmp1 += tmp2 * xFrac;
                tmp1 >>= 6;
                tmp3 += tmp4 * xFrac;
                tmp3 >>= 6;
                cbr[8] = (uint8_t)tmp3;
                *cbr++ = (uint8_t)tmp1;

                tmp1 = *++ptrA;
                tmp3 = ptrA[width];
                tmp5 = ptrA[width*2];
                tmp1 *= valY;
                tmp1 += tmp3 * yFrac;
                tmp3 *= valY;
                tmp3 += tmp5 * yFrac;
                tmp2 = tmp2 * valX + plus32;
                tmp4 = tmp4 * valX + plus32;
                tmp2 += tmp1 * xFrac;
                tmp2 >>= 6;
                tmp4 += tmp3 * xFrac;
                tmp4 >>= 6;
                cbr[8] = (uint8_t)tmp4;
                *cbr++ = (uint8_t)tmp2;
            }
            cbr += 2*8 - chromaPartWidth;
            ptrA += 2*width - chromaPartWidth;
        }
    }

	}

/*------------------------------------------------------------------------------

    Function: PredictChroma

        Functional description:
          Top level chroma prediction function that calls the appropriate
          interpolation function. The output is written to macroblock array.

------------------------------------------------------------------------------*/
static void PredictChroma(uint8_t *mbPartChroma,
													uint32_t xAL,uint32_t yAL,
  uint32_t partWidth,uint32_t partHeight,
  mv_t *mv,image_t *refPic) {
    uint32_t xFrac, yFrac, width, height, chromaPartWidth, chromaPartHeight;
    int32_t xInt, yInt;
    uint8_t *ref;


    ASSERT(mv);
    ASSERT(refPic);
    ASSERT(refPic->data);
    ASSERT(refPic->width);
    ASSERT(refPic->height);

    width  = 8 * refPic->width;
    height = 8 * refPic->height;

    xInt = (xAL >> 1) + (mv->hor >> 3);
    yInt = (yAL >> 1) + (mv->ver >> 3);
    xFrac = mv->hor & 0x7;
    yFrac = mv->ver & 0x7;

    chromaPartWidth  = partWidth >> 1;
    chromaPartHeight = partHeight >> 1;
    ref = refPic->data + 256 * refPic->width * refPic->height;

    if(xFrac && yFrac) {
        h264bsdInterpolateChromaHorVer(ref, mbPartChroma, xInt, yInt, width,
                height, xFrac, yFrac, chromaPartWidth, chromaPartHeight);
			}
    else if(xFrac) {
        h264bsdInterpolateChromaHor(ref, mbPartChroma, xInt, yInt, width,
                height, xFrac, chromaPartWidth, chromaPartHeight);
			}
    else if(yFrac) {
        h264bsdInterpolateChromaVer(ref, mbPartChroma, xInt, yInt, width,
                height, yFrac, chromaPartWidth, chromaPartHeight);
		  }
    else {
        h264bsdFillBlock(ref, mbPartChroma, xInt, yInt, width, height,
            chromaPartWidth, chromaPartHeight, 8);
        ref += width * height;
        h264bsdFillBlock(ref, mbPartChroma + 8*8, xInt, yInt, width, height,
            chromaPartWidth, chromaPartHeight, 8);
    }

	}


/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateVerHalf

        Functional description:
          Function to perform vertical interpolation of pixel position 'h'
          for a block. Overfilling is done only if needed. Reference
          image (ref) is read at correct position and the predicted part
          is written to macroblock array (mb)

------------------------------------------------------------------------------*/
#ifndef H264DEC_ARM11
void h264bsdInterpolateVerHalf(uint8_t *ref, uint8_t *mb,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t partWidth,uint32_t partHeight) {
    uint32_t p1[21*21/4+1];
    uint32_t i, j;
    int32_t tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    uint8_t *ptrC, *ptrV;
    const uint8_t *clp = h264bsdClip + 512;

    
    ASSERT(ref);
    ASSERT(mb);

    if((x0 < 0) || ((uint32_t)x0+partWidth > width) ||
        (y0 < 0) || ((uint32_t)y0+partHeight+5 > height)) {
        h264bsdFillBlock(ref, (uint8_t*)p1, x0, y0, width, height,
                partWidth, partHeight+5, partWidth);

        x0=0;
        y0=0;
        ref = (uint8_t*)p1;
        width = partWidth;
	    }

    ref += (uint32_t)y0 * width + (uint32_t)x0;

    ptrC = ref + width;
    ptrV = ptrC + 5*width;

    /* 4 pixels per iteration, interpolate using 5 vertical samples */
    for(i = (partHeight >> 2); i; i--) {
        /* h1 = (16 + A + 16(G+M) + 4(G+M) - 4(C+R) - (C+R) + T) >> 5 */
        for(j = partWidth; j; j--) {
            tmp4 = ptrV[-(int32_t)width*2];
            tmp5 = ptrV[-(int32_t)width];
            tmp1 = ptrV[width];
            tmp2 = ptrV[width*2];
            tmp6 = *ptrV++;

            tmp7 = tmp4 + tmp1;
            tmp2 -= (tmp7 << 2);
            tmp2 -= tmp7;
            tmp2 += 16;
            tmp7 = tmp5 + tmp6;
            tmp3 = ptrC[width*2];
            tmp2 += (tmp7 << 4);
            tmp2 += (tmp7 << 2);
            tmp2 += tmp3;
            tmp2 = clp[tmp2>>5];
            tmp1 += 16;
            mb[48] = (uint8_t)tmp2;

            tmp7 = tmp3 + tmp6;
            tmp1 -= (tmp7 << 2);
            tmp1 -= tmp7;
            tmp7 = tmp4 + tmp5;
            tmp2 = ptrC[width];
            tmp1 += (tmp7 << 4);
            tmp1 += (tmp7 << 2);
            tmp1 += tmp2;
            tmp1 = clp[tmp1>>5];
            tmp6 += 16;
            mb[32] = (uint8_t)tmp1;

            tmp7 = tmp2 + tmp5;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp7 = tmp4 + tmp3;
            tmp1 = *ptrC;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp6 += tmp1;
            tmp6 = clp[tmp6>>5];
            tmp5 += 16;
            mb[16] = (uint8_t)tmp6;

            tmp1 += tmp4;
            tmp5 -= (tmp1 << 2);
            tmp5 -= tmp1;
            tmp3 += tmp2;
            tmp6 = ptrC[-(int32_t)width];
            tmp5 += (tmp3 << 4);
            tmp5 += (tmp3 << 2);
            tmp5 += tmp6;
            tmp5 = clp[tmp5>>5];
            *mb++ = (uint8_t)tmp5;
            ptrC++;
        }
        ptrC += 4*width - partWidth;
        ptrV += 4*width - partWidth;
        mb += 4*16 - partWidth;
    }

	}

/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateVerQuarter

        Functional description:
          Function to perform vertical interpolation of pixel position 'd'
          or 'n' for a block. Overfilling is done only if needed. Reference
          image (ref) is read at correct position and the predicted part
          is written to macroblock array (mb)

------------------------------------------------------------------------------*/
void h264bsdInterpolateVerQuarter(uint8_t *ref, uint8_t *mb,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t partWidth,uint32_t partHeight,
  uint32_t verOffset)    /* 0 for pixel d, 1 for pixel n */
{
    uint32_t p1[21*21/4+1];
    uint32_t i, j;
    int32_t tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    uint8_t *ptrC, *ptrV, *ptrInt;
    const uint8_t *clp = h264bsdClip + 512;

    
    ASSERT(ref);
    ASSERT(mb);

    if((x0 < 0) || ((uint32_t)x0+partWidth > width) ||
        (y0 < 0) || ((uint32_t)y0+partHeight+5 > height)) {
        h264bsdFillBlock(ref, (uint8_t*)p1, x0, y0, width, height,
                partWidth, partHeight+5, partWidth);

        x0=0;
        y0=0;
        ref = (uint8_t*)p1;
        width = partWidth;
    }

    ref += (uint32_t)y0 * width + (uint32_t)x0;

    ptrC = ref + width;
    ptrV = ptrC + 5*width;

    /* Pointer to integer sample position, either M or R */
    ptrInt = ptrC + (2+verOffset)*width;

    /* 4 pixels per iteration
     * interpolate using 5 vertical samples and average between
     * interpolated value and integer sample value */
    for(i = (partHeight >> 2); i; i--) {
        /* h1 = (16 + A + 16(G+M) + 4(G+M) - 4(C+R) - (C+R) + T) >> 5 */
        for(j = partWidth; j; j--) {
            tmp4 = ptrV[-(int32_t)width*2];
            tmp5 = ptrV[-(int32_t)width];
            tmp1 = ptrV[width];
            tmp2 = ptrV[width*2];
            tmp6 = *ptrV++;

            tmp7 = tmp4 + tmp1;
            tmp2 -= (tmp7 << 2);
            tmp2 -= tmp7;
            tmp2 += 16;
            tmp7 = tmp5 + tmp6;
            tmp3 = ptrC[width*2];
            tmp2 += (tmp7 << 4);
            tmp2 += (tmp7 << 2);
            tmp2 += tmp3;
            tmp2 = clp[tmp2>>5];
            tmp7 = ptrInt[width*2];
            tmp1 += 16;
            tmp2++;
            mb[48] = (uint8_t)((tmp2 + tmp7) >> 1);

            tmp7 = tmp3 + tmp6;
            tmp1 -= (tmp7 << 2);
            tmp1 -= tmp7;
            tmp7 = tmp4 + tmp5;
            tmp2 = ptrC[width];
            tmp1 += (tmp7 << 4);
            tmp1 += (tmp7 << 2);
            tmp1 += tmp2;
            tmp1 = clp[tmp1>>5];
            tmp7 = ptrInt[width];
            tmp6 += 16;
            tmp1++;
            mb[32] = (uint8_t)((tmp1 + tmp7) >> 1);

            tmp7 = tmp2 + tmp5;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp7 = tmp4 + tmp3;
            tmp1 = *ptrC;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp6 += tmp1;
            tmp6 = clp[tmp6>>5];
            tmp7 = *ptrInt;
            tmp5 += 16;
            tmp6++;
            mb[16] = (uint8_t)((tmp6 + tmp7) >> 1);

            tmp1 += tmp4;
            tmp5 -= (tmp1 << 2);
            tmp5 -= tmp1;
            tmp3 += tmp2;
            tmp6 = ptrC[-(int32_t)width];
            tmp5 += (tmp3 << 4);
            tmp5 += (tmp3 << 2);
            tmp5 += tmp6;
            tmp5 = clp[tmp5>>5];
            tmp7 = ptrInt[-(int32_t)width];
            tmp5++;
            *mb++ = (uint8_t)((tmp5 + tmp7) >> 1);
            ptrC++;
            ptrInt++;
        }
        ptrC += 4*width - partWidth;
        ptrV += 4*width - partWidth;
        ptrInt += 4*width - partWidth;
        mb += 4*16 - partWidth;
    }

	}

/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateHorHalf

        Functional description:
          Function to perform horizontal interpolation of pixel position 'b'
          for a block. Overfilling is done only if needed. Reference
          image (ref) is read at correct position and the predicted part
          is written to macroblock array (mb)

------------------------------------------------------------------------------*/
void h264bsdInterpolateHorHalf(uint8_t *ref, uint8_t *mb,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t partWidth,uint32_t partHeight) {
    uint32_t p1[21*21/4+1];
    uint8_t *ptrJ;
    uint32_t x, y;
    int32_t tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    const uint8_t *clp = h264bsdClip + 512;

    
    ASSERT(ref);
    ASSERT(mb);
    ASSERT((partWidth&0x3) == 0);
    ASSERT((partHeight&0x3) == 0);

    if((x0 < 0) || ((uint32_t)x0+partWidth+5 > width) ||
        (y0 < 0) || ((uint32_t)y0+partHeight > height)) {
        h264bsdFillBlock(ref, (uint8_t*)p1, x0, y0, width, height,
                partWidth+5, partHeight, partWidth+5);

        x0=0;
        y0=0;
        ref = (uint8_t*)p1;
        width = partWidth + 5;
    }

    ref += (uint32_t)y0 * width + (uint32_t)x0;

    ptrJ = ref + 5;

    for(y = partHeight; y; y--) {
        tmp6 = *(ptrJ - 5);
        tmp5 = *(ptrJ - 4);
        tmp4 = *(ptrJ - 3);
        tmp3 = *(ptrJ - 2);
        tmp2 = *(ptrJ - 1);

        /* calculate 4 pels per iteration */
        for(x = (partWidth >> 2); x; x--) {
            /* First pixel */
            tmp6 += 16;
            tmp7 = tmp3 + tmp4;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp7 = tmp2 + tmp5;
            tmp1 = *ptrJ++;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp6 += tmp1;
            tmp6 = clp[tmp6>>5];
            /* Second pixel */
            tmp5 += 16;
            tmp7 = tmp2 + tmp3;
            *mb++ = (uint8_t)tmp6;
            tmp5 += (tmp7 << 4);
            tmp5 += (tmp7 << 2);
            tmp7 = tmp1 + tmp4;
            tmp6 = *ptrJ++;
            tmp5 -= (tmp7 << 2);
            tmp5 -= tmp7;
            tmp5 += tmp6;
            tmp5 = clp[tmp5>>5];
            /* Third pixel */
            tmp4 += 16;
            tmp7 = tmp1 + tmp2;
            *mb++ = (uint8_t)tmp5;
            tmp4 += (tmp7 << 4);
            tmp4 += (tmp7 << 2);
            tmp7 = tmp6 + tmp3;
            tmp5 = *ptrJ++;
            tmp4 -= (tmp7 << 2);
            tmp4 -= tmp7;
            tmp4 += tmp5;
            tmp4 = clp[tmp4>>5];
            /* Fourth pixel */
            tmp3 += 16;
            tmp7 = tmp6 + tmp1;
            *mb++ = (uint8_t)tmp4;
            tmp3 += (tmp7 << 4);
            tmp3 += (tmp7 << 2);
            tmp7 = tmp5 + tmp2;
            tmp4 = *ptrJ++;
            tmp3 -= (tmp7 << 2);
            tmp3 -= tmp7;
            tmp3 += tmp4;
            tmp3 = clp[tmp3>>5];
            tmp7 = tmp4;
            tmp4 = tmp6;
            tmp6 = tmp2;
            tmp2 = tmp7;
            *mb++ = (uint8_t)tmp3;
            tmp3 = tmp5;
            tmp5 = tmp1;
        }
        ptrJ += width - partWidth;
        mb += 16 - partWidth;
    }

	}

/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateHorQuarter

        Functional description:
          Function to perform horizontal interpolation of pixel position 'a'
          or 'c' for a block. Overfilling is done only if needed. Reference
          image (ref) is read at correct position and the predicted part
          is written to macroblock array (mb)

------------------------------------------------------------------------------*/
void h264bsdInterpolateHorQuarter(uint8_t *ref, uint8_t *mb,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t partWidth,uint32_t partHeight,
  uint32_t horOffset) /* 0 for pixel a, 1 for pixel c */
{
    uint32_t p1[21*21/4+1];
    uint8_t *ptrJ;
    uint32_t x, y;
    int32_t tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    const uint8_t *clp = h264bsdClip + 512;

    
    ASSERT(ref);
    ASSERT(mb);

    if((x0 < 0) || ((uint32_t)x0+partWidth+5 > width) ||
        (y0 < 0) || ((uint32_t)y0+partHeight > height)) {
        h264bsdFillBlock(ref, (uint8_t*)p1, x0, y0, width, height,
                partWidth+5, partHeight, partWidth+5);

        x0=0;
        y0=0;
        ref = (uint8_t*)p1;
        width = partWidth + 5;
		  }

    ref += (uint32_t)y0 * width + (uint32_t)x0;

    ptrJ = ref + 5;

    for(y = partHeight; y; y--) {
        tmp6 = *(ptrJ - 5);
        tmp5 = *(ptrJ - 4);
        tmp4 = *(ptrJ - 3);
        tmp3 = *(ptrJ - 2);
        tmp2 = *(ptrJ - 1);

        /* calculate 4 pels per iteration */
        for(x = (partWidth >> 2); x; x--) {
            /* First pixel */
            tmp6 += 16;
            tmp7 = tmp3 + tmp4;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp7 = tmp2 + tmp5;
            tmp1 = *ptrJ++;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp6 += tmp1;
            tmp6 = clp[tmp6>>5];
            tmp5 += 16;
            if(!horOffset)
                tmp6 += tmp4;
            else
                tmp6 += tmp3;
            *mb++ = (uint8_t)((tmp6 + 1) >> 1);
            /* Second pixel */
            tmp7 = tmp2 + tmp3;
            tmp5 += (tmp7 << 4);
            tmp5 += (tmp7 << 2);
            tmp7 = tmp1 + tmp4;
            tmp6 = *ptrJ++;
            tmp5 -= (tmp7 << 2);
            tmp5 -= tmp7;
            tmp5 += tmp6;
            tmp5 = clp[tmp5>>5];
            tmp4 += 16;
            if(!horOffset)
                tmp5 += tmp3;
            else
                tmp5 += tmp2;
            *mb++ = (uint8_t)((tmp5 + 1) >> 1);
            /* Third pixel */
            tmp7 = tmp1 + tmp2;
            tmp4 += (tmp7 << 4);
            tmp4 += (tmp7 << 2);
            tmp7 = tmp6 + tmp3;
            tmp5 = *ptrJ++;
            tmp4 -= (tmp7 << 2);
            tmp4 -= tmp7;
            tmp4 += tmp5;
            tmp4 = clp[tmp4>>5];
            tmp3 += 16;
            if(!horOffset)
                tmp4 += tmp2;
            else
                tmp4 += tmp1;
            *mb++ = (uint8_t)((tmp4 + 1) >> 1);
            /* Fourth pixel */
            tmp7 = tmp6 + tmp1;
            tmp3 += (tmp7 << 4);
            tmp3 += (tmp7 << 2);
            tmp7 = tmp5 + tmp2;
            tmp4 = *ptrJ++;
            tmp3 -= (tmp7 << 2);
            tmp3 -= tmp7;
            tmp3 += tmp4;
            tmp3 = clp[tmp3>>5];
            if(!horOffset)
                tmp3 += tmp1;
            else
                tmp3 += tmp6;
            *mb++ = (uint8_t)((tmp3 + 1) >> 1);
            tmp3 = tmp5;
            tmp5 = tmp1;
            tmp7 = tmp4;
            tmp4 = tmp6;
            tmp6 = tmp2;
            tmp2 = tmp7;
        }
        ptrJ += width - partWidth;
        mb += 16 - partWidth;
    }

	}

/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateHorVerQuarter

        Functional description:
          Function to perform horizontal and vertical interpolation of pixel
          position 'e', 'g', 'p' or 'r' for a block. Overfilling is done only
          if needed. Reference image (ref) is read at correct position and
          the predicted part is written to macroblock array (mb)

------------------------------------------------------------------------------*/
void h264bsdInterpolateHorVerQuarter(uint8_t *ref, uint8_t *mb,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t partWidth,uint32_t partHeight,
  uint32_t horVerOffset) /* 0 for pixel e, 1 for pixel g,
                       2 for pixel p, 3 for pixel r */
{
    uint32_t p1[21*21/4+1];
    uint8_t *ptrC, *ptrJ, *ptrV;
    uint32_t x, y;
    int32_t tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    const uint8_t *clp = h264bsdClip + 512;

    
    ASSERT(ref);
    ASSERT(mb);

    if((x0 < 0) || ((uint32_t)x0+partWidth+5 > width) ||
        (y0 < 0) || ((uint32_t)y0+partHeight+5 > height)) {
        h264bsdFillBlock(ref, (uint8_t*)p1, x0, y0, width, height,
                partWidth+5, partHeight+5, partWidth+5);

        x0=0;
        y0=0;
        ref = (uint8_t*)p1;
        width = partWidth+5;
    }

    /* Ref points to G + (-2, -2) */
    ref += (uint32_t)y0 * width + (uint32_t)x0;

    /* ptrJ points to either J or Q, depending on vertical offset */
    ptrJ = ref + (((horVerOffset & 0x2) >> 1) + 2) * width + 5;

    /* ptrC points to either C or D, depending on horizontal offset */
    ptrC = ref + width + 2 + (horVerOffset & 0x1);

    for(y = partHeight; y; y--) {
        tmp6 = *(ptrJ - 5);
        tmp5 = *(ptrJ - 4);
        tmp4 = *(ptrJ - 3);
        tmp3 = *(ptrJ - 2);
        tmp2 = *(ptrJ - 1);

        /* Horizontal interpolation, calculate 4 pels per iteration */
        for(x = (partWidth >> 2); x; x--) {
            /* First pixel */
            tmp6 += 16;
            tmp7 = tmp3 + tmp4;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp7 = tmp2 + tmp5;
            tmp1 = *ptrJ++;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp6 += tmp1;
            tmp6 = clp[tmp6>>5];
            /* Second pixel */
            tmp5 += 16;
            tmp7 = tmp2 + tmp3;
            *mb++ = (uint8_t)tmp6;
            tmp5 += (tmp7 << 4);
            tmp5 += (tmp7 << 2);
            tmp7 = tmp1 + tmp4;
            tmp6 = *ptrJ++;
            tmp5 -= (tmp7 << 2);
            tmp5 -= tmp7;
            tmp5 += tmp6;
            tmp5 = clp[tmp5>>5];
            /* Third pixel */
            tmp4 += 16;
            tmp7 = tmp1 + tmp2;
            *mb++ = (uint8_t)tmp5;
            tmp4 += (tmp7 << 4);
            tmp4 += (tmp7 << 2);
            tmp7 = tmp6 + tmp3;
            tmp5 = *ptrJ++;
            tmp4 -= (tmp7 << 2);
            tmp4 -= tmp7;
            tmp4 += tmp5;
            tmp4 = clp[tmp4>>5];
            /* Fourth pixel */
            tmp3 += 16;
            tmp7 = tmp6 + tmp1;
            *mb++ = (uint8_t)tmp4;
            tmp3 += (tmp7 << 4);
            tmp3 += (tmp7 << 2);
            tmp7 = tmp5 + tmp2;
            tmp4 = *ptrJ++;
            tmp3 -= (tmp7 << 2);
            tmp3 -= tmp7;
            tmp3 += tmp4;
            tmp3 = clp[tmp3>>5];
            tmp7 = tmp4;
            tmp4 = tmp6;
            tmp6 = tmp2;
            tmp2 = tmp7;
            *mb++ = (uint8_t)tmp3;
            tmp3 = tmp5;
            tmp5 = tmp1;
        }
        ptrJ += width - partWidth;
        mb += 16 - partWidth;
    }

    mb -= 16*partHeight;
    ptrV = ptrC + 5*width;

    for(y = (partHeight >> 2); y; y--) {
        /* Vertical interpolation and averaging, 4 pels per iteration */
        for(x = partWidth; x; x--) {
            tmp4 = ptrV[-(int32_t)width*2];
            tmp5 = ptrV[-(int32_t)width];
            tmp1 = ptrV[width];
            tmp2 = ptrV[width*2];
            tmp6 = *ptrV++;

            tmp7 = tmp4 + tmp1;
            tmp2 -= (tmp7 << 2);
            tmp2 -= tmp7;
            tmp2 += 16;
            tmp7 = tmp5 + tmp6;
            tmp3 = ptrC[width*2];
            tmp2 += (tmp7 << 4);
            tmp2 += (tmp7 << 2);
            tmp2 += tmp3;
            tmp7 = clp[tmp2>>5];
            tmp2 = mb[48];
            tmp1 += 16;
            tmp7++;
            mb[48] = (uint8_t)((tmp2 + tmp7) >> 1);

            tmp7 = tmp3 + tmp6;
            tmp1 -= (tmp7 << 2);
            tmp1 -= tmp7;
            tmp7 = tmp4 + tmp5;
            tmp2 = ptrC[width];
            tmp1 += (tmp7 << 4);
            tmp1 += (tmp7 << 2);
            tmp1 += tmp2;
            tmp7 = clp[tmp1>>5];
            tmp1 = mb[32];
            tmp6 += 16;
            tmp7++;
            mb[32] = (uint8_t)((tmp1 + tmp7) >> 1);

            tmp1 = *ptrC;
            tmp7 = tmp2 + tmp5;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp7 = tmp4 + tmp3;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp6 += tmp1;
            tmp7 = clp[tmp6>>5];
            tmp6 = mb[16];
            tmp5 += 16;
            tmp7++;
            mb[16] = (uint8_t)((tmp6 + tmp7) >> 1);

            tmp6 = ptrC[-(int32_t)width];
            tmp1 += tmp4;
            tmp5 -= (tmp1 << 2);
            tmp5 -= tmp1;
            tmp3 += tmp2;
            tmp5 += (tmp3 << 4);
            tmp5 += (tmp3 << 2);
            tmp5 += tmp6;
            tmp7 = clp[tmp5>>5];
            tmp5 = *mb;
            tmp7++;
            *mb++ = (uint8_t)((tmp5 + tmp7) >> 1);
            ptrC++;

        }
        ptrC += 4*width - partWidth;
        ptrV += 4*width - partWidth;
        mb += 4*16 - partWidth;
    }

}
#endif

/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateMidHalf

        Functional description:
          Function to perform horizontal and vertical interpolation of pixel
          position 'j' for a block. Overfilling is done only if needed.
          Reference image (ref) is read at correct position and the predicted
          part is written to macroblock array (mb)

------------------------------------------------------------------------------*/
void h264bsdInterpolateMidHalf(uint8_t *ref,uint8_t *mb,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t partWidth,uint32_t partHeight) {
    uint32_t p1[21*21/4+1];
    uint32_t x, y;
    int32_t tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    int32_t *ptrC, *ptrV, *b1;
    uint8_t  *ptrJ;
    int32_t table[21*16];
    const uint8_t *clp = h264bsdClip + 512;

    
    ASSERT(ref);
    ASSERT(mb);

    if((x0 < 0) || ((uint32_t)x0+partWidth+5 > width) ||
        (y0 < 0) || ((uint32_t)y0+partHeight+5 > height)) {
        h264bsdFillBlock(ref, (uint8_t*)p1, x0, y0, width, height,
                partWidth+5, partHeight+5, partWidth+5);

        x0=0;
        y0=0;
        ref = (uint8_t*)p1;
        width = partWidth+5;
			}

    ref += (uint32_t)y0 * width + (uint32_t)x0;

    b1 = table;
    ptrJ = ref + 5;

    /* First step: calculate intermediate values for horizontal interpolation */
    for(y = partHeight + 5; y; y--) {
        tmp6 = *(ptrJ - 5);
        tmp5 = *(ptrJ - 4);
        tmp4 = *(ptrJ - 3);
        tmp3 = *(ptrJ - 2);
        tmp2 = *(ptrJ - 1);

        /* 4 pels per iteration */
        for(x = (partWidth >> 2); x; x--)
{
            /* First pixel */
            tmp7 = tmp3 + tmp4;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp7 = tmp2 + tmp5;
            tmp1 = *ptrJ++;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp6 += tmp1;
            *b1++ = tmp6;
            /* Second pixel */
            tmp7 = tmp2 + tmp3;
            tmp5 += (tmp7 << 4);
            tmp5 += (tmp7 << 2);
            tmp7 = tmp1 + tmp4;
            tmp6 = *ptrJ++;
            tmp5 -= (tmp7 << 2);
            tmp5 -= tmp7;
            tmp5 += tmp6;
            *b1++ = tmp5;
            /* Third pixel */
            tmp7 = tmp1 + tmp2;
            tmp4 += (tmp7 << 4);
            tmp4 += (tmp7 << 2);
            tmp7 = tmp6 + tmp3;
            tmp5 = *ptrJ++;
            tmp4 -= (tmp7 << 2);
            tmp4 -= tmp7;
            tmp4 += tmp5;
            *b1++ = tmp4;
            /* Fourth pixel */
            tmp7 = tmp6 + tmp1;
            tmp3 += (tmp7 << 4);
            tmp3 += (tmp7 << 2);
            tmp7 = tmp5 + tmp2;
            tmp4 = *ptrJ++;
            tmp3 -= (tmp7 << 2);
            tmp3 -= tmp7;
            tmp3 += tmp4;
            *b1++ = tmp3;
            tmp7 = tmp4;
            tmp4 = tmp6;
            tmp6 = tmp2;
            tmp2 = tmp7;
            tmp3 = tmp5;
            tmp5 = tmp1;
        }
        ptrJ += width - partWidth;
    }

    /* Second step: calculate vertical interpolation */
    ptrC = table + partWidth;
    ptrV = ptrC + 5*partWidth;
    for(y = (partHeight >> 2); y; y--) {
        /* 4 pels per iteration */
        for(x = partWidth; x; x--) {
            tmp4 = ptrV[-(int32_t)partWidth*2];
            tmp5 = ptrV[-(int32_t)partWidth];
            tmp1 = ptrV[partWidth];
            tmp2 = ptrV[partWidth*2];
            tmp6 = *ptrV++;

            tmp7 = tmp4 + tmp1;
            tmp2 -= (tmp7 << 2);
            tmp2 -= tmp7;
            tmp2 += 512;
            tmp7 = tmp5 + tmp6;
            tmp3 = ptrC[partWidth*2];
            tmp2 += (tmp7 << 4);
            tmp2 += (tmp7 << 2);
            tmp2 += tmp3;
            tmp7 = clp[tmp2>>10];
            tmp1 += 512;
            mb[48] = (uint8_t)tmp7;

            tmp7 = tmp3 + tmp6;
            tmp1 -= (tmp7 << 2);
            tmp1 -= tmp7;
            tmp7 = tmp4 + tmp5;
            tmp2 = ptrC[partWidth];
            tmp1 += (tmp7 << 4);
            tmp1 += (tmp7 << 2);
            tmp1 += tmp2;
            tmp7 = clp[tmp1>>10];
            tmp6 += 512;
            mb[32] = (uint8_t)tmp7;

            tmp1 = *ptrC;
            tmp7 = tmp2 + tmp5;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp7 = tmp4 + tmp3;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp6 += tmp1;
            tmp7 = clp[tmp6>>10];
            tmp5 += 512;
            mb[16] = (uint8_t)tmp7;

            tmp6 = ptrC[-(int32_t)partWidth];
            tmp1 += tmp4;
            tmp5 -= (tmp1 << 2);
            tmp5 -= tmp1;
            tmp3 += tmp2;
            tmp5 += (tmp3 << 4);
            tmp5 += (tmp3 << 2);
            tmp5 += tmp6;
            tmp7 = clp[tmp5>>10];
            *mb++ = (uint8_t)tmp7;
            ptrC++;
        }
        mb += 4*16 - partWidth;
        ptrC += 3*partWidth;
        ptrV += 3*partWidth;
    }

}


/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateMidVerQuarter

        Functional description:
          Function to perform horizontal and vertical interpolation of pixel
          position 'f' or 'q' for a block. Overfilling is done only if needed.
          Reference image (ref) is read at correct position and the predicted
          part is written to macroblock array (mb)

------------------------------------------------------------------------------*/
void h264bsdInterpolateMidVerQuarter(uint8_t *ref, uint8_t *mb,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t partWidth,uint32_t partHeight,
  uint32_t verOffset)    /* 0 for pixel f, 1 for pixel q */
{
    uint32_t p1[21*21/4+1];
    uint32_t x, y;
    int32_t tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    int32_t *ptrC, *ptrV, *ptrInt, *b1;
    uint8_t  *ptrJ;
    int32_t table[21*16];
    const uint8_t *clp = h264bsdClip + 512;

    
    ASSERT(ref);
    ASSERT(mb);

    if((x0 < 0) || ((uint32_t)x0+partWidth+5 > width) ||
        (y0 < 0) || ((uint32_t)y0+partHeight+5 > height)) {
        h264bsdFillBlock(ref, (uint8_t*)p1, x0, y0, width, height,
                partWidth+5, partHeight+5, partWidth+5);

        x0=0;
        y0=0;
        ref = (uint8_t*)p1;
        width = partWidth+5;
    }

    ref += (uint32_t)y0 * width + (uint32_t)x0;

    b1 = table;
    ptrJ = ref + 5;

    /* First step: calculate intermediate values for
     * horizontal interpolation */
    for(y = partHeight + 5; y; y--) {
        tmp6 = *(ptrJ - 5);
        tmp5 = *(ptrJ - 4);
        tmp4 = *(ptrJ - 3);
        tmp3 = *(ptrJ - 2);
        tmp2 = *(ptrJ - 1);
        for(x = (partWidth >> 2); x; x--) {
            /* First pixel */
            tmp7 = tmp3 + tmp4;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp7 = tmp2 + tmp5;
            tmp1 = *ptrJ++;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp6 += tmp1;
            *b1++ = tmp6;
            /* Second pixel */
            tmp7 = tmp2 + tmp3;
            tmp5 += (tmp7 << 4);
            tmp5 += (tmp7 << 2);
            tmp7 = tmp1 + tmp4;
            tmp6 = *ptrJ++;
            tmp5 -= (tmp7 << 2);
            tmp5 -= tmp7;
            tmp5 += tmp6;
            *b1++ = tmp5;
            /* Third pixel */
            tmp7 = tmp1 + tmp2;
            tmp4 += (tmp7 << 4);
            tmp4 += (tmp7 << 2);
            tmp7 = tmp6 + tmp3;
            tmp5 = *ptrJ++;
            tmp4 -= (tmp7 << 2);
            tmp4 -= tmp7;
            tmp4 += tmp5;
            *b1++ = tmp4;
            /* Fourth pixel */
            tmp7 = tmp6 + tmp1;
            tmp3 += (tmp7 << 4);
            tmp3 += (tmp7 << 2);
            tmp7 = tmp5 + tmp2;
            tmp4 = *ptrJ++;
            tmp3 -= (tmp7 << 2);
            tmp3 -= tmp7;
            tmp3 += tmp4;
            *b1++ = tmp3;
            tmp7 = tmp4;
            tmp4 = tmp6;
            tmp6 = tmp2;
            tmp2 = tmp7;
            tmp3 = tmp5;
            tmp5 = tmp1;
        }
        ptrJ += width - partWidth;
    }

    /* Second step: calculate vertical interpolation and average */
    ptrC = table + partWidth;
    ptrV = ptrC + 5*partWidth;
    /* Pointer to integer sample position, either M or R */
    ptrInt = ptrC + (2+verOffset)*partWidth;
    for(y = (partHeight >> 2); y; y--) {
        for(x = partWidth; x; x--) {
            tmp4 = ptrV[-(int32_t)partWidth*2];
            tmp5 = ptrV[-(int32_t)partWidth];
            tmp1 = ptrV[partWidth];
            tmp2 = ptrV[partWidth*2];
            tmp6 = *ptrV++;

            tmp7 = tmp4 + tmp1;
            tmp2 -= (tmp7 << 2);
            tmp2 -= tmp7;
            tmp2 += 512;
            tmp7 = tmp5 + tmp6;
            tmp3 = ptrC[partWidth*2];
            tmp2 += (tmp7 << 4);
            tmp2 += (tmp7 << 2);
            tmp7 = ptrInt[partWidth*2];
            tmp2 += tmp3;
            tmp2 = clp[tmp2>>10];
            tmp7 += 16;
            tmp7 = clp[tmp7>>5];
            tmp1 += 512;
            tmp2++;
            mb[48] = (uint8_t)((tmp7 + tmp2) >> 1);

            tmp7 = tmp3 + tmp6;
            tmp1 -= (tmp7 << 2);
            tmp1 -= tmp7;
            tmp7 = tmp4 + tmp5;
            tmp2 = ptrC[partWidth];
            tmp1 += (tmp7 << 4);
            tmp1 += (tmp7 << 2);
            tmp7 = ptrInt[partWidth];
            tmp1 += tmp2;
            tmp1 = clp[tmp1>>10];
            tmp7 += 16;
            tmp7 = clp[tmp7>>5];
            tmp6 += 512;
            tmp1++;
            mb[32] = (uint8_t)((tmp7 + tmp1) >> 1);

            tmp1 = *ptrC;
            tmp7 = tmp2 + tmp5;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp7 = tmp4 + tmp3;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp7 = *ptrInt;
            tmp6 += tmp1;
            tmp6 = clp[tmp6>>10];
            tmp7 += 16;
            tmp7 = clp[tmp7>>5];
            tmp5 += 512;
            tmp6++;
            mb[16] = (uint8_t)((tmp7 + tmp6) >> 1);

            tmp6 = ptrC[-(int32_t)partWidth];
            tmp1 += tmp4;
            tmp5 -= (tmp1 << 2);
            tmp5 -= tmp1;
            tmp3 += tmp2;
            tmp5 += (tmp3 << 4);
            tmp5 += (tmp3 << 2);
            tmp7 = ptrInt[-(int32_t)partWidth];
            tmp5 += tmp6;
            tmp5 = clp[tmp5>>10];
            tmp7 += 16;
            tmp7 = clp[tmp7>>5];
            tmp5++;
            *mb++ = (uint8_t)((tmp7 + tmp5) >> 1);
            ptrC++;
            ptrInt++;
        }
        mb += 4*16 - partWidth;
        ptrC += 3*partWidth;
        ptrV += 3*partWidth;
        ptrInt += 3*partWidth;
    }

}


/*------------------------------------------------------------------------------

    Function: h264bsdInterpolateMidHorQuarter

        Functional description:
          Function to perform horizontal and vertical interpolation of pixel
          position 'i' or 'k' for a block. Overfilling is done only if needed.
          Reference image (ref) is read at correct position and the predicted
          part is written to macroblock array (mb)

------------------------------------------------------------------------------*/
void h264bsdInterpolateMidHorQuarter(uint8_t *ref,uint8_t *mb,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t partWidth,uint32_t partHeight,
  uint32_t horOffset)    /* 0 for pixel i, 1 for pixel k */
{
    uint32_t p1[21*21/4+1];
    uint32_t x, y;
    int32_t tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    int32_t *ptrJ, *ptrInt, *h1;
    uint8_t  *ptrC, *ptrV;
    int32_t table[21*16];
    int32_t tableWidth = (int32_t)partWidth+5;
    const uint8_t *clp = h264bsdClip + 512;

    
    ASSERT(ref);
    ASSERT(mb);

    if((x0 < 0) || ((uint32_t)x0+partWidth+5 > width) ||
        (y0 < 0) || ((uint32_t)y0+partHeight+5 > height)) {
        h264bsdFillBlock(ref, (uint8_t*)p1, x0, y0, width, height,
                partWidth+5, partHeight+5, partWidth+5);

        x0=0;
        y0=0;
        ref = (uint8_t*)p1;
        width = partWidth+5;
    }

    ref += (uint32_t)y0 * width + (uint32_t)x0;

    h1 = table + tableWidth;
    ptrC = ref + width;
    ptrV = ptrC + 5*width;

    /* First step: calculate intermediate values for
     * vertical interpolation */
    for(y = (partHeight >> 2); y; y--) {
        for(x = (uint32_t)tableWidth; x; x--) {
            tmp4 = ptrV[-(int32_t)width*2];
            tmp5 = ptrV[-(int32_t)width];
            tmp1 = ptrV[width];
            tmp2 = ptrV[width*2];
            tmp6 = *ptrV++;

            tmp7 = tmp4 + tmp1;
            tmp2 -= (tmp7 << 2);
            tmp2 -= tmp7;
            tmp7 = tmp5 + tmp6;
            tmp3 = ptrC[width*2];
            tmp2 += (tmp7 << 4);
            tmp2 += (tmp7 << 2);
            tmp2 += tmp3;
            h1[tableWidth*2] = tmp2;

            tmp7 = tmp3 + tmp6;
            tmp1 -= (tmp7 << 2);
            tmp1 -= tmp7;
            tmp7 = tmp4 + tmp5;
            tmp2 = ptrC[width];
            tmp1 += (tmp7 << 4);
            tmp1 += (tmp7 << 2);
            tmp1 += tmp2;
            h1[tableWidth] = tmp1;

            tmp1 = *ptrC;
            tmp7 = tmp2 + tmp5;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp7 = tmp4 + tmp3;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp6 += tmp1;
            *h1 = tmp6;

            tmp6 = ptrC[-(int32_t)width];
            tmp1 += tmp4;
            tmp5 -= (tmp1 << 2);
            tmp5 -= tmp1;
            tmp3 += tmp2;
            tmp5 += (tmp3 << 4);
            tmp5 += (tmp3 << 2);
            tmp5 += tmp6;
            h1[-tableWidth] = tmp5;
            h1++;
            ptrC++;
        }
        ptrC += 4*width - partWidth - 5;
        ptrV += 4*width - partWidth - 5;
        h1 += 3*tableWidth;
		  }

    /* Second step: calculate horizontal interpolation and average */
    ptrJ = table + 5;
    /* Pointer to integer sample position, either G or H */
    ptrInt = table + 2 + horOffset;
    for(y = partHeight; y; y--) {
        tmp6 = *(ptrJ - 5);
        tmp5 = *(ptrJ - 4);
        tmp4 = *(ptrJ - 3);
        tmp3 = *(ptrJ - 2);
        tmp2 = *(ptrJ - 1);
        for(x = (partWidth>>2); x; x--) {
            /* First pixel */
            tmp6 += 512;
            tmp7 = tmp3 + tmp4;
            tmp6 += (tmp7 << 4);
            tmp6 += (tmp7 << 2);
            tmp7 = tmp2 + tmp5;
            tmp1 = *ptrJ++;
            tmp6 -= (tmp7 << 2);
            tmp6 -= tmp7;
            tmp7 = *ptrInt++;
            tmp6 += tmp1;
            tmp6 = clp[tmp6 >> 10];
            tmp7 += 16;
            tmp7 = clp[tmp7 >> 5];
            tmp5 += 512;
            tmp6++;
            *mb++ = (uint8_t)((tmp6 + tmp7) >> 1);
            /* Second pixel */
            tmp7 = tmp2 + tmp3;
            tmp5 += (tmp7 << 4);
            tmp5 += (tmp7 << 2);
            tmp7 = tmp1 + tmp4;
            tmp6 = *ptrJ++;
            tmp5 -= (tmp7 << 2);
            tmp5 -= tmp7;
            tmp7 = *ptrInt++;
            tmp5 += tmp6;
            tmp5 = clp[tmp5 >> 10];
            tmp7 += 16;
            tmp7 = clp[tmp7 >> 5];
            tmp4 += 512;
            tmp5++;
            *mb++ = (uint8_t)((tmp5 + tmp7) >> 1);
            /* Third pixel */
            tmp7 = tmp1 + tmp2;
            tmp4 += (tmp7 << 4);
            tmp4 += (tmp7 << 2);
            tmp7 = tmp6 + tmp3;
            tmp5 = *ptrJ++;
            tmp4 -= (tmp7 << 2);
            tmp4 -= tmp7;
            tmp7 = *ptrInt++;
            tmp4 += tmp5;
            tmp4 = clp[tmp4 >> 10];
            tmp7 += 16;
            tmp7 = clp[tmp7 >> 5];
            tmp3 += 512;
            tmp4++;
            *mb++ = (uint8_t)((tmp4 + tmp7) >> 1);
            /* Fourth pixel */
            tmp7 = tmp6 + tmp1;
            tmp3 += (tmp7 << 4);
            tmp3 += (tmp7 << 2);
            tmp7 = tmp5 + tmp2;
            tmp4 = *ptrJ++;
            tmp3 -= (tmp7 << 2);
            tmp3 -= tmp7;
            tmp7 = *ptrInt++;
            tmp3 += tmp4;
            tmp3 = clp[tmp3 >> 10];
            tmp7 += 16;
            tmp7 = clp[tmp7 >> 5];
            tmp3++;
            *mb++ = (uint8_t)((tmp3 + tmp7) >> 1);
            tmp3 = tmp5;
            tmp5 = tmp1;
            tmp7 = tmp4;
            tmp4 = tmp6;
            tmp6 = tmp2;
            tmp2 = tmp7;
        }
        ptrJ += 5;
        ptrInt += 5;
        mb += 16 - partWidth;
    }

	}


/*------------------------------------------------------------------------------

    Function: h264bsdPredictSamples

        Functional description:
          This function reconstructs a prediction for a macroblock partition.
          The prediction is either copied or interpolated using the reference
          frame and the motion vector. Both luminance and chrominance parts are
          predicted. The prediction is stored in given macroblock array (data).
        Inputs:
          data          pointer to macroblock array (384 bytes) for output
          mv            pointer to motion vector used for prediction
          refPic        pointer to reference picture structure
          xA            x-coordinate for current macroblock
          yA            y-coordinate for current macroblock
          partX         x-offset for partition in macroblock
          partY         y-offset for partition in macroblock
          partWidth     width of partition
          partHeight    height of partition
        Outputs:
          data          macroblock array (16x16+8x8+8x8) where predicted
                        partition is stored at correct position

------------------------------------------------------------------------------*/
void h264bsdPredictSamples(uint8_t *data, mv_t *mv,
  image_t *refPic,
  uint32_t xA,uint32_t yA,
  uint32_t partX,uint32_t partY,
  uint32_t partWidth,uint32_t partHeight) {
    uint32_t xFrac, yFrac, width, height;
    int32_t xInt, yInt;
    uint8_t *lumaPartData;


    ASSERT(data);
    ASSERT(mv);
    ASSERT(partWidth);
    ASSERT(partHeight);
    ASSERT(refPic);
    ASSERT(refPic->data);
    ASSERT(refPic->width);
    ASSERT(refPic->height);

    /* luma */
    lumaPartData = data + 16*partY + partX;

    xFrac = mv->hor & 0x3;
    yFrac = mv->ver & 0x3;

    width = 16 * refPic->width;
    height = 16 * refPic->height;

    xInt = (int32_t)xA + (int32_t)partX + (mv->hor >> 2);
    yInt = (int32_t)yA + (int32_t)partY + (mv->ver >> 2);

    ASSERT(lumaFracPos[xFrac][yFrac] < 16);

    switch(lumaFracPos[xFrac][yFrac]) {
        case 0: /* G */
            h264bsdFillBlock(refPic->data, lumaPartData,
                    xInt,yInt,width,height,partWidth,partHeight,16);
            break;
        case 1: /* d */
            h264bsdInterpolateVerQuarter(refPic->data, lumaPartData,
                    xInt, yInt-2, width, height, partWidth, partHeight, 0);
            break;
        case 2: /* h */
            h264bsdInterpolateVerHalf(refPic->data, lumaPartData,
                    xInt, yInt-2, width, height, partWidth, partHeight);
            break;
        case 3: /* n */
            h264bsdInterpolateVerQuarter(refPic->data, lumaPartData,
                    xInt, yInt-2, width, height, partWidth, partHeight, 1);
            break;
        case 4: /* a */
            h264bsdInterpolateHorQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt, width, height, partWidth, partHeight, 0);
            break;
        case 5: /* e */
            h264bsdInterpolateHorVerQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt-2, width, height, partWidth, partHeight, 0);
            break;
        case 6: /* i */
            h264bsdInterpolateMidHorQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt-2, width, height, partWidth, partHeight, 0);
            break;
        case 7: /* p */
            h264bsdInterpolateHorVerQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt-2, width, height, partWidth, partHeight, 2);
            break;
        case 8: /* b */
            h264bsdInterpolateHorHalf(refPic->data, lumaPartData,
                    xInt-2, yInt, width, height, partWidth, partHeight);
            break;
        case 9: /* f */
            h264bsdInterpolateMidVerQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt-2, width, height, partWidth, partHeight, 0);
            break;
        case 10: /* j */
            h264bsdInterpolateMidHalf(refPic->data, lumaPartData,
                    xInt-2, yInt-2, width, height, partWidth, partHeight);
            break;
        case 11: /* q */
            h264bsdInterpolateMidVerQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt-2, width, height, partWidth, partHeight, 1);
            break;
        case 12: /* c */
            h264bsdInterpolateHorQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt, width, height, partWidth, partHeight, 1);
            break;
        case 13: /* g */
            h264bsdInterpolateHorVerQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt-2, width, height, partWidth, partHeight, 1);
            break;
        case 14: /* k */
            h264bsdInterpolateMidHorQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt-2, width, height, partWidth, partHeight, 1);
            break;
        default: /* case 15, r */
            h264bsdInterpolateHorVerQuarter(refPic->data, lumaPartData,
                    xInt-2, yInt-2, width, height, partWidth, partHeight, 3);
            break;
    }

    /* chroma */
    PredictChroma(
      data + 16*16 + (partY>>1)*8 + (partX>>1),
      xA + partX,
      yA + partY,
      partWidth,
      partHeight,
      mv,
      refPic);

	}

#else /* H264DEC_OMXDL */
/*------------------------------------------------------------------------------

    Function: h264bsdPredictSamples

        Functional description:
          This function reconstructs a prediction for a macroblock partition.
          The prediction is either copied or interpolated using the reference
          frame and the motion vector. Both luminance and chrominance parts are
          predicted. The prediction is stored in given macroblock array (data).
        Inputs:
          data          pointer to macroblock array (384 bytes) for output
          mv            pointer to motion vector used for prediction
          refPic        pointer to reference picture structure
          xA            x-coordinate for current macroblock
          yA            y-coordinate for current macroblock
          partX         x-offset for partition in macroblock
          partY         y-offset for partition in macroblock
          partWidth     width of partition
          partHeight    height of partition
        Outputs:
          data          macroblock array (16x16+8x8+8x8) where predicted
                        partition is stored at correct position

------------------------------------------------------------------------------*/
void h264bsdPredictSamples(uint8_t *data, mv_t *mv, image_t *refPic,
  uint32_t colAndRow,
  uint32_t part, uint8_t *pFill) {
    uint32_t xFrac, yFrac;
    uint32_t width, height;
    int32_t xInt, yInt, x0, y0;
    uint8_t *partData, *ref;
    OMXSize roi;
    uint32_t fillWidth;
    uint32_t fillHeight;
    OMXResult res;
    uint32_t xA, yA;
    uint32_t partX, partY;
    uint32_t partWidth, partHeight;


    ASSERT(data);
    ASSERT(mv);
    ASSERT(refPic);
    ASSERT(refPic->data);
    ASSERT(refPic->width);
    ASSERT(refPic->height);

    xA = (colAndRow & 0xFFFF0000) >> 16;
    yA = (colAndRow & 0x0000FFFF);

    partX = (part & 0xFF000000) >> 24;
    partY = (part & 0x00FF0000) >> 16;
    partWidth = (part & 0x0000FF00) >> 8;
    partHeight = (part & 0x000000FF);

    ASSERT(partWidth);
    ASSERT(partHeight);

    /* luma */
    partData = data + 16*partY + partX;

    xFrac = mv->hor & 0x3;
    yFrac = mv->ver & 0x3;

    width = 16 * refPic->width;
    height = 16 * refPic->height;

    xInt = (int32_t)xA + (int32_t)partX + (mv->hor >> 2);
    yInt = (int32_t)yA + (int32_t)partY + (mv->ver >> 2);

    x0 = (xFrac) ? xInt-2 : xInt;
    y0 = (yFrac) ? yInt-2 : yInt;

    if(xFrac) {
        if(partWidth == 16)
            fillWidth = 32;
        else
            fillWidth = 16;
    }
    else
        fillWidth = (partWidth*2);
    if(yFrac)
        fillHeight = partHeight+5;
    else
        fillHeight = partHeight;


    if((x0 < 0) || ((uint32_t)x0+fillWidth > width) ||
        (y0 < 0) || ((uint32_t)y0+fillHeight > height)) {
        h264bsdFillBlock(refPic->data, (uint8_t*)pFill, x0, y0, width, height,
                fillWidth, fillHeight, fillWidth);

        x0=0;
        y0=0;
        ref = pFill;
        width = fillWidth;
        if(yFrac)
            ref += 2*width;
        if(xFrac)
            ref += 2;
	    }
    else {
        ref = refPic->data + yInt*width + xInt;
		  }
    /* Luma interpolation */
    roi.width = (int32_t)partWidth;
    roi.height = (int32_t)partHeight;

    res = omxVCM4P10_InterpolateLuma(ref, (int32_t)width, partData, 16,
                                        (int32_t)xFrac, (int32_t)yFrac, roi);
    ASSERT(res == 0);

    /* Chroma */
    width  = 8 * refPic->width;
    height = 8 * refPic->height;

    x0 = ((xA + partX) >> 1) + (mv->hor >> 3);
    y0 = ((yA + partY) >> 1) + (mv->ver >> 3);
    xFrac = mv->hor & 0x7;
    yFrac = mv->ver & 0x7;

    ref = refPic->data + 256 * refPic->width * refPic->height;

    roi.width = (int32_t)(partWidth >> 1);
    fillWidth = ((partWidth >> 1) + 8) & ~0x7;
    roi.height = (int32_t)(partHeight >> 1);
    fillHeight = (partHeight >> 1) + 1;

    if((x0 < 0) || ((uint32_t)x0+fillWidth > width) ||
        (y0 < 0) || ((uint32_t)y0+fillHeight > height)) {
        h264bsdFillBlock(ref, pFill, x0, y0, width, height,
            fillWidth, fillHeight, fillWidth);
        ref += width * height;
        h264bsdFillBlock(ref, pFill + fillWidth*fillHeight,
            x0, y0, width, height, fillWidth,
            fillHeight, fillWidth);

        ref = pFill;
        x0=0;
        y0=0;
        width = fillWidth;
        height = fillHeight;
    }

    partData = data + 16*16 + (partY>>1)*8 + (partX>>1);

    /* Chroma interpolation */
    /*lint --e(737) Loss of sign */
    ref += y0 * width + x0;
    res = armVCM4P10_Interpolate_Chroma(ref, width, partData, 8,
                            (uint32_t)roi.width, (uint32_t)roi.height, xFrac, yFrac);
    ASSERT(res == 0);
    partData += 8 * 8;
    ref += height * width;
    res = armVCM4P10_Interpolate_Chroma(ref, width, partData, 8,
                            (uint32_t)roi.width, (uint32_t)roi.height, xFrac, yFrac);
    ASSERT(res == 0);
	}

#endif /* H264DEC_OMXDL */


/*------------------------------------------------------------------------------

    Function: FillRow1

        Functional description:
          This function gets a row of reference pels in a 'normal' case when no
          overfilling is necessary.

------------------------------------------------------------------------------*/
static void FillRow1(uint8_t *ref, uint8_t *fill, int32_t left,
  int32_t center,
  int32_t right) {

#ifndef FLASCC
    ASSERT(ref);
    ASSERT(fill);

    memcpy(fill, ref, center);
#else
    int i=0;    
    uint8_t *pdest = (uint8_t*) fill;
    uint8_t *psrc = (uint8_t*) ref;
    int loops = (center / sizeof(uint32_t));

    ASSERT(ref);
    ASSERT(fill);

    for(i=0; i < loops; ++i) {
        *((uint32_t*)pdest) = *((uint32_t*)psrc);
        pdest += sizeof(uint32_t);
        psrc += sizeof(uint32_t);
	    }

    loops = (center % sizeof(uint32_t));
    for(i=0; i < loops; ++i)
        *pdest++ = *psrc++;
#endif

}


/*------------------------------------------------------------------------------

    Function: h264bsdFillRow7

        Functional description:
          This function gets a row of reference pels when horizontal coordinate
          is partly negative or partly greater than reference picture width
          (overfilling some pels on left and/or right edge).
        Inputs:
          ref       pointer to reference samples
          left      amount of pixels to overfill on left-edge
          center    amount of pixels to copy
          right     amount of pixels to overfill on right-edge
        Outputs:
          fill      pointer where samples are stored

------------------------------------------------------------------------------*/
#ifndef H264DEC_NEON
void h264bsdFillRow7(uint8_t *ref, uint8_t *fill, int32_t left,
  int32_t center,
  int32_t right) {
    uint8_t tmp = '\0';

    ASSERT(ref);
    ASSERT(fill);

    if(left)
        tmp = *ref;

    for( ; left; left--)
        /*lint -esym(644,tmp)  tmp is initialized if used */
        *fill++ = tmp;

    for( ; center; center--)
        *fill++ = *ref++;

    if(right)
        tmp = ref[-1];

    for( ; right; right--)
        /*lint -esym(644,tmp)  tmp is initialized if used */
        *fill++ = tmp;
	}
#endif
/*------------------------------------------------------------------------------

    Function: h264bsdFillBlock

        Functional description:
          This function gets a block of reference pels. It determines whether
          overfilling is needed or not and repeatedly calls an appropriate
          function (by using a function pointer) that fills one row the block.
        Inputs:
          ref               pointer to reference frame
          x0                x-coordinate for block
          y0                y-coordinate for block
          width             width of reference frame
          height            height of reference frame
          blockWidth        width of block
          blockHeight       height of block
          fillScanLength    length of a line in output array (pixels)
        Outputs:
          fill              pointer to array where output block is written

------------------------------------------------------------------------------*/
void h264bsdFillBlock(uint8_t *ref, uint8_t *fill,
  int32_t x0,int32_t y0,
  uint32_t width,uint32_t height,
  uint32_t blockWidth,uint32_t blockHeight,
  uint32_t fillScanLength) {
    int32_t xstop, ystop;
    void (*fp)(uint8_t*, uint8_t*, int32_t, int32_t, int32_t);
    int32_t left, x, right;
    int32_t top, y, bottom;


    ASSERT(ref);
    ASSERT(fill);
    ASSERT(width);
    ASSERT(height);
    ASSERT(fill);
    ASSERT(blockWidth);
    ASSERT(blockHeight);

    xstop = x0 + (int32_t)blockWidth;
    ystop = y0 + (int32_t)blockHeight;

    /* Choose correct function whether overfilling on left-edge or right-edge
     * is needed or not */
    if(x0 >= 0 && xstop <= (int32_t)width)
        fp = FillRow1;
    else
        fp = h264bsdFillRow7;

    if(ystop < 0)
        y0 = -(int32_t)blockHeight;

    if(xstop < 0)
        x0 = -(int32_t)blockWidth;

    if(y0 > (int32_t)height)
        y0 = (int32_t)height;

    if(x0 > (int32_t)width)
        x0 = (int32_t)width;

    xstop = x0 + (int32_t)blockWidth;
    ystop = y0 + (int32_t)blockHeight;

    if(x0 > 0)
        ref += x0;

    if(y0 > 0)
        ref += y0 * (int32_t)width;

    left = x0 < 0 ? -x0 : 0;
    right = xstop > (int32_t)width ? xstop - (int32_t)width : 0;
    x = (int32_t)blockWidth - left - right;

    top = y0 < 0 ? -y0 : 0;
    bottom = ystop > (int32_t)height ? ystop - (int32_t)height : 0;
    y = (int32_t)blockHeight - top - bottom;

    if(x0 >= 0 && xstop <= (int32_t)width) {
        for( ; top; top-- ) {
            FillRow1(ref, fill, left, x, right);
            fill += fillScanLength;
        }
        for( ; top; top-- ) {
            FillRow1(ref, fill, left, x, right);            
        }
        for( ; y; y-- ) {
            FillRow1(ref, fill, left, x, right);
            ref += width;
            fill += fillScanLength;
        }
    }
    else {
        for( ; top; top-- ) {
            h264bsdFillRow7(ref, fill, left, x, right);
            fill += fillScanLength;
        }
        for( ; top; top-- ) {
            h264bsdFillRow7(ref, fill, left, x, right);            
        }
        for( ; y; y-- ) {
            h264bsdFillRow7(ref, fill, left, x, right);
            ref += width;
            fill += fillScanLength;
        }
    }
    /* Top-overfilling */
    

    /* Lines inside reference image */
    

    ref -= width;

    /* Bottom-overfilling */
    for( ; bottom; bottom-- ) {
        //(*fp)(ref, fill, left, x, right);
        if(x0 >= 0 && xstop <= (int32_t)width)
            FillRow1(ref, fill, left, x, right);
        else
            h264bsdFillRow7(ref, fill, left, x, right);
        fill += fillScanLength;
    }
	}


static const uint32_t numClockTS[9] = {1,1,1,2,2,3,3,2,3};
static const uint32_t ceilLog2NumSliceGroups[9] = {0,1,1,2,2,3,3,3,3};

static uint32_t DecodeBufferingPeriod(strmData_t *pStrmData,
  seiBufferingPeriod_t *pBufferingPeriod,
  uint32_t cpbCnt,
  uint32_t initialCpbRemovalDelayLength,
  uint32_t nalHrdBpPresentFlag,
  uint32_t vclHrdBpPresentFlag);

static uint32_t DecodePictureTiming(strmData_t *pStrmData,
  seiPicTiming_t *pPicTiming,
  uint32_t cpbRemovalDelayLength,
  uint32_t dpbOutputDelayLength,
  uint32_t timeOffsetLength,
  uint32_t cpbDpbDelaysPresentFlag,
  uint32_t picStructPresentFlag);

static uint32_t DecodePanScanRectangle(strmData_t *pStrmData,
  seiPanScanRect_t *pPanScanRectangle);
static uint32_t DecodeFillerPayload(strmData_t *pStrmData, uint32_t payloadSize);
static uint32_t DecodeUserDataRegisteredITuTT35(strmData_t *pStrmData,
  seiUserDataRegisteredItuTT35_t *pUserDataRegisteredItuTT35,
  uint32_t payloadSize);
static uint32_t DecodeUserDataUnregistered(strmData_t *pStrmData,
  seiUserDataUnregistered_t *pUserDataUnregistered,
  uint32_t payloadSize);
static uint32_t DecodeRecoveryPoint(strmData_t *pStrmData,
  seiRecoveryPoint_t *pRecoveryPoint);
static uint32_t DecodeDecRefPicMarkingRepetition(strmData_t *pStrmData,
  seiDecRefPicMarkingRepetition_t *pDecRefPicMarkingRepetition,
  uint32_t numRefFrames);
static uint32_t DecodeSparePic(strmData_t *pStrmData,
  seiSparePic_t *pSparePic,uint32_t picSizeInMapUnits);
static uint32_t DecodeSceneInfo(strmData_t *pStrmData, seiSceneInfo_t *pSceneInfo);
static uint32_t DecodeSubSeqInfo(strmData_t *pStrmData, seiSubSeqInfo_t *pSubSeqInfo);
static uint32_t DecodeSubSeqLayerCharacteristics(strmData_t *pStrmData,
  seiSubSeqLayerCharacteristics_t *pSubSeqLayerCharacteristics);
static uint32_t DecodeSubSeqCharacteristics(strmData_t *pStrmData,
  seiSubSeqCharacteristics_t *pSubSeqCharacteristics);
static uint32_t DecodeFullFrameFreeze(strmData_t *pStrmData,
  seiFullFrameFreeze_t *pFullFrameFreeze);
static uint32_t DecodeFullFrameSnapshot(strmData_t *pStrmData,
  seiFullFrameSnapshot_t *pFullFrameSnapshot);
static uint32_t DecodeProgressiveRefinementSegmentStart(strmData_t *pStrmData,
  seiProgressiveRefinementSegmentStart_t *pProgressiveRefinementSegmentStart);
static uint32_t DecodeProgressiveRefinementSegmentEnd(strmData_t *pStrmData,
  seiProgressiveRefinementSegmentEnd_t *pProgressiveRefinementSegmentEnd);
static uint32_t DecodeMotionConstrainedSliceGroupSet(strmData_t *pStrmData,
  seiMotionConstrainedSliceGroupSet_t *pMotionConstrainedSliceGroupSet,
  uint32_t numSliceGroups);
static uint32_t DecodeReservedSeiMessage(
  strmData_t *pStrmData,
  seiReservedSeiMessage_t *pReservedSeiMessage,
  uint32_t payloadSize);

/*------------------------------------------------------------------------------

    Function: h264bsdDecodeSeiMessage

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeSeiMessage(strmData_t *pStrmData, seqParamSet_t *pSeqParamSet,
  seiMessage_t *pSeiMessage, uint32_t numSliceGroups) {
    uint32_t tmp, payloadType, payloadSize, status;


    ASSERT(pStrmData);
    ASSERT(pSeiMessage);


    memset(pSeiMessage, 0, sizeof(seiMessage_t));

    do{
        payloadType=0;
        while((tmp = h264bsdGetBits(pStrmData, 8)) == 0xFF)
          payloadType += 255;

        if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
        payloadType += tmp;

        payloadSize=0;
        while((tmp = h264bsdGetBits(pStrmData, 8)) == 0xFF)
          payloadSize += 255;

        if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
        payloadSize += tmp;

        pSeiMessage->payloadType = payloadType;

        switch(payloadType) {
          case 0:
              ASSERT(pSeqParamSet);
              status = DecodeBufferingPeriod(pStrmData,
                &pSeiMessage->bufferingPeriod,
                pSeqParamSet->vuiParameters->vclHrdParameters.cpbCnt,
                pSeqParamSet->vuiParameters->vclHrdParameters.initialCpbRemovalDelayLength,
                pSeqParamSet->vuiParameters->nalHrdParametersPresentFlag,
                pSeqParamSet->vuiParameters->vclHrdParametersPresentFlag);
              break;

          case 1:
              ASSERT(pSeqParamSet->vuiParametersPresentFlag);
              status = DecodePictureTiming(pStrmData,
                &pSeiMessage->picTiming,
                pSeqParamSet->vuiParameters->vclHrdParameters.cpbRemovalDelayLength,
                pSeqParamSet->vuiParameters->vclHrdParameters.dpbOutputDelayLength,
                pSeqParamSet->vuiParameters->vclHrdParameters.timeOffsetLength,
                pSeqParamSet->vuiParameters->nalHrdParametersPresentFlag ||
                pSeqParamSet->vuiParameters->vclHrdParametersPresentFlag ?
                HANTRO_TRUE : HANTRO_FALSE,
                pSeqParamSet->vuiParameters->picStructPresentFlag);
              break;

          case 2:
              status = DecodePanScanRectangle(pStrmData,&pSeiMessage->panScanRect);
              break;

          case 3:
              status = DecodeFillerPayload(pStrmData, payloadSize);
              break;

          case 4:
              status = DecodeUserDataRegisteredITuTT35(pStrmData,
                &pSeiMessage->userDataRegisteredItuTT35,payloadSize);
              break;

          case 5:
              status = DecodeUserDataUnregistered(pStrmData,
                &pSeiMessage->userDataUnregistered,payloadSize);
              break;

          case 6:
              status = DecodeRecoveryPoint(pStrmData,&pSeiMessage->recoveryPoint);
              break;

          case 7:
              status = DecodeDecRefPicMarkingRepetition(pStrmData,
                &pSeiMessage->decRefPicMarkingRepetition,
                pSeqParamSet->numRefFrames);
              break;

          case 8:
              ASSERT(pSeqParamSet);
              status = DecodeSparePic(pStrmData,&pSeiMessage->sparePic,
                pSeqParamSet->picWidthInMbs * pSeqParamSet->picHeightInMbs);
              break;

          case 9:
              status = DecodeSceneInfo(pStrmData,&pSeiMessage->sceneInfo);
              break;

          case 10:
              status = DecodeSubSeqInfo(pStrmData,&pSeiMessage->subSeqInfo);
              break;

          case 11:
              status = DecodeSubSeqLayerCharacteristics(pStrmData,
                &pSeiMessage->subSeqLayerCharacteristics);
              break;

          case 12:
              status = DecodeSubSeqCharacteristics(pStrmData,
                &pSeiMessage->subSeqCharacteristics);
              break;

          case 13:
              status = DecodeFullFrameFreeze(pStrmData,&pSeiMessage->fullFrameFreeze);
              break;

          case 14: /* This SEI does not contain data, what to do ??? */
              status = HANTRO_OK;
              break;

          case 15:
              status = DecodeFullFrameSnapshot(pStrmData,&pSeiMessage->fullFrameSnapshot);
              break;

          case 16:
              status = DecodeProgressiveRefinementSegmentStart(pStrmData,
                &pSeiMessage->progressiveRefinementSegmentStart);
              break;

          case 17:
              status = DecodeProgressiveRefinementSegmentEnd(pStrmData,
                &pSeiMessage->progressiveRefinementSegmentEnd);
              break;

          case 18:
              ASSERT(numSliceGroups);
              status = DecodeMotionConstrainedSliceGroupSet(pStrmData,
                &pSeiMessage->motionConstrainedSliceGroupSet,
                numSliceGroups);
              break;

          default:
              status = DecodeReservedSeiMessage(pStrmData,
                &pSeiMessage->reservedSeiMessage,payloadSize);
              break;
					}

        if(status != HANTRO_OK)
            return(status);

        while(!h264bsdIsByteAligned(pStrmData)) {
            if(h264bsdGetBits(pStrmData, 1) != 1)
                return(HANTRO_NOK);
            while(!h264bsdIsByteAligned(pStrmData)) {
                if(h264bsdGetBits(pStrmData, 1) != 0)
                    return(HANTRO_NOK);
            }
        }
			} while(h264bsdMoreRbspData(pStrmData));

  return(h264bsdRbspTrailingBits(pStrmData));
	}

/*------------------------------------------------------------------------------

    Function: DecodeBufferingPeriod

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeBufferingPeriod(strmData_t *pStrmData,
  seiBufferingPeriod_t *pBufferingPeriod,
  uint32_t cpbCnt,
  uint32_t initialCpbRemovalDelayLength,
  uint32_t nalHrdBpPresentFlag, uint32_t vclHrdBpPresentFlag) {
    uint32_t tmp, i;


    ASSERT(pStrmData);
    ASSERT(pBufferingPeriod);
    ASSERT(cpbCnt);
    ASSERT(initialCpbRemovalDelayLength);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pBufferingPeriod->seqParameterSetId);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pBufferingPeriod->seqParameterSetId > 31)
        return(HANTRO_NOK);

    if(nalHrdBpPresentFlag) {
        for(i=0; i < cpbCnt; i++) {
            tmp = h264bsdGetBits(pStrmData, initialCpbRemovalDelayLength);
            if(tmp == END_OF_STREAM)
                return(HANTRO_NOK);
            if(tmp == 0)
                return(HANTRO_NOK);
            pBufferingPeriod->initialCpbRemovalDelay[i] = tmp;

            tmp = h264bsdGetBits(pStrmData, initialCpbRemovalDelayLength);
            if(tmp == END_OF_STREAM)
                return(HANTRO_NOK);
            pBufferingPeriod->initialCpbRemovalDelayOffset[i] = tmp;
        }
    }

    if(vclHrdBpPresentFlag) {
        for(i=0; i < cpbCnt; i++) {
            tmp = h264bsdGetBits(pStrmData, initialCpbRemovalDelayLength);
            if(tmp == END_OF_STREAM)
                return(HANTRO_NOK);
            pBufferingPeriod->initialCpbRemovalDelay[i] = tmp;

            tmp = h264bsdGetBits(pStrmData, initialCpbRemovalDelayLength);
            if(tmp == END_OF_STREAM)
                return(HANTRO_NOK);
            pBufferingPeriod->initialCpbRemovalDelayOffset[i] = tmp;
        }
    }

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: DecodePictureTiming

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodePictureTiming(strmData_t *pStrmData,
  seiPicTiming_t *pPicTiming, uint32_t cpbRemovalDelayLength,
  uint32_t dpbOutputDelayLength, uint32_t timeOffsetLength,
  uint32_t cpbDpbDelaysPresentFlag, uint32_t picStructPresentFlag) {
    uint32_t tmp, i;
    int32_t itmp;


    ASSERT(pStrmData);
    ASSERT(pPicTiming);


    if(cpbDpbDelaysPresentFlag) {
        tmp = h264bsdGetBits(pStrmData, cpbRemovalDelayLength);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pPicTiming->cpbRemovalDelay = tmp;

        tmp = h264bsdGetBits(pStrmData, dpbOutputDelayLength);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pPicTiming->dpbOutputDelay = tmp;
			}

    if(picStructPresentFlag) {
        tmp = h264bsdGetBits(pStrmData, 4);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        if(tmp > 8)
            return(HANTRO_NOK);
        pPicTiming->picStruct = tmp;

        for(i=0; i < numClockTS[pPicTiming->picStruct]; i++) {
            tmp = h264bsdGetBits(pStrmData, 1);
            if(tmp == END_OF_STREAM)
                return(HANTRO_NOK);
            pPicTiming->clockTimeStampFlag[i] = tmp == 1 ?
                                    HANTRO_TRUE : HANTRO_FALSE;

            if(pPicTiming->clockTimeStampFlag[i]) {
                tmp = h264bsdGetBits(pStrmData, 2);
                if(tmp == END_OF_STREAM)
                    return(HANTRO_NOK);
                pPicTiming->ctType[i] = tmp;

                tmp = h264bsdGetBits(pStrmData, 1);
                if(tmp == END_OF_STREAM)
                    return(HANTRO_NOK);
                pPicTiming->nuitFieldBasedFlag[i] = tmp == 1 ?
                                    HANTRO_TRUE : HANTRO_FALSE;

                tmp = h264bsdGetBits(pStrmData, 5);
                if(tmp == END_OF_STREAM)
                    return(HANTRO_NOK);
                if(tmp > 6)
                    return(HANTRO_NOK);
                pPicTiming->countingType[i] = tmp;

                tmp = h264bsdGetBits(pStrmData, 1);
                if(tmp == END_OF_STREAM)
                    return(HANTRO_NOK);
                pPicTiming->fullTimeStampFlag[i] = tmp == 1 ?
                                    HANTRO_TRUE : HANTRO_FALSE;

                tmp = h264bsdGetBits(pStrmData, 1);
                if(tmp == END_OF_STREAM)
                    return(HANTRO_NOK);
                pPicTiming->discontinuityFlag[i] = tmp == 1 ?
                                    HANTRO_TRUE : HANTRO_FALSE;

                tmp = h264bsdGetBits(pStrmData, 1);
                if(tmp == END_OF_STREAM)
                    return(HANTRO_NOK);
                pPicTiming->cntDroppedFlag[i] = tmp == 1 ?
                                    HANTRO_TRUE : HANTRO_FALSE;

                tmp = h264bsdGetBits(pStrmData, 8);
                if(tmp == END_OF_STREAM)
                    return(HANTRO_NOK);
                pPicTiming->nFrames[i] = tmp;

                if(pPicTiming->fullTimeStampFlag[i])  {
                    tmp = h264bsdGetBits(pStrmData, 6);
                    if(tmp == END_OF_STREAM)
                        return(HANTRO_NOK);
                    if(tmp > 59)
                        return(HANTRO_NOK);
                    pPicTiming->secondsValue[i] = tmp;

                    tmp = h264bsdGetBits(pStrmData, 6);
                    if(tmp == END_OF_STREAM)
                        return(HANTRO_NOK);
                    if(tmp > 59)
                        return(HANTRO_NOK);
                    pPicTiming->minutesValue[i] = tmp;

                    tmp = h264bsdGetBits(pStrmData, 5);
                    if(tmp == END_OF_STREAM)
                        return(HANTRO_NOK);
                    if(tmp > 23)
                        return(HANTRO_NOK);
                    pPicTiming->hoursValue[i] = tmp;
                }
                else {
                    tmp = h264bsdGetBits(pStrmData, 1);
                    if(tmp == END_OF_STREAM)
                        return(HANTRO_NOK);
                    pPicTiming->secondsFlag[i] = tmp == 1 ?
                                    HANTRO_TRUE : HANTRO_FALSE;

                    if(pPicTiming->secondsFlag[i])  {
                        tmp = h264bsdGetBits(pStrmData, 6);
                        if(tmp == END_OF_STREAM)
                            return(HANTRO_NOK);
                        if(tmp > 59)
                            return(HANTRO_NOK);
                        pPicTiming->secondsValue[i] = tmp;

                        tmp = h264bsdGetBits(pStrmData, 1);
                        if(tmp == END_OF_STREAM)
                            return(HANTRO_NOK);
                        pPicTiming->minutesFlag[i] = tmp == 1 ?
                                    HANTRO_TRUE : HANTRO_FALSE;

                        if(pPicTiming->minutesFlag[i])      {
                            tmp = h264bsdGetBits(pStrmData, 6);
                            if(tmp == END_OF_STREAM)
                                return(HANTRO_NOK);
                            if(tmp > 59)
                                return(HANTRO_NOK);
                            pPicTiming->minutesValue[i] = tmp;

                            tmp = h264bsdGetBits(pStrmData, 1);
                            if(tmp == END_OF_STREAM)
                                return(HANTRO_NOK);
                            pPicTiming->hoursFlag[i] = tmp == 1 ?
                                    HANTRO_TRUE : HANTRO_FALSE;

                            if(pPicTiming->hoursFlag[i])          {
                                tmp = h264bsdGetBits(pStrmData, 5);
                                if(tmp == END_OF_STREAM)
                                    return(HANTRO_NOK);
                                if(tmp > 23)
                                    return(HANTRO_NOK);
                                pPicTiming->hoursValue[i] = tmp;
                            }
                        }
                    }
                }
                if(timeOffsetLength) {
                    tmp = h264bsdGetBits(pStrmData, timeOffsetLength);
                    if(tmp == END_OF_STREAM)
                        return(HANTRO_NOK);
                    itmp = (int32_t)tmp;
                    /* following "converts" timeOffsetLength-bit signed
                     * integer into int32_t */
                    /*lint -save -e701 -e702 */
                    itmp <<= (32 - timeOffsetLength);
                    itmp >>= (32 - timeOffsetLength);
                    /*lint -restore */
                    pPicTiming->timeOffset[i] = itmp;
                                    }
                else
                    pPicTiming->timeOffset[i]=0;
            }
        }
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodePanScanRectangle

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodePanScanRectangle(strmData_t *pStrmData,
  seiPanScanRect_t *pPanScanRectangle) {
    uint32_t tmp, i;


    ASSERT(pStrmData);
    ASSERT(pPanScanRectangle);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pPanScanRectangle->panScanRectId);
    if(tmp != HANTRO_OK)
        return tmp;

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pPanScanRectangle->panScanRectCancelFlag = tmp == 1 ?
                                HANTRO_TRUE : HANTRO_FALSE;

    if(!pPanScanRectangle->panScanRectCancelFlag) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
          &pPanScanRectangle->panScanCnt);
        if(tmp != HANTRO_OK)
            return tmp;
        if(pPanScanRectangle->panScanCnt > 2)
            return(HANTRO_NOK);
        pPanScanRectangle->panScanCnt++;

        for(i=0; i < pPanScanRectangle->panScanCnt; i++) {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData,
              &pPanScanRectangle->panScanRectLeftOffset[i]);
            if(tmp != HANTRO_OK)
                return tmp;

            tmp = h264bsdDecodeExpGolombSigned(pStrmData,
              &pPanScanRectangle->panScanRectRightOffset[i]);
            if(tmp != HANTRO_OK)
                return tmp;

            tmp = h264bsdDecodeExpGolombSigned(pStrmData,
              &pPanScanRectangle->panScanRectTopOffset[i]);
            if(tmp != HANTRO_OK)
                return tmp;

            tmp = h264bsdDecodeExpGolombSigned(pStrmData,
              &pPanScanRectangle->panScanRectBottomOffset[i]);
            if(tmp != HANTRO_OK)
                return tmp;
	        }
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
          &pPanScanRectangle->panScanRectRepetitionPeriod);
        if(tmp != HANTRO_OK)
            return tmp;
        if(pPanScanRectangle->panScanRectRepetitionPeriod > 16384)
            return(HANTRO_NOK);
        if(pPanScanRectangle->panScanCnt > 1 &&
          pPanScanRectangle->panScanRectRepetitionPeriod > 1)
            return(HANTRO_NOK);
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeFillerPayload

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeFillerPayload(strmData_t *pStrmData, uint32_t payloadSize) {

    ASSERT(pStrmData);


    if(payloadSize)
        if(h264bsdFlushBits(pStrmData, 8 * payloadSize) == END_OF_STREAM)
            return(HANTRO_NOK);

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: DecodeUserDataRegisteredITuTT35

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeUserDataRegisteredITuTT35(strmData_t *pStrmData,
  seiUserDataRegisteredItuTT35_t *pUserDataRegisteredItuTT35,
  uint32_t payloadSize) {
  uint32_t tmp, i, j;


    ASSERT(pStrmData);
    ASSERT(pUserDataRegisteredItuTT35);
    ASSERT(payloadSize);

    tmp = h264bsdGetBits(pStrmData, 8);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pUserDataRegisteredItuTT35->ituTT35CountryCode = tmp;

    if(pUserDataRegisteredItuTT35->ituTT35CountryCode != 0xFF)
        i = 1;
    else {
        tmp = h264bsdGetBits(pStrmData, 8);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pUserDataRegisteredItuTT35->ituTT35CountryCodeExtensionByte = tmp;
        i = 2;
    }

    /* where corresponding FREE() ??? */
    ALLOCATE(pUserDataRegisteredItuTT35->ituTT35PayloadByte,payloadSize-i,uint8_t);
    pUserDataRegisteredItuTT35->numPayloadBytes = payloadSize - i;
    if(pUserDataRegisteredItuTT35->ituTT35PayloadByte == NULL)
        return(MEMORY_ALLOCATION_ERROR);

    j=0;
    do{
        tmp = h264bsdGetBits(pStrmData, 8);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pUserDataRegisteredItuTT35->ituTT35PayloadByte[j] = (uint8_t)tmp;
        i++;
        j++;
			} while(i < payloadSize);

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeUserDataUnregistered

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeUserDataUnregistered(strmData_t *pStrmData,
  seiUserDataUnregistered_t *pUserDataUnregistered,
  uint32_t payloadSize) {
    uint32_t i, tmp;


    ASSERT(pStrmData);
    ASSERT(pUserDataUnregistered);


    for(i=0; i < 4; i++) {
        pUserDataUnregistered->uuidIsoIec11578[i] = h264bsdShowBits32(pStrmData);
        if(h264bsdFlushBits(pStrmData,32) == END_OF_STREAM)
            return(HANTRO_NOK);
			}

    /* where corresponding FREE() ??? */
    ALLOCATE(pUserDataUnregistered->userDataPayloadByte, payloadSize - 16, uint8_t);
    if(pUserDataUnregistered->userDataPayloadByte == NULL)
        return(MEMORY_ALLOCATION_ERROR);

    pUserDataUnregistered->numPayloadBytes = payloadSize - 16;

    for(i=0; i < payloadSize - 16; i++) {
      tmp = h264bsdGetBits(pStrmData, 8);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pUserDataUnregistered->userDataPayloadByte[i] = (uint8_t)tmp;
		  }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeRecoveryPoint

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeRecoveryPoint(strmData_t *pStrmData, seiRecoveryPoint_t *pRecoveryPoint) {
    uint32_t tmp;


    ASSERT(pStrmData);
    ASSERT(pRecoveryPoint);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
        &pRecoveryPoint->recoveryFrameCnt);
    if(tmp != HANTRO_OK)
        return tmp;

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pRecoveryPoint->exactMatchFlag = tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pRecoveryPoint->brokenLinkFlag = tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdGetBits(pStrmData, 2);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    if(tmp > 2)
        return(HANTRO_NOK);
    pRecoveryPoint->changingSliceGroupIdc = tmp;

    return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeDecRefPicMarkingRepetition

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeDecRefPicMarkingRepetition(strmData_t *pStrmData,
  seiDecRefPicMarkingRepetition_t *pDecRefPicMarkingRepetition,
  uint32_t numRefFrames) {
    uint32_t tmp;


    ASSERT(pStrmData);
    ASSERT(pDecRefPicMarkingRepetition);


    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pDecRefPicMarkingRepetition->originalIdrFlag = tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pDecRefPicMarkingRepetition->originalFrameNum);
    if(tmp != HANTRO_OK)
        return tmp;

    /* frame_mbs_only_flag assumed always true so some field related syntax
     * elements are skipped, see H.264 standard */
    // tmp = h264bsdDecRefPicMarking(pStrmData,
    //  &pDecRefPicMarkingRepetition->decRefPicMarking, NAL_SEI, numRefFrames);
    return(HANTRO_NOK);

    return tmp;

}

/*------------------------------------------------------------------------------

    Function: DecodeSparePic

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeSparePic(strmData_t *pStrmData, seiSparePic_t *pSparePic,
  uint32_t picSizeInMapUnits) {
    uint32_t tmp, i, j, mapUnitCnt;


    ASSERT(pStrmData);
    ASSERT(pSparePic);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
        &pSparePic->targetFrameNum);
    if(tmp != HANTRO_OK)
        return tmp;

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSparePic->spareFieldFlag = tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;
    /* do not accept fields */
    if(pSparePic->spareFieldFlag)
        return(HANTRO_NOK);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pSparePic->numSparePics);
    if(tmp != HANTRO_OK)
        return tmp;
    pSparePic->numSparePics++;
    if(pSparePic->numSparePics > MAX_NUM_SPARE_PICS)
        return(HANTRO_NOK);

    for(i=0; i < pSparePic->numSparePics; i++) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
          &pSparePic->deltaSpareFrameNum[i]);
        if(tmp != HANTRO_OK)
            return tmp;

        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &pSparePic->spareAreaIdc[i]);
        if(tmp != HANTRO_OK)
            return tmp;
        if(pSparePic->spareAreaIdc[i] > 2)
            return(HANTRO_NOK);

        if(pSparePic->spareAreaIdc[i] == 1) {
            /* where corresponding FREE() ??? */
            ALLOCATE(pSparePic->spareUnitFlag[i], picSizeInMapUnits, uint32_t);
            if(pSparePic->spareUnitFlag[i] == NULL)
                return(MEMORY_ALLOCATION_ERROR);
            pSparePic->zeroRunLength[i] = NULL;

            for(j=0; j < picSizeInMapUnits; j++) {
                tmp = h264bsdGetBits(pStrmData, 1);
                if(tmp == END_OF_STREAM)
                    return(HANTRO_NOK);
                pSparePic->spareUnitFlag[i][j] = tmp == 1 ?
                                    HANTRO_TRUE : HANTRO_FALSE;
            }
        }
        else if(pSparePic->spareAreaIdc[i] == 2) {
            /* where corresponding FREE() ??? */
            ALLOCATE(pSparePic->zeroRunLength[i], picSizeInMapUnits, uint32_t);
            if(pSparePic->zeroRunLength[i] == NULL)
                return(MEMORY_ALLOCATION_ERROR);
            pSparePic->spareUnitFlag[i] = NULL;

            for(j=0, mapUnitCnt=0; mapUnitCnt < picSizeInMapUnits; j++) {
                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
                  &pSparePic->zeroRunLength[i][j]);
                if(tmp != HANTRO_OK)
                    return tmp;
                mapUnitCnt += pSparePic->zeroRunLength[i][j] + 1;
            }
        }
    }

    /* set rest to null */
    for(i = pSparePic->numSparePics; i < MAX_NUM_SPARE_PICS; i++) {
        pSparePic->spareUnitFlag[i] = NULL;
        pSparePic->zeroRunLength[i] = NULL;
    }

    return(HANTRO_OK);

	}

/*------------------------------------------------------------------------------

    Function: DecodeSceneInfo

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeSceneInfo(strmData_t *pStrmData, seiSceneInfo_t *pSceneInfo) {
    uint32_t tmp;



    ASSERT(pStrmData);
    ASSERT(pSceneInfo);


    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSceneInfo->sceneInfoPresentFlag = tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

    if(pSceneInfo->sceneInfoPresentFlag) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pSceneInfo->sceneId);
        if(tmp != HANTRO_OK)
            return tmp;

        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
          &pSceneInfo->sceneTransitionType);
        if(tmp != HANTRO_OK)
            return tmp;
        if(pSceneInfo->sceneTransitionType > 6)
            return(HANTRO_NOK);

        if(pSceneInfo->sceneTransitionType) {
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
              &pSceneInfo->secondSceneId);
            if(tmp != HANTRO_OK)
                return tmp;
        }

    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeSubSeqInfo

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

-----------------------------------------------------------------------------*/
static uint32_t DecodeSubSeqInfo(strmData_t *pStrmData, seiSubSeqInfo_t *pSubSeqInfo) {
    uint32_t tmp;


    ASSERT(pStrmData);
    ASSERT(pSubSeqInfo);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
        &pSubSeqInfo->subSeqLayerNum);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pSubSeqInfo->subSeqLayerNum > 255)
        return(HANTRO_NOK);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pSubSeqInfo->subSeqId);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pSubSeqInfo->subSeqId > 65535)
        return(HANTRO_NOK);

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSubSeqInfo->firstRefPicFlag = tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSubSeqInfo->leadingNonRefPicFlag = tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSubSeqInfo->lastPicFlag = tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSubSeqInfo->subSeqFrameNumFlag = tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

    if(pSubSeqInfo->subSeqFrameNumFlag) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &pSubSeqInfo->subSeqFrameNum);
        if(tmp != HANTRO_OK)
            return tmp;
    }

    return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeSubSeqLayerCharacteristics

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeSubSeqLayerCharacteristics(strmData_t *pStrmData,
  seiSubSeqLayerCharacteristics_t *pSubSeqLayerCharacteristics) {
  uint32_t tmp, i;


    ASSERT(pStrmData);
    ASSERT(pSubSeqLayerCharacteristics);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pSubSeqLayerCharacteristics->numSubSeqLayers);
    if(tmp != HANTRO_OK)
        return tmp;
    pSubSeqLayerCharacteristics->numSubSeqLayers++;
    if(pSubSeqLayerCharacteristics->numSubSeqLayers > MAX_NUM_SUB_SEQ_LAYERS)
        return(HANTRO_NOK);

    for(i=0; i < pSubSeqLayerCharacteristics->numSubSeqLayers; i++) {
      tmp = h264bsdGetBits(pStrmData, 1);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pSubSeqLayerCharacteristics->accurateStatisticsFlag[i] =
          tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

      tmp = h264bsdGetBits(pStrmData, 16);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pSubSeqLayerCharacteristics->averageBitRate[i] = tmp;

      tmp = h264bsdGetBits(pStrmData, 16);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pSubSeqLayerCharacteristics->averageFrameRate[i] = tmp;
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeSubSeqCharacteristics

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeSubSeqCharacteristics(strmData_t *pStrmData,
  seiSubSeqCharacteristics_t *pSubSeqCharacteristics) {
    uint32_t tmp, i;


    ASSERT(pStrmData);
    ASSERT(pSubSeqCharacteristics);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pSubSeqCharacteristics->subSeqLayerNum);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pSubSeqCharacteristics->subSeqLayerNum > MAX_NUM_SUB_SEQ_LAYERS-1)
        return(HANTRO_NOK);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
        &pSubSeqCharacteristics->subSeqId);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pSubSeqCharacteristics->subSeqId > 65535)
        return(HANTRO_NOK);

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSubSeqCharacteristics->durationFlag = tmp == 1 ?
                            HANTRO_TRUE : HANTRO_FALSE;

    if(pSubSeqCharacteristics->durationFlag) {
        pSubSeqCharacteristics->subSeqDuration = h264bsdShowBits32(pStrmData);
        if(h264bsdFlushBits(pStrmData,32) == END_OF_STREAM)
            return(HANTRO_NOK);
    }

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSubSeqCharacteristics->averageRateFlag = tmp == 1 ?
                            HANTRO_TRUE : HANTRO_FALSE;

    if(pSubSeqCharacteristics->averageRateFlag) {
        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pSubSeqCharacteristics->accurateStatisticsFlag =
            tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

        tmp = h264bsdGetBits(pStrmData, 16);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pSubSeqCharacteristics->averageBitRate = tmp;

        tmp = h264bsdGetBits(pStrmData, 16);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pSubSeqCharacteristics->averageFrameRate = tmp;
    }

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pSubSeqCharacteristics->numReferencedSubseqs);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pSubSeqCharacteristics->numReferencedSubseqs > MAX_NUM_SUB_SEQ_LAYERS-1)
        return(HANTRO_NOK);

    for(i=0; i < pSubSeqCharacteristics->numReferencedSubseqs; i++) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
          &pSubSeqCharacteristics->refSubSeqLayerNum[i]);
        if(tmp != HANTRO_OK)
            return tmp;

        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
          &pSubSeqCharacteristics->refSubSeqId[i]);
        if(tmp != HANTRO_OK)
            return tmp;

        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pSubSeqCharacteristics->refSubSeqDirection[i] = tmp;
			}

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeFullFrameFreeze

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeFullFrameFreeze(strmData_t *pStrmData,
  seiFullFrameFreeze_t *pFullFrameFreeze) {
    uint32_t tmp;


    ASSERT(pStrmData);
    ASSERT(pFullFrameFreeze);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pFullFrameFreeze->fullFrameFreezeRepetitionPeriod);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pFullFrameFreeze->fullFrameFreezeRepetitionPeriod > 16384)
        return(HANTRO_NOK);

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: DecodeFullFrameSnapshot

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeFullFrameSnapshot(strmData_t *pStrmData,
  seiFullFrameSnapshot_t *pFullFrameSnapshot) {
  uint32_t tmp;


    ASSERT(pStrmData);
    ASSERT(pFullFrameSnapshot);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pFullFrameSnapshot->snapShotId);
    if(tmp != HANTRO_OK)
        return tmp;

    return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeProgressiveRefinementSegmentStart

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeProgressiveRefinementSegmentStart(strmData_t *pStrmData,
  seiProgressiveRefinementSegmentStart_t *pProgressiveRefinementSegmentStart) {
    uint32_t tmp;


    ASSERT(pStrmData);
    ASSERT(pProgressiveRefinementSegmentStart);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pProgressiveRefinementSegmentStart->progressiveRefinementId);
    if(tmp != HANTRO_OK)
        return tmp;

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pProgressiveRefinementSegmentStart->numRefinementSteps);
    if(tmp != HANTRO_OK)
        return tmp;
    pProgressiveRefinementSegmentStart->numRefinementSteps++;

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: DecodeProgressiveRefinementSegmentEnd

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeProgressiveRefinementSegmentEnd(strmData_t *pStrmData,
  seiProgressiveRefinementSegmentEnd_t *pProgressiveRefinementSegmentEnd) {
  uint32_t tmp;



    ASSERT(pStrmData);
    ASSERT(pProgressiveRefinementSegmentEnd);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pProgressiveRefinementSegmentEnd->progressiveRefinementId);
    if(tmp != HANTRO_OK)
        return tmp;

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: DecodeMotionConstrainedSliceGroupSet

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeMotionConstrainedSliceGroupSet(strmData_t *pStrmData,
  seiMotionConstrainedSliceGroupSet_t *pMotionConstrainedSliceGroupSet,
  uint32_t numSliceGroups) {
  uint32_t tmp,i;



    ASSERT(pStrmData);
    ASSERT(pMotionConstrainedSliceGroupSet);
    ASSERT(numSliceGroups < MAX_NUM_SLICE_GROUPS);


    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pMotionConstrainedSliceGroupSet->numSliceGroupsInSet);
    if(tmp != HANTRO_OK)
        return tmp;
    pMotionConstrainedSliceGroupSet->numSliceGroupsInSet++;
    if(pMotionConstrainedSliceGroupSet->numSliceGroupsInSet > numSliceGroups)
        return(HANTRO_NOK);

    for(i=0; i < pMotionConstrainedSliceGroupSet->numSliceGroupsInSet; i++) {
      tmp = h264bsdGetBits(pStrmData,
          ceilLog2NumSliceGroups[numSliceGroups]);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pMotionConstrainedSliceGroupSet->sliceGroupId[i] = tmp;
      if(pMotionConstrainedSliceGroupSet->sliceGroupId[i] >
        pMotionConstrainedSliceGroupSet->numSliceGroupsInSet-1)
          return(HANTRO_NOK);
			}

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pMotionConstrainedSliceGroupSet->exactSampleValueMatchFlag =
        tmp == 1 ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pMotionConstrainedSliceGroupSet->panScanRectFlag = tmp == 1 ?
                                        HANTRO_TRUE : HANTRO_FALSE;

    if(pMotionConstrainedSliceGroupSet->panScanRectFlag) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
          &pMotionConstrainedSliceGroupSet->panScanRectId);
        if(tmp != HANTRO_OK)
            return tmp;
		  }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeReservedSeiMessage

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/
static uint32_t DecodeReservedSeiMessage(strmData_t *pStrmData,
  seiReservedSeiMessage_t *pReservedSeiMessage,
  uint32_t payloadSize) {
  uint32_t i, tmp;


  ASSERT(pStrmData);
  ASSERT(pReservedSeiMessage);


  /* where corresponding FREE() ??? */
  ALLOCATE(pReservedSeiMessage->reservedSeiMessagePayloadByte,payloadSize,uint8_t);
  if(!pReservedSeiMessage->reservedSeiMessagePayloadByte)
      return(MEMORY_ALLOCATION_ERROR);

  pReservedSeiMessage->numPayloadBytes = payloadSize;

  for(i=0; i < payloadSize; i++) {
    tmp = h264bsdGetBits(pStrmData,8);
    if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
    pReservedSeiMessage->reservedSeiMessagePayloadByte[i] = (uint8_t)tmp;
		}

  return(HANTRO_OK);
	}



/* enumeration to indicate invalid return value from the GetDpbSize function */
enum {INVALID_DPB_SIZE=0x7FFFFFFF};

static uint32_t GetDpbSize(uint32_t picSizeInMbs, uint32_t levelIdc);

/*------------------------------------------------------------------------------

    Function name: h264bsdDecodeSeqParamSet

        Functional description:
            Decode sequence parameter set information from the stream.

            Function allocates memory for offsetForRefFrame array if
            picture order count type is 1 and numRefFramesInPicOrderCntCycle
            is greater than zero.

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            pSeqParamSet    decoded information is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      failure, invalid information or end of stream
            MEMORY_ALLOCATION_ERROR for memory allocation failure

		https://crosvm.dev/doc/cros_codecs/decoders/h264/parser/struct.Pps.html

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeSeqParamSet(strmData_t *pStrmData, seqParamSet_t *pSeqParamSet) {
  uint32_t tmp, i, value;


  ASSERT(pStrmData);
  ASSERT(pSeqParamSet);

  memset(pSeqParamSet, 0, sizeof(seqParamSet_t));

  /* profile_idc */
  tmp = h264bsdGetBits(pStrmData, 8);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  DEBUGP2("Profile %d", tmp);
  if(tmp != 66) 
    EPRINT2("NOT BASELINE PROFILE %d", tmp);

  pSeqParamSet->profileIdc = tmp;

  /* constrained_set0_flag */
  tmp = h264bsdGetBits(pStrmData, 1);
  /* constrained_set1_flag */
  tmp = h264bsdGetBits(pStrmData, 1);
  /* constrained_set2_flag */
  tmp = h264bsdGetBits(pStrmData, 1);

  /* constrained_set3-5_flag */
  tmp = h264bsdGetBits(pStrmData, 1);
  tmp = h264bsdGetBits(pStrmData, 1);
  tmp = h264bsdGetBits(pStrmData, 1);

  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);

  /* (reserved_zero_5bits, values of these bits shall be ignored  DICE che sarebbero altri 3 constraint e poi 2 reserved=0*/
  tmp = h264bsdGetBits(pStrmData, 2);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  if(tmp)
    EPRINT2("RESERVED ZERO bits %d", tmp);

  tmp = h264bsdGetBits(pStrmData, 8);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  pSeqParamSet->levelIdc = tmp;
	
/*
Baseline profile_idc = 66=0x42, constraint_set1_flag=0
Constrained Baseline profile_idc = 66=0x42, constraint_set1_flag = 1
Main profile_idc = 77=0x4D
Extended profile_idc = 88=0x58
High profile_idc = 100=0x64*/

  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,&pSeqParamSet->seqParameterSetId);
  if(tmp != HANTRO_OK)
    return tmp;
  if(pSeqParamSet->seqParameterSetId >= MAX_NUM_SEQ_PARAM_SETS) {
    EPRINT("seq_param_set_id");
    return(HANTRO_NOK);
    }

	if(pSeqParamSet->profileIdc==66) {		// file di test...
		}
	// https://membrane.stream/learn/h264/5
	else {		// 100, DVR
		tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);		// CHROMA FORMAT IDC 
		if(value==3)
		  tmp = h264bsdGetBits(pStrmData, 1);		//  separate_colour_plane_flag

		tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);		// BIT DEPTH LUMA
		tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);		// BIT DEPTH CHROMA

		tmp = h264bsdGetBits(pStrmData, 1);		//  qpprime_y_zero_transform_bypass_flag
		tmp = h264bsdGetBits(pStrmData, 1);		//  seq_scaling_matrix_present_flag
		if(tmp) {
			for(i=0; i<8; i++) {
				tmp = h264bsdGetBits(pStrmData, 1);		//  
				if(tmp) {
					if(i<6) {
						int lastScale=8,nextScale=8,delta_scale;
						int value;
						for(int j=0; j<16; j++) {		// https://stackoverflow.com/questions/79208659/h-264-sps-parsing-for-frame-dimension
							if(nextScale) {
								tmp = h264bsdDecodeExpGolombSigned(pStrmData, &value);
								delta_scale=value;
								nextScale=(lastScale+delta_scale+256) & 0xff;
								pSeqParamSet->scalingList[j]=nextScale == 0 ? lastScale : nextScale;
								lastScale=pSeqParamSet->scalingList[j];
								}
							}
						}
					else {
						int lastScale=8,nextScale=8,delta_scale;
						int value;
						for(int j=0; j<64; j++) {
							if(nextScale) {
								tmp = h264bsdDecodeExpGolombSigned(pStrmData, &value);
								delta_scale=value;
								nextScale=(lastScale+delta_scale+256) & 0xff;
								pSeqParamSet->scalingList[j]=nextScale == 0 ? lastScale : nextScale;
								lastScale=pSeqParamSet->scalingList[j];
								}
							}
						}
					}
				}
			/*         for( i=0; i < ( ( chroma_format_idc != 3 ) ? 8 : 12 ); i++ )	{	
             seq_scaling_list_present_flag[ i ]	u(1)
             if( seq_scaling_list_present_flag[ i ] )	
                 if( i < 6 )	
                     scaling_list( ScalingList4x4[ i ], 16, UseDefaultScalingMatrix4x4Flag[ i ] )	
                 else	
                     scaling_list( ScalingList8x8[ i - 6 ], 64, UseDefaultScalingMatrix8x8Flag[ i - 6 ] )	
         }*/
			}
		}


  /* log2_max_frame_num_minus4 */
  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
  if(tmp != HANTRO_OK)
    return tmp;
  if(value > 12) {
    EPRINT("log2_max_frame_num_minus4");
    return(HANTRO_NOK);
    }
  /* maxFrameNum = 2^(log2_max_frame_num_minus4 + 4) */
  pSeqParamSet->maxFrameNum = 1 << (value+4);

  /* valid POC types are 0, 1 and 2 */
  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
  if(tmp != HANTRO_OK)
    return tmp;
  if(value > 2) {
    EPRINT("pic_order_cnt_type");
    return(HANTRO_NOK);
    }
  pSeqParamSet->picOrderCntType = value;

  if(pSeqParamSet->picOrderCntType == 0) {
    /* log2_max_pic_order_cnt_lsb_minus4 */
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if(tmp != HANTRO_OK)
      return tmp;
    if(value > 12) {
      EPRINT("log2_max_pic_order_cnt_lsb_minus4");
      return(HANTRO_NOK);
      }
    /* maxPicOrderCntLsb = 2^(log2_max_pic_order_cnt_lsb_minus4 + 4) */
    pSeqParamSet->maxPicOrderCntLsb = 1 << (value+4);
    }
  else if(pSeqParamSet->picOrderCntType == 1) {
    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSeqParamSet->deltaPicOrderAlwaysZeroFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdDecodeExpGolombSigned(pStrmData,&pSeqParamSet->offsetForNonRefPic);
    if(tmp != HANTRO_OK)
        return tmp;

    tmp = h264bsdDecodeExpGolombSigned(pStrmData,&pSeqParamSet->offsetForTopToBottomField);
    if(tmp != HANTRO_OK)
      return tmp;

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
        &pSeqParamSet->numRefFramesInPicOrderCntCycle);
    if(tmp != HANTRO_OK)
      return tmp;
    if(pSeqParamSet->numRefFramesInPicOrderCntCycle > 255) {
      EPRINT("num_ref_frames_in_pic_order_cnt_cycle");
      return(HANTRO_NOK);
      }

    if(pSeqParamSet->numRefFramesInPicOrderCntCycle) {
      /* NOTE: This has to be freed somewhere! */
      ALLOCATE(pSeqParamSet->offsetForRefFrame, pSeqParamSet->numRefFramesInPicOrderCntCycle, int32_t);
      if(!pSeqParamSet->offsetForRefFrame)
        return(MEMORY_ALLOCATION_ERROR);

      for(i=0; i < pSeqParamSet->numRefFramesInPicOrderCntCycle; i++) {
        tmp =  h264bsdDecodeExpGolombSigned(pStrmData, pSeqParamSet->offsetForRefFrame + i);
        if(tmp != HANTRO_OK)
          return tmp;
        }
	    }
    else
      pSeqParamSet->offsetForRefFrame = NULL;
		}

  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pSeqParamSet->numRefFrames);
  if(tmp != HANTRO_OK)
    return tmp;
  if(pSeqParamSet->numRefFrames > MAX_NUM_REF_PICS) {
    EPRINT("num_ref_frames");
    return(HANTRO_NOK);
		}

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  pSeqParamSet->gapsInFrameNumValueAllowedFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
  if(tmp != HANTRO_OK)
    return tmp;
  pSeqParamSet->picWidthInMbs = value + 1;

  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
  if(tmp != HANTRO_OK)
    return tmp;
  pSeqParamSet->picHeightInMbs = value + 1;

  DEBUGP2("Width %d", pSeqParamSet->picWidthInMbs*MACROBLOCK_SIZE);
  DEBUGP2("Height %d", pSeqParamSet->picHeightInMbs*MACROBLOCK_SIZE);


  /* frame_mbs_only_flag, shall be 1 for baseline profile */
  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  if(!tmp) {
    EPRINT("frame_mbs_only_flag");
    return(HANTRO_NOK);
		}

  /* direct_8x8_inference_flag */
  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  pSeqParamSet->frameCroppingFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  if(pSeqParamSet->frameCroppingFlag) {
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,&pSeqParamSet->frameCropLeftOffset);
    if(tmp != HANTRO_OK)
      return tmp;
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,&pSeqParamSet->frameCropRightOffset);
    if(tmp != HANTRO_OK)
      return tmp;
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,&pSeqParamSet->frameCropTopOffset);
    if(tmp != HANTRO_OK)
      return tmp;
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,&pSeqParamSet->frameCropBottomOffset);
    if(tmp != HANTRO_OK)
      return tmp;

    /* check that frame cropping params are valid, parameters shall
     * specify non-negative area within the original picture */
    if(( (int32_t)pSeqParamSet->frameCropLeftOffset >
         ( 8 * (int32_t)pSeqParamSet->picWidthInMbs -
           ((int32_t)pSeqParamSet->frameCropRightOffset + 1))) ||
       ((int32_t)pSeqParamSet->frameCropTopOffset >
         ( 8 * (int32_t)pSeqParamSet->picHeightInMbs -
           ((int32_t)pSeqParamSet->frameCropBottomOffset + 1)))) {
      EPRINT("frame_cropping");
      return(HANTRO_NOK);
			}
		}

  /* check that image dimensions and levelIdc match */
  tmp = pSeqParamSet->picWidthInMbs * pSeqParamSet->picHeightInMbs;
  value = GetDpbSize(tmp, pSeqParamSet->levelIdc);
  if(value == INVALID_DPB_SIZE || pSeqParamSet->numRefFrames > value) {
    DEBUGP("WARNING! Invalid DPB size based on SPS Level!");
    DEBUGP2("WARNING! Using num_ref_frames =%d for DPB size!",pSeqParamSet->numRefFrames);
    value = pSeqParamSet->numRefFrames;
		}
  pSeqParamSet->maxDpbSize = value;

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
    return(HANTRO_NOK);
  pSeqParamSet->vuiParametersPresentFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  /* VUI */
  if(pSeqParamSet->vuiParametersPresentFlag) {
    ALLOCATE(pSeqParamSet->vuiParameters, 1, vuiParameters_t);
    if(pSeqParamSet->vuiParameters == NULL)
        return(MEMORY_ALLOCATION_ERROR);
    tmp = h264bsdDecodeVuiParameters(pStrmData,
        pSeqParamSet->vuiParameters);
    if(tmp != HANTRO_OK)
        return tmp;
    /* check numReorderFrames and maxDecFrameBuffering */
    if(pSeqParamSet->vuiParameters->bitstreamRestrictionFlag) {
      if(pSeqParamSet->vuiParameters->numReorderFrames >
            pSeqParamSet->vuiParameters->maxDecFrameBuffering ||
        pSeqParamSet->vuiParameters->maxDecFrameBuffering <
            pSeqParamSet->numRefFrames ||
        pSeqParamSet->vuiParameters->maxDecFrameBuffering >
            pSeqParamSet->maxDpbSize)
        return(HANTRO_NOK);


      /* standard says that "the sequence shall not require a DPB with
       * size of more than max(1, maxDecFrameBuffering) */
      pSeqParamSet->maxDpbSize = MAX(1, pSeqParamSet->vuiParameters->maxDecFrameBuffering);
      }
		}

  tmp = h264bsdRbspTrailingBits(pStrmData);

  /* ignore possible errors in trailing bits of parameters sets */
  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: GetDpbSize

        Functional description:
            Get size of the DPB in frames. Size is determined based on the
            picture size and MaxDPB for the specified level. These determine
            how many pictures may fit into to the buffer. However, the size
            is also limited to a maximum of 16 frames and therefore function
            returns the minimum of the determined size and 16.

        Inputs:
            picSizeInMbs    number of macroblocks in the picture
            levelIdc        indicates the level

        Outputs:
            none

        Returns:
            size of the DPB in frames
            INVALID_DPB_SIZE when invalid levelIdc specified or picSizeInMbs
            is higher than supported by the level in question

------------------------------------------------------------------------------*/
uint32_t GetDpbSize(uint32_t picSizeInMbs, uint32_t levelIdc) {
  uint32_t tmp;
  uint32_t maxPicSizeInMbs;


  ASSERT(picSizeInMbs);

  /* use tmp as the size of the DPB in bytes, computes as 1024 * MaxDPB
   * (from table A-1 in Annex A) */
  switch(levelIdc) {
    case 10:
      tmp = 152064;
      maxPicSizeInMbs = 99;
      break;
    case 11:
      tmp = 345600;
      maxPicSizeInMbs = 396;
      break;
    case 12:
      tmp = 912384;
      maxPicSizeInMbs = 396;
      break;
    case 13:
      tmp = 912384;
      maxPicSizeInMbs = 396;
      break;
    case 20:
      tmp = 912384;
      maxPicSizeInMbs = 396;
      break;
    case 21:
      tmp = 1824768;
      maxPicSizeInMbs = 792;
      break;
    case 22:
      tmp = 3110400;
      maxPicSizeInMbs = 1620;
      break;
    case 30:
      tmp = 3110400;
      maxPicSizeInMbs = 1620;
      break;
    case 31:
      tmp = 6912000;
      maxPicSizeInMbs = 3600;
      break;
    case 32:
      tmp = 7864320;
      maxPicSizeInMbs = 5120;
      break;
    case 40:
      tmp = 12582912;
      maxPicSizeInMbs = 8192;
      break;
    case 41:
      tmp = 12582912;
      maxPicSizeInMbs = 8192;
      break;
    case 42:
      tmp = 34816*384;
      maxPicSizeInMbs = 8704;
      break;
    case 50:
      /* standard says 42301440 here, but corrigendum "corrects" this to
       * 42393600 */
      tmp = 42393600;
      maxPicSizeInMbs = 22080;
      break;
    case 51:
      tmp = 70778880;
      maxPicSizeInMbs = 36864;
      break;
    default:
      return(INVALID_DPB_SIZE);
	  }

  /* this is not "correct" return value! However, it results in error in
   * decoding and this was easiest place to check picture size */
  if(picSizeInMbs > maxPicSizeInMbs)
      return(INVALID_DPB_SIZE);

  tmp /= (picSizeInMbs*384);

  return(MIN(tmp, 16));
	}

/*------------------------------------------------------------------------------

    Function name: h264bsdCompareSeqParamSets

        Functional description:
            Compare two sequence parameter sets.

        Inputs:
            pSps1   pointer to a sequence parameter set
            pSps2   pointer to another sequence parameter set

        Outputs:
            0       sequence parameter sets are equal
            1       otherwise

------------------------------------------------------------------------------*/
uint32_t h264bsdCompareSeqParamSets(seqParamSet_t *pSps1, seqParamSet_t *pSps2) {
    uint32_t i;


    ASSERT(pSps1);
    ASSERT(pSps2);

    /* first compare parameters whose existence does not depend on other
     * parameters and only compare the rest of the params if these are equal */
    if(pSps1->profileIdc        == pSps2->profileIdc &&
        pSps1->levelIdc          == pSps2->levelIdc &&
        pSps1->maxFrameNum       == pSps2->maxFrameNum &&
        pSps1->picOrderCntType   == pSps2->picOrderCntType &&
        pSps1->numRefFrames      == pSps2->numRefFrames &&
        pSps1->gapsInFrameNumValueAllowedFlag ==
            pSps2->gapsInFrameNumValueAllowedFlag &&
        pSps1->picWidthInMbs     == pSps2->picWidthInMbs &&
        pSps1->picHeightInMbs    == pSps2->picHeightInMbs &&
        pSps1->frameCroppingFlag == pSps2->frameCroppingFlag &&
        pSps1->vuiParametersPresentFlag == pSps2->vuiParametersPresentFlag) {
        if(pSps1->picOrderCntType == 0) {
            if(pSps1->maxPicOrderCntLsb != pSps2->maxPicOrderCntLsb)
                return 1;
	        }
        else if(pSps1->picOrderCntType == 1) {
            if(pSps1->deltaPicOrderAlwaysZeroFlag !=
                    pSps2->deltaPicOrderAlwaysZeroFlag ||
                pSps1->offsetForNonRefPic != pSps2->offsetForNonRefPic ||
                pSps1->offsetForTopToBottomField !=
                    pSps2->offsetForTopToBottomField ||
                pSps1->numRefFramesInPicOrderCntCycle !=
                    pSps2->numRefFramesInPicOrderCntCycle)
                return 1;

            else {
                for(i=0; i < pSps1->numRefFramesInPicOrderCntCycle; i++)
                    if(pSps1->offsetForRefFrame[i] !=
                        pSps2->offsetForRefFrame[i])
                        return 1;

            }
		      }
        if(pSps1->frameCroppingFlag) {
            if(pSps1->frameCropLeftOffset   != pSps2->frameCropLeftOffset ||
                pSps1->frameCropRightOffset  != pSps2->frameCropRightOffset ||
                pSps1->frameCropTopOffset    != pSps2->frameCropTopOffset ||
                pSps1->frameCropBottomOffset != pSps2->frameCropBottomOffset) {
                return 1;
            }
			    }

    return 0;
    }

  return 1;
	}


static void SetMbParams(mbStorage_t *pMb, sliceHeader_t *pSlice, uint32_t sliceId,
    int32_t chromaQpIndexOffset);

/*------------------------------------------------------------------------------

   5.1  Function name: h264bsdDecodeSliceData

        Functional description:
            Decode one slice. Function decodes stream data, i.e. macroblocks
            and possible skip_run fields. h264bsdDecodeMacroblock function is
            called to handle all other macroblock related processing.
            Macroblock to slice group mapping is considered when next
            macroblock to process is determined (h264bsdNextMbAddress function)
            map

        Inputs:
            pStrmData       pointer to stream data structure
            pStorage        pointer to storage structure
            currImage       pointer to current processed picture, needed for
                            intra prediction of the macroblocks
            pSliceHeader    pointer to slice header of the current slice

        Outputs:
            currImage       processed macroblocks are written to current image
            pStorage        mbStorage structure of each processed macroblock
                            is updated here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeSliceData(strmData_t *pStrmData, storage_t *pStorage,
    image_t *currImage, sliceHeader_t *pSliceHeader) {
    uint8_t mbData[384 + 15 + 32];
    uint8_t *data;
    uint32_t tmp;
    uint32_t skipRun;
    uint32_t prevSkipped;
    uint32_t currMbAddr;
    uint32_t moreMbs;
    uint32_t mbCount;
    int32_t qpY;
    macroblockLayer_t *mbLayer;



    ASSERT(pStrmData);
    ASSERT(pSliceHeader);
    ASSERT(pStorage);
    ASSERT(pSliceHeader->firstMbInSlice < pStorage->picSizeInMbs);

    /* ensure 16-byte alignment */
    data = (uint8_t*)ALIGN(mbData, 16);

    mbLayer = pStorage->mbLayer;

    currMbAddr = pSliceHeader->firstMbInSlice;
    skipRun=0;
    prevSkipped = HANTRO_FALSE;

    /* increment slice index, will be one for decoding of the first slice of
     * the picture */
    pStorage->slice->sliceId++;

    /* lastMbAddr stores address of the macroblock that was last successfully
     * decoded, needed for error handling */
    pStorage->slice->lastMbAddr=0;

    mbCount=0;
    /* initial quantization parameter for the slice is obtained as the sum of
     * initial QP for the picture and sliceQpDelta for the current slice */
    qpY = (int32_t)pStorage->activePps->picInitQp + pSliceHeader->sliceQpDelta;
    do {
        /* primary picture and already decoded macroblock -> error */
        if(!pSliceHeader->redundantPicCnt && pStorage->mb[currMbAddr].decoded) {
          EPRINT("Primary and already decoded");
          return(HANTRO_NOK);
		      }

        SetMbParams(pStorage->mb + currMbAddr, pSliceHeader,
            pStorage->slice->sliceId, pStorage->activePps->chromaQpIndexOffset);

        if(!IS_I_SLICE(pSliceHeader->sliceType)) {
          if(!prevSkipped) {
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &skipRun);
            if(tmp != HANTRO_OK)
                return tmp;
            /* skip_run shall be less than or equal to number of
             * macroblocks left */
            if(skipRun > (pStorage->picSizeInMbs - currMbAddr)) {
              EPRINT("skip_run");
              return(HANTRO_NOK);
							}
            if(skipRun) {
              prevSkipped = HANTRO_TRUE;
              memset(&mbLayer->mbPred, 0, sizeof(mbPred_t));
              /* mark current macroblock skipped */
              mbLayer->mbType = P_Skip;
							}
            }
				  }

        if(skipRun) {
          DEBUGP2("Skipping macroblock %d", currMbAddr);
          skipRun--;
			    }
        else {
          prevSkipped = HANTRO_FALSE;
          tmp = h264bsdDecodeMacroblockLayer(pStrmData, mbLayer,
            pStorage->mb + currMbAddr, pSliceHeader->sliceType,
            pSliceHeader->numRefIdxL0Active);
          if(tmp != HANTRO_OK) {
            EPRINT("macroblock_layer");
            return tmp;
            }
	        }

        tmp = h264bsdDecodeMacroblock(pStorage->mb + currMbAddr, mbLayer,
            currImage, pStorage->dpb, &qpY, currMbAddr,
            pStorage->activePps->constrainedIntraPredFlag, data);
        if(tmp != HANTRO_OK) {
          EPRINT("MACRO_BLOCK");
          return tmp;
		      }

        /* increment macroblock count only for macroblocks that were decoded
         * for the first time (redundant slices) */
        if(pStorage->mb[currMbAddr].decoded == 1)
          mbCount++;

        /* keep on processing as long as there is stream data left or
         * processing of macroblocks to be skipped based on the last skipRun is
         * not finished */
        moreMbs = (h264bsdMoreRbspData(pStrmData) || skipRun) ? HANTRO_TRUE : HANTRO_FALSE;

        /* lastMbAddr is only updated for intra slices (all macroblocks of
         * inter slices will be lost in case of an error) */
        if(IS_I_SLICE(pSliceHeader->sliceType))
          pStorage->slice->lastMbAddr = currMbAddr;

        currMbAddr = h264bsdNextMbAddress(pStorage->sliceGroupMap,
            pStorage->picSizeInMbs, currMbAddr);
        /* data left in the buffer but no more macroblocks for current slice
         * group -> error */
        if(moreMbs && !currMbAddr) {
          EPRINT("Next mb address");
          return(HANTRO_NOK);
					}

		  } while(moreMbs);

    if((pStorage->slice->numDecodedMbs + mbCount) > pStorage->picSizeInMbs) {
      EPRINT("Num decoded mbs");
      return(HANTRO_NOK);
	    }

    pStorage->slice->numDecodedMbs += mbCount;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

   5.2  Function: SetMbParams

        Functional description:
            Set macroblock parameters that remain constant for this slice

        Inputs:
            pSlice      pointer to current slice header
            sliceId     id of the current slice
            chromaQpIndexOffset

        Outputs:
            pMb         pointer to macroblock structure which is updated

        Returns:
            none

------------------------------------------------------------------------------*/
void SetMbParams(mbStorage_t *pMb, sliceHeader_t *pSlice, uint32_t sliceId,
    int32_t chromaQpIndexOffset) {
  uint32_t tmp1;
  int32_t tmp2, tmp3;



  tmp1 = pSlice->disableDeblockingFilterIdc;
  tmp2 = pSlice->sliceAlphaC0Offset;
  tmp3 = pSlice->sliceBetaOffset;
  pMb->sliceId = sliceId;
  pMb->disableDeblockingFilterIdc = tmp1;
  pMb->filterOffsetA = tmp2;
  pMb->filterOffsetB = tmp3;
  pMb->chromaQpIndexOffset = chromaQpIndexOffset;
	}

/*------------------------------------------------------------------------------

   5.3  Function name: h264bsdMarkSliceCorrupted

        Functional description:
            Mark macroblocks of the slice corrupted. If lastMbAddr in the slice
            storage is set -> picWidhtInMbs (or at least 10) macroblocks back
            from  the lastMbAddr are marked corrupted. However, if lastMbAddr
            is not set -> all macroblocks of the slice are marked.

        Inputs:
            pStorage        pointer to storage structure
            firstMbInSlice  address of the first macroblock in the slice, this
                            identifies the slice to be marked corrupted

        Outputs:
            pStorage        mbStorage for the corrupted macroblocks updated

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdMarkSliceCorrupted(storage_t *pStorage, uint32_t firstMbInSlice) {
    uint32_t tmp, i;
    uint32_t sliceId;
    uint32_t currMbAddr;


    ASSERT(pStorage);
    ASSERT(firstMbInSlice < pStorage->picSizeInMbs);

    currMbAddr = firstMbInSlice;

    sliceId = pStorage->slice->sliceId;

    /* DecodeSliceData sets lastMbAddr for I slices -> if it was set, go back
     * MAX(picWidthInMbs, 10) macroblocks and start marking from there */
    if(pStorage->slice->lastMbAddr) {
        ASSERT(pStorage->mb[pStorage->slice->lastMbAddr].sliceId == sliceId);
        i = pStorage->slice->lastMbAddr - 1;
        tmp=0;
        while(i > currMbAddr) {
          if(pStorage->mb[i].sliceId == sliceId) {
              tmp++;
              if(tmp >= MAX(pStorage->activeSps->picWidthInMbs, 10))
                  break;
            }
          i--;
					}
        currMbAddr = i;
			}

    do {
        if((pStorage->mb[currMbAddr].sliceId == sliceId) && (pStorage->mb[currMbAddr].decoded))
          pStorage->mb[currMbAddr].decoded--;
        else
          break;

        currMbAddr = h264bsdNextMbAddress(pStorage->sliceGroupMap,
            pStorage->picSizeInMbs, currMbAddr);

		  } while(currMbAddr);

	}



static void DecodeInterleavedMap(uint32_t *map,
  uint32_t numSliceGroups, uint32_t *runLength,
  uint32_t picSize);
static void DecodeDispersedMap(uint32_t *map,
  uint32_t numSliceGroups,
  uint32_t picWidth,uint32_t picHeight);
static void DecodeForegroundLeftOverMap(uint32_t *map,
  uint32_t numSliceGroups,
  uint32_t *topLeft,uint32_t *bottomRight,
  uint32_t picWidth,uint32_t picHeight);
static void DecodeBoxOutMap(uint32_t *map,
  uint32_t sliceGroupChangeDirectionFlag,
  uint32_t unitsInSliceGroup0,
  uint32_t picWidth,uint32_t picHeight);
static void DecodeRasterScanMap(uint32_t *map,
  uint32_t sliceGroupChangeDirectionFlag,
  uint32_t sizeOfUpperLeftGroup,
  uint32_t picSize);
static void DecodeWipeMap(uint32_t *map,
  uint32_t sliceGroupChangeDirectionFlag,
  uint32_t sizeOfUpperLeftGroup,
  uint32_t picWidth,uint32_t picHeight);

/*------------------------------------------------------------------------------

    Function: DecodeInterleavedMap

        Functional description:
            Function to decode interleaved slice group map type, i.e. slice
            group map type 0.

        Inputs:
            map             pointer to the map
            numSliceGroups  number of slice groups
            runLength       run_length[] values for each slice group
            picSize         picture size in macroblocks

        Outputs:
            map             slice group map is stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void DecodeInterleavedMap(uint32_t *map, uint32_t numSliceGroups, uint32_t *runLength, uint32_t picSize) {
  uint32_t i,j, group;


  ASSERT(map);
  ASSERT(numSliceGroups >= 1 && numSliceGroups <= MAX_NUM_SLICE_GROUPS);
  ASSERT(runLength);

  i=0;
  do {
    for(group=0; group < numSliceGroups && i < picSize; i += runLength[group++]) {
        ASSERT(runLength[group] <= picSize);
        for(j=0; j < runLength[group] && i + j < picSize; j++)
            map[i+j] = group;
      }
		} while(i < picSize);

	}

/*------------------------------------------------------------------------------

    Function: DecodeDispersedMap

        Functional description:
            Function to decode dispersed slice group map type, i.e. slice
            group map type 1.

        Inputs:
            map               pointer to the map
            numSliceGroups    number of slice groups
            picWidth          picture width in macroblocks
            picHeight         picture height in macroblocks

        Outputs:
            map               slice group map is stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void DecodeDispersedMap(uint32_t *map, uint32_t numSliceGroups,
  uint32_t picWidth,uint32_t picHeight) {
  uint32_t i, picSize;


  ASSERT(map);
  ASSERT(numSliceGroups >= 1 && numSliceGroups <= MAX_NUM_SLICE_GROUPS);
  ASSERT(picWidth);
  ASSERT(picHeight);

  picSize = picWidth * picHeight;

  for(i=0; i < picSize; i++)
    map[i] = ((i % picWidth) + (((i / picWidth) * numSliceGroups) >> 1)) %  numSliceGroups;
	}

/*------------------------------------------------------------------------------

    Function: DecodeForegroundLeftOverMap

        Functional description:
            Function to decode foreground with left-over slice group map type,
            i.e. slice group map type 2.

        Inputs:
            map               pointer to the map
            numSliceGroups    number of slice groups
            topLeft           top_left[] values
            bottomRight       bottom_right[] values
            picWidth          picture width in macroblocks
            picHeight         picture height in macroblocks

        Outputs:
            map               slice group map is stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void DecodeForegroundLeftOverMap(uint32_t *map,
  uint32_t numSliceGroups,
  uint32_t *topLeft,uint32_t *bottomRight,
  uint32_t picWidth,uint32_t picHeight) {

  uint32_t i,y,x,yTopLeft,yBottomRight,xTopLeft,xBottomRight, picSize;
  uint32_t group;


  ASSERT(map);
  ASSERT(numSliceGroups >= 1 && numSliceGroups <= MAX_NUM_SLICE_GROUPS);
  ASSERT(topLeft);
  ASSERT(bottomRight);
  ASSERT(picWidth);
  ASSERT(picHeight);

  picSize = picWidth * picHeight;

  for(i=0; i < picSize; i++)
      map[i] = numSliceGroups - 1;

  for(group = numSliceGroups - 1; group--; ) {
    ASSERT(topLeft[group] <= bottomRight[group] && bottomRight[group] < picSize );
    yTopLeft = topLeft[group] / picWidth;
    xTopLeft = topLeft[group] % picWidth;
    yBottomRight = bottomRight[group] / picWidth;
    xBottomRight = bottomRight[group] % picWidth;
    ASSERT(xTopLeft <= xBottomRight);

    for(y = yTopLeft; y <= yBottomRight; y++)
      for(x = xTopLeft; x <= xBottomRight; x++)
          map[ y * picWidth + x ] = group;
    }

	}

/*------------------------------------------------------------------------------

    Function: DecodeBoxOutMap

        Functional description:
            Function to decode box-out slice group map type, i.e. slice group
            map type 3.

        Inputs:
            map                               pointer to the map
            sliceGroupChangeDirectionFlag     slice_group_change_direction_flag
            unitsInSliceGroup0                mbs on slice group 0
            picWidth                          picture width in macroblocks
            picHeight                         picture height in macroblocks

        Outputs:
            map                               slice group map is stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void DecodeBoxOutMap(uint32_t *map,
  uint32_t sliceGroupChangeDirectionFlag,
  uint32_t unitsInSliceGroup0,
  uint32_t picWidth, uint32_t picHeight) {
  uint32_t i, k, picSize;
  int32_t x, y, xDir, yDir, leftBound, topBound, rightBound, bottomBound;
  uint32_t mapUnitVacant;


  ASSERT(map);
  ASSERT(picWidth);
  ASSERT(picHeight);

  picSize = picWidth * picHeight;
  ASSERT(unitsInSliceGroup0 <= picSize);

  for(i=0; i < picSize; i++)
      map[i] = 1;

  x = (picWidth - (uint32_t)sliceGroupChangeDirectionFlag) >> 1;
  y = (picHeight - (uint32_t)sliceGroupChangeDirectionFlag) >> 1;

  leftBound = x;
  topBound = y;

  rightBound = x;
  bottomBound = y;

  xDir = (int32_t)sliceGroupChangeDirectionFlag - 1;
  yDir = (int32_t)sliceGroupChangeDirectionFlag;

  for(k=0; k < unitsInSliceGroup0; k += mapUnitVacant ? 1 : 0) {
      mapUnitVacant = (map[ (uint32_t)y * picWidth + (uint32_t)x ] == 1) ?
                                      HANTRO_TRUE : HANTRO_FALSE;

      if(mapUnitVacant)
          map[ (uint32_t)y * picWidth + (uint32_t)x ]=0;

      if(xDir == -1 && x == leftBound) {
          leftBound = MAX(leftBound - 1, 0);
          x = leftBound;
          xDir=0;
          yDir = 2 * (int32_t)sliceGroupChangeDirectionFlag - 1;
				}
      else if(xDir == 1 && x == rightBound) {
          rightBound = MIN(rightBound + 1, (int32_t)picWidth - 1);
          x = rightBound;
          xDir=0;
          yDir = 1 - 2 * (int32_t)sliceGroupChangeDirectionFlag;
				}
      else if(yDir == -1 && y == topBound) {
          topBound = MAX(topBound - 1, 0);
          y = topBound;
          xDir = 1 - 2 * (int32_t)sliceGroupChangeDirectionFlag;
          yDir=0;
				}
      else if(yDir == 1 && y == bottomBound) {
          bottomBound = MIN(bottomBound + 1, (int32_t)picHeight - 1);
          y = bottomBound;
          xDir = 2 * (int32_t)sliceGroupChangeDirectionFlag - 1;
          yDir=0;
				}
      else {
          x += xDir;
          y += yDir;
				}
    }

	}

/*------------------------------------------------------------------------------

    Function: DecodeRasterScanMap

        Functional description:
            Function to decode raster scan slice group map type, i.e. slice
            group map type 4.

        Inputs:
            map                               pointer to the map
            sliceGroupChangeDirectionFlag     slice_group_change_direction_flag
            sizeOfUpperLeftGroup              mbs in upperLeftGroup
            picSize                           picture size in macroblocks

        Outputs:
            map                               slice group map is stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void DecodeRasterScanMap(uint32_t *map, uint32_t sliceGroupChangeDirectionFlag,
  uint32_t sizeOfUpperLeftGroup, uint32_t picSize) {
  uint32_t i;


  ASSERT(map);
  ASSERT(picSize);
  ASSERT(sizeOfUpperLeftGroup <= picSize);

  for(i=0; i < picSize; i++)
      if(i < sizeOfUpperLeftGroup)
          map[i] = (uint32_t)sliceGroupChangeDirectionFlag;
      else
          map[i] = 1 - (uint32_t)sliceGroupChangeDirectionFlag;

	}

/*------------------------------------------------------------------------------

    Function: DecodeWipeMap

        Functional description:
            Function to decode wipe slice group map type, i.e. slice group map
            type 5.

        Inputs:
            sliceGroupChangeDirectionFlag     slice_group_change_direction_flag
            sizeOfUpperLeftGroup              mbs in upperLeftGroup
            picWidth                          picture width in macroblocks
            picHeight                         picture height in macroblocks

        Outputs:
            map                               slice group map is stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void DecodeWipeMap(uint32_t *map,
  uint32_t sliceGroupChangeDirectionFlag,
  uint32_t sizeOfUpperLeftGroup,
  uint32_t picWidth, uint32_t picHeight) {
    uint32_t i,j,k;


    ASSERT(map);
    ASSERT(picWidth);
    ASSERT(picHeight);
    ASSERT(sizeOfUpperLeftGroup <= picWidth * picHeight);

    k=0;
    for(j=0; j < picWidth; j++)
        for(i=0; i < picHeight; i++)
            if(k++ < sizeOfUpperLeftGroup)
                map[ i * picWidth + j ] = (uint32_t)sliceGroupChangeDirectionFlag;
            else
                map[ i * picWidth + j ] = 1 -
                    (uint32_t)sliceGroupChangeDirectionFlag;


	}

/*------------------------------------------------------------------------------

    Function: h264bsdDecodeSliceGroupMap

        Functional description:
            Function to decode macroblock to slice group map. Construction
            of different slice group map types is handled by separate
            functions defined above. See standard for details how slice group
            maps are computed.

        Inputs:
            pps                     active picture parameter set
            sliceGroupChangeCycle   slice_group_change_cycle
            picWidth                picture width in macroblocks
            picHeight               picture height in macroblocks

        Outputs:
            map                     slice group map is stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdDecodeSliceGroupMap(uint32_t *map, picParamSet_t *pps,
  uint32_t sliceGroupChangeCycle, uint32_t picWidth, uint32_t picHeight) {
    uint32_t i, picSize, unitsInSliceGroup0=0, sizeOfUpperLeftGroup=0;


    ASSERT(map);
    ASSERT(pps);
    ASSERT(picWidth);
    ASSERT(picHeight);
    ASSERT(pps->sliceGroupMapType < 7);

    picSize = picWidth * picHeight;

    /* just one slice group -> all macroblocks belong to group 0 */
    if(pps->numSliceGroups == 1) {
        memset(map, 0, picSize * sizeof(uint32_t));
        return;
			}

    if(pps->sliceGroupMapType > 2 && pps->sliceGroupMapType < 6) {
        ASSERT(pps->sliceGroupChangeRate &&
               pps->sliceGroupChangeRate <= picSize);

        unitsInSliceGroup0 =
            MIN(sliceGroupChangeCycle * pps->sliceGroupChangeRate, picSize);

        if(pps->sliceGroupMapType == 4 || pps->sliceGroupMapType == 5)
            sizeOfUpperLeftGroup = pps->sliceGroupChangeDirectionFlag ?
                (picSize - unitsInSliceGroup0) : unitsInSliceGroup0;
	    }

    switch(pps->sliceGroupMapType) {
      case 0:
          DecodeInterleavedMap(map, pps->numSliceGroups,
            pps->runLength, picSize);
          break;
      case 1:
          DecodeDispersedMap(map, pps->numSliceGroups, picWidth,
            picHeight);
          break;
      case 2:
          DecodeForegroundLeftOverMap(map, pps->numSliceGroups,
            pps->topLeft, pps->bottomRight, picWidth, picHeight);
          break;
      case 3:
          DecodeBoxOutMap(map, pps->sliceGroupChangeDirectionFlag,
            unitsInSliceGroup0, picWidth, picHeight);
          break;
      case 4:
          DecodeRasterScanMap(map,
            pps->sliceGroupChangeDirectionFlag, sizeOfUpperLeftGroup,
            picSize);
          break;
      case 5:
          DecodeWipeMap(map, pps->sliceGroupChangeDirectionFlag,
            sizeOfUpperLeftGroup, picWidth, picHeight);
          break;
      default:
        ASSERT(pps->sliceGroupId);
        for(i=0; i < picSize; i++) {
          ASSERT(pps->sliceGroupId[i] < pps->numSliceGroups);
          map[i] = pps->sliceGroupId[i];
	        }
        break;
    }

	}



static uint32_t RefPicListReordering(strmData_t *, refPicListReordering_t *,
    uint32_t, uint32_t);
static uint32_t NumSliceGroupChangeCycleBits(uint32_t picSizeInMbs,
    uint32_t sliceGroupChangeRate);
static uint32_t DecRefPicMarking(strmData_t *pStrmData,
    decRefPicMarking_t *pDecRefPicMarking, nalUnitType_e nalUnitType,
    uint32_t numRefFrames);


/*------------------------------------------------------------------------------

    Function name: h264bsdDecodeSliceHeader

        Functional description:
            Decode slice header data from the stream.

        Inputs:
            pStrmData       pointer to stream data structure
            pSeqParamSet    pointer to active sequence parameter set
            pPicParamSet    pointer to active picture parameter set
            pNalUnit        pointer to current NAL unit structure

        Outputs:
            pSliceHeader    decoded data is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data or end of stream

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeSliceHeader(strmData_t *pStrmData, sliceHeader_t *pSliceHeader,
    seqParamSet_t *pSeqParamSet, picParamSet_t *pPicParamSet,
    nalUnit_t *pNalUnit) {
    uint32_t tmp, i, value;
    int32_t itmp;
    uint32_t picSizeInMbs;


    ASSERT(pStrmData);
    ASSERT(pSliceHeader);
    ASSERT(pSeqParamSet);
    ASSERT(pPicParamSet);
    ASSERT( pNalUnit->nalUnitType == NAL_CODED_SLICE ||
            pNalUnit->nalUnitType == NAL_CODED_SLICE_IDR );


    memset(pSliceHeader, 0, sizeof(sliceHeader_t));

    picSizeInMbs = pSeqParamSet->picWidthInMbs * pSeqParamSet->picHeightInMbs;
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;
    pSliceHeader->firstMbInSlice = value;
    if(value >= picSizeInMbs) {
      EPRINT("first_mb_in_slice");
      return(HANTRO_NOK);
			}

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;
    pSliceHeader->sliceType = value;
    /* slice type has to be either I or P slice. P slice is not allowed when
     * current NAL unit is an IDR NAL unit or num_ref_frames is 0 */
    if( !IS_I_SLICE(pSliceHeader->sliceType) &&
       ( !IS_P_SLICE(pSliceHeader->sliceType) ||
         IS_IDR_NAL_UNIT(pNalUnit) ||
         !pSeqParamSet->numRefFrames )) {
      EPRINT("slice_type");
      return(HANTRO_NOK);
			}

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;
    pSliceHeader->picParameterSetId = value;
    if(pSliceHeader->picParameterSetId != pPicParamSet->picParameterSetId) {
      EPRINT("pic_parameter_set_id");
      return(HANTRO_NOK);
			}

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i=0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    tmp = h264bsdGetBits(pStrmData, i);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    if(IS_IDR_NAL_UNIT(pNalUnit) && tmp != 0) {
        EPRINT("frame_num");
        return(HANTRO_NOK);
	    }
    pSliceHeader->frameNum = tmp;

    if(IS_IDR_NAL_UNIT(pNalUnit)) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK)
            return tmp;
        pSliceHeader->idrPicId = value;
        if(value > 65535) {
            EPRINT("idr_pic_id");
            return(HANTRO_NOK);
        }
		  }

    if(pSeqParamSet->picOrderCntType == 0) {
        /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
        i=0;
        while(pSeqParamSet->maxPicOrderCntLsb >> i)
            i++;
        i--;

        tmp = h264bsdGetBits(pStrmData, i);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pSliceHeader->picOrderCntLsb = tmp;

        if(pPicParamSet->picOrderPresentFlag) {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if(tmp != HANTRO_OK)
                return tmp;
            pSliceHeader->deltaPicOrderCntBottom = itmp;
					}

        /* check that picOrderCnt for IDR picture will be zero. See
         * DecodePicOrderCnt function to understand the logic here */
        if( IS_IDR_NAL_UNIT(pNalUnit) &&
             ((pSliceHeader->picOrderCntLsb >
                pSeqParamSet->maxPicOrderCntLsb/2) ||
                MIN((int32_t)pSliceHeader->picOrderCntLsb,
                    (int32_t)pSliceHeader->picOrderCntLsb +
                    pSliceHeader->deltaPicOrderCntBottom) != 0 )) {
            return(HANTRO_NOK);
        }
			}

    if((pSeqParamSet->picOrderCntType == 1) &&
         !pSeqParamSet->deltaPicOrderAlwaysZeroFlag ) {
        tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
        if(tmp != HANTRO_OK)
            return tmp;
        pSliceHeader->deltaPicOrderCnt[0] = itmp;

        if(pPicParamSet->picOrderPresentFlag) {
          tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
          if(tmp != HANTRO_OK)
              return tmp;
          pSliceHeader->deltaPicOrderCnt[1] = itmp;
					}

        /* check that picOrderCnt for IDR picture will be zero. See
         * DecodePicOrderCnt function to understand the logic here */
        if(IS_IDR_NAL_UNIT(pNalUnit) &&
             MIN(pSliceHeader->deltaPicOrderCnt[0],
                 pSliceHeader->deltaPicOrderCnt[0] +
                 pSeqParamSet->offsetForTopToBottomField +
                 pSliceHeader->deltaPicOrderCnt[1]) != 0)
            return(HANTRO_NOK);
			}

    if(pPicParamSet->redundantPicCntPresentFlag) {
      tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
      if(tmp != HANTRO_OK)
          return tmp;
      pSliceHeader->redundantPicCnt = value;
      if(value > 127) {
        EPRINT("redundant_pic_cnt");
        return(HANTRO_NOK);
        }
			}

    if(IS_P_SLICE(pSliceHeader->sliceType)) {
      tmp = h264bsdGetBits(pStrmData, 1);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pSliceHeader->numRefIdxActiveOverrideFlag = tmp;

      if(pSliceHeader->numRefIdxActiveOverrideFlag) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK)
            return tmp;
        if(value > 15) {
          EPRINT("num_ref_idx_l0_active_minus1");
          return(HANTRO_NOK);
          }
        pSliceHeader->numRefIdxL0Active = value + 1;
        }
        /* set numRefIdxL0Active from pic param set */
        else {
            /* if value (minus1) in picture parameter set exceeds 15 it should
             * have been overridden here */
            if(pPicParamSet->numRefIdxL0Active > 16) {
              EPRINT("num_ref_idx_active_override_flag");
              return(HANTRO_NOK);
					    }
            pSliceHeader->numRefIdxL0Active = pPicParamSet->numRefIdxL0Active;
				  }
			}

    if(IS_P_SLICE(pSliceHeader->sliceType)) {
        tmp = RefPicListReordering(pStrmData,
            &pSliceHeader->refPicListReordering,
            pSliceHeader->numRefIdxL0Active,
            pSeqParamSet->maxFrameNum);
        if(tmp != HANTRO_OK)
            return tmp;
			}

    if(pNalUnit->nalRefIdc != 0) {
        tmp = DecRefPicMarking(pStrmData, &pSliceHeader->decRefPicMarking,
            pNalUnit->nalUnitType, pSeqParamSet->numRefFrames);
        if(tmp != HANTRO_OK)
            return tmp;
			}

    /* decode sliceQpDelta and check that initial QP for the slice will be on
     * the range [0, 51] */
    tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
    if(tmp != HANTRO_OK)
        return tmp;
    pSliceHeader->sliceQpDelta = itmp;
    itmp += (int32_t)pPicParamSet->picInitQp;
    if((itmp < 0) || (itmp > 51)) {
      EPRINT("slice_qp_delta");
      return(HANTRO_NOK);
		  }

    if(pPicParamSet->deblockingFilterControlPresentFlag) {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK)
            return tmp;
        pSliceHeader->disableDeblockingFilterIdc = value;
        if(pSliceHeader->disableDeblockingFilterIdc > 2) {
          EPRINT("disable_deblocking_filter_idc");
          return(HANTRO_NOK);
					}

        if(pSliceHeader->disableDeblockingFilterIdc != 1) {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if(tmp != HANTRO_OK)
                return tmp;
            if((itmp < -6) || (itmp > 6)) {
              EPRINT("slice_alpha_c0_offset_div2");
              return(HANTRO_NOK);
					    }
            pSliceHeader->sliceAlphaC0Offset = itmp * 2;

            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if(tmp != HANTRO_OK)
                return tmp;
            if((itmp < -6) || (itmp > 6)) {
               EPRINT("slice_beta_offset_div2");
               return(HANTRO_NOK);
				      }
            pSliceHeader->sliceBetaOffset = itmp * 2;
					}
			}

    if((pPicParamSet->numSliceGroups > 1) &&
         (pPicParamSet->sliceGroupMapType >= 3) &&
         (pPicParamSet->sliceGroupMapType <= 5)) {
        /* set tmp to number of bits used to represent slice_group_change_cycle
         * in the stream */
        tmp = NumSliceGroupChangeCycleBits(picSizeInMbs,
            pPicParamSet->sliceGroupChangeRate);
        value = h264bsdGetBits(pStrmData, tmp);
        if(value == END_OF_STREAM)
            return(HANTRO_NOK);
        pSliceHeader->sliceGroupChangeCycle = value;

        /* corresponds to tmp = Ceil(picSizeInMbs / sliceGroupChangeRate) */
        tmp = (picSizeInMbs + pPicParamSet->sliceGroupChangeRate - 1) /
              pPicParamSet->sliceGroupChangeRate;
        if(pSliceHeader->sliceGroupChangeCycle > tmp) {
          EPRINT("slice_group_change_cycle");
          return(HANTRO_NOK);
        }
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: NumSliceGroupChangeCycleBits

        Functional description:
            Determine number of bits needed to represent
            slice_group_change_cycle in the stream. The standard states that
            slice_group_change_cycle is represented by
                Ceil( Log2((picSizeInMbs / sliceGroupChangeRate) + 1))

            bits. Division "/" in the equation is non-truncating division.

        Inputs:
            picSizeInMbs            picture size in macroblocks
            sliceGroupChangeRate

        Outputs:
            none

        Returns:
            number of bits needed

------------------------------------------------------------------------------*/
uint32_t NumSliceGroupChangeCycleBits(uint32_t picSizeInMbs, uint32_t sliceGroupChangeRate) {
    uint32_t tmp,numBits,mask;


    ASSERT(picSizeInMbs);
    ASSERT(sliceGroupChangeRate);
    ASSERT(sliceGroupChangeRate <= picSizeInMbs);

    /* compute (picSizeInMbs / sliceGroupChangeRate + 1), rounded up */
    if(picSizeInMbs % sliceGroupChangeRate)
        tmp = 2 + picSizeInMbs/sliceGroupChangeRate;
    else
        tmp = 1 + picSizeInMbs/sliceGroupChangeRate;

    numBits=0;
    mask = ~0U;

    /* set numBits to position of right-most non-zero bit */
    while(tmp & (mask<<++numBits))
        ;
    numBits--;

    /* add one more bit if value greater than 2^numBits */
    if(tmp & ((1<<numBits)-1))
        numBits++;

    return(numBits);
	}

/*------------------------------------------------------------------------------

    Function: RefPicListReordering

        Functional description:
            Decode reference picture list reordering syntax elements from
            the stream. Max number of reordering commands is numRefIdxActive.

        Inputs:
            pStrmData       pointer to stream data structure
            numRefIdxActive number of active reference indices to be used for
                            current slice
            maxPicNum       maxFrameNum from the active SPS

        Outputs:
            pRefPicListReordering   decoded data is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t RefPicListReordering(strmData_t *pStrmData,
  refPicListReordering_t *pRefPicListReordering, uint32_t numRefIdxActive,
  uint32_t maxPicNum) {
  uint32_t tmp, value, i;
  uint32_t command;



  ASSERT(pStrmData);
  ASSERT(pRefPicListReordering);
  ASSERT(numRefIdxActive);
  ASSERT(maxPicNum);


  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);

  pRefPicListReordering->refPicListReorderingFlagL0 = tmp;

  if(pRefPicListReordering->refPicListReorderingFlagL0) {
      i=0;

      do {
        if(i > numRefIdxActive) {
          EPRINT("Too many reordering commands");
          return(HANTRO_NOK);
          }

          tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &command);
          if(tmp != HANTRO_OK)
              return tmp;
          if(command > 3) {
            EPRINT("reordering_of_pic_nums_idc");
            return(HANTRO_NOK);
						}

          pRefPicListReordering->command[i].reorderingOfPicNumsIdc = command;

          if((command == 0) || (command == 1)) {
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if(tmp != HANTRO_OK)
                return tmp;
            if(value >= maxPicNum) {
              EPRINT("abs_diff_pic_num_minus1");
              return(HANTRO_NOK);
              }
              pRefPicListReordering->command[i].absDiffPicNum = value + 1;
                          }
          else if(command == 2) {
              tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
              if(tmp != HANTRO_OK)
                  return tmp;
              pRefPicListReordering->command[i].longTermPicNum = value;
                          }
          i++;
				} while(command != 3);

      /* there shall be at least one reordering command if
       * refPicListReorderingFlagL0 was set */
      if(i == 1) {
          EPRINT("ref_pic_list_reordering");
          return(HANTRO_NOK);
      }
		}

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecRefPicMarking

        Functional description:
            Decode decoded reference picture marking syntax elements from
            the stream.

        Inputs:
            pStrmData       pointer to stream data structure
            nalUnitType     type of the current NAL unit
            numRefFrames    max number of reference frames from the active SPS

        Outputs:
            pDecRefPicMarking   decoded data is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t DecRefPicMarking(strmData_t *pStrmData,
    decRefPicMarking_t *pDecRefPicMarking, nalUnitType_e nalUnitType,
    uint32_t numRefFrames) {
    uint32_t tmp, value;
    uint32_t i;
    uint32_t operation;
    /* variables for error checking purposes, store number of memory
     * management operations of certain type */
    uint32_t num4=0, num5=0, num6=0, num1to3=0;


    ASSERT( nalUnitType == NAL_CODED_SLICE_IDR ||
            nalUnitType == NAL_CODED_SLICE ||
            nalUnitType == NAL_SEI );


    if(nalUnitType == NAL_CODED_SLICE_IDR) {
        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pDecRefPicMarking->noOutputOfPriorPicsFlag = tmp;

        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pDecRefPicMarking->longTermReferenceFlag = tmp;
        if(!numRefFrames && pDecRefPicMarking->longTermReferenceFlag) {
            EPRINT("long_term_reference_flag");
            return(HANTRO_NOK);
        }
	    }
    else {
        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pDecRefPicMarking->adaptiveRefPicMarkingModeFlag = tmp;
        if(pDecRefPicMarking->adaptiveRefPicMarkingModeFlag) {
            i=0;
            do {
              /* see explanation of the MAX_NUM_MMC_OPERATIONS in
               * slice_header.h */
              if(i > (2 * numRefFrames + 2)) {
                  EPRINT("Too many management operations");
                  return(HANTRO_NOK);
								}

              tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &operation);
              if(tmp != HANTRO_OK)
                  return tmp;
              if(operation > 6) {
                  EPRINT("memory_management_control_operation");
                  return(HANTRO_NOK);
								}

              pDecRefPicMarking->operation[i].memoryManagementControlOperation = operation;
              if((operation == 1) || (operation == 3)) {
                  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                  if(tmp != HANTRO_OK)
                      return tmp;
                  pDecRefPicMarking->operation[i].differenceOfPicNums = value + 1;
								}
              if(operation == 2) {
                  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                  if(tmp != HANTRO_OK)
                      return tmp;
                  pDecRefPicMarking->operation[i].longTermPicNum = value;
								}
              if((operation == 3) || (operation == 6)) {
                  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                  if(tmp != HANTRO_OK)
                      return tmp;
                  pDecRefPicMarking->operation[i].longTermFrameIdx = value;
		            }
              if(operation == 4) {
                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                if(tmp != HANTRO_OK)
                    return tmp;
                /* value shall be in range [0, numRefFrames] */
                if(value > numRefFrames)  {
                  EPRINT("max_long_term_frame_idx_plus1");
                  return(HANTRO_NOK);
			            }
                if(value == 0)
                  pDecRefPicMarking->operation[i].maxLongTermFrameIdx = NO_LONG_TERM_FRAME_INDICES;
                else
                  pDecRefPicMarking->operation[i].maxLongTermFrameIdx = value - 1;
                num4++;
	              }
              if(operation == 5)
                  num5++;
              if(operation && operation <= 3)
                  num1to3++;
              if(operation == 6)
                  num6++;

              i++;
							} while(operation != 0);

            /* error checking */
            if(num4 > 1 || num5 > 1 || num6 > 1 || (num1to3 && num5))
                return(HANTRO_NOK);

        }
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function name: h264bsdCheckPpsId

        Functional description:
            Peek value of pic_parameter_set_id from the slice header. Function
            does not modify current stream positions but copies the stream
            data structure to tmp structure which is used while accessing
            stream data.

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            picParamSetId   value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckPpsId(strmData_t *pStrmData, uint32_t *picParamSetId) {
  uint32_t tmp, value;
  strmData_t tmpStrmData[1];


  ASSERT(pStrmData);

  /* don't touch original stream position params */
  *tmpStrmData = *pStrmData;

  /* first_mb_in_slice */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* slice_type */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;
  if(value >= MAX_NUM_PIC_PARAM_SETS)
      return(HANTRO_NOK);

  *picParamSetId = value;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckFrameNum

        Functional description:
            Peek value of frame_num from the slice header. Function does not
            modify current stream positions but copies the stream data
            structure to tmp structure which is used while accessing stream
            data.

        Inputs:
            pStrmData       pointer to stream data structure
            maxFrameNum

        Outputs:
            frameNum        value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckFrameNum(strmData_t *pStrmData, uint32_t maxFrameNum, uint32_t *frameNum) {
  uint32_t tmp, value, i;
  strmData_t tmpStrmData[1];


  ASSERT(pStrmData);
  ASSERT(maxFrameNum);
  ASSERT(frameNum);

  /* don't touch original stream position params */
  *tmpStrmData = *pStrmData;

  /* skip first_mb_in_slice */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* skip slice_type */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* skip pic_parameter_set_id */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* log2(maxFrameNum) -> num bits to represent frame_num */
  i=0;
  while(maxFrameNum >> i)
      i++;
  i--;

  /* frame_num */
  tmp = h264bsdGetBits(tmpStrmData, i);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  *frameNum = tmp;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckIdrPicId

        Functional description:
            Peek value of idr_pic_id from the slice header. Function does not
            modify current stream positions but copies the stream data
            structure to tmp structure which is used while accessing stream
            data.

        Inputs:
            pStrmData       pointer to stream data structure
            maxFrameNum     max frame number from active SPS
            nalUnitType     type of the current NAL unit

        Outputs:
            idrPicId        value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckIdrPicId(strmData_t *pStrmData,
  uint32_t maxFrameNum, nalUnitType_e nalUnitType,
  uint32_t *idrPicId) {
    uint32_t tmp, value, i;
    strmData_t tmpStrmData[1];


    ASSERT(pStrmData);
    ASSERT(maxFrameNum);
    ASSERT(idrPicId);

    /* nalUnitType must be equal to 5 because otherwise idrPicId is not
     * present */
    if(nalUnitType != NAL_CODED_SLICE_IDR)
        return(HANTRO_NOK);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i=0;
    while(maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);

    /* idr_pic_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, idrPicId);
    if(tmp != HANTRO_OK)
        return tmp;

    return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckPicOrderCntLsb

        Functional description:
            Peek value of pic_order_cnt_lsb from the slice header. Function
            does not modify current stream positions but copies the stream
            data structure to tmp structure which is used while accessing
            stream data.

        Inputs:
            pStrmData       pointer to stream data structure
            pSeqParamSet    pointer to active SPS
            nalUnitType     type of the current NAL unit

        Outputs:
            picOrderCntLsb  value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckPicOrderCntLsb(strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet, nalUnitType_e nalUnitType,
  uint32_t *picOrderCntLsb) {
  uint32_t tmp, value, i;
  strmData_t tmpStrmData[1];


  ASSERT(pStrmData);
  ASSERT(pSeqParamSet);
  ASSERT(picOrderCntLsb);

  /* picOrderCntType must be equal to 0 */
  ASSERT(pSeqParamSet->picOrderCntType == 0);
  ASSERT(pSeqParamSet->maxFrameNum);
  ASSERT(pSeqParamSet->maxPicOrderCntLsb);

  /* don't touch original stream position params */
  *tmpStrmData = *pStrmData;

  /* skip first_mb_in_slice */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* skip slice_type */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* skip pic_parameter_set_id */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* log2(maxFrameNum) -> num bits to represent frame_num */
  i=0;
  while(pSeqParamSet->maxFrameNum >> i)
      i++;
  i--;

  /* skip frame_num */
  tmp = h264bsdGetBits(tmpStrmData, i);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);

  /* skip idr_pic_id when necessary */
  if(nalUnitType == NAL_CODED_SLICE_IDR) {
      tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
      if(tmp != HANTRO_OK)
          return tmp;
		}

  /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
  i=0;
  while(pSeqParamSet->maxPicOrderCntLsb >> i)
      i++;
  i--;

  /* pic_order_cnt_lsb */
  tmp = h264bsdGetBits(tmpStrmData, i);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  *picOrderCntLsb = tmp;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckDeltaPicOrderCntBottom

        Functional description:
            Peek value of delta_pic_order_cnt_bottom from the slice header.
            Function does not modify current stream positions but copies the
            stream data structure to tmp structure which is used while
            accessing stream data.

        Inputs:
            pStrmData       pointer to stream data structure
            pSeqParamSet    pointer to active SPS
            nalUnitType     type of the current NAL unit

        Outputs:
            deltaPicOrderCntBottom  value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckDeltaPicOrderCntBottom(strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet, nalUnitType_e nalUnitType,
  int32_t *deltaPicOrderCntBottom) {
  uint32_t tmp, value, i;
  strmData_t tmpStrmData[1];


  ASSERT(pStrmData);
  ASSERT(pSeqParamSet);
  ASSERT(deltaPicOrderCntBottom);

  /* picOrderCntType must be equal to 0 and picOrderPresentFlag must be TRUE
   * */
  ASSERT(pSeqParamSet->picOrderCntType == 0);
  ASSERT(pSeqParamSet->maxFrameNum);
  ASSERT(pSeqParamSet->maxPicOrderCntLsb);

  /* don't touch original stream position params */
  *tmpStrmData = *pStrmData;

  /* skip first_mb_in_slice */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* skip slice_type */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* skip pic_parameter_set_id */
  tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
  if(tmp != HANTRO_OK)
      return tmp;

  /* log2(maxFrameNum) -> num bits to represent frame_num */
  i=0;
  while(pSeqParamSet->maxFrameNum >> i)
      i++;
  i--;

  /* skip frame_num */
  tmp = h264bsdGetBits(tmpStrmData, i);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);

  /* skip idr_pic_id when necessary */
  if(nalUnitType == NAL_CODED_SLICE_IDR) {
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;
		}

  /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
  i=0;
  while(pSeqParamSet->maxPicOrderCntLsb >> i)
      i++;
  i--;

  /* skip pic_order_cnt_lsb */
  tmp = h264bsdGetBits(tmpStrmData, i);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);

  /* delta_pic_order_cnt_bottom */
  tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, deltaPicOrderCntBottom);
  if(tmp != HANTRO_OK)
      return tmp;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckDeltaPicOrderCnt

        Functional description:
            Peek values delta_pic_order_cnt[0] and delta_pic_order_cnt[1]
            from the slice header. Function does not modify current stream
            positions but copies the stream data structure to tmp structure
            which is used while accessing stream data.

        Inputs:
            pStrmData               pointer to stream data structure
            pSeqParamSet            pointer to active SPS
            nalUnitType             type of the current NAL unit
            picOrderPresentFlag     flag indicating if delta_pic_order_cnt[1]
                                    is present in the stream

        Outputs:
            deltaPicOrderCnt        values are stored here

        Returns:
            HANTRO_OK               success
            HANTRO_NOK              invalid stream data

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckDeltaPicOrderCnt(strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet, nalUnitType_e nalUnitType,
  uint32_t picOrderPresentFlag, int32_t *deltaPicOrderCnt) {
    uint32_t tmp, value, i;
    strmData_t tmpStrmData[1];


    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);
    ASSERT(deltaPicOrderCnt);

    /* picOrderCntType must be equal to 1 and deltaPicOrderAlwaysZeroFlag must
     * be FALSE */
    ASSERT(pSeqParamSet->picOrderCntType == 1);
    ASSERT(!pSeqParamSet->deltaPicOrderAlwaysZeroFlag);
    ASSERT(pSeqParamSet->maxFrameNum);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i=0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);

    /* skip idr_pic_id when necessary */
    if(nalUnitType == NAL_CODED_SLICE_IDR) {
        tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
        if(tmp != HANTRO_OK)
            return tmp;
			}

    /* delta_pic_order_cnt[0] */
    tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &deltaPicOrderCnt[0]);
    if(tmp != HANTRO_OK)
        return tmp;

    /* delta_pic_order_cnt[1] if present */
    if(picOrderPresentFlag) {
        tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &deltaPicOrderCnt[1]);
        if(tmp != HANTRO_OK)
            return tmp;
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckRedundantPicCnt

        Functional description:
            Peek value of redundant_pic_cnt from the slice header. Function
            does not modify current stream positions but copies the stream
            data structure to tmp structure which is used while accessing
            stream data.

        Inputs:
            pStrmData       pointer to stream data structure
            pSeqParamSet    pointer to active SPS
            pPicParamSet    pointer to active PPS
            nalUnitType     type of the current NAL unit

        Outputs:
            redundantPicCnt value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckRedundantPicCnt(strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet, picParamSet_t *pPicParamSet,
  nalUnitType_e nalUnitType, uint32_t *redundantPicCnt) {
    uint32_t tmp, value, i;
    int32_t ivalue;
    strmData_t tmpStrmData[1];


    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);
    ASSERT(pPicParamSet);
    ASSERT(redundantPicCnt);

    /* redundant_pic_cnt_flag must be TRUE */
    ASSERT(pPicParamSet->redundantPicCntPresentFlag);
    ASSERT(pSeqParamSet->maxFrameNum);
    ASSERT(pSeqParamSet->picOrderCntType > 0 ||
           pSeqParamSet->maxPicOrderCntLsb);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i=0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);

    /* skip idr_pic_id when necessary */
    if(nalUnitType == NAL_CODED_SLICE_IDR) {
        tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
        if(tmp != HANTRO_OK)
            return tmp;
			}

    if(pSeqParamSet->picOrderCntType == 0) {
        /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
        i=0;
        while(pSeqParamSet->maxPicOrderCntLsb >> i)
            i++;
        i--;

        /* pic_order_cnt_lsb */
        tmp = h264bsdGetBits(tmpStrmData, i);
        if(tmp == END_OF_STREAM)
            return(HANTRO_NOK);

        if(pPicParamSet->picOrderPresentFlag) {
            /* skip delta_pic_order_cnt_bottom */
            tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
            if(tmp != HANTRO_OK)
                return tmp;
        }
			}

    if(pSeqParamSet->picOrderCntType == 1 &&
      !pSeqParamSet->deltaPicOrderAlwaysZeroFlag) {
        /* delta_pic_order_cnt[0] */
        tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
        if(tmp != HANTRO_OK)
            return tmp;

        /* delta_pic_order_cnt[1] if present */
        if(pPicParamSet->picOrderPresentFlag) {
            tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
            if(tmp != HANTRO_OK)
                return tmp;
        }
		  }

    /* redundant_pic_cnt */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, redundantPicCnt);
    if(tmp != HANTRO_OK)
        return tmp;

    return(HANTRO_OK);
	}


/*------------------------------------------------------------------------------

    Function: h264bsdCheckPriorPicsFlag

        Functional description:
            Peek value of no_output_of_prior_pics_flag from the slice header.
            Function does not modify current stream positions but copies
            the stream data structure to tmp structure which is used while
            accessing stream data.

        Inputs:
            pStrmData       pointer to stream data structure
            pSeqParamSet    pointer to active SPS
            pPicParamSet    pointer to active PPS
            nalUnitType     type of the current NAL unit

        Outputs:
            noOutputOfPriorPicsFlag value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckPriorPicsFlag(uint32_t *noOutputOfPriorPicsFlag,
                              const strmData_t *pStrmData,
                              const seqParamSet_t *pSeqParamSet, const picParamSet_t *pPicParamSet,
                              nalUnitType_e nalUnitType) {
    uint32_t tmp, value, i;
    int32_t ivalue;
    strmData_t tmpStrmData[1];


    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);
    ASSERT(pPicParamSet);
    ASSERT(noOutputOfPriorPicsFlag);

    /* must be IDR lsice */
    ASSERT(nalUnitType == NAL_CODED_SLICE_IDR);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i=0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    /* skip idr_pic_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return tmp;

    if(pSeqParamSet->picOrderCntType == 0) {
        /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
        i=0;
        while(pSeqParamSet->maxPicOrderCntLsb >> i)
            i++;
        i--;

        /* skip pic_order_cnt_lsb */
        tmp = h264bsdGetBits(tmpStrmData, i);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);

        if(pPicParamSet->picOrderPresentFlag) {
            /* skip delta_pic_order_cnt_bottom */
            tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
            if(tmp != HANTRO_OK)
                return tmp;
        }
			}

    if(pSeqParamSet->picOrderCntType == 1 &&
       !pSeqParamSet->deltaPicOrderAlwaysZeroFlag) {
        /* skip delta_pic_order_cnt[0] */
        tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
        if(tmp != HANTRO_OK)
            return tmp;

        /* skip delta_pic_order_cnt[1] if present */
        if(pPicParamSet->picOrderPresentFlag) {
            tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
            if(tmp != HANTRO_OK)
                return tmp;
        }
		  }

    /* skip redundant_pic_cnt */
    if(pPicParamSet->redundantPicCntPresentFlag) {
        tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
        if(tmp != HANTRO_OK)
            return tmp;
	    }

    *noOutputOfPriorPicsFlag = h264bsdGetBits(tmpStrmData, 1);
    if(*noOutputOfPriorPicsFlag == END_OF_STREAM)
        return (HANTRO_NOK);

    return (HANTRO_OK);

	}




static uint32_t CheckPps(picParamSet_t *pps, seqParamSet_t *sps);

/*------------------------------------------------------------------------------

    Function name: h264bsdInitStorage

        Functional description:
            Initialize storage structure. Sets contents of the storage to '0'
            except for the active parameter set ids, which are initialized
            to invalid values.

        Inputs:

        Outputs:
            pStorage    initialized data stored here

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdInitStorage(storage_t *pStorage) {

    ASSERT(pStorage);

    memset(pStorage, 0, sizeof(storage_t));

    pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS;
    pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS;

    pStorage->aub->firstCallFlag = HANTRO_TRUE;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdStoreSeqParamSet

        Functional description:
            Store sequence parameter set into the storage. If active SPS is
            overwritten -> check if contents changes and if it does, set
            parameters to force reactivation of parameter sets

        Inputs:
            pStorage        pointer to storage structure
            pSeqParamSet    pointer to param set to be stored

        Outputs:
            none

        Returns:
            HANTRO_OK                success
            MEMORY_ALLOCATION_ERROR  failure in memory allocation


------------------------------------------------------------------------------*/
uint32_t h264bsdStoreSeqParamSet(storage_t *pStorage, seqParamSet_t *pSeqParamSet) {
    uint32_t id;


    ASSERT(pStorage);
    ASSERT(pSeqParamSet);
    ASSERT(pSeqParamSet->seqParameterSetId < MAX_NUM_SEQ_PARAM_SETS);

    id = pSeqParamSet->seqParameterSetId;

    /* seq parameter set with id not used before -> allocate memory */
    if(!pStorage->sps[id]) {
        ALLOCATE(pStorage->sps[id], 1, seqParamSet_t);
        if(!pStorage->sps[id])
            return(MEMORY_ALLOCATION_ERROR);
			}
    /* sequence parameter set with id equal to id of active sps */
    else if(id == pStorage->activeSpsId) {
        /* if seq parameter set contents changes
         *    -> overwrite and re-activate when next IDR picture decoded
         *    ids of active param sets set to invalid values to force
         *    re-activation. Memories allocated for old sps freed
         * otherwise free memeries allocated for just decoded sps and
         * continue */
        if(h264bsdCompareSeqParamSets(pSeqParamSet, pStorage->activeSps) != 0) {
            FREE(pStorage->sps[id]->offsetForRefFrame);
            FREE(pStorage->sps[id]->vuiParameters);
            pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS + 1;
            pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS + 1;
            pStorage->activeSps = NULL;
            pStorage->activePps = NULL;
        }
        else {
            FREE(pSeqParamSet->offsetForRefFrame);
            FREE(pSeqParamSet->vuiParameters);
            return(HANTRO_OK);
        }
	    }
    /* overwrite seq param set other than active one -> free memories
     * allocated for old param set */
    else {
        FREE(pStorage->sps[id]->offsetForRefFrame);
        FREE(pStorage->sps[id]->vuiParameters);
    }

  *pStorage->sps[id] = *pSeqParamSet;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdStorePicParamSet

        Functional description:
            Store picture parameter set into the storage. If active PPS is
            overwritten -> check if active SPS changes and if it does -> set
            parameters to force reactivation of parameter sets

        Inputs:
            pStorage        pointer to storage structure
            pPicParamSet    pointer to param set to be stored

        Outputs:
            none

        Returns:
            HANTRO_OK                success
            MEMORY_ALLOCATION_ERROR  failure in memory allocation

------------------------------------------------------------------------------*/
uint32_t h264bsdStorePicParamSet(storage_t *pStorage, picParamSet_t *pPicParamSet) {
    uint32_t id;


    ASSERT(pStorage);
    ASSERT(pPicParamSet);
    ASSERT(pPicParamSet->picParameterSetId < MAX_NUM_PIC_PARAM_SETS);
    ASSERT(pPicParamSet->seqParameterSetId < MAX_NUM_SEQ_PARAM_SETS);

    id = pPicParamSet->picParameterSetId;

    /* pic parameter set with id not used before -> allocate memory */
    if(!pStorage->pps[id]) {
        ALLOCATE(pStorage->pps[id], 1, picParamSet_t);
        if(pStorage->pps[id] == NULL)
            return(MEMORY_ALLOCATION_ERROR);
    }
    /* picture parameter set with id equal to id of active pps */
    else if(id == pStorage->activePpsId) {
        /* check whether seq param set changes, force re-activation of
         * param set if it does. Set activeSpsId to invalid value to
         * accomplish this */
        if(pPicParamSet->seqParameterSetId != pStorage->activeSpsId) {
            pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS + 1;
        }
        /* free memories allocated for old param set */
        FREE(pStorage->pps[id]->runLength);
        FREE(pStorage->pps[id]->topLeft);
        FREE(pStorage->pps[id]->bottomRight);
        FREE(pStorage->pps[id]->sliceGroupId);
			}
    /* overwrite pic param set other than active one -> free memories
     * allocated for old param set */
    else {
        FREE(pStorage->pps[id]->runLength);
        FREE(pStorage->pps[id]->topLeft);
        FREE(pStorage->pps[id]->bottomRight);
        FREE(pStorage->pps[id]->sliceGroupId);
			}

    *pStorage->pps[id] = *pPicParamSet;

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdActivateParamSets

        Functional description:
            Activate certain SPS/PPS combination. This function shall be
            called in the beginning of each picture. Picture parameter set
            can be changed as wanted, but sequence parameter set may only be
            changed when the starting picture is an IDR picture.

            When new SPS is activated the function allocates memory for
            macroblock storages and slice group map and (re-)initializes the
            decoded picture buffer. If this is not the first activation the old
            allocations are freed and FreeDpb called before new allocations.

        Inputs:
            pStorage        pointer to storage data structure
            ppsId           identifies the PPS to be activated, SPS id obtained
                            from the PPS
            isIdr           flag to indicate if the picture is an IDR picture

        Outputs:
            none

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      non-existing or invalid param set combination,
                            trying to change SPS with non-IDR picture
            MEMORY_ALLOCATION_ERROR     failure in memory allocation

------------------------------------------------------------------------------*/
uint32_t h264bsdActivateParamSets(storage_t *pStorage, uint32_t ppsId, uint32_t isIdr) {
  uint32_t tmp;
  uint32_t flag;


  ASSERT(pStorage);
  ASSERT(ppsId < MAX_NUM_PIC_PARAM_SETS);

  /* check that pps and corresponding sps exist */
  if((!pStorage->pps[ppsId]) || (!pStorage->sps[pStorage->pps[ppsId]->seqParameterSetId]))
      return(HANTRO_NOK);

  /* check that pps parameters do not violate picture size constraints */
  tmp = CheckPps(pStorage->pps[ppsId], pStorage->sps[pStorage->pps[ppsId]->seqParameterSetId]);
  if(tmp != HANTRO_OK)
      return tmp;

  /* first activation part1 */
  if(pStorage->activePpsId == MAX_NUM_PIC_PARAM_SETS) {
    pStorage->activePpsId = ppsId;
    pStorage->activePps = pStorage->pps[ppsId];
    pStorage->activeSpsId = pStorage->activePps->seqParameterSetId;
    pStorage->activeSps = pStorage->sps[pStorage->activeSpsId];
    pStorage->picSizeInMbs =
        pStorage->activeSps->picWidthInMbs *
        pStorage->activeSps->picHeightInMbs;

    pStorage->currImage->width = pStorage->activeSps->picWidthInMbs;
    pStorage->currImage->height = pStorage->activeSps->picHeightInMbs;

    pStorage->pendingActivation = HANTRO_TRUE;
		}
  /* first activation part2 */
  else if(pStorage->pendingActivation) {
      pStorage->pendingActivation = HANTRO_FALSE;

      FREE(pStorage->mb);
      FREE(pStorage->sliceGroupMap);

      ALLOCATE(pStorage->mb, pStorage->picSizeInMbs, mbStorage_t);
      ALLOCATE(pStorage->sliceGroupMap, pStorage->picSizeInMbs, uint32_t);
      if(pStorage->mb == NULL || pStorage->sliceGroupMap == NULL)
          return(MEMORY_ALLOCATION_ERROR);

      memset(pStorage->mb, 0,
          pStorage->picSizeInMbs * sizeof(mbStorage_t));

      h264bsdInitMbNeighbours(pStorage->mb,
          pStorage->activeSps->picWidthInMbs,
          pStorage->picSizeInMbs);

      /* dpb output reordering disabled if
       * 1) application set noReordering flag
       * 2) POC type equal to 2
       * 3) num_reorder_frames in vui equal to 0 */
      if( pStorage->noReordering ||
           pStorage->activeSps->picOrderCntType == 2 ||
           (pStorage->activeSps->vuiParametersPresentFlag &&
            pStorage->activeSps->vuiParameters->bitstreamRestrictionFlag &&
            !pStorage->activeSps->vuiParameters->numReorderFrames))
          flag = HANTRO_TRUE;
      else
          flag = HANTRO_FALSE;

      tmp = h264bsdResetDpb(pStorage->dpb,
          pStorage->activeSps->picWidthInMbs * pStorage->activeSps->picHeightInMbs,
          pStorage->activeSps->maxDpbSize,
          pStorage->activeSps->numRefFrames,pStorage->activeSps->maxFrameNum,
          flag);
      if(tmp != HANTRO_OK)
          return tmp;
		}
  else if(ppsId != pStorage->activePpsId) {
      /* sequence parameter set shall not change but before an IDR picture */
      if(pStorage->pps[ppsId]->seqParameterSetId != pStorage->activeSpsId) {
          DEBUGP("SEQ PARAM SET CHANGING...");
          if(isIdr) {
            pStorage->activePpsId = ppsId;
            pStorage->activePps = pStorage->pps[ppsId];
            pStorage->activeSpsId = pStorage->activePps->seqParameterSetId;
            pStorage->activeSps = pStorage->sps[pStorage->activeSpsId];
            pStorage->picSizeInMbs =
                pStorage->activeSps->picWidthInMbs *
                pStorage->activeSps->picHeightInMbs;

            pStorage->currImage->width = pStorage->activeSps->picWidthInMbs;
            pStorage->currImage->height =
                pStorage->activeSps->picHeightInMbs;

            pStorage->pendingActivation = HANTRO_TRUE;
						}
          else {
              DEBUGP("TRYING TO CHANGE SPS IN NON-IDR SLICE");
              return(HANTRO_NOK);
						}
				}
      else {
        pStorage->activePpsId = ppsId;
        pStorage->activePps = pStorage->pps[ppsId];
				}
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdResetStorage

        Functional description:
            Reset contents of the storage. This should be called before
            processing of new image is started.

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            none

        Returns:
            none


------------------------------------------------------------------------------*/
void h264bsdResetStorage(storage_t *pStorage) {
    uint32_t i;


    ASSERT(pStorage);

    pStorage->slice->numDecodedMbs=0;
    pStorage->slice->sliceId=0;

    for(i=0; i < pStorage->picSizeInMbs; i++) {
        pStorage->mb[i].sliceId=0;
        pStorage->mb[i].decoded=0;
    }
	}

/*------------------------------------------------------------------------------

    Function: h264bsdIsStartOfPicture

        Functional description:
            Determine if the decoder is in the start of a picture. This
            information is needed to decide if h264bsdActivateParamSets and
            h264bsdCheckGapsInFrameNum functions should be called. Function
            considers that new picture is starting if no slice headers
            have been successfully decoded for the current access unit.

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            none

        Returns:
            HANTRO_TRUE        new picture is starting
            HANTRO_FALSE       not starting

------------------------------------------------------------------------------*/
uint32_t h264bsdIsStartOfPicture(storage_t *pStorage) {

  if(pStorage->validSliceInAccessUnit == HANTRO_FALSE)
      return(HANTRO_TRUE);
  else
      return(HANTRO_FALSE);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdIsEndOfPicture

        Functional description:
            Determine if the decoder is in the end of a picture. This
            information is needed to determine when deblocking filtering
            and reference picture marking processes should be performed.

            If the decoder is processing primary slices the return value
            is determined by checking the value of numDecodedMbs in the
            storage. On the other hand, if the decoder is processing
            redundant slices the numDecodedMbs may not contain valid
            informationa and each macroblock has to be checked separately.

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            none

        Returns:
            HANTRO_TRUE        end of picture
            HANTRO_FALSE       noup

------------------------------------------------------------------------------*/
uint32_t h264bsdIsEndOfPicture(storage_t *pStorage) {
  uint32_t i, tmp;


  /* primary picture */
  if(!pStorage->sliceHeader[0].redundantPicCnt) {
      if(pStorage->slice->numDecodedMbs == pStorage->picSizeInMbs)
          return(HANTRO_TRUE);
		}
  else {
      for(i=0, tmp=0; i < pStorage->picSizeInMbs; i++)
          tmp += pStorage->mb[i].decoded ? 1 : 0;

      if(tmp == pStorage->picSizeInMbs)
          return(HANTRO_TRUE);
    }

  return(HANTRO_FALSE);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdComputeSliceGroupMap

        Functional description:
            Compute slice group map. Just call h264bsdDecodeSliceGroupMap with
            appropriate parameters.

        Inputs:
            pStorage                pointer to storage structure
            sliceGroupChangeCycle

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdComputeSliceGroupMap(storage_t *pStorage, uint32_t sliceGroupChangeCycle) {

  h264bsdDecodeSliceGroupMap(pStorage->sliceGroupMap,
                      pStorage->activePps, sliceGroupChangeCycle,
                      pStorage->activeSps->picWidthInMbs,pStorage->activeSps->picHeightInMbs);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckAccessUnitBoundary

        Functional description:
            Check if next NAL unit starts a new access unit. Following
            conditions specify start of a new access unit:

                -NAL unit types 6-11, 13-18 (e.g. SPS, PPS)

           following conditions checked only for slice NAL units, values
           compared to ones obtained from previous slice:

                -NAL unit type differs (slice / IDR slice)
                -frame_num differs
                -nal_ref_idc differs and one of the values is 0
                -POC information differs
                -both are IDR slices and idr_pic_id differs

        Inputs:
            strm        pointer to stream data structure
            nuNext      pointer to NAL unit structure
            storage     pointer to storage structure

        Outputs:
            accessUnitBoundaryFlag  the result is stored here, TRUE for
                                    access unit boundary, FALSE otherwise

        Returns:
            HANTRO_OK           success
            HANTRO_NOK          failure, invalid stream data
            PARAM_SET_ERROR     invalid param set usage

------------------------------------------------------------------------------*/
uint32_t h264bsdCheckAccessUnitBoundary(strmData_t *strm,
  nalUnit_t *nuNext, storage_t *storage, uint32_t *accessUnitBoundaryFlag) {
    uint32_t tmp, ppsId, frameNum, idrPicId, picOrderCntLsb;
    int32_t deltaPicOrderCntBottom, deltaPicOrderCnt[2];
    seqParamSet_t *sps;
    picParamSet_t *pps;


    ASSERT(strm);
    ASSERT(nuNext);
    ASSERT(storage);
    ASSERT(storage->sps);
    ASSERT(storage->pps);

    /* initialize default output to FALSE */
    *accessUnitBoundaryFlag = HANTRO_FALSE;

    if(( (nuNext->nalUnitType > 5) && (nuNext->nalUnitType < 12)) ||
         ((nuNext->nalUnitType > 12) && (nuNext->nalUnitType <= 18))) {
        *accessUnitBoundaryFlag = HANTRO_TRUE;
        return(HANTRO_OK);
	    }
    else if( nuNext->nalUnitType != NAL_CODED_SLICE &&
              nuNext->nalUnitType != NAL_CODED_SLICE_IDR ) {
        return(HANTRO_OK);
		  }

    /* check if this is the very first call to this function */
    if(storage->aub->firstCallFlag) {
        *accessUnitBoundaryFlag = HANTRO_TRUE;
        storage->aub->firstCallFlag = HANTRO_FALSE;
			}

    /* get picture parameter set id */
    tmp = h264bsdCheckPpsId(strm, &ppsId);
    if(tmp != HANTRO_OK)
        return tmp;

    /* store sps and pps in separate pointers just to make names shorter */
    pps = storage->pps[ppsId];
    if( pps == NULL || storage->sps[pps->seqParameterSetId] == NULL  ||
         (storage->activeSpsId != MAX_NUM_SEQ_PARAM_SETS &&
          pps->seqParameterSetId != storage->activeSpsId &&
          nuNext->nalUnitType != NAL_CODED_SLICE_IDR))
        return(PARAM_SET_ERROR);
    sps = storage->sps[pps->seqParameterSetId];

    if(storage->aub->nuPrev->nalRefIdc != nuNext->nalRefIdc &&
      (storage->aub->nuPrev->nalRefIdc == 0 || nuNext->nalRefIdc == 0))
        *accessUnitBoundaryFlag = HANTRO_TRUE;

    if((storage->aub->nuPrev->nalUnitType == NAL_CODED_SLICE_IDR &&
          nuNext->nalUnitType != NAL_CODED_SLICE_IDR) ||
      (storage->aub->nuPrev->nalUnitType != NAL_CODED_SLICE_IDR &&
       nuNext->nalUnitType == NAL_CODED_SLICE_IDR))
        *accessUnitBoundaryFlag = HANTRO_TRUE;

    tmp = h264bsdCheckFrameNum(strm, sps->maxFrameNum, &frameNum);
    if(tmp != HANTRO_OK)
        return(HANTRO_NOK);

    if(storage->aub->prevFrameNum != frameNum) {
        storage->aub->prevFrameNum = frameNum;
        *accessUnitBoundaryFlag = HANTRO_TRUE;
			}

    if(nuNext->nalUnitType == NAL_CODED_SLICE_IDR) {
        tmp = h264bsdCheckIdrPicId(strm, sps->maxFrameNum, nuNext->nalUnitType,
          &idrPicId);
        if(tmp != HANTRO_OK)
            return(HANTRO_NOK);

        if(storage->aub->nuPrev->nalUnitType == NAL_CODED_SLICE_IDR &&
          storage->aub->prevIdrPicId != idrPicId)
            *accessUnitBoundaryFlag = HANTRO_TRUE;

        storage->aub->prevIdrPicId = idrPicId;
    }

    if(sps->picOrderCntType == 0) {
        tmp = h264bsdCheckPicOrderCntLsb(strm, sps, nuNext->nalUnitType,
          &picOrderCntLsb);
        if(tmp != HANTRO_OK)
            return(HANTRO_NOK);

        if(storage->aub->prevPicOrderCntLsb != picOrderCntLsb) {
            storage->aub->prevPicOrderCntLsb = picOrderCntLsb;
            *accessUnitBoundaryFlag = HANTRO_TRUE;
        }

        if(pps->picOrderPresentFlag) {
            tmp = h264bsdCheckDeltaPicOrderCntBottom(strm, sps,
                nuNext->nalUnitType, &deltaPicOrderCntBottom);
            if(tmp != HANTRO_OK)
                return tmp;

            if(storage->aub->prevDeltaPicOrderCntBottom != deltaPicOrderCntBottom) {
                storage->aub->prevDeltaPicOrderCntBottom =
                    deltaPicOrderCntBottom;
                *accessUnitBoundaryFlag = HANTRO_TRUE;
            }
        }
		  }
    else if(sps->picOrderCntType == 1 && !sps->deltaPicOrderAlwaysZeroFlag) {
        tmp = h264bsdCheckDeltaPicOrderCnt(strm, sps, nuNext->nalUnitType,
          pps->picOrderPresentFlag, deltaPicOrderCnt);
        if(tmp != HANTRO_OK)
            return tmp;

        if(storage->aub->prevDeltaPicOrderCnt[0] != deltaPicOrderCnt[0]) {
            storage->aub->prevDeltaPicOrderCnt[0] = deltaPicOrderCnt[0];
            *accessUnitBoundaryFlag = HANTRO_TRUE;
			    }

        if(pps->picOrderPresentFlag)
            if(storage->aub->prevDeltaPicOrderCnt[1] != deltaPicOrderCnt[1]) {
                storage->aub->prevDeltaPicOrderCnt[1] = deltaPicOrderCnt[1];
                *accessUnitBoundaryFlag = HANTRO_TRUE;
            }
	    }

    *storage->aub->nuPrev = *nuNext;

    return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: CheckPps

        Functional description:
            Check picture parameter set. Contents of the picture parameter
            set information that depends on the image dimensions is checked
            against the dimensions in the sps.

        Inputs:
            pps     pointer to picture paramter set
            sps     pointer to sequence parameter set

        Outputs:
            none

        Returns:
            HANTRO_OK      everything ok
            HANTRO_NOK     invalid data in picture parameter set

------------------------------------------------------------------------------*/
uint32_t CheckPps(picParamSet_t *pps, seqParamSet_t *sps) {
  uint32_t i;
  uint32_t picSize;

  picSize = sps->picWidthInMbs * sps->picHeightInMbs;

  /* check slice group params */
  if(pps->numSliceGroups > 1) {
      if(pps->sliceGroupMapType == 0) {
          ASSERT(pps->runLength);
          for(i=0; i < pps->numSliceGroups; i++) {
              if(pps->runLength[i] > picSize)
                  return(HANTRO_NOK);
          }
				}
      else if(pps->sliceGroupMapType == 2) {
          ASSERT(pps->topLeft);
          ASSERT(pps->bottomRight);
          for(i=0; i < pps->numSliceGroups-1; i++) {
              if(pps->topLeft[i] > pps->bottomRight[i] || pps->bottomRight[i] >= picSize)
                  return(HANTRO_NOK);

              if((pps->topLeft[i] % sps->picWidthInMbs) > (pps->bottomRight[i] % sps->picWidthInMbs))
                  return(HANTRO_NOK);
          }
				}
      else if(pps->sliceGroupMapType > 2 && pps->sliceGroupMapType < 6) {
          if(pps->sliceGroupChangeRate > picSize)
              return(HANTRO_NOK);
				}
      else if(pps->sliceGroupMapType == 6 && pps->picSizeInMapUnits < picSize)
          return(HANTRO_NOK);
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdValidParamSets

        Functional description:
            Check if any valid SPS/PPS combination exists in the storage.
            Function tries each PPS in the buffer and checks if corresponding
            SPS exists and calls CheckPps to determine if the PPS conforms
            to image dimensions of the SPS.

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            HANTRO_OK   there is at least one valid combination
            HANTRO_NOK  no valid combinations found


------------------------------------------------------------------------------*/
uint32_t h264bsdValidParamSets(storage_t *pStorage) {
  uint32_t i;


  ASSERT(pStorage);

  for(i=0; i < MAX_NUM_PIC_PARAM_SETS; i++) {
    if(pStorage->pps[i] &&
       pStorage->sps[pStorage->pps[i]->seqParameterSetId] &&
       CheckPps(pStorage->pps[i],pStorage->sps[pStorage->pps[i]->seqParameterSetId]) ==
           HANTRO_OK)
      return(HANTRO_OK);
	  }

  return(HANTRO_NOK);
	}


/*------------------------------------------------------------------------------

    Function: h264bsdGetBits

        Functional description:
            Read and remove bits from the stream buffer.

        Input:
            pStrmData   pointer to stream data structure
            numBits     number of bits to read

        Output:
            none

        Returns:
            bits read from stream
            END_OF_STREAM if not enough bits left

------------------------------------------------------------------------------*/
uint32_t h264bsdGetBits(strmData_t *pStrmData, uint32_t numBits) {
  uint32_t out;

  ASSERT(pStrmData);
  ASSERT(numBits < 32);

  out = h264bsdShowBits32(pStrmData) >> (32 - numBits);

  if(h264bsdFlushBits(pStrmData, numBits) == HANTRO_OK)
    return out;
  else
    return(END_OF_STREAM);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdShowBits32

        Functional description:
            Read 32 bits from the stream buffer. Buffer is left as it is, i.e.
            no bits are removed. First bit read from the stream is the MSB of
            the return value. If there is not enough bits in the buffer ->
            bits beyong the end of the stream are set to '0' in the return
            value.

        Input:
            pStrmData   pointer to stream data structure

        Output:
            none

        Returns:
            bits read from stream

------------------------------------------------------------------------------*/
uint32_t h264bsdShowBits32(strmData_t *pStrmData) {
  int32_t bits, shift;
  uint32_t out;
  uint8_t *pStrm;

  ASSERT(pStrmData);
  ASSERT(pStrmData->pStrmCurrPos);
  ASSERT(pStrmData->bitPosInWord < 8);
  ASSERT(pStrmData->bitPosInWord == (pStrmData->strmBuffReadBits & 0x7));

  pStrm = pStrmData->pStrmCurrPos;

  /* number of bits left in the buffer */
  bits = (int32_t)pStrmData->strmBuffSize*8 - (int32_t)pStrmData->strmBuffReadBits;

  /* at least 32-bits in the buffer */
  if(bits >= 32) {
    uint32_t bitPosInWord = pStrmData->bitPosInWord;
    out = ((uint32_t)pStrm[0] << 24) | ((uint32_t)pStrm[1] << 16) |
          ((uint32_t)pStrm[2] <<  8) | ((uint32_t)pStrm[3]);

    if(bitPosInWord) {
      uint32_t byte = (uint32_t)pStrm[4];
      uint32_t tmp = (8-bitPosInWord);
      out <<= bitPosInWord;
      out |= byte>>tmp;
      }
    return out;
    }
  /* at least one bit in the buffer */
  else if(bits > 0) {
    shift = (int32_t)(24 + pStrmData->bitPosInWord);
    out = (uint32_t)(*pStrm++) << shift;
    bits -= (int32_t)(8 - pStrmData->bitPosInWord);
    while(bits > 0) {
      shift -= 8;
      out |= (uint32_t)(*pStrm++) << shift;
      bits -= 8;
      }
    return out;
		}
  else
    return 0;
	}

/*------------------------------------------------------------------------------

    Function: h264bsdFlushBits

        Functional description:
            Remove bits from the stream buffer

        Input:
            pStrmData       pointer to stream data structure
            numBits         number of bits to remove

        Output:
            none

        Returns:
            HANTRO_OK       success
            END_OF_STREAM   not enough bits left

------------------------------------------------------------------------------*/
#ifndef H264DEC_NEON
uint32_t h264bsdFlushBits(strmData_t *pStrmData, uint32_t numBits) {

  ASSERT(pStrmData);
  ASSERT(pStrmData->pStrmBuffStart);
  ASSERT(pStrmData->pStrmCurrPos);
  ASSERT(pStrmData->bitPosInWord < 8);
  ASSERT(pStrmData->bitPosInWord == (pStrmData->strmBuffReadBits & 0x7));

  pStrmData->strmBuffReadBits += numBits;
  pStrmData->bitPosInWord = pStrmData->strmBuffReadBits & 0x7;
  if((pStrmData->strmBuffReadBits ) <= (8*pStrmData->strmBuffSize)) {
    pStrmData->pStrmCurrPos = pStrmData->pStrmBuffStart +
        (pStrmData->strmBuffReadBits >> 3);
    return(HANTRO_OK);
	  }
  else
    return(END_OF_STREAM);
	}
#endif

/*------------------------------------------------------------------------------

    Function: h264bsdIsByteAligned

        Functional description:
            Check if current stream position is byte aligned.

        Inputs:
            pStrmData   pointer to stream data structure

        Outputs:
            none

        Returns:
            TRUE        stream is byte aligned
            FALSE       stream is not byte aligned

------------------------------------------------------------------------------*/
uint32_t h264bsdIsByteAligned(strmData_t *pStrmData) {

    if(!pStrmData->bitPosInWord)
        return(HANTRO_TRUE);
    else
        return(HANTRO_FALSE);

	}



/* Switch off the following Lint messages for this file:
 * Info 701: Shift left of signed quantity (int)
 * Info 702: Shift right of signed quantity (int)
 */

/* LevelScale function */
static const int32_t levelScale[6][3] = {
{10,13,16}, {11,14,18}, {13,16,20}, {14,18,23}, {16,20,25}, {18,23,29}};

/* qp % 6 as a function of qp */
static const uint8_t qpMod6[52] = {0,1,2,3,4,5,0,1,2,3,4,5,0,1,2,3,4,5,0,1,2,3,4,5,
    0,1,2,3,4,5,0,1,2,3,4,5,0,1,2,3,4,5,0,1,2,3,4,5,0,1,2,3};

/* qp / 6 as a function of qp */
static const uint8_t qpDiv6[52] = {0,0,0,0,0,0,1,1,1,1,1,1,2,2,2,2,2,2,3,3,3,3,3,3,
    4,4,4,4,4,4,5,5,5,5,5,5,6,6,6,6,6,6,7,7,7,7,7,7,8,8,8,8};


/*------------------------------------------------------------------------------

    Function: h264bsdProcessBlock

        Functional description:
            Function performs inverse zig-zag scan, inverse scaling and
            inverse transform for a luma or a chroma residual block

        Inputs:
            data            pointer to data to be processed
            qp              quantization parameter
            skip            skip processing of data[0], set to non-zero value
                            if dc coeff hanled separately
            coeffMap        16 lsb's indicate which coeffs are non-zero,
                            bit 0 (lsb) for coeff 0, bit 1 for coeff 1 etc.

        Outputs:
            data            processed data

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      processed data not in valid range [-512, 511]

------------------------------------------------------------------------------*/
uint32_t h264bsdProcessBlock(int32_t *data, uint32_t qp, uint32_t skip, uint32_t coeffMap) {
    int32_t tmp0, tmp1, tmp2, tmp3;
    int32_t d1, d2, d3;
    uint32_t row,col;
    uint32_t qpDiv;
    int32_t *ptr;


    qpDiv = qpDiv6[qp];
    tmp1 = levelScale[qpMod6[qp]][0] << qpDiv;
    tmp2 = levelScale[qpMod6[qp]][1] << qpDiv;
    tmp3 = levelScale[qpMod6[qp]][2] << qpDiv;

    if(!skip)
        data[0] = (data[0] * tmp1);

    /* at least one of the rows 1, 2 or 3 contain non-zero coeffs, mask takes
     * the scanning order into account */
    if(coeffMap & 0xFF9C) {
        /* do the zig-zag scan and inverse quantization */
        d1 = data[1];
        d2 = data[14];
        d3 = data[15];
        data[1] = (d1 * tmp2);
        data[14] = (d2 * tmp2);
        data[15] = (d3 * tmp3);

        d1 = data[2];
        d2 = data[5];
        d3 = data[4];
        data[4] = (d1 * tmp2);
        data[2]  = (d2 * tmp1);
        data[5] = (d3 * tmp3);

        d1 = data[8];
        d2 = data[3];
        d3 = data[6];
        tmp0 = (d1 * tmp2);
        data[8] = (d2 * tmp1);
        data[3]  = (d3 * tmp2);
        d1 = data[7];
        d2 = data[12];
        d3 = data[9];
        data[6]  = (d1 * tmp2);
        data[7]  = (d2 * tmp3);
        data[12] = (d3 * tmp2);
        data[9]  = tmp0;

        d1 = data[10];
        d2 = data[11];
        d3 = data[13];
        data[13] = (d1 * tmp3);
        data[10] = (d2 * tmp1);
        data[11] = (d3 * tmp2);

        /* horizontal transform */
        for(row = 4, ptr = data; row--; ptr += 4) {
          tmp0 = ptr[0] + ptr[2];
          tmp1 = ptr[0] - ptr[2];
          tmp2 = (ptr[1] >> 1) - ptr[3];
          tmp3 = ptr[1] + (ptr[3] >> 1);
          ptr[0] = tmp0 + tmp3;
          ptr[1] = tmp1 + tmp2;
          ptr[2] = tmp1 - tmp2;
		      ptr[3] = tmp0 - tmp3;
			    }

        /* then vertical transform */
        for(col = 4; col--; data++) {
          tmp0 = data[0] + data[8];
          tmp1 = data[0] - data[8];
          tmp2 = (data[4] >> 1) - data[12];
          tmp3 = data[4] + (data[12] >> 1);
          data[0 ] = (tmp0 + tmp3 + 32)>>6;
          data[4 ] = (tmp1 + tmp2 + 32)>>6;
          data[8 ] = (tmp1 - tmp2 + 32)>>6;
          data[12] = (tmp0 - tmp3 + 32)>>6;
          /* check that each value is in the range [-512,511] */
          if(((uint32_t)(data[0] + 512) > 1023) ||
            ((uint32_t)(data[4] + 512) > 1023) ||
            ((uint32_t)(data[8] + 512) > 1023) ||
            ((uint32_t)(data[12] + 512) > 1023))
            return(HANTRO_NOK);
			      }
	    }
    else /* rows 1, 2 and 3 are zero */
{
      /* only dc-coeff is non-zero, i.e. coeffs at original positions
       * 1, 5 and 6 are zero */
      if((coeffMap & 0x62) == 0) {
          tmp0 = (data[0] + 32) >> 6;
          /* check that value is in the range [-512,511] */
          if((uint32_t)(tmp0 + 512) > 1023)
              return(HANTRO_NOK);
          data[0] = data[1]  = data[2]  = data[3]  = data[4]  = data[5]  =
                    data[6]  = data[7]  = data[8]  = data[9]  = data[10] =
                    data[11] = data[12] = data[13] = data[14] = data[15] =
                    tmp0;
				}
      else /* at least one of the coeffs 1, 5 or 6 is non-zero */
				{
          data[1] = (data[1] * tmp2);
          data[2] = (data[5] * tmp1);
          data[3] = (data[6] * tmp2);
          tmp0 = data[0] + data[2];
          tmp1 = data[0] - data[2];
          tmp2 = (data[1] >> 1) - data[3];
          tmp3 = data[1] + (data[3] >> 1);
          data[0] = (tmp0 + tmp3 + 32)>>6;
          data[1] = (tmp1 + tmp2 + 32)>>6;
          data[2] = (tmp1 - tmp2 + 32)>>6;
          data[3] = (tmp0 - tmp3 + 32)>>6;
          data[4] = data[8] = data[12] = data[0];
          data[5] = data[9] = data[13] = data[1];
          data[6] = data[10] = data[14] = data[2];
          data[7] = data[11] = data[15] = data[3];
          /* check that each value is in the range [-512,511] */
          if(((uint32_t)(data[0] + 512) > 1023) ||
              ((uint32_t)(data[1] + 512) > 1023) ||
              ((uint32_t)(data[2] + 512) > 1023) ||
              ((uint32_t)(data[3] + 512) > 1023))
              return(HANTRO_NOK);
				}
		}

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: h264bsdProcessLumaDc

        Functional description:
            Function performs inverse zig-zag scan, inverse transform and
            inverse scaling for a luma DC coefficients block

        Inputs:
            data            pointer to data to be processed
            qp              quantization parameter

        Outputs:
            data            processed data

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdProcessLumaDc(int32_t *data, uint32_t qp) {
  int32_t tmp0, tmp1, tmp2, tmp3;
  uint32_t row,col;
  uint32_t qpMod, qpDiv;
  int32_t levScale;
  int32_t *ptr;


  qpMod = qpMod6[qp];
  qpDiv = qpDiv6[qp];

  /* zig-zag scan */
  tmp0 = data[2];
  data[2]  = data[5];
  data[5] = data[4];
  data[4] = tmp0;

  tmp0 = data[8];
  data[8] = data[3];
  data[3]  = data[6];
  data[6]  = data[7];
  data[7]  = data[12];
  data[12] = data[9];
  data[9]  = tmp0;

  tmp0 = data[10];
  data[10] = data[11];
  data[11] = data[13];
  data[13] = tmp0;

  /* horizontal transform */
  for(row = 4, ptr = data; row--; ptr += 4) {
    tmp0 = ptr[0] + ptr[2];
    tmp1 = ptr[0] - ptr[2];
    tmp2 = ptr[1] - ptr[3];
    tmp3 = ptr[1] + ptr[3];
    ptr[0] = tmp0 + tmp3;
    ptr[1] = tmp1 + tmp2;
    ptr[2] = tmp1 - tmp2;
    ptr[3] = tmp0 - tmp3;
    }

  /* then vertical transform and inverse scaling */
  levScale = levelScale[ qpMod ][0];
  if(qp >= 12) {
      levScale <<= (qpDiv-2);
      for(col = 4; col--; data++) {
          tmp0 = data[0] + data[8 ];
          tmp1 = data[0] - data[8 ];
          tmp2 = data[4] - data[12];
          tmp3 = data[4] + data[12];
          data[0 ] = ((tmp0 + tmp3)*levScale);
          data[4 ] = ((tmp1 + tmp2)*levScale);
          data[8 ] = ((tmp1 - tmp2)*levScale);
          data[12] = ((tmp0 - tmp3)*levScale);
				}
			}
    else {
        int32_t tmp;
        tmp = ((1 - qpDiv) == 0) ? 1 : 2;
        for(col = 4; col--; data++) {
            tmp0 = data[0] + data[8 ];
            tmp1 = data[0] - data[8 ];
            tmp2 = data[4] - data[12];
            tmp3 = data[4] + data[12];
            data[0 ] = ((tmp0 + tmp3)*levScale+tmp) >> (2-qpDiv);
            data[4 ] = ((tmp1 + tmp2)*levScale+tmp) >> (2-qpDiv);
            data[8 ] = ((tmp1 - tmp2)*levScale+tmp) >> (2-qpDiv);
            data[12] = ((tmp0 - tmp3)*levScale+tmp) >> (2-qpDiv);
				  }
			}

	}

/*------------------------------------------------------------------------------

    Function: h264bsdProcessChromaDc

        Functional description:
            Function performs inverse transform and inverse scaling for a
            chroma DC coefficients block

        Inputs:
            data            pointer to data to be processed
            qp              quantization parameter

        Outputs:
            data            processed data

        Returns:
            none

------------------------------------------------------------------------------*/
void h264bsdProcessChromaDc(int32_t *data, uint32_t qp) {
  int32_t tmp0, tmp1, tmp2, tmp3;
  uint32_t qpDiv;
  int32_t levScale;
  uint32_t levShift;


  qpDiv = qpDiv6[qp];
  levScale = levelScale[ qpMod6[qp] ][0];

  if(qp >= 6) {
      levScale <<= (qpDiv-1);
      levShift=0;
	  }
  else
    levShift = 1;

  tmp0 = data[0] + data[2];
  tmp1 = data[0] - data[2];
  tmp2 = data[1] - data[3];
  tmp3 = data[1] + data[3];
  data[0] = ((tmp0 + tmp3) * levScale) >> levShift;
  data[1] = ((tmp0 - tmp3) * levScale) >> levShift;
  data[2] = ((tmp1 + tmp2) * levScale) >> levShift;
  data[3] = ((tmp1 - tmp2) * levScale) >> levShift;
  tmp0 = data[4] + data[6];
  tmp1 = data[4] - data[6];
  tmp2 = data[5] - data[7];
  tmp3 = data[5] + data[7];
  data[4] = ((tmp0 + tmp3) * levScale) >> levShift;
  data[5] = ((tmp0 - tmp3) * levScale) >> levShift;
  data[6] = ((tmp1 + tmp2) * levScale) >> levShift;
  data[7] = ((tmp1 - tmp2) * levScale) >> levShift;
	}


/* look-up table for expected values of stuffing bits */
static const uint32_t stuffingTable[8] = {0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80};

/* look-up table for chroma quantization parameter as a function of luma QP */
const uint32_t h264bsdQpC[52] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
    20,21,22,23,24,25,26,27,28,29,29,30,31,32,32,33,34,34,35,35,36,36,37,37,37,
    38,38,38,39,39,39,39};


/*------------------------------------------------------------------------------

   5.1  Function: h264bsdCountLeadingZeros

        Functional description:
            Count leading zeros in a code word. Code word is assumed to be
            right-aligned, last bit of the code word in the lsb of the value.

        Inputs:
            value   code word
            length  number of bits in the code word

        Outputs:
            none

        Returns:
            number of leading zeros in the code word

------------------------------------------------------------------------------*/
#ifndef H264DEC_NEON
uint32_t h264bsdCountLeadingZeros(uint32_t value, uint32_t length) {
  uint32_t zeros=0;
  uint32_t mask = 1 << (length - 1);


  ASSERT(length <= 32);

  while(mask && !(value & mask)) {
      zeros++;
      mask >>= 1;
		}

  return(zeros);
	}
#endif

/*------------------------------------------------------------------------------

   5.2  Function: h264bsdRbspTrailingBits

        Functional description:
            Check Raw Byte Stream Payload (RBSP) trailing bits, i.e. stuffing.
            Rest of the current byte (whole byte if allready byte aligned)
            in the stream buffer shall contain a '1' bit followed by zero or
            more '0' bits.

        Inputs:
            pStrmData   pointer to stream data structure

        Outputs:
            none

        Returns:
            HANTRO_OK      RBSP trailing bits found
            HANTRO_NOK     otherwise

------------------------------------------------------------------------------*/
uint32_t h264bsdRbspTrailingBits(strmData_t *pStrmData) {
  uint32_t stuffing;
  uint32_t stuffingLength;


  ASSERT(pStrmData);
  ASSERT(pStrmData->bitPosInWord < 8);

  stuffingLength = 8 - pStrmData->bitPosInWord;

  stuffing = h264bsdGetBits(pStrmData, stuffingLength);
  if(stuffing == END_OF_STREAM)
      return(HANTRO_NOK);

  if(stuffing != stuffingTable[stuffingLength - 1])
      return(HANTRO_NOK);
  else
      return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

   5.3  Function: h264bsdMoreRbspData

        Functional description:
            Check if there is more data in the current RBSP. The standard
            defines this function so that there is more data if
                -more than 8 bits left or
                -last bits are not RBSP trailing bits

        Inputs:
            pStrmData   pointer to stream data structure

        Outputs:
            none

        Returns:
            HANTRO_TRUE    there is more data
            HANTRO_FALSE   no more data

------------------------------------------------------------------------------*/
uint32_t h264bsdMoreRbspData(strmData_t *pStrmData) {
  uint32_t bits;


  ASSERT(pStrmData);
  ASSERT(pStrmData->strmBuffReadBits <= 8 * pStrmData->strmBuffSize);

  bits = pStrmData->strmBuffSize * 8 - pStrmData->strmBuffReadBits;

  if(bits == 0)
      return(HANTRO_FALSE);

  if((bits > 8) ||
       ((h264bsdShowBits32(pStrmData)>>(32-bits)) != (1ul << (bits-1))))
      return(HANTRO_TRUE);
  else
      return(HANTRO_FALSE);
	}

/*------------------------------------------------------------------------------

   5.4  Function: h264bsdNextMbAddress

        Functional description:
            Get address of the next macroblock in the current slice group.

        Inputs:
            pSliceGroupMap      slice group for each macroblock
            picSizeInMbs        size of the picture
            currMbAddr          where to start

        Outputs:
            none

        Returns:
            address of the next macroblock
            0   if none of the following macroblocks belong to same slice
                group as currMbAddr

------------------------------------------------------------------------------*/
uint32_t h264bsdNextMbAddress(uint32_t *pSliceGroupMap, uint32_t picSizeInMbs, uint32_t currMbAddr) {
  uint32_t i, sliceGroup;


  ASSERT(pSliceGroupMap);
  ASSERT(picSizeInMbs);
  ASSERT(currMbAddr < picSizeInMbs);

  sliceGroup = pSliceGroupMap[currMbAddr];

  i = currMbAddr + 1;
  while((i < picSizeInMbs) && (pSliceGroupMap[i] != sliceGroup))
    i++;

  if(i == picSizeInMbs)
    i=0;

  return(i);
	}


/*------------------------------------------------------------------------------

   5.5  Function: h264bsdSetCurrImageMbPointers

        Functional description:
            Set luma and chroma pointers in image_t for current MB

        Inputs:
            image       Current image
            mbNum       number of current MB

        Outputs:
            none

        Returns:
            none
------------------------------------------------------------------------------*/
void h264bsdSetCurrImageMbPointers(image_t *image, uint32_t mbNum) {
  uint32_t width, height;
  uint32_t picSize;
  uint32_t row, col;
  uint32_t tmp;

  width = image->width;
  height = image->height;
  row = mbNum / width;
  col = mbNum % width;

  tmp = row * width;
  picSize = width * height;

  image->luma = (uint8_t*)(image->data + col * MACROBLOCK_SIZE + tmp * 256);
  image->cb = (uint8_t*)(image->data + picSize * 256 + tmp * 64 + col * 8);
  image->cr = (uint8_t*)(image->cb + picSize * 64);
	}




// per VLC ***

/* definition of special code num, this along with the return value is used
 * to handle code num in the range [0, 2^32] in the DecodeExpGolombUnsigned
 * function */
#define BIG_CODE_NUM 0xFFFFFFFFU

/* Mapping tables for coded_block_pattern, used for decoding of mapped
 * Exp-Golomb codes */
static const uint8_t codedBlockPatternIntra4x4[48] = {
    47,31,15,0,23,27,29,30,7,11,13,14,39,43,45,46,16,3,5,10,12,19,21,26,28,35,
    37,42,44,1,2,4,8,17,18,20,24,6,9,22,25,32,33,34,36,40,38,41};

static const uint8_t codedBlockPatternInter[48] = {
    0,16,1,2,4,8,32,3,5,10,12,15,47,7,11,13,14,6,9,31,35,37,42,44,33,34,36,40,
    39,43,45,46,17,18,20,24,19,21,26,28,23,27,29,30,22,25,38,41};


/*------------------------------------------------------------------------------

   5.1  Function: h264bsdDecodeExpGolombUnsigned

        Functional description:
            Decode unsigned Exp-Golomb code. This is the same as codeNum used
            in other Exp-Golomb code mappings. Code num (i.e. the decoded
            symbol) is determined as

                codeNum = 2^leadingZeros - 1 + GetBits(leadingZeros)

            Normal decoded symbols are in the range [0, 2^32 - 2]. Symbol
            2^32-1 is indicated by BIG_CODE_NUM with return value HANTRO_OK
            while symbol 2^32  is indicated by BIG_CODE_NUM with return value
            HANTRO_NOK.  These two symbols are special cases with code length
            of 65, i.e.  32 '0' bits, a '1' bit, and either 0 or 1 represented
            by 32 bits.

            Symbol 2^32 is out of unsigned 32-bit range but is needed for
            DecodeExpGolombSigned to express value -2^31.

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            codeNum         decoded code word is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      failure, no valid code word found, note exception
                            with BIG_CODE_NUM

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeExpGolombUnsigned(strmData_t *pStrmData, uint32_t *codeNum) {
  uint32_t bits, numZeros;


  ASSERT(pStrmData);
  ASSERT(codeNum);

  bits = h264bsdShowBits32(pStrmData);

  /* first bit is 1 -> code length 1 */
  if(bits >= 0x80000000) {
    h264bsdFlushBits(pStrmData, 1);
    *codeNum=0;
    return(HANTRO_OK);
    }
  /* second bit is 1 -> code length 3 */
  else if(bits >= 0x40000000) {
    if(h264bsdFlushBits(pStrmData, 3) == END_OF_STREAM)
      return(HANTRO_NOK);
    *codeNum = 1 + ((bits >> 29) & 0x1);
    return(HANTRO_OK);
    }
  /* third bit is 1 -> code length 5 */
  else if(bits >= 0x20000000) {
    if(h264bsdFlushBits(pStrmData, 5) == END_OF_STREAM)
      return(HANTRO_NOK);
    *codeNum = 3 + ((bits >> 27) & 0x3);
    return(HANTRO_OK);
		}
  /* fourth bit is 1 -> code length 7 */
  else if(bits >= 0x10000000) {
    if(h264bsdFlushBits(pStrmData, 7) == END_OF_STREAM)
      return(HANTRO_NOK);
    *codeNum = 7 + ((bits >> 25) & 0x7);
    return(HANTRO_OK);
		}
    /* other code lengths */
  else {
#ifndef H264DEC_NEON
  numZeros = 4 + h264bsdCountLeadingZeros(bits, 28);
#else
  numZeros = h264bsdCountLeadingZeros(bits);
#endif
    /* all 32 bits are zero */
    if(numZeros == 32) {
      *codeNum=0;
      h264bsdFlushBits(pStrmData,32);
      bits = h264bsdGetBits(pStrmData, 1);
      /* check 33rd bit, must be 1 */
      if(bits == 1) {
        /* cannot use h264bsdGetBits, limited to 31 bits */
        bits = h264bsdShowBits32(pStrmData);
        if(h264bsdFlushBits(pStrmData, 32) == END_OF_STREAM)
          return(HANTRO_NOK);
        /* code num 2^32 - 1, needed for unsigned mapping */
        if(bits == 0) {
          *codeNum = BIG_CODE_NUM;
          return(HANTRO_OK);
          }
        /* code num 2^32, needed for unsigned mapping
         * (results in -2^31) */
        else if(bits == 1) {
          *codeNum = BIG_CODE_NUM;
          return(HANTRO_NOK);
          }
        }
      /* if more zeros than 32, it is an error */
      return(HANTRO_NOK);
	    }
    else
      h264bsdFlushBits(pStrmData,numZeros+1);

    bits = h264bsdGetBits(pStrmData, numZeros);
    if(bits == END_OF_STREAM)
      return(HANTRO_NOK);

    *codeNum = (1 << numZeros) - 1 + bits;
    }

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

   5.2  Function: h264bsdDecodeExpGolombSigned

        Functional description:
            Decode signed Exp-Golomb code. Code num is determined by
            h264bsdDecodeExpGolombUnsigned and then mapped to signed
            representation as

                symbol = (-1)^(codeNum+1) * (codeNum+1)/2

            Signed symbols shall be in the range [-2^31, 2^31 - 1]. Symbol
            -2^31 is obtained when codeNum is 2^32, which cannot be expressed
            by unsigned 32-bit value. This is signaled as a special case from
            the h264bsdDecodeExpGolombUnsigned by setting codeNum to
            BIG_CODE_NUM and returning HANTRO_NOK status.

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            value           decoded code word is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      failure, no valid code word found

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeExpGolombSigned(strmData_t *pStrmData, int32_t *value) {
  uint32_t status, codeNum=0;


  ASSERT(pStrmData);
  ASSERT(value);

  status = h264bsdDecodeExpGolombUnsigned(pStrmData, &codeNum);

  if(codeNum == BIG_CODE_NUM) {
    /* BIG_CODE_NUM and HANTRO_OK status means codeNum 2^32-1 which would
     * result in signed integer valued 2^31 (i.e. out of 32-bit signed
     * integer range) */
    if(status == HANTRO_OK)
        return(HANTRO_NOK);
    /* BIG_CODE_NUM and HANTRO_NOK status means codeNum 2^32 which results
     * in signed integer valued -2^31 */
    else {
      *value = (int32_t)(2147483648U);
      return (HANTRO_OK);
      }
		}
  else if(status == HANTRO_OK) {
    /* (-1)^(codeNum+1) results in positive sign if codeNum is odd,
     * negative when it is even. (codeNum+1)/2 is obtained as
     * (codeNum+1)>>1 when value is positive and as (-codeNum)>>1 for
     * negative value */
    /*lint -e702 */
    *value = (codeNum & 0x1) ? (int32_t)((codeNum + 1) >> 1) :
                              -(int32_t)((codeNum + 1) >> 1);
    return(HANTRO_OK);
    }

  return(HANTRO_NOK);
	}

/*------------------------------------------------------------------------------

   5.3  Function: h264bsdDecodeExpGolombMapped

        Functional description:
            Decode mapped Exp-Golomb code. Code num is determined by
            h264bsdDecodeExpGolombUnsigned and then mapped to codedBlockPattern
            either for intra or inter macroblock. The mapping is implemented by
            look-up tables defined in the beginning of the file.

        Inputs:
            pStrmData       pointer to stream data structure
            isIntra         flag to indicate if intra or inter mapping is to
                            be used

        Outputs:
            value           decoded code word is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      failure, no valid code word found

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeExpGolombMapped(strmData_t *pStrmData, uint32_t *value, uint32_t isIntra) {
  uint32_t status, codeNum;


  ASSERT(pStrmData);
  ASSERT(value);

  status = h264bsdDecodeExpGolombUnsigned(pStrmData, &codeNum);

  if(status != HANTRO_OK)
    return (HANTRO_NOK);
  else  {
    /* range of valid codeNums [0,47] */
    if(codeNum > 47)
      return (HANTRO_NOK);
    if(isIntra)
      *value = codedBlockPatternIntra4x4[codeNum];
    else
      *value = codedBlockPatternInter[codeNum];
    return(HANTRO_OK);
    }

	}

/*------------------------------------------------------------------------------

   5.4  Function: h264bsdDecodeExpGolombTruncated

        Functional description:
            Decode truncated Exp-Golomb code. greaterThanOne flag indicates
            the range of the symbol to be decoded as follows:
                FALSE   ->  [0,1]
                TRUE    ->  [0,2^32-1]

            If flag is false the decoding is performed by reading one bit
            from the stream with h264bsdGetBits and mapping this to decoded
            symbol as
                symbol = bit ? 0 : 1

            Otherwise, i.e. when flag is TRUE, code num is determined by
            h264bsdDecodeExpGolombUnsigned and this is used as the decoded
            symbol.

        Inputs:
            pStrmData       pointer to stream data structure
            greaterThanOne  flag to indicate if range is wider than [0,1]

        Outputs:
            value           decoded code word is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      failure, no valid code word found

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeExpGolombTruncated(strmData_t *pStrmData,
  uint32_t *value, uint32_t greaterThanOne) {


  ASSERT(pStrmData);
  ASSERT(value);

  if(greaterThanOne) {
    return(h264bsdDecodeExpGolombUnsigned(pStrmData, value));
		}
  else {
    *value = h264bsdGetBits(pStrmData,1);
    if(*value == END_OF_STREAM)
      return (HANTRO_NOK);
    *value ^= 0x1;
		}

  return (HANTRO_OK);
	}



#define MAX_DPB_SIZE 16
#define MAX_BR       240000 /* for level 5.1 */
#define MAX_CPB      240000 /* for level 5.1 */


static uint32_t DecodeHrdParameters(strmData_t *pStrmData, hrdParameters_t *pHrdParameters);

/*------------------------------------------------------------------------------

    Function: h264bsdDecodeVuiParameters

        Functional description:
            Decode VUI parameters from the stream. See standard for details.

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            pVuiParameters  decoded information is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data or end of stream

------------------------------------------------------------------------------*/
uint32_t h264bsdDecodeVuiParameters(strmData_t *pStrmData, vuiParameters_t *pVuiParameters) {
  uint32_t tmp;


  ASSERT(pStrmData);
  ASSERT(pVuiParameters);

  memset(pVuiParameters, 0, sizeof(vuiParameters_t));

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pVuiParameters->aspectRatioPresentFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  if(pVuiParameters->aspectRatioPresentFlag) {
      tmp = h264bsdGetBits(pStrmData, 8);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pVuiParameters->aspectRatioIdc = tmp;

      if(pVuiParameters->aspectRatioIdc == ASPECT_RATIO_EXTENDED_SAR) {
          tmp = h264bsdGetBits(pStrmData, 16);
          if(tmp == END_OF_STREAM)
              return(HANTRO_NOK);
          pVuiParameters->sarWidth = tmp;

          tmp = h264bsdGetBits(pStrmData, 16);
          if(tmp == END_OF_STREAM)
              return(HANTRO_NOK);
          pVuiParameters->sarHeight = tmp;
      }
		}

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pVuiParameters->overscanInfoPresentFlag = (tmp == 1) ?
                              HANTRO_TRUE : HANTRO_FALSE;

  if(pVuiParameters->overscanInfoPresentFlag) {
      tmp = h264bsdGetBits(pStrmData, 1);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pVuiParameters->overscanAppropriateFlag = (tmp == 1) ?
                              HANTRO_TRUE : HANTRO_FALSE;
	  }

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pVuiParameters->videoSignalTypePresentFlag = (tmp == 1) ?
                              HANTRO_TRUE : HANTRO_FALSE;

  if(pVuiParameters->videoSignalTypePresentFlag) {
      tmp = h264bsdGetBits(pStrmData, 3);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pVuiParameters->videoFormat = tmp;

      tmp = h264bsdGetBits(pStrmData, 1);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pVuiParameters->videoFullRangeFlag = (tmp == 1) ?
                              HANTRO_TRUE : HANTRO_FALSE;

      tmp = h264bsdGetBits(pStrmData, 1);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pVuiParameters->colourDescriptionPresentFlag =
          (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

      if(pVuiParameters->colourDescriptionPresentFlag) {
          tmp = h264bsdGetBits(pStrmData, 8);
          if(tmp == END_OF_STREAM)
              return(HANTRO_NOK);
          pVuiParameters->colourPrimaries = tmp;

          tmp = h264bsdGetBits(pStrmData, 8);
          if(tmp == END_OF_STREAM)
              return(HANTRO_NOK);
          pVuiParameters->transferCharacteristics = tmp;

          tmp = h264bsdGetBits(pStrmData, 8);
          if(tmp == END_OF_STREAM)
              return(HANTRO_NOK);
          pVuiParameters->matrixCoefficients = tmp;
      }
      else {
          pVuiParameters->colourPrimaries         = 2;
          pVuiParameters->transferCharacteristics = 2;
          pVuiParameters->matrixCoefficients      = 2;
      }
		}
  else {
      pVuiParameters->videoFormat             = 5;
      pVuiParameters->colourPrimaries         = 2;
      pVuiParameters->transferCharacteristics = 2;
      pVuiParameters->matrixCoefficients      = 2;
		}

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pVuiParameters->chromaLocInfoPresentFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  if(pVuiParameters->chromaLocInfoPresentFlag) {
      tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
        &pVuiParameters->chromaSampleLocTypeTopField);
      if(tmp != HANTRO_OK)
          return tmp;
      if(pVuiParameters->chromaSampleLocTypeTopField > 5)
          return(HANTRO_NOK);

      tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
        &pVuiParameters->chromaSampleLocTypeBottomField);
      if(tmp != HANTRO_OK)
          return tmp;
      if(pVuiParameters->chromaSampleLocTypeBottomField > 5)
          return(HANTRO_NOK);
  }

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pVuiParameters->timingInfoPresentFlag =
      (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  if(pVuiParameters->timingInfoPresentFlag) {
      tmp = h264bsdShowBits32(pStrmData);
      if(h264bsdFlushBits(pStrmData, 32) == END_OF_STREAM)
          return(HANTRO_NOK);
      if(tmp == 0)
          return(HANTRO_NOK);
      pVuiParameters->numUnitsInTick = tmp;

      tmp = h264bsdShowBits32(pStrmData);
      if(h264bsdFlushBits(pStrmData, 32) == END_OF_STREAM)
          return(HANTRO_NOK);
      if(tmp == 0)
          return(HANTRO_NOK);
      pVuiParameters->timeScale = tmp;

      tmp = h264bsdGetBits(pStrmData, 1);
      if(tmp == END_OF_STREAM)
          return(HANTRO_NOK);
      pVuiParameters->fixedFrameRateFlag =
          (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;
		}

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pVuiParameters->nalHrdParametersPresentFlag =
      (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  if(pVuiParameters->nalHrdParametersPresentFlag) {
      tmp = DecodeHrdParameters(pStrmData, &pVuiParameters->nalHrdParameters);
      if(tmp != HANTRO_OK)
          return tmp;
		}
  else {
      pVuiParameters->nalHrdParameters.cpbCnt          = 1;
      /* MaxBR and MaxCPB should be the values correspondig to the levelIdc
       * in the SPS containing these VUI parameters. However, these values
       * are not used anywhere and maximum for any level will be used here */
      pVuiParameters->nalHrdParameters.bitRateValue[0] = 1200 * MAX_BR + 1;
      pVuiParameters->nalHrdParameters.cpbSizeValue[0] = 1200 * MAX_CPB + 1;
      pVuiParameters->nalHrdParameters.initialCpbRemovalDelayLength = 24;
      pVuiParameters->nalHrdParameters.cpbRemovalDelayLength        = 24;
      pVuiParameters->nalHrdParameters.dpbOutputDelayLength         = 24;
      pVuiParameters->nalHrdParameters.timeOffsetLength             = 24;
  }

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pVuiParameters->vclHrdParametersPresentFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  if(pVuiParameters->vclHrdParametersPresentFlag) {
      tmp = DecodeHrdParameters(pStrmData, &pVuiParameters->vclHrdParameters);
      if(tmp != HANTRO_OK)
          return tmp;
		}
  else {
      pVuiParameters->vclHrdParameters.cpbCnt          = 1;
      /* MaxBR and MaxCPB should be the values correspondig to the levelIdc
       * in the SPS containing these VUI parameters. However, these values
       * are not used anywhere and maximum for any level will be used here */
      pVuiParameters->vclHrdParameters.bitRateValue[0] = 1000 * MAX_BR + 1;
      pVuiParameters->vclHrdParameters.cpbSizeValue[0] = 1000 * MAX_CPB + 1;
      pVuiParameters->vclHrdParameters.initialCpbRemovalDelayLength = 24;
      pVuiParameters->vclHrdParameters.cpbRemovalDelayLength        = 24;
      pVuiParameters->vclHrdParameters.dpbOutputDelayLength         = 24;
      pVuiParameters->vclHrdParameters.timeOffsetLength             = 24;
		}

  if(pVuiParameters->nalHrdParametersPresentFlag ||
    pVuiParameters->vclHrdParametersPresentFlag) {
    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pVuiParameters->lowDelayHrdFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;
	  }

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pVuiParameters->picStructPresentFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  tmp = h264bsdGetBits(pStrmData, 1);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pVuiParameters->bitstreamRestrictionFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

  if(pVuiParameters->bitstreamRestrictionFlag) {
    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pVuiParameters->motionVectorsOverPicBoundariesFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pVuiParameters->maxBytesPerPicDenom);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pVuiParameters->maxBytesPerPicDenom > 16)
        return(HANTRO_NOK);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pVuiParameters->maxBitsPerMbDenom);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pVuiParameters->maxBitsPerMbDenom > 16)
        return(HANTRO_NOK);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pVuiParameters->log2MaxMvLengthHorizontal);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pVuiParameters->log2MaxMvLengthHorizontal > 16)
        return(HANTRO_NOK);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pVuiParameters->log2MaxMvLengthVertical);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pVuiParameters->log2MaxMvLengthVertical > 16)
        return(HANTRO_NOK);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pVuiParameters->numReorderFrames);
    if(tmp != HANTRO_OK)
        return tmp;

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pVuiParameters->maxDecFrameBuffering);
    if(tmp != HANTRO_OK)
        return tmp;
		}
  else {
    pVuiParameters->motionVectorsOverPicBoundariesFlag = HANTRO_TRUE;
    pVuiParameters->maxBytesPerPicDenom       = 2;
    pVuiParameters->maxBitsPerMbDenom         = 1;
    pVuiParameters->log2MaxMvLengthHorizontal = 16;
    pVuiParameters->log2MaxMvLengthVertical   = 16;
    pVuiParameters->numReorderFrames          = MAX_DPB_SIZE;
    pVuiParameters->maxDecFrameBuffering      = MAX_DPB_SIZE;
		}

  return(HANTRO_OK);
	}

/*------------------------------------------------------------------------------

    Function: DecodeHrdParameters

        Functional description:
            Decode HRD parameters from the stream. See standard for details.

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            pHrdParameters  decoded information is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/
static uint32_t DecodeHrdParameters(strmData_t *pStrmData, hrdParameters_t *pHrdParameters) {
  uint32_t tmp, i;



  ASSERT(pStrmData);
  ASSERT(pHrdParameters);


  tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &pHrdParameters->cpbCnt);
  if(tmp != HANTRO_OK)
      return tmp;
  /* cpbCount = cpb_cnt_minus1 + 1 */
  pHrdParameters->cpbCnt++;
  if(pHrdParameters->cpbCnt > MAX_CPB_CNT)
      return(HANTRO_NOK);

  tmp = h264bsdGetBits(pStrmData, 4);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pHrdParameters->bitRateScale = tmp;

  tmp = h264bsdGetBits(pStrmData, 4);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pHrdParameters->cpbSizeScale = tmp;

  for(i=0; i < pHrdParameters->cpbCnt; i++) {
    /* bit_rate_value_minus1 in the range [0, 2^32 - 2] */
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pHrdParameters->bitRateValue[i]);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pHrdParameters->bitRateValue[i] > 4294967294U)
        return(HANTRO_NOK);
    pHrdParameters->bitRateValue[i]++;
    /* this may result in overflow, but this value is not used for
     * anything */
    pHrdParameters->bitRateValue[i] *=
        1 << (6 + pHrdParameters->bitRateScale);

    /* cpb_size_value_minus1 in the range [0, 2^32 - 2] */
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
      &pHrdParameters->cpbSizeValue[i]);
    if(tmp != HANTRO_OK)
        return tmp;
    if(pHrdParameters->cpbSizeValue[i] > 4294967294U)
        return(HANTRO_NOK);
    pHrdParameters->cpbSizeValue[i]++;
    /* this may result in overflow, but this value is not used for
     * anything */
    pHrdParameters->cpbSizeValue[i] *=
        1 << (4 + pHrdParameters->cpbSizeScale);

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return(HANTRO_NOK);
	  pHrdParameters->cbrFlag[i] = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;
		}

  tmp = h264bsdGetBits(pStrmData, 5);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pHrdParameters->initialCpbRemovalDelayLength = tmp + 1;

  tmp = h264bsdGetBits(pStrmData, 5);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pHrdParameters->cpbRemovalDelayLength = tmp + 1;

  tmp = h264bsdGetBits(pStrmData, 5);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pHrdParameters->dpbOutputDelayLength = tmp + 1;

  tmp = h264bsdGetBits(pStrmData, 5);
  if(tmp == END_OF_STREAM)
      return(HANTRO_NOK);
  pHrdParameters->timeOffsetLength = tmp;

  return(HANTRO_OK);
	}

