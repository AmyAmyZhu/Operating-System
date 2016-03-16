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
    #if OPT_A2
    DEBUG(DB_EXEC, "start sys_exit\n");
    lock_acquire(proc_lock);
    proc_exit(p, exitcode);
    lock_release(proc_lock);
    DEBUG(DB_EXEC, "finish sys_exit\n");
    #endif // OPT_A2a
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
    #if OPT_A2
    DEBUG(DB_EXEC, "start sys_getpid\n");
    KASSERT(curproc != NULL);
    *retval = curproc->curpid;
    DEBUG(DB_EXEC, "finish sys_getpid\n");
    #endif // OPT_A2a
    return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
            userptr_t status,
            int options,
            pid_t *retval)
{
#if OPT_A2
    int exitstatus = 0;
    int result = 0;
#else
    int exitstatus;
    int result;
#endif // OPT_A2a
    
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
    #if OPT_A2
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
    #endif // OPT_A2a
    
    result = copyout((void *)&exitstatus,status,sizeof(int));
    if (result) {
        return(result);
    }
    *retval = pid;
    return(0);
}

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval){
    KASSERT(curproc != NULL);
    DEBUG(DB_EXEC, "start sys_fork\n");
    struct proc* p = proc_create_runprogram("system_fork");
    if(p == NULL){
        return ENOMEM;
    }
    struct addrspace *c_addr = kmalloc(sizeof(struct addrspace));
    if(c_addr == NULL){
        proc_destroy(p);
        return ENOMEM;
    }
    struct trapframe *c_trap = kmalloc(sizeof(struct trapframe));
    if(c_trap == NULL){
        kfree(c_trap);
        as_destroy(c_addr);
        proc_destroy(p);
        return ENOMEM;
    }
    
    int result = as_copy(curproc_getas(), &c_addr);
    
    if(result){
        kfree(c_addr);
        proc_destroy(p);
        return result;
    }
    
    p->p_addrspace = c_addr;
    memcpy(c_trap, tf, sizeof(struct trapframe));
    
    result = thread_fork("check_fork", p, enter_forked_process, c_trap, 1);
    if(result){
        kfree(c_trap);
        return result;
    }
    *retval = p->curpid;
    DEBUG(DB_EXEC, "finish sys_fork\n");
    return 0;
}
#endif // OPT_A2a

/*#if OPT_A2
int sys_execv(char *program, char **args){
    int total;
    for(total = 0; args[total] != NULL; total++);
}
#endif // OPT_A2b

*/

#if OPT_A2
int sys_execv(char* program, char** args) {
    char name[strlen(program) + 1];
    int result = copyin((userptr_t) program,
                    name, (strlen(program) + 1) * sizeof(char));
    
    int total;
    for(total = 0; args[total] != NULL; total++);
    
    int totalArgs[total];
    for(int i = 0; i < total; i++) {
        totalArgs[i] = strlen(args[i]);
    }
    
    char* kernArgs[total];
    for(int i = 0; i < total; i++) {
        kernArgs[i] = kmalloc(sizeof(char) * totalArgs[i] + 1);
        copyin((const_userptr_t)args[i], kernArgs[i],
               (totalArgs[i] + 1) * sizeof(char));
    }

    struct addrspace* oldAddr;
    struct vnode* v;
    vaddr_t entrypoint, stackptr;
    
    result = vfs_open(program, O_RDONLY, 0, &v);
    if(result) {
        return result;
    }
    
    as_deactivate();
    oldAddr = curproc_setas(NULL);
    as_destroy(oldAddr);
    
    struct addrspace* newAddr = as_create();
    if(newAddr == NULL) {
        vfs_close(v);
        return ENOMEM;
    }
    curproc_setas(newAddr);
    as_activate();
    
    result = load_elf(v, &entrypoint);
    if(result) {
        vfs_close(v);
        return result;
    }
    vfs_close(v);
    result = as_define_stack(newAddr, &stackptr);
    if(result) {
        return result;
    }
    vaddr_t argsPtr = stackptr;
    for(int i = 0; i < total + 1; i++) {
        argsPtr -= 4;
    }
    vaddr_t start = argsPtr;
    vaddr_t temp = argsPtr;
    copyout(NULL, (userptr_t)(stackptr - 4), 4);
    for(int i = 0; i < total; i++) {
        argsPtr = argsPtr - sizeof(char) * (strlen(kernArgs[i]) + 1);
        copyout(kernArgs[i], (userptr_t)argsPtr, sizeof(char) * (strlen(kernArgs[i]) + 1));
        copyout(&argsPtr, (userptr_t)temp, sizeof(char* ));
        temp += 4;
    }
    for(int i = 0; i < total; i++) {
        kfree(kernArgs[i]);
    }
    while(argsPtr % 8 != 0) {
        argsPtr--;
    }
    enter_new_process(total, (userptr_t)start, (vaddr_t)argsPtr, entrypoint);
    panic("enter_new_process returned");
    return EINVAL;
}

#endif // OPT_A2b
