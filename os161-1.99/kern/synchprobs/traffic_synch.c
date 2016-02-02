#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */

// this structure is different from traffic, this is a linked list
struct VehiclesL
{
    Direction origin;
    Direction destination;
    struct VehiclesL *next;
}

static struct lock *TempLock;
static struct cv *Nor, *Sou, *We, *Ea;
static struct VehiclesL *v;
static struct Direction *intersection;

/*
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
    TempLock = lock_create("TempLock");
    
    intersection = {'N', 'E', 'W', 'S'};
    
    if (intersection == NULL) {
        panic("could not create intersection semaphore");
    }
    
    Nor = cv_create("CV_N");
    Sou = cv_create("CV_S");
    We = cv_create("CV_W");
    Ea = cv_create("CV_E");
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
    KASSERT(intersection != NULL);
    KASSERT(TempLock != NULL);
        
    cv_destroy(Nor);
    cv_destroy(Sou);
    cv_destroy(We);
    cv_destroy(Ea);
    
    lock_destroy(TempLock);

    if(intersection != NULL){
        kfree( (void *) intersection);
        intersection = NULL;
    }
    
    if(v != NULL){
        kfree( (void *) v);
        v = NULL;
    }
}

// helper function to deep copy a linked list of v

void copy(VehiclesL *x, VehiclesL* &copy){
    while (x != NULL) {
        VehiclesL *copy = new VehiclesL;
        copy->origin = x->origin;
        copy->destination = x->destination;
        copy->next = x->next;
        x = x->next;
    }
}

/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
    (void)origin;  /* avoid compiler complaint about unused parameter */
    (void)destination; /* avoid compiler complaint about unused parameter */
    KASSERT(TempLock != NULL);
    lock_aquire(TempLock);
    
    Direction OppDest;
    
    if(destination == north){
        OppDest = south;
    } else if(destination == south){
        OppDest = north;
    } else if(destination == west){
        OppDest = east;
    } else {
        OppDest = west;
    }
    
    bool R1 = true; // entered from the same direction
    bool R2 = true; // going in opposite direction
    bool R3 = true; // two cars different destination, at least one is right-turn
    
    VehiclesL *c1, *c2, *c3, *c4;
    copy(v, c1);
    copy(v, c2);
    copy(v, c3);
    copy(v, c4);
    
    while(c1 != NULL){ // R1
        if(c1->origin != origin){
            R1 = false;
            break;
        }
        c1 = c1->next;
    }
    
    while(c2 != NULL){ // R2
        if(c2->destination != OppDest){
            R2 = false;
            break;
        }
        c2 = c2->next;
    }
    
    while(c3 != NULL){ // R3
        while(c4 != NULL){
            if(c3->destination != c4->destination){
                R3 = false;
                break;
            }
            c4 = c4->next;
        }
        c3 = c3->next;
    }

    while ((R1 != true) || (R2 != true) || (R3 != true)) {
        if(origin == north){
            cv_wait(Nor, TempLock);
        } else if(origin == south){
            cv_wait(Sou, TempLock);
        } else if(origin == west){
            cv_wait(We, TempLock);
        } else{
            cv_wait(Ea, TempLock);
        }
    }
    
    v->next.origin = origin;
    v->next.destination = destination;
    
    lock_release(TempLock);
}

/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
    /* replace this default implementation with your own implementation */
    (void)origin;  /* avoid compiler complaint about unused parameter */
    (void)destination; /* avoid compiler complaint about unused parameter */
    KASSERT(intersection != NULL);
    lock_aquire(TempLock);
    while(v != NULL){
        if((v->origin == origin) && (v->destination == destination)){
            if(v->next == NULL){
                v->origin = NULL;
                v->destination = NULL;
                v->next = NULL;
            } else {
                v->origin = v->next->origin;
                v->destination = v->next->destination;
                v->next = v->next->next;
            }
        }
        v = v->next;
    }
    
    if(origin == north){
        cv_broadcast(Nor, TempLock);
    } else if(origin == south){
        cv_broadcast(Sou, TempLock);
    } else if(origin == west){
        cv_broadcast(We, TempLock);
    } else{
        cv_broadcast(Ea, TempLock);
    }
    
    if(v == NULL){
        if(origin == north){
            cv_broadcast(Nor, TempLock);
        } else if(origin == south){
            cv_broadcast(Sou, TempLock);
        } else if(origin == west){
            cv_broadcast(We, TempLock);
        } else{
            cv_broadcast(Ea, TempLock);
        }
    }
    
    lock_release(TempLock);
}
