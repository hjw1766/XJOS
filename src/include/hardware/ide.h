#ifndef IDE_H_
#define IDE_H_

#include <xjos/types.h>
#include <xjos/mutex.h>


#define SECTOR_SIZE 512 

#define IDE_CTRL_NR 2
#define IDE_DISK_NR 2
#define IDE_PART_NR 4   // mbr max partition number

typedef struct part_entry_t {
    u8 bootable;           // bootable flag
    u8 start_head;         // start head
    u8 start_sector : 6;    // start sector
    u16 start_cylinder : 10; // start cylinder
    u8 system;              // system id
    u8 end_head;           // end head
    u8 end_sector : 6;      // end sector
    u16 end_cylinder : 10;   // end cylinder
    u32 start;              // start lba
    u32 count;              // use sector count
}_packed part_entry_t;

typedef struct boot_sector_t {
    u8 code[446];           // boot code
    part_entry_t entry[4];  // partition table
    u16 signature;          // 55aa
}_packed boot_sector_t;

typedef struct ide_part_t {
    char name[8];               // partition name
    struct ide_disk_t *disk;    // disk pointer
    u32 system;                 // system type
    u32 start;                  // start lba 
    u32 count;                  // use sector count
}ide_part_t;

typedef struct ide_disk_t {
    char name[8];               // disk name
    struct ide_ctrl_t *ctrl;    // ctrl pointer
    u8 selector;                // disk select
    bool master;                // master disk
    u32 total_lba;               // total lba count
    u32 cylinders;               // cylinder count
    u32 heads;                   // head count
    u32 sectors;                 // sector count
    u32 interface;             // 0: PIO, 1: DMA, 2: LBA48
    ide_part_t parts[IDE_PART_NR]; // disk partition
}ide_disk_t;

typedef struct ide_ctrl_t {
    char name[8];                   // ctrl name
    mutex_t lock;                   // lock
    u16 iobase;                     // IO reg base
    ide_disk_t disks[IDE_DISK_NR];  // disk
    ide_disk_t *active;             // current select disk
    u8 devsel;                      // cached HDDEVSEL value
    u8 control;                     // control Byte
    task_t *waiter;          // waiting task
}ide_ctrl_t;

int ide_pio_read(ide_disk_t *disk, void *buf, u8 count, idx_t lba);
int ide_pio_write(ide_disk_t *disk, void *buf, u8 count, idx_t lba);

#endif /* IDE_H_ */