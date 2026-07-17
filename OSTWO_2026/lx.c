// File: lx.c
// OS/2 LX (Linear Executable) loader implementation

#include "lx.h"
#include "process.h"
#include "vmm.h"
#include "pmm.h"

// External functions
extern void printf(const char* format, ...);
extern process_t* process_current(void);
extern uint32_t process_next_pid(void);
extern process_t* process_alloc_slot(void);

// User-mode thunk blob (lx_thunks.asm)
extern uint8_t lx_thunk_blob_start[];
extern uint8_t lx_thunk_blob_end[];
typedef struct { uint32_t module; uint32_t ordinal; uint32_t offset; } lx_thunk_entry_t;
extern lx_thunk_entry_t lx_thunk_table[];
extern uint32_t lx_thunk_count;

// Module ids in the thunk table
#define LX_MOD_DOSCALLS 0
#define LX_MOD_NLS      1
#define LX_MOD_MSG      2

// User addresses of the current LX process's info blocks, published for
// the DosGetInfoBlocks syscall. One LX process runs at a time here.
uint32_t lx_pib_addr = 0;
uint32_t lx_tib_addr = 0;

#define LX_MAX_OBJECTS 16

static int str_copy_safe(char* dest, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
    return i;
}

// Locate the LX header inside the file (behind an MZ stub or bare)
static const lx_header_t* lx_find_header(const uint8_t* data, uint32_t size) {
    if (size < sizeof(lx_header_t)) {
        return NULL;
    }

    uint32_t lx_off = 0;
    if (data[0] == 'M' && data[1] == 'Z') {
        if (size < 0x40) {
            return NULL;
        }
        lx_off = *(const uint32_t*)(data + 0x3C);  // e_lfanew
        if (lx_off == 0 || lx_off + sizeof(lx_header_t) > size) {
            return NULL;
        }
    }

    const lx_header_t* hdr = (const lx_header_t*)(data + lx_off);
    if (hdr->magic[0] != 'L' || hdr->magic[1] != 'X') {
        return NULL;
    }
    return hdr;
}

int lx_validate(const void* data, uint32_t size) {
    return lx_find_header((const uint8_t*)data, size) != NULL;
}

// Case-insensitive counted-string compare against an ASCII literal
static int lx_name_is(const char* name, uint8_t len, const char* lit) {
    for (uint8_t i = 0; i < len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (lit[i] == '\0' || c != lit[i]) {
            return 0;
        }
    }
    return lit[len] == '\0';
}

// Look up an import in the thunk table.
static uint32_t lx_resolve_import(const lx_header_t* hdr, uint32_t module_ord,
                                  uint32_t proc_ord) {
    // Walk the import module name table (counted strings, 1-based)
    const uint8_t* p = (const uint8_t*)hdr + hdr->import_mod_off;
    const char* mod_name = NULL;
    uint8_t mod_len = 0;

    for (uint32_t m = 1; m <= hdr->import_mod_count; m++) {
        mod_len = *p;
        if (m == module_ord) {
            mod_name = (const char*)(p + 1);
            break;
        }
        p += 1 + mod_len;
    }

    if (!mod_name) {
        printf("[LX] ERROR: Import references invalid module %d\n", module_ord);
        return 0;
    }

    int mod_id = -1;
    if (lx_name_is(mod_name, mod_len, "DOSCALLS")) {
        mod_id = LX_MOD_DOSCALLS;
    } else if (lx_name_is(mod_name, mod_len, "MSG")) {
        mod_id = LX_MOD_MSG;
    } else if (lx_name_is(mod_name, mod_len, "NLS")) {
        mod_id = LX_MOD_NLS;
    }

    if (mod_id >= 0) {
        for (uint32_t i = 0; i < lx_thunk_count; i++) {
            if (lx_thunk_table[i].module == (uint32_t)mod_id &&
                lx_thunk_table[i].ordinal == proc_ord) {
                return LX_THUNK_BASE + lx_thunk_table[i].offset;
            }
        }
    }

    // No implementation: resolve to the fallback stub so the module
    // still loads (programs import far more than they call). Calling
    // the stub returns ERROR_INVALID_FUNCTION.
    extern uint32_t lx_thunk_unimpl_offset;
    printf("[LX] WARNING: import ");
    for (uint8_t i = 0; i < mod_len; i++) printf("%c", mod_name[i]);
    printf(".%d not implemented (stubbed)\n", proc_ord);
    return LX_THUNK_BASE + lx_thunk_unimpl_offset;
}

// Expand an ITERATED (run-length) page: a sequence of records
//   uint16 iterations, uint16 data_size, uint8 data[data_size]
// each expanded to `iterations` copies of the data block.
// Returns bytes written to dst, or -1 on malformed data.
static int lx_expand_iterdata(uint8_t* dst, int dst_size,
                              const uint8_t* src, int src_size) {
    int si = 0, di = 0;
    while (si + 4 <= src_size) {
        uint16_t iterations = src[si] | (src[si + 1] << 8);
        uint16_t data_size = src[si + 2] | (src[si + 3] << 8);
        si += 4;
        if (iterations == 0 && data_size == 0) {
            break;
        }
        if (si + data_size > src_size ||
            di + (int)iterations * data_size > dst_size) {
            return -1;
        }
        for (uint16_t it = 0; it < iterations; it++) {
            for (uint16_t j = 0; j < data_size; j++) {
                dst[di++] = src[si + j];
            }
        }
        si += data_size;
    }
    return di;
}

// Expand an EXEPACK2 (ITERDATA2) page. Stream of variable-length
// records selected by the low 2 bits of the first byte:
//   00: b==0        -> [0, count, value]: run of `count` copies of value
//                      (count 0 = end of stream)
//       b!=0        -> literal block of (b >> 2) bytes
//   01: 16-bit word -> ((w>>2)&3) literal bytes, then copy ((w>>4)&7)+3
//                      bytes from dst - (w>>7)
//   10: 16-bit word -> copy ((w>>2)&3)+3 bytes from dst - (w>>4)
//   11: 24-bit word -> ((w>>2)&0xF) literal bytes, then copy (w>>6)&0x3F
//                      bytes from dst - (w>>12)
// Back-references may overlap (byte-by-byte copy).
// Returns bytes written to dst, or -1 on malformed data.
static int lx_expand_iterdata2(uint8_t* dst, int dst_size,
                               const uint8_t* src, int src_size) {
    int si = 0, di = 0;
    while (si < src_size) {
        uint8_t b = src[si];
        switch (b & 3) {
            case 0: {
                if (b == 0) {
                    if (si + 1 >= src_size || src[si + 1] == 0) {
                        return di;  // End of stream
                    }
                    if (si + 2 >= src_size) return -1;
                    int count = src[si + 1];
                    uint8_t value = src[si + 2];
                    if (di + count > dst_size) return -1;
                    while (count--) dst[di++] = value;
                    si += 3;
                } else {
                    int len = b >> 2;
                    if (si + 1 + len > src_size || di + len > dst_size) return -1;
                    for (int j = 0; j < len; j++) dst[di++] = src[si + 1 + j];
                    si += 1 + len;
                }
                break;
            }
            case 1: {
                if (si + 2 > src_size) return -1;
                uint16_t w = src[si] | (src[si + 1] << 8);
                int lit = (w >> 2) & 3;
                int len = ((w >> 4) & 7) + 3;
                int off = w >> 7;
                si += 2;
                if (si + lit > src_size || di + lit + len > dst_size) return -1;
                for (int j = 0; j < lit; j++) dst[di++] = src[si++];
                if (off == 0 || off > di) return -1;
                for (int j = 0; j < len; j++, di++) dst[di] = dst[di - off];
                break;
            }
            case 2: {
                if (si + 2 > src_size) return -1;
                uint16_t w = src[si] | (src[si + 1] << 8);
                int len = ((w >> 2) & 3) + 3;
                int off = w >> 4;
                si += 2;
                if (off == 0 || off > di || di + len > dst_size) return -1;
                for (int j = 0; j < len; j++, di++) dst[di] = dst[di - off];
                break;
            }
            default: {  // case 3
                if (si + 3 > src_size) return -1;
                uint32_t w = src[si] | (src[si + 1] << 8) | ((uint32_t)src[si + 2] << 16);
                int lit = (w >> 2) & 0xF;
                int len = (w >> 6) & 0x3F;
                int off = w >> 12;
                si += 3;
                if (si + lit > src_size || di + lit + len > dst_size) return -1;
                for (int j = 0; j < lit; j++) dst[di++] = src[si++];
                if (len > 0) {
                    if (off == 0 || off > di) return -1;
                    for (int j = 0; j < len; j++, di++) dst[di] = dst[di - off];
                }
                break;
            }
        }
    }
    return di;
}

// Apply the fixups for one logical page.
// Must be called with the process page directory active.
static int lx_apply_page_fixups(const lx_header_t* hdr, uint32_t logical_page,
                                uint32_t page_vaddr, const uint32_t* obj_bases,
                                uint32_t obj_count) {
    const uint8_t* lx_base = (const uint8_t*)hdr;
    const uint32_t* fixup_page_table = (const uint32_t*)(lx_base + hdr->fixup_page_off);
    const uint8_t* rec_base = lx_base + hdr->fixup_rec_off;

    const uint8_t* ptr = rec_base + fixup_page_table[logical_page];
    const uint8_t* end = rec_base + fixup_page_table[logical_page + 1];

    while (ptr < end) {
        uint8_t src = *ptr++;
        uint8_t flags = *ptr++;
        uint8_t src_type = src & LX_FIXUP_SRC_MASK;

        // Source offset(s)
        int16_t src_off = 0;
        uint8_t src_count = 1;
        const uint8_t* src_list = NULL;
        if (src & LX_FIXUP_SRC_LIST) {
            src_count = *ptr++;
        } else {
            src_off = (int16_t)(ptr[0] | (ptr[1] << 8));
            ptr += 2;
        }

        // Target address
        uint32_t target = 0;
        uint8_t tgt_type = flags & LX_FIXUP_TGT_MASK;

        if (tgt_type == LX_FIXUP_TGT_INTERNAL) {
            uint32_t obj_num;
            if (flags & LX_FIXUP_MOD16) {
                obj_num = ptr[0] | (ptr[1] << 8);
                ptr += 2;
            } else {
                obj_num = *ptr++;
            }
            uint32_t tgt_off = 0;
            if (src_type != 0x02) {  // Selector fixups carry no offset
                if (flags & LX_FIXUP_TGTOFF32) {
                    tgt_off = *(const uint32_t*)ptr;
                    ptr += 4;
                } else {
                    tgt_off = ptr[0] | (ptr[1] << 8);
                    ptr += 2;
                }
            }
            if (obj_num == 0 || obj_num > obj_count) {
                printf("[LX] ERROR: Fixup targets invalid object %d\n", obj_num);
                return -1;
            }
            target = obj_bases[obj_num - 1] + tgt_off;
        } else if (tgt_type == LX_FIXUP_TGT_IMPORD) {
            uint32_t mod_ord;
            if (flags & LX_FIXUP_MOD16) {
                mod_ord = ptr[0] | (ptr[1] << 8);
                ptr += 2;
            } else {
                mod_ord = *ptr++;
            }
            uint32_t proc_ord;
            if (flags & LX_FIXUP_ORD8) {
                proc_ord = *ptr++;
            } else if (flags & LX_FIXUP_TGTOFF32) {
                proc_ord = *(const uint32_t*)ptr;
                ptr += 4;
            } else {
                proc_ord = ptr[0] | (ptr[1] << 8);
                ptr += 2;
            }
            target = lx_resolve_import(hdr, mod_ord, proc_ord);
            if (target == 0) {
                return -1;
            }
        } else {
            printf("[LX] ERROR: Unsupported fixup target type %d\n", tgt_type);
            return -1;
        }

        // Additive value
        uint32_t additive = 0;
        if (flags & LX_FIXUP_ADDITIVE) {
            if (flags & LX_FIXUP_ADD32) {
                additive = *(const uint32_t*)ptr;
                ptr += 4;
            } else {
                additive = ptr[0] | (ptr[1] << 8);
                ptr += 2;
            }
        }

        if (src & LX_FIXUP_SRC_LIST) {
            src_list = ptr;
            ptr += src_count * 2;
        }

        // Patch each source location
        for (uint8_t s = 0; s < src_count; s++) {
            int16_t off = src_list
                ? (int16_t)(src_list[s * 2] | (src_list[s * 2 + 1] << 8))
                : src_off;
            uint32_t site = page_vaddr + off;

            switch (src_type) {
                case LX_FIXUP_SRC_OFF32:
                    *(uint32_t*)site = target + additive;
                    break;
                case LX_FIXUP_SRC_REL32:
                    *(uint32_t*)site = (target + additive) - (site + 4);
                    break;
                case 0x06:
                    // 16:32 far pointer (offset dword + selector word).
                    // The flat user code selector makes far calls to
                    // 32-bit targets work normally.
                    *(uint32_t*)site = target + additive;
                    *(uint16_t*)(site + 4) = 0x1B;
                    break;
                case 0x03:
                    // 16:16 far pointer - needs LDT selector tiling we
                    // don't provide. Null selector: unused 16-bit API
                    // paths stay inert; calling one faults cleanly.
                    *(uint16_t*)site = (uint16_t)((target + additive) & 0xFFFF);
                    *(uint16_t*)(site + 2) = 0;
                    break;
                case 0x02:
                    // 16-bit selector only
                    *(uint16_t*)site = 0;
                    break;
                default:
                    printf("[LX] ERROR: Unsupported fixup source type 0x%x\n", src_type);
                    return -1;
            }
        }
    }

    return 0;
}

// Load objects, pages and fixups into the given page directory.
// Returns entry point, or 0 on failure. Fills esp_out if the module
// specifies an initial stack (else leaves it untouched).
static uint32_t lx_load_image(const uint8_t* file, uint32_t size,
                              const lx_header_t* hdr, uint32_t page_directory,
                              uint32_t* esp_out) {
    const uint8_t* lx_base = (const uint8_t*)hdr;
    const lx_object_t* objects = (const lx_object_t*)(lx_base + hdr->obj_table_off);
    const lx_page_t* page_table = (const lx_page_t*)(lx_base + hdr->obj_page_off);

    if (hdr->obj_count == 0 || hdr->obj_count > LX_MAX_OBJECTS) {
        printf("[LX] ERROR: Bad object count %d\n", hdr->obj_count);
        return 0;
    }
    if (hdr->page_size != PAGE_SIZE) {
        printf("[LX] ERROR: Unsupported page size %d\n", hdr->page_size);
        return 0;
    }

    // Prefer the module's linked base addresses. OS/2 EXEs link into
    // the private arena (0x10000+) and often have their internal fixups
    // stripped, in which case they can ONLY run at the preferred base.
    // The VMM supports user mappings below 0xA0000 through private
    // page-table clones, so preferred-base loading works for modules
    // that fit in the 0x10000-0x9FFFF window (or anywhere >= 0x40000000).
    // Modules that don't fit are rebased into the user arena instead -
    // valid only when internal fixup records are present.
    uint32_t obj_bases[LX_MAX_OBJECTS];
    int use_preferred = 1;
    for (uint32_t i = 0; i < hdr->obj_count; i++) {
        uint32_t base = objects[i].reloc_base;
        uint32_t end = base + objects[i].virtual_size;
        int in_low_window = (base >= 0x10000 && end <= 0xA0000);
        int in_user_space = (base >= 0x40000000 && end <= 0xB0000000);
        if (base == 0 || (!in_low_window && !in_user_space)) {
            use_preferred = 0;
        }
    }

    uint32_t load_next = 0x40000000;
    for (uint32_t i = 0; i < hdr->obj_count; i++) {
        if (use_preferred) {
            obj_bases[i] = objects[i].reloc_base;
        } else {
            obj_bases[i] = load_next;
            load_next += (objects[i].virtual_size + 0xFFFF) & ~0xFFFF;
        }
    }
    if (!use_preferred) {
        printf("[LX] Preferred bases unusable, rebasing to 0x40000000 "
               "(requires internal fixups)\n");
    }

    uint32_t old_pd = vmm_get_current_directory();
    vmm_switch_page_directory(page_directory);

    // Pass 1: allocate, map and fill every object's pages
    for (uint32_t i = 0; i < hdr->obj_count; i++) {
        const lx_object_t* obj = &objects[i];
        uint32_t base = obj_bases[i];
        uint32_t vsize = obj->virtual_size;
        uint32_t num_pages = (vsize + PAGE_SIZE - 1) / PAGE_SIZE;

        printf("[LX] Object %d: base=0x%x (pref 0x%x) size=0x%x flags=0x%x pages=%d\n",
               i + 1, base, obj->reloc_base, vsize, obj->flags, num_pages);

        for (uint32_t p = 0; p < num_pages; p++) {
            uint32_t virt = base + p * PAGE_SIZE;
            uint32_t phys = pmm_alloc_page();
            if (phys == 0 ||
                !vmm_map_page(virt, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER)) {
                printf("[LX] ERROR: Failed to map page at 0x%x\n", virt);
                vmm_switch_page_directory(old_pd);
                return 0;
            }
            uint32_t* zp = (uint32_t*)virt;
            for (uint32_t j = 0; j < PAGE_SIZE / 4; j++) {
                zp[j] = 0;
            }
        }

        // Copy page data from the file
        for (uint32_t p = 0; p < obj->page_count && p < num_pages; p++) {
            const lx_page_t* pg = &page_table[obj->page_table_index - 1 + p];
            uint8_t* dst = (uint8_t*)(base + p * PAGE_SIZE);

            if (pg->flags == LX_PAGE_ZEROED) {
                continue;  // Already zeroed
            }

            uint32_t file_off = hdr->data_pages_off +
                                (pg->data_offset << hdr->page_shift);
            if (file_off + pg->data_size > size) {
                printf("[LX] ERROR: Page data outside file (off=0x%x)\n", file_off);
                vmm_switch_page_directory(old_pd);
                return 0;
            }
            const uint8_t* src = file + file_off;

            int expanded;
            switch (pg->flags) {
                case LX_PAGE_VALID:
                    for (uint32_t j = 0; j < pg->data_size; j++) {
                        dst[j] = src[j];
                    }
                    expanded = pg->data_size;
                    break;
                case LX_PAGE_ITERATED:
                    expanded = lx_expand_iterdata(dst, PAGE_SIZE, src, pg->data_size);
                    break;
                case LX_PAGE_ITERDATA2:
                    expanded = lx_expand_iterdata2(dst, PAGE_SIZE, src, pg->data_size);
                    break;
                default:
                    printf("[LX] ERROR: Unsupported page type %d\n", pg->flags);
                    vmm_switch_page_directory(old_pd);
                    return 0;
            }
            if (expanded < 0) {
                printf("[LX] ERROR: Corrupt compressed page (type %d)\n", pg->flags);
                vmm_switch_page_directory(old_pd);
                return 0;
            }
        }
    }

    // Pass 2: apply fixups per logical page
    for (uint32_t i = 0; i < hdr->obj_count; i++) {
        const lx_object_t* obj = &objects[i];
        for (uint32_t p = 0; p < obj->page_count; p++) {
            uint32_t logical = obj->page_table_index - 1 + p;
            uint32_t vaddr = obj_bases[i] + p * PAGE_SIZE;
            if (lx_apply_page_fixups(hdr, logical, vaddr, obj_bases,
                                     hdr->obj_count) != 0) {
                vmm_switch_page_directory(old_pd);
                return 0;
            }
        }
    }

    // Map the DOSCALLS thunk page
    {
        uint32_t phys = pmm_alloc_page();
        if (phys == 0 ||
            !vmm_map_page(LX_THUNK_BASE, phys,
                          PTE_PRESENT | PTE_WRITABLE | PTE_USER)) {
            printf("[LX] ERROR: Failed to map thunk page\n");
            vmm_switch_page_directory(old_pd);
            return 0;
        }
        uint32_t blob_size = (uint32_t)(lx_thunk_blob_end - lx_thunk_blob_start);
        uint8_t* dst = (uint8_t*)LX_THUNK_BASE;
        for (uint32_t j = 0; j < blob_size && j < PAGE_SIZE; j++) {
            dst[j] = lx_thunk_blob_start[j];
        }
    }

    vmm_switch_page_directory(old_pd);

    // Entry point
    if (hdr->eip_object == 0 || hdr->eip_object > hdr->obj_count) {
        printf("[LX] ERROR: Bad entry object %d\n", hdr->eip_object);
        return 0;
    }
    uint32_t entry = obj_bases[hdr->eip_object - 1] + hdr->eip_offset;

    // Module-specified initial stack (optional)
    if (esp_out && hdr->esp_object != 0 && hdr->esp_object <= hdr->obj_count) {
        *esp_out = obj_bases[hdr->esp_object - 1] + hdr->esp_offset;
    }

    return entry;
}

// Create a process from an LX executable image
uint32_t lx_exec(const char* name, const char* args,
                 const void* data, uint32_t size) {
    printf("[LX] Loading OS/2 executable '%s' (%d bytes)\n", name, size);

    const lx_header_t* hdr = lx_find_header((const uint8_t*)data, size);
    if (!hdr) {
        printf("[LX] ERROR: Not an LX executable\n");
        return 0;
    }

    printf("[LX] LX module: %d objects, %d pages, entry obj %d\n",
           hdr->obj_count, hdr->module_pages, hdr->eip_object);

    process_t* proc = process_alloc_slot();
    if (!proc) {
        printf("[LX] ERROR: Process table full\n");
        return 0;
    }
    uint32_t pid = process_next_pid();
    proc->pid = pid;
    proc->state = PROCESS_STATE_READY;
    proc->priority = PRIORITY_REGULAR;
    str_copy_safe(proc->name, name, 32);
    proc->parent_pid = process_current() ? process_current()->pid : 0;
    proc->time_slice = 10;
    proc->total_time = 0;
    proc->exit_code = 0;
    proc->child_count = 0;

    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        proc->allocations[i].in_use = 0;
    }
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        proc->file_descriptors[i].in_use = 0;
        proc->file_descriptors[i].node = NULL;
    }

    uint32_t page_directory = vmm_create_page_directory();
    if (page_directory == 0) {
        printf("[LX] ERROR: Failed to create page directory\n");
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }
    proc->page_directory = page_directory;

    uint32_t module_esp = 0;
    uint32_t entry = lx_load_image((const uint8_t*)data, size, hdr,
                                   page_directory, &module_esp);
    if (entry == 0) {
        vmm_destroy_page_directory(page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Kernel stack
    uint32_t kernel_stack_phys = pmm_alloc_pages(KERNEL_STACK_PAGES);
    if (kernel_stack_phys == 0) {
        printf("[LX] ERROR: Failed to allocate kernel stack\n");
        vmm_destroy_page_directory(page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }
    proc->kernel_stack = kernel_stack_phys + KERNEL_STACK_SIZE;

    // Default user stack (used when the module doesn't specify one)
    uint32_t user_stack_virt = 0xBFFFF000;
    uint32_t user_stack_phys = pmm_alloc_page();
    if (user_stack_phys == 0) {
        printf("[LX] ERROR: Failed to allocate user stack\n");
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    uint32_t old_pd = vmm_get_current_directory();
    vmm_switch_page_directory(page_directory);
    if (!vmm_map_page(user_stack_virt, user_stack_phys,
                      PTE_PRESENT | PTE_WRITABLE | PTE_USER)) {
        printf("[LX] ERROR: Failed to map user stack\n");
        vmm_switch_page_directory(old_pd);
        pmm_free_page(user_stack_phys);
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Build the OS/2 process startup frame on the initial stack:
    //   [ESP+0]  module handle
    //   [ESP+4]  reserved (0)
    //   [ESP+8]  environment block pointer
    //   [ESP+12] command line pointer (program name, arguments after its NUL)
    // The command line must directly follow the environment block in
    // memory: the C runtime locates the program name by scanning
    // backward from the command line pointer.
    uint32_t stack_top = module_esp ? module_esp
                                    : (user_stack_virt + PAGE_SIZE - 4);
    // OS/2 info blocks (TIB/PIB/TIB2) live at the top of the stack page,
    // above the argument/environment strings (read-only to the program).
    // DosGetInfoBlocks returns these; the C runtime reads the command
    // line and environment pointers out of the PIB to build argv/environ.
    uint32_t pib_addr  = (stack_top - 28) & ~3u;
    uint32_t tib_addr  = (pib_addr  - 24) & ~3u;
    uint32_t tib2_addr = (tib_addr  - 16) & ~3u;
    stack_top = tib2_addr;
    {
        static const char env_block[] = "PATH=C:\\\0COMSPEC=C:\\OS2\\CMD.EXE";
        uint32_t name_len = 0;
        while (name[name_len]) name_len++;
        uint32_t args_len = 0;
        if (args) {
            while (args[args_len]) args_len++;
        }

        uint32_t env_size = sizeof(env_block) + 1;   // strings + final NUL
        // OS/2 layout after the environment: the fully qualified
        // program path ("C:\NAME\0"), then the command line
        // ("name\0args\0"). Runtimes find argv[0] by scanning backward
        // from the command line pointer to the path string.
        uint32_t path_len = 3 + name_len;            // "C:\" + name
        uint32_t blk_size = env_size + path_len + 1 +
                            name_len + 1 + args_len + 1;
        uint32_t blk_addr = (stack_top - blk_size) & ~3u;

        uint8_t* blk = (uint8_t*)blk_addr;
        for (uint32_t i = 0; i < sizeof(env_block); i++) {
            blk[i] = env_block[i];
        }
        blk[sizeof(env_block)] = 0;                  // end of environment

        uint8_t* path = blk + env_size;
        path[0] = 'C'; path[1] = ':'; path[2] = '\\';
        for (uint32_t i = 0; i < name_len; i++) {
            path[3 + i] = name[i];
        }
        path[path_len] = 0;

        uint8_t* cmd = path + path_len + 1;
        for (uint32_t i = 0; i < name_len; i++) {
            cmd[i] = name[i];
        }
        cmd[name_len] = 0;
        for (uint32_t i = 0; i < args_len; i++) {
            cmd[name_len + 1 + i] = args[i];
        }
        cmd[name_len + 1 + args_len] = 0;

        uint32_t env_addr = blk_addr;
        uint32_t cmd_addr = blk_addr + env_size + path_len + 1;

        uint32_t frame = (blk_addr - 16) & ~3u;
        uint32_t* f = (uint32_t*)frame;
        f[0] = 1;                                    // module handle
        f[1] = 0;                                    // reserved
        f[2] = env_addr;                             // environment
        f[3] = cmd_addr;                             // command line
        stack_top = frame;

        // Fill the OS/2 Process/Thread Information Blocks.
        uint32_t* pib = (uint32_t*)pib_addr;
        pib[0] = pid;                                // pib_ulpid
        pib[1] = 0;                                  // pib_ulppid
        pib[2] = 1;                                  // pib_hmte
        pib[3] = cmd_addr;                           // pib_pchcmd
        pib[4] = env_addr;                           // pib_pchenv
        pib[5] = 0;                                  // pib_flstatus
        pib[6] = 1;                                  // pib_ultype
        uint32_t* tib = (uint32_t*)tib_addr;
        tib[0] = 0xFFFFFFFF;                         // tib_pexchain (end)
        tib[1] = user_stack_virt;                    // tib_pstack (low)
        tib[2] = user_stack_virt + PAGE_SIZE;        // tib_pstacklimit (high)
        tib[3] = tib2_addr;                          // tib_ptib2
        tib[4] = 0;                                  // tib_version
        tib[5] = 0;                                  // tib_ordinal
        uint32_t* tib2 = (uint32_t*)tib2_addr;
        tib2[0] = 1;                                 // tib2_ultid
        tib2[1] = 0x0200;                            // tib2_ulpri
        tib2[2] = 0;                                 // tib2_version
        tib2[3] = 0;                                 // MCCount/ForceFlag

        lx_pib_addr = pib_addr;
        lx_tib_addr = tib_addr;
    }
    vmm_switch_page_directory(old_pd);

    proc->user_stack = stack_top;
    proc->eip = entry;
    proc->esp = proc->user_stack;
    proc->ebp = proc->user_stack;

    proc->cs = 0x1B;
    proc->ds = proc->es = proc->fs = proc->gs = proc->ss = 0x23;
    proc->eflags = 0x0202;

    proc->eax = proc->ebx = proc->ecx = proc->edx = 0;
    proc->esi = proc->edi = 0;

    printf("[LX] Created OS/2 process '%s' (PID %d) entry=0x%x esp=0x%x\n",
           name, pid, entry, proc->esp);
    return pid;
}
