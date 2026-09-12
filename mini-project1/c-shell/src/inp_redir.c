#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#include "parser.h"
#include "inp_redir.h"

int inp_redir(tknll *head, pid_t *helper_pid) { //FIX2
    *helper_pid=-1;

    int cap=10;
    char **in_files=malloc(cap*sizeof(char *));
    int in_cnt=0;

    if(in_files==NULL) return -1;

    tknll *ptr=head;

    while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP) {
        if(ptr->type==OP_LT) {
            if(ptr->next!=NULL && ptr->next->type==WORD) {
                if(in_cnt>=cap) {
                    int new_cap=cap*2;
                    char **tmp=realloc(in_files,new_cap*sizeof(char *));
                    if(tmp==NULL) {
                        free(in_files);
                        return -1;
                    }
                    in_files=tmp;
                    cap=new_cap;
                }
                in_files[in_cnt++]=ptr->next->tkn;
            }
        }
        ptr=ptr->next;
    }

    if(in_cnt==0) {
        free(in_files);
        return STDIN_FILENO;
    }

    if(in_cnt==1) {
        int fd=open(in_files[0], O_RDONLY);
        free(in_files);
        if(fd==-1) {
            printf("cshell: no such file or directory\n");
            return -1;
        }
        return fd;
    }

    int *fds=malloc((size_t)in_cnt*sizeof(int));

    for(int i=0; i<in_cnt; i++) {
        fds[i]=open(in_files[i], O_RDONLY);
        if(fds[i]==-1) {
            printf("cshell: no such file or directory\n");
            for(int j=0; j<i; j++) close(fds[j]);
            free(fds);
            free(in_files);
            return -1;
        }
    }

    int pfd[2];
    if(pipe(pfd)==-1) {
        perror("pipe");
        for(int i=0; i<in_cnt; i++) close(fds[i]);
        free(fds);
        free(in_files);
        return -1;
    }

    pid_t pid=fork();
    if(pid==-1) {
        perror("fork");
        close(pfd[0]);
        close(pfd[1]);
        for(int i=0; i<in_cnt; i++) close(fds[i]);
        free(fds);
        free(in_files);
        return -1;
    }

    if(pid==0) {
        close(pfd[0]);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        for(int i=0; i<in_cnt; i++) {
            char buf[4096];
            int bytes;

            while((bytes=read(fds[i],buf,sizeof(buf)))>0) {
                int written=0;
                while(written<bytes) {
                    int n=write(pfd[1],buf+written,bytes-written);
                    if(n<0) {
                        for(int j=0; j<in_cnt; j++) close(fds[j]);
                        close(pfd[1]);
                        free(fds);
                        free(in_files);
                        _exit(1);
                    }
                    written+=n;
                }
            }

            if(bytes<0) {
                for(int j=0; j<in_cnt; j++) close(fds[j]);
                close(pfd[1]);
                free(fds);
                free(in_files);
                _exit(1);
            }
        }

        for(int i=0; i<in_cnt; i++) close(fds[i]);
        close(pfd[1]);
        free(fds);
        free(in_files);
        _exit(0);
    }

    close(pfd[1]);
    for(int i=0; i<in_cnt; i++) close(fds[i]);
    free(fds);
    free(in_files);
    *helper_pid=pid;
    return pfd[0];
}