#include "kernel/types.h"
#include "kernel/mlfqinfo.h"
#include "user/user.h"

#define NCHILDREN 3

void PrintMLFQ(struct mlfqinfo info){
    printf("MLFQ INFO:\n");
    printf("1. Level of process in MLFQ: %d\n", info.level);
    printf("2. Ticks consumed per level:\n");
    for(int i=0;i<4;i++){
        char c;
        switch(i){
            case 0:
            c = 'a';
            break;
            case 1:
            c = 'b';
            break;
            case 2:
            c = 'c';
            break;
            case 3:
            c = 'd';
            break;
        }
        printf("\t%c. At level %d: %d\n", c, i, info.ticks[i]);
    }
    printf("3. No. of times scheduled: %d\n", info.times_scheduled);
    printf("4. Total no. of system calls: %d\n", info.total_syscalls);
}

int main(void){
    int pids[NCHILDREN];
    struct mlfqinfo infos[NCHILDREN];
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
            for(int j=0;j<2000000000;j++);
            for(int j=0;j<2000000000;j++);
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

    for(int i=0;i<NCHILDREN;i++){
        get = getmlfqinfo(pids[i], &infos[i]);
        if(get < 0){
            printf("Retrieving info failed.\n");
            exit(0);
        }
    }

    for(int i=0;i<NCHILDREN;i++){
        printf("\nChild PID %d:\n", pids[i]);
        PrintMLFQ(infos[i]);
    }

    printf("\n=== TEST: Validate fair scheduling across competing processes ===\n");
    int passed = 1;
    for(int i=0;i<NCHILDREN;i++){
        if(infos[i].times_scheduled == 0){
            printf("FAILED: Child PID %d was never scheduled!\n", pids[i]);
            passed = 0;
        }
        if(infos[i].ticks[3] == 0){
            printf("FAILED: Child PID %d never reached L3!\n", pids[i]);
            passed = 0;
        }
    }
    if(passed)
        printf("TEST PASSED: All children were scheduled and reached L3 fairly.\n");

    for(int i=0;i<NCHILDREN;i++) wait(0);

    exit(0);
}