#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char** argv){
    int num_times = 0;
    int pause_time = 0;
    int towait = 1;
    if(argc > 1){
        num_times = atoi(argv[1]); //Number of children to fork
        if(argc > 2) {
            pause_time = atoi(argv[2]); //Time to pause the main process for
            if(argc > 3) towait = atoi(argv[3]); //Should the process wait for children to finish or not?
        }
        else{
            pause_time = 2*num_times + 1;
        }
    }
    // int status;
    for(int i=0;i<num_times;i++){
        int p = fork();
        if(p==0){
            pause((2*i+1)*5);
            printf("This is child process %d\n", i+1);
            exit(0);
        }
    }
    int no_of_children = getnumchild();
    printf("This process has %d children\n", no_of_children);
    pause((pause_time/2)*2*5);
    no_of_children = getnumchild();
    printf("This process has %d children now\n", no_of_children);
    int remaining_processes=num_times;
    if(towait > 0){
        for(int i=0;i<num_times;i++){
            if(wait(0)==-1){
                break;
            }
            remaining_processes--;
        }
    }
    no_of_children = getnumchild();
    printf("This process has %d children now after waiting/not waiting\n", no_of_children);
    for(int i=0;i<remaining_processes;i++){
        wait(0);
    }
    exit(0);
}