#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "parser.h"
#include "exec.h"

void execute(tknll *head) {
    int argc=0;
    tknll *ptr=head;
    
    while(ptr!=NULL && ptr->type==WORD) {
        argc++;
        ptr=ptr->next;
    }

    if(argc==0) return;

    char **argv=malloc((argc+1)*sizeof(char*));
    ptr=head;
    for(int i=0; i<argc; i++) {
        argv[i]=ptr->tkn;
        ptr=ptr->next;
    }
    argv[argc]=NULL;

    char *cmd=argv[0];
    char exec_path[MAXPATHLEN+5]="";
    int use_path=0;

    if(strchr(cmd, '/')!=NULL) {
        strcpy(exec_path, cmd);
    } 
    else if(cmd[0]=='%') {
        cmd++;
        argv[0]=cmd; 
        use_path=1;
    } 
    else {
        char cwd_path[MAXPATHLEN+5];
        snprintf(cwd_path, sizeof(cwd_path), "./%s", cmd);
        
        if(access(cwd_path, X_OK)==0) {
            strcpy(exec_path, cwd_path);
        } else {
            use_path=1;
        }
    }

    pid_t pid=fork();
    
    if(pid==-1) {
        perror("fork");
        free(argv);
        return;
    }

    if(pid==0) {
        if(use_path!=0) {
            execvp(cmd, argv);
        } else {
            execv(exec_path, argv);
        }
        
        printf("cshell: command not found (%s)\n", cmd);
        free(argv);
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        free(argv);
    }
}