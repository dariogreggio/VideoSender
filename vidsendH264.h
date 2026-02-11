/*
   H.264 JM coder/decoder
		https://github.com/petrkalos/JM/tree/master
		GD/C  adapted 15/1/26
		Classe 6/2/26
 */

#ifndef _VIDSEND_H264_INCLUDED
#define _VIDSEND_H264_INCLUDED

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/timeb.h>
#include <setjmp.h>

#include "vidsendRTSP.h"

//#define INT64T int64_t
typedef int64_t INT64T;		// provato a 32bit ma non va... ora pare andare, 10/2/26 VA SOLO DI NOTTE ;)
typedef int16_t PIXEL_COORD;		// se metto unsigned escono artefatti e poi si pianta... RIVERIFICARE
typedef int16_t BLOCK_COORD;
typedef int32_t PocType;		// serve 32...

#define JTRACE 0
#define TIMING_DISABLE

#define JM                  "19 (FRExt)"
#define VERSION             "19.2"		// froci ;)
#define EXT_VERSION         "(FRExt)"

#define DUMP_DPB                  0    //!< Dump DPB info for debug purposes
#define PRINTREFLIST              0    //!< Print ref list info for debug purposes
#define PAIR_FIELDS_IN_OUTPUT     0    //!< Pair field pictures for output purposes
#define IMGTYPE                   0    //!< Define imgpel size type. 0 implies uint8_t (cannot handle >8 bit depths) and 1 implies uint16_t
#define ENABLE_FIELD_CTX          1    //!< Enables Field mode related context types for CABAC
#define ENABLE_HIGH444_CTX        0    //!< Enables High 444 profile context types for CABAC. 
#define ZEROSNR                   0    //!< PSNR computation method
#define ENABLE_OUTPUT_TONEMAPPING 1    //!< enable tone map the output if tone mapping SEI present
#define JCOST_CALC_SCALEUP        1    //!< 1: J = (D<<LAMBDA_ACCURACY_BITS)+Lambda*R; 0: J = D + ((Lambda*R+Rounding)>>LAMBDA_ACCURACY_BITS)
#define DISABLE_ERC               0    //!< Disable any error concealment processes
#define JM_PARALLEL_DEBLOCK       0    //!< Enables Parallel Deblocking
#define SIMULCAST_ENABLE          0    //!< to test the decoder

#define MVC_EXTENSION_ENABLE      0    //!< enable support for the Multiview High Profile
#define ENABLE_DEC_STATS          0    //!< enable decoder statistics collection

#define MVC_INIT_VIEW_ID          -1
#define MAX_VIEW_NUM              1024   
#define BASE_VIEW_IDX             0

//#define _LEAKYBUCKET_

#define BLOCK_SHIFT            2
#define BLOCK_SIZE             4
#define BLOCK_SIZE_8x8         8
#define SMB_BLOCK_SIZE         8
#define BLOCK_PIXELS          16
#define MB_BLOCK_SIZE         16
#define MB_PIXELS            (MB_BLOCK_SIZE * MB_BLOCK_SIZE)
#define MB_PIXELS_SHIFT        8 // log2(MB_BLOCK_SIZE * MB_BLOCK_SIZE)
#define MB_BLOCK_SHIFT         4
#define BLOCK_MULTIPLE         4 // (MB_BLOCK_SIZE/BLOCK_SIZE)
#define MB_BLOCK_PARTITIONS   (BLOCK_MULTIPLE * BLOCK_MULTIPLE)
#define BLOCK_CONTEXT         (4 * MB_BLOCK_PARTITIONS)

// These variables relate to the subpel accuracy supported by the software (1/4)
#define BLOCK_SIZE_SP      (BLOCK_SIZE << 2)
#define BLOCK_SIZE_8x8_SP  (BLOCK_SIZE_8x8 << 2)

//#include "typedefs.h"

#define SSE_MEMORY_ALIGNMENT      16
#if IMGTYPE == 0
typedef uint8_t   imgpel;           //!< pixel type
typedef uint16_t distpel;          //!< distortion type (for pixels)
typedef int16_t  distblk;          //!< distortion type (for CMacroblock)
typedef int32_t  transpel;         //!< transformed coefficient type
#else
typedef uint16_t imgpel;
typedef uint32_t distpel;
typedef INT64  distblk;
typedef int32_t  transpel;
#endif



// #include "nalucommon.h"
// In the MPEG-4 AVC/H.264 syntax, frequently flags are used that indicate the presence of
// certain pieces of information in the NALU.  Here, these flags are also
// present.  In the encoder, those bits indicate that the values signaled to
// be present are meaningful and that this part of the syntax should be
// written to the NALU.  In the decoder, the flag indicates that information
// was received from the decoded NALU and should be used henceforth.
// The structure names were chosen as indicated in the MPEG-4 AVC/H.264 syntax


//#include "defines.h"

#define MAXIMUMPARSETRBSPSIZE   1500
#define MAXIMUMPARSETNALUSIZE   1500

#define MAXSPS  32
#define MAXPPS  256

//#define MAX_NUM_SLICES 150
#define MAX_NUM_SLICES     100		// era 50
#define MAX_REFERENCE_PICTURES 32               //!< H.264 allows 32 fields
#define MAX_CODED_FRAME_SIZE 256000    /*8000000*/         //!< bytes for one frame (occhio hi-res, 952x576 ecc specie quando la compressione è bassa! di giorno
#define MAX_NUM_DECSLICES  16
#define MAX_DEC_THREADS    16                  //16 core deocoding;
#define MCBUF_LUMA_PAD_X       32
#define MCBUF_LUMA_PAD_Y       12
#define MCBUF_CHROMA_PAD_X     16
#define MCBUF_CHROMA_PAD_Y     8
#define MAX_NUM_DPB_LAYERS     2

/* macro to clip a value z, so that 0 <= z =< 255 */
#define CLIP1(z) (((z) < 0) ? 0 : (((z) > 255) ? 255 : (z)))

typedef enum {
  CM_UNKNOWN = -1,
  CM_YUV     =  0,
  CM_RGB     =  1,
  CM_XYZ     =  2
	} ColorModel;

typedef enum {
  CF_UNKNOWN = -1,     //!< Unknown color format
  YUV400     =  0,     //!< Monochrome
  YUV420     =  1,     //!< 4:2:0
  YUV422     =  2,     //!< 4:2:2
  YUV444     =  3      //!< 4:4:4
	} ColorFormat;

typedef enum {
  PF_UNKNOWN = -1,     //!< Unknown color ordering
  UYVY       =  0,     //!< UYVY
  YUY2       =  1,     //!< YUY2
  YUYV       =  1,     //!< YUYV
  YVYU       =  2,     //!< YVYU
  BGR        =  3,     //!< BGR
	RGB					= 4,			// RGB
  V210       =  5      //!< Video Clarity 422 format (10 bits)
	} PixelFormat;

#define MAXIMUMVALUEOFcpb_cnt   32
typedef struct {
  unsigned int cpb_cnt_minus1;                                   // ue(v)
  uint8_t bit_rate_scale;                                   // u(4)
  uint8_t cpb_size_scale;                                   // u(4)
  unsigned int bit_rate_value_minus1 [MAXIMUMVALUEOFcpb_cnt];    // ue(v)
  unsigned int cpb_size_value_minus1 [MAXIMUMVALUEOFcpb_cnt];    // ue(v)
  bool  cbr_flag              [MAXIMUMVALUEOFcpb_cnt];    // u(1)
  uint8_t initial_cpb_removal_delay_length_minus1;          // u(5)
  uint8_t cpb_removal_delay_length_minus1;                  // u(5)
  uint8_t dpb_output_delay_length_minus1;                   // u(5)
  uint8_t time_offset_length;                               // u(5)
	} hrd_parameters_t;

typedef struct {
  bool      aspect_ratio_info_present_flag;                   // u(1)
  uint8_t aspect_ratio_idc;                                 // u(8)
  PIXEL_COORD sar_width;                                      // u(16)
  PIXEL_COORD sar_height;                                     // u(16)
  bool      overscan_info_present_flag;                       // u(1)
  bool      overscan_appropriate_flag;                        // u(1)
  bool      video_signal_type_present_flag;                   // u(1)
  uint8_t video_format;                                     // u(3)
  bool      video_full_range_flag;                            // u(1)
  bool      colour_description_present_flag;                  // u(1)
  uint8_t colour_primaries;                                 // u(8)
  uint8_t transfer_characteristics;                         // u(8)
  uint8_t matrix_coefficients;                              // u(8)
  bool      chroma_location_info_present_flag;                // u(1)
  unsigned int  chroma_sample_loc_type_top_field;                // ue(v)
  unsigned int  chroma_sample_loc_type_bottom_field;             // ue(v)
  bool      timing_info_present_flag;                         // u(1)
  unsigned int num_units_in_tick;                                // u(32)
  unsigned int time_scale;                                       // u(32)
  bool      fixed_frame_rate_flag;                            // u(1)
  bool      nal_hrd_parameters_present_flag;                  // u(1)
  hrd_parameters_t nal_hrd_parameters;                           // hrd_paramters_t
  bool      vcl_hrd_parameters_present_flag;                  // u(1)
  hrd_parameters_t vcl_hrd_parameters;                           // hrd_paramters_t
  // if ((nal_hrd_parameters_present_flag || (vcl_hrd_parameters_present_flag))
  bool      low_delay_hrd_flag;                               // u(1)
  bool      pic_struct_present_flag;                          // u(1)
  bool      bitstream_restriction_flag;                       // u(1)
  bool      motion_vectors_over_pic_boundaries_flag;          // u(1)
  unsigned int max_bytes_per_pic_denom;                          // ue(v)
  unsigned int max_bits_per_mb_denom;                            // ue(v)
  unsigned int log2_max_mv_length_vertical;                      // ue(v)
  unsigned int log2_max_mv_length_horizontal;                    // ue(v)
  unsigned int num_reorder_frames;                               // ue(v)
  unsigned int max_dec_frame_buffering;                          // ue(v)
	} vui_seq_parameters_t;


//AVC Profile IDC definitions
typedef enum {
  NO_PROFILE     =  0,       //!< disable profile checking for experimental coding (enables FRExt, but disables MV)
  FREXT_CAVLC444 = 44,       //!< YUV 4:4:4/14 "CAVLC 4:4:4"
  BASELINE       = 66,       //!< YUV 4:2:0/8  "Baseline"
  MAIN           = 77,       //!< YUV 4:2:0/8  "Main"
  EXTENDED       = 88,       //!< YUV 4:2:0/8  "Extended"
  FREXT_HP       = 100,      //!< YUV 4:2:0/8  "High"
  FREXT_Hi10P    = 110,      //!< YUV 4:2:0/10 "High 10"
  FREXT_Hi422    = 122,      //!< YUV 4:2:2/10 "High 4:2:2"
  FREXT_Hi444    = 244,      //!< YUV 4:4:4/14 "High 4:4:4"
  MVC_HIGH       = 118,      //!< YUV 4:2:0/8  "Multiview High"
  STEREO_HIGH    = 128       //!< YUV 4:2:0/8  "Stereo High"
	} ProfileIDC;

#define MAXnum_slice_groups_minus1  8
typedef struct {
  bool   Valid;                  // indicates the parameter set is valid
  uint16_t pic_parameter_set_id;                             // ue(v)
  uint16_t seq_parameter_set_id;                             // ue(v)
  bool   entropy_coding_mode_flag;                            // u(1)
  bool   transform_8x8_mode_flag;                             // u(1)

  bool  pic_scaling_matrix_present_flag;                     // u(1)
  bool  pic_scaling_list_present_flag[12];                   // u(1)
  int16_t ScalingList4x4[6][16];                               // se(v)
  int16_t ScalingList8x8[6][64];                               // se(v)
  bool   UseDefaultScalingMatrix4x4Flag[6];
  bool   UseDefaultScalingMatrix8x8Flag[6];

  // if( pic_order_cnt_type < 2 )  in the sequence parameter set
  bool      bottom_field_pic_order_in_frame_present_flag;                           // u(1)
  unsigned int num_slice_groups_minus1;                          // ue(v)
  unsigned int slice_group_map_type;                        // ue(v)
  // if( slice_group_map_type = = 0 )
  unsigned int run_length_minus1[MAXnum_slice_groups_minus1]; // ue(v)
  // else if( slice_group_map_type = = 2 )
  unsigned int top_left[MAXnum_slice_groups_minus1];         // ue(v)
  unsigned int bottom_right[MAXnum_slice_groups_minus1];     // ue(v)
  // else if( slice_group_map_type = = 3 || 4 || 5
  bool   slice_group_change_direction_flag;            // u(1)
  unsigned int slice_group_change_rate_minus1;               // ue(v)
  // else if( slice_group_map_type = = 6 )
  unsigned int pic_size_in_map_units_minus1;             // ue(v)
  uint8_t      *slice_group_id;                              // complete MBAmap u(v)

  int num_ref_idx_l0_default_active_minus1;                     // ue(v)
  int num_ref_idx_l1_default_active_minus1;                     // ue(v)
  bool    weighted_pred_flag;                               // u(1)
  uint8_t weighted_bipred_idc;                              // u(2)
  int8_t    pic_init_qp_minus26;                              // se(v)
  int8_t    pic_init_qs_minus26;                              // se(v)
  int       chroma_qp_index_offset;                           // se(v)

  int       cb_qp_index_offset;                               // se(v)
  int       cr_qp_index_offset;                               // se(v)
  int       second_chroma_qp_index_offset;                    // se(v)

  bool   deblocking_filter_control_present_flag;           // u(1)
  bool   constrained_intra_pred_flag;                      // u(1)
  bool   redundant_pic_cnt_present_flag;                   // u(1)
  bool   vui_pic_parameters_flag;                          // u(1)
	} pic_parameter_set_rbsp_t;


#define MAXnum_ref_frames_in_pic_order_cnt_cycle  256
typedef struct {
  bool   Valid;                  // indicates the parameter set is valid
  ProfileIDC profile_idc;                                       // u(8)
  bool   constrained_set0_flag;                                // u(1)
  bool   constrained_set1_flag;                                // u(1)
  bool   constrained_set2_flag;                                // u(1)
  bool   constrained_set3_flag;                                // u(1)
#if MVC_EXTENSION_ENABLE
  bool   constrained_set4_flag;                                // u(1)
  bool   constrained_set5_flag;                                // u(2)
#endif
  uint8_t level_idc;                                        // u(8)
  unsigned int seq_parameter_set_id;                             // ue(v)
  ColorFormat chroma_format_idc;                                // ue(v)

  bool seq_scaling_matrix_present_flag;                   // u(1)
  bool seq_scaling_list_present_flag[12];                 // u(1)
  int16_t ScalingList4x4[6][16];                             // se(v)
  int16_t ScalingList8x8[6][64];                             // se(v)
  bool   UseDefaultScalingMatrix4x4Flag[6];
  bool   UseDefaultScalingMatrix8x8Flag[6];

  uint8_t bit_depth_luma_minus8;                            // ue(v)
  uint8_t bit_depth_chroma_minus8;                          // ue(v)
  unsigned int log2_max_frame_num_minus4;                        // ue(v)
  uint8_t pic_order_cnt_type;
  // if( pic_order_cnt_type == 0 )
  unsigned int log2_max_pic_order_cnt_lsb_minus4;                 // ue(v)
  // else if( pic_order_cnt_type == 1 )
  bool delta_pic_order_always_zero_flag;               // u(1)
  int     offset_for_non_ref_pic;                         // se(v)
  int     offset_for_top_to_bottom_field;                 // se(v)
  unsigned int num_ref_frames_in_pic_order_cnt_cycle;          // ue(v)
  // for( i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
  int   offset_for_ref_frame[MAXnum_ref_frames_in_pic_order_cnt_cycle];   // se(v)
  unsigned int num_ref_frames;                                   // ue(v)
  bool   gaps_in_frame_num_value_allowed_flag;             // u(1)
  PIXEL_COORD pic_width_in_mbs_minus1;                          // ue(v)
  PIXEL_COORD pic_height_in_map_units_minus1;                   // ue(v)
  bool   frame_mbs_only_flag;                              // u(1)
  // if( !frame_mbs_only_flag )
  bool   mb_adaptive_frame_field_flag;                   // u(1)
  bool   direct_8x8_inference_flag;                        // u(1)
  bool   frame_cropping_flag;                              // u(1)
  PIXEL_COORD frame_crop_left_offset;                // ue(v)
  PIXEL_COORD frame_crop_right_offset;               // ue(v)
  PIXEL_COORD frame_crop_top_offset;                 // ue(v)
  PIXEL_COORD frame_crop_bottom_offset;              // ue(v)
  bool   vui_parameters_present_flag;                      // u(1)
  vui_seq_parameters_t vui_seq_parameters;                  // vui_seq_parameters_t
  bool separate_colour_plane_flag;                       // u(1)
#if MVC_EXTENSION_ENABLE
  int max_dec_frame_buffering;
#endif
  bool lossless_qpprime_flag;
	} seq_parameter_set_rbsp_t;

#if MVC_EXTENSION_ENABLE
typedef struct mvcvui_tag {
  int num_ops_minus1;
  uint8_t *temporal_id;
  int *num_target_output_views_minus1;
  int8_t **view_id;
  int8_t *timing_info_present_flag;
  int *num_units_in_tick;
  int *time_scale;
  int8_t *fixed_frame_rate_flag;
  int8_t *nal_hrd_parameters_present_flag;
  int8_t *vcl_hrd_parameters_present_flag;
  int8_t *low_delay_hrd_flag;
  int8_t *pic_struct_present_flag;

  //hrd parameters;
  int8_t cpb_cnt_minus1;
  int8_t bit_rate_scale;
  int8_t cpb_size_scale;
  int bit_rate_value_minus1[32];
  int cpb_size_value_minus1[32];
  int8_t cbr_flag[32];
  int8_t initial_cpb_removal_delay_length_minus1;
  int8_t cpb_removal_delay_length_minus1;
  int8_t dpb_output_delay_length_minus1;
  int8_t time_offset_length;
	} MVCVUI_t;

typedef struct {
  seq_parameter_set_rbsp_t sps;

  uint8_t bit_equal_to_one;
  int num_views_minus1;
  int8_t *view_id;
  int *num_anchor_refs_l0;
  int **anchor_ref_l0;
  int *num_anchor_refs_l1;
  int **anchor_ref_l1;

  int *num_non_anchor_refs_l0;
  int **non_anchor_ref_l0;
  int *num_non_anchor_refs_l1;
  int **non_anchor_ref_l1;
   
  int num_level_values_signalled_minus1;
  uint8_t *level_idc;
  int *num_applicable_ops_minus1;
  uint8_t **applicable_op_temporal_id;
  int **applicable_op_num_target_views_minus1;
  int8_t ***applicable_op_target_view_id;
  int **applicable_op_num_views_minus1;

  bool  mvc_vui_parameters_present_flag;
  bool  Valid;                  // indicates the parameter set is valid
  MVCVUI_t  MVCVUIParams;
	} subset_seq_parameter_set_rbsp_t;

#endif


typedef struct annex_b_struct {
  int32_t BitStreamFile;                //!< the bit stream file, anche CRSTPClient!
  uint8_t *iobuffer;
  uint8_t *iobufferread;
  int bytesinbuffer;
  bool is_eof;
  int iIOBufferSize;

  int IsFirstByteStreamNALU;
  int nextstartcodebytes;
  uint8_t *Buf;  
	} ANNEXB_t;

#define MAX_PLANE       3

typedef struct {
  BLOCK_COORD x;
  BLOCK_COORD y;
	} BlockPos;

//! cbp structure
typedef struct cbp_s {
  INT64T         blk     ;
  INT64T         bits    ;
  INT64T         bits_8x8;
	} CBPStructure;


/***********************************************************************
 * T y p e    d e f i n i t i o n s    f o r    T M L
 ***********************************************************************
 */

typedef enum {
  // YUV
  PLANE_Y = 0,  // PLANE_Y
  PLANE_U = 1,  // PLANE_Cb
  PLANE_V = 2,  // PLANE_Cr
  // RGB
  PLANE_G = 0,
  PLANE_B = 1,
  PLANE_R = 2
	} ColorPlane;

enum {
  LIST_0 = 0,
  LIST_1 = 1,
  BI_PRED = 2,
  BI_PRED_L0 = 3,
  BI_PRED_L1 = 4
	};

enum {
  ERROR_SAD = 0,
  ERROR_SSE = 1,
  ERROR_SATD = 2,
  ERROR_PSATD = 3
	};

enum {
  ME_Y_ONLY = 0,
  ME_YUV_FP = 1,
  ME_YUV_FP_SP = 2
	};


enum {
  DISTORTION_MSE = 0
	};


//! Data Partitioning Modes
typedef enum {
  PAR_DP_1,   //!< no data partitioning is supported
  PAR_DP_3    //!< data partitioning with 3 partitions
	} PAR_DP_TYPE;

//! Output File Types
typedef enum {
  PAR_OF_ANNEXB,    //!< Annex B uint8_t stream format
  PAR_OF_RTP,       //!< RTP packets in outfile  SARA' INFILE?? ;)
  PAR_OF_GDRTP       //!< i miei RTP packets!
	} PAR_OF_TYPE;

//! Field Coding Types
typedef enum {
  FRAME_CODING         = 0,
  FIELD_CODING         = 1,
  ADAPTIVE_CODING      = 2,
  FRAME_MB_PAIR_CODING = 3
	} CodingType;


//! definition of H.264 syntax elements
typedef enum {
  SE_HEADER,
  SE_PTYPE,
  SE_MBTYPE,
  SE_REFFRAME,
  SE_INTRAPREDMODE,
  SE_MVD,
  SE_CBP,
  SE_LUM_DC_INTRA,
  SE_CHR_DC_INTRA,
  SE_LUM_AC_INTRA,
  SE_CHR_AC_INTRA,
  SE_LUM_DC_INTER,
  SE_CHR_DC_INTER,
  SE_LUM_AC_INTER,
  SE_CHR_AC_INTER,
  SE_DELTA_QUANT,
  SE_BFRAME,
  SE_EOS,
  SE_MAX_ELEMENTS = 20 //!< number of maximum syntax elements
	} SE_type;             // substituting the definitions in elements.h


typedef enum {
  NO_SLICES,
  FIXED_MB,
  FIXED_RATE,
  CALL_BACK
	} SliceMode;


typedef enum {
  CAVLC,
  CABAC
	} SymbolMode;

typedef enum {
  FULL_SEARCH      = -1,
  FAST_FULL_SEARCH =  0,
  UM_HEX           =  1,
  UM_HEX_SIMPLE    =  2,
  EPZS             =  3
	} SearchType;


typedef enum {
  FRAME,
  TOP_FIELD,
  BOTTOM_FIELD
	} PictureStructure;           //!< New enum for field processing

typedef enum {
  P_SLICE = 0,
  B_SLICE = 1,
  I_SLICE = 2,
  SP_SLICE = 3,
  SI_SLICE = 4,
  NUM_SLICE_TYPES = 5
	} SliceType;

//Motion Estimation levels
typedef enum {
  F_PEL,   //!< Full Pel refinement
  H_PEL,   //!< Half Pel refinement
  Q_PEL    //!< Quarter Pel refinement
	} MELevel;

typedef enum {
  FAST_ACCESS = 0,    //!< Fast/safe reference access
  UMV_ACCESS = 1      //!< unconstrained reference access
	} REF_ACCESS_TYPE;

typedef enum {
  IS_LUMA = 0,
  IS_CHROMA = 1
	} Component_Type;

typedef enum {
  RC_MODE_0 = 0,
  RC_MODE_1 = 1,
  RC_MODE_2 = 2,
  RC_MODE_3 = 3
	} RCModeType;


typedef enum {
  _SSE              = 0,
  SSE_RGB          = 1,  
  PSNR             = 2,
  PSNR_RGB         = 3,
  SSIM             = 4,
  SSIM_RGB         = 5,
  MS_SSIM          = 6,
  MS_SSIM_RGB      = 7,
  TOTAL_DIST_TYPES = 8
	} distortion_types;

typedef enum {
  WP_MCPREC_PLUS0 =       4,
  WP_MCPREC_PLUS1 =       5,
  WP_MCPREC_MINUS0 =      6,
  WP_MCPREC_MINUS1 =      7,
  WP_MCPREC_MINUS_PLUS0 = 8,
  WP_REGULAR =            9
	} weighted_prediction_types;

/***********************************************************************
 * D a t a    t y p e s   f o r  C A B A C
 ***********************************************************************
 */

typedef struct pix_pos {
  bool  available;
  int   mb_addr;
  BLOCK_COORD x;
  BLOCK_COORD y;
  BLOCK_COORD pos_x;
  BLOCK_COORD pos_y;
	} PixelPos;

//! struct to characterize the state of the arithmetic coding engine
typedef struct {
  unsigned int    Drange;
  unsigned int    Dvalue;
  int             DbitsLeft;
  uint8_t         *Dcodestrm;
  int             *Dcodestrm_len;
	} DecodingEnvironment;

typedef DecodingEnvironment *DecodingEnvironmentPtr;

// Motion Vector structure
typedef struct {
  BLOCK_COORD mv_x;
  BLOCK_COORD mv_y;
	} MotionVector;

static const MotionVector zero_mv = {0, 0};

//! struct for context management
typedef struct {
  uint16_t state;         // index into state-table CP
  uint8_t  MPS;           // Least Probable Symbol 0/1 CP
  uint8_t dummy;          // for alignment
	} BiContextType;

typedef BiContextType *BiContextTypePtr;

//  Available MB modes
typedef enum {
  PSKIP        =  0,
  BSKIP_DIRECT =  0,
  P16x16       =  1,
  P16x8        =  2,
  P8x16        =  3,
  SMB8x8       =  4,
  SMB8x4       =  5,
  SMB4x8       =  6,
  SMB4x4       =  7,
  P8x8         =  8,
  I4MB         =  9,
  I16MB        = 10,
  IBLOCK       = 11,
  SI4MB        = 12,
  I8MB         = 13,
  IPCM         = 14,
  MAXMODE      = 15
	} MBModeTypes;

// CAVLC block types
typedef enum {
  LUMA              =  0,
  LUMA_INTRA16x16DC =  1,
  LUMA_INTRA16x16AC =  2,
  CB                =  3,
  CB_INTRA16x16DC   =  4,
  CB_INTRA16x16AC   =  5,
  CR_DC     =   6,			//GD
  CR_AC     =   7,			//GD
  CR                =  8,
  CR_INTRA16x16DC   =  9,
  CR_INTRA16x16AC   = 10
	} CAVLCBlockTypes;

// CABAC block types
typedef enum {
  LUMA_16DC     =   0,
  LUMA_16AC     =   1,
  LUMA_8x8      =   2,
  LUMA_8x4      =   3,
  LUMA_4x8      =   4,
  LUMA_4x4      =   5,
  CHROMA_DC     =   6,
  CHROMA_AC     =   7,
  CHROMA_DC_2x4 =   8,
  CHROMA_DC_4x4 =   9,
  CB_16DC       =  10,
  CB_16AC       =  11,
  CB_8x8        =  12,
  CB_8x4        =  13,
  CB_4x8        =  14,
  CB_4x4        =  15,
  CR_16DC       =  16,
  CR_16AC       =  17,
  CR_8x8        =  18,
  CR_8x4        =  19,
  CR_4x8        =  20,
  CR_4x4        =  21
	} CABACBlockTypes;

// 4x4 intra prediction modes 
typedef enum {
  VERT_PRED            = 0,
  HOR_PRED             = 1,
  DC_PRED              = 2,
  DIAG_DOWN_LEFT_PRED  = 3,
  DIAG_DOWN_RIGHT_PRED = 4,
  VERT_RIGHT_PRED      = 5,
  HOR_DOWN_PRED        = 6,
  VERT_LEFT_PRED       = 7,
  HOR_UP_PRED          = 8
	} I4x4PredModes;

// 16x16 intra prediction modes
typedef enum {
  VERT_PRED_16   = 0,
  HOR_PRED_16    = 1,
  DC_PRED_16     = 2,
  PLANE_16       = 3
	} I16x16PredModes;

// 8x8 chroma intra prediction modes
typedef enum {
  DC_PRED_8     =  0,
  HOR_PRED_8    =  1,
  VERT_PRED_8   =  2,
  PLANE_8       =  3
	} I8x8PredModes;

// MV Prediction types
typedef enum {
  MVPRED_MEDIAN   = 0,
  MVPRED_L        = 1,
  MVPRED_U        = 2,
  MVPRED_UR       = 3
	} MVPredTypes;


class CDecoderH264;
class CVideoParameters;
class CSlice;
class CSyntaxElement;


//! CMacroblock -------------------------------------------------------------------------------------------------
class CMacroblock {		// classe, in futuro metterci i metodi relativi (provato 9/2/26 ma è un casino!
public:
  CSlice    *p_Slice;                    //!< pointer to the current slice
  CVideoParameters *p_Vid;                      //!< pointer to CVideoParameters
  struct inp_par  *p_Inp;
  int              mbAddrX;                    //!< current MB address
  int mbAddrA, mbAddrB, mbAddrC, mbAddrD;
  bool mbAvailA, mbAvailB, mbAvailC, mbAvailD;
  BlockPos mb;
  BLOCK_COORD block_x;
  BLOCK_COORD block_y;
  BLOCK_COORD block_y_aff;
  BLOCK_COORD pix_x;
  BLOCK_COORD pix_y;
  BLOCK_COORD pix_c_x;
  BLOCK_COORD pix_c_y;

  BLOCK_COORD subblock_x;
  BLOCK_COORD subblock_y;

  int8_t     qp;                    //!< QP luma
  int8_t     qpc[2];                //!< QP chroma
  int8_t     qp_scaled[MAX_PLANE];  //!< QP scaled for all comps.
  bool       is_lossless;
  bool       is_intra_block;
  bool       is_v_block;
  int8_t     DeblockCall;

  int16_t       slice_nr;
  bool          ei_flag;             //!< error indicator flag that enables concealment
  bool          dpl_flag;            //!< error indicator flag that signals a missing data partition
  int16_t       delta_quant;          //!< for rate control
  int16_t       list_offset;

  CMacroblock *mb_up;   //!< pointer to neighboring MB (CABAC)
  CMacroblock *mb_left; //!< pointer to neighboring MB (CABAC)

  CMacroblock *mbup;   // neighbors for loopfilter
  CMacroblock *mbleft; // neighbors for loopfilter

  // some storage of macroblock syntax elements for global access
  MBModeTypes   mb_type;
  int16_t       mvd[2][BLOCK_MULTIPLE][BLOCK_MULTIPLE][2];      //!< indices correspond to [forw,backw][block_y][block_x][x,y]
  //int16_t         ****mvd;      //!< indices correspond to [forw,backw][block_y][block_x][x,y]
  int8_t           cbp;
  CBPStructure  s_cbp[3];

  I16x16PredModes i16mode;
  int8_t        b8mode[4];
  int8_t        b8pdir[4];
  int8_t        ipmode_DPCM;
  I8x8PredModes c_ipred_mode;       //!< chroma intra prediction mode
  bool          skip_flag;
  int8_t        DFDisableIdc;
  int16_t       DFAlphaC0Offset;
  int16_t       DFBetaOffset;

  bool       mb_field;
  //Flag for MBAFF deblocking;
  bool       mixedModeEdgeFlag;

  // deblocking strength indices
  uint8_t strength_ver[4][4];
  uint8_t strength_hor[4][16];

  bool luma_transform_size_8x8_flag;
  bool NoMbPartLessThan8x8Flag;

  void (CDecoderH264::*itrans_4x4)(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
  void (CDecoderH264::*itrans_8x8)(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);

  void (CDecoderH264::*GetMVPredictor) (CMacroblock *currMB, PixelPos *block, 
    MotionVector *pmv, int16_t ref_frame, struct pic_motion_params **mv_info, int list, BLOCK_COORD mb_x, BLOCK_COORD mb_y, 
		BLOCK_COORD blockshape_x, BLOCK_COORD blockshape_y);

  int (CDecoderH264::*read_and_store_CBP_block_bit)(CMacroblock *currMB, 
		DecodingEnvironmentPtr dep_dp, CABACBlockTypes type);
  int8_t (CDecoderH264::*readRefPictureIdx)(CMacroblock *currMB, CSyntaxElement *currSE, 
		struct datapartition_dec *dP, int8_t b8mode, int list);

  void (CDecoderH264::*read_comp_coeff_4x4_CABAC)(CMacroblock *currMB, CSyntaxElement *currSE, ColorPlane pl, 
		int16_t(*InvLevelScale4x4)[4], int8_t qp_per, int8_t cbp);
  void (CDecoderH264::*read_comp_coeff_8x8_CABAC)(CMacroblock *currMB, CSyntaxElement *currSE, ColorPlane pl);

  void (CDecoderH264::*read_comp_coeff_4x4_CAVLC)(CMacroblock *currMB, ColorPlane pl, 
		int16_t(*InvLevelScale4x4)[4], int8_t qp_per, int8_t cbp, uint8_t **nzcoeff);
  void (CDecoderH264::*read_comp_coeff_8x8_CAVLC)(CMacroblock *currMB, ColorPlane pl, 
		int16_t(*InvLevelScale8x8)[8], int8_t qp_per, int8_t cbp, uint8_t **nzcoeff);

public:
	inline void reset_mbs();

	void iMBtrans4x4(ColorPlane pl, bool smb);
	void iMBtrans8x8(ColorPlane pl);

	void itrans_sp_cr(int uv);

	void Inv_Residual_trans_4x4(ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	void Inv_Residual_trans_8x8(ColorPlane pl, PIXEL_COORD ioff,PIXEL_COORD joff);
	void Inv_Residual_trans_16x16(ColorPlane pl);
	void Inv_Residual_trans_Chroma(int uv);

	void itrans4x4   (ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	void itrans4x4_ls(ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	void itrans_sp   (ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	void itrans_2    (ColorPlane pl);
	void iTransform  (ColorPlane pl, bool smb);
	};

typedef struct coding_par {
  uint8_t layer_id;
  ProfileIDC profile_idc;
  PIXEL_COORD width;
  PIXEL_COORD height;
  PIXEL_COORD width_cr;                               //!< width chroma  
  PIXEL_COORD height_cr;                              //!< height chroma

  uint8_t pic_unit_bitsize_on_disk;
  int8_t bitdepth_luma;
  int8_t bitdepth_chroma;
  int8_t bitdepth_scale[2];
  int8_t bitdepth_luma_qp_scale;
  int8_t bitdepth_chroma_qp_scale;
  unsigned int dc_pred_value_comp[MAX_PLANE]; //!< component value for DC prediction (depends on component pel bit depth)
#if IMGTYPE == 0
  uint8_t max_pel_value_comp[MAX_PLANE];       //!< max value that one picture element (pixel) can take (depends on pic_unit_bitdepth)
#else
  uint16_t max_pel_value_comp[MAX_PLANE];       //!< max value that one picture element (pixel) can take (depends on pic_unit_bitdepth)
#endif

  ColorFormat yuv_format;
  bool lossless_qpprime_flag;
  int num_blk8x8_uv;
  int num_uv_blocks;
  int num_cdc_coeff;
  BLOCK_COORD mb_cr_size_x;
  BLOCK_COORD mb_cr_size_y;
  BLOCK_COORD mb_cr_size_x_blk;
  BLOCK_COORD mb_cr_size_y_blk;
  int mb_cr_size;
  uint32_t mb_size[3][2];                         //!< component macroblock dimensions
  uint32_t mb_size_blk[3][2];                     //!< component macroblock dimensions 
  uint32_t mb_size_shift[3][2];
  
  int max_vmv_r;                             //!< maximum vertical motion vector range in luma quarter frame pixel units for the current level_idc
  bool separate_colour_plane_flag;
  ColorFormat ChromaArrayType;
  int max_frame_num;
  PIXEL_COORD PicWidthInMbs;
  PIXEL_COORD PicHeightInMapUnits;
  PIXEL_COORD FrameHeightInMbs;
  PIXEL_COORD FrameSizeInMbs;
  PIXEL_COORD iLumaPadX;
  PIXEL_COORD iLumaPadY;
  PIXEL_COORD iChromaPadX;
  PIXEL_COORD iChromaPadY;

  BLOCK_COORD subpel_x;
  BLOCK_COORD subpel_y;
  BLOCK_COORD shiftpel_x;
  BLOCK_COORD shiftpel_y;
  int total_scale;
  unsigned int oldFrameSizeInMbs;

  //padding info;
  void (CDecoderH264::*img2buf)(imgpel** imgX, uint8_t* buf, PIXEL_COORD size_x, PIXEL_COORD size_y, uint8_t symbol_size_in_bytes, 
		PIXEL_COORD crop_left, PIXEL_COORD crop_right, PIXEL_COORD crop_top, PIXEL_COORD crop_bottom, PIXEL_COORD iOutStride);
  bool rgb_output;

  imgpel **imgY_ref;                              //!< reference frame find snr
  imgpel ***imgUV_ref;
  CMacroblock *mb_data;               //!< array containing all MBs of a whole frame
  CMacroblock *mb_data_JV[MAX_PLANE]; //!< mb_data to be used for 4:4:4 independent mode
  bool *intra_block;
  bool *intra_block_JV[MAX_PLANE];
  BlockPos *PicPos;  
  uint8_t **ipredmode;                  //!< prediction type [90][74]
  uint8_t **ipredmode_JV[MAX_PLANE];
  uint8_t ****nz_coeff;
  int **siblock;
  int **siblock_JV[MAX_PLANE];
  int8_t *qp_per_matrix;
  int8_t *qp_rem_matrix;
	} CodingParameters;

typedef struct layer_par {
  uint8_t layer_id;
  CVideoParameters *p_Vid;
  CodingParameters *p_Cps;
  seq_parameter_set_rbsp_t *p_SPS;
  struct decoded_picture_buffer *p_Dpb;
	} LayerParameters;


//****************************** ~DM ***********************************
typedef struct bit_stream_dec Bitstream;

/*! Buffer structure for decoded reference picture marking commands */
typedef struct DecRefPicMarking_s {
  int memory_management_control_operation;
  int difference_of_pic_nums_minus1;
  int long_term_pic_num;
  int long_term_frame_idx;
  int max_long_term_frame_idx_plus1;
  struct DecRefPicMarking_s *Next;
	} DecRefPicMarking_t;

//! DataPartition
typedef struct datapartition_dec {
  Bitstream           *bitstream;
  DecodingEnvironment de_cabac;

  int (CDecoderH264::*readSyntaxElement)(CMacroblock *currMB, CSyntaxElement *, struct datapartition_dec *);
      /*!< virtual function;
           actual method depends on chosen data partition and entropy coding method  */
	} DataPartition;

#define NUM_MB_TYPE_CTX  11
#define NUM_B8_TYPE_CTX  9
#define NUM_MV_RES_CTX   10
#define NUM_REF_NO_CTX   6
#define NUM_DELTA_QP_CTX 4
#define NUM_MB_AFF_CTX 4
#define NUM_TRANSFORM_SIZE_CTX 3

typedef struct {
  BiContextType mb_type_contexts [3][NUM_MB_TYPE_CTX];
  BiContextType b8_type_contexts [2][NUM_B8_TYPE_CTX];
  BiContextType mv_res_contexts  [2][NUM_MV_RES_CTX];
  BiContextType ref_no_contexts  [2][NUM_REF_NO_CTX];
  BiContextType delta_qp_contexts[NUM_DELTA_QP_CTX];
  BiContextType mb_aff_contexts  [NUM_MB_AFF_CTX];
	} MotionInfoContexts;

#define NUM_IPR_CTX    2
#define NUM_CIPR_CTX   4
#define NUM_CBP_CTX    4
#define NUM_BCBP_CTX   4
#define NUM_MAP_CTX   15
#define NUM_LAST_CTX  15
#define NUM_ONE_CTX    5
#define NUM_ABS_CTX    5

#if (ENABLE_HIGH444_CTX == 1)
# define NUM_BLOCK_TYPES 22  
#else
# define NUM_BLOCK_TYPES 10
#endif

typedef struct {
  BiContextType transform_size_contexts [NUM_TRANSFORM_SIZE_CTX];
  BiContextType ipr_contexts [NUM_IPR_CTX];
  BiContextType cipr_contexts[NUM_CIPR_CTX];
  BiContextType cbp_contexts [3][NUM_CBP_CTX];
  BiContextType bcbp_contexts[NUM_BLOCK_TYPES][NUM_BCBP_CTX];
  BiContextType map_contexts [2][NUM_BLOCK_TYPES][NUM_MAP_CTX];
  BiContextType last_contexts[2][NUM_BLOCK_TYPES][NUM_LAST_CTX];
  BiContextType one_contexts [NUM_BLOCK_TYPES][NUM_ONE_CTX];
  BiContextType abs_contexts [NUM_BLOCK_TYPES][NUM_ABS_CTX];
	} TextureInfoContexts;

#if MVC_EXTENSION_ENABLE
typedef struct nalunitheadermvcext_tag {
  bool non_idr_flag;
  unsigned int priority_id;
  int8_t view_id;
  uint32_t temporal_id;
  bool anchor_pic_flag;
  bool inter_view_flag;
  uint8_t reserved_one_bit;
  unsigned int iPrefixNALU;
	} NALUnitHeaderMVCExt_t;
#endif

typedef struct wp_params {
  int16_t weight[3];
  int16_t offset[3];
	} WPParams;

typedef enum {
  EOS = 1,    //!< End Of Sequence
  SOP = 2,    //!< Start Of Picture
  SOS = 3,     //!< Start Of CSlice
  SOS_CONT = 4
	} HEADER_TYPE;


//! CSlice   -------------------------------------------------------------------------------------------------
class CSlice {				// classe, in futuro metterci i metodi relativi (provato 9/2/26 ma è un casino!
public:
  CVideoParameters    *p_Vid;
  struct inp_par      *p_Inp;
  pic_parameter_set_rbsp_t *active_pps;
  seq_parameter_set_rbsp_t *active_sps;
  int8_t svc_extension_flag;		// -1 0 1

  // dpb pointer
  struct decoded_picture_buffer *p_Dpb;

  //slice property;
  bool idr_flag;
  int idr_pic_id;
  int nal_reference_idc;                       //!< nal_reference_idc from NAL unit
  bool Transform8x8Mode;
  bool chroma444_not_separate;              //!< indicates chroma 4:4:4 coding with separate_colour_plane_flag equal to zero

  PocType toppoc;      //poc for this top field
  PocType bottompoc;   //poc of bottom field of frame
  PocType framepoc;    //poc of this frame

  //the following is for slice header syntax elements of poc
  // for poc mode 0.
  unsigned int pic_order_cnt_lsb;
  int delta_pic_order_cnt_bottom;
  // for poc mode 1.
  int delta_pic_order_cnt[2];

  // ////////////////////////
  // for POC mode 0:
  signed int PicOrderCntMsb;

  //signed   int PrevPicOrderCntMsb;
  //unsigned int PrevPicOrderCntLsb;

  // for POC mode 1:
  unsigned int AbsFrameNum;
  PocType ThisPOC;
  //signed int ExpectedPicOrderCnt, PicOrderCntCycleCnt, FrameNumInPicOrderCntCycle;
  //unsigned int PreviousFrameNum, FrameNumOffset;
  //int ExpectedDeltaPerPicOrderCntCycle;
  //int PreviousFrameNumOffset;
  // /////////////////////////

  //information need to move to slice;
  unsigned int current_mb_nr; // bitstream order
  unsigned int num_dec_mb;
  int16_t      current_slice_nr;
  //int mb_x;
  //int mb_y;
  //int block_x;
  //int block_y;
  //int pix_c_x;
  //int pix_c_y;
  int cod_counter;                   //!< Current count of number of skipped macroblocks in a row
  bool allrefzero;
  //end;

  bool                mb_aff_frame_flag;
  bool                direct_spatial_mv_pred_flag;       //!< Indicator for direct mode type (1 for Spatial, 0 for Temporal)
  int                 num_ref_idx_active[2];             //!< number of available list references
  //int                 num_ref_idx_l0_active;             //!< number of available list 0 references
  //int                 num_ref_idx_l1_active;             //!< number of available list 1 references

  bool                ei_flag;       //!< 0 if the partArr[0] contains valid information
  int8_t              qp;
  int                 slice_qp_delta;
  int8_t              qs;
  int                 slice_qs_delta;
  SliceType           slice_type;    //!< slice type
  int8_t              model_number;  //!< cabac model number
  unsigned int        frame_num;   //frame_num for this frame
  bool								field_pic_flag;
  bool								bottom_field_flag;
  PictureStructure    structure;     //!< Identify picture structure type
  int                 start_mb_nr;   //!< MUST be set by NAL even in case of ei_flag == 1
  int                 end_mb_nr_plus1;
  int                 max_part_nr;
  int8_t              dp_mode;       //!< data partitioning mode
  HEADER_TYPE         current_header;
  int                 next_header;
  int                 last_dquant;

  //slice header information;
  ColorPlane colour_plane_id;               //!< colour_plane_id of the current coded slice
  int redundant_pic_cnt;
  int8_t sp_switch;                              //!< 1 for switching sp, 0 for normal sp  
  int slice_group_change_cycle;
  int redundant_slice_ref_idx;     //!< reference index of redundant slice
  bool no_output_of_prior_pics_flag;
  bool long_term_reference_flag;
  bool adaptive_ref_pic_buffering_flag;
  DecRefPicMarking_t *dec_ref_pic_marking_buffer;                    //!< stores the memory management control operations

  int8_t listXsize[6];
  struct storable_picture **listX[6];

  //  int                 last_mb_nr;    //!< only valid when entropy coding == CABAC
  DataPartition       *partArr;      //!< array of partitions
  MotionInfoContexts  *mot_ctx;      //!< pointer to struct of context models for use in CABAC
  TextureInfoContexts *tex_ctx;      //!< pointer to struct of context models for use in CABAC

  int mvscale[6][MAX_REFERENCE_PICTURES];

  bool                ref_pic_list_reordering_flag[2];
  int                 *modification_of_pic_nums_idc[2];
  int                 *abs_diff_pic_num_minus1[2];
  int                 *long_term_pic_idx[2];

#if MVC_EXTENSION_ENABLE
  int                 *abs_diff_view_idx_minus1[2];
  int8_t              view_id;
  bool                inter_view_flag;
  bool                anchor_pic_flag;
  NALUnitHeaderMVCExt_t NaluHeaderMVCExt;
#endif

  uint8_t             layer_id;
  int8_t              DFDisableIdc;     //!< Disable deblocking filter on slice
  int16_t             DFAlphaC0Offset;  //!< Alpha and C0 offset for filtering slice
  int16_t             DFBetaOffset;     //!< Beta offset for filtering slice

  int                 pic_parameter_set_id;   //!<the ID of the picture parameter set the slice is reffering to

  bool                dpB_NotPresent;    //!< non-zero, if data partition B is lost
  bool                dpC_NotPresent;    //!< non-zero, if data partition C is lost

  bool is_reset_coeff;
  bool is_reset_coeff_cr;
  imgpel  ***mb_pred;
  imgpel  ***mb_rec;
  int     ***mb_rres;
  int     ***cof;
  int     ***fcf;

  int cofu[16];

  imgpel **tmp_block_l0;
  imgpel **tmp_block_l1;  
  int    **tmp_res;
  imgpel **tmp_block_l2;
  imgpel **tmp_block_l3;  

  // Scaling matrix info
  int16_t InvLevelScale4x4_Intra[3][6][4][4];
  int16_t InvLevelScale4x4_Inter[3][6][4][4];
  int16_t InvLevelScale8x8_Intra[3][6][8][8];
  int16_t InvLevelScale8x8_Inter[3][6][8][8];

  const int16_t *qmatrix[12];

  // Cabac
  int  coeff[64]; // one more for EOB
  int  coeff_ctr;
  int  pos;  


  //weighted prediction
  bool weighted_pred_flag;
  uint8_t weighted_bipred_idc;

  uint16_t luma_log2_weight_denom;
  uint16_t chroma_log2_weight_denom;
  
  WPParams **wp_params; // wp parameters in [list][index]

  int ***wp_weight;  // weight in [list][index][component] order
  int ***wp_offset;  // offset in [list][index][component] order
  int ****wbp_weight; //weight in [list][fw_index][bw_index][component] order
  int16_t wp_round_luma;
  int16_t wp_round_chroma;

#if MVC_EXTENSION_ENABLE
  int listinterviewidx0;
  int listinterviewidx1;
  struct frame_store **fs_listinterview0;
  struct frame_store **fs_listinterview1;
#endif

  // for signalling to the neighbour logic that this is a deblocker call
  //uint8_t mixedModeEdgeFlag;
  int max_mb_vmv_r;                   //!< maximum vertical motion vector range in luma quarter pixel units for the current level_idc
  int8_t ref_flag[17];                //!< 0: i-th previous frame is incorrect

  int erc_mvperMB;
  CMacroblock *mb_data;
  struct storable_picture *dec_picture;
  int **siblock;
  uint8_t **ipredmode;
  bool *intra_block;
  int8_t  chroma_vector_adjustment[6][32];
  void (CDecoderH264::*read_CBP_and_coeffs_from_NAL) (CMacroblock *currMB);
  int  (CDecoderH264::*decode_one_component     )    (CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, struct storable_picture *dec_picture);
  int  (CDecoderH264::*readSlice                )    (CVideoParameters *, struct inp_par *);  
  bool (CDecoderH264::*nal_startcode_follows    )    (CSlice*, bool);
  void (CDecoderH264::*read_motion_info_from_NAL)    (CMacroblock *currMB);
  void (CDecoderH264::*read_one_macroblock      )    (CMacroblock *currMB);
  void (CDecoderH264::*interpret_mb_mode        )    (CMacroblock *currMB);
  void (CDecoderH264::*init_lists               )    (CSlice *currSlice);

  void (CDecoderH264::*intra_pred_chroma        )    (CMacroblock *currMB);
  int  (CDecoderH264::*intra_pred_4x4)               (CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff,PIXEL_COORD i4,PIXEL_COORD j4);
  int  (CDecoderH264::*intra_pred_8x8)               (CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
  int  (CDecoderH264::*intra_pred_16x16)             (CMacroblock *currMB, ColorPlane pl, I16x16PredModes predmode);

  void (CDecoderH264::*linfo_cbp_intra          )    (int len, int info, int8_t *cbp, int *dummy);
  void (CDecoderH264::*linfo_cbp_inter          )    (int len, int info, int8_t *cbp, int *dummy);    
  void (CDecoderH264::*update_direct_mv_info    )    (CMacroblock *currMB);
  void (CDecoderH264::*read_coeff_4x4_CAVLC     )    (CMacroblock *currMB, CAVLCBlockTypes block_type, int i, int j, 
		int8_t levarr[16], int8_t runarr[16], int8_t *number_coefficients);
	};

#define TIMEB    timeb
#define TIME_T   struct timeval
#define OPENFLAGS_WRITE OF_WRITE //_O_WRONLY|_O_CREAT|_O_BINARY|_O_TRUNC
#define OPEN_PERMISSIONS _S_IREAD | _S_IWRITE
#define OPENFLAGS_READ  OF_READ //_O_RDONLY|_O_BINARY
#define FORMAT_OFF_T "I64d"

typedef struct frame_format {  
  ColorFormat yuv_format;                    //!< YUV format (0=4:0:0, 1=4:2:0, 2=4:2:2, 3=4:4:4)
  ColorModel  color_model;                   //!< 4:4:4 format (0: YUV, 1: RGB, 2: XYZ)
  PixelFormat pixel_format;                  //!< pixel format support for certain interleaved yuv sources
  double      frame_rate;                    //!< frame rate
  PIXEL_COORD width[3];                      //!< component frame width
  PIXEL_COORD height[3];                     //!< component frame height    
  int         auto_crop_right;               //!< luma component auto crop right
  int         auto_crop_bottom;              //!< luma component auto crop bottom
  int         auto_crop_right_cr;            //!< chroma component auto crop right
  int         auto_crop_bottom_cr;           //!< chroma component auto crop bottom
  PIXEL_COORD width_crop;                    //!< width after cropping consideration
  PIXEL_COORD height_crop;                   //!< height after cropping consideration
  PIXEL_COORD mb_width;                      //!< luma component frame width
  PIXEL_COORD mb_height;                     //!< luma component frame height    
  int         size_cmp[3];                   //!< component sizes (width * height)
  int         size;                          //!< total image size (sum of size_cmp)
  int8_t      bit_depth[3];                  //!< component bit depth  
  int         max_value[3];                  //!< component max value
  int         max_value_sq[3];               //!< component max value squared
  int         pic_unit_size_on_disk;         //!< picture sample unit size on storage medium
  int         pic_unit_size_shift3;          //!< pic_unit_size_on_disk >> 3
	} FrameFormat;

typedef struct decodedpic_t {
  int8_t bValid;                 //0: invalid, 1: valid, 3: valid for 3D output;
  int8_t iViewId;                //-1: single view, >=0 multiview[VIEW1|VIEW0];
  PocType iPOC;
  ColorFormat iYUVFormat;             //0: 4:0:0, 1: 4:2:0, 2: 4:2:2, 3: 4:4:4
  int8_t iYUVStorageFormat;      //0: YUV seperate; 1: YUV interleaved; 2: 3D output;
  int8_t iBitDepth;
  uint8_t *pY;                   //if iPictureFormat is 1, [0]: top; [1] bottom;
  uint8_t *pU;
  uint8_t *pV;
  PIXEL_COORD iWidth;                 //frame width;              
  PIXEL_COORD iHeight;                //frame height;
  PIXEL_COORD iYBufStride;            //stride of pY[0/1] buffer in bytes;
  PIXEL_COORD iUVBufStride;           //stride of pU[0/1] and pV[0/1] buffer in bytes;
  int iSkipPicNum;
  uint32_t iBufSize;
  struct decodedpic_t *pNext;
	} DecodedPicList;

typedef struct image_data {
  FrameFormat format;               //!< image format
  // Standard data
  imgpel **frm_data[MAX_PLANE];     //!< Frame Data
  imgpel **top_data[MAX_PLANE];     //!< pointers to top field data
  imgpel **bot_data[MAX_PLANE];     //!< pointers to bottom field data

  imgpel **frm_data_buf[2][MAX_PLANE];     //!< Frame Data
  imgpel **top_data_buf[2][MAX_PLANE];     //!< pointers to top field data
  imgpel **bot_data_buf[2][MAX_PLANE];     //!< pointers to bottom field data
  
  //! Optional data (could also add uint8 data in case imgpel is of type uint16_t)
  //! These can be useful for enabling input/conversion of content of different types
  //! while keeping optimal processing size.
  uint16_t **frm_uint16[MAX_PLANE];   //!< optional frame Data for uint16_t
  uint16_t **top_uint16[MAX_PLANE];   //!< optional pointers to top field data
  uint16_t **bot_uint16[MAX_PLANE];   //!< optional pointers to bottom field data

  PIXEL_COORD frm_stride[MAX_PLANE];
  PIXEL_COORD top_stride[MAX_PLANE];
  PIXEL_COORD bot_stride[MAX_PLANE];
	} ImageData;


class CSlice;

//! Syntaxelement -------------------------------------------------------------------------------------------------
class CSyntaxElement {		// classe, in futuro metterci i metodi relativi (provato 9/2/26 ma è un casino!
public:
  int8_t        type;                  //!< type of syntax element for data part.
  int8_t        value1;                //!< numerical value of syntax element
	// DOVREBBERO andare anche a 16 o addirittura a 8bit, ma si pianta con memoria corrotta... verificare TUTTE le funzioni implicate lv ecc
  int           value2;                //!< for blocked symbols, e.g. run/level
  int           len;                   //!< length of code
  int           inf;                   //!< info part of CAVLC code
  unsigned int  bitpattern;            //!< CAVLC bitpattern
  int8_t        context;               //!< CABAC context
  int8_t        k;                     //!< CABAC context for coeff_count,uv

#if JTRACE
  #define       TRACESTRING_SIZE 100           //!< size of trace string
  char          tracestring[TRACESTRING_SIZE]; //!< trace string
#endif

  //! for mapping of CAVLC to syntaxElement
  void (CDecoderH264::*mapping)(int len, int info, int8_t *value1, int *value2);
  //! used for CABAC: refers to actual coding method of each individual syntax element type
  void (CDecoderH264::*reading)(CMacroblock *currMB, CSyntaxElement *, DecodingEnvironmentPtr);
	};


/*
* typedefs
*/

//! YUV pixel domain image arrays for a video frame
typedef struct frame_s {
  CVideoParameters *p_Vid;
  imgpel *yptr;
  imgpel *uptr;
  imgpel *vptr;
	} frame;

/* segment data structure */
typedef struct ercSegment_s {
  int16_t  startMBPos;
  int16_t  endMBPos;
  bool     fCorrupted;
	} ercSegment_t;

/* Error detector & concealment instance data structure */
typedef struct ercVariables_s {
  /*  Number of macroblocks (size or size/4 of the arrays) */
  int   nOfMBs;
  /* Number of segments (slices) in frame */
  int     nOfSegments;

  /*  Array for conditions of Y blocks */
  int8_t   *yCondition;
  /*  Array for conditions of U blocks */
  int8_t   *uCondition;
  /*  Array for conditions of V blocks */
  int8_t   *vCondition;

  /* Array for CSlice level information */
  ercSegment_t *segments;
  int     currSegment;

  /* Conditions of the MBs of the previous frame */
  int8_t  *prevFrameYCondition;

  /* Flag telling if the current segment was found to be corrupted */
  int   currSegmentCorrupted;
  /* Counter for corrupted segments per picture */
  int   nOfCorruptedSegments;

  /* State variables for error detector and concealer */
  int   concealment;
	} ercVariables_t;


//! region structure stores information about a region that is needed for concealment
typedef struct object_buffer {
  uint8_t regionMode;  //!< region mode as above
  BLOCK_COORD xMin;         //!< X coordinate of the pixel position of the top-left corner of the region
  BLOCK_COORD yMin;         //!< Y coordinate of the pixel position of the top-left corner of the region
  int mv[3];        //!< motion vectors in 1/4 pixel units: mvx = mv[0], mvy = mv[1], and ref_frame = mv[2]
	} objectBuffer_t;


//! definition of pic motion parameters
typedef struct pic_motion_params_old {
  bool *mb_field;      //!< field macroblock indicator
	} PicMotionParamsOld;

//! definition a picture (field or frame)
typedef struct storable_picture {
  PictureStructure structure;

  PocType     poc;
  PocType     top_poc;
  PocType     bottom_poc;
  PocType     frame_poc;
  unsigned int  frame_num;
  unsigned int  recovery_frame;

  int         pic_num;
  int         long_term_pic_num;
  int         long_term_frame_idx;

  uint8_t     is_long_term;
  bool        used_for_reference;
  bool        is_output;
  bool        non_existing;
  bool        separate_colour_plane_flag;

  int16_t     max_slice_id;

  PIXEL_COORD    size_x, size_y, size_x_cr, size_y_cr;
  PIXEL_COORD    size_x_m1, size_y_m1, size_x_cr_m1, size_y_cr_m1;
  bool        coded_frame;
  bool        mb_aff_frame_flag;
  PIXEL_COORD    PicWidthInMbs;
  PIXEL_COORD    PicSizeInMbs;
  PIXEL_COORD    iLumaPadY, iLumaPadX;
  PIXEL_COORD    iChromaPadY, iChromaPadX;

  imgpel **     imgY;         //!< Y picture component
  imgpel ***    imgUV;        //!< U and V picture components

  struct pic_motion_params **mv_info;          //!< Motion info
  struct pic_motion_params **JVmv_info[MAX_PLANE];          //!< Motion info

  struct pic_motion_params_old motion;              //!< Motion info  
  struct pic_motion_params_old JVmotion[MAX_PLANE]; //!< Motion info for 4:4:4 independent mode decoding

  struct storable_picture *top_field;     // for mb aff, if frame for referencing the top field
  struct storable_picture *bottom_field;  // for mb aff, if frame for referencing the bottom field
  struct storable_picture *frame;         // for mb aff, if field for referencing the combined frame

  SliceType   slice_type;
  bool        idr_flag;
  bool        no_output_of_prior_pics_flag;
  bool        long_term_reference_flag;
  bool        adaptive_ref_pic_buffering_flag;

  ColorFormat chroma_format_idc;
  bool        frame_mbs_only_flag;
  bool        frame_cropping_flag;
  BLOCK_COORD     frame_crop_left_offset;
  BLOCK_COORD     frame_crop_right_offset;
  BLOCK_COORD     frame_crop_top_offset;
  int16_t     frame_crop_bottom_offset;
  int8_t      qp;
  int         chroma_qp_offset[2];
  int         slice_qp_delta;
  DecRefPicMarking_t *dec_ref_pic_marking_buffer;                    //!< stores the memory management control operations

  // picture error concealment
  bool        concealed_pic; //indicates if this is a concealed picture
  
  // variables for tone mapping
  bool        seiHasTone_mapping;
  int8_t      tone_mapping_model_id;
  int8_t      tonemapped_bit_depth;  
  imgpel*     tone_mapping_lut;                //!< tone mapping look up table

  int         proc_flag;
#if MVC_EXTENSION_ENABLE
  int8_t      view_id;
  bool        inter_view_flag;
  bool        anchor_pic_flag;
#endif
  PIXEL_COORD iLumaStride;
  PIXEL_COORD iChromaStride;
  PIXEL_COORD iLumaExpandedHeight;
  PIXEL_COORD iChromaExpandedHeight;
  imgpel **cur_imgY; // for more efficient get_block_luma
  bool no_ref;
  CodingType iCodingType;
  //
  int8_t listXsize[MAX_NUM_SLICES][2];
  struct storable_picture **listX[MAX_NUM_SLICES][2];
  uint8_t     layer_id;
	} StorablePicture;

typedef StorablePicture *StorablePicturePtr;

struct concealment_node {
  StorablePicture *picture;
  int  missingpocs;
  struct concealment_node *next;
	};

// video parameters -------------------------------------------------------------------------------------------------
class CVideoParameters {		// classe, in futuro metterci i metodi relativi (provato 9/2/26 ma è un casino!
public:
  struct inp_par *p_Inp;
  pic_parameter_set_rbsp_t *active_pps;
  seq_parameter_set_rbsp_t *active_sps;
  seq_parameter_set_rbsp_t SeqParSet[MAXSPS];
  pic_parameter_set_rbsp_t PicParSet[MAXPPS];
  struct decoded_picture_buffer *p_Dpb_layer[MAX_NUM_DPB_LAYERS];
  CodingParameters *p_EncodePar[MAX_NUM_DPB_LAYERS];
  LayerParameters *p_LayerPar[MAX_NUM_DPB_LAYERS];

#if MVC_EXTENSION_ENABLE
  subset_seq_parameter_set_rbsp_t *active_subset_sps;
  //int svc_extension_flag;
  subset_seq_parameter_set_rbsp_t SubsetSeqParSet[MAXSPS];
  PIXEL_COORD last_pic_width_in_mbs_minus1;
  PIXEL_COORD last_pic_height_in_map_units_minus1;
  int last_max_dec_frame_buffering;
  ProfileIDC last_profile_idc;
#endif

  struct sei_params  *p_SEI;

  struct old_slice_par *old_slice;
  struct snr_par       *snr;
  int number;                                 //!< frame number
  
  //current picture property;
  unsigned int num_dec_mb;
  int iSliceNumOfCurrPic;
  int iNumOfSlicesAllocated;
  int iNumOfSlicesDecoded;
  CSlice **ppSliceList;
  bool *intra_block;
  bool *intra_block_JV[MAX_PLANE];
  //int qp;                                     //!< quant for the current frame

  //int sp_switch;                              //!< 1 for switching sp, 0 for normal sp  
  SliceType type;                              //!< image type INTER/INTRA
	uint32_t timestamp;

  uint8_t **ipredmode;                  //!< prediction type [90][74]
  uint8_t **ipredmode_JV[MAX_PLANE];
  uint8_t ****nz_coeff;
  int **siblock;
  int **siblock_JV[MAX_PLANE];
  BlockPos *PicPos;

  bool newframe;
  PictureStructure structure;                     //!< Identify picture structure type

  //CSlice      *currentSlice;          //!< pointer to current CSlice data struct
  CSlice      *pNextSlice;             //!< pointer to first CSlice of next picture;
  CMacroblock *mb_data;               //!< array containing all MBs of a whole frame
  CMacroblock *mb_data_JV[MAX_PLANE]; //!< mb_data to be used for 4:4:4 independent mode
  //int colour_plane_id;               //!< colour_plane_id of the current coded slice
  ColorFormat ChromaArrayType;

  // picture error concealment
  // concealment_head points to first node in list, concealment_end points to
  // last node in list. Initialize both to NULL, meaning no nodes in list yet
  struct concealment_node *concealment_head;
  struct concealment_node *concealment_end;

  unsigned int pre_frame_num;           //!< store the frame_num in the last decoded slice. For detecting gap in frame_num.
  bool non_conforming_stream;

  // ////////////////////////
  // for POC mode 0:
  signed   int PrevPicOrderCntMsb;
  unsigned int PrevPicOrderCntLsb;

  // for POC mode 1:
  signed int ExpectedPicOrderCnt, PicOrderCntCycleCnt, FrameNumInPicOrderCntCycle;
  unsigned int PreviousFrameNum, FrameNumOffset;
  int ExpectedDeltaPerPicOrderCntCycle;
  PocType ThisPOC;
  int PreviousFrameNumOffset;
  // /////////////////////////

  PIXEL_COORD PicHeightInMbs;
  PIXEL_COORD PicSizeInMbs;

  bool no_output_of_prior_pics_flag;

  bool last_has_mmco_5;
  int last_pic_bottom_field;

  int idr_psnr_number;
  int psnr_number;

  // Timing related variables
  TIME_T start_time;
  TIME_T end_time;

  // picture error concealment
  PocType last_ref_pic_poc;
  PocType ref_poc_gap;
  PocType poc_gap;
  int8_t conceal_mode;
  PocType earlier_missing_poc;
  unsigned int frame_to_conceal;
  bool IDR_concealment_flag;
  SliceType conceal_slice_type;

  bool first_sps;
  // random access point decoding
  int recovery_point;
  bool recovery_point_found;
  int recovery_frame_cnt;
  int recovery_frame_num;
  PocType recovery_poc;

  uint8_t *buf;
  uint8_t *ibuf;

  ImageData imgData;           //!< Image data to be encoded (dummy variable for now)
  ImageData imgData0;          //!< base layer input
  ImageData imgData1;          //!< temp buffer for left de-muxed view
  ImageData imgData2;          //!< temp buffer for right de-muxed view

  // Data needed for 3:2 pulldown or temporal interleaving
  ImageData imgData32;           //!< Image data to be encoded
  ImageData imgData4;
  ImageData imgData5;
  ImageData imgData6;

  // Redundant slices. Should be moved to another structure and allocated only if extended profile
  unsigned int previous_frame_num; //!< frame number of previous slice
  //!< non-zero: i-th previous frame is correct
  bool Is_primary_correct;          //!< if primary frame is correct, 0: incorrect
  bool Is_redundant_correct;        //!< if redundant frame is correct, 0:incorrect

  // Time 
  int32_t tot_time;

  // files
  int p_out;                       //!< file descriptor to output YUV file
#if MVC_EXTENSION_ENABLE
  int p_out_mvc[MAX_VIEW_NUM];     //!< file descriptor to output YUV file for MVC
#endif
  int p_ref;                       //!< pointer to input original reference YUV file file

  //FILE *p_log;                     //!< SNR file
  int LastAccessUnitExists;
  int NALUCount;

  // B pictures
  int  Bframe_ctr;
  int  frame_no;

  int  g_nFrame;
  bool global_init_done[2];

  // global picture format dependent buffers, memory allocation in decod.c
  imgpel **imgY_ref;                              //!< reference frame find snr
  imgpel ***imgUV_ref;

  int8_t *qp_per_matrix;
  int8_t *qp_rem_matrix;

  struct frame_store *last_out_fs;
  PocType pocs_in_dpb[100];

  struct storable_picture *dec_picture;
  struct storable_picture *dec_picture_JV[MAX_PLANE];  //!< dec_picture to be used during 4:4:4 independent mode decoding
  struct storable_picture *no_reference_picture; //!< dummy storable picture for recovery point

  // Error parameters
  struct object_buffer  *erc_object_list;
  struct ercVariables_s *erc_errorVar;

  int erc_mvperMB;
  CVideoParameters *erc_img;
  int8_t ec_flag[SE_MAX_ELEMENTS];        //!< array to set errorconcealment

  struct annex_b_struct *annex_b;

  struct frame_store *out_buffer;

  struct storable_picture *pending_output;
  int    pending_output_state;
  bool   recovery_flag;

  int32_t BitStreamFile;

  // report
  char cslice_type[9];  
  // FMO
  int *MbToSliceGroupMap;
  int *MapUnitToSliceGroupMap;
  int  NumberOfSliceGroups;    // the number of slice groups -1 (0 == scan order, 7 == maximum)

#if ENABLE_OUTPUT_TONEMAPPING
  struct tone_mapping_struct_s *seiToneMapping;
#endif

  void (CDecoderH264::*buf2img)          (imgpel** imgX, uint8_t* buf, PIXEL_COORD size_x, PIXEL_COORD size_y, PIXEL_COORD o_size_x, PIXEL_COORD o_size_y, 
		int symbol_size_in_bytes, int16_t bitshift);
  void (CDecoderH264::*getNeighbour)     (CMacroblock *currMB, BLOCK_COORD xN, BLOCK_COORD yN, uint32_t mb_size[2], PixelPos *pix);
  void (CDecoderH264::*get_mb_block_pos) (BlockPos *PicPos, int mb_addr, BLOCK_COORD *x, BLOCK_COORD *y);
  void (CDecoderH264::*GetStrengthVer)   (CMacroblock *MbQ, uint8_t edge, uint8_t mvlimit, struct storable_picture *p);
  void (CDecoderH264::*GetStrengthHor)   (CMacroblock *MbQ, uint8_t edge, uint8_t mvlimit, struct storable_picture *p);
  void (CDecoderH264::*EdgeLoopLumaVer)  (ColorPlane pl, imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge);
  void (CDecoderH264::*EdgeLoopLumaHor)  (ColorPlane pl, imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, struct storable_picture *p);
  void (CDecoderH264::*EdgeLoopChromaVer)(imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, int uv, struct storable_picture *p);
  void (CDecoderH264::*EdgeLoopChromaHor)(imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, int uv, struct storable_picture *p);
  void (CDecoderH264::*img2buf)          (imgpel** imgX, uint8_t* buf, PIXEL_COORD size_x, PIXEL_COORD size_y, uint8_t symbol_size_in_bytes, 
																				PIXEL_COORD crop_left, PIXEL_COORD crop_right, PIXEL_COORD crop_top, PIXEL_COORD crop_bottom, PIXEL_COORD iOutStride);

  ImageData tempData3;
  DecodedPicList *pDecOuputPic;
  int8_t iDeblockMode;  //0: deblock in picture, 1: deblock in slice;
  struct nalu_t *nalu;
  BLOCK_COORD iLumaPadX;
  BLOCK_COORD iLumaPadY;
  BLOCK_COORD iChromaPadX;
  BLOCK_COORD iChromaPadY;
  //control;
  uint8_t bDeblockEnable;
  bool  iPostProcess;
  bool  bFrameInit;
#if _FLTDBG_
  FILE *fpDbg;
#endif
  pic_parameter_set_rbsp_t *pNextPPS;
  PocType last_dec_poc;
  int8_t last_dec_view_id;
  uint8_t last_dec_layer_id;
  uint8_t dpb_layer_id;

/******************* deprecative variables; ***************************************/
  PIXEL_COORD width;
  PIXEL_COORD height;
  PIXEL_COORD width_cr;                               //!< width chroma  
  PIXEL_COORD height_cr;                              //!< height chroma
  // Fidelity Range Extensions Stuff
  uint8_t pic_unit_bitsize_on_disk;
  int8_t bitdepth_luma;
  int8_t bitdepth_chroma;
  int8_t bitdepth_scale[2];
  int8_t bitdepth_luma_qp_scale;
  int8_t bitdepth_chroma_qp_scale;
  unsigned int dc_pred_value_comp[MAX_PLANE]; //!< component value for DC prediction (depends on component pel bit depth)
#if IMGTYPE == 0
  uint8_t max_pel_value_comp[MAX_PLANE];       //!< max value that one picture element (pixel) can take (depends on pic_unit_bitdepth)
#else
  uint16_t max_pel_value_comp[MAX_PLANE];       //!< max value that one picture element (pixel) can take (depends on pic_unit_bitdepth)
#endif

  bool separate_colour_plane_flag;
  int pic_unit_size_on_disk;

  ProfileIDC profile_idc;
  ColorFormat yuv_format;
  bool lossless_qpprime_flag;
  int num_blk8x8_uv;
  int num_uv_blocks;
  int num_cdc_coeff;
  BLOCK_COORD mb_cr_size_x;
  BLOCK_COORD mb_cr_size_y;
  BLOCK_COORD mb_cr_size_x_blk;
  BLOCK_COORD mb_cr_size_y_blk;
  int mb_cr_size;
  uint32_t mb_size[3][2];                         //!< component macroblock dimensions
  uint32_t mb_size_blk[3][2];                     //!< component macroblock dimensions 
  uint32_t mb_size_shift[3][2];
  BLOCK_COORD subpel_x;
  BLOCK_COORD subpel_y;
  BLOCK_COORD shiftpel_x;
  BLOCK_COORD shiftpel_y;
  int total_scale;
  int max_frame_num;

  PIXEL_COORD PicWidthInMbs;
  unsigned int PicHeightInMapUnits;
  PIXEL_COORD FrameHeightInMbs;
  unsigned int FrameSizeInMbs;
  unsigned int oldFrameSizeInMbs;
  int max_vmv_r;                             //!< maximum vertical motion vector range in luma quarter frame pixel units for the current level_idc
  //int max_mb_vmv_r;                        //!< maximum vertical motion vector range in luma quarter pixel units for the current level_idc
/******************* end deprecative variables; ***************************************/

  struct dec_stat_parameters *dec_stats;

public:
	void ercPixConcealIMB(imgpel *currFrame, PIXEL_COORD row, PIXEL_COORD column, int predBlocks[], PIXEL_COORD frameWidth, 
		BLOCK_COORD mbWidthInBlocks);
	static int ercCollect8PredBlocks(int predBlocks[], PIXEL_COORD currRow, PIXEL_COORD currColumn, int8_t *condition,
														PIXEL_COORD maxRow, PIXEL_COORD maxColumn, int8_t step, uint8_t fNoCornerNeigh);
	static int ercCollectColumnBlocks(int predBlocks[], PIXEL_COORD currRow, PIXEL_COORD currColumn, int8_t *condition, 
		PIXEL_COORD maxRow, PIXEL_COORD maxColumn, int8_t step);
	void ercInit(PIXEL_COORD pic_sizex, PIXEL_COORD pic_sizey, bool flag);
	ercVariables_t *ercOpen(void);
	void ercReset(ercVariables_t *errorVar, int nOfMBs, int numOfSegments, PIXEL_COORD picSizeX);
	void ercClose(ercVariables_t *errorVar);
	static void ercSetErrorConcealment(ercVariables_t *errorVar, int value);

	static void ercStartSegment(int currMBNum, int segment, unsigned int bitPos, ercVariables_t *errorVar);
	static void ercStopSegment(int currMBNum, int segment, unsigned int bitPos, ercVariables_t *errorVar);
	static void ercMarkCurrSegmentLost(PIXEL_COORD picSizeX, ercVariables_t *errorVar);
	static void ercMarkCurrSegmentOK(PIXEL_COORD picSizeX, ercVariables_t *errorVar);
	static void ercMarkCurrMBConcealed(int currMBNum, int8_t comp, PIXEL_COORD picSizeX, ercVariables_t *errorVar);

	int ercConcealIntraFrame(frame *recfr, PIXEL_COORD picSizeX, PIXEL_COORD picSizeY, 
												 ercVariables_t *errorVar);
	int ercConcealInterFrame(frame *recfr, objectBuffer_t *object_list,
                          PIXEL_COORD picSizeX, PIXEL_COORD picSizeY, ercVariables_t *errorVar, 
													ColorFormat chroma_format_idc);

	void concealBlocks(PIXEL_COORD lastColumn, PIXEL_COORD lastRow, int8_t comp, frame *recfr, PIXEL_COORD picSizeX, 
		int8_t *condition);
	void pixMeanInterpolateBlock(imgpel *src[], imgpel *block, int blockSize, PIXEL_COORD frameWidth);

	void add_node(struct concealment_node *concealment_new);
	void delete_node(struct concealment_node *ptr);
	void delete_list(struct concealment_node *ptr);

	int8_t set_ec_flag(int8_t se);
	void reset_ec_flags();
	int get_concealed_element(CSyntaxElement *sym);

	int fmo_init(CSlice *pSlice);
	int FmoFinit();

	int FmoGetNumberOfSliceGroup();
	int FmoGetLastMBOfPicture   ();
	int FmoGetLastMBInSliceGroup(int SliceGroup);
	int FmoGetSliceGroupId      (int mb);
	int FmoGetNextMBNr          (int CurrentMbNr);
	int FmoGenerateMapUnitToSliceGroupMap(CSlice *currSlice);
	void FmoGenerateType0MapUnitMap(unsigned PicSizeInMapUnits);
	void FmoGenerateType1MapUnitMap(unsigned PicSizeInMapUnits);
  void FmoGenerateType2MapUnitMap(unsigned PicSizeInMapUnits);
	void FmoGenerateType3MapUnitMap(unsigned PicSizeInMapUnits, CSlice *currSlice);
	void FmoGenerateType4MapUnitMap(unsigned PicSizeInMapUnits, CSlice *currSlice);
	void FmoGenerateType5MapUnitMap(unsigned PicSizeInMapUnits, CSlice *currSlice);
	void FmoGenerateType6MapUnitMap(unsigned PicSizeInMapUnits);
	int FmoGenerateMbToSliceGroupMap(CSlice *pSlice);

	int dumppoc();

	void setup_buffers(uint8_t layer_id);
	void Error_tracking(CSlice *currSlice);
	static void CopyPOC(CSlice *pSlice0, CSlice *currSlice);

	void copy_dec_picture_JV(StorablePicture *dst, StorablePicture *src);
#if MVC_EXTENSION_ENABLE
	int8_t GetVOIdx(int iViewId);
#endif
	int8_t GetViewIdx(int8_t iVOIdx);
	int get_maxViewIdx(int8_t view_id, bool anchor_pic_flag, int listidx);
	};

#define MAXRBSPSIZE 64000
#define MAXNALUSIZE 64000

//! values for nal_unit_type
typedef enum {
	NALU_TYPE_SLICE    = 1,
	NALU_TYPE_DPA      = 2,
	NALU_TYPE_DPB      = 3,
	NALU_TYPE_DPC      = 4,
	NALU_TYPE_IDR      = 5,
	NALU_TYPE_SEI      = 6,
	NALU_TYPE_SPS      = 7,
	NALU_TYPE_PPS      = 8,
	NALU_TYPE_AUD      = 9,
	NALU_TYPE_EOSEQ    = 10,
	NALU_TYPE_EOSTREAM = 11,
	NALU_TYPE_FILL     = 12,
#if MVC_EXTENSION_ENABLE
	NALU_TYPE_PREFIX   = 14,
	NALU_TYPE_SUB_SPS  = 15,
	NALU_TYPE_SLC_EXT  = 20,
	NALU_TYPE_VDRD     = 24  // View and Dependency Representation Delimiter NAL Unit
#endif
	} NaluType;

//! values for nal_ref_idc
typedef enum {
	NALU_PRIORITY_HIGHEST     = 3,
	NALU_PRIORITY_HIGH        = 2,
	NALU_PRIORITY_LOW         = 1,
	NALU_PRIORITY_DISPOSABLE  = 0
	} NalRefIdc;

//! NAL unit structure
typedef struct nalu_t {
  int       startcodeprefix_len;   //!< 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
  unsigned  len;                   //!< Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
  unsigned  max_size;              //!< NAL Unit Buffer size
  int8_t    forbidden_bit;         //!< should be always FALSE
  NaluType  nal_unit_type;         //!< NALU_TYPE_xxxx
  NalRefIdc nal_reference_idc;     //!< NALU_PRIORITY_xxxx  
  uint8_t   *buf;                   //!< contains the first uint8_t followed by the EBSP
  uint16_t  lost_packets;          //!< true, if packet loss is detected
#if MVC_EXTENSION_ENABLE
  int8_t    svc_extension_flag;    //!< should be always 0, for MVC
  bool      non_idr_flag;          //!< 0 = current is IDR
  int       priority_id;           //!< a lower value of priority_id specifies a higher priority
  int8_t    view_id;               //!< view identifier for the NAL unit
  bool      anchor_pic_flag;       //!< anchor access unit
  bool      inter_view_flag;       //!< inter-view prediction enable
  int8_t    reserved_one_bit;      //!< shall be equal to 1
#endif
  uint32_t  temporal_id;           //!< temporal identifier for the NAL unit  MI SERVE anche in profile base!
	} NALU_t;




#define DEFAULTCONFIGFILENAME "decoder.cfg"

//#include "config_common.h"
//#define PROFILE_IDC     88
//#define LEVEL_IDC       21

#define FILE_NAME_SIZE  255

typedef struct video_size {
  char* name;
  PIXEL_COORD x_size;
  PIXEL_COORD y_size;
	} VIDEO_SIZE;

typedef enum {
  VIDEO_UNKNOWN = -1,
  VIDEO_YUV     =  0,
  VIDEO_RGB     =  1,
  VIDEO_XYZ     =  2,
  VIDEO_TIFF    =  3,
  VIDEO_AVI     =  4
	} VideoFileType;

typedef struct video_data_file {
  //char*         fname;          //!< video file name
  char          fname[FILE_NAME_SIZE]; //!< video file name
  char          fhead[FILE_NAME_SIZE]; //!< header of video file
  char          ftail[FILE_NAME_SIZE]; //!< tail of video file
  int           f_num;                 //!< video file number     
  VideoFileType vdtype;                //!< File format
  FrameFormat   format;                //!< video format information
  bool          is_concatenated;       //!< Single or multifile input?
  bool          is_interleaved;        //!< Support for interleaved and non-interleaved input sources
  int           zero_pad;              //!< Used when separate image files are used as input. Enables zero padding for file numbering
  int           num_digits;            //!< Number of digits for file numbering
  int           start_frame;           //!< start frame
  int           end_frame;             //!< end frame
  int           nframes;               //!< number of frames
  PIXEL_COORD   crop_x_size;           //!< crop information (x component)
  PIXEL_COORD   crop_y_size;           //!< crop information (y component)
  PIXEL_COORD   crop_x_offset;         //!< crop offset (x component);
  PIXEL_COORD   crop_y_offset;         //!< crop offset (y component);

  // AVI related information to be added here
  int* avi;
  //avi_t* avi;
  //int    header;
  //char   compressor[8];    
	} VideoDataFile;

// input parameters from configuration file
typedef struct inp_par {
  char infile[FILE_NAME_SIZE];                       //!< H.264 inputfile
  char outfile[FILE_NAME_SIZE];                      //!< Decoded YUV 4:2:0 output
  char reffile[FILE_NAME_SIZE];                      //!< Optional YUV 4:2:0 reference file for SNR measurement
	uint8_t *outbuf;
	uint32_t outbufSize;

  PAR_OF_TYPE FileFormat;                         //!< File format of the Input file, PAR_OF_ANNEXB or PAR_OF_RTP
  int ref_offset;
  int poc_scale;
  int write_uv;
  int8_t silent;
  int8_t intra_profile_deblocking;               //!< Loop filter usage determined by flags and parameters in bitstream 

  // Input/output sequence format related variables
  FrameFormat source;                   //!< source related information
  FrameFormat output;                   //!< output related information
	ColorModel outputFormat;

//  int  ProcessInput;
  int8_t  enable_32_pulldown;
  VideoDataFile input_file1;          //!< Input video file1
  VideoDataFile input_file2;          //!< Input video file2
  VideoDataFile input_file3;          //!< Input video file3
#if MVC_EXTENSION_ENABLE
  bool  DecodeAllLayers;
#endif

#ifdef _LEAKYBUCKET_
  uint32_t R_decoder;                //!< Decoder Rate in HRD Model
  uint32_t B_decoder;                //!< Decoder Buffer size in HRD model
  uint32_t F_decoder;                //!< Decoder Initial buffer fullness in HRD model
  char LeakyBucketParamFile[FILE_NAME_SIZE];         //!< LeakyBucketParamFile
#endif

  // picture error concealment
  int8_t conceal_mode;
  int ref_poc_gap;
  int poc_gap;

  // dummy for encoder
  int start_frame;

  // Needed to allow compilation for decoder. May be used later for distortion computation operations
  bool stdRange;                         //!< 1 - standard range, 0 - full range
  int8_t videoCode;                        //!< 1 - 709, 3 - 601:  See VideoCode in io_tiff.
  bool export_views;
  
  int iDecFrmNum;

  bool bDisplayDecParams;
  int8_t dpb_plus[2];
	} InputParameters;


typedef struct {
  char *TokenName;    //!< name
  void *Place;        //!< address
  int8_t Type;           //!< type:  0-int, 1-char[], 2-double
  double Default;     //!< default value
  int8_t param_limits;   //!< 0: no limits, 1: both min and max, 2: only min (i.e. no negatives), 3: special case for QPs since min needs bitdepth_qp_scale
  double min_limit;
  double max_limit;
  int8_t  char_size;   //!< Dimension of type char[]
	} Mapping;


typedef struct dec_stat_parameters {
  int    frame_ctr           [NUM_SLICE_TYPES];          //!< Counter for different frame coding types (assumes one slice type per frame)
  int32_t  mode_use          [NUM_SLICE_TYPES][MAXMODE]; //!< CMacroblock mode usage per slice
  int32_t  mode_use_transform[NUM_SLICE_TYPES][MAXMODE][2];

  int32_t  *histogram_mv  [2][2];    //!< mv histogram (per list and per direction)
  int32_t  *histogram_refs[2];       //!< reference histogram (per list)
	} DecStatParameters;



#ifdef TRACE
#undef TRACE
#endif
#if defined _DEBUG
# define TRACE           2     //!< 0:Trace off 1:Trace on 2:detailed CABAC context information
#else
# define TRACE           0     //!< 0:Trace off 1:Trace on 2:detailed CABAC context information
#endif



#define INPUT_TEXT_SIZE 1024



// number of intra prediction modes
#define NO_INTRA_PMODE  9

// Direct Mode types
typedef enum {
  DIR_TEMPORAL = 0, //!< Temporal Direct Mode
  DIR_SPATIAL  = 1 //!< Spatial Direct Mode
	} DirectModes;


// Macro defines
#define Q_BITS          15
#define DQ_BITS          6
#define Q_BITS_8        16
#define DQ_BITS_8        6 


#define IS_I16MB(MB)    ((MB)->mb_type==I16MB || (MB)->mb_type==IPCM)
#define IS_DIRECT(MB)   ((MB)->mb_type==0     && (currSlice->slice_type == B_SLICE ))

#define TOTRUN_NUM       15
#define RUNBEFORE_NUM     7
#define RUNBEFORE_NUM_M1  6

// Quantization parameter range
#define MIN_QP          0
#define MAX_QP          51

enum {
  DECODING_OK     = 0,
  SEARCH_SYNC     = 1,
  PICTURE_DECODED = 2
	};

#define LAMBDA_ACCURACY_BITS         16
#define INVALIDINDEX  (-135792468)

#define RC_MAX_TEMPORAL_LEVELS   5

//Start code and Emulation Prevention need this to be defined in identical manner at encoder and decoder
#define ZEROBYTES_SHORTSTARTCODE 2 //indicates the number of zero bytes in the int16_t start-code prefix



/*!
 *  definition of H.264 syntax elements
 *  order of elements follow dependencies for picture reconstruction
 */
/*!
 * \brief   Assignment of old TYPE partition elements to new
 *          elements
 *
 *  old element     | new elements
 *  ----------------+-------------------------------------------------------------------
 *  TYPE_HEADER     | SE_HEADER, SE_PTYPE
 *  TYPE_MBHEADER   | SE_MBTYPE, SE_REFFRAME, SE_INTRAPREDMODE
 *  TYPE_MVD        | SE_MVD
 *  TYPE_CBP        | SE_CBP_INTRA, SE_CBP_INTER
 *  SE_DELTA_QUANT_INTER
 *  SE_DELTA_QUANT_INTRA
 *  TYPE_COEFF_Y    | SE_LUM_DC_INTRA, SE_LUM_AC_INTRA, SE_LUM_DC_INTER, SE_LUM_AC_INTER
 *  TYPE_2x2DC      | SE_CHR_DC_INTRA, SE_CHR_DC_INTER
 *  TYPE_COEFF_C    | SE_CHR_AC_INTRA, SE_CHR_AC_INTER
 *  TYPE_EOS        | SE_EOS
*/

#define SE_HEADER           0
#define SE_PTYPE            1
#define SE_MBTYPE           2
#define SE_REFFRAME         3
#define SE_INTRAPREDMODE    4
#define SE_MVD              5
#define SE_CBP_INTRA        6
#define SE_LUM_DC_INTRA     7
#define SE_CHR_DC_INTRA     8
#define SE_LUM_AC_INTRA     9
#define SE_CHR_AC_INTRA     10
#define SE_CBP_INTER        11
#define SE_LUM_DC_INTER     12
#define SE_CHR_DC_INTER     13
#define SE_LUM_AC_INTER     14
#define SE_CHR_AC_INTER     15
#define SE_DELTA_QUANT_INTER      16
#define SE_DELTA_QUANT_INTRA      17
#define SE_BFRAME           18
#define SE_EOS              19
#define SE_MAX_ELEMENTS     20


typedef enum {
	NO_EC=   0,   //!< no error concealment necessary
	EC_REQ=  1,   //!< error concealment required
	EC_SYNC= 2   //!< search and sync on next header element
	} ErrorConcealment;

#define MAXPARTITIONMODES   2   //!< maximum possible partition modes as defined in assignSE2partition[][]

/*!
 *  \brief  lookup-table to assign different elements to partition
 *
 *  \note   here we defined up to 6 different partitions similar to
 *          document Q15-k-18 described in the PROGFRAMEMODE.
 *          The Sliceheader contains the PSYNC information. \par
 *
 *          Elements inside a partition are not ordered. They are
 *          ordered by occurence in the stream.
 *          Assumption: Only partitionlosses are considered. \par
 *
 *          The texture elements luminance and chrominance are
 *          not ordered in the progressive form
 *          This may be changed in image.c \par
 *
 *          We also defined the proposed internet partition mode
 *          of Stephan Wenger here. To select the desired mode
 *          uncomment one of the two following lines. \par
 *
 *  -IMPORTANT:
 *          Picture- or Sliceheaders must be assigned to partition 0. \par
 *          Furthermore partitions must follow syntax dependencies as
 *          outlined in document Q15-J-23.
 */




/*
* Defines
*/

/* If the average motion vector of the correctly received macroblocks is less than the
threshold, concealByCopy is used, otherwise concealByTrial is used. */
#define MVPERMB_THR 8

/* used to determine the size of the allocated memory for a temporal Region (MB) */
#define DEF_REGION_SIZE 384  /* 8*8*6 */

#define ERC_BLOCK_OK                3
#define ERC_BLOCK_CONCEALED         2
#define ERC_BLOCK_CORRUPTED         1
#define ERC_BLOCK_EMPTY             0


/*
* Functions to convert MBNum representation to blockNum
*/
#define xPosYBlock(currYBlockNum,picSizeX) ((currYBlockNum)%((picSizeX)>>3))
#define yPosYBlock(currYBlockNum,picSizeX) ((currYBlockNum)/((picSizeX)>>3))
#define xPosMB(currMBNum,picSizeX) ((currMBNum)%((picSizeX)>>4))
#define yPosMB(currMBNum,picSizeX) ((currMBNum)/((picSizeX)>>4))
#define MBxy2YBlock(currXPos,currYPos,comp,picSizeX) \
	((((currYPos)<<1)+((comp)>>1))*((picSizeX)>>3)+((currXPos)<<1)+((comp)&1))
#define MBNum2YBlock(currMBNum,comp,picSizeX) \
	MBxy2YBlock(xPosMB((currMBNum),(picSizeX)),yPosMB((currMBNum),(picSizeX)),(comp),(picSizeX))



/* Thomson APIs for concealing entire frame loss */


//! Frame Stores for Decoded Picture Buffer
typedef struct frame_store {
  uint8_t   is_used;               //!< 0=empty; 1=top; 2=bottom; 3=both fields (or frame)
  uint8_t   is_reference;           //!< 0=not used for ref; 1=top used; 2=bottom used; 3=both fields (or frame) used
  uint8_t   is_long_term;           //!< 0=not used for ref; 1=top used; 2=bottom used; 3=both fields (or frame) used
  uint8_t   is_orig_reference;      //!< original marking by nal_ref_idc: 0=not used for ref; 1=top used; 2=bottom used; 3=both fields (or frame) used

  bool      is_non_existent;

  unsigned  frame_num;
  unsigned  recovery_frame;

  int       frame_num_wrap;
  int       long_term_frame_idx;
  bool      is_output;
  PocType   poc;

  // picture error concealment
  bool concealment_reference;

  StorablePicture *frame;
  StorablePicture *top_field;
  StorablePicture *bottom_field;

#if MVC_EXTENSION_ENABLE
  int8_t   view_id;
  bool      inter_view_flag[2];
  bool      anchor_pic_flag[2];
#endif
  uint8_t   layer_id;
	} FrameStore;

//! Decoded Picture Buffer
typedef struct decoded_picture_buffer {
  CVideoParameters *p_Vid;
  InputParameters *p_Inp;
  FrameStore  **fs;
  FrameStore  **fs_ref;
  FrameStore  **fs_ltref;
  FrameStore  **fs_ilref; // inter-layer reference (for multi-layered codecs)
  unsigned      size;
  unsigned      used_size;
  unsigned      ref_frames_in_buffer;
  unsigned      ltref_frames_in_buffer;
  PocType       last_output_poc;
#if MVC_EXTENSION_ENABLE
  int8_t       last_output_view_id;
#endif
  int           max_long_term_pic_idx;  

  bool          init_done;
  int           num_ref_frames;

  FrameStore   *last_picture;
  unsigned     used_size_il;
  uint8_t      layer_id;

  //DPB related function;
	} DecodedPictureBuffer;


#define isSplitted(object_list,currMBNum) \
    ((object_list+((currMBNum)<<2))->regionMode >= REGMODE_SPLITTED)

/* this can be used as isBlock(...,INTRA) or isBlock(...,INTER_COPY) */
#define isBlock(object_list,currMBNum,comp,regMode) \
    (isSplitted(object_list,currMBNum) ? \
     ((object_list+((currMBNum)<<2)+(comp))->regionMode == REGMODE_##regMode##_8x8) : \
     ((object_list+((currMBNum)<<2))->regionMode == REGMODE_##regMode))

/* this can be used as getParam(...,mv) or getParam(...,xMin) or getParam(...,yMin) */
#define getParam(object_list,currMBNum,comp,param) \
    (isSplitted(object_list,currMBNum) ? \
     ((object_list+((currMBNum)<<2)+(comp))->param) : \
     ((object_list+((currMBNum)<<2))->param))


/* "block" means an 8x8 pixel area */

/* Region modes */
#define REGMODE_INTER_COPY       0  //!< Copy region
#define REGMODE_INTER_PRED       1  //!< Inter region with motion vectors
#define REGMODE_INTRA            2  //!< Intra region
#define REGMODE_SPLITTED         3  //!< Any region mode higher than this indicates that the region
                                    //!< is splitted which means 8x8 block
#define REGMODE_INTER_COPY_8x8   4
#define REGMODE_INTER_PRED_8x8   5
#define REGMODE_INTRA_8x8        6



/***********************************************************************
 * T y p e    d e f i n i t i o n s    f o r    J M
 ***********************************************************************
 */
typedef enum {
  DEC_OPENED = 0,
  DEC_STOPPED,
	} DecoderStatus_e;

typedef enum {
  LumaComp = 0,
  CrComp = 1,
  CbComp = 2
	} Color_Component;


/**********************************************************************
 * C O N T E X T S   F O R   T M L   S Y N T A X   E L E M E N T S
 **********************************************************************
 */



//*********************** end of data type definition for CABAC *******************

/***********************************************************************
 * N e w   D a t a    t y p e s   f o r    T M L
 ***********************************************************************
 */


//! Bitstream
struct bit_stream_dec {
  // CABAC Decoding
  int           read_len;           //!< actual position in the codebuffer, CABAC only
  int           code_len;           //!< overall codebuffer length, CABAC only
  // CAVLC Decoding
  int           frame_bitoffset;    //!< actual position in the codebuffer, bit-oriented, CAVLC only
  int           bitstream_length;   //!< over codebuffer lnegth, uint8_t oriented, CAVLC only
  // ErrorConcealment
  uint8_t       *streamBuffer;      //!< actual codebuffer for read bytes
  bool          ei_flag;            //!< error indication, 0: no error, else unspecified error
	};



// signal to noise ratio parameters
typedef struct snr_par {
  int   frame_ctr;
  float snr[3];                                //!< current SNR (component)
  float snr1[3];                               //!< SNR (dB) first frame (component)
  float snra[3];                               //!< Average component SNR (dB) remaining frames
  float sse[3];                                //!< component SSE 
  float msse[3];                                //!< Average component SSE 
	} SNRParameters;


typedef struct old_slice_par {
  bool     field_pic_flag;   
  unsigned frame_num;
  int      nal_ref_idc;
  unsigned pic_oder_cnt_lsb;
  int      delta_pic_oder_cnt_bottom;
  int      delta_pic_order_cnt[2];
  bool  bottom_field_flag;
  bool  idr_flag;
  int      idr_pic_id;
  int      pps_id;
#if MVC_EXTENSION_ENABLE
  int8_t  view_id;
  bool     inter_view_flag;
  bool     anchor_pic_flag;
#endif
  uint8_t  layer_id;
	} OldSliceParams;

typedef struct decoder_params {
  InputParameters   *p_Inp;          //!< Input Parameters
  CVideoParameters   *p_Vid;          //!< Image Parameters
  int32_t /*64*/     bufferSize;     //!< buffersize for tiff reads (not currently supported)
  int                UsedBits;      // for internal statistics, is adjusted by read_se_v, read_ue_v, read_u_1
  FILE              *p_trace;        //!< Trace file
  int                bitcounter;
	jmp_buf						mark;  
	} DecoderParams;



typedef enum{
  DEC_GEN_NOERR = 0,
  DEC_OPEN_NOERR = 0,
  DEC_CLOSE_NOERR = 0,  
  DEC_SUCCEED = 0,
  DEC_EOS =1,
  DEC_NEED_DATA = 2,
  DEC_INVALID_PARAM = 3,
  DEC_ERRMASK = 0x8000
//  DEC_ERRMASK = 0x80000000
	} DecErrCode;

typedef struct dec_set_t {
  int iPostprocLevel; // valid interval are [0..100]
  int bDBEnable;
  int bAllLayers;
  int time_incr;
  int bDecCompAdapt;
	} DecSet_t;



#define GROUP_SIZE  1

/*********************************************************************************************************/

// NOTE: In principle, the alpha and beta tables are calculated with the formulas below
//       Alpha( qp ) = 0.8 * (2^(qp/6)  -  1)
//       Beta ( qp ) = 0.5 * qp  -  7




#define MAX_LIST_SIZE 33

//! definition of pic motion parameters
typedef struct pic_motion_params {
  struct storable_picture *ref_pic[2];  //!< referrence picture pointer
  MotionVector             mv[2];       //!< motion vector  
  int8_t                   ref_idx[2];  //!< reference picture   [list][subblock_y][subblock_x]
  //uint8_t                   mb_field;    //!< field macroblock indicator
  uint8_t                  slice_no;
	} PicMotionParams;




#define MAXRTPPAYLOADLEN  (65536 - 40)    //!< Maximum payload size of an RTP packet */
#define MAXRTPPACKETSIZE  (65536 - 28)    //!< Maximum size of an RTP packet incl. header */
#define H264PAYLOADTYPE 105               //!< RTP paylaod type fixed here for simplicity*/
#define H264SSRC 0x12345678               //!< SSRC, chosen to simplify debugging */
#define RTP_TR_TIMESTAMP_MULT 1000        //!< should be something like 27 Mhz / 29.97 Hz */

typedef struct {
  uint8_t  v;          //!< Version, 2 bits, MUST be 0x2
  uint8_t  p;          //!< Padding bit, Padding MUST NOT be used
  uint8_t  x;          //!< Extension, MUST be zero
  unsigned int cc;         /*!< CSRC count, normally 0 in the absence
                                of RTP mixers */
  uint8_t  m;          //!< Marker bit
  uint8_t  pt;         //!< 7 bits, Payload Type, dynamically established
  uint16_t       seq;        /*!< RTP sequence number, incremented by one for each sent packet */
  unsigned int timestamp;  //!< timestamp, 27 MHz for H.264
  unsigned int ssrc;       //!< Synchronization Source, chosen randomly
  uint8_t *       payload;    //!< the payload including payload headers
  unsigned int paylen;     //!< length of payload in bytes
  uint8_t *       packet;     //!< complete packet including header and payload
  unsigned int packlen;    //!< length of packet, typically paylen+12
	} RTPpacket_t;



typedef enum {
  SEI_BUFFERING_PERIOD = 0,
  SEI_PIC_TIMING,
  SEI_PAN_SCAN_RECT,
  SEI_FILLER_PAYLOAD,
  SEI_USER_DATA_REGISTERED_ITU_T_T35,
  SEI_USER_DATA_UNREGISTERED,
  SEI_RECOVERY_POINT,
  SEI_DEC_REF_PIC_MARKING_REPETITION,
  SEI_SPARE_PIC,
  SEI_SCENE_INFO,
  SEI_SUB_SEQ_INFO,
  SEI_SUB_SEQ_LAYER_CHARACTERISTICS,
  SEI_SUB_SEQ_CHARACTERISTICS,
  SEI_FULL_FRAME_FREEZE,
  SEI_FULL_FRAME_FREEZE_RELEASE,
  SEI_FULL_FRAME_SNAPSHOT,
  SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START,
  SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END,
  SEI_MOTION_CONSTRAINED_SLICE_GROUP_SET,
  SEI_FILM_GRAIN_CHARACTERISTICS,
  SEI_DEBLOCKING_FILTER_DISPLAY_PREFERENCE,
  SEI_STEREO_VIDEO_INFO,
  SEI_POST_FILTER_HINTS,
  SEI_TONE_MAPPING,
  SEI_SCALABILITY_INFO,
  SEI_SUB_PIC_SCALABLE_LAYER,
  SEI_NON_REQUIRED_LAYER_REP,
  SEI_PRIORITY_LAYER_INFO,
  SEI_LAYERS_NOT_PRESENT,
  SEI_LAYER_DEPENDENCY_CHANGE,
  SEI_SCALABLE_NESTING,
  SEI_BASE_LAYER_TEMPORAL_HRD,
  SEI_QUALITY_LAYER_INTEGRITY_CHECK,
  SEI_REDUNDANT_PIC_PROPERTY,
  SEI_TL0_DEP_REP_INDEX,
  SEI_TL_SWITCHING_POINT,
  SEI_PARALLEL_DECODING_INFO,
  SEI_MVC_SCALABLE_NESTING,
  SEI_VIEW_SCALABILITY_INFO,
  SEI_MULTIVIEW_SCENE_INFO,
  SEI_MULTIVIEW_ACQUISITION_INFO,
  SEI_NON_REQUIRED_VIEW_COMPONENT,
  SEI_VIEW_DEPENDENCY_CHANGE,
  SEI_OPERATION_POINTS_NOT_PRESENT,
  SEI_BASE_VIEW_TEMPORAL_HRD,
  SEI_FRAME_PACKING_ARRANGEMENT,
  SEI_GREEN_METADATA=56,

  SEI_MAX_ELEMENTS  //!< number of maximum syntax elements
	} SEI_type;

#define MAX_FN 256
// tone mapping information
#define MAX_CODED_BIT_DEPTH  12
#define MAX_SEI_BIT_DEPTH    12
#define MAX_NUM_PIVOTS     (1<<MAX_CODED_BIT_DEPTH)

#if ENABLE_OUTPUT_TONEMAPPING
typedef struct tone_mapping_struct_s {
  bool seiHasTone_mapping;
  unsigned int tone_map_repetition_period;
  uint8_t coded_data_bit_depth;
  uint8_t sei_bit_depth;
  int8_t model_id;
  unsigned int count;
  
  imgpel lut[1<<MAX_CODED_BIT_DEPTH];     //<! look up table for mapping the coded data value to output data value

  Bitstream *data;
  int payloadSize;
	} ToneMappingSEI;
#endif

//! Frame packing arrangement Information
typedef struct {
  unsigned int  frame_packing_arrangement_id;
  bool       frame_packing_arrangement_cancel_flag;
  uint8_t frame_packing_arrangement_type;
  bool       quincunx_sampling_flag;
  uint8_t content_interpretation_type;
  bool       spatial_flipping_flag;
  bool       frame0_flipped_flag;
  bool       field_views_flag;
  bool       current_frame_is_frame0_flag;
  bool       frame0_self_contained_flag;
  bool       frame1_self_contained_flag;
  uint8_t frame0_grid_position_x;
  uint8_t frame0_grid_position_y;
  uint8_t frame1_grid_position_x;
  uint8_t frame1_grid_position_y;
  uint8_t frame_packing_arrangement_reserved_byte;
  unsigned int  frame_packing_arrangement_repetition_period;
  bool       frame_packing_arrangement_extension_flag;
	} frame_packing_arrangement_information_struct;


//! Green metada Information
typedef struct {
  uint8_t  green_metadata_type;
  uint8_t  period_type;
  uint16_t num_seconds;
  uint16_t num_pictures;
  uint8_t percent_non_zero_macroblocks;
  uint8_t percent_intra_coded_macroblocks;
  uint8_t percent_six_tap_filtering;
  uint8_t percent_alpha_point_deblocking_instance;
  uint8_t xsd_metric_type;
  uint16_t xsd_metric_value;
	} Green_metadata_information_struct;



// Distortion data structure. Could be extended in the future to support other data 
typedef struct distortion_data {
  int      i4x4rd[4][4];         //! i4x4 rd cost
  distblk  i4x4  [4][4];         //! i4x4 cost
  distblk  i8x8  [2][2];         //! i8x8 cost
  int      i8x8rd[2][2];         //! i8x8 rd cost
  int      i16x16;
  int      i16x16rd;
  double   rd_cost;
	} DistortionData;

typedef struct lambda_params {
  double md;     //!< Mode decision Lambda
  double me[3];  //!< Motion Estimation Lambda
  int    mf[3];  //!< Integer formatted Motion Estimation Lambda
	} LambdaParams;

typedef struct level_quant_params {
  int   OffsetComp;
  int    ScaleComp;
  int InvScaleComp;
	} LevelQuantParams;

typedef struct quant_params {
  int AdaptRndWeight;
  int AdaptRndCrWeight;

  LevelQuantParams *****q_params_4x4;
  LevelQuantParams *****q_params_8x8;

  int *qp_per_matrix;
  int *qp_rem_matrix;

  int16_t **OffsetList4x4input;
  int16_t **OffsetList8x8input;
  int16_t ***OffsetList4x4;
  int16_t ***OffsetList8x8;
	} QuantParameters;

typedef struct quant_methods {
  BLOCK_COORD block_y; 
  BLOCK_COORD block_x;
  int   qp; 
  int*  ACLevel;
  int*  ACRun;
  int **fadjust; 
  LevelQuantParams **q_params;
  int *coeff_cost;
  const uint8_t(*pos_scan)[2];
  const uint8_t *c_cost;
  char type;
	} QuantMethods;

typedef struct {
  unsigned int tone_map_id;
  uint8_t tone_map_cancel_flag;
  unsigned int tone_map_repetition_period;
  uint8_t coded_data_bit_depth;
  uint8_t sei_bit_depth;
  unsigned int  model_id;
  // variables for model 0
  int min_value;
  int max_value;
  // variables for model 1
  int sigmoid_midpoint;
  int sigmoid_width;
  // variables for model 2
  int start_of_coded_interval[1<<MAX_SEI_BIT_DEPTH];
  // variables for model 3
  int num_pivots;
  int coded_pivot_value[MAX_NUM_PIVOTS];
  int sei_pivot_value[MAX_NUM_PIVOTS];
	} tone_mapping_struct_tmp;


// -------------------------------------------------------------------------------------------------------------------
class CDecoderH264_MemoryMgr {
private:
	HANDLE hHeap;
	uint32_t heapSize;

public:
	CDecoderH264_MemoryMgr(uint32_t heapsize=10000000UL);
	~CDecoderH264_MemoryMgr();

	inline void *H264CALLOC(size_t a,size_t  b);
	inline void *H264MALLOC(size_t a);
	inline void H264FREE(void *a);

	void error(const char *text);

	int get_mem2Ddist(DistortionData ***array2D, int dim0, int dim1);

	int get_mem2Dlm (LambdaParams ***array2D, int dim0, int dim1);
	int get_mem2Dolm(LambdaParams ***array2D, int dim0, int dim1, int offset);

	int get_mem2Dmp (PicMotionParams ***array2D, int dim0, int dim1);
	int get_mem3Dmp (PicMotionParams ****array3D, int dim0, int dim1, int dim2);

	int get_mem2Dquant(LevelQuantParams ***array2D, int dim0, int dim1);
	int get_mem3Dquant(LevelQuantParams ****array3D, int dim0, int dim1, int dim2);
	int get_mem4Dquant(LevelQuantParams *****array4D, int dim0, int dim1, int dim2, int dim3);
	int get_mem5Dquant(LevelQuantParams ******array5D, int dim0, int dim1, int dim2, int dim3, int dim4);

	int get_mem2Dmv(MotionVector ***array2D, int dim0, int dim1);
	int get_mem3Dmv(MotionVector ****array3D, int dim0, int dim1, int dim2);
	int get_mem4Dmv(MotionVector *****array4D, int dim0, int dim1, int dim2, int dim3);
	int get_mem5Dmv(MotionVector ******array5D, int dim0, int dim1, int dim2, int dim3, int dim4);
	int get_mem6Dmv(MotionVector *******array6D, int dim0, int dim1, int dim2, int dim3, int dim4, int dim5);
	int get_mem7Dmv(MotionVector ********array7D, int dim0, int dim1, int dim2, int dim3, int dim4, int dim5, int dim6);

	uint8_t **new_mem2D(int dim0, int dim1);
	int get_mem2D(uint8_t ***array2D, int dim0, int dim1);
	int get_mem3D(uint8_t ****array3D, int dim0, int dim1, int dim2);
	int get_mem4D(uint8_t *****array4D, int dim0, int dim1, int dim2, int dim3);

	int **new_mem2Dint(int dim0, int dim1);
	int get_mem2Dint(int ***array2D, int dim0, int dim1);
	int get_mem2Dint_pad(int ***array2D, int dim0, int dim1, int iPadY, int iPadX);
	int get_mem2Dint64(INT64T ***array2D, int dim0, int dim1);
	int get_mem3Dint(int ****array3D, int dim0, int dim1, int dim2);
	int get_mem3Dint64(INT64T ****array3D, int dim0, int dim1, int dim2);
	int get_mem4Dint(int *****array4D, int dim0, int dim1, int dim2, int dim3);
	int get_mem4Dint64(INT64T *****array4D, int dim0, int dim1, int dim2, int dim3);
	int get_mem5Dint(int ******array5D, int dim0, int dim1, int dim2, int dim3, int dim4);

	uint16_t** new_mem2Duint16(int dim0, int dim1);
	int get_mem2Duint16(uint16_t ***array2D, int dim0, int dim1);
	int get_mem3Duint16(uint16_t ****array3D,int dim0, int dim1, int dim2);
	int get_mem4Duint16(uint16_t *****array4D, int dim0, int dim1, int dim2, int dim3);

	int get_mem2Ddistblk(distblk ***array2D, int dim0, int dim1);
	int get_mem3Ddistblk(distblk ****array3D, int dim0, int dim1, int dim2);
	int get_mem4Ddistblk(distblk *****array4D, int dim0, int dim1, int dim2, int dim3);

	int get_mem2Dshort(int16_t ***array2D, int dim0, int dim1);
	int get_mem3Dshort(int16_t ****array3D, int dim0, int dim1, int dim2);
	int get_mem4Dshort(int16_t *****array4D, int dim0, int dim1, int dim2, int dim3);
	int get_mem5Dshort(int16_t ******array5D, int dim0, int dim1, int dim2, int dim3, int dim4);
	int get_mem6Dshort(int16_t *******array6D, int dim0, int dim1, int dim2, int dim3, int dim4, int dim5);
	int get_mem7Dshort(int16_t ********array7D, int dim0, int dim1, int dim2, int dim3, int dim4, int dim5, int dim6);

	int get_mem1Dpel(imgpel **array2D, int dim0);
	int get_mem2Dpel(imgpel ***array2D, int dim0, int dim1);
	int get_mem2Dpel_pad(imgpel ***array2D, int dim0, int dim1, int iPadY, int iPadX);

	int get_mem3Dpel   (imgpel ****array3D, int dim0, int dim1, int dim2);
	int get_mem3Dpel_pad(imgpel ****array3D, int dim0, int dim1, int dim2, int iPadY, int iPadX);
	int get_mem4Dpel   (imgpel *****array4D, int dim0, int dim1, int dim2, int dim3);
	int get_mem4Dpel_pad(imgpel *****array4D, int dim0, int dim1, int dim2, int dim3, int iPadY, int iPadX);
	int get_mem5Dpel   (imgpel ******array5D, int dim0, int dim1, int dim2, int dim3, int dim4);
	int get_mem5Dpel_pad(imgpel ******array5D, int dim0, int dim1, int dim2, int dim3, int dim4, int iPadY, int iPadX);
	int get_mem2Ddouble(double ***array2D, int dim0, int dim1);

	int get_mem1Dodouble(double **array1D, int dim0, int offset);
	int get_mem2Dodouble(double ***array2D, int dim0, int dim1, int offset);
	int get_mem3Dodouble(double ****array2D, int dim0, int dim1, int dim2, int offset);

	int get_mem2Doint(int ***array2D, int dim0, int dim1, int offset);
	int get_mem3Doint(int ****array3D, int dim0, int dim1, int dim2, int offset);

	int get_mem2Dwp(WPParams ***array2D, int dim0, int dim1);

	int get_offset_mem2Dshort(int16_t ***array2D, int rows, int columns, int offset_y, int offset_x);

	void free_offset_mem2Dshort(int16_t **array2D, int columns, int offset_x, int offset_y);

	void free_mem2Ddist(DistortionData **array2D);

	void free_mem2Dlm  (LambdaParams **array2D);
	void free_mem2Dolm (LambdaParams **array2D, int offset);

	void free_mem2Dmp  (PicMotionParams    **array2D);
	void free_mem3Dmp  (PicMotionParams   ***array2D);

	void free_mem2Dquant(LevelQuantParams    **array2D);
	void free_mem3Dquant(LevelQuantParams   ***array2D);
	void free_mem4Dquant(LevelQuantParams  ****array2D);
	void free_mem5Dquant(LevelQuantParams *****array2D);

	void free_mem2Dmv  (MotionVector     **array2D);
	void free_mem3Dmv  (MotionVector    ***array2D);
	void free_mem4Dmv  (MotionVector   ****array2D);
	void free_mem5Dmv  (MotionVector  *****array2D);
	void free_mem6Dmv  (MotionVector ******array2D);
	void free_mem7Dmv  (MotionVector *******array7D);

	int get_mem2D_spp(StorablePicturePtr  ***array3D, int dim0, int dim1);
	int get_mem3D_spp(StorablePicturePtr ****array3D, int dim0, int dim1, int dim2);

	void free_mem2D_spp(StorablePicturePtr  **array2D);
	void free_mem3D_spp(StorablePicturePtr ***array2D);

	void free_mem2D    (uint8_t      **array2D);
	void free_mem3D    (uint8_t     ***array3D);
	void free_mem4D    (uint8_t    ****array4D);

	void free_mem2Dint (int       **array2D);
	void free_mem2Dint_pad(int **array2D, int iPadY, int iPadX);
	void free_mem3Dint (int      ***array3D);
	void free_mem4Dint (int     ****array4D);
	void free_mem5Dint (int    *****array5D);

	void free_mem2Duint16(uint16_t **array2D);
	void free_mem3Duint16(uint16_t ***array3D);
	void free_mem4Duint16(uint16_t ****array4D);

	void free_mem2Dint64(INT64T     **array2D);
	void free_mem3Dint64(INT64T    ***array3D);
	void free_mem4Dint64(INT64T     ****array4D);

	void free_mem2Ddistblk(distblk     **array2D);
	void free_mem3Ddistblk(distblk    ***array3D);
	void free_mem4Ddistblk(distblk     ****array4D);

	void free_mem2Dshort(int16_t      **array2D);
	void free_mem3Dshort(int16_t     ***array3D);
	void free_mem4Dshort(int16_t    ****array4D);
	void free_mem5Dshort(int16_t   *****array5D);
	void free_mem6Dshort(int16_t  ******array6D);
	void free_mem7Dshort(int16_t *******array7D);

	void free_mem1Dpel   (imgpel     *array1D);
	void free_mem2Dpel   (imgpel    **array2D);
	void free_mem2Dpel_pad(imgpel **array2D, int iPadY, int iPadX);
	void free_mem3Dpel   (imgpel   ***array3D);
	void free_mem3Dpel_pad(imgpel ***array3D, int iDim12, int iPadY, int iPadX);
	void free_mem4Dpel   (imgpel  ****array4D);
	void free_mem4Dpel_pad(imgpel  ****array4D, int iFrames, int iPadY, int iPadX);
	void free_mem5Dpel   (imgpel *****array5D);
	void free_mem5Dpel_pad(imgpel *****array5D, int iFrames, int iPadY, int iPadX);
	void free_mem2Ddouble(double **array2D);
	void free_mem3Ddouble(double ***array3D);

	void free_mem1Dodouble(double  *array1D, int offset);
	void free_mem2Dodouble(double **array2D, int offset);
	void free_mem3Dodouble(double ***array3D, int rows, int columns, int offset);
	void free_mem2Doint  (int **array2D, int offset);
	void free_mem3Doint  (int ***array3D, int rows, int columns, int offset);

	int  init_top_bot_planes(imgpel **imgFrame, int height, imgpel ***imgTopField, imgpel ***imgBotField);
	void free_top_bot_planes(imgpel **imgTopField, imgpel **imgBotField);

	void free_mem2Dwp(WPParams **array2D);

	void copy2DImage(imgpel **dst_img, imgpel **src_img, int size_x, int size_y);
	void no_mem_exit(const char *where);
	int  malloc_mem2Dpel_2SLayers(imgpel ***buf0, int imgtype0, imgpel ***buf1, int imgtype1, int height, int width);
	int  malloc_mem3Dpel_2SLayers(imgpel ****buf0, int imgtype0, imgpel ****buf1, int imgtype1, int frames, int height, int width);

	void free_mem2Dpel_2SLayers(imgpel ***buf0, imgpel ***buf1);
	void free_mem3Dpel_2SLayers(imgpel ****buf0, imgpel ****buf1);

	inline void* sse_malloc(size_t nitems);
	inline void* sse_calloc(size_t nitems, size_t size);
	inline void free_pointer(void *);
	inline void sse_free(void *);
	};


// -------------------------------------------------------------------------------------------------------------------
class CDecoderH264 {
public:
  InputParameters   *p_Inp;          //!< Input Parameters
  CVideoParameters  *p_Vid;          //!< Image Parameters
  int32_t /*64*/    bufferSize;     //!< buffersize for tiff reads (not currently supported)
  int               UsedBits;      // for internal statistics, is adjusted by read_se_v, read_ue_v, read_u_1
  FILE              *p_trace;        //!< Trace file
  int               bitcounter;
	jmp_buf						mark;  

	static InputParameters cfgparams;

private:
	static const char yuv_types[4][6];

	//! single scan pattern
	static const uint8_t SNGL_SCAN[16][2];
	//! field scan pattern
	static const uint8_t FIELD_SCAN[16][2];
	//! used to control block sizes : Not used/16x16/16x8/8x16/8x8/8x4/4x8/4x4
	static const uint8_t BLOCK_STEP[8][2];
	//! single scan pattern
	static const uint8_t SNGL_SCAN8x8[64][2];
	//! field scan pattern
	static const uint8_t FIELD_SCAN8x8[64][2];   // 8x8
	//! single scan pattern
	static const uint8_t SCAN_YUV422[8][2];
	static const uint8_t cbp_blk_chroma[8][4];
	static const uint8_t cofuv_blk_x[3][8][4];
	static const uint8_t cofuv_blk_y[3][8][4];
	static const uint8_t ZZ_SCAN[16];
	static const uint8_t ZZ_SCAN8[64];
	//! gives CBP value from codeword number, both for intra and inter
	static const uint8_t NCBP[2][48][2];
	//! for the linfo_levrun_inter routine
	static const uint8_t NTAB1[4][8][2];
	static const uint8_t LEVRUN1[16];
	static const uint8_t NTAB2[4][8][2];
	//! for the linfo_levrun__c2x2 routine
	static const uint8_t LEVRUN3[4];
	static const uint8_t NTAB3[2][2][2];
	// exported variables
	static const int16_t dequant_coef8[6][8][8];
	//! Dequantization coefficients
	static const int16_t dequant_coef[6][4][4];
	static const int16_t quant_coef[6][4][4];
	// SP decoding parameter(EQ. 8-425)
	static const int8_t A[4][4];
	static const uint8_t assignSE2partition[][SE_MAX_ELEMENTS];

/* Range table for  LPS */
	static const uint8_t rLPS_table_64x4[64][4];
	static const uint8_t AC_next_state_MPS_64[64];
	static const uint8_t AC_next_state_LPS_64[64];
	static const uint8_t renorm_table_32[32];

	static const uint8_t QP_SCALE_CR[52];

//! look up tables for FRExt_chroma support
	static const uint8_t subblk_offset_x[3][8][4];
	static const uint8_t subblk_offset_y[3][8][4];
	static const uint8_t decode_block_scan[16];

// The tables actually used have been "hand optimized" though(by Anthony Joch). So, the
// table values might be a little different to formula-generated values. Also, the first
// few values of both tables is set to zero to force the filter off at low qp’s
	static const uint8_t ALPHA_TABLE[MAX_QP+1];
	static const uint8_t BETA_TABLE[MAX_QP+1];
	static const uint8_t CLIP_TAB[MAX_QP+1][5];
	static const int8_t chroma_edge[2][4][4];		//[dir][edge][yuv_format]
	static const int8_t pelnum_cr[2][4];  //[dir:0=vert, 1=hor.][yuv_format]

	static const int8_t maxpos       [];
	static const int8_t c1isdc       [];
	static const int8_t type2ctx_bcbp[];
	static const int8_t type2ctx_map [];
	static const int8_t type2ctx_last[];
	static const int8_t type2ctx_one [];
	static const int8_t type2ctx_abs [];
	static const int8_t max_c2       [];

	static const uint8_t  pos2ctx_map8x8 [];
	static const uint8_t  pos2ctx_map8x4 [];
	static const uint8_t  pos2ctx_map4x4 [];
	static const uint8_t  pos2ctx_map2x4c[];
	static const uint8_t  pos2ctx_map4x4c[];
	static const uint8_t* pos2ctx_map    [];
	static const uint8_t  pos2ctx_map8x8i[];
	static const uint8_t  pos2ctx_map8x4i[];
	static const uint8_t  pos2ctx_map4x8i[];
	static const uint8_t* pos2ctx_map_int[];
	static const uint8_t  pos2ctx_last8x8 [];
	static const uint8_t  pos2ctx_last8x4 [];
	static const uint8_t  pos2ctx_last4x4 [];
	static const uint8_t  pos2ctx_last2x4c[];
	static const uint8_t  pos2ctx_last4x4c[];
	static const uint8_t* pos2ctx_last    [];

	static const int8_t INIT_MB_TYPE_I[1][3][11][2];
	static const int8_t INIT_MB_TYPE_P[3][3][11][2];
	static const int8_t INIT_B8_TYPE_I[1][2][9][2];
	static const int8_t INIT_B8_TYPE_P[3][2][9][2];
	static const int8_t INIT_MV_RES_I[1][2][10][2];
	static const int8_t INIT_MV_RES_P[3][2][10][2];

	static const int8_t INIT_REF_NO_I[1][2][6][2];
	static const int8_t INIT_REF_NO_P[3][2][6][2];
	static const int8_t INIT_TRANSFORM_SIZE_I[1][1][3][2];
	static const int8_t INIT_TRANSFORM_SIZE_P[3][1][3][2];
	static const int8_t INIT_DELTA_QP_I[1][1][4][2];
	static const int8_t INIT_DELTA_QP_P[3][1][4][2];

	static const int8_t INIT_MB_AFF_I[1][1][4][2];
	static const int8_t INIT_MB_AFF_P[3][1][4][2];
	static const int8_t INIT_IPR_I[1][1][2][2];

	static const int8_t INIT_IPR_P[3][1][2][2];
	static const int8_t INIT_CIPR_I[1][1][4][2];
	static const int8_t INIT_CIPR_P[3][1][4][2];

	static const int8_t INIT_CBP_I[1][3][4][2];
	static const int8_t INIT_CBP_P[3][3][4][2];
	static const int8_t INIT_BCBP_I[1][22][4][2];
	static const int8_t INIT_BCBP_P[3][22][4][2];
	static const int8_t INIT_MAP_I[1][22][15][2];
	static const int8_t INIT_MAP_P[3][22][15][2];
	static const int8_t INIT_LAST_I[1][22][15][2];
	static const int8_t INIT_LAST_P[3][22][15][2];
	static const int8_t INIT_ONE_I[1][22][5][2];
	static const int8_t INIT_ONE_P[3][22][5][2];
	static const int8_t INIT_ABS_I[1][22][5][2];
	static const int8_t INIT_ABS_P[3][22][5][2];

#if ENABLE_FIELD_CTX
	static const int8_t INIT_FLD_MAP_I[1][22][15][2];
	static const int8_t INIT_FLD_MAP_P[3][22][15][2];
	static const int8_t INIT_FLD_LAST_I[1][22][15][2];
	static const int8_t INIT_FLD_LAST_P[3][22][15][2];
#endif

	static const uint8_t uv_div[2][4];
	static const int8_t ICBPTAB[6];

	static const int16_t quant_intra_default[16];
	static const int16_t quant_inter_default[16];
	static const int16_t quant8_intra_default[64];
	static const int16_t quant8_inter_default[64];
	static const int16_t quant_org[16];
	static const int16_t quant8_org[64];
	static const int incVlc[];
	static const int8_t SubWidthC [4];
	static const int8_t SubHeightC[4];

	static const uint8_t lentab[3][4][17];
	static const uint8_t codtab[3][4][17];

	LARGE_INTEGER freq;
	static Mapping Map[];

#define ET_SIZE 300      //!< size of error text buffer
 	static char errortext[ET_SIZE]; //!< buffer for error message for exit with error()

	static const int8_t COEF[6];
	static const int8_t mv_mul;
	static const INT64T po2[64];


public:
	CDecoderH264(uint32_t heapsize=10000000UL);
	CDecoderH264(const char *n,ColorModel c=CM_RGB,uint32_t heapsize=10000000UL);
	CDecoderH264(CRTSPClientSocket *s,ColorModel c=CM_RGB,uint32_t heapsize=10000000UL);
	CDecoderH264(ColorModel c,uint32_t hsize);
	~CDecoderH264();

public:
	int OpenDecoder(InputParameters *p_Inp=NULL);
	int DecodeOneFrame(DecodedPicList **ppDecPic);
	int FinitDecoder(DecodedPicList **ppDecPicList);
	int CloseDecoder();
	int SetOptsDecoder(DecSet_t *pDecOpts);
	int SetVideoBuffer(uint8_t *);
	uint8_t *GetVideoBuffer() { return p_Inp->outbuf; }
	uint32_t GetVideoBufferSize() { return p_Inp->outbufSize; }

protected:
	void preconstruct(uint32_t);
	void init(CVideoParameters *p_Vid);
	void exit_picture(CVideoParameters *p_Vid, StorablePicture **dec_picture);
	int  decode_one_frame();
	static bool testEndian(void);

#if TRACE
	void dectracebitcnt(int count);
	void tracebits (const char *trace_str, int len, int info, int value1);
	void tracebits2(const char *trace_str, int len, int info);
	void trace_info(CSyntaxElement *currSE, const char *description_str, int value1);
#endif

	void buildPredRegionYUV(CVideoParameters *p_Vid, int *mv, BLOCK_COORD x, BLOCK_COORD y, imgpel *predMB);
	void buildPredblockRegionYUV(CVideoParameters *p_Vid, int *mv, BLOCK_COORD x, BLOCK_COORD y, imgpel *predMB, int list, int current_mb_nr);
	void copyPredMB(int currYBlockNum, imgpel *predMB, frame *recfr, PIXEL_COORD picSizeX, int regionSize);
	int edgeDistortion(int predBlocks[], int currYBlockNum, imgpel *predMB, imgpel *recY, int picSizeX, int regionSize);

	void reorder_lists(CSlice *currSlice);
	void init_cur_imgy(CVideoParameters *p_Vid,CSlice *currSlice,ColorPlane pl);
	void init_cur_imgy(CSlice *currSlice, CVideoParameters *p_Vid);
	void intra_chroma_DC_single(imgpel **curr_img, bool up_avail, bool left_avail, PixelPos up, PixelPos left, 
																	 BLOCK_COORD blk_x, BLOCK_COORD blk_y, int *pred, int direction);
	void intra_chroma_DC_all(imgpel **curr_img, bool up_avail, bool left_avail, PixelPos up, PixelPos left, 
																BLOCK_COORD blk_x, BLOCK_COORD blk_y, int *pred);
	void intrapred_chroma_dc(CMacroblock *currMB);

	static inline int compare_pic_by_pic_num_desc(const void *arg1, const void *arg2);
	static inline int compare_pic_by_lt_pic_num_asc(const void *arg1, const void *arg2);
	static inline int compare_pic_by_poc_asc(const void *arg1, const void *arg2);
	static inline int compare_pic_by_poc_desc(const void *arg1, const void *arg2);
	static inline int compare_fs_by_lt_pic_idx_asc(const void *arg1, const void *arg2);
	static inline int compare_fs_by_poc_asc(const void *arg1, const void *arg2);
	static inline int compare_fs_by_poc_desc(const void *arg1, const void *arg2);
	static inline int compare_fs_by_frame_num_desc(const void *arg1, const void *arg2);

	void CopyImgData(imgpel **inputY, imgpel ***inputUV, imgpel **outputY, imgpel ***outputUV, 
                        PIXEL_COORD img_width, PIXEL_COORD img_height, PIXEL_COORD img_width_cr, PIXEL_COORD img_height_cr);
	StorablePicture* get_last_ref_pic_from_dpb(DecodedPictureBuffer *p_Dpb);
	void copy_to_conceal(StorablePicture *src, StorablePicture *dst, CVideoParameters *p_Vid);
	void copy_prev_pic_to_concealed_pic(StorablePicture *picture, DecodedPictureBuffer *p_Dpb);
	void update_ref_list_for_concealment(DecodedPictureBuffer *p_Dpb);
	void update_ref_list(DecodedPictureBuffer *p_Dpb);
	StorablePicture *get_pic_from_dpb(DecodedPictureBuffer *p_Dpb, int missingpoc, unsigned int *pos);
	void add_node(CVideoParameters *p_Vid, struct concealment_node *concealment_new);
	void delete_node(CVideoParameters *p_Vid, struct concealment_node *ptr);
	void delete_list(CVideoParameters *p_Vid, struct concealment_node *ptr);

	int fmo_init(CVideoParameters *p_Vid, CSlice *pSlice);
	int FmoFinit(CVideoParameters *p_Vid);

	int FmoGetNumberOfSliceGroup(CVideoParameters *p_Vid);
	int FmoGetLastMBOfPicture   (CVideoParameters *p_Vid);
	int FmoGetLastMBInSliceGroup(CVideoParameters *p_Vid, int SliceGroup);
	int FmoGetSliceGroupId      (CVideoParameters *p_Vid, int mb);
	int FmoGenerateMbToSliceGroupMap(CVideoParameters *p_Vid, CSlice *pSlice);
	int FmoGenerateMapUnitToSliceGroupMap(CVideoParameters *p_Vid, CSlice *currSlice);

	void ref_pic_list_reordering(CSlice *currSlice);
	static void reset_wp_params(CSlice *currSlice);
	void pred_weight_table(CSlice *currSlice);

	static inline void reset_mbs(CMacroblock *currMB);
	void init_picture(CVideoParameters *p_Vid, CSlice *currSlice, InputParameters *p_Inp);

	void alloc_video_params(CVideoParameters **p_Vid);
	void alloc_params(InputParameters **p_Inp);
	void free_img(CVideoParameters *p_Vid);

	void update_mbaff_macroblock_data(imgpel **cur_img, imgpel(*temp)[16], int x0, PIXEL_COORD width, PIXEL_COORD height);
	void MbAffPostProc(CVideoParameters *p_Vid);
	void fill_wp_params(CSlice *currSlice);

	void init_picture_decoding(CVideoParameters *p_Vid);
	static void Error_tracking(CVideoParameters *p_Vid, CSlice *currSlice);
	void CopyPOC(CSlice *pSlice0, CSlice *currSlice);
	void buffer2img(imgpel** imgX, uint8_t* buf, PIXEL_COORD size_x, PIXEL_COORD size_y, uint8_t symbol_size_in_bytes);
	static INT64T compute_SSE(imgpel **imgRef, imgpel **imgSrc, int xRef, int xSrc, PIXEL_COORD ySize, PIXEL_COORD xSize);

	inline void LowPassForIntra8x8Pred(imgpel *PredPel, bool block_up_left, bool block_up, bool block_left);
	inline void LowPassForIntra8x8PredHor(imgpel *PredPel, bool block_up_left, bool block_up, bool block_left);
	inline void LowPassForIntra8x8PredVer(imgpel *PredPel, bool block_up_left, bool block_up, bool block_left);

	void reset_dpb(CVideoParameters *p_Vid, DecodedPictureBuffer *p_Dpb);
	void init_frext(CVideoParameters *p_Vid);
	void Report(CVideoParameters *p_Vid);
	void report_stats_on_error(void);

	void set_loop_filter_functions_mbaff(CVideoParameters *p_Vid);
	void free_slice(CSlice *currSlice);

	void get_strength_ver_MBAff(uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, uint8_t mvlimit, StorablePicture *p);
	void get_strength_hor_MBAff(uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, uint8_t mvlimit, StorablePicture *p);
	void edge_loop_luma_ver_MBAff(ColorPlane pl, imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge);
	void edge_loop_luma_hor_MBAff(ColorPlane pl, imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, StorablePicture *p);
	void edge_loop_chroma_ver_MBAff(imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, int uv, StorablePicture *p);
	void edge_loop_chroma_hor_MBAff(imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, int uv, StorablePicture *p);
	void get_db_strength_mbaff(CVideoParameters *p_Vid, StorablePicture *p, int MbQAddr);
	void perform_db_mbaff(CVideoParameters *p_Vid, StorablePicture *p, int MbQAddr);
	void set_loop_filter_functions_normal(CVideoParameters *p_Vid);
	void get_strength_ver(CMacroblock *MbQ, uint8_t edge, uint8_t mvlimit, StorablePicture *p);
	void get_strength_hor(CMacroblock *MbQ, uint8_t edge, uint8_t mvlimit, StorablePicture *p);
	void luma_ver_deblock_strong(imgpel **cur_img, PIXEL_COORD pos_x1, int Alpha, int Beta);
	void luma_ver_deblock_normal(imgpel **cur_img, PIXEL_COORD pos_x1, int Alpha, int Beta, int C0, imgpel max_imgpel_value);
	void edge_loop_luma_ver(ColorPlane pl, imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge);
	void luma_hor_deblock_strong(imgpel *imgP, imgpel *imgQ, uint8_t width, int Alpha, int Beta);
	void luma_hor_deblock_normal(imgpel *imgP, imgpel *imgQ, PIXEL_COORD width, int Alpha, int Beta, int C0, imgpel max_imgpel_value);
	void edge_loop_luma_hor(ColorPlane pl, imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, StorablePicture *p);
	void edge_loop_chroma_ver(imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, int uv, StorablePicture *p);
	void edge_loop_chroma_hor(imgpel** Img, uint8_t *Strength, CMacroblock *MbQ, uint8_t edge, int uv, StorablePicture *p);
	void perform_db_ind_normal(CMacroblock *MbQ, StorablePicture *p);
	void perform_db_dep_normal(CMacroblock   *MbQ, StorablePicture *p);
	void perform_db_normal(CVideoParameters *p_Vid, StorablePicture *p, int MbQAddr);
	void get_db_strength_normal(CVideoParameters *p_Vid, StorablePicture *p, int MbQAddr, int *piCnt);
	void deblock_normal(CVideoParameters *p_Vid, StorablePicture *p);

	void init_neighbors(CVideoParameters *p_Vid);

	void DeblockMb(CVideoParameters *p_Vid, StorablePicture *p, int MbQAddr);
	void get_db_strength(CVideoParameters *p_Vid, StorablePicture *p, int MbQAddr);
	void perform_db(CVideoParameters *p_Vid, StorablePicture *p, int MbQAddr);

	int8_t readRefPictureIdx_VLC(CMacroblock *currMB, CSyntaxElement *currSE, DataPartition *dP, int8_t b8mode, int list);
	int8_t readRefPictureIdx_FLC(CMacroblock *currMB, CSyntaxElement *currSE, DataPartition *dP, int8_t b8mode, int list);
	int8_t readRefPictureIdx_Null(CMacroblock *currMB, CSyntaxElement *currSE, DataPartition *dP, int8_t b8mode, int list);
	void prepareListforRefIdx(CMacroblock *currMB, CSyntaxElement *currSE, DataPartition *dP, int num_ref_idx_active, int refidx_present);
	static void set_chroma_qp(CMacroblock* currMB);
	void read_delta_quant(CSyntaxElement *currSE, DataPartition *dP, CMacroblock *currMB, const uint8_t *partMap, int type);
	void readMBRefPictureIdx(CSyntaxElement *currSE, DataPartition *dP, CMacroblock *currMB, PicMotionParams **mv_info, int list, int step_v0, int step_h0);
	void readMBMotionVectors(CSyntaxElement *currSE, DataPartition *dP, CMacroblock *currMB, int list, int step_h0, int step_v0);

	void invScaleCoeff(CMacroblock *currMB, int level, int run, int8_t qp_per, int i, int j, int i0, int j0, 
									 int coef_ctr, const uint8_t(*pos_scan4x4)[2], int(*InvLevelScale4x4)[4]);
	inline void setup_mb_pos_info(CMacroblock *currMB);
	void interpret_mb_mode_P(CMacroblock *currMB);
	void interpret_mb_mode_I(CMacroblock *currMB);
	void interpret_mb_mode_B(CMacroblock *currMB);
	void interpret_mb_mode_SI(CMacroblock *currMB);

	void setup_read_macroblock(CSlice *currSlice);

	void read_motion_info_from_NAL_p_slice(CMacroblock *currMB);
	void read_motion_info_from_NAL_b_slice(CMacroblock *currMB);

	int decode_one_component_i_slice(CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, 
																		StorablePicture *dec_picture);
	int decode_one_component_p_slice(CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, 
																		StorablePicture *dec_picture);
	int decode_one_component_sp_slice(CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
	int decode_one_component_b_slice(CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
	bool mb_is_available(int mbAddr, CMacroblock *currMB);
	void CheckAvailabilityOfNeighbors(CMacroblock *currMB);

	void read_ipred_8x8_modes(CMacroblock *currMB);
	void read_ipred_4x4_modes(CMacroblock *currMB);
	void read_ipred_modes(CMacroblock *currMB);
	void read_ipred_4x4_modes_mbaff(CMacroblock *currMB);
	void read_ipred_8x8_modes_mbaff(CMacroblock *currMB);

	static inline void update_neighbor_mvs(PicMotionParams **motion, const PicMotionParams *mv_info, int i4);
	static void set_chroma_vector(CMacroblock *currMB);

	inline void reset_mv_info(PicMotionParams *mv_info, int slice_no);
	void concealIPCMcoeffs(CMacroblock *currMB);
	void init_macroblock(CMacroblock *currMB);
	void init_macroblock_direct(CMacroblock *currMB);
	void init_macroblock_basic(CMacroblock *currMB);
	static inline void reset_mv_info_list(PicMotionParams *mv_info, int list, int slice_no);

	static inline void update_pixel_pos8(PixelPos *pos_block, const PixelPos *pos_mb, int pos);
	void read_skip_macroblock(CMacroblock *currMB);
	inline void field_flag_inference(CMacroblock *currMB);
	void skip_macroblock(CMacroblock *currMB);
	void read_intra_macroblock(CMacroblock *currMB);
	inline void reset_coeffs(CMacroblock *currMB);
	inline void SetB8Mode(CMacroblock* currMB, int value, int i);
	void read_IPCM_coeffs_from_NAL(CSlice *currSlice, struct datapartition_dec *dP);
	void init_decoding_engine_IPCM(CSlice *currSlice);
	void read_i_pcm_macroblock(CMacroblock *currMB, const uint8_t *partMap);
	void read_inter_macroblock(CMacroblock *currMB);
	void read_intra4x4_macroblock_cabac(CMacroblock *currMB, const uint8_t *partMap);
	void read_intra4x4_macroblock_cavlc(CMacroblock *currMB, const uint8_t *partMap);
	void read_one_macroblock_i_slice_cavlc(CMacroblock *currMB);
	void read_P8x8_macroblock(CMacroblock *currMB, DataPartition *dP, CSyntaxElement *currSE);
	void read_one_macroblock_i_slice_cabac(CMacroblock *currMB);
	void read_one_macroblock_p_slice_cavlc(CMacroblock *currMB);
	void read_one_macroblock_p_slice_cabac(CMacroblock *currMB);

	void CheckAvailabilityOfNeighborsNormal(CMacroblock *currMB);
	void CheckAvailabilityOfNeighborsMBAFF(CMacroblock *currMB);

	void getAffNeighbour(CMacroblock *currMB, BLOCK_COORD xN, BLOCK_COORD yN, uint32_t mb_size[2], PixelPos *pix);
	void getNonAffNeighbour(CMacroblock *currMB, BLOCK_COORD xN, BLOCK_COORD yN, uint32_t mb_size[2], PixelPos *pix);

	void get_mb_block_pos_normal(BlockPos *PicPos, int mb_addr, BLOCK_COORD *x, BLOCK_COORD *y);
	void get_mb_block_pos_mbaff(BlockPos *PicPos, int mb_addr, BLOCK_COORD *x, BLOCK_COORD *y);
	void get_mb_pos(CVideoParameters *p_Vid, int mb_addr, uint32_t mb_size[2], BLOCK_COORD *x, BLOCK_COORD *y);

	void read_coeff_4x4_CAVLC(CMacroblock *currMB, CAVLCBlockTypes block_type,
                           int i, int j, int8_t levarr[16], int8_t runarr[16],
                           int8_t *number_coefficients);
	void read_coeff_4x4_CAVLC_444(CMacroblock *currMB, CAVLCBlockTypes block_type,
                               int i, int j, int8_t levarr[16], int8_t runarr[16],
                               int8_t *number_coefficients);

	void read_one_macroblock_b_slice_cabac(CMacroblock *currMB);

	void init_motion_vector_prediction(CMacroblock *currMB, bool mb_aff_frame_flag);
	bool get_colocated_info_8x8(CMacroblock *currMB, StorablePicture *list1, int i, int j);
	bool get_colocated_info_4x4(CMacroblock *currMB, StorablePicture *list1, int i, int j);

	void set_read_CBP_and_coeffs_cabac(CSlice *currSlice);
	void set_read_CBP_and_coeffs_cavlc(CSlice *currSlice);
	void set_read_comp_coeff_cavlc(CMacroblock *currMB);
	void set_read_comp_coeff_cabac(CMacroblock *currMB);

	static void update_direct_types(CSlice *currSlice);

	void read_one_macroblock_b_slice_cavlc(CMacroblock *currMB);

	void check_dp_neighbors(CMacroblock *currMB);

	static void dump_dpb(DecodedPictureBuffer *p_Dpb);
	int getDpbSize(CVideoParameters *p_Vid, seq_parameter_set_rbsp_t *active_sps);

	void check_num_ref(DecodedPictureBuffer *p_Dpb);

	void update_direct_mv_info_spatial_4x4(CMacroblock *currMB);
	void update_direct_mv_info_spatial_8x8(CMacroblock *currMB);

	void weighted_bi_prediction(imgpel *mb_pred, imgpel *block_l0, imgpel *block_l1, 
                                   BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
                                   int wp_scale_l0, int wp_scale_l1, int wp_offset, 
                                   int weight_denom, int color_clip);
	void weighted_mc_prediction(imgpel **mb_pred, imgpel **block, 
                                   BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
                                   PIXEL_COORD ioff, int wp_scale, int wp_offset, 
																	 int weight_denom, int color_clip);

	void get_block_00(imgpel *block, imgpel *cur_img, int span, BLOCK_COORD block_size_y);
	void get_luma_10(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x,
										BLOCK_COORD x_pos, imgpel max_imgpel_value);
	void get_luma_20(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, imgpel max_imgpel_value);
	void get_luma_30(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos , imgpel max_imgpel_value);
	void get_luma_01(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x,
										BLOCK_COORD x_pos, BLOCK_COORD shift_x, imgpel max_imgpel_value);
	void get_luma_02(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, BLOCK_COORD shift_x, imgpel max_imgpel_value);
	void get_luma_03(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, BLOCK_COORD shift_x, imgpel max_imgpel_value);
	void get_luma_21(imgpel **block, imgpel **cur_imgY, int **tmp_res, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, imgpel max_imgpel_value);
	void get_luma_22(imgpel **block, imgpel **cur_imgY, int **tmp_res, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, imgpel max_imgpel_value);
	void get_luma_23(imgpel **block, imgpel **cur_imgY, int **tmp_res, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, imgpel max_imgpel_value);
	void get_luma_12(imgpel **block, imgpel **cur_imgY, int **tmp_res, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, BLOCK_COORD shift_x, imgpel max_imgpel_value);
	void get_luma_32(imgpel **block, imgpel **cur_imgY, int **tmp_res, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, BLOCK_COORD shift_x, imgpel max_imgpel_value);
	void get_luma_33(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, BLOCK_COORD shift_x, imgpel max_imgpel_value);
	void get_luma_11(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, BLOCK_COORD shift_x, imgpel max_imgpel_value);
	void get_luma_13(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, BLOCK_COORD shift_x, imgpel max_imgpel_value);
	void get_luma_31(imgpel **block, imgpel **cur_imgY, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
										BLOCK_COORD x_pos, BLOCK_COORD shift_x, imgpel max_imgpel_value);
	void get_chroma_0X(imgpel *block, imgpel *cur_img, int span, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
											int w00, int w01, int total_scale);
	void get_chroma_X0(imgpel *block, imgpel *cur_img, int span, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
											int w00, int w10, int total_scale);
	void get_chroma_XY(imgpel *block, imgpel *cur_img, int span, BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, 
											int w00, int w01, int w10, int w11, int total_scale);

	void alloc_pic_motion(PicMotionParamsOld *motion, PIXEL_COORD size_y, PIXEL_COORD size_x);
	void free_pic_motion(PicMotionParamsOld *motion);

	StorablePicture* get_short_term_pic(CSlice *currSlice, DecodedPictureBuffer *p_Dpb, int picNum);
	StorablePicture *get_long_term_pic(CSlice *currSlice, DecodedPictureBuffer *p_Dpb, int LongtermPicNum);
	void reorder_long_term(CSlice *currSlice, StorablePicture **RefPicListX, int16_t num_ref_idx_lX_active_minus1, 
													int LongTermPicNum, int *refIdxLX);
	void reorder_short_term(CSlice *currSlice, int cur_list, int16_t num_ref_idx_lX_active_minus1, int picNumLX, int *refIdxLX);

	void sliding_window_memory_management(DecodedPictureBuffer *p_Dpb, StorablePicture *p);
	void adaptive_memory_management(DecodedPictureBuffer *p_Dpb, StorablePicture *p);

	inline void copy_img_data(imgpel *out_img, imgpel *in_img, PIXEL_COORD ostride, PIXEL_COORD istride, 
														 PIXEL_COORD size_y, PIXEL_COORD size_x);

	int remove_unused_proc_pic_from_dpb(DecodedPictureBuffer *p_Dpb);
	void update_direct_mv_info_temporal(CMacroblock *currMB);

	void insert_picture_in_dpb(CVideoParameters *p_Vid, FrameStore *fs, StorablePicture *p);
	int output_one_frame_from_dpb(DecodedPictureBuffer *p_Dpb);
	void gen_field_ref_ids(CVideoParameters *p_Vid, StorablePicture *p);

	void mc_prediction(imgpel **mb_pred, imgpel **block, 
											BLOCK_COORD block_size_y, BLOCK_COORD block_size_x, PIXEL_COORD ioff);
	void bi_prediction(imgpel **mb_pred, imgpel **block_l0, imgpel **block_l1,
                      BLOCK_COORD block_size_y, BLOCK_COORD block_size_x,
                      PIXEL_COORD ioff);
	void get_block_chroma(StorablePicture *curr_ref, PIXEL_COORD x_pos, PIXEL_COORD y_pos, BLOCK_COORD subpel_x, BLOCK_COORD subpel_y, PIXEL_COORD maxold_x, PIXEL_COORD maxold_y,
                         BLOCK_COORD block_size_x, BLOCK_COORD vert_block_size, BLOCK_COORD shiftpel_x, BLOCK_COORD shiftpel_y,
                         imgpel *block1, imgpel *block2, int total_scale, imgpel no_ref_value, CVideoParameters *p_Vid);

	void img2buf_normal(imgpel** imgX, uint8_t* buf, PIXEL_COORD size_x, PIXEL_COORD size_y, uint8_t symbol_size_in_bytes, 
											 PIXEL_COORD crop_left, PIXEL_COORD crop_right, PIXEL_COORD crop_top, PIXEL_COORD crop_bottom, PIXEL_COORD iOutStride);
	void img2buf_byte(imgpel** imgX, uint8_t* buf, PIXEL_COORD size_x, PIXEL_COORD size_y, uint8_t symbol_size_in_bytes, 
										PIXEL_COORD crop_left, PIXEL_COORD crop_right, PIXEL_COORD crop_top, PIXEL_COORD crop_bottom, PIXEL_COORD iOutStride);
	void img2buf_endian(imgpel** imgX, uint8_t* buf, PIXEL_COORD size_x, PIXEL_COORD size_y, uint8_t symbol_size_in_bytes, 
											 PIXEL_COORD crop_left, PIXEL_COORD crop_right, PIXEL_COORD crop_top, PIXEL_COORD crop_bottom, PIXEL_COORD iOutStride);

	void write_picture(CVideoParameters *p_Vid, StorablePicture *p, int p_out, int real_structure);
	void write_out_picture(CVideoParameters *p_Vid, StorablePicture *p, int p_out);

	void allocate_p_dec_pic(CVideoParameters *p_Vid, DecodedPicList *pDecPic, StorablePicture *p, uint32_t iLumaSize, uint32_t iFrameSize, BLOCK_COORD iLumaSizeX, BLOCK_COORD iLumaSizeY, 
													 BLOCK_COORD iChromaSizeX, BLOCK_COORD iChromaSizeY);

	void read_comp_coeff_8x8_MB_CABAC_ls(CMacroblock *currMB, CSyntaxElement *currSE, ColorPlane pl);
	void read_CBP_and_coeffs_from_NAL_CABAC_420(CMacroblock *currMB);
	void readCompCoeff8x8_CABAC_lossless(CMacroblock *currMB, CSyntaxElement *currSE, ColorPlane pl, int b8);
	void read_CBP_and_coeffs_from_NAL_CABAC_444(CMacroblock *currMB);
	void read_CBP_and_coeffs_from_NAL_CABAC_422(CMacroblock *currMB);
	void read_CBP_and_coeffs_from_NAL_CABAC_400(CMacroblock *currMB);
	void read_comp_coeff_8x8_MB_CABAC(CMacroblock *currMB, CSyntaxElement *currSE, ColorPlane pl);
	void readCompCoeff8x8_CABAC(CMacroblock *currMB, CSyntaxElement *currSE, ColorPlane pl, int b8);
	void read_comp_coeff_4x4_CABAC_ls(CMacroblock *currMB, CSyntaxElement *currSE, ColorPlane pl, 
																				 int16_t(*InvLevelScale4x4)[4], int8_t qp_per, int8_t cbp);
	void read_comp_coeff_4x4_CABAC(CMacroblock *currMB, CSyntaxElement *currSE, ColorPlane pl, 
																			int16_t(*InvLevelScale4x4)[4], int8_t qp_per, int8_t cbp);
	void read_CBP_and_coeffs_from_NAL_CAVLC_422(CMacroblock *currMB);
	void read_CBP_and_coeffs_from_NAL_CAVLC_400(CMacroblock *currMB);
	void read_CBP_and_coeffs_from_NAL_CAVLC_444(CMacroblock *currMB);
	void read_CBP_and_coeffs_from_NAL_CAVLC_420(CMacroblock *currMB);
	void read_comp_coeff_8x8_CAVLC_ls(CMacroblock *currMB, ColorPlane pl, int16_t(*InvLevelScale8x8)[8], 
																		 int8_t qp_per, int8_t cbp, uint8_t **nzcoeff);
	void read_comp_coeff_4x4_CAVLC_ls(CMacroblock *currMB, ColorPlane pl, int16_t(*InvLevelScale4x4)[4], 
																		 int8_t qp_per, int8_t cbp, uint8_t **nzcoeff);
	void read_comp_coeff_8x8_CAVLC(CMacroblock *currMB, ColorPlane pl, int16_t(*InvLevelScale8x8)[8], int8_t qp_per, 
																	int8_t cbp, uint8_t **nzcoeff);
	void read_comp_coeff_4x4_CAVLC(CMacroblock *currMB, ColorPlane pl, int16_t(*InvLevelScale4x4)[4], 
																	int8_t qp_per, int8_t cbp, uint8_t **nzcoeff);
	void read_comp_coeff_4x4_smb_CABAC(CMacroblock *currMB, CSyntaxElement *currSE, ColorPlane pl, BLOCK_COORD block_y, BLOCK_COORD block_x, 
																			int start_scan, INT64T *cbp_blk);

	int predict_nnz(CMacroblock *currMB, CAVLCBlockTypes block_type, int i,int j);
	int predict_nnz_chroma(CMacroblock *currMB, int i,int j);

	int DecomposeRTPpacket(RTPpacket_t *p);
	int RTPReadPacket(RTPpacket_t *p, int bitstream);

	static void set_dequant4x4(int16_t(*InvLevelScale4x4)[4], const int16_t(*dequant)[4], const int16_t *qmatrix);
  static void set_dequant8x8(int16_t(*InvLevelScale8x8)[8], const int16_t(*dequant)[8], const int16_t *qmatrix);

	void clear_picture(CVideoParameters *p_Vid, StorablePicture *p);
	void write_unpaired_field(CVideoParameters *p_Vid, FrameStore* fs, int p_out);
	void flush_direct_output(CVideoParameters *p_Vid, int p_out);

	static void updateMaxValue(FrameFormat *format);
	static void reset_format_info(seq_parameter_set_rbsp_t *sps, CVideoParameters *p_Vid, FrameFormat *source, FrameFormat *output);
	static void set_coding_par(seq_parameter_set_rbsp_t *sps, CodingParameters *cps);
	static void setup_layer_info(CVideoParameters *p_Vid, seq_parameter_set_rbsp_t *sps, LayerParameters *p_Lps);

	static void copy8x8(imgpel **mb_rec, imgpel **mpr, PIXEL_COORD ioff);
	static void recon8x8_lossless(int **m7, imgpel **mb_rec, imgpel **mpr, imgpel max_imgpel_value, PIXEL_COORD ioff);
	static void recon8x8(int **m7, imgpel **mb_rec, imgpel **mpr, imgpel max_imgpel_value, PIXEL_COORD ioff);
	static inline int ShowBitsThres(int inf, int numbits);
	int code_from_bitstream_2d(CSyntaxElement *sym,Bitstream *currStream,
                              const uint8_t *lentab,const uint8_t *codtab,
                              int tabwidth,int tabheight, int *code);

	void gen_pic_list_from_frame_list(PictureStructure currStructure, FrameStore **fs_list, int list_idx, StorablePicture **list, 
																	int8_t *list_size, int long_term);
	void update_ltref_list(DecodedPictureBuffer *p_Dpb);

	seq_parameter_set_rbsp_t *AllocSPS();
	pic_parameter_set_rbsp_t *AllocPPS();
	void FreeSPS(seq_parameter_set_rbsp_t *sps);
	void FreePPS(pic_parameter_set_rbsp_t *pps);
	static int sps_is_equal(seq_parameter_set_rbsp_t *sps1, seq_parameter_set_rbsp_t *sps2);
	static int pps_is_equal(pic_parameter_set_rbsp_t *pps1, pic_parameter_set_rbsp_t *pps2);

	void sample_reconstruct(imgpel **curImg, imgpel **mpr, int **mb_rres, BLOCK_COORD mb_x, int opix_x, PIXEL_COORD width, PIXEL_COORD height, 
		imgpel max_imgpel_value, uint8_t dq_bits);

	static int ParameterNameToMapIndex(Mapping *Map, const char *s);

	void GetMotionVectorPredictorMBAFF(CMacroblock *currMB, PixelPos *block, MotionVector *pmv, int16_t ref_frame, 
																		PicMotionParams **mv_info, int list, BLOCK_COORD mb_x, BLOCK_COORD mb_y,
                                    BLOCK_COORD blockshape_x,BLOCK_COORD blockshape_y);
	void GetMotionVectorPredictorNormal(CMacroblock *currMB, PixelPos *block, MotionVector *pmv, int16_t ref_frame,
                                      PicMotionParams **mv_info, int list, BLOCK_COORD mb_x,BLOCK_COORD mb_y,
                                      BLOCK_COORD blockshape_x,BLOCK_COORD blockshape_y);

	int read_and_store_CBP_block_bit_444(CMacroblock *currMB, DecodingEnvironmentPtr  dep_dp, CABACBlockTypes type);
	int read_and_store_CBP_block_bit_normal(CMacroblock *currMB, DecodingEnvironmentPtr  dep_dp, CABACBlockTypes type);
	int read_significance_map(CMacroblock *currMB, DecodingEnvironmentPtr  dep_dp, int type, int coeff[]);
	void read_significant_coefficients(DecodingEnvironmentPtr dep_dp, TextureInfoContexts  *tex_ctx, int type, int *coeff);
	unsigned int exp_golomb_decode_eq_prob(DecodingEnvironmentPtr dep_dp,int k);
	inline void conf_read_check(int val, int expected);
	void PatchInp(InputParameters *p_Inp);
	void Configure(InputParameters *p_Inp, int ac, char *av[]);

	void mm_mark_current_picture_long_term(DecodedPictureBuffer *p_Dpb, StorablePicture *p, int long_term_frame_idx);
	void mm_unmark_short_term_for_reference(DecodedPictureBuffer *p_Dpb, StorablePicture *p, int difference_of_pic_nums_minus1);
	void mm_unmark_long_term_for_reference(DecodedPictureBuffer *p_Dpb, StorablePicture *p, int long_term_pic_num);
	void unmark_long_term_frame_for_reference_by_frame_idx(DecodedPictureBuffer *p_Dpb, int long_term_frame_idx);
	void mark_pic_long_term(DecodedPictureBuffer *p_Dpb, StorablePicture* p, int long_term_frame_idx, int picNumX);
	void mm_assign_long_term_frame_idx(DecodedPictureBuffer *p_Dpb, StorablePicture* p, int difference_of_pic_nums_minus1, int long_term_frame_idx);
	void unmark_long_term_field_for_reference_by_frame_idx(DecodedPictureBuffer *p_Dpb, PictureStructure structure, 
																													int long_term_frame_idx, bool mark_current, unsigned curr_frame_num, int curr_pic_num);
	void mm_update_max_long_term_frame_idx(DecodedPictureBuffer *p_Dpb, int max_long_term_frame_idx_plus1);
	void mm_unmark_all_short_term_for_reference(DecodedPictureBuffer *p_Dpb);
	void mm_unmark_all_long_term_for_reference(DecodedPictureBuffer *p_Dpb);
	bool is_used_for_reference(FrameStore *fs);
	void get_smallest_poc(DecodedPictureBuffer *p_Dpb, PocType *poc, int *pos);
	int remove_unused_frame_from_dpb(DecodedPictureBuffer *p_Dpb);
	inline void set_direct_references(const PixelPos *mb, int8_t *l0_rFrame, int8_t *l1_rFrame, PicMotionParams **mv_info);
	void set_direct_references_mb_field(const PixelPos *mb, int8_t *l0_rFrame, int8_t *l1_rFrame, PicMotionParams **mv_info, CMacroblock *mb_data);
	void set_direct_references_mb_frame(const PixelPos *mb, int8_t *l0_rFrame, int8_t *l1_rFrame, PicMotionParams **mv_info, CMacroblock *mb_data);
	void check_motion_vector_range(const MotionVector *mv, CSlice *pSlice);
	inline int check_vert_mv(int llimit, int vec1_y,int rlimit);
	void perform_mc_single_wp(CMacroblock *currMB, ColorPlane pl, StorablePicture *dec_picture, int8_t pred_dir, 
														 BLOCK_COORD i, BLOCK_COORD j, BLOCK_COORD block_size_x, BLOCK_COORD block_size_y);
	void perform_mc_single(CMacroblock *currMB, ColorPlane pl, StorablePicture *dec_picture, int8_t pred_dir, int i, int j, 
													BLOCK_COORD block_size_x, BLOCK_COORD block_size_y);
	void perform_mc_bi_wp(CMacroblock *currMB, ColorPlane pl, StorablePicture *dec_picture, int i, int j, 
												 BLOCK_COORD block_size_x, BLOCK_COORD block_size_y);
	void perform_mc_bi(CMacroblock *currMB, ColorPlane pl, StorablePicture *dec_picture, int i, int j, 
											BLOCK_COORD block_size_x, BLOCK_COORD block_size_y);

	static bool is_short_term_reference(FrameStore* fs);
	static bool is_long_term_reference(FrameStore* fs);

	static int get_pic_num_x(StorablePicture *p, int difference_of_pic_nums_minus1);

	int concealByTrial(frame *recfr, imgpel *predMB, int currMBNum, objectBuffer_t *object_list, int predBlocks[],
                      PIXEL_COORD picSizeX, PIXEL_COORD picSizeY, int8_t *yCondition);
	int concealByCopy(frame *recfr, int currMBNum, objectBuffer_t *object_list, PIXEL_COORD picSizeX);
	void pixMeanInterpolateBlock(CVideoParameters *p_Vid, imgpel *src[], imgpel *block, int blockSize, PIXEL_COORD frameWidth);
	void copyBetweenFrames(frame *recfr, int currYBlockNum, PIXEL_COORD picSizeX, int regionSize);

	inline int set_cbp_bit(CMacroblock *neighbor_mb);
	inline int set_cbp_bit_ac(CMacroblock *neighbor_mb, PixelPos *block);

	static void init_lists_for_non_reference_loss(DecodedPictureBuffer *p_Dpb, SliceType, PictureStructure);

	void conceal_non_ref_pics(DecodedPictureBuffer *p_Dpb, int diff);
	void conceal_lost_frames (DecodedPictureBuffer *p_Dpb, CSlice *pSlice);

	static void sliding_window_poc_management(DecodedPictureBuffer *p_Dpb, StorablePicture *p);
	void write_lost_non_ref_pic       (DecodedPictureBuffer *p_Dpb, PocType poc, int p_out);
	void write_lost_ref_after_idr     (DecodedPictureBuffer *p_Dpb, int pos);

	static int comp(const void *i, const void *j) {
	  return *(int *)i - *(int *)j;
		}

	/*!
	 ************************************************************************
	*\brief
	*   returns true, if picture is short term reference picture
	 *
	 ************************************************************************
	 */
	static inline bool is_short_ref(StorablePicture *s) {
		return (s->used_for_reference && !s->is_long_term);
		}

	/*!
	 ************************************************************************
	*\brief
	*   returns true, if picture is long term reference picture
	 *
	 ************************************************************************
	 */
	static inline bool is_long_ref(StorablePicture *s) {
		return (s->used_for_reference && s->is_long_term);
		}


	struct concealment_node *init_node(StorablePicture *, int);
	void print_node(struct concealment_node *);
	void print_list(struct concealment_node *);

	void init_dec_stats  (DecStatParameters *stats);
	void delete_dec_stats(DecStatParameters *stats);

	/*
	* External function interface
	*/
	void ercInit(CVideoParameters *p_Vid, PIXEL_COORD pic_sizex, PIXEL_COORD pic_sizey, bool flag);
	ercVariables_t *ercOpen(void);
	void ercReset(ercVariables_t *errorVar, int nOfMBs, int numOfSegments, PIXEL_COORD picSizeX);
	void ercClose(CVideoParameters *p_Vid, ercVariables_t *errorVar);
	void ercSetErrorConcealment(ercVariables_t *errorVar, int value);
	void ercWriteMBMODEandMV(CMacroblock *currMB);

	void ercMarkCurrMBConcealed(int currMBNum, int8_t comp, PIXEL_COORD picSizeX, ercVariables_t *errorVar);

	int ercConcealInterFrame(frame *recfr, objectBuffer_t *object_list,
                          PIXEL_COORD picSizeX, PIXEL_COORD picSizeY, ercVariables_t *errorVar, ColorFormat chroma_format_idc);

	//! Maps parameter name to its address, type etc.
	char *GetConfigFileContent(char *Filename);
	int  InitParams           (Mapping *Map);
	int TestParams(Mapping *Map, int bitdepth_qp_scale[3]);
	int DisplayParams(Mapping *Map, const char *message);
	void ParseContent(InputParameters *p_Inp, Mapping *Map, char *buf, int bufsize);


	void JMDecHelpExit();
	void ParseCommand(InputParameters *p_Inp, int ac, char *av[]);

	void init_contexts(CSlice *currslice);

	//! allocate one NAL Unit
	NALU_t *AllocNALU(int);
	//! free one NAL Unit
	void FreeNALU(NALU_t *n);

#if MVC_EXTENSION_ENABLE
	void nal_unit_header_svc_extension();
	void prefix_nal_unit_svc();
#endif

	int get_annex_b_NALU(NALU_t *nalu, ANNEXB_t *annex_b);

	void open_annex_b  (char *fn, ANNEXB_t *annex_b);
	void close_annex_b (ANNEXB_t *annex_b);
	void malloc_annex_b(CVideoParameters *p_Vid, ANNEXB_t **p_annex_b);
	void free_annex_b  (ANNEXB_t **p_annex_b);
	static void init_annex_b  (ANNEXB_t *annex_b);
	void reset_annex_b (ANNEXB_t *annex_b);

	void arideco_start_decoding(DecodingEnvironmentPtr eep, uint8_t *code_buffer, int firstbyte, int *code_len);
	int  arideco_bits_read(DecodingEnvironmentPtr dep);
	void arideco_done_decoding(DecodingEnvironmentPtr dep);
	void biari_init_context(int8_t qp, BiContextTypePtr ctx, const int8_t *ini);
	unsigned int biari_decode_symbol(DecodingEnvironment *dep, BiContextType *bi_ct);
	unsigned int biari_decode_symbol_eq_prob(DecodingEnvironmentPtr dep);
	unsigned int biari_decode_final(DecodingEnvironmentPtr dep);

	void iMBtrans4x4(CMacroblock *currMB, ColorPlane pl, bool smb);
	void iMBtrans8x8(CMacroblock *currMB, ColorPlane pl);

	void itrans_sp_cr(CMacroblock *currMB, int uv);

	void Inv_Residual_trans_4x4(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	void Inv_Residual_trans_8x8(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff,PIXEL_COORD joff);

	void itrans4x4   (CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	void itrans4x4_ls(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	void itrans_sp   (CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	void itrans_2    (CMacroblock *currMB, ColorPlane pl);
	void iTransform  (CMacroblock *currMB, ColorPlane pl, bool smb);

	static void copy_image_data(imgpel  **imgBuf1, imgpel  **imgBuf2, PIXEL_COORD off1, PIXEL_COORD off2, 
																		 PIXEL_COORD width, PIXEL_COORD height);
	static void copy_image_data_16x16(imgpel  **imgBuf1, imgpel  **imgBuf2, PIXEL_COORD off1, PIXEL_COORD  off2);
	static void copy_image_data_8x8  (imgpel  **imgBuf1, imgpel  **imgBuf2, PIXEL_COORD off1, PIXEL_COORD off2);
	static void copy_image_data_4x4  (imgpel  **imgBuf1, imgpel  **imgBuf2, PIXEL_COORD off1, PIXEL_COORD off2);

	int CheckVertMV(CMacroblock *currMB, int vec1_y, BLOCK_COORD block_size_y);

	MotionInfoContexts * create_contexts_MotionInfo(void);
	TextureInfoContexts *create_contexts_TextureInfo(void);
	void delete_contexts_MotionInfo(MotionInfoContexts *enco_ctx);
	void delete_contexts_TextureInfo(TextureInfoContexts *enco_ctx);

	void cabac_new_slice(CSlice *currSlice);

	void readMB_typeInfo_CABAC_i_slice   (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readMB_typeInfo_CABAC_p_slice   (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readMB_typeInfo_CABAC_b_slice   (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readB8_typeInfo_CABAC_p_slice   (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readB8_typeInfo_CABAC_b_slice   (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readIntraPredMode_CABAC         (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readRefFrame_CABAC              (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void read_MVD_CABAC                  (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void read_mvd_CABAC_mbaff            (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void read_CBP_CABAC                  (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readRunLevel_CABAC              (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void read_dQuant_CABAC               (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readCIPredMode_CABAC            (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void read_skip_flag_CABAC_p_slice    (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void read_skip_flag_CABAC_b_slice    (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readFieldModeInfo_CABAC         (CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);
	void readMB_transform_size_flag_CABAC(CMacroblock *currMB, CSyntaxElement *se, DecodingEnvironmentPtr dep_dp);

	void readIPCM_CABAC(CSlice *currSlice, struct datapartition_dec *dP);

	bool cabac_startcode_follows(CSlice *currSlice, bool eos_bit);

	int readSyntaxElement_CABAC(CMacroblock *currMB, CSyntaxElement *se, DataPartition *this_dataPart);

	bool check_next_mb_and_get_field_mode_CABAC_p_slice(CSlice *currSlice, CSyntaxElement *se, DataPartition  *act_dp);
	bool check_next_mb_and_get_field_mode_CABAC_b_slice(CSlice *currSlice, CSyntaxElement *se, DataPartition  *act_dp);

	void CheckAvailabilityOfNeighborsCABAC(CMacroblock *currMB);

	void set_read_and_store_CBP(CMacroblock **currMB, ColorFormat chroma_format_idc);

	void InterpretSEIMessage                                (uint8_t* payload, int16_t size, CVideoParameters *p_Vid, CSlice *pSlice);
	void interpret_spare_pic                                (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_subsequence_info                         (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_subsequence_layer_characteristics_info   (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_subsequence_characteristics_info         (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_scene_information                        (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_user_data_registered_itu_t_t35_info      (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_user_data_unregistered_info              (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_pan_scan_rect_info                       (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_recovery_point_info                      (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_filler_payload_info                      (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_dec_ref_pic_marking_repetition_info      (uint8_t* payload, int16_t size, CVideoParameters *p_Vid, CSlice *pSlice);
	void interpret_full_frame_freeze_info                   (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_full_frame_freeze_release_info           (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_full_frame_snapshot_info                 (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_progressive_refinement_start_info        (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_progressive_refinement_end_info          (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_motion_constrained_slice_group_set_info  (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_reserved_info                            (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_buffering_period_info                    (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_picture_timing_info                      (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_film_grain_characteristics_info          (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_deblocking_filter_display_preference_info(uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_stereo_video_info_info                   (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_post_filter_hints_info                   (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_tone_mapping                             (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_frame_packing_arrangement_info           (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);
	void interpret_green_metadata_info                      (uint8_t* payload, int16_t size, CVideoParameters *p_Vid);

#if ENABLE_OUTPUT_TONEMAPPING
	void tone_map               (imgpel** imgX, imgpel* lut, PIXEL_COORD size_x, PIXEL_COORD size_y);
	static void init_tone_mapping_sei  (ToneMappingSEI *seiToneMapping);
	static void update_tone_mapping_sei(ToneMappingSEI *seiToneMapping);
#endif

	void itrans8x8(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	void icopy8x8 (CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);

	int read_se_v(char *tracestring, Bitstream *bitstream, int *used_bits);
	int read_ue_v(char *tracestring, Bitstream *bitstream, int *used_bits);
	bool read_u_1(char *tracestring, Bitstream *bitstream, int *used_bits);
	int read_u_v(uint8_t LenInBits, char *tracestring, Bitstream *bitstream, int *used_bits);
	int read_i_v(uint8_t LenInBits, char *tracestring, Bitstream *bitstream, int *used_bits);

	// CAVLC mapping
	void linfo_ue(int len, int info, int8_t *value1, int *dummy);
	void linfo_se(int len, int info, int8_t *value1, int *dummy);

	void linfo_cbp_intra_normal(int len,int info,int8_t *cbp, int *dummy);
	void linfo_cbp_inter_normal(int len,int info,int8_t *cbp, int *dummy);
	void linfo_cbp_intra_other(int len,int info,int8_t *cbp, int *dummy);
	void linfo_cbp_inter_other(int len,int info,int8_t *cbp, int *dummy);

	void linfo_levrun_inter(int len,int info,int8_t *level,int *irun);
	void linfo_levrun_c2x2(int len,int info,int8_t *level,int *irun);

	bool uvlc_startcode_follows(CSlice *currSlice, bool dummy);

	int readSyntaxElement_VLC (CSyntaxElement *sym, Bitstream *currStream);
	int readSyntaxElement_UVLC(CMacroblock *currMB, CSyntaxElement *sym, struct datapartition_dec *dp);
	int readSyntaxElement_Intra4x4PredictionMode(CSyntaxElement *sym, Bitstream *currStream);

	int GetVLCSymbol (uint8_t buffer[],int totbitoffset,int *info, int bytecount);
	int GetVLCSymbol_IntraMode (uint8_t buffer[],int totbitoffset,int *info, int bytecount);

	int8_t readSyntaxElement_FLC                      (CSyntaxElement *sym, Bitstream *currStream);
	int readSyntaxElement_NumCoeffTrailingOnes        (CSyntaxElement *sym,  Bitstream *currStream, char *type);
	int readSyntaxElement_NumCoeffTrailingOnesChromaDC(CVideoParameters *p_Vid, CSyntaxElement *sym, Bitstream *currStream);
	int readSyntaxElement_Level_VLC0                  (CSyntaxElement *sym, Bitstream *currStream);
	int readSyntaxElement_Level_VLCN                  (CSyntaxElement *sym, int vlc, Bitstream *currStream);
	int readSyntaxElement_TotalZeros                  (CSyntaxElement *sym, Bitstream *currStream);
	int readSyntaxElement_TotalZerosChromaDC          (CVideoParameters *p_Vid, CSyntaxElement *sym, Bitstream *currStream);
	int readSyntaxElement_Run                         (CSyntaxElement *sym, Bitstream *currStream);
	static int GetBits(uint8_t buffer[],int totbitoffset,int *info, int bitcount, int numbits);		// serve int numbits!
	static int ShowBits(uint8_t buffer[],int totbitoffset,int bitcount, int numbits);

	static int more_rbsp_data(uint8_t buffer[],int totbitoffset,int bytecount);

private:
	// prototypes
	void error(const char *text, int code);

	// dynamic mem allocation
	int  init_global_buffers(CVideoParameters *p_Vid, uint8_t layer_id);
	void free_global_buffers(CVideoParameters *p_Vid);
	void free_layer_buffers(CVideoParameters *p_Vid, uint8_t layer_id);

	int RBSPtoSODB(uint8_t *streamBuffer, int last_byte_pos);
	int EBSPtoRBSP(uint8_t *streamBuffer, int end_bytepos, int begin_bytepos);
	int NALUtoRBSP(NALU_t *nalu);

	DataPartition *AllocPartition(uint8_t n);
	void FreePartition(DataPartition *dp, uint8_t n);

	static unsigned CeilLog2   (unsigned uiVal);
	static unsigned CeilLog2_sf(unsigned uiVal);

	// For 4:4:4 independent mode
	void change_plane_JV      (CVideoParameters *p_Vid, int nplane, CSlice *pSlice);
	void make_frame_picture_JV(CVideoParameters *p_Vid);

#if MVC_EXTENSION_ENABLE
	void nal_unit_header_mvc_extension(NALUnitHeaderMVCExt_t *NaluHeaderMVCExt, struct bit_stream_dec *bitstream);
	void ref_pic_list_mvc_modification(CSlice *currSlice);
	void init_mvc_picture(CSlice *currSlice);
	void OpenOutputFiles(CVideoParameters *p_Vid, int view0_id, int view1_id);
#endif

	void FreeDecPicList (DecodedPicList *pDecPicList);
	void ClearDecPicList(CVideoParameters *p_Vid);
	DecodedPicList *get_one_avail_dec_pic_from_list(DecodedPicList *pDecPicList, bool b3D, int8_t view_id);
	CSlice *malloc_slice(InputParameters *p_Inp, CVideoParameters *p_Vid);
	void copy_slice_info(CSlice *currSlice, OldSliceParams *p_old_slice);
	void set_global_coding_par(CVideoParameters *p_Vid, CodingParameters *cps);

	static inline bool is_FREXT_profile(ProfileIDC profile_idc) {
		// we allow all FRExt tools, when no profile is active
		return (profile_idc==NO_PROFILE || profile_idc==FREXT_HP || profile_idc==FREXT_Hi10P || profile_idc==FREXT_Hi422 || profile_idc==FREXT_Hi444 || profile_idc == FREXT_CAVLC444);
		}

	static inline bool is_HI_intra_only_profile(ProfileIDC profile_idc, bool constrained_set3_flag) {
		return ((((profile_idc == FREXT_Hi10P) || (profile_idc == FREXT_Hi422) || (profile_idc == FREXT_Hi444)) && constrained_set3_flag) || 
			(profile_idc == FREXT_CAVLC444));
		}
	static inline bool is_BL_profile(ProfileIDC profile_idc) {
		return (profile_idc == FREXT_CAVLC444 || profile_idc == BASELINE || profile_idc == MAIN || profile_idc == EXTENDED ||
						profile_idc == FREXT_HP || profile_idc == FREXT_Hi10P || profile_idc == FREXT_Hi422 || profile_idc == FREXT_Hi444);
		}
	static inline bool is_EL_profile(ProfileIDC profile_idc) {
		return ((profile_idc == MVC_HIGH) || (profile_idc == STEREO_HIGH));
		}

	static inline bool is_MVC_profile(ProfileIDC profile_idc) {
		return ((0)
	#if MVC_EXTENSION_ENABLE
			|| (profile_idc == MVC_HIGH) || (profile_idc == STEREO_HIGH)
	#endif
			);
		}

	static inline int compare_mvs(const MotionVector *mv0, const MotionVector *mv1, uint8_t mvlimit) {
		return (iabs(mv0->mv_x-mv1->mv_x) >= 4) | (iabs(mv0->mv_y-mv1->mv_y) >= mvlimit);
		}

/*!
 ************************************************************************
*\brief
*   Set context for reference frames
 ************************************************************************
 */
	static inline int BType2CtxRef(int btype) {
		return btype >= 4;
		}

	static void forward4x4  (int **block , int **tblock, BLOCK_COORD pos_y, BLOCK_COORD pos_x);
	static void inverse4x4  (int **tblock, int **block , BLOCK_COORD pos_y, BLOCK_COORD pos_x);
	static void forward8x8  (int **block , int **tblock, BLOCK_COORD pos_y, BLOCK_COORD pos_x);
	static void inverse8x8  (int **tblock, int **block , BLOCK_COORD pos_x);
	static void hadamard4x4 (int **block , int **tblock);
	static void ihadamard4x4(int **tblock, int **block);
	static void hadamard4x2 (int **block , int **tblock);
	static void ihadamard4x2(int **tblock, int **block);
	static void hadamard2x2 (int **block , int tblock[4]);
	static void ihadamard2x2(int block[4], int tblock[4]);

	void set_intra_prediction_modes(CSlice *currSlice);
	void intra_pred_chroma_mbaff(CMacroblock *currMB);
	int intra4x4_dc_pred(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_vert_pred(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_hor_pred(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_diag_down_right_pred(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_diag_down_left_pred(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_vert_right_pred(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_vert_left_pred(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_hor_up_pred(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_hor_down_pred(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);

	int intra4x4_dc_pred_mbaff(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_vert_pred_mbaff(CMacroblock *currMB, ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff);
	int intra4x4_hor_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	int intra4x4_diag_down_right_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	int intra4x4_diag_down_left_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	int intra4x4_vert_right_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	int intra4x4_vert_left_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	int intra4x4_hor_up_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	int intra4x4_hor_down_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);

	int intra4x4_pred_normal(CMacroblock *currMB,ColorPlane pl,PIXEL_COORD ioff,PIXEL_COORD joff,
                         PIXEL_COORD img_block_x,PIXEL_COORD img_block_y);
	int intra_pred_4x4_mbaff (CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff, 
																	 PIXEL_COORD img_block_x, PIXEL_COORD img_block_y);
	int intra_pred_4x4_normal(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff, 
																	 PIXEL_COORD img_block_x, PIXEL_COORD img_block_y);

	int intra16x16_vert_pred(CMacroblock *currMB, ColorPlane pl);
	int intra16x16_hor_pred(CMacroblock *currMB, ColorPlane pl);
	int intra16x16_vert_pred_mbaff(CMacroblock *currMB, ColorPlane pl);
	int intra16x16_hor_pred_mbaff(CMacroblock *currMB, ColorPlane pl);
	int intra16x16_plane_pred_mbaff(CMacroblock *currMB, ColorPlane pl);
	int intra16x16_plane_pred(CMacroblock *currMB, ColorPlane pl);
	int intra16x16_dc_pred_mbaff(CMacroblock *currMB, ColorPlane pl);
	int intrapred_16x16_normal(CMacroblock *currMB,ColorPlane pl,I16x16PredModes predmode);

	void intrapred_chroma_hor(CMacroblock *currMB);
	void intrapred_chroma_ver(CMacroblock *currMB);
	void intrapred_chroma_plane(CMacroblock *currMB);
	void intra_pred_chroma(CMacroblock *currMB);
	void intra_chroma_DC_single_mbaff(imgpel **curr_img, bool up_avail, bool left_avail, PixelPos up, PixelPos left[17], int blk_x, int blk_y, int *pred, int direction);
	void intrapred_chroma_ver_mbaff(CMacroblock *currMB);
	void intra_chroma_DC_all_mbaff(imgpel **curr_img, bool up_avail, bool left_avail, PixelPos up, PixelPos left[17], int blk_x, int blk_y, int *pred);

	inline int intra8x8_dc_pred(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_vert_pred(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_hor_pred(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_diag_down_right_pred(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_diag_down_left_pred(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_vert_right_pred(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_vert_left_pred(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_hor_up_pred(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_hor_down_pred(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);

	inline int intra8x8_dc_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_vert_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_hor_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_diag_down_right_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_diag_down_left_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_vert_right_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_vert_left_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_hor_up_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	inline int intra8x8_hor_down_pred_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);

	int intrapred8x8_normal(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	int intra16x16_dc_pred(CMacroblock *currMB, ColorPlane pl);

	void DeblockPicture(CVideoParameters *p_Vid, StorablePicture *p);
#if JM_PARALLEL_DEBLOCK
	void DeblockParallel(CVideoParameters *p_Vid, StorablePicture *p, unsigned int column, int block, int n_last);
#endif
	void init_Deblock(CVideoParameters *p_Vid, bool mb_aff_frame_flag);

public:
	static inline int16_t smin(int16_t a, int16_t b) {
		return (int16_t)(((a) <(b)) ? (a) : (b));
		}
	static inline int16_t smax(int16_t a, int16_t b) {
		return (int16_t)(((a) >(b)) ? (a) : (b));
		}
	static inline int imin(int a, int b) {
		return ((a) <(b)) ? (a) : (b);
		}
	static inline int imin3(int a, int b, int c) {
		return ((a) <(b)) ? imin(a, c) : imin(b, c);
		}
	static inline int imax(int a, int b) {
		return ((a) >(b)) ? (a) : (b);
		}
	static inline int imedian(int a,int b,int c) {
		if(a>b) { // a>b
			if(b>c) 
				return(b); // a>b>c
			else if(a>c) 
				return(c); // a>c>b
			else 
				return(a); // c>a>b
			}
		else  { // b>a
			if(a>c) 
				return(a); // b>a>c
			else if(b>c)
				return(c); // b>c>a
			else
				return(b);  // c>b>a
			}
		}
	static inline int imedian_old(int a, int b, int c) {
		return (a+b+c-imin(a, imin(b, c))-imax(a, imax(b ,c)));
		}
	static inline double dmin(double a, double b) {
		return ((a) < (b)) ? (a) : (b);
		}
	static inline double dmax(double a, double b) {
		return ((a) > (b)) ? (a) : (b);
		}
	static inline INT64T i64min(INT64T a, INT64T b) {
		return ((a) < (b)) ? (a) : (b);
		}
	static inline INT64T i64max(INT64T a, INT64T b) {
		return ((a) > (b)) ? (a) : (b);
		}
	static inline distblk distblkmin(distblk a, distblk b) {
		return ((a) < (b)) ? (a) : (b);
		}
	static inline distblk distblkmax(distblk a, distblk b) {
		return ((a) > (b)) ? (a) : (b);
		}
	static inline int16_t sabs(int16_t x) {
		static const int16_t SHORT_BITS =(sizeof(int16_t)*CHAR_BIT)-1;
		int16_t y =(int16_t)(x >> SHORT_BITS);
		return (int16_t)((x ^ y)-y);
		}

	static inline int iabs(int x) {
		static const int INT_BITS =(sizeof(int)*CHAR_BIT)-1;
		int y=x >> INT_BITS;
		return (x ^ y)-y;
		}
	static inline double dabs(double x) {
		return((x)<0) ? -(x) : (x);
		}
	static inline INT64T i64abs(INT64T x) {
		static const INT64T INT64_BITS =(sizeof(INT64T)*CHAR_BIT)-1;
		INT64T y=x >> INT64_BITS;
		return (x ^ y)-y;
		}
	static inline double dabs2(double x) {
		return (x) * (x);
		}
	static inline int iabs2(int x) {
		return (x) * (x);
		}
	static inline INT64T i64abs2(INT64T x) {
		return (x) * (x);
		}
	static inline int isign(int x) {
		return ((x>0) - (x<0));
		}
	static inline int isignab(int a, int b) {
		return ((b)<0) ? -iabs(a) : iabs(a);
		}
	static inline int rshift_rnd(int x, int a) {
		return (a>0) ? ((x +(1 << (a-1))) >> a) :(x <<(-a));
		}
	static inline int rshift_rnd_sign(int x, int a) {
		return (x>0) ? ((x +(1 << (a-1))) >> a) :(-((iabs(x) +(1 <<(a-1))) >> a));
		}
	static inline unsigned int rshift_rnd_us(unsigned int x, unsigned int a) {
		return (a>0) ? ((x +(1 << (a-1))) >> a) : x;
		}
	static inline int rshift_rnd_sf(int x, int a) {
		return ((x +(1 << (a-1))) >> a);
		}
	static inline int shift_off_sf(int x, int o, int a) {
		return ((x+o) >> a);
		}
	static inline unsigned int rshift_rnd_us_sf(unsigned int x, unsigned int a) {
		return ((x +(1 << (a-1))) >> a);
		}
	static inline int iClip1(int high, int x) {
		x=imax(x,0);
		x=imin(x,high);
		return x;
		}
	static inline int iClip3(int low, int high, int x) {
		x=imax(x,low);
		x=imin(x,high);
		return x;
		}
	static inline int16_t sClip3(int16_t low, int16_t high, int16_t x) {
		x=smax(x,low);
		x=smin(x,high);
		return x;
		}
	static inline double dClip3(double low, double high, double x) {
		x=dmax(x,low);
		x=dmin(x,high);
		return x;
		}
	static inline distblk weighted_cost(int factor, int bits) {
	#if JCOST_CALC_SCALEUP
		return ((distblk)(factor))*((distblk)(bits));
	#else
	#if(USE_RND_COST)
		return rshift_rnd_sf((lambda) *(bits), LAMBDA_ACCURACY_BITS);
	#else
		return ((factor)*(bits))>>LAMBDA_ACCURACY_BITS;
	#endif
	#endif
		}

	static inline int RSD(int x) {
		return ((x & 2) ? (x | 1) : (x & (~1)));
		}
	static inline int power2(int x) {
		return 1 << x;
		}

	static inline INT64T i64_power2(int x);
	static inline int float2int(float x);
	static inline int get_bit(INT64T x,int n);

#if ZEROSNR
	static inline float psnr(int max_sample_sq, int samples, float sse_distortion);
#else
	static inline float psnr(int max_sample_sq, int samples, float sse_distortion);
#endif

	static inline int RoundLog2(int iValue) {
		int iRet=0;
		int iValue_square=iValue*iValue;
		while((1 <<(iRet+1)) <= iValue_square)
			iRet++;

		iRet =(iRet+1) >> 1;
		return iRet;
		}
	static inline void i32_swap(int *x, int *y) {
		int temp=*x;
		*x=*y;
		*y=temp;
		}
	static inline void i64_swap(INT64T *x, INT64T *y) {
		INT64T temp=*x;
		*x=*y;
		*y=temp;
		}

private:
	static inline bool is_intra_mb(MBModeTypes mb_type);

	static inline int CheckCost_Shift(INT64T mcost, INT64T min_mcost);
	static inline int CheckCost(INT64T mcost, INT64T min_mcost);
	inline void down_scale(distblk *pblkdistCost);
	inline void up_scale(distblk *pblkdistCost);

	inline distblk dist_scale(distblk blkdistCost);
	inline int dist_down(distblk blkdistCost);

	static inline int getChunk(ANNEXB_t *annex_b);
	static inline uint8_t getfbyte(ANNEXB_t *annex_b);
	static inline int FindStartCode(uint8_t *Buf, int zeros_in_startcode);
	inline unsigned int getbyte(DecodingEnvironmentPtr dep);
	inline unsigned int getword(DecodingEnvironmentPtr dep);

/***********************************************************************
*L O C A L L Y   D E F I N E D   F U N C T I O N   P R O T O T Y P E S
 ***********************************************************************
 */
	unsigned int unary_bin_decode            (DecodingEnvironmentPtr dep_dp, BiContextTypePtr ctx, int ctx_offset);
	unsigned int unary_bin_max_decode        (DecodingEnvironmentPtr dep_dp, BiContextTypePtr ctx, int ctx_offset, unsigned int max_symbol);
	unsigned int unary_exp_golomb_level_decode(DecodingEnvironmentPtr dep_dp, BiContextTypePtr ctx);
	unsigned int unary_exp_golomb_mv_decode  (DecodingEnvironmentPtr dep_dp, BiContextTypePtr ctx, unsigned int max_bin);

	static void gettime(TIME_T* time);
	static INT64T timediff(TIME_T* start, TIME_T* end);

private:
	int FirstPartOfSliceHeader(CSlice *currSlice);
	int RestOfSliceHeader     (CSlice *currSlice);

	DecodingEnvironmentPtr arideco_create_decoding_environment();
 	void arideco_delete_decoding_environment(DecodingEnvironmentPtr dep);

	void dec_ref_pic_marking(CVideoParameters *p_Vid, Bitstream *currStream, CSlice *pSlice);

	void decode_poc(CVideoParameters *p_Vid, CSlice *pSlice);
	static int dumppoc   (CVideoParameters *p_Vid);

	void calculate_frame_no(CVideoParameters *p_Vid, StorablePicture *p);
	void find_snr          (CVideoParameters *p_Vid, StorablePicture *p, int *p_ref);
	int  picture_order     (CSlice *pSlice);

	void decode_one_slice(CSlice *currSlice);
	int  read_new_slice  (CSlice *currSlice);

	static int8_t is_new_picture(StorablePicture *dec_picture, CSlice *currSlice, OldSliceParams *p_old_slice);
	static void init_old_slice(OldSliceParams *p_old_slice);
	// For 4:4:4 independent mode
	void copy_dec_picture_JV(CVideoParameters *p_Vid, StorablePicture *dst, StorablePicture *src);

	void frame_postprocessing(CVideoParameters *p_Vid);
	void field_postprocessing(CVideoParameters *p_Vid);

#if MVC_EXTENSION_ENABLE
	int8_t GetViewIdx(CVideoParameters *p_Vid, int8_t iVOIdx);
	static int8_t GetVOIdx(CVideoParameters *p_Vid, int iViewId);
	static int get_maxViewIdx(CVideoParameters *p_Vid, int8_t view_id, bool anchor_pic_flag, int listidx);
	static int is_view_id_in_ref_view_list(int view_id, int *ref_view_id, int num_ref_views);
	static StorablePicture *get_inter_view_pic(CVideoParameters *p_Vid, CSlice *currSlice, 
																						int targetViewID, PocType currPOC, int listidx);
	void gen_pic_list_from_frame_interview_list(PictureStructure currStructure, FrameStore **fs_list, int list_idx, StorablePicture **list, int8_t *list_size);
	void reorder_interview(CVideoParameters *p_Vid, CSlice *currSlice, StorablePicture **RefPicListX, int16_t num_ref_idx_lX_active_minus1, 
															int *refIdxLX, int targetViewID, PocType currPOC, int listidx);
	int InterpretSubsetSPS(CVideoParameters *p_Vid, DataPartition *p, int *curr_seq_set_id);
	int MemAlloc1D(void** ppBuf, int iEleSize, int iNum);
	void hrd_parameters(MVCVUI_t *pMVCVUI, Bitstream *s);
#endif

	void init_slice(CVideoParameters *p_Vid, CSlice *currSlice);
	void decode_slice(CSlice *currSlice, HEADER_TYPE current_header);

	int intra_pred_8x8_normal(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);
	int intra_pred_8x8_mbaff(CMacroblock *currMB, ColorPlane pl, PIXEL_COORD ioff, PIXEL_COORD joff);

	int intra_pred_16x16_mbaff (CMacroblock *currMB, ColorPlane pl, I16x16PredModes predmode);
	int intra_pred_16x16_normal(CMacroblock *currMB, ColorPlane pl, I16x16PredModes predmode);

#ifdef _LEAKYBUCKET_
	// Leaky Bucket functions
	uint32_t GetBigDoubleWord(FILE *fp);
	void calc_buffer(InputParameters *p_Inp);
#endif

	void setup_slice_methods_mbaff(CSlice *currSlice);
	void setup_slice_methods      (CSlice *currSlice);
	void get_neighbors(CMacroblock *currMB, PixelPos *block, BLOCK_COORD mb_x, BLOCK_COORD mb_y, PIXEL_COORD blockshape_x);
	CMacroblock* get_non_aff_neighbor_luma(CMacroblock *mb, int xN, int yN);
	void get4x4Neighbour(CMacroblock *currMB, BLOCK_COORD block_x, BLOCK_COORD block_y, uint32_t mb_size[2], PixelPos *pix);
	void get4x4NeighbourBase(CMacroblock *currMB, BLOCK_COORD block_x, BLOCK_COORD block_y, uint32_t mb_size[2], PixelPos *pix);

	void start_macroblock(CSlice *currSlice, CMacroblock **currMB);
	int  decode_one_macroblock(CMacroblock *currMB, StorablePicture *dec_picture);
	bool exit_macroblock(CSlice *currSlice, int eos_bit);
	void update_qp(CMacroblock *currMB, int8_t qp);


	int mb_pred_intra4x4      (CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
	int mb_pred_intra16x16    (CMacroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
	int mb_pred_intra8x8      (CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);

	int mb_pred_skip          (CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
	int mb_pred_sp_skip       (CMacroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
	int mb_pred_p_inter8x8    (CMacroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
	int mb_pred_p_inter16x16  (CMacroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
	int mb_pred_p_inter16x8   (CMacroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
	int mb_pred_p_inter8x16   (CMacroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
	int mb_pred_b_d4x4spatial (CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
	int mb_pred_b_d8x8spatial (CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
	int mb_pred_b_d4x4temporal(CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
	int mb_pred_b_d8x8temporal(CMacroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
	int mb_pred_b_inter8x8    (CMacroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
	int mb_pred_ipcm          (CMacroblock *currMB);


	void             init_dpb(CVideoParameters *p_Vid, DecodedPictureBuffer *p_Dpb, int type);
	void             re_init_dpb(CVideoParameters *p_Vid, DecodedPictureBuffer *p_Dpb, int type);
	void             free_dpb(DecodedPictureBuffer *p_Dpb);
	FrameStore      *alloc_frame_store(void);
	void             free_frame_store (FrameStore* f);
	StorablePicture *alloc_storable_picture(CVideoParameters *p_Vid, PictureStructure type, PIXEL_COORD size_x, PIXEL_COORD size_y, 
																						PIXEL_COORD size_x_cr, PIXEL_COORD size_y_cr, bool is_output);
	void             free_storable_picture (StorablePicture* p);
	void             store_picture_in_dpb(DecodedPictureBuffer *p_Dpb, StorablePicture* p);
	StorablePicture *get_int16_t_term_pic (CSlice *currSlice, DecodedPictureBuffer *p_Dpb, int picNum);

#if MVC_EXTENSION_ENABLE
	void             idr_memory_management(DecodedPictureBuffer *p_Dpb, StorablePicture* p);
	void             flush_dpbs(DecodedPictureBuffer **p_Dpb, uint8_t nLayers);
	int              GetMaxDecFrameBuffering(CVideoParameters *p_Vid);
	void             append_interview_list(DecodedPictureBuffer *p_Dpb, PictureStructure currPicStructure, int list_idx, 
																					FrameStore **list, int *listXsize, PocType currPOC, 
																					int8_t curr_view_id, bool anchor_pic_flag);
#else
	void             idr_memory_management(DecodedPictureBuffer *p_Dpb, StorablePicture* p);
#endif

	void unmark_for_reference(FrameStore* fs);
	void unmark_for_long_term_reference(FrameStore* fs);
	void remove_frame_from_dpb(DecodedPictureBuffer *p_Dpb, int pos);

	void flush_dpb(DecodedPictureBuffer *p_Dpb);
	void init_lists_p_slice (CSlice *currSlice);
	void init_lists_b_slice (CSlice *currSlice);
	void init_lists_i_slice (CSlice *currSlice);
	void update_pic_num     (CSlice *currSlice);

	void dpb_split_field      (CVideoParameters *p_Vid, FrameStore *fs);
	void dpb_combine_field    (CVideoParameters *p_Vid, FrameStore *fs);
	void dpb_combine_field_yuv(CVideoParameters *p_Vid, FrameStore *fs);

	void reorder_ref_pic_list(CSlice *currSlice, int cur_list);

	void init_mbaff_lists(CVideoParameters *p_Vid, CSlice *currSlice);
	void alloc_ref_pic_list_reordering_buffer(CSlice *currSlice);
	void free_ref_pic_list_reordering_buffer(CSlice *currSlice);

	void fill_frame_num_gap(CVideoParameters *p_Vid, CSlice *pSlice);

	void compute_colocated(CSlice *currSlice, StorablePicture **listX[6]);

	int init_img_data(CVideoParameters *p_Vid, ImageData *p_ImgData, seq_parameter_set_rbsp_t *sps);
	void free_img_data(CVideoParameters *p_Vid, ImageData *p_ImgData);
	void pad_dec_picture(CVideoParameters *p_Vid, StorablePicture *dec_picture);
	void pad_buf(imgpel *pImgBuf, BLOCK_COORD iWidth, BLOCK_COORD iHeight, BLOCK_COORD iStride, BLOCK_COORD iPadX, BLOCK_COORD iPadY);
	void process_picture_in_dpb_s(CVideoParameters *p_Vid, StorablePicture *p_pic);
	StorablePicture *clone_storable_picture(CVideoParameters *p_Vid, StorablePicture *p_pic);
	void store_proc_picture_in_dpb(DecodedPictureBuffer *p_Dpb, StorablePicture* p);

#if MVC_EXTENSION_ENABLE
	void reorder_lists_mvc     (CSlice * currSlice, PocType currPOC);
	void init_lists_p_slice_mvc(CSlice *currSlice);
	void init_lists_b_slice_mvc(CSlice *currSlice);
	void init_lists_i_slice_mvc(CSlice *currSlice);

	void reorder_ref_pic_list_mvc(CSlice *currSlice, int cur_list, int **anchor_ref, int **non_anchor_ref,
																 int8_t view_id, bool anchor_pic_flag, PocType currPOC, int listidx);

	void reorder_short_term(CSlice *currSlice, int cur_list, int16_t num_ref_idx_lX_active_minus1, int picNumLX, 
																 int *refIdxLX, int currViewID);
	void reorder_long_term(CSlice *currSlice, StorablePicture **RefPicListX, int16_t num_ref_idx_lX_active_minus1, int LongTermPicNum, 
																int *refIdxLX, int currViewID);
#endif

	int  allocate_pred_mem(CSlice *currSlice);
	void free_pred_mem    (CSlice *currSlice);

	void get_block_luma(StorablePicture *curr_ref, BLOCK_COORD x_pos, BLOCK_COORD y_pos, PIXEL_COORD block_size_x, PIXEL_COORD block_size_y, imgpel **block,
											 BLOCK_COORD shift_x,PIXEL_COORD maxold_x,PIXEL_COORD maxold_y,int **tmp_res,imgpel max_imgpel_value,imgpel no_ref_value,CMacroblock *currMB);

	void intra_cr_decoding    (CMacroblock *currMB, int8_t yuv);
	void prepare_direct_params(CMacroblock *currMB, StorablePicture *dec_picture, MotionVector *pmvl0, MotionVector *pmvl1,int8_t *l0_rFrame, int8_t *l1_rFrame);
	void perform_mc           (CMacroblock *currMB, ColorPlane pl, StorablePicture *dec_picture, int8_t pred_dir, int i, int j, 
 														BLOCK_COORD block_size_x, BLOCK_COORD block_size_y);

	static void CheckZeroByteNonVCL(CVideoParameters *p_Vid, NALU_t *nalu);
	static void CheckZeroByteVCL   (CVideoParameters *p_Vid, NALU_t *nalu);

	int read_next_nalu(CVideoParameters *p_Vid, NALU_t *nalu);

	void write_stored_frame(CVideoParameters *p_Vid, FrameStore *fs, int p_out);
	void direct_output     (CVideoParameters *p_Vid, StorablePicture *p, int p_out);
	void init_out_buffer   (CVideoParameters *p_Vid);
	void uninit_out_buffer (CVideoParameters *p_Vid);
#if PAIR_FIELDS_IN_OUTPUT
	void flush_pending_output(CVideoParameters *p_Vid, int p_out);
#endif
	void init_output(CodingParameters *p_CodingParams, uint8_t symbol_size_in_bytes);
	void reinit_decoder();

	void Scaling_List(int16_t *scalingList, int16_t sizeOfScalingList, bool *UseDefaultScalingMatrix, Bitstream *s);

	static void InitVUI(seq_parameter_set_rbsp_t *sps);
	int  ReadVUI(DataPartition *p, seq_parameter_set_rbsp_t *sps);
	int  ReadHRDParameters(DataPartition *p, hrd_parameters_t *hrd);

	static void PPSConsistencyCheck(pic_parameter_set_rbsp_t *pps);
	static void SPSConsistencyCheck(seq_parameter_set_rbsp_t *sps);

	void MakePPSavailable(CVideoParameters *p_Vid, int id, pic_parameter_set_rbsp_t *pps);
	void MakeSPSavailable(CVideoParameters *p_Vid, int id, seq_parameter_set_rbsp_t *sps);

	void ProcessSPS(CVideoParameters *p_Vid, NALU_t *nalu);
	void ProcessPPS(CVideoParameters *p_Vid, NALU_t *nalu);

	void CleanUpPPS(CVideoParameters *p_Vid);

	void activate_sps(CVideoParameters *p_Vid, seq_parameter_set_rbsp_t *sps);
	void activate_pps(CVideoParameters *p_Vid, pic_parameter_set_rbsp_t *pps);

	void UseParameterSet(CSlice *currSlice);

#if MVC_EXTENSION_ENABLE
	void SubsetSPSConsistencyCheck(subset_seq_parameter_set_rbsp_t *subset_sps);
	void ProcessSubsetSPS(CVideoParameters *p_Vid, NALU_t *nalu);

	void mvc_vui_parameters_extension(MVCVUI_t *pMVCVUI, Bitstream *s);
	void seq_parameter_set_mvc_extension(subset_seq_parameter_set_rbsp_t *subset_sps, Bitstream *s);
	void init_subset_sps_list(subset_seq_parameter_set_rbsp_t *subset_sps_list, int iSize);
	void reset_subset_sps(subset_seq_parameter_set_rbsp_t *subset_sps);
	int  GetBaseViewId(CVideoParameters *p_Vid, subset_seq_parameter_set_rbsp_t **subset_sps);
	void get_max_dec_frame_buf_size(seq_parameter_set_rbsp_t *sps);
#endif


	// exported functions
	// quantization initialization
	void init_qp_process (CodingParameters *cps);
	void free_qp_matrices(CodingParameters *cps);

	// For Q-matrix
	void assign_quant_params   (CSlice *currslice);
	void CalculateQuant4x4Param(CSlice *currslice);
	void CalculateQuant8x8Param(CSlice *currslice);

public:
	void init_time(void);
	INT64T timenorm(INT64T cur_time);
	int WriteOneFrame(DecodedPicList *pDecPic, int hFileOutput0, int hFileOutput1, bool bOutputAllFrames);
//int GetOneFrame(DecodedPicList *pDecPic,uint8_t *outbuf);

	void DumpRTPHeader(RTPpacket_t *p);
	int  GetRTPNALU(NALU_t *nalu, int BitStreamFile);
	int  GetRTSPClientNALU(NALU_t *nalu, CRTSPClientSocket *);
	void OpenRTPFile (char *fn, int *p_BitStreamFile);
	void CloseRTPFile(int *p_BitStreamFile);

	int InterpretSPS(DataPartition *p, seq_parameter_set_rbsp_t *sps);
	int InterpretPPS(DataPartition *p, pic_parameter_set_rbsp_t *pps);
	int GetSPS(CRTSPClientSocket *sock,seq_parameter_set_rbsp_t *sps);
	int GetPPS(CRTSPClientSocket *sock,pic_parameter_set_rbsp_t *pps);

	uint32_t GetTimestamp() { return p_Vid->timestamp; }

private:
	CDecoderH264_MemoryMgr *p_Memory;

	friend class CDecoderH264_MemoryMgr;
	friend class CVideoParameters;
	friend class CMacroblock;
	};


#endif

