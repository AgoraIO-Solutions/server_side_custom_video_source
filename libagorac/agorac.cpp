#include <stdbool.h>
#include <fstream>
#include "agorac.h"

#include <string>
#include <cstring>
#include <iostream>
#include <fstream>

#include <string.h>
#include <chrono>
#include <algorithm>
#include <cmath>

//agora header files
#include "NGIAgoraRtcConnection.h"
#include "IAgoraService.h"
#include "AgoraBase.h"

#include "workqueue.h"

#include "helpers/agoradecoder.h"
#include "helpers/agoraencoder.h"
#include "helpers/agoralog.h"
#include "helpers/localconfig.h"

#include "userobserver.h"

//agora types
using AgoraVideoSender_ptr=agora::agora_refptr<agora::rtc::IVideoEncodedImageSender>;
using AgoraAudioSender_ptr=agora::agora_refptr<agora::rtc::IAudioEncodedFrameSender>;
using AgoraVideoFrameType=agora::rtc::VIDEO_FRAME_TYPE;
using ConnectionConfig=agora::rtc::RtcConnectionConfiguration;

using AgoraDecoder_ptr=std::shared_ptr<AgoraDecoder>;
using AgoraEncoder_ptr=std::shared_ptr<AgoraEncoder>;

using LocalConfig_ptr=std::shared_ptr<LocalConfig>;

//helper stuffs
using TimePoint=std::chrono::steady_clock::time_point;

static TimePoint Now(){
   return std::chrono::steady_clock::now();
}

static long GetTimeDiff(const TimePoint& start, const TimePoint& end){
  return std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
}

//a context that group all require info about agora
struct agora_context_t{

  agora::base::IAgoraService*                      service;
  agora::agora_refptr<agora::rtc::IRtcConnection>  connection;

  int                                              isConnected;
  ConnectionConfig                                 config;

  AgoraVideoSender_ptr                             videoSender;
  AgoraAudioSender_ptr                             audioSender;


  WorkQueue_ptr                                   videoQueueHigh;
  WorkQueue_ptr                                   videoQueueLow;
  WorkQueue_ptr                                   audioQueue;

  std::shared_ptr<std::thread>                    videoThreadHigh;
  std::shared_ptr<std::thread>                    videoThreadLow;

  std::shared_ptr<std::thread>                    audioThread;

  agora::agora_refptr<agora::rtc::ILocalAudioTrack> audioTrack;
  agora::agora_refptr<agora::rtc::ILocalVideoTrack> videoTrack;

  bool                                            isRunning;

  AgoraDecoder_ptr                                videoDecoder;
  AgoraEncoder_ptr                                videoEncoder;


  bool                                            encodeNextFrame;
  bool                                            enable_dual;
  agora_log_func_t                                log_func;
  void                                            *log_ctx;

  std::shared_ptr<MyUserObserver>                 localUserObserver;

  uint8_t                                         fps;
  float                                         predictedFps;

  TimePoint                                       lastFpsPrintTime; 
  TimePoint                                       lastBufferingTime;

  uint16_t                                        jb_size; 

  LocalConfig_ptr                                 callConfig;

  uint64_t                                        reBufferingCount;    

  long                                            lastFrameTimestamp; 
  long                                            timestampPerSecond;         
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

   bool                  is_finished;
   
};

//threads
static void VideoThreadHandlerHigh(agora_context_t* ctx);
static void VideoThreadHandlerLow(agora_context_t* ctx);
static void AudioThreadHandler(agora_context_t* ctx);


//do not use it before calling agora_init
void agora_log(agora_context_t* ctx, const char* message){
   ctx->log_func(ctx->log_ctx, message);
}

std::string GetAddressAsString(agora_context_t* ctx){
   return std::to_string((long)(ctx))+": ";
}

//helper function for creating a service
agora::base::IAgoraService* createAndInitAgoraService(bool enableAudioDevice,
                                                      bool enableAudioProcessor,
						      bool enableVideo,
						      bool stringUserid,
						      bool enableEncryption, const char* appid) {
  auto service = createAgoraService();
  agora::base::AgoraServiceConfiguration scfg;
  scfg.enableAudioProcessor = enableAudioProcessor;
  scfg.enableAudioDevice = enableAudioDevice;
  scfg.enableVideo = enableVideo;
  scfg.useStringUid=stringUserid;
  if (enableEncryption) {
    scfg.appId = appid;
  }

  int ret = service->initialize(scfg);
  return (ret == agora::ERR_OK) ? service : nullptr;
}

bool isNumber(const std::string& userIdString)
{
    return !userIdString.empty() && std::find_if(userIdString.begin(), userIdString.end(), [](unsigned char ch)
		    { return !std::isdigit(ch); }) == userIdString.end();
}

#define ENC_KEY_LENGTH        128
agora_context_t*  agora_init(char* in_app_id, char* in_ch_id, char* in_user_id, bool enable_enc,
		                         short enable_dual, unsigned int  dual_vbr, 
			                       unsigned short  dual_width, unsigned short  dual_height,
                             unsigned short min_video_jb){

  //rotate log file if necessary
  CheckAndRollLogFile();

  agora_context_t* ctx=new agora_context_t;

  std::string app_id(in_app_id);
  std::string chanel_id=(in_ch_id);
  std::string user_id(in_user_id);
  std::string proj_appid = "abcd";
  char encryptionKey[ENC_KEY_LENGTH] = "";

  //set configuration
  ctx->config.clientRoleType = agora::rtc::CLIENT_ROLE_BROADCASTER;
  ctx->config.channelProfile = agora::CHANNEL_PROFILE_LIVE_BROADCASTING;
  ctx->config.autoSubscribeAudio = false;
  ctx->config.autoSubscribeVideo = false;

  ctx->enable_dual=enable_dual;

  //create a local config
  ctx->callConfig=std::make_shared<LocalConfig>();
  ctx->callConfig->loadConfig("/usr/local/nginx/conf/rtmpg.conf");
  ctx->callConfig->print();

  ctx->jb_size=ctx->callConfig->getInitialJbSize();

  if (enable_enc) {
    std::ifstream inf;
    std::string temp_str;
    inf.open("/tmp/nginx_agora_appid.txt",std::ifstream::in);
    if(!inf.is_open()){
      logMessage("agora Failed to open AppId and key for encryption!");
    }
    getline(inf, proj_appid);
    //logMessage(proj_appid.c_str());
    getline(inf, temp_str);
    //logMessage(temp_str.c_str());
    inf.close();
    int str_leng = temp_str.copy(encryptionKey, temp_str.length());
    encryptionKey[str_leng] = '\0';
  }

  // Create Agora service
  if(user_id=="" ||  isNumber(user_id)){
      logMessage("numeric  user id: "+ user_id);
      ctx->service = createAndInitAgoraService(false, true, true,  false, enable_enc, proj_appid.c_str());
  }
  else{
     logMessage("string user id: "+ user_id);
     ctx->service = createAndInitAgoraService(false, true, true,  true, enable_enc, proj_appid.c_str());
  }

  if (!ctx->service) {
    delete ctx;
    return NULL;
  }

  ctx->connection =ctx->service->createRtcConnection(ctx->config);
  if (!ctx->connection) {
     delete ctx;
     return NULL;
  }

  // open the encryption mode
  if ( ctx->connection && enable_enc) {
    agora::rtc::EncryptionConfig Config;
    Config.encryptionMode = agora::rtc::SM4_128_ECB;  //currently only this mode is supported
    Config.encryptionKey = encryptionKey;

    if (ctx->connection->enableEncryption(true, Config) < 0) {
      logMessage("agora Failed to enable Encryption!");
      delete ctx;
      return NULL;
    } else {
      logMessage("agora built-in encryption enabled!");
    }
  }

  // Connect to Agora channel
  auto  connected =  ctx->connection->connect(app_id.c_str(), chanel_id.c_str(), user_id.c_str());
  if (connected) {
     delete ctx;
     return NULL;
  }

  //ctx->localUserObserver = std::make_shared<MyUserObserver>(ctx->connection->getLocalUser());

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
  options.ccMode=agora::base::CC_ENABLED;  //for send_dual_h264
  ctx->videoTrack =ctx->service->createCustomVideoTrack(ctx->videoSender,options);
  if (!ctx->videoTrack) {
    return NULL;
  }

  // Set the dual_model
  agora::rtc::SimulcastStreamConfig Low_streamConfig;
  ctx->videoTrack->enableSimulcastStream(true, Low_streamConfig);

  // Publish audio & video track
  ctx->connection->getLocalUser()->publishAudio(ctx->audioTrack);
  ctx->connection->getLocalUser()->publishVideo(ctx->videoTrack);

  ctx->isConnected=1;
  
  //create queues
  ctx->videoQueueHigh=std::make_shared<WorkQueue <Work_ptr> >();
  ctx->audioQueue=std::make_shared<WorkQueue <Work_ptr> >();

  ctx->isRunning=true;

  //start thread handlers
  ctx->videoThreadHigh=std::make_shared<std::thread>(&VideoThreadHandlerHigh,ctx);
  ctx->audioThread=std::make_shared<std::thread>(&AudioThreadHandler,ctx);
 
  //is dual streaming is enabled 
  if(ctx->enable_dual){
      //create video encoder/decoder
      ctx->videoDecoder=std::make_shared<AgoraDecoder>();
      ctx->videoEncoder=std::make_shared<AgoraEncoder>(dual_width,dual_height,dual_vbr);
      if(!ctx->videoDecoder->init() || ! ctx->videoEncoder->init()){
         return NULL;
      }

      ctx->videoQueueLow=std::make_shared<WorkQueue <Work_ptr> >();
      ctx->videoThreadLow=std::make_shared<std::thread>(&VideoThreadHandlerLow,ctx);
  }


  ctx->encodeNextFrame=true;

  ctx->fps=0;
  ctx->lastFpsPrintTime=Now();
  ctx->lastBufferingTime=Now();
  ctx->reBufferingCount=0;

  ctx->lastFrameTimestamp=0; 
  ctx->timestampPerSecond=0;

  //initially set to 30fps
  ctx->predictedFps=30;

  return ctx;
}


bool doSendLowVideo(agora_context_t* ctx, const unsigned char* buffer,  unsigned long len,int is_key_frame){

  const uint32_t MAX_FRAME_SIZE=1024*1000;
  uint8_t transcodingBuffer[MAX_FRAME_SIZE];

  auto frameType=agora::rtc::VIDEO_FRAME_TYPE_DELTA_FRAME;
  if(is_key_frame){
     frameType=agora::rtc::VIDEO_FRAME_TYPE_KEY_FRAME;
  }


  //decoder and rencode video here
  uint32_t trancodingBufferSize=0;
  ctx->videoDecoder->decode(buffer, len, transcodingBuffer, trancodingBufferSize);
  auto frame=ctx->videoDecoder->getFrame();

  //check if we need to skip some frames
  if( ctx->encodeNextFrame==true || is_key_frame){
    //reencode
    trancodingBufferSize=0;
    ctx->videoEncoder->encode(frame, transcodingBuffer, trancodingBufferSize,is_key_frame);
  
    agora::rtc::EncodedVideoFrameInfo videoEncodedFrameInfo;
    videoEncodedFrameInfo.rotation = agora::rtc::VIDEO_ORIENTATION_0;
    videoEncodedFrameInfo.codecType = agora::rtc::VIDEO_CODEC_H264;
    videoEncodedFrameInfo.framesPerSecond = 15;
    videoEncodedFrameInfo.frameType = frameType;

    videoEncodedFrameInfo.streamType = agora::rtc::VIDEO_STREAM_LOW;
    ctx->videoSender->sendEncodedVideoImage(transcodingBuffer,trancodingBufferSize,videoEncodedFrameInfo);

  }

  ctx->encodeNextFrame= !ctx->encodeNextFrame;

  return true;

}

bool doSendHighVideo(agora_context_t* ctx, const unsigned char* buffer,  unsigned long len,int is_key_frame){

  auto frameType=agora::rtc::VIDEO_FRAME_TYPE_DELTA_FRAME; 
  if(is_key_frame){
     frameType=agora::rtc::VIDEO_FRAME_TYPE_KEY_FRAME;
  }

  agora::rtc::EncodedVideoFrameInfo videoEncodedFrameInfo;
  videoEncodedFrameInfo.rotation = agora::rtc::VIDEO_ORIENTATION_0;
  videoEncodedFrameInfo.codecType = agora::rtc::VIDEO_CODEC_H264;
  videoEncodedFrameInfo.framesPerSecond = 30;
  videoEncodedFrameInfo.frameType = frameType;
  videoEncodedFrameInfo.streamType = agora::rtc::VIDEO_STREAM_HIGH;

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
int agora_send_video(agora_context_t* ctx,
                    const unsigned char * buffer,  
                    unsigned long len,
                    int is_key_frame,
                    long timestamp){

   auto     timePerFrame=(1000/(float)ctx->predictedFps);
   Work_ptr work=std::make_shared<Work>(buffer,len, is_key_frame);
   uint16_t maxFps=200;
   
   ctx->videoQueueHigh->add(work);
   if(ctx->enable_dual){
      ctx->videoQueueLow->add(work);
   }

   if(ctx->lastFrameTimestamp!=0){

     auto diff=timestamp-ctx->lastFrameTimestamp;
     ctx->timestampPerSecond +=diff;
   }
   ctx->lastFrameTimestamp=timestamp;

   //calculate fps stuffs
   ctx->fps +=1;
   if(ctx->callConfig->useFpsLog() &&
      GetTimeDiff(ctx->lastFpsPrintTime, Now())>=1000){

      float averageTimeStamp=ctx->timestampPerSecond/((float)ctx->fps);

      //safty check when  averageTimeStamp is not correct
      if(averageTimeStamp>1000/maxFps){ 
        ctx->predictedFps=1000/averageTimeStamp;
      }

      logMessage(GetAddressAsString(ctx)+"AGORA-JITTER: buffer size (ms): " +
                           std::to_string((int)(ctx->videoQueueHigh->size()*timePerFrame)) +
                            "/"+
                           std::to_string(ctx->jb_size) +
                           ", fps="+std::to_string(ctx->fps)+
                           ", predicted fps="+ std::to_string(ctx->predictedFps));

       ctx->lastFpsPrintTime=Now();
       ctx->fps=0;

      ctx->timestampPerSecond=0;
   }

   return 0; //no errors
}

struct PacerInfo {
  int sendTimes;
  int sendIntervalInMs;
  std::chrono::steady_clock::time_point startTime;
};

void waitBeforeNextSend(PacerInfo& pacer) {
  auto sendFrameEndTime = std::chrono::steady_clock::now();
  int nextDurationInMs = (++pacer.sendTimes * pacer.sendIntervalInMs);
  int waitIntervalInMs = nextDurationInMs - std::chrono::duration_cast<std::chrono::milliseconds>(
                                                sendFrameEndTime - pacer.startTime)
                                                .count();
  if (waitIntervalInMs > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(waitIntervalInMs));
  }
}

static TimePoint GetNextSamplingPoint(agora_context_t* ctx,uint8_t& currentFramePerSecond){
  
  //error in fps prediction (in ms)
  auto   timePerFrame=std::ceil((1000/(float)ctx->predictedFps));
  auto   samplingFrequency=(long)(timePerFrame);

  //if we have more video frames in the queue, decrease sending time a bit
  if((ctx->videoQueueHigh->size()-1)*timePerFrame>ctx->jb_size){

      samplingFrequency = (long)(samplingFrequency*2/3.0);
      logMessage(GetAddressAsString(ctx)+"speeding up frame consumption by 1/3: " +
                   std::to_string((int)(ctx->videoQueueHigh->size()*timePerFrame))+
                                   "/"+
                                   std::to_string(ctx->jb_size)
                 );
  }

  TimePoint  nextSample = Now()+std::chrono::milliseconds(samplingFrequency);
  
   return nextSample;
}

void WaitForBuffering(agora_context_t* ctx){

  const uint8_t MAX_ITERATIONS=200;
  const int     waitTimeForBufferToBeFull=33; //ms
  auto          timePerFrame=(1000/(float)ctx->predictedFps);

  bool buffered=false;

  //wait untill the buffer has min buffer size again
  int waitForBufferingCount=0;
  while(ctx->isRunning==true &&
        waitForBufferingCount<MAX_ITERATIONS && 
        (ctx->videoQueueHigh->size()*timePerFrame)<ctx->jb_size){

      std::this_thread::sleep_for(std::chrono::milliseconds(waitTimeForBufferToBeFull));
	    logMessage(GetAddressAsString(ctx)+"Video-JB: buffering ("+
                                   std::to_string((int)(ctx->videoQueueHigh->size()*timePerFrame))+
                                   "/"+
                                   std::to_string(ctx->jb_size)+
                                  ") ...");

      waitForBufferingCount++;
      buffered=true;
  }
 
  if(buffered){
    logMessage(GetAddressAsString(ctx)+"Video-JB: ----- end buffering -----");
  }
}

void ResizeBuffer(agora_context_t* ctx){

   const uint8_t MAX_REBUFFERING_COUNT=1;  

   //double JB size if the buffer  keeps running out to frquently
   auto diffBufferingTime=GetTimeDiff(ctx->lastBufferingTime, Now());
   if(ctx->reBufferingCount>=MAX_REBUFFERING_COUNT &&   //just ignore the first buffering, which might not be real
        diffBufferingTime<ctx->callConfig->getTimeToIncreaseJbSize()*1000){

        ctx->jb_size = ctx->jb_size*2;

        logMessage(GetAddressAsString(ctx)+"Video-JB: JB size increased to "+
             std::to_string(ctx->jb_size)+", because it runs out too frequently");
     }

     ctx->lastBufferingTime=Now();
     ctx->reBufferingCount++;
}

/*
 * buffer size doubling logic;
     input: jb-initial-size-ms -- intial buffer size in ms
            jb-max-size-ms     -- max buffer size in ms
            Jb-max-doubles-if-emptied-within-seconds 
                 -- time limit where buffer size is doupled if it becomes empty within it
      logic:
            jb-size=jb-initial-size-ms
            while (running)
               if no frames arrived withing the last 3 seconds and the buffer is empty
                  set require-buffering=true
                if require-buffering 
                   wait for the buffer to be filled up for max 200 iterations 
                   if last-buffering-time < Jb-max-doubles-if-emptied-within-seconds 
                                               AND jb-size <jb-max-size-ms
                       jb-size =jb-size*2


 */
void CheckAndFillInVideoJb(agora_context_t* ctx, const TimePoint& lastSendTime){

 bool  waitForBufferToBeFull=false;

  //check if no frames arrive for 3 seconds. If so, fill the buffer with frames
  auto diff=GetTimeDiff(lastSendTime, Now());
  if(diff>3*ctx->predictedFps && 
     ctx->videoQueueHigh->isEmpty()){

	    waitForBufferToBeFull=true;
  }

  //should wait for the buffer to have min size
  if(waitForBufferToBeFull==true ){
      WaitForBuffering(ctx);

      if(ctx->jb_size<ctx->callConfig->getMaxJbSize()){
         ResizeBuffer(ctx);
      }
  }
}

static void VideoThreadHandlerHigh(agora_context_t* ctx){

   TimePoint  lastSendTime=Now();
   uint8_t currentFramePerSecond=0;

   while(ctx->isRunning==true){

     auto   timePerFrame=(1000/(float)ctx->predictedFps);

     TimePoint  nextSample = GetNextSamplingPoint(ctx,currentFramePerSecond);

     //check if we need to fill JB
     CheckAndFillInVideoJb(ctx, lastSendTime);
     if(ctx->callConfig->useDetailedVideoLog()){

        logMessage(GetAddressAsString(ctx)+"AGORA-JITTER: buffer size: " +
                           std::to_string(ctx->videoQueueHigh->size()*timePerFrame));
     }

     lastSendTime=Now();

     //wait untill work is available
     ctx->videoQueueHigh->waitForWork();	  
     Work_ptr work=ctx->videoQueueHigh->get();

     if(work==nullptr) continue;
     if(work->is_finished==1) break;

     doSendHighVideo(ctx,work->buffer, work->len, (bool)(work->is_key_frame));

     //sleep until our next frame time
     std::this_thread::sleep_until(nextSample);
  }

  logMessage("VideoThreadHandlerHigh ended");
}

static void VideoThreadHandlerLow(agora_context_t* ctx){

   PacerInfo pacer = {0, (int)(1000 / (float)ctx->predictedFps),std::chrono::steady_clock::now()};

   while(ctx->isRunning==true){

     //wait untill work is available
     ctx->videoQueueLow->waitForWork();
     Work_ptr work=ctx->videoQueueLow->get();
     if(work==NULL) continue;

     if(work->is_finished==1){
        break;
     }

     doSendLowVideo(ctx,work->buffer, work->len, (bool)(work->is_key_frame));

     waitBeforeNextSend(pacer);  // sleep for a while before sending next frame
  }

  logMessage("VideoThreadHandlerLow ended");
}

static void AudioThreadHandler(agora_context_t* ctx){

  float const audioSamplingRateKz=48;
   PacerInfo pacer = {0, (int)((1000 /audioSamplingRateKz)*0.95),std::chrono::steady_clock::now()};
   
   while(ctx->isRunning==true){

     //wait untill work is available
     ctx->audioQueue->waitForWork();
     Work_ptr work=ctx->audioQueue->get();
     if(work==NULL) continue;

     if(work->is_finished){
        return;
     }

     doSendAudio(ctx,work->buffer, work->len);

     waitBeforeNextSend(pacer);
  }
}
void agora_disconnect(agora_context_t** ctx){

  logMessage("start agora disonnect ...");

  auto tempCtx=(*ctx);

  tempCtx->isRunning=false;
   //tell the thread that we are finished
    Work_ptr work=std::make_shared<Work>(nullptr,0, false);
    work->is_finished=true;

   tempCtx->videoQueueHigh->add(work);
   if(tempCtx->enable_dual){
       tempCtx->videoQueueLow->clear();
       tempCtx->videoQueueLow->add(work);
   }
   tempCtx->audioQueue->add(work);

   std::this_thread::sleep_for(std::chrono::seconds(2));

   tempCtx->connection->getLocalUser()->unpublishAudio(tempCtx->audioTrack);
   tempCtx->connection->getLocalUser()->unpublishVideo(tempCtx->videoTrack);

   bool  isdisconnected=tempCtx->connection->disconnect();
   if(isdisconnected){
      return;
   }


   tempCtx->audioSender = nullptr;
   tempCtx->videoSender = nullptr;
   tempCtx->audioTrack = nullptr;
   tempCtx->videoTrack = nullptr;


   tempCtx->videoQueueHigh=nullptr;;
   tempCtx->videoQueueLow=nullptr;
   tempCtx->audioQueue=nullptr;;

   //delete context
   tempCtx->connection=nullptr;

   tempCtx->service->release();
   tempCtx->service = nullptr;
  
   tempCtx->videoThreadHigh->detach();
   if(tempCtx->enable_dual){
      tempCtx->videoThreadLow->detach();
   }
   tempCtx->audioThread->detach();

   tempCtx->videoThreadHigh=nullptr;;
   tempCtx->videoThreadLow=nullptr;
   tempCtx->audioThread=nullptr; 

   tempCtx->videoDecoder=nullptr;
   tempCtx->videoEncoder=nullptr;

   delete (*ctx);

   logMessage("Agora disonnected ");
}

int agora_send_audio(agora_context_t* ctx,const unsigned char * buffer,  unsigned long len){

    Work_ptr work=std::make_shared<Work>(buffer,len, 0);
    ctx->audioQueue->add(work);

    return 0;
}

void agora_set_log_function(agora_context_t* ctx, agora_log_func_t f, void* log_ctx){

    ctx->log_func=f;
    ctx->log_ctx=log_ctx;
}

void agora_log_message(agora_context_t* ctx, const char* message){

   if(ctx->callConfig->useDetailedAudioLog()){
      logMessage(std::string(message));
   }
}