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
    printf("3. Times scheduled: %d\n", info.times_scheduled);
    printf("4. Total syscalls: %d\n", info.total_syscalls);
}
int main(void){
    for(int i=0;i<3;i++){
        hello();
    }
    struct mlfqinfo info1;
    struct mlfqinfo info2;
    struct mlfqinfo info3;
    struct mlfqinfo info4;
    struct mlfqinfo info5;
    int pid = getpid();
    int get;
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    get = getmlfqinfo(pid, &info1);
    if(get < 0){
        printf("Retrieving info failed.\n");
        exit(0);
    }
    printf("After initial CPU work - Level: %d\n", info1.level);
    PrintMLFQ(info1);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    get = getmlfqinfo(pid, &info2);
    if(get < 0){
        printf("Retrieving info failed.\n");
        exit(0);
    }
    printf("After more CPU work - Level: %d\n", info2.level);
    PrintMLFQ(info2);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    get = getmlfqinfo(pid, &info3);
    if(get < 0){
        printf("Retrieving info failed.\n");
        exit(0);
    }
    printf("After even more CPU work - Level: %d\n", info3.level);
    PrintMLFQ(info3);
    int cpid = fork();
    if(cpid<0){
        printf("Fork failed\n");
        exit(0);
    }
    else if(cpid==0){
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        get = getmlfqinfo(getpid(), &info4);
        if(get < 0){
            printf("Retrieving info failed.\n");
            exit(0);
        }
        printf("Child after CPU work - Level: %d\n", info4.level);
        PrintMLFQ(info4);
        exit(0);
    }
    else{
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        get = getmlfqinfo(getpid(), &info5);
        if(get < 0){
            printf("Retrieving info failed.\n");
            exit(0);
        }
        printf("Parent after child work - Level: %d\n", info5.level);
        PrintMLFQ(info5);
        wait(0);
    }
    printf("\n=== TEST: Validate CPU-bound process demotion through levels 0→1→2→3 ===\n");
    if(info3.level == 3 && info3.ticks[0] >= 2 && info3.ticks[1] >= 4 && info3.ticks[2] >= 8)
        printf("TEST PASSED\n");
    else
        printf("TEST FAILED: Review output\n");
    exit(0);
}