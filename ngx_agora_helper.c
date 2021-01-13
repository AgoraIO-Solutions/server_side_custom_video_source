#include "ngx_rtmp.h"
#include "ngx_rtmp_cmd_module.h"
#include "ngx_rtmp_netcall_module.h"
#include "ngx_rtmp_codec_module.h"
#include "ngx_rtmp_record_module.h"

#include "agorac.h"

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>

#include <opus/opus.h>

#define NGX_RTMP_HLS_BUFSIZE            (1024*1024)
#define MAX_AUDIO_BUFFER_SIZE           2*1024

#define INPUT_SAMPLE_COUNT               1024
#define OPUS_SAMPLE_COUNT                960
#define MAX_CHANNEL_COUNT                2

typedef struct 
{

  //AAC decoder context
   AVCodecContext *avContext;

   //rtmp buffer
   ngx_buf_t          *b;

   OpusEncoder        *opus_encoder;

   uint8_t            channel_count;
   uint32_t           sample_rate;
   uint32_t           bitrate;

   uint32_t           remaining_count;
   uint8_t            remaining_bytes[INPUT_SAMPLE_COUNT*sizeof(int16_t)*MAX_CHANNEL_COUNT];
} agora_audio_context_t;;

typedef struct
{
   agora_context_t        *agora_ctx;
   agora_audio_context_t  *audio_context;
} ngx_agora_context_t;

static ngx_int_t append_aud(ngx_rtmp_session_t *s, ngx_buf_t *out)
{
    static u_char   aud_nal[] = { 0x00, 0x00, 0x00, 0x01, 0x09, 0xf0 };

    if (out->last + sizeof(aud_nal) > out->end) {
        return NGX_ERROR;
    }

    out->last = ngx_cpymem(out->last, aud_nal, sizeof(aud_nal));

    return NGX_OK;
}

static ngx_int_t ngx_custom_copy(ngx_rtmp_session_t *s, void *dst, u_char **src, size_t n, ngx_chain_t **in)
{
    u_char  *last;
    size_t   pn;

    if (*in == NULL) {
        return NGX_ERROR;
    }

    for ( ;; ) {
        last = (*in)->buf->last;

        if ((size_t)(last - *src) >= n) {
            if (dst) {
                ngx_memcpy(dst, *src, n);
            }

            *src += n;

            while (*in && *src == (*in)->buf->last) {
                *in = (*in)->next;
                if (*in) {
                    *src = (*in)->buf->pos;
                }
            }

            return NGX_OK;
        }

        pn = last - *src;

        if (dst) {
            ngx_memcpy(dst, *src, pn);
            dst = (u_char *)dst + pn;
        }

        n -= pn;
        *in = (*in)->next;

        if (*in == NULL) {
            ngx_log_error(NGX_LOG_ERR, s->connection->log, 0, "failed to read %uz byte(s)", n);
            return NGX_ERROR;
        }

        *src = (*in)->buf->pos;
    }
}
static ngx_int_t append_sps_pps(ngx_rtmp_session_t *s, ngx_buf_t *out)
{
    ngx_rtmp_codec_ctx_t           *codec_ctx;
    u_char                         *p;
    ngx_chain_t                    *in;
    int8_t                          nnals;
    uint16_t                        len, rlen;
    ngx_int_t                       n;

    codec_ctx = ngx_rtmp_get_module_ctx(s, ngx_rtmp_codec_module);

    if ( codec_ctx == NULL) {
        return NGX_ERROR;
    }

    in = codec_ctx->avc_header;
    if (in == NULL) {
        return NGX_ERROR;
    }

    p = in->buf->pos;

    /*
     * Skip bytes:
     * - flv fmt
     * - H264 CONF/PICT (0x00)
     * - 0
     * - 0
     * - 0
     * - version
     * - profile
     * - compatibility
     * - level
     * - nal bytes
     */

    if (ngx_custom_copy(s, NULL, &p, 10, &in) != NGX_OK) {
        return NGX_ERROR;
    }

    /* number of SPS NALs */
    if (ngx_custom_copy(s, &nnals, &p, 1, &in) != NGX_OK) {
        return NGX_ERROR;
    }

    nnals &= 0x1f; /* 5lsb */

    ngx_log_debug1(NGX_LOG_DEBUG_RTMP, s->connection->log, 0,
                   "SPS number: %uz", nnals);

    /* SPS */
    for (n = 0; ; ++n) {
        for (; nnals; --nnals) {

            /* NAL length */
            if (ngx_custom_copy(s, &rlen, &p, 2, &in) != NGX_OK) {
                return NGX_ERROR;
            }

            ngx_rtmp_rmemcpy(&len, &rlen, 2);

            ngx_log_debug1(NGX_LOG_DEBUG_RTMP, s->connection->log, 0,
                           "header NAL length: %uz", (size_t) len);

            /* AnnexB prefix */
            if (out->end - out->last < 4) {
                ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                              "too small buffer for header NAL size");
                return NGX_ERROR;
            }

            *out->last++ = 0;
            *out->last++ = 0;
            *out->last++ = 0;
            *out->last++ = 1;

            /* NAL body */
            if (out->end - out->last < len) {
                ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                              "too small buffer for header NAL");
                return NGX_ERROR;
            }

            if (ngx_custom_copy(s, out->last, &p, len, &in) != NGX_OK) {
                return NGX_ERROR;
            }

            out->last += len;
        }

        if (n == 1) {
            break;
        }

        /* number of PPS NALs */
        if (ngx_custom_copy(s, &nnals, &p, 1, &in) != NGX_OK) {
            return NGX_ERROR;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_RTMP, s->connection->log, 0,
                       "PPS number: %uz", nnals);
    }

    return NGX_OK;
}

ngx_int_t  avcc_to_annexb(ngx_rtmp_session_t *s, ngx_rtmp_header_t *h, ngx_chain_t *in, u_char** out_buffer){

    ngx_rtmp_codec_ctx_t           *codec_ctx;
    u_char                         *p;
    uint8_t                         fmt, ftype, htype, nal_type, src_nal_type;
    uint32_t                        len, rlen;
    ngx_buf_t                       out;
    uint32_t                        cts;
    ngx_uint_t                      nal_bytes;
    ngx_int_t                       aud_sent, sps_pps_sent;
    static u_char                   buffer[NGX_RTMP_HLS_BUFSIZE];



    codec_ctx = ngx_rtmp_get_module_ctx(s, ngx_rtmp_codec_module);

    if (codec_ctx == NULL ||
        codec_ctx->avc_header == NULL || h->mlen < 1)
    {
        return NGX_OK;
    }

    /* Only H264 is supported */
    if (codec_ctx->video_codec_id != NGX_RTMP_VIDEO_H264) {
        return NGX_OK;
    }

    p = in->buf->pos;
    if (ngx_custom_copy(s, &fmt, &p, 1, &in) != NGX_OK) {
        return NGX_ERROR;
    }

    /* 1: keyframe (IDR)
     * 2: inter frame
     * 3: disposable inter frame */

    ftype = (fmt & 0xf0) >> 4;

    /* H264 HDR/PICT */

    if (ngx_custom_copy(s, &htype, &p, 1, &in) != NGX_OK) {
        return NGX_ERROR;
    }

    /* proceed only with PICT */

    if (htype != 1) {
        return NGX_OK;
    }

    /* 3 bytes: decoder delay */

    if (ngx_custom_copy(s, &cts, &p, 3, &in) != NGX_OK) {
        return NGX_ERROR;
    }

    cts = ((cts & 0x00FF0000) >> 16) | ((cts & 0x000000FF) << 16) |
          (cts & 0x0000FF00);

    ngx_memzero(&out, sizeof(out));

    out.start = buffer;
    out.end = buffer + sizeof(buffer);
    out.pos = out.start;
    out.last = out.pos;

    nal_bytes = codec_ctx->avc_nal_bytes;
    aud_sent = 0;
    sps_pps_sent = 0;

    while (in) {
        if (ngx_custom_copy(s, &rlen, &p, nal_bytes, &in) != NGX_OK) {
            return NGX_OK;
        }

        len = 0;
        ngx_rtmp_rmemcpy(&len, &rlen, nal_bytes);

        if (len == 0) {
            continue;
        }

        if (ngx_custom_copy(s, &src_nal_type, &p, 1, &in) != NGX_OK) {
            return NGX_OK;
        }

        nal_type = src_nal_type & 0x1f;

        ngx_log_debug2(NGX_LOG_DEBUG_RTMP, s->connection->log, 0,
                       " h264 NAL type=%ui, len=%uD",
                       (ngx_uint_t) nal_type, len);

        if (nal_type >= 7 && nal_type <= 9) {
            if (ngx_custom_copy(s, NULL, &p, len - 1, &in) != NGX_OK) {
                return NGX_ERROR;
            }
            continue;
        }

        if (!aud_sent) {
            switch (nal_type) {
                case 1:
                case 5:
                case 6:
                    if (append_aud(s, &out) != NGX_OK) {
                        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                                      "error appending AUD NAL");
                    }
                    /* fall through */
                case 9:
                    aud_sent = 1;
                    break;
            }
        }

        switch (nal_type) {
            case 1:
                sps_pps_sent = 0;
                break;
            case 5:
                if (sps_pps_sent) {
                    break;
                }
                if (append_sps_pps(s, &out) != NGX_OK) {
                    ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                                  "error appenging SPS/PPS NALs");
                }
                sps_pps_sent = 1;
                break;
        }

        /* AnnexB prefix */

        if (out.end - out.last < 5) {
            ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                          "not enough buffer for AnnexB prefix");
            return NGX_OK;
        }

        /* first AnnexB prefix is long (4 bytes) */

        if (out.last == out.pos) {
            *out.last++ = 0;
        }

        *out.last++ = 0;
        *out.last++ = 0;
        *out.last++ = 1;
        *out.last++ = src_nal_type;

        /* NAL body */

        if (out.end - out.last < (ngx_int_t) len) {
            ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                          "not enough buffer for NAL");
            return NGX_OK;
        }

        if (ngx_custom_copy(s, out.last, &p, len - 1, &in) != NGX_OK) {
            return NGX_ERROR;
        }

        out.last += (len - 1);
    }


    ngx_int_t buffer_length=(out.last-out.start);
    (*out_buffer)=(u_char*)malloc(buffer_length);
    memcpy(*out_buffer, buffer,  buffer_length);

    if(ftype){}

    return  buffer_length;
}

ngx_int_t ngx_agora_send_video(ngx_agora_context_t* ctx,  ngx_rtmp_session_t *s, ngx_rtmp_header_t *h, ngx_chain_t *in){
	
u_char*                   buffer=NULL;
ngx_int_t                 keyframe=0;
ngx_int_t                 len=0;

	keyframe=(ngx_rtmp_get_video_frame_type(in) == NGX_RTMP_VIDEO_KEY_FRAME);
        //show_states(s, keyframe);

        len=avcc_to_annexb(s,h,in,&buffer);
        if(buffer!=NULL){
           agora_send_video(ctx->agora_ctx,  buffer,len,keyframe);
           free(buffer);

           return len;
        }

	return 0;
}


//convert audio samples from singed float 32 bit to signed 16 bit
static void convert_audio_sample_f32_s16(const uint8_t* in, int16_t* out, uint16_t count) { 
	
   uint8_t src_size=sizeof(float);
   uint8_t target_size=sizeof(int16_t);
   
   float input_32_bit_float;
   for(int i=0;i<count;i++){

      memcpy(&input_32_bit_float, &in[i*src_size],src_size);
      int16_t out_16_bit_unsigned_int = ((input_32_bit_float ) * 32768) - 1;
      memcpy(&out[i], &out_16_bit_unsigned_int,target_size);
   }
} 


agora_audio_context_t*  agora_init_audio(ngx_int_t bitrate){

  av_register_all();

   int err=0;

   AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
   if(codec==NULL){
      return NULL;
   }

   AVCodecContext *avCtx = avcodec_alloc_context3(codec);
   if(avCtx==NULL){
      return NULL;
   }

  //set audio context
  avCtx->sample_rate = 48000;
  avCtx->channel_layout=AV_CH_LAYOUT_MONO;
  if (avcodec_open2(avCtx, codec, NULL) < 0){
     return NULL;
  }

  agora_audio_context_t* ctx=(agora_audio_context_t*)malloc(sizeof(agora_audio_context_t));
  ctx->avContext=avCtx;

  ctx->b=NULL;
  
  ctx->channel_count=1;
  ctx->sample_rate=48000;
  ctx->bitrate=bitrate;

  ctx->remaining_count=0;

   //init opus
  ctx-> opus_encoder= opus_encoder_create(ctx->sample_rate,
		        ctx->channel_count, OPUS_APPLICATION_VOIP, &err);
  if(err<0){
     return NULL;
  }
  err = opus_encoder_ctl(ctx-> opus_encoder, OPUS_SET_BITRATE(ctx->bitrate));
  if(err<0){
     return NULL;
  }

  return ctx;
}

void get_next_packet(agora_audio_context_t* audio_ctx,  const uint8_t* in,const uint64_t in_length, uint8_t* out)
{

   uint32_t opus_packet_size=OPUS_SAMPLE_COUNT*audio_ctx->channel_count*sizeof(int16_t);
  if(in==NULL){

    if(audio_ctx->remaining_count<opus_packet_size) {
        return;
     }

     memcpy(&out[0], &audio_ctx->remaining_bytes[0], opus_packet_size);
     audio_ctx->remaining_count -=opus_packet_size;
	 
     return;
  }

				   		   
  //we neeed to fill the input buffer with remaining bytes from the previous call + some of the new ones
  int previous_bytes=audio_ctx->remaining_count;
  memcpy(&out[0], &audio_ctx->remaining_bytes[0], previous_bytes);


  int free_bytes=opus_packet_size-previous_bytes;
  memcpy(&out[previous_bytes], &in[0], free_bytes);
				   
  //we should save the new remaining bytes in the circular buffer
  audio_ctx->remaining_count =in_length-free_bytes;
  memcpy(&audio_ctx->remaining_bytes[0], &in[free_bytes],audio_ctx->remaining_count);

}
static ngx_int_t encode_opus(agora_audio_context_t* audio_ctx,  int16_t* in, uint8_t*out)
{ 
   uint16_t input_packet_size=INPUT_SAMPLE_COUNT*audio_ctx->channel_count*sizeof(int16_t);
   int16_t temp[OPUS_SAMPLE_COUNT*MAX_CHANNEL_COUNT];

   get_next_packet(audio_ctx,(const uint8_t*)in,input_packet_size,(uint8_t*)temp);

   return opus_encode(audio_ctx->opus_encoder,temp,OPUS_SAMPLE_COUNT, out, 1000);
}

static ngx_int_t flush_encode_opus(agora_audio_context_t* audio_ctx, uint8_t*out)
{
   uint16_t opus_packet_size=OPUS_SAMPLE_COUNT*audio_ctx->channel_count*sizeof(int16_t);
   if(audio_ctx->remaining_count>=opus_packet_size){

       int16_t temp[OPUS_SAMPLE_COUNT*MAX_CHANNEL_COUNT];
       get_next_packet(audio_ctx,NULL,0,(uint8_t*)temp);
       audio_ctx->remaining_count=0;
       return opus_encode(audio_ctx->opus_encoder,temp,OPUS_SAMPLE_COUNT, out, 1000);
   }

   return 0;
}
int decode_acc_audio(ngx_agora_context_t *actx, uint8_t *data, uint16_t input_size){


    AVPacket avPacket;
    av_init_packet(&avPacket);

    avPacket.size = input_size; 
    avPacket.data = data;

    agora_audio_context_t* ctx=actx->audio_context;

    AVFrame * decoded_frame = av_frame_alloc();

    decoded_frame->format=AV_SAMPLE_FMT_S16;
    decoded_frame->channel_layout =ctx->avContext->channel_layout;
    decoded_frame->sample_rate = ctx->avContext->sample_rate;


    if(avcodec_send_packet(ctx->avContext, &avPacket)!=0){
       return -1;
    }

   if( avcodec_receive_frame(ctx->avContext,  decoded_frame)!=0){
       return -1;
   }


    av_samples_get_buffer_size( NULL,
                                                ctx->channel_count,
                                                decoded_frame->nb_samples,
                                                ctx->avContext->sample_fmt,
                                                1);

    uint16_t decoded_samples=decoded_frame->nb_samples;

    int16_t buffer[MAX_CHANNEL_COUNT*INPUT_SAMPLE_COUNT];
    convert_audio_sample_f32_s16(decoded_frame->data[0],buffer, decoded_samples*ctx->channel_count);

    uint8_t  out[1000];
    ngx_int_t encoded_bytes=encode_opus(ctx, buffer, out);
    agora_send_audio(actx->agora_ctx,out, encoded_bytes);

    //flush any remaining bytes in the buffer
    encoded_bytes=flush_encode_opus(ctx,out);
    if(encoded_bytes>0){
      agora_send_audio(actx->agora_ctx, out, encoded_bytes);
    }

   av_frame_free(&decoded_frame);

   return  decoded_samples;
}


static ngx_int_t ngx_flush_audio(ngx_agora_context_t* ctx)
{
    uint16_t buffer_size=0;
    uint16_t sample_count=0;

    ngx_buf_t           *b=ctx->audio_context->b;
    if (b == NULL || b->pos == b->last) {
        return NGX_OK;
    }

    buffer_size= b->last- b->pos;

    sample_count=decode_acc_audio(ctx,b->pos, buffer_size);
   
    b->pos = b->last = b->start;

    return sample_count;
}

static ngx_int_t ngx_parse_aac_header(ngx_rtmp_session_t *s, 
		    ngx_uint_t *objtype, ngx_uint_t *srindex,
		    ngx_uint_t *chconf){

    ngx_rtmp_codec_ctx_t   *codec_ctx;
    ngx_chain_t            *cl;
    u_char                 *p, b0, b1;

    codec_ctx = ngx_rtmp_get_module_ctx(s, ngx_rtmp_codec_module);
    cl = codec_ctx->aac_header;

    p = cl->buf->pos;
    if (ngx_custom_copy(s, NULL, &p, 2, &cl) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_custom_copy(s, &b0, &p, 1, &cl) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_custom_copy(s, &b1, &p, 1, &cl) != NGX_OK) {
        return NGX_ERROR;
    }

    *objtype = b0 >> 3;
    if (*objtype == 0 || *objtype == 0x1f) {
        ngx_log_debug1(NGX_LOG_DEBUG_RTMP, s->connection->log, 0,
                       "hls: unsupported adts object type:%ui", *objtype);
        return NGX_ERROR;
    }

    if (*objtype > 4) {

        /*
         * Mark all extended profiles as LC
         * to make Android as happy as possible.
         */

        *objtype = 2;
    }

    *srindex = ((b0 << 1) & 0x0f) | ((b1 & 0x80) >> 7);
    if (*srindex == 0x0f) {
        ngx_log_debug1(NGX_LOG_DEBUG_RTMP, s->connection->log, 0,
                       "unsupported adts sample rate:%ui", *srindex);
        return NGX_ERROR;
    }

    *chconf = (b1 >> 3) & 0x0f;
    ngx_log_debug3(NGX_LOG_DEBUG_RTMP, s->connection->log, 0,
                   "aac object_type:%ui, sample_rate_index:%ui, "
                   "channel_config:%ui", *objtype, *srindex, *chconf);

    return NGX_OK;
}

ngx_int_t allocate_buffer(agora_audio_context_t *audio_ctx,ngx_rtmp_session_t *s)
{
     audio_ctx->b = ngx_pcalloc(s->connection->pool, sizeof(ngx_buf_t));
     if (audio_ctx->b == NULL) {
          return NGX_ERROR;
     }

     audio_ctx->b->start = ngx_palloc(s->connection->pool,MAX_AUDIO_BUFFER_SIZE);
     if (audio_ctx->b->start == NULL) {
         return NGX_ERROR;
     }

     audio_ctx->b->end =audio_ctx->b->start + MAX_AUDIO_BUFFER_SIZE;
     audio_ctx->b->pos = audio_ctx->b->last = audio_ctx->b->start;

      return NGX_OK;
}
ngx_int_t ngx_agora_send_audio(ngx_agora_context_t *ctx,  ngx_rtmp_session_t *s, ngx_rtmp_header_t *h, ngx_chain_t *in)
{
    ngx_rtmp_codec_ctx_t            *codec_ctx;
    size_t                          bsize;
    u_char                          *p;
    ngx_uint_t                      objtype, srindex, chconf, size;

    ngx_buf_t           *b;

    agora_audio_context_t* audio_ctx=ctx->audio_context;

   if( audio_ctx==NULL){
      ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,"Agora audio has not be initialized!");
      return NGX_OK;
   }

    codec_ctx = ngx_rtmp_get_module_ctx(s, ngx_rtmp_codec_module);
    if ( codec_ctx == NULL  || h->mlen < 2)
    {
        return NGX_OK;
    }

    if (codec_ctx->audio_codec_id != NGX_RTMP_AUDIO_AAC ||
        codec_ctx->aac_header == NULL || ngx_rtmp_is_codec_header(in))
    {
        return NGX_OK;
    }


    //allocate the buffer if it has not been allocated before
    if (audio_ctx->b == NULL) {
        allocate_buffer(audio_ctx,s);
    }
    b=audio_ctx->b;

    size = h->mlen - 2 + 7;
    if (b->start + size > b->end) {
        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,"too big audio frame");
        return NGX_OK;
    }

   if( b->last > b->pos){
       ngx_flush_audio(ctx);
    }

   if (b->last + size > b->end) {
         ngx_flush_audio(ctx);
    }

    ngx_log_debug1(NGX_LOG_DEBUG_RTMP, s->connection->log, 0,
                   " audio pts=%uL", pts);

    if (b->last + 7 > b->end) {
        ngx_log_debug0(NGX_LOG_DEBUG_RTMP, s->connection->log, 0,
                       "not enough buffer for audio header");
        return NGX_OK;
    }

    p = b->last;
    b->last += 5;

    /* copy payload */
    for (; in && b->last < b->end; in = in->next) {

        bsize = in->buf->last - in->buf->pos;
        if (b->last + bsize > b->end) {
            bsize = b->end - b->last;
        }

        b->last = ngx_cpymem(b->last, in->buf->pos, bsize);
    }

    /* make up ADTS header */
    if (ngx_parse_aac_header(s, &objtype, &srindex, &chconf)
        != NGX_OK)
    {
        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                      "hls: aac header error");
        return NGX_OK;
    }

    /* we have 5 free bytes + 2 bytes of RTMP frame header */

    p[0] = 0xff;
    p[1] = 0xf1;
    p[2] = (u_char) (((objtype - 1) << 6) | (srindex << 2) |
                     ((chconf & 0x04) >> 2));
    p[3] = (u_char) (((chconf & 0x03) << 6) | ((size >> 11) & 0x03));
    p[4] = (u_char) (size >> 3);
    p[5] = (u_char) ((size << 5) | 0x1f);
    p[6] = 0xfc;

    if (p != b->start) {
        return NGX_OK;
    }

    if (codec_ctx->sample_rate == 0) {
        return NGX_OK;
    }

    return NGX_OK;
}

ngx_agora_context_t* ngx_agora_init(char* app_id, char* ch_id,char* user_id, ngx_int_t bitrate)
{
   agora_context_t        *agora_ctx;
   agora_audio_context_t  *audio_context;

   ngx_agora_context_t* ctx=(ngx_agora_context_t*)malloc(sizeof(ngx_agora_context_t));
   if(ctx==NULL){
      return NULL;
   }

   //initialize agora
   agora_ctx=agora_init(app_id, ch_id,user_id);
   if(agora_ctx==NULL){
     free(ctx);
     return NULL;
   }

   //initialize audio encoder/decoder
   audio_context=agora_init_audio(bitrate);
   if(audio_context==NULL){
     free(ctx);
     return NULL;
   }

   ctx->agora_ctx=agora_ctx;
   ctx->audio_context=audio_context;

   return ctx;
}

void ngx_agora_disconnect(ngx_agora_context_t** ctx_ptr)
{
   ngx_agora_context_t* ctx;
   if((*ctx_ptr)==NULL){
      return;
   }

   ctx=(*ctx_ptr);

   //disconnect agora
   agora_disconnect(ctx->agora_ctx);

   opus_encoder_destroy(ctx->audio_context->opus_encoder);
   avcodec_free_context(&ctx->audio_context->avContext);
   free(ctx->audio_context);

   free(*ctx_ptr);
   *ctx_ptr=NULL;
}

char* find_arg_end(char* arg){

   //if we can find & char, return this pos
   char* end_pos=strchr(arg,'&');
   if(end_pos!=NULL){
      return end_pos;
   }

   //otherwise we return the last position of the string
   return arg+strlen(arg);
}
ngx_int_t parse_args(ngx_str_t args, char* app_id, char* ch_id){

   const char* appid_label="appid";
   const char* channel_label="channel";
   char* appid_start_pos=NULL;
   char* channel_start_pos=NULL;

   char* appid_end_pos=NULL;
   char* channel_end_pos=NULL;

   appid_start_pos=strstr((char*)args.data,appid_label);
   channel_start_pos=strstr((char*)args.data, channel_label);

   if(appid_start_pos==NULL || channel_start_pos==NULL){
      return -1;
   }   

  appid_end_pos=find_arg_end(appid_start_pos);
  channel_end_pos=find_arg_end(channel_start_pos);

  //update the exact start position of the values
  appid_start_pos +=strlen(appid_label)+1;
  channel_start_pos +=strlen(channel_label)+1;

  //copy appid and channel
  memcpy(app_id,appid_start_pos, appid_end_pos-appid_start_pos);
  memcpy(ch_id, channel_start_pos, channel_end_pos-channel_start_pos);

   return 0;
}

ngx_int_t get_arg_value(ngx_str_t args, const char* key, char* value)
{
   char* key_start_pos=NULL;
   char* value_start_pos=NULL;
   char* value_end_pos=NULL;

   key_start_pos=strstr((char*)args.data,key);
   if(key_start_pos==NULL){
      return -1;
   }
   value_start_pos=key_start_pos+strlen(key)+1;
   value_end_pos=find_arg_end(key_start_pos);

    memcpy(value,value_start_pos, value_end_pos-value_start_pos);

    return 0;
}
