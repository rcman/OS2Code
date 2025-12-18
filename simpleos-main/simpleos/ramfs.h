// File: ramfs.h
// Simple RAM-based filesystem

#ifndef RAMFS_H
#define RAMFS_H

#include "types.h"

#define RAMFS_MAX_FILES     32
#define RAMFS_MAX_FILENAME  32
#define RAMFS_MAX_FILESIZE  4096

// File entry
typedef struct {
    char name[RAMFS_MAX_FILENAME];
    uint8_t data[RAMFS_MAX_FILESIZE];
    uint32_t size;
    bool used;
} ramfs_file_t;

// Initialize the filesystem
void ramfs_init(void);

// Create a new file (returns 0 on success, -1 on error)
int ramfs_create(const char* name);

// Write data to a file (returns bytes written, -1 on error)
int ramfs_write(const char* name, const void* data, uint32_t size);

// Append data to a file (returns bytes written, -1 on error)
int ramfs_append(const char* name, const void* data, uint32_t size);

// Read data from a file (returns bytes read, -1 on error)
int ramfs_read(const char* name, void* buffer, uint32_t max_size);

// Delete a file (returns 0 on success, -1 on error)
int ramfs_delete(const char* name);

// Check if file exists (returns 1 if exists, 0 if not)
int ramfs_exists(const char* name);

// Get file size (returns size, -1 if not found)
int ramfs_size(const char* name);

// List all files (calls callback for each file)
void ramfs_list(void (*callback)(const char* name, uint32_t size));

// Get number of files
int ramfs_count(void);

// Get free space
uint32_t ramfs_free_space(void);

#endif // RAMFS_H
