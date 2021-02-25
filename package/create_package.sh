#!/bin/bash

# setup.sh also creates the package
dpkg-deb --build source bin/agora-rtmp-stream.deb
