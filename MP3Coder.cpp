#include "stdafx.h"
#include "vidsend.h"
#include "vidsendLog.h"

#include "MP3Coder.h"






/*
 *      LAME MP3 encoding engine
 *
 *      Copyright (c) 1999 Mark Taylor
 *      Copyright (c) 2000-2002 Takehiro Tominaga
 *      Copyright (c) 2000-2011 Robert Hegemann
 *      Copyright (c) 2001 Gabriel Bouvigne
 *      Copyright (c) 2001 John Dahlstrom

 * adapted for VideoSender (C) G.Dar 2023 

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

/* $Id: encoder.c,v 1.114 2017/08/26 10:54:57 robert Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif




/*
 * auto-adjust of ATH, useful for low volume
 * Gabriel Bouvigne 3 feb 2001
 *
 * modifies some values in
 *   gfp->internal_flags->ATH
 *   (gfc->ATH)
 */
static void adjust_ATH(lame_internal_flags const *const gfc) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    FLOAT   gr2_max, max_pow;

    if(gfc->ATH->use_adjust == 0) {
        gfc->ATH->adjust_factor = 1.0; /* no adjustment */
        return;
    }

    /* jd - 2001 mar 12, 27, jun 30 */
    /* loudness based on equal loudness curve; */
    /* use granule with maximum combined loudness */
    max_pow = gfc->ov_psy.loudness_sq[0][0];
    gr2_max = gfc->ov_psy.loudness_sq[1][0];
    if(cfg->channels_out == 2) {
        max_pow += gfc->ov_psy.loudness_sq[0][1];
        gr2_max += gfc->ov_psy.loudness_sq[1][1];
    }
    else {
        max_pow += max_pow;
        gr2_max += gr2_max;
    }
    if(cfg->mode_gr == 2) {
        max_pow = Max(max_pow, gr2_max);
    }
    max_pow *= 0.5;     /* max_pow approaches 1.0 for full band noise */

    /* jd - 2001 mar 31, jun 30 */
    /* user tuning of ATH adjustment region */
    max_pow *= gfc->ATH->aa_sensitivity_p;

    /*  adjust ATH depending on range of maximum value
     */

    /* jd - 2001 feb27, mar12,20, jun30, jul22 */
    /* continuous curves based on approximation */
    /* to GB's original values. */
    /* For an increase in approximate loudness, */
    /* set ATH adjust to adjust_limit immediately */
    /* after a delay of one frame. */
    /* For a loudness decrease, reduce ATH adjust */
    /* towards adjust_limit gradually. */
    /* max_pow is a loudness squared or a power. */
    if(max_pow > 0.03125) { /* ((1 - 0.000625)/ 31.98) from curve below */
        if(gfc->ATH->adjust_factor >= 1.0) {
            gfc->ATH->adjust_factor = 1.0;
        }
        else {
            /* preceding frame has lower ATH adjust; */
            /* ascend only to the preceding adjust_limit */
            /* in case there is leading low volume */
            if(gfc->ATH->adjust_factor < gfc->ATH->adjust_limit) {
                gfc->ATH->adjust_factor = gfc->ATH->adjust_limit;
            }
        }
        gfc->ATH->adjust_limit = 1.0;
    }
    else {              /* adjustment curve */
        /* about 32 dB maximum adjust (0.000625) */
        FLOAT const adj_lim_new = 31.98 * max_pow + 0.000625;
        if(gfc->ATH->adjust_factor >= adj_lim_new) { /* descend gradually */
            gfc->ATH->adjust_factor *= adj_lim_new * 0.075 + 0.925;
            if(gfc->ATH->adjust_factor < adj_lim_new) { /* stop descent */
                gfc->ATH->adjust_factor = adj_lim_new;
            }
        }
        else {          /* ascend */
            if(gfc->ATH->adjust_limit >= adj_lim_new) {
                gfc->ATH->adjust_factor = adj_lim_new;
            }
            else {      /* preceding frame has lower ATH adjust; */
                /* ascend only to the preceding adjust_limit */
                if(gfc->ATH->adjust_factor < gfc->ATH->adjust_limit) {
                    gfc->ATH->adjust_factor = gfc->ATH->adjust_limit;
                }
            }
        }
        gfc->ATH->adjust_limit = adj_lim_new;
    }
}

/***********************************************************************
 *
 *  some simple statistics
 *
 *  bitrate index 0: free bitrate -> not allowed in VBR mode
 *  : bitrates, kbps depending on MPEG version
 *  bitrate index 15: forbidden
 *
 *  mode_ext:
 *  0:  LR
 *  1:  LR-i
 *  2:  MS
 *  3:  MS-i
 *
 ***********************************************************************/

static void updateStats(lame_internal_flags * const gfc) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t *eov = &gfc->ov_enc;
    int     gr, ch;
    assert(0 <= eov->bitrate_index && eov->bitrate_index < 16);
    assert(0 <= eov->mode_ext && eov->mode_ext < 4);

    /* count bitrate indices */
    eov->bitrate_channelmode_hist[eov->bitrate_index][4]++;
    eov->bitrate_channelmode_hist[15][4]++;

    /* count 'em for every mode extension in case of 2 channel encoding */
    if(cfg->channels_out == 2) {
        eov->bitrate_channelmode_hist[eov->bitrate_index][eov->mode_ext]++;
        eov->bitrate_channelmode_hist[15][eov->mode_ext]++;
    }
    for(gr = 0; gr < cfg->mode_gr; ++gr) {
        for(ch = 0; ch < cfg->channels_out; ++ch) {
            int     bt = gfc->l3_side.tt[gr][ch].block_type;
            if(gfc->l3_side.tt[gr][ch].mixed_block_flag)
                bt = 4;
            eov->bitrate_blocktype_hist[eov->bitrate_index][bt]++;
            eov->bitrate_blocktype_hist[eov->bitrate_index][5]++;
            eov->bitrate_blocktype_hist[15][bt]++;
            eov->bitrate_blocktype_hist[15][5]++;
        }
    }
}




static void
lame_encode_frame_init(lame_internal_flags * gfc, const sample_t *const inbuf[2])
{
    SessionConfig_t const *const cfg = &gfc->cfg;

    int     ch, gr;

    if(gfc->lame_encode_frame_init == 0) {
        sample_t primebuff0[286 + 1152 + 576];
        sample_t primebuff1[286 + 1152 + 576];
        int const framesize = 576 * cfg->mode_gr;
        /* prime the MDCT/polyphase filterbank with a short block */
        int     i, j;
        gfc->lame_encode_frame_init = 1;
        memset(primebuff0, 0, sizeof(primebuff0));
        memset(primebuff1, 0, sizeof(primebuff1));
        for(i = 0, j = 0; i < 286 + 576 * (1 + cfg->mode_gr); ++i) {
            if(i < framesize) {
                primebuff0[i] = 0;
                if(cfg->channels_out == 2)
                    primebuff1[i] = 0;
            }
            else {
                primebuff0[i] = inbuf[0][j];
                if(cfg->channels_out == 2)
                    primebuff1[i] = inbuf[1][j];
                ++j;
            }
        }
        /* polyphase filtering / mdct */
        for(gr = 0; gr < cfg->mode_gr; gr++) {
            for(ch = 0; ch < cfg->channels_out; ch++) {
                gfc->l3_side.tt[gr][ch].block_type = SHORT_TYPE;
            }
        }
        mdct_sub48(gfc, primebuff0, primebuff1);

        /* check FFT will not use a negative starting offset */
#if 576 < FFTOFFSET
# error FFTOFFSET greater than 576: FFT uses a negative offset
#endif
        /* check if we have enough data for FFT */
        assert(gfc->sv_enc.mf_size >= (BLKSIZE + framesize - FFTOFFSET));
        /* check if we have enough data for polyphase filterbank */
        assert(gfc->sv_enc.mf_size >= (512 + framesize - 32));
    }

}







/************************************************************************
*
* encodeframe()           Layer 3
*
* encode a single frame
*
************************************************************************
lame_encode_frame()


                       gr 0            gr 1
inbuf:           |--------------|--------------|--------------|


Polyphase (18 windows, each shifted 32)
gr 0:
window1          <----512---->
window18                 <----512---->

gr 1:
window1                         <----512---->
window18                                <----512---->



MDCT output:  |--------------|--------------|--------------|

FFT's                    <---------1024---------->
                                         <---------1024-------->



    inbuf = buffer of PCM data size=MP3 framesize
    encoder acts on inbuf[ch][0], but output is delayed by MDCTDELAY
    so the MDCT coefficints are from inbuf[ch][-MDCTDELAY]

    psy-model FFT has a 1 granule delay, so we feed it data for the 
    next granule.
    FFT is centered over granule:  224+576+224
    So FFT starts at:   576-224-MDCTDELAY

    MPEG2:  FFT ends at:  BLKSIZE+576-224-MDCTDELAY      (1328)
    MPEG1:  FFT ends at:  BLKSIZE+2*576-224-MDCTDELAY    (1904)

    MPEG2:  polyphase first window:  [0..511]
                      18th window:   [544..1055]          (1056)
    MPEG1:            36th window:   [1120..1631]         (1632)
            data needed:  512+framesize-32

    A close look newmdct.c shows that the polyphase filterbank
    only uses data from [0..510] for each window.  Perhaps because the window
    used by the filterbank is zero for the last point, so Takehiro's
    code doesn't bother to compute with it.

    FFT starts at 576-224-MDCTDELAY (304)  = 576-FFTOFFSET

*/

typedef FLOAT chgrdata[2][2];


int
lame_encode_mp3_frame(       /* Output */
                         lame_internal_flags * gfc, /* Context */
                         sample_t const *inbuf_l, /* Input */
                         sample_t const *inbuf_r, /* Input */
                         unsigned char *mp3buf, /* Output */
                         int mp3buf_size)
{                       /* Output */
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     mp3count;
    III_psy_ratio masking_LR[2][2]; /*LR masking & energy */
    III_psy_ratio masking_MS[2][2]; /*MS masking & energy */
    const III_psy_ratio (*masking)[2]; /*pointer to selected maskings */
    const sample_t *inbuf[2];

    FLOAT   tot_ener[2][4];
    FLOAT   ms_ener_ratio[2] = { .5, .5 };
    FLOAT   pe[2][2] = { {0., 0.}, {0., 0.} }, pe_MS[2][2] = { {
    0., 0.}, {
    0., 0.}};
    FLOAT (*pe_use)[2];

    int     ch, gr;

    inbuf[0] = inbuf_l;
    inbuf[1] = inbuf_r;

    if(gfc->lame_encode_frame_init == 0) {
        /*first run? */
        lame_encode_frame_init(gfc, inbuf);

    }


    /********************** padding *****************************/
    /* padding method as described in 
     * "MPEG-Layer3 / Bitstream Syntax and Decoding"
     * by Martin Sieler, Ralph Sperschneider
     *
     * note: there is no padding for the very first frame
     *
     * Robert Hegemann 2000-06-22
     */
    gfc->ov_enc.padding = FALSE;
    if((gfc->sv_enc.slot_lag -= gfc->sv_enc.frac_SpF) < 0) {
        gfc->sv_enc.slot_lag += cfg->samplerate_out;
        gfc->ov_enc.padding = TRUE;
    }



    /****************************************
    *   Stage 1: psychoacoustic model       *
    ****************************************/

    {
        /* psychoacoustic model
         * psy model has a 1 granule (576) delay that we must compensate for
         * (mt 6/99).
         */
        int     ret;
        const sample_t *bufp[2] = {0, 0}; /* address of beginning of left & right granule */
        int     blocktype[2];

        for(gr = 0; gr < cfg->mode_gr; gr++) {

            for(ch = 0; ch < cfg->channels_out; ch++) {
                bufp[ch] = &inbuf[ch][576 + gr * 576 - FFTOFFSET];
            }
            ret = L3psycho_anal_vbr(gfc, bufp, gr,
                                    masking_LR, masking_MS,
                                    pe[gr], pe_MS[gr], tot_ener[gr], blocktype);
            if(ret != 0)
                return -4;

            if(cfg->mode == JOINT_STEREO) {
                ms_ener_ratio[gr] = tot_ener[gr][2] + tot_ener[gr][3];
                if(ms_ener_ratio[gr] > 0)
                    ms_ener_ratio[gr] = tot_ener[gr][3] / ms_ener_ratio[gr];
            }

            /* block type flags */
            for(ch = 0; ch < cfg->channels_out; ch++) {
                gr_info *const cod_info = &gfc->l3_side.tt[gr][ch];
                cod_info->block_type = blocktype[ch];
                cod_info->mixed_block_flag = 0;
            }
        }
    }


    /* auto-adjust of ATH, useful for low volume */
    adjust_ATH(gfc);


    /****************************************
    *   Stage 2: MDCT                       *
    ****************************************/

    /* polyphase filtering / mdct */
    mdct_sub48(gfc, inbuf[0], inbuf[1]);


    /****************************************
    *   Stage 3: MS/LR decision             *
    ****************************************/

    /* Here will be selected MS or LR coding of the 2 stereo channels */
    gfc->ov_enc.mode_ext = MPG_MD_LR_LR;

    if(cfg->force_ms) {
        gfc->ov_enc.mode_ext = MPG_MD_MS_LR;
    }
    else if(cfg->mode == JOINT_STEREO) {
        /* ms_ratio = is scaled, for historical reasons, to look like
           a ratio of side_channel / total.
           0 = signal is 100% mono
           .5 = L & R uncorrelated
         */

        /* [0] and [1] are the results for the two granules in MPEG-1,
         * in MPEG-2 it's only a faked averaging of the same value
         * _prev is the value of the last granule of the previous frame
         * _next is the value of the first granule of the next frame
         */

        FLOAT   sum_pe_MS = 0;
        FLOAT   sum_pe_LR = 0;
        for(gr = 0; gr < cfg->mode_gr; gr++) {
            for(ch = 0; ch < cfg->channels_out; ch++) {
                sum_pe_MS += pe_MS[gr][ch];
                sum_pe_LR += pe[gr][ch];
            }
        }

        /* based on PE: M/S coding would not use much more bits than L/R */
        if(sum_pe_MS <= 1.00 * sum_pe_LR) {

            gr_info const *const gi0 = &gfc->l3_side.tt[0][0];
            gr_info const *const gi1 = &gfc->l3_side.tt[cfg->mode_gr - 1][0];

            if(gi0[0].block_type == gi0[1].block_type && gi1[0].block_type == gi1[1].block_type) {

                gfc->ov_enc.mode_ext = MPG_MD_MS_LR;
            }
        }
    }

    /* bit and noise allocation */
    if(gfc->ov_enc.mode_ext == MPG_MD_MS_LR) {
        masking = (const III_psy_ratio (*)[2])masking_MS; /* use MS masking */
        pe_use = pe_MS;
    }
    else {
        masking = (const III_psy_ratio (*)[2])masking_LR; /* use LR masking */
        pe_use = pe;
    }


    /* copy data for MP3 frame analyzer */
    if(cfg->analysis && gfc->pinfo != NULL) {
        for(gr = 0; gr < cfg->mode_gr; gr++) {
            for(ch = 0; ch < cfg->channels_out; ch++) {
                gfc->pinfo->ms_ratio[gr] = 0;
                gfc->pinfo->ms_ener_ratio[gr] = ms_ener_ratio[gr];
                gfc->pinfo->blocktype[gr][ch] = gfc->l3_side.tt[gr][ch].block_type;
                gfc->pinfo->pe[gr][ch] = pe_use[gr][ch];
                memcpy(gfc->pinfo->xr[gr][ch], &gfc->l3_side.tt[gr][ch].xr[0], sizeof(FLOAT) * 576);
                /* in psymodel, LR and MS data was stored in pinfo.  
                   switch to MS data: */
                if(gfc->ov_enc.mode_ext == MPG_MD_MS_LR) {
                    gfc->pinfo->ers[gr][ch] = gfc->pinfo->ers[gr][ch + 2];
                    memcpy(gfc->pinfo->energy[gr][ch], gfc->pinfo->energy[gr][ch + 2],
                           sizeof(gfc->pinfo->energy[gr][ch]));
                }
            }
        }
			}


    /****************************************
    *   Stage 4: quantization loop          *
    ****************************************/

    if(cfg->vbr == vbr_off || cfg->vbr == vbr_abr) {
        static FLOAT const fircoef[9] = {
            -0.0207887 * 5, -0.0378413 * 5, -0.0432472 * 5, -0.031183 * 5,
            7.79609e-18 * 5, 0.0467745 * 5, 0.10091 * 5, 0.151365 * 5,
            0.187098 * 5
        };

        int     i;
        FLOAT   f;

        for(i = 0; i < 18; i++)
            gfc->sv_enc.pefirbuf[i] = gfc->sv_enc.pefirbuf[i + 1];

        f = 0.0;
        for(gr = 0; gr < cfg->mode_gr; gr++)
            for(ch = 0; ch < cfg->channels_out; ch++)
                f += pe_use[gr][ch];
        gfc->sv_enc.pefirbuf[18] = f;

        f = gfc->sv_enc.pefirbuf[9];
        for(i = 0; i < 9; i++)
            f += (gfc->sv_enc.pefirbuf[i] + gfc->sv_enc.pefirbuf[18 - i]) * fircoef[i];

        f = (670 * 5 * cfg->mode_gr * cfg->channels_out) / f;
        for(gr = 0; gr < cfg->mode_gr; gr++) {
            for(ch = 0; ch < cfg->channels_out; ch++) {
                pe_use[gr][ch] *= f;
            }
        }
			}
    switch(cfg->vbr)    {
    default:
    case vbr_off:
        CBR_iteration_loop(gfc, (const FLOAT (*)[2])pe_use, ms_ener_ratio, masking);
        break;
    case vbr_abr:
        ABR_iteration_loop(gfc, (const FLOAT (*)[2])pe_use, ms_ener_ratio, masking);
        break;
    case vbr_rh:
        VBR_old_iteration_loop(gfc, (const FLOAT (*)[2])pe_use, ms_ener_ratio, masking);
        break;
    case vbr_mt:
    case vbr_mtrh:
        VBR_new_iteration_loop(gfc, (const FLOAT (*)[2])pe_use, ms_ener_ratio, masking);
        break;
    }


    /****************************************
    *   Stage 5: bitstream formatting       *
    ****************************************/


    /*  write the frame to the bitstream  */
    format_bitstream(gfc);

    /* copy mp3 bit buffer into array */
    mp3count = copy_buffer(gfc, mp3buf, mp3buf_size, 1);


    if(cfg->write_lame_tag) {
      AddVbrFrame(gfc);
			}

    if(cfg->analysis && gfc->pinfo != NULL) {
        int     framesize = 576 * cfg->mode_gr;
        for(ch = 0; ch < cfg->channels_out; ch++) {
            int     j;
            for(j = 0; j < FFTOFFSET; j++)
                gfc->pinfo->pcmdata[ch][j] = gfc->pinfo->pcmdata[ch][j + framesize];
            for(j = FFTOFFSET; j < 1600; j++) {
                gfc->pinfo->pcmdata[ch][j] = inbuf[ch][j - FFTOFFSET];
            }
        }
        gfc->sv_qnt.masking_lower = 1.0;

        set_frame_pinfo(gfc, masking);
    }

    ++gfc->ov_enc.frame_number;

    updateStats(gfc);

  return mp3count;
	}



/*
 *      MP3 bitstream Output interface for LAME
 *
 *      Copyright (c) 1999-2000 Mark Taylor
 *      Copyright (c) 1999-2002 Takehiro Tominaga
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
 *
 * $Id: bitstream.c,v 1.99 2017/08/31 14:14:46 robert Exp $
 */





/* unsigned int is at least this large:  */
/* we work with ints, so when doing bit manipulation, we limit
 * ourselves to MAX_LENGTH-2 just to be on the safe side */
#define MAX_LENGTH      32



#ifdef DEBUG
static int hogege;
#endif



static int calcFrameLength(SessionConfig_t const *const cfg, int kbps, int pad) {

  return 8 * ((cfg->version + 1) * 72000 * kbps / cfg->samplerate_out + pad);
	}


/***********************************************************************
 * compute bitsperframe and mean_bits for a layer III frame
 **********************************************************************/
int getframebits(const lame_internal_flags * gfc) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t const *const eov = &gfc->ov_enc;
    int     bit_rate;

    /* get bitrate in kbps [?] */
    if(eov->bitrate_index)
        bit_rate = bitrate_table[cfg->version][eov->bitrate_index];
    else
        bit_rate = cfg->avg_bitrate;
    /*assert(bit_rate <= 550); */
    assert(8 <= bit_rate && bit_rate <= 640);

  /* main encoding routine toggles padding on and off */
  /* one Layer3 Slot consists of 8 bits */
  return calcFrameLength(cfg, bit_rate, eov->padding);
	}

int get_max_frame_buffer_size_by_constraint(SessionConfig_t const * cfg, int constraint) {
    int     maxmp3buf = 0;

    if(cfg->avg_bitrate > 320) {
        /* in freeformat the buffer is constant */
        if(constraint == MDB_STRICT_ISO) {
            maxmp3buf = calcFrameLength(cfg, cfg->avg_bitrate, 0);
        }
        else {
            /* maximum allowed bits per granule are 7680 */
            maxmp3buf = 7680 * (cfg->version + 1);
        }
			}
    else {
        int     max_kbps;
        if(cfg->samplerate_out < 16000) {
            max_kbps = bitrate_table[cfg->version][8]; /* default: allow 64 kbps (MPEG-2.5) */
        }
        else {
            max_kbps = bitrate_table[cfg->version][14];
        }
        switch(constraint) 
        {
        default:
        case MDB_DEFAULT:
            /* Bouvigne suggests this more lax interpretation of the ISO doc instead of using 8*960. */
            /* All mp3 decoders should have enough buffer to handle this value: size of a 320kbps 32kHz frame */
            maxmp3buf = 8 * 1440;
            break;
        case MDB_STRICT_ISO:
            maxmp3buf = calcFrameLength(cfg, max_kbps, 0);
            break;
        case MDB_MAXIMUM:
            maxmp3buf = 7680 * (cfg->version + 1);
            break;
        }
    }
    return maxmp3buf;
	}


static void putheader_bits(lame_internal_flags * gfc) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *const esv = &gfc->sv_enc;
    Bit_stream_struc *bs = &gfc->bs;

#ifdef DEBUG
    hogege += cfg->sideinfo_len * 8;
#endif
    memcpy(&bs->buf[bs->buf_byte_idx], esv->header[esv->w_ptr].buf, cfg->sideinfo_len);
    bs->buf_byte_idx += cfg->sideinfo_len;
    bs->totbit += cfg->sideinfo_len * 8;
    esv->w_ptr = (esv->w_ptr + 1) & (MAX_HEADER_BUF - 1);
	}




/*write j bits into the bit stream */
inline static void putbits2(lame_internal_flags * gfc, int val, int j) {
    EncStateVar_t const *const esv = &gfc->sv_enc;
    Bit_stream_struc *bs;
    bs = &gfc->bs;

    assert(j < MAX_LENGTH - 2);

    while(j > 0) {
        int     k;
        if(bs->buf_bit_idx == 0) {
            bs->buf_bit_idx = 8;
            bs->buf_byte_idx++;
            assert(bs->buf_byte_idx < BUFFER_SIZE);
            assert(esv->header[esv->w_ptr].write_timing >= bs->totbit);
            if(esv->header[esv->w_ptr].write_timing == bs->totbit) {
                putheader_bits(gfc);
            }
            bs->buf[bs->buf_byte_idx] = 0;
        }

        k = Min(j, bs->buf_bit_idx);
        j -= k;

        bs->buf_bit_idx -= k;

        assert(j < MAX_LENGTH); /* 32 too large on 32 bit machines */
        assert(bs->buf_bit_idx < MAX_LENGTH);

        bs->buf[bs->buf_byte_idx] |= ((val >> j) << bs->buf_bit_idx);
        bs->totbit += k;
    }
	}

/*write j bits into the bit stream, ignoring frame headers */
inline static void putbits_noheaders(lame_internal_flags * gfc, int val, int j) {
    Bit_stream_struc *bs;
    bs = &gfc->bs;

    assert(j < MAX_LENGTH - 2);

    while(j > 0) {
        int     k;
        if(bs->buf_bit_idx == 0) {
            bs->buf_bit_idx = 8;
            bs->buf_byte_idx++;
            assert(bs->buf_byte_idx < BUFFER_SIZE);
            bs->buf[bs->buf_byte_idx] = 0;
        }

        k = Min(j, bs->buf_bit_idx);
        j -= k;

        bs->buf_bit_idx -= k;

        assert(j < MAX_LENGTH); /* 32 too large on 32 bit machines */
        assert(bs->buf_bit_idx < MAX_LENGTH);

        bs->buf[bs->buf_byte_idx] |= ((val >> j) << bs->buf_bit_idx);
        bs->totbit += k;
    }
}


/*
  Some combinations of bitrate, Fs, and stereo make it impossible to stuff
  out a frame using just main_data, due to the limited number of bits to
  indicate main_data_length. In these situations, we put stuffing bits into
  the ancillary data...
*/
inline static void drain_into_ancillary(lame_internal_flags * gfc, int remainingBits) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *const esv = &gfc->sv_enc;
    int     i;
    assert(remainingBits >= 0);

    if(remainingBits >= 8) {
        putbits2(gfc, 0x4c, 8);
        remainingBits -= 8;
    }
    if(remainingBits >= 8) {
        putbits2(gfc, 0x41, 8);
        remainingBits -= 8;
    }
    if(remainingBits >= 8) {
        putbits2(gfc, 0x4d, 8);
        remainingBits -= 8;
    }
    if(remainingBits >= 8) {
        putbits2(gfc, 0x45, 8);
        remainingBits -= 8;
    }

    if(remainingBits >= 32) {
        const char *const version = get_lame_short_version();
        if(remainingBits >= 32)
            for(i = 0; i < (int) strlen(version) && remainingBits >= 8; ++i) {
                remainingBits -= 8;
                putbits2(gfc, version[i], 8);
            }
    }

    for(; remainingBits >= 1; remainingBits -= 1) {
        putbits2(gfc, esv->ancillary_flag, 1);
        esv->ancillary_flag ^= !cfg->disable_reservoir;
    }

    assert(remainingBits == 0);

	}

/*write N bits into the header */
inline static void writeheader(lame_internal_flags * gfc, int val, int j) {
    EncStateVar_t *const esv = &gfc->sv_enc;
    int     ptr = esv->header[esv->h_ptr].ptr;

    while(j > 0) {
        int const k = Min(j, 8 - (ptr & 7));
        j -= k;
        assert(j < MAX_LENGTH); /* >> 32  too large for 32 bit machines */
        esv->header[esv->h_ptr].buf[ptr >> 3]
            |= ((val >> j)) << (8 - (ptr & 7) - k);
        ptr += k;
    }
    esv->header[esv->h_ptr].ptr = ptr;
}


static int CRC_update(int value, int crc) {
    int     i;
    value <<= 8;

    for(i = 0; i < 8; i++) {
        value <<= 1;
        crc <<= 1;

        if(((crc ^ value) & 0x10000))
            crc ^= CRC16_POLYNOMIAL;
    }
    return crc;
	}


void CRC_writeheader(lame_internal_flags const *gfc, char *header) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     crc = 0xffff;    /* (jo) init crc16 for error_protection */
    int     i;

    crc = CRC_update(((unsigned char *) header)[2], crc);
    crc = CRC_update(((unsigned char *) header)[3], crc);
    for(i = 6; i < cfg->sideinfo_len; i++) {
        crc = CRC_update(((unsigned char *) header)[i], crc);
    }

    header[4] = crc >> 8;
    header[5] = crc & 255;
	}

inline static void encodeSideInfo2(lame_internal_flags * gfc, int bitsPerFrame) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t const *const eov = &gfc->ov_enc;
    EncStateVar_t *const esv = &gfc->sv_enc;
    III_side_info_t *l3_side;
    int     gr, ch;

    l3_side = &gfc->l3_side;
    esv->header[esv->h_ptr].ptr = 0;
    memset(esv->header[esv->h_ptr].buf, 0, cfg->sideinfo_len);
    if(cfg->samplerate_out < 16000)
        writeheader(gfc, 0xffe, 12);
    else
        writeheader(gfc, 0xfff, 12);
    writeheader(gfc, (cfg->version), 1);
    writeheader(gfc, 4 - 3, 2);
    writeheader(gfc, (!cfg->error_protection), 1);
    writeheader(gfc, (eov->bitrate_index), 4);
    writeheader(gfc, (cfg->samplerate_index), 2);
    writeheader(gfc, (eov->padding), 1);
    writeheader(gfc, (cfg->extension), 1);
    writeheader(gfc, (cfg->mode), 2);
    writeheader(gfc, (eov->mode_ext), 2);
    writeheader(gfc, (cfg->copyright), 1);
    writeheader(gfc, (cfg->original), 1);
    writeheader(gfc, (cfg->emphasis), 2);
    if(cfg->error_protection) {
        writeheader(gfc, 0, 16); /* dummy */
    }

    if(cfg->version == 1) {
        /* MPEG1 */
        assert(l3_side->main_data_begin >= 0);
        writeheader(gfc, (l3_side->main_data_begin), 9);

        if(cfg->channels_out == 2)
            writeheader(gfc, l3_side->private_bits, 3);
        else
            writeheader(gfc, l3_side->private_bits, 5);

        for(ch = 0; ch < cfg->channels_out; ch++) {
            int     band;
            for(band = 0; band < 4; band++) {
                writeheader(gfc, l3_side->scfsi[ch][band], 1);
            }
        }

        for(gr = 0; gr < 2; gr++) {
            for(ch = 0; ch < cfg->channels_out; ch++) {
                gr_info *const gi = &l3_side->tt[gr][ch];
                writeheader(gfc, gi->part2_3_length + gi->part2_length, 12);
                writeheader(gfc, gi->big_values / 2, 9);
                writeheader(gfc, gi->global_gain, 8);
                writeheader(gfc, gi->scalefac_compress, 4);

                if(gi->block_type != NORM_TYPE) {
                    writeheader(gfc, 1, 1); /* window_switching_flag */
                    writeheader(gfc, gi->block_type, 2);
                    writeheader(gfc, gi->mixed_block_flag, 1);

                    if(gi->table_select[0] == 14)
                        gi->table_select[0] = 16;
                    writeheader(gfc, gi->table_select[0], 5);
                    if(gi->table_select[1] == 14)
                        gi->table_select[1] = 16;
                    writeheader(gfc, gi->table_select[1], 5);

                    writeheader(gfc, gi->subblock_gain[0], 3);
                    writeheader(gfc, gi->subblock_gain[1], 3);
                    writeheader(gfc, gi->subblock_gain[2], 3);
                }
                else {
                    writeheader(gfc, 0, 1); /* window_switching_flag */
                    if(gi->table_select[0] == 14)
                        gi->table_select[0] = 16;
                    writeheader(gfc, gi->table_select[0], 5);
                    if(gi->table_select[1] == 14)
                        gi->table_select[1] = 16;
                    writeheader(gfc, gi->table_select[1], 5);
                    if(gi->table_select[2] == 14)
                        gi->table_select[2] = 16;
                    writeheader(gfc, gi->table_select[2], 5);

                    assert(0 <= gi->region0_count && gi->region0_count < 16);
                    assert(0 <= gi->region1_count && gi->region1_count < 8);
                    writeheader(gfc, gi->region0_count, 4);
                    writeheader(gfc, gi->region1_count, 3);
                }
                writeheader(gfc, gi->preflag, 1);
                writeheader(gfc, gi->scalefac_scale, 1);
                writeheader(gfc, gi->count1table_select, 1);
            }
        }
    }
    else {
        /* MPEG2 */
        assert(l3_side->main_data_begin >= 0);
        writeheader(gfc, (l3_side->main_data_begin), 8);
        writeheader(gfc, l3_side->private_bits, cfg->channels_out);

        gr = 0;
        for(ch = 0; ch < cfg->channels_out; ch++) {
            gr_info *const gi = &l3_side->tt[gr][ch];
            writeheader(gfc, gi->part2_3_length + gi->part2_length, 12);
            writeheader(gfc, gi->big_values / 2, 9);
            writeheader(gfc, gi->global_gain, 8);
            writeheader(gfc, gi->scalefac_compress, 9);

            if(gi->block_type != NORM_TYPE) {
                writeheader(gfc, 1, 1); /* window_switching_flag */
                writeheader(gfc, gi->block_type, 2);
                writeheader(gfc, gi->mixed_block_flag, 1);

                if(gi->table_select[0] == 14)
                    gi->table_select[0] = 16;
                writeheader(gfc, gi->table_select[0], 5);
                if(gi->table_select[1] == 14)
                    gi->table_select[1] = 16;
                writeheader(gfc, gi->table_select[1], 5);

                writeheader(gfc, gi->subblock_gain[0], 3);
                writeheader(gfc, gi->subblock_gain[1], 3);
                writeheader(gfc, gi->subblock_gain[2], 3);
            }
            else {
                writeheader(gfc, 0, 1); /* window_switching_flag */
                if(gi->table_select[0] == 14)
                    gi->table_select[0] = 16;
                writeheader(gfc, gi->table_select[0], 5);
                if(gi->table_select[1] == 14)
                    gi->table_select[1] = 16;
                writeheader(gfc, gi->table_select[1], 5);
                if(gi->table_select[2] == 14)
                    gi->table_select[2] = 16;
                writeheader(gfc, gi->table_select[2], 5);

                assert(0 <= gi->region0_count && gi->region0_count < 16);
                assert(0 <= gi->region1_count && gi->region1_count < 8);
                writeheader(gfc, gi->region0_count, 4);
                writeheader(gfc, gi->region1_count, 3);
            }

            writeheader(gfc, gi->scalefac_scale, 1);
            writeheader(gfc, gi->count1table_select, 1);
        }
			}

    if(cfg->error_protection) {
        /* (jo) error_protection: add crc16 information to header */
        CRC_writeheader(gfc, esv->header[esv->h_ptr].buf);
		  }

    {
        int const old = esv->h_ptr;
        assert(esv->header[old].ptr == cfg->sideinfo_len * 8);

        esv->h_ptr = (old + 1) & (MAX_HEADER_BUF - 1);
        esv->header[esv->h_ptr].write_timing = esv->header[old].write_timing + bitsPerFrame;

        if(esv->h_ptr == esv->w_ptr) {
            /* yikes! we are out of header buffer space */
            ERRORF(gfc, "Error: MAX_HEADER_BUF too small in bitstream.c \n");
        }

    }
	}


inline static int huffman_coder_count1(lame_internal_flags * gfc, gr_info const *gi) {
    /* Write count1 area */
    struct HUFFCODETAB const *const h = &ht[gi->count1table_select + 32];
    int     i, bits = 0;
#ifdef DEBUG
    int     gegebo = gfc->bs.totbit;
#endif

    int const *ix = &gi->l3_enc[gi->big_values];
    FLOAT const *xr = &gi->xr[gi->big_values];
    assert(gi->count1table_select < 2);

    for(i = (gi->count1 - gi->big_values) / 4; i > 0; --i) {
        int     huffbits = 0;
        int     p = 0, v;

        v = ix[0];
        if(v) {
            p += 8;
            if(xr[0] < 0.0f)
                huffbits++;
            assert(v <= 1);
        }

        v = ix[1];
        if(v) {
            p += 4;
            huffbits *= 2;
            if(xr[1] < 0.0f)
                huffbits++;
            assert(v <= 1);
        }

        v = ix[2];
        if(v) {
            p += 2;
            huffbits *= 2;
            if(xr[2] < 0.0f)
                huffbits++;
            assert(v <= 1);
        }

        v = ix[3];
        if(v) {
            p++;
            huffbits *= 2;
            if(xr[3] < 0.0f)
                huffbits++;
            assert(v <= 1);
        }

        ix += 4;
        xr += 4;
        putbits2(gfc, huffbits + h->table[p], h->hlen[p]);
        bits += h->hlen[p];
    }
#ifdef DEBUG
    DEBUGF(gfc, "count1: real: %ld counted:%d (bigv %d count1len %d)\n",
           gfc->bs.totbit - gegebo, gi->count1bits, gi->big_values, gi->count1);
#endif
  return bits;
	}



/*
  Implements the pseudocode of page 98 of the IS
  */
inline static int Huffmancode(lame_internal_flags * const gfc, const unsigned int tableindex,
            int start, int end, gr_info const *gi) {
    struct HUFFCODETAB const *const h = &ht[tableindex];
    unsigned int const linbits = h->xlen;
    int     i, bits = 0;

    assert(tableindex < 32u);
    if(!tableindex)
        return bits;

    for(i = start; i < end; i += 2) {
        int16_t  cbits = 0;
        uint16_t xbits = 0;
        unsigned int xlen = h->xlen;
        unsigned int ext = 0;
        unsigned int x1 = gi->l3_enc[i];
        unsigned int x2 = gi->l3_enc[i + 1];

        assert(gi->l3_enc[i] >= 0);
        assert(gi->l3_enc[i+1] >= 0);

        if(x1 != 0u) {
            if(gi->xr[i] < 0.0f)
                ext++;
            cbits--;
        }

        if(tableindex > 15u) {
            /* use ESC-words */
            if(x1 >= 15u) {
                uint16_t const linbits_x1 = x1 - 15u;
                assert(linbits_x1 <= h->linmax);
                ext |= linbits_x1 << 1u;
                xbits = linbits;
                x1 = 15u;
            }

            if(x2 >= 15u) {
                uint16_t const linbits_x2 = x2 - 15u;
                assert(linbits_x2 <= h->linmax);
                ext <<= linbits;
                ext |= linbits_x2;
                xbits += linbits;
                x2 = 15u;
            }
            xlen = 16;
        }

        if(x2 != 0u) {
            ext <<= 1;
            if(gi->xr[i + 1] < 0.0f)
                ext++;
            cbits--;
        }

        assert((x1 | x2) < 16u);

        x1 = x1 * xlen + x2;
        xbits -= cbits;
        cbits += h->hlen[x1];

        assert(cbits <= MAX_LENGTH);
        assert(xbits <= MAX_LENGTH);

        putbits2(gfc, h->table[x1], cbits);
        putbits2(gfc, (int)ext, xbits);
        bits += cbits + xbits;
    }
    return bits;
	}

/*
  Note the discussion of huffmancodebits() on pages 28
  and 29 of the IS, as well as the definitions of the side
  information on pages 26 and 27.
  */
static int ShortHuffmancodebits(lame_internal_flags * gfc, gr_info const *gi) {
    int     bits;
    int     region1Start;

    region1Start = 3 * gfc->scalefac_band.s[3];
    if(region1Start > gi->big_values)
        region1Start = gi->big_values;

    /* short blocks do not have a region2 */
    bits = Huffmancode(gfc, gi->table_select[0], 0, region1Start, gi);
    bits += Huffmancode(gfc, gi->table_select[1], region1Start, gi->big_values, gi);
    return bits;
}

static int LongHuffmancodebits(lame_internal_flags * gfc, gr_info const *gi) {
    unsigned int i;
    int     bigvalues, bits;
    int     region1Start, region2Start;

    bigvalues = gi->big_values;
    assert(0 <= bigvalues && bigvalues <= 576);

    assert(gi->region0_count >= -1);
    assert(gi->region1_count >= -1);
    i = gi->region0_count + 1;
    assert((size_t) i < dimension_of(gfc->scalefac_band.l));
    region1Start = gfc->scalefac_band.l[i];
    i += gi->region1_count + 1;
    assert((size_t) i < dimension_of(gfc->scalefac_band.l));
    region2Start = gfc->scalefac_band.l[i];

    if(region1Start > bigvalues)
        region1Start = bigvalues;

    if(region2Start > bigvalues)
        region2Start = bigvalues;

    bits = Huffmancode(gfc, gi->table_select[0], 0, region1Start, gi);
    bits += Huffmancode(gfc, gi->table_select[1], region1Start, region2Start, gi);
    bits += Huffmancode(gfc, gi->table_select[2], region2Start, bigvalues, gi);
    return bits;
}

inline static int writeMainData(lame_internal_flags * const gfc) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    III_side_info_t const *const l3_side = &gfc->l3_side;
    int     gr, ch, sfb, data_bits, tot_bits = 0;

    if(cfg->version == 1) {
        /* MPEG 1 */
        for(gr = 0; gr < 2; gr++) {
            for(ch = 0; ch < cfg->channels_out; ch++) {
                gr_info const *const gi = &l3_side->tt[gr][ch];
                int const slen1 = slen1_tab[gi->scalefac_compress];
                int const slen2 = slen2_tab[gi->scalefac_compress];
                data_bits = 0;
#ifdef DEBUG
                hogege = gfc->bs.totbit;
#endif
                for(sfb = 0; sfb < gi->sfbdivide; sfb++) {
                    if(gi->scalefac[sfb] == -1)
                        continue; /* scfsi is used */
                    putbits2(gfc, gi->scalefac[sfb], slen1);
                    data_bits += slen1;
                }
                for(; sfb < gi->sfbmax; sfb++) {
                    if(gi->scalefac[sfb] == -1)
                        continue; /* scfsi is used */
                    putbits2(gfc, gi->scalefac[sfb], slen2);
                    data_bits += slen2;
                }
                assert(data_bits == gi->part2_length);

                if(gi->block_type == SHORT_TYPE) {
                    data_bits += ShortHuffmancodebits(gfc, gi);
                }
                else {
                    data_bits += LongHuffmancodebits(gfc, gi);
                }
                data_bits += huffman_coder_count1(gfc, gi);
#ifdef DEBUG
                DEBUGF(gfc, "<%ld> ", gfc->bs.totbit - hogege);
#endif
                /* does bitcount in quantize.c agree with actual bit count? */
                assert(data_bits == gi->part2_3_length + gi->part2_length);
                tot_bits += data_bits;
            }           /* for ch */
        }               /* for gr */
    }
    else {
        /* MPEG 2 */
        gr = 0;
        for(ch = 0; ch < cfg->channels_out; ch++) {
            gr_info const *const gi = &l3_side->tt[gr][ch];
            int     i, sfb_partition, scale_bits = 0;
            assert(gi->sfb_partition_table);
            data_bits = 0;
#ifdef DEBUG
            hogege = gfc->bs.totbit;
#endif
            sfb = 0;
            sfb_partition = 0;

            if(gi->block_type == SHORT_TYPE) {
                for(; sfb_partition < 4; sfb_partition++) {
                    int const sfbs = gi->sfb_partition_table[sfb_partition] / 3;
                    int const slen = gi->slen[sfb_partition];
                    for(i = 0; i < sfbs; i++, sfb++) {
                        putbits2(gfc, Max(gi->scalefac[sfb * 3 + 0], 0), slen);
                        putbits2(gfc, Max(gi->scalefac[sfb * 3 + 1], 0), slen);
                        putbits2(gfc, Max(gi->scalefac[sfb * 3 + 2], 0), slen);
                        scale_bits += 3 * slen;
                    }
                }
                data_bits += ShortHuffmancodebits(gfc, gi);
            }
            else {
                for(; sfb_partition < 4; sfb_partition++) {
                    int const sfbs = gi->sfb_partition_table[sfb_partition];
                    int const slen = gi->slen[sfb_partition];
                    for(i = 0; i < sfbs; i++, sfb++) {
                        putbits2(gfc, Max(gi->scalefac[sfb], 0), slen);
                        scale_bits += slen;
                    }
                }
                data_bits += LongHuffmancodebits(gfc, gi);
            }
            data_bits += huffman_coder_count1(gfc, gi);
#ifdef DEBUG
            DEBUGF(gfc, "<%ld> ", gfc->bs.totbit - hogege);
#endif
            /* does bitcount in quantize.c agree with actual bit count? */
            assert(data_bits == gi->part2_3_length);
            assert(scale_bits == gi->part2_length);
            tot_bits += scale_bits + data_bits;
        }               /* for ch */
    }                   /* for gf */

  return tot_bits;
	}                       /* main_data */



/* compute the number of bits required to flush all mp3 frames
   currently in the buffer.  This should be the same as the
   reservoir size.  Only call this routine between frames - i.e.
   only after all headers and data have been added to the buffer
   by format_bitstream().

   Also compute total_bits_output =
       size of mp3 buffer (including frame headers which may not
       have yet been send to the mp3 buffer) +
       number of bits needed to flush all mp3 frames.

   total_bytes_output is the size of the mp3 output buffer if
   lame_encode_flush_nogap() was called right now.

 */
int compute_flushbits(const lame_internal_flags * gfc, int *total_bytes_output) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t const *const esv = &gfc->sv_enc;
    int     flushbits, remaining_headers;
    int     bitsPerFrame;
    int     last_ptr, first_ptr;
    first_ptr = esv->w_ptr; /* first header to add to bitstream */
    last_ptr = esv->h_ptr - 1; /* last header to add to bitstream */
    if(last_ptr == -1)
        last_ptr = MAX_HEADER_BUF - 1;

    /* add this many bits to bitstream so we can flush all headers */
    flushbits = esv->header[last_ptr].write_timing - gfc->bs.totbit;
    *total_bytes_output = flushbits;

    if(flushbits >= 0) {
        /* if flushbits >= 0, some headers have not yet been written */
        /* reduce flushbits by the size of the headers */
        remaining_headers = 1 + last_ptr - first_ptr;
        if(last_ptr < first_ptr)
            remaining_headers = 1 + last_ptr - first_ptr + MAX_HEADER_BUF;
        flushbits -= remaining_headers * 8 * cfg->sideinfo_len;
    }


    /* finally, add some bits so that the last frame is complete
     * these bits are not necessary to decode the last frame, but
     * some decoders will ignore last frame if these bits are missing
     */
    bitsPerFrame = getframebits(gfc);
    flushbits += bitsPerFrame;
    *total_bytes_output += bitsPerFrame;
    /* round up:   */
    if(*total_bytes_output % 8)
        *total_bytes_output = 1 + (*total_bytes_output / 8);
    else
        *total_bytes_output = (*total_bytes_output / 8);
    *total_bytes_output += gfc->bs.buf_byte_idx + 1;


    if(flushbits < 0) {
#if 0
        /* if flushbits < 0, this would mean that the buffer looks like:
         * (data...)  last_header  (data...)  (extra data that should not be here...)
         */
        DEBUGF(gfc, "last header write_timing = %i \n", esv->header[last_ptr].write_timing);
        DEBUGF(gfc, "first header write_timing = %i \n", esv->header[first_ptr].write_timing);
        DEBUGF(gfc, "bs.totbit:                 %i \n", gfc->bs.totbit);
        DEBUGF(gfc, "first_ptr, last_ptr        %i %i \n", first_ptr, last_ptr);
        DEBUGF(gfc, "remaining_headers =        %i \n", remaining_headers);
        DEBUGF(gfc, "bitsperframe:              %i \n", bitsPerFrame);
        DEBUGF(gfc, "sidelen:                   %i \n", cfg->sideinfo_len);
#endif
        ERRORF(gfc, "strange error flushing buffer ... \n");
    }

  return flushbits;
	}


void flush_bitstream(lame_internal_flags * gfc) {
    EncStateVar_t *const esv = &gfc->sv_enc;
    III_side_info_t *l3_side;
    int     nbytes;
    int     flushbits;
    int     last_ptr = esv->h_ptr - 1; /* last header to add to bitstream */

    if(last_ptr == -1)
        last_ptr = MAX_HEADER_BUF - 1;
    l3_side = &gfc->l3_side;


    if((flushbits = compute_flushbits(gfc, &nbytes)) < 0)
        return;
    drain_into_ancillary(gfc, flushbits);

    /* check that the 100% of the last frame has been written to bitstream */
    assert(esv->header[last_ptr].write_timing + getframebits(gfc)
           == gfc->bs.totbit);

    /* we have padded out all frames with ancillary data, which is the
       same as filling the bitreservoir with ancillary data, so : */
    esv->ResvSize = 0;
    l3_side->main_data_begin = 0;
	}



void add_dummy_byte(lame_internal_flags * gfc, unsigned char val, unsigned int n) {
    EncStateVar_t *const esv = &gfc->sv_enc;
    int     i;

    while(n-- > 0u) {
        putbits_noheaders(gfc, val, 8);

        for(i = 0; i < MAX_HEADER_BUF; ++i)
            esv->header[i].write_timing += 8;
    }
	}


/*
  format_bitstream()

  This is called after a frame of audio has been quantized and coded.
  It will write the encoded audio to the bitstream. Note that
  from a layer3 encoder's perspective the bit stream is primarily
  a series of main_data() blocks, with header and side information
  inserted at the proper locations to maintain framing. (See Figure A.7
  in the IS).
  */
int format_bitstream(lame_internal_flags * gfc) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *const esv = &gfc->sv_enc;
    int     bits, nbytes;
    III_side_info_t *l3_side;
    int     bitsPerFrame;
    l3_side = &gfc->l3_side;

    bitsPerFrame = getframebits(gfc);
    drain_into_ancillary(gfc, l3_side->resvDrain_pre);

    encodeSideInfo2(gfc, bitsPerFrame);
    bits = 8 * cfg->sideinfo_len;
    bits += writeMainData(gfc);
    drain_into_ancillary(gfc, l3_side->resvDrain_post);
    bits += l3_side->resvDrain_post;

    l3_side->main_data_begin += (bitsPerFrame - bits) / 8;

    /* compare number of bits needed to clear all buffered mp3 frames
     * with what we think the resvsize is: */
    if(compute_flushbits(gfc, &nbytes) != esv->ResvSize) {
        ERRORF(gfc, "Internal buffer inconsistency. flushbits <> ResvSize");
    }


    /* compare main_data_begin for the next frame with what we
     * think the resvsize is: */
    if((l3_side->main_data_begin * 8) != esv->ResvSize) {
        ERRORF(gfc, "bit reservoir error: \n"
               "l3_side->main_data_begin: %i \n"
               "Resvoir size:             %i \n"
               "resv drain (post)         %i \n"
               "resv drain (pre)          %i \n"
               "header and sideinfo:      %i \n"
               "data bits:                %i \n"
               "total bits:               %i (remainder: %i) \n"
               "bitsperframe:             %i \n",
               8 * l3_side->main_data_begin,
               esv->ResvSize,
               l3_side->resvDrain_post,
               l3_side->resvDrain_pre,
               8 * cfg->sideinfo_len,
               bits - l3_side->resvDrain_post - 8 * cfg->sideinfo_len,
               bits, bits % 8, bitsPerFrame);

        ERRORF(gfc, "This is a fatal error.  It has several possible causes:");
        ERRORF(gfc, "90%%  LAME compiled with buggy version of gcc using advanced optimizations");
        ERRORF(gfc, " 9%%  Your system is overclocked");
        ERRORF(gfc, " 1%%  bug in LAME encoding library");

        esv->ResvSize = l3_side->main_data_begin * 8;
    };
    assert(gfc->bs.totbit % 8 == 0);

    if(gfc->bs.totbit > 1000000000) {
        /* to avoid totbit overflow, (at 8h encoding at 128kbs) lets reset bit counter */
        int     i;
        for(i = 0; i < MAX_HEADER_BUF; ++i)
            esv->header[i].write_timing -= gfc->bs.totbit;
        gfc->bs.totbit = 0;
    }


  return 0;
	}


static int do_gain_analysis(lame_internal_flags * gfc, unsigned char* buffer, int minimum) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    RpgStateVar_t const *const rsv = &gfc->sv_rpg;
    RpgResult_t *const rov = &gfc->ov_rpg;

#ifdef DECODE_ON_THE_FLY
    if(cfg->decode_on_the_fly) { /* decode the frame */
        sample_t pcm_buf[2][1152];
        int     mp3_in = minimum;
        int     samples_out = -1;

        /* re-synthesis to pcm.  Repeat until we get a samples_out=0 */
        while(samples_out != 0) {

            samples_out = hip_decode1_unclipped(gfc->hip, buffer, mp3_in, pcm_buf[0], pcm_buf[1]);
            /* samples_out = 0:  need more data to decode
             * samples_out = -1:  error.  Lets assume 0 pcm output
             * samples_out = number of samples output */

            /* set the lenght of the mp3 input buffer to zero, so that in the
             * next iteration of the loop we will be querying mpglib about
             * buffered data */
            mp3_in = 0;

            if(samples_out == -1) {
                /* error decoding. Not fatal, but might screw up
                 * the ReplayGain tag. What should we do? Ignore for now */
                samples_out = 0;
							}
            if(samples_out > 0) {
                /* process the PCM data */

                /* this should not be possible, and indicates we have
                 * overflown the pcm_buf buffer */
                assert(samples_out <= 1152);

                if(cfg->findPeakSample) {
                    int     i;
                    /* FIXME: is this correct? maybe Max(fabs(pcm),PeakSample) */
                    for(i=0; i < samples_out; i++) {
                        if(pcm_buf[0][i] > rov->PeakSample)
                            rov->PeakSample = pcm_buf[0][i];
                        else if(-pcm_buf[0][i] > rov->PeakSample)
                            rov->PeakSample = -pcm_buf[0][i];
											}
                    if(cfg->channels_out > 1)
                        for(i=0; i < samples_out; i++) {
                            if(pcm_buf[1][i] > rov->PeakSample)
                                rov->PeakSample = pcm_buf[1][i];
                            else if(-pcm_buf[1][i] > rov->PeakSample)
                                rov->PeakSample = -pcm_buf[1][i];
                        }
                }

                if(cfg->findReplayGain)
                  if(AnalyzeSamples(rsv->rgdata, pcm_buf[0], pcm_buf[1], samples_out,
                    cfg->channels_out) == GAIN_ANALYSIS_ERROR)
                    return -6;

            }       /* if(samples_out>0) */
        }           /* while(samples_out!=0) */
    }               /* if(gfc->decode_on_the_fly) */
#endif

  return minimum;
	}

static int do_copy_buffer(lame_internal_flags * gfc, unsigned char *buffer, int size) {
  Bit_stream_struc *const bs = &gfc->bs;
  int const minimum = bs->buf_byte_idx + 1;

  if(minimum <= 0)
      return 0;
  if(minimum > size)
      return -1;      /* buffer is too small */
  memcpy(buffer, bs->buf, minimum);
  bs->buf_byte_idx = -1;
  bs->buf_bit_idx = 0;

  return minimum;
	}

/* copy data out of the internal MP3 bit buffer into a user supplied
   unsigned char buffer.

   mp3data=0      indicates data in buffer is an id3tags and VBR tags
   mp3data=1      data is real mp3 frame data.


*/
int copy_buffer(lame_internal_flags * gfc, unsigned char *buffer, int size, int mp3data) {
    int const minimum = do_copy_buffer(gfc, buffer, size);

    if(minimum > 0 && mp3data) {
        UpdateMusicCRC(&gfc->nMusicCRC, buffer, minimum);

        /** sum number of bytes belonging to the mp3 stream
         *  this info will be written into the Xing/LAME header for seeking
         */
        gfc->VBR_seek_table.nBytesWritten += minimum;

        return do_gain_analysis(gfc, buffer, minimum);
    }                   /* if(mp3data) */

  return minimum;
	}


void init_bit_stream_w(lame_internal_flags * gfc) {
    EncStateVar_t *const esv = &gfc->sv_enc;

    esv->h_ptr = esv->w_ptr = 0;
    esv->header[esv->h_ptr].write_timing = 0;

    gfc->bs.buf = lame_calloc(unsigned char, BUFFER_SIZE);
    gfc->bs.buf_size = BUFFER_SIZE;
    gfc->bs.buf_byte_idx = -1;
    gfc->bs.buf_bit_idx = 0;
    gfc->bs.totbit = 0;
	}

/* end of bitstream.c */



/*
** FFT and FHT routines
**  Copyright 1988, 1993; Ron Mayer
**      Copyright (c) 1999-2000 Takehiro Tominaga
**
**  fht(fz,n);
**      Does a hartley transform of "n" points in the array "fz".
**
** NOTE: This routine uses at least 2 patented algorithms, and may be
**       under the restrictions of a bunch of different organizations.
**       Although I wrote it completely myself; it is kind of a derivative
**       of a routine I once authored and released under the GPL, so it
**       may fall under the free software foundation's restrictions;
**       it was worked on as a Stanford Univ project, so they claim
**       some rights to it; it was further optimized at work here, so
**       I think this company claims parts of it.  The patents are
**       held by R. Bracewell (the FHT algorithm) and O. Buneman (the
**       trig generator), both at Stanford Univ.
**       If it were up to me, I'd say go do whatever you want with it;
**       but it would be polite to give credit to the following people
**       if you use this anywhere:
**           Euler     - probable inventor of the fourier transform.
**           Gauss     - probable inventor of the FFT.
**           Hartley   - probable inventor of the hartley transform.
**           Buneman   - for a really cool trig generator
**           Mayer(me) - for authoring this particular version and
**                       including all the optimizations in one package.
**       Thanks,
**       Ron Mayer; mayer@acuson.com
** and added some optimization by
**           Mather    - idea of using lookup table
**           Takehiro  - some dirty hack for speed up
*/

/* $Id: fft.c,v 1.39 2017/09/06 15:07:29 robert Exp $ */




#define TRI_SIZE (5-1)  /* 1024 =  4**5 */

/* fft.c    */

static const FLOAT costab[TRI_SIZE * 2] = {
    9.238795325112867e-01, 3.826834323650898e-01,
    9.951847266721969e-01, 9.801714032956060e-02,
    9.996988186962042e-01, 2.454122852291229e-02,
    9.999811752826011e-01, 6.135884649154475e-03
};

static void fht(FLOAT * fz, int n) {
    const FLOAT *tri = costab;
    int     k4;
    FLOAT  *fi, *gi;
    FLOAT const *fn;

    n <<= 1;            /* to get BLKSIZE, because of 3DNow! ASM routine */
    fn = fz + n;
    k4 = 4;
    do {
        FLOAT   s1, c1;
        int     i, k1, k2, k3, kx;
        kx = k4 >> 1;
        k1 = k4;
        k2 = k4 << 1;
        k3 = k2 + k1;
        k4 = k2 << 1;
        fi = fz;
        gi = fi + kx;
        do {
            FLOAT   f0, f1, f2, f3;
            f1 = fi[0] - fi[k1];
            f0 = fi[0] + fi[k1];
            f3 = fi[k2] - fi[k3];
            f2 = fi[k2] + fi[k3];
            fi[k2] = f0 - f2;
            fi[0] = f0 + f2;
            fi[k3] = f1 - f3;
            fi[k1] = f1 + f3;
            f1 = gi[0] - gi[k1];
            f0 = gi[0] + gi[k1];
            f3 = SQRT2 * gi[k3];
            f2 = SQRT2 * gi[k2];
            gi[k2] = f0 - f2;
            gi[0] = f0 + f2;
            gi[k3] = f1 - f3;
            gi[k1] = f1 + f3;
            gi += k4;
            fi += k4;
        } while(fi < fn);
        c1 = tri[0];
        s1 = tri[1];
        for(i = 1; i < kx; i++) {
            FLOAT   c2, s2;
            c2 = 1 - (2 * s1) * s1;
            s2 = (2 * s1) * c1;
            fi = fz + i;
            gi = fz + k1 - i;
            do {
                FLOAT   a, b, g0, f0, f1, g1, f2, g2, f3, g3;
                b = s2 * fi[k1] - c2 * gi[k1];
                a = c2 * fi[k1] + s2 * gi[k1];
                f1 = fi[0] - a;
                f0 = fi[0] + a;
                g1 = gi[0] - b;
                g0 = gi[0] + b;
                b = s2 * fi[k3] - c2 * gi[k3];
                a = c2 * fi[k3] + s2 * gi[k3];
                f3 = fi[k2] - a;
                f2 = fi[k2] + a;
                g3 = gi[k2] - b;
                g2 = gi[k2] + b;
                b = s1 * f2 - c1 * g3;
                a = c1 * f2 + s1 * g3;
                fi[k2] = f0 - a;
                fi[0] = f0 + a;
                gi[k3] = g1 - b;
                gi[k1] = g1 + b;
                b = c1 * g2 - s1 * f3;
                a = s1 * g2 + c1 * f3;
                gi[k2] = g0 - a;
                gi[0] = g0 + a;
                fi[k3] = f1 - b;
                fi[k1] = f1 + b;
                gi += k4;
                fi += k4;
            } while(fi < fn);
            c2 = c1;
            c1 = c2 * tri[0] - s1 * tri[1];
            s1 = c2 * tri[1] + s1 * tri[0];
        }
        tri += 2;
    } while(k4 < n);
	}


static const unsigned char rv_tbl[] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe
};

#define ch01(index)  (buffer[chn][index])

#define ml00(f) (window[i        ] * f(i))
#define ml10(f) (window[i + 0x200] * f(i + 0x200))
#define ml20(f) (window[i + 0x100] * f(i + 0x100))
#define ml30(f) (window[i + 0x300] * f(i + 0x300))

#define ml01(f) (window[i + 0x001] * f(i + 0x001))
#define ml11(f) (window[i + 0x201] * f(i + 0x201))
#define ml21(f) (window[i + 0x101] * f(i + 0x101))
#define ml31(f) (window[i + 0x301] * f(i + 0x301))

#define ms00(f) (window_s[i       ] * f(i + k))
#define ms10(f) (window_s[0x7f - i] * f(i + k + 0x80))
#define ms20(f) (window_s[i + 0x40] * f(i + k + 0x40))
#define ms30(f) (window_s[0x3f - i] * f(i + k + 0xc0))

#define ms01(f) (window_s[i + 0x01] * f(i + k + 0x01))
#define ms11(f) (window_s[0x7e - i] * f(i + k + 0x81))
#define ms21(f) (window_s[i + 0x41] * f(i + k + 0x41))
#define ms31(f) (window_s[0x3e - i] * f(i + k + 0xc1))

void fft_short(lame_internal_flags const *const gfc,
          FLOAT x_real[3][BLKSIZE_s], int chn, const sample_t *const buffer[2]) {
    int     i;
    int     j;
    int     b;

#define window_s gfc->cd_psy->window_s
#define window gfc->cd_psy->window

    for(b = 0; b < 3; b++) {
        FLOAT  *x = &x_real[b][BLKSIZE_s / 2];
        short const k = (576 / 3) * (b + 1);
        j = BLKSIZE_s / 8 - 1;
        do {
            FLOAT   f0, f1, f2, f3, w;

            i = rv_tbl[j << 2];

            f0 = ms00(ch01);
            w = ms10(ch01);
            f1 = f0 - w;
            f0 = f0 + w;
            f2 = ms20(ch01);
            w = ms30(ch01);
            f3 = f2 - w;
            f2 = f2 + w;

            x -= 4;
            x[0] = f0 + f2;
            x[2] = f0 - f2;
            x[1] = f1 + f3;
            x[3] = f1 - f3;

            f0 = ms01(ch01);
            w = ms11(ch01);
            f1 = f0 - w;
            f0 = f0 + w;
            f2 = ms21(ch01);
            w = ms31(ch01);
            f3 = f2 - w;
            f2 = f2 + w;

            x[BLKSIZE_s / 2 + 0] = f0 + f2;
            x[BLKSIZE_s / 2 + 2] = f0 - f2;
            x[BLKSIZE_s / 2 + 1] = f1 + f3;
            x[BLKSIZE_s / 2 + 3] = f1 - f3;
        } while(--j >= 0);

#undef window
#undef window_s

        gfc->fft_fht(x, BLKSIZE_s / 2);
        /* BLKSIZE_s/2 because of 3DNow! ASM routine */
    }
	}

void fft_long(lame_internal_flags const *const gfc,
         FLOAT x[BLKSIZE], int chn, const sample_t *const buffer[2]) {
    int     i;
    int     jj = BLKSIZE / 8 - 1;
    x += BLKSIZE / 2;

#define window_s gfc->cd_psy->window_s
#define window gfc->cd_psy->window

    do {
        FLOAT   f0, f1, f2, f3, w;

        i = rv_tbl[jj];
        f0 = ml00(ch01);
        w = ml10(ch01);
        f1 = f0 - w;
        f0 = f0 + w;
        f2 = ml20(ch01);
        w = ml30(ch01);
        f3 = f2 - w;
        f2 = f2 + w;

        x -= 4;
        x[0] = f0 + f2;
        x[2] = f0 - f2;
        x[1] = f1 + f3;
        x[3] = f1 - f3;

        f0 = ml01(ch01);
        w = ml11(ch01);
        f1 = f0 - w;
        f0 = f0 + w;
        f2 = ml21(ch01);
        w = ml31(ch01);
        f3 = f2 - w;
        f2 = f2 + w;

        x[BLKSIZE / 2 + 0] = f0 + f2;
        x[BLKSIZE / 2 + 2] = f0 - f2;
        x[BLKSIZE / 2 + 1] = f1 + f3;
        x[BLKSIZE / 2 + 3] = f1 - f3;
    } while(--jj >= 0);

#undef window
#undef window_s

  gfc->fft_fht(x, BLKSIZE / 2);
  /* BLKSIZE/2 because of 3DNow! ASM routine */
	}

#ifdef HAVE_NASM
extern void fht_3DN(FLOAT * fz, int n);
extern void fht_SSE(FLOAT * fz, int n);
#endif

void init_fft(lame_internal_flags * const gfc) {
    int     i;

    /* The type of window used here will make no real difference, but */
    /* in the interest of merging nspsytune stuff - switch to blackman window */
    for(i = 0; i < BLKSIZE; i++)
        /* blackman window */
        gfc->cd_psy->window[i] = 0.42 - 0.5 * cos(2 * PI * (i + .5) / BLKSIZE) +
            0.08 * cos(4 * PI * (i + .5) / BLKSIZE);

    for(i = 0; i < BLKSIZE_s / 2; i++)
        gfc->cd_psy->window_s[i] = 0.5 * (1.0 - cos(2.0 * PI * (i + 0.5) / BLKSIZE_s));

    gfc->fft_fht = fht;
#ifdef HAVE_NASM
    if(gfc->CPU_features.AMD_3DNow) {
        gfc->fft_fht = fht_3DN;
    }
    else if(gfc->CPU_features.SSE) {
        gfc->fft_fht = fht_SSE;
    }
    else {
        gfc->fft_fht = fht;
    }
#else
#ifdef HAVE_XMMINTRIN_H
#ifdef MIN_ARCH_SSE
    gfc->fft_fht = fht_SSE2;
#endif
#endif
#endif
}


/*
 * MP3 quantization, intrinsics functions
 *
 *      Copyright (c) 2005-2006 Gabriel Bouvigne
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.     See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */




#ifdef HAVE_XMMINTRIN_H

#include <xmmintrin.h>

typedef union {
    int32_t _i_32[4]; /* unions are initialized by its first member */
    float   _float[4];
    __m128  _m128;
} vecfloat_union;

#define TRI_SIZE (5-1)  /* 1024 =  4**5 */
static const FLOAT costab[TRI_SIZE * 2] = {
    9.238795325112867e-01, 3.826834323650898e-01,
    9.951847266721969e-01, 9.801714032956060e-02,
    9.996988186962042e-01, 2.454122852291229e-02,
    9.999811752826011e-01, 6.135884649154475e-03
};


/* make sure functions with SSE instructions maintain their own properly aligned stack */
#define SSE_FUNCTION


SSE_FUNCTION void init_xrpow_core_sse(gr_info * const cod_info, FLOAT xrpow[576], int upper, FLOAT * sum) {
    int     i;
    float   tmp_max = 0;
    float   tmp_sum = 0;
    int     upper4 = (upper / 4) * 4;
    int     rest = upper-upper4;

    const vecfloat_union fabs_mask = {{ 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF }};
    const __m128 vec_fabs_mask = _mm_loadu_ps(&fabs_mask._float[0]);
    vecfloat_union vec_xrpow_max;
    vecfloat_union vec_sum;
    vecfloat_union vec_tmp;

    _mm_prefetch((char *) cod_info->xr, _MM_HINT_T0);
    _mm_prefetch((char *) xrpow, _MM_HINT_T0);

    vec_xrpow_max._m128 = _mm_set_ps1(0);
    vec_sum._m128 = _mm_set_ps1(0);

    for(i = 0; i < upper4; i += 4) {
        vec_tmp._m128 = _mm_loadu_ps(&(cod_info->xr[i])); /* load */
        vec_tmp._m128 = _mm_and_ps(vec_tmp._m128, vec_fabs_mask); /* fabs */
        vec_sum._m128 = _mm_add_ps(vec_sum._m128, vec_tmp._m128);
        vec_tmp._m128 = _mm_sqrt_ps(_mm_mul_ps(vec_tmp._m128, _mm_sqrt_ps(vec_tmp._m128)));
        vec_xrpow_max._m128 = _mm_max_ps(vec_xrpow_max._m128, vec_tmp._m128); /* retrieve max */
        _mm_storeu_ps(&(xrpow[i]), vec_tmp._m128); /* store into xrpow[] */
    }
    vec_tmp._m128 = _mm_set_ps1(0);
    switch(rest) {
        case 3: vec_tmp._float[2] = cod_info->xr[upper4+2];
        case 2: vec_tmp._float[1] = cod_info->xr[upper4+1];
        case 1: vec_tmp._float[0] = cod_info->xr[upper4+0];
            vec_tmp._m128 = _mm_and_ps(vec_tmp._m128, vec_fabs_mask); /* fabs */
            vec_sum._m128 = _mm_add_ps(vec_sum._m128, vec_tmp._m128);
            vec_tmp._m128 = _mm_sqrt_ps(_mm_mul_ps(vec_tmp._m128, _mm_sqrt_ps(vec_tmp._m128)));
            vec_xrpow_max._m128 = _mm_max_ps(vec_xrpow_max._m128, vec_tmp._m128); /* retrieve max */
            switch(rest) {
                case 3: xrpow[upper4+2] = vec_tmp._float[2];
                case 2: xrpow[upper4+1] = vec_tmp._float[1];
                case 1: xrpow[upper4+0] = vec_tmp._float[0];
                default:
                    break;
            }
        default:
            break;
    }
    tmp_sum = vec_sum._float[0] + vec_sum._float[1] + vec_sum._float[2] + vec_sum._float[3];
    {
        float ma = vec_xrpow_max._float[0] > vec_xrpow_max._float[1]
                ? vec_xrpow_max._float[0] : vec_xrpow_max._float[1];
        float mb = vec_xrpow_max._float[2] > vec_xrpow_max._float[3]
                ? vec_xrpow_max._float[2] : vec_xrpow_max._float[3];
        tmp_max = ma > mb ? ma : mb;
    }
    cod_info->xrpow_max = tmp_max;
    *sum = tmp_sum;
}


SSE_FUNCTION static void store4(__m128 v, float* f0, float* f1, float* f2, float* f3) {
    vecfloat_union r;

    r._m128 = v;
    *f0 = r._float[0];
    *f1 = r._float[1];
    *f2 = r._float[2];
    *f3 = r._float[3];
}


SSE_FUNCTION void fht_SSE2(FLOAT * fz, int n) {
    const FLOAT *tri = costab;
    int     k4;
    FLOAT  *fi, *gi;
    FLOAT const *fn;

    n <<= 1;            /* to get BLKSIZE, because of 3DNow! ASM routine */
    fn = fz + n;
    k4 = 4;
    do {
        FLOAT   s1, c1;
        int     i, k1, k2, k3, kx;
        kx = k4 >> 1;
        k1 = k4;
        k2 = k4 << 1;
        k3 = k2 + k1;
        k4 = k2 << 1;
        fi = fz;
        gi = fi + kx;
        do {
            FLOAT   f0, f1, f2, f3;
            f1 = fi[0] - fi[k1];
            f0 = fi[0] + fi[k1];
            f3 = fi[k2] - fi[k3];
            f2 = fi[k2] + fi[k3];
            fi[k2] = f0 - f2;
            fi[0] = f0 + f2;
            fi[k3] = f1 - f3;
            fi[k1] = f1 + f3;
            f1 = gi[0] - gi[k1];
            f0 = gi[0] + gi[k1];
            f3 = SQRT2 * gi[k3];
            f2 = SQRT2 * gi[k2];
            gi[k2] = f0 - f2;
            gi[0] = f0 + f2;
            gi[k3] = f1 - f3;
            gi[k1] = f1 + f3;
            gi += k4;
            fi += k4;
        } while(fi < fn);
        c1 = tri[0];
        s1 = tri[1];
        for(i = 1; i < kx; i++) {
            __m128 v_s2;
            __m128 v_c2;
            __m128 v_c1;
            __m128 v_s1;
            FLOAT   c2, s2, s1_2 = s1+s1;
            c2 = 1 - s1_2 * s1;
            s2 = s1_2 * c1;
            fi = fz + i;
            gi = fz + k1 - i;
            v_c1 = _mm_set_ps1(c1);
            v_s1 = _mm_set_ps1(s1);
            v_c2 = _mm_set_ps1(c2);
            v_s2 = _mm_set_ps1(s2);
            {
                static const vecfloat_union sign_mask = {{0x80000000,0,0,0}};
                v_c1 = _mm_xor_ps(sign_mask._m128, v_c1); /* v_c1 := {-c1, +c1, +c1, +c1} */
            }
            {
                static const vecfloat_union sign_mask = {{0,0x80000000,0,0}};
                v_s1 = _mm_xor_ps(sign_mask._m128, v_s1); /* v_s1 := {+s1, -s1, +s1, +s1} */
            }
            {
                static const vecfloat_union sign_mask = {{0,0,0x80000000,0x80000000}};
                v_c2 = _mm_xor_ps(sign_mask._m128, v_c2); /* v_c2 := {+c2, +c2, -c2, -c2} */
            }
            do {
                __m128 p, q, r;

                q = _mm_setr_ps(fi[k1], fi[k3], gi[k1], gi[k3]); /* Q := {fi_k1,fi_k3,gi_k1,gi_k3}*/
                p = _mm_mul_ps(_mm_set_ps1(s2), q);              /* P := s2 * Q */
                q = _mm_mul_ps(v_c2, q);                         /* Q := c2 * Q */
                q = _mm_shuffle_ps(q, q, _MM_SHUFFLE(1,0,3,2));  /* Q := {-c2*gi_k1,-c2*gi_k3,c2*fi_k1,c2*fi_k3} */
                p = _mm_add_ps(p, q);
                
                r = _mm_setr_ps(gi[0], gi[k2], fi[0], fi[k2]);   /* R := {gi_0,gi_k2,fi_0,fi_k2} */
                q = _mm_sub_ps(r, p);                            /* Q := {gi_0-p0,gi_k2-p1,fi_0-p2,fi_k2-p3} */
                r = _mm_add_ps(r, p);                            /* R := {gi_0+p0,gi_k2+p1,fi_0+p2,fi_k2+p3} */
                p = _mm_shuffle_ps(q, r, _MM_SHUFFLE(2,0,2,0));  /* P := {q0,q2,r0,r2} */
                p = _mm_shuffle_ps(p, p, _MM_SHUFFLE(3,1,2,0));  /* P := {q0,r0,q2,r2} */
                q = _mm_shuffle_ps(q, r, _MM_SHUFFLE(3,1,3,1));  /* Q := {q1,q3,r1,r3} */
                r = _mm_mul_ps(v_c1, q);
                q = _mm_mul_ps(v_s1, q);
                q = _mm_shuffle_ps(q, q, _MM_SHUFFLE(0,1,2,3));  /* Q := {q3,q2,q1,q0} */
                q = _mm_add_ps(q, r);

                store4(_mm_sub_ps(p, q), &gi[k3], &gi[k2], &fi[k3], &fi[k2]);
                store4(_mm_add_ps(p, q), &gi[k1], &gi[ 0], &fi[k1], &fi[ 0]);

                gi += k4;
                fi += k4;
            } while(fi < fn);
            c2 = c1;
            c1 = c2 * tri[0] - s1 * tri[1];
            s1 = c2 * tri[1] + s1 * tri[0];
        }
        tri += 2;
    } while(k4 < n);
	}

#endif	/* HAVE_XMMINTRIN_H */



/*
 *  ReplayGainAnalysis - analyzes input samples and give the recommended dB change
 *  Copyright (C) 2001 David Robinson and Glen Sawyer
 *  Improvements and optimizations added by Frank Klemm, and by Marcel Muller 
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
 *  original coding by Glen Sawyer (mp3gain@hotmail.com)
 *    -- blame him if you think this runs too slowly, or the coding is otherwise flawed
 *
 *  lots of code improvements by Frank Klemm ( http://www.uni-jena.de/~pfk/mpp/ )
 *    -- credit him for all the _good_ programming ;)
 *
 *
 *  For an explanation of the concepts and the basic algorithms involved, go to:
 *    http://www.replaygain.org/
 */

/*
 *  Here's the deal. Call
 *
 *    InitGainAnalysis ( long samplefreq );
 *
 *  to initialize everything. Call
 *
 *    AnalyzeSamples ( const Float_t*  left_samples,
 *                     const Float_t*  right_samples,
 *                     size_t          num_samples,
 *                     int             num_channels );
 *
 *  as many times as you want, with as many or as few samples as you want.
 *  If mono, pass the sample buffer in through left_samples, leave
 *  right_samples NULL, and make sure num_channels = 1.
 *
 *    GetTitleGain()
 *
 *  will return the recommended dB level change for all samples analyzed
 *  SINCE THE LAST TIME you called GetTitleGain() OR InitGainAnalysis().
 *
 *    GetAlbumGain()
 *
 *  will return the recommended dB level change for all samples analyzed
 *  since InitGainAnalysis() was called and finalized with GetTitleGain().
 *
 *  Pseudo-code to process an album:
 *
 *    Float_t       l_samples [4096];
 *    Float_t       r_samples [4096];
 *    size_t        num_samples;
 *    unsigned int  num_songs;
 *    unsigned int  i;
 *
 *    InitGainAnalysis ( 44100 );
 *    for( i = 1; i <= num_songs; i++ ) {
 *        while( ( num_samples = getSongSamples ( song[i], left_samples, right_samples ) ) > 0 )
 *            AnalyzeSamples ( left_samples, right_samples, num_samples, 2 );
 *        fprintf ("Recommended dB change for song %2d: %+6.2f dB\n", i, GetTitleGain() );
 *    }
 *    fprintf ("Recommended dB change for whole album: %+6.2f dB\n", GetAlbumGain() );
 */

/*
 *  So here's the main source of potential code confusion:
 *
 *  The filters applied to the incoming samples are IIR filters,
 *  meaning they rely on up to <filter order> number of previous samples
 *  AND up to <filter order> number of previous filtered samples.
 *
 *  I set up the AnalyzeSamples routine to minimize memory usage and interface
 *  complexity. The speed isn't compromised too much (I don't think), but the
 *  internal complexity is higher than it should be for such a relatively
 *  simple routine.
 *
 *  Optimization/clarity suggestions are welcome.
 */


/* for each filter: */
/* [0] 48 kHz, [1] 44.1 kHz, [2] 32 kHz, [3] 24 kHz, [4] 22050 Hz, [5] 16 kHz, [6] 12 kHz, [7] is 11025 Hz, [8] 8 kHz */

#ifdef WIN32
#pragma warning ( disable : 4305 )
#endif


/*lint -save -e736 loss of precision */
static const Float_t ABYule[9][multiple_of(4, 2 * YULE_ORDER + 1)] = {
    /* 20                 18                 16                 14                 12                 10                 8                  6                  4                  2                 0                 19                 17                 15                 13                 11                 9                  7                  5                  3                  1              */
    { 0.00288463683916,  0.00012025322027,  0.00306428023191,  0.00594298065125, -0.02074045215285,  0.02161526843274, -0.01655260341619, -0.00009291677959, -0.00123395316851, -0.02160367184185, 0.03857599435200, 0.13919314567432, -0.86984376593551,  2.75465861874613, -5.87257861775999,  9.48293806319790,-12.28759895145294, 13.05504219327545,-11.34170355132042,  7.81501653005538, -3.84664617118067},
    {-0.00187763777362,  0.00674613682247, -0.00240879051584,  0.01624864962975, -0.02596338512915,  0.02245293253339, -0.00834990904936, -0.00851165645469, -0.00848709379851, -0.02911007808948, 0.05418656406430, 0.13149317958808, -0.75104302451432,  2.19611684890774, -4.39470996079559,  6.85401540936998, -8.81498681370155,  9.47693607801280, -8.54751527471874,  6.36317777566148, -3.47845948550071},
    {-0.00881362733839,  0.00651420667831, -0.01390589421898,  0.03174092540049,  0.00222312597743,  0.04781476674921, -0.05588393329856,  0.02163541888798, -0.06247880153653, -0.09331049056315, 0.15457299681924, 0.02347897407020, -0.05032077717131,  0.16378164858596, -0.45953458054983,  1.00595954808547, -1.67148153367602,  2.23697657451713, -2.64577170229825,  2.84868151156327, -2.37898834973084},
    {-0.02950134983287,  0.00205861885564, -0.00000828086748,  0.06276101321749, -0.00584456039913, -0.02364141202522, -0.00915702933434,  0.03282930172664, -0.08587323730772, -0.22613988682123, 0.30296907319327, 0.00302439095741,  0.02005851806501,  0.04500235387352, -0.22138138954925,  0.39120800788284, -0.22638893773906, -0.16276719120440, -0.25656257754070,  1.07977492259970, -1.61273165137247},
    {-0.01760176568150, -0.01635381384540,  0.00832043980773,  0.05724228140351, -0.00589500224440, -0.00469977914380, -0.07834489609479,  0.11921148675203, -0.11828570177555, -0.25572241425570, 0.33642304856132, 0.02977207319925, -0.04237348025746,  0.08333755284107, -0.04067510197014, -0.12453458140019,  0.47854794562326, -0.80774944671438,  0.12205022308084,  0.87350271418188, -1.49858979367799},
    { 0.00541907748707, -0.03193428438915, -0.01863887810927,  0.10478503600251,  0.04097565135648, -0.12398163381748,  0.04078262797139, -0.01419140100551, -0.22784394429749, -0.14351757464547, 0.44915256608450, 0.03222754072173,  0.05784820375801,  0.06747620744683,  0.00613424350682,  0.22199650564824, -0.42029820170918,  0.00213767857124, -0.37256372942400,  0.29661783706366, -0.62820619233671},
    {-0.00588215443421, -0.03788984554840,  0.08647503780351,  0.00647310677246, -0.27562961986224,  0.30931782841830, -0.18901604199609,  0.16744243493672,  0.16242137742230, -0.75464456939302, 0.56619470757641, 0.01807364323573,  0.01639907836189, -0.04784254229033,  0.06739368333110, -0.33032403314006,  0.45054734505008,  0.00819999645858, -0.26806001042947,  0.29156311971249, -1.04800335126349},
    {-0.00749618797172, -0.03721611395801,  0.06920467763959,  0.01628462406333, -0.25344790059353,  0.15558449135573,  0.02377945217615,  0.17520704835522, -0.14289799034253, -0.53174909058578, 0.58100494960553, 0.01818801111503,  0.02442357316099, -0.02505961724053, -0.05246019024463, -0.23313271880868,  0.38952639978999,  0.14728154134330, -0.20256413484477, -0.31863563325245, -0.51035327095184},
    {-0.02217936801134,  0.04788665548180, -0.04060034127000, -0.11202315195388, -0.02459864859345,  0.14590772289388, -0.10214864179676,  0.04267842219415, -0.00275953611929, -0.42163034350696, 0.53648789255105, 0.04704409688120,  0.05477720428674, -0.18823009262115, -0.17556493366449,  0.15113130533216,  0.26408300200955, -0.04678328784242, -0.03424681017675, -0.43193942311114, -0.25049871956020}
};

static const Float_t ABButter[9][multiple_of(4, 2 * BUTTER_ORDER + 1)] = {
    /* 5                4                  3                  2                 1              */
    {0.98621192462708, 0.97261396931306, -1.97242384925416, -1.97223372919527, 0.98621192462708},
    {0.98500175787242, 0.97022847566350, -1.97000351574484, -1.96977855582618, 0.98500175787242},
    {0.97938932735214, 0.95920349965459, -1.95877865470428, -1.95835380975398, 0.97938932735214},
    {0.97531843204928, 0.95124613669835, -1.95063686409857, -1.95002759149878, 0.97531843204928},
    {0.97316523498161, 0.94705070426118, -1.94633046996323, -1.94561023566527, 0.97316523498161},
    {0.96454515552826, 0.93034775234268, -1.92909031105652, -1.92783286977036, 0.96454515552826},
    {0.96009142950541, 0.92177618768381, -1.92018285901082, -1.91858953033784, 0.96009142950541},
    {0.95856916599601, 0.91885558323625, -1.91713833199203, -1.91542108074780, 0.95856916599601},
    {0.94597685600279, 0.89487434461664, -1.89195371200558, -1.88903307939452, 0.94597685600279}
};

/*lint -restore */

#ifdef WIN32
#pragma warning ( default : 4305 )
#endif

/* When calling this procedure, make sure that ip[-order] and op[-order] point to real data! */
static void filterYule(const Float_t * input, Float_t * output, size_t nSamples, const Float_t * const kernel) {

    while(nSamples--) {
        Float_t y0 =  input[-10] * kernel[ 0];
        Float_t y2 =  input[ -9] * kernel[ 1];
        Float_t y4 =  input[ -8] * kernel[ 2];
        Float_t y6 =  input[ -7] * kernel[ 3];
        Float_t s00 = y0 + y2 + y4 + y6;
        Float_t y8 =  input[ -6] * kernel[ 4];
        Float_t yA =  input[ -5] * kernel[ 5];
        Float_t yC =  input[ -4] * kernel[ 6];
        Float_t yE =  input[ -3] * kernel[ 7];
        Float_t s01 = y8 + yA + yC + yE;
        Float_t yG =  input[ -2] * kernel[ 8] + input[ -1] * kernel[ 9];
        Float_t yK =  input[  0] * kernel[10];

        Float_t s1 = s00 + s01 + yG + yK;

        Float_t x1 = output[-10] * kernel[11] + output[ -9] * kernel[12];
        Float_t x5 = output[ -8] * kernel[13] + output[ -7] * kernel[14];
        Float_t x9 = output[ -6] * kernel[15] + output[ -5] * kernel[16];
        Float_t xD = output[ -4] * kernel[17] + output[ -3] * kernel[18];
        Float_t xH = output[ -2] * kernel[19] + output[ -1] * kernel[20];

        Float_t s2 = x1 + x5 + x9 + xD + xH;

        output[0] = (Float_t)(s1 - s2);

        ++output;
        ++input;
    }
	}

static void filterButter(const Float_t * input, Float_t * output, size_t nSamples, const Float_t * const kernel) {

    while(nSamples--) {
        Float_t s1 =  input[-2] * kernel[0] +  input[-1] * kernel[2] +  input[ 0] * kernel[4];
        Float_t s2 = output[-2] * kernel[1] + output[-1] * kernel[3];
        output[0] = (Float_t)(s1 - s2);
        ++output;
        ++input;
    }
	}



static int ResetSampleFrequency(replaygain_t * rgData, long samplefreq);

/* returns a INIT_GAIN_ANALYSIS_OK if successful, INIT_GAIN_ANALYSIS_ERROR if not */

int ResetSampleFrequency(replaygain_t * rgData, long samplefreq) {

    /* zero out initial values, only first MAX_ORDER values */
    memset(rgData->linprebuf, 0, MAX_ORDER * sizeof(*rgData->linprebuf));
    memset(rgData->rinprebuf, 0, MAX_ORDER * sizeof(*rgData->rinprebuf));
    memset(rgData->lstepbuf,  0, MAX_ORDER * sizeof(*rgData->lstepbuf));
    memset(rgData->rstepbuf,  0, MAX_ORDER * sizeof(*rgData->rstepbuf));
    memset(rgData->loutbuf,   0, MAX_ORDER * sizeof(*rgData->loutbuf));
    memset(rgData->routbuf,   0, MAX_ORDER * sizeof(*rgData->routbuf));

  switch((int) (samplefreq)) {
    case 48000:
        rgData->freqindex = 0;
        break;
    case 44100:
        rgData->freqindex = 1;
        break;
    case 32000:
        rgData->freqindex = 2;
        break;
    case 24000:
        rgData->freqindex = 3;
        break;
    case 22050:
        rgData->freqindex = 4;
        break;
    case 16000:
        rgData->freqindex = 5;
        break;
    case 12000:
        rgData->freqindex = 6;
        break;
    case 11025:
        rgData->freqindex = 7;
        break;
    case 8000:
        rgData->freqindex = 8;
        break;
    default:
        return INIT_GAIN_ANALYSIS_ERROR;
    }

    rgData->sampleWindow =
        (samplefreq * RMS_WINDOW_TIME_NUMERATOR + RMS_WINDOW_TIME_DENOMINATOR -
         1) / RMS_WINDOW_TIME_DENOMINATOR;

    rgData->lsum = 0.;
    rgData->rsum = 0.;
    rgData->totsamp = 0;

    memset(rgData->A, 0, sizeof(rgData->A));

  return INIT_GAIN_ANALYSIS_OK;
	}

int InitGainAnalysis(replaygain_t * rgData, long samplefreq) {

    if(ResetSampleFrequency(rgData, samplefreq) != INIT_GAIN_ANALYSIS_OK) {
        return INIT_GAIN_ANALYSIS_ERROR;
    }

    rgData->linpre = rgData->linprebuf + MAX_ORDER;
    rgData->rinpre = rgData->rinprebuf + MAX_ORDER;
    rgData->lstep = rgData->lstepbuf + MAX_ORDER;
    rgData->rstep = rgData->rstepbuf + MAX_ORDER;
    rgData->lout = rgData->loutbuf + MAX_ORDER;
    rgData->rout = rgData->routbuf + MAX_ORDER;

    memset(rgData->B, 0, sizeof(rgData->B));

    return INIT_GAIN_ANALYSIS_OK;
	}

/* returns GAIN_ANALYSIS_OK if successful, GAIN_ANALYSIS_ERROR if not */
int AnalyzeSamples(replaygain_t * rgData, const Float_t * left_samples, const Float_t * right_samples,
               size_t num_samples, int num_channels) {
    const Float_t *curleft;
    const Float_t *curright;
    long    batchsamples;
    long    cursamples;
    long    cursamplepos;
    int     i;
    Float_t sum_l, sum_r;

    if(num_samples == 0)
        return GAIN_ANALYSIS_OK;

    cursamplepos = 0;
    batchsamples = (long) num_samples;

    switch(num_channels) {
    case 1:
        right_samples = left_samples;
        break;
    case 2:
        break;
    default:
        return GAIN_ANALYSIS_ERROR;
			}

    if(num_samples < MAX_ORDER) {
        memcpy(rgData->linprebuf + MAX_ORDER, left_samples, num_samples * sizeof(Float_t));
        memcpy(rgData->rinprebuf + MAX_ORDER, right_samples, num_samples * sizeof(Float_t));
			}
    else {
        memcpy(rgData->linprebuf + MAX_ORDER, left_samples, MAX_ORDER * sizeof(Float_t));
        memcpy(rgData->rinprebuf + MAX_ORDER, right_samples, MAX_ORDER * sizeof(Float_t));
			}

    while(batchsamples > 0) {
        cursamples = batchsamples > rgData->sampleWindow - rgData->totsamp ?
            rgData->sampleWindow - rgData->totsamp : batchsamples;
        if(cursamplepos < MAX_ORDER) {
            curleft = rgData->linpre + cursamplepos;
            curright = rgData->rinpre + cursamplepos;
            if(cursamples > MAX_ORDER - cursamplepos)
                cursamples = MAX_ORDER - cursamplepos;
				  }
        else {
            curleft = left_samples + cursamplepos;
            curright = right_samples + cursamplepos;
					}

        YULE_FILTER(curleft, rgData->lstep + rgData->totsamp, cursamples,
                    ABYule[rgData->freqindex]);
        YULE_FILTER(curright, rgData->rstep + rgData->totsamp, cursamples,
                    ABYule[rgData->freqindex]);

        BUTTER_FILTER(rgData->lstep + rgData->totsamp, rgData->lout + rgData->totsamp, cursamples,
                      ABButter[rgData->freqindex]);
        BUTTER_FILTER(rgData->rstep + rgData->totsamp, rgData->rout + rgData->totsamp, cursamples,
                      ABButter[rgData->freqindex]);

        curleft = rgData->lout + rgData->totsamp; /* Get the squared values */
        curright = rgData->rout + rgData->totsamp;

        sum_l = 0;
        sum_r = 0;
        i = cursamples & 0x03;
        while(i--) {
            Float_t const l = *curleft++;
            Float_t const r = *curright++;
            sum_l += l * l;
            sum_r += r * r;
					}
        i = cursamples / 4;
        while(i--) {
            Float_t l0 = curleft[0] * curleft[0];
            Float_t l1 = curleft[1] * curleft[1];
            Float_t l2 = curleft[2] * curleft[2];
            Float_t l3 = curleft[3] * curleft[3];
            Float_t sl = l0 + l1 + l2 + l3;
            Float_t r0 = curright[0] * curright[0];
            Float_t r1 = curright[1] * curright[1];
            Float_t r2 = curright[2] * curright[2];
            Float_t r3 = curright[3] * curright[3];
            Float_t sr = r0 + r1 + r2 + r3;
            sum_l += sl;
            curleft += 4;
            sum_r += sr;
            curright += 4;
					}
        rgData->lsum += sum_l;
        rgData->rsum += sum_r;

        batchsamples -= cursamples;
        cursamplepos += cursamples;
        rgData->totsamp += cursamples;
        if(rgData->totsamp == rgData->sampleWindow) { /* Get the Root Mean Square (RMS) for this set of samples */
            double const val =
                STEPS_per_dB * 10. * log10((rgData->lsum + rgData->rsum) / rgData->totsamp * 0.5 +
                                           1.e-37);
            size_t  ival = (val <= 0) ? 0 : (size_t) val;
            if(ival >= sizeof(rgData->A) / sizeof(*(rgData->A)))
                ival = sizeof(rgData->A) / sizeof(*(rgData->A)) - 1;
            rgData->A[ival]++;
            rgData->lsum = rgData->rsum = 0.;
            memmove(rgData->loutbuf, rgData->loutbuf + rgData->totsamp,
                    MAX_ORDER * sizeof(Float_t));
            memmove(rgData->routbuf, rgData->routbuf + rgData->totsamp,
                    MAX_ORDER * sizeof(Float_t));
            memmove(rgData->lstepbuf, rgData->lstepbuf + rgData->totsamp,
                    MAX_ORDER * sizeof(Float_t));
            memmove(rgData->rstepbuf, rgData->rstepbuf + rgData->totsamp,
                    MAX_ORDER * sizeof(Float_t));
            rgData->totsamp = 0;
				  }
        if(rgData->totsamp > rgData->sampleWindow) /* somehow I really screwed up: Error in programming! Contact author about totsamp > sampleWindow */
            return GAIN_ANALYSIS_ERROR;
			}
    if(num_samples < MAX_ORDER) {
        memmove(rgData->linprebuf, rgData->linprebuf + num_samples,
                (MAX_ORDER - num_samples) * sizeof(Float_t));
        memmove(rgData->rinprebuf, rgData->rinprebuf + num_samples,
                (MAX_ORDER - num_samples) * sizeof(Float_t));
        memcpy(rgData->linprebuf + MAX_ORDER - num_samples, left_samples,
               num_samples * sizeof(Float_t));
        memcpy(rgData->rinprebuf + MAX_ORDER - num_samples, right_samples,
               num_samples * sizeof(Float_t));
			}
    else {
        memcpy(rgData->linprebuf, left_samples + num_samples - MAX_ORDER,
               MAX_ORDER * sizeof(Float_t));
        memcpy(rgData->rinprebuf, right_samples + num_samples - MAX_ORDER,
               MAX_ORDER * sizeof(Float_t));
			}

  return GAIN_ANALYSIS_OK;
	}


static  Float_t analyzeResult(uint32_t const *Array, size_t len) {
    uint32_t elems;
    uint32_t upper;
    uint32_t sum;
    size_t  i;

    elems = 0;
    for(i = 0; i < len; i++)
        elems += Array[i];
    if(elems == 0)
        return GAIN_NOT_ENOUGH_SAMPLES;

    upper = (uint32_t) ceil(elems * (1. - RMS_PERCENTILE));
    sum = 0;
    for(i = len; i-- > 0;) {
        sum += Array[i];
        if(sum >= upper) {
            break;
        }
    }

  return (Float_t) ((Float_t) PINK_REF - (Float_t) i / (Float_t) STEPS_per_dB);
	}


Float_t GetTitleGain(replaygain_t * rgData) {
    Float_t retval;
    unsigned int i;

    retval = analyzeResult(rgData->A, sizeof(rgData->A) / sizeof(*(rgData->A)));

    for(i = 0; i < sizeof(rgData->A) / sizeof(*(rgData->A)); i++) {
        rgData->B[i] += rgData->A[i];
        rgData->A[i] = 0;
    }

    for(i = 0; i < MAX_ORDER; i++)
        rgData->linprebuf[i] = rgData->lstepbuf[i]
            = rgData->loutbuf[i]
            = rgData->rinprebuf[i]
            = rgData->rstepbuf[i]
            = rgData->routbuf[i] = 0.f;

    rgData->totsamp = 0;
    rgData->lsum = rgData->rsum = 0.;
    return retval;
	}

#if 0
static Float_t GetAlbumGain(replaygain_t const* rgData);

Float_t GetAlbumGain(replaygain_t const* rgData) {

    return analyzeResult(rgData->B, sizeof(rgData->B) / sizeof(*(rgData->B)));
}
#endif

/* end of gain_analysis.c */




/* -*- mode: C; mode: fold -*- */
/*
 *      LAME MP3 encoding engine
 *
 *      Copyright (c) 1999-2000 Mark Taylor
 *      Copyright (c) 2000-2005 Takehiro Tominaga
 *      Copyright (c) 2000-2017 Robert Hegemann
 *      Copyright (c) 2000-2005 Gabriel Bouvigne
 *      Copyright (c) 2000-2004 Alexander Leidinger
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

/* $Id: lame.c,v 1.377 2017/09/26 12:14:02 robert Exp $ */


#define LAME_DEFAULT_QUALITY 3


int is_lame_global_flags_valid(const lame_global_flags *gfp) {

  if(!gfp)
    return 0;
  if(gfp->class_id != LAME_ID)
    return 0;
  return 1;
	}


int is_lame_internal_flags_valid(const lame_internal_flags *gfc) {

  if(!gfc)
    return 0;
  if(gfc->class_id != LAME_ID)
    return 0;
  if(gfc->lame_init_params_successful <=0)
    return 0;
  return 1;
	}



static FLOAT filter_coef(FLOAT x) {

  if(x > 1.0)
      return 0.0;
  if(x <= 0.0)
      return 1.0;

  return cos(PI / 2 * x);
	}

static void lame_init_params_ppflt(lame_internal_flags * gfc) {
    SessionConfig_t *const cfg = &gfc->cfg;
    
    /***************************************************************/
    /* compute info needed for polyphase filter (filter type==0, default) */
    /***************************************************************/

    int     band, maxband, minband;
    FLOAT   freq;
    int     lowpass_band = 32;
    int     highpass_band = -1;

    if(cfg->lowpass1 > 0) {
        minband = 999;
        for(band = 0; band <= 31; band++) {
            freq = band / 31.0;
            /* this band and above will be zeroed: */
            if(freq >= cfg->lowpass2) {
                lowpass_band = Min(lowpass_band, band);
            }
            if(cfg->lowpass1 < freq && freq < cfg->lowpass2) {
                minband = Min(minband, band);
            }
        }

        /* compute the *actual* transition band implemented by
         * the polyphase filter */
        if(minband == 999) {
            cfg->lowpass1 = (lowpass_band - .75) / 31.0;
        }
        else {
            cfg->lowpass1 = (minband - .75) / 31.0;
        }
        cfg->lowpass2 = lowpass_band / 31.0;
    }

    /* make sure highpass filter is within 90% of what the effective
     * highpass frequency will be */
    if(cfg->highpass2 > 0) {
        if(cfg->highpass2 < .9 * (.75 / 31.0)) {
            cfg->highpass1 = 0;
            cfg->highpass2 = 0;
            MSGF(gfc, "Warning: highpass filter disabled.  " "highpass frequency too small\n");
        }
    }

    if(cfg->highpass2 > 0) {
        maxband = -1;
        for(band = 0; band <= 31; band++) {
            freq = band / 31.0;
            /* this band and below will be zereod */
            if(freq <= cfg->highpass1) {
                highpass_band = Max(highpass_band, band);
            }
            if(cfg->highpass1 < freq && freq < cfg->highpass2) {
                maxband = Max(maxband, band);
            }
        }
        /* compute the *actual* transition band implemented by
         * the polyphase filter */
        cfg->highpass1 = highpass_band / 31.0;
        if(maxband == -1) {
            cfg->highpass2 = (highpass_band + .75) / 31.0;
        }
        else {
            cfg->highpass2 = (maxband + .75) / 31.0;
        }
    }

    for(band = 0; band < 32; band++) {
        FLOAT fc1, fc2;
        freq = band / 31.0f;
        if(cfg->highpass2 > cfg->highpass1) {
            fc1 = filter_coef((cfg->highpass2 - freq) / (cfg->highpass2 - cfg->highpass1 + 1e-20));
        }
        else {
            fc1 = 1.0f;
        }
        if(cfg->lowpass2 > cfg->lowpass1) {
            fc2 = filter_coef((freq - cfg->lowpass1)  / (cfg->lowpass2 - cfg->lowpass1 + 1e-20));
        }
        else {
            fc2 = 1.0f;
        }
        gfc->sv_enc.amp_filter[band] = fc1 * fc2;
    }
	}


static void optimum_bandwidth(double *const lowerlimit, double *const upperlimit, const unsigned bitrate) {
/*
 *  Input:
 *      bitrate     total bitrate in kbps
 *
 *   Output:
 *      lowerlimit: best lowpass frequency limit for input filter in Hz
 *      upperlimit: best highpass frequency limit for input filter in Hz
 */
    int     table_index;

    typedef struct {
        int     bitrate;     /* only indicative value */
        int     lowpass;
    } band_pass_t;

    const band_pass_t freq_map[] = {
        {8, 2000},
        {16, 3700},
        {24, 3900},
        {32, 5500},
        {40, 7000},
        {48, 7500},
        {56, 10000},
        {64, 11000},
        {80, 13500},
        {96, 15100},
        {112, 15600},
        {128, 17000},
        {160, 17500},
        {192, 18600},
        {224, 19400},
        {256, 19700},
        {320, 20500}
    };


    table_index = nearestBitrateFullIndex(bitrate);

    freq_map[table_index].bitrate;
    *lowerlimit = freq_map[table_index].lowpass;


/*
 *  Now we try to choose a good high pass filtering frequency.
 *  This value is currently not used.
 *    For fu < 16 kHz:  sqrt(fu*fl) = 560 Hz
 *    For fu = 18 kHz:  no high pass filtering
 *  This gives:
 *
 *   2 kHz => 160 Hz
 *   3 kHz => 107 Hz
 *   4 kHz =>  80 Hz
 *   8 kHz =>  40 Hz
 *  16 kHz =>  20 Hz
 *  17 kHz =>  10 Hz
 *  18 kHz =>   0 Hz
 *
 *  These are ad hoc values and these can be optimized if a high pass is available.
 */
/*    if(f_low <= 16000)
        f_high = 16000. * 20. / f_low;
    else if(f_low <= 18000)
        f_high = 180. - 0.01 * f_low;
    else
        f_high = 0.;*/

    /*
     *  When we sometimes have a good highpass filter, we can add the highpass
     *  frequency to the lowpass frequency
     */

  /*if(upperlimit != NULL)
   *upperlimit = f_high;*/
  upperlimit;
	}


static int optimum_samplefreq(int lowpassfreq, int input_samplefreq) {
/*
 * Rules:
 *  - if possible, sfb21 should NOT be used
 *
 */
    int     suggested_samplefreq = 44100;

    if(input_samplefreq >= 48000)
        suggested_samplefreq = 48000;
    else if(input_samplefreq >= 44100)
        suggested_samplefreq = 44100;
    else if(input_samplefreq >= 32000)
        suggested_samplefreq = 32000;
    else if(input_samplefreq >= 24000)
        suggested_samplefreq = 24000;
    else if(input_samplefreq >= 22050)
        suggested_samplefreq = 22050;
    else if(input_samplefreq >= 16000)
        suggested_samplefreq = 16000;
    else if(input_samplefreq >= 12000)
        suggested_samplefreq = 12000;
    else if(input_samplefreq >= 11025)
        suggested_samplefreq = 11025;
    else if(input_samplefreq >= 8000)
        suggested_samplefreq = 8000;

    if(lowpassfreq == -1)
        return suggested_samplefreq;

    if(lowpassfreq <= 15960)
        suggested_samplefreq = 44100;
    if(lowpassfreq <= 15250)
        suggested_samplefreq = 32000;
    if(lowpassfreq <= 11220)
        suggested_samplefreq = 24000;
    if(lowpassfreq <= 9970)
        suggested_samplefreq = 22050;
    if(lowpassfreq <= 7230)
        suggested_samplefreq = 16000;
    if(lowpassfreq <= 5420)
        suggested_samplefreq = 12000;
    if(lowpassfreq <= 4510)
        suggested_samplefreq = 11025;
    if(lowpassfreq <= 3970)
        suggested_samplefreq = 8000;

    if(input_samplefreq < suggested_samplefreq) {
        /* choose a valid MPEG sample frequency above the input sample frequency
           to avoid SFB21/12 bitrate bloat
           rh 061115
         */
        if(input_samplefreq > 44100) {
            return 48000;
        }
        if(input_samplefreq > 32000) {
            return 44100;
        }
        if(input_samplefreq > 24000) {
            return 32000;
        }
        if(input_samplefreq > 22050) {
            return 24000;
        }
        if(input_samplefreq > 16000) {
            return 22050;
        }
        if(input_samplefreq > 12000) {
            return 16000;
        }
        if(input_samplefreq > 11025) {
            return 12000;
        }
        if(input_samplefreq > 8000) {
            return 11025;
        }
        return 8000;
    }

  return suggested_samplefreq;
	}





/* set internal feature flags.  USER should not access these since
 * some combinations will produce strange results */
static void lame_init_qval(lame_global_flags *gfp) {
  lame_internal_flags *const gfc = gfp->internal_flags;
  SessionConfig_t *const cfg = &gfc->cfg;

  switch(gfp->quality) {
		default:
    case 9:            /* no psymodel, no noise shaping */
        cfg->noise_shaping = 0;
        cfg->noise_shaping_amp = 0;
        cfg->noise_shaping_stop = 0;
        cfg->use_best_huffman = 0;
        cfg->full_outer_loop = 0;
        break;

    case 8:
        gfp->quality = 7;
        /*lint --fallthrough */
    case 7:            /* use psymodel (for short block and m/s switching), but no noise shapping */
        cfg->noise_shaping = 0;
        cfg->noise_shaping_amp = 0;
        cfg->noise_shaping_stop = 0;
        cfg->use_best_huffman = 0;
        cfg->full_outer_loop = 0;
        if(cfg->vbr == vbr_mt || cfg->vbr == vbr_mtrh) {
            cfg->full_outer_loop  = -1;
        }
        break;

    case 6:
        if(cfg->noise_shaping == 0)
            cfg->noise_shaping = 1;
        cfg->noise_shaping_amp = 0;
        cfg->noise_shaping_stop = 0;
        if(cfg->subblock_gain == -1)
            cfg->subblock_gain = 1;
        cfg->use_best_huffman = 0;
        cfg->full_outer_loop = 0;
        break;

    case 5:
        if(cfg->noise_shaping == 0)
            cfg->noise_shaping = 1;
        cfg->noise_shaping_amp = 0;
        cfg->noise_shaping_stop = 0;
        if(cfg->subblock_gain == -1)
            cfg->subblock_gain = 1;
        cfg->use_best_huffman = 0;
        cfg->full_outer_loop = 0;
        break;

    case 4:
        if(cfg->noise_shaping == 0)
            cfg->noise_shaping = 1;
        cfg->noise_shaping_amp = 0;
        cfg->noise_shaping_stop = 0;
        if(cfg->subblock_gain == -1)
            cfg->subblock_gain = 1;
        cfg->use_best_huffman = 1;
        cfg->full_outer_loop = 0;
        break;

    case 3:
        if(cfg->noise_shaping == 0)
            cfg->noise_shaping = 1;
        cfg->noise_shaping_amp = 1;
        cfg->noise_shaping_stop = 1;
        if(cfg->subblock_gain == -1)
            cfg->subblock_gain = 1;
        cfg->use_best_huffman = 1;
        cfg->full_outer_loop = 0;
        break;

    case 2:
        if(cfg->noise_shaping == 0)
            cfg->noise_shaping = 1;
        if(gfc->sv_qnt.substep_shaping == 0)
            gfc->sv_qnt.substep_shaping = 2;
        cfg->noise_shaping_amp = 1;
        cfg->noise_shaping_stop = 1;
        if(cfg->subblock_gain == -1)
            cfg->subblock_gain = 1;
        cfg->use_best_huffman = 1; /* inner loop */
        cfg->full_outer_loop = 0;
        break;

    case 1:
        if(cfg->noise_shaping == 0)
            cfg->noise_shaping = 1;
        if(gfc->sv_qnt.substep_shaping == 0)
            gfc->sv_qnt.substep_shaping = 2;
        cfg->noise_shaping_amp = 2;
        cfg->noise_shaping_stop = 1;
        if(cfg->subblock_gain == -1)
            cfg->subblock_gain = 1;
        cfg->use_best_huffman = 1;
        cfg->full_outer_loop = 0;
        break;

    case 0:
        if(cfg->noise_shaping == 0)
            cfg->noise_shaping = 1;
        if(gfc->sv_qnt.substep_shaping == 0)
            gfc->sv_qnt.substep_shaping = 2;
        cfg->noise_shaping_amp = 2;
        cfg->noise_shaping_stop = 1;
        if(cfg->subblock_gain == -1)
            cfg->subblock_gain = 1;
        cfg->use_best_huffman = 1; /*type 2 disabled because of it slowness,
                                      in favor of full outer loop search */
        cfg->full_outer_loop = 1;
        break;
    }

	}



static double linear_int(double a, double b, double m) {

  return a + m * (b - a);
	}



/********************************************************************
 *   initialize internal params based on data in gf
 *   (globalflags struct filled in by calling program)
 *
 *  OUTLINE:
 *
 * We first have some complex code to determine bitrate,
 * output samplerate and mode.  It is complicated by the fact
 * that we allow the user to set some or all of these parameters,
 * and need to determine best possible values for the rest of them:
 *
 *  1. set some CPU related flags
 *  2. check if we are mono->mono, stereo->mono or stereo->stereo
 *  3.  compute bitrate and output samplerate:
 *          user may have set compression ratio
 *          user may have set a bitrate
 *          user may have set a output samplerate
 *  4. set some options which depend on output samplerate
 *  5. compute the actual compression ratio
 *  6. set mode based on compression ratio
 *
 *  The remaining code is much simpler - it just sets options
 *  based on the mode & compression ratio:
 *
 *   set allow_diff_short based on mode
 *   select lowpass filter based on compression ratio & mode
 *   set the bitrate index, and min/max bitrates for VBR modes
 *   disable VBR tag if it is not appropriate
 *   initialize the bitstream
 *   initialize scalefac_band data
 *   set sideinfo_len (based on channels, CRC, out_samplerate)
 *   write an id3v2 tag into the bitstream
 *   write VBR tag into the bitstream
 *   set mpeg1/2 flag
 *   estimate the number of frames (based on a lot of data)
 *
 *   now we set more flags:
 *   nspsytune:
 *      see code
 *   VBR modes
 *      see code
 *   CBR/ABR
 *      see code
 *
 *  Finally, we set the algorithm flags based on the gfp->quality value
 *  lame_init_qval(gfp);
 *
 ********************************************************************/
int lame_init_params(lame_global_flags *gfp) {
    int     i;
    int     j;
    lame_internal_flags *gfc;
    SessionConfig_t *cfg;

    if(!is_lame_global_flags_valid(gfp)) 
        return -1;

    gfc = gfp->internal_flags;
    if(gfc == 0) 
        return -1;

    if(is_lame_internal_flags_valid(gfc))
        return -1; /* already initialized */

    /* start updating lame internal flags */
    gfc->class_id = LAME_ID;
    gfc->lame_init_params_successful = 0; /* will be set to one, when we get through until the end */

    if(gfp->samplerate_in < 1)
        return -1; /* input sample rate makes no sense */
    if(gfp->num_channels < 1 || 2 < gfp->num_channels)
        return -1; /* number of input channels makes no sense */
    if(gfp->samplerate_out != 0) {
      BYTE v=0;
      if(SmpFrqIndex(gfp->samplerate_out, &v) < 0)
        return -1; /* output sample rate makes no sense */
			}

    cfg = &gfc->cfg;

    cfg->enforce_min_bitrate = gfp->VBR_hard_min;
    cfg->analysis = gfp->analysis;
    if(cfg->analysis)
        gfp->write_lame_tag = 0;

    /* some file options not allowed if output is: not specified or stdout */
    if(gfc->pinfo != NULL)
        gfp->write_lame_tag = 0; /* disable Xing VBR tag */

    /* report functions */
    gfc->report_msg = gfp->report.msgf;
    gfc->report_dbg = gfp->report.debugf;
    gfc->report_err = gfp->report.errorf;

    if(gfp->asm_optimizations.amd3dnow)
        gfc->CPU_features.AMD_3DNow = has_3DNow();
    else
        gfc->CPU_features.AMD_3DNow = 0;

    if(gfp->asm_optimizations.mmx)
        gfc->CPU_features.MMX = has_MMX();
    else
        gfc->CPU_features.MMX = 0;

    if(gfp->asm_optimizations.sse) {
        gfc->CPU_features.SSE = has_SSE();
        gfc->CPU_features.SSE2 = has_SSE2();
    }
    else {
        gfc->CPU_features.SSE = 0;
        gfc->CPU_features.SSE2 = 0;
    }


    cfg->vbr = gfp->VBR;
    cfg->error_protection = gfp->error_protection;
    cfg->copyright = gfp->copyright;
    cfg->original = gfp->original;
    cfg->extension = gfp->extension;
    cfg->emphasis = gfp->emphasis;

    cfg->channels_in = gfp->num_channels;
    if(cfg->channels_in == 1)
        gfp->mode = MONO;
    cfg->channels_out = (gfp->mode == MONO) ? 1 : 2;
    if(gfp->mode != JOINT_STEREO)
        gfp->force_ms = 0; /* forced mid/side stereo for j-stereo only */
    cfg->force_ms = gfp->force_ms;

    if(cfg->vbr == vbr_off && gfp->VBR_mean_bitrate_kbps != 128 && gfp->brate == 0)
        gfp->brate = gfp->VBR_mean_bitrate_kbps;

  switch(cfg->vbr) {
    case vbr_off:
    case vbr_mtrh:
    case vbr_mt:
        /* these modes can handle free format condition */
        break;
    default:
        gfp->free_format = 0; /* mode can't be mixed with free format */
        break;
    }

    cfg->free_format = gfp->free_format;

    if(cfg->vbr == vbr_off && gfp->brate == 0) {
        /* no bitrate or compression ratio specified, use 11.025 */
        if(EQ(gfp->compression_ratio, 0))
            gfp->compression_ratio = 11.025; /* rate to compress a CD down to exactly 128000 bps */
    }

    /* find bitrate if user specify a compression ratio */
    if(cfg->vbr == vbr_off && gfp->compression_ratio > 0) {

        if(gfp->samplerate_out == 0)
            gfp->samplerate_out = map2MP3Frequency((int) (0.97 * gfp->samplerate_in)); /* round up with a margin of 3% */

        /* choose a bitrate for the output samplerate which achieves
         * specified compression ratio
         */
        gfp->brate = gfp->samplerate_out * 16 * cfg->channels_out / (1.e3 * gfp->compression_ratio);

        /* we need the version for the bitrate table look up */
        cfg->samplerate_index = SmpFrqIndex(gfp->samplerate_out, &cfg->version);
        assert(cfg->samplerate_index >=0);

        if(!cfg->free_format) /* for non Free Format find the nearest allowed bitrate */
            gfp->brate = FindNearestBitrate(gfp->brate, cfg->version, gfp->samplerate_out);
			}
    if(gfp->samplerate_out) {
        if(gfp->samplerate_out < 16000) {
            gfp->VBR_mean_bitrate_kbps = Max(gfp->VBR_mean_bitrate_kbps, 8);
            gfp->VBR_mean_bitrate_kbps = Min(gfp->VBR_mean_bitrate_kbps, 64);
        }
        else if(gfp->samplerate_out < 32000) {
            gfp->VBR_mean_bitrate_kbps = Max(gfp->VBR_mean_bitrate_kbps, 8);
            gfp->VBR_mean_bitrate_kbps = Min(gfp->VBR_mean_bitrate_kbps, 160);
        }
        else {
            gfp->VBR_mean_bitrate_kbps = Max(gfp->VBR_mean_bitrate_kbps, 32);
            gfp->VBR_mean_bitrate_kbps = Min(gfp->VBR_mean_bitrate_kbps, 320);
        }
    }
    /* WORK IN PROGRESS */
    /* mapping VBR scale to internal VBR quality settings */
    if(gfp->samplerate_out == 0 && (cfg->vbr == vbr_mt || cfg->vbr == vbr_mtrh)) {
        float const qval = gfp->VBR_q + gfp->VBR_q_frac;
        struct q_map { int sr_a; float qa, qb, ta, tb; int lp; };
        struct q_map const m[9]
        = { {48000, 0.0,6.5,  0.0,6.5, 23700}
          , {44100, 0.0,6.5,  0.0,6.5, 21780}
          , {32000, 6.5,8.0,  5.2,6.5, 15800}
          , {24000, 8.0,8.5,  5.2,6.0, 11850}
          , {22050, 8.5,9.01, 5.2,6.5, 10892}
          , {16000, 9.01,9.4, 4.9,6.5,  7903}
          , {12000, 9.4,9.6,  4.5,6.0,  5928}
          , {11025, 9.6,9.9,  5.1,6.5,  5446}
          , { 8000, 9.9,10.,  4.9,6.5,  3952}
        };
        for(i = 2; i < 9; ++i) {
            if(gfp->samplerate_in == m[i].sr_a) {
                if(qval < m[i].qa) {
                    double d = qval / m[i].qa;
                    d = d * m[i].ta;
                    gfp->VBR_q = (int)d;
                    gfp->VBR_q_frac = d - gfp->VBR_q;
                }
            }
            if(gfp->samplerate_in >= m[i].sr_a) {
                if(m[i].qa <= qval && qval < m[i].qb) {
                    float const q_ = m[i].qb-m[i].qa;
                    float const t_ = m[i].tb-m[i].ta;
                    double d = m[i].ta + t_ * (qval-m[i].qa) / q_;
                    gfp->VBR_q = (int)d;
                    gfp->VBR_q_frac = d - gfp->VBR_q;
                    gfp->samplerate_out = m[i].sr_a;
                    if(gfp->lowpassfreq == 0) {
                        gfp->lowpassfreq = -1;
                    }
                    break;
                }
            }
        }
    }

    /****************************************************************/
    /* if a filter has not been enabled, see if we should add one: */
    /****************************************************************/
    if(gfp->lowpassfreq == 0) {
        double  lowpass = 16000;
        double  highpass;

        switch(cfg->vbr) {
        case vbr_off:{
                optimum_bandwidth(&lowpass, &highpass, gfp->brate);
                break;
            }
        case vbr_abr:{
                optimum_bandwidth(&lowpass, &highpass, gfp->VBR_mean_bitrate_kbps);
                break;
            }
        case vbr_rh:{
                int const x[11] = {
                    19500, 19000, 18600, 18000, 17500, 16000, 15600, 14900, 12500, 10000, 3950
                };
                if(0 <= gfp->VBR_q && gfp->VBR_q <= 9) {
                    double  a = x[gfp->VBR_q], b = x[gfp->VBR_q + 1], m = gfp->VBR_q_frac;
                    lowpass = linear_int(a, b, m);
                }
                else {
                    lowpass = 19500;
                }
                break;
            }
        case vbr_mtrh:
        case vbr_mt:{
                int const x[11] = {
                    24000, 19500, 18500, 18000, 17500, 17000, 16500, 15600, 15200, 7230, 3950
                };
                if(0 <= gfp->VBR_q && gfp->VBR_q <= 9) {
                    double  a = x[gfp->VBR_q], b = x[gfp->VBR_q + 1], m = gfp->VBR_q_frac;
                    lowpass = linear_int(a, b, m);
                }
                else {
                    lowpass = 21500;
                }
                break;
            }
        default:{
                int const x[11] = {
                    19500, 19000, 18500, 18000, 17500, 16500, 15500, 14500, 12500, 9500, 3950
                };
                if(0 <= gfp->VBR_q && gfp->VBR_q <= 9) {
                    double  a = x[gfp->VBR_q], b = x[gfp->VBR_q + 1], m = gfp->VBR_q_frac;
                    lowpass = linear_int(a, b, m);
                }
                else {
                    lowpass = 19500;
                }
            }
        }

        if(gfp->mode == MONO && (cfg->vbr == vbr_off || cfg->vbr == vbr_abr))
            lowpass *= 1.5;

        gfp->lowpassfreq = lowpass;
    }

    if(gfp->samplerate_out == 0) {
        if(2 * gfp->lowpassfreq > gfp->samplerate_in) {
            gfp->lowpassfreq = gfp->samplerate_in / 2;
        }
        gfp->samplerate_out = optimum_samplefreq((int) gfp->lowpassfreq, gfp->samplerate_in);
    }
    if(cfg->vbr == vbr_mt || cfg->vbr == vbr_mtrh) {
        gfp->lowpassfreq = Min(24000, gfp->lowpassfreq);
    }
    else {
        gfp->lowpassfreq = Min(20500, gfp->lowpassfreq);
    }
    gfp->lowpassfreq = Min(gfp->samplerate_out / 2, gfp->lowpassfreq);

    if(cfg->vbr == vbr_off) {
        gfp->compression_ratio = gfp->samplerate_out * 16 * cfg->channels_out / (1.e3 * gfp->brate);
    }
    if(cfg->vbr == vbr_abr) {
        gfp->compression_ratio =
            gfp->samplerate_out * 16 * cfg->channels_out / (1.e3 * gfp->VBR_mean_bitrate_kbps);
    }

    cfg->disable_reservoir = gfp->disable_reservoir;
    cfg->lowpassfreq = gfp->lowpassfreq;
    cfg->highpassfreq = gfp->highpassfreq;
    cfg->samplerate_in = gfp->samplerate_in;
    cfg->samplerate_out = gfp->samplerate_out;
    cfg->mode_gr = cfg->samplerate_out <= 24000 ? 1 : 2; /* Number of granules per frame */


    /*
     *  sample freq       bitrate     compression ratio
     *     [kHz]      [kbps/channel]   for 16 bit input
     *     44.1            56               12.6
     *     44.1            64               11.025
     *     44.1            80                8.82
     *     22.05           24               14.7
     *     22.05           32               11.025
     *     22.05           40                8.82
     *     16              16               16.0
     *     16              24               10.667
     *
     */
    /*
     *  For VBR, take a guess at the compression_ratio.
     *  For example:
     *
     *    VBR_q    compression     like
     *     -        4.4         320 kbps/44 kHz
     *   0...1      5.5         256 kbps/44 kHz
     *     2        7.3         192 kbps/44 kHz
     *     4        8.8         160 kbps/44 kHz
     *     6       11           128 kbps/44 kHz
     *     9       14.7          96 kbps
     *
     *  for lower bitrates, downsample with --resample
     */

    switch(cfg->vbr) {
    case vbr_mt:
    case vbr_rh:
    case vbr_mtrh:
        {
            /*numbers are a bit strange, but they determine the lowpass value */
            FLOAT const cmp[] = { 5.7, 6.5, 7.3, 8.2, 10, 11.9, 13, 14, 15, 16.5 };
            gfp->compression_ratio = cmp[gfp->VBR_q];
        }
        break;
    case vbr_abr:
        gfp->compression_ratio =
            cfg->samplerate_out * 16 * cfg->channels_out / (1.e3 * gfp->VBR_mean_bitrate_kbps);
        break;
    default:
        gfp->compression_ratio = cfg->samplerate_out * 16 * cfg->channels_out / (1.e3 * gfp->brate);
        break;
    }


    /* mode = -1 (not set by user) or
     * mode = MONO (because of only 1 input channel).
     * If mode has not been set, then select J-STEREO
     */
    if(gfp->mode == NOT_SET) {
        gfp->mode = JOINT_STEREO;
    }

    cfg->mode = gfp->mode;


    /* apply user driven high pass filter */
    if(cfg->highpassfreq > 0) {
        cfg->highpass1 = 2. * cfg->highpassfreq;

        if(gfp->highpasswidth >= 0)
            cfg->highpass2 = 2. * (cfg->highpassfreq + gfp->highpasswidth);
        else            /* 0% above on default */
            cfg->highpass2 = (1 + 0.00) * 2. * cfg->highpassfreq;

        cfg->highpass1 /= cfg->samplerate_out;
        cfg->highpass2 /= cfg->samplerate_out;
    }
    else {
        cfg->highpass1 = 0;
        cfg->highpass2 = 0;
    }
    /* apply user driven low pass filter */
    cfg->lowpass1 = 0;
    cfg->lowpass2 = 0;
    if(cfg->lowpassfreq > 0 && cfg->lowpassfreq < (cfg->samplerate_out / 2) ) {
        cfg->lowpass2 = 2. * cfg->lowpassfreq;
        if(gfp->lowpasswidth >= 0) {
            cfg->lowpass1 = 2. * (cfg->lowpassfreq - gfp->lowpasswidth);
            if(cfg->lowpass1 < 0) /* has to be >= 0 */
                cfg->lowpass1 = 0;
        }
        else {          /* 0% below on default */
            cfg->lowpass1 = (1 - 0.00) * 2. * cfg->lowpassfreq;
        }
        cfg->lowpass1 /= cfg->samplerate_out;
        cfg->lowpass2 /= cfg->samplerate_out;
    }




  /**********************************************************************/
    /* compute info needed for polyphase filter (filter type==0, default) */
  /**********************************************************************/
    lame_init_params_ppflt(gfc);


  /*******************************************************
   * samplerate and bitrate index
   *******************************************************/
    cfg->samplerate_index = SmpFrqIndex(cfg->samplerate_out, &cfg->version);
    assert(cfg->samplerate_index >= 0);

    if(cfg->vbr == vbr_off) {
        if(cfg->free_format) {
            gfc->ov_enc.bitrate_index = 0;
        }
        else {
            gfp->brate = FindNearestBitrate(gfp->brate, cfg->version, cfg->samplerate_out);
            gfc->ov_enc.bitrate_index = BitrateIndex(gfp->brate, cfg->version, cfg->samplerate_out);
            if(gfc->ov_enc.bitrate_index <= 0) {
                /* This never happens, because of preceding FindNearestBitrate!
                 * But, set a sane value, just in case
                 */
                assert(0);
                gfc->ov_enc.bitrate_index = 8;
            }
        }
    }
    else {
        gfc->ov_enc.bitrate_index = 1;
    }

    init_bit_stream_w(gfc);

    j = cfg->samplerate_index + (3 * cfg->version) + 6 * (cfg->samplerate_out < 16000);
    for(i=0; i < SBMAX_l + 1; i++)
        gfc->scalefac_band.l[i] = sfBandIndex[j].l[i];

    for(i=0; i < PSFB21 + 1; i++) {
        int const size = (gfc->scalefac_band.l[22] - gfc->scalefac_band.l[21]) / PSFB21;
        int const start = gfc->scalefac_band.l[21] + i * size;
        gfc->scalefac_band.psfb21[i] = start;
    }
    gfc->scalefac_band.psfb21[PSFB21] = 576;

    for(i=0; i < SBMAX_s + 1; i++)
        gfc->scalefac_band.s[i] = sfBandIndex[j].s[i];

    for(i=0; i < PSFB12 + 1; i++) {
        int const size = (gfc->scalefac_band.s[13] - gfc->scalefac_band.s[12]) / PSFB12;
        int const start = gfc->scalefac_band.s[12] + i * size;
        gfc->scalefac_band.psfb12[i] = start;
    }
    gfc->scalefac_band.psfb12[PSFB12] = 192;

    /* determine the mean bitrate for main data */
    if(cfg->mode_gr == 2) /* MPEG 1 */
        cfg->sideinfo_len = (cfg->channels_out == 1) ? 4 + 17 : 4 + 32;
    else                /* MPEG 2 */
        cfg->sideinfo_len = (cfg->channels_out == 1) ? 4 + 9 : 4 + 17;

    if(cfg->error_protection)
        cfg->sideinfo_len += 2;

    {
        int     k;

        for(k=0; k < 19; k++)
            gfc->sv_enc.pefirbuf[k] = 700 * cfg->mode_gr * cfg->channels_out;

        if(gfp->ATHtype == -1)
            gfp->ATHtype = 4;
    }

  assert(gfp->VBR_q <= 9);
  assert(gfp->VBR_q >= 0);

  switch(cfg->vbr) {

    case vbr_mt:
    case vbr_mtrh:{
            if(gfp->strict_ISO < 0) {
                gfp->strict_ISO = MDB_MAXIMUM;
            }
            if(gfp->useTemporal < 0) {
                gfp->useTemporal=0; /* off by default for this VBR mode */
            }

            apply_preset(gfp, 500 - (gfp->VBR_q * 10), 0);
            /*  The newer VBR code supports only a limited
               subset of quality levels:
               9-5=5 are the same, uses x^3/4 quantization
               4-0=0 are the same  5 plus best huffman divide code
             */
            if(gfp->quality < 0)
                gfp->quality = LAME_DEFAULT_QUALITY;
            if(gfp->quality < 5)
                gfp->quality = 0;
            if(gfp->quality > 7)
                gfp->quality = 7;

            /*  sfb21 extra only with MPEG-1 at higher sampling rates
             */
            if(gfp->experimentalY)
                gfc->sv_qnt.sfb21_extra = 0;
            else
                gfc->sv_qnt.sfb21_extra = (cfg->samplerate_out > 44000);

            break;

        }

    case vbr_rh:{
            apply_preset(gfp, 500 - (gfp->VBR_q * 10), 0);

            /*  sfb21 extra only with MPEG-1 at higher sampling rates
             */
            if(gfp->experimentalY)
                gfc->sv_qnt.sfb21_extra = 0;
            else
                gfc->sv_qnt.sfb21_extra = (cfg->samplerate_out > 44000);

            /*  VBR needs at least the output of GPSYCHO,
             *  so we have to garantee that by setting a minimum
             *  quality level, actually level 6 does it.
             *  down to level 6
             */
            if(gfp->quality > 6)
                gfp->quality = 6;


            if(gfp->quality < 0)
                gfp->quality = LAME_DEFAULT_QUALITY;

            break;
        }

    default:           /* cbr/abr */  {
            /*  no sfb21 extra with CBR code
             */
            gfc->sv_qnt.sfb21_extra = 0;

            if(gfp->quality < 0)
                gfp->quality = LAME_DEFAULT_QUALITY;


            if(cfg->vbr == vbr_off)
                lame_set_VBR_mean_bitrate_kbps(gfp, gfp->brate);
            /* second, set parameters depending on bitrate */
            apply_preset(gfp, gfp->VBR_mean_bitrate_kbps, 0);
            gfp->VBR = cfg->vbr;

            break;
        }
    }

    /*initialize default values common for all modes */

    gfc->sv_qnt.mask_adjust = gfp->maskingadjust;
    gfc->sv_qnt.mask_adjust_short = gfp->maskingadjust_short;

    /*  just another daily changing developer switch  */
    if(gfp->tune) {
        gfc->sv_qnt.mask_adjust += gfp->tune_value_a;
        gfc->sv_qnt.mask_adjust_short += gfp->tune_value_a;
    }


    if(cfg->vbr != vbr_off) { /* choose a min/max bitrate for VBR */
        /* if the user didn't specify VBR_max_bitrate: */
        cfg->vbr_min_bitrate_index = 1; /* default: allow   8 kbps (MPEG-2) or  32 kbps (MPEG-1) */
        cfg->vbr_max_bitrate_index = 14; /* default: allow 160 kbps (MPEG-2) or 320 kbps (MPEG-1) */
        if(cfg->samplerate_out < 16000)
            cfg->vbr_max_bitrate_index = 8; /* default: allow 64 kbps (MPEG-2.5) */
        if(gfp->VBR_min_bitrate_kbps) {
            gfp->VBR_min_bitrate_kbps =
                FindNearestBitrate(gfp->VBR_min_bitrate_kbps, cfg->version, cfg->samplerate_out);
            cfg->vbr_min_bitrate_index =
                BitrateIndex(gfp->VBR_min_bitrate_kbps, cfg->version, cfg->samplerate_out);
            if(cfg->vbr_min_bitrate_index < 0) {
                /* This never happens, because of preceding FindNearestBitrate!
                 * But, set a sane value, just in case
                 */
                assert(0);
                cfg->vbr_min_bitrate_index = 1;
            }
        }
        if(gfp->VBR_max_bitrate_kbps) {
            gfp->VBR_max_bitrate_kbps =
                FindNearestBitrate(gfp->VBR_max_bitrate_kbps, cfg->version, cfg->samplerate_out);
            cfg->vbr_max_bitrate_index =
                BitrateIndex(gfp->VBR_max_bitrate_kbps, cfg->version, cfg->samplerate_out);
            if(cfg->vbr_max_bitrate_index < 0) {
                /* This never happens, because of preceding FindNearestBitrate!
                 * But, set a sane value, just in case
                 */
                assert(0);
                cfg->vbr_max_bitrate_index = cfg->samplerate_out < 16000 ? 8 : 14;
            }
        }
        gfp->VBR_min_bitrate_kbps = bitrate_table[cfg->version][cfg->vbr_min_bitrate_index];
        gfp->VBR_max_bitrate_kbps = bitrate_table[cfg->version][cfg->vbr_max_bitrate_index];
        gfp->VBR_mean_bitrate_kbps =
            Min(bitrate_table[cfg->version][cfg->vbr_max_bitrate_index],
                gfp->VBR_mean_bitrate_kbps);
        gfp->VBR_mean_bitrate_kbps =
            Max(bitrate_table[cfg->version][cfg->vbr_min_bitrate_index],
                gfp->VBR_mean_bitrate_kbps);
    }

    cfg->preset = gfp->preset;
    cfg->write_lame_tag = gfp->write_lame_tag;
    gfc->sv_qnt.substep_shaping = gfp->substep_shaping;
    cfg->noise_shaping = gfp->noise_shaping;
    cfg->subblock_gain = gfp->subblock_gain;
    cfg->use_best_huffman = gfp->use_best_huffman;
    cfg->avg_bitrate = gfp->brate;
    cfg->vbr_avg_bitrate_kbps = gfp->VBR_mean_bitrate_kbps;
    cfg->compression_ratio = gfp->compression_ratio;

    /* initialize internal qval settings */
    lame_init_qval(gfp);


    /*  automatic ATH adjustment on
     */
    if(gfp->athaa_type < 0)
        gfc->ATH->use_adjust = 3;
    else
        gfc->ATH->use_adjust = gfp->athaa_type;


    /* initialize internal adaptive ATH settings  -jd */
    gfc->ATH->aa_sensitivity_p = pow(10.0, gfp->athaa_sensitivity / -10.0);


    if(gfp->short_blocks == short_block_not_set) {
        gfp->short_blocks = short_block_allowed;
    }

    /*Note Jan/2003: Many hardware decoders cannot handle short blocks in regular
       stereo mode unless they are coupled (same type in both channels)
       it is a rare event (1 frame per min. or so) that LAME would use
       uncoupled short blocks, so lets turn them off until we decide
       how to handle this.  No other encoders allow uncoupled short blocks,
       even though it is in the standard.  */
    /* rh 20040217: coupling makes no sense for mono and dual-mono streams
     */
    if(gfp->short_blocks == short_block_allowed
        && (cfg->mode == JOINT_STEREO || cfg->mode == STEREO)) {
        gfp->short_blocks = short_block_coupled;
    }

    cfg->short_blocks = gfp->short_blocks;


    if(lame_get_quant_comp(gfp) < 0)
        lame_set_quant_comp(gfp, 1);
    if(lame_get_quant_comp_short(gfp) < 0)
        lame_set_quant_comp_short(gfp, 0);

    if(lame_get_msfix(gfp) < 0)
        lame_set_msfix(gfp, 0);

    /* select psychoacoustic model */
    lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 1);

    if(gfp->ATHtype < 0)
        gfp->ATHtype = 4;

    if(gfp->ATHcurve < 0)
        gfp->ATHcurve = 4;

    if(gfp->interChRatio < 0)
        gfp->interChRatio = 0;

    if(gfp->useTemporal < 0)
        gfp->useTemporal = 1; /* on by default */


    cfg->interChRatio = gfp->interChRatio;
    cfg->msfix = gfp->msfix;
    cfg->ATH_offset_db = 0-gfp->ATH_lower_db;
    cfg->ATH_offset_factor = powf(10.f, cfg->ATH_offset_db * 0.1f);
    cfg->ATHcurve = gfp->ATHcurve;
    cfg->ATHtype = gfp->ATHtype;
    cfg->ATHonly = gfp->ATHonly;
    cfg->ATHshort = gfp->ATHshort;
    cfg->noATH = gfp->noATH;

    cfg->quant_comp = gfp->quant_comp;
    cfg->quant_comp_short = gfp->quant_comp_short;

    cfg->use_temporal_masking_effect = gfp->useTemporal;
    if(cfg->mode == JOINT_STEREO) {
        cfg->use_safe_joint_stereo = gfp->exp_nspsytune & 2;
    }
    else {
        cfg->use_safe_joint_stereo = 0;
    }
    {
        cfg->adjust_bass_db = (gfp->exp_nspsytune >> 2) & 63;
        if(cfg->adjust_bass_db >= 32.f)
            cfg->adjust_bass_db -= 64.f;
        cfg->adjust_bass_db *= 0.25f;

        cfg->adjust_alto_db = (gfp->exp_nspsytune >> 8) & 63;
        if(cfg->adjust_alto_db >= 32.f)
            cfg->adjust_alto_db -= 64.f;
        cfg->adjust_alto_db *= 0.25f;

        cfg->adjust_treble_db = (gfp->exp_nspsytune >> 14) & 63;
        if(cfg->adjust_treble_db >= 32.f)
            cfg->adjust_treble_db -= 64.f;
        cfg->adjust_treble_db *= 0.25f;

        /*  to be compatible with Naoki's original code, the next 6 bits
         *  define only the amount of changing treble for sfb21 */
        cfg->adjust_sfb21_db = (gfp->exp_nspsytune >> 20) & 63;
        if(cfg->adjust_sfb21_db >= 32.f)
            cfg->adjust_sfb21_db -= 64.f;
        cfg->adjust_sfb21_db *= 0.25f;
        cfg->adjust_sfb21_db += cfg->adjust_treble_db;
    }

    /* Setting up the PCM input data transform matrix, to apply 
     * user defined re-scaling, and or two-to-one channel downmix.
     */
    {
        FLOAT   m[2][2] = { {1.0f, 0.0f}, {0.0f, 1.0f} };

        /* user selected scaling of the samples */
        m[0][0] *= gfp->scale;
        m[0][1] *= gfp->scale;
        m[1][0] *= gfp->scale;
        m[1][1] *= gfp->scale;
        /* user selected scaling of the channel 0 (left) samples */
        m[0][0] *= gfp->scale_left;
        m[0][1] *= gfp->scale_left;
        /* user selected scaling of the channel 1 (right) samples */
        m[1][0] *= gfp->scale_right;
        m[1][1] *= gfp->scale_right;
        /* Downsample to Mono if 2 channels in and 1 channel out */
        if(cfg->channels_in == 2 && cfg->channels_out == 1) {
            m[0][0] = 0.5f * (m[0][0] + m[1][0]);
            m[0][1] = 0.5f * (m[0][1] + m[1][1]);
            m[1][0] = 0;
            m[1][1] = 0;
        }
        cfg->pcm_transform[0][0] = m[0][0];
        cfg->pcm_transform[0][1] = m[0][1];
        cfg->pcm_transform[1][0] = m[1][0];
        cfg->pcm_transform[1][1] = m[1][1];
    }

    /* padding method as described in
     * "MPEG-Layer3 / Bitstream Syntax and Decoding"
     * by Martin Sieler, Ralph Sperschneider
     *
     * note: there is no padding for the very first frame
     *
     * Robert Hegemann 2000-06-22
     */
    gfc->sv_enc.slot_lag = gfc->sv_enc.frac_SpF = 0;
    if(cfg->vbr == vbr_off)
        gfc->sv_enc.slot_lag = gfc->sv_enc.frac_SpF
            = ((cfg->version + 1) * 72000L * cfg->avg_bitrate) % cfg->samplerate_out;

    lame_init_bitstream(gfp);

    iteration_init(gfc);
    psymodel_init(gfp);

    cfg->buffer_constraint = get_max_frame_buffer_size_by_constraint(cfg, gfp->strict_ISO);


    cfg->findReplayGain = gfp->findReplayGain;
    cfg->decode_on_the_fly = gfp->decode_on_the_fly;

    if(cfg->decode_on_the_fly)
        cfg->findPeakSample = 1;

    if(cfg->findReplayGain) {
        if(InitGainAnalysis(gfc->sv_rpg.rgdata, cfg->samplerate_out) == INIT_GAIN_ANALYSIS_ERROR) {
            /* Actually this never happens, our samplerates are the ones RG accepts!
             * But just in case, turn RG off
             */
            assert(0);
            cfg->findReplayGain = 0;
        }
    }

#ifdef DECODE_ON_THE_FLY
    if(cfg->decode_on_the_fly && !gfp->decode_only) {
        if(gfc->hip) {
            hip_decode_exit(gfc->hip);
        }
        gfc->hip = hip_decode_init();
        /* report functions */
        hip_set_errorf(gfc->hip, gfp->report.errorf);
        hip_set_debugf(gfc->hip, gfp->report.debugf);
        hip_set_msgf(gfc->hip, gfp->report.msgf);
    }
#endif
    /* updating lame internal flags finished successful */
    gfc->lame_init_params_successful = 1;
    return 0;
	}

static void concatSep(char* dest, char const* sep, char const* str) {

    if(*dest != 0) 
			strcat(dest, sep);
    strcat(dest, str);
	}

/*
 *  print_config
 *
 *  Prints some selected information about the coding parameters via
 *  the macro command MSGF(), which is currently mapped to lame_errorf
 *  (reports via a error function?), which is a printf-like function
 *  for <stderr>.
 */
void lame_print_config(const lame_global_flags * gfp) {
    lame_internal_flags const *const gfc = gfp->internal_flags;
    SessionConfig_t const *const cfg = &gfc->cfg;
    double const out_samplerate = cfg->samplerate_out;
    double const in_samplerate = cfg->samplerate_in;

    MSGF(gfc, "LAME %s %s (%s)\n", get_lame_version(), get_lame_os_bitness(), get_lame_url());

#if(LAME_ALPHA_VERSION)
    MSGF(gfc, "warning: alpha versions should be used for testing only\n");
#endif
    if(gfc->CPU_features.MMX
        || gfc->CPU_features.AMD_3DNow || gfc->CPU_features.SSE || gfc->CPU_features.SSE2) {
        char    text[256] = { 0 };
        int     fft_asm_used = 0;
#ifdef HAVE_NASM
        if(gfc->CPU_features.AMD_3DNow) {
            fft_asm_used = 1;
        }
        else if(gfc->CPU_features.SSE) {
            fft_asm_used = 2;
        }
#else
# if defined( HAVE_XMMINTRIN_H ) && defined( MIN_ARCH_SSE )
        {
            fft_asm_used = 3;
        }
# endif
#endif
        if(gfc->CPU_features.MMX) {
#ifdef MMX_choose_table
            concatSep(text, ", ", "MMX (ASM used)");
#else
            concatSep(text, ", ", "MMX");
#endif
        }
        if(gfc->CPU_features.AMD_3DNow) {
            concatSep(text, ", ", (fft_asm_used == 1) ? "3DNow! (ASM used)" : "3DNow!");
        }
        if(gfc->CPU_features.SSE) {
#if defined(HAVE_XMMINTRIN_H)
            concatSep(text, ", ", "SSE (ASM used)");
#else
            concatSep(text, ", ", (fft_asm_used == 2) ? "SSE (ASM used)" : "SSE");
#endif
        }
        if(gfc->CPU_features.SSE2) {
            concatSep(text, ", ", (fft_asm_used == 3) ? "SSE2 (ASM used)" : "SSE2");
        }
        MSGF(gfc, "CPU features: %s\n", text);
    }

    if(cfg->channels_in == 2 && cfg->channels_out == 1 /* mono */ ) {
        MSGF(gfc, "Autoconverting from stereo to mono. Setting encoding to mono mode.\n");
    }

    if(isResamplingNecessary(cfg)) {
        MSGF(gfc, "Resampling:  input %g kHz  output %g kHz\n",
             1.e-3 * in_samplerate, 1.e-3 * out_samplerate);
    }

    if(cfg->highpass2 > 0.)
        MSGF(gfc,
             "Using polyphase highpass filter, transition band: %5.0f Hz - %5.0f Hz\n",
             0.5 * cfg->highpass1 * out_samplerate, 0.5 * cfg->highpass2 * out_samplerate);
    if(0. < cfg->lowpass1 || 0. < cfg->lowpass2) {
        MSGF(gfc,
             "Using polyphase lowpass filter, transition band: %5.0f Hz - %5.0f Hz\n",
             0.5 * cfg->lowpass1 * out_samplerate, 0.5 * cfg->lowpass2 * out_samplerate);
    }
    else {
        MSGF(gfc, "polyphase lowpass filter disabled\n");
    }

    if(cfg->free_format) {
        MSGF(gfc, "Warning: many decoders cannot handle free format bitstreams\n");
        if(cfg->avg_bitrate > 320) {
            MSGF(gfc,
                 "Warning: many decoders cannot handle free format bitrates >320 kbps (see documentation)\n");
        }
    }
	}


/**     rh:
 *      some pretty printing is very welcome at this point!
 *      so, if someone is willing to do so, please do it!
 *      add more, if you see more...
 */
void lame_print_internals(const lame_global_flags * gfp) {
    lame_internal_flags const *const gfc = gfp->internal_flags;
    SessionConfig_t const *const cfg = &gfc->cfg;
    const char *pc = "";

    /*  compiler/processor optimizations, operational, etc.
     */
    MSGF(gfc, "\nmisc:\n\n");

    MSGF(gfc, "\tscaling: %g\n", gfp->scale);
    MSGF(gfc, "\tch0 (left) scaling: %g\n", gfp->scale_left);
    MSGF(gfc, "\tch1 (right) scaling: %g\n", gfp->scale_right);
    switch(cfg->use_best_huffman) {
    default:
        pc = "normal";
        break;
    case 1:
        pc = "best (outside loop)";
        break;
    case 2:
        pc = "best (inside loop, slow)";
        break;
    }
    MSGF(gfc, "\thuffman search: %s\n", pc);
    MSGF(gfc, "\texperimental Y=%d\n", gfp->experimentalY);
    MSGF(gfc, "\t...\n");

    /*  everything controlling the stream format
     */
    MSGF(gfc, "\nstream format:\n\n");
    switch(cfg->version) {
    case 0:
        pc = "2.5";
        break;
    case 1:
        pc = "1";
        break;
    case 2:
        pc = "2";
        break;
    default:
        pc = "?";
        break;
    }
    MSGF(gfc, "\tMPEG-%s Layer 3\n", pc);
    switch(cfg->mode) {
    case JOINT_STEREO:
        pc = "joint stereo";
        break;
    case STEREO:
        pc = "stereo";
        break;
    case DUAL_CHANNEL:
        pc = "dual channel";
        break;
    case MONO:
        pc = "mono";
        break;
    case NOT_SET:
        pc = "not set (error)";
        break;
    default:
        pc = "unknown (error)";
        break;
    }
    MSGF(gfc, "\t%d channel - %s\n", cfg->channels_out, pc);

    switch(cfg->vbr) {
    case vbr_off:
        pc = "off";
        break;
    default:
        pc = "all";
        break;
    }
    MSGF(gfc, "\tpadding: %s\n", pc);

    if(vbr_default == cfg->vbr)
        pc = "(default)";
    else if(cfg->free_format)
        pc = "(free format)";
    else
        pc = "";
    switch(cfg->vbr) {
    case vbr_off:
        MSGF(gfc, "\tconstant bitrate - CBR %s\n", pc);
        break;
    case vbr_abr:
        MSGF(gfc, "\tvariable bitrate - ABR %s\n", pc);
        break;
    case vbr_rh:
        MSGF(gfc, "\tvariable bitrate - VBR rh %s\n", pc);
        break;
    case vbr_mt:
        MSGF(gfc, "\tvariable bitrate - VBR mt %s\n", pc);
        break;
    case vbr_mtrh:
        MSGF(gfc, "\tvariable bitrate - VBR mtrh %s\n", pc);
        break;
    default:
        MSGF(gfc, "\t ?? oops, some new one ?? \n");
        break;
    }
    if(cfg->write_lame_tag)
        MSGF(gfc, "\tusing LAME Tag\n");
    MSGF(gfc, "\t...\n");

    /*  everything controlling psychoacoustic settings, like ATH, etc.
     */
    MSGF(gfc, "\npsychoacoustic:\n\n");

    switch(cfg->short_blocks) {
    default:
    case short_block_not_set:
        pc = "?";
        break;
    case short_block_allowed:
        pc = "allowed";
        break;
    case short_block_coupled:
        pc = "channel coupled";
        break;
    case short_block_dispensed:
        pc = "dispensed";
        break;
    case short_block_forced:
        pc = "forced";
        break;
    }
    MSGF(gfc, "\tusing short blocks: %s\n", pc);
    MSGF(gfc, "\tsubblock gain: %d\n", cfg->subblock_gain);
    MSGF(gfc, "\tadjust masking: %g dB\n", gfc->sv_qnt.mask_adjust);
    MSGF(gfc, "\tadjust masking short: %g dB\n", gfc->sv_qnt.mask_adjust_short);
    MSGF(gfc, "\tquantization comparison: %d\n", cfg->quant_comp);
    MSGF(gfc, "\t ^ comparison short blocks: %d\n", cfg->quant_comp_short);
    MSGF(gfc, "\tnoise shaping: %d\n", cfg->noise_shaping);
    MSGF(gfc, "\t ^ amplification: %d\n", cfg->noise_shaping_amp);
    MSGF(gfc, "\t ^ stopping: %d\n", cfg->noise_shaping_stop);

    pc = "using";
    if(cfg->ATHshort)
        pc = "the only masking for short blocks";
    if(cfg->ATHonly)
        pc = "the only masking";
    if(cfg->noATH)
        pc = "not used";
    MSGF(gfc, "\tATH: %s\n", pc);
    MSGF(gfc, "\t ^ type: %d\n", cfg->ATHtype);
    MSGF(gfc, "\t ^ shape: %g%s\n", cfg->ATHcurve, " (only for type 4)");
    MSGF(gfc, "\t ^ level adjustement: %g dB\n", cfg->ATH_offset_db);
    MSGF(gfc, "\t ^ adjust type: %d\n", gfc->ATH->use_adjust);
    MSGF(gfc, "\t ^ adjust sensitivity power: %f\n", gfc->ATH->aa_sensitivity_p);

    MSGF(gfc, "\texperimental psy tunings by Naoki Shibata\n");
    MSGF(gfc, "\t   adjust masking bass=%g dB, alto=%g dB, treble=%g dB, sfb21=%g dB\n",
         10 * log10(gfc->sv_qnt.longfact[0]),
         10 * log10(gfc->sv_qnt.longfact[7]),
         10 * log10(gfc->sv_qnt.longfact[14]), 10 * log10(gfc->sv_qnt.longfact[21]));

    pc = cfg->use_temporal_masking_effect ? "yes" : "no";
    MSGF(gfc, "\tusing temporal masking effect: %s\n", pc);
    MSGF(gfc, "\tinterchannel masking ratio: %g\n", cfg->interChRatio);
    MSGF(gfc, "\t...\n");

    /*  that's all ?
     */
    MSGF(gfc, "\n");
    return;
	}


static void save_gain_values(lame_internal_flags * gfc) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    RpgStateVar_t const *const rsv = &gfc->sv_rpg;
    RpgResult_t *const rov = &gfc->ov_rpg;

    /* save the ReplayGain value */
    if(cfg->findReplayGain) {
        FLOAT const RadioGain = (FLOAT) GetTitleGain(rsv->rgdata);
        if(NEQ(RadioGain, GAIN_NOT_ENOUGH_SAMPLES)) {
            rov->RadioGain = (int) floor(RadioGain * 10.0 + 0.5); /* round to nearest */
        }
        else {
            rov->RadioGain = 0;
        }
    }

    /* find the gain and scale change required for no clipping */
    if(cfg->findPeakSample) {
        rov->noclipGainChange = (int) ceil(log10(rov->PeakSample / 32767.0) * 20.0 * 10.0); /* round up */

        if(rov->noclipGainChange > 0) { /* clipping occurs */
            rov->noclipScale = floor((32767.0f / rov->PeakSample) * 100.0f) / 100.0f; /* round down */
        }
        else            /* no clipping */
            rov->noclipScale = -1.0f;
    }
	}



static int update_inbuffer_size(lame_internal_flags * gfc, const int nsamples) {
    EncStateVar_t *const esv = &gfc->sv_enc;

    if(esv->in_buffer_0 == 0 || esv->in_buffer_nsamples < nsamples) {
        if(esv->in_buffer_0) {
            free(esv->in_buffer_0);
        }
        if(esv->in_buffer_1) {
            free(esv->in_buffer_1);
        }
        esv->in_buffer_0 = lame_calloc(sample_t, nsamples);
        esv->in_buffer_1 = lame_calloc(sample_t, nsamples);
        esv->in_buffer_nsamples = nsamples;
    }
    if(esv->in_buffer_0 == NULL || esv->in_buffer_1 == NULL) {
        if(esv->in_buffer_0) {
            free(esv->in_buffer_0);
        }
        if(esv->in_buffer_1) {
            free(esv->in_buffer_1);
        }
        esv->in_buffer_0 = 0;
        esv->in_buffer_1 = 0;
        esv->in_buffer_nsamples = 0;
        ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
        return -2;
    }

  return 0;
	}


static int calcNeeded(SessionConfig_t const * cfg) {
    int     mf_needed;
    int     pcm_samples_per_frame = 576 * cfg->mode_gr;

    /* some sanity checks */
#if ENCDELAY < MDCTDELAY
# error ENCDELAY is less than MDCTDELAY, see encoder.h
#endif
#if FFTOFFSET > BLKSIZE
# error FFTOFFSET is greater than BLKSIZE, see encoder.h
#endif

    mf_needed = BLKSIZE + pcm_samples_per_frame - FFTOFFSET; /* amount needed for FFT */
    /*mf_needed = Max(mf_needed, 286 + 576 * (1 + gfc->mode_gr)); */
    mf_needed = Max(mf_needed, 512 + pcm_samples_per_frame - 32);

    assert(MFSIZE >= mf_needed);
    
    return mf_needed;
	}


/*
 * THE MAIN LAME ENCODING INTERFACE
 * mt 3/00
 *
 * input pcm data, output (maybe) mp3 frames.
 * This routine handles all buffering, resampling and filtering for you.
 * The required mp3buffer_size can be computed from num_samples,
 * samplerate and encoding rate, but here is a worst case estimate:
 *
 * mp3buffer_size in bytes = 1.25*num_samples + 7200
 *
 * return code = number of bytes output in mp3buffer.  can be 0
 *
 * NOTE: this routine uses LAME's internal PCM data representation,
 * 'sample_t'.  It should not be used by any application.
 * applications should use lame_encode_buffer(),
 *                         lame_encode_buffer_float()
 *                         lame_encode_buffer_int()
 * etc... depending on what type of data they are working with.
*/
static int lame_encode_buffer_sample_t(lame_internal_flags * gfc,
                            int nsamples, unsigned char *mp3buf, const int mp3buf_size) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *const esv = &gfc->sv_enc;
    int     pcm_samples_per_frame = 576 * cfg->mode_gr;
    int     mp3size = 0, ret, i, ch, mf_needed;
    int     mp3out;
    sample_t *mfbuf[2];
    sample_t *in_buffer[2];

    if(gfc->class_id != LAME_ID)
        return -3;

    if(nsamples == 0)
        return 0;

    /* copy out any tags that may have been written into bitstream */
    {   /* if user specifed buffer size = 0, dont check size */
        int const buf_size = mp3buf_size == 0 ? INT_MAX : mp3buf_size;
        mp3out = copy_buffer(gfc, mp3buf, buf_size, 0);
    }
    if(mp3out < 0)
        return mp3out;  /* not enough buffer space */
    mp3buf += mp3out;
    mp3size += mp3out;

    in_buffer[0] = esv->in_buffer_0;
    in_buffer[1] = esv->in_buffer_1;

    mf_needed = calcNeeded(cfg);

    mfbuf[0] = esv->mfbuf[0];
    mfbuf[1] = esv->mfbuf[1];

    while(nsamples > 0) {
        sample_t const *in_buffer_ptr[2];
        int     n_in = 0;    /* number of input samples processed with fill_buffer */
        int     n_out = 0;   /* number of samples output with fill_buffer */
        /* n_in <> n_out if we are resampling */

        in_buffer_ptr[0] = in_buffer[0];
        in_buffer_ptr[1] = in_buffer[1];
        /* copy in new samples into mfbuf, with resampling */
        fill_buffer(gfc, mfbuf, &in_buffer_ptr[0], nsamples, &n_in, &n_out);

        /* compute ReplayGain of resampled input if requested */
        if(cfg->findReplayGain && !cfg->decode_on_the_fly)
            if(AnalyzeSamples
                (gfc->sv_rpg.rgdata, &mfbuf[0][esv->mf_size], &mfbuf[1][esv->mf_size], n_out,
                 cfg->channels_out) == GAIN_ANALYSIS_ERROR)
                return -6;



        /* update in_buffer counters */
        nsamples -= n_in;
        in_buffer[0] += n_in;
        if(cfg->channels_out == 2)
            in_buffer[1] += n_in;

        /* update mfbuf[] counters */
        esv->mf_size += n_out;
        assert(esv->mf_size <= MFSIZE);
        
        /* lame_encode_flush may have set gfc->mf_sample_to_encode to 0
         * so we have to reinitialize it here when that happened.
         */
        if(esv->mf_samples_to_encode < 1) {
            esv->mf_samples_to_encode = ENCDELAY + POSTDELAY;
					}        
        esv->mf_samples_to_encode += n_out;


        if(esv->mf_size >= mf_needed) {
            /* encode the frame.  */
            /* mp3buf              = pointer to current location in buffer */
            /* mp3buf_size         = size of original mp3 output buffer */
            /*                     = 0 if we should not worry about the */
            /*                       buffer size because calling program is  */
            /*                       to lazy to compute it */
            /* mp3size             = size of data written to buffer so far */
            /* mp3buf_size-mp3size = amount of space avalable  */

            int     buf_size = mp3buf_size - mp3size;
            if(mp3buf_size == 0)
                buf_size = INT_MAX;

            ret = lame_encode_mp3_frame(gfc, mfbuf[0], mfbuf[1], mp3buf, buf_size);

            if(ret < 0)
                return ret;
            mp3buf += ret;
            mp3size += ret;

            /* shift out old samples */
            esv->mf_size -= pcm_samples_per_frame;
            esv->mf_samples_to_encode -= pcm_samples_per_frame;
            for(ch = 0; ch < cfg->channels_out; ch++)
                for(i = 0; i < esv->mf_size; i++)
                    mfbuf[ch][i] = mfbuf[ch][i + pcm_samples_per_frame];
        }
	    }
    assert(nsamples == 0);

  return mp3size;
	}

enum PCMSampleType {
	pcm_short_type
,   pcm_int_type
,   pcm_long_type
,   pcm_float_type
,   pcm_double_type
};

static void lame_copy_inbuffer(lame_internal_flags* gfc, 
                   void const* l, void const* r, int nsamples,
                   enum PCMSampleType pcm_type, int jump, FLOAT s) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *const esv = &gfc->sv_enc;
    sample_t* ib0 = esv->in_buffer_0;
    sample_t* ib1 = esv->in_buffer_1;
    FLOAT   m[2][2];

    /* Apply user defined re-scaling */
    m[0][0] = s * cfg->pcm_transform[0][0];
    m[0][1] = s * cfg->pcm_transform[0][1];
    m[1][0] = s * cfg->pcm_transform[1][0];
    m[1][1] = s * cfg->pcm_transform[1][1];

    /* make a copy of input buffer, changing type to sample_t */
#define COPY_AND_TRANSFORM(T) \
{ \
    T const *bl = (T const*)l, *br = (T const*)r; \
    int     i; \
    for(i=0; i < nsamples; i++) { \
        sample_t const xl = *bl; \
        sample_t const xr = *br; \
        sample_t const u = xl * m[0][0] + xr * m[0][1]; \
        sample_t const v = xl * m[1][0] + xr * m[1][1]; \
        ib0[i] = u; \
        ib1[i] = v; \
        bl += jump; \
        br += jump; \
    } \
}
  switch(pcm_type) {
    case pcm_short_type: 
        COPY_AND_TRANSFORM(short int);
        break;
    case pcm_int_type:
        COPY_AND_TRANSFORM(int);
        break;
    case pcm_long_type:
        COPY_AND_TRANSFORM(long int);
        break;
    case pcm_float_type:
        COPY_AND_TRANSFORM(float);
        break;
    case pcm_double_type:
        COPY_AND_TRANSFORM(double);
        break;
    }
	}


static int lame_encode_buffer_template(lame_global_flags * gfp,
                            void const* buffer_l, void const* buffer_r, const int nsamples,
                            unsigned char *mp3buf, const int mp3buf_size, enum PCMSampleType pcm_type, int aa, FLOAT norm) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            SessionConfig_t const *const cfg = &gfc->cfg;

            if(nsamples == 0)
                return 0;

            if(update_inbuffer_size(gfc, nsamples) != 0) {
                return -2;
							}
            // make a copy of input buffer, changing type to sample_t
            if(cfg->channels_in > 1) {
              if(buffer_l == NULL || buffer_r == NULL) {
                return 0;
                }
              lame_copy_inbuffer(gfc, buffer_l, buffer_r, nsamples, pcm_type, aa, norm);
							}
            else {
              if(buffer_l == NULL) {
                return 0;
                }
              lame_copy_inbuffer(gfc, buffer_l, buffer_l, nsamples, pcm_type, aa, norm);
	            }

            return lame_encode_buffer_sample_t(gfc, nsamples, mp3buf, mp3buf_size);
        }
    }

  return -3;
	}

int lame_encode_buffer(lame_global_flags * gfp,
                   const short int pcm_l[], const short int pcm_r[], const int nsamples,
                   unsigned char *mp3buf, const int mp3buf_size) {

  return lame_encode_buffer_template(gfp, pcm_l, pcm_r, nsamples, mp3buf, mp3buf_size, pcm_short_type, 1, 1.0);
	}


int lame_encode_buffer_float(lame_global_flags * gfp,
                         const float pcm_l[], const float pcm_r[], const int nsamples,
                         unsigned char *mp3buf, const int mp3buf_size) {

  /* input is assumed to be normalized to +/- 32768 for full scale */
  return lame_encode_buffer_template(gfp, pcm_l, pcm_r, nsamples, mp3buf, mp3buf_size, pcm_float_type, 1, 1.0);
	}


int lame_encode_buffer_ieee_float(lame_t gfp,
                         const float pcm_l[], const float pcm_r[], const int nsamples,
                         unsigned char *mp3buf, const int mp3buf_size) {

  /* input is assumed to be normalized to +/- 1.0 for full scale */
  return lame_encode_buffer_template(gfp, pcm_l, pcm_r, nsamples, mp3buf, mp3buf_size, pcm_float_type, 1, 32767.0);
	}


int lame_encode_buffer_interleaved_ieee_float(lame_t gfp,
                         const float pcm[], const int nsamples,
                         unsigned char *mp3buf, const int mp3buf_size) {

  /* input is assumed to be normalized to +/- 1.0 for full scale */
  return lame_encode_buffer_template(gfp, pcm, pcm+1, nsamples, mp3buf, mp3buf_size, pcm_float_type, 2, 32767.0);
	}


int lame_encode_buffer_ieee_double(lame_t gfp,
                         const double pcm_l[], const double pcm_r[], const int nsamples,
                         unsigned char *mp3buf, const int mp3buf_size) {

  /* input is assumed to be normalized to +/- 1.0 for full scale */
  return lame_encode_buffer_template(gfp, pcm_l, pcm_r, nsamples, mp3buf, mp3buf_size, pcm_double_type, 1, 32767.0);
	}


int lame_encode_buffer_interleaved_ieee_double(lame_t gfp,
                         const double pcm[], const int nsamples,
                         unsigned char *mp3buf, const int mp3buf_size) {

  /* input is assumed to be normalized to +/- 1.0 for full scale */
  return lame_encode_buffer_template(gfp, pcm, pcm+1, nsamples, mp3buf, mp3buf_size, pcm_double_type, 2, 32767.0);
	}


int lame_encode_buffer_int(lame_global_flags * gfp,
                       const int pcm_l[], const int pcm_r[], const int nsamples,
                       unsigned char *mp3buf, const int mp3buf_size) {

  /* input is assumed to be normalized to +/- MAX_INT for full scale */
  FLOAT const norm = (1.0 / (1L << (8 * sizeof(int) - 16)));
  return lame_encode_buffer_template(gfp, pcm_l, pcm_r, nsamples, mp3buf, mp3buf_size, pcm_int_type, 1, norm);
	}


int lame_encode_buffer_long2(lame_global_flags * gfp,
                         const long pcm_l[],  const long pcm_r[], const int nsamples,
                         unsigned char *mp3buf, const int mp3buf_size) {

  /* input is assumed to be normalized to +/- MAX_LONG for full scale */
  FLOAT const norm = (1.0 / (1L << (8 * sizeof(long) - 16)));
  return lame_encode_buffer_template(gfp, pcm_l, pcm_r, nsamples, mp3buf, mp3buf_size, pcm_long_type, 1, norm);
	}


int lame_encode_buffer_long(lame_global_flags * gfp,
                        const long pcm_l[], const long pcm_r[], const int nsamples,
                        unsigned char *mp3buf, const int mp3buf_size) {

  /* input is assumed to be normalized to +/- 32768 for full scale */
  return lame_encode_buffer_template(gfp, pcm_l, pcm_r, nsamples, mp3buf, mp3buf_size, pcm_long_type, 1, 1.0);
	}


int lame_encode_buffer_interleaved(lame_global_flags * gfp,
                               const short int pcm[], int nsamples,
                               unsigned char *mp3buf, int mp3buf_size) {

  /* input is assumed to be normalized to +/- MAX_SHORT for full scale */
  return lame_encode_buffer_template(gfp, pcm, pcm+1, nsamples, mp3buf, mp3buf_size, pcm_short_type, 2, 1.0);
	}


int lame_encode_buffer_interleaved_int(lame_t gfp,
                                   const int pcm[], const int nsamples,
                                   unsigned char *mp3buf, const int mp3buf_size) {

  /* input is assumed to be normalized to +/- MAX(int) for full scale */
  FLOAT const norm = (1.0 / (1L << (8 * sizeof(int)-16)));
  return lame_encode_buffer_template(gfp, pcm, pcm + 1, nsamples, mp3buf, mp3buf_size, pcm_int_type, 2, norm);
	}




/*****************************************************************
 Flush mp3 buffer, pad with ancillary data so last frame is complete.
 Reset reservoir size to 0
 but keep all PCM samples and MDCT data in memory
 This option is used to break a large file into several mp3 files
 that when concatenated together will decode with no gaps
 Because we set the reservoir=0, they will also decode seperately
 with no errors.
*********************************************************************/
int lame_encode_flush_nogap(lame_global_flags * gfp, unsigned char *mp3buffer, int mp3buffer_size) {
    int rc = -3;

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            flush_bitstream(gfc);
            /* if user specifed buffer size = 0, dont check size */
            if(mp3buffer_size == 0)
                mp3buffer_size = INT_MAX;
            rc = copy_buffer(gfc, mp3buffer, mp3buffer_size, 1);
            save_gain_values(gfc);
        }
    }

  return rc;
	}


/* called by lame_init_params.  You can also call this after flush_nogap
   if you want to write new id3v2 and Xing VBR tags into the bitstream */
int lame_init_bitstream(lame_global_flags * gfp) {

  if(is_lame_global_flags_valid(gfp)) {
    lame_internal_flags *const gfc = gfp->internal_flags;
    if(gfc) {
      gfc->ov_enc.frame_number = 0;

      if(gfp->write_id3tag_automatic) {
	      id3tag_write_v2(gfp);
		    }
      /* initialize histogram data optionally used by frontend */
      memset(gfc->ov_enc.bitrate_channelmode_hist, 0,
             sizeof(gfc->ov_enc.bitrate_channelmode_hist));
      memset(gfc->ov_enc.bitrate_blocktype_hist, 0,
             sizeof(gfc->ov_enc.bitrate_blocktype_hist));

      gfc->ov_rpg.PeakSample = 0.0;

      /* Write initial VBR Header to bitstream and init VBR data */
      if(gfc->cfg.write_lame_tag)
        InitVbrTag(gfp);

      return 0;
      }
    }

  return -3;
	}


/*****************************************************************/
/* flush internal PCM sample buffers, then mp3 buffers           */
/* then write id3 v1 tags into bitstream.                        */
/*****************************************************************/
int lame_encode_flush(lame_global_flags * gfp, unsigned char *mp3buffer, int mp3buffer_size) {
  lame_internal_flags *gfc;
  SessionConfig_t const *cfg;
  EncStateVar_t *esv;
  short int buffer[2][1152];
  int imp3 = 0, mp3count, mp3buffer_size_remaining;

  /* we always add POSTDELAY=288 padding to make sure granule with real
   * data can be complety decoded (because of 50% overlap with next granule */
  int     end_padding;
  int     frames_left;
  int     samples_to_encode;
  int     pcm_samples_per_frame;
  int     mf_needed;
  int     is_resampling_necessary;
  double  resample_ratio = 1;

  if(!is_lame_global_flags_valid(gfp)) {
    return -3;
    }
  gfc = gfp->internal_flags;
  if(!is_lame_internal_flags_valid(gfc)) {
    return -3;
    }
  cfg = &gfc->cfg;
  esv = &gfc->sv_enc;
    
    /* Was flush already called? */
  if(esv->mf_samples_to_encode < 1) {
      return 0;
    }
  pcm_samples_per_frame = 576 * cfg->mode_gr;
  mf_needed = calcNeeded(cfg);

    samples_to_encode = esv->mf_samples_to_encode - POSTDELAY;

    memset(buffer, 0, sizeof(buffer));
    mp3count = 0;

    is_resampling_necessary = isResamplingNecessary(cfg);
    if(is_resampling_necessary) {
        resample_ratio = (double)cfg->samplerate_in / (double)cfg->samplerate_out;
        /* delay due to resampling; needs to be fixed, if resampling code gets changed */
        samples_to_encode += 16. / resample_ratio;
    }
    end_padding = pcm_samples_per_frame - (samples_to_encode % pcm_samples_per_frame);
    if(end_padding < 576)
        end_padding += pcm_samples_per_frame;
    gfc->ov_enc.encoder_padding = end_padding;
    
    frames_left = (samples_to_encode + end_padding) / pcm_samples_per_frame;
    while(frames_left > 0 && imp3 >= 0) {
        int const frame_num = gfc->ov_enc.frame_number;
        int     bunch = mf_needed - esv->mf_size;

        bunch *= resample_ratio;
        if(bunch > 1152) bunch = 1152;
        if(bunch < 1) bunch = 1;

        mp3buffer_size_remaining = mp3buffer_size - mp3count;

        /* if user specifed buffer size = 0, dont check size */
        if(mp3buffer_size == 0)
            mp3buffer_size_remaining = 0;

        /* send in a frame of 0 padding until all internal sample buffers
         * are flushed
         */
        imp3 = lame_encode_buffer(gfp, buffer[0], buffer[1], bunch,
                                  mp3buffer, mp3buffer_size_remaining);

        mp3buffer += imp3;
        mp3count += imp3;
        {   /* even a single pcm sample can produce several frames!
             * for example: 1 Hz input file resampled to 8 kHz mpeg2.5
             */
            int const new_frames = gfc->ov_enc.frame_number - frame_num;
            if(new_frames > 0)
                frames_left -=  new_frames;
        }
    }
    /* Set esv->mf_samples_to_encode to 0, so we may detect
     * and break loops calling it more than once in a row.
     */
    esv->mf_samples_to_encode = 0;

    if(imp3 < 0) {
        /* some type of fatal error */
        return imp3;
    }

    mp3buffer_size_remaining = mp3buffer_size - mp3count;
    /* if user specifed buffer size = 0, dont check size */
    if(mp3buffer_size == 0)
        mp3buffer_size_remaining = INT_MAX;

    /* mp3 related stuff.  bit buffer might still contain some mp3 data */
    flush_bitstream(gfc);
    imp3 = copy_buffer(gfc, mp3buffer, mp3buffer_size_remaining, 1);
    save_gain_values(gfc);
    if(imp3 < 0) {
      /* some type of fatal error */
      return imp3;
			}
    mp3buffer += imp3;
    mp3count += imp3;
    mp3buffer_size_remaining = mp3buffer_size - mp3count;
    /* if user specifed buffer size = 0, dont check size */
    if(mp3buffer_size == 0)
        mp3buffer_size_remaining = INT_MAX;

    if(gfp->write_id3tag_automatic) {
        /* write a id3 tag to the bitstream */
        id3tag_write_v1(gfp);

        imp3 = copy_buffer(gfc, mp3buffer, mp3buffer_size_remaining, 0);

        if(imp3 < 0) {
            return imp3;
        }
        mp3count += imp3;
    }
#if 0
    {
        int const ed = gfc->ov_enc.encoder_delay;
        int const ep = gfc->ov_enc.encoder_padding;
        int const ns = (gfc->ov_enc.frame_number * pcm_samples_per_frame) - (ed + ep);
        double  duration = ns;
        duration /= cfg->samplerate_out;
        MSGF(gfc, "frames=%d\n", gfc->ov_enc.frame_number);
        MSGF(gfc, "pcm_samples_per_frame=%d\n", pcm_samples_per_frame);
        MSGF(gfc, "encoder delay=%d\n", ed);
        MSGF(gfc, "encoder padding=%d\n", ep);
        MSGF(gfc, "sample count=%d (%g)\n", ns, cfg->samplerate_in * duration);
        MSGF(gfc, "duration=%g sec\n", duration);
    }
#endif

  return mp3count;
	}

/***********************************************************************
 *
 *      lame_close ()
 *
 *  frees internal buffers
 *
 ***********************************************************************/
int lame_close(lame_global_flags *gfp) {
  int ret = 0;

	if(gfp && gfp->class_id == LAME_ID) {
    lame_internal_flags *const gfc = gfp->internal_flags;
    gfp->class_id = 0;
    if(!gfc || gfc->class_id != LAME_ID) {
      ret = -3;
		  }
    if(gfc) {
      gfc->lame_init_params_successful = 0;
      gfc->class_id = 0;
      /* this routine will free all malloc'd data in gfc, and then free gfc: */
      freegfc(gfc);
      gfp->internal_flags = NULL;
			}
    if(gfp->lame_allocated_gfp) {
      gfp->lame_allocated_gfp = 0;
      free(gfp);
      }
    }

  return ret;
	}

/*****************************************************************/
/* flush internal mp3 buffers, and free internal buffers         */
/*****************************************************************/
#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
int CDECL
lame_encode_finish(lame_global_flags * gfp, unsigned char *mp3buffer, int mp3buffer_size);
#else
#endif

int lame_encode_finish(lame_global_flags * gfp, unsigned char *mp3buffer, int mp3buffer_size) {
    int const ret = lame_encode_flush(gfp, mp3buffer, mp3buffer_size);

    lame_close(gfp);

  return ret;
	}

/*****************************************************************/
/* write VBR Xing header, and ID3 version 1 tag, if asked for    */
/*****************************************************************/
void lame_mp3_tags_fid(lame_global_flags * gfp, FILE * fpStream);

void lame_mp3_tags_fid(lame_global_flags * gfp, FILE * fpStream) {
  lame_internal_flags *gfc;
  SessionConfig_t const *cfg;

  if(!is_lame_global_flags_valid(gfp)) {
      return;
    }
  gfc = gfp->internal_flags;
  if(!is_lame_internal_flags_valid(gfc)) {
      return;
    }
  cfg = &gfc->cfg;
  if(!cfg->write_lame_tag) {
      return;
    }
  /* Write Xing header again */
  if(fpStream && !fseek(fpStream, 0, SEEK_SET)) {
      int     rc = PutVbrTag(gfp, fpStream);
      switch(rc) {
        default:
            /* OK */
            break;

        case -1:
            ERRORF(gfc, "Error: could not update LAME tag.\n");
            break;

        case -2:
            ERRORF(gfc, "Error: could not update LAME tag, file not seekable.\n");
            break;

        case -3:
            ERRORF(gfc, "Error: could not update LAME tag, file not readable.\n");
            break;
        }
    }
	}


static int lame_init_internal_flags(lame_internal_flags* gfc) {

    if(!gfc)
        return -1;

    gfc->cfg.vbr_min_bitrate_index = 1; /* not  0 ????? */
    gfc->cfg.vbr_max_bitrate_index = 13; /* not 14 ????? */
    gfc->cfg.decode_on_the_fly = 0;
    gfc->cfg.findReplayGain = 0;
    gfc->cfg.findPeakSample = 0;

    gfc->sv_qnt.OldValue[0] = 180;
    gfc->sv_qnt.OldValue[1] = 180;
    gfc->sv_qnt.CurrentStep[0] = 4;
    gfc->sv_qnt.CurrentStep[1] = 4;
    gfc->sv_qnt.masking_lower = 1;

    /* The reason for
     *       int mf_samples_to_encode = ENCDELAY + POSTDELAY;
     * ENCDELAY = internal encoder delay.  And then we have to add POSTDELAY=288
     * because of the 50% MDCT overlap.  A 576 MDCT granule decodes to
     * 1152 samples.  To synthesize the 576 samples centered under this granule
     * we need the previous granule for the first 288 samples (no problem), and
     * the next granule for the next 288 samples (not possible if this is last
     * granule).  So we need to pad with 288 samples to make sure we can
     * encode the 576 samples we are interested in.
     */
    gfc->sv_enc.mf_samples_to_encode = ENCDELAY + POSTDELAY;
    gfc->sv_enc.mf_size = ENCDELAY - MDCTDELAY; /* we pad input with this many 0's */
    gfc->ov_enc.encoder_padding = 0;
    gfc->ov_enc.encoder_delay = ENCDELAY;

    gfc->ov_rpg.RadioGain = 0;
    gfc->ov_rpg.noclipGainChange = 0;
    gfc->ov_rpg.noclipScale = -1.0;

    gfc->ATH = lame_calloc(ATH_t, 1);
    if(!gfc->ATH)
        return -2;      /* maybe error codes should be enumerated in lame.h ?? */

    gfc->sv_rpg.rgdata = lame_calloc(replaygain_t, 1);
    if(!gfc->sv_rpg.rgdata) {
        return -2;
    }
  return 0;
	}

/* initialize mp3 encoder */
#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
static
#else
#endif
int lame_init_old(lame_global_flags *gfp) {

  disable_FPE();      /* disable floating point exceptions */

  memset(gfp, 0, sizeof(lame_global_flags));

  gfp->class_id = LAME_ID;

  /* Global flags.  set defaults here for non-zero values */
  /* see lame.h for description */
  /* set integer values to -1 to mean that LAME will compute the
   * best value, UNLESS the calling program as set it
   * (and the value is no longer -1)
   */
  gfp->strict_ISO = MDB_MAXIMUM;

  gfp->mode = NOT_SET;
  gfp->original = 1;
  gfp->samplerate_in = 44100;
  gfp->num_channels = 2;
  gfp->num_samples = MAX_U_32_NUM;

  gfp->write_lame_tag = 1;
  gfp->quality = -1;
  gfp->short_blocks = short_block_not_set;
  gfp->subblock_gain = -1;

  gfp->lowpassfreq = 0;
  gfp->highpassfreq = 0;
  gfp->lowpasswidth = -1;
  gfp->highpasswidth = -1;

  gfp->VBR = vbr_off;
  gfp->VBR_q = 4;
  gfp->VBR_mean_bitrate_kbps = 128;
  gfp->VBR_min_bitrate_kbps = 0;
  gfp->VBR_max_bitrate_kbps = 0;
  gfp->VBR_hard_min = 0;

  gfp->quant_comp = -1;
  gfp->quant_comp_short = -1;

  gfp->msfix = -1;

  gfp->attackthre = -1;
  gfp->attackthre_s = -1;

  gfp->scale = 1;
  gfp->scale_left = 1;
  gfp->scale_right = 1;

  gfp->ATHcurve = -1;
  gfp->ATHtype = -1;  /* default = -1 = set in lame_init_params */
  /* 2 = equal loudness curve */
  gfp->athaa_sensitivity = 0.0; /* no offset */
  gfp->athaa_type = -1;
  gfp->useTemporal = -1;
  gfp->interChRatio = -1;

  gfp->findReplayGain = 0;
  gfp->decode_on_the_fly = 0;

  gfp->asm_optimizations.mmx = 1;
  gfp->asm_optimizations.amd3dnow = 1;
  gfp->asm_optimizations.sse = 1;

  gfp->preset = 0;

  gfp->write_id3tag_automatic = 1;

  gfp->report.debugf = &lame_report_def;
  gfp->report.errorf = &lame_report_def;
  gfp->report.msgf = &lame_report_def;

  gfp->internal_flags = lame_calloc(lame_internal_flags, 1);

  if(lame_init_internal_flags(gfp->internal_flags) < 0) {
    freegfc(gfp->internal_flags);
    gfp->internal_flags = 0;
    return -1;
    }

  return 0;
	}


lame_global_flags *lame_init(void) {
  lame_global_flags *gfp;
  int     ret;

  init_log_table();

  gfp = lame_calloc(lame_global_flags, 1);
  if(!gfp)
    return NULL;

  ret = lame_init_old(gfp);
  if(ret != 0) {
    free(gfp);
    return NULL;
    }

  gfp->lame_allocated_gfp = 1;
  return gfp;
	}


/***********************************************************************
 *
 *  some simple statistics
 *
 *  Robert Hegemann 2000-10-11
 *
 ***********************************************************************/

/*  histogram of used bitrate indexes:
 *  One has to weight them to calculate the average bitrate in kbps
 *
 *  bitrate indices:
 *  there are 14 possible bitrate indices, 0 has the special meaning
 *  "free format" which is not possible to mix with VBR and 15 is forbidden
 *  anyway.
 *
 *  stereo modes:
 *  0: LR   number of left-right encoded frames
 *  1: LR-I number of left-right and intensity encoded frames
 *  2: MS   number of mid-side encoded frames
 *  3: MS-I number of mid-side and intensity encoded frames
 *
 *  4: number of encoded frames
 *
 */
void lame_bitrate_kbps(const lame_global_flags * gfp, int bitrate_kbps[14]) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            SessionConfig_t const *const cfg = &gfc->cfg;
            int     i;
            if(cfg->free_format) {
                for(i=0; i < 14; i++)
                    bitrate_kbps[i] = -1;
                bitrate_kbps[0] = cfg->avg_bitrate;
            }
            else {
                for(i=0; i < 14; i++)
                    bitrate_kbps[i] = bitrate_table[cfg->version][i + 1];
            }
        }
    }
	}


void lame_bitrate_hist(const lame_global_flags * gfp, int bitrate_count[14]) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            SessionConfig_t const *const cfg = &gfc->cfg;
            EncResult_t const *const eov = &gfc->ov_enc;
            int     i;

            if(cfg->free_format) {
                for(i=0; i < 14; i++) {
                    bitrate_count[i]=0;
                }
                bitrate_count[0] = eov->bitrate_channelmode_hist[0][4];
            }
            else {
                for(i=0; i < 14; i++) {
                    bitrate_count[i] = eov->bitrate_channelmode_hist[i + 1][4];
                }
            }
        }
    }
	}


void lame_stereo_mode_hist(const lame_global_flags * gfp, int stmode_count[4]) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            EncResult_t const *const eov = &gfc->ov_enc;
            int     i;

            for(i=0; i < 4; i++) {
                stmode_count[i] = eov->bitrate_channelmode_hist[15][i];
            }
        }
    }
	}



void lame_bitrate_stereo_mode_hist(const lame_global_flags * gfp, int bitrate_stmode_count[14][4]) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            SessionConfig_t const *const cfg = &gfc->cfg;
            EncResult_t const *const eov = &gfc->ov_enc;
            int     i;
            int     j;

            if(cfg->free_format) {
                for(j=0; j < 14; j++)
                    for(i=0; i < 4; i++) {
                        bitrate_stmode_count[j][i]=0;
                    }
                for(i=0; i < 4; i++) {
                    bitrate_stmode_count[0][i] = eov->bitrate_channelmode_hist[0][i];
                }
            }
            else {
                for(j=0; j < 14; j++) {
                    for(i=0; i < 4; i++) {
                        bitrate_stmode_count[j][i] = eov->bitrate_channelmode_hist[j + 1][i];
                    }
                }
            }
        }
    }
	}


void lame_block_type_hist(const lame_global_flags * gfp, int btype_count[6]) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            EncResult_t const *const eov = &gfc->ov_enc;
            int     i;

            for(i=0; i < 6; ++i) {
                btype_count[i] = eov->bitrate_blocktype_hist[15][i];
            }
        }
    }
	}


void lame_bitrate_block_type_hist(const lame_global_flags * gfp, int bitrate_btype_count[14][6]) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            SessionConfig_t const *const cfg = &gfc->cfg;
            EncResult_t const *const eov = &gfc->ov_enc;
            int     i, j;

            if(cfg->free_format) {
                for(j=0; j < 14; ++j) {
                    for(i=0; i < 6; ++i) {
                        bitrate_btype_count[j][i] = 0;
                    }
                }
                for(i=0; i < 6; ++i) {
                    bitrate_btype_count[0][i] = eov->bitrate_blocktype_hist[0][i];
                }
            }
            else {
                for(j=0; j < 14; ++j) {
                    for(i=0; i < 6; ++i) {
                        bitrate_btype_count[j][i] = eov->bitrate_blocktype_hist[j + 1][i];
                    }
                }
            }
        }
    }
	}

/* end of lame.c */



/*
 *      MP3 window subband -> subband filtering -> mdct routine
 *
 *      Copyright (c) 1999-2000 Takehiro Tominaga
 *
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

/*
 *         Special Thanks to Patrick De Smet for your advices.
 */

/* $Id: newmdct.c,v 1.39 2011/05/07 16:05:17 rbrito Exp $ */



#ifndef USE_GOGO_SUBBAND
static const FLOAT enwindow[] = {
    -4.77e-07 * 0.740951125354959 / 2.384e-06, 1.03951e-04 * 0.740951125354959 / 2.384e-06,
    9.53674e-04 * 0.740951125354959 / 2.384e-06, 2.841473e-03 * 0.740951125354959 / 2.384e-06,
    3.5758972e-02 * 0.740951125354959 / 2.384e-06, 3.401756e-03 * 0.740951125354959 / 2.384e-06, 9.83715e-04 * 0.740951125354959 / 2.384e-06, 9.9182e-05 * 0.740951125354959 / 2.384e-06, /* 15 */
    1.2398e-05 * 0.740951125354959 / 2.384e-06, 1.91212e-04 * 0.740951125354959 / 2.384e-06,
    2.283096e-03 * 0.740951125354959 / 2.384e-06, 1.6994476e-02 * 0.740951125354959 / 2.384e-06,
    -1.8756866e-02 * 0.740951125354959 / 2.384e-06, -2.630711e-03 * 0.740951125354959 / 2.384e-06,
    -2.47478e-04 * 0.740951125354959 / 2.384e-06, -1.4782e-05 * 0.740951125354959 / 2.384e-06,
    9.063471690191471e-01,
    1.960342806591213e-01,


    -4.77e-07 * 0.773010453362737 / 2.384e-06, 1.05858e-04 * 0.773010453362737 / 2.384e-06,
    9.30786e-04 * 0.773010453362737 / 2.384e-06, 2.521515e-03 * 0.773010453362737 / 2.384e-06,
    3.5694122e-02 * 0.773010453362737 / 2.384e-06, 3.643036e-03 * 0.773010453362737 / 2.384e-06, 9.91821e-04 * 0.773010453362737 / 2.384e-06, 9.6321e-05 * 0.773010453362737 / 2.384e-06, /* 14 */
    1.1444e-05 * 0.773010453362737 / 2.384e-06, 1.65462e-04 * 0.773010453362737 / 2.384e-06,
    2.110004e-03 * 0.773010453362737 / 2.384e-06, 1.6112804e-02 * 0.773010453362737 / 2.384e-06,
    -1.9634247e-02 * 0.773010453362737 / 2.384e-06, -2.803326e-03 * 0.773010453362737 / 2.384e-06,
    -2.77042e-04 * 0.773010453362737 / 2.384e-06, -1.6689e-05 * 0.773010453362737 / 2.384e-06,
    8.206787908286602e-01,
    3.901806440322567e-01,


    -4.77e-07 * 0.803207531480645 / 2.384e-06, 1.07288e-04 * 0.803207531480645 / 2.384e-06,
    9.02653e-04 * 0.803207531480645 / 2.384e-06, 2.174854e-03 * 0.803207531480645 / 2.384e-06,
    3.5586357e-02 * 0.803207531480645 / 2.384e-06, 3.858566e-03 * 0.803207531480645 / 2.384e-06, 9.95159e-04 * 0.803207531480645 / 2.384e-06, 9.3460e-05 * 0.803207531480645 / 2.384e-06, /* 13 */
    1.0014e-05 * 0.803207531480645 / 2.384e-06, 1.40190e-04 * 0.803207531480645 / 2.384e-06,
    1.937389e-03 * 0.803207531480645 / 2.384e-06, 1.5233517e-02 * 0.803207531480645 / 2.384e-06,
    -2.0506859e-02 * 0.803207531480645 / 2.384e-06, -2.974033e-03 * 0.803207531480645 / 2.384e-06,
    -3.07560e-04 * 0.803207531480645 / 2.384e-06, -1.8120e-05 * 0.803207531480645 / 2.384e-06,
    7.416505462720353e-01,
    5.805693545089249e-01,


    -4.77e-07 * 0.831469612302545 / 2.384e-06, 1.08242e-04 * 0.831469612302545 / 2.384e-06,
    8.68797e-04 * 0.831469612302545 / 2.384e-06, 1.800537e-03 * 0.831469612302545 / 2.384e-06,
    3.5435200e-02 * 0.831469612302545 / 2.384e-06, 4.049301e-03 * 0.831469612302545 / 2.384e-06, 9.94205e-04 * 0.831469612302545 / 2.384e-06, 9.0599e-05 * 0.831469612302545 / 2.384e-06, /* 12 */
    9.060e-06 * 0.831469612302545 / 2.384e-06, 1.16348e-04 * 0.831469612302545 / 2.384e-06,
    1.766682e-03 * 0.831469612302545 / 2.384e-06, 1.4358521e-02 * 0.831469612302545 / 2.384e-06,
    -2.1372318e-02 * 0.831469612302545 / 2.384e-06, -3.14188e-03 * 0.831469612302545 / 2.384e-06,
    -3.39031e-04 * 0.831469612302545 / 2.384e-06, -1.9550e-05 * 0.831469612302545 / 2.384e-06,
    6.681786379192989e-01,
    7.653668647301797e-01,


    -4.77e-07 * 0.857728610000272 / 2.384e-06, 1.08719e-04 * 0.857728610000272 / 2.384e-06,
    8.29220e-04 * 0.857728610000272 / 2.384e-06, 1.399517e-03 * 0.857728610000272 / 2.384e-06,
    3.5242081e-02 * 0.857728610000272 / 2.384e-06, 4.215240e-03 * 0.857728610000272 / 2.384e-06, 9.89437e-04 * 0.857728610000272 / 2.384e-06, 8.7261e-05 * 0.857728610000272 / 2.384e-06, /* 11 */
    8.106e-06 * 0.857728610000272 / 2.384e-06, 9.3937e-05 * 0.857728610000272 / 2.384e-06,
    1.597881e-03 * 0.857728610000272 / 2.384e-06, 1.3489246e-02 * 0.857728610000272 / 2.384e-06,
    -2.2228718e-02 * 0.857728610000272 / 2.384e-06, -3.306866e-03 * 0.857728610000272 / 2.384e-06,
    -3.71456e-04 * 0.857728610000272 / 2.384e-06, -2.1458e-05 * 0.857728610000272 / 2.384e-06,
    5.993769336819237e-01,
    9.427934736519954e-01,


    -4.77e-07 * 0.881921264348355 / 2.384e-06, 1.08719e-04 * 0.881921264348355 / 2.384e-06,
    7.8392e-04 * 0.881921264348355 / 2.384e-06, 9.71317e-04 * 0.881921264348355 / 2.384e-06,
    3.5007000e-02 * 0.881921264348355 / 2.384e-06, 4.357815e-03 * 0.881921264348355 / 2.384e-06, 9.80854e-04 * 0.881921264348355 / 2.384e-06, 8.3923e-05 * 0.881921264348355 / 2.384e-06, /* 10 */
    7.629e-06 * 0.881921264348355 / 2.384e-06, 7.2956e-05 * 0.881921264348355 / 2.384e-06,
    1.432419e-03 * 0.881921264348355 / 2.384e-06, 1.2627602e-02 * 0.881921264348355 / 2.384e-06,
    -2.3074150e-02 * 0.881921264348355 / 2.384e-06, -3.467083e-03 * 0.881921264348355 / 2.384e-06,
    -4.04358e-04 * 0.881921264348355 / 2.384e-06, -2.3365e-05 * 0.881921264348355 / 2.384e-06,
    5.345111359507916e-01,
    1.111140466039205e+00,


    -9.54e-07 * 0.903989293123443 / 2.384e-06, 1.08242e-04 * 0.903989293123443 / 2.384e-06,
    7.31945e-04 * 0.903989293123443 / 2.384e-06, 5.15938e-04 * 0.903989293123443 / 2.384e-06,
    3.4730434e-02 * 0.903989293123443 / 2.384e-06, 4.477024e-03 * 0.903989293123443 / 2.384e-06, 9.68933e-04 * 0.903989293123443 / 2.384e-06, 8.0585e-05 * 0.903989293123443 / 2.384e-06, /* 9 */
    6.676e-06 * 0.903989293123443 / 2.384e-06, 5.2929e-05 * 0.903989293123443 / 2.384e-06,
    1.269817e-03 * 0.903989293123443 / 2.384e-06, 1.1775017e-02 * 0.903989293123443 / 2.384e-06,
    -2.3907185e-02 * 0.903989293123443 / 2.384e-06, -3.622532e-03 * 0.903989293123443 / 2.384e-06,
    -4.38213e-04 * 0.903989293123443 / 2.384e-06, -2.5272e-05 * 0.903989293123443 / 2.384e-06,
    4.729647758913199e-01,
    1.268786568327291e+00,


    -9.54e-07 * 0.92387953251128675613 / 2.384e-06,
    1.06812e-04 * 0.92387953251128675613 / 2.384e-06,
    6.74248e-04 * 0.92387953251128675613 / 2.384e-06,
    3.3379e-05 * 0.92387953251128675613 / 2.384e-06,
    3.4412861e-02 * 0.92387953251128675613 / 2.384e-06,
    4.573822e-03 * 0.92387953251128675613 / 2.384e-06,
    9.54151e-04 * 0.92387953251128675613 / 2.384e-06,
    7.6771e-05 * 0.92387953251128675613 / 2.384e-06,
    6.199e-06 * 0.92387953251128675613 / 2.384e-06, 3.4332e-05 * 0.92387953251128675613 / 2.384e-06,
    1.111031e-03 * 0.92387953251128675613 / 2.384e-06,
    1.0933399e-02 * 0.92387953251128675613 / 2.384e-06,
    -2.4725437e-02 * 0.92387953251128675613 / 2.384e-06,
    -3.771782e-03 * 0.92387953251128675613 / 2.384e-06,
    -4.72546e-04 * 0.92387953251128675613 / 2.384e-06,
    -2.7657e-05 * 0.92387953251128675613 / 2.384e-06,
    4.1421356237309504879e-01, /* tan(PI/8) */
    1.414213562373095e+00,


    -9.54e-07 * 0.941544065183021 / 2.384e-06, 1.05381e-04 * 0.941544065183021 / 2.384e-06,
    6.10352e-04 * 0.941544065183021 / 2.384e-06, -4.75883e-04 * 0.941544065183021 / 2.384e-06,
    3.4055710e-02 * 0.941544065183021 / 2.384e-06, 4.649162e-03 * 0.941544065183021 / 2.384e-06, 9.35555e-04 * 0.941544065183021 / 2.384e-06, 7.3433e-05 * 0.941544065183021 / 2.384e-06, /* 7 */
    5.245e-06 * 0.941544065183021 / 2.384e-06, 1.7166e-05 * 0.941544065183021 / 2.384e-06,
    9.56535e-04 * 0.941544065183021 / 2.384e-06, 1.0103703e-02 * 0.941544065183021 / 2.384e-06,
    -2.5527000e-02 * 0.941544065183021 / 2.384e-06, -3.914356e-03 * 0.941544065183021 / 2.384e-06,
    -5.07355e-04 * 0.941544065183021 / 2.384e-06, -3.0041e-05 * 0.941544065183021 / 2.384e-06,
    3.578057213145241e-01,
    1.546020906725474e+00,


    -9.54e-07 * 0.956940335732209 / 2.384e-06, 1.02520e-04 * 0.956940335732209 / 2.384e-06,
    5.39303e-04 * 0.956940335732209 / 2.384e-06, -1.011848e-03 * 0.956940335732209 / 2.384e-06,
    3.3659935e-02 * 0.956940335732209 / 2.384e-06, 4.703045e-03 * 0.956940335732209 / 2.384e-06, 9.15051e-04 * 0.956940335732209 / 2.384e-06, 7.0095e-05 * 0.956940335732209 / 2.384e-06, /* 6 */
    4.768e-06 * 0.956940335732209 / 2.384e-06, 9.54e-07 * 0.956940335732209 / 2.384e-06,
    8.06808e-04 * 0.956940335732209 / 2.384e-06, 9.287834e-03 * 0.956940335732209 / 2.384e-06,
    -2.6310921e-02 * 0.956940335732209 / 2.384e-06, -4.048824e-03 * 0.956940335732209 / 2.384e-06,
    -5.42164e-04 * 0.956940335732209 / 2.384e-06, -3.2425e-05 * 0.956940335732209 / 2.384e-06,
    3.033466836073424e-01,
    1.662939224605090e+00,


    -1.431e-06 * 0.970031253194544 / 2.384e-06, 9.9182e-05 * 0.970031253194544 / 2.384e-06,
    4.62532e-04 * 0.970031253194544 / 2.384e-06, -1.573563e-03 * 0.970031253194544 / 2.384e-06,
    3.3225536e-02 * 0.970031253194544 / 2.384e-06, 4.737377e-03 * 0.970031253194544 / 2.384e-06, 8.91685e-04 * 0.970031253194544 / 2.384e-06, 6.6280e-05 * 0.970031253194544 / 2.384e-06, /* 5 */
    4.292e-06 * 0.970031253194544 / 2.384e-06, -1.3828e-05 * 0.970031253194544 / 2.384e-06,
    6.61850e-04 * 0.970031253194544 / 2.384e-06, 8.487225e-03 * 0.970031253194544 / 2.384e-06,
    -2.7073860e-02 * 0.970031253194544 / 2.384e-06, -4.174709e-03 * 0.970031253194544 / 2.384e-06,
    -5.76973e-04 * 0.970031253194544 / 2.384e-06, -3.4809e-05 * 0.970031253194544 / 2.384e-06,
    2.504869601913055e-01,
    1.763842528696710e+00,


    -1.431e-06 * 0.98078528040323 / 2.384e-06, 9.5367e-05 * 0.98078528040323 / 2.384e-06,
    3.78609e-04 * 0.98078528040323 / 2.384e-06, -2.161503e-03 * 0.98078528040323 / 2.384e-06,
    3.2754898e-02 * 0.98078528040323 / 2.384e-06, 4.752159e-03 * 0.98078528040323 / 2.384e-06, 8.66413e-04 * 0.98078528040323 / 2.384e-06, 6.2943e-05 * 0.98078528040323 / 2.384e-06, /* 4 */
    3.815e-06 * 0.98078528040323 / 2.384e-06, -2.718e-05 * 0.98078528040323 / 2.384e-06,
    5.22137e-04 * 0.98078528040323 / 2.384e-06, 7.703304e-03 * 0.98078528040323 / 2.384e-06,
    -2.7815342e-02 * 0.98078528040323 / 2.384e-06, -4.290581e-03 * 0.98078528040323 / 2.384e-06,
    -6.11782e-04 * 0.98078528040323 / 2.384e-06, -3.7670e-05 * 0.98078528040323 / 2.384e-06,
    1.989123673796580e-01,
    1.847759065022573e+00,


    -1.907e-06 * 0.989176509964781 / 2.384e-06, 9.0122e-05 * 0.989176509964781 / 2.384e-06,
    2.88486e-04 * 0.989176509964781 / 2.384e-06, -2.774239e-03 * 0.989176509964781 / 2.384e-06,
    3.2248020e-02 * 0.989176509964781 / 2.384e-06, 4.748821e-03 * 0.989176509964781 / 2.384e-06, 8.38757e-04 * 0.989176509964781 / 2.384e-06, 5.9605e-05 * 0.989176509964781 / 2.384e-06, /* 3 */
    3.338e-06 * 0.989176509964781 / 2.384e-06, -3.9577e-05 * 0.989176509964781 / 2.384e-06,
    3.88145e-04 * 0.989176509964781 / 2.384e-06, 6.937027e-03 * 0.989176509964781 / 2.384e-06,
    -2.8532982e-02 * 0.989176509964781 / 2.384e-06, -4.395962e-03 * 0.989176509964781 / 2.384e-06,
    -6.46591e-04 * 0.989176509964781 / 2.384e-06, -4.0531e-05 * 0.989176509964781 / 2.384e-06,
    1.483359875383474e-01,
    1.913880671464418e+00,


    -1.907e-06 * 0.995184726672197 / 2.384e-06, 8.4400e-05 * 0.995184726672197 / 2.384e-06,
    1.91689e-04 * 0.995184726672197 / 2.384e-06, -3.411293e-03 * 0.995184726672197 / 2.384e-06,
    3.1706810e-02 * 0.995184726672197 / 2.384e-06, 4.728317e-03 * 0.995184726672197 / 2.384e-06,
    8.09669e-04 * 0.995184726672197 / 2.384e-06, 5.579e-05 * 0.995184726672197 / 2.384e-06,
    3.338e-06 * 0.995184726672197 / 2.384e-06, -5.0545e-05 * 0.995184726672197 / 2.384e-06,
    2.59876e-04 * 0.995184726672197 / 2.384e-06, 6.189346e-03 * 0.995184726672197 / 2.384e-06,
    -2.9224873e-02 * 0.995184726672197 / 2.384e-06, -4.489899e-03 * 0.995184726672197 / 2.384e-06,
    -6.80923e-04 * 0.995184726672197 / 2.384e-06, -4.3392e-05 * 0.995184726672197 / 2.384e-06,
    9.849140335716425e-02,
    1.961570560806461e+00,


    -2.384e-06 * 0.998795456205172 / 2.384e-06, 7.7724e-05 * 0.998795456205172 / 2.384e-06,
    8.8215e-05 * 0.998795456205172 / 2.384e-06, -4.072189e-03 * 0.998795456205172 / 2.384e-06,
    3.1132698e-02 * 0.998795456205172 / 2.384e-06, 4.691124e-03 * 0.998795456205172 / 2.384e-06,
    7.79152e-04 * 0.998795456205172 / 2.384e-06, 5.2929e-05 * 0.998795456205172 / 2.384e-06,
    2.861e-06 * 0.998795456205172 / 2.384e-06, -6.0558e-05 * 0.998795456205172 / 2.384e-06,
    1.37329e-04 * 0.998795456205172 / 2.384e-06, 5.462170e-03 * 0.998795456205172 / 2.384e-06,
    -2.9890060e-02 * 0.998795456205172 / 2.384e-06, -4.570484e-03 * 0.998795456205172 / 2.384e-06,
    -7.14302e-04 * 0.998795456205172 / 2.384e-06, -4.6253e-05 * 0.998795456205172 / 2.384e-06,
    4.912684976946725e-02,
    1.990369453344394e+00,


    3.5780907e-02 * SQRT2 * 0.5 / 2.384e-06, 1.7876148e-02 * SQRT2 * 0.5 / 2.384e-06,
    3.134727e-03 * SQRT2 * 0.5 / 2.384e-06, 2.457142e-03 * SQRT2 * 0.5 / 2.384e-06,
    9.71317e-04 * SQRT2 * 0.5 / 2.384e-06, 2.18868e-04 * SQRT2 * 0.5 / 2.384e-06,
    1.01566e-04 * SQRT2 * 0.5 / 2.384e-06, 1.3828e-05 * SQRT2 * 0.5 / 2.384e-06,

    3.0526638e-02 / 2.384e-06, 4.638195e-03 / 2.384e-06, 7.47204e-04 / 2.384e-06,
    4.9591e-05 / 2.384e-06,
    4.756451e-03 / 2.384e-06, 2.1458e-05 / 2.384e-06, -6.9618e-05 / 2.384e-06, /*    2.384e-06/2.384e-06 */
};
#endif


#define NS 12
#define NL 36

static const FLOAT win[4][NL] = {
    {
     2.382191739347913e-13,
     6.423305872147834e-13,
     9.400849094049688e-13,
     1.122435026096556e-12,
     1.183840321267481e-12,
     1.122435026096556e-12,
     9.400849094049690e-13,
     6.423305872147839e-13,
     2.382191739347918e-13,

     5.456116108943412e-12,
     4.878985199565852e-12,
     4.240448995017367e-12,
     3.559909094758252e-12,
     2.858043359288075e-12,
     2.156177623817898e-12,
     1.475637723558783e-12,
     8.371015190102974e-13,
     2.599706096327376e-13,

     -5.456116108943412e-12,
     -4.878985199565852e-12,
     -4.240448995017367e-12,
     -3.559909094758252e-12,
     -2.858043359288076e-12,
     -2.156177623817898e-12,
     -1.475637723558783e-12,
     -8.371015190102975e-13,
     -2.599706096327376e-13,

     -2.382191739347923e-13,
     -6.423305872147843e-13,
     -9.400849094049696e-13,
     -1.122435026096556e-12,
     -1.183840321267481e-12,
     -1.122435026096556e-12,
     -9.400849094049694e-13,
     -6.423305872147840e-13,
     -2.382191739347918e-13,
     },
    {
     2.382191739347913e-13,
     6.423305872147834e-13,
     9.400849094049688e-13,
     1.122435026096556e-12,
     1.183840321267481e-12,
     1.122435026096556e-12,
     9.400849094049688e-13,
     6.423305872147841e-13,
     2.382191739347918e-13,

     5.456116108943413e-12,
     4.878985199565852e-12,
     4.240448995017367e-12,
     3.559909094758253e-12,
     2.858043359288075e-12,
     2.156177623817898e-12,
     1.475637723558782e-12,
     8.371015190102975e-13,
     2.599706096327376e-13,

     -5.461314069809755e-12,
     -4.921085770524055e-12,
     -4.343405037091838e-12,
     -3.732668368707687e-12,
     -3.093523840190885e-12,
     -2.430835727329465e-12,
     -1.734679010007751e-12,
     -9.748253656609281e-13,
     -2.797435120168326e-13,

     0.000000000000000e+00,
     0.000000000000000e+00,
     0.000000000000000e+00,
     0.000000000000000e+00,
     0.000000000000000e+00,
     0.000000000000000e+00,
     -2.283748241799531e-13,
     -4.037858874020686e-13,
     -2.146547464825323e-13,
     },
    {
     1.316524975873958e-01, /* win[SHORT_TYPE] */
     4.142135623730950e-01,
     7.673269879789602e-01,

     1.091308501069271e+00, /* tantab_l */
     1.303225372841206e+00,
     1.569685577117490e+00,
     1.920982126971166e+00,
     2.414213562373094e+00,
     3.171594802363212e+00,
     4.510708503662055e+00,
     7.595754112725146e+00,
     2.290376554843115e+01,

     0.98480775301220802032, /* cx */
     0.64278760968653936292,
     0.34202014332566882393,
     0.93969262078590842791,
     -0.17364817766693030343,
     -0.76604444311897790243,
     0.86602540378443870761,
     0.500000000000000e+00,

     -5.144957554275265e-01, /* ca */
     -4.717319685649723e-01,
     -3.133774542039019e-01,
     -1.819131996109812e-01,
     -9.457419252642064e-02,
     -4.096558288530405e-02,
     -1.419856857247115e-02,
     -3.699974673760037e-03,

     8.574929257125442e-01, /* cs */
     8.817419973177052e-01,
     9.496286491027329e-01,
     9.833145924917901e-01,
     9.955178160675857e-01,
     9.991605581781475e-01,
     9.998991952444470e-01,
     9.999931550702802e-01,
     },
    {
     0.000000000000000e+00,
     0.000000000000000e+00,
     0.000000000000000e+00,
     0.000000000000000e+00,
     0.000000000000000e+00,
     0.000000000000000e+00,
     2.283748241799531e-13,
     4.037858874020686e-13,
     2.146547464825323e-13,

     5.461314069809755e-12,
     4.921085770524055e-12,
     4.343405037091838e-12,
     3.732668368707687e-12,
     3.093523840190885e-12,
     2.430835727329466e-12,
     1.734679010007751e-12,
     9.748253656609281e-13,
     2.797435120168326e-13,

     -5.456116108943413e-12,
     -4.878985199565852e-12,
     -4.240448995017367e-12,
     -3.559909094758253e-12,
     -2.858043359288075e-12,
     -2.156177623817898e-12,
     -1.475637723558782e-12,
     -8.371015190102975e-13,
     -2.599706096327376e-13,

     -2.382191739347913e-13,
     -6.423305872147834e-13,
     -9.400849094049688e-13,
     -1.122435026096556e-12,
     -1.183840321267481e-12,
     -1.122435026096556e-12,
     -9.400849094049688e-13,
     -6.423305872147841e-13,
     -2.382191739347918e-13,
     }
};

#define tantab_l (win[SHORT_TYPE]+3)
#define cx (win[SHORT_TYPE]+12)
#define ca (win[SHORT_TYPE]+20)
#define cs (win[SHORT_TYPE]+28)

/************************************************************************
*
* window_subband()
*
* PURPOSE:  Overlapping window on PCM samples
*
* SEMANTICS:
* 32 16-bit pcm samples are scaled to fractional 2's complement and
* concatenated to the end of the window buffer #x#. The updated window
* buffer #x# is then windowed by the analysis window #c# to produce the
* windowed sample #z#
*
************************************************************************/

/*
 *      new IDCT routine written by Takehiro TOMINAGA
 */
static const int order[] = {
    0, 1, 16, 17, 8, 9, 24, 25, 4, 5, 20, 21, 12, 13, 28, 29,
    2, 3, 18, 19, 10, 11, 26, 27, 6, 7, 22, 23, 14, 15, 30, 31
};


/* returns sum_j=0^31 a[j]*cos(PI*j*(k+1/2)/32), 0<=k<32 */
inline static void window_subband(const sample_t * x1, FLOAT a[SBLIMIT]) {
    int     i;
    FLOAT const *wp = enwindow + 10;

    const sample_t *x2 = &x1[238 - 14 - 286];

    for(i = -15; i < 0; i++) {
        FLOAT   w, s, t;

        w = wp[-10];
        s = x2[-224] * w;
        t = x1[224] * w;
        w = wp[-9];
        s += x2[-160] * w;
        t += x1[160] * w;
        w = wp[-8];
        s += x2[-96] * w;
        t += x1[96] * w;
        w = wp[-7];
        s += x2[-32] * w;
        t += x1[32] * w;
        w = wp[-6];
        s += x2[32] * w;
        t += x1[-32] * w;
        w = wp[-5];
        s += x2[96] * w;
        t += x1[-96] * w;
        w = wp[-4];
        s += x2[160] * w;
        t += x1[-160] * w;
        w = wp[-3];
        s += x2[224] * w;
        t += x1[-224] * w;

        w = wp[-2];
        s += x1[-256] * w;
        t -= x2[256] * w;
        w = wp[-1];
        s += x1[-192] * w;
        t -= x2[192] * w;
        w = wp[0];
        s += x1[-128] * w;
        t -= x2[128] * w;
        w = wp[1];
        s += x1[-64] * w;
        t -= x2[64] * w;
        w = wp[2];
        s += x1[0] * w;
        t -= x2[0] * w;
        w = wp[3];
        s += x1[64] * w;
        t -= x2[-64] * w;
        w = wp[4];
        s += x1[128] * w;
        t -= x2[-128] * w;
        w = wp[5];
        s += x1[192] * w;
        t -= x2[-192] * w;

        /*
         * this multiplyer could be removed, but it needs more 256 FLOAT data.
         * thinking about the data cache performance, I think we should not
         * use such a huge table. tt 2000/Oct/25
         */
        s *= wp[6];
        w = t - s;
        a[30 + i * 2] = t + s;
        a[31 + i * 2] = wp[7] * w;
        wp += 18;
        x1--;
        x2++;
    }
    {
        FLOAT   s, t, u, v;
        t = x1[-16] * wp[-10];
        s = x1[-32] * wp[-2];
        t += (x1[-48] - x1[16]) * wp[-9];
        s += x1[-96] * wp[-1];
        t += (x1[-80] + x1[48]) * wp[-8];
        s += x1[-160] * wp[0];
        t += (x1[-112] - x1[80]) * wp[-7];
        s += x1[-224] * wp[1];
        t += (x1[-144] + x1[112]) * wp[-6];
        s -= x1[32] * wp[2];
        t += (x1[-176] - x1[144]) * wp[-5];
        s -= x1[96] * wp[3];
        t += (x1[-208] + x1[176]) * wp[-4];
        s -= x1[160] * wp[4];
        t += (x1[-240] - x1[208]) * wp[-3];
        s -= x1[224];

        u = s - t;
        v = s + t;

        t = a[14];
        s = a[15] - t;

        a[31] = v + t;  /* A0 */
        a[30] = u + s;  /* A1 */
        a[15] = u - s;  /* A2 */
        a[14] = v - t;  /* A3 */
    }
    {
        FLOAT   xr;
        xr = a[28] - a[0];
        a[0] += a[28];
        a[28] = xr * wp[-2 * 18 + 7];
        xr = a[29] - a[1];
        a[1] += a[29];
        a[29] = xr * wp[-2 * 18 + 7];

        xr = a[26] - a[2];
        a[2] += a[26];
        a[26] = xr * wp[-4 * 18 + 7];
        xr = a[27] - a[3];
        a[3] += a[27];
        a[27] = xr * wp[-4 * 18 + 7];

        xr = a[24] - a[4];
        a[4] += a[24];
        a[24] = xr * wp[-6 * 18 + 7];
        xr = a[25] - a[5];
        a[5] += a[25];
        a[25] = xr * wp[-6 * 18 + 7];

        xr = a[22] - a[6];
        a[6] += a[22];
        a[22] = xr * SQRT2;
        xr = a[23] - a[7];
        a[7] += a[23];
        a[23] = xr * SQRT2 - a[7];
        a[7] -= a[6];
        a[22] -= a[7];
        a[23] -= a[22];

        xr = a[6];
        a[6] = a[31] - xr;
        a[31] = a[31] + xr;
        xr = a[7];
        a[7] = a[30] - xr;
        a[30] = a[30] + xr;
        xr = a[22];
        a[22] = a[15] - xr;
        a[15] = a[15] + xr;
        xr = a[23];
        a[23] = a[14] - xr;
        a[14] = a[14] + xr;

        xr = a[20] - a[8];
        a[8] += a[20];
        a[20] = xr * wp[-10 * 18 + 7];
        xr = a[21] - a[9];
        a[9] += a[21];
        a[21] = xr * wp[-10 * 18 + 7];

        xr = a[18] - a[10];
        a[10] += a[18];
        a[18] = xr * wp[-12 * 18 + 7];
        xr = a[19] - a[11];
        a[11] += a[19];
        a[19] = xr * wp[-12 * 18 + 7];

        xr = a[16] - a[12];
        a[12] += a[16];
        a[16] = xr * wp[-14 * 18 + 7];
        xr = a[17] - a[13];
        a[13] += a[17];
        a[17] = xr * wp[-14 * 18 + 7];

        xr = -a[20] + a[24];
        a[20] += a[24];
        a[24] = xr * wp[-12 * 18 + 7];
        xr = -a[21] + a[25];
        a[21] += a[25];
        a[25] = xr * wp[-12 * 18 + 7];

        xr = a[4] - a[8];
        a[4] += a[8];
        a[8] = xr * wp[-12 * 18 + 7];
        xr = a[5] - a[9];
        a[5] += a[9];
        a[9] = xr * wp[-12 * 18 + 7];

        xr = a[0] - a[12];
        a[0] += a[12];
        a[12] = xr * wp[-4 * 18 + 7];
        xr = a[1] - a[13];
        a[1] += a[13];
        a[13] = xr * wp[-4 * 18 + 7];
        xr = a[16] - a[28];
        a[16] += a[28];
        a[28] = xr * wp[-4 * 18 + 7];
        xr = -a[17] + a[29];
        a[17] += a[29];
        a[29] = xr * wp[-4 * 18 + 7];

        xr = SQRT2 * (a[2] - a[10]);
        a[2] += a[10];
        a[10] = xr;
        xr = SQRT2 * (a[3] - a[11]);
        a[3] += a[11];
        a[11] = xr;
        xr = SQRT2 * (-a[18] + a[26]);
        a[18] += a[26];
        a[26] = xr - a[18];
        xr = SQRT2 * (-a[19] + a[27]);
        a[19] += a[27];
        a[27] = xr - a[19];

        xr = a[2];
        a[19] -= a[3];
        a[3] -= xr;
        a[2] = a[31] - xr;
        a[31] += xr;
        xr = a[3];
        a[11] -= a[19];
        a[18] -= xr;
        a[3] = a[30] - xr;
        a[30] += xr;
        xr = a[18];
        a[27] -= a[11];
        a[19] -= xr;
        a[18] = a[15] - xr;
        a[15] += xr;

        xr = a[19];
        a[10] -= xr;
        a[19] = a[14] - xr;
        a[14] += xr;
        xr = a[10];
        a[11] -= xr;
        a[10] = a[23] - xr;
        a[23] += xr;
        xr = a[11];
        a[26] -= xr;
        a[11] = a[22] - xr;
        a[22] += xr;
        xr = a[26];
        a[27] -= xr;
        a[26] = a[7] - xr;
        a[7] += xr;

        xr = a[27];
        a[27] = a[6] - xr;
        a[6] += xr;

        xr = SQRT2 * (a[0] - a[4]);
        a[0] += a[4];
        a[4] = xr;
        xr = SQRT2 * (a[1] - a[5]);
        a[1] += a[5];
        a[5] = xr;
        xr = SQRT2 * (a[16] - a[20]);
        a[16] += a[20];
        a[20] = xr;
        xr = SQRT2 * (a[17] - a[21]);
        a[17] += a[21];
        a[21] = xr;

        xr = -SQRT2 * (a[8] - a[12]);
        a[8] += a[12];
        a[12] = xr - a[8];
        xr = -SQRT2 * (a[9] - a[13]);
        a[9] += a[13];
        a[13] = xr - a[9];
        xr = -SQRT2 * (a[25] - a[29]);
        a[25] += a[29];
        a[29] = xr - a[25];
        xr = -SQRT2 * (a[24] + a[28]);
        a[24] -= a[28];
        a[28] = xr - a[24];

        xr = a[24] - a[16];
        a[24] = xr;
        xr = a[20] - xr;
        a[20] = xr;
        xr = a[28] - xr;
        a[28] = xr;

        xr = a[25] - a[17];
        a[25] = xr;
        xr = a[21] - xr;
        a[21] = xr;
        xr = a[29] - xr;
        a[29] = xr;

        xr = a[17] - a[1];
        a[17] = xr;
        xr = a[9] - xr;
        a[9] = xr;
        xr = a[25] - xr;
        a[25] = xr;
        xr = a[5] - xr;
        a[5] = xr;
        xr = a[21] - xr;
        a[21] = xr;
        xr = a[13] - xr;
        a[13] = xr;
        xr = a[29] - xr;
        a[29] = xr;

        xr = a[1] - a[0];
        a[1] = xr;
        xr = a[16] - xr;
        a[16] = xr;
        xr = a[17] - xr;
        a[17] = xr;
        xr = a[8] - xr;
        a[8] = xr;
        xr = a[9] - xr;
        a[9] = xr;
        xr = a[24] - xr;
        a[24] = xr;
        xr = a[25] - xr;
        a[25] = xr;
        xr = a[4] - xr;
        a[4] = xr;
        xr = a[5] - xr;
        a[5] = xr;
        xr = a[20] - xr;
        a[20] = xr;
        xr = a[21] - xr;
        a[21] = xr;
        xr = a[12] - xr;
        a[12] = xr;
        xr = a[13] - xr;
        a[13] = xr;
        xr = a[28] - xr;
        a[28] = xr;
        xr = a[29] - xr;
        a[29] = xr;

        xr = a[0];
        a[0] += a[31];
        a[31] -= xr;
        xr = a[1];
        a[1] += a[30];
        a[30] -= xr;
        xr = a[16];
        a[16] += a[15];
        a[15] -= xr;
        xr = a[17];
        a[17] += a[14];
        a[14] -= xr;
        xr = a[8];
        a[8] += a[23];
        a[23] -= xr;
        xr = a[9];
        a[9] += a[22];
        a[22] -= xr;
        xr = a[24];
        a[24] += a[7];
        a[7] -= xr;
        xr = a[25];
        a[25] += a[6];
        a[6] -= xr;
        xr = a[4];
        a[4] += a[27];
        a[27] -= xr;
        xr = a[5];
        a[5] += a[26];
        a[26] -= xr;
        xr = a[20];
        a[20] += a[11];
        a[11] -= xr;
        xr = a[21];
        a[21] += a[10];
        a[10] -= xr;
        xr = a[12];
        a[12] += a[19];
        a[19] -= xr;
        xr = a[13];
        a[13] += a[18];
        a[18] -= xr;
        xr = a[28];
        a[28] += a[3];
        a[3] -= xr;
        xr = a[29];
        a[29] += a[2];
        a[2] -= xr;
    }

	}


/*-------------------------------------------------------------------*/
/*                                                                   */
/*   Function: Calculation of the MDCT                               */
/*   In the case of long blocks (type 0,1,3) there are               */
/*   36 coefficents in the time domain and 18 in the frequency       */
/*   domain.                                                         */
/*   In the case of short blocks (type 2) there are 3                */
/*   transformations with short length. This leads to 12 coefficents */
/*   in the time and 6 in the frequency domain. In this case the     */
/*   results are stored side by side in the vector out[].            */
/*                                                                   */
/*   New layer3                                                      */
/*                                                                   */
/*-------------------------------------------------------------------*/

inline static void mdct_short(FLOAT * inout) {
    int     l;

    for(l=0; l < 3; l++) {
        FLOAT   tc0, tc1, tc2, ts0, ts1, ts2;

        ts0 = inout[2 * 3] * win[SHORT_TYPE][0] - inout[5 * 3];
        tc0 = inout[0 * 3] * win[SHORT_TYPE][2] - inout[3 * 3];
        tc1 = ts0 + tc0;
        tc2 = ts0 - tc0;

        ts0 = inout[5 * 3] * win[SHORT_TYPE][0] + inout[2 * 3];
        tc0 = inout[3 * 3] * win[SHORT_TYPE][2] + inout[0 * 3];
        ts1 = ts0 + tc0;
        ts2 = -ts0 + tc0;

        tc0 = (inout[1 * 3] * win[SHORT_TYPE][1] - inout[4 * 3]) * 2.069978111953089e-11; /* tritab_s[1] */
        ts0 = (inout[4 * 3] * win[SHORT_TYPE][1] + inout[1 * 3]) * 2.069978111953089e-11; /* tritab_s[1] */

        inout[3 * 0] = tc1 * 1.907525191737280e-11 /* tritab_s[2] */  + tc0;
        inout[3 * 5] = -ts1 * 1.907525191737280e-11 /* tritab_s[0] */  + ts0;

        tc2 = tc2 * 0.86602540378443870761 * 1.907525191737281e-11 /* tritab_s[2] */ ;
        ts1 = ts1 * 0.5 * 1.907525191737281e-11 + ts0;
        inout[3 * 1] = tc2 - ts1;
        inout[3 * 2] = tc2 + ts1;

        tc1 = tc1 * 0.5 * 1.907525191737281e-11 - tc0;
        ts2 = ts2 * 0.86602540378443870761 * 1.907525191737281e-11 /* tritab_s[0] */ ;
        inout[3 * 3] = tc1 + ts2;
        inout[3 * 4] = tc1 - ts2;

        inout++;
    }
	}

inline static void mdct_long(FLOAT * out, FLOAT const *in) {
    FLOAT   ct, st;

    {
        FLOAT   tc1, tc2, tc3, tc4, ts5, ts6, ts7, ts8;
        /* 1,2, 5,6, 9,10, 13,14, 17 */
        tc1 = in[17] - in[9];
        tc3 = in[15] - in[11];
        tc4 = in[14] - in[12];
        ts5 = in[0] + in[8];
        ts6 = in[1] + in[7];
        ts7 = in[2] + in[6];
        ts8 = in[3] + in[5];

        out[17] = (ts5 + ts7 - ts8) - (ts6 - in[4]);
        st = (ts5 + ts7 - ts8) * cx[7] + (ts6 - in[4]);
        ct = (tc1 - tc3 - tc4) * cx[6];
        out[5] = ct + st;
        out[6] = ct - st;

        tc2 = (in[16] - in[10]) * cx[6];
        ts6 = ts6 * cx[7] + in[4];
        ct = tc1 * cx[0] + tc2 + tc3 * cx[1] + tc4 * cx[2];
        st = -ts5 * cx[4] + ts6 - ts7 * cx[5] + ts8 * cx[3];
        out[1] = ct + st;
        out[2] = ct - st;

        ct = tc1 * cx[1] - tc2 - tc3 * cx[2] + tc4 * cx[0];
        st = -ts5 * cx[5] + ts6 - ts7 * cx[3] + ts8 * cx[4];
        out[9] = ct + st;
        out[10] = ct - st;

        ct = tc1 * cx[2] - tc2 + tc3 * cx[0] - tc4 * cx[1];
        st = ts5 * cx[3] - ts6 + ts7 * cx[4] - ts8 * cx[5];
        out[13] = ct + st;
        out[14] = ct - st;
    }
    {
        FLOAT   ts1, ts2, ts3, ts4, tc5, tc6, tc7, tc8;

        ts1 = in[8] - in[0];
        ts3 = in[6] - in[2];
        ts4 = in[5] - in[3];
        tc5 = in[17] + in[9];
        tc6 = in[16] + in[10];
        tc7 = in[15] + in[11];
        tc8 = in[14] + in[12];

        out[0] = (tc5 + tc7 + tc8) + (tc6 + in[13]);
        ct = (tc5 + tc7 + tc8) * cx[7] - (tc6 + in[13]);
        st = (ts1 - ts3 + ts4) * cx[6];
        out[11] = ct + st;
        out[12] = ct - st;

        ts2 = (in[7] - in[1]) * cx[6];
        tc6 = in[13] - tc6 * cx[7];
        ct = tc5 * cx[3] - tc6 + tc7 * cx[4] + tc8 * cx[5];
        st = ts1 * cx[2] + ts2 + ts3 * cx[0] + ts4 * cx[1];
        out[3] = ct + st;
        out[4] = ct - st;

        ct = -tc5 * cx[5] + tc6 - tc7 * cx[3] - tc8 * cx[4];
        st = ts1 * cx[1] + ts2 - ts3 * cx[2] - ts4 * cx[0];
        out[7] = ct + st;
        out[8] = ct - st;

        ct = -tc5 * cx[4] + tc6 - tc7 * cx[5] - tc8 * cx[3];
        st = ts1 * cx[0] - ts2 + ts3 * cx[1] - ts4 * cx[2];
        out[15] = ct + st;
        out[16] = ct - st;
    }
	}


void mdct_sub48(lame_internal_flags * gfc, const sample_t * w0, const sample_t * w1) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *const esv = &gfc->sv_enc;
    int     gr, k, ch;
    const sample_t *wk;

    wk = w0 + 286;
    /* thinking cache performance, ch->gr loop is better than gr->ch loop */
    for(ch=0; ch < cfg->channels_out; ch++) {
        for(gr=0; gr < cfg->mode_gr; gr++) {
            int     band;
            gr_info *const gi = &(gfc->l3_side.tt[gr][ch]);
            FLOAT  *mdct_enc = gi->xr;
            FLOAT  *samp = esv->sb_sample[ch][1 - gr][0];

            for(k=0; k < 18 / 2; k++) {
                window_subband(wk, samp);
                window_subband(wk + 32, samp + 32);
                samp += 64;
                wk += 64;
                /*
                 * Compensate for inversion in the analysis filter
                 */
                for(band = 1; band < 32; band += 2) {
                    samp[band - 32] *= -1;
                }
            }

            /*
             * Perform imdct of 18 previous subband samples
             * + 18 current subband samples
             */
            for(band=0; band < 32; band++, mdct_enc += 18) {
                int     type = gi->block_type;
                FLOAT const *const band0 = esv->sb_sample[ch][gr][0] + order[band];
                FLOAT  *const band1 = esv->sb_sample[ch][1 - gr][0] + order[band];
                if(gi->mixed_block_flag && band < 2)
                    type = 0;
                if(esv->amp_filter[band] < 1e-12) {
                    memset(mdct_enc, 0, 18 * sizeof(FLOAT));
                }
                else {
                    if(esv->amp_filter[band] < 1.0) {
                        for(k=0; k < 18; k++)
                            band1[k * 32] *= esv->amp_filter[band];
                    }
                    if(type == SHORT_TYPE) {
                        for(k = -NS / 4; k < 0; k++) {
                            FLOAT const w = win[SHORT_TYPE][k + 3];
                            mdct_enc[k * 3 + 9] = band0[(9 + k) * 32] * w - band0[(8 - k) * 32];
                            mdct_enc[k * 3 + 18] = band0[(14 - k) * 32] * w + band0[(15 + k) * 32];
                            mdct_enc[k * 3 + 10] = band0[(15 + k) * 32] * w - band0[(14 - k) * 32];
                            mdct_enc[k * 3 + 19] = band1[(2 - k) * 32] * w + band1[(3 + k) * 32];
                            mdct_enc[k * 3 + 11] = band1[(3 + k) * 32] * w - band1[(2 - k) * 32];
                            mdct_enc[k * 3 + 20] = band1[(8 - k) * 32] * w + band1[(9 + k) * 32];
                        }
                        mdct_short(mdct_enc);
                    }
                    else {
                        FLOAT   work[18];
                        for(k = -NL / 4; k < 0; k++) {
                            FLOAT   a, b;
                            a = win[type][k + 27] * band1[(k + 9) * 32]
                                + win[type][k + 36] * band1[(8 - k) * 32];
                            b = win[type][k + 9] * band0[(k + 9) * 32]
                                - win[type][k + 18] * band0[(8 - k) * 32];
                            work[k + 9] = a - b * tantab_l[k + 9];
                            work[k + 18] = a * tantab_l[k + 9] + b;
                        }

                        mdct_long(mdct_enc, work);
                    }
                }
                /*
                 * Perform aliasing reduction butterfly
                 */
                if(type != SHORT_TYPE && band != 0) {
                    for(k = 7; k >= 0; --k) {
                        FLOAT   bu, bd;
                        bu = mdct_enc[k] * ca[k] + mdct_enc[-1 - k] * cs[k];
                        bd = mdct_enc[k] * cs[k] - mdct_enc[-1 - k] * ca[k];

                        mdct_enc[-1 - k] = bu;
                        mdct_enc[k] = bd;
                    }
                }
            }
        }
        wk = w1 + 286;
        if(cfg->mode_gr == 1) {
            memcpy(esv->sb_sample[ch][0], esv->sb_sample[ch][1], 576 * sizeof(FLOAT));
        }
    }
	}




/*
 * presets.c -- Apply presets
 *
 *	Copyright (c) 2002-2008 Gabriel Bouvigne
 *	Copyright (c) 2007-2012 Robert Hegemann
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



#define SET_OPTION(opt, val, def) if(enforce) \
    lame_set_##opt(gfp, val); \
    else if(!(fabs(lame_get_##opt(gfp) - def) > 0)) \
    lame_set_##opt(gfp, val);

#define SET__OPTION(opt, val, def) if(enforce) \
    lame_set_##opt(gfp, val); \
    else if(!(fabs(lame_get_##opt(gfp) - def) > 0)) \
    lame_set_##opt(gfp, val);

#undef Min
#undef Max

static inline int min_int(int a, int b) {

    if(a < b) {
        return a;
    }
    return b;
	}

static inline int max_int(int a, int b) {

    if(a > b) {
        return a;
    }
    return b;
	}


typedef struct {
    int     vbr_q;
    int     quant_comp;
    int     quant_comp_s;
    int     expY;
    FLOAT   st_lrm;          /*short threshold */
    FLOAT   st_s;
    FLOAT   masking_adj;
    FLOAT   masking_adj_short;
    FLOAT   ath_lower;
    FLOAT   ath_curve;
    FLOAT   ath_sensitivity;
    FLOAT   interch;
    int     safejoint;
    int     sfb21mod;
    FLOAT   msfix;
    FLOAT   minval;
    FLOAT   ath_fixpoint;
} vbr_presets_t;

    /* *INDENT-OFF* */
    
    /* Switch mappings for VBR mode VBR_RH */
    static const vbr_presets_t vbr_old_switch_map[] = {
    /*vbr_q  qcomp_l  qcomp_s  expY  st_lrm   st_s  mask adj_l  adj_s  ath_lower  ath_curve  ath_sens  interChR  safejoint sfb21mod  msfix */
        {0,       9,       9,    0,   5.20, 125.0,      -4.2,   -6.3,       4.8,       1,          0,   0,              2,      21,  0.97, 5, 100},
        {1,       9,       9,    0,   5.30, 125.0,      -3.6,   -5.6,       4.5,       1.5,        0,   0,              2,      21,  1.35, 5, 100},
        {2,       9,       9,    0,   5.60, 125.0,      -2.2,   -3.5,       2.8,       2,          0,   0,              2,      21,  1.49, 5, 100},
        {3,       9,       9,    1,   5.80, 130.0,      -1.8,   -2.8,       2.6,       3,         -4,   0,              2,      20,  1.64, 5, 100},
        {4,       9,       9,    1,   6.00, 135.0,      -0.7,   -1.1,       1.1,       3.5,       -8,   0,              2,       0,  1.79, 5, 100},
        {5,       9,       9,    1,   6.40, 140.0,       0.5,    0.4,      -7.5,       4,        -12,   0.0002,         0,       0,  1.95, 5, 100},
        {6,       9,       9,    1,   6.60, 145.0,       0.67,   0.65,    -14.7,       6.5,      -19,   0.0004,         0,       0,  2.30, 5, 100},
        {7,       9,       9,    1,   6.60, 145.0,       0.8,    0.75,    -19.7,       8,        -22,   0.0006,         0,       0,  2.70, 5, 100},
        {8,       9,       9,    1,   6.60, 145.0,       1.2,    1.15,    -27.5,      10,        -23,   0.0007,         0,       0,  0,    5, 100},
        {9,       9,       9,    1,   6.60, 145.0,       1.6,    1.6,     -36,        11,        -25,   0.0008,         0,       0,  0,    5, 100},
        {10,      9,       9,    1,   6.60, 145.0,       2.0,    2.0,     -36,        12,        -25,   0.0008,         0,       0,  0,    5, 100}
    };
    
    static const vbr_presets_t vbr_mt_psy_switch_map[] = {
    /*vbr_q  qcomp_l  qcomp_s  expY  st_lrm   st_s  mask adj_l  adj_s  ath_lower  ath_curve  ath_sens  ---  safejoint sfb21mod  msfix */
        {0,       9,       9,    0,   4.20,  25.0,      -6.8,   -6.8,       7.1,       1,          0,   0,         2,      31,  1.000,  5, 100},
        {1,       9,       9,    0,   4.20,  25.0,      -4.8,   -4.8,       5.4,       1.4,       -1,   0,         2,      27,  1.122,  5,  98},
        {2,       9,       9,    0,   4.20,  25.0,      -2.6,   -2.6,       3.7,       2.0,       -3,   0,         2,      23,  1.288,  5,  97},
        {3,       9,       9,    1,   4.20,  25.0,      -1.6,   -1.6,       2.0,       2.0,       -5,   0,         2,      18,  1.479,  5,  96},
        {4,       9,       9,    1,   4.20,  25.0,      -0.0,   -0.0,       0.0,       2.0,       -8,   0,         2,      12,  1.698,  5,  95},
        {5,       9,       9,    1,   4.20,  25.0,       1.3,    1.3,      -6,         3.5,      -11,   0,         2,       8,  1.950,  5,  94.2},
#if 0
        {6,       9,       9,    1,   4.50, 100.0,       1.5,    1.5,     -24.0,       6.0,      -14,   0,         2,       4,  2.239,  3,  93.9},
        {7,       9,       9,    1,   4.80, 200.0,       1.7,    1.7,     -28.0,       9.0,      -20,   0,         2,       0,  2.570,  1,  93.6},
#else
        {6,       9,       9,    1,   4.50, 100.0,       2.2,    2.3,     -12.0,       6.0,      -14,   0,         2,       4,  2.239,  3,  93.9},
        {7,       9,       9,    1,   4.80, 200.0,       2.7,    2.7,     -18.0,       9.0,      -17,   0,         2,       0,  2.570,  1,  93.6},
#endif
        {8,       9,       9,    1,   5.30, 300.0,       2.8,    2.8,     -21.0,      10.0,      -23,   0.0002,    0,       0,  2.951,  0,  93.3},
        {9,       9,       9,    1,   6.60, 300.0,       2.8,    2.8,     -23.0,      11.0,      -25,   0.0006,    0,       0,  3.388,  0,  93.3},
        {10,      9,       9,    1,  25.00, 300.0,       2.8,    2.8,     -25.0,      12.0,      -27,   0.0025,    0,       0,  3.500,  0,  93.3}
    };

    /* *INDENT-ON* */

static vbr_presets_t const* get_vbr_preset(int v) {

  switch(v) {
    case vbr_mtrh:
    case vbr_mt:
        return &vbr_mt_psy_switch_map[0];
    default:
        return &vbr_old_switch_map[0];
    }
	}

#define NOOP(m) (void)p.m
#define LERP(m) (p.m = p.m + x * (q.m - p.m))

static void apply_vbr_preset(lame_global_flags * gfp, int a, int enforce) {
    vbr_presets_t const *vbr_preset = get_vbr_preset(lame_get_VBR(gfp));
    float   x = gfp->VBR_q_frac;
    vbr_presets_t p = vbr_preset[a];
    vbr_presets_t q = vbr_preset[a + 1];
    vbr_presets_t const *set = &p;

    NOOP(vbr_q);
    NOOP(quant_comp);
    NOOP(quant_comp_s);
    NOOP(expY);
    LERP(st_lrm);
    LERP(st_s);
    LERP(masking_adj);
    LERP(masking_adj_short);
    LERP(ath_lower);
    LERP(ath_curve);
    LERP(ath_sensitivity);
    LERP(interch);
    NOOP(safejoint);
    LERP(sfb21mod);
    LERP(msfix);
    LERP(minval);
    LERP(ath_fixpoint);

    lame_set_VBR_q(gfp, set->vbr_q);
    SET_OPTION(quant_comp, set->quant_comp, -1);
    SET_OPTION(quant_comp_short, set->quant_comp_s, -1);
    if(set->expY) {
        lame_set_experimentalY(gfp, set->expY);
    }
    SET_OPTION(short_threshold_lrm, set->st_lrm, -1);
    SET_OPTION(short_threshold_s, set->st_s, -1);
    SET_OPTION(maskingadjust, set->masking_adj, 0);
    SET_OPTION(maskingadjust_short, set->masking_adj_short, 0);
    if(lame_get_VBR(gfp) == vbr_mt || lame_get_VBR(gfp) == vbr_mtrh) {
        lame_set_ATHtype(gfp, 5);
    }
    SET_OPTION(ATHlower, set->ath_lower, 0);
    SET_OPTION(ATHcurve, set->ath_curve, -1);
    SET_OPTION(athaa_sensitivity, set->ath_sensitivity, 0);
    if(set->interch > 0) {
        SET_OPTION(interChRatio, set->interch, -1);
    }

    /* parameters for which there is no proper set/get interface */
    if(set->safejoint > 0) {
        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 2);
    }
    if(set->sfb21mod > 0) {
        int const nsp = lame_get_exp_nspsytune(gfp);
        int const val = (nsp >> 20) & 63;
        if(val == 0) {
            int const sf21mod = (set->sfb21mod << 20) | nsp;
            lame_set_exp_nspsytune(gfp, sf21mod);
        }
    }
    SET__OPTION(msfix, set->msfix, -1);

    if(enforce == 0) {
        gfp->VBR_q = a;
        gfp->VBR_q_frac = x;
    }
    gfp->internal_flags->cfg.minval = set->minval;
    {   /* take care of gain adjustments */
        double const x = fabs(gfp->scale);
        double const y = (x > 0.f) ? (10.f * log10(x)) : 0.f;
        gfp->internal_flags->cfg.ATHfixpoint = set->ath_fixpoint - y;
    }
	}

static int apply_abr_preset(lame_global_flags * gfp, int preset, int enforce) {
    typedef struct {
        int     abr_kbps;
        int     quant_comp;
        int     quant_comp_s;
        int     safejoint;
        FLOAT   nsmsfix;
        FLOAT   st_lrm;      /*short threshold */
        FLOAT   st_s;
        FLOAT   scale;
        FLOAT   masking_adj;
        FLOAT   ath_lower;
        FLOAT   ath_curve;
        FLOAT   interch;
        int     sfscale;
    } abr_presets_t;


    /* *INDENT-OFF* */

    /* 
     *  Switch mappings for ABR mode
     */
    const abr_presets_t abr_switch_map[] = {        
    /* kbps  quant q_s safejoint nsmsfix st_lrm  st_s  scale   msk ath_lwr ath_curve  interch , sfscale */
      {  8,     9,  9,        0,      0,  6.60,  145,  0.95,    0,  -30.0,     11,    0.0012,        1}, /*   8, impossible to use in stereo */
      { 16,     9,  9,        0,      0,  6.60,  145,  0.95,    0,  -25.0,     11,    0.0010,        1}, /*  16 */
      { 24,     9,  9,        0,      0,  6.60,  145,  0.95,    0,  -20.0,     11,    0.0010,        1}, /*  24 */
      { 32,     9,  9,        0,      0,  6.60,  145,  0.95,    0,  -15.0,     11,    0.0010,        1}, /*  32 */
      { 40,     9,  9,        0,      0,  6.60,  145,  0.95,    0,  -10.0,     11,    0.0009,        1}, /*  40 */
      { 48,     9,  9,        0,      0,  6.60,  145,  0.95,    0,  -10.0,     11,    0.0009,        1}, /*  48 */
      { 56,     9,  9,        0,      0,  6.60,  145,  0.95,    0,   -6.0,     11,    0.0008,        1}, /*  56 */
      { 64,     9,  9,        0,      0,  6.60,  145,  0.95,    0,   -2.0,     11,    0.0008,        1}, /*  64 */
      { 80,     9,  9,        0,      0,  6.60,  145,  0.95,    0,     .0,      8,    0.0007,        1}, /*  80 */
      { 96,     9,  9,        0,   2.50,  6.60,  145,  0.95,    0,    1.0,      5.5,  0.0006,        1}, /*  96 */
      {112,     9,  9,        0,   2.25,  6.60,  145,  0.95,    0,    2.0,      4.5,  0.0005,        1}, /* 112 */
      {128,     9,  9,        0,   1.95,  6.40,  140,  0.95,    0,    3.0,      4,    0.0002,        1}, /* 128 */
      {160,     9,  9,        1,   1.79,  6.00,  135,  0.95,   -2,    5.0,      3.5,  0,             1}, /* 160 */
      {192,     9,  9,        1,   1.49,  5.60,  125,  0.97,   -4,    7.0,      3,    0,             0}, /* 192 */
      {224,     9,  9,        1,   1.25,  5.20,  125,  0.98,   -6,    9.0,      2,    0,             0}, /* 224 */
      {256,     9,  9,        1,   0.97,  5.20,  125,  1.00,   -8,   10.0,      1,    0,             0}, /* 256 */
      {320,     9,  9,        1,   0.90,  5.20,  125,  1.00,  -10,   12.0,      0,    0,             0}  /* 320 */
    };

    /* *INDENT-ON* */

    /* Variables for the ABR stuff */
    int     r;
    int     actual_bitrate = preset;

    r = nearestBitrateFullIndex(preset);
    
    lame_set_VBR(gfp, vbr_abr);
    lame_set_VBR_mean_bitrate_kbps(gfp, (actual_bitrate));
    lame_set_VBR_mean_bitrate_kbps(gfp, min_int(lame_get_VBR_mean_bitrate_kbps(gfp), 320));
    lame_set_VBR_mean_bitrate_kbps(gfp, max_int(lame_get_VBR_mean_bitrate_kbps(gfp), 8));
    lame_set_brate(gfp, lame_get_VBR_mean_bitrate_kbps(gfp));


    /* parameters for which there is no proper set/get interface */
    if(abr_switch_map[r].safejoint > 0)
        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 2); /* safejoint */

    if(abr_switch_map[r].sfscale > 0)
        lame_set_sfscale(gfp, 1);


    SET_OPTION(quant_comp, abr_switch_map[r].quant_comp, -1);
    SET_OPTION(quant_comp_short, abr_switch_map[r].quant_comp_s, -1);

    SET__OPTION(msfix, abr_switch_map[r].nsmsfix, -1);

    SET_OPTION(short_threshold_lrm, abr_switch_map[r].st_lrm, -1);
    SET_OPTION(short_threshold_s, abr_switch_map[r].st_s, -1);

    /* ABR seems to have big problems with clipping, especially at low bitrates */
    /* so we compensate for that here by using a scale value depending on bitrate */
    lame_set_scale(gfp, lame_get_scale(gfp) * abr_switch_map[r].scale);

    SET_OPTION(maskingadjust, abr_switch_map[r].masking_adj, 0);
    if(abr_switch_map[r].masking_adj > 0) {
        SET_OPTION(maskingadjust_short, abr_switch_map[r].masking_adj * .9, 0);
    }
    else {
        SET_OPTION(maskingadjust_short, abr_switch_map[r].masking_adj * 1.1, 0);
    }


    SET_OPTION(ATHlower, abr_switch_map[r].ath_lower, 0);
    SET_OPTION(ATHcurve, abr_switch_map[r].ath_curve, -1);

    SET_OPTION(interChRatio, abr_switch_map[r].interch, -1);

    abr_switch_map[r].abr_kbps;

    gfp->internal_flags->cfg.minval = 5. * (abr_switch_map[r].abr_kbps / 320.);

    return preset;
	}



int apply_preset(lame_global_flags * gfp, int preset, int enforce) {

    /*translate legacy presets */
    switch(preset) {
    case R3MIX:
        {
            preset = V3;
            lame_set_VBR(gfp, vbr_mtrh);
            break;
        }
    case MEDIUM:
    case MEDIUM_FAST:
        {
            preset = V4;
            lame_set_VBR(gfp, vbr_mtrh);
            break;
        }
    case STANDARD:
    case STANDARD_FAST:
        {
            preset = V2;
            lame_set_VBR(gfp, vbr_mtrh);
            break;
        }
    case EXTREME:
    case EXTREME_FAST:
        {
            preset = V0;
            lame_set_VBR(gfp, vbr_mtrh);
            break;
        }
    case INSANE:
        {
            preset = 320;
            gfp->preset = preset;
            apply_abr_preset(gfp, preset, enforce);
            lame_set_VBR(gfp, vbr_off);
            return preset;
        }
    }

    gfp->preset = preset;
    {
        switch(preset) {
        case V9:
            apply_vbr_preset(gfp, 9, enforce);
            return preset;
        case V8:
            apply_vbr_preset(gfp, 8, enforce);
            return preset;
        case V7:
            apply_vbr_preset(gfp, 7, enforce);
            return preset;
        case V6:
            apply_vbr_preset(gfp, 6, enforce);
            return preset;
        case V5:
            apply_vbr_preset(gfp, 5, enforce);
            return preset;
        case V4:
            apply_vbr_preset(gfp, 4, enforce);
            return preset;
        case V3:
            apply_vbr_preset(gfp, 3, enforce);
            return preset;
        case V2:
            apply_vbr_preset(gfp, 2, enforce);
            return preset;
        case V1:
            apply_vbr_preset(gfp, 1, enforce);
            return preset;
        case V0:
            apply_vbr_preset(gfp, 0, enforce);
            return preset;
        default:
            break;
        }
    }
    if(8 <= preset && preset <= 320) {
        return apply_abr_preset(gfp, preset, enforce);
    }

  gfp->preset = 0;    /*no corresponding preset found */
  return preset;
	}



/*
 *      psymodel.c
 *
 *      Copyright (c) 1999-2000 Mark Taylor
 *      Copyright (c) 2001-2002 Naoki Shibata
 *      Copyright (c) 2000-2003 Takehiro Tominaga
 *      Copyright (c) 2000-2012 Robert Hegemann
 *      Copyright (c) 2000-2005 Gabriel Bouvigne
 *      Copyright (c) 2000-2005 Alexander Leidinger
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

/* $Id: psymodel.c,v 1.216 2017/09/06 19:38:23 aleidinger Exp $ */


/*
PSYCHO ACOUSTICS


This routine computes the psycho acoustics, delayed by one granule.  

Input: buffer of PCM data (1024 samples).  

This window should be centered over the 576 sample granule window.
The routine will compute the psycho acoustics for
this granule, but return the psycho acoustics computed
for the *previous* granule.  This is because the block
type of the previous granule can only be determined
after we have computed the psycho acoustics for the following
granule.  

Output:  maskings and energies for each scalefactor band.
block type, PE, and some correlation measures.  
The PE is used by CBR modes to determine if extra bits
from the bit reservoir should be used.  The correlation
measures are used to determine mid/side or regular stereo.
*/
/*
Notation:

barks:  a non-linear frequency scale.  Mapping from frequency to
        barks is given by freq2bark()

scalefactor bands: The spectrum (frequencies) are broken into 
                   SBMAX "scalefactor bands".  Thes bands
                   are determined by the MPEG ISO spec.  In
                   the noise shaping/quantization code, we allocate
                   bits among the partition bands to achieve the
                   best possible quality

partition bands:   The spectrum is also broken into about
                   64 "partition bands".  Each partition 
                   band is about .34 barks wide.  There are about 2-5
                   partition bands for each scalefactor band.

LAME computes all psycho acoustic information for each partition
band.  Then at the end of the computations, this information
is mapped to scalefactor bands.  The energy in each scalefactor
band is taken as the sum of the energy in all partition bands
which overlap the scalefactor band.  The maskings can be computed
in the same way (and thus represent the average masking in that band)
or by taking the minmum value multiplied by the number of
partition bands used (which represents a minimum masking in that band).
*/
/*
The general outline is as follows:

1. compute the energy in each partition band
2. compute the tonality in each partition band
3. compute the strength of each partion band "masker"
4. compute the masking (via the spreading function applied to each masker)
5. Modifications for mid/side masking.  

Each partition band is considiered a "masker".  The strength
of the i'th masker in band j is given by:

    s3(bark(i)-bark(j))*strength(i)

The strength of the masker is a function of the energy and tonality.
The more tonal, the less masking.  LAME uses a simple linear formula
(controlled by NMT and TMN) which says the strength is given by the
energy divided by a linear function of the tonality.
*/
/*
s3() is the "spreading function".  It is given by a formula
determined via listening tests.  

The total masking in the j'th partition band is the sum over
all maskings i.  It is thus given by the convolution of
the strength with s3(), the "spreading function."

masking(j) = sum_over_i  s3(i-j)*strength(i)  = s3 o strength

where "o" = convolution operator.  s3 is given by a formula determined
via listening tests.  It is normalized so that s3 o 1 = 1.

Note: instead of a simple convolution, LAME also has the
option of using "additive masking"

The most critical part is step 2, computing the tonality of each
partition band.  LAME has two tonality estimators.  The first
is based on the ISO spec, and measures how predictiable the
signal is over time.  The more predictable, the more tonal.
The second measure is based on looking at the spectrum of
a single granule.  The more peaky the spectrum, the more
tonal.  By most indications, the latter approach is better.

Finally, in step 5, the maskings for the mid and side
channel are possibly increased.  Under certain circumstances,
noise in the mid & side channels is assumed to also
be masked by strong maskers in the L or R channels.


Other data computed by the psy-model:

ms_ratio        side-channel / mid-channel masking ratio (for previous granule)
ms_ratio_next   side-channel / mid-channel masking ratio for this granule

percep_entropy[2]     L and R values (prev granule) of PE - A measure of how 
                      much pre-echo is in the previous granule
percep_entropy_MS[2]  mid and side channel values (prev granule) of percep_entropy
energy[4]             L,R,M,S energy in each channel, prev granule
blocktype_d[2]        block type to use for previous granule
*/




#define NSFIRLEN 21

#ifdef M_LN10
#define  LN_TO_LOG10  (M_LN10/10)
#else
#define  LN_TO_LOG10  0.2302585093
#endif


/*
   L3psycho_anal.  Compute psycho acoustics.

   Data returned to the calling program must be delayed by one 
   granule. 

   This is done in two places.  
   If we do not need to know the blocktype, the copying
   can be done here at the top of the program: we copy the data for
   the last granule (computed during the last call) before it is
   overwritten with the new data.  It looks like this:
  
   0. static psymodel_data 
   1. calling_program_data = psymodel_data
   2. compute psymodel_data
    
   For data which needs to know the blocktype, the copying must be
   done at the end of this loop, and the old values must be saved:
   
   0. static psymodel_data_old 
   1. compute psymodel_data
   2. compute possible block type of this granule
   3. compute final block type of previous granule based on #2.
   4. calling_program_data = psymodel_data_old
   5. psymodel_data_old = psymodel_data
*/





/* psycho_loudness_approx
   jd - 2001 mar 12
in:  energy   - BLKSIZE/2 elements of frequency magnitudes ^ 2
     gfp      - uses out_samplerate, ATHtype (also needed for ATHformula)
returns: loudness^2 approximation, a positive value roughly tuned for a value
         of 1.0 for signals near clipping.
notes:   When calibrated, feeding this function binary white noise at sample
         values +32767 or -32768 should return values that approach 3.
         ATHformula is used to approximate an equal loudness curve.
future:  Data indicates that the shape of the equal loudness curve varies
         with intensity.  This function might be improved by using an equal
         loudness curve shaped for typical playback levels (instead of the
         ATH, that is shaped for the threshold).  A flexible realization might
         simply bend the existing ATH curve to achieve the desired shape.
         However, the potential gain may not be enough to justify an effort.
*/
static  FLOAT psycho_loudness_approx(FLOAT const *energy, FLOAT const *eql_w) {
    int     i;
    FLOAT   loudness_power;

    loudness_power = 0.0;
    /* apply weights to power in freq. bands */
    for(i=0; i < BLKSIZE / 2; ++i)
        loudness_power += energy[i] * eql_w[i];
    loudness_power *= VO_SCALE;

    return loudness_power;
	}

/* mask_add optimization */
/* init the limit values used to avoid computing log in mask_add when it is not necessary */

/* For example, with i = 10*log10(m2/m1)/10*16         (= log10(m2/m1)*16)
 *
 * abs(i)>8 is equivalent (as i is an integer) to
 * abs(i)>=9
 * i>=9 || i<=-9
 * equivalent to (as i is the biggest integer smaller than log10(m2/m1)*16 
 * or the smallest integer bigger than log10(m2/m1)*16 depending on the sign of log10(m2/m1)*16)
 * log10(m2/m1)>=9/16 || log10(m2/m1)<=-9/16
 * exp10 is strictly increasing thus this is equivalent to
 * m2/m1 >= 10^(9/16) || m2/m1<=10^(-9/16) which are comparisons to constants
 */


#define I1LIMIT 8       /* as in if(i>8)  */
#define I2LIMIT 23      /* as in if(i>24) -> changed 23 */
#define MLIMIT  15      /* as in if(m<15) */

/* pow(10, (I1LIMIT + 1) / 16.0); */
static const FLOAT ma_max_i1 = 3.6517412725483771;
/* pow(10, (I2LIMIT + 1) / 16.0); */
static const FLOAT ma_max_i2 = 31.622776601683793;
/* pow(10, (MLIMIT) / 10.0); */
static const FLOAT ma_max_m  = 31.622776601683793;

    /*This is the masking table:
       According to tonality, values are going from 0dB (TMN)
       to 9.3dB (NMT).
       After additive masking computation, 8dB are added, so
       final values are going from 8dB to 17.3dB
     */
static const FLOAT tab[] = {
    1.0 /*pow(10, -0) */ ,
    0.79433 /*pow(10, -0.1) */ ,
    0.63096 /*pow(10, -0.2) */ ,
    0.63096 /*pow(10, -0.2) */ ,
    0.63096 /*pow(10, -0.2) */ ,
    0.63096 /*pow(10, -0.2) */ ,
    0.63096 /*pow(10, -0.2) */ ,
    0.25119 /*pow(10, -0.6) */ ,
    0.11749             /*pow(10, -0.93) */
};

static const int tab_mask_add_delta[] = { 2, 2, 2, 1, 1, 1, 0, 0, -1 };
#define STATIC_ASSERT_EQUAL_DIMENSION(A,B) enum{static_assert_##A=1/((dimension_of(A) == dimension_of(B))?1:0)}

inline static int mask_add_delta(int i) {
    STATIC_ASSERT_EQUAL_DIMENSION(tab_mask_add_delta,tab);
    assert(i < (int)dimension_of(tab));
    return tab_mask_add_delta[i];
}


static void init_mask_add_max_values(void) {

#ifndef NDEBUG
    FLOAT const _ma_max_i1 = pow(10, (I1LIMIT + 1) / 16.0);
    FLOAT const _ma_max_i2 = pow(10, (I2LIMIT + 1) / 16.0);
    FLOAT const _ma_max_m = pow(10, (MLIMIT) / 10.0);
    assert(fabs(ma_max_i1 - _ma_max_i1) <= FLT_EPSILON);
    assert(fabs(ma_max_i2 - _ma_max_i2) <= FLT_EPSILON);
    assert(fabs(ma_max_m  - _ma_max_m ) <= FLT_EPSILON);
#endif
	}




/* addition of simultaneous masking   Naoki Shibata 2000/7 */
inline static FLOAT vbrpsy_mask_add(FLOAT m1, FLOAT m2, int b, int delta) {
    static const FLOAT table2[] = {
        1.33352 * 1.33352, 1.35879 * 1.35879, 1.38454 * 1.38454, 1.39497 * 1.39497,
        1.40548 * 1.40548, 1.3537 * 1.3537, 1.30382 * 1.30382, 1.22321 * 1.22321,
        1.14758 * 1.14758,
        1
    };

    FLOAT   ratio;

    if(m1 < 0) {
        m1 = 0;
    }
    if(m2 < 0) {
        m2 = 0;
    }
    if(m1 <= 0) {
        return m2;
    }
    if(m2 <= 0) {
        return m1;
    }
    if(m2 > m1) {
        ratio = m2 / m1;
    }
    else {
        ratio = m1 / m2;
    }
    if(abs(b) <= delta) {       /* approximately, 1 bark = 3 partitions */
        /* originally 'if(i > 8)' */
        if(ratio >= ma_max_i1) {
            return m1 + m2;
        }
        else {
            int     i = (int) (FAST_LOG10_X(ratio, 16.0f));
            return (m1 + m2) * table2[i];
        }
    }
    if(ratio < ma_max_i2) {
        return m1 + m2;
    }
    if(m1 < m2) {
        m1 = m2;
    }
  return m1;
	}


/* short block threshold calculation (part 2)

    partition band bo_s[sfb] is at the transition from scalefactor
    band sfb to the next one sfb+1; enn and thmm have to be split
    between them
*/
static void convert_partition2scalefac(PsyConst_CB2SB_t const *const gd, FLOAT const *eb, FLOAT const *thr,
                           FLOAT enn_out[], FLOAT thm_out[]) {
    FLOAT   enn, thmm;
    int     sb, b, n = gd->n_sb;

    enn = thmm = 0.0f;
    for(sb = b = 0; sb < n; ++b, ++sb) {
        int const bo_sb = gd->bo[sb];
        int const npart = gd->npart;
        int const b_lim = bo_sb < npart ? bo_sb : npart;
        while(b < b_lim) {
            assert(eb[b] >= 0); /* iff failed, it may indicate some index error elsewhere */
            assert(thr[b] >= 0);
            enn += eb[b];
            thmm += thr[b];
            b++;
        }
        if(b >= npart) {
            enn_out[sb] = enn;
            thm_out[sb] = thmm;
            ++sb;
            break;
        }
        assert(eb[b] >= 0); /* iff failed, it may indicate some index error elsewhere */
        assert(thr[b] >= 0);
        {
            /* at transition sfb -> sfb+1 */
            FLOAT const w_curr = gd->bo_weight[sb];
            FLOAT const w_next = 1.0f - w_curr;
            enn += w_curr * eb[b];
            thmm += w_curr * thr[b];
            enn_out[sb] = enn;
            thm_out[sb] = thmm;
            enn = w_next * eb[b];
            thmm = w_next * thr[b];
        }
    }
    /* zero initialize the rest */
    for(; sb < n; ++sb) {
        enn_out[sb] = 0;
        thm_out[sb] = 0;
    }
	}

static void convert_partition2scalefac_s(lame_internal_flags * gfc, FLOAT const *eb, FLOAT const *thr, int chn,
                             int sblock) {
    PsyStateVar_t *const psv = &gfc->sv_psy;
    PsyConst_CB2SB_t const *const gds = &gfc->cd_psy->s;
    FLOAT   enn[SBMAX_s], thm[SBMAX_s];
    int     sb;

    convert_partition2scalefac(gds, eb, thr, enn, thm);
    for(sb=0; sb < SBMAX_s; ++sb) {
        psv->en[chn].s[sb][sblock] = enn[sb];
        psv->thm[chn].s[sb][sblock] = thm[sb];
    }
	}

/* longblock threshold calculation (part 2) */
static void convert_partition2scalefac_l(lame_internal_flags * gfc, FLOAT const *eb, FLOAT const *thr, int chn) {
    PsyStateVar_t *const psv = &gfc->sv_psy;
    PsyConst_CB2SB_t const *const gdl = &gfc->cd_psy->l;
    FLOAT  *enn = &psv->en[chn].l[0];
    FLOAT  *thm = &psv->thm[chn].l[0];

    convert_partition2scalefac(gdl, eb, thr, enn, thm);
	}

static void convert_partition2scalefac_l_to_s(lame_internal_flags * gfc, FLOAT const *eb, FLOAT const *thr,
                                  int chn) {
    PsyStateVar_t *const psv = &gfc->sv_psy;
    PsyConst_CB2SB_t const *const gds = &gfc->cd_psy->l_to_s;
    FLOAT   enn[SBMAX_s], thm[SBMAX_s];
    int     sb, sblock;

    convert_partition2scalefac(gds, eb, thr, enn, thm);
    for(sb=0; sb < SBMAX_s; ++sb) {
        FLOAT const scale = 1. / 64.f;
        FLOAT const tmp_enn = enn[sb];
        FLOAT const tmp_thm = thm[sb] * scale;
        for(sblock = 0; sblock < 3; ++sblock) {
            psv->en[chn].s[sb][sblock] = tmp_enn;
            psv->thm[chn].s[sb][sblock] = tmp_thm;
        }
    }
	}


static inline FLOAT NS_INTERP(FLOAT x, FLOAT y, FLOAT r) {

    /* was pow((x),(r))*pow((y),1-(r)) */
    if(r >= 1.0f)
        return x;       /* 99.7% of the time */
    if(r <= 0.0f)
        return y;
    if(y > 0.0f)
        return powf(x / y, r) * y; /* rest of the time */
    return 0.0f;        /* never happens */
	}


static  FLOAT pecalc_s(III_psy_ratio const *mr, FLOAT masking_lower) {
    FLOAT   pe_s;
    static const FLOAT regcoef_s[] = {
        11.8,           /* these values are tuned only for 44.1kHz... */
        13.6,
        17.2,
        32,
        46.5,
        51.3,
        57.5,
        67.1,
        71.5,
        84.6,
        97.6,
        130,
/*      255.8 */
    };
    unsigned int sb, sblock;

    pe_s = 1236.28f / 4;
    for(sb=0; sb < SBMAX_s - 1; sb++) {
        for(sblock=0; sblock < 3; sblock++) {
            FLOAT const thm = mr->thm.s[sb][sblock];
            assert(sb < dimension_of(regcoef_s));
            if(thm > 0.0f) {
                FLOAT const x = thm * masking_lower;
                FLOAT const en = mr->en.s[sb][sblock];
                if(en > x) {
                    if(en > x * 1e10f) {
                        pe_s += regcoef_s[sb] * (10.0f * LOG10);
                    }
                    else {
                        assert(x > 0);
                        pe_s += regcoef_s[sb] * FAST_LOG10(en / x);
                    }
                }
            }
        }
    }

    return pe_s;
	}

static FLOAT pecalc_l(III_psy_ratio const *mr, FLOAT masking_lower) {
    FLOAT   pe_l;
    static const FLOAT regcoef_l[] = {
        6.8,            /* these values are tuned only for 44.1kHz... */
        5.8,
        5.8,
        6.4,
        6.5,
        9.9,
        12.1,
        14.4,
        15,
        18.9,
        21.6,
        26.9,
        34.2,
        40.2,
        46.8,
        56.5,
        60.7,
        73.9,
        85.7,
        93.4,
        126.1,
/*      241.3 */
    };
    unsigned int sb;

    pe_l = 1124.23f / 4;
    for(sb=0; sb < SBMAX_l - 1; sb++) {
        FLOAT const thm = mr->thm.l[sb];
        assert(sb < dimension_of(regcoef_l));
        if(thm > 0.0f) {
            FLOAT const x = thm * masking_lower;
            FLOAT const en = mr->en.l[sb];
            if(en > x) {
                if(en > x * 1e10f) {
                    pe_l += regcoef_l[sb] * (10.0f * LOG10);
                }
                else {
                    assert(x > 0);
                    pe_l += regcoef_l[sb] * FAST_LOG10(en / x);
                }
            }
        }
    }

  return pe_l;
	}


static void calc_energy(PsyConst_CB2SB_t const *l, FLOAT const *fftenergy, FLOAT * eb, FLOAT * max, FLOAT * avg) {
    int     b, j;

    for(b = j = 0; b < l->npart; ++b) {
        FLOAT   ebb = 0, m = 0;
        int     i;
        for(i = 0; i < l->numlines[b]; ++i, ++j) {
            FLOAT const el = fftenergy[j];
            assert(el >= 0);
            ebb += el;
            if(m < el)
                m = el;
        }
        eb[b] = ebb;
        max[b] = m;
        avg[b] = ebb * l->rnumlines[b];
        assert(l->rnumlines[b] >= 0);
        assert(ebb >= 0);
        assert(eb[b] >= 0);
        assert(max[b] >= 0);
        assert(avg[b] >= 0);
    }
	}


static void calc_mask_index_l(lame_internal_flags const *gfc, FLOAT const *max,
                  FLOAT const *avg, unsigned char *mask_idx) {
    PsyConst_CB2SB_t const *const gdl = &gfc->cd_psy->l;
    FLOAT   m, a;
    int     b, k;
    int const last_tab_entry = sizeof(tab) / sizeof(tab[0]) - 1;

    b = 0;
    a = avg[b] + avg[b + 1];
    assert(a >= 0);
    if(a > 0.0f) {
        m = max[b];
        if(m < max[b + 1])
            m = max[b + 1];
        assert((gdl->numlines[b] + gdl->numlines[b + 1] - 1) > 0);
        a = 20.0f * (m * 2.0f - a)
            / (a * (gdl->numlines[b] + gdl->numlines[b + 1] - 1));
        k = (int) a;
        if(k > last_tab_entry)
            k = last_tab_entry;
        mask_idx[b] = k;
    }
    else {
        mask_idx[b] = 0;
    }

    for(b = 1; b < gdl->npart - 1; b++) {
        a = avg[b - 1] + avg[b] + avg[b + 1];
        assert(a >= 0);
        if(a > 0.0f) {
            m = max[b - 1];
            if(m < max[b])
                m = max[b];
            if(m < max[b + 1])
                m = max[b + 1];
            assert((gdl->numlines[b - 1] + gdl->numlines[b] + gdl->numlines[b + 1] - 1) > 0);
            a = 20.0f * (m * 3.0f - a)
                / (a * (gdl->numlines[b - 1] + gdl->numlines[b] + gdl->numlines[b + 1] - 1));
            k = (int) a;
            if(k > last_tab_entry)
                k = last_tab_entry;
            mask_idx[b] = k;
        }
        else {
            mask_idx[b] = 0;
        }
    }
    assert(b > 0);
    assert(b == gdl->npart - 1);

    a = avg[b - 1] + avg[b];
    assert(a >= 0);
    if(a > 0.0f) {
        m = max[b - 1];
        if(m < max[b])
            m = max[b];
        assert((gdl->numlines[b - 1] + gdl->numlines[b] - 1) > 0);
        a = 20.0f * (m * 2.0f - a)
            / (a * (gdl->numlines[b - 1] + gdl->numlines[b] - 1));
        k = (int) a;
        if(k > last_tab_entry)
            k = last_tab_entry;
        mask_idx[b] = k;
    }
    else {
        mask_idx[b] = 0;
    }
  assert(b == (gdl->npart - 1));
	}


static void vbrpsy_compute_fft_l(lame_internal_flags * gfc, const sample_t * const buffer[2], int chn,
                     int gr_out, FLOAT fftenergy[HBLKSIZE], FLOAT(*wsamp_l)[BLKSIZE]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    PsyStateVar_t *psv = &gfc->sv_psy;
    plotting_data *plt = cfg->analysis ? gfc->pinfo : 0;
    int     j;

    if(chn < 2) {
        fft_long(gfc, *wsamp_l, chn, buffer);
    }
    else if(chn == 2) {
        FLOAT const sqrt2_half = SQRT2 * 0.5f;
        /* FFT data for mid and side channel is derived from L & R */
        for(j = BLKSIZE - 1; j >= 0; --j) {
            FLOAT const l = wsamp_l[0][j];
            FLOAT const r = wsamp_l[1][j];
            wsamp_l[0][j] = (l + r) * sqrt2_half;
            wsamp_l[1][j] = (l - r) * sqrt2_half;
        }
    }

    /*********************************************************************
    *  compute energies
    *********************************************************************/
    fftenergy[0] = wsamp_l[0][0];
    fftenergy[0] *= fftenergy[0];

    for(j = BLKSIZE / 2 - 1; j >= 0; --j) {
        FLOAT const re = (*wsamp_l)[BLKSIZE / 2 - j];
        FLOAT const im = (*wsamp_l)[BLKSIZE / 2 + j];
        fftenergy[BLKSIZE / 2 - j] = (re * re + im * im) * 0.5f;
    }
    /* total energy */
    {
        FLOAT   totalenergy = 0.0f;
        for(j = 11; j < HBLKSIZE; j++)
            totalenergy += fftenergy[j];

        psv->tot_ener[chn] = totalenergy;
    }

    if(plt) {
        for(j=0; j < HBLKSIZE; j++) {
            plt->energy[gr_out][chn][j] = plt->energy_save[chn][j];
            plt->energy_save[chn][j] = fftenergy[j];
        }
    }
	}


static void vbrpsy_compute_fft_s(lame_internal_flags const *gfc, const sample_t * const buffer[2], int chn,
                     int sblock, FLOAT(*fftenergy_s)[HBLKSIZE_s], FLOAT(*wsamp_s)[3][BLKSIZE_s]) {
    int     j;

    if(sblock == 0 && chn < 2) {
        fft_short(gfc, *wsamp_s, chn, buffer);
    }
    if(chn == 2) {
        FLOAT const sqrt2_half = SQRT2 * 0.5f;
        /* FFT data for mid and side channel is derived from L & R */
        for(j = BLKSIZE_s - 1; j >= 0; --j) {
            FLOAT const l = wsamp_s[0][sblock][j];
            FLOAT const r = wsamp_s[1][sblock][j];
            wsamp_s[0][sblock][j] = (l + r) * sqrt2_half;
            wsamp_s[1][sblock][j] = (l - r) * sqrt2_half;
        }
    }

    /*********************************************************************
    *  compute energies
    *********************************************************************/
    fftenergy_s[sblock][0] = (*wsamp_s)[sblock][0];
    fftenergy_s[sblock][0] *= fftenergy_s[sblock][0];
    for(j = BLKSIZE_s / 2 - 1; j >= 0; --j) {
        FLOAT const re = (*wsamp_s)[sblock][BLKSIZE_s / 2 - j];
        FLOAT const im = (*wsamp_s)[sblock][BLKSIZE_s / 2 + j];
        fftenergy_s[sblock][BLKSIZE_s / 2 - j] = (re * re + im * im) * 0.5f;
    }
}


    /*********************************************************************
    * compute loudness approximation (used for ATH auto-level adjustment) 
    *********************************************************************/
static void vbrpsy_compute_loudness_approximation_l(lame_internal_flags * gfc, int gr_out, int chn,
                                        const FLOAT fftenergy[HBLKSIZE]) {
    PsyStateVar_t *psv = &gfc->sv_psy;

    if(chn < 2) {      /*no loudness for mid/side ch */
        gfc->ov_psy.loudness_sq[gr_out][chn] = psv->loudness_sq_save[chn];
        psv->loudness_sq_save[chn] = psycho_loudness_approx(fftenergy, gfc->ATH->eql_w);
    }
	}


    /**********************************************************************
    *  Apply HPF of fs/4 to the input signal.
    *  This is used for attack detection / handling.
    **********************************************************************/
static void vbrpsy_attack_detection(lame_internal_flags * gfc, const sample_t * const buffer[2], int gr_out,
                        III_psy_ratio masking_ratio[2][2], III_psy_ratio masking_MS_ratio[2][2],
                        FLOAT energy[4], FLOAT sub_short_factor[4][3], int ns_attacks[4][4],
                        int uselongblock[2]) {
    FLOAT   ns_hpfsmpl[2][576];
    SessionConfig_t const *const cfg = &gfc->cfg;
    PsyStateVar_t *const psv = &gfc->sv_psy;
    plotting_data *plt = cfg->analysis ? gfc->pinfo : 0;
    int const n_chn_out = cfg->channels_out;
    /* chn=2 and 3 = Mid and Side channels */
    int const n_chn_psy = (cfg->mode == JOINT_STEREO) ? 4 : n_chn_out;
    int     chn, i, j;

    memset(&ns_hpfsmpl[0][0], 0, sizeof(ns_hpfsmpl));
    /* Don't copy the input buffer into a temporary buffer */
    /* unroll the loop 2 times */
    for(chn=0; chn < n_chn_out; chn++) {
        static const FLOAT fircoef[] = {
            -8.65163e-18 * 2, -0.00851586 * 2, -6.74764e-18 * 2, 0.0209036 * 2,
            -3.36639e-17 * 2, -0.0438162 * 2, -1.54175e-17 * 2, 0.0931738 * 2,
            -5.52212e-17 * 2, -0.313819 * 2
        };
        /* apply high pass filter of fs/4 */
        const sample_t *const firbuf = &buffer[chn][576 - 350 - NSFIRLEN + 192];
        assert(dimension_of(fircoef) == ((NSFIRLEN - 1) / 2));
        for(i=0; i < 576; i++) {
            FLOAT   sum1, sum2;
            sum1 = firbuf[i + 10];
            sum2 = 0.0;
            for(j=0; j < ((NSFIRLEN - 1) / 2) - 1; j += 2) {
                sum1 += fircoef[j] * (firbuf[i + j] + firbuf[i + NSFIRLEN - j]);
                sum2 += fircoef[j + 1] * (firbuf[i + j + 1] + firbuf[i + NSFIRLEN - j - 1]);
            }
            ns_hpfsmpl[chn][i] = sum1 + sum2;
        }
        masking_ratio[gr_out][chn].en = psv->en[chn];
        masking_ratio[gr_out][chn].thm = psv->thm[chn];
        if(n_chn_psy > 2) {
            /* MS maskings  */
            /*percep_MS_entropy         [chn-2]     = gfc -> pe  [chn];  */
            masking_MS_ratio[gr_out][chn].en = psv->en[chn + 2];
            masking_MS_ratio[gr_out][chn].thm = psv->thm[chn + 2];
        }
    }
    for(chn=0; chn < n_chn_psy; chn++) {
        FLOAT   attack_intensity[12];
        FLOAT   en_subshort[12];
        FLOAT   en_short[4] = { 0, 0, 0, 0 };
        FLOAT const *pf = ns_hpfsmpl[chn & 1];
        int     ns_uselongblock = 1;

        if(chn == 2) {
            for(i=0, j = 576; j > 0; ++i, --j) {
                FLOAT const l = ns_hpfsmpl[0][i];
                FLOAT const r = ns_hpfsmpl[1][i];
                ns_hpfsmpl[0][i] = l + r;
                ns_hpfsmpl[1][i] = l - r;
            }
        }
        /*************************************************************** 
        * determine the block type (window type)
        ***************************************************************/
        /* calculate energies of each sub-shortblocks */
        for(i=0; i < 3; i++) {
            en_subshort[i] = psv->last_en_subshort[chn][i + 6];
            assert(psv->last_en_subshort[chn][i + 4] > 0);
            attack_intensity[i] = en_subshort[i] / psv->last_en_subshort[chn][i + 4];
            en_short[0] += en_subshort[i];
        }

        for(i=0; i < 9; i++) {
            FLOAT const *const pfe = pf + 576 / 9;
            FLOAT   p = 1.;
            for(; pf < pfe; pf++)
                if(p < fabs(*pf))
                    p = fabs(*pf);
            psv->last_en_subshort[chn][i] = en_subshort[i + 3] = p;
            en_short[1 + i / 3] += p;
            if(p > en_subshort[i + 3 - 2]) {
                assert(en_subshort[i + 3 - 2] > 0);
                p = p / en_subshort[i + 3 - 2];
            }
            else if(en_subshort[i + 3 - 2] > p * 10.0f) {
                assert(p > 0);
                p = en_subshort[i + 3 - 2] / (p * 10.0f);
            }
            else {
                p = 0.0;
            }
            attack_intensity[i + 3] = p;
        }

        /* pulse like signal detection for fatboy.wav and so on */
        for(i=0; i < 3; ++i) {
            FLOAT const enn =
                en_subshort[i * 3 + 3] + en_subshort[i * 3 + 4] + en_subshort[i * 3 + 5];
            FLOAT   factor = 1.f;
            if(en_subshort[i * 3 + 5] * 6 < enn) {
                factor *= 0.5f;
                if(en_subshort[i * 3 + 4] * 6 < enn) {
                    factor *= 0.5f;
                }
            }
            sub_short_factor[chn][i] = factor;
        }

        if(plt) {
            FLOAT   x = attack_intensity[0];
            for(i = 1; i < 12; i++) {
                if(x < attack_intensity[i]) {
                    x = attack_intensity[i];
                }
            }
            plt->ers[gr_out][chn] = plt->ers_save[chn];
            plt->ers_save[chn] = x;
        }

        /* compare energies between sub-shortblocks */
        {
            FLOAT   x = gfc->cd_psy->attack_threshold[chn];
            for(i=0; i < 12; i++) {
                if(ns_attacks[chn][i / 3] == 0) {
                    if(attack_intensity[i] > x) {
                        ns_attacks[chn][i / 3] = (i % 3) + 1;
                    }
                }
            }
        }
        /* should have energy change between short blocks, in order to avoid periodic signals */
        /* Good samples to show the effect are Trumpet test songs */
        /* GB: tuned (1) to avoid too many short blocks for test sample TRUMPET */
        /* RH: tuned (2) to let enough short blocks through for test sample FSOL and SNAPS */


#define         Min(A, B)       ((A) < (B) ? (A) : (B))
				// NON LE VEDE Qua cheffrocio
#define         Max(A, B)       ((A) > (B) ? (A) : (B))

        for(i = 1; i < 4; i++) {
            FLOAT const u = en_short[i - 1];
            FLOAT const v = en_short[i];
            FLOAT const m = Max(u, v);
            if(m < 40000) { /* (2) */
                if(u < 1.7f * v && v < 1.7f * u) { /* (1) */
                    if(i == 1 && ns_attacks[chn][0] <= ns_attacks[chn][i]) {
                        ns_attacks[chn][0] = 0;
                    }
                    ns_attacks[chn][i] = 0;
                }
            }
					}

        if(ns_attacks[chn][0] <= psv->last_attacks[chn]) {
            ns_attacks[chn][0] = 0;
					}

        if(psv->last_attacks[chn] == 3 ||
            ns_attacks[chn][0] + ns_attacks[chn][1] + ns_attacks[chn][2] + ns_attacks[chn][3]) {
            ns_uselongblock = 0;

            if(ns_attacks[chn][1] && ns_attacks[chn][0]) {
                ns_attacks[chn][1] = 0;
            }
            if(ns_attacks[chn][2] && ns_attacks[chn][1]) {
                ns_attacks[chn][2] = 0;
            }
            if(ns_attacks[chn][3] && ns_attacks[chn][2]) {
                ns_attacks[chn][3] = 0;
            }
        }

        if(chn < 2) {
            uselongblock[chn] = ns_uselongblock;
        }
        else {
            if(ns_uselongblock == 0) {
                uselongblock[0] = uselongblock[1] = 0;
            }
        }

        /* there is a one granule delay.  Copy maskings computed last call
         * into masking_ratio to return to calling program.
         */
        energy[chn] = psv->tot_ener[chn];
    }
	}


static void vbrpsy_skip_masking_s(lame_internal_flags * gfc, int chn, int sblock) {

    if(sblock == 0) {
        FLOAT  *nbs2 = &gfc->sv_psy.nb_s2[chn][0];
        FLOAT  *nbs1 = &gfc->sv_psy.nb_s1[chn][0];
        int const n = gfc->cd_psy->s.npart;
        int     b;
        for(b = 0; b < n; b++) {
            nbs2[b] = nbs1[b];
        }
    }
	}


static void vbrpsy_calc_mask_index_s(lame_internal_flags const *gfc, FLOAT const *max,
                         FLOAT const *avg, unsigned char *mask_idx) {
    PsyConst_CB2SB_t const *const gds = &gfc->cd_psy->s;
    FLOAT   m, a;
    int     b, k;
    int const last_tab_entry = dimension_of(tab) - 1;

    b = 0;
    a = avg[b] + avg[b + 1];
    assert(a >= 0);
    if(a > 0.0f) {
        m = max[b];
        if(m < max[b + 1])
            m = max[b + 1];
        assert((gds->numlines[b] + gds->numlines[b + 1] - 1) > 0);
        a = 20.0f * (m * 2.0f - a)
            / (a * (gds->numlines[b] + gds->numlines[b + 1] - 1));
        k = (int) a;
        if(k > last_tab_entry)
            k = last_tab_entry;
        mask_idx[b] = k;
			}
    else {
        mask_idx[b] = 0;
			}

    for(b = 1; b < gds->npart - 1; b++) {
        a = avg[b - 1] + avg[b] + avg[b + 1];
        assert(b + 1 < gds->npart);
        assert(a >= 0);
        if(a > 0.0) {
            m = max[b - 1];
            if(m < max[b])
                m = max[b];
            if(m < max[b + 1])
                m = max[b + 1];
            assert((gds->numlines[b - 1] + gds->numlines[b] + gds->numlines[b + 1] - 1) > 0);
            a = 20.0f * (m * 3.0f - a)
                / (a * (gds->numlines[b - 1] + gds->numlines[b] + gds->numlines[b + 1] - 1));
            k = (int) a;
            if(k > last_tab_entry)
                k = last_tab_entry;
            mask_idx[b] = k;
        }
        else {
            mask_idx[b] = 0;
        }
			}
    assert(b > 0);
    assert(b == gds->npart - 1);

    a = avg[b - 1] + avg[b];
    assert(a >= 0);
    if(a > 0.0f) {
        m = max[b - 1];
        if(m < max[b])
            m = max[b];
        assert((gds->numlines[b - 1] + gds->numlines[b] - 1) > 0);
        a = 20.0f * (m * 2.0f - a)
            / (a * (gds->numlines[b - 1] + gds->numlines[b] - 1));
        k = (int) a;
        if(k > last_tab_entry)
            k = last_tab_entry;
        mask_idx[b] = k;
			}
    else {
        mask_idx[b] = 0;
			}
  assert(b == (gds->npart - 1));
	}


static void vbrpsy_compute_masking_s(lame_internal_flags * gfc, const FLOAT(*fftenergy_s)[HBLKSIZE_s],
                         FLOAT * eb, FLOAT * thr, int chn, int sblock) {
    PsyStateVar_t *const psv = &gfc->sv_psy;
    PsyConst_CB2SB_t const *const gds = &gfc->cd_psy->s;
    FLOAT   max[CBANDS], avg[CBANDS];
    int     i, j, b;
    unsigned char mask_idx_s[CBANDS];

    memset(max, 0, sizeof(max));
    memset(avg, 0, sizeof(avg));

    for(b = j = 0; b < gds->npart; ++b) {
        FLOAT   ebb = 0, m = 0;
        int const n = gds->numlines[b];
        for(i = 0; i < n; ++i, ++j) {
            FLOAT const el = fftenergy_s[sblock][j];
            ebb += el;
            if(m < el)
                m = el;
        }
        eb[b] = ebb;
        assert(ebb >= 0);
        max[b] = m;
        assert(n > 0);
        avg[b] = ebb * gds->rnumlines[b];
        assert(avg[b] >= 0);
    }
    assert(b == gds->npart);
    assert(j == 129);
    vbrpsy_calc_mask_index_s(gfc, max, avg, mask_idx_s);
    for(j = b = 0; b < gds->npart; b++) {
        int     kk = gds->s3ind[b][0];
        int const last = gds->s3ind[b][1];
        int const delta = mask_add_delta(mask_idx_s[b]);
        int     dd, dd_n;
        FLOAT   x, ecb, avg_mask;
        FLOAT const masking_lower = gds->masking_lower[b] * gfc->sv_qnt.masking_lower;

        dd = mask_idx_s[kk];
        dd_n = 1;
        ecb = gds->s3[j] * eb[kk] * tab[mask_idx_s[kk]];
        ++j, ++kk;
        while(kk <= last) {
            dd += mask_idx_s[kk];
            dd_n += 1;
            x = gds->s3[j] * eb[kk] * tab[mask_idx_s[kk]];
            ecb = vbrpsy_mask_add(ecb, x, kk - b, delta);
            ++j, ++kk;
        }
        dd = (1 + 2 * dd) / (2 * dd_n);
        avg_mask = tab[dd] * 0.5f;
        ecb *= avg_mask;
#if 0                   /* we can do PRE ECHO control now here, or do it later */
        if(psv->blocktype_old[chn & 0x01] == SHORT_TYPE) {
            /* limit calculated threshold by even older granule */
            FLOAT const t1 = rpelev_s * psv->nb_s1[chn][b];
            FLOAT const t2 = rpelev2_s * psv->nb_s2[chn][b];
            FLOAT const tm = (t2 > 0) ? Min(ecb, t2) : ecb;
            thr[b] = (t1 > 0) ? NS_INTERP(Min(tm, t1), ecb, 0.6) : ecb;
        }
        else {
            /* limit calculated threshold by older granule */
            FLOAT const t1 = rpelev_s * psv->nb_s1[chn][b];
            thr[b] = (t1 > 0) ? NS_INTERP(Min(ecb, t1), ecb, 0.6) : ecb;
        }
#else /* we do it later */
        thr[b] = ecb;
#endif
        psv->nb_s2[chn][b] = psv->nb_s1[chn][b];
        psv->nb_s1[chn][b] = ecb;
        {
            /*  if THR exceeds EB, the quantization routines will take the difference
             *  from other bands. in case of strong tonal samples (tonaltest.wav)
             *  this leads to heavy distortions. that's why we limit THR here.
             */
            x = max[b];
            x *= gds->minval[b];
            x *= avg_mask;
            if(thr[b] > x) {
                thr[b] = x;
            }
        }
        if(masking_lower > 1) {
            thr[b] *= masking_lower;
        }
        if(thr[b] > eb[b]) {
            thr[b] = eb[b];
        }
        if(masking_lower < 1) {
            thr[b] *= masking_lower;
        }

        assert(thr[b] >= 0);
    }
    for(; b < CBANDS; ++b) {
        eb[b] = 0;
        thr[b] = 0;
    }
	}


static void vbrpsy_compute_masking_l(lame_internal_flags * gfc, const FLOAT fftenergy[HBLKSIZE],
                         FLOAT eb_l[CBANDS], FLOAT thr[CBANDS], int chn) {
    PsyStateVar_t *const psv = &gfc->sv_psy;
    PsyConst_CB2SB_t const *const gdl = &gfc->cd_psy->l;
    FLOAT   max[CBANDS], avg[CBANDS];
    unsigned char mask_idx_l[CBANDS + 2];
    int     k, b;

 /*********************************************************************
    *    Calculate the energy and the tonality of each partition.
 *********************************************************************/
    calc_energy(gdl, fftenergy, eb_l, max, avg);
    calc_mask_index_l(gfc, max, avg, mask_idx_l);

 /*********************************************************************
    *      convolve the partitioned energy and unpredictability
    *      with the spreading function, s3_l[b][k]
 ********************************************************************/
    k = 0;
    for(b = 0; b < gdl->npart; b++) {
        FLOAT   x, ecb, avg_mask, t;
        FLOAT const masking_lower = gdl->masking_lower[b] * gfc->sv_qnt.masking_lower;
        /* convolve the partitioned energy with the spreading function */
        int     kk = gdl->s3ind[b][0];
        int const last = gdl->s3ind[b][1];
        int const delta = mask_add_delta(mask_idx_l[b]);
        int     dd = 0, dd_n = 0;

        dd = mask_idx_l[kk];
        dd_n += 1;
        ecb = gdl->s3[k] * eb_l[kk] * tab[mask_idx_l[kk]];
        ++k, ++kk;
        while(kk <= last) {
            dd += mask_idx_l[kk];
            dd_n += 1;
            x = gdl->s3[k] * eb_l[kk] * tab[mask_idx_l[kk]];
            t = vbrpsy_mask_add(ecb, x, kk - b, delta);
#if 0
            ecb += eb_l[kk];
            if(ecb > t) {
                ecb = t;
            }
#else
            ecb = t;
#endif
            ++k, ++kk;
        }
        dd = (1 + 2 * dd) / (2 * dd_n);
        avg_mask = tab[dd] * 0.5f;
        ecb *= avg_mask;

        /****   long block pre-echo control   ****/
        /* dont use long block pre-echo control if previous granule was 
         * a short block.  This is to avoid the situation:   
         * frame0:  quiet (very low masking)  
         * frame1:  surge  (triggers short blocks)
         * frame2:  regular frame.  looks like pre-echo when compared to 
         *          frame0, but all pre-echo was in frame1.
         */
        /* chn=0,1   L and R channels
           chn=2,3   S and M channels.
         */
        if(psv->blocktype_old[chn & 0x01] == SHORT_TYPE) {
            FLOAT const ecb_limit = rpelev * psv->nb_l1[chn][b];
            if(ecb_limit > 0) {
                thr[b] = Min(ecb, ecb_limit);
            }
            else {
                /* Robert 071209:
                   Because we don't calculate long block psy when we know a granule
                   should be of short blocks, we don't have any clue how the granule
                   before would have looked like as a long block. So we have to guess
                   a little bit for this END_TYPE block.
                   Most of the time we get away with this sloppyness. (fingers crossed :)
                   The speed increase is worth it.
                 */
                thr[b] = Min(ecb, eb_l[b] * NS_PREECHO_ATT2);
            }
        }
        else {
            FLOAT   ecb_limit_2 = rpelev2 * psv->nb_l2[chn][b];
            FLOAT   ecb_limit_1 = rpelev * psv->nb_l1[chn][b];
            FLOAT   ecb_limit;
            if(ecb_limit_2 <= 0) {
                ecb_limit_2 = ecb;
            }
            if(ecb_limit_1 <= 0) {
                ecb_limit_1 = ecb;
            }
            if(psv->blocktype_old[chn & 0x01] == NORM_TYPE) {
                ecb_limit = Min(ecb_limit_1, ecb_limit_2);
            }
            else {
                ecb_limit = ecb_limit_1;
            }
            thr[b] = Min(ecb, ecb_limit);
        }
        psv->nb_l2[chn][b] = psv->nb_l1[chn][b];
        psv->nb_l1[chn][b] = ecb;
        {
            /*  if THR exceeds EB, the quantization routines will take the difference
             *  from other bands. in case of strong tonal samples (tonaltest.wav)
             *  this leads to heavy distortions. that's why we limit THR here.
             */
            x = max[b];
            x *= gdl->minval[b];
            x *= avg_mask;
            if(thr[b] > x) {
                thr[b] = x;
            }
        }
        if(masking_lower > 1) {
            thr[b] *= masking_lower;
        }
        if(thr[b] > eb_l[b]) {
            thr[b] = eb_l[b];
        }
        if(masking_lower < 1) {
            thr[b] *= masking_lower;
        }
        assert(thr[b] >= 0);
    }
    for(; b < CBANDS; ++b) {
        eb_l[b] = 0;
        thr[b] = 0;
    }
	}


static void vbrpsy_compute_block_type(SessionConfig_t const *cfg, int *uselongblock) {
    int     chn;

    if(cfg->short_blocks == short_block_coupled
        /* force both channels to use the same block type */
        /* this is necessary if the frame is to be encoded in ms_stereo.  */
        /* But even without ms_stereo, FhG  does this */
        && !(uselongblock[0] && uselongblock[1]))
        uselongblock[0] = uselongblock[1] = 0;

    for(chn = 0; chn < cfg->channels_out; chn++) {
        /* disable short blocks */
        if(cfg->short_blocks == short_block_dispensed) {
            uselongblock[chn] = 1;
        }
        if(cfg->short_blocks == short_block_forced) {
            uselongblock[chn] = 0;
        }
    }
}


static void vbrpsy_apply_block_type(PsyStateVar_t * psv, int nch, int const *uselongblock, int *blocktype_d) {
    int     chn;

    /* update the blocktype of the previous granule, since it depends on what
     * happend in this granule */
    for(chn = 0; chn < nch; chn++) {
        int     blocktype = NORM_TYPE;
        /* disable short blocks */

        if(uselongblock[chn]) {
            /* no attack : use long blocks */
            assert(psv->blocktype_old[chn] != START_TYPE);
            if(psv->blocktype_old[chn] == SHORT_TYPE)
                blocktype = STOP_TYPE;
        }
        else {
            /* attack : use short blocks */
            blocktype = SHORT_TYPE;
            if(psv->blocktype_old[chn] == NORM_TYPE) {
                psv->blocktype_old[chn] = START_TYPE;
            }
            if(psv->blocktype_old[chn] == STOP_TYPE)
                psv->blocktype_old[chn] = SHORT_TYPE;
        }

        blocktype_d[chn] = psv->blocktype_old[chn]; /* value returned to calling program */
        psv->blocktype_old[chn] = blocktype; /* save for next call to l3psy_anal */
    }
	}


/*************************************************************** 
 * compute M/S thresholds from Johnston & Ferreira 1992 ICASSP paper
 ***************************************************************/

static void vbrpsy_compute_MS_thresholds(const FLOAT eb[4][CBANDS], FLOAT thr[4][CBANDS],
                             const FLOAT cb_mld[CBANDS], const FLOAT ath_cb[CBANDS], FLOAT athlower,
                             FLOAT msfix, int n) {
    FLOAT const msfix2 = msfix * 2.f;
    FLOAT   rside, rmid;
    int     b;

    for(b = 0; b < n; ++b) {
        FLOAT const ebM = eb[2][b];
        FLOAT const ebS = eb[3][b];
        FLOAT const thmL = thr[0][b];
        FLOAT const thmR = thr[1][b];
        FLOAT   thmM = thr[2][b];
        FLOAT   thmS = thr[3][b];

        /* use this fix if L & R masking differs by 2db or less */
        /* if db = 10*log10(x2/x1) < 2 */
        /* if(x2 < 1.58*x1) { */
        if(thmL <= 1.58f * thmR && thmR <= 1.58f * thmL) {
            FLOAT const mld_m = cb_mld[b] * ebS;
            FLOAT const mld_s = cb_mld[b] * ebM;
            FLOAT const tmp_m = Min(thmS, mld_m);
            FLOAT const tmp_s = Min(thmM, mld_s);
            rmid = Max(thmM, tmp_m);
            rside = Max(thmS, tmp_s);
        }
        else {
            rmid = thmM;
            rside = thmS;
        }
        if(msfix > 0.f) {
            /***************************************************************/
            /* Adjust M/S maskings if user set "msfix"                     */
            /***************************************************************/
            /* Naoki Shibata 2000 */
            FLOAT   thmLR, thmMS;
            FLOAT const ath = ath_cb[b] * athlower;
            FLOAT const tmp_l = Max(thmL, ath);
            FLOAT const tmp_r = Max(thmR, ath);
            thmLR = Min(tmp_l, tmp_r);
            thmM = Max(rmid, ath);
            thmS = Max(rside, ath);
            thmMS = thmM + thmS;
            if(thmMS > 0.f && (thmLR * msfix2) < thmMS) {
                FLOAT const f = thmLR * msfix2 / thmMS;
                thmM *= f;
                thmS *= f;
                assert(thmMS > 0.f);
            }
            rmid = Min(thmM, rmid);
            rside = Min(thmS, rside);
        }
        if(rmid > ebM) {
            rmid = ebM;
        }
        if(rside > ebS) {
            rside = ebS;
        }
        thr[2][b] = rmid;
        thr[3][b] = rside;
    }
	}


/*
 * NOTE: the bitrate reduction from the inter-channel masking effect is low
 * compared to the chance of getting annyoing artefacts. L3psycho_anal_vbr does
 * not use this feature. (Robert 071216)
*/

int L3psycho_anal_vbr(lame_internal_flags * gfc,
                  const sample_t * const buffer[2], int gr_out,
                  III_psy_ratio masking_ratio[2][2],
                  III_psy_ratio masking_MS_ratio[2][2],
                  FLOAT percep_entropy[2], FLOAT percep_MS_entropy[2],
                  FLOAT energy[4], int blocktype_d[2]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    PsyStateVar_t *const psv = &gfc->sv_psy;
    PsyConst_CB2SB_t const *const gdl = &gfc->cd_psy->l;
    PsyConst_CB2SB_t const *const gds = &gfc->cd_psy->s;
    plotting_data *plt = cfg->analysis ? gfc->pinfo : 0;

    III_psy_xmin last_thm[4];

    /* fft and energy calculation   */
    FLOAT(*wsamp_l)[BLKSIZE];
    FLOAT(*wsamp_s)[3][BLKSIZE_s];
    FLOAT   fftenergy[HBLKSIZE];
    FLOAT   fftenergy_s[3][HBLKSIZE_s];
    FLOAT   wsamp_L[2][BLKSIZE];
    FLOAT   wsamp_S[2][3][BLKSIZE_s];
    FLOAT   eb[4][CBANDS], thr[4][CBANDS];

    FLOAT   sub_short_factor[4][3];
    FLOAT   thmm;
    FLOAT const pcfact = 0.6f;
    FLOAT const ath_factor =
        (cfg->msfix > 0.f) ? (cfg->ATH_offset_factor * gfc->ATH->adjust_factor) : 1.f;

    const   FLOAT(*const_eb)[CBANDS] = (const FLOAT(*)[CBANDS]) eb;
    const   FLOAT(*const_fftenergy_s)[HBLKSIZE_s] = (const FLOAT(*)[HBLKSIZE_s]) fftenergy_s;

    /* block type  */
    int     ns_attacks[4][4] = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} };
    int     uselongblock[2];

    /* usual variables like loop indices, etc..    */
    int     chn, sb, sblock;

    /* chn=2 and 3 = Mid and Side channels */
    int const n_chn_psy = (cfg->mode == JOINT_STEREO) ? 4 : cfg->channels_out;

    memcpy(&last_thm[0], &psv->thm[0], sizeof(last_thm));

    vbrpsy_attack_detection(gfc, buffer, gr_out, masking_ratio, masking_MS_ratio, energy,
                            sub_short_factor, ns_attacks, uselongblock);

    vbrpsy_compute_block_type(cfg, uselongblock);

    /* LONG BLOCK CASE */
    {
        for(chn = 0; chn < n_chn_psy; chn++) {
            int const ch01 = chn & 0x01;

            wsamp_l = wsamp_L + ch01;
            vbrpsy_compute_fft_l(gfc, buffer, chn, gr_out, fftenergy, wsamp_l);
            vbrpsy_compute_loudness_approximation_l(gfc, gr_out, chn, fftenergy);
            vbrpsy_compute_masking_l(gfc, fftenergy, eb[chn], thr[chn], chn);
        }
        if(cfg->mode == JOINT_STEREO) {
            if((uselongblock[0] + uselongblock[1]) == 2) {
                vbrpsy_compute_MS_thresholds(const_eb, thr, gdl->mld_cb, gfc->ATH->cb_l,
                                             ath_factor, cfg->msfix, gdl->npart);
            }
        }
        /* TODO: apply adaptive ATH masking here ?? */
        for(chn = 0; chn < n_chn_psy; chn++) {
            convert_partition2scalefac_l(gfc, eb[chn], thr[chn], chn);
            convert_partition2scalefac_l_to_s(gfc, eb[chn], thr[chn], chn);
        }
    }
    /* SHORT BLOCKS CASE */
    {
        int const force_short_block_calc = gfc->cd_psy->force_short_block_calc;
        for(sblock = 0; sblock < 3; sblock++) {
            for(chn = 0; chn < n_chn_psy; ++chn) {
                int const ch01 = chn & 0x01;
                if(uselongblock[ch01] && !force_short_block_calc) {
                    vbrpsy_skip_masking_s(gfc, chn, sblock);
                }
                else {
                    /* compute masking thresholds for short blocks */
                    wsamp_s = wsamp_S + ch01;
                    vbrpsy_compute_fft_s(gfc, buffer, chn, sblock, fftenergy_s, wsamp_s);
                    vbrpsy_compute_masking_s(gfc, const_fftenergy_s, eb[chn], thr[chn], chn,
                                             sblock);
                }
            }
            if(cfg->mode == JOINT_STEREO) {
                if((uselongblock[0] + uselongblock[1]) == 0) {
                    vbrpsy_compute_MS_thresholds(const_eb, thr, gds->mld_cb, gfc->ATH->cb_s,
                                                 ath_factor, cfg->msfix, gds->npart);
                }
            }
            /* TODO: apply adaptive ATH masking here ?? */
            for(chn = 0; chn < n_chn_psy; ++chn) {
                int const ch01 = chn & 0x01;
                if(!uselongblock[ch01] || force_short_block_calc) {
                    convert_partition2scalefac_s(gfc, eb[chn], thr[chn], chn, sblock);
                }
            }
        }

        /****   short block pre-echo control   ****/
        for(chn = 0; chn < n_chn_psy; chn++) {
            for(sb = 0; sb < SBMAX_s; sb++) {
                FLOAT   new_thmm[3], prev_thm, t1, t2;
                for(sblock = 0; sblock < 3; sblock++) {
                    thmm = psv->thm[chn].s[sb][sblock];
                    thmm *= NS_PREECHO_ATT0;

                    t1 = t2 = thmm;

                    if(sblock > 0) {
                        prev_thm = new_thmm[sblock - 1];
                    }
                    else {
                        prev_thm = last_thm[chn].s[sb][2];
                    }
                    if(ns_attacks[chn][sblock] >= 2 || ns_attacks[chn][sblock + 1] == 1) {
                        t1 = NS_INTERP(prev_thm, thmm, NS_PREECHO_ATT1 * pcfact);
                    }
                    thmm = Min(t1, thmm);
                    if(ns_attacks[chn][sblock] == 1) {
                        t2 = NS_INTERP(prev_thm, thmm, NS_PREECHO_ATT2 * pcfact);
                    }
                    else if((sblock == 0 && psv->last_attacks[chn] == 3)
                             || (sblock > 0 && ns_attacks[chn][sblock - 1] == 3)) { /* 2nd preceeding block */
                        switch(sblock) {
                        case 0:
                            prev_thm = last_thm[chn].s[sb][1];
                            break;
                        case 1:
                            prev_thm = last_thm[chn].s[sb][2];
                            break;
                        case 2:
                            prev_thm = new_thmm[0];
                            break;
                        }
                        t2 = NS_INTERP(prev_thm, thmm, NS_PREECHO_ATT2 * pcfact);
                    }

                    thmm = Min(t1, thmm);
                    thmm = Min(t2, thmm);

                    /* pulse like signal detection for fatboy.wav and so on */
                    thmm *= sub_short_factor[chn][sblock];

                    new_thmm[sblock] = thmm;
                }
                for(sblock = 0; sblock < 3; sblock++) {
                    psv->thm[chn].s[sb][sblock] = new_thmm[sblock];
                }
            }
        }
    }
    for(chn = 0; chn < n_chn_psy; chn++) {
        psv->last_attacks[chn] = ns_attacks[chn][2];
    }


    /*************************************************************** 
    * determine final block type
    ***************************************************************/
    vbrpsy_apply_block_type(psv, cfg->channels_out, uselongblock, blocktype_d);

    /*********************************************************************
    * compute the value of PE to return ... no delay and advance
    *********************************************************************/
    for(chn = 0; chn < n_chn_psy; chn++) {
        FLOAT  *ppe;
        int     type;
        III_psy_ratio const *mr;

        if(chn > 1) {
            ppe = percep_MS_entropy - 2;
            type = NORM_TYPE;
            if(blocktype_d[0] == SHORT_TYPE || blocktype_d[1] == SHORT_TYPE)
                type = SHORT_TYPE;
            mr = &masking_MS_ratio[gr_out][chn - 2];
        }
        else {
            ppe = percep_entropy;
            type = blocktype_d[chn];
            mr = &masking_ratio[gr_out][chn];
        }
        if(type == SHORT_TYPE) {
            ppe[chn] = pecalc_s(mr, gfc->sv_qnt.masking_lower);
        }
        else {
            ppe[chn] = pecalc_l(mr, gfc->sv_qnt.masking_lower);
        }

        if(plt) {
            plt->pe[gr_out][chn] = ppe[chn];
        }
    }
  return 0;
	}




/* 
 *   The spreading function.  Values returned in units of energy
 */
static FLOAT s3_func(FLOAT bark) {
    FLOAT   tempx, x, tempy, temp;

    tempx = bark;
    if(tempx >= 0)
        tempx *= 3;
    else
        tempx *= 1.5;

    if(tempx >= 0.5 && tempx <= 2.5) {
        temp = tempx - 0.5;
        x = 8.0 * (temp * temp - 2.0 * temp);
    }
    else
        x = 0.0;
    tempx += 0.474;
    tempy = 15.811389 + 7.5 * tempx - 17.5 * sqrt(1.0 + tempx * tempx);

    if(tempy <= -60.0)
        return 0.0;

    tempx = exp((x + tempy) * LN_TO_LOG10);

    /* Normalization.  The spreading function should be normalized so that:
       +inf
       /
       |  s3 [ bark ]  d(bark)   =  1
       /
       -inf
     */
    tempx /= .6609193;
    return tempx;
	}

#if 0
static  FLOAT norm_s3_func(void) {
    double  lim_a = 0, lim_b = 0;
    double  x = 0, l, h;

    for(x = 0; s3_func(x) > 1e-20; x -= 1);
    l = x;
    h = 0;
    while(fabs(h - l) > 1e-12) {
        x = (h + l) / 2;
        if(s3_func(x) > 0) {
            h = x;
        }
        else {
            l = x;
        }
    }
    lim_a = l;
    for(x = 0; s3_func(x) > 1e-20; x += 1);
    l = 0;
    h = x;
    while(fabs(h - l) > 1e-12) {
        x = (h + l) / 2;
        if(s3_func(x) > 0) {
            l = x;
        }
        else {
            h = x;
        }
    }
    lim_b = h;
    {
        double  sum = 0;
        int const m = 1000;
        int     i;
        for(i = 0; i <= m; ++i) {
            double  x = lim_a + i * (lim_b - lim_a) / m;
            double  y = s3_func(x);
            sum += y;
        }
        {
            double  norm = (m + 1) / (sum * (lim_b - lim_a));
            /*printf( "norm = %lf\n",norm); */
            return norm;
        }
    }
	}
#endif

static FLOAT stereo_demask(double f) {

    /* setup stereo demasking thresholds */
    /* formula reverse enginerred from plot in paper */
    double  arg = freq2bark(f);
    arg = (Min(arg, 15.5) / 15.5);

    return pow(10.0, 1.25 * (1 - cos(PI * arg)) - 2.5);
	}

static void init_numline(PsyConst_CB2SB_t * gd, FLOAT sfreq, int fft_size,
             int mdct_size, int sbmax, int const *scalepos) {
    FLOAT   b_frq[CBANDS + 1];
    FLOAT const mdct_freq_frac = sfreq / (2.0f * mdct_size);
    FLOAT const deltafreq = fft_size / (2.0f * mdct_size);
    int     partition[HBLKSIZE] = { 0 };
    int     i, j, ni;
    int     sfb;

    sfreq /= fft_size;
    j = 0;
    ni = 0;
    /* compute numlines, the number of spectral lines in each partition band */
    /* each partition band should be about DELBARK wide. */
    for(i = 0; i < CBANDS; i++) {
        FLOAT   bark1;
        int     j2, nl;
        bark1 = freq2bark(sfreq * j);

        b_frq[i] = sfreq * j;

        for(j2 = j; freq2bark(sfreq * j2) - bark1 < DELBARK && j2 <= fft_size / 2; j2++);

        nl = j2 - j;
        gd->numlines[i] = nl;
        gd->rnumlines[i] = (nl > 0) ? (1.0f / nl) : 0;

        ni = i + 1;

        while(j < j2) {
            assert(j < HBLKSIZE);
            partition[j++] = i;
        }
        if(j > fft_size / 2) {
            j = fft_size / 2;
            ++i;
            break;
        }
    }
    assert(i < CBANDS);
    b_frq[i] = sfreq * j;

    gd->n_sb = sbmax;
    gd->npart = ni;

    {
        j = 0;
        for(i = 0; i < gd->npart; i++) {
            int const nl = gd->numlines[i];
            FLOAT const freq = sfreq * (j + nl / 2);
            gd->mld_cb[i] = stereo_demask(freq);
            j += nl;
        }
        for(; i < CBANDS; ++i) {
            gd->mld_cb[i] = 1;
        }
    }
    for(sfb = 0; sfb < sbmax; sfb++) {
        int     i1, i2, bo;
        int     start = scalepos[sfb];
        int     end = scalepos[sfb + 1];

        i1 = floor(.5 + deltafreq * (start - .5));
        if(i1 < 0)
            i1 = 0;
        i2 = floor(.5 + deltafreq * (end - .5));

        if(i2 > fft_size / 2)
            i2 = fft_size / 2;

        bo = partition[i2];
        gd->bm[sfb] = (partition[i1] + partition[i2]) / 2;
        gd->bo[sfb] = bo;

        /* calculate how much of this band belongs to current scalefactor band */
        {
            FLOAT const f_tmp = mdct_freq_frac * end;
            FLOAT   bo_w = (f_tmp - b_frq[bo]) / (b_frq[bo + 1] - b_frq[bo]);
            if(bo_w < 0) {
                bo_w = 0;
            }
            else {
                if(bo_w > 1) {
                    bo_w = 1;
                }
            }
            gd->bo_weight[sfb] = bo_w;
        }
        gd->mld[sfb] = stereo_demask(mdct_freq_frac * start);
    }
	}

static void compute_bark_values(PsyConst_CB2SB_t const *gd, FLOAT sfreq, int fft_size,
                    FLOAT * bval, FLOAT * bval_width) {
    /* compute bark values of each critical band */
    int     k, j = 0, ni = gd->npart;

    sfreq /= fft_size;
    for(k = 0; k < ni; k++) {
        int const w = gd->numlines[k];
        FLOAT   bark1, bark2;

        bark1 = freq2bark(sfreq * (j));
        bark2 = freq2bark(sfreq * (j + w - 1));
        bval[k] = .5 * (bark1 + bark2);

        bark1 = freq2bark(sfreq * (j - .5));
        bark2 = freq2bark(sfreq * (j + w - .5));
        bval_width[k] = bark2 - bark1;
        j += w;
    }
	}

static int init_s3_values(FLOAT ** p, int (*s3ind)[2], int npart,
               FLOAT const *bval, FLOAT const *bval_width, FLOAT const *norm) {
    FLOAT   s3[CBANDS][CBANDS];
    /* The s3 array is not linear in the bark scale.
     * bval[x] should be used to get the bark value.
     */
    int     i, j, k;
    int     numberOfNoneZero = 0;

    memset(&s3[0][0], 0, sizeof(s3));

    /* s[i][j], the value of the spreading function,
     * centered at band j (masker), for band i (maskee)
     *
     * i.e.: sum over j to spread into signal barkval=i
     * NOTE: i and j are used opposite as in the ISO docs
     */
    for(i = 0; i < npart; i++) {
        for(j = 0; j < npart; j++) {
            FLOAT   v = s3_func(bval[i] - bval[j]) * bval_width[j];
            s3[i][j] = v * norm[i];
        }
    }
    for(i = 0; i < npart; i++) {
        for(j = 0; j < npart; j++) {
            if(s3[i][j] > 0.0f)
                break;
        }
        s3ind[i][0] = j;

        for(j = npart - 1; j > 0; j--) {
            if(s3[i][j] > 0.0f)
                break;
        }
        s3ind[i][1] = j;
        numberOfNoneZero += (s3ind[i][1] - s3ind[i][0] + 1);
    }
    *p = lame_calloc(FLOAT, numberOfNoneZero);
    if(!*p)
        return -1;

    k = 0;
    for(i = 0; i < npart; i++)
        for(j = s3ind[i][0]; j <= s3ind[i][1]; j++)
            (*p)[k++] = s3[i][j];

    return 0;
	}

int psymodel_init(lame_global_flags const *gfp) {
    lame_internal_flags *const gfc = gfp->internal_flags;
    SessionConfig_t *const cfg = &gfc->cfg;
    PsyStateVar_t *const psv = &gfc->sv_psy;
    PsyConst_t *gd;
    int     i, j, b, sb, k;
    FLOAT   bvl_a = 13, bvl_b = 24;
    FLOAT   snr_l_a = 0, snr_l_b = 0;
    FLOAT   snr_s_a = -8.25, snr_s_b = -4.5;

    FLOAT   bval[CBANDS];
    FLOAT   bval_width[CBANDS];
    FLOAT   norm[CBANDS];
    FLOAT const sfreq = cfg->samplerate_out;

    FLOAT   xav = 10, xbv = 12;
    FLOAT const minval_low = (0.f - cfg->minval);

    if(gfc->cd_psy != 0) {
        return 0;
    }
    memset(norm, 0, sizeof(norm));

    gd = lame_calloc(PsyConst_t, 1);
    gfc->cd_psy = gd;

    gd->force_short_block_calc = gfp->experimentalZ;

    psv->blocktype_old[0] = psv->blocktype_old[1] = NORM_TYPE; /* the vbr header is long blocks */

    for(i = 0; i < 4; ++i) {
        for(j = 0; j < CBANDS; ++j) {
            psv->nb_l1[i][j] = 1e20;
            psv->nb_l2[i][j] = 1e20;
            psv->nb_s1[i][j] = psv->nb_s2[i][j] = 1.0;
        }
        for(sb = 0; sb < SBMAX_l; sb++) {
            psv->en[i].l[sb] = 1e20;
            psv->thm[i].l[sb] = 1e20;
        }
        for(j = 0; j < 3; ++j) {
            for(sb = 0; sb < SBMAX_s; sb++) {
                psv->en[i].s[sb][j] = 1e20;
                psv->thm[i].s[sb][j] = 1e20;
            }
            psv->last_attacks[i] = 0;
        }
        for(j = 0; j < 9; j++)
            psv->last_en_subshort[i][j] = 10.;
    }


    /* init. for loudness approx. -jd 2001 mar 27 */
    psv->loudness_sq_save[0] = psv->loudness_sq_save[1] = 0.0;



    /*************************************************************************
     * now compute the psychoacoustic model specific constants
     ************************************************************************/
    /* compute numlines, bo, bm, bval, bval_width, mld */
    init_numline(&gd->l, sfreq, BLKSIZE, 576, SBMAX_l, gfc->scalefac_band.l);
    assert(gd->l.npart < CBANDS);
    compute_bark_values(&gd->l, sfreq, BLKSIZE, bval, bval_width);

    /* compute the spreading function */
    for(i = 0; i < gd->l.npart; i++) {
        double  snr = snr_l_a;
        if(bval[i] >= bvl_a) {
            snr = snr_l_b * (bval[i] - bvl_a) / (bvl_b - bvl_a)
                + snr_l_a * (bvl_b - bval[i]) / (bvl_b - bvl_a);
        }
        norm[i] = pow(10.0, snr / 10.0);
    }
    i = init_s3_values(&gd->l.s3, gd->l.s3ind, gd->l.npart, bval, bval_width, norm);
    if(i)
        return i;

    /* compute long block specific values, ATH and MINVAL */
    j = 0;
    for(i = 0; i < gd->l.npart; i++) {
        double  x;

        /* ATH */
        x = FLOAT_MAX;
        for(k = 0; k < gd->l.numlines[i]; k++, j++) {
            FLOAT const freq = sfreq * j / (1000.0 * BLKSIZE);
            FLOAT   level;
            /* freq = Min(.1,freq); *//* ATH below 100 Hz constant, not further climbing */
            level = ATHformula(cfg, freq * 1000) - 20; /* scale to FFT units; returned value is in dB */
            level = pow(10., 0.1 * level); /* convert from dB -> energy */
            level *= gd->l.numlines[i];
            if(x > level)
                x = level;
        }
        gfc->ATH->cb_l[i] = x;

        /* MINVAL.
           For low freq, the strength of the masking is limited by minval
           this is an ISO MPEG1 thing, dont know if it is really needed */
        /* FIXME: it does work to reduce low-freq problems in S53-Wind-Sax
           and lead-voice samples, but introduces some 3 kbps bit bloat too.
           TODO: Further refinement of the shape of this hack.
         */
        x = 20.0 * (bval[i] / xav - 1.0);
        if(x > 6) {
            x = 30;
        }
        if(x < minval_low) {
            x = minval_low;
        }
        if(cfg->samplerate_out < 44000) {
            x = 30;
        }
        x -= 8.;
        gd->l.minval[i] = pow(10.0, x / 10.) * gd->l.numlines[i];
    }

    /************************************************************************
     * do the same things for short blocks
     ************************************************************************/
    init_numline(&gd->s, sfreq, BLKSIZE_s, 192, SBMAX_s, gfc->scalefac_band.s);
    assert(gd->s.npart < CBANDS);
    compute_bark_values(&gd->s, sfreq, BLKSIZE_s, bval, bval_width);

    /* SNR formula. short block is normalized by SNR. is it still right ? */
    j = 0;
    for(i = 0; i < gd->s.npart; i++) {
        double  x;
        double  snr = snr_s_a;
        if(bval[i] >= bvl_a) {
            snr = snr_s_b * (bval[i] - bvl_a) / (bvl_b - bvl_a)
                + snr_s_a * (bvl_b - bval[i]) / (bvl_b - bvl_a);
        }
        norm[i] = pow(10.0, snr / 10.0);

        /* ATH */
        x = FLOAT_MAX;
        for(k = 0; k < gd->s.numlines[i]; k++, j++) {
            FLOAT const freq = sfreq * j / (1000.0 * BLKSIZE_s);
            FLOAT   level;
            /* freq = Min(.1,freq); *//* ATH below 100 Hz constant, not further climbing */
            level = ATHformula(cfg, freq * 1000) - 20; /* scale to FFT units; returned value is in dB */
            level = pow(10., 0.1 * level); /* convert from dB -> energy */
            level *= gd->s.numlines[i];
            if(x > level)
                x = level;
        }
        gfc->ATH->cb_s[i] = x;

        /* MINVAL.
           For low freq, the strength of the masking is limited by minval
           this is an ISO MPEG1 thing, dont know if it is really needed */
        x = 7.0 * (bval[i] / xbv - 1.0);
        if(bval[i] > xbv) {
            x *= 1 + log(1 + x) * 3.1;
        }
        if(bval[i] < xbv) {
            x *= 1 + log(1 - x) * 2.3;
        }
        if(x > 6) {
            x = 30;
        }
        if(x < minval_low) {
            x = minval_low;
        }
        if(cfg->samplerate_out < 44000) {
            x = 30;
        }
        x -= 8;
        gd->s.minval[i] = pow(10.0, x / 10) * gd->s.numlines[i];
    }

    i = init_s3_values(&gd->s.s3, gd->s.s3ind, gd->s.npart, bval, bval_width, norm);
    if(i)
        return i;


    init_mask_add_max_values();
    init_fft(gfc);

    /* setup temporal masking */
    gd->decay = exp(-1.0 * LOG10 / (temporalmask_sustain_sec * sfreq / 192.0));

    {
        FLOAT   msfix;
        msfix = NS_MSFIX;
        if(cfg->use_safe_joint_stereo)
            msfix = 1.0;
        if(fabs(cfg->msfix) > 0.0)
            msfix = cfg->msfix;
        cfg->msfix = msfix;

        /* spread only from npart_l bands.  Normally, we use the spreading
         * function to convolve from npart_l down to npart_l bands 
         */
        for(b = 0; b < gd->l.npart; b++)
            if(gd->l.s3ind[b][1] > gd->l.npart - 1)
                gd->l.s3ind[b][1] = gd->l.npart - 1;
    }

    /*  prepare for ATH auto adjustment:
     *  we want to decrease the ATH by 12 dB per second
     */
#define  frame_duration (576. * cfg->mode_gr / sfreq)
    gfc->ATH->decay = pow(10., -12. / 10. * frame_duration);
    gfc->ATH->adjust_factor = 0.01; /* minimum, for leading low loudness */
    gfc->ATH->adjust_limit = 1.0; /* on lead, allow adjust up to maximum */
#undef  frame_duration

    assert(gd->l.bo[SBMAX_l - 1] <= gd->l.npart);
    assert(gd->s.bo[SBMAX_s - 1] <= gd->s.npart);

    if(cfg->ATHtype != -1) {
        /* compute equal loudness weights (eql_w) */
        FLOAT   freq;
        FLOAT const freq_inc = (FLOAT) cfg->samplerate_out / (FLOAT) (BLKSIZE);
        FLOAT   eql_balance = 0.0;
        freq = 0.0;
        for(i = 0; i < BLKSIZE / 2; ++i) {
            /* convert ATH dB to relative power (not dB) */
            /*  to determine eql_w */
            freq += freq_inc;
            gfc->ATH->eql_w[i] = 1. / pow(10, ATHformula(cfg, freq) / 10);
            eql_balance += gfc->ATH->eql_w[i];
        }
        eql_balance = 1.0 / eql_balance;
        for(i = BLKSIZE / 2; --i >= 0;) { /* scale weights */
            gfc->ATH->eql_w[i] *= eql_balance;
        }
    }
    {
        for(b = j = 0; b < gd->s.npart; ++b) {
            for(i = 0; i < gd->s.numlines[b]; ++i) {
                ++j;
            }
        }
        assert(j == 129);
        for(b = j = 0; b < gd->l.npart; ++b) {
            for(i = 0; i < gd->l.numlines[b]; ++i) {
                ++j;
            }
        }
        assert(j == 513);
    }
    /* short block attack threshold */
    {
        float   x = gfp->attackthre;
        float   y = gfp->attackthre_s;
        if(x < 0) {
            x = NSATTACKTHRE;
        }
        if(y < 0) {
            y = NSATTACKTHRE_S;
        }
        gd->attack_threshold[0] = gd->attack_threshold[1] = gd->attack_threshold[2] = x;
        gd->attack_threshold[3] = y;
    }
    {
        float   sk_s = -10.f, sk_l = -4.7f;
        static float const sk[] =
            { -7.4, -7.4, -7.4, -9.5, -7.4, -6.1, -5.5, -4.7, -4.7, -4.7, -4.7 };
        if(gfp->VBR_q < 4) {
            sk_l = sk_s = sk[0];
        }
        else {
            sk_l = sk_s = sk[gfp->VBR_q] + gfp->VBR_q_frac * (sk[gfp->VBR_q] - sk[gfp->VBR_q + 1]);
        }
        b = 0;
        for(; b < gd->s.npart; b++) {
            float   m = (float) (gd->s.npart - b) / gd->s.npart;
            gd->s.masking_lower[b] = powf(10.f, sk_s * m * 0.1f);
        }
        for(; b < CBANDS; ++b) {
            gd->s.masking_lower[b] = 1.f;
        }
        b = 0;
        for(; b < gd->l.npart; b++) {
            float   m = (float) (gd->l.npart - b) / gd->l.npart;
            gd->l.masking_lower[b] = powf(10.f, sk_l * m * 0.1f);
        }
        for(; b < CBANDS; ++b) {
            gd->l.masking_lower[b] = 1.f;
        }
    }
    memcpy(&gd->l_to_s, &gd->l, sizeof(gd->l_to_s));
    init_numline(&gd->l_to_s, sfreq, BLKSIZE, 192, SBMAX_s, gfc->scalefac_band.s);
    return 0;
	}




/*
 * MP3 quantization
 *
 *      Copyright (c) 1999-2000 Mark Taylor
 *      Copyright (c) 1999-2003 Takehiro Tominaga
 *      Copyright (c) 2000-2011 Robert Hegemann
 *      Copyright (c) 2001-2005 Gabriel Bouvigne
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.     See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id: quantize.c,v 1.219 2017/08/02 19:48:05 robert Exp $ */




/* convert from L/R <-> Mid/Side */
static void ms_convert(III_side_info_t * l3_side, int gr) {
    int     i;

    for(i = 0; i < 576; ++i) {
        FLOAT   l, r;
        l = l3_side->tt[gr][0].xr[i];
        r = l3_side->tt[gr][1].xr[i];
        l3_side->tt[gr][0].xr[i] = (l + r) * (FLOAT) (SQRT2 * 0.5);
        l3_side->tt[gr][1].xr[i] = (l - r) * (FLOAT) (SQRT2 * 0.5);
    }
	}

/************************************************************************
 *
 *      init_outer_loop()
 *  mt 6/99
 *
 *  initializes cod_info, scalefac and xrpow
 *
 *  returns 0 if all energies in xr are zero, else 1
 *
 ************************************************************************/
static void init_xrpow_core_c(gr_info * const cod_info, FLOAT xrpow[576], int upper, FLOAT * sum) {
    int     i;
    FLOAT   tmp;
    *sum = 0;

    for(i = 0; i <= upper; ++i) {
        tmp = fabs(cod_info->xr[i]);
        *sum += tmp;
        xrpow[i] = sqrt(tmp * sqrt(tmp));

        if(xrpow[i] > cod_info->xrpow_max)
            cod_info->xrpow_max = xrpow[i];
    }
	}



void init_xrpow_core_init(lame_internal_flags * const gfc) {
    gfc->init_xrpow_core = init_xrpow_core_c;

#if defined(HAVE_XMMINTRIN_H)
    if(gfc->CPU_features.SSE)
        gfc->init_xrpow_core = init_xrpow_core_sse;
#endif
#ifndef HAVE_NASM
#ifdef MIN_ARCH_SSE
    gfc->init_xrpow_core = init_xrpow_core_sse;
#endif
#endif
}



static int init_xrpow(lame_internal_flags *gfc, gr_info * const cod_info, FLOAT xrpow[576]) {
    FLOAT   sum = 0;
    int     i;
    int const upper = cod_info->max_nonzero_coeff;

    assert(xrpow != NULL);
    cod_info->xrpow_max = 0;

    /*  check if there is some energy we have to quantize
     *  and calculate xrpow matching our fresh scalefactors
     */
    assert(0 <= upper && upper <= 575);
    memset(&(xrpow[upper]), 0, (576 - upper) * sizeof(xrpow[0]));


    gfc->init_xrpow_core(cod_info, xrpow, upper, &sum);

    /*  return 1 if we have something to quantize, else 0
     */
    if(sum > (FLOAT) 1E-20) {
        int     j = 0;
        if(gfc->sv_qnt.substep_shaping & 2)
            j = 1;

        for(i = 0; i < cod_info->psymax; i++)
            gfc->sv_qnt.pseudohalf[i] = j;

        return 1;
    }

    memset(&cod_info->l3_enc[0], 0, sizeof(int) * 576);
    return 0;
	}





/*
Gabriel Bouvigne feb/apr 2003
Analog silence detection in partitionned sfb21
or sfb12 for short blocks

From top to bottom of sfb, changes to 0
coeffs which are below ath. It stops on the first
coeff higher than ath.
*/
static void psfb21_analogsilence(lame_internal_flags const *gfc, gr_info * const cod_info) {
    ATH_t const *const ATH = gfc->ATH;
    FLOAT  *const xr = cod_info->xr;

    if(cod_info->block_type != SHORT_TYPE) { /* NORM, START or STOP type, but not SHORT blocks */
        int     gsfb;
        int     stop = 0;
        for(gsfb = PSFB21 - 1; gsfb >= 0 && !stop; gsfb--) {
            int const start = gfc->scalefac_band.psfb21[gsfb];
            int const end = gfc->scalefac_band.psfb21[gsfb + 1];
            int     j;
            FLOAT   ath21;
            ath21 = athAdjust(ATH->adjust_factor, ATH->psfb21[gsfb], ATH->floor, 0);

            if(gfc->sv_qnt.longfact[21] > 1e-12f)
                ath21 *= gfc->sv_qnt.longfact[21];

            for(j = end - 1; j >= start; j--) {
                if(fabs(xr[j]) < ath21)
                    xr[j] = 0;
                else {
                    stop = 1;
                    break;
                }
            }
        }
		  }
    else {
        /*note: short blocks coeffs are reordered */
        int     block;
        for(block = 0; block < 3; block++) {

            int     gsfb;
            int     stop = 0;
            for(gsfb = PSFB12 - 1; gsfb >= 0 && !stop; gsfb--) {
                int const start = gfc->scalefac_band.s[12] * 3 +
                    (gfc->scalefac_band.s[13] - gfc->scalefac_band.s[12]) * block +
                    (gfc->scalefac_band.psfb12[gsfb] - gfc->scalefac_band.psfb12[0]);
                int const end =
                    start + (gfc->scalefac_band.psfb12[gsfb + 1] - gfc->scalefac_band.psfb12[gsfb]);
                int     j;
                FLOAT   ath12;
                ath12 = athAdjust(ATH->adjust_factor, ATH->psfb12[gsfb], ATH->floor, 0);

                if(gfc->sv_qnt.shortfact[12] > 1e-12f)
                    ath12 *= gfc->sv_qnt.shortfact[12];

                for(j = end - 1; j >= start; j--) {
                    if(fabs(xr[j]) < ath12)
                        xr[j] = 0;
                    else {
                        stop = 1;
                        break;
                    }
                }
            }
        }
    }

	}



static void init_outer_loop(lame_internal_flags const *gfc, gr_info * const cod_info) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     sfb, j;

    /*  initialize fresh cod_info
     */
    cod_info->part2_3_length = 0;
    cod_info->big_values = 0;
    cod_info->count1 = 0;
    cod_info->global_gain = 210;
    cod_info->scalefac_compress = 0;
    /* mixed_block_flag, block_type was set in psymodel.c */
    cod_info->table_select[0] = 0;
    cod_info->table_select[1] = 0;
    cod_info->table_select[2] = 0;
    cod_info->subblock_gain[0] = 0;
    cod_info->subblock_gain[1] = 0;
    cod_info->subblock_gain[2] = 0;
    cod_info->subblock_gain[3] = 0; /* this one is always 0 */
    cod_info->region0_count = 0;
    cod_info->region1_count = 0;
    cod_info->preflag = 0;
    cod_info->scalefac_scale = 0;
    cod_info->count1table_select = 0;
    cod_info->part2_length = 0;
    if(cfg->samplerate_out <= 8000) {
      cod_info->sfb_lmax = 17;
      cod_info->sfb_smin = 9;
      cod_info->psy_lmax = 17;
    }
    else {
      cod_info->sfb_lmax = SBPSY_l;
      cod_info->sfb_smin = SBPSY_s;
      cod_info->psy_lmax = gfc->sv_qnt.sfb21_extra ? SBMAX_l : SBPSY_l;
    }
    cod_info->psymax = cod_info->psy_lmax;
    cod_info->sfbmax = cod_info->sfb_lmax;
    cod_info->sfbdivide = 11;
    for(sfb = 0; sfb < SBMAX_l; sfb++) {
        cod_info->width[sfb]
            = gfc->scalefac_band.l[sfb + 1] - gfc->scalefac_band.l[sfb];
        cod_info->window[sfb] = 3; /* which is always 0. */
		  }
    if(cod_info->block_type == SHORT_TYPE) {
        FLOAT   ixwork[576];
        FLOAT  *ix;

        cod_info->sfb_smin = 0;
        cod_info->sfb_lmax = 0;
        if(cod_info->mixed_block_flag) {
            /*
             *  MPEG-1:      sfbs 0-7 long block, 3-12 short blocks
             *  MPEG-2(.5):  sfbs 0-5 long block, 3-12 short blocks
             */
            cod_info->sfb_smin = 3;
            cod_info->sfb_lmax = cfg->mode_gr * 2 + 4;
        }
        if(cfg->samplerate_out <= 8000) {
            cod_info->psymax
                = cod_info->sfb_lmax
                + 3 * (9 - cod_info->sfb_smin);
            cod_info->sfbmax = cod_info->sfb_lmax + 3 * (9 - cod_info->sfb_smin);
        }
        else {
            cod_info->psymax
                = cod_info->sfb_lmax
                + 3 * ((gfc->sv_qnt.sfb21_extra ? SBMAX_s : SBPSY_s) - cod_info->sfb_smin);
            cod_info->sfbmax = cod_info->sfb_lmax + 3 * (SBPSY_s - cod_info->sfb_smin);
        }
        cod_info->sfbdivide = cod_info->sfbmax - 18;
        cod_info->psy_lmax = cod_info->sfb_lmax;
        /* re-order the short blocks, for more efficient encoding below */
        /* By Takehiro TOMINAGA */
        /*
           Within each scalefactor band, data is given for successive
           time windows, beginning with window 0 and ending with window 2.
           Within each window, the quantized values are then arranged in
           order of increasing frequency...
         */
        ix = &cod_info->xr[gfc->scalefac_band.l[cod_info->sfb_lmax]];
        memcpy(ixwork, cod_info->xr, 576 * sizeof(FLOAT));
        for(sfb = cod_info->sfb_smin; sfb < SBMAX_s; sfb++) {
            int const start = gfc->scalefac_band.s[sfb];
            int const end = gfc->scalefac_band.s[sfb + 1];
            int     window, l;
            for(window = 0; window < 3; window++) {
                for(l = start; l < end; l++) {
                    *ix++ = ixwork[3 * l + window];
                }
            }
        }

        j = cod_info->sfb_lmax;
        for(sfb = cod_info->sfb_smin; sfb < SBMAX_s; sfb++) {
            cod_info->width[j] = cod_info->width[j + 1] = cod_info->width[j + 2]
                = gfc->scalefac_band.s[sfb + 1] - gfc->scalefac_band.s[sfb];
            cod_info->window[j] = 0;
            cod_info->window[j + 1] = 1;
            cod_info->window[j + 2] = 2;
            j += 3;
        }
    }

    cod_info->count1bits = 0;
    cod_info->sfb_partition_table = nr_of_sfb_block[0][0];
    cod_info->slen[0] = 0;
    cod_info->slen[1] = 0;
    cod_info->slen[2] = 0;
    cod_info->slen[3] = 0;

    cod_info->max_nonzero_coeff = 575;

    /*  fresh scalefactors are all zero
     */
    memset(cod_info->scalefac, 0, sizeof(cod_info->scalefac));

    if(cfg->vbr != vbr_mt && cfg->vbr != vbr_mtrh && cfg->vbr != vbr_abr && cfg->vbr != vbr_off) {
        psfb21_analogsilence(gfc, cod_info);
    }
	}



/************************************************************************
 *
 *      bin_search_StepSize()
 *
 *  author/date??
 *
 *  binary step size search
 *  used by outer_loop to get a quantizer step size to start with
 *
 ************************************************************************/

typedef enum {
    BINSEARCH_NONE,
    BINSEARCH_UP,
    BINSEARCH_DOWN
} binsearchDirection_t;

static int bin_search_StepSize(lame_internal_flags * const gfc, gr_info * const cod_info,
                    int desired_rate, const int ch, const FLOAT xrpow[576]) {
    int     nBits;
    int     CurrentStep = gfc->sv_qnt.CurrentStep[ch];
    int     flag_GoneOver = 0;
    int const start = gfc->sv_qnt.OldValue[ch];
    binsearchDirection_t Direction = BINSEARCH_NONE;
    cod_info->global_gain = start;
    desired_rate -= cod_info->part2_length;

    assert(CurrentStep);
    for(;;) {
        int     step;
        nBits = count_bits(gfc, xrpow, cod_info, 0);

        if(CurrentStep == 1 || nBits == desired_rate)
            break;      /* nothing to adjust anymore */

        if(nBits > desired_rate) {
            /* increase Quantize_StepSize */
            if(Direction == BINSEARCH_DOWN)
                flag_GoneOver = 1;

            if(flag_GoneOver)
                CurrentStep /= 2;
            Direction = BINSEARCH_UP;
            step = CurrentStep;
        }
        else {
            /* decrease Quantize_StepSize */
            if(Direction == BINSEARCH_UP)
                flag_GoneOver = 1;

            if(flag_GoneOver)
                CurrentStep /= 2;
            Direction = BINSEARCH_DOWN;
            step = -CurrentStep;
        }
        cod_info->global_gain += step;
        if(cod_info->global_gain < 0) {
            cod_info->global_gain = 0;
            flag_GoneOver = 1;
        }
        if(cod_info->global_gain > 255) {
            cod_info->global_gain = 255;
            flag_GoneOver = 1;
        }
    }

    assert(cod_info->global_gain >= 0);
    assert(cod_info->global_gain < 256);

    while(nBits > desired_rate && cod_info->global_gain < 255) {
        cod_info->global_gain++;
        nBits = count_bits(gfc, xrpow, cod_info, 0);
    }
    gfc->sv_qnt.CurrentStep[ch] = (start - cod_info->global_gain >= 4) ? 4 : 2;
    gfc->sv_qnt.OldValue[ch] = cod_info->global_gain;
    cod_info->part2_3_length = nBits;
  return nBits;
	}




/************************************************************************
 *
 *      trancate_smallspectrums()
 *
 *  Takehiro TOMINAGA 2002-07-21
 *
 *  trancate smaller nubmers into 0 as long as the noise threshold is allowed.
 *
 ************************************************************************/
static int floatcompare(const void *v1, const void *v2) {
    const FLOAT *const a = (const FLOAT *const)v1, *const b = (const FLOAT *const)v2;

    if(*a > *b)
        return 1;
    if(*a < *b)
        return -1;
    return 0;
	}

static void trancate_smallspectrums(lame_internal_flags const *gfc,
                        gr_info * const gi, const FLOAT * const l3_xmin, FLOAT * const work) {
    int     sfb, j, width;
    FLOAT   distort[SFBMAX];
    calc_noise_result dummy;

    if((!(gfc->sv_qnt.substep_shaping & 4) && gi->block_type == SHORT_TYPE)
        || gfc->sv_qnt.substep_shaping & 0x80)
        return;
    calc_noise(gi, l3_xmin, distort, &dummy, 0);
    for(j = 0; j < 576; j++) {
        FLOAT   xr = 0.0;
        if(gi->l3_enc[j] != 0)
            xr = fabs(gi->xr[j]);
        work[j] = xr;
    }

    j = 0;
    sfb = 8;
    if(gi->block_type == SHORT_TYPE)
        sfb = 6;
    do {
        FLOAT   allowedNoise, trancateThreshold;
        int     nsame, start;

        width = gi->width[sfb];
        j += width;
        if(distort[sfb] >= 1.0)
            continue;

        qsort(&work[j - width], width, sizeof(FLOAT), floatcompare);
        if(EQ(work[j - 1], 0.0))
            continue;   /* all zero sfb */

        allowedNoise = (1.0 - distort[sfb]) * l3_xmin[sfb];
        trancateThreshold = 0.0;
        start = 0;
        do {
            FLOAT   noise;
            for(nsame = 1; start + nsame < width; nsame++)
                if(NEQ(work[start + j - width], work[start + j + nsame - width]))
                    break;

            noise = work[start + j - width] * work[start + j - width] * nsame;
            if(allowedNoise < noise) {
                if(start != 0)
                    trancateThreshold = work[start + j - width - 1];
                break;
            }
            allowedNoise -= noise;
            start += nsame;
        } while(start < width);
        if(EQ(trancateThreshold, 0.0))
            continue;

/*      printf("%e %e %e\n", */
/*             trancateThreshold/l3_xmin[sfb], */
/*             trancateThreshold/(l3_xmin[sfb]*start), */
/*             trancateThreshold/(l3_xmin[sfb]*(start+width)) */
/*          ); */
/*      if(trancateThreshold > 1000*l3_xmin[sfb]*start) */
/*          trancateThreshold = 1000*l3_xmin[sfb]*start; */

        do {
            if(fabs(gi->xr[j - width]) <= trancateThreshold)
                gi->l3_enc[j - width] = 0;
        } while(--width > 0);
    } while(++sfb < gi->psymax);

  gi->part2_3_length = noquant_count_bits(gfc, gi, 0);
	}


/*************************************************************************
 *
 *      loop_break()
 *
 *  author/date??
 *
 *  Function: Returns zero if there is a scalefac which has not been
 *            amplified. Otherwise it returns one.
 *
 *************************************************************************/
inline static int loop_break(const gr_info * const cod_info) {
    int     sfb;

    for(sfb = 0; sfb < cod_info->sfbmax; sfb++)
        if(cod_info->scalefac[sfb]
            + cod_info->subblock_gain[cod_info->window[sfb]] == 0)
            return 0;

  return 1;
	}




/*  mt 5/99:  Function: Improved calc_noise for a single channel   */

/*************************************************************************
 *
 *      quant_compare()
 *
 *  author/date??
 *
 *  several different codes to decide which quantization is better
 *
 *************************************************************************/
static double penalties(double noise) {
    return FAST_LOG10(0.368 + 0.632 * noise * noise * noise);
	}

static double get_klemm_noise(const FLOAT * distort, const gr_info * const gi) {
    int     sfb;
    double  klemm_noise = 1E-37;

    for(sfb = 0; sfb < gi->psymax; sfb++)
        klemm_noise += penalties(distort[sfb]);

    return Max(1e-20, klemm_noise);
	}

inline static int quant_compare(const int quant_comp,
              const calc_noise_result * const best,
              calc_noise_result * const calc, const gr_info * const gi, const FLOAT * distort) {
    /*
       noise is given in decibels (dB) relative to masking thesholds.

       over_noise:  ??? (the previous comment is fully wrong)
       tot_noise:   ??? (the previous comment is fully wrong)
       max_noise:   max quantization noise

     */
    int     better;

    switch(quant_comp) {
    default:
    case 9:{
            if(best->over_count > 0) {
                /* there are distorted sfb */
                better = calc->over_SSD <= best->over_SSD;
                if(calc->over_SSD == best->over_SSD)
                    better = calc->bits < best->bits;
            }
            else {
                /* no distorted sfb */
                better = ((calc->max_noise < 0) &&
                          ((calc->max_noise * 10 + calc->bits) <=
                           (best->max_noise * 10 + best->bits)));
            }
            break;
        }

    case 0:
        better = calc->over_count < best->over_count
            || (calc->over_count == best->over_count && calc->over_noise < best->over_noise)
            || (calc->over_count == best->over_count &&
                EQ(calc->over_noise, best->over_noise) && calc->tot_noise < best->tot_noise);
        break;

    case 8:
        calc->max_noise = get_klemm_noise(distort, gi);
        /*lint --fallthrough */
    case 1:
        better = calc->max_noise < best->max_noise;
        break;
    case 2:
        better = calc->tot_noise < best->tot_noise;
        break;
    case 3:
        better = (calc->tot_noise < best->tot_noise)
            && (calc->max_noise < best->max_noise);
        break;
    case 4:
        better = (calc->max_noise <= 0.0 && best->max_noise > 0.2)
            || (calc->max_noise <= 0.0 &&
                best->max_noise < 0.0 &&
                best->max_noise > calc->max_noise - 0.2 && calc->tot_noise < best->tot_noise)
            || (calc->max_noise <= 0.0 &&
                best->max_noise > 0.0 &&
                best->max_noise > calc->max_noise - 0.2 &&
                calc->tot_noise < best->tot_noise + best->over_noise)
            || (calc->max_noise > 0.0 &&
                best->max_noise > -0.05 &&
                best->max_noise > calc->max_noise - 0.1 &&
                calc->tot_noise + calc->over_noise < best->tot_noise + best->over_noise)
            || (calc->max_noise > 0.0 &&
                best->max_noise > -0.1 &&
                best->max_noise > calc->max_noise - 0.15 &&
                calc->tot_noise + calc->over_noise + calc->over_noise <
                best->tot_noise + best->over_noise + best->over_noise);
        break;
    case 5:
        better = calc->over_noise < best->over_noise
            || (EQ(calc->over_noise, best->over_noise) && calc->tot_noise < best->tot_noise);
        break;
    case 6:
        better = calc->over_noise < best->over_noise
            || (EQ(calc->over_noise, best->over_noise) &&
                (calc->max_noise < best->max_noise
                 || (EQ(calc->max_noise, best->max_noise) && calc->tot_noise <= best->tot_noise)
                ));
        break;
    case 7:
        better = calc->over_count < best->over_count || calc->over_noise < best->over_noise;
        break;
    }


    if(best->over_count == 0) {
        /*
           If no distorted bands, only use this quantization
           if it is better, and if it uses less bits.
           Unfortunately, part2_3_length is sometimes a poor
           estimator of the final size at low bitrates.
         */
        better = better && calc->bits < best->bits;
    }


  return better;
	}



/*************************************************************************
 *
 *          amp_scalefac_bands()
 *
 *  author/date??
 *
 *  Amplify the scalefactor bands that violate the masking threshold.
 *  See ISO 11172-3 Section C.1.5.4.3.5
 *
 *  distort[] = noise/masking
 *  distort[] > 1   ==> noise is not masked
 *  distort[] < 1   ==> noise is masked
 *  max_dist = maximum value of distort[]
 *
 *  Three algorithms:
 *  noise_shaping_amp
 *        0             Amplify all bands with distort[]>1.
 *
 *        1             Amplify all bands with distort[] >= max_dist^(.5);
 *                     ( 50% in the db scale)
 *
 *        2             Amplify first band with distort[] >= max_dist;
 *
 *
 *  For algorithms 0 and 1, if max_dist < 1, then amplify all bands
 *  with distort[] >= .95*max_dist.  This is to make sure we always
 *  amplify at least one band.
 *
 *
 *************************************************************************/
static void amp_scalefac_bands(lame_internal_flags * gfc,
                   gr_info * const cod_info, FLOAT const *distort, FLOAT xrpow[576], int bRefine) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     j, sfb;
    FLOAT   ifqstep34, trigger;
    int     noise_shaping_amp;

    if(cod_info->scalefac_scale == 0) {
        ifqstep34 = 1.29683955465100964055; /* 2**(.75*.5) */
    }
    else {
        ifqstep34 = 1.68179283050742922612; /* 2**(.75*1) */
    }

    /* compute maximum value of distort[]  */
    trigger = 0;
    for(sfb = 0; sfb < cod_info->sfbmax; sfb++) {
        if(trigger < distort[sfb])
            trigger = distort[sfb];
    }

    noise_shaping_amp = cfg->noise_shaping_amp;
    if(noise_shaping_amp == 3) {
        if(bRefine == 1)
            noise_shaping_amp = 2;
        else
            noise_shaping_amp = 1;
    }
    switch(noise_shaping_amp) {
    case 2:
        /* amplify exactly 1 band */
        break;

    case 1:
        /* amplify bands within 50% of max (on db scale) */
        if(trigger > 1.0)
            trigger = pow(trigger, .5);
        else
            trigger *= .95;
        break;

    case 0:
    default:
        /* ISO algorithm.  amplify all bands with distort>1 */
        if(trigger > 1.0)
            trigger = 1.0;
        else
            trigger *= .95;
        break;
    }

    j = 0;
    for(sfb = 0; sfb < cod_info->sfbmax; sfb++) {
        int const width = cod_info->width[sfb];
        int     l;
        j += width;
        if(distort[sfb] < trigger)
            continue;

        if(gfc->sv_qnt.substep_shaping & 2) {
            gfc->sv_qnt.pseudohalf[sfb] = !gfc->sv_qnt.pseudohalf[sfb];
            if(!gfc->sv_qnt.pseudohalf[sfb] && cfg->noise_shaping_amp == 2)
                return;
        }
        cod_info->scalefac[sfb]++;
        for(l = -width; l < 0; l++) {
            xrpow[j + l] *= ifqstep34;
            if(xrpow[j + l] > cod_info->xrpow_max)
                cod_info->xrpow_max = xrpow[j + l];
        }

        if(cfg->noise_shaping_amp == 2)
            return;
    }
	}

/*************************************************************************
 *
 *      inc_scalefac_scale()
 *
 *  Takehiro Tominaga 2000-xx-xx
 *
 *  turns on scalefac scale and adjusts scalefactors
 *
 *************************************************************************/
static void inc_scalefac_scale(gr_info * const cod_info, FLOAT xrpow[576]) {
    int     l, j, sfb;
    const FLOAT ifqstep34 = 1.29683955465100964055;

    j = 0;
    for(sfb = 0; sfb < cod_info->sfbmax; sfb++) {
        int const width = cod_info->width[sfb];
        int     s = cod_info->scalefac[sfb];
        if(cod_info->preflag)
            s += pretab[sfb];
        j += width;
        if(s & 1) {
            s++;
            for(l = -width; l < 0; l++) {
                xrpow[j + l] *= ifqstep34;
                if(xrpow[j + l] > cod_info->xrpow_max)
                    cod_info->xrpow_max = xrpow[j + l];
            }
        }
        cod_info->scalefac[sfb] = s >> 1;
    }
    cod_info->preflag = 0;
    cod_info->scalefac_scale = 1;
}



/*************************************************************************
 *
 *      inc_subblock_gain()
 *
 *  Takehiro Tominaga 2000-xx-xx
 *
 *  increases the subblock gain and adjusts scalefactors
 *
 *************************************************************************/
static int inc_subblock_gain(const lame_internal_flags * const gfc, gr_info * const cod_info, FLOAT xrpow[576]) {
    int     sfb, window;
    int    *const scalefac = cod_info->scalefac;

    /* subbloc_gain can't do anything in the long block region */
    for(sfb = 0; sfb < cod_info->sfb_lmax; sfb++) {
        if(scalefac[sfb] >= 16)
            return 1;
    }

    for(window = 0; window < 3; window++) {
        int     s1, s2, l, j;
        s1 = s2 = 0;

        for(sfb = cod_info->sfb_lmax + window; sfb < cod_info->sfbdivide; sfb += 3) {
            if(s1 < scalefac[sfb])
                s1 = scalefac[sfb];
        }
        for(; sfb < cod_info->sfbmax; sfb += 3) {
            if(s2 < scalefac[sfb])
                s2 = scalefac[sfb];
        }

        if(s1 < 16 && s2 < 8)
            continue;

        if(cod_info->subblock_gain[window] >= 7)
            return 1;

        /* even though there is no scalefactor for sfb12
         * subblock gain affects upper frequencies too, that's why
         * we have to go up to SBMAX_s
         */
        cod_info->subblock_gain[window]++;
        j = gfc->scalefac_band.l[cod_info->sfb_lmax];
        for(sfb = cod_info->sfb_lmax + window; sfb < cod_info->sfbmax; sfb += 3) {
            FLOAT   amp;
            int const width = cod_info->width[sfb];
            int     s = scalefac[sfb];
            assert(s >= 0);
            s = s - (4 >> cod_info->scalefac_scale);
            if(s >= 0) {
                scalefac[sfb] = s;
                j += width * 3;
                continue;
            }

            scalefac[sfb] = 0;
            {
                int const gain = 210 + (s << (cod_info->scalefac_scale + 1));
                amp = IPOW20(gain);
            }
            j += width * (window + 1);
            for(l = -width; l < 0; l++) {
                xrpow[j + l] *= amp;
                if(xrpow[j + l] > cod_info->xrpow_max)
                    cod_info->xrpow_max = xrpow[j + l];
            }
            j += width * (3 - window - 1);
        }

        {
            FLOAT const amp = IPOW20(202);
            j += cod_info->width[sfb] * (window + 1);
            for(l = -cod_info->width[sfb]; l < 0; l++) {
                xrpow[j + l] *= amp;
                if(xrpow[j + l] > cod_info->xrpow_max)
                    cod_info->xrpow_max = xrpow[j + l];
            }
        }
    }
  return 0;
	}



/********************************************************************
 *
 *      balance_noise()
 *
 *  Takehiro Tominaga /date??
 *  Robert Hegemann 2000-09-06: made a function of it
 *
 *  amplifies scalefactor bands,
 *   - if all are already amplified returns 0
 *   - if some bands are amplified too much:
 *      * try to increase scalefac_scale
 *      * if already scalefac_scale was set
 *          try on short blocks to increase subblock gain
 *
 ********************************************************************/
inline static int balance_noise(lame_internal_flags * gfc,
              gr_info * const cod_info, FLOAT const *distort, FLOAT xrpow[576], int bRefine) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     status;

    amp_scalefac_bands(gfc, cod_info, distort, xrpow, bRefine);

    /* check to make sure we have not amplified too much
     * loop_break returns 0 if there is an unamplified scalefac
     * scale_bitcount returns 0 if no scalefactors are too large
     */

    status = loop_break(cod_info);

    if(status)
        return 0;       /* all bands amplified */

    /* not all scalefactors have been amplified.  so these
     * scalefacs are possibly valid.  encode them:
     */
    status = scale_bitcount(gfc, cod_info);

    if(!status)
        return 1;       /* amplified some bands not exceeding limits */

    /*  some scalefactors are too large.
     *  lets try setting scalefac_scale=1
     */
    if(cfg->noise_shaping > 1) {
        memset(&gfc->sv_qnt.pseudohalf[0], 0, sizeof(gfc->sv_qnt.pseudohalf));
        if(!cod_info->scalefac_scale) {
            inc_scalefac_scale(cod_info, xrpow);
            status = 0;
        }
        else {
            if(cod_info->block_type == SHORT_TYPE && cfg->subblock_gain > 0) {
                status = inc_subblock_gain(gfc, cod_info, xrpow)
                    || loop_break(cod_info);
            }
        }
    }

    if(!status) {
        status = scale_bitcount(gfc, cod_info);
    }

  return !status;
	}



/************************************************************************
 *
 *  outer_loop ()
 *
 *  Function: The outer iteration loop controls the masking conditions
 *  of all scalefactorbands. It computes the best scalefac and
 *  global gain. This module calls the inner iteration loop
 *
 *  mt 5/99 completely rewritten to allow for bit reservoir control,
 *  mid/side channels with L/R or mid/side masking thresholds,
 *  and chooses best quantization instead of last quantization when
 *  no distortion free quantization can be found.
 *
 *  added VBR support mt 5/99
 *
 *  some code shuffle rh 9/00
 ************************************************************************/
static int outer_loop(lame_internal_flags * gfc, gr_info * const cod_info, const FLOAT * const l3_xmin, /* allowed distortion */
           FLOAT xrpow[576], /* coloured magnitudes of spectral */
           const int ch, const int targ_bits) {                       /* maximum allowed bits */
    SessionConfig_t const *const cfg = &gfc->cfg;
    gr_info cod_info_w;
    FLOAT   save_xrpow[576];
    FLOAT   distort[SFBMAX];
    calc_noise_result best_noise_info;
    int     huff_bits;
    int     better;
    int     age;
    calc_noise_data prev_noise;
    int     best_part2_3_length = 9999999;
    int     bEndOfSearch = 0;
    int     bRefine = 0;
    int     best_ggain_pass1 = 0;

    bin_search_StepSize(gfc, cod_info, targ_bits, ch, xrpow);

    if(!cfg->noise_shaping)
        /* fast mode, no noise shaping, we are ready */
        return 100;     /* default noise_info.over_count */

    memset(&prev_noise, 0, sizeof(calc_noise_data));


    /* compute the distortion in this quantization */
    /* coefficients and thresholds both l/r (or both mid/side) */
    calc_noise(cod_info, l3_xmin, distort, &best_noise_info, &prev_noise);
    best_noise_info.bits = cod_info->part2_3_length;

    cod_info_w = *cod_info;
    age = 0;
    /* if(cfg->vbr == vbr_rh || cfg->vbr == vbr_mtrh) */
    memcpy(save_xrpow, xrpow, sizeof(FLOAT) * 576);

    while(!bEndOfSearch) {
        /* BEGIN MAIN LOOP */
        do {
            calc_noise_result noise_info;
            int     search_limit;
            int     maxggain = 255;

            /* When quantization with no distorted bands is found,
             * allow up to X new unsuccesful tries in serial. This
             * gives us more possibilities for different quant_compare modes.
             * Much more than 3 makes not a big difference, it is only slower.
             */

            if(gfc->sv_qnt.substep_shaping & 2) {
                search_limit = 20;
            }
            else {
                search_limit = 3;
            }



            /* Check if the last scalefactor band is distorted.
             * in VBR mode we can't get rid of the distortion, so quit now
             * and VBR mode will try again with more bits.
             * (makes a 10% speed increase, the files I tested were
             * binary identical, 2000/05/20 Robert Hegemann)
             * distort[] > 1 means noise > allowed noise
             */
            if(gfc->sv_qnt.sfb21_extra) {
                if(distort[cod_info_w.sfbmax] > 1.0)
                    break;
                if(cod_info_w.block_type == SHORT_TYPE
                    && (distort[cod_info_w.sfbmax + 1] > 1.0
                        || distort[cod_info_w.sfbmax + 2] > 1.0))
                    break;
            }

            /* try a new scalefactor conbination on cod_info_w */
            if(balance_noise(gfc, &cod_info_w, distort, xrpow, bRefine) == 0)
                break;
            if(cod_info_w.scalefac_scale)
                maxggain = 254;

            /* inner_loop starts with the initial quantization step computed above
             * and slowly increases until the bits < huff_bits.
             * Thus it is important not to start with too large of an inital
             * quantization step.  Too small is ok, but inner_loop will take longer
             */
            huff_bits = targ_bits - cod_info_w.part2_length;
            if(huff_bits <= 0)
                break;

            /*  increase quantizer stepsize until needed bits are below maximum
             */
            while((cod_info_w.part2_3_length
                    = count_bits(gfc, xrpow, &cod_info_w, &prev_noise)) > huff_bits
                   && cod_info_w.global_gain <= maxggain)
                cod_info_w.global_gain++;

            if(cod_info_w.global_gain > maxggain)
                break;

            if(best_noise_info.over_count == 0) {

                while((cod_info_w.part2_3_length
                        = count_bits(gfc, xrpow, &cod_info_w, &prev_noise)) > best_part2_3_length
                       && cod_info_w.global_gain <= maxggain)
                    cod_info_w.global_gain++;

                if(cod_info_w.global_gain > maxggain)
                    break;
            }

            /* compute the distortion in this quantization */
            calc_noise(&cod_info_w, l3_xmin, distort, &noise_info, &prev_noise);
            noise_info.bits = cod_info_w.part2_3_length;

            /* check if this quantization is better
             * than our saved quantization */
            if(cod_info->block_type != SHORT_TYPE) /* NORM, START or STOP type */
                better = cfg->quant_comp;
            else
                better = cfg->quant_comp_short;


            better = quant_compare(better, &best_noise_info, &noise_info, &cod_info_w, distort);


            /* save data so we can restore this quantization later */
            if(better) {
                best_part2_3_length = cod_info->part2_3_length;
                best_noise_info = noise_info;
                *cod_info = cod_info_w;
                age = 0;
                /* save data so we can restore this quantization later */
                /*if(cfg->vbr == vbr_rh || cfg->vbr == vbr_mtrh) */  {
                    /* store for later reuse */
                    memcpy(save_xrpow, xrpow, sizeof(FLOAT) * 576);
                }
            }
            else {
                /* early stop? */
                if(cfg->full_outer_loop == 0) {
                    if(++age > search_limit && best_noise_info.over_count == 0)
                        break;
                    if((cfg->noise_shaping_amp == 3) && bRefine && age > 30)
                        break;
                    if((cfg->noise_shaping_amp == 3) && bRefine &&
                        (cod_info_w.global_gain - best_ggain_pass1) > 15)
                        break;
                }
            }
        }         while((cod_info_w.global_gain + cod_info_w.scalefac_scale) < 255);

        if(cfg->noise_shaping_amp == 3) {
            if(!bRefine) {
                /* refine search */
                cod_info_w = *cod_info;
                memcpy(xrpow, save_xrpow, sizeof(FLOAT) * 576);
                age = 0;
                best_ggain_pass1 = cod_info_w.global_gain;

                bRefine = 1;
            }
            else {
                /* search already refined, stop */
                bEndOfSearch = 1;
            }

        }
        else {
            bEndOfSearch = 1;
        }
    }

    assert((cod_info->global_gain + cod_info->scalefac_scale) <= 255);
    /*  finish up
     */
    if(cfg->vbr == vbr_rh || cfg->vbr == vbr_mtrh || cfg->vbr == vbr_mt)
        /* restore for reuse on next try */
        memcpy(xrpow, save_xrpow, sizeof(FLOAT) * 576);
    /*  do the 'substep shaping'
     */
    else if(gfc->sv_qnt.substep_shaping & 1)
        trancate_smallspectrums(gfc, cod_info, l3_xmin, xrpow);

  return best_noise_info.over_count;
	}





/************************************************************************
 *
 *      iteration_finish_one()
 *
 *  Robert Hegemann 2000-09-06
 *
 *  update reservoir status after FINAL quantization/bitrate
 *
 ************************************************************************/

static void iteration_finish_one(lame_internal_flags * gfc, int gr, int ch) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    III_side_info_t *const l3_side = &gfc->l3_side;
    gr_info *const cod_info = &l3_side->tt[gr][ch];

    /*  try some better scalefac storage
     */
    best_scalefac_store(gfc, gr, ch, l3_side);

    /*  best huffman_divide may save some bits too
     */
    if(cfg->use_best_huffman == 1)
        best_huffman_divide(gfc, cod_info);

    /*  update reservoir status after FINAL quantization/bitrate
     */
    ResvAdjust(gfc, cod_info);
	}



/*********************************************************************
 *
 *      VBR_encode_granule()
 *
 *  2000-09-04 Robert Hegemann
 *
 *********************************************************************/

static void VBR_encode_granule(lame_internal_flags * gfc, gr_info * const cod_info, const FLOAT * const l3_xmin, /* allowed distortion of the scalefactor */
                   FLOAT xrpow[576], /* coloured magnitudes of spectral values */
                   const int ch, int min_bits, int max_bits) {
    gr_info bst_cod_info;
    FLOAT   bst_xrpow[576];
    int const Max_bits = max_bits;
    int     real_bits = max_bits + 1;
    int     this_bits = (max_bits + min_bits) / 2;
    int     dbits, over, found = 0;
    int const sfb21_extra = gfc->sv_qnt.sfb21_extra;

    assert(Max_bits <= MAX_BITS_PER_CHANNEL);
    memset(bst_cod_info.l3_enc, 0, sizeof(bst_cod_info.l3_enc));

    /*  search within round about 40 bits of optimal
     */
    do {
        assert(this_bits >= min_bits);
        assert(this_bits <= max_bits);
        assert(min_bits <= max_bits);

        if(this_bits > Max_bits - 42)
            gfc->sv_qnt.sfb21_extra = 0;
        else
            gfc->sv_qnt.sfb21_extra = sfb21_extra;

        over = outer_loop(gfc, cod_info, l3_xmin, xrpow, ch, this_bits);

        /*  is quantization as good as we are looking for ?
         *  in this case: is no scalefactor band distorted?
         */
        if(over <= 0) {
            found = 1;
            /*  now we know it can be done with "real_bits"
             *  and maybe we can skip some iterations
             */
            real_bits = cod_info->part2_3_length;

            /*  store best quantization so far
             */
            bst_cod_info = *cod_info;
            memcpy(bst_xrpow, xrpow, sizeof(FLOAT) * 576);

            /*  try with fewer bits
             */
            max_bits = real_bits - 32;
            dbits = max_bits - min_bits;
            this_bits = (max_bits + min_bits) / 2;
        }
        else {
            /*  try with more bits
             */
            min_bits = this_bits + 32;
            dbits = max_bits - min_bits;
            this_bits = (max_bits + min_bits) / 2;

            if(found) {
                found = 2;
                /*  start again with best quantization so far
                 */
                *cod_info = bst_cod_info;
                memcpy(xrpow, bst_xrpow, sizeof(FLOAT) * 576);
            }
        }
    } while(dbits > 12);

    gfc->sv_qnt.sfb21_extra = sfb21_extra;

    /*  found=0 => nothing found, use last one
     *  found=1 => we just found the best and left the loop
     *  found=2 => we restored a good one and have now l3_enc to restore too
     */
    if(found == 2) {
        memcpy(cod_info->l3_enc, bst_cod_info.l3_enc, sizeof(int) * 576);
    }
    assert(cod_info->part2_3_length <= Max_bits);

	}



/************************************************************************
 *
 *      get_framebits()
 *
 *  Robert Hegemann 2000-09-05
 *
 *  calculates
 *  * how many bits are available for analog silent granules
 *  * how many bits to use for the lowest allowed bitrate
 *  * how many bits each bitrate would provide
 *
 ************************************************************************/
static void get_framebits(lame_internal_flags * gfc, int frameBits[15]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t *const eov = &gfc->ov_enc;
    int     bitsPerFrame, i;

    /*  always use at least this many bits per granule per channel
     *  unless we detect analog silence, see below
     */
    eov->bitrate_index = cfg->vbr_min_bitrate_index;
    bitsPerFrame = getframebits(gfc);

    /*  bits for analog silence
     */
    eov->bitrate_index = 1;
    bitsPerFrame = getframebits(gfc);

    for(i = 1; i <= cfg->vbr_max_bitrate_index; i++) {
        eov->bitrate_index = i;
        frameBits[i] = ResvFrameBegin(gfc, &bitsPerFrame);
    }
	}



/*********************************************************************
 *
 *      VBR_prepare()
 *
 *  2000-09-04 Robert Hegemann
 *
 *  * converts LR to MS coding when necessary
 *  * calculates allowed/adjusted quantization noise amounts
 *  * detects analog silent frames
 *
 *  some remarks:
 *  - lower masking depending on Quality setting
 *  - quality control together with adjusted ATH MDCT scaling
 *    on lower quality setting allocate more noise from
 *    ATH masking, and on higher quality setting allocate
 *    less noise from ATH masking.
 *  - experiments show that going more than 2dB over GPSYCHO's
 *    limits ends up in very annoying artefacts
 *
 *********************************************************************/

/* RH: this one needs to be overhauled sometime */

static int VBR_old_prepare(lame_internal_flags * gfc,
                const FLOAT pe[2][2], FLOAT const ms_ener_ratio[2],
                const III_psy_ratio ratio[2][2],
                FLOAT l3_xmin[2][2][SFBMAX],
                int frameBits[16], int min_bits[2][2], int max_bits[2][2], int bands[2][2]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t *const eov = &gfc->ov_enc;

    FLOAT   masking_lower_db, adjust = 0.0;
    int     gr, ch;
    int     analog_silence = 1;
    int     avg, mxb, bits = 0;

    eov->bitrate_index = cfg->vbr_max_bitrate_index;
    avg = ResvFrameBegin(gfc, &avg) / cfg->mode_gr;

    get_framebits(gfc, frameBits);

    for(gr = 0; gr < cfg->mode_gr; gr++) {
        mxb = on_pe(gfc, pe, max_bits[gr], avg, gr, 0);
        if(gfc->ov_enc.mode_ext == MPG_MD_MS_LR) {
            ms_convert(&gfc->l3_side, gr);
            reduce_side(max_bits[gr], ms_ener_ratio[gr], avg, mxb);
        }
        for(ch = 0; ch < cfg->channels_out; ++ch) {
            gr_info *const cod_info = &gfc->l3_side.tt[gr][ch];

            if(cod_info->block_type != SHORT_TYPE) { /* NORM, START or STOP type */
                adjust = 1.28 / (1 + exp(3.5 - pe[gr][ch] / 300.)) - 0.05;
                masking_lower_db = gfc->sv_qnt.mask_adjust - adjust;
            }
            else {
                adjust = 2.56 / (1 + exp(3.5 - pe[gr][ch] / 300.)) - 0.14;
                masking_lower_db = gfc->sv_qnt.mask_adjust_short - adjust;
            }
            gfc->sv_qnt.masking_lower = pow(10.0, masking_lower_db * 0.1);

            init_outer_loop(gfc, cod_info);
            bands[gr][ch] = calc_xmin(gfc, &ratio[gr][ch], cod_info, l3_xmin[gr][ch]);
            if(bands[gr][ch])
                analog_silence = 0;

            min_bits[gr][ch] = 126;

            bits += max_bits[gr][ch];
        }
    }
    for(gr = 0; gr < cfg->mode_gr; gr++) {
        for(ch = 0; ch < cfg->channels_out; ch++) {
            if(bits > frameBits[cfg->vbr_max_bitrate_index] && bits > 0) {
                max_bits[gr][ch] *= frameBits[cfg->vbr_max_bitrate_index];
                max_bits[gr][ch] /= bits;
            }
            if(min_bits[gr][ch] > max_bits[gr][ch])
                min_bits[gr][ch] = max_bits[gr][ch];

        }               /* for ch */
    }                   /* for gr */

  return analog_silence;
	}

static void bitpressure_strategy(lame_internal_flags const *gfc,
                     FLOAT l3_xmin[2][2][SFBMAX], const int min_bits[2][2], int max_bits[2][2]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     gr, ch, sfb;

    for(gr = 0; gr < cfg->mode_gr; gr++) {
        for(ch = 0; ch < cfg->channels_out; ch++) {
            gr_info const *const gi = &gfc->l3_side.tt[gr][ch];
            FLOAT  *pxmin = l3_xmin[gr][ch];
            for(sfb = 0; sfb < gi->psy_lmax; sfb++)
                *pxmin++ *= 1. + .029 * sfb * sfb / SBMAX_l / SBMAX_l;

            if(gi->block_type == SHORT_TYPE) {
                for(sfb = gi->sfb_smin; sfb < SBMAX_s; sfb++) {
                    *pxmin++ *= 1. + .029 * sfb * sfb / SBMAX_s / SBMAX_s;
                    *pxmin++ *= 1. + .029 * sfb * sfb / SBMAX_s / SBMAX_s;
                    *pxmin++ *= 1. + .029 * sfb * sfb / SBMAX_s / SBMAX_s;
                }
            }
            max_bits[gr][ch] = Max(min_bits[gr][ch], 0.9 * max_bits[gr][ch]);
        }
    }
	}

/************************************************************************
 *
 *      VBR_iteration_loop()
 *
 *  tries to find out how many bits are needed for each granule and channel
 *  to get an acceptable quantization. An appropriate bitrate will then be
 *  choosed for quantization.  rh 8/99
 *
 *  Robert Hegemann 2000-09-06 rewrite
 *
 ************************************************************************/
void VBR_old_iteration_loop(lame_internal_flags * gfc, const FLOAT pe[2][2],
                       const FLOAT ms_ener_ratio[2], const III_psy_ratio ratio[2][2]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t *const eov = &gfc->ov_enc;
    FLOAT   l3_xmin[2][2][SFBMAX];

    FLOAT   xrpow[576];
    int     bands[2][2];
    int     frameBits[15];
    int     used_bits;
    int     bits;
    int     min_bits[2][2], max_bits[2][2];
    int     mean_bits;
    int     ch, gr, analog_silence;
    III_side_info_t *const l3_side = &gfc->l3_side;

    analog_silence = VBR_old_prepare(gfc, pe, ms_ener_ratio, ratio,
                                     l3_xmin, frameBits, min_bits, max_bits, bands);

    /*---------------------------------*/
    for(;;) {

        /*  quantize granules with lowest possible number of bits
         */

        used_bits = 0;

        for(gr = 0; gr < cfg->mode_gr; gr++) {
            for(ch = 0; ch < cfg->channels_out; ch++) {
                int     ret;
                gr_info *const cod_info = &l3_side->tt[gr][ch];

                /*  init_outer_loop sets up cod_info, scalefac and xrpow
                 */
                ret = init_xrpow(gfc, cod_info, xrpow);
                if(ret == 0 || max_bits[gr][ch] == 0) {
                    /*  xr contains no energy
                     *  l3_enc, our encoding data, will be quantized to zero
                     */
                    continue; /* with next channel */
                }

                VBR_encode_granule(gfc, cod_info, l3_xmin[gr][ch], xrpow,
                                   ch, min_bits[gr][ch], max_bits[gr][ch]);

                /*  do the 'substep shaping'
                 */
                if(gfc->sv_qnt.substep_shaping & 1) {
                    trancate_smallspectrums(gfc, &l3_side->tt[gr][ch], l3_xmin[gr][ch], xrpow);
                }

                ret = cod_info->part2_3_length + cod_info->part2_length;
                used_bits += ret;
            }           /* for ch */
        }               /* for gr */

        /*  find lowest bitrate able to hold used bits
         */
        if(analog_silence && !cfg->enforce_min_bitrate)
            /*  we detected analog silence and the user did not specify
             *  any hard framesize limit, so start with smallest possible frame
             */
            eov->bitrate_index = 1;
        else
            eov->bitrate_index = cfg->vbr_min_bitrate_index;

        for(; eov->bitrate_index < cfg->vbr_max_bitrate_index; eov->bitrate_index++) {
            if(used_bits <= frameBits[eov->bitrate_index])
                break;
        }
        bits = ResvFrameBegin(gfc, &mean_bits);

        if(used_bits <= bits)
            break;

        bitpressure_strategy(gfc, l3_xmin, (const int (*)[2])min_bits, max_bits);

    }                   /* breaks adjusted */
    /*--------------------------------------*/

    for(gr = 0; gr < cfg->mode_gr; gr++) {
        for(ch = 0; ch < cfg->channels_out; ch++) {
            iteration_finish_one(gfc, gr, ch);
        }               /* for ch */
    }                   /* for gr */
    ResvFrameEnd(gfc, mean_bits);
	}



static int VBR_new_prepare(lame_internal_flags * gfc,
                const FLOAT pe[2][2], const III_psy_ratio ratio[2][2],
                FLOAT l3_xmin[2][2][SFBMAX], int frameBits[16], int max_bits[2][2],
                int* max_resv) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t *const eov = &gfc->ov_enc;

    int     gr, ch;
    int     analog_silence = 1;
    int     avg, bits = 0;
    int     maximum_framebits;

    if(!cfg->free_format) {
        eov->bitrate_index = cfg->vbr_max_bitrate_index;
        ResvFrameBegin(gfc, &avg);
        *max_resv = gfc->sv_enc.ResvMax;

        get_framebits(gfc, frameBits);
        maximum_framebits = frameBits[cfg->vbr_max_bitrate_index];
    }
    else {
        eov->bitrate_index = 0;
        maximum_framebits = ResvFrameBegin(gfc, &avg);
        frameBits[0] = maximum_framebits;
        *max_resv = gfc->sv_enc.ResvMax;
    }

    for(gr = 0; gr < cfg->mode_gr; gr++) {
        on_pe(gfc, pe, max_bits[gr], avg, gr, 0);
        if(gfc->ov_enc.mode_ext == MPG_MD_MS_LR) {
            ms_convert(&gfc->l3_side, gr);
        }
        for(ch = 0; ch < cfg->channels_out; ++ch) {
            gr_info *const cod_info = &gfc->l3_side.tt[gr][ch];

            gfc->sv_qnt.masking_lower = pow(10.0, gfc->sv_qnt.mask_adjust * 0.1);

            init_outer_loop(gfc, cod_info);
            if(0 != calc_xmin(gfc, &ratio[gr][ch], cod_info, l3_xmin[gr][ch]))
                analog_silence = 0;

            bits += max_bits[gr][ch];
        }
    }
    for(gr = 0; gr < cfg->mode_gr; gr++) {
        for(ch = 0; ch < cfg->channels_out; ch++) {
            if(bits > maximum_framebits && bits > 0) {
                max_bits[gr][ch] *= maximum_framebits;
                max_bits[gr][ch] /= bits;
            }

        }               /* for ch */
    }                   /* for gr */
    if(analog_silence) {
        *max_resv = 0;
    }

  return analog_silence;
	}



void VBR_new_iteration_loop(lame_internal_flags * gfc, const FLOAT pe[2][2],
                       const FLOAT ms_ener_ratio[2], const III_psy_ratio ratio[2][2]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t *const eov = &gfc->ov_enc;
    FLOAT   l3_xmin[2][2][SFBMAX];

    FLOAT   xrpow[2][2][576];
    int     frameBits[15];
    int     used_bits;
    int     max_bits[2][2];
    int     ch, gr, analog_silence, pad;
    III_side_info_t *const l3_side = &gfc->l3_side;

    const FLOAT (*const_l3_xmin)[2][SFBMAX] = (const FLOAT (*)[2][SFBMAX])l3_xmin;
    const FLOAT (*const_xrpow)[2][576] = (const FLOAT (*)[2][576])xrpow;
    const int (*const_max_bits)[2] = (const int (*)[2])max_bits;
    
    ms_ener_ratio; /* not used */

    memset(xrpow, 0, sizeof(xrpow));

    analog_silence = VBR_new_prepare(gfc, pe, ratio, l3_xmin, frameBits, max_bits, &pad);

    for(gr = 0; gr < cfg->mode_gr; gr++) {
        for(ch = 0; ch < cfg->channels_out; ch++) {
            gr_info *const cod_info = &l3_side->tt[gr][ch];

            /*  init_outer_loop sets up cod_info, scalefac and xrpow
             */
            if(0 == init_xrpow(gfc, cod_info, xrpow[gr][ch])) {
                max_bits[gr][ch] = 0; /* silent granule needs no bits */
            }
        }               /* for ch */
			}                   /* for gr */

    /*  quantize granules with lowest possible number of bits
     */

    used_bits = VBR_encode_frame(gfc, const_xrpow, const_l3_xmin, const_max_bits);

    if(!cfg->free_format) {
        int     i, j;

        /*  find lowest bitrate able to hold used bits
         */
        if(analog_silence && !cfg->enforce_min_bitrate) {
            /*  we detected analog silence and the user did not specify
             *  any hard framesize limit, so start with smallest possible frame
             */
            i = 1;
        }
        else {
            i = cfg->vbr_min_bitrate_index;
        }

        for(; i < cfg->vbr_max_bitrate_index; i++) {
            if(used_bits <= frameBits[i]) 
                break;
        }
        if(i > cfg->vbr_max_bitrate_index) {
            i = cfg->vbr_max_bitrate_index;
        }
        if(pad > 0) {
            for(j = cfg->vbr_max_bitrate_index; j > i; --j) {
                int const unused = frameBits[j] - used_bits;
                if(unused <= pad) 
                    break;
            }
            eov->bitrate_index = j;
        }
        else {
            eov->bitrate_index = i;
        }
			}
    else {
#if 0
        static int mmm = 0;
        int     fff = getFramesize_kbps(gfc, used_bits);
        int     hhh = getFramesize_kbps(gfc, MAX_BITS_PER_GRANULE * cfg->mode_gr);
        if(mmm < fff)
            mmm = fff;
        printf("demand=%3d kbps  max=%3d kbps   limit=%3d kbps\n", fff, mmm, hhh);
#endif
        eov->bitrate_index = 0;
    }
    if(used_bits <= frameBits[eov->bitrate_index]) {
        /* update Reservoire status */
        int     mean_bits, fullframebits;
        fullframebits = ResvFrameBegin(gfc, &mean_bits);
        assert(used_bits <= fullframebits);
        for(gr = 0; gr < cfg->mode_gr; gr++) {
            for(ch = 0; ch < cfg->channels_out; ch++) {
                gr_info const *const cod_info = &l3_side->tt[gr][ch];
                ResvAdjust(gfc, cod_info);
            }
        }
        ResvFrameEnd(gfc, mean_bits);
    }
    else {
        /* SHOULD NOT HAPPEN INTERNAL ERROR
         */
        ERRORF(gfc, "INTERNAL ERROR IN VBR NEW CODE, please send bug report\n");
        exit(-1);
    }
	}





/********************************************************************
 *
 *  calc_target_bits()
 *
 *  calculates target bits for ABR encoding
 *
 *  mt 2000/05/31
 *
 ********************************************************************/
static void calc_target_bits(lame_internal_flags * gfc,
                 const FLOAT pe[2][2],
                 FLOAT const ms_ener_ratio[2],
                 int targ_bits[2][2], int *analog_silence_bits, int *max_frame_bits) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t *const eov = &gfc->ov_enc;
    III_side_info_t const *const l3_side = &gfc->l3_side;
    FLOAT   res_factor;
    int     gr, ch, totbits, mean_bits;
    int     framesize = 576 * cfg->mode_gr;

    eov->bitrate_index = cfg->vbr_max_bitrate_index;
    *max_frame_bits = ResvFrameBegin(gfc, &mean_bits);

    eov->bitrate_index = 1;
    mean_bits = getframebits(gfc) - cfg->sideinfo_len * 8;
    *analog_silence_bits = mean_bits / (cfg->mode_gr * cfg->channels_out);

    mean_bits = cfg->vbr_avg_bitrate_kbps * framesize * 1000;
    if(gfc->sv_qnt.substep_shaping & 1)
        mean_bits *= 1.09;
    mean_bits /= cfg->samplerate_out;
    mean_bits -= cfg->sideinfo_len * 8;
    mean_bits /= (cfg->mode_gr * cfg->channels_out);

    /*
       res_factor is the percentage of the target bitrate that should
       be used on average.  the remaining bits are added to the
       bitreservoir and used for difficult to encode frames.

       Since we are tracking the average bitrate, we should adjust
       res_factor "on the fly", increasing it if the average bitrate
       is greater than the requested bitrate, and decreasing it
       otherwise.  Reasonable ranges are from .9 to 1.0

       Until we get the above suggestion working, we use the following
       tuning:
       compression ratio    res_factor
       5.5  (256kbps)         1.0      no need for bitreservoir
       11   (128kbps)         .93      7% held for reservoir

       with linear interpolation for other values.

     */
    res_factor = .93 + .07 * (11.0 - cfg->compression_ratio) / (11.0 - 5.5);
    if(res_factor < .90)
        res_factor = .90;
    if(res_factor > 1.00)
        res_factor = 1.00;

    for(gr = 0; gr < cfg->mode_gr; gr++) {
        int     sum = 0;
        for(ch = 0; ch < cfg->channels_out; ch++) {
            targ_bits[gr][ch] = res_factor * mean_bits;

            if(pe[gr][ch] > 700) {
                int     add_bits = (pe[gr][ch] - 700) / 1.4;

                gr_info const *const cod_info = &l3_side->tt[gr][ch];
                targ_bits[gr][ch] = res_factor * mean_bits;

                /* short blocks use a little extra, no matter what the pe */
                if(cod_info->block_type == SHORT_TYPE) {
                    if(add_bits < mean_bits / 2)
                        add_bits = mean_bits / 2;
                }
                /* at most increase bits by 1.5*average */
                if(add_bits > mean_bits * 3 / 2)
                    add_bits = mean_bits * 3 / 2;
                else if(add_bits < 0)
                    add_bits = 0;

                targ_bits[gr][ch] += add_bits;
            }
            if(targ_bits[gr][ch] > MAX_BITS_PER_CHANNEL) {
                targ_bits[gr][ch] = MAX_BITS_PER_CHANNEL;
            }
            sum += targ_bits[gr][ch];
	        }               /* for ch */
        if(sum > MAX_BITS_PER_GRANULE) {
            for(ch = 0; ch < cfg->channels_out; ++ch) {
                targ_bits[gr][ch] *= MAX_BITS_PER_GRANULE;
                targ_bits[gr][ch] /= sum;
            }
        }
		  }                   /* for gr */

    if(gfc->ov_enc.mode_ext == MPG_MD_MS_LR)
        for(gr = 0; gr < cfg->mode_gr; gr++) {
            reduce_side(targ_bits[gr], ms_ener_ratio[gr], mean_bits * cfg->channels_out,
                        MAX_BITS_PER_GRANULE);
        }

    /*  sum target bits
     */
    totbits = 0;
    for(gr = 0; gr < cfg->mode_gr; gr++) {
        for(ch = 0; ch < cfg->channels_out; ch++) {
            if(targ_bits[gr][ch] > MAX_BITS_PER_CHANNEL)
                targ_bits[gr][ch] = MAX_BITS_PER_CHANNEL;
            totbits += targ_bits[gr][ch];
        }
    }

    /*  repartion target bits if needed
     */
    if(totbits > *max_frame_bits && totbits > 0) {
        for(gr = 0; gr < cfg->mode_gr; gr++) {
            for(ch = 0; ch < cfg->channels_out; ch++) {
                targ_bits[gr][ch] *= *max_frame_bits;
                targ_bits[gr][ch] /= totbits;
            }
        }
    }
	}





/********************************************************************
 *
 *  ABR_iteration_loop()
 *
 *  encode a frame with a disired average bitrate
 *
 *  mt 2000/05/31
 *
 ********************************************************************/
void ABR_iteration_loop(lame_internal_flags * gfc, const FLOAT pe[2][2],
                   const FLOAT ms_ener_ratio[2], const III_psy_ratio ratio[2][2]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t *const eov = &gfc->ov_enc;
    FLOAT   l3_xmin[SFBMAX];
    FLOAT   xrpow[576];
    int     targ_bits[2][2];
    int     mean_bits, max_frame_bits;
    int     ch, gr, ath_over;
    int     analog_silence_bits;
    gr_info *cod_info;
    III_side_info_t *const l3_side = &gfc->l3_side;

    mean_bits = 0;

    calc_target_bits(gfc, pe, ms_ener_ratio, targ_bits, &analog_silence_bits, &max_frame_bits);

    /*  encode granules
     */
    for(gr = 0; gr < cfg->mode_gr; gr++) {

        if(gfc->ov_enc.mode_ext == MPG_MD_MS_LR) {
            ms_convert(&gfc->l3_side, gr);
        }
        for(ch = 0; ch < cfg->channels_out; ch++) {
            FLOAT   adjust, masking_lower_db;
            cod_info = &l3_side->tt[gr][ch];

            if(cod_info->block_type != SHORT_TYPE) { /* NORM, START or STOP type */
                /* adjust = 1.28/(1+exp(3.5-pe[gr][ch]/300.))-0.05; */
                adjust = 0;
                masking_lower_db = gfc->sv_qnt.mask_adjust - adjust;
            }
            else {
                /* adjust = 2.56/(1+exp(3.5-pe[gr][ch]/300.))-0.14; */
                adjust = 0;
                masking_lower_db = gfc->sv_qnt.mask_adjust_short - adjust;
            }
            gfc->sv_qnt.masking_lower = pow(10.0, masking_lower_db * 0.1);


            /*  cod_info, scalefac and xrpow get initialized in init_outer_loop
             */
            init_outer_loop(gfc, cod_info);
            if(init_xrpow(gfc, cod_info, xrpow)) {
                /*  xr contains energy we will have to encode
                 *  calculate the masking abilities
                 *  find some good quantization in outer_loop
                 */
                ath_over = calc_xmin(gfc, &ratio[gr][ch], cod_info, l3_xmin);
                if(0 == ath_over) /* analog silence */
                    targ_bits[gr][ch] = analog_silence_bits;

                outer_loop(gfc, cod_info, l3_xmin, xrpow, ch, targ_bits[gr][ch]);
            }
            iteration_finish_one(gfc, gr, ch);
        }               /* ch */
    }                   /* gr */

    /*  find a bitrate which can refill the resevoir to positive size.
     */
    for(eov->bitrate_index = cfg->vbr_min_bitrate_index;
         eov->bitrate_index <= cfg->vbr_max_bitrate_index; eov->bitrate_index++) {
        if(ResvFrameBegin(gfc, &mean_bits) >= 0)
            break;
    }
    assert(eov->bitrate_index <= cfg->vbr_max_bitrate_index);

    ResvFrameEnd(gfc, mean_bits);
	}





/************************************************************************
 *
 *      CBR_iteration_loop()
 *
 *  author/date??
 *
 *  encodes one frame of MP3 data with constant bitrate
 *
 ************************************************************************/
void CBR_iteration_loop(lame_internal_flags * gfc, const FLOAT pe[2][2],
                   const FLOAT ms_ener_ratio[2], const III_psy_ratio ratio[2][2]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    FLOAT   l3_xmin[SFBMAX];
    FLOAT   xrpow[576];
    int     targ_bits[2];
    int     mean_bits, max_bits;
    int     gr, ch;
    III_side_info_t *const l3_side = &gfc->l3_side;
    gr_info *cod_info;

    ResvFrameBegin(gfc, &mean_bits);

    /* quantize! */
    for(gr = 0; gr < cfg->mode_gr; gr++) {

        /*  calculate needed bits
         */
        max_bits = on_pe(gfc, pe, targ_bits, mean_bits, gr, gr);

        if(gfc->ov_enc.mode_ext == MPG_MD_MS_LR) {
            ms_convert(&gfc->l3_side, gr);
            reduce_side(targ_bits, ms_ener_ratio[gr], mean_bits, max_bits);
        }

        for(ch = 0; ch < cfg->channels_out; ch++) {
            FLOAT   adjust, masking_lower_db;
            cod_info = &l3_side->tt[gr][ch];

            if(cod_info->block_type != SHORT_TYPE) { /* NORM, START or STOP type */
                /* adjust = 1.28/(1+exp(3.5-pe[gr][ch]/300.))-0.05; */
                adjust = 0;
                masking_lower_db = gfc->sv_qnt.mask_adjust - adjust;
            }
            else {
                /* adjust = 2.56/(1+exp(3.5-pe[gr][ch]/300.))-0.14; */
                adjust = 0;
                masking_lower_db = gfc->sv_qnt.mask_adjust_short - adjust;
            }
            gfc->sv_qnt.masking_lower = pow(10.0, masking_lower_db * 0.1);

            /*  init_outer_loop sets up cod_info, scalefac and xrpow
             */
            init_outer_loop(gfc, cod_info);
            if(init_xrpow(gfc, cod_info, xrpow)) {
                /*  xr contains energy we will have to encode
                 *  calculate the masking abilities
                 *  find some good quantization in outer_loop
                 */
                calc_xmin(gfc, &ratio[gr][ch], cod_info, l3_xmin);
                outer_loop(gfc, cod_info, l3_xmin, xrpow, ch, targ_bits[ch]);
            }

            iteration_finish_one(gfc, gr, ch);
            assert(cod_info->part2_3_length <= MAX_BITS_PER_CHANNEL);
            assert(cod_info->part2_3_length <= targ_bits[ch]);
        }               /* for ch */
    }                   /* for gr */

  ResvFrameEnd(gfc, mean_bits);
	}



/*
 *      quantize_pvt source file
 *
 *      Copyright (c) 1999-2002 Takehiro Tominaga
 *      Copyright (c) 2000-2012 Robert Hegemann
 *      Copyright (c) 2001 Naoki Shibata
 *      Copyright (c) 2002-2005 Gabriel Bouvigne
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

/* $Id: quantize_pvt.c,v 1.175 2017/09/06 15:07:30 robert Exp $ */


#define NSATHSCALE 100  /* Assuming dynamic range=96dB, this value should be 92 */

/*
  The following table is used to implement the scalefactor
  partitioning for MPEG2 as described in section
  2.4.3.2 of the IS. The indexing corresponds to the
  way the tables are presented in the IS:

  [table_number][row_in_table][column of nr_of_sfb]
*/
const int nr_of_sfb_block[6][3][4] = {
    {
     {6, 5, 5, 5},
     {9, 9, 9, 9},
     {6, 9, 9, 9}
     },
    {
     {6, 5, 7, 3},
     {9, 9, 12, 6},
     {6, 9, 12, 6}
     },
    {
     {11, 10, 0, 0},
     {18, 18, 0, 0},
     {15, 18, 0, 0}
     },
    {
     {7, 7, 7, 0},
     {12, 12, 12, 0},
     {6, 15, 12, 0}
     },
    {
     {6, 6, 6, 3},
     {12, 9, 9, 6},
     {6, 12, 9, 6}
     },
    {
     {8, 8, 5, 0},
     {15, 12, 9, 0},
     {6, 18, 9, 0}
     }
};


/* Table B.6: layer3 preemphasis */
const int pretab[SBMAX_l] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0
};

/*
  Here are MPEG1 Table B.8 and MPEG2 Table B.1
  -- Layer III scalefactor bands. 
  Index into this using a method such as:
    idx  = fr_ps->header->sampling_frequency
           + (fr_ps->header->version * 3)
*/


const scalefac_struct sfBandIndex[9] = {
    {                   /* Table B.2.b: 22.05 kHz */
     {0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464,
      522, 576},
     {0, 4, 8, 12, 18, 24, 32, 42, 56, 74, 100, 132, 174, 192}
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb21 pseudo sub bands */
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb12 pseudo sub bands */
     },
    {                   /* Table B.2.c: 24 kHz */ /* docs: 332. mpg123(broken): 330 */
     {0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 114, 136, 162, 194, 232, 278, 332, 394, 464,
      540, 576},
     {0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 136, 180, 192}
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb21 pseudo sub bands */
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb12 pseudo sub bands */
     },
    {                   /* Table B.2.a: 16 kHz */
     {0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464,
      522, 576},
     {0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192}
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb21 pseudo sub bands */
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb12 pseudo sub bands */
     },
    {                   /* Table B.8.b: 44.1 kHz */
     {0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 52, 62, 74, 90, 110, 134, 162, 196, 238, 288, 342, 418,
      576},
     {0, 4, 8, 12, 16, 22, 30, 40, 52, 66, 84, 106, 136, 192}
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb21 pseudo sub bands */
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb12 pseudo sub bands */
     },
    {                   /* Table B.8.c: 48 kHz */
     {0, 4, 8, 12, 16, 20, 24, 30, 36, 42, 50, 60, 72, 88, 106, 128, 156, 190, 230, 276, 330, 384,
      576},
     {0, 4, 8, 12, 16, 22, 28, 38, 50, 64, 80, 100, 126, 192}
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb21 pseudo sub bands */
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb12 pseudo sub bands */
     },
    {                   /* Table B.8.a: 32 kHz */
     {0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 54, 66, 82, 102, 126, 156, 194, 240, 296, 364, 448, 550,
      576},
     {0, 4, 8, 12, 16, 22, 30, 42, 58, 78, 104, 138, 180, 192}
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb21 pseudo sub bands */
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb12 pseudo sub bands */
     },
    {                   /* MPEG-2.5 11.025 kHz */
     {0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464,
      522, 576},
     {0 / 3, 12 / 3, 24 / 3, 36 / 3, 54 / 3, 78 / 3, 108 / 3, 144 / 3, 186 / 3, 240 / 3, 312 / 3,
      402 / 3, 522 / 3, 576 / 3}
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb21 pseudo sub bands */
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb12 pseudo sub bands */
     },
    {                   /* MPEG-2.5 12 kHz */
     {0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464,
      522, 576},
     {0 / 3, 12 / 3, 24 / 3, 36 / 3, 54 / 3, 78 / 3, 108 / 3, 144 / 3, 186 / 3, 240 / 3, 312 / 3,
      402 / 3, 522 / 3, 576 / 3}
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb21 pseudo sub bands */
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb12 pseudo sub bands */
     },
    {                   /* MPEG-2.5 8 kHz */
     {0, 12, 24, 36, 48, 60, 72, 88, 108, 132, 160, 192, 232, 280, 336, 400, 476, 566, 568, 570,
      572, 574, 576},
     {0 / 3, 24 / 3, 48 / 3, 72 / 3, 108 / 3, 156 / 3, 216 / 3, 288 / 3, 372 / 3, 480 / 3, 486 / 3,
      492 / 3, 498 / 3, 576 / 3}
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb21 pseudo sub bands */
     , {0, 0, 0, 0, 0, 0, 0} /*  sfb12 pseudo sub bands */
     }
};


/* FIXME: move global variables in some struct */

FLOAT   pow20[Q_MAX + Q_MAX2 + 1];
FLOAT   ipow20[Q_MAX];
FLOAT   pow43[PRECALC_SIZE];
/* initialized in first call to iteration_init */
#ifdef TAKEHIRO_IEEE754_HACK
FLOAT   adj43asm[PRECALC_SIZE];
#else
FLOAT   adj43[PRECALC_SIZE];
#endif

/* 
compute the ATH for each scalefactor band 
cd range:  0..96db

Input:  3.3kHz signal  32767 amplitude  (3.3kHz is where ATH is smallest = -5db)
longblocks:  sfb=12   en0/bw=-11db    max_en0 = 1.3db
shortblocks: sfb=5           -9db              0db

Input:  1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 (repeated)
longblocks:  amp=1      sfb=12   en0/bw=-103 db      max_en0 = -92db
            amp=32767   sfb=12           -12 db                 -1.4db 

Input:  1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 (repeated)
shortblocks: amp=1      sfb=5   en0/bw= -99                    -86 
            amp=32767   sfb=5           -9  db                  4db 


MAX energy of largest wave at 3.3kHz = 1db
AVE energy of largest wave at 3.3kHz = -11db
Let's take AVE:  -11db = maximum signal in sfb=12.  
Dynamic range of CD: 96db.  Therefor energy of smallest audible wave 
in sfb=12  = -11  - 96 = -107db = ATH at 3.3kHz.  

ATH formula for this wave: -5db.  To adjust to LAME scaling, we need
ATH = ATH_formula  - 103  (db)
ATH = ATH * 2.5e-10      (ener)

*/
static  FLOAT ATHmdct(SessionConfig_t const *cfg, FLOAT f) {
    FLOAT   ath;

    ath = ATHformula(cfg, f);

    if(cfg->ATHfixpoint > 0) {
        ath -= cfg->ATHfixpoint;
    }
    else {
        ath -= NSATHSCALE;
    }
    ath += cfg->ATH_offset_db;

    /* modify the MDCT scaling for the ATH and convert to energy */
    ath = powf(10.0f, ath * 0.1f);
    return ath;
	}

static void compute_ath(lame_internal_flags const* gfc) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    FLOAT  *const ATH_l = gfc->ATH->l;
    FLOAT  *const ATH_psfb21 = gfc->ATH->psfb21;
    FLOAT  *const ATH_s = gfc->ATH->s;
    FLOAT  *const ATH_psfb12 = gfc->ATH->psfb12;
    int     sfb, i, start, end;
    FLOAT   ATH_f;
    FLOAT const samp_freq = cfg->samplerate_out;

    for(sfb = 0; sfb < SBMAX_l; sfb++) {
        start = gfc->scalefac_band.l[sfb];
        end = gfc->scalefac_band.l[sfb + 1];
        ATH_l[sfb] = FLOAT_MAX;
        for(i = start; i < end; i++) {
            FLOAT const freq = i * samp_freq / (2 * 576);
            ATH_f = ATHmdct(cfg, freq); /* freq in kHz */
            ATH_l[sfb] = Min(ATH_l[sfb], ATH_f);
        }
    }

    for(sfb = 0; sfb < PSFB21; sfb++) {
        start = gfc->scalefac_band.psfb21[sfb];
        end = gfc->scalefac_band.psfb21[sfb + 1];
        ATH_psfb21[sfb] = FLOAT_MAX;
        for(i = start; i < end; i++) {
            FLOAT const freq = i * samp_freq / (2 * 576);
            ATH_f = ATHmdct(cfg, freq); /* freq in kHz */
            ATH_psfb21[sfb] = Min(ATH_psfb21[sfb], ATH_f);
        }
    }

    for(sfb = 0; sfb < SBMAX_s; sfb++) {
        start = gfc->scalefac_band.s[sfb];
        end = gfc->scalefac_band.s[sfb + 1];
        ATH_s[sfb] = FLOAT_MAX;
        for(i = start; i < end; i++) {
            FLOAT const freq = i * samp_freq / (2 * 192);
            ATH_f = ATHmdct(cfg, freq); /* freq in kHz */
            ATH_s[sfb] = Min(ATH_s[sfb], ATH_f);
        }
        ATH_s[sfb] *= (gfc->scalefac_band.s[sfb + 1] - gfc->scalefac_band.s[sfb]);
    }

    for(sfb = 0; sfb < PSFB12; sfb++) {
        start = gfc->scalefac_band.psfb12[sfb];
        end = gfc->scalefac_band.psfb12[sfb + 1];
        ATH_psfb12[sfb] = FLOAT_MAX;
        for(i = start; i < end; i++) {
            FLOAT const freq = i * samp_freq / (2 * 192);
            ATH_f = ATHmdct(cfg, freq); /* freq in kHz */
            ATH_psfb12[sfb] = Min(ATH_psfb12[sfb], ATH_f);
        }
        /*not sure about the following */
        ATH_psfb12[sfb] *= (gfc->scalefac_band.s[13] - gfc->scalefac_band.s[12]);
    }


    /*  no-ATH mode:
     *  reduce ATH to -200 dB
     */

    if(cfg->noATH) {
        for(sfb = 0; sfb < SBMAX_l; sfb++) {
            ATH_l[sfb] = 1E-20;
        }
        for(sfb = 0; sfb < PSFB21; sfb++) {
            ATH_psfb21[sfb] = 1E-20;
        }
        for(sfb = 0; sfb < SBMAX_s; sfb++) {
            ATH_s[sfb] = 1E-20;
        }
        for(sfb = 0; sfb < PSFB12; sfb++) {
            ATH_psfb12[sfb] = 1E-20;
        }
    }

    /*  work in progress, don't rely on it too much
     */
    gfc->ATH->floor = 10. * log10(ATHmdct(cfg, -1.));

    /*
       {   FLOAT g=10000, t=1e30, x;
       for( f = 100; f < 10000; f++ ) {
       x = ATHmdct( cfg, f );
       if( t > x ) t = x, g = f;
       }
       printf("min=%g\n", g);
       } */
	}


static float const payload_long[2][4] = {
	{-0.000f, -0.000f, -0.000f, +0.000f}
, {-0.500f, -0.250f, -0.025f, +0.500f}
};
static float const payload_short[2][4] = {
	{-0.000f, -0.000f, -0.000f, +0.000f}
, {-2.000f, -1.000f, -0.050f, +0.500f}
};

/************************************************************************/
/*  initialization for iteration_loop */
/************************************************************************/
void iteration_init(lame_internal_flags * gfc) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    III_side_info_t *const l3_side = &gfc->l3_side;
    FLOAT   adjust, db;
    int     i, sel;

    if(gfc->iteration_init_init == 0) {
        gfc->iteration_init_init = 1;

        l3_side->main_data_begin = 0;
        compute_ath(gfc);

        pow43[0] = 0.0;
        for(i = 1; i < PRECALC_SIZE; i++)
            pow43[i] = pow((FLOAT) i, 4.0 / 3.0);

#ifdef TAKEHIRO_IEEE754_HACK
        adj43asm[0] = 0.0;
        for(i = 1; i < PRECALC_SIZE; i++)
            adj43asm[i] = i - 0.5 - pow(0.5 * (pow43[i - 1] + pow43[i]), 0.75);
#else
        for(i = 0; i < PRECALC_SIZE - 1; i++)
            adj43[i] = (i + 1) - pow(0.5 * (pow43[i] + pow43[i + 1]), 0.75);
        adj43[i] = 0.5;
#endif
        for(i = 0; i < Q_MAX; i++)
            ipow20[i] = pow(2.0, (double) (i - 210) * -0.1875);
        for(i = 0; i <= Q_MAX + Q_MAX2; i++)
            pow20[i] = pow(2.0, (double) (i - 210 - Q_MAX2) * 0.25);

        huffman_init(gfc);
        init_xrpow_core_init(gfc);

        sel = 1;/* RH: all modes like vbr-new (cfg->vbr == vbr_mt || cfg->vbr == vbr_mtrh) ? 1 : 0;*/

        /* long */
        db = cfg->adjust_bass_db + payload_long[sel][0];
        adjust = powf(10.f, db * 0.1f);
        for(i = 0; i <= 6; ++i) {
            gfc->sv_qnt.longfact[i] = adjust;
        }
        db = cfg->adjust_alto_db + payload_long[sel][1];
        adjust = powf(10.f, db * 0.1f);
        for(; i <= 13; ++i) {
            gfc->sv_qnt.longfact[i] = adjust;
        }
        db = cfg->adjust_treble_db + payload_long[sel][2];
        adjust = powf(10.f, db * 0.1f);
        for(; i <= 20; ++i) {
            gfc->sv_qnt.longfact[i] = adjust;
        }
        db = cfg->adjust_sfb21_db + payload_long[sel][3];
        adjust = powf(10.f, db * 0.1f);
        for(; i < SBMAX_l; ++i) {
            gfc->sv_qnt.longfact[i] = adjust;
        }

        /* short */
        db = cfg->adjust_bass_db + payload_short[sel][0];
        adjust = powf(10.f, db * 0.1f);
        for(i = 0; i <= 2; ++i) {
            gfc->sv_qnt.shortfact[i] = adjust;
        }
        db = cfg->adjust_alto_db + payload_short[sel][1];
        adjust = powf(10.f, db * 0.1f);
        for(; i <= 6; ++i) {
            gfc->sv_qnt.shortfact[i] = adjust;
        }
        db = cfg->adjust_treble_db + payload_short[sel][2];
        adjust = powf(10.f, db * 0.1f);
        for(; i <= 11; ++i) {
            gfc->sv_qnt.shortfact[i] = adjust;
        }
        db = cfg->adjust_sfb21_db + payload_short[sel][3];
        adjust = powf(10.f, db * 0.1f);
        for(; i < SBMAX_s; ++i) {
            gfc->sv_qnt.shortfact[i] = adjust;
        }
    }
	}





/************************************************************************
 * allocate bits among 2 channels based on PE
 * mt 6/99
 * bugfixes rh 8/01: often allocated more than the allowed 4095 bits
 ************************************************************************/
int on_pe(lame_internal_flags * gfc, const FLOAT pe[][2], int targ_bits[2], int mean_bits, int gr, int cbr) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     extra_bits = 0, tbits, bits;
    int     add_bits[2] = {0, 0};
    int     max_bits;        /* maximum allowed bits for this granule */
    int     ch;

    /* allocate targ_bits for granule */
    ResvMaxBits(gfc, mean_bits, &tbits, &extra_bits, cbr);
    max_bits = tbits + extra_bits;
    if(max_bits > MAX_BITS_PER_GRANULE) /* hard limit per granule */
        max_bits = MAX_BITS_PER_GRANULE;

    for(bits = 0, ch = 0; ch < cfg->channels_out; ++ch) {
        /******************************************************************
         * allocate bits for each channel 
         ******************************************************************/
        targ_bits[ch] = Min(MAX_BITS_PER_CHANNEL, tbits / cfg->channels_out);

        add_bits[ch] = targ_bits[ch] * pe[gr][ch] / 700.0 - targ_bits[ch];

        /* at most increase bits by 1.5*average */
        if(add_bits[ch] > mean_bits * 3 / 4)
            add_bits[ch] = mean_bits * 3 / 4;
        if(add_bits[ch] < 0)
            add_bits[ch] = 0;

        if(add_bits[ch] + targ_bits[ch] > MAX_BITS_PER_CHANNEL)
            add_bits[ch] = Max(0, MAX_BITS_PER_CHANNEL - targ_bits[ch]);

        bits += add_bits[ch];
    }
    if(bits > extra_bits && bits > 0) {
        for(ch = 0; ch < cfg->channels_out; ++ch) {
            add_bits[ch] = extra_bits * add_bits[ch] / bits;
        }
    }

    for(ch = 0; ch < cfg->channels_out; ++ch) {
        targ_bits[ch] += add_bits[ch];
        extra_bits -= add_bits[ch];
    }

    for(bits = 0, ch = 0; ch < cfg->channels_out; ++ch) {
        bits += targ_bits[ch];
    }
    if(bits > MAX_BITS_PER_GRANULE) {
        int     sum = 0;
        for(ch = 0; ch < cfg->channels_out; ++ch) {
            targ_bits[ch] *= MAX_BITS_PER_GRANULE;
            targ_bits[ch] /= bits;
            sum += targ_bits[ch];
        }
        assert(sum <= MAX_BITS_PER_GRANULE);
    }

  return max_bits;
	}



void reduce_side(int targ_bits[2], FLOAT ms_ener_ratio, int mean_bits, int max_bits) {
    int     move_bits;
    FLOAT   fac;

    assert(max_bits <= MAX_BITS_PER_GRANULE);
    assert(targ_bits[0] + targ_bits[1] <= MAX_BITS_PER_GRANULE);

    /*  ms_ener_ratio = 0:  allocate 66/33  mid/side  fac=.33  
     *  ms_ener_ratio =.5:  allocate 50/50 mid/side   fac= 0 */
    /* 75/25 split is fac=.5 */
    /* float fac = .50*(.5-ms_ener_ratio[gr])/.5; */
    fac = .33 * (.5 - ms_ener_ratio) / .5;
    if(fac < 0)
        fac = 0;
    if(fac > .5)
        fac = .5;

    /* number of bits to move from side channel to mid channel */
    /*    move_bits = fac*targ_bits[1];  */
    move_bits = fac * .5 * (targ_bits[0] + targ_bits[1]);

    if(move_bits > MAX_BITS_PER_CHANNEL - targ_bits[0]) {
        move_bits = MAX_BITS_PER_CHANNEL - targ_bits[0];
    }
    if(move_bits < 0)
        move_bits = 0;

    if(targ_bits[1] >= 125) {
        /* dont reduce side channel below 125 bits */
        if(targ_bits[1] - move_bits > 125) {

            /* if mid channel already has 2x more than average, dont bother */
            /* mean_bits = bits per granule (for both channels) */
            if(targ_bits[0] < mean_bits)
                targ_bits[0] += move_bits;
            targ_bits[1] -= move_bits;
        }
        else {
            targ_bits[0] += targ_bits[1] - 125;
            targ_bits[1] = 125;
        }
    }

    move_bits = targ_bits[0] + targ_bits[1];
    if(move_bits > max_bits) {
        targ_bits[0] = (max_bits * targ_bits[0]) / move_bits;
        targ_bits[1] = (max_bits * targ_bits[1]) / move_bits;
    }
    assert(targ_bits[0] <= MAX_BITS_PER_CHANNEL);
    assert(targ_bits[1] <= MAX_BITS_PER_CHANNEL);
    assert(targ_bits[0] + targ_bits[1] <= MAX_BITS_PER_GRANULE);
	}


/**
 *  Robert Hegemann 2001-04-27:
 *  this adjusts the ATH, keeping the original noise floor
 *  affects the higher frequencies more than the lower ones
 */
FLOAT athAdjust(FLOAT a, FLOAT x, FLOAT athFloor, float ATHfixpoint) {
    /*  work in progress
     */
    FLOAT const o = 90.30873362f;
    FLOAT const p = (ATHfixpoint < 1.f) ? 94.82444863f : ATHfixpoint;
    FLOAT   u = FAST_LOG10_X(x, 10.0f);
    FLOAT const v = a * a;
    FLOAT   w = 0.0f;
    u -= athFloor;      /* undo scaling */
    if(v > 1E-20f)
        w = 1.f + FAST_LOG10_X(v, 10.0f / o);
    if(w < 0)
        w = 0.f;
    u *= w;
    u += athFloor + o - p; /* redo scaling */

  return powf(10.f, 0.1f * u);
	}



/*************************************************************************/
/*            calc_xmin                                                  */
/*************************************************************************/

/*
  Calculate the allowed distortion for each scalefactor band,
  as determined by the psychoacoustic model.
  xmin(sb) = ratio(sb) * en(sb) / bw(sb)

  returns number of sfb's with energy > ATH
*/
int calc_xmin(lame_internal_flags const *gfc,
          III_psy_ratio const *const ratio, gr_info * const cod_info, FLOAT * pxmin) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     sfb, gsfb, j = 0, ath_over = 0, k;
    ATH_t const *const ATH = gfc->ATH;
    const FLOAT *const xr = cod_info->xr;
    int     max_nonzero;

    for(gsfb = 0; gsfb < cod_info->psy_lmax; gsfb++) {
        FLOAT   en0, xmin;
        FLOAT   rh1, rh2, rh3;
        int     width, l;

        xmin = athAdjust(ATH->adjust_factor, ATH->l[gsfb], ATH->floor, cfg->ATHfixpoint);
        xmin *= gfc->sv_qnt.longfact[gsfb];

        width = cod_info->width[gsfb];
        rh1 = xmin / width;
#ifdef DBL_EPSILON
        rh2 = DBL_EPSILON;
#else
        rh2 = 2.2204460492503131e-016;
#endif
        en0 = 0.0;
        for(l = 0; l < width; ++l) {
            FLOAT const xa = xr[j++];
            FLOAT const x2 = xa * xa;
            en0 += x2;
            rh2 += (x2 < rh1) ? x2 : rh1;
        }
        if(en0 > xmin)
            ath_over++;

        if(en0 < xmin) {
            rh3 = en0;
        }
        else if(rh2 < xmin) {
            rh3 = xmin;
        }
        else {
            rh3 = rh2;
        }
        xmin = rh3;
        {
            FLOAT const e = ratio->en.l[gsfb];
            if(e > 1e-12f) {
                FLOAT   x;
                x = en0 * ratio->thm.l[gsfb] / e;
                x *= gfc->sv_qnt.longfact[gsfb];
                if(xmin < x)
                    xmin = x;
            }
        }
        xmin = Max(xmin, DBL_EPSILON);
        cod_info->energy_above_cutoff[gsfb] = (en0 > xmin+1e-14f) ? 1 : 0;
        *pxmin++ = xmin;
    }                   /* end of long block loop */




    /*use this function to determine the highest non-zero coeff */
    max_nonzero = 0;
    for(k = 575; k > 0; --k) {
        if(fabs(xr[k]) > 1e-12f) {
            max_nonzero = k;
            break;
        }
    }
    if(cod_info->block_type != SHORT_TYPE) { /* NORM, START or STOP type, but not SHORT */
        max_nonzero |= 1; /* only odd numbers */
    }
    else {
        max_nonzero /= 6; /* 3 short blocks */
        max_nonzero *= 6;
        max_nonzero += 5;
    }

    if(gfc->sv_qnt.sfb21_extra == 0 && cfg->samplerate_out < 44000) {
      int const sfb_l = (cfg->samplerate_out <= 8000) ? 17 : 21;
      int const sfb_s = (cfg->samplerate_out <= 8000) ?  9 : 12;
      int   limit = 575;
      if(cod_info->block_type != SHORT_TYPE) { /* NORM, START or STOP type, but not SHORT */
          limit = gfc->scalefac_band.l[sfb_l]-1;
      }
      else {
          limit = 3*gfc->scalefac_band.s[sfb_s]-1;
      }
      if(max_nonzero > limit) {
          max_nonzero = limit;
      }
    }
    cod_info->max_nonzero_coeff = max_nonzero;



    for(sfb = cod_info->sfb_smin; gsfb < cod_info->psymax; sfb++, gsfb += 3) {
        int     width, b, l;
        FLOAT   tmpATH;

        tmpATH = athAdjust(ATH->adjust_factor, ATH->s[sfb], ATH->floor, cfg->ATHfixpoint);
        tmpATH *= gfc->sv_qnt.shortfact[sfb];
        
        width = cod_info->width[gsfb];
        for(b = 0; b < 3; b++) {
            FLOAT   en0 = 0.0, xmin = tmpATH;
            FLOAT   rh1, rh2, rh3;

            rh1 = tmpATH / width;
#ifdef DBL_EPSILON
            rh2 = DBL_EPSILON;
#else
            rh2 = 2.2204460492503131e-016;
#endif
            for(l = 0; l < width; ++l) {
                FLOAT const xa = xr[j++];
                FLOAT const x2 = xa * xa;
                en0 += x2;
                rh2 += (x2 < rh1) ? x2 : rh1;
            }
            if(en0 > tmpATH)
                ath_over++;
            
            if(en0 < tmpATH) {
                rh3 = en0;
            }
            else if(rh2 < tmpATH) {
                rh3 = tmpATH;
            }
            else {
                rh3 = rh2;
            }
            xmin = rh3;
            {
                FLOAT const e = ratio->en.s[sfb][b];
                if(e > 1e-12f) {
                    FLOAT   x;
                    x = en0 * ratio->thm.s[sfb][b] / e;
                    x *= gfc->sv_qnt.shortfact[sfb];
                    if(xmin < x)
                        xmin = x;
                }
            }
            xmin = Max(xmin, DBL_EPSILON);
            cod_info->energy_above_cutoff[gsfb+b] = (en0 > xmin+1e-14f) ? 1 : 0;
            *pxmin++ = xmin;
        }               /* b */
        if(cfg->use_temporal_masking_effect) {
            if(pxmin[-3] > pxmin[-3 + 1])
                pxmin[-3 + 1] += (pxmin[-3] - pxmin[-3 + 1]) * gfc->cd_psy->decay;
            if(pxmin[-3 + 1] > pxmin[-3 + 2])
                pxmin[-3 + 2] += (pxmin[-3 + 1] - pxmin[-3 + 2]) * gfc->cd_psy->decay;
        }
    }                   /* end of short block sfb loop */

  return ath_over;
	}


static  FLOAT calc_noise_core_c(const gr_info * const cod_info, int *startline, int l, FLOAT step) {
    FLOAT   noise = 0;
    int     j = *startline;
    const int *const ix = cod_info->l3_enc;

    if(j > cod_info->count1) {
        while(l--) {
            FLOAT   temp;
            temp = cod_info->xr[j];
            j++;
            noise += temp * temp;
            temp = cod_info->xr[j];
            j++;
            noise += temp * temp;
        }
    }
    else if(j > cod_info->big_values) {
        FLOAT   ix01[2];
        ix01[0] = 0;
        ix01[1] = step;
        while(l--) {
            FLOAT   temp;
            temp = fabs(cod_info->xr[j]) - ix01[ix[j]];
            j++;
            noise += temp * temp;
            temp = fabs(cod_info->xr[j]) - ix01[ix[j]];
            j++;
            noise += temp * temp;
        }
    }
    else {
        while(l--) {
            FLOAT   temp;
            temp = fabs(cod_info->xr[j]) - pow43[ix[j]] * step;
            j++;
            noise += temp * temp;
            temp = fabs(cod_info->xr[j]) - pow43[ix[j]] * step;
            j++;
            noise += temp * temp;
        }
    }

    *startline = j;

  return noise;
	}


/*************************************************************************/
/*            calc_noise                                                 */
/*************************************************************************/

/* -oo dB  =>  -1.00 */
/* - 6 dB  =>  -0.97 */
/* - 3 dB  =>  -0.80 */
/* - 2 dB  =>  -0.64 */
/* - 1 dB  =>  -0.38 */
/*   0 dB  =>   0.00 */
/* + 1 dB  =>  +0.49 */
/* + 2 dB  =>  +1.06 */
/* + 3 dB  =>  +1.68 */
/* + 6 dB  =>  +3.69 */
/* +10 dB  =>  +6.45 */
int calc_noise(gr_info const *const cod_info,
           FLOAT const *l3_xmin,
           FLOAT * distort, calc_noise_result * const res, calc_noise_data * prev_noise) {
    int     sfb, l, over = 0;
    FLOAT   over_noise_db = 0;
    FLOAT   tot_noise_db = 0; /*    0 dB relative to masking */
    FLOAT   max_noise = -20.0; /* -200 dB relative to masking */
    int     j = 0;
    const int *scalefac = cod_info->scalefac;

    res->over_SSD = 0;


    for(sfb = 0; sfb < cod_info->psymax; sfb++) {
        int const s =
            cod_info->global_gain - (((*scalefac++) + (cod_info->preflag ? pretab[sfb] : 0))
                                     << (cod_info->scalefac_scale + 1))
            - cod_info->subblock_gain[cod_info->window[sfb]] * 8;
        FLOAT const r_l3_xmin = 1.f / *l3_xmin++;
        FLOAT   distort_ = 0.0f;
        FLOAT   noise = 0.0f;

        if(prev_noise && (prev_noise->step[sfb] == s)) {

            /* use previously computed values */
            j += cod_info->width[sfb];
            distort_ = r_l3_xmin * prev_noise->noise[sfb];

            noise = prev_noise->noise_log[sfb];

        }
        else {
            FLOAT const step = POW20(s);
            l = cod_info->width[sfb] >> 1;

            if((j + cod_info->width[sfb]) > cod_info->max_nonzero_coeff) {
                int     usefullsize;
                usefullsize = cod_info->max_nonzero_coeff - j + 1;

                if(usefullsize > 0)
                    l = usefullsize >> 1;
                else
                    l = 0;
            }

            noise = calc_noise_core_c(cod_info, &j, l, step);


            if(prev_noise) {
                /* save noise values */
                prev_noise->step[sfb] = s;
                prev_noise->noise[sfb] = noise;
            }

            distort_ = r_l3_xmin * noise;

            /* multiplying here is adding in dB, but can overflow */
            noise = FAST_LOG10(Max(distort_, 1E-20f));

            if(prev_noise) {
                /* save noise values */
                prev_noise->noise_log[sfb] = noise;
            }
        }
        *distort++ = distort_;

        if(prev_noise) {
            /* save noise values */
            prev_noise->global_gain = cod_info->global_gain;;
        }


        /*tot_noise *= Max(noise, 1E-20); */
        tot_noise_db += noise;

        if(noise > 0.0) {
            int     tmp;

            tmp = Max((int) (noise * 10 + .5), 1);
            res->over_SSD += tmp * tmp;

            over++;
            /* multiplying here is adding in dB -but can overflow */
            /*over_noise *= noise; */
            over_noise_db += noise;
        }
        max_noise = Max(max_noise, noise);

    }

    res->over_count = over;
    res->tot_noise = tot_noise_db;
    res->over_noise = over_noise_db;
    res->max_noise = max_noise;

  return over;
	}





/************************************************************************
 *
 *  set_pinfo()
 *
 *  updates plotting data    
 *
 *  Mark Taylor 2000-??-??                
 *
 *  Robert Hegemann: moved noise/distortion calc into it
 *
 ************************************************************************/
static void set_pinfo(lame_internal_flags const *gfc,
          gr_info * const cod_info, const III_psy_ratio * const ratio, const int gr, const int ch) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     sfb, sfb2;
    int     j, i, l, start, end, bw;
    FLOAT   en0, en1;
    FLOAT const ifqstep = (cod_info->scalefac_scale == 0) ? .5 : 1.0;
    int const *const scalefac = cod_info->scalefac;

    FLOAT   l3_xmin[SFBMAX], xfsf[SFBMAX];
    calc_noise_result noise;

    calc_xmin(gfc, ratio, cod_info, l3_xmin);
    calc_noise(cod_info, l3_xmin, xfsf, &noise, 0);

    j = 0;
    sfb2 = cod_info->sfb_lmax;
    if(cod_info->block_type != SHORT_TYPE && !cod_info->mixed_block_flag)
        sfb2 = 22;
    for(sfb = 0; sfb < sfb2; sfb++) {
        start = gfc->scalefac_band.l[sfb];
        end = gfc->scalefac_band.l[sfb + 1];
        bw = end - start;
        for(en0 = 0.0; j < end; j++)
            en0 += cod_info->xr[j] * cod_info->xr[j];
        en0 /= bw;
        /* convert to MDCT units */
        en1 = 1e15;     /* scaling so it shows up on FFT plot */
        gfc->pinfo->en[gr][ch][sfb] = en1 * en0;
        gfc->pinfo->xfsf[gr][ch][sfb] = en1 * l3_xmin[sfb] * xfsf[sfb] / bw;

        if(ratio->en.l[sfb] > 0 && !cfg->ATHonly)
            en0 = en0 / ratio->en.l[sfb];
        else
            en0 = 0.0;

        gfc->pinfo->thr[gr][ch][sfb] = en1 * Max(en0 * ratio->thm.l[sfb], gfc->ATH->l[sfb]);

        /* there is no scalefactor bands >= SBPSY_l */
        gfc->pinfo->LAMEsfb[gr][ch][sfb] = 0;
        if(cod_info->preflag && sfb >= 11)
            gfc->pinfo->LAMEsfb[gr][ch][sfb] = -ifqstep * pretab[sfb];

        if(sfb < SBPSY_l) {
            assert(scalefac[sfb] >= 0); /* scfsi should be decoded by caller side */
            gfc->pinfo->LAMEsfb[gr][ch][sfb] -= ifqstep * scalefac[sfb];
        }
    }                   /* for sfb */

    if(cod_info->block_type == SHORT_TYPE) {
        sfb2 = sfb;
        for(sfb = cod_info->sfb_smin; sfb < SBMAX_s; sfb++) {
            start = gfc->scalefac_band.s[sfb];
            end = gfc->scalefac_band.s[sfb + 1];
            bw = end - start;
            for(i = 0; i < 3; i++) {
                for(en0 = 0.0, l = start; l < end; l++) {
                    en0 += cod_info->xr[j] * cod_info->xr[j];
                    j++;
                }
                en0 = Max(en0 / bw, 1e-20);
                /* convert to MDCT units */
                en1 = 1e15; /* scaling so it shows up on FFT plot */

                gfc->pinfo->en_s[gr][ch][3 * sfb + i] = en1 * en0;
                gfc->pinfo->xfsf_s[gr][ch][3 * sfb + i] = en1 * l3_xmin[sfb2] * xfsf[sfb2] / bw;
                if(ratio->en.s[sfb][i] > 0)
                    en0 = en0 / ratio->en.s[sfb][i];
                else
                    en0 = 0.0;
                if(cfg->ATHonly || cfg->ATHshort)
                    en0 = 0;

                gfc->pinfo->thr_s[gr][ch][3 * sfb + i] =
                    en1 * Max(en0 * ratio->thm.s[sfb][i], gfc->ATH->s[sfb]);

                /* there is no scalefactor bands >= SBPSY_s */
                gfc->pinfo->LAMEsfb_s[gr][ch][3 * sfb + i]
                    = -2.0 * cod_info->subblock_gain[i];
                if(sfb < SBPSY_s) {
                    gfc->pinfo->LAMEsfb_s[gr][ch][3 * sfb + i] -= ifqstep * scalefac[sfb2];
                }
                sfb2++;
            }
        }
    }                   /* block type short */
    gfc->pinfo->LAMEqss[gr][ch] = cod_info->global_gain;
    gfc->pinfo->LAMEmainbits[gr][ch] = cod_info->part2_3_length + cod_info->part2_length;
    gfc->pinfo->LAMEsfbits[gr][ch] = cod_info->part2_length;

    gfc->pinfo->over[gr][ch] = noise.over_count;
    gfc->pinfo->max_noise[gr][ch] = noise.max_noise * 10.0;
    gfc->pinfo->over_noise[gr][ch] = noise.over_noise * 10.0;
    gfc->pinfo->tot_noise[gr][ch] = noise.tot_noise * 10.0;
    gfc->pinfo->over_SSD[gr][ch] = noise.over_SSD;
	}


/************************************************************************
 *
 *  set_frame_pinfo()
 *
 *  updates plotting data for a whole frame  
 *
 *  Robert Hegemann 2000-10-21                          
 *
 ************************************************************************/
void set_frame_pinfo(lame_internal_flags * gfc, const III_psy_ratio ratio[2][2]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     ch;
    int     gr;

    /* for every granule and channel patch l3_enc and set info
     */
    for(gr = 0; gr < cfg->mode_gr; gr++) {
        for(ch = 0; ch < cfg->channels_out; ch++) {
            gr_info *const cod_info = &gfc->l3_side.tt[gr][ch];
            int     scalefac_sav[SFBMAX];
            memcpy(scalefac_sav, cod_info->scalefac, sizeof(scalefac_sav));

            /* reconstruct the scalefactors in case SCFSI was used 
             */
            if(gr == 1) {
                int     sfb;
                for(sfb = 0; sfb < cod_info->sfb_lmax; sfb++) {
                    if(cod_info->scalefac[sfb] < 0) /* scfsi */
                        cod_info->scalefac[sfb] = gfc->l3_side.tt[0][ch].scalefac[sfb];
                }
            }

            set_pinfo(gfc, cod_info, &ratio[gr][ch], gr, ch);
            memcpy(cod_info->scalefac, scalefac_sav, sizeof(scalefac_sav));
        }               /* for ch */
    }                   /* for gr */
	}




/*
 *	MP3 quantization
 *
 *	Copyright (c) 1999-2000 Mark Taylor
 *	Copyright (c) 2000-2012 Robert Hegemann
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

/* $Id: vbrquantize.c,v 1.142 2012/02/07 13:36:35 robert Exp $ */



struct algo_s;
typedef struct algo_s algo_t;

typedef void (*alloc_sf_f) (const algo_t *, const int *, const int *, int);
typedef uint8_t (*find_sf_f) (const FLOAT *, const FLOAT *, FLOAT, unsigned int, uint8_t);

struct algo_s {
    alloc_sf_f alloc;
    find_sf_f  find;
    const FLOAT *xr34orig;
    lame_internal_flags *gfc;
    gr_info *cod_info;
    int     mingain_l;
    int     mingain_s[3];
};



/*  Remarks on optimizing compilers:
 *
 *  the MSVC compiler may get into aliasing problems when accessing
 *  memory through the fi_union. declaring it volatile does the trick here
 *
 *  the calc_sfb_noise_* functions are not inlined because the intel compiler
 *  optimized executeables won't work as expected anymore
 */

#ifdef _MSC_VER
#  if _MSC_VER < 1400
#  define VOLATILE volatile
#  else
#  define VOLATILE
#  endif
#else
#  define VOLATILE
#endif

typedef VOLATILE union {
    float   f;
    int     i;
} fi_union;



#ifdef TAKEHIRO_IEEE754_HACK
#define DOUBLEX double
#else
#define DOUBLEX FLOAT
#endif
 
#define MAGIC_FLOAT_def (65536*(128))
#define MAGIC_INT_def    0x4b000000

#ifdef TAKEHIRO_IEEE754_HACK
#else
/*********************************************************************
 * XRPOW_FTOI is a macro to convert floats to ints.
 * if XRPOW_FTOI(x) = nearest_int(x), then QUANTFAC(x)=adj43asm[x]
 *                                         ROUNDFAC= -0.0946
 *
 * if XRPOW_FTOI(x) = floor(x), then QUANTFAC(x)=asj43[x]
 *                                   ROUNDFAC=0.4054
 *********************************************************************/
#  define QUANTFAC(rx)  adj43[rx]
#  define ROUNDFAC_def 0.4054f
#  define XRPOW_FTOI(src,dest) ((dest) = (int)(src))
#endif

static int const MAGIC_INT = MAGIC_INT_def;
#ifndef TAKEHIRO_IEEE754_HACK
static DOUBLEX const ROUNDFAC = ROUNDFAC_def;
#endif
static DOUBLEX const MAGIC_FLOAT = MAGIC_FLOAT_def;


inline static float vec_max_c(const float * xr34, unsigned int bw) {
    float   xfsf = 0;
    unsigned int i = bw >> 2u;
    unsigned int const remaining = (bw & 0x03u);

    while(i-- > 0) {
        if(xfsf < xr34[0]) {
            xfsf = xr34[0];
        }
        if(xfsf < xr34[1]) {
            xfsf = xr34[1];
        }
        if(xfsf < xr34[2]) {
            xfsf = xr34[2];
        }
        if(xfsf < xr34[3]) {
            xfsf = xr34[3];
        }
        xr34 += 4;
    }
  switch( remaining ) {
    case 3: if(xfsf < xr34[2]) xfsf = xr34[2];
    case 2: if(xfsf < xr34[1]) xfsf = xr34[1];
    case 1: if(xfsf < xr34[0]) xfsf = xr34[0];
    default: break;
    }
  return xfsf;
	}


inline static uint8_t find_lowest_scalefac(const FLOAT xr34) {
    uint8_t sf_ok = 255;
    uint8_t sf = 128, delsf = 64;
    uint8_t i;
    FLOAT const ixmax_val = IXMAX_VAL;

    for(i = 0; i < 8; ++i) {
        FLOAT const xfsf = ipow20[sf] * xr34;
        if(xfsf <= ixmax_val) {
            sf_ok = sf;
            sf -= delsf;
        }
        else {
            sf += delsf;
        }
        delsf >>= 1;
    }
  return sf_ok;
	}


inline static void k_34_4(DOUBLEX x[4], int l3[4]) {

#ifdef TAKEHIRO_IEEE754_HACK
    fi_union fi[4];

    assert(x[0] <= IXMAX_VAL && x[1] <= IXMAX_VAL && x[2] <= IXMAX_VAL && x[3] <= IXMAX_VAL);
    x[0] += MAGIC_FLOAT;
    fi[0].f = x[0];
    x[1] += MAGIC_FLOAT;
    fi[1].f = x[1];
    x[2] += MAGIC_FLOAT;
    fi[2].f = x[2];
    x[3] += MAGIC_FLOAT;
    fi[3].f = x[3];
    fi[0].f = x[0] + adj43asm[fi[0].i - MAGIC_INT];
    fi[1].f = x[1] + adj43asm[fi[1].i - MAGIC_INT];
    fi[2].f = x[2] + adj43asm[fi[2].i - MAGIC_INT];
    fi[3].f = x[3] + adj43asm[fi[3].i - MAGIC_INT];
    l3[0] = fi[0].i - MAGIC_INT;
    l3[1] = fi[1].i - MAGIC_INT;
    l3[2] = fi[2].i - MAGIC_INT;
    l3[3] = fi[3].i - MAGIC_INT;
#else
    assert(x[0] <= IXMAX_VAL && x[1] <= IXMAX_VAL && x[2] <= IXMAX_VAL && x[3] <= IXMAX_VAL);
    XRPOW_FTOI(x[0], l3[0]);
    XRPOW_FTOI(x[1], l3[1]);
    XRPOW_FTOI(x[2], l3[2]);
    XRPOW_FTOI(x[3], l3[3]);
    x[0] += QUANTFAC(l3[0]);
    x[1] += QUANTFAC(l3[1]);
    x[2] += QUANTFAC(l3[2]);
    x[3] += QUANTFAC(l3[3]);
    XRPOW_FTOI(x[0], l3[0]);
    XRPOW_FTOI(x[1], l3[1]);
    XRPOW_FTOI(x[2], l3[2]);
    XRPOW_FTOI(x[3], l3[3]);
#endif
	}





/*  do call the calc_sfb_noise_* functions only with sf values
 *  for which holds: sfpow34*xr34 <= IXMAX_VAL
 */
static FLOAT calc_sfb_noise_x34(const FLOAT * xr, const FLOAT * xr34, unsigned int bw, uint8_t sf) {
    DOUBLEX x[4];
    int     l3[4];
    const FLOAT sfpow = pow20[sf + Q_MAX2]; /*pow(2.0,sf/4.0); */
    const FLOAT sfpow34 = ipow20[sf]; /*pow(sfpow,-3.0/4.0); */

    FLOAT   xfsf = 0;
    unsigned int i = bw >> 2u;
    unsigned int const remaining = (bw & 0x03u);

    while(i-- > 0) {
        x[0] = sfpow34 * xr34[0];
        x[1] = sfpow34 * xr34[1];
        x[2] = sfpow34 * xr34[2];
        x[3] = sfpow34 * xr34[3];

        k_34_4(x, l3);

        x[0] = fabsf(xr[0]) - sfpow * pow43[l3[0]];
        x[1] = fabsf(xr[1]) - sfpow * pow43[l3[1]];
        x[2] = fabsf(xr[2]) - sfpow * pow43[l3[2]];
        x[3] = fabsf(xr[3]) - sfpow * pow43[l3[3]];
        xfsf += (x[0] * x[0] + x[1] * x[1]) + (x[2] * x[2] + x[3] * x[3]);

        xr += 4;
        xr34 += 4;
    }
    if(remaining) {
        x[0] = x[1] = x[2] = x[3] = 0;
        switch( remaining ) {
        case 3: x[2] = sfpow34 * xr34[2];
        case 2: x[1] = sfpow34 * xr34[1];
        case 1: x[0] = sfpow34 * xr34[0];
        }

        k_34_4(x, l3);
        x[0] = x[1] = x[2] = x[3] = 0;

        switch( remaining ) {
        case 3: x[2] = fabsf(xr[2]) - sfpow * pow43[l3[2]];
        case 2: x[1] = fabsf(xr[1]) - sfpow * pow43[l3[1]];
        case 1: x[0] = fabsf(xr[0]) - sfpow * pow43[l3[0]];
        }
        xfsf += (x[0] * x[0] + x[1] * x[1]) + (x[2] * x[2] + x[3] * x[3]);
    }

  return xfsf;
	}



struct calc_noise_cache {
    int     valid;
    FLOAT   value;
};

typedef struct calc_noise_cache calc_noise_cache_t;


static uint8_t tri_calc_sfb_noise_x34(const FLOAT * xr, const FLOAT * xr34, FLOAT l3_xmin, unsigned int bw,
                       uint8_t sf, calc_noise_cache_t * did_it) {

    if(did_it[sf].valid == 0) {
        did_it[sf].valid = 1;
        did_it[sf].value = calc_sfb_noise_x34(xr, xr34, bw, sf);
    }
    if(l3_xmin < did_it[sf].value) {
        return 1;
    }
    if(sf < 255) {
        uint8_t const sf_x = sf + 1;
        if(did_it[sf_x].valid == 0) {
            did_it[sf_x].valid = 1;
            did_it[sf_x].value = calc_sfb_noise_x34(xr, xr34, bw, sf_x);
        }
        if(l3_xmin < did_it[sf_x].value) {
            return 1;
        }
    }
    if(sf > 0) {
        uint8_t const sf_x = sf - 1;
        if(did_it[sf_x].valid == 0) {
            did_it[sf_x].valid = 1;
            did_it[sf_x].value = calc_sfb_noise_x34(xr, xr34, bw, sf_x);
        }
        if(l3_xmin < did_it[sf_x].value) {
            return 1;
        }
    }

  return 0;
	}


/**
 *  Robert Hegemann 2001-05-01
 *  calculates quantization step size determined by allowed masking
 */
static int calc_scalefac(FLOAT l3_xmin, int bw) {
    FLOAT const c = 5.799142446; /* 10 * 10^(2/3) * log10(4/3) */

  return 210 + (int) (c * log10f(l3_xmin / bw) - .5f);
	}

static uint8_t guess_scalefac_x34(const FLOAT * xr, const FLOAT * xr34, FLOAT l3_xmin, unsigned int bw, uint8_t sf_min) {
    int const guess = calc_scalefac(l3_xmin, bw);

    if(guess < sf_min) 
			return sf_min;
    if(guess >= 255) 
			return 255;
    (void) xr;
    (void) xr34;
  return guess;
	}


/* the find_scalefac* routines calculate
 * a quantization step size which would
 * introduce as much noise as is allowed.
 * The larger the step size the more
 * quantization noise we'll get. The
 * scalefactors are there to lower the
 * global step size, allowing limited
 * differences in quantization step sizes
 * per band (shaping the noise).
 */
static  uint8_t find_scalefac_x34(const FLOAT * xr, const FLOAT * xr34, FLOAT l3_xmin, unsigned int bw,
                  uint8_t sf_min) {
    calc_noise_cache_t did_it[256];
    uint8_t sf = 128, sf_ok = 255, delsf = 128, seen_good_one = 0, i;

    memset(did_it, 0, sizeof(did_it));
    for(i = 0; i < 8; ++i) {
        delsf >>= 1;
        if(sf <= sf_min) {
            sf += delsf;
        }
        else {
            uint8_t const bad = tri_calc_sfb_noise_x34(xr, xr34, l3_xmin, bw, sf, did_it);
            if(bad) {  /* distortion.  try a smaller scalefactor */
                sf -= delsf;
            }
            else {
                sf_ok = sf;
                sf += delsf;
                seen_good_one = 1;
            }
        }
    }
    /*  returning a scalefac without distortion, if possible
     */
    if(seen_good_one > 0) {
        sf = sf_ok;
    }
    if(sf <= sf_min) {
        sf = sf_min;
    }

  return sf;
	}



/***********************************************************************
 *
 *      calc_short_block_vbr_sf()
 *      calc_long_block_vbr_sf()
 *
 *  Mark Taylor 2000-??-??
 *  Robert Hegemann 2000-10-25 made functions of it
 *
 ***********************************************************************/

/* a variation for vbr-mtrh */
static int block_sf(algo_t * that, const FLOAT l3_xmin[SFBMAX], int vbrsf[SFBMAX], int vbrsfmin[SFBMAX]) {
    FLOAT   max_xr34;
    const FLOAT *const xr = &that->cod_info->xr[0];
    const FLOAT *const xr34_orig = &that->xr34orig[0];
    const int *const width = &that->cod_info->width[0];
    const char *const energy_above_cutoff = &that->cod_info->energy_above_cutoff[0];
    unsigned int const max_nonzero_coeff = (unsigned int) that->cod_info->max_nonzero_coeff;
    uint8_t maxsf = 0;
    int     sfb = 0, m_o = -1;
    unsigned int j = 0, i = 0;
    int const psymax = that->cod_info->psymax;

    assert(that->cod_info->max_nonzero_coeff >= 0);

    that->mingain_l = 0;
    that->mingain_s[0] = 0;
    that->mingain_s[1] = 0;
    that->mingain_s[2] = 0;
    while(j <= max_nonzero_coeff) {
        unsigned int const w = (unsigned int) width[sfb];
        unsigned int const m = (unsigned int) (max_nonzero_coeff - j + 1);
        unsigned int l = w;
        uint8_t m1, m2;
        if(l > m) {
            l = m;
        }
        max_xr34 = vec_max_c(&xr34_orig[j], l);

        m1 = find_lowest_scalefac(max_xr34);
        vbrsfmin[sfb] = m1;
        if(that->mingain_l < m1) {
            that->mingain_l = m1;
        }
        if(that->mingain_s[i] < m1) {
            that->mingain_s[i] = m1;
        }
        if(++i > 2) {
            i = 0;
        }
        if(sfb < psymax && w > 2) { /* mpeg2.5 at 8 kHz doesn't use all scalefactors, unused have width 2 */
            if(energy_above_cutoff[sfb]) {
                m2 = that->find(&xr[j], &xr34_orig[j], l3_xmin[sfb], l, m1);
#if 0
                if(0) {
                    /** Robert Hegemann 2007-09-29:
                     *  It seems here is some more potential for speed improvements.
                     *  Current find method does 11-18 quantization calculations.
                     *  Using a "good guess" may help to reduce this amount.
                     */
                    uint8_t guess = calc_scalefac(l3_xmin[sfb], l);
                    DEBUGF(that->gfc, "sfb=%3d guess=%3d found=%3d diff=%3d\n", sfb, guess, m2,
                           m2 - guess);
                }
#endif
                if(maxsf < m2) {
                    maxsf = m2;
                }
                if(m_o < m2 && m2 < 255) {
                    m_o = m2;
                }
            }
            else {
                m2 = 255;
                maxsf = 255;
            }
        }
        else {
            if(maxsf < m1) {
                maxsf = m1;
            }
            m2 = maxsf;
        }
        vbrsf[sfb] = m2;
        ++sfb;
        j += w;        
    }
    for(; sfb < SFBMAX; ++sfb) {
        vbrsf[sfb] = maxsf;
        vbrsfmin[sfb] = 0;
    }
    if(m_o > -1) {
        maxsf = m_o;
        for(sfb = 0; sfb < SFBMAX; ++sfb) {
            if(vbrsf[sfb] == 255) {
                vbrsf[sfb] = m_o;
            }
        }
    }

  return maxsf;
	}



/***********************************************************************
 *
 *  quantize xr34 based on scalefactors
 *
 *  block_xr34
 *
 *  Mark Taylor 2000-??-??
 *  Robert Hegemann 2000-10-20 made functions of them
 *
 ***********************************************************************/
static void quantize_x34(const algo_t * that) {
    DOUBLEX x[4];
    const FLOAT *xr34_orig = that->xr34orig;
    gr_info *const cod_info = that->cod_info;
    int const ifqstep = (cod_info->scalefac_scale == 0) ? 2 : 4;
    int    *l3 = cod_info->l3_enc;
    unsigned int j = 0, sfb = 0;
    unsigned int const max_nonzero_coeff = (unsigned int) cod_info->max_nonzero_coeff;

    assert(cod_info->max_nonzero_coeff >= 0);
    assert(cod_info->max_nonzero_coeff < 576);

    while(j <= max_nonzero_coeff) {
        int const s =
            (cod_info->scalefac[sfb] + (cod_info->preflag ? pretab[sfb] : 0)) * ifqstep
            + cod_info->subblock_gain[cod_info->window[sfb]] * 8;
        uint8_t const sfac = (uint8_t) (cod_info->global_gain - s);
        FLOAT const sfpow34 = ipow20[sfac];
        unsigned int const w = (unsigned int) cod_info->width[sfb];
        unsigned int const m = (unsigned int) (max_nonzero_coeff - j + 1);
        unsigned int i, remaining;

        assert((cod_info->global_gain - s) >= 0);
        assert(cod_info->width[sfb] >= 0);
        j += w;
        ++sfb;
        
        i = (w <= m) ? w : m;
        remaining = (i & 0x03u);
        i >>= 2u;

        while(i-- > 0) {
            x[0] = sfpow34 * xr34_orig[0];
            x[1] = sfpow34 * xr34_orig[1];
            x[2] = sfpow34 * xr34_orig[2];
            x[3] = sfpow34 * xr34_orig[3];

            k_34_4(x, l3);

            l3 += 4;
            xr34_orig += 4;
        }
        if(remaining) {
            int tmp_l3[4];
            x[0] = x[1] = x[2] = x[3] = 0;
            switch( remaining ) {
            case 3: x[2] = sfpow34 * xr34_orig[2];
            case 2: x[1] = sfpow34 * xr34_orig[1];
            case 1: x[0] = sfpow34 * xr34_orig[0];
            }

            k_34_4(x, tmp_l3);

            switch( remaining ) {
            case 3: l3[2] = tmp_l3[2];
            case 2: l3[1] = tmp_l3[1];
            case 1: l3[0] = tmp_l3[0];
            }

            l3 += remaining;
            xr34_orig += remaining;
        }
    }
	}



static const uint8_t max_range_short[SBMAX_s * 3] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    0, 0, 0
};

static const uint8_t max_range_long[SBMAX_l] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0
};

static const uint8_t max_range_long_lsf_pretab[SBMAX_l] = {
    7, 7, 7, 7, 7, 7, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};



/*
    sfb=0..5  scalefac < 16
    sfb>5     scalefac < 8

    ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
    ol_sf =  (cod_info->global_gain-210.0);
    ol_sf -= 8*cod_info->subblock_gain[i];
    ol_sf -= ifqstep*scalefac[gr][ch].s[sfb][i];
*/

static void set_subblock_gain(gr_info * cod_info, const int mingain_s[3], int sf[]) {
    const int maxrange1 = 15, maxrange2 = 7;
    const int ifqstepShift = (cod_info->scalefac_scale == 0) ? 1 : 2;
    int    *const sbg = cod_info->subblock_gain;
    unsigned int const psymax = (unsigned int) cod_info->psymax;
    unsigned int psydiv = 18;
    int     sbg0, sbg1, sbg2;
    unsigned int sfb, i;
    int     min_sbg = 7;

    if(psydiv > psymax) {
        psydiv = psymax;
    }
    for(i = 0; i < 3; ++i) {
        int     maxsf1 = 0, maxsf2 = 0, minsf = 1000;
        /* see if we should use subblock gain */
        for(sfb = i; sfb < psydiv; sfb += 3) { /* part 1 */
            int const v = -sf[sfb];
            if(maxsf1 < v) {
                maxsf1 = v;
            }
            if(minsf > v) {
                minsf = v;
            }
        }
        for(; sfb < SFBMAX; sfb += 3) { /* part 2 */
            int const v = -sf[sfb];
            if(maxsf2 < v) {
                maxsf2 = v;
            }
            if(minsf > v) {
                minsf = v;
            }
        }

        /* boost subblock gain as little as possible so we can
         * reach maxsf1 with scalefactors
         * 8*sbg >= maxsf1
         */
        {
            int const m1 = maxsf1 - (maxrange1 << ifqstepShift);
            int const m2 = maxsf2 - (maxrange2 << ifqstepShift);

            maxsf1 = Max(m1, m2);
        }
        if(minsf > 0) {
            sbg[i] = minsf >> 3;
        }
        else {
            sbg[i] = 0;
        }
        if(maxsf1 > 0) {
            int const m1 = sbg[i];
            int const m2 = (maxsf1 + 7) >> 3;
            sbg[i] = Max(m1, m2);
        }
        if(sbg[i] > 0 && mingain_s[i] > (cod_info->global_gain - sbg[i] * 8)) {
            sbg[i] = (cod_info->global_gain - mingain_s[i]) >> 3;
        }
        if(sbg[i] > 7) {
            sbg[i] = 7;
        }
        if(min_sbg > sbg[i]) {
            min_sbg = sbg[i];
        }
    }
    sbg0 = sbg[0] * 8;
    sbg1 = sbg[1] * 8;
    sbg2 = sbg[2] * 8;
    for(sfb = 0; sfb < SFBMAX; sfb += 3) {
        sf[sfb + 0] += sbg0;
        sf[sfb + 1] += sbg1;
        sf[sfb + 2] += sbg2;
    }
    if(min_sbg > 0) {
        for(i = 0; i < 3; ++i) {
            sbg[i] -= min_sbg;
        }
        cod_info->global_gain -= min_sbg * 8;
    }
	}



/*
	  ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
	  ol_sf =  (cod_info->global_gain-210.0);
	  ol_sf -= ifqstep*scalefac[gr][ch].l[sfb];
	  if(cod_info->preflag && sfb>=11)
	  ol_sf -= ifqstep*pretab[sfb];
*/
static void set_scalefacs(gr_info * cod_info, const int *vbrsfmin, int sf[], const uint8_t * max_range) {
    const int ifqstep = (cod_info->scalefac_scale == 0) ? 2 : 4;
    const int ifqstepShift = (cod_info->scalefac_scale == 0) ? 1 : 2;
    int    *const scalefac = cod_info->scalefac;
    int const sfbmax = cod_info->sfbmax;
    int     sfb;
    int const *const sbg = cod_info->subblock_gain;
    int const *const window = cod_info->window;
    int const preflag = cod_info->preflag;

    if(preflag) {
        for(sfb = 11; sfb < sfbmax; ++sfb) {
            sf[sfb] += pretab[sfb] * ifqstep;
        }
    }
    for(sfb = 0; sfb < sfbmax; ++sfb) {
        int const gain = cod_info->global_gain - (sbg[window[sfb]] * 8)
            - ((preflag ? pretab[sfb] : 0) * ifqstep);

        if(sf[sfb] < 0) {
            int const m = gain - vbrsfmin[sfb];
            /* ifqstep*scalefac >= -sf[sfb], so round UP */
            scalefac[sfb] = (ifqstep - 1 - sf[sfb]) >> ifqstepShift;

            if(scalefac[sfb] > max_range[sfb]) {
                scalefac[sfb] = max_range[sfb];
            }
            if(scalefac[sfb] > 0 && (scalefac[sfb] << ifqstepShift) > m) {
                scalefac[sfb] = m >> ifqstepShift;
            }
        }
        else {
            scalefac[sfb] = 0;
        }
    }
    for(; sfb < SFBMAX; ++sfb) {
        scalefac[sfb] = 0; /* sfb21 */
    }
	}


#ifndef NDEBUG
static int checkScalefactor(const gr_info * cod_info, const int vbrsfmin[SFBMAX]) {
    int const ifqstep = cod_info->scalefac_scale == 0 ? 2 : 4;
    int     sfb;

    for(sfb = 0; sfb < cod_info->psymax; ++sfb) {
        const int s =
            ((cod_info->scalefac[sfb] +
              (cod_info->preflag ? pretab[sfb] : 0)) * ifqstep) +
            cod_info->subblock_gain[cod_info->window[sfb]] * 8;

        if((cod_info->global_gain - s) < vbrsfmin[sfb]) {
            /*
               fprintf( stdout, "sf %d\n", sfb );
               fprintf( stdout, "min %d\n", vbrsfmin[sfb] );
               fprintf( stdout, "ggain %d\n", cod_info->global_gain );
               fprintf( stdout, "scalefac %d\n", cod_info->scalefac[sfb] );
               fprintf( stdout, "pretab %d\n", (cod_info->preflag ? pretab[sfb] : 0) );
               fprintf( stdout, "scale %d\n", (cod_info->scalefac_scale + 1) );
               fprintf( stdout, "subgain %d\n", cod_info->subblock_gain[cod_info->window[sfb]] * 8 );
               fflush( stdout );
               exit(-1);
             */
            return 0;
        }
    }
  return 1;
	}
#endif


/******************************************************************
 *
 *  short block scalefacs
 *
 ******************************************************************/
static void short_block_constrain(const algo_t * that, const int vbrsf[SFBMAX],
                      const int vbrsfmin[SFBMAX], int vbrmax) {
    gr_info *const cod_info = that->cod_info;
    lame_internal_flags const *const gfc = that->gfc;
    SessionConfig_t const *const cfg = &gfc->cfg;
    int const maxminsfb = that->mingain_l;
    int     mover, maxover0 = 0, maxover1 = 0, delta = 0;
    int     v, v0, v1;
    int     sfb;
    int const psymax = cod_info->psymax;

    for(sfb = 0; sfb < psymax; ++sfb) {
        assert(vbrsf[sfb] >= vbrsfmin[sfb]);
        v = vbrmax - vbrsf[sfb];
        if(delta < v) {
            delta = v;
        }
        v0 = v - (4 * 14 + 2 * max_range_short[sfb]);
        v1 = v - (4 * 14 + 4 * max_range_short[sfb]);
        if(maxover0 < v0) {
            maxover0 = v0;
        }
        if(maxover1 < v1) {
            maxover1 = v1;
        }
    }
    if(cfg->noise_shaping == 2) {
        /* allow scalefac_scale=1 */
        mover = Min(maxover0, maxover1);
    }
    else {
        mover = maxover0;
    }
    if(delta > mover) {
        delta = mover;
    }
    vbrmax -= delta;
    maxover0 -= mover;
    maxover1 -= mover;

    if(maxover0 == 0) {
        cod_info->scalefac_scale = 0;
    }
    else if(maxover1 == 0) {
        cod_info->scalefac_scale = 1;
    }
    if(vbrmax < maxminsfb) {
        vbrmax = maxminsfb;
    }
    cod_info->global_gain = vbrmax;

    if(cod_info->global_gain < 0) {
        cod_info->global_gain = 0;
    }
    else if(cod_info->global_gain > 255) {
        cod_info->global_gain = 255;
    }
    {
        int     sf_temp[SFBMAX];
        for(sfb = 0; sfb < SFBMAX; ++sfb) {
            sf_temp[sfb] = vbrsf[sfb] - vbrmax;
        }
        set_subblock_gain(cod_info, &that->mingain_s[0], sf_temp);
        set_scalefacs(cod_info, vbrsfmin, sf_temp, max_range_short);
    }
  assert(checkScalefactor(cod_info, vbrsfmin));
	}



/******************************************************************
 *
 *  long block scalefacs
 *
 ******************************************************************/
static void long_block_constrain(const algo_t * that, const int vbrsf[SFBMAX], const int vbrsfmin[SFBMAX],
                     int vbrmax) {
    gr_info *const cod_info = that->cod_info;
    lame_internal_flags const *const gfc = that->gfc;
    SessionConfig_t const *const cfg = &gfc->cfg;
    uint8_t const *max_rangep;
    int const maxminsfb = that->mingain_l;
    int     sfb;
    int     maxover0, maxover1, maxover0p, maxover1p, mover, delta = 0;
    int     v, v0, v1, v0p, v1p, vm0p = 1, vm1p = 1;
    int const psymax = cod_info->psymax;

    max_rangep = cfg->mode_gr == 2 ? max_range_long : max_range_long_lsf_pretab;

    maxover0 = 0;
    maxover1 = 0;
    maxover0p = 0;      /* pretab */
    maxover1p = 0;      /* pretab */

    for(sfb = 0; sfb < psymax; ++sfb) {
        assert(vbrsf[sfb] >= vbrsfmin[sfb]);
        v = vbrmax - vbrsf[sfb];
        if(delta < v) {
            delta = v;
        }
        v0 = v - 2 * max_range_long[sfb];
        v1 = v - 4 * max_range_long[sfb];
        v0p = v - 2 * (max_rangep[sfb] + pretab[sfb]);
        v1p = v - 4 * (max_rangep[sfb] + pretab[sfb]);
        if(maxover0 < v0) {
            maxover0 = v0;
        }
        if(maxover1 < v1) {
            maxover1 = v1;
        }
        if(maxover0p < v0p) {
            maxover0p = v0p;
        }
        if(maxover1p < v1p) {
            maxover1p = v1p;
        }
    }
    if(vm0p == 1) {
        int     gain = vbrmax - maxover0p;
        if(gain < maxminsfb) {
            gain = maxminsfb;
        }
        for(sfb = 0; sfb < psymax; ++sfb) {
            int const a = (gain - vbrsfmin[sfb]) - 2 * pretab[sfb];
            if(a <= 0) {
                vm0p = 0;
                vm1p = 0;
                break;
            }
        }
    }
    if(vm1p == 1) {
        int     gain = vbrmax - maxover1p;
        if(gain < maxminsfb) {
            gain = maxminsfb;
        }
        for(sfb = 0; sfb < psymax; ++sfb) {
            int const b = (gain - vbrsfmin[sfb]) - 4 * pretab[sfb];
            if(b <= 0) {
                vm1p = 0;
                break;
            }
        }
    }
    if(vm0p == 0) {
        maxover0p = maxover0;
    }
    if(vm1p == 0) {
        maxover1p = maxover1;
    }
    if(cfg->noise_shaping != 2) {
        maxover1 = maxover0;
        maxover1p = maxover0p;
    }
    mover = Min(maxover0, maxover0p);
    mover = Min(mover, maxover1);
    mover = Min(mover, maxover1p);

    if(delta > mover) {
        delta = mover;
    }
    vbrmax -= delta;
    if(vbrmax < maxminsfb) {
        vbrmax = maxminsfb;
    }
    maxover0 -= mover;
    maxover0p -= mover;
    maxover1 -= mover;
    maxover1p -= mover;

    if(maxover0 == 0) {
        cod_info->scalefac_scale = 0;
        cod_info->preflag = 0;
        max_rangep = max_range_long;
    }
    else if(maxover0p == 0) {
        cod_info->scalefac_scale = 0;
        cod_info->preflag = 1;
    }
    else if(maxover1 == 0) {
        cod_info->scalefac_scale = 1;
        cod_info->preflag = 0;
        max_rangep = max_range_long;
    }
    else if(maxover1p == 0) {
        cod_info->scalefac_scale = 1;
        cod_info->preflag = 1;
    }
    else {
        assert(0);      /* this should not happen */
    }
    cod_info->global_gain = vbrmax;
    if(cod_info->global_gain < 0) {
        cod_info->global_gain = 0;
    }
    else if(cod_info->global_gain > 255) {
        cod_info->global_gain = 255;
    }
    {
        int     sf_temp[SFBMAX];
        for(sfb = 0; sfb < SFBMAX; ++sfb) {
            sf_temp[sfb] = vbrsf[sfb] - vbrmax;
        }
        set_scalefacs(cod_info, vbrsfmin, sf_temp, max_rangep);
    }
    assert(checkScalefactor(cod_info, vbrsfmin));
	}



static void bitcount(const algo_t * that) {
    int rc = scale_bitcount(that->gfc, that->cod_info);

    if(rc == 0) {
        return;
    }
    /*  this should not happen due to the way the scalefactors are selected  */
    ERRORF(that->gfc, "INTERNAL ERROR IN VBR NEW CODE (986), please send bug report\n");
    exit(-1);
	}



static int quantizeAndCountBits(const algo_t * that) {

    quantize_x34(that);
    that->cod_info->part2_3_length = noquant_count_bits(that->gfc, that->cod_info, 0);
    return that->cod_info->part2_3_length;
}



static int tryGlobalStepsize(const algo_t * that, const int sfwork[SFBMAX],
                  const int vbrsfmin[SFBMAX], int delta) {
    FLOAT const xrpow_max = that->cod_info->xrpow_max;
    int     sftemp[SFBMAX], i, nbits;
    int     gain, vbrmax = 0;
    for(i = 0; i < SFBMAX; ++i) {
        gain = sfwork[i] + delta;
        if(gain < vbrsfmin[i]) {
            gain = vbrsfmin[i];
        }
        if(gain > 255) {
            gain = 255;
        }
        if(vbrmax < gain) {
            vbrmax = gain;
        }
        sftemp[i] = gain;
    }
    that->alloc(that, sftemp, vbrsfmin, vbrmax);
    bitcount(that);
    nbits = quantizeAndCountBits(that);
    that->cod_info->xrpow_max = xrpow_max;
    return nbits;
	}



static void searchGlobalStepsizeMax(const algo_t * that, const int sfwork[SFBMAX],
                        const int vbrsfmin[SFBMAX], int target) {
    gr_info const *const cod_info = that->cod_info;
    const int gain = cod_info->global_gain;
    int     curr = gain;
    int     gain_ok = 1024;
    int     nbits = LARGE_BITS;
    int     l = gain, r = 512;

    assert(gain >= 0);
    while(l <= r) {
        curr = (l + r) >> 1;
        nbits = tryGlobalStepsize(that, sfwork, vbrsfmin, curr - gain);
        if(nbits == 0 || (nbits + cod_info->part2_length) < target) {
            r = curr - 1;
            gain_ok = curr;
        }
        else {
            l = curr + 1;
            if(gain_ok == 1024) {
                gain_ok = curr;
            }
        }
    }
    if(gain_ok != curr) {
        curr = gain_ok;
        nbits = tryGlobalStepsize(that, sfwork, vbrsfmin, curr - gain);
    }
	}



static int sfDepth(const int sfwork[SFBMAX]) {
    int     m = 0;
    unsigned int i, j;

    for(j = SFBMAX, i = 0; j > 0; --j, ++i) {
        int const di = 255 - sfwork[i];
        if(m < di) {
            m = di;
        }
        assert(sfwork[i] >= 0);
        assert(sfwork[i] <= 255);
    }
    assert(m >= 0);
    assert(m <= 255);
    return m;
	}


static void cutDistribution(const int sfwork[SFBMAX], int sf_out[SFBMAX], int cut) {
    unsigned int i, j;

    for(j = SFBMAX, i = 0; j > 0; --j, ++i) {
        int const x = sfwork[i];
        sf_out[i] = x < cut ? x : cut;
    }
	}


static int flattenDistribution(const int sfwork[SFBMAX], int sf_out[SFBMAX], int dm, int k, int p) {
    unsigned int i, j;
    int     x, sfmax = 0;

    if(dm > 0) {
        for(j = SFBMAX, i = 0; j > 0; --j, ++i) {
            int const di = p - sfwork[i];
            x = sfwork[i] + (k * di) / dm;
            if(x < 0) {
                x = 0;
            }
            else {
                if(x > 255) {
                    x = 255;
                }
            }
            sf_out[i] = x;
            if(sfmax < x) {
                sfmax = x;
            }
        }
    }
    else {
        for(j = SFBMAX, i = 0; j > 0u; --j, ++i) {
            x = sfwork[i];
            sf_out[i] = x;
            if(sfmax < x) {
                sfmax = x;
            }
        }
    }

  return sfmax;
	}


static int tryThatOne(algo_t const* that, const int sftemp[SFBMAX], const int vbrsfmin[SFBMAX], int vbrmax) {
    FLOAT const xrpow_max = that->cod_info->xrpow_max;
    int     nbits = LARGE_BITS;

    that->alloc(that, sftemp, vbrsfmin, vbrmax);
    bitcount(that);
    nbits = quantizeAndCountBits(that);
    nbits += that->cod_info->part2_length;
    that->cod_info->xrpow_max = xrpow_max;
    return nbits;
	}


static void outOfBitsStrategy(algo_t const* that, const int sfwork[SFBMAX], const int vbrsfmin[SFBMAX], int target) {
    int     wrk[SFBMAX];
    int const dm = sfDepth(sfwork);
    int const p = that->cod_info->global_gain;
    int     nbits;

    /* PART 1 */
    {
        int     bi = dm / 2;
        int     bi_ok = -1;
        int     bu = 0;
        int     bo = dm;
        for(;;) {
            int const sfmax = flattenDistribution(sfwork, wrk, dm, bi, p);
            nbits = tryThatOne(that, wrk, vbrsfmin, sfmax);
            if(nbits <= target) {
                bi_ok = bi;
                bo = bi - 1;
            }
            else {
                bu = bi + 1;
            }
            if(bu <= bo) {
                bi = (bu + bo) / 2;
            }
            else {
                break;
            }
        }
        if(bi_ok >= 0) {
            if(bi != bi_ok) {
                int const sfmax = flattenDistribution(sfwork, wrk, dm, bi_ok, p);
                nbits = tryThatOne(that, wrk, vbrsfmin, sfmax);
            }
            return;
        }
    }

    /* PART 2: */
    {
        int     bi = (255 + p) / 2;
        int     bi_ok = -1;
        int     bu = p;
        int     bo = 255;
        for(;;) {
            int const sfmax = flattenDistribution(sfwork, wrk, dm, dm, bi);
            nbits = tryThatOne(that, wrk, vbrsfmin, sfmax);
            if(nbits <= target) {
                bi_ok = bi;
                bo = bi - 1;
            }
            else {
                bu = bi + 1;
            }
            if(bu <= bo) {
                bi = (bu + bo) / 2;
            }
            else {
                break;
            }
        }
        if(bi_ok >= 0) {
            if(bi != bi_ok) {
                int const sfmax = flattenDistribution(sfwork, wrk, dm, dm, bi_ok);
                nbits = tryThatOne(that, wrk, vbrsfmin, sfmax);
            }
            return;
        }
    }

    /* fall back to old code, likely to be never called */
    searchGlobalStepsizeMax(that, wrk, vbrsfmin, target);
	}


static int reduce_bit_usage(lame_internal_flags * gfc, int gr, int ch
#if 0
                 , const FLOAT xr34orig[576], const FLOAT l3_xmin[SFBMAX], int maxbits
#endif
    )
{
    SessionConfig_t const *const cfg = &gfc->cfg;
    gr_info *const cod_info = &gfc->l3_side.tt[gr][ch];
    /*  try some better scalefac storage
     */
    best_scalefac_store(gfc, gr, ch, &gfc->l3_side);

    /*  best huffman_divide may save some bits too
     */
    if(cfg->use_best_huffman == 1)
        best_huffman_divide(gfc, cod_info);
    return cod_info->part2_3_length + cod_info->part2_length;
	}




int VBR_encode_frame(lame_internal_flags * gfc, const FLOAT xr34orig[2][2][576],
                 const FLOAT l3_xmin[2][2][SFBMAX], const int max_bits[2][2]) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     sfwork_[2][2][SFBMAX];
    int     vbrsfmin_[2][2][SFBMAX];
    algo_t  that_[2][2];
    int const ngr = cfg->mode_gr;
    int const nch = cfg->channels_out;
    int     max_nbits_ch[2][2] = {{0, 0}, {0 ,0}};
    int     max_nbits_gr[2] = {0, 0};
    int     max_nbits_fr = 0;
    int     use_nbits_ch[2][2] = {{MAX_BITS_PER_CHANNEL+1, MAX_BITS_PER_CHANNEL+1}
                                 ,{MAX_BITS_PER_CHANNEL+1, MAX_BITS_PER_CHANNEL+1}};
    int     use_nbits_gr[2] = { MAX_BITS_PER_GRANULE+1, MAX_BITS_PER_GRANULE+1 };
    int     use_nbits_fr = MAX_BITS_PER_GRANULE+MAX_BITS_PER_GRANULE;
    int     gr, ch;
    int     ok, sum_fr;

    /* set up some encoding parameters
     */
    for(gr = 0; gr < ngr; ++gr) {
        max_nbits_gr[gr] = 0;
        for(ch = 0; ch < nch; ++ch) {
            max_nbits_ch[gr][ch] = max_bits[gr][ch];
            use_nbits_ch[gr][ch] = 0;
            max_nbits_gr[gr] += max_bits[gr][ch];
            max_nbits_fr += max_bits[gr][ch];
            that_[gr][ch].find = (cfg->full_outer_loop < 0) ? guess_scalefac_x34 : find_scalefac_x34;
            that_[gr][ch].gfc = gfc;
            that_[gr][ch].cod_info = &gfc->l3_side.tt[gr][ch];
            that_[gr][ch].xr34orig = xr34orig[gr][ch];
            if(that_[gr][ch].cod_info->block_type == SHORT_TYPE) {
                that_[gr][ch].alloc = short_block_constrain;
            }
            else {
                that_[gr][ch].alloc = long_block_constrain;
            }
        }               /* for ch */
    }
    /* searches scalefactors
     */
    for(gr = 0; gr < ngr; ++gr) {
        for(ch = 0; ch < nch; ++ch) {
            if(max_bits[gr][ch] > 0) {
                algo_t *that = &that_[gr][ch];
                int    *sfwork = sfwork_[gr][ch];
                int    *vbrsfmin = vbrsfmin_[gr][ch];
                int     vbrmax;

                vbrmax = block_sf(that, l3_xmin[gr][ch], sfwork, vbrsfmin);
                that->alloc(that, sfwork, vbrsfmin, vbrmax);
                bitcount(that);
            }
            else {
                /*  xr contains no energy 
                 *  l3_enc, our encoding data, will be quantized to zero
                 *  continue with next channel
                 */
            }
        }               /* for ch */
    }
    /* encode 'as is'
     */
    use_nbits_fr = 0;
    for(gr = 0; gr < ngr; ++gr) {
        use_nbits_gr[gr] = 0;
        for(ch = 0; ch < nch; ++ch) {
            algo_t const *that = &that_[gr][ch];
            if(max_bits[gr][ch] > 0) {
                memset(&that->cod_info->l3_enc[0], 0, sizeof(that->cod_info->l3_enc));
                (void) quantizeAndCountBits(that);
            }
            else {
                /*  xr contains no energy 
                 *  l3_enc, our encoding data, will be quantized to zero
                 *  continue with next channel
                 */
            }
            use_nbits_ch[gr][ch] = reduce_bit_usage(gfc, gr, ch);
            use_nbits_gr[gr] += use_nbits_ch[gr][ch];
        }               /* for ch */
        use_nbits_fr += use_nbits_gr[gr];
    }

    /* check bit constrains
     */
    if(use_nbits_fr <= max_nbits_fr) {
        ok = 1;
        for(gr = 0; gr < ngr; ++gr) {
            if(use_nbits_gr[gr] > MAX_BITS_PER_GRANULE) {
                /* violates the rule that every granule has to use no more
                 * bits than MAX_BITS_PER_GRANULE
                 */
                ok = 0;
            }
            for(ch = 0; ch < nch; ++ch) {
                if(use_nbits_ch[gr][ch] > MAX_BITS_PER_CHANNEL) {
                    /* violates the rule that every gr_ch has to use no more
                     * bits than MAX_BITS_PER_CHANNEL
                     *
                     * This isn't explicitly stated in the ISO docs, but the
                     * part2_3_length field has only 12 bits, that makes it
                     * up to a maximum size of 4095 bits!!!
                     */
                    ok = 0;
                }
            }
        }
        if(ok) {
            return use_nbits_fr;
        }
    }
    
    /* OK, we are in trouble and have to define how many bits are
     * to be used for each granule
     */
    {
        ok = 1;
        sum_fr = 0;

        for(gr = 0; gr < ngr; ++gr) {
            max_nbits_gr[gr] = 0;
            for(ch = 0; ch < nch; ++ch) {
                if(use_nbits_ch[gr][ch] > MAX_BITS_PER_CHANNEL) {
                    max_nbits_ch[gr][ch] = MAX_BITS_PER_CHANNEL;
                }
                else {
                    max_nbits_ch[gr][ch] = use_nbits_ch[gr][ch];
                }
                max_nbits_gr[gr] += max_nbits_ch[gr][ch];
            }
            if(max_nbits_gr[gr] > MAX_BITS_PER_GRANULE) {
                float   f[2] = {0.0f, 0.0f}, s = 0.0f;
                for(ch = 0; ch < nch; ++ch) {
                    if(max_nbits_ch[gr][ch] > 0) {
                        f[ch] = sqrt(sqrt(max_nbits_ch[gr][ch]));
                        s += f[ch];
                    }
                    else {
                        f[ch] = 0;
                    }
                }
                for(ch = 0; ch < nch; ++ch) {
                    if(s > 0) {
                        max_nbits_ch[gr][ch] = MAX_BITS_PER_GRANULE * f[ch] / s;
                    }
                    else {
                        max_nbits_ch[gr][ch] = 0;
                    }
                }
                if(nch > 1) {
                    if(max_nbits_ch[gr][0] > use_nbits_ch[gr][0] + 32) {
                        max_nbits_ch[gr][1] += max_nbits_ch[gr][0];
                        max_nbits_ch[gr][1] -= use_nbits_ch[gr][0] + 32;
                        max_nbits_ch[gr][0] = use_nbits_ch[gr][0] + 32;
                    }
                    if(max_nbits_ch[gr][1] > use_nbits_ch[gr][1] + 32) {
                        max_nbits_ch[gr][0] += max_nbits_ch[gr][1];
                        max_nbits_ch[gr][0] -= use_nbits_ch[gr][1] + 32;
                        max_nbits_ch[gr][1] = use_nbits_ch[gr][1] + 32;
                    }
                    if(max_nbits_ch[gr][0] > MAX_BITS_PER_CHANNEL) {
                        max_nbits_ch[gr][0] = MAX_BITS_PER_CHANNEL;
                    }
                    if(max_nbits_ch[gr][1] > MAX_BITS_PER_CHANNEL) {
                        max_nbits_ch[gr][1] = MAX_BITS_PER_CHANNEL;
                    }
                }
                max_nbits_gr[gr] = 0;
                for(ch = 0; ch < nch; ++ch) {
                    max_nbits_gr[gr] += max_nbits_ch[gr][ch];
                }
            }
            sum_fr += max_nbits_gr[gr];
        }
        if(sum_fr > max_nbits_fr) {
            {
                float   f[2] = {0.0f, 0.0f}, s = 0.0f;
                for(gr = 0; gr < ngr; ++gr) {
                    if(max_nbits_gr[gr] > 0) {
                        f[gr] = sqrt(max_nbits_gr[gr]);
                        s += f[gr];
                    }
                    else {
                        f[gr] = 0;
                    }
                }
                for(gr = 0; gr < ngr; ++gr) {
                    if(s > 0) {
                        max_nbits_gr[gr] = max_nbits_fr * f[gr] / s;
                    }
                    else {
                        max_nbits_gr[gr] = 0;
                    }
                }
            }
            if(ngr > 1) {
                if(max_nbits_gr[0] > use_nbits_gr[0] + 125) {
                    max_nbits_gr[1] += max_nbits_gr[0];
                    max_nbits_gr[1] -= use_nbits_gr[0] + 125;
                    max_nbits_gr[0] = use_nbits_gr[0] + 125;
                }
                if(max_nbits_gr[1] > use_nbits_gr[1] + 125) {
                    max_nbits_gr[0] += max_nbits_gr[1];
                    max_nbits_gr[0] -= use_nbits_gr[1] + 125;
                    max_nbits_gr[1] = use_nbits_gr[1] + 125;
                }
                for(gr = 0; gr < ngr; ++gr) {
                    if(max_nbits_gr[gr] > MAX_BITS_PER_GRANULE) {
                        max_nbits_gr[gr] = MAX_BITS_PER_GRANULE;
                    }
                }
            }
            for(gr = 0; gr < ngr; ++gr) {
                float   f[2] = {0.0f, 0.0f}, s = 0.0f;
                for(ch = 0; ch < nch; ++ch) {
                    if(max_nbits_ch[gr][ch] > 0) {
                        f[ch] = sqrt(max_nbits_ch[gr][ch]);
                        s += f[ch];
                    }
                    else {
                        f[ch] = 0;
                    }
                }
                for(ch = 0; ch < nch; ++ch) {
                    if(s > 0) {
                        max_nbits_ch[gr][ch] = max_nbits_gr[gr] * f[ch] / s;
                    }
                    else {
                        max_nbits_ch[gr][ch] = 0;
                    }
                }
                if(nch > 1) {
                    if(max_nbits_ch[gr][0] > use_nbits_ch[gr][0] + 32) {
                        max_nbits_ch[gr][1] += max_nbits_ch[gr][0];
                        max_nbits_ch[gr][1] -= use_nbits_ch[gr][0] + 32;
                        max_nbits_ch[gr][0] = use_nbits_ch[gr][0] + 32;
                    }
                    if(max_nbits_ch[gr][1] > use_nbits_ch[gr][1] + 32) {
                        max_nbits_ch[gr][0] += max_nbits_ch[gr][1];
                        max_nbits_ch[gr][0] -= use_nbits_ch[gr][1] + 32;
                        max_nbits_ch[gr][1] = use_nbits_ch[gr][1] + 32;
                    }
                    for(ch = 0; ch < nch; ++ch) {
                        if(max_nbits_ch[gr][ch] > MAX_BITS_PER_CHANNEL) {
                            max_nbits_ch[gr][ch] = MAX_BITS_PER_CHANNEL;
                        }
                    }
                }
            }
        }
        /* sanity check */
        sum_fr = 0;
        for(gr = 0; gr < ngr; ++gr) {
            int     sum_gr = 0;
            for(ch = 0; ch < nch; ++ch) {
                sum_gr += max_nbits_ch[gr][ch];
                if(max_nbits_ch[gr][ch] > MAX_BITS_PER_CHANNEL) {
                    ok = 0;
                }
            }
            sum_fr += sum_gr;
            if(sum_gr > MAX_BITS_PER_GRANULE) {
                ok = 0;
            }
        }
        if(sum_fr > max_nbits_fr) {
            ok = 0;
        }
        if(!ok) {
            /* we must have done something wrong, fallback to 'on_pe' based constrain */
            for(gr = 0; gr < ngr; ++gr) {
                for(ch = 0; ch < nch; ++ch) {
                    max_nbits_ch[gr][ch] = max_bits[gr][ch];
                }
            }
        }
    }

    /* we already called the 'best_scalefac_store' function, so we need to reset some
     * variables before we can do it again.
     */
    for(ch = 0; ch < nch; ++ch) {
        gfc->l3_side.scfsi[ch][0] = 0;
        gfc->l3_side.scfsi[ch][1] = 0;
        gfc->l3_side.scfsi[ch][2] = 0;
        gfc->l3_side.scfsi[ch][3] = 0;
    }
    for(gr = 0; gr < ngr; ++gr) {
        for(ch = 0; ch < nch; ++ch) {
            gfc->l3_side.tt[gr][ch].scalefac_compress = 0;
        }
    }

    /* alter our encoded data, until it fits into the target bitrate
     */
    use_nbits_fr = 0;
    for(gr = 0; gr < ngr; ++gr) {
        use_nbits_gr[gr] = 0;
        for(ch = 0; ch < nch; ++ch) {
            algo_t const *that = &that_[gr][ch];
            use_nbits_ch[gr][ch] = 0;
            if(max_bits[gr][ch] > 0) {
                int    *sfwork = sfwork_[gr][ch];
                int const *vbrsfmin = vbrsfmin_[gr][ch];
                cutDistribution(sfwork, sfwork, that->cod_info->global_gain);
                outOfBitsStrategy(that, sfwork, vbrsfmin, max_nbits_ch[gr][ch]);
            }
            use_nbits_ch[gr][ch] = reduce_bit_usage(gfc, gr, ch);
            assert(use_nbits_ch[gr][ch] <= max_nbits_ch[gr][ch]);
            use_nbits_gr[gr] += use_nbits_ch[gr][ch];
        }               /* for ch */
        use_nbits_fr += use_nbits_gr[gr];
    }

    /* check bit constrains, but it should always be ok, iff there are no bugs ;-)
     */
    if(use_nbits_fr <= max_nbits_fr) {
        return use_nbits_fr;
    }

    ERRORF(gfc, "INTERNAL ERROR IN VBR NEW CODE (1313), please send bug report\n"
           "maxbits=%d usedbits=%d\n", max_nbits_fr, use_nbits_fr);
    exit(-1);
	}


/*
 *	MPEG layer 3 tables source file
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

/* $Id: tables.c,v 1.29 2011/05/07 16:05:17 rbrito Exp $ */



static const uint16_t t1HB[] = {
    1, 1,
    1, 0
};

static const uint16_t t2HB[] = {
    1, 2, 1,
    3, 1, 1,
    3, 2, 0
};

static const uint16_t t3HB[] = {
    3, 2, 1,
    1, 1, 1,
    3, 2, 0
};

static const uint16_t t5HB[] = {
    1, 2, 6, 5,
    3, 1, 4, 4,
    7, 5, 7, 1,
    6, 1, 1, 0
};

static const uint16_t t6HB[] = {
    7, 3, 5, 1,
    6, 2, 3, 2,
    5, 4, 4, 1,
    3, 3, 2, 0
};

static const uint16_t t7HB[] = {
    1, 2, 10, 19, 16, 10,
    3, 3, 7, 10, 5, 3,
    11, 4, 13, 17, 8, 4,
    12, 11, 18, 15, 11, 2,
    7, 6, 9, 14, 3, 1,
    6, 4, 5, 3, 2, 0
};

static const uint16_t t8HB[] = {
    3, 4, 6, 18, 12, 5,
    5, 1, 2, 16, 9, 3,
    7, 3, 5, 14, 7, 3,
    19, 17, 15, 13, 10, 4,
    13, 5, 8, 11, 5, 1,
    12, 4, 4, 1, 1, 0
};

static const uint16_t t9HB[] = {
    7, 5, 9, 14, 15, 7,
    6, 4, 5, 5, 6, 7,
    7, 6, 8, 8, 8, 5,
    15, 6, 9, 10, 5, 1,
    11, 7, 9, 6, 4, 1,
    14, 4, 6, 2, 6, 0
};

static const uint16_t t10HB[] = {
    1, 2, 10, 23, 35, 30, 12, 17,
    3, 3, 8, 12, 18, 21, 12, 7,
    11, 9, 15, 21, 32, 40, 19, 6,
    14, 13, 22, 34, 46, 23, 18, 7,
    20, 19, 33, 47, 27, 22, 9, 3,
    31, 22, 41, 26, 21, 20, 5, 3,
    14, 13, 10, 11, 16, 6, 5, 1,
    9, 8, 7, 8, 4, 4, 2, 0
};

static const uint16_t t11HB[] = {
    3, 4, 10, 24, 34, 33, 21, 15,
    5, 3, 4, 10, 32, 17, 11, 10,
    11, 7, 13, 18, 30, 31, 20, 5,
    25, 11, 19, 59, 27, 18, 12, 5,
    35, 33, 31, 58, 30, 16, 7, 5,
    28, 26, 32, 19, 17, 15, 8, 14,
    14, 12, 9, 13, 14, 9, 4, 1,
    11, 4, 6, 6, 6, 3, 2, 0
};

static const uint16_t t12HB[] = {
    9, 6, 16, 33, 41, 39, 38, 26,
    7, 5, 6, 9, 23, 16, 26, 11,
    17, 7, 11, 14, 21, 30, 10, 7,
    17, 10, 15, 12, 18, 28, 14, 5,
    32, 13, 22, 19, 18, 16, 9, 5,
    40, 17, 31, 29, 17, 13, 4, 2,
    27, 12, 11, 15, 10, 7, 4, 1,
    27, 12, 8, 12, 6, 3, 1, 0
};

static const uint16_t t13HB[] = {
    1, 5, 14, 21, 34, 51, 46, 71, 42, 52, 68, 52, 67, 44, 43, 19,
    3, 4, 12, 19, 31, 26, 44, 33, 31, 24, 32, 24, 31, 35, 22, 14,
    15, 13, 23, 36, 59, 49, 77, 65, 29, 40, 30, 40, 27, 33, 42, 16,
    22, 20, 37, 61, 56, 79, 73, 64, 43, 76, 56, 37, 26, 31, 25, 14,
    35, 16, 60, 57, 97, 75, 114, 91, 54, 73, 55, 41, 48, 53, 23, 24,
    58, 27, 50, 96, 76, 70, 93, 84, 77, 58, 79, 29, 74, 49, 41, 17,
    47, 45, 78, 74, 115, 94, 90, 79, 69, 83, 71, 50, 59, 38, 36, 15,
    72, 34, 56, 95, 92, 85, 91, 90, 86, 73, 77, 65, 51, 44, 43, 42,
    43, 20, 30, 44, 55, 78, 72, 87, 78, 61, 46, 54, 37, 30, 20, 16,
    53, 25, 41, 37, 44, 59, 54, 81, 66, 76, 57, 54, 37, 18, 39, 11,
    35, 33, 31, 57, 42, 82, 72, 80, 47, 58, 55, 21, 22, 26, 38, 22,
    53, 25, 23, 38, 70, 60, 51, 36, 55, 26, 34, 23, 27, 14, 9, 7,
    34, 32, 28, 39, 49, 75, 30, 52, 48, 40, 52, 28, 18, 17, 9, 5,
    45, 21, 34, 64, 56, 50, 49, 45, 31, 19, 12, 15, 10, 7, 6, 3,
    48, 23, 20, 39, 36, 35, 53, 21, 16, 23, 13, 10, 6, 1, 4, 2,
    16, 15, 17, 27, 25, 20, 29, 11, 17, 12, 16, 8, 1, 1, 0, 1
};

static const uint16_t t15HB[] = {
    7, 12, 18, 53, 47, 76, 124, 108, 89, 123, 108, 119, 107, 81, 122, 63,
    13, 5, 16, 27, 46, 36, 61, 51, 42, 70, 52, 83, 65, 41, 59, 36,
    19, 17, 15, 24, 41, 34, 59, 48, 40, 64, 50, 78, 62, 80, 56, 33,
    29, 28, 25, 43, 39, 63, 55, 93, 76, 59, 93, 72, 54, 75, 50, 29,
    52, 22, 42, 40, 67, 57, 95, 79, 72, 57, 89, 69, 49, 66, 46, 27,
    77, 37, 35, 66, 58, 52, 91, 74, 62, 48, 79, 63, 90, 62, 40, 38,
    125, 32, 60, 56, 50, 92, 78, 65, 55, 87, 71, 51, 73, 51, 70, 30,
    109, 53, 49, 94, 88, 75, 66, 122, 91, 73, 56, 42, 64, 44, 21, 25,
    90, 43, 41, 77, 73, 63, 56, 92, 77, 66, 47, 67, 48, 53, 36, 20,
    71, 34, 67, 60, 58, 49, 88, 76, 67, 106, 71, 54, 38, 39, 23, 15,
    109, 53, 51, 47, 90, 82, 58, 57, 48, 72, 57, 41, 23, 27, 62, 9,
    86, 42, 40, 37, 70, 64, 52, 43, 70, 55, 42, 25, 29, 18, 11, 11,
    118, 68, 30, 55, 50, 46, 74, 65, 49, 39, 24, 16, 22, 13, 14, 7,
    91, 44, 39, 38, 34, 63, 52, 45, 31, 52, 28, 19, 14, 8, 9, 3,
    123, 60, 58, 53, 47, 43, 32, 22, 37, 24, 17, 12, 15, 10, 2, 1,
    71, 37, 34, 30, 28, 20, 17, 26, 21, 16, 10, 6, 8, 6, 2, 0
};

static const uint16_t t16HB[] = {
    1, 5, 14, 44, 74, 63, 110, 93, 172, 149, 138, 242, 225, 195, 376, 17,
    3, 4, 12, 20, 35, 62, 53, 47, 83, 75, 68, 119, 201, 107, 207, 9,
    15, 13, 23, 38, 67, 58, 103, 90, 161, 72, 127, 117, 110, 209, 206, 16,
    45, 21, 39, 69, 64, 114, 99, 87, 158, 140, 252, 212, 199, 387, 365, 26,
    75, 36, 68, 65, 115, 101, 179, 164, 155, 264, 246, 226, 395, 382, 362, 9,
    66, 30, 59, 56, 102, 185, 173, 265, 142, 253, 232, 400, 388, 378, 445, 16,
    111, 54, 52, 100, 184, 178, 160, 133, 257, 244, 228, 217, 385, 366, 715, 10,
    98, 48, 91, 88, 165, 157, 148, 261, 248, 407, 397, 372, 380, 889, 884, 8,
    85, 84, 81, 159, 156, 143, 260, 249, 427, 401, 392, 383, 727, 713, 708, 7,
    154, 76, 73, 141, 131, 256, 245, 426, 406, 394, 384, 735, 359, 710, 352, 11,
    139, 129, 67, 125, 247, 233, 229, 219, 393, 743, 737, 720, 885, 882, 439, 4,
    243, 120, 118, 115, 227, 223, 396, 746, 742, 736, 721, 712, 706, 223, 436, 6,
    202, 224, 222, 218, 216, 389, 386, 381, 364, 888, 443, 707, 440, 437, 1728, 4,
    747, 211, 210, 208, 370, 379, 734, 723, 714, 1735, 883, 877, 876, 3459, 865, 2,
    377, 369, 102, 187, 726, 722, 358, 711, 709, 866, 1734, 871, 3458, 870, 434, 0,
    12, 10, 7, 11, 10, 17, 11, 9, 13, 12, 10, 7, 5, 3, 1, 3
};

static const uint16_t t24HB[] = {
    15, 13, 46, 80, 146, 262, 248, 434, 426, 669, 653, 649, 621, 517, 1032, 88,
    14, 12, 21, 38, 71, 130, 122, 216, 209, 198, 327, 345, 319, 297, 279, 42,
    47, 22, 41, 74, 68, 128, 120, 221, 207, 194, 182, 340, 315, 295, 541, 18,
    81, 39, 75, 70, 134, 125, 116, 220, 204, 190, 178, 325, 311, 293, 271, 16,
    147, 72, 69, 135, 127, 118, 112, 210, 200, 188, 352, 323, 306, 285, 540, 14,
    263, 66, 129, 126, 119, 114, 214, 202, 192, 180, 341, 317, 301, 281, 262, 12,
    249, 123, 121, 117, 113, 215, 206, 195, 185, 347, 330, 308, 291, 272, 520, 10,
    435, 115, 111, 109, 211, 203, 196, 187, 353, 332, 313, 298, 283, 531, 381, 17,
    427, 212, 208, 205, 201, 193, 186, 177, 169, 320, 303, 286, 268, 514, 377, 16,
    335, 199, 197, 191, 189, 181, 174, 333, 321, 305, 289, 275, 521, 379, 371, 11,
    668, 184, 183, 179, 175, 344, 331, 314, 304, 290, 277, 530, 383, 373, 366, 10,
    652, 346, 171, 168, 164, 318, 309, 299, 287, 276, 263, 513, 375, 368, 362, 6,
    648, 322, 316, 312, 307, 302, 292, 284, 269, 261, 512, 376, 370, 364, 359, 4,
    620, 300, 296, 294, 288, 282, 273, 266, 515, 380, 374, 369, 365, 361, 357, 2,
    1033, 280, 278, 274, 267, 264, 259, 382, 378, 372, 367, 363, 360, 358, 356, 0,
    43, 20, 19, 17, 15, 13, 11, 9, 7, 6, 4, 7, 5, 3, 1, 3
};

static const uint16_t t32HB[] = {
    1 << 0, 5 << 1, 4 << 1, 5 << 2, 6 << 1, 5 << 2, 4 << 2, 4 << 3,
    7 << 1, 3 << 2, 6 << 2, 0 << 3, 7 << 2, 2 << 3, 3 << 3, 1 << 4
};

static const uint16_t t33HB[] = {
    15 << 0, 14 << 1, 13 << 1, 12 << 2, 11 << 1, 10 << 2, 9 << 2, 8 << 3,
    7 << 1, 6 << 2, 5 << 2, 4 << 3, 3 << 2, 2 << 3, 1 << 3, 0 << 4
};


const uint8_t t1l[] = {
    1, 4,
    3, 5
};

static const uint8_t t2l[] = {
    1, 4, 7,
    4, 5, 7,
    6, 7, 8
};

static const uint8_t t3l[] = {
    2, 3, 7,
    4, 4, 7,
    6, 7, 8
};

static const uint8_t t5l[] = {
    1, 4, 7, 8,
    4, 5, 8, 9,
    7, 8, 9, 10,
    8, 8, 9, 10
};

static const uint8_t t6l[] = {
    3, 4, 6, 8,
    4, 4, 6, 7,
    5, 6, 7, 8,
    7, 7, 8, 9
};

static const uint8_t t7l[] = {
    1, 4, 7, 9, 9, 10,
    4, 6, 8, 9, 9, 10,
    7, 7, 9, 10, 10, 11,
    8, 9, 10, 11, 11, 11,
    8, 9, 10, 11, 11, 12,
    9, 10, 11, 12, 12, 12
};

static const uint8_t t8l[] = {
    2, 4, 7, 9, 9, 10,
    4, 4, 6, 10, 10, 10,
    7, 6, 8, 10, 10, 11,
    9, 10, 10, 11, 11, 12,
    9, 9, 10, 11, 12, 12,
    10, 10, 11, 11, 13, 13
};

static const uint8_t t9l[] = {
    3, 4, 6, 7, 9, 10,
    4, 5, 6, 7, 8, 10,
    5, 6, 7, 8, 9, 10,
    7, 7, 8, 9, 9, 10,
    8, 8, 9, 9, 10, 11,
    9, 9, 10, 10, 11, 11
};

static const uint8_t t10l[] = {
    1, 4, 7, 9, 10, 10, 10, 11,
    4, 6, 8, 9, 10, 11, 10, 10,
    7, 8, 9, 10, 11, 12, 11, 11,
    8, 9, 10, 11, 12, 12, 11, 12,
    9, 10, 11, 12, 12, 12, 12, 12,
    10, 11, 12, 12, 13, 13, 12, 13,
    9, 10, 11, 12, 12, 12, 13, 13,
    10, 10, 11, 12, 12, 13, 13, 13
};

static const uint8_t t11l[] = {
    2, 4, 6, 8, 9, 10, 9, 10,
    4, 5, 6, 8, 10, 10, 9, 10,
    6, 7, 8, 9, 10, 11, 10, 10,
    8, 8, 9, 11, 10, 12, 10, 11,
    9, 10, 10, 11, 11, 12, 11, 12,
    9, 10, 11, 12, 12, 13, 12, 13,
    9, 9, 9, 10, 11, 12, 12, 12,
    9, 9, 10, 11, 12, 12, 12, 12
};

static const uint8_t t12l[] = {
    4, 4, 6, 8, 9, 10, 10, 10,
    4, 5, 6, 7, 9, 9, 10, 10,
    6, 6, 7, 8, 9, 10, 9, 10,
    7, 7, 8, 8, 9, 10, 10, 10,
    8, 8, 9, 9, 10, 10, 10, 11,
    9, 9, 10, 10, 10, 11, 10, 11,
    9, 9, 9, 10, 10, 11, 11, 12,
    10, 10, 10, 11, 11, 11, 11, 12
};

static const uint8_t t13l[] = {
    1, 5, 7, 8, 9, 10, 10, 11, 10, 11, 12, 12, 13, 13, 14, 14,
    4, 6, 8, 9, 10, 10, 11, 11, 11, 11, 12, 12, 13, 14, 14, 14,
    7, 8, 9, 10, 11, 11, 12, 12, 11, 12, 12, 13, 13, 14, 15, 15,
    8, 9, 10, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 15, 15,
    9, 9, 11, 11, 12, 12, 13, 13, 12, 13, 13, 14, 14, 15, 15, 16,
    10, 10, 11, 12, 12, 12, 13, 13, 13, 13, 14, 13, 15, 15, 16, 16,
    10, 11, 12, 12, 13, 13, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16,
    11, 11, 12, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 16, 18, 18,
    10, 10, 11, 12, 12, 13, 13, 14, 14, 14, 14, 15, 15, 16, 17, 17,
    11, 11, 12, 12, 13, 13, 13, 15, 14, 15, 15, 16, 16, 16, 18, 17,
    11, 12, 12, 13, 13, 14, 14, 15, 14, 15, 16, 15, 16, 17, 18, 19,
    12, 12, 12, 13, 14, 14, 14, 14, 15, 15, 15, 16, 17, 17, 17, 18,
    12, 13, 13, 14, 14, 15, 14, 15, 16, 16, 17, 17, 17, 18, 18, 18,
    13, 13, 14, 15, 15, 15, 16, 16, 16, 16, 16, 17, 18, 17, 18, 18,
    14, 14, 14, 15, 15, 15, 17, 16, 16, 19, 17, 17, 17, 19, 18, 18,
    13, 14, 15, 16, 16, 16, 17, 16, 17, 17, 18, 18, 21, 20, 21, 18
};

static const uint8_t t15l[] = {
    3, 5, 6, 8, 8, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 14,
    5, 5, 7, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13,
    6, 7, 7, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12, 13, 13, 13,
    7, 8, 8, 9, 9, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13,
    8, 8, 9, 9, 10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 13,
    9, 9, 9, 10, 10, 10, 11, 11, 11, 11, 12, 12, 13, 13, 13, 14,
    10, 9, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 14, 14,
    10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 14,
    10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 14, 14, 14,
    10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14,
    11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 15, 14,
    11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15,
    12, 12, 11, 12, 12, 12, 13, 13, 13, 13, 13, 13, 14, 14, 15, 15,
    12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 14, 15, 15,
    13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 14, 15,
    13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 15, 15, 15, 15
};

static const uint8_t t16_5l[] = {
    1, 5, 7, 9, 10, 10, 11, 11, 12, 12, 12, 13, 13, 13, 14, 11,
    4, 6, 8, 9, 10, 11, 11, 11, 12, 12, 12, 13, 14, 13, 14, 11,
    7, 8, 9, 10, 11, 11, 12, 12, 13, 12, 13, 13, 13, 14, 14, 12,
    9, 9, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 14, 15, 15, 13,
    10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 12,
    10, 10, 11, 11, 12, 13, 13, 14, 13, 14, 14, 15, 15, 15, 16, 13,
    11, 11, 11, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 16, 13,
    11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 15, 17, 17, 13,
    11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 13,
    12, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 15, 16, 15, 14,
    12, 13, 12, 13, 14, 14, 14, 14, 15, 16, 16, 16, 17, 17, 16, 13,
    13, 13, 13, 13, 14, 14, 15, 16, 16, 16, 16, 16, 16, 15, 16, 14,
    13, 14, 14, 14, 14, 15, 15, 15, 15, 17, 16, 16, 16, 16, 18, 14,
    15, 14, 14, 14, 15, 15, 16, 16, 16, 18, 17, 17, 17, 19, 17, 14,
    14, 15, 13, 14, 16, 16, 15, 16, 16, 17, 18, 17, 19, 17, 16, 14,
    11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 12
};

static const uint8_t t16l[] = {
    1, 5, 7, 9, 10, 10, 11, 11, 12, 12, 12, 13, 13, 13, 14, 10,
    4, 6, 8, 9, 10, 11, 11, 11, 12, 12, 12, 13, 14, 13, 14, 10,
    7, 8, 9, 10, 11, 11, 12, 12, 13, 12, 13, 13, 13, 14, 14, 11,
    9, 9, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 14, 15, 15, 12,
    10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 11,
    10, 10, 11, 11, 12, 13, 13, 14, 13, 14, 14, 15, 15, 15, 16, 12,
    11, 11, 11, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 16, 12,
    11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 15, 17, 17, 12,
    11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 12,
    12, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 15, 16, 15, 13,
    12, 13, 12, 13, 14, 14, 14, 14, 15, 16, 16, 16, 17, 17, 16, 12,
    13, 13, 13, 13, 14, 14, 15, 16, 16, 16, 16, 16, 16, 15, 16, 13,
    13, 14, 14, 14, 14, 15, 15, 15, 15, 17, 16, 16, 16, 16, 18, 13,
    15, 14, 14, 14, 15, 15, 16, 16, 16, 18, 17, 17, 17, 19, 17, 13,
    14, 15, 13, 14, 16, 16, 15, 16, 16, 17, 18, 17, 19, 17, 16, 13,
    10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 10
};

static const uint8_t t24l[] = {
    4, 5, 7, 8, 9, 10, 10, 11, 11, 12, 12, 12, 12, 12, 13, 10,
    5, 6, 7, 8, 9, 10, 10, 11, 11, 11, 12, 12, 12, 12, 12, 10,
    7, 7, 8, 9, 9, 10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 9,
    8, 8, 9, 9, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 9,
    9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 12, 13, 9,
    10, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 9,
    10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 9,
    11, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 10,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 10,
    11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 10,
    12, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 10,
    12, 12, 11, 11, 11, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 10,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 10,
    12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 10,
    13, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 10,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 6
};

const uint8_t t32l[] = {
    1 + 0, 4 + 1, 4 + 1, 5 + 2, 4 + 1, 6 + 2, 5 + 2, 6 + 3,
    4 + 1, 5 + 2, 5 + 2, 6 + 3, 5 + 2, 6 + 3, 6 + 3, 6 + 4
};

const uint8_t t33l[] = {
    4 + 0, 4 + 1, 4 + 1, 4 + 2, 4 + 1, 4 + 2, 4 + 2, 4 + 3,
    4 + 1, 4 + 2, 4 + 2, 4 + 3, 4 + 2, 4 + 3, 4 + 3, 4 + 4
	};



// https://stackoverflow.com/questions/8534526/how-to-initialize-an-array-of-struct-in-c
const struct HUFFCODETAB ht[HTN] = {
    /* xlen, linmax, table, hlen */
    {0, 0, NULL, NULL},
    {2, 0, (unsigned short*)t1HB, (unsigned char*)t1l},
    {3, 0, (unsigned short*)t2HB, (unsigned char*)t2l},
    {3, 0, (unsigned short*)t3HB, (unsigned char*)t3l},
    {0, 0, NULL, NULL}, /* Apparently not used */
    {4, 0, (unsigned short*)t5HB, (unsigned char*)t5l},
    {4, 0, (unsigned short*)t6HB, (unsigned char*)t6l},
    {6, 0, (unsigned short*)t7HB, (unsigned char*)t7l},
    {6, 0, (unsigned short*)t8HB, (unsigned char*)t8l},
    {6, 0, (unsigned short*)t9HB, (unsigned char*)t9l},
    {8, 0, (unsigned short*)t10HB, (unsigned char*)t10l},
    {8, 0, (unsigned short*)t11HB, (unsigned char*)t11l},
    {8, 0, (unsigned short*)t12HB, (unsigned char*)t12l},
    {16, 0, (unsigned short*)t13HB, (unsigned char*)t13l},
    {0, 0, NULL, (unsigned char*)t16_5l}, /* Apparently not used */
    {16, 0, (unsigned short*)t15HB, (unsigned char*)t15l},

    {1, 1, (unsigned short*)t16HB, (unsigned char*)t16l},
    {2, 3, (unsigned short*)t16HB, (unsigned char*)t16l},
    {3, 7, (unsigned short*)t16HB, (unsigned char*)t16l},
    {4, 15, (unsigned short*)t16HB, (unsigned char*)t16l},
    {6, 63, (unsigned short*)t16HB, (unsigned char*)t16l},
    {8, 255, (unsigned short*)t16HB, (unsigned char*)t16l},
    {10, 1023, (unsigned short*)t16HB, (unsigned char*)t16l},
    {13, 8191, (unsigned short*)t16HB, (unsigned char*)t16l},

    {4, 15, (unsigned short*)t24HB, (unsigned char*)t24l},
    {5, 31, (unsigned short*)t24HB, (unsigned char*)t24l},
    {6, 63, (unsigned short*)t24HB, (unsigned char*)t24l},
    {7, 127, (unsigned short*)t24HB, (unsigned char*)t24l},
    {8, 255, (unsigned short*)t24HB, (unsigned char*)t24l},
    {9, 511, (unsigned short*)t24HB, (unsigned char*)t24l},
    {11, 2047, (unsigned short*)t24HB, (unsigned char*)t24l},
    {13, 8191, (unsigned short*)t24HB, (unsigned char*)t24l},

    {0, 0, (unsigned short*)t32HB, (unsigned char*)t32l},
    {0, 0, (unsigned short*)t33HB, (unsigned char*)t33l},

	};





/*  for(i = 0; i < 16*16; i++) {
 *      largetbl[i] = ((ht[16].hlen[i]) << 16) + ht[24].hlen[i];
 *  }
 */
const uint32_t largetbl[16 * 16] = {
    0x010004, 0x050005, 0x070007, 0x090008, 0x0a0009, 0x0a000a, 0x0b000a, 0x0b000b,
    0x0c000b, 0x0c000c, 0x0c000c, 0x0d000c, 0x0d000c, 0x0d000c, 0x0e000d, 0x0a000a,
    0x040005, 0x060006, 0x080007, 0x090008, 0x0a0009, 0x0b000a, 0x0b000a, 0x0b000b,
    0x0c000b, 0x0c000b, 0x0c000c, 0x0d000c, 0x0e000c, 0x0d000c, 0x0e000c, 0x0a000a,
    0x070007, 0x080007, 0x090008, 0x0a0009, 0x0b0009, 0x0b000a, 0x0c000a, 0x0c000b,
    0x0d000b, 0x0c000b, 0x0d000b, 0x0d000c, 0x0d000c, 0x0e000c, 0x0e000d, 0x0b0009,
    0x090008, 0x090008, 0x0a0009, 0x0b0009, 0x0b000a, 0x0c000a, 0x0c000a, 0x0c000b,
    0x0d000b, 0x0d000b, 0x0e000b, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x0c0009,
    0x0a0009, 0x0a0009, 0x0b0009, 0x0b000a, 0x0c000a, 0x0c000a, 0x0d000a, 0x0d000b,
    0x0d000b, 0x0e000b, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x0f000d, 0x0b0009,
    0x0a000a, 0x0a0009, 0x0b000a, 0x0b000a, 0x0c000a, 0x0d000a, 0x0d000b, 0x0e000b,
    0x0d000b, 0x0e000b, 0x0e000c, 0x0f000c, 0x0f000c, 0x0f000c, 0x10000c, 0x0c0009,
    0x0b000a, 0x0b000a, 0x0b000a, 0x0c000a, 0x0d000a, 0x0d000b, 0x0d000b, 0x0d000b,
    0x0e000b, 0x0e000c, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x10000d, 0x0c0009,
    0x0b000b, 0x0b000a, 0x0c000a, 0x0c000a, 0x0d000b, 0x0d000b, 0x0d000b, 0x0e000b,
    0x0e000c, 0x0f000c, 0x0f000c, 0x0f000c, 0x0f000c, 0x11000d, 0x11000d, 0x0c000a,
    0x0b000b, 0x0c000b, 0x0c000b, 0x0d000b, 0x0d000b, 0x0d000b, 0x0e000b, 0x0e000b,
    0x0f000b, 0x0f000c, 0x0f000c, 0x0f000c, 0x10000c, 0x10000d, 0x10000d, 0x0c000a,
    0x0c000b, 0x0c000b, 0x0c000b, 0x0d000b, 0x0d000b, 0x0e000b, 0x0e000b, 0x0f000c,
    0x0f000c, 0x0f000c, 0x0f000c, 0x10000c, 0x0f000d, 0x10000d, 0x0f000d, 0x0d000a,
    0x0c000c, 0x0d000b, 0x0c000b, 0x0d000b, 0x0e000b, 0x0e000c, 0x0e000c, 0x0e000c,
    0x0f000c, 0x10000c, 0x10000c, 0x10000d, 0x11000d, 0x11000d, 0x10000d, 0x0c000a,
    0x0d000c, 0x0d000c, 0x0d000b, 0x0d000b, 0x0e000b, 0x0e000c, 0x0f000c, 0x10000c,
    0x10000c, 0x10000c, 0x10000c, 0x10000d, 0x10000d, 0x0f000d, 0x10000d, 0x0d000a,
    0x0d000c, 0x0e000c, 0x0e000c, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x0f000c,
    0x0f000c, 0x11000c, 0x10000d, 0x10000d, 0x10000d, 0x10000d, 0x12000d, 0x0d000a,
    0x0f000c, 0x0e000c, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x10000c, 0x10000c,
    0x10000d, 0x12000d, 0x11000d, 0x11000d, 0x11000d, 0x13000d, 0x11000d, 0x0d000a,
    0x0e000d, 0x0f000c, 0x0d000c, 0x0e000c, 0x10000c, 0x10000c, 0x0f000c, 0x10000d,
    0x10000d, 0x11000d, 0x12000d, 0x11000d, 0x13000d, 0x11000d, 0x10000d, 0x0d000a,
    0x0a0009, 0x0a0009, 0x0a0009, 0x0b0009, 0x0b0009, 0x0c0009, 0x0c0009, 0x0c0009,
    0x0d0009, 0x0d0009, 0x0d0009, 0x0d000a, 0x0d000a, 0x0d000a, 0x0d000a, 0x0a0006
};

/*  for(i = 0; i < 3*3; i++) {
 *      table23[i] = ((ht[2].hlen[i]) << 16) + ht[3].hlen[i];
 *  }
 */
const uint32_t table23[3 * 3] = {
    0x010002, 0x040003, 0x070007,
    0x040004, 0x050004, 0x070007,
    0x060006, 0x070007, 0x080008
};

/*   for(i = 0; i < 4*4; i++) {
 *       table56[i] = ((ht[5].hlen[i]) << 16) + ht[6].hlen[i];
 *   }
 */
const uint32_t table56[4 * 4] = {
    0x010003, 0x040004, 0x070006, 0x080008, 0x040004, 0x050004, 0x080006, 0x090007,
    0x070005, 0x080006, 0x090007, 0x0a0008, 0x080007, 0x080007, 0x090008, 0x0a0009
};



/* 
 * 0: MPEG-2 LSF
 * 1: MPEG-1
 * 2: MPEG-2.5 LSF FhG extention                  (1995-07-11 shn)
 */

typedef enum {
    MPEG_2 = 0,
    MPEG_1 = 1,
    MPEG_25 = 2
} MPEG_t;

const int bitrate_table[3][16] = {
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, -1}, /* MPEG 2 */
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1}, /* MPEG 1 */
    {0, 8, 16, 24, 32, 40, 48, 56, 64, -1, -1, -1, -1, -1, -1, -1}, /* MPEG 2.5 */
};

const int samplerate_table[3][4] = {
    {22050, 24000, 16000, -1}, /* MPEG 2 */
    {44100, 48000, 32000, -1}, /* MPEG 1 */
    {11025, 12000, 8000, -1}, /* MPEG 2.5 */
};

int lame_get_bitrate(int mpeg_version, int table_index) {

    if(0 <= mpeg_version && mpeg_version <= 2) {
        if(0 <= table_index && table_index <= 15) {
            return bitrate_table[mpeg_version][table_index];
        }
    }
    return -1;
	}

int lame_get_samplerate(int mpeg_version, int table_index) {

    if(0 <= mpeg_version && mpeg_version <= 2) {
        if(0 <= table_index && table_index <= 3) {
            return samplerate_table[mpeg_version][table_index];
        }
    }
  return -1;
	}


/* This is the scfsi_band table from 2.4.2.7 of the IS */
const int scfsi_band[5] = { 0, 6, 11, 16, 21 };

/* end of tables.c */



/*
 *	MP3 huffman table selecting and bit counting
 *
 *	Copyright (c) 1999-2005 Takehiro TOMINAGA
 *	Copyright (c) 2002-2005 Gabriel Bouvigne
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

/* $Id: takehiro.c,v 1.80 2017/09/06 15:07:30 robert Exp $ */


static const struct {
    /*const tolto idem*/ int region0_count;
    /*const */ int region1_count;
} subdv_table[23] = {
    {0, 0},              /* 0 bands */
    {0, 0},              /* 1 bands */
    {0, 0},              /* 2 bands */
    {0, 0},              /* 3 bands */
    {0, 0},              /* 4 bands */
    {0, 1},              /* 5 bands */
    {1, 1},              /* 6 bands */
    {1, 1},              /* 7 bands */
    {1, 2},              /* 8 bands */
    {2, 2},              /* 9 bands */
    {2, 3},              /* 10 bands */
    {2, 3},              /* 11 bands */
    {3, 4},              /* 12 bands */
    {3, 4},              /* 13 bands */
    {3, 4},              /* 14 bands */
    {4, 5},              /* 15 bands */
    {4, 5},              /* 16 bands */
    {4, 6},              /* 17 bands */
    {5, 6},              /* 18 bands */
    {5, 6},              /* 19 bands */
    {5, 7},              /* 20 bands */
    {6, 7},              /* 21 bands */
    {6, 7},              /* 22 bands */
};





/*********************************************************************
 * nonlinear quantization of xr 
 * More accurate formula than the ISO formula.  Takes into account
 * the fact that we are quantizing xr -> ix, but we want ix^4/3 to be 
 * as close as possible to x^4/3.  (taking the nearest int would mean
 * ix is as close as possible to xr, which is different.)
 *
 * From Segher Boessenkool <segher@eastsite.nl>  11/1999
 *
 * 09/2000: ASM code removed in favor of IEEE754 hack by Takehiro
 * Tominaga. If you need the ASM code, check CVS circa Aug 2000.
 *
 * 01/2004: Optimizations by Gabriel Bouvigne
 *********************************************************************/


static void quantize_lines_xrpow_01(unsigned int l, FLOAT istep, const FLOAT * xr, int *ix) {
    const FLOAT compareval0 = (1.0f - 0.4054f) / istep;
    unsigned int i;

    assert(l > 0);
    assert(l % 2 == 0);
    for(i = 0; i < l; i += 2) {
        FLOAT const xr_0 = xr[i+0];
        FLOAT const xr_1 = xr[i+1];
        int const ix_0 = (compareval0 > xr_0) ? 0 : 1;
        int const ix_1 = (compareval0 > xr_1) ? 0 : 1;
        ix[i+0] = ix_0;
        ix[i+1] = ix_1;
    }
}



#ifdef TAKEHIRO_IEEE754_HACK

typedef union {
    float   f;
    int     i;
} fi_union;

#define MAGIC_FLOAT (65536*(128))
#define MAGIC_INT 0x4b000000


static void quantize_lines_xrpow(unsigned int l, FLOAT istep, const FLOAT * xp, int *pi) {
    fi_union *fi;
    unsigned int remaining;

    assert(l > 0);

    fi = (fi_union *) pi;

    l = l >> 1;
    remaining = l % 2;
    l = l >> 1;
    while(l--) {
        double  x0 = istep * xp[0];
        double  x1 = istep * xp[1];
        double  x2 = istep * xp[2];
        double  x3 = istep * xp[3];

        x0 += MAGIC_FLOAT;
        fi[0].f = x0;
        x1 += MAGIC_FLOAT;
        fi[1].f = x1;
        x2 += MAGIC_FLOAT;
        fi[2].f = x2;
        x3 += MAGIC_FLOAT;
        fi[3].f = x3;

        fi[0].f = x0 + adj43asm[fi[0].i - MAGIC_INT];
        fi[1].f = x1 + adj43asm[fi[1].i - MAGIC_INT];
        fi[2].f = x2 + adj43asm[fi[2].i - MAGIC_INT];
        fi[3].f = x3 + adj43asm[fi[3].i - MAGIC_INT];

        fi[0].i -= MAGIC_INT;
        fi[1].i -= MAGIC_INT;
        fi[2].i -= MAGIC_INT;
        fi[3].i -= MAGIC_INT;
        fi += 4;
        xp += 4;
    };
    if(remaining) {
        double  x0 = istep * xp[0];
        double  x1 = istep * xp[1];

        x0 += MAGIC_FLOAT;
        fi[0].f = x0;
        x1 += MAGIC_FLOAT;
        fi[1].f = x1;

        fi[0].f = x0 + adj43asm[fi[0].i - MAGIC_INT];
        fi[1].f = x1 + adj43asm[fi[1].i - MAGIC_INT];

        fi[0].i -= MAGIC_INT;
        fi[1].i -= MAGIC_INT;
    }

	}


#else

/*********************************************************************
 * XRPOW_FTOI is a macro to convert floats to ints.  
 * if XRPOW_FTOI(x) = nearest_int(x), then QUANTFAC(x)=adj43asm[x]
 *                                         ROUNDFAC= -0.0946
 *
 * if XRPOW_FTOI(x) = floor(x), then QUANTFAC(x)=asj43[x]   
 *                                   ROUNDFAC=0.4054
 *
 * Note: using floor() or (int) is extremely slow. On machines where
 * the TAKEHIRO_IEEE754_HACK code above does not work, it is worthwile
 * to write some ASM for XRPOW_FTOI().  
 *********************************************************************/
#define XRPOW_FTOI(src,dest) ((dest) = (int)(src))
#define QUANTFAC(rx)  adj43[rx]
#define ROUNDFAC 0.4054


static void quantize_lines_xrpow(unsigned int l, FLOAT istep, const FLOAT * xr, int *ix) {
    unsigned int remaining;

    assert(l > 0);

    l = l >> 1;
    remaining = l % 2;
    l = l >> 1;
    while(l--) {
        FLOAT   x0, x1, x2, x3;
        int     rx0, rx1, rx2, rx3;

        x0 = *xr++ * istep;
        x1 = *xr++ * istep;
        XRPOW_FTOI(x0, rx0);
        x2 = *xr++ * istep;
        XRPOW_FTOI(x1, rx1);
        x3 = *xr++ * istep;
        XRPOW_FTOI(x2, rx2);
        x0 += QUANTFAC(rx0);
        XRPOW_FTOI(x3, rx3);
        x1 += QUANTFAC(rx1);
        XRPOW_FTOI(x0, *ix++);
        x2 += QUANTFAC(rx2);
        XRPOW_FTOI(x1, *ix++);
        x3 += QUANTFAC(rx3);
        XRPOW_FTOI(x2, *ix++);
        XRPOW_FTOI(x3, *ix++);
    };
    if(remaining) {
        FLOAT   x0, x1;
        int     rx0, rx1;

        x0 = *xr++ * istep;
        x1 = *xr++ * istep;
        XRPOW_FTOI(x0, rx0);
        XRPOW_FTOI(x1, rx1);
        x0 += QUANTFAC(rx0);
        x1 += QUANTFAC(rx1);
        XRPOW_FTOI(x0, *ix++);
        XRPOW_FTOI(x1, *ix++);
    }

	}



#endif



/*********************************************************************
 * Quantization function
 * This function will select which lines to quantize and call the
 * proper quantization function
 *********************************************************************/

static void quantize_xrpow(const FLOAT * xp, int *pi, FLOAT istep, gr_info const *const cod_info,
               calc_noise_data const *prev_noise) {
    /* quantize on xr^(3/4) instead of xr */
    int     sfb;
    int     sfbmax;
    int     j = 0;
    int     prev_data_use;
    int    *iData;
    int     accumulate = 0;
    int     accumulate01 = 0;
    int    *acc_iData;
    const FLOAT *acc_xp;

    iData = pi;
    acc_xp = xp;
    acc_iData = iData;


    /* Reusing previously computed data does not seems to work if global gain
       is changed. Finding why it behaves this way would allow to use a cache of 
       previously computed values (let's 10 cached values per sfb) that would 
       probably provide a noticeable speedup */
    prev_data_use = (prev_noise && (cod_info->global_gain == prev_noise->global_gain));

    if(cod_info->block_type == SHORT_TYPE)
        sfbmax = 38;
    else
        sfbmax = 21;

    for(sfb = 0; sfb <= sfbmax; sfb++) {
        int     step = -1;

        if(prev_data_use || cod_info->block_type == NORM_TYPE) {
            step =
                cod_info->global_gain
                - ((cod_info->scalefac[sfb] + (cod_info->preflag ? pretab[sfb] : 0))
                   << (cod_info->scalefac_scale + 1))
                - cod_info->subblock_gain[cod_info->window[sfb]] * 8;
        }
        assert(cod_info->width[sfb] >= 0);
        if(prev_data_use && (prev_noise->step[sfb] == step)) {
            /* do not recompute this part,
               but compute accumulated lines */
            if(accumulate) {
                quantize_lines_xrpow(accumulate, istep, acc_xp, acc_iData);
                accumulate = 0;
            }
            if(accumulate01) {
                quantize_lines_xrpow_01(accumulate01, istep, acc_xp, acc_iData);
                accumulate01 = 0;
            }
        }
        else {          /*should compute this part */
            int     l;
            l = cod_info->width[sfb];

            if((j + cod_info->width[sfb]) > cod_info->max_nonzero_coeff) {
                /*do not compute upper zero part */
                int     usefullsize;
                usefullsize = cod_info->max_nonzero_coeff - j + 1;
                memset(&pi[cod_info->max_nonzero_coeff], 0,
                       sizeof(int) * (576 - cod_info->max_nonzero_coeff));
                l = usefullsize;

                if(l < 0) {
                    l = 0;
                }

                /* no need to compute higher sfb values */
                sfb = sfbmax + 1;
            }

            /*accumulate lines to quantize */
            if(!accumulate && !accumulate01) {
                acc_iData = iData;
                acc_xp = xp;
            }
            if(prev_noise &&
                prev_noise->sfb_count1 > 0 &&
                sfb >= prev_noise->sfb_count1 &&
                prev_noise->step[sfb] > 0 && step >= prev_noise->step[sfb]) {

                if(accumulate) {
                    quantize_lines_xrpow(accumulate, istep, acc_xp, acc_iData);
                    accumulate = 0;
                    acc_iData = iData;
                    acc_xp = xp;
                }
                accumulate01 += l;
            }
            else {
                if(accumulate01) {
                    quantize_lines_xrpow_01(accumulate01, istep, acc_xp, acc_iData);
                    accumulate01 = 0;
                    acc_iData = iData;
                    acc_xp = xp;
                }
                accumulate += l;
            }

            if(l <= 0) {
                /*  rh: 20040215
                 *  may happen due to "prev_data_use" optimization 
                 */
                if(accumulate01) {
                    quantize_lines_xrpow_01(accumulate01, istep, acc_xp, acc_iData);
                    accumulate01 = 0;
                }
                if(accumulate) {
                    quantize_lines_xrpow(accumulate, istep, acc_xp, acc_iData);
                    accumulate = 0;
                }

                break;  /* ends for-loop */
            }
        }
        if(sfb <= sfbmax) {
            iData += cod_info->width[sfb];
            xp += cod_info->width[sfb];
            j += cod_info->width[sfb];
        }
    }
    if(accumulate) {   /*last data part */
        quantize_lines_xrpow(accumulate, istep, acc_xp, acc_iData);
        accumulate = 0;
    }
    if(accumulate01) { /*last data part */
        quantize_lines_xrpow_01(accumulate01, istep, acc_xp, acc_iData);
        accumulate01 = 0;
    }

	}




/*************************************************************************/
/*	      ix_max							 */
/*************************************************************************/
static int ix_max(const int *ix, const int *end) {
    int     max1 = 0, max2 = 0;

    do {
        int const x1 = *ix++;
        int const x2 = *ix++;
        if(max1 < x1)
            max1 = x1;

        if(max2 < x2)
            max2 = x2;
    } while(ix < end);
    if(max1 < max2)
        max1 = max2;
    return max1;
	}




static int count_bit_ESC(const int *ix, const int *const end, int t1, const int t2, unsigned int *const s) {
    /* ESC-table is used */
    unsigned int const linbits = ht[t1].xlen * 65536u + ht[t2].xlen;
    unsigned int sum = 0, sum2;

    do {
        unsigned int x = *ix++;
        unsigned int y = *ix++;

        if(x >= 15u) {
            x = 15u;
            sum += linbits;
        }
        if(y >= 15u) {
            y = 15u;
            sum += linbits;
        }
        x <<= 4u;
        x += y;
        sum += largetbl[x];
    } while(ix < end);

    sum2 = sum & 0xffffu;
    sum >>= 16u;

    if(sum > sum2) {
        sum = sum2;
        t1 = t2;
    }

    *s += sum;
    return t1;
	}


static int count_bit_noESC(const int *ix, const int *end, int mx, unsigned int *s) {
    /* No ESC-words */
    unsigned int sum1 = 0;
    const uint8_t *const hlen1 = ht[1].hlen;
    mx;

    do {
        unsigned int const x0 = *ix++;
        unsigned int const x1 = *ix++;
        sum1 += hlen1[ x0+x0 + x1 ];
    } while(ix < end);

    *s += sum1;
    return 1;
	}


static const int huf_tbl_noESC[] = {
    1, 2, 5, 7, 7, 10, 10, 13, 13, 13, 13, 13, 13, 13, 13
};


static int count_bit_noESC_from2(const int *ix, const int *end, int max, unsigned int *s) {
    int t1 = huf_tbl_noESC[max - 1];
    /* No ESC-words */
    const unsigned int xlen = ht[t1].xlen;
    uint32_t const* table = (t1 == 2) ? &table23[0] : &table56[0];
    unsigned int sum = 0, sum2;

    do {
        unsigned int const x0 = *ix++;
        unsigned int const x1 = *ix++;
        sum += table[ x0 * xlen + x1 ];
    } while(ix < end);

    sum2 = sum & 0xffffu;
    sum >>= 16u;

    if(sum > sum2) {
        sum = sum2;
        t1++;
    }

    *s += sum;
    return t1;
	}


inline static int count_bit_noESC_from3(const int *ix, const int *end, int max, unsigned int * s) {
    int t1 = huf_tbl_noESC[max - 1];
    /* No ESC-words */
    unsigned int sum1 = 0;
    unsigned int sum2 = 0;
    unsigned int sum3 = 0;
    const unsigned int xlen = ht[t1].xlen;
    const uint8_t *const hlen1 = ht[t1].hlen;
    const uint8_t *const hlen2 = ht[t1 + 1].hlen;
    const uint8_t *const hlen3 = ht[t1 + 2].hlen;
    int     t;

    do {
        unsigned int x0 = *ix++;
        unsigned int x1 = *ix++;
        unsigned int x = x0 * xlen + x1;
        sum1 += hlen1[x];
        sum2 += hlen2[x];
        sum3 += hlen3[x];
    } while(ix < end);

    t = t1;
    if(sum1 > sum2) {
        sum1 = sum2;
        t++;
    }
    if(sum1 > sum3) {
        sum1 = sum3;
        t = t1 + 2;
    }
    *s += sum1;

    return t;  
	}


/*************************************************************************/
/*	      choose table						 */
/*************************************************************************/

/*
  Choose the Huffman table that will encode ix[begin..end] with
  the fewest bits.

  Note: This code contains knowledge about the sizes and characteristics
  of the Huffman tables as defined in the IS (Table B.7), and will not work
  with any arbitrary tables.
*/
static int count_bit_null(const int* ix, const int* end, int max, unsigned int* s) {
    ix;
    end;
    max;
    s;

    return 0;
	}

typedef int (*count_fnc)(const int* ix, const int* end, int max, unsigned int* s);
  
static const count_fnc count_fncs[] = {
	&count_bit_null
, &count_bit_noESC
, &count_bit_noESC_from2
, &count_bit_noESC_from2
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
, &count_bit_noESC_from3
};

static int choose_table_nonMMX(const int *ix, const int *const end, int *const _s) {
    unsigned int* s = (unsigned int*)_s;
    unsigned int  max;
    int     choice, choice2;
    max = ix_max(ix, end);

    if(max <= 15) {
      return count_fncs[max](ix, end, max, s);
    }
    /* try tables with linbits */
    if(max > IXMAX_VAL) {
        *s = LARGE_BITS;
        return -1;
    }
    max -= 15u;
    for(choice2 = 24; choice2 < 32; choice2++) {
        if(ht[choice2].linmax >= max) {
            break;
        }
    }

    for(choice = choice2 - 8; choice < 24; choice++) {
        if(ht[choice].linmax >= max) {
            break;
        }
    }
    return count_bit_ESC(ix, end, choice, choice2, s);
	}



/*************************************************************************/
/*	      count_bit							 */
/*************************************************************************/
int noquant_count_bits(lame_internal_flags const *const gfc,
                   gr_info * const gi, calc_noise_data * prev_noise) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     bits = 0;
    int     i, a1, a2;
    int const *const ix = gi->l3_enc;

    i = Min(576, ((gi->max_nonzero_coeff + 2) >> 1) << 1);

    if(prev_noise)
        prev_noise->sfb_count1 = 0;

    /* Determine count1 region */
    for(; i > 1; i -= 2)
        if(ix[i - 1] | ix[i - 2])
            break;
    gi->count1 = i;

    /* Determines the number of bits to encode the quadruples. */
    a1 = a2 = 0;
    for(; i > 3; i -= 4) {
        int x4 = ix[i-4];
        int x3 = ix[i-3];
        int x2 = ix[i-2];
        int x1 = ix[i-1];
        int     p;
        /* hack to check if all values <= 1 */
        if((unsigned int) (x4 | x3 | x2 | x1) > 1)
            break;

        p = ((x4 * 2 + x3) * 2 + x2) * 2 + x1;
        a1 += t32l[p];
        a2 += t33l[p];
    }

    bits = a1;
    gi->count1table_select = 0;
    if(a1 > a2) {
        bits = a2;
        gi->count1table_select = 1;
    }

    gi->count1bits = bits;
    gi->big_values = i;
    if(i == 0)
        return bits;

    if(gi->block_type == SHORT_TYPE) {
        a1 = 3 * gfc->scalefac_band.s[3];
        if(a1 > gi->big_values)
            a1 = gi->big_values;
        a2 = gi->big_values;

    }
    else if(gi->block_type == NORM_TYPE) {
        assert(i <= 576); /* bv_scf has 576 entries (0..575) */
        a1 = gi->region0_count = gfc->sv_qnt.bv_scf[i - 2];
        a2 = gi->region1_count = gfc->sv_qnt.bv_scf[i - 1];

        assert(a1 + a2 + 2 < SBPSY_l);
        a2 = gfc->scalefac_band.l[a1 + a2 + 2];
        a1 = gfc->scalefac_band.l[a1 + 1];
        if(a2 < i)
            gi->table_select[2] = gfc->choose_table(ix + a2, ix + i, &bits);

    }
    else {
        gi->region0_count = 7;
        /*gi->region1_count = SBPSY_l - 7 - 1; */
        gi->region1_count = SBMAX_l - 1 - 7 - 1;
        a1 = gfc->scalefac_band.l[7 + 1];
        a2 = i;
        if(a1 > a2) {
            a1 = a2;
        }
    }


    /* have to allow for the case when bigvalues < region0 < region1 */
    /* (and region0, region1 are ignored) */
    a1 = Min(a1, i);
    a2 = Min(a2, i);

    assert(a1 >= 0);
    assert(a2 >= 0);

    /* Count the number of bits necessary to code the bigvalues region. */
    if(0 < a1)
        gi->table_select[0] = gfc->choose_table(ix, ix + a1, &bits);
    if(a1 < a2)
        gi->table_select[1] = gfc->choose_table(ix + a1, ix + a2, &bits);
    if(cfg->use_best_huffman == 2) {
        gi->part2_3_length = bits;
        best_huffman_divide(gfc, gi);
        bits = gi->part2_3_length;
    }


    if(prev_noise) {
        if(gi->block_type == NORM_TYPE) {
            int     sfb = 0;
            while(gfc->scalefac_band.l[sfb] < gi->big_values) {
                sfb++;
            }
            prev_noise->sfb_count1 = sfb;
        }
    }

  return bits;
	}

int count_bits(lame_internal_flags const *const gfc,
           const FLOAT * const xr, gr_info * const gi, calc_noise_data * prev_noise) {
    int    *const ix = gi->l3_enc;

    /* since quantize_xrpow uses table lookup, we need to check this first: */
    FLOAT const w = (IXMAX_VAL) / IPOW20(gi->global_gain);

    if(gi->xrpow_max > w)
        return LARGE_BITS;

    quantize_xrpow(xr, ix, IPOW20(gi->global_gain), gi, prev_noise);

    if(gfc->sv_qnt.substep_shaping & 2) {
        int     sfb, j = 0;
        /* 0.634521682242439 = 0.5946*2**(.5*0.1875) */
        int const gain = gi->global_gain + gi->scalefac_scale;
        const FLOAT roundfac = 0.634521682242439 / IPOW20(gain);
        for(sfb = 0; sfb < gi->sfbmax; sfb++) {
            int const width = gi->width[sfb];
            assert(width >= 0);
            if(!gfc->sv_qnt.pseudohalf[sfb]) {
                j += width;
            }
            else {
                int     k;
                for(k = j, j += width; k < j; ++k) {
                    ix[k] = (xr[k] >= roundfac) ? ix[k] : 0;
                }
            }
        }
    }

  return noquant_count_bits(gfc, gi, prev_noise);
	}

/***********************************************************************
  re-calculate the best scalefac_compress using scfsi
  the saved bits are kept in the bit reservoir.
 **********************************************************************/
inline static void recalc_divide_init(const lame_internal_flags * const gfc,
                   gr_info const *cod_info,
                   int const *const ix, int r01_bits[], int r01_div[], int r0_tbl[], int r1_tbl[]) {
    int     r0, r1, bigv, r0t, r1t, bits;

    bigv = cod_info->big_values;

    for(r0 = 0; r0 <= 7 + 15; r0++) {
        r01_bits[r0] = LARGE_BITS;
    }

    for(r0 = 0; r0 < 16; r0++) {
        int const a1 = gfc->scalefac_band.l[r0 + 1];
        int     r0bits;
        if(a1 >= bigv)
            break;
        r0bits = 0;
        r0t = gfc->choose_table(ix, ix + a1, &r0bits);

        for(r1 = 0; r1 < 8; r1++) {
            int const a2 = gfc->scalefac_band.l[r0 + r1 + 2];
            if(a2 >= bigv)
                break;

            bits = r0bits;
            r1t = gfc->choose_table(ix + a1, ix + a2, &bits);
            if(r01_bits[r0 + r1] > bits) {
                r01_bits[r0 + r1] = bits;
                r01_div[r0 + r1] = r0;
                r0_tbl[r0 + r1] = r0t;
                r1_tbl[r0 + r1] = r1t;
            }
        }
    }
	}

inline static void recalc_divide_sub(const lame_internal_flags * const gfc,
                  const gr_info * cod_info2,
                  gr_info * const gi,
                  const int *const ix,
                  const int r01_bits[], const int r01_div[], const int r0_tbl[], const int r1_tbl[]) {
    int     bits, r2, a2, bigv, r2t;

    bigv = cod_info2->big_values;

    for(r2 = 2; r2 < SBMAX_l + 1; r2++) {
        a2 = gfc->scalefac_band.l[r2];
        if(a2 >= bigv)
            break;

        bits = r01_bits[r2 - 2] + cod_info2->count1bits;
        if(gi->part2_3_length <= bits)
            break;

        r2t = gfc->choose_table(ix + a2, ix + bigv, &bits);
        if(gi->part2_3_length <= bits)
            continue;

        memcpy(gi, cod_info2, sizeof(gr_info));
        gi->part2_3_length = bits;
        gi->region0_count = r01_div[r2 - 2];
        gi->region1_count = r2 - 2 - r01_div[r2 - 2];
        gi->table_select[0] = r0_tbl[r2 - 2];
        gi->table_select[1] = r1_tbl[r2 - 2];
        gi->table_select[2] = r2t;
    }
	}



void best_huffman_divide(const lame_internal_flags * const gfc, gr_info * const gi) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     i, a1, a2;
    gr_info cod_info2;
    int const *const ix = gi->l3_enc;

    int     r01_bits[7 + 15 + 1];
    int     r01_div[7 + 15 + 1];
    int     r0_tbl[7 + 15 + 1];
    int     r1_tbl[7 + 15 + 1];


    /* SHORT BLOCK stuff fails for MPEG2 */
    if(gi->block_type == SHORT_TYPE && cfg->mode_gr == 1)
        return;


    memcpy(&cod_info2, gi, sizeof(gr_info));
    if(gi->block_type == NORM_TYPE) {
        recalc_divide_init(gfc, gi, ix, r01_bits, r01_div, r0_tbl, r1_tbl);
        recalc_divide_sub(gfc, &cod_info2, gi, ix, r01_bits, r01_div, r0_tbl, r1_tbl);
    }

    i = cod_info2.big_values;
    if(i == 0 || (unsigned int) (ix[i - 2] | ix[i - 1]) > 1)
        return;

    i = gi->count1 + 2;
    if(i > 576)
        return;

    /* Determines the number of bits to encode the quadruples. */
    memcpy(&cod_info2, gi, sizeof(gr_info));
    cod_info2.count1 = i;
    a1 = a2 = 0;

    assert(i <= 576);

    for(; i > cod_info2.big_values; i -= 4) {
        int const p = ((ix[i - 4] * 2 + ix[i - 3]) * 2 + ix[i - 2]) * 2 + ix[i - 1];
        a1 += t32l[p];
        a2 += t33l[p];
    }
    cod_info2.big_values = i;

    cod_info2.count1table_select = 0;
    if(a1 > a2) {
        a1 = a2;
        cod_info2.count1table_select = 1;
    }

    cod_info2.count1bits = a1;

    if(cod_info2.block_type == NORM_TYPE)
        recalc_divide_sub(gfc, &cod_info2, gi, ix, r01_bits, r01_div, r0_tbl, r1_tbl);
    else {
        /* Count the number of bits necessary to code the bigvalues region. */
        cod_info2.part2_3_length = a1;
        a1 = gfc->scalefac_band.l[7 + 1];
        if(a1 > i) {
            a1 = i;
        }
        if(a1 > 0)
            cod_info2.table_select[0] =
                gfc->choose_table(ix, ix + a1, (int *) &cod_info2.part2_3_length);
        if(i > a1)
            cod_info2.table_select[1] =
                gfc->choose_table(ix + a1, ix + i, (int *) &cod_info2.part2_3_length);
        if(gi->part2_3_length > cod_info2.part2_3_length)
            memcpy(gi, &cod_info2, sizeof(gr_info));
    }
	}

static const int slen1_n[16] = { 1, 1, 1, 1, 8, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16 };
static const int slen2_n[16] = { 1, 2, 4, 8, 1, 2, 4, 8, 2, 4, 8, 2, 4, 8, 4, 8 };
const int slen1_tab[16] = { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
const int slen2_tab[16] = { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 };

static void scfsi_calc(int ch, III_side_info_t * l3_side) {
    unsigned int i;
    int     s1, s2, c1, c2;
    int     sfb;
    gr_info *const gi = &l3_side->tt[1][ch];
    gr_info const *const g0 = &l3_side->tt[0][ch];

    for(i = 0; i < (sizeof(scfsi_band) / sizeof(int)) - 1; i++) {
        for(sfb = scfsi_band[i]; sfb < scfsi_band[i + 1]; sfb++) {
            if(g0->scalefac[sfb] != gi->scalefac[sfb]
                && gi->scalefac[sfb] >= 0)
                break;
        }
        if(sfb == scfsi_band[i + 1]) {
            for(sfb = scfsi_band[i]; sfb < scfsi_band[i + 1]; sfb++) {
                gi->scalefac[sfb] = -1;
            }
            l3_side->scfsi[ch][i] = 1;
        }
    }

    s1 = c1 = 0;
    for(sfb = 0; sfb < 11; sfb++) {
        if(gi->scalefac[sfb] == -1)
            continue;
        c1++;
        if(s1 < gi->scalefac[sfb])
            s1 = gi->scalefac[sfb];
    }

    s2 = c2 = 0;
    for(; sfb < SBPSY_l; sfb++) {
        if(gi->scalefac[sfb] == -1)
            continue;
        c2++;
        if(s2 < gi->scalefac[sfb])
            s2 = gi->scalefac[sfb];
    }

    for(i = 0; i < 16; i++) {
        if(s1 < slen1_n[i] && s2 < slen2_n[i]) {
            int const c = slen1_tab[i] * c1 + slen2_tab[i] * c2;
            if(gi->part2_length > c) {
                gi->part2_length = c;
                gi->scalefac_compress = (int)i;
            }
        }
    }
	}

/*
Find the optimal way to store the scalefactors.
Only call this routine after final scalefactors have been
chosen and the channel/granule will not be re-encoded.
 */
void best_scalefac_store(const lame_internal_flags * gfc,
                    const int gr, const int ch, III_side_info_t * const l3_side) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    /* use scalefac_scale if we can */
    gr_info *const gi = &l3_side->tt[gr][ch];
    int     sfb, i, j, l;
    int     recalc = 0;

    /* remove scalefacs from bands with ix=0.  This idea comes
     * from the AAC ISO docs.  added mt 3/00 */
    /* check if l3_enc=0 */
    j = 0;
    for(sfb = 0; sfb < gi->sfbmax; sfb++) {
        int const width = gi->width[sfb];
        assert(width >= 0);
        for(l = j, j += width; l < j; ++l) {
            if(gi->l3_enc[l] != 0)
                break;
        }
        if(l == j)
            gi->scalefac[sfb] = recalc = -2; /* anything goes. */
        /*  only best_scalefac_store and calc_scfsi 
         *  know--and only they should know--about the magic number -2. 
         */
    }

    if(!gi->scalefac_scale && !gi->preflag) {
        int     s = 0;
        for(sfb = 0; sfb < gi->sfbmax; sfb++)
            if(gi->scalefac[sfb] > 0)
                s |= gi->scalefac[sfb];

        if(!(s & 1) && s != 0) {
            for(sfb = 0; sfb < gi->sfbmax; sfb++)
                if(gi->scalefac[sfb] > 0)
                    gi->scalefac[sfb] >>= 1;

            gi->scalefac_scale = recalc = 1;
        }
    }

    if(!gi->preflag && gi->block_type != SHORT_TYPE && cfg->mode_gr == 2) {
        for(sfb = 11; sfb < SBPSY_l; sfb++)
            if(gi->scalefac[sfb] < pretab[sfb] && gi->scalefac[sfb] != -2)
                break;
        if(sfb == SBPSY_l) {
            for(sfb = 11; sfb < SBPSY_l; sfb++)
                if(gi->scalefac[sfb] > 0)
                    gi->scalefac[sfb] -= pretab[sfb];

            gi->preflag = recalc = 1;
        }
    }

    for(i = 0; i < 4; i++)
        l3_side->scfsi[ch][i] = 0;

    if(cfg->mode_gr == 2 && gr == 1
        && l3_side->tt[0][ch].block_type != SHORT_TYPE
        && l3_side->tt[1][ch].block_type != SHORT_TYPE) {
        scfsi_calc(ch, l3_side);
        recalc = 0;
    }
    for(sfb = 0; sfb < gi->sfbmax; sfb++) {
        if(gi->scalefac[sfb] == -2) {
            gi->scalefac[sfb] = 0; /* if anything goes, then 0 is a good choice */
        }
    }
    if(recalc) {
        scale_bitcount(gfc, gi);
    }
}


#ifndef NDEBUG
static int all_scalefactors_not_negative(int const *scalefac, int n) {
    int     i;

    for(i = 0; i < n; ++i) {
        if(scalefac[i] < 0)
            return 0;
    }
    return 1;
	}
#endif


/* number of bits used to encode scalefacs */

/* 18*slen1_tab[i] + 18*slen2_tab[i] */
static const int scale_short[16] = {
    0, 18, 36, 54, 54, 36, 54, 72, 54, 72, 90, 72, 90, 108, 108, 126
};

/* 17*slen1_tab[i] + 18*slen2_tab[i] */
static const int scale_mixed[16] = {
    0, 18, 36, 54, 51, 35, 53, 71, 52, 70, 88, 69, 87, 105, 104, 122
};

/* 11*slen1_tab[i] + 10*slen2_tab[i] */
static const int scale_long[16] = {
    0, 10, 20, 30, 33, 21, 31, 41, 32, 42, 52, 43, 53, 63, 64, 74
};


/*************************************************************************/
/*            scale_bitcount                                             */
/*************************************************************************/

/* Also calculates the number of bits necessary to code the scalefactors. */

static int mpeg1_scale_bitcount(const lame_internal_flags * gfc, gr_info * const cod_info) {
    int     k, sfb, max_slen1 = 0, max_slen2 = 0;

    /* maximum values */
    const int *tab;
    int    *const scalefac = cod_info->scalefac;

    gfc;
    assert(all_scalefactors_not_negative(scalefac, cod_info->sfbmax));

    if(cod_info->block_type == SHORT_TYPE) {
        tab = scale_short;
        if(cod_info->mixed_block_flag)
            tab = scale_mixed;
    }
    else {              /* block_type == 1,2,or 3 */
        tab = scale_long;
        if(!cod_info->preflag) {
            for(sfb = 11; sfb < SBPSY_l; sfb++)
                if(scalefac[sfb] < pretab[sfb])
                    break;

            if(sfb == SBPSY_l) {
                cod_info->preflag = 1;
                for(sfb = 11; sfb < SBPSY_l; sfb++)
                    scalefac[sfb] -= pretab[sfb];
            }
        }
    }

    for(sfb = 0; sfb < cod_info->sfbdivide; sfb++)
        if(max_slen1 < scalefac[sfb])
            max_slen1 = scalefac[sfb];

    for(; sfb < cod_info->sfbmax; sfb++)
        if(max_slen2 < scalefac[sfb])
            max_slen2 = scalefac[sfb];

    /* from Takehiro TOMINAGA <tominaga@isoternet.org> 10/99
     * loop over *all* posible values of scalefac_compress to find the
     * one which uses the smallest number of bits.  ISO would stop
     * at first valid index */
    cod_info->part2_length = LARGE_BITS;
    for(k = 0; k < 16; k++) {
        if(max_slen1 < slen1_n[k] && max_slen2 < slen2_n[k]
            && cod_info->part2_length > tab[k]) {
            cod_info->part2_length = tab[k];
            cod_info->scalefac_compress = k;
        }
    }
    return cod_info->part2_length == LARGE_BITS;
	}



/*
  table of largest scalefactor values for MPEG2
*/
static const int max_range_sfac_tab[6][4] = {
    {15, 15, 7, 7},
    {15, 15, 7, 0},
    {7, 3, 0, 0},
    {15, 31, 31, 0},
    {7, 7, 7, 0},
    {3, 3, 0, 0}
	};




/*************************************************************************/
/*            scale_bitcount_lsf                                         */
/*************************************************************************/

/* Also counts the number of bits to encode the scalefacs but for MPEG 2 */
/* Lower sampling frequencies  (24, 22.05 and 16 kHz.)                   */

/*  This is reverse-engineered from section 2.4.3.2 of the MPEG2 IS,     */
/* "Audio Decoding Layer III"                                            */
static int mpeg2_scale_bitcount(const lame_internal_flags * gfc, gr_info * const cod_info) {
    int     table_number, row_in_table, partition, nr_sfb, window, over;
    int     i, sfb, max_sfac[4];
    const int *partition_table;
    int const *const scalefac = cod_info->scalefac;

    /*
       Set partition table. Note that should try to use table one,
       but do not yet...
     */
    if(cod_info->preflag)
        table_number = 2;
    else
        table_number = 0;

    for(i = 0; i < 4; i++)
        max_sfac[i] = 0;

    if(cod_info->block_type == SHORT_TYPE) {
        row_in_table = 1;
        partition_table = &nr_of_sfb_block[table_number][row_in_table][0];
        for(sfb = 0, partition = 0; partition < 4; partition++) {
            nr_sfb = partition_table[partition] / 3;
            for(i = 0; i < nr_sfb; i++, sfb++)
                for(window = 0; window < 3; window++)
                    if(scalefac[sfb * 3 + window] > max_sfac[partition])
                        max_sfac[partition] = scalefac[sfb * 3 + window];
        }
    }
    else {
        row_in_table = 0;
        partition_table = &nr_of_sfb_block[table_number][row_in_table][0];
        for(sfb = 0, partition = 0; partition < 4; partition++) {
            nr_sfb = partition_table[partition];
            for(i = 0; i < nr_sfb; i++, sfb++)
                if(scalefac[sfb] > max_sfac[partition])
                    max_sfac[partition] = scalefac[sfb];
        }
    }

    for(over = 0, partition = 0; partition < 4; partition++) {
        if(max_sfac[partition] > max_range_sfac_tab[table_number][partition])
            over++;
    }
    if(!over) {
        /*
           Since no bands have been over-amplified, we can set scalefac_compress
           and slen[] for the formatter
         */
        static const int log2tab[] = { 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };

        int     slen1, slen2, slen3, slen4;

        cod_info->sfb_partition_table = nr_of_sfb_block[table_number][row_in_table];
        for(partition = 0; partition < 4; partition++)
            cod_info->slen[partition] = log2tab[max_sfac[partition]];

        /* set scalefac_compress */
        slen1 = cod_info->slen[0];
        slen2 = cod_info->slen[1];
        slen3 = cod_info->slen[2];
        slen4 = cod_info->slen[3];

        switch(table_number) {
        case 0:
            cod_info->scalefac_compress = (((slen1 * 5) + slen2) << 4)
                + (slen3 << 2)
                + slen4;
            break;

        case 1:
            cod_info->scalefac_compress = 400 + (((slen1 * 5) + slen2) << 2)
                + slen3;
            break;

        case 2:
            cod_info->scalefac_compress = 500 + (slen1 * 3) + slen2;
            break;

        default:
            ERRORF(gfc, "intensity stereo not implemented yet\n");
            break;
        }
    }
#ifdef DEBUG
    if(over)
        ERRORF(gfc, "---WARNING !! Amplification of some bands over limits\n");
#endif
    if(!over) {
        assert(cod_info->sfb_partition_table);
        cod_info->part2_length = 0;
        for(partition = 0; partition < 4; partition++)
            cod_info->part2_length +=
                cod_info->slen[partition] * cod_info->sfb_partition_table[partition];
    }

  return over;
	}


int scale_bitcount(const lame_internal_flags * gfc, gr_info * cod_info) {

    if(gfc->cfg.mode_gr == 2) {
        return mpeg1_scale_bitcount(gfc, cod_info);
    }
    else {
        return mpeg2_scale_bitcount(gfc, cod_info);
    }
	}


#ifdef MMX_choose_table
extern int choose_table_MMX(const int *ix, const int *const end, int *const s);
#endif

void huffman_init(lame_internal_flags * const gfc) {
    int     i;

    gfc->choose_table = choose_table_nonMMX;

#ifdef MMX_choose_table
    if(gfc->CPU_features.MMX) {
        gfc->choose_table = choose_table_MMX;
    }
#endif

    for(i = 2; i <= 576; i += 2) {
        int     scfb_anz = 0, bv_index;
        while(gfc->scalefac_band.l[++scfb_anz] < i);

        bv_index = subdv_table[scfb_anz].region0_count;
        while(gfc->scalefac_band.l[bv_index + 1] > i)
            bv_index--;

        if(bv_index < 0) {
            /* this is an indication that everything is going to
               be encoded as region0:  bigvalues < region0 < region1
               so lets set region0, region1 to some value larger
               than bigvalues */
            bv_index = subdv_table[scfb_anz].region0_count;
        }

        gfc->sv_qnt.bv_scf[i - 2] = bv_index;

        bv_index = subdv_table[scfb_anz].region1_count;
        while(gfc->scalefac_band.l[bv_index + gfc->sv_qnt.bv_scf[i - 2] + 2] > i)
            bv_index--;

        if(bv_index < 0) {
            bv_index = subdv_table[scfb_anz].region1_count;
        }

        gfc->sv_qnt.bv_scf[i - 1] = bv_index;
    }
	}



/*
 *      Xing VBR tagging for LAME.
 *
 *      Copyright (c) 1999 A.L. Faber
 *      Copyright (c) 2001 Jonathan Dee
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

/* $Id: VbrTag.c,v 1.106 2017/08/06 18:15:47 robert Exp $ */



#ifdef _DEBUG
/*  #define DEBUG_VBRTAG */
#endif

/*
 *    4 bytes for Header Tag
 *    4 bytes for Header Flags
 *  100 bytes for entry (NUMTOCENTRIES)
 *    4 bytes for FRAME SIZE
 *    4 bytes for STREAM_SIZE
 *    4 bytes for VBR SCALE. a VBR quality indicator: 0=best 100=worst
 *   20 bytes for LAME tag.  for example, "LAME3.12 (beta 6)"
 * ___________
 *  140 bytes
*/
#define VBRHEADERSIZE (NUMTOCENTRIES+4+4+4+4+4)

#define LAMEHEADERSIZE (VBRHEADERSIZE + 9 + 1 + 1 + 8 + 1 + 1 + 3 + 1 + 1 + 2 + 4 + 2 + 2)

/* the size of the Xing header (MPEG1 and MPEG2) in kbps */
#define XING_BITRATE1 128
#define XING_BITRATE2  64
#define XING_BITRATE25 32

extern const char* get_lame_tag_encoder_short_version(void);

static const char VBRTag0[] = { "Xing" };
static const char VBRTag1[] = { "Info" };




/* Lookup table for fast CRC computation
 * See 'CRC_update_lookup'
 * Uses the polynomial x^16+x^15+x^2+1 */

static const unsigned int crc16_lookup[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};





/***********************************************************************
 *  Robert Hegemann 2001-01-17
 ***********************************************************************/
static void addVbr(VBR_seek_info_t * v, int bitrate) {
    int     i;

    v->nVbrNumFrames++;
    v->sum += bitrate;
    v->seen++;

    if(v->seen < v->want) {
        return;
    }

    if(v->pos < v->size) {
        v->bag[v->pos] = v->sum;
        v->pos++;
        v->seen = 0;
    }
    if(v->pos == v->size) {
        for(i = 1; i < v->size; i += 2) {
            v->bag[i / 2] = v->bag[i];
        }
        v->want *= 2;
        v->pos /= 2;
    }
	}

static void Xing_seek_table(VBR_seek_info_t const* v, unsigned char *t) {
    int     i, indx;
    int     seek_point;

    if(v->pos <= 0)
        return;

    for(i = 1; i < NUMTOCENTRIES; ++i) {
        float   j = i / (float) NUMTOCENTRIES, act, sum;
        indx = (int) (floor(j * v->pos));
        if(indx > v->pos - 1)
            indx = v->pos - 1;
        act = v->bag[indx];
        sum = v->sum;
        seek_point = (int) (256. * act / sum);
        if(seek_point > 255)
            seek_point = 255;
        t[i] = seek_point;
    }
	}

#ifdef DEBUG_VBR_SEEKING_TABLE
static void print_seeking(unsigned char *t) {
    int     i;

    printf("seeking table ");
    for(i = 0; i < NUMTOCENTRIES; ++i) {
        printf(" %d ", t[i]);
    }
    printf("\n");
	}
#endif


/****************************************************************************
 * AddVbrFrame: Add VBR entry, used to fill the VBR the TOC entries
 * Paramters:
 *      nStreamPos: how many bytes did we write to the bitstream so far
 *                              (in Bytes NOT Bits)
 ****************************************************************************
*/
void AddVbrFrame(lame_internal_flags * gfc) {
    int     kbps = bitrate_table[gfc->cfg.version][gfc->ov_enc.bitrate_index];

    assert(gfc->VBR_seek_table.bag);
    addVbr(&gfc->VBR_seek_table, kbps);
	}


/*-------------------------------------------------------------*/
static int ExtractI4(const unsigned char *buf) {
  int     x;

  /* big endian extract */
  x = buf[0];
  x <<= 8;
  x |= buf[1];
  x <<= 8;
  x |= buf[2];
  x <<= 8;
  x |= buf[3];
  return x;
	}

static void CreateI4(unsigned char *buf, uint32_t nValue) {

  /* big endian create */
  buf[0] = (nValue >> 24) & 0xff;
  buf[1] = (nValue >> 16) & 0xff;
  buf[2] = (nValue >> 8) & 0xff;
  buf[3] = (nValue) & 0xff;
	}

static void CreateI2(unsigned char *buf, int nValue) {

    /* big endian create */
    buf[0] = (nValue >> 8) & 0xff;
    buf[1] = (nValue) & 0xff;
	}

/* check for magic strings*/
static int IsVbrTag(const unsigned char *buf) {
    int     isTag0, isTag1;

    isTag0 = ((buf[0] == VBRTag0[0]) && (buf[1] == VBRTag0[1]) && (buf[2] == VBRTag0[2])
              && (buf[3] == VBRTag0[3]));
    isTag1 = ((buf[0] == VBRTag1[0]) && (buf[1] == VBRTag1[1]) && (buf[2] == VBRTag1[2])
              && (buf[3] == VBRTag1[3]));

  return (isTag0 || isTag1);
	}

#define SHIFT_IN_BITS_VALUE(x,n,v) ( x = (x << (n)) | ( (v) & ~(-1 << (n)) ) )

static void setLameTagFrameHeader(lame_internal_flags const *gfc, unsigned char *buffer) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncResult_t const *const eov = &gfc->ov_enc;
    char    abyte, bbyte;

    SHIFT_IN_BITS_VALUE(buffer[0], 8u, 0xffu);

    SHIFT_IN_BITS_VALUE(buffer[1], 3u, 7);
    SHIFT_IN_BITS_VALUE(buffer[1], 1u, (cfg->samplerate_out < 16000) ? 0 : 1);
    SHIFT_IN_BITS_VALUE(buffer[1], 1u, cfg->version);
    SHIFT_IN_BITS_VALUE(buffer[1], 2u, 4 - 3);
    SHIFT_IN_BITS_VALUE(buffer[1], 1u, (!cfg->error_protection) ? 1 : 0);

    SHIFT_IN_BITS_VALUE(buffer[2], 4u, eov->bitrate_index);
    SHIFT_IN_BITS_VALUE(buffer[2], 2u, cfg->samplerate_index);
    SHIFT_IN_BITS_VALUE(buffer[2], 1u, 0);
    SHIFT_IN_BITS_VALUE(buffer[2], 1u, cfg->extension);

    SHIFT_IN_BITS_VALUE(buffer[3], 2u, cfg->mode);
    SHIFT_IN_BITS_VALUE(buffer[3], 2u, eov->mode_ext);
    SHIFT_IN_BITS_VALUE(buffer[3], 1u, cfg->copyright);
    SHIFT_IN_BITS_VALUE(buffer[3], 1u, cfg->original);
    SHIFT_IN_BITS_VALUE(buffer[3], 2u, cfg->emphasis);

    /* the default VBR header. 48 kbps layer III, no padding, no crc */
    /* but sampling freq, mode andy copyright/copy protection taken */
    /* from first valid frame */
    buffer[0] = (uint8_t) 0xff;
    abyte = (buffer[1] & (unsigned char) 0xf1);
    {
        int     bitrate;
        if(1 == cfg->version) {
            bitrate = XING_BITRATE1;
        }
        else {
            if(cfg->samplerate_out < 16000)
                bitrate = XING_BITRATE25;
            else
                bitrate = XING_BITRATE2;
        }

        if(cfg->vbr == vbr_off)
            bitrate = cfg->avg_bitrate;

        if(cfg->free_format)
            bbyte = 0x00;
        else
            bbyte = 16 * BitrateIndex(bitrate, cfg->version, cfg->samplerate_out);
    }

    /* Use as much of the info from the real frames in the
     * Xing header:  samplerate, channels, crc, etc...
     */
    if(cfg->version == 1) {
        /* MPEG1 */
        buffer[1] = abyte | (char) 0x0a; /* was 0x0b; */
        abyte = buffer[2] & (char) 0x0d; /* AF keep also private bit */
        buffer[2] = (char) bbyte | abyte; /* 64kbs MPEG1 frame */
    }
    else {
        /* MPEG2 */
        buffer[1] = abyte | (char) 0x02; /* was 0x03; */
        abyte = buffer[2] & (char) 0x0d; /* AF keep also private bit */
        buffer[2] = (char) bbyte | abyte; /* 64kbs MPEG2 frame */
    }
	}

#if 0
static int CheckVbrTag(unsigned char *buf);

/*-------------------------------------------------------------*/
/* Same as GetVbrTag below, but only checks for the Xing tag.
   requires buf to contain only 40 bytes */
/*-------------------------------------------------------------*/
int CheckVbrTag(unsigned char *buf) {
    int     h_id, h_mode;

    /* get selected MPEG header data */
    h_id = (buf[1] >> 3) & 1;
    h_mode = (buf[3] >> 6) & 3;

    /*  determine offset of header */
    if(h_id) {
        /* mpeg1 */
        if(h_mode != 3)
            buf += (32 + 4);
        else
            buf += (17 + 4);
    }
    else {
        /* mpeg2 */
        if(h_mode != 3)
            buf += (17 + 4);
        else
            buf += (9 + 4);
    }

  return IsVbrTag(buf);
	}
#endif

int GetVbrTag(VBRTAGDATA * pTagData, const unsigned char *buf) {
    int     i, head_flags;
    int     h_bitrate, h_id, h_mode, h_sr_index, h_layer;
    int     enc_delay, enc_padding;

    /* get Vbr header data */
    pTagData->flags = 0;

    /* get selected MPEG header data */
    h_layer = (buf[1] >> 1) & 3;
    if( h_layer != 0x01 ) {
        /* the following code assumes Layer-3, so give up here */
        return 0;
    }
    h_id = (buf[1] >> 3) & 1;
    h_sr_index = (buf[2] >> 2) & 3;
    h_mode = (buf[3] >> 6) & 3;
    h_bitrate = ((buf[2] >> 4) & 0xf);
    h_bitrate = bitrate_table[h_id][h_bitrate];

    /* check for FFE syncword */
    if((buf[1] >> 4) == 0xE)
        pTagData->samprate = samplerate_table[2][h_sr_index];
    else
        pTagData->samprate = samplerate_table[h_id][h_sr_index];
    /* if( h_id == 0 ) */
    /*  pTagData->samprate >>= 1; */



    /*  determine offset of header */
    if(h_id) {
        /* mpeg1 */
        if(h_mode != 3)
            buf += (32 + 4);
        else
            buf += (17 + 4);
    }
    else {
        /* mpeg2 */
        if(h_mode != 3)
            buf += (17 + 4);
        else
            buf += (9 + 4);
    }

    if(!IsVbrTag(buf))
        return 0;

    buf += 4;

    pTagData->h_id = h_id;

    head_flags = pTagData->flags = ExtractI4(buf);
    buf += 4;           /* get flags */

    if(head_flags & FRAMES_FLAG) {
        pTagData->frames = ExtractI4(buf);
        buf += 4;
    }

    if(head_flags & BYTES_FLAG) {
        pTagData->bytes = ExtractI4(buf);
        buf += 4;
    }

    if(head_flags & TOC_FLAG) {
        if(pTagData->toc != NULL) {
            for(i = 0; i < NUMTOCENTRIES; i++)
                pTagData->toc[i] = buf[i];
        }
        buf += NUMTOCENTRIES;
    }

    pTagData->vbr_scale = -1;

    if(head_flags & VBR_SCALE_FLAG) {
        pTagData->vbr_scale = ExtractI4(buf);
        buf += 4;
    }

    pTagData->headersize = ((h_id + 1) * 72000 * h_bitrate) / pTagData->samprate;

    buf += 21;
    enc_delay = buf[0] << 4;
    enc_delay += buf[1] >> 4;
    enc_padding = (buf[1] & 0x0F) << 8;
    enc_padding += buf[2];
    /* check for reasonable values (this may be an old Xing header, */
    /* not a INFO tag) */
    if(enc_delay < 0 || enc_delay > 3000)
        enc_delay = -1;
    if(enc_padding < 0 || enc_padding > 3000)
        enc_padding = -1;

    pTagData->enc_delay = enc_delay;
    pTagData->enc_padding = enc_padding;

#ifdef DEBUG_VBRTAG
    fprintf(stderr, "\n\n********************* VBR TAG INFO *****************\n");
    fprintf(stderr, "tag         :%s\n", VBRTag);
    fprintf(stderr, "head_flags  :%d\n", head_flags);
    fprintf(stderr, "bytes       :%d\n", pTagData->bytes);
    fprintf(stderr, "frames      :%d\n", pTagData->frames);
    fprintf(stderr, "VBR Scale   :%d\n", pTagData->vbr_scale);
    fprintf(stderr, "enc_delay  = %i \n", enc_delay);
    fprintf(stderr, "enc_padding= %i \n", enc_padding);
    fprintf(stderr, "toc:\n");
    if(pTagData->toc != NULL) {
        for(i = 0; i < NUMTOCENTRIES; i++) {
            if((i % 10) == 0)
                fprintf(stderr, "\n");
            fprintf(stderr, " %3d", (int) (pTagData->toc[i]));
        }
    }
    fprintf(stderr, "\n***************** END OF VBR TAG INFO ***************\n");
#endif

  return 1;           /* success */
	}


/****************************************************************************
 * InitVbrTag: Initializes the header, and write empty frame to stream
 * Paramters:
 *                              fpStream: pointer to output file stream
 *                              nMode   : Channel Mode: 0=STEREO 1=JS 2=DS 3=MONO
 ****************************************************************************
*/
int InitVbrTag(lame_global_flags * gfp) {
    lame_internal_flags *gfc = gfp->internal_flags;
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     kbps_header;

#define MAXFRAMESIZE 2880 /* or 0xB40, the max freeformat 640 32kHz framesize */

    /*
     * Xing VBR pretends to be a 48kbs layer III frame.  (at 44.1kHz).
     * (at 48kHz they use 56kbs since 48kbs frame not big enough for
     * table of contents)
     * let's always embed Xing header inside a 64kbs layer III frame.
     * this gives us enough room for a LAME version string too.
     * size determined by sampling frequency (MPEG1)
     * 32kHz:    216 bytes@48kbs    288bytes@ 64kbs
     * 44.1kHz:  156 bytes          208bytes@64kbs     (+1 if padding = 1)
     * 48kHz:    144 bytes          192
     *
     * MPEG 2 values are the same since the framesize and samplerate
     * are each reduced by a factor of 2.
     */


    if(1 == cfg->version) {
        kbps_header = XING_BITRATE1;
    }
    else {
        if(cfg->samplerate_out < 16000)
            kbps_header = XING_BITRATE25;
        else
            kbps_header = XING_BITRATE2;
    }

    if(cfg->vbr == vbr_off)
        kbps_header = cfg->avg_bitrate;

    /** make sure LAME Header fits into Frame
     */
    {
        int     total_frame_size = ((cfg->version + 1) * 72000 * kbps_header) / cfg->samplerate_out;
        int     header_size = (cfg->sideinfo_len + LAMEHEADERSIZE);
        gfc->VBR_seek_table.TotalFrameSize = total_frame_size;
        if(total_frame_size < header_size || total_frame_size > MAXFRAMESIZE) {
            /* disable tag, it wont fit */
            gfc->cfg.write_lame_tag = 0;
            return 0;
        }
    }

    gfc->VBR_seek_table.nVbrNumFrames = 0;
    gfc->VBR_seek_table.nBytesWritten = 0;
    gfc->VBR_seek_table.sum = 0;

    gfc->VBR_seek_table.seen = 0;
    gfc->VBR_seek_table.want = 1;
    gfc->VBR_seek_table.pos = 0;

    if(gfc->VBR_seek_table.bag == NULL) {
        gfc->VBR_seek_table.bag = lame_calloc(int, 400);
        if(gfc->VBR_seek_table.bag != NULL) {
            gfc->VBR_seek_table.size = 400;
        }
        else {
            gfc->VBR_seek_table.size = 0;
            ERRORF(gfc, "Error: can't allocate VbrFrames buffer\n");
            gfc->cfg.write_lame_tag = 0;
            return -1;
        }
    }

    /* write dummy VBR tag of all 0's into bitstream */
    {
        uint8_t buffer[MAXFRAMESIZE];
        size_t  i, n;

        memset(buffer, 0, sizeof(buffer));
        setLameTagFrameHeader(gfc, buffer);
        n = gfc->VBR_seek_table.TotalFrameSize;
        for(i = 0; i < n; ++i) {
            add_dummy_byte(gfc, buffer[i], 1);
        }
    }

  /* Success */
  return 0;
	}



/* fast CRC-16 computation - uses table crc16_lookup 8*/
static uint16_t CRC_update_lookup(uint16_t value, uint16_t crc) {
    uint16_t tmp;

    tmp = crc ^ value;
    crc = (crc >> 8) ^ crc16_lookup[tmp & 0xff];
    return crc;
	}

void UpdateMusicCRC(uint16_t * crc, unsigned char const *buffer, int size) {
  int     i;

  for(i=0; i < size; ++i)
      *crc = CRC_update_lookup(buffer[i], *crc);
	}





/****************************************************************************
 * Jonathan Dee 2001/08/31
 *
 * PutLameVBR: Write LAME info: mini version + info on various switches used
 * Paramters:
 *                              pbtStreamBuffer : pointer to output buffer
 *                              id3v2size               : size of id3v2 tag in bytes
 *                              crc                             : computation of crc-16 of Lame Tag so far (starting at frame sync)
 *
 ****************************************************************************
*/
static int PutLameVBR(lame_global_flags const *gfp, size_t nMusicLength, uint8_t * pbtStreamBuffer, uint16_t crc) {
	lame_internal_flags const *gfc = gfp->internal_flags;
    SessionConfig_t const *const cfg = &gfc->cfg;

    int     nBytesWritten = 0;
    int     i;

    int     enc_delay = gfc->ov_enc.encoder_delay; /* encoder delay */
    int     enc_padding = gfc->ov_enc.encoder_padding; /* encoder padding  */

    /*recall: cfg->vbr_q is for example set by the switch -V  */
    /*   gfp->quality by -q, -h, -f, etc */

    int     nQuality = (100 - 10 * gfp->VBR_q - gfp->quality);


    /*
    NOTE:
            Even though the specification for the LAME VBR tag
            did explicitly mention other encoders than LAME,
            many SW/HW decoder seem to be able to make use of
            this tag only, if the encoder version starts with LAME.
            To be compatible with such decoders, ANY encoder will
            be forced to write a fake LAME version string!
            As a result, the encoder version info becomes worthless.
    */
    const char *szVersion = get_lame_tag_encoder_short_version();
    uint8_t nVBR;
    uint8_t nRevision = 0x00;
    uint8_t nRevMethod;
    uint8_t vbr_type_translator[] = { 1, 5, 3, 2, 4, 0, 3 }; /*numbering different in vbr_mode vs. Lame tag */

    uint8_t nLowpass =
        (((cfg->lowpassfreq / 100.0) + .5) > 255 ? 255 : (cfg->lowpassfreq / 100.0) + .5);

    uint32_t nPeakSignalAmplitude = 0;

    uint16_t nRadioReplayGain = 0;
    uint16_t nAudiophileReplayGain = 0;

    uint8_t nNoiseShaping = cfg->noise_shaping;
    uint8_t nStereoMode = 0;
    int     bNonOptimal = 0;
    uint8_t nSourceFreq = 0;
    uint8_t nMisc = 0;
    uint16_t nMusicCRC = 0;

    /*psy model type: Gpsycho or NsPsytune */
    unsigned char bExpNPsyTune = 1; /* only NsPsytune */
    unsigned char bSafeJoint = (cfg->use_safe_joint_stereo) != 0;

    unsigned char bNoGapMore = 0;
    unsigned char bNoGapPrevious = 0;

    int     nNoGapCount = gfp->nogap_total;
    int     nNoGapCurr = gfp->nogap_current;


    uint8_t nAthType = cfg->ATHtype; /*4 bits. */

    uint8_t nFlags = 0;

    /* if ABR, {store bitrate <=255} else { store "-b"} */
    int     nABRBitrate;
    switch(cfg->vbr) {
			case vbr_abr:{
            nABRBitrate = cfg->vbr_avg_bitrate_kbps;
            break;
        }
			case vbr_off:{
            nABRBitrate = cfg->avg_bitrate;
            break;
        }
			default:{          /*vbr modes */
            nABRBitrate = bitrate_table[cfg->version][cfg->vbr_min_bitrate_index];;
        }
    }


    /*revision and vbr method */
    if(cfg->vbr < sizeof(vbr_type_translator))
        nVBR = vbr_type_translator[cfg->vbr];
    else
        nVBR = 0x00;    /*unknown. */

    nRevMethod = 0x10 * nRevision + nVBR;


    /* ReplayGain */
    if(cfg->findReplayGain) {
        int RadioGain = gfc->ov_rpg.RadioGain;
        if(RadioGain > 0x1FE)
            RadioGain = 0x1FE;
        if(RadioGain < -0x1FE)
            RadioGain = -0x1FE;

        nRadioReplayGain = 0x2000; /* set name code */
        nRadioReplayGain |= 0xC00; /* set originator code to `determined automatically' */

        if(RadioGain >= 0)
            nRadioReplayGain |= RadioGain; /* set gain adjustment */
        else {
            nRadioReplayGain |= 0x200; /* set the sign bit */
            nRadioReplayGain |= -RadioGain; /* set gain adjustment */
        }
    }

    // peak sample 
    if(cfg->findPeakSample)
      nPeakSignalAmplitude =
        abs((int) ((((FLOAT) gfc->ov_rpg.PeakSample) / 32767.0) * pow(2, 23) + .5));

    /*nogap */
    if(nNoGapCount != -1) {
      if(nNoGapCurr > 0)
          bNoGapPrevious = 1;

      if(nNoGapCurr < nNoGapCount - 1)
          bNoGapMore = 1;
			}

    //flags 

    nFlags = nAthType + (bExpNPsyTune << 4)
        + (bSafeJoint << 5)
        + (bNoGapMore << 6)
        + (bNoGapPrevious << 7);


    if(nQuality < 0)
        nQuality = 0;

    //stereo mode field... a bit ugly.

    switch(cfg->mode) {
			case MONO:
        nStereoMode = 0;
        break;
			case STEREO:
        nStereoMode = 1;
        break;
			case DUAL_CHANNEL:
        nStereoMode = 2;
        break;
			case JOINT_STEREO:
        if(cfg->force_ms)
            nStereoMode = 4;
        else
            nStereoMode = 3;
        break;
			case NOT_SET:
        /* FALLTHROUGH */
	    default:
        nStereoMode = 7;
        break;
		  }

    /*Intensity stereo : nStereoMode = 6. IS is not implemented */

    if(cfg->samplerate_in <= 32000)
        nSourceFreq = 0x00;
    else if(cfg->samplerate_in == 48000)
        nSourceFreq = 0x02;
    else if(cfg->samplerate_in > 48000)
        nSourceFreq = 0x03;
    else
        nSourceFreq = 0x01; /*default is 44100Hz. */


    /*Check if the user overrided the default LAME behaviour with some nasty options */

    if(cfg->short_blocks == short_block_forced || cfg->short_blocks == short_block_dispensed || ((cfg->lowpassfreq == -1) && (cfg->highpassfreq == -1)) || /* "-k" */
        (cfg->disable_reservoir && cfg->avg_bitrate < 320) ||
        cfg->noATH || cfg->ATHonly || (nAthType == 0) || cfg->samplerate_in <= 32000)
        bNonOptimal = 1;

    nMisc = nNoiseShaping + (nStereoMode << 2)
        + (bNonOptimal << 5)
        + (nSourceFreq << 6);


    nMusicCRC = gfc->nMusicCRC;


    /*Write all this information into the stream */
    CreateI4(&pbtStreamBuffer[nBytesWritten], nQuality);
    nBytesWritten += 4;

    strncpy((char *) &pbtStreamBuffer[nBytesWritten], szVersion, 9);
    nBytesWritten += 9;

    pbtStreamBuffer[nBytesWritten] = nRevMethod;
    nBytesWritten++;

    pbtStreamBuffer[nBytesWritten] = nLowpass;
    nBytesWritten++;

    CreateI4(&pbtStreamBuffer[nBytesWritten], nPeakSignalAmplitude);
    nBytesWritten += 4;

    CreateI2(&pbtStreamBuffer[nBytesWritten], nRadioReplayGain);
    nBytesWritten += 2;

    CreateI2(&pbtStreamBuffer[nBytesWritten], nAudiophileReplayGain);
    nBytesWritten += 2;

    pbtStreamBuffer[nBytesWritten] = nFlags;
    nBytesWritten++;

    if(nABRBitrate >= 255)
        pbtStreamBuffer[nBytesWritten] = 0xFF;
    else
        pbtStreamBuffer[nBytesWritten] = nABRBitrate;
    nBytesWritten++;

    pbtStreamBuffer[nBytesWritten] = enc_delay >> 4; /* works for win32, does it for unix? */
    pbtStreamBuffer[nBytesWritten + 1] = (enc_delay << 4) + (enc_padding >> 8);
    pbtStreamBuffer[nBytesWritten + 2] = enc_padding;

    nBytesWritten += 3;

    pbtStreamBuffer[nBytesWritten] = nMisc;
    nBytesWritten++;


    pbtStreamBuffer[nBytesWritten++] = 0; /*unused in rev0 */

    CreateI2(&pbtStreamBuffer[nBytesWritten], cfg->preset);
    nBytesWritten += 2;

    CreateI4(&pbtStreamBuffer[nBytesWritten], (int) nMusicLength);
    nBytesWritten += 4;

    CreateI2(&pbtStreamBuffer[nBytesWritten], nMusicCRC);
    nBytesWritten += 2;

    /*Calculate tag CRC.... must be done here, since it includes
     *previous information*/

    for(i = 0; i < nBytesWritten; i++)
        crc = CRC_update_lookup(pbtStreamBuffer[i], crc);

    CreateI2(&pbtStreamBuffer[nBytesWritten], crc);
    nBytesWritten += 2;

  return nBytesWritten;
	}

static long skipId3v2(FILE * fpStream) {
    size_t  nbytes;
    long    id3v2TagSize;
    unsigned char id3v2Header[10];

    /* seek to the beginning of the stream */
    if(fseek(fpStream, 0, SEEK_SET) != 0) {
        return -2;      /* not seekable, abort */
    }
    /* read 10 bytes in case there's an ID3 version 2 header here */
    nbytes = fread(id3v2Header, 1, sizeof(id3v2Header), fpStream);
    if(nbytes != sizeof(id3v2Header)) {
        return -3;      /* not readable, maybe opened Write-Only */
    }
    /* does the stream begin with the ID3 version 2 file identifier? */
    if(!strncmp((char *) id3v2Header, "ID3", 3)) {
        /* the tag size (minus the 10-byte header) is encoded into four
         * bytes where the most significant bit is clear in each byte */
        id3v2TagSize = (((id3v2Header[6] & 0x7f) << 21)
                        | ((id3v2Header[7] & 0x7f) << 14)
                        | ((id3v2Header[8] & 0x7f) << 7)
                        | (id3v2Header[9] & 0x7f))
            + sizeof id3v2Header;
    }
    else {
        /* no ID3 version 2 tag in this stream */
        id3v2TagSize = 0;
    }

  return id3v2TagSize;
	}



size_t lame_get_lametag_frame(lame_global_flags const *gfp, unsigned char *buffer, size_t size) {
    lame_internal_flags *gfc;
    SessionConfig_t const *cfg;
    unsigned long stream_size;
    unsigned int  nStreamIndex;
    uint8_t btToc[NUMTOCENTRIES];

    if(!gfp) {
        return 0;
    }
    gfc = gfp->internal_flags;
    if(gfc == 0) {
        return 0;
			}
    if(!is_lame_internal_flags_valid(gfc)) {
        return 0;
			}
    cfg = &gfc->cfg;
    if(cfg->write_lame_tag == 0) {
        return 0;
    }
    if(gfc->VBR_seek_table.pos <= 0) {
        return 0;
    }
    if(size < gfc->VBR_seek_table.TotalFrameSize) {
        return gfc->VBR_seek_table.TotalFrameSize;
    }
    if(!buffer) {
        return 0;
    }

    memset(buffer, 0, gfc->VBR_seek_table.TotalFrameSize);

    /* 4 bytes frame header */
    setLameTagFrameHeader(gfc, buffer);

    /* Clear all TOC entries */
    memset(btToc, 0, sizeof(btToc));

    if(cfg->free_format) {
        int     i;
        for(i = 1; i < NUMTOCENTRIES; ++i)
            btToc[i] = 255 * i / 100;
    }
    else {
        Xing_seek_table(&gfc->VBR_seek_table, btToc);
    }
#ifdef DEBUG_VBR_SEEKING_TABLE
    print_seeking(btToc);
#endif

    /* Start writing the tag after the zero frame */
    nStreamIndex = cfg->sideinfo_len;
    /* note! Xing header specifies that Xing data goes in the
     * ancillary data with NO ERROR PROTECTION.  If error protecton
     * in enabled, the Xing data still starts at the same offset,
     * and now it is in sideinfo data block, and thus will not
     * decode correctly by non-Xing tag aware players */
    if(cfg->error_protection)
        nStreamIndex -= 2;

    /* Put Vbr tag */
    if(cfg->vbr == vbr_off) {
        buffer[nStreamIndex++] = VBRTag1[0];
        buffer[nStreamIndex++] = VBRTag1[1];
        buffer[nStreamIndex++] = VBRTag1[2];
        buffer[nStreamIndex++] = VBRTag1[3];

			}
    else {
        buffer[nStreamIndex++] = VBRTag0[0];
        buffer[nStreamIndex++] = VBRTag0[1];
        buffer[nStreamIndex++] = VBRTag0[2];
        buffer[nStreamIndex++] = VBRTag0[3];
			}

    /* Put header flags */
    CreateI4(&buffer[nStreamIndex], FRAMES_FLAG + BYTES_FLAG + TOC_FLAG + VBR_SCALE_FLAG);
    nStreamIndex += 4;

    /* Put Total Number of frames */
    CreateI4(&buffer[nStreamIndex], gfc->VBR_seek_table.nVbrNumFrames);
    nStreamIndex += 4;

    /* Put total audio stream size, including Xing/LAME Header */
    stream_size = gfc->VBR_seek_table.nBytesWritten + gfc->VBR_seek_table.TotalFrameSize;
    CreateI4(&buffer[nStreamIndex], stream_size);
    nStreamIndex += 4;

    /* Put TOC */
    memcpy(&buffer[nStreamIndex], btToc, sizeof(btToc));
    nStreamIndex += sizeof(btToc);


    if(cfg->error_protection) {
        /* (jo) error_protection: add crc16 information to header */
        CRC_writeheader(gfc, (char *) buffer);
    }
    {
        /*work out CRC so far: initially crc = 0 */
        uint16_t crc = 0x00;
        unsigned int i;
        for(i = 0; i < nStreamIndex; i++)
            crc = CRC_update_lookup(buffer[i], crc);
        /*Put LAME VBR info */
        nStreamIndex += PutLameVBR(gfp, stream_size, buffer + nStreamIndex, crc);
    }

#ifdef DEBUG_VBRTAG
    {
        VBRTAGDATA TestHeader;
        GetVbrTag(&TestHeader, buffer);
    }
#endif

  return gfc->VBR_seek_table.TotalFrameSize;
	}

/***********************************************************************
 *
 * PutVbrTag: Write final VBR tag to the file
 * Paramters:
 *                              lpszFileName: filename of MP3 bit stream
 *                              nVbrScale       : encoder quality indicator (0..100)
 ****************************************************************************
 */
int PutVbrTag(lame_global_flags const *gfp, FILE * fpStream) {
    lame_internal_flags *gfc = gfp->internal_flags;

    long    lFileSize;
    long    id3v2TagSize;
    size_t  nbytes;
    uint8_t buffer[MAXFRAMESIZE];

    if(gfc->VBR_seek_table.pos <= 0)
        return -1;

    /* Seek to end of file */
    fseek(fpStream, 0, SEEK_END);

    /* Get file size */
    lFileSize = ftell(fpStream);

    /* Abort if file has zero length. Yes, it can happen :) */
    if(lFileSize == 0)
        return -1;

    /*
     * The VBR tag may NOT be located at the beginning of the stream.
     * If an ID3 version 2 tag was added, then it must be skipped to write
     * the VBR tag data.
     */

    id3v2TagSize = skipId3v2(fpStream);

    if(id3v2TagSize < 0) {
        return id3v2TagSize;
    }

    /*Seek to the beginning of the stream */
    fseek(fpStream, id3v2TagSize, SEEK_SET);

    nbytes = lame_get_lametag_frame(gfp, buffer, sizeof(buffer));
    if(nbytes > sizeof(buffer)) {
        return -1;
    }

    if(nbytes < 1) {
        return 0;
    }

    /* Put it all to disk again */
    if(fwrite(buffer, nbytes, 1, fpStream) != 1) {
        return -1;
    }

  return 0;           /* success */
	}



/*
 *	lame utility library source file
 *
 *	Copyright (c) 1999 Albert L Faber
 *	Copyright (c) 2000-2005 Alexander Leidinger
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

/* $Id: util.c,v 1.159 2017/09/06 15:07:30 robert Exp $ */


#define PRECOMPUTE


/***********************************************************************
*
*  Global Function Definitions
*
***********************************************************************/
/*empty and close mallocs in gfc */

void free_id3tag(lame_internal_flags * const gfc) {

  gfc->tag_spec.language[0] = 0;
  if(gfc->tag_spec.title != 0) {
      free(gfc->tag_spec.title);
      gfc->tag_spec.title = 0;
    }
  if(gfc->tag_spec.artist != 0) {
      free(gfc->tag_spec.artist);
      gfc->tag_spec.artist = 0;
    }
  if(gfc->tag_spec.album != 0) {
      free(gfc->tag_spec.album);
      gfc->tag_spec.album = 0;
    }
  if(gfc->tag_spec.comment != 0) {
      free(gfc->tag_spec.comment);
      gfc->tag_spec.comment = 0;
    }

  if(gfc->tag_spec.albumart != 0) {
      free(gfc->tag_spec.albumart);
      gfc->tag_spec.albumart = 0;
      gfc->tag_spec.albumart_size = 0;
      gfc->tag_spec.albumart_mimetype = MIMETYPE_NONE;
    }
    if(gfc->tag_spec.v2_head != 0) {
        FrameDataNode *node = gfc->tag_spec.v2_head;
        do {
            void   *p = node->dsc.ptr.b;
            void   *q = node->txt.ptr.b;
            void   *r = node;
            node = node->nxt;
            free(p);
            free(q);
            free(r);
      } while(node);
    gfc->tag_spec.v2_head = 0;
    gfc->tag_spec.v2_tail = 0;
    }
	}


static void free_global_data(lame_internal_flags * gfc) {

    if(gfc && gfc->cd_psy) {
        if(gfc->cd_psy->l.s3) {
            /* XXX allocated in psymodel_init() */
            free(gfc->cd_psy->l.s3);
        }
        if(gfc->cd_psy->s.s3) {
            /* XXX allocated in psymodel_init() */
            free(gfc->cd_psy->s.s3);
        }
        free(gfc->cd_psy);
        gfc->cd_psy = 0;
    }
	}


void freegfc(lame_internal_flags * const gfc) {                       /* bit stream structure */
    int     i;

    if(!gfc) 
			return;

    for(i=0; i <= 2 * BPC; i++)
        if(gfc->sv_enc.blackfilt[i] != NULL) {
            free(gfc->sv_enc.blackfilt[i]);
            gfc->sv_enc.blackfilt[i] = NULL;
        }
    if(gfc->sv_enc.inbuf_old[0]) {
        free(gfc->sv_enc.inbuf_old[0]);
        gfc->sv_enc.inbuf_old[0] = NULL;
    }
    if(gfc->sv_enc.inbuf_old[1]) {
        free(gfc->sv_enc.inbuf_old[1]);
        gfc->sv_enc.inbuf_old[1] = NULL;
    }

    if(gfc->bs.buf != NULL) {
        free(gfc->bs.buf);
        gfc->bs.buf = NULL;
    }

    if(gfc->VBR_seek_table.bag) {
        free(gfc->VBR_seek_table.bag);
        gfc->VBR_seek_table.bag = NULL;
        gfc->VBR_seek_table.size = 0;
    }
    if(gfc->ATH) {
        free(gfc->ATH);
			}
    if(gfc->sv_rpg.rgdata) {
        free(gfc->sv_rpg.rgdata);
			}
    if(gfc->sv_enc.in_buffer_0) {
        free(gfc->sv_enc.in_buffer_0);
			}
    if(gfc->sv_enc.in_buffer_1) {
        free(gfc->sv_enc.in_buffer_1);
			}
    free_id3tag(gfc);

#ifdef DECODE_ON_THE_FLY
    if(gfc->hip) {
        hip_decode_exit(gfc->hip);
        gfc->hip = 0;
    }
#endif

  free_global_data(gfc);

  free(gfc);
	}

void calloc_aligned(aligned_pointer_t * ptr, unsigned int size, unsigned int bytes) {

    if(ptr) {
        if(!ptr->pointer) {
            ptr->pointer = malloc(size + bytes);
            if(ptr->pointer != 0) {
                memset(ptr->pointer, 0, size + bytes);
                if(bytes > 0) {
                    ptr->aligned = (void *) ((((size_t) ptr->pointer + bytes - 1) / bytes) * bytes);
                }
                else {
                    ptr->aligned = ptr->pointer;
                }
            }
            else {
                ptr->aligned = 0;
            }
        }
    }
	}

void free_aligned(aligned_pointer_t * ptr) {

    if(ptr) {
        if(ptr->pointer) {
            free(ptr->pointer);
            ptr->pointer = 0;
            ptr->aligned = 0;
        }
    }
	}

/*those ATH formulas are returning
their minimum value for input = -1*/

static FLOAT ATHformula_GB(FLOAT f, FLOAT value, FLOAT f_min, FLOAT f_max) {
    /* from Painter & Spanias
       modified by Gabriel Bouvigne to better fit the reality
       ath =    3.640 * pow(f,-0.8)
       - 6.800 * exp(-0.6*pow(f-3.4,2.0))
       + 6.000 * exp(-0.15*pow(f-8.7,2.0))
       + 0.6* 0.001 * pow(f,4.0);


       In the past LAME was using the Painter &Spanias formula.
       But we had some recurrent problems with HF content.
       We measured real ATH values, and found the older formula
       to be inacurate in the higher part. So we made this new
       formula and this solved most of HF problematic testcases.
       The tradeoff is that in VBR mode it increases a lot the
       bitrate. */


/*this curve can be udjusted according to the VBR scale:
it adjusts from something close to Painter & Spanias
on V9 up to Bouvigne's formula for V0. This way the VBR
bitrate is more balanced according to the -V value.*/

    FLOAT   ath;

    /* the following Hack allows to ask for the lowest value */
    if(f < -.3)
        f = 3410;

    f /= 1000;          /* convert to khz */
    f = Max(f_min, f);
    f = Min(f_max, f);

    ath = 3.640 * pow(f, -0.8)
        - 6.800 * exp(-0.6 * pow(f - 3.4, 2.0))
        + 6.000 * exp(-0.15 * pow(f - 8.7, 2.0))
        + (0.6 + 0.04 * value) * 0.001 * pow(f, 4.0);

  return ath;
	}



FLOAT ATHformula(SessionConfig_t const *cfg, FLOAT f) {
    FLOAT   ath;

    switch(cfg->ATHtype) {
    case 0:
        ath = ATHformula_GB(f, 9, 0.1f, 24.0f);
        break;
    case 1:
        ath = ATHformula_GB(f, -1, 0.1f, 24.0f); /*over sensitive, should probably be removed */
        break;
    case 2:
        ath = ATHformula_GB(f, 0, 0.1f, 24.0f);
        break;
    case 3:
        ath = ATHformula_GB(f, 1, 0.1f, 24.0f) + 6; /*modification of GB formula by Roel */
        break;
    case 4:
        ath = ATHformula_GB(f, cfg->ATHcurve, 0.1f, 24.0f);
        break;
    case 5:
        ath = ATHformula_GB(f, cfg->ATHcurve, 3.41f, 16.1f);
        break;
    default:
        ath = ATHformula_GB(f, 0, 0.1f, 24.0f);
        break;
    }

  return ath;
	}

/* see for example "Zwicker: Psychoakustik, 1982; ISBN 3-540-11401-7 */
FLOAT freq2bark(FLOAT freq) {

  /* input: freq in hz  output: barks */
  if(freq < 0)
      freq = 0;
  freq = freq * 0.001;
  return 13.0 * atan(.76 * freq) + 3.5 * atan(freq * freq / (7.5 * 7.5));
	}

#if 0
extern FLOAT freq2cbw(FLOAT freq);

/* see for example "Zwicker: Psychoakustik, 1982; ISBN 3-540-11401-7 */
FLOAT
freq2cbw(FLOAT freq) {

    /* input: freq in hz  output: critical band width */
    freq = freq * 0.001;
    return 25 + 75 * pow(1 + 1.4 * (freq * freq), 0.69);
	}

#endif




#define ABS(A) (((A)>0) ? (A) : -(A))

int FindNearestBitrate(int bRate, /* legal rates from 8 to 320 */
                   int version, int samplerate) {                       /* MPEG-1 or MPEG-2 LSF */
    int     bitrate;
    int     i;

    if(samplerate < 16000)
        version = 2;

    bitrate = bitrate_table[version][1];

    for(i = 2; i <= 14; i++) {
        if(bitrate_table[version][i] > 0) {
            if(ABS(bitrate_table[version][i] - bRate) < ABS(bitrate - bRate))
                bitrate = bitrate_table[version][i];
        }
    }

  return bitrate;
	}





#ifndef Min
#define         Min(A, B)       ((A) < (B) ? (A) : (B))
#endif
#ifndef Max
#define         Max(A, B)       ((A) > (B) ? (A) : (B))
#endif


/* Used to find table index when
 * we need bitrate-based values
 * determined using tables
 *
 * bitrate in kbps
 *
 * Gabriel Bouvigne 2002-11-03
 */
int nearestBitrateFullIndex(uint16_t bitrate) {
    /* borrowed from DM abr presets */

    const int full_bitrate_table[] =
        { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };


    int     lower_range = 0, lower_range_kbps = 0, upper_range = 0, upper_range_kbps = 0;


    int     b;


    /* We assume specified bitrate will be 320kbps */
    upper_range_kbps = full_bitrate_table[16];
    upper_range = 16;
    lower_range_kbps = full_bitrate_table[16];
    lower_range = 16;

    /* Determine which significant bitrates the value specified falls between,
     * if loop ends without breaking then we were correct above that the value was 320
     */
    for(b = 0; b < 16; b++) {
        if((Max(bitrate, full_bitrate_table[b + 1])) != bitrate) {
            upper_range_kbps = full_bitrate_table[b + 1];
            upper_range = b + 1;
            lower_range_kbps = full_bitrate_table[b];
            lower_range = (b);
            break;      /* We found upper range */
        }
    }

    /* Determine which range the value specified is closer to */
    if((upper_range_kbps - bitrate) > (bitrate - lower_range_kbps)) {
        return lower_range;
    }

  return upper_range;
	}





/* map frequency to a valid MP3 sample frequency
 *
 * Robert Hegemann 2000-07-01
 */
int map2MP3Frequency(int freq) {

    if(freq <= 8000)
        return 8000;
    if(freq <= 11025)
        return 11025;
    if(freq <= 12000)
        return 12000;
    if(freq <= 16000)
        return 16000;
    if(freq <= 22050)
        return 22050;
    if(freq <= 24000)
        return 24000;
    if(freq <= 32000)
        return 32000;
    if(freq <= 44100)
        return 44100;

    return 48000;
	}

int BitrateIndex(int bRate,      /* legal rates from 32 to 448 kbps */
             int version,    /* MPEG-1 or MPEG-2/2.5 LSF */
             int samplerate) {                       /* convert bitrate in kbps to index */
    int     i;

    if(samplerate < 16000)
        version = 2;
    for(i = 0; i <= 14; i++) {
        if(bitrate_table[version][i] > 0) {
            if(bitrate_table[version][i] == bRate) {
                return i;
            }
        }
    }
  return -1;
	}

/* convert samp freq in Hz to index */
int SmpFrqIndex(int sample_freq, BYTE *const version) {

  switch(sample_freq) {
    case 44100:
        *version = 1;
        return 0;
    case 48000:
        *version = 1;
        return 1;
    case 32000:
        *version = 1;
        return 2;
    case 22050:
        *version = 0;
        return 0;
    case 24000:
        *version = 0;
        return 1;
    case 16000:
        *version = 0;
        return 2;
    case 11025:
        *version = 0;
        return 0;
    case 12000:
        *version = 0;
        return 1;
    case 8000:
        *version = 0;
        return 2;
    default:
        *version = 0;
        return -1;
    }
	}


/*****************************************************************************
*
*  End of bit_stream.c package
*
*****************************************************************************/










/* resampling via FIR filter, blackman window */
inline static FLOAT blackman(FLOAT x, FLOAT fcn, int l) {
    /* This algorithm from:
       SIGNAL PROCESSING ALGORITHMS IN FORTRAN AND C
       S.D. Stearns and R.A. David, Prentice-Hall, 1992
     */
    FLOAT   bkwn, x2;
    FLOAT const wcn = (PI * fcn);

    x /= l;
    if(x < 0)
        x = 0;
    if(x > 1)
        x = 1;
    x2 = x - .5;

    bkwn = 0.42 - 0.5 * cos(2 * x * PI) + 0.08 * cos(4 * x * PI);
    if(fabs(x2) < 1e-9)
        return wcn / PI;
    else
        return (bkwn * sin(l * wcn * x2) / (PI * l * x2));


	}




/* gcd - greatest common divisor */
/* Joint work of Euclid and M. Hendry */

static int gcd(int i, int j) {
    /*    assert ( i > 0  &&  j > 0 ); */
    return j ? gcd(j, i % j) : i;
	}



static int fill_buffer_resample(lame_internal_flags * gfc,
                     sample_t * outbuf,
                     int desired_len, sample_t const *inbuf, int len, int *num_used, int ch) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *esv = &gfc->sv_enc;
    double  resample_ratio = (double)cfg->samplerate_in / (double)cfg->samplerate_out;
    int     BLACKSIZE;
    FLOAT   offset, xvalue;
    int     i, j = 0, k;
    int     filter_l;
    FLOAT   fcn, intratio;
    FLOAT  *inbuf_old;
    int     bpc;             /* number of convolution functions to pre-compute */

    bpc = cfg->samplerate_out / gcd(cfg->samplerate_out, cfg->samplerate_in);
    if(bpc > BPC)
        bpc = BPC;

    intratio = (fabs(resample_ratio - floor(.5 + resample_ratio)) < FLT_EPSILON);
    fcn = 1.00 / resample_ratio;
    if(fcn > 1.00)
        fcn = 1.00;
    filter_l = 31;     /* must be odd */
    filter_l += intratio; /* unless resample_ratio=int, it must be even */


    BLACKSIZE = filter_l + 1; /* size of data needed for FIR */

    if(gfc->fill_buffer_resample_init == 0) {
        esv->inbuf_old[0] = lame_calloc(sample_t, BLACKSIZE);
        esv->inbuf_old[1] = lame_calloc(sample_t, BLACKSIZE);
        for(i = 0; i <= 2 * bpc; ++i)
            esv->blackfilt[i] = lame_calloc(sample_t, BLACKSIZE);

        esv->itime[0] = 0;
        esv->itime[1] = 0;

        /* precompute blackman filter coefficients */
        for(j = 0; j <= 2 * bpc; j++) {
            FLOAT   sum = 0.;
            offset = (j - bpc) / (2. * bpc);
            for(i = 0; i <= filter_l; i++)
                sum += esv->blackfilt[j][i] = blackman(i - offset, fcn, filter_l);
            for(i = 0; i <= filter_l; i++)
                esv->blackfilt[j][i] /= sum;
        }
        gfc->fill_buffer_resample_init = 1;
    }

    inbuf_old = esv->inbuf_old[ch];

    /* time of j'th element in inbuf = itime + j/ifreq; */
    /* time of k'th element in outbuf   =  j/ofreq */
    for(k = 0; k < desired_len; k++) {
        double  time0 = k * resample_ratio; /* time of k'th output sample */
        int     joff;

        j = floor(time0 - esv->itime[ch]);

        /* check if we need more input data */
        if((filter_l + j - filter_l / 2) >= len)
            break;

        /* blackman filter.  by default, window centered at j+.5(filter_l%2) */
        /* but we want a window centered at time0.   */
        offset = (time0 - esv->itime[ch] - (j + .5 * (filter_l % 2)));
        assert(fabs(offset) <= .501);

        /* find the closest precomputed window for this offset: */
        joff = floor((offset * 2 * bpc) + bpc + .5);

        xvalue = 0.;
        for(i = 0; i <= filter_l; ++i) {
            int const j2 = i + j - filter_l / 2;
            sample_t y;
            assert(j2 < len);
            assert(j2 + BLACKSIZE >= 0);
            y = (j2 < 0) ? inbuf_old[BLACKSIZE + j2] : inbuf[j2];
#ifdef PRECOMPUTE
            xvalue += y * esv->blackfilt[joff][i];
#else
            xvalue += y * blackman(i - offset, fcn, filter_l); /* very slow! */
#endif
        }
        outbuf[k] = xvalue;
    }


    /* k = number of samples added to outbuf */
    /* last k sample used data from [j-filter_l/2,j+filter_l-filter_l/2]  */

    /* how many samples of input data were used:  */
    *num_used = Min(len, filter_l + j - filter_l / 2);

    /* adjust our input time counter.  Incriment by the number of samples used,
     * then normalize so that next output sample is at time 0, next
     * input buffer is at time itime[ch] */
    esv->itime[ch] += *num_used - k * resample_ratio;

    /* save the last BLACKSIZE samples into the inbuf_old buffer */
    if(*num_used >= BLACKSIZE) {
        for(i = 0; i < BLACKSIZE; i++)
            inbuf_old[i] = inbuf[*num_used + i - BLACKSIZE];
    }
    else {
        /* shift in *num_used samples into inbuf_old  */
        int const n_shift = BLACKSIZE - *num_used; /* number of samples to shift */

        /* shift n_shift samples by *num_used, to make room for the
         * num_used new samples */
        for(i = 0; i < n_shift; ++i)
            inbuf_old[i] = inbuf_old[i + *num_used];

        /* shift in the *num_used samples */
        for(j = 0; i < BLACKSIZE; ++i, ++j)
            inbuf_old[i] = inbuf[j];

        assert(j == *num_used);
    }

  return k;           /* return the number samples created at the new samplerate */
	}

int isResamplingNecessary(SessionConfig_t const* cfg) {
    int const l = cfg->samplerate_out * 0.9995f;
    int const h = cfg->samplerate_out * 1.0005f;

  return (cfg->samplerate_in < l) || (h < cfg->samplerate_in) ? 1 : 0;
	}

/* copy in new samples from in_buffer into mfbuf, with resampling
   if necessary.  n_in = number of samples from the input buffer that
   were used.  n_out = number of samples copied into mfbuf  */

void fill_buffer(lame_internal_flags * gfc,
            sample_t * const mfbuf[2], sample_t const * const in_buffer[2], int nsamples, int *n_in, int *n_out) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int     mf_size = gfc->sv_enc.mf_size;
    int     framesize = 576 * cfg->mode_gr;
    int     nout, ch = 0;
    int     nch = cfg->channels_out;

    /* copy in new samples into mfbuf, with resampling if necessary */
    if(isResamplingNecessary(cfg)) {
        do {
            nout =
                fill_buffer_resample(gfc, &mfbuf[ch][mf_size],
                                     framesize, in_buffer[ch], nsamples, n_in, ch);
        } while(++ch < nch);
        *n_out = nout;
	    }
    else {
        nout = Min(framesize, nsamples);
        do {
            memcpy(&mfbuf[ch][mf_size], &in_buffer[ch][0], nout * sizeof(mfbuf[0][0]));
        } while(++ch < nch);
    *n_out = nout;
    *n_in = nout;
    }
	}







/***********************************************************************
*
*  Message Output
*
***********************************************************************/
void lame_report_def(const char *format, va_list args) {

    vfprintf(stderr, format, args);
    fflush(stderr); /* an debug function should flush immediately */
}

void lame_report_fnc(lame_report_function print_f, const char *format, ...) {
    
	if(print_f) {
        va_list args;
        va_start(args, format);
        print_f(format, args);
        va_end(args);
    }
	}


void lame_debugf(const lame_internal_flags* gfc, const char *format, ...) {

    if(gfc && gfc->report_dbg) {
        va_list args;
        va_start(args, format);
        gfc->report_dbg(format, args);
        va_end(args);
    }
	}


void lame_msgf(const lame_internal_flags* gfc, const char *format, ...) {

    if(gfc && gfc->report_msg) {
        va_list args;
        va_start(args, format);
        gfc->report_msg(format, args);
        va_end(args);
    }
	}


void lame_errorf(const lame_internal_flags* gfc, const char *format, ...) {

    if(gfc && gfc->report_err) {
        va_list args;
        va_start(args, format);
        gfc->report_err(format, args);
        va_end(args);
    }
	}



/***********************************************************************
 *
 *      routines to detect CPU specific features like 3DNow, MMX, SSE
 *
 *  donated by Frank Klemm
 *  added Robert Hegemann 2000-10-10
 *
 ***********************************************************************/

#ifdef HAVE_NASM
extern int has_MMX_nasm(void);
extern int has_3DNow_nasm(void);
extern int has_SSE_nasm(void);
extern int has_SSE2_nasm(void);
#endif

int has_MMX(void) {
#ifdef HAVE_NASM
    return has_MMX_nasm();
#else
    return 0;           /* don't know, assume not */
#endif
}

int has_3DNow(void) {
#ifdef HAVE_NASM
    return has_3DNow_nasm();
#else
    return 0;           /* don't know, assume not */
#endif
}

int has_SSE(void) {
#ifdef HAVE_NASM
    return has_SSE_nasm();
#else
#if defined( _M_X64 ) || defined( MIN_ARCH_SSE )
    return 1;
#else
    return 0;           /* don't know, assume not */
#endif
#endif
}

int has_SSE2(void) {
#ifdef HAVE_NASM
    return has_SSE2_nasm();
#else
#if defined( _M_X64 ) || defined( MIN_ARCH_SSE )
    return 1;
#else
    return 0;           /* don't know, assume not */
#endif
#endif
}

void disable_FPE(void) {

/* extremely system dependent stuff, move to a lib to make the code readable */
/*==========================================================================*/


    /*
     *  Disable floating point exceptions
     */


#if defined(__FreeBSD__) && !defined(__alpha__)
    {
        /* seet floating point mask to the Linux default */
        fp_except_t mask;
        mask = fpgetmask();
        /* if bit is set, we get SIGFPE on that error! */
        fpsetmask(mask & ~(FP_X_INV | FP_X_DZ));
        /*  DEBUGF("FreeBSD mask is 0x%x\n",mask); */
    }
#endif

#if defined(__riscos__) && !defined(ABORTFP)
    /* Disable FPE's under RISC OS */
    /* if bit is set, we disable trapping that error! */
    /*   _FPE_IVO : invalid operation */
    /*   _FPE_DVZ : divide by zero */
    /*   _FPE_OFL : overflow */
    /*   _FPE_UFL : underflow */
    /*   _FPE_INX : inexact */
    DisableFPETraps(_FPE_IVO | _FPE_DVZ | _FPE_OFL);
#endif

    /*
     *  Debugging stuff
     *  The default is to ignore FPE's, unless compiled with -DABORTFP
     *  so add code below to ENABLE FPE's.
     */

#if defined(ABORTFP)
#if defined(_MSC_VER)
    {
#if 0
        /* rh 061207
           the following fix seems to be a workaround for a problem in the
           parent process calling LAME. It would be better to fix the broken
           application => code disabled.
         */

        /* set affinity to a single CPU.  Fix for EAC/lame on SMP systems from
           "Todd Richmond" <todd.richmond@openwave.com> */
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        SetProcessAffinityMask(GetCurrentProcess(), si.dwActiveProcessorMask);
#endif
#include <float.h>
        unsigned int mask;
        mask = _controlfp(0, 0);
        mask &= ~(_EM_OVERFLOW | _EM_UNDERFLOW | _EM_ZERODIVIDE | _EM_INVALID);
        mask = _controlfp(mask, _MCW_EM);
    }
#elif defined(__CYGWIN__)
#  define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
#  define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))

#  define _EM_INEXACT     0x00000020 /* inexact (precision) */
#  define _EM_UNDERFLOW   0x00000010 /* underflow */
#  define _EM_OVERFLOW    0x00000008 /* overflow */
#  define _EM_ZERODIVIDE  0x00000004 /* zero divide */
#  define _EM_INVALID     0x00000001 /* invalid */
    {
        unsigned int mask;
        _FPU_GETCW(mask);
        /* Set the FPU control word to abort on most FPEs */
        mask &= ~(_EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID);
        _FPU_SETCW(mask);
    }
# elif defined(__linux__)
    {

#  include <fpu_control.h>
#  ifndef _FPU_GETCW
#  define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
#  endif
#  ifndef _FPU_SETCW
#  define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))
#  endif

        /* 
         * Set the Linux mask to abort on most FPE's
         * if bit is set, we _mask_ SIGFPE on that error!
         *  mask &= ~( _FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM | _FPU_MASK_UM );
         */

        unsigned int mask;
        _FPU_GETCW(mask);
        mask &= ~(_FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM);
        _FPU_SETCW(mask);
    }
#endif
#endif /* ABORTFP */
	}





#ifdef USE_FAST_LOG
/***********************************************************************
 *
 * Fast Log Approximation for log2, used to approximate every other log
 * (log10 and log)
 * maximum absolute error for log10 is around 10-6
 * maximum *relative* error can be high when x is almost 1 because error/log10(x) tends toward x/e
 *
 * use it if typical RESULT values are > 1e-5 (for example if x>1.00001 or x<0.99999)
 * or if the relative precision in the domain around 1 is not important (result in 1 is exact and 0)
 *
 ***********************************************************************/


#define LOG2_SIZE       (512)
#define LOG2_SIZE_L2    (9)

static ieee754_float32_t log_table[LOG2_SIZE + 1];



void init_log_table(void) {
    int     j;
    static int init = 0;

    /* Range for log2(x) over [1,2[ is [0,1[ */
    assert((1 << LOG2_SIZE_L2) == LOG2_SIZE);

    if(!init) {
        for(j = 0; j < LOG2_SIZE + 1; j++)
            log_table[j] = log(1.0f + j / (ieee754_float32_t) LOG2_SIZE) / log(2.0f);
    }
  init = 1;
	}



ieee754_float32_t fast_log2(ieee754_float32_t x) {
    ieee754_float32_t log2val, partial;
    union {
        ieee754_float32_t f;
        int     i;
    } fi;
    int     mantisse;

    fi.f = x;
    mantisse = fi.i & 0x7fffff;
    log2val = ((fi.i >> 23) & 0xFF) - 0x7f;
    partial = (mantisse & ((1 << (23 - LOG2_SIZE_L2)) - 1));
    partial *= 1.0f / ((1 << (23 - LOG2_SIZE_L2)));


    mantisse >>= (23 - LOG2_SIZE_L2);

    /* log2val += log_table[mantisse];  without interpolation the results are not good */
    log2val += log_table[mantisse] * (1.0f - partial) + log_table[mantisse + 1] * partial;

    return log2val;
	}

#else /* Don't use FAST_LOG */


void init_log_table(void) {
	}

#endif

/* end of util.c */



/* -*- mode: C; mode: fold -*- */
/*
 * set/get functions for lame_global_flags
 *
 * Copyright (c) 2001-2005 Alexander Leidinger
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

/* $Id: set_get.c,v 1.104 2017/09/06 15:07:30 robert Exp $ */


/*
 * input stream description
 */


/* number of samples */
/* it's unlikely for this function to return an error */
int lame_set_num_samples(lame_global_flags * gfp, unsigned long num_samples) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 2^32-1 */
        gfp->num_samples = num_samples;
        return 0;
    }
  return -1;
	}

unsigned long lame_get_num_samples(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->num_samples;
    }
  return 0;
	}


/* input samplerate */
int lame_set_in_samplerate(lame_global_flags * gfp, int in_samplerate) {

    if(is_lame_global_flags_valid(gfp)) {
        if(in_samplerate < 1)
            return -1;
        /* input sample rate in Hz,  default = 44100 Hz */
        gfp->samplerate_in = in_samplerate;
        return 0;
    }
  return -1;
	}

int lame_get_in_samplerate(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->samplerate_in;
    }
  return 0;
	}


/* number of channels in input stream */
int lame_set_num_channels(lame_global_flags * gfp, int num_channels) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 2 */
        if(2 < num_channels || 0 >= num_channels) {
            return -1;  /* we don't support more than 2 channels */
        }
        gfp->num_channels = num_channels;
        return 0;
    }

  return -1;
	}

int lame_get_num_channels(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->num_channels;
    }

  return 0;
	}


/* scale the input by this amount before encoding (not used for decoding) */
int lame_set_scale(lame_global_flags * gfp, float scale) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 1 */
        gfp->scale = scale;
        return 0;
    }

  return -1;
	}

float lame_get_scale(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->scale;
    }

  return 0;
	}


/* scale the channel 0 (left) input by this amount before 
   encoding (not used for decoding) */
int lame_set_scale_left(lame_global_flags * gfp, float scale) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 1 */
        gfp->scale_left = scale;
        return 0;
    }

  return -1;
	}

float lame_get_scale_left(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->scale_left;
    }

  return 0;
	}


/* scale the channel 1 (right) input by this amount before 
   encoding (not used for decoding) */
int lame_set_scale_right(lame_global_flags * gfp, float scale) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 1 */
        gfp->scale_right = scale;
        return 0;
    }

  return -1;
	}

float lame_get_scale_right(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->scale_right;
    }

  return 0;
	}


/* output sample rate in Hz */
int lame_set_out_samplerate(lame_global_flags *gfp, int out_samplerate) {

    if(is_lame_global_flags_valid(gfp)) {
        /*
         * default = 0: LAME picks best value based on the amount
         *              of compression
         * MPEG only allows:
         *  MPEG1    32, 44.1,   48khz
         *  MPEG2    16, 22.05,  24
         *  MPEG2.5   8, 11.025, 12
         *
         * (not used by decoding routines)
         */
        if(out_samplerate != 0) {
          BYTE v=0;
          if(SmpFrqIndex(out_samplerate, &v) < 0)
              return -1;
					}
        gfp->samplerate_out = out_samplerate;
        return 0;
    }
  return -1;
	}

int lame_get_out_samplerate(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->samplerate_out;
    }
  return 0;
	}




/*
 * general control parameters
 */

/* collect data for an MP3 frame analzyer */
int lame_set_analysis(lame_global_flags * gfp, int analysis) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > analysis || 1 < analysis)
            return -1;
        gfp->analysis = analysis;
        return 0;
    }
  return -1;
	}

int lame_get_analysis(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->analysis && 1 >= gfp->analysis);
        return gfp->analysis;
    }
  return 0;
	}


/* write a Xing VBR header frame */
int lame_set_bWriteVbrTag(lame_global_flags * gfp, int bWriteVbrTag) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 1 (on) for VBR/ABR modes, 0 (off) for CBR mode */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > bWriteVbrTag || 1 < bWriteVbrTag)
            return -1;
        gfp->write_lame_tag = bWriteVbrTag;
        return 0;
    }
  return -1;
	}

int lame_get_bWriteVbrTag(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->write_lame_tag && 1 >= gfp->write_lame_tag);
        return gfp->write_lame_tag;
    }
  return 0;
	}



/* decode only, use lame/mpglib to convert mp3 to wav */
int lame_set_decode_only(lame_global_flags * gfp, int decode_only) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > decode_only || 1 < decode_only)
            return -1;
        gfp->decode_only = decode_only;
        return 0;
    }
  return -1;
	}

int lame_get_decode_only(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->decode_only && 1 >= gfp->decode_only);
        return gfp->decode_only;
    }
  return 0;
	}


#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
/* 1=encode a Vorbis .ogg file.  default=0 */
/* DEPRECATED */
int CDECL lame_set_ogg(lame_global_flags *, int);
int CDECL lame_get_ogg(const lame_global_flags *);
#else
#endif

/* encode a Vorbis .ogg file */
/* DEPRECATED */
int lame_set_ogg(lame_global_flags * gfp, int ogg) {

  gfp;
  ogg;
    return -1;
	}

int lame_get_ogg(const lame_global_flags * gfp) {

  gfp;
  return 0;
	}


/*
 * Internal algorithm selection.
 * True quality is determined by the bitrate but this variable will effect
 * quality by selecting expensive or cheap algorithms.
 * quality=0..9.  0=best (very slow).  9=worst.  
 * recommended:  3     near-best quality, not too slow
 *               5     good quality, fast
 *               7     ok quality, really fast
 */
int lame_set_quality(lame_global_flags * gfp, int quality) {

    if(is_lame_global_flags_valid(gfp)) {
        if(quality < 0) {
            gfp->quality = 0;
        }
        else if(quality > 9) {
            gfp->quality = 9;
        }
        else {
            gfp->quality = quality;
        }
        return 0;
    }
  return -1;
	}

int lame_get_quality(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->quality;
    }
  return 0;
	}


/* mode = STEREO, JOINT_STEREO, DUAL_CHANNEL (not supported), MONO */
int lame_set_mode(lame_global_flags * gfp, MPEG_mode mode) {

    if(is_lame_global_flags_valid(gfp)) {
        int     mpg_mode = mode;
        /* default: lame chooses based on compression ratio and input channels */
        if(mpg_mode < 0 || MAX_INDICATOR <= mpg_mode)
            return -1;  /* Unknown MPEG mode! */
        gfp->mode = mode;
        return 0;
    }
  return -1;
	}

MPEG_mode lame_get_mode(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(gfp->mode < MAX_INDICATOR);
        return gfp->mode;
    }
  return NOT_SET;
	}


#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
/*
  mode_automs.  Use a M/S mode with a switching threshold based on
  compression ratio
  DEPRECATED
*/
int CDECL lame_set_mode_automs(lame_global_flags *, int);
int CDECL lame_get_mode_automs(const lame_global_flags *);
#else
#endif

/* Us a M/S mode with a switching threshold based on compression ratio */
/* DEPRECATED */
int lame_set_mode_automs(lame_global_flags * gfp, int mode_automs) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > mode_automs || 1 < mode_automs)
            return -1;
        lame_set_mode(gfp, JOINT_STEREO);
        return 0;
    }
  return -1;
	}

int lame_get_mode_automs(const lame_global_flags * gfp) {

  gfp;
  return 1;
	}


/*
 * Force M/S for all frames.  For testing only.
 * Requires mode = 1.
 */
int lame_set_force_ms(lame_global_flags * gfp, int force_ms) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > force_ms || 1 < force_ms)
            return -1;
        gfp->force_ms = force_ms;
        return 0;
    }
  return -1;
	}

int lame_get_force_ms(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->force_ms && 1 >= gfp->force_ms);
        return gfp->force_ms;
    }
    return 0;
	}


/* Use free_format. */
int lame_set_free_format(lame_global_flags * gfp, int free_format) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > free_format || 1 < free_format)
            return -1;
        gfp->free_format = free_format;
        return 0;
    }
  return -1;
	}

int lame_get_free_format(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->free_format && 1 >= gfp->free_format);
        return gfp->free_format;
    }

  return 0;
	}



// Perform ReplayGain analysis
int lame_set_findReplayGain(lame_global_flags * gfp, BYTE findReplayGain) {

  if(is_lame_global_flags_valid(gfp)) {
      /* default = 0 (disabled) */

      /* enforce disable/enable meaning, if we need more than two values
         we need to switch to an enum to have an apropriate representation
         of the possible meanings of the value */
      if(0 > findReplayGain || 1 < findReplayGain)
          return -1;
      gfp->findReplayGain = findReplayGain;
      return 0;
    }

  return -1;
	}

BYTE lame_get_findReplayGain(const lame_global_flags *gfp) {

  if(is_lame_global_flags_valid(gfp)) {
    assert(0 <= gfp->findReplayGain && 1 >= gfp->findReplayGain);
    return gfp->findReplayGain;
    }

  return 0;
	}


/* Decode on the fly. Find the peak sample. If ReplayGain analysis is 
   enabled then perform it on the decoded data. */
int lame_set_decode_on_the_fly(lame_global_flags * gfp, int decode_on_the_fly) {

    if(is_lame_global_flags_valid(gfp)) {
#ifndef DECODE_ON_THE_FLY
        return -1;
#else
        /* default = 0 (disabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > decode_on_the_fly || 1 < decode_on_the_fly)
            return -1;

        gfp->decode_on_the_fly = decode_on_the_fly;

        return 0;
#endif
    }
  return -1;
	}

int lame_get_decode_on_the_fly(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->decode_on_the_fly && 1 >= gfp->decode_on_the_fly);
        return gfp->decode_on_the_fly;
    }
  return 0;
	}

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
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
#else
#endif

/* DEPRECATED. same as lame_set_decode_on_the_fly() */
int lame_set_findPeakSample(lame_global_flags *gfp, int arg) {
  return lame_set_decode_on_the_fly(gfp, arg);
	}

int lame_get_findPeakSample(const lame_global_flags *gfp) {
  return lame_get_decode_on_the_fly(gfp);
	}

/* DEPRECATED. same as lame_set_findReplayGain() */
int lame_set_ReplayGain_input(lame_global_flags *gfp, int arg) {
  return lame_set_findReplayGain(gfp, arg);
	}

int lame_get_ReplayGain_input(const lame_global_flags *gfp) {
  return lame_get_findReplayGain(gfp);
	}

/* DEPRECATED. same as lame_set_decode_on_the_fly() &&
   lame_set_findReplayGain() */
int lame_set_ReplayGain_decode(lame_global_flags *gfp, int arg) {

  if(lame_set_decode_on_the_fly(gfp, arg) < 0 || lame_set_findReplayGain(gfp, arg) < 0)
      return -1;
  else
      return 0;
	}

int lame_get_ReplayGain_decode(const lame_global_flags * gfp) {

  if(lame_get_decode_on_the_fly(gfp) > 0 && lame_get_findReplayGain(gfp) > 0)
      return 1;
  else
      return 0;
	}


/* set and get some gapless encoding flags */
int lame_set_nogap_total(lame_global_flags * gfp, int the_nogap_total) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->nogap_total = the_nogap_total;
        return 0;
    }
  return -1;
	}

int lame_get_nogap_total(const lame_global_flags * gfp) { 
	
	if(is_lame_global_flags_valid(gfp)) {
        return gfp->nogap_total;
    }
  return 0;
	}

int lame_set_nogap_currentindex(lame_global_flags * gfp, int the_nogap_index) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->nogap_current = the_nogap_index;
        return 0;
    }
  return -1;
	}

int lame_get_nogap_currentindex(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->nogap_current;
    }
    return 0;
}


/* message handlers */
int lame_set_errorf(lame_global_flags * gfp, void (*func) (const char *, va_list)) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->report.errorf = func;
        return 0;
    }
  return -1;
	}

int lame_set_debugf(lame_global_flags * gfp, void (*func) (const char *, va_list)) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->report.debugf = func;
        return 0;
    }
  return -1;
	}

int lame_set_msgf(lame_global_flags * gfp, void (*func) (const char *, va_list)) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->report.msgf = func;
        return 0;
    }
  return -1;
	}


/*
 * Set one of
 *  - brate
 *  - compression ratio.
 *
 * Default is compression ratio of 11.
 */
int lame_set_brate(lame_global_flags * gfp, int brate) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->brate = brate;
        if(brate > 320) {
            gfp->disable_reservoir = 1;
        }
        return 0;
    }
    return -1;
	}

int lame_get_brate(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->brate;
    }
  return 0;
}

int lame_set_compression_ratio(lame_global_flags * gfp, float compression_ratio) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->compression_ratio = compression_ratio;
        return 0;
    }
    return -1;
}

float lame_get_compression_ratio(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->compression_ratio;
    }
    return 0;
	}




/*
 * frame parameters
 */

/* Mark as copyright protected. */
int lame_set_copyright(lame_global_flags * gfp, int copyright) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > copyright || 1 < copyright)
            return -1;
        gfp->copyright = copyright;
        return 0;
    }
    return -1;
	}

int lame_get_copyright(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->copyright && 1 >= gfp->copyright);
        return gfp->copyright;
    }
    return 0;
}


/* Mark as original. */
int lame_set_original(lame_global_flags * gfp, int original) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 1 (enabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > original || 1 < original)
            return -1;
        gfp->original = original;
        return 0;
    }
  return -1;
	}

int lame_get_original(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->original && 1 >= gfp->original);
        return gfp->original;
    }
  return 0;
	}


/*
 * error_protection.
 * Use 2 bytes from each frame for CRC checksum.
 */
int lame_set_error_protection(lame_global_flags * gfp, int error_protection) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > error_protection || 1 < error_protection)
            return -1;
        gfp->error_protection = error_protection;
        return 0;
    }
  return -1;
	}

int lame_get_error_protection(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->error_protection && 1 >= gfp->error_protection);
        return gfp->error_protection;
    }
  return 0;
	}


#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
/* padding_type. 0=pad no frames  1=pad all frames 2=adjust padding(default) */
int CDECL lame_set_padding_type(lame_global_flags *, Padding_type);
Padding_type CDECL lame_get_padding_type(const lame_global_flags *);
#else
#endif

/*
 * padding_type.
 *  PAD_NO     = pad no frames
 *  PAD_ALL    = pad all frames
 *  PAD_ADJUST = adjust padding
 */
int lame_set_padding_type(lame_global_flags * gfp, Padding_type padding_type) {

    (void) gfp;
    (void) padding_type;
    return 0;
}

Padding_type
lame_get_padding_type(const lame_global_flags * gfp) {

    (void) gfp;
    return PAD_ADJUST;
}


/* MP3 'private extension' bit. Meaningless. */
int lame_set_extension(lame_global_flags * gfp, int extension) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */
        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > extension || 1 < extension)
            return -1;
        gfp->extension = extension;
        return 0;
    }
    return -1;
}

int lame_get_extension(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->extension && 1 >= gfp->extension);
        return gfp->extension;
    }
    return 0;
}


/* Enforce strict ISO compliance. */
int lame_set_strict_ISO(lame_global_flags * gfp, int val) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */
        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(val < MDB_DEFAULT || MDB_MAXIMUM < val)
            return -1;
        gfp->strict_ISO = val;
        return 0;
    }
    return -1;
}

int lame_get_strict_ISO(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->strict_ISO;
    }
    return 0;
	}




/********************************************************************
 * quantization/noise shaping 
 ***********************************************************************/

/* Disable the bit reservoir. For testing only. */
int lame_set_disable_reservoir(lame_global_flags * gfp, int disable_reservoir) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > disable_reservoir || 1 < disable_reservoir)
            return -1;
        gfp->disable_reservoir = disable_reservoir;
        return 0;
    }
  return -1;
	}

int lame_get_disable_reservoir(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->disable_reservoir && 1 >= gfp->disable_reservoir);
        return gfp->disable_reservoir;
    }
  return 0;
	}



int lame_set_experimentalX(lame_global_flags * gfp, int experimentalX) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_set_quant_comp(gfp, experimentalX);
        lame_set_quant_comp_short(gfp, experimentalX);
        return 0;
    }
    return -1;
}

int lame_get_experimentalX(const lame_global_flags * gfp) {

    return lame_get_quant_comp(gfp);
}


/* Select a different "best quantization" function. default = 0 */
int lame_set_quant_comp(lame_global_flags * gfp, int quant_type) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->quant_comp = quant_type;
        return 0;
    }
    return -1;
}

int lame_get_quant_comp(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->quant_comp;
    }
    return 0;
}


/* Select a different "best quantization" function. default = 0 */
int lame_set_quant_comp_short(lame_global_flags * gfp, int quant_type) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->quant_comp_short = quant_type;
        return 0;
    }
    return -1;
}

int lame_get_quant_comp_short(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->quant_comp_short;
    }
    return 0;
}


/* Another experimental option. For testing only. */
int lame_set_experimentalY(lame_global_flags * gfp, int experimentalY) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->experimentalY = experimentalY;
        return 0;
    }
    return -1;
}

int lame_get_experimentalY(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->experimentalY;
    }
    return 0;
}


int lame_set_experimentalZ(lame_global_flags * gfp, int experimentalZ) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->experimentalZ = experimentalZ;
        return 0;
    }
    return -1;
}

int lame_get_experimentalZ(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->experimentalZ;
    }
    return 0;
}


/* Naoki's psycho acoustic model. */
int lame_set_exp_nspsytune(lame_global_flags * gfp, int exp_nspsytune) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */
        gfp->exp_nspsytune = exp_nspsytune;
        return 0;
    }
    return -1;
}

int lame_get_exp_nspsytune(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->exp_nspsytune;
    }
    return 0;
}




/********************************************************************
 * VBR control
 ***********************************************************************/

/* Types of VBR.  default = vbr_off = CBR */
int lame_set_VBR(lame_global_flags * gfp, vbr_mode VBR) {

    if(is_lame_global_flags_valid(gfp)) {
        int     vbr_q = VBR;
        if(0 > vbr_q || vbr_max_indicator <= vbr_q)
            return -1;  /* Unknown VBR mode! */
        gfp->VBR = VBR;
        return 0;
    }
    return -1;
}

vbr_mode lame_get_VBR(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(gfp->VBR < vbr_max_indicator);
        return gfp->VBR;
    }
    return vbr_off;
}


/*
 * VBR quality level.
 *  0 = highest
 *  9 = lowest 
 */
int lame_set_VBR_q(lame_global_flags * gfp, int VBR_q) {

    if(is_lame_global_flags_valid(gfp)) {
        int     ret = 0;

        if(0 > VBR_q) {
            ret = -1;   /* Unknown VBR quality level! */
            VBR_q = 0;
        }
        if(9 < VBR_q) {
            ret = -1;
            VBR_q = 9;
        }
        gfp->VBR_q = VBR_q;
        gfp->VBR_q_frac = 0;
        return ret;
    }
    return -1;
}

int lame_get_VBR_q(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->VBR_q && 10 > gfp->VBR_q);
        return gfp->VBR_q;
    }
    return 0;
}

int lame_set_VBR_quality(lame_global_flags * gfp, float VBR_q) {

    if(is_lame_global_flags_valid(gfp)) {
        int     ret = 0;

        if(0 > VBR_q) {
            ret = -1;   /* Unknown VBR quality level! */
            VBR_q = 0;
        }
        if(9.999 < VBR_q) {
            ret = -1;
            VBR_q = 9.999;
        }

        gfp->VBR_q = (int) VBR_q;
        gfp->VBR_q_frac = VBR_q - gfp->VBR_q;

        return ret;
    }
    return -1;
}

float lame_get_VBR_quality(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->VBR_q + gfp->VBR_q_frac;
    }
    return 0;
}


/* Ignored except for VBR = vbr_abr (ABR mode) */
int lame_set_VBR_mean_bitrate_kbps(lame_global_flags * gfp, int VBR_mean_bitrate_kbps) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->VBR_mean_bitrate_kbps = VBR_mean_bitrate_kbps;
        return 0;
    }
    return -1;
}

int lame_get_VBR_mean_bitrate_kbps(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->VBR_mean_bitrate_kbps;
    }
    return 0;
}

int lame_set_VBR_min_bitrate_kbps(lame_global_flags * gfp, int VBR_min_bitrate_kbps) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->VBR_min_bitrate_kbps = VBR_min_bitrate_kbps;
        return 0;
    }
    return -1;
}

int lame_get_VBR_min_bitrate_kbps(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->VBR_min_bitrate_kbps;
    }
    return 0;
}

int lame_set_VBR_max_bitrate_kbps(lame_global_flags * gfp, int VBR_max_bitrate_kbps) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->VBR_max_bitrate_kbps = VBR_max_bitrate_kbps;
        return 0;
    }
    return -1;
}

int lame_get_VBR_max_bitrate_kbps(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->VBR_max_bitrate_kbps;
    }
    return 0;
}


/*
 * Strictly enforce VBR_min_bitrate.
 * Normally it will be violated for analog silence.
 */
int lame_set_VBR_hard_min(lame_global_flags * gfp, int VBR_hard_min) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 (disabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > VBR_hard_min || 1 < VBR_hard_min)
            return -1;

        gfp->VBR_hard_min = VBR_hard_min;

        return 0;
    }
    return -1;
}

int lame_get_VBR_hard_min(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->VBR_hard_min && 1 >= gfp->VBR_hard_min);
        return gfp->VBR_hard_min;
    }
    return 0;
}


/********************************************************************
 * Filtering control
 ***********************************************************************/

/*
 * Freqency in Hz to apply lowpass.
 *   0 = default = lame chooses
 *  -1 = disabled
 */
int lame_set_lowpassfreq(lame_global_flags * gfp, int lowpassfreq) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->lowpassfreq = lowpassfreq;
        return 0;
    }
    return -1;
}

int lame_get_lowpassfreq(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->lowpassfreq;
    }
    return 0;
}


/*
 * Width of transition band (in Hz).
 *  default = one polyphase filter band
 */
int lame_set_lowpasswidth(lame_global_flags * gfp, int lowpasswidth) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->lowpasswidth = lowpasswidth;
        return 0;
    }
    return -1;
}

int lame_get_lowpasswidth(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->lowpasswidth;
    }
    return 0;
}


/*
 * Frequency in Hz to apply highpass.
 *   0 = default = lame chooses
 *  -1 = disabled
 */
int lame_set_highpassfreq(lame_global_flags * gfp, int highpassfreq) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->highpassfreq = highpassfreq;
        return 0;
    }
    return -1;
}

int lame_get_highpassfreq(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->highpassfreq;
    }
    return 0;
}


/*
 * Width of transition band (in Hz).
 *  default = one polyphase filter band
 */
int lame_set_highpasswidth(lame_global_flags * gfp, int highpasswidth) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->highpasswidth = highpasswidth;
        return 0;
    }
    return -1;
}

int lame_get_highpasswidth(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->highpasswidth;
    }
    return 0;
}




/*
 * psycho acoustics and other arguments which you should not change 
 * unless you know what you are doing
 */


/* Adjust masking values. */
int lame_set_maskingadjust(lame_global_flags * gfp, float adjust) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->maskingadjust = adjust;
        return 0;
    }
    return -1;
}

float lame_get_maskingadjust(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->maskingadjust;
    }
    return 0;
}

int lame_set_maskingadjust_short(lame_global_flags * gfp, float adjust) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->maskingadjust_short = adjust;
        return 0;
    }
    return -1;
}

float lame_get_maskingadjust_short(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->maskingadjust_short;
    }
    return 0;
}

/* Only use ATH for masking. */
int lame_set_ATHonly(lame_global_flags * gfp, int ATHonly) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->ATHonly = ATHonly;
        return 0;
    }
    return -1;
}

int lame_get_ATHonly(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->ATHonly;
    }
    return 0;
}


/* Only use ATH for short blocks. */
int lame_set_ATHshort(lame_global_flags * gfp, int ATHshort) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->ATHshort = ATHshort;
        return 0;
    }
    return -1;
}

int lame_get_ATHshort(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->ATHshort;
    }
    return 0;
}


/* Disable ATH. */
int lame_set_noATH(lame_global_flags * gfp, int noATH) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->noATH = noATH;
        return 0;
    }
    return -1;
}

int
lame_get_noATH(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->noATH;
    }
    return 0;
}


/* Select ATH formula. */
int
lame_set_ATHtype(lame_global_flags * gfp, int ATHtype)
{
    if(is_lame_global_flags_valid(gfp)) {
        /* XXX: ATHtype should be converted to an enum. */
        gfp->ATHtype = ATHtype;
        return 0;
    }
    return -1;
}

int
lame_get_ATHtype(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->ATHtype;
    }
    return 0;
}


/* Select ATH formula 4 shape. */
int
lame_set_ATHcurve(lame_global_flags * gfp, float ATHcurve)
{
    if(is_lame_global_flags_valid(gfp)) {
        gfp->ATHcurve = ATHcurve;
        return 0;
    }
    return -1;
}

float
lame_get_ATHcurve(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->ATHcurve;
    }
    return 0;
}


/* Lower ATH by this many db. */
int
lame_set_ATHlower(lame_global_flags * gfp, float ATHlower)
{
    if(is_lame_global_flags_valid(gfp)) {
        gfp->ATH_lower_db = ATHlower;
        return 0;
    }
    return -1;
}

float
lame_get_ATHlower(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->ATH_lower_db;
    }
    return 0;
}


/* Select ATH adaptive adjustment scheme. */
int
lame_set_athaa_type(lame_global_flags * gfp, int athaa_type)
{
    if(is_lame_global_flags_valid(gfp)) {
        gfp->athaa_type = athaa_type;
        return 0;
    }
    return -1;
}

int
lame_get_athaa_type(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->athaa_type;
    }
    return 0;
}


#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
int CDECL lame_set_athaa_loudapprox(lame_global_flags * gfp, int athaa_loudapprox);
int CDECL lame_get_athaa_loudapprox(const lame_global_flags * gfp);
#else
#endif

/* Select the loudness approximation used by the ATH adaptive auto-leveling. */
int
lame_set_athaa_loudapprox(lame_global_flags * gfp, int athaa_loudapprox)
{
    (void) gfp;
    (void) athaa_loudapprox;
    return 0;
}

int
lame_get_athaa_loudapprox(const lame_global_flags * gfp)
{
    (void) gfp;
    /* obsolete, the type known under number 2 is the only survival */
    return 2;
}


/* Adjust (in dB) the point below which adaptive ATH level adjustment occurs. */
int
lame_set_athaa_sensitivity(lame_global_flags * gfp, float athaa_sensitivity)
{
    if(is_lame_global_flags_valid(gfp)) {
        gfp->athaa_sensitivity = athaa_sensitivity;
        return 0;
    }
    return -1;
}

float
lame_get_athaa_sensitivity(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->athaa_sensitivity;
    }
    return 0;
}


/* Predictability limit (ISO tonality formula) */
int     lame_set_cwlimit(lame_global_flags * gfp, int cwlimit);
int     lame_get_cwlimit(const lame_global_flags * gfp);

int
lame_set_cwlimit(lame_global_flags * gfp, int cwlimit)
{
    (void) gfp;
    (void) cwlimit;
    return 0;
}

int
lame_get_cwlimit(const lame_global_flags * gfp)
{
    (void) gfp;
    return 0;
}



/*
 * Allow blocktypes to differ between channels.
 * default:
 *  0 for jstereo => block types coupled
 *  1 for stereo  => block types may differ
 */
int
lame_set_allow_diff_short(lame_global_flags * gfp, int allow_diff_short)
{
    if(is_lame_global_flags_valid(gfp)) {
        gfp->short_blocks = allow_diff_short ? short_block_allowed : short_block_coupled;
        return 0;
    }
    return -1;
}

int
lame_get_allow_diff_short(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        if(gfp->short_blocks == short_block_allowed)
            return 1;   /* short blocks allowed to differ */
        else
            return 0;   /* not set, dispensed, forced or coupled */
    }
    return 0;
}


/* Use temporal masking effect */
int lame_set_useTemporal(lame_global_flags * gfp, int useTemporal) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 1 (enabled) */

        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 <= useTemporal && useTemporal <= 1) {
            gfp->useTemporal = useTemporal;
            return 0;
        }
    }
    return -1;
}

int
lame_get_useTemporal(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->useTemporal && 1 >= gfp->useTemporal);
        return gfp->useTemporal;
    }
    return 0;
}


/* Use inter-channel masking effect */
int
lame_set_interChRatio(lame_global_flags * gfp, float ratio)
{
    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0.0 (no inter-channel maskin) */
        if(0 <= ratio && ratio <= 1.0) {
            gfp->interChRatio = ratio;
            return 0;
        }
    }
    return -1;
}

float
lame_get_interChRatio(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        assert((0 <= gfp->interChRatio && gfp->interChRatio <= 1.0) || EQ(gfp->interChRatio, -1));
        return gfp->interChRatio;
    }
    return 0;
}


/* Use pseudo substep shaping method */
int
lame_set_substep(lame_global_flags * gfp, int method)
{
    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0.0 (no substep noise shaping) */
        if(0 <= method && method <= 7) {
            gfp->substep_shaping = method;
            return 0;
        }
    }
    return -1;
}

int
lame_get_substep(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->substep_shaping && gfp->substep_shaping <= 7);
        return gfp->substep_shaping;
    }
    return 0;
}

/* scalefactors scale */
int
lame_set_sfscale(lame_global_flags * gfp, int val)
{
    if(is_lame_global_flags_valid(gfp)) {
        gfp->noise_shaping = (val != 0) ? 2 : 1;
        return 0;
    }
    return -1;
}

int
lame_get_sfscale(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return (gfp->noise_shaping == 2) ? 1 : 0;
    }
    return 0;
}

/* subblock gain */
int
lame_set_subblock_gain(lame_global_flags * gfp, int sbgain)
{
    if(is_lame_global_flags_valid(gfp)) {
        gfp->subblock_gain = sbgain;
        return 0;
    }
    return -1;
}

int
lame_get_subblock_gain(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->subblock_gain;
    }
    return 0;
}


/* Disable short blocks. */
int
lame_set_no_short_blocks(lame_global_flags * gfp, int no_short_blocks)
{
    if(is_lame_global_flags_valid(gfp)) {
        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 <= no_short_blocks && no_short_blocks <= 1) {
            gfp->short_blocks = no_short_blocks ? short_block_dispensed : short_block_allowed;
            return 0;
        }
    }
    return -1;
}

int
lame_get_no_short_blocks(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        switch(gfp->short_blocks) {
        default:
        case short_block_not_set:
            return -1;
        case short_block_dispensed:
            return 1;
        case short_block_allowed:
        case short_block_coupled:
        case short_block_forced:
            return 0;
        }
    }
    return -1;
}


/* Force short blocks. */
int
lame_set_force_short_blocks(lame_global_flags * gfp, int short_blocks)
{
    if(is_lame_global_flags_valid(gfp)) {
        /* enforce disable/enable meaning, if we need more than two values
           we need to switch to an enum to have an apropriate representation
           of the possible meanings of the value */
        if(0 > short_blocks || 1 < short_blocks)
            return -1;

        if(short_blocks == 1)
            gfp->short_blocks = short_block_forced;
        else if(gfp->short_blocks == short_block_forced)
            gfp->short_blocks = short_block_allowed;

        return 0;
    }
    return -1;
}

int
lame_get_force_short_blocks(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        switch(gfp->short_blocks) {
        default:
        case short_block_not_set:
            return -1;
        case short_block_dispensed:
        case short_block_allowed:
        case short_block_coupled:
            return 0;
        case short_block_forced:
            return 1;
        }
    }
    return -1;
}

int
lame_set_short_threshold_lrm(lame_global_flags * gfp, float lrm)
{
    if(is_lame_global_flags_valid(gfp)) {
        gfp->attackthre = lrm;
        return 0;
    }
    return -1;
}

float
lame_get_short_threshold_lrm(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->attackthre;
    }
    return 0;
}

int
lame_set_short_threshold_s(lame_global_flags * gfp, float s)
{
    if(is_lame_global_flags_valid(gfp)) {
        gfp->attackthre_s = s;
        return 0;
    }
    return -1;
}

float
lame_get_short_threshold_s(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->attackthre_s;
    }
    return 0;
}

int
lame_set_short_threshold(lame_global_flags * gfp, float lrm, float s)
{
    if(is_lame_global_flags_valid(gfp)) {
        lame_set_short_threshold_lrm(gfp, lrm);
        lame_set_short_threshold_s(gfp, s);
        return 0;
    }
    return -1;
}


/*
 * Input PCM is emphased PCM
 * (for instance from one of the rarely emphased CDs).
 *
 * It is STRONGLY not recommended to use this, because psycho does not
 * take it into account, and last but not least many decoders
 * ignore these bits
 */
int lame_set_emphasis(lame_global_flags * gfp, int emphasis) {

    if(is_lame_global_flags_valid(gfp)) {
        /* XXX: emphasis should be converted to an enum */
        if(0 <= emphasis && emphasis < 4) {
            gfp->emphasis = emphasis;
            return 0;
        }
    }
    return -1;
}

int lame_get_emphasis(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        assert(0 <= gfp->emphasis && gfp->emphasis < 4);
        return gfp->emphasis;
    }
    return 0;
}




/***************************************************************/
/* internal variables, cannot be set...                        */
/* provided because they may be of use to calling application  */
/***************************************************************/

/* MPEG version.
 *  0 = MPEG-2
 *  1 = MPEG-1
 * (2 = MPEG-2.5)    
 */
int
lame_get_version(const lame_global_flags * gfp)
{
    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return gfc->cfg.version;
        }
    }
    return 0;
}


/* Encoder delay. */
int lame_get_encoder_delay(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return gfc->ov_enc.encoder_delay;
        }
    }
    return 0;
}

/* padding added to the end of the input */
int lame_get_encoder_padding(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return gfc->ov_enc.encoder_padding;
        }
    }
    return 0;
}


/* Size of MPEG frame. */
int lame_get_framesize(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            SessionConfig_t const *const cfg = &gfc->cfg;
            return 576 * cfg->mode_gr;
        }
    }
    return 0;
}


/* Number of frames encoded so far. */
int lame_get_frameNum(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return gfc->ov_enc.frame_number;
        }
    }
    return 0;
}

int lame_get_mf_samples_to_encode(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return gfc->sv_enc.mf_samples_to_encode;
        }
    }
    return 0;
}

int     CDECL lame_get_size_mp3buffer(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            int     size;
            compute_flushbits(gfc, &size);
            return size;
        }
    }
    return 0;
}

int lame_get_RadioGain(const lame_global_flags * gfp) {
    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return gfc->ov_rpg.RadioGain;
        }
    }
    return 0;
}

int lame_get_AudiophileGain(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return 0;
        }
    }
    return 0;
}

float lame_get_PeakSample(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return (float) gfc->ov_rpg.PeakSample;
        }
    }
    return 0;
}

int lame_get_noclipGainChange(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return gfc->ov_rpg.noclipGainChange;
        }
    }
    return 0;
}

float lame_get_noclipScale(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            return gfc->ov_rpg.noclipScale;
        }
    }
    return 0;
}


/*
 * LAME's estimate of the total number of frames to be encoded.
 * Only valid if calling program set num_samples.
 */
int lame_get_totalframes(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        lame_internal_flags const *const gfc = gfp->internal_flags;
        if(is_lame_internal_flags_valid(gfc)) {
            SessionConfig_t const *const cfg = &gfc->cfg;
            unsigned long const pcm_samples_per_frame = 576 * cfg->mode_gr;
            unsigned long pcm_samples_to_encode = gfp->num_samples;
            unsigned long end_padding = 0;
            int frames = 0;

            if(pcm_samples_to_encode == (0ul-1ul))
                return 0; /* unknown */

            /* estimate based on user set num_samples: */
            if(cfg->samplerate_in != cfg->samplerate_out) {
                /* resampling, estimate new samples_to_encode */
                double resampled_samples_to_encode = 0.0, frames_f = 0.0;
                if(cfg->samplerate_in > 0) {
                    resampled_samples_to_encode = pcm_samples_to_encode;
                    resampled_samples_to_encode *= cfg->samplerate_out;
                    resampled_samples_to_encode /= cfg->samplerate_in;
                }
                if(resampled_samples_to_encode <= 0.0)
                    return 0; /* unlikely to happen, so what, no estimate! */
                frames_f = floor(resampled_samples_to_encode / pcm_samples_per_frame);
                if(frames_f >= (INT_MAX-2))
                    return 0; /* overflow, happens eventually, no estimate! */
                frames = frames_f;
                resampled_samples_to_encode -= frames * pcm_samples_per_frame;
                pcm_samples_to_encode = ceil(resampled_samples_to_encode);
            }
            else {
                frames = pcm_samples_to_encode / pcm_samples_per_frame;
                pcm_samples_to_encode -= frames * pcm_samples_per_frame;
            }
            pcm_samples_to_encode += 576ul;
            end_padding = pcm_samples_per_frame - (pcm_samples_to_encode % pcm_samples_per_frame);
            if(end_padding < 576ul) {
                end_padding += pcm_samples_per_frame;
            }
            pcm_samples_to_encode += end_padding;
            frames += (pcm_samples_to_encode / pcm_samples_per_frame);
            /* check to see if we underestimated totalframes */
            /*    if(totalframes < gfp->frameNum) */
            /*        totalframes = gfp->frameNum; */
            return frames;
        }
    }
    return 0;
}





int lame_set_preset(lame_global_flags * gfp, int preset) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->preset = preset;
        return apply_preset(gfp, preset, 1);
    }
    return -1;
}



int lame_set_asm_optimizations(lame_global_flags * gfp, int optim, int mode) {

    if(is_lame_global_flags_valid(gfp)) {
        mode = (mode == 1 ? 1 : 0);
        switch(optim) {
        case MMX:{
                gfp->asm_optimizations.mmx = mode;
                return optim;
            }
        case AMD_3DNOW:{
                gfp->asm_optimizations.amd3dnow = mode;
                return optim;
            }
        case SSE:{
                gfp->asm_optimizations.sse = mode;
                return optim;
            }
        default:
            return optim;
        }
    }
    return -1;
}


void lame_set_write_id3tag_automatic(lame_global_flags * gfp, int v) {
    if(is_lame_global_flags_valid(gfp)) {
        gfp->write_id3tag_automatic = v;
    }
}


int lame_get_write_id3tag_automatic(lame_global_flags const *gfp) {
    if(is_lame_global_flags_valid(gfp)) {
        return gfp->write_id3tag_automatic;
    }
    return 1;
}


/*

UNDOCUMENTED, experimental settings.  These routines are not prototyped
in lame.h.  You should not use them, they are experimental and may
change.  

*/


/*
 *  just another daily changing developer switch  
 */
void CDECL lame_set_tune(lame_global_flags *, float);

void lame_set_tune(lame_global_flags * gfp, float val) {

    if(is_lame_global_flags_valid(gfp)) {
        gfp->tune_value_a = val;
        gfp->tune = 1;
    }
}

/* Custom msfix hack */
void lame_set_msfix(lame_global_flags * gfp, double msfix) {

    if(is_lame_global_flags_valid(gfp)) {
        /* default = 0 */
        gfp->msfix = msfix;
    }
}

float lame_get_msfix(const lame_global_flags * gfp) {

    if(is_lame_global_flags_valid(gfp)) {
        return gfp->msfix;
    }
    return 0;
}

#if DEPRECATED_OR_OBSOLETE_CODE_REMOVED
int CDECL lame_set_preset_expopts(lame_global_flags *, int);
#else
#endif

int lame_set_preset_expopts(lame_global_flags * gfp, int preset_expopts) {
    (void) gfp;
    (void) preset_expopts;
    return 0;
}


int lame_set_preset_notune(lame_global_flags * gfp, int preset_notune) {
    (void) gfp;
    (void) preset_notune;
    return 0;
}

static int calc_maximum_input_samples_for_buffer_size(lame_internal_flags const* gfc, size_t buffer_size) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    int const pcm_samples_per_frame = 576 * cfg->mode_gr;
    int     frames_per_buffer = 0, input_samples_per_buffer = 0;
    int     kbps = 320;

    if(cfg->samplerate_out < 16000)
        kbps = 64;
    else if(cfg->samplerate_out < 32000)
        kbps = 160;
    else
        kbps = 320;
    if(cfg->free_format)
        kbps = cfg->avg_bitrate;
    else if(cfg->vbr == vbr_off) {
        kbps = cfg->avg_bitrate;
    }
    {
        int const pad = 1;
        int const bpf = ((cfg->version + 1) * 72000 * kbps / cfg->samplerate_out + pad);
        frames_per_buffer = buffer_size / bpf;
    }
    {
        double ratio = (double) cfg->samplerate_in / cfg->samplerate_out;
        input_samples_per_buffer = pcm_samples_per_frame * frames_per_buffer * ratio;
    }
  return input_samples_per_buffer;
	}

int lame_get_maximum_number_of_samples(lame_t gfp, size_t buffer_size) {

    if(is_lame_global_flags_valid(gfp)) {
      lame_internal_flags const *const gfc = gfp->internal_flags;
      if(is_lame_internal_flags_valid(gfc)) {
        return calc_maximum_input_samples_for_buffer_size(gfc, buffer_size);
        }
    }
  return LAME_GENERICERROR;
	}



/*
 *      Version numbering for LAME.
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

/*!
  \file   version.c
  \brief  Version numbering for LAME.

  Contains functions which describe the version of LAME.

  \author A.L. Faber
  \version \$Id: version.c,v 1.34 2011/11/18 09:51:02 robert Exp $
  \ingroup libmp3lame
*/


/*! Get the LAME version string. */
/*!
  \param void
  \return a pointer to a string which describes the version of LAME.
*/
const char *get_lame_version(void) {
	/* primary to write screen reports */
    /* Here we can also add informations about compile time configurations */

#if   LAME_ALPHA_VERSION
    static /*@observer@ */ const char *const str =
        STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) " "
        "(alpha " STR(LAME_PATCH_VERSION) ", " __DATE__ " " __TIME__ ")";
#elif LAME_BETA_VERSION
    static /*@observer@ */ const char *const str =
        STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) " "
        "(beta " STR(LAME_PATCH_VERSION) ", " __DATE__ ")";
#elif LAME_RELEASE_VERSION && (LAME_PATCH_VERSION > 0)
    static /*@observer@ */ const char *const str =
        STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) "." STR(LAME_PATCH_VERSION);
#else
    static /*@observer@ */ const char *const str =
        STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION);
#endif

  return str;
	}


/*! Get the short LAME version string. */
/*!
  It's mainly for inclusion into the MP3 stream.

  \param void   
  \return a pointer to the short version of the LAME version string.
*/
const char *get_lame_short_version(void) {

    /* adding date and time to version string makes it harder for output
       validation */

#if   LAME_ALPHA_VERSION
    static /*@observer@ */ const char *const str =
        STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) " (alpha " STR(LAME_PATCH_VERSION) ")";
#elif LAME_BETA_VERSION
    static /*@observer@ */ const char *const str =
        STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) " (beta " STR(LAME_PATCH_VERSION) ")";
#elif LAME_RELEASE_VERSION && (LAME_PATCH_VERSION > 0)
    static /*@observer@ */ const char *const str =
        STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) "." STR(LAME_PATCH_VERSION);
#else
    static /*@observer@ */ const char *const str =
        STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION);
#endif

    return str;
	}

/*! Get the _very_ short LAME version string. */
/*!
  It's used in the LAME VBR tag only.

  \param void   
  \return a pointer to the short version of the LAME version string.
*/
const char *get_lame_very_short_version(void) {

    /* adding date and time to version string makes it harder for output
       validation */
#if   LAME_ALPHA_VERSION
#define P "a"
#elif LAME_BETA_VERSION
#define P "b"
#elif LAME_RELEASE_VERSION && (LAME_PATCH_VERSION > 0)
#define P "r"
#else
#define P " "
#endif
    static /*@observer@ */ const char *const str =
#if(LAME_PATCH_VERSION > 0)
      "LAME" STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) P STR(LAME_PATCH_VERSION)
#else
      "LAME" STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) P
#endif
      ;

  return str;
	}

/*! Get the _very_ short LAME version string. */
/*!
  It's used in the LAME VBR tag only, limited to 9 characters max.
  Due to some 3rd party HW/SW decoders, it has to start with LAME.

  \param void   
  \return a pointer to the short version of the LAME version string.
 */
const char* get_lame_tag_encoder_short_version(void) {
    static /*@observer@ */ const char *const str =
            /* FIXME: new scheme / new version counting / drop versioning here ? */
    "LAME" STR(LAME_MAJOR_VERSION) "." STR(LAME_MINOR_VERSION) P
    ;
  
	return str;
	}

/*! Get the version string for GPSYCHO. */
/*!
  \param void
  \return a pointer to a string which describes the version of GPSYCHO.
*/
const char * get_psy_version(void) {

#if   PSY_ALPHA_VERSION > 0
    static /*@observer@ */ const char *const str =
        STR(PSY_MAJOR_VERSION) "." STR(PSY_MINOR_VERSION)
        " (alpha " STR(PSY_ALPHA_VERSION) ", " __DATE__ " " __TIME__ ")";
#elif PSY_BETA_VERSION > 0
    static /*@observer@ */ const char *const str =
        STR(PSY_MAJOR_VERSION) "." STR(PSY_MINOR_VERSION)
        " (beta " STR(PSY_BETA_VERSION) ", " __DATE__ ")";
#else
    static /*@observer@ */ const char *const str =
        STR(PSY_MAJOR_VERSION) "." STR(PSY_MINOR_VERSION);
#endif

    return str;
}


/*! Get the URL for the LAME website. */
/*!
  \param void
  \return a pointer to a string which is a URL for the LAME website.
*/
const char *get_lame_url(void) {
    static /*@observer@ */ const char *const str = LAME_URL;

    return str;
}


/*! Get the numerical representation of the version. */
/*!
  Writes the numerical representation of the version of LAME and
  GPSYCHO into lvp.

  \param lvp    
*/
void get_lame_version_numerical(lame_version_t * lvp) {
    static /*@observer@ */ const char *const features = ""; /* obsolete */

    /* generic version */
    lvp->major = LAME_MAJOR_VERSION;
    lvp->minor = LAME_MINOR_VERSION;
#if LAME_ALPHA_VERSION
    lvp->alpha = LAME_PATCH_VERSION;
    lvp->beta = 0;
#elif LAME_BETA_VERSION
    lvp->alpha = 0;
    lvp->beta = LAME_PATCH_VERSION;
#else
    lvp->alpha = 0;
    lvp->beta = 0;
#endif

    /* psy version */
    lvp->psy_major = PSY_MAJOR_VERSION;
    lvp->psy_minor = PSY_MINOR_VERSION;
    lvp->psy_alpha = PSY_ALPHA_VERSION;
    lvp->psy_beta = PSY_BETA_VERSION;

    /* compile time features */
    /*@-mustfree@ */
    lvp->features = features;
    /*@=mustfree@ */
}


const char * get_lame_os_bitness(void) {
    static /*@observer@ */ const char *const strXX = "";
    static /*@observer@ */ const char *const str32 = "32bits";
    static /*@observer@ */ const char *const str64 = "64bits";

    switch(sizeof(void *)) {
    case 4:
        return str32;

    case 8:
        return str64;

    default:
        return strXX;
    }
}

/* end of version.c */



/*
 *      bit reservoir source file
 *
 *      Copyright (c) 1999-2000 Mark Taylor
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

/* $Id: reservoir.c,v 1.45 2011/05/07 16:05:17 rbrito Exp $ */



/*
  ResvFrameBegin:
  Called (repeatedly) at the beginning of a frame. Updates the maximum
  size of the reservoir, and checks to make sure main_data_begin
  was set properly by the formatter
*/

/*
 *  Background information:
 *
 *  This is the original text from the ISO standard. Because of
 *  sooo many bugs and irritations correcting comments are added
 *  in brackets []. A '^W' means you should remove the last word.
 *
 *  1) The following rule can be used to calculate the maximum
 *     number of bits used for one granule [^W frame]:
 *     At the highest possible bitrate of Layer III (320 kbps
 *     per stereo signal [^W^W^W], 48 kHz) the frames must be of
 *     [^W^W^W are designed to have] constant length, i.e.
 *     one buffer [^W^W the frame] length is:
 *
 *         320 kbps * 1152/48 kHz = 7680 bit = 960 byte
 *
 *     This value is used as the maximum buffer per channel [^W^W] at
 *     lower bitrates [than 320 kbps]. At 64 kbps mono or 128 kbps
 *     stereo the main granule length is 64 kbps * 576/48 kHz = 768 bit
 *     [per granule and channel] at 48 kHz sampling frequency.
 *     This means that there is a maximum deviation (short time buffer
 *     [= reservoir]) of 7680 - 2*2*768 = 4608 bits is allowed at 64 kbps.
 *     The actual deviation is equal to the number of bytes [with the
 *     meaning of octets] denoted by the main_data_end offset pointer.
 *     The actual maximum deviation is (2^9-1)*8 bit = 4088 bits
 *     [for MPEG-1 and (2^8-1)*8 bit for MPEG-2, both are hard limits].
 *     ... The xchange of buffer bits between the left and right channel
 *     is allowed without restrictions [exception: dual channel].
 *     Because of the [constructed] constraint on the buffer size
 *     main_data_end is always set to 0 in the case of bit_rate_index==14,
 *     i.e. data rate 320 kbps per stereo signal [^W^W^W]. In this case
 *     all data are allocated between adjacent header [^W sync] words
 *     [, i.e. there is no buffering at all].
 */

int ResvFrameBegin(lame_internal_flags * gfc, int *mean_bits) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *const esv = &gfc->sv_enc;
    int     fullFrameBits;
    int     resvLimit;
    int     maxmp3buf;
    III_side_info_t *const l3_side = &gfc->l3_side;
    int     frameLength;
    int     meanBits;

    frameLength = getframebits(gfc);
    meanBits = (frameLength - cfg->sideinfo_len * 8) / cfg->mode_gr;

/*
 *  Meaning of the variables:
 *      resvLimit: (0, 8, ..., 8*255 (MPEG-2), 8*511 (MPEG-1))
 *          Number of bits can be stored in previous frame(s) due to
 *          counter size constaints
 *      maxmp3buf: ( ??? ... 8*1951 (MPEG-1 and 2), 8*2047 (MPEG-2.5))
 *          Number of bits allowed to encode one frame (you can take 8*511 bit
 *          from the bit reservoir and at most 8*1440 bit from the current
 *          frame (320 kbps, 32 kHz), so 8*1951 bit is the largest possible
 *          value for MPEG-1 and -2)
 *
 *          maximum allowed granule/channel size times 4 = 8*2047 bits.,
 *          so this is the absolute maximum supported by the format.
 *
 *
 *      fullFrameBits:  maximum number of bits available for encoding
 *                      the current frame.
 *
 *      mean_bits:      target number of bits per granule.
 *
 *      frameLength:
 *
 *      gfc->ResvMax:   maximum allowed reservoir
 *
 *      gfc->ResvSize:  current reservoir size
 *
 *      l3_side->resvDrain_pre:
 *         ancillary data to be added to previous frame:
 *         (only usefull in VBR modes if it is possible to have
 *         maxmp3buf < fullFrameBits)).  Currently disabled,
 *         see #define NEW_DRAIN
 *         2010-02-13: RH now enabled, it seems to be needed for CBR too,
 *                     as there exists one example, where the FhG decoder
 *                     can't decode a -b320 CBR file anymore.
 *
 *      l3_side->resvDrain_post:
 *         ancillary data to be added to this frame:
 *
 */

    /* main_data_begin has 9 bits in MPEG-1, 8 bits MPEG-2 */
    resvLimit = (8 * 256) * cfg->mode_gr - 8;

    /* maximum allowed frame size.  dont use more than this number of
       bits, even if the frame has the space for them: */
    maxmp3buf = cfg->buffer_constraint;
    esv->ResvMax = maxmp3buf - frameLength;
    if(esv->ResvMax > resvLimit)
        esv->ResvMax = resvLimit;
    if(esv->ResvMax < 0 || cfg->disable_reservoir)
        esv->ResvMax = 0;
    
    fullFrameBits = meanBits * cfg->mode_gr + Min(esv->ResvSize, esv->ResvMax);

    if(fullFrameBits > maxmp3buf)
        fullFrameBits = maxmp3buf;

    assert(0 == esv->ResvMax % 8);
    assert(esv->ResvMax >= 0);

    l3_side->resvDrain_pre = 0;

    if(gfc->pinfo != NULL) {
        gfc->pinfo->mean_bits = meanBits / 2; /* expected bits per channel per granule [is this also right for mono/stereo, MPEG-1/2 ?] */
        gfc->pinfo->resvsize = esv->ResvSize;
    }
    *mean_bits = meanBits;
    return fullFrameBits;
}


/*
  ResvMaxBits
  returns targ_bits:  target number of bits to use for 1 granule
         extra_bits:  amount extra available from reservoir
  Mark Taylor 4/99
*/
void ResvMaxBits(lame_internal_flags * gfc, int mean_bits, int *targ_bits, int *extra_bits, int cbr) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *const esv = &gfc->sv_enc;
    int     add_bits, targBits, extraBits;
    int     ResvSize = esv->ResvSize, ResvMax = esv->ResvMax;

    /* conpensate the saved bits used in the 1st granule */
    if(cbr)
        ResvSize += mean_bits;

    if(gfc->sv_qnt.substep_shaping & 1)
        ResvMax *= 0.9;

    targBits = mean_bits;

    /* extra bits if the reservoir is almost full */
    if(ResvSize * 10 > ResvMax * 9) {
        add_bits = ResvSize - (ResvMax * 9) / 10;
        targBits += add_bits;
        gfc->sv_qnt.substep_shaping |= 0x80;
    }
    else {
        add_bits = 0;
        gfc->sv_qnt.substep_shaping &= 0x7f;
        /* build up reservoir.  this builds the reservoir a little slower
         * than FhG.  It could simple be mean_bits/15, but this was rigged
         * to always produce 100 (the old value) at 128kbs */
        /*    *targ_bits -= (int) (mean_bits/15.2); */
        if(!cfg->disable_reservoir && !(gfc->sv_qnt.substep_shaping & 1))
            targBits -= .1 * mean_bits;
    }


    /* amount from the reservoir we are allowed to use. ISO says 6/10 */
    extraBits = (ResvSize < (esv->ResvMax * 6) / 10 ? ResvSize : (esv->ResvMax * 6) / 10);
    extraBits -= add_bits;

    if(extraBits < 0)
        extraBits = 0;

    *targ_bits = targBits;
    *extra_bits = extraBits;
}

/*
  ResvAdjust:
  Called after a granule's bit allocation. Readjusts the size of
  the reservoir to reflect the granule's usage.
*/
void
ResvAdjust(lame_internal_flags * gfc, gr_info const *gi)
{
    gfc->sv_enc.ResvSize -= gi->part2_3_length + gi->part2_length;
}


/*
  ResvFrameEnd:
  Called after all granules in a frame have been allocated. Makes sure
  that the reservoir size is within limits, possibly by adding stuffing
  bits.
*/
void ResvFrameEnd(lame_internal_flags * gfc, int mean_bits) {
    SessionConfig_t const *const cfg = &gfc->cfg;
    EncStateVar_t *const esv = &gfc->sv_enc;
    III_side_info_t *const l3_side = &gfc->l3_side;
    int     stuffingBits;
    int     over_bits;

    esv->ResvSize += mean_bits * cfg->mode_gr;
    stuffingBits = 0;
    l3_side->resvDrain_post = 0;
    l3_side->resvDrain_pre = 0;

    /* we must be byte aligned */
    if((over_bits = esv->ResvSize % 8) != 0)
        stuffingBits += over_bits;


    over_bits = (esv->ResvSize - stuffingBits) - esv->ResvMax;
    if(over_bits > 0) {
        assert(0 == over_bits % 8);
        assert(over_bits >= 0);
        stuffingBits += over_bits;
    }


    /* NOTE: enabling the NEW_DRAIN code fixes some problems with FhG decoder
             shipped with MS Windows operating systems. Using this, it is even
             possible to use Gabriel's lax buffer consideration again, which
             assumes, any decoder should have a buffer large enough
             for a 320 kbps frame at 32 kHz sample rate.

       old drain code:
             lame -b320 BlackBird.wav ---> does not play with GraphEdit.exe using FhG decoder V1.5 Build 50

       new drain code:
             lame -b320 BlackBird.wav ---> plays fine with GraphEdit.exe using FhG decoder V1.5 Build 50

             Robert Hegemann, 2010-02-13.
     */
    /* drain as many bits as possible into previous frame ancillary data
     * In particular, in VBR mode ResvMax may have changed, and we have
     * to make sure main_data_begin does not create a reservoir bigger
     * than ResvMax  mt 4/00*/
    {
        int     mdb_bytes = Min(l3_side->main_data_begin * 8, stuffingBits) / 8;
        l3_side->resvDrain_pre += 8 * mdb_bytes;
        stuffingBits -= 8 * mdb_bytes;
        esv->ResvSize -= 8 * mdb_bytes;
        l3_side->main_data_begin -= mdb_bytes;
    }
    /* drain the rest into this frames ancillary data */
    l3_side->resvDrain_post += stuffingBits;
    esv->ResvSize -= stuffingBits;
	}


/*
 * id3tag.c -- Write ID3 version 1 and 2 tags.
 *
 * Copyright (C) 2000 Don Melton
 * Copyright (C) 2011-2017 Robert Hegemann
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

/*
 * HISTORY: This source file is part of LAME (see http://www.mp3dev.org)
 * and was originally adapted by Conrad Sanderson <c.sanderson@me.gu.edu.au>
 * from mp3info by Ricardo Cerqueira <rmc@rccn.net> to write only ID3 version 1
 * tags.  Don Melton <don@blivet.com> COMPLETELY rewrote it to support version
 * 2 tags and be more conformant to other standards while remaining flexible.
 *
 * NOTE: See http://id3.org/ for more information about ID3 tag formats.
 */

/* $Id: id3tag.c,v 1.80 2017/08/28 15:39:51 robert Exp $ */


static const char *const genre_names[] = {
    /*
     * NOTE: The spelling of these genre names is identical to those found in
     * Winamp and mp3info.
     */
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
    "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
    "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
    "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop",
    "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental",
    "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Alternative Rock",
    "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop",
    "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial",
    "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy",
    "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
    "Native US", "Cabaret", "New Wave", "Psychedelic", "Rave",
    "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
    "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
    "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin",
    "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock",
    "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
    "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour", "Speech",
    "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass",
    "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba",
    "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet",
    "Punk Rock", "Drum Solo", "A Cappella", "Euro-House", "Dance Hall",
    "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie",
    "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta",
    "Heavy Metal", "Black Metal", "Crossover", "Contemporary Christian",
    "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
    "SynthPop"
};

#define GENRE_NAME_COUNT \
    ((int)(sizeof genre_names / sizeof (const char *const)))

static const int genre_alpha_map[] = {
    123, 34, 74, 73, 99, 20, 40, 26, 145, 90, 116, 41, 135, 85, 96, 138, 89, 0,
    107, 132, 65, 88, 104, 102, 97, 136, 61, 141, 32, 1, 112, 128, 57, 140, 2,
    139, 58, 3, 125, 50, 22, 4, 55, 127, 122, 120, 98, 52, 48, 54, 124, 25, 84,
    80, 115, 81, 119, 5, 30, 36, 59, 126, 38, 49, 91, 6, 129, 79, 137, 7, 35,
    100, 131, 19, 33, 46, 47, 8, 29, 146, 63, 86, 71, 45, 142, 9, 77, 82, 64,
    133, 10, 66, 39, 11, 103, 12, 75, 134, 13, 53, 62, 109, 117, 23, 108, 92,
    67, 93, 43, 121, 15, 68, 14, 16, 76, 87, 118, 17, 78, 143, 114, 110, 69, 21,
    111, 95, 105, 42, 37, 24, 56, 44, 101, 83, 94, 106, 147, 113, 18, 51, 130,
    144, 60, 70, 31, 72, 27, 28
};

#define GENRE_ALPHA_COUNT ((int)(sizeof genre_alpha_map / sizeof (int)))

#define GENRE_INDEX_OTHER 12


#define FRAME_ID(a, b, c, d) \
    ( ((unsigned long)(a) << 24) \
    | ((unsigned long)(b) << 16) \
    | ((unsigned long)(c) <<  8) \
    | ((unsigned long)(d) <<  0) )

typedef enum UsualStringIDs { ID_TITLE = FRAME_ID('T', 'I', 'T', '2')
        , ID_ARTIST = FRAME_ID('T', 'P', 'E', '1')
        , ID_ALBUM = FRAME_ID('T', 'A', 'L', 'B')
        , ID_GENRE = FRAME_ID('T', 'C', 'O', 'N')
        , ID_ENCODER = FRAME_ID('T', 'S', 'S', 'E')
        , ID_PLAYLENGTH = FRAME_ID('T', 'L', 'E', 'N')
        , ID_COMMENT = FRAME_ID('C', 'O', 'M', 'M') /* full text string */
} UsualStringIDs;

typedef enum NumericStringIDs { ID_DATE = FRAME_ID('T', 'D', 'A', 'T') /* "ddMM" */
        , ID_TIME = FRAME_ID('T', 'I', 'M', 'E') /* "hhmm" */
        , ID_TPOS = FRAME_ID('T', 'P', 'O', 'S') /* '0'-'9' and '/' allowed */
        , ID_TRACK = FRAME_ID('T', 'R', 'C', 'K') /* '0'-'9' and '/' allowed */
        , ID_YEAR = FRAME_ID('T', 'Y', 'E', 'R') /* "yyyy" */
} NumericStringIDs;

typedef enum MiscIDs { ID_TXXX = FRAME_ID('T', 'X', 'X', 'X')
        , ID_WXXX = FRAME_ID('W', 'X', 'X', 'X')
        , ID_SYLT = FRAME_ID('S', 'Y', 'L', 'T')
        , ID_APIC = FRAME_ID('A', 'P', 'I', 'C')
        , ID_GEOB = FRAME_ID('G', 'E', 'O', 'B')
        , ID_PCNT = FRAME_ID('P', 'C', 'N', 'T')
        , ID_AENC = FRAME_ID('A', 'E', 'N', 'C')
        , ID_LINK = FRAME_ID('L', 'I', 'N', 'K')
        , ID_ENCR = FRAME_ID('E', 'N', 'C', 'R')
        , ID_GRID = FRAME_ID('G', 'R', 'I', 'D')
        , ID_PRIV = FRAME_ID('P', 'R', 'I', 'V')
        , ID_USLT = FRAME_ID('U', 'S', 'L', 'T') /* full text string */
        , ID_USER = FRAME_ID('U', 'S', 'E', 'R') /* full text string */
        , ID_PCST = FRAME_ID('P', 'C', 'S', 'T') /* iTunes Podcast indicator, only presence important */
        , ID_WFED = FRAME_ID('W', 'F', 'E', 'D') /* iTunes Podcast URL as TEXT FRAME !!! violates standard */
} MiscIDs;


static int frame_id_matches(int id, int mask) {
    int     result = 0, i, window = 0xff;

    for(i = 0; i < 4; ++i, window <<= 8) {
        int const mw = (mask & window);
        int const iw = (id & window);
        if(mw != 0 && mw != iw) {
            result |= iw;
        }
    }
    return result;
	}

static int isFrameIdMatching(int id, int mask) {

    return frame_id_matches(id, mask) == 0 ? 1 : 0;
	}

static int test_tag_spec_flags(lame_internal_flags const *gfc, unsigned int tst) {

    return (gfc->tag_spec.flags & tst) != 0u ? 1 : 0;
	}

#if 0
static void
debug_tag_spec_flags(lame_internal_flags * gfc, const char* info) {

    MSGF(gfc, "%s\n", info);
    MSGF(gfc, "CHANGED_FLAG  : %d\n", test_tag_spec_flags(gfc, CHANGED_FLAG )); 
    MSGF(gfc, "ADD_V2_FLAG   : %d\n", test_tag_spec_flags(gfc, ADD_V2_FLAG  )); 
    MSGF(gfc, "V1_ONLY_FLAG  : %d\n", test_tag_spec_flags(gfc, V1_ONLY_FLAG )); 
    MSGF(gfc, "V2_ONLY_FLAG  : %d\n", test_tag_spec_flags(gfc, V2_ONLY_FLAG )); 
    MSGF(gfc, "SPACE_V1_FLAG : %d\n", test_tag_spec_flags(gfc, SPACE_V1_FLAG)); 
    MSGF(gfc, "PAD_V2_FLAG   : %d\n", test_tag_spec_flags(gfc, PAD_V2_FLAG  )); 
}
#endif

static int is_lame_internal_flags_null(lame_t gfp) {
    return (gfp && gfp->internal_flags) ? 0 : 1;
	}

static int id3v2_add_ucs2_lng(lame_t gfp, uint32_t frame_id, unsigned short const *desc, unsigned short const *text);
static int id3v2_add_latin1_lng(lame_t gfp, uint32_t frame_id, char const *desc, char const *text);


static void copyV1ToV2(lame_t gfp, int frame_id, char const *s) {
    lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;

    if(gfc) {
        unsigned int flags = gfc->tag_spec.flags;
        id3v2_add_latin1_lng(gfp, frame_id, 0, s);
        gfc->tag_spec.flags = flags;
#if 0
        debug_tag_spec_flags(gfc, "copyV1ToV2");
#endif
    }
	}


static void id3v2AddLameVersion(lame_t gfp) {
    char buffer[1024];
    const char *b = get_lame_os_bitness();
    const char *v = get_lame_version();
    const char *u = get_lame_url();
    const size_t lenb = strlen(b);

    if(lenb > 0) {
        sprintf(buffer, "LAMEgd %s version %s (%s)", b, v, u);
    }
    else {
        sprintf(buffer, "LAMEgd version %s (%s)", v, u);
    }
    copyV1ToV2(gfp, ID_ENCODER, buffer);
	}

static void id3v2AddAudioDuration(lame_t gfp, double ms) {
    SessionConfig_t const *const cfg = &gfp->internal_flags->cfg; /* caller checked pointers */
    char buffer[1024];
    double const max_ulong = MAX_U_32_NUM;
    unsigned long playlength_ms;

    ms *= 1000;
    ms /= cfg->samplerate_in;
    if(ms > max_ulong) {
        playlength_ms = max_ulong;
    }
    else if(ms < 0) {
        playlength_ms = 0;
    }
    else {
        playlength_ms = ms;
    }
  sprintf(buffer, "%lu", playlength_ms);
  copyV1ToV2(gfp, ID_PLAYLENGTH, buffer);
	}

void id3tag_genre_list(void (*handler) (int, const char *, void *), void *cookie) {

    if(handler) {
        int     i;
        for(i = 0; i < GENRE_NAME_COUNT; ++i) {
            if(i < GENRE_ALPHA_COUNT) {
                int     j = genre_alpha_map[i];
                handler(j, genre_names[j], cookie);
            }
        }
    }
	}

#define GENRE_NUM_UNKNOWN 255



void id3tag_init(lame_t gfp) {
  lame_internal_flags *gfc = NULL;

  if(is_lame_internal_flags_null(gfp)) {
      return;
    }
  gfc = gfp->internal_flags;
  free_id3tag(gfc);
  memset(&gfc->tag_spec, 0, sizeof gfc->tag_spec);
  gfc->tag_spec.genre_id3v1 = GENRE_NUM_UNKNOWN;
  gfc->tag_spec.padding_size = 128;
  id3v2AddLameVersion(gfp);
	}



void id3tag_add_v2(lame_t gfp) {
  lame_internal_flags *gfc = NULL;

    if(is_lame_internal_flags_null(gfp)) {
        return;
    }
  gfc = gfp->internal_flags;
  gfc->tag_spec.flags &= ~V1_ONLY_FLAG;
  gfc->tag_spec.flags |= ADD_V2_FLAG;
	}

void id3tag_v1_only(lame_t gfp) {
  lame_internal_flags *gfc = NULL;

    if(is_lame_internal_flags_null(gfp)) {
        return;
    }
    gfc = gfp->internal_flags;
    gfc->tag_spec.flags &= ~(ADD_V2_FLAG | V2_ONLY_FLAG);
    gfc->tag_spec.flags |= V1_ONLY_FLAG;
	}

void id3tag_v2_only(lame_t gfp) {
  lame_internal_flags *gfc = NULL;

    if(is_lame_internal_flags_null(gfp)) {
        return;
    }
    gfc = gfp->internal_flags;
    gfc->tag_spec.flags &= ~V1_ONLY_FLAG;
    gfc->tag_spec.flags |= V2_ONLY_FLAG;
	}

void id3tag_space_v1(lame_t gfp) {
  lame_internal_flags *gfc = NULL;

    if(is_lame_internal_flags_null(gfp)) {
        return;
    }
    gfc = gfp->internal_flags;
    gfc->tag_spec.flags &= ~V2_ONLY_FLAG;
    gfc->tag_spec.flags |= SPACE_V1_FLAG;
	}

void id3tag_pad_v2(lame_t gfp) {
    id3tag_set_pad(gfp, 128);
	}

void id3tag_set_pad(lame_t gfp, size_t n) {
  lame_internal_flags *gfc = NULL;

    if(is_lame_internal_flags_null(gfp)) {
        return;
    }
  gfc = gfp->internal_flags;
  gfc->tag_spec.flags &= ~V1_ONLY_FLAG;
  gfc->tag_spec.flags |= PAD_V2_FLAG;
  gfc->tag_spec.flags |= ADD_V2_FLAG;
  gfc->tag_spec.padding_size = (unsigned int)n;
	}

static int hasUcs2ByteOrderMarker(unsigned short bom) {

    if(bom == 0xFFFEu || bom == 0xFEFFu) {
        return 1;
    }
    return 0;
	}


static unsigned short swap_bytes(unsigned short w) {
    return (0xff00u & (w << 8)) | (0x00ffu & (w >> 8));
	}


static unsigned short toLittleEndian(unsigned short bom, unsigned short c) {

    if(bom == 0xFFFEu) {
        return swap_bytes(c);
    }
    return c;
	}

static unsigned short fromLatin1Char(const unsigned short* s, unsigned short c) {

    if(s[0] == 0xFFFEu) {
        return swap_bytes(c);
    }
    return c;
	}


static size_t local_strdup(char **dst, const char *src) {

    if(!dst) {
        return 0;
    }
    free(*dst);
    *dst = 0;
    if(src) {
        size_t  n;
        for(n = 0; src[n] != 0; ++n) { /* calc src string length */
        }
        if(n > 0) {    /* string length without zero termination */
            assert(sizeof(*src) == sizeof(**dst));
            *dst = lame_calloc(char, n + 1);
            if(*dst != 0) {
                memcpy(*dst, src, n * sizeof(**dst));
                (*dst)[n] = 0;
                return n;
            }
        }
    }
    return 0;
}

static  size_t local_ucs2_strdup(unsigned short **dst, unsigned short const *src) {

    if(!dst) {
        return 0;
    }
    free(*dst);         /* free old string pointer */
    *dst = 0;
    if(src) {
        size_t  n;
        for(n = 0; src[n] != 0; ++n) { /* calc src string length */
        }
        if(n > 0) {    /* string length without zero termination */
            assert(sizeof(*src) >= 2);
            assert(sizeof(*src) == sizeof(**dst));
            *dst = lame_calloc(unsigned short, n + 1);
            if(*dst != 0) {
                memcpy(*dst, src, n * sizeof(**dst));
                (*dst)[n] = 0;
                return n;
            }
        }
    }
    return 0;
}


static size_t local_ucs2_strlen(unsigned short const *s) {
    size_t  n = 0;

    if(s) {
        while(*s++) {
            ++n;
        }
    }
    return n;
}


static size_t local_ucs2_substr(unsigned short** dst, unsigned short const* src, size_t start, size_t end) {
    size_t const len = 1 + 1 + ((start < end) ? (end - start) : 0);
    size_t n = 0;
    unsigned short *ptr = lame_calloc(unsigned short, len);

    *dst = ptr;
    if(ptr == 0 || src == 0) {
        return 0;
    }
    if(hasUcs2ByteOrderMarker(src[0])) {
        ptr[n++] = src[0];
        if(start == 0) {
            ++start;
        }
    }
    while(start < end) {
        ptr[n++] = src[start++];
    }
    ptr[n] = 0;
    return n;
}

static int local_ucs2_pos(unsigned short const* str, unsigned short c) {
    int     i;

    for(i = 0; str != 0 && str[i] != 0; ++i) {
        if(str[i] == c) {
            return i;
        }
    }
    return -1;
}

static int local_char_pos(char const* str, char c) {
    int     i;

    for(i = 0; str != 0 && str[i] != 0; ++i) {
        if(str[i] == c) {
            return i;
        }
    }
    return -1;
	}

static int maybeLatin1(unsigned short const* text) {

    if(text) {
        unsigned short bom = *text++;
        while(*text) {
            unsigned short c = toLittleEndian(bom, *text++);
            if(c > 0x00fe) return 0;
        }
    }
    return 1;
	}

static int searchGenre(char const* genre);
static int sloppySearchGenre(char const* genre);

static int lookupGenre(char const* genre) {
    char   *str;
    int     num = strtol(genre, &str, 10);

    /* is the input a string or a valid number? */
    if(*str) {
        num = searchGenre(genre);
        if(num == GENRE_NAME_COUNT) {
            num = sloppySearchGenre(genre);
        }
        if(num == GENRE_NAME_COUNT) {
            return -2; /* no common genre text found */
        }
    }
    else {
        if((num < 0) || (num >= GENRE_NAME_COUNT)) {
            return -1; /* number unknown */
        }
    }
    return num;
	}

static unsigned char *writeLoBytes(unsigned char *frame, unsigned short const *str, size_t n);

static char *local_strdup_utf16_to_latin1(unsigned short const* utf16) {
    size_t  len = local_ucs2_strlen(utf16);
    unsigned char* latin1 = lame_calloc(unsigned char, len+1);

    writeLoBytes(latin1, utf16, len);
    return (char*)latin1;
	}


static int id3tag_set_genre_utf16(lame_t gfp, unsigned short const* text) {
    lame_internal_flags* gfc = gfp->internal_flags;
    int   ret;

    if(!text) {
        return -3;
    }
    if(!hasUcs2ByteOrderMarker(text[0])) {
        return -3;
    }
    if(maybeLatin1(text)) {
        char*   latin1 = local_strdup_utf16_to_latin1(text);
        int     num = lookupGenre(latin1);
        free(latin1);
        if(num == -1) return -1; /* number out of range */
        if(num >= 0) {           /* common genre found  */
            gfc->tag_spec.flags |= CHANGED_FLAG;
            gfc->tag_spec.genre_id3v1 = num;
            copyV1ToV2(gfp, ID_GENRE, genre_names[num]);
            return 0;
        }
    }
    ret = id3v2_add_ucs2_lng(gfp, ID_GENRE, 0, text);
    if(ret == 0) {
        gfc->tag_spec.flags |= CHANGED_FLAG;
        gfc->tag_spec.genre_id3v1 = GENRE_INDEX_OTHER;
    }
    return ret;
}

/*
Some existing options for ID3 tag can be specified by --tv option
as follows.
--tt <value>, --tv TIT2=value
--ta <value>, --tv TPE1=value
--tl <value>, --tv TALB=value
--ty <value>, --tv TYER=value
--tn <value>, --tv TRCK=value
--tg <value>, --tv TCON=value
(although some are not exactly same)*/

int id3tag_set_albumart(lame_t gfp, const char *image, size_t size) {
    int     mimetype = MIMETYPE_NONE;
    lame_internal_flags *gfc = 0;

    if(is_lame_internal_flags_null(gfp)) {
        return 0;
    }
    gfc = gfp->internal_flags;

    if(image != 0) {
        unsigned char const *data = (unsigned char const *) image;
        /* determine MIME type from the actual image data */
        if(2 < size && data[0] == 0xFF && data[1] == 0xD8) {
            mimetype = MIMETYPE_JPEG;
        }
        else if(4 < size && data[0] == 0x89 && strncmp((const char *) &data[1], "PNG", 3) == 0) {
            mimetype = MIMETYPE_PNG;
        }
        else if(4 < size && strncmp((const char *) data, "GIF8", 4) == 0) {
            mimetype = MIMETYPE_GIF;
        }
        else {
            return -1;
        }
    }
    if(gfc->tag_spec.albumart != 0) {
        free(gfc->tag_spec.albumart);
        gfc->tag_spec.albumart = 0;
        gfc->tag_spec.albumart_size = 0;
        gfc->tag_spec.albumart_mimetype = MIMETYPE_NONE;
    }
    if(size < 1 || mimetype == MIMETYPE_NONE) {
        return 0;
    }
    gfc->tag_spec.albumart = lame_calloc(unsigned char, size);
    if(gfc->tag_spec.albumart != 0) {
        memcpy(gfc->tag_spec.albumart, image, size);
        gfc->tag_spec.albumart_size = (unsigned int)size;
        gfc->tag_spec.albumart_mimetype = mimetype;
        gfc->tag_spec.flags |= CHANGED_FLAG;
        id3tag_add_v2(gfp);
    }
    return 0;
}

static unsigned char *set_4_byte_value(unsigned char *bytes, uint32_t value) {
    int     i;

    for(i = 3; i >= 0; --i) {
        bytes[i] = value & 0xffUL;
        value >>= 8;
    }
    return bytes + 4;
}

static uint32_t toID3v2TagId(char const *s) {
    unsigned int i, x = 0;

    if(s == 0) {
        return 0;
    }
    for(i = 0; i < 4 && s[i] != 0; ++i) {
        char const c = s[i];
        unsigned int const u = 0x0ff & c;
        x <<= 8;
        x |= u;
        if(c < 'A' || 'Z' < c) {
            if(c < '0' || '9' < c) {
                return 0;
            }
        }
    }
    return x;
	}

static uint32_t toID3v2TagId_ucs2(unsigned short const *s) {
    unsigned int i, x = 0;
    unsigned short bom = 0;

    if(s == 0) {
        return 0;
    }
    bom = s[0];
    if(hasUcs2ByteOrderMarker(bom)) {
        ++s;
    }
    for(i = 0; i < 4 && s[i] != 0; ++i) {
        unsigned short const c = toLittleEndian(bom, s[i]);
        if(c < 'A' || 'Z' < c) {
            if(c < '0' || '9' < c) {
                return 0;
            }
        }
        x <<= 8;
        x |= c;
    }
    return x;
	}

#if 0
static int isNumericString(uint32_t frame_id) {

    switch(frame_id) {
    case ID_DATE:
    case ID_TIME:
    case ID_TPOS:
    case ID_TRACK:
    case ID_YEAR:
        return 1;
    }
    return 0;
}
#endif

static int isMultiFrame(uint32_t frame_id) {

    switch(frame_id) {
    case ID_TXXX:
    case ID_WXXX:
    case ID_COMMENT:
    case ID_SYLT:
    case ID_APIC:
    case ID_GEOB:
    case ID_PCNT:
    case ID_AENC:
    case ID_LINK:
    case ID_ENCR:
    case ID_GRID:
    case ID_PRIV:
        return 1;
    }
    return 0;
	}

#if 0
static int isFullTextString(int frame_id) {

    switch(frame_id) {
    case ID_VSLT:
    case ID_COMMENT:
        return 1;
    }
    return 0;
	}
#endif

static FrameDataNode *findNode(id3tag_spec const *tag, uint32_t frame_id, FrameDataNode const *last) {
    FrameDataNode *node = last ? last->nxt : tag->v2_head;

    while(node) {
        if(node->fid == frame_id) {
            return node;
        }
        node = node->nxt;
			}
    return 0;
	}

static void appendNode(id3tag_spec * tag, FrameDataNode * node) {

    if(tag->v2_tail == 0 || tag->v2_head == 0) {
        tag->v2_head = node;
        tag->v2_tail = node;
    }
    else {
        tag->v2_tail->nxt = node;
        tag->v2_tail = node;
    }
}

static void setLang(char *dst, char const *src) {
    int     i;

    if(src == 0 || src[0] == 0) {
        dst[0] = 'e';
        dst[1] = 'n';
        dst[2] = 'g';
    }
    else {
        for(i = 0; i < 3 && src && *src; ++i) {
            dst[i] = src[i];
        }
        for(; i < 3; ++i) {
            dst[i] = ' ';
        }
    }
}

static int isSameLang(char const *l1, char const *l2) {
    char    d[3];
    int     i;

    setLang(d, l2);
    for(i = 0; i < 3; ++i) {
        char    a = tolower(l1[i]);
        char    b = tolower(d[i]);
        if(a < ' ')
            a = ' ';
        if(b < ' ')
            b = ' ';
        if(a != b) {
            return 0;
        }
    }
    return 1;
	}

static int isSameDescriptor(FrameDataNode const *node, char const *dsc) {
    size_t  i;

    if(node->dsc.enc == 1 && node->dsc.dim > 0) {
        return 0;
    }
    for(i = 0; i < node->dsc.dim; ++i) {
        if(!dsc || node->dsc.ptr.l[i] != dsc[i]) {
            return 0;
        }
    }
    return 1;
	}

static int isSameDescriptorUcs2(FrameDataNode const *node, unsigned short const *dsc) {
    size_t  i;

    if(node->dsc.enc != 1 && node->dsc.dim > 0) {
        return 0;
    }
    for(i = 0; i < node->dsc.dim; ++i) {
        if(!dsc || node->dsc.ptr.u[i] != dsc[i]) {
            return 0;
        }
    }
    return 1;
	}

static int id3v2_add_ucs2(lame_t gfp, uint32_t frame_id, char const *lng, unsigned short const *desc, unsigned short const *text) {
    lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;

    if(gfc) {
        FrameDataNode *node = findNode(&gfc->tag_spec, frame_id, 0);
        char lang[4];
        setLang(lang, lng);
        if(isMultiFrame(frame_id)) {
            while(node) {
                if(isSameLang(node->lng, lang)) {
                    if(isSameDescriptorUcs2(node, desc)) {
                        break;
                    }
                }
                node = findNode(&gfc->tag_spec, frame_id, node);
            }
        }
        if(node == 0) {
            node = lame_calloc(FrameDataNode, 1);
            if(node == 0) {
                return -254; /* memory problem */
            }
            appendNode(&gfc->tag_spec, node);
        }
        node->fid = frame_id;
        setLang(node->lng, lang);
        node->dsc.dim = local_ucs2_strdup(&node->dsc.ptr.u, desc);
        node->dsc.enc = 1;
        node->txt.dim = local_ucs2_strdup(&node->txt.ptr.u, text);
        node->txt.enc = 1;
        gfc->tag_spec.flags |= (CHANGED_FLAG | ADD_V2_FLAG);
        return 0;
    }

  return -255;
	}

static int id3v2_add_latin1(lame_t gfp, uint32_t frame_id, char const *lng, char const *desc, char const *text) {
    lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;

    if(gfc) {
        FrameDataNode *node = findNode(&gfc->tag_spec, frame_id, 0);
        char lang[4];
        setLang(lang, lng);
        if(isMultiFrame(frame_id)) {
            while(node) {
                if(isSameLang(node->lng, lang)) {
                    if(isSameDescriptor(node, desc)) {
                        break;
                    }
                }
                node = findNode(&gfc->tag_spec, frame_id, node);
            }
        }
        if(node == 0) {
            node = lame_calloc(FrameDataNode, 1);
            if(node == 0) {
                return -254; /* memory problem */
            }
            appendNode(&gfc->tag_spec, node);
        }
        node->fid = frame_id;
        setLang(node->lng, lang);
        node->dsc.dim = local_strdup(&node->dsc.ptr.l, desc);
        node->dsc.enc = 0;
        node->txt.dim = local_strdup(&node->txt.ptr.l, text);
        node->txt.enc = 0;
        gfc->tag_spec.flags |= (CHANGED_FLAG | ADD_V2_FLAG);
        return 0;
    }
  return -255;
	}

static char const *id3v2_get_language(lame_t gfp) {
    lame_internal_flags const* gfc = gfp ? gfp->internal_flags : 0;

    if(gfc) 
			return gfc->tag_spec.language;
    return 0;
	}

static int id3v2_add_ucs2_lng(lame_t gfp, uint32_t frame_id, unsigned short const *desc, unsigned short const *text) {
    char const* lang = id3v2_get_language(gfp);

    return id3v2_add_ucs2(gfp, frame_id, lang, desc, text);
	}

static int id3v2_add_latin1_lng(lame_t gfp, uint32_t frame_id, char const *desc, char const *text) {
    char const* lang = id3v2_get_language(gfp);

    return id3v2_add_latin1(gfp, frame_id, lang, desc, text);
	}

static int id3tag_set_userinfo_latin1(lame_t gfp, uint32_t id, char const *fieldvalue) {
    char const separator = '=';
    int     rc = -7;
    int     a = local_char_pos(fieldvalue, separator);

    if(a >= 0) {
        char*   dup = 0;
        local_strdup(&dup, fieldvalue);
        dup[a] = 0;
        rc = id3v2_add_latin1_lng(gfp, id, dup, dup+a+1);
        free(dup);
    }
    return rc;
	}

static int id3tag_set_userinfo_ucs2(lame_t gfp, uint32_t id, unsigned short const *fieldvalue) {
    unsigned short const separator = fromLatin1Char(fieldvalue,'=');
    int     rc = -7;
    size_t  b = local_ucs2_strlen(fieldvalue);
    int     a = local_ucs2_pos(fieldvalue, separator);

    if(a >= 0) {
        unsigned short* dsc = 0, *val = 0;
        local_ucs2_substr(&dsc, fieldvalue, 0, a);
        local_ucs2_substr(&val, fieldvalue, a+1, b);
        rc = id3v2_add_ucs2_lng(gfp, id, dsc, val);
        free(dsc);
        free(val);
    }
  return rc;
	}

int id3tag_set_textinfo_utf16(lame_t gfp, char const *id, unsigned short const *text) {
    uint32_t const frame_id = toID3v2TagId(id);

    if(frame_id == 0) {
        return -1;
    }
    if(is_lame_internal_flags_null(gfp)) {
        return 0;
    }
    if(text == 0) {
        return 0;
    }
    if(!hasUcs2ByteOrderMarker(text[0])) {
        return -3;  /* BOM missing */
    }
    if(frame_id == ID_TXXX || frame_id == ID_WXXX || frame_id == ID_COMMENT) {
        return id3tag_set_userinfo_ucs2(gfp, frame_id, text);
    }
    if(frame_id == ID_GENRE) {
        return id3tag_set_genre_utf16(gfp, text);
    }
    if(frame_id == ID_PCST) {
        return id3v2_add_ucs2_lng(gfp, frame_id, 0, text);
    }
    if(frame_id == ID_USER) {
        return id3v2_add_ucs2_lng(gfp, frame_id, text, 0);
    }
    if(frame_id == ID_WFED) {
        return id3v2_add_ucs2_lng(gfp, frame_id, text, 0); /* iTunes expects WFED to be a text frame */
    }
    if(isFrameIdMatching(frame_id, FRAME_ID('T', 0, 0, 0))
      ||isFrameIdMatching(frame_id, FRAME_ID('W', 0, 0, 0))) {
#if 0
        if(isNumericString(frame_id)) {
            return -2;  /* must be Latin-1 encoded */
        }
#endif
        return id3v2_add_ucs2_lng(gfp, frame_id, 0, text);
    }
    return -255;        /* not supported by now */
}

extern int id3tag_set_textinfo_ucs2(lame_t gfp, char const *id, unsigned short const *text);

int id3tag_set_textinfo_ucs2(lame_t gfp, char const *id, unsigned short const *text) {
    return id3tag_set_textinfo_utf16(gfp, id, text);
}

int id3tag_set_textinfo_latin1(lame_t gfp, char const *id, char const *text) {
    uint32_t const frame_id = toID3v2TagId(id);

    if(frame_id == 0) {
        return -1;
    }
    if(is_lame_internal_flags_null(gfp)) {
        return 0;
    }
    if(text == 0) {
        return 0;
    }
    if(frame_id == ID_TXXX || frame_id == ID_WXXX || frame_id == ID_COMMENT) {
        return id3tag_set_userinfo_latin1(gfp, frame_id, text);
    }
    if(frame_id == ID_GENRE) {
        return id3tag_set_genre(gfp, text);
    }
    if(frame_id == ID_PCST) {
        return id3v2_add_latin1_lng(gfp, frame_id, 0, text);
    }
    if(frame_id == ID_USER) {
        return id3v2_add_latin1_lng(gfp, frame_id, text, 0);
    }
    if(frame_id == ID_WFED) {
        return id3v2_add_latin1_lng(gfp, frame_id, text, 0); /* iTunes expects WFED to be a text frame */
    }
    if(isFrameIdMatching(frame_id, FRAME_ID('T', 0, 0, 0))
      ||isFrameIdMatching(frame_id, FRAME_ID('W', 0, 0, 0))) {
        return id3v2_add_latin1_lng(gfp, frame_id, 0, text);
    }
    return -255;        /* not supported by now */
}


int id3tag_set_comment_latin1(lame_t gfp, char const *lang, char const *desc, char const *text) {

    if(is_lame_internal_flags_null(gfp)) {
        return 0;
    }
    return id3v2_add_latin1(gfp, ID_COMMENT, lang, desc, text);
}


int id3tag_set_comment_utf16(lame_t gfp, char const *lang, unsigned short const *desc, unsigned short const *text) {

    if(is_lame_internal_flags_null(gfp)) {
        return 0;
    }
    return id3v2_add_ucs2(gfp, ID_COMMENT, lang, desc, text);
}

extern int id3tag_set_comment_ucs2(lame_t gfp, char const *lang, unsigned short const *desc, unsigned short const *text);


int id3tag_set_comment_ucs2(lame_t gfp, char const *lang, unsigned short const *desc, unsigned short const *text) {

    if(is_lame_internal_flags_null(gfp)) {
        return 0;
    }
    return id3tag_set_comment_utf16(gfp, lang, desc, text);
}


void id3tag_set_title(lame_t gfp, const char *title) {
    lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;

    if(gfc && title && *title) {
        local_strdup(&gfc->tag_spec.title, title);
        gfc->tag_spec.flags |= CHANGED_FLAG;
        copyV1ToV2(gfp, ID_TITLE, title);
    }
}

void id3tag_set_artist(lame_t gfp, const char *artist) {
    lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;

    if(gfc && artist && *artist) {
        local_strdup(&gfc->tag_spec.artist, artist);
        gfc->tag_spec.flags |= CHANGED_FLAG;
        copyV1ToV2(gfp, ID_ARTIST, artist);
    }
}

void id3tag_set_album(lame_t gfp, const char *album) {
    lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;

    if(gfc && album && *album) {
        local_strdup(&gfc->tag_spec.album, album);
        gfc->tag_spec.flags |= CHANGED_FLAG;
        copyV1ToV2(gfp, ID_ALBUM, album);
    }
	}

void id3tag_set_year(lame_t gfp, const char *year) {
    lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;

    if(gfc && year && *year) {
        int     num = atoi(year);
        if(num < 0) {
            num = 0;
        }
        /* limit a year to 4 digits so it fits in a version 1 tag */
        if(num > 9999) {
            num = 9999;
        }
        if(num) {
            gfc->tag_spec.year = num;
            gfc->tag_spec.flags |= CHANGED_FLAG;
        }
        copyV1ToV2(gfp, ID_YEAR, year);
    }
	}

void id3tag_set_comment(lame_t gfp, const char *comment) {
    lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;

    if(gfc && comment && *comment) {
        local_strdup(&gfc->tag_spec.comment, comment);
        gfc->tag_spec.flags |= CHANGED_FLAG;
        {
            uint32_t const flags = gfc->tag_spec.flags;
            id3v2_add_latin1_lng(gfp, ID_COMMENT, "", comment);
            gfc->tag_spec.flags = flags;
        }
    }
	}

int id3tag_set_track(lame_t gfp, const char *track) {
    char const *trackcount;
    lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;
    int     ret = 0;

    if(gfc && track && *track) {
        int     num = atoi(track);
        /* check for valid ID3v1 track number range */
        if(num < 1 || num > 255) {
            num = 0;
            ret = -1;   /* track number out of ID3v1 range, ignored for ID3v1 */
            gfc->tag_spec.flags |= (CHANGED_FLAG | ADD_V2_FLAG);
        }
        if(num) {
            gfc->tag_spec.track_id3v1 = num;
            gfc->tag_spec.flags |= CHANGED_FLAG;
        }
        /* Look for the total track count after a "/", same restrictions */
        trackcount = strchr(track, '/');
        if(trackcount && *trackcount) {
            gfc->tag_spec.flags |= (CHANGED_FLAG | ADD_V2_FLAG);
        }
        copyV1ToV2(gfp, ID_TRACK, track);
    }

  return ret;
	}

/* would use real "strcasecmp" but it isn't portable */
static int local_strcasecmp(const char *s1, const char *s2) {
    unsigned char c1;
    unsigned char c2;

    do {
        c1 = tolower(*s1);
        c2 = tolower(*s2);
        if(!c1) {
            break;
        }
        ++s1;
        ++s2;
    } while(c1 == c2);

  return c1 - c2;
	}


static const char* nextUpperAlpha(const char* p, char x) {
    char c;

    for(c = toupper(*p); *p != 0; c = toupper(*++p)) {
        if('A' <= c && c <= 'Z') {
            if(c != x) {
                return p;
            }
        }
    }
    return p;
}


static int sloppyCompared(const char* p, const char* q) {
    char cp, cq;

    p = nextUpperAlpha(p, 0);
    q = nextUpperAlpha(q, 0);
    cp = toupper(*p);
    cq = toupper(*q);
    while(cp == cq) {
        if(cp == 0) {
            return 1;
        }
        if(p[1] == '.') { /* some abbrevation */
            while(*q && *q++ != ' ') {
            }
        }
        p = nextUpperAlpha(p, cp);
        q = nextUpperAlpha(q, cq);
        cp = toupper(*p);
        cq = toupper(*q);
    }
    return 0;
}


static int sloppySearchGenre(const char *genre) {
    int i;

    for(i = 0; i < GENRE_NAME_COUNT; ++i) {
        if(sloppyCompared(genre, genre_names[i])) {
            return i;
        }
    }
    return GENRE_NAME_COUNT;
}


static int searchGenre(const char* genre) {
    int i;

    for(i = 0; i < GENRE_NAME_COUNT; ++i) {
        if(!local_strcasecmp(genre, genre_names[i])) {
            return i;
        }
    }

  return GENRE_NAME_COUNT;
	}


int id3tag_set_genre(lame_t gfp, const char *genre) {
	lame_internal_flags *gfc = gfp != 0 ? gfp->internal_flags : 0;
  int     ret = 0;

    if(gfc && genre && *genre) {
        int const num = lookupGenre(genre);
        if(num == -1) return num;
        gfc->tag_spec.flags |= CHANGED_FLAG;
        if(num >= 0) {
            gfc->tag_spec.genre_id3v1 = num;
            genre = genre_names[num];
        }
        else {
            gfc->tag_spec.genre_id3v1 = GENRE_INDEX_OTHER;
            gfc->tag_spec.flags |= ADD_V2_FLAG;
        }
        copyV1ToV2(gfp, ID_GENRE, genre);
    }

  return ret;
	}


static size_t sizeOfNode(FrameDataNode const *node) {
    size_t  n = 0;

    if(node) {
        n = 10;         /* header size */
        n += 1;         /* text encoding flag */
        switch(node->txt.enc) {
        default:
        case 0:
            if(node->dsc.dim > 0) {
                n += node->dsc.dim + 1;
            }
            n += node->txt.dim;
            break;
        case 1:
            if(node->dsc.dim > 0) {
                n += (node->dsc.dim+1) * 2;
            }
            n += node->txt.dim * 2;
            break;
        }
    }
    return n;
	}

static size_t sizeOfCommentNode(FrameDataNode const *node) {
    size_t  n = 0;

    if(node) {
        n = 10;         /* header size */
        n += 1;         /* text encoding flag */
        n += 3;         /* language */
        switch(node->dsc.enc) {
        default:
        case 0:
            n += 1 + node->dsc.dim;
            break;
        case 1:
            n += 2 + node->dsc.dim * 2;
            break;
        }
        switch(node->txt.enc) {
        default:
        case 0:
            n += node->txt.dim;
            break;
        case 1:
            n += node->txt.dim * 2;
            break;
        }
    }

  return n;
	}

static size_t sizeOfWxxxNode(FrameDataNode const *node) {
    size_t  n = 0;

    if(node) {
        n = 10;         /* header size */
        if(node->dsc.dim > 0) {
            n += 1;         /* text encoding flag */
            switch(node->dsc.enc) {
            default:
            case 0:
                n += 1 + node->dsc.dim;
                break;
            case 1:
                n += 2 + node->dsc.dim * 2;
                break;
            }
        }
        if(node->txt.dim > 0) {
            switch(node->txt.enc) {
            default:
            case 0:
                n += node->txt.dim;
                break;
            case 1:
                n += node->txt.dim - 1; /* UCS2 -> Latin1, skip BOM */
                break;
            }
        }
    }

  return n;
	}

static unsigned char *writeChars(unsigned char *frame, char const *str, size_t n) {

    while(n--) {
        *frame++ = *str++;
    }

  return frame;
	}

static unsigned char *writeUcs2s(unsigned char *frame, unsigned short const *str, size_t n) {

    if(n > 0) {
        unsigned short const bom = *str;
        while(n--) {
            unsigned short const c = toLittleEndian(bom, *str++);
            *frame++ = 0x00ffu & c;
            *frame++ = 0x00ffu & (c >> 8);
        }
    }
    return frame;
}

static unsigned char *writeLoBytes(unsigned char *frame, unsigned short const *str, size_t n) {

    if(n > 0) {
        unsigned short const bom = *str;
        if(hasUcs2ByteOrderMarker(bom)) {
            str++; n--; /* skip BOM */
        }
        while(n--) {
            unsigned short const c = toLittleEndian(bom, *str++);
            if(c < 0x0020u || 0x00ffu < c) {
                *frame++ = 0x0020; /* blank */
            }
            else {
                *frame++ = c;
            }
        }
    }

  return frame;
	}

static unsigned char *set_frame_comment(unsigned char *frame, FrameDataNode const *node) {
    size_t const n = sizeOfCommentNode(node);

    if(n > 10) {
        frame = set_4_byte_value(frame, node->fid);
        frame = set_4_byte_value(frame, (uint32_t) (n - 10));
        /* clear 2-byte header flags */
        *frame++ = 0;
        *frame++ = 0;
        /* encoding descriptor byte */
        *frame++ = node->txt.enc == 1 ? 1 : 0;
        /* 3 bytes language */
        *frame++ = node->lng[0];
        *frame++ = node->lng[1];
        *frame++ = node->lng[2];
        /* descriptor with zero byte(s) separator */
        if(node->dsc.enc != 1) {
            frame = writeChars(frame, node->dsc.ptr.l, node->dsc.dim);
            *frame++ = 0;
        }
        else {
            frame = writeUcs2s(frame, node->dsc.ptr.u, node->dsc.dim);
            *frame++ = 0;
            *frame++ = 0;
        }
        /* comment full text */
        if(node->txt.enc != 1) {
            frame = writeChars(frame, node->txt.ptr.l, node->txt.dim);
        }
        else {
            frame = writeUcs2s(frame, node->txt.ptr.u, node->txt.dim);
        }
    }
    return frame;
}

static unsigned char *set_frame_custom2(unsigned char *frame, FrameDataNode const *node) {
    size_t const n = sizeOfNode(node);

    if(n > 10) {
        frame = set_4_byte_value(frame, node->fid);
        frame = set_4_byte_value(frame, (unsigned long) (n - 10));
        /* clear 2-byte header flags */
        *frame++ = 0;
        *frame++ = 0;
        /* clear 1 encoding descriptor byte to indicate ISO-8859-1 format */
        *frame++ = node->txt.enc == 1 ? 1 : 0;
        if(node->dsc.dim > 0) {
            if(node->dsc.enc != 1) {
                frame = writeChars(frame, node->dsc.ptr.l, node->dsc.dim);
                *frame++ = 0;
            }
            else {
                frame = writeUcs2s(frame, node->dsc.ptr.u, node->dsc.dim);
                *frame++ = 0;
                *frame++ = 0;
            }
        }
        if(node->txt.enc != 1) {
            frame = writeChars(frame, node->txt.ptr.l, node->txt.dim);
        }
        else {
            frame = writeUcs2s(frame, node->txt.ptr.u, node->txt.dim);
        }
    }

  return frame;
	}

static unsigned char *set_frame_wxxx(unsigned char *frame, FrameDataNode const *node) {
    size_t const n = sizeOfWxxxNode(node);

    if(n > 10) {
        frame = set_4_byte_value(frame, node->fid);
        frame = set_4_byte_value(frame, (unsigned long) (n - 10));
        /* clear 2-byte header flags */
        *frame++ = 0;
        *frame++ = 0;
        if(node->dsc.dim > 0) {
            /* clear 1 encoding descriptor byte to indicate ISO-8859-1 format */
            *frame++ = node->dsc.enc == 1 ? 1 : 0;
            if(node->dsc.enc != 1) {
                frame = writeChars(frame, node->dsc.ptr.l, node->dsc.dim);
                *frame++ = 0;
            }
            else {
                frame = writeUcs2s(frame, node->dsc.ptr.u, node->dsc.dim);
                *frame++ = 0;
                *frame++ = 0;
            }
        }
        if(node->txt.enc != 1) {
            frame = writeChars(frame, node->txt.ptr.l, node->txt.dim);
        }
        else {
            frame = writeLoBytes(frame, node->txt.ptr.u, node->txt.dim);
        }
    }

  return frame;
	}

static unsigned char *set_frame_apic(unsigned char *frame, const char *mimetype, const unsigned char *data, size_t size) {

    /* ID3v2.3 standard APIC frame:
     *     <Header for 'Attached picture', ID: "APIC">
     *     Text encoding    $xx
     *     MIME type        <text string> $00
     *     Picture type     $xx
     *     Description      <text string according to encoding> $00 (00)
     *     Picture data     <binary data>
     */
    if(mimetype && data && size) {
        frame = set_4_byte_value(frame, FRAME_ID('A', 'P', 'I', 'C'));
        frame = set_4_byte_value(frame, (unsigned long) (4 + strlen(mimetype) + size));
        /* clear 2-byte header flags */
        *frame++ = 0;
        *frame++ = 0;
        /* clear 1 encoding descriptor byte to indicate ISO-8859-1 format */
        *frame++ = 0;
        /* copy mime_type */
        while(*mimetype) {
            *frame++ = *mimetype++;
        }
        *frame++ = 0;
        /* set picture type to 0 */
        *frame++ = 0;
        /* empty description field */
        *frame++ = 0;
        /* copy the image data */
        while(size--) {
            *frame++ = *data++;
        }
    }

  return frame;
	}

int id3tag_set_fieldvalue(lame_t gfp, const char *fieldvalue) {

    if(is_lame_internal_flags_null(gfp)) {
        return 0;
    }
    if(fieldvalue && *fieldvalue) {
        if(strlen(fieldvalue) < 5 || fieldvalue[4] != '=') {
            return -1;
        }
        return id3tag_set_textinfo_latin1(gfp, fieldvalue, &fieldvalue[5]);
    }

  return 0;
	}

int id3tag_set_fieldvalue_utf16(lame_t gfp, const unsigned short *fieldvalue) {

    if(is_lame_internal_flags_null(gfp)) {
        return 0;
    }
    if(fieldvalue && *fieldvalue) {
        size_t dx = hasUcs2ByteOrderMarker(fieldvalue[0]);
        unsigned short const separator = fromLatin1Char(fieldvalue, '=');
        char fid[5] = {0,0,0,0,0};
        uint32_t const frame_id = toID3v2TagId_ucs2(fieldvalue);
        if(local_ucs2_strlen(fieldvalue) < (5+dx) || fieldvalue[4+dx] != separator) {
            return -1;
        }
        fid[0] = (frame_id >> 24) & 0x0ff;
        fid[1] = (frame_id >> 16) & 0x0ff;
        fid[2] = (frame_id >> 8) & 0x0ff;
        fid[3] = frame_id & 0x0ff;
        if(frame_id != 0) {
            unsigned short* txt = 0;
            int     rc;
            local_ucs2_substr(&txt, fieldvalue, dx+5, local_ucs2_strlen(fieldvalue));
            rc = id3tag_set_textinfo_utf16(gfp, fid, txt);
            free(txt);
            return rc;
        }
    }

  return -1;
	}

extern int id3tag_set_fieldvalue_ucs2(lame_t gfp, const unsigned short *fieldvalue);

int id3tag_set_fieldvalue_ucs2(lame_t gfp, const unsigned short *fieldvalue) {

    if(is_lame_internal_flags_null(gfp)) {
        return 0;
    }

  return id3tag_set_fieldvalue_utf16(gfp, fieldvalue);
	}

size_t lame_get_id3v2_tag(lame_t gfp, unsigned char *buffer, size_t size) {
  lame_internal_flags *gfc = 0;

  if(is_lame_internal_flags_null(gfp)) {
    return 0;
    }
  gfc = gfp->internal_flags;
  if(test_tag_spec_flags(gfc, V1_ONLY_FLAG)) {
    return 0;
    }
#if 0
    debug_tag_spec_flags(gfc, "lame_get_id3v2_tag");
#endif
    {
      int usev2 = test_tag_spec_flags(gfc, ADD_V2_FLAG | V2_ONLY_FLAG);
      /* calculate length of four fields which may not fit in verion 1 tag */
      size_t  title_length = gfc->tag_spec.title ? strlen(gfc->tag_spec.title) : 0;
      size_t  artist_length = gfc->tag_spec.artist ? strlen(gfc->tag_spec.artist) : 0;
      size_t  album_length = gfc->tag_spec.album ? strlen(gfc->tag_spec.album) : 0;
      size_t  comment_length = gfc->tag_spec.comment ? strlen(gfc->tag_spec.comment) : 0;
      /* write tag if explicitly requested or if fields overflow */
      if((title_length > 30) || (artist_length > 30)
        || (album_length > 30) || (comment_length > 30)
        || (gfc->tag_spec.track_id3v1 && (comment_length > 28))) {
        usev2 = 1;
        }
      if(usev2) {
        size_t  tag_size;
        unsigned char *p;
        size_t  adjusted_tag_size;
        const char *albumart_mime = NULL;
        static const char *mime_jpeg = "image/jpeg";
        static const char *mime_png = "image/png";
        static const char *mime_gif = "image/gif";

        if(gfp->num_samples != MAX_U_32_NUM) {
          id3v2AddAudioDuration(gfp, gfp->num_samples);
          }

        /* calulate size of tag starting with 10-byte tag header */
        tag_size = 10;
        if(gfc->tag_spec.albumart && gfc->tag_spec.albumart_size) {
            switch(gfc->tag_spec.albumart_mimetype) {
            case MIMETYPE_JPEG:
                albumart_mime = mime_jpeg;
                break;
            case MIMETYPE_PNG:
                albumart_mime = mime_png;
                break;
            case MIMETYPE_GIF:
                albumart_mime = mime_gif;
                break;
              }
            if(albumart_mime) {
                tag_size += 10 + 4 + strlen(albumart_mime) + gfc->tag_spec.albumart_size;
              }
            }
            {
              id3tag_spec *tag = &gfc->tag_spec;
              if(tag->v2_head != 0) {
                FrameDataNode *node;
                for(node = tag->v2_head; node != 0; node = node->nxt) {
                  if(node->fid == ID_COMMENT || node->fid == ID_USER) {
                    tag_size += sizeOfCommentNode(node);
                    }
                  else if(isFrameIdMatching(node->fid, FRAME_ID('W',0,0,0))) {
                    tag_size += sizeOfWxxxNode(node);
                    }
                  else {
                    tag_size += sizeOfNode(node);
                    }
                  }
                }
            }
            if(test_tag_spec_flags(gfc, PAD_V2_FLAG)) {
              // add some bytes of padding
              tag_size += gfc->tag_spec.padding_size;
		          }
            if(size < tag_size) {
              return tag_size;
	            }
            if(!buffer) {
              return 0;
							}
            p = buffer;
            /* set tag header starting with file identifier */
            *p++ = 'I';
            *p++ = 'D';
            *p++ = '3';
            /* set version number word */
            *p++ = 3;
            *p++ = 0;
            /* clear flags byte */
            *p++ = 0;
            /* calculate and set tag size = total size - header size */
            adjusted_tag_size = tag_size - 10;
            /* encode adjusted size into four bytes where most significant 
             * bit is clear in each byte, for 28-bit total */
            *p++ = (unsigned char) ((adjusted_tag_size >> 21) & 0x7fu);
            *p++ = (unsigned char) ((adjusted_tag_size >> 14) & 0x7fu);
            *p++ = (unsigned char) ((adjusted_tag_size >> 7) & 0x7fu);
            *p++ = (unsigned char) (adjusted_tag_size & 0x7fu);

            /*
             * NOTE: The remainder of the tag (frames and padding, if any)
             * are not "unsynchronized" to prevent false MPEG audio headers
             * from appearing in the bitstream.  Why?  Well, most players
             * and utilities know how to skip the ID3 version 2 tag by now
             * even if they don't read its contents, and it's actually
             * very unlikely that such a false "sync" pattern would occur
             * in just the simple text frames added here.
             */

            /* set each frame in tag */
            {
              id3tag_spec *tag = &gfc->tag_spec;
              if(tag->v2_head != 0) {
                  FrameDataNode *node;
                  for(node = tag->v2_head; node != 0; node = node->nxt) {
                      if(node->fid == ID_COMMENT || node->fid == ID_USER) {
                          p = set_frame_comment(p, node);
                      }
                      else if(isFrameIdMatching(node->fid,FRAME_ID('W',0,0,0))) {
                          p = set_frame_wxxx(p, node);
                      }
                      else {
                          p = set_frame_custom2(p, node);
                      }
                  }
                }
            }
        if(albumart_mime) {
          p = set_frame_apic(p, albumart_mime, gfc->tag_spec.albumart,
                             gfc->tag_spec.albumart_size);
          }
      /* clear any padding bytes */
      memset(p, 0, tag_size - (p - buffer));
      return tag_size;
      }
    }
  return 0;
	}

int id3tag_write_v2(lame_t gfp) {
  lame_internal_flags *gfc = 0;

  if(is_lame_internal_flags_null(gfp)) {
    return 0;
    }
  gfc = gfp->internal_flags;
#if 0
    debug_tag_spec_flags(gfc, "write v2");
#endif
  if(test_tag_spec_flags(gfc, V1_ONLY_FLAG)) {
    return 0;
    }
  if(test_tag_spec_flags(gfc, CHANGED_FLAG)) {
      unsigned char *tag = 0;
      size_t  tag_size, n;

      n = lame_get_id3v2_tag(gfp, 0, 0);
      tag = lame_calloc(unsigned char, n);
    if(tag == 0) {
      return -1;
      }
    tag_size = lame_get_id3v2_tag(gfp, tag, n);
    if(tag_size > n) {
      free(tag);
      return -1;
      }
    else {
      size_t  i;
      /* write tag directly into bitstream at current position */
      for(i=0; i < tag_size; ++i) {
        add_dummy_byte(gfc, tag[i], 1);
        }
      }
    free(tag);
    return (int)tag_size; /* ok, tag should not exceed 2GB */
    }

  return 0;
	}

static unsigned char *set_text_field(unsigned char *field, const char *text, size_t size, int pad) {

  while(size--) {
    if(text && *text) {
      *field++ = *text++;
      }
    else {
      *field++ = pad;
      }
    }

  return field;
	}

size_t lame_get_id3v1_tag(lame_t gfp, unsigned char *buffer, size_t size) {
  size_t const tag_size = 128;
  lame_internal_flags *gfc;

  if(gfp == 0) {
      return 0;
    }
  if(size < tag_size) {
      return tag_size;
    }
  gfc = gfp->internal_flags;
  if(gfc == 0) {
      return 0;
    }
  if(buffer == 0) {
      return 0;
    }
	if(test_tag_spec_flags(gfc, V2_ONLY_FLAG)) {
			return 0;
    }
    if(test_tag_spec_flags(gfc, CHANGED_FLAG)) {
        unsigned char *p = buffer;
        int     pad = test_tag_spec_flags(gfc, SPACE_V1_FLAG) ? ' ' : 0;
        char    year[5];

        /* set tag identifier */
        *p++ = 'T';
        *p++ = 'A';
        *p++ = 'G';
        /* set each field in tag */
        p = set_text_field(p, gfc->tag_spec.title, 30, pad);
        p = set_text_field(p, gfc->tag_spec.artist, 30, pad);
        p = set_text_field(p, gfc->tag_spec.album, 30, pad);
        sprintf(year, "%d", gfc->tag_spec.year);
        p = set_text_field(p, gfc->tag_spec.year ? year : NULL, 4, pad);
        /* limit comment field to 28 bytes if a track is specified */
        p = set_text_field(p, gfc->tag_spec.comment, gfc->tag_spec.track_id3v1 ? 28 : 30, pad);
        if(gfc->tag_spec.track_id3v1) {
            /* clear the next byte to indicate a version 1.1 tag */
            *p++ = 0;
            *p++ = gfc->tag_spec.track_id3v1;
        }
        *p++ = gfc->tag_spec.genre_id3v1;
        return tag_size;
    }

  return 0;
	}

int id3tag_write_v1(lame_t gfp) {
  lame_internal_flags* gfc = 0;
  size_t  i, n, m;
  unsigned char tag[128];

  if(is_lame_internal_flags_null(gfp)) {
      return 0;
    }
  gfc = gfp->internal_flags;

  m = sizeof(tag);
  n = lame_get_id3v1_tag(gfp, tag, m);
  if(n > m) {
      return 0;
    }
    /* write tag directly into bitstream at current position */
    for(i=0; i < n; ++i) {
        add_dummy_byte(gfc, tag[i], 1);
    }

  return (int) n;     /* ok, tag has fixed size of 128 bytes, well below 2GB */
	}


