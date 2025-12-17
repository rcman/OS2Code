# Implementing OS/2 APIs in 2ine: Complete Example

Let's implement **VioWrtCharStr** - a common function for writing text to the console.

## Step 1: Understanding the API

### OS/2 API Signature (from IBM docs):

```c
USHORT VioWrtCharStr(
    PCH     pchString,     // Pointer to string to write
    USHORT  cbString,      // Length of string
    USHORT  usRow,         // Starting row position
    USHORT  usColumn,      // Starting column position
    HVIO    hvio           // Video handle (reserved, must be 0)
);
```

### What it does:
- Writes a string at specific screen coordinates
- Row 0, Column 0 is top-left corner
- Returns 0 on success, error code on failure

### Common error codes:
- `0` - NO_ERROR
- `355` - ERROR_VIO_ROW
- `356` - ERROR_VIO_COL
- `436` - ERROR_VIO_INVALID_HANDLE

## Step 2: Create the Implementation

Open `2ine/lib2ine.c` and add this function:

```c
//--------------------------------------------------
// VioWrtCharStr - Write string at position
//--------------------------------------------------
uint16_t LX_VioWrtCharStr(
    const uint8_t *pchString, 
    uint16_t cbString,
    uint16_t usRow, 
    uint16_t usColumn,
    uint16_t hvio)
{
    // Validate parameters
    if (hvio != 0) {
        return 436;  // ERROR_VIO_INVALID_HANDLE
    }
    
    if (!pchString || cbString == 0) {
        return 0;  // Nothing to write
    }
    
    // Validate screen coordinates (assuming 80x25 terminal)
    if (usRow >= 25) {
        return 355;  // ERROR_VIO_ROW
    }
    if (usColumn >= 80) {
        return 356;  // ERROR_VIO_COL
    }
    
    // Move cursor to position using ANSI escape codes
    // ESC[row;colH moves cursor (1-based)
    printf("\033[%d;%dH", usRow + 1, usColumn + 1);
    
    // Write the string
    fwrite(pchString, 1, cbString, stdout);
    fflush(stdout);
    
    return 0;  // NO_ERROR
}
```

## Step 3: Register the Function

Find the function registry table in `lib2ine.c`. It looks like this:

```c
static const LxExport lx_native_exports[] = {
    // ... existing entries ...
    
    // Add your new function here:
    { "VIOCALLS", "VIOWRTCHARSTR", 116, LX_VioWrtCharStr },
    
    // Note: The ordinal number (116) comes from OS/2 API docs
    // You can find ordinals with: ./lx_dump yourapp.exe
    
    // ... more entries ...
};
```

**Important:** The ordinal number must match what the executable expects. Find it by:
```bash
./lx_dump yourapp.exe | grep -i "VioWrtCharStr"
```

## Step 4: Build and Test

```bash
cd 2ine/build
make

# Test with your app
./lx_loader ../tests/testapp.exe
```

---

# Let's Do Another: DosBeep

This one is simpler but shows different techniques.

## Step 1: API Definition

```c
USHORT DosBeep(
    ULONG ulFrequency,    // Sound frequency in Hz
    ULONG ulDuration      // Duration in milliseconds
);
```

## Step 2: Implementation Options

### Option A: Silent Stub (Quick & Easy)
```c
uint16_t LX_DosBeep(uint32_t ulFrequency, uint32_t ulDuration) {
    // Do nothing - silent implementation
    // Good for: getting app running quickly
    return 0;
}
```

### Option B: Terminal Bell
```c
uint16_t LX_DosBeep(uint32_t ulFrequency, uint32_t ulDuration) {
    // Use terminal bell character
    printf("\a");
    fflush(stdout);
    
    // Sleep for the duration
    usleep(ulDuration * 1000);  // Convert ms to microseconds
    
    return 0;
}
```

### Option C: Full Implementation (Linux PC Speaker)
```c
#include <sys/ioctl.h>
#include <linux/kd.h>

uint16_t LX_DosBeep(uint32_t ulFrequency, uint32_t ulDuration) {
    static int console_fd = -1;
    
    // Open console on first call
    if (console_fd == -1) {
        console_fd = open("/dev/console", O_WRONLY);
        if (console_fd == -1) {
            // Fallback to terminal bell
            printf("\a");
            usleep(ulDuration * 1000);
            return 0;
        }
    }
    
    // Clamp frequency to valid range
    if (ulFrequency < 20 || ulFrequency > 20000) {
        return 0;  // Invalid frequency - silent
    }
    
    // Start beep
    if (ioctl(console_fd, KIOCSOUND, (int)(1193180 / ulFrequency)) == -1) {
        // Fall back to terminal bell
        printf("\a");
    }
    
    // Wait for duration
    usleep(ulDuration * 1000);
    
    // Stop beep
    ioctl(console_fd, KIOCSOUND, 0);
    
    return 0;
}
```

### Option D: SDL2 Audio (Best for complex apps)
```c
#include <SDL2/SDL.h>

static SDL_AudioDeviceID audio_device = 0;

void beep_callback(void *userdata, uint8_t *stream, int len) {
    uint32_t freq = *(uint32_t*)userdata;
    static double phase = 0;
    int16_t *buffer = (int16_t*)stream;
    int samples = len / 2;
    
    for (int i = 0; i < samples; i++) {
        buffer[i] = (int16_t)(32000 * sin(phase));
        phase += 2.0 * M_PI * freq / 44100.0;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
    }
}

uint16_t LX_DosBeep(uint32_t ulFrequency, uint32_t ulDuration) {
    if (audio_device == 0) {
        SDL_Init(SDL_INIT_AUDIO);
        SDL_AudioSpec want = {
            .freq = 44100,
            .format = AUDIO_S16SYS,
            .channels = 1,
            .samples = 1024,
            .callback = beep_callback,
            .userdata = &ulFrequency
        };
        audio_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    }
    
    SDL_PauseAudioDevice(audio_device, 0);  // Start
    SDL_Delay(ulDuration);
    SDL_PauseAudioDevice(audio_device, 1);  // Stop
    
    return 0;
}
```

## Step 3: Choose Your Approach

For 2ine, I'd recommend **Option B** (Terminal Bell) because:
- ✓ No special permissions needed
- ✓ Works everywhere
- ✓ Simple and reliable
- ✗ Not frequency-accurate (but good enough)

## Step 4: Register It

```c
{ "DOSCALLS", "DOSBEEP", 286, LX_DosBeep },
```

---

# Complex Example: DosOpen

This shows parameter handling and error translation.

## Step 1: OS/2 API

```c
USHORT DosOpen(
    PSZ      pszFileName,      // File name
    PHFILE   pHandle,          // Handle (output)
    PULONG   pulAction,        // Action taken (output)
    ULONG    ulFileSize,       // Initial file size
    ULONG    ulAttribute,      // File attribute
    ULONG    ulOpenFlags,      // Open flags
    ULONG    ulOpenMode,       // Open mode
    PEAOP2   pEABuf            // Extended attributes (can be NULL)
);
```

### Flag translations needed:
```c
// OS/2 Open Flags -> Linux flags
#define OS2_FILE_OPEN       0x0001  // Open existing
#define OS2_FILE_CREATE     0x0010  // Create if doesn't exist
#define OS2_FILE_TRUNCATE   0x0002  // Truncate to 0

// OS/2 Open Mode -> Linux mode
#define OS2_OPEN_ACCESS_READONLY      0x0000
#define OS2_OPEN_ACCESS_WRITEONLY     0x0001
#define OS2_OPEN_ACCESS_READWRITE     0x0002
#define OS2_OPEN_SHARE_DENYREADWRITE  0x0010
#define OS2_OPEN_SHARE_DENYWRITE      0x0020
#define OS2_OPEN_SHARE_DENYREAD       0x0030
#define OS2_OPEN_SHARE_DENYNONE       0x0040
```

## Step 2: Implementation

```c
uint16_t LX_DosOpen(
    const uint8_t *pszFileName,
    uint16_t *pHandle,
    uint32_t *pulAction,
    uint32_t ulFileSize,
    uint32_t ulAttribute,
    uint32_t ulOpenFlags,
    uint32_t ulOpenMode,
    void *pEABuf)
{
    if (!pszFileName || !pHandle) {
        return 87;  // ERROR_INVALID_PARAMETER
    }
    
    // Translate flags
    int linux_flags = 0;
    
    // Access mode
    switch (ulOpenMode & 0x0003) {
        case 0x0000: linux_flags |= O_RDONLY; break;
        case 0x0001: linux_flags |= O_WRONLY; break;
        case 0x0002: linux_flags |= O_RDWR; break;
    }
    
    // Open/create flags
    if (ulOpenFlags & 0x0010) {  // FILE_CREATE
        linux_flags |= O_CREAT;
    }
    if (ulOpenFlags & 0x0002) {  // FILE_TRUNCATE
        linux_flags |= O_TRUNC;
    }
    
    // Attempt to open
    int fd = open((const char*)pszFileName, linux_flags, 0644);
    
    if (fd == -1) {
        // Translate errno to OS/2 error
        switch (errno) {
            case ENOENT: return 2;   // ERROR_FILE_NOT_FOUND
            case EACCES: return 5;   // ERROR_ACCESS_DENIED
            case EMFILE: return 4;   // ERROR_TOO_MANY_OPEN_FILES
            case EEXIST: return 80;  // ERROR_FILE_EXISTS
            case EISDIR: return 5;   // ERROR_ACCESS_DENIED
            default:     return 1;   // ERROR_INVALID_FUNCTION
        }
    }
    
    // Set action taken
    if (pulAction) {
        if (linux_flags & O_CREAT) {
            *pulAction = 2;  // FILE_CREATED
        } else {
            *pulAction = 1;  // FILE_EXISTED
        }
    }
    
    // Return handle
    *pHandle = (uint16_t)fd;
    
    return 0;  // NO_ERROR
}
```

## Step 3: Register

```c
{ "DOSCALLS", "DOSOPEN", 273, LX_DosOpen },
```

---

# Testing Your Implementation

## Create Test App (if you have OS/2 compiler):

```c
// test_beep.c
#define INCL_DOS
#include <os2.h>

int main(void) {
    ULONG rc;
    
    // Beep at 1000 Hz for 500ms
    rc = DosBeep(1000, 500);
    
    if (rc == 0) {
        printf("Beep succeeded!\n");
    } else {
        printf("Beep failed: %lu\n", rc);
    }
    
    return 0;
}
```

Compile on OS/2:
```bash
icc /Q test_beep.c
# or
wcl386 -l=os2v2 test_beep.c
```

## Test on Linux with 2ine:

```bash
./lx_loader test_beep.exe
```

---

# Debugging Tips

## Add debug output:

```c
uint16_t LX_DosBeep(uint32_t ulFrequency, uint32_t ulDuration) {
    #ifdef DEBUG_2INE
    fprintf(stderr, "[2ine] DosBeep(%u Hz, %u ms)\n", 
            ulFrequency, ulDuration);
    #endif
    
    printf("\a");
    usleep(ulDuration * 1000);
    
    return 0;
}
```

Build with debug:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

## Trace all API calls:

Add to each function:
```c
fprintf(stderr, "[API] %s called\n", __FUNCTION__);
```

Then run:
```bash
./lx_loader app.exe 2>&1 | grep "\[API\]"
```

---

# Quick Reference: Common Patterns

## Pattern 1: File Operations
```c
uint16_t LX_DosRead(uint16_t handle, void *buf, uint32_t count, uint32_t *bytesRead) {
    ssize_t result = read(handle, buf, count);
    if (result == -1) return 6;  // ERROR_INVALID_HANDLE
    if (bytesRead) *bytesRead = result;
    return 0;
}
```

## Pattern 2: Console Output
```c
uint16_t LX_VioWrtTTY(const uint8_t *str, uint16_t len, uint16_t reserved) {
    write(STDOUT_FILENO, str, len);
    return 0;
}
```

## Pattern 3: Memory Allocation
```c
uint16_t LX_DosAllocMem(void **ppv, uint32_t cb, uint32_t flags) {
    *ppv = malloc(cb);
    return (*ppv == NULL) ? 8 : 0;  // ERROR_NOT_ENOUGH_MEMORY or success
}
```

## Pattern 4: Thread Sleep
```c
uint16_t LX_DosSleep(uint32_t msec) {
    usleep(msec * 1000);
    return 0;
}
```

---

# What to Implement First

Priority order for most apps:

1. **File I/O**: DosOpen, DosRead, DosWrite, DosClose
2. **Console**: VioWrtTTY, VioWrtCharStr, VioGetCurPos
3. **Memory**: DosAllocMem, DosFreeMem
4. **Process**: DosExit, DosSleep
5. **Error**: DosError, DosBeep (notifications)

Skip for now:
- ❌ PM* functions (GUI - too complex)
- ❌ Gpi* functions (Graphics)
- ❌ Complex threading (DosCreateThread, semaphores)
- ❌ Advanced file systems (EAs, ACLs)

Start simple, test often, add complexity gradually!
