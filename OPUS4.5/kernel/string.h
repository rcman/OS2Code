#ifndef STRING_H
#define STRING_H

#include "types.h"

extern "C" {
    size_t strlen(const char* str);
    int strcmp(const char* s1, const char* s2);
    int strncmp(const char* s1, const char* s2, size_t n);
    char* strcpy(char* dest, const char* src);
    char* strncpy(char* dest, const char* src, size_t n);
    char* strcat(char* dest, const char* src);
    void* memset(void* ptr, int value, size_t num);
    void* memcpy(void* dest, const void* src, size_t num);
    int memcmp(const void* s1, const void* s2, size_t n);
    char* strchr(const char* str, int c);
    int atoi(const char* str);
    void itoa(int value, char* str, int base);
}

#endif
