#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char** argv){
    int num_times = 5;
    if(argc > 1){
        num_times = atoi(argv[1]);
    }
    int start = getsyscount();
    for(int i=0; i<num_times; i++){
        hello();
    }
    int end = getsyscount();
    printf("No. of system calls before loop and after loop : %d\n", end - start);

    int p = fork();
    int num = 0;
    if(p==0){
        write(1, "Hello from the child process\n", 30);
        num = getsyscount();
        printf("No. of system calls called in child is %d\n", num);
        exit(0);
    }
    else if(p>0){
        pause(2);
        num = getsyscount();
        printf("The no. of systems calls so far in main process is %d\n", num);
        wait(0);
    }
    else {
        printf("Fork failed\n");
    }
    exit(0);
}