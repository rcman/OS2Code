#include "../shell/shell.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../kernel/string.h"

Shell shell;

// Simulated filesystem entries
struct FileEntry {
    const char* name;
    const char* content;
    bool is_dir;
};

static FileEntry root_files[] = {
    {"OS2", NULL, true},
    {"DOCS", NULL, true},
    {"CONFIG.SYS", "PROTSHELL=C:\\OS2\\CMD.EXE\nSET PATH=C:\\OS2;C:\\OS2\\SYSTEM\nSET DPATH=C:\\OS2;C:\\OS2\\SYSTEM\nSET LIBPATH=.;C:\\OS2\\DLL\n", false},
    {"AUTOEXEC.BAT", "@ECHO OFF\nECHO Welcome to OS/2 Clone!\nSET PROMPT=$P$G\n", false},
    {"README.TXT", "OS/2 Clone Operating System\n===========================\n\nThis is a minimal OS/2-style operating system clone.\nBuilt for educational purposes.\n\nType HELP for a list of commands.\n", false},
    {NULL, NULL, false}
};

void Shell::init() {
    cmd_pos = 0;
    memset(cmd_buffer, 0, MAX_CMD_LENGTH);
    strcpy(current_dir, "C:\\");
}

void Shell::printPrompt() {
    vga.setColor(VGA_LIGHT_CYAN, VGA_BLACK);
    vga.puts("[");
    vga.setColor(VGA_WHITE, VGA_BLACK);
    vga.puts(current_dir);
    vga.setColor(VGA_LIGHT_CYAN, VGA_BLACK);
    vga.puts("]");
    vga.setColor(VGA_YELLOW, VGA_BLACK);
    vga.puts("$ ");
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
}

void Shell::readLine() {
    cmd_pos = 0;
    memset(cmd_buffer, 0, MAX_CMD_LENGTH);
    
    while (true) {
        char c = keyboard.getchar();
        
        if (c == '\n') {
            vga.putchar('\n');
            return;
        } else if (c == '\b') {
            if (cmd_pos > 0) {
                cmd_pos--;
                cmd_buffer[cmd_pos] = '\0';
                vga.putchar('\b');
            }
        } else if (c >= 32 && c < 127 && cmd_pos < MAX_CMD_LENGTH - 1) {
            cmd_buffer[cmd_pos++] = c;
            vga.putchar(c);
        }
    }
}

void Shell::parseCommand(char* args[], int* argc) {
    *argc = 0;
    char* token = cmd_buffer;
    bool in_token = false;
    
    while (*token && *argc < MAX_ARGS) {
        if (*token == ' ' || *token == '\t') {
            if (in_token) {
                *token = '\0';
                in_token = false;
            }
        } else {
            if (!in_token) {
                args[(*argc)++] = token;
                in_token = true;
            }
        }
        token++;
    }
}

// Convert string to uppercase for case-insensitive commands
static void to_upper(char* str) {
    while (*str) {
        if (*str >= 'a' && *str <= 'z') {
            *str -= 32;
        }
        str++;
    }
}

void Shell::executeCommand() {
    if (cmd_buffer[0] == '\0') return;
    
    char* args[MAX_ARGS];
    int argc;
    parseCommand(args, &argc);
    
    if (argc == 0) return;
    
    // Convert command to uppercase for comparison
    char cmd_upper[64];
    strncpy(cmd_upper, args[0], 63);
    cmd_upper[63] = '\0';
    to_upper(cmd_upper);
    
    if (strcmp(cmd_upper, "HELP") == 0 || strcmp(cmd_upper, "?") == 0) {
        cmd_help();
    } else if (strcmp(cmd_upper, "CLS") == 0) {
        cmd_cls();
    } else if (strcmp(cmd_upper, "VER") == 0) {
        cmd_ver();
    } else if (strcmp(cmd_upper, "ECHO") == 0) {
        cmd_echo(args, argc);
    } else if (strcmp(cmd_upper, "DIR") == 0) {
        cmd_dir();
    } else if (strcmp(cmd_upper, "TYPE") == 0) {
        cmd_type(args, argc);
    } else if (strcmp(cmd_upper, "CD") == 0) {
        cmd_cd(args, argc);
    } else if (strcmp(cmd_upper, "DATE") == 0) {
        cmd_date();
    } else if (strcmp(cmd_upper, "TIME") == 0) {
        cmd_time();
    } else if (strcmp(cmd_upper, "MEM") == 0) {
        cmd_mem();
    } else if (strcmp(cmd_upper, "COPY") == 0) {
        cmd_copy(args, argc);
    } else if (strcmp(cmd_upper, "DEL") == 0 || strcmp(cmd_upper, "ERASE") == 0) {
        cmd_del(args, argc);
    } else if (strcmp(cmd_upper, "MD") == 0 || strcmp(cmd_upper, "MKDIR") == 0) {
        cmd_md(args, argc);
    } else if (strcmp(cmd_upper, "RD") == 0 || strcmp(cmd_upper, "RMDIR") == 0) {
        cmd_rd(args, argc);
    } else if (strcmp(cmd_upper, "SET") == 0) {
        cmd_set(args, argc);
    } else if (strcmp(cmd_upper, "EXIT") == 0) {
        cmd_exit();
    } else if (strcmp(cmd_upper, "COLOR") == 0) {
        cmd_color(args, argc);
    } else if (strcmp(cmd_upper, "SYSINFO") == 0) {
        cmd_sysinfo();
    } else {
        vga.setColor(VGA_LIGHT_RED, VGA_BLACK);
        vga.puts("Bad command or file name: ");
        vga.puts(args[0]);
        vga.puts("\n");
        vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
    }
}

void Shell::cmd_help() {
    vga.setColor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga.puts("\n OS/2 Clone Command Reference\n");
    vga.puts(" ============================\n\n");
    vga.setColor(VGA_WHITE, VGA_BLACK);
    vga.puts(" CLS      - Clear the screen\n");
    vga.puts(" VER      - Display version information\n");
    vga.puts(" HELP     - Display this help message\n");
    vga.puts(" ECHO     - Display a message\n");
    vga.puts(" DIR      - List directory contents\n");
    vga.puts(" TYPE     - Display contents of a file\n");
    vga.puts(" CD       - Change directory\n");
    vga.puts(" DATE     - Display current date\n");
    vga.puts(" TIME     - Display current time\n");
    vga.puts(" MEM      - Display memory information\n");
    vga.puts(" COPY     - Copy files (simulated)\n");
    vga.puts(" DEL      - Delete files (simulated)\n");
    vga.puts(" MD       - Make directory (simulated)\n");
    vga.puts(" RD       - Remove directory (simulated)\n");
    vga.puts(" SET      - Display environment variables\n");
    vga.puts(" COLOR    - Change text color\n");
    vga.puts(" SYSINFO  - Display system information\n");
    vga.puts(" EXIT     - Halt the system\n\n");
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
}

void Shell::cmd_cls() {
    vga.clear();
}

void Shell::cmd_ver() {
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
    vga.puts(" (c) 2024 Educational Purposes Only\n");
    vga.puts(" Based on IBM OS/2 Command Interface\n\n");
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
}

void Shell::cmd_echo(char* args[], int argc) {
    for (int i = 1; i < argc; i++) {
        vga.puts(args[i]);
        if (i < argc - 1) vga.putchar(' ');
    }
    vga.putchar('\n');
}

void Shell::cmd_dir() {
    vga.setColor(VGA_WHITE, VGA_BLACK);
    vga.puts("\n Volume in drive C has no label\n");
    vga.puts(" Directory of ");
    vga.puts(current_dir);
    vga.puts("\n\n");
    
    int file_count = 0;
    int dir_count = 0;
    
    for (int i = 0; root_files[i].name != NULL; i++) {
        vga.puts(" ");
        if (root_files[i].is_dir) {
            vga.setColor(VGA_LIGHT_BLUE, VGA_BLACK);
            vga.puts("<DIR>     ");
            dir_count++;
        } else {
            vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
            vga.puts("          ");
            file_count++;
        }
        vga.setColor(VGA_WHITE, VGA_BLACK);
        vga.puts(root_files[i].name);
        vga.puts("\n");
    }
    
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
    char buf[16];
    itoa(file_count, buf, 10);
    vga.puts("\n        ");
    vga.puts(buf);
    vga.puts(" file(s)\n");
    itoa(dir_count, buf, 10);
    vga.puts("        ");
    vga.puts(buf);
    vga.puts(" dir(s)\n\n");
}

void Shell::cmd_type(char* args[], int argc) {
    if (argc < 2) {
        vga.puts("Usage: TYPE <filename>\n");
        return;
    }
    
    char filename[64];
    strncpy(filename, args[1], 63);
    filename[63] = '\0';
    to_upper(filename);
    
    for (int i = 0; root_files[i].name != NULL; i++) {
        char name_upper[64];
        strncpy(name_upper, root_files[i].name, 63);
        name_upper[63] = '\0';
        to_upper(name_upper);
        
        if (strcmp(filename, name_upper) == 0) {
            if (root_files[i].is_dir) {
                vga.puts("Access denied - ");
                vga.puts(args[1]);
                vga.puts(" is a directory\n");
                return;
            }
            if (root_files[i].content) {
                vga.puts(root_files[i].content);
            }
            return;
        }
    }
    
    vga.setColor(VGA_LIGHT_RED, VGA_BLACK);
    vga.puts("File not found: ");
    vga.puts(args[1]);
    vga.puts("\n");
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
}

void Shell::cmd_cd(char* args[], int argc) {
    if (argc < 2) {
        vga.puts(current_dir);
        vga.puts("\n");
        return;
    }
    
    if (strcmp(args[1], "..") == 0) {
        strcpy(current_dir, "C:\\");
    } else if (strcmp(args[1], "\\") == 0 || strcmp(args[1], "/") == 0) {
        strcpy(current_dir, "C:\\");
    } else {
        // Simple directory change simulation
        char dirname[64];
        strncpy(dirname, args[1], 63);
        dirname[63] = '\0';
        to_upper(dirname);
        
        for (int i = 0; root_files[i].name != NULL; i++) {
            char name_upper[64];
            strncpy(name_upper, root_files[i].name, 63);
            name_upper[63] = '\0';
            to_upper(name_upper);
            
            if (root_files[i].is_dir && strcmp(dirname, name_upper) == 0) {
                strcpy(current_dir, "C:\\");
                strcat(current_dir, root_files[i].name);
                strcat(current_dir, "\\");
                return;
            }
        }
        vga.setColor(VGA_LIGHT_RED, VGA_BLACK);
        vga.puts("Directory not found: ");
        vga.puts(args[1]);
        vga.puts("\n");
        vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
    }
}

void Shell::cmd_date() {
    vga.puts("Current date: Mon 01-01-2024\n");
    vga.puts("(Date cannot be changed in this environment)\n");
}

void Shell::cmd_time() {
    vga.puts("Current time: 12:00:00.00\n");
    vga.puts("(Time cannot be changed in this environment)\n");
}

void Shell::cmd_mem() {
    vga.setColor(VGA_WHITE, VGA_BLACK);
    vga.puts("\n Memory Information\n");
    vga.puts(" ==================\n\n");
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
    vga.puts(" Extended Memory:     16384 KB\n");
    vga.puts(" Conventional Memory:   640 KB\n");
    vga.puts(" Total System Memory: 17024 KB\n");
    vga.puts("\n Kernel Memory Usage:   128 KB\n");
    vga.puts(" Available Memory:    16896 KB\n\n");
}

void Shell::cmd_copy(char* args[], int argc) {
    if (argc < 3) {
        vga.puts("Usage: COPY <source> <destination>\n");
        return;
    }
    vga.puts("        1 file(s) copied. (simulated)\n");
}

void Shell::cmd_del(char* args[], int argc) {
    if (argc < 2) {
        vga.puts("Usage: DEL <filename>\n");
        return;
    }
    vga.puts("File deleted. (simulated)\n");
}

void Shell::cmd_md(char* args[], int argc) {
    if (argc < 2) {
        vga.puts("Usage: MD <directory>\n");
        return;
    }
    vga.puts("Directory created. (simulated)\n");
}

void Shell::cmd_rd(char* args[], int argc) {
    if (argc < 2) {
        vga.puts("Usage: RD <directory>\n");
        return;
    }
    vga.puts("Directory removed. (simulated)\n");
}

void Shell::cmd_set(char* args[], int argc) {
    vga.puts("\n Environment Variables:\n");
    vga.puts(" ======================\n\n");
    vga.puts(" PATH=C:\\OS2;C:\\OS2\\SYSTEM\n");
    vga.puts(" DPATH=C:\\OS2;C:\\OS2\\SYSTEM\n");
    vga.puts(" LIBPATH=.;C:\\OS2\\DLL\n");
    vga.puts(" PROMPT=$P$G\n");
    vga.puts(" COMSPEC=C:\\OS2\\CMD.EXE\n");
    vga.puts(" OS=OS2_CLONE\n");
    vga.puts(" PROCESSOR=386\n\n");
}

void Shell::cmd_exit() {
    vga.setColor(VGA_YELLOW, VGA_BLACK);
    vga.puts("\n System halted. You may now turn off your computer.\n");
    vga.puts(" (Or close the QEMU window)\n");
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
    
    // Halt the CPU
    asm volatile ("cli; hlt");
}

void Shell::cmd_color(char* args[], int argc) {
    if (argc < 2) {
        vga.puts("Usage: COLOR <0-F>\n");
        vga.puts("  0=Black, 1=Blue, 2=Green, 3=Cyan\n");
        vga.puts("  4=Red, 5=Magenta, 6=Brown, 7=LightGrey\n");
        vga.puts("  8=DarkGrey, 9=LightBlue, A=LightGreen\n");
        vga.puts("  B=LightCyan, C=LightRed, D=LightMagenta\n");
        vga.puts("  E=Yellow, F=White\n");
        return;
    }
    
    char c = args[1][0];
    int color = 7;
    
    if (c >= '0' && c <= '9') {
        color = c - '0';
    } else if (c >= 'A' && c <= 'F') {
        color = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        color = c - 'a' + 10;
    }
    
    vga.setColor((VGAColor)color, VGA_BLACK);
}

void Shell::cmd_sysinfo() {
    vga.setColor(VGA_LIGHT_GREEN, VGA_BLACK);
    vga.puts("\n System Information\n");
    vga.puts(" ==================\n\n");
    vga.setColor(VGA_WHITE, VGA_BLACK);
    vga.puts(" OS Name:           OS/2 Clone\n");
    vga.puts(" Version:           1.0.0\n");
    vga.puts(" Architecture:      i386 (32-bit Protected Mode)\n");
    vga.puts(" Processor:         Intel 386 compatible\n");
    vga.puts(" Video Mode:        VGA 80x25 Text Mode\n");
    vga.puts(" Kernel Type:       Monolithic\n");
    vga.puts(" Boot Method:       BIOS -> Bootloader -> Kernel\n");
    vga.puts(" Shell:             CMD.EXE Clone\n\n");
    vga.setColor(VGA_LIGHT_GREY, VGA_BLACK);
}

void Shell::run() {
    while (true) {
        printPrompt();
        readLine();
        executeCommand();
    }
}
