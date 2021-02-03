export AGORA_SDK_DEMO_PATH=.
export DATA_PATH=$AGORA_SDK_DEMO_PATH/test_data/

echo sync-data: mkdir -p $DATA_PATH
mkdir -p $DATA_PATH

echo sync-data: get data
headers='--header=apikey:DHKG34j12F1sdfsdfijw --header=X-JFrog-Art-Api:AKCp5fUPFcG4rhD2UWa9TMA7Xvy1rwNK7dY4DuR3Vz7pyX9rYoFKB99xpGhBGtAEURXGZtNH8'
wget $headers https://artifactory.sh.agoralab.co/artifactory/SDK_repo/test_data/ehren-paper_lights-96.opus -P $DATA_PATH/
wget $headers https://artifactory.sh.agoralab.co/artifactory/SDK_repo/test_data/he_aac.aac -P $DATA_PATH/
wget $headers https://artifactory.sh.agoralab.co/artifactory/SDK_repo/test_data/test.aac -P $DATA_PATH/
wget $headers https://artifactory.sh.agoralab.co/artifactory/SDK_repo/test_data/test.vp8.ivf -P $DATA_PATH/
wget $headers https://artifactory.sh.agoralab.co/artifactory/SDK_repo/test_data/test.wav -P $DATA_PATH/
wget $headers https://artifactory.sh.agoralab.co/artifactory/SDK_repo/test_data/test_multi_slice.h264 -P $DATA_PATH/
wget $headers https://artifactory.sh.agoralab.co/artifactory/SDK_repo/test_data/stefan_cif.yuv -P $DATA_PATH/
wget $headers https://artifactory.sh.agoralab.co/artifactory/SDK_repo/test_data/test.pcm -P $DATA_PATH/