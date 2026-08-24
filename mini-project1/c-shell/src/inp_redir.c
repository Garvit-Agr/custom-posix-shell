#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "parser.h"
#include "inp_redir.h"

int inp_redir(tknll *head, pid_t *helper_pid) {
    *helper_pid=-1;
    char *in_files[100];
    int in_cnt=0;
    
    tknll *ptr=head;
    
    while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP) {
        if(ptr->type==OP_LT) {
            if(ptr->next!=NULL && ptr->next->type==WORD) {
                in_files[in_cnt++]=ptr->next->tkn;
            }
        }
        ptr=ptr->next;
    }

    if(in_cnt==0) {
        return STDIN_FILENO;
    }

    for(int i=0; i<in_cnt; i++) {
        if(access(in_files[i], R_OK)!=0) {
            printf("cshell: no such file or directory\n");
            return -1;
        }
    }

    if(in_cnt==1) {
        int fd=open(in_files[0], O_RDONLY);
        if(fd==-1) {
            printf("cshell: no such file or directory\n");
            return -1;
        }
        return fd;
    }

    int pfd[2];
    if(pipe(pfd)==-1) {
        perror("pipe");
        return -1;
    }

    pid_t pid=fork();
    if(pid==-1) {
        perror("fork");
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }

    if(pid==0) {
        close(pfd[0]);
        
        for(int i=0; i<in_cnt; i++) {
            int fd=open(in_files[i], O_RDONLY);
            if(fd!=-1) {
                char buf[4096];
                int bytes;
                while((bytes=read(fd, buf, sizeof(buf)))>0) {
                    write(pfd[1], buf, bytes);
                }
                close(fd);
            }
        }
        close(pfd[1]);
        //fixing exit issue in child process
        _exit(0);
    }

    close(pfd[1]);
    *helper_pid=pid;
    return pfd[0];
}