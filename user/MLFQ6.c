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
    int get;
    struct mlfqinfo cpu_info, io_info;

    // CPU-bound process
    int cpu_pid = fork();
    if(cpu_pid == 0){
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        exit(0);
    }

    // I/O-bound process (simulated by frequent syscalls)
    int io_pid = fork();
    if(io_pid == 0){
        for(int i=0;i<200000;i++){
            getpid();
            getpid();
            getpid();
        }
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        exit(0);
    }

    // Parent spins to give children time to run and settle into their levels
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);

    // Query both children while they are still alive
    get = getmlfqinfo(cpu_pid, &cpu_info);
    if(get < 0){
        printf("Retrieving CPU-bound info failed.\n");
        exit(0);
    }
    get = getmlfqinfo(io_pid, &io_info);
    if(get < 0){
        printf("Retrieving I/O-bound info failed.\n");
        exit(0);
    }

    printf("CPU-bound process state:\n");
    PrintMLFQ(cpu_info);

    printf("I/O-bound process state:\n");
    PrintMLFQ(io_info);

    printf("\n=== TEST: Verify CPU-bound level > I/O-bound level ===\n");
    if(cpu_info.level > io_info.level)
        printf("TEST PASSED: CPU-bound at level %d, I/O-bound at level %d\n", cpu_info.level, io_info.level);
    else
        printf("TEST FAILED: CPU-bound at level %d, I/O-bound at level %d\n", cpu_info.level, io_info.level);

    wait(0);
    wait(0);

    exit(0);
}