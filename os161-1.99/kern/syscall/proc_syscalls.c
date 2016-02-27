#include "opt-A2.h"
#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <synch.h>
//#include <proctree.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <vfs.h>
#include <kern/fcntl.h>

/* this implementation of sys__exit does not do anything with the exit code */
/* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
    
    struct addrspace *as;
    struct proc *p = curproc;
    /* for now, just include this to keep the compiler from complaining about
     an unused variable */
    (void)exitcode;
    
    DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);
    
    KASSERT(curproc->p_addrspace != NULL);
    as_deactivate();
    /*
     * clear p_addrspace before calling as_destroy. Otherwise if
     * as_destroy sleeps (which is quite possible) when we
     * come back we'll be calling as_activate on a
     * half-destroyed address space. This tends to be
     * messily fatal.
     */
    as = curproc_setas(NULL);
    as_destroy(as);
    
    /* detach this thread from its process */
    /* note: curproc cannot be used after this call */
    proc_remthread(curthread);
    //#if OPT_A2
    DEBUG(DB_EXEC, "start sys_exit\n");
    lock_acquire(proc_lock);
    proc_exit(p, exitcode);
    lock_release(proc_lock);
    DEBUG(DB_EXEC, "finish sys_exit\n");
    //#endif // OPT_A2
    /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
    //proc_destroy(p);
    
    thread_exit();
    /* thread_exit() does not return, so we should never get here */
    panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
    /* for now, this is just a stub that always returns a PID of 1 */
    /* you need to fix this to make it work properly */
    //#if OPT_A2
    DEBUG(DB_EXEC, "start sys_getpid\n");
    KASSERT(curproc != NULL);
    struct proc *new = curproc;
    *retval = new->curpid;
    DEBUG(DB_EXEC, "finish sys_getpid\n");
    //#endif // OPT_A2
    return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
            userptr_t status,
            int options,
            pid_t *retval)
{
    int exitstatus;
    int result;
    
    /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.
     Fix this!
     */
    
    if (options != 0) {
        return(EINVAL);
    }
    /* for now, just pretend the exitstatus is 0 */
    //#if OPT_A2
    DEBUG(DB_EXEC, "start sys_waitpid\n");
    lock_acquire(proc_lock);
    struct proc *parent = curproc;
    struct proc *children = array_get(proctree, pid);
    
    if(children == NULL){
        result = ESRCH;
    } else if(children->parent_pid != parent->curpid){
        result = ECHILD;
    }
    
    if(result){
        lock_release(proc_lock);
        return result;
    }
    
    while(children->state == 1){
        cv_wait(children->wait, proc_lock);
    }
    exitstatus = children->exitcode;
    lock_release(proc_lock);
    DEBUG(DB_EXEC, "finish sys_waitpid\n");
    //#endif // OPT_A2
    
    result = copyout((void *)&exitstatus,status,sizeof(int));
    if (result) {
        return(result);
    }
    *retval = pid;
    return(0);
}

int sys_fork(struct trapframe *tf, pid_t *retval){
    KASSERT(curproc != NULL);
    DEBUG(DB_EXEC, "start sys_fork\n");
    struct proc* p = proc_create_runprogram("system_fork");
    if(p == NULL){
        return ENOMEM;
    }
    struct trapframe *c_trap = kmalloc(sizeof(struct trapframe));
    if(c_trap == NULL){
        return ENOMEM;
    }
    
    *retval = p->curpid;
    memcpy(c_trap, tf, sizeof(struct trapframe));
    
    struct addrspace *c_addr;
    int result = as_copy(curproc_getas(), &c_addr);
    
    if(result){
        return result;
    }
    
    p->p_addrspace = c_addr;
    
    result = thread_fork("check_fork", p, enter_forked_process, c_trap, 1);
    if(result){
        kfree(c_trap);
        return result;
    }
    DEBUG(DB_EXEC, "finish sys_fork\n");
    return 0;
}

