#include "canvas.h"

static Window *s_window;
static Layer *s_canvaslayer;

static PenStatusCallBack s_pen_event;
static CanvaseClosedCallBack s_canvas_closed;

static bool s_drawingcursor_on = true;
static bool s_pen_down = false;
static GPoint s_cursor_loc;
static GPoint s_last_loc;
static GBitmap *s_image = NULL;

static void updatecanvas(Layer *layer, GContext *cxt);

static void initialise_ui(void) {
  s_window = window_create();
  window_set_fullscreen(s_window, true);
  
  s_canvaslayer = layer_create(GRect(0, 0, 144, 168));
  layer_set_update_proc(s_canvaslayer, updatecanvas);
  layer_add_child(window_get_root_layer(s_window), s_canvaslayer);
}

static void destroy_ui(void) {
  window_destroy(s_window);
  layer_destroy(s_canvaslayer);
}

static void handle_window_unload(Window* window) {
  set_paused();
  destroy_ui();
  if (s_canvas_closed != NULL) s_canvas_closed();
  if (s_image != NULL) {
    gbitmap_destroy(s_image);
    s_image = NULL;
  }
}

static void updatecanvas(Layer *layer, GContext *ctx) {
  
  // If there is an image in memory, copy it back to the screen (since it clears every time this proc is called)
  if (s_image != NULL) {
    GBitmap *screen = graphics_capture_frame_buffer(ctx);
      
    if (screen != NULL) {
      // Pebble screen (144x168) uses 20 bytes per row, so copy 20x168 bytes of the bitmap data
      // from the drawn image back to the framebuffer
      memcpy(screen->addr, s_image->addr, 20*168);
      graphics_release_frame_buffer(ctx, screen);
    }
  }
  
  if (s_pen_down) {
    // Draw a line between the last cursor position and the new position (in case the cursor jumps)
    graphics_draw_line(ctx, s_last_loc, s_cursor_loc);
    
    // Capture framebuffer to save image
    if (s_image == NULL) s_image = gbitmap_create_blank(GSize(144, 168));
    
    if (s_image != NULL) {
      GBitmap *screen = graphics_capture_frame_buffer(ctx);
      
      if (screen != NULL) {
        // Pebble screen (144x168) uses 20 bytes per row, so copy 20x168 bytes of the bitmap data
        // from the framebuffer to save the drawn image
        memcpy(s_image->addr, screen->addr, 20*168);
        graphics_release_frame_buffer(ctx, screen);
      }
    }
  }
  
  // If drawing cursor on or pen is not down, draw a cursor over the image
  if (s_drawingcursor_on || !s_pen_down) {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_compositing_mode(ctx, GCompOpAssignInverted);
    graphics_draw_line(ctx, GPoint(s_cursor_loc.x, s_cursor_loc.y - 5), GPoint(s_cursor_loc.x, s_cursor_loc.y + 5));
    graphics_draw_line(ctx, GPoint(s_cursor_loc.x - 5, s_cursor_loc.y), GPoint(s_cursor_loc.x + 5, s_cursor_loc.y));
  }
  
}

void set_drawingcursor(bool cursor_on) {
  s_drawingcursor_on = cursor_on;
  layer_mark_dirty(s_canvaslayer);
}

void toggle_pen(void) {
  s_pen_down = !s_pen_down;
  layer_mark_dirty(s_canvaslayer);
  if (s_pen_event != NULL) s_pen_event(s_pen_down);
}

bool is_pen_down(void) {
  return s_pen_down;
}

void set_paused(void) {
  s_pen_down = false;
  if (s_pen_event != NULL) s_pen_event(s_pen_down);
}

void* get_imagedata(void) {
  if (s_image == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Getting image data - NULL");
    return NULL;
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Getting image data - NOT NULL");
    return s_image->addr;
  }
}

void init_imagedata(void) {
  if (s_image == NULL)
    s_image = gbitmap_create_blank(GSize(144, 168));
}

void cursor_set_loc(GPoint loc) {
  if (loc.x != s_cursor_loc.x || loc.y != s_cursor_loc.y) {
    s_last_loc = s_cursor_loc;
    s_cursor_loc = loc;
    layer_mark_dirty(s_canvaslayer);
  }
}

void clear_image(void) {
  if (s_image != NULL) {
    gbitmap_destroy(s_image);
    s_image = NULL;
    vibes_short_pulse();
  }
  layer_mark_dirty(s_canvaslayer);
}

void init_click_events(ClickConfigProvider click_config_provider) {
  window_set_click_config_provider(s_window, click_config_provider);
}

void show_canvas(PenStatusCallBack pen_event, CanvaseClosedCallBack closed_event) {
  s_pen_event = pen_event;
  s_canvas_closed = closed_event;
  s_cursor_loc = GPoint(72, 84);
  s_last_loc = s_cursor_loc;
  initialise_ui();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
  });
  window_stack_push(s_window, true);
}

void hide_canvas(void) {
  window_stack_remove(s_window, true);
  
}
