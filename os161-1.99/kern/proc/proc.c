/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */


#include "opt-A2.h"
#include <types.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <vfs.h>
#include <synch.h>
#include <kern/fcntl.h>
#include <kern/wait.h>

/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

/*
 * Mechanism for making the kernel menu thread sleep while processes are running
 */
#ifdef UW
/* count of the number of processes, excluding kproc */
static volatile unsigned int proc_count;
/* provides mutual exclusion for proc_count */
/* it would be better to use a lock here, but we use a semaphore because locks are not implemented in the base kernel */ 
static struct semaphore *proc_count_mutex;
/* used to signal the kernel menu thread when there are no processes */
struct semaphore *no_proc_sem;   
#endif  // UW

#if OPT_A2
int count = 0;
int arraysize = 32;
// initillization the proctree, add new to p
int add_proctree(struct proc *p, struct proc *new){
    KASSERT(proc_lock != NULL);
    KASSERT(p != NULL);
    //DEBUG(DB_EXEC, "start add_proctree\n");
    int change = 0; // record error
    if(count+1 == arraysize){ // if the array overflow, multiple the size by 2
        arraysize *= arraysize;
        array_setsize(proctree, arraysize);
    }
    
    for(int i = 1; i < arraysize; i++){
        if(array_get(proctree, i) == NULL){
            p->curpid = i;
            array_set(proctree, i, p);
            break;
        }
    }
    
    // error pid for p
    if(p->curpid == -1){
        change = -1;
    }
    // add no parent new for p
    //DEBUG(DB_EXEC, "start add_proctree\n");
    if(new == NULL){
        p->parent_pid = -1;
    } else {
        p->parent_pid = new->curpid;
    }
    p->state = 1;
    count++;
    //DEBUG(DB_EXEC, "finish add_proctree\n");
    return change;
}

void proc_exit(struct proc *p, int exitcode){
    KASSERT(p != NULL);
    KASSERT(p->curpid > 0);
    
    p->state = 0; // exit code
    p->exitcode = _MKWAIT_EXIT(exitcode);
    int exit_pid = p->curpid;
    
    for(int i = 1; i < arraysize; i++) {
        struct proc *new = array_get(proctree, i);
        if(new != NULL && new->parent_pid == exit_pid){
            int new_state = new->state;
            if(new_state == 1){
                new->parent_pid = -1;
            } else if(new_state == 0){
                KASSERT(new != NULL);
                array_set(proctree, new->curpid,NULL);
                proc_destroy(new);
                count--;
            }
        }
    }
    
    // no parent, just remove children p
    if(p->parent_pid == -1){
        //kprintf("Here!!come remove proctree\n");
        DEBUG(DB_EXEC, "start proc_exit\n");
        KASSERT(p != NULL);
        array_set(proctree, p->curpid, NULL);
        //kprintf("Here!!come proc destroy\n");
        proc_destroy(p);
        count--;
        //kprintf("Here!!leave proc destroy\n");
        DEBUG(DB_EXEC, "end proc_exit\n");
        //kprintf("Here!!leave remove proctree\n");
    } else {
        cv_signal(p->wait, proc_lock);
    }
    //kprintf("Here!!leave remove proctree!!\n");
}

#endif // OPT_A2

/*
 * Create a proc structure.
 */
static
struct proc *
proc_create(const char *name)
{
	struct proc *proc;

	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		return NULL;
	}
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		kfree(proc);
		return NULL;
	}

#if OPT_A2
    proc->wait = cv_create("newcvproc");
    if(proc->wait == NULL){
        kfree(proc);
        return NULL;
    }
#endif // OPT_A2
    
	threadarray_init(&proc->p_threads);
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

#ifdef UW
	proc->console = NULL;
#endif // UW

#if OPT_A2
    int err = 0;
    proc->curpid = -1;

    if(kproc == NULL){
        err = add_proctree(proc, NULL);
    } else {
        lock_acquire(proc_lock);
        if(curproc != kproc){
            err = add_proctree(proc, curproc);
        } else {
            err = add_proctree(proc, NULL);
        }
        lock_release(proc_lock);
    }
    if(err){
        return NULL;
    }
#endif // OPT_A2
    
	return proc;
}

/*
 * Destroy a proc structure.
 */
void
proc_destroy(struct proc *proc)
{
    /*
     * note: some parts of the process structure, such as the address space,
     *  are destroyed in sys_exit, before we get here
     *
     * note: depending on where this function is called from, curproc may not
     * be defined because the calling thread may have already detached itself
     * from the process.
     */
    
    KASSERT(proc != NULL);
    KASSERT(proc != kproc);
    
    /*
     * We don't take p_lock in here because we must have the only
     * reference to this structure. (Otherwise it would be
     * incorrect to destroy it.)
     */
    
    /* VFS fields */
    if (proc->p_cwd) {
        VOP_DECREF(proc->p_cwd);
        proc->p_cwd = NULL;
    }
    
    
#ifndef UW  // in the UW version, space destruction occurs in sys_exit, not here
    if (proc->p_addrspace) {
        /*
         * In case p is the currently running process (which
         * it might be in some circumstances, or if this code
         * gets moved into exit as suggested above), clear
         * p_addrspace before calling as_destroy. Otherwise if
         * as_destroy sleeps (which is quite possible) when we
         * come back we'll be calling as_activate on a
         * half-destroyed address space. This tends to be
         * messily fatal.
         */
        struct addrspace *as;
        
        as_deactivate();
        as = curproc_setas(NULL);
        as_destroy(as);
    }
#endif // UW
    
#ifdef UW
    //if (proc->console) {
      //  vfs_close(proc->console);
    //}
#endif // UW
    
    threadarray_cleanup(&proc->p_threads);
    spinlock_cleanup(&proc->p_lock);
    
    kfree(proc->p_name);
    kfree(proc);
    
#ifdef UW
    /* decrement the process count */
    /* note: kproc is not included in the process count, but proc_destroy
     is never called on kproc (see KASSERT above), so we're OK to decrement
     the proc_count unconditionally here */
    P(proc_count_mutex);
    KASSERT(proc_count > 0);
    proc_count--;
    /* signal the kernel menu thread if the process count has reached zero */
    if (proc_count == 0) {
        V(no_proc_sem);
    }
    V(proc_count_mutex);
#endif // UW
    
    
}


/*
 * Create the process structure for the kernel.
 */
void
proc_bootstrap(void)
{
#if OPT_A2
    proctree = array_create();
    array_setsize(proctree, arraysize);
    proc_lock = lock_create("proc_lock");
    for(int i = 1; i < arraysize; i++){
        array_set(proctree, i, NULL);
    }
#endif // OPT_A2
  kproc = proc_create("[kernel]");
  if (kproc == NULL) {
    panic("proc_create for kproc failed\n");
  }
#ifdef UW
  proc_count = 0;
  proc_count_mutex = sem_create("proc_count_mutex",1);
  if (proc_count_mutex == NULL) {
    panic("could not create proc_count_mutex semaphore\n");
  }
  no_proc_sem = sem_create("no_proc_sem",0);
  if (no_proc_sem == NULL) {
    panic("could not create no_proc_sem semaphore\n");
  }
#endif // UW
}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *proc;
	char *console_path;

	proc = proc_create(name);
	if (proc == NULL) {
		return NULL;
	}
    
#ifdef UW
	/* open the console - this should always succeed */
	console_path = kstrdup("con:");
	if (console_path == NULL) {
	  panic("unable to copy console path name during process creation\n");
	}
	if (vfs_open(console_path,O_WRONLY,0,&(proc->console))) {
	  panic("unable to open the console during process creation\n");
	}
	kfree(console_path);
#endif // UW
	  
	/* VM fields */

	proc->p_addrspace = NULL;

	/* VFS fields */

#ifdef UW
	/* we do not need to acquire the p_lock here, the running thread should
           have the only reference to this process */
        /* also, acquiring the p_lock is problematic because VOP_INCREF may block */
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		proc->p_cwd = curproc->p_cwd;
	}
#else // UW
	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		proc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);
#endif // UW

#ifdef UW
	/* increment the count of processes */
        /* we are assuming that all procs, including those created by fork(),
           are created using a call to proc_create_runprogram  */
	P(proc_count_mutex); 
	proc_count++;
	V(proc_count_mutex);
#endif // UW

	return proc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
	int result;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	result = threadarray_add(&proc->p_threads, t, NULL);
	spinlock_release(&proc->p_lock);
	if (result) {
		return result;
	}
	t->t_proc = proc;
	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 */
void
proc_remthread(struct thread *t)
{
	struct proc *proc;
	unsigned i, num;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	/* ugh: find the thread in the array */
	num = threadarray_num(&proc->p_threads);
	for (i=0; i<num; i++) {
		if (threadarray_get(&proc->p_threads, i) == t) {
			threadarray_remove(&proc->p_threads, i);
			spinlock_release(&proc->p_lock);
			t->t_proc = NULL;
			return;
		}
	}
	/* Did not find it. */
	spinlock_release(&proc->p_lock);
	panic("Thread (%p) has escaped from its process (%p)\n", t, proc);
}

/*
 * Fetch the address space of the current process. Caution: it isn't
 * refcounted. If you implement multithreaded processes, make sure to
 * set up a refcount scheme or some other method to make this safe.
 */
struct addrspace *
curproc_getas(void)
{
	struct addrspace *as;
#ifdef UW
        /* Until user processes are created, threads used in testing 
         * (i.e., kernel threads) have no process or address space.
         */
	if (curproc == NULL) {
		return NULL;
	}
#endif

	spinlock_acquire(&curproc->p_lock);
	as = curproc->p_addrspace;
	spinlock_release(&curproc->p_lock);
	return as;
}

/*
 * Change the address space of the current process, and return the old
 * one.
 */
struct addrspace *
curproc_setas(struct addrspace *newas)
{
	struct addrspace *oldas;
	struct proc *proc = curproc;

	spinlock_acquire(&proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	spinlock_release(&proc->p_lock);
	return oldas;
}
