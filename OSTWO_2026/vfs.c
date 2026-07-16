// File: vfs.c
// Virtual File System implementation

#include "vfs.h"

// External functions
extern void printf(const char* format, ...);

// Maximum registered filesystems
#define MAX_FILESYSTEMS 8

// Registered filesystems
static vfs_filesystem_t* filesystems[MAX_FILESYSTEMS];
static int filesystem_count = 0;

// Root filesystem
static vfs_filesystem_t* root_fs = NULL;

// String utilities
static int str_len(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static int str_cmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

static void str_cpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static int str_ncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (a[i] == '\0') break;
    }
    return 0;
}

// Initialize VFS
void vfs_init(void) {
    for (int i = 0; i < MAX_FILESYSTEMS; i++) {
        filesystems[i] = NULL;
    }
    filesystem_count = 0;
    root_fs = NULL;
    printf("[VFS] Virtual File System initialized\n");
}

// Register a filesystem
int vfs_register_filesystem(vfs_filesystem_t* fs) {
    if (!fs) {
        printf("[VFS] ERROR: NULL filesystem\n");
        return -1;
    }

    if (filesystem_count >= MAX_FILESYSTEMS) {
        printf("[VFS] ERROR: Maximum filesystems registered\n");
        return -1;
    }

    filesystems[filesystem_count++] = fs;
    printf("[VFS] Registered filesystem: %s\n", fs->name);

    // If this is the first filesystem, make it the root
    if (!root_fs) {
        root_fs = fs;
        printf("[VFS] Set root filesystem to: %s\n", fs->name);
    }

    return 0;
}

// Get root filesystem node
vfs_node_t* vfs_get_root(void) {
    if (!root_fs || !root_fs->get_root) {
        return NULL;
    }
    return root_fs->get_root();
}

// Parse a path into parent and name components
// E.g., "/dir1/dir2/file.txt" -> parent="/dir1/dir2", name="file.txt"
void vfs_parse_path(const char* path, char* parent, char* name) {
    // Find last '/' in path
    int last_slash = -1;
    int len = str_len(path);

    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash == -1) {
        // No slash - just a filename in root
        if (parent) parent[0] = '\0';
        if (name) str_cpy(name, path);
    } else {
        // Copy parent path
        if (parent) {
            for (int i = 0; i < last_slash; i++) {
                parent[i] = path[i];
            }
            parent[last_slash] = '\0';
        }

        // Copy name
        if (name) {
            str_cpy(name, path + last_slash + 1);
        }
    }
}

// Resolve a path to a VFS node
vfs_node_t* vfs_resolve_path(const char* path) {
    if (!path || path[0] == '\0') {
        return NULL;
    }

    // Start from root
    vfs_node_t* current = vfs_get_root();
    if (!current) {
        printf("[VFS] ERROR: No root filesystem\n");
        return NULL;
    }

    // Handle root path "/"
    if (path[0] == '/' && path[1] == '\0') {
        return current;
    }

    // Skip leading '/'
    const char* p = path;
    if (*p == '/') p++;

    // Traverse path components
    char component[256];
    int comp_idx = 0;

    while (*p) {
        if (*p == '/') {
            // End of component
            if (comp_idx > 0) {
                component[comp_idx] = '\0';

                // Find this component in current directory
                if (current->type != FILE_TYPE_DIRECTORY) {
                    printf("[VFS] ERROR: Not a directory in path\n");
                    return NULL;
                }

                current = current->finddir(current, component);
                if (!current) {
                    return NULL;  // Component not found
                }

                comp_idx = 0;
            }
            p++;
        } else {
            component[comp_idx++] = *p++;
        }
    }

    // Handle last component
    if (comp_idx > 0) {
        component[comp_idx] = '\0';

        if (current->type != FILE_TYPE_DIRECTORY) {
            return NULL;
        }

        current = current->finddir(current, component);
    }

    return current;
}

// Check if a path exists
int vfs_path_exists(const char* path) {
    return (vfs_resolve_path(path) != NULL) ? 1 : 0;
}

// Open a file
vfs_node_t* vfs_open(const char* path, uint32_t flags) {
    vfs_node_t* node = vfs_resolve_path(path);

    if (!node) {
        // File doesn't exist
        if (flags & O_CREAT) {
            // Create new file
            return NULL;  // TODO: Implement file creation
        }
        return NULL;
    }

    // Call filesystem-specific open
    if (node->open) {
        node->open(node, flags);
    }

    return node;
}

// Close a file
void vfs_close(vfs_node_t* node) {
    if (!node) return;

    if (node->close) {
        node->close(node);
    }
}

// Read from a file
uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) {
        return 0;
    }

    if (node->type != FILE_TYPE_REGULAR) {
        return 0;  // Can't read from directory
    }

    if (!node->read) {
        return 0;  // No read operation
    }

    return node->read(node, offset, size, buffer);
}

// Write to a file
uint32_t vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !buffer) {
        return 0;
    }

    if (node->type != FILE_TYPE_REGULAR) {
        return 0;  // Can't write to directory
    }

    if (!node->write) {
        return 0;  // No write operation
    }

    return node->write(node, offset, size, buffer);
}

// Read directory entry
vfs_node_t* vfs_readdir(vfs_node_t* node, uint32_t index) {
    if (!node) {
        return NULL;
    }

    if (node->type != FILE_TYPE_DIRECTORY) {
        return NULL;  // Not a directory
    }

    if (!node->readdir) {
        return NULL;  // No readdir operation
    }

    return node->readdir(node, index);
}

// Find entry in directory
vfs_node_t* vfs_finddir(vfs_node_t* node, const char* name) {
    if (!node || !name) {
        return NULL;
    }

    if (node->type != FILE_TYPE_DIRECTORY) {
        return NULL;  // Not a directory
    }

    if (!node->finddir) {
        return NULL;  // No finddir operation
    }

    return node->finddir(node, name);
}

// Create a file
int vfs_create_file(const char* path, uint32_t flags) {
    if (!root_fs || !root_fs->create_file) {
        return -1;
    }

    vfs_node_t* node = root_fs->create_file(path, flags);
    return (node != NULL) ? 0 : -1;
}

// Delete a file
int vfs_delete_file(const char* path) {
    if (!root_fs || !root_fs->delete_file) {
        return -1;
    }

    return root_fs->delete_file(path);
}

// Create a directory
int vfs_create_directory(const char* path) {
    if (!root_fs || !root_fs->create_dir) {
        return -1;
    }

    vfs_node_t* node = root_fs->create_dir(path);
    return (node != NULL) ? 0 : -1;
}

// Delete a directory
int vfs_delete_directory(const char* path) {
    if (!root_fs || !root_fs->delete_dir) {
        return -1;
    }

    return root_fs->delete_dir(path);
}
