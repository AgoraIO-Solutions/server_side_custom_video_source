/*
 * this file contains helper funtions to send video to agora nginx-rtmp-module
 */

/*
 * convert h264 video from avcc fromat to annex B format
 * @in: s -- current rtmp session
 * @in: h -- frame header
 * @in: in -- frame data packets
 * @out: converted  bytes
 * @return: the length of converted bytes
 */
ngx_int_t  avcc_to_annexb(ngx_rtmp_session_t *s, ngx_rtmp_header_t *h, ngx_chain_t *in, u_char** out_buffer);

/*
 * send video to agora, internally does the conversion from avcc to  annex B h264 format
 * * @in: s -- current rtmp session
 * @in: h -- frame header
 * @in: in -- frame data packets
 * @return: actual number of sent bytes
 */
 ngx_int_t send_video_to_agora(ngx_rtmp_session_t *s, ngx_rtmp_header_t *h, ngx_chain_t *in);
