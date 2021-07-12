#!/bin/sh

#creating/updating  binary distribution 
cd package

#assuming nginx is run from inside an install dir (e.g., /home/ubuntu/custom-ngnix/server_side_custom_video_source)
cp ../../nginx/objs/nginx   source/usr/bin/

mkdir -p source/usr/local/lib/

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
mv $DIST_DIR/package/install.sh $DIST_DIR/
mv $DIST_DIR/package/recover.sh $DIST_DIR/

#move read me to $DIST_DIR to directry
mv $DIST_DIR/package/README $DIST_DIR

tar -czvf $DIST_DIR.tar.gz $DIST_DIR

#remove not needed temp dir
rm -rf $DIST_DIR

RELEASE_DIR=release

if [ -d "$RELEASE_DIR" ]; then rm -Rf $RELEASE_DIR; fi

mkdir $RELEASE_DIR

mv $DIST_DIR.tar.gz $RELEASE_DIR

echo "release is written to $RELEASE_DIR/$DIST_DIR.tar.gz"

