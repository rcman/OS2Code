#!/bin/bash
# Test script for DosExecPgm functionality

echo "Testing DosExecPgm implementation..."
echo "===================================="
echo ""

# Test 1: List files
echo "Test 1: Listing files in RamFS..."
{
    sleep 2
    echo "ls"
    sleep 1
} | timeout 5 qemu-system-i386 -kernel kernel.bin -m 64M -nographic 2>&1 | grep -A 10 "kernel>" | head -15

echo ""
echo "Test 2: Execute hello.bin..."
{
    sleep 2
    echo "exec hello.bin"
    sleep 3
} | timeout 8 qemu-system-i386 -kernel kernel.bin -m 64M -nographic 2>&1 | tail -20

echo ""
echo "Done!"
