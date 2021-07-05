#!/bin/sh
  
# this package installs a binary distribution of agora rtmp module

#install required libraries
sudo apt update
sudo apt install -y libpcre3 libpcre3-dev zlib1g zlib1g-dev libssl-dev unzip
sudo apt install -y libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libx264-dev nasm libavfilter-dev libopus-dev

sudo apt install -y dpkg

NGNIX_DIR="/usr/local/nginx"
if [ -d "$NGNIX_DIR" ]; then

    backup_dir="ngnix-bk-$(date +'%m-%d-%Y')"
    echo backing up previous installation of ngnix to $backup_dir 
    
    #delete previous backup  directory if there is any with same name
    if [ -d "$backup_dir" ]; then rm -Rf $backup_dir; fi
   
    mkdir $backup_dir
    cp /usr/bin/nginx $backup_dir
    cp $NGNIX_DIR/conf/rtmpg.conf $backup_dir
    cp /usr/local/lib/libagorac.so $backup_dir
    cp /usr/local/lib/libagora_rtc_sdk.so $backup_dir
fi

#install binary files
dpkg -i package/bin/agora-rtmp-stream.deb

#configure binray files
sudo ldconfig

#create and copy nginx configs
sudo mkdir -p /usr/local/nginx/logs/
sudo cp -r package/bin/conf /usr/local/nginx/


sudo /usr/bin/nginx -s stop;  sudo /usr/bin/nginx;
echo "@reboot   /usr/bin/nginx;"  | sudo crontab -
