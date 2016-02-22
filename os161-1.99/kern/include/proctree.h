#ifndef _PROCTREE_H_
#define _PROCTREE_H_

#include "opt-A2.h"
#include <array.h>
#include <synch.h>
#include <spinlock.h>
#include <thread.h> /* required for struct threadarray */

#if OPT_A2
struct proctree{
    int exitcode;
    pid_t proctree_pid;
    pid_t parent;
    struct array *children;
    struct semaphore *sem;
};

struct proctree *init_proctree(struct proc *proc, int curpid);
int update(int i, int curpid, struct array *arr);
#endif // OPT_A2

#endif // proctree.h
