#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "buf.h"
#include "disk_swap.h"
struct request request_queue[NREQUEST];
int CURR_HEAD_POS = SWAPSIZE;
int policy = 0;
uint64 globalArrival = 0;
struct spinlock sched_lock;
void disk_swap_init(void){
    policy = 1;
    CURR_HEAD_POS = SWAPSTART;
    initlock(&sched_lock, "disk sched");
    globalArrival = 0;
    // printf("disk swap initialized\n");
}
void send_request(uint64 addr, int rw, int REQUESTED_BLOCK){
    acquire(&sched_lock);
    // printf("current head position: %d\n", CURR_HEAD_POS);
    for(struct request* req = request_queue; req < &request_queue[NREQUEST]; req++){
        if(req->addr == 0){
            req->addr = (void *)addr;
            req->rw = rw;
            req->START_BLOCK = REQUESTED_BLOCK;
            int val = REQUESTED_BLOCK - CURR_HEAD_POS;
            int abs_val = val < 0? -val: val;
            // if(policy == 0){
            req->arrivalOrder = globalArrival;
            globalArrival++;
            req->latency = abs_val + C_R;
            break;
            // }
            // else{
            // }
        }
    }
    // printf("request sent\n");
    sched_disk(policy);
    
}
void sched_disk(int policy){
    struct request* choice = 0;
    if(policy == 0){ //FCFS
        int min = globalArrival;
        for(struct request* req = request_queue;req < &request_queue[NREQUEST]; req++){
            if(req->arrivalOrder <= min && req->addr != 0){
                min = req->arrivalOrder;
                choice = req;
                // printf("new min\n");
            }
        }
    }
    else{ //SSTF
        int min = SWAPSIZE + C_R;
        for(struct request* req = request_queue;req < &request_queue[NREQUEST]; req++){
            if(req->latency <= min && req->addr != 0){
                min = req->latency;
                choice = req;
                // printf("new min\n");
            }
        }
    }
    // printf("Chosen request below: \n");
    // printf("%ld %d %d\n", (uint64)choice->addr, choice->rw, choice->START_BLOCK);
    if(choice == 0) panic("no request found");
    int REQUEST_BLOCK = choice->START_BLOCK, rw = choice->rw;
    uint64 addr = (uint64)choice->addr;
    choice->arrivalOrder = 0;
    choice->latency = 0;
    choice->rw = 0;
    choice->START_BLOCK = 0;
    choice->addr = 0;
    if(policy == 0){
        printf("FCFS\n");
    }
    else{
        printf("SSTF\n");
    }
    release(&sched_lock);
    if(rw == 0){ //read

        disk_read((void*)addr, REQUEST_BLOCK);
    }
    else{ //write
        disk_write((void*)addr, REQUEST_BLOCK);
    }
    CURR_HEAD_POS = REQUEST_BLOCK + (PGSIZE/BSIZE);
}
void disk_read(void* addr, int START_BLOCK){
    int NO_OF_BLOCKS = PGSIZE / BSIZE;
    struct buf* b;
    for(int i=0;i<NO_OF_BLOCKS;i++){
        b = bread(ROOTDEV, START_BLOCK + i);
        memmove((void *)((uint64)addr + i*BSIZE), (void *)b->data, BSIZE);
        brelse(b);
    }
    // printf("Processed read\n");
}
void disk_write(void* addr, int START_BLOCK){
    int NO_OF_BLOCKS = PGSIZE / BSIZE;
    struct buf* b;
    for(int i=0;i<NO_OF_BLOCKS;i++){
        b = bread(ROOTDEV, START_BLOCK + i);
        memmove((void *)b->data, (void *)((uint64)addr + i*BSIZE), BSIZE);
        bwrite(b);
        brelse(b);
    }   
    // printf("Processed write\n");
}