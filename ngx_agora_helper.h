
/*
 * this file contains helper funtions to send video to agora nginx-rtmp-module
 */
typedef struct ngx_agora_context_t ngx_agora_context_t; 
//typedef struct ngx_rtmp_session_t  ngx_rtmp_session_t;

/*
 * initialize agora context and audio encoder/decoders. 
 * Must be called before sending any audio/video
 * @return: allocated agora context
 */
ngx_agora_context_t* ngx_agora_init(ngx_rtmp_session_t *s);

/*
 * disconnect from agora. Deallocate agora context
 * @in: ctx --agora context to free
 */
void  ngx_agora_disconnect(ngx_agora_context_t** ctx);

/*
 * send video to agora, internally does the conversion from avcc to  annex B h264 format
 * @ctx --agora context that has been allocated at the begining 
 * @in: s -- current rtmp session
 * @in: h -- frame header
 * @in: in -- frame data packets
 * @return: actual number of sent bytes
 */
 ngx_int_t ngx_agora_send_video(ngx_agora_context_t* ctx, ngx_rtmp_session_t *s, ngx_rtmp_header_t *h, ngx_chain_t *in);


/*
 * send audio to agora and perfom any required preprocessing
 ** @ctx --agora context that has been allocated at the begining 
 * @in: s -- current rtmp session
 * @in: h -- frame header
 * @in: in -- frame data packets
 * @return: actual number of sent bytes
 */
ngx_int_t ngx_agora_send_audio(ngx_agora_context_t* ctx,  ngx_rtmp_session_t *s, ngx_rtmp_header_t *h, ngx_chain_t *in);

/*
 * parse argument list and extract app_id and channel id
 * @in: args-- argument list in the form key1=value2&key2=value2 ...
 * @out: app_id -- extracted app id from the arg list
 * @out: ch_id -- extracted channel id
 * @return 0 on success or -1 on failure
 */
ngx_int_t parse_args(ngx_str_t args, char* app_id, char* ch_id);

/*
 * extract a value from a certain key from the arg list
 */
ngx_int_t  get_arg_value(ngx_str_t args, const char* key, char* value);
