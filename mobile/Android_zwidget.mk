
LOCAL_PATH := $(call my-dir)/../libraries/ZWidget/

include $(CLEAR_VARS)

LOCAL_MODULE := zwidget

LOCAL_C_INCLUDES :=     $(LOCAL_PATH)/include \
                        $(LOCAL_PATH)/include/zwidget \
                        $(LOCAL_PATH)/src \
                        $(SDL_INCLUDE_PATHS) \

LOCAL_CPPFLAGS :=  -std=c++17 -fexceptions -fpermissive -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -fsigned-char

LOCAL_SRC_FILES =  	\
	src/core/canvas.cpp \
	src/core/font.cpp \
	src/core/image.cpp \
	src/core/span_layout.cpp \
	src/core/timer.cpp \
	src/core/widget.cpp \
	src/core/utf8reader.cpp \
	src/core/pathfill.cpp \
	src/core/truetypefont.cpp \
	src/core/picopng/picopng.cpp \
	src/core/nanosvg/nanosvg.cpp \
	src/widgets/lineedit/lineedit.cpp \
	src/widgets/mainwindow/mainwindow.cpp \
	src/widgets/menubar/menubar.cpp \
	src/widgets/scrollbar/scrollbar.cpp \
	src/widgets/statusbar/statusbar.cpp \
	src/widgets/textedit/textedit.cpp \
	src/widgets/toolbar/toolbar.cpp \
	src/widgets/toolbar/toolbarbutton.cpp \
	src/widgets/imagebox/imagebox.cpp \
	src/widgets/textlabel/textlabel.cpp \
	src/widgets/pushbutton/pushbutton.cpp \
	src/widgets/checkboxlabel/checkboxlabel.cpp \
	src/widgets/listview/listview.cpp \
	src/widgets/tabwidget/tabwidget.cpp \
	src/window/window.cpp \



LOCAL_CFLAGS += -fvisibility=hidden -fdata-sections -ffunction-sections

include $(BUILD_STATIC_LIBRARY)








