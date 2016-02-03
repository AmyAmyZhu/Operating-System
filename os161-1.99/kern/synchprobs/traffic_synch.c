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
};

static struct lock *TempLock;
static struct cv *cvTraffic;
static struct VehiclesList v[12];

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
    

    cvTraffic = cv_create("CV_TRAFFIC");
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
    KASSERT(TempLock != NULL);
        
    cv_destroy(cvTraffic);
    
    lock_destroy(TempLock);
    
    for(int i = 0; i < 12; i++){
        v[i].num = 0;
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
    lock_acquire(TempLock);
    
    bool R1 = false; // entered from the same direction
    bool R2 = false; // going in opposite direction
    bool R3 = false; // two cars different destination, at least one is right-turn
    
    // R1
    if(origin == north){
        if((v[3].num == 0) && (v[4].num == 0) && (v[5].num == 0) &&
           (v[6].num == 0) && (v[7].num == 0) && (v[8].num == 0) &&
           (v[9].num == 0) && (v[10].num == 0) && (v[11].num == 0)){
            R1 = true;
        }
    } else if(origin == east){
        if((v[0].num == 0) && (v[1].num == 0) && (v[2].num == 0) &&
           (v[6].num == 0) && (v[7].num == 0) && (v[8].num == 0) &&
           (v[9].num == 0) && (v[10].num == 0) && (v[11].num == 0)){
            R1 = true;
        }
    } else if(origin == south){
        if((v[3].num == 0) && (v[4].num == 0) && (v[5].num == 0) &&
           (v[0].num == 0) && (v[1].num == 0) && (v[2].num == 0) &&
           (v[9].num == 0) && (v[10].num == 0) && (v[11].num == 0)){
            R1 = true;
        }
    } else if(origin == west){
        if((v[3].num == 0) && (v[4].num == 0) && (v[5].num == 0) &&
           (v[6].num == 0) && (v[7].num == 0) && (v[8].num == 0) &&
           (v[0].num == 0) && (v[1].num == 0) && (v[2].num == 0)){
            R1 = true;
        }
    }
    
    // R2
    if(destination == north){
        if((v[0].num == 0) && (v[2].num == 0) && (v[3].num == 0) &&
           (v[4].num == 0) && (v[6].num == 0) && (v[7].num == 0) &&
           (v[8].num == 0) && (v[9].num == 0) && (v[10].num == 0)){ // south
            R2 = true;
        }
    } else if(destination == south){
        if((v[0].num == 0) && (v[1].num == 0) && (v[2].num == 0) &&
           (v[4].num == 0) && (v[5].num == 0) && (v[6].num == 0) &&
           (v[8].num == 0) && (v[10].num == 0) && (v[11].num == 0)){ // north
            R2 = true;
        }
    } else if(destination == west){
        if((v[1].num == 0) && (v[2].num == 0) && (v[3].num == 0) &&
           (v[4].num == 0) && (v[5].num == 0) && (v[7].num == 0) &&
           (v[8].num == 0) && (v[9].num == 0) && (v[11].num == 0)){ // east
            R2 = true;
        }
    } else if(destination == east){
        if((v[0].num == 0) && (v[1].num == 0) && (v[3].num == 0) &&
           (v[5].num == 0) && (v[6].num == 0) && (v[7].num == 0) &&
           (v[9].num == 0) && (v[10].num == 0) && (v[11].num == 0)){ // west
            R2 = true;
        }
    }
    
    // R3
    if((origin == north) && (destination == west)){
        R3 = true;
        if((v[2].num != 0) || (v[4].num != 0) || (v[8].num != 0)) {
            R3 = false;
        }
    } else if((origin == south) && (destination == east)){
        R3 = true;
        if((v[0].num != 0) || (v[6].num != 0) || (v[10].num != 0)) {
            R3 = false;
        }
    } else if((origin == east) && (destination == north)){
        R3 = true;
        if((v[3].num != 0) || (v[7].num != 0) || (v[9].num != 0)) {
            R3 = false;
        }
    } else if((origin == west) && (destination == south)){
        R3 = true;
        if((v[1].num != 0) || (v[5].num != 0) || (v[11].num != 0)) {
            R3 = false;
        }
    }
    
    kprintf("%d, %d, %d\n", R1, R2, R3);
    
    // check
    while ((R1 != true) && (R2 != true) && (R3 != true)) {
        cv_wait(cvTraffic, TempLock);
    }
    
    // add car
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
    lock_acquire(TempLock);
    
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
    kprintf("car几？");
    
    cv_broadcast(cvTraffic, TempLock);
    
    lock_release(TempLock);
}
