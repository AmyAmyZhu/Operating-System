#include <types.h>
#include <proc.h>
#include <kern/wait.h>

#define PROCINLINE
#include <proctable.h>

/*
 * Each index of the global proctable represents a PID that can be used to lookup a desired process.
 * The proctable is responsible for managing all PIDs in the system.
 */

// Call once during system startup to allocate data structures.
void proctable_bootstrap(void) {

  // initial process limit to conserve memory
  pidLimit = 16;
  procTable = procarray_create();

  if (procTable == NULL) {
    panic("could not create proctable.");
  }
  
  procarray_setsize(procTable, pidLimit);

  procTableLock = lock_create("proctableLock");

  procCount = 0;

  if (procTableLock == NULL) {
    panic("could not create proctableLock");
  }

  for (int i = MIN_PID; i < pidLimit; i++) {
    procarray_set(procTable, i, NULL);
  }
}

// Add process to table
// @proc_created is the newly created process
// @proc_parent is the process that created it and must be assigned as its parent

int proctable_add_process(struct proc *proc_created, struct proc *proc_parent) {
  KASSERT(procTableLock != NULL);
  KASSERT(proc_created != NULL);
  
  // Grow the proctable as more processes come in.
  // Subtract 1 for zero-indexing.
  if (procCount == pidLimit - 1) {
    if (pidLimit < MAX_PID) {
      pidLimit = pidLimit * 2;
      procarray_setsize(procTable, pidLimit);
    }
    else {
      return -1;
    }
  }

  // Assign a PID to the new process
  for (int i = MIN_PID; i < pidLimit; i++) {
    if (procarray_get(procTable, i) == NULL) {
      setPID(proc_created, i);
      procarray_set(procTable, i, proc_created);
      break;
    }
  }

  // Check to see if a PID was available
  if (getPID(proc_created) == PROC_NO_PID) {
    return -1;
  }

  DEBUG(DB_EXEC, "New process in table: %d\n", getPID(proc_created));

  // Increase the count of processes in the procTable
  procCount++;

  // Assign the process' parent PID
  if (proc_parent == NULL) {
    setPPID(proc_created, PROC_NO_PID);
  }
  else {
    setPPID(proc_created, getPID(proc_parent));
  }

  // Finally, set the state of the proces to running
  setState(proc_created, PROC_RUNNING);

  return 0;
}

// Switch a process from running state to exited state.
// The PID of the process remains in the table until its parent,
// the only interested party, also exits.
// @proc_exited is the process that has finished
// @exitcode is the exitcode the process finished with

void proctable_exit_process(struct proc *proc_exited, int exitcode) {
  // DEBUG(DB_EXEC, "Exiting PID: %d from proctable\n", getPID(proc_exited));

  KASSERT(proc_exited != NULL);
  KASSERT(proc_exited->p_pid > 0);

  // set the process state to exited
  setState(proc_exited, PROC_EXITED);

  // encode the exit code as per the docs.
  setExitcode(proc_exited, _MKWAIT_EXIT(exitcode));

  // Next we need to evaluate some cases:
  // If proc_exited has living children, they should now have a NULL parent.
  // If proc_exited has dead children, they should now be destroyed.

  int exitedPID = getPID(proc_exited);

  // Find the children of proc_exited.
  for (int i = MIN_PID; i < pidLimit; i++) {
    struct proc* cur = procarray_get(procTable, i);
    if (cur != NULL && getPPID(cur) == exitedPID) {
      // Check state of child
      int state = getState(cur);
      
      // A running child has its parent set to NULL
      if (state == PROC_RUNNING) {
        setPPID(cur, PROC_NO_PID);
      }

      // An exited child can now be completely removed.
      else if (state == PROC_EXITED) {
        proctable_remove_process(cur);
      }
    }
  }

  // If proc_exited has no parent, it can be removed
  if (getPPID(proc_exited) == PROC_NO_PID) {
    proctable_remove_process(proc_exited);
  }

  // Otherwise if proc_exited has a parent
  // then proc_exited must wake its potentially waiting parent
  else {
    cv_signal(proc_exited->wait_cv, procTableLock);
  }
}

// Remove a process from the process table. This frees the
// PID to be used by another process and destroys all remaining
// information about the process, including any reference to its
// exitcode.
// @proc_removed is the process to be removed from the table.

void proctable_remove_process(struct proc *proc_removed) {
  KASSERT(proc_removed != NULL);

  int pid = getPID(proc_removed);
  procarray_set(procTable, pid, NULL);
  procCount--;

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(proc_removed);
}

// Return a process from the process table
// @pid is the PID of the process to be retrieved

struct proc* proctable_get_process(pid_t pid) {
  if (pid < MIN_PID || pid > MAX_PID) {
    return NULL;
  }

  return procarray_get(procTable, pid);
}
