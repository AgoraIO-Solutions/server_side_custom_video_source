#ifndef _LOCAL_CONFIG_H_
#define _LOCAL_CONFIG_H_

#include <string>
#include <fstream>

class LocalConfig{

 public:

   LocalConfig();

   bool loadConfig(const std::string& filePath);

   bool     useDetailedVideoLog(){return _useDetailedVideoLog;}
   bool     useDetailedAudioLog(){return _useDetailedAudioLog;}
   bool     useFpsLog(){return _useFpsLog;}

   uint8_t  getJbSize(){return _jbSize;}

   void print();

protected:

  bool getKeyValue(const std::string& line,std::string& key, std::string& value);
  bool readConfig(std::ifstream& file);

  std::string getStringfromBool(const bool& flag);

 private:

    bool       _useDetailedVideoLog;
    bool       _useDetailedAudioLog;
    bool       _useFpsLog;
    uint8_t    _jbSize;
};

#endif
