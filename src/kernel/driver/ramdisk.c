#include <xjos/memory.h>
#include <xjos/string.h>
#include <drivers/device.h>
#include <xjos/stdio.h>
#include <xjos/assert.h>
#include <xjos/debug.h>

#define SECTOR_SIZE 512

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define RAMDISK_NR 4

typedef struct ramdisk_t {
    u8 *start;    // Pointer to the start of the ramdisk memory
    u32 size;     // Size of the ramdisk in bytes
} ramdisk_t;

static ramdisk_t ramdisks[RAMDISK_NR];

int ramdisk_ioctl(ramdisk_t *disk, int cmd, void *args, int flags) {
    switch (cmd) {
        case DEV_CMD_SECTOR_START:
            return 0;
        case DEV_CMD_SECTOR_SIZE:
            return disk->size / SECTOR_SIZE;
        default:
            panic("device command %d can't recognized!!!\n", cmd);
            break;
    }
}

int ramdisk_read(ramdisk_t *disk, void *buf, u8 count, idx_t lba) {
    void *addr = disk->start + lba * SECTOR_SIZE;
    u32 len = count * SECTOR_SIZE;
    assert(((u32)addr + len) < ((u32)disk->start + disk->size));
    memcpy(buf, addr, len);
    return count;
}

int ramdisk_write(ramdisk_t *disk, void *buf, u8 count, idx_t lba) {
    void *addr = disk->start + lba * SECTOR_SIZE;
    u32 len = count * SECTOR_SIZE;
    assert(((u32)addr + len) < ((u32)disk->start + disk->size));
    memcpy(addr, buf, len);
    return count;
}

void ramdisk_init() {
    LOGK("ramdisk init...\n");

    u32 size = KERNEL_RAMDISK_SIZE / RAMDISK_NR;
    assert(size % SECTOR_SIZE == 0);

    char name[32];
    
    for (size_t i = 0; i < RAMDISK_NR; i++) {
        ramdisk_t *ramdisk = &ramdisks[i];
        ramdisk->start = (u8 *)(KERNEL_RAMDISK_MEM + size * i);
        ramdisk->size = size;
        sprintf(name, "md%c", i + 'a');
        device_install(DEV_BLOCK, DEV_RAMDISK, ramdisk, name, 0,
            ramdisk_ioctl, ramdisk_read, ramdisk_write);
    }
}