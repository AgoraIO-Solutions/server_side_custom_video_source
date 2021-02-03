#ifndef _AGORA_C_H_
#define _AGORA_C_H_

#ifdef __cplusplus
 #define EXTERNC extern "C"
 #else
 #define EXTERNC
 #endif

 typedef  struct agora_context_t agora_context_t;

 typedef  void (*agora_log_func_t)(void*, const char*);

 EXTERNC agora_context_t*  agora_init(char* app_id, char* ch_id, char* user_id,
		                      short enable_dual, unsigned int  dual_vbr, 
				      unsigned short  dual_width,  unsigned short  dual_height);

 EXTERNC int  agora_send_video(agora_context_t* ctx,  const unsigned char* buffer,  unsigned long len, int is_key_frame);
 EXTERNC int  agora_send_audio(agora_context_t* ctx,  const unsigned char* buffer,  unsigned long len);

 EXTERNC void agora_disconnect(agora_context_t* ctx);

 EXTERNC void agora_set_log_function(agora_context_t* ctx, agora_log_func_t f, void* log_ctx);

 #undef EXTERNC


#endif
