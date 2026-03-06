#include <xjos/task.h>
#include <xjos/errno.h>

extern task_t *tasks_table[TASK_NR];

mode_t sys_umask(mode_t mask) {
    task_t *task = running_task();
    mode_t old = task->umask;
    task->umask = mask & 0777;
    return old;
}

int sys_setpgid(int pid, int pgid) {
    task_t *current = running_task();

    if (!pid)
        pid = current->pid;
    if (!pgid)
        pgid = current->pid;

    for (int i = 0; i < TASK_NR; i++) {
        task_t *task = tasks_table[i];
        if (!task)
            continue;
        if (task->pid != pid)
            continue;

        if (task_leader(task))
            return -EPERM; // 进程组领导者不能被移出其进程组
        if (task->sid != current->sid)
            return -EPERM; // 只能改变同一会话中的进程的进程组
        task->pgid = pgid;
        return EOK;
    }   
    return -ESRCH; // 没有找到指定 PID 的进程
}

int sys_getpgrp() {
    task_t *task = running_task();
    return task->pgid;
}

int sys_setsid() {
    task_t *task = running_task();
    if (task_leader(task))
        return -EPERM; // 进程组领导者不能创建新的会话
    task->sid = task->pgid = task->pid; // 将会话 ID 和进程组 ID 设置为当前进程的 PID
    return task->sid;
}