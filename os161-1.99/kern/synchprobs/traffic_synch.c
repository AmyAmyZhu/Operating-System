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

struct VehiclesList
{
    int num;
    Direction origin;
    Direction destination;
}

static struct lock *TempLock;
static struct cv *Nor, *Sou, *We, *Ea;
static struct VehiclesList v[12];
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
    
    intersection = {north, east, west, south};
    
    for(int i = 0; i < 12; i++){
        v[i].num = 0;
    }
    
    v[0].origin = north;
    v[0].destination = east;
    v[1].origin = north;
    v[1].destination = south;
    v[2].origin = north;
    v[2].destination = west;

    v[3].origin = east;
    v[3].destination = north;
    v[4].origin = east;
    v[4].destination = west;
    v[5].origin = east;
    v[5].destination = south;
    
    v[6].origin = south;
    v[6].destination = east;
    v[7].origin = south;
    v[7].destination = north;
    v[8].origin = south;
    v[8].destination = west;
    
    v[9].origin = west;
    v[9].destination = north;
    v[10].origin = west;
    v[10].destination = east;
    v[11].origin = west;
    v[11].destination = south;
    
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
    
    kfree(intersection);
    kfree(v);
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
    bool R3 = false; // two cars different destination, at least one is right-turn
    
    for(int i = 0; i < 12; i++){
        if((v[i].origin != origin) && (v[i].origin != NULL)){
            R1 = false;
            break;
        }
    }
    
    for(int i = 0; i < 12; i++){
        if((v[i].destination != OppDest) && (v[i].destination != NULL)){
            R2 = false;
            break;
        }
    }
    
    for(int i = 0; i < 12; i++){
        if((v[i].origin == north) && (v[i].destination == west)){
            R3 = true;
            for(int j = i+1; j < 12; j++){
                if(v[j].destination == west){
                    R3 = false;
                    break;
                }
            }
        } else if((v[i].origin == south) && (v[i].destination == east)){
            R3 = true;
            for(int j = i+1; j < 12; j++){
                if(v[j].destination == east){
                    R3 = false;
                    break;
                }
            }
        } else if((v[i].origin == east) && (v[i].destination == north)){
            R3 = true;
            for(int j = i+1; j < 12; j++){
                if(v[j].destination == north){
                    R3 = false;
                    break;
                }
            }
        } else if((v[i].origin == west) && (v[i].destination == south)){
            R3 = true;
            for(int j = i+1; j < 12; j++){
                if(v[j].destination == south){
                    R3 = false;
                    break;
                }
            }
        }
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

    if(origin == north){
        if(destination == east){
            (v[0].num)++;
        }else if(destination == south){
            (v[1].num)++;
        }else if(destination == west){
            (v[2].num)++;
        }
    } else if(origin == east){
        if(destination == north){
            (v[3].num)++;
        }else if(destination == west){
            (v[4].num)++;
        }else if(destination == south){
            (v[5].num)++;
        }
    } else if(origin == south){
        if(destination == east){
            (v[6].num)++;
        }else if(destination == north){
            (v[7].num)++;
        }else if(destination == west){
            (v[8].num)++;
        }
    } else if(origin == west){
        if(destination == north){
            (v[9].num)++;
        }else if(destination == east){
            (v[10].num)++;
        }else if(destination == south){
            (v[11].num)++;
        }
    }
    
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
    
    if(origin == north){
        if(destination == east){
            (v[0].num)--;
        }else if(destination == south){
            (v[1].num)--;
        }else if(destination == west){
            (v[2].num)--;
        }
    } else if(origin == east){
        if(destination == north){
            (v[3].num)--;
        }else if(destination == west){
            (v[4].num)--;
        }else if(destination == south){
            (v[5].num)--;
        }
    } else if(origin == south){
        if(destination == east){
            (v[6].num)--;
        }else if(destination == north){
            (v[7].num)--;
        }else if(destination == west){
            (v[8].num)--;
        }
    } else if(origin == west){
        if(destination == north){
            (v[9].num)--;
        }else if(destination == east){
            (v[10].num)--;
        }else if(destination == south){
            (v[11].num)--;
        }
    }
    
    /*if((origin == north) && (v != NULL)) {
        cv_broadcast(Nor, TempLock);
    } else if((origin == south) && (v != NULL)) {
        cv_broadcast(Sou, TempLock);
    } else if((origin == west) && (v != NULL)) {
        cv_broadcast(We, TempLock);
    } else if((origin == east) && (v != NULL)) {
        cv_broadcast(Ea, TempLock);
    }
    */
    //if(v == NULL){
        if(origin == north){
            cv_broadcast(Nor, TempLock);
        } else if(origin == south){
            cv_broadcast(Sou, TempLock);
        } else if(origin == west){
            cv_broadcast(We, TempLock);
        } else{
            cv_broadcast(Ea, TempLock);
        }
    //}
    
    lock_release(TempLock);
}
