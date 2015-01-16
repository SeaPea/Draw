#include "infowin.h"
#include <pebble.h>

// Simple window with info on what each button does on the main window and how to clear the image
  
static InfoWinClosedCallBack s_closed_event = NULL;
  
// BEGIN AUTO-GENERATED UI CODE; DO NOT MODIFY
static Window *s_window;
static TextLayer *s_textlayer_1;
static TextLayer *s_textlayer_2;
static TextLayer *s_textlayer_3;
static TextLayer *s_textlayer_4;
static TextLayer *s_textlayer_5;
static TextLayer *s_textlayer_7;
static TextLayer *s_textlayer_8;
static TextLayer *s_textlayer_9;
static TextLayer *s_textlayer_10;
static TextLayer *s_textlayer_6;

static void initialise_ui(void) {
  s_window = window_create();
  window_set_fullscreen(s_window, true);
  
  // s_textlayer_1
  s_textlayer_1 = text_layer_create(GRect(3, 24, 50, 62));
  text_layer_set_background_color(s_textlayer_1, GColorClear);
  text_layer_set_text(s_textlayer_1, "Exit app (Image auto-saves)");
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_1);
  
  // s_textlayer_2
  s_textlayer_2 = text_layer_create(GRect(65, 12, 64, 43));
  text_layer_set_background_color(s_textlayer_2, GColorClear);
  text_layer_set_text(s_textlayer_2, "Re-center pen");
  text_layer_set_text_alignment(s_textlayer_2, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_2);
  
  // s_textlayer_3
  s_textlayer_3 = text_layer_create(GRect(124, 12, 19, 20));
  text_layer_set_background_color(s_textlayer_3, GColorClear);
  text_layer_set_text(s_textlayer_3, "-->");
  text_layer_set_text_alignment(s_textlayer_3, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_3);
  
  // s_textlayer_4
  s_textlayer_4 = text_layer_create(GRect(3, 12, 28, 20));
  text_layer_set_background_color(s_textlayer_4, GColorClear);
  text_layer_set_text(s_textlayer_4, "<--");
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_4);
  
  // s_textlayer_5
  s_textlayer_5 = text_layer_create(GRect(124, 74, 19, 20));
  text_layer_set_background_color(s_textlayer_5, GColorClear);
  text_layer_set_text(s_textlayer_5, "-->");
  text_layer_set_text_alignment(s_textlayer_5, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_5);
  
  // s_textlayer_7
  s_textlayer_7 = text_layer_create(GRect(126, 139, 18, 20));
  text_layer_set_background_color(s_textlayer_7, GColorClear);
  text_layer_set_text(s_textlayer_7, "-->");
  text_layer_set_text_alignment(s_textlayer_7, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_7);
  
  // s_textlayer_8
  s_textlayer_8 = text_layer_create(GRect(13, 139, 116, 17));
  text_layer_set_background_color(s_textlayer_8, GColorClear);
  text_layer_set_text(s_textlayer_8, "Actions & Settings");
  text_layer_set_text_alignment(s_textlayer_8, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_8);
  
  // s_textlayer_9
  s_textlayer_9 = text_layer_create(GRect(2, 105, 140, 30));
  text_layer_set_background_color(s_textlayer_9, GColorClear);
  text_layer_set_text(s_textlayer_9, "Shake Pebble while pen is off to undo/clear");
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_9);
  
  // s_textlayer_10
  s_textlayer_10 = text_layer_create(GRect(51, 76, 78, 30));
  text_layer_set_background_color(s_textlayer_10, GColorClear);
  text_layer_set_text(s_textlayer_10, "Hold: Toggle eraser");
  text_layer_set_text_alignment(s_textlayer_10, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_10);
  
  // s_textlayer_6
  s_textlayer_6 = text_layer_create(GRect(46, 49, 82, 32));
  text_layer_set_background_color(s_textlayer_6, GColorClear);
  text_layer_set_text(s_textlayer_6, "Click: Toggle pen");
  text_layer_set_text_alignment(s_textlayer_6, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_6);
}

static void destroy_ui(void) {
  window_destroy(s_window);
  text_layer_destroy(s_textlayer_1);
  text_layer_destroy(s_textlayer_2);
  text_layer_destroy(s_textlayer_3);
  text_layer_destroy(s_textlayer_4);
  text_layer_destroy(s_textlayer_5);
  text_layer_destroy(s_textlayer_7);
  text_layer_destroy(s_textlayer_8);
  text_layer_destroy(s_textlayer_9);
  text_layer_destroy(s_textlayer_10);
  text_layer_destroy(s_textlayer_6);
}
// END AUTO-GENERATED UI CODE


static void click_handler(ClickRecognizerRef recognizer, void *context) {
  hide_infowin();
}
  
// Trap single clicks so that any button click dismisses this window
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, click_handler);
}

static void handle_window_unload(Window* window) {
  destroy_ui();
  s_closed_event();
}

void show_infowin(InfoWinClosedCallBack closed_event) {
  s_closed_event = closed_event;
  initialise_ui();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
  });
  window_stack_push(s_window, true);
  window_set_click_config_provider(s_window, click_config_provider);
}

void hide_infowin(void) {
  window_stack_remove(s_window, true);
}
