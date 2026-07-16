// File: gui.h
// GUI Window Manager for OS/Two

#ifndef GUI_H
#define GUI_H

#include "types.h"

// Window flags
#define WINDOW_VISIBLE      0x01
#define WINDOW_FOCUSED      0x02
#define WINDOW_DRAGGABLE    0x04
#define WINDOW_CLOSABLE     0x08
#define WINDOW_RESIZABLE    0x10
#define WINDOW_MINIMIZABLE  0x20
#define WINDOW_MAXIMIZABLE  0x40
#define WINDOW_MAXIMIZED    0x80

// Resize modes
#define RESIZE_NONE         0
#define RESIZE_LEFT         1
#define RESIZE_RIGHT        2
#define RESIZE_TOP          3
#define RESIZE_BOTTOM       4
#define RESIZE_TOP_LEFT     5
#define RESIZE_TOP_RIGHT    6
#define RESIZE_BOTTOM_LEFT  7
#define RESIZE_BOTTOM_RIGHT 8

// Resize edge detection threshold (pixels)
#define RESIZE_EDGE_SIZE    5

// Minimum window size
#define MIN_WINDOW_WIDTH    200
#define MIN_WINDOW_HEIGHT   100

// Window colors
#define COLOR_WINDOW_FRAME      7   // Light gray
#define COLOR_WINDOW_TITLE_BAR  1   // Blue
#define COLOR_WINDOW_TITLE_TEXT 15  // White
#define COLOR_WINDOW_BACKGROUND 0   // Black
#define COLOR_DESKTOP           3   // Cyan/Blue

// Maximum windows
#define MAX_WINDOWS 8

// Taskbar settings
#define TASKBAR_HEIGHT 30
#define TASKBAR_BUTTON_WIDTH 120
#define TASKBAR_BUTTON_SPACING 2

// Start Menu settings
#define START_MENU_WIDTH 150
#define START_MENU_ITEM_HEIGHT 30
#define START_MENU_MAX_ITEMS 6

// Start Menu item types
#define MENU_ITEM_NORMAL 0
#define MENU_ITEM_SEPARATOR 1

// Menu item structure
typedef struct {
    char text[32];      // Menu item text
    uint8_t type;       // MENU_ITEM_NORMAL or MENU_ITEM_SEPARATOR
    uint8_t enabled;    // 1 if enabled, 0 if disabled
} menu_item_t;

// Window structure
typedef struct {
    int x, y;                   // Position
    int width, height;          // Dimensions
    char title[64];             // Window title
    uint8_t flags;              // Window flags
    uint8_t fg_color;           // Foreground color
    uint8_t bg_color;           // Background color

    // Text content buffer
    char* content;              // Text content
    int content_rows;           // Number of rows
    int content_cols;           // Number of columns
    int cursor_row;             // Current cursor row
    int cursor_col;             // Current cursor column
    int scroll_offset;          // Scroll offset for content

    // Mouse drag state
    int drag_start_x;
    int drag_start_y;
    int dragging;

    // Resize state
    int resizing;               // Resize mode (RESIZE_*)
    int resize_start_x;         // Mouse X when resize started
    int resize_start_y;         // Mouse Y when resize started
    int resize_start_w;         // Window width when resize started
    int resize_start_h;         // Window height when resize started

    // Saved position/size for maximize/restore
    int saved_x, saved_y;
    int saved_width, saved_height;
} window_t;

// Cursor dimensions
#define CURSOR_WIDTH 20
#define CURSOR_HEIGHT 32

// Desktop state
typedef struct {
    window_t windows[MAX_WINDOWS];
    int num_windows;
    int focused_window;         // Index of focused window (-1 if none)
    uint8_t desktop_color;      // Desktop background color

    // Mouse state for window manager
    int mouse_x;
    int mouse_y;
    int mouse_buttons;

    // Cursor save buffer for flicker-free cursor drawing (supports both 8-bit and 32-bit modes)
    uint32_t cursor_save[CURSOR_HEIGHT][CURSOR_WIDTH];
    int cursor_saved_x;
    int cursor_saved_y;
    int cursor_saved;           // 1 if cursor area is saved

    // Taskbar state
    int taskbar_visible;        // 1 if taskbar is shown

    // Start Menu state
    int start_menu_open;        // 1 if start menu is visible
    int start_menu_hover;       // Index of hovered menu item (-1 if none)
    menu_item_t start_menu_items[START_MENU_MAX_ITEMS];
    int start_menu_num_items;
} desktop_t;

// Initialize GUI subsystem
void gui_init(void);

// Desktop management
void gui_draw_desktop(void);
void gui_set_desktop_color(uint8_t color);
void gui_draw_cursor(void);

// Taskbar management
void gui_draw_taskbar(void);
void gui_show_taskbar(void);
void gui_hide_taskbar(void);
int gui_taskbar_hit_test(int x, int y);
int gui_taskbar_button_hit_test(int x, int y);  // Returns window index or -1

// Window management
window_t* gui_create_window(int x, int y, int width, int height, const char* title);
void gui_destroy_window(window_t* window);
void gui_show_window(window_t* window);
void gui_hide_window(window_t* window);
void gui_focus_window(window_t* window);
void gui_draw_window(window_t* window);
void gui_draw_all_windows(void);
void gui_close_window(window_t* window);
void gui_minimize_window(window_t* window);
void gui_maximize_window(window_t* window);
void gui_restore_window(window_t* window);

// Window text operations
void gui_window_clear(window_t* window);
void gui_window_putchar(window_t* window, char c);
void gui_window_print(window_t* window, const char* str);
void gui_window_scroll_up(window_t* window);

// Mouse event handling
void gui_handle_mouse_move(int x, int y);
void gui_handle_mouse_down(int x, int y, int button);
void gui_handle_mouse_up(int x, int y, int button);

// Window hit testing
window_t* gui_window_at_point(int x, int y);
int gui_window_title_bar_hit_test(window_t* window, int x, int y);
int gui_window_resize_hit_test(window_t* window, int x, int y);
int gui_window_close_button_hit_test(window_t* window, int x, int y);
int gui_window_minimize_button_hit_test(window_t* window, int x, int y);
int gui_window_maximize_button_hit_test(window_t* window, int x, int y);

// Get desktop state
desktop_t* gui_get_desktop(void);

// Start Menu management
void gui_init_start_menu(void);
void gui_draw_start_menu(void);
void gui_toggle_start_menu(void);
void gui_close_start_menu(void);
int gui_start_menu_hit_test(int x, int y);  // Returns item index or -1
void gui_execute_menu_item(int item_index);

#endif // GUI_H
