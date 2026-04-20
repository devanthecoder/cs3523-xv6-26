#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char** argv){
    int num_times = 5;
    int wait_time = num_times+1;
    int inv_pid = 1000;
    if(argc>1){
        num_times = atoi(argv[1]);
        if(argc>2) {
            wait_time = atoi(argv[2]);
            if(argc > 3){
                inv_pid = atoi(argv[3]); 
            }
        }
        else wait_time = num_times+1;
    }
    int p1 = fork();
    if(p1==0){ // Child
        char buff[13] = "Wassup\n";
        write(1, buff, 7);
        for(int i=0;i<num_times - 1;i++){
            pause(4);
            char buff1[16] = "Wassup again\n";
            write(1, buff1, 16);
        }
        exit(0); // 3
    }
    else if(p1>0){
        pause(wait_time*4+2);
        printf("No. of system calls called by child process with pid %d is %d\n", p1, getchildsyscount(p1));
        wait(0);
    }
    // Assuming that we can call getchildsyscount() for Zombie Children Processes as well.
    if(getchildsyscount(inv_pid) < 0){
        printf("The pid %d is either an invalid pid or doesn't correspond to a child of the main process\n", inv_pid);
    }
    else printf("No. of system calls called by child process with pid %d is %d\n", inv_pid, getchildsyscount(inv_pid));
    exit(0);
}