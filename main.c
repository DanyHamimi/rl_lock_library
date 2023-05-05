#include <stdio.h>
#include <stdlib.h>
#include "rl_lock_library.h"

int main(){
    //rl_init_library();
    rl_descriptor fd = rl_open("test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(fd.d == -1){
        printf("Error opening file\n");
        exit(1);
    }
    printf("File opened\n");
    rl_close(fd);
    printf("File closed\n");
    return 0;
    
}