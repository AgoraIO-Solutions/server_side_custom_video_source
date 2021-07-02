This project extends the NGINX RTMP module, enabling it to publish RTMP audio and video directly into an Agora channel.

For more information on the NGINX RTMP module see 
      https://www.nginx.com/blog/video-streaming-for-remote-learning-with-nginx/

The following steps assume you are using Ubuntu and have been verified with Ubuntu 18.04 on an AWS t3.medium instance where the publication of a single RTMP stream consumed less than 10% of one CPU core. 


## install a develpment version

 $ cd /your/dir/to/server_side_custom_video_source
 $ sudo ./setup.sh /path/to/this/repo /path/to/build/at 

 for example:
 $ ./setup.sh /home/ubuntu/server_side_custom_video_source /home/ubuntu

 ## build a binary distribution package 

   after you have installled a development version on a server, you can build a binary distribution  package by following these steps:

   $ cd /your/dir/to/server_side_custom_video_source
   $ sudo ./build.sh

   now, you will get dist-agora-rtmp.tar.gz which you can use to install the app on the user machine

## intall a binary distribution package on user machine

  you will need to copy the file dist-agora-rtmp.tar.gz  to the user machine, then follow these steps to install:

  (1) extract  the binary distribution package

    $tar -xvzf dist-agora-rtmp.tar.gz 
    $cd dist-agora-rtmp

  (2) install the binary package

    $ sudo ./install.sh

    install.sh will collect important files from previous installation and put them in ngnix-bk-xx, where xx is the installation date. You can recover that later if you would like

  (3) recover prevous installation

      $ ./recover.sh /path/to/backup/dir
