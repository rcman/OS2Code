// File: dos_file.c
// DOS File System APIs implementation

#include "dos_file.h"
#include "vfs.h"
#include "process.h"

// External functions
extern void printf(const char* format, ...);
extern process_t* process_current(void);

// String utilities
static int str_len(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static void str_cpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Maximum directory search handles
#define MAX_FIND_HANDLES 32

// Directory search handle table (type defined in vfs.h)
static find_handle_t find_handles[MAX_FIND_HANDLES];

// Initialize DOS file system
void dos_file_init(void) {
    // Clear find handle table
    for (int i = 0; i < MAX_FIND_HANDLES; i++) {
        find_handles[i].in_use = 0;
        find_handles[i].dir_node = NULL;
    }
    printf("[DOS File] DOS File API initialized\n");
}

// Get file descriptor from handle
static file_descriptor_t* get_fd(uint32_t handle) {
    process_t* proc = process_current();
    if (!proc) return NULL;

    if (handle >= MAX_OPEN_FILES) return NULL;

    file_descriptor_t* fd = &proc->file_descriptors[handle];
    if (!fd->in_use) return NULL;

    return fd;
}

// Find free file descriptor
static int find_free_fd(void) {
    process_t* proc = process_current();
    if (!proc) return -1;

    // Start from 3 (0, 1, 2 are stdin, stdout, stderr)
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (!proc->file_descriptors[i].in_use) {
            return i;
        }
    }
    return -1;
}

// DosOpen - Open or create a file
uint32_t DosOpen(const char* filename, uint32_t* file_handle, uint32_t* action,
                 uint32_t initial_size, uint32_t attributes, uint32_t open_flags,
                 uint32_t open_mode, void* reserved) {
    (void)initial_size;
    (void)attributes;
    (void)reserved;

    if (!filename || !file_handle) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    // Find free file descriptor
    int fd_index = find_free_fd();
    if (fd_index < 0) {
        return DOS_ERROR_TOO_MANY_OPEN_FILES;
    }

    // Try to open existing file
    vfs_node_t* node = vfs_open(filename, open_mode);

    if (!node) {
        // File doesn't exist
        if (open_flags & O_CREAT) {
            // Create new file
            if (vfs_create_file(filename, open_mode) != 0) {
                return DOS_ERROR_ACCESS_DENIED;
            }

            // Try to open again
            node = vfs_open(filename, open_mode);
            if (!node) {
                return DOS_ERROR_FILE_NOT_FOUND;
            }

            if (action) {
                *action = 1;  // File was created
            }
        } else {
            return DOS_ERROR_FILE_NOT_FOUND;
        }
    } else {
        // File exists
        if ((open_flags & O_CREAT) && (open_flags & O_EXCL)) {
            return DOS_ERROR_FILE_EXISTS;
        }

        if (action) {
            *action = 2;  // File was opened
        }

        // Truncate if requested
        if (open_mode & O_TRUNC) {
            // Clear file contents by writing 0 bytes at offset 0
            // (This is a simplified implementation)
        }
    }

    // Set up file descriptor
    process_t* proc = process_current();
    file_descriptor_t* fd = &proc->file_descriptors[fd_index];
    fd->node = node;
    fd->position = 0;
    fd->flags = open_mode;
    fd->in_use = 1;

    *file_handle = fd_index;
    return DOS_ERROR_SUCCESS;
}

// DosClose - Close a file
uint32_t DosClose(uint32_t file_handle) {
    file_descriptor_t* fd = get_fd(file_handle);
    if (!fd) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    // Close VFS node
    if (fd->node) {
        vfs_close((vfs_node_t*)fd->node);
    }

    // Clear file descriptor
    fd->node = NULL;
    fd->position = 0;
    fd->flags = 0;
    fd->in_use = 0;

    return DOS_ERROR_SUCCESS;
}

// DosRead - Read from a file
uint32_t DosRead(uint32_t file_handle, void* buffer, uint32_t buffer_length,
                 uint32_t* bytes_read) {
    if (!buffer || !bytes_read) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    file_descriptor_t* fd = get_fd(file_handle);
    if (!fd) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    vfs_node_t* node = (vfs_node_t*)fd->node;
    if (!node) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    // Read from VFS
    uint32_t read = vfs_read(node, fd->position, buffer_length, (uint8_t*)buffer);

    // Update position
    fd->position += read;
    *bytes_read = read;

    return DOS_ERROR_SUCCESS;
}

// DosWrite - Write to a file
uint32_t DosWrite(uint32_t file_handle, const void* buffer, uint32_t buffer_length,
                  uint32_t* bytes_written) {
    if (!buffer || !bytes_written) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    file_descriptor_t* fd = get_fd(file_handle);
    if (!fd) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    vfs_node_t* node = (vfs_node_t*)fd->node;
    if (!node) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    // Check if opened for writing
    if ((fd->flags & O_WRONLY) == 0 && (fd->flags & O_RDWR) == 0) {
        return DOS_ERROR_ACCESS_DENIED;
    }

    // Write to VFS
    uint32_t written = vfs_write(node, fd->position, buffer_length, (const uint8_t*)buffer);

    // Update position
    fd->position += written;
    *bytes_written = written;

    return DOS_ERROR_SUCCESS;
}

// DosSetFilePtr - Set file pointer position
uint32_t DosSetFilePtr(uint32_t file_handle, int32_t distance,
                       uint32_t move_method, uint32_t* new_position) {
    file_descriptor_t* fd = get_fd(file_handle);
    if (!fd) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    vfs_node_t* node = (vfs_node_t*)fd->node;
    if (!node) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    int32_t new_pos = 0;

    switch (move_method) {
        case SEEK_SET:  // From beginning
            new_pos = distance;
            break;

        case SEEK_CUR:  // From current position
            new_pos = fd->position + distance;
            break;

        case SEEK_END:  // From end of file
            new_pos = node->size + distance;
            break;

        default:
            return DOS_ERROR_INVALID_PARAMETER;
    }

    // Check bounds
    if (new_pos < 0) {
        new_pos = 0;
    }

    fd->position = new_pos;

    if (new_position) {
        *new_position = new_pos;
    }

    return DOS_ERROR_SUCCESS;
}

// DosDelete - Delete a file
uint32_t DosDelete(const char* filename) {
    if (!filename) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    if (vfs_delete_file(filename) != 0) {
        return DOS_ERROR_FILE_NOT_FOUND;
    }

    return DOS_ERROR_SUCCESS;
}

// DosQueryFileInfo - Query file information
uint32_t DosQueryFileInfo(uint32_t file_handle, uint32_t info_level,
                          void* info_buffer, uint32_t buffer_size) {
    (void)info_level;

    if (!info_buffer || buffer_size < sizeof(dos_file_info_t)) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    file_descriptor_t* fd = get_fd(file_handle);
    if (!fd) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    vfs_node_t* node = (vfs_node_t*)fd->node;
    if (!node) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    dos_file_info_t* info = (dos_file_info_t*)info_buffer;
    info->creation_time = node->creation_time;
    info->access_time = node->creation_time;
    info->modification_time = node->modification_time;
    info->size = node->size;
    info->attributes = node->attributes;

    return DOS_ERROR_SUCCESS;
}

// DosSetFileInfo - Set file information
uint32_t DosSetFileInfo(uint32_t file_handle, uint32_t info_level,
                        void* info_buffer, uint32_t buffer_size) {
    (void)info_level;
    (void)buffer_size;

    if (!info_buffer) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    file_descriptor_t* fd = get_fd(file_handle);
    if (!fd) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    vfs_node_t* node = (vfs_node_t*)fd->node;
    if (!node) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    dos_file_info_t* info = (dos_file_info_t*)info_buffer;
    node->attributes = info->attributes;
    node->modification_time = info->modification_time;

    return DOS_ERROR_SUCCESS;
}

// Find free find handle
static int find_free_find_handle(void) {
    for (int i = 0; i < MAX_FIND_HANDLES; i++) {
        if (!find_handles[i].in_use) {
            return i;
        }
    }
    return -1;
}

// Simple pattern matching (only supports * wildcard)
static int pattern_match(const char* pattern, const char* str) {
    // Simple implementation - just check for "*" (match all)
    if (pattern[0] == '*') {
        return 1;
    }

    // Exact match
    while (*pattern && *str) {
        if (*pattern != *str) {
            return 0;
        }
        pattern++;
        str++;
    }

    return (*pattern == '\0' && *str == '\0');
}

// DosFindFirst - Find first file matching pattern
uint32_t DosFindFirst(const char* pattern, uint32_t* dir_handle,
                      uint32_t attributes, dos_dir_entry_t* entry,
                      uint32_t entry_size, uint32_t* search_count,
                      void* reserved) {
    (void)attributes;
    (void)entry_size;
    (void)reserved;

    if (!pattern || !dir_handle || !entry) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    // Find free handle
    int handle = find_free_find_handle();
    if (handle < 0) {
        return DOS_ERROR_TOO_MANY_OPEN_FILES;
    }

    // Get root directory (simplified - always search root)
    vfs_node_t* root = vfs_get_root();
    if (!root) {
        return DOS_ERROR_PATH_NOT_FOUND;
    }

    // Set up find handle
    find_handle_t* fh = &find_handles[handle];
    fh->dir_node = root;
    fh->index = 0;
    fh->in_use = 1;
    str_cpy(fh->pattern, pattern);

    *dir_handle = handle;

    // Find first matching entry
    return DosFindNext(handle, entry, entry_size, search_count);
}

// DosFindNext - Find next file in directory search
uint32_t DosFindNext(uint32_t dir_handle, dos_dir_entry_t* entry,
                     uint32_t entry_size, uint32_t* search_count) {
    (void)entry_size;

    if (!entry) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    if (dir_handle >= MAX_FIND_HANDLES || !find_handles[dir_handle].in_use) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    find_handle_t* fh = &find_handles[dir_handle];

    // Search for next matching entry
    while (1) {
        vfs_node_t* node = vfs_readdir(fh->dir_node, fh->index);

        if (!node) {
            // No more entries
            if (search_count) {
                *search_count = 0;
            }
            return DOS_ERROR_FILE_NOT_FOUND;
        }

        fh->index++;

        // Check if name matches pattern
        if (pattern_match(fh->pattern, node->name)) {
            // Fill entry
            str_cpy(entry->name, node->name);
            entry->attributes = node->attributes;
            entry->size = node->size;
            entry->creation_time = node->creation_time;
            entry->modification_time = node->modification_time;

            if (search_count) {
                *search_count = 1;
            }
            return DOS_ERROR_SUCCESS;
        }
    }
}

// DosFindClose - Close directory search
uint32_t DosFindClose(uint32_t dir_handle) {
    if (dir_handle >= MAX_FIND_HANDLES || !find_handles[dir_handle].in_use) {
        return DOS_ERROR_INVALID_HANDLE;
    }

    find_handles[dir_handle].in_use = 0;
    find_handles[dir_handle].dir_node = NULL;

    return DOS_ERROR_SUCCESS;
}

// DosMkDir - Create a directory
uint32_t DosMkDir(const char* dirname, void* reserved) {
    (void)reserved;

    if (!dirname) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    if (vfs_create_directory(dirname) != 0) {
        return DOS_ERROR_ACCESS_DENIED;
    }

    return DOS_ERROR_SUCCESS;
}

// DosRmDir - Remove a directory
uint32_t DosRmDir(const char* dirname) {
    if (!dirname) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    if (vfs_delete_directory(dirname) != 0) {
        return DOS_ERROR_DIR_NOT_EMPTY;
    }

    return DOS_ERROR_SUCCESS;
}

// DosSetCurrentDir - Change current directory
uint32_t DosSetCurrentDir(const char* dirname) {
    if (!dirname) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    // Check if directory exists
    vfs_node_t* node = vfs_resolve_path(dirname);
    if (!node || node->type != FILE_TYPE_DIRECTORY) {
        return DOS_ERROR_PATH_NOT_FOUND;
    }

    // TODO: Store current directory per process
    return DOS_ERROR_SUCCESS;
}

// DosQueryCurrentDir - Query current directory
uint32_t DosQueryCurrentDir(uint32_t drive, char* buffer, uint32_t* buffer_length) {
    (void)drive;

    if (!buffer || !buffer_length) {
        return DOS_ERROR_INVALID_PARAMETER;
    }

    // TODO: Return actual current directory
    // For now, just return root "/"
    if (*buffer_length < 2) {
        *buffer_length = 2;
        return DOS_ERROR_INVALID_PARAMETER;
    }

    buffer[0] = '/';
    buffer[1] = '\0';
    *buffer_length = 1;

    return DOS_ERROR_SUCCESS;
}
