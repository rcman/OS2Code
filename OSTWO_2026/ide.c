// File: ide.c
// IDE/ATA hard disk driver implementation

#include "ide.h"

// External I/O functions
extern void outb(uint16_t port, uint8_t value);
extern uint8_t inb(uint16_t port);
extern void outw(uint16_t port, uint16_t value);
extern uint16_t inw(uint16_t port);
extern void printf(const char* format, ...);

// IDE drive table
static ide_drive_t ide_drives[IDE_MAX_DRIVES];

// String utilities
static void str_copy(char* dest, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Small delay for IDE timing (400ns)
static void ide_delay(uint16_t ctrl) {
    // Read alt status 4 times (each read takes ~100ns)
    for (int i = 0; i < 4; i++) {
        inb(ctrl + IDE_REG_ALT_STATUS);
    }
}

// Wait for drive to be ready
static int ide_wait_ready(uint16_t base) {
    int timeout = 100000;
    uint8_t status;

    while (timeout > 0) {
        status = inb(base + IDE_REG_STATUS);

        // Check if busy
        if (!(status & IDE_STATUS_BSY)) {
            // Check if ready or data request
            if (status & (IDE_STATUS_DRDY | IDE_STATUS_DRQ)) {
                return 0;  // Success
            }
        }

        timeout--;
    }

    printf("[IDE] Timeout waiting for drive ready (status=0x%x)\n", status);
    return -1;  // Timeout
}

// Wait for DRQ (data request)
static int ide_wait_drq(uint16_t base) {
    int timeout = 100000;
    uint8_t status;

    while (timeout > 0) {
        status = inb(base + IDE_REG_STATUS);

        if (status & IDE_STATUS_DRQ) {
            return 0;  // DRQ set
        }

        if (status & IDE_STATUS_ERR) {
            printf("[IDE] Error waiting for DRQ (status=0x%x, error=0x%x)\n",
                   status, inb(base + IDE_REG_ERROR));
            return -1;
        }

        timeout--;
    }

    printf("[IDE] Timeout waiting for DRQ\n");
    return -1;
}

// Identify IDE drive
static int ide_identify(uint16_t base, uint16_t ctrl, uint8_t drive_sel, ide_drive_t* drive) {
    uint16_t identify_data[256];

    // Select drive
    outb(base + IDE_REG_DRIVE_SELECT, drive_sel);
    ide_delay(ctrl);

    // Disable interrupts
    outb(ctrl + IDE_REG_DEVICE_CTRL, IDE_CTRL_nIEN);

    // Send IDENTIFY command
    outb(base + IDE_REG_COMMAND, IDE_CMD_IDENTIFY);
    ide_delay(ctrl);

    // Check if drive exists
    uint8_t status = inb(base + IDE_REG_STATUS);
    if (status == 0 || status == 0xFF) {
        return -1;  // No drive
    }

    // ATAPI devices (CD-ROMs) abort the ATA IDENTIFY command and set
    // a signature in the LBA mid/high registers. Detect this before
    // waiting for DRQ so a CD-ROM doesn't get reported as a disk error.
    if (status & IDE_STATUS_ERR) {
        uint8_t lba_mid  = inb(base + 4);   // LBA mid register
        uint8_t lba_high = inb(base + 5);   // LBA high register
        if ((lba_mid == 0x14 && lba_high == 0xEB) ||
            (lba_mid == 0x69 && lba_high == 0x96)) {
            printf("[IDE] ATAPI device detected (CD-ROM), skipping\n");
        }
        return -1;
    }

    // Wait for drive to be ready
    if (ide_wait_ready(base) != 0) {
        return -1;
    }

    // Wait for data
    if (ide_wait_drq(base) != 0) {
        return -1;
    }

    // Read 256 words of identify data
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(base + IDE_REG_DATA);
    }

    // Extract information
    drive->exists = 1;
    drive->base = base;
    drive->ctrl = ctrl;
    drive->drive = drive_sel & 0x10 ? 1 : 0;

    // Get total sectors (LBA28)
    drive->sectors = (identify_data[61] << 16) | identify_data[60];

    // Get geometry (CHS)
    drive->cylinders = identify_data[1];
    drive->heads = identify_data[3];
    drive->sectors_per_track = identify_data[6];

    // Get model string (words 27-46, byte-swapped)
    for (int i = 0; i < 20; i++) {
        drive->model[i * 2] = (identify_data[27 + i] >> 8) & 0xFF;
        drive->model[i * 2 + 1] = identify_data[27 + i] & 0xFF;
    }
    drive->model[40] = '\0';

    // Trim trailing spaces from model
    for (int i = 39; i >= 0; i--) {
        if (drive->model[i] == ' ') {
            drive->model[i] = '\0';
        } else {
            break;
        }
    }

    // Get serial number (words 10-19, byte-swapped)
    for (int i = 0; i < 10; i++) {
        drive->serial[i * 2] = (identify_data[10 + i] >> 8) & 0xFF;
        drive->serial[i * 2 + 1] = identify_data[10 + i] & 0xFF;
    }
    drive->serial[20] = '\0';

    return 0;
}

// Initialize IDE subsystem
void ide_init(void) {
    printf("[IDE] Initializing IDE subsystem...\n");

    // Clear drive table
    for (int i = 0; i < IDE_MAX_DRIVES; i++) {
        ide_drives[i].exists = 0;
    }

    // Detect drives
    uint16_t channels[2][2] = {
        {IDE_PRIMARY_BASE, IDE_PRIMARY_CTRL},
        {IDE_SECONDARY_BASE, IDE_SECONDARY_CTRL}
    };

    int drive_idx = 0;

    for (int ch = 0; ch < 2; ch++) {
        uint16_t base = channels[ch][0];
        uint16_t ctrl = channels[ch][1];

        // Try master drive
        if (ide_identify(base, ctrl, IDE_DRIVE_MASTER | IDE_DRIVE_LBA, &ide_drives[drive_idx]) == 0) {
            printf("[IDE] Drive %d: %s (%d MB)\n",
                   drive_idx, ide_drives[drive_idx].model,
                   ide_drives[drive_idx].sectors / 2048);
            drive_idx++;
        } else {
            drive_idx++;
        }

        // Try slave drive
        if (ide_identify(base, ctrl, IDE_DRIVE_SLAVE | IDE_DRIVE_LBA, &ide_drives[drive_idx]) == 0) {
            printf("[IDE] Drive %d: %s (%d MB)\n",
                   drive_idx, ide_drives[drive_idx].model,
                   ide_drives[drive_idx].sectors / 2048);
            drive_idx++;
        } else {
            drive_idx++;
        }
    }

    printf("[IDE] Initialization complete\n");
}

// Read sectors from IDE drive
int ide_read_sectors(uint8_t drive_num, uint32_t lba, uint8_t count, uint8_t* buffer) {
    if (drive_num >= IDE_MAX_DRIVES || !ide_drives[drive_num].exists) {
        return -1;
    }

    ide_drive_t* drive = &ide_drives[drive_num];

    // Select drive and set LBA mode
    uint8_t drive_sel = (drive->drive == 0 ? IDE_DRIVE_MASTER : IDE_DRIVE_SLAVE) | IDE_DRIVE_LBA;
    drive_sel |= (lba >> 24) & 0x0F;  // LBA bits 24-27

    outb(drive->base + IDE_REG_DRIVE_SELECT, drive_sel);
    ide_delay(drive->ctrl);

    // Disable interrupts
    outb(drive->ctrl + IDE_REG_DEVICE_CTRL, IDE_CTRL_nIEN);

    // Set sector count and LBA
    outb(drive->base + IDE_REG_SECTOR_COUNT, count);
    outb(drive->base + IDE_REG_LBA_LO, lba & 0xFF);
    outb(drive->base + IDE_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(drive->base + IDE_REG_LBA_HI, (lba >> 16) & 0xFF);

    // Send read command
    outb(drive->base + IDE_REG_COMMAND, IDE_CMD_READ_SECTORS);
    ide_delay(drive->ctrl);

    // Read sectors
    for (int s = 0; s < count; s++) {
        // Wait for drive to be ready
        if (ide_wait_ready(drive->base) != 0) {
            return -1;
        }

        // Wait for data
        if (ide_wait_drq(drive->base) != 0) {
            return -1;
        }

        // Read 256 words (512 bytes)
        uint16_t* buf16 = (uint16_t*)(buffer + s * 512);
        for (int i = 0; i < 256; i++) {
            buf16[i] = inw(drive->base + IDE_REG_DATA);
        }
    }

    return 0;
}

// Write sectors to IDE drive
int ide_write_sectors(uint8_t drive_num, uint32_t lba, uint8_t count, const uint8_t* buffer) {
    if (drive_num >= IDE_MAX_DRIVES || !ide_drives[drive_num].exists) {
        return -1;
    }

    ide_drive_t* drive = &ide_drives[drive_num];

    // Select drive and set LBA mode
    uint8_t drive_sel = (drive->drive == 0 ? IDE_DRIVE_MASTER : IDE_DRIVE_SLAVE) | IDE_DRIVE_LBA;
    drive_sel |= (lba >> 24) & 0x0F;

    outb(drive->base + IDE_REG_DRIVE_SELECT, drive_sel);
    ide_delay(drive->ctrl);

    // Disable interrupts
    outb(drive->ctrl + IDE_REG_DEVICE_CTRL, IDE_CTRL_nIEN);

    // Set sector count and LBA
    outb(drive->base + IDE_REG_SECTOR_COUNT, count);
    outb(drive->base + IDE_REG_LBA_LO, lba & 0xFF);
    outb(drive->base + IDE_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(drive->base + IDE_REG_LBA_HI, (lba >> 16) & 0xFF);

    // Send write command
    outb(drive->base + IDE_REG_COMMAND, IDE_CMD_WRITE_SECTORS);
    ide_delay(drive->ctrl);

    // Write sectors
    for (int s = 0; s < count; s++) {
        // Wait for drive to be ready
        if (ide_wait_ready(drive->base) != 0) {
            return -1;
        }

        // Wait for data request
        if (ide_wait_drq(drive->base) != 0) {
            return -1;
        }

        // Write 256 words (512 bytes)
        const uint16_t* buf16 = (const uint16_t*)(buffer + s * 512);
        for (int i = 0; i < 256; i++) {
            outw(drive->base + IDE_REG_DATA, buf16[i]);
        }
    }

    // Flush cache
    outb(drive->base + IDE_REG_COMMAND, IDE_CMD_FLUSH_CACHE);
    ide_delay(drive->ctrl);
    ide_wait_ready(drive->base);

    return 0;
}

// Get drive information
ide_drive_t* ide_get_drive(uint8_t drive) {
    if (drive >= IDE_MAX_DRIVES) {
        return NULL;
    }

    if (!ide_drives[drive].exists) {
        return NULL;
    }

    return &ide_drives[drive];
}
