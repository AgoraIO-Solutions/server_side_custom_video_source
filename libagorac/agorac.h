#ifndef _AGORA_C_H_
#define _AGORA_C_H_

#ifdef __cplusplus
 #define EXTERNC extern "C"
 #else
 #define EXTERNC
 #endif

 typedef  struct agora_context_t agora_context_t;

 EXTERNC agora_context_t*  agora_init(char* app_id, char* ch_id);

 EXTERNC int  agora_send_video(agora_context_t* ctx,  const unsigned char* buffer,  unsigned long len, int is_key_frame);
 EXTERNC int  agora_send_audio(agora_context_t* ctx,  const unsigned char* buffer,  unsigned long len);

 EXTERNC void agora_disconnect(agora_context_t* ctx);

 #undef EXTERNC


#endif
