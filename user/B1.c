#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char** argv){
    int wait_time = 5;
    int let_parent_exit = 0;
    if(argc > 1){
        wait_time = atoi(argv[1]);
        if(argc > 2) let_parent_exit = atoi(argv[2]);
    }
    int ppid = getppid();
    if(ppid < 0){
        printf("This process has no parent, %d", ppid);
    }
    else{
        printf("The ID for parent process of this process is %d", ppid);
    }
    printf("\n");
    int pid = getpid();
    int p = fork();
    if(p==0){
        if(let_parent_exit>0){
            pause(wait_time*5);
        }
        if(getppid()==pid){
            printf("This is a child process of current running process and hence parent pid : %d = pid of current running process : %d\n", getppid(), pid);
        }
        else{
            printf("Current process finished running and so the child process has been reparented to initproc, whose ID is %d (1)\n", getppid());
        }
        exit(0);
    }
    else if(p > 0){
        if(let_parent_exit<=0){
            wait(0);
            pause(wait_time*5);
        }
        // exit(0);
    }
    else{
        printf("Fork failed\n");
    }
    exit(0);
}