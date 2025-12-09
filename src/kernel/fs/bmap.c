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
}


// get inode -> .block, if create and not exist, alloc blk
idx_t bmap(inode_t *inode, idx_t block, bool create) {
    assert(block >= 0 && block < TOTAL_BLOCK);

    u16 index = block;

    u16 *array = inode->desc->zones;

    buffer_t *buf = inode->buf;
    buf->count += 1;

    int level = 0;      // 0-direct,1-indirect,2-double indirect
    int divider = 1;

    // CASE A: direct block (0 ~ 6)
    if (block < DIRECT_BLOCK) {
        // in inode->zones[0-6]
        goto reckon;
    }

    block -= DIRECT_BLOCK;
    // CASE B: indirect block (7 ~ 7 + 512)
    if (block < INDIRECT1_BLOCK) {
        // inode->zones[7]
        index = DIRECT_BLOCK;
        level = 1;
        divider = 1;
        goto reckon;
    }

    block -= INDIRECT1_BLOCK;
    // CASE C: double indirect block
    assert(block < INDIRECT2_BLOCK);

    // inode->zones[8]
    index = DIRECT_BLOCK + 1;
    level = 2;
    divider = BLOCK_INDEXES;

reckon:
    for (; level >= 0; level--) {
        // 1.auto alloc logical
        if (!array[index] && create) {
            // alloc new blk
            array[index] = balloc(inode->dev);
            // write-back inode buf
            buf->dirty = true;
        }

        /*  2.
            once cycle, free count+1 buffer
            subsequent cycle, brelse upper-level buffer
        */
        brelse(buf);

        /*
            3. end condition
            if level==0, return direct block
            if !array[index] create==false, return 0
        */
        if (level == 0 || !array[index]) {
            return array[index];
        }

        // 4. down level(level != 0)
        buf = bread(inode->dev, array[index]);
        // get block index array
        index = block / divider;
        block = block % divider;
        divider /= BLOCK_INDEXES;
        array = (u16 *)buf->data;
    }

}