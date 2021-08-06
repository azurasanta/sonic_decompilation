LOCAL_PATH := $(call my-dir)

OGG_DIR := dependencies/android/libogg
VORBIS_DIR := dependencies/android/libvorbis

OGG_INCLUDES    := $(LOCAL_PATH)/$(OGG_DIR)/include
VORBIS_INCLUDES := $(LOCAL_PATH)/$(VORBIS_DIR)/include \
	                 $(LOCAL_PATH)/$(VORBIS_DIR)/lib

######################################################################
# OGG
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE   := libogg
LOCAL_CFLAGS   := -ffast-math -fsigned-char -O2 -fPIC -DPIC \
                  -DBYTE_ORDER=LITTLE_ENDIAN -D_ARM_ASSEM_

LOCAL_C_INCLUDES := $(OGG_INCLUDES)

WILDCARD_SETUP := \
  $(wildcard $(LOCAL_PATH)/$(OGG_DIR)/src/*.c)


LOCAL_SRC_FILES := \
	$(subst jni/src/, , $(WILDCARD_SETUP))

include $(BUILD_STATIC_LIBRARY)

######################################################################
# VORBIS
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE   := libvorbis
LOCAL_CFLAGS   := -ffast-math -fsigned-char -O2 -fPIC -DPIC \
                  -DBYTE_ORDER=LITTLE_ENDIAN -D_ARM_ASSEM_

LOCAL_C_INCLUDES := $(OGG_INCLUDES) $(VORBIS_INCLUDES)

WILDCARD_SETUP := \
  $(wildcard $(LOCAL_PATH)/$(VORBIS_DIR)/lib/*.c)
FILTERED := $(filter-out $(LOCAL_PATH)/$(VORBIS_DIR)/lib/psytune.c, $(WILDCARD_SETUP))

LOCAL_SRC_FILES := \
	$(subst jni/src/, , $(FILTERED))

include $(BUILD_STATIC_LIBRARY)

######################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL

LOCAL_CFLAGS   := -fexceptions

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/$(SDL_PATH)/include \
    $(LOCAL_PATH)/dependencies/all \
    $(LOCAL_PATH)/$(VORBIS_DIR)/include \
    $(LOCAL_PATH)/$(OGG_DIR)/include


WILDCARD_SETUP := \
  $(wildcard $(LOCAL_PATH)/RSDKv4/*.cpp)

LOCAL_SRC_FILES := $(subst jni/src/, , $(WILDCARD_SETUP))

LOCAL_SHARED_LIBRARIES := SDL2 libogg libvorbis

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

include $(BUILD_SHARED_LIBRARY)
