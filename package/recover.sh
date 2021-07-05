#!/bin/sh
  
if [ "$#" -ne 1 ]; then
        echo "Usage: $0 /path/to/backup/dir" >&2
  exit 1
fi

backup_dir=$1

NGNIX_DIR="/usr/local/nginx"

sudo /usr/bin/nginx -s stop
sleep 2

cp $backup_dir/nginx /usr/bin/nginx 
cp $backup_dir/rtmpg.conf $NGNIX_DIR/conf/rtmpg.conf 
cp $backup_dir/libagorac.so /usr/local/lib/libagorac.so
cp $backup_dir/libagora_rtc_sdk.so  /usr/local/lib/libagora_rtc_sdk.so 

sudo /usr/bin/nginx;
echo "@reboot   /usr/bin/nginx;"  | sudo crontab -
