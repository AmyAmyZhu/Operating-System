#include "opt-A2.h"
#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

#if OPT_A2
extern struct array *arr;
#endif // OPT_A2

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
    //(void)exitcode;
    
#if OPT_A2
    struct lock *l = get_plock();
    KASSERT(l != NULL);
    lock_acquire(l);
    int find = -1;
    int size = array_num(arr);
    
    struct proctree *new2;
    for(int i = 0; i < size; i++){
        new2 = array_get(arr, i);
        if(new2->proctree_pid == p->pid){
            find = i;
        }
    }
    struct proctree *new = array_get(arr, find);
    new->exitcode = _MKWAIT_EXIT(exitcode);
    is_children(find);
    V(new->sem);
    lock_release(l);
#endif // OPT_A2
    
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
    
#endif // OPT_A2
  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
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
    KASSERT(curproc != NULL);
    *retval = curproc->pid;
#else
    *retval = 1;
#endif // OPT_A2
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
    
#if OPT_A2
    int find = -1;
    int size = array_num(arr);
    struct proctree *new2;
    for(int i = 0; i < size; i++){
        new2 = array_get(arr, i);
        if(new2->proctree_pid == pid){
            find = i;
        }
    }
    struct proctree *newtree = array_get(arr, find);
    if((newtree == NULL) || (newtree->parent != curproc->pid)){
        return ECHILD;
    }
    exitstatus = get_exitcode(pid);
#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif // OPT_A2
    
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
// code you created or modified for ASST2 goes here

int sys_fork(struct trapframe *tf, pid_t *retval){
    KASSERT(curproc != NULL);
    
    struct proc *p = pro_create_runprogram("child");
    if(p == NULL){
        return ENOMEM;
    }
    
    struct addrspace *c_addr = kmalloc(sizeof(struct addrspace));
    if(c_addr == NULL) {
        proc_destroy(p);
        return ENOMEM;
    }
    
    struct trapframe *c_trap = kmalloc(sizeof(struct trapframe));
    if(c_trap == NULL){
        as_destroy(c_addr);
        proc_destroy(p);
        return ENOMEM;
    }
    
    int e = -1;
    e = as_copy(curproc->p_addrspace, &p->p_addrspace);
    if(e != -1){
        proc_destroy(p);
        return e;
    }

    *c_trap = *tf;
    KASSERT(c_trap != NULL);
    thread_fork("cc", p, enter_forked_process, c_trap, 0);
    *retval = p->pid;
    return 0;
}
// old (pre-A2) version of the code goes here,
//  and is ignored by the compiler when you compile ASST2
// the ‘‘else’’ part is optional and can be left
// out if you are just inserting new code for ASST2
#endif /* OPT_A2 */
