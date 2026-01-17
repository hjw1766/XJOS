#ifndef XJOS_STDDEF_H
#define XJOS_STDDEF_H


#include <xjos/types.h>

// ===================================
//      Core Utility Macros
// ===================================

/**
 * @brief Calculates the byte offset of a member within a struct.
 */
#define element_offset(type, member) ((u32)(&((type *)0)->member))

/**
 * @brief Calculates the starting address of the containing struct
 * from a pointer to one of its members.
 */
#define element_entry(type, member, ptr) ((type *)((u32)(ptr) - element_offset(type, member)))







#endif /* XJOS_STDDEF_H */