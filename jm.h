/*
   H.264 JM coder/decoder
		https://github.com/petrkalos/JM/tree/master
		GD/C  adapted 15/1/26
 */


#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/timeb.h>

#define JTRACE 1
#define TIMING_DISABLE

#define JM                  "19 (FRExt)"
#define VERSION             "19.0"
#define EXT_VERSION         "(FRExt)"

#define DUMP_DPB                  1    //!< Dump DPB info for debug purposes
#define PRINTREFLIST              1    //!< Print ref list info for debug purposes
#define PAIR_FIELDS_IN_OUTPUT     0    //!< Pair field pictures for output purposes
#define IMGTYPE                   0    //!< Define imgpel size type. 0 implies uint8_t (cannot handle >8 bit depths) and 1 implies uint16_t
#define ENABLE_FIELD_CTX          1    //!< Enables Field mode related context types for CABAC
#define ENABLE_HIGH444_CTX        1    //!< Enables High 444 profile context types for CABAC. 
#define ZEROSNR                   0    //!< PSNR computation method
#define ENABLE_OUTPUT_TONEMAPPING 1    //!< enable tone map the output if tone mapping SEI present
#define JCOST_CALC_SCALEUP        1    //!< 1: J = (D<<LAMBDA_ACCURACY_BITS)+Lambda*R; 0: J = D + ((Lambda*R+Rounding)>>LAMBDA_ACCURACY_BITS)
#define DISABLE_ERC               0    //!< Disable any error concealment processes
#define JM_PARALLEL_DEBLOCK       0    //!< Enables Parallel Deblocking
#define SIMULCAST_ENABLE          0    //!< to test the decoder

#define MVC_EXTENSION_ENABLE      1    //!< enable support for the Multiview High Profile
#define ENABLE_DEC_STATS          1    //!< enable decoder statistics collection

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
#define MB_PIXELS            256 // MB_BLOCK_SIZE * MB_BLOCK_SIZE
#define MB_PIXELS_SHIFT        8 // log2(MB_BLOCK_SIZE * MB_BLOCK_SIZE)
#define MB_BLOCK_SHIFT         4
#define BLOCK_MULTIPLE         4 // (MB_BLOCK_SIZE/BLOCK_SIZE)
#define MB_BLOCK_PARTITIONS   16 // (BLOCK_MULTIPLE * BLOCK_MULTIPLE)
#define BLOCK_CONTEXT         64 // (4 * MB_BLOCK_PARTITIONS)

// These variables relate to the subpel accuracy supported by the software (1/4)
#define BLOCK_SIZE_SP      16  // BLOCK_SIZE << 2
#define BLOCK_SIZE_8x8_SP  32  // BLOCK_SIZE8x8 << 2

//#include "typedefs.h"

#define SSE_MEMORY_ALIGNMENT      16
#if IMGTYPE == 0
typedef uint8_t   imgpel;           //!< pixel type
typedef uint16_t distpel;          //!< distortion type (for pixels)
typedef int32_t  distblk;          //!< distortion type (for Macroblock)
typedef int32_t  transpel;         //!< transformed coefficient type
#else
typedef uint16_t imgpel;
typedef uint32_t distpel;
typedef int64_t  distblk;
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
#define MAX_CODED_FRAME_SIZE 8000000         //!< bytes for one frame
#define MAX_NUM_DECSLICES  16
#define MAX_DEC_THREADS    16                  //16 core deocoding;
#define MCBUF_LUMA_PAD_X        32
#define MCBUF_LUMA_PAD_Y        12
#define MCBUF_CHROMA_PAD_X      16
#define MCBUF_CHROMA_PAD_Y      8
#define MAX_NUM_DPB_LAYERS      2

#define MAXIMUMVALUEOFcpb_cnt   32
typedef struct {
  unsigned int cpb_cnt_minus1;                                   // ue(v)
  unsigned int bit_rate_scale;                                   // u(4)
  unsigned int cpb_size_scale;                                   // u(4)
  unsigned int bit_rate_value_minus1 [MAXIMUMVALUEOFcpb_cnt];    // ue(v)
  unsigned int cpb_size_value_minus1 [MAXIMUMVALUEOFcpb_cnt];    // ue(v)
  unsigned int cbr_flag              [MAXIMUMVALUEOFcpb_cnt];    // u(1)
  unsigned int initial_cpb_removal_delay_length_minus1;          // u(5)
  unsigned int cpb_removal_delay_length_minus1;                  // u(5)
  unsigned int dpb_output_delay_length_minus1;                   // u(5)
  unsigned int time_offset_length;                               // u(5)
} hrd_parameters_t;

typedef struct {
  bool      aspect_ratio_info_present_flag;                   // u(1)
  uint8_t aspect_ratio_idc;                                 // u(8)
  uint16_t sar_width;                                      // u(16)
  uint16_t sar_height;                                     // u(16)
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


#define MAXnum_slice_groups_minus1  8
typedef struct {
  bool   Valid;                  // indicates the parameter set is valid
  unsigned int pic_parameter_set_id;                             // ue(v)
  unsigned int seq_parameter_set_id;                             // ue(v)
  bool   entropy_coding_mode_flag;                            // u(1)
  bool   transform_8x8_mode_flag;                             // u(1)

  bool   pic_scaling_matrix_present_flag;                     // u(1)
  uint8_t   pic_scaling_list_present_flag[12];                   // u(1)
  int       ScalingList4x4[6][16];                               // se(v)
  int       ScalingList8x8[6][64];                               // se(v)
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
  bool   weighted_pred_flag;                               // u(1)
  uint8_t weighted_bipred_idc;                              // u(2)
  int       pic_init_qp_minus26;                              // se(v)
  int       pic_init_qs_minus26;                              // se(v)
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

  uint8_t profile_idc;                                       // u(8)
  bool   constrained_set0_flag;                                // u(1)
  bool   constrained_set1_flag;                                // u(1)
  bool   constrained_set2_flag;                                // u(1)
  bool   constrained_set3_flag;                                // u(1)
#if (MVC_EXTENSION_ENABLE)
  bool   constrained_set4_flag;                                // u(1)
  bool   constrained_set5_flag;                                // u(2)
#endif
  uint8_t level_idc;                                        // u(8)
  unsigned  int seq_parameter_set_id;                             // ue(v)
  unsigned  int chroma_format_idc;                                // ue(v)

  bool   seq_scaling_matrix_present_flag;                   // u(1)
  uint8_t seq_scaling_list_present_flag[12];                 // u(1)
  int       ScalingList4x4[6][16];                             // se(v)
  int       ScalingList8x8[6][64];                             // se(v)
  bool   UseDefaultScalingMatrix4x4Flag[6];
  bool   UseDefaultScalingMatrix8x8Flag[6];

  unsigned int bit_depth_luma_minus8;                            // ue(v)
  unsigned int bit_depth_chroma_minus8;                          // ue(v)
  unsigned int log2_max_frame_num_minus4;                        // ue(v)
  unsigned int pic_order_cnt_type;
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
  unsigned int pic_width_in_mbs_minus1;                          // ue(v)
  unsigned int pic_height_in_map_units_minus1;                   // ue(v)
  bool   frame_mbs_only_flag;                              // u(1)
  // if( !frame_mbs_only_flag )
  bool   mb_adaptive_frame_field_flag;                   // u(1)
  bool   direct_8x8_inference_flag;                        // u(1)
  bool   frame_cropping_flag;                              // u(1)
  unsigned int frame_crop_left_offset;                // ue(v)
  unsigned int frame_crop_right_offset;               // ue(v)
  unsigned int frame_crop_top_offset;                 // ue(v)
  unsigned int frame_crop_bottom_offset;              // ue(v)
  bool   vui_parameters_present_flag;                      // u(1)
  vui_seq_parameters_t vui_seq_parameters;                  // vui_seq_parameters_t
  uint8_t separate_colour_plane_flag;                       // u(1)
#if (MVC_EXTENSION_ENABLE)
  int max_dec_frame_buffering;
#endif
  int lossless_qpprime_flag;
} seq_parameter_set_rbsp_t;

#if (MVC_EXTENSION_ENABLE)
typedef struct mvcvui_tag {
  int num_ops_minus1;
  char *temporal_id;
  int *num_target_output_views_minus1;
  int **view_id;
  char *timing_info_present_flag;
  int *num_units_in_tick;
  int *time_scale;
  char *fixed_frame_rate_flag;
  char *nal_hrd_parameters_present_flag;
  char *vcl_hrd_parameters_present_flag;
  char *low_delay_hrd_flag;
  char *pic_struct_present_flag;

  //hrd parameters;
  char cpb_cnt_minus1;
  char bit_rate_scale;
  char cpb_size_scale;
  int bit_rate_value_minus1[32];
  int cpb_size_value_minus1[32];
  char cbr_flag[32];
  char initial_cpb_removal_delay_length_minus1;
  char cpb_removal_delay_length_minus1;
  char dpb_output_delay_length_minus1;
  char time_offset_length;
} MVCVUI_t;

typedef struct {
  seq_parameter_set_rbsp_t sps;

  unsigned int bit_equal_to_one;
  int num_views_minus1;
  int *view_id;
  int *num_anchor_refs_l0;
  int **anchor_ref_l0;
  int *num_anchor_refs_l1;
  int **anchor_ref_l1;

  int *num_non_anchor_refs_l0;
  int **non_anchor_ref_l0;
  int *num_non_anchor_refs_l1;
  int **non_anchor_ref_l1;
   
  int num_level_values_signalled_minus1;
  int *level_idc;
  int *num_applicable_ops_minus1;
  int **applicable_op_temporal_id;
  int **applicable_op_num_target_views_minus1;
  int ***applicable_op_target_view_id;
  int **applicable_op_num_views_minus1;

  unsigned int mvc_vui_parameters_present_flag;
  bool   Valid;                  // indicates the parameter set is valid
  MVCVUI_t  MVCVUIParams;
} subset_seq_parameter_set_rbsp_t;

#endif

pic_parameter_set_rbsp_t *AllocPPS (void);
seq_parameter_set_rbsp_t *AllocSPS (void);

void FreePPS (pic_parameter_set_rbsp_t *pps);
void FreeSPS (seq_parameter_set_rbsp_t *sps);

int sps_is_equal(seq_parameter_set_rbsp_t *sps1, seq_parameter_set_rbsp_t *sps2);
int pps_is_equal(pic_parameter_set_rbsp_t *pps1, pic_parameter_set_rbsp_t *pps2);


typedef struct annex_b_struct {
  int  BitStreamFile;                //!< the bit stream file
  uint8_t *iobuffer;
  uint8_t *iobufferread;
  int bytesinbuffer;
  int is_eof;
  int iIOBufferSize;

  int IsFirstByteStreamNALU;
  int nextstartcodebytes;
  uint8_t *Buf;  
} ANNEXB_t;

#define MAX_PLANE       3

typedef struct {
  int16_t x;
  int16_t y;
} BlockPos;

//! cbp structure
typedef struct cbp_s {
  int64_t         blk     ;
  int64_t         bits    ;
  int64_t         bits_8x8;
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
  PAR_OF_RTP       //!< RTP packets in outfile
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
  SSE              = 0,
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
  int   available;
  int   mb_addr;
  int16_t x;
  int16_t y;
  int16_t pos_x;
  int16_t pos_y;
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
  int16_t mv_x;
  int16_t mv_y;
} MotionVector;

static const MotionVector zero_mv = {0, 0};

//! struct for context management
typedef struct {
  uint16_t state;         // index into state-table CP
  unsigned char  MPS;           // Least Probable Symbol 0/1 CP
  unsigned char dummy;          // for alignment
} BiContextType;

typedef BiContextType *BiContextTypePtr;

//! Macroblock
typedef struct macroblock_dec {
  struct slice       *p_Slice;                    //!< pointer to the current slice
  struct video_par   *p_Vid;                      //!< pointer to VideoParameters
  struct inp_par     *p_Inp;
  int                 mbAddrX;                    //!< current MB address
  int mbAddrA, mbAddrB, mbAddrC, mbAddrD;
  bool mbAvailA, mbAvailB, mbAvailC, mbAvailD;
  BlockPos mb;
  int block_x;
  int block_y;
  int block_y_aff;
  int pix_x;
  int pix_y;
  int pix_c_x;
  int pix_c_y;

  int subblock_x;
  int subblock_y;

  int           qp;                    //!< QP luma
  int           qpc[2];                //!< QP chroma
  int           qp_scaled[MAX_PLANE];  //!< QP scaled for all comps.
  bool       is_lossless;
  bool       is_intra_block;
  bool       is_v_block;
  int           DeblockCall;

  int16_t         slice_nr;
  bool          ei_flag;             //!< error indicator flag that enables concealment
  bool          dpl_flag;            //!< error indicator flag that signals a missing data partition
  int16_t         delta_quant;          //!< for rate control
  int16_t         list_offset;

  struct macroblock_dec   *mb_up;   //!< pointer to neighboring MB (CABAC)
  struct macroblock_dec   *mb_left; //!< pointer to neighboring MB (CABAC)

  struct macroblock_dec   *mbup;   // neighbors for loopfilter
  struct macroblock_dec   *mbleft; // neighbors for loopfilter

  // some storage of macroblock syntax elements for global access
  int16_t         mb_type;
  int16_t         mvd[2][BLOCK_MULTIPLE][BLOCK_MULTIPLE][2];      //!< indices correspond to [forw,backw][block_y][block_x][x,y]
  //int16_t         ****mvd;      //!< indices correspond to [forw,backw][block_y][block_x][x,y]
  int           cbp;
  CBPStructure  s_cbp[3];

  int           i16mode;
  char          b8mode[4];
  char          b8pdir[4];
  char          ipmode_DPCM;
  char          c_ipred_mode;       //!< chroma intra prediction mode
  bool          skip_flag;
  int16_t         DFDisableIdc;
  int16_t         DFAlphaC0Offset;
  int16_t         DFBetaOffset;

  bool       mb_field;
  //Flag for MBAFF deblocking;
  uint8_t          mixedModeEdgeFlag;

  // deblocking strength indices
  uint8_t strength_ver[4][4];
  uint8_t strength_hor[4][16];


  bool       luma_transform_size_8x8_flag;
  bool       NoMbPartLessThan8x8Flag;

  void (*itrans_4x4)(struct macroblock_dec *currMB, ColorPlane pl, int ioff, int joff);
  void (*itrans_8x8)(struct macroblock_dec *currMB, ColorPlane pl, int ioff, int joff);

  void (*GetMVPredictor) (struct macroblock_dec *currMB, PixelPos *block, 
    MotionVector *pmv, int16_t ref_frame, struct pic_motion_params **mv_info, int list, int mb_x, int mb_y, int blockshape_x, int blockshape_y);

  int  (*read_and_store_CBP_block_bit)  (struct macroblock_dec *currMB, DecodingEnvironmentPtr  dep_dp, int type);
  char (*readRefPictureIdx)             (struct macroblock_dec *currMB, struct syntaxelement_dec *currSE, struct datapartition_dec *dP, char b8mode, int list);

  void (*read_comp_coeff_4x4_CABAC)     (struct macroblock_dec *currMB, struct syntaxelement_dec *currSE, ColorPlane pl, int (*InvLevelScale4x4)[4], int qp_per, int cbp);
  void (*read_comp_coeff_8x8_CABAC)     (struct macroblock_dec *currMB, struct syntaxelement_dec *currSE, ColorPlane pl);

  void (*read_comp_coeff_4x4_CAVLC)     (struct macroblock_dec *currMB, ColorPlane pl, int (*InvLevelScale4x4)[4], int qp_per, int cbp, uint8_t **nzcoeff);
  void (*read_comp_coeff_8x8_CAVLC)     (struct macroblock_dec *currMB, ColorPlane pl, int (*InvLevelScale8x8)[8], int qp_per, int cbp, uint8_t **nzcoeff);
} Macroblock;

typedef struct coding_par {
  int layer_id;
  int profile_idc;
  int width;
  int height;
  int width_cr;                               //!< width chroma  
  int height_cr;                              //!< height chroma

  int pic_unit_bitsize_on_disk;
  int16_t bitdepth_luma;
  int16_t bitdepth_chroma;
  int bitdepth_scale[2];
  int bitdepth_luma_qp_scale;
  int bitdepth_chroma_qp_scale;
  unsigned int dc_pred_value_comp[MAX_PLANE]; //!< component value for DC prediction (depends on component pel bit depth)
  int max_pel_value_comp[MAX_PLANE];       //!< max value that one picture element (pixel) can take (depends on pic_unit_bitdepth)

  int yuv_format;
  int lossless_qpprime_flag;
  int num_blk8x8_uv;
  int num_uv_blocks;
  int num_cdc_coeff;
  int mb_cr_size_x;
  int mb_cr_size_y;
  int mb_cr_size_x_blk;
  int mb_cr_size_y_blk;
  int mb_cr_size;
  int mb_size[3][2];                         //!< component macroblock dimensions
  int mb_size_blk[3][2];                     //!< component macroblock dimensions 
  int mb_size_shift[3][2];
  
  int max_vmv_r;                             //!< maximum vertical motion vector range in luma quarter frame pixel units for the current level_idc
  int separate_colour_plane_flag;
  int ChromaArrayType;
  int max_frame_num;
  unsigned int PicWidthInMbs;
  unsigned int PicHeightInMapUnits;
  unsigned int FrameHeightInMbs;
  unsigned int FrameSizeInMbs;
  int iLumaPadX;
  int iLumaPadY;
  int iChromaPadX;
  int iChromaPadY;

  int subpel_x;
  int subpel_y;
  int shiftpel_x;
  int shiftpel_y;
  int total_scale;
  unsigned int oldFrameSizeInMbs;

  //padding info;
  void (*img2buf)  (imgpel** imgX, unsigned char* buf, int size_x, int size_y, int symbol_size_in_bytes, int crop_left, int crop_right, int crop_top, int crop_bottom, int iOutStride);
  int rgb_output;

  imgpel **imgY_ref;                              //!< reference frame find snr
  imgpel ***imgUV_ref;
  Macroblock *mb_data;               //!< array containing all MBs of a whole frame
  Macroblock *mb_data_JV[MAX_PLANE]; //!< mb_data to be used for 4:4:4 independent mode
  int8_t *intra_block;
  int8_t *intra_block_JV[MAX_PLANE];
  BlockPos *PicPos;  
  uint8_t **ipredmode;                  //!< prediction type [90][74]
  uint8_t **ipredmode_JV[MAX_PLANE];
  uint8_t ****nz_coeff;
  int **siblock;
  int **siblock_JV[MAX_PLANE];
  int *qp_per_matrix;
  int *qp_rem_matrix;
	} CodingParameters;

typedef struct layer_par {
  int layer_id;
  struct video_par *p_Vid;
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

  int     (*readSyntaxElement)(struct macroblock_dec *currMB, struct syntaxelement_dec *, struct datapartition_dec *);
          /*!< virtual function;
               actual method depends on chosen data partition and
               entropy coding method  */
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
  BiContextType  transform_size_contexts [NUM_TRANSFORM_SIZE_CTX];
  BiContextType  ipr_contexts [NUM_IPR_CTX];
  BiContextType  cipr_contexts[NUM_CIPR_CTX];
  BiContextType  cbp_contexts [3][NUM_CBP_CTX];
  BiContextType  bcbp_contexts[NUM_BLOCK_TYPES][NUM_BCBP_CTX];
  BiContextType  map_contexts [2][NUM_BLOCK_TYPES][NUM_MAP_CTX];
  BiContextType  last_contexts[2][NUM_BLOCK_TYPES][NUM_LAST_CTX];
  BiContextType  one_contexts [NUM_BLOCK_TYPES][NUM_ONE_CTX];
  BiContextType  abs_contexts [NUM_BLOCK_TYPES][NUM_ABS_CTX];
} TextureInfoContexts;

#if (MVC_EXTENSION_ENABLE)
typedef struct nalunitheadermvcext_tag {
   unsigned int non_idr_flag;
   unsigned int priority_id;
   unsigned int view_id;
   unsigned int temporal_id;
   unsigned int anchor_pic_flag;
   unsigned int inter_view_flag;
   unsigned int reserved_one_bit;
   unsigned int iPrefixNALU;
} NALUnitHeaderMVCExt_t;
#endif

typedef struct wp_params {
  int16_t weight[3];
  int16_t offset[3];
} WPParams;

//! Slice
typedef struct slice {
  struct video_par    *p_Vid;
  struct inp_par      *p_Inp;
  pic_parameter_set_rbsp_t *active_pps;
  seq_parameter_set_rbsp_t *active_sps;
  int svc_extension_flag;

  // dpb pointer
  struct decoded_picture_buffer *p_Dpb;

  //slice property;
  int idr_flag;
  int idr_pic_id;
  int nal_reference_idc;                       //!< nal_reference_idc from NAL unit
  int Transform8x8Mode;
  bool chroma444_not_separate;              //!< indicates chroma 4:4:4 coding with separate_colour_plane_flag equal to zero

  int toppoc;      //poc for this top field
  int bottompoc;   //poc of bottom field of frame
  int framepoc;    //poc of this frame

  //the following is for slice header syntax elements of poc
  // for poc mode 0.
  unsigned int pic_order_cnt_lsb;
  int delta_pic_order_cnt_bottom;
  // for poc mode 1.
  int delta_pic_order_cnt[2];

  // ////////////////////////
  // for POC mode 0:
  signed   int PicOrderCntMsb;

  //signed   int PrevPicOrderCntMsb;
  //unsigned int PrevPicOrderCntLsb;

  // for POC mode 1:
  unsigned int AbsFrameNum;
  int ThisPOC;
  //signed int ExpectedPicOrderCnt, PicOrderCntCycleCnt, FrameNumInPicOrderCntCycle;
  //unsigned int PreviousFrameNum, FrameNumOffset;
  //int ExpectedDeltaPerPicOrderCntCycle;
  //int PreviousFrameNumOffset;
  // /////////////////////////

  //information need to move to slice;
  unsigned int current_mb_nr; // bitstream order
  unsigned int num_dec_mb;
  int16_t        current_slice_nr;
  //int mb_x;
  //int mb_y;
  //int block_x;
  //int block_y;
  //int pix_c_x;
  //int pix_c_y;
  int cod_counter;                   //!< Current count of number of skipped macroblocks in a row
  int allrefzero;
  //end;

  int                 mb_aff_frame_flag;
  int                 direct_spatial_mv_pred_flag;       //!< Indicator for direct mode type (1 for Spatial, 0 for Temporal)
  int                 num_ref_idx_active[2];             //!< number of available list references
  //int                 num_ref_idx_l0_active;             //!< number of available list 0 references
  //int                 num_ref_idx_l1_active;             //!< number of available list 1 references

  int                 ei_flag;       //!< 0 if the partArr[0] contains valid information
  int                 qp;
  int                 slice_qp_delta;
  int                 qs;
  int                 slice_qs_delta;
  int                 slice_type;    //!< slice type
  int                 model_number;  //!< cabac model number
  unsigned int        frame_num;   //frame_num for this frame
  unsigned int        field_pic_flag;
  uint8_t                bottom_field_flag;
  PictureStructure    structure;     //!< Identify picture structure type
  int                 start_mb_nr;   //!< MUST be set by NAL even in case of ei_flag == 1
  int                 end_mb_nr_plus1;
  int                 max_part_nr;
  int                 dp_mode;       //!< data partitioning mode
  int                 current_header;
  int                 next_header;
  int                 last_dquant;

  //slice header information;
  int colour_plane_id;               //!< colour_plane_id of the current coded slice
  int redundant_pic_cnt;
  int sp_switch;                              //!< 1 for switching sp, 0 for normal sp  
  int slice_group_change_cycle;
  int redundant_slice_ref_idx;     //!< reference index of redundant slice
  int no_output_of_prior_pics_flag;
  int long_term_reference_flag;
  int adaptive_ref_pic_buffering_flag;
  DecRefPicMarking_t *dec_ref_pic_marking_buffer;                    //!< stores the memory management control operations

  char listXsize[6];
  struct storable_picture **listX[6];

  //  int                 last_mb_nr;    //!< only valid when entropy coding == CABAC
  DataPartition       *partArr;      //!< array of partitions
  MotionInfoContexts  *mot_ctx;      //!< pointer to struct of context models for use in CABAC
  TextureInfoContexts *tex_ctx;      //!< pointer to struct of context models for use in CABAC

  int mvscale[6][MAX_REFERENCE_PICTURES];

  int                 ref_pic_list_reordering_flag[2];
  int                 *modification_of_pic_nums_idc[2];
  int                 *abs_diff_pic_num_minus1[2];
  int                 *long_term_pic_idx[2];

#if (MVC_EXTENSION_ENABLE)
  int                 *abs_diff_view_idx_minus1[2];

  int                 view_id;
  int                 inter_view_flag;
  int                 anchor_pic_flag;

  NALUnitHeaderMVCExt_t NaluHeaderMVCExt;
#endif
  int                 layer_id;
  int16_t               DFDisableIdc;     //!< Disable deblocking filter on slice
  int16_t               DFAlphaC0Offset;  //!< Alpha and C0 offset for filtering slice
  int16_t               DFBetaOffset;     //!< Beta offset for filtering slice

  int                 pic_parameter_set_id;   //!<the ID of the picture parameter set the slice is reffering to

  int                 dpB_NotPresent;    //!< non-zero, if data partition B is lost
  int                 dpC_NotPresent;    //!< non-zero, if data partition C is lost

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
  int  InvLevelScale4x4_Intra[3][6][4][4];
  int  InvLevelScale4x4_Inter[3][6][4][4];
  int  InvLevelScale8x8_Intra[3][6][8][8];
  int  InvLevelScale8x8_Inter[3][6][8][8];

  int  *qmatrix[12];

  // Cabac
  int  coeff[64]; // one more for EOB
  int  coeff_ctr;
  int  pos;  


  //weighted prediction
  uint16_t weighted_pred_flag;
  uint16_t weighted_bipred_idc;

  uint16_t luma_log2_weight_denom;
  uint16_t chroma_log2_weight_denom;
  
  WPParams **wp_params; // wp parameters in [list][index]

  int ***wp_weight;  // weight in [list][index][component] order
  int ***wp_offset;  // offset in [list][index][component] order
  int ****wbp_weight; //weight in [list][fw_index][bw_index][component] order
  int16_t wp_round_luma;
  int16_t wp_round_chroma;

#if (MVC_EXTENSION_ENABLE)
  int listinterviewidx0;
  int listinterviewidx1;
  struct frame_store **fs_listinterview0;
  struct frame_store **fs_listinterview1;
#endif

  // for signalling to the neighbour logic that this is a deblocker call
  //uint8_t mixedModeEdgeFlag;
  int max_mb_vmv_r;                          //!< maximum vertical motion vector range in luma quarter pixel units for the current level_idc
  int ref_flag[17];                //!< 0: i-th previous frame is incorrect

  int erc_mvperMB;
  Macroblock *mb_data;
  struct storable_picture *dec_picture;
  int **siblock;
  uint8_t **ipredmode;
  int8_t *intra_block;
  char  chroma_vector_adjustment[6][32];
  void (*read_CBP_and_coeffs_from_NAL) (Macroblock *currMB);
  int  (*decode_one_component     )    (Macroblock *currMB, ColorPlane curr_plane, imgpel **currImg, struct storable_picture *dec_picture);
  int  (*readSlice                )    (struct video_par *, struct inp_par *);  
  int  (*nal_startcode_follows    )    (struct slice*, int );
  void (*read_motion_info_from_NAL)    (Macroblock *currMB);
  void (*read_one_macroblock      )    (Macroblock *currMB);
  void (*interpret_mb_mode        )    (Macroblock *currMB);
  void (*init_lists               )    (struct slice *currSlice);

  void (*intra_pred_chroma        )    (Macroblock *currMB);
  int  (*intra_pred_4x4)               (Macroblock *currMB, ColorPlane pl, int ioff, int joff,int i4,int j4);
  int  (*intra_pred_8x8)               (Macroblock *currMB, ColorPlane pl, int ioff, int joff);
  int  (*intra_pred_16x16)             (Macroblock *currMB, ColorPlane pl, int predmode);

  void (*linfo_cbp_intra          )    (int len, int info, int *cbp, int *dummy);
  void (*linfo_cbp_inter          )    (int len, int info, int *cbp, int *dummy);    
  void (*update_direct_mv_info    )    (Macroblock *currMB);
  void (*read_coeff_4x4_CAVLC     )    (Macroblock *currMB, int block_type, int i, int j, int levarr[16], int runarr[16], int *number_coefficients);
	} Slice;

#define  TIMEB    timeb
#define  TIME_T   struct timeval
#define  OPENFLAGS_WRITE OF_WRITE //_O_WRONLY|_O_CREAT|_O_BINARY|_O_TRUNC
#define  OPEN_PERMISSIONS _S_IREAD | _S_IWRITE
#define  OPENFLAGS_READ  OF_READ //_O_RDONLY|_O_BINARY
#define FORMAT_OFF_T "I64d"

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
  V210       =  4      //!< Video Clarity 422 format (10 bits)
} PixelFormat;

typedef struct frame_format {  
  ColorFormat yuv_format;                    //!< YUV format (0=4:0:0, 1=4:2:0, 2=4:2:2, 3=4:4:4)
  ColorModel  color_model;                   //!< 4:4:4 format (0: YUV, 1: RGB, 2: XYZ)
  PixelFormat pixel_format;                  //!< pixel format support for certain interleaved yuv sources
  double      frame_rate;                    //!< frame rate
  int         width[3];                      //!< component frame width
  int         height[3];                     //!< component frame height    
  int         auto_crop_right;               //!< luma component auto crop right
  int         auto_crop_bottom;              //!< luma component auto crop bottom
  int         auto_crop_right_cr;            //!< chroma component auto crop right
  int         auto_crop_bottom_cr;           //!< chroma component auto crop bottom
  int         width_crop;                    //!< width after cropping consideration
  int         height_crop;                   //!< height after cropping consideration
  int         mb_width;                      //!< luma component frame width
  int         mb_height;                     //!< luma component frame height    
  int         size_cmp[3];                   //!< component sizes (width * height)
  int         size;                          //!< total image size (sum of size_cmp)
  int         bit_depth[3];                  //!< component bit depth  
  int         max_value[3];                  //!< component max value
  int         max_value_sq[3];               //!< component max value squared
  int         pic_unit_size_on_disk;         //!< picture sample unit size on storage medium
  int         pic_unit_size_shift3;          //!< pic_unit_size_on_disk >> 3
} FrameFormat;


typedef struct decodedpic_t {
  int bValid;                 //0: invalid, 1: valid, 3: valid for 3D output;
  int iViewId;                //-1: single view, >=0 multiview[VIEW1|VIEW0];
  int iPOC;
  int iYUVFormat;             //0: 4:0:0, 1: 4:2:0, 2: 4:2:2, 3: 4:4:4
  int iYUVStorageFormat;      //0: YUV seperate; 1: YUV interleaved; 2: 3D output;
  int iBitDepth;
  uint8_t *pY;                   //if iPictureFormat is 1, [0]: top; [1] bottom;
  uint8_t *pU;
  uint8_t *pV;
  int iWidth;                 //frame width;              
  int iHeight;                //frame height;
  int iYBufStride;            //stride of pY[0/1] buffer in bytes;
  int iUVBufStride;           //stride of pU[0/1] and pV[0/1] buffer in bytes;
  int iSkipPicNum;
  int iBufSize;
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

  int frm_stride[MAX_PLANE];
  int top_stride[MAX_PLANE];
  int bot_stride[MAX_PLANE];
} ImageData;

	// video parameters
typedef struct video_par {
  struct inp_par      *p_Inp;
  pic_parameter_set_rbsp_t *active_pps;
  seq_parameter_set_rbsp_t *active_sps;
  seq_parameter_set_rbsp_t SeqParSet[MAXSPS];
  pic_parameter_set_rbsp_t PicParSet[MAXPPS];
  struct decoded_picture_buffer *p_Dpb_layer[MAX_NUM_DPB_LAYERS];
  CodingParameters *p_EncodePar[MAX_NUM_DPB_LAYERS];
  LayerParameters *p_LayerPar[MAX_NUM_DPB_LAYERS];

#if (MVC_EXTENSION_ENABLE)
  subset_seq_parameter_set_rbsp_t *active_subset_sps;
  //int svc_extension_flag;
  subset_seq_parameter_set_rbsp_t SubsetSeqParSet[MAXSPS];
  int last_pic_width_in_mbs_minus1;
  int last_pic_height_in_map_units_minus1;
  int last_max_dec_frame_buffering;
  int last_profile_idc;
#endif

  struct sei_params        *p_SEI;

  struct old_slice_par *old_slice;
  struct snr_par       *snr;
  int number;                                 //!< frame number
  
  //current picture property;
  unsigned int num_dec_mb;
  int iSliceNumOfCurrPic;
  int iNumOfSlicesAllocated;
  int iNumOfSlicesDecoded;
  Slice **ppSliceList;
  int8_t *intra_block;
  int8_t *intra_block_JV[MAX_PLANE];
  //int qp;                                     //!< quant for the current frame

  //int sp_switch;                              //!< 1 for switching sp, 0 for normal sp  
  int type;                                   //!< image type INTER/INTRA

  uint8_t **ipredmode;                  //!< prediction type [90][74]
  uint8_t **ipredmode_JV[MAX_PLANE];
  uint8_t ****nz_coeff;
  int **siblock;
  int **siblock_JV[MAX_PLANE];
  BlockPos *PicPos;

  int newframe;
  int structure;                     //!< Identify picture structure type

  //Slice      *currentSlice;          //!< pointer to current Slice data struct
  Slice      *pNextSlice;             //!< pointer to first Slice of next picture;
  Macroblock *mb_data;               //!< array containing all MBs of a whole frame
  Macroblock *mb_data_JV[MAX_PLANE]; //!< mb_data to be used for 4:4:4 independent mode
  //int colour_plane_id;               //!< colour_plane_id of the current coded slice
  int ChromaArrayType;

  // picture error concealment
  // concealment_head points to first node in list, concealment_end points to
  // last node in list. Initialize both to NULL, meaning no nodes in list yet
  struct concealment_node *concealment_head;
  struct concealment_node *concealment_end;

  unsigned int pre_frame_num;           //!< store the frame_num in the last decoded slice. For detecting gap in frame_num.
  int non_conforming_stream;

  // ////////////////////////
  // for POC mode 0:
  signed   int PrevPicOrderCntMsb;
  unsigned int PrevPicOrderCntLsb;

  // for POC mode 1:
  signed int ExpectedPicOrderCnt, PicOrderCntCycleCnt, FrameNumInPicOrderCntCycle;
  unsigned int PreviousFrameNum, FrameNumOffset;
  int ExpectedDeltaPerPicOrderCntCycle;
  int ThisPOC;
  int PreviousFrameNumOffset;
  // /////////////////////////

  unsigned int PicHeightInMbs;
  unsigned int PicSizeInMbs;

  int no_output_of_prior_pics_flag;

  int last_has_mmco_5;
  int last_pic_bottom_field;

  int idr_psnr_number;
  int psnr_number;

  // Timing related variables
  TIME_T start_time;
  TIME_T end_time;

  // picture error concealment
  int last_ref_pic_poc;
  int ref_poc_gap;
  int poc_gap;
  int conceal_mode;
  int earlier_missing_poc;
  unsigned int frame_to_conceal;
  int IDR_concealment_flag;
  int conceal_slice_type;

  bool first_sps;
  // random access point decoding
  int recovery_point;
  int recovery_point_found;
  int recovery_frame_cnt;
  int recovery_frame_num;
  int recovery_poc;

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
  int Is_primary_correct;          //!< if primary frame is correct, 0: incorrect
  int Is_redundant_correct;        //!< if redundant frame is correct, 0:incorrect

  // Time 
  int64_t tot_time;

  // files
  int p_out;                       //!< file descriptor to output YUV file
#if (MVC_EXTENSION_ENABLE)
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

  int *qp_per_matrix;
  int *qp_rem_matrix;

  struct frame_store *last_out_fs;
  int pocs_in_dpb[100];

  struct storable_picture *dec_picture;
  struct storable_picture *dec_picture_JV[MAX_PLANE];  //!< dec_picture to be used during 4:4:4 independent mode decoding
  struct storable_picture *no_reference_picture; //!< dummy storable picture for recovery point

  // Error parameters
  struct object_buffer  *erc_object_list;
  struct ercVariables_s *erc_errorVar;

  int erc_mvperMB;
  struct video_par *erc_img;
  int ec_flag[SE_MAX_ELEMENTS];        //!< array to set errorconcealment

  struct annex_b_struct *annex_b;

  struct frame_store *out_buffer;

  struct storable_picture *pending_output;
  int    pending_output_state;
  int    recovery_flag;

  int BitStreamFile;

  // report
  char cslice_type[9];  
  // FMO
  int *MbToSliceGroupMap;
  int *MapUnitToSliceGroupMap;
  int  NumberOfSliceGroups;    // the number of slice groups -1 (0 == scan order, 7 == maximum)

#if (ENABLE_OUTPUT_TONEMAPPING)
  struct tone_mapping_struct_s *seiToneMapping;
#endif

  void (*buf2img)          (imgpel** imgX, unsigned char* buf, int size_x, int size_y, int o_size_x, int o_size_y, int symbol_size_in_bytes, int bitshift);
  void (*getNeighbour)     (Macroblock *currMB, int xN, int yN, int mb_size[2], PixelPos *pix);
  void (*get_mb_block_pos) (BlockPos *PicPos, int mb_addr, int16_t *x, int16_t *y);
  void (*GetStrengthVer)   (Macroblock *MbQ, int edge, int mvlimit, struct storable_picture *p);
  void (*GetStrengthHor)   (Macroblock *MbQ, int edge, int mvlimit, struct storable_picture *p);
  void (*EdgeLoopLumaVer)  (ColorPlane pl, imgpel** Img, uint8_t *Strength, Macroblock *MbQ, int edge);
  void (*EdgeLoopLumaHor)  (ColorPlane pl, imgpel** Img, uint8_t *Strength, Macroblock *MbQ, int edge, struct storable_picture *p);
  void (*EdgeLoopChromaVer)(imgpel** Img, uint8_t *Strength, Macroblock *MbQ, int edge, int uv, struct storable_picture *p);
  void (*EdgeLoopChromaHor)(imgpel** Img, uint8_t *Strength, Macroblock *MbQ, int edge, int uv, struct storable_picture *p);
  void (*img2buf)          (imgpel** imgX, unsigned char* buf, int size_x, int size_y, int symbol_size_in_bytes, int crop_left, int crop_right, int crop_top, int crop_bottom, int iOutStride);

  ImageData tempData3;
  DecodedPicList *pDecOuputPic;
  int iDeblockMode;  //0: deblock in picture, 1: deblock in slice;
  struct nalu_t *nalu;
  int iLumaPadX;
  int iLumaPadY;
  int iChromaPadX;
  int iChromaPadY;
  //control;
  int bDeblockEnable;
  int iPostProcess;
  int bFrameInit;
#if _FLTDBG_
  FILE *fpDbg;
#endif
  pic_parameter_set_rbsp_t *pNextPPS;
  int last_dec_poc;
  int last_dec_view_id;
  int last_dec_layer_id;
  int dpb_layer_id;

/******************* deprecative variables; ***************************************/
  int width;
  int height;
  int width_cr;                               //!< width chroma  
  int height_cr;                              //!< height chroma
  // Fidelity Range Extensions Stuff
  int pic_unit_bitsize_on_disk;
  int16_t bitdepth_luma;
  int16_t bitdepth_chroma;
  int bitdepth_scale[2];
  int bitdepth_luma_qp_scale;
  int bitdepth_chroma_qp_scale;
  unsigned int dc_pred_value_comp[MAX_PLANE]; //!< component value for DC prediction (depends on component pel bit depth)
  int max_pel_value_comp[MAX_PLANE];       //!< max value that one picture element (pixel) can take (depends on pic_unit_bitdepth)

  int separate_colour_plane_flag;
  int pic_unit_size_on_disk;

  int profile_idc;
  int yuv_format;
  int lossless_qpprime_flag;
  int num_blk8x8_uv;
  int num_uv_blocks;
  int num_cdc_coeff;
  int mb_cr_size_x;
  int mb_cr_size_y;
  int mb_cr_size_x_blk;
  int mb_cr_size_y_blk;
  int mb_cr_size;
  int mb_size[3][2];                         //!< component macroblock dimensions
  int mb_size_blk[3][2];                     //!< component macroblock dimensions 
  int mb_size_shift[3][2];
  int subpel_x;
  int subpel_y;
  int shiftpel_x;
  int shiftpel_y;
  int total_scale;
  int max_frame_num;

  unsigned int PicWidthInMbs;
  unsigned int PicHeightInMapUnits;
  unsigned int FrameHeightInMbs;
  unsigned int FrameSizeInMbs;
  unsigned int oldFrameSizeInMbs;
  int max_vmv_r;                             //!< maximum vertical motion vector range in luma quarter frame pixel units for the current level_idc
  //int max_mb_vmv_r;                        //!< maximum vertical motion vector range in luma quarter pixel units for the current level_idc
/******************* end deprecative variables; ***************************************/

  struct dec_stat_parameters *dec_stats;
	} VideoParameters;

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
#if (MVC_EXTENSION_ENABLE)
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
  int       forbidden_bit;         //!< should be always FALSE
  NaluType  nal_unit_type;         //!< NALU_TYPE_xxxx
  NalRefIdc nal_reference_idc;     //!< NALU_PRIORITY_xxxx  
  uint8_t     *buf;                   //!< contains the first uint8_t followed by the EBSP
  uint16_t    lost_packets;          //!< true, if packet loss is detected
#if (MVC_EXTENSION_ENABLE)
  int       svc_extension_flag;    //!< should be always 0, for MVC
  int       non_idr_flag;          //!< 0 = current is IDR
  int       priority_id;           //!< a lower value of priority_id specifies a higher priority
  int       view_id;               //!< view identifier for the NAL unit
  int       temporal_id;           //!< temporal identifier for the NAL unit
  int       anchor_pic_flag;       //!< anchor access unit
  int       inter_view_flag;       //!< inter-view prediction enable
  int       reserved_one_bit;      //!< shall be equal to 1
#endif
} NALU_t;

//! allocate one NAL Unit
extern NALU_t *AllocNALU(int);

//! free one NAL Unit
extern void FreeNALU(NALU_t *n);

#if (MVC_EXTENSION_ENABLE)
extern void nal_unit_header_svc_extension();
extern void prefix_nal_unit_svc();
#endif

extern int  get_annex_b_NALU (VideoParameters *p_Vid, NALU_t *nalu, ANNEXB_t *annex_b);

extern void open_annex_b     (char *fn, ANNEXB_t *annex_b);
extern void close_annex_b    (ANNEXB_t *annex_b);
extern void malloc_annex_b   (VideoParameters *p_Vid, ANNEXB_t **p_annex_b);
extern void free_annex_b     (ANNEXB_t **p_annex_b);
extern void init_annex_b     (ANNEXB_t *annex_b);
extern void reset_annex_b    (ANNEXB_t *annex_b);






extern void arideco_start_decoding(DecodingEnvironmentPtr eep, unsigned char *code_buffer, int firstbyte, int *code_len);
extern int  arideco_bits_read(DecodingEnvironmentPtr dep);
extern void arideco_done_decoding(DecodingEnvironmentPtr dep);
extern void biari_init_context (int qp, BiContextTypePtr ctx, const char* ini);
extern unsigned int biari_decode_symbol(DecodingEnvironment *dep, BiContextType *bi_ct );
extern unsigned int biari_decode_symbol_eq_prob(DecodingEnvironmentPtr dep);
extern unsigned int biari_decode_final(DecodingEnvironmentPtr dep);



//#include "global.h"
//#include "transform8x8.h"


extern void iMBtrans4x4(Macroblock *currMB, ColorPlane pl, int smb);
extern void iMBtrans8x8(Macroblock *currMB, ColorPlane pl);

extern void itrans_sp_cr(Macroblock *currMB, int uv);

extern void Inv_Residual_trans_4x4(Macroblock *currMB, ColorPlane pl, int ioff, int joff);
extern void Inv_Residual_trans_8x8(Macroblock *currMB, ColorPlane pl, int ioff,int joff);
extern void Inv_Residual_trans_16x16 (Macroblock *currMB, ColorPlane pl);
extern void Inv_Residual_trans_Chroma(Macroblock *currMB, int uv);

extern void itrans4x4   (Macroblock *currMB, ColorPlane pl, int ioff, int joff);
extern void itrans4x4_ls(Macroblock *currMB, ColorPlane pl, int ioff, int joff);
extern void itrans_sp   (Macroblock *currMB, ColorPlane pl, int ioff, int joff);
extern void itrans_2    (Macroblock *currMB, ColorPlane pl);
extern void iTransform  (Macroblock *currMB, ColorPlane pl, int smb);

extern void copy_image_data       (imgpel  **imgBuf1, imgpel  **imgBuf2, int off1, int off2, int width, int height);
extern void copy_image_data_16x16 (imgpel  **imgBuf1, imgpel  **imgBuf2, int off1, int off2);
extern void copy_image_data_8x8   (imgpel  **imgBuf1, imgpel  **imgBuf2, int off1, int off2);
extern void copy_image_data_4x4   (imgpel  **imgBuf1, imgpel  **imgBuf2, int off1, int off2);
extern int CheckVertMV(Macroblock *currMB, int vec1_y, int block_size_y);



extern MotionInfoContexts*  create_contexts_MotionInfo(void);
extern TextureInfoContexts* create_contexts_TextureInfo(void);
extern void delete_contexts_MotionInfo(MotionInfoContexts *enco_ctx);
extern void delete_contexts_TextureInfo(TextureInfoContexts *enco_ctx);

extern void cabac_new_slice(Slice *currSlice);

//! Syntaxelement
typedef struct syntaxelement_dec {
  int           type;                  //!< type of syntax element for data part.
  int           value1;                //!< numerical value of syntax element
  int           value2;                //!< for blocked symbols, e.g. run/level
  int           len;                   //!< length of code
  int           inf;                   //!< info part of CAVLC code
  unsigned int  bitpattern;            //!< CAVLC bitpattern
  int           context;               //!< CABAC context
  int           k;                     //!< CABAC context for coeff_count,uv

#if JTRACE
  #define       TRACESTRING_SIZE 100           //!< size of trace string
  char          tracestring[TRACESTRING_SIZE]; //!< trace string
#endif

  //! for mapping of CAVLC to syntaxElement
  void  (*mapping)(int len, int info, int *value1, int *value2);
  //! used for CABAC: refers to actual coding method of each individual syntax element type
  void  (*reading)(struct macroblock_dec *currMB, struct syntaxelement_dec *, DecodingEnvironmentPtr);
} SyntaxElement;

extern void readMB_typeInfo_CABAC_i_slice   (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readMB_typeInfo_CABAC_p_slice   (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readMB_typeInfo_CABAC_b_slice   (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readB8_typeInfo_CABAC_p_slice   (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readB8_typeInfo_CABAC_b_slice   (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readIntraPredMode_CABAC         (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readRefFrame_CABAC              (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void read_MVD_CABAC                  (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void read_mvd_CABAC_mbaff            (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void read_CBP_CABAC                  (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readRunLevel_CABAC              (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void read_dQuant_CABAC               (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readCIPredMode_CABAC            (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void read_skip_flag_CABAC_p_slice    (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void read_skip_flag_CABAC_b_slice    (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readFieldModeInfo_CABAC         (Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);
extern void readMB_transform_size_flag_CABAC(Macroblock *currMB, SyntaxElement *se, DecodingEnvironmentPtr dep_dp);

extern void readIPCM_CABAC(Slice *currSlice, struct datapartition_dec *dP);

extern int  cabac_startcode_follows(Slice *currSlice, int eos_bit);

extern int  readSyntaxElement_CABAC         (Macroblock *currMB, SyntaxElement *se, DataPartition *this_dataPart);

extern int check_next_mb_and_get_field_mode_CABAC_p_slice( Slice *currSlice, SyntaxElement *se, DataPartition  *act_dp);
extern int check_next_mb_and_get_field_mode_CABAC_b_slice( Slice *currSlice, SyntaxElement *se, DataPartition  *act_dp);

extern void CheckAvailabilityOfNeighborsCABAC(Macroblock *currMB);

extern void set_read_and_store_CBP(Macroblock **currMB, int chroma_format_idc);



#define DEFAULTCONFIGFILENAME "decoder.cfg"

//#include "config_common.h"
//#define PROFILE_IDC     88
//#define LEVEL_IDC       21

#define FILE_NAME_SIZE  255

typedef struct video_size {
  char* name;
  int x_size;
  int y_size;
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
  int           is_concatenated;       //!< Single or multifile input?
  int           is_interleaved;        //!< Support for interleaved and non-interleaved input sources
  int           zero_pad;              //!< Used when separate image files are used as input. Enables zero padding for file numbering
  int           num_digits;            //!< Number of digits for file numbering
  int           start_frame;           //!< start frame
  int           end_frame;             //!< end frame
  int           nframes;               //!< number of frames
  int           crop_x_size;           //!< crop information (x component)
  int           crop_y_size;           //!< crop information (y component)
  int           crop_x_offset;         //!< crop offset (x component);
  int           crop_y_offset;         //!< crop offset (y component);

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

  int FileFormat;                         //!< File format of the Input file, PAR_OF_ANNEXB or PAR_OF_RTP
  int ref_offset;
  int poc_scale;
  int write_uv;
  int silent;
  int intra_profile_deblocking;               //!< Loop filter usage determined by flags and parameters in bitstream 

  // Input/output sequence format related variables
  FrameFormat source;                   //!< source related information
  FrameFormat output;                   //!< output related information

  int  ProcessInput;
  int  enable_32_pulldown;
  VideoDataFile input_file1;          //!< Input video file1
  VideoDataFile input_file2;          //!< Input video file2
  VideoDataFile input_file3;          //!< Input video file3
#if (MVC_EXTENSION_ENABLE)
  int  DecodeAllLayers;
#endif

#ifdef _LEAKYBUCKET_
  unsigned long R_decoder;                //!< Decoder Rate in HRD Model
  unsigned long B_decoder;                //!< Decoder Buffer size in HRD model
  unsigned long F_decoder;                //!< Decoder Initial buffer fullness in HRD model
  char LeakyBucketParamFile[FILE_NAME_SIZE];         //!< LeakyBucketParamFile
#endif

  // picture error concealment
  int conceal_mode;
  int ref_poc_gap;
  int poc_gap;


  // dummy for encoder
  int start_frame;

  // Needed to allow compilation for decoder. May be used later for distortion computation operations
  int stdRange;                         //!< 1 - standard range, 0 - full range
  int videoCode;                        //!< 1 - 709, 3 - 601:  See VideoCode in io_tiff.
  int export_views;
  
  int iDecFrmNum;

  int bDisplayDecParams;
  int dpb_plus[2];
} InputParameters;


typedef struct {
  char *TokenName;    //!< name
  void *Place;        //!< address
  int Type;           //!< type:  0-int, 1-char[], 2-double
  double Default;     //!< default value
  int param_limits;   //!< 0: no limits, 1: both min and max, 2: only min (i.e. no negatives), 3: special case for QPs since min needs bitdepth_qp_scale
  double min_limit;
  double max_limit;
  int    char_size;   //!< Dimension of type char[]
} Mapping;

//! Maps parameter name to its address, type etc.
extern char *GetConfigFileContent (char *Filename);
extern int  InitParams            (Mapping *Map);
extern int TestParams(Mapping *Map, int bitdepth_qp_scale[3]);
extern int DisplayParams(Mapping *Map, char *message);
extern void ParseContent          (InputParameters *p_Inp, Mapping *Map, char *buf, int bufsize);

extern Mapping Map[];

extern void JMDecHelpExit ();
extern void ParseCommand(InputParameters *p_Inp, int ac, char *av[]);





extern void  init_contexts  (Slice *currslice);


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


typedef struct dec_stat_parameters {
  int    frame_ctr           [NUM_SLICE_TYPES];          //!< Counter for different frame coding types (assumes one slice type per frame)
  int64_t  mode_use            [NUM_SLICE_TYPES][MAXMODE]; //!< Macroblock mode usage per slice
  int64_t  mode_use_transform  [NUM_SLICE_TYPES][MAXMODE][2];

  int64_t  *histogram_mv  [2][2];    //!< mv histogram (per list and per direction)
  int64_t  *histogram_refs[2];       //!< reference histogram (per list)
} DecStatParameters;

extern void init_dec_stats  (DecStatParameters *stats);
extern void delete_dec_stats(DecStatParameters *stats);



#ifdef TRACE
#undef TRACE
#endif
#if defined _DEBUG
# define TRACE           2     //!< 0:Trace off 1:Trace on 2:detailed CABAC context information
#else
# define TRACE           0     //!< 0:Trace off 1:Trace on 2:detailed CABAC context information
#endif



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

#define FILE_NAME_SIZE  255
#define INPUT_TEXT_SIZE 1024



// number of intra prediction modes
#define NO_INTRA_PMODE  9

// Direct Mode types
typedef enum {
  DIR_TEMPORAL = 0, //!< Temporal Direct Mode
  DIR_SPATIAL  = 1 //!< Spatial Direct Mode
} DirectModes;

// CAVLC block types
typedef enum {
  LUMA              =  0,
  LUMA_INTRA16x16DC =  1,
  LUMA_INTRA16x16AC =  2,
  CB                =  3,
  CB_INTRA16x16DC   =  4,
  CB_INTRA16x16AC   =  5,
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

// Macro defines
#define Q_BITS          15
#define DQ_BITS          6
#define Q_BITS_8        16
#define DQ_BITS_8        6 


#define IS_I16MB(MB)    ((MB)->mb_type==I16MB  || (MB)->mb_type==IPCM)
#define IS_DIRECT(MB)   ((MB)->mb_type==0     && (currSlice->slice_type == B_SLICE ))

#define TOTRUN_NUM       15
#define RUNBEFORE_NUM     7
#define RUNBEFORE_NUM_M1  6

// Quantization parameter range
#define MIN_QP          0
#define MAX_QP          51
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


enum {
  EOS = 1,    //!< End Of Sequence
  SOP = 2,    //!< Start Of Picture
  SOS = 3,     //!< Start Of Slice
  SOS_CONT = 4
};

// MV Prediction types
typedef enum {
  MVPRED_MEDIAN   = 0,
  MVPRED_L        = 1,
  MVPRED_U        = 2,
  MVPRED_UR       = 3
} MVPredTypes;

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


#define NO_EC               0   //!< no error concealment necessary
#define EC_REQ              1   //!< error concealment required
#define EC_SYNC             2   //!< search and sync on next header element

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


/*
* typedefs
*/

/* segment data structure */
typedef struct ercSegment_s {
  int16_t     startMBPos;
  int16_t     endMBPos;
  char      fCorrupted;
} ercSegment_t;

/* Error detector & concealment instance data structure */
typedef struct ercVariables_s {
  /*  Number of macroblocks (size or size/4 of the arrays) */
  int   nOfMBs;
  /* Number of segments (slices) in frame */
  int     nOfSegments;

  /*  Array for conditions of Y blocks */
  char     *yCondition;
  /*  Array for conditions of U blocks */
  char     *uCondition;
  /*  Array for conditions of V blocks */
  char     *vCondition;

  /* Array for Slice level information */
  ercSegment_t *segments;
  int     currSegment;

  /* Conditions of the MBs of the previous frame */
  char   *prevFrameYCondition;

  /* Flag telling if the current segment was found to be corrupted */
  int   currSegmentCorrupted;
  /* Counter for corrupted segments per picture */
  int   nOfCorruptedSegments;

  /* State variables for error detector and concealer */
  int   concealment;

} ercVariables_t;

/*
* External function interface
*/

void ercInit (VideoParameters *p_Vid, int pic_sizex, int pic_sizey, int flag);
ercVariables_t *ercOpen( void );
void ercReset( ercVariables_t *errorVar, int nOfMBs, int numOfSegments, int picSizeX );
void ercClose( VideoParameters *p_Vid, ercVariables_t *errorVar );
void ercSetErrorConcealment( ercVariables_t *errorVar, int value );

void ercStartSegment( int currMBNum, int segment, unsigned int bitPos, ercVariables_t *errorVar );
void ercStopSegment( int currMBNum, int segment, unsigned int bitPos, ercVariables_t *errorVar );
void ercMarkCurrSegmentLost(int picSizeX, ercVariables_t *errorVar );
void ercMarkCurrSegmentOK(int picSizeX, ercVariables_t *errorVar );
void ercMarkCurrMBConcealed( int currMBNum, int comp, int picSizeX, ercVariables_t *errorVar );

//! YUV pixel domain image arrays for a video frame
typedef struct frame_s {
  VideoParameters *p_Vid;
  imgpel *yptr;
  imgpel *uptr;
  imgpel *vptr;
	} frame;

int ercConcealIntraFrame( VideoParameters *p_Vid, frame *recfr, int picSizeX, int picSizeY, ercVariables_t *errorVar );

//! region structure stores information about a region that is needed for concealment
typedef struct object_buffer {
  uint8_t regionMode;  //!< region mode as above
  int xMin;         //!< X coordinate of the pixel position of the top-left corner of the region
  int yMin;         //!< Y coordinate of the pixel position of the top-left corner of the region
  int mv[3];        //!< motion vectors in 1/4 pixel units: mvx = mv[0], mvy = mv[1],
                    //!< and ref_frame = mv[2]
} objectBuffer_t;

int ercConcealInterFrame( frame *recfr, objectBuffer_t *object_list,
                          int picSizeX, int picSizeY, ercVariables_t *errorVar, int chroma_format_idc );


/* Thomson APIs for concealing entire frame loss */

//#include "mbuffer.h"
//#include "output.h"

//! definition of pic motion parameters
typedef struct pic_motion_params_old {
  uint8_t *      mb_field;      //!< field macroblock indicator
	} PicMotionParamsOld;

//! definition a picture (field or frame)
typedef struct storable_picture {
  PictureStructure structure;

  int         poc;
  int         top_poc;
  int         bottom_poc;
  int         frame_poc;
  unsigned int  frame_num;
  unsigned int  recovery_frame;

  int         pic_num;
  int         long_term_pic_num;
  int         long_term_frame_idx;

  uint8_t        is_long_term;
  int         used_for_reference;
  int         is_output;
  int         non_existing;
  int         separate_colour_plane_flag;

  int16_t       max_slice_id;

  int         size_x, size_y, size_x_cr, size_y_cr;
  int         size_x_m1, size_y_m1, size_x_cr_m1, size_y_cr_m1;
  int         coded_frame;
  int         mb_aff_frame_flag;
  unsigned    PicWidthInMbs;
  unsigned    PicSizeInMbs;
  int         iLumaPadY, iLumaPadX;
  int         iChromaPadY, iChromaPadX;


  imgpel **     imgY;         //!< Y picture component
  imgpel ***    imgUV;        //!< U and V picture components

  struct pic_motion_params **mv_info;          //!< Motion info
  struct pic_motion_params **JVmv_info[MAX_PLANE];          //!< Motion info

  struct pic_motion_params_old  motion;              //!< Motion info  
  struct pic_motion_params_old  JVmotion[MAX_PLANE]; //!< Motion info for 4:4:4 independent mode decoding

  struct storable_picture *top_field;     // for mb aff, if frame for referencing the top field
  struct storable_picture *bottom_field;  // for mb aff, if frame for referencing the bottom field
  struct storable_picture *frame;         // for mb aff, if field for referencing the combined frame

  int         slice_type;
  int         idr_flag;
  int         no_output_of_prior_pics_flag;
  int         long_term_reference_flag;
  int         adaptive_ref_pic_buffering_flag;

  int         chroma_format_idc;
  int         frame_mbs_only_flag;
  int         frame_cropping_flag;
  int         frame_crop_left_offset;
  int         frame_crop_right_offset;
  int         frame_crop_top_offset;
  int         frame_crop_bottom_offset;
  int         qp;
  int         chroma_qp_offset[2];
  int         slice_qp_delta;
  DecRefPicMarking_t *dec_ref_pic_marking_buffer;                    //!< stores the memory management control operations

  // picture error concealment
  int         concealed_pic; //indicates if this is a concealed picture
  
  // variables for tone mapping
  int         seiHasTone_mapping;
  int         tone_mapping_model_id;
  int         tonemapped_bit_depth;  
  imgpel*     tone_mapping_lut;                //!< tone mapping look up table

  int         proc_flag;
#if (MVC_EXTENSION_ENABLE)
  int         view_id;
  int         inter_view_flag;
  int         anchor_pic_flag;
#endif
  int         iLumaStride;
  int         iChromaStride;
  int         iLumaExpandedHeight;
  int         iChromaExpandedHeight;
  imgpel **cur_imgY; // for more efficient get_block_luma
  int no_ref;
  int iCodingType;
  //
  char listXsize[MAX_NUM_SLICES][2];
  struct storable_picture **listX[MAX_NUM_SLICES][2];
  int         layer_id;
} StorablePicture;

typedef StorablePicture *StorablePicturePtr;

struct concealment_node {
    StorablePicture* picture;
    int  missingpocs;
    struct concealment_node *next;
};

extern struct concealment_node * init_node(StorablePicture* , int );
extern void print_node( struct concealment_node * );
extern void print_list( struct concealment_node * );


//! Frame Stores for Decoded Picture Buffer
typedef struct frame_store {
  int       is_used;                //!< 0=empty; 1=top; 2=bottom; 3=both fields (or frame)
  int       is_reference;           //!< 0=not used for ref; 1=top used; 2=bottom used; 3=both fields (or frame) used
  int       is_long_term;           //!< 0=not used for ref; 1=top used; 2=bottom used; 3=both fields (or frame) used
  int       is_orig_reference;      //!< original marking by nal_ref_idc: 0=not used for ref; 1=top used; 2=bottom used; 3=both fields (or frame) used

  int       is_non_existent;

  unsigned  frame_num;
  unsigned  recovery_frame;

  int       frame_num_wrap;
  int       long_term_frame_idx;
  int       is_output;
  int       poc;

  // picture error concealment
  int concealment_reference;

  StorablePicture *frame;
  StorablePicture *top_field;
  StorablePicture *bottom_field;

#if (MVC_EXTENSION_ENABLE)
  int       view_id;
  int       inter_view_flag[2];
  int       anchor_pic_flag[2];
#endif
  int       layer_id;
} FrameStore;

//! Decoded Picture Buffer
typedef struct decoded_picture_buffer {
  VideoParameters *p_Vid;
  InputParameters *p_Inp;
  FrameStore  **fs;
  FrameStore  **fs_ref;
  FrameStore  **fs_ltref;
  FrameStore  **fs_ilref; // inter-layer reference (for multi-layered codecs)
  unsigned      size;
  unsigned      used_size;
  unsigned      ref_frames_in_buffer;
  unsigned      ltref_frames_in_buffer;
  int           last_output_poc;
#if (MVC_EXTENSION_ENABLE)
  int           last_output_view_id;
#endif
  int           max_long_term_pic_idx;  


  int           init_done;
  int           num_ref_frames;

  FrameStore   *last_picture;
  unsigned     used_size_il;
  int          layer_id;

  //DPB related function;

} DecodedPictureBuffer;

extern void init_lists_for_non_reference_loss(DecodedPictureBuffer *p_Dpb, int , PictureStructure );

extern void conceal_non_ref_pics(DecodedPictureBuffer *p_Dpb, int diff);
extern void conceal_lost_frames (DecodedPictureBuffer *p_Dpb, Slice *pSlice);

extern void sliding_window_poc_management(DecodedPictureBuffer *p_Dpb, StorablePicture *p);
extern void write_lost_non_ref_pic       (DecodedPictureBuffer *p_Dpb, int poc, int p_out);
extern void write_lost_ref_after_idr     (DecodedPictureBuffer *p_Dpb, int pos);

extern int comp(const void *, const void *);




void ercPixConcealIMB    (VideoParameters *p_Vid, imgpel *currFrame, int row, int column, int predBlocks[], int frameWidth, int mbWidthInBlocks);
int ercCollect8PredBlocks( int predBlocks[], int currRow, int currColumn, char *condition,
                          int maxRow, int maxColumn, int step, uint8_t fNoCornerNeigh );
int ercCollectColumnBlocks( int predBlocks[], int currRow, int currColumn, char *condition, int maxRow, int maxColumn, int step );

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




extern int  get_concealed_element(VideoParameters *p_Vid, SyntaxElement *sym);
extern int  set_ec_flag          (VideoParameters *p_Vid, int se);
extern void reset_ec_flags       (VideoParameters *p_Vid);



#if TRACE
extern void dectracebitcnt(int count);
extern void tracebits     ( const char *trace_str, int len, int info, int value1);
extern void tracebits2    ( const char *trace_str, int len, int info);
extern void trace_info    ( SyntaxElement *currSE, const char *description_str, int value1 );
#endif



extern int fmo_init (VideoParameters *p_Vid, Slice *pSlice);
extern int FmoFinit (VideoParameters *p_Vid);

extern int FmoGetNumberOfSliceGroup(VideoParameters *p_Vid);
extern int FmoGetLastMBOfPicture   (VideoParameters *p_Vid);
extern int FmoGetLastMBInSliceGroup(VideoParameters *p_Vid, int SliceGroup);
extern int FmoGetSliceGroupId      (VideoParameters *p_Vid, int mb);
extern int FmoGetNextMBNr          (VideoParameters *p_Vid, int CurrentMbNr);




#define ET_SIZE 300      //!< size of error text buffer
extern char errortext[ET_SIZE]; //!< buffer for error message for exit with error()

struct pic_motion_params_old;
struct pic_motion_params;

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
// structures that will be declared somewhere else
struct storable_picture;
struct datapartition_dec;
struct syntaxelement_dec;




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
  unsigned field_pic_flag;   
  unsigned frame_num;
  int      nal_ref_idc;
  unsigned pic_oder_cnt_lsb;
  int      delta_pic_oder_cnt_bottom;
  int      delta_pic_order_cnt[2];
  uint8_t     bottom_field_flag;
  uint8_t     idr_flag;
  int      idr_pic_id;
  int      pps_id;
#if (MVC_EXTENSION_ENABLE)
  int      view_id;
  int      inter_view_flag;
  int      anchor_pic_flag;
#endif
  int      layer_id;
} OldSliceParams;

typedef struct decoder_params {
  InputParameters   *p_Inp;          //!< Input Parameters
  VideoParameters   *p_Vid;          //!< Image Parameters
  int64_t              bufferSize;     //!< buffersize for tiff reads (not currently supported)
  int                UsedBits;      // for internal statistics, is adjusted by read_se_v, read_ue_v, read_u_1
  FILE              *p_trace;        //!< Trace file
  int                bitcounter;
} DecoderParams;

extern DecoderParams  *p_Dec;

// prototypes
extern void error(char *text, int code);

// dynamic mem allocation
extern int  init_global_buffers( VideoParameters *p_Vid, int layer_id );
extern void free_global_buffers( VideoParameters *p_Vid);
extern void free_layer_buffers( VideoParameters *p_Vid, int layer_id );

extern int RBSPtoSODB(uint8_t *streamBuffer, int last_byte_pos);
extern int EBSPtoRBSP(uint8_t *streamBuffer, int end_bytepos, int begin_bytepos);

extern void FreePartition (DataPartition *dp, int n);
extern DataPartition *AllocPartition(int n);

extern void tracebits (const char *trace_str, int len, int info, int value1);
extern void tracebits2(const char *trace_str, int len, int info);

extern unsigned CeilLog2   ( unsigned uiVal);
extern unsigned CeilLog2_sf( unsigned uiVal);

// For 4:4:4 independent mode
extern void change_plane_JV      ( VideoParameters *p_Vid, int nplane, Slice *pSlice);
extern void make_frame_picture_JV( VideoParameters *p_Vid );

#if (MVC_EXTENSION_ENABLE)
extern void nal_unit_header_mvc_extension(NALUnitHeaderMVCExt_t *NaluHeaderMVCExt, struct bit_stream_dec *bitstream);
#endif

extern void FreeDecPicList ( DecodedPicList *pDecPicList );
extern void ClearDecPicList( VideoParameters *p_Vid );
extern DecodedPicList *get_one_avail_dec_pic_from_list(DecodedPicList *pDecPicList, int b3D, int view_id);
extern Slice *malloc_slice( InputParameters *p_Inp, VideoParameters *p_Vid );
extern void copy_slice_info ( Slice *currSlice, OldSliceParams *p_old_slice );
extern void OpenOutputFiles(VideoParameters *p_Vid, int view0_id, int view1_id);
extern void set_global_coding_par(VideoParameters *p_Vid, CodingParameters *cps);

static inline int is_FREXT_profile(unsigned int profile_idc) {
  // we allow all FRExt tools, when no profile is active
  return ( profile_idc==NO_PROFILE || profile_idc==FREXT_HP || profile_idc==FREXT_Hi10P || profile_idc==FREXT_Hi422 || profile_idc==FREXT_Hi444 || profile_idc == FREXT_CAVLC444 );
}

static inline int is_HI_intra_only_profile(unsigned int profile_idc, bool constrained_set3_flag) {
  return ( ( ( (profile_idc == FREXT_Hi10P)||(profile_idc == FREXT_Hi422)|| (profile_idc == FREXT_Hi444)) && constrained_set3_flag) || (profile_idc == FREXT_CAVLC444) );
}
static inline int is_BL_profile(unsigned int profile_idc) {
  return ( profile_idc == FREXT_CAVLC444 || profile_idc == BASELINE || profile_idc == MAIN || profile_idc == EXTENDED ||
           profile_idc == FREXT_HP || profile_idc == FREXT_Hi10P || profile_idc == FREXT_Hi422 || profile_idc == FREXT_Hi444);
}
static inline int is_EL_profile(unsigned int profile_idc) {
  return ( (profile_idc == MVC_HIGH) || (profile_idc == STEREO_HIGH) );
}

static inline int is_MVC_profile(unsigned int profile_idc){
  return ( (0)
#if (MVC_EXTENSION_ENABLE)
  || (profile_idc == MVC_HIGH) || (profile_idc == STEREO_HIGH)
#endif
  );
}




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
}DecErrCode;

typedef struct dec_set_t {
  int iPostprocLevel; // valid interval are [0..100]
  int bDBEnable;
  int bAllLayers;
  int time_incr;
  int bDecCompAdapt;
} DecSet_t;

#ifdef __cplusplus
extern "C" {
#endif

int OpenDecoder(InputParameters *p_Inp);
int DecodeOneFrame(DecodedPicList **ppDecPic);
int FinitDecoder(DecodedPicList **ppDecPicList);
int CloseDecoder();
int SetOptsDecoder(DecSet_t *pDecOpts);

#ifdef __cplusplus
}
#endif



extern int FirstPartOfSliceHeader(Slice *currSlice);
extern int RestOfSliceHeader     (Slice *currSlice);

extern void dec_ref_pic_marking(VideoParameters *p_Vid, Bitstream *currStream, Slice *pSlice);

extern void decode_poc(VideoParameters *p_Vid, Slice *pSlice);
extern int  dumppoc   (VideoParameters *p_Vid);



//#include "mbuffer.h"

extern void calculate_frame_no(VideoParameters *p_Vid, StorablePicture *p);
extern void find_snr          (VideoParameters *p_Vid, StorablePicture *p, int *p_ref);
extern int  picture_order     ( Slice *pSlice );

extern void decode_one_slice  (Slice *currSlice);
extern int  read_new_slice    (Slice *currSlice);
extern void exit_picture      (VideoParameters *p_Vid, StorablePicture **dec_picture);
extern int  decode_one_frame  (DecoderParams *pDecoder);

extern int  is_new_picture(StorablePicture *dec_picture, Slice *currSlice, OldSliceParams *p_old_slice);
extern void init_old_slice(OldSliceParams *p_old_slice);
// For 4:4:4 independent mode
extern void copy_dec_picture_JV (VideoParameters *p_Vid, StorablePicture *dst, StorablePicture *src );

extern void frame_postprocessing(VideoParameters *p_Vid);
extern void field_postprocessing(VideoParameters *p_Vid);

#if (MVC_EXTENSION_ENABLE)
extern int GetViewIdx(VideoParameters *p_Vid, int iVOIdx);
extern int GetVOIdx(VideoParameters *p_Vid, int iViewId);
extern int get_maxViewIdx(VideoParameters *p_Vid, int view_id, int anchor_pic_flag, int listidx);
#endif

extern void init_slice(VideoParameters *p_Vid, Slice *currSlice);
extern void decode_slice(Slice *currSlice, int current_header);




extern int intra_pred_4x4_mbaff (Macroblock *currMB, ColorPlane pl, int ioff, int joff, int img_block_x, int img_block_y);
extern int intra_pred_4x4_normal(Macroblock *currMB, ColorPlane pl, int ioff, int joff, int img_block_x, int img_block_y);



extern int intra_pred_8x8_normal(Macroblock *currMB, ColorPlane pl, int ioff, int joff);
extern int intra_pred_8x8_mbaff(Macroblock *currMB, ColorPlane pl, int ioff, int joff);


extern int intra_pred_16x16_mbaff (Macroblock *currMB, ColorPlane pl, int predmode);
extern int intra_pred_16x16_normal(Macroblock *currMB, ColorPlane pl, int predmode);


#ifdef _LEAKYBUCKET_
// Leaky Bucket functions
unsigned long GetBigDoubleWord(FILE *fp);
void calc_buffer(InputParameters *p_Inp);
#endif



#define GROUP_SIZE  1

/*********************************************************************************************************/

// NOTE: In principle, the alpha and beta tables are calculated with the formulas below
//       Alpha( qp ) = 0.8 * (2^(qp/6)  -  1)
//       Beta ( qp ) = 0.5 * qp  -  7


extern void setup_slice_methods_mbaff(Slice *currSlice);
extern void setup_slice_methods      (Slice *currSlice);
extern void get_neighbors(Macroblock *currMB, PixelPos *block, int mb_x, int mb_y, int blockshape_x);

extern void start_macroblock     (Slice *currSlice, Macroblock **currMB);
extern int  decode_one_macroblock(Macroblock *currMB, StorablePicture *dec_picture);
extern bool  exit_macroblock  (Slice *currSlice, int eos_bit);
extern void update_qp            (Macroblock *currMB, int qp);




extern int mb_pred_intra4x4      (Macroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
extern int mb_pred_intra16x16    (Macroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
extern int mb_pred_intra8x8      (Macroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);

extern int mb_pred_skip          (Macroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
extern int mb_pred_sp_skip       (Macroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
extern int mb_pred_p_inter8x8    (Macroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
extern int mb_pred_p_inter16x16  (Macroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
extern int mb_pred_p_inter16x8   (Macroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
extern int mb_pred_p_inter8x16   (Macroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
extern int mb_pred_b_d4x4spatial (Macroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
extern int mb_pred_b_d8x8spatial (Macroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
extern int mb_pred_b_d4x4temporal(Macroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
extern int mb_pred_b_d8x8temporal(Macroblock *currMB, ColorPlane curr_plane, imgpel **currImg, StorablePicture *dec_picture);
extern int mb_pred_b_inter8x8    (Macroblock *currMB, ColorPlane curr_plane, StorablePicture *dec_picture);
extern int mb_pred_ipcm          (Macroblock *currMB);



#define MAX_LIST_SIZE 33

//! definition of pic motion parameters
typedef struct pic_motion_params {
  struct storable_picture *ref_pic[2];  //!< referrence picture pointer
  MotionVector             mv[2];       //!< motion vector  
  char                     ref_idx[2];  //!< reference picture   [list][subblock_y][subblock_x]
  //uint8_t                   mb_field;    //!< field macroblock indicator
  uint8_t                     slice_no;
} PicMotionParams;




extern void              init_dpb(VideoParameters *p_Vid, DecodedPictureBuffer *p_Dpb, int type);
extern void              re_init_dpb(VideoParameters *p_Vid, DecodedPictureBuffer *p_Dpb, int type);
extern void              free_dpb(DecodedPictureBuffer *p_Dpb);
extern FrameStore*       alloc_frame_store(void);
extern void              free_frame_store (FrameStore* f);
extern StorablePicture*  alloc_storable_picture(VideoParameters *p_Vid, PictureStructure type, int size_x, int size_y, int size_x_cr, int size_y_cr, int is_output);
extern void              free_storable_picture (StorablePicture* p);
extern void              store_picture_in_dpb(DecodedPictureBuffer *p_Dpb, StorablePicture* p);
extern StorablePicture*  get_int16_t_term_pic (Slice *currSlice, DecodedPictureBuffer *p_Dpb, int picNum);

#if (MVC_EXTENSION_ENABLE)
extern void             idr_memory_management(DecodedPictureBuffer *p_Dpb, StorablePicture* p);
extern void             flush_dpbs(DecodedPictureBuffer **p_Dpb, int nLayers);
extern int              GetMaxDecFrameBuffering(VideoParameters *p_Vid);
extern void             append_interview_list(DecodedPictureBuffer *p_Dpb, 
                                              PictureStructure currPicStructure, int list_idx, 
                                              FrameStore **list, int *listXsize, int currPOC, 
                                              int curr_view_id, int anchor_pic_flag);
#endif

extern void unmark_for_reference(FrameStore* fs);
extern void unmark_for_long_term_reference(FrameStore* fs);
extern void remove_frame_from_dpb(DecodedPictureBuffer *p_Dpb, int pos);

extern void             flush_dpb(DecodedPictureBuffer *p_Dpb);
extern void             init_lists_p_slice (Slice *currSlice);
extern void             init_lists_b_slice (Slice *currSlice);
extern void             init_lists_i_slice (Slice *currSlice);
extern void             update_pic_num     (Slice *currSlice);

extern void             dpb_split_field      (VideoParameters *p_Vid, FrameStore *fs);
extern void             dpb_combine_field    (VideoParameters *p_Vid, FrameStore *fs);
extern void             dpb_combine_field_yuv(VideoParameters *p_Vid, FrameStore *fs);

extern void             reorder_ref_pic_list(Slice *currSlice, int cur_list);

extern void             init_mbaff_lists     (VideoParameters *p_Vid, Slice *currSlice);
extern void             alloc_ref_pic_list_reordering_buffer(Slice *currSlice);
extern void             free_ref_pic_list_reordering_buffer(Slice *currSlice);

extern void             fill_frame_num_gap(VideoParameters *p_Vid, Slice *pSlice);

extern void compute_colocated (Slice *currSlice, StorablePicture **listX[6]);


extern int init_img_data(VideoParameters *p_Vid, ImageData *p_ImgData, seq_parameter_set_rbsp_t *sps);
extern void free_img_data(VideoParameters *p_Vid, ImageData *p_ImgData);
extern void pad_dec_picture(VideoParameters *p_Vid, StorablePicture *dec_picture);
extern void pad_buf(imgpel *pImgBuf, int iWidth, int iHeight, int iStride, int iPadX, int iPadY);
extern void process_picture_in_dpb_s(VideoParameters *p_Vid, StorablePicture *p_pic);
extern StorablePicture * clone_storable_picture( VideoParameters *p_Vid, StorablePicture *p_pic );
extern void store_proc_picture_in_dpb(DecodedPictureBuffer *p_Dpb, StorablePicture* p);



#if (MVC_EXTENSION_ENABLE)
extern void reorder_lists_mvc     (Slice * currSlice, int currPOC);
extern void init_lists_p_slice_mvc(Slice *currSlice);
extern void init_lists_b_slice_mvc(Slice *currSlice);
extern void init_lists_i_slice_mvc(Slice *currSlice);

extern void reorder_ref_pic_list_mvc(Slice *currSlice, int cur_list, int **anchor_ref, int **non_anchor_ref,
                                                 int view_id, int anchor_pic_flag, int currPOC, int listidx);

extern void reorder_short_term(Slice *currSlice, int cur_list, int num_ref_idx_lX_active_minus1, int picNumLX, int *refIdxLX, int currViewID);
extern void reorder_long_term(Slice *currSlice, StorablePicture **RefPicListX, int num_ref_idx_lX_active_minus1, int LongTermPicNum, int *refIdxLX, int currViewID);
#endif



extern int  allocate_pred_mem(Slice *currSlice);
extern void free_pred_mem    (Slice *currSlice);

extern void get_block_luma(StorablePicture *curr_ref, int x_pos, int y_pos, int block_size_x, int block_size_y, imgpel **block,
                           int shift_x,int maxold_x,int maxold_y,int **tmp_res,int max_imgpel_value,imgpel no_ref_value,Macroblock *currMB);

extern void intra_cr_decoding    (Macroblock *currMB, int yuv);
extern void prepare_direct_params(Macroblock *currMB, StorablePicture *dec_picture, MotionVector *pmvl0, MotionVector *pmvl1,char *l0_rFrame, char *l1_rFrame);
extern void perform_mc           (Macroblock *currMB, ColorPlane pl, StorablePicture *dec_picture, int pred_dir, int i, int j, int block_size_x, int block_size_y);



extern void CheckZeroByteNonVCL(VideoParameters *p_Vid, NALU_t *nalu);
extern void CheckZeroByteVCL   (VideoParameters *p_Vid, NALU_t *nalu);

extern int read_next_nalu(VideoParameters *p_Vid, NALU_t *nalu);



extern void write_stored_frame(VideoParameters *p_Vid, FrameStore *fs, int p_out);
extern void direct_output     (VideoParameters *p_Vid, StorablePicture *p, int p_out);
extern void init_out_buffer   (VideoParameters *p_Vid);
extern void uninit_out_buffer (VideoParameters *p_Vid);
#if (PAIR_FIELDS_IN_OUTPUT)
extern void flush_pending_output(VideoParameters *p_Vid, int p_out);
#endif
extern void init_output(CodingParameters *p_CodingParams, int symbol_size_in_bytes);




extern void Scaling_List(int *scalingList, int sizeOfScalingList, bool *UseDefaultScalingMatrix, Bitstream *s);

extern void InitVUI(seq_parameter_set_rbsp_t *sps);
extern int  ReadVUI(DataPartition *p, seq_parameter_set_rbsp_t *sps);
extern int  ReadHRDParameters(DataPartition *p, hrd_parameters_t *hrd);

extern void PPSConsistencyCheck (pic_parameter_set_rbsp_t *pps);
extern void SPSConsistencyCheck (seq_parameter_set_rbsp_t *sps);

extern void MakePPSavailable (VideoParameters *p_Vid, int id, pic_parameter_set_rbsp_t *pps);
extern void MakeSPSavailable (VideoParameters *p_Vid, int id, seq_parameter_set_rbsp_t *sps);

extern void ProcessSPS (VideoParameters *p_Vid, NALU_t *nalu);
extern void ProcessPPS (VideoParameters *p_Vid, NALU_t *nalu);

extern void CleanUpPPS(VideoParameters *p_Vid);

extern void activate_sps (VideoParameters *p_Vid, seq_parameter_set_rbsp_t *sps);
extern void activate_pps (VideoParameters *p_Vid, pic_parameter_set_rbsp_t *pps);

extern void UseParameterSet (Slice *currSlice);

#if (MVC_EXTENSION_ENABLE)
extern void SubsetSPSConsistencyCheck (subset_seq_parameter_set_rbsp_t *subset_sps);
extern void ProcessSubsetSPS (VideoParameters *p_Vid, NALU_t *nalu);

extern void mvc_vui_parameters_extension(MVCVUI_t *pMVCVUI, Bitstream *s);
extern void seq_parameter_set_mvc_extension(subset_seq_parameter_set_rbsp_t *subset_sps, Bitstream *s);
extern void init_subset_sps_list(subset_seq_parameter_set_rbsp_t *subset_sps_list, int iSize);
extern void reset_subset_sps(subset_seq_parameter_set_rbsp_t *subset_sps);
extern int  GetBaseViewId(VideoParameters *p_Vid, subset_seq_parameter_set_rbsp_t **subset_sps);
extern void get_max_dec_frame_buf_size(seq_parameter_set_rbsp_t *sps);
#endif



// exported functions
// quantization initialization
extern void init_qp_process (CodingParameters *cps);
extern void free_qp_matrices(CodingParameters *cps);

// For Q-matrix
extern void assign_quant_params   (Slice *currslice);
extern void CalculateQuant4x4Param(Slice *currslice);
extern void CalculateQuant8x8Param(Slice *currslice);




#define MAXRTPPAYLOADLEN  (65536 - 40)    //!< Maximum payload size of an RTP packet */
#define MAXRTPPACKETSIZE  (65536 - 28)    //!< Maximum size of an RTP packet incl. header */
#define H264PAYLOADTYPE 105               //!< RTP paylaod type fixed here for simplicity*/
#define H264SSRC 0x12345678               //!< SSRC, chosen to simplify debugging */
#define RTP_TR_TIMESTAMP_MULT 1000        //!< should be something like 27 Mhz / 29.97 Hz */

typedef struct {
  unsigned int v;          //!< Version, 2 bits, MUST be 0x2
  unsigned int p;          //!< Padding bit, Padding MUST NOT be used
  unsigned int x;          //!< Extension, MUST be zero
  unsigned int cc;         /*!< CSRC count, normally 0 in the absence
                                of RTP mixers */
  unsigned int m;          //!< Marker bit
  unsigned int pt;         //!< 7 bits, Payload Type, dynamically established
  uint16_t       seq;        /*!< RTP sequence number, incremented by one for
                                each sent packet */
  unsigned int timestamp;  //!< timestamp, 27 MHz for H.264
  unsigned int ssrc;       //!< Synchronization Source, chosen randomly
  uint8_t *       payload;    //!< the payload including payload headers
  unsigned int paylen;     //!< length of payload in bytes
  uint8_t *       packet;     //!< complete packet including header and payload
  unsigned int packlen;    //!< length of packet, typically paylen+12
} RTPpacket_t;

void DumpRTPHeader (RTPpacket_t *p);
int  GetRTPNALU  (VideoParameters *p_Vid, NALU_t *nalu, int BitStreamFile);
void OpenRTPFile (char *fn, int *p_BitStreamFile);
void CloseRTPFile(int *p_BitStreamFile);



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

#if (ENABLE_OUTPUT_TONEMAPPING)
typedef struct tone_mapping_struct_s {
  bool seiHasTone_mapping;
  unsigned int  tone_map_repetition_period;
  unsigned char coded_data_bit_depth;
  unsigned char sei_bit_depth;
  unsigned int  model_id;
  unsigned int count;
  
  imgpel lut[1<<MAX_CODED_BIT_DEPTH];                 //<! look up table for mapping the coded data value to output data value

  Bitstream *data;
  int payloadSize;
} ToneMappingSEI;
#endif

//! Frame packing arrangement Information
typedef struct {
  unsigned int  frame_packing_arrangement_id;
  bool       frame_packing_arrangement_cancel_flag;
  unsigned char frame_packing_arrangement_type;
  bool       quincunx_sampling_flag;
  unsigned char content_interpretation_type;
  bool       spatial_flipping_flag;
  bool       frame0_flipped_flag;
  bool       field_views_flag;
  bool       current_frame_is_frame0_flag;
  bool       frame0_self_contained_flag;
  bool       frame1_self_contained_flag;
  unsigned char frame0_grid_position_x;
  unsigned char frame0_grid_position_y;
  unsigned char frame1_grid_position_x;
  unsigned char frame1_grid_position_y;
  unsigned char frame_packing_arrangement_reserved_byte;
  unsigned int  frame_packing_arrangement_repetition_period;
  bool       frame_packing_arrangement_extension_flag;
} frame_packing_arrangement_information_struct;


//! Green metada Information
typedef struct {
  unsigned char  green_metadata_type;
  unsigned char  period_type;
  uint16_t num_seconds;
  uint16_t num_pictures;
  unsigned char percent_non_zero_macroblocks;
  unsigned char percent_intra_coded_macroblocks;
  unsigned char percent_six_tap_filtering;
  unsigned char percent_alpha_point_deblocking_instance;
  unsigned char xsd_metric_type;
  uint16_t xsd_metric_value;
} Green_metadata_information_struct;


void InterpretSEIMessage                                ( uint8_t* payload, int size, VideoParameters *p_Vid, Slice *pSlice );
void interpret_spare_pic                                ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_subsequence_info                         ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_subsequence_layer_characteristics_info   ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_subsequence_characteristics_info         ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_scene_information                        ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_user_data_registered_itu_t_t35_info      ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_user_data_unregistered_info              ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_pan_scan_rect_info                       ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_recovery_point_info                      ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_filler_payload_info                      ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_dec_ref_pic_marking_repetition_info      ( uint8_t* payload, int size, VideoParameters *p_Vid, Slice *pSlice );
void interpret_full_frame_freeze_info                   ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_full_frame_freeze_release_info           ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_full_frame_snapshot_info                 ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_progressive_refinement_start_info        ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_progressive_refinement_end_info          ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_motion_constrained_slice_group_set_info  ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_reserved_info                            ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_buffering_period_info                    ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_picture_timing_info                      ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_film_grain_characteristics_info          ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_deblocking_filter_display_preference_info( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_stereo_video_info_info                   ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_post_filter_hints_info                   ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_tone_mapping                             ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_frame_packing_arrangement_info           ( uint8_t* payload, int size, VideoParameters *p_Vid );
void interpret_green_metadata_info                       (uint8_t* payload, int size, VideoParameters *p_Vid );

#if (ENABLE_OUTPUT_TONEMAPPING)
void tone_map               (imgpel** imgX, imgpel* lut, int size_x, int size_y);
void init_tone_mapping_sei  (ToneMappingSEI *seiToneMapping);
void update_tone_mapping_sei(ToneMappingSEI *seiToneMapping);
#endif



extern void itrans8x8   (Macroblock *currMB, ColorPlane pl, int ioff, int joff);
extern void icopy8x8    (Macroblock *currMB, ColorPlane pl, int ioff, int joff);


extern int read_se_v (char *tracestring, Bitstream *bitstream, int *used_bits);
extern int read_ue_v (char *tracestring, Bitstream *bitstream, int *used_bits);
extern bool read_u_1 (char *tracestring, Bitstream *bitstream, int *used_bits);
extern int read_u_v (int LenInBits, char *tracestring, Bitstream *bitstream, int *used_bits);
extern int read_i_v (int LenInBits, char *tracestring, Bitstream *bitstream, int *used_bits);

// CAVLC mapping
extern void linfo_ue(int len, int info, int *value1, int *dummy);
extern void linfo_se(int len, int info, int *value1, int *dummy);

extern void linfo_cbp_intra_normal(int len,int info,int *cbp, int *dummy);
extern void linfo_cbp_inter_normal(int len,int info,int *cbp, int *dummy);
extern void linfo_cbp_intra_other(int len,int info,int *cbp, int *dummy);
extern void linfo_cbp_inter_other(int len,int info,int *cbp, int *dummy);

extern void linfo_levrun_inter(int len,int info,int *level,int *irun);
extern void linfo_levrun_c2x2(int len,int info,int *level,int *irun);

extern int  uvlc_startcode_follows(Slice *currSlice, int dummy);

extern int  readSyntaxElement_VLC (SyntaxElement *sym, Bitstream *currStream);
extern int  readSyntaxElement_UVLC(Macroblock *currMB, SyntaxElement *sym, struct datapartition_dec *dp);
extern int  readSyntaxElement_Intra4x4PredictionMode(SyntaxElement *sym, Bitstream   *currStream);

extern int  GetVLCSymbol (uint8_t buffer[],int totbitoffset,int *info, int bytecount);
extern int  GetVLCSymbol_IntraMode (uint8_t buffer[],int totbitoffset,int *info, int bytecount);

extern int readSyntaxElement_FLC                         (SyntaxElement *sym, Bitstream *currStream);
extern int readSyntaxElement_NumCoeffTrailingOnes        (SyntaxElement *sym,  Bitstream *currStream, char *type);
extern int readSyntaxElement_NumCoeffTrailingOnesChromaDC(VideoParameters *p_Vid, SyntaxElement *sym, Bitstream *currStream);
extern int readSyntaxElement_Level_VLC0                  (SyntaxElement *sym, Bitstream *currStream);
extern int readSyntaxElement_Level_VLCN                  (SyntaxElement *sym, int vlc, Bitstream *currStream);
extern int readSyntaxElement_TotalZeros                  (SyntaxElement *sym, Bitstream *currStream);
extern int readSyntaxElement_TotalZerosChromaDC          (VideoParameters *p_Vid, SyntaxElement *sym, Bitstream *currStream);
extern int readSyntaxElement_Run                         (SyntaxElement *sym, Bitstream *currStream);
extern int GetBits  (uint8_t buffer[],int totbitoffset,int *info, int bitcount, int numbits);
extern int ShowBits (uint8_t buffer[],int totbitoffset,int bitcount, int numbits);

extern int more_rbsp_data (uint8_t buffer[],int totbitoffset,int bytecount);


void init_time(void);
int WriteOneFrame(DecodedPicList *pDecPic, int hFileOutput0, int hFileOutput1, int bOutputAllFrames);

