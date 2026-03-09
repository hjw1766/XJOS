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
    u32 total_bits = map->length * 8;
    u32 next_bit = 0;
    u32 counter = 0;

    if (!count || count > total_bits)
        return EOF;

    while (next_bit < total_bits) {
        u32 byte_idx = next_bit / 8;
        u8 byte = map->bits[byte_idx];

        if (byte == 0xFF) {
            counter = 0;
            next_bit = (byte_idx + 1) * 8;
            continue;
        }

        for (u8 bit_idx = next_bit % 8; bit_idx < 8 && next_bit < total_bits; bit_idx++, next_bit++) {
            if (!(byte & (1 << bit_idx)))
                counter++;
            else
                counter = 0;

            // find Continuous count bits, set start, break
            if (counter == count) {
                start = next_bit + 1 - count;
                goto found;
            }
        }
    }

    // no find
    if (start == EOF)
        return EOF;

found:
    next_bit = start;

    for (u32 bits_left = count; bits_left > 0; bits_left--) {
        map->bits[next_bit / 8] |= (1 << (next_bit % 8));
        next_bit++;
    }

    return start + map->offset;
}