#include <pebble.h>
#include "common.h"
#include "canvas.h"  
#include "intmath.h"
#include "infowin.h"
#include "settings.h"
#include "msg.h"

// Main app unit - controls application and processes acceleromoter events
  
#define HIGH_TILT UINT16_MAX / 14   // Approx. 25deg
#define MEDIUM_TILT UINT16_MAX / 10 // Approx. 35deg
#define LOW_TILT UINT16_MAX / 8     // 45deg
  
#define FILTER_K 0.9  // Accelerometer smoothing constant (Must be less than 1. Higher = smoother, slower. Lower = faster, less smooth)

#define IMAGE_CHUNK_SIZE 512
#define PERSIST_SIZE_MAX 256
  
// App settings index keys
enum SettingKeys {
  DRAWINGCURSORON_KEY = 0,
  BACKLIGHTAWAYSON_KEY = 1,
  SENSITIVITY_KEY = 3,
  SECONDSHAKE_CLEAR_KEY = 4,
  ERASERSIZE_KEY = 5,
  PENWIDTH_KEY = 6,
  IMAGEDATA_START_KEY = 20
};

// App message keys
enum AppMsgKeys {
  IMAGE_DATA_SEND_KEY = 1,
  CHUNK_STATUS_KEY = 2
};

// Image data chunk sending statuses
enum ChunkStatuses {
  FIRST_CHUNK = 1,
  MID_CHUNK = 2,
  LAST_CHUNK = 3
};

// Variables for cursor centering
static bool s_centered = false;
static uint16_t s_center_x;
static uint16_t s_center_y;

// Variables for filtered (smoothed) acceleromter values
static int s_filtered_x;
static int s_filtered_y;
static int s_filtered_z;

static int s_max_tilt;
static bool s_changed = false; // Indicates if the image has changed
static bool s_infocus = true;  // Indicates if the app is in focus
static bool s_perm_light_on = false;

static bool s_sending_image = false;
static int s_chunk_pos;

static char s_msg[100];

// Settings struct used for passing to/from Settings window
// (Not used for saving settings as it is easier to store them individually when settings may be added)
static struct Settings_st s_settings;

// Accelerometer handler, where cursor movement is processed
static void accel_handler(AccelData *data, uint32_t num_samples) {
  
  if (s_infocus) {
    // Only process accelerometer values when app is in focus
    
    bool has_valid_sample = true;
    int total_x = 0;
    int total_y = 0;
    int total_z = 0;
    int avg_x = 0;
    int avg_y = 0;
    int avg_z = 0;
    
    // Calculate totals in order to average accel in each axis for the sample size
    for (int i = 0; i < (int)num_samples; i++) {
      if (data[i].did_vibrate) {
        has_valid_sample = false;
        break;
      }
      total_x += data[i].x;
      total_y += -data[i].y;
      total_z += data[i].z;
    }
    
    if (has_valid_sample) {
      // If there was no vibration, calculate average accel values for sample size
      avg_x = divide(total_x, num_samples);
      avg_y = divide(total_y, num_samples);
      avg_z = divide(total_z, num_samples);
        
      if (s_centered) {
        // If cursor center has been fixed, move cursor as necessary
        
        // Average values for sample size are still a little
        // erratic, so use a single pole, IIR filter to smooth them out
        s_filtered_x = (s_filtered_x * FILTER_K) + ((1.0 - FILTER_K) * avg_x);
        s_filtered_y = (s_filtered_y * FILTER_K) + ((1.0 - FILTER_K) * avg_y);
        s_filtered_z = (s_filtered_z * FILTER_K) + ((1.0 - FILTER_K) * avg_z);
        
        // Convert filtered accel values to angles in the x and y plane
        // (Angle values are 0 to 2^16 representing 0 to 360 degrees linearly)
        uint16_t adj = intsqrt(s_filtered_y * s_filtered_y + s_filtered_z * s_filtered_z);
        uint16_t angle_x = atan2_lookup(s_filtered_x * ((s_filtered_z > 0) ? -1 : 1), adj);
        adj = intsqrt(s_filtered_x * s_filtered_x + s_filtered_z * s_filtered_z) * ((s_filtered_z > 0) ? -1 : 1);
        uint16_t angle_y = atan2_lookup(s_filtered_y, adj);
        
        GPoint loc;
        
        // Calculate the difference from the center values to represent cursor movement.
        // Use overflow of uint16 math into a int16 to correctly calculate diffs for cursor position.
        // (Angle values are 0 to UINT16_MAX representing 0 to 360 degrees, so 180 to 360 will overflow
        //  causing rotation to reverse, but that is correct as the watch will be upside down)
        int16_t diff_x = angle_x - s_center_x;
  
        if (diff_x < -s_max_tilt)
          loc.x = 0;
        else if (diff_x > s_max_tilt)
          loc.x = IMG_WIDTH;
        else
          loc.x = (IMG_WIDTH/2) + divide(diff_x * (IMG_WIDTH/2), s_max_tilt);
        
        int16_t diff_y = angle_y - s_center_y;
        
        if (diff_y < -s_max_tilt)
          loc.y = 0;
        else if (diff_y > s_max_tilt)
          loc.y = IMG_HEIGHT;
        else
          loc.y = (IMG_HEIGHT/2) + divide(diff_y * (IMG_HEIGHT/2), s_max_tilt);
        
        // Move the cursor on the canvas window (The 'pen down' setting will determine if anything is drawn)
        cursor_set_loc(loc);
        
      } else {
        // Use this sample average as the center location for calculating change for moving the cursor
        
        // Convert accel values to angles in the x and y plane
        uint16_t adj = intsqrt(avg_y * avg_y + avg_z * avg_z);
        uint16_t angle_x = atan2_lookup(avg_x * ((avg_z > 0) ? -1 : 1), adj);
        adj = intsqrt(avg_x * avg_x + avg_z * avg_z) * ((avg_z > 0) ? -1 : 1);
        uint16_t angle_y = atan2_lookup(avg_y, adj);
        
        s_center_x = angle_x;
        s_center_y = angle_y;
        s_filtered_x = avg_x;
        s_filtered_y = avg_y;
        s_filtered_z = avg_z;
        s_centered = true;
        
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Baselines - x: %d g, x: %d deg, y: %d g, y: %d deg, z %d g", 
                avg_x, (int)divide(360 * angle_x, UINT16_MAX), avg_y, (int)divide(360 * angle_y, UINT16_MAX), avg_z);
      }
    }
  }
}

static void light_delay(void *data) {
  light_enable_interaction();
}

static void light_control(bool light_on) {
  if (light_on != s_perm_light_on) {
    light_enable(light_on);
    s_perm_light_on = light_on;
    // If turning permanent light off, enable 3 second light after short delay so it doesn't turn off immediately
    if (!light_on) app_timer_register(5, light_delay, NULL);
  }
}

// Load settings into the app
static void load_settings(void) {
  set_drawingcursor(s_settings.drawingcursor_on);
  
  light_control(s_settings.backlight_alwayson && is_pen_down());
  
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
  
  set_penwith(s_settings.pen_width);
  
  set_eraserwidth(s_settings.eraser_width);
  
  set_undo_undo(!s_settings.secondshake_clear);
}

// Event fires when settings window is closed
static void settings_closed(void) {
  load_settings();
  
  // Save settings
  persist_write_bool(DRAWINGCURSORON_KEY, s_settings.drawingcursor_on);
  persist_write_bool(BACKLIGHTAWAYSON_KEY, s_settings.backlight_alwayson);
  persist_write_int(SENSITIVITY_KEY, s_settings.sensitivity);
  persist_write_int(ERASERSIZE_KEY, s_settings.eraser_width);
  persist_write_int(PENWIDTH_KEY, s_settings.pen_width);
  persist_write_bool(SECONDSHAKE_CLEAR_KEY, s_settings.secondshake_clear);
}

// Save image pixel data into the watch storage
static void save_image() {
  set_paused();
  
  if (s_changed) {
    // Save image data
    void *bytes = get_imagedata();
    
    // For whatever reason it is faster to always delete and then write the image data
    for (int s = 0; s < ((IMG_PIXELS / PERSIST_SIZE_MAX) + 1); s++) {
      if (persist_exists(IMAGEDATA_START_KEY + s))
        persist_delete(IMAGEDATA_START_KEY + s);
    }
    
    if (bytes != NULL) {
      // Store the image pixel byte array as chunks in the watch storage
      // (Each entry can only store up to 256 bytes)
      for (int s = 0; s < (IMG_PIXELS / PERSIST_SIZE_MAX); s++) {
        persist_write_data(IMAGEDATA_START_KEY + s, bytes + (s * PERSIST_SIZE_MAX), PERSIST_SIZE_MAX); 
      }
      // Store remainder pixels
      persist_write_data(IMAGEDATA_START_KEY + (IMG_PIXELS / PERSIST_SIZE_MAX), 
                         bytes + ((IMG_PIXELS / PERSIST_SIZE_MAX) * PERSIST_SIZE_MAX), 
                         IMG_PIXELS % PERSIST_SIZE_MAX); 
      s_changed = false;
    }
  }
}

// Send a chunk of image pixel data to the phone (ignore *data parameter, used as timer procdure)
// (app message outbox has a limit of just over 512 bytes so the image must be sent in chunks)
static void send_image_chunk(void *data) {
  // Get the pixel bytes chunk start position
  void *bytes = get_imagedata() + s_chunk_pos;
  
  // Assume max chunk size
  int len = IMAGE_CHUNK_SIZE;
  
  // Indicate if 1st or one of many middle chunk
  int chunk_status_flag = (s_chunk_pos == 0) ? FIRST_CHUNK : MID_CHUNK;
  
  if (s_chunk_pos + len > IMG_PIXELS) {
    // If chunk goes past end of pixel array, only send remainder as last chunk
    len = IMG_PIXELS - s_chunk_pos;
    chunk_status_flag = LAST_CHUNK;
  }
  
  // Setup tuplets for sending data to phone
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
  // Finally, send image chunk
  app_message_outbox_send();
}

// Event fired when send failed
static void send_image_chunk_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Send image chunk failed: %d", reason);
  s_sending_image = false;
  // Show error message for 15 seconds
  snprintf(s_msg, sizeof(s_msg), "Sending failed\n(Error: %d)", reason);
  show_msg(s_msg, true, 15);
}

// Event fired when image chunk succesfully sent
static void sent_image_chunk(DictionaryIterator *iter, void *context) {
  if (s_sending_image && s_chunk_pos + IMAGE_CHUNK_SIZE < IMG_PIXELS) {
    // If there is more to send, send it
    s_chunk_pos += IMAGE_CHUNK_SIZE;
    snprintf(s_msg, sizeof(s_msg), "Sending to Phone...\n%d%%", (int)divide(s_chunk_pos * 100, IMG_PIXELS));
    show_msg(s_msg, true, 0);
    // Send next chunk after brief delay to allow display to update
    app_timer_register(15, send_image_chunk, NULL);
  } else {
    // Else sent all chunks, so stop sending and show Sent message for 15 seconds
    s_sending_image = false;
    show_msg("Sent to Phone.\nGo to Draw app Settings in Pebble phone app to view.", false, 15);
  }
}

// Start sending the image data to the phone
static void send_image(void) {
  if (get_imagedata() == NULL) {
    // No image data
    show_msg("No image to send.\nPress Back and then Select button to start drawing", false, 5);
  } else {
    // If have image data, start sending
    s_chunk_pos = 0;
    s_sending_image = true;
    show_msg("Sending to Phone...\n0%", true, 0);
    // Start sending after a brief delay to allow message to display
    app_timer_register(300, send_image_chunk, NULL);
  }
}

// Handle Up button clicks
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Pause drawing and reset center (accel handler will re-center when s_centered == false)
  set_paused();
  s_centered = false;
}

// Handle Select button clicks
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Mark image as changed and toggle 'pen' on/off
  s_changed = true;
  toggle_pen();
}

// Handle holding Select button
static void select_hold_handler(ClickRecognizerRef recognizer, void *context) {
  // Mark image as changed and toggle 'eraser' on/off
  s_changed = true;
  toggle_eraser();
}

// Handle Down button clicks
static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Pause drawing and show settings window
  set_paused();
  show_settings(&s_settings, send_image, clear_image, settings_closed);
}
  
// Trap single clicks
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 2000, select_hold_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

// Handle app focus changes
static void focus_handler(bool in_focus) {
  // Pause drawing if we lose focus (notification)
  if (!in_focus) set_paused();
  s_infocus = in_focus;
}

// Handle 'tap' event when watch is sharply shaken/tapped
static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (!is_pen_down() && axis == ACCEL_AXIS_Y && s_infocus && is_canvas_on_top()) {
    // If not drawing and tap was in y plane and app is in focus and canvase is showing, clear image
    s_changed = true;
    if (has_undo())
      undo_image();
    else if (s_settings.secondshake_clear)
      clear_image();
  }
}

// Event fired when info window is closed
static void info_closed(void) {
  // Re-center cursor on closing info window so it is centered when used is looking at watch
  s_centered = false;
}

// Event fired when 'pen' status changes
static void pen_status_changed(bool pen_down, bool eraser_on) {
  light_control(s_settings.backlight_alwayson && (pen_down || eraser_on));
}

// Event fired when canvas window closes
static void canvas_closed(void) {
  // Save image data to watch storage
  save_image();
}

static void init(void) {
  
  // Show the main screen and update the UI
  show_canvas(pen_status_changed, canvas_closed);
  
  // Get settings
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
  
  if (persist_exists(SECONDSHAKE_CLEAR_KEY))
    s_settings.secondshake_clear = persist_read_bool(SECONDSHAKE_CLEAR_KEY);
  else
    s_settings.secondshake_clear = true;
  
  if (persist_exists(ERASERSIZE_KEY))
    s_settings.eraser_width = persist_read_int(ERASERSIZE_KEY);
  else
    s_settings.eraser_width = 3;
  
  if (persist_exists(PENWIDTH_KEY))
    s_settings.pen_width = persist_read_int(PENWIDTH_KEY);
  else
    s_settings.pen_width = 1;
  
  load_settings();
  
  // If saved, load image data from storage
  if (persist_exists(IMAGEDATA_START_KEY)) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Storage has image data - Initializing image");
    
    init_imagedata();
    void *bytes = get_imagedata();
    
    if (bytes != NULL) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Image initialized - reading image data");
      for (int s = 0; s < (IMG_PIXELS / PERSIST_SIZE_MAX); s++) {
        if (persist_exists(IMAGEDATA_START_KEY + s)) {
          persist_read_data(IMAGEDATA_START_KEY + s, bytes + (s * PERSIST_SIZE_MAX), PERSIST_SIZE_MAX);
        }
      }
      if (persist_exists(IMAGEDATA_START_KEY + (IMG_PIXELS / PERSIST_SIZE_MAX)))
        persist_read_data(IMAGEDATA_START_KEY + (IMG_PIXELS / PERSIST_SIZE_MAX), 
                          bytes + ((IMG_PIXELS / PERSIST_SIZE_MAX) * PERSIST_SIZE_MAX), 
                          IMG_PIXELS % PERSIST_SIZE_MAX);
    }
  }
  
  // Subscribe to events
  init_click_events(click_config_provider);
  accel_data_service_subscribe(5, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
  app_focus_service_subscribe(focus_handler);
  accel_tap_service_subscribe(tap_handler);
  
  // Show info window to explain buttons
  show_infowin(info_closed);
  
  // Init app message for sending image to phone
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