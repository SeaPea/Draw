#include <pebble.h>
#include "settings.h"
#include "common.h"

// Application settings window, using a simple menu layer

#define MIN_PEN_WIDTH 1
#define MAX_PEN_WIDTH 9
  
#define MIN_ERASER_WIDTH 1
#define MAX_ERASER_WIDTH 15
  
#define NUM_MENU_SECTIONS 2
#define NUM_MENU_ACTION_ITEMS 2
#define NUM_MENU_MISC_ITEMS 6
#define MENU_ACTION_SECTION 0
#define MENU_SEND_ITEM 0
#define MENU_CLEAR_ITEM 1
#define MENU_MISC_SECTION 1
#define MENU_PENWIDTH_ITEM 0
#define MENU_DRAWINGCURSOR_ITEM 1
#define MENU_BACKLIGHT_ITEM 2
#define MENU_SENSITIVTY_ITEM 3
#define MENU_ERASERWIDTH_ITEM 4
#define MENU_SECONDSHAKE_ITEM 5
  
static struct Settings_st *s_settings; // Settings struct passed from main unit
static SendToPhoneCallBack s_send_event;
static ClearImageCallBack s_clear_event;
static SettingsClosedCallBack s_settings_closed;

static Window *s_window;
static MenuLayer *settings_layer;

static void initialise_ui(void) {
  s_window = window_create();
  window_set_fullscreen(s_window, false);
  
  // settings_layer
  settings_layer = menu_layer_create(GRect(0, 0, 144, 152));
  menu_layer_set_click_config_onto_window(settings_layer, s_window);
  layer_add_child(window_get_root_layer(s_window), (Layer *)settings_layer);
}

static void destroy_ui(void) {
  window_destroy(s_window);
  menu_layer_destroy(settings_layer);
  // Let main unit know settings window has been closed
  if (s_settings_closed != NULL) s_settings_closed();
}

// Set menu section count
static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return NUM_MENU_SECTIONS;
}

// Set menu section item counts
static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  switch (section_index) {
    case MENU_ACTION_SECTION:
      return NUM_MENU_ACTION_ITEMS;
    case MENU_MISC_SECTION:
      return NUM_MENU_MISC_ITEMS;
    default:
      return 0;
  }
}

// Set default menu item height
static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  switch (section_index) {
    case MENU_ACTION_SECTION:
      return MENU_CELL_BASIC_HEADER_HEIGHT;
    case MENU_MISC_SECTION:
      return MENU_CELL_BASIC_HEADER_HEIGHT;
    default:
      return MENU_CELL_BASIC_HEADER_HEIGHT;
  }
}

// Draw menu section headers
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  switch (section_index) {
    case MENU_ACTION_SECTION:
      menu_cell_basic_header_draw(ctx, cell_layer, "Actions");
      break;
    case MENU_MISC_SECTION:
      menu_cell_basic_header_draw(ctx, cell_layer, "Misc Settings");
      break;
  }
}

// Draw menu items
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  
  char pen_width_str[10];
  char eraser_width_str[10];
  
  switch (cell_index->section) {
    case MENU_ACTION_SECTION:
      switch (cell_index->row) {
        case MENU_SEND_ITEM:
          // Option for sending image to phone
          menu_cell_basic_draw(ctx, cell_layer, "Send to Phone", NULL, NULL);
          break;
        case MENU_CLEAR_ITEM:
          // Option for clearing image
          menu_cell_basic_draw(ctx, cell_layer, "Clear Image", NULL, NULL);
          break;
      }
      break;
    
    case MENU_MISC_SECTION:
      switch (cell_index->row) {
        case MENU_PENWIDTH_ITEM:
          // Show pen width
          if (s_settings->pen_width == 1)
            strncpy(pen_width_str, "1 Pixel", sizeof(pen_width_str));
          else
            snprintf(pen_width_str, sizeof(pen_width_str), "%d Pixels", s_settings->pen_width);
        
          menu_cell_basic_draw(ctx, cell_layer, "Pen Width", pen_width_str, NULL);
          
          break;
        case MENU_DRAWINGCURSOR_ITEM:
          // Enable/Disable cursor while drawing
          menu_cell_basic_draw(ctx, cell_layer, "Drawing Cursor", s_settings->drawingcursor_on ? "ON" : "OFF", NULL);
          break;
        case MENU_BACKLIGHT_ITEM:
          // Backlight always on or not
          menu_cell_basic_draw(ctx, cell_layer, "Backlight", s_settings->backlight_alwayson ? "ON while drawing" : "Default setting", NULL);
          break;
        case MENU_SENSITIVTY_ITEM:
          // Adjust tilt sensitivity
          switch (s_settings->sensitivity) {
            case CS_LOW:
              menu_cell_basic_draw(ctx, cell_layer, "Sensitivity", "Low", NULL);
              break;
            case CS_MEDIUM:
              menu_cell_basic_draw(ctx, cell_layer, "Sensitivity", "Medium", NULL);
              break;
            case CS_HIGH:
              menu_cell_basic_draw(ctx, cell_layer, "Sensitivity", "High", NULL);
              break;
            default:
              menu_cell_basic_draw(ctx, cell_layer, "Sensitivity", "???", NULL);
              break;
          }
          break;
        case MENU_ERASERWIDTH_ITEM:
          if (s_settings->eraser_width == 1)
            strncpy(eraser_width_str, "1 Pixel", sizeof(eraser_width_str));
          else
            snprintf(eraser_width_str, sizeof(eraser_width_str), "%d Pixels", s_settings->eraser_width);
        
          menu_cell_basic_draw(ctx, cell_layer, "Eraser Width", eraser_width_str, NULL);
          
          break;
        case MENU_SECONDSHAKE_ITEM:
          if (s_settings->secondshake_clear)
            menu_cell_basic_draw(ctx, cell_layer, "Second Shake", "Clears image", NULL);
          else
            menu_cell_basic_draw(ctx, cell_layer, "Second Shake", "Undo the last undo", NULL);
        
          break;
      }
      break;
  }
}

// Process menu item select clicks
static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  switch (cell_index->section) {
    case MENU_ACTION_SECTION:
      switch (cell_index->row) {
        case MENU_SEND_ITEM:
          // Call event in main unit to send image to phone
          if (s_send_event != NULL) s_send_event();
          hide_settings();
          break;
        case MENU_CLEAR_ITEM:
          // Call event in main unit to clear image
          if (s_clear_event != NULL) s_clear_event();
          hide_settings();
          break;
      }
      break;
    case MENU_MISC_SECTION:
      switch (cell_index->row) {
        case MENU_PENWIDTH_ITEM:
          // Cycle through pen widths in 2 pixel increments (odd pixel widths used to center point)
          if (s_settings->pen_width >= MAX_PEN_WIDTH)
            s_settings->pen_width = MIN_PEN_WIDTH;
          else
            s_settings->pen_width += 2;
          break;
        case MENU_DRAWINGCURSOR_ITEM:
          // Toggle cursor while drawing on/off
          s_settings->drawingcursor_on = !s_settings->drawingcursor_on;
          break;
        case MENU_BACKLIGHT_ITEM:
          // Toggle backlight while drawing on/off
          s_settings->backlight_alwayson = !s_settings->backlight_alwayson;
          break;
        case MENU_SENSITIVTY_ITEM:
          // Cycle through tilt sensitivity settings
          s_settings->sensitivity = (s_settings->sensitivity == CS_HIGH ? CS_LOW : s_settings->sensitivity + 1);
          break;
        case MENU_ERASERWIDTH_ITEM:
          // Cycle through eraser widths
          if (s_settings->eraser_width >= MAX_ERASER_WIDTH)
            s_settings->eraser_width = MIN_ERASER_WIDTH;
          else
            s_settings->eraser_width += 2;
          break;
        case MENU_SECONDSHAKE_ITEM:
          s_settings->secondshake_clear = !s_settings->secondshake_clear;
          break;
      }
      layer_mark_dirty(menu_layer_get_layer(settings_layer));
      break;
  }

}

static void handle_window_unload(Window* window) {
  destroy_ui();
}

// Show settings window with settings passed as reference to structure and with callback procedures
void show_settings(struct Settings_st *settings, SendToPhoneCallBack send_event, ClearImageCallBack clear_event, SettingsClosedCallBack settings_closed) {
  initialise_ui();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
  });
  
  s_settings = settings;
  s_send_event = send_event;
  s_clear_event = clear_event;
  s_settings_closed = settings_closed;
  
  // Set all the callbacks for the menu layer
  menu_layer_set_callbacks(settings_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_callback
  });
  
  window_stack_push(s_window, true);
}

void hide_settings(void) {
  window_stack_remove(s_window, true);
}