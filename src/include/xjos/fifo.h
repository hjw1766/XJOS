#ifndef XJOS_FIFO_H_
#define XJOS_FIFO_H_

#include <xjos/types.h>
#include <xjos/assert.h>

typedef struct {
    char *buf;
    u32 size;       // buffer length
    u32 head;       // write pointer
    u32 tail;       // read pointer
} fifo_t;

static _inline void fifo_init(fifo_t *fifo, char *buf, u32 length);
static _inline bool fifo_full(fifo_t *fifo);
static _inline bool fifo_empty(fifo_t *fifo);
static _inline char fifo_get(fifo_t *fifo);
static _inline void fifo_put(fifo_t *fifo, char byte);

static _inline u32 fifo_next(fifo_t *fifo, u32 pos) {
    return (pos + 1) % fifo->size;
}


static _inline void fifo_init(fifo_t *fifo, char *buf, u32 length) {
    fifo->buf = buf;
    fifo->size = length;
    fifo->head = 0;
    fifo->tail = 0;
}


static _inline bool fifo_full(fifo_t *fifo) {
    bool full = (fifo_next(fifo, fifo->head) == fifo->tail);
    return full;
}


static _inline bool fifo_empty(fifo_t *fifo) {
    return (fifo->head == fifo->tail);
}


static _inline char fifo_get(fifo_t *fifo) {
    assert(!fifo_empty(fifo));
    char byte = fifo->buf[fifo->tail];  // cycling queue
    fifo->tail = fifo_next(fifo, fifo->tail);
    return byte;
}


static _inline void fifo_put(fifo_t *fifo, char byte) {
    while (fifo_full(fifo)) {
        fifo_get(fifo);
    }

    fifo->buf[fifo->head] = byte;
    fifo->head = fifo_next(fifo, fifo->head);
}



#endif /* XJOS_FIFO_H_ */