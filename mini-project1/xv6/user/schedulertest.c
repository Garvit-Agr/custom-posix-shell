#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define NFORK 5
#define IOPS 10000000

int main() {
  int n, pid;
  
  printf("Starting MLFQ test with %d processes...\n", NFORK);

  for(n=0; n<NFORK; n++) {
    pid=fork();
    if(pid<0)
      break;
    if(pid==0) {
      volatile int i, j;
      for (i=0; i<100; i++) {
        for (j=0; j<IOPS; j++) {
          // Empty loop to consume clock ticks
        }
      }
      printf("Process %d finished.\n", getpid());
      exit(0);
    }
  }

  for(; n > 0; n--) {
    if(wait(0)>=0) {} 
  }
  
  printf("All processes completed.\n");
  exit(0);
}