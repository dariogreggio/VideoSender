/*
 * Copyright (C) 2009 The Android Open Source Project
 * Modified for use by h264bsd standalone library
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * GD/C adapted 12/1/2026 
 */

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>


#define _DEBUG_PRINT
#define _ERROR_PRINT

#define VOLATILE


#if defined(VC1SWDEC_16BIT) || defined(MP4ENC_ARM11)
typedef unsigned short  u16x;
typedef signed short    i16x;
#else
typedef unsigned int    u16x;
typedef signed int      i16x;
#endif



typedef struct {
  uint8_t  *pStrmBuffStart;    /* pointer to start of stream buffer */
  uint8_t  *pStrmCurrPos;      /* current read address in stream buffer */
  uint32_t  bitPosInWord;      /* bit position in stream buffer byte */
  uint32_t  strmBuffSize;      /* size of stream buffer (bytes) */
  uint32_t  strmBuffReadBits;  /* number of bits read from stream buffer */
	} strmData_t;


uint32_t h264bsdExtractNalUnit(uint8_t *pByteStream, uint32_t len, strmData_t *pStrmData,
    uint32_t *readBytes);
uint32_t h264bsdDecodeResidualBlockCavlc(strmData_t *pStrmData,
  int32_t *coeffLevel, int32_t nc, uint32_t maxNumCoeff);


#define MAX_NUM_REF_PICS 16
#define MAX_NUM_SLICE_GROUPS 8
#define MAX_NUM_SEQ_PARAM_SETS 32
#define MAX_NUM_PIC_PARAM_SETS 256

#define MACROBLOCK_SIZE 16			// OCCHIO cmq :)

/* data structure to store PPS information decoded from the stream */
typedef struct {
  uint32_t picParameterSetId;
  uint32_t seqParameterSetId;
  uint32_t picOrderPresentFlag;
  uint32_t numSliceGroups;
  uint32_t sliceGroupMapType;
  uint32_t *runLength;
  uint32_t *topLeft;
  uint32_t *bottomRight;
  uint32_t sliceGroupChangeDirectionFlag;
  uint32_t sliceGroupChangeRate;
  uint32_t picSizeInMapUnits;
  uint32_t *sliceGroupId;
  uint32_t numRefIdxL0Active;
  uint32_t picInitQp;
  int32_t chromaQpIndexOffset;
  uint32_t deblockingFilterControlPresentFlag;
  uint32_t constrainedIntraPredFlag;
  uint32_t redundantPicCntPresentFlag;
	} picParamSet_t;


#define MAX_CPB_CNT 32
/* structure to store Hypothetical Reference Decoder (HRD) parameters */
typedef struct {
  uint32_t cpbCnt;
  uint32_t bitRateScale;
  uint32_t cpbSizeScale;
  uint32_t bitRateValue[MAX_CPB_CNT];
  uint32_t cpbSizeValue[MAX_CPB_CNT];
  uint32_t cbrFlag[MAX_CPB_CNT];
  uint32_t initialCpbRemovalDelayLength;
  uint32_t cpbRemovalDelayLength;
  uint32_t dpbOutputDelayLength;
  uint32_t timeOffsetLength;
	} hrdParameters_t;

/* storage for VUI parameters */
typedef struct {
  uint32_t aspectRatioPresentFlag;
  uint32_t aspectRatioIdc;
  uint32_t sarWidth;
  uint32_t sarHeight;
  uint32_t overscanInfoPresentFlag;
  uint32_t overscanAppropriateFlag;
  uint32_t videoSignalTypePresentFlag;
  uint32_t videoFormat;
  uint32_t videoFullRangeFlag;
  uint32_t colourDescriptionPresentFlag;
  uint32_t colourPrimaries;
  uint32_t transferCharacteristics;
  uint32_t matrixCoefficients;
  uint32_t chromaLocInfoPresentFlag;
  uint32_t chromaSampleLocTypeTopField;
  uint32_t chromaSampleLocTypeBottomField;
  uint32_t timingInfoPresentFlag;
  uint32_t numUnitsInTick;
  uint32_t timeScale;
  uint32_t fixedFrameRateFlag;
  uint32_t nalHrdParametersPresentFlag;
  hrdParameters_t nalHrdParameters;
  uint32_t vclHrdParametersPresentFlag;
  hrdParameters_t vclHrdParameters;
  uint32_t lowDelayHrdFlag;
  uint32_t picStructPresentFlag;
  uint32_t bitstreamRestrictionFlag;
  uint32_t motionVectorsOverPicBoundariesFlag;
  uint32_t maxBytesPerPicDenom;
  uint32_t maxBitsPerMbDenom;
  uint32_t log2MaxMvLengthHorizontal;
  uint32_t log2MaxMvLengthVertical;
  uint32_t numReorderFrames;
  uint32_t maxDecFrameBuffering;
	} vuiParameters_t;

/* structure to store sequence parameter set information decoded from the stream */
typedef struct {
  uint32_t profileIdc;
  uint32_t levelIdc;
  uint32_t seqParameterSetId;
  uint32_t maxFrameNum;
  uint32_t picOrderCntType;
  uint32_t maxPicOrderCntLsb;
  uint32_t deltaPicOrderAlwaysZeroFlag;
  int32_t offsetForNonRefPic;
  int32_t offsetForTopToBottomField;
  uint32_t numRefFramesInPicOrderCntCycle;
  int32_t *offsetForRefFrame;
  uint32_t numRefFrames;
  uint32_t gapsInFrameNumValueAllowedFlag;
  uint32_t picWidthInMbs;
  uint32_t picHeightInMbs;
  uint32_t frameCroppingFlag;
  uint32_t frameCropLeftOffset;
  uint32_t frameCropRightOffset;
  uint32_t frameCropTopOffset;
  uint32_t frameCropBottomOffset;
  uint32_t vuiParametersPresentFlag;
  vuiParameters_t *vuiParameters;
  uint32_t maxDpbSize;
	uint32_t scalingList[64];
	} seqParamSet_t;

typedef struct {
  uint32_t sliceId;
  uint32_t numDecodedMbs;
  uint32_t lastMbAddr;
	} sliceStorage_t;

typedef enum {
  P_Skip          = 0,
  P_L0_16x16      = 1,
  P_L0_L0_16x8    = 2,
  P_L0_L0_8x16    = 3,
  P_8x8           = 4,
  P_8x8ref0       = 5,
  I_4x4           = 6,
  I_16x16_0_0_0   = 7,
  I_16x16_1_0_0   = 8,
  I_16x16_2_0_0   = 9,
  I_16x16_3_0_0   = 10,
  I_16x16_0_1_0   = 11,
  I_16x16_1_1_0   = 12,
  I_16x16_2_1_0   = 13,
  I_16x16_3_1_0   = 14,
  I_16x16_0_2_0   = 15,
  I_16x16_1_2_0   = 16,
  I_16x16_2_2_0   = 17,
  I_16x16_3_2_0   = 18,
  I_16x16_0_0_1   = 19,
  I_16x16_1_0_1   = 20,
  I_16x16_2_0_1   = 21,
  I_16x16_3_0_1   = 22,
  I_16x16_0_1_1   = 23,
  I_16x16_1_1_1   = 24,
  I_16x16_2_1_1   = 25,
  I_16x16_3_1_1   = 26,
  I_16x16_0_2_1   = 27,
  I_16x16_1_2_1   = 28,
  I_16x16_2_2_1   = 29,
  I_16x16_3_2_1   = 30,
  I_PCM           = 31
	} mbType_e;

typedef struct {
  /* MvPrediction16x16 assumes that MVs are 16bits */
  int16_t hor;
  int16_t ver;
	} mv_t;
typedef struct mbStorage {
  mbType_e mbType;
  uint32_t sliceId;
  uint32_t disableDeblockingFilterIdc;
  int32_t filterOffsetA;
  int32_t filterOffsetB;
  uint32_t qpY;
  int32_t chromaQpIndexOffset;
#ifdef H264DEC_OMXDL
  uint8_t totalCoeff[27];
#else
  int16_t totalCoeff[27];
#endif
  uint8_t intra4x4PredMode[16];
  uint32_t refPic[4];
  uint8_t* refAddr[4];
  mv_t mv[16];
  uint32_t decoded;
  struct mbStorage *mbA;
  struct mbStorage *mbB;
  struct mbStorage *mbC;
  struct mbStorage *mbD;
	} mbStorage_t;

/* enumeration to represent status of buffered image */
typedef enum {
  UNUSED = 0,
  NON_EXISTING,
  SHORT_TERM,
  LONG_TERM
	} dpbPictureStatus_e;

/* structure to represent a buffered picture */
typedef struct {
  uint8_t *data;           /* 16-byte aligned pointer of pAllocatedData */
  uint8_t *pAllocatedData; /* allocated picture pointer; (size + 15) bytes */
  int32_t picNum;
  uint32_t frameNum;
  int32_t picOrderCnt;
  dpbPictureStatus_e status;
  uint32_t toBeDisplayed;
  uint32_t picId;
  uint32_t numErrMbs;
  uint32_t isIdr;
	} dpbPicture_t;


/* structure to represent display image output from the buffer */
typedef struct {
  uint8_t *data;
  uint32_t picId;
  uint32_t numErrMbs;
  uint32_t isIdr;
	} dpbOutPicture_t;

/* structure to represent DPB */
typedef struct {
  dpbPicture_t *buffer;
  dpbPicture_t **list;
  dpbPicture_t *currentOut;
  dpbOutPicture_t *outBuf;
  uint32_t numOut;
  uint32_t outIndex;
  uint32_t maxRefFrames;
  uint32_t dpbSize;
  uint32_t maxFrameNum;
  uint32_t maxLongTermFrameIdx;
  uint32_t numRefFrames;
  uint32_t fullness;
  uint32_t prevRefFrameNum;
  uint32_t lastContainsMmco5;
  uint32_t noReordering;
  uint32_t flushed;
	} dpbStorage_t;

/* structure to store information computed for previous picture, needed for
 * POC computation of a picture. Two first fields for POC type 0, last two
 * for types 1 and 2 */
typedef struct {
  uint32_t prevPicOrderCntLsb;
  int32_t prevPicOrderCntMsb;
  uint32_t prevFrameNum;
  uint32_t prevFrameNumOffset;
	} pocStorage_t;

typedef enum {
  NAL_CODED_SLICE = 1,
  NAL_CODED_SLICE_IDR = 5,
  NAL_SEI = 6,
  NAL_SEQ_PARAM_SET = 7,
  NAL_PIC_PARAM_SET = 8,
  NAL_ACCESS_UNIT_DELIMITER = 9,
  NAL_END_OF_SEQUENCE = 10,
  NAL_END_OF_STREAM = 11,
  NAL_FILLER_DATA = 12,
  NAL_MAX_TYPE_VALUE = 31
	} nalUnitType_e;

typedef struct {
  nalUnitType_e nalUnitType;
  uint32_t nalRefIdc;
	} nalUnit_t;

/* structure to store parameters needed for access unit boundary checking */
typedef struct {
  nalUnit_t nuPrev[1];
  uint32_t prevFrameNum;
  uint32_t prevIdrPicId;
  uint32_t prevPicOrderCntLsb;
  int32_t prevDeltaPicOrderCntBottom;
  int32_t prevDeltaPicOrderCnt[2];
  uint32_t firstCallFlag;
	} aubCheck_t;

typedef struct {
  uint8_t *data;
  uint32_t width;
  uint32_t height;
  /* current MB's components */
  uint8_t *luma;
  uint8_t *cb;
  uint8_t *cr;
	} image_t;


/* structure to store data of one reference picture list reordering operation */
typedef struct {
  uint32_t reorderingOfPicNumsIdc;
  uint32_t absDiffPicNum;
  uint32_t longTermPicNum;
	} refPicListReorderingOperation_t;

/* structure to store reference picture list reordering operations */
typedef struct {
  uint32_t refPicListReorderingFlagL0;
  refPicListReorderingOperation_t command[MAX_NUM_REF_PICS+1];
	} refPicListReordering_t;

/* structure to store data of one DPB memory management control operation */
typedef struct {
  uint32_t memoryManagementControlOperation;
  uint32_t differenceOfPicNums;
  uint32_t longTermPicNum;
  uint32_t longTermFrameIdx;
  uint32_t maxLongTermFrameIdx;
	} memoryManagementOperation_t;

/* worst case scenario: all MAX_NUM_REF_PICS pictures in the buffer are
 * short term pictures, each one of them is first marked as long term
 * reference picture which is then marked as unused for reference.
 * Additionally, max long-term frame index is set and current picture is
 * marked as long term reference picture. Last position reserved for
 * end memory_management_control_operation command */
#define MAX_NUM_MMC_OPERATIONS (2*MAX_NUM_REF_PICS+2+1)
/* structure to store decoded reference picture marking data */
typedef struct {
  uint32_t noOutputOfPriorPicsFlag;
  uint32_t longTermReferenceFlag;
  uint32_t adaptiveRefPicMarkingModeFlag;
  memoryManagementOperation_t operation[MAX_NUM_MMC_OPERATIONS];
	} decRefPicMarking_t;

/* structure to store slice header data decoded from the stream */
typedef struct {
  uint32_t firstMbInSlice;
  uint32_t sliceType;
  uint32_t picParameterSetId;
  uint32_t frameNum;
  uint32_t idrPicId;
  uint32_t picOrderCntLsb;
  int32_t deltaPicOrderCntBottom;
  int32_t deltaPicOrderCnt[2];
  uint32_t redundantPicCnt;
  uint32_t numRefIdxActiveOverrideFlag;
  uint32_t numRefIdxL0Active;
  int32_t sliceQpDelta;
  uint32_t disableDeblockingFilterIdc;
  int32_t sliceAlphaC0Offset;
  int32_t sliceBetaOffset;
  uint32_t sliceGroupChangeCycle;
  refPicListReordering_t refPicListReordering;
  decRefPicMarking_t decRefPicMarking;
	} sliceHeader_t;

typedef struct {
  uint32_t prevIntra4x4PredModeFlag[16];
  uint32_t remIntra4x4PredMode[16];
  uint32_t intraChromaPredMode;
  uint32_t refIdxL0[4];
  mv_t mvdL0[4];
	} mbPred_t;


typedef enum {
  P_L0_8x8 = 0,
  P_L0_8x4 = 1,
  P_L0_4x8 = 2,
  P_L0_4x4 = 3
	} subMbType_e;

typedef struct {
  subMbType_e subMbType[4];
  uint32_t refIdxL0[4];
  mv_t mvdL0[4][4];
	} subMbPred_t;

typedef struct {
#ifdef H264DEC_OMXDL
  uint8_t posCoefBuf[27*16*3];
  uint8_t totalCoeff[27];
#else
  int16_t totalCoeff[27];
#endif
  int32_t level[26][16];
  uint32_t coeffMap[24];
	} residual_t;

typedef struct {
  mbType_e mbType;
  uint32_t codedBlockPattern;
  int32_t mbQpDelta;
  mbPred_t mbPred;
  subMbPred_t subMbPred;
  residual_t residual;
	} macroblockLayer_t;

/* storage data structure, holds all data of a decoder instance */
typedef struct storage {
  /* active parameter set ids and pointers */
  uint32_t oldSpsId;
  uint32_t activePpsId;
  uint32_t activeSpsId;
  picParamSet_t *activePps;
  seqParamSet_t *activeSps;
  seqParamSet_t *sps[MAX_NUM_SEQ_PARAM_SETS];
  picParamSet_t *pps[MAX_NUM_PIC_PARAM_SETS];
  /* current slice group map, recomputed for each slice */
  uint32_t *sliceGroupMap;
  uint32_t picSizeInMbs;
  /* this flag is set after all macroblocks of a picture successfully
   * decoded -> redundant slices not decoded */
  uint32_t skipRedundantSlices;
  uint32_t picStarted;
  /* flag to indicate if current access unit contains any valid slices */
  uint32_t validSliceInAccessUnit;
  /* store information needed for handling of slice decoding */
  sliceStorage_t slice[1];
  /* number of concealed macroblocks in the current image */
  uint32_t numConcealedMbs;
  /* picId given by application */
  uint32_t currentPicId;
  /* macroblock specific storages, size determined by image dimensions */
  mbStorage_t *mb;
  /* flag to store noOutputReordering flag set by the application */
  uint32_t noReordering;
  /* DPB */
  dpbStorage_t dpb[1];
  /* structure to store picture order count related information */
  pocStorage_t poc[1];
  /* access unit boundary checking related data */
  aubCheck_t aub[1];
  /* current processed image */
  image_t currImage[1];
  /* last valid NAL unit header is stored here */
  nalUnit_t prevNalUnit[1];
  /* slice header, second structure used as a temporary storage while
   * decoding slice header, first one stores last successfully decoded
   * slice header */
  sliceHeader_t sliceHeader[2];
  /* fields to store old stream buffer pointers, needed when only part of
   * a stream buffer is processed by h264bsdDecode function */
  uint32_t prevBufNotFinished;
  uint8_t *prevBufPointer;
  uint32_t prevBytesConsumed;
  strmData_t strm[1];
  /* macroblock layer structure, there is no need to store this but it
   * would have increased the stack size excessively and needed to be
   * allocated from head -> easiest to put it here */
  macroblockLayer_t *mbLayer;
  uint32_t pendingActivation; /* Activate parameter sets after returning
                            HEADERS_RDY to the user */
  uint32_t intraConcealmentFlag; /* 0 gray picture for corrupted intra
                               1 previous frame used if available */
  uint32_t* conversionBuffer; // used to perform yuv conversion
  size_t conversionBufferSize;
	} storage_t;


uint32_t h264bsdConceal(storage_t *pStorage, image_t *currImage, uint32_t sliceType);


/* String length for tracing */
#define H264DEC_TRACE_STR_LEN 100

typedef struct {
  enum {
    UNINITIALIZED,
    INITIALIZED,
    NEW_HEADERS
    } decStat;

  uint32_t picNumber;
  storage_t storage;
#ifdef H264DEC_TRACE
  char str[H264DEC_TRACE_STR_LEN];
#endif
	} decContainer_t;


void h264bsdFilterPicture(image_t *image,mbStorage_t *mb);


/* enumerated return values of the functions */
enum {
  H264BSD_RDY,
  H264BSD_PIC_RDY,
  H264BSD_HDRS_RDY,
  H264BSD_ERROR,
  H264BSD_PARAM_SET_ERROR,
  H264BSD_MEMALLOC_ERROR
	};

uint32_t h264bsdInit(storage_t *pStorage, uint32_t noOutputReordering);
uint32_t h264bsdDecode(storage_t *pStorage, uint8_t *byteStrm, uint32_t len, uint32_t picId,
    uint32_t *readBytes);
void h264bsdShutdown(storage_t *pStorage);

uint8_t* h264bsdNextOutputPicture(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic, uint32_t *numErrMbs);
uint32_t* h264bsdNextOutputPictureRGBA(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic, uint32_t *numErrMbs);
uint32_t* h264bsdNextOutputPictureBGRA(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic, uint32_t *numErrMbs);
uint32_t* h264bsdNextOutputPictureYCbCrA(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic, uint32_t *numErrMbs);
uint32_t* h264bsdNextOutputPictureBGR(storage_t *pStorage, uint32_t *picId, uint32_t *isIdrPic, uint32_t *numErrMbs);

uint32_t h264bsdPicWidth(storage_t *pStorage);
uint32_t h264bsdPicHeight(storage_t *pStorage);
uint32_t h264bsdVideoRange(storage_t *pStorage);
uint32_t h264bsdMatrixCoefficients(storage_t *pStorage);
void h264bsdCroppingParams(storage_t *pStorage, uint32_t *croppingFlag,
    uint32_t *left, uint32_t *width, uint32_t *top, uint32_t *height);
void h264bsdSampleAspectRatio(storage_t *pStorage, uint32_t *sarWidth, uint32_t *sarHeight);
uint32_t h264bsdCheckValidParamSets(storage_t *pStorage);

void h264bsdFlushBuffer(storage_t *pStorage);

uint32_t h264bsdProfile(storage_t *pStorage);

storage_t* h264bsdAlloc();
void h264bsdFree(storage_t *pStorage);

void h264bsdConvertToRGBA(uint32_t width, uint32_t height, uint8_t* data, uint32_t *pOutput);
void h264bsdConvertToBGRA(uint32_t width, uint32_t height, uint8_t* data, uint32_t *pOutput);
void h264bsdConvertToYCbCrA(uint32_t width, uint32_t height, uint8_t* data, uint32_t *pOutput);
void h264bsdConvertToBGR(uint32_t width, uint32_t height, uint8_t* data, uint8_t *pOutput);





uint32_t h264bsdInitDpb(dpbStorage_t *dpb,
  uint32_t picSizeInMbs, uint32_t dpbSize,
  uint32_t numRefFrames, uint32_t maxFrameNum,
  uint32_t noReordering);
uint32_t h264bsdResetDpb(dpbStorage_t *dpb,
  uint32_t picSizeInMbs, uint32_t dpbSize,
  uint32_t numRefFrames, uint32_t maxFrameNum,
  uint32_t noReordering);
void h264bsdInitRefPicList(dpbStorage_t *dpb);
uint8_t* h264bsdAllocateDpbImage(dpbStorage_t *dpb);
uint8_t* h264bsdGetRefPicData(dpbStorage_t *dpb, uint32_t index);
uint32_t h264bsdReorderRefPicList(dpbStorage_t *dpb,
  refPicListReordering_t *order, uint32_t currFrameNum,
  uint32_t numRefIdxActive);
uint32_t h264bsdMarkDecRefPic(dpbStorage_t *dpb, decRefPicMarking_t *mark,
  image_t *image, uint32_t frameNum,
  int32_t picOrderCnt, uint32_t isIdr,
  uint32_t picId, uint32_t numErrMbs);
uint32_t h264bsdCheckGapsInFrameNum(dpbStorage_t *dpb, uint32_t frameNum, uint32_t isRefPic,
                               uint32_t gapsAllowed);
dpbOutPicture_t* h264bsdDpbOutputPicture(dpbStorage_t *dpb);
void h264bsdFlushDpb(dpbStorage_t *dpb);
void h264bsdFreeDpb(dpbStorage_t *dpb);
void h264bsdWriteMacroblock(image_t *image, uint8_t *data);

#ifndef H264DEC_OMXDL
void h264bsdWriteOutputBlocks(image_t *image, uint32_t mbNum, uint8_t *data,
    int32_t residual[][16]);
#endif


uint32_t h264bsdInterPrediction(mbStorage_t *pMb, macroblockLayer_t *pMbLayer,
    dpbStorage_t *dpb, uint32_t mbNum, image_t *image, uint8_t *data);


#ifndef H264DEC_OMXDL
uint32_t h264bsdIntraPrediction(mbStorage_t *pMb, macroblockLayer_t *mbLayer,
    image_t *image, uint32_t mbNum, uint32_t constrainedIntraPred, uint8_t *data);
uint32_t h264bsdIntra4x4Prediction(mbStorage_t *pMb, uint8_t *data,
                              macroblockLayer_t *mbLayer,
                              uint8_t *above, uint8_t *left, uint32_t constrainedIntraPred);
uint32_t h264bsdIntra16x16Prediction(mbStorage_t *pMb, uint8_t *data, int32_t residual[][16],
    uint8_t *above, uint8_t *left, uint32_t constrainedIntraPred);
uint32_t h264bsdIntraChromaPrediction(mbStorage_t *pMb, uint8_t *data, int32_t residual[][16],
    uint8_t *above, uint8_t *left, uint32_t predMode, uint32_t constrainedIntraPred);
void h264bsdGetNeighbourPels(image_t *image, uint8_t *above, uint8_t *left, uint32_t mbNum);
#else
uint32_t h264bsdIntra4x4Prediction(mbStorage_t *pMb, uint8_t *data,
                              macroblockLayer_t *mbLayer,
                              uint8_t *pImage, uint32_t width,
                              uint32_t constrainedIntraPred, uint32_t block);
uint32_t h264bsdIntra16x16Prediction(mbStorage_t *pMb, uint8_t *data, uint8_t *pImage,
                            uint32_t width, uint32_t constrainedIntraPred);
uint32_t h264bsdIntraChromaPrediction(mbStorage_t *pMb, uint8_t *data, image_t *image,
                                        uint32_t predMode, uint32_t constrainedIntraPred);
#endif


/* Macro to determine if a mb is an intra mb */
#define IS_INTRA_MB(a) ((a).mbType > 5)
/* Macro to determine if a mb is an I_PCM mb */
#define IS_I_PCM_MB(a) ((a).mbType == 31)

typedef enum {
  MB_P_16x16 = 0,
  MB_P_16x8,
  MB_P_8x16,
  MB_P_8x8
	} mbPartMode_e;

typedef enum {
  MB_SP_8x8 = 0,
  MB_SP_8x4,
  MB_SP_4x8,
  MB_SP_4x4
	} subMbPartMode_e;

typedef enum {
  PRED_MODE_INTRA4x4 = 0,
  PRED_MODE_INTRA16x16  ,
  PRED_MODE_INTER
	} mbPartPredMode_e;


uint32_t h264bsdDecodeMacroblockLayer(strmData_t *pStrmData,
    macroblockLayer_t *pMbLayer, mbStorage_t *pMb, uint32_t sliceType,
    uint32_t numRefIdxActive);
uint32_t h264bsdNumMbPart(mbType_e mbType);
uint32_t h264bsdNumSubMbPart(subMbType_e subMbType);
subMbPartMode_e h264bsdSubMbPartMode(subMbType_e subMbType);
uint32_t h264bsdDecodeMacroblock(mbStorage_t *pMb, macroblockLayer_t *pMbLayer,
    image_t *currImage, dpbStorage_t *dpb, int32_t *qpY, uint32_t mbNum,
    uint32_t constrainedIntraPredFlag, uint8_t* data);
uint32_t h264bsdPredModeIntra16x16(mbType_e mbType);
mbPartPredMode_e h264bsdMbPartPredMode(mbType_e mbType);
#ifdef H264DEC_NEON
uint32_t h264bsdClearMbLayer(macroblockLayer_t *pMbLayer, uint32_t size);
#endif


/* macro to determine if NAL unit pointed by pNalUnit contains an IDR slice */
#define IS_IDR_NAL_UNIT(pNalUnit) \
    ((pNalUnit)->nalUnitType == NAL_CODED_SLICE_IDR)



uint32_t h264bsdDecodeNalUnit(strmData_t *pStrmData, nalUnit_t *pNalUnit);



typedef enum {
  MB_A = 0,
  MB_B,
  MB_C,
  MB_D,
  MB_CURR,
  MB_NA = 0xFF
	} neighbourMb_e;

typedef struct {
  neighbourMb_e   mb;
  uint8_t             index;
	} neighbour_t;

void h264bsdInitMbNeighbours(mbStorage_t *pMbStorage, uint32_t picWidth,
    uint32_t picSizeInMbs);
mbStorage_t* h264bsdGetNeighbourMb(mbStorage_t *pMb, neighbourMb_e neighbour);
uint32_t h264bsdIsNeighbourAvailable(mbStorage_t *pMb, mbStorage_t *pNeighbour);
const neighbour_t* h264bsdNeighbour4x4BlockA(uint32_t blockIndex);
const neighbour_t* h264bsdNeighbour4x4BlockB(uint32_t blockIndex);
const neighbour_t* h264bsdNeighbour4x4BlockC(uint32_t blockIndex);
const neighbour_t* h264bsdNeighbour4x4BlockD(uint32_t blockIndex);


int32_t h264bsdDecodePicOrderCnt(pocStorage_t *poc, seqParamSet_t *sps,
    sliceHeader_t *sliceHeader, nalUnit_t *pNalUnit);
uint32_t h264bsdDecodePicParamSet(strmData_t *pStrmData,
    picParamSet_t *pPicParamSet, uint32_t profile);



#ifndef H264DEC_OMXDL
void h264bsdPredictSamples(uint8_t *data,
  mv_t *mv,
  image_t *refPic,
  uint32_t xA, uint32_t yA,
  uint32_t partX,  uint32_t partY,
  uint32_t partWidth,  uint32_t partHeight);
#else
void h264bsdPredictSamples(uint8_t *data,
  mv_t *mv,
  image_t *refPic,
  uint32_t colAndRow,/* packaged data | column    | row                |*/
  uint32_t part,     /* packaged data |partX|partY|partWidth|partHeight|*/
  uint8_t *pFill);
#endif

void h264bsdFillBlock(
  uint8_t * ref,
  uint8_t * fill,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t blockWidth,  uint32_t blockHeight,
  uint32_t fillScanLength);
void h264bsdInterpolateChromaHor(
  uint8_t *pRef,
  uint8_t *predPartChroma,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t xFrac,
  uint32_t chromaPartWidth,  uint32_t chromaPartHeight);
void h264bsdInterpolateChromaVer(
  uint8_t *pRef,
  uint8_t *predPartChroma,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t yFrac,
  uint32_t chromaPartWidth,  uint32_t chromaPartHeight);
void h264bsdInterpolateChromaHorVer(
  uint8_t *ref,
  uint8_t *predPartChroma,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t xFrac,  uint32_t yFrac,
  uint32_t chromaPartWidth,  uint32_t chromaPartHeight);
void h264bsdInterpolateVerHalf(uint8_t *ref,
  uint8_t *mb,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t partWidth,  uint32_t partHeight);
void h264bsdInterpolateVerQuarter(uint8_t *ref,
  uint8_t *mb,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t partWidth,  uint32_t partHeight,
  uint32_t verOffset);
void h264bsdInterpolateHorHalf(uint8_t *ref,
  uint8_t *mb,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t partWidth,  uint32_t partHeight);
void h264bsdInterpolateHorQuarter(uint8_t *ref,
  uint8_t *mb,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t partWidth,  uint32_t partHeight,
  uint32_t horOffset);
void h264bsdInterpolateHorVerQuarter(uint8_t *ref,
  uint8_t *mb,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t partWidth,  uint32_t partHeight,
  uint32_t horVerOffset);
void h264bsdInterpolateMidHalf(uint8_t *ref,
  uint8_t *mb,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t partWidth,  uint32_t partHeight);
void h264bsdInterpolateMidVerQuarter(uint8_t *ref,
  uint8_t *mb,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t partWidth,  uint32_t partHeight,
  uint32_t verOffset);
void h264bsdInterpolateMidHorQuarter(uint8_t *ref,
  uint8_t *mb,
  int32_t x0,  int32_t y0,
  uint32_t width,  uint32_t height,
  uint32_t partWidth,  uint32_t partHeight,
  uint32_t horOffset);
void h264bsdFillRow7(uint8_t *ref, uint8_t *fill,
  int32_t left, int32_t center, int32_t right);



#define MAX_PAN_SCAN_CNT 32
#define MAX_NUM_SPARE_PICS 16
#define MAX_NUM_CLOCK_TS 3
#define MAX_NUM_SUB_SEQ_LAYERS 256

typedef struct {
  uint32_t seqParameterSetId;
  uint32_t initialCpbRemovalDelay[MAX_CPB_CNT];
  uint32_t initialCpbRemovalDelayOffset[MAX_CPB_CNT];
	} seiBufferingPeriod_t;

typedef struct {
  uint32_t cpbRemovalDelay;
  uint32_t dpbOutputDelay;
  uint32_t picStruct;
  uint32_t clockTimeStampFlag[MAX_NUM_CLOCK_TS];
  uint32_t clockTimeStamp[MAX_NUM_CLOCK_TS];
  uint32_t ctType[MAX_NUM_CLOCK_TS];
  uint32_t nuitFieldBasedFlag[MAX_NUM_CLOCK_TS];
  uint32_t countingType[MAX_NUM_CLOCK_TS];
  uint32_t fullTimeStampFlag[MAX_NUM_CLOCK_TS];
  uint32_t discontinuityFlag[MAX_NUM_CLOCK_TS];
  uint32_t cntDroppedFlag[MAX_NUM_CLOCK_TS];
  uint32_t nFrames[MAX_NUM_CLOCK_TS];
  uint32_t secondsFlag[MAX_NUM_CLOCK_TS];
  uint32_t secondsValue[MAX_NUM_CLOCK_TS];
  uint32_t minutesFlag[MAX_NUM_CLOCK_TS];
  uint32_t minutesValue[MAX_NUM_CLOCK_TS];
  uint32_t hoursFlag[MAX_NUM_CLOCK_TS];
  uint32_t hoursValue[MAX_NUM_CLOCK_TS];
  int32_t timeOffset[MAX_NUM_CLOCK_TS];
	} seiPicTiming_t;

typedef struct {
  uint32_t panScanRectId;
  uint32_t panScanRectCancelFlag;
  uint32_t panScanCnt;
  int32_t panScanRectLeftOffset[MAX_PAN_SCAN_CNT];
  int32_t panScanRectRightOffset[MAX_PAN_SCAN_CNT];
  int32_t panScanRectTopOffset[MAX_PAN_SCAN_CNT];
  int32_t panScanRectBottomOffset[MAX_PAN_SCAN_CNT];
  uint32_t panScanRectRepetitionPeriod;
	} seiPanScanRect_t;

typedef struct {
  uint32_t ituTT35CountryCode;
  uint32_t ituTT35CountryCodeExtensionByte;
  uint8_t *ituTT35PayloadByte;
  uint32_t numPayloadBytes;
	} seiUserDataRegisteredItuTT35_t;

typedef struct {
  uint32_t uuidIsoIec11578[4];
  uint8_t *userDataPayloadByte;
  uint32_t numPayloadBytes;
	} seiUserDataUnregistered_t;

typedef struct {
  uint32_t recoveryFrameCnt;
  uint32_t exactMatchFlag;
  uint32_t brokenLinkFlag;
  uint32_t changingSliceGroupIdc;
	} seiRecoveryPoint_t;

typedef struct {
  uint32_t originalIdrFlag;
  uint32_t originalFrameNum;
  decRefPicMarking_t decRefPicMarking;
	} seiDecRefPicMarkingRepetition_t;

typedef struct {
  uint32_t targetFrameNum;
  uint32_t spareFieldFlag;
  uint32_t targetBottomFieldFlag;
  uint32_t numSparePics;
  uint32_t deltaSpareFrameNum[MAX_NUM_SPARE_PICS];
  uint32_t spareBottomFieldFlag[MAX_NUM_SPARE_PICS];
  uint32_t spareAreaIdc[MAX_NUM_SPARE_PICS];
  uint32_t *spareUnitFlag[MAX_NUM_SPARE_PICS];
  uint32_t *zeroRunLength[MAX_NUM_SPARE_PICS];
	} seiSparePic_t;

typedef struct {
  uint32_t sceneInfoPresentFlag;
  uint32_t sceneId;
  uint32_t sceneTransitionType;
  uint32_t secondSceneId;
	} seiSceneInfo_t;

typedef struct {
  uint32_t subSeqLayerNum;
  uint32_t subSeqId;
  uint32_t firstRefPicFlag;
  uint32_t leadingNonRefPicFlag;
  uint32_t lastPicFlag;
  uint32_t subSeqFrameNumFlag;
  uint32_t subSeqFrameNum;
	} seiSubSeqInfo_t;

typedef struct {
  uint32_t numSubSeqLayers;
  uint32_t accurateStatisticsFlag[MAX_NUM_SUB_SEQ_LAYERS];
  uint32_t averageBitRate[MAX_NUM_SUB_SEQ_LAYERS];
  uint32_t averageFrameRate[MAX_NUM_SUB_SEQ_LAYERS];
	} seiSubSeqLayerCharacteristics_t;

typedef struct {
  uint32_t subSeqLayerNum;
  uint32_t subSeqId;
  uint32_t durationFlag;
  uint32_t subSeqDuration;
  uint32_t averageRateFlag;
  uint32_t accurateStatisticsFlag;
  uint32_t averageBitRate;
  uint32_t averageFrameRate;
  uint32_t numReferencedSubseqs;
  uint32_t refSubSeqLayerNum[MAX_NUM_SUB_SEQ_LAYERS];
  uint32_t refSubSeqId[MAX_NUM_SUB_SEQ_LAYERS];
  uint32_t refSubSeqDirection[MAX_NUM_SUB_SEQ_LAYERS];
	} seiSubSeqCharacteristics_t;

typedef struct {
  uint32_t fullFrameFreezeRepetitionPeriod;
	} seiFullFrameFreeze_t;

typedef struct {
  uint32_t snapShotId;
	} seiFullFrameSnapshot_t;

typedef struct {
  uint32_t progressiveRefinementId;
  uint32_t numRefinementSteps;
	} seiProgressiveRefinementSegmentStart_t;

typedef struct {
  uint32_t progressiveRefinementId;
	} seiProgressiveRefinementSegmentEnd_t;

typedef struct {
  uint32_t numSliceGroupsInSet;
  uint32_t sliceGroupId[MAX_NUM_SLICE_GROUPS];
  uint32_t exactSampleValueMatchFlag;
  uint32_t panScanRectFlag;
  uint32_t panScanRectId;
	} seiMotionConstrainedSliceGroupSet_t;

typedef struct {
  uint8_t *reservedSeiMessagePayloadByte;
  uint32_t numPayloadBytes;
	} seiReservedSeiMessage_t;

typedef struct {
  uint32_t payloadType;
  seiBufferingPeriod_t bufferingPeriod;
  seiPicTiming_t picTiming;
  seiPanScanRect_t panScanRect;
  seiUserDataRegisteredItuTT35_t userDataRegisteredItuTT35;
  seiUserDataUnregistered_t userDataUnregistered;
  seiRecoveryPoint_t recoveryPoint;
  seiDecRefPicMarkingRepetition_t decRefPicMarkingRepetition;
  seiSparePic_t sparePic;
  seiSceneInfo_t sceneInfo;
  seiSubSeqInfo_t subSeqInfo;
  seiSubSeqLayerCharacteristics_t subSeqLayerCharacteristics;
  seiSubSeqCharacteristics_t subSeqCharacteristics;
  seiFullFrameFreeze_t fullFrameFreeze;
  seiFullFrameSnapshot_t fullFrameSnapshot;
  seiProgressiveRefinementSegmentStart_t progressiveRefinementSegmentStart;
  seiProgressiveRefinementSegmentEnd_t progressiveRefinementSegmentEnd;
  seiMotionConstrainedSliceGroupSet_t motionConstrainedSliceGroupSet;
  seiReservedSeiMessage_t reservedSeiMessage;
	} seiMessage_t;

uint32_t h264bsdDecodeSeiMessage(strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet,seiMessage_t *pSeiMessage,
  uint32_t numSliceGroups);
uint32_t h264bsdDecodeSeqParamSet(strmData_t *pStrmData,
    seqParamSet_t *pSeqParamSet);
uint32_t h264bsdCompareSeqParamSets(seqParamSet_t *pSps1, seqParamSet_t *pSps2);
uint32_t h264bsdDecodeSliceData(strmData_t *pStrmData, storage_t *pStorage,
    image_t *currImage, sliceHeader_t *pSliceHeader);
void h264bsdMarkSliceCorrupted(storage_t *pStorage, uint32_t firstMbInSlice);
void h264bsdDecodeSliceGroupMap(uint32_t *mapp, picParamSet_t *pps,
  uint32_t sliceGroupChangeCycle,
  uint32_t picWidth,uint32_t picHeight);


enum {
  P_SLICE = 0,
  I_SLICE = 2
	};

enum {NO_LONG_TERM_FRAME_INDICES = 0xFFFF};

/* macro to determine if slice is an inter slice, sliceTypes 0 and 5 */
#define IS_P_SLICE(sliceType) (((sliceType) == P_SLICE) || \
    ((sliceType) == P_SLICE + 5))

/* macro to determine if slice is an intra slice, sliceTypes 2 and 7 */
#define IS_I_SLICE(sliceType) (((sliceType) == I_SLICE) || \
    ((sliceType) == I_SLICE + 5))



uint32_t h264bsdDecodeSliceHeader(strmData_t *pStrmData, sliceHeader_t *pSliceHeader,
  seqParamSet_t *pSeqParamSet, picParamSet_t *pPicParamSet,
  nalUnit_t *pNalUnit);
uint32_t h264bsdCheckPpsId(strmData_t *pStrmData, uint32_t *ppsId);
uint32_t h264bsdCheckFrameNum(strmData_t *pStrmData,
  uint32_t maxFrameNum, uint32_t *frameNum);
uint32_t h264bsdCheckIdrPicId(strmData_t *pStrmData,
  uint32_t maxFrameNum, nalUnitType_e nalUnitType,
  uint32_t *idrPicId);
uint32_t h264bsdCheckPicOrderCntLsb(strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet, nalUnitType_e nalUnitType,
  uint32_t *picOrderCntLsb);
uint32_t h264bsdCheckDeltaPicOrderCntBottom(strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet, nalUnitType_e nalUnitType,
  int32_t *deltaPicOrderCntBottom);
uint32_t h264bsdCheckDeltaPicOrderCnt(strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet, nalUnitType_e nalUnitType,
  uint32_t picOrderPresentFlag, int32_t *deltaPicOrderCnt);
uint32_t h264bsdCheckRedundantPicCnt(strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet, picParamSet_t *pPicParamSet,
  nalUnitType_e nalUnitType, uint32_t *redundantPicCnt);
uint32_t h264bsdCheckPriorPicsFlag(uint32_t *noOutputOfPriorPicsFlag,
                              const strmData_t *pStrmData,
                              const seqParamSet_t *pSeqParamSet,
                              const picParamSet_t *pPicParamSet,
                              nalUnitType_e nalUnitType);



void h264bsdInitStorage(storage_t *pStorage);
void h264bsdResetStorage(storage_t *pStorage);
uint32_t h264bsdIsStartOfPicture(storage_t *pStorage);
uint32_t h264bsdIsEndOfPicture(storage_t *pStorage);
uint32_t h264bsdStoreSeqParamSet(storage_t *pStorage, seqParamSet_t *pSeqParamSet);
uint32_t h264bsdStorePicParamSet(storage_t *pStorage, picParamSet_t *pPicParamSet);
uint32_t h264bsdActivateParamSets(storage_t *pStorage, uint32_t ppsId, uint32_t isIdr);
void h264bsdComputeSliceGroupMap(storage_t *pStorage,
    uint32_t sliceGroupChangeCycle);
uint32_t h264bsdCheckAccessUnitBoundary(strmData_t *strm, nalUnit_t *nuNext,
  storage_t *storage,
  uint32_t *accessUnitBoundaryFlag);
uint32_t h264bsdValidParamSets(storage_t *pStorage);




uint32_t h264bsdGetBits(strmData_t *pStrmData, uint32_t numBits);
uint32_t h264bsdShowBits32(strmData_t *pStrmData);
uint32_t h264bsdFlushBits(strmData_t *pStrmData, uint32_t numBits);
uint32_t h264bsdIsByteAligned(strmData_t *);




uint32_t h264bsdProcessBlock(int32_t *data, uint32_t qp, uint32_t skip, uint32_t coeffMap);
void h264bsdProcessLumaDc(int32_t *data, uint32_t qp);
void h264bsdProcessChromaDc(int32_t *data, uint32_t qp);



#ifdef _ASSERT_USED
#include <assert.h>
#endif

#if defined(_RANGE_CHECK) || defined(_DEBUG_PRINT) || defined(_ERROR_PRINT)
#include <stdio.h>
#endif


#define HANTRO_OK   0
#define HANTRO_NOK  1

#define HANTRO_TRUE     (1)
#define HANTRO_FALSE    (0)


#define MEMORY_ALLOCATION_ERROR 0xFFFF
#define PARAM_SET_ERROR 0xFFF0

/* value to be returned by GetBits if stream buffer is empty */
#define END_OF_STREAM 0xFFFFFFFFU

#define EMPTY_RESIDUAL_INDICATOR 0xFFFFFF

/* macro to mark a residual block empty, i.e. contain zero coefficients */
#define MARK_RESIDUAL_EMPTY(residual) ((residual)[0] = EMPTY_RESIDUAL_INDICATOR)
/* macro to check if residual block is empty */
#define IS_RESIDUAL_EMPTY(residual) ((residual)[0] == EMPTY_RESIDUAL_INDICATOR)


/* macro for range checking an value, used only if compiler flag _RANGE_CHECK
 * is defined */
#ifdef _RANGE_CHECK
#define RANGE_CHECK(value, minBound, maxBound) \
{ \
    if ((value) < (minBound) || (value) > (maxBound)) \
        fprintf(stderr, "Warning: Value exceeds given limit(s)!\n"); \
}
#else
#define RANGE_CHECK(value, minBound, maxBound)
#endif

/* macro for range checking an array, used only if compiler flag _RANGE_CHECK
 * is defined */
#ifdef _RANGE_CHECK
#define RANGE_CHECK_ARRAY(array, minBound, maxBound, length) \
{ \
    int32_t i; \
    for (i = 0; i < (length); i++) \
        if ((array)[i] < (minBound) || (array)[i] > (maxBound)) \
            fprintf(stderr,"Warning: Value [%d] exceeds given limit(s)!\n",i); \
}
#else
#define RANGE_CHECK_ARRAY(array, minBound, maxBound, length)
#endif

/* macro for debug printing, used only if compiler flag _DEBUG_PRINT is defined */
#ifdef _DEBUG_PRINT
#define DEBUGP(arg1) theApp.FileSpool->print(CLogFile::flagInfo,arg1)
#define DEBUGP2(arg1,arg2) theApp.FileSpool->print(CLogFile::flagInfo,arg1,arg2)
#else
#define DEBUGP(args)
#endif

/* macro for error printing, used only if compiler flag _ERROR_PRINT is
 * defined */
#ifdef _ERROR_PRINT
#define EPRINT2(msg,arg) theApp.FileSpool->print(CLogFile::flagError,msg,arg)
#define EPRINT(msg) theApp.FileSpool->print(CLogFile::flagError,msg)
#else
#define EPRINT(msg)
#endif

/* macro to get smaller of two values */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
/* macro to get greater of two values */
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
/* macro to get absolute value */
#define ABS(a) (((a) < 0) ? -(a) : (a))

/* macro to clip a value z, so that x <= z =< y */
#define CLIP3(x,y,z) (((z) < (x)) ? (x) : (((z) > (y)) ? (y) : (z)))

/* macro to clip a value z, so that 0 <= z =< 255 */
#define CLIP1(z) (((z) < 0) ? 0 : (((z) > 255) ? 255 : (z)))

/* macro to allocate memory */
#define ALLOCATE(ptr, count, type) \
{ \
    (ptr) = (type*)malloc((count) * sizeof(type)); \
}

/* macro to free allocated memory */
#define FREE(ptr) \
{ \
    free((ptr)); (ptr) = NULL; \
}

#define ALIGN(ptr, bytePos) \
        (ptr + ( ((bytePos - (uintptr_t)ptr) & (bytePos - 1)) / sizeof(*ptr) ))

extern const uint32_t h264bsdQpC[52];

#ifndef H264DEC_NEON
uint32_t h264bsdCountLeadingZeros(uint32_t value, uint32_t length);
#else
uint32_t h264bsdCountLeadingZeros(uint32_t value);
#endif
uint32_t h264bsdRbspTrailingBits(strmData_t *strmData);
uint32_t h264bsdMoreRbspData(strmData_t *strmData);
uint32_t h264bsdNextMbAddress(uint32_t *pSliceGroupMap, uint32_t picSizeInMbs, uint32_t currMbAddr);
void h264bsdSetCurrImageMbPointers(image_t *image, uint32_t mbNum);

int32_t abs(int32_t a);
int32_t clip(int32_t x, int32_t y, int32_t z);



// per VLC:
uint32_t h264bsdDecodeExpGolombUnsigned(strmData_t *pStrmData, uint32_t *value);
uint32_t h264bsdDecodeExpGolombSigned(strmData_t *pStrmData, int32_t *value);
uint32_t h264bsdDecodeExpGolombMapped(strmData_t *pStrmData, uint32_t *value, uint32_t isIntra);
uint32_t h264bsdDecodeExpGolombTruncated(strmData_t *pStrmData, uint32_t *value, uint32_t greaterThanOne);


/* enumerated sample aspect ratios, ASPECT_RATIO_M_N means M:N */
enum {
  ASPECT_RATIO_UNSPECIFIED = 0,
  ASPECT_RATIO_1_1,
  ASPECT_RATIO_12_11,
  ASPECT_RATIO_10_11,
  ASPECT_RATIO_16_11,
  ASPECT_RATIO_40_33,
  ASPECT_RATIO_24_11,
  ASPECT_RATIO_20_11,
  ASPECT_RATIO_32_11,
  ASPECT_RATIO_80_33,
  ASPECT_RATIO_18_11,
  ASPECT_RATIO_15_11,
  ASPECT_RATIO_64_33,
  ASPECT_RATIO_160_99,
  ASPECT_RATIO_EXTENDED_SAR = 255
	};


uint32_t h264bsdDecodeVuiParameters(strmData_t *pStrmData, vuiParameters_t *pVuiParameters);


