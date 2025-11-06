#ifndef XJOS_DEVICE_H
#define XJOS_DEVICE_H


#include <xjos/types.h>
#include <xjos/list.h>

#define NAMELEN 16

// device type
enum device_type_t {
    DEV_NULL,       // empty device
    DEV_CHAR,       // character device
    DEV_BLOCK       // block device
};

// device child type
enum device_subtype_t {
    DEV_CONSOLE = 1,    // console
    DEV_KEYBOARD,       // keyboard
    DEV_IDE_DISK,       // IDE disk
    DEV_IDE_PART        // IDE disk part
};

// device commands
enum device_cmd_t {
    DEV_CMD_SECTOR_START = 1,   // get dev start sector
    DEV_CMD_SECTOR_SIZE,        // get dev sector size
};

#define REQ_READ 0
#define REQ_WRITE 1

// block device request
typedef struct request_t {
    dev_t dev;
    u32 type;
    u32 idx;
    u32 count;
    int flags;
    u8 *buf;
    struct task_t *task;        // req process
    list_node_t node;          // list node
}request_t;

typedef struct device_t {
    char name[NAMELEN];
    int type;
    int subtype;
    dev_t dev;          // device number
    dev_t parent;       // parent device number
    void *ptr;          // pointer to device
    list_t request_list;    // block dev req list

    // device control
    int (*ioctl)(void *dev, int cmd, void *args, int flags);
    // read
    int (*read)(void *dev, void *buf, size_t count, idx_t idx, int flags); 
    // write
    int (*write)(void *dev, void *buf, size_t count, idx_t idx, int flags);
}device_t;


// install
dev_t device_install(
    int type, int subtype,
    void *ptr, char *name, dev_t parent,
    void *ioctl, void *read, void *write);

// According to subtype find the device
device_t *device_find(int subtype, idx_t idx);

// According to device number find the device
device_t *device_get(dev_t dev);

// control device
int device_ioctl(dev_t dev, int cmd, void *args, int flags);

// read device
int device_read(dev_t dev, void *buf, size_t count, idx_t idx, int flags);

// write device
int device_write(dev_t dev, void *buf, size_t count, idx_t idx, int flags);

// block dev req
void device_request(dev_t dev, void *buf, u8 count, idx_t idx, int flags, u32 type);

#endif /* XJOS_DEVICE_H */