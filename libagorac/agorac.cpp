
#include "agorac.h"

#include <string>
#include <iostream>
#include <fstream>

//agora header files
#include "NGIAgoraRtcConnection.h"
#include "IAgoraService.h"

#include "workqueue.h"

#include <string.h>
#include <chrono>

//agora types

using AgoraVideoSender_ptr=agora::agora_refptr<agora::rtc::IVideoEncodedImageSender>;
using AgoraAudioSender_ptr=agora::agora_refptr<agora::rtc::IAudioEncodedFrameSender>;
using AgoraVideoFrameType=agora::rtc::VIDEO_FRAME_TYPE;
using ConnectionConfig=agora::rtc::RtcConnectionConfiguration;

//a context that group all require info about agora
struct agora_context_t{

  agora::base::IAgoraService*                      service;
  agora::agora_refptr<agora::rtc::IRtcConnection>  connection;

  int                                              isConnected;
  ConnectionConfig                                 config;

  AgoraVideoSender_ptr                             videoSender;
  AgoraAudioSender_ptr                             audioSender;


  WorkQueue_ptr                                   videoQueue;
  WorkQueue_ptr                                   audioQueue;

  std::shared_ptr<std::thread>                    videoThread;
  std::shared_ptr<std::thread>                    audioThread;

  agora::agora_refptr<agora::rtc::ILocalAudioTrack> audioTrack;
  agora::agora_refptr<agora::rtc::ILocalVideoTrack> videoTrack;

  bool                                            isRunning;

};

class Work{

public:

   Work(const unsigned char* b, const unsigned long& l, bool is_key):
	buffer(nullptr){
  
       if(b==nullptr){
	  return;
       }

       buffer=new unsigned char[l];
       memcpy(buffer, b, l);

       len=l;
    
       is_key_frame=is_key;
       is_finished=0;
   }

   ~Work(){
     if(buffer==nullptr) return;
     delete [] buffer;
   }

   unsigned char*        buffer;
   unsigned long         len;
   bool                  is_key_frame;

   bool                   is_finished;
   
};
void logMessage(std::string message){
  
}

static void VideoThreadHandler(agora_context_t* ctx);
static void AudioThreadHandler(agora_context_t* ctx);


//helper function for creating a service
agora::base::IAgoraService* createAndInitAgoraService(bool enableAudioDevice,
                                                      bool enableAudioProcessor, bool enableVideo) {
  auto service = createAgoraService();
  agora::base::AgoraServiceConfiguration scfg;
  scfg.enableAudioProcessor = enableAudioProcessor;
  scfg.enableAudioDevice = enableAudioDevice;
  scfg.enableVideo = enableVideo;

  int ret = service->initialize(scfg);
  return (ret == agora::ERR_OK) ? service : nullptr;
}


agora_context_t*  agora_init(char* in_app_id, char* in_ch_id){


  agora_context_t* ctx=new agora_context_t;

  std::string app_id(in_app_id);
  std::string chanel_id=(in_ch_id);
  std::string user_id="";

  //set configuration
  ctx->config.clientRoleType = agora::rtc::CLIENT_ROLE_BROADCASTER;;
  ctx->config.channelProfile = agora::CHANNEL_PROFILE_LIVE_BROADCASTING;
  ctx->config.autoSubscribeAudio = false;
  ctx->config.autoSubscribeVideo = false;

  // Create Agora service
  ctx->service = createAndInitAgoraService(false, true, true);
  if (!ctx->service) {
    delete ctx;
    return NULL;
  }


  ctx->connection =ctx->service->createRtcConnection(ctx->config);
  if (!ctx->connection) {
     delete ctx;
     return NULL;
  }

  // Connect to Agora channel
  auto  connected =  ctx->connection->connect(app_id.c_str(), chanel_id.c_str(), user_id.c_str());
  if (connected) {
     delete ctx;
     return NULL;
  }


  // Create media node factory
  auto factory = ctx->service->createMediaNodeFactory();
  if (!factory) {
    return NULL;
  }


  //audio
  // Create audio data sender
   ctx->audioSender = factory->createAudioEncodedFrameSender();
  if (!ctx->audioSender) {
    return NULL;
  }

  // Create audio track
  ctx->audioTrack =ctx->service->createCustomAudioTrack(ctx->audioSender, agora::base::MIX_DISABLED);
  if (!ctx->audioTrack) {
    return NULL;
  }

  // Create video frame sender
  ctx->videoSender = factory->createVideoEncodedImageSender();
  if (!ctx->videoSender) {
    return NULL;
  }


  // Create video track
  agora::base::SenderOptions options;
  ctx->videoTrack =ctx->service->createCustomVideoTrack(ctx->videoSender,options);
  if (!ctx->videoTrack) {
    return NULL;
  }

  // Publish audio & video track
  ctx->connection->getLocalUser()->publishAudio(ctx->audioTrack);
  ctx->connection->getLocalUser()->publishVideo(ctx->videoTrack);

  ctx->isConnected=1;

  ctx->videoQueue=std::make_shared<WorkQueue <Work_ptr> >();
  ctx->audioQueue=std::make_shared<WorkQueue <Work_ptr> >();

  ctx->isRunning=true;

  //start thread handlers
  ctx->videoThread=std::make_shared<std::thread>(&VideoThreadHandler,ctx);
  ctx->audioThread=std::make_shared<std::thread>(&AudioThreadHandler,ctx);

  return ctx;
}


bool doSendVideo(agora_context_t* ctx, const unsigned char* buffer,  unsigned long len,int is_key_frame){

  auto frameType=agora::rtc::VIDEO_FRAME_TYPE_DELTA_FRAME; 
  if(is_key_frame){
     frameType=agora::rtc::VIDEO_FRAME_TYPE_KEY_FRAME;
  }

  
  agora::rtc::EncodedVideoFrameInfo videoEncodedFrameInfo;
  videoEncodedFrameInfo.rotation = agora::rtc::VIDEO_ORIENTATION_0;
  videoEncodedFrameInfo.codecType = agora::rtc::VIDEO_CODEC_H264;
  videoEncodedFrameInfo.framesPerSecond = 30;
  videoEncodedFrameInfo.frameType = frameType;

  ctx->videoSender->sendEncodedVideoImage(buffer,len,videoEncodedFrameInfo);
  return true;
}

bool doSendAudio(agora_context_t* ctx, const unsigned char* buffer,  unsigned long len){

  agora::rtc::EncodedAudioFrameInfo audioFrameInfo;
  audioFrameInfo.numberOfChannels =1; //TODO
  audioFrameInfo.sampleRateHz = 48000; //TODO
  audioFrameInfo.codec = agora::rtc::AUDIO_CODEC_OPUS;

  ctx->audioSender->sendEncodedAudioFrame(buffer,len, audioFrameInfo); 
  return true;
}


int agora_send_video(agora_context_t* ctx,const unsigned char * buffer,  unsigned long len,int is_key_frame){

   Work_ptr work=std::make_shared<Work>(buffer,len, is_key_frame);
   ctx->videoQueue->add(work);

   return 0; //no errors
}

static void VideoThreadHandler(agora_context_t* ctx){

   while(ctx->isRunning==true){

     //wait untill work is available
     ctx->videoQueue->waitForWork();	  
     Work_ptr work=ctx->videoQueue->get();
     if(work==NULL) continue;

     if(work->is_finished==1){
	break;
     }

     doSendVideo(ctx,work->buffer, work->len, (bool)(work->is_key_frame));
  }

}

static void AudioThreadHandler(agora_context_t* ctx){
   while(ctx->isRunning==true){

     //wait untill work is available
     ctx->audioQueue->waitForWork();
     Work_ptr work=ctx->audioQueue->get();
     if(work==NULL) continue;

     if(work->is_finished){
        return;
     }

     doSendAudio(ctx,work->buffer, work->len);
  }
}
void agora_disconnect(agora_context_t* ctx){

   ctx->isRunning=true;
   //tell the thread that we are finished
   Work_ptr work=std::make_shared<Work>(nullptr,0, false);
   work->is_finished=true;

   ctx->videoQueue->add(work);
   ctx->audioQueue->add(work);

   ctx->connection->getLocalUser()->unpublishAudio(ctx->audioTrack);
   ctx->connection->getLocalUser()->unpublishVideo(ctx->videoTrack);

   bool  isdisconnected=ctx->connection->disconnect();
   if(!isdisconnected){
      return;
   }
//   std::this_thread::sleep_for(std::chrono::seconds(1));

   ctx->audioSender = nullptr;
   ctx->videoSender = nullptr;
   ctx->audioTrack = nullptr;
   ctx->videoTrack = nullptr;

   //delete context
   ctx->connection=nullptr;
  
   ctx->videoThread->detach();
   ctx->audioThread->detach(); 
}

int agora_send_audio(agora_context_t* ctx,const unsigned char * buffer,  unsigned long len){

    Work_ptr work=std::make_shared<Work>(buffer,len, 0);
    ctx->audioQueue->add(work);

    return 0;
}
