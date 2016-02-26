#include "opt-A2.h"
#include <types.h>
#include <proc.h>
#include <kern/wait.h>
#include <proctree.h>

#if OPT_A2

int num;
int limit;

struct array *proctree;
struct lock *proc_lock;

int add_proctree(struct proc *p, struct proc *new){
    KASSERT(proc_lock != NULL);
    KASSERT(p != NULL);
    
    if(num == limit-1){
        if(limit < 256) {
            limit = limit * 2;
            array_setsize(proctree, limit);
        } else {
            return -1;
        }
    }
    
    for(int i = 1; i < limit; i++){
        if(array_get(proctree, i) == NULL){
            set_curpid(p, i);
            array_set(proctree, i, p);
            break;
        }
    }
    
    if(get_curpid(p) == PNOPID){
        return -1;
    }
    num++;
    if(new == NULL){
        set_parent_pid(p, PNOPID);
    } else {
        set_parent_pid(p, get_curpid(new));
    }
    set_state(p, PPORCESS);
    return 0;
}

void remove_proctree(struct proc *p){
    KASSERT(p != NULL);
    int pid = get_curpid(p);
    array_set(proctree, pid, NULL);
    num--;
    proc_destroy(p);
}

void exit(struct proc *p, int exitcode){
    KASSERT(p != NULL);
    
    set_state(p, PEXIT);
    set_exitcode(p, _MKWAIT_EXIT(exitcode));
    int exit_pid = get_curpid(p);
    
    for(int i = 1; i < limit; i++) {
        struct proc *new = array_get(proctree, i);
        if(new != NULL && get_parent_pid(new) == exit_pid){
            int new_state = get_state(new);
            if(new_state == PPORCESS){
                set_parent_pid(new, PNOPID);
            } else if(new_state = PEXIT){
                remove_proctree(new);
            }
        }
    }
    if(get_parent_pid(p) == PNOPID){
        remove_proctree(p);
    } else {
        cv_signal(p->wait, proc_lock);
    }
}

struct proc* get_proctree(pid_t pid){
    return procarray_get(proctree, pid);
}

#endif // OPT_A2

