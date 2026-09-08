#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h> // required for waitpid()
#include <sys/param.h>

#include "parser.h"
#include "exec.h"
#include "inp_redir.h"
#include "out_redir.h"
#include "cmds_hop.h"
#include "cmds_reveal.h"
#include "cmds_peek.h"
#include "cmds_locate.h"
#include "cmds_activities.h"
#include "jobs.h"

int execute(tknll *head, char *homwd, char *prevwd, int bg) {
    if(head==NULL) return 0;

    int prev_pipe[2]={-1, -1};
    pid_t pids[100];
    int num_cmds=0;
    pid_t helpers[200];
    int num_helpers=0;
    
    tknll *pipe_st=head;
    
    while(pipe_st!=NULL && pipe_st->type!=OP_SEMI && pipe_st->type!=OP_AMP) {
        int argc=0;
        tknll *ptr=pipe_st;
        int exp_redir=0;
        
        while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP) {
            if(ptr->type==OP_LT || ptr->type==OP_GT || ptr->type==OP_GTGT) exp_redir=1;

            else if(ptr->type==WORD) {
                if(exp_redir) exp_redir=0;
                else argc++;
            }

            ptr=ptr->next;
        }

        if(argc==0) {
            if(prev_pipe[0]!=-1) close(prev_pipe[0]);
            if(prev_pipe[1]!=-1) close(prev_pipe[1]);
            break;
        }

        char **argv=malloc((argc+1)*sizeof(char*));
        ptr=pipe_st;
        exp_redir=0;
        int idx=0;

        while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP) {
            if(ptr->type==OP_LT || ptr->type==OP_GT || ptr->type==OP_GTGT) exp_redir=1;
            
            else if(ptr->type==WORD) {

                if(exp_redir) exp_redir=0;
                else argv[idx++]=ptr->tkn;
            }

            ptr=ptr->next;
        }
        argv[argc]=NULL;

        pid_t in_pid=-1;
        int in_fd=inp_redir(pipe_st, &in_pid);

        pid_t out_pid=-1;
        int out_fd=out_redir(pipe_st, &out_pid);
        
        int curr_pipe[2]={-1, -1};
        if(ptr!=NULL && ptr->type==OP_PIPE) pipe(curr_pipe);


        if(in_fd==-1 || out_fd==-1) {
            if(in_fd!=-1 && in_fd!=STDIN_FILENO) close(in_fd);
            if(out_fd!=-1 && out_fd!=STDOUT_FILENO) close(out_fd);
            if(prev_pipe[0]!=-1) close(prev_pipe[0]);
            if(prev_pipe[1]!=-1) close(prev_pipe[1]);
            if(curr_pipe[0]!=-1) close(curr_pipe[0]);
            if(curr_pipe[1]!=-1) close(curr_pipe[1]);
            free(argv);
            return 1;
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
            if(out_fd!=STDOUT_FILENO) close(out_fd);
            if(prev_pipe[0]!=-1) close(prev_pipe[0]);
            if(prev_pipe[1]!=-1) close(prev_pipe[1]);
            if(curr_pipe[0]!=-1) close(curr_pipe[0]);
            if(curr_pipe[1]!=-1) close(curr_pipe[1]);
            free(argv);
            return 1;
        }

        if(pid==0) {
            if(prev_pipe[0]!=-1) dup2(prev_pipe[0], STDIN_FILENO);
            if(curr_pipe[1]!=-1) dup2(curr_pipe[1], STDOUT_FILENO);

            if(in_fd!=STDIN_FILENO) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            if(out_fd!=STDOUT_FILENO) {
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }

            if(prev_pipe[0]!=-1) close(prev_pipe[0]);
            if(prev_pipe[1]!=-1) close(prev_pipe[1]);
            if(curr_pipe[0]!=-1) close(curr_pipe[0]);
            if(curr_pipe[1]!=-1) close(curr_pipe[1]);

            if(strcmp(cmd, "hop")==0) { hop(pipe_st, homwd, prevwd); fflush(stdout); _exit(0); }
            else if(strcmp(cmd, "reveal")==0) { reveal(pipe_st, homwd, prevwd); fflush(stdout); _exit(0); }
            else if(strcmp(cmd, "peek")==0) { peek(pipe_st, homwd, prevwd); fflush(stdout); _exit(0); }
            else if(strcmp(cmd, "locate")==0) { locate(pipe_st, homwd, prevwd); fflush(stdout); _exit(0); }
            else if(strcmp(cmd, "activities")==0) { activities(pipe_st); fflush(stdout); _exit(0); }

            if(use_path!=0) execvp(cmd, argv);
            else execv(exec_path, argv);

            printf("cshell: command not found (%s)\n", cmd);
            free(argv);
            fflush(stdout);
            _exit(1);
        }
        
        else {
            pids[num_cmds++]=pid;
            if(in_pid!=-1) helpers[num_helpers++]=in_pid;
            if(out_pid!=-1) helpers[num_helpers++]=out_pid;

            if(in_fd!=STDIN_FILENO) close(in_fd);
            if(out_fd!=STDOUT_FILENO) close(out_fd);

            if(prev_pipe[0]!=-1) close(prev_pipe[0]);
            if(prev_pipe[1]!=-1) close(prev_pipe[1]);
            
            prev_pipe[0]=curr_pipe[0];
            prev_pipe[1]=curr_pipe[1];

            free(argv);
        }

        if(ptr!=NULL && ptr->type==OP_PIPE) pipe_st=ptr->next;
        else break;
    }

    int failed=0;
    if(bg==0) {
        for(int i=0; i<num_cmds; i++) {
            int status;
            waitpid(pids[i], &status, 0);
            if(WIFEXITED(status) && WEXITSTATUS(status)==1) failed=1;
        }

        for(int i=0; i<num_helpers; i++) {
            int status;
            waitpid(helpers[i], &status, 0);
        }
    }
    else {
        char full_cmd[1024]="";
        tknll *tmp=head;
        while(tmp!=NULL && tmp->type!=OP_SEMI && tmp->type!=OP_AMP) {
            strcat(full_cmd, tmp->tkn);
            strcat(full_cmd, " ");
            tmp=tmp->next;
        }
        if(num_cmds>0) {
            add_job(pids[0], full_cmd);
            printf("[%d] %d\n", next_job_num-1, pids[0]);
        }
    }
    
    return failed;
}