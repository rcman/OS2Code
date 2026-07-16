# DosExecPgm Testing Guide

## Quick Start

### Build Everything
```bash
cd userprogs
make clean && make
cd ..
make clean && make
```

### Run the OS
```bash
make run              # Opens QEMU window (recommended)
# OR
make run-nographic    # Runs in terminal
```

## Test Sequence

### Test 1: Verify Files Loaded
Once booted, you should see:
```
System ready! Type 'help' for commands.
kernel>
```

Type: `ls`

Expected output:
```
Files in RamFS (2):
  hello.bin (60 bytes)
  loop.bin (144 bytes)
```

✅ **Pass:** Both binaries are loaded
❌ **Fail:** Files missing - check kernel boot messages for "[RamFS] Loaded 2 built-in programs"

---

### Test 2: Execute hello.bin

Type: `exec hello.bin`

Expected output:
```
Executing 'hello.bin' from RamFS...
[Process] Created process 'hello.bin' (PID 1) from binary (60 bytes at 0x400000)
Created process 1, starting scheduler...
[Scheduler] Starting scheduler...
[Scheduler] Switching to process 'hello.bin' (PID 1)
Hello from user program!
[Syscall] Process 1 exiting with code 0
[Process] Terminating process 'hello.bin' (PID 1) with exit code 0
```

✅ **Pass:** Message "Hello from user program!" appears
❌ **Fail:** See troubleshooting below

**What This Tests:**
- Binary loading from RamFS
- Process creation from binary
- Memory mapping (code + stack pages)
- User mode (Ring 3) execution
- DosWrite syscall
- DosExit syscall
- Process termination
- Scheduler switching

---

### Test 3: Execute loop.bin

Type: `exec loop.bin`

Expected output:
```
Executing 'loop.bin' from RamFS...
[Process] Created process 'loop.bin' (PID 2) from binary (144 bytes at 0x400000)
Created process 2, starting scheduler...
[Scheduler] Starting scheduler...
[Scheduler] Switching to process 'loop.bin' (PID 2)
[Loop] Starting...
XXXXXXXXXX
[Loop] Exiting with code 42
[Syscall] Process 2 exiting with code 42
[Process] Terminating process 'loop.bin' (PID 2) with exit code 42
```

✅ **Pass:** "X" appears 10 times
❌ **Fail:** See troubleshooting below

**What This Tests:**
- Longer program execution
- Busy-wait loops in user mode
- Multiple syscalls in sequence
- Non-zero exit code

---

### Test 4: Check Help

Type: `help`

Verify that `exec <file>` appears in the command list.

---

### Test 5: Error Handling

Type: `exec nonexistent.bin`

Expected output:
```
Executing 'nonexistent.bin' from RamFS...
ERROR: File 'nonexistent.bin' not found in RamFS
Use 'ls' to see available files.
```

✅ **Pass:** Error message shown, kernel doesn't crash
❌ **Fail:** Kernel crashes or hangs

---

## Advanced Tests

### Test 6: Multiple Processes (Not Yet Implemented)

This will require creating multiple processes and having them run concurrently.

Type: `testproc` to run existing multi-process test, then we can extend it.

---

## Troubleshooting

### Problem: Files not in RamFS

**Symptoms:** `ls` shows 0 files

**Cause:** Binary headers not found during build

**Fix:**
```bash
cd userprogs
make clean && make
xxd -i hello.bin > ../hello_bin.h
xxd -i loop.bin > ../loop_bin.h
cd ..
make clean && make
```

---

### Problem: "File not found" error

**Symptoms:** `exec hello.bin` says file not found

**Diagnosis:**
1. Run `ls` to verify file exists
2. Check filename spelling (case-sensitive)

---

### Problem: Program doesn't print output

**Symptoms:** Process created but no "Hello from user program!" appears

**Diagnosis:**
1. Check kernel boot for syscall handler registration
2. Look for "[Syscall-Debug]" messages showing syscall execution
3. Verify Ring 3 execution (CS should be 0x1B)

**Possible causes:**
- Syscall handler not working
- Binary corrupted
- Memory mapping failed

**Debug:**
```
kernel> ps
```
Should show the process in TERMINATED state.

---

### Problem: Triple fault / kernel crash

**Symptoms:** QEMU resets or "Triple fault" message

**Possible causes:**
1. Page fault during binary copy
2. Invalid instruction in user program
3. Stack overflow
4. Page directory corruption

**Debug:**
Add `-d int,cpu_reset` to QEMU command in Makefile to see fault details.

---

### Problem: Kernel hangs

**Symptoms:** No response after `exec` command

**Possible causes:**
1. Scheduler not switching back
2. Process stuck in infinite loop without exits
3. Interrupt problem

**Debug:**
Press Ctrl+C to break, or reset QEMU (Ctrl+A, X in nographic mode).

---

## Verification Checklist

- [ ] Kernel builds without errors
- [ ] Kernel boots and shows "System ready!"
- [ ] RamFS shows "[RamFS] Loaded 2 built-in programs"
- [ ] `ls` command lists hello.bin and loop.bin
- [ ] `exec hello.bin` prints "Hello from user program!"
- [ ] `exec loop.bin` prints X ten times
- [ ] Both programs exit cleanly
- [ ] Kernel returns to command prompt after execution
- [ ] `exec nonexistent.bin` shows error without crashing
- [ ] `ps` command works after program execution

## Expected Kernel Messages

On successful execution, look for these key messages:

1. **Binary Loading:**
   ```
   [RamFS] Loaded 2 built-in programs
   ```

2. **Process Creation:**
   ```
   [Process] Created process 'hello.bin' (PID X) from binary (60 bytes at 0x400000)
   ```

3. **Scheduler Start:**
   ```
   [Scheduler] Starting scheduler...
   [Scheduler] Switching to process 'hello.bin' (PID X)
   ```

4. **Syscall Execution:**
   ```
   [Syscall-Debug] #0: num=2, CS=0x1b (Ring 3)
   ```

5. **Process Exit:**
   ```
   [Syscall] Process X exiting with code 0
   [Process] Terminating process 'hello.bin' (PID X) with exit code 0
   ```

## Known Issues

1. **Scheduler doesn't return to shell:**
   - Current implementation: Once scheduler starts, it runs continuously
   - Workaround: Reset QEMU to return to shell
   - Fix needed: Implement scheduler_stop() or idle process handling

2. **Page directory cleanup:**
   - Process page directories not freed on exit
   - Causes memory leak
   - Not critical for testing (only 32 processes max)

3. **No concurrent program execution:**
   - Only one user program at a time
   - Need to extend `exec` to add to scheduler queue without replacing

## Success Criteria

✅ Implementation is successful if:
1. Programs load from RamFS
2. Programs execute in Ring 3
3. Programs can call syscalls (DosWrite, DosExit)
4. Programs print expected output
5. Programs terminate cleanly
6. No kernel crashes

## Next Steps After Testing

Once basic testing passes:

1. **Fix Scheduler:** Allow returning to shell after program exits
2. **Multi-process:** Run multiple programs concurrently
3. **Cleanup:** Free page directories on process exit
4. **Arguments:** Pass arguments to DosExecPgm
5. **Larger programs:** Support multi-page binaries
6. **LX Loader:** Begin implementing OS/2 executable format

## Manual Test Log Template

```
Date: ___________
Tester: ___________

Test 1 - File listing: [ ] PASS [ ] FAIL
  Notes: ________________________________

Test 2 - hello.bin: [ ] PASS [ ] FAIL
  Output: ________________________________

Test 3 - loop.bin: [ ] PASS [ ] FAIL
  Output: ________________________________

Test 4 - Error handling: [ ] PASS [ ] FAIL
  Notes: ________________________________

Issues found:
_____________________________________________
_____________________________________________

Overall: [ ] ALL PASS [ ] NEEDS WORK
```

---

**Ready to test!** Run `make run` and follow the test sequence above.
