#include <pebble.h>

void toggle_cursor(void);
void toggle_pen(void);
bool is_pen_down(void);
void set_paused(void);
void cursor_set_loc(GPoint loc);
void clear_image(void);

void init_click_events(ClickConfigProvider click_config_provider);
void show_canvas(void);
void hide_canvas(void);
