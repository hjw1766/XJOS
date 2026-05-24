#include <net/chksum.h>

#define CRC_POLY 0xEDB88320

u32 eth_fcs(void *data, int len) {
    u32 crc = -1;
    u8 *ptr = (u8 *)data;
    for (int i = 0; i < len; i++) {
        crc ^= ptr[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC_POLY;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

static u16 chksum(void *data, int len, u32 sum) {
    u16 *ptr = (u16 *)data;
    for (; len > 1; len -= 2) {
        // 32位整形不会溢出
        sum += *ptr++;
    }

    // 奇数情况，小段补零
    if (len == 1) {
        sum += *(u8 *)ptr;
    }

    /*
        0x1002 = 0x0002 + 1 -> 0x0003. 延迟进位
    */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (u16)(sum);
}

u16 ip_chksum(void *data, int len) {
    u16 sum = chksum(data, len, 0);
    return ~sum;
}