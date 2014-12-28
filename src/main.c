#include <pebble.h>
#include <math.h>
#include "canvas.h"  
#include "infowin.h"

#define MAX_TILT 400
  
#define FILTER_K 0.9
  
static bool s_baselined = false;
static int s_baseline_x;
static int s_baseline_y;
//static int s_baseline_z;
static int s_filtered_x;
static int s_filtered_y;
  
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
      if (diff_x < -MAX_TILT)
        loc.x = 0;
      else if (diff_x > MAX_TILT)
        loc.x = 144;
      else
        loc.x = 72 + round(((float)diff_x / MAX_TILT) * 72);
      
      int diff_y = s_filtered_y - s_baseline_y;
      if (diff_y < -MAX_TILT)
        loc.y = 0;
      else if (diff_y > MAX_TILT)
        loc.y = 168;
      else
        loc.y = 84 + round(((float)diff_y / MAX_TILT) * 84);
      
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

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_baselined = false;
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  toggle_pen();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  toggle_cursor();
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
  if (!is_pen_down() && axis == ACCEL_AXIS_Y) clear_image();
}

static void init(void) {
  
  // Show the main screen and update the UI
  show_canvas();
  
  init_click_events(click_config_provider);
  
  accel_data_service_subscribe(5, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
  
  app_focus_service_subscribe(focus_handler);
  
  accel_tap_service_subscribe(tap_handler);
  
  show_infowin();
}

static void deinit(void) {
  
  accel_data_service_unsubscribe();
  app_focus_service_unsubscribe();
  accel_tap_service_unsubscribe();
  
  hide_canvas();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}