#include <drivers/device.h>
#include <xjos/string.h>
#include <xjos/task.h>
#include <xjos/assert.h>
#include <xjos/debug.h>
#include <xjos/arena.h>



#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define DEVICE_NR 64


static device_t devices[DEVICE_NR];


// get null device
static device_t *get_null_device() {
    for (size_t i = 1; i < DEVICE_NR; i++) {
        device_t *device = &devices[i];
        if (device->type == DEV_NULL)
            return device;
    }

    panic("no more devices!!!");
}


int device_ioctl(dev_t dev, int cmd, void *args, int flags) {
    device_t *device = device_get(dev);
    if (device->ioctl) {
        return device->ioctl(device->ptr, cmd, args, flags);
    }

    LOGK("ioctl of device %d not implemented!!!\n");
    return EOF;
}


int device_read(dev_t dev, void *buf, size_t count, idx_t idx, int flags) {
    device_t *device = device_get(dev);
    if (device->read) {
        return device->read(device->ptr, buf, count, idx, flags);
    }

    LOGK("read of device %d not implemented!!!\n");
    return EOF;
}


int device_write(dev_t dev, void *buf, size_t count, idx_t idx, int flags) {
    device_t *device = device_get(dev);
    if (device->write) {
        return device->write(device->ptr, buf, count, idx, flags);
    }
    
    LOGK("write of device %d not implemented!!!\n", dev);
    return EOF;
}


dev_t device_install(
    int type, int subtype,
    void *ptr, char *name, dev_t parent,
    void *ioctl, void *read, void *write) {
    device_t *device = get_null_device();
    device->ptr = ptr;
    device->parent = parent;
    device->type = type;
    device->subtype = subtype;
    strlcpy(device->name, name, NAMELEN);   // strlcpy maybe truncate 
    
    device->ioctl = ioctl;
    device->read = read;
    device->write = write;
    return device->dev;  
}


void device_init() {
    for (size_t i = 0; i < DEVICE_NR; i++) {
        device_t *device = &devices[i];
        strcpy((char *)device->name, "null");
        
        device->type = DEV_NULL;
        device->subtype = DEV_NULL;
        device->dev = i;
        device->parent = 0;
        device->ioctl = NULL;
        device->read = NULL;
        device->write = NULL;

        list_init(&device->request_list);
        device->direct = DIRECT_UP;
    }
}


device_t *device_find(int subtype, idx_t idx) {
    idx_t nr = 0;
    for (size_t i = 0; i < DEVICE_NR; i++) {
        device_t *device = &devices[i];
        if (device->subtype != subtype)
            continue;
        if (nr == idx)      // first match type, second match idx
            return device;
        nr++;
    }

    return NULL;
}


device_t *device_get(dev_t dev) {
    assert(dev < DEVICE_NR);
    device_t *device = &devices[dev];
    assert(device->type != DEV_NULL);

    return device;
}


static void do_request(request_t *req) {
    LOGK("dev %d do requset pba %d\n", req->dev, req->offset);

    switch (req->type) {
        case REQ_READ:
            device_read(req->dev, req->buf, req->count, req->offset, req->flags);
            break;
        case REQ_WRITE:
            device_write(req->dev, req->buf, req->count, req->offset, req->flags);
            break;
        default:
            panic("req type %d unknown!!!\n", req->dev);
            break;
    }
}


static request_t *request_nextreq(device_t *device, request_t *req) {
    list_t *list = &device->request_list;

    // change direction at the ends
    if (device->direct == DIRECT_UP && req->node.next == &list->head)
        device->direct = DIRECT_DOWN;   // change direction
    else if (device->direct == DIRECT_DOWN && req->node.prev == &list->head)
        device->direct = DIRECT_UP;     // change direction

    // get next req according to direction
    void *next = NULL;
    if (device->direct == DIRECT_UP)
        next = req->node.next;
    else
        next = req->node.prev;

    if (next == &list->head)
        return NULL;

    return list_entry(next, request_t, node);
}


void device_request(dev_t dev, void *buf, u8 count, idx_t idx, int flags, u32 type) {
    device_t *device = device_get(dev);
    assert(device->type == DEV_BLOCK);

    idx_t offset = idx + device_ioctl(device->dev, DEV_CMD_SECTOR_START, 0, 0);
    // get parent device, /dev/hda1 -> /dev/hda
    if (device->parent)   
        device = device_get(device->parent);
    
    request_t *req = kmalloc(sizeof(request_t));

    req->dev = device->dev;
    req->buf = buf;
    req->count = count;
    req->idx = idx;
    req->offset = offset;
    req->flags = flags;
    req->type = type;
    req->task = NULL;

    LOGK("dev %d requset idx %d\n", req->dev, req->idx);

    bool empty = list_empty(&device->request_list);

    // req to device reqlist
    // list_pushback(&device->request_list, &req->node);

    list_insert_sort(&device->request_list, &req->node, list_node_offset(request_t, node, offset));

    if (!empty) {   // wait for device idle
        req->task = running_task();
        task_block(req->task, NULL, TASK_BLOCKED);
    }

    do_request(req);    // do req

    request_t *nextreq = request_nextreq(device, req);
    list_remove(&req->node);    // remove req from device reqlist

    kfree(req);   // free req

    if (nextreq) {
        assert(nextreq->task->magic == XJOS_MAGIC);
        task_unblock(nextreq->task);   // wake up next req task
    }
}