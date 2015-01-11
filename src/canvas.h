#include <pebble.h>

typedef void (*CanvaseClosedCallBack)();
typedef void (*PenStatusCallBack)(bool pen_down);
  
void set_drawingcursor(bool cursor_on);
void toggle_pen(void);
bool is_pen_down(void);
void set_paused(void);
void cursor_set_loc(GPoint loc);
void clear_image(void);
bool is_canvas_on_top();

void* get_imagedata(void);
void init_imagedata(void);

void init_click_events(ClickConfigProvider click_config_provider);
void show_canvas(PenStatusCallBack pen_event, CanvaseClosedCallBack closed_event);
void hide_canvas(void);
