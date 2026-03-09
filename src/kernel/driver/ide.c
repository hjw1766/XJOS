#include <hardware/ide.h>
#include <hardware/io.h>
#include <xjos/printk.h>
#include <xjos/stdio.h>
#include <xjos/memory.h>
#include <xjos/interrupt.h>
#include <xjos/task.h>
#include <xjos/string.h>
#include <xjos/debug.h>
#include <xjos/assert.h>
#include <drivers/device.h>
#include <xjos/timer.h>
#include <xjos/errno.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define IDE_TIMEOUT 60000

// IDE Reg Addresses
#define IDE_IOBASE_PRIMARY 0x1F0    // master
#define IDE_IOBASE_SECONDARY 0x170  // slave

// IDE Register Offsets (relative to iobase, e.g., 0x1F0)
#define IDE_DATA 0x0000     // Data Register (Read/Write data)
#define IDE_ERR 0x0001      // Error Register (Read)
#define IDE_FEATURE 0x0001  // Feature Register (Write)
#define IDE_SECTOR 0x0002   // Sector Count Register
#define IDE_LBA_LOW 0x0003  // LBA Low Byte Register
#define IDE_CHS_SECTOR 0x0003 // CHS Sector location
#define IDE_LBA_MID 0x0004  // LBA Mid Byte Register
#define IDE_CHS_CYL 0x0004    // CHS Cylinder low byte
#define IDE_LBA_HIGH 0x0005 // LBA High Byte Register
#define IDE_CHS_CYH 0x0005    // CHS Cylinder high byte
#define IDE_HDDEVSEL 0x0006 // Drive/Head Select Register
#define IDE_STATUS 0x0007   // Status Register (Read)
#define IDE_COMMAND 0x0007  // Command Register (Write)

// These are on the second IO range (iobase + 0x206)
#define IDE_ALT_STATUS 0x0206 // Alternate Status Register (Read)
#define IDE_CONTROL 0x0206    // Device Control Register (Write)
#define IDE_DEVCTRL 0x0206    // (Alias for Device Control)

// IDE Commands (written to IDE_COMMAND register)
#define IDE_CMD_READ 0x20     // Read Sectors
#define IDE_CMD_WRITE 0x30    // Write Sectors
#define IDE_CMD_IDENTIFY 0xEC // Identify Drive
#define IDE_CMD_DIAGNOSTIC 0x90 // Run Diagnostics

// IDE Status Register Bits (read from IDE_STATUS or IDE_ALT_STATUS)
#define IDE_SR_NULL 0x00 // NULL
#define IDE_SR_ERR 0x01  // Error
#define IDE_SR_IDX 0x02  // Index
#define IDE_SR_CORR 0x04 // Corrected data
#define IDE_SR_DRQ 0x08  // Data Request (Ready to transfer data)
#define IDE_SR_DSC 0x10  // Drive seek complete
#define IDE_SR_DWF 0x20  // Drive write fault
#define IDE_SR_DRDY 0x40 // Drive ready (Ready for command)
#define IDE_SR_BSY 0x80  // Controller busy

// IDE Control Register Bits (written to IDE_CONTROL)
#define IDE_CTRL_HD15 0x00 // Use 4 bits for head (not used, was 0x08)
#define IDE_CTRL_SRST 0x04 // Soft reset
#define IDE_CTRL_NIEN 0x02 // Disable interrupts

// IDE Error Register Bits (read from IDE_ERR)
#define IDE_ER_AMNF 0x01  // Address mark not found
#define IDE_ER_TK0NF 0x02 // Track 0 not found
#define IDE_ER_ABRT 0x04  // Abort
#define IDE_ER_MCR 0x08   // Media change requested
#define IDE_ER_IDNF 0x10  // Sector id not found
#define IDE_ER_MC 0x20    // Media change
#define IDE_ER_UNC 0x40   // Uncorrectable data error
#define IDE_ER_BBK 0x80   // Bad block

// Values for IDE_HDDEVSEL register (Drive Select)
#define IDE_LBA_MASTER 0b11100000 // LBA Mode, Master Drive (0xE0)
#define IDE_LBA_SLAVE 0b11110000  // LBA Mode, Slave Drive (0xF0)
#define IDE_SEL_MASK 0b10110000   // CHS Mode MASK

#define IDE_INTERFACE_UNKNOWN 0
#define IDE_INTERFACE_ATA 1
#define IDE_INTERFACE_ATAPI 2

typedef enum PART_FS {
    PART_FS_FAT12 = 1,      // fat 12
    PART_FS_EXTENDED = 5,   // extend part
    PART_FS_MINIX = 0x80,   // minux
    PART_FS_LINUX = 0x83    // linux
}PART_FS;

/*
 * This structure maps the 512-byte data block returned by
 * the ATA IDENTIFY DEVICE command. The comments indicate the
 * word offset (1 word = 2 bytes) as per the specification.
 */
typedef struct ide_params_t
{
    u16 config;                     // 0: General configuration bits
    u16 cylinders;                  // 01: Number of cylinders
    u16 RESERVED;                   // 02: Reserved
    u16 heads;                      // 03: Number of heads
    u16 RESERVED[5 - 3];            // 04-05: Reserved (vendor specific)
    u16 sectors;                    // 06: Sectors per track
    u16 RESERVED[9 - 6];            // 07-09: Reserved (vendor specific)
    u8 serial[20];                  // 10-19: Serial number (ASCII)
    u16 RESERVED[22 - 19];          // 20-22: Reserved
    u8 firmware[8];                 // 23-26: Firmware version (ASCII)
    u8 model[40];                   // 27-46: Model number (ASCII)
    u8 drq_sectors;                 // 47: Max sectors per DRQ (older drives)
    u8 RESERVED[3];                 // 48: Reserved
    u16 capabilities;               // 49: Capabilities (e.g., LBA support)
    u16 RESERVED[59 - 49];          // 50-59: Reserved
    u32 total_lba;                  // 60-61: Total number of LBA sectors (if LBA supported)
    u16 RESERVED;                   // 62: Reserved
    u16 mdma_mode;                  // 63: Multiword DMA mode
    u8 RESERVED;                    // 64: Reserved
    u8 pio_mode;                    // 64: PIO mode
    u16 RESERVED[79 - 64];          // 65-79: Reserved (See ATA specification)
    u16 major_version;              // 80: Major version number
    u16 minor_version;              // 81: Minor version number
    u16 commmand_sets[87 - 81];     // 82-87: Supported command sets
    u16 RESERVED[118 - 87];         // 88-118: Reserved
    u16 support_settings;           // 119: Supported settings
    u16 enable_settings;            // 120: Enabled settings
    u16 RESERVED[221 - 120];        // 121-221: Reserved
    u16 transport_major;            // 222: Transport major version
    u16 transport_minor;            // 223: Transport minor version
    u16 RESERVED[254 - 223];        // 224-254: Reserved
    u16 integrity;                  // 255: Checksum / Integrity word
} _packed ide_params_t;


ide_ctrl_t controllers[IDE_CTRL_NR];

static int ide_reset_controller(ide_ctrl_t *ctrl);

static void ide_handler(int vector) {
    send_eoi(vector);

    // exp. vector = 0x20 + 0xe = 0x2e, 0x2e - 0x20 - 0xe = 0
    ide_ctrl_t *ctrl = &controllers[vector - IRQ_HARDDISK - 0x20];

    // clear IRQ
    u8 state = inb(ctrl->iobase + IDE_STATUS);
    MM_TRACEK("harddisk interrupt vector %d state 0x%x\n", vector, state);

    if (ctrl->waiter) {
        // have process waiter
        task_unblock(ctrl->waiter, EOK);
        ctrl->waiter = NULL;
    }
}


// disk delay
static void ide_delay() {
    task_sleep(1);     // wait for 25ms, enough for most operations to complete
}


static void ide_error(ide_ctrl_t *ctrl) {
    u8 error = inb(ctrl->iobase + IDE_ERR);
    
    if (error & IDE_ER_BBK)
        LOGK("bad block\n");
    if (error & IDE_ER_UNC)
        LOGK("uncorrectable data\n");
    if (error & IDE_ER_MC)
        LOGK("media change\n");
    if (error & IDE_ER_IDNF)
        LOGK("id not found\n");
    if (error & IDE_ER_MCR)
        LOGK("media change requested\n");
    if (error & IDE_ER_ABRT)
        LOGK("abort\n");
    if (error & IDE_ER_TK0NF)
        LOGK("track 0 not found\n");
    if (error & IDE_ER_AMNF)
        LOGK("address mark not found\n");
}


// c -> asm .wait
static err_t ide_busy_wait(ide_ctrl_t *ctrl, u8 mask, int timeout_ms) {
    int expires = timer_expire_jiffies(timeout_ms);

    while (true) {
        if (timeout_ms > 0 && timer_is_expires(expires)) {
            LOGK("ide busy wait timeout\n");
            return -ETIME;
        }

        u8 state = inb(ctrl->iobase + IDE_ALT_STATUS);
        if (state & IDE_SR_ERR) { // error
            ide_error(ctrl);
            ide_reset_controller(ctrl);
            return -EIO;
        }
        
        if (state & IDE_SR_BSY) { // dv busy
            ide_delay();
            continue;
        }
        
        if ((state & mask) == mask) // wait state done
            return EOK;
    }      
}


// disk reset
static err_t ide_reset_controller(ide_ctrl_t *ctrl) {
    outb(ctrl->iobase + IDE_CONTROL, IDE_CTRL_SRST);
    ide_delay();
    outb(ctrl->iobase + IDE_CONTROL, ctrl->control);
    return ide_busy_wait(ctrl, IDE_SR_NULL, IDE_TIMEOUT);
}


static void ide_select_device(ide_disk_t *disk, u8 devsel) {
    ide_ctrl_t *ctrl = disk->ctrl;
    if (ctrl->active == disk && ctrl->devsel == devsel)
        return;

    outb(ctrl->iobase + IDE_HDDEVSEL, devsel);
    ctrl->active = disk;
    ctrl->devsel = devsel;
}


// select disk
static void ide_select_drive(ide_disk_t *disk) {
    ide_select_device(disk, disk->selector);
}


// select sector
static void ide_select_sector(ide_disk_t *disk, u32 lba, u8 count) {
    ide_ctrl_t *ctrl = disk->ctrl;

    // functional reg
    outb(ctrl->iobase + IDE_FEATURE, 0);

    // 0x1F2 <- count
    outb(ctrl->iobase + IDE_SECTOR, count);
    
    // LBA address 0-23
    outb(ctrl->iobase + IDE_LBA_LOW, lba & 0xFF);
    outb(ctrl->iobase + IDE_LBA_MID, (lba >> 8) & 0xFF);
    outb(ctrl->iobase + IDE_LBA_HIGH, (lba >> 16) & 0xFF);
}


// read -> buf
static void ide_pio_read_sector(ide_disk_t *disk, u16 *buf) {
    u16 port = disk->ctrl->iobase + IDE_DATA;
    asm volatile("cld; rep insw" : "+D"(buf) : "d"(port), "c"(SECTOR_SIZE / 2) : "memory");
}


// write -> buf
static void ide_pio_write_sector(ide_disk_t *disk, u16 *buf) {
    u16 port = disk->ctrl->iobase + IDE_DATA;
    asm volatile("cld; rep outsw" : : "S"(buf), "d"(port), "c"(SECTOR_SIZE / 2) : "memory");
}


// disk control
int ide_pio_ioctl(ide_disk_t *disk, int cmd, void *args, int flags) {
    switch (cmd) {
        case DEV_CMD_SECTOR_START:
            return 0;
        case DEV_CMD_SECTOR_SIZE:
            return disk->total_lba;
        default:
            panic("device command %d can't recognized\n");
            break;
    }
}


int ide_pio_read(ide_disk_t *disk, void *buf, u8 count, idx_t lba) {
    assert(count > 0);
    assert(!get_interrupt_state());     // interrupts must be disabled

    ide_ctrl_t *ctrl = disk->ctrl;

    mutex_lock(&ctrl->lock);

    int ret = -EIO;

    // lock -  select disk - wait - select sector
    // send read cmd - unlock
    ide_select_device(disk, ((lba >> 24) & 0xf) | disk->selector);

    if((ret = ide_busy_wait(ctrl, IDE_SR_DRDY, IDE_TIMEOUT)) < EOK)
        goto rollback;

    ide_select_sector(disk, lba, count);

    outb(ctrl->iobase + IDE_COMMAND, IDE_CMD_READ);
    
    task_t *task = running_task();
    for (size_t i = 0; i < count; i++) {
        ctrl->waiter = task;
        if ((ret = task_block(task, NULL, TASK_BLOCKED, IDE_TIMEOUT)) < EOK)
            goto rollback;

        // DRQ, cpu ready to receive data
        if ((ret = ide_busy_wait(ctrl, IDE_SR_DRQ, IDE_TIMEOUT)) < EOK)
            goto rollback;
        // sector i
        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ide_pio_read_sector(disk, (u16 *)offset);
    }
    ret = EOK;

rollback:
    mutex_unlock(&ctrl->lock);

    return ret;
}


int ide_pio_write(ide_disk_t *disk, void *buf, u8 count, idx_t lba) {
    assert(count > 0);
    assert(!get_interrupt_state());

    ide_ctrl_t *ctrl = disk->ctrl;

    mutex_lock(&ctrl->lock);

    int ret = EOK;

    MM_TRACEK("write lab 0x%x\n", lba);

    ide_select_device(disk, ((lba >> 24) & 0xf) | disk->selector);
    if((ret = ide_busy_wait(ctrl, IDE_SR_DRDY, IDE_TIMEOUT)) < EOK)
        goto rollback;
    ide_select_sector(disk, lba, count);
    outb(ctrl->iobase + IDE_COMMAND, IDE_CMD_WRITE);

    task_t *task = running_task();
    for (size_t i = 0; i < count; i++) {
        
        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ide_pio_write_sector(disk, (u16 *)offset);
        
        ctrl->waiter = task;
        if ((ret = task_block(task, NULL, TASK_BLOCKED, IDE_TIMEOUT)) < EOK)
            goto rollback;

        // wait for BSY = 1
        if ((ret = ide_busy_wait(ctrl, IDE_SR_NULL, IDE_TIMEOUT)) < EOK)
            goto rollback;
    }
    ret = EOK;

rollback:    
    mutex_unlock(&ctrl->lock);

    return ret;
}


// part control
int ide_pio_part_ioctl(ide_part_t *part, int cmd, void *args, int flags) {
    switch (cmd) {
        case DEV_CMD_SECTOR_START:
            return part->start;
        case DEV_CMD_SECTOR_SIZE:
            return part->count;
        default:
            panic("device command %d can't recognized\n");
            break;
    }
}


// read partiton
int ide_pio_part_read(ide_part_t *part, void *buf, u8 count, idx_t lba) {
    return ide_pio_read(part->disk, buf, count, lba);
}


// write partiton
int ide_pio_part_wrtie(ide_part_t *part, void *buf, u8 count, idx_t lba) {
    return ide_pio_write(part->disk, buf, count, lba);
}


// detect dev
static err_t ide_probe_device(ide_disk_t *disk) {
    outb(disk->ctrl->iobase + IDE_HDDEVSEL, disk->selector & IDE_SEL_MASK);
    ide_delay();

    outb(disk->ctrl->iobase + IDE_SECTOR, 0x55);
    outb(disk->ctrl->iobase + IDE_CHS_SECTOR, 0xAA);

    outb(disk->ctrl->iobase + IDE_SECTOR, 0xAA);
    outb(disk->ctrl->iobase + IDE_CHS_SECTOR, 0x55);

    outb(disk->ctrl->iobase + IDE_SECTOR, 0x55);
    outb(disk->ctrl->iobase + IDE_CHS_SECTOR, 0xAA);

    u8 sector_count = inb(disk->ctrl->iobase + IDE_SECTOR);
    u8 sector_index = inb(disk->ctrl->iobase + IDE_CHS_SECTOR);

    if (sector_count == 0x55 && sector_index == 0xAA)
        return EOK;
    return -EIO;
}


// check disk interface type
static int ide_interface_type(ide_disk_t *disk) {
    outb(disk->ctrl->iobase + IDE_COMMAND, IDE_CMD_DIAGNOSTIC);
    if(ide_busy_wait(disk->ctrl, IDE_SR_NULL, IDE_TIMEOUT) < EOK)
        return IDE_INTERFACE_UNKNOWN;

    outb(disk->ctrl->iobase + IDE_HDDEVSEL, disk->selector & IDE_SEL_MASK);
    ide_delay();

    u8 sector_count = inb(disk->ctrl->iobase + IDE_SECTOR);
    u8 sector_index = inb(disk->ctrl->iobase + IDE_LBA_LOW);
    if (sector_count != 1 || sector_index != 1)
        return IDE_INTERFACE_UNKNOWN;

    u8 cylinder_low = inb(disk->ctrl->iobase + IDE_CHS_CYL);
    u8 cylinder_high = inb(disk->ctrl->iobase + IDE_CHS_CYH);
    u8 state = inb(disk->ctrl->iobase + IDE_STATUS);

    if (cylinder_low == 0x14 && cylinder_high == 0xeb)
        return IDE_INTERFACE_ATAPI;

    if (cylinder_low == 0 && cylinder_high == 0 && state != 0)
        return IDE_INTERFACE_ATA;

    return IDE_INTERFACE_UNKNOWN;
}


// Big-endian to little-endian
static void ide_fixstrings(char *buf, u32 len) {
    for (size_t i = 0; i < len; i += 2) {
        // swap pairs of bytes
        register char ch = buf[i];
        buf[i] = buf[i + 1];
        buf[i + 1] = ch;
    }

    buf[len - 1] = '\0';
}


// Identify disk
static err_t ide_identify(ide_disk_t *disk, u16 *buf) {
    LOGK("identifing disk %s...\n", disk->name);

    /*
        lock - select disk - wait - send identify cmd - wait - read data - unlock
    */
    mutex_lock(&disk->ctrl->lock);
    ide_select_drive(disk);

    int ret = EOK;

    outb(disk->ctrl->iobase + IDE_COMMAND, IDE_CMD_IDENTIFY);
    if(ide_busy_wait(disk->ctrl, IDE_SR_NULL, IDE_TIMEOUT) < EOK)
        goto rollback;
    
    ide_params_t *params = (ide_params_t *)buf;
    // read 512 bytes -> buf
    ide_pio_read_sector(disk, buf);

    ide_fixstrings(params->serial, sizeof(params->serial));
    LOGK("disk %s serial number %s\n", disk->name, params->serial);

    ide_fixstrings(params->firmware, sizeof(params->firmware));
    LOGK("disk %s firmware version %s\n", disk->name, params->firmware);

    ide_fixstrings(params->model, sizeof(params->model));
    LOGK("disk %s model number %s\n", disk->name, params->model);

    if (params->total_lba == 0) {
        ret = -EIO;
        goto rollback;
    }
    LOGK("disk %s total lba %d\n", disk->name, params->total_lba);


    disk->total_lba = params->total_lba;
    disk->cylinders = params->cylinders;
    disk->heads = params->heads;
    disk->sectors = params->sectors;
    ret = EOK;

rollback:
    mutex_unlock(&disk->ctrl->lock);
    return ret;
}


static void ide_part_init(ide_disk_t *disk, u16 *buf) {
    // disk died
    if (!disk->total_lba)
        return;

    // read mbr
    ide_pio_read(disk, buf, 1, 0);

    boot_sector_t *boot = (boot_sector_t *)buf;

    for (size_t i = 0; i < IDE_PART_NR; i++) {
        part_entry_t *entry = &boot->entry[i];
        ide_part_t *part = &disk->parts[i];

        if (!entry->count)
            continue;

        sprintf(part->name, "%s%d", disk->name, i + 1);
        LOGK("part %s \n", part->name);
        LOGK("      bootable %d\n", entry->bootable);
        LOGK("      start lba %d\n", entry->start);
        LOGK("      count %d\n", entry->count);
        LOGK("      system 0x%x\n", entry->system);

        part->disk = disk;
        part->count = entry->count;
        part->system = entry->system;
        part->start = entry->start;

        if (entry->system == PART_FS_EXTENDED) {
            LOGK("Unsupported extended partition!!!\n");

            boot_sector_t *eboot = (boot_sector_t *)(buf + SECTOR_SIZE);
            ide_pio_read(disk, (void *)eboot, 1, entry->start);

            for (size_t j = 0; j < IDE_PART_NR; j++) {
                part_entry_t *eentry = &eboot->entry[j];
                if (!eentry->count)
                    continue;

                LOGK("part %d extend %d\n", i, j);
                LOGK("      bootable %d\n", eentry->bootable);
                LOGK("      start lba %d\n", eentry->start + entry->start);
                LOGK("      count %d\n", eentry->count);
                LOGK("      system 0x%x\n", eentry->system);
            }
        }
    }
}


static void ide_ctrl_init() {
    // init controller
    u16 *buf = (u16 *)alloc_kpage(1);
    for (size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++) {
        ide_ctrl_t *ctrl = &controllers[cidx];
        sprintf(ctrl->name, "ide%u", cidx); // ide0 ide1
        mutex_init(&ctrl->lock);
        ctrl->active = NULL;
        ctrl->devsel = 0xFF;
        ctrl->waiter = NULL;

        if (cidx) {
            ctrl->iobase = IDE_IOBASE_SECONDARY;
        } else {
            ctrl->iobase = IDE_IOBASE_PRIMARY;
        }

        ctrl->control = inb(ctrl->iobase + IDE_CONTROL);

        for (size_t didx = 0; didx < IDE_DISK_NR; didx++) {
            ide_disk_t *disk = &ctrl->disks[didx];
            // hda, hdb...
            sprintf(disk->name, "hd%c", 'a' + cidx * 2 + didx);
            
            // hda ctrl -> ide0
            disk->ctrl = ctrl;

            if (didx) {
                disk->master = false;
                disk->selector = IDE_LBA_SLAVE;
            } else {
                disk->master = true;
                disk->selector = IDE_LBA_MASTER;
            }

            if (ide_probe_device(disk) < 0) {
                LOGK("IDE device %s not exists...\n", disk->name);
                continue;
            }

            disk->interface = ide_interface_type(disk);
            LOGK("IDE device %s type %d...\n", disk->name, disk->interface);
            if (disk->interface == IDE_INTERFACE_UNKNOWN)
                continue;

            if (disk->interface == IDE_INTERFACE_ATA) {
                ide_identify(disk, buf);
                ide_part_init(disk, buf);
            } else if (disk->interface == IDE_INTERFACE_ATAPI) {
                LOGK("Disk %s interface is ATAPI\n", disk->name);
            }
        }
    }
    free_kpage((u32)buf, 1);
}


static void ide_install() {
    for (size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++) {
        ide_ctrl_t *ctrl = &controllers[cidx];
        for (size_t didx = 0; didx < IDE_DISK_NR; didx++) {
            ide_disk_t *disk = &ctrl->disks[didx];

            if (!disk->total_lba)   // disk died
                continue;
            if (disk->interface == IDE_INTERFACE_ATA) {
                dev_t dev = device_install(
                    DEV_BLOCK, DEV_IDE_DISK, 
                    disk, disk->name, 0, 
                    ide_pio_ioctl, ide_pio_read,
                    ide_pio_write);
            
            for (size_t i = 0; i < IDE_PART_NR; i++) {
                ide_part_t *part = &disk->parts[i];
                if (!part->count)
                    continue;
                device_install(DEV_BLOCK, DEV_IDE_PART, 
                    part, part->name, dev, 
                    ide_pio_part_ioctl, ide_pio_part_read, 
                    ide_pio_part_wrtie);
            }     
        }
        }
    }
}


void ide_init() {
    LOGK("ide init...\n");
    
    // register int
    set_interrupt_handler(IRQ_HARDDISK, ide_handler);
    set_interrupt_handler(IRQ_HARDDISK2, ide_handler);
    set_interrupt_mask(IRQ_HARDDISK, true);
    set_interrupt_mask(IRQ_HARDDISK2, true);
    set_interrupt_mask(IRQ_CASCADE, true);
    
    ide_ctrl_init();

    ide_install();
}