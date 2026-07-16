#!/bin/bash
# Test script for syscall interface

echo "=== Testing Syscall Interface ==="
echo ""

echo "Test 1: testsyscall (DosWrite, DosGetPID, DosPutChar)"
echo "testsyscall" | timeout 8 qemu-system-i386 -kernel kernel.bin -m 64M -nographic 2>&1 | tail -50
echo ""
echo "----------------------------------------"
echo ""

echo "Test 2: testproc (multitasking with syscalls - A, B, C)"
echo "testproc" | timeout 8 qemu-system-i386 -kernel kernel.bin -m 64M -nographic 2>&1 | tail -30
echo ""
echo "----------------------------------------"
echo ""

echo "Test 3: testexit (DosExit test)"
echo "testexit" | timeout 8 qemu-system-i386 -kernel kernel.bin -m 64M -nographic 2>&1 | tail -30
echo ""
echo "========================================="
