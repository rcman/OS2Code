// File: gui.c
// GUI Window Manager Implementation

#include "gui.h"
#include "vga_gfx.h"
#include "types.h"
#include "pmm.h"

// Actual screen dimensions from the active video mode. The GUI code
// previously hardcoded 1024x768 (the VBE mode) in many places, which
// broke hit-testing and clipping in the default 320x200 VGA mode:
// e.g. the taskbar was drawn at the real screen bottom but hit-tested
// at y>=738, so the Start button never responded to clicks.
static inline int gui_screen_w(void) { return vga_get_mode_info()->width; }
static inline int gui_screen_h(void) { return vga_get_mode_info()->height; }

// External I/O functions from io.asm
extern void outb(uint16_t port, uint8_t value);
extern void outw(uint16_t port, uint16_t value);

// String utility functions
static uint32_t str_len(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}

static void mem_set(void* dest, uint8_t val, uint32_t len) {
    uint8_t* d = (uint8_t*)dest;
    for (uint32_t i = 0; i < len; i++) {
        d[i] = val;
    }
}

static void mem_copy(void* dest, const void* src, uint32_t len) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < len; i++) {
        d[i] = s[i];
    }
}

// Forward declarations
static void gui_window_realloc_content(window_t* window);

// Pages needed for a rows x cols text buffer. At 1024x768 a fullscreen
// window needs ~12KB - a single page (as previously allocated) overflows
// into neighboring physical frames and corrupts them.
static uint32_t content_pages(int rows, int cols) {
    uint32_t size = (uint32_t)rows * (uint32_t)cols;
    uint32_t pages = (size + 4095) / 4096;
    return pages ? pages : 1;
}
extern uint32_t pmm_alloc_pages(uint32_t count);
extern void pmm_free_pages(uint32_t phys_addr, uint32_t count);
static void gui_save_cursor_area(void);
static void gui_restore_cursor_area(void);

// Global desktop state
static desktop_t desktop;

// Initialize GUI subsystem
void gui_init(void) {
    mem_set(&desktop, 0, sizeof(desktop_t));
    desktop.num_windows = 0;
    desktop.focused_window = -1;
    desktop.desktop_color = COLOR_DESKTOP;
    desktop.mouse_x = gui_screen_w() / 2;  // Center of screen
    desktop.mouse_y = gui_screen_h() / 2;
    desktop.mouse_buttons = 0;
    desktop.taskbar_visible = 1;  // Show taskbar by default
    desktop.cursor_saved = 0;  // No cursor area saved yet

    // Initialize Start Menu
    gui_init_start_menu();

    // Draw initial desktop
    gui_draw_desktop();
    gui_draw_taskbar();

    // Save area before drawing cursor for the first time
    gui_save_cursor_area();
    gui_draw_cursor();
}

// Draw a Workplace-Shell-style desktop icon glyph (32 wide) at (x,y).
static void gui_desktop_icon(int x, int y, int type, const char* label) {
    switch (type) {
        case 0:  // OS/2 System - a monitor
            vga_fill_rect32(x + 3, y + 2, 26, 18, 0x303030);
            vga_fill_rect32(x + 5, y + 4, 22, 14, 0x2060B0);
            vga_fill_rect32(x + 10, y + 22, 12, 4, 0x505050);
            vga_fill_rect32(x + 6, y + 26, 20, 3, 0x808080);
            break;
        case 1:  // Drives - a disk stack
            for (int i = 0; i < 3; i++) {
                vga_fill_rect32(x + 2, y + 2 + i * 9, 28, 7, 0xC8C8C8);
                vga_bevel32(x + 2, y + 2 + i * 9, 28, 7, 1);
                vga_fill_rect32(x + 5, y + 5 + i * 9, 10, 2, 0x606060);
                vga_fill_rect32(x + 25, y + 4 + i * 9, 3, 3, 0x30B030);
            }
            break;
        case 2:  // Programs - yellow folder
            vga_fill_rect32(x + 2, y + 8, 28, 20, 0xE0C040);
            vga_fill_rect32(x + 2, y + 5, 12, 5, 0xE0C040);
            vga_bevel32(x + 2, y + 8, 28, 20, 1);
            break;
        case 3:  // Information - blue folder with i
            vga_fill_rect32(x + 2, y + 8, 28, 20, 0x5090D0);
            vga_fill_rect32(x + 2, y + 5, 12, 5, 0x5090D0);
            vga_bevel32(x + 2, y + 8, 28, 20, 1);
            vga_draw_string32(x + 13, y + 13, "i", 0xFFFFFF);
            break;
        case 4:  // Shredder
            vga_fill_rect32(x + 5, y + 4, 22, 6, 0x808080);
            vga_bevel32(x + 5, y + 4, 22, 6, 1);
            for (int i = 0; i < 5; i++)
                vga_fill_rect32(x + 8 + i * 4, y + 12, 2, 14, 0xB0B0B0);
            vga_fill_rect32(x + 6, y + 26, 20, 3, 0x606060);
            break;
    }
    int len = 0; while (label[len]) len++;
    int lx = x + 16 - len * 4; if (lx < 2) lx = 2;
    vga_draw_string32(lx, y + 34, label, 0xE8F0F0);
}

// Draw desktop background
void gui_draw_desktop(void) {
    if (vga_get_mode_info()->bpp == 32) {
        // OS/2 Warp teal desktop with Workplace Shell object icons
        vga_fill_rect32(0, 0, gui_screen_w(), gui_screen_h(), 0x2E8B8B);
        gui_desktop_icon(20,  30, 0, "OS/2 System");
        gui_desktop_icon(20, 120, 1, "Drives");
        gui_desktop_icon(20, 210, 2, "Programs");
        gui_desktop_icon(20, 300, 3, "Information");
        gui_desktop_icon(20, 390, 4, "Shredder");
    } else {
        vga_clear_screen(desktop.desktop_color);
    }
    // Note: cursor will be drawn by caller (gui_draw_all_windows or gui_draw_cursor)
}

// Set desktop background color
void gui_set_desktop_color(uint8_t color) {
    desktop.desktop_color = color;
    gui_draw_desktop();
    gui_draw_all_windows();  // This calls gui_draw_cursor() at the end
}

// Create a new window
window_t* gui_create_window(int x, int y, int width, int height, const char* title) {
    if (desktop.num_windows >= MAX_WINDOWS) {
        return 0;  // No more windows available
    }

    window_t* window = &desktop.windows[desktop.num_windows++];
    mem_set(window, 0, sizeof(window_t));

    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->flags = WINDOW_VISIBLE | WINDOW_DRAGGABLE | WINDOW_CLOSABLE | WINDOW_RESIZABLE | WINDOW_MINIMIZABLE | WINDOW_MAXIMIZABLE;
    window->fg_color = 15;  // White text
    window->bg_color = COLOR_WINDOW_BACKGROUND;

    // Copy title
    uint32_t title_len = str_len(title);
    if (title_len > 63) title_len = 63;
    mem_copy(window->title, title, title_len);
    window->title[title_len] = '\0';

    // Calculate text area dimensions (8x8 font)
    window->content_cols = (width - 4) / 8;   // 2 pixel border on each side
    window->content_rows = (height - 16) / 8; // Title bar is 12 pixels, 2 pixel bottom border

    // Allocate content buffer (enough pages for the whole text area)
    uint32_t buffer_size = window->content_rows * window->content_cols;
    window->content = (char*)pmm_alloc_pages(
        content_pages(window->content_rows, window->content_cols));
    if (window->content) {
        mem_set(window->content, ' ', buffer_size);
    }

    window->cursor_row = 0;
    window->cursor_col = 0;
    window->scroll_offset = 0;
    window->dragging = 0;
    window->resizing = RESIZE_NONE;
    window->resize_start_x = 0;
    window->resize_start_y = 0;
    window->resize_start_w = 0;
    window->resize_start_h = 0;
    window->saved_x = 0;
    window->saved_y = 0;
    window->saved_width = 0;
    window->saved_height = 0;

    // Focus the new window
    gui_focus_window(window);

    // Draw the window
    gui_draw_window(window);

    return window;
}

// Destroy a window
void gui_destroy_window(window_t* window) {
    if (!window) return;

    // Free content buffer
    if (window->content) {
        pmm_free_pages((uint32_t)window->content,
                       content_pages(window->content_rows, window->content_cols));
        window->content = 0;
    }

    // Find window index
    int index = -1;
    for (int i = 0; i < desktop.num_windows; i++) {
        if (&desktop.windows[i] == window) {
            index = i;
            break;
        }
    }

    if (index == -1) return;

    // Shift remaining windows down
    for (int i = index; i < desktop.num_windows - 1; i++) {
        desktop.windows[i] = desktop.windows[i + 1];
    }
    desktop.num_windows--;

    // Update focused window
    if (desktop.focused_window == index) {
        desktop.focused_window = desktop.num_windows > 0 ? 0 : -1;
    } else if (desktop.focused_window > index) {
        desktop.focused_window--;
    }

    // Redraw desktop
    gui_draw_desktop();
    gui_draw_all_windows();
}

// Show window
void gui_show_window(window_t* window) {
    if (!window) return;
    window->flags |= WINDOW_VISIBLE;
    gui_draw_window(window);
    gui_draw_cursor();
}

// Hide window
void gui_hide_window(window_t* window) {
    if (!window) return;
    window->flags &= ~WINDOW_VISIBLE;
    gui_draw_desktop();
    gui_draw_all_windows();
}

// Focus window
void gui_focus_window(window_t* window) {
    if (!window) return;

    // Find window index
    for (int i = 0; i < desktop.num_windows; i++) {
        if (&desktop.windows[i] == window) {
            desktop.focused_window = i;
            desktop.windows[i].flags |= WINDOW_FOCUSED;
        } else {
            desktop.windows[i].flags &= ~WINDOW_FOCUSED;
        }
    }
}

// Draw window frame and title bar
void gui_draw_window(window_t* window) {
    if (!window || !(window->flags & WINDOW_VISIBLE)) return;

    int x = window->x;
    int y = window->y;
    int w = window->width;
    int h = window->height;

    int focused = (window->flags & WINDOW_FOCUSED) != 0;

    // OS/2 Workplace Shell window: raised gray frame around a sunken
    // content area, with a blue title bar (muted when inactive).
    vga_fill_rect32(x, y, w, h, 0xBDBDBD);
    vga_bevel32(x, y, w, h, 1);
    vga_fill_rect32(x + 2, y + 13, w - 4, h - 15, 0x0C1014);   // content bg
    vga_bevel32(x + 2, y + 12, w - 4, h - 14, 0);              // sunken content

    // Title bar
    vga_fill_rect32(x + 2, y + 1, w - 4, 11, focused ? 0x00308A : 0x64789A);

    // System-menu box (left) - OS/2's title-bar icon
    vga_fill_rect32(x + 3, y + 2, 9, 9, 0xBDBDBD);
    vga_bevel32(x + 3, y + 2, 9, 9, 1);
    vga_fill_rect32(x + 5, y + 6, 5, 2, 0x303030);

    // Title text
    vga_draw_string32(x + 15, y + 3, window->title, 0xFFFFFF);

    // Control buttons (right) - gray beveled, OS/2 style. Geometry
    // matches the *_button_hit_test() functions exactly.
    int button_size = 10;
    int button_y = y + 1;
    int button_x = x + w - 4;

    if (window->flags & WINDOW_CLOSABLE) {
        button_x -= button_size;
        vga_fill_rect32(button_x, button_y, button_size, button_size, 0xBDBDBD);
        vga_bevel32(button_x, button_y, button_size, button_size, 1);
        vga_draw_char32(button_x + 1, button_y + 1, 'x', 0x202020);
    }
    if (window->flags & WINDOW_MAXIMIZABLE) {
        button_x -= (button_size + 2);
        vga_fill_rect32(button_x, button_y, button_size, button_size, 0xBDBDBD);
        vga_bevel32(button_x, button_y, button_size, button_size, 1);
        vga_fill_rect32(button_x + 2, button_y + 2, 6, 6, 0xBDBDBD);
        vga_bevel32(button_x + 2, button_y + 2, 6, 6, 1);      // maximize square
    }
    if (window->flags & WINDOW_MINIMIZABLE) {
        button_x -= (button_size + 2);
        vga_fill_rect32(button_x, button_y, button_size, button_size, 0xBDBDBD);
        vga_bevel32(button_x, button_y, button_size, button_size, 1);
        vga_fill_rect32(button_x + 2, button_y + 6, 6, 2, 0x303030);  // minimize
    }

    // Draw window content
    if (window->content) {
        int text_x = x + 2;
        int text_y = y + 14;

        for (int row = 0; row < window->content_rows; row++) {
            for (int col = 0; col < window->content_cols; col++) {
                int offset = ((row + window->scroll_offset) % window->content_rows) * window->content_cols + col;
                char c = window->content[offset];
                vga_draw_char(text_x + col * 8, text_y + row * 8, c, window->fg_color);
            }
        }
    }

    // Draw cursor if focused
    if (window->flags & WINDOW_FOCUSED) {
        int cursor_x = x + 2 + window->cursor_col * 8;
        int cursor_y = y + 14 + (window->cursor_row - window->scroll_offset) * 8;
        if (cursor_y >= y + 14 && cursor_y < y + h - 2) {
            vga_draw_char(cursor_x, cursor_y, '_', window->fg_color);
        }
    }
}

// Draw all windows
void gui_draw_all_windows(void) {
    for (int i = 0; i < desktop.num_windows; i++) {
        gui_draw_window(&desktop.windows[i]);
    }
    if (desktop.taskbar_visible) {
        gui_draw_taskbar();
    }
    // Draw Start Menu on top of everything except cursor
    gui_draw_start_menu();
    gui_draw_cursor();
}

// Save the area under the cursor
static void gui_save_cursor_area(void) {
    int x = desktop.mouse_x;
    int y = desktop.mouse_y;

    for (int row = 0; row < CURSOR_HEIGHT; row++) {
        for (int col = 0; col < CURSOR_WIDTH; col++) {
            int px = x + col;
            int py = y + row;

            if (px >= 0 && px < gui_screen_w() && py >= 0 && py < gui_screen_h()) {
                desktop.cursor_save[row][col] = vga_get_pixel32(px, py);
            } else {
                desktop.cursor_save[row][col] = 0;  // Black for out of bounds
            }
        }
    }

    desktop.cursor_saved_x = x;
    desktop.cursor_saved_y = y;
    desktop.cursor_saved = 1;
}

// Restore the saved area under the cursor
static void gui_restore_cursor_area(void) {
    if (!desktop.cursor_saved) return;

    int x = desktop.cursor_saved_x;
    int y = desktop.cursor_saved_y;

    for (int row = 0; row < CURSOR_HEIGHT; row++) {
        for (int col = 0; col < CURSOR_WIDTH; col++) {
            int px = x + col;
            int py = y + row;

            if (px >= 0 && px < gui_screen_w() && py >= 0 && py < gui_screen_h()) {
                vga_plot_pixel32(px, py, desktop.cursor_save[row][col]);
            }
        }
    }

    desktop.cursor_saved = 0;
}

// Draw mouse cursor
void gui_draw_cursor(void) {
    int x = desktop.mouse_x;
    int y = desktop.mouse_y;

    // Large arrow cursor (20x32 pixels) - like standard OS cursors
    // 1 = white, 0 = black outline, -1 = transparent
    static const int cursor[32][20] = {
        {0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 1, 0, 0, -1, -1, 0, 0, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 0, 0, -1, -1, -1, -1, 0, 0, 1, 1, 0, 0, -1, -1, -1, -1, -1, -1},
        {0, 0, 0, -1, -1, -1, -1, -1, -1, 0, 0, 1, 1, 0, 0, -1, -1, -1, -1, -1},
        {0, 0, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 1, 0, 0, -1, -1, -1, -1},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 1, 0, 0, -1, -1, -1},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 1, 0, 0, -1, -1},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 1, 0, 0, -1},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 1, 0, 0},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0}
    };

    // Draw cursor
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 20; col++) {
            int pixel = cursor[row][col];
            if (pixel < 0) continue;  // Transparent

            int px = x + col;
            int py = y + row;

            // Check bounds
            if (px >= 0 && px < gui_screen_w() && py >= 0 && py < gui_screen_h()) {
                vga_plot_pixel(px, py, pixel == 1 ? 15 : 0);  // White or black
            }
        }
    }
}

// Draw taskbar at bottom of screen
void gui_draw_taskbar(void) {
    if (!desktop.taskbar_visible) return;

    int screen_width = gui_screen_w();
    int screen_height = gui_screen_h();
    int taskbar_y = screen_height - TASKBAR_HEIGHT;

    // OS/2 gray taskbar with a raised bevel
    vga_fill_rect32(0, taskbar_y, screen_width, TASKBAR_HEIGHT, 0xBDBDBD);
    vga_bevel32(0, taskbar_y, screen_width, TASKBAR_HEIGHT, 1);

    // OS/2 System button (Start)
    int start_button_width = 90;
    vga_fill_rect32(4, taskbar_y + 4, start_button_width, TASKBAR_HEIGHT - 8, 0xBDBDBD);
    vga_bevel32(4, taskbar_y + 4, start_button_width, TASKBAR_HEIGHT - 8, 1);
    vga_draw_string32(12, taskbar_y + 11, "OS/2 System", 0x000000);

    // Window buttons
    int button_x = start_button_width + 4 + TASKBAR_BUTTON_SPACING;
    for (int i = 0; i < desktop.num_windows; i++) {
        window_t* win = &desktop.windows[i];
        int focused = (i == desktop.focused_window && (win->flags & WINDOW_VISIBLE));
        vga_fill_rect32(button_x, taskbar_y + 4, TASKBAR_BUTTON_WIDTH, TASKBAR_HEIGHT - 8, 0xBDBDBD);
        vga_bevel32(button_x, taskbar_y + 4, TASKBAR_BUTTON_WIDTH, TASKBAR_HEIGHT - 8, !focused);
        vga_draw_string32(button_x + 6, taskbar_y + 11, win->title, 0x000000);
        button_x += TASKBAR_BUTTON_WIDTH + TASKBAR_BUTTON_SPACING;
    }

    // System tray (right)
    int systray_x = screen_width - 104;
    vga_fill_rect32(systray_x, taskbar_y + 4, 100, TASKBAR_HEIGHT - 8, 0xBDBDBD);
    vga_bevel32(systray_x, taskbar_y + 4, 100, TASKBAR_HEIGHT - 8, 0);
    vga_draw_string32(systray_x + 26, taskbar_y + 11, "OS/Two", 0x000000);
}

// Show taskbar
void gui_show_taskbar(void) {
    desktop.taskbar_visible = 1;
    gui_draw_all_windows();
}

// Hide taskbar
void gui_hide_taskbar(void) {
    desktop.taskbar_visible = 0;
    gui_draw_desktop();
    gui_draw_all_windows();
}

// Test if point is on taskbar
int gui_taskbar_hit_test(int x, int y) {
    if (!desktop.taskbar_visible) return 0;

    int taskbar_y = gui_screen_h() - TASKBAR_HEIGHT;
    return (y >= taskbar_y && y < gui_screen_h());
}

// Test which window button was clicked, returns window index or -1
int gui_taskbar_button_hit_test(int x, int y) {
    if (!gui_taskbar_hit_test(x, y)) return -1;

    int taskbar_y = gui_screen_h() - TASKBAR_HEIGHT;
    int button_x = 60 + 4 + TASKBAR_BUTTON_SPACING;  // After Start button

    for (int i = 0; i < desktop.num_windows; i++) {
        if (x >= button_x && x < button_x + TASKBAR_BUTTON_WIDTH &&
            y >= taskbar_y + 2 && y < taskbar_y + TASKBAR_HEIGHT - 2) {
            return i;
        }
        button_x += TASKBAR_BUTTON_WIDTH + TASKBAR_BUTTON_SPACING;
    }

    return -1;
}

// ============================================================================
// Start Menu Functions
// ============================================================================

// Helper function to copy string
static void str_copy_menu(char* dest, const char* src, int max_len) {
    int i = 0;
    while (i < max_len - 1 && src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Initialize Start Menu
void gui_init_start_menu(void) {
    desktop.start_menu_open = 0;
    desktop.start_menu_hover = -1;
    desktop.start_menu_num_items = 0;

    // Add menu items
    menu_item_t* item;

    // New Shell
    item = &desktop.start_menu_items[desktop.start_menu_num_items++];
    str_copy_menu(item->text, "New Shell", 32);
    item->type = MENU_ITEM_NORMAL;
    item->enabled = 1;

    // System Info
    item = &desktop.start_menu_items[desktop.start_menu_num_items++];
    str_copy_menu(item->text, "System Info", 32);
    item->type = MENU_ITEM_NORMAL;
    item->enabled = 1;

    // Separator
    item = &desktop.start_menu_items[desktop.start_menu_num_items++];
    str_copy_menu(item->text, "", 32);
    item->type = MENU_ITEM_SEPARATOR;
    item->enabled = 0;

    // Reboot
    item = &desktop.start_menu_items[desktop.start_menu_num_items++];
    str_copy_menu(item->text, "Reboot", 32);
    item->type = MENU_ITEM_NORMAL;
    item->enabled = 1;

    // Shutdown
    item = &desktop.start_menu_items[desktop.start_menu_num_items++];
    str_copy_menu(item->text, "Shutdown", 32);
    item->type = MENU_ITEM_NORMAL;
    item->enabled = 1;
}

// Draw Start Menu
void gui_draw_start_menu(void) {
    if (!desktop.start_menu_open) return;

    int menu_x = 2;
    int menu_y = gui_screen_h() - TASKBAR_HEIGHT - (desktop.start_menu_num_items * START_MENU_ITEM_HEIGHT);
    int menu_width = START_MENU_WIDTH;
    int menu_height = desktop.start_menu_num_items * START_MENU_ITEM_HEIGHT;

    // Draw menu background
    vga_fill_rect(menu_x, menu_y, menu_width, menu_height, 7);  // Light gray
    vga_draw_rect(menu_x, menu_y, menu_width, menu_height, 0);  // Black border

    // Draw menu items
    for (int i = 0; i < desktop.start_menu_num_items; i++) {
        menu_item_t* item = &desktop.start_menu_items[i];
        int item_y = menu_y + (i * START_MENU_ITEM_HEIGHT);

        if (item->type == MENU_ITEM_SEPARATOR) {
            // Draw separator line
            int sep_y = item_y + (START_MENU_ITEM_HEIGHT / 2);
            for (int x = menu_x + 4; x < menu_x + menu_width - 4; x++) {
                vga_plot_pixel(x, sep_y, 8);  // Dark gray
            }
        } else {
            // Draw menu item background (highlight if hovered)
            uint8_t bg_color = (i == desktop.start_menu_hover) ? 11 : 7;  // Light cyan : Light gray
            vga_fill_rect(menu_x + 1, item_y + 1, menu_width - 2, START_MENU_ITEM_HEIGHT - 2, bg_color);

            // Draw menu item text
            uint8_t text_color = item->enabled ? 0 : 8;  // Black : Dark gray
            vga_draw_string(menu_x + 10, item_y + 10, item->text, text_color);
        }
    }
}

// Toggle Start Menu
void gui_toggle_start_menu(void) {
    desktop.start_menu_open = !desktop.start_menu_open;
    desktop.start_menu_hover = -1;

    // Redraw everything
    gui_draw_desktop();
    gui_draw_all_windows();
}

// Close Start Menu
void gui_close_start_menu(void) {
    if (desktop.start_menu_open) {
        desktop.start_menu_open = 0;
        desktop.start_menu_hover = -1;

        // Redraw everything
        gui_draw_desktop();
        gui_draw_all_windows();
    }
}

// Start Menu hit test - returns item index or -1
int gui_start_menu_hit_test(int x, int y) {
    if (!desktop.start_menu_open) return -1;

    int menu_x = 2;
    int menu_y = gui_screen_h() - TASKBAR_HEIGHT - (desktop.start_menu_num_items * START_MENU_ITEM_HEIGHT);
    int menu_width = START_MENU_WIDTH;

    // Check if inside menu bounds
    if (x < menu_x || x >= menu_x + menu_width) return -1;
    if (y < menu_y || y >= menu_y + (desktop.start_menu_num_items * START_MENU_ITEM_HEIGHT)) return -1;

    // Calculate which item was clicked
    int item_index = (y - menu_y) / START_MENU_ITEM_HEIGHT;

    if (item_index >= 0 && item_index < desktop.start_menu_num_items) {
        menu_item_t* item = &desktop.start_menu_items[item_index];
        // Don't return separator or disabled items
        if (item->type == MENU_ITEM_SEPARATOR || !item->enabled) {
            return -1;
        }
        return item_index;
    }

    return -1;
}

// Execute menu item action
void gui_execute_menu_item(int item_index) {
    if (item_index < 0 || item_index >= desktop.start_menu_num_items) return;

    menu_item_t* item = &desktop.start_menu_items[item_index];

    // Close menu first
    gui_close_start_menu();

    // Execute action based on item text
    if (item->text[0] == 'N' && item->text[1] == 'e' && item->text[2] == 'w') {  // "New Shell"
        // Create a new shell window
        window_t* window = gui_create_window(100 + (desktop.num_windows * 20),
                                              100 + (desktop.num_windows * 20),
                                              600, 400, "Shell");
        if (window) {
            gui_show_window(window);
            gui_focus_window(window);
            gui_draw_all_windows();
        }
    } else if (item->text[0] == 'S' && item->text[1] == 'y') {  // "System Info"
        // Create system info window
        window_t* window = gui_create_window(150, 150, 500, 350, "System Information");
        if (window) {
            gui_window_clear(window);
            gui_window_print(window, "OS/Two v0.3 (Alpha)\n");
            gui_window_print(window, "OS/2-Compatible Operating System\n");
            gui_window_print(window, "\n");
            gui_window_print(window, "Graphics: VBE 1024x768x32\n");
            gui_window_print(window, "GUI: Window Manager Active\n");
            gui_show_window(window);
            gui_focus_window(window);
            gui_draw_all_windows();
        }
    } else if (item->text[0] == 'R' && item->text[1] == 'e') {  // "Reboot"
        // Reboot the system
        outb(0x64, 0xFE);  // Keyboard controller reset
    } else if (item->text[0] == 'S' && item->text[1] == 'h') {  // "Shutdown"
        // Shutdown using ACPI (simplified)
        outw(0x604, 0x2000);  // QEMU/Bochs shutdown
    }
}

// ============================================================================

// Clear window content
void gui_window_clear(window_t* window) {
    if (!window || !window->content) return;

    uint32_t buffer_size = window->content_rows * window->content_cols;
    mem_set(window->content, ' ', buffer_size);
    window->cursor_row = 0;
    window->cursor_col = 0;
    window->scroll_offset = 0;

    gui_draw_window(window);
}

// Scroll window content up by one line
void gui_window_scroll_up(window_t* window) {
    if (!window || !window->content) return;

    // Shift all lines up
    for (int row = 0; row < window->content_rows - 1; row++) {
        for (int col = 0; col < window->content_cols; col++) {
            int dst = row * window->content_cols + col;
            int src = (row + 1) * window->content_cols + col;
            window->content[dst] = window->content[src];
        }
    }

    // Clear last line
    int last_row = window->content_rows - 1;
    for (int col = 0; col < window->content_cols; col++) {
        window->content[last_row * window->content_cols + col] = ' ';
    }
}

// Put character in window
void gui_window_putchar(window_t* window, char c) {
    if (!window || !window->content) return;

    if (c == '\n') {
        // Newline
        window->cursor_col = 0;
        window->cursor_row++;
        if (window->cursor_row >= window->content_rows) {
            gui_window_scroll_up(window);
            window->cursor_row = window->content_rows - 1;
        }
    } else if (c == '\b') {
        // Backspace
        if (window->cursor_col > 0) {
            window->cursor_col--;
            int offset = window->cursor_row * window->content_cols + window->cursor_col;
            window->content[offset] = ' ';
        }
    } else if (c >= 32 && c < 127) {
        // Printable character
        if (window->cursor_col >= window->content_cols) {
            window->cursor_col = 0;
            window->cursor_row++;
            if (window->cursor_row >= window->content_rows) {
                gui_window_scroll_up(window);
                window->cursor_row = window->content_rows - 1;
            }
        }

        int offset = window->cursor_row * window->content_cols + window->cursor_col;
        window->content[offset] = c;
        window->cursor_col++;
    }
}

// Print string to window
void gui_window_print(window_t* window, const char* str) {
    if (!window || !str) return;

    while (*str) {
        gui_window_putchar(window, *str);
        str++;
    }

    gui_draw_window(window);
}

// Handle mouse movement
void gui_handle_mouse_move(int x, int y) {
    desktop.mouse_x = x;
    desktop.mouse_y = y;

    bool needs_full_redraw = false;

    // Update Start Menu hover state
    if (desktop.start_menu_open) {
        int old_hover = desktop.start_menu_hover;
        desktop.start_menu_hover = gui_start_menu_hit_test(x, y);

        // Redraw if hover changed
        if (old_hover != desktop.start_menu_hover) {
            needs_full_redraw = true;
        }
    }

    // Handle window operations
    if (desktop.focused_window >= 0) {
        window_t* window = &desktop.windows[desktop.focused_window];

        // Handle window dragging
        if (window->dragging) {
            int dx = x - window->drag_start_x;
            int dy = y - window->drag_start_y;

            // Restore old cursor area before redrawing
            gui_restore_cursor_area();

            // Redraw desktop to clear old window position
            gui_draw_desktop();

            // Move window
            window->x += dx;
            window->y += dy;

            // Clamp to screen bounds (get screen dimensions from VGA)
            int screen_w = gui_screen_w();
            int screen_h = gui_screen_h();
            if (window->x < 0) window->x = 0;
            if (window->y < 0) window->y = 0;
            if (window->x + window->width > screen_w) window->x = screen_w - window->width;
            if (window->y + window->height > screen_h) window->y = screen_h - window->height;

            window->drag_start_x = x;
            window->drag_start_y = y;

            // Redraw all windows (includes cursor)
            gui_draw_all_windows();
            // Save cursor area for next movement
            gui_save_cursor_area();
            return;  // Already redrawn, exit early
        }
        // Handle window resizing
        else if (window->resizing != RESIZE_NONE) {
            int dx = x - window->resize_start_x;
            int dy = y - window->resize_start_y;

            // Restore old cursor area before redrawing
            gui_restore_cursor_area();

            // Redraw desktop to clear old window
            gui_draw_desktop();

            // Calculate new dimensions based on resize mode
            int new_x = window->x;
            int new_y = window->y;
            int new_w = window->resize_start_w;
            int new_h = window->resize_start_h;

            switch (window->resizing) {
                case RESIZE_LEFT:
                    new_x = window->x + dx;
                    new_w = window->resize_start_w - dx;
                    break;
                case RESIZE_RIGHT:
                    new_w = window->resize_start_w + dx;
                    break;
                case RESIZE_TOP:
                    new_y = window->y + dy;
                    new_h = window->resize_start_h - dy;
                    break;
                case RESIZE_BOTTOM:
                    new_h = window->resize_start_h + dy;
                    break;
                case RESIZE_TOP_LEFT:
                    new_x = window->x + dx;
                    new_y = window->y + dy;
                    new_w = window->resize_start_w - dx;
                    new_h = window->resize_start_h - dy;
                    break;
                case RESIZE_TOP_RIGHT:
                    new_y = window->y + dy;
                    new_w = window->resize_start_w + dx;
                    new_h = window->resize_start_h - dy;
                    break;
                case RESIZE_BOTTOM_LEFT:
                    new_x = window->x + dx;
                    new_w = window->resize_start_w - dx;
                    new_h = window->resize_start_h + dy;
                    break;
                case RESIZE_BOTTOM_RIGHT:
                    new_w = window->resize_start_w + dx;
                    new_h = window->resize_start_h + dy;
                    break;
            }

            // Apply minimum size constraints
            if (new_w < MIN_WINDOW_WIDTH) {
                if (window->resizing == RESIZE_LEFT || window->resizing == RESIZE_TOP_LEFT ||
                    window->resizing == RESIZE_BOTTOM_LEFT) {
                    new_x = window->x + window->width - MIN_WINDOW_WIDTH;
                }
                new_w = MIN_WINDOW_WIDTH;
            }
            if (new_h < MIN_WINDOW_HEIGHT) {
                if (window->resizing == RESIZE_TOP || window->resizing == RESIZE_TOP_LEFT ||
                    window->resizing == RESIZE_TOP_RIGHT) {
                    new_y = window->y + window->height - MIN_WINDOW_HEIGHT;
                }
                new_h = MIN_WINDOW_HEIGHT;
            }

            // Update window dimensions
            window->x = new_x;
            window->y = new_y;
            window->width = new_w;
            window->height = new_h;

            // Reallocate content buffer if size changed
            gui_window_realloc_content(window);

            // Redraw all windows (includes cursor)
            gui_draw_all_windows();
            // Save cursor area for next movement
            gui_save_cursor_area();
            return;  // Already redrawn, exit early
        }
    }

    // For normal mouse movement, use cursor save/restore for flicker-free updates
    if (needs_full_redraw) {
        // Full redraw needed (Start Menu hover changed, etc.)
        // Restore old cursor area first
        gui_restore_cursor_area();
        gui_draw_desktop();
        gui_draw_all_windows();
        // Note: gui_draw_all_windows() calls gui_draw_cursor() which draws at the new position
        // We need to save the new position after drawing
        gui_save_cursor_area();
    } else {
        // Just move the cursor - restore old area, save new area (before drawing!), then draw cursor
        gui_restore_cursor_area();
        gui_save_cursor_area();
        gui_draw_cursor();
    }
}

// Handle mouse button down
void gui_handle_mouse_down(int x, int y, int button) {
    desktop.mouse_buttons |= (1 << button);

    if (button == 0) {  // Left button
        // Check for Start Menu item clicks first
        int menu_item = gui_start_menu_hit_test(x, y);
        if (menu_item >= 0) {
            gui_execute_menu_item(menu_item);
            return;
        }

        // Check for Start button click
        if (gui_taskbar_hit_test(x, y)) {
            int taskbar_y = gui_screen_h() - TASKBAR_HEIGHT;
            if (x >= 2 && x < 2 + 60 && y >= taskbar_y + 2 && y < taskbar_y + TASKBAR_HEIGHT - 2) {
                gui_toggle_start_menu();
                return;
            }
        }

        // If Start Menu is open and click is outside menu, close it
        if (desktop.start_menu_open) {
            gui_close_start_menu();
            // Don't return - allow click to be processed
        }

        // Check for taskbar button clicks
        int taskbar_window = gui_taskbar_button_hit_test(x, y);
        if (taskbar_window >= 0) {
            window_t* window = &desktop.windows[taskbar_window];
            // If window is hidden/minimized, show and focus it
            if (!(window->flags & WINDOW_VISIBLE)) {
                gui_show_window(window);
                gui_focus_window(window);
            }
            // If already visible and focused, minimize it
            else if (taskbar_window == desktop.focused_window) {
                gui_minimize_window(window);
            }
            // If visible but not focused, focus it
            else {
                gui_focus_window(window);
            }
            gui_draw_all_windows();
            return;
        }

        // Find window at point
        window_t* window = gui_window_at_point(x, y);
        if (window) {
            // Focus window
            gui_focus_window(window);

            // Check for window control button clicks first
            if (gui_window_close_button_hit_test(window, x, y)) {
                gui_close_window(window);
                gui_draw_all_windows();
                return;
            }
            else if (gui_window_maximize_button_hit_test(window, x, y)) {
                if (window->flags & WINDOW_MAXIMIZED) {
                    gui_restore_window(window);
                } else {
                    gui_maximize_window(window);
                }
                return;
            }
            else if (gui_window_minimize_button_hit_test(window, x, y)) {
                gui_minimize_window(window);
                gui_draw_all_windows();
                return;
            }
            // Check for resize edge/corner
            else {
                int resize_mode = gui_window_resize_hit_test(window, x, y);
                if (resize_mode != RESIZE_NONE) {
                    // Start resizing
                    window->resizing = resize_mode;
                    window->resize_start_x = x;
                    window->resize_start_y = y;
                    window->resize_start_w = window->width;
                    window->resize_start_h = window->height;
                }
                // Check if clicking on title bar
                else if (gui_window_title_bar_hit_test(window, x, y)) {
                    if (window->flags & WINDOW_DRAGGABLE) {
                        window->dragging = 1;
                        window->drag_start_x = x;
                        window->drag_start_y = y;
                    }
                }
            }

            gui_draw_all_windows();
        }
    }
}

// Handle mouse button up
void gui_handle_mouse_up(int x, int y, int button) {
    (void)x;  // Unused
    (void)y;  // Unused
    desktop.mouse_buttons &= ~(1 << button);

    if (button == 0) {  // Left button
        // Stop dragging and resizing
        if (desktop.focused_window >= 0) {
            desktop.windows[desktop.focused_window].dragging = 0;
            desktop.windows[desktop.focused_window].resizing = RESIZE_NONE;
        }
    }
}

// Find window at point (topmost)
window_t* gui_window_at_point(int x, int y) {
    // Check from top to bottom (reverse order)
    for (int i = desktop.num_windows - 1; i >= 0; i--) {
        window_t* window = &desktop.windows[i];
        if (!(window->flags & WINDOW_VISIBLE)) continue;

        if (x >= window->x && x < window->x + window->width &&
            y >= window->y && y < window->y + window->height) {
            return window;
        }
    }
    return 0;
}

// Test if point is in window title bar
int gui_window_title_bar_hit_test(window_t* window, int x, int y) {
    if (!window) return 0;

    return (x >= window->x && x < window->x + window->width &&
            y >= window->y && y < window->y + 12);
}

// Test if point is on resize edge/corner
int gui_window_resize_hit_test(window_t* window, int x, int y) {
    if (!window || !(window->flags & WINDOW_RESIZABLE)) return RESIZE_NONE;

    int wx = window->x;
    int wy = window->y;
    int ww = window->width;
    int wh = window->height;

    // Check if point is within window bounds
    if (x < wx || x >= wx + ww || y < wy || y >= wy + wh) {
        return RESIZE_NONE;
    }

    // Check corners first (priority over edges)
    if (x >= wx && x < wx + RESIZE_EDGE_SIZE && y >= wy && y < wy + RESIZE_EDGE_SIZE) {
        return RESIZE_TOP_LEFT;
    }
    if (x >= wx + ww - RESIZE_EDGE_SIZE && x < wx + ww && y >= wy && y < wy + RESIZE_EDGE_SIZE) {
        return RESIZE_TOP_RIGHT;
    }
    if (x >= wx && x < wx + RESIZE_EDGE_SIZE && y >= wy + wh - RESIZE_EDGE_SIZE && y < wy + wh) {
        return RESIZE_BOTTOM_LEFT;
    }
    if (x >= wx + ww - RESIZE_EDGE_SIZE && x < wx + ww && y >= wy + wh - RESIZE_EDGE_SIZE && y < wy + wh) {
        return RESIZE_BOTTOM_RIGHT;
    }

    // Check edges
    if (x >= wx && x < wx + RESIZE_EDGE_SIZE) {
        return RESIZE_LEFT;
    }
    if (x >= wx + ww - RESIZE_EDGE_SIZE && x < wx + ww) {
        return RESIZE_RIGHT;
    }
    if (y >= wy && y < wy + RESIZE_EDGE_SIZE) {
        return RESIZE_TOP;
    }
    if (y >= wy + wh - RESIZE_EDGE_SIZE && y < wy + wh) {
        return RESIZE_BOTTOM;
    }

    return RESIZE_NONE;
}

// Test if point is on close button
int gui_window_close_button_hit_test(window_t* window, int x, int y) {
    if (!window || !(window->flags & WINDOW_CLOSABLE)) return 0;

    int button_size = 10;
    int button_x = window->x + window->width - 4 - button_size;
    int button_y = window->y + 1;

    return (x >= button_x && x < button_x + button_size &&
            y >= button_y && y < button_y + button_size);
}

// Test if point is on maximize button
int gui_window_maximize_button_hit_test(window_t* window, int x, int y) {
    if (!window || !(window->flags & WINDOW_MAXIMIZABLE)) return 0;

    int button_size = 10;
    int button_x = window->x + window->width - 4 - button_size;
    if (window->flags & WINDOW_CLOSABLE) {
        button_x -= (button_size + 2);
    }
    int button_y = window->y + 1;

    return (x >= button_x && x < button_x + button_size &&
            y >= button_y && y < button_y + button_size);
}

// Test if point is on minimize button
int gui_window_minimize_button_hit_test(window_t* window, int x, int y) {
    if (!window || !(window->flags & WINDOW_MINIMIZABLE)) return 0;

    int button_size = 10;
    int button_x = window->x + window->width - 4 - button_size;
    if (window->flags & WINDOW_CLOSABLE) {
        button_x -= (button_size + 2);
    }
    if (window->flags & WINDOW_MAXIMIZABLE) {
        button_x -= (button_size + 2);
    }
    int button_y = window->y + 1;

    return (x >= button_x && x < button_x + button_size &&
            y >= button_y && y < button_y + button_size);
}

// Close window
void gui_close_window(window_t* window) {
    if (!window) return;
    gui_destroy_window(window);
}

// Minimize window (hide it for now, until we have a taskbar)
void gui_minimize_window(window_t* window) {
    if (!window) return;
    gui_hide_window(window);
}

// Maximize window (fill screen)
void gui_maximize_window(window_t* window) {
    if (!window || (window->flags & WINDOW_MAXIMIZED)) return;

    // Save current position and size
    window->saved_x = window->x;
    window->saved_y = window->y;
    window->saved_width = window->width;
    window->saved_height = window->height;

    // Maximize to screen size (accounting for taskbar)
    window->x = 0;
    window->y = 0;
    window->width = gui_screen_w();
    window->height = gui_screen_h() - TASKBAR_HEIGHT;  // Leave room for taskbar
    window->flags |= WINDOW_MAXIMIZED;

    // Reallocate content buffer
    gui_window_realloc_content(window);

    // Redraw
    gui_draw_desktop();
    gui_draw_all_windows();
}

// Restore window from maximized state
void gui_restore_window(window_t* window) {
    if (!window || !(window->flags & WINDOW_MAXIMIZED)) return;

    // Restore saved position and size
    window->x = window->saved_x;
    window->y = window->saved_y;
    window->width = window->saved_width;
    window->height = window->saved_height;
    window->flags &= ~WINDOW_MAXIMIZED;

    // Reallocate content buffer
    gui_window_realloc_content(window);

    // Redraw
    gui_draw_desktop();
    gui_draw_all_windows();
}

// Reallocate window content buffer (helper for resizing)
static void gui_window_realloc_content(window_t* window) {
    if (!window) return;

    // Calculate new text area dimensions
    int new_cols = (window->width - 4) / 8;
    int new_rows = (window->height - 16) / 8;

    if (new_cols == window->content_cols && new_rows == window->content_rows) {
        return;  // No change needed
    }

    // Allocate new buffer
    char* new_content = (char*)pmm_alloc_pages(content_pages(new_rows, new_cols));
    if (!new_content) return;

    uint32_t new_size = new_rows * new_cols;
    mem_set(new_content, ' ', new_size);

    // Copy old content to new buffer (as much as fits)
    if (window->content) {
        int copy_rows = (new_rows < window->content_rows) ? new_rows : window->content_rows;
        int copy_cols = (new_cols < window->content_cols) ? new_cols : window->content_cols;

        for (int row = 0; row < copy_rows; row++) {
            for (int col = 0; col < copy_cols; col++) {
                int old_offset = row * window->content_cols + col;
                int new_offset = row * new_cols + col;
                new_content[new_offset] = window->content[old_offset];
            }
        }

        // Free old buffer
        pmm_free_pages((uint32_t)window->content,
                       content_pages(window->content_rows, window->content_cols));
    }

    // Update window
    window->content = new_content;
    window->content_rows = new_rows;
    window->content_cols = new_cols;

    // Adjust cursor position if needed
    if (window->cursor_row >= new_rows) {
        window->cursor_row = new_rows - 1;
    }
    if (window->cursor_col >= new_cols) {
        window->cursor_col = new_cols - 1;
    }
}

// Get desktop state
desktop_t* gui_get_desktop(void) {
    return &desktop;
}
