/* qsv_common.h
 *
 * Copyright (c) 2003-2019 HandBrake Team
 * This file is part of the HandBrake source code.
 * Homepage: <http://handbrake.fr/>.
 * It may be used under the terms of the GNU General Public License v2.
 * For full terms see the file COPYING file or visit http://www.gnu.org/licenses/gpl-2.0.html
 */

#ifndef HANDBRAKE_QSV_COMMON_H
#define HANDBRAKE_QSV_COMMON_H

int            hb_qsv_available();

#include "handbrake/project.h"

#if HB_PROJECT_FEATURE_QSV

// Public API
int  hb_qsv_impl_set_preferred(const char *name);
void hb_qsv_force_workarounds(); // for developers only

#ifdef __LIBHB__
// Private API

#include "mfx/mfxvideo.h"
#include "mfx/mfxplugin.h"
#include "libavcodec/avcodec.h"
#include "handbrake/hb_dict.h"

/* Minimum Intel Media SDK version (currently 1.3, for Sandy Bridge support) */
#define HB_QSV_MINVERSION_MAJOR HB_QSV_MSDK_VERSION_MAJOR
#define HB_QSV_MINVERSION_MINOR HB_QSV_MSDK_VERSION_MINOR

/*
 * Get & store all available Intel Quick Sync information:
 *
 * - general availability
 * - available implementations (hardware-accelerated, software fallback, etc.)
 * - available codecs, filters, etc. for direct access (convenience)
 * - supported API version
 * - supported resolutions
 */
typedef struct hb_qsv_info_s
{
    // each info struct only corresponds to one CodecId and implementation combo
    const mfxU32  codec_id;
    mfxIMPL implementation;

    // whether the encoder is available for this implementation
    int available;

    // version-specific or hardware-specific capabilities
    uint64_t capabilities;
    // support for API 1.6 or later
#define HB_QSV_CAP_MSDK_API_1_6      (1LL <<  0)
    // H.264, H.265: B-frames can be used as references
#define HB_QSV_CAP_B_REF_PYRAMID     (1LL <<  1)
#define HB_QSV_CAP_LOWPOWER_ENCODE   (1LL <<  2)
    // mfxExtVideoSignalInfo
#define HB_QSV_CAP_VUI_VSINFO        (1LL <<  3)
    // optional rate control methods
#define HB_QSV_CAP_RATECONTROL_LA    (1LL << 10)
#define HB_QSV_CAP_RATECONTROL_LAi   (1LL << 11)
#define HB_QSV_CAP_RATECONTROL_ICQ   (1LL << 12)
    // mfxExtCodingOption
#define HB_QSV_CAP_OPTION1           (1LL << 20)
    // mfxExtCodingOption2
#define HB_QSV_CAP_OPTION2           (1LL << 30)
#define HB_QSV_CAP_OPTION2_MBBRC     (1LL << 31)
#define HB_QSV_CAP_OPTION2_EXTBRC    (1LL << 32)
#define HB_QSV_CAP_OPTION2_TRELLIS   (1LL << 33)
#define HB_QSV_CAP_OPTION2_BREFTYPE  (1LL << 34)
#define HB_QSV_CAP_OPTION2_IB_ADAPT  (1LL << 35)
#define HB_QSV_CAP_OPTION2_LA_DOWNS  (1LL << 36)
#define HB_QSV_CAP_OPTION2_NMPSLICE  (1LL << 37)

    // TODO: add maximum encode resolution, etc.
} hb_qsv_info_t;

/* Intel Quick Sync Video utilities */
hb_display_t * hb_qsv_display_init(void);
int            hb_qsv_video_encoder_is_enabled(int encoder);
int            hb_qsv_audio_encoder_is_enabled(int encoder);
int            hb_qsv_info_init();
void           hb_qsv_info_print();
hb_qsv_info_t* hb_qsv_info_get(int encoder);

/* Automatically load and unload any required MFX plug-ins */
hb_list_t* hb_qsv_load_plugins  (hb_qsv_info_t *info, mfxSession session, mfxVersion version);
void       hb_qsv_unload_plugins(hb_list_t     **_l,  mfxSession session, mfxVersion version);

/* Intel Quick Sync Video DECODE utilities */
const char* hb_qsv_decode_get_codec_name(enum AVCodecID codec_id);
int hb_qsv_decode_is_enabled(hb_job_t *job);

/*
 * mfxCoreInterface::CopyFrame had a bug preventing us from using it, but
 * it was fixed in newer drivers - we can use this to determine usability
 */
int hb_qsv_copyframe_is_slow(int encoder);

/* Media SDK parameters handling */
enum
{
    HB_QSV_PARAM_OK,
    HB_QSV_PARAM_ERROR,
    HB_QSV_PARAM_BAD_NAME,
    HB_QSV_PARAM_BAD_VALUE,
    HB_QSV_PARAM_UNSUPPORTED,
};

typedef struct
{
    /*
     * Supported mfxExtBuffer.BufferId values:
     *
     * MFX_EXTBUFF_AVC_REFLIST_CTRL
     * MFX_EXTBUFF_AVC_TEMPORAL_LAYERS
     * MFX_EXTBUFF_CODING_OPTION
     * MFX_EXTBUFF_CODING_OPTION_SPSPPS
     * MFX_EXTBUFF_CODING_OPTION2
     * MFX_EXTBUFF_ENCODER_CAPABILITY
     * MFX_EXTBUFF_ENCODER_RESET_OPTION
     * MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION
     * MFX_EXTBUFF_PICTURE_TIMING_SEI
     * MFX_EXTBUFF_VIDEO_SIGNAL_INFO
     *
     * This should cover all encode-compatible extended
     * buffers that can be attached to an mfxVideoParam.
     */
#define HB_QSV_ENC_NUM_EXT_PARAM_MAX 10
    mfxExtBuffer*         ExtParamArray[HB_QSV_ENC_NUM_EXT_PARAM_MAX];
    mfxExtCodingOption    codingOption;
    mfxExtCodingOption2   codingOption2;
    mfxExtVideoSignalInfo videoSignalInfo;
    struct
    {
        int b_pyramid;
        int gop_pic_size;
        int gop_ref_dist;
        int int_ref_cycle_size;
    } gop;
    struct
    {
        int   icq;
        int   lookahead;
        int   cqp_offsets[3];
        int   vbv_max_bitrate;
        int   vbv_buffer_size;
        float vbv_buffer_init;
    } rc;

    // assigned via hb_qsv_param_default, may be shared with another structure
    mfxVideoParam *videoParam;
} hb_qsv_param_t;

static const char* const hb_qsv_preset_names1[] = { "speed", "balanced",            NULL, };
static const char* const hb_qsv_preset_names2[] = { "speed", "balanced", "quality", NULL, };
const char* const* hb_qsv_preset_get_names();
const char* const* hb_qsv_profile_get_names(int encoder);
const char* const* hb_qsv_level_get_names(int encoder);

const char* hb_qsv_video_quality_get_name(uint32_t codec);
void hb_qsv_video_quality_get_limits(uint32_t codec, float *low, float *high, float *granularity, int *direction);

#define HB_QSV_CLIP3(min, max, val) ((val < min) ? min : (val > max) ? max : val)
int         hb_qsv_codingoption_xlat    (int val);
const char* hb_qsv_codingoption_get_name(int val);

int   hb_qsv_trellisvalue_xlat(int val);
int   hb_qsv_atoindex(const char* const *arr, const char *str, int *err);
int   hb_qsv_atobool (const char *str, int *err);
int   hb_qsv_atoi    (const char *str, int *err);
float hb_qsv_atof    (const char *str, int *err);

int hb_qsv_param_default_preset(hb_qsv_param_t *param, mfxVideoParam *videoParam, hb_qsv_info_t *info, const char *preset);
int hb_qsv_param_default       (hb_qsv_param_t *param, mfxVideoParam *videoParam, hb_qsv_info_t *info);
int hb_qsv_param_parse         (hb_qsv_param_t *param,                            hb_qsv_info_t *info, const char *key, const char *value);
int hb_qsv_profile_parse       (hb_qsv_param_t *param,                            hb_qsv_info_t *info, const char *profile_key, const int codec);
int hb_qsv_level_parse         (hb_qsv_param_t *param,                            hb_qsv_info_t *info, const char *level_key);

typedef struct
{
    const char *name;
    const char *key;
    const int value;
}
hb_triplet_t;

hb_triplet_t* hb_triplet4value(hb_triplet_t *triplets, const int  value);
hb_triplet_t* hb_triplet4name (hb_triplet_t *triplets, const char *name);
hb_triplet_t* hb_triplet4key  (hb_triplet_t *triplets, const char *key);

const char* hb_qsv_codec_name    (uint32_t codec_id);
const char* hb_qsv_profile_name  (uint32_t codec_id, uint16_t profile_id);
const char* hb_qsv_level_name    (uint32_t codec_id, uint16_t level_id);
const char* hb_qsv_frametype_name(uint16_t qsv_frametype);
uint8_t     hb_qsv_frametype_xlat(uint16_t qsv_frametype, uint16_t *out_flags);

const char* hb_qsv_impl_get_name(int impl);
const char* hb_qsv_impl_get_via_name(int impl);


typedef struct QSVMid {
    AVBufferRef *hw_frames_ref;
    mfxHDL handle;

    void *texture;

    AVFrame *locked_frame;
    AVFrame *hw_frame;
    mfxFrameSurface1 surf;
} QSVMid;

typedef struct QSVFrame {
    AVFrame *frame;
    mfxFrameSurface1 surface;
    mfxEncodeCtrl enc_ctrl;
    mfxExtDecodedFrameInfo dec_info;
    mfxExtBuffer *ext_param;

    int queued;
    int used;

    struct QSVFrame *next;
} QSVFrame;

// Large size is driven by hevc which has a maximum GOP size of 256
#define HB_POOL_SURFACE_SIZE (64)

typedef struct EncQSVFramesContext {
    AVBufferRef *hw_frames_ctx;
    AVBufferRef *hw_frames_ctx2;
    //void *logctx;

    /* The memory ids for the external frames.
     * Refcounted, since we need one reference owned by the QSVFramesContext
     * (i.e. by the encoder/decoder) and another one given to the MFX session
     * from the frame allocator. */
    AVBufferRef *mids_buf;
    AVBufferRef *mids_buf2;
    QSVMid *mids;
    QSVMid *mids2;
    int  nb_mids;
    int pool[HB_POOL_SURFACE_SIZE];
    int pool2[HB_POOL_SURFACE_SIZE];
    void *input_texture;
} EncQSVFramesContext;

/* Full QSV pipeline helpers */
int hb_qsv_full_path_is_enabled(hb_job_t *job);
AVBufferRef *hb_qsv_create_mids(AVBufferRef *hw_frames_ref);
hb_buffer_t* hb_qsv_copy_frame(AVFrame *frame, hb_qsv_context *qsv_ctx);
void hb_qsv_get_free_surface_from_pool(QSVMid **out_mid, mfxFrameSurface1 **out_surface, int pool_size);
int hb_qsv_get_buffer(AVCodecContext *s, AVFrame *frame, int flags);
enum AVPixelFormat hb_qsv_get_format(AVCodecContext *s, const enum AVPixelFormat *pix_fmts);
int hb_qsv_preset_is_zero_copy_enabled(const hb_dict_t *job_dict);
void hb_qsv_uninit_dec(AVCodecContext *s);
void hb_qsv_uninit_enc();

#endif // __LIBHB__
#endif // HB_PROJECT_FEATURE_QSV
#endif // HANDBRAKE_QSV_COMMON_H
