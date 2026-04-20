#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char** argv){
    int num_times = 0;
    if(argc >= 2){
        num_times = atoi(argv[1]);
    }
    printf("This process has ID %d\n", getpid2());
    for(int i=0;i<num_times;i++){
        int p = fork();
        if(p==0){
            pause((i+1)*5);
            printf("The ID for this child process is %d\n", getpid2());
            exit(0);
        }
    }
    for(int i=0;i<num_times;i++){
        wait(0);
    }
    printf("This same process has ID %d\n", getpid2());
    exit(0);
}