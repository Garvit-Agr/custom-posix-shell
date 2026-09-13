#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "prompt.h"
#include "parser.h"
#include "cmds_hop.h"
#include "cmds_reveal.h"
#include "cmds_peek.h"
#include "cmds_locate.h"
#include "cmds_activities.h"
#include "cmds_ping.h"
#include "cmds_spy.h"
#include "cmds_snoop.h"
#include "cmds_resume.h"
#include "exec.h"
#include "inp_redir.h"
#include "out_redir.h"
#include "jobs.h"

#define MAX_USER_INPUT_ALLOWED 1024

char homewd[PATH_MAX+1];
volatile sig_atomic_t sigchld_pending=0;

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno=errno;
    int status;
    pid_t pid;

    while((pid=waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED))>0) {
        sigchld_pending=1;

        for(int i=0;i<job_cnt;i++) {
            int found=0;
            for(int j=0;j<bg_jobs[i].num_pids;j++) {
                if(bg_jobs[i].pids[j]!=pid) continue;

                found=1;
                if(WIFEXITED(status) || WIFSIGNALED(status)) {
                    if(j==0) {
                        if(WIFEXITED(status) && WEXITSTATUS(status)==0) bg_jobs[i].exit_status=0;
                        else bg_jobs[i].exit_status=1;
                    }
                    bg_jobs[i].pids[j]=-1;
                }
                else if(WIFSTOPPED(status)) {
                    bg_jobs[i].state=0;
                }
                else if(WIFCONTINUED(status)) {
                    bg_jobs[i].state=1;
                }
                break;
            }

            if(found) {
                int all_done=1;
                for(int j=0;j<bg_jobs[i].num_pids;j++) {
                    if(bg_jobs[i].pids[j]>0) {
                        all_done=0;
                        break;
                    }
                }
                if(all_done) bg_jobs[i].is_done=1;
                break;
            }
        }
    }

    errno=saved_errno;
}

void prcs_cmplt_job(void) {
    if(!sigchld_pending) return;

    sigset_t mask;
    sigset_t old_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &old_mask);

    sigchld_pending=0;
    for(int i=0;i<job_cnt;i++) {
        if(!bg_jobs[i].is_done) continue;

        char first_word[1024]="";
        sscanf(bg_jobs[i].cmd, "%1023s", first_word);
        if(bg_jobs[i].exit_status==0) printf("%s with pid %d exited normally\n", first_word, bg_jobs[i].pid);
        else printf("%s with pid %d exited abnormally\n", first_word, bg_jobs[i].pid);
        remove_job(bg_jobs[i].pid);
        i--;
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

int builtin_cmd(const char *cmd) {
    return strcmp(cmd,"hop")==0 || strcmp(cmd,"reveal")==0 || strcmp(cmd,"peek")==0 ||
           strcmp(cmd,"locate")==0 || strcmp(cmd,"activities")==0 || strcmp(cmd,"resume")==0 ||
           strcmp(cmd,"ping")==0 || strcmp(cmd,"spy")==0 || strcmp(cmd,"snoop")==0;
}

int cmd_has_pipe(tknll *head) {
    for(tknll *ptr=head;ptr!=NULL && ptr->type!=OP_SEMI && ptr->type!=OP_AMP;ptr=ptr->next) {
        if(ptr->type==OP_PIPE) return 1;
    }
    return 0;
}

int cmd_is_bg(tknll *head) {
    for(tknll *ptr=head;ptr!=NULL && ptr->type!=OP_SEMI;ptr=ptr->next) {
        if(ptr->type==OP_AMP) return 1;
    }
    return 0;
}

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler=sigchld_handler;
    sa.sa_flags=SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    pid_t shell_pid=getpid();
    setpgid(shell_pid, shell_pid);
    tcsetpgrp(STDIN_FILENO, shell_pid);

    if(getcwd(homewd, sizeof(homewd))==NULL) return 1;

    char prevwd[PATH_MAX+1]={0};
    int eof_warned=0;

    while(1) {
        prcs_cmplt_job();
        display_prompt(homewd);

        char input[MAX_USER_INPUT_ALLOWED+5];
        if(fgets(input, sizeof(input), stdin)==NULL) {
            if(errno==EINTR) {
                clearerr(stdin);
                prcs_cmplt_job();
                continue;
            }

            if(feof(stdin)) {
                int has_stopped=0;
                sigset_t mask;
                sigset_t old_mask;
                sigemptyset(&mask);
                sigaddset(&mask, SIGCHLD);
                sigprocmask(SIG_BLOCK, &mask, &old_mask);

                for(int i=0;i<job_cnt;i++) {
                    if(!bg_jobs[i].is_done && bg_jobs[i].state==0) {
                        has_stopped=1;
                        break;
                    }
                }

                if(has_stopped) {
                    printf("\ncshell: there are stopped jobs\n");
                    if(eof_warned) {
                        for(int i=0;i<job_cnt;i++) {
                            if(!bg_jobs[i].is_done && bg_jobs[i].pid>0) kill(-bg_jobs[i].pid, SIGHUP);
                        }
                        sigprocmask(SIG_SETMASK, &old_mask, NULL);
                        printf("\n");
                        break;
                    }
                    eof_warned=1;
                    sigprocmask(SIG_SETMASK, &old_mask, NULL);
                    clearerr(stdin);
                    continue;
                }

                for(int i=0;i<job_cnt;i++) {
                    if(!bg_jobs[i].is_done && bg_jobs[i].pid>0) kill(-bg_jobs[i].pid, SIGHUP);
                }
                sigprocmask(SIG_SETMASK, &old_mask, NULL);
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

        if(!valid_grmr(head)) {
            printf("cshell: invalid syntax\n");
            free_tkn_ll(head);
            continue;
        }

        tknll *curr_cmd=head;
        while(curr_cmd!=NULL) {
            if(curr_cmd->type==OP_SEMI || curr_cmd->type==OP_AMP) {
                curr_cmd=curr_cmd->next;
                continue;
            }

            int bg=cmd_is_bg(curr_cmd);
            int is_builtin=builtin_cmd(curr_cmd->tkn);
            int has_pipe=cmd_has_pipe(curr_cmd);
            int exec_res=0;

            if(is_builtin && !has_pipe && !bg) {
                sigset_t builtin_mask;
                sigset_t builtin_old_mask;
                sigemptyset(&builtin_mask);
                sigaddset(&builtin_mask, SIGCHLD);
                sigprocmask(SIG_BLOCK, &builtin_mask, &builtin_old_mask);

                pid_t in_pid=-1;
                int in_fd=inp_redir(curr_cmd, &in_pid);
                pid_t out_pid=-1;
                int out_fd=out_redir(curr_cmd, &out_pid);

                int builtin_ran=0;
                if(in_fd!=-1 && out_fd!=-1) {
                    int saved_stdin=dup(STDIN_FILENO);
                    int saved_stdout=dup(STDOUT_FILENO);

                    if(saved_stdin!=-1 && saved_stdout!=-1) {
                        if(in_fd!=STDIN_FILENO) dup2(in_fd, STDIN_FILENO);
                        if(out_fd!=STDOUT_FILENO) dup2(out_fd, STDOUT_FILENO);

                        if(strcmp(curr_cmd->tkn,"hop")==0) hop(curr_cmd, homewd, prevwd);
                        else if(strcmp(curr_cmd->tkn,"reveal")==0) reveal(curr_cmd, homewd, prevwd);
                        else if(strcmp(curr_cmd->tkn,"peek")==0) peek(curr_cmd);
                        else if(strcmp(curr_cmd->tkn,"locate")==0) locate(curr_cmd);
                        else if(strcmp(curr_cmd->tkn,"activities")==0) activities(curr_cmd);
                        else if(strcmp(curr_cmd->tkn,"resume")==0) resume_cmd(curr_cmd);
                        else if(strcmp(curr_cmd->tkn,"ping")==0) ping_cmd(curr_cmd);
                        else if(strcmp(curr_cmd->tkn,"spy")==0) spy_cmd(curr_cmd);
                        else if(strcmp(curr_cmd->tkn,"snoop")==0) snoop_cmd(curr_cmd);

                        fflush(stdout);
                        dup2(saved_stdin, STDIN_FILENO);
                        dup2(saved_stdout, STDOUT_FILENO);
                        builtin_ran=1;
                    }
                    if(saved_stdin!=-1) close(saved_stdin);
                    if(saved_stdout!=-1) close(saved_stdout);
                }

                if(in_fd!=STDIN_FILENO && in_fd!=-1) close(in_fd);
                if(out_fd!=STDOUT_FILENO && out_fd!=-1) close(out_fd);
                if(in_pid!=-1) {
                    kill(in_pid, SIGTERM);
                    waitpid(in_pid, NULL, 0);
                }
                if(out_pid!=-1) {
                    if(builtin_ran) waitpid(out_pid, NULL, 0);
                    else {
                        kill(out_pid, SIGTERM);
                        waitpid(out_pid, NULL, 0);
                    }
                }
                sigprocmask(SIG_SETMASK, &builtin_old_mask, NULL);
            }
            else {
                exec_res=execute(curr_cmd, homewd, prevwd, bg);
            }

            if(exec_res==1) break;

            while(curr_cmd!=NULL && curr_cmd->type!=OP_SEMI && curr_cmd->type!=OP_AMP) curr_cmd=curr_cmd->next;
            if(curr_cmd!=NULL) curr_cmd=curr_cmd->next;
        }

        free_tkn_ll(head);
    }

    return 0;
}