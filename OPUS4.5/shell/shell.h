#ifndef SHELL_H
#define SHELL_H

#include "../kernel/types.h"

#define MAX_CMD_LENGTH 256
#define MAX_ARGS 16

class Shell {
public:
    void init();
    void run();

private:
    char cmd_buffer[MAX_CMD_LENGTH];
    int cmd_pos;
    char current_dir[64];
    
    void printPrompt();
    void readLine();
    void executeCommand();
    void parseCommand(char* args[], int* argc);
    
    // Built-in commands
    void cmd_help();
    void cmd_cls();
    void cmd_ver();
    void cmd_echo(char* args[], int argc);
    void cmd_dir();
    void cmd_type(char* args[], int argc);
    void cmd_cd(char* args[], int argc);
    void cmd_date();
    void cmd_time();
    void cmd_mem();
    void cmd_copy(char* args[], int argc);
    void cmd_del(char* args[], int argc);
    void cmd_md(char* args[], int argc);
    void cmd_rd(char* args[], int argc);
    void cmd_set(char* args[], int argc);
    void cmd_exit();
    void cmd_color(char* args[], int argc);
    void cmd_sysinfo();
};

extern Shell shell;

#endif
