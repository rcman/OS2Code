// File: lx.h
// OS/2 LX (Linear Executable) loader
//
// LX is the native executable format of 32-bit OS/2 (2.0 through Warp).
// This loader supports flat (uncompressed) and zero-fill pages, internal
// and import-by-ordinal fixups, and resolves DOSCALLS imports to a page
// of user-mode syscall thunks mapped into the process.

#ifndef LX_H
#define LX_H

#include "types.h"

// Virtual address where the DOSCALLS thunk page is mapped in LX processes
#define LX_THUNK_BASE   0xB0000000

// LX header, located at e_lfanew (offset 0x3C of the MZ stub),
// or at file offset 0 for stubless modules
typedef struct {
    uint8_t  magic[2];          // "LX"
    uint8_t  byte_order;        // 0 = little endian
    uint8_t  word_order;        // 0 = little endian
    uint32_t format_level;
    uint16_t cpu_type;          // 2 = 386+
    uint16_t os_type;           // 1 = OS/2
    uint32_t module_version;
    uint32_t module_flags;
    uint32_t module_pages;      // Number of pages in module
    uint32_t eip_object;        // Object number for entry point (1-based)
    uint32_t eip_offset;
    uint32_t esp_object;        // Object number for initial stack (1-based, 0 = none)
    uint32_t esp_offset;
    uint32_t page_size;         // Always 4096
    uint32_t page_shift;        // Shift for page data offsets
    uint32_t fixup_size;
    uint32_t fixup_checksum;
    uint32_t loader_size;
    uint32_t loader_checksum;
    uint32_t obj_table_off;     // All table offsets relative to LX header
    uint32_t obj_count;
    uint32_t obj_page_off;
    uint32_t obj_iter_off;
    uint32_t rsrc_off;
    uint32_t rsrc_count;
    uint32_t resname_off;
    uint32_t entry_off;
    uint32_t directives_off;
    uint32_t directives_count;
    uint32_t fixup_page_off;
    uint32_t fixup_rec_off;
    uint32_t import_mod_off;
    uint32_t import_mod_count;
    uint32_t import_proc_off;
    uint32_t page_checksum_off;
    uint32_t data_pages_off;    // Relative to start of FILE
    uint32_t preload_count;
    uint32_t nonres_off;        // Relative to start of FILE
    uint32_t nonres_size;
    uint32_t nonres_checksum;
    uint32_t auto_ds_object;
    uint32_t debug_off;
    uint32_t debug_size;
    uint32_t instance_preload;
    uint32_t instance_demand;
    uint32_t heap_size;
} __attribute__((packed)) lx_header_t;

// Object (segment) table entry
typedef struct {
    uint32_t virtual_size;
    uint32_t reloc_base;        // Preferred virtual base address
    uint32_t flags;
    uint32_t page_table_index;  // 1-based index into object page table
    uint32_t page_count;
    uint32_t reserved;
} __attribute__((packed)) lx_object_t;

// Object flags
#define LX_OBJ_READABLE     0x0001
#define LX_OBJ_WRITABLE     0x0002
#define LX_OBJ_EXECUTABLE   0x0004
#define LX_OBJ_RESOURCE     0x0008
#define LX_OBJ_DISCARDABLE  0x0010
#define LX_OBJ_SHARED       0x0020
#define LX_OBJ_PRELOAD      0x0040

// Object page table entry
typedef struct {
    uint32_t data_offset;       // << page_shift, relative to data_pages_off
    uint16_t data_size;
    uint16_t flags;
} __attribute__((packed)) lx_page_t;

// Page flags
#define LX_PAGE_VALID       0x0000  // Uncompressed data in file
#define LX_PAGE_ITERATED    0x0001  // Run-length compressed (iteration records)
#define LX_PAGE_INVALID     0x0002
#define LX_PAGE_ZEROED      0x0003  // Zero-fill, no file data
#define LX_PAGE_RANGE       0x0004
#define LX_PAGE_ITERDATA2   0x0005  // EXEPACK2 compressed (IBM linker /EXEPACK:2)

// Fixup source types (low nibble of source byte)
#define LX_FIXUP_SRC_MASK       0x0F
#define LX_FIXUP_SRC_OFF32      0x07  // 32-bit offset
#define LX_FIXUP_SRC_REL32      0x08  // 32-bit self-relative
#define LX_FIXUP_SRC_LIST       0x20  // Source list flag

// Fixup target flags
#define LX_FIXUP_TGT_MASK       0x03
#define LX_FIXUP_TGT_INTERNAL   0x00
#define LX_FIXUP_TGT_IMPORD     0x01  // Import by ordinal
#define LX_FIXUP_TGT_IMPNAME    0x02  // Import by name
#define LX_FIXUP_TGT_ENTRY      0x03
#define LX_FIXUP_ADDITIVE       0x04
#define LX_FIXUP_TGTOFF32       0x10  // 32-bit target offset
#define LX_FIXUP_ADD32          0x20  // 32-bit additive value
#define LX_FIXUP_MOD16          0x40  // 16-bit object/module number
#define LX_FIXUP_ORD8           0x80  // 8-bit import ordinal

// Returns 1 if the buffer looks like an LX executable (MZ stub or bare)
int lx_validate(const void* data, uint32_t size);

// Create a process from an LX executable image. `args` is the argument
// string placed after the program name in the OS/2 command line (may be
// NULL or empty). Returns PID on success, 0 on failure.
uint32_t lx_exec(const char* name, const char* args,
                 const void* data, uint32_t size);

#endif // LX_H
