//#define DEBUG_MODE 1

#include <fstream>
#include <chrono>
#include <sys/time.h>
#include <mutex>

std::mutex g_mutex;

void logMessage(const std::string& message){

#ifdef DEBUG_MODE


  char buffer[30];
  struct timeval tv;

  time_t curtime;

  gettimeofday(&tv, NULL); 
  curtime=tv.tv_sec;
  strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
  int ms=(int)((tv.tv_usec)/1000);

  char fullTime[40];
  snprintf(fullTime, 40, "%s%3d",buffer, ms);

  std::lock_guard<std::mutex> guard(g_mutex);
  std::ofstream file("/tmp/agora.log",std::ios::app);
  if(!file.is_open()){
     return;
  }
  //file<<buff<<":"<<msBuffer<<": "<<message<<std::endl;
  file<<fullTime<<": "<<message<<std::endl;
  file.close();

#endif	
  
}

