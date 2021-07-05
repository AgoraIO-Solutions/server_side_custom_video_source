
1. [Overview](#overview)
2. [Development Setup](#develop)
3. [Binary Creation, Distribution & Installation](#binary)
4. [Usage](#usage)

## Overview <a name="overview"></a>
This project extends the NGINX RTMP module, enabling it to publish RTMP audio and video directly into an Agora channel using the Agora Linux SDK. 
We refer to this product as the RTMP Gateway or RTMPG. It is has been well used in production by some customers with OBS and Vmix. It supports Dual Stream such that a single RTMP stream can result in a low and high quality stream available on Agora.

For more information on the NGINX RTMP module see 
      https://www.nginx.com/blog/video-streaming-for-remote-learning-with-nginx/
      
      
## Development Setup <a name="develop"></a>
The following steps assume you are using Ubuntu and have been verified with Ubuntu 18.04 on an AWS t3.medium instance where the publication of a single RTMP stream consumed less than 10% of one CPU core. 

It is also possible to perform all of the steps below using ./server_side_custom_video_source/setup.sh after cloning this repo. 

(1) install the required libs

      $ sudo apt update
      $ sudo apt install build-essential git libpcre3 libpcre3-dev zlib1g zlib1g-dev libssl-dev unzip
      $ sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libx264-dev nasm libavfilter-dev libopus-dev

(2) create a directory to build the custom NGINX in

      $ mkdir custom-ngnix

(3) clone this current repo into the directory

(4) get a recent copy of the Agora Linux SDk (Nov 2020 onwards) and unzip it into the directory

(5) clone the nginx and nginx-rtmp-module repos into the directory

      $ git clone https://github.com/arut/nginx-rtmp-module.git
      $ git clone https://github.com/nginx/nginx.git 

   now, you should have the following dir structure
   + /custom-ngnix
      + nginx
      + server_side_custom_video_source
      + nginx-rtmp-module
      + custom-ngnix/agora_rtc_sdk

(6) install Agora C wrapper library and AgoraSDK

      $ cd server_side_custom_video_source/libagorac
      $ sudo ./install.sh /home/ubuntu/custom-ngnix/custom-ngnix/agora_rtc_sdk
      $ cd ..

(7) copy ngx_agora_helper.c and  ngx_agora_helper.h to nginx-rtmp-module

      $ cp ngx_agora_helper.c  ../nginx-rtmp-module/
      $ cp ngx_agora_helper.h  ../nginx-rtmp-module/
   
(8) copy nginx and nginx-rtmp-module patches

      $ cp patches/nginx.patch ../nginx
      $ cp patches/nginx-rtmp-module.patch ../nginx-rtmp-module/

(9) apply the patches 

      $ cd ../nginx
      $ patch -f -p 1 < nginx.patch
      $ cd ../nginx-rtmp-module/
      $ patch -f -p 1 < nginx-rtmp-module.patch
      $ cd ../nginx

(10) compile and install

      $ ./auto/configure --add-module=../nginx-rtmp-module 
      $ make 
      $ sudo make install
   
(11) configure

      $ sudo vi /usr/local/nginx/conf/nginx.conf
      
      # add the following block to the NGINX config file (above the http block is fine)
      rtmp {
          server {
              listen 1935;
              application live {
                      live on;
                      interleave on;
                      record all;
                      record_path /tmp/rec;
              }
          }
      }

      To enable media log: add this line to the config:
      error_log logs/error.log debug;
      
      $ sudo mkdir /tmp/rec; sudo chmod 777 /tmp/rec;
   
(12) run

      $ sudo /home/ubuntu/custom-ngnix/nginx/objs/nginx;  

The NGINX recording module will now convert an inbound RTMP bitstream and send the audio and video to Agora

No recordings will be written to disk but it was still necessary to create the folder /tmp/rec and give it read/write permission

### Install a development version using the setup script
After cloning the project into your current folder
 $ cd server_side_custom_video_source
 $ sudo ./setup.sh /path/to/the/current/folder /path/to/build/at 

 Example:
 $ ./setup.sh /home/ubuntu/server_side_custom_video_source /home/ubuntu

## Binary Creation, Distribution & Installation <a name="binary"></a>

  Build a binary distribution package by following these steps:

   $ cd /your/dir/to/server_side_custom_video_source
   $ sudo ./build.sh

   This will create dist-agora-rtmp.tar.gz which you can use to install the RTMPG on another server

## Install a binary distribution package 

  Copy dist-agora-rtmp.tar.gz to the destination server and run the following:

  (1) extract the binary distribution package

    $tar -xvzf dist-agora-rtmp.tar.gz 
    $cd dist-agora-rtmp

  (2) backup and install the binary package

    $ sudo ./install.sh

    This will collect the important files from previous installation and put them in ngnix-bk-xx, where xx is the installation date. 
    You can recover that later with the following:

   (3) recover prevous installation

    $ ./recover.sh /path/to/backup/dir

## Usage <a name="Usage"></a>

Publishing RTMP

      Set the RTMP URI to rtmp://server_ip:1935/live?appid=APP_ID_OR_TOKEN&channel=CHANNEL&uid=USER_ID&abr=50000
      
      appid can contain either the Agora App Id or an Agora Authentication Token
      
      channel is the Agora channel name

      uid is the agora user id (optional)
      
      abr is the 'audio bitrate' in bits/second. You should use 50000 for voice applications and 250000 for high definition music

      To enable media encryption, on your server add a file /tmp/nginx_agora_appid.txt. This file should have Agrora App Id and the encryption key. Currently only SM4_128_ECB is supported.

      sample nginx_agora_appid.txt:
      402xxxxxxxxxxxxxxxxxxxxxxxxxxxxx
      12345ABCDx

      add &enc=1 to enable encryption (&enc=0) to disable it (optional)
      

Using OBS

     In Settings > Stream set the Service to Custom and the Server to the RTMP URI described above
     
     In Settings > Output set the Keyframe Interval to be 4s and the Profile to be baseline.
     

You can now start streaming from OBS into Agora

You can view the stream inside Agora using the simple web demo here and setting the relevant appId and channel
https://agoraio-community.github.io/AgoraWebSDK-NG/demo/basicVideoCall/index.html


You can share the following usage instructions with customers:
https://tinyurl.com/rtmp2agora     
     
Logging 
     Log level is set in /usr/local/nginx/conf/nginx.conf
     default is 
     error_log logs/error.log notice;
     Can change notice to debug and then restart with 
         sudo /home/ubuntu/custom-ngnix/nginx/objs/nginx -s stop;  sudo /home/ubuntu/custom-ngnix/nginx/objs/nginx 
     log files are in /usr/local/nginx/conf and /tmp for agora logs
     
