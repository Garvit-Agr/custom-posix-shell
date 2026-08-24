#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "parser.h"
#include "out_redir.h"

int out_redir(tknll *head, pid_t *helper_pid) {
    *helper_pid=-1;
    char *out_files[100];
    int out_modes[100];
    int out_cnt=0;
    
    tknll *ptr=head;
    
    while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP) {
        if(ptr->type==OP_GT || ptr->type==OP_GTGT) {
            if(ptr->next!=NULL && ptr->next->type==WORD) {
                out_files[out_cnt]=ptr->next->tkn;
                if(ptr->type==OP_GTGT) out_modes[out_cnt]=1;
                else out_modes[out_cnt]=0;
                out_cnt++;
            }
        }
        ptr=ptr->next;
    }

    if(out_cnt==0) return STDOUT_FILENO;

    int fds[100];
    for(int i=0; i<out_cnt; i++) {
        int flags= O_WRONLY|O_CREAT;
        if(out_modes[i]==0) flags= flags|O_TRUNC;
        else flags= flags|O_APPEND;
        
        fds[i]=open(out_files[i], flags, 0644);
        if(fds[i]==-1) {
            printf("cshell: unable to create file for writing\n");
            for(int j=0; j<i; j++) close(fds[j]);
            return -1;
        }
    }

    if(out_cnt==1) {
        return fds[0];
    }

    int pfd[2];
    if(pipe(pfd)==-1) {
        perror("pipe");
        for(int i=0; i<out_cnt; i++) close(fds[i]);
        return -1;
    }

    pid_t pid=fork();
    if(pid==-1) {
        perror("fork");
        close(pfd[0]);
        close(pfd[1]);
        for(int i=0; i<out_cnt; i++) close(fds[i]);
        return -1;
    }

    if(pid==0) {
        close(pfd[1]);
        
        char buf[4096];
        int bytes;
        while((bytes=read(pfd[0], buf, sizeof(buf)))>0) {
            for(int i=0; i<out_cnt; i++) {
                write(fds[i], buf, bytes);
            }
        }
        
        close(pfd[0]);
        for(int i=0; i<out_cnt; i++) close(fds[i]);
        //fixing exit issue in child process
        _exit(0);
    }

    close(pfd[0]);
    for(int i=0; i<out_cnt; i++) close(fds[i]);
    *helper_pid=pid;


    return pfd[1];
}