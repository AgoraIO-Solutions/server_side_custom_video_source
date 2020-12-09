#include "ngx_rtmp.h"
#include "ngx_rtmp_cmd_module.h"
#include "ngx_rtmp_netcall_module.h"
#include "ngx_rtmp_codec_module.h"
#include "ngx_rtmp_record_module.h"

#include "agorac.h"

#define NGX_RTMP_HLS_BUFSIZE            (1024*1024)

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

ngx_int_t send_video_to_agora(ngx_rtmp_session_t *s, ngx_rtmp_header_t *h, ngx_chain_t *in){
	
u_char*                   buffer=NULL;
ngx_int_t                 keyframe=0;
ngx_int_t                 len=0;

	keyframe=(ngx_rtmp_get_video_frame_type(in) == NGX_RTMP_VIDEO_KEY_FRAME);
        //show_states(s, keyframe);

        len=avcc_to_annexb(s,h,in,&buffer);
        if(buffer!=NULL){
                agora_send_video(buffer,len,keyframe);
                free(buffer);
		return len;
        }

	return 0;
}

