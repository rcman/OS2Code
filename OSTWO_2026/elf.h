// File: elf.h
// ELF (Executable and Linkable Format) definitions for Linux binary compatibility

#ifndef ELF_H
#define ELF_H

#include "types.h"

// ELF identification indexes
#define EI_MAG0         0   // File identification
#define EI_MAG1         1   // File identification
#define EI_MAG2         2   // File identification
#define EI_MAG3         3   // File identification
#define EI_CLASS        4   // File class
#define EI_DATA         5   // Data encoding
#define EI_VERSION      6   // File version
#define EI_OSABI        7   // OS/ABI identification
#define EI_ABIVERSION   8   // ABI version
#define EI_PAD          9   // Start of padding bytes
#define EI_NIDENT       16  // Size of e_ident[]

// ELF magic number
#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'

// ELF class
#define ELFCLASSNONE    0   // Invalid class
#define ELFCLASS32      1   // 32-bit objects
#define ELFCLASS64      2   // 64-bit objects

// ELF data encoding
#define ELFDATANONE     0   // Invalid data encoding
#define ELFDATA2LSB     1   // Little-endian
#define ELFDATA2MSB     2   // Big-endian

// ELF version
#define EV_NONE         0   // Invalid version
#define EV_CURRENT      1   // Current version

// ELF OS/ABI
#define ELFOSABI_NONE       0   // UNIX System V ABI
#define ELFOSABI_LINUX      3   // Linux
#define ELFOSABI_STANDALONE 255 // Standalone (embedded)

// ELF file types
#define ET_NONE         0   // No file type
#define ET_REL          1   // Relocatable file
#define ET_EXEC         2   // Executable file
#define ET_DYN          3   // Shared object file
#define ET_CORE         4   // Core file

// ELF machine types
#define EM_NONE         0   // No machine
#define EM_386          3   // Intel 80386
#define EM_X86_64       62  // AMD x86-64

// Program header types
#define PT_NULL         0   // Program header table entry unused
#define PT_LOAD         1   // Loadable program segment
#define PT_DYNAMIC      2   // Dynamic linking information
#define PT_INTERP       3   // Program interpreter
#define PT_NOTE         4   // Auxiliary information
#define PT_SHLIB        5   // Reserved
#define PT_PHDR         6   // Entry for header table itself
#define PT_TLS          7   // Thread-local storage segment
#define PT_GNU_STACK    0x6474e551  // Indicates stack executability

// Program header flags
#define PF_X            0x1 // Segment is executable
#define PF_W            0x2 // Segment is writable
#define PF_R            0x4 // Segment is readable

// Section header types
#define SHT_NULL        0   // Section header table entry unused
#define SHT_PROGBITS    1   // Program data
#define SHT_SYMTAB      2   // Symbol table
#define SHT_STRTAB      3   // String table
#define SHT_RELA        4   // Relocation entries with addends
#define SHT_HASH        5   // Symbol hash table
#define SHT_DYNAMIC     6   // Dynamic linking information
#define SHT_NOTE        7   // Notes
#define SHT_NOBITS      8   // Program space with no data (bss)
#define SHT_REL         9   // Relocation entries, no addends
#define SHT_SHLIB       10  // Reserved
#define SHT_DYNSYM      11  // Dynamic linker symbol table

// Section header flags
#define SHF_WRITE       0x1     // Writable
#define SHF_ALLOC       0x2     // Occupies memory during execution
#define SHF_EXECINSTR   0x4     // Executable

// Dynamic array tags
#define DT_NULL         0   // Marks end of dynamic section
#define DT_NEEDED       1   // Name of needed library
#define DT_PLTRELSZ     2   // Size in bytes of PLT relocs
#define DT_PLTGOT       3   // Processor defined value
#define DT_HASH         4   // Address of symbol hash table
#define DT_STRTAB       5   // Address of string table
#define DT_SYMTAB       6   // Address of symbol table
#define DT_RELA         7   // Address of Rela relocs
#define DT_RELASZ       8   // Total size of Rela relocs
#define DT_RELAENT      9   // Size of one Rela reloc
#define DT_STRSZ        10  // Size of string table
#define DT_SYMENT       11  // Size of one symbol table entry
#define DT_INIT         12  // Address of init function
#define DT_FINI         13  // Address of termination function

// ELF32 data types
typedef uint32_t Elf32_Addr;    // Unsigned program address
typedef uint16_t Elf32_Half;    // Unsigned medium integer
typedef uint32_t Elf32_Off;     // Unsigned file offset
typedef int32_t  Elf32_Sword;   // Signed large integer
typedef uint32_t Elf32_Word;    // Unsigned large integer

// ELF32 Header (52 bytes)
typedef struct {
    uint8_t     e_ident[EI_NIDENT]; // Magic number and other info
    Elf32_Half  e_type;             // Object file type
    Elf32_Half  e_machine;          // Architecture
    Elf32_Word  e_version;          // Object file version
    Elf32_Addr  e_entry;            // Entry point virtual address
    Elf32_Off   e_phoff;            // Program header table file offset
    Elf32_Off   e_shoff;            // Section header table file offset
    Elf32_Word  e_flags;            // Processor-specific flags
    Elf32_Half  e_ehsize;           // ELF header size in bytes
    Elf32_Half  e_phentsize;        // Program header table entry size
    Elf32_Half  e_phnum;            // Program header table entry count
    Elf32_Half  e_shentsize;        // Section header table entry size
    Elf32_Half  e_shnum;            // Section header table entry count
    Elf32_Half  e_shstrndx;         // Section header string table index
} Elf32_Ehdr;

// ELF32 Program Header (32 bytes)
typedef struct {
    Elf32_Word  p_type;     // Segment type
    Elf32_Off   p_offset;   // Segment file offset
    Elf32_Addr  p_vaddr;    // Segment virtual address
    Elf32_Addr  p_paddr;    // Segment physical address
    Elf32_Word  p_filesz;   // Segment size in file
    Elf32_Word  p_memsz;    // Segment size in memory
    Elf32_Word  p_flags;    // Segment flags
    Elf32_Word  p_align;    // Segment alignment
} Elf32_Phdr;

// ELF32 Section Header (40 bytes)
typedef struct {
    Elf32_Word  sh_name;        // Section name (string tbl index)
    Elf32_Word  sh_type;        // Section type
    Elf32_Word  sh_flags;       // Section flags
    Elf32_Addr  sh_addr;        // Section virtual addr at execution
    Elf32_Off   sh_offset;      // Section file offset
    Elf32_Word  sh_size;        // Section size in bytes
    Elf32_Word  sh_link;        // Link to another section
    Elf32_Word  sh_info;        // Additional section information
    Elf32_Word  sh_addralign;   // Section alignment
    Elf32_Word  sh_entsize;     // Entry size if section holds table
} Elf32_Shdr;

// ELF32 Symbol Table Entry
typedef struct {
    Elf32_Word  st_name;    // Symbol name (string tbl index)
    Elf32_Addr  st_value;   // Symbol value
    Elf32_Word  st_size;    // Symbol size
    uint8_t     st_info;    // Symbol type and binding
    uint8_t     st_other;   // Symbol visibility
    Elf32_Half  st_shndx;   // Section index
} Elf32_Sym;

// ELF32 Dynamic Structure
typedef struct {
    Elf32_Sword d_tag;      // Dynamic entry type
    union {
        Elf32_Word d_val;   // Integer value
        Elf32_Addr d_ptr;   // Address value
    } d_un;
} Elf32_Dyn;

// ELF Loader functions

// Validate ELF header
// Returns: 0 on success, -1 on error
int elf_validate_header(const Elf32_Ehdr* ehdr);

// Load ELF binary from memory into process address space
// Returns: entry point address on success, 0 on failure
uint32_t elf_load(const void* elf_data, uint32_t size, uint32_t* entry_point);

// Create process from ELF binary
// Returns: PID on success, 0 on failure
uint32_t elf_exec(const char* name, const void* elf_data, uint32_t size);

#endif // ELF_H
