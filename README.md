This project will extend the NGINX RTMP module enabling it to publish RTMP video directly into an Agora channel.


(1) create a directory to build your custom NGINX in

      $ mkdir test-ngnix

(2) clone this repo into the directory

(3) get a recent copy of the AgoraSDk and unzip it in this dir (or somewhere else)

(4) clone the nginx and nginx-rtmp-module repos

      $ git clone https://github.com/arut/nginx-rtmp-module.git
      $ git clone https://github.com/nginx/nginx.git 

   now, you should have the following dir structure
   + /test-ngnix
      + nginx
      + server_side_custom_video_source
      + nginx-rtmp-module
      + AgoraSDK

(5) install Agora C wrapper library and AgoraSDK

      $ cd server_side_custom_video_source/libagorac
      $ sudo ./install.sh /path/to/agora/sdk
      $ cd ..

(6) copy ngx_agora_helper.c and  ngx_agora_helper.h to nginx-rtmp-module

      $ cp ngx_agora_helper.c  ../nginx-rtmp-module/
      $ cp ngx_agora_helper.h  ../nginx-rtmp-module/
   
(7) copy nginx and nginx-rtmp-module patches

      $ cp patches/nginx.patch ../nginx
      $ cp patches/nginx-rtmp-module.patch ../nginx-rtmp-module/

(8) apply the patches 

      $ cd ../nginx
      $ patch -f -p 1 < nginx.patch
      $ cd ../nginx-rtmp-module/
      $ patch -f -p 1 < nginx-rtmp-module.patch
      $ cd ../nginx

(9) Find and set the Agora AppId, Channel and optional Token in server_side_custom_video_source/libagorac/agorac.cpp as follows:

     std::string app_id="APPID";
     std::string chanel_id="CHANNEL";
     std::string user_id="";

(10) compile and run

      $ ./auto/configure --add-module=../nginx-rtmp-module 
      $ make 
      $ sudo make insatll
   

Add this block to the NGINX config. The recording module will now convert the bitstream and send to Agora.

      rtmp {
          server {
              listen 1935;
              application live {
                      live on;
                      interleave on;
                      record all;
                      record_path /tmp/rec;
                      record_unique off;
              }
          }
      }
       
