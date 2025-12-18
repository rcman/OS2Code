// File: ramfs.c
// Simple RAM-based filesystem implementation

#include "ramfs.h"

// Filesystem storage
static ramfs_file_t files[RAMFS_MAX_FILES];

// String comparison
static int str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

// String copy
static void str_copy(char* dest, const char* src, uint32_t max) {
    uint32_t i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// String length
static uint32_t str_len(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

// Memory copy
static void mem_copy(void* dest, const void* src, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
}

// Memory set
static void mem_set(void* dest, uint8_t val, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    while (n--) {
        *d++ = val;
    }
}

// Find a file by name
static ramfs_file_t* find_file(const char* name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && str_equal(files[i].name, name)) {
            return &files[i];
        }
    }
    return 0;
}

// Find a free slot
static ramfs_file_t* find_free_slot(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            return &files[i];
        }
    }
    return 0;
}

// Initialize the filesystem
void ramfs_init(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        files[i].used = false;
        files[i].size = 0;
        files[i].name[0] = '\0';
    }
}

// Create a new file
int ramfs_create(const char* name) {
    // Check if already exists
    if (find_file(name)) {
        return -1;
    }
    
    // Find free slot
    ramfs_file_t* file = find_free_slot();
    if (!file) {
        return -1;
    }
    
    // Initialize file
    str_copy(file->name, name, RAMFS_MAX_FILENAME);
    file->size = 0;
    file->used = true;
    mem_set(file->data, 0, RAMFS_MAX_FILESIZE);
    
    return 0;
}

// Write data to a file (overwrites existing content)
int ramfs_write(const char* name, const void* data, uint32_t size) {
    ramfs_file_t* file = find_file(name);
    if (!file) {
        // Auto-create if doesn't exist
        if (ramfs_create(name) < 0) {
            return -1;
        }
        file = find_file(name);
    }
    
    // Clamp size
    if (size > RAMFS_MAX_FILESIZE) {
        size = RAMFS_MAX_FILESIZE;
    }
    
    // Copy data
    mem_copy(file->data, data, size);
    file->size = size;
    
    return size;
}

// Append data to a file
int ramfs_append(const char* name, const void* data, uint32_t size) {
    ramfs_file_t* file = find_file(name);
    if (!file) {
        return -1;
    }
    
    // Calculate how much we can append
    uint32_t available = RAMFS_MAX_FILESIZE - file->size;
    if (size > available) {
        size = available;
    }
    
    if (size == 0) {
        return 0;
    }
    
    // Append data
    mem_copy(file->data + file->size, data, size);
    file->size += size;
    
    return size;
}

// Read data from a file
int ramfs_read(const char* name, void* buffer, uint32_t max_size) {
    ramfs_file_t* file = find_file(name);
    if (!file) {
        return -1;
    }
    
    // Calculate how much to read
    uint32_t to_read = file->size;
    if (to_read > max_size) {
        to_read = max_size;
    }
    
    // Copy data
    mem_copy(buffer, file->data, to_read);
    
    return to_read;
}

// Delete a file
int ramfs_delete(const char* name) {
    ramfs_file_t* file = find_file(name);
    if (!file) {
        return -1;
    }
    
    file->used = false;
    file->size = 0;
    file->name[0] = '\0';
    
    return 0;
}

// Check if file exists
int ramfs_exists(const char* name) {
    return find_file(name) != 0;
}

// Get file size
int ramfs_size(const char* name) {
    ramfs_file_t* file = find_file(name);
    if (!file) {
        return -1;
    }
    return file->size;
}

// List all files
void ramfs_list(void (*callback)(const char* name, uint32_t size)) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used) {
            callback(files[i].name, files[i].size);
        }
    }
}

// Get number of files
int ramfs_count(void) {
    int count = 0;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used) {
            count++;
        }
    }
    return count;
}

// Get free space (total available for new files)
uint32_t ramfs_free_space(void) {
    uint32_t free_slots = 0;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            free_slots++;
        }
    }
    return free_slots * RAMFS_MAX_FILESIZE;
}
