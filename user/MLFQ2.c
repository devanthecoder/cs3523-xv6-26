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
    int pid = getpid();
    for(int i=0;i<1000000000;i++);
    for(int i=0;i<40000;i++){
        pid = getpid();
        pid = getpid();
        pid = getpid();
    }
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    int get = getmlfqinfo(pid, &info1);
    if(get < 0){
        printf("Retrieving info failed.\n");
        exit(0);
    }
    printf("Final level: %d\n", info1.level);
    PrintMLFQ(info1);
    printf("\n=== TEST: Validate syscall-aware demotion prevents extreme priority loss ===\n");
    if(info1.level >= 2 && info1.total_syscalls > 100000)
        printf("TEST PASSED\n");
    else
        printf("TEST FAILED: Review output\n");
    exit(0);
}