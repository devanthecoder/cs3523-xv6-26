#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char** argv){
    int num_times = 5;
    if(argc >= 2){
        num_times = atoi(argv[1]);
    }
    for(int i=0;i<num_times;i++){
        hello();
    }
    exit(0);
}