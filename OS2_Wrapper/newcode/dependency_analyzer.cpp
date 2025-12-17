#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

// --- LX Header Structures ---
#pragma pack(push, 1)

typedef struct {
    uint8_t  magic[2];
    uint8_t  padding[58];
    uint32_t lfanew;
} DOS_HEADER;

typedef struct {
    uint8_t  magic[2];
    uint8_t  byte_order;
    uint8_t  word_order;
    uint32_t format_level;
    uint32_t cpu_type;
    uint32_t os_type;
    uint32_t module_version;
    uint32_t module_flags;
    uint32_t module_pages;
    uint32_t eip_object;
    uint32_t eip_offset;
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

// Import Procedure Table Entry
typedef struct {
    uint8_t type;  // 0 = by ordinal, other = by name
    union {
        uint16_t ordinal;
        struct {
            uint8_t name_len;
            char name[255];
        } named;
    } proc;
} IMPORT_PROC_ENTRY;

#pragma pack(pop)

// Module type detection
typedef enum {
    APP_CONSOLE,
    APP_GUI_PM,
    APP_DOS,
    APP_UNKNOWN
} AppType;

void read_at(int fd, size_t offset, void* buffer, size_t size) {
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("lseek failed");
        exit(1);
    }
    ssize_t bytes_read = read(fd, buffer, size);
    if (bytes_read != (ssize_t)size) {
        fprintf(stderr, "Failed to read %zu bytes at offset %zx\n", size, offset);
        exit(1);
    }
}

const char* get_module_type(const char* dll_name) {
    char upper[256];
    for (int i = 0; dll_name[i] && i < 255; i++) {
        upper[i] = toupper(dll_name[i]);
    }
    
    if (strstr(upper, "PMWIN") || strstr(upper, "PMGPI") || 
        strstr(upper, "PMSHAPI") || strstr(upper, "PMVIOP")) {
        return "GUI (Presentation Manager)";
    } else if (strstr(upper, "DOSCALL") || strstr(upper, "DOS")) {
        return "System/DOS calls";
    } else if (strstr(upper, "VIOCALLS") || strstr(upper, "KBDCALLS") ||
               strstr(upper, "MOUCALLS")) {
        return "Console/VIO";
    } else if (strstr(upper, "REXX")) {
        return "REXX scripting";
    } else if (strstr(upper, "TCP") || strstr(upper, "SOCKET")) {
        return "Networking";
    } else if (strstr(upper, "MMPM") || strstr(upper, "MCIAPI")) {
        return "Multimedia";
    }
    return "Other";
}

AppType detect_app_type(char imports[][256], int import_count) {
    int has_pm = 0, has_vio = 0;
    
    for (int i = 0; i < import_count; i++) {
        char upper[256];
        for (int j = 0; imports[i][j] && j < 255; j++) {
            upper[j] = toupper(imports[i][j]);
        }
        
        if (strstr(upper, "PMWIN") || strstr(upper, "PMGPI")) {
            has_pm = 1;
        }
        if (strstr(upper, "VIO") || strstr(upper, "KBD") || strstr(upper, "MOU")) {
            has_vio = 1;
        }
    }
    
    if (has_pm) return APP_GUI_PM;
    if (has_vio) return APP_CONSOLE;
    return APP_UNKNOWN;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <os2_exe_file>\n", argv[0]);
        printf("\nAnalyzes OS/2 LX executables to determine:\n");
        printf("  - Application type (Console/GUI/DOS)\n");
        printf("  - Required DLL dependencies\n");
        printf("  - API categories used\n");
        printf("  - 2ine compatibility assessment\n");
        return 1;
    }

    const char *filename = argv[1];
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Could not open file");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("Failed to get file size");
        close(fd);
        return 1;
    }
    off_t file_size = st.st_size;

    // Read DOS Header
    DOS_HEADER dos_hdr;
    read_at(fd, 0, &dos_hdr, sizeof(DOS_HEADER));

    if (dos_hdr.magic[0] != 'M' || dos_hdr.magic[1] != 'Z') {
        fprintf(stderr, "Not a valid MZ executable.\n");
        close(fd);
        return 1;
    }

    if (dos_hdr.lfanew >= file_size) {
        fprintf(stderr, "Invalid LX header offset\n");
        close(fd);
        return 1;
    }

    // Read LX Header
    LX_HEADER lx_hdr;
    read_at(fd, dos_hdr.lfanew, &lx_hdr, sizeof(LX_HEADER));

    if (lx_hdr.magic[0] != 'L' || lx_hdr.magic[1] != 'X') {
        fprintf(stderr, "Not a valid OS/2 LX executable.\n");
        close(fd);
        return 1;
    }

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  OS/2 LX EXECUTABLE ANALYSIS\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    printf("📄 FILE: %s\n", filename);
    printf("   Size: %ld bytes\n\n", (long)file_size);

    // Analyze module flags
    printf("🔧 MODULE INFO:\n");
    printf("   Format Level: %u\n", lx_hdr.format_level);
    printf("   CPU Type: %u (386+)\n", lx_hdr.cpu_type);
    printf("   Module Flags: 0x%08x\n", lx_hdr.module_flags);
    
    int is_dll = (lx_hdr.module_flags & 0x8000) != 0;
    int is_pm = (lx_hdr.module_flags & 0x0300) != 0;
    
    printf("   Type: %s\n", is_dll ? "DLL/Library" : "Executable");
    if (!is_dll) {
        if (is_pm) {
            printf("   UI Type: PM (GUI) Application\n");
        } else {
            printf("   UI Type: Console/Fullscreen Application\n");
        }
    }
    printf("\n");

    // Read Import Module Table
    char imports[256][256];
    int import_count = 0;

    if (lx_hdr.import_module_table_count > 0) {
        off_t table_offset = dos_hdr.lfanew + lx_hdr.import_module_table_offset;
        
        if (table_offset < file_size) {
            if (lseek(fd, table_offset, SEEK_SET) != -1) {
                for (uint32_t i = 0; i < lx_hdr.import_module_table_count && i < 256; i++) {
                    uint8_t len;
                    if (read(fd, &len, 1) == 1 && len > 0 && len < 255) {
                        if (read(fd, imports[i], len) == len) {
                            imports[i][len] = '\0';
                            import_count++;
                        }
                    }
                }
            }
        }
    }

    printf("📚 DEPENDENCIES (%d modules):\n", import_count);
    
    if (import_count == 0) {
        printf("   ⚠️  No imports found - Standalone binary or stripped?\n\n");
    } else {
        for (int i = 0; i < import_count; i++) {
            printf("   [%2d] %-20s (%s)\n", i + 1, imports[i], 
                   get_module_type(imports[i]));
        }
        printf("\n");
    }

    // Detect application type
    AppType app_type = detect_app_type(imports, import_count);
    
    printf("🎯 APPLICATION TYPE DETECTED:\n");
    switch (app_type) {
        case APP_GUI_PM:
            printf("   ⚠️  GUI (Presentation Manager) Application\n");
            printf("   Uses graphical windowing APIs\n");
            break;
        case APP_CONSOLE:
            printf("   ✓ Console/Text Mode Application\n");
            printf("   Uses VIO/keyboard/mouse APIs\n");
            break;
        case APP_DOS:
            printf("   DOS-based Application\n");
            break;
        case APP_UNKNOWN:
            printf("   Unknown/Minimal Dependencies\n");
            break;
    }
    printf("\n");

    // 2ine compatibility assessment
    printf("🚀 2INE COMPATIBILITY ASSESSMENT:\n");
    
    if (app_type == APP_GUI_PM) {
        printf("   Status: ❌ INCOMPATIBLE\n");
        printf("   Reason: Requires Presentation Manager (GUI) support\n");
        printf("   \n");
        printf("   2ine currently only supports command-line applications.\n");
        printf("   This executable uses PM APIs which are not implemented.\n");
        printf("   \n");
        printf("   Recommendations:\n");
        printf("   • Use VirtualBox/QEMU with full OS/2\n");
        printf("   • Try ArcaOS on bare metal/VM\n");
        printf("   • Wait for 2ine PM support (if ever)\n");
    } else if (app_type == APP_CONSOLE) {
        printf("   Status: ⚠️  POSSIBLY COMPATIBLE\n");
        printf("   Reason: Console application (VIO-based)\n");
        printf("   \n");
        printf("   This is a text-mode application that 2ine might support.\n");
        printf("   Success depends on which specific APIs are used.\n");
        printf("   \n");
        printf("   Try running with: ./lx_loader %s\n", filename);
    } else {
        printf("   Status: ❓ UNKNOWN\n");
        printf("   Reason: Unable to determine application type clearly\n");
        printf("   \n");
        printf("   Try running it with 2ine and see what happens!\n");
    }
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    close(fd);
    return 0;
}
