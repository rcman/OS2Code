#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// DOS MZ Header
typedef struct {
uint16_t e_magic;    // "MZ"
uint8_t  e_skip[58]; // Skip to new header offset
uint32_t e_lfanew;   // Offset to LE header
} __attribute__((packed)) mz_header_t;

// LE Header (simplified - only key fields)
typedef struct {
uint16_t magic;           // "LE" or "LX"
uint8_t  byte_order;
uint8_t  word_order;
uint32_t format_level;
uint16_t cpu_type;
uint16_t target_os;
uint32_t module_version;
uint32_t module_flags;
uint32_t module_pages;
uint32_t eip_object;      // Entry point object number
uint32_t eip;             // Entry point offset
uint32_t esp_object;      // Stack object number
uint32_t esp;             // Stack pointer
uint32_t page_size;
uint32_t page_offset_shift;
uint32_t fixup_size;
uint32_t fixup_checksum;
uint32_t loader_size;
uint32_t loader_checksum;
uint32_t object_table_offset;
uint32_t object_count;
uint32_t object_page_table_offset;
uint32_t object_iter_pages_offset;
uint32_t resource_table_offset;
uint32_t resource_count;
uint32_t resident_names_offset;
uint32_t entry_table_offset;
uint32_t module_directives_offset;
uint32_t module_directives_count;
uint32_t fixup_page_table_offset;
uint32_t fixup_record_table_offset;
uint32_t imported_modules_offset;
uint32_t imported_modules_count;
uint32_t imported_proc_table_offset;
uint32_t per_page_checksum_offset;
uint32_t data_pages_offset;
uint32_t preload_pages_count;
uint32_t non_resident_names_offset;
uint32_t non_resident_names_length;
uint32_t non_resident_names_checksum;
uint32_t auto_ds_object;
uint32_t debug_info_offset;
uint32_t debug_info_length;
uint32_t instance_preload_count;
uint32_t instance_demand_count;
uint32_t heap_size;
} __attribute__((packed)) le_header_t;

// Object Table Entry
typedef struct {
uint32_t virtual_size;
uint32_t reloc_base_addr;
uint32_t object_flags;
uint32_t page_table_index;
uint32_t page_table_entries;
uint32_t reserved;
} __attribute__((packed)) object_entry_t;

// Object flags
#define OBJREADABLE   0x0001
#define OBJWRITEABLE  0x0002
#define OBJEXECUTABLE 0x0004
#define OBJRESOURCE   0x0008
#define OBJDISCARDABLE 0x0010
#define OBJSHARED     0x0020
#define OBJPRELOAD    0x0040
#define OBJINVALID    0x0080

typedef struct {
void *base;
size_t size;
uint32_t flags;
} loaded_object_t;

typedef struct {
uint8_t *file_data;
size_t file_size;
le_header_t *le_header;
object_entry_t *objects;
loaded_object_t *loaded_objects;
void *entry_point;
} os2_exe_t;

// Read entire file into memory
uint8_t* read_file(const char *filename, size_t *size) {
int fd = open(filename, O_RDONLY);
if (fd < 0) {
perror("open");
return NULL;
}

off_t len = lseek(fd, 0, SEEK_END);
if (len < 0) {
    perror("lseek");
    close(fd);
    return NULL;
}

lseek(fd, 0, SEEK_SET);

uint8_t *data = malloc(len);
if (!data) {
    close(fd);
    return NULL;
}

if (read(fd, data, len) != len) {
    perror("read");
    free(data);
    close(fd);
    return NULL;
}

close(fd);
*size = len;
return data;

}

// Parse LE executable
os2_exe_t* parse_le_exe(const char *filename) {
os2_exe_t *exe = calloc(1, sizeof(os2_exe_t));
if (!exe) return NULL;

exe->file_data = read_file(filename, &exe->file_size);
if (!exe->file_data) {
    free(exe);
    return NULL;
}

// Check MZ header
mz_header_t *mz = (mz_header_t*)exe->file_data;
if (mz->e_magic != 0x5A4D) { // "MZ"
    fprintf(stderr, "Not a valid DOS executable\n");
    free(exe->file_data);
    free(exe);
    return NULL;
}

// Get LE header
exe->le_header = (le_header_t*)(exe->file_data + mz->e_lfanew);

if (exe->le_header->magic != 0x454C && exe->le_header->magic != 0x584C) { // "LE" or "LX"
    fprintf(stderr, "Not a valid LE/LX executable (magic: 0x%04x)\n", exe->le_header->magic);
    free(exe->file_data);
    free(exe);
    return NULL;
}

printf("LE executable found:\n");
printf("  CPU type: %d\n", exe->le_header->cpu_type);
printf("  Target OS: %d\n", exe->le_header->target_os);
printf("  Object count: %d\n", exe->le_header->object_count);
printf("  Entry point: Object %d, Offset 0x%08x\n", 
       exe->le_header->eip_object, exe->le_header->eip);

// Get object table
exe->objects = (object_entry_t*)(exe->file_data + mz->e_lfanew + 
                                  exe->le_header->object_table_offset);

return exe;

}

// Load objects into memory
int load_objects(os2_exe_t *exe) {
exe->loaded_objects = calloc(exe->le_header->object_count, sizeof(loaded_object_t));
if (!exe->loaded_objects) return -1;

uint32_t data_pages_offset = exe->le_header->data_pages_offset;

for (uint32_t i = 0; i < exe->le_header->object_count; i++) {
    object_entry_t *obj = &exe->objects[i];
    
    printf("\nObject %d:\n", i + 1);
    printf("  Virtual size: 0x%08x\n", obj->virtual_size);
    printf("  Base address: 0x%08x\n", obj->reloc_base_addr);
    printf("  Flags: 0x%08x ", obj->object_flags);
    if (obj->object_flags & OBJREADABLE) printf("R");
    if (obj->object_flags & OBJWRITEABLE) printf("W");
    if (obj->object_flags & OBJEXECUTABLE) printf("X");
    printf("\n");
    
    // Determine protection flags
    int prot = PROT_NONE;
    if (obj->object_flags & OBJREADABLE) prot |= PROT_READ;
    if (obj->object_flags & OBJWRITEABLE) prot |= PROT_WRITE;
    if (obj->object_flags & OBJEXECUTABLE) prot |= PROT_EXEC;
    
    // Allocate memory for object
    size_t alloc_size = (obj->virtual_size + 0xFFF) & ~0xFFF; // Page align
    void *mem = mmap(NULL, alloc_size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (mem == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    
    exe->loaded_objects[i].base = mem;
    exe->loaded_objects[i].size = alloc_size;
    exe->loaded_objects[i].flags = obj->object_flags;
    
    // Copy object data from file
    // Note: This is simplified - real implementation needs to handle page tables
    uint32_t page_offset = (obj->page_table_index - 1) * exe->le_header->page_size;
    uint32_t copy_size = obj->virtual_size < obj->page_table_entries * exe->le_header->page_size ?
                         obj->virtual_size : obj->page_table_entries * exe->le_header->page_size;
    
    if (data_pages_offset + page_offset + copy_size <= exe->file_size) {
        memcpy(mem, exe->file_data + data_pages_offset + page_offset, copy_size);
        printf("  Loaded %d bytes at %p\n", copy_size, mem);
    }
}

// Calculate entry point
if (exe->le_header->eip_object > 0 && 
    exe->le_header->eip_object <= exe->le_header->object_count) {
    exe->entry_point = (uint8_t*)exe->loaded_objects[exe->le_header->eip_object - 1].base + 
                      exe->le_header->eip;
    printf("\nEntry point calculated: %p\n", exe->entry_point);
}

return 0;

}

// Execute the loaded program
void execute_os2_program(os2_exe_t *exe) {
if (!exe->entry_point) {
fprintf(stderr, "No entry point found\n");
return;
}

printf("\n=== Attempting to execute OS/2 program ===\n");
printf("WARNING: This will likely crash without proper API emulation!\n");
printf("Entry point: %p\n\n", exe->entry_point);

// Cast to function pointer and call
// Note: This WILL crash for real OS/2 apps without API emulation
void (*entry)() = (void(*)())exe->entry_point;
entry();

}

// Cleanup
void free_os2_exe(os2_exe_t *exe) {
if (!exe) return;

if (exe->loaded_objects) {
    for (uint32_t i = 0; i < exe->le_header->object_count; i++) {
        if (exe->loaded_objects[i].base) {
            munmap(exe->loaded_objects[i].base, exe->loaded_objects[i].size);
        }
    }
    free(exe->loaded_objects);
}

free(exe->file_data);
free(exe);

}

int main(int argc, char **argv) {
if (argc < 2) {
fprintf(stderr, "Usage: %s <os2_executable.exe>\n", argv[0]);
return 1;
}

printf("OS/2 LE Loader for Linux\n");
printf("========================\n\n");

os2_exe_t *exe = parse_le_exe(argv[1]);
if (!exe) {
    return 1;
}

if (load_objects(exe) < 0) {
    fprintf(stderr, "Failed to load objects\n");
    free_os2_exe(exe);
    return 1;
}

printf("\n=== Loader complete ===\n");
printf("To actually run OS/2 apps, you'll need to implement API emulation\n");
printf("Press Enter to attempt execution (will likely crash)...\n");
getchar();

execute_os2_program(exe);

free_os2_exe(exe);
return 0;

}