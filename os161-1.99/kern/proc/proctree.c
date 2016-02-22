#include "opt-A2.h"
#include <proctree.h>
#include <array.h>
#include <limits.h>
#include <types.h>

#if OPT_A2

struct proctree *init_proctree(struct proc *proc, int curpid){
    if(curpid == -1){
        return NULL;
    }
    proc->pid = curpid;
    
    struct proctree *new;
    new = kmalloc(sizeof(struct proctree));
    new->proctree_pid = proc->pid;
    new->exitcode = -1;
    new->sem = sem_create("tree_sem");
    
    struct array *child;
    child = array_create();
    array_setsize(child, 0);
    new->children = child;
    
    if(curpid == 0){
        new->parent = -1;
    }else{
        new->parent = curproc->pid;
    }
    
    return new;
}

int update(int i, int curpid, struct array *arr){
    int size = array_num(arr);
    int q = i;
    struct proctree *new;
    for(int r = 0; r < size; r++){
        new = array_get(arr, r);
        if(new->proctree_pid == q) {
            q++;
            return update(q);
        }
    }
    curpid = q;
    return q;
}

#endif // OPT_A2

