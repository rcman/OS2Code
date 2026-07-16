// File: ramfs_vfs.c
// VFS-compatible RAM filesystem with directory support

#include "vfs.h"
#include "ramfs.h"

// External functions
extern void printf(const char* format, ...);
extern void* pmm_alloc_page(void);
extern void pmm_free_page(uint32_t addr);

// Maximum nodes (files + directories)
#define MAX_RAMFS_NODES 128

// RamFS node data
typedef struct {
    char name[64];
    uint8_t* data;                  // File data (NULL for directories)
    uint32_t data_size;             // Allocated size
    uint32_t file_size;             // Actual file size
    uint32_t type;                  // FILE_TYPE_REGULAR or FILE_TYPE_DIRECTORY
    uint32_t parent_index;          // Parent directory index
    uint32_t in_use;                // 1 if allocated
} ramfs_node_data_t;

// RamFS node table
static ramfs_node_data_t node_table[MAX_RAMFS_NODES];
static vfs_node_t vfs_nodes[MAX_RAMFS_NODES];

// Root directory index
static uint32_t root_index = 0;

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

// Find a free node slot
static int find_free_node(void) {
    for (int i = 0; i < MAX_RAMFS_NODES; i++) {
        if (!node_table[i].in_use) {
            return i;
        }
    }
    return -1;
}

// Find node by name in parent directory
static int find_node_in_dir(uint32_t parent_idx, const char* name) {
    for (int i = 0; i < MAX_RAMFS_NODES; i++) {
        if (node_table[i].in_use &&
            node_table[i].parent_index == parent_idx &&
            str_cmp(node_table[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// VFS operations for RamFS

// Read from file
static uint32_t ramfs_vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !node->fs_data || !buffer) {
        return 0;
    }

    ramfs_node_data_t* data = (ramfs_node_data_t*)node->fs_data;

    if (data->type != FILE_TYPE_REGULAR) {
        return 0;  // Can't read directory
    }

    if (offset >= data->file_size) {
        return 0;  // EOF
    }

    // Calculate actual bytes to read
    uint32_t bytes_to_read = size;
    if (offset + bytes_to_read > data->file_size) {
        bytes_to_read = data->file_size - offset;
    }

    // Copy data
    for (uint32_t i = 0; i < bytes_to_read; i++) {
        buffer[i] = data->data[offset + i];
    }

    return bytes_to_read;
}

// Write to file
static uint32_t ramfs_vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !node->fs_data || !buffer) {
        return 0;
    }

    ramfs_node_data_t* data = (ramfs_node_data_t*)node->fs_data;

    if (data->type != FILE_TYPE_REGULAR) {
        return 0;  // Can't write to directory
    }

    // Check if we need more space
    uint32_t required_size = offset + size;
    if (required_size > data->data_size) {
        // Allocate more pages
        uint32_t new_size = ((required_size + 4095) / 4096) * 4096;  // Round up to page

        if (new_size > 64 * 4096) {  // Max 256KB per file
            printf("[RamFS] File too large\n");
            return 0;
        }

        // Allocate new buffer
        uint8_t* new_data = (uint8_t*)pmm_alloc_page();
        if (!new_data) {
            printf("[RamFS] Out of memory\n");
            return 0;
        }

        // Copy old data
        if (data->data) {
            for (uint32_t i = 0; i < data->file_size; i++) {
                new_data[i] = data->data[i];
            }
            pmm_free_page((uint32_t)data->data);
        }

        data->data = new_data;
        data->data_size = 4096;  // One page for now
    }

    // Write data
    for (uint32_t i = 0; i < size; i++) {
        data->data[offset + i] = buffer[i];
    }

    // Update file size
    if (offset + size > data->file_size) {
        data->file_size = offset + size;
        node->size = data->file_size;
    }

    return size;
}

// Open file (no-op for RamFS)
static void ramfs_vfs_open(vfs_node_t* node, uint32_t flags) {
    (void)node;
    (void)flags;
    // Nothing to do for RamFS
}

// Close file (no-op for RamFS)
static void ramfs_vfs_close(vfs_node_t* node) {
    (void)node;
    // Nothing to do for RamFS
}

// Read directory entry by index
static vfs_node_t* ramfs_vfs_readdir(vfs_node_t* node, uint32_t index) {
    if (!node || !node->fs_data) {
        return NULL;
    }

    ramfs_node_data_t* dir_data = (ramfs_node_data_t*)node->fs_data;
    if (dir_data->type != FILE_TYPE_DIRECTORY) {
        return NULL;
    }

    // Find the node_table index for this directory
    uint32_t dir_idx = 0;
    for (uint32_t i = 0; i < MAX_RAMFS_NODES; i++) {
        if (&node_table[i] == dir_data) {
            dir_idx = i;
            break;
        }
    }

    // Find the index-th child
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_RAMFS_NODES; i++) {
        if (node_table[i].in_use && node_table[i].parent_index == dir_idx) {
            if (count == index) {
                return &vfs_nodes[i];
            }
            count++;
        }
    }

    return NULL;  // Index out of range
}

// Find entry in directory by name
static vfs_node_t* ramfs_vfs_finddir(vfs_node_t* node, const char* name) {
    if (!node || !node->fs_data || !name) {
        return NULL;
    }

    ramfs_node_data_t* dir_data = (ramfs_node_data_t*)node->fs_data;
    if (dir_data->type != FILE_TYPE_DIRECTORY) {
        return NULL;
    }

    // Find the node_table index for this directory
    uint32_t dir_idx = 0;
    for (uint32_t i = 0; i < MAX_RAMFS_NODES; i++) {
        if (&node_table[i] == dir_data) {
            dir_idx = i;
            break;
        }
    }

    // Search for name in children
    int idx = find_node_in_dir(dir_idx, name);
    if (idx < 0) {
        return NULL;
    }

    return &vfs_nodes[idx];
}

// Initialize a VFS node from node_data
static void init_vfs_node(uint32_t idx) {
    vfs_node_t* vnode = &vfs_nodes[idx];
    ramfs_node_data_t* data = &node_table[idx];

    str_cpy(vnode->name, data->name);
    vnode->inode = idx;
    vnode->type = data->type;
    vnode->size = data->file_size;
    vnode->attributes = FILE_ATTR_NORMAL;
    vnode->creation_time = 0;
    vnode->modification_time = 0;
    vnode->fs_data = data;

    // Set operations
    vnode->read = ramfs_vfs_read;
    vnode->write = ramfs_vfs_write;
    vnode->open = ramfs_vfs_open;
    vnode->close = ramfs_vfs_close;
    vnode->readdir = ramfs_vfs_readdir;
    vnode->finddir = ramfs_vfs_finddir;
}

// Create a file or directory node
static int create_node(const char* name, uint32_t parent_idx, uint32_t type) {
    int idx = find_free_node();
    if (idx < 0) {
        printf("[RamFS] No free nodes\n");
        return -1;
    }

    ramfs_node_data_t* data = &node_table[idx];
    data->in_use = 1;
    str_cpy(data->name, name);
    data->type = type;
    data->parent_index = parent_idx;
    data->file_size = 0;
    data->data_size = 0;
    data->data = NULL;

    init_vfs_node(idx);

    return idx;
}

// RamFS filesystem operations

static vfs_node_t* ramfs_get_root(void) {
    return &vfs_nodes[root_index];
}

static vfs_node_t* ramfs_create_file(const char* path, uint32_t flags) {
    (void)flags;

    // Parse path to get parent and name
    char parent_path[256];
    char name[64];

    // Simple implementation: just use filename for now
    str_cpy(name, path);

    // Create in root directory
    int idx = create_node(name, root_index, FILE_TYPE_REGULAR);
    if (idx < 0) {
        return NULL;
    }

    return &vfs_nodes[idx];
}

static int ramfs_delete_file(const char* path) {
    // Find the file in root directory
    int idx = find_node_in_dir(root_index, path);
    if (idx < 0) {
        return -1;
    }

    ramfs_node_data_t* data = &node_table[idx];

    // Free data if it's a file
    if (data->data) {
        pmm_free_page((uint32_t)data->data);
    }

    data->in_use = 0;
    return 0;
}

static vfs_node_t* ramfs_create_dir(const char* path) {
    // Create in root directory
    int idx = create_node(path, root_index, FILE_TYPE_DIRECTORY);
    if (idx < 0) {
        return NULL;
    }

    return &vfs_nodes[idx];
}

static int ramfs_delete_dir(const char* path) {
    // Find the directory in root
    int idx = find_node_in_dir(root_index, path);
    if (idx < 0) {
        return -1;
    }

    // Check if directory is empty
    for (uint32_t i = 0; i < MAX_RAMFS_NODES; i++) {
        if (node_table[i].in_use && node_table[i].parent_index == (uint32_t)idx) {
            printf("[RamFS] Directory not empty\n");
            return -1;
        }
    }

    node_table[idx].in_use = 0;
    return 0;
}

// RamFS filesystem structure
static vfs_filesystem_t ramfs_filesystem = {
    .name = "ramfs",
    .mount = NULL,
    .unmount = NULL,
    .get_root = ramfs_get_root,
    .create_file = ramfs_create_file,
    .delete_file = ramfs_delete_file,
    .create_dir = ramfs_create_dir,
    .delete_dir = ramfs_delete_dir
};

// Initialize RamFS with VFS support
void ramfs_vfs_init(void) {
    // Clear node table
    for (int i = 0; i < MAX_RAMFS_NODES; i++) {
        node_table[i].in_use = 0;
        node_table[i].data = NULL;
    }

    // Create root directory
    root_index = 0;
    node_table[root_index].in_use = 1;
    str_cpy(node_table[root_index].name, "/");
    node_table[root_index].type = FILE_TYPE_DIRECTORY;
    node_table[root_index].parent_index = root_index;  // Root is its own parent
    node_table[root_index].file_size = 0;
    node_table[root_index].data = NULL;

    init_vfs_node(root_index);

    printf("[RamFS-VFS] Initialized with directory support\n");

    // Register with VFS
    extern int vfs_register_filesystem(vfs_filesystem_t* fs);
    vfs_register_filesystem(&ramfs_filesystem);
}
