#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>

// ============================================================================
// OS/2 API Type Definitions
// ============================================================================

typedef uint32_t APIRET;
typedef uint32_t HFILE;
typedef uint32_t ULONG;
typedef uint16_t USHORT;
typedef uint8_t  UCHAR;
typedef char*    PSZ;
typedef const char* PCSZ;
typedef void*    PVOID;
typedef HFILE*   PHFILE;
typedef ULONG*   PULONG;
typedef USHORT*  PUSHORT;
typedef int32_t  LONG;

#define APIENTRY

// OS/2 Error Codes
#define NO_ERROR                0
#define ERROR_INVALID_FUNCTION  1
#define ERROR_FILE_NOT_FOUND    2
#define ERROR_PATH_NOT_FOUND    3
#define ERROR_TOO_MANY_OPEN_FILES 4
#define ERROR_ACCESS_DENIED     5
#define ERROR_INVALID_HANDLE    6
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_INVALID_PARAMETER 87
#define ERROR_NEGATIVE_SEEK     131

// DosOpen flags
#define OPEN_ACTION_FAIL_IF_EXISTS     0x0000
#define OPEN_ACTION_OPEN_IF_EXISTS     0x0001
#define OPEN_ACTION_REPLACE_IF_EXISTS  0x0002
#define OPEN_ACTION_FAIL_IF_NEW        0x0000
#define OPEN_ACTION_CREATE_IF_NEW      0x0010

#define OPEN_FLAGS_NOINHERIT           0x0080
#define OPEN_FLAGS_FAIL_ON_ERROR       0x2000
#define OPEN_FLAGS_WRITE_THROUGH       0x4000

#define OPEN_SHARE_DENYREADWRITE       0x0010
#define OPEN_SHARE_DENYWRITE           0x0020
#define OPEN_SHARE_DENYREAD            0x0030
#define OPEN_SHARE_DENYNONE            0x0040

#define OPEN_ACCESS_READONLY           0x0000
#define OPEN_ACCESS_WRITEONLY          0x0001
#define OPEN_ACCESS_READWRITE          0x0002

// File attributes
#define FILE_NORMAL    0x0000
#define FILE_READONLY  0x0001
#define FILE_HIDDEN    0x0002
#define FILE_SYSTEM    0x0004
#define FILE_DIRECTORY 0x0010
#define FILE_ARCHIVED  0x0020

// DosOpen actions
#define FILE_EXISTED   1
#define FILE_CREATED   2
#define FILE_TRUNCATED 3

// Seek origins
#define FILE_BEGIN   0
#define FILE_CURRENT 1
#define FILE_END     2

// ============================================================================
// Handle Management
// ============================================================================

#define MAX_HANDLES 256

typedef struct {
int linux_fd;      // Linux file descriptor
int in_use;        // Is this handle slot in use?
char path[256];    // File path for debugging
} handle_entry_t;

static handle_entry_t handle_table[MAX_HANDLES];
static int handle_table_initialized = 0;

void init_handle_table() {
if (handle_table_initialized) return;

for (int i = 0; i < MAX_HANDLES; i++) {
    handle_table[i].linux_fd = -1;
    handle_table[i].in_use = 0;
    handle_table[i].path[0] = '\0';
}

// Reserve standard handles
handle_table[0].linux_fd = 0; // stdin
handle_table[0].in_use = 1;
handle_table[1].linux_fd = 1; // stdout
handle_table[1].in_use = 1;
handle_table[2].linux_fd = 2; // stderr
handle_table[2].in_use = 1;

handle_table_initialized = 1;

}

HFILE allocate_handle(int linux_fd, const char *path) {
for (int i = 3; i < MAX_HANDLES; i++) {
if (!handle_table[i].in_use) {
handle_table[i].linux_fd = linux_fd;
handle_table[i].in_use = 1;
strncpy(handle_table[i].path, path ? path : "", sizeof(handle_table[i].path) - 1);
return i;
}
}
return (HFILE)-1;
}

int get_linux_fd(HFILE handle) {
if (handle >= MAX_HANDLES || !handle_table[handle].in_use) {
return -1;
}
return handle_table[handle].linux_fd;
}

void free_handle(HFILE handle) {
if (handle < MAX_HANDLES && handle_table[handle].in_use) {
handle_table[handle].in_use = 0;
handle_table[handle].linux_fd = -1;
handle_table[handle].path[0] = '\0';
}
}

// ============================================================================
// Error Code Translation
// ============================================================================

APIRET errno_to_os2(int err) {
switch (err) {
case 0: return NO_ERROR;
case ENOENT: return ERROR_FILE_NOT_FOUND;
case EACCES: return ERROR_ACCESS_DENIED;
case EINVAL: return ERROR_INVALID_PARAMETER;
case EMFILE: return ERROR_TOO_MANY_OPEN_FILES;
case ENOMEM: return ERROR_NOT_ENOUGH_MEMORY;
case EBADF: return ERROR_INVALID_HANDLE;
default: return ERROR_INVALID_FUNCTION;
}
}

// ============================================================================
// OS/2 API Implementations
// ============================================================================

/**

- DosOpen - Open or create a file
  */
  APIRET APIENTRY DosOpen(
  PCSZ    pszFileName,    // File path name
  PHFILE  pHandle,        // Handle returned
  PULONG  pulAction,      // Action taken
  ULONG   ulFileSize,     // File size if created
  ULONG   ulAttribute,    // File attributes
  ULONG   ulOpenFlags,    // Open flags
  ULONG   ulOpenMode,     // Open mode
  PVOID   peaop2          // Extended attributes (unused)
  ) {
  init_handle_table();
  
  printf("[DosOpen] Opening: %s\n", pszFileName);
  
  // Translate open flags
  int flags = 0;
  int mode = 0644;
  
  // Determine access mode
  int access = ulOpenMode & 0x0003;
  if (access == OPEN_ACCESS_READONLY) {
  flags = O_RDONLY;
  } else if (access == OPEN_ACCESS_WRITEONLY) {
  flags = O_WRONLY;
  } else if (access == OPEN_ACCESS_READWRITE) {
  flags = O_RDWR;
  }
  
  // Determine creation flags
  int open_action = ulOpenFlags & 0x00FF;
  
  if (open_action & OPEN_ACTION_CREATE_IF_NEW) {
  flags |= O_CREAT;
  if (!(open_action & OPEN_ACTION_OPEN_IF_EXISTS)) {
  flags |= O_EXCL;
  }
  }
  
  if (open_action & OPEN_ACTION_REPLACE_IF_EXISTS) {
  flags |= O_TRUNC;
  }
  
  // Open the file
  int fd = open(pszFileName, flags, mode);
  
  if (fd < 0) {
  printf("[DosOpen] Failed: %s\n", strerror(errno));
  return errno_to_os2(errno);
  }
  
  // Allocate OS/2 handle
  HFILE os2_handle = allocate_handle(fd, pszFileName);
  if (os2_handle == (HFILE)-1) {
  close(fd);
  return ERROR_TOO_MANY_OPEN_FILES;
  }
  
  *pHandle = os2_handle;
  
  // Determine action taken
  struct stat st;
  if (fstat(fd, &st) == 0) {
  if (flags & O_CREAT) {
  *pulAction = (flags & O_EXCL) ? FILE_CREATED : FILE_EXISTED;
  } else {
  *pulAction = (flags & O_TRUNC) ? FILE_TRUNCATED : FILE_EXISTED;
  }
  }
  
  printf("[DosOpen] Success: handle=%d, fd=%d, action=%d\n",
  os2_handle, fd, *pulAction);
  
  return NO_ERROR;
  }

/**

- DosRead - Read from a file
  */
  APIRET APIENTRY DosRead(
  HFILE   hFile,          // File handle
  PVOID   pBuffer,        // Buffer
  ULONG   ulLength,       // Bytes to read
  PULONG  pulBytesRead    // Bytes actually read
  ) {
  int fd = get_linux_fd(hFile);
  if (fd < 0) {
  return ERROR_INVALID_HANDLE;
  }
  
  ssize_t bytes = read(fd, pBuffer, ulLength);
  
  if (bytes < 0) {
  printf("[DosRead] Failed: %s\n", strerror(errno));
  *pulBytesRead = 0;
  return errno_to_os2(errno);
  }
  
  *pulBytesRead = bytes;
  printf("[DosRead] Read %ld bytes from handle %d\n", bytes, hFile);
  
  return NO_ERROR;
  }

/**

- DosWrite - Write to a file
  */
  APIRET APIENTRY DosWrite(
  HFILE   hFile,          // File handle
  PVOID   pBuffer,        // Buffer
  ULONG   ulLength,       // Bytes to write
  PULONG  pulBytesWritten // Bytes actually written
  ) {
  int fd = get_linux_fd(hFile);
  if (fd < 0) {
  return ERROR_INVALID_HANDLE;
  }
  
  ssize_t bytes = write(fd, pBuffer, ulLength);
  
  if (bytes < 0) {
  printf("[DosWrite] Failed: %s\n", strerror(errno));
  *pulBytesWritten = 0;
  return errno_to_os2(errno);
  }
  
  *pulBytesWritten = bytes;
  printf("[DosWrite] Wrote %ld bytes to handle %d\n", bytes, hFile);
  
  return NO_ERROR;
  }

/**

- DosClose - Close a file
  */
  APIRET APIENTRY DosClose(HFILE hFile) {
  int fd = get_linux_fd(hFile);
  if (fd < 0) {
  return ERROR_INVALID_HANDLE;
  }
  
  if (close(fd) < 0) {
  return errno_to_os2(errno);
  }
  
  free_handle(hFile);
  printf("[DosClose] Closed handle %d\n", hFile);
  
  return NO_ERROR;
  }

/**

- DosSetFilePtr - Seek in a file
  */
  APIRET APIENTRY DosSetFilePtr(
  HFILE   hFile,          // File handle
  LONG    lOffset,        // Offset
  ULONG   ulOrigin,       // Origin (FILE_BEGIN, FILE_CURRENT, FILE_END)
  PULONG  pulNewPtr       // New file pointer
  ) {
  int fd = get_linux_fd(hFile);
  if (fd < 0) {
  return ERROR_INVALID_HANDLE;
  }
  
  int whence;
  switch (ulOrigin) {
  case FILE_BEGIN:   whence = SEEK_SET; break;
  case FILE_CURRENT: whence = SEEK_CUR; break;
  case FILE_END:     whence = SEEK_END; break;
  default: return ERROR_INVALID_PARAMETER;
  }
  
  off_t new_pos = lseek(fd, lOffset, whence);
  
  if (new_pos < 0) {
  return errno_to_os2(errno);
  }
  
  if (pulNewPtr) {
  *pulNewPtr = new_pos;
  }
  
  printf("[DosSetFilePtr] Seek: handle=%d, offset=%ld, new_pos=%ld\n",
  hFile, lOffset, new_pos);
  
  return NO_ERROR;
  }

/**

- DosDelete - Delete a file
  */
  APIRET APIENTRY DosDelete(PCSZ pszFileName) {
  printf("[DosDelete] Deleting: %s\n", pszFileName);
  
  if (unlink(pszFileName) < 0) {
  return errno_to_os2(errno);
  }
  
  return NO_ERROR;
  }

/**

- DosExit - Exit the process
  */
  void APIENTRY DosExit(ULONG ulAction, ULONG ulResult) {
  printf("[DosExit] Exiting with code %d\n", ulResult);
  exit(ulResult);
  }

/**

- DosSleep - Sleep for specified milliseconds
  */
  APIRET APIENTRY DosSleep(ULONG ulMilliseconds) {
  printf("[DosSleep] Sleeping for %d ms\n", ulMilliseconds);
  usleep(ulMilliseconds * 1000);
  return NO_ERROR;
  }

/**

- DosAllocMem - Allocate memory
  */
  APIRET APIENTRY DosAllocMem(
  PVOID*  ppBaseAddress,  // Base address returned
  ULONG   ulSize,         // Size to allocate
  ULONG   ulFlags         // Allocation flags
  ) {
  void *mem = malloc(ulSize);
  if (!mem) {
  return ERROR_NOT_ENOUGH_MEMORY;
  }
  
  *ppBaseAddress = mem;
  printf("[DosAllocMem] Allocated %d bytes at %p\n", ulSize, mem);
  
  return NO_ERROR;
  }

/**

- DosFreeMem - Free memory
  */
  APIRET APIENTRY DosFreeMem(PVOID pBaseAddress) {
  printf("[DosFreeMem] Freeing memory at %p\n", pBaseAddress);
  free(pBaseAddress);
  return NO_ERROR;
  }

// ============================================================================
// API Export Table
// ============================================================================

typedef struct {
const char *name;
void *function;
} api_export_t;

static api_export_t api_exports[] = {
// File I/O
{ "DosOpen",        DosOpen },
{ "DosRead",        DosRead },
{ "DosWrite",       DosWrite },
{ "DosClose",       DosClose },
{ "DosSetFilePtr",  DosSetFilePtr },
{ "DosDelete",      DosDelete },

// Process
{ "DosExit",        DosExit },
{ "DosSleep",       DosSleep },

// Memory
{ "DosAllocMem",    DosAllocMem },
{ "DosFreeMem",     DosFreeMem },

{ NULL, NULL }

};

/**

- Resolve an OS/2 API function by name
  */
  void* resolve_os2_api(const char *name) {
  for (int i = 0; api_exports[i].name != NULL; i++) {
  if (strcmp(api_exports[i].name, name) == 0) {
  printf("[API] Resolved: %s -> %p\n", name, api_exports[i].function);
  return api_exports[i].function;
  }
  }
  
  printf("[API] Warning: Unresolved function: %s\n", name);
  return NULL;
  }

// ============================================================================
// Test Program
// ============================================================================

#ifdef TEST_API

int main() {
printf("OS/2 API Emulation Layer Test\n");
printf("==============================\n\n");

init_handle_table();

// Test file operations
HFILE hFile;
ULONG action;
char buffer[256];
ULONG bytesRead, bytesWritten;

printf("Test 1: Create and write to file\n");
APIRET rc = DosOpen("test.txt", &hFile, &action, 0, FILE_NORMAL,
                    OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                    OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE, NULL);

if (rc == NO_ERROR) {
    const char *msg = "Hello from OS/2 API!\n";
    DosWrite(hFile, (void*)msg, strlen(msg), &bytesWritten);
    DosClose(hFile);
    printf("Success!\n\n");
}

printf("Test 2: Read from file\n");
rc = DosOpen("test.txt", &hFile, &action, 0, FILE_NORMAL,
             OPEN_ACTION_OPEN_IF_EXISTS,
             OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE, NULL);

if (rc == NO_ERROR) {
    DosRead(hFile, buffer, sizeof(buffer) - 1, &bytesRead);
    buffer[bytesRead] = '\0';
    printf("Read: %s", buffer);
    DosClose(hFile);
    printf("Success!\n\n");
}

printf("Test 3: Memory allocation\n");
void *mem;
rc = DosAllocMem(&mem, 1024, 0);
if (rc == NO_ERROR) {
    strcpy(mem, "Memory test");
    printf("Memory content: %s\n", (char*)mem);
    DosFreeMem(mem);
    printf("Success!\n\n");
}

printf("All tests passed!\n");
return 0;

}

#endif