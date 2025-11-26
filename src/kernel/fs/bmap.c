#include <fs/fs.h>
#include <fs/buffer.h>
#include <xjos/debug.h>
#include <xjos/bitmap.h>
#include <libc/assert.h>
#include <libc/string.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


// alloc file block
idx_t balloc(dev_t dev) {
    super_block_t *sb = get_super(dev);
    assert(sb);

    buffer_t *buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for (size_t i = 0; i < ZMAP_NR; i++) {
        buf = sb->zmaps[i];
        assert(buf);

        // zone map 2 blk, map
        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS + sb->desc->firstdatazone - 1);

        // scan bit
        bit = bitmap_scan(&map, 1);
        if (bit != EOF) {
            assert(bit < sb->desc->zones);
            buf->dirty = true;
            break;
        }
    }
    bwrite(buf);        // todo
    return bit;
}


// free file blk
void bfree(dev_t dev, idx_t idx) {
    super_block_t *sb = get_super(dev);
    assert(sb != NULL);
    assert(idx < sb->desc->zones);

    buffer_t *buf = NULL;
    bitmap_t map;
    for (size_t i = 0; i < ZMAP_NR; i++) {
        if (idx > BLOCK_BITS * (i + 1))
            continue;
        
        buf = sb->zmaps[i];
        assert(buf);

        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS + sb->desc->firstdatazone - 1);

        // idx - set 0
        assert(bitmap_test(&map, idx));
        bitmap_set(&map, idx, 0);

        buf->dirty = true;
        break;
    }
    bwrite(buf);        // todo
}


// alloc inode
idx_t ialloc(dev_t dev) {
    super_block_t *sb = get_super(dev);
    assert(sb);

    buffer_t *buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for (size_t i = 0; i < IMAP_NR; i++) {
        buf = sb->imaps[i];
        assert(buf);

        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS);
        bit = bitmap_scan(&map, 1);
        if (bit != EOF) {
            assert(bit < sb->desc->inodes);
            buf->dirty = true;
            break;
        }
    }
    bwrite(buf);        // todo
    return bit;
}


// free inode
void ifree(dev_t dev, idx_t idx) {
    super_block_t *sb = get_super(dev);
    assert(sb != NULL);
    assert(idx < sb->desc->inodes);

    buffer_t *buf = NULL;
    bitmap_t map;
    for (size_t i = 0; i < IMAP_NR; i++) {
        if (idx > BLOCK_BITS * (i + 1))
            continue;
        
        buf = sb->imaps[i];
        assert(buf);

        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS);

        // idx - set 0
        assert(bitmap_test(&map, idx));
        bitmap_set(&map, idx, 0);

        buf->dirty = true;
        break;
    }
    bwrite(buf);        // todo
}