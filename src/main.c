#include <pebble.h>
#include "canvas.h"  
#include "infowin.h"
#include "settings.h"

//#define DEBUG
  
#define HIGH_TILT UINT16_MAX / 14   // Approx. 25deg
#define MEDIUM_TILT UINT16_MAX / 10 // Approx. 35deg
#define LOW_TILT UINT16_MAX / 8     // 45deg
  
#define FILTER_K 0.9

enum SettingKeys {
  DRAWINGCURSORON_KEY = 0,
  BACKLIGHTAWAYSON_KEY = 1,
  SENSITIVITY_KEY = 3,
  IMAGEDATA_START_KEY = 20
};

enum AppMsgKeys {
  IMAGE_DATA_SEND_KEY = 1,
  CHUNK_STATUS_KEY = 2
};

enum ChunkStatuses {
  FIRST_CHUNK = 1,
  MID_CHUNK = 2,
  LAST_CHUNK = 3
};

static bool s_baselined = false;
static uint16_t s_baseline_x;
static uint16_t s_baseline_y;
static int s_filtered_x;
static int s_filtered_y;
static int s_filtered_z;
static int s_max_tilt;
static bool s_changed = false;
#ifdef DEBUG
static uint64_t s_last_debug_angle = 0;
#endif

static bool s_sending_image = false;
static int s_chunk_pos;

static struct Settings_st s_settings;

// Integer division with rounding
static int32_t divide(int32_t n, int32_t d)
{
  return ((n < 0) ^ (d < 0)) ? ((n - d/2) / d) : ((n + d/2) / d);
}

// Fast integer square-root with rounding
static uint32_t intsqrt(uint32_t input)
{
    if (input == 0) return 0;
  
    uint32_t op  = input;
    uint32_t result = 0;
    uint32_t one = 1uL << 30; 

    // Find the highest power of four <= than the input.
    while (one > op)
    {
        one >>= 2;
    }

    while (one != 0)
    {
        if (op >= result + one)
        {
            op = op - (result + one);
            result = result +  2 * one;
        }
        result >>= 1;
        one >>= 2;
    }

    // Round result
    if (op > result)
    {
        result++;
    }

    return result;
}

static void accel_handler(AccelData *data, uint32_t num_samples) {
  
  bool has_valid_sample = true;
  int total_x = 0;
  int total_y = 0;
  int total_z = 0;
  int avg_x = 0;
  int avg_y = 0;
  int avg_z = 0;
  
#ifdef DEBUG
  uint64_t latest_sample = 0;
#endif
  
  // Calculate totals in order to average accel in each axis for the sample size
  for (int i = 0; i < (int)num_samples; i++) {
    if (data[i].did_vibrate) {
      has_valid_sample = false;
      break;
    }
    total_x += data[i].x;
    total_y += -data[i].y;
    total_z += data[i].z;
#ifdef DEBUG
    latest_sample = data[i].timestamp;
#endif
  }
  
  if (has_valid_sample) {
    avg_x = divide(total_x, num_samples);
    avg_y = divide(total_y, num_samples);
    avg_z = divide(total_z, num_samples);
      
    if (s_baselined) {
      // Average values for last 5 accel readings are still a little
      // erratic, so use a single pole, IIR filter to smooth them out
      s_filtered_x = (s_filtered_x * FILTER_K) + ((1.0 - FILTER_K) * avg_x);
      s_filtered_y = (s_filtered_y * FILTER_K) + ((1.0 - FILTER_K) * avg_y);
      s_filtered_z = (s_filtered_z * FILTER_K) + ((1.0 - FILTER_K) * avg_z);
      
      // Convert filtered accel values to angles in the x and y plane
      uint16_t adj = intsqrt(s_filtered_y * s_filtered_y + s_filtered_z * s_filtered_z);
      uint16_t angle_x = atan2_lookup(s_filtered_x, adj);
      adj = intsqrt(s_filtered_x * s_filtered_x + s_filtered_z * s_filtered_z);
      uint16_t angle_y = atan2_lookup(s_filtered_y, adj);
      
      GPoint loc;
      
      // Use overflow of uint16 math into a int16 to correctly calculate diffs for cursor position
      int16_t diff_x = angle_x - s_baseline_x;
      
      if (diff_x < -s_max_tilt)
        loc.x = 0;
      else if (diff_x > s_max_tilt)
        loc.x = 144;
      else
        loc.x = 72 + divide(diff_x * 72, s_max_tilt);
      
      int16_t diff_y = angle_y - s_baseline_y;
      
      if (diff_y < -s_max_tilt)
        loc.y = 0;
      else if (diff_y > s_max_tilt)
        loc.y = 168;
      else
        loc.y = 84 + divide(diff_y * 84, s_max_tilt);
      
#ifdef DEBUG
    if (latest_sample - s_last_debug_angle >= 3000) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Filtered Values - x: %d g, x raw: %d, x: %d deg, y: %d g, y raw: %d, y: %d deg, z %d g, adj x-z: %d", 
              s_filtered_x, angle_x, (int)divide(360 * angle_x, UINT16_MAX), s_filtered_y, angle_y, (int)divide(360 * angle_y, UINT16_MAX), s_filtered_z, adj);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Diff - x: %d, y: %d", (int)diff_x, (int)diff_y);
      s_last_debug_angle = latest_sample;  
    }
#endif
      
      cursor_set_loc(loc);
    } else {
      // Use this sample average as the baseline for calculating change for moving the cursor
      
      // Convert accel values to angles in the x and y plane
      uint16_t adj = intsqrt(avg_y * avg_y + avg_z * avg_z);
      uint16_t angle_x = atan2_lookup(avg_x, adj);
      adj = intsqrt(avg_x * avg_x + avg_z * avg_z);
      uint16_t angle_y = atan2_lookup(avg_y, adj);
      
      s_baseline_x = angle_x;
      s_baseline_y = angle_y;
      s_filtered_x = avg_x;
      s_filtered_y = avg_y;
      s_filtered_z = avg_z;
      s_baselined = true;
      
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Baselines - x: %d, y: %d", angle_x, angle_y);
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
  
  int chunk_status_flag = (s_chunk_pos == 0) ? FIRST_CHUNK : MID_CHUNK;
  
  if (s_chunk_pos + len > 20*168) {
    len = (20*168) - s_chunk_pos;
    chunk_status_flag = LAST_CHUNK;
  }
  
  Tuplet data_chunk = TupletBytes(IMAGE_DATA_SEND_KEY, bytes, len);
  Tuplet chunk_status = TupletInteger(CHUNK_STATUS_KEY, chunk_status_flag);
  
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Send image chunk iter is NULL");
    s_sending_image = false;
    return;
  }

  dict_write_tuplet(iter, &data_chunk);
  dict_write_tuplet(iter, &chunk_status);
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

static void pen_status_changed(bool pen_down) {
  if (s_settings.backlight_alwayson) {
    if (pen_down)
      light_enable(true);
    else
      light_enable(false);
  }
}

static void canvas_closed(void) {
  save_image();
}

static void init(void) {
  
  // Show the main screen and update the UI
  show_canvas(pen_status_changed, canvas_closed);
  
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