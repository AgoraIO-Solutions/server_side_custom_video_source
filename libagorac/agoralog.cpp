//#define DEBUG_AGORA 1

#include <fstream>

void logMessage(const std::string& message){

#ifdef DEBUG_AGORA

  std::ofstream file("/tmp/agora.log",std::ios::app);
  if(!file.is_open()){
     return;
  }

  file<<message<<std::endl;
  file.close();

#endif	
  
}

