#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int n=10;
    
    for(int i=0;i<n;i++){
        int pid=fork();
        if(pid<0){
            printf("fork failed\n");
            exit(1);
        }
        if(pid==0){
            for(volatile int j=0;j<100000000;j++){}
            exit(0);
        }
    }

    int wtime,rtime,ttime;
    int total_wtime=0;
    int total_rtime=0;
    int total_ttime=0;
    
    printf("PID\tTurnaround\tWait\tResponse\n");
    for(int i=0;i<n;i++){
        int pid=waitx(0,&wtime,&rtime,&ttime);
        printf("%d\t%d\t\t%d\t%d\n",pid,ttime,wtime,rtime);
        total_wtime=total_wtime+wtime;
        total_rtime=total_rtime+rtime;
        total_ttime=total_ttime+ttime;
    }

    printf("\nAverage Turnaround: %d\n",total_ttime/n);
    printf("Average Wait: %d\n",total_wtime/n);
    printf("Average Response: %d\n",total_rtime/n);
    
    exit(0);
}