
LOCAL_PATH := $(call my-dir)/../libraries/lzma


include $(CLEAR_VARS)

LOCAL_MODULE    := lzma_gl3

LOCAL_CFLAGS = -Wall -fomit-frame-pointer -D_7ZIP_ST -DZ7_PPMD_SUPPORT

LOCAL_C_INCLUDES :=

LOCAL_SRC_FILES =  \
	C/7zAlloc.c \
	C/7zArcIn.c \
	C/7zBuf.c \
	C/7zBuf2.c \
	C/7zCrc.c \
	C/7zCrcOpt.c \
	C/7zDec.c \
	C/7zFile.c \
	C/7zStream.c \
	C/Alloc.c \
	C/Bcj2.c \
	C/Bcj2Enc.c \
	C/Bra.c \
	C/Bra86.c \
	C/CpuArch.c \
	C/Delta.c \
	C/DllSecur.c \
	C/LzFind.c \
	C/LzFindMt.c \
	C/LzFindOpt.c \
	C/Lzma2Dec.c \
	C/Lzma2DecMt.c \
	C/Lzma2Enc.c \
	C/LzmaDec.c \
	C/LzmaEnc.c \
	C/LzmaLib.c \
	C/MtCoder.c \
	C/MtDec.c \
	C/Ppmd7.c \
	C/Ppmd7Dec.c \
	C/Ppmd7Enc.c \
	C/Sha256.c \
	C/Sort.c \
	C/SwapBytes.c \
	C/Threads.c \
	C/Xz.c \
	C/XzCrc64.c \
	C/XzCrc64Opt.c \
	C/XzDec.c \
	C/XzEnc.c \
	C/XzIn.c

LOCAL_CFLAGS += -fvisibility=hidden -fdata-sections -ffunction-sections

include $(BUILD_STATIC_LIBRARY)








