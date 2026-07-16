// File: dos_file.h
// DOS File System APIs (OS/2 compatible)

#ifndef DOS_FILE_H
#define DOS_FILE_H

#include "types.h"

// DOS File API error codes
#define DOS_ERROR_SUCCESS           0
#define DOS_ERROR_INVALID_HANDLE    6
#define DOS_ERROR_ACCESS_DENIED     5
#define DOS_ERROR_NOT_ENOUGH_MEMORY 8
#define DOS_ERROR_INVALID_PARAMETER 87
#define DOS_ERROR_FILE_NOT_FOUND    2
#define DOS_ERROR_PATH_NOT_FOUND    3
#define DOS_ERROR_TOO_MANY_OPEN_FILES 4
#define DOS_ERROR_FILE_EXISTS       80
#define DOS_ERROR_DIR_NOT_EMPTY     145

// File information structure (for DosQueryFileInfo)
typedef struct {
    uint32_t creation_time;
    uint32_t access_time;
    uint32_t modification_time;
    uint32_t size;
    uint32_t attributes;
} dos_file_info_t;

// Directory entry structure (for DosFindFirst/Next)
typedef struct {
    char name[256];
    uint32_t attributes;
    uint32_t size;
    uint32_t creation_time;
    uint32_t modification_time;
} dos_dir_entry_t;

// Initialize DOS file system
void dos_file_init(void);

// DOS File APIs (OS/2 compatible)

// Open or create a file
// Returns: 0 on success, error code on failure
// Output: file_handle - file descriptor
uint32_t DosOpen(const char* filename, uint32_t* file_handle, uint32_t* action,
                 uint32_t initial_size, uint32_t attributes, uint32_t open_flags,
                 uint32_t open_mode, void* reserved);

// Close a file
// Returns: 0 on success, error code on failure
uint32_t DosClose(uint32_t file_handle);

// Read from a file
// Returns: 0 on success, error code on failure
// Output: bytes_read - actual number of bytes read
uint32_t DosRead(uint32_t file_handle, void* buffer, uint32_t buffer_length,
                 uint32_t* bytes_read);

// Write to a file
// Returns: 0 on success, error code on failure
// Output: bytes_written - actual number of bytes written
uint32_t DosWrite(uint32_t file_handle, const void* buffer, uint32_t buffer_length,
                  uint32_t* bytes_written);

// Set file pointer position
// Returns: 0 on success, error code on failure
// Output: new_position - new file position
uint32_t DosSetFilePtr(uint32_t file_handle, int32_t distance,
                       uint32_t move_method, uint32_t* new_position);

// Delete a file
// Returns: 0 on success, error code on failure
uint32_t DosDelete(const char* filename);

// Query file information
// Returns: 0 on success, error code on failure
uint32_t DosQueryFileInfo(uint32_t file_handle, uint32_t info_level,
                          void* info_buffer, uint32_t buffer_size);

// Set file information
// Returns: 0 on success, error code on failure
uint32_t DosSetFileInfo(uint32_t file_handle, uint32_t info_level,
                        void* info_buffer, uint32_t buffer_size);

// Find first file matching pattern
// Returns: 0 on success, error code on failure
// Output: dir_handle - directory search handle, entry - first matching entry
uint32_t DosFindFirst(const char* pattern, uint32_t* dir_handle,
                      uint32_t attributes, dos_dir_entry_t* entry,
                      uint32_t entry_size, uint32_t* search_count,
                      void* reserved);

// Find next file in directory search
// Returns: 0 on success, error code on failure
// Output: entry - next matching entry
uint32_t DosFindNext(uint32_t dir_handle, dos_dir_entry_t* entry,
                     uint32_t entry_size, uint32_t* search_count);

// Close directory search
// Returns: 0 on success, error code on failure
uint32_t DosFindClose(uint32_t dir_handle);

// Create a directory
// Returns: 0 on success, error code on failure
uint32_t DosMkDir(const char* dirname, void* reserved);

// Remove a directory
// Returns: 0 on success, error code on failure
uint32_t DosRmDir(const char* dirname);

// Change current directory
// Returns: 0 on success, error code on failure
uint32_t DosSetCurrentDir(const char* dirname);

// Query current directory
// Returns: 0 on success, error code on failure
uint32_t DosQueryCurrentDir(uint32_t drive, char* buffer, uint32_t* buffer_length);

#endif // DOS_FILE_H
