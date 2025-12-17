#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

// --- LX Header Structures ---
#pragma pack(push, 1)

typedef struct {
    uint8_t  magic[2];      // "MZ"
    uint8_t  padding[58];
    uint32_t lfanew;        // Offset to the LX header
} DOS_HEADER;

typedef struct {
    uint8_t  magic[2];      // "LX"
    uint8_t  byte_order;
    uint8_t  word_order;
    uint32_t format_level;
    uint32_t cpu_type;
    uint32_t os_type;
    uint32_t module_version;
    uint32_t module_flags;
    uint32_t module_pages;  
    uint32_t eip_object;    // Entry Point Object (Segment)
    uint32_t eip_offset;    // Entry Point Offset
    uint32_t esp_object;
    uint32_t esp_offset;
    uint32_t page_size;     
    uint32_t page_offset_shift;
    uint32_t fixup_section_size;
    uint32_t fixup_section_checksum;
    uint32_t loader_section_size;
    uint32_t loader_section_checksum;
    uint32_t object_table_offset;
    uint32_t object_count; 
    uint32_t object_page_table_offset;
    uint32_t object_iter_pages_offset;
    uint32_t resource_table_offset;
    uint32_t resource_table_count;
    uint32_t resident_name_table_offset;
    uint32_t entry_table_offset;
    uint32_t module_directives_offset;
    uint32_t module_directives_count;
    uint32_t fixup_page_table_offset;
    uint32_t fixup_record_table_offset;
    uint32_t import_module_table_offset;
    uint32_t import_module_table_count;
    uint32_t import_proc_table_offset;
    uint32_t per_page_checksum_offset;
    uint32_t data_pages_offset;
    uint32_t preload_page_count;
    uint32_t non_resident_name_table_offset;
    uint32_t non_resident_name_table_len;
    uint32_t non_resident_name_table_checksum;
    uint32_t auto_ds_object;
    uint32_t debug_info_offset;
    uint32_t debug_info_len;
    uint32_t instance_preload_count;
    uint32_t instance_demand_count;
    uint32_t heapsize;
} LX_HEADER;

typedef struct {
    uint32_t virtual_size;
    uint32_t reloc_base_addr;
    uint32_t object_flags;
    uint32_t page_table_index;
    uint32_t page_count;
    uint32_t reserved;
} LX_OBJECT_ENTRY;

#pragma pack(pop)

void read_at(int fd, size_t offset, void* buffer, size_t size) {
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("lseek failed");
        exit(1);
    }
    ssize_t bytes_read = read(fd, buffer, size);
    if (bytes_read != (ssize_t)size) {
        fprintf(stderr, "Failed to read %zu bytes at offset %zx (got %zd bytes)\n", 
                size, offset, bytes_read);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <os2_exe_file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Could not open file");
        return 1;
    }

    // Get file size for bounds checking
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("Failed to get file size");
        close(fd);
        return 1;
    }
    off_t file_size = st.st_size;

    // 1. Read DOS Header
    DOS_HEADER dos_hdr;
    read_at(fd, 0, &dos_hdr, sizeof(DOS_HEADER));

    if (dos_hdr.magic[0] != 'M' || dos_hdr.magic[1] != 'Z') {
        fprintf(stderr, "Not a valid MZ executable.\n");
        close(fd);
        return 1;
    }

    // Validate LX header offset
    if (dos_hdr.lfanew >= file_size || dos_hdr.lfanew + sizeof(LX_HEADER) > file_size) {
        fprintf(stderr, "Invalid LX header offset: 0x%x (file size: %ld)\n", 
                dos_hdr.lfanew, (long)file_size);
        close(fd);
        return 1;
    }

    // 2. Read LX Header
    LX_HEADER lx_hdr;
    read_at(fd, dos_hdr.lfanew, &lx_hdr, sizeof(LX_HEADER));

    if (lx_hdr.magic[0] != 'L' || lx_hdr.magic[1] != 'X') {
        fprintf(stderr, "Not a valid OS/2 LX executable.\n");
        close(fd);
        return 1;
    }

    printf("[+] Parsed LX Header.\n");

    // 3. Read Import Module Table
    printf("\n[+] Import Module Table (Dependencies):\n");
    printf("    Count: %u modules\n", lx_hdr.import_module_table_count);

    if (lx_hdr.import_module_table_count > 0) {
        // Calculate absolute offset in file
        off_t table_offset = dos_hdr.lfanew + lx_hdr.import_module_table_offset;
        
        // Validate offset is within file bounds
        if (table_offset >= file_size) {
            fprintf(stderr, "Import table offset 0x%lx is beyond file size %ld\n",
                    (long)table_offset, (long)file_size);
            close(fd);
            return 1;
        }

        // Seek to the start of the table
        if (lseek(fd, table_offset, SEEK_SET) == -1) {
            perror("Failed to seek to import table");
            close(fd);
            return 1;
        }

        // Read each import module name
        for (uint32_t i = 0; i < lx_hdr.import_module_table_count; i++) {
            uint8_t len;
            ssize_t bytes_read = read(fd, &len, 1);
            
            if (bytes_read != 1) {
                fprintf(stderr, "Failed to read length byte for module %u\n", i + 1);
                break;
            }

            if (len == 0) {
                printf("    [%u] <empty name>\n", i + 1);
                continue;
            }

            // Protect against buffer overflow
            char name_buf[256];
            size_t read_len = (len > 255) ? 255 : len;
            
            bytes_read = read(fd, name_buf, read_len);
            if (bytes_read != (ssize_t)read_len) {
                fprintf(stderr, "Failed to read name for module %u (expected %zu, got %zd)\n",
                        i + 1, read_len, bytes_read);
                break;
            }

            // If we truncated, skip the remaining bytes
            if (len > 255) {
                if (lseek(fd, len - 255, SEEK_CUR) == -1) {
                    fprintf(stderr, "Failed to skip truncated bytes for module %u\n", i + 1);
                    break;
                }
            }

            name_buf[read_len] = '\0';
            printf("    [%u] %s", i + 1, name_buf);
            if (len > 255) {
                printf(" (truncated from %u bytes)", len);
            }
            printf("\n");
        }
    } else {
        printf("    No imports found (Standalone binary?)\n");
    }

    close(fd);
    return 0;
}
