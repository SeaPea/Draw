#pragma once
#include <pebble.h>

  
#define DEBUG
#ifndef DEBUG
#undef APP_LOG
#define APP_LOG(...)
#endif


#define IMG_WIDTH 144
#define IMG_HEIGHT 168
#define IMG_ROW_BYTES 20
#define IMG_PIXELS IMG_ROW_BYTES*IMG_HEIGHT
