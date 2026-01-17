#include <xjos/xjos.h>
#include <xjos/assert.h>
#include <xjos/bitmap.h>
#include <xjos/string.h>


void bitmap_init(bitmap_t *map, char *bits, u32 length, u32 start) {
    memset(bits, 0, length);
    bitmap_make(map, bits, length, start);
}


void bitmap_make(bitmap_t *map, char *bits, u32 length, u32 offset) {
    map->bits = bits;       // pointer to bits
    map->length = length;   // bitmap length
    map->offset = offset;   // logically start index
}


bool bitmap_test(bitmap_t *map, idx_t index) {
    assert(index >= map->offset);

    // bitmap index
    idx_t idx = index - map->offset;

    // calculate byte and bit
    u32 bytes = idx / 8;
    u8 bits = idx % 8;

    assert(bytes < map->length);

    return (map->bits[bytes] & (1 << bits));
}


void bitmap_set(bitmap_t *map, idx_t index, bool value) {
    assert(value == 1 || value == 0);

    assert(index >= map->offset);
    
    idx_t idx = index - map->offset;

    u32 bytes = idx / 8;
    u8 bits = idx % 8;

    if (value) {
        map->bits[bytes] |= (1 << bits);    // set 1
    } else {
        map->bits[bytes] &= ~(1 << bits);   // set 0
    }
}


int bitmap_scan(bitmap_t *map, u32 count) {
    int start = EOF;
    u32 bits_left = map->length * 8;    // all bits
    u32 next_bit = 0;   
    u32 counter = 0;

    while (bits_left-- > 0) {
        if (!bitmap_test(map, map->offset + next_bit))
            counter++;      // next_bits Available
        else
            counter = 0;

        next_bit++;

        // find Continuous count bits, set start, break
        if (counter == count) {
            start = next_bit - count;
            break;
        }
    }

    // no find
    if (start == EOF)
        return EOF;

    bits_left = count;  // need set bits count
    next_bit = start;   

    while (bits_left-- > 0) {
        bitmap_set(map, map->offset + next_bit, true);
        next_bit++;
    }

    return start + map->offset;
}