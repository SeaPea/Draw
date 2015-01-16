#include "canvas.h"
#include "common.h"

// Canvas is main window of the application that draws the image
  
static Window *s_window;
static Layer *s_canvaslayer;

static PenStatusCallBack s_pen_event;
static CanvaseClosedCallBack s_canvas_closed;

static bool s_drawingcursor_on = true;
static int s_eraser_width = 3;
static bool s_undo_undo = false;
static bool s_pen_down = false;
static bool s_eraser_on = false;
static GPoint s_cursor_loc;
static GPoint s_last_loc;
static GBitmap *s_image = NULL;
static GBitmap *s_undo_img = NULL;

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
  if (s_undo_img != NULL) {
    gbitmap_destroy(s_undo_img);
    s_undo_img = NULL;
  }
}

// Handle canvas layer being redrawn (which also does the image drawing)
static void updatecanvas(Layer *layer, GContext *ctx) {
  
  // If there is an image in memory, copy it back to the screen (since it clears every time this proc is called)
  if (s_image != NULL) {
    // Access framebuffer directly to get pixel data
    GBitmap *screen = graphics_capture_frame_buffer(ctx);
      
    if (screen != NULL) {
      // Pebble screen (144x168) uses 20 bytes per row, so copy 20x168 bytes of the bitmap data
      // from the drawn image back to the framebuffer
      memcpy(screen->addr, s_image->addr, IMG_PIXELS);
      graphics_release_frame_buffer(ctx, screen); // Must release for line draw to work
    }
  }
  
  if (s_pen_down || s_eraser_on) {
    if (s_pen_down) {
      // Draw a line between the last cursor position and the new position (in case the cursor jumps)
      // (Could write my own line draw directly to the framebuffer, but why reinvent the wheel?)
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_line(ctx, s_last_loc, s_cursor_loc);
    } else if (s_eraser_on) {
      // Draw a 3x3 white square to 'erase' the current location
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, GRect(s_cursor_loc.x-(s_eraser_width/2), s_cursor_loc.y-(s_eraser_width/2), s_eraser_width, s_eraser_width), 0, GCornerNone);
    }
    
    // Create bitmap to store framebuffer pixel data after drawing line (lazy way to draw)
    init_imagedata();
    
    if (s_image != NULL) {
      // Access framebuffer directly to copy the updated pixels back to the bitmap
      GBitmap *screen = graphics_capture_frame_buffer(ctx);
      
      if (screen != NULL) {
        // Pebble screen (144x168) uses 20 bytes per row, so copy 20x168 bytes of the bitmap data
        // from the framebuffer to save the drawn image
        memcpy(s_image->addr, screen->addr, IMG_PIXELS);
        graphics_release_frame_buffer(ctx, screen);
      }
    }
  }
  
  // If drawing cursor on or pen is not down, draw a cursor over the image
  if ((s_drawingcursor_on || !s_pen_down) && !s_eraser_on) {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_compositing_mode(ctx, GCompOpAssignInverted);
    graphics_draw_line(ctx, GPoint(s_cursor_loc.x, s_cursor_loc.y - 5), GPoint(s_cursor_loc.x, s_cursor_loc.y + 5));
    graphics_draw_line(ctx, GPoint(s_cursor_loc.x - 5, s_cursor_loc.y), GPoint(s_cursor_loc.x + 5, s_cursor_loc.y));
  }
  
  // If erasing, draw a 3x3 square outline to show where the erasor is
  if (s_eraser_on) {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_rect(ctx, GRect(s_cursor_loc.x-(s_eraser_width/2), s_cursor_loc.y-(s_eraser_width/2), s_eraser_width, s_eraser_width));
  }
}

// Turns cursor while drawing on/off
void set_drawingcursor(bool cursor_on) {
  s_drawingcursor_on = cursor_on;
  layer_mark_dirty(s_canvaslayer);
}

void set_eraserwidth(int width) {
  s_eraser_width = width;
}

void set_undo_undo(bool undo_undo) {
  s_undo_undo = undo_undo;
}

// Save the current image for undo
static void save_undo(void) {
  
  if (s_image != NULL) {
    if (s_undo_img == NULL)
      s_undo_img = gbitmap_create_blank(GSize(IMG_WIDTH, IMG_HEIGHT));
    
    memcpy(s_undo_img->addr, s_image->addr, IMG_PIXELS);
  } else {
    // No image, so make sure there is no undo
    if (s_undo_img != NULL) {
      gbitmap_destroy(s_undo_img);
      s_undo_img = NULL;
    }
  }
}

// Indicates if an undo is saved
bool has_undo(void) {
  return (s_undo_img != NULL);
}

// Roll back the image to the last undo
void undo_image(void) {
  if (has_undo()) {
    GBitmap *prev_img = NULL;
    if (s_undo_undo) {
      // If set to undo the undo, temporarily save the image before undoing
      prev_img = gbitmap_create_blank(GSize(IMG_WIDTH, IMG_HEIGHT));
      memcpy(prev_img->addr, s_image->addr, IMG_PIXELS);
    }
    
    // Roll back to the last undo
    memcpy(s_image->addr, s_undo_img->addr, IMG_PIXELS);
    
    if (s_undo_undo && prev_img != NULL) {
      // If set to undo the undo, set the undo image to the image before undoing
      memcpy(s_undo_img->addr, prev_img->addr, IMG_PIXELS);
      gbitmap_destroy(prev_img);
    } else {
      // Else dispose the undo image
      gbitmap_destroy(s_undo_img);
      s_undo_img = NULL;
    }
    
    vibes_short_pulse();
    layer_mark_dirty(s_canvaslayer);
  }
}

// Toggles 'pen' (drawing) on/off (down/up)
void toggle_pen(void) {
  if (s_eraser_on)
    s_eraser_on = false;
  else {
    // If about to turn the drawing on, save the current image for undoing
    if (!s_pen_down)
      save_undo();
    
    s_pen_down = !s_pen_down;
  }
  
  layer_mark_dirty(s_canvaslayer);
  if (s_pen_event != NULL) s_pen_event(s_pen_down, s_eraser_on);
}

// Toggles 'eraser' on/off
void toggle_eraser(void) {
  if (s_pen_down)
    s_pen_down = false;
  
  // If about to turn erasor on, save the current image for undoing
  if (!s_eraser_on) 
    save_undo();
  
  s_eraser_on = !s_eraser_on;
  layer_mark_dirty(s_canvaslayer);
  if (s_pen_event != NULL) s_pen_event(s_pen_down, s_eraser_on);
}

// Indicates if 'pen' is down (drawing is on)
bool is_pen_down(void) {
  return s_pen_down;
}

// Pauses drawing by setting 'pen' up
void set_paused(void) {
  s_pen_down = false;
  s_eraser_on = false;
  if (s_pen_event != NULL) s_pen_event(s_pen_down, s_eraser_on);
}

// Gets reference to the image pixel data
void* get_imagedata(void) {
  if (s_image == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Getting image data - NULL");
    return NULL;
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Getting image data - NOT NULL");
    return s_image->addr;
  }
}

// Initializes bitmap that stores the image data
void init_imagedata(void) {
  if (s_image == NULL)
    s_image = gbitmap_create_blank(GSize(IMG_WIDTH, IMG_HEIGHT));
}

// Updates the cursor location (if 'pen' is down this will draw on the screen)
void cursor_set_loc(GPoint loc) {
  if (loc.x != s_cursor_loc.x || loc.y != s_cursor_loc.y) {
    // If the cursor location has changed, update the screen
    s_last_loc = s_cursor_loc;
    s_cursor_loc = loc;
    layer_mark_dirty(s_canvaslayer);
  }
}

// Clears the image data and updates the screen
void clear_image(void) {
  if (s_image != NULL) {
    gbitmap_destroy(s_image);
    s_image = NULL;
    vibes_double_pulse();
    layer_mark_dirty(s_canvaslayer);
  }
}

// Indicates if the canvas window is on top of the stack (is displaying)
bool is_canvas_on_top() {
  if (s_window == NULL)
    return false;
  else 
    return (s_window == window_stack_get_top_window());
}

// Trap button clicks for this window
void init_click_events(ClickConfigProvider click_config_provider) {
  window_set_click_config_provider(s_window, click_config_provider);
}

// Initialize and show the main window with event callbacks
void show_canvas(PenStatusCallBack pen_event, CanvaseClosedCallBack closed_event) {
  s_pen_event = pen_event;
  s_canvas_closed = closed_event;
  s_cursor_loc = GPoint((IMG_WIDTH/2), (IMG_HEIGHT/2));
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
