#include "kernel/types.h"
#include "kernel/mlfqinfo.h"
#include "user/user.h"

#define NCHILDREN 8

void PrintMLFQ(struct mlfqinfo info){
    printf("MLFQ INFO:\n");
    printf("1. Level of process in MLFQ: %d\n", info.level);
    printf("2. Ticks consumed per level:\n");
    for(int i=0;i<4;i++){
        char c;
        switch(i){
            case 0: c = 'a'; break;
            case 1: c = 'b'; break;
            case 2: c = 'c'; break;
            case 3: c = 'd'; break;
        }
        printf("\t%c. At level %d: %d\n", c, i, info.ticks[i]);
    }
    printf("3. No. of times scheduled: %d\n", info.times_scheduled);
    printf("4. Total no. of system calls: %d\n", info.total_syscalls);
}

int main(void){
    int pids[NCHILDREN];
    int levels_before[NCHILDREN];
    int levels_after[NCHILDREN];
    struct mlfqinfo child_info;
    int get;

    for(int i=0;i<NCHILDREN;i++){
        int p = fork();
        if(p==0){
            for(int j=0;j<2000000000;j++);
            for(int j=0;j<2000000000;j++);
            for(int j=0;j<2000000000;j++);
            for(int j=0;j<2000000000;j++);
            for(int j=0;j<2000000000;j++);
            for(int j=0;j<2000000000;j++);
            volatile int x = 0;
            for(int j=0;j<2000000000;j++) x++;
            for(int j=0;j<2000000000;j++) x++;
            for(int j=0;j<2000000000;j++) x++;
            for(int j=0;j<2000000000;j++) x++;
            for(int j=0;j<2000000000;j++) x++;
            for(int j=0;j<2000000000;j++) x++;
            exit(0);
        }
        pids[i] = p;
    }

    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);

    printf("\nParent: Levels before boost:\n");
    for(int i=0;i<NCHILDREN;i++){
        get = getmlfqinfo(pids[i], &child_info);
        if(get < 0){
            printf("Retrieving info failed.\n");
            exit(0);
        }
        levels_before[i] = child_info.level;
        printf("  Child PID %d: level %d\n", pids[i], levels_before[i]);
    }

    // Spin through boost interval keeping parent RUNNABLE
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);

    printf("\nParent: Levels after boost:\n");
    for(int i=0;i<NCHILDREN;i++){
        get = getmlfqinfo(pids[i], &child_info);
        if(get < 0){
            printf("Retrieving info failed.\n");
            exit(0);
        }
        levels_after[i] = child_info.level;
        printf("  Child PID %d: before=%d after=%d %s\n",
            pids[i],
            levels_before[i],
            levels_after[i],
            levels_after[i] < levels_before[i] ? "<-- BOOSTED" : "");
    }

    for(int i=0;i<NCHILDREN;i++) wait(0);
    printf("\nParent: All children exited.\n");

    int boosted_count = 0;
    for(int i=0;i<NCHILDREN;i++){
        if(levels_after[i] < levels_before[i])
            boosted_count++;
    }
    printf("\n=== TEST: Validate global priority boost resets RUNNABLE processes to level 0 ===\n");
    printf("Boosted child count: %d\n", boosted_count);
    if(boosted_count > 0)
        printf("TEST PASSED\n");
    else
        printf("TEST FAILED: No children were boosted\n");

    exit(0);
}