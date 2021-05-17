#!/bin/sh

#creating/updating  binary distribution 
cd package

#assuming nginx is installed on /home/ubuntu/custom-ngnix
cp /home/ubuntu/custom-ngnix/nginx/objs/nginx   source/usr/bin/
cp /usr/local/lib/libagora_rtc_sdk.so source/usr/local/lib/
cp /usr/local/lib/libagorac.so        source/usr/local/lib/
cp ../rtmpg.conf bin/conf/rtmpg.conf
dpkg-deb --build source bin/agora-rtmp-stream.deb
#cp bin/agora-rtmp-stream.deb  package/bin/

