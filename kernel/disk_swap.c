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
    raid_level = 0;
    FailingDisk = -1;
    memset(request_queue, 0, sizeof(request_queue));
}

int get_physical_block(int DISK, int OFFSET) {
    return SWAPSTART + (DISK * DISKSIZE) + (OFFSET * (PGSIZE / BSIZE));
}

int disk_read(void* addr, int START_BLOCK) {
    int NO_OF_BLOCKS = PGSIZE / BSIZE;
    struct buf* b;
    // printf("reading to address %ld from BLOCK %d\n", (uint64)addr, START_BLOCK);

    for(int i = 0; i < NO_OF_BLOCKS; i++) {
        int phys = 0;

        switch(raid_level) {
            case 0: {
                int LB   = START_BLOCK + i;
                int DISK = LB % NDISK;
                int OFF  = LB / NDISK;
                phys = get_physical_block(DISK, OFF);
                break;
            }
            case 1: {
                int LB   = START_BLOCK + i;
                int DISK = LB % (NDISK / 2);
                int OFF  = LB / (NDISK / 2);
                phys = get_physical_block(DISK, OFF);
                if(DISK == FailingDisk)
                    phys += (NDISK / 2) * DISKSIZE;
                break;
            }
            case 5: {
                int LB     = START_BLOCK + i;
                int STRIPE = LB / (NDISK - 1);
                int SLOT   = LB % (NDISK - 1);
                int P_DISK = STRIPE % NDISK;
                int DISK   = SLOT < P_DISK ? SLOT : SLOT + 1;
                phys = get_physical_block(DISK, STRIPE);
                break;
            }
        }

        b = bread(ROOTDEV, phys);
        if(b == 0) return -1;
        memmove((void*)((uint64)addr + i * BSIZE), b->data, BSIZE);
        brelse(b);
    }
    return 0;
}

int disk_write(void* addr, int START_BLOCK) {
    int NO_OF_BLOCKS = PGSIZE / BSIZE;
    struct buf* b, *b1 = 0;
    // printf("writing from address %ld to BLOCK %d\n", (uint64)addr, START_BLOCK);
    int isRaid1 = 0;
    int phys = 0;

    for(int i = 0; i < NO_OF_BLOCKS; i++) {
        switch(raid_level) {
            case 0: {
                int LB   = START_BLOCK + i;
                int DISK = LB % NDISK;
                int OFF  = LB / NDISK;
                phys = get_physical_block(DISK, OFF);
                break;
            }
            case 1: {
                isRaid1 = 1;
                int LB   = START_BLOCK + i;
                int DISK = LB % (NDISK / 2);
                int OFF  = LB / (NDISK / 2);
                phys = get_physical_block(DISK, OFF);
                break;
            }
            case 5: {
                int LB     = START_BLOCK + i;
                int STRIPE = LB / (NDISK - 1);
                int SLOT   = LB % (NDISK - 1);
                int P_DISK = STRIPE % NDISK;
                int DISK   = SLOT < P_DISK ? SLOT : SLOT + 1;
                phys = get_physical_block(DISK, STRIPE);
                break;
            }
        }

        if(isRaid1) {
            b1 = bread(ROOTDEV, phys + (NDISK / 2) * DISKSIZE);
            if(b1 == 0) return -1;
            memmove(b1->data, (void*)((uint64)addr + i * BSIZE), BSIZE);
            if(bwrite(b1) != 0) { brelse(b1); return -1; }
            brelse(b1);
        }
        b = bread(ROOTDEV, phys);
        if(b == 0) return -1;
        memmove(b->data, (void*)((uint64)addr + i * BSIZE), BSIZE);
        if(bwrite(b) != 0) { brelse(b); return -1; }
        brelse(b);
    }
    return 0;
}

int raid_write(void* addr, int LOGICAL_BLOCK) {
    switch(raid_level) {
        case 0:
            release(&sched_lock);
            if(disk_write(addr, LOGICAL_BLOCK) != 0) return -1;
            break;

        case 1: {
            release(&sched_lock);
            if(disk_write(addr, LOGICAL_BLOCK) != 0) return -1;
            break;
        }

        case 5: {
            release(&sched_lock);
            int LB, STRIPE, SLOT, P_DISK, DISK, OFFSET;
            for (int i = 0; i < PGSIZE / BSIZE; i++) {
                LB     = LOGICAL_BLOCK + i;
                STRIPE = LB / (NDISK - 1);
                SLOT   = LB % (NDISK - 1);
                P_DISK = STRIPE % NDISK;
                DISK   = SLOT < P_DISK ? SLOT : SLOT + 1;
                OFFSET = STRIPE;

                char *old_data   = kalloc();
                char *old_parity = kalloc();
                char *new_parity = kalloc();
                if (!old_data || !old_parity || !new_parity) panic("raid5 write: kalloc failed");

                int data_phys   = get_physical_block(DISK, OFFSET);
                int parity_phys = get_physical_block(P_DISK, OFFSET);

                // Read old data
                struct buf *b = bread(ROOTDEV, data_phys);
                if (!b) { kfree(old_data); kfree(old_parity); kfree(new_parity); return -1; }
                memmove(old_data, b->data, BSIZE);
                brelse(b);

                // Read old parity
                b = bread(ROOTDEV, parity_phys);
                if (!b) { kfree(old_data); kfree(old_parity); kfree(new_parity); return -1; }
                memmove(old_parity, b->data, BSIZE);
                brelse(b);

                // new_parity = old_parity ^ old_data ^ new_data
                char *new_data = (char *)addr + i * BSIZE;
                for (int j = 0; j < BSIZE; j++)
                    new_parity[j] = old_parity[j] ^ old_data[j] ^ new_data[j];

                // Write new data
                b = bread(ROOTDEV, data_phys);
                if (!b) { kfree(old_data); kfree(old_parity); kfree(new_parity); return -1; }
                memmove(b->data, new_data, BSIZE);
                if (bwrite(b) != 0) { brelse(b); kfree(old_data); kfree(old_parity); kfree(new_parity); return -1; }
                brelse(b);

                // Write new parity
                b = bread(ROOTDEV, parity_phys);
                if (!b) { kfree(old_data); kfree(old_parity); kfree(new_parity); return -1; }
                memmove(b->data, new_parity, BSIZE);
                if (bwrite(b) != 0) { brelse(b); kfree(old_data); kfree(old_parity); kfree(new_parity); return -1; }
                brelse(b);

                kfree(old_data);
                kfree(old_parity);
                kfree(new_parity);
            }
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
            }
            release(&sched_lock);
            disk_read(addr, LOGICAL_BLOCK);
            break;

        case 1: {
            release(&sched_lock);
            disk_read(addr, LOGICAL_BLOCK);
            break;
        }

        case 5: {
            release(&sched_lock);
            int phys, LB, STRIPE, SLOT, P_DISK;
            char* dest;
            for (int i = 0; i < PGSIZE / BSIZE; i++) {
                LB      = LOGICAL_BLOCK + i;
                STRIPE  = LB / (NDISK - 1);
                SLOT    = LB % (NDISK - 1);
                P_DISK  = STRIPE % NDISK;
                DISK    = SLOT < P_DISK ? SLOT : SLOT + 1;
                OFFSET  = STRIPE;
                phys    = get_physical_block(DISK, OFFSET);
                dest    = (char *)addr + i * BSIZE;

                if (DISK == FailingDisk) {
                    char *tmp = kalloc();
                    if (!tmp) panic("raid5 read reconstruct: kalloc failed");

                    // Start with parity
                    struct buf *b = bread(ROOTDEV, get_physical_block(P_DISK, OFFSET));
                    if (!b) { kfree(tmp); return -1; }
                    memmove(tmp, b->data, BSIZE);
                    brelse(b);

                    // XOR every other data disk in this stripe
                    for (int d = 0; d < NDISK; d++) {
                        if (d == DISK || d == P_DISK) continue;
                        b = bread(ROOTDEV, get_physical_block(d, OFFSET));
                        if (!b) { kfree(tmp); return -1; }
                        for (int j = 0; j < BSIZE; j++)
                            tmp[j] ^= b->data[j];
                        brelse(b);
                    }

                    memmove(dest, tmp, BSIZE);
                    kfree(tmp);
                } else {
                    struct buf *b = bread(ROOTDEV, phys);
                    if (!b) return -1;
                    memmove(dest, b->data, BSIZE);
                    brelse(b);
                }
            }
            break;
        }
    }
    return 0;
}

void send_request(uint64 addr, int rw, int REQUESTED_BLOCK){
    acquire(&sched_lock);
    for(struct request* req = request_queue; req < &request_queue[NREQUEST]; req++){
        if(req->addr == 0){
            req->addr = (void *)addr;
            req->rw = rw;
            req->START_BLOCK = REQUESTED_BLOCK;
            int val = REQUESTED_BLOCK - CURR_HEAD_POS;
            int abs_val = val < 0? -val: val;
            req->arrivalOrder = globalArrival;
            globalArrival++;
            req->latency = abs_val + C_R;
            req->p = myproc();
            break;
        }
    }
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

    if(choice == 0) panic("no request found");
    int REQUEST_BLOCK = choice->START_BLOCK, rw = choice->rw;
    uint64 addr = (uint64)choice->addr;
    acquire(&(choice->p->lock));
    if(rw == 0){
        choice->p->disk_reads++;
    } else {
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
    if(rw == 0){
        result = raid_read((void*)addr, REQUEST_BLOCK);
    } else {
        result = raid_write((void*)addr, REQUEST_BLOCK);
    }
    if(result != 0) {
        setkilled(choice->p);
    }
}