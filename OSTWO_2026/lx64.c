// File: lx64.c
// OS/2 LX loader for the 64-bit kernel's 32-bit compatibility mode.
//
// A focused port of lx.c: parses an LX module, loads its objects at
// their preferred base into low RAM (the first 2MB, granted user
// access), resolves DOSCALLS imports to fixed-slot int-0x80 thunks and
// applies internal + import fixups. Enough to run the hand-built OS/2
// test executables in compat mode under the 64-bit kernel.

#include <stdint.h>
#include "lx.h"
#include "thunk32.h"

extern void sputs(const char* s);
extern void sputhex64(uint64_t v);

// Compat-mode load layout, all inside the user-granted first 2MB:
//   thunks at 0x30000, app objects at their preferred base (0x10000+)
#define LX64_THUNK_BASE  0x30000u
#define LX64_SLOT_SIZE   32u

// DOSCALLS ordinal -> thunk slot index (matches thunk32.asm order)
static int lx64_slot(uint32_t ordinal) {
    switch (ordinal) {
        case 282: return 0;   // DosWrite
        case 234: return 1;   // DosExit
        case 286: return 2;   // DosBeep
        case 229: return 3;   // DosSleep
        case 281: return 4;   // DosRead
        default:  return -1;
    }
}

static const lx_header_t* lx64_find_header(const uint8_t* d, uint32_t size) {
    if (size < sizeof(lx_header_t)) return 0;
    uint32_t off = 0;
    if (d[0] == 'M' && d[1] == 'Z') {
        if (size < 0x40) return 0;
        off = *(const uint32_t*)(d + 0x3C);
        if (off == 0 || off + sizeof(lx_header_t) > size) return 0;
    }
    const lx_header_t* h = (const lx_header_t*)(d + off);
    if (h->magic[0] != 'L' || h->magic[1] != 'X') return 0;
    return h;
}

int lx64_is_lx(const void* d, uint32_t size) {
    return lx64_find_header((const uint8_t*)d, size) != 0;
}

static int lx64_name_is(const char* n, uint8_t len, const char* lit) {
    for (uint8_t i = 0; i < len; i++) {
        char c = n[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (lit[i] == 0 || c != lit[i]) return 0;
    }
    return lit[len] == 0;
}

static uint32_t lx64_resolve_import(const lx_header_t* h, uint32_t mod, uint32_t ord) {
    const uint8_t* p = (const uint8_t*)h + h->import_mod_off;
    const char* name = 0;
    uint8_t len = 0;
    for (uint32_t m = 1; m <= h->import_mod_count; m++) {
        len = *p;
        if (m == mod) { name = (const char*)(p + 1); break; }
        p += 1 + len;
    }
    if (!name || !lx64_name_is(name, len, "DOSCALLS")) return 0;
    int slot = lx64_slot(ord);
    if (slot < 0) {
        sputs("[64] LX: unimplemented DOSCALLS ordinal\n");
        return 0;
    }
    return LX64_THUNK_BASE + (uint32_t)slot * LX64_SLOT_SIZE;
}

// Apply fixups for one logical page (compat-mode subset: source types
// 0x07 32-bit offset and 0x08 32-bit self-relative; internal + import
// targets). Writes directly into the identity-mapped low pages.
static int lx64_fixups(const lx_header_t* h, uint32_t page, uint32_t page_va,
                       const uint32_t* bases, uint32_t nobj) {
    const uint8_t* base = (const uint8_t*)h;
    const uint32_t* fpt = (const uint32_t*)(base + h->fixup_page_off);
    const uint8_t* rec = base + h->fixup_rec_off;
    const uint8_t* p = rec + fpt[page];
    const uint8_t* end = rec + fpt[page + 1];

    while (p < end) {
        uint8_t src = *p++;
        uint8_t flags = *p++;
        uint8_t stype = src & LX_FIXUP_SRC_MASK;

        int16_t soff = 0;
        uint8_t count = 1;
        const uint8_t* slist = 0;
        if (src & LX_FIXUP_SRC_LIST) count = *p++;
        else { soff = (int16_t)(p[0] | (p[1] << 8)); p += 2; }

        uint32_t target = 0;
        uint8_t ttype = flags & LX_FIXUP_TGT_MASK;
        if (ttype == LX_FIXUP_TGT_INTERNAL) {
            uint32_t obj = (flags & LX_FIXUP_MOD16) ? (p[0] | (p[1] << 8)) : *p;
            p += (flags & LX_FIXUP_MOD16) ? 2 : 1;
            uint32_t toff = 0;
            if (stype != 0x02) {
                if (flags & LX_FIXUP_TGTOFF32) { toff = *(const uint32_t*)p; p += 4; }
                else { toff = p[0] | (p[1] << 8); p += 2; }
            }
            if (obj == 0 || obj > nobj) return -1;
            target = bases[obj - 1] + toff;
        } else if (ttype == LX_FIXUP_TGT_IMPORD) {
            uint32_t mod = (flags & LX_FIXUP_MOD16) ? (p[0] | (p[1] << 8)) : *p;
            p += (flags & LX_FIXUP_MOD16) ? 2 : 1;
            uint32_t ord;
            if (flags & LX_FIXUP_ORD8) { ord = *p; p += 1; }
            else if (flags & LX_FIXUP_TGTOFF32) { ord = *(const uint32_t*)p; p += 4; }
            else { ord = p[0] | (p[1] << 8); p += 2; }
            target = lx64_resolve_import(h, mod, ord);
            if (target == 0) return -1;
        } else {
            return -1;
        }

        uint32_t add = 0;
        if (flags & LX_FIXUP_ADDITIVE) {
            if (flags & LX_FIXUP_ADD32) { add = *(const uint32_t*)p; p += 4; }
            else { add = p[0] | (p[1] << 8); p += 2; }
        }
        if (src & LX_FIXUP_SRC_LIST) { slist = p; p += count * 2; }

        for (uint8_t s = 0; s < count; s++) {
            int16_t off = slist ? (int16_t)(slist[s*2] | (slist[s*2+1] << 8)) : soff;
            uint32_t site = page_va + off;
            if (stype == LX_FIXUP_SRC_OFF32) {
                *(uint32_t*)(uint64_t)site = target + add;
            } else if (stype == LX_FIXUP_SRC_REL32) {
                *(uint32_t*)(uint64_t)site = (target + add) - (site + 4);
            } else {
                return -1;
            }
        }
    }
    return 0;
}

// Load an LX module into low RAM; returns the 32-bit entry point (EIP)
// or 0 on failure. Assumes the first 2MB is identity-mapped and has
// been granted ring-3 access by the caller.
uint32_t lx64_load(const uint8_t* data, uint32_t size) {
    const lx_header_t* h = lx64_find_header(data, size);
    if (!h) { sputs("[64] LX: bad header\n"); return 0; }
    if (h->page_size != 4096 || h->obj_count == 0 || h->obj_count > 8) {
        sputs("[64] LX: unsupported layout\n");
        return 0;
    }

    const uint8_t* lx = (const uint8_t*)h;
    const lx_object_t* objs = (const lx_object_t*)(lx + h->obj_table_off);
    const lx_page_t* pages = (const lx_page_t*)(lx + h->obj_page_off);

    uint32_t bases[8];
    for (uint32_t i = 0; i < h->obj_count; i++) {
        bases[i] = objs[i].reloc_base;   // preferred base (0x10000+)
        if (bases[i] < 0x10000 || bases[i] + objs[i].virtual_size > 0x200000) {
            sputs("[64] LX: object outside compat low window\n");
            return 0;
        }
    }

    // Copy the DOSCALLS thunk blob to its base
    uint8_t* thunks = (uint8_t*)(uint64_t)LX64_THUNK_BASE;
    for (uint32_t i = 0; i < thunk32_blob_len; i++) thunks[i] = thunk32_blob[i];

    // Load object pages
    for (uint32_t i = 0; i < h->obj_count; i++) {
        const lx_object_t* o = &objs[i];
        uint32_t np = (o->virtual_size + 4095) / 4096;
        // zero the object
        uint8_t* ob = (uint8_t*)(uint64_t)bases[i];
        for (uint32_t j = 0; j < np * 4096; j++) ob[j] = 0;

        for (uint32_t pgi = 0; pgi < o->page_count && pgi < np; pgi++) {
            const lx_page_t* pg = &pages[o->page_table_index - 1 + pgi];
            if (pg->flags == LX_PAGE_ZEROED) continue;
            if (pg->flags != LX_PAGE_VALID) {
                sputs("[64] LX: compressed pages unsupported in compat loader\n");
                return 0;
            }
            uint32_t foff = h->data_pages_off + (pg->data_offset << h->page_shift);
            if (foff + pg->data_size > size) return 0;
            uint8_t* dst = (uint8_t*)(uint64_t)(bases[i] + pgi * 4096);
            for (uint32_t j = 0; j < pg->data_size; j++) dst[j] = data[foff + j];
        }
    }

    // Apply fixups per logical page
    for (uint32_t i = 0; i < h->obj_count; i++) {
        const lx_object_t* o = &objs[i];
        for (uint32_t pgi = 0; pgi < o->page_count; pgi++) {
            uint32_t logical = o->page_table_index - 1 + pgi;
            uint32_t va = bases[i] + pgi * 4096;
            if (lx64_fixups(h, logical, va, bases, h->obj_count) != 0) {
                sputs("[64] LX: fixup failed\n");
                return 0;
            }
        }
    }

    if (h->eip_object == 0 || h->eip_object > h->obj_count) return 0;
    return bases[h->eip_object - 1] + h->eip_offset;
}
