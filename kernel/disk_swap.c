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
int FailingDisk;
void disk_swap_init(void){
    policy = 0;
    CURR_HEAD_POS = 0;
    initlock(&sched_lock, "disk sched");
    globalArrival = 0;
    raid_level = 5;
    FailingDisk = -1;
    memset(request_queue, 0, sizeof(request_queue));
    // printf("disk swap initialized\n");
}
int get_physical_block(int DISK, int OFFSET) {
    return SWAPSTART + (DISK * DISKSIZE) + (OFFSET * (PGSIZE / BSIZE));
}
int disk_read(void* addr, int START_BLOCK){
    int NO_OF_BLOCKS = PGSIZE / BSIZE;
    struct buf* b;
    
    // printf("reading to address %ld from BLOCK %d\n", (uint64)addr, START_BLOCK);
    for(int i=0;i<NO_OF_BLOCKS;i++){
        b = bread(ROOTDEV, START_BLOCK + i);
        if(b == 0) return -1; // bread failed
        memmove((void *)((uint64)addr + i*BSIZE), (void *)b->data, BSIZE);
        brelse(b);
    }
    // printf("Processed read\n");
    return 0;
}
int disk_write(void* addr, int START_BLOCK){
    int NO_OF_BLOCKS = PGSIZE / BSIZE;
    struct buf* b;
    // printf("writing from address %ld to BLOCK %d\n", (uint64)addr, START_BLOCK);
    for(int i=0;i<NO_OF_BLOCKS;i++){
        b = bread(ROOTDEV, START_BLOCK + i);
        if(b == 0) return -1; // bread failed
        memmove((void *)b->data, (void *)((uint64)addr + i*BSIZE), BSIZE);
        if(bwrite(b) != 0) {
            brelse(b);
            return -1; // bwrite failed
        }
        brelse(b);
    }   
    // printf("Processed write\n");
    return 0;
}
int raid_write(void* addr, int LOGICAL_BLOCK) {
    int DISK, OFFSET, DISK2;

    switch(raid_level) {
        case 0:
            DISK = LOGICAL_BLOCK % NDISK;
            OFFSET = LOGICAL_BLOCK / NDISK;
            release(&sched_lock);
            if(disk_write(addr, get_physical_block(DISK, OFFSET)) != 0) return -1;
            break;
        case 1:
            DISK = LOGICAL_BLOCK % (NDISK / 2);
            DISK2 = DISK + (NDISK / 2);
            OFFSET = LOGICAL_BLOCK / (NDISK / 2);
            release(&sched_lock);
            // printf("Beginning writes");
            if(disk_write(addr, get_physical_block(DISK, OFFSET)) != 0) return -1;
            // printf("Processed the first RAID 1 write.\n");
            if(disk_write(addr, get_physical_block(DISK2, OFFSET)) != 0) return -1;
            // printf("Processed the second RAID 1 write.\n");
            break;

        case 5: {
            int STRIPE = LOGICAL_BLOCK / (NDISK - 1);
            int SLOT   = LOGICAL_BLOCK % (NDISK - 1);  
            int P_DISK = STRIPE % NDISK;                
            DISK       = SLOT < P_DISK ? SLOT : SLOT + 1;
            OFFSET     = STRIPE;

            // 1. Setup pointers: Use 'addr' directly for the slot we are writing!
            char *data_blocks[3];
            for (int i = 0; i < 3; i++) {
                if (i == SLOT) {
                    data_blocks[i] = (char *)addr; // No kalloc needed!
                } else {
                    data_blocks[i] = kalloc();
                    if (!data_blocks[i]) panic("raid5 write: kalloc failed");
                }
            }
            
            char *parity = kalloc();
            if (!parity) panic("raid5 write: kalloc failed");

            // 2. Read ONLY the other data blocks (skip the one we are overwriting)
            release(&sched_lock);
            for (int slot = 0; slot < 3; slot++) {
                if (slot == SLOT) continue; // We already have the new data, save a disk read!
                
                int disk = slot < P_DISK ? slot : slot + 1;
                if(disk_read(data_blocks[slot], get_physical_block(disk, OFFSET)) != 0) {
                    for (int i = 0; i < 3; i++) if (i != SLOT) kfree(data_blocks[i]);
                    kfree(parity);
                    return -1;
                }
            }
            acquire(&sched_lock);

            // 3. Compute Parity (New Data ^ Old Other Data)
            memset(parity, 0, PGSIZE);
            for (int i = 0; i < PGSIZE; i++) {
                parity[i] = data_blocks[0][i] ^ data_blocks[1][i] ^ data_blocks[2][i];
            }

            // 4. Write new data and new parity
            release(&sched_lock);
            if(disk_write(addr, get_physical_block(DISK, OFFSET)) != 0 || 
            disk_write(parity, get_physical_block(P_DISK, OFFSET)) != 0) {
                for (int i = 0; i < 3; i++) if (i != SLOT) kfree(data_blocks[i]);
                kfree(parity);
                return -1;
            }

            // 5. Clean up ONLY the memory we actually allocated
            for (int i = 0; i < 3; i++) {
                if (i != SLOT) {
                    kfree(data_blocks[i]);
                }
            }
            kfree(parity);
            break;
        }
    }
    return 0;
}

int raid_read(void* addr, int LOGICAL_BLOCK) {
    int DISK, OFFSET;

    switch(raid_level) {
        case 0:
            DISK = LOGICAL_BLOCK % NDISK;
            OFFSET = LOGICAL_BLOCK / NDISK;
            if(DISK == FailingDisk) {
                panic("disk failure");
            } else {
                release(&sched_lock);
                disk_read(addr, get_physical_block(DISK, OFFSET));
            }
            break;
            
        case 1:
            DISK = LOGICAL_BLOCK % (NDISK / 2);
            OFFSET = LOGICAL_BLOCK / (NDISK / 2);
            if(DISK == FailingDisk) {
                int DISK2 = DISK + (NDISK / 2);
                release(&sched_lock);
                disk_read(addr, get_physical_block(DISK2, OFFSET));
            } else {
                release(&sched_lock);
                disk_read(addr, get_physical_block(DISK, OFFSET));
            }
            break;
            
        case 5: {
            int STRIPE = LOGICAL_BLOCK / (NDISK - 1);
            int SLOT   = LOGICAL_BLOCK % (NDISK - 1);
            int P_DISK = STRIPE % NDISK;
            DISK       = SLOT < P_DISK ? SLOT : SLOT + 1;
            OFFSET     = STRIPE;
            
            if(DISK == FailingDisk) {
                char *tmp = kalloc();
                if (!tmp) panic("raid5 reconstruct: kalloc failed");

                // start with zeros in addr
                memset(addr, 0, PGSIZE);

                // XOR every disk in the stripe EXCEPT the failed one
                // this includes the parity disk — P XOR d0 XOR d1 = missing d2
                // FailingDisk = -1;
                release(&sched_lock);
                for (int d = 0; d < NDISK; d++) {
                    if (d == DISK) continue;
                    disk_read(tmp, get_physical_block(d, OFFSET));
                    for (int i = 0; i < PGSIZE; i++)
                    ((char*)addr)[i] ^= tmp[i];
                }
                kfree(tmp);
            } else {
                release(&sched_lock);
                disk_read(addr, get_physical_block(DISK, OFFSET));
            }
            break;
        }
    }
    return 0;
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
    acquire(&(choice->p->lock));
    if(rw == 0){
        choice->p->disk_reads++;
    }
    else{
        choice->p->disk_writes++;
    }
    choice->p->avg_disk_latency = (choice->p->avg_disk_latency * (choice->p->disk_reads + choice->p->disk_writes - 1) + choice->latency * 100)/(choice->p->disk_reads + choice->p->disk_writes); 
    release(&(choice->p->lock));
    choice->arrivalOrder = 0;
    choice->latency = 0;
    choice->rw = 0;
    choice->START_BLOCK = 0;
    choice->addr = 0;
    CURR_HEAD_POS = REQUEST_BLOCK + (PGSIZE/BSIZE);
    int result;
    if(rw == 0){ //read
        result = raid_read((void*)addr, REQUEST_BLOCK);
    }
    else{ //write
        result = raid_write((void*)addr, REQUEST_BLOCK);
    }
    if(result != 0) {
        // disk I/O failed, kill the process
        setkilled(choice->p);
    }
}