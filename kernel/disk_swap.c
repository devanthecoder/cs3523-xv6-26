#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "buf.h"
#include "proc.h"
#include "disk_swap.h"
struct request request_queue[NREQUEST];
int CURR_HEAD_POS;
int policy;
uint64 globalArrival;
struct spinlock sched_lock;
int raid_level;
void disk_swap_init(void){
    policy = 0;
    CURR_HEAD_POS = 0;
    initlock(&sched_lock, "disk sched");
    globalArrival = 0;
    raid_level = 5;
    memset(request_queue, 0, sizeof(request_queue));
    // printf("disk swap initialized\n");
}
int get_physical_block(int DISK, int OFFSET) {
    return SWAPSTART + (DISK * DISKSIZE) + (OFFSET * (PGSIZE / BSIZE));
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
void raid_write(void* addr, int LOGICAL_BLOCK) {
    int DISK, OFFSET, DISK2;

    switch(raid_level) {
        case 0:
            DISK = LOGICAL_BLOCK % NDISK;
            OFFSET = LOGICAL_BLOCK / NDISK;
            disk_write(addr, get_physical_block(DISK, OFFSET));
            break;

        case 1:
            DISK = LOGICAL_BLOCK % (NDISK / 2);
            DISK2 = DISK + (NDISK / 2);
            OFFSET = LOGICAL_BLOCK / (NDISK / 2);
            disk_write(addr, get_physical_block(DISK, OFFSET));
            disk_write(addr, get_physical_block(DISK2, OFFSET));
            break;

        case 5: {
            // stripe row and slot within stripe
            int STRIPE = LOGICAL_BLOCK / (NDISK - 1);
            int SLOT   = LOGICAL_BLOCK % (NDISK - 1);  // 0, 1, or 2
            int P_DISK = STRIPE % NDISK;                // parity disk rotates
            // data disk: skip over parity disk
            DISK       = SLOT < P_DISK ? SLOT : SLOT + 1;
            OFFSET     = STRIPE;

            // figure out the other two data slots in this stripe
            // (the two slot values in {0,1,2} that aren't SLOT)
            int other0 = (SLOT == 0) ? 1 : 0;
            int other1 = (SLOT == 2) ? 1 : 2;
            // map those slots to physical disks (skipping P_DISK)
            int disk_other0 = other0 < P_DISK ? other0 : other0 + 1;
            int disk_other1 = other1 < P_DISK ? other1 : other1 + 1;

            // read the other two data blocks in this stripe
            char *d_other0 = kalloc();
            char *d_other1 = kalloc();
            char *parity   = kalloc();
            if (!d_other0 || !d_other1 || !parity)
                panic("raid5 write: kalloc failed");

            disk_read(d_other0, get_physical_block(disk_other0, OFFSET));
            disk_read(d_other1, get_physical_block(disk_other1, OFFSET));

            // P = new_data XOR other0 XOR other1
            char *new_data = (char *)addr;
            for (int i = 0; i < PGSIZE; i++)
                parity[i] = new_data[i] ^ d_other0[i] ^ d_other1[i];

            // write data and parity
            disk_write(addr,    get_physical_block(DISK,   OFFSET));
            disk_write(parity,  get_physical_block(P_DISK, OFFSET));

            kfree(d_other0);
            kfree(d_other1);
            kfree(parity);
            break;
        }
    }
}

void raid_read(void* addr, int LOGICAL_BLOCK) {
    int DISK, OFFSET;

    switch(raid_level) {
        case 0:
            DISK = LOGICAL_BLOCK % NDISK;
            OFFSET = LOGICAL_BLOCK / NDISK;
            disk_read(addr, get_physical_block(DISK, OFFSET));
            break;

        case 1:
            DISK = LOGICAL_BLOCK % (NDISK / 2);
            OFFSET = LOGICAL_BLOCK / (NDISK / 2);
            disk_read(addr, get_physical_block(DISK, OFFSET));
            break;

        case 5: {
            int STRIPE = LOGICAL_BLOCK / (NDISK - 1);
            int SLOT   = LOGICAL_BLOCK % (NDISK - 1);
            int P_DISK = STRIPE % NDISK;
            DISK       = SLOT < P_DISK ? SLOT : SLOT + 1;
            OFFSET     = STRIPE;

            // normal path: data disk is alive, just read it
            disk_read(addr, get_physical_block(DISK, OFFSET));
            break;
        }
    }
}

// call this instead of raid_read when DISK is dead
// reconstructs the missing block by XORing all surviving disks
void raid5_reconstruct(void* addr, int LOGICAL_BLOCK) {
    int STRIPE = LOGICAL_BLOCK / (NDISK - 1);
    int SLOT   = LOGICAL_BLOCK % (NDISK - 1);
    int P_DISK = STRIPE % NDISK;
    int DISK   = SLOT < P_DISK ? SLOT : SLOT + 1;
    int OFFSET = STRIPE;

    char *tmp = kalloc();
    if (!tmp) panic("raid5 reconstruct: kalloc failed");

    // start with zeros in addr
    memset(addr, 0, PGSIZE);

    // XOR every disk in the stripe EXCEPT the failed one
    // this includes the parity disk — P XOR d0 XOR d1 = missing d2
    for (int d = 0; d < NDISK; d++) {
        if (d == DISK) continue;
        disk_read(tmp, get_physical_block(d, OFFSET));
        for (int i = 0; i < PGSIZE; i++)
            ((char*)addr)[i] ^= tmp[i];
    }

    kfree(tmp);
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
            req->p = myproc();
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
    if(policy == 0) { // FCFS
        for(struct request* req = request_queue; req < &request_queue[NREQUEST]; req++) {
            if(req->addr != 0) {
                if (choice == 0 || req->arrivalOrder < choice->arrivalOrder || 
                    (req->arrivalOrder == choice->arrivalOrder && req->p->level < choice->p->level)) {
                    choice = req;
                }
            }
        }
    } else { // SSTF
        for(struct request* req = request_queue; req < &request_queue[NREQUEST]; req++) {
            if(req->addr != 0) {
                if (choice == 0 || req->latency < choice->latency || 
                    (req->latency == choice->latency && req->p->level < choice->p->level)) {
                    choice = req;
                }
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
    release(&sched_lock);
    if(rw == 0){ //read

        raid_read((void*)addr, REQUEST_BLOCK);
    }
    else{ //write
        raid_write((void*)addr, REQUEST_BLOCK);
    }
    CURR_HEAD_POS = REQUEST_BLOCK + (PGSIZE/BSIZE);
}