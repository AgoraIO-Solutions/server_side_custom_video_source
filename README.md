
(1) create a directory to hold you installation:

   $ mkdir test-ngnix

(2) clone nginx-agora inside it

(3) get a recent copy of AgoraSDk and unzip it in this dir (or somewhere else)

(4) clone nginx and nginx-rtmp-module
   $ git clone https://github.com/arut/nginx-rtmp-module.git
   $ git clone https://github.com/nginx/nginx.git 

   now, you should have the following dir structure
   + /test-ngnix
      + nginx
      + nginx-agora
      + nginx-rtmp-module
      + AgoraSDK

(5) install Agora C wrapper library and AgoraSDK
   $ cd nginx-agora/libagorac
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

(9) compile and run
   $ ./auto/configure --add-module=../nginx-rtmp-module 
   $ make 
   $ sudo make insatll
   





Add this block to the NGINX config. The recording module will convert the bitstream and send to Agora.

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
       
