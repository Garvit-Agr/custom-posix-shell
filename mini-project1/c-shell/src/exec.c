#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h> // required for waitpid()
#include <sys/param.h>

#include "parser.h"
#include "exec.h"
#include "redir.h"

void execute(tknll *head) {
    if(head==NULL) return;

    int argc=0;
    tknll *ptr=head;
    int exp_redir=0;
    
    while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP) {
        if(ptr->type==OP_LT) exp_redir=1;
        else if(ptr->type==WORD) {
            if(exp_redir) exp_redir=0;
            else argc++;
        }
        ptr=ptr->next;
    }

    if(argc==0) return;

    char **argv=malloc((argc+1)*sizeof(char*));
    ptr=head;
    exp_redir=0;
    int idx=0;

    while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP) {
        if(ptr->type==OP_LT) exp_redir=1;
        else if(ptr->type==WORD) {
            if(exp_redir) exp_redir=0;
            else argv[idx++]=ptr->tkn;
        }
        ptr=ptr->next;
    }
    argv[argc]=NULL;

    pid_t helper_pid=-1;
    int in_fd=inp_redir(head, &helper_pid);
    
    if(in_fd==-1 && helper_pid==-1) {
        free(argv);
        return;
    }

    char *cmd=argv[0];
    char exec_path[MAXPATHLEN+5]="";
    int use_path=0;

    if(cmd[0]=='%') {
        cmd++;
        argv[0]=cmd; 
        use_path=1;
    } 
    else if(strchr(cmd, '/')!=NULL) strcpy(exec_path, cmd);

    else {
        char cwd_path[MAXPATHLEN+5];
        snprintf(cwd_path, sizeof(cwd_path), "./%s", cmd);
        
        DIR *dir_check=opendir(cwd_path);
        if (dir_check!=NULL) {
            closedir(dir_check);
            use_path=1;
        }
        else {
            if(access(cwd_path, X_OK)==0) strcpy(exec_path, cwd_path);
            else use_path=1;
        }
    }

    pid_t pid=fork();
    
    if(pid==-1) {
        perror("fork");
        if(in_fd!=STDIN_FILENO) close(in_fd);
        free(argv);
        return;
    }

    if(pid==0) {
        if(in_fd!=STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }

        if(use_path!=0) execvp(cmd, argv);
        else execv(exec_path, argv);

        printf("cshell: command not found (%s)\n", cmd);
        free(argv);
        exit(1);
    }
    else {
        if(in_fd!=STDIN_FILENO) close(in_fd);

        int status;
        waitpid(pid, &status, 0);

        if(helper_pid!=-1) waitpid(helper_pid, &status, 0);

        free(argv);
    }
}

