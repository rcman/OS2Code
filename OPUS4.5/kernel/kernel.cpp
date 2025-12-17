#include "../kernel/types.h"
#include "../kernel/string.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../shell/shell.h"

// Draw a fancy OS/2 style boot screen
void draw_boot_screen() {
    vga.setColor(VGA_LIGHT_CYAN, VGA_BLUE);
    vga.clear();
    
    // Draw top border
    vga.setCursor(0, 0);
    for (int i = 0; i < 80; i++) vga.putchar(205);
    
    // OS/2 Clone banner
    vga.setCursor(20, 2);
    vga.setColor(VGA_WHITE, VGA_BLUE);
    vga.puts("OS/2 Clone Operating System");
    
    vga.setCursor(25, 3);
    vga.setColor(VGA_LIGHT_CYAN, VGA_BLUE);
    vga.puts("Version 1.0.0");
    
    // System info box
    vga.setCursor(5, 6);
    vga.setColor(VGA_YELLOW, VGA_BLUE);
    vga.puts("System Initialization");
    
    vga.setColor(VGA_WHITE, VGA_BLUE);
    vga.setCursor(5, 8);
    vga.puts("[*] Protected Mode................ OK");
    vga.setCursor(5, 9);
    vga.puts("[*] VGA Text Driver............... OK");
    vga.setCursor(5, 10);
    vga.puts("[*] Keyboard Driver............... OK");
    vga.setCursor(5, 11);
    vga.puts("[*] Memory Manager................ OK");
    vga.setCursor(5, 12);
    vga.puts("[*] Command Shell................. OK");
    
    vga.setCursor(5, 14);
    vga.setColor(VGA_LIGHT_GREEN, VGA_BLUE);
    vga.puts("System initialization complete!");
    
    vga.setCursor(5, 16);
    vga.setColor(VGA_LIGHT_CYAN, VGA_BLUE);
    vga.puts("Press any key to continue...");
    
    // Draw bottom border
    vga.setCursor(0, 24);
    vga.setColor(VGA_LIGHT_CYAN, VGA_BLUE);
    for (int i = 0; i < 80; i++) vga.putchar(205);
    
    // Wait for keypress
    keyboard.getchar();
}

void print_welcome() {
    vga.setColor(VGA_LIGHT_CYAN, VGA_BLACK);
    vga.puts("\n");
    vga.puts("  ___  ____   ______   ____ _                 \n");
    vga.puts(" / _ \\/ ___| / /___ \\ / ___| | ___  _ __   ___ \n");
    vga.puts("| | | \\___ \\/ /  __) | |   | |/ _ \\| '_ \\ / _ \\\n");
    vga.puts("| |_| |___) / /  / __/| |___| | (_) | | | |  __/\n");
    vga.puts(" \\___/|____/_/  |_____|\\____|_|\\___/|_| |_|\\___|\n");
    vga.puts("\n");
    
    vga.setColor(VGA_WHITE, VGA_BLACK);
    vga.puts(" OS/2 Clone Operating System [Version 1.0.0]\n");
    vga.puts(" (c) 2024 Educational Purposes Only\n\n");
    
    vga.setColor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga.puts(" Type HELP for a list of available commands.\n\n");
    
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
}

// Kernel entry point (called from assembly)
extern "C" void kernel_main() {
    // Initialize VGA
    vga.init();
    
    // Initialize keyboard
    keyboard.init();
    
    // Show boot screen
    draw_boot_screen();
    
    // Clear and show command prompt
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
    vga.clear();
    
    // Print welcome message
    print_welcome();
    
    // Initialize and run shell
    shell.init();
    shell.run();
    
    // Should never reach here
    while (true) {
        asm volatile ("hlt");
    }
}
