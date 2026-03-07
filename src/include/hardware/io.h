#ifndef XJOS_IO_H
#define XJOS_IO_H

#include <xjos/types.h>

extern u8 inb(u16 port);
extern u16 inw(u16 port);
extern u32 inl(u16 port);

extern void outb(u16 port, u8 value);
extern void outw(u16 port, u16 value);
extern void outl(u16 port, u32 value);

#endif /* XJOS_IO_H */