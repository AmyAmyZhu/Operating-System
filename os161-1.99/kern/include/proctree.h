#ifndef _PROCTREE_H_
#define _PROCTREE_H_

//#include "opt-A2.h"
#include <proc.h>
#include <synch.h>
#include <limits.h>
#include <array.h>

//#if OPT_A2

DECLARRAY(proc);
DEFARRAY(proc, INLINE);

int add_proctree(struct proc *p, struct proc *new);
void remove_proctree(struct proc *p);
struct proc* get_proctree(pid_t pid);
void exit(struct proc *p, int exitcode);

//#endif // OPT_A2

#endif // proctree.h

