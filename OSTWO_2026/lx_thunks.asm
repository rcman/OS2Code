; File: lx_thunks.asm
; User-mode API thunks for LX (OS/2) executables.
;
; The LX loader copies this blob into a user page at LX_THUNK_BASE and
; resolves DOSCALLS/NLS import fixups to these stubs. Each stub
; translates the OS/2 _System calling convention (args pushed
; right-to-left, caller cleans up, APIRET in EAX, 0 = NO_ERROR;
; EBX/ESI/EDI/EBP are callee-saved) into an OSTwo int 0x80 syscall
; (EAX = number, args in EBX/ECX/EDX/ESI).
;
; IMPORTANT: EBX and ESI must be preserved - compiled OS/2 code keeps
; live values in them across API calls.
;
; The code must be position-independent: no absolute references.

[BITS 32]

section .data

global lx_thunk_blob_start
global lx_thunk_blob_end
global lx_thunk_table
global lx_thunk_count

lx_thunk_blob_start:

; Fallback for any imported API without an implementation. Loading
; succeeds (real apps import much more than they call); actually
; calling one of these fails cleanly.
thunk_unimplemented:
    mov eax, 1                  ; ERROR_INVALID_FUNCTION
    ret

; APIRET DosWrite(HFILE handle, PVOID buf, ULONG len, PULONG written)  [DOSCALLS.282]
; Handles 0-2 go to the console; 3+ to the file system.
thunk_doswrite:
    push ebx
    mov ebx, [esp + 8]          ; handle
    mov ecx, [esp + 12]         ; buffer
    mov edx, [esp + 16]         ; length
    cmp ebx, 3
    jae .file
    mov eax, 2                  ; SYSCALL_WRITE (console)
    int 0x80                    ; returns bytes written
    mov ecx, [esp + 20]         ; PULONG written (may be NULL)
    jecxz .done
    mov [ecx], eax
.done:
    pop ebx
    xor eax, eax                ; NO_ERROR
    ret
.file:
    push esi
    mov esi, [esp + 24]         ; PULONG written (two registers pushed)
    mov eax, 29                 ; SYSCALL_FILEWRITE (kernel fills *written)
    int 0x80
    pop esi
    pop ebx
    ret                         ; APIRET from kernel

; VOID DosExit(ULONG action, ULONG result)  [DOSCALLS.234]
thunk_dosexit:
    mov ebx, [esp + 8]          ; result code (no return: no need to save)
    mov eax, 1                  ; SYSCALL_EXIT
    int 0x80
.hang:
    jmp .hang

; APIRET DosSleep(ULONG msec)  [DOSCALLS.229]
thunk_dossleep:
    push ebx
    mov ebx, [esp + 8]
    mov eax, 7                  ; SYSCALL_SLEEP
    int 0x80
    pop ebx
    xor eax, eax
    ret

; APIRET DosBeep(ULONG freq, ULONG dur)  [DOSCALLS.286]
thunk_dosbeep:
    push ebx
    mov ebx, [esp + 8]
    mov ecx, [esp + 12]
    mov eax, 8                  ; SYSCALL_BEEP
    int 0x80
    pop ebx
    xor eax, eax
    ret

; APIRET DosRead(HFILE handle, PVOID buf, ULONG len, PULONG read)  [DOSCALLS.281]
thunk_dosread:
    push ebx
    mov ebx, [esp + 8]
    mov ecx, [esp + 12]
    mov edx, [esp + 16]
    cmp ebx, 3
    jae .file
    mov eax, 3                  ; SYSCALL_READ (console)
    int 0x80
    mov ecx, [esp + 20]
    jecxz .done
    mov [ecx], eax
.done:
    pop ebx
    xor eax, eax
    ret
.file:
    push esi
    mov esi, [esp + 24]         ; PULONG read
    mov eax, 28                 ; SYSCALL_FILEREAD (kernel fills *read)
    int 0x80
    pop esi
    pop ebx
    ret

; APIRET DosAllocMem(PPVOID base, ULONG size, ULONG flags)  [DOSCALLS.299]
thunk_dosallocmem:
    push ebx
    mov ebx, [esp + 12]         ; size in bytes
    mov eax, 15                 ; SYSCALL_ALLOCMEM
    int 0x80                    ; returns virtual address or 0
    pop ebx
    test eax, eax
    jz .fail
    mov ecx, [esp + 4]          ; PPVOID base
    mov [ecx], eax
    xor eax, eax
    ret
.fail:
    mov eax, 8                  ; ERROR_NOT_ENOUGH_MEMORY
    ret

; APIRET DosFreeMem(PVOID base)  [DOSCALLS.304]
thunk_dosfreemem:
    push ebx
    mov ebx, [esp + 8]
    mov eax, 16                 ; SYSCALL_FREEMEM
    int 0x80
    pop ebx
    xor eax, eax
    ret

; APIRET DosGetDateTime(PDATETIME pdt)  [DOSCALLS.230]
thunk_dosgetdatetime:
    push ebx
    mov ebx, [esp + 8]
    mov eax, 9                  ; SYSCALL_GETDATETIME
    int 0x80
    pop ebx
    xor eax, eax
    ret

; APIRET DosQueryHType(HFILE h, PULONG type, PULONG attr)  [DOSCALLS.224]
; Handles 0-2 are the console (character device); 3+ are files.
thunk_dosqueryhtype:
    xor edx, edx                ; HANDTYPE_FILE
    cmp dword [esp + 4], 3
    jae .set
    inc edx                     ; HANDTYPE_DEVICE
.set:
    mov ecx, [esp + 8]
    mov [ecx], edx
    mov ecx, [esp + 12]
    jecxz .done
    mov dword [ecx], 0
.done:
    xor eax, eax
    ret

; APIRET DosDevConfig(PVOID info, ULONG item)  [DOSCALLS.231]
; No printers/coprocessors/etc. to report.
thunk_dosdevconfig:
    mov ecx, [esp + 4]
    mov byte [ecx], 0
    xor eax, eax
    ret

; APIRET DosResetBuffer(HFILE)  [DOSCALLS.254]
; Accept and succeed (no write-back cache to flush).
thunk_dosnoop:
    xor eax, eax
    ret

; APIRET DosClose(HFILE)  [DOSCALLS.257]
; Console handles need no bookkeeping; files close through the VFS.
thunk_dosclose:
    cmp dword [esp + 4], 3
    jb .console
    push ebx
    mov ebx, [esp + 8]
    mov eax, 27                 ; SYSCALL_CLOSE
    int 0x80
    pop ebx
    ret
.console:
    xor eax, eax
    ret

; APIRET DosOpen(PSZ name, PHFILE ph, PULONG action, ULONG cbFile,
;                ULONG attr, ULONG openflags, ULONG openmode, PEAOP2 ea)  [DOSCALLS.273]
; The kernel's O_CREAT (0x10) equals OS/2's OPEN_ACTION_CREATE_IF_NEW,
; so openflags pass through; the access mode is off by one
; (OS/2 0/1/2 = kernel O_RDONLY/O_WRONLY/O_RDWR 1/2/3).
thunk_dosopen:
    push ebx
    push esi
    mov ebx, [esp + 12]         ; name
    mov ecx, [esp + 16]         ; PHFILE
    mov edx, [esp + 32]         ; openflags (action flags)
    mov esi, [esp + 36]         ; openmode
    and esi, 7                  ; access bits only
    inc esi                     ; -> kernel mode
    mov eax, 26                 ; SYSCALL_OPEN
    int 0x80
    pop esi
    pop ebx
    test eax, eax
    jnz .done                   ; propagate OS/2 error code
    mov ecx, [esp + 12]         ; PULONG action
    jecxz .done
    mov dword [ecx], 1          ; FILE_EXISTED (close enough)
.done:
    ret

; APIRET DosSetFilePtr(HFILE, LONG off, ULONG origin, PULONG actual)  [DOSCALLS.256]
; Console handles are not seekable; files seek through the VFS.
thunk_dossetfileptr:
    cmp dword [esp + 4], 3
    jae .file
    mov ecx, [esp + 16]
    jecxz .done
    mov dword [ecx], 0
.done:
    xor eax, eax
    ret
.file:
    push ebx
    push esi
    mov ebx, [esp + 12]         ; handle
    mov ecx, [esp + 16]         ; distance
    mov edx, [esp + 20]         ; origin (0=begin, 1=current, 2=end)
    mov esi, [esp + 24]         ; PULONG new position
    mov eax, 30                 ; SYSCALL_SEEK
    int 0x80
    pop esi
    pop ebx
    ret

; APIRET DosQueryCp(ULONG cb, PULONG codepages, PULONG actual)  [DOSCALLS.291]
; Report codepage 437 (US) as the only entry.
thunk_dosquerycp:
    cmp dword [esp + 4], 4
    jb .small
    mov ecx, [esp + 8]
    mov dword [ecx], 437
    mov ecx, [esp + 12]
    jecxz .done
    mov dword [ecx], 4
.done:
    xor eax, eax
    ret
.small:
    mov eax, 473                ; ERROR_CPLIST_TOO_SMALL
    ret

; APIRET DosQuerySysInfo(ULONG first, ULONG last, PVOID buf, ULONG size)  [DOSCALLS.348]
thunk_dosquerysysinfo:
    push ebx
    push esi
    mov ebx, [esp + 12]         ; first QSV index
    mov ecx, [esp + 16]         ; last QSV index
    mov edx, [esp + 20]         ; buffer
    mov esi, [esp + 24]         ; buffer size
    mov eax, 51                 ; SYSCALL_QUERYSYSINFO
    int 0x80                    ; returns APIRET directly
    pop esi
    pop ebx
    ret

; APIRET DosSetRelMaxFH(PLONG delta, PULONG current)  [DOSCALLS.382]
; Handle table is fixed-size; report the current maximum.
thunk_dossetrelmaxfh:
    mov ecx, [esp + 8]
    jecxz .done
    mov dword [ecx], 20
.done:
    xor eax, eax
    ret

; APIRET DosQueryModuleHandle(PSZ name, PHMODULE ph)  [DOSCALLS.319]
; No DLL loading yet: report module not found (the C runtime uses this
; to probe for optional APIs like DosOpenL and falls back cleanly).
thunk_dosquerymodhandle:
    mov eax, 126                ; ERROR_MOD_NOT_FOUND
    ret

; APIRET DosQueryProcAddr(HMODULE h, ULONG ord, PSZ name, PFN* pfn)  [DOSCALLS.321]
thunk_dosqueryprocaddr:
    mov eax, 127                ; ERROR_PROC_NOT_FOUND
    ret

; SEL DosFlatToSel(PVOID p)  [DOSCALLS.425]
; PVOID DosSelToFlat(ULONG sel16) [DOSCALLS.426]
; No LDT tiling: identity transform keeps dead 16-bit paths harmless.
thunk_dosflattosel:
thunk_dosseltoflat:
    mov eax, [esp + 4]
    ret

; APIRET DosQueryDBCSEnv(ULONG cb, COUNTRYCODE* cc, PCHAR buf)  [NLS.6]
; Not a DBCS system: return an empty lead-byte vector.
thunk_dosquerydbcsenv:
    cmp dword [esp + 4], 2
    jb .small
    mov ecx, [esp + 12]
    mov word [ecx], 0
    xor eax, eax
    ret
.small:
    mov eax, 87                 ; ERROR_INVALID_PARAMETER
    ret

; APIRET DosGetInfoBlocks(PTIB *pptib, PPIB *pppib)  [DOSCALLS.312]
; The C runtime reads the command line / environment out of the PIB.
thunk_dosgetinfoblocks:
    push ebx
    mov ebx, [esp + 8]          ; PTIB* (may be NULL)
    mov ecx, [esp + 12]         ; PPIB* (may be NULL)
    mov eax, 52                 ; SYSCALL_GETINFOBLOCKS -> EBX=*PTIB, ECX=*PPIB
    int 0x80
    pop ebx
    xor eax, eax
    ret

; APIRET DosExitList(ULONG ordercode, PFNEXITLIST pfn)  [DOSCALLS.296]
; Accept exit-list registration and succeed (we don't run the handlers).
thunk_dosexitlist:
    xor eax, eax
    ret

; APIRET DosCreateMutexSem(PSZ name, PHMTX ph, ULONG flags, BOOL init)  [DOSCALLS.331]
; Hand back a non-zero pseudo-handle; the runtime just needs something
; it can pass to request/release (which are no-ops here).
thunk_doscreatemutexsem:
    mov ecx, [esp + 8]          ; PHMTX
    jecxz .done
    mov dword [ecx], 1
.done:
    xor eax, eax
    ret

; APIRET DosRaiseException(PEXCEPTIONREPORTRECORD p)  [DOSCALLS.356]
; APIRET DosSetSignalExceptionFocus(BOOL flag, PULONG pcnt)  [DOSCALLS.378]
thunk_dossetsigexcfocus:
    mov ecx, [esp + 8]
    jecxz .d
    mov dword [ecx], 1
.d:
    xor eax, eax
    ret

thunk_dosnoop2:                 ; generic "succeed and do nothing" stub
    xor eax, eax
    ret

; The app's action call (DosForceSystemDump / DosSysCtl / DosDumpProcess)
; and MSG message retrieval - report "not supported" so the tool prints
; its error/usage instead of crashing.
thunk_dosfail:
    mov eax, 1                  ; ERROR_INVALID_FUNCTION
    ret

lx_thunk_blob_end:

; Import table: module id (0=DOSCALLS, 1=NLS, 2=MSG), ordinal, blob offset
align 4
lx_thunk_table:
    dd 0, 282, thunk_doswrite        - lx_thunk_blob_start
    dd 0, 234, thunk_dosexit         - lx_thunk_blob_start
    dd 0, 229, thunk_dossleep        - lx_thunk_blob_start
    dd 0, 286, thunk_dosbeep         - lx_thunk_blob_start
    dd 0, 281, thunk_dosread         - lx_thunk_blob_start
    dd 0, 299, thunk_dosallocmem     - lx_thunk_blob_start
    dd 0, 304, thunk_dosfreemem     - lx_thunk_blob_start
    dd 0, 230, thunk_dosgetdatetime  - lx_thunk_blob_start
    dd 0, 224, thunk_dosqueryhtype   - lx_thunk_blob_start
    dd 0, 231, thunk_dosdevconfig    - lx_thunk_blob_start
    dd 0, 254, thunk_dosnoop         - lx_thunk_blob_start
    dd 0, 257, thunk_dosclose        - lx_thunk_blob_start
    dd 0, 273, thunk_dosopen         - lx_thunk_blob_start
    dd 0, 256, thunk_dossetfileptr   - lx_thunk_blob_start
    dd 0, 291, thunk_dosquerycp      - lx_thunk_blob_start
    dd 0, 348, thunk_dosquerysysinfo - lx_thunk_blob_start
    dd 0, 382, thunk_dossetrelmaxfh  - lx_thunk_blob_start
    dd 0, 319, thunk_dosquerymodhandle - lx_thunk_blob_start
    dd 0, 321, thunk_dosqueryprocaddr  - lx_thunk_blob_start
    dd 0, 425, thunk_dosflattosel    - lx_thunk_blob_start
    dd 0, 426, thunk_dosseltoflat    - lx_thunk_blob_start
    dd 0, 312, thunk_dosgetinfoblocks - lx_thunk_blob_start
    dd 0, 296, thunk_dosexitlist     - lx_thunk_blob_start
    dd 0, 331, thunk_doscreatemutexsem - lx_thunk_blob_start
    dd 0, 356, thunk_dosnoop2        - lx_thunk_blob_start
    dd 0, 378, thunk_dossetsigexcfocus - lx_thunk_blob_start
    dd 0, 444, thunk_dosfail         - lx_thunk_blob_start   ; DosForceSystemDump
    dd 0, 876, thunk_dosfail         - lx_thunk_blob_start   ; DosSysCtl
    dd 0, 113, thunk_dosfail         - lx_thunk_blob_start   ; DosDumpProcess
    dd 1,   6, thunk_dosquerydbcsenv - lx_thunk_blob_start
    dd 2,   6, thunk_dosfail         - lx_thunk_blob_start   ; MSG.6

lx_thunk_count:
    dd 28

; Blob offset of the unimplemented-API fallback (used by the loader
; for imports that have no thunk table entry)
global lx_thunk_unimpl_offset
lx_thunk_unimpl_offset:
    dd thunk_unimplemented - lx_thunk_blob_start

section .note.GNU-stack noalloc noexec nowrite progbits
