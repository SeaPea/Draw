#include <pebble.h>
#include <math.h>
#include "canvas.h"  
#include "infowin.h"
#include "settings.h"

#define HIGH_TILT 300
#define MEDIUM_TILT 400
#define LOW_TILT 500
  
#define FILTER_K 0.9

enum SettingKeys {
  DRAWINGCURSORON_KEY = 0,
  BACKLIGHTAWAYSON_KEY = 1,
  SENSITIVITY_KEY = 3,
  IMAGEDATA_START_KEY = 20
};

enum AppMsgKeys {
  IMAGE_DATA_SEND_KEY = 1
};

static bool s_baselined = false;
static int s_baseline_x;
static int s_baseline_y;
//static int s_baseline_z;
static int s_filtered_x;
static int s_filtered_y;
static int s_max_tilt;
static bool s_changed = false;

static bool s_sending_image = false;
static int s_chunk_pos;

static struct Settings_st s_settings;

static void accel_handler(AccelData *data, uint32_t num_samples) {
  
  bool has_valid_sample = true;
  int total_x = 0;
  int total_y = 0;
  //int total_z = 0;
  int avg_x = 0;
  int avg_y = 0;
  //int avg_z = 0;
  
  // Calculate totals in order to average accel in each axis for the sample size
  for (int i = 0; i < (int)num_samples; i++) {
    if (data[i].did_vibrate) {
      has_valid_sample = false;
      break;
    }
    total_x += data[i].x;
    total_y += -data[i].y;
    //total_z += data[i].z;
  }
  
  if (has_valid_sample) {
    avg_x = round((float)total_x / (float)num_samples);
    avg_y = round((float)total_y / (float)num_samples);
    //avg_z = round((float)total_z / (float)num_samples);
    
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Avg - x: %d, y: %d", avg_x, avg_y);
    
    if (s_baselined) {
      // Average values for last 5 accel readings are still a little
      // erratic, so use a single pole, IIR filter to smooth them out
      s_filtered_x = (s_filtered_x * FILTER_K) + ((1.0 - FILTER_K) * avg_x);
      s_filtered_y = (s_filtered_y * FILTER_K) + ((1.0 - FILTER_K) * avg_y);
      
      GPoint loc;
      
      int diff_x = s_filtered_x - s_baseline_x;
      if (diff_x < -s_max_tilt)
        loc.x = 0;
      else if (diff_x > s_max_tilt)
        loc.x = 144;
      else
        loc.x = 72 + round(((float)diff_x / s_max_tilt) * 72);
      
      int diff_y = s_filtered_y - s_baseline_y;
      if (diff_y < -s_max_tilt)
        loc.y = 0;
      else if (diff_y > s_max_tilt)
        loc.y = 168;
      else
        loc.y = 84 + round(((float)diff_y / s_max_tilt) * 84);
      
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "Diff - x: %d, y: %d", diff_x, diff_y);
      
      cursor_set_loc(loc);
    } else {
      // Use this sample average as the baseline for calculating change for moving the cursor
      s_baseline_x = avg_x;
      s_baseline_y = avg_y;
      //s_baseline_z = avg_z;
      s_filtered_x = avg_x;
      s_filtered_y = avg_y;
      s_baselined = true;
    }
  }
  
}

static void load_settings(void) {
  set_drawingcursor(s_settings.drawingcursor_on);
  
  if (s_settings.backlight_alwayson)
    light_enable(true);
  else
    light_enable(false);
  
  switch (s_settings.sensitivity) {
    case CS_HIGH:
      s_max_tilt = HIGH_TILT;
      break;
    case CS_LOW:
      s_max_tilt = LOW_TILT;
      break;
    default:
      s_max_tilt = MEDIUM_TILT;
      break;
  }
}

static void settings_closed(void) {
  load_settings();
  
  // Save settings
  persist_write_bool(DRAWINGCURSORON_KEY, s_settings.drawingcursor_on);
  persist_write_bool(BACKLIGHTAWAYSON_KEY, s_settings.backlight_alwayson);
  persist_write_int(SENSITIVITY_KEY, s_settings.sensitivity);
}

static void save_image() {
  set_paused();
  
  if (s_changed) {
    // Save image data
    void *bytes = get_imagedata();
    
    if (bytes == NULL) {
      for (int s = 0; s < 14; s++) {
        if (persist_exists(IMAGEDATA_START_KEY + s))
          persist_delete(IMAGEDATA_START_KEY + s);
      }
    } else {
      
      for (int s = 0; s < 13; s++) {
        persist_write_data(IMAGEDATA_START_KEY + s, bytes + (s * 256), 256); 
      }
      persist_write_data(IMAGEDATA_START_KEY + 13, bytes + (13 * 256), 32); 
      s_changed = false;
    }
  }
}

static void send_image_chunk(void *data) {
  void *bytes = get_imagedata() + s_chunk_pos;
  
  int len = 512;
  
  if (s_chunk_pos + len > 20*168)
    len = (20*168) - s_chunk_pos;
  
  Tuplet data_chunk = TupletBytes(IMAGE_DATA_SEND_KEY, bytes, len);
  
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Send image chunk iter is NULL");
    s_sending_image = false;
    return;
  }

  dict_write_tuplet(iter, &data_chunk);
  dict_write_end(iter);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sending image chunk - Pos: %d, Len: %d", s_chunk_pos, len);
  
  app_message_outbox_send();
}

static void send_image_chunk_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Send image chunk failed: %d", reason);
  s_sending_image = false;
}

static void sent_image_chunk(DictionaryIterator *iter, void *context) {
  if (s_sending_image && s_chunk_pos + 512 < 20*168) {
    s_chunk_pos += 512;
    app_timer_register(15, send_image_chunk, NULL);
  }
  else
    s_sending_image = false;
}

static void send_image(void) {
  s_chunk_pos = 0;
  s_sending_image = true;
  send_image_chunk(NULL);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  set_paused();
  s_baselined = false;
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_changed = true;
  toggle_pen();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  set_paused();
  show_settings(&s_settings, send_image, settings_closed);
}
  
// Trap single clicks
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void focus_handler(bool in_focus) {
  if (!in_focus) set_paused();
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (!is_pen_down() && axis == ACCEL_AXIS_Y) {
    s_changed = true;
    clear_image();
  }
}

static void info_closed(void) {
  // Re-baseline on closing info window
  s_baselined = false;
}

static void canvas_closed(void) {
  save_image();
}

static void init(void) {
  
  // Show the main screen and update the UI
  show_canvas(canvas_closed);
  
  if (persist_exists(DRAWINGCURSORON_KEY))
    s_settings.drawingcursor_on = persist_read_bool(DRAWINGCURSORON_KEY);
  else
    s_settings.drawingcursor_on = true;
  
  if (persist_exists(BACKLIGHTAWAYSON_KEY))
    s_settings.backlight_alwayson = persist_read_bool(BACKLIGHTAWAYSON_KEY);
  else
    s_settings.backlight_alwayson = false;
  
  if (persist_exists(SENSITIVITY_KEY))
    s_settings.sensitivity = persist_read_int(SENSITIVITY_KEY);
  else
    s_settings.sensitivity = CS_MEDIUM;
  
  load_settings();
  
  if (persist_exists(IMAGEDATA_START_KEY)) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Storage has image data - Initializing image");
    
    init_imagedata();
    void *bytes = get_imagedata();
    
    if (bytes != NULL) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Image initialized - reading image data");
      for (int s = 0; s < 13; s++) {
        if (persist_exists(IMAGEDATA_START_KEY + s)) {
          persist_read_data(IMAGEDATA_START_KEY + s, bytes + (s * 256), 256);
        }
      }
      if (persist_exists(IMAGEDATA_START_KEY + 13))
        persist_read_data(IMAGEDATA_START_KEY + 13, bytes + (13 * 256), 32);
    }
  }
  
  init_click_events(click_config_provider);
  
  accel_data_service_subscribe(5, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
  
  app_focus_service_subscribe(focus_handler);
  
  accel_tap_service_subscribe(tap_handler);
  
  show_infowin(info_closed);
  
  app_message_register_outbox_sent(sent_image_chunk);
  app_message_register_outbox_failed(send_image_chunk_failed);
  app_message_open(64, app_message_outbox_size_maximum());
}

static void deinit(void) {
  
  accel_data_service_unsubscribe();
  app_focus_service_unsubscribe();
  accel_tap_service_unsubscribe();
  light_enable(false);
  
  hide_canvas();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}