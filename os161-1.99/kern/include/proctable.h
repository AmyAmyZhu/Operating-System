#ifndef _PROCTABLE_H_
#define _PROCTABLE_H_

#include <proc.h>
#include <synch.h>
#include <limits.h>
#include <array.h>

// Consider changing to PID_MAX defined in LIMITS
#define MAX_PID 256

// Begins at 1 because in wait.h, PID 0 is defined in a special way for process groups
// 1 is reserved for the kernel process.
#define MIN_PID 1

/*
 * Array of processes.
 */
#ifndef PROCINLINE
#define PROCINLINE INLINE
#endif

DECLARRAY(proc);
DEFARRAY(proc, PROCINLINE);

int procCount;
int pidLimit;

// procTable to hold all processes
struct procarray *procTable;

// lock to shield critical sections, such as when a parent calls wait as child calls exit
struct lock *procTableLock;

// Call once during system startup to allocate data structures.
void proctable_bootstrap(void);

// Add process to table, associate it with its parent
int proctable_add_process(struct proc *proc_created, struct proc *proc_parent);

// Switch a process from running to exiting
void proctable_exit_process(struct proc *proc_exited, int exitCode);

// Remove a process from the process table
void proctable_remove_process(struct proc *proc_removed);

// Return a process from the proctable
struct proc* proctable_get_process(pid_t pid);

#endif /* _PROCTABLE_H_ */


