#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "parser.h"
#include "out_redir.h"

int out_redir(tknll *head, pid_t *helper_pid) {
    *helper_pid=-1;

    char **out_files=NULL;
    int *out_modes=NULL;
    int out_cnt=0;
    int cap=0;

    for(tknll *ptr=head; ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP; ptr=ptr->next) {
        if(ptr->type!=OP_GT && ptr->type!=OP_GTGT) continue;
        if(ptr->next==NULL || ptr->next->type!=WORD) continue;

        if(out_cnt==cap) {
            int new_cap=cap==0 ? 8 : cap*2;
            char **new_files=malloc((size_t)new_cap*sizeof(char *));
            int *new_modes=malloc((size_t)new_cap*sizeof(int));
            if(new_files==NULL || new_modes==NULL) {
                free(new_files);
                free(new_modes);
                free(out_files);
                free(out_modes);
                return -1;
            }
            for(int i=0;i<out_cnt;i++) {
                new_files[i]=out_files[i];
                new_modes[i]=out_modes[i];
            }
            free(out_files);
            free(out_modes);
            out_files=new_files;
            out_modes=new_modes;
            cap=new_cap;
        }

        out_files[out_cnt]=ptr->next->tkn;
        out_modes[out_cnt]=(ptr->type==OP_GTGT);
        out_cnt++;
    }

    if(out_cnt==0) {
        free(out_files);
        free(out_modes);
        return STDOUT_FILENO;
    }

    int *fds=malloc((size_t)out_cnt*sizeof(int));
    if(fds==NULL) {
        free(out_files);
        free(out_modes);
        return -1;
    }

    for(int i=0;i<out_cnt;i++) {
        int flags=O_WRONLY|O_CREAT|(out_modes[i] ? O_APPEND : O_TRUNC);
        fds[i]=open(out_files[i], flags, 0644);
        if(fds[i]==-1) {
            printf("cshell: unable to create file for writing\n");
            for(int j=0;j<i;j++) close(fds[j]);
            free(fds);
            free(out_files);
            free(out_modes);
            return -1;
        }
    }

    free(out_files);
    free(out_modes);

    if(out_cnt==1) {
        int fd=fds[0];
        free(fds);
        return fd;
    }

    int pfd[2];
    if(pipe(pfd)==-1) {
        perror("pipe");
        for(int i=0;i<out_cnt;i++) close(fds[i]);
        free(fds);
        return -1;
    }

    pid_t pid=fork();
    if(pid==-1) {
        perror("fork");
        close(pfd[0]);
        close(pfd[1]);
        for(int i=0;i<out_cnt;i++) close(fds[i]);
        free(fds);
        return -1;
    }

    if(pid==0) {
        close(pfd[1]);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        char buf[4096];
        ssize_t bytes;
        while((bytes=read(pfd[0], buf, sizeof(buf)))>0) {
            for(int i=0;i<out_cnt;i++) {
                ssize_t written=0;
                while(written<bytes) {
                    ssize_t n=write(fds[i], buf+written, (size_t)(bytes-written));
                    if(n<0) _exit(1);
                    written+=n;
                }
            }
        }

        close(pfd[0]);
        for(int i=0;i<out_cnt;i++) close(fds[i]);
        _exit(bytes<0 ? 1 : 0);
    }

    close(pfd[0]);
    for(int i=0;i<out_cnt;i++) close(fds[i]);
    free(fds);
    *helper_pid=pid;
    return pfd[1];
}