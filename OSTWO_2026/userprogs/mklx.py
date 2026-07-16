#!/usr/bin/env python3
"""Build OS/2 LX-format test executables for OSTwo.

hello_os2.exe - plain (VALID) pages; DosBeep/DosWrite/DosExit imports
packed.exe    - data pages compressed as ITERDATA2 (EXEPACK2) and
                ITERDATA (iteration records), exercising the loader's
                decompression paths

Also emits *_bin.h headers for embedding into the kernel's RamFS.
"""
import struct

PAGE = 4096
LX_HDR_SIZE = 196
MZ_SIZE = 0x40

# Page types
PG_VALID = 0
PG_ITERDATA = 1
PG_ZEROED = 3
PG_ITERDATA2 = 5

# DOSCALLS ordinals
ORD_DOSSLEEP = 229
ORD_DOSEXIT = 234
ORD_DOSWRITE = 282
ORD_DOSBEEP = 286


# ---------------------------------------------------------------- compressors
def pack_exepack2(data):
    """EXEPACK2-compress using only type-0 records (literals + byte runs)."""
    out = bytearray()
    i = 0
    while i < len(data):
        # Find a run of identical bytes
        run = 1
        while i + run < len(data) and data[i + run] == data[i] and run < 255:
            run += 1
        if run >= 4:
            out += bytes([0, run, data[i]])
            i += run
            continue
        # Literal block (up to 63 bytes, stopping at the next long run)
        j = i
        while j < len(data) and j - i < 63:
            r = 1
            while j + r < len(data) and data[j + r] == data[j] and r < 255:
                r += 1
            if r >= 4:
                break
            j += r
        lit = data[i:j] if j > i else data[i:i + 1]
        out += bytes([len(lit) << 2]) + lit
        i += len(lit)
    out += bytes([0, 0])  # end marker
    return bytes(out)


def pack_iterdata(records):
    """ITERDATA page from explicit (iterations, block) records."""
    out = bytearray()
    for iterations, block in records:
        out += struct.pack('<HH', iterations, len(block)) + block
    return bytes(out)


# ---------------------------------------------------------------- code gen
class Code:
    def __init__(self):
        self.buf = bytearray()
        self.fixups = []  # (offset, kind, target)

    def push_imm32(self, val):
        self.buf.append(0x68)
        self.buf.extend(struct.pack('<I', val))

    def push_addr(self, obj, off):
        self.buf.append(0x68)
        self.fixups.append((len(self.buf), 'int', (obj, off)))
        self.buf.extend(b'\0\0\0\0')

    def push_imm8(self, val):
        self.buf.extend(bytes([0x6A, val]))

    def call_import(self, ordinal):
        self.buf.append(0xE8)
        self.fixups.append((len(self.buf), 'imp', ordinal))
        self.buf.extend(b'\0\0\0\0')

    def add_esp(self, n):
        self.buf.extend(bytes([0x83, 0xC4, n]))

    def dos_write(self, msg_obj, msg_off, msg_len, written_obj, written_off):
        self.push_addr(written_obj, written_off)
        self.push_imm32(msg_len)
        self.push_addr(msg_obj, msg_off)
        self.push_imm8(1)
        self.call_import(ORD_DOSWRITE)
        self.add_esp(16)

    def dos_exit(self):
        self.push_imm8(0)
        self.push_imm8(1)
        self.call_import(ORD_DOSEXIT)


def fixup_records(fixups):
    recs = bytearray()
    for site, kind, target in fixups:
        if kind == 'imp':
            recs += struct.pack('<BBhBH', 0x08, 0x01, site, 1, target)
        else:
            obj, off = target
            recs += struct.pack('<BBhBH', 0x07, 0x00, site, obj, off)
    return bytes(recs)


# ---------------------------------------------------------------- LX builder
def build_lx(objects, page0_fixups):
    """objects: list of (base, obj_flags, vsize, [(page_type, page_data)])."""
    obj_table = b''
    page_table = b''
    page_datas = []
    data_cursor = 0
    logical_pages = 0

    for base, oflags, vsize, pages in objects:
        obj_table += struct.pack('<6I', vsize, base, oflags,
                                 logical_pages + 1, len(pages), 0)
        for ptype, pdata in pages:
            page_table += struct.pack('<IHH', data_cursor, len(pdata), ptype)
            page_datas.append(pdata)
            data_cursor += len(pdata)
            logical_pages += 1

    import_mod = bytes([len(b'DOSCALLS')]) + b'DOSCALLS'
    import_proc = b'\x00'
    recs = fixup_records(page0_fixups)
    fixup_page_table = struct.pack('<I', 0)
    fixup_page_table += struct.pack('<I', len(recs)) * logical_pages

    off = LX_HDR_SIZE
    obj_table_off = off;    off += len(obj_table)
    obj_page_off = off;     off += len(page_table)
    import_mod_off = off;   off += len(import_mod)
    import_proc_off = off;  off += len(import_proc)
    fixup_page_off = off;   off += len(fixup_page_table)
    fixup_rec_off = off;    off += len(recs)
    data_pages_off = MZ_SIZE + off

    hdr = struct.pack(
        '<2sBBIHH' + 'I' * 40,
        b'LX', 0, 0, 0, 2, 1,
        0, 0x200, logical_pages,
        1, 0,                   # eip object, offset
        0, 0,                   # esp object, offset
        PAGE, 0,                # page size, shift
        len(fixup_page_table) + len(recs), 0,
        fixup_rec_off - obj_table_off + len(recs), 0,
        obj_table_off, len(objects),
        obj_page_off, 0,
        0, 0, 0, 0, 0, 0,
        fixup_page_off, fixup_rec_off,
        import_mod_off, 1,
        import_proc_off, 0,
        data_pages_off, 0,
        0, 0, 0,
        2,
        0, 0, 0, 0, 0)
    hdr += b'\0' * (LX_HDR_SIZE - len(hdr))
    assert len(hdr) == LX_HDR_SIZE

    mz = bytearray(MZ_SIZE)
    mz[0:2] = b'MZ'
    struct.pack_into('<I', mz, 0x3C, MZ_SIZE)

    return bytes(mz) + hdr + obj_table + page_table + import_mod \
        + import_proc + fixup_page_table + recs + b''.join(page_datas)


def emit_header(image, name, path):
    with open(path, 'w') as f:
        f.write('// Generated by userprogs/mklx.py\n')
        f.write('static const unsigned char %s[] = {\n' % name)
        for i in range(0, len(image), 12):
            f.write('    ' + ', '.join('0x%02x' % b for b in image[i:i+12]) + ',\n')
        f.write('};\n')
        f.write('static const unsigned int %s_len = %d;\n' % (name, len(image)))


# ---------------------------------------------------------------- hello_os2.exe
MSG = b"Hello from a real OS/2 LX executable!\r\nDosWrite via DOSCALLS.282\r\n"
WRITTEN_OFF = (len(MSG) + 3) & ~3

c = Code()
c.push_imm32(150)
c.push_imm32(750)
c.call_import(ORD_DOSBEEP)
c.add_esp(8)
c.dos_write(2, 0, len(MSG), 2, WRITTEN_OFF)
c.dos_exit()

img = build_lx(
    [(0x10000, 0x0005, len(c.buf), [(PG_VALID, bytes(c.buf))]),
     (0x20000, 0x0003, WRITTEN_OFF + 4, [(PG_VALID, MSG)])],
    c.fixups)
open('hello_os2.exe', 'wb').write(img)
emit_header(img, 'hello_os2_bin', '../hello_os2_bin.h')
print('hello_os2.exe: %d bytes' % len(img))

# ---------------------------------------------------------------- packed.exe
# Object 2: ITERDATA2-compressed page (long '=' runs compress well)
MSG2 = (b"ITERDATA2 (EXEPACK2) page decompressed OK!\r\n"
        + b"=" * 60 + b"\r\n"
        + b"Long runs and literals both survived the trip.\r\n")
W2_OFF = (len(MSG2) + 3) & ~3

# Object 3: ITERDATA page from iteration records
MSG3_BLOCK = b"<OS/2!>"
MSG3_REPS = 10
MSG3_TAIL = b"\r\nITERDATA iteration records expanded OK!\r\n"
MSG3 = MSG3_BLOCK * MSG3_REPS + MSG3_TAIL
iter_page = pack_iterdata([(MSG3_REPS, MSG3_BLOCK), (1, MSG3_TAIL)])

packed2 = pack_exepack2(MSG2)
assert len(packed2) < len(MSG2), "compression should shrink the page"

c2 = Code()
c2.dos_write(2, 0, len(MSG2), 2, W2_OFF)
c2.dos_write(3, 0, len(MSG3), 2, W2_OFF)
c2.dos_exit()

img2 = build_lx(
    [(0x10000, 0x0005, len(c2.buf), [(PG_VALID, bytes(c2.buf))]),
     (0x20000, 0x0003, W2_OFF + 4, [(PG_ITERDATA2, packed2)]),
     (0x30000, 0x0003, len(MSG3), [(PG_ITERDATA, iter_page)])],
    c2.fixups)
open('packed.exe', 'wb').write(img2)
emit_header(img2, 'packed_bin', '../packed_bin.h')
print('packed.exe: %d bytes (msg2 %d -> %d compressed, msg3 %d -> %d records)'
      % (len(img2), len(MSG2), len(packed2), len(MSG3), len(iter_page)))
