// File: vfs.h
// Virtual File System - Abstract filesystem interface

#ifndef VFS_H
#define VFS_H

#include "types.h"

// File access modes (for DosOpen)
#define O_RDONLY    0x0001    // Read only
#define O_WRONLY    0x0002    // Write only
#define O_RDWR      0x0003    // Read/write
#define O_CREAT     0x0010    // Create if doesn't exist
#define O_TRUNC     0x0020    // Truncate to zero length
#define O_APPEND    0x0040    // Append mode
#define O_EXCL      0x0080    // Fail if file exists (with O_CREAT)

// File attributes
#define FILE_ATTR_NORMAL    0x00
#define FILE_ATTR_READONLY  0x01
#define FILE_ATTR_HIDDEN    0x02
#define FILE_ATTR_SYSTEM    0x04
#define FILE_ATTR_DIRECTORY 0x10
#define FILE_ATTR_ARCHIVE   0x20

// Seek modes (for DosSetFilePtr)
#define SEEK_SET    0    // From beginning of file
#define SEEK_CUR    1    // From current position
#define SEEK_END    2    // From end of file

// Maximum open files per process
#define MAX_OPEN_FILES  32

// Maximum path length
#define MAX_PATH_LENGTH 256

// File types
#define FILE_TYPE_REGULAR    1
#define FILE_TYPE_DIRECTORY  2

// Forward declarations
struct vfs_node;
struct vfs_filesystem;

// VFS node (represents a file or directory)
typedef struct vfs_node {
    char name[64];                      // File/directory name
    uint32_t inode;                     // Inode number
    uint32_t type;                      // FILE_TYPE_REGULAR or FILE_TYPE_DIRECTORY
    uint32_t size;                      // File size in bytes
    uint32_t attributes;                // File attributes
    uint32_t creation_time;             // Creation timestamp
    uint32_t modification_time;         // Last modification timestamp

    struct vfs_filesystem* fs;          // Filesystem this node belongs to
    void* fs_data;                      // Filesystem-specific data

    // Directory operations
    struct vfs_node* (*readdir)(struct vfs_node* node, uint32_t index);
    struct vfs_node* (*finddir)(struct vfs_node* node, const char* name);

    // File operations
    uint32_t (*read)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct vfs_node* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
    void (*open)(struct vfs_node* node, uint32_t flags);
    void (*close)(struct vfs_node* node);
} vfs_node_t;

// Filesystem operations interface
typedef struct vfs_filesystem {
    const char* name;                   // Filesystem name (e.g., "ramfs", "fat16")

    // Mount/unmount
    int (*mount)(const char* device, const char* mountpoint);
    int (*unmount)(const char* mountpoint);

    // Node operations
    vfs_node_t* (*get_root)(void);
    vfs_node_t* (*create_file)(const char* path, uint32_t flags);
    int (*delete_file)(const char* path);
    vfs_node_t* (*create_dir)(const char* path);
    int (*delete_dir)(const char* path);
} vfs_filesystem_t;

// File descriptor (per-process open file)
typedef struct {
    vfs_node_t* node;                   // VFS node for this file
    uint32_t position;                  // Current file position
    uint32_t flags;                     // Open flags
    uint32_t in_use;                    // 1 if allocated, 0 if free
} file_descriptor_t;

// File information (for DosQueryFileInfo)
typedef struct {
    uint32_t creation_time;             // File creation time
    uint32_t access_time;               // Last access time
    uint32_t modification_time;         // Last modification time
    uint32_t size;                      // File size
    uint32_t attributes;                // File attributes
} file_info_t;

// Directory entry (for DosFindFirst/DosFindNext)
typedef struct {
    char name[256];                     // Filename
    uint32_t attributes;                // File attributes
    uint32_t size;                      // File size
    uint32_t creation_time;             // Creation time
    uint32_t modification_time;         // Modification time
} dir_entry_t;

// Find handle (for directory enumeration)
typedef struct {
    vfs_node_t* dir_node;               // Directory being enumerated
    uint32_t index;                     // Current index in directory
    uint32_t in_use;                    // 1 if allocated, 0 if free
    char pattern[256];                  // Search pattern (e.g., "*.txt")
} find_handle_t;

// Initialize VFS
void vfs_init(void);

// Register a filesystem
int vfs_register_filesystem(vfs_filesystem_t* fs);

// Mount/unmount
int vfs_mount(const char* device, const char* mountpoint, const char* fstype);
int vfs_unmount(const char* mountpoint);

// File operations
vfs_node_t* vfs_open(const char* path, uint32_t flags);
void vfs_close(vfs_node_t* node);
uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);

// Directory operations
vfs_node_t* vfs_readdir(vfs_node_t* node, uint32_t index);
vfs_node_t* vfs_finddir(vfs_node_t* node, const char* name);

// Path operations
vfs_node_t* vfs_resolve_path(const char* path);
int vfs_create_file(const char* path, uint32_t flags);
int vfs_delete_file(const char* path);
int vfs_create_directory(const char* path);
int vfs_delete_directory(const char* path);

// Utility functions
void vfs_parse_path(const char* path, char* parent, char* name);
int vfs_path_exists(const char* path);

// Get root filesystem node
vfs_node_t* vfs_get_root(void);

#endif // VFS_H
