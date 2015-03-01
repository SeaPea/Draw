#include "canvas.h"
#include "common.h"
#include "intmath.h"

// Canvas is main window of the application that draws the image
  
static Window *s_window;
static Layer *s_canvaslayer;

static PenStatusCallBack s_pen_event;
static CanvaseClosedCallBack s_canvas_closed;

static bool s_drawingcursor_on = true;
static int8_t s_pen_width = 1;
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

// Draw line with width
// (Based on code found here http://rosettacode.org/wiki/Bitmap/Bresenham's_line_algorithm#C)
static void graphics_draw_line2(GContext *ctx, GPoint p0, GPoint p1, int8_t width) {
  // Order points so that lower x is first
  int16_t x0, x1, y0, y1;
  if (p0.x <= p1.x) {
    x0 = p0.x; x1 = p1.x; y0 = p0.y; y1 = p1.y;
  } else {
    x0 = p1.x; x1 = p0.x; y0 = p1.y; y1 = p0.y;
  }
  
  // Init loop variables
  int16_t dx = x1-x0;
  int16_t dy = abs(y1-y0);
  int16_t sy = y0<y1 ? 1 : -1; 
  int16_t err = (dx>dy ? dx : -dy)/2;
  int16_t e2;
  
  // Calculate whether line thickness will be added vertically or horizontally based on line angle
  int8_t xdiff, ydiff;
  
  if (dx > dy) {
    xdiff = 0;
    ydiff = width/2;
  } else {
    xdiff = width/2;
    ydiff = 0;
  }
  
  // Use Bresenham's integer algorithm, with slight modification for line width, to draw line at any angle
  while (true) {
    // Draw line thickness at each point by drawing another line 
    // (horizontally when > +/-45 degrees, vertically when <= +/-45 degrees)
    graphics_draw_line(ctx, GPoint(x0-xdiff, y0-ydiff), GPoint(x0+xdiff, y0+ydiff));
    
    if (x0==x1 && y0==y1) break;
    e2 = err;
    if (e2 >-dx) { err -= dy; x0++; }
    if (e2 < dy) { err += dx; y0 += sy; }
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
      // Pen is down, so use graphics routines to draw to the framebuffer (which will then be copied to save image)
      graphics_context_set_stroke_color(ctx, GColorBlack);
      if (s_pen_width == 1) {
        if (abs(s_cursor_loc.x-s_last_loc.x) > 1 || abs(s_cursor_loc.y-s_last_loc.y) > 1)
          // Draw line between last and current cursor position if it jumps more than 1 pixel
          graphics_draw_line(ctx, s_last_loc, s_cursor_loc);
        else
          graphics_draw_pixel(ctx, s_cursor_loc);
      } else {
        graphics_context_set_fill_color(ctx, GColorBlack);
          
        if (abs(s_cursor_loc.x-s_last_loc.x) > 1 || abs(s_cursor_loc.y-s_last_loc.y) > 1)
          // Draw line (with thickness) between last and current cursor position if it jumps more than 1 pixel
          graphics_draw_line2(ctx, s_last_loc, s_cursor_loc, s_pen_width);

        // Round end of line/current cursor point
        graphics_fill_circle(ctx, s_cursor_loc, s_pen_width/2);
      }
    } else if (s_eraser_on) {
      // Draw a WxW white square to 'erase' the current location
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, GRect(s_cursor_loc.x-(s_eraser_width/2), s_cursor_loc.y-(s_eraser_width/2), s_eraser_width, s_eraser_width), 0, GCornerNone);
    }
    
    // Create bitmap to store framebuffer pixel data after drawing line
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

void set_penwith(int width) {
  s_pen_width = width;
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
