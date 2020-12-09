#ifndef _AGORA_C_H_
#define _AGORA_C_H_

#ifdef __cplusplus
 #define EXTERNC extern "C"
 #else
 #define EXTERNC
 #endif

 EXTERNC int agora_init();

 EXTERNC int  agora_send_video(const unsigned char* buffer,  unsigned long len, int is_key_frame);
 EXTERNC int  agora_send_audio(const unsigned char* buffer,  unsigned long len);

  EXTERNC void agora_disconnect();

 #undef EXTERNC


#endif
