#include <xjos/task.h>
#include <xjos/timer.h>


extern int sys_kill();

static void task_alarm(timer_t *timer) {
    timer->task->alarm = NULL; // 解除任务的闹钟关联
    sys_kill(timer->task->pid, SIGALRM); // 向任务发送 SIGALRM 信号
}

int sys_alarm(int sec) {
    task_t *task = running_task();
    if (task->alarm) {
        timer_put(task->alarm); // 取消之前的闹钟
    }

    task->alarm = timer_add(sec * 1000, task_alarm, NULL, task); // 设置新的闹钟
    return 0;
}