#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
int main(){
    int n=5;
    int pid;
    printf("Starting scheduler test...\n");
    for(int i=0;i<n;i++){
        pid=fork();
        if(pid<0){
            printf("fork failed\n");
            exit(1);
        }
        if(pid==0){
            if(i==0){
                for(volatile int j=0;j<2000000000;j++){}
                for(volatile int j=0;j<2000000000;j++){}
            }else if(i==1){
                for(volatile int j=0;j<50000000;j++){}
                pause(2);
                for(volatile int j=0;j<2000000000;j++){}
                for(volatile int j=0;j<2000000000;j++){}
            }else if(i==2){
                for(int j=0;j<40;j++){
                    for(volatile int k=0;k<10000000;k++){}
                    pause(2);
                }
            }else if(i==3){
                pause(10);
                for(volatile int j=0;j<2000000000;j++){}
                for(volatile int j=0;j<2000000000;j++){}
            }else{
                for(int j=0;j<10;j++){
                    for(volatile int k=0;k<300000000;k++){}
                    pause(3);
                }
            }
            exit(0);
        }
    }
    for(int i=0;i<n;i++){
        int wtime,rtime,ttime;
        int status;
        int ret=waitx(&status,&wtime,&rtime,&ttime);
        if(ret>=0){
            printf("Process %d finished: wtime=%d, rtime=%d, ttime=%d\n",ret,wtime,rtime,ttime);
        }
    }
    printf("Scheduler test finished\n");
    exit(0);
}
