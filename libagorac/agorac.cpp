
#include "agorac.h"

#include <string>
#include <iostream>
#include <fstream>

//agora header files
#include "NGIAgoraRtcConnection.h"
#include "IAgoraService.h"
//#include "wrapper/local_user_observer.h"
//#include "wrapper/helper.h"

#include "workqueue.h"

#include <string.h>
#include <chrono>

//agora types

using AgoraVideoSender_ptr=agora::agora_refptr<agora::rtc::IVideoEncodedImageSender>;
using AgoraAudioSender_ptr=agora::agora_refptr<agora::rtc::IAudioEncodedFrameSender>;
using AgoraVideoFrameType=agora::rtc::VIDEO_FRAME_TYPE;
using ConnectionConfig=agora::rtc::RtcConnectionConfiguration;

//a context that group all require info about agora
typedef struct{

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

  bool                                            isRunning;

} Agora_ctx;

class Work{

public:

   Work(const unsigned char* b, const unsigned long& l, bool is_key){
  
       buffer=new unsigned char[l];
       memcpy(buffer, b, l);

       len=l;
    
       is_key_frame=is_key;
       is_finished=0;
   }

   ~Work(){
     delete [] buffer;
   }

   unsigned char*        buffer;
   unsigned long         len;
   bool                  is_key_frame;

   bool                   is_finished;
   
};

void logMessage(std::string message){
  
}

static void VideoThreadHandler();


//a global context, this might not be a thread safe
Agora_ctx  agora_ctx;

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


int agora_init(){


  std::string app_id="APPID";
  std::string chanel_id="CHANNEL";
  std::string user_id="";

  //set configuration
  agora_ctx.config.clientRoleType = agora::rtc::CLIENT_ROLE_BROADCASTER;;
  agora_ctx.config.channelProfile = agora::CHANNEL_PROFILE_LIVE_BROADCASTING;
  agora_ctx.config.autoSubscribeAudio = false;
  agora_ctx.config.autoSubscribeVideo = false;

  // Create Agora service
  agora_ctx.service = createAndInitAgoraService(false, true, true);
  if (!agora_ctx.service) {
    logMessage("Cannot create service");
    return 0;
  }

  logMessage("create service");

  agora_ctx.connection =agora_ctx.service->createRtcConnection(agora_ctx.config);
  if (!agora_ctx.connection) {
     return 0;
  }

  // Connect to Agora channel
  auto  connected =  agora_ctx.connection->connect(app_id.c_str(), chanel_id.c_str(), user_id.c_str());
  if (connected) {
     return 0;
  }


  // Create media node factory
  auto factory = agora_ctx.service->createMediaNodeFactory();
  if (!factory) {
    return 0;
  }


  //audio
  // Create audio data sender
   agora_ctx.audioSender = factory->createAudioEncodedFrameSender();
  if (!agora_ctx.audioSender) {
    return false;
  }

  //video
  // Create audio track
  auto customAudioTrack =agora_ctx.service->createCustomAudioTrack(agora_ctx.audioSender, agora::base::MIX_DISABLED);
  if (!customAudioTrack) {
    return false;
  }

  // Create video frame sender
  agora_ctx.videoSender = factory->createVideoEncodedImageSender();
  if (!agora_ctx.videoSender) {
    return false;
  }


  // Create video track
  agora::base::SenderOptions options;
  auto customVideoTrack =agora_ctx.service->createCustomVideoTrack(agora_ctx.videoSender,options);
  if (!customVideoTrack) {
    return false;
  }

  // Publish audio & video track
  agora_ctx.connection->getLocalUser()->publishAudio(customAudioTrack);
  agora_ctx.connection->getLocalUser()->publishVideo(customVideoTrack);

  agora_ctx.isConnected=1;

  agora_ctx.videoQueue=std::make_shared<WorkQueue <Work_ptr> >();
  agora_ctx.audioQueue=std::make_shared<WorkQueue <Work_ptr> >();

  agora_ctx.isRunning=true;

  //start thread handlers
  agora_ctx.videoThread=std::make_shared<std::thread>(&VideoThreadHandler);
 // agora_ctx.audioThread=std::make_shared<std::thread>(&AudioThreadHandler);

  return 1;
}


bool doSendVideo(const unsigned char* buffer,  unsigned long len,int is_key_frame){

  auto frameType=agora::rtc::VIDEO_FRAME_TYPE_DELTA_FRAME; 
  if(is_key_frame){
     frameType=agora::rtc::VIDEO_FRAME_TYPE_KEY_FRAME;
  }

  
  agora::rtc::EncodedVideoFrameInfo videoEncodedFrameInfo;
  videoEncodedFrameInfo.rotation = agora::rtc::VIDEO_ORIENTATION_0;
  videoEncodedFrameInfo.codecType = agora::rtc::VIDEO_CODEC_H264;
  videoEncodedFrameInfo.framesPerSecond = 30;
  videoEncodedFrameInfo.frameType = frameType;

  agora_ctx.videoSender->sendEncodedVideoImage(buffer,len,videoEncodedFrameInfo);
  return true;
}


int agora_send_video(const unsigned char * buffer,  unsigned long len,int is_key_frame){

   Work_ptr work=std::make_shared<Work>(buffer,len, is_key_frame);
   agora_ctx.videoQueue->add(work);
}

static void VideoThreadHandler(){


   while(agora_ctx.isRunning=true){

     //wait untill work is available
     agora_ctx.videoQueue->waitForWork();	  
     Work_ptr work=agora_ctx.videoQueue->get();
     if(work==NULL) continue;

     if(work->is_finished){
	return;
     }

     doSendVideo(work->buffer, work->len, (bool)(work->is_key_frame));

  }
}

void agora_disconnect(){


   agora_ctx.isRunning=true;
   //tell the thread that we are finished
   Work_ptr work=std::make_shared<Work>((const unsigned char*)"finished",9, false);
   work->is_finished=true;
   agora_ctx.videoQueue->add(work);

   std::this_thread::sleep_for(std::chrono::seconds(1));

    //delete context
   agora_ctx.connection->disconnect();
   agora_ctx.connection=nullptr;
  
   agora_ctx.videoThread->detach();    
}

int agora_send_audio(const unsigned char * buffer,  unsigned long len){
}
