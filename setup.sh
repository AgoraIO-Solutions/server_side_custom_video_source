#!/bin/sh


if [ "$#" -ne 2 ]; then
	echo "Usage: $0 /path/to/this/repo /path/to/build/at " >&2
	echo "e.g. ./setup.sh /home/forte/server_side_custom_video_source /home/forte" >&2
	echo "e.g. ./setup.sh /home/ubuntu/server_side_custom_video_source /home/ubuntu" >&2
  exit 1
fi

sudo apt update
sudo apt install -y build-essential git libpcre3 libpcre3-dev zlib1g zlib1g-dev libssl-dev unzip
sudo apt install -y libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libx264-dev nasm libavfilter-dev libopus-dev

cd $2

if  [ -e "$2/custom-ngnix" ]; then
  rm -rf "$2/custom-ngnix"
fi
if ! [ -e "$2/custom-ngnix" ]; then
  echo "creating $2/custom-ngnix" 
  mkdir "$2/custom-ngnix" 
fi


cd "$2/custom-ngnix" 
git clone https://github.com/arut/nginx-rtmp-module.git
git clone https://github.com/nginx/nginx.git

cp -r $1 $2/custom-ngnix
#cp $1/sdk/agora_linux.zip $2/custom-ngnix
cp $1/new-sdk/agora_rtc_sdk.zip $2/custom-ngnix
unzip $2/custom-ngnix/agora_rtc_sdk
cd $2/custom-ngnix/server_side_custom_video_source/libagorac
sudo ./install.sh $2/custom-ngnix/agora_rtc_sdk
cd $2/custom-ngnix/server_side_custom_video_source
cp ngx_agora_helper.c  ../nginx-rtmp-module/
cp ngx_agora_helper.h  ../nginx-rtmp-module/
cp patches/nginx.patch ../nginx
cp patches/nginx-rtmp-module.patch ../nginx-rtmp-module/
cd ../nginx
patch -f -p 1 < nginx.patch
cd ../nginx-rtmp-module/
patch -f -p 1 < nginx-rtmp-module.patch
cd ../nginx
./auto/configure --add-module=../nginx-rtmp-module
make
sudo make install
sudo cp $1/nginx.conf /usr/local/nginx/conf/nginx.conf
echo "@reboot   $2/custom-ngnix/nginx/objs/nginx;"  | sudo crontab -



echo "Clean up"
rm -f "$2/custom-ngnix/agora_linux.zip"
rm -rf "$2/custom-ngnix/server_side_custom_video_source"
rm -rf "$2/custom-ngnix/nginx-rtmp-module"
