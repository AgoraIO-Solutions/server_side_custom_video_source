
CC = g++  # C compiler

FLAGS        = -std=c++11  -I${AGORA_SDK_DIR}/lib/linux/agora_media_sdk/low_level_api/include
CFLAGS       = -fPIC -g -std=c++11 -DEBUG_AGORA
LDFLAGS      = -shared

TARGET_LIB = libagorac.so
DESTDIR?=/usr/local/lib

all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS) agorac.cpp
	$(CC)  $(FLAGS) ${LDFLAGS} -o $(TARGET_LIB) agorac.cpp agoralog.cpp  $(CFLAGS)

clean:
	rm $(TARGET_LIB)

install:
	mv $(TARGET_LIB) ${DESTDIR}


