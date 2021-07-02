#!/bin/sh

#creating/updating  binary distribution 
cd package

#assuming nginx is installed on /home/ubuntu/custom-ngnix
cp /home/ubuntu/custom-ngnix/nginx/objs/nginx   source/usr/bin/
cp /usr/local/lib/libagora_rtc_sdk.so source/usr/local/lib/
cp /usr/local/lib/libagorac.so        source/usr/local/lib/
cp ../rtmpg.conf bin/conf/rtmpg.conf
dpkg-deb --build source bin/agora-rtmp-stream.deb


#create a distribution package
DIST_DIR="dist-agora-rtmp"
cd ..
if [ -d "$DIST_DIR" ]; then rm -Rf $DIST_DIR; fi

mkdir $DIST_DIR
cp -r package $DIST_DIR/
cp install.sh $DIST_DIR/
cp recover.sh $DIST_DIR/

#move read me to $DIST_DIR to directry
mv $DIST_DIR/package/README $DIST_DIR

tar -czvf $DIST_DIR.tar.gz $DIST_DIR

