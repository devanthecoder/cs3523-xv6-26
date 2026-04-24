struct request{
    struct proc* p;
    void* addr;
    int rw;
    int latency;
    int arrivalOrder;
    int START_BLOCK;
};
extern struct request request_queue[NREQUEST];
extern int CURR_HEAD_POS;
extern int policy;
extern uint64 globalArrival;
extern struct spinlock sched_lock;
extern int raid_level;
extern int FailingDisk;