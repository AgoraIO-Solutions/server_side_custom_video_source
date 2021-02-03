//#define DEBUG_MODE 1

#include <fstream>
#include <chrono>
#include <sys/time.h>

void logMessage(const std::string& message){

#ifdef DEBUG_MODE

  std::ofstream file("/tmp/agora.log",std::ios::app);
  if(!file.is_open()){
     return;
  }

  auto now = std::chrono::system_clock::now();
  auto now_c = std::chrono::system_clock::to_time_t(now);
    
  struct timeval tv;
  gettimeofday(&tv, NULL);
    
  char buff[25];
  strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_c));
   
  char msBuffer[4];
  snprintf(msBuffer,4,"%3d",(int)(tv.tv_usec));

  file<<buff<<":"<<msBuffer<<": "<<message<<std::endl;
  file.close();

#endif	
  
}

