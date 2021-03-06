#pragma once
#include <pebble.h>
  
typedef void (*SettingsClosedCallBack)();
typedef void (*SendToPhoneCallBack)();
typedef void (*ClearImageCallBack)();

typedef enum CursorSensitivity {
  CS_LOW = 1,
  CS_MEDIUM = 2,
  CS_HIGH = 3
} CursorSensitivity;

struct Settings_st {
  bool drawingcursor_on;
  bool backlight_alwayson;
  CursorSensitivity sensitivity;
  bool secondshake_clear;
  int eraser_width;
  int pen_width;
};

void show_settings(struct Settings_st *settings, SendToPhoneCallBack send_event, ClearImageCallBack clear_event, SettingsClosedCallBack settings_closed);
void hide_settings(void);