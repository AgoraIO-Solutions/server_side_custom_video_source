#!/bin/sh
  
# this package installs a binary distribution of agora rtmp module

#install required libraries
sudo apt install -y libpcre3 libpcre3-dev zlib1g zlib1g-dev libssl-dev unzip
sudo apt install -y libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libx264-dev nasm libavfilter-dev libopus-dev

sudo apt install -y dpkg

#install binary files
dpkg -i package/agora-rtmp-stream.deb

#configure binray files
sudo ldconfig

#create and copy nginx configs
sudo mkdir -p /usr/local/nginx/logs/
sudo cp -r conf /usr/local/nginx/


sudo /usr/bin/nginx -s stop;  sudo /usr/bin/nginx;
echo "@reboot   /usr/bin/nginx;"  | sudo crontab -
