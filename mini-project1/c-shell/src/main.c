#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h> //included for "MAXPATHLEN"
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <signal.h>

#include "prompt.h"
#include "parser.h"
#include "cmds_hop.h"
#include "cmds_reveal.h"
#include "cmds_peek.h"
#include "cmds_locate.h"
#include "cmds_activities.h"
#include "cmds_bg.h"
#include "cmds_fg.h"
#include "cmds_ping.h"
#include "exec.h"
#include "inp_redir.h"
#include "out_redir.h"
#include "jobs.h"

#define MAX_USER_INPUT_ALLOWED 1024

int fg_running=0;
char homewd[MAXPATHLEN+5]; // made global for sigchld prompt redraw

void sigchld_handler(int sig) {
    int status;
    for(int i=0;i<job_cnt;i++) {
        
        for(int j=0; j<bg_jobs[i].num_pids; j++) {
            if(bg_jobs[i].pids[j]>0 && bg_jobs[i].pids[j]!=bg_jobs[i].pid) {
                if(waitpid(bg_jobs[i].pids[j], &status, WNOHANG)>0) {
                    bg_jobs[i].pids[j]=-1; 
                }
            }
        }
        
        if(bg_jobs[i].is_done==0) {
            pid_t pid=waitpid(bg_jobs[i].pid, &status, WNOHANG);
            if(pid>0) {
                bg_jobs[i].is_done=1;
                if(WIFEXITED(status) && WEXITSTATUS(status)==0) bg_jobs[i].exit_status=0;
                else bg_jobs[i].exit_status=1;

                if(fg_running==0) {
                    char first_word[1024];
                    sscanf(bg_jobs[i].cmd, "%s", first_word);
                    if(bg_jobs[i].exit_status==0) {
                        printf("\n%s with pid %d exited normally\n", first_word, pid);
                    }
                    else {
                        printf("\n%s with pid %d exited abnormally\n", first_word, pid);
                    }
                    remove_job(pid);
                    i--; 
                    
                    display_prompt(homewd);
                    fflush(stdout);
                }
            }
        }
    }
}


int main() {
    
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGCHLD, sigchld_handler);
    
    pid_t shell_pid=getpid();
    setpgid(shell_pid, shell_pid);
    tcsetpgrp(STDIN_FILENO, shell_pid);

    getcwd(homewd,MAXPATHLEN+5);
    
    char prevwd[MAXPATHLEN+5]={0}; //found this in description of "man getcwd"
    int eof_warned=0;
    
    while(1) {

        for(int i=0;i<job_cnt;i++) {
            if(bg_jobs[i].is_done==1) {
                char first_word[1024];
                sscanf(bg_jobs[i].cmd, "%s", first_word);
                if(bg_jobs[i].exit_status==0) {
                    printf("%s with pid %d exited normally\n", first_word, bg_jobs[i].pid);
                }
                else {
                    printf("%s with pid %d exited abnormally\n", first_word, bg_jobs[i].pid);
                }
                remove_job(bg_jobs[i].pid);
                i--;
            }
        }

        display_prompt(homewd);
        char input[MAX_USER_INPUT_ALLOWED+5];
        
        if (fgets(input, sizeof(input), stdin)==NULL) { // used this instead if scanf, so that multi-word sentences can be taken easily as an input
                                                        // put "if" to bypass ctrl+d issue.
            if(feof(stdin)) {
                int has_stopped=0;
                for(int i=0;i<job_cnt;i++) {
                    if(bg_jobs[i].is_done==0) {
                        char st_pth[256];
                        snprintf(st_pth, sizeof(st_pth), "/proc/%d/stat", bg_jobs[i].pid);
                        FILE *f=fopen(st_pth, "r");
                        if(f!=NULL) {
                            char buf[1024];
                            if(fgets(buf, sizeof(buf), f)!=NULL) {
                                char *cls=strrchr(buf, ')');
                                if(cls!=NULL && *(cls+2)=='T') has_stopped=1;
                            }
                            fclose(f);
                        }
                    }
                }

                if(has_stopped==1 && eof_warned==0) {
                    printf("\ncshell: There are stopped jobs.\n");
                    eof_warned=1;
                    clearerr(stdin);
                    continue;
                }

                for(int i=0;i<job_cnt;i++) {
                    if(bg_jobs[i].is_done==0) {
                        for(int j=0;j<bg_jobs[i].num_pids;j++) {
                            if(bg_jobs[i].pids[j]>0) kill(bg_jobs[i].pids[j], SIGKILL);
                        }
                    }
                }

                printf("\n");
                break;
            }
            clearerr(stdin);
            continue;
        }
        input[strcspn(input,"\n")]='\0';
        eof_warned=0;

        tknll *head=lexer(input);

        if(head==NULL) continue;

        if(valid_grmr(head)==false) {
            printf("cshell: invalid syntax\n");
            free_tkn_ll(head);
            continue;
        }


        
        tknll *curr_cmd=head;

        while(curr_cmd!=NULL) {
            if(curr_cmd->type==OP_SEMI) {
                curr_cmd=curr_cmd->next;
                continue;
            }
            if(curr_cmd->type==OP_AMP) {
                curr_cmd=curr_cmd->next;
                continue;
            }

            int bg=0;
            tknll *check_bg=curr_cmd;
            while(check_bg!=NULL && check_bg->type!=OP_SEMI) {
                if(check_bg->type==OP_AMP) {
                    bg=1;
                    break;
                }
                check_bg=check_bg->next;
            }

            int is_builtin=(strcmp(curr_cmd->tkn,"hop")==0 || strcmp(curr_cmd->tkn,"reveal")==0 ||
            strcmp(curr_cmd->tkn,"peek")==0 || strcmp(curr_cmd->tkn,"locate")==0 ||
            strcmp(curr_cmd->tkn,"activities")==0 || strcmp(curr_cmd->tkn,"bg")==0 ||
            strcmp(curr_cmd->tkn,"fg")==0 || strcmp(curr_cmd->tkn,"ping")==0);
            
            int has_pipe=0;
            tknll *tmp=curr_cmd;
            while(tmp!=NULL && tmp->type!=OP_SEMI && tmp->type!=OP_AMP) {
                if(tmp->type==OP_PIPE) has_pipe=1;
                tmp=tmp->next;
            }

            int exec_res=0;

            if (is_builtin && !has_pipe) {
                fg_running=1;
                pid_t in_pid=-1, out_pid=-1;
                int in_fd=inp_redir(curr_cmd, &in_pid);
                int out_fd=out_redir(curr_cmd, &out_pid);
                
                if (in_fd!=-1 && out_fd!=-1) {
                    int saved_stdin=dup(STDIN_FILENO);
                    int saved_stdout=dup(STDOUT_FILENO);

                    if (in_fd!=STDIN_FILENO) dup2(in_fd, STDIN_FILENO);
                    if (out_fd!=STDOUT_FILENO) dup2(out_fd, STDOUT_FILENO);

                    if(strcmp(curr_cmd->tkn,"hop")==0) hop(curr_cmd, homewd, prevwd);
                    else if(strcmp(curr_cmd->tkn,"reveal")==0) reveal(curr_cmd, homewd, prevwd);
                    else if(strcmp(curr_cmd->tkn,"peek")==0) peek(curr_cmd, homewd, prevwd);
                    else if(strcmp(curr_cmd->tkn,"locate")==0) locate(curr_cmd, homewd, prevwd);
                    else if(strcmp(curr_cmd->tkn,"activities")==0) activities(curr_cmd);
                    else if(strcmp(curr_cmd->tkn,"bg")==0) bg_cmd(curr_cmd);
                    else if(strcmp(curr_cmd->tkn,"fg")==0) fg_cmd(curr_cmd);
                    else if(strcmp(curr_cmd->tkn,"ping")==0) ping_cmd(curr_cmd);

                    fflush(stdout);

                    dup2(saved_stdin, STDIN_FILENO);
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdin);
                    close(saved_stdout);
                }
                if(in_fd!=-1 && in_fd!=STDIN_FILENO) close(in_fd);
                if(out_fd!=-1 && out_fd!=STDOUT_FILENO) close(out_fd);
                if(in_pid!=-1) waitpid(in_pid, NULL, 0);
                if(out_pid!=-1) waitpid(out_pid, NULL, 0);
                fg_running=0;
            }
            else {
                if(bg==0) fg_running=1;
                exec_res=execute(curr_cmd, homewd, prevwd, bg);
                if(bg==0) fg_running=0;
            }

            if(exec_res==1) break;

            while(curr_cmd!=NULL && curr_cmd->type!=OP_SEMI && curr_cmd->type!=OP_AMP) {
                curr_cmd=curr_cmd->next;
            }
            
            if(curr_cmd!=NULL && (curr_cmd->type==OP_SEMI || curr_cmd->type==OP_AMP)) {
                curr_cmd=curr_cmd->next;
            }
        }





        free_tkn_ll(head);
    }


    return 0;
}