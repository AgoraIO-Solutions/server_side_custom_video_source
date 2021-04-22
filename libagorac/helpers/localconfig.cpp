
#include "localconfig.h"
#include "agoralog.h"
#include <algorithm>

#include <mutex>

std::mutex g_config_file_mutex;

LocalConfig::LocalConfig():
_useDetailedVideoLog(false),
_useDetailedAudioLog(false),
_useFpsLog(false),
_jbSize(4),
_dynamicBufferChangeTime(15),
_dynamicBufferChangeFrames(16)  //500/30
{

}

 bool LocalConfig::loadConfig(const std::string& filePath){

  std::lock_guard<std::mutex> guard(g_config_file_mutex);

  std::ifstream configFile(filePath, std::ios::in);
  if(!configFile.is_open()){
	 logMessage("Not able to open rtmp config file: "+filePath);
     logMessage("Default configs will be used!");
	 return false;
  }

   return readConfig(configFile);
 }

 bool LocalConfig::readConfig(std::ifstream& file){

  char buffer[1024];

  std::string key;
  std::string value;

  while(!file.eof()){
	  
	  file.getline(buffer, 1024);

      std::string line(buffer);
      line.erase(remove_if(line.begin(), line.end(), isspace), line.end());

      //ignore empty lines
      if(line.empty()) continue;

	  if(!getKeyValue(line, key, value)){
		  return false;
	  }

      if(key=="detailed-video-log" && value=="yes"){
         _useDetailedVideoLog=true;
      }
      else if(key=="detailed-audio-log" && value=="yes"){
         _useDetailedAudioLog=true;
      }
      else if(key=="fps-log" && value=="yes"){
         _useFpsLog=true;
      }
      else if(key=="jb-size"){
         _jbSize=std::atoi(value.c_str());
      }
      else if(key=="buffer-increase-time"){
         _dynamicBufferChangeTime=std::atoi(value.c_str());
      }
      else if(key=="buffer-increase-frames"){
         _dynamicBufferChangeFrames=std::atoi(value.c_str());
      }
  }

  //validate user input
  if(_jbSize<0 || _jbSize>32){
      _jbSize=4;  //default
  }

  return true;
 }

 bool LocalConfig::getKeyValue(const std::string& line,std::string& key, std::string& value){
	
  auto ret=line.find("=");
  if(ret!=std::string::npos){
     key=line.substr(0, ret);
     value=line.substr(ret+1, line.length());
     return true;
  }
	
  return false;
}

 void LocalConfig::print(){
    
   logMessage("Will use the following config for this call: ");

   logMessage("Detailed video log: "+getStringfromBool(_useDetailedVideoLog));
   logMessage("Detailed audio log: "+getStringfromBool(_useDetailedAudioLog));
   logMessage("FPS log: "+getStringfromBool(_useFpsLog));

   logMessage("JB size: "+std::to_string(_jbSize));

   logMessage("buffer-increase-time: "+std::to_string(_dynamicBufferChangeTime));
   logMessage("buffer-increase-frames: "+std::to_string(_dynamicBufferChangeFrames));
 }

 std::string LocalConfig::getStringfromBool(const bool& flag){
    
    std::string ret=flag==true? "yes": "no";

    return ret;
 }