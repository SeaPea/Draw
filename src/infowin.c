#include "infowin.h"
#include <pebble.h>

// BEGIN AUTO-GENERATED UI CODE; DO NOT MODIFY
static Window *s_window;
static TextLayer *s_textlayer_1;
static TextLayer *s_textlayer_2;
static TextLayer *s_textlayer_3;
static TextLayer *s_textlayer_4;
static TextLayer *s_textlayer_5;
static TextLayer *s_textlayer_6;
static TextLayer *s_textlayer_7;
static TextLayer *s_textlayer_8;
static TextLayer *s_textlayer_9;

static void initialise_ui(void) {
  s_window = window_create();
  window_set_fullscreen(s_window, true);
  
  // s_textlayer_1
  s_textlayer_1 = text_layer_create(GRect(19, 12, 53, 44));
  text_layer_set_background_color(s_textlayer_1, GColorClear);
  text_layer_set_text(s_textlayer_1, "Dismiss this message");
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_1);
  
  // s_textlayer_2
  s_textlayer_2 = text_layer_create(GRect(79, 12, 50, 33));
  text_layer_set_background_color(s_textlayer_2, GColorClear);
  text_layer_set_text(s_textlayer_2, "Re-zero tilt");
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
  
  // s_textlayer_6
  s_textlayer_6 = text_layer_create(GRect(59, 65, 70, 36));
  text_layer_set_background_color(s_textlayer_6, GColorClear);
  text_layer_set_text(s_textlayer_6, "Toggle pen on/off");
  text_layer_set_text_alignment(s_textlayer_6, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_6);
  
  // s_textlayer_7
  s_textlayer_7 = text_layer_create(GRect(126, 139, 18, 20));
  text_layer_set_background_color(s_textlayer_7, GColorClear);
  text_layer_set_text(s_textlayer_7, "-->");
  text_layer_set_text_alignment(s_textlayer_7, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_7);
  
  // s_textlayer_8
  s_textlayer_8 = text_layer_create(GRect(40, 135, 90, 29));
  text_layer_set_background_color(s_textlayer_8, GColorClear);
  text_layer_set_text(s_textlayer_8, "Toggle cursor while drawing");
  text_layer_set_text_alignment(s_textlayer_8, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_8);
  
  // s_textlayer_9
  s_textlayer_9 = text_layer_create(GRect(2, 98, 95, 30));
  text_layer_set_background_color(s_textlayer_9, GColorClear);
  text_layer_set_text(s_textlayer_9, "Shake while pen is off to clear");
  layer_add_child(window_get_root_layer(s_window), (Layer *)s_textlayer_9);
}

static void destroy_ui(void) {
  window_destroy(s_window);
  text_layer_destroy(s_textlayer_1);
  text_layer_destroy(s_textlayer_2);
  text_layer_destroy(s_textlayer_3);
  text_layer_destroy(s_textlayer_4);
  text_layer_destroy(s_textlayer_5);
  text_layer_destroy(s_textlayer_6);
  text_layer_destroy(s_textlayer_7);
  text_layer_destroy(s_textlayer_8);
  text_layer_destroy(s_textlayer_9);
}
// END AUTO-GENERATED UI CODE

static void handle_window_unload(Window* window) {
  destroy_ui();
}

void show_infowin(void) {
  initialise_ui();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
  });
  window_stack_push(s_window, true);
}

void hide_infowin(void) {
  window_stack_remove(s_window, true);
}
