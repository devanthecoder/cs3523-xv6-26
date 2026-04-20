#include "kernel/types.h"
#include "kernel/mlfqinfo.h"
#include "user/user.h"
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
    int num_times = 3;
    for(int i=0;i<num_times;i++){
        hello();
    }
    int pid = getpid();
    int pids[3];
    struct mlfqinfo info1;
    struct mlfqinfo child_infos[3];
    int get;
    for(int i=0;i<num_times;i++){
        int p = fork();
        if(p==0){
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
    // Parent competes with children simultaneously
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    // Query all while children still alive
    get = getmlfqinfo(pid, &info1);
    if(get < 0){
        printf("Retrieving info failed.\n");
        exit(0);
    }
    printf("Parent level: %d\n", info1.level);
    PrintMLFQ(info1);
    for(int i=0;i<num_times;i++){
        get = getmlfqinfo(pids[i], &child_infos[i]);
        if(get < 0){
            printf("Retrieving info failed.\n");
            exit(0);
        }
        printf("Child PID %d level: %d\n", pids[i], child_infos[i].level);
        PrintMLFQ(child_infos[i]);
    }
    for(int i=0;i<num_times;i++) wait(0);
    printf("\n=== TEST: Validate fair scheduling and no starvation ===\n");
    int passed = 1;
    for(int i=0;i<num_times;i++){
        if(child_infos[i].times_scheduled == 0){
            printf("FAILED: Child PID %d was never scheduled!\n", pids[i]);
            passed = 0;
        }
    }
    if(passed)
        printf("TEST PASSED: All processes were scheduled fairly.\n");
    exit(0);
}