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

#if OPT_A2
int sys_execv(const_userptr_t path, userptr_t argv){
    int argc, result, i, offset;
    size_t path_len;
    struct addrspace *old_as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr, argvptr;
    
    char *kpath = kmalloc(PATH_MAX);
    if(kpath == NULL) return ENOMEM;
    result = copyinstr(path, kpath, PATH_MAX, &path_len);
    if(result){
        return result;
    }
    if(path_len == 1){
        return ENOENT;
    }
    
    char** argv_ptr = (char**) argv;
    for(argc = 0; argv_ptr[argc] != NULL; ++argc);
    if(argc > ARG_MAX) return E2BIG;
    userptr_t k_ptr = kmalloc(sizeof(userptr_t));
    if (k_ptr == NULL)
        return ENOMEM;
    
    int arg_size = (argc+1) * sizeof(char*); // don't forget + 1
    char** kargv = kmalloc(arg_size);
    if (kargv == NULL)
        return ENOMEM;
    
    for (i = 0; i<argc; ++i){
        result = copyin((const_userptr_t)&(argv_ptr[i]), &k_ptr, sizeof(userptr_t));  //copy addr, so &(argv_ptr[i])
        if (result){
            return ENOMEM;
        }
        kargv[i] = kmalloc(PATH_MAX);
        result = copyinstr((const_userptr_t)k_ptr, kargv[i], PATH_MAX, &path_len);
        if (result) {
            return result;
        }
    }
    
    // argument array should end with NULL.
    kargv[argc] = NULL;
    
    
    /* Open the file. */
    char *fname_temp;
    fname_temp = kstrdup(kpath);
    result = vfs_open(fname_temp, O_RDONLY, 0, &v);             // ????
    kfree(fname_temp);
    if (result) {
        return result;
    }
    
    // Replace old address space &
    // Create a new address space.
    
    KASSERT(curproc_getas() != NULL);
    old_as = curproc_getas();
    spinlock_acquire(&curproc->p_lock);
    curproc->p_addrspace = as_create();
    spinlock_release(&curproc->p_lock);
    
    if (curproc->p_addrspace ==NULL) {
        vfs_close(v);
        return ENOMEM;
    }
    
    /* activate new as. */
    as_activate();
    
    /* Load the executable. */
    result = load_elf(v, &entrypoint);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        vfs_close(v);
        return result;
    }
    
    /* Done with the file now. */
    vfs_close(v);
    
    /* Define the user stack in the address space */
    // stackptr is the very top of the user address space
    result = as_define_stack(curproc->p_addrspace, &stackptr);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        return result;
    }
    
    // copy argv onto the stack of user address space
    char** addr_ptr = kmalloc((argc+1)*sizeof(char*)); // sizeof(char*) = 4 bytes
    if (addr_ptr == NULL)
        return ENOMEM;
    
    for (i=argc-1; i>=0; --i){
        char* arg_str = kargv[i];
        int length = strlen(arg_str)+1;
        stackptr-=length;
        result = copyout(arg_str, (userptr_t)stackptr, length);     // assume it automatically fill 0
        if (result) {
            /* p_addrspace will go away when curproc is destroyed */
            return result;
        }
        addr_ptr[i] = (char*)stackptr;
    }
    addr_ptr[argc] = NULL;
    
    offset = stackptr%4;
    stackptr-=stackptr%4;
    bzero((void *)stackptr, offset);
    
    offset = (argc+1)*sizeof(char*);
    stackptr-=offset;
    result = copyout(addr_ptr, (userptr_t)stackptr, offset);     // assume it automatically fill 0
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        return result;
    }
    argvptr = stackptr;
    
    offset = stackptr%8;
    stackptr-=stackptr%8;
    bzero((void *)stackptr, offset);
    
    // clean variables
    as_destroy(old_as);
    kfree(kargv);
    kfree(kpath);
    kfree(addr_ptr);
    // kfree(k_ptr);
    // kfree(argv_ptr);
    
    /* Warp to user mode. */
    enter_new_process(argc /*argc*/, (userptr_t)argvptr /*userspace addr of argv*/,
                      stackptr, entrypoint);
    
    /* enter_new_process does not return. */
    panic("enter_new_process returned\n");
    return EINVAL;
}
#endif // OPT_A2b
