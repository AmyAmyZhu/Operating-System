#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <proctable.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <vfs.h>
#include <kern/fcntl.h>

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

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

  // Ensure synchronization in case another process is trying to getpid()
  // and inform the proctable that the process is exiting.
  lock_acquire(procTableLock);
    proctable_exit_process(p, exitcode);
  lock_release(procTableLock);

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}

int
sys_getpid(pid_t *retval)
{
  KASSERT(curproc != NULL);

  struct proc *p = curproc;

  // return the current process' PID.
  *retval = getPID(p);
  return(0);
}

int
sys_waitpid(pid_t pid, // pid that you want to wait for
	    userptr_t status, // status address you want the exitcode returned in
	    int options, // number of options
	    pid_t *retval) // return value for waitpid, if success should be pid of process waitee.
{
  int exitstatus = 0;
  int result = 0;

  if (options != 0) {
    return(EINVAL);
  }

  lock_acquire(procTableLock);
    struct proc *child = proctable_get_process(pid);
    struct proc *parent = curproc;

    // Error if PID waited on does not refer to a valid process
    if (child == NULL) {
      result = ESRCH;
    }
    // Error if requested PID was not a child of parent process
    else if (getPPID(child) != getPID(parent)) {
      result = ECHILD;
    }

    // If any of the above errors were set, return error
    if (result) {
      lock_release(procTableLock);
      return(result);
    }
    
    // Otherwise, if waitpid call was valid, check to see if child has exited.
    // If child is still running, have parent wait for child to exit
    // Need a while loop, Mesa semantics, because a different child could wake him up,
    // Even though the one we're waiting on here is still running
    while (getState(child) == PROC_RUNNING) {
      cv_wait(child->wait_cv, procTableLock);
    }

    // We are now awoken because we waited for the child to exit,
    // or the child had already exited. Either way, we can now collect
    // their exit code. We will remove them from proctable when we exit.
    exitstatus = getExitcode(child);

  lock_release(procTableLock);

  // copy the exitstatus of the process from kernel address space to
  // user space, at the address of the user address `status` that was passed in for that purpose.
  result = copyout((void *)&exitstatus,status,sizeof(int));

  if (result) {
    return(result);
  }

  // the return value of waitpid is always the PID of the process
  // whose exit status goes in `status`, in OS161
  *retval = pid;
  return(0);
}

int sys_fork(struct trapframe* tf, pid_t *retval) {
  struct proc* proc_created = proc_create_runprogram("Forked process");
  struct addrspace* as;
  int result;

  // could not create child due to memory constraints
  if (proc_created == NULL) {
    return ENOMEM;
  }

  // the parent needs to return the retval of the child
  // synchronization is not required since the only one interested
  // in the child process is the parent and we are the parent
  *retval = getPID(proc_created);

  // allocate duplicate trapframe on kernel heap for child process
  struct trapframe* dupTrap = kmalloc(sizeof(struct trapframe));

  if (dupTrap == NULL) {
    return ENOMEM;
  }

  memcpy(dupTrap, tf, sizeof(struct trapframe));
  
  // copy the address space of the parent process
  result = as_copy(curproc_getas(), &as);
  if (result) {
    return result;
  }
  
  // if successful, now set the new proc's address space
  // to be the copied parent's. We don't need any synchronization to do this
  // because only the parent knows about this child and it is not yet running
  proc_created->p_addrspace = as;

  // we are now ready to create the child process using thread_fork
  DEBUG(DB_EXEC, "Starting Forked program\n");

  result = thread_fork("Forked thread", // name of thread
                        proc_created, // process to attach thread to
                        enter_forked_process, // entrypoint function
                        (void *)dupTrap, // pass trapframe as data
                        1 // pass in as number of args
                      );

  if (result) {
    kfree(dupTrap);
    return result;
  }

  // If there has been no error in thread_fork, then the child process is now running
  // and could run before this code is executed

  return 0;
}

int sys_execv(char* program, userptr_t args) {

  // Ensure that we were given a valid program name.
  if (program == NULL) {
    return EFAULT;
  }

  int result;

  // Save new and old address space
  struct addrspace *oldas;
  struct addrspace *newas;
  struct vnode *v;

  // entrypoint and location of stack for new program we're running
  vaddr_t entrypoint, stackptr;

  /////////////////////////////////////////////
  // Copy in arguments from user space to the kernel
  /////////////////////////////////////////////

  struct argscopy* argscopy = argscopy_create();

  if (argscopy == NULL) {
    return ENOMEM;
  }

  result = copyinargs(args, argscopy);

  if (result) {
    argscopy_destroy(argscopy);
    return result;
  }

  /* Open the file. */
  char *progname;
  progname = kstrdup(program);
  result = vfs_open(progname, O_RDONLY, 0, &v);
  kfree(progname);

  if (result) {
    argscopy_destroy(argscopy);
    return result;
  }

  // Create the new user address space memory
  newas = as_create();
  if (newas == NULL) {
    argscopy_destroy(argscopy);
    vfs_close(v);
    return ENOMEM;
  }

  /* Switch to it and activate it. Keeping the old one in case we need to rollback. */
  oldas = curproc_setas(newas);
  as_activate();

  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    argscopy_destroy(argscopy);
    // Restore old address space since there was an error
    curproc_setas(oldas);
    return result;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(newas, &stackptr);
  if (result) {
    argscopy_destroy(argscopy);
    // Restore old address space since there was an error
    curproc_setas(oldas);
    return result;
  }

  /////////////////////////////////////////////
  // Copy out arguments from kernel to new user space
  /////////////////////////////////////////////

  result = copyoutargs(argscopy, &stackptr);

  if (result) {
    argscopy_destroy(argscopy);
    // Make sure to restore the oldas on failure.
    curproc_setas(oldas);
    return result;
  }

  // Now that everything has succeeded, we can free the old user address space memory
  // and the argscopy
  int nargs = argscopy->nargs;
  argscopy_destroy(argscopy);
  as_destroy(oldas);

  /* Warp to user mode. */
  enter_new_process(nargs, (userptr_t)(stackptr) /*userspace addr of argv*/,
        stackptr, entrypoint);
  
  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");

  return EINVAL;
}
