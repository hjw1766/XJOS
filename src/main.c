extern void interrupt_init();
extern void clock_init();
extern void time_init();
extern void rtc_init();
extern void memory_map_init();
extern void mapping_int();
extern void arena_init();
extern void task_init();
extern void syscall_init();
extern void keyboard_init();
extern void tss_init();
extern void ide_init();
extern void buffer_init();
extern void super_init();

#include <xjos/interrupt.h>
#include <xjos/debug.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

void kernel_init() {
    tss_init();
    memory_map_init();
    mapping_int();
    arena_init();

    interrupt_init();
    clock_init();
    keyboard_init();
    time_init();
    ide_init();   
    buffer_init(); 
    task_init();
    syscall_init();

    super_init();

    while (1);

    set_interrupt_state(true);
}
