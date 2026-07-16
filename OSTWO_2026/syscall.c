// File: syscall.c
// System call handler

#include "types.h"
#include "ramfs.h"

// Constants
#define MAX_PROCESSES 32

// Signal handler type
typedef void (*signal_handler_t)(int signum);

// External functions
extern void printf(const char* format, ...);
extern void vga_putchar(char c);
extern void vga_puts(const char* str);
extern void register_interrupt_handler(uint8_t n, isr_t handler);
extern void process_exit(uint32_t pid);
extern void process_exit_with_code(uint32_t pid, uint32_t exit_code);
extern void* process_current(void);
extern uint32_t process_get_pid(void* proc);
extern uint32_t process_get_ppid(void* proc);
extern void process_set_priority(void* proc, uint32_t priority);
extern void scheduler_schedule(void);
extern uint32_t process_create_from_binary(const char* name, const void* binary, uint32_t size, uint32_t priority);
extern int ramfs_read(const char* name, void* buffer, uint32_t max_size);
extern int ramfs_size(const char* name);
extern void speaker_beep(uint32_t frequency, uint32_t duration_ms);
extern void rtc_get_datetime(void* dt);
extern void rtc_set_datetime(const void* dt);
extern uint32_t timer_get_ticks(void);
extern void* process_alloc_mem(void* proc, uint32_t size_bytes);
extern int process_free_mem(void* proc, void* addr);
extern uint32_t sem_create_mutex(const char* name, uint32_t initial_state);
extern uint32_t sem_create_event(const char* name, uint32_t initial_state);
extern int32_t sem_request(uint32_t sem_id, uint32_t timeout_ms);
extern int32_t sem_clear(uint32_t sem_id);
extern int32_t sem_set(uint32_t sem_id);
extern int32_t sem_close(uint32_t sem_id);
extern uint32_t thread_create(const char* name, void (*entry_point)(void* arg), void* arg, uint32_t priority);
extern void thread_exit(uint32_t exit_code);
extern void* thread_current(void);
extern uint32_t thread_get_tid(void* thread);
extern uint32_t DosOpen(const char* filename, uint32_t* file_handle, uint32_t* action,
                        uint32_t initial_size, uint32_t attributes, uint32_t open_flags,
                        uint32_t open_mode, void* reserved);
extern uint32_t DosClose(uint32_t file_handle);
extern uint32_t DosRead(uint32_t file_handle, void* buffer, uint32_t buffer_length,
                        uint32_t* bytes_read);
extern uint32_t DosWrite(uint32_t file_handle, const void* buffer, uint32_t buffer_length,
                         uint32_t* bytes_written);
extern uint32_t DosSetFilePtr(uint32_t file_handle, int32_t distance,
                              uint32_t move_method, uint32_t* new_position);
extern uint32_t DosDelete(const char* filename);
extern uint32_t DosMkDir(const char* dirname, void* reserved);
extern uint32_t DosRmDir(const char* dirname);
extern uint32_t pipe_create(const char* name, uint32_t flags);
extern uint32_t pipe_open(const char* name, uint32_t flags);
extern int pipe_read(uint32_t pipe_id, void* buffer, uint32_t size);
extern int pipe_write(uint32_t pipe_id, const void* buffer, uint32_t size);
extern int pipe_close(uint32_t pipe_id);
extern uint32_t shm_create(uint32_t key, uint32_t size, uint32_t flags);
extern void* shm_attach(uint32_t shm_id);
extern int shm_detach(void* addr);
extern int shm_destroy(uint32_t shm_id);
extern uint32_t msgq_create(uint32_t key, uint32_t flags);
extern int msgq_send(uint32_t msgq_id, const void* msg, uint32_t size, uint32_t type);
extern int msgq_receive(uint32_t msgq_id, void* msg, uint32_t size, uint32_t* type);
extern int msgq_destroy(uint32_t msgq_id);
extern int signal_set_handler(uint32_t pid, int signum, signal_handler_t handler);
extern int signal_send(uint32_t pid, int signum);
extern int signal_block(uint32_t pid, int signum);
extern int signal_unblock(uint32_t pid, int signum);

// Syscall numbers
#define SYSCALL_EXIT    1
#define SYSCALL_WRITE   2
#define SYSCALL_READ    3
#define SYSCALL_FORK    4
#define SYSCALL_EXEC    5
#define SYSCALL_GETPID  6
#define SYSCALL_SLEEP   7
#define SYSCALL_BEEP    8
#define SYSCALL_GETDATETIME 9
#define SYSCALL_SETDATETIME 10
#define SYSCALL_GETPPID     11
#define SYSCALL_KILL        12
#define SYSCALL_SETPRIORITY 13
#define SYSCALL_WAITCHILD   14
#define SYSCALL_ALLOCMEM    15
#define SYSCALL_FREEMEM     16
#define SYSCALL_CREATEMUTEX 17
#define SYSCALL_CREATEEVENT 18
#define SYSCALL_SEMREQUEST  19
#define SYSCALL_SEMCLEAR    20
#define SYSCALL_SEMSET      21
#define SYSCALL_SEMCLOSE    22
#define SYSCALL_CREATETHREAD 23
#define SYSCALL_EXITTHREAD  24
#define SYSCALL_GETTID      25
#define SYSCALL_OPEN        26
#define SYSCALL_CLOSE       27
#define SYSCALL_FILEREAD    28
#define SYSCALL_FILEWRITE   29
#define SYSCALL_SEEK        30
#define SYSCALL_DELETE      31
#define SYSCALL_MKDIR       32
#define SYSCALL_RMDIR       33
#define SYSCALL_PIPE_CREATE 34
#define SYSCALL_PIPE_OPEN   35
#define SYSCALL_PIPE_READ   36
#define SYSCALL_PIPE_WRITE  37
#define SYSCALL_PIPE_CLOSE  38
#define SYSCALL_SHM_CREATE  39
#define SYSCALL_SHM_ATTACH  40
#define SYSCALL_SHM_DETACH  41
#define SYSCALL_SHM_DESTROY 42
#define SYSCALL_MSGQ_CREATE 43
#define SYSCALL_MSGQ_SEND   44
#define SYSCALL_MSGQ_RECEIVE 45
#define SYSCALL_MSGQ_DESTROY 46
#define SYSCALL_SIGNAL_SETHANDLER 47
#define SYSCALL_SIGNAL_SEND 48
#define SYSCALL_SIGNAL_BLOCK 49
#define SYSCALL_SIGNAL_UNBLOCK 50
#define SYSCALL_QUERYSYSINFO 51

// User mode test handler
extern void usermode_syscall_handler(registers_t* regs);

// Syscall handler
static void syscall_handler(registers_t* regs) {
    uint32_t syscall_num = regs->eax;

    // Debug logging of the first few syscalls (verifies Ring 3 entry
    // and IOPL=0). Kept behind a compile-time switch so normal runs
    // aren't cluttered with [Syscall-Debug] lines.
#ifdef SYSCALL_DEBUG
    static int syscall_count = 0;
    if (syscall_count < 5) {
        printf("[Syscall-Debug] #%d: num=%d, CS=0x%x (Ring %d)\n",
               syscall_count, syscall_num, regs->cs, regs->cs & 0x3);
        syscall_count++;
    }
#endif

    switch (syscall_num) {
        case SYSCALL_EXIT: {
            uint32_t exit_code = regs->ebx;
            void* proc = process_current();
            if (proc) {
                uint32_t pid = process_get_pid(proc);
                printf("\n[Syscall] Process %d exiting with code %d\n", pid, exit_code);
                process_exit_with_code(pid, exit_code);

                // Return to kernel using saved stack
                extern void exit_to_kernel(void);
                exit_to_kernel();

                // Should never reach here
                printf("[Syscall] ERROR: exit_to_kernel returned!\n");
                while (1) { __asm__ volatile("hlt"); }
            }
            break;
        }
        
        case SYSCALL_WRITE: {
            // Args: EBX = fd, ECX = buffer, EDX = length
            uint32_t fd = regs->ebx;
            const char* buffer = (const char*)regs->ecx;
            uint32_t length = regs->edx;

            if (fd == 1 || fd == 2) {  // stdout or stderr
                // Route through printf so output reaches the VGA text
                // buffer, the serial port AND the GUI shell window.
                // (The old vga_putchar-only loop made user program
                // output invisible in graphics mode.)
                for (uint32_t i = 0; i < length; i++) {
                    printf("%c", buffer[i]);
                }
                regs->eax = length;  // Return bytes written
            } else {
                regs->eax = (uint32_t)-1;  // Error
            }
            break;
        }
        
        case SYSCALL_READ: {
            // Args: EBX = fd, ECX = buffer, EDX = length
            // TODO: Implement proper keyboard buffering in Phase 2
            // For now, return "not implemented"
            regs->eax = (uint32_t)-1;
            break;
        }
        
        case SYSCALL_FORK: {
            printf("[Syscall] FORK called (not implemented)\n");
            regs->eax = (uint32_t)-1;
            break;
        }
        
        case SYSCALL_EXEC: {
            // Args: EBX = filename (const char*)
            const char* filename = (const char*)regs->ebx;

            // Get file size first
            int file_size = ramfs_size(filename);
            if (file_size < 0) {
                printf("[Syscall] EXEC: File '%s' not found\n", filename);
                regs->eax = 0;  // Return 0 (failure)
                break;
            }

            if (file_size > 4096) {
                printf("[Syscall] EXEC: File '%s' too large (%d bytes)\n", filename, file_size);
                regs->eax = 0;
                break;
            }

            // Static buffer: RAMFS_MAX_FILESIZE doesn't fit a kernel stack
            static uint8_t binary_buffer[RAMFS_MAX_FILESIZE];

            // Read binary from RamFS
            int bytes_read = ramfs_read(filename, binary_buffer, sizeof(binary_buffer));
            if (bytes_read != file_size) {
                printf("[Syscall] EXEC: Failed to read file '%s'\n", filename);
                regs->eax = 0;
                break;
            }

            // Create process from binary
            uint32_t pid = process_create_from_binary(filename, binary_buffer, file_size, 1);

            if (pid == 0) {
                printf("[Syscall] EXEC: Failed to create process from '%s'\n", filename);
                regs->eax = 0;
                break;
            }

            printf("[Syscall] EXEC: Created process %d from '%s'\n", pid, filename);
            regs->eax = pid;  // Return child PID
            break;
        }
        
        case SYSCALL_GETPID: {
            void* proc = process_current();
            if (proc) {
                regs->eax = process_get_pid(proc);
            } else {
                regs->eax = 0;  // No current process
            }
            break;
        }

        case SYSCALL_SLEEP: {
            // Args: EBX = milliseconds to sleep
            uint32_t duration_ms = regs->ebx;

            // Convert milliseconds to ticks (100 Hz = 10ms per tick)
            uint32_t ticks_to_wait = (duration_ms + 9) / 10;  // Round up
            if (ticks_to_wait == 0) ticks_to_wait = 1;

            // Simple busy-wait implementation
            // TODO: In Phase 2, implement proper process blocking
            uint32_t start_tick = timer_get_ticks();
            uint32_t end_tick = start_tick + ticks_to_wait;

            // INT 0x80 is an interrupt gate, so IF is clear here.
            // Timer ticks cannot advance without interrupts - enable
            // them for the wait or this loop never terminates.
            __asm__ volatile("sti");
            while (timer_get_ticks() < end_tick) {
                __asm__ volatile("pause");  // CPU hint for spin loop
            }
            __asm__ volatile("cli");

            regs->eax = 0;  // Success
            break;
        }

        case SYSCALL_BEEP: {
            // Args: EBX = frequency (Hz), ECX = duration (ms)
            uint32_t frequency = regs->ebx;
            uint32_t duration_ms = regs->ecx;

            // Play beep. speaker_beep busy-waits on timer ticks, which
            // need interrupts enabled (we arrived via interrupt gate).
            __asm__ volatile("sti");
            speaker_beep(frequency, duration_ms);
            __asm__ volatile("cli");
            regs->eax = 0;  // Success
            break;
        }

        case SYSCALL_QUERYSYSINFO: {
            // OS/2 DosQuerySysInfo:
            // EBX = first QSV index, ECX = last, EDX = buffer, ESI = buffer size
            uint32_t first = regs->ebx;
            uint32_t last = regs->ecx;
            uint32_t* buf = (uint32_t*)regs->edx;
            uint32_t bufsize = regs->esi;

            if (first == 0 || first > last || last > 23 || buf == NULL ||
                (last - first + 1) * 4 > bufsize) {
                regs->eax = 87;  // ERROR_INVALID_PARAMETER
                break;
            }

            extern uint32_t pmm_get_highest_address(void);
            for (uint32_t q = first; q <= last; q++) {
                uint32_t v = 0;
                switch (q) {
                    case 1:  v = 260; break;                   // QSV_MAX_PATH_LENGTH
                    case 2:  v = 16; break;                    // QSV_MAX_TEXT_SESSIONS
                    case 3:  v = 16; break;                    // QSV_MAX_PM_SESSIONS
                    case 4:  v = 0; break;                     // QSV_MAX_VDM_SESSIONS
                    case 5:  v = 3; break;                     // QSV_BOOT_DRIVE (C:)
                    case 6:  v = 1; break;                     // QSV_DYN_PRI_VARIATION
                    case 7:  v = 1; break;                     // QSV_MAX_WAIT (s)
                    case 8:  v = 32; break;                    // QSV_MIN_SLICE (ms)
                    case 9:  v = 65536; break;                 // QSV_MAX_SLICE (ms)
                    case 10: v = 4096; break;                  // QSV_PAGE_SIZE
                    case 11: v = 20; break;                    // QSV_VERSION_MAJOR
                    case 12: v = 40; break;                    // QSV_VERSION_MINOR (Warp 4)
                    case 13: v = 0; break;                     // QSV_VERSION_REVISION
                    case 14: v = timer_get_ticks() * 10; break;// QSV_MS_COUNT
                    case 15: v = 0; break;                     // QSV_TIME_LOW
                    case 16: v = 0; break;                     // QSV_TIME_HIGH
                    case 17: v = pmm_get_highest_address(); break; // QSV_TOTPHYSMEM
                    case 18: v = 8 * 1024 * 1024; break;       // QSV_TOTRESMEM
                    case 19: v = 32 * 1024 * 1024; break;      // QSV_TOTAVAILMEM
                    case 20: v = 32 * 1024 * 1024; break;      // QSV_MAXPRMEM
                    case 21: v = 16 * 1024 * 1024; break;      // QSV_MAXSHMEM
                    case 22: v = 100; break;                   // QSV_TIMER_INTERVAL (0.1ms)
                    case 23: v = 255; break;                   // QSV_MAX_COMP_LENGTH
                }
                buf[q - first] = v;
            }
            regs->eax = 0;  // NO_ERROR
            break;
        }

        case SYSCALL_GETDATETIME: {
            // Args: EBX = pointer to DateTime structure
            void* dt_ptr = (void*)regs->ebx;

            if (dt_ptr == 0) {
                regs->eax = (uint32_t)-1;  // Error: NULL pointer
                break;
            }

            // Read current date/time from RTC
            rtc_get_datetime(dt_ptr);
            regs->eax = 0;  // Success
            break;
        }

        case SYSCALL_SETDATETIME: {
            // Args: EBX = pointer to DateTime structure
            const void* dt_ptr = (const void*)regs->ebx;

            if (dt_ptr == 0) {
                regs->eax = (uint32_t)-1;  // Error: NULL pointer
                break;
            }

            // Set RTC date/time
            rtc_set_datetime(dt_ptr);
            regs->eax = 0;  // Success
            break;
        }

        case SYSCALL_GETPPID: {
            // Get parent process ID
            void* proc = process_current();
            if (proc) {
                regs->eax = process_get_ppid(proc);
            } else {
                regs->eax = 0;  // No current process
            }
            break;
        }

        case SYSCALL_KILL: {
            // Args: EBX = PID to kill, ECX = signal (ignored for now)
            uint32_t pid = regs->ebx;

            if (pid == 0 || pid > MAX_PROCESSES) {
                regs->eax = (uint32_t)-1;  // Invalid PID
                break;
            }

            // Kill the process
            process_exit(pid);
            regs->eax = 0;  // Success
            break;
        }

        case SYSCALL_SETPRIORITY: {
            // Args: EBX = PID (0 = current), ECX = new priority
            uint32_t pid = regs->ebx;
            uint32_t priority = regs->ecx;

            if (priority > 3) {  // Max priority is 3 (REALTIME)
                regs->eax = (uint32_t)-1;  // Invalid priority
                break;
            }

            void* proc;
            if (pid == 0) {
                proc = process_current();
            } else {
                extern void* process_get(uint32_t pid);
                proc = process_get(pid);
            }

            if (proc) {
                process_set_priority(proc, priority);
                regs->eax = 0;  // Success
            } else {
                regs->eax = (uint32_t)-1;  // Process not found
            }
            break;
        }

        case SYSCALL_ALLOCMEM: {
            // Args: EBX = size in bytes
            uint32_t size_bytes = regs->ebx;

            void* proc = process_current();
            if (!proc) {
                regs->eax = 0;  // No current process
                break;
            }

            void* virtual_addr = process_alloc_mem(proc, size_bytes);
            regs->eax = (uint32_t)virtual_addr;  // Return virtual address (or 0 on failure)
            break;
        }

        case SYSCALL_FREEMEM: {
            // Args: EBX = virtual address to free
            void* addr = (void*)regs->ebx;

            void* proc = process_current();
            if (!proc) {
                regs->eax = (uint32_t)-1;  // No current process
                break;
            }

            int result = process_free_mem(proc, addr);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_CREATEMUTEX: {
            // Args: EBX = name (const char*), ECX = initial_state
            const char* name = (const char*)regs->ebx;
            uint32_t initial_state = regs->ecx;

            uint32_t sem_id = sem_create_mutex(name, initial_state);
            regs->eax = sem_id;  // Return semaphore ID (or 0 on failure)
            break;
        }

        case SYSCALL_CREATEEVENT: {
            // Args: EBX = name (const char*), ECX = initial_state
            const char* name = (const char*)regs->ebx;
            uint32_t initial_state = regs->ecx;

            uint32_t sem_id = sem_create_event(name, initial_state);
            regs->eax = sem_id;  // Return semaphore ID (or 0 on failure)
            break;
        }

        case SYSCALL_SEMREQUEST: {
            // Args: EBX = semaphore ID, ECX = timeout_ms
            uint32_t sem_id = regs->ebx;
            uint32_t timeout_ms = regs->ecx;

            int32_t result = sem_request(sem_id, timeout_ms);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_SEMCLEAR: {
            // Args: EBX = semaphore ID
            uint32_t sem_id = regs->ebx;

            int32_t result = sem_clear(sem_id);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_SEMSET: {
            // Args: EBX = semaphore ID
            uint32_t sem_id = regs->ebx;

            int32_t result = sem_set(sem_id);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_SEMCLOSE: {
            // Args: EBX = semaphore ID
            uint32_t sem_id = regs->ebx;

            int32_t result = sem_close(sem_id);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_CREATETHREAD: {
            // Args: EBX = name, ECX = entry_point, EDX = arg, ESI = priority
            const char* name = (const char*)regs->ebx;
            void (*entry_point)(void* arg) = (void (*)(void*))regs->ecx;
            void* arg = (void*)regs->edx;
            uint32_t priority = regs->esi;

            uint32_t tid = thread_create(name, entry_point, arg, priority);
            regs->eax = tid;  // Return TID (or 0 on failure)
            break;
        }

        case SYSCALL_EXITTHREAD: {
            // Args: EBX = exit_code
            uint32_t exit_code = regs->ebx;

            // This will not return
            thread_exit(exit_code);
            break;
        }

        case SYSCALL_GETTID: {
            // Get current thread ID
            void* thread = thread_current();
            if (thread) {
                regs->eax = thread_get_tid(thread);
            } else {
                regs->eax = 0;  // No current thread
            }
            break;
        }

        case SYSCALL_OPEN: {
            // Args: EBX = filename, ECX = file_handle ptr, EDX = open_flags, ESI = open_mode
            const char* filename = (const char*)regs->ebx;
            uint32_t* file_handle = (uint32_t*)regs->ecx;
            uint32_t open_flags = regs->edx;
            uint32_t open_mode = regs->esi;

            uint32_t action = 0;
            uint32_t result = DosOpen(filename, file_handle, &action, 0, 0, open_flags, open_mode, NULL);
            regs->eax = result;  // Return error code (0 = success)
            break;
        }

        case SYSCALL_CLOSE: {
            // Args: EBX = file_handle
            uint32_t file_handle = regs->ebx;

            uint32_t result = DosClose(file_handle);
            regs->eax = result;  // Return error code (0 = success)
            break;
        }

        case SYSCALL_FILEREAD: {
            // Args: EBX = file_handle, ECX = buffer, EDX = buffer_length, ESI = bytes_read ptr
            uint32_t file_handle = regs->ebx;
            void* buffer = (void*)regs->ecx;
            uint32_t buffer_length = regs->edx;
            uint32_t* bytes_read = (uint32_t*)regs->esi;

            uint32_t result = DosRead(file_handle, buffer, buffer_length, bytes_read);
            regs->eax = result;  // Return error code (0 = success)
            break;
        }

        case SYSCALL_FILEWRITE: {
            // Args: EBX = file_handle, ECX = buffer, EDX = buffer_length, ESI = bytes_written ptr
            uint32_t file_handle = regs->ebx;
            const void* buffer = (const void*)regs->ecx;
            uint32_t buffer_length = regs->edx;
            uint32_t* bytes_written = (uint32_t*)regs->esi;

            uint32_t result = DosWrite(file_handle, buffer, buffer_length, bytes_written);
            regs->eax = result;  // Return error code (0 = success)
            break;
        }

        case SYSCALL_SEEK: {
            // Args: EBX = file_handle, ECX = distance, EDX = move_method, ESI = new_position ptr
            uint32_t file_handle = regs->ebx;
            int32_t distance = (int32_t)regs->ecx;
            uint32_t move_method = regs->edx;
            uint32_t* new_position = (uint32_t*)regs->esi;

            uint32_t result = DosSetFilePtr(file_handle, distance, move_method, new_position);
            regs->eax = result;  // Return error code (0 = success)
            break;
        }

        case SYSCALL_DELETE: {
            // Args: EBX = filename
            const char* filename = (const char*)regs->ebx;

            uint32_t result = DosDelete(filename);
            regs->eax = result;  // Return error code (0 = success)
            break;
        }

        case SYSCALL_MKDIR: {
            // Args: EBX = dirname
            const char* dirname = (const char*)regs->ebx;

            uint32_t result = DosMkDir(dirname, NULL);
            regs->eax = result;  // Return error code (0 = success)
            break;
        }

        case SYSCALL_RMDIR: {
            // Args: EBX = dirname
            const char* dirname = (const char*)regs->ebx;

            uint32_t result = DosRmDir(dirname);
            regs->eax = result;  // Return error code (0 = success)
            break;
        }

        case SYSCALL_PIPE_CREATE: {
            // Args: EBX = name (const char*, NULL for anonymous), ECX = flags
            const char* name = (const char*)regs->ebx;
            uint32_t flags = regs->ecx;

            uint32_t pipe_id = pipe_create(name, flags);
            regs->eax = pipe_id;  // Return pipe ID (or -1 on failure)
            break;
        }

        case SYSCALL_PIPE_OPEN: {
            // Args: EBX = name (const char*), ECX = flags
            const char* name = (const char*)regs->ebx;
            uint32_t flags = regs->ecx;

            uint32_t pipe_id = pipe_open(name, flags);
            regs->eax = pipe_id;  // Return pipe ID (or -1 on failure)
            break;
        }

        case SYSCALL_PIPE_READ: {
            // Args: EBX = pipe_id, ECX = buffer, EDX = size
            uint32_t pipe_id = regs->ebx;
            void* buffer = (void*)regs->ecx;
            uint32_t size = regs->edx;

            int bytes_read = pipe_read(pipe_id, buffer, size);
            regs->eax = bytes_read;  // Return bytes read (or -1 on error)
            break;
        }

        case SYSCALL_PIPE_WRITE: {
            // Args: EBX = pipe_id, ECX = buffer, EDX = size
            uint32_t pipe_id = regs->ebx;
            const void* buffer = (const void*)regs->ecx;
            uint32_t size = regs->edx;

            int bytes_written = pipe_write(pipe_id, buffer, size);
            regs->eax = bytes_written;  // Return bytes written (or -1 on error)
            break;
        }

        case SYSCALL_PIPE_CLOSE: {
            // Args: EBX = pipe_id
            uint32_t pipe_id = regs->ebx;

            int result = pipe_close(pipe_id);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_SHM_CREATE: {
            // Args: EBX = key, ECX = size, EDX = flags
            uint32_t key = regs->ebx;
            uint32_t size = regs->ecx;
            uint32_t flags = regs->edx;

            uint32_t shm_id = shm_create(key, size, flags);
            regs->eax = shm_id;  // Return shared memory ID (or -1 on failure)
            break;
        }

        case SYSCALL_SHM_ATTACH: {
            // Args: EBX = shm_id
            uint32_t shm_id = regs->ebx;

            void* addr = shm_attach(shm_id);
            regs->eax = (uint32_t)addr;  // Return address (or NULL on failure)
            break;
        }

        case SYSCALL_SHM_DETACH: {
            // Args: EBX = address
            void* addr = (void*)regs->ebx;

            int result = shm_detach(addr);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_SHM_DESTROY: {
            // Args: EBX = shm_id
            uint32_t shm_id = regs->ebx;

            int result = shm_destroy(shm_id);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_MSGQ_CREATE: {
            // Args: EBX = key, ECX = flags
            uint32_t key = regs->ebx;
            uint32_t flags = regs->ecx;

            uint32_t msgq_id = msgq_create(key, flags);
            regs->eax = msgq_id;  // Return queue ID (or -1 on failure)
            break;
        }

        case SYSCALL_MSGQ_SEND: {
            // Args: EBX = msgq_id, ECX = msg buffer, EDX = size, ESI = type
            uint32_t msgq_id = regs->ebx;
            const void* msg = (const void*)regs->ecx;
            uint32_t size = regs->edx;
            uint32_t type = regs->esi;

            int result = msgq_send(msgq_id, msg, size, type);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_MSGQ_RECEIVE: {
            // Args: EBX = msgq_id, ECX = msg buffer, EDX = size, ESI = type ptr
            uint32_t msgq_id = regs->ebx;
            void* msg = (void*)regs->ecx;
            uint32_t size = regs->edx;
            uint32_t* type = (uint32_t*)regs->esi;

            int bytes_received = msgq_receive(msgq_id, msg, size, type);
            regs->eax = bytes_received;  // Return bytes received (or -1 on error)
            break;
        }

        case SYSCALL_MSGQ_DESTROY: {
            // Args: EBX = msgq_id
            uint32_t msgq_id = regs->ebx;

            int result = msgq_destroy(msgq_id);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_SIGNAL_SETHANDLER: {
            // Args: EBX = pid, ECX = signum, EDX = handler
            uint32_t pid = regs->ebx;
            int signum = (int)regs->ecx;
            signal_handler_t handler = (signal_handler_t)regs->edx;

            int result = signal_set_handler(pid, signum, handler);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_SIGNAL_SEND: {
            // Args: EBX = pid, ECX = signum
            uint32_t pid = regs->ebx;
            int signum = (int)regs->ecx;

            int result = signal_send(pid, signum);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_SIGNAL_BLOCK: {
            // Args: EBX = pid, ECX = signum
            uint32_t pid = regs->ebx;
            int signum = (int)regs->ecx;

            int result = signal_block(pid, signum);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        case SYSCALL_SIGNAL_UNBLOCK: {
            // Args: EBX = pid, ECX = signum
            uint32_t pid = regs->ebx;
            int signum = (int)regs->ecx;

            int result = signal_unblock(pid, signum);
            regs->eax = result;  // Return 0 on success, -1 on error
            break;
        }

        default:
            printf("[Syscall] Unknown syscall %d\n", syscall_num);
            regs->eax = (uint32_t)-1;
            break;
    }
}

// Initialize syscall handler
void syscall_init(void) {
    register_interrupt_handler(128, syscall_handler);
    printf("[Syscall] Handler registered (int 0x80)\n");
}
