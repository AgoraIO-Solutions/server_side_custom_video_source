This project extends the NGINX RTMP module, enabling it to publish RTMP audio and video directly into an Agora channel.

For more information on the NGINX RTMP module see 
      https://www.nginx.com/blog/video-streaming-for-remote-learning-with-nginx/

The following steps assume you are using Ubuntu and have been verified with Ubuntu 18.04 on an AWS t3.medium instance where the publication of a single RTMP stream consumed less than 10% of one CPU core. 

(1) install the required libs

      $ sudo apt update
      $ sudo apt install build-essential git libpcre3 libpcre3-dev zlib1g zlib1g-dev libssl-dev
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
      + Agora_Native_SDK_for_Linux_x64_rel.v2.7.1.1_2815_FULL_20201114_0418

(6) install Agora C wrapper library and AgoraSDK

      $ cd server_side_custom_video_source/libagorac
      $ sudo ./install.sh /home/ubuntu/custom-ngnix/Agora_Native_SDK_for_Linux_x64_rel.v2.7.1.1_2815_FULL_20201114_0418
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
      
      $ sudo mkdir /tmp/rec; sudo chmod 777 /tmp/rec;
   
(12) run

      $ sudo /home/ubuntu/custom-ngnix/nginx/objs/nginx;  

The NGINX recording module will now convert an inbound RTMP bitstream and send the audio and video to Agora

No recordings will be written to disk but it was still necessary to create the folder /tmp/rec and give it read/write permission

Publishing RTMP

      Set the RTMP URI to rtmp://server_ip:1935/live?appid=APP_ID_OR_TOKEN&channel=CHANNEL&uid=USER_ID&abr=50000&end=true
      
      appid can contain either the Agora App Id or an Agora Authentication Token
      
      channel is the Agora channel name

      uid is agora user id (optional)
      
      abr is the 'audio bitrate' in bits/second. You should use 50000 for voice applications and 250000 for high definition music
      
      &end=true is required to terminate the params

Using OBS

     In Settings > Stream set the Service to Custom and the Server to the RTMP URI described above
     
     In Settings > Output set the Keyframe Interval to be 4s and the Profile to be baseline.
     

You can now start streaming from OBS into Agora

You can view the stream inside Agora using the simple web demo here and setting the relevant appId and channel

	https://webdemo.agora.io/agora-web-showcase/examples/Agora-Web-Tutorial-1to1-Web/

To stop NGINX 
     sudo /home/ubuntu/custom-ngnix/nginx/objs/nginx -s stop


