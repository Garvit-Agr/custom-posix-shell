#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "parser.h"
#include "cmds_resume.h"
#include "jobs.h"

volatile sig_atomic_t fg_active_pid=-1;
volatile sig_atomic_t fg_timed_out=0;

int parse_number(const char *str, int *value) {
    if(str==NULL || *str=='\0') return 0;

    int num=0;
    for(int i=0;str[i]!='\0';i++) {
        if(str[i]<'0' || str[i]>'9') return 0;
        if(num>(2147483647-(str[i]-'0'))/10) return 0;
        num=num*10+(str[i]-'0');
    }

    *value=num;
    return 1;
}

void fg_alarm_handler(int sig) {
    (void)sig;
    if(fg_active_pid>0) {
        kill(-fg_active_pid, SIGTERM);
        fg_timed_out=1;
    }
}

int fg_job(int job_idx, int timeout) {
    if(job_idx<0 || job_idx>=job_cnt) return -1;

    sigset_t block_mask;
    sigset_t old_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

    job *fg_job_ptr=&bg_jobs[job_idx];
    fg_job_ptr->state=1;

    if(tcsetpgrp(STDIN_FILENO, fg_job_ptr->pid)<0) {
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return -1;
    }

    if(kill(-fg_job_ptr->pid, SIGCONT)<0) {
        tcsetpgrp(STDIN_FILENO, getpid());
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return -1;
    }

    printf("%s\n", fg_job_ptr->cmd);
    fflush(stdout);

    fg_active_pid=fg_job_ptr->pid;
    fg_timed_out=0;

    struct sigaction alrm_action;
    struct sigaction old_alrm_action;
    memset(&alrm_action, 0, sizeof(alrm_action));
    alrm_action.sa_handler=fg_alarm_handler;
    sigemptyset(&alrm_action.sa_mask);

    if(timeout>0) {
        sigaction(SIGALRM, &alrm_action, &old_alrm_action);
        alarm((unsigned int)timeout);
    }

    int stopped=0;
    int live=0;
    for(int i=0;i<fg_job_ptr->num_pids;i++) {
        if(fg_job_ptr->pids[i]>0) live++;
    }

    while(live>0) {
        int status=0;
        pid_t waited=waitpid(-fg_job_ptr->pid, &status, WUNTRACED);
        if(waited<0) {
            if(errno==EINTR) {
                if(fg_timed_out!=0) break;
                continue;
            }
            break;
        }

        if(WIFSTOPPED(status)) {
            stopped=1;
            fg_job_ptr->state=0;
            break;
        }

        if(WIFEXITED(status) || WIFSIGNALED(status)) {
            live--;
            for(int i=0;i<fg_job_ptr->num_pids;i++) {
                if(fg_job_ptr->pids[i]==waited) {
                    if(i==0) {
                        if(WIFEXITED(status) && WEXITSTATUS(status)==0) fg_job_ptr->exit_status=0;
                        else fg_job_ptr->exit_status=1;
                    }
                    fg_job_ptr->pids[i]=-1;
                    break;
                }
            }
        }
    }

    if(timeout>0) {
        alarm(0);
        sigaction(SIGALRM, &old_alrm_action, NULL);
    }

    fg_active_pid=-1;
    tcsetpgrp(STDIN_FILENO, getpid());

    if(fg_timed_out!=0) {
        printf("resume: job timed out\n");
        fflush(stdout);
        fg_job_ptr->state=1;
    }
    else if(stopped!=0) {
        printf("\n[%d] + Stopped    %s\n", fg_job_ptr->job_num, fg_job_ptr->cmd);
        fflush(stdout);
    }
    else {
        fg_job_ptr->is_done=1;
        remove_job(fg_job_ptr->pid);
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    return 0;
}

void resume_cmd(tknll *head) {
    if(head==NULL || head->next==NULL || head->next->type!=WORD) {
        printf("resume: invalid syntax\n");
        return;
    }

    tknll *job_tkn=head->next;
    tknll *mode_tkn=job_tkn->next;
    int job_num=0;

    if(job_tkn->tkn[0]!='%' || !parse_number(job_tkn->tkn+1, &job_num) ||
       mode_tkn==NULL || mode_tkn->type!=WORD) {
        printf("resume: invalid syntax\n");
        return;
    }

    int timeout=0;
    tknll *extra=mode_tkn->next;

    if(strcmp(mode_tkn->tkn,"bg")==0) {
        if(extra!=NULL) {
            printf("resume: invalid syntax\n");
            return;
        }
    }
    else if(strcmp(mode_tkn->tkn,"fg")==0) {
        if(extra!=NULL) {
            tknll *timeout_tkn=(extra->type==WORD && strcmp(extra->tkn,"--timeout")==0) ? extra->next : NULL;
            if(extra->type!=WORD || strcmp(extra->tkn,"--timeout")!=0 ||
               timeout_tkn==NULL || timeout_tkn->type!=WORD ||
               !parse_number(timeout_tkn->tkn, &timeout)) {
                printf("resume: invalid syntax\n");
                return;
            }
            tknll *after_timeout=timeout_tkn->next;
            if(after_timeout!=NULL && after_timeout->type==WORD) {
                printf("resume: invalid syntax\n");
                return;
            }
        }
    }
    else {
        printf("resume: invalid syntax\n");
        return;
    }

    int job_idx=-1;
    for(int i=0;i<job_cnt;i++) {
        if(bg_jobs[i].job_num==job_num && bg_jobs[i].is_done==0) {
            job_idx=i;
            break;
        }
    }

    if(job_idx==-1) {
        printf("resume: no such job\n");
        return;
    }

    if(strcmp(mode_tkn->tkn,"bg")==0) {
        if(kill(-bg_jobs[job_idx].pid, SIGCONT)<0) {
            printf("resume: no such job\n");
            return;
        }
        bg_jobs[job_idx].state=1;
        printf("[%d] + Running    %s\n", bg_jobs[job_idx].job_num, bg_jobs[job_idx].cmd);
    }
    else {
        if(fg_job(job_idx, timeout)<0) printf("resume: no such job\n");
    }
}
