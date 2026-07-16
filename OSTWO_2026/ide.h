// File: ide.h
// IDE/ATA hard disk driver (PIO mode)

#ifndef IDE_H
#define IDE_H

#include "types.h"

// IDE controller ports (primary and secondary)
#define IDE_PRIMARY_BASE        0x1F0
#define IDE_PRIMARY_CTRL        0x3F6
#define IDE_SECONDARY_BASE      0x170
#define IDE_SECONDARY_CTRL      0x376

// IDE register offsets (from base)
#define IDE_REG_DATA            0   // Data register (16-bit)
#define IDE_REG_ERROR           1   // Error register (read)
#define IDE_REG_FEATURES        1   // Features register (write)
#define IDE_REG_SECTOR_COUNT    2   // Sector count
#define IDE_REG_LBA_LO          3   // LBA low byte
#define IDE_REG_LBA_MID         4   // LBA mid byte
#define IDE_REG_LBA_HI          5   // LBA high byte
#define IDE_REG_DRIVE_SELECT    6   // Drive/head select
#define IDE_REG_STATUS          7   // Status register (read)
#define IDE_REG_COMMAND         7   // Command register (write)

// Control register (from ctrl base)
#define IDE_REG_ALT_STATUS      0   // Alternate status (read)
#define IDE_REG_DEVICE_CTRL     0   // Device control (write)
#define IDE_REG_DRIVE_ADDR      1   // Drive address

// Status register bits
#define IDE_STATUS_ERR      0x01    // Error
#define IDE_STATUS_IDX      0x02    // Index
#define IDE_STATUS_CORR     0x04    // Corrected data
#define IDE_STATUS_DRQ      0x08    // Data request
#define IDE_STATUS_DSC      0x10    // Drive seek complete
#define IDE_STATUS_DF       0x20    // Drive fault
#define IDE_STATUS_DRDY     0x40    // Drive ready
#define IDE_STATUS_BSY      0x80    // Busy

// Error register bits
#define IDE_ERROR_AMNF      0x01    // Address mark not found
#define IDE_ERROR_TK0NF     0x02    // Track 0 not found
#define IDE_ERROR_ABRT      0x04    // Aborted command
#define IDE_ERROR_MCR       0x08    // Media change request
#define IDE_ERROR_IDNF      0x10    // ID not found
#define IDE_ERROR_MC        0x20    // Media changed
#define IDE_ERROR_UNC       0x40    // Uncorrectable data error
#define IDE_ERROR_BBK       0x80    // Bad block detected

// Device control register bits
#define IDE_CTRL_nIEN       0x02    // Disable interrupts
#define IDE_CTRL_SRST       0x04    // Software reset
#define IDE_CTRL_HOB        0x80    // High order byte

// Drive select bits
#define IDE_DRIVE_MASTER    0xA0    // Master drive (drive 0)
#define IDE_DRIVE_SLAVE     0xB0    // Slave drive (drive 1)
#define IDE_DRIVE_LBA       0x40    // LBA mode

// ATA commands
#define IDE_CMD_READ_SECTORS    0x20    // Read sectors (PIO)
#define IDE_CMD_WRITE_SECTORS   0x30    // Write sectors (PIO)
#define IDE_CMD_IDENTIFY        0xEC    // Identify drive
#define IDE_CMD_FLUSH_CACHE     0xE7    // Flush write cache

// IDE drive information
typedef struct {
    uint16_t base;              // Base I/O port
    uint16_t ctrl;              // Control I/O port
    uint8_t  drive;             // 0=master, 1=slave
    uint8_t  exists;            // 1 if drive exists
    uint32_t sectors;           // Total sectors (LBA28)
    uint16_t cylinders;         // Number of cylinders
    uint16_t heads;             // Number of heads
    uint16_t sectors_per_track; // Sectors per track
    char     model[41];         // Model string
    char     serial[21];        // Serial number
} ide_drive_t;

// Maximum IDE drives (2 channels * 2 drives)
#define IDE_MAX_DRIVES 4

// Initialize IDE subsystem
void ide_init(void);

// Read sectors from IDE drive
// drive: 0-3 (primary master/slave, secondary master/slave)
// lba: Logical block address
// count: Number of sectors to read
// buffer: Buffer to read into (must be count * 512 bytes)
// Returns: 0 on success, -1 on error
int ide_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint8_t* buffer);

// Write sectors to IDE drive
// drive: 0-3
// lba: Logical block address
// count: Number of sectors to write
// buffer: Buffer to write from (must be count * 512 bytes)
// Returns: 0 on success, -1 on error
int ide_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const uint8_t* buffer);

// Get drive information
ide_drive_t* ide_get_drive(uint8_t drive);

#endif // IDE_H
